#!/bin/sh
#			F A S T . S H
#
# A quick way of recompiling MGED using multiple processors.
#
#  $Header$

cake \
 adc.o \
 anal.o \
 arb.o \
 arbs.o \
 attach.o &

cake \
 buttons.o \
 chgmodel.o \
 chgtree.o \
 chgview.o \
 cmd.o \
 columns.o &

cake \
 grid.o \
 axes.o \
 qray.o \
 color_scheme.o \
 share.o \
 polyif.o \
 predictor.o \
 edpipe.o \
 edars.o &

cake \
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

cake \
 dir.o \
 dodraw.o \
 dozoom.o \
 edarb.o \
 edsol.o &

cake \
 facedef.o \
 ged.o \
 history.o \
 inside.o \
 mater.o &

cake \
 menu.o \
 mover.o \
 overlay.o \
 plot.o \
 proc_reg.o \
 rtif.o &

cake \
 scroll.o \
 tedit.o \
 titles.o \
 track.o &

cake \
 typein.o \
 usepen.o \
 concat.o \
 utility1.o \
 utility2.o &

wait
echo --- Collecting any stragglers.
cake
