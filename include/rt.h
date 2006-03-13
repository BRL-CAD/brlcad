/*                              R T . H
 *
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * WARNING: do not use this file in external applications, as it will
 * become something else in a future version of BRL-CAD.
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @file rt.h
 *
 *  All the data structures and manifest constants
 *  for the core raytracing routines
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
 *  Libraries Used -
 *	LIBRT LIBRT_LIBES -lm -lc
 *
 *  $Header$
 */


#ifndef RT_H
#define RT_H seen

#include "common.h"
#include "bu.h"
#include "bn.h"

__BEGIN_DECLS

#ifndef RT_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef RT_EXPORT_DLL
#      define RT_EXPORT __declspec(dllexport)
#    else
#      define RT_EXPORT __declspec(dllimport)
#    endif
#  else
#    define RT_EXPORT
#  endif
#endif

#define RAYTRACE_H_VERSION	"@(#)$Header$ (BRL)"

/*
 *				D E B U G
 *
 *  Each type of debugging support is independently controled,
 *  by a separate bit in the word RT_G_DEBUG
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
#define DEBUG_VOL	0x00000800	/* 12 Volume & opaque Binary solid */

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
#ifdef INFINITY
#	undef INFINITY
#endif

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
 *			R T _ D B _ I N T E R N A L
 *
 *  A handle on the internal format of an MGED database object.
 */
struct rt_db_internal  {
	long		idb_magic;
	int		idb_major_type;
	int		idb_minor_type;		/* ID_xxx */
	const struct rt_functab *idb_meth;	/* for ft_ifree(), etc. */
	genptr_t	idb_ptr;
	struct bu_attribute_value_set idb_avs;
};
#define idb_type		idb_minor_type
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
	const struct rt_functab *st_meth; /* pointer to per-solid methods */
	struct rt_i	*st_rtip;	/* "up" pointer to rt_i */
	long		st_uses;	/* Usage count, for instanced solids */
	int		st_id;		/* Solid ident */
	point_t		st_center;	/* Centroid of solid */
	fastf_t		st_aradius;	/* Radius of APPROXIMATING sphere */
	fastf_t		st_bradius;	/* Radius of BOUNDING sphere */
	genptr_t	st_specific;	/* -> ID-specific (private) struct */
	const struct directory *st_dp;	/* Directory entry of solid */
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

  /* Add a new primitive id above here (this is will break v5 format)
   * XXX must update the non-geometric object id's below XXX
   */
#define	ID_MAX_SOLID	36	/* Maximum defined ID_xxx for solids */

/*
 *	Non-geometric objects
 */
#define ID_COMBINATION	31	/* Combination Record */
#define ID_BINEXPM	32	/* Experimental binary */
#define ID_BINUNIF	33	/* Uniform-array binary */
#define ID_BINMIME	34	/* MIME-typed binary */

/* XXX - superellipsoid should be 31, but is not v5 compatible */
#define ID_SUPERELL	35	/* Superquadratic ellipsoid */

#define ID_MAXIMUM	36	/* Maximum defined ID_xxx value */

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
	const char	*reg_name;	/* Identifying string */
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
	struct bu_mro	**attr_values;	/* Null terminated array of MRO structs
					 * Each containing a value for the corresponding
					 * attribute name passed to rt_gettrees_and_attrs() */
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
	memcpy((char *)(&(new)->RT_PT_MIDDLE_START), (char *)(&(old)->RT_PT_MIDDLE_START), RT_PT_MIDDLE_LEN(old) ); \
	(new)->pt_overlap_reg = NULL; \
	bu_ptbl_cat( &(new)->pt_seglist, &(old)->pt_seglist );  }

/* Clear out the pointers, empty the hit list */
#define GET_PT_INIT(ip,p,res)	{\
	GET_PT(ip,p,res); \
	memset( ((char *) &(p)->RT_PT_MIDDLE_START), 0, RT_PT_MIDDLE_LEN(p) ); }

