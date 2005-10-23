#                      S E A R C H . M 4
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
# BC_SEARCH_DIRECTORY
#
# designates a directory that should get searched for libraries,
# headers, and binaries.  optional second argument is the directory
# group name.
#
###

###
AC_DEFUN([BC_SEARCH_DIRECTORY], [
search_dir="$1"
search_label="$2"
bc_dir_msg=""
if test "x$search_label" = "x" ; then
	bc_dir_msg="for"
else
	bc_dir_msg="for $search_label in"
fi

AC_MSG_CHECKING([$bc_dir_msg $search_dir])
if test -d "$search_dir" ; then
	bc_found=0
	if test -d "${search_dir}/bin" ; then
		if test "x$PATH" = "x" ; then
			PATH="${search_dir}/bin"
		else
			PATH="${PATH}:${search_dir}/bin"
		fi
		bc_found=1
	fi
	if test -d "${search_dir}/include" ; then
		if test "x$CPPFLAGS" = "x" ; then
			CPPFLAGS="-I${search_dir}/include"
		else
			CPPFLAGS="${CPPFLAGS} -I${search_dir}/include"
		fi
		bc_found=1
	fi
	if test -d "${search_dir}/lib64" ; then
		if test "x$LDFLAGS" = "x" ; then
			LDFLAGS="-L${search_dir}/lib64"
		else
			LDFLAGS="${LDFLAGS} -L${search_dir}/lib64"
		fi
		bc_found=1
	fi
	if test -d "${search_dir}/lib" ; then
		if test "x$LDFLAGS" = "x" ; then
			LDFLAGS="-L${search_dir}/lib"
		else
			LDFLAGS="${LDFLAGS} -L${search_dir}/lib"
		fi
		bc_found=1
	fi
	if test "x$bc_found" = "x1" ; then
		AC_MSG_RESULT([found])
	else
		AC_MSG_RESULT([found, but empty])
	fi
else
	AC_MSG_RESULT([not found])
fi
])

# Local Variables:
# mode: m4
# tab-width: 8
# standard-indent: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
