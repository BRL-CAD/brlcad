#                     C M A K E L I S T S . T X T
# BRL-CAD
#
# Copyright (c) 2010-2021 United States Government as represented by
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


#  ************ libdm tests *************

BRLCAD_ADDEXEC(dm_test dm_test.c "libdm;libbu" TEST)

if (BRLCAD_ENABLE_TK)
  include_directories(
    ${TCL_INCLUDE_PATH}
    ${TK_INCLUDE_PATH}
    )

  BRLCAD_ADDEXEC(tcl_img tcl_img.cpp "libdm;libbu;${TCL_LIBRARY};${TK_LIBRARY}" TEST_USEDATA)
  BRLCAD_ADDEXEC(tcl_tevent tcl_tevent.cpp "libdm;libbu;${TCL_LIBRARY};${TK_LIBRARY}" TEST_USEDATA)
endif (BRLCAD_ENABLE_TK)

CMAKEFILES(CMakeLists.txt)
CMAKEFILES(
  tcl_img.cpp
  tcl_tevent.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8


