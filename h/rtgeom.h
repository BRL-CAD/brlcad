/*
 *			R T G E O M . H
 *
 *  Details of the internal forms used by the LIBRT geometry routines
 *  for the different solids.
 *
 *  These structures are what the struct rt_db_internal
 *  generic pointer idb_ptr points at,
 *  based on idb_type indicating a solid id ID_xxx, such as ID_TGC.
 *
 *  Depends on having machine.h, vmath.h, and rtlist.h included first.
 *  RT_xxx_CK_MAGIC() can only be used if raytrace.h is included too.
 *
 *  The proper order for including them all is:
 *	#include <stdio.h>
 *	#include <math.h>
 *	#include "machine.h"
 *	#include "bu.h"
 *	#include "vmath.h"
 *	#include "nmg.h"
 *	#include "raytrace.h"
 *	#include "nurb.h"
 *	#include "rtgeom.h"
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  $Header$
 */

#ifndef SEEN_RTGEOM_H
#define SEEN_RTGEOM_H seen

#undef r_a /* defined on alliant in <machine/reg.h> included in signal.h */

/*
 *	ID_TOR
 */
struct rt_tor_internal {
	long	magic;
	point_t	v;		/* center point */
	vect_t	h;		/* normal, unit length */
	fastf_t	r_h;		/* radius in H direction (r2) */
	fastf_t	r_a;		/* radius in A direction (r1) */
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	vect_t	a;		/* r_a length */
	vect_t	b;		/* r_b length */
	fastf_t	r_b;		/* radius in B direction (typ == r_a) */
};
#define RT_TOR_INTERNAL_MAGIC	0x9bffed87
#define RT_TOR_CK_MAGIC(_p)	RT_CKMAG(_p,RT_TOR_INTERNAL_MAGIC,"rt_tor_internal")

/*
 *	ID_TGC and ID_REC
 */
struct rt_tgc_internal {
	long	magic;
	vect_t	v;
	vect_t	h;
	vect_t	a;
	vect_t	b;
	vect_t	c;
	vect_t	d;
};
#define RT_TGC_INTERNAL_MAGIC	0xaabbdd87
#define RT_TGC_CK_MAGIC(_p)	RT_CKMAG(_p,RT_TGC_INTERNAL_MAGIC,"rt_tgc_internal")

/*
 *	ID_ELL, and ID_SPH
 */
struct rt_ell_internal  {
	long	magic;
	point_t	v;
	vect_t	a;
	vect_t	b;
	vect_t	c;
};
#define RT_ELL_INTERNAL_MAGIC	0x93bb23ff
#define RT_ELL_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ELL_INTERNAL_MAGIC,"rt_ell_internal")

/*
 *	ID_ARB8
 *
 *  The internal (in memory) form of an ARB8 -- 8 points in space.
 *  The first 4 form the "bottom" face, the second 4 form the "top" face.
 */
struct rt_arb_internal {
	long	magic;
	point_t	pt[8];
};
#define RT_ARB_INTERNAL_MAGIC	0x9befd010
#define RT_ARB_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ARB_INTERNAL_MAGIC,"rt_arb_internal")

/*
 *	ID_ARS
 */
struct rt_ars_internal {
	long	magic;
	int	ncurves;
	int	pts_per_curve;
	fastf_t	**curves;
};
#define RT_ARS_INTERNAL_MAGIC	0x77ddbbe3
#define RT_ARS_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ARS_INTERNAL_MAGIC,"rt_ars_internal")

/*
 *	ID_HALF
 */
struct rt_half_internal  {
	long	magic;
	plane_t	eqn;
};
#define RT_HALF_INTERNAL_MAGIC	0xaa87bbdd
#define RT_HALF_CK_MAGIC(_p)	RT_CKMAG(_p,RT_HALF_INTERNAL_MAGIC,"rt_half_internal")

/*
 *	ID_GRIP
 */
struct rt_grip_internal {
	long	magic;
	point_t	center;
	/* Remaining elemnts are used for display purposes only */
	vect_t	normal;
	fastf_t	mag;
};
#define RT_GRIP_INTERNAL_MAGIC	0x31196205
#define RT_GRIP_CK_MAGIC(_p)	RT_CKMAG(_p,RT_GRIP_INTERNAL_MAGIC,"rt_grip_internal")
	
