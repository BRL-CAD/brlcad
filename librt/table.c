/*
 *			T A B L E . C
 *
 *  Tables for the BRL-CAD Package ray-tracing library "librt".
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCStree[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "./debug.h"

#if __STDC__ && !defined(alliant)
# define RT_DECLARE_INTERFACE(name)	\
	RT_EXTERN(int rt_##name##_prep, (struct soltab *stp, \
			struct rt_db_internal *ip, struct rt_i *rtip )); \
	RT_EXTERN(int rt_##name##_shot, (struct soltab *stp,\
			register struct xray *rp, \
			struct application *ap, struct seg *seghead )); \
	RT_EXTERN(void rt_##name##_print, (CONST struct soltab *stp)); \
	RT_EXTERN(void rt_##name##_norm, (struct hit *hitp, \
			struct soltab *stp, struct xray *rp)); \
	RT_EXTERN(void rt_##name##_uv, (struct application *ap, \
			struct soltab *stp, struct hit *hitp, \
			struct uvcoord *uvp)); \
	RT_EXTERN(void rt_##name##_curve, (struct curvature *cvp, \
			struct hit *hitp, struct soltab *stp)); \
	RT_EXTERN(int rt_##name##_class, ()); \
	RT_EXTERN(void rt_##name##_free, (struct soltab *stp)); \
	RT_EXTERN(int rt_##name##_plot, (struct rt_list *vhead, \
			struct rt_db_internal *ip, \
			CONST struct rt_tess_tol *ttol, \
			CONST struct rt_tol *tol)); \
	RT_EXTERN(void rt_##name##_vshot, (struct soltab *stp[], \
			struct xray *rp[], \
			struct seg segp[], int n, struct application *ap )); \
	RT_EXTERN(int rt_##name##_tess, (struct nmgregion **r, \
			struct model *m, struct rt_db_internal *ip, \
			CONST struct rt_tess_tol *ttol, \
			CONST struct rt_tol *tol)); \
	RT_EXTERN(int rt_##name##_tnurb, (struct nmgregion **r, \
			struct model *m, struct rt_db_internal *ip, \
			CONST struct rt_tol *tol)); \
	RT_EXTERN(int rt_##name##_import, (struct rt_db_internal *ip, \
			CONST struct rt_external *ep, CONST mat_t mat)); \
	RT_EXTERN(int rt_##name##_export, (struct rt_external *ep, \
			CONST struct rt_db_internal *ip, \
			double local2mm)); \
	RT_EXTERN(void rt_##name##_ifree, (struct rt_db_internal *ip)); \
	RT_EXTERN(int rt_##name##_describe, (struct rt_vls *str, \
			struct rt_db_internal *ip, int verbose, \
			double mm2local)); \
	RT_EXTERN(int rt_##name##_xform, (struct rt_db_internal *op, \
			CONST mat_t mat, struct rt_db_internal *ip, \
			int free));
#else
# define RT_DECLARE_INTERFACE(name)	\
	RT_EXTERN(int rt_/**/name/**/_prep, (struct soltab *stp, \
			struct rt_db_internal *ip, struct rt_i *rtip )); \
	RT_EXTERN(int rt_/**/name/**/_shot, (struct soltab *stp, struct xray *rp, \
			struct application *ap, struct seg *seghead )); \
	RT_EXTERN(void rt_/**/name/**/_print, (CONST struct soltab *stp)); \
	RT_EXTERN(void rt_/**/name/**/_norm, (struct hit *hitp, \
			struct soltab *stp, struct xray *rp)); \
	RT_EXTERN(void rt_/**/name/**/_uv, (struct application *ap, \
			struct soltab *stp, struct hit *hitp, \
			struct uvcoord *uvp)); \
	RT_EXTERN(void rt_/**/name/**/_curve, (struct curvature *cvp, \
			struct hit *hitp, struct soltab *stp)); \
	RT_EXTERN(int rt_/**/name/**/_class, ()); \
	RT_EXTERN(void rt_/**/name/**/_free, (struct soltab *stp)); \
	RT_EXTERN(int rt_/**/name/**/_plot, (struct rt_list *vhead, \
			struct rt_db_internal *ip, \
			CONST struct rt_tess_tol *ttol, \
			CONST struct rt_tol *tol)); \
	RT_EXTERN(void rt_/**/name/**/_vshot, (struct soltab *stp[], \
			struct xray *rp[], \
			struct seg segp[], int n, struct application *ap )); \
	RT_EXTERN(int rt_/**/name/**/_tess, (struct nmgregion **r, \
			struct model *m, struct rt_db_internal *ip, \
			CONST struct rt_tess_tol *ttol, \
			CONST struct rt_tol *tol)); \
	RT_EXTERN(int rt_/**/name/**/_tnurb, (struct nmgregion **r, \
			struct model *m, struct rt_db_internal *ip, \
			CONST struct rt_tol *tol)); \
	RT_EXTERN(int rt_/**/name/**/_import, (struct rt_db_internal *ip, \
			CONST struct rt_external *ep, CONST mat_t mat)); \
	RT_EXTERN(int rt_/**/name/**/_export, (struct rt_external *ep, \
			CONST struct rt_db_internal *ip, \
			double local2mm)); \
	RT_EXTERN(void rt_/**/name/**/_ifree, (struct rt_db_internal *ip)); \
	RT_EXTERN(int rt_/**/name/**/_describe, (struct rt_vls *str, \
			struct rt_db_internal *ip, int verbose, \
			double mm2local)); \
	RT_EXTERN(int rt_/**/name/**/_xform, (struct rt_db_internal *op, \
			CONST mat_t mat, struct rt_db_internal *ip, \
			int free));
