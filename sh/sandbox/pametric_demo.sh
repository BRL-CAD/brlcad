#!/bin/sh


if test $# -ne 2 ; then
    echo "Usage: $0 file.g object"
    exit 1
fi

db=$1
obj=$2

if ! test -f "$db" ; then
    echo "ERROR: $db doesn't exist"
    echo "Usage: $0 file.g object"
    exit 2
fi

BASE="`basename $db`_${obj}"

DIR="pametric_demo/${BASE}"
if ! test -d "$DIR" ; then
    echo "Creating $DIR"
    mkdir -p "$DIR"
else
    echo "$DIR already exists"
fi
rm -f "$DIR/*.png" "$DIR/*.pix" "$DIR/*.pdf"


TOTAL_EASY=0
TOTAL_HARD=0
TOTAL=0

render ( ) {
    db="$1"
    obj="$2"
    az="$3"
    el="$4"
    
    pix="${DIR}/${BASE}.$az-$el.rte.pix"
    png="$pix.png"
    log="$pix.log"
    rm -f "$pix" "$png" "$log"

    echo "========="
    echo "Rendering $db:$obj @ azel $az/$el..."

    # gz=25.4 # 1.0 inch
    gz=12.7 # 0.5 inch
    # gz=2.54 # 0.1 inch
    
    # render rtedge
    echo "rtedge -R -W -a $az -e $el -g$gz -o \"$pix\" \"$db\" \"$obj\" > \"$log\" 2>&1"
    rtedge -R -W -a $az -e $el -g$gz -o "$pix" "$db" "$obj" > "$log" 2>&1

    sz="`grep Grid: $log | awk '{print $5,$6}' | tr -d ',()'`"
    width="`echo $sz | cut -d' ' -f1`"
    height="`echo $sz | cut -d' ' -f2`"

    pix-png -w $width -n $height "$pix" > "$png"

    orient="`grep Orientation: $log | awk '{print $2,$3,$4,$5}' | sed 's/, /\//g'`"
    # grep Eye: "$log"
    eye="`grep Eye_pos: $log | awk '{print $2,$3,$4}' | sed 's/ //g'`"
    vsize="`grep Size: $log | awk '{print $2}' | tr -d '[a-zA-Z]'`"

    rtwpix="${DIR}/${BASE}.$az-$el.rtw.pix"
    rtwpng="$rtwpix.png"
    rtwlog="$rtwpix.log"
    rm -f "$rtwpix" "$rtwpng" "$rtwlog"

    # render rtwizard
    # --viewsize `echo 2k $vsize 1.5 / p | dc`
    echo "rtwizard -i \"$db\" -o \"$rtwpix\" -w $width -n $height -g \"$obj\" -l \"$obj\" -G 1 --viewsize $vsize --orientation $orient --eye_pt $eye --line-color 255/0/0 -C 0/0/255 > $rtwlog 2>&1"
    rtwizard -i "$db" -o "$rtwpix" -w $width -n $height -g "$obj" -l "$obj" -G 1 --viewsize $vsize --orientation $orient --eye_pt $eye --line-color 255/0/0 -C 0/0/255 > $rtwlog 2>&1
    # rtwizard -i "$db" -o "$rtwpix" -s 1024 -g "$obj" -l "$obj" -G 1 --viewsize $vsize --orientation $orient --eye_pt $eye --line-color 255/0/0 -C 0/0/255 > $rtwlog 2>&1

    pix-png -w $width -n $height "$rtwpix" > "$rtwpng"

    # tally up the counts
    cnts="`pixcount $rtwpix`"
    bgpx=`echo "$cnts" | grep "  0   0 255  " | awk '{print $4}'`
    edpx=`echo "$cnts" | grep "255   0   0  " | awk '{print $4}'`
    arpx=`echo "$cnts" | grep -v "^  0   0 255  " | grep -v "^255   0   0  " | awk '{print $4}' | awk '{sum+=$1} END {print sum}'`
    area=`echo "$edpx $arpx + p" | dc`

    # echo "bgpx = $bgpx"
    # echo "edpx = $edpx"
    # echo "arpx = $arpx"
    # echo "area = $area"

    rtalog="$rtwpix.rtarea.log"
    # echo rtarea -w $width -n $height -R -W -a $az -e $el -g$gz "$db" "$obj"
    rtarea -w $width -n $height -R -W -a $az -e $el -g$gz "$db" "$obj" > "$rtalog" 2>&1
    actual="`grep Total $rtalog | awk '{print $7}'`"

    hard=`echo 10k $edpx $gz 25.4 \* \* p | dc`
    easy=`echo 10k $arpx $gz $gz \* \* p | dc`
    # what's not hard is easy
    if awk "BEGIN {exit !($actual > $hard)}" ; then
        easy=`echo 10k $actual $hard - p | dc`
    else
        easy=0
    fi
    echo "Hard area: $hard"
    echo "Easy area: $easy"
    echo "Total area: $actual"

    TOTAL_HARD="`echo 10k $TOTAL_HARD $hard + p | dc`"
    TOTAL_EASY="`echo 10k $TOTAL_EASY $easy + p | dc`"
    TOTAL="`echo 10k $TOTAL $actual + p | dc`"

    # label our images
    rtwann="$rtwpng.ann.png"
    # convert "$rtwpng" -gravity South -fill white  -undercolor '#00000080' -pointsize 60 -annotate +100+100 'Hard area: $hard\nEasy area: $easy\nTotal area: $actual' "$rtwann"

}


