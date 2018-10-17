#!/bin/sh

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

for cmd in loop mged; do
    find_cmd "$cmd" >/dev/null || exit 1
done

loop="$(find_cmd loop)"
mged="$(find_cmd mged)"

rm -f overlaps_report.txt

for DB in "/usr/brlcad/share/db/"*.g; do
    db=`basename $DB`
    tops=`$mged -c $DB tops -n 2>&1`
    for obj in $tops ; do
	echo "\n$db - $obj\n"
	for gs in `$loop 30 10 -10` ; do
	    echo "\ncheck overlaps -g $gs,$gs $obj"
	    $mged -c $DB check overlaps -g $gs,$gs $obj 2> check.txt
	    sed -n '/maximum depth/{
		    s/[<>]//g
		    s/[,:] / /g
		    s/^[[:space:]]*//g
		    s/mm[[:space:]]*$//g
		    p
		    }' check.txt | cut -f 1,2,3,9 -d ' ' | sort > sort.check.txt
	    cat sort.check.txt

	    echo "\ngqa -Ao -g $gs,$gs $obj"
	    $mged -c $DB gqa -Ao -g $gs,$gs $obj 2> gqa.txt
	    sed -n '/dist:/{
		    s/count://g
		    s/dist://g
		    s/mm @.*$//g
		    p
		    }' gqa.txt | sort > sort.gqa.txt
	    cat sort.gqa.txt

	    diff -u0 --label $db.$obj.check.$gs --label $db.$obj.gqa.$gs sort.check.txt sort.gqa.txt >> overlaps_report.txt
	done
    done
done

# clean-up
rm -f check.txt
rm -f gqa.txt
rm -f sort.check.txt
rm -f sort.gqa.txt