#endif

/* Note:  no semi-colons at the end of these, please */	
RT_DECLARE_INTERFACE(nul)
#define rt_tor_xform rt_generic_xform
RT_DECLARE_INTERFACE(tor)
#define rt_tgc_xform rt_generic_xform
RT_DECLARE_INTERFACE(tgc)
#define rt_ell_xform rt_generic_xform
RT_DECLARE_INTERFACE(ell)
#define rt_arb_xform rt_generic_xform
RT_DECLARE_INTERFACE(arb)
#define rt_ars_xform rt_generic_xform
RT_DECLARE_INTERFACE(ars)
RT_DECLARE_INTERFACE(hlf)
#define rt_rec_xform rt_generic_xform
RT_DECLARE_INTERFACE(rec)
#define rt_pg_xform rt_generic_xform
RT_DECLARE_INTERFACE(pg)
#define rt_nurb_xform rt_generic_xform
RT_DECLARE_INTERFACE(nurb)
#define rt_sph_xform rt_generic_xform
RT_DECLARE_INTERFACE(sph)
#define rt_ebm_xform rt_generic_xform
RT_DECLARE_INTERFACE(ebm)
#define rt_vol_xform rt_generic_xform
RT_DECLARE_INTERFACE(vol)
#define rt_arbn_xform rt_generic_xform
RT_DECLARE_INTERFACE(arbn)
#define rt_pipe_xform rt_generic_xform
RT_DECLARE_INTERFACE(pipe)
#define rt_part_xform rt_generic_xform
RT_DECLARE_INTERFACE(part)
#define rt_nmg_xform rt_generic_xform
RT_DECLARE_INTERFACE(nmg)
#define rt_rpc_xform rt_generic_xform
RT_DECLARE_INTERFACE(rpc)
#define rt_rhc_xform rt_generic_xform
RT_DECLARE_INTERFACE(rhc)
#define rt_epa_xform rt_generic_xform
RT_DECLARE_INTERFACE(epa)
#define rt_ehy_xform rt_generic_xform
RT_DECLARE_INTERFACE(ehy)
#define rt_eto_xform rt_generic_xform
RT_DECLARE_INTERFACE(eto)
#define rt_grp_xform rt_generic_xform
RT_DECLARE_INTERFACE(grp)
#define rt_hf_xform rt_generic_xform
RT_DECLARE_INTERFACE(hf)

/* from db_comb.c */
RT_EXTERN(int rt_comb_import, (struct rt_db_internal *ip,
		CONST struct rt_external *ep, CONST mat_t mat));
RT_EXTERN(int rt_comb_export, (struct rt_external *ep,
		CONST struct rt_db_internal *ip,
		double local2mm));
RT_EXTERN(void rt_comb_ifree, (struct rt_db_internal *ip));
RT_EXTERN(int rt_comb_describe, (struct rt_vls *str,
		struct rt_db_internal *ip, int verbose,
		double mm2local));

