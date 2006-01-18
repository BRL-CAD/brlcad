#                     R E S T O R E . M 4
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
# BC_RESTORE_CLOBBERED
#
# restores a file that has been backed up to a directory if the contents
# have changed.  if the files have Revision numbers, those are used to
# determine which file is newer.
#
# an automake bug in older versions will clobber existing COPYING and
# INSTALL files, so those are common files that need to be restored.
#
###

AC_DEFUN([BC_RESTORE_CLOBBERED], [

files_to_check="ifelse([$1], , [], [$1])"
backup_dir="ifelse([$2], , [.], [$2])"

for file in $files_to_check ; do
	curr="$srcdir/$file"
	back="$srcdir/${backup_dir}/${file}.backup"
	if ! test -f "$back" ; then
		continue
	fi

	AC_MSG_CHECKING([whether ${file} needs to be restored])
	current=""
	backup=""
	if test -f "$curr" ; then
		if test -f "$back" ; then
			current="`cat $curr`"
			backup="`cat $back`"
			if test "x$current" = "x$backup" ; then
				# contents match
				AC_MSG_RESULT([no])
			else
				current_rev=`grep '$Revision' "$curr" | awk '{print $[2]}' | sed 's/\.//g'`
				if test "x$current_rev" = "x" ; then
					current_rev=0
				fi
				backup_rev=`grep '$Revision' "$back" | awk '{print $[2]}' | sed 's/\.//g'`
				if test "x$backup_rev" = "x" ; then
					backup_rev=0
				fi

				if test "$current_rev" -gt "$backup_rev" ; then
					AC_MSG_RESULT([no, saving backup])
					# new version in/from cvs.. back it up
					cp -pr "$curr" "$back"
				else
					AC_MSG_RESULT([yes])
					cp -pr "$back" "$curr"
				fi
			fi
		else
			AC_MSG_RESULT([no, backup missing])
		fi
	else
		# missing current
		if test -f "$back" ; then
			AC_MSG_RESULT([yes, missing])
			cp -pr "$back" "$curr"
		else
			AC_MSG_RESULT([yes, but no backup])
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
