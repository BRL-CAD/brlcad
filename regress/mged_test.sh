#!/bin/sh
#                    M G E D _ T E S T . S H
# BRL-CAD
#
# Copyright (c) 2010-2011 United States Government as represented by
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


# Clear out older results files
rm -f mged.g mged.log mged_files

# Create mged.g as an empty db file.
$MGED -c mged.g <<EOF > mged.log 2>&1
EOF

# The difficulty with creating a self-contained test script for MGED
# is the number of commands that make sense only after OTHER commands
# have already been executed - this unavoidably complicates the creation
# and execution of testing scripts.  It also makes it more difficult to
# be sure all scripts have been tested - a side-by-side comparison of
# a complete command list with this script would not be very helpful
# unless the commands are organized in some fashion.  An alphabetical
# organization makes the most sense from a comparison standpoint as
# well as providing a "correct" place for new commands, but rules
# out a "one-shot" in-order sh script.  In order to address both
# concerns, each individual command will create its own $CMDNAME.mged_regress
# script file containing the per-command logic, and those scripts
# will be combined at the end to form the "runnable" master script.
# This means placement logic is separate from the individual command
# logic, and each command will have two entries to maintain.  The check
# performed to make sure no command is missing from the final script is
# to make sure the line count of the final mged.mged_regress file matches
# the sum of all the individual #CMDNAME.mged_regress files.  The final
# mged.mged_regress file will be organized according to functionality -
# for example, all geometry creation commands will be grouped - in, make,
# special purpose commands like tire, etc.  Similarly, geometry editing
# commands like translate, rot, oed, etc. will be grouped.

#
#                  3 P T A R B
#

cat > 3ptarb.mged_regress << EOF
3ptarb 3ptarb_test.s 0 0 0 10 0 0 10 10 0 z 0 10 6
EOF

#
#                  A C C E P T
#
cat > accept.mged_regress << EOF
in accept.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
Z
e accept.s
sed accept.s
l accept.s
tra 10 10 10
accept
l accept.s
Z
EOF

#
#		A D C
#
cat > adc.mged_regress << EOF
adc
knob
adc
EOF


#
#		A E
#
cat > ae.mged_regress << EOF
ae 11 12.2 111
view quat
view ypr
view aet
view center
view eye
view size
EOF

#
#                  A R B
#

cat > arb.mged_regress << EOF
arb arb_test1.s 30 3
arb arb_test2.s 55 20
EOF

#
#                A R C E D
#
cat > arced.mged_regress << EOF
in arced1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
make arced2.s ell
make arced3.s eto
comb arced1.c u arced1.s u arced2.s
comb arced2.c u arced2.s u arced3.s
arced arced1.c/arced1.s matrix rarc xlate 30 30 5
l arced1.c
arced arced1.c/arced2.s matrix rarc rot 5 15 35
l arced1.c
arced arced2.c/arced2.s matrix rarc rot 33 33 33
l arced1.c
l arced2.c
arced arced2.c/arced3.s matrix rarc rot 10 10 14
l arced2.c
arced arced2.c/arced3.s matrix lmul xlate 13 13 13
l arced2.c
arced arced2.c/arced3.s matrix rmul rot 4 4 40
l arced2.c
EOF

#
#                A R O T
#
cat > arot.mged_regress << EOF
in arot.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
Z
e arot.s
sed arot.s
arot 1 2 -9 60
accept
Z
EOF


#
#		A U T O V I E W
#
cat > autoview.mged_regress << EOF
Z
view center -1000 -1000 0
view aet 34 23 2
autoview
view quat
view ypr
view aet
view center
view eye
view size
EOF

#
#		B
#
cat > B.mged_regress << EOF
in B_test1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
make B_test2.s eto
comb B_test.c u B_test1.s
Z
e B_test2.s
who
B B_test1.s B_test.c
who
EOF

#
#          B U I L D _ R E G I O N
#
# build_region requires a large number of target objects with the correct
# naming conventions - rather than do this manually, clone is used - this
# means clone should be tested before build_region

cat > build_region.mged_regress << EOF
make -s 10 br_epa.s epa
clone -p 20 0 0 -r 0 0 10 -n 36 br_epa.s
build_region br_epa 0 10000
build_region -a 42 br_epa 2000 10000
EOF

#
#			C E N T E R
#
cat > center.mged_regress << EOF
center
center 111 2 -300
center
EOF

#
#                       C L O N E
#

cat > clone.mged_regress << EOF
make -s 10 eto_a.s eto
clone -a 10 10 10 10 eto_a.s
g clone1.c eto_a.s*
cp eto_a.s eto_b.s
clone -b 10 100 100 100 eto_b.s
g clone2.c eto_b.s*
cp eto_b.s eto_c.s
clone -i 30 -m x 10 -n 1 eto_c.s
g clone3.c eto_c.s*
cp eto_c.s eto_d.s
clone -i 5 -p 30 30 30 -r 10 10 15 -n 20 eto_d.s
g clone4.c eto_d.s*
cp eto_d.s eto_e.s
clone -i 11 -t 1 1 1 -n 20 eto_e.s
g clone5.c eto_e.s*
cp clone5.c clone_a.c
clone -i 2 -p 5 0 10 -r 10 20 30 -n 20 clone_a.c
g comb_clone.c clone_a.c*
EOF


