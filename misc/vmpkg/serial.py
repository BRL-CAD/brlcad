#!/usr/bin/env python3
#                     S E R I A L . P Y
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

"""Enforce serial builds and tests without modifying release sources."""

from contextlib import contextmanager
import os
from pathlib import Path
import shutil
import sys


def run(executable):
    arguments = []
    pending = iter(sys.argv[1:])
    jobs_operand = False
    for argument in pending:
        if jobs_operand and argument.isdecimal():
            jobs_operand = False
            continue
        jobs_operand = False
        if argument == '--':
            arguments.extend([argument, *pending])
            break
        if argument in ('-j', '--jobs', '--parallel'):
            jobs_operand = True
        elif not argument.startswith(('-j', '--jobs=', '--parallel=')):
            arguments.append(argument)
    os.execv(executable, [executable, '-j1', *arguments])


@contextmanager
def build_tools(directory):
    directory.mkdir(parents=True, exist_ok=True)
    serial_environment = {'MAKEFLAGS': '-j1', 'MFLAGS': '-j1',
                          'CMAKE_BUILD_PARALLEL_LEVEL': '1', 'CTEST_PARALLEL_LEVEL': '1'}
    original = {name: os.environ.get(name) for name in ('PATH', 'MAKE', *serial_environment)}
    try:
        for name in ('make', 'gmake', 'ninja', 'ctest'):
            executable = shutil.which(name)
            if executable is None:
                continue
            wrapper = directory / name
            wrapper.write_text('#!' + sys.executable + '\n'
                               'import runpy\n'
                               'runpy.run_path(' + repr(str(Path(__file__).resolve())) +
                               ')["run"](' + repr(executable) + ')\n')
            wrapper.chmod(0o755)
            if name == 'make':
                # GCC's LTO driver honors MAKE independently of PATH.
                serial_environment['MAKE'] = str(wrapper)
        os.environ['PATH'] = str(directory) + os.pathsep + os.environ['PATH']
        os.environ.update(serial_environment)
        yield
    finally:
        for name, value in original.items():
            if value is None:
                os.environ.pop(name, None)
            else:
                os.environ[name] = value
