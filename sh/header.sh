#!/bin/sh
#                       H E A D E R . S H
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
# This script ensures that a legal header has been placed at the top
# of a file.
#
# The script assumes the license and one file as arguments, so example
# uses might be:
#
#   sh/header.sh BSD configure.ac
#
#   find . -type f -name Makefile.am -not -regex '.*src/other.*' -exec sh/header.sh BSD {} \;
#
#   find src/lib* -type f \( -name \*.c -or -name \*.h \) -exec sh/header.sh LGPL {} \;
#
#   find src -type f \( -name \*.c -or -name \*.h \) -not -regex '.*src/lib.*' -exec sh/header.sh GPL {} \;
#
# Author -
#   Christopher Sean Morrison
#
# Source -
#   The U.S. Army Research Laboratory
#   Aberdeen Proving Ground, Maryland 21005-5068  USA
###

LICE="$1"
FILE="$2"
PROJ="$3"
COPY="$4"

# force locale setting to C so things like date output as expected
LC_ALL=C

USAGE="Usage: $0 LICENSE FILE [ProjectName] [CopyrightHolder]"

##################
# validate input #
##################
if [ "x$LICE" = "x" ] ; then
    echo "ERROR: must give a license type (BSD, LGPL, GPL, GFDL)"
    echo "$USAGE"
    exit 1
fi
case $LICE in
    bsd|BSD)
        LICE=BSD
	;;
    lgpl|LGPL)
        LICE=LGPL
	;;
    gpl|GPL)
        LICE=GPL
	;;
    gfdl|fdl|GFDL|FDL)
        LICE=GFDL
    	;;
    *)
        echo "ERROR: Unknown license type: $LICE"
	echo "License should be one of BSD, LGPL, GPL, GFDL"
	echo "$USAGE"
	exit 1
	;;
esac

if [ "x$FILE" = "x" ] ; then
    echo "ERROR: must give the name/path of a file to check/update"
    echo "$USAGE"
    exit 1
elif [ ! -f "$FILE" ] ; then
    echo "ERROR: unable to find $FILE"
    exit 1
elif [ ! -r "$FILE" ] ; then
    echo "ERROR: unable to read $FILE"
    exit 2
elif [ ! -w "$FILE" ] ; then
    echo "ERROR: unable to write to $FILE"
    exit 2
fi

if [ "x$PROJ" = "x" ] ; then
    PROJ="BRL-CAD"
else
    echo "Outputting header for [$PROJ]"
fi

if [ "x$COPY" = "x" ] ; then
    echo "Assuming U.S. Government copyright assignment"
fi

if [ "x$5" != "x" ] ; then
    echo "ERROR: LICENSE should be one of BSD, LGPL, GPL, or GFDL"
    echo "No other arguments should follow."
    echo "$USAGE"
    exit 3
fi


########################
# figure out file type #
########################
# wrap is whether or not in needs to be incased in /* */
# commentprefix is the comment character to prefex each line
###
case $FILE in
    *.sh )
	echo "$FILE is a shell script"
	wrap=0
	commentprefix="#"
	;;
    *.c )
	echo "$FILE is a C source file"
	wrap=1
	commentprefix=" *"
	;;
    *.h )
	echo "$FILE is a C header"
	wrap=1
	commentprefix=" *"
	;;
    *.cc | *.cp | *.cxx | *.cpp | *.cpp | *.CPP | *.c++ | *.C )
	echo "$FILE is a C source file"
	wrap=1
	commentprefix=" *"
	;;
    *.hh | *.H )
	echo "$FILE is a C++ header"
	wrap=1
	commentprefix=" *"
	;;
    *.m )
	echo "$FILE is an Objective-C source file"
	wrap=1
	commentprefix=" *"
	;;
    *.mm | *.M )
	echo "$FILE is an Objective-C++ source file"
	wrap=1
	commentprefix=" *"
	;;
    *.java )
	echo "$FILE is a Java source file"
	wrap=1
	commentprefix=" *"
	;;
    *.tcl )
	echo "$FILE is a Tcl source file"
	wrap=0
	commentprefix="#"
	;;
    *.tk )
	echo "$FILE is a Tk source file"
	wrap=0
	commentprefix="#"
	;;
    *.itcl )
	echo "$FILE is a IncrTcl source file"
	wrap=0
	commentprefix="#"
	;;
    *.itk )
	echo "$FILE is a IncrTk source file"
	wrap=0
	commentprefix="#"
	;;
    *.pl )
	echo "$FILE is a Perl source file"
	wrap=0
	commentprefix="#"
	;;
    *.am )
	echo "$FILE is an Automake template file"
	wrap=0
	commentprefix="#"
	;;
    *.in )
	echo "$FILE is an Autoconf template file"
	wrap=0
	commentprefix="#"
	;;
    *.ac )
	echo "$FILE is an Autoconf template file"
	wrap=0
	commentprefix="#"
	;;
    *.m4 )
	echo "$FILE is an m4 macro file"
	wrap=0
	commentprefix="#"
	;;
    *.mk )
	echo "$FILE is a make resource file"
	wrap=0
	commentprefix="#"
	;;
    *.bat )
	echo "$FILE is a batch shell script"
	wrap=0
	commentprefix="REM "
	;;
    *.vim )
	echo "$FILE is a VIM syntax file"
	wrap=0
	commentprefix="\""
	;;
    *.el )
	echo "$FILE is an Emacs Lisp file"
	wrap=0
	commentprefix=";;"
	;;
    *.[0-9] )
	echo "$FILE is a manual page"
	wrap=0
	commentprefix=".\\\""
	;;
    * )
	# check the first line, see if it is a script
	filesig="`head -n 1 $FILE`"
	case $filesig in
	    */bin/sh )
		echo "$FILE is a shell script"
		wrap=0
		commentprefix="#"
		;;
	    */bin/tclsh )
		echo "$FILE is a Tcl script"
		wrap=0
		commentprefix="#"
		;;
	    */bin/wish )
		echo "$FILE is a Tk script"
		wrap=0
		commentprefix="#"
		;;
	    */bin/perl )
		echo "$FILE is a Perl script"
		wrap=0
		commentprefix="#"
		;;
	    * )
		echo "ERROR: $FILE has an unknown filetype"
		exit 0
		;;
	esac
