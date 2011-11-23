#      C H E C K P R O T O T Y P E E X I S T S . C M A K E
# BRL-CAD
#
# Copyright (c) 2006-2011 United States Government as represented by
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
# - Check if the prototype for a function exists.
# CHECK_PROTOTYPE_EXISTS (FUNCTION HEADER VARIABLE)
#
#  FUNCTION - the name of the function you are looking for
#  HEADER - the header(s) where the prototype should be declared
#  VARIABLE - variable to store the result
#
# The following variables may be set before calling this macro to
# modify the way the check is run:
#
#  CMAKE_REQUIRED_FLAGS = string of compile command line flags
#  CMAKE_REQUIRED_DEFINITIONS = list of macros to define (-DFOO=bar)
#  CMAKE_REQUIRED_INCLUDES = list of include directories

# Copyright (c) 2006, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


INCLUDE(CheckCXXSourceCompiles)

MACRO (CHECK_PROTOTYPE_EXISTS _SYMBOL _HEADER _RESULT)
   SET(_INCLUDE_FILES)
   FOREACH (it ${_HEADER})
      SET(_INCLUDE_FILES "${_INCLUDE_FILES}#include <${it}>\n")
   ENDFOREACH (it)

   SET(_CHECK_PROTO_EXISTS_SOURCE_CODE "
${_INCLUDE_FILES}
int main()
{
#ifndef ${_SYMBOL}
   int i = sizeof(&${_SYMBOL});
#endif
  return 0;
}
")
   CHECK_CXX_SOURCE_COMPILES("${_CHECK_PROTO_EXISTS_SOURCE_CODE}" ${_RESULT})
ENDMACRO (CHECK_PROTOTYPE_EXISTS _SYMBOL _HEADER _RESULT)


# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