/* XXX from shoot.c / vshoot.c */
RT_EXTERN(void rt_vstub, (struct soltab *stp[], struct xray *rp[],
	struct seg segp[], int n, struct application *ap ));
RT_EXTERN(int rt_generic_xform, (struct rt_db_internal *op, 
	CONST mat_t mat, struct rt_db_internal *ip,
	int free));

struct rt_functab rt_functab[ID_MAXIMUM+3] = {
	"ID_NULL",	0,		/* 0 */
		rt_nul_prep,	rt_nul_shot,	rt_nul_print, 	rt_nul_norm,
	 	rt_nul_uv,	rt_nul_curve,	rt_nul_class,	rt_nul_free,
		rt_nul_plot,	rt_nul_vshot,	rt_nul_tess,	rt_nul_tnurb,
		rt_nul_import,	rt_nul_export,	rt_nul_ifree,
		rt_nul_describe,rt_nul_xform,
		
	"ID_TOR",	1,		/* 1 */
		rt_tor_prep,	rt_tor_shot,	rt_tor_print,	rt_tor_norm,
		rt_tor_uv,	rt_tor_curve,	rt_tor_class,	rt_tor_free,
		rt_tor_plot,	rt_tor_vshot,	rt_tor_tess,	rt_nul_tnurb,
		rt_tor_import,	rt_tor_export,	rt_tor_ifree,
		rt_tor_describe,rt_tor_xform,

	"ID_TGC",	1,		/* 2 */
		rt_tgc_prep,	rt_tgc_shot,	rt_tgc_print,	rt_tgc_norm,
		rt_tgc_uv,	rt_tgc_curve,	rt_tgc_class,	rt_tgc_free,
		rt_tgc_plot,	rt_tgc_vshot,	rt_tgc_tess,	rt_tgc_tnurb,
		rt_tgc_import,	rt_tgc_export,	rt_tgc_ifree,
		rt_tgc_describe,rt_tgc_xform,

	"ID_ELL",	1,		/* 3 */
		rt_ell_prep,	rt_ell_shot,	rt_ell_print,	rt_ell_norm,
		rt_ell_uv,	rt_ell_curve,	rt_ell_class,	rt_ell_free,
		rt_ell_plot,	rt_ell_vshot,	rt_ell_tess,	rt_ell_tnurb,
		rt_ell_import,	rt_ell_export,	rt_ell_ifree,
		rt_ell_describe,rt_ell_xform,

	"ID_ARB8",	0,		/* 4 */
		rt_arb_prep,	rt_arb_shot,	rt_arb_print,	rt_arb_norm,
		rt_arb_uv,	rt_arb_curve,	rt_arb_class,	rt_arb_free,
		rt_arb_plot,	rt_arb_vshot,	rt_arb_tess,	rt_arb_tnurb,
		rt_arb_import,	rt_arb_export,	rt_arb_ifree,
		rt_arb_describe,rt_arb_xform,

	"ID_ARS",	1,		/* 5 */
		rt_ars_prep,	rt_ars_shot,	rt_ars_print,	rt_ars_norm,
		rt_ars_uv,	rt_ars_curve,	rt_ars_class,	rt_ars_free,
		rt_ars_plot,	rt_vstub,	rt_ars_tess,	rt_nul_tnurb,
		rt_ars_import,	rt_ars_export,	rt_ars_ifree,
		rt_ars_describe,rt_ars_xform,

	"ID_HALF",	0,		/* 6 */
		rt_hlf_prep,	rt_hlf_shot,	rt_hlf_print,	rt_hlf_norm,
		rt_hlf_uv,	rt_hlf_curve,	rt_hlf_class,	rt_hlf_free,
		rt_hlf_plot,	rt_hlf_vshot,	rt_hlf_tess,	rt_nul_tnurb,
		rt_hlf_import,	rt_hlf_export,	rt_hlf_ifree,
		rt_hlf_describe,rt_generic_xform,

	"ID_REC",	1,		/* 7 */
		rt_rec_prep,	rt_rec_shot,	rt_rec_print,	rt_rec_norm,
		rt_rec_uv,	rt_rec_curve,	rt_rec_class,	rt_rec_free,
		rt_tgc_plot,	rt_rec_vshot,	rt_tgc_tess,	rt_nul_tnurb,
		rt_tgc_import,	rt_tgc_export,	rt_tgc_ifree,
		rt_tgc_describe,rt_rec_xform,

