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

extern int	rt_nul_prep(), rt_nul_class();
extern int	rt_tor_prep(), rt_tor_class();
extern int	rt_tgc_prep(), rt_tgc_class();
extern int	rt_ell_prep(), rt_ell_class();
extern int	rt_arb_prep(), rt_arb_class();
extern int	rt_hlf_prep(), rt_hlf_class();
extern int	rt_ars_prep(), rt_ars_class();
extern int	rt_rec_prep(), rt_rec_class();
extern int	rt_pg_prep(), rt_pg_class();
extern int	rt_spl_prep(), rt_spl_class();
extern int	rt_sph_prep(), rt_sph_class();
extern int	rt_ebm_prep(), rt_ebm_class();
extern int	rt_vol_prep(), rt_vol_class();
extern int	rt_arbn_prep(), rt_arbn_class();
extern int	rt_part_prep(), rt_part_class();
extern int	rt_pipe_prep(), rt_pipe_class();

extern void	rt_nul_print(), rt_nul_norm(), rt_nul_uv();
extern void	rt_tor_print(), rt_tor_norm(), rt_tor_uv();
extern void	rt_tgc_print(), rt_tgc_norm(), rt_tgc_uv();
extern void	rt_ell_print(), rt_ell_norm(), rt_ell_uv();
extern void	rt_arb_print(), rt_arb_norm(), rt_arb_uv();
extern void	rt_hlf_print(), rt_hlf_norm(), rt_hlf_uv();
extern void	rt_ars_print(), rt_ars_norm(), rt_ars_uv();
extern void	rt_rec_print(), rt_rec_norm(), rt_rec_uv();
extern void	rt_pg_print(),  rt_pg_norm(),  rt_pg_uv();
extern void	rt_spl_print(), rt_spl_norm(), rt_spl_uv();
extern void	rt_sph_print(), rt_sph_norm(), rt_sph_uv();
extern void	rt_ebm_print(), rt_ebm_norm(), rt_ebm_uv();
extern void	rt_vol_print(), rt_vol_norm(), rt_vol_uv();
extern void	rt_arbn_print(), rt_arbn_norm(), rt_arbn_uv();
extern void	rt_part_print(), rt_part_norm(), rt_part_uv();
extern void	rt_pipe_print(), rt_pipe_norm(), rt_pipe_uv();

extern void	rt_nul_curve(), rt_nul_free();
extern void	rt_tor_curve(), rt_tor_free();
extern void	rt_tgc_curve(), rt_tgc_free();
extern void	rt_ell_curve(), rt_ell_free();
extern void	rt_arb_curve(), rt_arb_free();
extern void	rt_hlf_curve(), rt_hlf_free();
extern void	rt_ars_curve(), rt_ars_free();
extern void	rt_rec_curve(), rt_rec_free();
extern void	rt_pg_curve(),  rt_pg_free();
extern void	rt_spl_curve(), rt_spl_free();
extern void	rt_sph_curve(), rt_sph_free();
extern void	rt_ebm_curve(), rt_ebm_free();
extern void	rt_vol_curve(), rt_vol_free();
extern void	rt_arbn_curve(), rt_arbn_free();
extern void	rt_part_curve(), rt_part_free();
extern void	rt_pipe_curve(), rt_pipe_free();

extern int	rt_nul_plot();
extern int	rt_tor_plot();
extern int	rt_tgc_plot();
extern int	rt_ell_plot();
extern int	rt_arb_plot();
extern int	rt_hlf_plot();
extern int	rt_ars_plot();
extern int	rt_pg_plot();
extern int	rt_spl_plot();
extern int	rt_ebm_plot();
extern int	rt_vol_plot();
extern int	rt_arbn_plot();
extern int	rt_part_plot();
extern int	rt_pipe_plot();

