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

#ifdef __cplusplus
extern "C" {
#endif

#define RAYTRACE_H_VERSION	"@(#)$Header$ (BRL)"

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
	long	idb_magic;
	int	idb_type;		/* ID_xxx */
	genptr_t idb_ptr;
};
#define RT_DB_INTERNAL_MAGIC	0x0dbbd867
#define RT_INIT_DB_INTERNAL(_p)	{(_p)->idb_magic = RT_DB_INTERNAL_MAGIC; \
	(_p)->idb_type = -1; (_p)->idb_ptr = GENPTR_NULL;}
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
#define DB_FULL_PATH_MAGIC	0x64626670
#define RT_CK_FULL_PATH(_p)	BU_CKMAG(_p, DB_FULL_PATH_MAGIC, "db_full_path")

/*
 *			X R A Y
 *
 * All necessary information about a ray.
 * Not called just "ray" to prevent conflicts with VLD stuff.
 */
struct xray {
	point_t		r_pt;		/* Point at which ray starts */
	vect_t		r_dir;		/* Direction of ray (UNIT Length) */
	fastf_t		r_min;		/* entry dist to bounding sphere */
	fastf_t		r_max;		/* exit dist from bounding sphere */
};
#define RAY_NULL	((struct xray *)0)


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
	fastf_t		hit_dist;	/* dist from r_pt to hit_point */
	point_t		hit_point;	/* Intersection point */
	vect_t		hit_normal;	/* Surface Normal at hit_point */
	vect_t		hit_vpriv;	/* PRIVATE vector for xxx_*() */
	genptr_t	hit_private;	/* PRIVATE handle for xxx_shot() */
	int		hit_surfno;	/* solid-specific surface indicator */
};
#define HIT_NULL	((struct hit *)0)

/*
 * Old macro:
 *  Only the hit_dist field of pt_inhit and pt_outhit are valid
 *  when a_hit() is called;  to compute both hit_point and hit_normal,
 *  use RT_HIT_NORM() macro;  to compute just hit_point, use
 *  VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
 */
#define RT_HIT_NORM( _hitp, _stp, _rayp )  { \
	register int _id = (_stp)->st_id; \
	RT_CHECK_SOLTAB(_stp); \
	if( _id <= 0 || _id > ID_MAXIMUM ) { \
		bu_log("stp=x%x, id=%d. hitp=x%x, rayp=x%x\n", _stp, _id, _hitp, _rayp); \
		rt_bomb("RT_HIT_NORM:  bad st_id");\
	} \
	rt_functab[_id].ft_norm(_hitp, _stp, _rayp); }

/*
 *  New macro:  Leave _hitp undisturbed, return post-boolean normal into
 *  caller-provided point.
 */
#define RT_HIT_NORMAL( _normal, _hitp, _stp, _rayp, _flipflag )  { \
	RT_CHECK_SOLTAB(_stp); \
	rt_functab[(_stp)->st_id].ft_norm(_hitp, _stp, _rayp); \
	if( _flipflag )  { \
		VREVERSE( _normal, (_hitp)->hit_normal ); \
	} else { \
		VMOVE( _normal, (_hitp)->hit_normal ); \
	} \
 }


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
	register int _id = (_stp)->st_id; \
	RT_CHECK_SOLTAB(_stp); \
	if( _id <= 0 || _id > ID_MAXIMUM )  { \
		bu_log("stp=x%x, id=%d.\n", _stp, _id); \
		rt_bomb("RT_CURVE:  bad st_id"); \
	} \
	rt_functab[_id].ft_curve( _curvp, _hitp, _stp ); \
	if( _flipflag )  { \
		(_curvp)->crv_c1 = - (_curvp)->crv_c1; \
		(_curvp)->crv_c2 = - (_curvp)->crv_c2; \
	} \
 }

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
	register int _id = (_stp)->st_id; \
	RT_CHECK_SOLTAB(_stp); \
	if( _id <= 0 || _id > ID_MAXIMUM )  { \
		bu_log("stp=x%x, id=%d.\n", _stp, _id); \
		rt_bomb("RT_UVCOORD:  bad st_id"); \
	} \
	rt_functab[_id].ft_uv( ap, _stp, _hitp, uvp ); }


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
 * XXX move to vmath.h
 */

/*
 *  Compare two bounding RPPs;  return true if disjoint.
 *  RPP 1 is defined by lo1, hi1, RPP 2 by lo2, hi2.
 */
#define RT_2RPP_DISJOINT(_l1, _h1, _l2, _h2) \
      ( (_l1)[X] > (_h2)[X] || (_l1)[Y] > (_h2)[Y] || (_l1)[2] > (_h2)[2] || \
	(_l2)[X] > (_h1)[X] || (_l2)[Y] > (_h1)[1] || (_l2)[2] > (_h1)[2] )

/* Test for point being inside or on an RPP */
#define RT_POINT_IN_RPP(_pt, _min, _max)	\
	( ( (_pt)[X] >= (_min)[X] && (_pt)[X] <= (_max)[X] ) &&  \
	  ( (_pt)[Y] >= (_min)[Y] && (_pt)[Y] <= (_max)[Y] ) &&  \
	  ( (_pt)[Z] >= (_min)[Z] && (_pt)[Z] <= (_max)[Z] ) )

/*
 *			S O L T A B
 *
 * Internal information used to keep track of solids in the model
 * Leaf name and Xform matrix are unique identifier.
 */
struct soltab {
	struct bu_list	l;		/* links, headed by rti_headsolid */
	struct bu_list	l2;		/* links, headed by st_dp->d_use_hd */
	struct rt_i	*st_rtip;	/* "up" pointer to rt_i */
	int		st_uses;	/* Usage count, for instanced solids */
	int		st_id;		/* Solid ident */
	vect_t		st_center;	/* Centroid of solid */
	fastf_t		st_aradius;	/* Radius of APPROXIMATING sphere */
	fastf_t		st_bradius;	/* Radius of BOUNDING sphere */
	genptr_t	st_specific;	/* -> ID-specific (private) struct */
	CONST struct directory *st_dp;	/* Directory entry of solid */
	vect_t		st_min;		/* min X, Y, Z of bounding RPP */
	vect_t		st_max;		/* max X, Y, Z of bounding RPP */
	int		st_bit;		/* solids bit vector index (const) */
	struct bu_ptbl	st_regions;	/* ptrs to regions using this solid (const) */
	matp_t		st_matp;	/* solid coords to model space, NULL=identity */
	struct db_full_path st_path;	/* path from region to leaf */
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

#define ID_MAXIMUM	24	/* Maximum defined ID_xxx value */

#define ID_COMBINATION	(ID_MAXIMUM+1)	/* Combination Record (non-geometric) */

/*
 *			M A T E R _ I N F O
 */
struct mater_info {
	float	ma_color[3];		/* explicit color:  0..1  */
	char	ma_override;		/* non-0 ==> ma_color is valid */
	char	ma_cinherit;		/* DB_INH_LOWER / DB_INH_HIGHER */
	char	ma_minherit;		/* DB_INH_LOWER / DB_INH_HIGHER */
	char	*ma_shader;		/* shader name & parms */
};

/*
 *			R E G I O N
 *
 *  The region structure.
 */
struct region  {
	long		reg_magic;
	CONST char	*reg_name;	/* Identifying string */
	union tree	*reg_treetop;	/* Pointer to boolean tree */
	int		reg_bit;	/* constant index into Regions[] */
	int		reg_regionid;	/* Region ID code.  If <=0, use reg_aircode */
	int		reg_aircode;	/* Region ID AIR code */
	int		reg_gmater;	/* GIFT Material code */
	int		reg_los;	/* equivalent LOS estimate ?? */
	struct region	*reg_forw;	/* linked list of all regions */
	struct mater_info reg_mater;	/* Real material information */
	genptr_t	reg_mfuncs;	/* User appl. funcs for material */
	genptr_t	reg_udata;	/* User appl. data for material */
	int		reg_transmit;	/* flag:  material transmits light */
	int		reg_instnum;	/* instance number, from d_uses */
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
 *  NOTE:  rt_get_pt allows enough storage at the end of the partition
 *  for a bit vector of "rt_i.nsolids" bits in length.
 *
 *  NOTE:  The number of solids in a model can change from frame to frame
 *  due to the effect of animations, so partition structures can be expected
 *  to change length over the course of a single application program.
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
	struct bu_ptbl	pt_solids_hit;
};
#define PT_NULL		((struct partition *)0)
#define PT_MAGIC	0x87687681
#define PT_HD_MAGIC	0x87687680

#define RT_CHECK_PT(_p)	RT_CK_PT(_p)	/* compat */
#define RT_CK_PT(_p)	BU_CKMAG(_p,PT_MAGIC, "struct partition")
#define RT_CK_PT_HD(_p)	BU_CKMAG(_p,PT_HD_MAGIC, "struct partition list head")

/* Macros for copying only the essential "middle" part of a partition struct */
#define RT_PT_MIDDLE_START	pt_inseg		/* 1st elem to copy */
#define RT_PT_MIDDLE_END	pt_solids_hit.l.magic	/* copy up to this elem (non-inclusive) */
#define RT_PT_MIDDLE_LEN(p) \
	(((char *)&(p)->RT_PT_MIDDLE_END) - ((char *)&(p)->RT_PT_MIDDLE_START))

#define RT_DUP_PT(ip,new,old,res)	{ \
	GET_PT(ip,new,res); \
	bcopy((char *)(&(old)->RT_PT_MIDDLE_START), (char *)(&(new)->RT_PT_MIDDLE_START), RT_PT_MIDDLE_LEN(old) ); \
	bu_ptbl_cat( &(new)->pt_solids_hit, &(old)->pt_solids_hit );  }

