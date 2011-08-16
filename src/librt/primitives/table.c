/*                         T A B L E . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup g_ */
/** @{ */
/** @file primitives/table.c
 *
 * Tables for the BRL-CAD Package ray-tracing library "librt".
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"


#define RT_DECLARE_INTERFACE(name) \
    extern int rt_##name##_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip); \
    extern int rt_##name##_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead); \
    extern int rt_##name##_piece_shot(struct rt_piecestate *psp, struct rt_piecelist *plp, double dist_corr, struct xray *rp, struct application *ap, struct seg *seghead); \
    extern void rt_##name##_piece_hitsegs(struct rt_piecestate *psp, struct seg *seghead, struct application *ap); \
    extern void rt_##name##_print(const struct soltab *stp); \
    extern void rt_##name##_norm(struct hit *hitp, struct soltab *stp, struct xray *rp); \
    extern void rt_##name##_uv(struct application *ap, struct soltab *stp, struct hit *hitp, struct uvcoord *uvp); \
    extern void rt_##name##_curve(struct curvature *cvp, struct hit *hitp, struct soltab *stp); \
    extern int rt_##name##_class(); \
    extern void rt_##name##_free(struct soltab *stp); \
    extern int rt_##name##_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol); \
    extern void rt_##name##_vshot(struct soltab *stp[], struct xray *rp[], struct seg *segp, int n, struct application *ap); \
    extern int rt_##name##_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol); \
    extern int rt_##name##_tnurb(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern void rt_##name##_brep(ON_Brep **b, struct rt_db_internal *ip, const struct bn_tol *tol); \
    extern int rt_##name##_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp); \
    extern int rt_##name##_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip, struct resource *resp); \
    extern int rt_##name##_import4(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp); \
    extern int rt_##name##_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip, struct resource *resp); \
    extern void rt_##name##_ifree(struct rt_db_internal *ip); \
    extern int rt_##name##_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr); \
    extern int rt_##name##_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv); \
    extern int rt_##name##_bbox(struct rt_db_internal *ip, point_t *min, point_t *max); \
    extern int rt_##name##_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local, struct resource *resp, struct db_i *db_i); \
    extern void rt_##name##_make(const struct rt_functab *ftp, struct rt_db_internal *intern); \
    extern int rt_##name##_xform(struct rt_db_internal *op, const mat_t mat, struct rt_db_internal *ip, int release, struct db_i *dbip, struct resource *resp); \
    extern int rt_##name##_params(struct pc_pc_set *ps, const struct rt_db_internal *ip); \
    extern int rt_##name##_mirror(struct rt_db_internal *ip, const plane_t *plane); \
    extern const struct bu_structparse rt_##name##_parse[]


RT_DECLARE_INTERFACE(tor);
RT_DECLARE_INTERFACE(tgc);
RT_DECLARE_INTERFACE(ell);
RT_DECLARE_INTERFACE(arb);
RT_DECLARE_INTERFACE(ars);
RT_DECLARE_INTERFACE(hlf);
RT_DECLARE_INTERFACE(rec);
RT_DECLARE_INTERFACE(pg);
RT_DECLARE_INTERFACE(nurb);
RT_DECLARE_INTERFACE(sph);
RT_DECLARE_INTERFACE(ebm);
RT_DECLARE_INTERFACE(vol);
RT_DECLARE_INTERFACE(arbn);
RT_DECLARE_INTERFACE(pipe);
RT_DECLARE_INTERFACE(part);
RT_DECLARE_INTERFACE(nmg);
RT_DECLARE_INTERFACE(rpc);
RT_DECLARE_INTERFACE(rhc);
RT_DECLARE_INTERFACE(epa);
RT_DECLARE_INTERFACE(ehy);
RT_DECLARE_INTERFACE(eto);
RT_DECLARE_INTERFACE(grp);
RT_DECLARE_INTERFACE(hf);
RT_DECLARE_INTERFACE(dsp);
RT_DECLARE_INTERFACE(sketch);
RT_DECLARE_INTERFACE(extrude);
RT_DECLARE_INTERFACE(submodel);
RT_DECLARE_INTERFACE(cline);
RT_DECLARE_INTERFACE(bot);
RT_DECLARE_INTERFACE(superell);
RT_DECLARE_INTERFACE(metaball);
RT_DECLARE_INTERFACE(hyp);
RT_DECLARE_INTERFACE(revolve);
RT_DECLARE_INTERFACE(constraint);
/* RT_DECLARE_INTERFACE(binunif); */
RT_DECLARE_INTERFACE(pnts);

#if OBJ_BREP
RT_DECLARE_INTERFACE(brep);
#endif


/* generics for object manipulation, in generic.c */
extern int rt_generic_get(struct bu_vls *, const struct rt_db_internal *, const char *);
extern int rt_generic_adjust(struct bu_vls *, struct rt_db_internal *, int, const char **);
extern int rt_generic_form(struct bu_vls *, const struct rt_functab *);
extern void rt_generic_make(const struct rt_functab *, struct rt_db_internal *);
extern int rt_generic_xform(struct rt_db_internal *, const mat_t, struct rt_db_internal *, int, struct db_i *, struct resource *);

/* from db5_bin.c */
extern int rt_binunif_import5(struct rt_db_internal * ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);
extern int rt_binunif_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip, struct resource *resp);
extern void rt_binunif_ifree(struct rt_db_internal *ip);
extern int rt_binunif_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local, struct resource *resp, struct db_i *db_i);
extern void rt_binunif_make(const struct rt_functab *ftp, struct rt_db_internal *intern);
extern int rt_binunif_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr);
extern int rt_binunif_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv);

