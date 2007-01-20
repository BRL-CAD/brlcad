#                       S T A G E . M 4
# BRL-CAD
#
# Copyright (c) 2005-2007 United States Government as represented by
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
# BC_BOLD
# BC_UNBOLD
#
# sets and unsets the output to bold for emphasis
#
#
# BC_CONFIGURE_STAGE
#
# outputs status on the the specified configure stage being started.
#
###

AC_DEFUN([BC_BOLD], [
if test -t 1 ; then
	if test -f "${srcdir}/sh/shtool" ; then
		${srcdir}/sh/shtool echo -n -e %B
	elif test -f "${srcdir}/misc/shtool" ; then
		${srcdir}/misc/shtool echo -n -e %B
	fi
fi
])


AC_DEFUN([BC_UNBOLD], [
if test -t 1 ; then
	if test -f "${srcdir}/sh/shtool" ; then
		${srcdir}/sh/shtool echo -n -e %b
	elif test -f "${srcdir}/misc/shtool" ; then
		${srcdir}/misc/shtool echo -n -e %b
	fi
fi
])


AC_DEFUN([BC_CONFIGURE_STAGE], [

_bc_stage="[$1]"
_bc_status="[$2]"
_bc_stage="`echo $_bc_stage | sed 's/\(.\)/\1 /g'`"

BC_BOLD

AC_MSG_CHECKING([... $_bc_stage])
AC_MSG_RESULT([($_bc_status)])

BC_UNBOLD
])

# Local Variables:
# mode: m4
# tab-width: 8
# standard-indent: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