/*
 *	ID_POLY
 */
struct rt_pg_internal {
	long	magic;
	int	npoly;
	struct rt_pg_face_internal {
		int	npts;		/* number of points for this polygon */
		fastf_t	*verts;		/* has 3*npts elements */
		fastf_t	*norms;		/* has 3*npts elements */
	} *poly;			/* has npoly elements */
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	int	max_npts;		/* maximum value of npts in poly[] */
};
#define RT_PG_INTERNAL_MAGIC	0x9bfed887
#define RT_PG_CK_MAGIC(_p)	RT_CKMAG(_p,RT_PG_INTERNAL_MAGIC,"rt_pg_internal")

/* ID_BSPLINE */
#ifdef NMG_H				/* Only if we have seen struct face_g_snurb */
struct rt_nurb_internal {
	long		magic;
	int	 	nsrf;		/* number of surfaces */
	struct face_g_snurb **srfs;	/* The surfaces themselves */
};

#define RT_NURB_INTERNAL_MAGIC	0x002b2bdd
#define RT_NURB_CK_MAGIC( _p) RT_CKMAG(_p,RT_NURB_INTERNAL_MAGIC,"rt_nurb_internal");
#endif
#define RT_NURB_GET_CONTROL_POINT(_s,_u,_v)	((_s)->ctl_points[ \
	((_v)*(_s)->s_size[0]+(_u))*RT_NURB_EXTRACT_COORDS((_s)->pt_type)])

/*
 *	ID_NMG
 *
 *  The internal form of the NMG is not rt_nmg_internal, but just
 *  a "struct model", from nmg.h.  e.g.:
 *	if( intern.idb_type == ID_NMG )
 *		m = (struct model *)intern.idb_ptr;
 */

/*
 *	ID_EBM
 */
#define RT_EBM_NAME_LEN 256
struct rt_ebm_internal  {
	long		magic;
	char		file[RT_EBM_NAME_LEN];
	int		xdim;		/* X dimension (w cells) */
	int		ydim;		/* Y dimension (n cells) */
	fastf_t		tallness;	/* Z dimension (mm) */
	mat_t		mat;		/* convert local coords to model space */
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	struct rt_mapped_file	*mp;	/* actual data */
};
#define RT_EBM_INTERNAL_MAGIC	0xf901b231
#define RT_EBM_CK_MAGIC(_p)	RT_CKMAG(_p,RT_EBM_INTERNAL_MAGIC,"rt_ebm_internal")

/*
 *	ID_VOL
 */
#define RT_VOL_NAME_LEN 128
struct rt_vol_internal  {
	long		magic;
	char		file[RT_VOL_NAME_LEN];
	int		xdim;		/* X dimension */
	int		ydim;		/* Y dimension */
	int		zdim;		/* Z dimension */
	int		lo;		/* Low threshold */
	int		hi;		/* High threshold */
	vect_t		cellsize;	/* ideal coords: size of each cell */
	mat_t		mat;		/* convert local coords to model space */
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	unsigned char	*map;
};
#define RT_VOL_INTERNAL_MAGIC	0x987ba1d0
#define RT_VOL_CK_MAGIC(_p)	RT_CKMAG(_p,RT_VOL_INTERNAL_MAGIC,"rt_vol_internal")

/*
 *	ID_HF
 */
struct rt_hf_internal {
	long		magic;
	/* BEGIN USER SETABLE VARIABLES */
	char		cfile[128];	/* name of control file (optional) */
	char		dfile[128];	/* name of data file */
	char		fmt[8];		/* CV style file format descriptor */
	int		w;		/* # samples wide of data file.  ("i", "x") */
	int		n;		/* nlines of data file.  ("j", "y") */
	int		shorts;		/* !0 --> memory array is short, not float */
	fastf_t		file2mm;	/* scale factor to cvt file units to mm */
	vect_t		v;		/* origin of HT in model space */
	vect_t		x;		/* model vect corresponding to "w" dir (will be unitized) */
	vect_t		y;		/* model vect corresponding to "n" dir (will be unitized) */
	fastf_t		xlen;		/* model len of HT rpp in "w" dir */
	fastf_t		ylen;		/* model len of HT rpp in "n" dir */
	fastf_t		zscale;		/* scale of data in ''up'' dir (after file2mm is applied) */
	/* END USER SETABLE VARIABLES, BEGIN INTERNAL STUFF */
	struct rt_mapped_file	*mp;	/* actual data */
};
#define RT_HF_INTERNAL_MAGIC	0x4846494d
#define RT_HF_CK_MAGIC(_p)	RT_CKMAG(_p,RT_HF_INTERNAL_MAGIC,"rt_hf_internal")

