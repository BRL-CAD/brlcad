#!/bin/sh
#			F A S T . S H
#
# A quick way of recompiling MGED using multiple processors.
#
# Optional flag:  -s	for silent running
#
#  $Header$

SILENT="$1"

# Prevent the massive compilation from degrading interactive windows.
renice 12 $$ > /dev/null 2>&1

cake ${SILENT} \
 adc.o \
 anal.o \
 arb.o \
 arbs.o \
 attach.o &

cake ${SILENT} \
 buttons.o \
 chgmodel.o \
 chgtree.o \
 chgview.o \
 cmd.o \
 columns.o &

cake ${SILENT} \
 grid.o \
 axes.o \
 qray.o \
 color_scheme.o \
 share.o \
 polyif.o \
 predictor.o \
 edpipe.o \
 edars.o &

cake ${SILENT} \
 red.o \
 set.o \
 animedit.o \
 solids_on_ray.o \
 comb_std.o \
 vrlink.o \
 dm-generic.o \
 vdraw.o \
 fbserv.o \
 rect.o &

cake ${SILENT} \
 dir.o \
 dodraw.o \
 dozoom.o \
 edarb.o \
 edsol.o &

cake ${SILENT} \
 facedef.o \
 ged.o \
 history.o \
 inside.o \
 mater.o &

cake ${SILENT} \
 menu.o \
 mover.o \
 overlay.o \
 plot.o \
 proc_reg.o \
 rtif.o &

cake ${SILENT} \
 scroll.o \
 tedit.o \
 titles.o \
 track.o &

cake ${SILENT} \
 typein.o \
 usepen.o \
 concat.o \
 utility1.o \
 utility2.o &

cake ${SILENT} \
 dm-plot.o dm-ps.o \
 vparse.o \
 doevent.o muves.o \
 bodyio.o &

wait
if test "${SILENT}" = ""
then
	echo --- Collecting any stragglers.
fi
cake ${SILENT}

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
