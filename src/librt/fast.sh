#!/bin/sh
#			F A S T . S H
#
# A quick way of recompiling LIBRT using multiple processors.
#
# Optional flag:  -s	for silent running
#
#  $Header$

SILENT="$1"

# Prevent the massive compilation from degrading interactive windows.
renice 12 $$ > /dev/null 2>&1

cake ${SILENT}  \
 nmg_misc.o \
 g_nurb.o \
 g_nmg.o \
 cmd.o \
 cut.o \
 dir.o \
 g_hf.o \
 &

cake ${SILENT} \
 g_arb.o \
 nmg_rt_isect.o \
 g_ars.o \
 db_alloc.o \
 db_anim.o \
 db_io.o \
 db_lookup.o &

cake ${SILENT} \
 nmg_rt_segs.o \
 g_ebm.o \
 g_ell.o \
 g_extrude.o \
 g_submodel.o \
 g_bot.o \
 db_open.o \
 db_path.o \
 db_scan.o \
 nmg_ck.o \
 nmg_pt_fu.o \
 db_walk.o &

cake ${SILENT} \
 g_half.o \
 nmg_fcut.o \
 nmg_index.o \
 nmg_inter.o \
 nmg_extrude.o \
 nmg_tri.o \
 g_sph.o \
 &

cake ${SILENT} \
 g_tgc.o \
 g_torus.o \
 g_part.o \
 g_pipe.o \
 g_dsp.o \
 g_sketch.o \
 nmg_class.o \
 &

cake ${SILENT} \
 nmg_manif.o \
 nmg_visit.o \
 nmg_info.o \
 nmg_pr.o \
 global.o \
 mater.o \
 pmalloc.o \
 memalloc.o \
 tcl.o \
 &

cake ${SILENT} \
 pr.o \
 db_tree.o \
 db_comb.o \
 db_match.o \
 g_vol.o \
 g_rpc.o \
 g_rhc.o \
 g_epa.o \
 g_ehy.o \
 g_eto.o \
 g_grip.o \
 nmg_eval.o &

cake ${SILENT} \
 g_rec.o \
 g_pg.o \
 bool.o \
 nmg_mesh.o \
 nmg_mk.o \
 shoot.o \
 nmg_plot.o \
 &

cake ${SILENT} \
 wdb.o \
 fortray.o \
 nmg_bool.o \
 nmg_fuse.o \
 prep.o \
 roots.o \
 g_arbn.o \
 &

cake ${SILENT} \
 nmg_mod.o \
 storage.o \
 spectrum.o \
 table.o \
 timer42.o \
 vlist.o \
 regionfix.o \
 tree.o &

cake ${SILENT} \
 nurb_basis.o nurb_bound.o nurb_diff.o nurb_eval.o nurb_flat.o \
 nurb_knot.o nurb_norm.o nurb_poly.o nurb_ray.o nurb_refine.o \
 nurb_solve.o nurb_split.o nurb_util.o nurb_xsplit.o nurb_copy.o &

cake ${SILENT} \
 nurb_c2.o oslo_calc.o oslo_map.o nurb_plot.o nurb_bezier.o nurb_trim.o \
 nurb_interp.o nurb_reverse.o nurb_tess.o nurb_trim_util.o &

cake ${SILENT} \
 db5_scan.o db5_io.o db5_comb.o db5_alloc.o db5_types.o db5_bin.o \
 g_cline.o htbl.o mkbundle.o bundle.o many.o rt_dspline.o &

cake ${SILENT} \
 bomb.o wdb_obj.o view_obj.o dg_obj.o vdraw.o wdb_comb_std.o &

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
