/*
 *			W D B . H
 *
 *  Interface structures and routines for libwdb
 *
 *  Notes -
 *	Any source file that includes this header file must also include
 *	<stdio.h> to obtain the typedef for FILE
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
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 *
 *  Include Sequencing -
 *	#include <stdio.h>	/_* For FILE * *_/
 *	#include <math.h>
 *	#include "machine.h"	/_* For fastf_t definition on this machine *_/
 *	#include "bu.h"
 *	#include "vmath.h"	/_* For vect_t definition *_/
 *	#include "nmg.h"	/_* OPTIONAL, precedes wdb.h when used *_/
 *	#include "raytrace.h"	/_* OPTIONAL, precedes wdb.h when used *_/
 *	#include "nurb.h"	/_* OPTIONAL, precedes wdb.h when used *_/
 *	#include "wdb.h"
 *
 *  Libraries Used -
 *	LIBWDB LIBRT LIBRT_LIBES -lm -lc
 *
 *  $Header$
 */
#ifndef SEEN_BU_H
# include "bu.h"
#endif

#ifndef SEEN_BN_H
# include "bn.h"
#endif

#ifndef WDB_H
#define WDB_H seen
/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 */
#if __STDC__ || USE_PROTOTYPES
#	define	WDB_EXTERN(type_and_name,args)	extern type_and_name args
#	define	WDB_ARGS(args)			args
#else
#	define	WDB_EXTERN(type_and_name,args)	extern type_and_name()
#	define	WDB_ARGS(args)			()
#endif

/*
 *  In-memory form of database combinations
 */
struct wmember  {
	struct bu_list	l;
	int		wm_op;		/* Boolean operation */
	mat_t		wm_mat;
	char		wm_name[16+3];	/* NAMESIZE */
};
#define WMEMBER_NULL	((struct wmember *)0)
#define WMEMBER_MAGIC	0x43128912

/*
 *  Definitions for pipe (wire) segments
 */

struct wdb_pipept {
	struct bu_list	l;		/* doubly linked list support */
	point_t		pp_coord;	/* "control" point for pipe solid */
	fastf_t		pp_id;		/* inner diam, <=0 if solid (wire) */
	fastf_t		pp_od;		/* pipe outer diam */
	fastf_t		pp_bendradius;	/* bend radius to use for a bend at this point */
};

#define WDB_PIPESEG_NULL	((struct wdb_pipeseg *)0)
#define WDB_PIPESEG_MAGIC	0x9723ffef

/*
 *  Solid conversion routines
 */
WDB_EXTERN(int mk_id, (FILE *fp, CONST char *title) );
WDB_EXTERN(int mk_id_units, (FILE *fp, CONST char *title, CONST char *units) );
WDB_EXTERN(int mk_half, (FILE *fp, char *name, CONST vect_t norm, double d) );
WDB_EXTERN(int mk_rpp, (FILE *fp, char *name, CONST point_t min,
			CONST point_t max) );
WDB_EXTERN(int mk_wedge, (FILE *fp, char *name, CONST point_t vert,
			CONST vect_t xdirv, CONST vect_t zdirv,
			fastf_t xlen, fastf_t ylen, fastf_t zlen,
			fastf_t x_top_len) );
WDB_EXTERN(int mk_arb4, (FILE *fp, char *name, CONST fastf_t *pts4) );
WDB_EXTERN(int mk_arb8, (FILE *fp, char *name, CONST fastf_t *pts8) );
WDB_EXTERN(int mk_sph, (FILE *fp, char *name, CONST point_t center,
			fastf_t radius) );
WDB_EXTERN(int mk_ell, (FILE *fp, char *name, CONST point_t center,
			CONST vect_t a, CONST vect_t b, CONST vect_t c) );
WDB_EXTERN(int mk_tor, (FILE *fp, char *name, CONST point_t center,
			CONST vect_t inorm, double r1, double r2) );
WDB_EXTERN(int mk_rcc, (FILE *fp, char *name, CONST point_t base,
			CONST vect_t height, fastf_t radius) );
WDB_EXTERN(int mk_tgc, (FILE *fp, char *name, CONST point_t base,
			CONST vect_t height, CONST vect_t a, CONST vect_t b,
			CONST vect_t c, CONST vect_t d) );
