#!/usr/bin/env python3
# Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import os
import platform
import sys
from distutils import dir_util

FLAGS = None

def log(msg):
    try:
        print(msg, file=sys.stderr)
    except Exception:
        print('<failed to log>', file=sys.stderr)

def fail(msg):
    fail_if(True, msg)


def fail_if(p, msg):
    if p:
        print('error: {}'.format(msg), file=sys.stderr)
        sys.exit(1)

def target_platform():
    if FLAGS.target_platform is not None:
        return FLAGS.target_platform
    return platform.system().lower()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('--src',
                        type=str,
                        required=True,
                        help='Source directory to install.')
    parser.add_argument('--dest',
                        type=str,
                        required=False,
                        help='Install directory. Will be created if necessary.')
    parser.add_argument('--dest-basename',
                        type=str,
                        required=False,
                        help='Name for the last segment of the destination directory. If not specified uses the basename of src path.')
    parser.add_argument(
        '--target-platform',
        required=False,
        default=None,
        help=
        'Target for build, can be "ubuntu", "windows" or "jetpack". If not specified, uses the current platform.'
    )

    FLAGS = parser.parse_args()

    if not FLAGS.dest:
        log('install_src: source not installed, no destination specified')
        sys.exit(0)

    # The destination directory within FLAGS.dest is the same as the
    # source dir unless an explicit basename was specified.
    if FLAGS.dest_basename:
        dest_dir = os.path.join(FLAGS.dest, FLAGS.dest_basename)
    else:
        dest_dir = os.path.join(FLAGS.dest, os.path.basename(FLAGS.src))

    log('install_src: installing src: {} -> {}'.format(FLAGS.src, dest_dir))

    if os.path.isdir(dest_dir):
        dir_util.remove_tree(dest_dir)

    dir_util.copy_tree(FLAGS.src, dest_dir, preserve_symlinks=True)
