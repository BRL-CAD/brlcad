#                      P R E F I X . M 4
# BRL-CAD
#
# Copyright (c) 2008-2011 United States Government as represented by
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
# BC_BRLCAD_ROOT
#
# set up the installation path
#
#
# BC_BRLCAD_DATA
#
# set up the BRLCAD_DATA install directory
#
###

AC_DEFUN([BC_BRLCAD_ROOT], [

# set up the BRLCAD_ROOT installation path
AC_MSG_CHECKING([where BRL-CAD is to be installed])
bc_prefix="$prefix"
eval "bc_prefix=\"$bc_prefix\""
eval "bc_prefix=\"$bc_prefix\""
if test "x$bc_prefix" = "xNONE" ; then
    bc_prefix="$ac_default_prefix"
    # should be /usr/brlcad, but just in case
    eval "bc_prefix=\"$bc_prefix\""
    eval "bc_prefix=\"$bc_prefix\""
fi
AC_DEFINE_UNQUOTED([BRLCAD_ROOT], "$bc_prefix", "Location BRL-CAD will install to")
AC_MSG_RESULT($bc_prefix)
if test ! "x$BRLCAD_ROOT" = "x" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([BRLCAD_ROOT should only be used to override an install directory at runtime])
	AC_MSG_WARN([BRLCAD_ROOT is presently set to "${BRLCAD_ROOT}"])
	AC_MSG_NOTICE([It is highly recommended that BRLCAD_ROOT be unset and not used])
	if test "x$BRLCAD_ROOT" = "x$bc_prefix" ; then
		AC_MSG_WARN([BRLCAD_ROOT is not necessary and may cause unexpected behavior])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	else
		AC_MSG_ERROR([*** Environment variable BRLCAD_ROOT conflicts with --prefix ***])
	fi
	# emphasize that this is unnecessary
	emphasis=2
	AC_MSG_NOTICE([Pausing $emphasis seconds...])
	sleep $emphasis
else
	# compensate for autoconf inconsistencies
	BRLCAD_ROOT="$bc_prefix"
fi
AC_SUBST(BRLCAD_ROOT)

# make sure the user doesn't try to set /usr as the prefix
if test "x$BRLCAD_ROOT" = "x/usr" ; then
    if test "x$BRLCAD_ROOT_OVERRIDE" = "x" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([It is STRONGLY recommended that you DO NOT install BRL-CAD into /usr])
	AC_MSG_WARN([as BRL-CAD provides several libraries that may conflict with other])
	AC_MSG_WARN([libraries (e.g. librt, libbu, libbn) on certain system configurations.])
	AC_MSG_WARN([])
	AC_MSG_WARN([Since our libraries predate all those that we're known to conflict with])
	AC_MSG_WARN([and are at the very core of our geometry services and project heritage,])
	AC_MSG_WARN([we have no plans to change the names of our libraries at this time.])
	AC_MSG_WARN([])
	AC_MSG_WARN([INSTALLING INTO /usr CAN MAKE A SYSTEM COMPLETELY UNUSABLE.  If you])
	AC_MSG_WARN([choose to continue installing into /usr, you do so entirely at your])
	AC_MSG_WARN([own risk.  You have been warned.])
	AC_MSG_NOTICE([])
	AC_MSG_NOTICE([Consider using a different --prefix or separate --libdir value.])
	# emphasize just how bad of an idea this is
	emphasis=15
	AC_MSG_NOTICE([Pausing $emphasis seconds...])
	sleep $emphasis
	tries=0
	while true ; do
	    AC_MSG_NOTICE([])
	    AC_MSG_NOTICE([Would you like to continue with /usr as the install prefix? [[yes/no]]])
	    read bc_answer
	    if test "x$?" = "x0" ; then
		case "x$bc_answer" in
		    x*[[yY]][[eE]][[sS]]*)
			bc_answer=yes
			break ;;
		    x*[[nN]][[oO]]*)
			bc_answer=no
			break ;;
		    x*)
			AC_MSG_NOTICE([Please answer 'yes' or 'no'])
			;;
	        esac
	    else
		AC_MSG_NOTICE([Unable to read response, continuing])
		break
	    fi
	    tries="`expr $tries \+ 1`"
	    if test "x$tries" = "x10" ; then
		AC_MSG_NOTICE([Unable to read response, continuing])
		break
	    fi
	done
	if test "x$bc_answer" = "xno" ; then
	    AC_MSG_ERROR([*** Aborting due to --prefix=/usr ***])
	else
	    AC_MSG_NOTICE([Package maintainers can quell the root warning and confirmation prompt])
	    AC_MSG_NOTICE([by setting the BRLCAD_ROOT_OVERRIDE environment variable.])
	fi
    fi
fi

])


AC_DEFUN([BC_BRLCAD_DATA], [

AC_MSG_CHECKING([where BRL-CAD resources are to be installed])
bc_data_dir="${datadir}"
if test "x$bc_data_dir" = "xNONE/share" ; then
	bc_data_dir="${bc_prefix}/share/brlcad/${BRLCAD_VERSION}"
elif test "x$bc_data_dir" = "x\${prefix}/share" ; then
	bc_data_dir="${bc_prefix}/share/brlcad/${BRLCAD_VERSION}"
fi
eval "bc_data_dir=\"$bc_data_dir\""
eval "bc_data_dir=\"$bc_data_dir\""
if test "x$bc_data_dir" = "xNONE/share" ; then
	bc_data_dir="${bc_prefix}/share/brlcad/${BRLCAD_VERSION}"
fi
AC_DEFINE_UNQUOTED([BRLCAD_DATA], "$bc_data_dir", "Location BRL-CAD resources will install to")
AC_MSG_RESULT($bc_data_dir)
if test ! "x$BRLCAD_DATA" = "x" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([BRLCAD_DATA should only be used to override an install directory at runtime])
	AC_MSG_WARN([BRLCAD_DATA is presently set to "${BRLCAD_DATA}"])
	AC_MSG_NOTICE([It is highly recommended that BRLCAD_DATA be unset and not used])
	if test "x$BRLCAD_DATA" = "x$bc_data_dir" ; then
		AC_MSG_WARN([BRLCAD_DATA is not necessary and may cause unexpected behavior])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	else
		AC_MSG_ERROR([*** Environment variable BRLCAD_DATA conflicts with --prefix or --datadir ***])
	fi
	# emphasize that this is unnecessary
	emphasis=2
	AC_MSG_NOTICE([Pausing $emphasis seconds...])
	sleep $emphasis
else
	# compensate for autoconf inconsistencies
	BRLCAD_DATA="$bc_data_dir"
fi
AC_SUBST(BRLCAD_DATA)

])

# Local Variables:
# mode: m4
# tab-width: 8
# standard-indent: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
