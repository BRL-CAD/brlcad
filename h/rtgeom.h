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
 *  Depends on having machine.h, bu.h, vmath.h, and bn.h included first.
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

#ifdef __cplusplus
extern "C" {
#endif

#undef r_a /* defined on alliant in <machine/reg.h> included in signal.h */

#define NAMELEN 16	/* NAMESIZE from db.h (can't call it NAMESIZE!!!!!) */

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
#define RT_TOR_CK_MAGIC(_p)	BU_CKMAG(_p,RT_TOR_INTERNAL_MAGIC,"rt_tor_internal")

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
#define RT_TGC_CK_MAGIC(_p)	BU_CKMAG(_p,RT_TGC_INTERNAL_MAGIC,"rt_tgc_internal")

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
#define RT_ELL_CK_MAGIC(_p)	BU_CKMAG(_p,RT_ELL_INTERNAL_MAGIC,"rt_ell_internal")

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
#define RT_ARB_CK_MAGIC(_p)	BU_CKMAG(_p,RT_ARB_INTERNAL_MAGIC,"rt_arb_internal")

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
#define RT_ARS_CK_MAGIC(_p)	BU_CKMAG(_p,RT_ARS_INTERNAL_MAGIC,"rt_ars_internal")

/*
 *	ID_HALF
 */
struct rt_half_internal  {
	long	magic;
	plane_t	eqn;
};
#define RT_HALF_INTERNAL_MAGIC	0xaa87bbdd
#define RT_HALF_CK_MAGIC(_p)	BU_CKMAG(_p,RT_HALF_INTERNAL_MAGIC,"rt_half_internal")

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
#define RT_GRIP_CK_MAGIC(_p)	BU_CKMAG(_p,RT_GRIP_INTERNAL_MAGIC,"rt_grip_internal")
	
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
#define RT_PG_CK_MAGIC(_p)	BU_CKMAG(_p,RT_PG_INTERNAL_MAGIC,"rt_pg_internal")

/* ID_BSPLINE */
#ifdef NMG_H				/* Only if we have seen struct face_g_snurb */
struct rt_nurb_internal {
	long		magic;
	int	 	nsrf;		/* number of surfaces */
	struct face_g_snurb **srfs;	/* The surfaces themselves */
};

#define RT_NURB_INTERNAL_MAGIC	0x002b2bdd
#define RT_NURB_CK_MAGIC( _p) BU_CKMAG(_p,RT_NURB_INTERNAL_MAGIC,"rt_nurb_internal");
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
	struct bu_mapped_file	*mp;	/* actual data */
};
#define RT_EBM_INTERNAL_MAGIC	0xf901b231
#define RT_EBM_CK_MAGIC(_p)	BU_CKMAG(_p,RT_EBM_INTERNAL_MAGIC,"rt_ebm_internal")

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
#define RT_VOL_CK_MAGIC(_p)	BU_CKMAG(_p,RT_VOL_INTERNAL_MAGIC,"rt_vol_internal")

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
	struct bu_mapped_file	*mp;	/* actual data */
};
#define RT_HF_INTERNAL_MAGIC	0x4846494d
#define RT_HF_CK_MAGIC(_p)	BU_CKMAG(_p,RT_HF_INTERNAL_MAGIC,"rt_hf_internal")

/*
 *	ID_ARBN
 */
