/*
 *			R A Y T R A C E . H
 *
 *  All the data structures and manifest constants
 *  necessary for interacting with the BRL-CAD LIBRT ray-tracing library.
 *
 *  Note that this header file defines many internal data structures,
 *  as well as the library's external (interface) data structures.  These are
 *  provided for the convenience of applications builders.  However,
 *  the internal data structures are subject to change in each release.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005 USA
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 *
 *  Include Sequencing -
 *	#include "conf.h"	/_* Must come before system <> includes *_/
 *	#ifdef USE_STRING_H	/_* OPTIONAL, for strcmp() etc. *_/
 *	#  include <string.h>
 *	#else
 *	#  include <strings.h>
 *	#endif
 *	#include <stdio.h>
 *	#include <math.h>
 *	#include "machine.h"	/_* For fastf_t definition on this machine *_/
 *	#include "externs.h"	/_* OPTIONAL, for defining syscalls *_/
 *	#include "bu.h"
 *	#include "vmath.h"	/_* For vect_t definition *_/
 *	#include "bn.h"
 *	#include "db.h"		/_* OPTIONAL, precedes raytrace.h when used *_/
 *	#include "nmg.h"	/_* OPTIONAL, precedes raytrace.h when used *_/
 *	#include "raytrace.h"
 *	#include "nurb.h"	/_* OPTIONAL, follows raytrace.h when used *_/
 *
 *  Libraries Used -
 *	LIBRT LIBRT_LIBES -lm -lc
 *
 *  $Header$
 */

#ifndef RAYTRACE_H
#define RAYTRACE_H seen

/*
 *  Auto-include the BRL-CAD Utilities library, and compatability macros
 */
#include "bu.h"
#include "compat4.h"
#include "bn.h"
#include "db5.h"
#include "tcl.h"

#ifndef NMG_H
#include "nmg.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RAYTRACE_H_VERSION	"@(#)$Header$ (BRL)"

/*
 *				D E B U G
 *  
 *  Each type of debugging support is independently controled,
 *  by a separate bit in the word rt_g.debug
 *
 *  For programs based on the "RT" program, these flags follow
 *  the "-x" (lower case x) option.
 */
#define DEBUG_OFF	0	/* No debugging */

/* These definitions are each for one bit */

/* Options useful for debugging applications */
#define DEBUG_ALLRAYS	0x00000001	/* 1 Print calls to rt_shootray() */
#define DEBUG_ALLHITS	0x00000002	/* 2 Print partitions passed to a_hit() */
#define DEBUG_SHOOT	0x00000004	/* 3 Info about rt_shootray() processing */
#define DEBUG_INSTANCE	0x00000008	/* 4 regionid instance revectoring */

/* Options useful for debugging the database */
#define DEBUG_DB	0x00000010	/* 5 Database debugging */
#define DEBUG_SOLIDS	0x00000020	/* 6 Print prep'ed solids */
#define DEBUG_REGIONS	0x00000040	/* 7 Print regions & boolean trees */
#define DEBUG_ARB8	0x00000080	/* 8 Print voluminus ARB8 details */
#define DEBUG_SPLINE	0x00000100	/* 9 Splines */
#define DEBUG_ANIM	0x00000200	/* 10 Animation */
#define DEBUG_ANIM_FULL	0x00000400	/* 11 Animation matrices */
#define DEBUG_VOL	0x00000800	/* 12 Volume solids */

/* Options useful for debugging the library */
#define DEBUG_ROOTS	0x00001000	/* 13 Print rootfinder details */
#define DEBUG_PARTITION	0x00002000	/* 14 Info about bool_weave() */
#define DEBUG_CUT	0x00004000	/* 15 Print space cutting statistics */
#define DEBUG_BOXING	0x00008000	/* 16 Object/box checking details */
#define DEBUG_MEM	0x00010000	/* 17 -->> BU_DEBUG_MEM_LOG */
#define DEBUG_MEM_FULL	0x00020000	/* 18 -->> BU_DEBUG_MEM_CHECK */
#define DEBUG_FDIFF	0x00040000	/* 19 bool/fdiff debugging */
#define DEBUG_PARALLEL	0x00080000	/* 20 -->> BU_DEBUG_PARALLEL */
#define DEBUG_CUTDETAIL	0x00100000	/* 21 Print space cutting details */
#define DEBUG_TREEWALK	0x00200000	/* 22 Database tree traversal */
#define DEBUG_TESTING	0x00400000	/* 23 One-shot debugging flag */
#define DEBUG_ADVANCE	0x00800000	/* 24 Cell-to-cell space partitioning */
#define DEBUG_MATH	0x01000000	/* 25 nmg math routines */

/* Options for debugging particular solids */
#define DEBUG_EBM	0x02000000	/* 26 Extruded bit-map solids */
#define DEBUG_HF	0x04000000	/* 27 Height Field solids */

/* Options which will cause the library to write binary debugging output */
#define DEBUG_PLOTSOLIDS 0x40000000	/* 31 plot all solids */
#define DEBUG_PLOTBOX	0x80000000	/* 32 Plot(3) bounding boxes and cuts */

/* Format string for bu_printb() */
#define DEBUG_FORMAT	\
"\020\040PLOTBOX\
\037PLOTSOLIDS\
\033HF\032EBM\031MATH\030ADVANCE\
\027TESTING\026TREEWALK\025CUTDETAIL\024PARALLEL\023FDIFF\022MEM_FULL\
\021MEM\020BOXING\017CUTTING\016PARTITION\015ROOTS\014VOL\
\013ANIM_FULL\012ANIM\011SPLINE\010ARB8\7REGIONS\6SOLIDS\5DB\
\4INSTANCE\3SHOOT\2ALLHITS\1ALLRAYS"

/*
 *  It is necessary to have a representation of 1.0/0.0, or "infinity"
 *  that fits within the dynamic range of the machine being used.
 *  This constant places an upper bound on the size object which
 *  can be represented in the model.
 */
#if defined(vax) || (defined(sgi) && !defined(mips))
#	define INFINITY	(1.0e20)	/* VAX limit is 10**37 */
#else
#	define INFINITY	(1.0e40)	/* IBM limit is 10**75 */
#endif

#define	RT_BADNUM(n)	(!((n) >= -INFINITY && (n) <= INFINITY))
#define RT_BADVEC(v)	(RT_BADNUM((v)[X]) || RT_BADNUM((v)[Y]) || RT_BADNUM((v)[Z]))

/*
 *  Unfortunately, to prevent divide-by-zero, some tolerancing
 *  needs to be introduced.
 *
 *  RT_LEN_TOL is the shortest length, in mm, that can be stood
 *  as the dimensions of a primitive.
 *  Can probably become at least SMALL.
 *
 *  Dot products smaller than RT_DOT_TOL are considered to have
 *  a dot product of zero, i.e., the angle is effectively zero.
 *  This is used to check vectors that should be perpendicular.
 *  asin(0.1   ) = 5.73917 degrees
 *  asin(0.01  ) = 0.572967
 *  asin(0.001 ) = 0.0572958 degrees
 *  asin(0.0001) = 0.00572958 degrees
 *
 *  sin(0.01 degrees) = sin(0.000174 radians) = 0.000174533
 *
 *  Many TGCs at least, in existing databases, will fail the
 *  perpendicularity test if DOT_TOL is much smaller than 0.001,
 *  which establishes a 1/20th degree tolerance.
 *  The intent is to eliminate grossly bad primitives, not pick nits.
 *
 *  RT_PCOEF_TOL is a tolerance on polynomial coefficients to prevent
 *  the root finder from having heartburn.
 */
#define RT_LEN_TOL	(1.0e-8)
#define RT_DOT_TOL	(0.001)
#define RT_PCOEF_TOL	(1.0e-10)


/*
 *			R T _ T E S S _ T O L
 *
 *  Tessellation (geometric) tolerances,
 *  different beasts than the calcuation tolerance in bn_tol.
 */
struct rt_tess_tol  {
	long		magic;
	double		abs;			/* absolute dist tol */
	double		rel;			/* rel dist tol */
	double		norm;			/* normal tol */
};
#define RT_TESS_TOL_MAGIC	0xb9090dab
#define RT_CK_TESS_TOL(_p)	BU_CKMAG(_p, RT_TESS_TOL_MAGIC, "rt_tess_tol")

/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 *  The setting of USE_PROTOTYPES is done in machine.h
 *  XXX These have been replaced by BU_EXTERN() and BU_ARGS()
 */
#if USE_PROTOTYPES
#	define	RT_EXTERN(type_and_name,args)	extern type_and_name args
#	define	RT_ARGS(args)			args
#else
#	define	RT_EXTERN(type_and_name,args)	extern type_and_name()
#	define	RT_ARGS(args)			()
#endif

/*
 *			R T _ D B _ I N T E R N A L
 *
 *  A handle on the internal format of an MGED database object.
 */
struct rt_db_internal  {
	long		idb_magic;
	int		idb_type;		/* ID_xxx */
	CONST struct rt_functab *idb_meth;	/* for ft_ifree(), etc. */
	genptr_t	idb_ptr;
	struct bu_attribute_value_set idb_avs;
};
#define RT_DB_INTERNAL_MAGIC	0x0dbbd867
#define RT_INIT_DB_INTERNAL(_p)	{(_p)->idb_magic = RT_DB_INTERNAL_MAGIC; \
	(_p)->idb_type = -1; (_p)->idb_ptr = GENPTR_NULL;\
	(_p)->idb_avs.magic = -1;}
#define RT_CK_DB_INTERNAL(_p)	BU_CKMAG(_p, RT_DB_INTERNAL_MAGIC, "rt_db_internal")

/*
 *			D B _ F U L L _ P A T H
 *
 *  For collecting paths through the database tree
 */
struct db_full_path {
	long		magic;
	int		fp_len;
	int		fp_maxlen;
	struct directory **fp_names;	/* array of dir pointers */
};
#define DB_FULL_PATH_POP(_pp)	{(_pp)->fp_len--;}
#define DB_FULL_PATH_CUR_DIR(_pp)	((_pp)->fp_names[(_pp)->fp_len-1])
#define DB_FULL_PATH_GET(_pp,_i)	((_pp)->fp_names[(_i)])
#define DB_FULL_PATH_MAGIC	0x64626670
#define RT_CK_FULL_PATH(_p)	BU_CKMAG(_p, DB_FULL_PATH_MAGIC, "db_full_path")

/*
 *			X R A Y
 *
 * All necessary information about a ray.
 * Not called just "ray" to prevent conflicts with VLD stuff.
 */
struct xray {
	long		magic;
	int		index;		/* Which ray of a bundle */
	point_t		r_pt;		/* Point at which ray starts */
	vect_t		r_dir;		/* Direction of ray (UNIT Length) */
	fastf_t		r_min;		/* entry dist to bounding sphere */
	fastf_t		r_max;		/* exit dist from bounding sphere */
};
#define RAY_NULL	((struct xray *)0)
#define RT_RAY_MAGIC	0x78726179	/* "xray" */
#define RT_CK_RAY(_p)	BU_CKMAG(_p,RT_RAY_MAGIC,"struct xray");

/*
 *			H I T
 *
 *  Information about where a ray hits the surface
 *
 * Important Note:  Surface Normals always point OUT of a solid.
 *
 *  Statement of intent:
 *	The hit_point and hit_normal elements will be removed from this
 *	structure, so as to separate the concept of the solid's normal
 *	at the hit point from the post-boolean normal at the hit point.
 */
struct hit {
	long		hit_magic;
	fastf_t		hit_dist;	/* dist from r_pt to hit_point */
	point_t		hit_point;	/* Intersection point */
	vect_t		hit_normal;	/* Surface Normal at hit_point */
	vect_t		hit_vpriv;	/* PRIVATE vector for xxx_*() */
	genptr_t	hit_private;	/* PRIVATE handle for xxx_shot() */
	int		hit_surfno;	/* solid-specific surface indicator */
	struct xray	*hit_rayp;	/* pointer to defining ray */
};
#define HIT_NULL	((struct hit *)0)
#define RT_HIT_MAGIC	0x20686974	/* " hit" */
#define RT_CK_HIT(_p)	BU_CKMAG(_p,RT_HIT_MAGIC,"struct hit")

/*
 * Old macro:
 *  Only the hit_dist field of pt_inhit and pt_outhit are valid
 *  when a_hit() is called;  to compute both hit_point and hit_normal,
 *  use RT_HIT_NORM() macro;  to compute just hit_point, use
 *  VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
 */
#define RT_HIT_NORM( _hitp, _stp, _unused )  { \
	RT_CK_HIT(_hitp); \
	RT_CK_SOLTAB(_stp); \
	(_stp)->st_meth->ft_norm(_hitp, _stp, (_hitp)->hit_rayp); }

/*
 *  New macro:  Compute normal into (_hitp)->hit_normal, but leave
 *  it un-flipped, as one hit may be shared between multiple partitions
 *  with different flip status.
 *  (Example:  box.r = box.s - sph.s; sph.r = sph.s)
 *  Return the post-boolean normal into caller-provided _normal vector.
 */
#define RT_HIT_NORMAL( _normal, _hitp, _stp, _unused, _flipflag )  { \
	RT_CK_HIT(_hitp); \
	RT_CK_SOLTAB(_stp); \
	RT_CK_FUNCTAB((_stp)->st_meth); \
	(_stp)->st_meth->ft_norm(_hitp, _stp, (_hitp)->hit_rayp); \
	if( _flipflag )  { \
		VREVERSE( _normal, (_hitp)->hit_normal ); \
	} else { \
		VMOVE( _normal, (_hitp)->hit_normal ); \
	} \
 }

/* A more powerful interface would be: */
/* RT_GET_NORMAL( _normal, _partition, inhit/outhit flag, ap ) */



/*
 *			C U R V A T U R E
 *
 *  Information about curvature of the surface at a hit point.
 *  The principal direction pdir has unit length and principal curvature c1.
 *  |c1| <= |c2|, i.e. c1 is the most nearly flat principle curvature.
 *  A POSITIVE curvature indicates that the surface bends TOWARD the
 *  (outward pointing) normal vector at that point.
 *  c1 and c2 are the inverse radii of curvature.
 *  The other principle direction is implied: pdir2 = normal x pdir1.
 */
struct curvature {
	vect_t		crv_pdir;	/* Principle direction */
	fastf_t		crv_c1;		/* curvature in principle dir */
	fastf_t		crv_c2;		/* curvature in other direction */
};
#define CURVE_NULL	((struct curvature *)0)

/*
 *  Use this macro after having computed the normal, to
 *  compute the curvature at a hit point.
 *
 *  In Release 4.4 and earlier, this was called RT_CURVE().
 *  When the extra argument was added the name was changed.
 */
#define RT_CURVATURE( _curvp, _hitp, _flipflag, _stp )  { \
	RT_CK_HIT(_hitp); \
	RT_CK_SOLTAB(_stp); \
	RT_CK_FUNCTAB((_stp)->st_meth); \
	(_stp)->st_meth->ft_curve( _curvp, _hitp, _stp ); \
	if( _flipflag )  { \
		(_curvp)->crv_c1 = - (_curvp)->crv_c1; \
		(_curvp)->crv_c2 = - (_curvp)->crv_c2; \
	} \
 }

/* A more powerful interface would be: */
/* RT_GET_CURVATURE(_curvp, _partition, inhit/outhit flag, ap) */

/*
 *			U V C O O R D
 *
 *  Mostly for texture mapping, information about parametric space.
 */
struct uvcoord {
	fastf_t		uv_u;		/* Range 0..1 */
	fastf_t		uv_v;		/* Range 0..1 */
	fastf_t		uv_du;		/* delta in u */
	fastf_t		uv_dv;		/* delta in v */
};
#define RT_HIT_UVCOORD( ap, _stp, _hitp, uvp )  { \
	RT_CK_HIT(_hitp); \
	RT_CK_SOLTAB(_stp); \
	RT_CK_FUNCTAB((_stp)->st_meth); \
	(_stp)->st_meth->ft_uv( ap, _stp, _hitp, uvp ); }

/* A more powerful interface would be: */
/* RT_GET_UVCOORD(_uvp, _partition, inhit/outhit flag, ap) */


/*
 *			S E G
 *
 * Intersection segment.
 *
 * Includes information about both endpoints of intersection.
 * Contains forward link to additional intersection segments
 * if the intersection spans multiple segments (eg, shooting
 * a ray through a torus).
 */
struct seg {
	struct bu_list	l;
	struct hit	seg_in;		/* IN information */
	struct hit	seg_out;	/* OUT information */
	struct soltab	*seg_stp;	/* pointer back to soltab */
};
#define RT_SEG_NULL	((struct seg *)0)
#define RT_SEG_MAGIC	0x98bcdef1

#define RT_CHECK_SEG(_p)	BU_CKMAG(_p, RT_SEG_MAGIC, "struct seg")
#define RT_CK_SEG(_p)		BU_CKMAG(_p, RT_SEG_MAGIC, "struct seg")

#define RT_GET_SEG(p,res)    { \
	while( !BU_LIST_WHILE((p),seg,&((res)->re_seg)) || !(p) ) \
		rt_get_seg(res); \
	BU_LIST_DEQUEUE( &((p)->l) ); \
	(p)->l.forw = (p)->l.back = BU_LIST_NULL; \
	(p)->seg_in.hit_magic = (p)->seg_out.hit_magic = RT_HIT_MAGIC; \
	res->re_segget++; }

#define RT_FREE_SEG(p,res)  { \
	RT_CHECK_SEG(p); \
	BU_LIST_INSERT( &((res)->re_seg), &((p)->l) ); \
	res->re_segfree++; }

/*  This could be
 *	BU_LIST_INSERT_LIST( &((_res)->re_seg), &((_segheadp)->l) )
 *  except for security of checking & counting each element this way.
 */
#define RT_FREE_SEG_LIST( _segheadp, _res )	{ \
	register struct seg *_a; \
	while( BU_LIST_WHILE( _a, seg, &((_segheadp)->l) ) )  { \
		BU_LIST_DEQUEUE( &(_a->l) ); \
		RT_FREE_SEG( _a, _res ); \
	} }

/*
 *  Macros to operate on Right Rectangular Parallelpipeds (RPPs).
 * XXX move to vmath.h?
 */
struct bound_rpp {
	point_t	min;
	point_t max;
};

#if 0
/*
 *  Compare two bounding RPPs;  return true if disjoint.
 */
#define RT_2RPP_DISJOINT(_l1, _h1, _l2, _h2) \
	V3RPP_DISJOINT(_l1, _h1, _l2, _h2)

/* Test for point being inside or on an RPP */
#define RT_POINT_IN_RPP(_pt, _min, _max)	\
	V3PT_IN_RPP(_pt, _min, _max)
#endif

/*
 *			S O L T A B
 *
 * Internal information used to keep track of solids in the model
 * Leaf name and Xform matrix are unique identifier.
 */
struct soltab {
	struct bu_list	l;		/* links, headed by rti_headsolid */
	struct bu_list	l2;		/* links, headed by st_dp->d_use_hd */
	CONST struct rt_functab *st_meth; /* pointer to per-solid methods */
	struct rt_i	*st_rtip;	/* "up" pointer to rt_i */
	long		st_uses;	/* Usage count, for instanced solids */
	int		st_id;		/* Solid ident */
	point_t		st_center;	/* Centroid of solid */
	fastf_t		st_aradius;	/* Radius of APPROXIMATING sphere */
	fastf_t		st_bradius;	/* Radius of BOUNDING sphere */
	genptr_t	st_specific;	/* -> ID-specific (private) struct */
	CONST struct directory *st_dp;	/* Directory entry of solid */
	point_t		st_min;		/* min X, Y, Z of bounding RPP */
	point_t		st_max;		/* max X, Y, Z of bounding RPP */
	long		st_bit;		/* solids bit vector index (const) */
	struct bu_ptbl	st_regions;	/* ptrs to regions using this solid (const) */
	matp_t		st_matp;	/* solid coords to model space, NULL=identity */
	struct db_full_path st_path;	/* path from region to leaf */
	/* Experimental stuff for accelerating "pieces" of solids */
	long		st_npieces;	/* # pieces used by this solid */
	long		st_piecestate_num; /* re_pieces[] subscript */
	struct bound_rpp *st_piece_rpps;/* bounding RPP of each piece of this solid */
};
#define st_name		st_dp->d_namep
#define RT_SOLTAB_NULL	((struct soltab *)0)
#define	SOLTAB_NULL	RT_SOLTAB_NULL	/* backwards compat */
#define RT_SOLTAB_MAGIC		0x92bfcde0	/* l.magic */
#define RT_SOLTAB2_MAGIC	0x92bfcde2	/* l2.magic */

#define RT_CHECK_SOLTAB(_p)	BU_CKMAG( _p, RT_SOLTAB_MAGIC, "struct soltab")
#define RT_CK_SOLTAB(_p)	BU_CKMAG( _p, RT_SOLTAB_MAGIC, "struct soltab")

/*
 *  Values for Solid ID.
 */
#define ID_NULL		0	/* Unused */
#define ID_TOR		1	/* Toroid */
#define ID_TGC		2	/* Generalized Truncated General Cone */
#define ID_ELL		3	/* Ellipsoid */
#define ID_ARB8		4	/* Generalized ARB.  V + 7 vectors */
#define ID_ARS		5	/* ARS */
#define ID_HALF		6	/* Half-space */
#define ID_REC		7	/* Right Elliptical Cylinder [TGC special] */
#define ID_POLY		8	/* Polygonal facted object */
#define ID_BSPLINE	9	/* B-spline object */
#define ID_SPH		10	/* Sphere */
#define	ID_NMG		11	/* n-Manifold Geometry solid */
#define ID_EBM		12	/* Extruded bitmap solid */
#define ID_VOL		13	/* 3-D Volume */
#define ID_ARBN		14	/* ARB with N faces */
#define ID_PIPE		15	/* Pipe (wire) solid */
#define ID_PARTICLE	16	/* Particle system solid */
#define ID_RPC		17	/* Right Parabolic Cylinder  */
#define ID_RHC		18	/* Right Hyperbolic Cylinder  */
#define ID_EPA		19	/* Elliptical Paraboloid  */
#define ID_EHY		20	/* Elliptical Hyperboloid  */
#define ID_ETO		21	/* Elliptical Torus  */
#define ID_GRIP		22	/* Pseudo Solid Grip */
#define ID_JOINT	23	/* Pseudo Solid/Region Joint */
#define ID_HF		24	/* Height Field */
#define ID_DSP		25	/* Displacement map */
#define	ID_SKETCH	26	/* 2D sketch */
#define	ID_EXTRUDE	27	/* Solid of extrusion */
#define ID_SUBMODEL	28	/* Instanced submodel */
#define	ID_CLINE	29	/* FASTGEN4 CLINE solid */
#define	ID_BOT		30	/* Bag o' triangles */
#define	ID_MAX_SOLID	30	/* Maximum defined ID_xxx for solids */

/*
 *	Non-geometric objects
 */
#define ID_COMBINATION	31	/* Combination Record */
#define ID_BINEXPM	32	/* Experimental binary */
#define ID_BINUNIF	33	/* Uniform-array binary */
#define ID_BINMIME	34	/* MIME-typed binary */
#define ID_MAXIMUM	35	/* Maximum defined ID_xxx value */

/*
 *			M A T E R _ I N F O
 */
struct mater_info {
	float	ma_color[3];		/* explicit color:  0..1  */
	float	ma_temperature;		/* positive ==> degrees Kelvin */
	char	ma_color_valid;		/* non-0 ==> ma_color is non-default */
	char	ma_cinherit;		/* color: DB_INH_LOWER / DB_INH_HIGHER */
	char	ma_minherit;		/* mater: DB_INH_LOWER / DB_INH_HIGHER */
	char	*ma_shader;		/* shader name & parms */
};

/*
 *			R E G I O N
 *
 *  The region structure.
 */
struct region  {
	struct bu_list	l;		/* magic # and doubly linked list */
	CONST char	*reg_name;	/* Identifying string */
	union tree	*reg_treetop;	/* Pointer to boolean tree */
	int		reg_bit;	/* constant index into Regions[] */
	int		reg_regionid;	/* Region ID code.  If <=0, use reg_aircode */
	int		reg_aircode;	/* Region ID AIR code */
	int		reg_gmater;	/* GIFT Material code */
	int		reg_los;	/* equivalent LOS estimate ?? */
	struct mater_info reg_mater;	/* Real material information */
	genptr_t	reg_mfuncs;	/* User appl. funcs for material */
	genptr_t	reg_udata;	/* User appl. data for material */
	int		reg_transmit;	/* flag:  material transmits light */
	long		reg_instnum;	/* instance number, from d_uses */
	short		reg_all_unions;	/* 1=boolean tree is all unions */
	short		reg_is_fastgen;	/* FASTGEN-compatability mode? */
#define REGION_NON_FASTGEN	0
#define REGION_FASTGEN_PLATE	1
#define REGION_FASTGEN_VOLUME	2
};
#define REGION_NULL	((struct region *)0)
#define RT_REGION_MAGIC	0xdffb8001
#define RT_CK_REGION(_p)	BU_CKMAG(_p,RT_REGION_MAGIC,"struct region")

/*
 *  			P A R T I T I O N
 *
 *  Partitions of a ray.  Passed from rt_shootray() into user's
 *  a_hit() function.
 *
 *  Not changed to a bu_list for backwards compatability, but
 *  you can iterate the whole list by writing:
 *	for( BU_LIST_FOR( pp, partition, (struct bu_list *)PartHeadp ) )
 */

struct partition {
	/* This can be thought of and operated on as a struct bu_list */
	long		pt_magic;		/* sanity check */
	struct partition *pt_forw;		/* forwards link */
	struct partition *pt_back;		/* backwards link */
	struct seg	*pt_inseg;		/* IN seg ptr (gives stp) */
	struct hit	*pt_inhit;		/* IN hit pointer */
	struct seg	*pt_outseg;		/* OUT seg pointer */
	struct hit	*pt_outhit;		/* OUT hit ptr */
	struct region	*pt_regionp;		/* ptr to containing region */
	char		pt_inflip;		/* flip inhit->hit_normal */
	char		pt_outflip;		/* flip outhit->hit_normal */
	struct region	**pt_overlap_reg;	/* NULL-terminated array of overlapping regions.  NULL if no overlap. */
	struct bu_ptbl	pt_seglist;		/* all segs in this partition */
};
#define PT_NULL		((struct partition *)0)
#define PT_MAGIC	0x87687681
#define PT_HD_MAGIC	0x87687680