#define GET_PT(ip,p,res)   { \
	if( BU_LIST_NON_EMPTY_P(p, partition, &res->re_parthead) )  { \
		BU_LIST_DEQUEUE((struct bu_list *)(p)); \
		bu_ptbl_reset( &(p)->pt_seglist ); \
	} else { \
		(p) = (struct partition *)bu_calloc(1, sizeof(struct partition), "struct partition"); \
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
			BU_LIST_DEQUEUE( (struct bu_list *)(_zap) ); \
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
		int	bn_piecelen;	/* # of piecelists used */
		int	bn_maxpiecelen; /* # of piecelists allocated */
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
	char			*const*dbi_filepath; /* search path for aux file opens (convenience var) */
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
	struct rt_wdb		*dbi_wdbp;	/* ptr back to containing rt_wdb */
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
	unsigned char	d_major_type;		/* object major type */
	unsigned char 	d_minor_type;		/* object minor type */
	struct bu_list	d_use_hd;		/* heads list of uses (struct soltab l2) */
	char		d_shortname[16];	/* Stash short names locally */
};
#define DIR_NULL	((struct directory *)0)
#define RT_DIR_MAGIC	0x05551212		/* Directory assistance */
#define RT_CK_DIR(_dp)	BU_CKMAG(_dp, RT_DIR_MAGIC, "(librt)directory")

#define d_addr	d_un.file_offset
#define RT_DIR_PHONY_ADDR	(-1L)	/* Special marker for d_addr field */

/* flags for db_diradd() and friends */
#define DIR_SOLID	0x1		/* this name is a solid */
#define DIR_COMB	0x2		/* combination */
#define DIR_REGION	0x4		/* region */
#define DIR_HIDDEN	0x8		/* object name is hidden */
#define	DIR_NON_GEOM	0x10		/* object is not geometry (e.g. binary object) */
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
 *  Perhaps move to wdb.h or rtgeom.h?
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
 *  Perhaps move to wdb.h or rtgeom.h?
 */
struct rt_binunif_internal {
	long		magic;
	int		type;
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
	struct bu_attribute_value_set	ts_attrs;	/* attribute/value structure */

