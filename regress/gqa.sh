#!/bin/sh
rm -f gqa.g density_table.txt gqa.log

echo "5 1 stuff" > density_table.txt
echo "2 1 gas" >> density_table.txt

../src/mged/mged -c gqa.g <<EOF > /dev/null 2>&1

units m
dbbinary -i u c _DENSITIES density_table.txt

in box1.s rpp 0 10 0 10 0 10
in box2.s rpp 1  9 1  9 1  9
in box3.s rpp 1 10 1  9 1  9
in box4.s rpp 0.5  9.5 0.5  5 0.5  9.5

r solid_box.r u box1.s
adjust solid_box.r GIFTmater 5
mater solid_box.r "plastic tr=0.5 di=0.5 sp=0.5" 128 128 128 0

r closed_box.r u box1.s - box2.s
adjust closed_box.r GIFTmater 5
mater closed_box.r "plastic tr=0.5 di=0.5 sp=0.5" 128 128 128 0

r open_box.r u box1.s - box3.s
mater open_box.r "plastic tr=0.5 di=0.5 sp=0.5" 128 128 128 0


r exposed_air.r u box3.s
adjust exposed_air.r air 2
mater exposed_air.r  "plastic tr=0.8 di=0.1 sp=0.1" 255 255 128 0
g exposed_air.g exposed_air.r open_box.r

r adj_air1.r u box2.s + box4.s
r adj_air2.r u box2.s - box4.s

adjust adj_air1.r air 3
adjust adj_air1.r GIFTmater 2

adjust adj_air2.r air 4
adjust adj_air2.r GIFTmater 2

mater adj_air1.r  "plastic tr=0.5 di=0.1 sp=0.1" 255 128 128  0
mater adj_air2.r  "plastic tr=0.5 di=0.1 sp=0.1" 128 128 255 0

g adj_air.g closed_box.r adj_air1.r adj_air2.r

g gap.g closed_box.r adj_air2.r

r overlap_obj.r u box3.s
adjust overlap_obj.r GIFTMater 5
g overlaps closed_box.r overlap_obj.r


q
EOF

#
# now that the inputs have been built, run the tests
#
# box1.s = 10x10x10	= 1000 m^3
# box2.s = 8x8x8	=  512 m^3
# box3.s = 8x8x9	=  576 m^3
# adj_air1.r = 512-256	= 256 m^3
# adj_air2.r = 512-256	= 256 m^3
# closed_box.r =1000-512= 488 m^3
# exposed_air.r 	= 576 m^3
# open_box.r = 1000-576 = 424 m^3


GQA="../src/gtools/g_qa -u m,m^3,kg -g 0.25m-0.5mm -p"
export GQA

STATUS=0
export STATUS

#
# somehow need to check results of these
#
CMD="$GQA -Ao gqa.g overlaps"
echo $CMD
echo $CMD >> gqa.log
$CMD >> gqa.log 2>&1


echo >> gqa.log
CMD="$GQA -Ae gqa.g exposed_air.g"
echo $CMD
echo $CMD >> gqa.log 2>&1
$CMD >> gqa.log 2>&1

echo >> gqa.log
CMD="$GQA -Aa gqa.g adj_air.g"
echo $CMD
echo $CMD >> gqa.log 2>&1
$CMD >> gqa.log 2>&1

echo >> gqa.log
CMD="$GQA -Ag gqa.g gap.g"
echo $CMD
echo $CMD >> gqa.log 2>&1
$CMD >> gqa.log 2>&1

echo >> gqa.log
CMD="$GQA -Av -v gqa.g closed_box.r"
echo $CMD
echo $CMD >> gqa.log 2>&1
$CMD >> gqa.log 2>&1

echo >> gqa.log
CMD="$GQA -r -Aw -v gqa.g closed_box.r"
echo $CMD
echo $CMD >> gqa.log 2>&1
$CMD >> gqa.log 2>&1


echo >> gqa.log
CMD="$GQA -r -Avw gqa.g solid_box.r"
echo $CMD
echo $CMD >> gqa.log 2>&1
$CMD >> gqa.log 2>&1


echo >> gqa.log
CMD="$GQA -r -Avw gqa.g adj_air.g"
echo $CMD
echo $CMD >> gqa.log 2>&1
$CMD >> gqa.log 2>&1


echo >> gqa.log
CMD="$GQA -r -v -g 0.25m-25mm -Awo gqa.g closed_box.r"
echo $CMD
echo $CMD >> gqa.log 2>&1
$CMD >> gqa.log 2>&1


CMD="$GQA -g 50mm -Ao gqa.g closed_box.r"
echo $CMD
echo $CMD >> gqa.log 2>&1
$CMD >> gqa.log 2>&1


if [ $STATUS = 0 ] ; then
    /bin/echo '-> gqa.sh succeeded'
else
    /bin/echo '-> gqa.sh failed'
fi
