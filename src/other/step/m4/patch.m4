#                       P A T C H . M 4
# BRL-CAD
#
# Copyright (c) 2007-2009 United States Government as represented by
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
# BC_PATCH_LIBTOOL
#
# automatically detect and patch faulty libtool scripts
#
###

AC_DEFUN([BC_PATCH_LIBTOOL], [

# libtoolize under Mac OS X originally used an -all_load directive in
# the generated libtool script which is outright wrong under certain
# conditions.  Check for the flag and remove it.

for script in $ac_top_builddir $ac_abs_builddir $ac_builddir . ; do
    if test "x$script" = "x" ; then
	libtoolscript="libtool"
    else
	libtoolscript="${script}/libtool"
    fi
    if test -f ${libtoolscript} ; then
	if test -w ${libtoolscript} ; then

	    case $host_os in
		# remove any -all_load option.
		# provokes libtool linker bug with noinst libraries.
	        darwin*)
		    sed 's/-all_load.*convenience//g' < $libtoolscript > ${libtoolscript}.sed
		    sed "s/temp_rpath=\$/temp_rpath=$TCL_PATH:$TK_PATH/g" < $libtoolscript.sed > ${libtoolscript}.sed2
		    if test ! "x`cat ${libtoolscript}`" = "x`cat ${libtoolscript}.sed2`" ; then
			AC_MSG_RESULT([Found -all_load in libtool script, removing])
			cp ${libtoolscript}.sed2 ${libtoolscript}
		    fi
		    rm -f ${libtoolscript}.sed ${libtoolscript}.sed2
		    ;;

		# make sure the debian devs don't screw with link_all_deplibs
		linux*)
		    sed 's/^link_all_deplibs=no/link_all_deplibs=unknown/g' < $libtoolscript > ${libtoolscript}.sed
		    if test ! "x`cat ${libtoolscript}`" = "x`cat ${libtoolscript}.sed`" ; then
		       	AC_MSG_RESULT([Found link_all_deplibs=no in libtool script, reverting])
			cp ${libtoolscript}.sed ${libtoolscript}
		    fi
		    rm -f ${libtoolscript}.sed
		    ;;
	    esac

	else
	    AC_MSG_WARN([libtool script exists but is not writable so not attempting to edit])
	fi
    fi
done

])

# Local Variables:
# mode: m4
# tab-width: 8
# standard-indent: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
