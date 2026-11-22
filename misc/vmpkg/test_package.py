#!/usr/bin/env python3
#                     T E S T _ P A C K A G E . P Y
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

"""Check SDK metadata relocation in actual CPack tar packages."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from vmpkg import HERE, extract_archive


@unittest.skipUnless(all(shutil.which(tool) for tool in ('cmake', 'cpack')), 'CMake and CPack are required')
class PackageTest(unittest.TestCase):
    def run_command(self, command):
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_sdk_metadata_follows_extracted_prefix(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'source'
            source.mkdir()
            (source / 'library').write_text('fixture library\n')
            (source / 'CMakeLists.txt').write_text(r'''cmake_minimum_required(VERSION 3.22)
project(SDK NONE)
install(FILES library DESTINATION lib)
install(CODE [[
  string(REGEX REPLACE "/+$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
  file(MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/lib/cmake/SDK" "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
  file(WRITE "${CMAKE_INSTALL_PREFIX}/lib/cmake/SDK/SDKConfig.cmake"
    "set(SDK_ROOT \"${CMAKE_INSTALL_PREFIX}\")\n")
  file(WRITE "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/sdk.pc"
    "prefix=${CMAKE_INSTALL_PREFIX}\nlibdir=${CMAKE_INSTALL_PREFIX}/lib\n")
]])
set(CPACK_PACKAGE_VERSION 1.0)
set(CPACK_PACKAGE_FILE_NAME sdk)
set(CPACK_PACKAGING_INSTALL_PREFIX "")
include(CPack)
''')
            self.run_command(['cmake', '-S', str(source), '-B', str(root / 'build')])
            reference = None
            for generator, suffix in [('TGZ', '.tar.gz'), ('TBZ2', '.tar.bz2')]:
                self.run_command(['cpack', '--config', str(root / 'build/CPackConfig.cmake'),
                                  '-G', generator, '-B', str(root / generator), '-D',
                                  'CPACK_PRE_BUILD_SCRIPTS=' + str(HERE / 'package-relocate.cmake')])
                extracted = root / (generator + '-extracted')
                extract_archive(root / generator / ('sdk' + suffix), extracted)
                prefix = extracted / 'sdk'
                config = prefix / 'lib/cmake/SDK/SDKConfig.cmake'
                pc = prefix / 'lib/pkgconfig/sdk.pc'
                contents = [config.read_text(), pc.read_text()]
                self.assertNotIn(str(root / generator), ''.join(contents))
                self.assertIn('${pcfiledir}/../../lib', contents[1])
                if reference is not None:
                    self.assertEqual(contents, reference)
                reference = contents
                check = root / 'check.cmake'
                check.write_text('include("' + str(config) + '")\n'
                                 'get_filename_component(actual "${SDK_ROOT}" REALPATH)\n'
                                 'if(NOT actual STREQUAL "' + str(prefix) + '")\n'
                                 '  message(FATAL_ERROR "SDK root differs: ${actual}")\n'
                                 'endif()\n')
                self.run_command(['cmake', '-P', str(check)])


if __name__ == '__main__':
    unittest.main()
