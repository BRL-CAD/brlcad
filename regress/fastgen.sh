#                     F A S T G E N . S H
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
# Basic series of fast4-g sanity checks
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. $1/regress/library.sh

F4G="`ensearch fast4-g`"
if test ! -f "$F4G" ; then
    echo "Unable to find fast4-g, aborting"
    exit 1
fi
G_DIFF="`ensearch g_diff`"
if test ! -f "$G_DIFF" ; then
    echo "Unable to find g_diff, aborting"
    exit 1
fi

echo "testing fast4-g importer..."

# create a fastgen file with unix line endings
rm -f fastgen_box.fast4
echo "Creating an input file with UNIX line-endings"
cat > fastgen_box.fast4 <<EOF
SECTION        1     501       2       0
\$COMMENT displayColor 0/157/157
GRID           1          12.200  -1.000 -10.805
GRID           2          12.200   1.000 -10.805
GRID           3          13.700  -1.000 -10.805
GRID           4          13.700   1.000 -10.805
GRID           5          12.200  -1.000  -8.805
GRID           6          13.700  -1.000  -8.805
GRID           7          12.200   1.000  -8.805
GRID           8          13.700   1.000  -8.805
CTRI           1       1       1       2       3           0.100       2
CTRI           2       1       3       2       4           0.100       2
CTRI           3       1       5       1       6           0.100       2
CTRI           4       1       6       1       3           0.100       2
CTRI           5       1       7       5       8           0.100       2
CTRI           6       1       8       5       6           0.100       2
CTRI           7       1       2       7       4           0.100       2
CTRI           8       1       4       7       8           0.100       2
CTRI           9       1       5       7       1           0.100       2
CTRI          10       1       1       7       2           0.100       2
CTRI          11       1       8       6       4           0.100       2
CTRI          12       1       4       6       3           0.100       2
EOF

rm -f fastgen_unix.g
echo "\$ $F4G fastgen_box.fast4 fastgen_unix.g"
$F4G fastgen_box.fast4 fastgen_unix.g
if test "$?" -ne 0 ; then
    echo "ERROR running $F4G fastgen_box.fast4 fastgen_unix.g"
    exit 1
fi
if test ! -f fastgen_unix.g ; then
    echo "Unable to convert fastgen_box.fast4 to fastgen_unix.g with fast4-g, aborting"
    exit 1
fi

echo "Creating an input file with DOS line-endings"
if test ! -f $PATH_TO_THIS/fastgen_dos.fast4 ; then
    echo "Unable to find fastgen_dos.fast4"
    exit 1
fi
rm -f fastgen_box.fast4
cp $PATH_TO_THIS/fastgen_dos.fast4 fastgen_box.fast4

rm -f fastgen_dos.g
echo "\$ $F4G fastgen_box.fast4 fastgen_dos.g"
$F4G fastgen_box.fast4 fastgen_dos.g
if test "$?" -ne 0 ; then
    echo "ERROR running $F4G fastgen_box.fast4 fastgen_dos.g"
    exit 1
fi
if test ! -f fastgen_dos.g ; then
    echo "Unable to convert fastgen_box.fast4 to fastgen_dos.g with fast4-g, aborting"
    exit 1
fi

echo "Comparing geometry files from sources with DOS and UNIX line endings."
echo "\$ $G_DIFF fastgen_unix.g fastgen_dos.g"
$G_DIFF fastgen_unix.g fastgen_dos.g
if test "$?" -ne 0 ; then
    echo "ERROR running $G_DIFF fastgen_unix.g fastgen_dos.g"
    exit 1
fi

echo "-> fastgen check succeeded"

exit $FAILED

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
