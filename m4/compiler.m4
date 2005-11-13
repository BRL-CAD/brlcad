#                   C O M P I L E R . M 4
# BRL-CAD
#
# Copyright (C) 2005 United States Government as represented by
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
#
# BC_COMPILER_AND_LINKER_RECOGNIZES
# checks if the compiler will work with the specified flags
#
# BC_COMPILER_RECOGNIZES
# checks if the compiler will work with the specified cflags
#
# BC_LINKER_RECOGNIZES
# checks if the linker will work with the specified ldflags
#
# BC_PREPROCESSOR_RECOGNIZES
# checks if the preprocessor will work with the specified cppflags
###

AC_DEFUN([BC_COMPILER_AND_LINKER_RECOGNIZES], [
__flag="$1"
bc_[$2]_works=yes
AC_MSG_CHECKING([if compiler and linker recognize $__flag])
PRECFLAGS="$CFLAGS"
PRELDFLAGS="$LDFLAGS"
CFLAGS="$CFLAGS $__flag"
LDFLAGS="$LDFLAGS $__flag"
m4_pushdef([AC_TRY_EVAL], [_AC_EVAL_STDERR]($$[1]))
AC_TRY_COMPILE( [], [int i;], [if AC_TRY_COMMAND([grep "nrecognize" conftest.err >/dev/null 2>&1]) ; then bc_[$2]_works=no ; fi], [bc_[$2]_works=no])
m4_popdef([AC_TRY_EVAL])
rm -f conftest.err
AC_TRY_RUN( [
#include <stdio.h>
int main(){exit(0);}
], [], [bc_[$2]_works=no])
AC_MSG_RESULT($bc_[$2]_works)
if test "x$bc_[$2]_works" = "xno" ; then
	CFLAGS="$PRECFLAGS"
	LDFLAGS="$PRELDFLAGS"
fi
])

AC_DEFUN([BC_COMPILER_AND_LINKER_RECOGNIZE], [
BC_COMPILER_AND_LINKER_RECOGNIZES([$1], [$2])
])


AC_DEFUN([BC_COMPILER_RECOGNIZES], [
__flag="$1"
AC_MSG_CHECKING([if compiler recognizes $__flag])
bc_[$2]_works=yes
PRECFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $__flag"
m4_pushdef([AC_TRY_EVAL], [_AC_EVAL_STDERR]($$[1]))
AC_TRY_COMPILE( [], [int i;], [if AC_TRY_COMMAND([grep "nrecognize" conftest.err >/dev/null 2>&1]) ; then bc_[$2]_works=no ; fi], [bc_[$2]_works=no])
m4_popdef([AC_TRY_EVAL])
rm -f conftest.err
AC_MSG_RESULT($bc_[$2]_works)
if test "x$bc_[$2]_works" = "xno" ; then
	CFLAGS="$PRECFLAGS"
fi
])

AC_DEFUN([BC_COMPILER_RECOGNIZE], [
BC_COMPILER_RECOGNIZES([$1], [$2])
])


AC_DEFUN([BC_LINKER_RECOGNIZES], [
__flag="$1"
AC_MSG_CHECKING([if linker recognizes $__flag])
bc_[$2]_works=yes
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $__flag"
m4_pushdef([AC_TRY_EVAL], [_AC_EVAL_STDERR]($$[1]))
AC_TRY_LINK( [], [int i;], [if AC_TRY_COMMAND([grep "nrecognize" conftest.err >/dev/null 2>&1]) ; then bc_[$2]_works=no ; fi], [bc_[$2]_works=no])
m4_popdef([AC_TRY_EVAL])
rm -f conftest.err
AC_MSG_RESULT($bc_[$2]_works)
if test "x$bc_[$2]_works" = "xno" ; then
	LDFLAGS="$PRELDFLAGS"
fi
])

AC_DEFUN([BC_LINKER_RECOGNIZE], [
BC_LINKER_RECOGNIZES([$1], [$2])
])


AC_DEFUN([BC_PREPROCESSOR_RECOGNIZES], [
__flag="$1"
AC_MSG_CHECKING([if preprocesser recognizes $__flag])
bc_[$2]_works=yes
PRECPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $__flag"
m4_pushdef([AC_TRY_EVAL], [_AC_EVAL_STDERR]($$[1]))
AC_TRY_COMPILE( [], [int i;], [if AC_TRY_COMMAND([grep "nrecognize" conftest.err >/dev/null 2>&1]) ; then bc_[$2]_works=no ; fi], [bc_[$2]_works=no])
m4_popdef([AC_TRY_EVAL])
rm -f conftest.err
AC_MSG_RESULT($bc_[$2]_works)
if test "x$bc_[$2]_works" = "xno" ; then
	CPPFLAGS="$PRECPPFLAGS"
fi
])

AC_DEFUN([BC_PREPROCESSOR_RECOGNIZE], [
BC_PREPROCESSOR_RECOGNIZES([$1], [$2])
])

# Local Variables:
# mode: m4
# tab-width: 8
# standard-indent: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