for az in 0 90 180 270 ; do
    render "$db" "$obj" "$az" 0
done
render "$db" "$obj" 0 90
render "$db" "$obj" 35 25

rtw="${DIR}/${BASE}.35-25.rtw.pix"
rm -f $rtw

#rtwizard -i "$db" -o "$rtw" -s 1024 -g "$obj" -l "$obj" -G 1 -a 35 -e 25 --line-color 255/0/0 -C 0/0/255
#rtwpng="$rtw.png"
#pix-png -s1024 $rtw > $rtwpng


echo "Summary"
echo "-------"
hard="`echo 5k $TOTAL_HARD 92903 / p | dc`"
easy="`echo 5k $TOTAL_EASY 92903 / p | dc`"
area="`echo 5k $TOTAL 92903 / p | dc`"
printf "Hard: %.1f ft^2\n" $hard
printf "Easy: %.1f ft^2\n" $easy
printf "Area: %.1f ft^2\n" $area
echo ""

# how much longer does it take to clean edges
factor=2
hard_factored="`echo 10k $TOTAL_HARD $factor \* p | dc`"
echo "Hard factored: $hard_factored"
hard_pct="`echo 10k $TOTAL_HARD $TOTAL / 100 \* p | dc`"
printf "Hard percent: %.1f %%\n" $hard_pct

# let's say it's 1 minute per square foot.
# assumes we're misrepresenting surface area with presented area by about 10x.
sqmm_to_sqft=929030

# total time estimate = ((hard_area * factor) + easy_area) / cleaning_rate
# total time estimate = ((hard_area * 2) + easy_area) / 60
difficulty_est="`echo 10k $hard_factored $TOTAL_EASY + $sqmm_to_sqft / p | dc`"
printf "Difficulty metric: %.1f\n" $difficulty_est
time_est="`echo 2k $difficulty_est 60 / p | dc`"
echo "Time: $time_est hour(s)"

out="${BASE}_montage.png"
outpre="$out.pre.png"
rm -f $out $outpre

echo "montage -geometry 1024x1024 \"${DIR}/${BASE}.*rtw*.png\" $outpre"
montage -geometry 1024x1024 "${DIR}/${BASE}.*rtw*.png" $outpre

metric=`printf %.0f $difficulty_est`
percent=`printf %.1f $hard_pct`
echo "convert -pointsize 80 -undercolor '#00000080' -fill white -gravity center -annotate +00+50 \"\\ \\ Summary: $metric complexity metric ($percent\\% hard)  \" -annotate +00+140 \"  Cleanup Estimate: $time_est hours  \" $outpre -gravity south $out"
convert -pointsize 80 -undercolor '#00000080' -fill white -gravity center -annotate +00+50 "\ \ Summary: $metric complexity metric ($percent\% hard)  " -annotate +00+140 "  Cleanup Estimate: $time_est hours  " $outpre -gravity south $out

