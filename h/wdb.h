/*
 *			W D B . H
 *
 *  Interface structures and routines for libwdb
 *
 *  Note -
 *	Rather than using a stdio (FILE *),
 *	we now use a (struct rt_wdb *) parameter.
 *	Rather than calling fopen(), call wdb_fopen();
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
 *	#include "rtgeom.h"
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

#ifndef RAYTRACE_H
# include "raytrace.h"
#endif

#ifndef SEEN_RTGEOM_H
# include "rtgeom.h"
#endif

#ifndef WDB_H
#define WDB_H seen

#ifdef __cplusplus
extern "C" {
#endif

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
	mat_t		wm_mat;		/* XXX Should be matp_t !!! */
	char		wm_name[32+3];	/* XXX Should be char* bu_strdup()ed */
};
#define WMEMBER_NULL	((struct wmember *)0)
#define WMEMBER_MAGIC	0x43128912
#define WDB_CK_WMEMBER(_p)	BU_CKMAG(_p, WMEMBER_MAGIC, "wmember" );

/*
 *  Definitions for pipe (wire) segments
 * XXX Why isn't this in h/rtgeom.h?
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


WDB_EXTERN(int mk_id, (struct rt_wdb *fp, const char *title) );
WDB_EXTERN(int mk_id_units, (struct rt_wdb *fp, const char *title, const char *units) );
int mk_id_editunits(
	struct rt_wdb *fp,
	const char *title,
	double local2mm );

/*
 *  Solid conversion routines
 */
/* wdb.c */
WDB_EXTERN(int mk_half, (struct rt_wdb *fp, const char *name, const vect_t norm, double d) );
int mk_grip( 
	struct rt_wdb *wdbp,
	const char *name,
	const point_t center,
	const vect_t normal,
	const fastf_t magnitude );
WDB_EXTERN(int mk_rpp, (struct rt_wdb *fp, const char *name, const point_t min,
			const point_t max) );
WDB_EXTERN(int mk_wedge, (struct rt_wdb *fp, const char *name, const point_t vert,
			const vect_t xdirv, const vect_t zdirv,
			fastf_t xlen, fastf_t ylen, fastf_t zlen,
			fastf_t x_top_len) );
WDB_EXTERN(int mk_arb4, (struct rt_wdb *fp, const char *name, const fastf_t *pts4) );
WDB_EXTERN(int mk_arb8, (struct rt_wdb *fp, const char *name, const fastf_t *pts8) );
WDB_EXTERN(int mk_sph, (struct rt_wdb *fp, const char *name, const point_t center,
			fastf_t radius) );
WDB_EXTERN(int mk_ell, (struct rt_wdb *fp, const char *name, const point_t center,
			const vect_t a, const vect_t b, const vect_t c) );
WDB_EXTERN(int mk_tor, (struct rt_wdb *fp, const char *name, const point_t center,
			const vect_t inorm, double r1, double r2) );
WDB_EXTERN(int mk_rcc, (struct rt_wdb *fp, const char *name, const point_t base,
			const vect_t height, fastf_t radius) );
WDB_EXTERN(int mk_tgc, (struct rt_wdb *fp, const char *name, const point_t base,
			const vect_t height, const vect_t a, const vect_t b,
			const vect_t c, const vect_t d) );
WDB_EXTERN(int mk_cone, (struct rt_wdb *fp, const char *name, const point_t base,
			const vect_t dirv, fastf_t height, fastf_t rad1,
			fastf_t rad2) );
#define mk_trc(wrong)	+++error_obsolete_libwdb_routine+++	/* This routine no longer exists */
WDB_EXTERN(int mk_trc_h, (struct rt_wdb *fp, const char *name, const point_t base,
			const vect_t height, fastf_t radbase, fastf_t radtop) );
WDB_EXTERN(int mk_trc_top, (struct rt_wdb *fp, const char *name, const point_t ibase,
			const point_t itop, fastf_t radbase, fastf_t radtop) );
int mk_rpc(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t height,
	const vect_t breadth,
	double half_w );
