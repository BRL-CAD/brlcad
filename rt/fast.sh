#!/bin/sh
#			F A S T . S H
#
# A quick way of recompiling RT using multiple processors.
#
# Optional flag:  -s	for silent running
#
#  $Header$

SILENT="$1"

# Prevent the massive compilation from degrading interactive windows.
renice 12 $$ > /dev/null 2>&1

cake ${SILENT} \
 do.o \
 material.o \
 opt.o \
 refract.o &

cake ${SILENT} \
 view.o \
 viewcheck.o \
 viewg3.o &

cake ${SILENT} \
 viewpp.o \
 viewrad.o \
 viewray.o &

cake ${SILENT} \
 shade.o \
 worker.o \
 wray.o &

cake ${SILENT} \
 main.o \
 rtshot.o \
 rtwalk.o &

#  NOTE!!!  The programs can't be built in parallel,
#  because the newvers.sh script always writes to vers.o
#
#wait
#if test "${SILENT}" = ""
#then
#	echo --- Building the programs
#fi
#
#cake ${SILENT} \
# rt &
#
#cake ${SILENT} \
# rtrad rtpp rtray rtweight &
#
#cake ${SILENT} \
# rtshot rtwalk rtcheck rtg3 &
#
#cake ${SILENT} \
# rtcell rtxray rthide rtfrac &
#
#cake ${SILENT} \
# rtrange rtregis rtscale rtsil &


wait
if test "${SILENT}" = ""
then
	echo --- Collecting any stragglers.
fi
cake ${SILENT}
