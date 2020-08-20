#                        R E D . S H
# BRL-CAD
#
# Copyright (c) 2008-2020 United States Government as represented by
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
. "$1/regress/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/red.log
    rm -f $LOGFILE
fi
log "=== TESTING red command ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    log "Unable to find mged, aborting"
    exit 1
fi

STATUS=0


# make our starting database
create_db ( ) {
    rm -f red.g
    $MGED -c >> $LOGFILE 2>&1 <<EOF
opendb red.g y
make sph sph
make sph sph2
comb sph.c u sph
r sph.r u sph.c
cp sph.r hps.r
mater hps.r light 255 0 0 0
cp sph.c sph_rot.c
e sph_rot.c
oed sph_rot.c sph
rot 20.2 30.1 10.4
accept
quit
EOF

    if test $? != 0 ; then
	log "INTERNAL ERROR: mged returned non-zero exit status $?"
	exit 1
    fi
    if test ! -f red.g ; then
	log "INTERNAL ERROR: Unable to run mged, aborting"
	exit 1
    fi

}


assert_different ( ) {
    if test "x`diff $SAMPLE $REDFILE`" = "x" ; then
	log "ERROR: sed failed"
	exit 1
    fi
}


dump ( ) {
    if test $# -ne 2 ; then
	log "INTERNAL ERROR: dump has wrong arg count ($# -ne 2)"
	exit 1
    fi
    if test "x$1" = "x" ; then
	log "INTERNAL ERROR: dump has empty object name #1"
	exit 1
    fi
    if test "x$2" = "x" ; then
	log "INTERNAL ERROR: dump has empty file name #2"
	exit 1
    fi
    rm -f $2
    EDITOR=cat $MGED -c red.g red $1 2>> $LOGFILE > $2
}


edit_and_dump ( ) {
    if test $# -ne 2 ; then
	log "INTERNAL ERROR: edit_and_dump has wrong arg count ($# -ne 2)"
	exit 1
    fi
    if test "x$1" = "x" ; then
	log "INTERNAL ERROR: edit_and_dump has empty object name #1"
	exit 1
    fi
    if test "x$2" = "x" ; then
	log "INTERNAL ERROR: edit_and_dump has empty file name #2"
	exit 1
    fi
    run $MGED -c red.g red $1
    dump $1 $2
}


init ( ) {
    if test $# -ne 2 ; then
	log "INTERNAL ERROR: init has wrong arg count ($# -ne 2)"
	exit 1
    fi
    if test "x$1" = "x" ; then
	log "INTERNAL ERROR: init has empty description string #1"
	exit 1
    fi
    if test "x$2" = "x" ; then
	log "INTERNAL ERROR: init has empty file name #2"
	exit 1
    fi
    log "===== $1 ====="
    create_db
    REDFILE="$2"
    export REDFILE
}

# write out our "editor"
rm -f red.edit.sh
cat > red.edit.sh <<EOF
#!/bin/sh
cat \$REDFILE > \$1
EOF
chmod u+rwx red.edit.sh
EDITOR="./red.edit.sh"
export EDITOR

# write out our initial unedited objects, verify sanity
create_db
SAMPLE=red.sph.r.out
dump sph.r $SAMPLE
ELPMAS=red.hps.r.out
dump hps.r $ELPMAS
files_differ $SAMPLE $ELPMAS
MATRIX=red.sph_rot.c.out
dump sph_rot.c $MATRIX
files_differ $SAMPLE $MATRIX

########
# nada #
########

init "Verify no edit is no change" red.virgin.out
cat $SAMPLE | sed 's/  / /g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new

##########
# region #
##########

init "Changing region to yes" red.region.yes.out
cat $SAMPLE | sed 's/region[^_].*=.*/region = yes/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new # was already a region

init "Changing region to 1" red.region.one.out
cat $SAMPLE | sed 's/region[^_].*=.*/region = 1/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new # was already a region

init "Changing region to empty" red.region.empty.out
cat $SAMPLE | sed 's/region[^_].*=.*/region =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new

init "Changing region to no" red.region.no.out
cat $SAMPLE | sed 's/region[^_].*=.*/region = no/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new

init "Changing region to 0" red.region.zero.out
cat $SAMPLE | sed 's/region[^_].*=.*/region = 0/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new

# check for rev 50521 fix
init "Editing region combination (-r50521)" red.region.edit-comb.out
cat $SAMPLE  > $REDFILE
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new
cat $REDFILE.new > $REDFILE.test
echo " u sph2" >> $REDFILE.test
files_differ $SAMPLE $REDFILE.test
cat $REDFILE.test | sed 's/u sph2/ /g' > $REDFILE.test2
dump sph.r $REDFILE.test2
files_match $SAMPLE $REDFILE.test2

#############
# region_id #
#############

init "Changing region_id to safe" red.region_id.safe.out
cat $SAMPLE | sed 's/region_id.*=.*/region_id = 2000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/2000/1000/g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing region_id to empty" red.region_id.empty.out
cat $SAMPLE | sed 's/region_id.*=.*/region_id =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new

init "Changing region_id to unsafe" red.region_id.unsafe.out
cat $SAMPLE | sed 's/region_id.*=.*/region_id = -1000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/-1000/1000/g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

###############
# material_id #
###############