esac


##################################
# figure out the file title line #
##################################
basefilename=`basename $FILE`
if [ "x$basefilename" = "x" ] ; then
    echo "ERROR: basename of $FILE failed"
    exit 4
fi

title="`echo $basefilename | tr [a-z] [A-Z] | sed 's/\(.\)/\1 /g' | sed 's/ $//'`"
length="`echo $title | wc | awk '{print $3}'`"
if [ "x$length" = "x" ] ; then
    echo "ERROR: could not determine title length??"
    exit 5
fi
titlesansext="`echo $title | sed 's/ \..*//'`"

prefixlen="`echo "$commentprefix " | wc | awk '{print $3}'`"
position=0
if [ $length -lt `expr 69 - $prefixlen - 1` ] ; then
    position=`expr \( \( 69 - $prefixlen - 1 - $length \) / 2 \) - 1`
    if [ "x$position" = "x" ] ; then
	echo "ERROR: could not determine title position??"
	exit 6
    fi
fi

if [ "x$wrap" = "x1" ] ; then
    titleline="* "
else
    titleline="$commentprefix "
fi
while [ $position -gt 0 ] ; do
    titleline="${titleline} "
    position="`expr $position - 1`"
done
titleline="${titleline}${title}"


###################################
# figure out the copyright extent #
###################################
copyright=""
currentyear="`date +%Y`"
copyrightline="`grep -i copyright $FILE | grep -v -i notice | grep -v -i '\.SH' | head -n 1`"
if [ "x$copyrightline" = "x" ] ; then
    copyrightline="`grep -i copyright $FILE | grep -v -i united | grep -v -i '\.SH' | head -n 1`"
fi
if [ "x$copyrightline" = "x" ] ; then
    startyear="$currentyear"
else
    startyear="`echo "$copyrightline" | sed 's/.*\([0-9][0-9][0-9][0-9]\)-[0-9][0-9][0-9][0-9].*/\1/'`"
    echo "start is $startyear"
    if [ `echo $startyear | wc | awk '{print $3}'` -gt 10 -o "x$startyear" = "x" ] ; then
        startyear="`echo "$copyrightline" | sed 's/.*[^0-9]\([0-9][0-9][0-9][0-9]\),[0-9][0-9][0-9][0-9],[0-9][0-9][0-9][0-9][^0-9].*/\1/'`"
	echo "start2 is $startyear"
	if [ `echo $startyear | wc | awk '{print $3}'` -gt 10 -o "x$startyear" = "x" ] ; then
	    # didn't find a year, so use current year
	    startyear="`echo "$copyrightline" | sed 's/.*[^0-9]\([0-9][0-9][0-9][0-9]\),[0-9][0-9][0-9][0-9][^0-9].*/\1/'`"
	    echo "start3 is $startyear"
	    if [ `echo $startyear | wc | awk '{print $3}'` -gt 10 -o "x$startyear" = "x" ] ; then
		startyear="`echo "$copyrightline" | sed 's/.*[^0-9]\([0-9][0-9][0-9][0-9]\)[^0-9].*/\1/'`"
		echo "start4 is $startyear"
		if [ `echo $startyear | wc | awk '{print $3}'` -gt 10 -o "x$startyear" = "x" ] ; then
		    startyear="$currentyear"
		fi
	    fi
	fi
    fi