int mk_rhc(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t height,
	const vect_t breadth,
	fastf_t	half_w,
	fastf_t asymp );
int mk_epa(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t height,
	const vect_t breadth,
	fastf_t r1,
	fastf_t r2 );
int mk_ehy(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t height,
	const vect_t breadth,
	fastf_t r1,
	fastf_t r2,
	fastf_t c );
int mk_eto(
	struct rt_wdb *wdbp,
	const char *name,
	const point_t vert,
	const vect_t norm,
	const vect_t smajor,
	fastf_t rrot,
	fastf_t sminor );

WDB_EXTERN(int mk_arbn, (struct rt_wdb *fp, const char *name, int neqn, plane_t eqn[]) );
WDB_EXTERN(int mk_ars, (struct rt_wdb *fp, const char *name, int ncurves, int pts_per_curve,
			fastf_t	*curves[]) );

#define mk_bsolid(fp,name,nsurf,res)	+++error_obsolete_libwdb_routine+++
#define mk_bsurf(fp,srf)		+++error_obsolete_libwdb_routine+++

/* bot.c */
int
mk_bot(
	struct rt_wdb *fp,
	const char *name,
	unsigned char	mode,
	unsigned char	orientation,
	unsigned char	error_mode,	/* may be used to indicate error handling (ignored for now) */
	int		num_vertices,
	int		num_faces,
	fastf_t		*vertices,	/* array of floats for vertices [num_vertices*3] */
	int		*faces,		/* array of ints for faces [num_faces*3] */
	fastf_t		*thickness,	/* array of plate mode thicknesses (corresponds to array of faces)
					 * NULL for modes RT_BOT_SURFACE and RT_BOT_SOLID.
					 */
	struct bu_bitv	*face_mode );	/* a flag for each face indicating thickness is appended to hit point,
					 * otherwise thickness is centered about hit point
					 */
/* nurb.c */
int mk_bspline( struct rt_wdb *wdbp, const char *name, struct face_g_snurb **surfs );

/* nmg.c */
int mk_nmg( struct rt_wdb *filep, const char *name, struct model *m );
int mk_bot_from_nmg( struct rt_wdb *ofp, const char *name, struct shell *s );

#define write_shell_as_polysolid(ofp,name,s)	mk_bot_from_nmg(ofp,name,s)

/* skt.c */
int mk_sketch(
	struct rt_wdb *fp,
	const char *name,
	struct rt_sketch_internal *skt );

/* extr.c */
int mk_extrusion(
	struct rt_wdb *fp,
	const char *name,
	const char *sketch_name,
	const point_t V,
	const vect_t h,
	const vect_t u_vec,
	const vect_t v_vec,
	int keypoint );

/* cline.c */
int mk_cline(
	struct rt_wdb *fp,
	const char *name,
	const point_t V,
	const vect_t height,
	fastf_t radius,
	fastf_t thickness );

/* pipe.c */
WDB_EXTERN(int mk_particle, (struct rt_wdb *fp, const char *name, point_t vertex,
			vect_t height, double vradius, double hradius) );
WDB_EXTERN(int mk_pipe, (struct rt_wdb *fp, const char *name, struct bu_list *headp) );
void mk_pipe_free( struct bu_list *headp );
void mk_add_pipe_pt(
	struct bu_list *headp,
	const point_t coord,
	double od,
	double id,
	double bendradius );
void mk_pipe_init( struct bu_list *headp );

/* strsol.c */
WDB_EXTERN(int mk_dsp, (struct rt_wdb *fp, const char *name, const char *file,
			int xdim, int ydim, const matp_t mat));
WDB_EXTERN(int mk_ebm, (struct rt_wdb *fp, const char *name, const char *file,
			int xdim, int ydim, fastf_t tallness, const matp_t mat));
WDB_EXTERN(int mk_vol, (struct rt_wdb *fp, const char *name, const char *file,
			int xdim, int ydim, int zdim, int lo, int hi,
			const vect_t cellsize, const matp_t mat));
