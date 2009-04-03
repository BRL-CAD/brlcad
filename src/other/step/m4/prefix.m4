#                      P R E F I X . M 4
# SCL
#
# Copyright (c) 2008-2009 United States Government as represented by
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
# BC_SCL_ROOT
#
# set up the installation path
#
#
# BC_SCL_DATA
#
# set up the SCL_DATA install directory
#
###

AC_DEFUN([BC_SCL_ROOT], [

# set up the SCL_ROOT installation path
AC_MSG_CHECKING([where SCL is to be installed])
bc_prefix="$prefix"
eval "bc_prefix=\"$bc_prefix\""
eval "bc_prefix=\"$bc_prefix\""
if test "x$bc_prefix" = "xNONE" ; then
    bc_prefix="$ac_default_prefix"
    # should be /usr/scl, but just in case
    eval "bc_prefix=\"$bc_prefix\""
    eval "bc_prefix=\"$bc_prefix\""
fi
AC_DEFINE_UNQUOTED([SCL_ROOT], "$bc_prefix", "Location SCL will install to")
AC_MSG_RESULT($bc_prefix)
if test ! "x$SCL_ROOT" = "x" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([SCL_ROOT should only be used to override an install directory at runtime])
	AC_MSG_WARN([SCL_ROOT is presently set to "${SCL_ROOT}"])
	AC_MSG_NOTICE([It is highly recommended that SCL_ROOT be unset and not used])
	if test "x$SCL_ROOT" = "x$bc_prefix" ; then
		AC_MSG_WARN([SCL_ROOT is not necessary and may cause unexpected behavior])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	else
		AC_MSG_ERROR([*** Environment variable SCL_ROOT conflicts with --prefix ***])
	fi
	# emphasize that this is unnecessary
	emphasis=2
	AC_MSG_NOTICE([Pausing $emphasis seconds...])
	sleep $emphasis
else
	# compensate for autoconf inconsistencies
	SCL_ROOT="$bc_prefix"
fi
AC_SUBST(SCL_ROOT)

])


AC_DEFUN([BC_SCL_DATA], [

AC_MSG_CHECKING([where SCL resources are to be installed])
bc_data_dir="${datadir}"
if test "x$bc_data_dir" = "xNONE/share" ; then
	bc_data_dir="${bc_prefix}/share/scl/${SCLLIB_VERSION}"
elif test "x$bc_data_dir" = "x\${prefix}/share" ; then
	bc_data_dir="${bc_prefix}/share/scl/${SCLLIB_VERSION}"
fi
eval "bc_data_dir=\"$bc_data_dir\""
eval "bc_data_dir=\"$bc_data_dir\""
if test "x$bc_data_dir" = "xNONE/share" ; then
	bc_data_dir="${bc_prefix}/share/brlcad/${SCLLIB_VERSION}"
fi
AC_DEFINE_UNQUOTED([SCL_DATA], "$bc_data_dir", "Location SCL resources will install to")
AC_MSG_RESULT($bc_data_dir)
if test ! "x$SCL_DATA" = "x" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([SCL_DATA should only be used to override an install directory at runtime])
	AC_MSG_WARN([SCL_DATA is presently set to "${SCL_DATA}"])
	AC_MSG_NOTICE([It is highly recommended that SCL_DATA be unset and not used])
	if test "x$SCL_DATA" = "x$bc_data_dir" ; then
		AC_MSG_WARN([SCL_DATA is not necessary and may cause unexpected behavior])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	else
		AC_MSG_ERROR([*** Environment variable SCL_DATA conflicts with --prefix or --datadir ***])
	fi
	# emphasize that this is unnecessary
	emphasis=2
	AC_MSG_NOTICE([Pausing $emphasis seconds...])
	sleep $emphasis
else
	# compensate for autoconf inconsistencies
	SCL_DATA="$bc_data_dir"
fi
AC_SUBST(SCL_DATA)

])

# Local Variables:
# mode: m4
# tab-width: 8
# standard-indent: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