/* Clear out the pointers, empty the hit list */
#define GET_PT_INIT(ip,p,res)	{\
	GET_PT(ip,p,res); \
	bzero( ((char *) &(p)->RT_PT_MIDDLE_START), RT_PT_MIDDLE_LEN(p) ); }

#define GET_PT(ip,p,res)   { \
	if( BU_LIST_NON_EMPTY_P(p, partition, &res->re_parthead) )  { \
		BU_LIST_DEQUEUE((struct bu_list *)(p)); \
		bu_ptbl_reset( &(p)->pt_solids_hit ); \
	} else { \
		(p) = (struct partition *)bu_malloc(sizeof(struct partition), "struct partition"); \
		(p)->pt_magic = PT_MAGIC; \
		bu_ptbl_init( &(p)->pt_solids_hit, 42, "pt_solids_hit ptbl" ); \
		(res)->re_partlen++; \
	} \
	res->re_partget++; }

#define FREE_PT(p,res)  { \
	BU_LIST_APPEND( &(res->re_parthead), (struct bu_list *)(p) ); \
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
 *  cn_type is an integer for efficiency of access in rt_shootray()
 *  on non-word addressing machines.
 */
union cutter  {
#define CUT_CUTNODE	1
#define CUT_BOXNODE	2
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
		struct soltab **bn_list;
		int	bn_len;		/* # of solids in list */
		int	bn_maxlen;	/* # of ptrs allocated to list */
	} bn;
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
#define	RT_DBNHASH		128	/* size of hash table */

#if	((RT_DBNHASH)&((RT_DBNHASH)-1)) != 0
#define	RT_DBHASH(sum)	((unsigned)(sum) % (RT_DBNHASH))
#else
#define	RT_DBHASH(sum)	((unsigned)(sum) & ((RT_DBNHASH)-1))
#endif

/*
 *			D B _ I
 *
 *  One of these structures is used to describe each separate instance
 *  of a model database file.
 *
 *  The contents of this structure are intended to be "opaque" to
 *  application programmers.
 */
struct db_i  {
	long			dbi_magic;	/* magic number */
	struct directory	*dbi_Head[RT_DBNHASH];
	int			dbi_fd;		/* UNIX file descriptor */
	FILE			*dbi_fp;	/* STDIO file descriptor */
	long			dbi_eof;	/* End+1 pos after db_scan() */
	long			dbi_nrec;	/* # records after db_scan() */
	int			dbi_localunit;	/* unit currently in effect */
	double			dbi_local2base;
	double			dbi_base2local;	/* unit conversion factors */
	char			*dbi_title;	/* title from IDENT rec */
	char			*dbi_filename;	/* file name */
	int			dbi_read_only;	/* !0 => read only file */
	struct mem_map		*dbi_freep;	/* map of free granules */
	char			*dbi_inmem;	/* ptr to in-memory copy */
	char			*dbi_shmaddr;	/* ptr to memory-mapped file */
	struct animate		*dbi_anroot;	/* heads list of anim at root lvl */
};
#define DBI_NULL	((struct db_i *)0)
#define DBI_MAGIC	0x57204381

#define RT_CHECK_DBI(_p)	BU_CKMAG(_p,DBI_MAGIC,"struct db_i")
#define RT_CK_DBI(_p)		BU_CKMAG(_p,DBI_MAGIC,"struct db_i")

/*
 *			D I R E C T O R Y
 */
struct directory  {
	long		d_magic;		/* Magic number */
	char		*d_namep;		/* pointer to name string */
	long		d_addr;			/* disk address in obj file */
	struct directory *d_forw;		/* link to next dir entry */
	struct animate	*d_animate;		/* link to animation */
	long		d_uses;			/* # uses, from instancing */
	long		d_len;			/* # of db granules used */
	long		d_nref;			/* # times ref'ed by COMBs */
	int		d_flags;		/* flags */
	struct bu_list	d_use_hd;		/* heads list of uses (struct soltab l2) */
};
#define DIR_NULL	((struct directory *)0)
#define RT_DIR_MAGIC	0x05551212		/* Directory assistance */
#define RT_CK_DIR(_dp)	BU_CKMAG(_dp, RT_DIR_MAGIC, "(librt)directory")

#define RT_DIR_PHONY_ADDR	(-1L)	/* Special marker for d_addr field */

#define DIR_SOLID	0x1		/* this name is a solid */
#define DIR_COMB	0x2		/* combination */
#define DIR_REGION	0x4		/* region */

/* Args to db_lookup() */
#define LOOKUP_NOISY	1
#define LOOKUP_QUIET	0

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
	/* Begin GIFT compatability */
	short		region_id;
	short		aircode;
	short		GIFTmater;
	short		los;
	/* End GIFT compatability */
	char		rgb_valid;	/* !0 ==> rgb[] has valid color */
	unsigned char	rgb[3];
	struct rt_vls	shader;
	struct rt_vls	material;
	char		inherit;
};
#define RT_COMB_MAGIC	0x436f6d49	/* "ComI" */
#define RT_CK_COMB(_p)		BU_CKMAG( _p , RT_COMB_MAGIC , "rt_comb_internal" )

/*
 *			D B _ T R E E _ S T A T E
 *
 *  State for database tree walker db_walk_tree()
 *  and related user-provided handler routines.
 */
struct db_tree_state {
	struct db_i	*ts_dbip;
	int		ts_sofar;		/* Flag bits */

	int		ts_regionid;	/* GIFT compat region ID code*/
	int		ts_aircode;	/* GIFT compat air code */
	int		ts_gmater;	/* GIFT compat material code */
	int		ts_los;		/* equivalent LOS estimate .. */
	struct mater_info ts_mater;	/* material properties */

	mat_t		ts_mat;		/* transform matrix */

	int		ts_stop_at_regions;	/* else stop at solids */
	int		(*ts_region_start_func) RT_ARGS((
				struct db_tree_state * /*tsp*/,
				struct db_full_path * /*pathp*/
			));
	union tree *	(*ts_region_end_func) RT_ARGS((
				struct db_tree_state * /*tsp*/,
				struct db_full_path * /*pathp*/,
				union tree * /*curtree*/
			));
	union tree *	(*ts_leaf_func) RT_ARGS((
				struct db_tree_state * /*tsp*/,
				struct db_full_path * /*pathp*/,
				struct bu_external * /*ep*/,
				int /*id*/
			));
	CONST struct rt_tess_tol *ts_ttol;	/* Tessellation tolerance */
	CONST struct bn_tol	*ts_tol;	/* Math tolerance */
#if defined(NMG_H)
	struct model		**ts_m;		/* ptr to ptr to NMG "model" */
#else
	genptr_t		*ts_m;		/* ptr to genptr */
#endif
};
#define TS_SOFAR_MINUS	1		/* Subtraction encountered above */
#define TS_SOFAR_INTER	2		/* Intersection encountered above */
#define TS_SOFAR_REGION	4		/* Region encountered above */

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
	}		an_u;
};
#define RT_AN_MATRIX	1			/* Matrix animation */
#define RT_AN_MATERIAL	2			/* Material property anim */
#define RT_AN_COLOR	3			/* Material color anim */
#define RT_AN_SOLID	4			/* Solid parameter anim */

#define ANIM_NULL	((struct animate *)0)
#define ANIMATE_MAGIC	0x414e4963		/* 1095649635 */
#define RT_CK_ANIMATE(_p)	BU_CKMAG((_p), ANIMATE_MAGIC, "animate")

/*
 *			R E S O U R C E
 *
 *  One of these structures is allocated per processor.
 *  To prevent excessive competition for free structures,
 *  memory is now allocated on a per-processor basis.
 *  The application structure a_resource element specifies
 *  the resource structure to be used;  if uniprocessing,
 *  a null a_resource pointer results in using the internal global
 *  structure, making initial application development simpler.
 *
 *  Applications are responsible for filling the resource structure
 *  with zeros before letting librt use them.
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
};
extern struct resource	rt_uniresource;	/* default.  Defined in librt/shoot.c */
#define RESOURCE_NULL	((struct resource *)0)
#define RESOURCE_MAGIC	0x83651835
#define RT_RESOURCE_CHECK(_p)	BU_CKMAG(_p, RESOURCE_MAGIC, "struct resource")
#define RT_CK_RESOURCE(_p)	BU_CKMAG(_p, RESOURCE_MAGIC, "struct resource")

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
 *	a_overlap()	If non-null, this routine will be called to
 *			handle overlap conditions.  See librt/bool.c
 *			for calling sequence.
 *			Return of 0 eliminates partition with overlap entirely
 *			Return of !0 retains one (random) partition in output
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
 *  rt_calloc( 1, sizeof(struct application), "application" );
 *  While this practice may not work on machines where "all bits off"
 *  does not signify a floating point zero, BRL-CAD does not support any
 *  such machines, so this is a moot issue.
 */
