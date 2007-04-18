#!/bin/sh
#                    E N U M E R A T E . S H
# BRL-CAD
#
# Copyright (c) 2006-2007 United States Government as represented by
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
# Compute various determinable enumerations about BRL-CAD including
# how many lines of code the package uses, how many
# applications/libraries there are, and how many files/directories.
# This separates the counts into various categories to account for the
# external codes and non-source files too.
#
#   ./enumerate.sh
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

# obtain the version of BRL-CAD from include/conf
MAJOR="`cat ${BASE}/include/conf/MAJOR`"
MINOR="`cat ${BASE}/include/conf/MINOR`"
PATCH="`cat ${BASE}/include/conf/PATCH`"
BRLCAD_VERSION="${MAJOR}.${MINOR}.${PATCH}"

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


# print the summary
printf -- "*****************************************\n"
printf    "** BRL-CAD PROJECT ENUMERATION SUMMARY **\n"
printf -- "*****************************************\n"
printf "BRL-CAD Version: $BRLCAD_VERSION\n"
printf "Enumeration Run: `date`\n"
printf "\n"
printf "Included below are various project enumeration statistics for BRL-CAD.\n"
printf "The format/output of this script is subject to change without notice.\n"
printf "Please contact devs@brlcad.org if there are any questions or comments.\n"
printf "\n"
printf "Now processing, please wait...\n\n"


# count number of installed libraries
preinstalled_libs="`find \"$BASE\" -type f -name Makefile.am | grep -v '/other/' | grep -v '/misc/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*LIBRARIES\" | grep -v 'noinst' | sed 's/.*=//g'`"
installed_libs=`for lib in $preinstalled_libs ; do echo $lib ; done | sort | uniq`
installed_libs_count="`echo \"$installed_libs\" | wc -l`"

# count number of non-installed libraries
preuninstalled_libs="`find \"$BASE\" -type f -name Makefile.am | grep -v '/other/' | grep -v '/misc/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*LIBRARIES\" | grep 'noinst' | sed 's/.*=//g'`"
uninstalled_libs=`for lib in $preuninstalled_libs ; do echo $lib ; done | sort | uniq`
uninstalled_libs_count="`echo \"$uninstalled_libs\" | wc -l`"

# summarize number of libraries
libs_count="`echo $installed_libs_count $uninstalled_libs_count + p | dc`"

# count number of installed applications
preinstalled_apps="`find \"$BASE\" -type f -name Makefile.am | grep -v '/other/' | grep -v '/misc/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*PROGRAMS\" | grep '^bin' | sed 's/.*=//g'`"
installed_apps=`for app in $preinstalled_apps ; do echo $app ; done | sort | uniq`
installed_apps_count="`echo \"$installed_apps\" | wc -l`"

# count number of non-installed applications
preuninstalled_apps="`find \"$BASE\" -type f -name Makefile.am | grep -v '/other/' | grep -v '/misc/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*PROGRAMS\" | grep -v '^bin' | sed 's/.*=//g'`"
uninstalled_apps=`for app in $preuninstalled_apps ; do echo $app ; done | sort | uniq`
uninstalled_apps_count="`echo \"$uninstalled_apps\" | wc -l`"

# summarize number of applications
apps_count="`echo $installed_apps_count $uninstalled_apps_count + p | dc`"

# count installed 3rd party libraries
preotherlibs="`find \"$BASE\" -type f -name Makefile.am | grep '/other/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*LIBRARIES\" | grep -v 'noinst' | sed 's/.*=//g'`"
premisclibs="`find \"$BASE\" -type f -name Makefile.am | grep '/misc/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*LIBRARIES\" | grep -v 'noinst' | sed 's/.*=//g'`"
otherlibs=`for app in $preotherlibs $premislibs ; do echo $app ; done | sort | uniq`
otherlibs_count="`echo \"$otherlibs\" | wc -l`"

# count 3rd party installed applications
preotherapps="`find \"$BASE\" -type f -name Makefile.am | grep '/other/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*PROGRAMS\" | grep '^bin' | sed 's/.*=//g'`"
premiscapps="`find \"$BASE\" -type f -name Makefile.am | grep '/misc/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*PROGRAMS\" | grep '^bin' | sed 's/.*=//g'`"
otherapps=`for app in $preotherapps $premisapps ; do echo $app ; done | sort | uniq`
otherapps_count="`echo \"$otherapps\" | wc -l`"

# output summary of compilation products
printf -- "-----------------------------------------\n"
printf -- "--        COMPILATION PRODUCTS         --\n"
printf -- "-----------------------------------------\n"
printf "%7d\t%s\n" "$libs_count" "BRL-CAD Libraries"
printf "\t%7d\t%s\n" "$installed_libs_count" "Installed"
printf "\t%7d\t%s\n" "$uninstalled_libs_count" "Not Installed"
printf "%7d\t%s\n" "$apps_count" "BRL-CAD Applications"
printf "\t%7d\t%s\n" "$installed_apps_count" "Installed"
printf "\t%7d\t%s\n" "$uninstalled_apps_count" "Not Installed"
printf "%7d\t%s\n" "$otherlibs_count" "3rd Party Installed Libraries"
printf "%7d\t%s\n" "$otherapps_count" "3rd Party Installed Applications"
printf "\n"

