#!/bin/sh

#export PATH=~/brlcad.trunk/.build/bin:$PATH
#export DYLD_LIBRARY_PATH=~/brlcad.trunk/.build/lib
export LIBRT_BOT_MINTIE=1

DB="share/db/m35.g"

# NOTE: geometry is set below in main loop due to eye point coupling

GROUND="ground"
BASES="component cab.r"
EYES="eye1.r eye2.r"
OCCLUDERS="pole"

DIFFCOLOR="255 115 115"
SAMECOLOR="201 210 165"
NODIFFCOLOR="63  63  63" # the spaces are important

GRIDDB="share/db/grid.g"
GRIDOBJ="grid.r"
GRID="./grid110_1024.pix" # TODO: generate the grid image automatically


VIEW=110000 # original example is approx. 110m wide FOV
SZ=1024
OPTS="-s$SZ"

label () {
    x=$1
    y=$2
    str="$3"
    if test "x$4" = "x" ; then
        font="fix.6"
    else
        font="$4"
    fi

    fblabel -f ${font} -F 0 -C 0/0/0 `expr $x - 1` `expr $y - 1` "$str"
    fblabel -f ${font} -F 0 -C 0/0/0 `expr $x + 1` `expr $y - 1` "$str"
    fblabel -f ${font} -F 0 -C 0/0/0 `expr $x - 1` `expr $y + 1` "$str"
    fblabel -f ${font} -F 0 -C 0/0/0 `expr $x + 1` `expr $y + 1` "$str"
    fblabel -f ${font} -F 0 $x $y "$str"
}


fbserv -s1024 0 /dev/X &
sleep 2

# do me once
# mater large_male_eye.r "light {f 1000 s 8 v 0}" . . . .
# mater small_male_eye.r "light {f 1000 s 8 v 0}" . . . .
# mater small_female_eye.r "light {f 1000 s 8 v 0}" . . . .
# mater test_eye.r "light {f 1000 s 8 v 0}" . . . .
#mged -c "$DB" <<EOF
#kill ground.s
#in ground.s half 0 0 1 0
#mater ground.r . 66 134 0 .
#EOF

# GENERATE TOP-DOWN VIEW
########################

for base in $BASES ; do

    for eye in $EYES ; do

        for occluder in $OCCLUDERS ; do
            echo "Processing TOP-DOWN $base @ $eye against $occluder"

            rtpretty="viz.${base}.${eye}.${occluder}.pretty.rt"
            rtwithout="viz.${base}.${eye}.${occluder}.without.rt"
            rtwith="viz.${base}.${eye}.${occluder}.with.rt"
            rtbase="viz.${base}.${eye}.${occluder}.base.rt"
            if ! test -f ${rtpretty} -a -f ${rtwithout} -a -f ${rtwith} -a -f ${rtbase} ; then
                rm -f $rtpretty $rtwithout $rtwith $rtbase

                mged -c "$DB" 2>&1 <<EOF
draw $eye
draw $base $GROUND
ae 0 90 0
view size $VIEW
saveview $rtwithout -R
draw $occluder
saveview $rtwith -R
erase $eye
saveview $rtpretty -R -c \\\"set ambSamples=64 ambSlow=1\\\" -A1.25
erase $GROUND
saveview $rtbase -R -c \\\"set ambSamples=128 ambSlow=1\\\" -A1.25
q
EOF
            fi
        done #occluders
    done #eyes

    for eye in $EYES ; do
        for occluder in $OCCLUDERS ; do
            rtbase="viz.${base}.${eye}.${occluder}.base.rt"
            rtpretty="viz.${base}.${eye}.${occluder}.pretty.rt"
            rtwithout="viz.${base}.${eye}.${occluder}.without.rt"
            rtwith="viz.${base}.${eye}.${occluder}.with.rt"
            diff="viz.${base}.${eye}.${occluder}.diff"
            draft="viz.${base}.${eye}.${occluder}.draft"
            final="viz.${base}.${eye}.${occluder}.final"

            do_diff=0
            if ! test -f ${rtbase}.pix -a -f ${rtpretty}.pix -a -f ${rtwithout}.pix -a -f ${rtwith}.pix -a -f ${diff}.pix -a -f ${diff} ; then
                do_diff=1
            fi

            if ! test -f ${rtbase}.pix ; then
                echo "Rendering $base without ground..."
                ./$rtbase $OPTS
                rm -f ${rtbase}.png
                pix-png -s$SZ -o "${rtbase}.png" "${rtbase}.pix"
            fi
            # pix-fb -s$SZ -F0 ${rtbase}.pix &

            if ! test -f ${rtpretty}.pix ; then
                echo "Rendering $base + $occluder in position..."
                ./$rtpretty $OPTS
                rm -f ${rtpretty}.png
                pix-png -s$SZ -o "${rtpretty}.png" "${rtpretty}.pix"
            fi
            # pix-fb -s$SZ -F0 ${rtpretty}.pix &
            
            if ! test -f ${rtwithout}.pix ; then
                echo "Rendering $base shadows..."
                ./$rtwithout $OPTS -B
                rm -f ${rtwithout}.png
                pix-png -s$SZ -o "${rtwithout}.png" "${rtwithout}.pix"
            fi
            # pix-fb -s$SZ -F0 ${rtwithout}.pix &
        
            if ! test -f ${rtwith}.pix ; then
                echo "Rendering $base + $occluder shadows..."
                ./$rtwith $OPTS -B
                rm -f ${rtwith}.png
                pix-png -s$SZ -o "${rtwith}.png" "${rtwith}.pix"
            fi
            # pix-fb -s$SZ -F0 ${rtwith}.pix &
            
            if test "x$do_diff" = "x1" ; then
                rm -f ${diff}.pix ${diff}
                echo "Comparing $base with/without the occluder $occluder"
                pixdiff "${rtwithout}.pix" "${rtwith}.pix" > "${diff}.pix" 2>"$diff"
                rm -f ${diff}.png
                pix-png -s$SZ -o "${diff}.png" "${diff}.pix"
            fi
            # pix-fb -s$SZ -F0 ${diff}.pix &

            rm -f ${draft}.pix ${draft}.png
            pixfade -s1024 -f .5 ${rtpretty}.pix | pixmatte -g ${diff}.pix =50/50/50 ${diff}.pix - | pixsubst $NODIFFCOLOR $SAMECOLOR | pixsubst 255 255 255 $DIFFCOLOR | pixmerge -g $GRID - | pixmatte -g ${rtbase}.pix =0/0/1 ${rtbase}.pix - > ${draft}.pix

            pix-png -s$SZ -o "${draft}.png" "${draft}.pix"
            pix-fb -s$SZ -F0 "${draft}.pix"
            label 963 490 '50m'
            label 515 1005 '0 deg'
            label 950 515 '90 deg'
            label 515 7 '180 deg'
            label 5 515 '270 deg'
            label 10 25 "`basename $DB`" nonie.b.8 # fix.8
            label 10 50 "$base w/ $occluder" nonie.b.8

            # calculate the delta
            count="`pixcount < ${diff}.pix`"
            pxdiff=`echo "$count" | grep "255 255 255" | awk '{print $4}'`
            pxsame=`echo "$count" | grep "$NODIFFCOLOR" | awk '{print $4}'`
            totdc="10k $pxdiff $SZ $SZ * / 100 * p"
            occdc="10k $pxdiff $pxdiff $pxsame + / 100 * p"
            totval="`dc -e \"$totdc\"`"
            occval="`dc -e \"$occdc\"`"
            labeltot="`printf \"Overall scene impact: %.1lf%%\" $totval`"
            labelocc="`printf \"Visibility reduction: %.1lf%%\" $occval`"
            label 10 970 "$labelocc" nonie.b.8
            label 10 950 "$labeltot" nonie.b.8

            # save out the final image
            rm -f ${final}.pix ${final}.png
            fb-pix -F0 -s$SZ ${final}.pix
            pix-png -s$SZ -o "${final}.png" "${final}.pix"

        done #occluders
    done #eyes
