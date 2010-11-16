#                   C O N V E R S I O N . S H
# BRL-CAD
#
# Copyright (c) 2010 United States Government as represented by
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
# Check the status of our conversion capability on given geometry
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

MGED="`which mged`"
if test ! -f "$MGED" ; then
    echo "Unable to find mged, aborting"
    exit 1
fi

if test $# -eq 0 ; then
    echo "No geometry files specified."
    echo "Usage: $0 file1.g [file2.g ...]"
    exit 2
fi

FILES=""
while test $# -gt 0 ; do
    FILE="$1"
    if ! test -f "$FILE" ; then
	echo "Unable to read file [$FILE]"
	shift
	continue
    fi

    WORK="${FILE}.conversion"
    cp "$FILE" "$WORK"

    $MGED -c "$WORK" search . 2>&1 | grep -v Using | while read object ; do

	obj="`basename \"$object\"`"
	found=`$MGED -c "$WORK" search . -name \"${obj}\" 2>&1 | grep -v Using`
	if test "x$found" != "x$object" ; then
	    echo "INTERNAL ERROR: Failed to find [$object] with [$obj] (got [$found])"
	    exit 3
	fi

	nmg=fail
	$MGED -c "$WORK" facetize -n \"${obj}.nmg\" \"${obj}\"
	found=`$MGED -c "$WORK" search . -name \"${obj}.nmg\" 2>&1 | grep -v Using`
	if test "x$found" = "x${object}.nmg" ; then
	    nmg=pass
	fi
	
	bot=fail
	$MGED -c "$WORK" facetize \"${obj}.bot\" \"${obj}\"
	found=`$MGED -c "$WORK" search . -name \"${obj}.bot\" 2>&1 | grep -v Using`
	if test "x$found" = "x${object}.bot" ; then
	    bot=pass
	fi
	
	printf "nmg:%s bot:%s %s:%s\n" $nmg $bot "$FILE" "$object"
    done

    rm -f "$WORK"

    shift
done

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