struct rt_arbn_internal  {
	long	magic;
	int	neqn;
	plane_t	*eqn;
};
#define RT_ARBN_INTERNAL_MAGIC	0x18236461
#define RT_ARBN_CK_MAGIC(_p)	BU_CKMAG(_p,RT_ARBN_INTERNAL_MAGIC,"rt_arbn_internal")

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
#define RT_PIPE_CK_MAGIC(_p)	BU_CKMAG(_p,RT_PIPE_INTERNAL_MAGIC,"rt_pipe_internal")

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
#define RT_PART_CK_MAGIC(_p)	BU_CKMAG(_p,RT_PART_INTERNAL_MAGIC,"rt_part_internal")

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
#define RT_RPC_CK_MAGIC(_p)	BU_CKMAG(_p,RT_RPC_INTERNAL_MAGIC,"rt_rpc_internal")

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
#define RT_RHC_CK_MAGIC(_p)	BU_CKMAG(_p,RT_RHC_INTERNAL_MAGIC,"rt_rhc_internal")

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
#define RT_EPA_CK_MAGIC(_p)	BU_CKMAG(_p,RT_EPA_INTERNAL_MAGIC,"rt_epa_internal")

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
#define RT_EHY_CK_MAGIC(_p)	BU_CKMAG(_p,RT_EHY_INTERNAL_MAGIC,"rt_ehy_internal")

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
#define RT_ETO_CK_MAGIC(_p)	BU_CKMAG(_p,RT_ETO_INTERNAL_MAGIC,"rt_eto_internal")

/*
 *	ID_DSP
 */
#define DSP_NAME_LEN 128
struct rt_dsp_internal{
	long		magic;
#define dsp_file dsp_name /* for backwards compatibility */
	struct bu_vls	dsp_name;		/* name of data file */
	unsigned int	dsp_xcnt;		/* # samples in row of data */
	unsigned int	dsp_ycnt;		/* # of columns in data */
	unsigned short	dsp_smooth;		/* bool: surf normal interp */
#define DSP_CUT_DIR_ADAPT	'a'
#define DSP_CUT_DIR_llUR	'l'
#define DSP_CUT_DIR_ULlr	'L'
    unsigned char   dsp_cuttype;		/* type of cut to make */
        
	mat_t		dsp_mtos;		/* model to solid space */
	/* END OF USER SETABLE VARIABLES, BEGIN INTERNAL STUFF */
	mat_t		dsp_stom;		/* solid to model space 
						 * computed from dsp_mtos */
	unsigned short	*dsp_buf;		/* actual data */
	struct bu_mapped_file *dsp_mp;		/* mapped file for data */
	struct rt_db_internal *dsp_bip;		/* db object for data */
#define RT_DSP_SRC_V4_FILE	'4'
#define RT_DSP_SRC_FILE	'f'
#define RT_DSP_SRC_OBJ	'o'
	char		dsp_datasrc;		/* which type of data source */
};
#define RT_DSP_INTERNAL_MAGIC	0xde6
#define RT_DSP_CK_MAGIC(_p)	BU_CKMAG(_p,RT_DSP_INTERNAL_MAGIC,"rt_dsp_internal")


/*
 *	ID_SKETCH
 */

#define SKETCH_NAME_LEN	16
struct rt_sketch_internal
{
	long		magic;
	point_t		V;		/* default embedding of sketch */
	vect_t		u_vec;		/* u_vec and v_vec are unit vectors defining the plane of */
	vect_t		v_vec;		/* the sketch */
	int		vert_count;	/* number of vertices in this sketch */
	point2d_t	*verts;		/* array of 2D vertices that may be used as
					 * endpoints, centers, or spline control points */
/* XXX this should have a distinctive name, like rt_curve */
	struct curve {
		int		seg_count;	/* number of segments in this curve */
		int		*reverse;	/* array of ints indicating if segment should be reversed */
		genptr_t	*segments;	/* array of pointers to segments in this curve */
	} skt_curve;				/* the curve in this sketch */
};
#define RT_SKETCH_INTERNAL_MAGIC	0x736b6574	/* sket */
#define RT_SKETCH_CK_MAGIC(_p)	BU_CKMAG(_p,RT_SKETCH_INTERNAL_MAGIC,"rt_sketch_internal")

/*
 *	ID_SUBMODEL
 */
struct rt_submodel_internal {
	long		magic;
	struct bu_vls	file;	/* .g filename, 0-len --> this database. */
	struct bu_vls	treetop;	/* one treetop only */
	int		meth;		/* space partitioning method */
	/* other option flags (lazy prep, etc.)?? */
	/* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
	mat_t		root2leaf;
	const struct db_i *dbip;
};
#define RT_SUBMODEL_INTERNAL_MAGIC	0x7375626d	/* subm */
#define RT_SUBMODEL_CK_MAGIC(_p)	BU_CKMAG(_p,RT_SUBMODEL_INTERNAL_MAGIC,"rt_submodel_internal")

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
	char		*sketch_name;	/* name of sketch object that defines
						 * the curve to be extruded */
	struct rt_sketch_internal	*skt;	/* pointer to referenced sketch */
};

