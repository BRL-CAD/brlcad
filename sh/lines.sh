#!/bin/sh
#                        L I N E S . S H
# BRL-CAD
#
# Copyright (c) 2006 United States Government as represented by
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
# Compute how many lines of code the package uses.  This separates the
# counts into various categories to account for the external codes and
# non-source files too.
#
#   ./lines.sh
#
# Author -
#   Christopher Sean Morrison
#
# Source -
#   BRL-CAD Open Source
###

SELF="$0"
ARGS="$*"
BASE="`dirname $0`/.."

# force locale setting to C so things like date output as expected
LC_ALL=C

# convenience for computing a sum of a list of integers without
# relying on wc or awk to behave consistently
sum() {
    if [ "x$2" = "x" ]; then
	echo $1
    else
	_total=0
	for _num in $* ; do
	    _total="`expr $_total \+ $_num`"
	done
   fi
    echo $_total
}


# compute documentation counts
dc1="`find \"$BASE\" -type f \( -name \*.[123456789n] \) | grep -v '/other/'`"
dc2="`find \"$BASE\" -type f \( -name README\* -or -name AUTHORS -or -name BUGS -or -name COPYING -or -name ChangeLog -or -name HACKING -or -name INSTALL -or -name NEWS -or -name TODO -or -name \*.txt -or -name \*.tr -or -name \*.htm\* \) -not -name \*~ | grep -v '/other/' | grep -v legal`"
dc="$dc1
$dc2"
dc_lc="`echo \"$dc\" | sort | xargs wc -l`"
dc_lc_lines="`echo \"$dc_lc\" | grep -v 'total$' | awk '{print $1}'`"
dc_lc_total="`sum $dc_lc_lines`"

# compute build infrastructure counts
bic1="`find \"$BASE\" -type f \( -name \*.am -or -name configure.ac -or -name autogen.sh \)`"
bic2="`find \"$BASE/sh\" -type f \( -name \*.sh \)`"
bic="$bic1
$bic2"
bic_lc="`echo \"$bic\" | sort | xargs wc -l`"
bic_lc_lines="`echo \"$bic_lc\" | grep -v 'total$' | awk '{print $1}'`"
bic_lc_total="`sum $bic_lc_lines`"
bic_lc_blank="`echo \"$bic\" | xargs awk ' /^[ 	]*$/ { ++x } END { print x } '`"
bic_lc_total="`expr $bic_lc_total - $bic_lc_blank`"

# compute header code counts
header="`find \"$BASE\" -type f \( -name \*.h \) | grep -v '/other/' | grep -v '/sh/' | grep -v misc`"
header_lc="`echo \"$header\" | sort | xargs wc -l`"
header_lc_lines="`echo \"$header_lc\" | grep -v 'total$' | awk '{print $1}'`"
header_lc_total="`sum $header_lc_lines`"
header_lc_blank="`echo \"$header\" | xargs awk ' /^[ 	]*$/ { ++x } END { print x } '`"
header_lc_total="`expr $header_lc_total - $header_lc_blank`"

# compute non-header library code counts
sourcelib="`find \"$BASE\" -type f \( -name \*.c \) | grep -v '/other/' | grep -v '/sh/' | grep -v misc | grep lib`"
sourcelib_lc="`echo \"$sourcelib\" | sort | xargs wc -l`"
sourcelib_lc_lines="`echo \"$sourcelib_lc\" | grep -v 'total$' | awk '{print $1}'`"
sourcelib_lc_total="`sum $sourcelib_lc_lines`"
sourcelib_lc_blank="`echo \"$sourcelib\" | xargs awk ' /^[ 	]*$/ { ++x } END { print x } '`"
sourcelib_lc_total="`expr $sourcelib_lc_total - $sourcelib_lc_blank`"

# compute non-header application code counts
sourcebin="`find \"$BASE\" -type f \( -name \*.c \) | grep -v '/other/' | grep -v '/sh/' | grep -v misc | grep -v lib`"
sourcebin_lc="`echo \"$sourcebin\" | sort | xargs wc -l`"
sourcebin_lc_lines="`echo \"$sourcebin_lc\" | grep -v 'total$' | awk '{print $1}'`"
sourcebin_lc_total="`sum $sourcebin_lc_lines`"
sourcebin_lc_blank="`echo \"$sourcebin\" | xargs awk ' /^[ 	]*$/ { ++x } END { print x } '`"
sourcebin_lc_total="`expr $sourcebin_lc_total - $sourcebin_lc_blank`"

# compute script code counts
scripts="`find \"$BASE\" -type f \( -name \*.sh -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk \) | grep -v '/other/' | grep -v '/sh/' | grep -v misc`"
scripts_lc="`echo \"$scripts\" | sort | xargs wc -l`"
scripts_lc_lines="`echo \"$scripts_lc\" | grep -v 'total$' | awk '{print $1}'`"
scripts_lc_total="`sum $scripts_lc_lines`"
scripts_lc_blank="`echo \"$scripts\" | xargs awk ' /^[ 	]*$/ { ++x } END { print x } '`"
scripts_lc_total="`expr $scripts_lc_total - $scripts_lc_blank`"

# compute 3rd party code counts
other="`find \"$BASE\" -type f \( -name \*.c -or -name \*.h -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.sh \) | grep '/other/'`"
other_lc="`echo \"$other\" | sort | xargs wc -l`"
other_lc_lines="`echo \"$other_lc\" | grep -v 'total$' | awk '{print $1}'`"
other_lc_total="`sum $other_lc_lines`"

# compute totals
sc_lc_total="`echo \"$sourcelib_lc_total $sourcebin_lc_total $scripts_lc_total + + p\" | dc`"
sch_lc_total="`echo \"$sc_lc_total $header_lc_total + p\" | dc`"
blank_lc_total="`echo \"$bic_lc_blank $header_lc_blank $sourcelib_lc_blank $sourcebin_lc_blank $scripts_lc_blank ++++ p\" | dc`"
total_code="`echo \"$bic_lc_total $sch_lc_total + p\" | dc`"
total_noncode="`echo \"$dc_lc_total p\" | dc`"
total="`echo \"$total_code $total_noncode + p\" | dc`"

# print the summary
printf -- "********************************\n"
printf "** BRL-CAD LINE COUNT SUMMARY **\n"
printf -- "********************************\n"
printf "%7d\t%s\n" "$other_lc_total" "3rd Party Code (not counted)"
printf "%7d\t%s\n" "$blank_lc_total" "Blank Lines of Source Code (not counted)"
printf -- "--------------------------------\n"
printf "%7d\t%s\n" "$dc_lc_total" "Documentation"
printf "%7d\t%s\n" "$bic_lc_total" "Build Infrastructure"
printf "%7d\t%s\n" "$sch_lc_total" "Source Code"
printf "\t%7d\t%s\n" "$header_lc_total" "Header"
printf "\t%7d\t%s\n" "$sc_lc_total" "Non-Header"
printf "\t\t%7d\t%s\n" "$sourcelib_lc_total" "Library Code"
printf "\t\t%7d\t%s\n" "$sourcebin_lc_total" "Application Code"
printf "\t\t%7d\t%s\n" "$scripts_lc_total" "Scripts"
printf -- "================================\n"
printf "%7d\t%s\n" "$total" "TOTAL BRL-CAD"


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
