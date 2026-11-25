// Copyright 2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/*
 * Feeds each request to htparser_run() split into two segments at every
 * byte offset, each segment in an exactly-sized heap buffer, and checks the
 * parsed fields match a single-buffer parse. Build with
 * -fsanitize=address,undefined to catch reads past the end of a segment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "evhtp/parser.h"

struct result {
    char   fields[4096];
    size_t fields_len;
    char   body[256];
    size_t body_len;
};

static struct result * res;

static void
add_field(const char * name, const char * data, size_t len)
{
    res->fields_len += snprintf(res->fields + res->fields_len,
                                sizeof(res->fields) - res->fields_len,
                                "%s: '%.*s'\n", name, (int)len, data);
}

#define DATA_HOOK(name)                                          \
    static int                                                   \
    _ ## name(htparser * p, const char * data, size_t len)       \
    {                                                            \
        (void)p;                                                 \
        add_field(#name, data, len);                             \
        return 0;                                                \
    }

DATA_HOOK(method)
DATA_HOOK(path)
DATA_HOOK(args)
DATA_HOOK(uri)
DATA_HOOK(hdr_key)
DATA_HOOK(hdr_val)

/* body may arrive in several calls when a segment boundary falls inside it */
static int
_body(htparser * p, const char * data, size_t len)
{
    (void)p;

    if (res->body_len + len >= sizeof(res->body))
    {
        return -1;
    }

    memcpy(res->body + res->body_len, data, len);
    res->body_len += len;

    return 0;
}

static int
_msg_complete(htparser * p)
{
    (void)p;

    add_field("body", res->body, res->body_len);
    add_field("complete", "", 0);
    res->body_len = 0;

    return 0;
}

static htparse_hooks hooks = {
    .method          = _method,
    .path            = _path,
    .args            = _args,
    .uri             = _uri,
    .hdr_key         = _hdr_key,
    .hdr_val         = _hdr_val,
    .body            = _body,
    .on_msg_complete = _msg_complete,
};

/* Returns -1 if htparser_run() reports consuming more than it was given. */
static int
feed(htparser * p, const char * src, size_t len)
{
    char * seg = malloc(len);
    size_t off = 0;
    int    rc  = 0;

    memcpy(seg, src, len);

    while (off < len)
    {
        size_t n = htparser_run(p, &hooks, seg + off, len - off);

        if (n > len - off)
        {
            rc = -1;
            break;
        }

        if (htparser_get_error(p) != htparse_error_none)
        {
            add_field("error", htparser_get_strerror(p),
                      strlen(htparser_get_strerror(p)));
            break;
        }

        off += n;
    }

    free(seg);

    return rc;
}

/* Parses req split at cut; cut == 0 parses it as a single segment. */
static int
parse(const char * req, size_t cut, struct result * out)
{
    htparser * p   = htparser_new();
    size_t     len = strlen(req);
    int        rc;

    memset(out, 0, sizeof(*out));
    res = out;

    htparser_init(p, htp_type_request);

    if (cut == 0)
    {
        rc = feed(p, req, len);
    } else {
        rc = feed(p, req, cut);

        if (rc == 0 && htparser_get_error(p) == htparse_error_none)
        {
            rc = feed(p, req + cut, len - cut);
        }
    }

    free(p);

    return rc;
}

int
main(void)
{
    static const char * requests[] = {
        "GET /a/bcd/efg?x=1&yy=22 HTTP/1.1\r\n"
        "Host: example\r\n"
        "X-Long-Header: some value here\r\n\r\n",

        "POST /v2/models/my_model/infer HTTP/1.1\r\n"
        "Content-Length: 4\r\n"
        "Content-Type: multipart/x\r\n\r\n"
        "abcd",

        "GET /abc HTTP/1.1\r\nA: b\r\n\r\nGET /second?q=v HTTP/1.1\r\nKey: val\r\n\r\n",

        "GET /\r\n",

        "PROPFIND /x/y HTTP/1.0\r\n\r\n",
    };
    struct result want;
    struct result got;
    int           failures = 0;
    size_t        r;
    size_t        cut;

    for (r = 0; r < sizeof(requests) / sizeof(requests[0]); r++)
    {
        size_t len = strlen(requests[r]);

        if (parse(requests[r], 0, &want) != 0)
        {
            printf("FAIL request %zu: consumed more than its length\n", r);
            failures++;
            continue;
        }

        for (cut = 1; cut < len; cut++)
        {
            int rc = parse(requests[r], cut, &got);

            if (rc != 0 || strcmp(want.fields, got.fields) != 0)
            {
                printf("FAIL request %zu split at %zu\n--- want\n%s--- got\n%s",
                       r, cut, want.fields, got.fields);
                failures++;
            }
        }
    }

    printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
