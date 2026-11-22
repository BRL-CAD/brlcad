#                     C M A K E L I S T S . T X T
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

if(BUILD_TESTING AND UNIX)
  find_package(Python3 3.11.8 QUIET COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    brlcad_add_test(NAME vmpkg_tools COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
      -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*.py")
  endif()
endif()

cmakefiles(
  CMakeLists.txt
  .gitignore
  README.md
  base-packages.txt
  bext.py
  build.sh
  builder-packages.txt
  freebsd-packages.txt
  guest-build.py
  guest-check.py
  guest-env.py
  inputs.json
  kickstart.cfg.in
  package-relocate.cmake
  rocky-release.md
  rocky.repo
  runtime-packages.txt
  serial.py
  smoke.py
  test_bext.py
  test_package.py
  test_serial.py
  test_vmpkg.py
  vmpkg.py
)