#
#                        C O M B
#
# Test commands for c/comb - because comb acts on existing primitives,
# first set up primitives for comb to act on.

cat > comb.mged_regress << EOF
make -s 10 comb_sph1.s sph
make -s 20 comb_sph2.s sph
make -s 30 comb_sph3.s sph
comb comb1.c u comb_sph1.s u comb_sph2.s u comb_sph3.s
comb comb2.c u comb_sph1.s + comb_sph2.s + comb_sph3.s
comb comb3.c u comb_sph3.s - comb_sph2.s - comb_sph1.s
comb comb4.c u comb_sph2.s - comb_sph1.s + comb_sph3.s
comb comb5.c u comb_sph2.s - comb_sph1.s + comb_sph3.s
comb comb6.c u comb2.c + comb1.c u comb3.c u comb4.c - comb5.c
EOF

#
#               C O P Y M A T
#
cat > copymat.mged_regress << EOF
in copymat.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb copymat1.c u copymat.s
comb copymat2.c u copymat.s
arced copymat1.c/copymat.s matrix rarc xlate 10 30 20
l copymat1.c
l copymat2.c
copymat copymat1.c/copymat.s copymat2.c/copymat.s
l copymat1.c
l copymat2.c
EOF

#
#                       C P
#
# Shallow object copy - test after make and comb but before
# clone

cat > cp.mged_regress << EOF
make -s 1 cp_sph.s sph
comb cp_comb.c u cp_sph.s
cp cp_sph.s cp_copy_sph.s
cp cp_comb.c cp_copy_comb.c
EOF

#
#                  C P I
#
# cpi leaves the new shape in an edit state - for
# scripting purposes, an accept is added to leave
# editing state and allow for further operations

cat > cpi.mged_regress << EOF
in cpi-base-cyl.s rcc 1 2 3 10 10 10 3.5
cpi cpi-base-cyl.s cpi-cyl.s
accept
EOF

#
#     D R A W / E, E R A S E / D and W H O
#
# Since erasing something from a view depends on it
# first being drawn successfully, and restoring the
# view to its original state after drawing requires
# erase, combine the tests for these two commands.
# The who command is needed to see the results of
# these draw operations from the shell, and can't
# be meaningfully tested on its own without having
# something drawn, so include who in this test as well
cat > draw_erase_who.mged_regress << EOF
make draw.s sph
make e.s sph
e e.s
draw draw.s
make d.s sph
make erase.s sph
e d.s
e erase.s
d d.s
erase erase.s
who
EOF

#
#                 D R A W
#
# Test draw with combinations
cat > draw.mged_regress << EOF
make draw_comb_shape.s sph
comb draw.c u draw_comb_shape.s
e draw.c
who
e draw_comb_shape.s
who
erase draw.c
erase draw_comb_shape.s
EOF

#
#	D A L L / E R A S E _ A L L
#
cat > dall.mged_regress << EOF
in dall1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
in dall2.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb dall1.c u dall1.s
comb dall2.c u dall1.s
comb dall3.c u dall1.s u dall2.s
comb dall4.c u dall2.s
Z
e dall1.s dall2.s dall1.c dall2.c dall3.c dall4.c
who
dall dall1.s
who
Z
e dall1.s dall2.s dall1.c dall2.c dall3.c dall4.c
who
erase_all dall1.s
who
EOF

#
#                 E R A S E
#
# Test erase with combinations
cat > erase.mged_regress << EOF
make erase_comb_shape.s sph
comb erase.c u erase_comb_shape.s
e erase.c
e erase_comb_shape.s
erase erase.c
who
erase erase_comb_shape.s
EOF

#
#	E Y E _ P T
#
cat > eye_pt.mged_regress << EOF
eye_pt
eye_pt 100 100 100
eye_pt
EOF

#
#       E X T R U D E
#
cat > extrude.mged_regress << EOF
in extrude.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
Z
e extrude.s
sed extrude.s
extrude 1234 100
accept
Z
EOF

#
#                F A C E D E F
#
cat > facedef.mged_regress << EOF
in facedef_a.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
cp facedef_a.s facedef_b.s
cp facedef_a.s facedef_c.s
cp facedef_a.s facedef_d.s
Z
e facedef_a.s
sed facedef_a.s
facedef 1234 a 1 .3 0 20
accept
Z
e facedef_b.s
sed facedef_b.s
facedef 2367 b 100 100 0 100 100 100 0 100 0
accept
Z
e facedef_c.s
sed facedef_c.s
facedef 3478 c 20 10 5 0 0
accept
Z
e facedef_d.s
sed facedef_d.s
facedef 1458 d 30 10 10
accept
Z
EOF

#
#                        G
#
# Test commands for g - because g acts on existing primitives,
# first set up primitives and combinations to act on.

cat > g.mged_regress << EOF
make -s 10 g_sph1.s sph
make -s 20 g_sph2.s sph
make -s 30 g_sph3.s sph
comb g_comb1.c u comb_sph1.s u comb_sph2.s u comb_sph3.s
g g1.c g_sph1.s g_sph2.s g_sph3.s
g g2.c g_sph1.s g_sph2.s g_comb1.c
EOF

#
#			H I D E
#
cat > hide.mged_regress << EOF
in hidden_1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
in hidden_2.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb hidden.c u hidden_2.s
ls hidden*
hide hidden_1.s
hide hidden.c
ls hidden*
EOF

