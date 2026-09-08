#!/usr/bin/env python3
#
#    Copyright (c) 2025 Rafael Micro
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
#
"""Compare RT58x flash/RAM usage between two sets of size reports.

Each size report is the output of `arm-none-eabi-size -A <elf>`, as produced by
scripts/examples/gn_rt58x_example.sh and collected by the build workflow as
<CHIP>-<example>-example-size.txt.

The report is informational: it always exits 0, so a size increase never blocks
a pull request.

Usage:
    compare_sizes.py --current <dir> [--base <dir>] [--warn-on-growth-percent 1.0]
"""

import argparse
import os
import sys
from pathlib import Path

# RT583 places flash at 0x00000000 and RAM at 0x20000000; RT584/RT584HA4 place
# flash at 0x10000000 and RAM at 0x30000000 (see
# examples/platform/rt58x/ldscript/*.ld). A single split at 0x20000000
# therefore classifies loadable sections correctly for every supported board.
RAM_BASE = 0x20000000

SIZE_FILE_SUFFIX = "-size.txt"


def parse_size_report(path: Path) -> dict:
    """Parse `arm-none-eabi-size -A` output into {section_name: (size, addr)}."""
    sections = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if len(fields) != 3 or not fields[0].startswith("."):
            # Skips the file header, the "section size addr" header and the
            # "Total <n>" line (which includes debug sections and is meaningless
            # as a footprint metric).
            continue
        try:
            size, addr = int(fields[1]), int(fields[2])
        except ValueError:
            continue
        sections[fields[0]] = (size, addr)
    return sections


def footprint(sections: dict) -> tuple:
    """Return (flash_bytes, ram_bytes) for a parsed size report."""
    flash = ram = 0
    for name, (size, addr) in sections.items():
        if addr == 0:
            # Debug info, .comment, .ARM.attributes: not loaded onto the device.
            continue
        if addr >= RAM_BASE:
            ram += size
        else:
            flash += size
    # .data lives in RAM at run time but its initialisers are stored in flash,
    # so it costs both.
    flash += sections.get(".data", (0, 0))[0]
    return flash, ram


def collect(directory: Path) -> dict:
    """Return {example_name: (flash, ram)} for every size report in a tree."""
    results = {}
    if not directory or not directory.is_dir():
        return results
    for path in sorted(directory.rglob("*" + SIZE_FILE_SUFFIX)):
        name = path.name[: -len(SIZE_FILE_SUFFIX)]
        results[name] = footprint(parse_size_report(path))
    return results


def format_delta(current: int, base) -> str:
    if base is None:
        return "new"
    delta = current - base
    if delta == 0:
        return "0"
    percent = (delta / base * 100) if base else 0.0
    return f"{delta:+,} ({percent:+.2f}%)"


def growth_percent(current: int, base) -> float:
    if not base:
        return 0.0
    return (current - base) / base * 100


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--current", type=Path, required=True,
                        help="directory holding the size reports for this build")
    parser.add_argument("--base", type=Path, default=None,
                        help="directory holding the size reports to compare against")
    parser.add_argument("--warn-on-growth-percent", type=float, default=None,
                        help="call out examples whose flash or RAM grew by more than "
                             "this percentage (default: no call-out). This never fails "
                             "the build; the report is informational only.")
    parser.add_argument("--output", type=Path, default=None,
                        help="file to append the markdown report to "
                             "(default: $GITHUB_STEP_SUMMARY, else stdout)")
    args = parser.parse_args()

    current = collect(args.current)
    base = collect(args.base)

    lines = ["## RT58x size report", ""]

    if not current:
        lines += ["No size reports found for this build.", ""]
        write(lines, args.output)
        return 0

    if not base:
        lines += ["> No baseline size reports available "
                  "(no successful base-branch run, or its artifacts expired). "
                  "Showing absolute sizes only.", ""]

    lines += ["| Example | Flash | Δ Flash | RAM | Δ RAM |",
              "| --- | ---: | ---: | ---: | ---: |"]

    notable = []
    for name in sorted(current):
        flash, ram = current[name]
        base_flash, base_ram = base.get(name, (None, None))
        lines.append(f"| {name} | {flash:,} | {format_delta(flash, base_flash)} "
                     f"| {ram:,} | {format_delta(ram, base_ram)} |")

        if args.warn_on_growth_percent is None:
            continue
        for label, value, base_value in (("flash", flash, base_flash), ("RAM", ram, base_ram)):
            if base_value and growth_percent(value, base_value) > args.warn_on_growth_percent:
                notable.append(
                    f"{name}: {label} grew by {format_delta(value, base_value)}")

    dropped = sorted(set(base) - set(current))
    if dropped:
        lines += ["", "Not built in this run: " + ", ".join(dropped)]

    if notable:
        lines += ["", f"### Size increases (> {args.warn_on_growth_percent}%)", ""]
        lines += [f"- {r}" for r in notable]

    lines.append("")
    write(lines, args.output)

    for entry in notable:
        print(f"::warning::{entry}", file=sys.stderr)

    # Always succeeds: this report is informational and never blocks a pull request.
    return 0


def write(lines: list, output: Path):
    text = "\n".join(lines) + "\n"
    target = output or os.environ.get("GITHUB_STEP_SUMMARY")
    if target:
        with open(target, "a") as handle:
            handle.write(text)
    print(text)


if __name__ == "__main__":
    sys.exit(main())
