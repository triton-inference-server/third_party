#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#ifndef _WIN32
#include <sys/queue.h>
#include <unistd.h>
#else
#include "sys/queue.h"
#endif

#ifdef __linux__
#include <sys/eventfd.h>
#define EVTHR_USE_EVENTFD 1
#endif

#include <pthread.h>

#include <event2/event.h>
#include <event2/thread.h>

#include "internal.h"
#include "evhtp/thread.h"

/*
 * Cross-thread command hand-off.
 *
 * Commands (a callback + argument) are queued in an in-memory FIFO owned by the
 * target thread and the thread is woken through a "doorbell" descriptor (an
 * eventfd on Linux, a socketpair elsewhere). The doorbell carries no data: it
 * is only rung when the queue goes from empty to non-empty, and the thread
 * drains the whole queue each time it wakes.
 *
 * This replaces the original design, which serialized every command through a
 * SOCK_STREAM socketpair and consumed one command per event-loop iteration.
 * That socket buffer (kernel default, ~208 KB, a few hundred commands once skb
 * overhead is charged) was a hard cliff: when it filled, evthr_defer() failed
 * with EVTHR_RES_RETRY and callers that did not retry lost the command for
 * good. With an in-memory queue a hand-off can only fail on allocation
 * failure, ordering per queue is preserved, and a burst of commands costs one
 * wakeup instead of one per command.
 */

struct evthr_cmd {
    uint8_t            stop;
    void             * args;
    evthr_cb           cb;
    struct evthr_cmd * next;
};

struct evthr {
    evutil_socket_t rdr;            /**< doorbell read end (== wdr for eventfd) */
    evutil_socket_t wdr;            /**< doorbell write end */
    char            err;
    ev_t          * event;
    evbase_t      * evbase;
    pthread_mutex_t lock;
    pthread_t     * thr;
    evthr_init_cb   init_cb;
    evthr_exit_cb   exit_cb;
    void          * arg;
    void          * aux;
    int             busy;

    pthread_mutex_t    q_lock;
    struct evthr_cmd * q_head;
    struct evthr_cmd * q_tail;
    unsigned int       q_len;       /**< commands queued and not yet taken by the thread */

    TAILQ_ENTRY(evthr) next;
};

TAILQ_HEAD(evthr_pool_slist, evthr);

typedef struct evthr_cmd        evthr_cmd_t;
typedef struct evthr_pool_slist evthr_pool_slist_t;

struct evthr_pool {
    int                nthreads;
    evthr_pool_slist_t threads;
};

/* Wake the thread. A failure here is harmless: the doorbell can only be
 * "full" if it already holds an unread wakeup, so the thread is going to run
 * anyway. */
static void
_evthr_ring(evthr_t * thread)
{
#ifdef EVTHR_USE_EVENTFD
    uint64_t one = 1;
    ssize_t  n;

    n = write(thread->wdr, &one, sizeof(one));
    (void)n;
#else
    char one = 1;

    (void)send(thread->wdr, &one, sizeof(one), 0);
#endif
}

/* Clear the doorbell. Must run BEFORE the queue is taken, otherwise a ring
 * placed between the two steps would be cleared and its command left in the
 * queue until the next unrelated wakeup. */
static void
_evthr_silence(evutil_socket_t sock)
{
#ifdef EVTHR_USE_EVENTFD
    uint64_t count;
    ssize_t  n;

    n = read(sock, &count, sizeof(count));
    (void)n;
#else
    char buf[64];

    while (recv(sock, buf, sizeof(buf), 0) > 0) {
    }
#endif
}

static void
_evthr_read_cmd(evutil_socket_t sock, short which, void * args)
{
    evthr_t     * thread;
    evthr_cmd_t * cmd;
    int           stopped;

    if (!(thread = (evthr_t *)args)) {
        return;
    }

    _evthr_silence(sock);

    pthread_mutex_lock(&thread->q_lock);
    cmd            = thread->q_head;
    thread->q_head = NULL;
    thread->q_tail = NULL;
    thread->q_len  = 0;
    pthread_mutex_unlock(&thread->q_lock);

    stopped = 0;

    /* Run everything that was queued, in order. Commands queued while we run
     * (including from these callbacks) ring the doorbell again and are picked
     * up on the next loop iteration, so socket events are not starved. */
    while (cmd != NULL) {
        evthr_cmd_t * next = cmd->next;

        if (cmd->stop) {
            stopped = 1;
        }

        if (evhtp_likely(cmd->cb != NULL)) {
            (cmd->cb)(thread, cmd->args, thread->arg);
        }

        free(cmd);
        cmd = next;
    }

    if (evhtp_unlikely(stopped == 1)) {
        event_base_loopbreak(thread->evbase);
    }

    return;
} /* _evthr_read_cmd */

