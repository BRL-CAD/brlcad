#!/bin/sh
#                       N I R T . S H
# BRL-CAD
#
# Copyright (c) 2012-2020 United States Government as represented by
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
. "$1/regress/library.sh"

if test "x$LOGFILE" = "x" ; then
    LOGFILE=`pwd`/nirt.log
    rm -f $LOGFILE
fi
log "=== TESTING nirt ==="

MGED="`ensearch mged`"
if test ! -f "$MGED" ; then
    echo "Unable to find mged, aborting"
    exit 1
fi
NIRT="`ensearch nirt`"
if test ! -f "$NIRT" ; then
    echo "Unable to find nirt, aborting"
    exit 1
fi

rm -f nirt.mged nirt.g
cat > nirt.mged <<EOF
opendb nirt.g y
title "NIRT example database"
in center_box.s rpp -1 1 -1 1 -1 1
in left_box.s rpp -3 -1 -1 1 -1 1
in right_box.s rpp 1 3 -1 1 -1 1
r center_cube.r u center_box.s
r center_overlap.r u center_box.s
r left_cube.r u left_box.s
r right_cube.r u right_box.s
r left_and_right_cubes.r u right_box.s u left_box.s
r all_cubes.r u left_box.s u center_box.s u right_box.s
comb overlap_example u center_overlap.r u all_cubes.r
r left_cube_color.r u left_box.s
r center_cube_color.r u center_box.s
r right_cube_color.r u right_box.s
attr set left_cube_color.r color 255/0/0
attr set center_cube_color.r color 0/255/0
attr set right_cube_color.r color 0/0/255
comb center_cube_air u center_box.s
attr set center_cube_air region 1 air 1
EOF

$MGED -c >> $LOGFILE 2>&1 << EOF
`cat nirt.mged`
EOF

log "Running nirt tests, saving output to nirt.out"

rm -f nirt.out.raw
logfile_saved="$LOGFILE"
LOGFILE=`pwd`/nirt.out.raw


log "*** Test 1 - shot command ***"
run $NIRT -v -H 0 -e "s;q" nirt.g center_cube.r
log "*** Test 2 - xyz command ***"
run $NIRT -v -H 0 -e "xyz;xyz 0 0 .5;s;q" nirt.g center_cube.r
log "*** Test 3 - backout command ***"
run $NIRT -v -H 0 -e "s;backout 1;s;q" nirt.g left_and_right_cubes.r
log "*** Test 4 - backout/xyz interaction ***"
run $NIRT -v -H 0 -e "backout 0;xyz;xyz 0 0 .5;s;backout 1;xyz;backout 0;xyz;backout 1;xyz 0 0 .8;s;backout 0;s;q" nirt.g left_and_right_cubes.r
log "*** Test 5 - dir command***"
run $NIRT -v -H 0 -e "xyz 0 0 0;dir;s;dir -1 -.5 0;dir;s;dir 0 0 1;s;q" nirt.g left_and_right_cubes.r
log "*** Test 6 - reporting of overlaps ***"
run $NIRT -v -H 0 -e "backout 1;s;dir 0 0 -1;s;q" nirt.g overlap_example
log "*** Test 7 - output formatting ***"
run $NIRT -v -H 0 -b -f csv -e "s;q" nirt.g left_cube.r center_cube.r right_cube.r
run $NIRT -v -H 0 -b -f csv-gap -e "s;q" nirt.g left_cube.r center_cube.r right_cube.r
run $NIRT -v -H 0 -b -f default -e "s;q" nirt.g left_cube.r center_cube.r right_cube.r
run $NIRT -v -H 0 -b -f entryexit -e "s;q" nirt.g left_cube.r center_cube.r right_cube.r
run $NIRT -v -H 0 -b -f gap1 -e "s;q" nirt.g left_cube.r center_cube.r right_cube.r
run $NIRT -v -H 0 -b -f gap2 -e "s;q" nirt.g left_cube.r center_cube.r right_cube.r
log "*** Test 8 - attribute reporting ***"
run $NIRT -v -H 0 -b -e "attr -p; attr rgb; attr -p;s;attr -f;attr -p; attr rgb region;attr -p;s;q" nirt.g left_cube_color.r center_cube_color.r right_cube_color.r
run $NIRT -v -H 0 -b -A rgb -e "attr -p;s;q" nirt.g left_cube_color.r center_cube_color.r right_cube_color.r
log "*** Test 9 - units ***"
run $NIRT -v -H 0 -b -e "units;s;units m;s;units in;s;units ft;s;q" nirt.g center_cube.r
log "*** Test 10 - air regions ***"
run $NIRT -v -H 0 -b -e "s;q" nirt.g left_cube.r center_cube_air
run $NIRT -v -H 0 -b -u 0 -e "s;q" nirt.g left_cube.r center_cube_air
run $NIRT -v -H 0 -b -u 1 -e "s;q" nirt.g left_cube.r center_cube_air
log "*** Test 11 - parsing with spaces before semicolon, Patch #314 ***"
run $NIRT -v -H 0 -e "xyz 0.5 1.5 0.5; dir 0 -1 0; s; q" nirt.g center_cube.r
run $NIRT -v -H 0 -e "xyz 0.5 1.5 0.5 ; dir 0 -1 0; s; q;" nirt.g center_cube.r