init "Changing material ID to safe" red.material_id.safe.out
cat $SAMPLE | sed 's/material_id.*=.*/material_id = 2/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/2/1/g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing material ID to empty" red.material_id.empty.out
cat $SAMPLE | sed 's/material_id.*=.*/material_id =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new

init "Changing material ID to unsafe" red.material_id.unsafe.out
cat $SAMPLE | sed 's/material_id.*=.*/material_id = -1/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/-1/1/g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

#######
# los #
#######

init "Changing los to safe" red.los.safe.out
cat $SAMPLE | sed 's/los.*=.*/los = 50/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/50/100/g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing los to empty" red.los.empty.out
cat $SAMPLE | sed 's/los.*=.*/los =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new

init "Changing los to unsafe" red.los.unsafe.out
cat $SAMPLE | sed 's/los.*=.*/los = 200/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/200/100/g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

#######
# air #
#######

init "Changing air to safe" red.air.safe.out
cat $SAMPLE | sed 's/air.*=.*/air = 1111/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/1111//g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing air to empty" red.air.empty.out
cat $SAMPLE | sed 's/air.*=.*/air =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new

init "Changing air to unsafe" red.air.unsafe.out
cat $SAMPLE | sed 's/air.*=.*/air = -1/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/-1//g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

#########
# color #
#########

init "Changing color to safe" red.color.safe.out
cat $SAMPLE | sed 's/color.*=.*/color = 255\/255\/255/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/255.255.255//g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing color to safe with comma delimiter" red.color.delim.out
cat $SAMPLE | sed 's/color.*=.*/color = 255,255,255/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/255.255.255//g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing color to empty" red.color.empty.out
cat $SAMPLE | sed 's/color.*=.*/color = /g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new
cat $SAMPLE | sed 's/color.*=.*/color =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new

init "Changing color to unsafe" red.color.unsafe.out
cat $SAMPLE | sed 's/color.*=.*/color = -123/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new

##########
# shader #
##########

init "Changing shader to safe" red.shader.safe.out
cat $SAMPLE | sed 's/shader.*=.*/shader = plastic/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/plastic//g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing shader to empty" red.shader.empty.out
cat $SAMPLE | sed 's/shader.*=.*/shader =/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new

init "Changing shader to unsafe" red.shader.unsafe.out
cat $SAMPLE | sed 's/shader.*=.*/shader = 1234567890/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/1234567890//g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

########
# tree #
########

init "Changing combination tree to safe" red.tree.safe.out
cat $SAMPLE | sed 's/u sph.c/u sph/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/u sph/u sph.c/g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing combination tree to empty" red.tree.empty.out
cat $SAMPLE | sed 's/u sph.c//g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new

init "Changing combination tree to safe" red.tree.unsafe.out
cat $SAMPLE | sed 's/u sph.c/i like pancakes/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new # edit should fail

########
# name #
########

init "Changing name to safe name" red.name.safe.out
cat $SAMPLE | sed 's/sph.r/sph_new.r/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new
dump sph_new.r $REDFILE.new
files_match $REDFILE $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/sph_new.r/sph.r/g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing name to safe name with extra edit" red.name.safe.out
cat $SAMPLE | sed 's/sph.r/sph_new.r/g' | sed 's/1000/2000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_match $SAMPLE $REDFILE.new
dump sph_new.r $REDFILE.new
files_differ $SAMPLE $REDFILE.new
cat $REDFILE.new | sed 's/sph_new.r/sph.r/g' | sed 's/2000/1000/g' > $REDFILE.test
files_match $SAMPLE $REDFILE.test

init "Changing name to empty name" red.name.empty.out
cat $SAMPLE | sed 's/sph.r//g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $REDFILE $REDFILE.new
files_match $SAMPLE $REDFILE.new

init "Changing name to empty name with extra edit" red.name.emptyedit.out
cat $SAMPLE | sed 's/sph.r//g' | sed 's/1000/2000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
files_differ $REDFILE $REDFILE.new # empty name
files_match $SAMPLE $REDFILE.new

init "Changing name to unsafe name" red.name.unsafe.out
cat $SAMPLE | sed 's/sph.r/hps.r/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
dump hps.r $REDFILE.test
files_match $SAMPLE $REDFILE.new
files_match $ELPMAS $REDFILE.test
files_differ $REDFILE.new $REDFILE.test

init "Changing name to unsafe name with edit" red.name.unsafe.out
cat $SAMPLE | sed 's/sph.r/hps.r/g' | sed 's/1000/2000/g' > $REDFILE
assert_different
edit_and_dump sph.r $REDFILE.new
dump hps.r $REDFILE.test
files_match $SAMPLE $REDFILE.new
files_match $ELPMAS $REDFILE.test
files_differ $REDFILE.new $REDFILE.test

##########
# matrix #
##########
init "Verify that red no-op leaves matrix intact" red.matrix.noop.out
cat $MATRIX | sed 's/  / /g' > $REDFILE
assert_different
edit_and_dump sph_rot.c $REDFILE.new
files_match $MATRIX $REDFILE.new


if test $STATUS -eq 0 ; then
    log "-> mged 'red' check succeeded"
else
    log "-> mged 'red' check FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

exit $STATUS


# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