static void *
_evthr_loop(void * args)
{
    evthr_t * thread;

    if (!(thread = (evthr_t *)args)) {
        return NULL;
    }

    if (thread == NULL || thread->thr == NULL) {
        pthread_exit(NULL);
    }

    thread->evbase = event_base_new();
    thread->event  = event_new(thread->evbase, thread->rdr,
                               EV_READ | EV_PERSIST, _evthr_read_cmd, args);

    event_add(thread->event, NULL);

    pthread_mutex_lock(&thread->lock);
    if (thread->init_cb != NULL) {
        (thread->init_cb)(thread, thread->arg);
    }

    pthread_mutex_unlock(&thread->lock);

    event_base_loop(thread->evbase, 0);

    pthread_mutex_lock(&thread->lock);
    if (thread->exit_cb != NULL) {
        (thread->exit_cb)(thread, thread->arg);
    }

    pthread_mutex_unlock(&thread->lock);

    if (thread->err == 1) {
        fprintf(stderr, "FATAL ERROR!\n");
    }

    pthread_exit(NULL);
} /* _evthr_loop */

static evthr_res
_evthr_enqueue(evthr_t * thread, evthr_cb cb, void * arg, uint8_t stop)
{
    evthr_cmd_t * cmd;
    int           was_empty;

    if (thread == NULL) {
        return EVTHR_RES_FATAL;
    }

    if (!(cmd = malloc(sizeof(*cmd)))) {
        return EVTHR_RES_RETRY;
    }

    cmd->cb   = cb;
    cmd->args = arg;
    cmd->stop = stop;
    cmd->next = NULL;

    pthread_mutex_lock(&thread->q_lock);
    was_empty = (thread->q_head == NULL);

    if (was_empty) {
        thread->q_head = cmd;
    } else {
        thread->q_tail->next = cmd;
    }

    thread->q_tail = cmd;
    thread->q_len++;
    pthread_mutex_unlock(&thread->q_lock);

    if (was_empty) {
        _evthr_ring(thread);
    }

    return EVTHR_RES_OK;
}

evthr_res
evthr_defer(evthr_t * thread, evthr_cb cb, void * arg)
{
    return _evthr_enqueue(thread, cb, arg, 0);
}

evthr_res
evthr_stop(evthr_t * thread)
{
    evthr_res res;

    if ((res = _evthr_enqueue(thread, NULL, NULL, 1)) != EVTHR_RES_OK) {
        return res;
    }

    pthread_join(*thread->thr, NULL);
    return EVTHR_RES_OK;
}

evbase_t *
evthr_get_base(evthr_t * thr)
{
    return thr ? thr->evbase : NULL;
}

void
evthr_set_aux(evthr_t * thr, void * aux)
{
    if (thr) {
        thr->aux = aux;
    }
}

void *
evthr_get_aux(evthr_t * thr)
{
    return thr ? thr->aux : NULL;
}

void
evthr_set_busy(evthr_t * thr, int busy)
{
    if (thr) {
        thr->busy = busy;
    }
}

int
evthr_set_initcb(evthr_t * thr, evthr_init_cb cb)
{
    if (thr == NULL) {
        return -1;
    }

    thr->init_cb = cb;

    return 0;
}

int
evthr_set_exitcb(evthr_t * thr, evthr_exit_cb cb)
{
    if (thr == NULL) {
        return -1;
    }

    thr->exit_cb = cb;

    return 0;
}

static evthr_t *
_evthr_new(evthr_init_cb init_cb, evthr_exit_cb exit_cb, void * args)
{
    evthr_t * thread;

    if (!(thread = calloc(sizeof(evthr_t), 1))) {
        return NULL;
    }

    if (pthread_mutex_init(&thread->lock, NULL)) {
        free(thread);
        return NULL;
    }

    if (pthread_mutex_init(&thread->q_lock, NULL)) {
        pthread_mutex_destroy(&thread->lock);
        free(thread);
        return NULL;
    }

#ifdef EVTHR_USE_EVENTFD
    {
        int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

        if (efd == -1) {
            evthr_free(thread);
            return NULL;
        }

        thread->rdr = efd;
        thread->wdr = efd;
    }
#else
    {
        evutil_socket_t fds[2];

        if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
            evthr_free(thread);
            return NULL;
        }

        evutil_make_socket_nonblocking(fds[0]);
        evutil_make_socket_nonblocking(fds[1]);

        thread->rdr = fds[0];
        thread->wdr = fds[1];
    }
#endif

    thread->thr     = malloc(sizeof(pthread_t));
    thread->arg     = args;
    thread->busy    = 0;

    thread->init_cb = init_cb;
    thread->exit_cb = exit_cb;

    return thread;
} /* evthr_new */

evthr_t *
evthr_new(evthr_init_cb init_cb, void * args)
{
    return _evthr_new(init_cb, NULL, args);
}

evthr_t *
evthr_wexit_new(evthr_init_cb init_cb, evthr_exit_cb exit_cb, void * args)
{
    return _evthr_new(init_cb, exit_cb, args);
}