# restore for final printing
LOGFILE="$logfile_saved"

# scrub output so we're not sensitive to things we don't care about
# like the path to nirt.
rm -f nirt.out
log "substituting $NIRT with nirt"
sed "/ running / s|$NIRT|nirt|g" < nirt.out.raw > nirt.out
if test -f nirt.out ; then
    rm -f nirt.out.raw
fi

rm -f nirt.ref
cat >> nirt.ref <<EOF
*** Test 1 - shot command ***
... running nirt -v -H 0 -e s;q nirt.g center_cube.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Object 'center_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-1, -1, -1)    model_max = (1, 1, 1)
Origin (x y z) = (0.00000000 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_cube.r        (   1.0000    0.0000    0.0000)   2.0000   0.0000 
Quitting...
*** Test 2 - xyz command ***
... running nirt -v -H 0 -e xyz;xyz 0 0 .5;s;q nirt.g center_cube.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Object 'center_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-1, -1, -1)    model_max = (1, 1, 1)
(x, y, z) = (0.00, 0.00, 0.00)
Origin (x y z) = (0.00000000 0.00000000 0.50000000)  (h v d) = (0.0000 0.5000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_cube.r        (   1.0000    0.0000    0.5000)   2.0000   0.0000 
Quitting...
*** Test 3 - backout command ***
... running nirt -v -H 0 -e s;backout 1;s;q nirt.g left_and_right_cubes.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Object 'left_and_right_cubes.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
Origin (x y z) = (0.00000000 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
left_and_right_cubes.r (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
Origin (x y z) = (6.63324958 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
left_and_right_cubes.r (   3.0000    0.0000    0.0000)   2.0000   0.0000 
left_and_right_cubes.r (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
Quitting...
*** Test 4 - backout/xyz interaction ***
... running nirt -v -H 0 -e backout 0;xyz;xyz 0 0 .5;s;backout 1;xyz;backout 0;xyz;backout 1;xyz 0 0 .8;s;backout 0;s;q nirt.g left_and_right_cubes.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Object 'left_and_right_cubes.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
(x, y, z) = (0.00, 0.00, 0.00)
Origin (x y z) = (0.00000000 0.00000000 0.50000000)  (h v d) = (0.0000 0.5000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
left_and_right_cubes.r (  -1.0000    0.0000    0.5000)   2.0000   0.0000 
(x, y, z) = (0.00, 0.00, 0.50)
(x, y, z) = (0.00, 0.00, 0.50)
Origin (x y z) = (6.63324958 0.00000000 0.80000000)  (h v d) = (0.0000 0.8000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
left_and_right_cubes.r (   3.0000    0.0000    0.8000)   2.0000   0.0000 
left_and_right_cubes.r (  -1.0000    0.0000    0.8000)   2.0000   0.0000 
Origin (x y z) = (0.00000000 0.00000000 0.80000000)  (h v d) = (0.0000 0.8000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
left_and_right_cubes.r (  -1.0000    0.0000    0.8000)   2.0000   0.0000 
Quitting...
*** Test 5 - dir command***
... running nirt -v -H 0 -e xyz 0 0 0;dir;s;dir -1 -.5 0;dir;s;dir 0 0 1;s;q nirt.g left_and_right_cubes.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Object 'left_and_right_cubes.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
(x, y, z) = (-1.00, 0.00, 0.00)
Origin (x y z) = (0.00000000 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
left_and_right_cubes.r (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
(x, y, z) = (-0.89, -0.45, 0.00)
Origin (x y z) = (0.00000000 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-0.89442719 -0.44721360 0.00000000)  (az el) = (26.56505118 -0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
left_and_right_cubes.r (  -1.0000   -0.5000    0.0000)   1.1180  26.5651 
Origin (x y z) = (0.00000000 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (0.00000000 0.00000000 1.00000000)  (az el) = (0.00000000 -90.00000000)
You missed the target
Quitting...
*** Test 6 - reporting of overlaps ***
... running nirt -v -H 0 -e backout 1;s;dir 0 0 -1;s;q nirt.g overlap_example
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Object 'overlap_example' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
Origin (x y z) = (6.63324958 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
all_cubes.r          (   3.0000    0.0000    0.0000)   2.0000   0.0000 
OVERLAP: 'all_cubes.r' and 'center_overlap.r' xyz_in=(1 0 0) los=2
center_overlap.r     (   1.0000    0.0000    0.0000)   2.0000   0.0000 
all_cubes.r          (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
Origin (x y z) = (0.00000000 0.00000000 6.63324958)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (0.00000000 0.00000000 -1.00000000)  (az el) = (0.00000000 90.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_overlap.r     (   0.0000    0.0000    1.0000)   2.0000   0.0000 
OVERLAP: 'all_cubes.r' and 'center_overlap.r' xyz_in=(0 0 1) los=2
Quitting...
*** Test 7 - output formatting ***
... running nirt -v -H 0 -b -f csv -e s;q nirt.g left_cube.r center_cube.r right_cube.r
Output format: csv (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube.r' 'center_cube.r' 'right_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
Ray:
x_orig,y_orig,z_orig,d_orig,h,v,x_dir,y_dir,z_dir,az,el
6.63324958,0.00000000,0.00000000,0.00000000,0.00000000,0.00000000,-1.00000000,0.00000000,0.00000000,0.00000000,0.00000000 

Results:
reg_name,path_name,reg_id,x_in,y_in,z_in,d_in,x_out,y_out,z_out,d_out,los,scaled_los,obliq_in,obliq_out,surf_num_in,surf_num_out
"right_cube.r","/right_cube.r",1003,3.000000,0.000000,0.000000,3.000000,1.000000,0.000000,0.000000,1.000000,2.000000,2.000000,0.000000,0.000000,0,1
"center_cube.r","/center_cube.r",1000,1.000000,0.000000,0.000000,1.000000,-1.000000,0.000000,0.000000,-1.000000,2.000000,2.000000,0.000000,0.000000,0,1
"left_cube.r","/left_cube.r",1002,-1.000000,0.000000,0.000000,-1.000000,-3.000000,0.000000,0.000000,-3.000000,2.000000,2.000000,0.000000,0.000000,0,1

Quitting...
... running nirt -v -H 0 -b -f csv-gap -e s;q nirt.g left_cube.r center_cube.r right_cube.r
Output format: csv-gap (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube.r' 'center_cube.r' 'right_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
Ray:
x_orig,y_orig,z_orig,d_orig,h,v,x_dir,y_dir,z_dir,az,el
6.63324958,0.00000000,0.00000000,0.00000000,0.00000000,0.00000000,-1.00000000,0.00000000,0.00000000,0.00000000,0.00000000 

Results:
reg_name,path_name,reg_id,x_in,y_in,z_in,d_in,x_out,y_out,z_out,d_out,los,scaled_los,obliq_in,obliq_out,surf_num_in,surf_num_out
"right_cube.r","/right_cube.r",1003,3.000000,0.000000,0.000000,3.000000,1.000000,0.000000,0.000000,1.000000,2.000000,2.000000,0.000000,0.000000,0,1
"center_cube.r","/center_cube.r",1000,1.000000,0.000000,0.000000,1.000000,-1.000000,0.000000,0.000000,-1.000000,2.000000,2.000000,0.000000,0.000000,0,1
"left_cube.r","/left_cube.r",1002,-1.000000,0.000000,0.000000,-1.000000,-3.000000,0.000000,0.000000,-3.000000,2.000000,2.000000,0.000000,0.000000,0,1

Quitting...
... running nirt -v -H 0 -b -f default -e s;q nirt.g left_cube.r center_cube.r right_cube.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube.r' 'center_cube.r' 'right_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
Origin (x y z) = (6.63324958 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
right_cube.r         (   3.0000    0.0000    0.0000)   2.0000   0.0000 
center_cube.r        (   1.0000    0.0000    0.0000)   2.0000   0.0000 
left_cube.r          (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
Quitting...
... running nirt -v -H 0 -b -f entryexit -e s;q nirt.g left_cube.r center_cube.r right_cube.r
Output format: entryexit (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube.r' 'center_cube.r' 'right_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
Origin (x y z) = (6.63324958 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)                   Exit (x y z)             Obliq_in Attrib
right_cube.r         (   3.0000    0.0000    0.0000) (   1.0000    0.0000    0.0000)   0.0000 
center_cube.r        (   1.0000    0.0000    0.0000) (  -1.0000    0.0000    0.0000)   0.0000 
left_cube.r          (  -1.0000    0.0000    0.0000) (  -3.0000    0.0000    0.0000)   0.0000 
Quitting...
... running nirt -v -H 0 -b -f gap1 -e s;q nirt.g left_cube.r center_cube.r right_cube.r
Output format: gap1 (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube.r' 'center_cube.r' 'right_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
Origin (x y z) = (6.63324958 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
right_cube.r         (   3.0000    0.0000    0.0000)   2.0000   0.0000 
center_cube.r        (   1.0000    0.0000    0.0000)   2.0000   0.0000 
left_cube.r          (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
Quitting...
... running nirt -v -H 0 -b -f gap2 -e s;q nirt.g left_cube.r center_cube.r right_cube.r
Output format: gap2 (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube.r' 'center_cube.r' 'right_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
Origin (x y z) = (6.63324958 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
right_cube.r         (   3.0000    0.0000    0.0000)   2.0000   0.0000 
center_cube.r        (   1.0000    0.0000    0.0000)   2.0000   0.0000 
left_cube.r          (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
Quitting...
*** Test 8 - attribute reporting ***
... running nirt -v -H 0 -b -e attr -p; attr rgb; attr -p;s;attr -f;attr -p; attr rgb region;attr -p;s;q nirt.g left_cube_color.r center_cube_color.r right_cube_color.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube_color.r' 'center_cube_color.r' 'right_cube_color.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
"color"
Get trees...
Prepping the geometry...
Objects 'left_cube_color.r' 'center_cube_color.r' 'right_cube_color.r' processed
Origin (x y z) = (6.63324958 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
right_cube_color.r   (   3.0000    0.0000    0.0000)   2.0000   0.0000 color=0/0/255 
center_cube_color.r  (   1.0000    0.0000    0.0000)   2.0000   0.0000 color=0/255/0 
left_cube_color.r    (  -1.0000    0.0000    0.0000)   2.0000   0.0000 color=255/0/0 
"color"
"region"
Get trees...
Prepping the geometry...
Objects 'left_cube_color.r' 'center_cube_color.r' 'right_cube_color.r' processed
Origin (x y z) = (6.63324958 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
right_cube_color.r   (   3.0000    0.0000    0.0000)   2.0000   0.0000 color=0/0/255 region=R 
center_cube_color.r  (   1.0000    0.0000    0.0000)   2.0000   0.0000 color=0/255/0 region=R 
left_cube_color.r    (  -1.0000    0.0000    0.0000)   2.0000   0.0000 color=255/0/0 region=R 
Quitting...
... running nirt -v -H 0 -b -A rgb -e attr -p;s;q nirt.g left_cube_color.r center_cube_color.r right_cube_color.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube_color.r' 'center_cube_color.r' 'right_cube_color.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (3, 1, 1)
"color"
Origin (x y z) = (6.63324958 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
right_cube_color.r   (   3.0000    0.0000    0.0000)   2.0000   0.0000 color=0/0/255 
center_cube_color.r  (   1.0000    0.0000    0.0000)   2.0000   0.0000 color=0/255/0 
left_cube_color.r    (  -1.0000    0.0000    0.0000)   2.0000   0.0000 color=255/0/0 
Quitting...
*** Test 9 - units ***
... running nirt -v -H 0 -b -e units;s;units m;s;units in;s;units ft;s;q nirt.g center_cube.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Object 'center_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-1, -1, -1)    model_max = (1, 1, 1)
units = 'mm'
Origin (x y z) = (3.46410162 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_cube.r        (   1.0000    0.0000    0.0000)   2.0000   0.0000 
Origin (x y z) = (0.00346410 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_cube.r        (   0.0010    0.0000    0.0000)   0.0020   0.0000 
Origin (x y z) = (0.13638195 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_cube.r        (   0.0394    0.0000    0.0000)   0.0787   0.0000 
Origin (x y z) = (0.01136516 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_cube.r        (   0.0033    0.0000    0.0000)   0.0066   0.0000 
Quitting...
*** Test 10 - air regions ***
... running nirt -v -H 0 -b -e s;q nirt.g left_cube.r center_cube_air
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube.r' 'center_cube_air' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (-1, 1, 1)
Origin (x y z) = (1.46410162 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
left_cube.r          (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
Quitting...
... running nirt -v -H 0 -b -u 0 -e s;q nirt.g left_cube.r center_cube_air
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube.r' 'center_cube_air' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (-1, 1, 1)
Origin (x y z) = (1.46410162 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
left_cube.r          (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
Quitting...
... running nirt -v -H 0 -b -u 1 -e s;q nirt.g left_cube.r center_cube_air
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Objects 'left_cube.r' 'center_cube_air' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-3, -1, -1)    model_max = (1, 1, 1)
Origin (x y z) = (3.89897949 0.00000000 0.00000000)  (h v d) = (0.0000 0.0000 0.0000)
Direction (x y z) = (-1.00000000 0.00000000 0.00000000)  (az el) = (0.00000000 0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_cube_air      (   1.0000    0.0000    0.0000)   2.0000   0.0000 
left_cube.r          (  -1.0000    0.0000    0.0000)   2.0000   0.0000 
Quitting...
*** Test 11 - parsing with spaces before semicolon, Patch #314 ***
... running nirt -v -H 0 -e xyz 0.5 1.5 0.5; dir 0 -1 0; s; q nirt.g center_cube.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Object 'center_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-1, -1, -1)    model_max = (1, 1, 1)
Origin (x y z) = (0.50000000 1.50000000 0.50000000)  (h v d) = (1.5000 0.5000 0.5000)
Direction (x y z) = (0.00000000 -1.00000000 0.00000000)  (az el) = (90.00000000 -0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_cube.r        (   0.5000    1.0000    0.5000)   2.0000   0.0000 
Quitting...
... running nirt -v -H 0 -e xyz 0.5 1.5 0.5 ; dir 0 -1 0; s; q; nirt.g center_cube.r
Output format: default (specify -L option for descriptive listing)
Formats available: csv-gap csv default diff entryexit gap1 gap2 (specify via -f option)
Database file:  'nirt.g'
Building the directory...
Get trees...
Prepping the geometry...
Object 'center_cube.r' processed
Database title: 'NIRT example database'
Database units: 'mm'
model_min = (-1, -1, -1)    model_max = (1, 1, 1)
Origin (x y z) = (0.50000000 1.50000000 0.50000000)  (h v d) = (1.5000 0.5000 0.5000)
Direction (x y z) = (0.00000000 -1.00000000 0.00000000)  (az el) = (90.00000000 -0.00000000)
    Region Name               Entry (x y z)              LOS  Obliq_in Attrib
center_cube.r        (   0.5000    1.0000    0.5000)   2.0000   0.0000 
Quitting...
EOF

run cmp nirt.ref nirt.out
STATUS=$?

if [ X$STATUS = X0 ] ; then
    log "-> nirt.sh succeeded"
else
    log "-> nirt.sh FAILED, see $LOGFILE"
    cat "$LOGFILE"
fi

exit $STATUS

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8