extern int	rt_nul_shot();
extern int	rt_tor_shot();
extern int	rt_tgc_shot();
extern int	rt_ell_shot();
extern int	rt_arb_shot();
extern int	rt_ars_shot();
extern int	rt_hlf_shot();
extern int	rt_rec_shot();
extern int	rt_pg_shot();
extern int	rt_spl_shot();
extern int	rt_sph_shot();
extern int	rt_ebm_shot();
extern int	rt_vol_shot();
extern int	rt_arbn_shot();
extern int	rt_part_shot();
extern int	rt_pipe_shot();

extern void	rt_nul_vshot();
extern void	rt_ell_vshot();
extern void	rt_sph_vshot();
extern void	rt_hlf_vshot();
extern void	rt_rec_vshot();
extern void	rt_arb_vshot();
extern void	rt_tgc_vshot();
extern void	rt_tor_vshot();
extern void	rt_arbn_vshot();
extern void	rt_part_vshot();
extern void	rt_pipe_vshot();
extern void	rt_vstub();	/* XXX vshoot.c */

extern int	rt_nul_tess();
extern int	rt_ell_tess();
extern int	rt_arb_tess();
extern int	rt_tgc_tess();
extern int	rt_tor_tess();
extern int	rt_pg_tess();
extern int	rt_hlf_tess();
extern int	rt_ars_tess();
extern int	rt_spl_tess();
#if 0
extern int	rt_ebm_tess();
extern int	rt_vol_tess();
#endif
extern int	rt_arbn_tess();
extern int	rt_part_tess();
extern int	rt_pipe_tess();

struct rt_functab rt_functab[ID_MAXIMUM+2] = {
	"ID_NULL",	0,
		rt_nul_prep,	rt_nul_shot,	rt_nul_print, 	rt_nul_norm,
	 	rt_nul_uv,	rt_nul_curve,	rt_nul_class,	rt_nul_free,
		rt_nul_plot,	rt_nul_vshot,	rt_nul_tess,
		
	"ID_TOR",	1,
		rt_tor_prep,	rt_tor_shot,	rt_tor_print,	rt_tor_norm,
		rt_tor_uv,	rt_tor_curve,	rt_tor_class,	rt_tor_free,
		rt_tor_plot,	rt_tor_vshot,	rt_tor_tess,

	"ID_TGC",	1,
		rt_tgc_prep,	rt_tgc_shot,	rt_tgc_print,	rt_tgc_norm,
		rt_tgc_uv,	rt_tgc_curve,	rt_tgc_class,	rt_tgc_free,
		rt_tgc_plot,	rt_tgc_vshot,	rt_tgc_tess,

	"ID_ELL",	1,
		rt_ell_prep,	rt_ell_shot,	rt_ell_print,	rt_ell_norm,
		rt_ell_uv,	rt_ell_curve,	rt_ell_class,	rt_ell_free,
		rt_ell_plot,	rt_ell_vshot,	rt_ell_tess,

	"ID_ARB8",	0,
		rt_arb_prep,	rt_arb_shot,	rt_arb_print,	rt_arb_norm,
		rt_arb_uv,	rt_arb_curve,	rt_arb_class,	rt_arb_free,
		rt_arb_plot,	rt_arb_vshot,	rt_arb_tess,

	"ID_ARS",	1,
		rt_ars_prep,	rt_ars_shot,	rt_ars_print,	rt_ars_norm,
		rt_ars_uv,	rt_ars_curve,	rt_ars_class,	rt_ars_free,
		rt_ars_plot,	rt_vstub,	rt_ars_tess,

	"ID_HALF",	0,
		rt_hlf_prep,	rt_hlf_shot,	rt_hlf_print,	rt_hlf_norm,
		rt_hlf_uv,	rt_hlf_curve,	rt_hlf_class,	rt_hlf_free,
		rt_hlf_plot,	rt_hlf_vshot,	rt_hlf_tess,

	"ID_REC",	1,
		rt_rec_prep,	rt_rec_shot,	rt_rec_print,	rt_rec_norm,
		rt_rec_uv,	rt_rec_curve,	rt_rec_class,	rt_rec_free,
		rt_tgc_plot,	rt_rec_vshot,	rt_tgc_tess,