#define RT_CHECK_PT(_p)	RT_CK_PT(_p)	/* compat */
#define RT_CK_PT(_p)	BU_CKMAG(_p,PT_MAGIC, "struct partition")
#define RT_CK_PARTITION(_p)	BU_CKMAG(_p,PT_MAGIC, "struct partition")
#define RT_CK_PT_HD(_p)	BU_CKMAG(_p,PT_HD_MAGIC, "struct partition list head")

/* Macros for copying only the essential "middle" part of a partition struct */
#define RT_PT_MIDDLE_START	pt_inseg		/* 1st elem to copy */
#define RT_PT_MIDDLE_END	pt_seglist.l.magic	/* copy up to this elem (non-inclusive) */
#define RT_PT_MIDDLE_LEN(p) \
	(((char *)&(p)->RT_PT_MIDDLE_END) - ((char *)&(p)->RT_PT_MIDDLE_START))

#define RT_DUP_PT(ip,new,old,res)	{ \
	GET_PT(ip,new,res); \
	bcopy((char *)(&(old)->RT_PT_MIDDLE_START), (char *)(&(new)->RT_PT_MIDDLE_START), RT_PT_MIDDLE_LEN(old) ); \
	(new)->pt_overlap_reg = NULL; \
	bu_ptbl_cat( &(new)->pt_seglist, &(old)->pt_seglist );  }

/* Clear out the pointers, empty the hit list */
#define GET_PT_INIT(ip,p,res)	{\
	GET_PT(ip,p,res); \
	bzero( ((char *) &(p)->RT_PT_MIDDLE_START), RT_PT_MIDDLE_LEN(p) ); }

#define GET_PT(ip,p,res)   { \
	if( BU_LIST_NON_EMPTY_P(p, partition, &res->re_parthead) )  { \
		BU_LIST_DEQUEUE((struct bu_list *)(p)); \
		bu_ptbl_reset( &(p)->pt_seglist ); \
	} else { \
		(p) = (struct partition *)bu_malloc(sizeof(struct partition), "struct partition"); \
		(p)->pt_magic = PT_MAGIC; \
		bu_ptbl_init( &(p)->pt_seglist, 42, "pt_seglist ptbl" ); \
		(res)->re_partlen++; \
	} \
	res->re_partget++; }

#define FREE_PT(p,res)  { \
	BU_LIST_APPEND( &(res->re_parthead), (struct bu_list *)(p) ); \
	if( (p)->pt_overlap_reg )  { \
		bu_free( (genptr_t)((p)->pt_overlap_reg), "pt_overlap_reg" );\
		(p)->pt_overlap_reg = NULL; \
	} \
	res->re_partfree++; }

#define RT_FREE_PT_LIST( _headp, _res )		{ \
		register struct partition *_pp, *_zap; \
		for( _pp = (_headp)->pt_forw; _pp != (_headp);  )  { \
			_zap = _pp; \
			_pp = _pp->pt_forw; \
			FREE_PT(_zap, _res); \
		} \
		(_headp)->pt_forw = (_headp)->pt_back = (_headp); \
	}

/* Insert "new" partition in front of "old" partition.  Note order change */
#define INSERT_PT(_new,_old)	BU_LIST_INSERT((struct bu_list *)_old,(struct bu_list *)_new)

/* Append "new" partition after "old" partition.  Note arg order change */
#define APPEND_PT(_new,_old)	BU_LIST_APPEND((struct bu_list *)_old,(struct bu_list *)_new)

/* Dequeue "cur" partition from doubly-linked list */
#define DEQUEUE_PT(_cur)	BU_LIST_DEQUEUE((struct bu_list *)_cur)

/*
 *			C U T
 *
 *  Structure for space subdivision.
 *
 *  cut_type is an integer for efficiency of access in rt_shootray()
 *  on non-word addressing machines.
 *
 *  If a solid has 'pieces', it will be listed either in bn_list (initially),
 *  or in bn_piecelist, but not both.
 */
union cutter  {
#define CUT_CUTNODE	1
#define CUT_BOXNODE	2
#define CUT_NUGRIDNODE	3
#define	CUT_MAXIMUM	3	
	int	cut_type;
	union cutter *cut_forw;		/* Freelist forward link */
	struct cutnode  {
		int	cn_type;
		int	cn_axis;	/* 0,1,2 = cut along X,Y,Z */
		fastf_t	cn_point;	/* cut through axis==point */
		union cutter *cn_l;	/* val < point */
		union cutter *cn_r;	/* val >= point */
	} cn;
	struct boxnode  {
		int	bn_type;
		fastf_t	bn_min[3];
		fastf_t	bn_max[3];
		struct soltab **bn_list; /* bn_list[bn_len] */
		int	bn_len;		/* # of solids in list */
		int	bn_maxlen;	/* # of ptrs allocated to list */
		struct rt_piecelist *bn_piecelist; /* [] solids with pieces */
		int	bn_piecelen;	/* # of solids with pieces */
	} bn;
	struct nugridnode {
		int	nu_type;
		struct nu_axis {
			fastf_t nu_spos;	/* cell start position */
			fastf_t	nu_epos;	/* cell end position */
			fastf_t	nu_width;	/* voxel size (end - start) */
		} *nu_axis[3];
		int	 nu_cells_per_axis[3]; /* number of slabs */
		int	 nu_stepsize[3];       /* number of cells to jump for one step in each axis */
		union cutter	*nu_grid;	 /* 3-D array of boxnodes */
	} nugn;
};

#define CUTTER_NULL	((union cutter *)0)

/*
 *			M E M _ M A P
 *
 *  These structures are used to manage internal resource maps.
 *  Typically these maps describe some kind of memory or file space.
 */
struct mem_map {
	struct mem_map	*m_nxtp;	/* Linking pointer to next element */
	unsigned	 m_size;	/* Size of this free element */
	unsigned long	 m_addr;	/* Address of start of this element */
};
#define MAP_NULL	((struct mem_map *) 0)


/*
 *  The directory is organized as forward linked lists hanging off of
 *  one of RT_DBNHASH headers in the db_i structure.
 */
#define	RT_DBNHASH		1024	/* size of hash table */

#if	((RT_DBNHASH)&((RT_DBNHASH)-1)) != 0
#define	RT_DBHASH(sum)	((unsigned)(sum) % (RT_DBNHASH))
#else
#define	RT_DBHASH(sum)	((unsigned)(sum) & ((RT_DBNHASH)-1))
#endif

/*
 *			D B _ I
 *
 *  One of these structures is used to describe each separate instance
 *  of a BRL-CAD model database ".g" file.
 *
 *  dbi_filepath is a C-style argv array of places to search when
 *  opening related files (such as data files for EBM solids or
 *  texture-maps).  The array and strings are all dynamically allocated.
 */
struct db_i  {
	long			dbi_magic;	/* magic number */
	/* THESE ELEMENTS ARE AVAILABLE FOR APPLICATIONS TO READ */
	char			*dbi_filename;	/* file name */
	int			dbi_read_only;	/* !0 => read only file */
	double			dbi_local2base;	/* local2mm */
	double			dbi_base2local;	/* unit conversion factors */
	char			*dbi_title;	/* title from IDENT rec */
	char			*CONST*dbi_filepath; /* search path for aux file opens (convenience var) */
	/* THESE ELEMENTS ARE FOR LIBRT ONLY, AND MAY CHANGE */
	struct directory	*dbi_Head[RT_DBNHASH];
	int			dbi_fd;		/* UNIX file descriptor */
	FILE			*dbi_fp;	/* STDIO file descriptor */
	long			dbi_eof;	/* End+1 pos after db_scan() */
	long			dbi_nrec;	/* # records after db_scan() */
	int			dbi_uses;	/* # of uses of this struct */
	struct mem_map		*dbi_freep;	/* map of free granules */
	genptr_t		dbi_inmem;	/* ptr to in-memory copy */
	struct animate		*dbi_anroot;	/* heads list of anim at root lvl */
	struct bu_mapped_file	*dbi_mf;	/* Only in read-only mode */
	struct bu_ptbl		dbi_clients;	/* List of rtip's using this db_i */
	int			dbi_version;	/* 4 or 5 */
};
#define DBI_NULL	((struct db_i *)0)
#define DBI_MAGIC	0x57204381

#define RT_CHECK_DBI(_p)		BU_CKMAG(_p,DBI_MAGIC,"struct db_i")
#define RT_CHECK_DBI_TCL(_interp,_p)	BU_CKMAG_TCL(_interp,_p,DBI_MAGIC,"struct db_i")
#define RT_CK_DBI(_p)			RT_CHECK_DBI(_p)
#define RT_CK_DBI_TCL(_interp,_p)	RT_CHECK_DBI_TCL(_interp,_p)

/*
 *			D I R E C T O R Y
 *
 *  One of these structures is allocated in memory to represent each
 *  named object in the database.
 *
 *  Note that a d_addr of RT_DIR_PHONY_ADDR (-1L) means that database
 *  storage has not been allocated yet.
 *
 *  Note that there is special handling for RT_DIR_INMEM "in memory" overrides.
 *
 *  Construction should be done only by using RT_GET_DIRECTORY()
 *  Destruction should be done only by using db_dirdelete().
 *
 *  Special note:  In order to reduce the overhead of calling bu_malloc()
 *  (really bu_strdup()) to stash the name in d_namep, we carry along
 *  enough storage for small names right in the structure itself (d_shortname).
 *  Thus, d_namep should never be assigned to directly, it should always
 *  be accessed using RT_DIR_SET_NAMEP() and RT_DIR_FREE_NAMEP().
 *
 *  The in-memory name of an object should only be changed using db_rename(),
 *  so that it can be requeued on the correct linked list, based on new hash.
 *  This should be followed by rt_db_put_internal() on the object to
 *  modify the on-disk name.
 */
struct directory  {
	long		d_magic;		/* Magic number */
	char		*d_namep;		/* pointer to name string */
	union {
		long	file_offset;		/* disk address in obj file */
		genptr_t ptr;			/* ptr to in-memory-only obj */
	} d_un;
	struct directory *d_forw;		/* link to next dir entry */
	struct animate	*d_animate;		/* link to animation */
	long		d_uses;			/* # uses, from instancing */
	long		d_len;			/* # of db granules used */
	long		d_nref;			/* # times ref'ed by COMBs */
	int		d_flags;		/* flags */
	struct bu_list	d_use_hd;		/* heads list of uses (struct soltab l2) */
	char		d_shortname[16];	/* Stash short names locally */
};
#define DIR_NULL	((struct directory *)0)
#define RT_DIR_MAGIC	0x05551212		/* Directory assistance */
#define RT_CK_DIR(_dp)	BU_CKMAG(_dp, RT_DIR_MAGIC, "(librt)directory")

#define d_addr	d_un.file_offset
#define RT_DIR_PHONY_ADDR	(-1L)	/* Special marker for d_addr field */

#define DIR_SOLID	0x1		/* this name is a solid */
#define DIR_COMB	0x2		/* combination */
#define DIR_REGION	0x4		/* region */
#define DIR_USED	0x80		/* One bit, used similar to d_nref */
#define RT_DIR_INMEM	0x100		/* object is in memory (only) */

/* Args to db_lookup() */
#define LOOKUP_NOISY	1
#define LOOKUP_QUIET	0

#define FOR_ALL_DIRECTORY_START(_dp,_dbip)	{ int _i; \
	for( _i = RT_DBNHASH-1; _i >= 0; _i-- )  { \
		for( (_dp) = (_dbip)->dbi_Head[_i]; (_dp); (_dp) = (_dp)->d_forw )  {

#define FOR_ALL_DIRECTORY_END	}}}

#define RT_DIR_SET_NAMEP(_dp,_name)	{ \
	if( strlen(_name) < sizeof((_dp)->d_shortname) )  {\
		strncpy( (_dp)->d_shortname, (_name), sizeof((_dp)->d_shortname) ); \
		(_dp)->d_namep = (_dp)->d_shortname; \
	} else { \
		(_dp)->d_namep = bu_strdup(_name); /* Calls bu_malloc() */ \
	} }

/* Use this macro to free the d_namep member, which is sometimes not dynamic. */
#define RT_DIR_FREE_NAMEP(_dp)	{ \
	if( (_dp)->d_namep != (_dp)->d_shortname )  \
		bu_free((_dp)->d_namep, "d_namep"); \
	(_dp)->d_namep = NULL; }

#if 1
/* The efficient way */
#define RT_GET_DIRECTORY(_p,_res)    { \
	while( ((_p) = (_res)->re_directory_hd) == NULL ) \
		db_get_directory(_res); \
	(_res)->re_directory_hd = (_p)->d_forw; \
	(_p)->d_forw = NULL; }
#else
/* XXX Conservative, for testing parallel problems with Ft. AP Hill */
#define RT_GET_DIRECTORY(_p,_res)    {BU_GETSTRUCT(_p, directory); \
	(_p)->d_magic = RT_DIR_MAGIC; \
	BU_LIST_INIT( &((_p)->d_use_hd) ); }
#endif

	

/*
 *			R T _ C O M B _ I N T E R N A L
 *
 *  In-memory format for database "combination" record (non-leaf node).
 *  (Regions and Groups are both a kind of Combination).
 *  Perhaps move to h/wdb.h or h/rtgeom.h?
 */
struct rt_comb_internal  {
	long		magic;
	union tree	*tree;		/* Leading to tree_db_leaf leaves */
	char		region_flag;	/* !0 ==> this COMB is a REGION */
	char		is_fastgen;	/* REGION_NON_FASTGEN/_PLATE/_VOLUME */
	/* Begin GIFT compatability */
	short		region_id;
	short		aircode;
	short		GIFTmater;
	short		los;
	/* End GIFT compatability */
	char		rgb_valid;	/* !0 ==> rgb[] has valid color */
	unsigned char	rgb[3];
	float		temperature;	/* > 0 ==> region temperature */
	struct bu_vls	shader;
	struct bu_vls	material;
	char		inherit;
};
#define RT_COMB_MAGIC	0x436f6d49	/* "ComI" */
#define RT_CHECK_COMB(_p)		BU_CKMAG( _p , RT_COMB_MAGIC , "rt_comb_internal" )
#define RT_CK_COMB(_p)			RT_CHECK_COMB(_p)
#define RT_CHECK_COMB_TCL(_interp,_p)	BU_CKMAG_TCL(interp,_p,RT_COMB_MAGIC, "rt_comb_internal" )
#define RT_CK_COMB_TCL(_interp,_p)	RT_CHECK_COMB_TCL(_interp,_p)

/*
 *			R T _ B I N U N I F _ I N T E R N A L
 *
 *  In-memory format for database uniform-array binary object.
 *  Perhaps move to h/wdb.h or h/rtgeom.h?
 */
struct rt_binunif_internal {
	long		magic;
	char		type;
	long		count;
	union		{
			    float		*flt;
			    double		*dbl;
			    char		*int8;
			    short		*int16;
			    int			*int32;
			    long		*int64;
			    unsigned char	*uint8;
			    unsigned short	*uint16;
			    unsigned int	*uint32;
			    unsigned long	*uint64;
	}		u;
};
#define RT_BINUNIF_INTERNAL_MAGIC	0x42696e55	/* "BinU" */
#define RT_CHECK_BINUNIF(_p)		BU_CKMAG( _p , RT_BINUNIF_INTERNAL_MAGIC , "rt_binunif_internal" )
#define RT_CK_BINUNIF(_p)		RT_CHECK_BINUNIF(_p)
#define RT_CHECK_BINUNIF_TCL(_interp,_p)	BU_CKMAG_TCL(interp,_p,RT_BINUNIF_MAGIC, "rt_binunif_internal" )
#define RT_CK_BINUNIF_TCL(_interp,_p)	RT_CHECK_BINUNIF_TCL(_interp,_p)

/*
 *			D B _ T R E E _ S T A T E
 *
 *  State for database tree walker db_walk_tree()
 *  and related user-provided handler routines.
 */
struct db_tree_state {
	long		magic;
	struct db_i	*ts_dbip;
	int		ts_sofar;		/* Flag bits */

	int		ts_regionid;	/* GIFT compat region ID code*/
	int		ts_aircode;	/* GIFT compat air code */
	int		ts_gmater;	/* GIFT compat material code */
	int		ts_los;		/* equivalent LOS estimate .. */
	struct mater_info ts_mater;	/* material properties */

			/* XXX ts_mat should be a matrix pointer, not a matrix */
	mat_t		ts_mat;		/* transform matrix */
	int		ts_is_fastgen;	/* REGION_NON_FASTGEN/_PLATE/_VOLUME */

	int		ts_stop_at_regions;	/* else stop at solids */
	int		(*ts_region_start_func) BU_ARGS((
				struct db_tree_state * /*tsp*/,
				struct db_full_path * /*pathp*/,
				CONST struct rt_comb_internal * /* combp */,
				genptr_t client_data
			));
	union tree *	(*ts_region_end_func) BU_ARGS((
				struct db_tree_state * /*tsp*/,
				struct db_full_path * /*pathp*/,
				union tree * /*curtree*/,
				genptr_t client_data
			));
	union tree *	(*ts_leaf_func) BU_ARGS((
				struct db_tree_state * /*tsp*/,
				struct db_full_path * /*pathp*/,
				struct rt_db_internal * /*ip*/,
				genptr_t client_data
			));
	CONST struct rt_tess_tol *ts_ttol;	/* Tessellation tolerance */
	CONST struct bn_tol	*ts_tol;	/* Math tolerance */
#if defined(NMG_H)
	struct model		**ts_m;		/* ptr to ptr to NMG "model" */
#else
	genptr_t		*ts_m;		/* ptr to genptr */
#endif
	struct rt_i		*ts_rtip;	/* Helper for rt_gettrees() */
	struct resource		*ts_resp;	/* Per-CPU data */
};
#define TS_SOFAR_MINUS	1		/* Subtraction encountered above */
#define TS_SOFAR_INTER	2		/* Intersection encountered above */
#define TS_SOFAR_REGION	4		/* Region encountered above */

#define RT_DBTS_MAGIC	0x64627473	/* "dbts" */
#define RT_CK_DBTS(_p)	BU_CKMAG(_p, RT_DBTS_MAGIC, "db_tree_state")

/*
 *			C O M B I N E D _ T R E E _ S T A T E
 */
struct combined_tree_state {
	long			magic;
	struct db_tree_state	cts_s;
	struct db_full_path	cts_p;
};
#define RT_CTS_MAGIC	0x98989123
#define RT_CK_CTS(_p)	BU_CKMAG(_p, RT_CTS_MAGIC, "combined_tree_state")

/*
 *			T R E E
 *
 *  Binary trees representing the Boolean operations between solids.
 */
#define MKOP(x)		(x)

#define OP_SOLID	MKOP(1)		/* Leaf:  tr_stp -> solid */
#define OP_UNION	MKOP(2)		/* Binary: L union R */
#define OP_INTERSECT	MKOP(3)		/* Binary: L intersect R */
#define OP_SUBTRACT	MKOP(4)		/* Binary: L subtract R */
#define OP_XOR		MKOP(5)		/* Binary: L xor R, not both*/
#define OP_REGION	MKOP(6)		/* Leaf: tr_stp -> combined_tree_state */
#define OP_NOP		MKOP(7)		/* Leaf with no effect */
/* Internal to library routines */
#define OP_NOT		MKOP(8)		/* Unary:  not L */
#define OP_GUARD	MKOP(9)		/* Unary:  not L, or else! */
#define OP_XNOP		MKOP(10)	/* Unary:  L, mark region */
#define OP_NMG_TESS	MKOP(11)	/* Leaf: tr_stp -> nmgregion */
/* LIBWDB import/export interface to combinations */
#define OP_DB_LEAF	MKOP(12)	/* Leaf of combination, db fmt */
#define OP_FREE		MKOP(13)	/* Unary:  L has free chain */

union tree {
	long	magic;				/* First word: magic number */
	/* Second word is always OP code */
	struct tree_node {
		long		magic;
		int		tb_op;		/* non-leaf */
		struct region	*tb_regionp;	/* ptr to containing region */
		union tree	*tb_left;
		union tree	*tb_right;
	} tr_b;
	struct tree_leaf {
		long		magic;
		int		tu_op;		/* leaf, OP_SOLID */
		struct region	*tu_regionp;	/* ptr to containing region */
		struct soltab	*tu_stp;
	} tr_a;
	struct tree_cts {
		long		magic;
		int		tc_op;		/* leaf, OP_REGION */
		struct region	*tc_pad;	/* unused */
		struct combined_tree_state	*tc_ctsp;
	} tr_c;
	struct tree_nmgregion {
		long		magic;
		int		td_op;		/* leaf, OP_NMG_TESS */
		CONST char	*td_name;	/* If non-null, dynamic string describing heritage of this region */
#if defined(NMG_H)
		struct nmgregion *td_r;		/* ptr to NMG region */
#else
		genptr_t	td_r;
#endif
	} tr_d;
	struct tree_db_leaf  {
		long		magic;
		int		tl_op;		/* leaf, OP_DB_LEAF */
		matp_t		tl_mat;		/* xform matp, NULL ==> identity */
		char		*tl_name;	/* Name of this leaf (bu_strdup'ed) */
	} tr_l;
};
/* Things which are in the same place in both A & B structures */
#define tr_op		tr_a.tu_op
#define tr_regionp	tr_a.tu_regionp

#define TREE_NULL	((union tree *)0)
#define RT_TREE_MAGIC	0x91191191
#define RT_CK_TREE(_p)	BU_CKMAG(_p, RT_TREE_MAGIC, "union tree")


/*		R T _ T R E E _ A R R A Y
 *
 *	flattened version of the union tree
 */
struct rt_tree_array
{
	union tree	*tl_tree;
	int		tl_op;
};

#define TREE_LIST_NULL	((struct tree_list *)0)

/* Some dubious defines, to support the wdb_obj.c evolution */
#define RT_MAXARGS		9000
#define RT_MAXLINE		10240
#define RT_NAMESIZE		16

/*
 *			R T _ W D B
 *
 *  This data structure is at the core of the "LIBWDB" support for
 *  allowing application programs to read and write BRL-CAD databases.
 *  Many different access styles are supported.
 */
 
struct rt_wdb  {
	struct bu_list	l;
	int		type;
	struct db_i	*dbip;
	struct bu_vls	wdb_name;	/* database object name */
	struct db_tree_state	wdb_initial_tree_state;
	struct rt_tess_tol	wdb_ttol;
	struct bn_tol		wdb_tol;
	struct resource		*wdb_resp;

	/* variables for name prefixing */
	char		wdb_prestr[RT_NAMESIZE];
	int		wdb_ncharadd;
	int		wdb_num_dups;

	/* default region ident codes for this particular database. */
	int		wdb_item_default;/* GIFT region ID */
	int		wdb_air_default;
	int		wdb_mat_default;/* GIFT material code */
	int		wdb_los_default;/* Line-of-sight estimate */
	struct bu_observer	wdb_observers;
};

#define	RT_WDB_MAGIC			0x5f576462
#define RT_CHECK_WDB(_p)		BU_CKMAG(_p,RT_WDB_MAGIC,"rt_wdb")
#define RT_CHECK_WDB_TCL(_interp,_p)	BU_CKMAG_TCL(_interp,_p,RT_WDB_MAGIC,"rt_wdb")
#define RT_CK_WDB(_p)			RT_CHECK_WDB(_p)
#define RT_CK_WDB_TCL(_interp,_p)	RT_CHECK_WDB_TCL(_interp,_p)
#define RT_WDB_NULL		((struct rt_wdb *)NULL)
#define RT_WDB_TYPE_DB_DISK			2
#define RT_WDB_TYPE_DB_DISK_APPEND_ONLY		3
#define RT_WDB_TYPE_DB_INMEM			4
#define RT_WDB_TYPE_DB_INMEM_APPEND_ONLY	5

extern struct rt_wdb HeadWDB;		/* head of BRLCAD database object list */

/*
 * Carl's vdraw stuff.
 */
#define RT_VDRW_PREFIX		"_VDRW"
#define RT_VDRW_PREFIX_LEN	6
#define RT_VDRW_MAXNAME	31
#define RT_VDRW_DEF_COLOR	0xffff00
struct vd_curve {
	struct bu_list	l;
	char		vdc_name[RT_VDRW_MAXNAME+1]; 	/* name array */
	long		vdc_rgb;	/* color */
	struct bu_list	vdc_vhd;	/* head of list of vertices */
};
#define VD_CURVE_NULL		((struct vd_curve *)NULL)

/*
 *			D G _ O B J
 *
 * A drawable geometry object is associated with a database object
 * and is used to maintain lists of geometry that are ready for display.
 * This geometry can come from a Brl-Cad database or from vdraw commands.
 * The drawable geometry object is also capabable of raytracing geometry
 * that comes from a Brl-Cad database.
 */
struct dg_obj {
	struct bu_list	l;
	struct bu_vls		dgo_name;	/* drawable geometry object name */
	struct rt_wdb		*dgo_wdbp;	/* associated database */
	struct bu_list		dgo_headSolid;	/* head of solid list */
	struct bu_list		dgo_headVDraw;	/* head of vdraw list */
	struct vd_curve		*dgo_currVHead;	/* current vdraw head */
	char			*dgo_rt_cmd[RT_MAXARGS];
	int			dgo_rt_cmd_len;
	struct bu_observer	dgo_observers;
};
extern struct dg_obj HeadDGObj;		/* head of drawable geometry object list */
#define RT_DGO_NULL		((struct dg_obj *)NULL)

/*
 *			V I E W _ O B J
 *
 * A view object maintains state for controlling a view.
 */
