#!/bin/sh
#                         C R A Y . S H
# BRL-CAD
#
# Copyright (c) 2004-2006 United States Government as represented by
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
#################################################################
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
#  This can be altered from within MGED by specifying the	#
#  -C compute_server flag, which this script intercepts and	#
#  uses to change the default/environment specification.	#
#  The -C flag is not passed on to RT.				#
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
#  Acknowledgements -						#
#	This Makefile draws heavily on Doug Gwyn's		#
#	"ipr" shell script for the option parsing technique	#
#								#
#  $Header$		#
#								#
#################################################################

# Silicon Graphics renamed /usr/ucb as /usr/bsd, sigh
PATH=/usr/bsd:$PATH
export PATH

PROG_NAME=$0

# Re-build the arguments, for easy parsing.
# This list must track the list in rt.c, plus "-C"
set -- `getopt C:SH:F:D:MA:x:X:s:f:a:e:l:O:o:p:P:Bb:n:w:iIJ "$@"`

# If no compute server specified in users environment, use default.
if test x$COMPUTE_SERVER = x
then
	COMPUTE_SERVER=patton.brl.mil
fi

# If no framebuffer is specified in the users environment,
# then route the image back over the network to the local display.
if test x$FB_FILE = x
then
	FB_FILE=`hostname`:
fi

# The "rrt" command in MGED provides only user-specified option overrides.
# However, several need to be provided here.
#  -M is mandatory -- it specifies view matrix comes from stdin
#  -I sets interactive mode (don't lower priority when running)
#  -s sets the square viewing size
ARGS="-M -I -s256"

# Grind through all the options, looking out specially for -F (framebuffer)
# and -C (override compute-server)
while :
do
	case $1 in
	-S|-M|-B|-i|-I|-J)
		ARGS="$ARGS $1";;
	-H|-D|-A|-x|-X|-s|-f|-a|-e|-l|-O|-o|-p|-P|-b|-n|-w)
		ARGS="$ARGS $1 $2"; shift;;
	-F)
		FB_FILE="$2"; shift;;
	-C)
		COMPUTE_SERVER="$2"; shift;;
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
REM_MAT="/tmp/${USER}mat"

g2asc < $DBASE | rsh $COMPUTE_SERVER "rm -f $REM_DB; asc2g > $REM_DB"

RCMD="cat > $REM_MAT; \
	rt $ARGS $REM_DB $OBJS < $REM_MAT; \
	rm -f $REM_DB $REM_MAT"

# Uses stdin from invoker, typ. MGED rrt command
# Note that RT needs to be able to seek backwards in the matrix file,
# so it is read from the network stdin, and stashed in a temp file.
rsh $COMPUTE_SERVER $RCMD
exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
