#!/bin/sh
#			F A S T . S H
#
# A quick way of recompiling LIBBN using multiple processors.
#
# Optional flag:  -s	for silent running
#
#  $Header$

SILENT="$1"

# Prevent the massive compilation from degrading interactive windows.
renice 12 $$ > /dev/null 2>&1

cake ${SILENT}  \
 anim.o \
 asize.o \
 axis.o \
 bn_tcl.o \
 complex.o \
 &

cake ${SILENT} \
 const.o \
 font.o \
 fortran.o \
 list.o \
 marker.o \
 &

cake ${SILENT} \
 mat.o \
 msr.o \
 noise.o \
 number.o \
 plane.o \
 plot3.o \
 &

cake ${SILENT} \
 poly.o \
 qmath.o \
 rand.o \
 scale.o \
 sphmap.o \
 &

cake ${SILENT} \
 symbol.o \
 tabdata.o \
 tplot.o \
 vectfont.o \
 vector.o \
 wavelet.o \
 &

wait
if test "${SILENT}" = ""
then
	echo --- Collecting any stragglers.
fi
cake ${SILENT}