#
#                         I
#
cat > i.mged_regress << EOF
make i_prim1.s sph
make i_prim2.s sph
comb i_comb.c u i_prim1.s
i i_prim2.s i_comb.c
EOF

#
#                         I N
#
# Test commands for in - because the behavior of this command must be
# checked on a per-primitive basis, one make command exists for each
# primitive type with working make support.

# Note:  need more sophisticated bot input example

cat > in.mged_regress << EOF
in in_arb4.s arb4 3 -3 -3 3 0 -3 3 0 0 0 0 -3
in in_arb5.s arb5 1 0 0 1 2 0 3 2 0 3 0 0 1.5 1.5 5
in in_arb6.s arb6 2 -.5 -.5 2 0 -.5 2 0 0 2 -.5 0 2.5 -.3 -.5 2.5 -.3 0
in in_arb7.s arb7 3.25 -1.25 -0.75 3.25 -0.25 -0.75 3.25 -0.25 0.25 3.25 -1.25 -0.25 2.25 -1.25 -0.75 2.25 -0.25 -0.75 2.25 -0.25 -0.25
in in_arb8.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
in in_arbn.s arbn 8 1 0 0 1000 -1 0 0 1000 0 1 0 1000 0 -1 0 1000 0 0 1 1000 0 0 -1 1000 0.57735 0.57735 0.57735 1000 -0.57735 -0.57735 -0.57735 200
in in_ars.s ars 3 3 0 0 0 0 0 100 100 0 100 100 100 100 0 0 200
in in_bot.s bot 4 4 2 1 0 0 0 10 10 0 -10 10 0 0 10 10 0 1 2 1 2 3 3 2 0 0 3 1
in in_ehy.s ehy 0 0 0 0 10 10 10 0 0 10 3
in in_ell.s ell 10 0 0 -12 0 0 0 -3 0 0 0 5
in in_ell1.s ell1 3 2 8 3 -1 8 4
in in_epa.s epa 0 0 0 3 0 0 0 5 0 3
in in_eto.s eto 0 0 0 1 1 1 10 0 2 2 1.5
in in_grip.s grip 0 0 0 3 0 0 6
in in_half.s half 1 1 1 5
in in_hyp.s hyp 0 0 0 0 0 10 3 0 0 4 .3
in in_part.s part 0 0 0 0 0 16 4 2
in in_pipe.s pipe 4 0 0 0 3 5 6 0 0 3 3 5 7 3 4 8 2 6 10 8 8 10 0 6 8
in in_rcc.s rcc 0 0 0 3 3 30 7
in in_rec.s rec 0 0 0 3 3 10 10 0 0 0 3 0
in in_rhc.s rhc 0 0 0 0 0 10 3 0 0 4 3
in in_rpc.s rpc 0 0 0 0 0 4 0 1 0 3
in in_rpp.s rpp 0 30 -3 12 -1 22
in in_sph.s sph 42 42 42 42
in in_tec.s tec 0 0 0 0 0 10 5 0 0 0 3 0 .6
in in_tgc.s tgc 0 0 0 0 0 10 5 0 0 0 8 0 2 9
in in_tor.s tor 0 0 0 1 1 3 5 2
in in_trc.s trc 0 0 0 0 0 10 4 7
EOF

# Unsupported
# in in_nmg.s nmg
# in in_sketch.s sketch

# Because sketch does not currently have in command support, it is necessary to
# to provide an example sketch for extrude.
cat >> in.mged_regress << EOF
put {extrude_test_sketch} sketch V {10 20 30} A {1 0 0} B {0 1 0} VL { {250 0} {500 0} {500 500} {0 500} {0 250} {250 250} {125 125} {0 125} {125 0} {200 200} } SL { { bezier D 4 P { 4 7 9 8 0 } } { line S 0 E 1 } { line S 1 E 2 } { line S 2 E 3 } { line S 3 E 4 } { carc S 6 E 5 R -1 L 0 O 0 } }
in in_extrude.s extrude 0 0 0 0 0 1000 10 0 0 0 10 0 extrude_test_sketch 1
EOF

#
#            K E Y P O I N T
#
cat > keypoint.mged_regress << EOF
make keypoint.s sph
e keypoint.s
sed keypoint.s
keypoint
keypoint -10 3 2
keypoint
accept
d keypoint.s
EOF

#
#		K I L L
#
cat > kill.mged_regress << EOF
in kill.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb kill.c u kill.s
ls kill.*
l kill.*
kill kill.s
ls kill.*
l kill.*
kill kill.c
ls kill.*
EOF

#
#		K I L L A L L
#
cat > killall.mged_regress << EOF
in killall_1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
cp killall_1.s killall_2.s
comb killall_1.c u killall_1.s u killall_2.s
cp killall_1.c killall_2.c
comb killall.c u killall_1.c u killall_2.s
ls killall*
l killall*
killall killall_1.s
ls killall*
l killall*
killall killall_1.c
ls killall*
l killall*
killall killall_2.s
ls killall*
l killall*
killall killall_2.c
ls killall*
l killall*
killall killall.c
ls killall*
l killall*
EOF

