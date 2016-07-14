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
if [ "x$DEBUG" != "x" ] ; then
    echo "DEBUG flag is SET, writing out file listing per enumeration."
    echo ""
fi
echo "Now processing, please wait..."
echo ""

# NOTE: disable shell globbing or we'll end up in a world of hurt
# attempting to make all the following regular expressions work with
# the find command while still matching or excluding properly.
set -f

source_pattern="-name *.c -or -name *.cxx -or -name *.cpp -or -name *.h -or -name *.hxx -or -name *.hpp -or -name *.java -or -name *.f"
script_pattern="-name *.sh -or -name *.tcl -or -name *.tk -or -name *.itcl -or -name *.itk -or -name *.pl -or -name *.php -or -name *.inc -or -name \*.mged"
parser_pattern="-name *.y -or -name *.l -or -name *.yy"
code_pattern="`echo $source_pattern -or $script_pattern -or $parser_pattern`"

# matches code that is not part of BRL-CAD, but is bundled within the
# hierarchy for convenience and build infrastructure needs.
other_pattern="-name other -or -regex .*/other/.* -or -name tools -or -regex .*/misc/tools/.*"

# pattern to match any build dirs and common temp/backup/trash files,
# in case we're running from an active checkout directory.
trash_pattern="-regex .*/\.[^\.].* -or -regex .*~ -or -regex .*/#.*"

# pattern to match anything that is not ours
those_pattern="$other_pattern -or $trash_pattern"

# prime a hierarchy cache, get a directory listing
dir_list=`find $BASE -type d -not \( $those_pattern \)`


echo "-----------------------------------------"
echo "--       FILESYSTEM ORGANIZATION       --"
echo "-----------------------------------------"

dir_count="`echo \"$dir_list\" | wc -l`"

printf "%7d\t%s\n" "$dir_count" "BRL-CAD Directories"
[ "x$DEBUG" = "x" ] || echo $dir_list | while read line ; do printf "\t%s\n" $line ; done

# count number of files
file_list=`find $BASE -type f -not \( $those_pattern \)`
file_count="`echo \"$file_list\" | wc -l`"

printf "%7d\t%s\n" "$file_count" "BRL-CAD Files"
[ "x$DEBUG" = "x" ] || echo $file_list | while read line ; do printf "\t%s\n" $line ; done

# count number of data files
data_list=`find $BASE -type f -not \( $code_pattern \) -not \( $those_pattern \)`
data_count="`echo \"$data_list\" | wc -l`"

printf "\t%7d\t%s\n" "$data_count" "BRL-CAD Data Files"
[ "x$DEBUG" = "x" ] || echo $data_list | while read line ; do printf "\t%s\n" $line ; done

# count number of source files
srcs_list=`find $BASE -type f -and \( $code_pattern \) -not \( $those_pattern \)`
srcs_count="`echo \"$srcs_list\" | wc -l`"

printf "\t%7d\t%s\n" "$srcs_count" "BRL-CAD Source Files"
[ "x$DEBUG" = "x" ] || echo $srcs_list | while read line ; do printf "\t%s\n" $line ; done

# count number of 3rd party directories
otherdir_list=`find $BASE -type d -and \( -name other -or -regex '.*/other/.*' \) -not \( $trash_pattern \)`
otherdir_count="`echo \"$otherdir_list\" | wc -l`"

printf "%7d\t%s\n" "$otherdir_count" "3rd Party Directories"
[ "x$DEBUG" = "x" ] || echo $otherdir_list | while read line ; do printf "\t%s\n" $line ; done

# count number of 3rd party files
otherfile_list=`find $BASE -type f -regex '.*/other/.*' -not \( $trash_pattern \)`
otherfile_count="`echo \"$otherfile_list\" | wc -l`"

printf "%7d\t%s\n" "$otherfile_count" "3rd Party Files"
[ "x$DEBUG" = "x" ] || echo $otherfile_list | while read line ; do printf "\t%s\n" $line ; done

# count number of 3rd party data files
otherdata_list=`find $BASE -type f -regex '.*/other/.*' -not \( $code_pattern \) -not \( $trash_pattern \)`
otherdata_count="`echo \"$otherdata_list\" | wc -l`"

printf "\t%7d\t%s\n" "$otherdata_count" "3rd Party Data Files"
[ "x$DEBUG" = "x" ] || echo $otherdata_list | while read line ; do printf "\t%s\n" $line ; done

# count number of 3rd party source files
othersrcs_files=`find $BASE -type f -regex '.*/other/.*' -and \( $code_pattern \) -not \( $trash_pattern \)`
othersrcs_count="`echo \"$othersrcs_files\" | wc -l`"

printf "\t%7d\t%s\n" "$othersrcs_count" "3rd Party Source Files"
[ "x$DEBUG" = "x" ] || echo $othersrcs_list | while read line ; do printf "\t%s\n" $line ; done

printf "\n"


# prime our compilation product searching with a list of build files
cmake_list=`find $BASE -type f -name CMakeLists.txt -not \( $those_pattern \)`