	int		ts_stop_at_regions;	/* else stop at solids */
	int		(*ts_region_start_func) BU_ARGS((
				struct db_tree_state * /*tsp*/,
				struct db_full_path * /*pathp*/,
				const struct rt_comb_internal * /* combp */,
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
	const struct rt_tess_tol *ts_ttol;	/* Tessellation tolerance */
	const struct bn_tol	*ts_tol;	/* Math tolerance */
	struct model		**ts_m;		/* ptr to ptr to NMG "model" */
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
		const char	*td_name;	/* If non-null, dynamic string describing heritage of this region */
		struct nmgregion *td_r;		/* ptr to NMG region */
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
	struct bu_vls	wdb_prestr;
	int		wdb_ncharadd;
	int		wdb_num_dups;

	/* default region ident codes for this particular database. */
	int		wdb_item_default;/* GIFT region ID */
	int		wdb_air_default;
	int		wdb_mat_default;/* GIFT material code */
	int		wdb_los_default;/* Line-of-sight estimate */
	struct bu_observer	wdb_observers;
	Tcl_Interp	*wdb_interp;
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

RT_EXPORT extern struct rt_wdb HeadWDB;		/* head of BRL-CAD database object list */

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
 * Used to keep track of forked rt's for possible future aborts.
 * Currently used in mged/rtif.c and librt/dg_obj.c
 */
struct run_rt {
	struct bu_list		l;
#ifdef _WIN32
	HANDLE			fd;
	HANDLE			hProcess;
	DWORD			pid;

#ifdef TCL_OK
	Tcl_Channel		chan;
#else
	genptr_t chan;
#endif
#else
	int			fd;
	int			pid;
#endif
	int			aborted;
};

/*
 *			D G _ O B J
 *
 * A drawable geometry object is associated with a database object
 * and is used to maintain lists of geometry that are ready for display.
 * This geometry can come from a Brl-Cad database or from vdraw commands.
 * The drawable geometry object is also capabable of raytracing geometry
 * that comes from a Brl-Cad database.
 */
struct dg_qray_color {
  unsigned char r;
  unsigned char g;
  unsigned char b;
};

struct dg_qray_fmt {
  char type;
  struct bu_vls fmt;
};

struct dg_obj {
	struct bu_list	l;
	struct bu_vls		dgo_name;	/* drawable geometry object name */
	struct rt_wdb		*dgo_wdbp;	/* associated database */
	struct bu_list		dgo_headSolid;	/* head of solid list */
	struct bu_list		dgo_headVDraw;	/* head of vdraw list */
	struct vd_curve		*dgo_currVHead;	/* current vdraw head */
	struct solid		*dgo_freeSolids; /* ptr to head of free solid list */
	char			*dgo_rt_cmd[RT_MAXARGS];
	int			dgo_rt_cmd_len;
	struct bu_observer	dgo_observers;
	struct run_rt		dgo_headRunRt;	/* head of forked rt processes */
	struct bu_vls		dgo_qray_basename;	/* basename of query ray vlist */
	struct bu_vls		dgo_qray_script;	/* query ray script */
	char			dgo_qray_effects;	/* t for text, g for graphics or b for both */
	int			dgo_qray_cmd_echo;	/* 0 - don't echo command, 1 - echo command */
	struct dg_qray_fmt	*dgo_qray_fmts;
	struct dg_qray_color	dgo_qray_odd_color;
	struct dg_qray_color	dgo_qray_even_color;
	struct dg_qray_color	dgo_qray_void_color;
	struct dg_qray_color	dgo_qray_overlap_color;
	int			dgo_shaded_mode;	/* 1 - draw bots shaded by default */
	char			*dgo_outputHandler;	/* tcl script for handling output */
	int			dgo_uplotOutputMode;	/* output mode for unix plots */
};
RT_EXPORT extern struct dg_obj HeadDGObj;		/* head of drawable geometry object list */
#define RT_DGO_NULL		((struct dg_obj *)NULL)


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

/* Used to set globals declared in g_bot.c */
#define RT_DEFAULT_MINPIECES		32
#define RT_DEFAULT_TRIS_PER_PIECE	4

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
RT_EXPORT extern struct resource	rt_uniresource;	/* default.  Defined in librt/shoot.c */
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


/*			R T _ R E P R E P _ O B J _ L I S T
 *
 *	Structure used by the "reprep" routines
 */
struct rt_reprep_obj_list {
	int ntopobjs;		/* number of objects in the original call to gettrees */
	char **topobjs;		/* list of the above object names */
	int nunprepped;		/* number of objects to be unprepped and re-prepped */
	char **unprepped;	/* list of the above objects */
	/* Above here must be filled in by application */
	/* Below here is used by dynamic geometry routines, should be zeroed by application before use */
	struct bu_ptbl paths;	/* list of all paths from topobjs to unprepped objects */
	struct db_tree_state **tsp;	/* tree state used by tree walker in "reprep" routines */
	struct bu_ptbl unprep_regions;	/* list of region structures that will be "unprepped" */
	long old_nsolids;		/* rtip->nsolids before unprep */
	long old_nregions;		/* rtip->nregions before unprep */
	long nsolids_unprepped;		/* number of soltab structures eliminated by unprep */
	long nregions_unprepped;	/* number of region structures eliminated by unprep */
};




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
 *  The entire structure should be zeroed (e.g. by memset(,0,) ) before it
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
 *  Therefore, rather than using compile-time structure
 *  initialization, you should create a zeroed-out structure, and then
 *  assign the intended values at runtime.  A zeroed structure can be
 *  obtained at compile time with "static struct application
 *  zero_ap;", or at run time by using "memset( (char *)ap, 0,
 *  sizeof(struct application) );" or bu_calloc( 1, sizeof(struct
 *  application), "application" ); While this practice may not work on
 *  machines where "all bits off" does not signify a floating point
 *  zero, BRL-CAD does not support any such machines, so this is a
 *  moot issue.
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
	void		(*a_multioverlap)BU_ARGS( (struct application *,	/* called to resolve overlaps */
						   struct partition *,
						   struct bu_ptbl *,
						   struct partition *) );
	void		(*a_logoverlap)BU_ARGS( (struct application *,	/* called to log overlaps */
						 const struct partition *,
						 const struct bu_ptbl *,
						 const struct partition *) );
	int		a_level;	/* recursion level (for printing) */
	int		a_x;		/* Screen X of ray, if applicable */
	int		a_y;		/* Screen Y of ray, if applicable */
	char		*a_purpose;	/* Debug string:  purpose of ray */
	fastf_t		a_rbeam;	/* initial beam radius (mm) */
	fastf_t		a_diverge;	/* slope of beam divergance/mm */
	int		a_return;	/* Return of a_hit()/a_miss() */
	int		a_no_booleans;	/* 1= partitions==segs, no booleans */
	char		**attrs;	/* null terminated list of attributes
					 * This list should be the same as passed to
					 * rt_gettrees_and_attrs() */
	/* THESE ELEMENTS ARE USED BY THE PROGRAM "rt" AND MAY BE USED BY */
	/* THE LIBRARY AT SOME FUTURE DATE */
	/* AT THIS TIME THEY MAY BE LEFT ZERO */
	struct pixel_ext *a_pixelext;	/* locations of pixel corners */
	/* THESE ELEMENTS ARE WRITTEN BY THE LIBRARY, AND MAY BE READ IN a_hit() */
	struct seg	*a_finished_segs_hdp;
	struct partition *a_Final_Part_hdp;
	vect_t		a_inv_dir;	/* filled in by rt_shootray(), inverse of ray direction cosines */
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
	int		a_flag;		/* application-specific flag */
	int		a_zero2;	/* must be zero (sanity check) */
};
#define RT_AFN_NULL	((int (*)())0)
#define RT_AP_MAGIC	0x4170706c	/* "Appl" */
#define RT_CK_AP(_p)	BU_CKMAG(_p,RT_AP_MAGIC,"struct application")
#define RT_CK_APPLICATION(_p)	BU_CKMAG(_p,RT_AP_MAGIC,"struct application")
#define RT_CK_AP_TCL(_interp,_p)	BU_CKMAG_TCL(_interp,_p,RT_AP_MAGIC,"struct application")
#define RT_APPLICATION_INIT(_p)	{ \
		memset( (char *)(_p), 0, sizeof(struct application) ); \
		(_p)->a_magic = RT_AP_MAGIC; \
	}


