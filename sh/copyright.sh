#!/bin/sh
#                    C O P Y R I G H T . S H
# BRL-CAD
#
# Copyright (c) 2004-2016 United States Government as represented by
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
# This script updates BRL-CAD Copyright notices to include the current
# year in the copyright lines.  If provided an argument, the script
# will process just a single file so that it may be used in
# conjunction with find or be used to update files manually.
#
###

files="$*"

# force locale setting to C so things like date output as expected
LC_ALL=C
# required so sed doesn't truncate files with illegal byte sequence
LANG=C

export LC_ALL LANG


# if we weren't already provided with a list of files, generate a list
# of files to check, excluding directories that are not BRL-CAD source
# files, CVS foo, or known binary files.
if [ "x$files" = "x" ] ; then
    files="`find . -type f | \
	grep -v '/.#' | \
	grep -v '\.cmake/' | \
	grep -v '\.deps/' | \
	grep -v '\.libs/' | \
	grep -v '\.svn/' | \
	grep -v 'CVS' | \
	grep -v 'Makefile$' |\
	grep -v 'Makefile.in$' |\
	grep -v 'autom4te.cache' | \
	grep -v 'config.cache' | \
	grep -v 'config.status' | \
	grep -v 'misc/enigma/' | \
	grep -v 'misc/vfont/' | \
	grep -v 'other/' | \
	grep -v '\.bak$' |\
	grep -v '\.bmp$' |\
	grep -v '\.dll$' |\
	grep -v '\.g$' | \
	grep -v '\.gif$' |\
	grep -v '\.ico$' |\
	grep -v '\.jpg$' |\
	grep -v '\.lo$' |\
	grep -v '\.log' |\
	grep -v '\.mpg$' |\
	grep -v '\.o$' |\
	grep -v '\.old$' |\
	grep -v '\.pdf$' |\
	grep -v '\.pix' |\
	grep -v '\.png$' |\
	grep -v '#$' |\
	grep -v '~$' \
	`"
fi

# process the files
for file in $files ; do
    printf $file

  # sanity checks
    if [ -d "$file" ] ; then
	echo ""
	echo "WARNING: $file is a directory, skipping"
	continue
    elif [ ! -f "$file" ] ; then
	echo ""
	echo "WARNING: $file does not exist, skipping"
	continue
    elif [ ! -r "$file" ] ; then
	echo ""
	echo "WARNING: $file is not readable"
	continue
    elif [ ! -w "$file" ] ; then
	echo ""
	echo "WARNING: $file is not writeable"
	continue
    fi

    printf "."

    if [ -f "$file.copyright.new" ] ; then
	# echo ""
	# echo "WARNING: $file.copyright.new is in the way (moving it to .bak)"
	mv $file.copyright.new $file.copyright.new.bak
    elif [ -f "$file.copyright.old" ] ; then
	# echo ""
	# echo "WARNING: $file.copyright.old is in the way (moving it to .bak)"
	mv $file.copyright.old $file.copyright.old.bak
    fi
    printf "."

    year=`date +%Y`

    sed "s/[cC][oO][pP][yY][rR][iI][gG][hH][tT] \{0,1\}(\{0,1\}[cC]\{0,1\})\{0,1\}.\{0,1\} \{0,1\}\([0-9][0-9][0-9][0-9]\) \{0,1\}-\{0,1\} \{0,1\}[0-9]\{0,1\}[0-9]\{0,1\}[0-9]\{0,1\}[0-9]\{0,1\}\([ .;]\{1,\}\)\(.*United \{1,\}States\)/Copyright (c) \1-$year\2\3/" < $file > $file.copyright.new
    printf "."

    modified=no
    filediff="`diff $file $file.copyright.new`"
    if [ "x$filediff" = "x" ] ; then
	printf "."
	rm -f $file.copyright.new
    elif [ ! "x`echo $filediff | grep \"No newline at end of file\"`" = "x" ] ; then
	echo ". `basename $file` has no newline -- SKIPPING"
	rm -f $file.copyright.new
	continue
    elif [ ! "x`echo $filediff | grep \"Binary files\"`" = "x" ] ; then
	echo ". `basename $file` is binary -- SKIPPING"
	rm -f $file.copyright.new
	continue
    else
	printf "."
	modified=yes
	mv $file $file.copyright.old
	mv $file.copyright.new $file
    fi

    if [ "x$modified" = "xyes" ] ; then
	# make sure it's not a current year
	sed "s/Copyright ([cC]) $year-$year/Copyright (c) $year/" < $file > $file.copyright.new
	printf "."

	filediff="`diff $file $file.copyright.new`"
	if [ "x$filediff" = "x" ] ; then
	    printf "."
	    rm -f $file.copyright.new
	elif [ ! "x`echo $filediff | grep \"No newline at end of file\"`" = "x" ] ; then
	    printf "."
	    rm -f $file.copyright.new
	elif [ ! "x`echo $filediff | grep \"Binary files\"`" = "x" ] ; then
	    printf "."
	    rm -f $file.copyright.new
	else
	    printf "."
	    modified=no
	    mv $file $file.repeat.copyright.old
	    mv $file.copyright.new $file
	fi

	if [ "x$modified" = "xyes" ] ; then
	    printf ". `basename $file` is MODIFIED"
	fi
    fi

    echo ""

done  # iteration over files

echo ""
echo "Done."

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