echo "-----------------------------------------"
echo "--        COMPILATION PRODUCTS         --"
echo "-----------------------------------------"

# count number of libraries
libs_list="`echo \"$cmake_list\" | xargs grep BRLCAD_ADDLIB\\( | sed 's/#.*//g' | grep -v -e ':$' | cut -d\(  -f2 | awk '{print $1}' | sort | uniq`"
libs_count="`echo \"$libs_list\" | wc -l`"

printf "%7d\t%s\n" "$libs_count" "BRL-CAD Libraries"
[ "x$DEBUG" = "x" ] || echo $libs_list | while read line ; do printf "\t%s\n" $line ; done

# count number of applications
apps_list="`echo \"$cmake_list\" | xargs grep BRLCAD_ADDEXEC\\( | sed 's/\#.*//g' | grep -v -e ':$' | cut -d\(  -f2 | awk '{print $1}' | sort | uniq`"
apps_count="`echo \"$apps_list\" | wc -l`"

printf "%7d\t%s\n" "$apps_count" "BRL-CAD Applications"
[ "x$DEBUG" = "x" ] || echo $apps_list | while read line ; do printf "\t%s\n" $line ; done

# count number of installed applications
installed_apps_list="`echo \"$cmake_list\" | xargs grep BRLCAD_ADDEXEC\\( | grep -v NO_INSTALL | sed 's/\#.*//g' | grep -v -e ':$' | cut -d\(  -f2 | awk '{print $1}' | sort | uniq`"
installed_apps_count="`echo \"$installed_apps_list\" | wc -l`"

printf "\t%7d\t%s\n" "$installed_apps_count" "Installed"
[ "x$DEBUG" = "x" ] || echo $installed_apps_list | while read line ; do printf "\t%s\n" $line ; done

# count number of non-installed applications
uninstalled_apps_list="`echo \"$cmake_list\" | xargs grep BRLCAD_ADDEXEC\\( | grep NO_INSTALL | sed 's/\#.*//g' | grep -v -e ':$' | cut -d\(  -f2 | awk '{print $1}' | sort | uniq`"
uninstalled_apps_count="`echo \"$uninstalled_apps_list\" | wc -l`"

printf "\t%7d\t%s\n" "$uninstalled_apps_count" "Not Installed"
[ "x$DEBUG" = "x" ] || echo $uninstalled_apps_list | while read line ; do printf "\t%s\n" $line ; done

printf "\n"


echo "-----------------------------------------"
echo "--          LINE COUNT TOTALS          --"
echo "-----------------------------------------"
# echo "   w/ws+= means with whitespace lines"
# echo "-----------------------------------------"

# compute build infrastructure line counts
bic=`find $BASE -type f \( -name configure -or -name CMakeLists.txt -or -regex .*\.cmake$ -or -regex .*\.cmake.in$ -or -regex .*/CMake.*\.in$ -or -regex .*/CMake.*\.sh$ -or -regex .*/sh/.*\.sh$ \) -not \( $those_pattern \)`
bic_lc="`echo \"$bic\" | sort | xargs wc -l`"
bic_lc_lines="`echo \"$bic_lc\" | grep -v 'total$' | awk '{total += $1} END {print total}'`"
bic_lc_blank="`echo \"$bic\" | xargs awk ' /^[  ]*$/ { ++x } END { print x } ' | awk '{total += $1} END {print total}'`"
bic_lc_total="`expr $bic_lc_lines - $bic_lc_blank`"

printf "%7d\t%s\n" "$bic_lc_total" "Build Infrastructure w/ws+=$bic_lc_blank"
[ "x$DEBUG" = "x" ] || echo $bic | while read line ; do printf "\t%s\n" $line ; done

# compute documentation line counts
dc=`find $BASE -type f \( -regex .*/[A-Z]*$ -or -regex .*/README.* -or -regex .*/TODO.* -or -regex .*/INSTALL.* -or -name ChangeLog -or -name \*.txt -or -name \*.tr -or -name \*.htm\* -or -name \*.xml -or -name \*.bib -or -name \*.tbl -or -name \*.mm -or -name \*csv \) -not -regex .*/misc/.* -not -regex .*/legal/.* -not -regex .*CMakeLists.txt.* -not -regex .*CMake.* -not -regex .*LICENSE.*\..* -not \( $those_pattern \)`
dc_lc="`echo \"$dc\" | sort | xargs wc -l`"
dc_lc_lines="`echo \"$dc_lc\" | grep -v 'total$' | awk '{total += $1} END {print total}'`"
dc_lc_blank="`echo \"$dc\" | xargs awk ' /^[  ]*$/ { ++x } END { print x } ' | awk '{total += $1} END {print total}'`"
dc_lc_total="`expr $dc_lc_lines - $dc_lc_blank`"

printf "%7d\t%s\n" "$dc_lc_total" "Documentation w/ws+=$dc_lc_blank"
[ "x$DEBUG" = "x" ] || echo $dc | while read line ; do printf "\t%s\n" $line ; done

