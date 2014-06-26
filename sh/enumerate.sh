#!/bin/sh
#                    E N U M E R A T E . S H
# BRL-CAD
#
# Copyright (c) 2006-2014 United States Government as represented by
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
###

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
sum ( ) {
    if test "x$2" = "x" ; then
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
echo "*****************************************"
echo "** BRL-CAD PROJECT ENUMERATION SUMMARY **"
echo "*****************************************"
echo "BRL-CAD Version: $BRLCAD_VERSION"
echo "Enumeration Run: `date`"
echo ""
echo "Included below are various project enumeration statistics for BRL-CAD."
echo "The format/output of this script is subject to change without notice."
echo "Please contact devs@brlcad.org if there are any questions or comments."
echo ""
echo "Now processing, please wait..."
echo ""


if false ; then #!!!

# count number of libraries
libs="`find \"$BASE\" -type f -name CMakeLists.txt -not -regex '.*/other/.*' -not -regex '.*/misc/.*' | xargs grep BRLCAD_ADDLIB\\( | sed 's/#.*//g' | grep -v -e ':$' | cut -d\(  -f2 | awk '{print $1}' | sort | uniq`"
libs_count="`echo \"$libs\" | wc -l | awk '{print $1}'`"

echo "-----------------------------------------"
echo "--        COMPILATION PRODUCTS         --"
echo "-----------------------------------------"
printf "%7d\t%s\n" "$libs_count" "BRL-CAD Libraries"

# count number of installed applications
installed_apps="`find \"$BASE\" -type f -name CMakeLists.txt -not -regex '.*/other/.*' -not -regex '.*/misc/.*' | xargs grep BRLCAD_ADDEXEC\\( | grep -v NO_INSTALL | sed 's/\#.*//g' | grep -v -e ':$' | cut -d\(  -f2 | awk '{print $1}' | sort | uniq`"
installed_apps_count="`echo \"$installed_apps\" | wc -l | awk '{print $1}'`"

# count number of non-installed applications
uninstalled_apps="`find \"$BASE\" -type f -name CMakeLists.txt -not -regex '.*/other/.*' -not -regex '.*/misc/.*' | xargs grep BRLCAD_ADDEXEC\\( | grep NO_INSTALL | sed 's/\#.*//g' | grep -v -e ':$' | cut -d\(  -f2 | awk '{print $1}' | sort | uniq`"
uninstalled_apps_count="`echo \"$uninstalled_apps\" | wc -l | awk '{print $1}'`"

# summarize number of applications
apps_count="`echo $installed_apps_count $uninstalled_apps_count + p | dc`"

printf "%7d\t%s\n" "$apps_count" "BRL-CAD Applications"
printf "\t%7d\t%s\n" "$installed_apps_count" "Installed"
printf "\t%7d\t%s\n" "$uninstalled_apps_count" "Not Installed"
printf "\n"

echo "-----------------------------------------"
echo "--       FILESYSTEM ORGANIZATION       --"
echo "-----------------------------------------"

# count number of directories
dir_count="`find \"$BASE\" -type d -not -regex '.*/other/.*' -not -regex '.*/\.[^\.].*' -not -regex '.*~' | wc -l`"

printf "%7d\t%s\n" "$dir_count" "BRL-CAD Directories"

# count number of files
data_count="`find \"$BASE\" -type f -not \( -name \*.c -or -name \*.h -or -name \*.cxx -or -name \*.cpp -or -name \*.hxx -or -name \*.hpp -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.pl -or -name \*.f -or -name \*.java -or -name \*.php -or -name \*.inc -or -name \*.y -or -name \*.l -or -name \*.yy \) -not \( -regex '.*/other/.*' -or -regex '.*/\.[^\.].*' -or -regex '.*~' \) | wc -l`"
srcs_count="`find \"$BASE\" -type f -and \( -name \*.c -or -name \*.h -or -name \*.cxx -or -name \*.cpp -or -name \*.hxx -or -name \*.hpp -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.pl -or -name \*.f -or -name \*.java -or -name \*.php -or -name \*.inc -or -name \*.y -or -name \*.l -or -name \*.yy \) -not \( -regex '.*/other/.*' -or -regex '.*/\.[^\.].*' -or -regex '.*~' \) | wc -l`"
file_count="`echo $data_count $srcs_count + p | dc`"

printf "%7d\t%s\n" "$file_count" "BRL-CAD Files"
printf "\t%7d\t%s\n" "$data_count" "BRL-CAD Data Files"
printf "\t%7d\t%s\n" "$srcs_count" "BRL-CAD Source Files"

# count number of 3rd party directories
otherdir_count="`find \"$BASE\" -type d -regex '.*/other/.*' -not -regex '.*/\.[^\.].*' -not -regex '.*~' | wc -l`"

printf "%7d\t%s\n" "$otherdir_count" "3rd Party Directories"

# count number of 3rd party files
otherdata_count="`find \"$BASE\" -type f -regex '.*/other/.*' -not \( -name \*.c -or -name \*.h -or -name \*.cxx -or -name \*.cpp -or -name \*.hxx -or -name \*.hpp -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.pl -or -name \*.f -or -name \*.php -or -name \*.inc -or -name \*.java -or -name \*.y -or -name \*.l -or -name \*.yy \) -not \( -regex '.*/\.[^\.].*' -or -regex '.*~' \) | wc -l`"
othersrcs_count="`find \"$BASE\" -type f -regex '.*/other/.*' -and \( -name \*.c -or -name \*.h -or -name \*.cxx -or -name \*.cpp -or -name \*.hxx -or -name \*.hpp -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.pl -or -name \*.f -or -name \*.php -or -name \*.inc -or -name \*.java -or -name \*.y -or -name \*.l -or -name \*.yy \) -not \( -regex '.*/\.[^\.].*' -or -regex '.*~' \) | wc -l`"
otherfile_count="`echo $otherdata_count $othersrcs_count + p | dc`"

printf "%7d\t%s\n" "$otherfile_count" "3rd Party Files"
printf "\t%7d\t%s\n" "$otherdata_count" "3rd Party Data Files"
printf "\t%7d\t%s\n" "$othersrcs_count" "3rd Party Source Files"

printf "\n"

fi #!!!

echo "-----------------------------------------"
echo "--          LINE COUNT TOTALS          --"
echo "-----------------------------------------"

# compute documentation line counts
dc="`find \"$BASE\" -type f \( -regex '.*/[A-Z]*$' -or -regex '.*/README.*' -or -regex '.*/TODO.*' -or -regex '.*/INSTALL.*' -or -name ChangeLog -or -name \*.txt -or -name \*.tr -or -name \*.htm\* -or -name \*.xml -or -name \*.bib -or -name \*.tbl -or -name \*.mm -or -name \*csv \) -not \( -regex '.*docbook/resources/standard.*' -or -regex '.*~' -or -regex '.*/\.[^\.].*' -or -regex '.*/other/.*' -or -regex '.*/misc/.*' -or -regex '.*/legal/.*' -or -regex '.*CMakeLists.txt.*' -or -regex '.*CMake.*' -or -regex '.*LICENSE.*\..*' \) `"
dc_lc="`echo \"$dc\" | sort | xargs wc -l`"
dc_lc_lines="`echo \"$dc_lc\" | grep -v 'total$' | awk '{print $1}'`"
dc_lc_total="`sum $dc_lc_lines`"

printf "\t%7d\t%s\n" "$dc_lc_total" "Documentation"

# compute build infrastructure line counts
bic="`find \"$BASE\" -type f \( -name configure -or -name CMakeLists.txt -or -regex '.*\.cmake$' -or -regex '.*\.cmake.in$' -or -regex '.*/CMake.*\.in$' -or -regex '.*/CMake.*\.sh$' -or -regex '.*/sh/.*\.sh$' \) -not \( -regex '.*~' -or -regex '.*/\.[^\.].*' -or -regex '.*/other/.*' \) `"
bic_lc="`echo \"$bic\" | sort | xargs wc -l`"
bic_lc_lines="`echo \"$bic_lc\" | grep -v 'total$' | awk '{print $1}'`"
bic_lc_total="`sum $bic_lc_lines`"
bic_lc_blank="`echo \"$bic\" | xargs awk ' /^[  ]*$/ { ++x } END { print x } '`"
bic_lc_total="`expr $bic_lc_total - $bic_lc_blank`"

printf "\t%7d\t%s\n" "$bic_lc_total" "Build Infrastructure"

# compute header code line counts (intentionally ignoring misc and not /misc/ due to src/external header)
header="`find \"$BASE\" -type f \( -name \*.h -or -name \*.hxx -or -name \*.hpp \) -not \( -regex '.*/other/.*' -or -regex '.*/sh/.*' -or -regex '.*misc.*' \)`"
header_lc="`echo \"$header\" | sort | xargs wc -l`"
header_lc_lines="`echo \"$header_lc\" | grep -v 'total$' | awk '{print $1}'`"
header_lc_total="`sum $header_lc_lines`"
header_lc_blank="`echo \"$header\" | xargs awk ' /^[    ]*$/ { ++x } END { print x } '`"
header_lc_total="`expr $header_lc_total - $header_lc_blank`"

printf "\t\t%7d\t%s\n" "$header_lc_total" "Header"


# compute non-header library code line counts
sourcelib="`find \"$BASE\" -type f -regex '.*lib.*' \( -name \*.c -or -name \*.cxx -or -name \*.cpp -or -name \*.java -or -name \*.f \) -not \( -regex '.*/other/.*' -or -regex '.*/sh/.*' -or -regex '.*misc.*' \)`"
sourcelib_lc="`echo \"$sourcelib\" | sort | xargs wc -l`"
sourcelib_lc_lines="`echo \"$sourcelib_lc\" | grep -v 'total$' | awk '{print $1}'`"
sourcelib_lc_total="`sum $sourcelib_lc_lines`"
sourcelib_lc_blank="`echo \"$sourcelib\" | xargs awk ' /^[      ]*$/ { ++x } END { print x } '`"
sourcelib_lc_total="`expr $sourcelib_lc_total - $sourcelib_lc_blank`"

printf "\t\t\t%7d\t%s\n" "$sourcelib_lc_total" "Library Code"

# compute non-header application code line counts
sourcebin="`find \"$BASE\" -type f \( -name \*.c -or -name \*.cxx -or -name \*.cpp -or -name \*.java -or -name \*.f \) -not \( -regex '.*/other/.*' -or -regex '.*/sh/.*' -or -regex '.*misc.*' -or -regex '.*lib.*' \)`"
sourcebin_lc="`echo \"$sourcebin\" | sort | xargs wc -l`"
sourcebin_lc_lines="`echo \"$sourcebin_lc\" | grep -v 'total$' | awk '{print $1}'`"
sourcebin_lc_total="`sum $sourcebin_lc_lines`"
sourcebin_lc_blank="`echo \"$sourcebin\" | xargs awk ' /^[      ]*$/ { ++x } END { print x } '`"
sourcebin_lc_total="`expr $sourcebin_lc_total - $sourcebin_lc_blank`"

printf "\t\t\t%7d\t%s\n" "$sourcebin_lc_total" "Application Code"

# compute script code line counts
scripts="`find \"$BASE\" -type f \( -name \*.sh -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk \) -not \( -regex '.*/other/.*' -or -regex '.*/sh/.*' -or -regex '.*misc.*' \)`"
scripts_lc="`echo \"$scripts\" | sort | xargs wc -l`"
scripts_lc_lines="`echo \"$scripts_lc\" | grep -v 'total$' | awk '{print $1}'`"
scripts_lc_total="`sum $scripts_lc_lines`"
scripts_lc_blank="`echo \"$scripts\" | xargs awk ' /^[  ]*$/ { ++x } END { print x } '`"
scripts_lc_total="`expr $scripts_lc_total - $scripts_lc_blank`"

printf "\t\t\t%7d\t%s\n" "$scripts_lc_total" "Scripts"

# compute 3rd party code line counts
other="`find \"$BASE\" -type f \( -name \*.c -or -name \*.cxx -or -name \*.cpp -or -name \*.h -or -name \*.hxx -or -name \*.hpp -or -name \*.tcl -or -name \*.tk -or -name \*.itcl -or -name \*.itk -or -name \*.sh -or -name \*.f -or -name \*.java \) -not -regex '.*/other/.*'`"
other_lc="`echo \"$other\" | sort | xargs wc -l`"
other_lc_lines="`echo \"$other_lc\" | grep -v 'total$' | awk '{print $1}'`"
other_lc_total="`sum $other_lc_lines`"

printf "%7d\t%s\n" "$other_lc_total" "3rd Party Code (not counted above)"

# compute line count totals
sc_lc_total="`echo \"$sourcelib_lc_total $sourcebin_lc_total $scripts_lc_total + + p\" | dc`"

printf "\t\t%7d\t%s\n" "$sc_lc_total" "Non-Header"

sch_lc_total="`echo \"$sc_lc_total $header_lc_total + p\" | dc`"

printf "\t%7d\t%s\n" "$sch_lc_total" "Source Code"

blank_lc_total="`echo \"$bic_lc_blank $header_lc_blank $sourcelib_lc_blank $sourcebin_lc_blank $scripts_lc_blank ++++ p\" | dc`"

printf "%7d\t%s\n" "$blank_lc_total" "Blank Lines (not counted above)"

total_code="`echo \"$bic_lc_total $sch_lc_total + p\" | dc`"
total_noncode="`echo \"$dc_lc_total p\" | dc`"

total="`echo \"$total_code $total_noncode + p\" | dc`"

printf "%7d\t%s\n" "$total" "BRL-CAD Project Total"

echo "========================================="


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
