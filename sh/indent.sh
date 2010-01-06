#!/bin/sh
#                       I N D E N T . S H
# BRL-CAD
#
# Copyright (c) 2005-2010 United States Government as represented by
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
# indent.sh runs emacs in batch mode to automatically indent source.
#
###

files="$*"


# locate our requisite lisp helper script
bir="batch-indent-region.el"
bir_dir="."
for dir in `dirname $0`/../misc misc ../misc . .. ; do
    if [ -f "$dir/$bir" ] ; then
	bir_dir="$dir"
    fi
done

# sanity check
if [ ! -f "$bir_dir/$bir" ] ; then
    echo "ERROR: Unable to find the batch-indent-region.el lisp script"
    echo "       Searched for $bir in:"
    echo "         `dirname $0`/../misc:misc:../misc:.:.."
    exit 1
fi
if [ ! -r "$bir_dir/$bir" ] ; then
    echo "ERROR: found $bir in $dir but the script is not readable"
    exit 1
fi

# generate a list of files to check, excluding directories that are
# not BRL-CAD sources, revision control files, or start with a dot
# among other files.  have to take care if including shell scripts;
# look for mistakes in here/now documents.  this is, if no file
# arguments were provided.
if [ "x$files" = "x" ] ; then
    files="`find . -type f -and \( \
	-name '*.c' -or \
	-name '*.h' -or \
	-name '*.tcl' -or \
	-name '*.tk' -or \
	-name '*.itcl' -or \
	-name '*.itk' -or \
	-name '*.pl' -or \
	-name '*.java' -or \
	-name '*.el' -or \
	-name '*.f' -or \
	-name '*.cc' -or \
	-name '*.cp' -or \
	-name '*.cxx' -or \
	-name '*.cpp' -or \
	-name '*.CPP' -or \
	-name '*.c++' -or \
	-name '*.C' -or \
	-name '*.hh' -or \
	-name '*.H' -or \
	-name '*.m' -or \
	-name '*.mm' -or \
	-name '*.M' \
	\) \
	-not -regex '.*cache.*' \
	-not -regex '.*config.*' \
	-not -regex '.*Makefile.*' \
	| \
	grep -v '/.svn/' | \
	grep -v '/CVS/' | \
	grep -v '/doc/' | \
	grep -v '/m4/' | \
	grep -v '/misc/' | \
	grep -v '/other/' | \
	grep -v 'aclocal.m4' \
	`"
fi

for file in $files ; do
    # sanity checks
    if [ -d "$file" ] ; then
	echo "WARNING: $file is a directory, skipping"
	continue
    elif [ ! -f "$file" ] ; then
	echo "WARNING: $file does not exist, skipping"
	continue
    elif [ ! -r "$file" ] ; then
	echo "WARNING: $file is not readable, skipping"
	continue
    elif [ ! -w "$file" ] ; then
	echo "WARNING: $file is not writeable, skipping"
	continue
    fi

    # process a backup until we're sure it makes a change
    echo "=== BEGIN ===> $file"
    if [ -f "$file.indent.new" ] ; then
	echo "WARNING: $file.indent.new was in the way (overwritten)"
	rm -f "$file.indent.new"
    fi
    cp "$file" "$file.indent.new"

    # do the work
    emacs -batch -l "$bir_dir/$bir" -f batch-indent-region "$file.indent.new"

    # if the file changed, move it into place
    filediff="`diff $file $file.indent.new`"
    if [ "x$filediff" = "x" ] ; then
	# no change, silently remove file
	rm -f "$file.indent.new"
    else
	if [ -f "$file.indent.old" ] ; then
	    echo "WARNING: $file.indent.old was in the way (overwritten)"
	    rm -f "$file.indent.old"
	fi
	mv "$file" "$file.indent.old"
	mv "$file.indent.new" "$file"
    fi

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