struct application  {
	/* THESE ELEMENTS ARE MANDATORY */
	struct xray	a_ray;		/* Actual ray to be shot */
	int		(*a_hit)();	/* called when shot hits model */
	int		(*a_miss)();	/* called when shot misses */
	int		a_onehit;	/* flag to stop on first hit */
	fastf_t		a_ray_length;	/* distance from ray start to end intersections */
	struct rt_i	*a_rt_i;	/* this librt instance */
	int		a_zero1;	/* must be zero (sanity check) */
	/* THESE ELEMENTS ARE USED BY THE LIBRARY, BUT MAY BE LEFT ZERO */
	struct resource	*a_resource;	/* dynamic memory resources */
	int		(*a_overlap)();	/* called when overlaps occur */
	int		a_level;	/* recursion level (for printing) */
	int		a_x;		/* Screen X of ray, if applicable */
	int		a_y;		/* Screen Y of ray, if applicable */
	char		*a_purpose;	/* Debug string:  purpose of ray */
	fastf_t		a_rbeam;	/* initial beam radius (mm) */
	fastf_t		a_diverge;	/* slope of beam divergance/mm */
	int		a_return;	/* Return of a_hit()/a_miss() */
	/* THE FOLLOWING ELEMENTS ARE MAINLINE & APPLICATION SPECIFIC. */
	/* THEY ARE NEVER EXAMINED BY THE LIBRARY. */
	int		a_user;		/* application-specific value */
	genptr_t	a_uptr;		/* application-specific pointer */
	fastf_t		a_color[3];	/* application-specific color */
	fastf_t		a_dist;		/* application-specific distance */
	vect_t		a_uvec;		/* application-specific vector */
	vect_t		a_vvec;		/* application-specific vector */
	fastf_t		a_refrac_index;	/* current index of refraction */
	fastf_t		a_cumlen;	/* cumulative length of ray */
	int		a_zero2;	/* must be zero (sanity check) */
};
#define RT_AFN_NULL	((int (*)())0)

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
	int		debug;		/* non-zero for debug, see debug.h */
	/*  Definitions necessary to interlock in a parallel environment */
	int		rtg_parallel;	/* !0 = trying to use multi CPUs */
	long		res_syscall;	/* lock on system calls */
	long		res_worker;	/* lock on work to do */
	long		res_stats;	/* lock on statistics */
	long		res_results;	/* lock on result buffer */
	long		res_model;	/* lock on model growth (splines) */
	struct bu_list	rtg_vlfree;	/* head of rt_vlist freelist */
	int		NMG_debug;	/* debug bits for NMG's see nmg.h */
};
extern struct rt_g rt_g;

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
	int		useair;		/* "air" regions are used */
	int		rti_nlights;	/* number of light sources */
	char		*rti_region_fix_file; /* rt_regionfix() file or NULL */
	/* THESE ITEMS ARE AVAILABLE FOR APPLICATIONS TO READ */
	vect_t		mdl_min;	/* min corner of model bounding RPP */
	vect_t		mdl_max;	/* max corner of model bounding RPP */
	vect_t		rti_pmin;	/* for plotting, min RPP */
	vect_t		rti_pmax;	/* for plotting, max RPP */
	double		rti_radius;	/* radius of model bounding sphere */
	struct db_i	*rti_dbip;	/* prt to Database instance struct */
	/* THESE ITEMS SHOULD BE CONSIDERED OPAQUE, AND SUBJECT TO CHANGE */
	int		needprep;	/* needs rt_prep */
	struct region	**Regions;	/* ptrs to regions [reg_bit] */
	struct region	*HeadRegion;	/* ptr of list of regions in model */
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
	union cutter	rti_CutHead;	/* Head of cut tree */
	union cutter	rti_inf_box;	/* List of infinite solids */
	union cutter	*rti_CutFree;	/* cut Freelist */
	struct bu_ptbl	rti_busy_cutter_nodes; /* List of "cutter" mallocs */
	struct bu_ptbl	rti_cuts_waiting;
	int		rti_cut_maxlen;	/* max len RPP list in 1 cut bin */
	int		rti_cut_nbins;	/* number of cut bins (leaves) */
	int		rti_cut_totobj;	/* # objs in all bins, total */
	int		rti_cut_maxdepth;/* max depth of cut tree */
	struct soltab	**rti_sol_by_type[ID_MAXIMUM+1];
	int		rti_nsol_by_type[ID_MAXIMUM+1];
	int		rti_maxsol_by_type;
	int		rti_air_discards;/* # of air regions discarded */
	struct bu_hist rti_hist_cellsize; /* occupancy of cut cells */
	struct bu_hist rti_hist_cutdepth; /* depth of cut tree */
	struct soltab	**rti_Solids;	/* ptrs to soltab [st_bit] */
	struct bn_tol	rti_tol;	/* Tolerances for this model */
	struct bu_list	rti_solidheads[RT_DBNHASH]; /* active solid lists */
	struct bu_ptbl	rti_resources;	/* list of 'struct resource'es encountered */
};
#define RTI_NULL	((struct rt_i *)0)
#define RTI_MAGIC	0x99101658	/* magic # for integrity check */

#define RT_CHECK_RTI(_p)	BU_CKMAG(_p, RTI_MAGIC, "struct rt_i")
#define RT_CK_RTI(_p)		BU_CKMAG(_p, RTI_MAGIC, "struct rt_i")

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
 *			R T _ V L I S T
 *
 *  Definitions for handling lists of vectors (really verticies, or points)
 *  and polygons in 3-space.
 *  Intented for common handling of wireframe display information,
 *  in the full resolution that is calculated in.
 *
 *  On 32-bit machines, RT_VLIST_CHUNK of 35 results in rt_vlist structures
 *  just less than 1k bytes.
 *
 *  The head of the doubly linked list can be just a "struct bu_list" head.
 *
 *  To visit all the elements in the vlist:
 *	for( BU_LIST_FOR( vp, rt_vlist, hp ) )  {
 *		register int	i;
 *		register int	nused = vp->nused;
 *		register int	*cmd = vp->cmd;
 *		register point_t *pt = vp->pt;
 *		for( i = 0; i < nused; i++,cmd++,pt++ )  {
 *			access( *cmd, *pt );
 *			access( vp->cmd[i], vp->pt[i] );
 *		}
 *	}
 */
#define RT_VLIST_CHUNK	35		/* 32-bit mach => just less than 1k */
struct rt_vlist  {
	struct bu_list	l;			/* magic, forw, back */
	int		nused;			/* elements 0..nused active */
	int		cmd[RT_VLIST_CHUNK];	/* VL_CMD_* */
	point_t		pt[RT_VLIST_CHUNK];	/* associated 3-point/vect */
};
#define RT_VLIST_NULL	((struct rt_vlist *)0)
#define RT_VLIST_MAGIC	0x98237474
#define RT_CK_VLIST(_p) BU_CKMAG((_p), RT_VLIST_MAGIC, "rt_vlist")

/* Values for cmd[] */
#define RT_VLIST_LINE_MOVE	0
#define RT_VLIST_LINE_DRAW	1
#define RT_VLIST_POLY_START	2	/* pt[] has surface normal */
#define RT_VLIST_POLY_MOVE	3	/* move to first poly vertex */
#define RT_VLIST_POLY_DRAW	4	/* subsequent poly vertex */
#define RT_VLIST_POLY_END	5	/* last vert (repeats 1st), draw poly */
#define RT_VLIST_POLY_VERTNORM	6	/* per-vertex normal, for interpoloation */

/* XXX Note that RT_GET_VLIST and RT_FREE_VLIST are non-PARALLEL */
/*
 *  Applications that are going to use RT_ADD_VLIST and RT_GET_VLIST
 *  are required to execute this macro once, first:
 *		BU_LIST_INIT( &rt_g.rtg_vlfree );
 */
#define RT_GET_VLIST(p) {\
		(p) = BU_LIST_FIRST( rt_vlist, &rt_g.rtg_vlfree ); \
		if( BU_LIST_IS_HEAD( (p), &rt_g.rtg_vlfree ) )  { \
			(p) = (struct rt_vlist *)rt_malloc(sizeof(struct rt_vlist), "rt_vlist"); \
			(p)->l.magic = RT_VLIST_MAGIC; \
		} else { \
			BU_LIST_DEQUEUE( &((p)->l) ); \
		} \
		(p)->nused = 0; \
	}

/* Place an entire chain of rt_vlist structs on the global freelist */
#define RT_FREE_VLIST(hd)	{ \
	BU_CK_LIST_HEAD( (hd) ); \
	BU_LIST_APPEND_LIST( &rt_g.rtg_vlfree, (hd) ); \
	}

#define RT_ADD_VLIST(hd,pnt,draw)  { \
	register struct rt_vlist *_vp; \
	BU_CK_LIST_HEAD( hd ); \
	_vp = BU_LIST_LAST( rt_vlist, (hd) ); \
	if( BU_LIST_IS_HEAD( _vp, (hd) ) || _vp->nused >= RT_VLIST_CHUNK )  { \
		RT_GET_VLIST(_vp); \
		BU_LIST_INSERT( (hd), &(_vp->l) ); \
	} \
	VMOVE( _vp->pt[_vp->nused], (pnt) ); \
	_vp->cmd[_vp->nused++] = (draw); \
	}

/* Macro BU_CK_LIST_HEAD() is defined in rtlist.h */

/* For NMG plotting, a way of separating vlists into colorer parts */
struct rt_vlblock {
	long		magic;
	int		nused;
	int		max;
	long		*rgb;		/* rgb[max] */
	struct bu_list	*head;		/* head[max] */
};
#define RT_VLBLOCK_MAGIC	0x981bd112
#define RT_CK_VLBLOCK(_p)	BU_CKMAG((_p), RT_VLBLOCK_MAGIC, "rt_vlblock")
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
 *			R T _ F U N C T A B
 *
 *  Object-oriented interface to geometry modules.
 *  Table is indexed by ID_xxx value of particular solid.
 *  This needs to be at the end of the header file,
 *  so that all the structure names are known.
 *  XXX the "union record" and "struct nmgregion" pointers are problematic.
 *
 *  XXX On SGI, can not use identifiers in prototypes inside structure!
 */
