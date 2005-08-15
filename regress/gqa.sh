#!/bin/sh
rm -f gqa.g density_table.txt gqa.log
echo "5 7.68 stuff" > density_table.txt
echo "2 1 gas" >> density_table.txt


# box1.s = 10x10x10	= 1000 m^3
# box2.s = 8x8x8	=  512 m^3
# box3.s = 8x8x9	=  576 m^3
# adj_air1.r = 512-256	= 256 m^3
# adj_air2.r = 512-256	= 256 m^3
# closed_box.r =1000-512= 488 m^3
# exposed_air.r 	= 576 m^3
# open_box.r = 1000-576 = 424 m^3



mged -c gqa.g <<EOF > /dev/null 2>&1

units m
dbbinary -i u c _DENSITIES density_table.txt

in box1.s rpp 0 10 0 10 0 10
in box2.s rpp 1  9 1  9 1  9
in box3.s rpp 1 10 1  9 1  9
in box4.s rpp 0.5  9.5 0.5  5 0.5  9.5

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
g overlaps closed_box.r overlap_obj.r


q
EOF

GQA="../src/gtools/.libs/g_qa -u m,m^3,kg"
export GQA

rm -f gqa.log

echo $GQA -Ao gqa.g overlaps
$GQA -Ao gqa.g overlaps >> gqa.log 2>&1


echo >> gqa.log
echo $GQA -Ae gqa.g exposed_air.g
$GQA -Ae gqa.g exposed_air.g >> gqa.log 2>&1

echo >> gqa.log
echo $GQA -Ag -r gqa.g gap.g
$GQA -Ag gqa.g gap.g >> gqa.log 2>&1

echo >> gqa.log
echo $GQA -Av -r gqa.g closed_box.r
$GQA -Av gqa.g closed_box.r >> gqa.log 2>&1

echo >> gqa.log
echo $GQA -Aw -r gqa.g closed_box.r
$GQA -Aw gqa.g closed_box.r >> gqa.log 2>&1


echo >> gqa.log
echo $GQA -Avw gqa.g adj_air.g
$GQA -Avw gqa.g adj_air.g >> gqa.log 2>&1