/*
 *	ID_ARBN
 */
struct rt_arbn_internal  {
	long	magic;
	int	neqn;
	plane_t	*eqn;
};
#define RT_ARBN_INTERNAL_MAGIC	0x18236461
#define RT_ARBN_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ARBN_INTERNAL_MAGIC,"rt_arbn_internal")

/*
 *	ID_PIPE
 */
struct rt_pipe_internal {
	long		pipe_magic;
	struct bu_list	pipe_segs_head;
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	int		pipe_count;
};
#define RT_PIPE_INTERNAL_MAGIC	0x7dd7bb3e
#define RT_PIPE_CK_MAGIC(_p)	RT_CKMAG(_p,RT_PIPE_INTERNAL_MAGIC,"rt_pipe_internal")

/*
 *	ID_PARTICLE
 */
struct rt_part_internal {
	long	part_magic;
	point_t	part_V;
	vect_t	part_H;
	fastf_t	part_vrad;
	fastf_t	part_hrad;
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	int	part_type;		/* sphere, cylinder, cone */
};
#define RT_PART_INTERNAL_MAGIC	0xaaccee87
#define RT_PART_CK_MAGIC(_p)	RT_CKMAG(_p,RT_PART_INTERNAL_MAGIC,"rt_part_internal")

#define RT_PARTICLE_TYPE_SPHERE		1
#define RT_PARTICLE_TYPE_CYLINDER	2
#define RT_PARTICLE_TYPE_CONE		3

/*
 *	ID_RPC
 */
struct rt_rpc_internal {
	long	rpc_magic;
	point_t	rpc_V;	/* rpc vertex */
	vect_t	rpc_H;	/* height vector */
	vect_t	rpc_B;	/* breadth vector */
	fastf_t	rpc_r;	/* scalar half-width of rectangular face */
};
#define RT_RPC_INTERNAL_MAGIC	0xaaccee88
#define RT_RPC_CK_MAGIC(_p)	RT_CKMAG(_p,RT_RPC_INTERNAL_MAGIC,"rt_rpc_internal")

/*
 *	ID_RHC
 */
struct rt_rhc_internal {
	long	rhc_magic;
	point_t	rhc_V;	/* rhc vertex */
	vect_t	rhc_H;	/* height vector */
	vect_t	rhc_B;	/* breadth vector */
	fastf_t	rhc_r;	/* scalar half-width of rectangular face */
	fastf_t	rhc_c;	/* dist from hyperbola to vertex of asymptotes */
};
#define RT_RHC_INTERNAL_MAGIC	0xaaccee89
#define RT_RHC_CK_MAGIC(_p)	RT_CKMAG(_p,RT_RHC_INTERNAL_MAGIC,"rt_rhc_internal")

/*
 *	ID_EPA
 */
struct rt_epa_internal {
	long	epa_magic;
	point_t	epa_V;	/* epa vertex */
	vect_t	epa_H;	/* height vector */
	vect_t	epa_Au;	/* unit vector along semi-major axis */
	fastf_t	epa_r1;	/* scalar semi-major axis length */
	fastf_t	epa_r2;	/* scalar semi-minor axis length */
};
#define RT_EPA_INTERNAL_MAGIC	0xaaccee90
#define RT_EPA_CK_MAGIC(_p)	RT_CKMAG(_p,RT_EPA_INTERNAL_MAGIC,"rt_epa_internal")

/*
 *	ID_EHY
 */