struct view_obj {
	struct bu_list	l;
	struct bu_vls		vo_name;		/* view object name/cmd */
	fastf_t			vo_scale;
	fastf_t			vo_size;		/* 2.0 * scale */
	fastf_t			vo_invSize;		/* 1.0 / size */
	fastf_t			vo_perspective;		/* perspective angle */
	fastf_t			vo_local2base;		/* scale local units to base units (i.e. mm) */
	fastf_t			vo_base2local;		/* scale base units (i.e. mm) to local units */
	vect_t			vo_aet;
	vect_t			vo_eye_pos;		/* eye position */
	mat_t			vo_rotation;
	mat_t			vo_center;
	mat_t			vo_model2view;
	mat_t			vo_pmodel2view;
	mat_t			vo_view2model;
	mat_t			vo_pmat;		/* perspective matrix */
	struct bu_observer	vo_observers;
};
extern struct view_obj HeadViewObj;		/* head of view object list */
#define RT_VIEW_OBJ_NULL		((struct view_obj *)NULL)

/*
 *			A N I M A T E
 *
 *  Each one of these structures specifies an arc in the tree that
 *  is to be operated on for animation purposes.  More than one
 *  animation operation may be applied at any given arc.  The directory
 *  structure points to a linked list of animate structures
 *  (built by rt_anim_add()), and the operations are processed in the
 *  order given.
 */
struct anim_mat {
	int		anm_op;			/* ANM_RSTACK, ANM_RARC... */
	mat_t		anm_mat;		/* Matrix */
};
#define ANM_RSTACK	1			/* Replace stacked matrix */
#define ANM_RARC	2			/* Replace arc matrix */
#define ANM_LMUL	3			/* Left (root side) mul */
#define ANM_RMUL	4			/* Right (leaf side) mul */
#define ANM_RBOTH	5			/* Replace stack, arc=Idn */

struct rt_anim_property {
	long		magic;
	int		anp_op;			/* RT_ANP_REPLACE, etc */
	struct bu_vls	anp_shader;		/* Update string */
};
#define RT_ANP_REPLACE	1			/* Replace shader string */
#define RT_ANP_APPEND	2			/* Append to shader string */
#define RT_ANP_MAGIC	0x41507270		/* 'APrp' */
#define RT_CK_ANP(_p)	BU_CKMAG((_p), RT_ANP_MAGIC, "rt_anim_property")

struct rt_anim_color {
	int		anc_rgb[3];		/* New color */
};

struct animate {
	long		magic;			/* magic number */
	struct animate	*an_forw;		/* forward link */
	struct db_full_path an_path;		/* (sub)-path pattern */
	int		an_type;		/* AN_MATRIX, AN_COLOR... */
	union animate_specific {
		struct anim_mat		anu_m;
		struct rt_anim_property	anu_p;
		struct rt_anim_color	anu_c;
		float			anu_t;
	}		an_u;
};
#define RT_AN_MATRIX	1			/* Matrix animation */
#define RT_AN_MATERIAL	2			/* Material property anim */
#define RT_AN_COLOR	3			/* Material color anim */
#define RT_AN_SOLID	4			/* Solid parameter anim */
#define RT_AN_TEMPERATURE 5			/* Region temperature */

#define ANIM_NULL	((struct animate *)0)
#define ANIMATE_MAGIC	0x414e4963		/* 1095649635 */
#define RT_CK_ANIMATE(_p)	BU_CKMAG((_p), ANIMATE_MAGIC, "animate")

/*
 *			R T _ Q E L E M
 *
 *	Structure for use by pmalloc()
 */
#define RT_PM_NBUCKETS        18

struct rt_qelem {
        struct rt_qelem *q_forw;
        struct rt_qelem *q_back;
};

struct rt_pm_res {
	struct rt_qelem buckets[RT_PM_NBUCKETS];
	struct rt_qelem adjhead;
};

/*
 *			R T _ H T B L
 *
 *  Support for variable length arrays of "struct hit".
 *  Patterned after the libbu/ptbl.c idea.
 */
struct rt_htbl {
	struct bu_list	l;	/* linked list for caller's use */
	int		end;	/* index of first available location */
	int		blen;	/* # of struct's of storage at *hits */
	struct hit 	*hits;	/* hits[blen] data storage area */
};
#define RT_HTBL_MAGIC		0x6874626c		/* "htbl" */
#define RT_CK_HTBL(_p)		BU_CKMAG(_p, RT_HTBL_MAGIC, "rt_htbl")

/*
 *			R T _ P I E C E S T A T E
 *
 *  Holds onto memory re-used by rt_shootray() from shot to shot.
 *  One of these for each solid which uses pieces.
 *  There is a separate array of these for each cpu.
 *  Storage for the bit vectors is pre-allocated at prep time.
 *  The array is subscripted by st_piecestate_num.
 *  The bit vector is subscripted by values found in rt_piecelist pieces[].
 */
struct rt_piecestate  {
	long		magic;
	long		ray_seqno;	/* res_nshootray */
	struct soltab	*stp;
	struct bu_bitv	*shot;
	fastf_t		mindist;	/* dist ray enters solids bounding volume */
	fastf_t		maxdist;	/* dist ray leaves solids bounding volume */
	struct rt_htbl	htab;		/* accumulating hits here */
	const union cutter *cutp;		/* current bounding volume */
};
#define RT_PIECESTATE_MAGIC	0x70637374	/* pcst */
#define RT_CK_PIECESTATE(_p)	BU_CKMAG(_p, RT_PIECESTATE_MAGIC, "struct rt_piecestate")

/*
 *			R T _ P I E C E L I S T
 *
 *  For each space partitioning cell,
 *  there is one of these for each solid in that cell which uses pieces.
 *  Storage for the array is allocated at cut time, and never changes.
 *
 *  It is expected that the indices allocated by any solid range from
 *  0..(npieces-1).
 *
 *  The piece indices are used as a subscript into a solid-specific table,
 *  and also into the 'shot' bitv of the corresponding rt_piecestate.
 *
 *  The values (subscripts) in pieces[] are specific to a single solid (stp).
 */
struct rt_piecelist  {
	long		magic;
	long		npieces;	/* number of pieces in pieces[] array */
	long		*pieces;	/* pieces[npieces], piece indices */
	struct soltab	*stp;		/* ref back to solid */
};
#define RT_PIECELIST_MAGIC	0x70636c73	/* pcls */
#define RT_CK_PIECELIST(_p)	BU_CKMAG(_p, RT_PIECELIST_MAGIC, "struct rt_piecelist")

/*
 *			R E S O U R C E
 *
 *  Per-CPU statistics and resources.
 *
 *  One of these structures is allocated per processor.
 *  To prevent excessive competition for free structures,
 *  memory is now allocated on a per-processor basis.
 *  The application structure a_resource element specifies
 *  the resource structure to be used;  if uniprocessing,
 *  a null a_resource pointer results in using the internal global
 *  structure (&rt_uniresource),
 *  making initial application development simpler.
 *
 *  Applications are responsible for calling rt_init_resource()
 *  for each resource structure before letting LIBRT use them.
 *
 *  Note that if multiple models are being used, the partition and bitv
 *  structures (which are variable length) will require there to be
 *  ncpus * nmodels resource structures, the selection of which will
 *  be the responsibility of the application.
 *
 *  Per-processor statistics are initially collected in here,
 *  and then posted to rt_i by rt_add_res_stats().
 */
struct resource {
	long		re_magic;	/* Magic number */
	int		re_cpu;		/* processor number, for ID */
	struct bu_list 	re_seg;		/* Head of segment freelist */
	struct bu_ptbl	re_seg_blocks;	/* Table of malloc'ed blocks of segs */
	long		re_seglen;
	long		re_segget;
	long		re_segfree;
	struct bu_list	re_parthead;	/* Head of freelist */
	long		re_partlen;
	long		re_partget;
	long		re_partfree;
	struct bu_list	re_solid_bitv;	/* head of freelist */
	struct bu_list	re_region_ptbl;	/* head of freelist */
	struct bu_list	re_nmgfree;	/* head of NMG hitmiss freelist */
	struct rt_pm_res re_pmem;	/* for use by pmalloc() */
	union tree	**re_boolstack;	/* Stack for rt_booleval() */
	long		re_boolslen;	/* # elements in re_boolstack[] */
	float		*re_randptr;	/* ptr into random number table */
	/* Statistics.  Only for examination by rt_add_res_stats() */
	long		re_nshootray;	/* Calls to rt_shootray() */
	long		re_nmiss_model;	/* Rays pruned by model RPP */
	/* Solid nshots = shot_hit + shot_miss */
	long		re_shots;	/* # calls to ft_shot() */
	long		re_shot_hit;	/* ft_shot() returned a miss */
	long		re_shot_miss;	/* ft_shot() returned a hit */
	/* Optimizations.  Rays not shot at solids */
	long		re_prune_solrpp;/* shot missed solid RPP, ft_shot skipped */
	long		re_ndup;	/* ft_shot() calls skipped for already-ft_shot() solids */
	long		re_nempty_cells; /* number of empty NUgrid cells passed through */
	/* Data for accelerating "pieces" of solids */
	struct rt_piecestate *re_pieces; /* array [rti_nsolids_with_pieces] */
	long		re_piece_ndup;	/* ft_piece_shot() calls skipped for already-ft_shot() solids */
	long		re_piece_shots;	/* # calls to ft_piece_shot() */
	long		re_piece_shot_hit;	/* ft_piece_shot() returned a miss */
	long		re_piece_shot_miss;	/* ft_piece_shot() returned a hit */
	struct bu_ptbl	re_pieces_pending; /* pieces with an odd hit pending */
	/* Per-processor cache of tree unions, to accelerate "tops" and treewalk */
	union tree	*re_tree_hd;  /* Head of free trees */
	long		re_tree_get;
	long		re_tree_malloc;
	long		re_tree_free;
	struct directory *re_directory_hd;
	struct bu_ptbl	re_directory_blocks;	/* Table of malloc'ed blocks */
};
extern struct resource	rt_uniresource;	/* default.  Defined in librt/shoot.c */
#define RESOURCE_NULL	((struct resource *)0)
#define RESOURCE_MAGIC	0x83651835
#define RT_RESOURCE_CHECK(_p)	BU_CKMAG(_p, RESOURCE_MAGIC, "struct resource")
#define RT_CK_RESOURCE(_p)	BU_CKMAG(_p, RESOURCE_MAGIC, "struct resource")

/* More malloc-efficient replacement for BU_GETUNION(tp, tree) */
#define RT_GET_TREE(_tp,_res)	{ \
	if( ((_tp) = (_res)->re_tree_hd) != TREE_NULL )  { \
		(_res)->re_tree_hd = (_tp)->tr_b.tb_left; \
		(_tp)->tr_b.tb_left = TREE_NULL; \
		(_res)->re_tree_get++; \
	} else { \
		GETUNION( _tp, tree ); \
		(_res)->re_tree_malloc++; \
	}\
	}
#define RT_FREE_TREE(_tp,_res)  { \
		(_tp)->tr_b.tb_left = (_res)->re_tree_hd; \
		(_tp)->tr_b.tb_right = TREE_NULL; \
		(_res)->re_tree_hd = (_tp); \
		(_tp)->tr_b.tb_op = OP_FREE; \
		(_res)->re_tree_free++; \
	}

/*
 *			P I X E L _ E X T
 *
 *  This structure is intended to descrbe the area and/or volume represented
 *  by a ray.  In the case of the "rt" program it represents the extent in
 *  model coordinates of the prism behind the pixel being rendered.
 *
 *  The r_pt values of the rays indicate the dimensions and location in model
 *  space of the ray origin (usually the pixel to be rendered). 
 *  The r_dir vectors indicate the edges (and thus the shape) of the prism 
 *  which is formed from the projection of the pixel into space.
 */
#define CORNER_PTS 4
struct pixel_ext {
	unsigned long	magic;
	struct xray	corner[CORNER_PTS];
};
/* This should have had an RT_ prefix */
#define PIXEL_EXT_MAGIC 0x50787400	/* "Pxt" */
#define BU_CK_PIXEL_EXT(_p)	BU_CKMAG(_p, PIXEL_EXT_MAGIC, "struct pixel_ext")

/*
 *			A P P L I C A T I O N
 *
 *  This structure is the only parameter to rt_shootray().
 *  The entire structure should be zeroed (e.g. by bzero() ) before it
 *  is used the first time.
 *
 *  When calling rt_shootray(), these fields are mandatory:
 *	a_ray.r_pt	Starting point of ray to be fired
 *	a_ray.r_dir	UNIT VECTOR with direction to fire in (dir cosines)
 *	a_hit()		Routine to call when something is hit
 *	a_miss()	Routine to call when ray misses everything
 *	a_rt_i		Must be set to the value returned by rt_dirbuild().
 *
 *  In addition, these fields are used by the library.  If they are
 *  set to zero, default behavior will be used.
 *	a_resource	Pointer to CPU-specific resources.  Multi-CPU only.
 *	a_overlap()	DEPRECATED, set a_multioverlap() instead.
 *			If non-null, this routine will be called to
 *			handle overlap conditions.  See librt/bool.c
 *			for calling sequence.
 *			Return of 0 eliminates partition with overlap entirely
 *			Return of !0 retains one partition in output
 *	a_multioverlap() Called when two or more regions overlap in a partition.
 *			Default behavior used if pointer not set.
 *			See librt/bool.c for calling sequence.
 *	a_level		Printed by librt on errors, but otherwise not used.
 *	a_x		Printed by librt on errors, but otherwise not used.
 *	a_y		Printed by librt on errors, but otherwise not used.
 *	a_purpose	Printed by librt on errors, but otherwise not used.
 *	a_rbeam		Used to compute beam coverage on geometry,
 *	a_diverge	for spline subdivision & many UV mappings.
 *
 *  Note that rt_shootray() returns the (int) return of the a_hit()/a_miss()
 *  function called, as well as placing it in a_return.
 *  A future "multiple rays at a time" interface will only provide a_return.
 *
 *  Note that the organization of this structure, and the details of
 *  the non-mandatory elements are subject to change in every release.
 *  Therefore, rather than using compile-time structure initialization,
 *  you should create a zeroed-out structure, and then assign the intended
 *  values at runtime.  A zeroed structure can be obtained at compile
 *  time with "static CONST struct application zero_ap;", or at run time
 *  by using "bzero( (char *)ap, sizeof(struct application) );" or
 *  bu_calloc( 1, sizeof(struct application), "application" );
 *  While this practice may not work on machines where "all bits off"
 *  does not signify a floating point zero, BRL-CAD does not support any
 *  such machines, so this is a moot issue.
 */
struct application  {
	long		a_magic;
	/* THESE ELEMENTS ARE MANDATORY */
	struct xray	a_ray;		/* Actual ray to be shot */
	int		(*a_hit)BU_ARGS( (struct application *, struct partition *, struct seg *));	/* called when shot hits model */
	int		(*a_miss)BU_ARGS( (struct application *));	/* called when shot misses */
	int		a_onehit;	/* flag to stop on first hit */
	fastf_t		a_ray_length;	/* distance from ray start to end intersections */
	struct rt_i	*a_rt_i;	/* this librt instance */
	int		a_zero1;	/* must be zero (sanity check) */
	/* THESE ELEMENTS ARE USED BY THE LIBRARY, BUT MAY BE LEFT ZERO */
	struct resource	*a_resource;	/* dynamic memory resources */
	int		(*a_overlap)();	/* DEPRECATED */
	void		(*a_multioverlap)BU_ARGS( (struct application *, struct partition *, struct bu_ptbl *, struct partition *) );	/* called to resolve overlaps */
	void		(*a_logoverlap)BU_ARGS( (struct application *, CONST struct partition *, CONST struct bu_ptbl *, CONST struct partition *) );	/* called to log overlaps */
	int		a_level;	/* recursion level (for printing) */
	int		a_x;		/* Screen X of ray, if applicable */
	int		a_y;		/* Screen Y of ray, if applicable */
	char		*a_purpose;	/* Debug string:  purpose of ray */
	fastf_t		a_rbeam;	/* initial beam radius (mm) */
	fastf_t		a_diverge;	/* slope of beam divergance/mm */
	int		a_return;	/* Return of a_hit()/a_miss() */
	int		a_no_booleans;	/* 1= partitions==segs, no booleans */
	/* THESE ELEMENTS ARE USED BY THE PROGRAM "rt" AND MAY BE USED BY */
	/* THE LIBRARY AT SOME FUTURE DATE */
	/* AT THIS TIME THEY MAY BE LEFT ZERO */
	struct pixel_ext *a_pixelext;	/* locations of pixel corners */
	/* THESE ELEMENTS ARE WRITTEN BY THE LIBRARY, AND MAY BE READ IN a_hit() */
	struct seg	*a_finished_segs_hdp;
	struct partition *a_Final_Part_hdp;
	/* THE FOLLOWING ELEMENTS ARE MAINLINE & APPLICATION SPECIFIC. */
	/* THEY ARE NEVER EXAMINED BY THE LIBRARY. */
	int		a_user;		/* application-specific value */
	genptr_t	a_uptr;		/* application-specific pointer */
	struct bn_tabdata *a_spectrum;	/* application-specific bn_tabdata prointer */
	fastf_t		a_color[3];	/* application-specific color */
	fastf_t		a_dist;		/* application-specific distance */
	vect_t		a_uvec;		/* application-specific vector */
	vect_t		a_vvec;		/* application-specific vector */
	fastf_t		a_refrac_index;	/* current index of refraction */
	fastf_t		a_cumlen;	/* cumulative length of ray */
	int		a_zero2;	/* must be zero (sanity check) */
};
#define RT_AFN_NULL	((int (*)())0)
#define RT_AP_MAGIC	0x4170706c	/* "Appl" */
#define RT_CK_AP(_p)	BU_CKMAG(_p,RT_AP_MAGIC,"struct application")
#define RT_CK_APPLICATION(_p)	BU_CKMAG(_p,RT_AP_MAGIC,"struct application")
#define RT_CK_AP_TCL(_interp,_p)	BU_CKMAG_TCL(_interp,_p,RT_AP_MAGIC,"struct application")
#define RT_APPLICATION_INIT(_p)	{ \
		bzero( (char *)(_p), sizeof(struct application) ); \
		(_p)->a_magic = RT_AP_MAGIC; \
	}

#define RT_AP_CHECK(_ap)	\
	{if((_ap)->a_zero1||(_ap)->a_zero2) \
		rt_bomb("corrupt application struct"); }

/*
 *			R T _ G
 *
 *  Definitions for librt.a which are global to the library
 *  regardless of how many different models are being worked on
 */
struct rt_g {
	int		debug;		/* !0 for debug, see librt/debug.h */
	/* XXX rtg_parallel is not used by LIBRT any longer */
	int		rtg_parallel;	/* !0 = trying to use multi CPUs */
	struct bu_list	rtg_vlfree;	/* head of bn_vlist freelist */
	int		NMG_debug;	/* debug bits for NMG's see h/nmg.h */
	struct rt_wdb	rtg_headwdb;	/* head of database object list */
};
extern struct rt_g rt_g;

/*
 *			S E M A P H O R E S
 *
 *  Definition of global parallel-processing semaphores.
 *
 * res_syscall is now	BU_SEM_SYSCALL
 */
#define RT_SEM_TREE0	(BU_SEM_LAST)
#define RT_SEM_TREE1	(RT_SEM_TREE0+1)
#define RT_SEM_TREE2	(RT_SEM_TREE1+1)
#define RT_SEM_TREE3	(RT_SEM_TREE2+1)
#define RT_SEM_WORKER	(RT_SEM_TREE3+1)
#define RT_SEM_STATS	(RT_SEM_WORKER+1)
#define RT_SEM_RESULTS	(RT_SEM_STATS+1)
#define RT_SEM_MODEL	(RT_SEM_RESULTS+1)

#define RT_SEM_LAST	(RT_SEM_MODEL+1)	/* Call bu_semaphore_init( RT_SEM_LAST ); */


/*
 *			R T _ I
 *
 *  Definitions for librt which are specific to the
 *  particular model being processed, one copy for each model.
 *  Initially, a pointer to this is returned from rt_dirbuild().
 *
 *  During gettree processing, the most time consuming step is
 *  searching the list of existing solids to see if a new solid is
 *  actually an identical instance of a previous solid.
 *  Therefore, the list has been divided into several lists.
 *  The same macros & hash value that accesses the dbi_Head[] array
 *  are used here.  The hash value is computed by db_dirhash().
 */
struct rt_i {
	long		rti_magic;	/* magic # for integrity check */
	/* THESE ITEMS ARE AVAILABLE FOR APPLICATIONS TO READ & MODIFY */
	int		useair;		/* 1="air" regions are retained while prepping */
	int		rti_save_overlaps; /* 1=fill in pt_overlap_reg, change boolweave behavior */
	int		rti_dont_instance; /* 1=Don't compress instances of solids into 1 while prepping */
	int		rti_hasty_prep;	/* 1=hasty prep, slower ray-trace */
	int		rti_nlights;	/* number of light sources */
	int		rti_prismtrace; /* add support for pixel prism trace */
	char		*rti_region_fix_file; /* rt_regionfix() file or NULL */
	int		rti_space_partition;  /* space partitioning method */
	int		rti_nugrid_dimlimit;  /* limit on nugrid dimensions */
	struct bn_tol	rti_tol;	/* Math tolerances for this model */
	struct rt_tess_tol rti_ttol;	/* Tessellation tolerance defaults */
	fastf_t		rti_max_beam_radius; /* Max threat radius for FASTGEN cline solid */
	/* THESE ITEMS ARE AVAILABLE FOR APPLICATIONS TO READ */
	point_t		mdl_min;	/* min corner of model bounding RPP */
	point_t		mdl_max;	/* max corner of model bounding RPP */
	point_t		rti_pmin;	/* for plotting, min RPP */
	point_t		rti_pmax;	/* for plotting, max RPP */
	double		rti_radius;	/* radius of model bounding sphere */
	struct db_i	*rti_dbip;	/* prt to Database instance struct */
	/* THESE ITEMS SHOULD BE CONSIDERED OPAQUE, AND SUBJECT TO CHANGE */
	int		needprep;	/* needs rt_prep */
	struct region	**Regions;	/* ptrs to regions [reg_bit] */
	struct bu_list	HeadRegion;	/* ptr of list of regions in model */
	/* Ray-tracing statistics */
	long		nregions;	/* total # of regions participating */
	long		nsolids;	/* total # of solids participating */
	long		rti_nrays;	/* # calls to rt_shootray() */
	long		nmiss_model;	/* rays missed model RPP */
	long		nshots;		/* # of calls to ft_shot() */
	long		nmiss;		/* solid ft_shot() returned a miss */
	long		nhits;		/* solid ft_shot() returned a hit */
	long		nmiss_tree;	/* shots missed sub-tree RPP */
	long		nmiss_solid;	/* shots missed solid RPP */
	long		ndup;		/* duplicate shots at a given solid */
	long		nempty_cells;	/* number of empty NUgrid cells */
	union cutter	rti_CutHead;	/* Head of cut tree */
	union cutter	rti_inf_box;	/* List of infinite solids */
	union cutter	*rti_CutFree;	/* cut Freelist */
	struct bu_ptbl	rti_busy_cutter_nodes; /* List of "cutter" mallocs */
	struct bu_ptbl	rti_cuts_waiting;
	int		rti_cut_maxlen;	/* max len RPP list in 1 cut bin */
	int		rti_ncut_by_type[CUT_MAXIMUM+1];	/* number of cuts by type */
	int		rti_cut_totobj;	/* # objs in all bins, total */
	int		rti_cut_maxdepth;/* max depth of cut tree */
	struct soltab	**rti_sol_by_type[ID_MAX_SOLID+1];
	int		rti_nsol_by_type[ID_MAX_SOLID+1];
	int		rti_maxsol_by_type;
	int		rti_air_discards;/* # of air regions discarded */
	struct bu_hist rti_hist_cellsize; /* occupancy of cut cells */
	struct bu_hist rti_hist_cell_pieces; /* solid pieces per cell */
	struct bu_hist rti_hist_cutdepth; /* depth of cut tree */
	struct soltab	**rti_Solids;	/* ptrs to soltab [st_bit] */
	struct bu_list	rti_solidheads[RT_DBNHASH]; /* active solid lists */
	struct bu_ptbl	rti_resources;	/* list of 'struct resource'es encountered */
	double		rti_nu_gfactor;	/* constant in numcells computation */
	int		rti_cutlen;	/* goal for # solids per boxnode */
	int		rti_cutdepth;	/* goal for depth of NUBSPT cut tree */
	/* Parameters required for rt_submodel */
	char		*rti_treetop;	/* bu_strduped, for rt_submodel rti's only */
	int		rti_uses;	/* for rt_submodel */
	/* Parameters for accelerating "pieces" of solids */
	int		rti_nsolids_with_pieces; /* #solids using pieces */
};

#define RT_NU_GFACTOR_DEFAULT	1.5	 /* see rt_cut_it() for a description
					    of this */
	
#define RTI_NULL	((struct rt_i *)0)
#define RTI_MAGIC	0x99101658	/* magic # for integrity check */

#define RT_CHECK_RTI(_p)		BU_CKMAG(_p,RTI_MAGIC,"struct rt_i")
#define RT_CHECK_RTI_TCL(_interp,_p)	BU_CKMAG_TCL(_interp,_p,RTI_MAGIC,"struct rt_i")
#define RT_CK_RTI(_p)			RT_CHECK_RTI(_p)
#define RT_CK_RTI_TCL(_interp,_p)	RT_CHECK_RTI_TCL(_interp,_p)

#define	RT_PART_NUBSPT	0
#define RT_PART_NUGRID	1
	
/*
 *  Macros to painlessly visit all the active solids.  Serving suggestion:
 *
 *	RT_VISIT_ALL_SOLTABS_START( stp, rtip )  {
 *		rt_pr_soltab( stp );
 *	} RT_VISIT_ALL_SOLTABS_END
 */
