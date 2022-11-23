#!/bin/sh

export LC_ALL=C

dry_run=false
if [ $# -gt 0 ] && [ "$1" = "-d" ]; then
    dry_run=true
    shift
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 filename.g [objects...]"
    exit 1
fi

db="$1"
sz="1024"
pwd=`pwd`

cmd_exists() {
    which "$1" >/dev/null 2>&1
}

find_cmd() {
    local cmd="$1"
    local ocmd="$1"
    local ret=1

    for stdpath in "" "/usr/brlcad/bin/" "/usr/brlcad/stable/bin/"; do
	fullcmd="$stdpath$cmd"

	if cmd_exists "$fullcmd"; then
	    cmd="$fullcmd"
	    ret=0
	    break
	fi
    done 

    if [ "$ret" -ne 0 ]; then
	echo "Error: couldn't find \"$ocmd\" command." >&2
    fi

    echo "$cmd"
    return "$ret"
}

for cmd in loop mged rtcheck gqa; do
    find_cmd "$cmd" >/dev/null || exit 1
done

loop="$(find_cmd loop)"
mged="$(find_cmd mged)"
rtcheck="$(find_cmd rtcheck)"
gqa="$(find_cmd gqa)"

gqa_opts="-q -g1mm,1mm"

echo ""
echo "CHECKING FOR OVERLAPS"
if $dry_run; then
    echo " (skipping rtcheck)"
fi
echo "====================="
echo ""

DIR=`basename $db 2>&1`.ck
JOB=ck.`basename $db 2>&1`

# stash a working copy
echo "Working on a copy in $DIR"
mkdir -p "$DIR"
cp $db $DIR/$JOB.g
cd $DIR
DB=$JOB.g

if [ $# -lt 2 ]; then
    tops=`$mged -c $DB tops -n 2>&1`
    echo "Processing top-level objects @ $sz:" $tops
else
    shift
    tops="$*"
    echo "Processing objects @ $sz:" $tops
fi

# count how many views (should be 16 per object)
total=0
for obj in $tops ; do
    for az in `$loop 0 179 45` ; do
	for el in `$loop 0 179 45` ; do
	    total="`expr $total + 1`"
	done
    done
done

# check the views
if ! $dry_run; then
    rm -f $OBJ.plot3
fi

# run rtcheck
count=1
for obj in $tops ; do
    OBJ=$JOB.$obj
    for az in `$loop 0 179 45` ; do
	for el in `$loop 0 179 45` ; do
	    echo "[$count/$total]"
	    if ! $dry_run; then
		rm -f $OBJ.$az.$el.plot3
		$rtcheck -o $OBJ.$az.$el.plot3 -s $sz -a $az -e $el $DB $obj 2> $OBJ.$az.$el.rtcheck.log
		if test -f $OBJ.$az.$el.plot3 ; then
		    chmod -f u+rw $OBJ.$az.$el.plot3
		    cat $OBJ.$az.$el.plot3 >> $OBJ.plot3
		fi
	    fi
	    count="`expr $count + 1`"
	done
    done
done


# parse out rtcheck overlap line pairings
rm -f $JOB.pairings
for obj in $tops ; do
    OBJ=$JOB.$obj
    rm -f $OBJ.pairings
    for az in `$loop 0 179 45` ; do
	for el in `$loop 0 179 45` ; do
	    if ! [ -f $OBJ.$az.$el.rtcheck.log ]; then
		echo "WARNING: $OBJ.$az.$el.rtcheck.log is MISSING"
	    else
		sed -n '/maximum depth/{
		    s/[<>]//g
		    s/[,:] / /g
		    s/^[[:space:]]*//g
		    s/mm[[:space:]]*$//g
		    p
		    }' $OBJ.$az.$el.rtcheck.log |
		    cut -f 1,2,3,9 -d ' ' |
		    awk '{if ($2 < $1) { tmp = $1; $1 = $2; $2 = tmp}; print $1, $2, $3 * $4}' >> $OBJ.pairings
	    fi
	done
    done
    cat $OBJ.pairings >> $JOB.pairings
done


# get a second opinion, run gqa
for obj in $tops ; do
    OBJ=$JOB.$obj
    echo "[$obj]"
    if ! $dry_run; then
	$gqa -Ao $gqa_opts $DB $obj > $OBJ.gqa.log 2>&1
    fi
done


# parse out gqa overlap line pairings
for obj in $tops ; do
    OBJ=$JOB.$obj
    rm -f $OBJ.pairings
    if ! [ -f $OBJ.gqa.log ]; then
	echo "WARNING: $OBJ.gqa.log is MISSING"
    else
	sed -n '/dist:/{
	        s/count://g
	        s/dist://g
	        s/mm @.*$//g
	        p
	       }' $OBJ.gqa.log |
	awk '{if ($2 < $1) { tmp = $1; $1 = $2; $2 = tmp}; print $1, $2, $3 * $4}' >> $OBJ.pairings

	cat $OBJ.pairings >> $JOB.pairings
    fi
