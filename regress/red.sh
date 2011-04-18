#                        R E D . S H
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
# Basic series of MGED 'red' command sanity tests
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

FAILURES=0

echo "testing mged 'red' command..."
rm -f red.log

# make our starting database
create_db ( ) {
    rm -f red.g
    $MGED -c >> red.log 2>&1 <<EOF
opendb red.g y
make sph sph
comb sph.c u sph
r sph.r u sph.c
cp sph.r hps.r
mater hps.r light 255 0 0 0
quit
EOF

    if test $? != 0 ; then
	echo "INTERNAL ERROR: mged returned non-zero exit status $?"
	exit 1
    fi
    if test ! -f red.g ; then
	echo "INTERNAL ERROR: Unable to run mged, aborting"
	exit 1
    fi

}

assert_different ( ) {
    if test "x`diff $SAMPLE $REDFILE`" = "x" ; then
	echo "ERROR: sed failed"
	exit 1
    fi
}

should_be_different ( ) {
    if test $# -ne 2 ; then
	echo "INTERNAL ERROR: should_be_different has wrong arg count ($# -ne 2)"
	exit 1
    fi
    if test "x$1" = "x" ; then
	echo "INTERNAL ERROR: should_be_different has empty file #1"
	exit 1
    fi
    if test "x$2" = "x" ; then
	echo "INTERNAL ERROR: should_be_different has empty file #2"
	exit 1
    fi
    if test "x`diff $1 $2`" = "x" ; then
	echo "ERROR: 'red' failed  ($1 and $2 are identical, expected change)"
	FAILURES="`expr $FAILURES + 1`"
	export FAILURES
    fi
}

should_be_same ( ) {
    if test $# -ne 2 ; then
	echo "INTERNAL ERROR: should_be_same has wrong arg count ($# -ne 2)"
	exit 1
    fi
    if test "x$1" = "x" ; then
	echo "INTERNAL ERROR: should_be_same has empty file #1"
	exit 1
    fi
    if test "x$2" = "x" ; then
	echo "INTERNAL ERROR: should_be_same has empty file #2"
	exit 1
    fi
    if test "x`diff $1 $2`" != "x" ; then
	echo "ERROR: 'red' failed  ($1 and $2 are different, expected no change)"
	diff -u $1 $2
	FAILURES="`expr $FAILURES + 1`"
	export FAILURES
    fi
}

dump ( ) {
    if test $# -ne 2 ; then
	echo "INTERNAL ERROR: dump has wrong arg count ($# -ne 2)"
	exit 1
    fi
    if test "x$1" = "x" ; then
	echo "INTERNAL ERROR: dump has empty object name #1"
	exit 1
    fi
    if test "x$2" = "x" ; then
	echo "INTERNAL ERROR: dump has empty file name #2"
	exit 1
    fi
    rm -f $2
    EDITOR=cat $MGED -c red.g red $1 2>/dev/null > $2
}

edit_and_dump ( ) {
    if test $# -ne 2 ; then
	echo "INTERNAL ERROR: edit_and_dump has wrong arg count ($# -ne 2)"
	exit 1
    fi
    if test "x$1" = "x" ; then
	echo "INTERNAL ERROR: edit_and_dump has empty object name #1"
	exit 1
    fi
    if test "x$2" = "x" ; then
	echo "INTERNAL ERROR: edit_and_dump has empty file name #2"
	exit 1
    fi
    $MGED -c red.g red $1 >> red.log 2>&1
    dump $1 $2
}

init ( ) {
    if test $# -ne 2 ; then
	echo "INTERNAL ERROR: init has wrong arg count ($# -ne 2)"
	exit 1
    fi
    if test "x$1" = "x" ; then
	echo "INTERNAL ERROR: init has empty description string #1"
	exit 1
    fi
    if test "x$2" = "x" ; then
	echo "INTERNAL ERROR: init has empty file name #2"
	exit 1
    fi
    echo "===== $1 ====="
    create_db
    REDFILE="$2"
    export REDFILE
}

# write out our "editor"
rm -rf red.edit.sh
cat > red.edit.sh <<EOF
#!/bin/sh
cat \$REDFILE > \$1
EOF
chmod u+rwx red.edit.sh
EDITOR=./red.edit.sh


# write out our initial unedited objects, verify sanity
create_db
SAMPLE=red.sph.r.out
dump sph.r $SAMPLE
ELPMAS=red.hps.r.out
dump hps.r $ELPMAS
should_be_different $SAMPLE $ELPMAS

########
# nada #
########

init "Verify no edit is no change" red.virgin.out
cat $SAMPLE | sed 's/  / /g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new

##########
# region #
##########

init "Changing region to yes" red.region.yes.out
cat $SAMPLE | sed 's/region[^_].*=.*/region = yes/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new # was already a region

init "Changing region to 1" red.region.one.out
cat $SAMPLE | sed 's/region[^_].*=.*/region = 1/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new # was already a region

init "Changing region to empty" red.region.empty.out
cat $SAMPLE | sed 's/region[^_].*=.*/region =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new

init "Changing region to no" red.region.no.out
cat $SAMPLE | sed 's/region[^_].*=.*/region = no/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new

init "Changing region to 0" red.region.zero.out
cat $SAMPLE | sed 's/region[^_].*=.*/region = 0/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new

#############
# region_id #
#############