#define RT_VISIT_ALL_SOLTABS_START(_s, _rti)	{ \
	register struct bu_list	*_head = &((_rti)->rti_solidheads[0]); \
	for( ; _head < &((_rti)->rti_solidheads[RT_DBNHASH]); _head++ ) \
		for( BU_LIST_FOR( _s, soltab, _head ) )  {

#define RT_VISIT_ALL_SOLTABS_END	} }

/*
 *  Applications that are going to use RT_ADD_VLIST and RT_GET_VLIST
 *  are required to execute this macro once, first:
 *		BU_LIST_INIT( &rt_g.rtg_vlfree );
 *
 * Note that RT_GET_VLIST and RT_FREE_VLIST are non-PARALLEL.
 */
#define RT_GET_VLIST(p)		BN_GET_VLIST(&rt_g.rtg_vlfree, p)

/* Place an entire chain of bn_vlist structs on the freelist */
#define RT_FREE_VLIST(hd)	BN_FREE_VLIST(&rt_g.rtg_vlfree, hd)

#define RT_ADD_VLIST(hd,pnt,draw)  BN_ADD_VLIST(&rt_g.rtg_vlfree,hd,pnt,draw)



/*
 *  Replacements for definitions from ../h/vmath.h
 */
#undef V2PRINT
#undef VPRINT
#undef HPRINT
#define V2PRINT(a,b)	bu_log("%s (%g, %g)\n", a, (b)[0], (b)[1] );
#define VPRINT(a,b)	bu_log("%s (%g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2])
#define HPRINT(a,b)	bu_log("%s (%g, %g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2], (b)[3])

/*
 *			C O M M A N D _ T A B
 *
 *  Table for driving generic command-parsing routines
 */
struct command_tab {
	char	*ct_cmd;
	char	*ct_parms;
	char	*ct_comment;
	int	(*ct_func)();
	int	ct_min;		/* min number of words in cmd */
	int	ct_max;		/* max number of words in cmd */
};

/*
 *			R T _ P O I N T _ L A B E L S
 *
 *  Used by MGED for labeling vertices of a solid.
 */
struct rt_point_labels {
	char	str[8];
	point_t	pt;
};

/*
 *			R T _ P T _ N O D E
 *
 *  Used by g_rpc.c and others to contain forward-linked lists of points.
 */
struct rt_pt_node {
	point_t			p;	/* a point */
	struct rt_pt_node	*next;	/* ptr to next pt */
};


/*
 *			L I N E _ S E G,  C A R C _ S E G,  N U R B _ S E G
 *	used by the sketch and solid of extrusion
 */

struct line_seg		/* line segment */
{
	long			magic;
	int			start, end;	/* indices into sketch's array of vertices */
};
#define CURVE_LSEG_MAGIC     0x6c736567		/* lseg */

struct carc_seg		/* circular arc segment */
{
	long			magic;
	int			start, end;	/* indices */
	fastf_t			radius;		/* radius < 0.0 -> full circle with start point on
						 * circle and "end" at center */
	int			center_is_left;	/* flag indicating where center of curvature is.
						 * If non-zero, then center is to left of vector
						 * from start to end */
	int			orientation;	/* 0 -> ccw, !0 -> cw */
	int			center;		/* index of vertex at center of arc (only used by rt_extrude_prep and rt_extrude_shot) */
};
#define CURVE_CARC_MAGIC     0x63617263		/* carc */

struct nurb_seg		/* NURB curve segment */
{
	long			magic;
	int			order;		/* order of NURB curve (degree - 1) */
	int			pt_type;	/* type of NURB curve */
	struct knot_vector	k;		/* knot vector for NURB curve */
	int			c_size;		/* number of control points */
	int			*ctl_points;	/* array of indicies for control points */
	fastf_t			*weights;	/* array of weights for control points (NULL if non_rational) */
};
#define CURVE_NURB_MAGIC     0x6e757262		/* nurb */


/*
 *			R T _ F U N C T A B
 *
 *  Object-oriented interface to BRL-CAD geometry.
 *
 *  These are the methods for a notional object class "brlcad_solid".
 *  The data for each instance is found separately in struct soltab.
 *  This table is indexed by ID_xxx value of particular solid
 *  found in st_id, or directly pointed at by st_meth.
 *
 *  This needs to be at the end of the raytrace.h header file,
 *  so that all the structure names are known.
 *  The "union record" and "struct nmgregion" pointers are problematic,
 *  so generic pointers are used when those header files have not yet
 *  been seen.
 *
 *  XXX On SGI, can not use identifiers in prototypes inside structure!
 */
struct rt_functab {
	long	magic;
	char	ft_name[16];
	char	ft_label[8];
	int	ft_use_rpp;
	int	(*ft_prep) BU_ARGS((struct soltab * /*stp*/,
			struct rt_db_internal * /*ip*/,
			struct rt_i * /*rtip*/ ));
	int 	(*ft_shot) BU_ARGS((struct soltab * /*stp*/,
			struct xray * /*rp*/,
			struct application * /*ap*/,	/* has resource */
			struct seg * /*seghead*/ ));
	void	(*ft_print) BU_ARGS((CONST struct soltab * /*stp*/));
	void	(*ft_norm) BU_ARGS((struct hit * /*hitp*/,
			struct soltab * /*stp*/,
			struct xray * /*rp*/));
	int 	(*ft_piece_shot) BU_ARGS((
			struct rt_piecestate * /*psp*/,
			struct rt_piecelist * /*plp*/,
			double /* dist_correction to apply to hit distances */,
			struct xray * /* ray transformed to be near cut cell */,
			struct application * /*ap*/));	/* has resource */
	void 	(*ft_piece_hitsegs) BU_ARGS((
			struct rt_piecestate * /*psp*/,
			struct seg * /*seghead*/,
			struct application * /*ap*/));	/* has resource */
	void	(*ft_uv) BU_ARGS((struct application * /*ap*/,	/* has resource */
			struct soltab * /*stp*/,
			struct hit * /*hitp*/,
			struct uvcoord * /*uvp*/));
	void	(*ft_curve) BU_ARGS((struct curvature * /*cvp*/,
			struct hit * /*hitp*/,
			struct soltab * /*stp*/));
	int	(*ft_classify) BU_ARGS((
			CONST struct soltab * /*stp*/,
			CONST vect_t /*min*/,
			CONST vect_t /*max*/,
			CONST struct bn_tol * /*tol*/));
	void	(*ft_free) BU_ARGS((struct soltab * /*stp*/));
	int	(*ft_plot) BU_ARGS((
			struct bu_list * /*vhead*/,
			struct rt_db_internal * /*ip*/,
			CONST struct rt_tess_tol * /*ttol*/,
			CONST struct bn_tol * /*tol*/));
	void	(*ft_vshot) BU_ARGS((struct soltab * /*stp*/[],
			struct xray *[] /*rp*/,
			struct seg [] /*segp*/, int /*n*/,
			struct application * /*ap*/ ));
#if defined(NMG_H)
	int	(*ft_tessellate) BU_ARGS((
			struct nmgregion ** /*r*/,
			struct model * /*m*/,
			struct rt_db_internal * /*ip*/,
			CONST struct rt_tess_tol * /*ttol*/,
			CONST struct bn_tol * /*tol*/));
	int	(*ft_tnurb) BU_ARGS((
			struct nmgregion ** /*r*/,
			struct model * /*m*/,
			struct rt_db_internal * /*ip*/,
			CONST struct bn_tol * /*tol*/));
#else
	int	(*ft_tessellate) BU_ARGS((
			genptr_t * /*r*/,
			genptr_t /*m*/,
			struct rt_db_internal * /*ip*/,
			CONST struct rt_tess_tol * /*ttol*/,
			CONST struct bn_tol * /*tol*/));
	int	(*ft_tnurb) BU_ARGS((
			genptr_t * /*r*/,
			genptr_t /*m*/,
			struct rt_db_internal * /*ip*/,
			CONST struct bn_tol * /*tol*/));
#endif
	int	(*ft_import5) BU_ARGS((struct rt_db_internal * /*ip*/,
			CONST struct bu_external * /*ep*/,
			CONST mat_t /*mat*/,
			CONST struct db_i * /*dbip*/,
			struct resource * /*resp*/));
	int	(*ft_export5) BU_ARGS((struct bu_external * /*ep*/,
			CONST struct rt_db_internal * /*ip*/,
			double /*local2mm*/,
			CONST struct db_i * /*dbip*/,
			struct resource * /*resp*/));
	int	(*ft_import) BU_ARGS((struct rt_db_internal * /*ip*/,
			CONST struct bu_external * /*ep*/,
			CONST mat_t /*mat*/,
			CONST struct db_i * /*dbip*/,
			struct resource * /*resp*/));
	int	(*ft_export) BU_ARGS((struct bu_external * /*ep*/,
			CONST struct rt_db_internal * /*ip*/,
			double /*local2mm*/,
			CONST struct db_i * /*dbip*/,
			struct resource * /*resp*/));
	void	(*ft_ifree) BU_ARGS((struct rt_db_internal * /*ip*/,
			struct resource * /*resp*/));
	int	(*ft_describe) BU_ARGS((struct bu_vls * /*str*/,
			CONST struct rt_db_internal * /*ip*/,
			int /*verbose*/,
			double /*mm2local*/,
			struct resource * /*resp*/));
	int	(*ft_xform) BU_ARGS((struct rt_db_internal * /*op*/,
			CONST mat_t /*mat*/, struct rt_db_internal * /*ip*/,
			int /*free*/, struct db_i * /*dbip*/,
			struct resource * /*resp*/));
	CONST struct bu_structparse *ft_parsetab;	/* rt_xxx_parse */
	size_t	ft_internal_size;	/* sizeof(struct rt_xxx_internal) */
	unsigned long	ft_internal_magic;	/* RT_XXX_INTERNAL_MAGIC */
#if defined(TCL_OK)
	int	(*ft_tclget) BU_ARGS((Tcl_Interp *,
			CONST struct rt_db_internal *, CONST char *item));
	int	(*ft_tcladjust) BU_ARGS((Tcl_Interp *,
			struct rt_db_internal *,
			int /*argc*/, char ** /*argv*/,
			struct resource * /*resp*/));
	int	(*ft_tclform) BU_ARGS((CONST struct rt_functab *,
			Tcl_Interp *));
#else
	int	(*ft_tclget) BU_ARGS((genptr_t /*interp*/,
			CONST struct rt_db_internal *, CONST char *item));
	int	(*ft_tcladjust) BU_ARGS((genptr_t /*interp*/,
			struct rt_db_internal *,
			int /*argc*/, char ** /*argv*/,
			struct resource * /*resp*/));
	int	(*ft_tclform) BU_ARGS((CONST struct rt_functab *,
			genptr_t /*interp*/));
#endif
	void	(*ft_make) BU_ARGS((CONST struct rt_functab *,
			struct rt_db_internal *, double /*diameter*/));
};
extern CONST struct rt_functab rt_functab[];
extern CONST int rt_nfunctab;
#define RT_FUNCTAB_MAGIC		0x46754e63	/* FuNc */
#define RT_CK_FUNCTAB(_p)	BU_CKMAG(_p, RT_FUNCTAB_MAGIC, "functab" );

#define RT_CLASSIFY_UNIMPLEMENTED	BN_CLASSIFY_UNIMPLEMENTED
#define RT_CLASSIFY_INSIDE		BN_CLASSIFY_INSIDE
#define RT_CLASSIFY_OVERLAPPING		BN_CLASSIFY_OVERLAPPING
#define RT_CLASSIFY_OUTSIDE		BN_CLASSIFY_OUTSIDE

/*
 *			R T _ S H O O T R A Y _ S T A T U S
 *
 *  Internal to shoot.c and bundle.c
 */
struct rt_shootray_status {
	fastf_t			dist_corr;	/* correction distance */
	fastf_t			odist_corr;
	fastf_t			box_start;
	fastf_t			obox_start; 
	fastf_t			box_end;
	fastf_t			obox_end;
	fastf_t			model_start;
	fastf_t			model_end;
	struct xray		newray;		/* closer ray start */
	struct application	*ap;
	struct resource		*resp;
	vect_t			inv_dir;      /* inverses of ap->a_ray.r_dir */
	vect_t			abs_inv_dir;  /* absolute values of inv_dir */
	int			rstep[3];     /* -/0/+ dir of ray in axis */
	CONST union cutter	*lastcut, *lastcell;
	CONST union cutter	*curcut;
	point_t			curmin, curmax;
	int			igrid[3];     /* integer cell coordinates */
	vect_t			tv;	      /* next t intercept values */
	int			out_axis;     /* axis ray will leave through */
	struct rt_shootray_status	*old_status;
	int			box_num;	/* which cell along ray */
};

#define NUGRID_T_SETUP(_ax,_cval,_cno) \
	if( ssp->rstep[_ax] > 0 ) { \
		ssp->tv[_ax] = t0 + (nu_axis[_ax][_cno].nu_epos - _cval) * \
					    ssp->inv_dir[_ax]; \
	} else if( ssp->rstep[_ax] < 0 ) { \
		ssp->tv[_ax] = t0 + (nu_axis[_ax][_cno].nu_spos - _cval) * \
					    ssp->inv_dir[_ax]; \
	} else { \
		ssp->tv[_ax] = INFINITY; \
	}
#define NUGRID_T_ADV(_ax,_cno) \
	if( ssp->rstep[_ax] != 0 )  { \
		ssp->tv[_ax] += nu_axis[_ax][_cno].nu_width * \
			ssp->abs_inv_dir[_ax]; \
	}

#define BACKING_DIST	(-2.0)		/* mm to look behind start point */
#define OFFSET_DIST	0.01		/* mm to advance point into box */

/*********************************************************************************
 *	The following section is an exact copy of what was previously "nmg_rt.h" *
 *      (with minor changes to GET_HITMISS and NMG_FREE_HITLIST                  *
 *	moved here to use rt_g.rtg_nmgfree freelist for hitmiss structs.         *
 *********************************************************************************
 *			N M G _ R T . H
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 *
 *  $Header$
 */

/* defining the following flag will improve NMG raytrace speed by eliminating some checking
 * Use with CAUTION!!! */
#define FAST_NMG	1

#define NMG_HIT_LIST	0
#define NMG_MISS_LIST	1
#define NMG_RT_HIT_MAGIC 0x48697400	/* "Hit" */
#define NMG_RT_HIT_SUB_MAGIC 0x48696d00	/* "Him" */
#define NMG_RT_MISS_MAGIC 0x4d697300	/* "Mis" */


/* These values are for the hitmiss "in_out" variable and indicate the
 * nature of the hit when known
 */
#define HMG_INBOUND_STATE(_hm) (((_hm)->in_out & 0x0f0) >> 4)
#define HMG_OUTBOUND_STATE(_hm) ((_hm)->in_out & 0x0f)


#define NMG_RAY_STATE_INSIDE	1
#define NMG_RAY_STATE_ON	2
#define NMG_RAY_STATE_OUTSIDE	4
#define NMG_RAY_STATE_ANY	8

#define HMG_HIT_IN_IN	0x11	/* hit internal structure */
#define HMG_HIT_IN_OUT	0x14	/* breaking out */
#define HMG_HIT_OUT_IN	0x41	/* breaking in */
#define HMG_HIT_OUT_OUT 0x44	/* edge/vertex graze */
#define HMG_HIT_IN_ON	0x12
#define HMG_HIT_ON_IN	0x21
#define HMG_HIT_ON_ON	0x22
#define HMG_HIT_OUT_ON	0x42
#define HMG_HIT_ON_OUT	0x24
#define HMG_HIT_ANY_ANY	0x88	/* hit on non-3-mainifold */

#define	NMG_VERT_ENTER 1
#define NMG_VERT_ENTER_LEAVE 0
#define NMG_VERT_LEAVE -1
#define NMG_VERT_UNKNOWN -2

#define NMG_HITMISS_SEG_IN 0x696e00	/* "in" */
#define NMG_HITMISS_SEG_OUT 0x6f757400	/* "out" */

struct hitmiss {
	struct bu_list	l;
	struct hit	hit;
	fastf_t		dist_in_plane;	/* distance from plane intersect */
	int		in_out;		/* status of ray as it transitions
					 * this hit point.
					 */
	long		*inbound_use;
	vect_t		inbound_norm;
	long		*outbound_use;
	vect_t		outbound_norm;
	int		start_stop;	/* is this a seg_in or seg_out */
	struct hitmiss	*other;		/* for keeping track of the other
					 * end of the segment when we know
					 * it
					 */
};

#define NMG_CK_HITMISS(hm) {\
	switch (hm->l.magic) { \
	case NMG_RT_HIT_MAGIC: \
	case NMG_RT_HIT_SUB_MAGIC: \
	case NMG_RT_MISS_MAGIC: \
		break; \
	case NMG_MISS_LIST: \
		bu_log("%s[%d]: struct hitmiss has  NMG_MISS_LIST magic #\n",\
			__FILE__, __LINE__); \
		rt_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	case NMG_HIT_LIST: \
		bu_log("%s[%d]: struct hitmiss has  NMG_MISS_LIST magic #\n",\
			__FILE__, __LINE__); \
		rt_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	default: \
		bu_log("%s[%d]: bad struct hitmiss magic: %d:(0x%08x)\n", \
			__FILE__, __LINE__, hm->l.magic, hm->l.magic); \
		rt_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	}\
	if (!hm->hit.hit_private) { \
		bu_log("%s[%d]: NULL hit_private in hitmiss struct\n", \
			__FILE__, __LINE__); \
		rt_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	} \
}

#define NMG_CK_HITMISS_LISTS(a_hit, rd) { \
    for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)){NMG_CK_HITMISS(a_hit);} \
    for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_miss)){NMG_CK_HITMISS(a_hit);} }


/*	Ray Data structure
 *
 * A)	the hitmiss table has one element for each nmg structure in the
 *	nmgmodel.  The table keeps track of which elements have been
 *	processed before and which haven't.  Elements in this table
 *	will either be:
 *		(NULL)		item not previously processed
 *		hitmiss ptr	item previously processed
 *
 *	the 0th item in the array is a pointer to the head of the "hit"
 *	list.  The 1th item in the array is a pointer to the head of the
 *	"miss" list.
 *
 * B)	If plane_pt is non-null then we are currently processing a face
 *	intersection.  The plane_dist and ray_dist_to_plane are valid.
 *	The ray/edge intersector should check the distance from the plane
 *	intercept to the edge and update "plane_closest" if the current
 *	edge is closer to the intercept than the previous closest object.
 */
#define NMG_PCA_EDGE	1
#define NMG_PCA_EDGE_VERTEX 2
#define NMG_PCA_VERTEX 3
#define NMG_RAY_DATA_MAGIC 0x1651771
#define NMG_CK_RD(_rd) NMG_CKMAG(_rd, NMG_RAY_DATA_MAGIC, "ray data");
struct ray_data {
	long magic;
	struct model		*rd_m;
	char			*manifolds; /*  structure 1-3manifold table */
	vect_t			rd_invdir;
	struct xray		*rp;
	struct application	*ap;
	struct seg		*seghead;
	struct soltab 		*stp;
	const struct bn_tol	*tol;
	struct hitmiss	**hitmiss;	/* 1 struct hitmiss ptr per elem. */
	struct bu_list	rd_hit;		/* list of hit elements */
	struct bu_list	rd_miss;	/* list of missed/sub-hit elements */

/* The following are to support isect_ray_face() */

	/* plane_pt is the intercept point of the ray with the plane of the
	 * face.
	 */
	point_t	plane_pt;	/* ray/plane(face) intercept point */

	/* ray_dist_to_plane is the parametric distance along the ray from
	 * the ray origin (rd->rp->r_pt) to the ray/plane intercept point
	 */
	fastf_t		ray_dist_to_plane; /* ray parametric dist to plane */

	/* the "face_subhit" element is a boolean used by isect_ray_face
	 * and [e|v]u_touch_func to record the fact that the ray/(plane/face)
	 * intercept point was within tolerance of an edge/vertex of the face.
	 * In such instances, isect_ray_face does NOT need to generate a hit 
	 * point for the face, as the hit point for the edge/vertex will 
	 * suffice.
	 */
	int		face_subhit;	

	/* the "classifying_ray" flag indicates that this ray is being used to
	 * classify a point, so that the "eu_touch" and "vu_touch" functions
	 * should not be called.
	 */
	int		classifying_ray;
};


#ifdef FAST_NMG
#define GET_HITMISS(_p, _ap) { \
	(_p) = BU_LIST_FIRST( hitmiss, &((_ap)->a_resource->re_nmgfree) ); \
	if( BU_LIST_IS_HEAD( (_p), &((_ap)->a_resource->re_nmgfree ) ) ) \
		(_p) = (struct hitmiss *)bu_calloc(1, sizeof( struct hitmiss ), "hitmiss" ); \
	else \
		BU_LIST_DEQUEUE( &((_p)->l) ); \
	}

#define NMG_FREE_HITLIST(_p, _ap) { \
	BU_CK_LIST_HEAD( (_p) ); \
	BU_LIST_APPEND_LIST( &((_ap)->a_resource->re_nmgfree), (_p) ); \
	}
#else
#define GET_HITMISS(_p) { \
	char str[64]; \
	(void)sprintf(str, "GET_HITMISS %s %d", __FILE__, __LINE__); \
	(_p) = (struct hitmiss *)bu_calloc(1, sizeof(struct hitmiss), str); \
	}

#define FREE_HITMISS(_p) { \
	char str[64]; \
	(void)sprintf(str, "FREE_HITMISS %s %d", __FILE__, __LINE__); \
	(void)bu_free( (char *)_p,  str); \
	}

#define NMG_FREE_HITLIST(_p) { \
	struct hitmiss *_hit; \
	while ( BU_LIST_WHILE(_hit, hitmiss, _p)) { \
		NMG_CK_HITMISS(_hit); \
		BU_LIST_DEQUEUE( &_hit->l ); \
		FREE_HITMISS( _hit ); \
	} }

#endif

#define HIT 1	/* a hit on a face */
#define MISS 0	/* a miss on the face */


#define nmg_rt_bomb(rd, str) { \
	bu_log("%s", str); \
	if (rt_g.NMG_debug & DEBUG_NMGRT) rt_bomb("End of diagnostics"); \
	BU_LIST_INIT(&rd->rd_hit); \
	BU_LIST_INIT(&rd->rd_miss); \
	rt_g.NMG_debug |= DEBUG_NMGRT; \
	nmg_isect_ray_model(rd); \
	(void) nmg_ray_segs(rd); \
	rt_bomb("Should have bombed before this\n"); }

struct nmg_radial {
	struct bu_list	l;
	struct edgeuse	*eu;
	struct faceuse	*fu;		/* Derrived from eu */
	struct shell	*s;		/* Derrived from eu */
	int		existing_flag;	/* !0 if this eu exists on dest edge */
	int		is_crack;	/* This eu is part of a crack. */
	int		is_outie;	/* This crack is an "outie" */
	int		needs_flip;	/* Insert eumate, not eu */
	fastf_t		ang;		/* angle, in radians.  0 to 2pi */
};
#define NMG_RADIAL_MAGIC	0x52614421	/* RaD! */
#define NMG_CK_RADIAL(_p)	NMG_CKMAG(_p, NMG_RADIAL_MAGIC, "nmg_radial")

struct nmg_inter_struct {
	long		magic;
	struct bu_ptbl	*l1;		/* vertexuses on the line of */
	struct bu_ptbl *l2;		/* intersection between planes */
	fastf_t		*mag1;		/* Distances along intersection line */
	fastf_t		*mag2;		/* for each vertexuse in l1 and l2. */
	int		mag_len;	/* Array size of mag1 and mag2 */
	struct shell	*s1;
	struct shell	*s2;
	struct faceuse	*fu1;		/* null if l1 comes from a wire */
	struct faceuse	*fu2;		/* null if l2 comes from a wire */
	struct bn_tol	tol;
	int		coplanar;	/* a flag */
	struct edge_g_lseg	*on_eg;		/* edge_g for line of intersection */
	point_t		pt;		/* 3D line of intersection */
	vect_t		dir;
	point_t		pt2d;		/* 2D projection of isect line */
	vect_t		dir2d;
	fastf_t		*vert2d;	/* Array of 2d vertex projections [index] */
	int		maxindex;	/* size of vert2d[] */
	mat_t		proj;		/* Matrix to project onto XY plane */
	CONST long	*twod;		/* ptr to face/edge of 2d projection */
};
#define NMG_INTER_STRUCT_MAGIC	0x99912120
#define NMG_CK_INTER_STRUCT(_p)	NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")

/*****************************************************************
 *                                                               *
 *          Applications interface to the RT library             *
 *                                                               *
 *****************************************************************/
					/* Read named MGED db, build toc */
BU_EXTERN(struct rt_i *rt_dirbuild, (CONST char *filename, char *buf, int len) );
					/* Prepare for raytracing */
BU_EXTERN(struct rt_i *rt_new_rti, (struct db_i *dbip));
BU_EXTERN(void rt_free_rti, (struct rt_i *rtip));
BU_EXTERN(void rt_prep, (struct rt_i *rtip) );
BU_EXTERN(void rt_prep_parallel, (struct rt_i *rtip, int ncpu) );
					/* DEPRECATED, set a_logoverlap */
BU_EXTERN(int rt_overlap_quietly, (struct application *ap,
	struct partition *pp, struct region *reg1,
	struct region *reg2, struct partition *pheadp) );
BU_EXTERN(void rt_default_multioverlap, (struct application *ap,
	struct partition *pp, struct bu_ptbl *regiontable,
	struct partition *InputHdp));
BU_EXTERN(void rt_silent_logoverlap, (struct application *ap,
	CONST struct partition *pp, CONST struct bu_ptbl *regiontable,
	CONST struct partition *InputHdp));
BU_EXTERN(void rt_default_logoverlap, (struct application *ap,
	CONST struct partition *pp, CONST struct bu_ptbl *regiontable,
	CONST struct partition *InputHdp));
					/* Shoot a ray */
BU_EXTERN(int rt_shootray, (struct application *ap) );
					/* Get expr tree for object */
BU_EXTERN(CONST char *	rt_basename, (CONST char *str));
BU_EXTERN(void rt_free_soltab, (struct soltab   *stp));
BU_EXTERN(int rt_gettree, (struct rt_i *rtip, CONST char *node) );
BU_EXTERN(int rt_gettrees, (struct rt_i	*rtip,
	int argc, CONST char **argv, int ncpus));
					/* Print seg struct */
BU_EXTERN(void rt_pr_seg, (CONST struct seg *segp) );
					/* Print the partitions */
