#!/usr/bin/env python3
#                     G U E S T - C H E C K . P Y
# BRL-CAD
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
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
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
###

"""Build and test a BRL-CAD source snapshot on a provisioned guest."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from vmpkg import digest, extract_archive, guest_lock, run, virtual_display


def check(args):
    work = args.work.resolve()
    options = ['-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=ON',
               '-DBRLCAD_ENABLE_STRICT=ON', '-DBRLCAD_ENABLE_QT=ON',
               '-DBRLCAD_ENABLE_X11=ON', '-DBRLCAD_ENABLE_OPENGL=ON',
               '-DBRLCAD_X11_PROVIDER=SYSTEM',
               '-DBRLCAD_EXTRADOCS=OFF', '-DBUILD_STATIC_LIBS=OFF',
               '-DBRLCAD_LTO_MODE=OFF'] + args.cmake_arg
    receipt = {'sources_sha256': digest(args.sources), 'cmake_options': options,
               'bext_dir': str(args.bext_dir.resolve()) if args.bext_dir else None}
    work.mkdir(parents=True, exist_ok=True)
    with guest_lock(work):
        check_locked(args, work, options, receipt)


def check_locked(args, work, options, receipt):
    sources = work / 'sources'
    identity = sources / 'inputs.json'
    if sources.exists():
        if json.loads(identity.read_text()) != receipt:
            raise ValueError('Build inputs changed; use a new work directory')
    else:
        if any(entry.name != '.running.lock' for entry in work.iterdir()):
            raise ValueError('Use an empty work directory')
        with tempfile.TemporaryDirectory(prefix='.sources-', dir=work) as temporary:
            pending = Path(temporary) / 'sources'
            extract_archive(args.sources, pending)
            if not (pending / 'brlcad' / 'CMakeLists.txt').is_file():
                raise ValueError('The source snapshot must include BRL-CAD')
            with (pending / identity.name).open('x') as metadata:
                json.dump(receipt, metadata, indent=2)
                metadata.write('\n')
            pending.rename(sources)

    build = work / 'build'
    configure = ['cmake', '-S', sources / 'brlcad', '-B', build, '-G', 'Ninja']
    configure += ['-DBRLCAD_EXT_PARALLEL=' + str(args.jobs)] + options
    if args.bext_dir:
        configure.append('-DBRLCAD_EXT_DIR=' + str(args.bext_dir.resolve()))
    elif (sources / 'bext' / 'CMakeLists.txt').is_file():
        configure.append('-DBRLCAD_EXT_SOURCE_DIR=' + str(sources / 'bext'))

    tests = ['ctest', '--test-dir', build, '--output-on-failure',
             '--exclude-regex', '^NOTE:', '--no-tests=error']
    if args.test_regex:
        tests += ['--tests-regex', args.test_regex]
    environment = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL=str(args.jobs), LIBGL_ALWAYS_SOFTWARE='1')
    environment.setdefault('CTEST_PARALLEL_LEVEL', str(args.jobs))
    for name, command in [('configure', configure),
                          ('graphics', ['cmake', '--build', build, '--parallel', args.jobs,
                                        '--target', 'dm-X', 'dm-ogl', 'qged']),
                          ('build', ['cmake', '--build', build, '--parallel', args.jobs])]:
        log = work / (name + '.log')
        print('Log: ' + str(log), flush=True)
        with log.open('a') as output:
            run(command, stdout=output, stderr=subprocess.STDOUT, env=environment)
    print('Log: ' + str(work / 'test.log'), flush=True)
    with virtual_display(work / 'Xvfb.log'), (work / 'test.log').open('a') as output:
        environment['DISPLAY'] = os.environ['DISPLAY']
        run(tests, stdout=output, stderr=subprocess.STDOUT, env=environment)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sources', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--bext-dir', type=Path, help='Use an existing native bext output directory')
    parser.add_argument('--cmake-arg', action='append', default=[],
                        help='Additional configure argument, such as --cmake-arg=-DBRLCAD_ENABLE_STEP=OFF')
    parser.add_argument('--test-regex', help='Run only the CTest tests matching this expression')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    for value in args.cmake_arg:
        if not re.fullmatch(r'-D[A-Za-z_][A-Za-z_0-9]*(:[A-Z]+)?=.*', value):
            parser.error('--cmake-arg expects a CMake cache definition such as -DOPTION=ON')
    if os.geteuid() == 0:
        parser.error('Run builds as an unprivileged guest user')
    for executable in ('cmake', 'ninja', 'ctest', 'Xvfb', 'xdotool', 'xdpyinfo'):
        if not shutil.which(executable):
            parser.error('Missing build or test tool: ' + executable)
    try:
        check(args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\nSee the logs in the work directory.\n')


if __name__ == '__main__':
    main()