fi

if [ "x$startyear" != "x$currentyear" ] ; then
    copyright="${startyear}-${currentyear}"
else
    copyright="${currentyear}"
fi

echo "using copyright of $copyright"


##############################
# generate the license block #
##############################
block=""
c="$commentprefix"
block="${block}${titleline}
$c ${PROJ}
$c"

if [ "x$COPY" = "x" ] ; then
    block="${block}
$c Copyright (c) $copyright United States Government as represented by
$c the U.S. Army Research Laboratory.
$c"
else
    block="${block}
$c Copyright (c) $copyright $COPY
$c"
fi


case $LICE in
    BSD)
        block="${block}
$c Redistribution and use in source and binary forms, with or without
$c modification, are permitted provided that the following conditions
$c are met:
$c
$c 1. Redistributions of source code must retain the above copyright
$c notice, this list of conditions and the following disclaimer.
$c
$c 2. Redistributions in binary form must reproduce the above
$c copyright notice, this list of conditions and the following
$c disclaimer in the documentation and/or other materials provided
$c with the distribution.
$c
$c 3. The name of the author may not be used to endorse or promote
$c products derived from this software without specific prior written
$c permission.
$c
$c THIS SOFTWARE IS PROVIDED BY THE AUTHOR \`\`AS IS'' AND ANY EXPRESS
$c OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
$c WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
$c ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
$c DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
$c DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
$c GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
$c INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
$c WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
$c NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
$c SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"
        ;;
    LGPL)
        block="${block}
$c This library is free software; you can redistribute it and/or
$c modify it under the terms of the GNU Lesser General Public License
$c as published by the Free Software Foundation.
$c
$c This library is distributed in the hope that it will be useful, but
$c WITHOUT ANY WARRANTY; without even the implied warranty of
$c MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
$c Lesser General Public License for more details.
$c
$c You should have received a copy of the GNU Lesser General Public
$c License along with this file; see the file named COPYING for more
$c information.
"
        ;;
    GPL)
        block="${block}
$c This program is free software; you can redistribute it and/or
$c modify it under the terms of the GNU General Public License as
$c published by the Free Software Foundation.
$c
$c This program is distributed in the hope that it will be useful, but
$c WITHOUT ANY WARRANTY; without even the implied warranty of
$c MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
$c General Public License for more details.
$c
$c You should have received a copy of the GNU General Public License
$c along with this file; see the file named COPYING for more
$c information.
"
        ;;
    GFDL)
        block="${block}
$c This document is made available under the terms of the GNU Free
$c Documentation License or, at your option, under the terms of the
$c GNU General Public License as published by the Free Software
$c Foundation.  Permission is granted to copy, distribute and/or
$c modify this document under the terms of the GNU Free Documentation
$c License as published by the Free Software Foundation; with no
$c Invariant Sections, no Front-Cover Texts, and no Back-Cover Texts.
$c Permission is also granted to redistribute this document under the
$c terms of the GNU General Public License.
$c
$c You should have received a copy of the GNU Free Documentation
$c License and/or the GNU General Public License along with this
$c document; see the file named COPYING for more information.
"
        ;;
    *)
        echo "ERROR: encountered unknown license type $LICE during processing"
	exit 6
	;;
esac

if [ "x$wrap" = "x1" ] ; then
    block="${block}${c}/"
else
    block="${block}${c}
${c}${c}${c}"
fi


###################################
# see if the license block exists #
###################################
foundtitle="`head -n 5 $FILE | grep "$title" | wc | awk '{print $1}'`"
prepend=no
if [ "x$foundtitle" = "x0" ] ; then
    prepend=yes
else
    licline="`echo "$block" | tail -n 7 | head -n 1`"
    foundfileheader="`head -n 50 $FILE | grep "$licline" | wc | awk '{print $1}'`"
    if [ "x$foundfileheader" = "x0" ] ; then
	prepend=yes
	echo "$FILE already has a title"
    fi
fi

if [ "x$prepend" = "xno" ] ; then
    echo "ERROR: $FILE already has a license header"
    exit 7
fi


#######################
# output the new file #
#######################
if [ -f ${FILE}.backup ] ; then
    echo "ERROR: backup file exists... ${FILE}.backup .. remove it"
    exit 8
fi
echo "$FILE ... appending"

mv -f $FILE ${FILE}.backup

closeit=0
skip=1
lineone="`cat ${FILE}.backup | head -n 1`"
linetwo="`cat ${FILE}.backup | head -n 2 | tail -n 1`"
linethree="`cat ${FILE}.backup | head -n 3 | tail -n 1`"
case "$lineone" in
    "/*"*${title})
        echo "Found C comment start with file header"
	skip=2
	case "$linetwo" in
	    " *")
	        echo "Found empty comment line"
		skip=3
		;;
	esac
	;;
    "/*"*${titlesansext})
        echo "Found C comment start with file header sans extension"
	skip=2
	case "$linetwo" in
	    " *")
	        echo "Found empty comment line"
		skip=3
		;;
	esac
	;;
    "/*")
        echo "Found C comment start"
	skip=2
	case "$linetwo" in
	    " *"*${title})
	        echo "Found old file header"
		skip=3
		;;
	    " *"*${titlesansext})
	        echo "Found old file header sans extension"
		skip=3
		;;
	    " *")
	        echo "Found empty comment line"
		skip=3
		if [ "x$foundtitle" = "x1" ] ; then
		    case "$linethree" in
			" *"*${title})
			    echo "Found old file header"
			    skip=4
			    ;;
			" *"*${titlesansext})
			    echo "Found old file header sans extension"
			    skip=4
			    ;;
		    esac
		fi
		;;
	esac
	;;
    ".TH"*)
	echo "Found manpage title header line"
	echo "$lineone" >> $FILE
	skip=2
	;;
    "/*"*)
        echo "WARNING: Found C comment start with stuff trailing"
	skip=0
	closeit=1
	;;
    "#!/bin/"*)
        echo "Found script exec line"
	echo "$lineone" >> $FILE
	skip=2
	case "$linetwo" in
	    "# "*${title})
	        echo "Found old file header"
		skip=3
		;;
	    "# "*${titlesansext})
	        echo "Found old file header sans extension"
		skip=3
		;;
	esac
	;;
    "# !/bin/"*)
        echo "Found script exec line"
	echo "$lineone" >> $FILE
	skip=2
	case "$linetwo" in
	    "# "*${title})
	        echo "Found old file header"
		skip=3
		;;
	    "# "*${titlesansext})
	        echo "Found old file header sans extension"
		skip=3
		;;
	esac
	;;
    "")
        echo "Found empty line"
	skip=2
	closeit=1
	case "$linetwo" in
	    "/*")
	        echo "Found C comment start"
		skip=3
		closeit=0
		case "$linethree" in
		    " *"*${title})
		        echo "Found old file header"
			skip=4
			;;
		    " *"*${titlesansext})
		        echo "Found old file header sans extension"
			skip=4
			;;
		    " *")
		        echo "Found empty comment line"
			skip=4
			if [ "x$foundtitle" = "x1" ] ; then
			    case "$linethree" in
				" *"*${title})
				    echo "Found old file header"
				    skip=5
				    ;;
				" *"*${titlesansext})
				    echo "Found old file header sans extension"
				    skip=5
				    ;;
			    esac

			fi
			;;
		esac
		;;
	    "/*"*${title})
	        echo "Found C comment start with file header"
		skip=3
		closeit=0
		case "$linetwo" in
		    " *")
	                echo "Found empty comment line"
			skip=4
			;;
		esac
		;;
	    "/*"*${titlesansext})
	        echo "Found C comment start with file header sans extension"
		skip=3
		closeit=0
		case "$linetwo" in
		    " *")
	                echo "Found empty comment line"
			skip=4
			;;
		esac
		;;
	esac
	;;
    [a-z]*)
        echo "found code"
	skip=0
	closeit=1
	;;
    \#include*)
        echo "found code"
	skip=0
	closeit=1
	;;
    \#*${title})
        echo "Found old file header"
	skip=2
	;;
    \#*${titlesansext})
        echo "Found old file header sans extension"
	skip=2
	;;
    \#*)
        echo "found comment line"
	skip=0
	closeit=1
	;;
    \@*)
        echo "found batch command"
	skip=0
	;;
    \REM*)
        echo "found batch comment"
	skip=0
	;;
    *)
        echo "${FILE}:1 ERROR: Unknown line one: $lineone"
	mv -f ${FILE}.backup $FILE
	exit 9
	;;
esac

if [ "x$wrap" = "x1" ] ; then
    if [ "x$closeit" = "x1" ] ; then
	echo "/${block}
/** @file $basefilename
 *
 */
" >> $FILE
    else
	echo "/${block}
/** @file $basefilename" >> $FILE
    fi
else
    echo "${block}" >> $FILE
fi

tail -n +${skip} ${FILE}.backup >> $FILE


exit 0


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