	"ID_POLY",	1,		/* 8 */
		rt_pg_prep,	rt_pg_shot,	rt_pg_print,	rt_pg_norm,
		rt_pg_uv,	rt_pg_curve,	rt_pg_class,	rt_pg_free,
		rt_pg_plot,	rt_vstub,	rt_pg_tess,	rt_nul_tnurb,
		rt_pg_import,	rt_pg_export,	rt_pg_ifree,
		rt_pg_describe, rt_pg_xform,

	"ID_BSPLINE",	1,		/* 9 */
		rt_nurb_prep,	rt_nurb_shot,	rt_nurb_print,	rt_nurb_norm,
		rt_nurb_uv,	rt_nurb_curve,	rt_nurb_class,	rt_nurb_free,
		rt_nurb_plot,	rt_vstub,	rt_nurb_tess,	rt_nul_tnurb,
		rt_nurb_import,	rt_nurb_export,	rt_nurb_ifree,
		rt_nurb_describe,rt_nurb_xform,

	"ID_SPH",	1,		/* 10 */
		rt_sph_prep,	rt_sph_shot,	rt_sph_print,	rt_sph_norm,
		rt_sph_uv,	rt_sph_curve,	rt_sph_class,	rt_sph_free,
		rt_ell_plot,	rt_sph_vshot,	rt_ell_tess,	rt_ell_tnurb,
		rt_ell_import,	rt_ell_export,	rt_ell_ifree,
		rt_ell_describe,rt_sph_xform,

	"ID_NMG",	1,		/* 11 */
		rt_nmg_prep,	rt_nmg_shot,	rt_nmg_print,	rt_nmg_norm,
		rt_nmg_uv,	rt_nmg_curve,	rt_nmg_class,	rt_nmg_free,
		rt_nmg_plot,	rt_nmg_vshot,	rt_nmg_tess,	rt_nul_tnurb,
		rt_nmg_import,	rt_nmg_export,	rt_nmg_ifree,
		rt_nmg_describe,rt_nmg_xform,

	"ID_EBM",	0,		/* 12 */
		rt_ebm_prep,	rt_ebm_shot,	rt_ebm_print,	rt_ebm_norm,
		rt_ebm_uv,	rt_ebm_curve,	rt_ebm_class,	rt_ebm_free,
		rt_ebm_plot,	rt_vstub,	rt_ebm_tess,	rt_nul_tnurb,
		rt_ebm_import,	rt_ebm_export,	rt_ebm_ifree,
		rt_ebm_describe,rt_ebm_xform,

	"ID_VOL",	0,		/* 13 */
		rt_vol_prep,	rt_vol_shot,	rt_vol_print,	rt_vol_norm,
		rt_vol_uv,	rt_vol_curve,	rt_vol_class,	rt_vol_free,
		rt_vol_plot,	rt_vstub,	rt_vol_tess,	rt_nul_tnurb,
		rt_vol_import,	rt_vol_export,	rt_vol_ifree,
		rt_vol_describe,rt_vol_xform,

	"ID_ARBN",	0,		/* 14 */
		rt_arbn_prep,	rt_arbn_shot,	rt_arbn_print,	rt_arbn_norm,
		rt_arbn_uv,	rt_arbn_curve,	rt_arbn_class,	rt_arbn_free,
		rt_arbn_plot,	rt_arbn_vshot,	rt_arbn_tess,	rt_nul_tnurb,
		rt_arbn_import,	rt_arbn_export,	rt_arbn_ifree,
		rt_arbn_describe,rt_arbn_xform,

	"ID_PIPE",	0,		/* 15 */
		rt_pipe_prep,	rt_pipe_shot,	rt_pipe_print,	rt_pipe_norm,
		rt_pipe_uv,	rt_pipe_curve,	rt_pipe_class,	rt_pipe_free,
		rt_pipe_plot,	rt_pipe_vshot,	rt_pipe_tess,	rt_nul_tnurb,
		rt_pipe_import,	rt_pipe_export,	rt_pipe_ifree,
		rt_pipe_describe,rt_pipe_xform,