BU_EXTERN(void rt_pr_partitions, (CONST struct rt_i *rtip,
	CONST struct partition *phead, CONST char *title) );
					/* Find solid by leaf name */
BU_EXTERN(struct soltab *rt_find_solid, (CONST struct rt_i *rtip,
	CONST char *name) );
					/* Start the timer */
BU_EXTERN(void rt_prep_timer, (void) );
					/* Read timer, return time + str */
BU_EXTERN(double rt_get_timer, (struct bu_vls *vp, double *elapsed));
					/* Return CPU time, text, & wall clock time */
BU_EXTERN(double rt_read_timer, (char *str, int len) );
					/* Plot a solid */
int rt_plot_solid(
	FILE			*fp,
	struct rt_i		*rtip,
	const struct soltab	*stp,
	struct resource		*resp);
					/* Release storage assoc with rt_i */
BU_EXTERN(void rt_clean, (struct rt_i *rtip) );
BU_EXTERN(int rt_del_regtree, (struct rt_i *rtip, struct region *delregp, struct resource *resp));
					/* Check in-memory data structures */
BU_EXTERN(void rt_ck, (struct rt_i *rtip));
BU_EXTERN(void rt_pr_library_version, () );

/*****************************************************************
 *                                                               *
 *  Internal routines in the RT library.			 *
 *  These routines are *not* intended for Applications to use.	 *
 *  The interface to these routines may change significantly	 *
 *  from release to release of this software.			 *
 *                                                               *
 *****************************************************************/

					/* Weave segs into partitions */
BU_EXTERN(void rt_boolweave, (struct seg *out_hd, struct seg *in_hd,
	struct partition *PartHeadp, struct application *ap) );
					/* Eval booleans over partitions */
BU_EXTERN(int rt_boolfinal, (struct partition *InputHdp,
	struct partition *FinalHdp,
	fastf_t startdist, fastf_t enddist,
	struct bu_ptbl *regionbits, struct application *ap,
	CONST struct bu_bitv *solidbits) );

BU_EXTERN(void rt_grow_boolstack, (struct resource *res) );
					/* Approx Floating compare */
BU_EXTERN(int rt_fdiff, (double a, double b) );
					/* Relative Difference */
BU_EXTERN(double rt_reldiff, (double a, double b) );
					/* Print a soltab */
BU_EXTERN(void rt_pr_soltab, (CONST struct soltab *stp) );
					/* Print a region */
BU_EXTERN(void rt_pr_region, (CONST struct region *rp) );
					/* Print an expr tree */
BU_EXTERN(void rt_pr_tree, (CONST union tree *tp, int lvl) );
					/* Print value of tree for a partition */
BU_EXTERN(void rt_pr_tree_val, (CONST union tree *tp,
	CONST struct partition *partp, int pr_name, int lvl));
					/* Print a partition */
BU_EXTERN(void rt_pr_pt, (CONST struct rt_i *rtip, CONST struct partition *pp) );
					/* Print a bit vector */
BU_EXTERN(void rt_pr_hit, (CONST char *str, CONST struct hit *hitp) );
/* rt_fastf_float, rt_mat_dbmat, rt_dbmat_mat
 * declarations moved to h/db.h */
					/* storage obtainers */
BU_EXTERN(void rt_get_seg, (struct resource *res) );
BU_EXTERN(void rt_cut_it, (struct rt_i *rtip, int ncpu) );
					/* print cut node */
BU_EXTERN(void rt_pr_cut, (CONST union cutter *cutp, int lvl) );
					/* free a cut tree */
BU_EXTERN(void rt_fr_cut, (struct rt_i *rtip, union cutter *cutp) );
					/* regionid-driven color override */

/* bool.c */
extern void rt_rebuild_overlaps(struct partition	*PartHdp,
				struct application	*ap,
				int		rebuild_fastgen_plates_only);


/* mater.c */

BU_EXTERN(void rt_region_color_map, (struct region *regp) );
					/* process ID_MATERIAL record */
void rt_color_addrec( int low, int hi, int r, int g, int b, long addr );
BU_EXTERN(void rt_color_free, () );
					/* extend a cut box */

/* cut.c */
extern void rt_pr_cut_info(const struct rt_i	*rtip,
			   const char		*str);

BU_EXTERN(void rt_cut_extend, (union cutter *cutp, struct soltab *stp,
	CONST struct rt_i *rtip) );
					/* find RPP of one region */
BU_EXTERN(int rt_rpp_region, (struct rt_i *rtip, CONST char *reg_name,
	fastf_t *min_rpp, fastf_t *max_rpp) );
BU_EXTERN(void rt_bomb, (CONST char *s));
BU_EXTERN(int rt_in_rpp, (struct xray *rp, CONST fastf_t *invdir,
		CONST fastf_t *min, CONST fastf_t *max));
BU_EXTERN(CONST union cutter *rt_cell_n_on_ray, (struct application *ap, int n));
extern void rt_cut_clean(struct rt_i *rtip);

/* cmd.c */
char *rt_read_cmd( FILE *fp );	/* Read semi-colon terminated line */
int rt_split_cmd(char **argv, int lim, char *lp);
int rt_do_cmd(struct rt_i *rtip, char *lp, const struct command_tab *tp );  /* do cmd from string via cmd table */


/* The database library */

/* wdb.c */
struct rt_wdb *wdb_fopen( const char *filename );
struct rt_wdb *wdb_dbopen( struct db_i *dbip, int mode );
int wdb_import(
	struct rt_wdb *wdbp,
	struct rt_db_internal *internp,
	const char *name,
	const mat_t mat );
int wdb_export_external(
	struct rt_wdb *wdbp,
	struct bu_external *ep,
	const char *name,
	int flags );
int wdb_put_internal(
	struct rt_wdb *wdbp,
	const char *name,
	struct rt_db_internal *ip,
	double local2mm );
int wdb_export(
	struct rt_wdb *wdbp,
	const char *name,
	genptr_t gp,
	int id,
	double local2mm );
void wdb_close( struct rt_wdb *wdbp );

/* db_anim.c */
extern struct animate  *db_parse_1anim(struct db_i     *dbip,
				       int             argc,
				       const char      **argv);

extern int db_parse_anim(struct db_i     *dbip,
			 int             argc,
			 const char              **argv);



BU_EXTERN(int db_add_anim, (struct db_i *dbip, struct animate *anp, int root) );
BU_EXTERN(int db_do_anim, (struct animate *anp, mat_t stack, mat_t arc,
	struct mater_info *materp) );
BU_EXTERN(void db_free_anim, (struct db_i *dbip) );
BU_EXTERN(void db_write_anim, (FILE *fop, struct animate *anp));
BU_EXTERN(struct animate	*db_parse_1anim, (struct db_i *dbip,
				int argc, CONST char **argv));
void			db_free_1anim( struct animate *anp );

/* db_path.c */
void db_full_path_init( struct db_full_path *pathp );
void db_add_node_to_full_path( struct db_full_path *pp, struct directory *dp );
void db_dup_full_path(struct db_full_path *newp,
	const struct db_full_path *oldp );
void db_extend_full_path( struct db_full_path *pathp, int incr );
void db_append_full_path( struct db_full_path *dest, const struct db_full_path *src );
void db_dup_path_tail(struct db_full_path	*newp,
			     const struct db_full_path	*oldp,
			     int			start);
char *db_path_to_string( const struct db_full_path *pp );
void db_path_to_vls( struct bu_vls *str, const struct db_full_path *pp );
void db_pr_full_path( const char *msg, const struct db_full_path *pathp );
int db_string_to_path(struct db_full_path *pp, const struct db_i *dbip, const char *str);
int db_argv_to_path(register struct db_full_path	*pp,
			   struct db_i			*dbip,
			   int				argc,
			   CONST char			*CONST*argv);
void db_free_full_path(struct db_full_path *pp);
int db_identical_full_paths( const struct db_full_path *a,
				const struct db_full_path *b );
int db_full_path_subset(
	const struct db_full_path *a,
	const struct db_full_path *b );
int db_full_path_search( const struct db_full_path *a,
	const struct directory *dp );


/* db_open.c */
extern void db_sync( struct db_i	*dbip );

					/* open an existing model database */
BU_EXTERN(struct db_i *db_open, ( CONST char *name, CONST char *mode ) );
					/* create a new model database */
BU_EXTERN(struct db_i *db_create, ( CONST char *name ) );
					/* close a model database */
BU_EXTERN(void db_close_client, (struct db_i *dbip, long *client));
BU_EXTERN(void db_close, ( struct db_i *dbip ) );
					/* dump a full copy of a database */
BU_EXTERN(int db_dump, (struct rt_wdb *wdbp, struct db_i *dbip));
BU_EXTERN(struct db_i *db_clone_dbi, (struct db_i *dbip, long *client));
/* db5_alloc.c */

extern int db5_write_free( struct db_i *dbip,
			   struct directory *dp,
			   long length );
extern int db5_realloc( struct db_i *dbip,
			struct directory *dp,
			struct bu_external *ep );

/* db5_io.c */
extern int rt_db_cvt_to_external5(struct bu_external *ext,
				  const char *name,
				  const struct rt_db_internal *ip,
				  double conv2mm,
				  struct db_i *dbip,
				struct resource *resp);

extern int db_wrap_v5_external( struct bu_external *ep, const char *name );

extern int rt_db_get_internal5(struct rt_db_internal	*ip,
			       const struct directory	*dp,
			       const struct db_i	*dbip,
			       const mat_t		mat,
				struct resource		*resp);
extern int rt_db_put_internal5(struct directory	*dp,
			       struct db_i		*dbip,
			       struct rt_db_internal	*ip,
				struct resource		*resp);

extern void db5_make_free_object_hdr( struct bu_external *ep, long length );
extern void db5_make_free_object( struct bu_external *ep, long length );
extern int db5_decode_signed(long			*lenp,
			     const unsigned char	*cp,
			     int			format);

extern int db5_decode_length(long			*lenp,
			     const unsigned char	*cp,
			     int			format);

extern int db5_select_length_encoding( long len );

extern void db5_import_color_table( char *cp );

extern int db5_import_attributes( struct bu_attribute_value_set *avs,
				  const struct bu_external *ap );

extern int db5_get_raw_internal_fp( struct db5_raw_internal	*rip,
				    FILE			*fp);

extern int db5_header_is_valid( const unsigned char *hp );
#define rt_fwrite_internal5	+++__deprecated_rt_fwrite_internal5__+++
BU_EXTERN(int db5_fwrite_ident, (FILE *, CONST char *, double));

extern int db5_put_color_table( struct db_i *dbip );
extern int db5_update_ident( struct db_i *dbip, const char *title, double local2mm );
extern int db_put_external5(struct bu_external *ep, struct directory *dp, struct db_i *dbip);

/* db_comb.c */
int db_tree_nleaves( const union tree *tp );
struct rt_tree_array *db_flatten_tree(
	struct rt_tree_array	*rt_tree_array,
	union tree		*tp,
	int			op,
	int			free,
	struct resource		*resp);
int rt_comb_import4(
	struct rt_db_internal		*ip,
	const struct bu_external	*ep,
	const mat_t			matrix,		/* NULL if identity */
	const struct db_i		*dbip,
	struct resource			*resp);
int rt_comb_export4(
	struct bu_external		*ep,
	const struct rt_db_internal	*ip,
	double				local2mm,
	const struct db_i		*dbip,
	struct resource			*resp);
void db_tree_flatten_describe(
	struct bu_vls		*vls,
	const union tree	*tp,
	int			indented,
	int			lvl,
	double			mm2local,
	struct resource		*resp);
void db_tree_describe( 
	struct bu_vls		*vls,
	const union tree	*tp,
	int			indented,
	int			lvl,
	double			mm2local);
void db_comb_describe(
	struct bu_vls	*str,
	const struct rt_comb_internal	*comb,
	int		verbose,
	double		mm2local,
	struct resource	*resp);
void rt_comb_ifree( struct rt_db_internal *ip, struct resource *resp );
int rt_comb_describe(
	struct bu_vls	*str,
	const struct rt_db_internal *ip,
	int		verbose,
	double		mm2local,
	struct resource *resp);
void db_wrap_v4_external( struct bu_external *op, const char *name );
int db_ck_left_heavy_tree(
	const union tree	*tp,
	int			no_unions);
int db_ck_v4gift_tree( CONST union tree *tp );
union tree *db_mkbool_tree(
	struct rt_tree_array *rt_tree_array,
	int		howfar,
	struct resource	*resp);
union tree *db_mkgift_tree(
	struct rt_tree_array	*trees,
	int			subtreecount,
	struct resource		*resp);

/* g_tgc.c */
extern void rt_pt_sort(register fastf_t t[], int npts);

/* g_ell.c */
extern void rt_ell_16pts(register fastf_t *ov,
			 register fastf_t *V,
			 fastf_t *A,
			 fastf_t *B);


/* roots.c */
extern int rt_poly_roots(bn_poly_t *eqn, bn_complex_t roots[]);


/* db_io.c */
extern int db_write(struct db_i	*dbip,
		    const genptr_t	addr,
		    long		count,
		    long		offset);
extern int db_fwrite_external(FILE			*fp,
			      CONST char		*name,
			      struct bu_external	*ep);


/* It is normal to test for __STDC__ when using *_DEFINED tests but in
 * in this case "union record" is used for db_getmrec's return type.  This
 * requires that the "union_record *db_getmrec" be used whenever 
 * RECORD_DEFINED is defined.
 */
#if defined(RECORD_DEFINED)
					/* malloc & read records */
BU_EXTERN(union record *db_getmrec, ( CONST struct db_i *, CONST struct directory *dp ) );
					/* get several records from db */
BU_EXTERN(int db_get, (CONST struct db_i *, CONST struct directory *dp,
	union record *where, int offset, int len ) );
					/* put several records into db */
BU_EXTERN(int db_put, (struct db_i *, CONST struct directory *dp, union record *where,
	int offset, int len ) );
#else /* RECORD_DEFINED */
					/* malloc & read records */
BU_EXTERN(genptr_t db_getmrec, ( CONST struct db_i *, CONST struct directory *dp ) );
					/* get several records from db */
BU_EXTERN(int db_get, (CONST struct db_i *, CONST struct directory *dp,
	genptr_t where, int offset, int len ) );
					/* put several records into db */
BU_EXTERN(int db_put, ( CONST struct db_i *, CONST struct directory *dp,
	genptr_t where, int offset, int len ) );
#endif /* RECORD_DEFINED */
BU_EXTERN(int db_get_external, ( struct bu_external *ep,
	CONST struct directory *dp, CONST struct db_i *dbip ) );
BU_EXTERN(int db_put_external, ( struct bu_external *ep,
	struct directory *dp, struct db_i *dbip ) );
BU_EXTERN(void db_free_external, ( struct bu_external *ep ) );

/* db_scan.c */
					/* read db (to build directory) */
BU_EXTERN(int db_scan, ( struct db_i *,
	int (*handler)BU_ARGS((struct db_i *, CONST char *name, long addr,
	int nrec, int flags, genptr_t client_data)),
	int do_old_matter, genptr_t client_data ) );
					/* update db unit conversions */
#define db_ident(a,b,c)		+++error+++
int db_update_ident( struct db_i *dbip, const char *title, double local2mm );
int db_fwrite_ident( FILE *fp, const char *title, double local2mm );
BU_EXTERN(void db_conversions, ( struct db_i *, int units ) );
int db_v4_get_units_code( const char *str );

/* db5_scan.c */
int db_dirbuild( struct db_i *dbip );

/* db_lookup.c */
extern int db_get_directory_size( const struct db_i	*dbip );
void db_ck_directory(const struct db_i *dbip);

extern void db_inmem(struct directory	*dp,
		     struct bu_external	*ext,
		     int			flags);

extern int db_is_directory_non_empty( const struct db_i	*dbip);

BU_EXTERN(int db_dirhash, (CONST char *str) );
					/* convert name to directory ptr */
BU_EXTERN(struct directory *db_lookup,( CONST struct db_i *, CONST char *name, int noisy ) );
					/* add entry to directory */
BU_EXTERN(struct directory *db_diradd, ( struct db_i *, CONST char *name, long laddr,
	int len, int flags, genptr_t ptr ) );
					/* delete entry from directory */
BU_EXTERN(int db_dirdelete, ( struct db_i *, struct directory *dp ) );
BU_EXTERN(int db_fwrite_ident, (FILE *, CONST char *, double));
BU_EXTERN(void db_pr_dir, ( CONST struct db_i *dbip ) );
BU_EXTERN(int db_rename, ( struct db_i *, struct directory *, CONST char *newname) );


/* db_match.c */
extern void db_update_nref( struct db_i *dbip, struct resource *resp );

BU_EXTERN(int db_regexp_match, (CONST char *pattern, CONST char *string));
BU_EXTERN(int db_regexp_match_all, (struct bu_vls *dest, struct db_i *dbip, CONST char *pattern));

/* db_alloc.c */
extern int db_flags_internal( const struct rt_db_internal *intern);

					/* allocate "count" granules */
BU_EXTERN(int db_alloc, ( struct db_i *, struct directory *dp, int count ) );
					/* delete "recnum" from entry */
BU_EXTERN(int db_delrec, ( struct db_i *, struct directory *dp, int recnum ) );
					/* delete all granules assigned dp */
BU_EXTERN(int db_delete, ( struct db_i *, struct directory *dp ) );
					/* write FREE records from 'start' */
BU_EXTERN(int db_zapper, ( struct db_i *, struct directory *dp, int start ) );

/* db_tree.c */
void db_dup_db_tree_state(struct db_tree_state *otsp, const struct db_tree_state *itsp);
void db_free_db_tree_state( struct db_tree_state *tsp );
void db_init_db_tree_state( struct db_tree_state *tsp, struct db_i *dbip, struct resource *resp );
BU_EXTERN(struct combined_tree_state *db_new_combined_tree_state,
	(CONST struct db_tree_state *tsp, CONST struct db_full_path *pathp));
BU_EXTERN(struct combined_tree_state *db_dup_combined_tree_state,
	(CONST struct combined_tree_state *old));
BU_EXTERN(void db_free_combined_tree_state,
	(struct combined_tree_state *ctsp));
BU_EXTERN(void db_pr_tree_state, (CONST struct db_tree_state *tsp));
BU_EXTERN(void db_pr_combined_tree_state,
	(CONST struct combined_tree_state *ctsp));
BU_EXTERN(int db_apply_state_from_comb, (struct db_tree_state *tsp,
	CONST struct db_full_path *pathp, CONST struct rt_comb_internal *comb));
BU_EXTERN(int db_apply_state_from_memb, (struct db_tree_state *tsp,
	struct db_full_path *pathp, CONST union tree *tp));
int db_apply_state_from_one_member( struct db_tree_state *tsp,
	struct db_full_path *pathp, const char *cp, int sofar,
	const union tree *tp );
union tree *db_find_named_leaf( union tree *tp, const char *cp );
union tree *db_find_named_leafs_parent( int *side, union tree *tp, const char *cp );
void db_tree_del_lhs( union tree *tp, struct resource *resp );
void db_tree_del_rhs( union tree *tp, struct resource *resp );
int db_tree_del_dbleaf(union tree **tp, const char *cp, struct resource *resp);
void db_tree_mul_dbleaf( union tree *tp, const mat_t mat );
void db_tree_funcleaf(
	struct db_i		*dbip,
	struct rt_comb_internal	*comb,
	union tree		*comb_tree,
	void			(*leaf_func)(),
	genptr_t		user_ptr1,
	genptr_t		user_ptr2,
	genptr_t		user_ptr3 );
int
db_follow_path(
	struct db_tree_state		*tsp,
	struct db_full_path		*total_path,
	CONST struct db_full_path	*new_path,
	int				noisy,
	int				depth );
BU_EXTERN(int db_follow_path_for_state, (struct db_tree_state *tsp,
	struct db_full_path *pathp, CONST char *orig_str, int noisy));
BU_EXTERN(union tree *db_recurse, (struct db_tree_state	*tsp,
	struct db_full_path *pathp,
	struct combined_tree_state **region_start_statepp, genptr_t client_data));
union tree *db_dup_subtree( const union tree *tp, struct resource *resp );
void db_ck_tree( const union tree *tp );
void db_free_tree( union tree *tp, struct resource *resp );
void db_left_hvy_node( union tree *tp );
void db_non_union_push( union tree *tp, struct resource *resp );
int db_count_tree_nodes( const union tree *tp, int count );
int db_is_tree_all_unions( const union tree *tp );
int db_count_subtree_regions( const union tree *tp );
int db_tally_subtree_regions(
	union tree	*tp,
	union tree	**reg_trees,
	int		cur,
	int		lim,
	struct resource *resp);
BU_EXTERN(int db_walk_tree, (struct db_i *dbip, int argc, CONST char **argv,
	int ncpu, CONST struct db_tree_state *init_state,
	int (*reg_start_func) (
		struct db_tree_state * /*tsp*/,
		struct db_full_path * /*pathp*/,
		CONST struct rt_comb_internal * /* combp */,
		genptr_t client_data ),
	union tree * (*reg_end_func) (
		struct db_tree_state * /*tsp*/,
		struct db_full_path * /*pathp*/,
		union tree * /*curtree*/,
		genptr_t client_data ),
	union tree * (*leaf_func) (
		struct db_tree_state * /*tsp*/,
		struct db_full_path * /*pathp*/,
		struct rt_db_internal * /*ip*/,
		genptr_t client_data ),
	genptr_t client_data ));
int db_path_to_mat(
	struct db_i		*dbip,
	struct db_full_path	*pathp,
	mat_t			mat,		/* result */
	int			depth,		/* number of arcs */
	struct resource		*resp);
BU_EXTERN(void db_apply_anims, (struct db_full_path *pathp,
	struct directory *dp, mat_t stck, mat_t arc,
	struct mater_info *materp));
/* XXX db_shader_mat, should be called rt_shader_mat */
int db_region_mat(
	mat_t		m,		/* result */
	struct db_i	*dbip,
	const char	*name,
	struct resource *resp);
int db_shader_mat(
	mat_t			model_to_shader,	/* result */
	const struct rt_i	*rtip,
	const struct region	*rp,
	point_t			p_min,	/* input/output: shader/region min point */
	point_t			p_max,	/* input/output: shader/region max point */
	struct resource		*resp);

/* dir.c */
extern struct rt_i *rt_dirbuild( const char *filename, char *buf, int len );
int rt_db_get_internal(
	struct rt_db_internal	*ip,
	const struct directory	*dp,
	const struct db_i	*dbip,
	const mat_t		mat,
	struct resource		*resp);
int rt_db_put_internal(
	struct directory	*dp,
	struct db_i		*dbip,
	struct rt_db_internal	*ip,
	struct resource		*resp);
extern int rt_fwrite_internal( FILE *fp, const char *name, const struct rt_db_internal *ip, double conv2mm );
extern void rt_db_free_internal( struct rt_db_internal *ip, struct resource *resp );
int rt_db_lookup_internal (
	struct db_i *dbip,
	const char *obj_name,
	struct directory **dpp,
	struct rt_db_internal *ip,
	int noisy,
	struct resource *resp);
extern void rt_optim_tree(register union tree *tp,
			  struct resource *resp);
void db_get_directory(register struct resource *resp);

/* db_walk.c */
BU_EXTERN(void db_functree, (struct db_i *dbip, struct directory *dp,
	void (*comb_func)(struct db_i *, struct directory *, genptr_t),
	void (*leaf_func)(struct db_i *, struct directory *, genptr_t),
	struct resource *resp,
	genptr_t client_data));

/* g_arb.c */
int rt_arb_std_type( const struct rt_db_internal *ip, const struct bn_tol *tol );
void rt_arb_centroid();			/* needs rt_arb_internal for arg list */
int rt_arb_calc_points();		/* needs wdb.h for arg list */
int rt_arb_3face_intersect(
	point_t			point,
	const plane_t		planes[6],
	int			type,		/* 4..8 */
	int			loc);

/* g_epa.c */
BU_EXTERN(void rt_ell, (fastf_t *ov, CONST fastf_t *V, CONST fastf_t *A,
			CONST fastf_t *B, int sides) );

/* g_pipe.c */
void rt_vls_pipept(
	struct bu_vls *vp,
	int seg_no,
	const struct rt_db_internal *ip,
	double mm2local);
void rt_pipept_print();		/* needs wdb_pipept for arg */
int rt_pipe_ck( const struct bu_list *headp );

/* g_rpc.c */
BU_EXTERN(int rt_mk_parabola, (struct rt_pt_node *pts, fastf_t r, fastf_t b, fastf_t dtol, fastf_t ntol));
BU_EXTERN(struct rt_pt_node *rt_ptalloc, () );

/* memalloc.c -- non PARALLEL routines */
BU_EXTERN(unsigned long rt_memalloc, (struct mem_map **pp, unsigned size) );
BU_EXTERN(struct mem_map * rt_memalloc_nosplit, (struct mem_map **pp, unsigned size) );
BU_EXTERN(unsigned long rt_memget, (struct mem_map **pp, unsigned int size,
	unsigned int place) );
BU_EXTERN(void rt_memfree, (struct mem_map **pp, unsigned size, unsigned long addr) );
BU_EXTERN(void rt_mempurge, (struct mem_map **pp) );
BU_EXTERN(void rt_memprint, (struct mem_map **pp) );
BU_EXTERN(void rt_memclose,() );

BU_EXTERN(struct bn_vlblock *rt_vlblock_init, () );
BU_EXTERN(void rt_vlblock_free, (struct bn_vlblock *vbp) );
BU_EXTERN(struct bu_list *rt_vlblock_find, (struct bn_vlblock *vbp,
	int r, int g, int b) );

/* g_ars.c */
BU_EXTERN(void rt_hitsort, (struct hit h[], int nh));

/* g_pg.c */
int rt_pg_to_bot( struct rt_db_internal *ip, const struct bn_tol *tol, struct resource *resp );

/* g_hf.c */
int rt_hf_to_dsp(struct rt_db_internal *db_intern, struct resource *resp);

