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

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"

struct rt_g rt_g;				/* All global state */

extern int	nul_prep(), nul_class();
extern int	tor_prep(), tor_class();
extern int	tgc_prep(), tgc_class();
extern int	ell_prep(), ell_class();
extern int	arb_prep(), arb_class();
extern int	hlf_prep(), hlf_class();
extern int	ars_prep(), ars_class();
extern int	rec_prep(), rec_class();
extern int	pg_prep(), pg_class();
extern int	spl_prep(), spl_class();
extern int	sph_prep(), sph_class();
extern int	ebm_prep(), ebm_class();

extern void	nul_print(), nul_norm(), nul_uv();
extern void	tor_print(), tor_norm(), tor_uv();
extern void	tgc_print(), tgc_norm(), tgc_uv();
extern void	ell_print(), ell_norm(), ell_uv();
extern void	arb_print(), arb_norm(), arb_uv();
extern void	hlf_print(), hlf_norm(), hlf_uv();
extern void	ars_print(), ars_norm(), ars_uv();
extern void	rec_print(), rec_norm(), rec_uv();
extern void	pg_print(),  pg_norm(),  pg_uv();
extern void	spl_print(), spl_norm(), spl_uv();
extern void	sph_print(), sph_norm(), sph_uv();
extern void	ebm_print(), ebm_norm(), ebm_uv();

extern void	nul_curve(), nul_free(), nul_plot();
extern void	tor_curve(), tor_free(), tor_plot();
extern void	tgc_curve(), tgc_free(), tgc_plot();
extern void	ell_curve(), ell_free(), ell_plot();
extern void	arb_curve(), arb_free(), arb_plot();
extern void	hlf_curve(), hlf_free(), hlf_plot();
extern void	ars_curve(), ars_free(), ars_plot();
extern void	rec_curve(), rec_free(), rec_plot();
extern void	pg_curve(),  pg_free(),  pg_plot();
extern void	spl_curve(), spl_free(), spl_plot();
extern void	sph_curve(), sph_free(), sph_plot();
extern void	ebm_curve(), ebm_free(), ebm_plot();

extern struct seg *nul_shot();
extern struct seg *tor_shot();
extern struct seg *tgc_shot();
extern struct seg *ell_shot();
extern struct seg *arb_shot();
extern struct seg *ars_shot();
extern struct seg *hlf_shot();
extern struct seg *rec_shot();
extern struct seg *pg_shot();
extern struct seg *spl_shot();
extern struct seg *sph_shot();
extern struct seg *ebm_shot();

extern void	nul_vshot();
extern void	ell_vshot();
extern void	sph_vshot();
extern void	hlf_vshot();
extern void	rec_vshot();
extern void	arb_vshot();
extern void	tgc_vshot();
extern void	tor_vshot();
extern void	rt_vstub();	/* XXX vshoot.c */

extern void	nul_tess();
#if 0
extern void	ell_tess();
extern void	sph_tess();
extern void	hlf_tess();
extern void	rec_tess();
#endif
extern void	arb_tess();
extern void	tgc_tess();
#if 0
extern void	tor_tess();
#endif

struct rt_functab rt_functab[ID_MAXIMUM+2] = {
	"ID_NULL",	0,
		nul_prep,	nul_shot,	nul_print, 	nul_norm,
	 	nul_uv,		nul_curve,	nul_class,	nul_free,
		nul_plot,	nul_vshot,	nul_tess,
		
	"ID_TOR",	1,
		tor_prep,	tor_shot,	tor_print,	tor_norm,
		tor_uv,		tor_curve,	tor_class,	tor_free,
		tor_plot,	tor_vshot,	nul_tess,

	"ID_TGC",	1,
		tgc_prep,	tgc_shot,	tgc_print,	tgc_norm,
		tgc_uv,		tgc_curve,	tgc_class,	tgc_free,
		tgc_plot,	tgc_vshot,	tgc_tess,

	"ID_ELL",	1,
		ell_prep,	ell_shot,	ell_print,	ell_norm,
		ell_uv,		ell_curve,	ell_class,	ell_free,
		ell_plot,	ell_vshot,	nul_tess,

	"ID_ARB8",	0,
		arb_prep,	arb_shot,	arb_print,	arb_norm,
		arb_uv,		arb_curve,	arb_class,	arb_free,
		arb_plot,	arb_vshot,	arb_tess,

	"ID_ARS",	1,
		ars_prep,	ars_shot,	ars_print,	ars_norm,
		ars_uv,		ars_curve,	ars_class,	ars_free,
		ars_plot,	rt_vstub,	nul_tess,

	"ID_HALF",	0,
		hlf_prep,	hlf_shot,	hlf_print,	hlf_norm,
		hlf_uv,		hlf_curve,	hlf_class,	hlf_free,
		hlf_plot,	hlf_vshot,	nul_tess,

	"ID_REC",	1,
		rec_prep,	rec_shot,	rec_print,	rec_norm,
		rec_uv,		rec_curve,	rec_class,	rec_free,
		tgc_plot,	rec_vshot,	nul_tess,

	"ID_POLY",	1,
		pg_prep,	pg_shot,	pg_print,	pg_norm,
		pg_uv,		pg_curve,	pg_class,	pg_free,
		pg_plot,	rt_vstub,	nul_tess,

	"ID_BSPLINE",	1,
		spl_prep,	spl_shot,	spl_print,	spl_norm,
		spl_uv,		spl_curve,	spl_class,	spl_free,
		spl_plot,	rt_vstub,	nul_tess,

	"ID_SPH",	1,
		sph_prep,	sph_shot,	sph_print,	sph_norm,
		sph_uv,		sph_curve,	sph_class,	sph_free,
		ell_plot,	sph_vshot,	nul_tess,

	"ID_STRINGSOL",	0,
		nul_prep,	nul_shot,	nul_print,	nul_norm,
		nul_uv,		nul_curve,	nul_class,	nul_free,
		nul_plot,	nul_vshot,	nul_tess,

	"ID_EBM",	1,
		ebm_prep,	ebm_shot,	ebm_print,	ebm_norm,
		ebm_uv,		ebm_curve,	ebm_class,	ebm_free,
		ebm_plot,	rt_vstub,	nul_tess,

	">ID_NULL",	0,
		nul_prep,	nul_shot,	nul_print,	nul_norm,
		nul_uv,		nul_curve,	nul_class,	nul_free,
		nul_plot,	nul_vshot,	nul_tess
};
int rt_nfunctab = sizeof(rt_functab)/sizeof(struct rt_functab);

/*
 *  Hooks for unimplemented routines
 */
#define DEF(func)	func() { rt_log("func unimplemented\n"); return; }
#define IDEF(func)	func() { rt_log("func unimplemented\n"); return(0); }

int IDEF(nul_prep)
struct seg * IDEF(nul_shot)
void DEF(nul_print)
void DEF(nul_norm)
void DEF(nul_uv)
void DEF(nul_curve)
int IDEF(nul_class)
void DEF(nul_free)
void DEF(nul_plot)
void DEF(nul_vshot)
void DEF(nul_tess);