	"ID_PARTICLE",	0,		/* 16 */
		rt_part_prep,	rt_part_shot,	rt_part_print,	rt_part_norm,
		rt_part_uv,	rt_part_curve,	rt_part_class,	rt_part_free,
		rt_part_plot,	rt_part_vshot,	rt_part_tess,	rt_nul_tnurb,
		rt_part_import,	rt_part_export,	rt_part_ifree,
		rt_part_describe,rt_part_xform,

	"ID_RPC",	0,		/* 17 */
		rt_rpc_prep,	rt_rpc_shot,	rt_rpc_print,	rt_rpc_norm,
		rt_rpc_uv,	rt_rpc_curve,	rt_rpc_class,	rt_rpc_free,
		rt_rpc_plot,	rt_rpc_vshot,	rt_rpc_tess,	rt_nul_tnurb,
		rt_rpc_import,	rt_rpc_export,	rt_rpc_ifree,
		rt_rpc_describe,rt_rpc_xform,

	"ID_RHC",	0,		/* 18 */
		rt_rhc_prep,	rt_rhc_shot,	rt_rhc_print,	rt_rhc_norm,
		rt_rhc_uv,	rt_rhc_curve,	rt_rhc_class,	rt_rhc_free,
		rt_rhc_plot,	rt_rhc_vshot,	rt_rhc_tess,	rt_nul_tnurb,
		rt_rhc_import,	rt_rhc_export,	rt_rhc_ifree,
		rt_rhc_describe,rt_rhc_xform,

	"ID_EPA",	0,		/* 19 */
		rt_epa_prep,	rt_epa_shot,	rt_epa_print,	rt_epa_norm,
		rt_epa_uv,	rt_epa_curve,	rt_epa_class,	rt_epa_free,
		rt_epa_plot,	rt_epa_vshot,	rt_epa_tess,	rt_nul_tnurb,
		rt_epa_import,	rt_epa_export,	rt_epa_ifree,
		rt_epa_describe,rt_epa_xform,

	"ID_EHY",	0,		/* 20 */
		rt_ehy_prep,	rt_ehy_shot,	rt_ehy_print,	rt_ehy_norm,
		rt_ehy_uv,	rt_ehy_curve,	rt_ehy_class,	rt_ehy_free,
		rt_ehy_plot,	rt_ehy_vshot,	rt_ehy_tess,	rt_nul_tnurb,
		rt_ehy_import,	rt_ehy_export,	rt_ehy_ifree,
		rt_ehy_describe,rt_ehy_xform,

	"ID_ETO",	0,		/* 21 */
		rt_eto_prep,	rt_eto_shot,	rt_eto_print,	rt_eto_norm,
		rt_eto_uv,	rt_eto_curve,	rt_eto_class,	rt_eto_free,
		rt_eto_plot,	rt_eto_vshot,	rt_eto_tess,	rt_nul_tnurb,
		rt_eto_import,	rt_eto_export,	rt_eto_ifree,
		rt_eto_describe,rt_eto_xform,

	"ID_GRIP",	0,		/* 22 */
		rt_grp_prep,	rt_grp_shot,	rt_grp_print,	rt_grp_norm,
		rt_grp_uv,	rt_grp_curve,	rt_grp_class,	rt_grp_free,
		rt_grp_plot,	rt_grp_vshot,	rt_grp_tess,	rt_nul_tnurb,
		rt_grp_import,	rt_grp_export,	rt_grp_ifree,
		rt_grp_describe,rt_grp_xform,

	"ID_JOINT",	0,		/* 23 -- XXX unimplemented */
		rt_nul_prep,	rt_nul_shot,	rt_nul_print,	rt_nul_norm,
		rt_nul_uv,	rt_nul_curve,	rt_nul_class,	rt_nul_free,
		rt_nul_plot,	rt_nul_vshot,	rt_nul_tess,	rt_nul_tnurb,
		rt_nul_import,	rt_nul_export,	rt_nul_ifree,
		rt_nul_describe,rt_nul_xform,

	"ID_HF",	0,		/* 24 */
		rt_hf_prep,	rt_hf_shot,	rt_hf_print,	rt_hf_norm,
		rt_hf_uv,	rt_hf_curve,	rt_hf_class,	rt_hf_free,
		rt_hf_plot,	rt_vstub,	rt_hf_tess,	rt_nul_tnurb,
		rt_hf_import,	rt_hf_export,	rt_hf_ifree,
		rt_hf_describe,rt_hf_xform,