# count number of files
dist_count="`find \"$BASE\" -type f -name Makefile.am | grep -v '/other/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*DIST\" | sed 's/.*=//g' | wc -l`"
data_count="`find \"$BASE\" -type f -name Makefile.am | grep -v '/other/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*DATA\" | sed 's/.*=//g' | wc -l`"
mans_count="`find \"$BASE\" -type f -name Makefile.am | grep -v '/other/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*MANS\" | sed 's/.*=//g' | wc -l`"
srcs_count="`find \"$BASE\" -type f \( -name \*.c -or -name \*.h -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.pl -or -name \*.f -or -name \*.java \) | grep -v '/other/' | wc -l`"
am_count="`find \"$BASE\" -type f \( -name Makefile.am -or -name configure.ac \) | wc -l`"
file_count="`echo $dist_count $data_count $mans_count $srcs_count $am_count ++++ p | dc`"

# count number of directories
dir_count="`find \"$BASE\" -type d -not \( -regex '.*/CVS.*' -or -regex '.*/\.libs.*' -or -regex '.*/\.deps.*' -or -regex '.*autom4te.cache.*' -or -regex '.*/other.*' \) | wc -l`"

# count number of 3rd party files
otherdist_count="`find \"$BASE\" -type f -name Makefile.am | grep '/other/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*DIST\" | sed 's/.*=//g' | wc -l`"
otherdata_count="`find \"$BASE\" -type f -name Makefile.am | grep '/other/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*DATA\" | sed 's/.*=//g' | wc -l`"
othermans_count="`find \"$BASE\" -type f -name Makefile.am | grep '/other/' | xargs cat | perl -pi -e 's/\\\\\n//g' | grep \"^[a-zA-Z_]*MANS\" | sed 's/.*=//g' | wc -l`"
othersrcs_count="`find \"$BASE\" -type f \( -name \*.c -or -name \*.h -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.pl -or -name \*.f -or -name \*.java \) | grep '/other/' | wc -l`"
otherfile_count="`echo $otherdist_count $otherdata_count $othermans_count $othersrcs_count +++ p | dc`"

# count number of 3rd party directories
otherdir_count="`find \"$BASE\" -type d -regex '.*/other/.*' -not \( -regex '.*/CVS.*' -or -regex '.*/\.libs.*' -or -regex '.*/\.deps.*' -or -regex '.*autom4te.cache.*' \) | wc -l`"

# output summary of filesystem organization
printf -- "-----------------------------------------\n"
printf -- "--       FILESYSTEM ORGANIZATION       --\n"
printf -- "-----------------------------------------\n"
printf "%7d\t%s\n" "$file_count" "BRL-CAD Files"
printf "%7d\t%s\n" "$dir_count" "BRL-CAD Directories"
printf "%7d\t%s\n" "$otherfile_count" "3rd Party Files"
printf "%7d\t%s\n" "$otherdir_count" "3rd Party Directories"
printf "\n"


# compute documentation line counts
dc1="`find \"$BASE\" -type f \( -name \*.[123456789n] \) | grep -v '/other/'`"
dc2="`find \"$BASE\" -type f \( -name README\* -or -name AUTHORS -or -name BUGS -or -name COPYING -or -name ChangeLog -or -name HACKING -or -name INSTALL -or -name NEWS -or -name TODO -or -name \*.txt -or -name \*.tr -or -name \*.htm\* \) -not -name \*~ | grep -v '/other/' | grep -v legal`"
dc="$dc1
$dc2"
dc_lc="`echo \"$dc\" | sort | xargs wc -l`"
dc_lc_lines="`echo \"$dc_lc\" | grep -v 'total$' | awk '{print $1}'`"
dc_lc_total="`sum $dc_lc_lines`"

# compute build infrastructure line counts
bic1="`find \"$BASE\" -type f \( -name \*.am -or -name configure.ac -or -name autogen.sh \)`"
bic2="`find \"$BASE/sh\" -type f \( -name \*.sh \)`"
bic="$bic1
$bic2"
bic_lc="`echo \"$bic\" | sort | xargs wc -l`"
bic_lc_lines="`echo \"$bic_lc\" | grep -v 'total$' | awk '{print $1}'`"
bic_lc_total="`sum $bic_lc_lines`"
bic_lc_blank="`echo \"$bic\" | xargs awk ' /^[  ]*$/ { ++x } END { print x } '`"
bic_lc_total="`expr $bic_lc_total - $bic_lc_blank`"

