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
#include "db.h"
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
extern int	vol_prep(), vol_class();
extern int	arbn_prep(), arbn_class();

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
extern void	vol_print(), vol_norm(), vol_uv();
extern void	arbn_print(), arbn_norm(), arbn_uv();

extern void	nul_curve(), nul_free();
extern void	tor_curve(), tor_free();
extern void	tgc_curve(), tgc_free();
extern void	ell_curve(), ell_free();
extern void	arb_curve(), arb_free();
extern void	hlf_curve(), hlf_free();
extern void	ars_curve(), ars_free();
extern void	rec_curve(), rec_free();
extern void	pg_curve(),  pg_free();
extern void	spl_curve(), spl_free();
extern void	sph_curve(), sph_free();
extern void	ebm_curve(), ebm_free();
extern void	vol_curve(), vol_free();
extern void	arbn_curve(), arbn_free();

extern int	nul_plot();
extern int	tor_plot();
extern int	tgc_plot();
extern int	ell_plot();
extern int	arb_plot();
extern int	hlf_plot();
extern int	ars_plot();
extern int	rec_plot();
extern int	pg_plot();
extern int	spl_plot();
extern int	sph_plot();
extern int	ebm_plot();
extern int	vol_plot();
extern int	arbn_plot();

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
extern struct seg *vol_shot();
extern struct seg *arbn_shot();

extern void	nul_vshot();
extern void	ell_vshot();
extern void	sph_vshot();
extern void	hlf_vshot();
extern void	rec_vshot();
extern void	arb_vshot();
extern void	tgc_vshot();
extern void	tor_vshot();
extern void	arbn_vshot();
extern void	rt_vstub();	/* XXX vshoot.c */

extern int	nul_tess();
extern int	ell_tess();
extern int	sph_tess();
#if 0
extern int	hlf_tess();
extern int	rec_tess();
#endif
extern int	arb_tess();
extern int	tgc_tess();
#if 0
extern int	tor_tess();
extern int	ebm_tess();
extern int	vol_tess();
#endif
extern int	arbn_tess();

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
		ell_plot,	ell_vshot,	ell_tess,

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
		ell_plot,	sph_vshot,	sph_tess,

	"ID_STRINGSOL",	0,
		nul_prep,	nul_shot,	nul_print,	nul_norm,
		nul_uv,		nul_curve,	nul_class,	nul_free,
		nul_plot,	nul_vshot,	nul_tess,

	"ID_EBM",	0,
		ebm_prep,	ebm_shot,	ebm_print,	ebm_norm,
		ebm_uv,		ebm_curve,	ebm_class,	ebm_free,
		ebm_plot,	rt_vstub,	nul_tess,

	"ID_VOL",	0,
		vol_prep,	vol_shot,	vol_print,	vol_norm,
		vol_uv,		vol_curve,	vol_class,	vol_free,
		vol_plot,	rt_vstub,	nul_tess,

	"ID_ARBN",	0,
		arbn_prep,	arbn_shot,	arbn_print,	arbn_norm,
		arbn_uv,	arbn_curve,	arbn_class,	arbn_free,
		arbn_plot,	arbn_vshot,	arbn_tess,

	">ID_MAXIMUM",	0,
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
#define NDEF(func)	func() { rt_log("func unimplemented\n"); return(-1); }

int IDEF(nul_prep)
struct seg * IDEF(nul_shot)
void DEF(nul_print)
void DEF(nul_norm)
void DEF(nul_uv)
void DEF(nul_curve)
int IDEF(nul_class)
void DEF(nul_free)
int NDEF(nul_plot)
void DEF(nul_vshot)
int NDEF(nul_tess)

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
rt_id_solid( rec )
register union record *rec;
{
	register int id;

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
	case ID_STRSOL:
		/* XXX This really needs to be some kind of table */
		if( strncmp( rec->ss.ss_str, "ebm", 3 ) == 0 )  {
			id = ID_EBM;
			break;
		} else if( strncmp( rec->ss.ss_str, "vol", 3 ) == 0 )  {
			id = ID_VOL;
			break;
		}
		rt_log("rt_id_solid(%s):  String solid type '%s' unknown\n",
			rec->ss.ss_name, rec->ss.ss_str );
		id = ID_NULL;		/* BAD */
		break;
	case DBID_ARBN:
		id = ID_ARBN;
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
