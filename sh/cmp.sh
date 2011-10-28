#!/bin/sh
#                          C M P . S H
# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
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
# This script compares two geometry objects in a .g file.
#
# This script reports a variety of analytic comparison metrics
# including:
#
#  - preparation and raytrace performance,
#  - optical (rt) accuracy,
#  - quantized thickness (rtxray) accuracy,
#  - contour (rtedge) accuracy,
#  - projected area (rtarea) accuracy,
#  - volumetric (gqa) accuracy, and more.
#
# Performance values include raw RTFM values but are more usefully
# also reported with values normalized to the number of foreground
# pixels in the base object.
#
# NOTE: This script assumes a variety of command-line tools sometimes
# common to BSD-style platforms such as printf, chmod, sed, awk, grep,
# tee, tail, and dc.  This script also, of course, assumes a variety
# of BRL-CAD tools are available in the current run-time PATH.
###

if test $# -ne 3 ; then
    echo "Usage: $0 file.g base.obj new.obj"
fi

SZ="1024"
HP="0"
GQTOL="-g10m, .1m"
dbfile="$1"

printf "\nStarted at `date`\n"
echo "Using -s$SZ -H$HP"

if ! test -f "$dbfile" ; then
    echo "ERROR: unable to find geometry database [$dbfile]"
    exit 1
fi

i="$3"
base="$2"
printf "\n=== $i ===\n"

if ! test -r "$base.base.rt" ; then
    mged -c "$dbfile" "e $base ; ae 35 25 ; zoom 1.25 ; saveview -e ./run.me -l /dev/stdout $base.base.rt"
    if ! test -r "$base.base.rt" ; then
        printf "ERROR: couldn't saveview from $dbfile to $base.base.rt\n"
        continue
    fi
fi

if ! test -f "run.me" ; then
    cat > run.me <<EOF
#!/bin/sh
\$RT \$*
EOF
fi
if ! test -x "run.me" ; then
    chmod ug+x "run.me"
fi

cat $base.base.rt | sed "s/\.base\././g" | sed "s/'$base'/'$i'/g" | sed "s/-o $base.rt.pix/-o $i.rt.pix/g" > $i.rt
export RT=rt

  ################
echo -n "Perf: "
export LIBRT_BOT_MINTIE=0
unset LIBRT_BOT_MINTIE
rm -f "$i.rt.log" "$i.rt.pix"
p1=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
rm -f "$i.rt.log" "$i.rt.pix"
p2=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
rm -f "$i.rt.log" "$i.rt.pix"
p3=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
rm -f "$i.rt.log" "$i.rt.pix"
p4=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
rm -f "$i.rt.log" "$i.rt.pix"
p5=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
if test "x$p1" = "x" ; then p1=0 ; fi
if test "x$p2" = "x" ; then p2=0 ; fi
if test "x$p3" = "x" ; then p3=0 ; fi
if test "x$p4" = "x" ; then p4=0 ; fi
if test "x$p5" = "x" ; then p5=0 ; fi
perf=`dc -e "3k $p1 $p2 $p3 $p4 $p5 + + + + 5.0 / p"`
if test "x$perf" = "x" ; then
    perf="0"
fi
rps=`printf -- "%10.0f rays/s" "${perf}"`
printf -- "%-25s ($p1 $p2 $p3 $p4 $p5)\n" "${rps}"

  ################
echo -n "Tied: "
export LIBRT_BOT_MINTIE=1
rm -f "$i.rt.log" "$i.rt.pix"
p1=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
rm -f "$i.rt.log" "$i.rt.pix"
p2=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
rm -f "$i.rt.log" "$i.rt.pix"
p3=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
rm -f "$i.rt.log" "$i.rt.pix"
p4=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
rm -f "$i.rt.log" "$i.rt.pix"
p5=`sh $i.rt -B -o $i.rt.pix -s$SZ -H$HP 2>&1 | tee $i.rt.log | grep RTFM | awk '{print $9}'`
if test "x$p1" = "x" ; then p1=0 ; fi
if test "x$p2" = "x" ; then p2=0 ; fi
if test "x$p3" = "x" ; then p3=0 ; fi
if test "x$p4" = "x" ; then p4=0 ; fi
if test "x$p5" = "x" ; then p5=0 ; fi
tperf=`dc -e "3k $p1 $p2 $p3 $p4 $p5 + + + + 5.0 / 0k p"`
if test "x$tperf" = "x" ; then
    tperf="0"
fi
rps=`printf -- "%10.0f rays/s" "${tperf}"`
printf -- "%-25s ($p1 $p2 $p3 $p4 $p5)\n" "${rps}"

  ################