#
#		K I L L T R E E
#
cat > killtree.mged_regress << EOF
put {killtree_sketch.s} sketch V {10 20 30} A {1 0 0} B {0 1 0} VL { {250 0} {500 0} {500 500} {0 500} {0 250} {250 250} {125 125} {0 125} {125 0} {200 200} } SL { { bezier D 4 P { 4 7 9 8 0 } } { line S 0 E 1 } { line S 1 E 2 } { line S 2 E 3 } { line S 3 E 4 } { carc S 6 E 5 R -1 L 0 O 0 } }
in killtree_extrude.s extrude 0 0 0 0 0 1000 10 0 0 0 10 0 killtree_sketch.s
in killtree_arb8.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
in killtree_1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb killtree_1.c u killtree_1.s
comb killtree.c u killtree_1.c u killtree_arb8.s u killtree_extrude.s killtree_sketch.s
ls killtree*
l killtree*
killtree killtree.c
ls killtree*
l killtree*
put {killtree_sketch.s} sketch V {10 20 30} A {1 0 0} B {0 1 0} VL { {250 0} {500 0} {500 500} {0 500} {0 250} {250 250} {125 125} {0 125} {125 0} {200 200} } SL { { bezier D 4 P { 4 7 9 8 0 } } { line S 0 E 1 } { line S 1 E 2 } { line S 2 E 3 } { line S 3 E 4 } { carc S 6 E 5 R -1 L 0 O 0 } }
in killtree_extrude.s extrude 0 0 0 0 0 1000 10 0 0 0 10 0 killtree_sketch.s
in killtree_arb8.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
in killtree_1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb killtree_1.c u killtree_1.s
comb killtree.c u killtree_1.c u killtree_arb8.s u killtree_extrude.s
ls killtree*
l killtree*
killtree killtree.c
ls killtree*
l killtree*
EOF



#
#		K N O B
#
cat > knob.mged_regress << EOF
knob zero
knob
knob x 2 y 2 z 2 X 3 Y 3 Z 3 S 30
knob
knob zero
knob ax 89 ay 73 az 3 aX 300 aY -200 aZ 23 aS 10
knob
knob zero
adc
knob xadc -10 yadc 3 ang1 23 ang2 55 distadc 44
knob
knob zero
knob
knob x 2 y 2 z 2 X 3 Y 3 Z 3 S 30
knob ax 89 ay 73 az 3 aX 300 aY -200 aZ 23 aS 10
knob xadc -10 yadc 3 ang1 23 ang2 55 distadc 44
knob
adc
knob zero
EOF

#
#		L O A D V I E W
#
cat > loadview.mged_regress << EOF
autoview
ae 35 25 0
view quat
loadview mged_saveview_test.rt
view quat
EOF

#
#		L O O K A T
#
cat > lookat.mged_regress << EOF
view quat
lookat 50 30 33
view quat
EOF

#
#                          M A K E
#
# Test commands for make - because the behavior of this command must be
# checked on a per-primitive basis, one make command exists for each
# primitive type with working make support.

cat > make.mged_regress << EOF
make make_arb4.s arb4
make make_arb5.s arb5
make make_arb6.s arb6
make make_arb7.s arb7
make make_arb8.s arb8
make make_arbn.s arbn
make make_ars.s ars
make make_bot.s bot
make make_ehy.s ehy
make make_ell.s ell
make make_ell1.s ell1
make make_epa.s epa
make make_eto.s eto
make make_extrude.s extrude
make make_grip.s grip
make make_half.s half
make make_nmg.s nmg
make make_part.s part
make make_pipe.s pipe
make make_rcc.s rcc
make make_rec.s rec
make make_rhc.s rhc
make make_rpc.s rpc
make make_rpp.s rpp
make make_sketch.s sketch
make make_sph.s sph
make make_tec.s tec
make make_tgc.s tgc
make make_tor.s tor
make make_trc.s trc
EOF

# Unimplemented as of 7.12.6
# make make_hyp.s hyp


# Next, test the effects of explicitly setting the origin
#cat >> make.mged_regress << EOF
#make -o 0,0,100 make_arb4_o.s arb4
#make -o 0,0,100 make_arb5_o.s arb5
#make -o 0,0,100 make_arb6_o.s arb6
#make -o 0,0,100 make_arb7_o.s arb7
#make -o 0,0,100 make_arb8_o.s arb8
#make -o 0,0,100 make_arbn_o.s arbn
#make -o 0,0,100 make_ars_o.s ars
#make -o 0,0,100 make_bot_o.s bot
#make -o 0,0,100 make_ehy_o.s ehy
#make -o 0,0,100 make_ell_o.s ell
#make -o 0,0,100 make_ell1_o.s ell1
#make -o 0,0,100 make_epa_o.s epa
#make -o 0,0,100 make_eto_o.s eto
#make -o 0,0,100 make_extrude_o.s extrude
#make -o 0,0,100 make_grip_o.s grip
#make -o 0,0,100 make_half_o.s half
#make -o 0,0,100 make_nmg_o.s nmg
#make -o 0,0,100 make_part_o.s part
#make -o 0,0,100 make_pipe_o.s pipe
#make -o 0,0,100 make_rcc_o.s rcc
#make -o 0,0,100 make_rec_o.s rec
#make -o 0,0,100 make_rhc_o.s rhc
#make -o 0,0,100 make_rpc_o.s rpc
#make -o 0,0,100 make_rpp_o.s rpp
#make -o 0,0,100 make_sketch_o.s sketch
#make -o 0,0,100 make_sph_o.s sph
#make -o 0,0,100 make_tec_o.s tec
#make -o 0,0,100 make_tgc_o.s tgc
#make -o 0,0,100 make_tor_o.s tor
#make -o 0,0,100 make_trc_o.s trc
#EOF