#define mk_strsol(fp,name,solid,arg)	+++error_obsolete_libwdb_routine+++
WDB_EXTERN(int mk_submodel, (struct rt_wdb *fp, const char *name, const char *file,
			const char *treetop, int meth));

/* binary_obj.c */
WDB_EXTERN(int mk_binunif, (struct rt_wdb *wdbp, const char *obj_name,
		    const char *file_name, unsigned int minor_type));

/*
 *  The polysolid has been replaced by the BoT.
 *  Automatic conversion is provided by rt_pg_to_bot()
 */
#define mk_polysolid(fp,name)	+++error_obsolete_libwdb_routine+++
#define mk_poly(fp,npts,verts,norms)	+++error_obsolete_libwdb_routine+++
#define mk_fpoly(fp,npts,verts)	+++error_obsolete_libwdb_routine+++

/* mater.c */
int mk_write_color_table( struct rt_wdb *ofp );


/* These routines have been replaced by the construction routines below */
#define mk_rcomb(fp,name,len,reg,shadername,mparam,rgb,id,air,mater,los,flag)	+++error_obsolete_libwdb_routine+++
#define mk_fcomb(fp,name,len,reg)				+++error_obsolete_libwdb_routine+++
#define mk_memb(fp,name,map,op)					+++error_obsolete_libwdb_routine+++

/*
 *  Combination (region&group) construction routines
 *
 *  First you build a list of nodes with mk_addmember,
 *  then you output the combination.
 */
struct wmember *mk_addmember(
	const char	*name,
	struct bu_list	*headp,
	mat_t mat,
	int		op);

#define mk_lcomb(_fp,_name,_headp,_rf,_shadername,_shaderargs,_rgb,_inh)	\
	mk_comb(_fp,_name,&((_headp)->l),_rf,_shadername,_shaderargs,\
		_rgb,0,0,0,0,_inh,0,0)

/* mk_lrcomb() would not append, and did not have GIFT semantics */
/* mk_lrcomb() had (struct wmember *) head, need (struct bu_list *) */
#define mk_lrcomb(fp, name, _headp, region_flag, shadername, shaderargs, rgb, id, air, material, los, inherit_flag )	\
	mk_comb( fp, name, &((_headp)->l), region_flag, shadername, shaderargs, \
		rgb, id, air, material, los, inherit_flag, 0, 0 )

int mk_comb(
	struct rt_wdb		*wdbp,
	const char		*combname,
	struct bu_list		*headp,		/* Made by mk_addmember() */
	int			region_kind,	/* 1 => region.  'P' and 'V' for FASTGEN */
	const char		*shadername,	/* shader name, or NULL */
	const char		*shaderargs,	/* shader args, or NULL */
	const unsigned char	*rgb,		/* NULL => no color */
	int			id,		/* region_id */
	int			air,		/* aircode */
	int			material,	/* GIFTmater */
	int			los,
	int			inherit,
	int			append_ok,	/* 0 = obj must not exit */
	int			gift_semantics);	/* 0 = pure, 1 = gift */

/* Convenience routines for quickly making combinations */
int mk_comb1( struct rt_wdb *fp,
	const char *combname,
	const char *membname,
	int regflag );
int
mk_region1(
	struct rt_wdb *fp,
	const char *combname,
	const char *membname,
	const char *shadername,
	const char *shaderargs,
	const unsigned char *rgb );

#define mk_fastgen_region(fp,name,headp,mode,shadername,shaderargs,rgb,id,air,material,los,inherit)	\
	mk_comb(fp,name,headp,mode,shadername,shaderargs,rgb,id,air,\
		material,los,inherit,0,0)


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
 *  Set this variable to either 4 or 5, depending on which version of
 *  the database you wish to write. 
 */
extern int	mk_version;		/* Which version database to write */

/*
 *  Internal routines
 */
void mk_freemembers( struct bu_list *headp );

#define mk_fwrite_internal(fp,name,ip)		+++error_obsolete_libwdb_routine+++
#define mk_export_fwrite(wdbp,name,gp,id)	wdb_export(wdbp,name,gp,id,mk_conv2mm)

#ifdef __cplusplus
}
#endif

#endif /* WDB_H */
