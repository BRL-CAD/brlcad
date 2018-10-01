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

for cmd in loop mged rtcheck; do
    find_cmd "$cmd" >/dev/null || exit 1
done

loop="$(find_cmd loop)"
mged="$(find_cmd mged)"
rtcheck="$(find_cmd rtcheck)"

rm -f rtcheck_report.txt

for DB in "/usr/brlcad/share/db/"*.g; do
    db=`basename $DB`
    tops=`$mged -c $DB tops -n 2>&1`
    for obj in $tops ; do
	echo "\n$db - $obj\n"
	gs=50
	for az in `$loop 0 179 15` ; do
	    for el in `$loop 0 179 15` ; do
		echo "\ncheck overlaps -g $gs,$gs -a $az -e $el $obj"
		$mged -c $DB check overlaps -g $gs,$gs -a $az -e $el -t 0.1 $obj 2> check.txt
		sed -n '/maximum depth/{
			s/[<>]//g
			s/[,:] / /g
			s/^[[:space:]]*//g
			s/mm[[:space:]]*$//g
			p
			}' check.txt | cut -f 1,2,3,9 -d ' ' | sort > sort.check.txt
		cat sort.check.txt

		echo "\nrtcheck -g $gs -G $gs -a $az -e $el $obj"
		$rtcheck -g $gs -G $gs -a $az -e $el -+t $DB $obj 2> rtcheck.txt 1> rtcheck.plot.txt
		sed -n '/maximum depth/{
			s/[<>]//g
			s/[,:] / /g
			s/^[[:space:]]*//g
			s/mm[[:space:]]*$//g
			p
			}' rtcheck.txt | cut -f 1,2,3,9 -d ' ' | sort > sort.rtcheck.txt
		cat sort.rtcheck.txt

		diff -u0 --label $db.$obj.check.g$gs.a$az.e$el --label $db.$obj.rtcheck.g$gs.a$az.e$el sort.check.txt sort.rtcheck.txt >> rtcheck_report.txt
	    done
	done
    done

    for obj in $tops ; do
	echo "\n$db - $obj\n"
	gs=1
	for az in `$loop 0 179 15` ; do
	    for el in `$loop 0 179 15` ; do
		echo "\ncheck overlaps -g $gs,$gs -a $az -e $el $obj"
		$mged -c $DB check overlaps -g $gs,$gs -a $az -e $el -t 0.1 $obj 2> check.txt
		sed -n '/maximum depth/{
			s/[<>]//g
			s/[,:] / /g
			s/^[[:space:]]*//g
			s/mm[[:space:]]*$//g
			p
			}' check.txt | cut -f 1,2,3,9 -d ' ' | sort > sort.check.txt
		cat sort.check.txt

		echo "\nrtcheck -g $gs -G $gs -a $az -e $el $obj"
		$rtcheck -g $gs -G $gs -a $az -e $el -+t $DB $obj 2> rtcheck.txt 1> rtcheck.plot.txt
		sed -n '/maximum depth/{
			s/[<>]//g
			s/[,:] / /g
			s/^[[:space:]]*//g
			s/mm[[:space:]]*$//g
			p
			}' rtcheck.txt | cut -f 1,2,3,9 -d ' ' | sort > sort.rtcheck.txt
		cat sort.rtcheck.txt

		diff -u0 --label $db.$obj.check.g$gs.a$az.e$el --label $db.$obj.rtcheck.g$gs.a$az.e$el sort.check.txt sort.rtcheck.txt >> rtcheck_report.txt
	    done
	done
    done
done

# clean-up
rm -f check.txt
rm -f rtcheck.txt
rm -f sort.check.txt
rm -f sort.rtcheck.txt
rm -f rtcheck.plot.txt
