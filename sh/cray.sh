#!/bin/sh
#################################################################
#								#
#			C R A Y . S H				#
#								#
#  This shell script is intended for use with the MGED "rrt"	#
#  (remote RT) command.  It takes the local copy of the model	#
#  database, converts it to ASCII, and sends it over the 	#
#  network to a compute server machine (at BRL, usually either	#
#  a Cray or Alliant), where it is converted back into a binary	#
#  file in /tmp.  RT (on the compute server) is started, with	#
#  the viewing matrix provided on stdin, from the MGED rrt	#
#  command.							#
#								#
#  If the environment variable FB_FILE is set before MGED is	#
#  run, then we arrange to force the image to be routed to that	#
#  display (it better be in host:device form!).  This can be	#
#  overriden by supplying a -F option to the "rrt" command.	#
#								#
#  If the environment variable COMPUTE_SERVER is set before	#
#  MGED is run, that network host is used to perform the	#
#  raytracing.  Otherwise, BRL's XMP48 is used by default.	#
#								#
#  The compute server is assumed to have a different floating	#
#  point format than the local machine, hence the conversion	#
#  to ASCII.  The procedure can be simplified for machines of	#
#  identical binary database format.  The procedure becomes	#
#  almost trivial when compatible machines have NFS too.	#
#								#
#  Author -							#
#	Michael John Muuss					#
#								#
#  Source -							#
#	SECAD/VLD Computing Consortium, Bldg 394		#
#	The U. S. Army Ballistic Research Laboratory		#
#	Aberdeen Proving Ground, Maryland  21005		#
#								#
#  Copyright Notice -						#
#	This software is Copyright (C) 1987 by 			#
#	the United States Army.  All rights reserved.		#
#								#
#  Acknowledgements -						#
#	This Makefile draws heavily on Doug Gwyn's		#
#	"ipr" shell script for the option parsing technique	#
#								#
#  $Header$		#
#								#
#################################################################

PROG_NAME=$0

# Re-built the arguments, for easy parsing.
# This list must track the list in rt.c
set -- `getopt SH:F:D:MA:x:X:s:f:a:e:l:O:o:p:P:Bb:n:w:iI "$@"`

# If no compute server specified in users environment, use default.
if test x$COMPUTE_SERVER -eq x
then
	COMPUTE_SERVER=patton.brl.mil
	COMPUTE_SERVER=bob.brl.mil
fi

# If no framebuffer is specified in the users environment,
# then route the image back over the network to the local display.
if test x$FB_FILE -eq x
then
	FB_FILE=`hostname`:
fi

# The "rrt" command in MGED provides only user-specified option overrides.
# However, several need to be provided here.
#  -M is mandatory -- it specifies view matrix comes from stdin
#  -I sets interactive mode (don't lower priority when running)
#  -s sets the square viewing size
ARGS="-M -I -s256"

# Grind through all the options, looking out specially for -F (framebuffer).
while :
do
	case $1 in
	-S|-M|-B|-i|-I)
		ARGS="$ARGS $1";;
	-H|-F|-D|-A|-x|-X|-s|-f|-a|-e|-l|-O|-o|-p|-P|-b|-n|-w)
		ARGS="$ARGS $1 $2"; shift;;
	-F)
		FB_FILE="$2"; shift;;
	--)
		break;;
	esac
	shift
done
shift			# eliminate getopt provided "--" from $1

# Always put a framebuffer specification on the front.
ARGS="-F $FB_FILE $ARGS"

# Make certain that a database and at least one option are specified
if test $# -lt 2
then
	echo 'Usage:  $PROG_NAME [rt opts] database.g object(s)'
	exit 1
fi

DBASE=$1; shift
OBJS=$*

if test ! -f $DBASE
then
	echo "$PROG_NAME -- $DBASE unreadable"
	exit 1
fi

echo ""
echo "SERVER	$COMPUTE_SERVER"
echo "FILE	$DBASE $OBJS"
echo "ARGS	$ARGS"
echo ""

REM_DB="/tmp/${USER}db"
REM_MAT="/tmp/${USER}mat"	# Actually on local machine, for now

# Capture stdin from invoker, MGED rrt command
cat > $REM_MAT
g2asc < $DBASE | rsh $COMPUTE_SERVER "rm -f $REM_DB; asc2g > $REM_DB"

rsh $COMPUTE_SERVER "rt $ARGS $REM_DB $OBJS; rm -f $REM_DB" < $REM_MAT
rm -f $REM_MAT
exit 0