echo -n "Prep: "
t1="`(time -p rt -B -o/dev/null -s1 $dbfile $i) 2>&1 | tail -3 | grep user | awk '{print $2}'`"
t2="`(time -p rt -B -o/dev/null -s1 $dbfile $i) 2>&1 | tail -3 | grep user | awk '{print $2}'`"
t3="`(time -p rt -B -o/dev/null -s1 $dbfile $i) 2>&1 | tail -3 | grep user | awk '{print $2}'`"
t4="`(time -p rt -B -o/dev/null -s1 $dbfile $i) 2>&1 | tail -3 | grep user | awk '{print $2}'`"
t5="`(time -p rt -B -o/dev/null -s1 $dbfile $i) 2>&1 | tail -3 | grep user | awk '{print $2}'`"
if test "x$t1" = "x" ; then t1=0 ; fi
if test "x$t2" = "x" ; then t2=0 ; fi
if test "x$t3" = "x" ; then t3=0 ; fi
if test "x$t4" = "x" ; then t4=0 ; fi
if test "x$t5" = "x" ; then t5=0 ; fi
prep=`dc -e "3k $t1 $t2 $t3 $t4 $t5 + + + + 5.0 / $perf * 0k p"`
if test "x$prep" = "x" ; then
    prep="0"
fi
rays=`printf -- "%10.0f rays" "${prep}"`
printf -- "%-25s ($t1 $t2 $t3 $t4 $t5)\n" "${rays}"

  ################
rm -f $base.base.rt.pix $base.base.rt.log
sh $base.base.rt -B -o $base.base.rt.pix -s$SZ >$base.base.rt.log 2>&1
if test -f $base.base.rt.pix ; then
    chmod 666 $base.base.rt.pix
fi
if ! test -f $base.base.rt.pix ; then
    echo "ERROR: $base.base.rt.pix failed to render"
    continue
fi

rm -f $i.tie.rt.pix $i.tie.rt.log $i.rt.pix $i.rt.log
export LIBRT_BOT_MINTIE=0
sh $i.rt -B -o $i.rt.pix -s$SZ >$i.rt.log 2>&1
if test -f "$i.rt.pix" ; then
    echo -n "Diff: "
    chmod 666 $i.rt.pix
    diff="`pixdiff $base.base.rt.pix $i.rt.pix 2>&1 1>/dev/null`"
    back="`pixcount $base.base.rt.pix 2>&1 | grep \"  0   0   1  \" | awk '{print $4}'`"
    fore="`expr $SZ \* $SZ - $back`"
    obm="`echo $diff | awk '{print $9}'`"
    percent=`dc -e "3k 1 $obm 3 / $fore / - 100 * d [0] sa 0.0 >a p"`
    printf -- "%10.1f %%%13s (`echo $diff | awk '{print $3/3, $4, $5/3, $6, $7, $8, $9/3, $10, $11, $12}'`, $back background, $fore foreground)\n" "$percent" " "
else
    printf -- "ERROR: $i.rt.pix failed to render\n"
fi
export LIBRT_BOT_MINTIE=1
sh $i.rt -B -o $i.tie.rt.pix -s$SZ >$i.tie.rt.log 2>&1
if test -f "$i.tie.rt.pix" ; then
    echo -n "TDif: "
    chmod 666 $i.tie.rt.pix
    diff="`pixdiff $i.rt.pix $i.tie.rt.pix 2>&1 1>/dev/null`"
    back="`pixcount $i.rt.pix 2>&1 | grep \"  0   0   1  \" | awk '{print $4}'`"
    fore="`expr $SZ \* $SZ - $back`"
    obm="`echo $diff | awk '{print $9}'`"
    percent=`dc -e "3k 1 $obm 3 / $fore / - 100 * d [0] sa 0.0 >a p"`
    printf -- "%10.1f %%%13s (`echo $diff | awk '{print $3/3, $4, $5/3, $6, $7, $8, $9/3, $10, $11, $12}'`, $back background, $fore foreground)\n" "$percent" " "
else
    printf -- "ERROR: $i.tie.rt.pix failed to render\n"
fi

  ################
echo -n "Norm: "
if test x`echo "$perf > $tperf" | bc` = x1 ; then
    bigger="$perf"
else
    bigger="$tperf"
fi
fore="`expr $SZ \* $SZ - $back`"
ratio=`dc -e "3k $fore $SZ d * / p"`
norm=`dc -e "3k $bigger $ratio * p"`
nrps=`printf -- "%10.0f rays/s" "${norm}"`
printf -- "%-25s ($bigger $fore $ratio)\n" "$nrps"

  ################
export RT=rtxray
echo -n "Xray: "
rm -f $base.base.rtxray.bw $base.base.rtxray.pix $base.base.rtxray.log
sh $base.base.rt -o $base.base.rtxray.pix -s$SZ >$base.base.rtxray.log 2>&1
mv $base.base.rtxray.pix $base.base.rtxray.bw
chmod 666 $base.base.rtxray.bw
if ! test -f $base.base.rtxray.bw ; then
    echo "ERROR: $base.base.rtxray.bw failed to render"
    continue
fi