/*	Note that the u_vec and v_vec are not unit vectors, their magnitude and direction are
 *	used for scaling and rotation
 */
#define RT_EXTRUDE_INTERNAL_MAGIC	0x65787472	/* extr */
#define RT_EXTRUDE_CK_MAGIC(_p)	BU_CKMAG(_p,RT_EXTRUDE_INTERNAL_MAGIC,"rt_extrude_internal")

/*
 *	ID_CLINE
 *
 *	Implementation of FASTGEN CLINE element
 */

struct rt_cline_internal
{
	long		magic;
	point_t		v;
	vect_t		h;
	fastf_t		radius;
	fastf_t		thickness; 	/* zero thickness means volume mode */
};
#define	RT_CLINE_INTERNAL_MAGIC		0x43767378	/* CLIN */
#define RT_CLINE_CK_MAGIC(_p)	BU_CKMAG(_p,RT_CLINE_INTERNAL_MAGIC,"rt_cline_internal")

/*
 *	ID_BOT
 */

struct rt_bot_internal
{
	long		magic;
	unsigned char	mode;
	unsigned char	orientation;
	unsigned char	bot_flags;		/* flags, (indicates surface normals available, for example) */
	int		num_vertices;
	int		num_faces;
	int		*faces;			/* array of ints for faces [num_faces*3] */
	fastf_t		*vertices;		/* array of floats for vertices [num_vertices*3] */
	fastf_t		*thickness;		/* array of plate mode thicknesses (corresponds to array of faces)
						 * NULL for modes RT_BOT_SURFACE and RT_BOT_SOLID.
						 */
	struct bu_bitv	*face_mode;		/* a flag for each face indicating thickness is appended to hit point
						 * in ray direction (if bit is set), otherwise thickness is centered
						 * about hit point (NULL for modes RT_BOT_SURFACE and RT_BOT_SOLID).
						 */
	int		num_normals;
	fastf_t		*normals;		/* array of unit surface normals [num_normals*3] */
	int		num_face_normals;	/* current size of the face_normals array below (number of faces in the array) */
	int		*face_normals;		/* array of indices into the "normals" array, one per face vertex [num_face_normals*3] */
};

/* orientationss for BOT */
#define	RT_BOT_UNORIENTED		1	/* unoriented triangles */
#define RT_BOT_CCW			2	/* oriented counter-clockwise */
#define RT_BOT_CW			3	/* oriented clockwise */

/* modes for BOT */
#define RT_BOT_SURFACE			1	/* triangles represent a surface (no volume) */
#define RT_BOT_SOLID			2	/* triangles respresent the boundary of a solid object */
#define	RT_BOT_PLATE			3	/* triangles represent plates. Thicknesses are specified in "thickness" array,
						 * and face mode is specified in "face_mode" bit vector.
						 * This is the FASTGEN "plate" mode. Orientation is ignored. */
#define RT_BOT_PLATE_NOCOS		4	/* same as plate mode, but LOS is set equal to face thickness, not
						 * the thickness divided by the cosine of the obliquity angle */

/* flags for bot_flags */
#define RT_BOT_HAS_SURFACE_NORMALS    0x1     /* This primitive may have surface normals at each face vertex */
#define RT_BOT_USE_NORMALS	      0x2     /* Use the surface normals if they exist */
#define RT_BOT_USE_FLOATS	      0x4     /* Use the single precision version of "tri_specific" during prep */

#define	RT_BOT_INTERNAL_MAGIC		0x626F7472	/* botr */
#define RT_BOT_CK_MAGIC(_p)	BU_CKMAG(_p,RT_BOT_INTERNAL_MAGIC,"rt_bot_internal")


#ifdef __cplusplus
}
#endif

#endif /* SEEN_RTGEOM_H */