# compute header code line counts
header="`find \"$BASE\" -type f \( -name \*.h \) | grep -v '/other/' | grep -v '/sh/' | grep -v misc`"
header_lc="`echo \"$header\" | sort | xargs wc -l`"
header_lc_lines="`echo \"$header_lc\" | grep -v 'total$' | awk '{print $1}'`"
header_lc_total="`sum $header_lc_lines`"
header_lc_blank="`echo \"$header\" | xargs awk ' /^[    ]*$/ { ++x } END { print x } '`"
header_lc_total="`expr $header_lc_total - $header_lc_blank`"

# compute non-header library code line counts
sourcelib="`find \"$BASE\" -type f \( -name \*.c -or -name \*.java -or -name \*.f \) | grep -v '/other/' | grep -v '/sh/' | grep -v misc | grep lib`"
sourcelib_lc="`echo \"$sourcelib\" | sort | xargs wc -l`"
sourcelib_lc_lines="`echo \"$sourcelib_lc\" | grep -v 'total$' | awk '{print $1}'`"
sourcelib_lc_total="`sum $sourcelib_lc_lines`"
sourcelib_lc_blank="`echo \"$sourcelib\" | xargs awk ' /^[      ]*$/ { ++x } END { print x } '`"
sourcelib_lc_total="`expr $sourcelib_lc_total - $sourcelib_lc_blank`"

# compute non-header application code line counts
sourcebin="`find \"$BASE\" -type f \( -name \*.c -or -name \*.java -or -name \*.f \) | grep -v '/other/' | grep -v '/sh/' | grep -v misc | grep -v lib`"
sourcebin_lc="`echo \"$sourcebin\" | sort | xargs wc -l`"
sourcebin_lc_lines="`echo \"$sourcebin_lc\" | grep -v 'total$' | awk '{print $1}'`"
sourcebin_lc_total="`sum $sourcebin_lc_lines`"
sourcebin_lc_blank="`echo \"$sourcebin\" | xargs awk ' /^[      ]*$/ { ++x } END { print x } '`"
sourcebin_lc_total="`expr $sourcebin_lc_total - $sourcebin_lc_blank`"

# compute script code line counts
scripts="`find \"$BASE\" -type f \( -name \*.sh -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk \) | grep -v '/other/' | grep -v '/sh/' | grep -v misc`"
scripts_lc="`echo \"$scripts\" | sort | xargs wc -l`"
scripts_lc_lines="`echo \"$scripts_lc\" | grep -v 'total$' | awk '{print $1}'`"
scripts_lc_total="`sum $scripts_lc_lines`"
scripts_lc_blank="`echo \"$scripts\" | xargs awk ' /^[  ]*$/ { ++x } END { print x } '`"
scripts_lc_total="`expr $scripts_lc_total - $scripts_lc_blank`"

# compute 3rd party code line counts
other="`find \"$BASE\" -type f \( -name \*.c -or -name \*.h -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.sh -or -name \*.f -or -name \*.java \) | grep '/other/'`"
other_lc="`echo \"$other\" | sort | xargs wc -l`"
other_lc_lines="`echo \"$other_lc\" | grep -v 'total$' | awk '{print $1}'`"
other_lc_total="`sum $other_lc_lines`"

# compute line count totals
sc_lc_total="`echo \"$sourcelib_lc_total $sourcebin_lc_total $scripts_lc_total + + p\" | dc`"
sch_lc_total="`echo \"$sc_lc_total $header_lc_total + p\" | dc`"
blank_lc_total="`echo \"$bic_lc_blank $header_lc_blank $sourcelib_lc_blank $sourcebin_lc_blank $scripts_lc_blank ++++ p\" | dc`"
total_code="`echo \"$bic_lc_total $sch_lc_total + p\" | dc`"
total_noncode="`echo \"$dc_lc_total p\" | dc`"
total="`echo \"$total_code $total_noncode + p\" | dc`"

# output summary of line count totals
printf -- "-----------------------------------------\n"
printf -- "--          LINE COUNT TOTALS          --\n"
printf -- "-----------------------------------------\n"
printf "%7d\t%s\n" "$total" "BRL-CAD Project Total"
printf "\t%7d\t%s\n" "$dc_lc_total" "Documentation"
printf "\t%7d\t%s\n" "$bic_lc_total" "Build Infrastructure"
printf "\t%7d\t%s\n" "$sch_lc_total" "Source Code"
printf "\t\t%7d\t%s\n" "$header_lc_total" "Header"
printf "\t\t%7d\t%s\n" "$sc_lc_total" "Non-Header"
printf "\t\t\t%7d\t%s\n" "$sourcelib_lc_total" "Library Code"
printf "\t\t\t%7d\t%s\n" "$sourcebin_lc_total" "Application Code"
printf "\t\t\t%7d\t%s\n" "$scripts_lc_total" "Scripts"
printf "%7d\t%s\n" "$blank_lc_total" "Blank Lines (not counted above)"
printf "%7d\t%s\n" "$other_lc_total" "3rd Party Code (not counted above)"
printf -- "=========================================\n"


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