# And finally, test the size factor option
cat >> make.mged_regress << EOF
make -s 42 make_arb4_s.s arb4
make -s 42 make_arb5_s.s arb5
make -s 42 make_arb6_s.s arb6
make -s 42 make_arb7_s.s arb7
make -s 42 make_arb8_s.s arb8
make -s 42 make_arbn_s.s arbn
make -s 42 make_ars_s.s ars
make -s 42 make_bot_s.s bot
make -s 42 make_ehy_s.s ehy
make -s 42 make_ell_s.s ell
make -s 42 make_ell1_s.s ell1
make -s 42 make_epa_s.s epa
make -s 42 make_eto_s.s eto
make -s 42 make_extrude_s.s extrude
make -s 42 make_grip_s.s grip
make -s 42 make_half_s.s half
make -s 42 make_nmg_s.s nmg
make -s 42 make_part_s.s part
make -s 42 make_pipe_s.s pipe
make -s 42 make_rcc_s.s rcc
make -s 42 make_rec_s.s rec
make -s 42 make_rhc_s.s rhc
make -s 42 make_rpc_s.s rpc
make -s 42 make_rpp_s.s rpp
make -s 42 make_sketch_s.s sketch
make -s 42 make_sph_s.s sph
make -s 42 make_tec_s.s tec
make -s 42 make_tgc_s.s tgc
make -s 42 make_tor_s.s tor
make -s 42 make_trc_s.s trc
EOF

#
#                  M A K E _ B B
#
# In order to test this routine for each primitive,
# the primitives from the make and in command tests will
# be reused.  BE SURE THE make AND in TESTS RUN BEFORE
# RUNNING THIS TEST. It also uses comb so run after the
# comb test.

cat > make_bb.mged_regress << EOF
make_bb arb4_bb.s make_arb4.s
make_bb arb5_bb.s make_arb5.s
make_bb arb6_bb.s make_arb6.s
make_bb arb7_bb.s make_arb7.s
make_bb arb8_bb.s make_arb8.s
make_bb arbn_bb.s make_arbn.s
make_bb ars_bb.s make_ars.s
make_bb bot_bb.s make_bot.s
make_bb ehy_bb.s make_ehy.s
make_bb ell_bb.s make_ell.s
make_bb ell1_bb.s make_ell1.s
make_bb epa_bb.s make_epa.s
make_bb eto_bb.s make_eto.s
make_bb extrude_bb.s in_extrude.s
make_bb grip_bb.s make_grip.s
make_bb half_bb.s make_half.s
make_bb nmg_bb.s make_nmg.s
make_bb part_bb.s make_part.s
make_bb pipe_bb.s make_pipe.s
make_bb rcc_bb.s make_rcc.s
make_bb rec_bb.s make_rec.s
make_bb rhc_bb.s make_rhc.s
make_bb rpc_bb.s make_rpc.s
make_bb rpp_bb.s make_rpp.s
make_bb sketch_bb.s extrude_test_sketch
make_bb sph_bb.s make_sph.s
make_bb tec_bb.s make_tec.s
make_bb tgc_bb.s make_tgc.s
make_bb tor_bb.s make_tor.s
make_bb trc_bb.s make_trc.s
comb bb_prim1.c u make_arb4.s u make_arb5.s u make_arb6.s u make_arb7.s u make_arb8.s u make_arbn.s
make_bb comb_bb1.s bb_prim1.c
comb bb_prim2.c u make_ars.s u make_bot.s u make_ehy.s u make_ell.s u make_ell1.s u make_epa.s
make_bb comb_bb2.s bb_prim2.c
comb bb_prim3.c u make_eto.s u in_extrude.s u make_grip.s u make_nmg.s u make_part.s u make_pipe.s
make_bb comb_bb3.s bb_prim3.c
comb bb_prim4.c u make_rcc.s u make_rec.s u make_rhc.s u make_rpc.s u make_rpp.s
make_bb comb_bb4.s bb_prim4.c
comb bb_prim5.c u extrude_test_sketch u make_sph.s u make_tec.s u make_tgc.s u make_tor.s u make_trc.s
make_bb comb_bb5.s bb_prim5.c
EOF

#
#                  M I R F A C E
#
cat > mirface.mged_regress << EOF
in mirface.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
Z
e mirface.s
sed mirface.s
tra -10000 0 0
mirface 1234 x
accept
Z
EOF

#
#                  M I R R O R
#

cat > mirror.mged_regress << EOF
make -s 30 mirror_eto1.s eto
in mirror_eto2.s eto -10 0 0 -1 -1 -1 20 0 3 3 2.5
comb mirror1.c u mirror_eto1.s u mirror_eto2.s
mirror -d {1 1 1} -o 100 mirror1.c mirror2.c
mirror -d {1 1 4} -p {3 2 2} -o 30 mirror1.c mirror3.c
mirror -x mirror1.c mirror4.c
mirror -y mirror1.c mirror5.c
mirror -z mirror1.c mirror6.c
EOF

#
#		M R O T
#
cat > mrot.mged_regress << EOF
view quat
mrot 3 22 -1
view quat
EOF