struct rt_ehy_internal {
	long	ehy_magic;
	point_t	ehy_V;	/* ehy vertex */
	vect_t	ehy_H;	/* height vector */
	vect_t	ehy_Au;	/* unit vector along semi-major axis */
	fastf_t	ehy_r1;	/* scalar semi-major axis length */
	fastf_t	ehy_r2;	/* scalar semi-minor axis length */
	fastf_t	ehy_c;	/* dist from hyperbola to vertex of asymptotes */
};
#define RT_EHY_INTERNAL_MAGIC	0xaaccee91
#define RT_EHY_CK_MAGIC(_p)	RT_CKMAG(_p,RT_EHY_INTERNAL_MAGIC,"rt_ehy_internal")

/*
 *	ID_ETO
 */
struct rt_eto_internal {
	long	eto_magic;
	point_t	eto_V;	/* eto vertex */
	vect_t	eto_N;	/* vector normal to plane of torus */
	vect_t	eto_C;	/* vector along semi-major axis of ellipse */
	fastf_t	eto_r;	/* scalar radius of rotation */
	fastf_t	eto_rd;	/* scalar length of semi-minor of ellipse */
};
#define RT_ETO_INTERNAL_MAGIC	0xaaccee92
#define RT_ETO_CK_MAGIC(_p)	RT_CKMAG(_p,RT_ETO_INTERNAL_MAGIC,"rt_eto_internal")

/*
 *	ID_DSP
 */
#define DSP_NAME_LEN 128
struct rt_dsp_internal{
	long		magic;
	char		dsp_file[DSP_NAME_LEN];	/* name of data file */
	unsigned	dsp_xcnt;		/* # samples in row of data */
	unsigned	dsp_ycnt;		/* # of columns in data */
	int		dsp_smooth;		/* bool: surf normal interp */
	double		dsp_xs;
	double		dsp_ys;
	double		dsp_zs;
	mat_t		dsp_mtos;		/* model to solid space */
	/* END OF USER SETABLE VARIABLES, BEGIN INTERNAL STUFF */
	mat_t		dsp_stom;		/* solid to model space */
	struct rt_mapped_file *dsp_mp;	/* actual data */
};
#define RT_DSP_INTERNAL_MAGIC	0xde6
#define RT_DSP_CK_MAGIC(_p)	RT_CKMAG(_p,RT_DSP_INTERNAL_MAGIC,"rt_dsp_internal")

/*
 *	ID_SKETCH
 */

#define SKETCH_NAME_LEN	16
struct rt_sketch_internal
{
	long		magic;
	point_t		V;		/* default embedding of sketch */
	vect_t		u_vec;
	vect_t		v_vec;
	int		vert_count;	/* number of vertices in this sketch */
	point2d_t	*verts;		/* array of 2D vertices that may be used as
					 * endpoints, centers, or NURB control points */

	int		curve_count;	/* number of curves in this sketch */
	struct curve {
		char		crv_name[SKETCH_NAME_LEN];
		int		seg_count;	/* number of segments in this curve */
		int		*reverse;	/* array of ints indicating if segment should be reversed */
		genptr_t	*segments;	/* array of pointers to segments in this curve */
	} *curves;			/* array of curves in this sketch */
};
#define RT_SKETCH_INTERNAL_MAGIC	0x736b6574	/* sket */
#define RT_SKETCH_CK_MAGIC(_p)	RT_CKMAG(_p,RT_SKETCH_INTERNAL_MAGIC,"rt_sketch_internal")


/*
 *	ID_EXTRUDE
 */

struct rt_extrude_internal
{
	long		magic;
	point_t		V;	/* vertex, start and end point of loop to be extruded */
	vect_t		h;	/* extrusion vector, may not be in (u_vec X v_vec) plane */
	vect_t		u_vec;	/* vector in U parameter direction */
	vect_t		v_vec;	/* vector in V parameter direction */
	int		keypoint;	/* index of keypoint vertex */
	char		sketch_name[SKETCH_NAME_LEN];	/* name of sketch object that defines
						 * the curve to be extruded */

	char		curve_name[SKETCH_NAME_LEN];	/* name of curve (in sketch) to be extruded */
	struct rt_sketch_internal	*skt;	/* pointer to referenced sketch */
};
#define RT_EXTRUDE_INTERNAL_MAGIC	0x65787472	/* extr */
#define RT_EXTRUDE_CK_MAGIC(_p)	RT_CKMAG(_p,RT_EXTRUDE_INTERNAL_MAGIC,"rt_extrude_internal")


#endif /* SEEN_RTGEOM_H */