#ifdef NO_BOMBING_MACROS
#  define RT_AP_CHECK(_ap)
#else
#  define RT_AP_CHECK(_ap)	\
	{if((_ap)->a_zero1||(_ap)->a_zero2) \
		rt_bomb("corrupt application struct"); }
#endif

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
	int		NMG_debug;	/* debug bits for NMG's see nmg.h */
	struct rt_wdb	rtg_headwdb;	/* head of database object list */
};
RT_EXPORT extern struct rt_g rt_g;

/* Normally set when in production mode, setting the RT_G_DEBUG define to 0
 * will allow chucks of code to poof away at compile time (since they are
 * truth-functionally constant (false))  This can boost raytrace performance
 * considerably (~10%).
 */
#ifdef NO_DEBUG_CHECKING
#	define	RT_G_DEBUG	0
#else
#	define	RT_G_DEBUG	rt_g.debug
#endif

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
	genptr_t	Orca_hash_tbl;	/* Hash table in matrices for ORCA */
	struct bu_ptbl	delete_regs;	/* list of region pointers to delete after light_init() */
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
	/* Parameters for dynamic geometry */
	int		rti_add_to_new_solids_list;
	struct bu_ptbl	rti_new_solids;
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
	const union cutter	*lastcut, *lastcell;
	const union cutter	*curcut;
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


/* Command stuff */

/* Read semi-colon terminated line */
RT_EXPORT BU_EXTERN(char *rt_read_cmd,
		    (FILE *fp));
RT_EXPORT BU_EXTERN(int rt_split_cmd,
		    (char **argv,
		     int lim,
		     char *lp));
/* do cmd from string via cmd table */
RT_EXPORT BU_EXTERN(int rt_do_cmd,
		    (struct rt_i *rtip,
		     const char *ilp,
		     const struct command_tab *tp));


/*****************************************************************
 *                                                               *
 *          Applications interface to the RT library             *
 *                                                               *
 *****************************************************************/
					/* Read named MGED db, build toc */
RT_EXPORT BU_EXTERN(struct rt_i *rt_dirbuild,
		    (const char *filename,
		     char *buf,
		     int len));
/* Prepare for raytracing */
RT_EXPORT BU_EXTERN(struct rt_i *rt_new_rti,
		    (struct db_i *dbip));
RT_EXPORT BU_EXTERN(void rt_free_rti,
		    (struct rt_i *rtip));
RT_EXPORT BU_EXTERN(void rt_prep,
		    (struct rt_i *rtip));
RT_EXPORT BU_EXTERN(void rt_prep_parallel,
		    (struct rt_i *rtip,
		     int ncpu));
/* DEPRECATED, set a_logoverlap */
RT_EXPORT BU_EXTERN(int rt_overlap_quietly,
		    (struct application *ap,
		     struct partition *pp,
		     struct region *reg1,
		     struct region *reg2,
		     struct partition *pheadp));