#
#                       M V
#
cat > mv.mged_regress << EOF
make -s 40 mv_sph.s sph
make -s 40 mv_comb_sph.s sph
comb mv_comb.c u mv_comb_sph.s
mv mv_sph.s moved_sph.s
mv mv_comb.c moved_comb.c
EOF


#
#                       M V A L L
#
cat > mvall.mged_regress << EOF
make -s 40 mvall_sph.s sph
make -s 40 mvall_comb_sph.s sph
comb mvall_comb_1.c u mvall_comb_sph.s
comb moved_all_comb_2.c u mvall_comb_sph.s
mvall mvall_sph.s movedall_sph.s
mvall mvall_comb_1.c movedall_comb1.c
mvall mvall_comb_sph.s moved_all_comb_sph.s
EOF

#
#                      O E D
#
cat > oed.mged_regress << EOF
make oed.s sph
comb oed1.c u oed.s
comb oed2.c u oed1.c
e oed2.c
status state
oed oed2.c oed1.c/oed.s
status state
accept
status state
d oed2.c
EOF

#
#		O R I E N T A T I O N
#
cat > orientation.mged_regress << EOF
view quat
orientation 0.285190215334 0.670633110412 0.649962411504 0.215561211557
view quat
EOF

#
#               O R O T
#
cat > orot.mged_regress << EOF
in orot.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb orot.c u orot.s
Z
e orot.c
oed / orot.c/orot.s
orot 11 -23 22
accept
Z
EOF

#
#                O S C A L E
#
cat > oscale.mged_regress << EOF
make oscale.s eto
comb oscale.c u oscale.s
Z
e oscale.c
oed / oscale.c/oscale.s
oscale 15
accept
Z
EOF

#
#		O V E R L A Y
#
#  NOTE : generates output file
cat > overlay.mged_regress << EOF
Z
overlay mged_plot_test.pl
make overlay_additional_shape.s epa
e overlay_additional_shape.s
ps mged_overlay_test.ps
EOF

#
#                  P E R M U T E
#
cat > permute.mged_regress << EOF
in permute.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
Z
e permute.s
sed permute.s
permute 321
accept
Z
EOF

#
#		P L O T
#
# NOTE : generates output file
cat > plot.mged_regress << EOF
autoview
ae 35 25 0
B clone4.c
plot mged_plot_test.pl
EOF

#
#                  P R E F I X
#

cat > prefix.mged_regress << EOF
make -s 23 prefix1.s sph
make -s 23 prefix2.s sph
prefix has_ prefix*
EOF

#
#		P S
#
# NOTE : generates output file
cat > ps.mged_regress << EOF
autoview
ae 35 25 0
B clone4.c
ps mged_ps_test.ps
EOF

#
#                  P U S H
#
cat > push.mged_regress << EOF
in push1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
in push2.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb push_l2.c u push1.s u push2.s
comb push_l1.c u push_l2.c
arced push_l1.c/push_l2.c matrix rarc rot 14 14 14
l push_l1.c
l push_l2.c
l push1.s
l push2.s
push push_l1.c
l push_l1.c
l push_l2.c
l push1.s
l push2.s
EOF

#
#                  P U T M A T
#
cat > putmat.mged_regress << EOF
in putmat1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
in putmat2.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb putmat.c u putmat1.s u putmat2.s
putmat putmat.c/putmat1.s {1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16}
putmat putmat.c/putmat2.s {1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16}
putmat putmat.c/putmat2.s {I}
l putmat.c
EOF

#
#                   Q O R O T
#
cat > qorot.mged_regress << EOF
in qorot.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb qorot.c u qorot.s
Z
e qorot.c
oed / qorot.c/qorot.s
qorot 2 2 3 10 2 3 40
qorot -2 3 1 20 20 20 40
accept
Z
EOF

#
#		Q V R O T
#
cat > qvrot.mged_regress << EOF
view quat
qvrot 4 -30 2 32
view quat
EOF

#
#                        R
#
# Test commands for r command to build regions - because region building
# acts on existing primitives and combinations, first set up primitives
# and combinations for testing.

cat > r.mged_regress << EOF
make -s 10 r_sph1.s sph
make -s 20 r_sph2.s sph
make -s 30 r_sph3.s sph
comb r_comb1.c u r_sph1.s u r_sph2.s u r_sph3.s
comb r_comb2.c u r_sph1.s + r_sph2.s + r_sph3.s
r r1.r u r_sph1.s u r_sph2.s u r_sph3.s
r r2.r u r_sph1.s u r_comb1.c
r r3.r u r_comb1.c u r_comb2.c
EOF

#
#               R E F R E S H
#
cat > refresh.mged_regress << EOF
refresh
EOF


#
#                  R E J E C T
#
cat > reject.mged_regress << EOF
in reject.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
Z
e reject.s
sed reject.s
l reject.s
tra 10 10 10
reject
l reject.s
Z
EOF

#
#                  R M
#
cat > rm.mged_regress << EOF
make rm_prim1.s sph
make rm_prim2.s sph
comb rm_comb1.c u rm_prim1.s
comb rm_comb2.c u rm_comb1.c u rm_prim2.s
rm rm_comb2.c rm_prim2.s
rm rm_comb2.c rm_prim1.s
EOF

