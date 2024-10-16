#!/usr/bin/env python3
# Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
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

"""
This is a basic patch tool that can parse unified diff format patches
and apply them to files. It is not a full-featured patch tool and
only supports a subset of the unified diff format, just enough to
apply the patches we need. It won't properly validate the patch file
format, so it's up to the user to ensure the patch file is correct.
It's also not optimized for performance, so it may be slow for large
patches or files.
"""

import os
import sys
from enum import Enum

class FileAlreadyPatchedError(Exception):
    pass

class FileNotMatchingError(Exception):
    pass

class PatchMalformedError(Exception):
    pass

class LineType(Enum):
    MATCH = ' '
    ADD = '+'
    REMOVE = '-'

class PatchLine:
    def __init__(self, line, line_type):
        self.line = line
        self.line_type = line_type

    def __str__(self):
        return f"{self.line_type.value} {self.line}"

class Hunk:
    def __init__(self, filename, location, count):
        self.filename = filename
        self.location = int(location)
        self.count = int(count)
        self.lines = []

    def apply(self):
        with open(self.filename, 'r') as file:
            lines = file.readlines()

        output = []
        current_line = 1
        relative_line = 0
        for line in lines:
            line = line.rstrip('\r\n')
            # Pass through the lines not inside the hunk
            if current_line < int(self.location) or current_line >= int(self.location + self.count):
                output.append(line)
            else:
                if self.lines[relative_line].line_type == LineType.MATCH:
                    if self.lines[relative_line].line != line:
                        raise FileNotMatchingError(f"Patch failed: {self.filename}:{current_line} does not match expected line:\nSource: {line}\nTarget: {self.lines[relative_line].line}")
                    output.append(line)
                    relative_line += 1
                elif self.lines[relative_line].line_type == LineType.ADD:
                    # Verify we're not already patched
                    if self.lines[relative_line].line == line:
                        raise FileAlreadyPatchedError(f"Patch failed: {self.filename}:{current_line} already patched")
                    # Write lines as long as we have a streak of ADD lines
                    while self.lines[relative_line].line_type == LineType.ADD:
                        output.append(self.lines[relative_line].line)
                        relative_line += 1
                    # A streak of ADD lines is supposed to be followed by a MATCH line
                    if self.lines[relative_line].line_type == LineType.MATCH:
                        # We still want to verify the line matches
                        if self.lines[relative_line].line != line:
                            raise FileNotMatchingError(f"Patch failed: {self.filename}:{current_line} does not match expected line:\nSource: {line}\nTarget: {self.lines[relative_line].line}")
                        output.append(line)
                        relative_line += 1
                    else:
                        raise PatchMalformedError(f"Unexpected line type: {self.lines[relative_line].line_type}")
                elif self.lines[relative_line].line_type == LineType.REMOVE:
                    relative_line += 1
            current_line += 1
        with open(self.filename, 'w') as file:
            file.write('\n'.join(output))

    def __str__(self):
        return f"Hunk: {self.filename} {self.location},{self.count}\n" + '\n'.join([str(line) for line in self.lines])

class Patch:
    def __init__(self, filename):
        self.filename = filename
        self.hunks = []

    def parse(self):
        with open(self.filename, 'r') as file:
            lines = file.readlines()

        hunk = None
        current_filename = None
        for line in lines:
            line = line.rstrip('\r\n')
            # Lines will start with '---', '+++', '@@', '-', ' ', or '+'. We
            # will not support patch files coming from git, with lines starting
            # with 'diff' or 'index'.
            if line.startswith('---'):
                # Ignore "old" filename
                continue
            elif line.startswith('+++'):
                # Store "new" filename
                current_filename = line[4:].lstrip(' ').split()[0]
            elif line.startswith('@@') and line.endswith('@@'):
                # Parse the hunk header's line numbers.
                # We only care about the new location, and old count. I know
                # this looks odd, but it makes sense when you look at how the
                # hunk is applied.
                old, new = line[3:-3].split()
                location = new.split(',')[0][1:]
                count = old.split(',')[1]
                if hunk:
                    self.hunks.append(hunk)
                hunk = Hunk(current_filename, location, count)
            elif line.startswith('-'):
                hunk.lines.append(PatchLine(line[1:], LineType.REMOVE))
            elif line.startswith('+'):
                hunk.lines.append(PatchLine(line[1:], LineType.ADD))
            elif line.startswith(' '):
                hunk.lines.append(PatchLine(line[1:], LineType.MATCH))
            else:
                # Throw an error if we encounter an unexpected line
                raise Exception(f"Unexpected line: {line}")

        if hunk:
            self.hunks.append(hunk)

    def apply(self):
        for hunk in self.hunks:
            hunk.apply()

    def __str__(self):
        return f"Patch: {self.filename}\n" + '\n'.join([str(hunk) for hunk in self.hunks])


def usage():
    print("Usage: patch.py <command> [options] <patchfile>")
    print("Commands:")
    print("  apply - Apply the patch")
    print("     -i, --ignore-already-patched - Ignore already patched files")
    print("     -d, --directory <directory>  - Directory to apply the patch to")
    print("  parse - Parse the patch file")
    print("  help  - Show this help message")
    sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        usage()

    command = sys.argv[1]
    if command == 'help':
        usage()
    elif command == 'parse':
        patch = Patch(sys.argv[2])
        patch.parse()
        print(patch)
    elif command == 'apply':
        ignore_already_patched = False
        # Parse options
        for i in range(2, len(sys.argv) - 1):
            if sys.argv[i] in ['-i', '--ignore-already-patched']:
                ignore_already_patched = True
            elif sys.argv[i] in ['-d', '--directory']:
                os.chdir(sys.argv[i + 1])
        patch = Patch(sys.argv[-1])
        patch.parse()
        try:
            patch.apply()
        except FileAlreadyPatchedError as e:
            if ignore_already_patched:
                print(f"Ignoring already patched file: {e}")
            else:
                raise
    else:
        usage()
