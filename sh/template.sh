#                     T E M P L A T E . S H
# BRL-CAD
#
# Copyright (c) 2007 United States Government as represented by
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
# Generates an empty template file with a standard header and footer
# already applied.  Basically, this is a wrapper on a couple other
# steps and scripts that are frequently used to stub out new files
# being added.
#
# Examples:
#
#   # create a new template C file with an LGPL header
#   sh/template.sh lgpl somefile.c
#
#   # create a new shell script file under the BSD license
#   sh/template.sh bsd newscript.sh
#
# Author -
#   Christopher Sean Morrison
#
# Source -
#   BRL-CAD Open Source
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
    echo "ERROR: must give a license type (BSD, BDL, LGPL)"
    echo "$USAGE"
    exit 1
fi
case $LICE in
    bsd|BSD)
	LICE=BSD
	;;
    bdl|BDL)
	LICE=BDL
	;;
    lgpl|LGPL)
	LICE=LGPL
	;;
    *)
	echo "ERROR: Unknown or unsupported license type: $LICE"
	echo "License should be one of BSD, BDL, LGPL"
	echo "$USAGE"
	exit 1
	;;
esac

if [ "x$FILE" = "x" ] ; then
    echo "ERROR: must give the name/path of a file to check/update"
    echo "$USAGE"
    exit 1
fi

# create the file with sanity checks
touch "$FILE"
if [ ! -f "$FILE" ] ; then
    echo "ERROR: unable to find $FILE"
    exit 1
elif [ ! -r "$FILE" ] ; then
    echo "ERROR: unable to read $FILE"
    exit 2
elif [ ! -w "$FILE" ] ; then
    echo "ERROR: unable to write to $FILE"
    exit 2
fi

# set up permissions to something reasonable for CVS
chmod ug+rw "$FILE"
case "x$FILE" in
    x*.sh)
	# shell scripts need execute bit when being added to CVS
	chmod ug+x "$FILE"
	;;
esac

# find the header script
for dir in `dirname $0` . .. sh ../sh ; do
    header_sh="$dir/header.sh"
    if [ -f "$header_sh" ] ; then
	break
    fi
done
if [ ! -f "$header_sh" ] ; then
    rm -f "$FILE"
    echo "ERROR: Unable to find the header.sh script"
    echo "       Searched for header.sh in:"
    echo "         `dirname $0`:.:..:sh:../sh"
    exit 1
fi

# apply the header
output="`/bin/sh \"$header_sh\" \"$LICE\" \"$FILE\" \"$PROJ\" \"$COPY\" 2>&1`"
if [ $? -ne 0 ] ; then
    rm -f "$FILE"
    echo "$output"
    echo "ERROR: the header failed to apply, aborting"
    exit 1
fi

# get rid of the backup
rm -f "$FILE.backup"

# find the footer script
for dir in `dirname $0` . .. sh ../sh ; do
    footer_sh="$dir/footer.sh"
    if [ -f "$footer_sh" ] ; then
	break
    fi
done
if [ ! -f "$footer_sh" ] ; then
    rm -f "$FILE"
    echo "ERROR: Unable to find the footer.sh script"
    echo "       Searched for footer.sh in:"
    echo "         `dirname $0`:.:..:sh:../sh"
    exit 1
fi

# apply the footer
output="`/bin/sh \"$footer_sh\" \"$FILE\" 2>&1`"
if [ $? -ne 0 ] ; then
    rm -f "$FILE"
    echo "$output"
    echo "ERROR: the header failed to apply, aborting"
    exit 1
fi

# get rid of the backup
rm -f "$FILE.backup"

echo "$FILE successfully created with $LICE license"

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