#
#                 R O T
#
# Test rot command when editing a primitive
cat > rot_edit.mged_regress << EOF
in rot.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
Z
e rot.s
sed rot.s
rot 15 20 -12
accept
Z
EOF

#
#                 R O T
#
# Test rot command when editing a primitive
cat > rot_view.mged_regress << EOF
view quat
rot 30 30 30
view quat
EOF


#
#              R O T O B J
#
cat > rotobj.mged_regress << EOF
in rotobj.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb rotobj.c u rotobj.s
Z
e rotobj.c
oed / rotobj.c/rotobj.s
rotobj -m 10 10 10
rotobj -m 10 10 10
rotobj -v 10 10 10
rotobj -v 10 10 10
accept
Z
EOF

#
#		S A V E V I E W
#
#   NOTE - GENERATES OUTPUT FILE
cat > saveview.mged_regress << EOF
autoview
ae 35 25 0
rot 33 33 31
sca 4
saveview mged_saveview_test.rt
EOF


#
#                  S C A
#
# Test sca command when editing geometry

cat > sca_edit.mged_regress << EOF
in sca.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
Z
e sca.s
sed sca.s
sca 10
accept
Z
EOF

#
#                  S C A
#
# Test sca command when viewing geometry

cat > sca_view.mged_regress << EOF
size
sca 100
size
EOF


#
#                  S E D
#
cat > sed.mged_regress << EOF
make sed.s sph
e sed.s
status state
sed sed.s
status state
press accept
status state
d sed.s
EOF

#
#		S E T
#
# For the moment, testing the setting of perspective
# only (present on quick reference card).

cat > set_perspective.mged_regress << EOF
set perspective
set perspective 3.3
set perspective
set perspective -1
set perspective
EOF

#
#		S E T V I E W
#
cat > setview.mged_regress << EOF
view quat
setview 23 3 1
view quat
EOF

#
#		S I Z E
#
cat > size.mged_regress << EOF
size
size 122323
size
EOF

#
#                  S T A T U S
#
cat > status.mged_regress << EOF
status
status state
status Viewscale
status base2local
status local2base
status toViewcenter
status Viewrot
status model2view
status view2model
status model2objview
status objview2model
EOF

#
#		S V
#
cat > sv.mged_regress << EOF
view center
sv 76 19
view center
EOF

#
#                  T R A
#
# Test tra command when editing geometry

cat > tra_edit.mged_regress << EOF
in tra.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb tra.c u tra.s
Z
e tra.s
sed tra.s
tra 10 -2 4
accept
Z
e tra.c
oed / tra.c/tra.s
tra -3 3 3
accept
Z
EOF

#
#		T R A
#
# Test tra command when viewing geometry

cat > tra_view.mged_regress << EOF
view center
tra -1000 2 3
view center
EOF

#
# 		T R A N S L A T E
#
cat > translate.mged_regress << EOF
make translate1.s sph
make translate2.s eto
make translate3.s tgc
comb translate1.c u translate1.s
comb translate2.c u translate1.s u translate2.s
Z
e translate1.s
sed translate1.s
translate 100 100 100
accept
Z
e translate1.c
oed / translate1.c/translate1.s
translate 200 200 200
accept
Z
e translate2.c
oed / translate2.c/translate2.s
translate 300 300 300
accept
Z
e translate3.s
sed translate3.s
translate -100 -100 -100
accept
Z
EOF

#
#		U N H I D E
#
cat > unhide.mged_regress << EOF
in unhidden_1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
in unhidden_2.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb unhidden.c u unhidden_2.s
ls unhidden*
hide unhidden_1.s
hide unhidden.c
ls unhidden*
unhide unhidden_1.s
unhide unhidden.c
ls unhidden*
EOF

#
#            V I E W
#
cat > view.mged_regress << EOF
view quat 0.413175911167 0.492403876506 0.586824088833 0.492403876506
view ypr 10 1 2
view aet 10 10 10
view center 23 -2 0
view eye 2 4 5
view size 10000
view quat
view ypr
view aet
view center
view eye
view size
EOF

#
#		V R O T
#
cat > vrot.mged_regress << EOF
view quat
vrot 23 23 -1
view quat
EOF

#
#                  W H O
#
# Test who with combinations
cat > who.mged_regress << EOF
make who1.s sph
make who2.s sph
comb who.c u who1.s u who2.s
e who1.s
e who.c
who
d who.c
d who1.s
EOF

#
#            X P U S H
#
cat > xpush.mged_regress << EOF
in xpush.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
comb xpush1.c u xpush.s
comb xpush2.c u xpush.s
arced xpush1.c/xpush.s matrix rarc rot 14 14 14
arced xpush2.c/xpush.s matrix rarc rot 24 24 24
comb xpush_top.c u xpush1.c u xpush2.c
xpush xpush_top.c
EOF

#
#		Z
#
cat > Z.mged_regress << EOF
in Z_test1.s arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0
make Z_test2.s eto
comb Z_test.c u Z_test1.s u Z_test2.s
e Z_test1.s Z_test2.s Z_test.c
who
Z
who
EOF

#
#  	    Z O O M
#
cat > zoom.mged_regress << EOF
size
zoom 30
size
EOF

##################################################################
#
#   Test script assembly - assemble each of the individual
#   scripts into a final mged.mged_regress that makes sense
#
##################################################################

