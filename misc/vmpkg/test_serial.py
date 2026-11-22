#!/usr/bin/env python3
#                     T E S T _ S E R I A L . P Y
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

"""Check serial build-tool overrides and environment restoration."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

import serial


def write_lock_worker(root):
    worker = root / 'worker.py'
    worker.write_text('import fcntl, time\n'
                      'with open("lock", "w") as lock:\n'
                      '    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)\n'
                      '    time.sleep(0.05)\n')
    return worker


@unittest.skipUnless(shutil.which('make') and shutil.which('ninja'), 'Make and Ninja are required')
class SerialTest(unittest.TestCase):
    def test_explicit_parallel_flags_and_recursive_make(self):
        # Release builds use GNU Make even on hosts that ship a BSD make.
        make = 'gmake' if shutil.which('gmake') else 'make'
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            worker = write_lock_worker(root)
            command = sys.executable + ' ' + str(worker)
            targets = ['job' + str(index) for index in range(8)]
            (root / 'Makefile').write_text(
                '.PHONY: all recursive ' + ' '.join(targets) + '\n'
                'all: ' + ' '.join(targets) + '\n'
                'recursive:\n\t$(MAKE) --jobs=8 all\n' +
                ''.join(target + ':\n\t' + command + '\n' for target in targets))
            (root / 'build.ninja').write_text(
                'rule work\n  command = ' + command + '\n' +
                ''.join('build ' + target + ': work\n' for target in targets))
            original = dict(os.environ)
            with serial.build_tools(root / 'tools'):
                for tool, flags in [(make, ['--jobs=8', 'recursive']),
                                    (make, ['-j', '--jobs=8', 'recursive']),
                                    (os.environ['MAKE'], ['-j8', 'all']),
                                    ('ninja', ['-j', '8']), ('ninja', ['-j8'])]:
                    result = subprocess.run([tool] + flags, cwd=root, text=True,
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    self.assertEqual(result.returncode, 0, result.stdout)
            self.assertEqual(dict(os.environ), original)

    @unittest.skipUnless(shutil.which('cmake') and shutil.which('ctest'), 'CMake and CTest are required')
    def test_cmake_check_overrides_explicit_ctest_jobs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            worker = write_lock_worker(root)
            (root / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.22)\n'
                'project(serial_tests NONE)\n'
                'enable_testing()\n'
                'foreach(index RANGE 1 8)\n'
                '  add_test(NAME job${index} COMMAND "' + sys.executable + '" "' + str(worker) + '")\n'
                'endforeach()\n'
                'add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -j8 --output-on-failure)\n')
            build = root / 'build'
            with serial.build_tools(root / 'tools'):
                commands = [
                    ['cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
                     '-DCMAKE_CTEST_COMMAND=' + shutil.which('ctest')],
                    ['cmake', '--build', str(build), '--target', 'check'],
                    ['ctest', '--test-dir', str(build), '--parallel', '8']]
                for command in commands:
                    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                            stderr=subprocess.STDOUT)
                    self.assertEqual(result.returncode, 0, result.stdout)

    def test_environment_is_restored_on_failure(self):
        original = dict(os.environ)
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(RuntimeError, 'failed'):
                with serial.build_tools(Path(temporary) / 'tools'):
                    raise RuntimeError('failed')
        self.assertEqual(dict(os.environ), original)


if __name__ == '__main__':
    unittest.main()
