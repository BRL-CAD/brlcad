#                       R E T R Y . M 4
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
# BC_RETRY_CONFIGURE
#
# restarts configure with the specified arguments.  the first argument
# is configure, the second are arguments to the reinvoked configure
# (flags, for example).  a retry will use any directories specified by
# the BC_RETRY_DIRECTORY macro.
#
#
# BC_RETRY_DIRECTORY
#
# designates a directory that should get searched if configure starts
# over.  optional second argument is the directory group name
#
###

dnl XXX - should only add to build paths when something fails that is
dnl necessary additionally, a compiler might complain if /usr/local or
dnl /sw are already in the system search path -- so need to try a
dnl compile test..

###
AC_DEFUN([BC_RETRY_DIRECTORY], [
retry_dir="$1"
retry_label="$2"
bc_dir_msg=""
if test "x$retry_label" = "x" ; then
	bc_dir_msg="for"
else
	bc_dir_msg="for $retry_label in"
fi

AC_MSG_CHECKING([$bc_dir_msg $retry_dir])
if test -d "$retry_dir" ; then
	bc_found=0
	if test -d "${retry_dir}/bin" ; then
		if test "x$bc_retry_path" = "x" ; then
			bc_retry_path="${retry_dir}/bin"
		else
			bc_retry_path="${bc_retry_path}:${retry_dir}/bin"
		fi
		bc_found=1
	fi
	if test -d "${retry_dir}/include" ; then
		if test "x$bc_retry_cppflags" = "x" ; then
			bc_retry_cppflags="-I${retry_dir}/include"
		else
			bc_retry_cppflags="${bc_retry_cppflags} -I${retry_dir}/include"
		fi
		bc_found=1
	fi
	if test -d "${retry_dir}/lib" ; then
		if test "x$bc_retry_ldflags" = "x" ; then
			bc_retry_ldflags="-L${retry_dir}/lib"
		else
			bc_retry_ldflags="${bc_retry_ldflags} -L${retry_dir}/lib"
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


###
AC_DEFUN([BC_RETRY_CONFIGURE], [
bc_configure="$1"
bc_configure_args="$2"
if test "x$BC_RETRY" = "x" && test "x$bc_configure_retry" = "xyes"; then
	if test "x$bc_configure" = "x" ; then
		bc_configure="${srcdir}/configure"
	fi
	if test "x$PATH" = "x" ; then
		PATH="$bc_retry_path"
	else
		PATH="${PATH}:${bc_retry_path}"
	fi
	if test "x$CPPFLAGS" = "x" ; then
		CPPFLAGS="$bc_retry_cppflags"
	else
		CPPFLAGS="$CPPFLAGS $bc_retry_cppflags"
	fi
	if test "x$LDFLAGS" = "x" ; then
		LDFLAGS="$bc_retry_ldflags"
	else
		LDFLAGS="$LDFLAGS $bc_retry_ldflags"
	fi
	if test -f "$bc_configure" ; then
		BC_RETRY=no
		AC_MSG_WARN([Restarting configure with additional flags])
		AC_MSG_NOTICE([Restarting with CFLAGS=$CFLAGS])
		AC_MSG_NOTICE([Restarting with CPPFLAGS=$CPPFLAGS])
		AC_MSG_NOTICE([Restarting with LDFLAGS=$LDFLAGS])
		AC_MSG_NOTICE([Restarting with PATH=$PATH])
		export CFLAGS CPPFLAGS LDFLAGS PATH
		AC_MSG_NOTICE([Running: $bc_configure $bc_configure_args])
		exec $bc_configure BC_RETRY=no $bc_configure_args
		exit $?
	else
		AC_MSG_ERROR([*** Unable to find configure ***])
	fi
fi
])

# Local Variables:
# mode: m4
# tab-width: 8
# standard-indent: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