# Create empty file to hold commands - all other MGED commands will
# be appended to this file.
cat > mged.mged_regress << EOF
EOF

#
#      GEOMETRIC INPUT COMMANDS
#
cat in.mged_regress >> mged.mged_regress
cat make.mged_regress >> mged.mged_regress
cat 3ptarb.mged_regress >> mged.mged_regress
cat arb.mged_regress >> mged.mged_regress
cat comb.mged_regress >> mged.mged_regress
cat g.mged_regress >> mged.mged_regress
cat r.mged_regress >> mged.mged_regress
cat make_bb.mged_regress >> mged.mged_regress
cat cp.mged_regress >> mged.mged_regress
cat cpi.mged_regress >> mged.mged_regress
cat mv.mged_regress >> mged.mged_regress
cat mvall.mged_regress >> mged.mged_regress
cat clone.mged_regress >> mged.mged_regress
cat build_region.mged_regress >> mged.mged_regress
cat prefix.mged_regress >> mged.mged_regress
cat mirror.mged_regress >> mged.mged_regress

#
#     DISPLAYING GEOMETRY - COMMANDS
#
cat draw_erase_who.mged_regress >> mged.mged_regress
cat who.mged_regress >> mged.mged_regress
cat draw.mged_regress >> mged.mged_regress
cat erase.mged_regress >> mged.mged_regress
cat Z.mged_regress >> mged.mged_regress
cat B.mged_regress >> mged.mged_regress
cat dall.mged_regress >> mged.mged_regress
cat hide.mged_regress >> mged.mged_regress
cat unhide.mged_regress >> mged.mged_regress

#
#     EDITING COMMANDS
#
cat status.mged_regress >> mged.mged_regress
cat sed.mged_regress >> mged.mged_regress
cat oed.mged_regress >> mged.mged_regress
cat i.mged_regress >> mged.mged_regress
cat rm.mged_regress >> mged.mged_regress
cat keypoint.mged_regress >> mged.mged_regress
cat arced.mged_regress >> mged.mged_regress
cat copymat.mged_regress >> mged.mged_regress
cat putmat.mged_regress >> mged.mged_regress
cat push.mged_regress >> mged.mged_regress
cat xpush.mged_regress >> mged.mged_regress
cat accept.mged_regress >> mged.mged_regress
cat reject.mged_regress >> mged.mged_regress
cat tra_edit.mged_regress >> mged.mged_regress
cat facedef.mged_regress >> mged.mged_regress
cat mirface.mged_regress >> mged.mged_regress
cat permute.mged_regress >> mged.mged_regress
cat translate.mged_regress >> mged.mged_regress
cat sca_edit.mged_regress >> mged.mged_regress
cat oscale.mged_regress >> mged.mged_regress
cat extrude.mged_regress >> mged.mged_regress
cat rot_edit.mged_regress >> mged.mged_regress
cat orot.mged_regress >> mged.mged_regress
cat arot.mged_regress >> mged.mged_regress
cat rotobj.mged_regress >> mged.mged_regress
cat qorot.mged_regress >> mged.mged_regress

#
#	DELETING GEOMETRY COMMANDS
#
cat kill.mged_regress >> mged.mged_regress
cat killall.mged_regress >> mged.mged_regress
cat killtree.mged_regress >> mged.mged_regress

#
#    VIEW COMMANDS
#
cat view.mged_regress >> mged.mged_regress
cat refresh.mged_regress >> mged.mged_regress
#cat autoview.mged_regress >> mged.mged_regress
cat ae.mged_regress >> mged.mged_regress
cat center.mged_regress >> mged.mged_regress
cat eye_pt.mged_regress >> mged.mged_regress
cat lookat.mged_regress >> mged.mged_regress
cat size.mged_regress >> mged.mged_regress
cat zoom.mged_regress >> mged.mged_regress
cat set_perspective.mged_regress >> mged.mged_regress
cat tra_view.mged_regress >> mged.mged_regress
cat sca_view.mged_regress >> mged.mged_regress
cat rot_view.mged_regress >> mged.mged_regress
cat mrot.mged_regress >> mged.mged_regress
cat vrot.mged_regress >> mged.mged_regress
cat qvrot.mged_regress >> mged.mged_regress
cat setview.mged_regress >> mged.mged_regress
cat sv.mged_regress >> mged.mged_regress
cat orientation.mged_regress >> mged.mged_regress
cat knob.mged_regress >> mged.mged_regress
cat adc.mged_regress >> mged.mged_regress
#cat saveview.mged_regress >> mged.mged_regress
#cat loadview.mged_regress >> mged.mged_regress
#cat ps.mged_regress >> mged.mged_regress
#cat plot.mged_regress >> mged.mged_regress
#cat overlay.mged_regress >> mged.mged_regress


##################################################################
#
# Run master script to generate log and .g file for diffing
#
##################################################################

$MGED -c mged.g << EOF >> mged.log 2>&1
`cat mged.mged_regress`
EOF

# group generated files into a single file so they can be
# included as part of the diff process.
#cat mged_saveview_test.rt >> mged_files
#cat mged_ps_test.ps >> mged_files
#cat mged_plot_test.pl >> mged_files
#cat mged_overlay_test.ps >> mged_files

# clean up files

rm -f *.mged_regress mged_saveview_test.rt mged_ps_test.ps mged_plot_test.pl mged_overlay_test.ps

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