/* from tcl.c and db5_comb.c */
extern int rt_comb_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip, struct resource *resp);
extern int rt_comb_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);
extern int rt_comb_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *item);
extern int rt_comb_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv);
extern int rt_comb_form(struct bu_vls *logstr, const struct rt_functab *ftp);
extern void rt_comb_make(const struct rt_functab *ftp, struct rt_db_internal *intern);
extern void rt_comb_ifree(struct rt_db_internal *ip);

extern int rt_ebm_form(struct bu_vls *logstr, const struct rt_functab *ftp);
extern int rt_bot_form(struct bu_vls *logstr, const struct rt_functab *ftp);
extern int rt_sketch_form(struct bu_vls *logstr, const struct rt_functab *ftp);
extern int rt_cline_form(struct bu_vls *logstr, const struct rt_functab *ftp);
extern int rt_extrude_form(struct bu_vls *logstr, const struct rt_functab *ftp);


const struct rt_functab rt_functab[] = {
    {
	/* 0: unused, for sanity checking. */
	RT_FUNCTAB_MAGIC, "ID_NULL", "NULL",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
    },

    {
	/* 1 */
	RT_FUNCTAB_MAGIC, "ID_TOR", "tor",
	1,
	rt_tor_prep,
	rt_tor_shot,
	rt_tor_print,
	rt_tor_norm,
	NULL,
	NULL,
	rt_tor_uv,
	rt_tor_curve,
	rt_tor_class,
	rt_tor_free,
	rt_tor_plot,
	rt_tor_vshot,
	rt_tor_tess,
	NULL,
	rt_tor_brep,
	rt_tor_import5,
	rt_tor_export5,
	rt_tor_import4,
	rt_tor_export4,
	rt_tor_ifree,
	rt_tor_bbox,
	rt_tor_describe,
	rt_generic_xform,
	rt_tor_parse,
	sizeof(struct rt_tor_internal),
	RT_TOR_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_tor_params
    },

    {
	/* 2 */
	RT_FUNCTAB_MAGIC, "ID_TGC", "tgc",
	1,
	rt_tgc_prep,
	rt_tgc_shot,
	rt_tgc_print,
	rt_tgc_norm,
	NULL,
	NULL,
	rt_tgc_uv,
	rt_tgc_curve,
	rt_tgc_class,
	rt_tgc_free,
	rt_tgc_plot,
	rt_tgc_vshot,
	rt_tgc_tess,
	rt_tgc_tnurb,
	rt_tgc_brep,
	rt_tgc_import5,
	rt_tgc_export5,
	rt_tgc_import4,
	rt_tgc_export4,
	rt_tgc_ifree,
	rt_tgc_bbox,
	rt_tgc_describe,
	rt_generic_xform,
	rt_tgc_parse,
	sizeof(struct rt_tgc_internal),
	RT_TGC_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_tgc_params
    },

    {
	/* 3 */
	RT_FUNCTAB_MAGIC, "ID_ELL", "ell",
	1,
	rt_ell_prep,
	rt_ell_shot,
	rt_ell_print,
	rt_ell_norm,
	NULL,
	NULL,
	rt_ell_uv,
	rt_ell_curve,
	rt_ell_class,
	rt_ell_free,
	rt_ell_plot,
	rt_ell_vshot,
	rt_ell_tess,
	rt_ell_tnurb,
	rt_ell_brep,
	rt_ell_import5,
	rt_ell_export5,
	rt_ell_import4,
	rt_ell_export4,
	rt_ell_ifree,
	rt_ell_bbox,
	rt_ell_describe,
	rt_generic_xform,
	rt_ell_parse,
	sizeof(struct rt_ell_internal),
	RT_ELL_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_ell_params
    },

    {
	/* 4 */
	RT_FUNCTAB_MAGIC, "ID_ARB8", "arb8",
	0,
	rt_arb_prep,
	rt_arb_shot,
	rt_arb_print,
	rt_arb_norm,
	NULL,
	NULL,
	rt_arb_uv,
	rt_arb_curve,
	rt_arb_class,
	rt_arb_free,
	rt_arb_plot,
	rt_arb_vshot,
	rt_arb_tess,
	rt_arb_tnurb,
	rt_arb_brep,
	rt_arb_import5,
	rt_arb_export5,
	rt_arb_import4,
	rt_arb_export4,
	rt_arb_ifree,
	rt_arb_bbox,
	rt_arb_describe,
	rt_generic_xform,
	rt_arb_parse,
	sizeof(struct rt_arb_internal),
	RT_ARB_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_arb_params
    },

    {
	/* 5 */
	RT_FUNCTAB_MAGIC, "ID_ARS", "ars",
	1,
	rt_ars_prep,
	rt_bot_shot,
	rt_ars_print,
	rt_bot_norm,
	rt_bot_piece_shot,
	rt_bot_piece_hitsegs,
	rt_ars_uv,
	rt_bot_curve,
	rt_bot_class,
	rt_bot_free,
	rt_ars_plot,
	NULL,
	rt_ars_tess,
	NULL,
	NULL,
	rt_ars_import5,
	rt_ars_export5,
	rt_ars_import4,
	rt_ars_export4,
	rt_ars_ifree,
	NULL,
	rt_ars_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct rt_ars_internal),
	RT_ARS_INTERNAL_MAGIC,
	rt_ars_get,
	rt_ars_adjust,
	rt_generic_form,
	NULL,
	rt_ars_params
    },

    {
	/* 6 */
	RT_FUNCTAB_MAGIC, "ID_HALF", "half",
	0,
	rt_hlf_prep,
	rt_hlf_shot,
	rt_hlf_print,
	rt_hlf_norm,
	NULL,
	NULL,
	rt_hlf_uv,
	rt_hlf_curve,
	rt_hlf_class,
	rt_hlf_free,
	rt_hlf_plot,
	rt_hlf_vshot,
	rt_hlf_tess,
	NULL,
	NULL,
	rt_hlf_import5,
	rt_hlf_export5,
	rt_hlf_import4,
	rt_hlf_export4,
	rt_hlf_ifree,
	NULL,
	rt_hlf_describe,
	rt_hlf_xform,
	rt_hlf_parse,
	sizeof(struct rt_half_internal),
	RT_HALF_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_hlf_params
    },

    {
	/* 7 */
	RT_FUNCTAB_MAGIC, "ID_REC", "rec",
	1,
	rt_rec_prep,
	rt_rec_shot,
	rt_rec_print,
	rt_rec_norm,
	NULL,
	NULL,
	rt_rec_uv,
	rt_rec_curve,
	rt_rec_class,
	rt_rec_free,
	rt_tgc_plot,
	rt_rec_vshot,
	rt_tgc_tess,
	NULL,
	rt_tgc_brep,
	rt_tgc_import5,
	rt_tgc_export5,
	rt_tgc_import4,
	rt_tgc_export4,
	rt_tgc_ifree,
	rt_rec_bbox,
	rt_tgc_describe,
	rt_generic_xform,
	rt_tgc_parse,
	sizeof(struct rt_tgc_internal),
	RT_TGC_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_rec_params
    },

    {
	/* 8 */
	RT_FUNCTAB_MAGIC, "ID_POLY", "poly",
	1,
	rt_pg_prep,
	rt_pg_shot,
	rt_pg_print,
	rt_pg_norm,
	NULL,
	NULL,
	rt_pg_uv,
	rt_pg_curve,
	rt_pg_class,
	rt_pg_free,
	rt_pg_plot,
	NULL,
	rt_pg_tess,
	NULL,
	NULL,
	NULL,
	NULL,
	rt_pg_import4,
	rt_pg_export4,
	rt_pg_ifree,
	NULL,
	rt_pg_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct rt_pg_internal),
	RT_PG_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_pg_params
    },

    {
	/* 9 */
	RT_FUNCTAB_MAGIC, "ID_BSPLINE", "bspline",
	1,
	rt_nurb_prep,
	rt_nurb_shot,
	rt_nurb_print,
	rt_nurb_norm,
	NULL,
	NULL,
	rt_nurb_uv,
	rt_nurb_curve,
	rt_nurb_class,
	rt_nurb_free,
	rt_nurb_plot,
	NULL,
	rt_nurb_tess,
	NULL,
	NULL,
	rt_nurb_import5,
	rt_nurb_export5,
	rt_nurb_import4,
	rt_nurb_export4,
	rt_nurb_ifree,
	NULL,
	rt_nurb_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct rt_nurb_internal),
	RT_NURB_INTERNAL_MAGIC,
	rt_nurb_get,
	rt_nurb_adjust,
	rt_generic_form,
	NULL,
	rt_nurb_params
    },

    {
	/* 10 */
	RT_FUNCTAB_MAGIC, "ID_SPH", "sph",
	0,
	rt_sph_prep,
	rt_sph_shot,
	rt_sph_print,
	rt_sph_norm,
	NULL,
	NULL,
	rt_sph_uv,
	rt_sph_curve,
	rt_sph_class,
	rt_sph_free,
	rt_ell_plot,
	rt_sph_vshot,
	rt_ell_tess,
	rt_ell_tnurb,
	rt_ell_brep,
	rt_ell_import5,
	rt_ell_export5,
	rt_ell_import4,
	rt_ell_export4,
	rt_ell_ifree,
	rt_ell_bbox,
	rt_ell_describe,
	rt_generic_xform,
	rt_ell_parse,
	sizeof(struct rt_ell_internal),
	RT_ELL_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_sph_params
    },

    {
	/* 11 */
	RT_FUNCTAB_MAGIC, "ID_NMG", "nmg",
	1,
	rt_nmg_prep,
	rt_nmg_shot,
	rt_nmg_print,
	rt_nmg_norm,
	NULL,
	NULL,
	rt_nmg_uv,
	rt_nmg_curve,
	rt_nmg_class,
	rt_nmg_free,
	rt_nmg_plot,
	NULL,
	rt_nmg_tess,
	NULL,
	rt_nmg_brep,
	rt_nmg_import5,
	rt_nmg_export5,
	rt_nmg_import4,
	rt_nmg_export4,
	rt_nmg_ifree,
	rt_nmg_bbox,
	rt_nmg_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct model),
	NMG_MODEL_MAGIC,
	rt_nmg_get,
	rt_nmg_adjust,
	rt_generic_form,
	rt_nmg_make,
	rt_nmg_params
    },

    {
	/* 12 */
	RT_FUNCTAB_MAGIC, "ID_EBM", "ebm",
	1,
	rt_ebm_prep,
	rt_ebm_shot,
	rt_ebm_print,
	rt_ebm_norm,
	NULL,
	NULL,
	rt_ebm_uv,
	rt_ebm_curve,
	rt_ebm_class,
	rt_ebm_free,
	rt_ebm_plot,
	NULL,
	rt_ebm_tess,
	NULL,
	rt_ebm_brep,
	rt_ebm_import5,
	rt_ebm_export5,
	rt_ebm_import4,
	rt_ebm_export4,
	rt_ebm_ifree,
	NULL,
	rt_ebm_describe,
	rt_generic_xform,
	rt_ebm_parse,
	sizeof(struct rt_ebm_internal),
	RT_EBM_INTERNAL_MAGIC,
	rt_ebm_get,
	rt_ebm_adjust,
	rt_ebm_form,
	rt_ebm_make,
	rt_ebm_params
    },

    {
	/* 13 */
	RT_FUNCTAB_MAGIC, "ID_VOL", "vol",
	1,
	rt_vol_prep,
	rt_vol_shot,
	rt_vol_print,
	rt_vol_norm,
	NULL,
	NULL,
	rt_vol_uv,
	rt_vol_curve,
	rt_vol_class,
	rt_vol_free,
	rt_vol_plot,
	NULL,
	rt_vol_tess,
	NULL,
	rt_vol_brep,
	rt_vol_import5,
	rt_vol_export5,
	rt_vol_import4,
	rt_vol_export4,
	rt_vol_ifree,
	NULL,
	rt_vol_describe,
	rt_generic_xform,
	rt_vol_parse,
	sizeof(struct rt_vol_internal),
	RT_VOL_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_vol_params
    },

    {
	/* 14 */
	RT_FUNCTAB_MAGIC, "ID_ARBN", "arbn",
	0,
	rt_arbn_prep,
	rt_arbn_shot,
	rt_arbn_print,
	rt_arbn_norm,
	NULL,
	NULL,
	rt_arbn_uv,
	rt_arbn_curve,
	rt_arbn_class,
	rt_arbn_free,
	rt_arbn_plot,
	NULL,
	rt_arbn_tess,
	NULL,
	rt_arbn_brep,
	rt_arbn_import5,
	rt_arbn_export5,
	rt_arbn_import4,
	rt_arbn_export4,
	rt_arbn_ifree,
	NULL,
	rt_arbn_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct rt_arbn_internal),
	RT_ARBN_INTERNAL_MAGIC,
	rt_arbn_get,
	rt_arbn_adjust,
	rt_generic_form,
	NULL,
	rt_arbn_params
    },

    {
	/* 15 */
	RT_FUNCTAB_MAGIC, "ID_PIPE", "pipe",
	1,
	rt_pipe_prep,
	rt_pipe_shot,
	rt_pipe_print,
	rt_pipe_norm,
	NULL,
	NULL,
	rt_pipe_uv,
	rt_pipe_curve,
	rt_pipe_class,
	rt_pipe_free,
	rt_pipe_plot,
	NULL,
	rt_pipe_tess,
	NULL,
	rt_pipe_brep,
	rt_pipe_import5,
	rt_pipe_export5,
	rt_pipe_import4,
	rt_pipe_export4,
	rt_pipe_ifree,
	NULL,
	rt_pipe_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct rt_pipe_internal),
	RT_PIPE_INTERNAL_MAGIC,
	rt_pipe_get,
	rt_pipe_adjust,
	rt_generic_form,
	NULL,
	rt_pipe_params
    },

    {
	/* 16 */
	RT_FUNCTAB_MAGIC, "ID_PARTICLE", "part",
	0,
	rt_part_prep,
	rt_part_shot,
	rt_part_print,
	rt_part_norm,
	NULL,
	NULL,
	rt_part_uv,
	rt_part_curve,
	rt_part_class,
	rt_part_free,
	rt_part_plot,
	NULL,
	rt_part_tess,
	NULL,
	NULL,
	rt_part_import5,
	rt_part_export5,
	rt_part_import4,
	rt_part_export4,
	rt_part_ifree,
	NULL,
	rt_part_describe,
	rt_generic_xform,
	rt_part_parse,
	sizeof(struct rt_part_internal),
	RT_PART_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_part_params
    },

    {
	/* 17 */
	RT_FUNCTAB_MAGIC, "ID_RPC", "rpc",
	0,
	rt_rpc_prep,
	rt_rpc_shot,
	rt_rpc_print,
	rt_rpc_norm,
	NULL,
	NULL,
	rt_rpc_uv,
	rt_rpc_curve,
	rt_rpc_class,
	rt_rpc_free,
	rt_rpc_plot,
	NULL,
	rt_rpc_tess,
	NULL,
	rt_rpc_brep,
	rt_rpc_import5,
	rt_rpc_export5,
	rt_rpc_import4,
	rt_rpc_export4,
	rt_rpc_ifree,
	NULL,
	rt_rpc_describe,
	rt_generic_xform,
	rt_rpc_parse,
	sizeof(struct rt_rpc_internal),
	RT_RPC_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_rpc_params
    },

    {
	/* 18 */
	RT_FUNCTAB_MAGIC, "ID_RHC", "rhc",
	0,
	rt_rhc_prep,
	rt_rhc_shot,
	rt_rhc_print,
	rt_rhc_norm,
	NULL,
	NULL,
	rt_rhc_uv,
	rt_rhc_curve,
	rt_rhc_class,
	rt_rhc_free,
	rt_rhc_plot,
	NULL,
	rt_rhc_tess,
	NULL,
	rt_rhc_brep,
	rt_rhc_import5,
	rt_rhc_export5,
	rt_rhc_import4,
	rt_rhc_export4,
	rt_rhc_ifree,
	NULL,
	rt_rhc_describe,
	rt_generic_xform,
	rt_rhc_parse,
	sizeof(struct rt_rhc_internal),
	RT_RHC_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_rhc_params
    },

    {
	/* 19 */
	RT_FUNCTAB_MAGIC, "ID_EPA", "epa",
	0,
	rt_epa_prep,
	rt_epa_shot,
	rt_epa_print,
	rt_epa_norm,
	NULL,
	NULL,
	rt_epa_uv,
	rt_epa_curve,
	rt_epa_class,
	rt_epa_free,
	rt_epa_plot,
	NULL,
	rt_epa_tess,
	NULL,
	rt_epa_brep,
	rt_epa_import5,
	rt_epa_export5,
	rt_epa_import4,
	rt_epa_export4,
	rt_epa_ifree,
	NULL,
	rt_epa_describe,
	rt_generic_xform,
	rt_epa_parse,
	sizeof(struct rt_epa_internal),
	RT_EPA_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_epa_params
    },

    {
	/* 20 */
	RT_FUNCTAB_MAGIC, "ID_EHY", "ehy",
	0,
	rt_ehy_prep,
	rt_ehy_shot,
	rt_ehy_print,
	rt_ehy_norm,
	NULL,
	NULL,
	rt_ehy_uv,
	rt_ehy_curve,
	rt_ehy_class,
	rt_ehy_free,
	rt_ehy_plot,
	NULL,
	rt_ehy_tess,
	NULL,
	rt_ehy_brep,
	rt_ehy_import5,
	rt_ehy_export5,
	rt_ehy_import4,
	rt_ehy_export4,
	rt_ehy_ifree,
	NULL,
	rt_ehy_describe,
	rt_generic_xform,
	rt_ehy_parse,
	sizeof(struct rt_ehy_internal),
	RT_EHY_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_ehy_params
    },

    {
	/* 21 */
	RT_FUNCTAB_MAGIC, "ID_ETO", "eto",
	1,
	rt_eto_prep,
	rt_eto_shot,
	rt_eto_print,
	rt_eto_norm,
	NULL,
	NULL,
	rt_eto_uv,
	rt_eto_curve,
	rt_eto_class,
	rt_eto_free,
	rt_eto_plot,
	NULL,
	rt_eto_tess,
	NULL,
	rt_eto_brep,
	rt_eto_import5,
	rt_eto_export5,
	rt_eto_import4,
	rt_eto_export4,
	rt_eto_ifree,
	NULL,
	rt_eto_describe,
	rt_generic_xform,
	rt_eto_parse,
	sizeof(struct rt_eto_internal),
	RT_ETO_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_eto_params
    },

    {
	/* 22 */
	RT_FUNCTAB_MAGIC, "ID_GRIP", "grip",
	1,
	rt_grp_prep,
	rt_grp_shot,
	rt_grp_print,
	rt_grp_norm,
	NULL,
	NULL,
	rt_grp_uv,
	rt_grp_curve,
	rt_grp_class,
	rt_grp_free,
	rt_grp_plot,
	NULL,
	rt_grp_tess,
	NULL,
	NULL,
	rt_grp_import5,
	rt_grp_export5,
	rt_grp_import4,
	rt_grp_export4,
	rt_grp_ifree,
	NULL,
	rt_grp_describe,
	rt_generic_xform,
	rt_grp_parse,
	sizeof(struct rt_grip_internal),
	RT_GRIP_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_grp_params
    },

    {
	/* 23 -- XXX unimplemented */
	RT_FUNCTAB_MAGIC, "ID_JOINT", "joint",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
    },

    {
	/* 24 */
	RT_FUNCTAB_MAGIC, "ID_HF", "hf",
	0,
	rt_hf_prep,
	rt_hf_shot,
	rt_hf_print,
	rt_hf_norm,
	NULL,
	NULL,
	rt_hf_uv,
	rt_hf_curve,
	rt_hf_class,
	rt_hf_free,
	rt_hf_plot,
	NULL,
	rt_hf_tess,
	NULL,
	NULL,
	rt_hf_import5,
	rt_hf_export5,
	rt_hf_import4,
	rt_hf_export4,
	rt_hf_ifree,
	NULL,
	rt_hf_describe,
	rt_generic_xform,
	rt_hf_parse,
	sizeof(struct rt_hf_internal),
	RT_HF_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_hf_params
    },

    {
	/* 25 Displacement Map (alt. height field) */
	RT_FUNCTAB_MAGIC, "ID_DSP", "dsp",
	1,
	rt_dsp_prep,
	rt_dsp_shot,
	rt_dsp_print,
	rt_dsp_norm,
	NULL,
	NULL,
	rt_dsp_uv,
	rt_dsp_curve,
	rt_dsp_class,
	rt_dsp_free,
	rt_dsp_plot,
	NULL,
	rt_dsp_tess,
	NULL,
	rt_dsp_brep,
	rt_dsp_import5,
	rt_dsp_export5,
	rt_dsp_import4,
	rt_dsp_export4,
	rt_dsp_ifree,
	NULL,
	rt_dsp_describe,
	rt_generic_xform,
	rt_dsp_parse,
	sizeof(struct rt_dsp_internal),
	RT_DSP_INTERNAL_MAGIC,
	rt_dsp_get,
	rt_dsp_adjust,
	NULL,
	rt_dsp_make,
	rt_dsp_params
    },

    {
	/* 26 2D sketch */
	RT_FUNCTAB_MAGIC, "ID_SKETCH", "sketch",
	0,
	rt_sketch_prep,
	rt_sketch_shot,
	rt_sketch_print,
	rt_sketch_norm,
	NULL,
	NULL,
	rt_sketch_uv,
	rt_sketch_curve,
	rt_sketch_class,
	rt_sketch_free,
	rt_sketch_plot,
	NULL,
	NULL,
	NULL,
	rt_sketch_brep,
	rt_sketch_import5,
	rt_sketch_export5,
	rt_sketch_import4,
	rt_sketch_export4,
	rt_sketch_ifree,
	NULL,
	rt_sketch_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct rt_sketch_internal),
	RT_SKETCH_INTERNAL_MAGIC,
	rt_sketch_get,
	rt_sketch_adjust,
	rt_sketch_form,
	NULL,
	rt_sketch_params
    },

    {
	/* 27 Solid of extrusion */
	RT_FUNCTAB_MAGIC, "ID_EXTRUDE", "extrude",
	1,
	rt_extrude_prep,
	rt_extrude_shot,
	rt_extrude_print,
	rt_extrude_norm,
	NULL,
	NULL,
	NULL,
	rt_extrude_curve,
	rt_extrude_class,
	rt_extrude_free,
	rt_extrude_plot,
	NULL,
	rt_extrude_tess,
	NULL,
	rt_extrude_brep,
	rt_extrude_import5,
	rt_extrude_export5,
	rt_extrude_import4,
	rt_extrude_export4,
	rt_extrude_ifree,
	NULL,
	rt_extrude_describe,
	rt_extrude_xform,
	NULL,
	sizeof(struct rt_extrude_internal),
	RT_EXTRUDE_INTERNAL_MAGIC,
	rt_extrude_get,
	rt_extrude_adjust,
	rt_extrude_form,
	NULL,
	rt_extrude_params
    },

    {
	/* 28 Instanced submodel */
	RT_FUNCTAB_MAGIC, "ID_SUBMODEL", "submodel",
	1,
	rt_submodel_prep,
	rt_submodel_shot,
	rt_submodel_print,
	rt_submodel_norm,
	NULL,
	NULL,
	rt_submodel_uv,
	rt_submodel_curve,
	rt_submodel_class,
	rt_submodel_free,
	rt_submodel_plot,
	NULL,
	rt_submodel_tess,
	NULL,
	NULL,
	rt_submodel_import5,
	rt_submodel_export5,
	rt_submodel_import4,
	rt_submodel_export4,
	rt_submodel_ifree,
	NULL,
	rt_submodel_describe,
	rt_generic_xform,
	rt_submodel_parse,
	sizeof(struct rt_submodel_internal),
	RT_SUBMODEL_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_submodel_params
    },

    {
	/* 29 Fastgen cline solid */
	RT_FUNCTAB_MAGIC, "ID_CLINE", "cline",
	0,
	rt_cline_prep,
	rt_cline_shot,
	rt_cline_print,
	rt_cline_norm,
	NULL,
	NULL,
	rt_cline_uv,
	rt_cline_curve,
	rt_cline_class,
	rt_cline_free,
	rt_cline_plot,
	NULL,
	rt_cline_tess,
	NULL,
	NULL,
	rt_cline_import5,
	rt_cline_export5,
	rt_cline_import4,
	rt_cline_export4,
	rt_cline_ifree,
	NULL,
	rt_cline_describe,
	rt_generic_xform,
	rt_cline_parse,
	sizeof(struct rt_cline_internal),
	RT_CLINE_INTERNAL_MAGIC,
	rt_cline_get,
	rt_cline_adjust,
	rt_cline_form,
	NULL,
	rt_cline_params
    },

    {
	/* 30 Bag o' Triangles */
	RT_FUNCTAB_MAGIC, "ID_BOT", "bot",
	0,
	rt_bot_prep,
	rt_bot_shot,
	rt_bot_print,
	rt_bot_norm,
	rt_bot_piece_shot,
	rt_bot_piece_hitsegs,
	rt_bot_uv,
	rt_bot_curve,
	rt_bot_class,
	rt_bot_free,
	rt_bot_plot,
	NULL,
	rt_bot_tess,
	NULL,
	rt_bot_brep,
	rt_bot_import5,
	rt_bot_export5,
	rt_bot_import4,
	rt_bot_export4,
	rt_bot_ifree,
	NULL,
	rt_bot_describe,
	rt_bot_xform,
	NULL,
	sizeof(struct rt_bot_internal),
	RT_BOT_INTERNAL_MAGIC,
	rt_bot_get,
	rt_bot_adjust,
	rt_bot_form,
	NULL,
	rt_bot_params
    },

    {
	/* 31 combination objects (should not be in this table) */
	RT_FUNCTAB_MAGIC, "ID_COMBINATION", "comb",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	rt_comb_import5,
	rt_comb_export5,
	rt_comb_import4,
	rt_comb_export4,
	rt_comb_ifree,
	NULL,
	rt_comb_describe,
	rt_generic_xform,
	NULL,
	0,
	0,
	rt_comb_get,
	rt_comb_adjust,
	rt_comb_form,
	rt_comb_make,
	NULL
    },

    {
	/* 32 available placeholder to not offset latter table indices
	 * (was ID_BINEXPM)
	 */
	RT_FUNCTAB_MAGIC, "ID_UNUSED1", "unused1",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
    },

    {
	/* 33 */
	RT_FUNCTAB_MAGIC, "ID_BINUNIF", "binunif",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	rt_binunif_export5,
	NULL,
	NULL,
	rt_binunif_ifree,
	NULL,
	rt_binunif_describe,
	rt_generic_xform,
	NULL,
	0,
	0,
	rt_binunif_get,
	rt_binunif_adjust,
	NULL,
	rt_binunif_make,
	NULL
    },

    {
	/* 34 available placeholder to not offset latter table indices
	 * (was ID_BINMIME)
	 */
	RT_FUNCTAB_MAGIC, "ID_UNUSED2", "unused2",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
    },

    {
	/* 35 (but "should" be 31) Superquadratic Ellipsoid */
	RT_FUNCTAB_MAGIC, "ID_SUPERELL", "superell",
	1,
	rt_superell_prep,
	rt_superell_shot,
	rt_superell_print,
	rt_superell_norm,
	NULL,
	NULL,
	rt_superell_uv,
	rt_superell_curve,
	rt_superell_class,
	rt_superell_free,
	rt_superell_plot,
	NULL,
	rt_superell_tess,
	NULL,
	NULL,
	rt_superell_import5,
	rt_superell_export5,
	rt_superell_import4,
	rt_superell_export4,
	rt_superell_ifree,
	NULL,
	rt_superell_describe,
	rt_generic_xform,
	rt_superell_parse,
	sizeof(struct rt_superell_internal),
	RT_SUPERELL_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_superell_params
    },

    {
	/* 36 (but "should" be 32) Metaball */
	RT_FUNCTAB_MAGIC, "ID_METABALL", "metaball",
	1,
	rt_metaball_prep,
	rt_metaball_shot,
	rt_metaball_print,
	rt_metaball_norm,
	NULL,
	NULL,
	rt_metaball_uv,
	rt_metaball_curve,
	rt_metaball_class,
	rt_metaball_free,
	rt_metaball_plot,
	NULL,
	rt_metaball_tess,
	NULL,
	NULL,
	rt_metaball_import5,
	rt_metaball_export5,
	NULL,
	NULL,
	rt_metaball_ifree,
	NULL,
	rt_metaball_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct rt_metaball_internal),
	RT_METABALL_INTERNAL_MAGIC,
	rt_metaball_get,
	rt_metaball_adjust,
	rt_generic_form,
	NULL,
	rt_metaball_params
    },