	/* ID_MAXIMUM.  Add new solids _above_ this point */

	"ID_COMBINATION",	0,
		rt_nul_prep,	rt_nul_shot,	rt_nul_print,	rt_nul_norm,
		rt_nul_uv,	rt_nul_curve,	rt_nul_class,	rt_nul_free,
		rt_nul_plot,	rt_nul_vshot,	rt_nul_tess,	rt_nul_tnurb,
		rt_comb_import,	rt_comb_export,	rt_comb_ifree,
		rt_comb_describe,rt_generic_xform,


	">ID_MAXIMUM",	0,
		rt_nul_prep,	rt_nul_shot,	rt_nul_print,	rt_nul_norm,
		rt_nul_uv,	rt_nul_curve,	rt_nul_class,	rt_nul_free,
		rt_nul_plot,	rt_nul_vshot,	rt_nul_tess,	rt_nul_tnurb,
		rt_nul_import,	rt_nul_export,	rt_nul_ifree,
		rt_nul_describe,rt_nul_xform
};
int rt_nfunctab = sizeof(rt_functab)/sizeof(struct rt_functab);

/*
 *  Hooks for unimplemented routines
 */
#if __STDC__
#define DEF(func,args)	func RT_ARGS(args) { \
	rt_log(#func " unimplemented\n"); return; }
#define IDEF(func,args)	func RT_ARGS(args) { \
	rt_log(#func " unimplemented\n"); return(0); }
#define NDEF(func,args)	func RT_ARGS(args) { \
	rt_log(#func " unimplemented\n"); return(-1); }
#else
#define DEF(func,args)	func RT_ARGS(args) { \
	rt_log("func unimplemented\n"); return; }
#define IDEF(func,args)	func RT_ARGS(args) { \
	rt_log("func unimplemented\n"); return(0); }
#define NDEF(func,args)	func RT_ARGS(args) { \
	rt_log("func unimplemented\n"); return(-1); }
#endif

int IDEF(rt_nul_prep,(struct soltab *stp,
			struct rt_db_internal *ip,
			struct rt_i *rtip))
int IDEF(rt_nul_shot,(struct soltab *stp,
			struct xray *rp,
			struct application *ap,
			struct seg *seghead))
void DEF(rt_nul_print,(CONST struct soltab *stp))
void DEF(rt_nul_norm,(struct hit *hitp,
			struct soltab *stp,
			struct xray *rp))
void DEF(rt_nul_uv,(struct application *ap,
			struct soltab *stp,
			struct hit *hitp,
			struct uvcoord *uvp))
void DEF(rt_nul_curve,(struct curvature *cvp,
			struct hit *hitp,
			struct soltab *stp))
int IDEF(rt_nul_class,())
void DEF(rt_nul_free,(struct soltab *stp))
int NDEF(rt_nul_plot,(struct rt_list *vhead,
			struct rt_db_internal *ip,
			CONST struct rt_tess_tol *ttol,
			CONST struct rt_tol *tol))
void DEF(rt_nul_vshot,(struct soltab *stp[],
			struct xray *rp[],
			struct seg segp[], int n,
			struct application *ap))
int NDEF(rt_nul_tess,(struct nmgregion **r,
			struct model *m,
			struct rt_db_internal *ip,
			CONST struct rt_tess_tol *ttol,
			CONST struct rt_tol *tol))
int NDEF(rt_nul_tnurb,(struct nmgregion **r,
			struct model *m,
			struct rt_db_internal *ip,
			CONST struct rt_tol *tol))
int NDEF(rt_nul_import,(struct rt_db_internal *ip,
			CONST struct rt_external *ep,
			CONST mat_t mat))
int NDEF(rt_nul_export,(struct rt_external *ep,
			CONST struct rt_db_internal *ip,
			double local2mm))
void DEF(rt_nul_ifree,(struct rt_db_internal *ip))
int NDEF(rt_nul_describe,(struct rt_vls *str,
			struct rt_db_internal *ip,
			int verbose, double mm2local))