WDB_EXTERN(int mk_cone, (FILE *fp, char *name, CONST point_t base,
			CONST vect_t dirv, fastf_t height, fastf_t rad1,
			fastf_t rad2) );
#define mk_trc(wrong)	+++error+++	/* This routine no longer exists */
WDB_EXTERN(int mk_trc_h, (FILE *fp, char *name, CONST point_t base,
			CONST vect_t height, fastf_t radbase, fastf_t radtop) );
WDB_EXTERN(int mk_trc_top, (FILE *fp, char *name, CONST point_t ibase,
			CONST point_t itop, fastf_t radbase, fastf_t radtop) );

WDB_EXTERN(int mk_arbn, (FILE *fp, char *name, int neqn, plane_t eqn[]) );
WDB_EXTERN(int mk_ars, (FILE *fp, char *name, int ncurves, int pts_per_curve,
			fastf_t	*curves[]) );
WDB_EXTERN(int mk_bsolid, (FILE *fp, char *name, int nsurf, double res) );
#if defined(NMG_H)
WDB_EXTERN(int mk_bsurf, (FILE *fp, struct face_g_snurb *bp) );
#else /* !NMG_H */
WDB_EXTERN(int mk_bsurf, (FILE *fp, genptr_t bp) );
#endif /* NMG_H */
WDB_EXTERN(int mk_particle, (FILE *fp, char *name, point_t vertex,
			vect_t height, double vradius, double hradius) );
WDB_EXTERN(int mk_pipe, (FILE *fp, char *name, struct wdb_pipept *headp) );

/*
 *  These routines will be replaced in the next release.
 *  Try not to use them.
 */
WDB_EXTERN(int mk_polysolid, (FILE *fp, char *name) );
WDB_EXTERN(int mk_poly, (FILE *fp, int npts,
			fastf_t verts[][3], fastf_t norms[][3]) );
WDB_EXTERN(int mk_fpoly, (FILE *fp, int npts, fastf_t verts[][3]) );
WDB_EXTERN(int mk_comb, (FILE *fp, CONST char *name, int len, int region_flag,
			CONST char *matname, CONST char *matparm,
			CONST unsigned char *rgb,
			int inherit_flag) );
WDB_EXTERN(int mk_rcomb, (FILE *fp, CONST char *name, int len, int region_flag,
			CONST char *matname, CONST char *matparm,
			CONST unsigned char *rgb,
			int id, int air, int material, int los,
			int inherit_flag) );
WDB_EXTERN(int mk_fcomb, (FILE *fp, CONST char *name, int len, int region_flag) );
WDB_EXTERN(int mk_memb, (FILE *fp, CONST char *name, CONST mat_t mat, int bool_op) );

/*
 *  Combination conversion routines
 */
WDB_EXTERN(struct wmember *mk_addmember, (CONST char *name,
			struct wmember *headp, int op) );
WDB_EXTERN(int mk_lcomb, (FILE *fp, CONST char *name, struct wmember *headp,
			int region_flag,
			CONST char *matname, CONST char *matparm,
			CONST unsigned char *rgb,
			int inherit_flag) );
WDB_EXTERN(int mk_lrcomb, (FILE *fp, CONST char *name, struct wmember *headp,
			int region_flag,
			CONST char *matname, CONST char *matparm,
			CONST unsigned char *rgb,
			int id, int air, int material, int los,
			int inherit_flag) );

/* Values for wm_op.  These must track db.h */
#define WMOP_INTERSECT	'+'
#define WMOP_SUBTRACT	'-'
#define WMOP_UNION	'u'

/* Convienient definitions */
#define mk_lfcomb(fp,name,headp,region)		mk_lcomb( fp, name, headp, \
	region, (char *)0, (char *)0, (unsigned char *)0, 0 );

/*
 *  Routines to establish conversion factors
 */
WDB_EXTERN(int mk_conversion, (char *units_string) );
WDB_EXTERN(int mk_set_conversion, (double val) );

/*
 * This internal variable should not be directly modified;
 * call mk_conversion() or mk_set_conversion() instead.
 */
extern double	mk_conv2mm;		/* Conversion factor to mm */

/*
 *  Internal routines
 */
WDB_EXTERN(int mk_freemembers, (struct wmember *headp) );
WDB_EXTERN(int mk_export_fwrite, (FILE *fp, char *name, genptr_t gp, int id));

#endif /* WDB_H */