#if OBJ_BREP
    {
	/* 37 */
	RT_FUNCTAB_MAGIC, "ID_BREP", "brep",
	1,
	rt_brep_prep,
	rt_brep_shot,
	rt_brep_print,
	rt_brep_norm,
	NULL,
	NULL,
	rt_brep_uv,
	rt_brep_curve,
	rt_brep_class,
	rt_brep_free,
	rt_brep_plot,
	NULL,
	rt_brep_tess,
	NULL,
	NULL,
	rt_brep_import5,
	rt_brep_export5,
	NULL,
	NULL,
	rt_brep_ifree,
	NULL,
	rt_brep_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct rt_brep_internal),
	RT_BREP_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_brep_params
    },
#else
    {
	/* 37 this entry for placeholder (so table offsets are correct) */
	RT_FUNCTAB_MAGIC, "ID_BREP_PLCHLDR", "brep",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
    },
#endif

    {
	/* 38 (but "should" be 34) Hyperboloid */
	RT_FUNCTAB_MAGIC, "ID_HYP", "hyp",
	1,
	rt_hyp_prep,
	rt_hyp_shot,
	rt_hyp_print,
	rt_hyp_norm,
	NULL,
	NULL,
	rt_hyp_uv,
	rt_hyp_curve,
	NULL,
	rt_hyp_free,
	rt_hyp_plot,
	NULL,
	rt_hyp_tess,
	NULL,
	rt_hyp_brep,
	rt_hyp_import5,
	rt_hyp_export5,
	NULL,
	NULL,
	rt_hyp_ifree,
	NULL,
	rt_hyp_describe,
	rt_generic_xform,
	rt_hyp_parse,
	sizeof(struct rt_hyp_internal),
	RT_HYP_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	rt_hyp_params
    },

    {
	/* 39 */
	RT_FUNCTAB_MAGIC, "ID_CONSTRAINT", "constrnt",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	rt_constraint_import5,
	rt_constraint_export5,
	NULL,
	NULL,
	rt_constraint_ifree,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
    },

    {
	/* 40 */
	RT_FUNCTAB_MAGIC, "ID_REVOLVE", "revolve",
	1,
	rt_revolve_prep,
	rt_revolve_shot,
	rt_revolve_print,
	rt_revolve_norm,
	NULL,
	NULL,
	rt_revolve_uv,
	rt_revolve_curve,
	rt_revolve_class,
	rt_revolve_free,
	rt_revolve_plot,
	NULL,
	rt_revolve_tess,
	NULL,
	rt_revolve_brep,
	rt_revolve_import5,
	rt_revolve_export5,
	NULL,
	NULL,
	rt_revolve_ifree,
	NULL,
	rt_revolve_describe,
	rt_revolve_xform,
	rt_revolve_parse,
	sizeof(struct rt_revolve_internal),
	RT_REVOLVE_INTERNAL_MAGIC,
	rt_generic_get,
	rt_generic_adjust,
	rt_generic_form,
	NULL,
	NULL
    },

    {
	/* 41 */
	RT_FUNCTAB_MAGIC, "ID_PNTS", "pnts",
	0,
	NULL,
	NULL,
	rt_pnts_print,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	rt_pnts_plot,
	NULL,
	NULL,
	NULL,
	NULL,
	rt_pnts_import5,
	rt_pnts_export5,
	NULL,
	NULL,
	rt_pnts_ifree,
	NULL,
	rt_pnts_describe,
	rt_generic_xform,
	NULL,
	sizeof(struct rt_pnts_internal),
	RT_PNTS_INTERNAL_MAGIC,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
    },

    {
	/* this entry for sanity only */
	0L, ">ID_MAXIMUM", ">id_max",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
    }
};


