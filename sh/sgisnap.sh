#!/bin/sh
#################################################################
#								#
#			S G I S N A P . S H			#
#								#
#  A general purpose script to photograph an arbitrary image	#
#  file (.rle, .pix, .bw) on a Dunn camera connected to an	#
#  SGI workstation.						#
#  The main point of this script is to automagicly determine	#
#  the type and size of the image, and then expand it to	#
#  occupy as much of the actual 1280x1024 screen as possible.	#
#								#
#  Author -							#
#	Michael John Muuss					#
#								#
#  Source -							#
#	SECAD/VLD Computing Consortium, Bldg 394		#
#	The U. S. Army Ballistic Research Laboratory		#
#	Aberdeen Proving Ground, Maryland  21005-5066		#
#								#
#  $Header$	#
#								#
#################################################################

FB_FILE=/dev/sgif	# Full screen
export FB_FILE
FB2=/dev/sgift		# Full screen, 30 hz

fbgamma 1.5

FILES="$*"

for i in $FILES
do
	WIDTH=0
	HEIGHT=0

	BASENAME=`basename $i`
	if test `basename $BASENAME .pix`.pix = $BASENAME
	then
		# .pix file
		# On BSD machines, $4 will be byte size
		# On SGI, $5 will be byte size (because of group being $4)
		set -- `ls -l $i`
		BYTES=$5
		case $BYTES in
		786432)
			# is 512, enlarge
			pixinterp2x -s512 $i | pix-fb -h
			WIDTH=1024; HEIGHT=1024;;
		921600)
			# is 640x480, enlarge to 1280x960
			pixinterp2x -w640 -n480 $i | pix-fb -w1280 -n960
			WIDTH=1280; HEIGHT=960;;
		3145728)
			pix-fb -h $i;
			WIDTH=1024; HEIGHT=1024;;
		3932160)
			pix-fb -w1280 -n1024 $i;
			WIDTH=1280; HEIGHT=1024;;
		*)
			echo "$i: $BYTES is an unusual size for a .pix file!";
			WIDTH=0; HEIGHT=0;;
		esac
	else
		if test `basename $BASENAME .bw`.bw = $BASENAME
		then
			# .bw file
			# On BSD machines, $4 will be byte size
			# On SGI, $5 will be byte size (because of group being $4)
			set -- `ls -l $i`
			BYTES=$5
			case $BYTES in
			262144)
				# is 512, enlarge
				bwscale -s512 -S1024 $i | bw-fb -s1024
				WIDTH=1024; HEIGHT=1024;;
			1048576)
				bw-fb -h $i;
				WIDTH=1024; HEIGHT=1024;;
			*)
				echo "$i: $BYTES is an unusual size for a .bw file!";
				WIDTH=0; HEIGHT=0;;
			esac
		else
			# assume .rle file
			SIZE=`rle-pix -H $i`
			set -- `getopt hs:S:w:W:n:N: $SIZE`
			while :
			do
				case $1 in
				-h)
					WIDTH=1024; HEIGHT=1024;;
				-s|-S)
					WIDTH=$2; HEIGHT=$2; shift;;
				-w|-W)
					WIDTH=$2; shift;;
				-n|-N)
					HEIGHT=$2; shift;;
				--)
					break;
				esac
				shift
			done
			shift		# eliminate getopt provided "--" from $1

			if test $WIDTH -ge 1024 -a $HEIGHT -eq 1024
			then
				# crunch colormap, to avoid ruining fbgamma
				rle-fb -c $i
			else
				# enlarge
				if test $WIDTH -eq 512 -a $HEIGHT -eq 512
				then
					rle-pix -c $i | pixinterp2x -s512 | pix-fb -h
					WIDTH=1024
					HEIGHT=1024
				else
					echo "$i:  cant enlarge ${WIDTH}x${HEIGHT} yet"
					WIDTH=0; HEIGHT=0
				fi
			fi
		fi
	fi

	echo "$i: now -w $WIDTH -n $HEIGHT"
	RETVAL=99
	while test $WIDTH -gt 0 -a $RETVAL -ne 0
	do
		dunnsnap -w $WIDTH -n $HEIGHT -F $FB2 1
		RETVAL=$?
		if test $RETVAL -ne 0
		then
			ANS=humbug
			while test $ANS != go
			do
				echo "$i: camera needs attention, enter -go-"
				read ANS
			done
			# retake will be done on next pass through
			Set60		# timeout leaves sgi in 30hz
		fi
	done
done

exit 0