struct rt_functab {
	char	*ft_name;
	int	ft_use_rpp;
	int	(*ft_prep) RT_ARGS((struct soltab * /*stp*/,
			struct rt_db_internal * /*ip*/,
			struct rt_i * /*rtip*/ ));
	int 	(*ft_shot) RT_ARGS((struct soltab * /*stp*/,
			struct xray * /*rp*/,
			struct application * /*ap*/,
			struct seg * /*seghead*/ ));
	void	(*ft_print) RT_ARGS((CONST struct soltab * /*stp*/));
	void	(*ft_norm) RT_ARGS((struct hit * /*hitp*/,
			struct soltab * /*stp*/,
			struct xray * /*rp*/));
	void	(*ft_uv) RT_ARGS((struct application * /*ap*/,
			struct soltab * /*stp*/,
			struct hit * /*hitp*/,
			struct uvcoord * /*uvp*/));
	void	(*ft_curve) RT_ARGS((struct curvature * /*cvp*/,
			struct hit * /*hitp*/,
			struct soltab * /*stp*/));
	int	(*ft_classify)();
	void	(*ft_free) RT_ARGS((struct soltab * /*stp*/));
	int	(*ft_plot) RT_ARGS((
			struct bu_list * /*vhead*/,
			struct rt_db_internal * /*ip*/,
			CONST struct rt_tess_tol * /*ttol*/,
			CONST struct bn_tol * /*tol*/));
	void	(*ft_vshot) RT_ARGS((struct soltab * /*stp*/[],
			struct xray *[] /*rp*/,
			struct seg [] /*segp*/, int /*n*/,
			struct application * /*ap*/ ));
#if defined(NMG_H)
	int	(*ft_tessellate) RT_ARGS((
			struct nmgregion ** /*r*/,
			struct model * /*m*/,
			struct rt_db_internal * /*ip*/,
			CONST struct rt_tess_tol * /*ttol*/,
			CONST struct bn_tol * /*tol*/));
	int	(*ft_tnurb) RT_ARGS((
			struct nmgregion ** /*r*/,
			struct model * /*m*/,
			struct rt_db_internal * /*ip*/,
			CONST struct bn_tol * /*tol*/));
#else
	int	(*ft_tessellate) RT_ARGS((
			genptr_t * /*r*/,
			genptr_t /*m*/,
			struct rt_db_internal * /*ip*/,
			CONST struct rt_tess_tol * /*ttol*/,
			CONST struct bn_tol * /*tol*/));
	int	(*ft_tnurb) RT_ARGS((
			genptr_t * /*r*/,
			genptr_t /*m*/,
			struct rt_db_internal * /*ip*/,
			CONST struct bn_tol * /*tol*/));
#endif
	int	(*ft_import) RT_ARGS((struct rt_db_internal * /*ip*/,
			CONST struct bu_external * /*ep*/,
			CONST mat_t /*mat*/));
	int	(*ft_export) RT_ARGS((struct bu_external * /*ep*/,
			CONST struct rt_db_internal * /*ip*/,
			double /*local2mm*/));
	void	(*ft_ifree) RT_ARGS((struct rt_db_internal * /*ip*/));
	int	(*ft_describe) RT_ARGS((struct bu_vls * /*str*/,
			struct rt_db_internal * /*ip*/,
			int /*verbose*/,
			double /*mm2local*/));
	int	(*ft_xform) RT_ARGS((struct rt_db_internal * /*op*/,
			CONST mat_t /*mat*/, struct rt_db_internal * /*ip*/,
			int /*free*/));
};
extern struct rt_functab rt_functab[];
extern int rt_nfunctab;


/*****************************************************************
 *                                                               *
 *          Applications interface to the RT library             *
 *                                                               *
 *****************************************************************/
					/* Read named MGED db, build toc */
RT_EXTERN(struct rt_i *rt_dirbuild, (char *filename, char *buf, int len) );
					/* Prepare for raytracing */
RT_EXTERN(struct rt_i *rt_new_rti, (struct db_i *dbip));
RT_EXTERN(void rt_prep, (struct rt_i *rtip) );
RT_EXTERN(void rt_prep_parallel, (struct rt_i *rtip, int ncpu) );
					/* Handle overlap w/o logging */
RT_EXTERN(int rt_overlap_quietly, (struct application *ap,
	struct partition *pp, struct region *reg1,
	struct region *reg2, struct partition *pheadp) );
					/* Shoot a ray */
RT_EXTERN(int rt_shootray, (struct application *ap) );
					/* Get expr tree for object */
RT_EXTERN(CONST char *	rt_basename, (CONST char *str));
RT_EXTERN(void rt_free_soltab, (struct soltab   *stp));
RT_EXTERN(int rt_gettree, (struct rt_i *rtip, CONST char *node) );
RT_EXTERN(int rt_gettrees, (struct rt_i	*rtip,
	int argc, CONST char **argv, int ncpus));
					/* Print seg struct */
RT_EXTERN(void rt_pr_seg, (CONST struct seg *segp) );
					/* Print the partitions */
RT_EXTERN(void rt_pr_partitions, (CONST struct rt_i *rtip,
	CONST struct partition *phead, CONST char *title) );
					/* Find solid by leaf name */
RT_EXTERN(struct soltab *rt_find_solid, (CONST struct rt_i *rtip,
	CONST char *name) );

RT_EXTERN(char *rt_read_cmd, (FILE *fp) );	/* Read semi-colon terminated line */
					/* do cmd from string via cmd table */
RT_EXTERN(int rt_do_cmd, (struct rt_i *rtip, char *lp, struct command_tab *ctp) );
					/* Start the timer */
RT_EXTERN(void rt_prep_timer, (void) );
					/* Read timer, return time + str */
RT_EXTERN(double rt_get_timer, (struct bu_vls *vp, double *elapsed));
					/* Return CPU time, text, & wall clock time */
RT_EXTERN(double rt_read_timer, (char *str, int len) );
					/* Plot a solid */
RT_EXTERN(int rt_plot_solid, (FILE *fp, struct rt_i *rtip, struct soltab *stp) );
					/* Release storage assoc with rt_i */
RT_EXTERN(void rt_clean, (struct rt_i *rtip) );
RT_EXTERN(void rt_add_res_stats, (struct rt_i *rtip, struct resource *resp) );
					/* Tally stats into struct rt_i */

/* The matrix math routines */

/* Routines from qmath.h */

/*****************************************************************
 *                                                               *
 *  Internal routines in the RT library.			 *
 *  These routines are *not* intended for Applications to use.	 *
 *  The interface to these routines may change significantly	 *
 *  from release to release of this software.			 *
 *                                                               *
 *****************************************************************/

					/* Weave segs into partitions */
RT_EXTERN(void rt_boolweave, (struct seg *out_hd, struct seg *in_hd,
	struct partition *PartHeadp, struct application *ap) );
					/* Eval booleans over partitions */
RT_EXTERN(int rt_boolfinal, (struct partition *InputHdp,
	struct partition *FinalHdp,
	fastf_t startdist, fastf_t enddist,
	struct bu_ptbl *regionbits, struct application *ap) );

RT_EXTERN(void rt_grow_boolstack, (struct resource *res) );
					/* Approx Floating compare */
RT_EXTERN(int rt_fdiff, (double a, double b) );
					/* Relative Difference */
RT_EXTERN(double rt_reldiff, (double a, double b) );
					/* Print a soltab */
RT_EXTERN(void rt_pr_soltab, (CONST struct soltab *stp) );
					/* Print a region */
RT_EXTERN(void rt_pr_region, (CONST struct region *rp) );
					/* Print an expr tree */
RT_EXTERN(void rt_pr_tree, (CONST union tree *tp, int lvl) );
					/* Print value of tree for a partition */
RT_EXTERN(void rt_pr_tree_val, (CONST union tree *tp,
	CONST struct partition *partp, int pr_name, int lvl));
					/* Print a partition */
RT_EXTERN(void rt_pr_pt, (CONST struct rt_i *rtip, CONST struct partition *pp) );
					/* Print a bit vector */
RT_EXTERN(void rt_pr_hit, (CONST char *str, CONST struct hit *hitp) );
/* rt_fastf_float, rt_mat_dbmat, rt_dbmat_mat
 * declarations moved to h/db.h */
					/* storage obtainers */
RT_EXTERN(void rt_get_seg, (struct resource *res) );
RT_EXTERN(void rt_cut_it, (struct rt_i *rtip, int ncpu) );
					/* print cut node */
RT_EXTERN(void rt_pr_cut, (CONST union cutter *cutp, int lvl) );
					/* free a cut tree */
RT_EXTERN(void rt_fr_cut, (struct rt_i *rtip, union cutter *cutp) );
					/* regionid-driven color override */
RT_EXTERN(void rt_region_color_map, (struct region *regp) );
					/* process ID_MATERIAL record */
RT_EXTERN(void rt_color_addrec, () );
					/* extend a cut box */
RT_EXTERN(void rt_cut_extend, (union cutter *cutp, struct soltab *stp) );
					/* find RPP of one region */
RT_EXTERN(int rt_rpp_region, (struct rt_i *rtip, CONST char *reg_name,
	fastf_t *min_rpp, fastf_t *max_rpp) );

/* The database library */

/* db_anim.c */
RT_EXTERN(int db_add_anim, (struct db_i *dbip, struct animate *anp, int root) );
RT_EXTERN(int db_do_anim, (struct animate *anp, mat_t stack, mat_t arc,
	struct mater_info *materp) );
RT_EXTERN(void db_free_anim, (struct db_i *dbip) );
RT_EXTERN(void db_write_anim, (FILE *fop, struct animate *anp));

/* db_path.c */
RT_EXTERN(void db_add_node_to_full_path, (struct db_full_path *pp,
	struct directory *dp) );
RT_EXTERN(void db_dup_full_path, (struct db_full_path *newp,
	CONST struct db_full_path *oldp) );
RT_EXTERN(char *db_path_to_string, (CONST struct db_full_path *pp) );
RT_EXTERN(void db_free_full_path, (struct db_full_path *pp) );
RT_EXTERN(void db_region_mat, (mat_t m, CONST struct db_i *dbip,
				CONST char *name) );
RT_EXTERN(void db_shader_mat, (mat_t model_to_shader, CONST struct rt_i *rtip,
				CONST struct region *rp, point_t p_min,
				point_t p_max) );
/* db_open.c */
					/* open an existing model database */
RT_EXTERN(struct db_i *db_open, ( char *name, char *mode ) );
					/* create a new model database */