int NDEF(rt_nul_xform, (struct rt_db_internal *op,
			CONST mat_t mat, struct rt_db_internal *ip,
			int free))

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
	ID_TOR,		/* TOR	16 toroid */
	ID_NULL,	/* TGC	17 truncated general cone */
	ID_TGC,		/* GENTGC 18 supergeneralized TGC; internal form */
	ID_ELL,		/* GENELL 19: V,A,B,C */
	ID_ARB8,	/* GENARB8 20:  V, and 7 other vectors */
	ID_NULL,	/* HACK: ARS 21: arbitrary triangular-surfaced polyhedron */
	ID_NULL,	/* HACK: ARSCONT 22: extension record type for ARS solid */
	ID_NULL,	/* ELLG 23:  gift-only */
	ID_HALF,	/* HALFSPACE 24:  halfspace */
	ID_NULL,	/* HACK: SPLINE 25 */
	ID_RPC,		/* HACK: RPC 26: right parabolic cylinder */
	ID_RHC,		/* HACK: RHC 27: right hyperbolic cylinder */
	ID_EPA,		/* HACK: EPA 28: elliptical paraboloid */
	ID_EHY,		/* HACK: EHY 29: elliptical hyperboloid */
	ID_ETO,		/* HACK: ETO 29: elliptical torus */
	ID_GRIP,	/* HACK: GRP 30: grip pseudo solid */
	ID_NULL		/* n+1 */
};

/*
 *			R T _ I D _ S O L I D
 *
 *  Given a database record, determine the proper rt_functab subscript.
 *  Used by MGED as well as internally to librt.
 *
 *  Returns ID_xxx if successful, or ID_NULL upon failure.
 */
int
rt_id_solid( ep )
struct rt_external	*ep;
{
	register union record *rec;
	register int id;

	RT_CK_EXTERNAL( ep );
	rec = (union record *)ep->ext_buf;

	switch( rec->u_id )  {
	case ID_SOLID:
		id = idmap[rec->s.s_type];
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
		if( strcmp( rec->ss.ss_keyword, "ebm" ) == 0 )  {
			id = ID_EBM;
			break;
		} else if( strcmp( rec->ss.ss_keyword, "vol" ) == 0 )  {
			id = ID_VOL;
			break;
		} else if( strcmp( rec->ss.ss_keyword, "hf" ) == 0 )  {
			id = ID_HF;
			break;
		}
		rt_log("rt_id_solid(%s):  String solid type '%s' unknown\n",
			rec->ss.ss_name, rec->ss.ss_keyword );
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
	default:
		rt_log("rt_id_solid:  u_id=x%x unknown\n", rec->u_id);
		id = ID_NULL;		/* BAD */
		break;
	}
	if( id < ID_NULL || id > ID_MAXIMUM )  {
		rt_log("rt_id_solid: internal error, id=%d?\n", id);
		id = ID_NULL;		/* very BAD */
	}
	return(id);
}

/*
 *			R T _ G E N E R I C _ X F O R M
 *
 *  Apply a 4x4 transformation matrix to the internal form of a solid.
 *
 *  If "free" flag is non-zero, storage for the original solid is released.
 *  If "os" is same as "is", storage for the original solid is
 *  overwritten with the new, transformed solid.
 *
 *
 *  Returns -
 *	-1	FAIL
 *	 0	OK
 */
int
rt_generic_xform(op, mat, ip, free)
struct rt_db_internal	*op;
CONST mat_t		mat;
struct rt_db_internal	*ip;
int			free;
{
	struct rt_external	ext;
	int			id;

	RT_CK_DB_INTERNAL( ip );
	id = ip->idb_type;
	RT_INIT_EXTERNAL(&ext);
	/* Scale change on export is 1.0 -- no change */
	if( rt_functab[id].ft_export( &ext, ip, 1.0 ) < 0 )  {
		rt_log("rt_generic_xform():  %s export failure\n",
			rt_functab[id].ft_name);
		return -1;			/* FAIL */
	}
	if( (free || op == ip) && ip->idb_ptr )  {
		rt_functab[id].ft_ifree( ip );
    		ip->idb_ptr = (genptr_t)0;
    	}

	if( rt_functab[id].ft_import( op, &ext, mat ) < 0 )  {
		rt_log("rt_generic_xform():  solid import failure\n");
		return -1;			/* FAIL */
	}

	db_free_external( &ext );

	RT_CK_DB_INTERNAL( op );
	return 0;				/* OK */
}