RT_EXPORT BU_EXTERN(void rt_default_multioverlap,
		    (struct application *ap,
		     struct partition *pp,
		     struct bu_ptbl *regiontable,
		     struct partition *InputHdp));
RT_EXPORT BU_EXTERN(void rt_silent_logoverlap,
		    (struct application *ap,
		     const struct partition *pp,
		     const struct bu_ptbl *regiontable,
		     const struct partition *InputHdp));
RT_EXPORT BU_EXTERN(void rt_default_logoverlap,
		    (struct application *ap,
		     const struct partition *pp,
		     const struct bu_ptbl *regiontable,
		     const struct partition *InputHdp));
/* Shoot a ray */
RT_EXPORT BU_EXTERN(int rt_shootray,
		    (struct application *ap));
/* Get expr tree for object */
RT_EXPORT BU_EXTERN(const char *rt_basename,
		    (const char *str));
RT_EXPORT BU_EXTERN(void rt_free_soltab,
		    (struct soltab   *stp));
RT_EXPORT BU_EXTERN(int rt_gettree,
		    (struct rt_i *rtip,
		     const char *node));
RT_EXPORT BU_EXTERN(int rt_gettrees,
		    (struct rt_i *rtip,
		     int argc,
		     const char **argv, int ncpus));
RT_EXPORT BU_EXTERN(int rt_gettrees_and_attrs,
		    (struct rt_i *rtip,
		     const char **attrs,
		     int argc,
		     const char **argv, int ncpus));
RT_EXPORT BU_EXTERN(int rt_gettrees_muves,
		    (struct rt_i *rtip,
		     const char **attrs,
		     int argc,
		     const char **argv,
		     int ncpus));
RT_EXPORT BU_EXTERN(int rt_load_attrs,
		    (struct rt_i *rtip,
		     char **attrs));
/* Print seg struct */
RT_EXPORT BU_EXTERN(void rt_pr_seg,
		    (const struct seg *segp));
/* Print the partitions */
RT_EXPORT BU_EXTERN(void rt_pr_partitions,
		    (const struct rt_i *rtip,
		     const struct partition *phead,
		     const char *title));
/* Find solid by leaf name */
RT_EXPORT BU_EXTERN(struct soltab *rt_find_solid,
		    (const struct rt_i *rtip,
		     const char *name));
/* Start the timer */
RT_EXPORT BU_EXTERN(void rt_prep_timer,
		    (void));
/* Read timer, return time + str */
RT_EXPORT BU_EXTERN(double rt_get_timer,
		    (struct bu_vls *vp,
		     double *elapsed));
/* Return CPU time, text, & wall clock time */
RT_EXPORT BU_EXTERN(double rt_read_timer,
		    (char *str, int len));
/* Plot a solid */
int rt_plot_solid(
	FILE			*fp,
	struct rt_i		*rtip,
	const struct soltab	*stp,
	struct resource		*resp);
/* Release storage assoc with rt_i */
RT_EXPORT BU_EXTERN(void rt_clean,
		    (struct rt_i *rtip));
RT_EXPORT BU_EXTERN(int rt_del_regtree,
		    (struct rt_i *rtip,
		     struct region *delregp,
		     struct resource *resp));
/* Check in-memory data structures */
RT_EXPORT BU_EXTERN(void rt_ck,
		    (struct rt_i *rtip));
RT_EXPORT BU_EXTERN(void rt_pr_library_version,
		    ());

/*****************************************************************
 *                                                               *
 *  Internal routines in the RT library.			 *
 *  These routines are *not* intended for Applications to use.	 *
 *  The interface to these routines may change significantly	 *
 *  from release to release of this software.			 *
 *                                                               *
 *****************************************************************/

					/* Weave segs into partitions */
RT_EXPORT BU_EXTERN(void rt_boolweave,
		    (struct seg *out_hd,
		     struct seg *in_hd,
		     struct partition *PartHeadp,
		     struct application *ap));
/* Eval booleans over partitions */
RT_EXPORT BU_EXTERN(int rt_boolfinal,
		    (struct partition *InputHdp,
		     struct partition *FinalHdp,
		     fastf_t startdist,
		     fastf_t enddist,
		     struct bu_ptbl *regionbits,
		     struct application *ap,
		     const struct bu_bitv *solidbits));

RT_EXPORT BU_EXTERN(void rt_grow_boolstack,
		    (struct resource *res));
/* Approx Floating compare */
RT_EXPORT BU_EXTERN(int rt_fdiff,
		    (double a, double b));