init "Changing region_id to safe" red.region_id.safe.out
cat $SAMPLE | sed 's/region_id.*=.*/region_id = 2000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/2000/1000/g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing region_id to empty" red.region_id.empty.out
cat $SAMPLE | sed 's/region_id.*=.*/region_id =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new

init "Changing region_id to unsafe" red.region_id.unsafe.out
cat $SAMPLE | sed 's/region_id.*=.*/region_id = -1000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/-1000/1000/g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

###############
# material_id #
###############

init "Changing material ID to safe" red.material_id.safe.out
cat $SAMPLE | sed 's/material_id.*=.*/material_id = 2/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/2/1/g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing material ID to empty" red.material_id.empty.out
cat $SAMPLE | sed 's/material_id.*=.*/material_id =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new

init "Changing material ID to unsafe" red.material_id.unsafe.out
cat $SAMPLE | sed 's/material_id.*=.*/material_id = -1/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/-1/1/g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

#######
# los #
#######

init "Changing los to safe" red.los.safe.out
cat $SAMPLE | sed 's/los.*=.*/los = 50/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/50/100/g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing los to empty" red.los.empty.out
cat $SAMPLE | sed 's/los.*=.*/los =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new

init "Changing los to unsafe" red.los.unsafe.out
cat $SAMPLE | sed 's/los.*=.*/los = 200/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/200/100/g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

#######
# air #
#######

init "Changing air to safe" red.air.safe.out
cat $SAMPLE | sed 's/air.*=.*/air = 1111/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/1111//g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing air to empty" red.air.empty.out
cat $SAMPLE | sed 's/air.*=.*/air =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new

init "Changing air to unsafe" red.air.unsafe.out
cat $SAMPLE | sed 's/air.*=.*/air = -1/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/-1//g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

#########
# color #
#########

init "Changing color to safe" red.color.safe.out
cat $SAMPLE | sed 's/color.*=.*/color = 255\/255\/255/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/255.255.255//g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing color to safe with comma delimeter" red.color.delim.out
cat $SAMPLE | sed 's/color.*=.*/color = 255,255,255/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/255.255.255//g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing color to empty" red.color.empty.out
cat $SAMPLE | sed 's/color.*=.*/color = /g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new
cat $SAMPLE | sed 's/color.*=.*/color =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new

init "Changing color to unsafe" red.color.unsafe.out
cat $SAMPLE | sed 's/color.*=.*/color = -123/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/-123//g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

##########
# shader #
##########

init "Changing shader to safe" red.shader.safe.out
cat $SAMPLE | sed 's/shader.*=.*/shader = plastic/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/plastic//g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing shader to empty" red.shader.empty.out
cat $SAMPLE | sed 's/shader.*=.*/shader =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new

init "Changing shader to unsafe" red.shader.unsafe.out
cat $SAMPLE | sed 's/shader.*=.*/shader = 1234567890/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/1234567890//g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

########
# tree #
########

init "Changing combination tree to safe" red.tree.safe.out
cat $SAMPLE | sed 's/u sph.c/u sph/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/u sph/u sph.c/g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing combination tree to empty" red.tree.empty.out
cat $SAMPLE | sed 's/u sph.c//g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new

init "Changing combination tree to safe" red.tree.unsafe.out
cat $SAMPLE | sed 's/u sph.c/i like pancakes/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new # edit should fail

########
# name #
########

init "Changing name to safe name" red.name.safe.out
cat $SAMPLE | sed 's/sph.r/sph_new.r/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new
dump sph_new.r $REDFILE.new
should_be_same $REDFILE $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/sph_new.r/sph.r/g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing name to safe name with extra edit" red.name.safe.out
cat $SAMPLE | sed 's/sph.r/sph_new.r/g' | sed 's/1000/2000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_same $SAMPLE $REDFILE.new
dump sph_new.r $REDFILE.new
should_be_different $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/sph_new.r/sph.r/g' | sed 's/2000/1000/g' > $REDFILE.test
should_be_same $SAMPLE $REDFILE.test

init "Changing name to empty name" red.name.empty.out
cat $SAMPLE | sed 's/sph.r//g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $REDFILE $REDFILE.new
should_be_same $SAMPLE $REDFILE.new

init "Changing name to empty name with extra edit" red.name.emptyedit.out
cat $SAMPLE | sed 's/sph.r//g' | sed 's/1000/2000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
should_be_different $REDFILE $REDFILE.new # empty name
should_be_same $SAMPLE $REDFILE.new

init "Changing name to unsafe name" red.name.unsafe.out
cat $SAMPLE | sed 's/sph.r/hps.r/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
dump hps.r $REDFILE.test
should_be_same $SAMPLE $REDFILE.new
should_be_same $ELPMAS $REDFILE.test
should_be_different $REDFILE.new $REDFILE.test

init "Changing name to unsafe name with edit" red.name.unsafe.out
cat $SAMPLE | sed 's/sph.r/hps.r/g' | sed 's/1000/2000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
dump hps.r $REDFILE.test
should_be_same $SAMPLE $REDFILE.new
should_be_same $ELPMAS $REDFILE.test
should_be_different $REDFILE.new $REDFILE.test


if test $FAILURES -eq 0 ; then
    echo "-> mged 'red' check succeeded"
else
    echo "-> mged 'red' check FAILED"
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