/* pr.c */
BU_EXTERN(void rt_pr_soltab, (CONST struct soltab *stp));
BU_EXTERN(void rt_pr_region, (CONST struct region *rp));
BU_EXTERN(void rt_pr_partitions, (CONST struct rt_i *rtip,
	CONST struct partition	*phead, CONST char *title));
BU_EXTERN(void rt_pr_pt_vls, (struct bu_vls *v,
	CONST struct rt_i *rtip, CONST struct partition *pp));
BU_EXTERN(void rt_pr_pt, (CONST struct rt_i *rtip, CONST struct partition *pp));
BU_EXTERN(void rt_pr_seg_vls, (struct bu_vls *, CONST struct seg *));
BU_EXTERN(void rt_pr_seg, (CONST struct seg *segp));
BU_EXTERN(void rt_pr_hit, (CONST char *str, CONST struct hit	*hitp));
BU_EXTERN(void rt_pr_hit_vls, (struct bu_vls *v, CONST char *str,
	CONST struct hit *hitp));
BU_EXTERN(void rt_pr_hitarray_vls, (struct bu_vls *v, CONST char *str,
	CONST struct hit *hitp, int count));
BU_EXTERN(void rt_pr_tree, (CONST union tree *tp, int lvl));
BU_EXTERN(void rt_pr_tree_vls, (struct bu_vls *vls, CONST union tree *tp));
BU_EXTERN(char *rt_pr_tree_str, (CONST union tree *tree));
BU_EXTERN(void rt_pr_tree_val, (CONST union tree *tp,
	CONST struct partition *partp, int pr_name, int lvl));
BU_EXTERN(void rt_pr_fallback_angle, (struct bu_vls *str, CONST char *prefix,
	CONST double angles[5]));
BU_EXTERN(void rt_find_fallback_angle, (double angles[5], CONST vect_t vec));
BU_EXTERN(void rt_pr_tol, (CONST struct bn_tol	*tol));

/* regionfix.c */
BU_EXTERN(void rt_regionfix, (struct rt_i *rtip));

/* table.c */
BU_EXTERN(int rt_id_solid, (struct bu_external *ep));
BU_EXTERN(CONST struct rt_functab *rt_get_functab_by_label, (CONST char *label));
int rt_generic_xform(
	struct rt_db_internal	*op,
	const mat_t		mat,
	struct rt_db_internal	*ip,
	int			free,
	struct db_i		*dbip,
	struct resource		*resp);


/* prep.c */
BU_EXTERN(void rt_plot_all_bboxes, (FILE *fp, struct rt_i *rtip));
void
rt_plot_all_solids(
	FILE		*fp,
	struct rt_i	*rtip,
	struct resource	*resp);
void rt_init_resource(
	struct resource *resp,
	int		cpu_num,
	struct rt_i	*rtip);
BU_EXTERN(void rt_clean_resource, (struct rt_i *rtip, struct resource *resp));

/* shoot.c */
BU_EXTERN(void rt_add_res_stats, (struct rt_i *rtip, struct resource *resp) );
					/* Tally stats into struct rt_i */
extern void rt_res_pieces_clean(struct resource *resp,
			   struct rt_i *rtip);
extern void rt_vstub(struct soltab	       *stp[],
		     struct xray		*rp[],
		     struct  seg            segp[],
		     int		  	    n,
		     struct application	*ap);


/* tree.c */
extern int rt_bound_tree( const union tree	*tp,
			  vect_t		tree_min,
			  vect_t		tree_max);
int rt_tree_elim_nops(union tree *, struct resource *resp);


/* vlist.c */
/* XXX Has some stuff mixed in here that should go in LIBBN */
struct bn_vlblock *
bn_vlblock_init(
	struct bu_list	*free_vlist_hd,		/* where to get/put free vlists */
	int		max_ent);
BU_EXTERN(struct bn_vlblock *	rt_vlblock_init, () );
BU_EXTERN(void			rt_vlblock_free, (struct bn_vlblock *vbp) );
BU_EXTERN(struct bu_list *	rt_vlblock_find, (struct bn_vlblock *vbp,
				int r, int g, int b) );
int rt_ck_vlist( const struct bu_list *vhead );
void rt_vlist_copy( struct bu_list *dest, const struct bu_list *src );
void bn_vlist_cleanup( struct bu_list *hd );
BU_EXTERN(void			rt_vlist_cleanup, () );
void bn_vlist_rpp( struct bu_list *hd, const point_t minn, CONST point_t maxx );
BU_EXTERN(void			rt_vlist_export, (struct bu_vls *vls,
				struct bu_list *hp,
				CONST char *name));
BU_EXTERN(void			rt_vlist_import, (struct bu_list *hp,
				struct bu_vls *namevls,
				CONST unsigned char *buf));
BU_EXTERN(void			rt_plot_vlblock, (FILE *fp,
				CONST struct bn_vlblock	*vbp) );
BU_EXTERN(void			rt_vlist_to_uplot, (FILE *fp,
				CONST struct bu_list *vhead));
BU_EXTERN(int			rt_process_uplot_value,
				(struct bu_list **vhead, struct bn_vlblock *vbp,
				FILE *fp, int c, double char_size) );
BU_EXTERN(int			rt_uplot_to_vlist, (struct bn_vlblock *vbp,
				FILE *fp, double char_size) );
BU_EXTERN(void			rt_label_vlist_verts, (struct bn_vlblock *vbp,
				struct bu_list *src, mat_t mat,
				double sz, double mm2local) );

/* pmalloc.c */
BU_EXTERN(char *rt_pmalloc, (long nbytes, struct rt_pm_res *pmem));
BU_EXTERN(void rt_pfree, (char *mem, struct rt_pm_res *pmem));
BU_EXTERN(char *rt_prealloc, (char *mem, unsigned nbytes, struct rt_pm_res *pmem));

#ifdef SEEN_RTGEOM_H
/* g_sketch.c */
extern void rt_sketch_ifree( struct rt_db_internal	*ip );
extern int curve_to_vlist(struct bu_list		*vhead,
			  const struct rt_tess_tol	*ttol,
			  point_t			V,
			  vect_t			u_vec,
			  vect_t			v_vec,
			  struct rt_sketch_internal *sketch_ip,
			  struct curve			*crv);

extern int rt_check_curve( struct curve *crv,
			   struct rt_sketch_internal *skt,
			   int noisey);

extern void rt_copy_curve(struct curve *crv_out, const struct curve *crv_in);

BU_EXTERN(int				rt_check_curve,
					(struct curve *crv,
					struct rt_sketch_internal *skt,
					int noisey));
BU_EXTERN(void				rt_curve_free, (struct curve *crv));
BU_EXTERN(void				rt_copy_curve,
					(struct curve *crv_out,
					CONST struct curve *crv_in));
BU_EXTERN(struct rt_sketch_internal	*rt_copy_sketch,
					(CONST struct rt_sketch_internal *sketch_ip));
BU_EXTERN(int				curve_to_tcl_list,
					(struct bu_vls *vls, struct curve *crv));
#endif

/* htbl.c */
BU_EXTERN(void			rt_htbl_init, (struct rt_htbl *b, int len, CONST char *str));
BU_EXTERN(void			rt_htbl_reset, (struct rt_htbl *b));
BU_EXTERN(void			rt_htbl_free, (struct rt_htbl *b));
BU_EXTERN(struct hit *		rt_htbl_get, (struct rt_htbl *b));

/************************************************************************
 *									*
 *			N M G Support Function Declarations		*
 *									*
 ************************************************************************/
#if defined(NMG_H)

/* From file nmg_mk.c */
/*	MAKE routines */
BU_EXTERN(struct model		*nmg_mm, () );
BU_EXTERN(struct model		*nmg_mmr, () );
BU_EXTERN(struct nmgregion	*nmg_mrsv, (struct model *m) );
BU_EXTERN(struct shell 		*nmg_msv, (struct nmgregion *r_p) );
BU_EXTERN(struct faceuse	*nmg_mf, (struct loopuse *lu1) );
BU_EXTERN(struct loopuse	*nmg_mlv, (long *magic, struct vertex *v, int orientation) );
BU_EXTERN(struct edgeuse	*nmg_me, (struct vertex *v1, struct vertex *v2, struct shell *s) );
BU_EXTERN(struct edgeuse	*nmg_meonvu, (struct vertexuse *vu) );
BU_EXTERN(struct loopuse	*nmg_ml, (struct shell *s) );
/*	KILL routines */
extern int nmg_keg( struct edgeuse	*eu);
BU_EXTERN(int			nmg_kvu, (struct vertexuse *vu) );
BU_EXTERN(int			nmg_kfu, (struct faceuse *fu1) );
BU_EXTERN(int			nmg_klu, (struct loopuse *lu1) );
BU_EXTERN(int			nmg_keu, (struct edgeuse *eu) );
BU_EXTERN(int			nmg_ks, (struct shell *s) );
BU_EXTERN(int			nmg_kr, (struct nmgregion *r) );
BU_EXTERN(void			nmg_km, (struct model *m) );
/*	Geometry and Attribute routines */
BU_EXTERN(void			nmg_vertex_gv, (struct vertex *v, CONST point_t pt) );
BU_EXTERN(void			nmg_vertex_g, (struct vertex *v, fastf_t x, fastf_t y, fastf_t z) );
BU_EXTERN(void			nmg_vertexuse_nv, (struct vertexuse *vu, CONST vect_t norm));
BU_EXTERN(void			nmg_vertexuse_a_cnurb, (struct vertexuse *vu, CONST fastf_t *uvw));
BU_EXTERN(void			nmg_edge_g, (struct edgeuse *eu) );
BU_EXTERN(void			nmg_edge_g_cnurb, (struct edgeuse *eu,
				int order, int n_knots, fastf_t *kv,
				int n_pts, int pt_type, fastf_t *points));
BU_EXTERN(void			nmg_edge_g_cnurb_plinear, (struct edgeuse *eu));
BU_EXTERN(int			nmg_use_edge_g, (struct edgeuse *eu, long *eg) );
BU_EXTERN(void			nmg_loop_g, (struct loop *l, CONST struct bn_tol *tol) );
BU_EXTERN(void			nmg_face_g, (struct faceuse *fu, CONST plane_t p) );
BU_EXTERN(void			nmg_face_new_g, (struct faceuse *fu, CONST plane_t pl));
BU_EXTERN(void			nmg_face_g_snurb, (struct faceuse *fu,
				int u_order, int v_order,
				int n_u_knots, int n_v_knots,
				fastf_t *ukv, fastf_t *vkv,
				int n_rows, int n_cols,
				int pt_type, fastf_t *mesh));
BU_EXTERN(void			nmg_face_bb, (struct face *f, CONST struct bn_tol *tol) );
BU_EXTERN(void			nmg_shell_a, (struct shell *s, CONST struct bn_tol *tol) );
BU_EXTERN(void			nmg_region_a, (struct nmgregion *r, CONST struct bn_tol *tol) );
/*	DEMOTE routines */
BU_EXTERN(int			nmg_demote_lu, (struct loopuse *lu) );
BU_EXTERN(int			nmg_demote_eu, (struct edgeuse *eu) );
/*	MODIFY routines */
BU_EXTERN(void			nmg_movevu, (struct vertexuse *vu, struct vertex *v) );
#define nmg_moveeu(a,b)		nmg_je(a,b)
BU_EXTERN(void			nmg_je, (struct edgeuse *eudst, struct edgeuse *eusrc) );
BU_EXTERN(void			nmg_unglueedge, (struct edgeuse *eu) );
BU_EXTERN(void			nmg_jv, (struct vertex *v1, struct vertex *v2) );
BU_EXTERN(void			nmg_jfg, (struct face *f1, struct face *f2));
BU_EXTERN(void			nmg_jeg, (struct edge_g_lseg *dest_eg,
				struct edge_g_lseg *src_eg) );

/* From nmg_mod.c */
/*	REGION Routines */
BU_EXTERN(void			nmg_merge_regions,
				(struct nmgregion *r1,
				struct nmgregion *r2,
				CONST struct bn_tol *tol));

/*	SHELL Routines */
BU_EXTERN(void			nmg_shell_coplanar_face_merge,
				(struct shell		*s,
				CONST struct bn_tol	*tol,
				CONST int		simplify));
BU_EXTERN(int			nmg_simplify_shell, (struct shell *s) );
BU_EXTERN(void			nmg_rm_redundancies, (struct shell *s, CONST struct bn_tol *tol ) );
BU_EXTERN(void			nmg_sanitize_s_lv, (struct shell *s,
				int orient) );
BU_EXTERN(void			nmg_s_split_touchingloops, (struct shell *s,
				CONST struct bn_tol *tol) );