/* Map for database solidrec objects to internal objects */
static char idmap[] = {
    ID_NULL,	/* undefined, 0 */
    ID_NULL,	/* RPP	1 axis-aligned rectangular parallelopiped */
    ID_NULL,	/* BOX	2 arbitrary rectangular parallelopiped */
    ID_NULL,	/* RAW	3 right-angle wedge */
    ID_NULL,	/* ARB4	4 tetrahedron */
    ID_NULL,	/* ARB5	5 pyramid */
    ID_NULL,	/* ARB6	6 extruded triangle */
    ID_NULL,	/* ARB7	7 weird 7-vertex shape */
    ID_NULL,	/* ARB8	8 hexahedron */
    ID_NULL,	/* ELL	9 ellipsoid */
    ID_NULL,	/* ELL1	10 another ellipsoid ? */
    ID_NULL,	/* SPH	11 sphere */
    ID_NULL,	/* RCC	12 right circular cylinder */
    ID_NULL,	/* REC	13 right elliptic cylinder */
    ID_NULL,	/* TRC	14 truncated regular cone */
    ID_NULL,	/* TEC	15 truncated elliptic cone */
    ID_TOR,	/* TOR	16 toroid */
    ID_NULL,	/* TGC	17 truncated general cone */
    ID_TGC,	/* GENTGC 18 supergeneralized TGC; internal form */
    ID_ELL,	/* GENELL 19: V, A, B, C */
    ID_ARB8,	/* GENARB8 20:  V, and 7 other vectors */
    ID_NULL,	/* HACK: ARS 21: arbitrary triangular-surfaced polyhedron */
    ID_NULL,	/* HACK: ARSCONT 22: extension record type for ARS solid */
    ID_NULL,	/* ELLG 23:  gift-only */
    ID_HALF,	/* HALFSPACE 24:  halfspace */
    ID_NULL,	/* HACK: SPLINE 25 */
    ID_RPC,	/* HACK: RPC 26: right parabolic cylinder */
    ID_RHC,	/* HACK: RHC 27: right hyperbolic cylinder */
    ID_EPA,	/* HACK: EPA 28: elliptical paraboloid */
    ID_EHY,	/* HACK: EHY 29: elliptical hyperboloid */
    ID_ETO,	/* HACK: ETO 29: elliptical torus */
    ID_GRIP,	/* HACK: GRP 30: grip pseudo solid */
    ID_NULL	/* n+1 */
};


