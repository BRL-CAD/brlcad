#!/bin/sh
#			F A S T . S H
#
# A quick way of recompiling LIBBU using multiple processors.
#
# Optional flag:  -s	for silent running
#
#  $Header$

SILENT="$1"

# Prevent the massive compilation from degrading interactive windows.
renice 12 $$ > /dev/null 2>&1

cake ${SILENT}  \
 association.o \
 avs.o \
 badmagic.o \
 bitv.o \
 bomb.o \
 &

cake ${SILENT} \
 brlcad_path.o \
 bu_tcl.o \
 cmd.o \
 cmdhist.o \
 cmdhist_obj.o \
 color.o \
 convert.o \
 &

cake ${SILENT} \
 getopt.o \
 hist.o \
 htond.o \
 htonf.o \
 ispar.o \
 linebuf.o \
 list.o \
 log.o \
 &

cake ${SILENT} \
 magic.o \
 malloc.o \
 mappedfile.o \
 memset.o \
 observer.o \
 parallel.o \
 parse.o \
 printb.o \
 ptbl.o \
 &

cake ${SILENT} \
 rb_create.o \
 rb_delete.o \
 rb_diag.o \
 rb_extreme.o \
 rb_free.o \
 rb_insert.o \
 rb_order_stats.o \
 rb_rotate.o \
 rb_search.o \
 rb_walk.o \
 &

cake ${SILENT} \
 semaphore.o \
 units.o \
 vfont.o \
 vls.o \
 xdr.o \
 &

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