done #bases

# clean up
rm -f *.rt.log

#fblabel -f nonie.r.8 -F 0 850 410 '40m'
#fblabel -f nonie.r.8 -F 0 735 365 '30m'
#fblabel -f nonie.r.8 -F 0 625 370 '20m'
#fblabel -f nonie.r.8 -F 0 540 420 '10m'


exit #!!!


# GENERATE PERSPECTIVE VIEW
###########################

for eye in $EYES ; do

    eye_pt=`mged -c "$DB" 2>&1 <<EOF
B $eye
center
EOF`

    for occluder in $OCCLUDERS ; do
        echo "Processing $eye on $base against $occluder"

        persp_w0="pviz.${base}.${eye}.${occluder}.with.00rt"
        persp_w1="pviz.${base}.${eye}.${occluder}.with.30.rt"
        persp_w2="pviz.${base}.${eye}.${occluder}.with.60.rt"
        persp_w3="pviz.${base}.${eye}.${occluder}.with.90.rt"
        persp_w4="pviz.${base}.${eye}.${occluder}.with.-30.rt"
        persp_w5="pviz.${base}.${eye}.${occluder}.with.-60.rt"
        persp_w6="pviz.${base}.${eye}.${occluder}.with.-90.rt"
        persp_wo0="pviz.${base}.${eye}.${occluder}.without.00.rt"
        persp_wo1="pviz.${base}.${eye}.${occluder}.without.30.rt"
        persp_wo2="pviz.${base}.${eye}.${occluder}.without.60.rt"
        persp_wo3="pviz.${base}.${eye}.${occluder}.without.90.rt"
        persp_wo4="pviz.${base}.${eye}.${occluder}.without.-30.rt"
        persp_wo5="pviz.${base}.${eye}.${occluder}.without.-60.rt"
        persp_wo6="pviz.${base}.${eye}.${occluder}.without.-90.rt"
        
        occluder_pt=`mged -c "$DB" 2>&1 <<EOF
B $occluder
center
EOF`

        mged -c "$DB" 2>&1 <<EOF
set perspective 70
draw $base
view size 1000
center $eye_pt
lookat $occluder
saveview $persp_wo0 -R
mrot 0 30 0
saveview $persp_wo1 -R
mrot 0 30 0
saveview $persp_wo2 -R
mrot 0 30 0
saveview $persp_wo3 -R
mrot 0 -120 0
saveview $persp_wo4 -R
mrot 0 -30 0
saveview $persp_wo5 -R
mrot 0 -30 0
saveview $persp_wo6 -R

draw $occluder
center $eye_pt
lookat $occluder
saveview $persp_w0 -R
mrot 0 30 0
saveview $persp_w1 -R
mrot 0 30 0
saveview $persp_w2 -R
mrot 0 30 0
saveview $persp_w3 -R
mrot 0 -120 0
saveview $persp_w4 -R
mrot 0 -30 0
saveview $persp_w5 -R
mrot 0 -30 0
saveview $persp_w6 -R
EOF
            
    done #occluders
done #eyes

for eye in $EYES ; do
    for occluder in $OCCLUDERS ; do
        echo "Rendering $eye on $base against $occluder"
    done #occluders
done #eyes

            
# rt -C135/206/235 -c "set ambSamples=64 ambSlow=1" -s1024 -A2 -R

