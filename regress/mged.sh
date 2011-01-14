#                       M G E D . S H
# BRL-CAD
#
# Copyright (c) 2008-2011 United States Government as represented by
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
# Basic series of MGED sanity checks
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. $1/regress/library.sh

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    echo "Unable to find mged, aborting"
    exit 1
fi


# test all commands
echo "testing mged commands..."

# make an empty database
rm -f mged.g
$MGED -c > mged.log 2>&1 <<EOF
opendb mged.g y
quit
EOF

if test ! -f mged.g ; then
    echo "Unable to run mged, aborting"
    exit 1
fi

cmds="`$MGED -c mged.g ? 2>&1 | grep -v Using`"
help="`$MGED -c mged.g help 2>&1 | grep -v Using`"
# cmds="$cmds `$MGED -c mged.g ?lib 2>&1`"
# cmds="$cmds `$MGED -c mged.g ?devel 2>&1`"

FAILED=0
for cmd in $cmds ; do
    echo "...$cmd"

    if test "x$cmd" = "x%" ; then
	# % is special because it invokes a shell
	$MGED -c mged.g $cmd > /dev/null 2>&1 <<EOF
exit
EOF
	if test $? != 0 ; then
	    echo "ERROR: $cmd returned non-zero exit status $?"
	    echo "Output: $output"
	    FAILED="`expr $FAILED + 1`"
	fi
	continue
    elif test "x$cmd" = "xedcolor" ; then
	# edcolor is special because it kicks off an editor
	echo "XXX: Unable to test edcolor"
	echo "It probably shouldn't kick off an editor without an argument"
	continue
    fi

    # make sure each command exists and will run without error
    output="`$MGED -c mged.g $cmd 2>&1`"
    if test $? != 0 ; then
	echo "ERROR: $cmd returned non-zero exit status $?"
	echo "Output: $output"
	FAILED="`expr $FAILED + 1`"
	continue
    fi
    if test "x`echo \"$output\" | grep -i invalid`" != "x" ; then
	echo "ERROR: $cmd does not exist!"
	echo "Output: $output"
	FAILED="`expr $FAILED + 1`"
	continue
    fi
    if test "x`echo \"$output\" | grep -i error | grep -i -v _error | grep -i -v error_`" != "x" ; then
	echo "ERROR: $cmd reported an error on default use"
	echo "Output: $output"
	FAILED="`expr $FAILED + 1`"
	continue
    fi

    # make sure each command has help listed
    output="`$MGED -c mged.g help $cmd 2>&1`"
    if test "x`echo \"$output\" | grep -i 'no help found'`" != "x" ; then
	echo "ERROR: $cmd does not have help"
	FAILED="`expr $FAILED + 1`"
	continue
    fi
done

if test $FAILED -eq 0 ; then
    echo "-> mged check succeeded"
else
    echo "-> mged check FAILED"
fi

exit $FAILED


# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
