#!/usr/bin/env python3
#
#    Copyright (c) 2026 Rafael Micro
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

"""Compiles chip_pch.h into a GCC precompiled header.

GCC only accepts a .gch whose command line matches the translation unit that
consumes it: every -D that the header looked at has to have the same value, and
the -march/-mabi/-std flags have to be identical.  Reproducing that flag set by
hand in GN would mean duplicating ~70 defines and ~60 include directories that
are assembled from about thirty different GN configs, and any drift would
silently disable the PCH.

Instead the flags are recovered from compile_commands.json, using one source
file of the GN target that owns the PCH config (src/platform/RT58x) as the
reference.  That guarantees the .gch is built with exactly the flags the
consumers see, and it keeps working when configs change.

Run from the ninja build directory.
"""

import argparse
import json
import os
import shlex
import subprocess
import sys

_FLAGS_WITH_VALUE_TO_DROP = frozenset(["-o", "-MF", "-MT", "-MQ"])
_FLAGS_TO_DROP = frozenset(["-c", "-MD", "-MMD", "-Winvalid-pch", "-Wno-error=invalid-pch"])
_SOURCE_SUFFIXES = (".c", ".cc", ".cpp", ".cxx", ".S", ".s")


def reference_flags(reference_source, pch_name):
    """Returns the compiler + flags used to build `reference_source`."""
    if not os.path.exists("compile_commands.json"):
        sys.exit(
            "build_pch.py: compile_commands.json not found in %s.\n"
            "The precompiled header needs it to discover the compile flags; run\n"
            "gn gen with --export-compile-commands, or set rt58x_use_pch=false." % os.getcwd())

    with open("compile_commands.json") as db_file:
        database = json.load(db_file)

    entry = next((e for e in database if e["file"].endswith(reference_source)), None)
    if entry is None:
        sys.exit("build_pch.py: no compile command for reference source %s.\n"
                 "Point --reference-source at a source file of the target that "
                 "carries the PCH config." % reference_source)

    argv = shlex.split(entry["command"])
    flags = []
    index = 0
    while index < len(argv):
        flag = argv[index]
        if flag in _FLAGS_WITH_VALUE_TO_DROP:
            index += 2
            continue
        if flag in _FLAGS_TO_DROP:
            index += 1
            continue
        if flag == "-include" and index + 1 < len(argv) and os.path.basename(argv[index + 1]) == pch_name:
            index += 2
            continue
        if flag.endswith(_SOURCE_SUFFIXES) and not flag.startswith("-"):
            index += 1
            continue
        flags.append(flag)
        index += 1

    return flags


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--header", required=True, help="chip_pch.h in the source tree")
    parser.add_argument("--out-dir", required=True, help="directory to hold chip_pch.h and chip_pch.h.gch")
    parser.add_argument("--reference-source", required=True,
                        help="source file whose compile flags the PCH must match")
    parser.add_argument("--depfile", required=True, help="depfile to write for ninja")
    args = parser.parse_args()

    pch_name = os.path.basename(args.header)
    staged_header = os.path.join(args.out_dir, pch_name)
    output = staged_header + ".gch"

    os.makedirs(args.out_dir, exist_ok=True)

    with open(args.header) as source, open(staged_header, "w") as staged:
        staged.write(source.read())

    command = reference_flags(args.reference_source, pch_name)
    command += [
        "-x", "c++-header",
        "-MMD", "-MF", args.depfile, "-MT", output,
        args.header,
        "-o", output,
    ]

    result = subprocess.run(command)
    if result.returncode != 0:
        if os.path.exists(output):
            os.remove(output)
        sys.stderr.write("build_pch.py: failed command was:\n  %s\n" % shlex.join(command))
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