RT_EXTERN(struct db_i *db_create, ( char *name ) );
					/* close a model database */
RT_EXTERN(void db_close, ( struct db_i *dbip ) );
/* db_io.c */
/* It is normal to test for __STDC__ when using *_DEFINED tests but in
 * in this case "union record" is used for db_getmrec's return type.  This
 * requires that the "union_record *db_getmrec" be used whenever 
 * RECORD_DEFINED is defined.
 */
#if defined(RECORD_DEFINED)
					/* malloc & read records */
RT_EXTERN(union record *db_getmrec, ( CONST struct db_i *, CONST struct directory *dp ) );
					/* get several records from db */
RT_EXTERN(int db_get, (CONST struct db_i *, CONST struct directory *dp,
	union record *where, int offset, int len ) );
					/* put several records into db */
RT_EXTERN(int db_put, ( CONST struct db_i *, CONST struct directory *dp, union record *where,
	int offset, int len ) );
#else /* RECORD_DEFINED */
					/* malloc & read records */
RT_EXTERN(genptr_t db_getmrec, ( CONST struct db_i *, CONST struct directory *dp ) );
					/* get several records from db */
RT_EXTERN(int db_get, (CONST struct db_i *, CONST struct directory *dp,
	genptr_t where, int offset, int len ) );
					/* put several records into db */
RT_EXTERN(int db_put, ( CONST struct db_i *, CONST struct directory *dp,
	genptr_t where, int offset, int len ) );
#endif /* RECORD_DEFINED */
RT_EXTERN(int db_get_external, ( struct bu_external *ep,
	CONST struct directory *dp, CONST struct db_i *dbip ) );
RT_EXTERN(int db_put_external, ( struct bu_external *ep,
	struct directory *dp, struct db_i *dbip ) );
RT_EXTERN(void db_free_external, ( struct bu_external *ep ) );

/* db_scan.c */
					/* read db (to build directory) */
RT_EXTERN(int db_scan, ( struct db_i *, int (*handler)(), int do_old_matter ) );
					/* update db unit conversions */
RT_EXTERN(void db_conversions, ( struct db_i *, int units ) );

/* db_lookup.c */
RT_EXTERN(int db_dirhash, (CONST char *str) );
					/* convert name to directory ptr */
RT_EXTERN(struct directory *db_lookup,( struct db_i *, CONST char *name, int noisy ) );
					/* add entry to directory */
RT_EXTERN(struct directory *db_diradd, ( struct db_i *, CONST char *name, long laddr,
	int len, int flags ) );
					/* delete entry from directory */
RT_EXTERN(int db_dirdelete, ( struct db_i *, struct directory *dp ) );
RT_EXTERN(int db_rename, ( struct db_i *, struct directory *, CONST char *newname) );
RT_EXTERN(void db_pr_dir, ( CONST struct db_i *dbip ) );

/* db_alloc.c */
					/* allocate "count" granules */
RT_EXTERN(int db_alloc, ( struct db_i *, struct directory *dp, int count ) );
					/* grow by "count" granules */
RT_EXTERN(int db_grow, ( struct db_i *, struct directory *dp, int count ) );
					/* truncate by "count" */
RT_EXTERN(int db_trunc, ( struct db_i *, struct directory *dp, int count ) );
					/* delete "recnum" from entry */
RT_EXTERN(int db_delrec, ( struct db_i *, struct directory *dp, int recnum ) );
					/* delete all granules assigned dp */
RT_EXTERN(int db_delete, ( struct db_i *, struct directory *dp ) );
					/* write FREE records from 'start' */
RT_EXTERN(int db_zapper, ( struct db_i *, struct directory *dp, int start ) );

/* db_tree.c */
RT_EXTERN(struct combined_tree_state *db_new_combined_tree_state,
	(CONST struct db_tree_state *tsp, CONST struct db_full_path *pathp));
RT_EXTERN(struct combined_tree_state *db_dup_combined_tree_state,
	(CONST struct combined_tree_state *old));
RT_EXTERN(void db_free_combined_tree_state,
	(struct combined_tree_state *ctsp));
RT_EXTERN(void db_pr_tree_state, (CONST struct db_tree_state *tsp));
RT_EXTERN(void db_pr_combined_tree_state,
	(CONST struct combined_tree_state *ctsp));
RT_EXTERN(int db_apply_state_from_comb, (struct db_tree_state *tsp,
	CONST struct db_full_path *pathp, CONST struct rt_comb_internal *comb));
RT_EXTERN(int db_apply_state_from_memb, (struct db_tree_state *tsp,
	struct db_full_path *pathp, CONST union tree *tp));
RT_EXTERN(int db_follow_path_for_state, (struct db_tree_state *tsp,
	struct db_full_path *pathp, CONST char *orig_str, int noisy));
RT_EXTERN(union tree *db_recurse, (struct db_tree_state	*tsp,
	struct db_full_path *pathp,
	struct combined_tree_state **region_start_statepp));
RT_EXTERN(union tree *db_dup_subtree, (CONST union tree	*tp));
RT_EXTERN(void db_free_tree, (union tree *tp));
RT_EXTERN(void db_non_union_push, (union tree *tp));
RT_EXTERN(int db_count_subtree_regions, (CONST union tree *tp));
RT_EXTERN(int db_tally_subtree_regions, (union tree *tp,
	union tree **reg_trees, int cur, int lim));
RT_EXTERN(int db_walk_tree, (struct db_i *dbip, int argc, CONST char **argv,
	int ncpu, CONST struct db_tree_state *init_state,
	int (*reg_start_func)(), union tree * (*reg_end_func)(),
	union tree * (*leaf_func)() ));
RT_EXTERN(int db_path_to_mat, (struct db_i *dbip, struct db_full_path *pathp,
	mat_t mat, int depth));
RT_EXTERN(void db_apply_anims, (struct db_full_path *pathp,
	struct directory *dp, mat_t stck, mat_t arc,
	struct mater_info *materp));

/* db_comb.c */


/* memalloc.c -- non PARALLEL routines */
RT_EXTERN(unsigned long rt_memalloc, (struct mem_map **pp, unsigned size) );
RT_EXTERN(unsigned long rt_memget, (struct mem_map **pp, unsigned int size,
	unsigned int place) );
RT_EXTERN(void rt_memfree, (struct mem_map **pp, unsigned size, unsigned long addr) );
RT_EXTERN(void rt_mempurge, (struct mem_map **pp) );
RT_EXTERN(void rt_memprint, (struct mem_map **pp) );
RT_EXTERN(void rt_memclose,() );

RT_EXTERN(struct rt_vlblock *rt_vlblock_init, () );
RT_EXTERN(void rt_vlblock_free, (struct rt_vlblock *vbp) );
RT_EXTERN(struct bu_list *rt_vlblock_find, (struct rt_vlblock *vbp,
	int r, int g, int b) );

/* plane.c */
 
/* bn_cx_div, CxSqrt */
extern void bn_pr_roots();		/* print complex roots */

/* rtassoc.c */
RT_EXTERN(struct bu_vls *rt_assoc, (char *fname, char *value, int field_sep));

/* pr.c */
RT_EXTERN(void rt_pr_tree_vls, (struct bu_vls *vls, CONST union tree *tp));
RT_EXTERN(void rt_pr_hit_vls, (struct bu_vls *v, CONST char *str,
	CONST struct hit *hitp));
RT_EXTERN(void rt_pr_pt_vls, (struct bu_vls *v, CONST struct rt_i *rtip,
	CONST struct partition *pp));
RT_EXTERN(void bu_logindent_vls, (struct bu_vls	*v));
RT_EXTERN(void rt_pr_fallback_angle, (struct bu_vls *str, CONST char *prefix,
	CONST double angles[5]));
RT_EXTERN(void rt_find_fallback_angle, (double angles[5], CONST vect_t vec));

/* table.c */
RT_EXTERN(int rt_id_solid, (struct bu_external *ep));

/* units.c */
RT_EXTERN(double rt_units_conversion, (CONST char *str) );
RT_EXTERN(CONST char *rt_units_string, (CONST double mm) );
RT_EXTERN(double rt_mm_value, (CONST char *s) );

/* prep.c */
RT_EXTERN(void rt_plot_all_bboxes, (FILE *fp, struct rt_i *rtip));
RT_EXTERN(void rt_plot_all_solids, (FILE *fp, struct rt_i *rtip));

/* vlist.c */
RT_EXTERN(struct rt_vlblock *	rt_vlblock_init, () );
RT_EXTERN(void			rt_vlblock_free, (struct rt_vlblock *vbp) );
RT_EXTERN(struct bu_list *	rt_vlblock_find, (struct rt_vlblock *vbp,
				int r, int g, int b) );
RT_EXTERN(void			rt_vlist_cleanup, () );
RT_EXTERN(void			rt_vlist_export, (struct bu_vls *vls,
				struct bu_list *hp,
				CONST char *name));
RT_EXTERN(void			rt_vlist_import, (struct bu_list *hp,
				struct bu_vls *namevls,
				CONST unsigned char *buf));
RT_EXTERN(void			rt_plot_vlblock, (FILE *fp,
				CONST struct rt_vlblock	*vbp) );
RT_EXTERN(void			rt_vlist_to_uplot, (FILE *fp,
				CONST struct bu_list *vhead));
RT_EXTERN(int			rt_uplot_to_vlist, (struct rt_vlblock *vbp,
				FILE *fp, double char_size) );
RT_EXTERN(void			rt_label_vlist_verts, (struct rt_vlblock *vbp,
				struct bu_list *src, mat_t mat,
				double sz, double mm2local) );


/************************************************************************
 *									*
 *			N M G Support Function Declarations		*
 *									*
 ************************************************************************/
#if defined(NMG_H)

