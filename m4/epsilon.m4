#                    E P S I L O N . M 4
# BRL-CAD
#
# Copyright (c) 2005-2006 United States Government as represented by
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
# BC_TRY_RUN_OUTPUT
# attempts to compile and run the provided program, saving the output
# in the provided variable
#
# BC_FLOAT_EPSILON
# determine the single-precision floating point tolerance epsilon
#
# BC_DOUBLE_EPSILON
# determine the double-precision floating point tolerance epsilon
#
# BC_COMPLIANT_FLOAT
# determine whether the single-precision floating point epsilon seems
# IEEE 754 compliant by computing the power of -23
#
# BC_COMPLIANT_DOUBLE
# determine whether the double-precision floating point epsilon seems
# IEEE 754 compliant by computing the power of -52
#
###

dnl BC_TRY_RUN_OUTPUT(VARIABLE, PROGRAM)
AC_DEFUN([BC_TRY_RUN_OUTPUT], [
AC_REQUIRE([AC_PROG_CC])
cat > conftest.$ac_ext <<EOF
[#]line __oline__ "configure"
#include "confdefs.h"
[$2]
EOF
if AC_TRY_EVAL(ac_link) && test -s conftest${ac_exeext} && (./conftest > conftest.stdout; exit) 2>/dev/null ; then
	$1=`cat ./conftest.stdout`
else
	echo "configure: failed program was:" >&AC_FD_CC
	cat conftest.$ac_ext >&AC_FD_CC
	$1=""
fi
rm -fr conftest*
])


# BC_FLOAT_EPSILON()
AC_DEFUN([BC_FLOAT_EPSILON], [
dnl determine the minimum single-precision floating point tolerance
dnl value at compile time such that: 1.0 + value != 1.0
dnl ANSI defines this as FLT_EPSILON but float.h may not provide it.
AC_MSG_CHECKING([single-precision floating point tolerance])
BC_TRY_RUN_OUTPUT(bc_flt_epsilon, [
#include <stdio.h>
int main (int ac, char *av[]) {
    /* must be volatile to avoid long registers */
    volatile float tol = 1.0f;
    while (1.0f + (tol * 0.5f) != 1.0f) {
	tol *= 0.5f;
    }
    printf("%.20e", tol);
    return 0;
}
])
AC_MSG_RESULT([$bc_flt_epsilon])
])


# BC_DOUBLE_EPSILON()
AC_DEFUN([BC_DOUBLE_EPSILON], [
dnl determine the minimum double-precision floating point tolerance
dnl value at compile time such that: 1.0 + value != 1.0
dnl ANSI defines this as DBL_EPSILON but float.h may not provide it.
AC_MSG_CHECKING([double-precision floating point tolerance])
BC_TRY_RUN_OUTPUT(bc_dbl_epsilon, [
#include <stdio.h>
int main (int ac, char *av[]) {
    /* must be volatile to avoid long registers */
    volatile double tol = 1.0;
    while (1.0 + (tol * 0.5) != 1.0) {
	tol *= 0.5;
    }
    printf("%.20e", tol);
    return 0;
}
])
AC_MSG_RESULT([$bc_dbl_epsilon])
])


# BC_COMPLIANT_FLOAT()
AC_DEFUN([BC_COMPLIANT_FLOAT], [
dnl determine whether the floating point implementation seems to be
dnl IEEE 754 compliant by checking whether the tolerance epsilon 
dnl matches -23 power.  make sure 0 == -0 too.
AC_MSG_CHECKING([whether floats conform to IEEE 754])
AC_TRY_RUN([
typedef union {
    float f;
    unsigned short int i[2];
} S;
int main (int ac, char *av[]) {
    /* must be volatile to avoid long registers */
    volatile float tol = 1.0f;
    volatile float power = 1.0f;
    int n = 23;
    S f;
    while (1.0f + (tol * 0.5f) != 1.0f) {
	tol *= 0.5f;
    }
    while (n-- > 0) {
	power *= 0.5f;
    }
    if (tol != power) {
	return 1;
    }
    /* make sure 0 == -0 */
    tol = 0.0f; power = -tol;
    if (tol != power) {
	return 2;
    }
    /* make sure -0 is implemented */
    f.f = power;
    n = (f.i[0] != 0) ? 0 : 1;
    if (f.i[n] != 0x8000) {
	return 3;
    }
    return 0;
}
], [ AC_MSG_RESULT(yes) ], [ AC_MSG_RESULT(no) ]
)
])


# BC_COMPLIANT_DOUBLE()
AC_DEFUN([BC_COMPLIANT_DOUBLE], [
dnl determine whether the floating point implementation seems to be
dnl IEEE 754 compliant by checking whether the tolerance epsilon 
dnl matches -23 power.  make sure 0 == -0 too.
AC_MSG_CHECKING([whether doubles conform to IEEE 754])
AC_TRY_RUN([
typedef union {
    double d;
    unsigned short int i[4];
} D;
int main (int ac, char *av[]) {
    /* must be volatile to avoid long registers */
    volatile double tol = 1.0;
    volatile double power = 1.0;
    int n = 52;
    D d;
    while (1.0f + (tol * 0.5) != 1.0) {
	tol *= 0.5;
    }
    while (n-- > 0) {
	power *= 0.5;
    }
    if (tol != power) {
	return 1;
    }
    /* make sure 0 == -0 */
    tol = 0.0; power = -tol;
    if (tol != power) {
	return 2;
    }
    /* make sure -0 is implemented */
    d.d = power;
    n = (d.i[0] != 0) ? 0 : (d.i[1] != 0) ? 1 : (d.i[2] != 0) ? 2 : 3;
    if (d.i[n] != 0x8000) {
	return 3;
    }
    return 0;
}
], [ AC_MSG_RESULT(yes) ], [ AC_MSG_RESULT(no) ]
)
])