	"ID_POLY",	1,
		rt_pg_prep,	rt_pg_shot,	rt_pg_print,	rt_pg_norm,
		rt_pg_uv,	rt_pg_curve,	rt_pg_class,	rt_pg_free,
		rt_pg_plot,	rt_vstub,	rt_pg_tess,

	"ID_BSPLINE",	1,
		rt_spl_prep,	rt_spl_shot,	rt_spl_print,	rt_spl_norm,
		rt_spl_uv,	rt_spl_curve,	rt_spl_class,	rt_spl_free,
		rt_spl_plot,	rt_vstub,	rt_spl_tess,

	"ID_SPH",	1,
		rt_sph_prep,	rt_sph_shot,	rt_sph_print,	rt_sph_norm,
		rt_sph_uv,	rt_sph_curve,	rt_sph_class,	rt_sph_free,
		rt_ell_plot,	rt_sph_vshot,	rt_ell_tess,

	"ID_STRINGSOL",	0,
		rt_nul_prep,	rt_nul_shot,	rt_nul_print,	rt_nul_norm,
		rt_nul_uv,	rt_nul_curve,	rt_nul_class,	rt_nul_free,
		rt_nul_plot,	rt_nul_vshot,	rt_nul_tess,

	"ID_EBM",	0,
		rt_ebm_prep,	rt_ebm_shot,	rt_ebm_print,	rt_ebm_norm,
		rt_ebm_uv,	rt_ebm_curve,	rt_ebm_class,	rt_ebm_free,
		rt_ebm_plot,	rt_vstub,	rt_nul_tess,

	"ID_VOL",	0,
		rt_vol_prep,	rt_vol_shot,	rt_vol_print,	rt_vol_norm,
		rt_vol_uv,	rt_vol_curve,	rt_vol_class,	rt_vol_free,
		rt_vol_plot,	rt_vstub,	rt_nul_tess,

	"ID_ARBN",	0,
		rt_arbn_prep,	rt_arbn_shot,	rt_arbn_print,	rt_arbn_norm,
		rt_arbn_uv,	rt_arbn_curve,	rt_arbn_class,	rt_arbn_free,
		rt_arbn_plot,	rt_arbn_vshot,	rt_arbn_tess,

	"ID_PIPE",	0,
		rt_pipe_prep,	rt_pipe_shot,	rt_pipe_print,	rt_pipe_norm,
		rt_pipe_uv,	rt_pipe_curve,	rt_pipe_class,	rt_pipe_free,
		rt_pipe_plot,	rt_pipe_vshot,	rt_pipe_tess,

	"ID_PARTICLE",	0,
		rt_part_prep,	rt_part_shot,	rt_part_print,	rt_part_norm,
		rt_part_uv,	rt_part_curve,	rt_part_class,	rt_part_free,
		rt_part_plot,	rt_part_vshot,	rt_part_tess,

	">ID_MAXIMUM",	0,
		rt_nul_prep,	rt_nul_shot,	rt_nul_print,	rt_nul_norm,
		rt_nul_uv,	rt_nul_curve,	rt_nul_class,	rt_nul_free,
		rt_nul_plot,	rt_nul_vshot,	rt_nul_tess
};
int rt_nfunctab = sizeof(rt_functab)/sizeof(struct rt_functab);

/*
 *  Hooks for unimplemented routines
 */
#define DEF(func)	func() { rt_log("func unimplemented\n"); return; }
#define IDEF(func)	func() { rt_log("func unimplemented\n"); return(0); }
#define NDEF(func)	func() { rt_log("func unimplemented\n"); return(-1); }

int IDEF(rt_nul_prep)
int	 IDEF(rt_nul_shot)
void DEF(rt_nul_print)
void DEF(rt_nul_norm)
void DEF(rt_nul_uv)
void DEF(rt_nul_curve)
int IDEF(rt_nul_class)
void DEF(rt_nul_free)
int NDEF(rt_nul_plot)
void DEF(rt_nul_vshot)
int NDEF(rt_nul_tess)

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
	case DBID_PIPE:
		id = ID_PIPE;
		break;
	case DBID_PARTICLE:
		id = ID_PARTICLE;
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