/* From file nmg_mk.c */
/*	MAKE routines */
RT_EXTERN(struct model		*nmg_mm, () );
RT_EXTERN(struct model		*nmg_mmr, () );
RT_EXTERN(struct nmgregion	*nmg_mrsv, (struct model *m) );
RT_EXTERN(struct shell 		*nmg_msv, (struct nmgregion *r_p) );
RT_EXTERN(struct faceuse	*nmg_mf, (struct loopuse *lu1) );
RT_EXTERN(struct loopuse	*nmg_mlv, (long *magic, struct vertex *v, int orientation) );
RT_EXTERN(struct edgeuse	*nmg_me, (struct vertex *v1, struct vertex *v2, struct shell *s) );
RT_EXTERN(struct edgeuse	*nmg_meonvu, (struct vertexuse *vu) );
RT_EXTERN(struct loopuse	*nmg_ml, (struct shell *s) );
/*	KILL routines */
RT_EXTERN(int			nmg_kvu, (struct vertexuse *vu) );
RT_EXTERN(int			nmg_kfu, (struct faceuse *fu1) );
RT_EXTERN(int			nmg_klu, (struct loopuse *lu1) );
RT_EXTERN(int			nmg_keu, (struct edgeuse *eu) );
RT_EXTERN(int			nmg_ks, (struct shell *s) );
RT_EXTERN(int			nmg_kr, (struct nmgregion *r) );
RT_EXTERN(void			nmg_km, (struct model *m) );
/*	Geometry and Attribute routines */
RT_EXTERN(void			nmg_vertex_gv, (struct vertex *v, CONST point_t pt) );
RT_EXTERN(void			nmg_vertex_g, (struct vertex *v, fastf_t x, fastf_t y, fastf_t z) );
RT_EXTERN(void			nmg_edge_g, (struct edgeuse *eu) );
RT_EXTERN(int			nmg_use_edge_g, (struct edgeuse *eu, long *eg) );
RT_EXTERN(void			nmg_loop_g, (struct loop *l, CONST struct bn_tol *tol) );
RT_EXTERN(void			nmg_face_g, (struct faceuse *fu, CONST plane_t p) );
RT_EXTERN(void			nmg_face_bb, (struct face *f, CONST struct bn_tol *tol) );
RT_EXTERN(void			nmg_shell_a, (struct shell *s, CONST struct bn_tol *tol) );
RT_EXTERN(void			nmg_region_a, (struct nmgregion *r, CONST struct bn_tol *tol) );
/*	DEMOTE routines */
RT_EXTERN(int			nmg_demote_lu, (struct loopuse *lu) );
RT_EXTERN(int			nmg_demote_eu, (struct edgeuse *eu) );
/*	MODIFY routines */
RT_EXTERN(void			nmg_movevu, (struct vertexuse *vu, struct vertex *v) );
#define nmg_moveeu(a,b)		nmg_je(a,b)
RT_EXTERN(void			nmg_je, (struct edgeuse *eudst, struct edgeuse *eusrc) );
RT_EXTERN(void			nmg_unglueedge, (struct edgeuse *eu) );
RT_EXTERN(void			nmg_jv, (struct vertex *v1, struct vertex *v2) );
RT_EXTERN(void			nmg_jeg, (struct edge_g_lseg *dest_eg,
				struct edge_g_lseg *src_eg) );

/* From nmg_mod.c */
/*	SHELL Routines */
RT_EXTERN(int			nmg_simplify_shell, (struct shell *s) );
RT_EXTERN(void			nmg_rm_redundancies, (struct shell *s, CONST struct bn_tol *tol ) );
RT_EXTERN(void			nmg_sanitize_s_lv, (struct shell *s,
				int orient) );
RT_EXTERN(void			nmg_s_split_touchingloops, (struct shell *s,
				CONST struct bn_tol *tol) );
/*	FACE Routines */
RT_EXTERN(struct faceuse	*nmg_cmface, (struct shell *s, struct vertex **vt[], int n) );
RT_EXTERN(struct faceuse	*nmg_cface, (struct shell *s, struct vertex **vt,	int n) );
RT_EXTERN(struct faceuse	*nmg_add_loop_to_face, (struct shell *s, struct faceuse *fu, struct vertex **verts, int n, int dir) );
RT_EXTERN(int			nmg_fu_planeeqn, (struct faceuse *fu, CONST struct bn_tol *tol) );
RT_EXTERN(void			nmg_gluefaces, (struct faceuse *fulist[], int n, CONST struct bn_tol *tol) );
RT_EXTERN(int			nmg_simplify_face, (struct faceuse *fu) );
RT_EXTERN(void			nmg_reverse_face, (struct faceuse *fu) );
RT_EXTERN(void			nmg_mv_fu_between_shells, (struct shell *dest,
				struct shell *src, struct faceuse *fu) );
RT_EXTERN(void			nmg_jf, (struct faceuse *dest_fu,
				struct faceuse *src_fu) );
RT_EXTERN(struct faceuse	*nmg_dup_face, (struct faceuse *fu, struct shell *s) );
/*	LOOP Routines */
RT_EXTERN(void			nmg_jl, (struct loopuse *lu, struct edgeuse *eu) );
RT_EXTERN(struct vertexuse	*nmg_join_2loops, (struct vertexuse *vu1, struct vertexuse *vu2) );
RT_EXTERN(struct vertexuse	*nmg_join_singvu_loop, (struct vertexuse *vu1, struct vertexuse *vu2) );
RT_EXTERN(struct vertexuse	*nmg_join_2singvu_loops, (struct vertexuse *vu1, struct vertexuse *vu2) );
RT_EXTERN(struct loopuse	*nmg_cut_loop, (struct vertexuse *vu1, struct vertexuse *vu2) );
RT_EXTERN(struct loopuse	*nmg_split_lu_at_vu, (struct loopuse *lu, struct vertexuse *vu) );
RT_EXTERN(void			nmg_split_touchingloops, (struct loopuse *lu,
				CONST struct bn_tol *tol) );
RT_EXTERN(int			nmg_join_touchingloops, (struct loopuse *lu) );
RT_EXTERN(void			nmg_simplify_loop, (struct loopuse *lu) );
RT_EXTERN(int			nmg_kill_snakes, (struct loopuse *lu) );
RT_EXTERN(void			nmg_mv_lu_between_shells, (struct shell *dest,
				struct shell *src, struct loopuse *lu) );
RT_EXTERN(void	 		nmg_moveltof, (struct faceuse *fu, struct shell *s) );
RT_EXTERN(struct loopuse	*nmg_dup_loop, (struct loopuse *lu,
				long *parent, long **trans_tbl) );
RT_EXTERN(void			nmg_set_lu_orientation, (struct loopuse *lu, int is_opposite) );
RT_EXTERN(void			nmg_lu_reorient, (struct loopuse *lu ) );
/*	EDGE Routines */
RT_EXTERN(struct edgeuse	*nmg_eusplit, (struct vertex *v, struct edgeuse *oldeu, int share_geom) );
RT_EXTERN(struct edgeuse	*nmg_esplit, (struct vertex *v, struct edgeuse *eu, int share_geom) );
RT_EXTERN(struct edgeuse	*nmg_ebreak, (struct vertex *v, struct edgeuse *eu));
RT_EXTERN(struct edgeuse	*nmg_ebreaker, (struct vertex *v,
				struct edgeuse *eu, CONST struct bn_tol *tol));
RT_EXTERN(struct vertex		*nmg_e2break, (struct edgeuse *eu1, struct edgeuse *eu2) );
RT_EXTERN(struct edgeuse	*nmg_eins, (struct edgeuse *eu) );
RT_EXTERN(void			nmg_mv_eu_between_shells, (struct shell *dest,
				struct shell *src, struct edgeuse *eu) );
/*	VERTEX Routines */
RT_EXTERN(void			nmg_mv_vu_between_shells, (struct shell *dest,
				struct shell *src, struct vertexuse *vu) );

/* From nmg_info.c */
	/* Model routines */
RT_EXTERN(struct model		*nmg_find_model, (CONST long *magic_p) );
RT_EXTERN(void			nmg_model_bb, (point_t min_pt, point_t max_pt, CONST struct model *m) );


	/* Shell routines */
RT_EXTERN(int			nmg_shell_is_empty, (CONST struct shell *s) );
RT_EXTERN(struct shell		*nmg_find_s_of_lu, (CONST struct loopuse *lu) );
RT_EXTERN(struct shell		*nmg_find_s_of_eu, (CONST struct edgeuse *eu) );
RT_EXTERN(struct shell		*nmg_find_s_of_vu, (CONST struct vertexuse *vu) );

	/* Face routines */
RT_EXTERN(struct faceuse	*nmg_find_fu_of_eu, (CONST struct edgeuse *eu));
RT_EXTERN(struct faceuse	*nmg_find_fu_of_lu, (CONST struct loopuse *lu));
RT_EXTERN(struct faceuse	*nmg_find_fu_of_vu, (CONST struct vertexuse *vu) );
RT_EXTERN(struct faceuse	*nmg_find_fu_with_fg_in_s, (CONST struct shell *s1,
				CONST struct faceuse *fu2));
RT_EXTERN(double		nmg_measure_fu_angle, (CONST struct edgeuse *eu,
				CONST vect_t xvec, CONST vect_t yvec,
				CONST vect_t zvec) );

	/* Loop routines */
RT_EXTERN(struct loopuse	*nmg_find_lu_of_vu, (CONST struct vertexuse *vu) );
RT_EXTERN(int			nmg_loop_is_a_crack, (CONST struct loopuse *lu) );
RT_EXTERN(int			nmg_loop_is_ccw, (CONST struct loopuse *lu,
				CONST plane_t norm, CONST struct bn_tol *tol) );