rm -f $i.rtxray.bw $i.rtxray.pix $i.rtxray.log
sh $i.rt -o $i.rtxray.pix -s$SZ >$i.rtxray.log 2>&1
mv $i.rtxray.pix $i.rtxray.bw
chmod 666 $i.rtxray.bw
if test -f "$i.rtxray.bw" ; then
    chmod 666 $i.rtxray.bw
    bw-pix $base.base.rtxray.bw $base.base.rtxray.pix
    bw-pix $i.rtxray.bw $i.rtxray.pix
    diff="`pixdiff $base.base.rtxray.pix $i.rtxray.pix 2>&1 1>/dev/null`"
    back="`bw-pix $base.base.rtxray.bw | pixcount 2>&1 | grep \"  0   0   0  \" | awk '{print $4}'`"
    fore="`expr $SZ \* $SZ - $back`"
    obm="`echo $diff | awk '{print $9}'`"
    percent=`dc -e "3k 1 $obm 3 / $fore / - 100 * d [0] sa 0.0 >a p"`
    printf -- "%10.1f %%%13s (`echo $diff | awk '{print $3/3, $4, $5/3, $6, $7, $8, $9/3, $10, $11, $12}'`, $back background, $fore foreground)\n" "$percent" " "
else
    printf -- "ERROR: $i.rtxray.bw failed to render\n"
fi

  ################
export RT=rtedge
echo -n "Edge: "
rm -f $base.base.rtedge.pix $base.base.rtedge.log
sh $base.base.rt -o $base.base.rtedge.pix -s$SZ >$base.base.rtedge.log 2>&1
chmod 666 $base.base.rtedge.pix
if ! test -f $base.base.rtedge.pix ; then
    echo "ERROR: $base.base.rtedge.pix failed to render"
    continue
fi
rm -f $i.rtedge.pix $i.rtedge.log
sh $i.rt -o $i.rtedge.pix -s$SZ >$i.rtedge.log 2>&1
chmod 666 $i.rtedge.pix
if test -f $i.rtedge.pix ; then
    diff="`pixdiff $base.base.rtedge.pix $i.rtedge.pix 2>&1 1>/dev/null`"
    back="`pixcount $base.base.rtedge.pix 2>&1 | grep \"  0   0   0  \" | awk '{print $4}'`"
    fore="`expr $SZ \* $SZ - $back`"
    obm="`echo $diff | awk '{print $9}'`"
    percent=`dc -e "3k 1 $obm 3 / $fore / - 100 * d [0] sa 0.0 >a p"`
    printf -- "%10.1f %%%13s (`echo $diff | awk '{print $3/3, $4, $5/3, $6, $7, $8, $9/3, $10, $11, $12}'`, $back background, $fore foreground)\n" "$percent" " "
else
    printf -- "ERROR: $i.rtedge.pix failed to render\n"
fi

  ################
export RT=rtarea
echo -n "Area: "
rm -f $base.base.rtarea.log
sh $base.base.rt -o /dev/null -s$SZ >$base.base.rtarea.log 2>&1
if ! test -f $base.base.rtarea.log ; then
    echo "ERROR: $base.base.rtarea.log failed to evaluate"
    continue
fi
bare="`grep Cumulative $base.base.rtarea.log | awk '{print $7}'`"
if test "x$bare" = "x0.0000" ; then
    bare=1
fi
if ! test "x$bare" = "x" ; then
    rm -f $i.rtarea.log
    area="`sh $i.rt -o /dev/null -s$SZ 2>&1 | tee $i.rtarea.log | grep Cumulative | awk '{print $7}'`"
    devi=`dc -e "3k 100.0 $bare $area - d * v $bare / 100.0 * - d [0] sa 0.0 >a p"`
    if test "x$devi" = "x" ; then
        devi="-1"
    fi
    printf -- "%10.1f %%%13s ($bare $area)\n" "$devi" " "
else
    echo "ERROR: $base.base.rtarea.log failed to compute area"
fi

  ################
echo -n "Volu: "
rm -f $base.base.gqa.log
g_qa $GQTOL -Av $dbfile $base >$base.base.gqa.log 2>&1
if ! test -f $base.base.gqa.log ; then
    echo "ERROR: $base.base.gqa.log failed to evaluate"
    continue
fi
bvol=`grep total $base.base.gqa.log | awk '{print $4}'`
bvol=`printf "%.1f" $bvol`
if ! test "x$bvol" = "x" && ! test "x$bvol" = "x0.0" ; then
    rm -f "$i.gqa.log"
    vol="`g_qa $GQTOL -Av $dbfile $i 2>&1 | tee $i.gqa.log | tail -n 5 | grep total | awk '{print $4}'`"
    vol=`printf "%.1f" $vol`
    if test "x$vol" = "x" ; then
        vol="0"
    fi
    devi=`dc -e "3k 100.0 $bvol $vol - d * v $bvol / 100.0 * - d [0] sa 0.0 >a p"`
    devi=`printf "%.1f" $devi`
    if test "x$devi" = "x" ; then
        devi="-1"
    fi
    printf -- "%10.1f %%%13s ($vol $bvol)\n" "$devi" " "
else
    echo "ERROR: $base.base.gqa.log failed to compute volume"
fi

printf "\n\nFinished at `date`\n"

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
