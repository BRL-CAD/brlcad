#!/usr/bin/env python3
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

"""Build Linux PGO targets and print failure diagnostics without saving dumps."""

import argparse
from pathlib import Path
import re
import shlex
import shutil
import subprocess


MAX_SYMBOL_LINES_PER_INPUT = 20
FAILURE_TARGET = re.compile(r"^FAILED:\s+(?:\[code=[^]]+\]\s+)?(.+)$")
CACHE_SETTING = re.compile(
    r"^(CMAKE_(?:BUILD_TYPE|(?:C|CXX)_COMPILER|(?:C|CXX|EXE_LINKER|"
    r"SHARED_LINKER|MODULE_LINKER)_FLAGS[^:]*)|BRLCAD_(?:PGO[^:]*|LTO[^:]*|"
    r"OPTIMIZED|ENABLE_STRICT)):"
)
MAP_SYMBOLS = ("_M_emplace", "_M_create_node", "_M_construct_node")


def run_build(command):
    print("+ " + shlex.join(command), flush=True)
    failures = {}
    pending_targets = []
    with subprocess.Popen(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, errors="replace") as process:
        for line in process.stdout:
            print(line, end="", flush=True)
            failure = FAILURE_TARGET.match(line)
            if failure:
                pending_targets = shlex.split(failure.group(1))
            elif pending_targets:
                for target in pending_targets:
                    failures[target] = line.strip()
                pending_targets = []
        return process.wait(), failures


def print_command(command):
    print("+ " + shlex.join(command), flush=True)
    result = subprocess.run(command, check=False)
    if result.returncode:
        print(f"Diagnostic command exited with status {result.returncode}", flush=True)


def print_link_symbols(build_dir, commands, compiler):
    inputs = set()
    for command in commands:
        for argument in shlex.split(command):
            if Path(argument).suffix in (".o", ".a"):
                path = build_dir / argument
                if path.is_file():
                    inputs.add(str(path))
    if not inputs:
        return

    compiler_name = Path(compiler).name
    nm_name = compiler_name.replace("g++", "gcc-nm").replace("clang++", "llvm-nm")
    nm = shutil.which(nm_name) if nm_name != compiler_name else None
    nm = nm or shutil.which("nm")
    if not nm:
        print("No symbol reader is available", flush=True)
        return

    # Demangled tree symbols expose the map types hidden by GCC's LTO trace.
    print("::group::Map construction symbols in failed link inputs", flush=True)
    command = [nm, "-A", "-C", "--defined-only", *sorted(inputs)]
    print("+ " + shlex.join(command), flush=True)
    count = 0
    input_counts = {}
    with subprocess.Popen(command, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, errors="replace") as process:
        for line in process.stdout:
            if any(symbol in line for symbol in MAP_SYMBOLS):
                owner = line.split(":", 1)[0]
                input_count = input_counts.get(owner, 0)
                if input_count < MAX_SYMBOL_LINES_PER_INPUT:
                    print(line, end="", flush=True)
                input_counts[owner] = input_count + 1
                count += 1
            elif any(message in line for message in
                     (": error:", ": warning:", "file format not recognized", "plugin needed", "no symbols")):
                print(line, end="", flush=True)
        status = process.wait()
    print(f"Matched {count} symbols; displayed at most {MAX_SYMBOL_LINES_PER_INPUT} per input", flush=True)
    if status:
        print(f"Symbol reader exited with status {status}", flush=True)
    print("::endgroup::", flush=True)


def diagnose(build_dir, failures):
    print("::group::PGO toolchain and build settings", flush=True)
    cache = (build_dir / "CMakeCache.txt").read_text()
    compilers = set()
    for line in cache.splitlines():
        if CACHE_SETTING.match(line):
            print(line, flush=True)
        if line.startswith(("CMAKE_C_COMPILER:", "CMAKE_CXX_COMPILER:")):
            compilers.add(line.split("=", 1)[1])
    for compiler in sorted(compilers):
        print_command([compiler, "-v"])
    print_command(["cmake", "--version"])
    print_command(["ninja", "--version"])
    print_command(["ld", "--version"])
    print("::endgroup::", flush=True)

    if not failures:
        print("No failed Ninja targets were reported; skipping the serial retry", flush=True)
        return

    print("::group::Serial verbose retry of failed PGO targets", flush=True)
    run_build(["cmake", "--build", str(build_dir), "--parallel", "1",
               "--verbose", "--target", *failures])
    print("::endgroup::", flush=True)

    cxx = re.search(r"^CMAKE_CXX_COMPILER:[^=]+=(.+)$", cache, re.MULTILINE)
    if cxx:
        print_link_symbols(build_dir, failures.values(), cxx.group(1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("targets", nargs="+")
    args = parser.parse_args()
    build_dir = args.build_dir.resolve()
    status, failures = run_build(["cmake", "--build", str(build_dir),
                                  "--target", *args.targets])
    if status:
        try:
            diagnose(build_dir, failures)
        except (OSError, ValueError) as error:
            print(f"Unable to finish PGO diagnostics: {error}", flush=True)
    # A successful diagnostic retry must not turn the original failure green.
    return status if status >= 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())


# Local Variables:
# mode: python
# tab-width: 8
# indent-tabs-mode: nil
# python-indent-offset: 4
# End:
# ex: shiftwidth=4 tabstop=8
