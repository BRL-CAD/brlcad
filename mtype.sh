#!/bin/sh
# A Shell script to determine the machine architecture type
# of the current system as a short descriptive string,
# to permit the separation of incompatible binary program files.
#
# This script must correctly report a machine type, of 5 chars or less:
#	vax	DEC VAX-11
#	sel	Gould (SEL) PowerNode (6000 or 9000)
#	fx	Alliant ("Dataflow") FX/4 or FX/8
#	3d	SGI 2400 or 3030
#	4d	SGI 4D
#	sun3	Sun-3 (68020 CPU)
#	sun4	Sun-4 (Sparc CPU)
#	pyr	Pyramid
#	tahoe	CCI/Harris Tahoe
#	cray2	Cray-2
#	xmp	Cray X-M/P
#
# -Mike Muuss, BRL.  6-March-1988

# Exists at least on Suns (eg, says "sun3")
if test -f /bin/arch
then
	/bin/arch; exit 0
fi

# Exists at least on SysV machines
for UNAME in /bin/uname /.attbin/uname
do
	if test -f $UNAME
	then
		case `$UNAME -m` in
		crayxmp)
			echo "xmp"; exit 0;;
		CRAY-2)
			echo "cray2"; exit 0;;
		m68020)
			# Iris specific
			case `$UNAME -t` in
			2300|2300T|3010)
				echo "3d"; exit 0;;
			2400|3030)
				echo "3d"; exit 0;;
			esac;;
		IP4)
			echo "4d"; exit 0;;
		Pyr90Mx)
			echo "pyr"; exit 0;;
		esac
	fi
done

# Vanilla Berkeley systems give few clues as to machine type
if test -d /usr/include/vaxif	; then	echo "vax"	; exit 0; fi
if test -d /usr/include/tahoeif	; then	echo "tahoe"	; exit 0; fi
if test -d /usr/include/selif	; then	echo "sel"	; exit 0; fi
if test -d /usr/include/dfif	; then	echo "fx"	; exit 0; fi

# Final hack for BSD systems:  depend on /etc/rc to put
# system description in 1st line of /etc/motd, use first word as selector
if test "(" -f /etc/motd ")" -a "(" -s /etc/motd ")"
then
	WORD=`sed -n -e "1s/ .*//p" /etc/motd`
	case $WORD in
	UTX/32)
		echo "sel"; exit 0;;
	Alliant)
		echo "fx"; exit 0;;
	esac
fi

echo "$0:  Your machine type is unknown -- FATAL ERROR" 1>&2
exit 1