/* Relative Difference */
RT_EXPORT BU_EXTERN(double rt_reldiff,
		    (double a, double b));
/* Print a soltab */
RT_EXPORT BU_EXTERN(void rt_pr_soltab,
		    (const struct soltab *stp));
/* Print a region */
RT_EXPORT BU_EXTERN(void rt_pr_region,
		    (const struct region *rp));
/* Print an expr tree */
RT_EXPORT BU_EXTERN(void rt_pr_tree,
		    (const union tree *tp,
		     int lvl));
/* Print value of tree for a partition */
RT_EXPORT BU_EXTERN(void rt_pr_tree_val,
		    (const union tree *tp,
		     const struct partition *partp,
		     int pr_name, int lvl));
/* Print a partition */
RT_EXPORT BU_EXTERN(void rt_pr_pt,
		    (const struct rt_i *rtip,
		     const struct partition *pp));
/* Print a bit vector */
RT_EXPORT BU_EXTERN(void rt_pr_hit,
		    (const char *str,
		     const struct hit *hitp));

/* rt_fastf_float, rt_mat_dbmat, rt_dbmat_mat
 * declarations moved to db.h */

/* storage obtainers */
RT_EXPORT BU_EXTERN(void rt_get_seg,
		    (struct resource *res));
RT_EXPORT BU_EXTERN(void rt_cut_it,
		    (struct rt_i *rtip,
		     int ncpu));
/* print cut node */
RT_EXPORT BU_EXTERN(void rt_pr_cut,
		    (const union cutter *cutp,
		     int lvl));
/* free a cut tree */
RT_EXPORT BU_EXTERN(void rt_fr_cut,
		    (struct rt_i *rtip,
		     union cutter *cutp));
/* regionid-driven color override */

/* bool.c */
RT_EXPORT BU_EXTERN(void rt_rebuild_overlaps,
		    (struct partition	*PartHdp,
		     struct application	*ap,
		     int		rebuild_fastgen_plates_only));
RT_EXPORT BU_EXTERN(int rt_partition_len,
		    (const struct partition *partheadp));
RT_EXPORT BU_EXTERN(int	rt_defoverlap,
		    (struct application *ap,
		     struct partition *pp,
		     struct region *reg1,
		     struct region *reg2,
		     struct partition *pheadp));



/* mater.c */

RT_EXPORT BU_EXTERN(void rt_region_color_map,
		    (struct region *regp));
/* process ID_MATERIAL record */
RT_EXPORT BU_EXTERN(void rt_color_addrec,
		    (int low,
		     int hi,
		     int r,
		     int g,
		     int b,
		     long addr));
RT_EXPORT BU_EXTERN(void rt_color_free,
		    ());

/* extend a cut box */

/* cut.c */
RT_EXPORT BU_EXTERN(void rt_pr_cut_info,
		    (const struct rt_i	*rtip,
		     const char		*str));
RT_EXPORT BU_EXTERN(void remove_from_bsp,
		    (struct soltab *stp,
		     union cutter *cutp,
		     struct bn_tol *tol));
RT_EXPORT BU_EXTERN(void insert_in_bsp,
		    (struct soltab *stp,
		     union cutter *cutp));
RT_EXPORT BU_EXTERN(void fill_out_bsp,
		    (struct rt_i *rtip,
		     union cutter *cutp,
		     struct resource *resp,
		     fastf_t bb[6]));
RT_EXPORT BU_EXTERN(void rt_cut_extend,
		    (union cutter *cutp,
		     struct soltab *stp,
		     const struct rt_i *rtip));
/* find RPP of one region */
RT_EXPORT BU_EXTERN(int rt_rpp_region,
		    (struct rt_i *rtip,
		     const char *reg_name,
		     fastf_t *min_rpp,
		     fastf_t *max_rpp));
RT_EXPORT BU_EXTERN(void rt_bomb,
		    (const char *s));
RT_EXPORT BU_EXTERN(int rt_in_rpp,
		    (struct xray *rp,
		     const fastf_t *invdir,
		     const fastf_t *min,
		     const fastf_t *max));
RT_EXPORT BU_EXTERN(const union cutter *rt_cell_n_on_ray,
		    (struct application *ap,
		     int n));
RT_EXPORT BU_EXTERN(void rt_cut_clean,
		    (struct rt_i *rtip));



__END_DECLS

#endif