BU_EXTERN(void			nmg_s_join_touchingloops,
				(struct shell		*s,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_js,
				(struct shell	*s1, struct shell	*s2,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_invert_shell,
				(struct shell		*s,
				CONST struct bn_tol	*tol));

/*	FACE Routines */
BU_EXTERN(struct faceuse	*nmg_cmface, (struct shell *s, struct vertex **vt[], int n) );
BU_EXTERN(struct faceuse	*nmg_cface, (struct shell *s, struct vertex **vt,	int n) );
BU_EXTERN(struct faceuse	*nmg_add_loop_to_face, (struct shell *s, struct faceuse *fu, struct vertex **verts, int n, int dir) );
BU_EXTERN(int			nmg_fu_planeeqn, (struct faceuse *fu, CONST struct bn_tol *tol) );
BU_EXTERN(void			nmg_gluefaces, (struct faceuse *fulist[], int n, CONST struct bn_tol *tol) );
BU_EXTERN(int			nmg_simplify_face, (struct faceuse *fu) );
BU_EXTERN(void			nmg_reverse_face, (struct faceuse *fu) );
BU_EXTERN(void			nmg_mv_fu_between_shells, (struct shell *dest,
				struct shell *src, struct faceuse *fu) );
BU_EXTERN(void			nmg_jf, (struct faceuse *dest_fu,
				struct faceuse *src_fu) );
BU_EXTERN(struct faceuse	*nmg_dup_face, (struct faceuse *fu, struct shell *s) );
/*	LOOP Routines */
BU_EXTERN(void			nmg_jl, (struct loopuse *lu, struct edgeuse *eu) );
BU_EXTERN(struct vertexuse	*nmg_join_2loops, (struct vertexuse *vu1, struct vertexuse *vu2) );
BU_EXTERN(struct vertexuse	*nmg_join_singvu_loop, (struct vertexuse *vu1, struct vertexuse *vu2) );
BU_EXTERN(struct vertexuse	*nmg_join_2singvu_loops, (struct vertexuse *vu1, struct vertexuse *vu2) );
BU_EXTERN(struct loopuse	*nmg_cut_loop, (struct vertexuse *vu1, struct vertexuse *vu2) );
BU_EXTERN(struct loopuse	*nmg_split_lu_at_vu, (struct loopuse *lu, struct vertexuse *vu) );
BU_EXTERN(struct vertexuse	*nmg_find_repeated_v_in_lu, (struct vertexuse	*vu));
BU_EXTERN(void			nmg_split_touchingloops, (struct loopuse *lu,
				CONST struct bn_tol *tol) );
BU_EXTERN(int			nmg_join_touchingloops, (struct loopuse *lu) );
BU_EXTERN(int			nmg_get_touching_jaunts,
				(CONST struct loopuse *lu,
				struct bu_ptbl *tbl,
				int *need_init));
BU_EXTERN(void			nmg_kill_accordions, (struct loopuse *lu));
BU_EXTERN(int			nmg_loop_split_at_touching_jaunt,
				(struct loopuse		*lu,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_simplify_loop, (struct loopuse *lu) );
BU_EXTERN(int			nmg_kill_snakes, (struct loopuse *lu) );
BU_EXTERN(void			nmg_mv_lu_between_shells, (struct shell *dest,
				struct shell *src, struct loopuse *lu) );
BU_EXTERN(void	 		nmg_moveltof, (struct faceuse *fu, struct shell *s) );
BU_EXTERN(struct loopuse	*nmg_dup_loop, (struct loopuse *lu,
				long *parent, long **trans_tbl) );
BU_EXTERN(void			nmg_set_lu_orientation, (struct loopuse *lu, int is_opposite) );
BU_EXTERN(void			nmg_lu_reorient, (struct loopuse *lu ) );
/*	EDGE Routines */
BU_EXTERN(struct edgeuse	*nmg_eusplit, (struct vertex *v, struct edgeuse *oldeu, int share_geom) );
BU_EXTERN(struct edgeuse	*nmg_esplit, (struct vertex *v, struct edgeuse *eu, int share_geom) );
BU_EXTERN(struct edgeuse	*nmg_ebreak, (struct vertex *v, struct edgeuse *eu));
BU_EXTERN(struct edgeuse	*nmg_ebreaker, (struct vertex *v,
				struct edgeuse *eu, CONST struct bn_tol *tol));
BU_EXTERN(struct vertex		*nmg_e2break, (struct edgeuse *eu1, struct edgeuse *eu2) );
BU_EXTERN(int			nmg_unbreak_edge, (struct edgeuse *eu1_first));
BU_EXTERN(int			nmg_unbreak_shell_edge_unsafe,
				(struct edgeuse	*eu1_first));
BU_EXTERN(struct edgeuse	*nmg_eins, (struct edgeuse *eu) );
BU_EXTERN(void			nmg_mv_eu_between_shells, (struct shell *dest,
				struct shell *src, struct edgeuse *eu) );
/*	VERTEX Routines */
BU_EXTERN(void			nmg_mv_vu_between_shells, (struct shell *dest,
				struct shell *src, struct vertexuse *vu) );

/* From nmg_info.c */
	/* Model routines */
BU_EXTERN(struct model		*nmg_find_model, (CONST long *magic_p) );
BU_EXTERN(void			nmg_model_bb, (point_t min_pt, point_t max_pt, CONST struct model *m) );


	/* Shell routines */
BU_EXTERN(int			nmg_shell_is_empty, (CONST struct shell *s) );
BU_EXTERN(struct shell		*nmg_find_s_of_lu, (CONST struct loopuse *lu) );
BU_EXTERN(struct shell		*nmg_find_s_of_eu, (CONST struct edgeuse *eu) );
BU_EXTERN(struct shell		*nmg_find_s_of_vu, (CONST struct vertexuse *vu) );

	/* Face routines */
BU_EXTERN(struct faceuse	*nmg_find_fu_of_eu, (CONST struct edgeuse *eu));
BU_EXTERN(struct faceuse	*nmg_find_fu_of_lu, (CONST struct loopuse *lu));
BU_EXTERN(struct faceuse	*nmg_find_fu_of_vu, (CONST struct vertexuse *vu) );
BU_EXTERN(struct faceuse	*nmg_find_fu_with_fg_in_s, (CONST struct shell *s1,
				CONST struct faceuse *fu2));
BU_EXTERN(double		nmg_measure_fu_angle, (CONST struct edgeuse *eu,
				CONST vect_t xvec, CONST vect_t yvec,
				CONST vect_t zvec) );

	/* Loop routines */
BU_EXTERN(struct loopuse	*nmg_find_lu_of_vu, (CONST struct vertexuse *vu) );
BU_EXTERN(int			nmg_loop_is_a_crack, (CONST struct loopuse *lu) );
BU_EXTERN(int			nmg_loop_is_ccw, (CONST struct loopuse *lu,
				CONST plane_t norm, CONST struct bn_tol *tol) );
BU_EXTERN(CONST struct vertexuse *nmg_loop_touches_self, (CONST struct loopuse *lu));
BU_EXTERN(int			nmg_2lu_identical, (CONST struct edgeuse *eu1,
				CONST struct edgeuse *eu2));

	/* Edge routines */
BU_EXTERN(struct edgeuse	*nmg_find_matching_eu_in_s, (CONST struct edgeuse	*eu1,
				CONST struct shell	*s2));
BU_EXTERN(struct edgeuse	*nmg_findeu, (CONST struct vertex *v1, CONST struct vertex *v2,
				CONST struct shell *s, CONST struct edgeuse *eup,
				int dangling_only) );
BU_EXTERN(struct edgeuse	*nmg_find_eu_in_face, (CONST struct vertex *v1,
				CONST struct vertex *v2, CONST struct faceuse *fu,
				CONST struct edgeuse *eup, int dangling_only));
BU_EXTERN(struct edgeuse	*nmg_find_e, (CONST struct vertex *v1,
				CONST struct vertex *v2,
				CONST struct shell *s,
				CONST struct edge *ep));
BU_EXTERN(struct edgeuse	*nmg_find_eu_of_vu, (CONST struct vertexuse *vu) );
BU_EXTERN(struct edgeuse	*nmg_find_eu_with_vu_in_lu, (CONST struct loopuse *lu,
				CONST struct vertexuse *vu) );
BU_EXTERN(CONST struct edgeuse	*nmg_faceradial, (CONST struct edgeuse *eu) );
BU_EXTERN(CONST struct edgeuse	*nmg_radial_face_edge_in_shell, (CONST struct edgeuse *eu) );
BU_EXTERN(CONST struct edgeuse *nmg_find_edge_between_2fu, (CONST struct faceuse *fu1,
				CONST struct faceuse *fu2, CONST struct bn_tol *tol));
BU_EXTERN(struct edge		*nmg_find_e_nearest_pt2, (long *magic_p,
				CONST point_t pt2, CONST mat_t mat,
				CONST struct bn_tol *tol) );
BU_EXTERN(struct edgeuse	*nmg_find_matching_eu_in_s, (
				CONST struct edgeuse *eu1, CONST struct shell *s2));
BU_EXTERN(void			nmg_eu_2vecs_perp, (vect_t xvec, vect_t yvec,
				vect_t zvec, CONST struct edgeuse *eu,
				CONST struct bn_tol *tol) );
BU_EXTERN(int			nmg_find_eu_leftvec, (vect_t left,
				CONST struct edgeuse *eu) );
BU_EXTERN(int			nmg_find_eu_left_non_unit, (vect_t left,
				CONST struct edgeuse	*eu));
BU_EXTERN(struct edgeuse	*nmg_find_ot_same_eu_of_e,
				(CONST struct edge *e));

	/* Vertex routines */
BU_EXTERN(struct vertexuse	*nmg_find_v_in_face, (CONST struct vertex *,
				CONST struct faceuse *) );
BU_EXTERN(struct vertexuse	*nmg_find_v_in_shell, (CONST struct vertex *v,
				CONST struct shell *s, int edges_only));
BU_EXTERN(struct vertexuse	*nmg_find_pt_in_lu, (CONST struct loopuse *lu,
				CONST point_t pt, CONST struct bn_tol *tol));
BU_EXTERN(struct vertexuse	*nmg_find_pt_in_face, (CONST struct faceuse *fu,
				CONST point_t pt,
				CONST struct bn_tol *tol) );
BU_EXTERN(struct vertex		*nmg_find_pt_in_shell, (CONST struct shell *s,
				CONST point_t pt, CONST struct bn_tol *tol) );
BU_EXTERN(struct vertex		*nmg_find_pt_in_model, (CONST struct model *m,
				CONST point_t pt, CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_is_vertex_in_edgelist, (CONST struct vertex *v,
				CONST struct bu_list *hd) );
BU_EXTERN(int			nmg_is_vertex_in_looplist, (CONST struct vertex *v,
				CONST struct bu_list *hd, int singletons) );
BU_EXTERN(struct vertexuse 	*nmg_is_vertex_in_face, (CONST struct vertex *v,
				CONST struct face *f));
BU_EXTERN(int			nmg_is_vertex_a_selfloop_in_shell, (CONST struct vertex *v,
				CONST struct shell *s) );
BU_EXTERN(int			nmg_is_vertex_in_facelist, (CONST struct vertex *v,
				CONST struct bu_list *hd) );
BU_EXTERN(int			nmg_is_edge_in_edgelist, (CONST struct edge *e,
				CONST struct bu_list *hd) );
BU_EXTERN(int			nmg_is_edge_in_looplist, (CONST struct edge *e,
				CONST struct bu_list *hd) );
BU_EXTERN(int			nmg_is_edge_in_facelist, (CONST struct edge *e,
				CONST struct bu_list *hd) );
BU_EXTERN(int			nmg_is_loop_in_facelist, (CONST struct loop *l,
				CONST struct bu_list *fu_hd) );

	/* Tabulation routines */
BU_EXTERN(void			nmg_vertex_tabulate, (struct bu_ptbl *tab,
				CONST long *magic_p));
BU_EXTERN(void			nmg_vertexuse_normal_tabulate, (struct bu_ptbl *tab,
				CONST long		*magic_p));
BU_EXTERN(void			nmg_edgeuse_tabulate, (struct bu_ptbl *tab,
				CONST long *magic_p));
BU_EXTERN(void			nmg_edge_tabulate, (struct bu_ptbl *tab,
				CONST long *magic_p));
BU_EXTERN(void			nmg_edge_g_tabulate, (struct bu_ptbl *tab,
				CONST long		*magic_p));
BU_EXTERN(void			nmg_face_tabulate, (struct bu_ptbl *tab,
				CONST long *magic_p));
BU_EXTERN(void			nmg_edgeuse_with_eg_tabulate, (struct bu_ptbl *tab,
				CONST struct edge_g_lseg *eg));
BU_EXTERN(void			nmg_edgeuse_on_line_tabulate,
				(struct bu_ptbl		*tab,
				CONST long		*magic_p,
				CONST point_t		pt,
				CONST vect_t		dir,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_e_and_v_tabulate,
				(struct bu_ptbl		*eutab,
				struct bu_ptbl		*vtab,
				CONST long		*magic_p));
BU_EXTERN(int			nmg_2edgeuse_g_coincident,
				(CONST struct edgeuse	*eu1,
				CONST struct edgeuse	*eu2,
				CONST struct bn_tol	*tol));

/* From nmg_extrude.c */
BU_EXTERN(void			nmg_translate_face, (struct faceuse *fu,
				CONST vect_t		Vec,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_extrude_face, (struct faceuse *fu,
				CONST vect_t Vec,
				CONST struct bn_tol	*tol));
BU_EXTERN(struct vertexuse	*nmg_find_vertex_in_lu, (CONST struct vertex *v,
				CONST struct loopuse *lu));
BU_EXTERN(void			nmg_fix_overlapping_loops, (struct shell *s,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_break_crossed_loops, (struct shell *is,
				CONST struct bn_tol *tol));
BU_EXTERN(struct shell		*nmg_extrude_cleanup, (struct shell *is,
				CONST int is_void,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_hollow_shell, (struct shell *s,
				CONST fastf_t thick,
				CONST int approximate,
				CONST struct bn_tol *tol));
BU_EXTERN(struct shell		*nmg_extrude_shell,
				(struct shell *s,
				CONST fastf_t dist,
				CONST int normal_ward,
				CONST int approximate,
				CONST struct bn_tol *tol));

/* From nmg_pr.c */
BU_EXTERN(char *		nmg_orientation, (int orientation) );
BU_EXTERN(void			nmg_pr_orient, (int orientation, CONST char *h) );
BU_EXTERN(void			nmg_pr_m, (CONST struct model *m) );
BU_EXTERN(void			nmg_pr_r, (CONST struct nmgregion *r, char *h) );
BU_EXTERN(void			nmg_pr_sa, (CONST struct shell_a *sa, char *h) );
BU_EXTERN(void			nmg_pr_lg, (CONST struct loop_g *lg, char *h) );
BU_EXTERN(void			nmg_pr_fg, (CONST long *magic, char *h) );
BU_EXTERN(void			nmg_pr_s, (CONST struct shell *s, char *h) );
BU_EXTERN(void 			nmg_pr_s_briefly, (CONST struct shell *s, char *h));
BU_EXTERN(void			nmg_pr_f, (CONST struct face *f, char *h) );
BU_EXTERN(void			nmg_pr_fu, (CONST struct faceuse *fu, char *h) );
BU_EXTERN(void			nmg_pr_fu_briefly, (CONST struct faceuse *fu,	char *h) );
BU_EXTERN(void			nmg_pr_l, (CONST struct loop *l, char *h) );
BU_EXTERN(void			nmg_pr_lu, (CONST struct loopuse *lu, char *h) );
BU_EXTERN(void			nmg_pr_lu_briefly, (CONST struct loopuse *lu, char *h) );
BU_EXTERN(void			nmg_pr_eg, (CONST long *eg, char *h) );
BU_EXTERN(void			nmg_pr_e, (CONST struct edge *e, char *h) );
BU_EXTERN(void			nmg_pr_eu, (CONST struct edgeuse *eu, char *h) );
BU_EXTERN(void			nmg_pr_eu_briefly, (CONST struct edgeuse *eu, char *h) );
BU_EXTERN(void 			nmg_pr_eu_endpoints, (CONST struct edgeuse *eu,
				char *h));
BU_EXTERN(void			nmg_pr_vg, (CONST struct vertex_g *vg, char *h) );
BU_EXTERN(void			nmg_pr_v, (CONST struct vertex *v, char *h) );
BU_EXTERN(void			nmg_pr_vu, (CONST struct vertexuse *vu, char *h) );
BU_EXTERN(void			nmg_pr_vu_briefly, (CONST struct vertexuse *vu, char *h) );
BU_EXTERN(void			nmg_pr_vua, (CONST long *magic_p, char *h) );
BU_EXTERN(void			nmg_euprint, (CONST char *str, CONST struct edgeuse *eu) );
BU_EXTERN(void			nmg_pr_ptbl, (CONST char *title,
				CONST struct bu_ptbl *tbl,
				int verbose));
BU_EXTERN(void			nmg_pr_ptbl_vert_list, (CONST char *str,
				CONST struct bu_ptbl *tbl,
				CONST fastf_t *mag));
BU_EXTERN(void			nmg_pr_one_eu_vecs,
				(CONST struct edgeuse	*eu,
				CONST vect_t		xvec,
				CONST vect_t		yvec,
				CONST vect_t		zvec,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_pr_fu_around_eu_vecs,
				(CONST struct edgeuse	*eu,
				CONST vect_t		xvec,
				CONST vect_t		yvec,
				CONST vect_t		zvec,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_pr_fu_around_eu, (CONST struct edgeuse *eu,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_pl_lu_around_eu, (CONST struct edgeuse *eu));
BU_EXTERN(void			nmg_pr_fus_in_fg, (CONST long *fg_magic));

/* From nmg_misc.c */
extern int rt_dist_pt3_line3(fastf_t		*dist,
			     point_t		pca,
			     const point_t	a,
			     const point_t	p,
			     const vect_t	dir,
			     const struct bn_tol *tol);

extern int rt_dist_line3_line3(fastf_t dist[2],
		    CONST point_t p1,
		    CONST point_t p2,
		    CONST vect_t d1,
		    CONST vect_t d2,
			       CONST struct bn_tol *tol);

BU_EXTERN(int			nmg_snurb_calc_lu_uv_orient, (CONST struct loopuse *lu));
BU_EXTERN(void			nmg_snurb_fu_eval, (CONST struct faceuse *fu,
				CONST fastf_t u,
				CONST fastf_t v,
				point_t pt_on_srf));
BU_EXTERN(void			nmg_snurb_fu_get_norm,
				(CONST struct faceuse *fu,
				CONST fastf_t u,
				CONST fastf_t v,
				vect_t norm));
BU_EXTERN(void			nmg_snurb_fu_get_norm_at_vu,
				(CONST struct faceuse *fu,
				CONST struct vertexuse *vu,
				vect_t norm));
BU_EXTERN(void			nmg_find_zero_length_edges, (CONST struct model *m));
BU_EXTERN(struct face		*nmg_find_top_face_in_dir,
				(CONST struct shell *s,
				int dir, long *flags));
BU_EXTERN(struct face		*nmg_find_top_face,
				(CONST struct shell *s,
				int *dir, long *flags));
BU_EXTERN(int			nmg_find_outer_and_void_shells,
				(struct nmgregion *r,
				struct bu_ptbl ***shells,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_mark_edges_real, (CONST long *magic_p));
BU_EXTERN(void			nmg_tabulate_face_g_verts,
				(struct bu_ptbl *tab, CONST struct face_g_plane *fg));
BU_EXTERN(void			nmg_isect_shell_self,
				(struct shell *s, CONST struct bn_tol *tol));
BU_EXTERN(struct edgeuse	*nmg_next_radial_eu, (CONST struct edgeuse *eu,
				CONST struct shell *s, CONST int wires));
BU_EXTERN(struct edgeuse	*nmg_prev_radial_eu, (CONST struct edgeuse *eu,
				CONST struct shell *s, CONST int wires));
BU_EXTERN(int			nmg_radial_face_count,
				(CONST struct edgeuse *eu, CONST struct shell *s));
BU_EXTERN(int			nmg_check_closed_shell, (CONST struct shell *s,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_move_lu_between_fus, (struct faceuse *dest,
				struct faceuse *src, struct loopuse *lu));
BU_EXTERN(void			nmg_loop_plane_newell, (CONST struct loopuse *lu,
				plane_t pl));
BU_EXTERN(fastf_t		nmg_loop_plane_area, (CONST struct loopuse *lu,
				plane_t pl));
BU_EXTERN(int			nmg_calc_face_plane, (struct faceuse *fu_in,
				plane_t pl));
BU_EXTERN(int			nmg_calc_face_g, (struct faceuse *fu));
BU_EXTERN(fastf_t		nmg_faceuse_area, (CONST struct faceuse *fu));
BU_EXTERN(fastf_t		nmg_shell_area, (CONST struct shell *s));
BU_EXTERN(fastf_t		nmg_region_area, (CONST struct nmgregion *r));
BU_EXTERN(fastf_t		nmg_model_area, (CONST struct model *m));
/* Some stray rt_ plane functions here */
BU_EXTERN(void			nmg_purge_unwanted_intersection_points, (struct bu_ptbl *vert_list, fastf_t *mag, CONST struct faceuse *fu, CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_in_or_ref, (struct vertexuse *vu, struct bu_ptbl *b) );
BU_EXTERN(void			nmg_rebound, (struct model *m, CONST struct bn_tol *tol) );
BU_EXTERN(void			nmg_count_shell_kids, (CONST struct model *m, unsigned long *total_wires, unsigned long *total_faces, unsigned long *total_points));
BU_EXTERN(void			nmg_close_shell, (struct shell *s, CONST struct bn_tol *tol));
BU_EXTERN(struct shell		*nmg_dup_shell , ( struct shell *s , long ***copy_tbl, CONST struct bn_tol *tol ) );
BU_EXTERN(struct edgeuse	*nmg_pop_eu, (struct bu_ptbl *stack));
BU_EXTERN(void			nmg_reverse_radials, (struct faceuse *fu, CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_reverse_face_and_radials, (struct faceuse *fu, CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_shell_is_void, (CONST struct shell *s));
BU_EXTERN(void			nmg_propagate_normals, (struct faceuse *fu_in,
				long *flags, CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_connect_same_fu_orients, (struct shell *s));
BU_EXTERN(void			nmg_fix_decomposed_shell_normals, (struct shell *s, CONST struct bn_tol *tol));
BU_EXTERN(struct model		*nmg_mk_model_from_region, (struct nmgregion *r, int reindex));
BU_EXTERN(void			nmg_fix_normals, (struct shell *s_orig,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_break_long_edges, (struct shell *s,
				CONST struct bn_tol *tol));
BU_EXTERN(struct faceuse	*nmg_mk_new_face_from_loop, (struct loopuse *lu));
BU_EXTERN(int			nmg_split_loops_into_faces, (long *magic_p, CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_decompose_shell, (struct shell *s, CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_stash_model_to_file, (CONST char *filename,
				CONST struct model *m, CONST char *title) );
BU_EXTERN(int			nmg_unbreak_region_edges, (long *magic_p));
/* rt_dist_pt3_line3 */
BU_EXTERN(int			nmg_mv_shell_to_region, (struct shell *s, struct nmgregion *r));
BU_EXTERN(int			nmg_find_isect_faces, (CONST struct vertex *new_v,
				struct bu_ptbl *faces,
				int *free_edges,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_simple_vertex_solve,
				(struct vertex *new_v, CONST struct bu_ptbl *faces,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_ck_vert_on_fus, (CONST struct vertex *v,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_make_faces_at_vert, (struct vertex *new_v,
				struct bu_ptbl *int_faces,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_kill_cracks_at_vertex, (CONST struct vertex *vp));
BU_EXTERN(int			nmg_complex_vertex_solve,
				(struct vertex *new_v,
				CONST struct bu_ptbl *faces,
				CONST int free_edges,
				CONST int approximate,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_bad_face_normals, (CONST struct shell *s,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_faces_are_radial,
				(CONST struct faceuse *fu1,
				CONST struct faceuse *fu2));
BU_EXTERN(int			nmg_move_edge_thru_pt, (struct edgeuse *mv_eu,
				CONST point_t pt, CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_vlist_to_wire_edges, (struct shell *s,
				CONST struct bu_list *vhead));
BU_EXTERN(void			nmg_follow_free_edges_to_vertex,
				(CONST struct vertex *vpa,
				CONST struct vertex *vpb,
				struct bu_ptbl *bad_verts,
				CONST struct shell *s,
				CONST struct edgeuse *eu,
				struct bu_ptbl *verts,
				int *found));
BU_EXTERN(void			nmg_glue_face_in_shell,
				(CONST struct faceuse *fu,
				struct shell *s,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_open_shells_connect,
				(struct shell *dst,
				struct shell *src,
				CONST long **copy_tbl,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_in_vert,
				(struct vertex *new_v,
				CONST int approximate,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_mirror_model, (struct model *m));
BU_EXTERN(int			nmg_kill_cracks, (struct shell *s));
BU_EXTERN(int			nmg_kill_zero_length_edgeuses, (struct model *m));
BU_EXTERN(void			nmg_make_faces_within_tol, (struct shell *s,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_intersect_loops_self,
				(struct shell *s, CONST struct bn_tol *tol));
BU_EXTERN(struct edge_g_cnurb *rt_join_cnurbs, (struct bu_list *crv_head));
BU_EXTERN(struct edge_g_cnurb *rt_arc2d_to_cnurb,
				(point_t i_center,
				point_t i_start,
				point_t i_end,
				int point_type,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_break_edge_at_verts, (struct edge *e,
				struct bu_ptbl *verts, CONST struct bn_tol *tol));
BU_EXTERN( void nmg_isect_shell_self , ( struct shell *s , CONST struct bn_tol *tol ) );
BU_EXTERN(fastf_t		nmg_loop_plane_area , (CONST struct loopuse *lu , plane_t pl ) );
BU_EXTERN(int			nmg_break_edges, (long *magic_p,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_lu_is_convex, (struct loopuse *lu,
				CONST struct bn_tol *tol));

#ifdef SEEN_RTGEOM_H
BU_EXTERN(int			nmg_to_arb, (CONST struct model *m,
				struct rt_arb_internal *arb_int));
BU_EXTERN(int			nmg_to_tgc, (CONST struct model *m,
				struct rt_tgc_internal *tgc_int,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_to_poly, (CONST struct model *m,
				struct rt_pg_internal *poly_int,
				CONST struct bn_tol *tol));
#endif

BU_EXTERN(int			nmg_simplify_shell_edges, (struct shell *s,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_edge_collapse, (struct model *m,
				CONST struct bn_tol *tol,
				CONST fastf_t tol_coll,
				CONST fastf_t min_angle));
struct rt_bot_internal		*nmg_bot( struct shell *s, const struct bn_tol *tol );

/* g_bot.c */
int rt_bot_find_v_nearest_pt2();	/* needs rt_bot_internal for arg list */
int rt_bot_edge_in_list( const int v1, const int v2, const int edge_list[], const int edge_count );
int rt_bot_find_e_nearest_pt2();	/* needs rt_bot_internal for arg list */
int rt_bot_vertex_fuse( struct rt_bot_internal *bot );
int rt_bot_same_orientation( const int *a, const int *b );
int rt_bot_face_fuse( struct rt_bot_internal *bot );
int rt_bot_condense( struct rt_bot_internal *bot );

/* From nmg_tri.c */
extern void nmg_triangulate_shell(struct shell *s, const struct bn_tol  *tol);


BU_EXTERN(void			nmg_triangulate_model, (struct model *m, CONST struct bn_tol   *tol) );
BU_EXTERN(void			nmg_triangulate_fu, (struct faceuse *fu, CONST struct bn_tol   *tol) );

/* nmg_manif.c */
BU_EXTERN(int			nmg_dangling_face, (CONST struct faceuse *fu,
				CONST char *manifolds));
/* static paint_face */
/* static set_edge_sub_manifold */
/* static set_loop_sub_manifold */
/* static set_face_sub_manifold */
BU_EXTERN(char 			*nmg_shell_manifolds, (struct shell *sp, char *tbl) );
BU_EXTERN(char	 		*nmg_manifolds, (struct model *m) );

/* g_nmg.c */
extern int nmg_ray_segs(struct ray_data	*rd);

/* g_torus.c */
extern int rt_num_circular_segments(double maxerr, double radius);

/* tcl.c */
int rt_tcl_parse_ray( Tcl_Interp *interp, struct xray *rp, CONST char *CONST*argv );
void rt_tcl_pr_cutter( Tcl_Interp *interp, CONST union cutter *cutp );
int rt_tcl_cutter( ClientData clientData, Tcl_Interp *interp, int argc, const char *const*argv );
void rt_tcl_pr_hit( Tcl_Interp *interp, struct hit *hitp, const struct seg *segp, const struct xray	*rayp, int flipflag );
void rt_tcl_setup(Tcl_Interp *interp);
int Rt_Init(Tcl_Interp *interp);
void db_full_path_appendresult( Tcl_Interp *interp, const struct db_full_path *pp );
extern int tcl_obj_to_int_array(Tcl_Interp *interp,
				Tcl_Obj *list,
				int **array,
				int *array_len);


extern int tcl_obj_to_fastf_array(Tcl_Interp *interp,
				  Tcl_Obj *list,
				  fastf_t **array,
				  int *array_len);

extern int tcl_list_to_int_array(Tcl_Interp *interp,
				 char *char_list,
				 int **array,
				 int *array_len);

extern int tcl_list_to_fastf_array(Tcl_Interp *interp,
				   char *char_list,
				   fastf_t **array,
				   int *array_len);


/* g_rhc.c */
extern int rt_mk_hyperbola(struct rt_pt_node *pts,
			   fastf_t r,
			   fastf_t b,
			   fastf_t c,
			   fastf_t dtol,
			   fastf_t ntol);



/* nmg_class.c */
extern int nmg_classify_pt_loop(const point_t pt,
				const struct loopuse *lu,
				const struct bn_tol *tol);

extern int nmg_classify_s_vs_s( struct shell *s,
				struct shell *s2,
				const struct bn_tol *tol);

extern int nmg_classify_lu_lu(const struct loopuse *lu1,
			      const struct loopuse *lu2,
			      const struct bn_tol *tol);

BU_EXTERN(int			nmg_class_pt_f, (CONST point_t pt,
				CONST struct faceuse *fu,
				CONST struct bn_tol *tol) );
BU_EXTERN(int			nmg_class_pt_s, (CONST point_t pt,
				CONST struct shell *s,
				CONST int in_or_out_only,
				CONST struct bn_tol *tol) );

/* From nmg_pt_fu.c */
extern int nmg_eu_is_part_of_crack( const struct edgeuse *eu);

extern int nmg_class_pt_lu_except(point_t		pt,
				  const struct loopuse	*lu,
				  const struct edge		*e_p,
				  const struct bn_tol	*tol);

BU_EXTERN(int			nmg_class_pt_fu_except, (CONST point_t pt,
				CONST struct faceuse *fu,
				CONST struct loopuse *ignore_lu,
				void (*eu_func)(), void (*vu_func)(),
				CONST char *priv,
				CONST int call_on_hits,
				CONST int in_or_out_only,
				CONST struct bn_tol *tol) );

/* From nmg_plot.c */
void
extern nmg_pl_shell(FILE		*fp,
		    const struct shell	*s,
		    int			fancy);

BU_EXTERN(void			nmg_vu_to_vlist, (struct bu_list *vhead,
				CONST struct vertexuse	*vu));
BU_EXTERN(void			nmg_eu_to_vlist, (struct bu_list *vhead,
				CONST struct bu_list	*eu_hd));
BU_EXTERN(void			nmg_lu_to_vlist, (struct bu_list *vhead,
				CONST struct loopuse	*lu,
				int			poly_markers,
				CONST vectp_t		normal));
BU_EXTERN(void			nmg_snurb_fu_to_vlist,
				(struct bu_list		*vhead,
				CONST struct faceuse	*fu,
				int			poly_markers));
BU_EXTERN(void			nmg_s_to_vlist,
				(struct bu_list		*vhead,
				CONST struct shell	*s,
				int			poly_markers));
BU_EXTERN(void			nmg_r_to_vlist,
				(struct bu_list		*vhead,
				CONST struct nmgregion	*r,
				int			poly_markers));
BU_EXTERN(void			nmg_m_to_vlist,
				(struct bu_list	*vhead,
				struct model	*m,
				int		poly_markers));
BU_EXTERN(void			nmg_offset_eu_vert,
				(point_t			base,
				CONST struct edgeuse	*eu,
				CONST vect_t		face_normal,
				int			tip));
	/* plot */
BU_EXTERN(void			nmg_pl_v, (FILE	*fp, CONST struct vertex *v,
				long *b) );
BU_EXTERN(void			nmg_pl_e, (FILE *fp, CONST struct edge *e,
				long *b, int red, int green, int blue) );
BU_EXTERN(void			nmg_pl_eu, (FILE *fp, CONST struct edgeuse *eu,
				long *b, int red, int green, int blue) );
BU_EXTERN(void			nmg_pl_lu, (FILE *fp, CONST struct loopuse *fu, 
				long *b, int red, int green, int blue) );
BU_EXTERN(void			nmg_pl_fu, (FILE *fp, CONST struct faceuse *fu,
				long *b, int red, int green, int blue ) );
BU_EXTERN(void			nmg_pl_s, (FILE *fp, CONST struct shell *s) );
BU_EXTERN(void			nmg_pl_r, (FILE *fp, CONST struct nmgregion *r) );
BU_EXTERN(void			nmg_pl_m, (FILE *fp, CONST struct model *m) );
BU_EXTERN(void			nmg_vlblock_v, (struct bn_vlblock *vbp,
				CONST struct vertex *v, long *tab) );
BU_EXTERN(void			nmg_vlblock_e, (struct bn_vlblock *vbp,
				CONST struct edge *e, long *tab,
				int red, int green, int blue, int fancy) );
BU_EXTERN(void			nmg_vlblock_eu, (struct bn_vlblock *vbp,
				CONST struct edgeuse *eu, long *tab,
				int red, int green, int blue,
				int fancy, int loopnum) );
BU_EXTERN(void			nmg_vlblock_euleft,
				(struct bu_list			*vh,
				CONST struct edgeuse		*eu,
				CONST point_t			center,
				CONST mat_t			mat,
				CONST vect_t			xvec,
				CONST vect_t			yvec,
				double				len,
				CONST struct bn_tol		*tol));
BU_EXTERN(void			nmg_vlblock_around_eu,
				(struct bn_vlblock		*vbp,
				CONST struct edgeuse		*arg_eu,
				long				*tab,
				int				fancy,
				CONST struct bn_tol		*tol));
BU_EXTERN(void			nmg_vlblock_lu, (struct bn_vlblock *vbp,
				CONST struct loopuse *lu, long *tab,
				int red, int green, int blue,
				int fancy, int loopnum) );
BU_EXTERN(void			nmg_vlblock_fu, (struct bn_vlblock *vbp,
				CONST struct faceuse *fu, long *tab, int fancy) );
BU_EXTERN(void			nmg_vlblock_s, (struct bn_vlblock *vbp,
				CONST struct shell *s, int fancy) );
BU_EXTERN(void			nmg_vlblock_r, (struct bn_vlblock *vbp,
				CONST struct nmgregion *r, int fancy) );
BU_EXTERN(void			nmg_vlblock_m, (struct bn_vlblock *vbp,
				CONST struct model *m, int fancy) );
	/* visualization helper routines */
BU_EXTERN(void			nmg_pl_edges_in_2_shells,
				(struct bn_vlblock	*vbp,
				long			*b,
				CONST struct edgeuse	*eu,
				int			fancy,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_pl_isect,
				(CONST char		*filename,
				CONST struct shell	*s,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_pl_comb_fu, (int num1, int num2,
				CONST struct faceuse *fu1) );
BU_EXTERN(void			nmg_pl_2fu, (CONST char *str, int num,
				CONST struct faceuse *fu1, CONST struct faceuse *fu2,
				int show_mates) );
	/* graphical display of classifier results */
BU_EXTERN(void			nmg_show_broken_classifier_stuff,
				(long	*p,
				long	*classlist[4],
				int	all_new,
				int	fancy,
				CONST char	*a_string));
BU_EXTERN(void			nmg_face_plot, (CONST struct faceuse *fu) );
BU_EXTERN(void			nmg_2face_plot, (CONST struct faceuse *fu1,
				CONST struct faceuse *fu2) );
BU_EXTERN(void			nmg_face_lu_plot, (CONST struct loopuse *lu,
				CONST struct vertexuse *vu1, CONST struct vertexuse *vu2) );
BU_EXTERN(void			nmg_plot_lu_ray,
				(CONST struct loopuse		*lu,
				CONST struct vertexuse		*vu1,
				CONST struct vertexuse		*vu2,
				CONST vect_t			left));
BU_EXTERN(void			nmg_plot_ray_face,
				(CONST char *fname,
				point_t pt,
				CONST vect_t dir,
				CONST struct faceuse *fu));
BU_EXTERN(void			nmg_plot_lu_around_eu,
				(CONST char		*prefix,
				CONST struct edgeuse	*eu,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_snurb_to_vlist,
				(struct bu_list			*vhead,
				CONST struct face_g_snurb	*fg,
				int				n_interior));
BU_EXTERN(void			nmg_cnurb_to_vlist, (struct bu_list *vhead,
				CONST struct edgeuse *eu,int n_interior,
				int cmd) );

/* nurb_util.c */
extern void
rt_nurb_clean_cnurb(struct edge_g_cnurb * crv);

/* nurb_knot.c */
extern int rt_nurb_knot_index(const struct knot_vector *kv,
			     fastf_t k_value,
			     int	order);

/* nurb_trim.c */
extern int nmg_uv_in_lu(const fastf_t u, const fastf_t v, 
			const struct loopuse *lu);


/* from nmg_mesh.c */
extern int nmg_mesh_two_faces(struct faceuse *fu1,
			      struct faceuse *fu2,
			      const struct bn_tol	*tol);

BU_EXTERN(void			nmg_radial_join_eu, (struct edgeuse *eu1,
				struct edgeuse *eu2, CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_mesh_faces, (struct faceuse *fu1,
				struct faceuse *fu2, CONST struct bn_tol *tol) );
BU_EXTERN(int			nmg_mesh_face_shell, (struct faceuse *fu1,
				struct shell *s, CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_mesh_shell_shell, (struct shell *s1,
				struct shell *s2, CONST struct bn_tol *tol));
BU_EXTERN(double		nmg_measure_fu_angle, (CONST struct edgeuse *eu,
				CONST vect_t xvec, CONST vect_t yvec,
				CONST vect_t zvec));

/* from nmg_bool.c */
BU_EXTERN(struct nmgregion	*nmg_do_bool, (struct nmgregion *s1,
				struct nmgregion *s2,
				CONST int oper, CONST struct bn_tol *tol) );
BU_EXTERN(void			nmg_shell_coplanar_face_merge,
				(struct shell *s, CONST struct bn_tol *tol,
				CONST int simplify) );
BU_EXTERN(int			nmg_two_region_vertex_fuse, (struct nmgregion *r1,
				struct nmgregion *r2, CONST struct bn_tol *tol));
BU_EXTERN(union tree		*nmg_booltree_leaf_tess, (struct db_tree_state *tsp,
				struct db_full_path *pathp,
				struct rt_db_internal *ip, genptr_t client_data));
BU_EXTERN(union tree		*nmg_booltree_leaf_tnurb, (struct db_tree_state *tsp,
				struct db_full_path *pathp,
				struct rt_db_internal *ip, genptr_t client_data));
BU_EXTERN(union tree		*nmg_booltree_evaluate, (union tree *tp,
				CONST struct bn_tol *tol, struct resource *resp));
int nmg_boolean( union tree *tp, struct model *m, const struct bn_tol *tol, struct resource *resp );

/* from nmg_class.c */
BU_EXTERN(void			nmg_class_shells, (struct shell *sA,
				struct shell *sB, long *classlist[4],
				CONST struct bn_tol *tol) );

/* from nmg_fcut.c */
/* static void ptbl_vsort */
BU_EXTERN(int			nmg_ck_vu_ptbl, (struct bu_ptbl	*p,
				struct faceuse	*fu));
BU_EXTERN(double		nmg_vu_angle_measure, (struct vertexuse	*vu,
				vect_t x_dir, vect_t y_dir, int assessment,
				int in) );
#if 0
BU_EXTERN(int			nmg_is_v_on_rs_list, (CONST struct nmg_ray_state *rs,
				CONST struct vertex		*v));
BU_EXTERN(int			nmg_assess_eu, (struct edgeuse *eu,
				int			forw,
				struct nmg_ray_state	*rs,
				int			pos));
BU_EXTERN(int			nmg_assess_vu, (struct nmg_ray_state	*rs,
				int			pos));
BU_EXTERN(void			nmg_pr_vu_stuff, (CONST struct nmg_vu_stuff *vs));
#endif
BU_EXTERN(int			nmg_wedge_class, (int	ass,			/* assessment of two edges forming wedge */
				double	a, double	b));
#if 0
BU_EXTERN(int			nmg_face_coincident_vu_sort,
				(struct nmg_ray_state	*rs,
				int			start,
				int			end));
#endif
BU_EXTERN(void			nmg_sanitize_fu, (struct faceuse	*fu));
#if 0
BU_EXTERN(void			nmg_face_rs_init,
				(struct nmg_ray_state	*rs,
				struct bu_ptbl	*b,
				struct faceuse	*fu1,
				struct faceuse	*fu2,
				point_t		pt,
				vect_t		dir,
				struct edge_g_lseg		*eg,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_edge_geom_isect_line,
				(struct edgeuse		*eu,
				struct nmg_ray_state	*rs,
				CONST char		*reason));
#endif
BU_EXTERN(void			nmg_unlist_v,
				(struct bu_ptbl	*b,
				fastf_t *mag,
				struct vertex	*v));
#if 0
BU_EXTERN(int			nmg_onon_fix,
				(struct nmg_ray_state	*rs,
				struct bu_ptbl		*b,
				struct bu_ptbl		*ob,
				fastf_t			*mag,
				fastf_t			*omag));
#endif
BU_EXTERN(struct edge_g_lseg	*nmg_face_cutjoin, (
				struct bu_ptbl *b1, struct bu_ptbl *b2,
				fastf_t *mag1, fastf_t *mag2,
				struct faceuse *fu1, struct faceuse *fu2,
				point_t pt, vect_t dir,
				struct edge_g_lseg *eg,
				CONST struct bn_tol *tol) );
BU_EXTERN(void			nmg_fcut_face_2d,
				(struct bu_ptbl *vu_list,
				fastf_t *mag,
				struct faceuse *fu1,
				struct faceuse *fu2,
				struct bn_tol *tol));
BU_EXTERN(int			nmg_insert_vu_if_on_edge,
				(struct vertexuse *vu1,
				struct vertexuse *vu2,
				struct edgeuse *new_eu,
				struct bn_tol *tol));
/* nmg_face_state_transition */

#define nmg_mev(_v, _u)	nmg_me((_v), (struct vertex *)NULL, (_u))

/* From nmg_eval.c */
BU_EXTERN(void			nmg_ck_lu_orientation, (struct loopuse *lu,
				CONST struct bn_tol *tolp));
BU_EXTERN(CONST char		*nmg_class_name, (int class_no) );
BU_EXTERN(void			nmg_evaluate_boolean,
				(struct shell	*sA,
				struct shell	*sB,
				int		op,
				long		*classlist[8],
				CONST struct bn_tol	*tol));
#if 0
/* These can't be included because struct nmg_bool_state is in nmg_eval.c */
/* nmg_eval_shell */
/* nmg_eval_action */
/* nmg_eval_plot */
#endif


/* From nmg_rt_isect.c */
extern void nmg_rt_print_hitlist( struct hitmiss *hl );

extern void nmg_rt_print_hitmiss( struct hitmiss *a_hit);

extern int nmg_class_ray_vs_shell(struct xray *rp,
				  const struct shell *s,
				  const int in_or_out_only,
				  const struct bn_tol *tol);

BU_EXTERN(void nmg_isect_ray_model, (struct ray_data *rd) );

/* From nmg_rt_segs.c */
#if 0
/* Don't have "nmg_specific" */
BU_EXTERN(int nmg_ray_isect_segs, (struct soltab *stp,
					struct xray *rp,
					struct application *ap,
					struct seg *seghead,
					struct nmg_specific *nmg_spec) );
#endif

/* From nmg_ck.c */
BU_EXTERN(void			nmg_vvg, (CONST struct vertex_g *vg));
BU_EXTERN(void			nmg_vvertex, (CONST struct vertex *v,
				CONST struct vertexuse *vup));
BU_EXTERN(void			nmg_vvua, (CONST long *vua));
BU_EXTERN(void			nmg_vvu, (CONST struct vertexuse *vu,
				CONST long *up_magic_p));
BU_EXTERN(void			nmg_veg, (CONST long *eg));
BU_EXTERN(void			nmg_vedge, (CONST struct edge *e,
				CONST struct edgeuse *eup));
BU_EXTERN(void			nmg_veu, (CONST struct bu_list	*hp,
				CONST long *up_magic_p));
BU_EXTERN(void			nmg_vlg, (CONST struct loop_g *lg));
BU_EXTERN(void			nmg_vloop, (CONST struct loop *l,
				CONST struct loopuse *lup));
BU_EXTERN(void			nmg_vlu, (CONST struct bu_list	*hp,
				CONST long *up));
BU_EXTERN(void			nmg_vfg, (CONST struct face_g_plane *fg));
BU_EXTERN(void			nmg_vface, (CONST struct face *f,
				CONST struct faceuse *fup));
BU_EXTERN(void			nmg_vfu, (CONST struct bu_list	*hp,
				CONST struct shell *s));
BU_EXTERN(void			nmg_vshell, (CONST struct bu_list *hp,
				CONST struct nmgregion *r));
BU_EXTERN(void			nmg_vregion, (CONST struct bu_list *hp,
				CONST struct model *m));
BU_EXTERN(void			nmg_vmodel, (CONST struct model *m));

/* checking routines */
BU_EXTERN(void			nmg_ck_e, (CONST struct edgeuse *eu,
				CONST struct edge *e, CONST char *str));
BU_EXTERN(void			nmg_ck_vu, (CONST long *parent,
				CONST struct vertexuse *vu,
				CONST char *str));
BU_EXTERN(void			nmg_ck_eu, (CONST long *parent,
				CONST struct edgeuse *eu,
				CONST char *str));
BU_EXTERN(void			nmg_ck_lg, (CONST struct loop *l,
				CONST struct loop_g *lg,
				CONST char *str));
BU_EXTERN(void			nmg_ck_l, (CONST struct loopuse *lu,
				CONST struct loop *l,
				CONST char *str));
BU_EXTERN(void			nmg_ck_lu, (CONST long *parent,
				CONST struct loopuse *lu,
				CONST char *str));
BU_EXTERN(void			nmg_ck_fg, (CONST struct face *f,
				CONST struct face_g_plane *fg,
				CONST char *str));
BU_EXTERN(void			nmg_ck_f, (CONST struct faceuse *fu,
				CONST struct face *f,
				CONST char *str));
BU_EXTERN(void			nmg_ck_fu, (CONST struct shell *s,
				CONST struct faceuse *fu,
				CONST char *str));
BU_EXTERN(int			nmg_ck_eg_verts, (CONST struct edge_g_lseg *eg,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_ck_geometry, (CONST struct model *m,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_ck_face_worthless_edges, (CONST struct faceuse *fu));
BU_EXTERN(void			nmg_ck_lueu, (CONST struct loopuse *lu, CONST char *s) );
BU_EXTERN(int			nmg_check_radial, (CONST struct edgeuse *eu, CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_eu_2s_orient_bad, (CONST struct edgeuse	*eu,
				CONST struct shell	*s1,
				CONST struct shell	*s2,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_ck_closed_surf, (CONST struct shell *s, CONST struct bn_tol *tol) );
BU_EXTERN(int			nmg_ck_closed_region, (CONST struct nmgregion *r, CONST struct bn_tol *tol) );
BU_EXTERN(void			nmg_ck_v_in_2fus, (CONST struct vertex *vp,
				CONST struct faceuse *fu1, CONST struct faceuse *fu2,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_ck_vs_in_region, (CONST struct nmgregion *r,
				CONST struct bn_tol *tol));


/* From nmg_inter.c */
BU_EXTERN(struct vertexuse	*nmg_make_dualvu,
				(struct vertex *v,
				struct faceuse *fu,
				CONST struct bn_tol *tol));
BU_EXTERN(struct vertexuse	*nmg_enlist_vu, (struct nmg_inter_struct	*is,
				CONST struct vertexuse *vu,
				struct vertexuse *dualvu,
				fastf_t dist));
BU_EXTERN(void			nmg_isect2d_prep, (struct nmg_inter_struct *is,
				CONST long *assoc_use));
BU_EXTERN(void			nmg_isect2d_cleanup, (struct nmg_inter_struct *is));
BU_EXTERN(void			nmg_isect2d_final_cleanup, ());
BU_EXTERN(void			nmg_isect_vert2p_face2p, (struct nmg_inter_struct *is,
				struct vertexuse *vu1, struct faceuse *fu2));
BU_EXTERN(struct edgeuse	*nmg_break_eu_on_v, (struct edgeuse *eu1,
				struct vertex *v2, struct faceuse *fu,
				struct nmg_inter_struct *is));
BU_EXTERN(void			nmg_break_eg_on_v,
				(CONST struct edge_g_lseg	*eg,
				struct vertex		*v,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_isect_2colinear_edge2p,
				(struct edgeuse	*eu1,
				struct edgeuse	*eu2,
				struct faceuse		*fu,
				struct nmg_inter_struct	*is,
				struct bu_ptbl		*l1,
				struct bu_ptbl		*l2));
BU_EXTERN(int			nmg_isect_edge2p_edge2p,
				(struct nmg_inter_struct	*is,
				struct edgeuse		*eu1,
				struct edgeuse		*eu2,
				struct faceuse		*fu1,
				struct faceuse		*fu2));
BU_EXTERN(int			nmg_isect_construct_nice_ray,
				(struct nmg_inter_struct	*is,
				struct faceuse		*fu2));
BU_EXTERN(void			nmg_enlist_one_vu,
				(struct nmg_inter_struct	*is,
				CONST struct vertexuse	*vu,
				fastf_t			dist));
BU_EXTERN(int			nmg_isect_line2_edge2p,
				(struct nmg_inter_struct	*is,
				struct bu_ptbl		*list,
				struct edgeuse		*eu1,
				struct faceuse		*fu1,
				struct faceuse		*fu2));
BU_EXTERN(void			nmg_isect_line2_vertex2,
				(struct nmg_inter_struct	*is,
				struct vertexuse	*vu1,
				struct faceuse		*fu1));
BU_EXTERN(int			nmg_isect_two_ptbls,
				(struct nmg_inter_struct		*is,
				CONST struct bu_ptbl		*t1,
				CONST struct bu_ptbl		*t2));
BU_EXTERN(struct edge_g_lseg	*nmg_find_eg_on_line,
				(CONST long		*magic_p,
				CONST point_t		pt,
				CONST vect_t		dir,
				CONST struct bn_tol	*tol));
BU_EXTERN(int 			nmg_k0eu, (struct vertex	*v));
BU_EXTERN(struct vertex		*nmg_repair_v_near_v,
				(struct vertex		*hit_v,
				struct vertex		*v,
				CONST struct edge_g_lseg	*eg1,
				CONST struct edge_g_lseg	*eg2,
				int			bomb,
				CONST struct bn_tol	*tol));
BU_EXTERN(struct vertex		*nmg_search_v_eg,
				(CONST struct edgeuse		*eu,
				int				second,
				CONST struct edge_g_lseg	*eg1,
				CONST struct edge_g_lseg	*eg2,
				struct vertex		*hit_v,
				CONST struct bn_tol		*tol));
BU_EXTERN(struct vertex		*nmg_common_v_2eg,
				(struct edge_g_lseg	*eg1,
				struct edge_g_lseg	*eg2,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_is_vertex_on_inter,
				(struct vertex *v,
				struct faceuse *fu1,
				struct faceuse *fu2,
				struct nmg_inter_struct *is));
BU_EXTERN(void			nmg_isect_eu_verts,
				(struct edgeuse *eu,
				struct vertex_g *vg1,
				struct vertex_g *vg2,
				struct bu_ptbl *verts,
				struct bu_ptbl *inters,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_isect_eu_eu,
				(struct edgeuse *eu1,
				struct vertex_g *vg1a,
				struct vertex_g *vg1b,
				vect_t dir1,
				struct edgeuse *eu2,
				struct bu_ptbl *verts,
				struct bu_ptbl *inters,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_isect_eu_fu,
				(struct nmg_inter_struct *is,
				struct bu_ptbl		*verts,
				struct edgeuse		*eu,
				struct faceuse          *fu));
BU_EXTERN(void			nmg_isect_fu_jra,
				(struct nmg_inter_struct	*is,
				struct faceuse		*fu1,
				struct faceuse		*fu2,
				struct bu_ptbl		*eu1_list,
				struct bu_ptbl		*eu2_list));
BU_EXTERN(void			nmg_isect_line2_face2pNEW, (struct nmg_inter_struct *is,
				struct faceuse *fu1, struct faceuse *fu2,
				struct bu_ptbl *eu1_list,
				struct bu_ptbl *eu2_list));
BU_EXTERN(int			nmg_is_eu_on_line3,
				(CONST struct edgeuse	*eu,
				CONST point_t		pt,
				CONST vect_t		dir,
				CONST struct bn_tol	*tol));
BU_EXTERN(struct edge_g_lseg	*nmg_find_eg_between_2fg,
				(CONST struct faceuse	*ofu1,
				CONST struct faceuse	*fu2,
				CONST struct bn_tol	*tol));
BU_EXTERN(struct edgeuse	*nmg_does_fu_use_eg,
				(CONST struct faceuse	*fu1,
				CONST long		*eg));
BU_EXTERN(int			rt_line_on_plane,
				(CONST point_t	pt,
				CONST vect_t	dir,
				CONST plane_t	plane,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_cut_lu_into_coplanar_and_non,
				(struct loopuse *lu,
				plane_t pl,
				struct nmg_inter_struct *is));
BU_EXTERN(void			nmg_check_radial_angles,
				(char *str,
				struct shell *s,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_faces_can_be_intersected,
				(struct nmg_inter_struct *bs,
				CONST struct faceuse *fu1,
				CONST struct faceuse *fu2,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_isect_two_generic_faces,
				(struct faceuse		*fu1,
				struct faceuse		*fu2,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_crackshells, (struct shell *s1, struct shell *s2, CONST struct bn_tol *tol) );
BU_EXTERN(int			nmg_fu_touchingloops, (CONST struct faceuse *fu));


/* From nmg_index.c */
BU_EXTERN(int			nmg_index_of_struct, (CONST long *p) );
BU_EXTERN(void			nmg_m_set_high_bit, (struct model *m));
BU_EXTERN(void			nmg_m_reindex, (struct model *m, long newindex) );
BU_EXTERN(void			nmg_vls_struct_counts, (struct bu_vls *str,
				CONST struct nmg_struct_counts *ctr));
BU_EXTERN(void			nmg_pr_struct_counts, 
				(CONST struct nmg_struct_counts *ctr,
				CONST char *str) );
BU_EXTERN(long			**nmg_m_struct_count,
				(struct nmg_struct_counts *ctr,
				CONST struct model *m) );
BU_EXTERN(void			nmg_struct_counts,
				(CONST struct model	*m,
				CONST char		*str));
BU_EXTERN(void			nmg_merge_models, (struct model *m1,
							struct model *m2) );
BU_EXTERN(long			nmg_find_max_index, (CONST struct model *m));

/* From nmg_rt.c */

/* From rt_dspline.c */
BU_EXTERN(void			rt_dspline_matrix, (mat_t m,CONST char *type,
					CONST double	tension,
					CONST double	bias) );
BU_EXTERN(double		rt_dspline4, (mat_t m, double a, double b,
					double c, double d, double alpha) );
BU_EXTERN(void			rt_dspline4v, (double *pt, CONST mat_t m,
					CONST double *a, CONST double *b,
					CONST double *c, CONST double *d,
					CONST int depth, CONST double alpha) );
BU_EXTERN(void			rt_dspline_n, (double *r, CONST mat_t m,
					CONST double *knots, CONST int n,
					CONST int depth, CONST double alpha));

/* From nurb_bezier.c */
BU_EXTERN( int rt_nurb_bezier, (struct bu_list *bezier_hd,
		CONST struct face_g_snurb *orig_surf, struct resource *res));
BU_EXTERN(int rt_bez_check, (CONST struct face_g_snurb * srf));
BU_EXTERN(int nurb_crv_is_bezier, (CONST struct edge_g_cnurb *crv));
BU_EXTERN(void nurb_c_to_bezier, (struct bu_list *clist, struct edge_g_cnurb *crv));

/* From nurb_copy.c */
BU_EXTERN(struct face_g_snurb *rt_nurb_scopy, (CONST struct face_g_snurb * srf,
		struct resource *res));
BU_EXTERN(struct edge_g_cnurb *rt_nurb_crv_copy, (CONST struct edge_g_cnurb * crv));

/* From nmg_fuse.c */
BU_EXTERN(int			nmg_is_common_bigloop, (CONST struct face *f1,
				CONST struct face *f2));
BU_EXTERN(void			nmg_region_v_unique, (struct nmgregion *r1,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_ptbl_vfuse, (struct bu_ptbl *t,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_region_both_vfuse, (struct bu_ptbl *t1,
				struct bu_ptbl *t2, CONST struct bn_tol	*tol));
/* nmg_two_region_vertex_fuse replaced with nmg_model_vertex_fuse */
BU_EXTERN(int			nmg_model_vertex_fuse, (struct model *m,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_cnurb_is_linear, (CONST struct edge_g_cnurb *cnrb));
BU_EXTERN(int			nmg_snurb_is_planar, (CONST struct face_g_snurb *srf, CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_eval_linear_trim_curve,
				(CONST struct face_g_snurb *snrb,
				CONST fastf_t uvw[3], point_t xyz));
BU_EXTERN(void			nmg_eval_trim_curve,
				(CONST struct edge_g_cnurb *cnrb,
				CONST struct face_g_snurb *snrb,
				CONST fastf_t t, point_t xyz));
/* nmg_split_trim */
BU_EXTERN(void			nmg_eval_trim_to_tol,
				(CONST struct edge_g_cnurb *cnrb,
				CONST struct face_g_snurb *snrb,
				CONST fastf_t t0, CONST fastf_t t1,
				struct bu_list *head,
				CONST struct bn_tol *tol));
/* nmg_split_linear_trim */
BU_EXTERN(void			nmg_eval_linear_trim_to_tol,
				(CONST struct edge_g_cnurb *cnrb,
				CONST struct face_g_snurb *snrb,
				CONST fastf_t uvw1[3],
				CONST fastf_t uvw2[3],
				struct bu_list *head,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_cnurb_lseg_coincident,
				(CONST struct edgeuse *eu1,
				CONST struct edge_g_cnurb *cnrb,
				CONST struct face_g_snurb *snrb,
				CONST point_t pt1,
				CONST point_t pt2,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_cnurb_is_on_crv,
				(CONST struct edgeuse *eu,
				CONST struct edge_g_cnurb *cnrb,
				CONST struct face_g_snurb *snrb,
				CONST struct bu_list *head,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_model_edge_fuse,
				(struct model *m, CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_model_edge_g_fuse,
				(struct model		*m,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_ck_fu_verts, (struct faceuse *fu1,
				struct face *f2,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_ck_fg_verts, (struct faceuse *fu1,
				struct face *f2, CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_two_face_fuse, (struct face	*f1,
				struct face *f2,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_model_face_fuse, (struct model *m,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_break_all_es_on_v, (long *magic_p,
				struct vertex *v, CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_model_break_e_on_v, (struct model *m,
				CONST struct bn_tol *tol));
BU_EXTERN(int			nmg_model_fuse, (struct model *m,
				CONST struct bn_tol *tol));

	/* radial routines */
BU_EXTERN(void			nmg_radial_sorted_list_insert,
				(struct bu_list *hd, struct nmg_radial *rad));
BU_EXTERN(void			nmg_radial_verify_pointers,
				(CONST struct bu_list *hd,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_radial_verify_monotone,
				(CONST struct bu_list	*hd,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_insure_radial_list_is_increasing,
				(struct bu_list	*hd,
				fastf_t amin, fastf_t amax));
BU_EXTERN(void			nmg_radial_build_list,
				(struct bu_list		*hd,
				struct bu_ptbl		*shell_tbl,
				int			existing,
				struct edgeuse		*eu,
				CONST vect_t		xvec,
				CONST vect_t		yvec,
				CONST vect_t		zvec,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_radial_merge_lists,
				(struct bu_list		*dest,
				struct bu_list		*src,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_is_crack_outie,
				(CONST struct edgeuse	*eu,
				CONST struct bn_tol	*tol));
BU_EXTERN(struct nmg_radial	*nmg_find_radial_eu, (CONST struct bu_list *hd,
				CONST struct edgeuse *eu));
BU_EXTERN(CONST struct edgeuse	*nmg_find_next_use_of_2e_in_lu,
				(CONST struct edgeuse	*eu,
				CONST struct edge	*e1,
				CONST struct edge	*e2));
BU_EXTERN(void			nmg_radial_mark_cracks,
				(struct bu_list		*hd,
				CONST struct edge	*e1,
				CONST struct edge	*e2,
				CONST struct bn_tol	*tol));
BU_EXTERN(struct nmg_radial	*nmg_radial_find_an_original,
				(CONST struct bu_list	*hd,
				CONST struct shell	*s,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_radial_mark_flips,
				(struct bu_list		*hd,
				CONST struct shell	*s,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_radial_check_parity,
				(CONST struct bu_list	*hd,
				CONST struct bu_ptbl	*shells,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_radial_implement_decisions,
				(struct bu_list		*hd,
				CONST struct bn_tol	*tol,
				struct edgeuse		*eu1,
				vect_t xvec, vect_t yvec, vect_t zvec));
BU_EXTERN(void			nmg_pr_radial, (CONST char *title,
				CONST struct nmg_radial	*rad));
BU_EXTERN(void			nmg_pr_radial_list, (CONST struct bu_list *hd,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_do_radial_flips, (struct bu_list *hd));
BU_EXTERN(void			nmg_do_radial_join, (struct bu_list *hd,
				struct edgeuse *eu1ref,
				vect_t xvec, vect_t yvec, vect_t zvec,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_radial_join_eu_NEW,
				(struct edgeuse *eu1, struct edgeuse *eu2,
				CONST struct bn_tol *tol));
BU_EXTERN(void			nmg_radial_exchange_marked,
				(struct bu_list		*hd,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_s_radial_harmonize,
				(struct shell		*s,
				CONST struct bn_tol	*tol));
BU_EXTERN(int			nmg_eu_radial_check,
				(CONST struct edgeuse	*eu,
				CONST struct shell	*s,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_s_radial_check,
				(struct shell		*s,
				CONST struct bn_tol	*tol));
BU_EXTERN(void			nmg_r_radial_check,
				(CONST struct nmgregion	*r,
				CONST struct bn_tol	*tol));


BU_EXTERN(struct edge_g_lseg	*nmg_pick_best_edge_g, (struct edgeuse *eu1,
				struct edgeuse *eu2, CONST struct bn_tol *tol));

/* nmg_visit.c */
BU_EXTERN(void			nmg_visit_vertex,
				(struct vertex			*v,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_vertexuse,
				(struct vertexuse		*vu,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_edge,
				(struct edge			*e,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_edgeuse,
				(struct edgeuse			*eu,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_loop,
				(struct loop			*l,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_loopuse,
				(struct loopuse			*lu,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_face,
				(struct face			*f,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_faceuse,
				(struct faceuse			*fu,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_shell,
				(struct shell			*s,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_region,
				(struct nmgregion		*r,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit_model,
				(struct model			*model,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));
BU_EXTERN(void			nmg_visit,
				(CONST long			*magicp,
				CONST struct nmg_visit_handlers	*htab,
				genptr_t			*state));

/* db5_types.c */
BU_EXTERN(int			db5_type_tag_from_major,
				(char				**tag,
				CONST unsigned char		major));

BU_EXTERN(int			db5_type_descrip_from_major,
				(char				**descrip,
				CONST unsigned char		major));

BU_EXTERN(int			db5_type_tag_from_codes,
				(char				**tag,
				CONST unsigned char		major,
				CONST unsigned char		minor));

BU_EXTERN(int			db5_type_descrip_from_codes,
				(char				**descrip,
				CONST unsigned char		major,
				CONST unsigned char		minor));

BU_EXTERN(int			db5_type_codes_from_tag,
				(unsigned char			*major,
				unsigned char			*minor,
				CONST char			*tag));

BU_EXTERN(int			db5_type_codes_from_descrip,
				(unsigned char			*major,
				unsigned char			*minor,
				CONST char			*descrip));

BU_EXTERN(size_t		db5_type_sizeof_h_binu,
				(CONST unsigned char		minor));

BU_EXTERN(size_t		db5_type_sizeof_n_binu,
				(CONST unsigned char		minor));

#endif

/*
 *  Constants provided and used by the RT library.
 */
extern CONST struct db_tree_state	rt_initial_tree_state;
extern const char   *rt_vlist_cmd_descriptions[];

/* vers.c (created by librt/Cakefile) */
extern CONST char   rt_version[];

#ifdef __cplusplus
}
#endif

#endif /* RAYTRACE_H */