RT_EXTERN(CONST struct vertexuse *nmg_loop_touches_self, (CONST struct loopuse *lu));
RT_EXTERN(int			nmg_2lu_identical, (CONST struct edgeuse *eu1,
				CONST struct edgeuse *eu2));

	/* Edge routines */
RT_EXTERN(struct edgeuse	*nmg_findeu, (CONST struct vertex *v1, CONST struct vertex *v2,
				CONST struct shell *s, CONST struct edgeuse *eup,
				int dangling_only) );
RT_EXTERN(struct edgeuse	*nmg_find_eu_in_face, (CONST struct vertex *v1,
				CONST struct vertex *v2, CONST struct faceuse *fu,
				CONST struct edgeuse *eup, int dangling_only));
RT_EXTERN(struct edgeuse	*nmg_find_eu_of_vu, (CONST struct vertexuse *vu) );
RT_EXTERN(struct edgeuse	*nmg_find_eu_with_vu_in_lu, (CONST struct loopuse *lu,
				CONST struct vertexuse *vu) );
RT_EXTERN(CONST struct edgeuse	*nmg_faceradial, (CONST struct edgeuse *eu) );
RT_EXTERN(CONST struct edgeuse	*nmg_radial_face_edge_in_shell, (CONST struct edgeuse *eu) );
RT_EXTERN(CONST struct edgeuse *nmg_find_edge_between_2fu, (CONST struct faceuse *fu1,
				CONST struct faceuse *fu2, CONST struct bn_tol *tol));
RT_EXTERN(struct edge		*nmg_find_e_nearest_pt2, (long *magic_p,
				CONST point_t pt2, CONST mat_t mat,
				CONST struct bn_tol *tol) );
RT_EXTERN(struct edgeuse	*nmg_find_matching_eu_in_s, (
				CONST struct edgeuse *eu1, CONST struct shell *s2));
RT_EXTERN(void			nmg_eu_2vecs_perp, (vect_t xvec, vect_t yvec,
				vect_t zvec, CONST struct edgeuse *eu,
				CONST struct bn_tol *tol) );
RT_EXTERN(int			nmg_find_eu_leftvec, (vect_t left,
				CONST struct edgeuse *eu) );

	/* Vertex routines */
RT_EXTERN(struct vertexuse	*nmg_find_v_in_face, (CONST struct vertex *,
				CONST struct faceuse *) );
RT_EXTERN(struct vertexuse	*nmg_find_v_in_shell, (CONST struct vertex *v,
				CONST struct shell *s, int edges_only));
RT_EXTERN(struct vertexuse	*nmg_find_pt_in_lu, (CONST struct loopuse *lu,
				CONST point_t pt, CONST struct bn_tol *tol));
RT_EXTERN(struct vertexuse	*nmg_find_pt_in_face, (CONST struct faceuse *fu,
				CONST point_t pt,
				CONST struct bn_tol *tol) );
RT_EXTERN(struct vertex		*nmg_find_pt_in_shell, (CONST struct shell *s,
				CONST point_t pt, CONST struct bn_tol *tol) );
RT_EXTERN(struct vertex		*nmg_find_pt_in_model, (CONST struct model *m,
				CONST point_t pt, CONST struct bn_tol *tol));
RT_EXTERN(int			nmg_is_vertex_in_edgelist, (CONST struct vertex *v,
				CONST struct bu_list *hd) );
RT_EXTERN(int			nmg_is_vertex_in_looplist, (CONST struct vertex *v,
				CONST struct bu_list *hd, int singletons) );
RT_EXTERN(int			nmg_is_vertex_a_selfloop_in_shell, (CONST struct vertex *v,
				CONST struct shell *s) );
RT_EXTERN(int			nmg_is_vertex_in_facelist, (CONST struct vertex *v,
				CONST struct bu_list *hd) );
RT_EXTERN(int			nmg_is_edge_in_edgelist, (CONST struct edge *e,
				CONST struct bu_list *hd) );
RT_EXTERN(int			nmg_is_edge_in_looplist, (CONST struct edge *e,
				CONST struct bu_list *hd) );
RT_EXTERN(int			nmg_is_edge_in_facelist, (CONST struct edge *e,
				CONST struct bu_list *hd) );
RT_EXTERN(int			nmg_is_loop_in_facelist, (CONST struct loop *l,
				CONST struct bu_list *fu_hd) );

RT_EXTERN(void			nmg_edgeuse_tabulate, (struct bu_ptbl *tab,
				CONST long *magic_p));
RT_EXTERN(void			nmg_vertex_tabulate, (struct bu_ptbl *tab,
				CONST long *magic_p));
RT_EXTERN(void			nmg_face_tabulate, (struct bu_ptbl *tab,
				CONST long *magic_p));

/* From nmg_pr.c */
RT_EXTERN(char *		nmg_orientation, (int orientation) );
RT_EXTERN(void			nmg_pr_orient, (int orientation, CONST char *h) );
RT_EXTERN(void			nmg_pr_m, (CONST struct model *m) );
RT_EXTERN(void			nmg_pr_r, (CONST struct nmgregion *r, char *h) );
RT_EXTERN(void			nmg_pr_sa, (CONST struct shell_a *sa, char *h) );
RT_EXTERN(void			nmg_pr_lg, (CONST struct loop_g *lg, char *h) );
RT_EXTERN(void			nmg_pr_fg, (CONST long *magic, char *h) );
RT_EXTERN(void			nmg_pr_s, (CONST struct shell *s, char *h) );
RT_EXTERN(void			nmg_pr_f, (CONST struct face *f, char *h) );
RT_EXTERN(void			nmg_pr_fu, (CONST struct faceuse *fu, char *h) );
RT_EXTERN(void			nmg_pr_fu_briefly, (CONST struct faceuse *fu,	char *h) );
RT_EXTERN(void			nmg_pr_l, (CONST struct loop *l, char *h) );
RT_EXTERN(void			nmg_pr_lu, (CONST struct loopuse *lu, char *h) );
RT_EXTERN(void			nmg_pr_lu_briefly, (CONST struct loopuse *lu, char *h) );
RT_EXTERN(void			nmg_pr_eg, (CONST long *eg, char *h) );
RT_EXTERN(void			nmg_pr_e, (CONST struct edge *e, char *h) );
RT_EXTERN(void			nmg_pr_eu, (CONST struct edgeuse *eu, char *h) );
RT_EXTERN(void			nmg_pr_eu_briefly, (CONST struct edgeuse *eu, char *h) );
RT_EXTERN(void			nmg_pr_vg, (CONST struct vertex_g *vg, char *h) );
RT_EXTERN(void			nmg_pr_v, (CONST struct vertex *v, char *h) );
RT_EXTERN(void			nmg_pr_vu, (CONST struct vertexuse *vu, char *h) );
RT_EXTERN(void			nmg_pr_vu_briefly, (CONST struct vertexuse *vu, char *h) );
RT_EXTERN(void			nmg_pr_vua, (CONST long *magic_p, char *h) );
RT_EXTERN(void			nmg_euprint, (CONST char *str, CONST struct edgeuse *eu) );

/* From nmg_misc.c */
RT_EXTERN(int			bu_ptbl, (struct bu_ptbl *b, int func, long *p) );
RT_EXTERN(void			nmg_purge_unwanted_intersection_points, (struct bu_ptbl *vert_list, fastf_t *mag, CONST struct faceuse *fu, CONST struct bn_tol *tol));
RT_EXTERN(int			nmg_in_or_ref, (struct vertexuse *vu, struct bu_ptbl *b) );
RT_EXTERN(void			nmg_rebound, (struct model *m, CONST struct bn_tol *tol) );
RT_EXTERN(void			nmg_count_shell_kids, (CONST struct model *m, unsigned long *total_wires, unsigned long *total_faces, unsigned long *total_points));
RT_EXTERN(void			nmg_stash_model_to_file, (CONST char *filename,
				CONST struct model *m, CONST char *title) );

/* From nmg_tri.c */
RT_EXTERN(void			nmg_triangulate_model, (struct model *m, CONST struct bn_tol   *tol) );
RT_EXTERN(void			nmg_triangulate_fu, (struct faceuse *fu, CONST struct bn_tol   *tol) );

/* nmg_manif.c */
RT_EXTERN(int			nmg_dangling_face, (CONST struct faceuse *fu,
				CONST char *manifolds));
/* static paint_face */
/* static set_edge_sub_manifold */
/* static set_loop_sub_manifold */
/* static set_face_sub_manifold */
RT_EXTERN(char 			*nmg_shell_manifolds, (struct shell *sp, char *tbl) );
RT_EXTERN(char	 		*nmg_manifolds, (struct model *m) );

/* g_nmg.c */

/* nmg_class.c */
RT_EXTERN(int			nmg_class_pt_f, (CONST point_t pt,
				CONST struct faceuse *fu,
				CONST struct bn_tol *tol) );
RT_EXTERN(int			nmg_class_pt_s, (CONST point_t pt,
				CONST struct shell *s,
				CONST int in_or_out_only,
				CONST struct bn_tol *tol) );

/* From nmg_pt_fu.c */
RT_EXTERN(int			nmg_class_pt_fu_except, (CONST point_t pt,
				CONST struct faceuse *fu,
				CONST struct loopuse *ignore_lu,
				void (*eu_func)(), void (*vu_func)(),
				CONST char *priv,
				CONST int call_on_hits,
				CONST int in_or_out_only,
				CONST struct bn_tol *tol) );

/* From nmg_plot.c */
/* add nmg_xxx_to_vlist routines here */
RT_EXTERN(void			nmg_pl_v, (FILE	*fp, CONST struct vertex *v,
				long *b) );
RT_EXTERN(void			nmg_pl_e, (FILE *fp, CONST struct edge *e,
				long *b, int red, int green, int blue) );
RT_EXTERN(void			nmg_pl_eu, (FILE *fp, CONST struct edgeuse *eu,
				long *b, int red, int green, int blue) );