int
evthr_start(evthr_t * thread)
{
    if (thread == NULL || thread->thr == NULL) {
        return -1;
    }

    if (pthread_create(thread->thr, NULL, _evthr_loop, (void *)thread)) {
        return -1;
    }

    return 0;
}

void
evthr_free(evthr_t * thread)
{
    evthr_cmd_t * cmd;

    if (thread == NULL) {
        return;
    }

    if (thread->rdr > 0) {
        close(thread->rdr);
    }

    if (thread->wdr > 0 && thread->wdr != thread->rdr) {
        close(thread->wdr);
    }

    if (thread->thr) {
        free(thread->thr);
    }

    if (thread->event) {
        event_free(thread->event);
    }

    if (thread->evbase) {
        event_base_free(thread->evbase);
    }

    /* Commands queued after the stop command are dropped, as before. */
    cmd = thread->q_head;

    while (cmd != NULL) {
        evthr_cmd_t * next = cmd->next;

        free(cmd);
        cmd = next;
    }

    pthread_mutex_destroy(&thread->q_lock);
    pthread_mutex_destroy(&thread->lock);

    free(thread);
} /* evthr_free */

void
evthr_pool_free(evthr_pool_t * pool)
{
    evthr_t * thread;
    evthr_t * save;

    if (pool == NULL) {
        return;
    }

    TAILQ_FOREACH_SAFE(thread, &pool->threads, next, save) {
        TAILQ_REMOVE(&pool->threads, thread, next);

        evthr_free(thread);
    }

    free(pool);
}

evthr_res
evthr_pool_stop(evthr_pool_t * pool)
{
    evthr_t * thr;
    evthr_t * save;

    if (pool == NULL) {
        return EVTHR_RES_FATAL;
    }

    TAILQ_FOREACH_SAFE(thr, &pool->threads, next, save) {
        evthr_stop(thr);
    }

    return EVTHR_RES_OK;
}

/* Load hint for placement, called from the acceptor thread. q_len is read
 * under q_lock so that the read is ordered against the producers and the
 * worker; the value is still only a hint, since the queue may change right
 * after the lock is released. */
static inline int
get_backlog_(evthr_t * thread)
{
    unsigned int len;

    if (thread->busy) {
        return INT_MAX;
    }

    pthread_mutex_lock(&thread->q_lock);
    len = thread->q_len;
    pthread_mutex_unlock(&thread->q_lock);

    return (int)len;
}

evthr_res
evthr_pool_defer(evthr_pool_t * pool, evthr_cb cb, void * arg)
{
    evthr_t * thread      = NULL;
    evthr_t * min_thread  = NULL;
    int       min_backlog = 0;

    if (pool == NULL) {
        return EVTHR_RES_FATAL;
    }

    if (cb == NULL) {
        return EVTHR_RES_NOCB;
    }

    TAILQ_FOREACH(thread, &pool->threads, next) {
        int backlog = get_backlog_(thread);

        if (backlog == 0) {
            min_thread = thread;
            break;
        }

        if (min_thread == NULL || backlog < min_backlog) {
            min_thread  = thread;
            min_backlog = backlog;
        }
    }

    return evthr_defer(min_thread, cb, arg);
} /* evthr_pool_defer */

static evthr_pool_t *
_evthr_pool_new(int           nthreads,
                evthr_init_cb init_cb,
                evthr_exit_cb exit_cb,
                void        * shared)
{
    evthr_pool_t * pool;
    int            i;

    if (nthreads == 0) {
        return NULL;
    }

    if (!(pool = calloc(sizeof(evthr_pool_t), 1))) {
        return NULL;
    }

    pool->nthreads = nthreads;
    TAILQ_INIT(&pool->threads);

    for (i = 0; i < nthreads; i++) {
        evthr_t * thread;

        if (!(thread = evthr_wexit_new(init_cb, exit_cb, shared))) {
            evthr_pool_free(pool);
            return NULL;
        }

        TAILQ_INSERT_TAIL(&pool->threads, thread, next);
    }

    return pool;
} /* _evthr_pool_new */

evthr_pool_t *
evthr_pool_new(int nthreads, evthr_init_cb init_cb, void * shared)
{
    return _evthr_pool_new(nthreads, init_cb, NULL, shared);
}

evthr_pool_t *
evthr_pool_wexit_new(int nthreads,
                     evthr_init_cb init_cb,
                     evthr_exit_cb exit_cb, void * shared)
{
    return _evthr_pool_new(nthreads, init_cb, exit_cb, shared);
}

int
evthr_pool_start(evthr_pool_t * pool)
{
    evthr_t * evthr = NULL;

    if (pool == NULL) {
        return -1;
    }

    TAILQ_FOREACH(evthr, &pool->threads, next) {
        if (evthr_start(evthr) < 0) {
            return -1;
        }

#ifdef _WIN32
        Sleep(5);
#else
        usleep(5000);
#endif
    }

    return 0;
}
