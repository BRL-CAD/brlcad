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

rm -f volume_report.txt

for DB in "/usr/brlcad/share/db/"*.g; do
    db=`basename $DB`
    tops=`$mged -c $DB tops -n 2>&1`
    for obj in $tops ; do
	echo "\n$db - $obj\n"
	for gs in `$loop 30 10 -10` ; do
	    echo "\ncheck volume -g $gs,$gs $obj"
	    $mged -c $DB check volume -g $gs,$gs $obj 2> check.txt
	    sed -n "/Average/{
		    s/.*: //g
		    p
		    }" check.txt > check.sed.txt
	    cat check.sed.txt

	    echo "\ngqa -Av -g $gs,$gs $obj"
	    $mged -c $DB gqa -Av -g $gs,$gs $obj 2> gqa.txt
	    sed -n "/Average/{
		    s/.*: //g
		    p
		    }" gqa.txt > gqa.sed.txt
	    cat gqa.sed.txt

	    diff -u0 --label $db.$obj.check.$gs --label $db.$obj.gqa.$gs check.sed.txt gqa.sed.txt >> volume_report.txt
	done
    done
done

# clean-up
rm -f check.txt
rm -f gqa.txt
rm -f check.sed.txt
rm -f gqa.sed.txt