RT_EXTERN(void			nmg_pl_lu, (FILE *fp, CONST struct loopuse *fu, 
				long *b, int red, int green, int blue) );
RT_EXTERN(void			nmg_pl_fu, (FILE *fp, CONST struct faceuse *fu,
				long *b, int red, int green, int blue ) );
RT_EXTERN(void			nmg_pl_s, (FILE *fp, CONST struct shell *s) );
RT_EXTERN(void			nmg_pl_r, (FILE *fp, CONST struct nmgregion *r) );
RT_EXTERN(void			nmg_pl_m, (FILE *fp, CONST struct model *m) );
RT_EXTERN(void			nmg_vlblock_v, (struct rt_vlblock *vbp,
				CONST struct vertex *v, long *tab) );
RT_EXTERN(void			nmg_vlblock_e, (struct rt_vlblock *vbp,
				CONST struct edge *e, long *tab,
				int red, int green, int blue, int fancy) );
RT_EXTERN(void			nmg_vlblock_eu, (struct rt_vlblock *vbp,
				CONST struct edgeuse *eu, long *tab,
				int red, int green, int blue,
				int fancy, int loopnum) );
RT_EXTERN(void			nmg_vlblock_lu, (struct rt_vlblock *vbp,
				CONST struct loopuse *lu, long *tab,
				int red, int green, int blue,
				int fancy, int loopnum) );
RT_EXTERN(void			nmg_vlblock_fu, (struct rt_vlblock *vbp,
				CONST struct faceuse *fu, long *tab, int fancy) );
RT_EXTERN(void			nmg_vlblock_s, (struct rt_vlblock *vbp,
				CONST struct shell *s, int fancy) );
RT_EXTERN(void			nmg_vlblock_r, (struct rt_vlblock *vbp,
				CONST struct nmgregion *r, int fancy) );
RT_EXTERN(void			nmg_vlblock_m, (struct rt_vlblock *vbp,
				CONST struct model *m, int fancy) );
RT_EXTERN(void			nmg_pl_around_edge, (FILE *fd,
				long *b, CONST struct edgeuse *eu) );
RT_EXTERN(void			nmg_pl_isect, (CONST char *filename,
				CONST struct shell *s, CONST struct bn_tol *tol) );
RT_EXTERN(void			nmg_pl_comb_fu, (int num1, int num2,
				CONST struct faceuse *fu1) );
RT_EXTERN(void			nmg_pl_2fu, (CONST char *str, int num,
				CONST struct faceuse *fu1, CONST struct faceuse *fu2,
				int show_mates) );
RT_EXTERN(void			nmg_face_plot, (CONST struct faceuse *fu) );
RT_EXTERN(void			nmg_2face_plot, (CONST struct faceuse *fu1,
				CONST struct faceuse *fu2) );
RT_EXTERN(void			nmg_face_lu_plot, (CONST struct loopuse *lu,
				CONST struct vertexuse *vu1, CONST struct vertexuse *vu2) );
RT_EXTERN(void			nmg_cnurb_to_vlist, (struct bu_list *vhead,
				CONST struct edgeuse *eu,int n_interior,
				int cmd) );


/* from nmg_mesh.c */
RT_EXTERN(void			nmg_radial_join_eu, (struct edgeuse *eu1,
				struct edgeuse *eu2, CONST struct bn_tol *tol));
RT_EXTERN(void			nmg_mesh_faces, (struct faceuse *fu1,
				struct faceuse *fu2, CONST struct bn_tol *tol) );
RT_EXTERN(int			nmg_mesh_face_shell, (struct faceuse *fu1,
				struct shell *s, CONST struct bn_tol *tol));
RT_EXTERN(int			nmg_mesh_shell_shell, (struct shell *s1,
				struct shell *s2, CONST struct bn_tol *tol));
RT_EXTERN(double		nmg_measure_fu_angle, (CONST struct edgeuse *eu,
				CONST vect_t xvec, CONST vect_t yvec,
				CONST vect_t zvec));

/* from nmg_bool.c */
RT_EXTERN(struct nmgregion	*nmg_do_bool, (struct nmgregion *s1,
				struct nmgregion *s2,
				CONST int oper, CONST struct bn_tol *tol) );
RT_EXTERN(void			nmg_shell_coplanar_face_merge,
				(struct shell *s, CONST struct bn_tol *tol,
				CONST int simplify) );
RT_EXTERN(int			nmg_two_region_vertex_fuse, (struct nmgregion *r1,
				struct nmgregion *r2, CONST struct bn_tol *tol));
RT_EXTERN(union tree		*nmg_booltree_leaf_tess, (struct db_tree_state *tsp,
				struct db_full_path *pathp,
				struct bu_external *ep, int id));
RT_EXTERN(union tree		*nmg_booltree_leaf_tnurb, (struct db_tree_state *tsp,
				struct db_full_path *pathp,
				struct bu_external *ep, int id));
RT_EXTERN(union tree		*nmg_booltree_evaluate, (union tree *tp,
				CONST struct bn_tol *tol));
RT_EXTERN(void			nmg_region_v_unique, (struct nmgregion *r1,
				CONST struct bn_tol *tol));

/* from nmg_class.c */
RT_EXTERN(void			nmg_class_shells, (struct shell *sA,
				struct shell *sB, long *classlist[4],
				CONST struct bn_tol *tol) );

/* from nmg_fcut.c */
RT_EXTERN(void			nmg_set_lu_orientation, (struct loopuse	*lu,
				int is_opposite) );
/* static void ptbl_vsort */
RT_EXTERN(double		nmg_vu_angle_measure, (struct vertexuse	*vu,
				vect_t x_dir, vect_t y_dir, int assessment,
				int in) );
RT_EXTERN(struct edge_g_lseg	*nmg_face_cutjoin, (
				struct bu_ptbl *b1, struct bu_ptbl *b2,
				fastf_t *mag1, fastf_t *mag2,
				struct faceuse *fu1, struct faceuse *fu2,
				point_t pt, vect_t dir,
				struct edge_g_lseg *eg,
				CONST struct bn_tol *tol) );

#define nmg_mev(_v, _u)	nmg_me((_v), (struct vertex *)NULL, (_u))

/* From nmg_eval.c */
RT_EXTERN(CONST char		*nmg_class_name, (int class) );
#if 0
/* These can't be included because struct nmg_bool_state is in nmg_eval.c */
RT_EXTERN(void			nmg_eval_shell, (struct shell *s,
				struct nmg_bool_state *bs ) );
RT_EXTERN(void			nmg_eval_plot, (struct nmg_bool_state *bs,
				int num, int delay) );
#endif


/* From nmg_rt_isect.c */
#if 0
RT_EXTERN(void nmg_isect_ray_model, (struct ray_data *rd) );

/* From nmg_rt_segs.c */
RT_EXTERN(int			nmg_ray_isect_segs, (struct soltab *stp,
					struct xray *rp,
					struct application *ap,
					struct seg *seghead,
					struct nmg_specific *nmg_spec) );
#endif
/* From nmg_ck.c */
/* XXX many others here */
RT_EXTERN(void			nmg_ck_list, (struct bu_list *hd, CONST char *str) );
RT_EXTERN(void			nmg_ck_lueu, (struct loopuse *lu, char *s) );
RT_EXTERN(int			nmg_check_radial, (CONST struct edgeuse *eu, CONST struct bn_tol *tol));
RT_EXTERN(int			nmg_ck_closed_surf, (CONST struct shell *s, CONST struct bn_tol *tol) );
RT_EXTERN(int			nmg_ck_closed_region, (CONST struct nmgregion *r, CONST struct bn_tol *tol) );
RT_EXTERN(void			nmg_ck_v_in_2fus, (CONST struct vertex *vp,
				CONST struct faceuse *fu1, CONST struct faceuse *fu2,
				CONST struct bn_tol *tol));

/* From nmg_inter.c */
RT_EXTERN(void			nmg_crackshells, (struct shell *s1, struct shell *s2, CONST struct bn_tol *tol) );

/* From nmg_index.c */
RT_EXTERN(int			nmg_index_of_struct, (long *p) );
RT_EXTERN(void			nmg_m_reindex, (struct model *m, long newindex) );
RT_EXTERN(void			nmg_vls_struct_counts, (struct bu_vls *str,
				CONST struct nmg_struct_counts *ctr));
RT_EXTERN(void			nmg_pr_struct_counts, 
				(CONST struct nmg_struct_counts *ctr,
				CONST char *str) );
RT_EXTERN(long			**nmg_m_struct_count,
				(struct nmg_struct_counts *ctr,
				CONST struct model *m) );
RT_EXTERN(void			nmg_merge_models, (struct model *m1,
							struct model *m2) );
/* From nmg_rt.c */

/* From rt_dspline.c */
RT_EXTERN(void			rt_dspline_matrix, (mat_t m,CONST char *type,
					CONST double	tension,
					CONST double	bias) );
RT_EXTERN(double		rt_dspline4, (mat_t m, double a, double b,
					double c, double d, double alpha) );
RT_EXTERN(void			rt_dspline4v, (double *pt, mat_t m, double *a,
					double *b, double *c, double *d,
					int depth, double alpha) );
RT_EXTERN(void			rt_dspline_n, (double *r, CONST mat_t m,
					CONST double *knots, CONST int n,
					CONST int depth, CONST double alpha));

#endif

/*
 *  Constants provided and used by the RT library.
 */
extern CONST struct db_tree_state	rt_initial_tree_state;
extern CONST char   *rt_vlist_cmd_descriptions[];

/* vers.c (created by librt/Cakefile) */
extern CONST char   rt_version[];

#if defined(NMG_H)
extern CONST struct nmg_visit_handlers  nmg_visit_handlers_null;
#endif

#ifdef __cplusplus
}
#endif

#endif /* RAYTRACE_H */