/**
 * R T _ I D _ S O L I D
 *
 * Given a database record, determine the proper rt_functab subscript.
 * Used by MGED as well as internally to librt.
 *
 * Returns ID_xxx if successful, or ID_NULL upon failure.
 */
int
rt_id_solid(struct bu_external *ep)
{
    register union record *rec;
    register int id;

    BU_CK_EXTERNAL(ep);
    rec = (union record *)ep->ext_buf;

    switch (rec->u_id) {
	case ID_SOLID:
	    id = idmap[(int)(rec->s.s_type)];
	    break;
	case ID_ARS_A:
	    id = ID_ARS;
	    break;
	case ID_P_HEAD:
	    id = ID_POLY;
	    break;
	case ID_BSOLID:
	    id = ID_BSPLINE;
	    break;
	case DBID_STRSOL:
	    /* XXX This really needs to be some kind of table */
	    if (BU_STR_EQUAL(rec->ss.ss_keyword, "ebm")) {
		id = ID_EBM;
		break;
	    } else if (BU_STR_EQUAL(rec->ss.ss_keyword, "vol")) {
		id = ID_VOL;
		break;
	    } else if (BU_STR_EQUAL(rec->ss.ss_keyword, "hf")) {
		id = ID_HF;
		break;
	    } else if (BU_STR_EQUAL(rec->ss.ss_keyword, "dsp")) {
		id = ID_DSP;
		break;
	    } else if (BU_STR_EQUAL(rec->ss.ss_keyword, "submodel")) {
		id = ID_SUBMODEL;
		break;
	    }
	    bu_log("rt_id_solid(%s):  String solid type '%s' unknown\n",
		   rec->ss.ss_name, rec->ss.ss_keyword);
	    id = ID_NULL;		/* BAD */
	    break;
	case DBID_ARBN:
	    id = ID_ARBN;
	    break;
	case DBID_PIPE:
	    id = ID_PIPE;
	    break;
	case DBID_PARTICLE:
	    id = ID_PARTICLE;
	    break;
	case DBID_NMG:
	    id = ID_NMG;
	    break;
	case DBID_SKETCH:
	    id = ID_SKETCH;
	    break;
	case DBID_EXTR:
	    id = ID_EXTRUDE;
	    break;
	case DBID_CLINE:
	    id = ID_CLINE;
	    break;
	case DBID_BOT:
	    id = ID_BOT;
	    break;
	default:
	    bu_log("rt_id_solid:  u_id=x%x unknown\n", rec->u_id);
	    id = ID_NULL;		/* BAD */
	    break;
    }
    if (id < ID_NULL || id > ID_MAX_SOLID) {
	bu_log("rt_id_solid: internal error, id=%d?\n", id);
	id = ID_NULL;		/* very BAD */
    }
    return id;
}


/**
 * R T _ G E T _ F U N C T A B _ B Y _ L A B E L
 *
 * Given the Tcl 'label' for a given solid type, find the appropriate
 * entry in rt_functab[].
 */
const struct rt_functab *
rt_get_functab_by_label(const char *label)
{
    register const struct rt_functab *ftp;

    for (ftp = rt_functab; ftp->magic != 0; ftp++) {
	if (strncmp(label, ftp->ft_label, 8) == 0)
	    return ftp;
    }
    return NULL;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