# compute script code line counts (intentionally not matching /misc/ due to src/external)
scripts=`find $BASE -type f \( $script_pattern \) -not -regex .*/sh/.* -not -regex .*misc.* -not \( $those_pattern \)`
scripts_lc="`echo \"$scripts\" | sort | xargs wc -l`"
scripts_lc_lines="`echo \"$scripts_lc\" | grep -v 'total$' | awk '{total += $1} END {print total}'`"
scripts_lc_blank="`echo \"$scripts\" | xargs awk ' /^[  ]*$/ { ++x } END { print x } ' | awk '{total += $1} END {print total}'`"
scripts_lc_total="`expr $scripts_lc_lines - $scripts_lc_blank`"

printf "%7d\t%s\n" "$scripts_lc_total" "Scripts w/ws+=$scripts_lc_blank"
[ "x$DEBUG" = "x" ] || echo $scripts | while read line ; do printf "\t%s\n" $line ; done

# compute application code line counts (intentionally not matching /misc/ due to src/external)
sourcebin=`find $BASE -type f \( $source_pattern  \) -not -regex .*/sh/.* -not -regex .*misc.* -not -regex .*lib.* -not -regex .*/include/.* -not \( $those_pattern \)`
sourcebin_lc="`echo \"$sourcebin\" | sort | xargs wc -l`"
sourcebin_lc_lines="`echo \"$sourcebin_lc\" | grep -v 'total$' | awk '{total += $1} END {print total}'`"
sourcebin_lc_blank="`echo \"$sourcebin\" | xargs awk ' /^[      ]*$/ { ++x } END { print x } ' | awk '{total += $1} END {print total}'`"
sourcebin_lc_total="`expr $sourcebin_lc_lines - $sourcebin_lc_blank`"

printf "%7d\t%s\n" "$sourcebin_lc_total" "Application Sources w/ws+=$sourcebin_lc_blank"
[ "x$DEBUG" = "x" ] || echo $sourcebin | while read line ; do printf "\t%s\n" $line ; done

# compute library code line counts (intentionally not matching /misc due to src/external)
sourcelib=`find $BASE -type f -and \( -regex .*/lib.* -or -regex .*/include/.* \) -and \( $source_pattern \) -not -regex .*/sh/.* -not -regex .*misc.* -not \( $those_pattern \)`
sourcelib_lc="`echo \"$sourcelib\" | sort | xargs wc -l`"
sourcelib_lc_lines="`echo \"$sourcelib_lc\" | grep -v 'total$' | awk '{total += $1} END {print total}'`"
sourcelib_lc_blank="`echo \"$sourcelib\" | xargs awk ' /^[      ]*$/ { ++x } END { print x } ' | awk '{total += $1} END {print total}'`"
sourcelib_lc_total="`expr $sourcelib_lc_lines - $sourcelib_lc_blank`"

printf "%7d\t%s\n" "$sourcelib_lc_total" "Library Sources w/ws+=$sourcelib_lc_blank"
[ "x$DEBUG" = "x" ] || echo $sourcelib | while read line ; do printf "\t%s\n" $line ; done

echo "-----------------------------------------"

blank_lc_total="`echo \"$bic_lc_blank $dc_lc_blank $scripts_lc_blank $sourcebin_lc_blank $sourcelib_lc_blank ++++ p\" | dc`"

printf "%7d\t%s\n" "$blank_lc_total" "BRL-CAD Blank (ws) Lines "

# compute 3rd party code line counts
other=`find $BASE -type f -and \( $code_pattern \) -and \( $other_pattern \)`
other_lc="`echo \"$other\" | sort | xargs wc -l`"
other_lc_lines="`echo \"$other_lc\" | grep -v 'total$' | awk '{total += $1} END {print total}'`"
other_lc_blank="`echo \"$other\" | xargs awk ' /^[      ]*$/ { ++x } END { print x } ' | awk '{total += $1} END {print total}'`"
other_lc_total="`expr $other_lc_lines - $other_lc_blank`"

printf "%7d\t%s\n" "$other_lc_total" "3rd Party Code w/ws+=$other_lc_blank"
[ "x$DEBUG" = "x" ] || echo $other | while read line ; do printf "\t%s\n" $line ; done

echo "-----------------------------------------"

real_lc_total="`echo \"$bic_lc_total $dc_lc_total $scripts_lc_total $sourcebin_lc_total $sourcelib_lc_total ++++ p\" | dc`"
repo_lc_total="`echo \"$real_lc_total $blank_lc_total $other_lc_total $other_lc_blank +++ p\" | dc`"

printf "%7d\t%s\n" "$repo_lc_total" "BRL-CAD Repository Total"

srcs_lc_total="`echo \"$real_lc_total $blank_lc_total + p\" | dc`"

printf "%7d\t%s\n" "$srcs_lc_total" "BRL-CAD Sources w/ Spaces"
printf "%7d\t%s\n" "$real_lc_total" "BRL-CAD Source Code Total"

echo "========================================="


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