done


# tabulate and sort into unique pairings
lines="`sort $JOB.pairings`"
if test "x$lines" = "x" ; then
    linecount=0
    echo ""
    echo "NO OVERLAPS DETECTED"
    cd $pwd
    exit
else
    linecount=`echo "$lines" | wc -l`
fi

echo "Processing $linecount pairings"


awkscript="{printf \"%.4f\", \$1 + (\$2 / $total)}"
leftprev=""
rightprev=""
totalprev=0
count=0
printf "[."
while read line ; do
    left=`echo $line | awk '{print $1}'`
    right=`echo $line | awk '{print $2}'`
    len=`echo $line | awk '{print $3}'`

    if test $linecount -gt 999 ; then
	# print every 100 pairings
	if test `expr $count % 100` -eq 0 ; then
	    printf "."
	fi
    elif test $linecount -gt 99 ; then
	# print every 10 pairings
	if test `expr $count % 10` -eq 0 ; then
	    printf "."
	fi
    else
	# print every pairing
	printf "."
    fi

    # init on first pass
    if test "x$leftprev" = "x" && test "x$rightprev" = "x" ; then
	leftprev=$left
	rightprev=$right
    fi

    # only record when current line is different from prevoius
    if test "x$leftprev" != "x$left" || test "x$rightprev" != "x$right" ; then
	LINES="$LINES
$leftprev $rightprev $totalprev"
	leftprev="$left"
	rightprev="$right"
	totalprev=0
    fi

    totalprev="`echo $totalprev $len | awk \"$awkscript\"`"
    count="`expr $count + 1`"
done <<EOF
$lines
EOF

# record the last line
if test "x$LINES" = "x" && test "x$leftprev" != "x" ; then
    LINES="$leftprev $rightprev $totalprev"
elif test "x$leftprev" != "x" ; then
    LINES="$LINES
$leftprev $rightprev $totalprev"
fi

printf ".]\n"


# report how many unique pairings
if test "x$LINES" = "x" && test "x$leftprev" = "x" ; then
    overlapcnt=0
    echo ""
    echo "NO UNIQUE PAIRINGS FOUND?"
    cd $pwd
    exit
else
    overlapcnt=`echo "$LINES" | sort -k3 -n -r | wc -l`
fi
echo "Found $overlapcnt unique pairings"


# disable until something is using the more.plot3 files
if `false` ; then

    # check each pairing
    sz="`echo $sz | awk '{print $1 / 4}'`"
    count=1
    if ! $dry_run; then
	rm -f $JOB.more.plot3
    fi
    while read overlap ; do
	echo "Inspecting overlap [$count/$overlapcnt] @ $sz"

	objs="`echo $overlap | awk '{print $1, $2}'`"

	for az in `$loop 0 179 45` ; do
	    for el in `$loop 0 179 45` ; do
		if ! $dry_run; then
		    rm -f $JOB.$count.$az.$el.plot3
		    $rtcheck -o $JOB.$count.$az.$el.plot3 -s $sz -a $az -e $el $DB $objs 2> $JOB.$count.$az.$el
		    if test -f $JOB.$count.$az.$el.plot3 ; then
			chmod -f u+rw $JOB.$count.$az.$el.plot3
			cat $JOB.$count.$az.$el.plot3 >> $JOB.more.plot3
		    fi
		fi
	    done
	done

	count="`expr $count + 1`"
    done <<EOF
`echo "$LINES" | sort -k3 -n -r`
EOF

fi


# summarize the overlaps
echo "Overlap Summary:"
echo "$LINES" | sort -k3 -n -r > $JOB.overlaps
cat $JOB.overlaps


cd $pwd
echo ""
echo "Overlaps saved to $DIR/$JOB.overlaps"
echo ""
echo "Done."

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
