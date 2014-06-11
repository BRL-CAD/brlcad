/*                      R A Y T R A C E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup librt */
/** @{ */
/** @file raytrace.h
 *
 * All the data structures and manifest constants necessary for
 * interacting with the BRL-CAD LIBRT ray-tracing library.
 *
 * Note that this header file defines many internal data structures,
 * as well as the library's external (interface) data structures.
 * These are provided for the convenience of applications builders.
 * However, the internal data structures are subject to change in each
 * release.
 *
 */

#ifndef RAYTRACE_H
#define RAYTRACE_H

#include "common.h"

/* interface headers */
#include "tcl.h"
#include "bu/avs.h"
#include "bu/bitv.h"
#include "bu/bu_tcl.h"
#include "bu/file.h"
#include "bu/hash.h"
#include "bu/hist.h"
#include "bu/malloc.h"
#include "bu/mapped_file.h"
#include "bu/list.h"
#include "bu/log.h"
#include "bu/parallel.h" /* needed for BU_SEM_LAST */
#include "bu/parse.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bn.h"
#include "db5.h"
#include "nmg.h"
#include "pc.h"
#include "rtgeom.h"

__BEGIN_DECLS

#include "./rt/defines.h"

/**
 * Each type of debugging support is independently controlled, by a
 * separate bit in the word RT_G_DEBUG
 *
 * For programs based on the "RT" program, these flags follow the "-x"
 * (lower case x) option.
 */
#define DEBUG_OFF	0	/**< @brief No debugging */

/* These definitions are each for one bit */

/* Options useful for debugging applications */
#define DEBUG_ALLRAYS	0x00000001	/**< @brief 1 Print calls to rt_shootray() */
#define DEBUG_ALLHITS	0x00000002	/**< @brief 2 Print partitions passed to a_hit() */
#define DEBUG_SHOOT	0x00000004	/**< @brief 3 Info about rt_shootray() processing */
#define DEBUG_INSTANCE	0x00000008	/**< @brief 4 regionid instance revectoring */

/* Options useful for debugging the database */
#define DEBUG_DB	0x00000010	/**< @brief 5 Database debugging */
#define DEBUG_SOLIDS	0x00000020	/**< @brief 6 Print prep'ed solids */
#define DEBUG_REGIONS	0x00000040	/**< @brief 7 Print regions & boolean trees */
#define DEBUG_ARB8	0x00000080	/**< @brief 8 Print voluminous ARB8 details */
#define DEBUG_SPLINE	0x00000100	/**< @brief 9 Splines */
#define DEBUG_ANIM	0x00000200	/**< @brief 10 Animation */
#define DEBUG_ANIM_FULL	0x00000400	/**< @brief 11 Animation matrices */
#define DEBUG_VOL	0x00000800	/**< @brief 12 Volume & opaque Binary solid */

/* Options useful for debugging the library */
#define DEBUG_ROOTS	0x00001000	/**< @brief 13 Print rootfinder details */
#define DEBUG_PARTITION	0x00002000	/**< @brief 14 Info about bool_weave() */
#define DEBUG_CUT	0x00004000	/**< @brief 15 Print space cutting statistics */
#define DEBUG_BOXING	0x00008000	/**< @brief 16 Object/box checking details */
#define DEBUG_MEM	0x00010000	/**< @brief 17 -->> BU_DEBUG_MEM_LOG */
#define DEBUG_MEM_FULL	0x00020000	/**< @brief 18 -->> BU_DEBUG_MEM_CHECK */
#define DEBUG_FDIFF	0x00040000	/**< @brief 19 bool/fdiff debugging */
#define DEBUG_PARALLEL	0x00080000	/**< @brief 20 -->> BU_DEBUG_PARALLEL */
#define DEBUG_CUTDETAIL	0x00100000	/**< @brief 21 Print space cutting details */
#define DEBUG_TREEWALK	0x00200000	/**< @brief 22 Database tree traversal */
#define DEBUG_TESTING	0x00400000	/**< @brief 23 One-shot debugging flag */
#define DEBUG_ADVANCE	0x00800000	/**< @brief 24 Cell-to-cell space partitioning */
#define DEBUG_MATH	0x01000000	/**< @brief 25 nmg math routines */

/* Options for debugging particular solids */
#define DEBUG_EBM	0x02000000	/**< @brief 26 Extruded bit-map solids */
#define DEBUG_HF	0x04000000	/**< @brief 27 Height Field solids */

#define DEBUG_UNUSED1	0x08000000	/**< @brief 28 unused */
#define DEBUG_UNUSED2	0x10000000	/**< @brief 29 unused */
#define DEBUG_UNUSED3	0x20000000	/**< @brief 30 unused */

/* Options which will cause the library to write binary debugging output */
#define DEBUG_PL_SOLIDS 0x40000000	/**< @brief 31 plot all solids */
#define DEBUG_PL_BOX	0x80000000	/**< @brief 32 Plot(3) bounding boxes and cuts */

/** Format string for bu_printb() */
#define DEBUG_FORMAT	\
    "\020\040PLOTBOX\
\037PLOTSOLIDS\
\033HF\032EBM\031MATH\030ADVANCE\
\027TESTING\026TREEWALK\025CUTDETAIL\024PARALLEL\023FDIFF\022MEM_FULL\
\021MEM\020BOXING\017CUTTING\016PARTITION\015ROOTS\014VOL\
\013ANIM_FULL\012ANIM\011SPLINE\010ARB8\7REGIONS\6SOLIDS\5DB\
\4INSTANCE\3SHOOT\2ALLHITS\1ALLRAYS"

/**
 * FIXME: These should probably be vmath macros
 */
#define	RT_BADNUM(n)	(!((n) >= -INFINITY && (n) <= INFINITY))
#define RT_BADVEC(v)	(RT_BADNUM((v)[X]) || RT_BADNUM((v)[Y]) || RT_BADNUM((v)[Z]))


/*
 * Unfortunately, to prevent divide-by-zero, some tolerancing needs to
 * be introduced.
 *
 *
 * RT_LEN_TOL is the shortest length, in mm, that can be stood as the
 * dimensions of a primitive.  Can probably become at least
 * SQRT_SMALL_FASTF.
 *
 * Dot products smaller than RT_DOT_TOL are considered to have a dot
 * product of zero, i.e., the angle is effectively zero.  This is used
 * to check vectors that should be perpendicular.
 *
 * asin(0.1) = 5.73917 degrees
 * asin(0.01) = 0.572967 degrees
 * asin(0.001) = 0.0572958 degrees
 * asin(0.0001) = 0.00572958 degrees
 *
 * sin(0.01 degrees) = sin(0.000174 radians) = 0.000174533
 *
 * Many TGCs at least, in existing databases, will fail the
 * perpendicularity test if DOT_TOL is much smaller than 0.001, which
 * establishes a 1/20th degree tolerance.  The intent is to eliminate
 * grossly bad primitives, not pick nits.
 *
 * RT_PCOEF_TOL is a tolerance on polynomial coefficients to prevent
 * the root finder from having heartburn.
 */
#define RT_LEN_TOL	(1.0e-8)
#define RT_DOT_TOL	(0.001)
#define RT_PCOEF_TOL	(1.0e-10)


/**
 * Tessellation (geometric) tolerances, different beasts than the
 * calculation tolerance in bn_tol.
 */
struct rt_tess_tol {
    uint32_t magic;
    double		abs;			/**< @brief absolute dist tol */
    double		rel;			/**< @brief rel dist tol */
    double		norm;			/**< @brief normal tol */
};
#define RT_CK_TESS_TOL(_p) BU_CKMAG(_p, RT_TESS_TOL_MAGIC, "rt_tess_tol")
#define RT_TESS_TOL_INIT_ZERO {RT_TESS_TOL_MAGIC, 0.0, 0.0, 0.0}

/**
 * A handle on the internal format of an MGED database object.
 */
struct rt_db_internal {
    uint32_t		idb_magic;
    int			idb_major_type;
    int			idb_minor_type;		/**< @brief ID_xxx */
    const struct rt_functab *idb_meth;	/**< @brief for ft_ifree(), etc. */
    void *		idb_ptr;
    struct bu_attribute_value_set idb_avs;
};
#define idb_type		idb_minor_type
#define RT_DB_INTERNAL_INIT(_p) { \
	(_p)->idb_magic = RT_DB_INTERNAL_MAGIC; \
	(_p)->idb_major_type = -1; \
	(_p)->idb_minor_type = -1; \
	(_p)->idb_meth = (const struct rt_functab *) ((void *)0); \
	(_p)->idb_ptr = ((void *)0); \
	bu_avs_init_empty(&(_p)->idb_avs); \
    }
#define RT_CK_DB_INTERNAL(_p) BU_CKMAG(_p, RT_DB_INTERNAL_MAGIC, "rt_db_internal")

/**
 * For collecting paths through the database tree.
 * The fp_bool array can optionally hold a boolean flag
 * associated with each corresponding dp in fp_names.  This
 * array must be manually maintained by the client code in
 * order for it to have valid data - many functions using
 * full paths (for example, conversion from strings) don't
 * have knowledge of a specific boolean tree.
 */
struct db_full_path {
    uint32_t		magic;
    size_t		fp_len;
    size_t		fp_maxlen;
    struct directory **	fp_names;	/**< @brief array of dir pointers */
    int	              * fp_bool;	/**< @brief array of boolean flags */
    matp_t 	      * fp_mat;		/**< @brief array of matrix pointers */
};
#define DB_FULL_PATH_POP(_pp) ((_pp)->fp_len > 0) ? (_pp)->fp_len-- : (_pp)->fp_len

#define DB_FULL_PATH_CUR_DIR(_pp) ((_pp)->fp_names[(_pp)->fp_len-1])
#define DB_FULL_PATH_CUR_BOOL(_pp) ((_pp)->fp_bool[(_pp)->fp_len-1])
#define DB_FULL_PATH_CUR_MATRIX(_pp) ((_pp)->fp_mat[(_pp)->fp_len-1])
#define DB_FULL_PATH_SET_CUR_BOOL(_pp, _i) ((_pp)->fp_bool[(_pp)->fp_len-1] = _i)
#define DB_FULL_PATH_SET_CUR_MATRIX(_pp, _m) { \
    (_pp)->fp_mat[(_pp)->fp_len-1] = (matp_t)bu_calloc((_pp)->fp_maxlen, sizeof(matp_t), "new db_full_path mat array"); \
    (void)memcpy((void *)(_pp->fp_mat[(_pp)->fp_len-1]), (const void *)(_m), sizeof(mat_t)); \
    }
#define DB_FULL_PATH_ROOT_DIR(_pp) ((_pp)->fp_names[0])
#define DB_FULL_PATH_GET(_pp, _i) ((_pp)->fp_names[(_i)])
#define DB_FULL_PATH_GET_BOOL(_pp, _i) ((_pp)->fp_bool[(_i)])
#define DB_FULL_PATH_SET_BOOL(_pp, _i, _j) ((_pp)->fp_bool[(_i)] = _j)
#define DB_FULL_PATH_GET_MATRIX(_pp, _i) ((_pp)->fp_mat[(_i)])
#define DB_FULL_PATH_SET_MATRIX(_pp, _i, _m) { \
    (_pp)->fp_mat[(_i)] = (matp_t)bu_calloc((_pp)->fp_maxlen, sizeof(matp_t), "new db_full_path mat array"); \
    (void)memcpy((void *)(_pp->fp_mat[(_i)]), (const void *)(_m), sizeof(mat_t)); \
    }



#define RT_CK_FULL_PATH(_p) BU_CKMAG(_p, DB_FULL_PATH_MAGIC, "db_full_path")

/**
 * All necessary information about a ray.
 * Not called just "ray" to prevent conflicts with VLD stuff.
 */
struct xray {
    uint32_t		magic;
    int			index;		/**< @brief Which ray of a bundle */
    point_t		r_pt;		/**< @brief Point at which ray starts */
    vect_t		r_dir;		/**< @brief Direction of ray (UNIT Length) */
    fastf_t		r_min;		/**< @brief entry dist to bounding sphere */
    fastf_t		r_max;		/**< @brief exit dist from bounding sphere */
};
#define RAY_NULL	((struct xray *)0)
#define RT_CK_RAY(_p) BU_CKMAG(_p, RT_RAY_MAGIC, "struct xray");

/**
 * This plural xrays structure is a bu_list based container designed
 * to hold a list or bundle of xray(s). This bundle is utilized by
 * rt_shootrays() through its application bundle input.
 *
 */
struct xrays
{
    struct bu_list l;
    struct xray ray;
};


/**
 * Information about where a ray hits the surface
 *
 * Important Note:  Surface Normals always point OUT of a solid.
 *
 * DEPRECATED: The hit_point and hit_normal elements will be removed
 * from this structure, so as to separate the concept of the solid's
 * normal at the hit point from the post-boolean normal at the hit
 * point.
 */
struct hit {
    uint32_t		hit_magic;
    fastf_t		hit_dist;	/**< @brief dist from r_pt to hit_point */
    point_t		hit_point;	/**< @brief DEPRECATED: Intersection point, use VJOIN1 hit_dist */
    vect_t		hit_normal;	/**< @brief DEPRECATED: Surface Normal at hit_point, use RT_HIT_NORMAL */
    vect_t		hit_vpriv;	/**< @brief PRIVATE vector for xxx_*() */
    void *		hit_private;	/**< @brief PRIVATE handle for xxx_shot() */
    int			hit_surfno;	/**< @brief solid-specific surface indicator */
    struct xray	*	hit_rayp;	/**< @brief pointer to defining ray */
};
#define HIT_NULL	((struct hit *)0)
#define RT_CK_HIT(_p) BU_CKMAG(_p, RT_HIT_MAGIC, "struct hit")
#define RT_HIT_INIT_ZERO { RT_HIT_MAGIC, 0.0, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, NULL, 0, NULL }

/**
 * Compute normal into (_hitp)->hit_normal.  Set flip-flag accordingly
 * depending on boolean logic, as one hit may be shared between
 * multiple partitions with different flip status.
 *
 * Example: box.r = box.s - sph.s; sph.r = sph.s
 *
 * Return the post-boolean normal into caller-provided _normal vector.
 */
#define RT_HIT_NORMAL(_normal, _hitp, _stp, _unused, _flipflag) { \
	RT_CK_HIT(_hitp); \
	RT_CK_SOLTAB(_stp); \
	RT_CK_FUNCTAB((_stp)->st_meth); \
	{ \
	    void *_n = (void *)_normal; \
	    if ((_stp)->st_meth->ft_norm) { \
		(_stp)->st_meth->ft_norm(_hitp, _stp, (_hitp)->hit_rayp); \
	    } \
	    if (_n != NULL) { \
		int _f = (int)_flipflag; \
		if (_f) { \
		    VREVERSE((fastf_t *)_normal, (_hitp)->hit_normal); \
		} else { \
		    VMOVE((fastf_t *)_normal, (_hitp)->hit_normal); \
		} \
	    } \
	} \
    }

/* A more powerful interface would be: */
/* RT_GET_NORMAL(_normal, _partition, inhit/outhit flag, ap) */


/**
 * Information about curvature of the surface at a hit point.  The
 * principal direction pdir has unit length and principal curvature
 * c1.  |c1| <= |c2|, i.e. c1 is the most nearly flat principle
 * curvature.  A POSITIVE curvature indicates that the surface bends
 * TOWARD the (outward pointing) normal vector at that point.  c1 and
 * c2 are the inverse radii of curvature.  The other principle
 * direction is implied: pdir2 = normal x pdir1.
 */
struct curvature {
    vect_t	crv_pdir;	/**< @brief Principle direction */
    fastf_t	crv_c1;		/**< @brief curvature in principle dir */
    fastf_t	crv_c2;		/**< @brief curvature in other direction */
};
#define CURVE_NULL	((struct curvature *)0)
#define RT_CURVATURE_INIT_ZERO { VINIT_ZERO, 0.0, 0.0 }

/**
 * Use this macro after having computed the normal, to compute the
 * curvature at a hit point.
 *
 * In Release 4.4 and earlier, this was called RT_CURVE().  When the
 * extra argument was added the name was changed.
 */
#define RT_CURVATURE(_curvp, _hitp, _flipflag, _stp) { \
	RT_CK_HIT(_hitp); \
	RT_CK_SOLTAB(_stp); \
	RT_CK_FUNCTAB((_stp)->st_meth); \
	if ((_stp)->st_meth->ft_curve) { \
	    (_stp)->st_meth->ft_curve(_curvp, _hitp, _stp); \
	} \
	if (_flipflag) { \
	    (_curvp)->crv_c1 = - (_curvp)->crv_c1; \
	    (_curvp)->crv_c2 = - (_curvp)->crv_c2; \
	} \
    }

/* A more powerful interface would be: */
/* RT_GET_CURVATURE(_curvp, _partition, inhit/outhit flag, ap) */

/**
 * Mostly for texture mapping, information about parametric space.
 */
struct uvcoord {
    fastf_t uv_u;	/**< @brief Range 0..1 */
    fastf_t uv_v;	/**< @brief Range 0..1 */
    fastf_t uv_du;	/**< @brief delta in u */
    fastf_t uv_dv;	/**< @brief delta in v */
};
#define RT_HIT_UVCOORD(ap, _stp, _hitp, uvp) { \
	RT_CK_HIT(_hitp); \
	RT_CK_SOLTAB(_stp); \
	RT_CK_FUNCTAB((_stp)->st_meth); \
	if ((_stp)->st_meth->ft_uv) { \
	    (_stp)->st_meth->ft_uv(ap, _stp, _hitp, uvp); \
	} \
    }


/* A more powerful interface would be: */
/* RT_GET_UVCOORD(_uvp, _partition, inhit/outhit flag, ap) */


/**
 * Intersection segment.
 *
 * Includes information about both endpoints of intersection.
 * Contains forward link to additional intersection segments if the
 * intersection spans multiple segments (e.g., shooting a ray through a
 * torus).
 */
struct seg {
    struct bu_list	l;
    struct hit		seg_in;		/**< @brief IN information */
    struct hit		seg_out;	/**< @brief OUT information */
    struct soltab *	seg_stp;	/**< @brief pointer back to soltab */
};
#define RT_SEG_NULL	((struct seg *)0)

#define RT_CHECK_SEG(_p) BU_CKMAG(_p, RT_SEG_MAGIC, "struct seg")
#define RT_CK_SEG(_p) BU_CKMAG(_p, RT_SEG_MAGIC, "struct seg")

#define RT_GET_SEG(p, res) { \
	while (!BU_LIST_WHILE((p), seg, &((res)->re_seg)) || !(p)) \
	    rt_alloc_seg_block(res); \
	BU_LIST_DEQUEUE(&((p)->l)); \
	(p)->l.forw = (p)->l.back = BU_LIST_NULL; \
	(p)->seg_in.hit_magic = (p)->seg_out.hit_magic = RT_HIT_MAGIC; \
	res->re_segget++; \
    }


#define RT_FREE_SEG(p, res) { \
	RT_CHECK_SEG(p); \
	BU_LIST_INSERT(&((res)->re_seg), &((p)->l)); \
	res->re_segfree++; \
    }


/**
 * This could be
 *	BU_LIST_INSERT_LIST(&((_res)->re_seg), &((_segheadp)->l))
 * except for security of checking & counting each element this way.
 */
#define RT_FREE_SEG_LIST(_segheadp, _res) { \
	register struct seg *_a; \
	while (BU_LIST_WHILE(_a, seg, &((_segheadp)->l))) { \
	    BU_LIST_DEQUEUE(&(_a->l)); \
	    RT_FREE_SEG(_a, _res); \
	} \
    }


/**
 * Macros to operate on Right Rectangular Parallelepipeds (RPPs).
 * TODO: move to vmath.h
 */
struct bound_rpp {
    point_t min;
    point_t max;
};


/**
 * Internal information used to keep track of solids in the model.
 * Leaf name and Xform matrix are unique identifier.  Note that all
 * objects store dimensional values in millimeters (mm).
 */
struct soltab {
    struct bu_list		l;		/**< @brief links, headed by rti_headsolid */
    struct bu_list		l2;		/**< @brief links, headed by st_dp->d_use_hd */
    const struct rt_functab *	st_meth;	/**< @brief pointer to per-solid methods */
    struct rt_i	*		st_rtip;	/**< @brief "up" pointer to rt_i */
    long			st_uses;	/**< @brief Usage count, for instanced solids */
    int				st_id;		/**< @brief Solid ident */
    point_t			st_center;	/**< @brief Centroid of solid */
    fastf_t			st_aradius;	/**< @brief Radius of APPROXIMATING sphere */
    fastf_t			st_bradius;	/**< @brief Radius of BOUNDING sphere */
    void *			st_specific;	/**< @brief -> ID-specific (private) struct */
    const struct directory *	st_dp;		/**< @brief Directory entry of solid */
    point_t			st_min;		/**< @brief min X, Y, Z of bounding RPP */
    point_t			st_max;		/**< @brief max X, Y, Z of bounding RPP */
    long			st_bit;		/**< @brief solids bit vector index (const) */
    struct bu_ptbl		st_regions;	/**< @brief ptrs to regions using this solid (const) */
    matp_t			st_matp;	/**< @brief solid coords to model space, NULL=identity */
    struct db_full_path 	st_path;	/**< @brief path from region to leaf */
    /* Experimental stuff for accelerating "pieces" of solids */
    long			st_npieces;	/**< @brief #  pieces used by this solid */
    long			st_piecestate_num; /**< @brief re_pieces[] subscript */
    struct bound_rpp *		st_piece_rpps;	/**< @brief bounding RPP of each piece of this solid */
};
#define st_name		st_dp->d_namep
#define RT_SOLTAB_NULL	((struct soltab *)0)
#define	SOLTAB_NULL	RT_SOLTAB_NULL		/**< @brief backwards compat */

#define RT_CHECK_SOLTAB(_p) BU_CKMAG(_p, RT_SOLTAB_MAGIC, "struct soltab")
#define RT_CK_SOLTAB(_p) BU_CKMAG(_p, RT_SOLTAB_MAGIC, "struct soltab")

/*
 * Values for Solid ID.
 */
#define ID_NULL		0	/**< @brief Unused */
#define ID_TOR		1	/**< @brief Toroid */
#define ID_TGC		2	/**< @brief Generalized Truncated General Cone */
#define ID_ELL		3	/**< @brief Ellipsoid */
#define ID_ARB8		4	/**< @brief Generalized ARB.  V + 7 vectors */
#define ID_ARS		5	/**< @brief ARS */
#define ID_HALF		6	/**< @brief Half-space */
#define ID_REC		7	/**< @brief Right Elliptical Cylinder [TGC special] */
#define ID_POLY		8	/**< @brief Polygonal faceted object */
#define ID_BSPLINE	9	/**< @brief B-spline object */
#define ID_SPH		10	/**< @brief Sphere */
#define	ID_NMG		11	/**< @brief n-Manifold Geometry solid */
#define ID_EBM		12	/**< @brief Extruded bitmap solid */
#define ID_VOL		13	/**< @brief 3-D Volume */
#define ID_ARBN		14	/**< @brief ARB with N faces */
#define ID_PIPE		15	/**< @brief Pipe (wire) solid */
#define ID_PARTICLE	16	/**< @brief Particle system solid */
#define ID_RPC		17	/**< @brief Right Parabolic Cylinder  */
#define ID_RHC		18	/**< @brief Right Hyperbolic Cylinder  */
#define ID_EPA		19	/**< @brief Elliptical Paraboloid  */
#define ID_EHY		20	/**< @brief Elliptical Hyperboloid  */
#define ID_ETO		21	/**< @brief Elliptical Torus  */
#define ID_GRIP		22	/**< @brief Pseudo Solid Grip */
#define ID_JOINT	23	/**< @brief Pseudo Solid/Region Joint */
#define ID_HF		24	/**< @brief Height Field */
#define ID_DSP		25	/**< @brief Displacement map */
#define	ID_SKETCH	26	/**< @brief 2D sketch */
#define	ID_EXTRUDE	27	/**< @brief Solid of extrusion */
#define ID_SUBMODEL	28	/**< @brief Instanced submodel */
#define	ID_CLINE	29	/**< @brief FASTGEN4 CLINE solid */
#define	ID_BOT		30	/**< @brief Bag o' triangles */

/* Add a new primitive id above here (this is will break v5 format)
 * NOTE: must update the non-geometric object id's below the
 * ADD_BELOW_HERE marker
 */
#define	ID_MAX_SOLID	44	/**< @brief Maximum defined ID_xxx for solids */

/*
 * Non-geometric objects
 */
#define ID_COMBINATION	31	/**< @brief Combination Record */
#define ID_UNUSED1	32	/**< @brief UNUSED (placeholder)  */
#define ID_BINUNIF	33	/**< @brief Uniform-array binary */
#define ID_UNUSED2	34	/**< @brief UNUSED (placeholder) */
#define ID_CONSTRAINT   39      /**< @brief Constraint object */

/* - ADD_BELOW_HERE - */
/* superellipsoid should be 31, but is not v5 compatible */
#define ID_SUPERELL	35	/**< @brief Superquadratic ellipsoid */
#define ID_METABALL	36	/**< @brief Metaball */
#define ID_BREP         37      /**< @brief B-rep object */
#define ID_HYP		38	/**< @brief Hyperboloid of one sheet */
#define ID_REVOLVE	40	/**< @brief Solid of Revolution */
#define ID_PNTS         41      /**< @brief Collection of Points */
#define ID_ANNOTATION   42      /**< @brief Annotation */
#define ID_HRT		43	/**< @brief Heart */

#define ID_MAXIMUM	44	/**< @brief Maximum defined ID_xxx value */

/**
 * Container for material information
 */
struct mater_info {
    float	ma_color[3];	/**< @brief explicit color:  0..1  */
    float	ma_temperature;	/**< @brief positive ==> degrees Kelvin */
    char	ma_color_valid;	/**< @brief non-0 ==> ma_color is non-default */
    char	ma_cinherit;	/**< @brief color: DB_INH_LOWER / DB_INH_HIGHER */
    char	ma_minherit;	/**< @brief mater: DB_INH_LOWER / DB_INH_HIGHER */
    char	*ma_shader;	/**< @brief shader name & parms */
};
#define RT_MATER_INFO_INIT_ZERO { VINIT_ZERO, 0.0, 0, 0, 0, NULL }


/**
 * The region structure.
 */
struct region {
    struct bu_list	l;		/**< @brief magic # and doubly linked list */
    const char *	reg_name;	/**< @brief Identifying string */
    union tree *	reg_treetop;	/**< @brief Pointer to boolean tree */
    int			reg_bit;	/**< @brief constant index into Regions[] */
    int			reg_regionid;	/**< @brief Region ID code.  If <=0, use reg_aircode */
    int			reg_aircode;	/**< @brief Region ID AIR code */
    int			reg_gmater;	/**< @brief GIFT Material code */
    int			reg_los;	/**< @brief approximate line-of-sight thickness equivalence */
    struct mater_info	reg_mater;	/**< @brief Real material information */
    void *		reg_mfuncs;	/**< @brief User appl. funcs for material */
    void *		reg_udata;	/**< @brief User appl. data for material */
    int			reg_transmit;	/**< @brief flag:  material transmits light */
    long		reg_instnum;	/**< @brief instance number, from d_uses */
    short		reg_all_unions;	/**< @brief 1=boolean tree is all unions */
    short		reg_is_fastgen;	/**< @brief FASTGEN-compatibility mode? */
#define REGION_NON_FASTGEN	0
#define REGION_FASTGEN_PLATE	1
#define REGION_FASTGEN_VOLUME	2
    struct bu_attribute_value_set attr_values;	/**< @brief Attribute/value set */
};
#define REGION_NULL	((struct region *)0)
#define RT_CK_REGION(_p) BU_CKMAG(_p, RT_REGION_MAGIC, "struct region")

/**
 * Partitions of a ray.  Passed from rt_shootray() into user's a_hit()
 * function.
 *
 * Not changed to a bu_list for backwards compatibility, but you can
 * iterate the whole list by writing:
 *
 * for (BU_LIST_FOR(pp, partition, (struct bu_list *)PartHeadp))
 */

struct partition {
    /* This can be thought of and operated on as a struct bu_list */
    uint32_t		pt_magic;	/**< @brief sanity check */
    struct partition *	pt_forw;	/**< @brief forwards link */
    struct partition *	pt_back;	/**< @brief backwards link */
    struct seg *	pt_inseg;	/**< @brief IN seg ptr (gives stp) */
    struct hit *	pt_inhit;	/**< @brief IN hit pointer */
    struct seg *	pt_outseg;	/**< @brief OUT seg pointer */
    struct hit *	pt_outhit;	/**< @brief OUT hit ptr */
    struct region *	pt_regionp;	/**< @brief ptr to containing region */
    char		pt_inflip;	/**< @brief flip inhit->hit_normal */
    char		pt_outflip;	/**< @brief flip outhit->hit_normal */
    struct region **	pt_overlap_reg;	/**< @brief NULL-terminated array of overlapping regions.  NULL if no overlap. */
    struct bu_ptbl	pt_seglist;	/**< @brief all segs in this partition */
};
#define PT_NULL		((struct partition *)0)

#define RT_CHECK_PT(_p) RT_CK_PT(_p)	/**< @brief compat */
#define RT_CK_PT(_p) BU_CKMAG(_p, PT_MAGIC, "struct partition")
#define RT_CK_PARTITION(_p) BU_CKMAG(_p, PT_MAGIC, "struct partition")
#define RT_CK_PT_HD(_p) BU_CKMAG(_p, PT_HD_MAGIC, "struct partition list head")

/* Macros for copying only the essential "middle" part of a partition struct */
#define RT_PT_MIDDLE_START	pt_inseg		/**< @brief 1st elem to copy */
#define RT_PT_MIDDLE_END	pt_seglist.l.magic	/**< @brief copy up to this elem (non-inclusive) */
#define RT_PT_MIDDLE_LEN(p) \
    (((char *)&(p)->RT_PT_MIDDLE_END) - ((char *)&(p)->RT_PT_MIDDLE_START))

#define RT_DUP_PT(ip, new, old, res) { \
	GET_PT(ip, new, res); \
	memcpy((char *)(&(new)->RT_PT_MIDDLE_START), (char *)(&(old)->RT_PT_MIDDLE_START), RT_PT_MIDDLE_LEN(old)); \
	(new)->pt_overlap_reg = NULL; \
	bu_ptbl_cat(&(new)->pt_seglist, &(old)->pt_seglist);  }

/** Clear out the pointers, empty the hit list */
#define GET_PT_INIT(ip, p, res) {\
	GET_PT(ip, p, res); \
	memset(((char *) &(p)->RT_PT_MIDDLE_START), 0, RT_PT_MIDDLE_LEN(p)); }

#define GET_PT(ip, p, res) { \
	if (BU_LIST_NON_EMPTY_P(p, partition, &res->re_parthead)) { \
	    BU_LIST_DEQUEUE((struct bu_list *)(p)); \
	    bu_ptbl_reset(&(p)->pt_seglist); \
	} else { \
	    BU_ALLOC((p), struct partition); \
	    (p)->pt_magic = PT_MAGIC; \
	    bu_ptbl_init(&(p)->pt_seglist, 42, "pt_seglist ptbl"); \
	    (res)->re_partlen++; \
	} \
	res->re_partget++; }

#define FREE_PT(p, res) { \
	BU_LIST_APPEND(&(res->re_parthead), (struct bu_list *)(p)); \
	if ((p)->pt_overlap_reg) { \
	    bu_free((void *)((p)->pt_overlap_reg), "pt_overlap_reg");\
	    (p)->pt_overlap_reg = NULL; \
	} \
	res->re_partfree++; }

#define RT_FREE_PT_LIST(_headp, _res) { \
	register struct partition *_pp, *_zap; \
	for (_pp = (_headp)->pt_forw; _pp != (_headp);) { \
	    _zap = _pp; \
	    _pp = _pp->pt_forw; \
	    BU_LIST_DEQUEUE((struct bu_list *)(_zap)); \
	    FREE_PT(_zap, _res); \
	} \
	(_headp)->pt_forw = (_headp)->pt_back = (_headp); \
    }

/** Insert "new" partition in front of "old" partition.  Note order change */
#define INSERT_PT(_new, _old) BU_LIST_INSERT((struct bu_list *)_old, (struct bu_list *)_new)

/** Append "new" partition after "old" partition.  Note arg order change */
#define APPEND_PT(_new, _old) BU_LIST_APPEND((struct bu_list *)_old, (struct bu_list *)_new)

/** Dequeue "cur" partition from doubly-linked list */
#define DEQUEUE_PT(_cur) BU_LIST_DEQUEUE((struct bu_list *)_cur)

/**
 * The partition_list structure - bu_list based structure for
 * holding ray bundles.
 *
 */
struct partition_list {
    struct bu_list l;
    struct application	*ap;
    struct partition PartHeadp;
    struct seg segHeadp;
    void *		userptr;
};


/**
 * Partition bundle.  Passed from rt_shootrays() into user's bundle_hit()
 * function.
 *
 */
struct partition_bundle {
    int hits;
    int misses;
    struct partition_list *list;
    struct application	*ap;
};


/**
 * Structures for space subdivision.
 *
 * cut_type is an integer for efficiency of access in rt_shootray() on
 * non-word addressing machines.
 *
 * If a solid has 'pieces', it will be listed either in bn_list
 * (initially), or in bn_piecelist, but not both.
 */
struct cutnode {
    int		cn_type;
    int		cn_axis;	        /**< @brief 0, 1, 2 = cut along X, Y, Z */
    fastf_t		cn_point;	/**< @brief cut through axis==point */
    union cutter *	cn_l;		/**< @brief val < point */
    union cutter *	cn_r;		/**< @brief val >= point */
};

struct boxnode {
    int		bn_type;
    fastf_t		bn_min[3];
    fastf_t		bn_max[3];
    struct soltab **bn_list;	        /**< @brief bn_list[bn_len] */
    size_t		bn_len;		/**< @brief # of solids in list */
    size_t		bn_maxlen;	/**< @brief # of ptrs allocated to list */
    struct rt_piecelist *bn_piecelist;  /**< @brief [] solids with pieces */
    size_t		bn_piecelen;	/**< @brief # of piecelists used */
    size_t		bn_maxpiecelen; /**< @brief # of piecelists allocated */
};

struct nu_axis {
    fastf_t	nu_spos;	/**< @brief cell start position */
    fastf_t	nu_epos;	/**< @brief cell end position */
    fastf_t	nu_width;	/**< @brief voxel size (end - start) */
};

struct nugridnode {
    int nu_type;
    struct nu_axis *nu_axis[3];
    int nu_cells_per_axis[3];	/**< @brief number of slabs */
    int nu_stepsize[3];		/**< @brief number of cells to jump for one step in each axis */
    union cutter *nu_grid;	/**< @brief 3-D array of boxnodes */
};

#define CUT_CUTNODE	1
#define CUT_BOXNODE	2
#define CUT_NUGRIDNODE	3
#define	CUT_MAXIMUM	3
union cutter {
    int	cut_type;
    union cutter *cut_forw;	/**< @brief Freelist forward link */
    struct cutnode cn;
    struct boxnode bn;
    struct nugridnode nugn;
};


#define CUTTER_NULL	((union cutter *)0)

/**
 * These structures are used to manage internal resource maps.
 * Typically these maps describe some kind of memory or file space.
 */
struct mem_map {
    struct mem_map *m_nxtp;	/**< @brief Linking pointer to next element */
    size_t m_size;		/**< @brief Size of this free element */
    off_t m_addr;		/**< @brief Address of start of this element */
};
#define MAP_NULL	((struct mem_map *) 0)


/**
 * DEPRECATED: external applications should use other LIBRT API to
 * access database objects.
 *
 * The directory is organized as forward linked lists hanging off of
 * one of RT_DBNHASH headers in the db_i structure.
 *
 * FIXME: this should not be public API, push container and iteration
 * down into LIBRT.  External applications should not use this.
 */
#define	RT_DBNHASH		8192	/**< @brief hash table is an
					 * array of linked lists with
					 * this many array pointer
					 * elements (Memory use for
					 * 32-bit: 32KB, 64-bit: 64KB)
					 */

#if	((RT_DBNHASH)&((RT_DBNHASH)-1)) != 0
/**
 * DEPRECATED: external applications should use other LIBRT API to
 * access database objects.
 */
#define	RT_DBHASH(sum)	((size_t)(sum) % (RT_DBNHASH))
#else
/**
 * DEPRECATED: external applications should use other LIBRT API to
 * access database objects.
 */
#define	RT_DBHASH(sum)	((size_t)(sum) & ((RT_DBNHASH)-1))
#endif


/**
 * One of these structures is used to describe each separate instance
 * of a BRL-CAD model database ".g" file.
 *
 * dbi_filepath is a C-style argv array of places to search when
 * opening related files (such as data files for EBM solids or
 * texture-maps).  The array and strings are all dynamically
 * allocated.
 *
 * Note that the current working units are specified as a conversion
 * factor to/from millimeters (they are the 'base' in local2base and
 * base2local) because database dimensional values are always stored
 * as millimeters (mm).  The units conversion factor only affects the
 * display and conversion of input values.  This helps prevent error
 * accumulation and improves numerical stability when calculations are
 * made.
 *
 */
struct db_i {
    uint32_t dbi_magic;		/**< @brief magic number */

    /* THESE ELEMENTS ARE AVAILABLE FOR APPLICATIONS TO READ */

    char * dbi_filename;		/**< @brief file name */
    int dbi_read_only;			/**< @brief !0 => read only file */
    double dbi_local2base;		/**< @brief local2mm */
    double dbi_base2local;		/**< @brief unit conversion factors */
    char * dbi_title;			/**< @brief title from IDENT rec */
    char ** dbi_filepath;		/**< @brief search path for aux file opens (convenience var) */

    /* THESE ELEMENTS ARE FOR LIBRT ONLY, AND MAY CHANGE */

    struct directory * dbi_Head[RT_DBNHASH]; /** @brief PRIVATE: object hash table */
    FILE * dbi_fp;			/**< @brief PRIVATE: standard file pointer */
    off_t dbi_eof;			/**< @brief PRIVATE: End+1 pos after db_scan() */
    size_t dbi_nrec;			/**< @brief PRIVATE: # records after db_scan() */
    int dbi_uses;			/**< @brief PRIVATE: # of uses of this struct */
    struct mem_map * dbi_freep;		/**< @brief PRIVATE: map of free granules */
    void *dbi_inmem;			/**< @brief PRIVATE: ptr to in-memory copy */
    struct animate * dbi_anroot;	/**< @brief PRIVATE: heads list of anim at root lvl */
    struct bu_mapped_file * dbi_mf;	/**< @brief PRIVATE: Only in read-only mode */
    struct bu_ptbl dbi_clients;		/**< @brief PRIVATE: List of rtip's using this db_i */
    int dbi_version;			/**< @brief PRIVATE: use db_version() */
    struct rt_wdb * dbi_wdbp;		/**< @brief PRIVATE: ptr back to containing rt_wdb */
};
#define DBI_NULL ((struct db_i *)0)
#define RT_CHECK_DBI(_p) BU_CKMAG(_p, DBI_MAGIC, "struct db_i")
#define RT_CK_DBI(_p) RT_CHECK_DBI(_p)


/**
 * One of these structures is allocated in memory to represent each
 * named object in the database.
 *
 * Note that a d_addr of RT_DIR_PHONY_ADDR ((off_t)-1) means that database
 * storage has not been allocated yet.
 *
 * Note that there is special handling for RT_DIR_INMEM "in memory"
 * overrides.
 *
 * Construction should be done only by using RT_GET_DIRECTORY()
 * Destruction should be done only by using db_dirdelete().
 *
 * Special note: In order to reduce the overhead of acquiring heap
 * memory (e.g., via bu_strdup()) to stash the name in d_namep, we
 * carry along enough storage for small names right in the structure
 * itself (d_shortname).  Thus, d_namep should never be assigned to
 * directly, it should always be accessed using RT_DIR_SET_NAMEP() and
 * RT_DIR_FREE_NAMEP().
 *
 * The in-memory name of an object should only be changed using
 * db_rename(), so that it can be requeued on the correct linked list,
 * based on new hash.  This should be followed by rt_db_put_internal()
 * on the object to modify the on-disk name.
 */
struct directory {
    uint32_t d_magic;	/**< @brief Magic number */
    char * d_namep;		/**< @brief pointer to name string */
    union {
	off_t file_offset;	/**< @brief disk address in obj file */
	void *ptr;		/**< @brief ptr to in-memory-only obj */
    } d_un;
    struct directory * d_forw;	/**< @brief link to next dir entry */
    struct animate * d_animate;	/**< @brief link to animation */
    long d_uses;		/**< @brief # uses, from instancing */
    size_t d_len;		/**< @brief # of db granules used */
    long d_nref;		/**< @brief # times ref'ed by COMBs */
    int d_flags;		/**< @brief flags */
    unsigned char d_major_type;	/**< @brief object major type */
    unsigned char d_minor_type;	/**< @brief object minor type */
    struct bu_list d_use_hd;	/**< @brief heads list of uses (struct soltab l2) */
    char d_shortname[16];	/**< @brief Stash short names locally */
};
#define RT_DIR_NULL	((struct directory *)0)
#define RT_CK_DIR(_dp) BU_CKMAG(_dp, RT_DIR_MAGIC, "(librt)directory")

#define d_addr	d_un.file_offset
#define RT_DIR_PHONY_ADDR	((off_t)-1)	/**< @brief Special marker for d_addr field */


/* flags for db_diradd() and friends */
#define RT_DIR_SOLID    0x1   /**< @brief this name is a solid */
#define RT_DIR_COMB     0x2   /**< @brief combination */
#define RT_DIR_REGION   0x4   /**< @brief region */
#define RT_DIR_HIDDEN   0x8   /**< @brief object name is hidden */
#define RT_DIR_NON_GEOM 0x10  /**< @brief object is not geometry (e.g. binary object) */
#define RT_DIR_USED     0x80  /**< @brief One bit, used similar to d_nref */
#define RT_DIR_INMEM    0x100 /**< @brief object is in memory (only) */

/**< @brief Args to db_lookup() */
#define LOOKUP_NOISY	1
#define LOOKUP_QUIET	0

#define FOR_ALL_DIRECTORY_START(_dp, _dbip) { int _i; \
	for (_i = RT_DBNHASH-1; _i >= 0; _i--) { \
	    for ((_dp) = (_dbip)->dbi_Head[_i]; (_dp); (_dp) = (_dp)->d_forw) {

#define FOR_ALL_DIRECTORY_END	}}}

#define RT_DIR_SET_NAMEP(_dp, _name) { \
	if (strlen(_name) < sizeof((_dp)->d_shortname)) {\
	    bu_strlcpy((_dp)->d_shortname, (_name), sizeof((_dp)->d_shortname)); \
	    (_dp)->d_namep = (_dp)->d_shortname; \
	} else { \
	    (_dp)->d_namep = bu_strdup(_name); /* Calls bu_malloc() */ \
	} }


/**
 * Use this macro to free the d_namep member, which is sometimes not
 * dynamic.
 */
#define RT_DIR_FREE_NAMEP(_dp) { \
	if ((_dp)->d_namep != (_dp)->d_shortname)  \
	    bu_free((_dp)->d_namep, "d_namep"); \
	(_dp)->d_namep = NULL; }


/**
 * allocate and link in a new directory entry to the resource
 * structure's freelist
 */
#define RT_GET_DIRECTORY(_p, _res) { \
	while (((_p) = (_res)->re_directory_hd) == NULL) \
	    db_alloc_directory_block(_res); \
	(_res)->re_directory_hd = (_p)->d_forw; \
	(_p)->d_forw = NULL; }


/**
 * In-memory format for database "combination" record (non-leaf node).
 * (Regions and Groups are both a kind of Combination).  Perhaps move
 * to wdb.h or rtgeom.h?
 */
struct rt_comb_internal {
    uint32_t		magic;
    union tree *	tree;		/**< @brief Leading to tree_db_leaf leaves */
    char		region_flag;	/**< @brief !0 ==> this COMB is a REGION */
    char		is_fastgen;	/**< @brief REGION_NON_FASTGEN/_PLATE/_VOLUME */
    /* Begin GIFT compatibility */
    long		region_id;      /* DEPRECATED, use attribute */
    long		aircode;        /* DEPRECATED, use attribute */
    long		GIFTmater;      /* DEPRECATED, use attribute */
    long		los;            /* DEPRECATED, use attribute */
    /* End GIFT compatibility */
    char		rgb_valid;	/**< @brief !0 ==> rgb[] has valid color */
    unsigned char	rgb[3];
    float		temperature;	/**< @brief > 0 ==> region temperature */
    struct bu_vls	shader;
    struct bu_vls	material;
    char		inherit;
};
#define RT_CHECK_COMB(_p) BU_CKMAG(_p, RT_COMB_MAGIC, "rt_comb_internal")
#define RT_CK_COMB(_p) RT_CHECK_COMB(_p)

/**
 * initialize an rt_comb_internal to empty.
 */
#define RT_COMB_INTERNAL_INIT(_p) { \
	(_p)->magic = RT_COMB_MAGIC; \
	(_p)->tree = TREE_NULL; \
	(_p)->region_flag = 0; \
	(_p)->is_fastgen = REGION_NON_FASTGEN; \
	(_p)->region_id = 0; \
	(_p)->aircode = 0; \
	(_p)->GIFTmater = 0; \
	(_p)->los = 0; \
	(_p)->rgb_valid = 0; \
	(_p)->rgb[0] = 0; \
	(_p)->rgb[1] = 0; \
	(_p)->rgb[2] = 0; \
	(_p)->temperature = 0.0; \
	BU_VLS_INIT(&(_p)->shader); \
	BU_VLS_INIT(&(_p)->material); \
	(_p)->inherit = 0; \
    }

/**
 * deinitialize an rt_comb_internal.  the tree pointer is not released
 * so callers will need to call RT_FREE_TREE() or db_free_tree()
 * directly if a tree is set.  the shader and material strings are
 * released.  the comb itself will need to be released with bu_free()
 * unless it resides on the stack.
 */
#define RT_FREE_COMB_INTERNAL(_p) { \
	bu_vls_free(&(_p)->shader); \
	bu_vls_free(&(_p)->material); \
	(_p)->tree = TREE_NULL; \
	(_p)->magic = 0; \
    }


/**
 * In-memory format for database uniform-array binary object.
 * Perhaps move to wdb.h or rtgeom.h?
 */
struct rt_binunif_internal {
    uint32_t		magic;
    int			type;
    size_t		count;
    union {
	float		*flt;
	double		*dbl;
	char		*int8;
	short		*int16;
	int	       	*int32;
	long		*int64;
	unsigned char	*uint8;
	unsigned short	*uint16;
	unsigned int	*uint32;
	unsigned long	*uint64;
    } u;
};
#define RT_CHECK_BINUNIF(_p) BU_CKMAG(_p, RT_BINUNIF_INTERNAL_MAGIC, "rt_binunif_internal")
#define RT_CK_BINUNIF(_p) RT_CHECK_BINUNIF(_p)


/**
 * In-memory format for database "constraint" record
 */
struct rt_constraint_internal {
    uint32_t magic;
    int id;
    int type;
    struct bu_vls expression;
};


#define RT_CHECK_CONSTRAINT(_p) BU_CKMAG(_p, PC_CONSTRAINT_MAGIC, "pc_constraint_internal")
#define RT_CK_CONSTRAINT(_p) PC_CHECK_CONSTRAINT(_p)


/**
 * State for database tree walker db_walk_tree() and related
 * user-provided handler routines.
 */
struct db_tree_state {
    uint32_t		magic;
    struct db_i	*	ts_dbip;
    int			ts_sofar;		/**< @brief Flag bits */

    int			ts_regionid;	/**< @brief GIFT compat region ID code*/
    int			ts_aircode;	/**< @brief GIFT compat air code */
    int			ts_gmater;	/**< @brief GIFT compat material code */
    int			ts_los;		/**< @brief equivalent LOS estimate */
    struct mater_info	ts_mater;	/**< @brief material properties */

    /* FIXME: ts_mat should be a matrix pointer, not a matrix */
    mat_t		ts_mat;		/**< @brief transform matrix */
    int			ts_is_fastgen;	/**< @brief REGION_NON_FASTGEN/_PLATE/_VOLUME */
    struct bu_attribute_value_set	ts_attrs;	/**< @brief attribute/value structure */

    int			ts_stop_at_regions;	/**< @brief else stop at solids */
    int			(*ts_region_start_func)(struct db_tree_state *tsp,
						const struct db_full_path *pathp,
						const struct rt_comb_internal *comb,
						void *client_data
	); /**< @brief callback during DAG downward traversal called on region nodes */
    union tree *	(*ts_region_end_func)(struct db_tree_state *tsp,
					      const struct db_full_path *pathp,
					      union tree *curtree,
					      void *client_data
	); /**< @brief callback during DAG upward traversal called on region nodes */
    union tree *	(*ts_leaf_func)(struct db_tree_state *tsp,
					const struct db_full_path *pathp,
					struct rt_db_internal *ip,
					void *client_data
	); /**< @brief callback during DAG traversal called on leaf primitive nodes */
    const struct rt_tess_tol *	ts_ttol;	/**< @brief  Tessellation tolerance */
    const struct bn_tol	*	ts_tol;		/**< @brief  Math tolerance */
    struct model **		ts_m;		/**< @brief  ptr to ptr to NMG "model" */
    struct rt_i *		ts_rtip;	/**< @brief  Helper for rt_gettrees() */
    struct resource *		ts_resp;	/**< @brief  Per-CPU data */
};
#define RT_DBTS_INIT_ZERO { RT_DBTS_MAGIC, NULL, 0, 0, 0, 0, 0, RT_MATER_INFO_INIT_ZERO, MAT_INIT_ZERO, 0, BU_AVS_INIT_ZERO, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }

#define TS_SOFAR_MINUS	1	/**< @brief  Subtraction encountered above */
#define TS_SOFAR_INTER	2	/**< @brief  Intersection encountered above */
#define TS_SOFAR_REGION	4	/**< @brief  Region encountered above */

#define RT_CK_DBTS(_p) BU_CKMAG(_p, RT_DBTS_MAGIC, "db_tree_state")

/**
 * State for database traversal functions.
 */
struct db_traverse
{
    uint32_t magic;
    struct db_i *dbip;
    void (*comb_enter_func) (
	struct db_i *,
	struct directory *,
	void *);
    void (*comb_exit_func) (
	struct db_i *,
	struct directory *,
	void *);
    void (*leaf_func) (
	struct db_i *,
	struct directory *,
	void *);
    struct resource *resp;
    void *client_data;
};
#define RT_DB_TRAVERSE_INIT(_p) {(_p)->magic = RT_DB_TRAVERSE_MAGIC;   \
	(_p)->dbip = ((void *)0); (_p)->comb_enter_func = ((void *)0); \
	(_p)->comb_exit_func = ((void *)0); (_p)->leaf_func = ((void *)0); \
	(_p)->resp = ((void *)0); (_p)->client_data = ((void *)0);}
#define RT_CK_DB_TRAVERSE(_p) BU_CKMAG(_p, RT_DB_TRAVERSE_MAGIC, "db_traverse")

struct combined_tree_state {
    uint32_t magic;
    struct db_tree_state	cts_s;
    struct db_full_path		cts_p;
};
#define RT_CK_CTS(_p) BU_CKMAG(_p, RT_CTS_MAGIC, "combined_tree_state")

/**
 * Binary trees representing the Boolean operations between solids.
 */
#define MKOP(x)		(x)

#define OP_SOLID	MKOP(1)		/**< @brief  Leaf:  tr_stp -> solid */
#define OP_UNION	MKOP(2)		/**< @brief  Binary: L union R */
#define OP_INTERSECT	MKOP(3)		/**< @brief  Binary: L intersect R */
#define OP_SUBTRACT	MKOP(4)		/**< @brief  Binary: L subtract R */
#define OP_XOR		MKOP(5)		/**< @brief  Binary: L xor R, not both*/
#define OP_REGION	MKOP(6)		/**< @brief  Leaf: tr_stp -> combined_tree_state */
#define OP_NOP		MKOP(7)		/**< @brief  Leaf with no effect */
/* Internal to library routines */
#define OP_NOT		MKOP(8)		/**< @brief  Unary:  not L */
#define OP_GUARD	MKOP(9)		/**< @brief  Unary:  not L, or else! */
#define OP_XNOP		MKOP(10)	/**< @brief  Unary:  L, mark region */
#define OP_NMG_TESS	MKOP(11)	/**< @brief  Leaf: tr_stp -> nmgregion */
/* LIBWDB import/export interface to combinations */
#define OP_DB_LEAF	MKOP(12)	/**< @brief  Leaf of combination, db fmt */
#define OP_FREE		MKOP(13)	/**< @brief  Unary:  L has free chain */

union tree {
    uint32_t magic;		/**< @brief  First word: magic number */
    /* Second word is always OP code */
    struct tree_node {
	uint32_t magic;
	int tb_op;			/**< @brief  non-leaf */
	struct region *tb_regionp;	/**< @brief  ptr to containing region */
	union tree *tb_left;
	union tree *tb_right;
    } tr_b;
    struct tree_leaf {
	uint32_t magic;
	int tu_op;			/**< @brief  leaf, OP_SOLID */
	struct region *tu_regionp;	/**< @brief  ptr to containing region */
	struct soltab *tu_stp;
    } tr_a;
    struct tree_cts {
	uint32_t magic;
	int tc_op;			/**< @brief  leaf, OP_REGION */
	struct region *tc_pad;		/**< @brief  unused */
	struct combined_tree_state *tc_ctsp;
    } tr_c;
    struct tree_nmgregion {
	uint32_t magic;
	int td_op;			/**< @brief  leaf, OP_NMG_TESS */
	const char *td_name;		/**< @brief  If non-null, dynamic string describing heritage of this region */
	struct nmgregion *td_r;		/**< @brief  ptr to NMG region */
    } tr_d;
    struct tree_db_leaf {
	uint32_t magic;
	int tl_op;			/**< @brief  leaf, OP_DB_LEAF */
	matp_t tl_mat;			/**< @brief  xform matp, NULL ==> identity */
	char *tl_name;			/**< @brief  Name of this leaf (bu_strdup'ed) */
    } tr_l;
};
/* Things which are in the same place in both A & B structures */
#define tr_op		tr_a.tu_op
#define tr_regionp	tr_a.tu_regionp

#define TREE_NULL	((union tree *)0)
#define RT_CK_TREE(_p) BU_CKMAG(_p, RT_TREE_MAGIC, "union tree")

/**
 * initialize a union tree to zero without a node operation set.  Use
 * the largest union so all values are effectively zero except for the
 * magic number.
 */
#define RT_TREE_INIT(_p) {		   \
	(_p)->magic = RT_TREE_MAGIC;	   \
	(_p)->tr_b.tb_op = 0;		   \
	(_p)->tr_b.tb_regionp = NULL;	   \
	(_p)->tr_b.tb_left = NULL;	   \
	(_p)->tr_b.tb_right = NULL;	   \
    }

/**
 * RT_GET_TREE returns a new initialized tree union pointer.  The
 * magic number is set to RT_TREE_MAGIC and all other members are
 * zero-initialized.
 *
 * This is a malloc-efficient BU_ALLOC(tp, union tree) replacement.
 * Previously used tree nodes are stored in the provided resource
 * pointer (during RT_FREE_TREE) as a single-linked list using the
 * tb_left field.  Requests for new nodes are pulled first from that
 * list or allocated fresh if needed.
 *
 * DEPRECATED, use BU_GET()
 */
#define RT_GET_TREE(_tp, _res) { \
	if (((_tp) = (_res)->re_tree_hd) != TREE_NULL) { \
	    (_res)->re_tree_hd = (_tp)->tr_b.tb_left;	 \
	    (_tp)->tr_b.tb_left = TREE_NULL;		 \
	    (_res)->re_tree_get++;			 \
	} else {					 \
	    BU_ALLOC(_tp, union tree);			 \
	    (_res)->re_tree_malloc++;			 \
	}						 \
	RT_TREE_INIT((_tp));				 \
    }

/**
 * RT_FREE_TREE deinitializes a tree union pointer.
 *
 * This is a malloc-efficient replacement for bu_free(tp).  Instead of
 * actually freeing the nodes, they are added to a single-linked list
 * in rt_tree_hd down the tb_left field.  Requests for new nodes (via
 * RT_GET_TREE()) pull from this list instead of allocating new nodes.
 *
 * DEPRECATED, use BU_PUT()
 */
#define RT_FREE_TREE(_tp, _res) { \
	(_tp)->magic = 0;			  \
	(_tp)->tr_b.tb_left = (_res)->re_tree_hd; \
	(_tp)->tr_b.tb_right = TREE_NULL;	  \
	(_res)->re_tree_hd = (_tp);		  \
	(_tp)->tr_b.tb_op = OP_FREE;		  \
	(_res)->re_tree_free++;			  \
    }


/**
 * flattened version of the union tree
 */
struct rt_tree_array
{
    union tree *tl_tree;
    int		tl_op;
};


#define TREE_LIST_NULL	((struct tree_list *)0)

/* FIXME: this is a dubious define that should be removed */
#define RT_MAXLINE		10240

/**
 * This data structure is at the core of the "LIBWDB" support for
 * allowing application programs to read and write BRL-CAD databases.
 * Many different access styles are supported.
 */

struct rt_wdb {
    struct bu_list	l;
    int			type;
    struct db_i	*	dbip;
    struct db_tree_state	wdb_initial_tree_state;
    struct rt_tess_tol	wdb_ttol;
    struct bn_tol	wdb_tol;
    struct resource*	wdb_resp;

    /* variables for name prefixing */
    struct bu_vls	wdb_prestr;
    int			wdb_ncharadd;
    int			wdb_num_dups;

    /* default region ident codes for this particular database. */
    int			wdb_item_default;/**< @brief  GIFT region ID */
    int			wdb_air_default;
    int			wdb_mat_default;/**< @brief  GIFT material code */
    int			wdb_los_default;/**< @brief  Line-of-sight estimate */

    /* These members are marked for removal */
    struct bu_vls	wdb_name;	/**< @brief  database object name */
    struct bu_observer	wdb_observers;
    Tcl_Interp *	wdb_interp;

};


#define RT_CHECK_WDB(_p) BU_CKMAG(_p, RT_WDB_MAGIC, "rt_wdb")
#define RT_CK_WDB(_p) RT_CHECK_WDB(_p)
#define RT_WDB_INIT_ZERO { {RT_WDB_MAGIC, BU_LIST_NULL, BU_LIST_NULL}, 0, NULL, RT_DBTS_INIT_ZERO, RT_TESS_TOL_INIT_ZERO, BN_TOL_INIT_ZERO, NULL, BU_VLS_INIT_ZERO, 0, 0, 0, 0, 0, 0, BU_VLS_INIT_ZERO, BU_OBSERVER_INIT_ZERO, NULL }
#define RT_WDB_NULL		((struct rt_wdb *)NULL)
#define RT_WDB_TYPE_DB_DISK			2
#define RT_WDB_TYPE_DB_DISK_APPEND_ONLY		3
#define RT_WDB_TYPE_DB_INMEM			4
#define RT_WDB_TYPE_DB_INMEM_APPEND_ONLY	5


#define RT_MINVIEWSIZE 0.0001
#define RT_MINVIEWSCALE 0.00005

/**
 * Each one of these structures specifies an arc in the tree that is
 * to be operated on for animation purposes.  More than one animation
 * operation may be applied at any given arc.  The directory structure
 * points to a linked list of animate structures (built by
 * rt_anim_add()), and the operations are processed in the order
 * given.
 */
struct anim_mat {
    int		anm_op;			/**< @brief  ANM_RSTACK, ANM_RARC... */
    mat_t	anm_mat;		/**< @brief  Matrix */
};
#define ANM_RSTACK	1		/**< @brief  Replace stacked matrix */
#define ANM_RARC	2		/**< @brief  Replace arc matrix */
#define ANM_LMUL	3		/**< @brief  Left (root side) mul */
#define ANM_RMUL	4		/**< @brief  Right (leaf side) mul */
#define ANM_RBOTH	5		/**< @brief  Replace stack, arc=Idn */

struct rt_anim_property {
    uint32_t magic;
    int			anp_op;		/**< @brief  RT_ANP_REPLACE, etc. */
    struct bu_vls	anp_shader;	/**< @brief  Update string */
};
#define RT_ANP_REPLACE	1		/**< @brief  Replace shader string */
#define RT_ANP_APPEND	2		/**< @brief  Append to shader string */
#define RT_CK_ANP(_p) BU_CKMAG((_p), RT_ANP_MAGIC, "rt_anim_property")

struct rt_anim_color {
    int anc_rgb[3];			/**< @brief  New color */
};


struct animate {
    uint32_t		magic;		/**< @brief  magic number */
    struct animate *	an_forw;	/**< @brief  forward link */
    struct db_full_path an_path;	/**< @brief  (sub)-path pattern */
    int			an_type;	/**< @brief  AN_MATRIX, AN_COLOR... */
    union animate_specific {
	struct anim_mat		anu_m;
	struct rt_anim_property	anu_p;
	struct rt_anim_color	anu_c;
	float			anu_t;
    } an_u;
};
#define RT_AN_MATRIX      1		/**< @brief  Matrix animation */
#define RT_AN_MATERIAL    2		/**< @brief  Material property anim */
#define RT_AN_COLOR       3		/**< @brief  Material color anim */
#define RT_AN_SOLID       4		/**< @brief  Solid parameter anim */
#define RT_AN_TEMPERATURE 5		/**< @brief  Region temperature */

#define ANIM_NULL	((struct animate *)0)
#define RT_CK_ANIMATE(_p) BU_CKMAG((_p), ANIMATE_MAGIC, "animate")

/**
 * Support for variable length arrays of "struct hit".
 * Patterned after the libbu/ptbl.c idea.
 */
struct rt_htbl {
    struct bu_list	l;	/**< @brief  linked list for caller's use */
    size_t		end;	/**< @brief  index of first available location */
    size_t		blen;	/**< @brief  # of struct's of storage at *hits */
    struct hit *	hits;	/**< @brief  hits[blen] data storage area */
};
#define RT_CK_HTBL(_p) BU_CKMAG(_p, RT_HTBL_MAGIC, "rt_htbl")

/**
 * Holds onto memory re-used by rt_shootray() from shot to shot.
 * One of these for each solid which uses pieces.
 * There is a separate array of these for each cpu.
 * Storage for the bit vectors is pre-allocated at prep time.
 * The array is subscripted by st_piecestate_num.
 * The bit vector is subscripted by values found in rt_piecelist pieces[].
 */
struct rt_piecestate {
    uint32_t		magic;
    long		ray_seqno;	/**< @brief  res_nshootray */
    struct soltab *	stp;
    struct bu_bitv *	shot;
    fastf_t		mindist;	/**< @brief  dist ray enters solids bounding volume */
    fastf_t		maxdist;	/**< @brief  dist ray leaves solids bounding volume */
    struct rt_htbl	htab;		/**< @brief  accumulating hits here */
    const union cutter *cutp;		/**< @brief  current bounding volume */
};
#define RT_CK_PIECESTATE(_p) BU_CKMAG(_p, RT_PIECESTATE_MAGIC, "struct rt_piecestate")

/**
 * For each space partitioning cell, there is one of these for each
 * solid in that cell which uses pieces.  Storage for the array is
 * allocated at cut time, and never changes.
 *
 * It is expected that the indices allocated by any solid range from
 * 0..(npieces-1).
 *
 * The piece indices are used as a subscript into a solid-specific
 * table, and also into the 'shot' bitv of the corresponding
 * rt_piecestate.
 *
 * The values (subscripts) in pieces[] are specific to a single solid
 * (stp).
 */
struct rt_piecelist {
    uint32_t		magic;
    size_t		npieces;	/**< @brief  number of pieces in pieces[] array */
    long		*pieces;	/**< @brief  pieces[npieces], piece indices */
    struct soltab	*stp;		/**< @brief  ref back to solid */
};
#define RT_CK_PIECELIST(_p) BU_CKMAG(_p, RT_PIECELIST_MAGIC, "struct rt_piecelist")

/* Used to set globals declared in bot.c */
#define RT_DEFAULT_MINPIECES		32
#define RT_DEFAULT_TRIS_PER_PIECE	4
#define RT_DEFAULT_MINTIE		0	/* disabled by default */

/**
 * Per-CPU statistics and resources.
 *
 * One of these structures is allocated per processor.  To prevent
 * excessive competition for free structures, memory is now allocated
 * on a per-processor basis.  The application structure a_resource
 * element specifies the resource structure to be used; if
 * uniprocessing, a null a_resource pointer results in using the
 * internal global structure (&rt_uniresource), making initial
 * application development simpler.
 *
 * Applications are responsible for calling rt_init_resource() for
 * each resource structure before letting LIBRT use them.
 *
 * Note that if multiple models are being used, the partition and bitv
 * structures (which are variable length) will require there to be
 * ncpus * nmodels resource structures, the selection of which will be
 * the responsibility of the application.
 *
 * Per-processor statistics are initially collected in here, and then
 * posted to rt_i by rt_add_res_stats().
 */
struct resource {
    uint32_t		re_magic;	/**< @brief  Magic number */
    int			re_cpu;		/**< @brief  processor number, for ID */
    struct bu_list 	re_seg;		/**< @brief  Head of segment freelist */
    struct bu_ptbl	re_seg_blocks;	/**< @brief  Table of malloc'ed blocks of segs */
    long		re_seglen;
    long		re_segget;
    long		re_segfree;
    struct bu_list	re_parthead;	/**< @brief  Head of freelist */
    long		re_partlen;
    long		re_partget;
    long		re_partfree;
    struct bu_list	re_solid_bitv;	/**< @brief  head of freelist */
    struct bu_list	re_region_ptbl;	/**< @brief  head of freelist */
    struct bu_list	re_nmgfree;	/**< @brief  head of NMG hitmiss freelist */
    union tree **	re_boolstack;	/**< @brief  Stack for rt_booleval() */
    long		re_boolslen;	/**< @brief  # elements in re_boolstack[] */
    float *		re_randptr;	/**< @brief  ptr into random number table */
    /* Statistics.  Only for examination by rt_add_res_stats() */
    long		re_nshootray;	/**< @brief  Calls to rt_shootray() */
    long		re_nmiss_model;	/**< @brief  Rays pruned by model RPP */
    /* Solid nshots = shot_hit + shot_miss */
    long		re_shots;	/**< @brief  # calls to ft_shot() */
    long		re_shot_hit;	/**< @brief  ft_shot() returned a miss */
    long		re_shot_miss;	/**< @brief  ft_shot() returned a hit */
    /* Optimizations.  Rays not shot at solids */
    long		re_prune_solrpp;/**< @brief  shot missed solid RPP, ft_shot skipped */
    long		re_ndup;	/**< @brief  ft_shot() calls skipped for already-ft_shot() solids */
    long		re_nempty_cells;	/**< @brief  number of empty NUgrid cells passed through */
    /* Data for accelerating "pieces" of solids */
    struct rt_piecestate *re_pieces;	/**< @brief  array [rti_nsolids_with_pieces] */
    long		re_piece_ndup;	/**< @brief  ft_piece_shot() calls skipped for already-ft_shot() solids */
    long		re_piece_shots;	/**< @brief  # calls to ft_piece_shot() */
    long		re_piece_shot_hit;	/**< @brief  ft_piece_shot() returned a miss */
    long		re_piece_shot_miss;	/**< @brief  ft_piece_shot() returned a hit */
    struct bu_ptbl	re_pieces_pending;	/**< @brief  pieces with an odd hit pending */
    /* Per-processor cache of tree unions, to accelerate "tops" and treewalk */
    union tree *	re_tree_hd;	/**< @brief  Head of free trees */
    long		re_tree_get;
    long		re_tree_malloc;
    long		re_tree_free;
    struct directory *	re_directory_hd;
    struct bu_ptbl	re_directory_blocks;	/**< @brief  Table of malloc'ed blocks */
};

/**
 * Resources for uniprocessor
 */
RT_EXPORT extern struct resource rt_uniresource;	/**< @brief  default.  Defined in librt/globals.c */
#define RESOURCE_NULL	((struct resource *)0)
#define RT_CK_RESOURCE(_p) BU_CKMAG(_p, RESOURCE_MAGIC, "struct resource")


/**
 * Structure used by the "reprep" routines
 */
struct rt_reprep_obj_list {
    size_t ntopobjs;		/**< @brief  number of objects in the original call to gettrees */
    char **topobjs;		/**< @brief  list of the above object names */
    size_t nunprepped;		/**< @brief  number of objects to be unprepped and re-prepped */
    char **unprepped;		/**< @brief  list of the above objects */
    /* Above here must be filled in by application */
    /* Below here is used by dynamic geometry routines, should be zeroed by application before use */
    struct bu_ptbl paths;	/**< @brief  list of all paths from topobjs to unprepped objects */
    struct db_tree_state **tsp;	/**< @brief  tree state used by tree walker in "reprep" routines */
    struct bu_ptbl unprep_regions;	/**< @brief  list of region structures that will be "unprepped" */
    size_t old_nsolids;		/**< @brief  rtip->nsolids before unprep */
    size_t old_nregions;	/**< @brief  rtip->nregions before unprep */
    size_t nsolids_unprepped;	/**< @brief  number of soltab structures eliminated by unprep */
    size_t nregions_unprepped;	/**< @brief  number of region structures eliminated by unprep */
};


/**
 * This structure is intended to describe the area and/or volume
 * represented by a ray.  In the case of the "rt" program it
 * represents the extent in model coordinates of the prism behind the
 * pixel being rendered.
 *
 * The r_pt values of the rays indicate the dimensions and location in
 * model space of the ray origin (usually the pixel to be rendered).
 * The r_dir vectors indicate the edges (and thus the shape) of the
 * prism which is formed from the projection of the pixel into space.
 */
#define CORNER_PTS 4
struct pixel_ext {
    uint32_t magic;
    struct xray	corner[CORNER_PTS];
};
/* This should have had an RT_ prefix */
#define BU_CK_PIXEL_EXT(_p) BU_CKMAG(_p, PIXEL_EXT_MAGIC, "struct pixel_ext")

/**
 * This structure is the only parameter to rt_shootray().  The entire
 * structure should be zeroed (e.g. by memset) before it is used the
 * first time.
 *
 * When calling rt_shootray(), these fields are mandatory:
 *
 *	- a_ray.r_pt	Starting point of ray to be fired
 *	- a_ray.r_dir	UNIT VECTOR with direction to fire in (dir cosines)
 *	- a_hit() Routine to call when something is hit
 *	- a_miss() Routine to call when ray misses everything
 *	- a_rt_i		Must be set to the value returned by rt_dirbuild().
 *
 * In addition, these fields are used by the library.  If they are set
 * to zero, default behavior will be used.
 *
 *	- a_resource	Pointer to CPU-specific resources.  Multi-CPU only.
 *	- a_overlap() DEPRECATED, set a_multioverlap() instead.
 *			If non-null, this routine will be called to
 *			handle overlap conditions.  See librt/bool.c
 *			for calling sequence.
 *			Return of 0 eliminates partition with overlap entirely
 *			Return of !0 retains one partition in output
 *	- a_multioverlap() Called when two or more regions overlap in a partition.
 *			Default behavior used if pointer not set.
 *			See librt/bool.c for calling sequence.
 *	- a_level		Printed by librt on errors, but otherwise not used.
 *	- a_x		Printed by librt on errors, but otherwise not used.
 *	- a_y		Printed by librt on errors, but otherwise not used.
 *	- a_purpose	Printed by librt on errors, but otherwise not used.
 *	- a_rbeam		Used to compute beam coverage on geometry,
 *	- a_diverge	for spline subdivision & many UV mappings.
 *
 *  Note that rt_shootray() returns the (int) return of the
 *  a_hit()/a_miss() function called, as well as placing it in
 *  a_return.  A future "multiple rays at a time" interface will only
 *  provide a_return.
 *
 *  Note that the organization of this structure, and the details of
 *  the non-mandatory elements are subject to change in every release.
 *  Therefore, rather than using compile-time structure
 *  initialization, you should create a zeroed-out structure, and then
 *  assign the intended values at runtime.  A zeroed structure can be
 *  obtained at compile time with "static struct application
 *  zero_ap;", or at run time by using memset(), bu_calloc(), or
 *  BU_ALLOC().
 */
struct application {
    uint32_t a_magic;
    /* THESE ELEMENTS ARE MANDATORY */
    struct xray		a_ray;		/**< @brief  Actual ray to be shot */
    int			(*a_hit)(struct application *, struct partition *, struct seg *);	/**< @brief  called when shot hits model */
    int			(*a_miss)(struct application *);	/**< @brief  called when shot misses */
    int			a_onehit;	/**< @brief  flag to stop on first hit */
    fastf_t		a_ray_length;	/**< @brief  distance from ray start to end intersections */
    struct rt_i	*	a_rt_i;		/**< @brief  this librt instance */
    int			a_zero1;	/**< @brief  must be zero (sanity check) */
    /* THESE ELEMENTS ARE USED BY THE LIBRARY, BUT MAY BE LEFT ZERO */
    struct resource *	a_resource;	/**< @brief  dynamic memory resources */
    int			(*a_overlap)(struct application *, struct partition *, struct region *, struct region *, struct partition *);	/**< @brief  DEPRECATED */
    void		(*a_multioverlap)(struct application *, struct partition *, struct bu_ptbl *, struct partition *);	/**< @brief  called to resolve overlaps */
    void		(*a_logoverlap)(struct application *, const struct partition *, const struct bu_ptbl *, const struct partition *);	/**< @brief  called to log overlaps */
    int			a_level;	/**< @brief  recursion level (for printing) */
    int			a_x;		/**< @brief  Screen X of ray, if applicable */
    int			a_y;		/**< @brief  Screen Y of ray, if applicable */
    const char *	a_purpose;	/**< @brief  Debug string:  purpose of ray */
    fastf_t		a_rbeam;	/**< @brief  initial beam radius (mm) */
    fastf_t		a_diverge;	/**< @brief  slope of beam divergence/mm */
    int			a_return;	/**< @brief  Return of a_hit()/a_miss() */
    int			a_no_booleans;	/**< @brief  1= partitions==segs, no booleans */
    char **		attrs;		/**< @brief  null terminated list of attributes
					 * This list should be the same as passed to
					 * rt_gettrees_and_attrs() */
    int			a_bot_reverse_normal_disabled;	/**< @brief  1= no bot normals get reversed in BOT_UNORIENTED_NORM */
    /* THESE ELEMENTS ARE USED BY THE PROGRAM "rt" AND MAY BE USED BY */
    /* THE LIBRARY AT SOME FUTURE DATE */
    /* AT THIS TIME THEY MAY BE LEFT ZERO */
    struct pixel_ext *	a_pixelext;	/**< @brief  locations of pixel corners */
    /* THESE ELEMENTS ARE WRITTEN BY THE LIBRARY, AND MAY BE READ IN a_hit() */
    struct seg *	a_finished_segs_hdp;
    struct partition *	a_Final_Part_hdp;
    vect_t		a_inv_dir;	/**< @brief  filled in by rt_shootray(), inverse of ray direction cosines */
    /* THE FOLLOWING ELEMENTS ARE MAINLINE & APPLICATION SPECIFIC. */
    /* THEY ARE NEVER EXAMINED BY THE LIBRARY. */
    int			a_user;		/**< @brief  application-specific value */
    void *		a_uptr;		/**< @brief  application-specific pointer */
    struct bn_tabdata *	a_spectrum;	/**< @brief  application-specific bn_tabdata pointer */
    fastf_t		a_color[3];	/**< @brief  application-specific color */
    fastf_t		a_dist;		/**< @brief  application-specific distance */
    vect_t		a_uvec;		/**< @brief  application-specific vector */
    vect_t		a_vvec;		/**< @brief  application-specific vector */
    fastf_t		a_refrac_index;	/**< @brief  current index of refraction */
    fastf_t		a_cumlen;	/**< @brief  cumulative length of ray */
    int			a_flag;		/**< @brief  application-specific flag */
    int			a_zero2;	/**< @brief  must be zero (sanity check) */
};


/**
 * This structure is the only parameter to rt_shootrays().  The entire
 * structure should be zeroed (e.g. by memset) before it is used the
 * first time.
 *
 * When calling rt_shootrays(), these fields are mandatory:
 *
 *	- b_ap		Members in this single ray application structure should be set
 *	 		in a similar fashion as when used with rt_shootray() with the
 *	 		exception of a_hit() and a_miss(). Default implementations of
 *	 		these routines are provided that simple update hit/miss counters
 *	 		and attach the hit partitions and segments to the
 *	 		partition_bundle structure. Users can still override this default
 *	 		functionality but have to make sure to move the partition and
 *	 		segment list to the new partition_bundle structure.
 *	- b_hit() Routine to call when something is hit by the ray bundle.
 *	- b_miss() Routine to call when ray bundle misses everything.
 *
 *  Note that rt_shootrays() returns the (int) return of the
 *  b_hit()/b_miss() function called, as well as placing it in
 *  b_return.
 *
 *  An integer field b_user and a void *field b_uptr are
 *  provided in the structure for custom user data.
 *
 */
struct application_bundle
{
    uint32_t b_magic;
    /* THESE ELEMENTS ARE MANDATORY */
    struct xrays b_rays; /**< @brief  Actual bundle of rays to be shot */
    struct application b_ap; /**< @brief  application setting to be applied to each ray */
    int (*b_hit)(struct application_bundle *, struct partition_bundle *); /**< @brief  called when bundle hits model */
    int (*b_miss)(struct application_bundle *); /**< @brief  called when entire bundle misses */
    int b_user; /**< @brief  application_bundle-specific value */
    void *b_uptr; /**< @brief  application_bundle-specific pointer */
    int b_return;
};


#define RT_APPLICATION_NULL ((struct application *)0)
#define RT_AFN_NULL	((int (*)(struct application *, struct partition *, struct region *, struct region *, struct partition *))NULL)
#define RT_CK_AP(_p) BU_CKMAG(_p, RT_AP_MAGIC, "struct application")
#define RT_CK_APPLICATION(_p) BU_CKMAG(_p, RT_AP_MAGIC, "struct application")
#define RT_APPLICATION_INIT(_p) { \
	memset((char *)(_p), 0, sizeof(struct application)); \
	(_p)->a_magic = RT_AP_MAGIC; \
    }


#ifdef NO_BOMBING_MACROS
#  define RT_AP_CHECK(_ap) BU_IGNORE((_ap))
#else
#  define RT_AP_CHECK(_ap)	\
    {if ((_ap)->a_zero1||(_ap)->a_zero2) \
	    bu_bomb("corrupt application struct"); }
#endif

/**
 * Definitions for librt.a which are global to the library regardless
 * of how many different models are being worked on
 */
struct rt_g {
    uint32_t		debug;		/**< @brief  !0 for debug, see librt/debug.h */
    /* DEPRECATED:  rtg_parallel is not used by LIBRT any longer (and will be removed) */
    int8_t		rtg_parallel;	/**< @brief  !0 = trying to use multi CPUs */
    struct bu_list	rtg_vlfree;	/**< @brief  head of bn_vlist freelist */
    uint32_t		NMG_debug;	/**< @brief  debug bits for NMG's see nmg.h */
    struct rt_wdb	rtg_headwdb;	/**< @brief  head of database object list */
};
#define RT_G_INIT_ZERO { 0, 0, BU_LIST_INIT_ZERO, 0, RT_WDB_INIT_ZERO }


/**
 * global ray-trace geometry state
 */
RT_EXPORT extern struct rt_g RTG;

/* Normally set when in production mode, setting the RT_G_DEBUG define
 * to 0 will allow chucks of code to poof away at compile time (since
 * they are truth-functionally constant (false)) This can boost
 * raytrace performance considerably (~10%).
 */
#ifdef NO_DEBUG_CHECKING
#  define RT_G_DEBUG 0
#else
#  define RT_G_DEBUG RTG.debug
#endif

/**
 * Definition of global parallel-processing semaphores.
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

#define RT_SEM_LAST	(RT_SEM_MODEL+1)	/**< @brief  Call bu_semaphore_init(RT_SEM_LAST); */


/**
 * @brief
 * This structure keeps track of almost everything for ray-tracing
 * support: Regions, primitives, model bounding box, statistics.
 *
 * Definitions for librt which are specific to the particular model
 * being processed, one copy for each model.  Initially, a pointer to
 * this is returned from rt_dirbuild().
 *
 * During gettree processing, the most time consuming step is
 * searching the list of existing solids to see if a new solid is
 * actually an identical instance of a previous solid.  Therefore, the
 * list has been divided into several lists.  The same macros & hash
 * value that accesses the dbi_Head[] array are used here.  The hash
 * value is computed by db_dirhash().
 */
struct rt_i {
    uint32_t		rti_magic;	/**< @brief  magic # for integrity check */
    /* THESE ITEMS ARE AVAILABLE FOR APPLICATIONS TO READ & MODIFY */
    int			useair;		/**< @brief  1="air" regions are retained while prepping */
    int			rti_save_overlaps; /**< @brief  1=fill in pt_overlap_reg, change boolweave behavior */
    int			rti_dont_instance; /**< @brief  1=Don't compress instances of solids into 1 while prepping */
    int			rti_hasty_prep;	/**< @brief  1=hasty prep, slower ray-trace */
    int			rti_nlights;	/**< @brief  number of light sources */
    int			rti_prismtrace; /**< @brief  add support for pixel prism trace */
    char *		rti_region_fix_file; /**< @brief  rt_regionfix() file or NULL */
    int			rti_space_partition;  /**< @brief  space partitioning method */
    int			rti_nugrid_dimlimit;  /**< @brief  limit on nugrid dimensions */
    struct bn_tol	rti_tol;	/**< @brief  Math tolerances for this model */
    struct rt_tess_tol	rti_ttol;	/**< @brief  Tessellation tolerance defaults */
    fastf_t		rti_max_beam_radius; /**< @brief  Max threat radius for FASTGEN cline solid */
    /* THESE ITEMS ARE AVAILABLE FOR APPLICATIONS TO READ */
    point_t		mdl_min;	/**< @brief  min corner of model bounding RPP */
    point_t		mdl_max;	/**< @brief  max corner of model bounding RPP */
    point_t		rti_pmin;	/**< @brief  for plotting, min RPP */
    point_t		rti_pmax;	/**< @brief  for plotting, max RPP */
    double		rti_radius;	/**< @brief  radius of model bounding sphere */
    struct db_i	*	rti_dbip;	/**< @brief  prt to Database instance struct */
    /* THESE ITEMS SHOULD BE CONSIDERED OPAQUE, AND SUBJECT TO CHANGE */
    int			needprep;	/**< @brief  needs rt_prep */
    struct region **	Regions;	/**< @brief  ptrs to regions [reg_bit] */
    struct bu_list	HeadRegion;	/**< @brief  ptr of list of regions in model */
    void *		Orca_hash_tbl;	/**< @brief  Hash table in matrices for ORCA */
    struct bu_ptbl	delete_regs;	/**< @brief  list of region pointers to delete after light_init() */
    /* Ray-tracing statistics */
    size_t		nregions;	/**< @brief  total # of regions participating */
    size_t		nsolids;	/**< @brief  total # of solids participating */
    size_t		rti_nrays;	/**< @brief  # calls to rt_shootray() */
    size_t		nmiss_model;	/**< @brief  rays missed model RPP */
    size_t		nshots;		/**< @brief  # of calls to ft_shot() */
    size_t		nmiss;		/**< @brief  solid ft_shot() returned a miss */
    size_t		nhits;		/**< @brief  solid ft_shot() returned a hit */
    size_t		nmiss_tree;	/**< @brief  shots missed sub-tree RPP */
    size_t		nmiss_solid;	/**< @brief  shots missed solid RPP */
    size_t		ndup;		/**< @brief  duplicate shots at a given solid */
    size_t		nempty_cells;	/**< @brief  number of empty NUgrid cells */
    union cutter	rti_CutHead;	/**< @brief  Head of cut tree */
    union cutter	rti_inf_box;	/**< @brief  List of infinite solids */
    union cutter *	rti_CutFree;	/**< @brief  cut Freelist */
    struct bu_ptbl	rti_busy_cutter_nodes; /**< @brief  List of "cutter" mallocs */
    struct bu_ptbl	rti_cuts_waiting;
    int			rti_cut_maxlen;	/**< @brief  max len RPP list in 1 cut bin */
    int			rti_ncut_by_type[CUT_MAXIMUM+1];	/**< @brief  number of cuts by type */
    int			rti_cut_totobj;	/**< @brief  # objs in all bins, total */
    int			rti_cut_maxdepth;/**< @brief  max depth of cut tree */
    struct soltab **	rti_sol_by_type[ID_MAX_SOLID+1];
    int			rti_nsol_by_type[ID_MAX_SOLID+1];
    int			rti_maxsol_by_type;
    int			rti_air_discards;/**< @brief  # of air regions discarded */
    struct bu_hist	rti_hist_cellsize; /**< @brief  occupancy of cut cells */
    struct bu_hist	rti_hist_cell_pieces; /**< @brief  solid pieces per cell */
    struct bu_hist	rti_hist_cutdepth; /**< @brief  depth of cut tree */
    struct soltab **	rti_Solids;	/**< @brief  ptrs to soltab [st_bit] */
    struct bu_list	rti_solidheads[RT_DBNHASH]; /**< @brief  active solid lists */
    struct bu_ptbl	rti_resources;	/**< @brief  list of 'struct resource's encountered */
    double		rti_nu_gfactor;	/**< @brief  constant in numcells computation */
    size_t		rti_cutlen;	/**< @brief  goal for # solids per boxnode */
    size_t		rti_cutdepth;	/**< @brief  goal for depth of NUBSPT cut tree */
    /* Parameters required for rt_submodel */
    char *		rti_treetop;	/**< @brief  bu_strduped, for rt_submodel rti's only */
    size_t		rti_uses;	/**< @brief  for rt_submodel */
    /* Parameters for accelerating "pieces" of solids */
    size_t		rti_nsolids_with_pieces; /**< @brief  # solids using pieces */
    /* Parameters for dynamic geometry */
    int			rti_add_to_new_solids_list;
    struct bu_ptbl	rti_new_solids;
};


#define RT_NU_GFACTOR_DEFAULT	1.5	 /**< @brief  see rt_cut_it() for a description
					    of this */

#define RTI_NULL	((struct rt_i *)0)

#define RT_CHECK_RTI(_p) BU_CKMAG(_p, RTI_MAGIC, "struct rt_i")
#define RT_CK_RTI(_p) RT_CHECK_RTI(_p)

#define	RT_PART_NUBSPT	0
#define RT_PART_NUGRID	1

/**
 * Macros to painlessly visit all the active solids.  Serving suggestion:
 *
 * RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
 *	rt_pr_soltab(stp);
 * } RT_VISIT_ALL_SOLTABS_END
 */
#define RT_VISIT_ALL_SOLTABS_START(_s, _rti) { \
	register struct bu_list	*_head = &((_rti)->rti_solidheads[0]); \
	for (; _head < &((_rti)->rti_solidheads[RT_DBNHASH]); _head++) \
	    for (BU_LIST_FOR(_s, soltab, _head)) {

#define RT_VISIT_ALL_SOLTABS_END	} }

/**
 * Applications that are going to use RT_ADD_VLIST and RT_GET_VLIST
 * are required to execute this macro once, first:
 *
 * BU_LIST_INIT(&RTG.rtg_vlfree);
 *
 * Note that RT_GET_VLIST and RT_FREE_VLIST are non-PARALLEL.
 */
#define RT_GET_VLIST(p) BN_GET_VLIST(&RTG.rtg_vlfree, p)

/** Place an entire chain of bn_vlist structs on the freelist */
#define RT_FREE_VLIST(hd) BN_FREE_VLIST(&RTG.rtg_vlfree, hd)

#define RT_ADD_VLIST(hd, pnt, draw) BN_ADD_VLIST(&RTG.rtg_vlfree, hd, pnt, draw)

/** Set a point size to apply to the vlist elements that follow. */
#define RT_VLIST_SET_POINT_SIZE(hd, size) BN_VLIST_SET_POINT_SIZE(&RTG.rtg_vlfree, hd, size)

/** Set a line width to apply to the vlist elements that follow. */
#define RT_VLIST_SET_LINE_WIDTH(hd, width) BN_VLIST_SET_LINE_WIDTH(&RTG.rtg_vlfree, hd, width)


/*
 * Replacements for definitions from vmath.h
 */
#undef V2PRINT
#undef VPRINT
#undef HPRINT
#define V2PRINT(a, b) bu_log("%s (%g, %g)\n", a, (b)[0], (b)[1]);
#define VPRINT(a, b) bu_log("%s (%g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2])
#define HPRINT(a, b) bu_log("%s (%g, %g, %g, %g)\n", a, (b)[0], (b)[1], (b)[2], (b)[3])

/**
 * Table for driving generic command-parsing routines
 */
struct command_tab {
    const char *ct_cmd;
    const char *ct_parms;
    const char *ct_comment;
    int	(*ct_func)(const int, const char **);
    int	ct_min;		/**< @brief  min number of words in cmd */
    int	ct_max;		/**< @brief  max number of words in cmd */
};


/**
 * Used by MGED for labeling vertices of a solid.
 */
struct rt_point_labels {
    char str[8];
    point_t pt;
};


/**
 * Used by rpc.c, ehy.c, epa.c, eto.c and rhc.c
 * to contain forward-linked lists of points.
 */
struct rt_pt_node {
    point_t p;			/**< @brief  a point */
    struct rt_pt_node *next;	/**< @brief  ptr to next pt */
};

/**
 * Normally, librt doesn't have a concept of a "display"
 * of the geometry.  However for at least the plotting routines,
 * view information is sometimes needed to produce more intelligent
 * output.  In those situations, the application should
 * populate and pass an rt_view_info struct.
 *
 * **TODO** this structure is NOT in final form and should not
 * be relied upon.
 */
struct rt_view_info {
    struct bu_list *vhead;
    const struct bn_tol *tol;

    /** The average distance between the segment points of plotted curves.
     * Smaller spacing means more points per curve, and thus smoother (more
     * accurate) plot curves.
     */
    fastf_t point_spacing;

    /** The average distance between plotted surface curves.
     * Smaller spacing means more curves are drawn, increasing the overall
     * density of the plot.
     */
    fastf_t curve_spacing;
};

/**
 * Specifies a subset of a primitive's geometry as the target for
 * an operation.
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection {
    void *obj; /**< @brief primitive-specific selection object */
};

/**
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection_set {
    struct bu_ptbl selections; /**< @brief holds struct rt_selection */

    /** selection-object-specific routine that will free all memory
     *  associated with any of the stored selections
     */
    void (*free_selection)(struct rt_selection *);
};

/**
 * Stores selections associated with an object. There is an entry in
 * the selections table for each kind of selection (e.g. "active",
 * "option"). The table entries are sets to allow more than one
 * selection of the same type (e.g. multiple "option" selections).
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_object_selections {
    /** selection type -> struct rt_selection_set */
    struct bu_hash_tbl *sets;
};

/**
 * Analogous to a database query. Specifies how to filter and sort
 * the selectable components of a primitive in order to find the most
 * relevant selections for a particular application.
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection_query {
    point_t start;     /**< @brief start point of query ray */
    vect_t dir;        /**< @brief direction of query ray */

#define RT_SORT_UNSORTED	 0
#define RT_SORT_CLOSEST_TO_START 1
    int sorting;
};

/**
 * Parameters of a translation applied to a selection.
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection_translation {
    fastf_t dx;
    fastf_t dy;
    fastf_t dz;
};

/**
 * Describes an operation that can be applied to a selection.
 *
 * TODO: This structure is tentative and subject to change or removal
 *       without notice.
 */
struct rt_selection_operation {
#define RT_SELECTION_NOP	 0
#define RT_SELECTION_TRANSLATION 1
    int type;
    union {
	struct rt_selection_translation tran;
    } parameters;
};

/**
 * Object-oriented interface to BRL-CAD geometry.
 *
 * These are the methods for a notional object class "brlcad_solid".
 * The data for each instance is found separately in struct soltab.
 * This table is indexed by ID_xxx value of particular solid found in
 * st_id, or directly pointed at by st_meth.
 *
 * This needs to be at the end of the raytrace.h header file, so that
 * all the structure names are known.  The "union record" and "struct
 * nmgregion" pointers are problematic, so generic pointers are used
 * when those header files have not yet been seen.
 *
 * DEPRECATED: the size of this structure will likely change with new
 * size for ft_label and new object callbacks.
 */
struct rt_functab {
    uint32_t magic;
    char ft_name[17]; /* current longest name is 16 chars, need one element for terminating NULL */
    char ft_label[9]; /* current longest label is 8 chars, need one element for terminating NULL */

    int ft_use_rpp;
    int (*ft_prep)(struct soltab *stp,
		   struct rt_db_internal *ip,
		   struct rt_i *rtip);
#define RTFUNCTAB_FUNC_PREP_CAST(_func) ((int (*)(struct soltab *, struct rt_db_internal *, struct rt_i *))_func)

    int (*ft_shot)(struct soltab *stp,
		   struct xray *rp,
		   struct application *ap, /* has resource */
		   struct seg *seghead);
#define RTFUNCTAB_FUNC_SHOT_CAST(_func) ((int (*)(struct soltab *, struct xray *, struct application *, struct seg *))_func)

    void (*ft_print)(const struct soltab *stp);
#define RTFUNCTAB_FUNC_PRINT_CAST(_func) ((void (*)(const struct soltab *))_func)

    void (*ft_norm)(struct hit *hitp,
		    struct soltab *stp,
		    struct xray *rp);
#define RTFUNCTAB_FUNC_NORM_CAST(_func) ((void (*)(struct hit *, struct soltab *, struct xray *))_func)

    int (*ft_piece_shot)(struct rt_piecestate *psp,
			 struct rt_piecelist *plp,
			 double dist, /* correction to apply to hit distances */
			 struct xray *ray, /* ray transformed to be near cut cell */
			 struct application *ap, /* has resource */
			 struct seg *seghead);	/* used only for PLATE mode hits */
#define RTFUNCTAB_FUNC_PIECE_SHOT_CAST(_func) ((int (*)(struct rt_piecestate *, struct rt_piecelist *, double dist, struct xray *, struct application *, struct seg *))_func)

    void (*ft_piece_hitsegs)(struct rt_piecestate *psp,
			     struct seg *seghead,
			     struct application *ap); /* has resource */
#define RTFUNCTAB_FUNC_PIECE_HITSEGS_CAST(_func) ((void (*)(struct rt_piecestate *, struct seg *, struct application *))_func)

    void (*ft_uv)(struct application *ap, /* has resource */
		  struct soltab *stp,
		  struct hit *hitp,
		  struct uvcoord *uvp);
#define RTFUNCTAB_FUNC_UV_CAST(_func) ((void (*)(struct application *, struct soltab *, struct hit *, struct uvcoord *))_func)

    void (*ft_curve)(struct curvature *cvp,
		     struct hit *hitp,
		     struct soltab *stp);
#define RTFUNCTAB_FUNC_CURVE_CAST(_func) ((void (*)(struct curvature *, struct hit *, struct soltab *))_func)

    int (*ft_classify)(const struct soltab * /*stp*/, const vect_t /*min*/, const vect_t /*max*/, const struct bn_tol * /*tol*/);
#define RTFUNCTAB_FUNC_CLASS_CAST(_func) ((int (*)(const struct soltab *, const vect_t, const vect_t, const struct bn_tol *))_func)

    void (*ft_free)(struct soltab * /*stp*/);
#define RTFUNCTAB_FUNC_FREE_CAST(_func) ((void (*)(struct soltab *))_func)

    int (*ft_plot)(struct bu_list * /*vhead*/,
		   struct rt_db_internal * /*ip*/,
		   const struct rt_tess_tol * /*ttol*/,
		   const struct bn_tol * /*tol*/,
		   const struct rt_view_info * /*view info*/);
#define RTFUNCTAB_FUNC_PLOT_CAST(_func) ((int (*)(struct bu_list *, struct rt_db_internal *, const struct rt_tess_tol *, const struct bn_tol *, const struct rt_view_info *))_func)

    int (*ft_adaptive_plot)(struct rt_db_internal * /*ip*/,
		   const struct rt_view_info * /*view info*/);
#define RTFUNCTAB_FUNC_ADAPTIVE_PLOT_CAST(_func) ((int (*)(struct rt_db_internal *, const struct rt_view_info *))_func)

    void (*ft_vshot)(struct soltab * /*stp*/[],
		     struct xray *[] /*rp*/,
		     struct seg * /*segp*/,
		     int /*n*/,
		     struct application * /*ap*/);
#define RTFUNCTAB_FUNC_VSHOT_CAST(_func) ((void (*)(struct soltab *[], struct xray *[], struct seg *, int, struct application *))_func)

    int (*ft_tessellate)(struct nmgregion ** /*r*/,
			 struct model * /*m*/,
			 struct rt_db_internal * /*ip*/,
			 const struct rt_tess_tol * /*ttol*/,
			 const struct bn_tol * /*tol*/);
#define RTFUNCTAB_FUNC_TESS_CAST(_func) ((int (*)(struct nmgregion **, struct model *, struct rt_db_internal *, const struct rt_tess_tol *, const struct bn_tol *))_func)
    int (*ft_tnurb)(struct nmgregion ** /*r*/,
		    struct model * /*m*/,
		    struct rt_db_internal * /*ip*/,
		    const struct bn_tol * /*tol*/);
#define RTFUNCTAB_FUNC_TNURB_CAST(_func) ((int (*)(struct nmgregion **, struct model *, struct rt_db_internal *, const struct bn_tol *))_func)

    void (*ft_brep)(ON_Brep ** /*b*/,
		    struct rt_db_internal * /*ip*/,
		    const struct bn_tol * /*tol*/);
#define RTFUNCTAB_FUNC_BREP_CAST(_func) ((void (*)(ON_Brep **, struct rt_db_internal *, const struct bn_tol *))_func)

    int (*ft_import5)(struct rt_db_internal * /*ip*/,
		      const struct bu_external * /*ep*/,
		      const mat_t /*mat*/,
		      const struct db_i * /*dbip*/,
		      struct resource * /*resp*/);
#define RTFUNCTAB_FUNC_IMPORT5_CAST(_func) ((int (*)(struct rt_db_internal *, const struct bu_external *, const mat_t, const struct db_i *, struct resource *))_func)

    int (*ft_export5)(struct bu_external * /*ep*/,
		      const struct rt_db_internal * /*ip*/,
		      double /*local2mm*/,
		      const struct db_i * /*dbip*/,
		      struct resource * /*resp*/);
#define RTFUNCTAB_FUNC_EXPORT5_CAST(_func) ((int (*)(struct bu_external *, const struct rt_db_internal *, double, const struct db_i *, struct resource *))_func)

    int (*ft_import4)(struct rt_db_internal * /*ip*/,
		      const struct bu_external * /*ep*/,
		      const mat_t /*mat*/,
		      const struct db_i * /*dbip*/,
		      struct resource * /*resp*/);
#define RTFUNCTAB_FUNC_IMPORT4_CAST(_func) ((int (*)(struct rt_db_internal *, const struct bu_external *, const mat_t, const struct db_i *, struct resource *))_func)

    int	(*ft_export4)(struct bu_external * /*ep*/,
		      const struct rt_db_internal * /*ip*/,
		      double /*local2mm*/,
		      const struct db_i * /*dbip*/,
		      struct resource * /*resp*/);
#define RTFUNCTAB_FUNC_EXPORT4_CAST(_func) ((int (*)(struct bu_external *, const struct rt_db_internal *, double, const struct db_i *, struct resource *))_func)

    void (*ft_ifree)(struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_IFREE_CAST(_func) ((void (*)(struct rt_db_internal *))_func)

    int	(*ft_describe)(struct bu_vls * /*str*/,
		       const struct rt_db_internal * /*ip*/,
		       int /*verbose*/,
		       double /*mm2local*/,
		       struct resource * /*resp*/,
		       struct db_i *);
#define RTFUNCTAB_FUNC_DESCRIBE_CAST(_func) ((int (*)(struct bu_vls *, const struct rt_db_internal *, int, double, struct resource *, struct db_i *))_func)

    int	(*ft_xform)(struct rt_db_internal * /*op*/,
		    const mat_t /*mat*/, struct rt_db_internal * /*ip*/,
		    int /*free*/, struct db_i * /*dbip*/,
		    struct resource * /*resp*/);
#define RTFUNCTAB_FUNC_XFORM_CAST(_func) ((int (*)(struct rt_db_internal *, const mat_t, struct rt_db_internal *, int, struct db_i *, struct resource *))_func)

    const struct bu_structparse *ft_parsetab;	/**< @brief  rt_xxx_parse */
    size_t ft_internal_size;	/**< @brief  sizeof(struct rt_xxx_internal) */
    uint32_t ft_internal_magic;	/**< @brief  RT_XXX_INTERNAL_MAGIC */

    int	(*ft_get)(struct bu_vls *, const struct rt_db_internal *, const char *item);
#define RTFUNCTAB_FUNC_GET_CAST(_func) ((int (*)(struct bu_vls *, const struct rt_db_internal *, const char *))_func)

    int	(*ft_adjust)(struct bu_vls *, struct rt_db_internal *, int /*argc*/, const char ** /*argv*/);
#define RTFUNCTAB_FUNC_ADJUST_CAST(_func) ((int (*)(struct bu_vls *, struct rt_db_internal *, int, const char **))_func)

    int	(*ft_form)(struct bu_vls *, const struct rt_functab *);
#define RTFUNCTAB_FUNC_FORM_CAST(_func) ((int (*)(struct bu_vls *, const struct rt_functab *))_func)

    void (*ft_make)(const struct rt_functab *, struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_MAKE_CAST(_func) ((void (*)(const struct rt_functab *, struct rt_db_internal *))_func)

    int (*ft_params)(struct pc_pc_set *, const struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_PARAMS_CAST(_func) ((int (*)(struct pc_pc_set *, const struct rt_db_internal *))_func)

    /* Axis aligned bounding box */
    int (*ft_bbox)(struct rt_db_internal * /*ip*/,
		   point_t * /*min X, Y, Z of bounding RPP*/,
		   point_t * /*max X, Y, Z of bounding RPP*/,
		   const struct bn_tol *);
#define RTFUNCTAB_FUNC_BBOX_CAST(_func) ((int (*)(struct rt_db_internal *, point_t *, point_t *, const struct bn_tol *))_func)

    void (*ft_volume)(fastf_t * /*vol*/, const struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_VOLUME_CAST(_func) ((void (*)(fastf_t *, const struct rt_db_internal *))_func)

    void (*ft_surf_area)(fastf_t * /*area*/, const struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_SURF_AREA_CAST(_func) ((void (*)(fastf_t *, const struct rt_db_internal *))_func)

    void (*ft_centroid)(point_t * /*cent*/, const struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_CENTROID_CAST(_func) ((void (*)(point_t *, const struct rt_db_internal *))_func)

    int (*ft_oriented_bbox)(struct rt_arb_internal * /* bounding arb8 */,
		   struct rt_db_internal * /*ip*/,
		   const fastf_t);
#define RTFUNCTAB_FUNC_ORIENTED_BBOX_CAST(_func) ((int (*)(struct rt_arb_internal *, struct rt_db_internal *, const fastf_t))_func)

    /** get a list of the selections matching a query */
    struct rt_selection_set *(*ft_find_selections)(const struct rt_db_internal *,
						   const struct rt_selection_query *);
#define RTFUNCTAB_FUNC_FIND_SELECTIONS_CAST(_func) ((struct rt_selection_set *(*)(const struct rt_db_internal *, const struct rt_selection_query *))_func)

    /** evaluate a logical selection expression (e.g. a INTERSECT b,
     *  NOT a) to create a new selection
     */
    struct rt_selection *(*ft_evaluate_selection)(const struct rt_db_internal *,
						 int op,
						 const struct rt_selection *,
						 const struct rt_selection *);
#define RTFUNCTAB_FUNC_EVALUATE_SELECTION_CAST(_func) ((struct rt_selection *(*)(const struct rt_db_internal *, int op, const struct rt_selection *, const struct rt_selection *))_func)

    /** apply an operation to a selected subset of a primitive */
    int (*ft_process_selection)(struct rt_db_internal *,
				const struct rt_selection *,
				const struct rt_selection_operation *);
#define RTFUNCTAB_FUNC_PROCESS_SELECTION_CAST(_func) ((int (*)(struct rt_db_internal *, const struct rt_selection *, const struct rt_selection_operation *))_func)

};


RT_EXPORT extern const struct rt_functab OBJ[];

#define RT_CK_FUNCTAB(_p) BU_CKMAG(_p, RT_FUNCTAB_MAGIC, "functab");


/**
 * Internal to shoot.c and bundle.c
 */
struct rt_shootray_status {
    fastf_t		dist_corr;	/**< @brief  correction distance */
    fastf_t		odist_corr;
    fastf_t		box_start;
    fastf_t		obox_start;
    fastf_t		box_end;
    fastf_t		obox_end;
    fastf_t		model_start;
    fastf_t		model_end;
    struct xray		newray;		/**< @brief  closer ray start */
    struct application *ap;
    struct resource *	resp;
    vect_t		inv_dir;      /**< @brief  inverses of ap->a_ray.r_dir */
    vect_t		abs_inv_dir;  /**< @brief  absolute values of inv_dir */
    int			rstep[3];     /**< @brief  -/0/+ dir of ray in axis */
    const union cutter *lastcut, *lastcell;
    const union cutter *curcut;
    point_t		curmin, curmax;
    int			igrid[3];     /**< @brief  integer cell coordinates */
    vect_t		tv;	      /**< @brief  next t intercept values */
    int			out_axis;     /**< @brief  axis ray will leave through */
    struct rt_shootray_status *old_status;
    int			box_num;	/**< @brief  which cell along ray */
};


#define NUGRID_T_SETUP(_ax, _cval, _cno) \
    if (ssp->rstep[_ax] > 0) { \
	ssp->tv[_ax] = t0 + (nu_axis[_ax][_cno].nu_epos - _cval) * \
	    ssp->inv_dir[_ax]; \
    } else if (ssp->rstep[_ax] < 0) { \
	ssp->tv[_ax] = t0 + (nu_axis[_ax][_cno].nu_spos - _cval) * \
	    ssp->inv_dir[_ax]; \
    } else { \
	ssp->tv[_ax] = INFINITY; \
    }
#define NUGRID_T_ADV(_ax, _cno) \
    if (ssp->rstep[_ax] != 0) { \
	ssp->tv[_ax] += nu_axis[_ax][_cno].nu_width * \
	    ssp->abs_inv_dir[_ax]; \
    }

#define BACKING_DIST	(-2.0)		/**< @brief  mm to look behind start point */
#define OFFSET_DIST	0.01		/**< @brief  mm to advance point into box */

/*********************************************************************************
 *	The following section is an exact copy of what was previously "nmg_rt.h" *
 *      (with minor changes to NMG_GET_HITMISS and NMG_FREE_HITLIST              *
 *	moved here to use RTG.rtg_nmgfree freelist for hitmiss structs.         *
 ******************************************************************************* */

#define NMG_HIT_LIST	0
#define NMG_MISS_LIST	1


/* These values are for the hitmiss "in_out" variable and indicate the
 * nature of the hit when known
 */
#define HMG_INBOUND_STATE(_hm) (((_hm)->in_out & 0x0f0) >> 4)
#define HMG_OUTBOUND_STATE(_hm) ((_hm)->in_out & 0x0f)


#define NMG_RAY_STATE_INSIDE	1
#define NMG_RAY_STATE_ON	2
#define NMG_RAY_STATE_OUTSIDE	4
#define NMG_RAY_STATE_ANY	8

#define HMG_HIT_IN_IN	0x11	/**< @brief  hit internal structure */
#define HMG_HIT_IN_OUT	0x14	/**< @brief  breaking out */
#define HMG_HIT_OUT_IN	0x41	/**< @brief  breaking in */
#define HMG_HIT_OUT_OUT 0x44	/**< @brief  edge/vertex graze */
#define HMG_HIT_IN_ON	0x12
#define HMG_HIT_ON_IN	0x21
#define HMG_HIT_ON_ON	0x22
#define HMG_HIT_OUT_ON	0x42
#define HMG_HIT_ON_OUT	0x24
#define HMG_HIT_ANY_ANY	0x88	/**< @brief  hit on non-3-manifold */

#define	NMG_VERT_ENTER 1
#define NMG_VERT_ENTER_LEAVE 0
#define NMG_VERT_LEAVE -1
#define NMG_VERT_UNKNOWN -2

#define NMG_HITMISS_SEG_IN 0x696e00	/**< @brief  "in" */
#define NMG_HITMISS_SEG_OUT 0x6f757400	/**< @brief  "out" */

struct hitmiss {
    struct bu_list	l;
    struct hit		hit;
    fastf_t		dist_in_plane;	/**< @brief  distance from plane intersect */
    int			in_out;		/**< @brief  status of ray as it transitions
					 * this hit point.
					 */
    long		*inbound_use;
    vect_t		inbound_norm;
    long		*outbound_use;
    vect_t		outbound_norm;
    int			start_stop;	/**< @brief  is this a seg_in or seg_out */
    struct hitmiss	*other;		/**< @brief  for keeping track of the other
					 * end of the segment when we know
					 * it
					 */
};


#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS(hm) BU_IGNORE((hm))
#else
#  define NMG_CK_HITMISS(hm) \
    {\
	switch (hm->l.magic) { \
	    case NMG_RT_HIT_MAGIC: \
	    case NMG_RT_HIT_SUB_MAGIC: \
	    case NMG_RT_MISS_MAGIC: \
		break; \
	    case NMG_MISS_LIST: \
		bu_log(BU_FLSTR ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
		bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	    case NMG_HIT_LIST: \
		bu_log(BU_FLSTR ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
		bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	    default: \
		bu_log(BU_FLSTR ": bad struct hitmiss magic: %u:(0x%08x)\n", \
		       hm->l.magic, hm->l.magic); \
		bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	}\
	if (!hm->hit.hit_private) { \
	    bu_log(BU_FLSTR ": NULL hit_private in hitmiss struct\n"); \
	    bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
	} \
    }
#endif

#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS_LISTS(rd) BU_IGNORE((rd))
#else
#  define NMG_CK_HITMISS_LISTS(rd) \
    { \
	struct hitmiss *_a_hit; \
	for (BU_LIST_FOR(_a_hit, hitmiss, &rd->rd_hit)) {NMG_CK_HITMISS(_a_hit);} \
	for (BU_LIST_FOR(_a_hit, hitmiss, &rd->rd_miss)) {NMG_CK_HITMISS(_a_hit);} \
    }
#endif


/**
 * Ray Data structure
 *
 * A) the hitmiss table has one element for each nmg structure in the
 * nmgmodel.  The table keeps track of which elements have been
 * processed before and which haven't.  Elements in this table will
 * either be: (NULL) item not previously processed hitmiss ptr item
 * previously processed
 *
 * the 0th item in the array is a pointer to the head of the "hit"
 * list.  The 1th item in the array is a pointer to the head of the
 * "miss" list.
 *
 * B) If plane_pt is non-null then we are currently processing a face
 * intersection.  The plane_dist and ray_dist_to_plane are valid.  The
 * ray/edge intersector should check the distance from the plane
 * intercept to the edge and update "plane_closest" if the current
 * edge is closer to the intercept than the previous closest object.
 */
struct ray_data {
    uint32_t		magic;
    struct model	*rd_m;
    char		*manifolds; /**< @brief   structure 1-3manifold table */
    vect_t		rd_invdir;
    struct xray		*rp;
    struct application	*ap;
    struct seg		*seghead;
    struct soltab 	*stp;
    const struct bn_tol	*tol;
    struct hitmiss	**hitmiss;	/**< @brief  1 struct hitmiss ptr per elem. */
    struct bu_list	rd_hit;		/**< @brief  list of hit elements */
    struct bu_list	rd_miss;	/**< @brief  list of missed/sub-hit elements */

/* The following are to support isect_ray_face() */

    /**
     * plane_pt is the intercept point of the ray with the plane of
     * the face.
     */
    point_t		plane_pt;	/**< @brief  ray/plane(face) intercept point */

    /**
     * ray_dist_to_plane is the parametric distance along the ray from
     * the ray origin (rd->rp->r_pt) to the ray/plane intercept point
     */
    fastf_t		ray_dist_to_plane; /**< @brief  ray parametric dist to plane */

    /**
     * the "face_subhit" element is a boolean used by isect_ray_face
     * and [e|v]u_touch_func to record the fact that the
     * ray/(plane/face) intercept point was within tolerance of an
     * edge/vertex of the face.  In such instances, isect_ray_face
     * does NOT need to generate a hit point for the face, as the hit
     * point for the edge/vertex will suffice.
     */
    int			face_subhit;

    /**
     * the "classifying_ray" flag indicates that this ray is being
     * used to classify a point, so that the "eu_touch" and "vu_touch"
     * functions should not be called.
     */
    int			classifying_ray;
};


#define NMG_PCA_EDGE	1
#define NMG_PCA_EDGE_VERTEX 2
#define NMG_PCA_VERTEX 3
#define NMG_CK_RD(_rd) NMG_CKMAG(_rd, NMG_RAY_DATA_MAGIC, "ray data");


#define NMG_GET_HITMISS(_p, _ap) { \
	(_p) = BU_LIST_FIRST(hitmiss, &((_ap)->a_resource->re_nmgfree)); \
	if (BU_LIST_IS_HEAD((_p), &((_ap)->a_resource->re_nmgfree))) \
	    BU_ALLOC((_p), struct hitmiss); \
	else \
	    BU_LIST_DEQUEUE(&((_p)->l)); \
    }


#define NMG_FREE_HITLIST(_p, _ap) { \
	BU_CK_LIST_HEAD((_p)); \
	BU_LIST_APPEND_LIST(&((_ap)->a_resource->re_nmgfree), (_p)); \
    }


#define HIT 1	/**< @brief  a hit on a face */
#define MISS 0	/**< @brief  a miss on the face */


#ifdef NO_BOMBING_MACROS
#  define nmg_bu_bomb(rd, str) BU_IGNORE((rd))
#else
#  define nmg_bu_bomb(rd, str) { \
	bu_log("%s", str); \
	if (RTG.NMG_debug & DEBUG_NMGRT) bu_bomb("End of diagnostics"); \
	BU_LIST_INIT(&rd->rd_hit); \
	BU_LIST_INIT(&rd->rd_miss); \
	RTG.NMG_debug |= DEBUG_NMGRT; \
	nmg_isect_ray_model(rd); \
	(void) nmg_ray_segs(rd); \
	bu_bomb("Should have bombed before this\n"); \
    }
#endif


struct nmg_radial {
    struct bu_list	l;
    struct edgeuse	*eu;
    struct faceuse	*fu;		/**< @brief  Derived from eu */
    struct shell	*s;		/**< @brief  Derived from eu */
    int			existing_flag;	/**< @brief  !0 if this eu exists on dest edge */
    int			is_crack;	/**< @brief  This eu is part of a crack. */
    int			is_outie;	/**< @brief  This crack is an "outie" */
    int			needs_flip;	/**< @brief  Insert eumate, not eu */
    fastf_t		ang;		/**< @brief  angle, in radians.  0 to 2pi */
};
#define NMG_CK_RADIAL(_p) NMG_CKMAG(_p, NMG_RADIAL_MAGIC, "nmg_radial")

struct nmg_inter_struct {
    uint32_t		magic;
    struct bu_ptbl	*l1;		/**< @brief  vertexuses on the line of */
    struct bu_ptbl	*l2;		/**< @brief  intersection between planes */
    fastf_t		*mag1;		/**< @brief  Distances along intersection line */
    fastf_t		*mag2;		/**< @brief  for each vertexuse in l1 and l2. */
    int			mag_len;	/**< @brief  Array size of mag1 and mag2 */
    struct shell	*s1;
    struct shell	*s2;
    struct faceuse	*fu1;		/**< @brief  null if l1 comes from a wire */
    struct faceuse	*fu2;		/**< @brief  null if l2 comes from a wire */
    struct bn_tol	tol;
    int			coplanar;	/**< @brief  a flag */
    struct edge_g_lseg	*on_eg;		/**< @brief  edge_g for line of intersection */
    point_t		pt;		/**< @brief  3D line of intersection */
    vect_t		dir;
    point_t		pt2d;		/**< @brief  2D projection of isect line */
    vect_t		dir2d;
    fastf_t		*vert2d;	/**< @brief  Array of 2d vertex projections [index] */
    int			maxindex;	/**< @brief  size of vert2d[] */
    mat_t		proj;		/**< @brief  Matrix to project onto XY plane */
    const uint32_t	*twod;		/**< @brief  ptr to face/edge of 2d projection */
};
#define NMG_CK_INTER_STRUCT(_p) NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")

/*****************************************************************
 *                                                               *
 *          Applications interface to the RT library             *
 *                                                               *
 *****************************************************************/

/* Prepare for raytracing */

RT_EXPORT extern struct rt_i *rt_new_rti(struct db_i *dbip);
RT_EXPORT extern void rt_free_rti(struct rt_i *rtip);
RT_EXPORT extern void rt_prep(struct rt_i *rtip);
RT_EXPORT extern void rt_prep_parallel(struct rt_i *rtip,
				       int ncpu);

/**
 * Default version of a_multioverlap().
 *
 * Resolve the overlap of multiple regions withing a single partition.
 * There are no null pointers in the table (they have been compressed
 * out by our caller).  Consider BU_PTBL_LEN(regiontable) overlapping
 * regions, and reduce to zero or one "claiming" regions, by setting
 * pointers in the bu_ptbl of non-claiming regions to NULL.
 *
 * This default routine reproduces the behavior of BRL-CAD Release 5.0
 * by considering the regions pairwise and calling the old
 * a_overlap().
 *
 * An application which knew how to handle multiple overlapping air
 * regions would provide its own very different version of this
 * routine as the a_multioverlap() handler.
 *
 * This routine is for resolving overlaps only, and should not print
 * any messages in normal operation; a_logoverlap() is for logging.
 *
 * InputHdp is the list of partitions up to this point.  It allows us
 * to look at the regions that have come before in deciding what to do
 */
RT_EXPORT extern void rt_default_multioverlap(struct application *ap,
					      struct partition *pp,
					      struct bu_ptbl *regiontable,
					      struct partition *InputHdp);

/**
 * If an application doesn't want any logging from LIBRT, it should
 * just set ap->a_logoverlap = rt_silent_logoverlap.
 */
RT_EXPORT extern void rt_silent_logoverlap(struct application *ap,
					   const struct partition *pp,
					   const struct bu_ptbl *regiontable,
					   const struct partition *InputHdp);

/**
 * Log a multiplicity of overlaps within a single partition.  This
 * function is intended for logging only, and a_multioverlap() is
 * intended for resolving the overlap, only.  This function can be
 * replaced by an application setting a_logoverlap().
 */
RT_EXPORT extern void rt_default_logoverlap(struct application *ap,
					    const struct partition *pp,
					    const struct bu_ptbl *regiontable,
					    const struct partition *InputHdp);

/**
 * Initial set of 'xrays' pattern generators that can
 * used to feed a bundle set of rays to rt_shootrays()
 */

/**
 * Make a bundle of rays around a main ray using a uniform rectangular
 * grid pattern with an elliptical extent.
 *
 * avec and bvec a.  The gridsize is
 * given in mm.
 *
 * rp[0].r_dir must have unit length.
 */
RT_EXPORT extern int rt_gen_elliptical_grid(struct xrays *rays,
					    const struct xray *center_ray,
					    const fastf_t *avec,
					    const fastf_t *bvec,
					    fastf_t gridsize);

/**
 * Make a bundle of rays around a main ray using a uniform rectangular
 * grid pattern with a circular extent.  The radius, gridsize is given
 * in mm.
 *
 * rp[0].r_dir must have unit length.
 */
RT_EXPORT extern int rt_gen_circular_grid(struct xrays *ray_bundle,
					  const struct xray *center_ray,
					  fastf_t radius,
					  const fastf_t *up_vector,
					  fastf_t gridsize);

/* Shoot a ray */
/**
 * Note that the direction vector r_dir must have unit length; this is
 * mandatory, and is not ordinarily checked, in the name of
 * efficiency.
 *
 * Input:  Pointer to an application structure, with these mandatory fields:
 * a_ray.r_pt ==> Starting point of ray to be fired
 * a_ray.r_dir => UNIT VECTOR with direction to fire in (dir cosines)
 * a_hit =======> Routine to call when something is hit
 * a_miss ======> Routine to call when ray misses everything
 *
 * Calls user's a_miss() or a_hit() routine as appropriate.  Passes
 * a_hit() routine list of partitions, with only hit_dist fields
 * valid.  Normal computation deferred to user code, to avoid needless
 * computation here.
 *
 * Formal Return: whatever the application function returns (an int).
 *
 * NOTE:  The application functions may call rt_shootray() recursively.
 * Thus, none of the local variables may be static.
 *
 * To prevent having to lock the statistics variables in a PARALLEL
 * environment, all the statistics variables have been moved into the
 * 'resource' structure, which is allocated per-CPU.
 */
RT_EXPORT extern int rt_shootray(struct application *ap);
/* Shoot a bundle of rays */

/*
 * Function for shooting a bundle of rays. Iteratively walks list of
 * rays contained in the application bundles xrays field 'b_rays'
 * passing each single ray to r_shootray().
 *
 * Input:
 *
 * bundle -  Pointer to an application_bundle structure.
 *
 * b_ap - Members in this single ray application structure should be
 * set in a similar fashion as when used with rt_shootray() with the
 * exception of a_hit() and a_miss(). Default implementations of these
 * routines are provided that simple update hit/miss counters and
 * attach the hit partitions and segments to the partition_bundle
 * structure. Users can still override this default functionality but
 * have to make sure to move the partition and segment list to the new
 * partition_bundle structure.
 *
 * b_hit() Routine to call when something is hit by the ray bundle.
 *
 * b_miss() Routine to call when ray bundle misses everything.
 *
 */
RT_EXPORT extern int rt_shootrays(struct application_bundle *bundle);

/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int rt_shootray_bundle(register struct application *ap, struct xray *rays, int nrays);

/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int rt_raybundle_maker(struct xray *rp, double radius, const fastf_t *avec, const fastf_t *bvec, int rays_per_ring, int nring);

/* Shoot a ray, returning the partition list */
/**
 * Shoot a single ray and return the partition list. Handles callback issues.
 *
 * Note that it calls malloc(), therefore should NOT be used if performance
 * matters.
 */
RT_EXPORT extern struct partition *rt_shootray_simple(struct application *ap,
						      point_t origin,
						      vect_t direction);
/* Get expr tree for object */
/**
 * Decrement use count on soltab structure.  If no longer needed,
 * release associated storage, and free the structure.
 *
 * This routine semaphore protects against other copies of itself
 * running in parallel, and against other routines (such as
 * _rt_find_identical_solid()) which might also be modifying the
 * linked list heads.
 *
 * Called by -
 * db_free_tree()
 * rt_clean()
 * rt_gettrees()
 * rt_kill_deal_solid_refs()
 */
RT_EXPORT extern void rt_free_soltab(struct soltab   *stp);


/**
 * User-called function to add a tree hierarchy to the displayed set.
 *
 * This function is not multiply re-entrant.
 *
 * Returns -
 * 0 Ordinarily
 * -1 On major error
 *
 * Note: -2 returns from rt_gettrees_and_attrs are filtered.
 */
RT_EXPORT extern int rt_gettree(struct rt_i *rtip,
				const char *node);
RT_EXPORT extern int rt_gettrees(struct rt_i *rtip,
				 int argc,
				 const char **argv, int ncpus);

/**
 * User-called function to add a set of tree hierarchies to the active
 * set.
 *
 * This function may run in parallel, but is not multiply re-entrant
 * itself, because db_walk_tree() isn't multiply re-entrant.
 *
 * Semaphores used for critical sections in parallel mode:
 * RT_SEM_TREE* protects rti_solidheads[] lists, d_uses(solids)
 * RT_SEM_RESULTS protects HeadRegion, mdl_min/max, d_uses(reg), nregions
 * RT_SEM_WORKER (db_walk_dispatcher, from db_walk_tree)
 * RT_SEM_STATS nsolids
 *
 * Returns -
 * 0 Ordinarily
 * -1 On major error
 * -2 If there were unresolved names
 */
RT_EXPORT extern int rt_gettrees_and_attrs(struct rt_i *rtip,
					   const char **attrs,
					   int argc,
					   const char **argv, int ncpus);


/**
 * User-called function to add a set of tree hierarchies to the active
 * set. Includes getting the indicated list of attributes and a
 * Tcl_HashTable for use with the ORCA man regions. (stashed in the
 * rt_i structure).
 *
 * This function may run in parallel, but is not multiply re-entrant
 * itself, because db_walk_tree() isn't multiply re-entrant.
 *
 * Semaphores used for critical sections in parallel mode:
 * RT_SEM_TREE ====> protects rti_solidheads[] lists, d_uses(solids)
 * RT_SEM_RESULTS => protects HeadRegion, mdl_min/max, d_uses(reg), nregions
 * RT_SEM_WORKER ==> (db_walk_dispatcher, from db_walk_tree)
 * RT_SEM_STATS ===> nsolids
 *
 * INPUTS:
 *
 * rtip - RT instance pointer
 *
 * attrs - attribute value set
 *
 * argc - number of trees to get
 *
 * argv - array of char pointers to the names of the tree tops
 *
 * ncpus - number of cpus to use
 *
 * Returns -
 * 0 Ordinarily
 * -1 On major error
 */
RT_EXPORT extern int rt_gettrees_muves(struct rt_i *rtip,
				       const char **attrs,
				       int argc,
				       const char **argv,
				       int ncpus);
DEPRECATED RT_EXPORT extern int rt_load_attrs(struct rt_i *rtip,
					      char **attrs);
/* Print seg struct */
RT_EXPORT extern void rt_pr_seg(const struct seg *segp);
/* Print the partitions */
RT_EXPORT extern void rt_pr_partitions(const struct rt_i *rtip,
				       const struct partition *phead,
				       const char *title);
/* Find solid by leaf name */
/**
 * Given the (leaf) name of a solid, find the first occurrence of it in
 * the solid list.  Used mostly to find the light source.  Returns
 * soltab pointer, or RT_SOLTAB_NULL.
 */
RT_EXPORT extern struct soltab *rt_find_solid(const struct rt_i *rtip,
					      const char *name);
/* Start the global timer */
/** @addtogroup timer */
/** @{ */
/** @file librt/timerhep.c
 *
 * To provide timing information for RT.
 * THIS VERSION FOR Denelcor HEP/UPX (System III-like)
 */

/** @file librt/timer42.c
*
* To provide timing information for RT when running on 4.2 BSD UNIX.
*
*/

 /** @file librt/timer-nt.c
 *
 * To provide timing information on Microsoft Windows NT.
 */
 /** @file librt/timerunix.c
 *
 * To provide timing information for RT.  This version for any non-BSD
 * UNIX system, including System III, Vr1, Vr2.  Version 6 & 7 should
 * also be able to use this (untested).  The time() and times()
 * sys-calls are used for all timing.
 *
 */


RT_EXPORT extern void rt_prep_timer(void);
/* Read global timer, return time + str */
/**
 * Reports on the passage of time, since rt_prep_timer() was called.
 * Explicit return is number of CPU seconds.  String return is
 * descriptive.  If "elapsed" pointer is non-null, number of elapsed
 * seconds are returned.  Times returned will never be zero.
 */
RT_EXPORT extern double rt_get_timer(struct bu_vls *vp,
				     double *elapsed);
/* Return CPU time, text, & wall clock time off the global timer */
/**
 * Compatibility routine
 */
RT_EXPORT extern double rt_read_timer(char *str, int len);
/* Plot a solid */
int rt_plot_solid(
    FILE			*fp,
    struct rt_i		*rtip,
    const struct soltab	*stp,
    struct resource		*resp);

/** @} */
/* Release storage assoc with rt_i */
RT_EXPORT extern void rt_clean(struct rt_i *rtip);
RT_EXPORT extern int rt_del_regtree(struct rt_i *rtip,
				    struct region *delregp,
				    struct resource *resp);
/* Check in-memory data structures */
RT_EXPORT extern void rt_ck(struct rt_i *rtip);
/* apply a matrix transformation */
/**
 * apply a matrix transformation to a given input object, setting the
 * resultant transformed object as the output solid.  if freeflag is
 * set, the input object will be released.
 *
 * returns zero if matrix transform was applied, non-zero on failure.
 */
RT_EXPORT extern int rt_matrix_transform(struct rt_db_internal *output, const mat_t matrix, struct rt_db_internal *input, int free_input, struct db_i *dbip, struct resource *resource);


/*****************************************************************
 *                                                               *
 *  Internal routines in the RT library.			 *
 *  These routines are *not* intended for Applications to use.	 *
 *  The interface to these routines may change significantly	 *
 *  from release to release of this software.			 *
 *                                                               *
 *****************************************************************/

/* Weave segs into partitions */

/**
 * Weave a chain of segments into an existing set of partitions.  The
 * edge of each partition is an inhit or outhit of some solid (seg).
 *
 * NOTE: When the final partitions are completed, it is the users
 * responsibility to honor the inflip and outflip flags.  They can not
 * be flipped here because an outflip=1 edge and an inflip=0 edge
 * following it may in fact be the same edge.  This could be dealt
 * with by giving the partition struct a COPY of the inhit and outhit
 * rather than a pointer, but that's more cycles than the neatness is
 * worth.
 *
 * Inputs -
 * Pointer to first segment in seg chain.
 * Pointer to head of circular doubly-linked list of
 * partitions of the original ray.
 *
 * Outputs -
 * Partitions, queued on doubly-linked list specified.
 *
 * Notes -
 * It is the responsibility of the CALLER to free the seg chain, as
 * well as the partition list that we return.
 */
RT_EXPORT extern void rt_boolweave(struct seg *out_hd,
				   struct seg *in_hd,
				   struct partition *PartHeadp,
				   struct application *ap);
/* Eval booleans over partitions */

/**
 * Consider each partition on the sorted & woven input partition list.
 * If the partition ends before this box's start, discard it
 * immediately.  If the partition begins beyond this box's end,
 * return.
 *
 * Next, evaluate the boolean expression tree for all regions that
 * have some presence in the partition.
 *
 * If 0 regions result, continue with next partition.
 *
 * If 1 region results, a valid hit has occurred, so transfer the
 * partition from the Input list to the Final list.
 *
 * If 2 or more regions claim the partition, then an overlap exists.
 *
 * If the overlap handler gives a non-zero return, then the
 * overlapping partition is kept, with the region ID being the first
 * one encountered.
 *
 * Otherwise, the partition is eliminated from further consideration.
 *
 * All partitions in the indicated range of the ray are evaluated.
 * All partitions which really exist (booleval is true) are appended
 * to the Final partition list.  All partitions on the Final partition
 * list have completely valid entry and exit information, except for
 * the last partition's exit information when a_onehit!=0 and a_onehit
 * is odd.
 *
 * The flag a_onehit is interpreted as follows:
 *
 * If a_onehit = 0, then the ray is traced to +infinity, and all hit
 * points in the final partition list are valid.
 *
 * If a_onehit != 0, the ray is traced through a_onehit hit points.
 * (Recall that each partition has 2 hit points, entry and exit).
 * Thus, if a_onehit is odd, the value of pt_outhit.hit_dist in the
 * last partition may be incorrect; this should not mater because the
 * application specifically said it only wanted pt_inhit there.  This
 * is most commonly seen when a_onehit = 1, which is useful for
 * lighting models.  Not having to correctly determine the exit point
 * can result in a significant savings of computer time.
 *
 * If a_onehit is negative, it indicates the number of non-air hits
 * needed.
 *
 * Returns -
 * 0 If more partitions need to be done
 * 1 Requested number of hits are available in FinalHdp
 *
 * The caller must free whatever is in both partition chains.
 *
 * NOTES for code improvements -
 *
 * With a_onehit != 0, it is difficult to stop at the 'enddist' value
 * (or the a_ray_length value), and always get correct results.  Need
 * to take into account some additional factors:
 *
 * 1) A region shouldn't be evaluated until all its solids have been
 * intersected, to prevent the "CERN" problem of out points being
 * wrong because subtracted solids aren't intersected yet.
 *
 * Maybe "all" solids don't have to be intersected, but some strong
 * statements are needed along these lines.
 *
 * A region is definitely ready to be evaluated IF all its solids
 * have been intersected.
 *
 * 2) A partition shouldn't be evaluated until all the regions within
 * it are ready to be evaluated.
 */
RT_EXPORT extern int rt_boolfinal(struct partition *InputHdp,
				  struct partition *FinalHdp,
				  fastf_t startdist,
				  fastf_t enddist,
				  struct bu_ptbl *regionbits,
				  struct application *ap,
				  const struct bu_bitv *solidbits);

/**
 * Increase the size of re_boolstack to double the previous size.
 * Depend on bu_realloc() to copy the previous data to the new area
 * when the size is increased.
 *
 * Return the new pointer for what was previously the last element.
 */
RT_EXPORT extern void rt_grow_boolstack(struct resource *res);

/* Print a soltab */
RT_EXPORT extern void rt_pr_soltab(const struct soltab *stp);
/* Print a region */
RT_EXPORT extern void rt_pr_region(const struct region *rp);
/* Print an expr tree */
RT_EXPORT extern void rt_pr_tree(const union tree *tp,
				 int lvl);
/* Print value of tree for a partition */
RT_EXPORT extern void rt_pr_tree_val(const union tree *tp,
				     const struct partition *partp,
				     int pr_name, int lvl);
/* Print a partition */
RT_EXPORT extern void rt_pr_pt(const struct rt_i *rtip,
			       const struct partition *pp);
/* Print a bit vector */
RT_EXPORT extern void rt_pr_hit(const char *str,
				const struct hit *hitp);

/* rt_fastf_float, rt_mat_dbmat, rt_dbmat_mat
 * declarations moved to db.h */

/**
 * Go through all the solids in the model, given the model mins and
 * maxes, and generate a cutting tree.  A strategy better than
 * incrementally cutting each solid is to build a box node which
 * contains everything in the model, and optimize it.
 *
 * This is the main entry point into space partitioning from
 * rt_prep().
 */
RT_EXPORT extern void rt_cut_it(struct rt_i *rtip,
				int ncpu);

/* print cut node */

/**
 * Print out a cut tree.
 *
 * lvl is recursion level.
 */
RT_EXPORT extern void rt_pr_cut(const union cutter *cutp,
				int lvl);
/* free a cut tree */
/**
 * Free a whole cut tree below the indicated node.  The strategy we
 * use here is to free everything BELOW the given node, so as not to
 * clobber rti_CutHead !
 */
RT_EXPORT extern void rt_fr_cut(struct rt_i *rtip,
				union cutter *cutp);

/* regionid-driven color override */

/* bool.c */

/**
 * XXX This routine seems to free things more than once.  For a
 * temporary measure, don't free things.
 */
RT_EXPORT extern void rt_rebuild_overlaps(struct partition	*PartHdp,
					  struct application	*ap,
					  int		rebuild_fastgen_plates_only);

/**
 * Return the length of a partition linked list.
 */
RT_EXPORT extern int rt_partition_len(const struct partition *partheadp);

/**
 * Default handler for overlaps in rt_boolfinal().
 *
 * Returns -
 * 0 to eliminate partition with overlap entirely
 * 1 to retain partition in output list, claimed by reg1
 * 2 to retain partition in output list, claimed by reg2
 */
RT_EXPORT extern int	rt_defoverlap(struct application *ap,
				      struct partition *pp,
				      struct region *reg1,
				      struct region *reg2,
				      struct partition *pheadp);

/* extend a cut box */

/* cut.c */

RT_EXPORT extern void rt_pr_cut_info(const struct rt_i	*rtip,
				     const char		*str);
RT_EXPORT extern void remove_from_bsp(struct soltab *stp,
				      union cutter *cutp,
				      struct bn_tol *tol);
RT_EXPORT extern void insert_in_bsp(struct soltab *stp,
				    union cutter *cutp);
RT_EXPORT extern void fill_out_bsp(struct rt_i *rtip,
				   union cutter *cutp,
				   struct resource *resp,
				   fastf_t bb[6]);

/**
 * Add a solid into a given boxnode, extending the lists there.  This
 * is used only for building the root node, which will then be
 * subdivided.
 *
 * Solids with pieces go onto a special list.
 */
RT_EXPORT extern void rt_cut_extend(union cutter *cutp,
				    struct soltab *stp,
				    const struct rt_i *rtip);
/* find RPP of one region */

/**
 * Calculate the bounding RPP for a region given the name of the
 * region node in the database.  See remarks in _rt_getregion() above
 * for name conventions.  Returns 0 for failure (and prints a
 * diagnostic), or 1 for success.
 */
RT_EXPORT extern int rt_rpp_region(struct rt_i *rtip,
				   const char *reg_name,
				   fastf_t *min_rpp,
				   fastf_t *max_rpp);

/**
 * Compute the intersections of a ray with a rectangular parallelepiped
 * (RPP) that has faces parallel to the coordinate planes
 *
 * The algorithm here was developed by Gary Kuehl for GIFT.  A good
 * description of the approach used can be found in "??" by XYZZY and
 * Barsky, ACM Transactions on Graphics, Vol 3 No 1, January 1984.
 *
 * Note: The computation of entry and exit distance is mandatory, as
 * the final test catches the majority of misses.
 *
 * Note: A hit is returned if the intersect is behind the start point.
 *
 * Returns -
 * 0 if ray does not hit RPP,
 * !0 if ray hits RPP.
 *
 * Implicit return -
 * rp->r_min = dist from start of ray to point at which ray ENTERS solid
 * rp->r_max = dist from start of ray to point at which ray LEAVES solid
 */
RT_EXPORT extern int rt_in_rpp(struct xray *rp,
			       const fastf_t *invdir,
			       const fastf_t *min,
			       const fastf_t *max);


/**
 * Return pointer to cell 'n' along a given ray.  Used for debugging
 * of how space partitioning interacts with shootray.  Intended to
 * mirror the operation of rt_shootray().  The first cell is 0.
 */
RT_EXPORT extern const union cutter *rt_cell_n_on_ray(struct application *ap,
						      int n);

/*
 * The rtip->rti_CutFree list can not be freed directly because is
 * bulk allocated.  Fortunately, we have a list of all the
 * bu_malloc()'ed blocks.  This routine may be called before the first
 * frame is done, so it must be prepared for uninitialized items.
 */
RT_EXPORT extern void rt_cut_clean(struct rt_i *rtip);

/* Find the bounding box given a struct rt_db_internal : bbox.c */

/**
 *
 * Calculate the bounding RPP of the internal format passed in 'ip'.
 * The bounding RPP is returned in rpp_min and rpp_max in mm
 * FIXME: This function needs to be modified to eliminate the rt_gettree() call and the related
 * parameters. In that case calling code needs to call another function before calling this function
 * That function must create a union tree with tr_a.tu_op=OP_SOLID. It can look as follows :
 * union tree * rt_comb_tree(const struct db_i *dbip, const struct rt_db_internal *ip). The tree is set
 * in the struct rt_db_internal * ip argument.
 * Once a suitable tree is set in the ip, then this function can be called with the struct rt_db_internal *
 * to return the BB properly without getting stuck during tree traversal in rt_bound_tree()
 *
 * Returns -
 *  0 success
 * -1 failure, the model bounds could not be got
 *
 */
RT_EXPORT extern int rt_bound_internal(struct db_i *dbip,
				       struct directory *dp,
				       point_t rpp_min,
				       point_t rpp_max);

/* cmd.c */
/* Read semi-colon terminated line */

/*
 * Read one semi-colon terminated string of arbitrary length from the
 * given file into a dynamically allocated buffer.  Various commenting
 * and escaping conventions are implemented here.
 *
 * Returns:
 * NULL on EOF
 * char * on good read
 */
RT_EXPORT extern char *rt_read_cmd(FILE *fp);
/* do cmd from string via cmd table */

/*
 * Slice up input buffer into whitespace separated "words", look up
 * the first word as a command, and if it has the correct number of
 * args, call that function.  Negative min/max values in the tp
 * command table effectively mean that they're not bounded.
 *
 * Expected to return -1 to halt command processing loop.
 *
 * Based heavily on mged/cmd.c by Chuck Kennedy.
 *
 * DEPRECATED: needs to migrate to libbu
 */
RT_EXPORT extern int rt_do_cmd(struct rt_i *rtip,
			       const char *ilp,
			       const struct command_tab *tp);

/* The database library */

/* wdb.c */
/** @addtogroup wdb */
/** @{ */
/** @file librt/wdb.c
 *
 * Routines to allow libwdb to use librt's import/export interface,
 * rather than having to know about the database formats directly.
 *
 */
RT_EXPORT extern struct rt_wdb *wdb_fopen(const char *filename);


/**
 * Create a libwdb output stream destined for a disk file.  This will
 * destroy any existing file by this name, and start fresh.  The file
 * is then opened in the normal "update" mode and an in-memory
 * directory is built along the way, allowing retrievals and object
 * replacements as needed.
 *
 * Users can change the database title by calling: ???
 */
RT_EXPORT extern struct rt_wdb *wdb_fopen_v(const char *filename,
					    int version);


/**
 * Create a libwdb output stream destined for an existing BRL-CAD
 * database, already opened via a db_open() call.
 *
 * RT_WDB_TYPE_DB_DISK Add to on-disk database
 * RT_WDB_TYPE_DB_DISK_APPEND_ONLY Add to on-disk database, don't clobber existing names, use prefix
 * RT_WDB_TYPE_DB_INMEM Add to in-memory database only
 * RT_WDB_TYPE_DB_INMEM_APPEND_ONLY Ditto, but give errors if name in use.
 */
RT_EXPORT extern struct rt_wdb *wdb_dbopen(struct db_i *dbip,
					   int mode);


/**
 * Returns -
 *  0 and modified *internp;
 * -1 ft_import failure (from rt_db_get_internal)
 * -2 db_get_external failure (from rt_db_get_internal)
 * -3 Attempt to import from write-only (stream) file.
 * -4 Name not found in database TOC.
 *
 * NON-PARALLEL because of rt_uniresource
 */
RT_EXPORT extern int wdb_import(struct rt_wdb *wdbp,
				struct rt_db_internal *internp,
				const char *name,
				const mat_t mat);


/**
 * The caller must free "ep".
 *
 * Returns -
 *  0 OK
 * <0 error
 */
RT_EXPORT extern int wdb_export_external(struct rt_wdb *wdbp,
					 struct bu_external *ep,
					 const char *name,
					 int flags,
					 unsigned char minor_type);


/**
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.
 *
 * The internal representation is always freed.  This is the analog of
 * rt_db_put_internal() for rt_wdb objects.
 *
 * Use this routine in preference to wdb_export() whenever the caller
 * already has an rt_db_internal structure handy.
 *
 * NON-PARALLEL because of rt_uniresource
 *
 * Returns -
 *  0 OK
 * <0 error
 */
RT_EXPORT extern int wdb_put_internal(struct rt_wdb *wdbp,
				      const char *name,
				      struct rt_db_internal *ip,
				      double local2mm);


/**
 * Export an in-memory representation of an object, as described in
 * the file h/rtgeom.h, into the indicated database.
 *
 * The internal representation (gp) is always freed.
 *
 * WARNING: The caller must be careful not to double-free gp,
 * particularly if it's been extracted from an rt_db_internal, e.g. by
 * passing intern.idb_ptr for gp.
 *
 * If the caller has an rt_db_internal structure handy already, they
 * should call wdb_put_internal() directly -- this is a convenience
 * routine intended primarily for internal use in LIBWDB.
 *
 * Returns -
 *  0 OK
 * <0 error
 */
RT_EXPORT extern int wdb_export(struct rt_wdb *wdbp,
				const char *name,
				void *gp,
				int id,
				double local2mm);
RT_EXPORT extern void wdb_init(struct rt_wdb *wdbp,
			       struct db_i   *dbip,
			       int           mode);


/**
 * Release from associated database "file", destroy dynamic data
 * structure.
 */
RT_EXPORT extern void wdb_close(struct rt_wdb *wdbp);


/**
 * Given the name of a database object or a full path to a leaf
 * object, obtain the internal form of that leaf.  Packaged separately
 * mainly to make available nice Tcl error handling.
 *
 * Returns -
 * BRLCAD_OK
 * BRLCAD_ERROR
 */
RT_EXPORT extern int wdb_import_from_path(struct bu_vls *logstr,
					  struct rt_db_internal *ip,
					  const char *path,
					  struct rt_wdb *wdb);


/**
 * Given the name of a database object or a full path to a leaf
 * object, obtain the internal form of that leaf.  Packaged separately
 * mainly to make available nice Tcl error handling. Additionally,
 * copies ts.ts_mat to matp.
 *
 * Returns -
 * BRLCAD_OK
 * BRLCAD_ERROR
 */
RT_EXPORT extern int wdb_import_from_path2(struct bu_vls *logstr,
					   struct rt_db_internal *ip,
					   const char *path,
					   struct rt_wdb *wdb,
					   matp_t matp);

/** @} */

/* db_anim.c */
RT_EXPORT extern struct animate *db_parse_1anim(struct db_i     *dbip,
						int             argc,
						const char      **argv);


/**
 * A common parser for mged and rt.  Experimental.  Not the best name
 * for this.
 */
RT_EXPORT extern int db_parse_anim(struct db_i     *dbip,
				   int             argc,
				   const char      **argv);

/**
 * Add a user-supplied animate structure to the end of the chain of
 * such structures hanging from the directory structure of the last
 * node of the path specifier.  When 'root' is non-zero, this matrix
 * is located at the root of the tree itself, rather than an arc, and
 * is stored differently.
 *
 * In the future, might want to check to make sure that callers
 * directory references are in the right database (dbip).
 */
RT_EXPORT extern int db_add_anim(struct db_i *dbip,
				 struct animate *anp,
				 int root);

/**
 * Perform the one animation operation.  Leave results in form that
 * additional operations can be cascaded.
 *
 * Note that 'materp' may be a null pointer, signifying that the
 * region has already been finalized above this point in the tree.
 */
RT_EXPORT extern int db_do_anim(struct animate *anp,
				mat_t stack,
				mat_t arc,
				struct mater_info *materp);

/**
 * Release chain of animation structures
 *
 * An unfortunate choice of name.
 */
RT_EXPORT extern void db_free_anim(struct db_i *dbip);

/**
 * Writes 'count' bytes into at file offset 'offset' from buffer at
 * 'addr'.  A wrapper for the UNIX write() sys-call that takes into
 * account syscall semaphores, stdio-only machines, and in-memory
 * buffering.
 *
 * Returns -
 * 0 OK
 * -1 FAILURE
 */
/* should be HIDDEN */
RT_EXPORT extern void db_write_anim(FILE *fop,
				    struct animate *anp);

/**
 * Parse one "anim" type command into an "animate" structure.
 *
 * argv[1] must be the "a/b" path spec,
 * argv[2] indicates what is to be animated on that arc.
 */
RT_EXPORT extern struct animate *db_parse_1anim(struct db_i *dbip,
						int argc,
						const char **argv);


/**
 * Free one animation structure
 */
RT_EXPORT extern void db_free_1anim(struct animate *anp);

/* db_path.c */

/**
 * Normalize a BRL-CAD path according to rules used for realpath, but
 * without filesystem (or database object) validation.
 *
 * @return
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 */
RT_EXPORT extern const char *db_normalize(const char *path);


/* db_fullpath.c */
RT_EXPORT extern void db_full_path_init(struct db_full_path *pathp);

RT_EXPORT extern void db_add_node_to_full_path(struct db_full_path *pp,
					       struct directory *dp);

RT_EXPORT extern void db_dup_full_path(struct db_full_path *newp,
				       const struct db_full_path *oldp);

/**
 * Extend "pathp" so that it can grow from current fp_len by incr more names.
 *
 * This is intended primarily as an internal method.
 */
RT_EXPORT extern void db_extend_full_path(struct db_full_path *pathp,
					  size_t incr);

RT_EXPORT extern void db_append_full_path(struct db_full_path *dest,
					  const struct db_full_path *src);

/**
 * Dup old path from starting index to end.
 */
RT_EXPORT extern void db_dup_path_tail(struct db_full_path *newp,
				       const struct db_full_path *oldp,
				       off_t start);


/**
 * Unlike rt_path_str(), this version can be used in parallel.
 * Caller is responsible for freeing the returned buffer.
 */
RT_EXPORT extern char *db_path_to_string(const struct db_full_path *pp);

/**
 * Append a string representation of the path onto the vls.  Must have
 * exactly the same formatting conventions as db_path_to_string().
 */
RT_EXPORT extern void db_path_to_vls(struct bu_vls *str,
				     const struct db_full_path *pp);

/**
 * Append a string representation of the path onto the vls, with
 * options to decorate nodes with additional information.
 */
#define DB_FP_PRINT_BOOL         0x1    /* print boolean operations */
#define DB_FP_PRINT_TYPE         0x2    /* print object types */
#define DB_FP_PRINT_MATRIX       0x4    /* print notice that a matrix is present */
RT_EXPORT extern void db_fullpath_to_vls(struct bu_vls *vls,
	                                 const struct db_full_path *full_path,
					 const struct db_i *dbip,  /* needed for type determination */
					 int fp_flags);


RT_EXPORT extern void db_pr_full_path(const char *msg,
				      const struct db_full_path *pathp);


/**
 * Reverse the effects of db_path_to_string().
 *
 * The db_full_path structure will be initialized.  If it was already
 * in use, user should call db_free_full_path() first.
 *
 * Returns -
 * -1 One or more components of path did not exist in the directory.
 * 0 OK
 */
RT_EXPORT extern int db_string_to_path(struct db_full_path *pp,
				       const struct db_i *dbip,
				       const char *str);


/**
 * Treat elements from argv[0] to argv[argc-1] as a path specification.
 *
 * The path structure will be fully initialized.  If it was already in
 * use, user should call db_free_full_path() first.
 *
 * Returns -
 * -1 One or more components of path did not exist in the directory.
 * 0 OK
 */
RT_EXPORT extern int db_argv_to_path(struct db_full_path	*pp,
				     struct db_i			*dbip,
				     int				argc,
				     const char			*const*argv);


/**
 * Free the contents of the db_full_path structure, but not the
 * structure itself, which might be automatic.
 */
RT_EXPORT extern void db_free_full_path(struct db_full_path *pp);


/**
 * Returns -
 * 1 match
 * 0 different
 */
RT_EXPORT extern int db_identical_full_paths(const struct db_full_path *a,
					     const struct db_full_path *b);


/**
 * Returns -
 * 1 if 'b' is a proper subset of 'a'
 * 0 if not.
 */
RT_EXPORT extern int db_full_path_subset(const struct db_full_path	*a,
					 const struct db_full_path	*b,
					 const int			skip_first);


/**
 * Returns -
 * 1 if 'a' matches the top part of 'b'
 * 0 if not.
 *
 * For example, /a/b matches both the top part of /a/b/c and /a/b.
 */
RT_EXPORT extern int db_full_path_match_top(const struct db_full_path	*a,
					    const struct db_full_path	*b);


/**
 * Returns -
 * 1 'dp' is found on this path
 * 0 not found
 */
RT_EXPORT extern int db_full_path_search(const struct db_full_path *a,
					 const struct directory *dp);


/* search.c */

#include "./rt/search.h"

/* db_open.c */
/**
 * Ensure that the on-disk database has been completely written out of
 * the operating system's cache.
 */
RT_EXPORT extern void db_sync(struct db_i	*dbip);


/**
 * for db_open(), open the specified file as read-only
 */
#define DB_OPEN_READONLY "r"

/**
 * for db_open(), open the specified file as read-write
 */
#define DB_OPEN_READWRITE "rw"

/**
 * Open the named database.
 *
 * The 'name' parameter specifies the file or filepath to a .g
 * geometry database file for reading and/or writing.
 *
 * The 'mode' parameter specifies whether to open read-only or in
 * read-write mode, specified via the DB_OPEN_READONLY and
 * DB_OPEN_READWRITE symbols respectively.
 *
 * As a convenience, the returned db_t structure's dbi_filepath field
 * is a C-style argv array of dirs to search when attempting to open
 * related files (such as data files for EBM solids or texture-maps).
 * The default values are "." and the directory containing the ".g"
 * file.  They may be overridden by setting the environment variable
 * BRLCAD_FILE_PATH.
 *
 * Returns:
 * DBI_NULL error
 * db_i * success
 */
RT_EXPORT extern struct db_i *
db_open(const char *name, const char *mode);


/* create a new model database */
/**
 * Create a new database containing just a header record, regardless
 * of whether the database previously existed or not, and open it for
 * reading and writing.
 *
 * This routine also calls db_dirbuild(), so the caller doesn't need
 * to.
 *
 * Returns:
 * DBI_NULL on error
 * db_i * on success
 */
RT_EXPORT extern struct db_i *db_create(const char *name,
					int version);

/* close a model database */
/**
 * De-register a client of this database instance, if provided, and
 * close out the instance.
 */
RT_EXPORT extern void db_close_client(struct db_i *dbip,
				      long *client);

/**
 * Close a database, releasing dynamic memory Wait until last user is
 * done, though.
 */
RT_EXPORT extern void db_close(struct db_i *dbip);


/* dump a full copy of a database */
/**
 * Dump a full copy of one database into another.  This is a good way
 * of committing a ".inmem" database to a ".g" file.  The input is a
 * database instance, the output is a LIBWDB object, which could be a
 * disk file or another database instance.
 *
 * Returns -
 * -1 error
 * 0 success
 */
RT_EXPORT extern int db_dump(struct rt_wdb *wdbp,
			     struct db_i *dbip);

/**
 * Obtain an additional instance of this same database.  A new client
 * is registered at the same time if one is specified.
 */
RT_EXPORT extern struct db_i *db_clone_dbi(struct db_i *dbip,
					   long *client);


/**
 * Create a v5 database "free" object of the specified size, and place
 * it at the indicated location in the database.
 *
 * There are two interesting cases:
 * - The free object is "small".  Just write it all at once.
 * - The free object is "large".  Write header and trailer
 * separately
 *
 * @return 0 OK
 * @return -1 Fail.  This is a horrible error.
 */
RT_EXPORT extern int db5_write_free(struct db_i *dbip,
				    struct directory *dp,
				    size_t length);


/**
 * Change the size of a v5 database object.
 *
 * If the object is getting smaller, break it into two pieces, and
 * write out free objects for both.  The caller is expected to
 * re-write new data on the first one.
 *
 * If the object is getting larger, seek a suitable "hole" large
 * enough to hold it, throwing back any surplus, properly marked.
 *
 * If the object is getting larger and there is no suitable "hole" in
 * the database, extend the file, write a free object in the new
 * space, and write a free object in the old space.
 *
 * There is no point to trying to extend in place, that would require
 * two searches through the memory map, and doesn't save any disk I/O.
 *
 * Returns -
 * 0 OK
 * -1 Failure
 */
RT_EXPORT extern int db5_realloc(struct db_i *dbip,
				 struct directory *dp,
				 struct bu_external *ep);


/**
 * A routine for merging together the three optional parts of an
 * object into the final on-disk format.  Results in extra data
 * copies, but serves as a starting point for testing.  Any of name,
 * attrib, and body may be null.
 */
RT_EXPORT extern void db5_export_object3(struct bu_external *out,
					 int			dli,
					 const char			*name,
					 const unsigned char	hidden,
					 const struct bu_external	*attrib,
					 const struct bu_external	*body,
					 int			major,
					 int			minor,
					 int			a_zzz,
					 int			b_zzz);


/**
 * The attributes are taken from ip->idb_avs
 *
 * If present, convert attributes to on-disk format.  This must happen
 * after exporting the body, in case the ft_export5() method happened
 * to extend the attribute set.  Combinations are one "solid" which
 * does this.
 *
 * The internal representation is NOT freed, that's the caller's job.
 *
 * The 'ext' pointer is accepted in uninitialized form, and an
 * initialized structure is always returned, so that the caller may
 * free it even when an error return is given.
 *
 * Returns -
 * 0 OK
 * -1 FAIL
 */
RT_EXPORT extern int rt_db_cvt_to_external5(struct bu_external *ext,
					    const char *name,
					    const struct rt_db_internal *ip,
					    double conv2mm,
					    struct db_i *dbip,
					    struct resource *resp,
					    const int major);


/*
 * Modify name of external object, if necessary.
 */
RT_EXPORT extern int db_wrap_v5_external(struct bu_external *ep,
					 const char *name);


/**
 * Get an object from the database, and convert it into its internal
 * representation.
 *
 * Applications and middleware shouldn't call this directly, they
 * should use the generic interface "rt_db_get_internal()".
 *
 * Returns -
 * <0 On error
 * id On success.
 */
RT_EXPORT extern int rt_db_get_internal5(struct rt_db_internal	*ip,
					 const struct directory	*dp,
					 const struct db_i	*dbip,
					 const mat_t		mat,
					 struct resource		*resp);


/**
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.
 *
 * Applications and middleware shouldn't call this directly, they
 * should use the version-generic interface "rt_db_put_internal()".
 *
 * The internal representation is always freed.  (Not the pointer,
 * just the contents).
 *
 * Returns -
 * <0 error
 * 0 success
 */
RT_EXPORT extern int rt_db_put_internal5(struct directory	*dp,
					 struct db_i		*dbip,
					 struct rt_db_internal	*ip,
					 struct resource		*resp,
					 const int		major);


/**
 * Make only the front (header) portion of a free object.  This is
 * used when operating on very large contiguous free objects in the
 * database (e.g. 50 MBytes).
 */
RT_EXPORT extern void db5_make_free_object_hdr(struct bu_external *ep,
					       size_t length);


/**
 * Make a complete, zero-filled, free object.  Note that free objects
 * can sometimes get quite large.
 */
RT_EXPORT extern void db5_make_free_object(struct bu_external *ep,
					   size_t length);


/**
 * Given a variable-width length field in network order (XDR), store
 * it in *lenp.
 *
 * This routine processes signed values.
 *
 * Returns -
 * The number of bytes of input that were decoded.
 */
RT_EXPORT extern int db5_decode_signed(size_t			*lenp,
				       const unsigned char	*cp,
				       int			format);

/**
 * Given a variable-width length field in network order (XDR), store
 * it in *lenp.
 *
 * This routine processes unsigned values.
 *
 * Returns -
 * The number of bytes of input that were decoded.
 */
RT_EXPORT extern size_t db5_decode_length(size_t *lenp,
					  const unsigned char *cp,
					  int format);

/**
 * Given a number to encode, decide which is the smallest encoding
 * format which will contain it.
 */
RT_EXPORT extern int db5_select_length_encoding(size_t len);


RT_EXPORT extern void db5_import_color_table(char *cp);

/**
 * Convert the on-disk encoding into a handy easy-to-use
 * bu_attribute_value_set structure.
 *
 * Take advantage of the readonly_min/readonly_max capability so that
 * we don't have to bu_strdup() each string, but can simply point to
 * it in the provided buffer *ap.  Important implication: don't free
 * *ap until you're done with this avs.
 *
 * The upshot of this is that bu_avs_add() and bu_avs_remove() can be
 * safely used with this *avs.
 *
 * Returns -
 * >0 count of attributes successfully imported
 * -1 Error, mal-formed input
 */
RT_EXPORT extern int db5_import_attributes(struct bu_attribute_value_set *avs,
					   const struct bu_external *ap);

/**
 * Encode the attribute-value pair information into the external
 * on-disk format.
 *
 * The on-disk encoding is:
 *
 * name1 NULL value1 NULL ... nameN NULL valueN NULL NULL
 *
 * 'ext' is initialized on behalf of the caller.
 */
RT_EXPORT extern void db5_export_attributes(struct bu_external *ap,
					    const struct bu_attribute_value_set *avs);


/**
 * Returns -
 * 0 on success
 * -1 on EOF
 * -2 on error
 */
RT_EXPORT extern int db5_get_raw_internal_fp(struct db5_raw_internal	*rip,
					     FILE			*fp);

/**
 * Verify that this is a valid header for a BRL-CAD v5 database.
 *
 * Returns -
 * 0 Not valid v5 header
 * 1 Valid v5 header
 */
RT_EXPORT extern int db5_header_is_valid(const unsigned char *hp);

RT_EXPORT extern int db5_fwrite_ident(FILE *,
				      const char *,
				      double);


/**
 * Put the old region-id-color-table into the global object.  A null
 * attribute is set if the material table is empty.
 *
 * Returns -
 * <0 error
 * 0 OK
 */
RT_EXPORT extern int db5_put_color_table(struct db_i *dbip);
RT_EXPORT extern int db5_update_ident(struct db_i *dbip,
				      const char *title,
				      double local2mm);

/**
 *
 * Given that caller already has an external representation of the
 * database object, update it to have a new name (taken from
 * dp->d_namep) in that external representation, and write the new
 * object into the database, obtaining different storage if the size
 * has changed.
 *
 * Changing the name on a v5 object is a relatively expensive
 * operation.
 *
 * Caller is responsible for freeing memory of external
 * representation, using bu_free_external().
 *
 * This routine is used to efficiently support MGED's "cp" and "keep"
 * commands, which don't need to import and decompress objects just to
 * rename and copy them.
 *
 * Returns -
 * -1 error
 * 0 success
 */
RT_EXPORT extern int db_put_external5(struct bu_external *ep,
				      struct directory *dp,
				      struct db_i *dbip);

/**
 * Update an arbitrary number of attributes on a given database
 * object.  For efficiency, this is done without looking at the object
 * body at all.
 *
 * Contents of the bu_attribute_value_set are freed, but not the
 * struct itself.
 *
 * Returns -
 * 0 on success
 * <0 on error
 */
RT_EXPORT extern int db5_update_attributes(struct directory *dp,
					   struct bu_attribute_value_set *avsp,
					   struct db_i *dbip);

/**
 * A convenience routine to update the value of a single attribute.
 *
 * Returns -
 * 0 on success
 * <0 on error
 */
RT_EXPORT extern int db5_update_attribute(const char *obj_name,
					  const char *aname,
					  const char *value,
					  struct db_i *dbip);

/**
 * Replace the attributes of a given database object.
 *
 * For efficiency, this is done without looking at the object body at
 * all.  Contents of the bu_attribute_value_set are freed, but not the
 * struct itself.
 *
 * Returns -
 * 0 on success
 * <0 on error
 */
RT_EXPORT extern int db5_replace_attributes(struct directory *dp,
					    struct bu_attribute_value_set *avsp,
					    struct db_i *dbip);

/**
 *
 * Get attributes for an object pointed to by *dp
 *
 * returns:
 * 0 - all is well
 * <0 - error
 */
RT_EXPORT extern int db5_get_attributes(const struct db_i *dbip,
					struct bu_attribute_value_set *avs,
					const struct directory *dp);

/* db_comb.c */

/**
 * Return count of number of leaf nodes in this tree.
 */
RT_EXPORT extern size_t db_tree_nleaves(const union tree *tp);

/**
 * Take a binary tree in "V4-ready" layout (non-unions pushed below unions,
 * left-heavy), and flatten it into an array layout, ready for conversion
 * back to the GIFT-inspired V4 database format.
 *
 * This is done using the db_non_union_push() routine.
 *
 * If argument 'free' is non-zero, then
 * the non-leaf nodes are freed along the way, to prevent memory leaks.
 * In this case, the caller's copy of 'tp' will be invalid upon return.
 *
 * When invoked at the very top of the tree, the op argument must be OP_UNION.
 */
RT_EXPORT extern struct rt_tree_array *db_flatten_tree(struct rt_tree_array	*rt_tree_array,
						       union tree			*tp,
						       int			op,
						       int			avail,
						       struct resource		*resp);

/**
 * Import a combination record from a V4 database into internal form.
 */
RT_EXPORT extern int rt_comb_import4(struct rt_db_internal	*ip,
				     const struct bu_external	*ep,
				     const mat_t		matrix,		/* NULL if identity */
				     const struct db_i		*dbip,
				     struct resource		*resp);

RT_EXPORT extern int rt_comb_export4(struct bu_external			*ep,
				     const struct rt_db_internal	*ip,
				     double				local2mm,
				     const struct db_i			*dbip,
				     struct resource			*resp);

/**
 * Produce a GIFT-compatible listing, one "member" per line,
 * regardless of the structure of the tree we've been given.
 */
RT_EXPORT extern void db_tree_flatten_describe(struct bu_vls	*vls,
					       const union tree	*tp,
					       int		indented,
					       int		lvl,
					       double		mm2local,
					       struct resource	*resp);

RT_EXPORT extern void db_tree_describe(struct bu_vls	*vls,
				       const union tree	*tp,
				       int		indented,
				       int		lvl,
				       double		mm2local);

RT_EXPORT extern void db_comb_describe(struct bu_vls	*str,
				       const struct rt_comb_internal	*comb,
				       int		verbose,
				       double		mm2local,
				       struct resource	*resp);

/**
 * OBJ[ID_COMBINATION].ft_describe() method
 */
RT_EXPORT extern int rt_comb_describe(struct bu_vls	*str,
				      const struct rt_db_internal *ip,
				      int		verbose,
				      double		mm2local,
				      struct resource *resp,
				      struct db_i *db_i);

/*==================== END g_comb.c / table.c interface ========== */

/**
 * As the v4 database does not really have the notion of "wrapping",
 * this function writes the object name into the
 * proper place (a standard location in all granules).
 */
RT_EXPORT extern void db_wrap_v4_external(struct bu_external *op,
					  const char *name);

/* Some export support routines */

/**
 * Support routine for db_ck_v4gift_tree().
 * Ensure that the tree below 'tp' is left-heavy, i.e. that there are
 * nothing but solids on the right side of any binary operations.
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
RT_EXPORT extern int db_ck_left_heavy_tree(const union tree	*tp,
					   int		no_unions);

/**
 * Look a gift-tree in the mouth.
 * Ensure that this boolean tree conforms to the GIFT convention that
 * union operations must bind the loosest.
 * There are two stages to this check:
 * 1) Ensure that if unions are present they are all at the root of tree,
 * 2) Ensure non-union children of union nodes are all left-heavy
 * (nothing but solid nodes permitted on rhs of binary operators).
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
RT_EXPORT extern int db_ck_v4gift_tree(const union tree *tp);

/**
 * Given a rt_tree_array array, build a tree of "union tree" nodes
 * appropriately connected together.  Every element of the
 * rt_tree_array array used is replaced with a TREE_NULL.
 * Elements which are already TREE_NULL are ignored.
 * Returns a pointer to the top of the tree.
 */
RT_EXPORT extern union tree *db_mkbool_tree(struct rt_tree_array *rt_tree_array,
					    size_t		howfar,
					    struct resource	*resp);

RT_EXPORT extern union tree *db_mkgift_tree(struct rt_tree_array *trees,
					    size_t subtreecount,
					    struct resource *resp);

/**
 * fills in rgb with the color for a given comb combination
 *
 * returns truthfully if a color could be got
 */
RT_EXPORT extern int rt_comb_get_color(unsigned char rgb[3], const struct rt_comb_internal *comb);

/* tgc.c */
RT_EXPORT extern void rt_pt_sort(fastf_t t[],
				 int npts);

/* ell.c */
RT_EXPORT extern void rt_ell_16pts(fastf_t *ov,
				   fastf_t *V,
				   fastf_t *A,
				   fastf_t *B);


/**
 * change all matching object names in the comb tree from old_name to new_name
 *
 * calling function must supply an initialized bu_ptbl, and free it once done.
 */
RT_EXPORT extern int db_comb_mvall(struct directory *dp,
				   struct db_i *dbip,
				   const char *old_name,
				   const char *new_name,
				   struct bu_ptbl *stack);

/* roots.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/roots.c
 *
 * Find the roots of a polynomial
 *
 */

/**
 * WARNING: The polynomial given as input is destroyed by this
 * routine.  The caller must save it if it is important!
 *
 * NOTE : This routine is written for polynomials with real
 * coefficients ONLY.  To use with complex coefficients, the Complex
 * Math library should be used throughout.  Some changes in the
 * algorithm will also be required.
 */
RT_EXPORT extern int rt_poly_roots(bn_poly_t *eqn,
				   bn_complex_t roots[],
				   const char *name);

/** @} */
/* db_io.c */
RT_EXPORT extern int db_write(struct db_i	*dbip,
			      const void *	addr,
			      size_t		count,
			      off_t		offset);

/**
 * Add name from dp->d_namep to external representation of solid, and
 * write it into a file.
 *
 * Caller is responsible for freeing memory of external
 * representation, using bu_free_external().
 *
 * The 'name' field of the external representation is modified to
 * contain the desired name.  The 'ep' parameter cannot be const.
 *
 * THIS ROUTINE ONLY SUPPORTS WRITING V4 GEOMETRY.
 *
 * Returns -
 * <0 error
 * 0 OK
 *
 * NOTE: Callers of this should be using wdb_export_external()
 * instead.
 */
RT_EXPORT extern int db_fwrite_external(FILE			*fp,
					const char		*name,
					struct bu_external	*ep);

/* malloc & read records */

/**
 * Retrieve all records in the database pertaining to an object, and
 * place them in malloc()'ed storage, which the caller is responsible
 * for free()'ing.
 *
 * This loads the combination into a local record buffer.
 * This is in external v4 format.
 *
 * Returns -
 * union record * - OK
 * (union record *)0 - FAILURE
 */
RT_EXPORT extern union record *db_getmrec(const struct db_i *,
					  const struct directory *dp);
/* get several records from db */

/**
 * Retrieve 'len' records from the database, "offset" granules into
 * this entry.
 *
 * Returns -
 * 0 OK
 * -1 FAILURE
 */
RT_EXPORT extern int db_get(const struct db_i *,
			    const struct directory *dp,
			    union record *where,
			    off_t offset,
			    size_t len);
/* put several records into db */

/**
 * Store 'len' records to the database, "offset" granules into this
 * entry.
 *
 * Returns:
 * 0 OK
 * non-0 FAILURE
 */
RT_EXPORT extern int db_put(struct db_i *,
			    const struct directory *dp,
			    union record *where,
			    off_t offset, size_t len);

/**
 * Obtains a object from the database, leaving it in external (on-disk)
 * format.
 *
 * The bu_external structure represented by 'ep' is initialized here,
 * the caller need not pre-initialize it.  On error, 'ep' is left
 * un-initialized and need not be freed, to simplify error recovery.
 * On success, the caller is responsible for calling
 * bu_free_external(ep);
 *
 * Returns -
 * -1 error
 * 0 success
 */
RT_EXPORT extern int db_get_external(struct bu_external *ep,
				     const struct directory *dp,
				     const struct db_i *dbip);

/**
 * Given that caller already has an external representation of the
 * database object, update it to have a new name (taken from
 * dp->d_namep) in that external representation, and write the new
 * object into the database, obtaining different storage if the size
 * has changed.
 *
 * Caller is responsible for freeing memory of external
 * representation, using bu_free_external().
 *
 * This routine is used to efficiently support MGED's "cp" and "keep"
 * commands, which don't need to import objects just to rename and
 * copy them.
 *
 * Returns -
 * <0 error
 * 0 success
 */
RT_EXPORT extern int db_put_external(struct bu_external *ep,
				     struct directory *dp,
				     struct db_i *dbip);

/* db_scan.c */
/* read db (to build directory) */
RT_EXPORT extern int db_scan(struct db_i *,
			     int (*handler)(struct db_i *,
					    const char *name,
					    off_t addr,
					    size_t nrec,
					    int flags,
					    void *client_data),
			     int do_old_matter,
			     void *client_data);
/* update db unit conversions */
#define db_ident(a, b, c)		+++error+++

/**
 * Update the _GLOBAL object, which in v5 serves the place of the
 * "ident" header record in v4 as the place to stash global
 * information.  Since every database will have one of these things,
 * it's no problem to update it.
 *
 * Returns -
 * 0 Success
 * -1 Fatal Error
 */
RT_EXPORT extern int db_update_ident(struct db_i *dbip,
				     const char *title,
				     double local2mm);

/**
 * Create a header for a v5 database.
 *
 * This routine has the same calling sequence as db_fwrite_ident()
 * which makes a v4 database header.
 *
 * In the v5 database, two database objects must be created to match
 * the semantics of what was in the v4 header:
 *
 * First, a database header object.
 *
 * Second, create a specially named attribute-only object which
 * contains the attributes "title=" and "units=" with the values of
 * title and local2mm respectively.
 *
 * Note that the current working units are specified as a conversion
 * factor to millimeters because database dimensional values are
 * always stored as millimeters (mm).  The units conversion factor
 * only affects the display and conversion of input values.  This
 * helps prevent error accumulation and improves numerical stability
 * when calculations are made.
 *
 * This routine should only be used by db_create().  Everyone else
 * should use db5_update_ident().
 *
 * Returns -
 * 0 Success
 * -1 Fatal Error
 */
RT_EXPORT extern int db_fwrite_ident(FILE *fp,
				     const char *title,
				     double local2mm);

/**
 * Initialize conversion factors given the v4 database unit
 */
RT_EXPORT extern void db_conversions(struct db_i *,
				     int units);

/**
 * Given a string, return the V4 database code representing the user's
 * preferred editing units.  The v4 database format does not have many
 * choices.
 *
 * Returns -
 * -1 Not a legal V4 database code
 * # The V4 database code number
 */
RT_EXPORT extern int db_v4_get_units_code(const char *str);

/* db5_scan.c */

/**
 * A generic routine to determine the type of the database, (v4 or v5)
 * and to invoke the appropriate db_scan()-like routine to build the
 * in-memory directory.
 *
 * It is the caller's responsibility to close the database in case of
 * error.
 *
 * Called from rt_dirbuild() and other places directly where a
 * raytrace instance is not required.
 *
 * Returns -
 * 0 OK
 * -1 failure
 */
RT_EXPORT extern int db_dirbuild(struct db_i *dbip);
RT_EXPORT extern struct directory *db5_diradd(struct db_i *dbip,
					      const struct db5_raw_internal *rip,
					      off_t laddr,
					      void *client_data);

/**
 * Scan a v5 database, sending each object off to a handler.
 *
 * Returns -
 * 0 Success
 * -1 Fatal Error
 */
RT_EXPORT extern int db5_scan(struct db_i *dbip,
			      void (*handler)(struct db_i *,
					      const struct db5_raw_internal *,
					      off_t addr,
					      void *client_data),
			      void *client_data);

/**
 * obtain the database version for a given database instance.
 *
 * presently returns only a 4 or 5 accordingly.
 */
RT_EXPORT extern int db_version(struct db_i *dbip);


/* db_corrupt.c */

/**
 * Detect whether a given geometry database file seems to be corrupt
 * or invalid due to flipped endianness.  Only relevant for v4
 * geometry files that are binary-incompatible with the runtime
 * platform.
 *
 * Returns true if flipping the endian type fixes all combination
 * member matrices.
 */
RT_EXPORT extern int rt_db_flip_endian(struct db_i *dbip);


/* db5_comb.c */

/**
 * Read a combination object in v5 external (on-disk) format, and
 * convert it into the internal format described in rtgeom.h
 *
 * This is an unusual conversion, because some of the data is taken
 * from attributes, not just from the object body.  By the time this
 * is called, the attributes will already have been cracked into
 * ip->idb_avs, we get the attributes from there.
 *
 * Returns -
 * 0 OK
 * -1 FAIL
 */
RT_EXPORT extern int rt_comb_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);

/* extrude.c */
RT_EXPORT extern int rt_extrude_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);


/**
 * "open" an in-memory-only database instance.  this initializes a
 * dbip for use, creating an inmem dbi_wdbp as the means to add
 * geometry to the directory (use wdb_export_external()).
 */
RT_EXPORT extern struct db_i * db_open_inmem(void);

/**
 * creates an in-memory-only database.  this is very similar to
 * db_open_inmem() with the exception that the this routine adds a
 * default _GLOBAL object.
 */
RT_EXPORT extern struct db_i * db_create_inmem(void);


/**
 * Transmogrify an existing directory entry to be an in-memory-only
 * one, stealing the external representation from 'ext'.
 */
RT_EXPORT extern void db_inmem(struct directory	*dp,
			       struct bu_external	*ext,
			       int		flags,
			       struct db_i	*dbip);

/* db_lookup.c */

/**
 * Return the number of "struct directory" nodes in the given
 * database.
 */
RT_EXPORT extern size_t db_directory_size(const struct db_i *dbip);

/**
 * For debugging, ensure that all the linked-lists for the directory
 * structure are intact.
 */
RT_EXPORT extern void db_ck_directory(const struct db_i *dbip);

/**
 * Returns -
 * 0 if the in-memory directory is empty
 * 1 if the in-memory directory has entries,
 * which implies that a db_scan() has already been performed.
 */
RT_EXPORT extern int db_is_directory_non_empty(const struct db_i	*dbip);

/**
 * Returns a hash index for a given string that corresponds with the
 * head of that string's hash chain.
 */
RT_EXPORT extern int db_dirhash(const char *str);

/**
 * Name -
 * Description -
 * This routine ensures that ret_name is not already in the
 * directory. If it is, it tries a fixed number of times to
 * modify ret_name before giving up. Note - most of the time,
 * the hash for ret_name is computed once.
 *
 * Inputs -
 * dbip database instance pointer
 * ret_name the original name
 * noisy to blather or not
 *
 * Outputs -
 * ret_name the name to use
 * headp pointer to the first (struct directory *) in the bucket
 *
 * Returns -
 * 0 success
 * <0 fail
 */
RT_EXPORT extern int db_dircheck(struct db_i *dbip,
				 struct bu_vls *ret_name,
				 int noisy,
				 struct directory ***headp);
/* convert name to directory ptr */

/**
 * This routine takes a name and looks it up in the directory table.
 * If the name is present, a pointer to the directory struct element
 * is returned, otherwise NULL is returned.
 *
 * If noisy is non-zero, a print occurs, else only the return code
 * indicates failure.
 *
 * Returns -
 * struct directory if name is found
 * RT_DIR_NULL on failure
 */
RT_EXPORT extern struct directory *db_lookup(const struct db_i *,
					     const char *name,
					     int noisy);
/* lookup directory entries based on attributes */

/**
 * lookup directory entries based on directory flags (dp->d_flags) and
 * attributes the "dir_flags" arg is a mask for the directory flags
 * the *"avs" is an attribute value set used to select from the
 * objects that *pass the flags mask. if "op" is 1, then the object
 * must have all the *attributes and values that appear in "avs" in
 * order to be *selected. If "op" is 2, then the object must have at
 * least one of *the attribute/value pairs from "avs".
 *
 * dir_flags are in the form used in struct directory (d_flags)
 *
 * for op:
 * 1 -> all attribute name/value pairs must be present and match
 * 2 -> at least one of the name/value pairs must be present and match
 *
 * returns a ptbl list of selected directory pointers an empty list
 * means nothing met the requirements a NULL return means something
 * went wrong.
 */
RT_EXPORT extern struct bu_ptbl *db_lookup_by_attr(struct db_i *dbip,
						   int dir_flags,
						   struct bu_attribute_value_set *avs,
						   int op);

/* add entry to directory */

/**
 * Add an entry to the directory.  Try to make the regular path
 * through the code as fast as possible, to speed up building the
 * table of contents.
 *
 * dbip is a pointer to a valid/opened database instance
 *
 * name is the string name of the object being added
 *
 * laddr is the offset into the file to the object
 *
 * len is the length of the object, number of db granules used
 *
 * flags are defined in raytrace.h (RT_DIR_SOLID, RT_DIR_COMB, RT_DIR_REGION,
 * RT_DIR_INMEM, etc.) for db version 5, ptr is the minor_type
 * (non-null pointer to valid unsigned char code)
 *
 * an laddr of RT_DIR_PHONY_ADDR means that database storage has not
 * been allocated yet.
 */
RT_EXPORT extern struct directory *db_diradd(struct db_i *,
					     const char *name,
					     off_t laddr,
					     size_t len,
					     int flags,
					     void *ptr);
RT_EXPORT extern struct directory *db_diradd5(struct db_i *dbip,
					      const char *name,
					      off_t				laddr,
					      unsigned char			major_type,
					      unsigned char 			minor_type,
					      unsigned char			name_hidden,
					      size_t				object_length,
					      struct bu_attribute_value_set	*avs);

/* delete entry from directory */

/**
 * Given a pointer to a directory entry, remove it from the linked
 * list, and free the associated memory.
 *
 * It is the responsibility of the caller to have released whatever
 * structures have been hung on the d_use_hd bu_list, first.
 *
 * Returns -
 * 0 on success
 * non-0 on failure
 */
RT_EXPORT extern int db_dirdelete(struct db_i *,
				  struct directory *dp);
RT_EXPORT extern int db_fwrite_ident(FILE *,
				     const char *,
				     double);

/**
 * For debugging, print the entire contents of the database directory.
 */
RT_EXPORT extern void db_pr_dir(const struct db_i *dbip);

/**
 * Change the name string of a directory entry.  Because of the
 * hashing function, this takes some extra work.
 *
 * Returns -
 * 0 on success
 * non-0 on failure
 */
RT_EXPORT extern int db_rename(struct db_i *,
			       struct directory *,
			       const char *newname);


/**
 * Updates the d_nref fields (which count the number of times a given entry
 * is referenced by a COMBination in the database).
 *
 */
RT_EXPORT extern void db_update_nref(struct db_i *dbip,
				     struct resource *resp);


/**
 * DEPRECATED: Use bu_fnmatch() instead of this function.
 *
 * If string matches pattern, return 1, else return 0
 *
 * special characters:
 *	*	Matches any string including the null string.
 *	?	Matches any single character.
 *	[...]	Matches any one of the characters enclosed.
 *	-	May be used inside brackets to specify range
 *		(i.e. str[1-58] matches str1, str2, ... str5, str8)
 *	\	Escapes special characters.
 */
DEPRECATED RT_EXPORT extern int db_regexp_match(const char *pattern,
						const char *string);


/**
 * Appends a list of all database matches to the given vls, or the pattern
 * itself if no matches are found.
 * Returns the number of matches.
 */
RT_EXPORT extern int db_regexp_match_all(struct bu_vls *dest,
					 struct db_i *dbip,
					 const char *pattern);

/* db_ls.c */
/**
 * db_ls takes a database instance pointer and assembles a
 * directory pointer array of objects in the database according
 * to a set of flags.
 *
 * The caller is responsible for freeing the array.
 *
 * Returns -
 * integer count of objects in dpv
 * struct directory ** array of objects in dpv via argument
 *
 * WARNING:  THIS FUNCTION IS STILL IN DEVELOPMENT - IT IS NOT YET
 * ASSUMED THAT THIS IS ITS FINAL FORM - DO NOT DEPEND ON IT REMAINING
 * THE SAME UNTIL THIS WARNING IS REMOVED
 */
#define DB_LS_PRIM         0x1    /* filter for primitives (solids)*/
#define DB_LS_COMB         0x2    /* filter for combinations */
#define DB_LS_REGION       0x4    /* filter for regions */
#define DB_LS_HIDDEN       0x8    /* include hidden objects in results */
#define DB_LS_NON_GEOM     0x10   /* filter for non-geometry objects */
#define DB_LS_TOPS         0x20   /* filter for objects un-referenced by other objects */
RT_EXPORT extern int db_ls(const struct db_i *dbip,
		           int flags,
			   struct directory ***dpv);

/**
 * convert an argv list of names to a directory pointer array.
 *
 * If db_lookup fails for any individual argv, an empty directory
 * structure is created and assigned the name and RT_DIR_PHONY_ADDR
 *
 * The returned directory ** structure is NULL terminated.
 */
RT_EXPORT extern struct directory **db_argv_to_dpv(const struct db_i *dbip,
						   const char **argv);


/**
 * convert a directory pointer array to an argv char pointer array.
 */
RT_EXPORT extern char **db_dpv_to_argv(struct directory **dpv);


/* db_flags.c */
/**
 * Given the internal form of a database object, return the
 * appropriate 'flags' word for stashing in the in-memory directory of
 * objects.
 */
RT_EXPORT extern int db_flags_internal(const struct rt_db_internal *intern);


/* XXX - should use in db5_diradd() */
/**
 * Given a database object in "raw" internal form, return the
 * appropriate 'flags' word for stashing in the in-memory directory of
 * objects.
 */
RT_EXPORT extern int db_flags_raw_internal(const struct db5_raw_internal *intern);

/* db_alloc.c */

/* allocate "count" granules */
RT_EXPORT extern int db_alloc(struct db_i *,
			      struct directory *dp,
			      size_t count);
/* delete "recnum" from entry */
RT_EXPORT extern int db_delrec(struct db_i *,
			       struct directory *dp,
			       int recnum);
/* delete all granules assigned dp */
RT_EXPORT extern int db_delete(struct db_i *,
			       struct directory *dp);
/* write FREE records from 'start' */
RT_EXPORT extern int db_zapper(struct db_i *,
			       struct directory *dp,
			       size_t start);

/**
 * This routine is called by the RT_GET_DIRECTORY macro when the
 * freelist is exhausted.  Rather than simply getting one additional
 * structure, we get a whole batch, saving overhead.
 */
RT_EXPORT extern void db_alloc_directory_block(struct resource *resp);

/**
 * This routine is called by the GET_SEG macro when the freelist is
 * exhausted.  Rather than simply getting one additional structure, we
 * get a whole batch, saving overhead.  When this routine is called,
 * the seg resource must already be locked.  malloc() locking is done
 * in bu_malloc.
 */
RT_EXPORT extern void rt_alloc_seg_block(struct resource *res);

/* db_tree.c */

/**
 * Duplicate the contents of a db_tree_state structure, including a
 * private copy of the ts_mater field(s) and the attribute/value set.
 */
RT_EXPORT extern void db_dup_db_tree_state(struct db_tree_state *otsp,
					   const struct db_tree_state *itsp);

/**
 * Release dynamic fields inside the structure, but not the structure
 * itself.
 */
RT_EXPORT extern void db_free_db_tree_state(struct db_tree_state *tsp);

/**
 * In most cases, you don't want to use this routine, you want to
 * struct copy mged_initial_tree_state or rt_initial_tree_state, and
 * then set ts_dbip in your copy.
 */
RT_EXPORT extern void db_init_db_tree_state(struct db_tree_state *tsp,
					    struct db_i *dbip,
					    struct resource *resp);
RT_EXPORT extern struct combined_tree_state *db_new_combined_tree_state(const struct db_tree_state *tsp,
									const struct db_full_path *pathp);
RT_EXPORT extern struct combined_tree_state *db_dup_combined_tree_state(const struct combined_tree_state *old);
RT_EXPORT extern void db_free_combined_tree_state(struct combined_tree_state *ctsp);
RT_EXPORT extern void db_pr_tree_state(const struct db_tree_state *tsp);
RT_EXPORT extern void db_pr_combined_tree_state(const struct combined_tree_state *ctsp);

/**
 * Handle inheritance of material property found in combination
 * record.  Color and the material property have separate inheritance
 * interlocks.
 *
 * Returns -
 * -1 failure
 * 0 success
 * 1 success, this is the top of a new region.
 */
RT_EXPORT extern int db_apply_state_from_comb(struct db_tree_state *tsp,
					      const struct db_full_path *pathp,
					      const struct rt_comb_internal *comb);

/**
 * Updates state via *tsp, pushes member's directory entry on *pathp.
 * (Caller is responsible for popping it).
 *
 * Returns -
 * -1 failure
 * 0 success, member pushed on path
 */
RT_EXPORT extern int db_apply_state_from_memb(struct db_tree_state *tsp,
					      struct db_full_path *pathp,
					      const union tree *tp);

/**
 * Returns -
 * -1 found member, failed to apply state
 * 0 unable to find member 'cp'
 * 1 state applied OK
 */
RT_EXPORT extern int db_apply_state_from_one_member(struct db_tree_state *tsp,
						    struct db_full_path *pathp,
						    const char *cp,
						    int sofar,
						    const union tree *tp);

/**
 * The search stops on the first match.
 *
 * Returns -
 * tp if found
 * TREE_NULL if not found in this tree
 */
RT_EXPORT extern union tree *db_find_named_leaf(union tree *tp, const char *cp);

/**
 * The search stops on the first match.
 *
 * Returns -
 * TREE_NULL if not found in this tree
 * tp if found
 * *side == 1 if leaf is on lhs.
 * *side == 2 if leaf is on rhs.
 *
 */
RT_EXPORT extern union tree *db_find_named_leafs_parent(int *side,
							union tree *tp,
							const char *cp);
RT_EXPORT extern void db_tree_del_lhs(union tree *tp,
				      struct resource *resp);
RT_EXPORT extern void db_tree_del_rhs(union tree *tp,
				      struct resource *resp);

/**
 * Given a name presumably referenced in a OP_DB_LEAF node, delete
 * that node, and the operation node that references it.  Not that
 * this may not produce an equivalent tree, for example when rewriting
 * (A - subtree) as (subtree), but that will be up to the caller/user
 * to adjust.  This routine gets rid of exactly two nodes in the tree:
 * leaf, and op.  Use some other routine if you wish to kill the
 * entire rhs below "-" and "intersect" nodes.
 *
 * The two nodes deleted will have their memory freed.
 *
 * If the tree is a single OP_DB_LEAF node, the leaf is freed and
 * *tp is set to NULL.
 *
 * Returns -
 * -3 Internal error
 * -2 Tree is empty
 * -1 Unable to find OP_DB_LEAF node specified by 'cp'.
 * 0 OK
 */
RT_EXPORT extern int db_tree_del_dbleaf(union tree **tp,
					const char *cp,
					struct resource *resp,
					int nflag);

/**
 * Multiply on the left every matrix found in a DB_LEAF node in a
 * tree.
 */
RT_EXPORT extern void db_tree_mul_dbleaf(union tree *tp,
					 const mat_t mat);

/**
 * This routine traverses a combination (union tree) in LNR order and
 * calls the provided function for each OP_DB_LEAF node.  Note that
 * this routine does not go outside this one combination!!!!
 *
 * was previously named comb_functree()
 */
RT_EXPORT extern void db_tree_funcleaf(struct db_i		*dbip,
				       struct rt_comb_internal	*comb,
				       union tree		*comb_tree,
                                       void (*leaf_func)(struct db_i *, struct rt_comb_internal *, union tree *,
                                                         void *, void *, void *, void *),
				       void *		user_ptr1,
				       void *		user_ptr2,
				       void *		user_ptr3,
				       void *		user_ptr4);

/**
 * Starting with possible prior partial path and corresponding
 * accumulated state, follow the path given by "new_path", updating
 * *tsp and *total_path with full state information along the way.  In
 * a better world, there would have been a "combined_tree_state" arg.
 *
 * Parameter 'depth' controls how much of 'new_path' is used:
 *
 * 0 use all of new_path
 * >0 use only this many of the first elements of the path
 * <0 use all but this many path elements.
 *
 * A much more complete version of rt_plookup() and pathHmat().  There
 * is also a TCL interface.
 *
 * Returns -
 * 0 success (plus *tsp is updated)
 * -1 error (*tsp values are not useful)
 */
RT_EXPORT extern int db_follow_path(struct db_tree_state *tsp,
				    struct db_full_path *total_path,
				    const struct db_full_path *new_path,
				    int noisy,
				    long pdepth);

/**
 * Follow the slash-separated path given by "cp", and update *tsp and
 * *total_path with full state information along the way.
 *
 * A much more complete version of rt_plookup().
 *
 * Returns -
 * 0 success (plus *tsp is updated)
 * -1 error (*tsp values are not useful)
 */
RT_EXPORT extern int db_follow_path_for_state(struct db_tree_state *tsp,
					      struct db_full_path *pathp,
					      const char *orig_str, int noisy);

/**
 * Recurse down the tree, finding all the leaves (or finding just all
 * the regions).
 *
 * ts_region_start_func() is called to permit regions to be skipped.
 * It is not intended to be used for collecting state.
 */
RT_EXPORT extern union tree *db_recurse(struct db_tree_state	*tsp,
					struct db_full_path *pathp,
					struct combined_tree_state **region_start_statepp,
					void *client_data);
RT_EXPORT extern union tree *db_dup_subtree(const union tree *tp,
					    struct resource *resp);
RT_EXPORT extern void db_ck_tree(const union tree *tp);


/**
 * Release all storage associated with node 'tp', including children
 * nodes.
 */
RT_EXPORT extern void db_free_tree(union tree *tp,
				   struct resource *resp);


/**
 * Re-balance this node to make it left heavy.  Union operators will
 * be moved to left side.  when finished "tp" MUST still point to top
 * node of this subtree.
 */
RT_EXPORT extern void db_left_hvy_node(union tree *tp);


/**
 * If there are non-union operations in the tree, above the region
 * nodes, then rewrite the tree so that the entire tree top is nothing
 * but union operations, and any non-union operations are clustered
 * down near the region nodes.
 */
RT_EXPORT extern void db_non_union_push(union tree *tp,
					struct resource *resp);


/**
 * Return a count of the number of "union tree" nodes below "tp",
 * including tp.
 */
RT_EXPORT extern int db_count_tree_nodes(const union tree *tp,
					 int count);


/**
 * Returns -
 * 1 if this tree contains nothing but union operations.
 * 0 if at least one subtraction or intersection op exists.
 */
RT_EXPORT extern int db_is_tree_all_unions(const union tree *tp);
RT_EXPORT extern int db_count_subtree_regions(const union tree *tp);
RT_EXPORT extern int db_tally_subtree_regions(union tree	*tp,
					      union tree	**reg_trees,
					      int		cur,
					      int		lim,
					      struct resource *resp);

/**
 * This is the top interface to the "tree walker."
 *
 * Parameters:
 *	rtip		rt_i structure to database (open with rt_dirbuild())
 *	argc		# of tree-tops named
 *	argv		names of tree-tops to process
 *	init_state	Input parameter: initial state of the tree.
 *			For example:  rt_initial_tree_state,
 *			and mged_initial_tree_state.
 *
 * These parameters are pointers to callback routines.
 * If NULL, they won't be called.
 *
 *	reg_start_func	Called at beginning of each region, before
 *			visiting any nodes within the region.  Return
 *			0 if region should be skipped without
 *			recursing, otherwise non-zero.  DO NOT USE FOR
 *			OTHER PURPOSES!  For example, can be used to
 *			quickly skip air regions.
 *
 *	reg_end_func    Called after all nodes within a region have been
 *			recursively processed by leaf_func.  If it
 *			wants to retain 'curtree' then it may steal
 *			that pointer and return TREE_NULL.  If it
 *			wants us to clean up some or all of that tree,
 *			then it returns a non-null (union tree *)
 *			pointer, and that tree is safely freed in a
 *			non-parallel section before we return.
 *
 *	leaf_func	Function to process a leaf node.  It is actually
 *			invoked from db_recurse() from
 *			_db_walk_subtree().  Returns (union tree *)
 *			representing the leaf, or TREE_NULL if leaf
 *			does not exist or has an error.
 *
 *
 * This routine will employ multiple CPUs if asked, but is not
 * multiply-parallel-recursive.  Call this routine with ncpu > 1 from
 * serial code only.  When called from within an existing thread, ncpu
 * must be 1.
 *
 * If ncpu > 1, the caller is responsible for making sure that
 * RTG.rtg_parallel is non-zero, and that the bu_semaphore_init()
 * functions has been performed, first.
 *
 * Plucks per-cpu resources out of rtip->rti_resources[].  They need
 * to have been initialized first.
 *
 * Returns -
 * -1 Failure to prepare even a single sub-tree
 * 0 OK
 */
RT_EXPORT extern int db_walk_tree(struct db_i *dbip,
				  int argc,
				  const char **argv,
				  int ncpu,
				  const struct db_tree_state *init_state,
				  int (*reg_start_func) (struct db_tree_state * /*tsp*/,
							 const struct db_full_path * /*pathp*/,
							 const struct rt_comb_internal * /* combp */,
							 void *client_data),
				  union tree *(*reg_end_func) (struct db_tree_state * /*tsp*/,
							       const struct db_full_path * /*pathp*/,
							       union tree * /*curtree*/,
							       void *client_data),
				  union tree *(*leaf_func) (struct db_tree_state * /*tsp*/,
							    const struct db_full_path * /*pathp*/,
							    struct rt_db_internal * /*ip*/,
							    void *client_data),
				  void *client_data);

/**
 * Returns -
 * 1 OK, path matrix written into 'mat'.
 * 0 FAIL
 *
 * Called in librt/db_tree.c, mged/dodraw.c, and mged/animedit.c
 */
RT_EXPORT extern int db_path_to_mat(struct db_i		*dbip,
				    struct db_full_path	*pathp,
				    mat_t			mat,		/* result */
				    int			depth,		/* number of arcs */
				    struct resource		*resp);

/**
 * 'arc' may be a null pointer, signifying an identity matrix.
 * 'materp' may be a null pointer, signifying that the region has
 * already been finalized above this point in the tree.
 */
RT_EXPORT extern void db_apply_anims(struct db_full_path *pathp,
				     struct directory *dp,
				     mat_t stck,
				     mat_t arc,
				     struct mater_info *materp);

/**
 * Given the name of a region, return the matrix which maps model
 * coordinates into "region" coordinates.
 *
 * Returns:
 * 0 OK
 * <0 Failure
 */
RT_EXPORT extern int db_region_mat(mat_t		m,		/* result */
				   struct db_i	*dbip,
				   const char	*name,
				   struct resource *resp);


/**
 * XXX given that this routine depends on rtip, it should be called
 * XXX rt_shader_mat().
 *
 * Given a region, return a matrix which maps model coordinates into
 * region "shader space".  This is a space where points in the model
 * within the bounding box of the region are mapped into "region"
 * space (the coordinate system in which the region is defined).  The
 * area occupied by the region's bounding box (in region coordinates)
 * are then mapped into the unit cube.  This unit cube defines "shader
 * space".
 *
 * Returns:
 * 0 OK
 * <0 Failure
 */
RT_EXPORT extern int rt_shader_mat(mat_t			model_to_shader,	/* result */
				   const struct rt_i	*rtip,
				   const struct region	*rp,
				   point_t			p_min,	/* input/output: shader/region min point */
				   point_t			p_max,	/* input/output: shader/region max point */
				   struct resource		*resp);

/**
 * Fills a bu_vls with a representation of the given tree appropriate
 * for processing by Tcl scripts.
 *
 * A tree 't' is represented in the following manner:
 *
 * t := { l dbobjname { mat } }
 *	   | { l dbobjname }
 *	   | { u t1 t2 }
 * 	   | { n t1 t2 }
 *	   | { - t1 t2 }
 *	   | { ^ t1 t2 }
 *         | { ! t1 }
 *	   | { G t1 }
 *	   | { X t1 }
 *	   | { N }
 *	   | {}
 *
 * where 'dbobjname' is a string containing the name of a database object,
 *       'mat'       is the matrix preceding a leaf,
 *       't1', 't2'  are trees (recursively defined).
 *
 * Notice that in most cases, this tree will be grossly unbalanced.
 */
RT_EXPORT extern int db_tree_list(struct bu_vls *vls, const union tree *tp);

/**
 * Take a TCL-style string description of a binary tree, as produced
 * by db_tree_list(), and reconstruct the in-memory form of that tree.
 */
RT_EXPORT extern union tree *db_tree_parse(struct bu_vls *vls, const char *str, struct resource *resp);


/* dir.c */

/**
 * Read named MGED db, build toc.
 */
RT_EXPORT extern struct rt_i *rt_dirbuild(const char *filename, char *buf, int len);

/**
 * Get an object from the database, and convert it into its internal
 * representation.
 *
 * Returns -
 * <0 On error
 * id On success.
 */
RT_EXPORT extern int rt_db_get_internal(struct rt_db_internal	*ip,
					const struct directory	*dp,
					const struct db_i	*dbip,
					const mat_t		mat,
					struct resource		*resp);

/**
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.  On success only, the internal
 * representation is freed.
 *
 * Returns -
 * <0 error
 * 0 success
 */
RT_EXPORT extern int rt_db_put_internal(struct directory	*dp,
					struct db_i		*dbip,
					struct rt_db_internal	*ip,
					struct resource		*resp);

/**
 * Put an object in internal format out onto a file in external
 * format.  Used by LIBWDB.
 *
 * Can't really require a dbip parameter, as many callers won't have one.
 *
 * THIS ROUTINE ONLY SUPPORTS WRITING V4 GEOMETRY.
 *
 * Returns -
 * 0 OK
 * <0 error
 */
RT_EXPORT extern int rt_fwrite_internal(FILE *fp,
					const char *name,
					const struct rt_db_internal *ip,
					double conv2mm);
RT_EXPORT extern void rt_db_free_internal(struct rt_db_internal *ip);


/**
 * Convert an object name to a rt_db_internal pointer
 *
 * Looks up the named object in the directory of the specified model,
 * obtaining a directory pointer.  Then gets that object from the
 * database and constructs its internal representation.  Returns
 * ID_NULL on error, otherwise returns the type of the object.
 */
RT_EXPORT extern int rt_db_lookup_internal(struct db_i *dbip,
					   const char *obj_name,
					   struct directory **dpp,
					   struct rt_db_internal *ip,
					   int noisy,
					   struct resource *resp);

RT_EXPORT extern void rt_optim_tree(union tree *tp,
				    struct resource *resp);

/**
 * This subroutine is called for a no-frills tree-walk, with the
 * provided subroutines being called at every combination and leaf
 * (solid) node, respectively.
 *
 * This routine is recursive, so no variables may be declared static.
 */
RT_EXPORT extern void db_functree(struct db_i *dbip,
				  struct directory *dp,
				  void (*comb_func)(struct db_i *,
						    struct directory *,
						    void *),
				  void (*leaf_func)(struct db_i *,
						    struct directory *,
						    void *),
				  struct resource *resp,
				  void *client_data);

/* mirror.c */
RT_EXPORT extern struct rt_db_internal *rt_mirror(struct db_i *dpip,
						  struct rt_db_internal *ip,
						  point_t mirror_pt,
						  vect_t mirror_dir,
						  struct resource *resp);

/*
  RT_EXPORT extern void db_preorder_traverse(struct directory *dp,
  struct db_traverse *dtp);
*/

/* arb8.c */
RT_EXPORT extern int rt_arb_get_cgtype(
    int *cgtype,
    struct rt_arb_internal *arb,
    const struct bn_tol *tol,
    register int *uvec,  /* array of indexes to unique points in arb->pt[] */
    register int *svec); /* array of indexes to like points in arb->pt[] */
RT_EXPORT extern int rt_arb_std_type(const struct rt_db_internal *ip,
				     const struct bn_tol *tol);
RT_EXPORT extern void rt_arb_centroid(point_t                       *cent,
				      const struct rt_db_internal   *ip);
RT_EXPORT extern int rt_arb_calc_points(struct rt_arb_internal *arb, int cgtype, const plane_t planes[6], const struct bn_tol *tol);		/* needs wdb.h for arg list */
RT_EXPORT extern int rt_arb_check_points(struct rt_arb_internal *arb,
					 int cgtype,
					 const struct bn_tol *tol);
RT_EXPORT extern int rt_arb_3face_intersect(point_t			point,
					    const plane_t		planes[6],
					    int			type,		/* 4..8 */
					    int			loc);
RT_EXPORT extern int rt_arb_calc_planes(struct bu_vls		*error_msg_ret,
					struct rt_arb_internal	*arb,
					int			type,
					plane_t			planes[6],
					const struct bn_tol	*tol);
RT_EXPORT extern int rt_arb_move_edge(struct bu_vls		*error_msg_ret,
				      struct rt_arb_internal	*arb,
				      vect_t			thru,
				      int			bp1,
				      int			bp2,
				      int			end1,
				      int			end2,
				      const vect_t		dir,
				      plane_t			planes[6],
				      const struct bn_tol	*tol);
RT_EXPORT extern int rt_arb_edit(struct bu_vls		*error_msg_ret,
				 struct rt_arb_internal	*arb,
				 int			arb_type,
				 int			edit_type,
				 vect_t			pos_model,
				 plane_t			planes[6],
				 const struct bn_tol	*tol);
RT_EXPORT extern int rt_arb_find_e_nearest_pt2(int *edge, int *vert1, int *vert2, const struct rt_db_internal *ip, const point_t pt2, const mat_t mat, fastf_t ptol);

RT_EXPORT extern const int rt_arb_faces[5][24];
RT_EXPORT extern short earb8[12][18];
RT_EXPORT extern short earb7[12][18];
RT_EXPORT extern short earb6[10][18];
RT_EXPORT extern short earb5[9][18];
RT_EXPORT extern short earb4[5][18];

RT_EXPORT extern short arb8_edge_vertex_mapping[12][2];
RT_EXPORT extern short arb7_edge_vertex_mapping[12][2];
RT_EXPORT extern short arb6_edge_vertex_mapping[10][2];
RT_EXPORT extern short arb5_edge_vertex_mapping[9][2];
RT_EXPORT extern short arb4_edge_vertex_mapping[5][2];

/* epa.c */
RT_EXPORT extern void rt_ell(fastf_t *ov,
			     const fastf_t *V,
			     const fastf_t *A,
			     const fastf_t *B,
			     int sides);

/* pipe.c */
RT_EXPORT extern void rt_vls_pipept(struct bu_vls *vp,
				    int seg_no,
				    const struct rt_db_internal *ip,
				    double mm2local);
RT_EXPORT extern void rt_pipept_print(const struct wdb_pipept *pipept, double mm2local);
RT_EXPORT extern int rt_pipe_ck(const struct bu_list *headp);

/* metaball.c */
struct rt_metaball_internal;
RT_EXPORT extern void rt_vls_metaballpt(struct bu_vls *vp,
					const int pt_no,
					const struct rt_db_internal *ip,
					const double mm2local);
RT_EXPORT extern void rt_metaballpt_print(const struct wdb_metaballpt *metaball, double mm2local);
RT_EXPORT extern int rt_metaball_ck(const struct bu_list *headp);
RT_EXPORT extern fastf_t rt_metaball_point_value(const point_t *p,
						 const struct rt_metaball_internal *mb);
RT_EXPORT extern int rt_metaball_point_inside(const point_t *p,
					      const struct rt_metaball_internal *mb);
RT_EXPORT extern int rt_metaball_lookup_type_id(const char *name);
RT_EXPORT extern const char *rt_metaball_lookup_type_name(const int id);
RT_EXPORT extern int rt_metaball_add_point(struct rt_metaball_internal *,
					   const point_t *loc,
					   const fastf_t fldstr,
					   const fastf_t goo);

/* rpc.c */
RT_EXPORT extern int rt_mk_parabola(struct rt_pt_node *pts,
				    fastf_t r,
				    fastf_t b,
				    fastf_t dtol,
				    fastf_t ntol);

/* memalloc.c -- non PARALLEL routines */

/**
 * Takes:		& pointer of map,
 * size.
 *
 * Returns:	NULL Error
 * <addr> Otherwise
 *
 * Comments:
 * Algorithm is first fit.
 */
RT_EXPORT extern size_t rt_memalloc(struct mem_map **pp,
				    size_t size);

/**
 * Takes:		& pointer of map,
 * size.
 *
 * Returns:	NULL Error
 * <addr> Otherwise
 *
 * Comments:
 * Algorithm is BEST fit.
 */
RT_EXPORT extern struct mem_map * rt_memalloc_nosplit(struct mem_map **pp,
						      size_t size);

/**
 * Returns:	NULL Error
 * <addr> Otherwise
 *
 * Comments:
 * Algorithm is first fit.
 * Free space can be split
 */
RT_EXPORT extern size_t rt_memget(struct mem_map **pp,
				  size_t size,
				  off_t place);

/**
 * Takes:
 * size,
 * address.
 *
 * Comments:
 * The routine does not check for wrap around when increasing sizes
 * or changing addresses.  Other wrap-around conditions are flagged.
 */
RT_EXPORT extern void rt_memfree(struct mem_map **pp,
				 size_t size,
				 off_t addr);

/**
 * Take everything on the current memory chain, and place it on the
 * freelist.
 */
RT_EXPORT extern void rt_mempurge(struct mem_map **pp);

/**
 * Print a memory chain.
 */
RT_EXPORT extern void rt_memprint(struct mem_map **pp);

/**
 * Return all the storage used by the rt_mem_freemap.
 */
RT_EXPORT extern void rt_memclose(void);


RT_EXPORT extern struct bn_vlblock *rt_vlblock_init(void);


RT_EXPORT extern void rt_vlblock_free(struct bn_vlblock *vbp);

RT_EXPORT extern struct bu_list *rt_vlblock_find(struct bn_vlblock *vbp,
						 int r,
						 int g,
						 int b);

/* ars.c */
RT_EXPORT extern void rt_hitsort(struct hit h[],
				 int nh);

/* pg.c */
RT_EXPORT extern int rt_pg_to_bot(struct rt_db_internal *ip,
				  const struct bn_tol *tol,
				  struct resource *resp0);
RT_EXPORT extern int rt_pg_plot(struct bu_list		*vhead,
				struct rt_db_internal	*ip,
				const struct rt_tess_tol *ttol,
				const struct bn_tol	*tol,
				const struct rt_view_info *info);
RT_EXPORT extern int rt_pg_plot_poly(struct bu_list		*vhead,
				     struct rt_db_internal	*ip,
				     const struct rt_tess_tol	*ttol,
				     const struct bn_tol	*tol);

/* hf.c */
RT_EXPORT extern int rt_hf_to_dsp(struct rt_db_internal *db_intern);

/* dsp.c */
RT_EXPORT extern int dsp_pos(point_t out,
			     struct soltab *stp,
			     point_t p);

/* pr.c */
RT_EXPORT extern void rt_pr_soltab(const struct soltab *stp);
RT_EXPORT extern void rt_pr_region(const struct region *rp);
RT_EXPORT extern void rt_pr_partitions(const struct rt_i *rtip,
				       const struct partition	*phead,
				       const char *title);
RT_EXPORT extern void rt_pr_pt_vls(struct bu_vls *v,
				   const struct rt_i *rtip,
				   const struct partition *pp);
RT_EXPORT extern void rt_pr_pt(const struct rt_i *rtip,
			       const struct partition *pp);
RT_EXPORT extern void rt_pr_seg_vls(struct bu_vls *,
				    const struct seg *);
RT_EXPORT extern void rt_pr_seg(const struct seg *segp);
RT_EXPORT extern void rt_pr_hit(const char *str,
				const struct hit	*hitp);
RT_EXPORT extern void rt_pr_hit_vls(struct bu_vls *v,
				    const char *str,
				    const struct hit *hitp);
RT_EXPORT extern void rt_pr_hitarray_vls(struct bu_vls *v,
					 const char *str,
					 const struct hit *hitp,
					 int count);
RT_EXPORT extern void rt_pr_tree(const union tree *tp,
				 int lvl);
RT_EXPORT extern void rt_pr_tree_vls(struct bu_vls *vls,
				     const union tree *tp);
RT_EXPORT extern char *rt_pr_tree_str(const union tree *tree);
RT_EXPORT extern void rt_pr_tree_val(const union tree *tp,
				     const struct partition *partp,
				     int pr_name,
				     int lvl);
RT_EXPORT extern void rt_pr_fallback_angle(struct bu_vls *str,
					   const char *prefix,
					   const double angles[5]);
RT_EXPORT extern void rt_find_fallback_angle(double angles[5],
					     const vect_t vec);
RT_EXPORT extern void rt_pr_tol(const struct bn_tol *tol);

/* regionfix.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/regionfix.c
 *
 * Subroutines for adjusting old GIFT-style region-IDs, to take into
 * account the presence of instancing.
 *
 */

/**
 * Apply any deltas to reg_regionid values to allow old applications
 * that use the reg_regionid number to distinguish between different
 * instances of the same prototype region.
 *
 * Called once, from rt_prep(), before raytracing begins.
 */
RT_EXPORT extern void rt_regionfix(struct rt_i *rtip);

/* table.c */
RT_EXPORT extern int rt_id_solid(struct bu_external *ep);
RT_EXPORT extern const struct rt_functab *rt_get_functab_by_label(const char *label);
RT_EXPORT extern int rt_generic_xform(struct rt_db_internal	*op,
				      const mat_t		mat,
				      struct rt_db_internal	*ip,
				      int			avail,
				      struct db_i		*dbip,
				      struct resource		*resp);


/* prep.c */
RT_EXPORT extern void rt_plot_all_bboxes(FILE *fp,
					 struct rt_i *rtip);
RT_EXPORT extern void rt_plot_all_solids(FILE		*fp,
					 struct rt_i	*rtip,
					 struct resource	*resp);
RT_EXPORT extern void rt_init_resource(struct resource *resp,
				       int		cpu_num,
				       struct rt_i	*rtip);
RT_EXPORT extern void rt_clean_resource(struct rt_i *rtip,
					struct resource *resp);
RT_EXPORT extern void rt_clean_resource_complete(struct rt_i *rtip,
						 struct resource *resp);
RT_EXPORT extern int rt_unprep(struct rt_i *rtip,
			       struct rt_reprep_obj_list *objs,
			       struct resource *resp);
RT_EXPORT extern int rt_reprep(struct rt_i *rtip,
			       struct rt_reprep_obj_list *objs,
			       struct resource *resp);
RT_EXPORT extern int re_prep_solids(struct rt_i *rtip,
				    int num_solids,
				    char **solid_names,
				    struct resource *resp);
RT_EXPORT extern int rt_find_paths(struct db_i *dbip,
				   struct directory *start,
				   struct directory *end,
				   struct bu_ptbl *paths,
				   struct resource *resp);

RT_EXPORT extern struct bu_bitv *rt_get_solidbitv(size_t nbits,
						  struct resource *resp);
/** @} */

/* shoot.c */
/** @addtogroup ray */
/** @{ */
/** @file librt/shoot.c
 *
 * Ray Tracing program shot coordinator.
 *
 * This is the heart of LIBRT's ray-tracing capability.
 *
 * Given a ray, shoot it at all the relevant parts of the model,
 * (building the finished_segs chain), and then call rt_boolregions()
 * to build and evaluate the partition chain.  If the ray actually hit
 * anything, call the application's a_hit() routine with a pointer to
 * the partition chain, otherwise, call the application's a_miss()
 * routine.
 *
 * It is important to note that rays extend infinitely only in the
 * positive direction.  The ray is composed of all points P, where
 *
 * P = r_pt + K * r_dir
 *
 * for K ranging from 0 to +infinity.  There is no looking backwards.
 *
 */


/**
 * To be called only in non-parallel mode, to tally up the statistics
 * from the resource structure(s) into the rt instance structure.
 *
 * Non-parallel programs should call
 * rt_add_res_stats(rtip, RESOURCE_NULL);
 * to have the default resource results tallied in.
 */
RT_EXPORT extern void rt_add_res_stats(struct rt_i *rtip,
				       struct resource *resp);
/* Tally stats into struct rt_i */
RT_EXPORT extern void rt_zero_res_stats(struct resource *resp);


RT_EXPORT extern void rt_res_pieces_clean(struct resource *resp,
					  struct rt_i *rtip);


/**
 * Allocate the per-processor state variables needed to support
 * rt_shootray()'s use of 'solid pieces'.
 */
RT_EXPORT extern void rt_res_pieces_init(struct resource *resp,
					 struct rt_i *rtip);
RT_EXPORT extern void rt_vstub(struct soltab *stp[],
			       struct xray *rp[],
			       struct  seg segp[],
			       int n,
			       struct application	*ap);

/** @} */


/* tree.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/tree.c
 *
 * Ray Tracing library database tree walker.
 *
 * Collect and prepare regions and solids for subsequent ray-tracing.
 *
 */


/**
 * Calculate the bounding RPP of the region whose boolean tree is
 * 'tp'.  The bounding RPP is returned in tree_min and tree_max, which
 * need not have been initialized first.
 *
 * Returns -
 * 0 success
 * -1 failure (tree_min and tree_max may have been altered)
 */
RT_EXPORT extern int rt_bound_tree(const union tree	*tp,
				   vect_t		tree_min,
				   vect_t		tree_max);

/**
 * Eliminate any references to NOP nodes from the tree.  It is safe to
 * use db_free_tree() here, because there will not be any dead solids.
 * They will all have been converted to OP_NOP nodes by
 * _rt_tree_kill_dead_solid_refs(), previously, so there is no need to
 * worry about multiple db_free_tree()'s repeatedly trying to free one
 * solid that has been instanced multiple times.
 *
 * Returns -
 * 0 this node is OK.
 * -1 request caller to kill this node
 */
RT_EXPORT extern int rt_tree_elim_nops(union tree *,
				       struct resource *resp);

/** @} */


/* vlist.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/vlist.c
 *
 * Routines for the import and export of vlist chains as:
 * Network independent binary,
 * BRL-extended UNIX-Plot files.
 *
 */

/* FIXME: Has some stuff mixed in here that should go in LIBBN */
/************************************************************************
 *									*
 *			Generic VLBLOCK routines			*
 *									*
 ************************************************************************/

RT_EXPORT extern struct bn_vlblock *bn_vlblock_init(struct bu_list	*free_vlist_hd,	/* where to get/put free vlists */
						    int		max_ent);

RT_EXPORT extern struct bn_vlblock *	rt_vlblock_init(void);


RT_EXPORT extern void rt_vlblock_free(struct bn_vlblock *vbp);

RT_EXPORT extern struct bu_list *rt_vlblock_find(struct bn_vlblock *vbp,
						 int r,
						 int g,
						 int b);


/************************************************************************
 *									*
 *			Generic BN_VLIST routines			*
 *									*
 ************************************************************************/

/**
 * Validate an bn_vlist chain for having reasonable values inside.
 * Calls bu_bomb() if not.
 *
 * Returns -
 * npts Number of point/command sets in use.
 */
RT_EXPORT extern int rt_ck_vlist(const struct bu_list *vhead);


/**
 * Duplicate the contents of a vlist.  Note that the copy may be more
 * densely packed than the source.
 */
RT_EXPORT extern void rt_vlist_copy(struct bu_list *dest,
				    const struct bu_list *src);


/**
 * The macro RT_FREE_VLIST() simply appends to the list
 * &RTG.rtg_vlfree.  Now, give those structures back to bu_free().
 */
RT_EXPORT extern void bn_vlist_cleanup(struct bu_list *hd);


/**
 * XXX This needs to remain a LIBRT function.
 */
RT_EXPORT extern void rt_vlist_cleanup(void);


/**
 * Given an RPP, draw the outline of it into the vlist.
 */
RT_EXPORT extern void bn_vlist_rpp(struct bu_list *hd,
				   const point_t minn,
				   const point_t maxx);


/************************************************************************
 *									*
 *			Binary VLIST import/export routines		*
 *									*
 ************************************************************************/

/**
 * Convert a vlist chain into a blob of network-independent binary,
 * for transmission to another machine.
 *
 * The result is stored in a vls string, so that both the address and
 * length are available conveniently.
 */
RT_EXPORT extern void rt_vlist_export(struct bu_vls *vls,
				      struct bu_list *hp,
				      const char *name);


/**
 * Convert a blob of network-independent binary prepared by
 * vls_vlist() and received from another machine, into a vlist chain.
 */
RT_EXPORT extern void rt_vlist_import(struct bu_list *hp,
				      struct bu_vls *namevls,
				      const unsigned char *buf);


/************************************************************************
 *									*
 *			UNIX-Plot VLIST import/export routines		*
 *									*
 ************************************************************************/

/**
 * Output a bn_vlblock object in extended UNIX-plot format, including
 * color.
 */
RT_EXPORT extern void rt_plot_vlblock(FILE *fp,
				      const struct bn_vlblock *vbp);


/**
 * Output a vlist as an extended 3-D floating point UNIX-Plot file.
 * You provide the file.  Uses libplot3 routines to create the
 * UNIX-Plot output.
 */
RT_EXPORT extern void rt_vlist_to_uplot(FILE *fp,
					const struct bu_list *vhead);
RT_EXPORT extern int rt_process_uplot_value(struct bu_list **vhead,
					    struct bn_vlblock *vbp,
					    FILE *fp,
					    int c,
					    double char_size,
					    int mode);


/**
 * Read a BRL-style 3-D UNIX-plot file into a vector list.  For now,
 * discard color information, only extract vectors.  This might be
 * more naturally located in mged/plot.c
 */
RT_EXPORT extern int rt_uplot_to_vlist(struct bn_vlblock *vbp,
				       FILE *fp,
				       double char_size,
				       int mode);


/**
 * Used by MGED's "labelvert" command.
 */
RT_EXPORT extern void rt_label_vlist_verts(struct bn_vlblock *vbp,
					   struct bu_list *src,
					   mat_t mat,
					   double sz,
					   double mm2local);
/** @} */
/* sketch.c */
RT_EXPORT extern int curve_to_vlist(struct bu_list		*vhead,
				    const struct rt_tess_tol	*ttol,
				    point_t			V,
				    vect_t			u_vec,
				    vect_t			v_vec,
				    struct rt_sketch_internal *sketch_ip,
				    struct rt_curve		*crv);

RT_EXPORT extern int rt_check_curve(const struct rt_curve *crv,
				    const struct rt_sketch_internal *skt,
				    int noisy);

RT_EXPORT extern void rt_curve_reverse_segment(uint32_t *lng);
RT_EXPORT extern void rt_curve_order_segments(struct rt_curve *crv);

RT_EXPORT extern void rt_copy_curve(struct rt_curve *crv_out,
				    const struct rt_curve *crv_in);

RT_EXPORT extern void rt_curve_free(struct rt_curve *crv);
RT_EXPORT extern void rt_copy_curve(struct rt_curve *crv_out,
				    const struct rt_curve *crv_in);
RT_EXPORT extern struct rt_sketch_internal *rt_copy_sketch(const struct rt_sketch_internal *sketch_ip);
RT_EXPORT extern int curve_to_tcl_list(struct bu_vls *vls,
				       struct rt_curve *crv);

/* htbl.c */

RT_EXPORT extern void rt_htbl_init(struct rt_htbl *b, size_t len, const char *str);

/**
 * Reset the table to have no elements, but retain any existing storage.
 */
RT_EXPORT extern void rt_htbl_reset(struct rt_htbl *b);

/**
 * Deallocate dynamic hit buffer and render unusable without a
 * subsequent rt_htbl_init().
 */
RT_EXPORT extern void rt_htbl_free(struct rt_htbl *b);

/**
 * Allocate another hit structure, extending the array if necessary.
 */
RT_EXPORT extern struct hit *rt_htbl_get(struct rt_htbl *b);

/************************************************************************
 *									*
 *			NMG Support Function Declarations		*
 *									*
 ************************************************************************/
#if defined(NMG_H)

/* From file nmg_mk.c */
/*	MAKE routines */
RT_EXPORT extern struct model *nmg_mm(void);
RT_EXPORT extern struct model *nmg_mmr(void);
RT_EXPORT extern struct nmgregion *nmg_mrsv(struct model *m);
RT_EXPORT extern struct shell *nmg_msv(struct nmgregion *r_p);
RT_EXPORT extern struct faceuse *nmg_mf(struct loopuse *lu1);
RT_EXPORT extern struct loopuse *nmg_mlv(uint32_t *magic,
					 struct vertex *v,
					 int orientation);
RT_EXPORT extern struct edgeuse *nmg_me(struct vertex *v1,
					struct vertex *v2,
					struct shell *s);
RT_EXPORT extern struct edgeuse *nmg_meonvu(struct vertexuse *vu);
RT_EXPORT extern struct loopuse *nmg_ml(struct shell *s);
/*	KILL routines */
RT_EXPORT extern int nmg_keg(struct edgeuse *eu);
RT_EXPORT extern int nmg_kvu(struct vertexuse *vu);
RT_EXPORT extern int nmg_kfu(struct faceuse *fu1);
RT_EXPORT extern int nmg_klu(struct loopuse *lu1);
RT_EXPORT extern int nmg_keu(struct edgeuse *eu);
RT_EXPORT extern int nmg_keu_zl(struct shell *s,
				const struct bn_tol *tol);
RT_EXPORT extern int nmg_ks(struct shell *s);
RT_EXPORT extern int nmg_kr(struct nmgregion *r);
RT_EXPORT extern void nmg_km(struct model *m);
/*	Geometry and Attribute routines */
RT_EXPORT extern void nmg_vertex_gv(struct vertex *v,
				    const point_t pt);
RT_EXPORT extern void nmg_vertex_g(struct vertex *v,
				   fastf_t x,
				   fastf_t y,
				   fastf_t z);
RT_EXPORT extern void nmg_vertexuse_nv(struct vertexuse *vu,
				       const vect_t norm);
RT_EXPORT extern void nmg_vertexuse_a_cnurb(struct vertexuse *vu,
					    const fastf_t *uvw);
RT_EXPORT extern void nmg_edge_g(struct edgeuse *eu);
RT_EXPORT extern void nmg_edge_g_cnurb(struct edgeuse *eu,
				       int order,
				       int n_knots,
				       fastf_t *kv,
				       int n_pts,
				       int pt_type,
				       fastf_t *points);
RT_EXPORT extern void nmg_edge_g_cnurb_plinear(struct edgeuse *eu);
RT_EXPORT extern int nmg_use_edge_g(struct edgeuse *eu,
				    uint32_t *eg);
RT_EXPORT extern void nmg_loop_g(struct loop *l,
				 const struct bn_tol *tol);
RT_EXPORT extern void nmg_face_g(struct faceuse *fu,
				 const plane_t p);
RT_EXPORT extern void nmg_face_new_g(struct faceuse *fu,
				     const plane_t pl);
RT_EXPORT extern void nmg_face_g_snurb(struct faceuse *fu,
				       int u_order,
				       int v_order,
				       int n_u_knots,
				       int n_v_knots,
				       fastf_t *ukv,
				       fastf_t *vkv,
				       int n_rows,
				       int n_cols,
				       int pt_type,
				       fastf_t *mesh);
RT_EXPORT extern void nmg_face_bb(struct face *f,
				  const struct bn_tol *tol);
RT_EXPORT extern void nmg_shell_a(struct shell *s,
				  const struct bn_tol *tol);
RT_EXPORT extern void nmg_region_a(struct nmgregion *r,
				   const struct bn_tol *tol);
/*	DEMOTE routines */
RT_EXPORT extern int nmg_demote_lu(struct loopuse *lu);
RT_EXPORT extern int nmg_demote_eu(struct edgeuse *eu);
/*	MODIFY routines */
RT_EXPORT extern void nmg_movevu(struct vertexuse *vu,
				 struct vertex *v);
RT_EXPORT extern void nmg_je(struct edgeuse *eudst,
			     struct edgeuse *eusrc);
RT_EXPORT extern void nmg_unglueedge(struct edgeuse *eu);
RT_EXPORT extern void nmg_jv(struct vertex *v1,
			     struct vertex *v2);
RT_EXPORT extern void nmg_jfg(struct face *f1,
			      struct face *f2);
RT_EXPORT extern void nmg_jeg(struct edge_g_lseg *dest_eg,
			      struct edge_g_lseg *src_eg);

/* From nmg_mod.c */
/*	REGION Routines */
RT_EXPORT extern void nmg_merge_regions(struct nmgregion *r1,
					struct nmgregion *r2,
					const struct bn_tol *tol);

/*	SHELL Routines */
RT_EXPORT extern void nmg_shell_coplanar_face_merge(struct shell *s,
						    const struct bn_tol *tol,
						    const int simplify);
RT_EXPORT extern int nmg_simplify_shell(struct shell *s);
RT_EXPORT extern void nmg_rm_redundancies(struct shell *s,
					  const struct bn_tol *tol);
RT_EXPORT extern void nmg_sanitize_s_lv(struct shell *s,
					int orient);
RT_EXPORT extern void nmg_s_split_touchingloops(struct shell *s,
						const struct bn_tol *tol);
RT_EXPORT extern void nmg_s_join_touchingloops(struct shell		*s,
					       const struct bn_tol	*tol);
RT_EXPORT extern void nmg_js(struct shell	*s1,
			     struct shell	*s2,
			     const struct bn_tol	*tol);
RT_EXPORT extern void nmg_invert_shell(struct shell		*s);

/*	FACE Routines */
RT_EXPORT extern struct faceuse *nmg_cmface(struct shell *s,
					    struct vertex **vt[],
					    int n);
RT_EXPORT extern struct faceuse *nmg_cface(struct shell *s,
					   struct vertex **vt,
					   int n);
RT_EXPORT extern struct faceuse *nmg_add_loop_to_face(struct shell *s,
						      struct faceuse *fu,
						      struct vertex **verts,
						      int n,
						      int dir);
RT_EXPORT extern int nmg_fu_planeeqn(struct faceuse *fu,
				     const struct bn_tol *tol);
RT_EXPORT extern void nmg_gluefaces(struct faceuse *fulist[],
				    int n,
				    const struct bn_tol *tol);
RT_EXPORT extern int nmg_simplify_face(struct faceuse *fu);
RT_EXPORT extern void nmg_reverse_face(struct faceuse *fu);
RT_EXPORT extern void nmg_mv_fu_between_shells(struct shell *dest,
					       struct shell *src,
					       struct faceuse *fu);
RT_EXPORT extern void nmg_jf(struct faceuse *dest_fu,
			     struct faceuse *src_fu);
RT_EXPORT extern struct faceuse *nmg_dup_face(struct faceuse *fu,
					      struct shell *s);
/*	LOOP Routines */
RT_EXPORT extern void nmg_jl(struct loopuse *lu,
			     struct edgeuse *eu);
RT_EXPORT extern struct vertexuse *nmg_join_2loops(struct vertexuse *vu1,
						   struct vertexuse *vu2);
RT_EXPORT extern struct vertexuse *nmg_join_singvu_loop(struct vertexuse *vu1,
							struct vertexuse *vu2);
RT_EXPORT extern struct vertexuse *nmg_join_2singvu_loops(struct vertexuse *vu1,
							  struct vertexuse *vu2);
RT_EXPORT extern struct loopuse *nmg_cut_loop(struct vertexuse *vu1,
					      struct vertexuse *vu2);
RT_EXPORT extern struct loopuse *nmg_split_lu_at_vu(struct loopuse *lu,
						    struct vertexuse *vu);
RT_EXPORT extern struct vertexuse *nmg_find_repeated_v_in_lu(struct vertexuse *vu);
RT_EXPORT extern void nmg_split_touchingloops(struct loopuse *lu,
					      const struct bn_tol *tol);
RT_EXPORT extern int nmg_join_touchingloops(struct loopuse *lu);
RT_EXPORT extern int nmg_get_touching_jaunts(const struct loopuse *lu,
					     struct bu_ptbl *tbl,
					     int *need_init);
RT_EXPORT extern void nmg_kill_accordions(struct loopuse *lu);
RT_EXPORT extern int nmg_loop_split_at_touching_jaunt(struct loopuse		*lu,
						      const struct bn_tol	*tol);
RT_EXPORT extern void nmg_simplify_loop(struct loopuse *lu);
RT_EXPORT extern int nmg_kill_snakes(struct loopuse *lu);
RT_EXPORT extern void nmg_mv_lu_between_shells(struct shell *dest,
					       struct shell *src,
					       struct loopuse *lu);
RT_EXPORT extern void nmg_moveltof(struct faceuse *fu,
				   struct shell *s);
RT_EXPORT extern struct loopuse *nmg_dup_loop(struct loopuse *lu,
					      uint32_t *parent,
					      long **trans_tbl);
RT_EXPORT extern void nmg_set_lu_orientation(struct loopuse *lu,
					     int is_opposite);
RT_EXPORT extern void nmg_lu_reorient(struct loopuse *lu);
/*	EDGE Routines */
RT_EXPORT extern struct edgeuse *nmg_eusplit(struct vertex *v,
					     struct edgeuse *oldeu,
					     int share_geom);
RT_EXPORT extern struct edgeuse *nmg_esplit(struct vertex *v,
					    struct edgeuse *eu,
					    int share_geom);
RT_EXPORT extern struct edgeuse *nmg_ebreak(struct vertex *v,
					    struct edgeuse *eu);
RT_EXPORT extern struct edgeuse *nmg_ebreaker(struct vertex *v,
					      struct edgeuse *eu,
					      const struct bn_tol *tol);
RT_EXPORT extern struct vertex *nmg_e2break(struct edgeuse *eu1,
					    struct edgeuse *eu2);
RT_EXPORT extern int nmg_unbreak_edge(struct edgeuse *eu1_first);
RT_EXPORT extern int nmg_unbreak_shell_edge_unsafe(struct edgeuse *eu1_first);
RT_EXPORT extern struct edgeuse *nmg_eins(struct edgeuse *eu);
RT_EXPORT extern void nmg_mv_eu_between_shells(struct shell *dest,
					       struct shell *src,
					       struct edgeuse *eu);
/*	VERTEX Routines */
RT_EXPORT extern void nmg_mv_vu_between_shells(struct shell *dest,
					       struct shell *src,
					       struct vertexuse *vu);

/* From nmg_info.c */
/* Model routines */
RT_EXPORT extern struct model *nmg_find_model(const uint32_t *magic_p);
RT_EXPORT extern struct shell *nmg_find_shell(const uint32_t *magic_p);
RT_EXPORT extern void nmg_model_bb(point_t min_pt,
				   point_t max_pt,
				   const struct model *m);


/* Shell routines */
RT_EXPORT extern int nmg_shell_is_empty(const struct shell *s);
RT_EXPORT extern struct shell *nmg_find_s_of_lu(const struct loopuse *lu);
RT_EXPORT extern struct shell *nmg_find_s_of_eu(const struct edgeuse *eu);
RT_EXPORT extern struct shell *nmg_find_s_of_vu(const struct vertexuse *vu);

/* Face routines */
RT_EXPORT extern struct faceuse *nmg_find_fu_of_eu(const struct edgeuse *eu);
RT_EXPORT extern struct faceuse *nmg_find_fu_of_lu(const struct loopuse *lu);
RT_EXPORT extern struct faceuse *nmg_find_fu_of_vu(const struct vertexuse *vu);
RT_EXPORT extern struct faceuse *nmg_find_fu_with_fg_in_s(const struct shell *s1,
							  const struct faceuse *fu2);
RT_EXPORT extern double nmg_measure_fu_angle(const struct edgeuse *eu,
					     const vect_t xvec,
					     const vect_t yvec,
					     const vect_t zvec);

/* Loop routines */
RT_EXPORT extern struct loopuse*nmg_find_lu_of_vu(const struct vertexuse *vu);
RT_EXPORT extern int nmg_loop_is_a_crack(const struct loopuse *lu);
RT_EXPORT extern int	nmg_loop_is_ccw(const struct loopuse *lu,
					const plane_t norm,
					const struct bn_tol *tol);
RT_EXPORT extern const struct vertexuse *nmg_loop_touches_self(const struct loopuse *lu);
RT_EXPORT extern int nmg_2lu_identical(const struct edgeuse *eu1,
				       const struct edgeuse *eu2);

/* Edge routines */
RT_EXPORT extern struct edgeuse *nmg_find_matching_eu_in_s(const struct edgeuse	*eu1,
							   const struct shell	*s2);
RT_EXPORT extern struct edgeuse	*nmg_findeu(const struct vertex *v1,
					    const struct vertex *v2,
					    const struct shell *s,
					    const struct edgeuse *eup,
					    int dangling_only);
RT_EXPORT extern struct edgeuse *nmg_find_eu_in_face(const struct vertex *v1,
						     const struct vertex *v2,
						     const struct faceuse *fu,
						     const struct edgeuse *eup,
						     int dangling_only);
RT_EXPORT extern struct edgeuse *nmg_find_e(const struct vertex *v1,
					    const struct vertex *v2,
					    const struct shell *s,
					    const struct edge *ep);
RT_EXPORT extern struct edgeuse *nmg_find_eu_of_vu(const struct vertexuse *vu);
RT_EXPORT extern struct edgeuse *nmg_find_eu_with_vu_in_lu(const struct loopuse *lu,
							   const struct vertexuse *vu);
RT_EXPORT extern const struct edgeuse *nmg_faceradial(const struct edgeuse *eu);
RT_EXPORT extern const struct edgeuse *nmg_radial_face_edge_in_shell(const struct edgeuse *eu);
RT_EXPORT extern const struct edgeuse *nmg_find_edge_between_2fu(const struct faceuse *fu1,
								 const struct faceuse *fu2,
								 const struct bn_tol *tol);
RT_EXPORT extern struct edge *nmg_find_e_nearest_pt2(uint32_t *magic_p,
						     const point_t pt2,
						     const mat_t mat,
						     const struct bn_tol *tol);
RT_EXPORT extern struct edgeuse *nmg_find_matching_eu_in_s(const struct edgeuse *eu1,
							   const struct shell *s2);
RT_EXPORT extern void nmg_eu_2vecs_perp(vect_t xvec,
					vect_t yvec,
					vect_t zvec,
					const struct edgeuse *eu,
					const struct bn_tol *tol);
RT_EXPORT extern int nmg_find_eu_leftvec(vect_t left,
					 const struct edgeuse *eu);
RT_EXPORT extern int nmg_find_eu_left_non_unit(vect_t left,
					       const struct edgeuse	*eu);
RT_EXPORT extern struct edgeuse *nmg_find_ot_same_eu_of_e(const struct edge *e);

/* Vertex routines */
RT_EXPORT extern struct vertexuse *nmg_find_v_in_face(const struct vertex *,
						      const struct faceuse *);
RT_EXPORT extern struct vertexuse *nmg_find_v_in_shell(const struct vertex *v,
						       const struct shell *s,
						       int edges_only);
RT_EXPORT extern struct vertexuse *nmg_find_pt_in_lu(const struct loopuse *lu,
						     const point_t pt,
						     const struct bn_tol *tol);
RT_EXPORT extern struct vertexuse *nmg_find_pt_in_face(const struct faceuse *fu,
						       const point_t pt,
						       const struct bn_tol *tol);
RT_EXPORT extern struct vertex *nmg_find_pt_in_shell(const struct shell *s,
						     const point_t pt,
						     const struct bn_tol *tol);
RT_EXPORT extern struct vertex *nmg_find_pt_in_model(const struct model *m,
						     const point_t pt,
						     const struct bn_tol *tol);
RT_EXPORT extern int nmg_is_vertex_in_edgelist(const struct vertex *v,
					       const struct bu_list *hd);
RT_EXPORT extern int nmg_is_vertex_in_looplist(const struct vertex *v,
					       const struct bu_list *hd,
					       int singletons);
RT_EXPORT extern struct vertexuse *nmg_is_vertex_in_face(const struct vertex *v,
							 const struct face *f);
RT_EXPORT extern int nmg_is_vertex_a_selfloop_in_shell(const struct vertex *v,
						       const struct shell *s);
RT_EXPORT extern int nmg_is_vertex_in_facelist(const struct vertex *v,
					       const struct bu_list *hd);
RT_EXPORT extern int nmg_is_edge_in_edgelist(const struct edge *e,
					     const struct bu_list *hd);
RT_EXPORT extern int nmg_is_edge_in_looplist(const struct edge *e,
					     const struct bu_list *hd);
RT_EXPORT extern int nmg_is_edge_in_facelist(const struct edge *e,
					     const struct bu_list *hd);
RT_EXPORT extern int nmg_is_loop_in_facelist(const struct loop *l,
					     const struct bu_list *fu_hd);

/* Tabulation routines */
RT_EXPORT extern void nmg_vertex_tabulate(struct bu_ptbl *tab,
					  const uint32_t *magic_p);
RT_EXPORT extern void nmg_vertexuse_normal_tabulate(struct bu_ptbl *tab,
						    const uint32_t *magic_p);
RT_EXPORT extern void nmg_edgeuse_tabulate(struct bu_ptbl *tab,
					   const uint32_t *magic_p);
RT_EXPORT extern void nmg_edge_tabulate(struct bu_ptbl *tab,
					const uint32_t *magic_p);
RT_EXPORT extern void nmg_edge_g_tabulate(struct bu_ptbl *tab,
					  const uint32_t *magic_p);
RT_EXPORT extern void nmg_face_tabulate(struct bu_ptbl *tab,
					const uint32_t *magic_p);
RT_EXPORT extern void nmg_edgeuse_with_eg_tabulate(struct bu_ptbl *tab,
						   const struct edge_g_lseg *eg);
RT_EXPORT extern void nmg_edgeuse_on_line_tabulate(struct bu_ptbl *tab,
						   const uint32_t *magic_p,
						   const point_t pt,
						   const vect_t dir,
						   const struct bn_tol *tol);
RT_EXPORT extern void nmg_e_and_v_tabulate(struct bu_ptbl *eutab,
					   struct bu_ptbl *vtab,
					   const uint32_t *magic_p);
RT_EXPORT extern int nmg_2edgeuse_g_coincident(const struct edgeuse	*eu1,
					       const struct edgeuse	*eu2,
					       const struct bn_tol	*tol);

/* From nmg_extrude.c */
RT_EXPORT extern void nmg_translate_face(struct faceuse *fu, const vect_t Vec);
RT_EXPORT extern int nmg_extrude_face(struct faceuse *fu, const vect_t Vec, const struct bn_tol *tol);
RT_EXPORT extern struct vertexuse *nmg_find_vertex_in_lu(const struct vertex *v, const struct loopuse *lu);
RT_EXPORT extern void nmg_fix_overlapping_loops(struct shell *s, const struct bn_tol *tol);
RT_EXPORT extern void nmg_break_crossed_loops(struct shell *is, const struct bn_tol *tol);
RT_EXPORT extern struct shell *nmg_extrude_cleanup(struct shell *is, const int is_void, const struct bn_tol *tol);
RT_EXPORT extern void nmg_hollow_shell(struct shell *s, const fastf_t thick, const int approximate, const struct bn_tol *tol);
RT_EXPORT extern struct shell *nmg_extrude_shell(struct shell *s, const fastf_t dist, const int normal_ward, const int approximate, const struct bn_tol *tol);

/* From nmg_pr.c */
RT_EXPORT extern char *nmg_orientation(int orientation);
RT_EXPORT extern void nmg_pr_orient(int orientation,
				    const char *h);
RT_EXPORT extern void nmg_pr_m(const struct model *m);
RT_EXPORT extern void nmg_pr_r(const struct nmgregion *r,
			       char *h);
RT_EXPORT extern void nmg_pr_sa(const struct shell_a *sa,
				char *h);
RT_EXPORT extern void nmg_pr_lg(const struct loop_g *lg,
				char *h);
RT_EXPORT extern void nmg_pr_fg(const uint32_t *magic,
				char *h);
RT_EXPORT extern void nmg_pr_s(const struct shell *s,
			       char *h);
RT_EXPORT extern void  nmg_pr_s_briefly(const struct shell *s,
					char *h);
RT_EXPORT extern void nmg_pr_f(const struct face *f,
			       char *h);
RT_EXPORT extern void nmg_pr_fu(const struct faceuse *fu,
				char *h);
RT_EXPORT extern void nmg_pr_fu_briefly(const struct faceuse *fu,
					char *h);
RT_EXPORT extern void nmg_pr_l(const struct loop *l,
			       char *h);
RT_EXPORT extern void nmg_pr_lu(const struct loopuse *lu,
				char *h);
RT_EXPORT extern void nmg_pr_lu_briefly(const struct loopuse *lu,
					char *h);
RT_EXPORT extern void nmg_pr_eg(const uint32_t *eg,
				char *h);
RT_EXPORT extern void nmg_pr_e(const struct edge *e,
			       char *h);
RT_EXPORT extern void nmg_pr_eu(const struct edgeuse *eu,
				char *h);
RT_EXPORT extern void nmg_pr_eu_briefly(const struct edgeuse *eu,
					char *h);
RT_EXPORT extern void nmg_pr_eu_endpoints(const struct edgeuse *eu,
					  char *h);
RT_EXPORT extern void nmg_pr_vg(const struct vertex_g *vg,
				char *h);
RT_EXPORT extern void nmg_pr_v(const struct vertex *v,
			       char *h);
RT_EXPORT extern void nmg_pr_vu(const struct vertexuse *vu,
				char *h);
RT_EXPORT extern void nmg_pr_vu_briefly(const struct vertexuse *vu,
					char *h);
RT_EXPORT extern void nmg_pr_vua(const uint32_t *magic_p,
				 char *h);
RT_EXPORT extern void nmg_euprint(const char *str,
				  const struct edgeuse *eu);
RT_EXPORT extern void nmg_pr_ptbl(const char *title,
				  const struct bu_ptbl *tbl,
				  int verbose);
RT_EXPORT extern void nmg_pr_ptbl_vert_list(const char *str,
					    const struct bu_ptbl *tbl,
					    const fastf_t *mag);
RT_EXPORT extern void nmg_pr_one_eu_vecs(const struct edgeuse *eu,
					 const vect_t xvec,
					 const vect_t yvec,
					 const vect_t zvec,
					 const struct bn_tol *tol);
RT_EXPORT extern void nmg_pr_fu_around_eu_vecs(const struct edgeuse *eu,
					       const vect_t xvec,
					       const vect_t yvec,
					       const vect_t zvec,
					       const struct bn_tol *tol);
RT_EXPORT extern void nmg_pr_fu_around_eu(const struct edgeuse *eu,
					  const struct bn_tol *tol);
RT_EXPORT extern void nmg_pl_lu_around_eu(const struct edgeuse *eu);
RT_EXPORT extern void nmg_pr_fus_in_fg(const uint32_t *fg_magic);

/* From nmg_misc.c */
RT_EXPORT extern int nmg_snurb_calc_lu_uv_orient(const struct loopuse *lu);
RT_EXPORT extern void nmg_snurb_fu_eval(const struct faceuse *fu,
					const fastf_t u,
					const fastf_t v,
					point_t pt_on_srf);
RT_EXPORT extern void nmg_snurb_fu_get_norm(const struct faceuse *fu,
					    const fastf_t u,
					    const fastf_t v,
					    vect_t norm);
RT_EXPORT extern void nmg_snurb_fu_get_norm_at_vu(const struct faceuse *fu,
						  const struct vertexuse *vu,
						  vect_t norm);
RT_EXPORT extern void nmg_find_zero_length_edges(const struct model *m);
RT_EXPORT extern struct face *nmg_find_top_face_in_dir(const struct shell *s,
						       int dir, long *flags);
RT_EXPORT extern struct face *nmg_find_top_face(const struct shell *s,
						int *dir,
						long *flags);
RT_EXPORT extern int nmg_find_outer_and_void_shells(struct nmgregion *r,
						    struct bu_ptbl ***shells,
						    const struct bn_tol *tol);
RT_EXPORT extern int nmg_mark_edges_real(const uint32_t *magic_p);
RT_EXPORT extern void nmg_tabulate_face_g_verts(struct bu_ptbl *tab,
						const struct face_g_plane *fg);
RT_EXPORT extern void nmg_isect_shell_self(struct shell *s,
					   const struct bn_tol *tol);
RT_EXPORT extern struct edgeuse *nmg_next_radial_eu(const struct edgeuse *eu,
						    const struct shell *s,
						    const int wires);
RT_EXPORT extern struct edgeuse *nmg_prev_radial_eu(const struct edgeuse *eu,
						    const struct shell *s,
						    const int wires);
RT_EXPORT extern int nmg_radial_face_count(const struct edgeuse *eu,
					   const struct shell *s);
RT_EXPORT extern int nmg_check_closed_shell(const struct shell *s,
					    const struct bn_tol *tol);
RT_EXPORT extern int nmg_move_lu_between_fus(struct faceuse *dest,
					     struct faceuse *src,
					     struct loopuse *lu);
RT_EXPORT extern void nmg_loop_plane_newell(const struct loopuse *lu,
					    plane_t pl);
RT_EXPORT extern fastf_t nmg_loop_plane_area(const struct loopuse *lu,
					     plane_t pl);
RT_EXPORT extern fastf_t nmg_loop_plane_area2(const struct loopuse *lu,
					      plane_t pl,
					      const struct bn_tol *tol);
RT_EXPORT extern int nmg_calc_face_plane(struct faceuse *fu_in,
					 plane_t pl);
RT_EXPORT extern int nmg_calc_face_g(struct faceuse *fu);
RT_EXPORT extern fastf_t nmg_faceuse_area(const struct faceuse *fu);
RT_EXPORT extern fastf_t nmg_shell_area(const struct shell *s);
RT_EXPORT extern fastf_t nmg_region_area(const struct nmgregion *r);
RT_EXPORT extern fastf_t nmg_model_area(const struct model *m);
/* Some stray rt_ plane functions here */
RT_EXPORT extern void nmg_purge_unwanted_intersection_points(struct bu_ptbl *vert_list,
							     fastf_t *mag,
							     const struct faceuse *fu,
							     const struct bn_tol *tol);
RT_EXPORT extern int nmg_in_or_ref(struct vertexuse *vu,
				   struct bu_ptbl *b);
RT_EXPORT extern void nmg_rebound(struct model *m,
				  const struct bn_tol *tol);
RT_EXPORT extern void nmg_count_shell_kids(const struct model *m,
					   size_t *total_wires,
					   size_t *total_faces,
					   size_t *total_points);
RT_EXPORT extern void nmg_close_shell(struct shell *s,
				      const struct bn_tol *tol);
RT_EXPORT extern struct shell *nmg_dup_shell(struct shell *s,
					     long ***copy_tbl,
					     const struct bn_tol *tol);
RT_EXPORT extern struct edgeuse *nmg_pop_eu(struct bu_ptbl *stack);
RT_EXPORT extern void nmg_reverse_radials(struct faceuse *fu,
					  const struct bn_tol *tol);
RT_EXPORT extern void nmg_reverse_face_and_radials(struct faceuse *fu,
						   const struct bn_tol *tol);
RT_EXPORT extern int nmg_shell_is_void(const struct shell *s);
RT_EXPORT extern void nmg_propagate_normals(struct faceuse *fu_in,
					    long *flags,
					    const struct bn_tol *tol);
RT_EXPORT extern void nmg_connect_same_fu_orients(struct shell *s);
RT_EXPORT extern void nmg_fix_decomposed_shell_normals(struct shell *s,
						       const struct bn_tol *tol);
RT_EXPORT extern struct model *nmg_mk_model_from_region(struct nmgregion *r,
							int reindex);
RT_EXPORT extern void nmg_fix_normals(struct shell *s_orig,
				      const struct bn_tol *tol);
RT_EXPORT extern int nmg_break_long_edges(struct shell *s,
					  const struct bn_tol *tol);
RT_EXPORT extern struct faceuse *nmg_mk_new_face_from_loop(struct loopuse *lu);
RT_EXPORT extern int nmg_split_loops_into_faces(uint32_t *magic_p,
						const struct bn_tol *tol);
RT_EXPORT extern int nmg_decompose_shell(struct shell *s,
					 const struct bn_tol *tol);
RT_EXPORT extern void nmg_stash_model_to_file(const char *filename,
					      const struct model *m,
					      const char *title);
RT_EXPORT extern int nmg_unbreak_region_edges(uint32_t *magic_p);
RT_EXPORT extern void nmg_vlist_to_eu(struct bu_list *vlist,
				      struct shell *s);
RT_EXPORT extern int nmg_mv_shell_to_region(struct shell *s,
					    struct nmgregion *r);
RT_EXPORT extern int nmg_find_isect_faces(const struct vertex *new_v,
					  struct bu_ptbl *faces,
					  int *free_edges,
					  const struct bn_tol *tol);
RT_EXPORT extern int nmg_simple_vertex_solve(struct vertex *new_v,
					     const struct bu_ptbl *faces,
					     const struct bn_tol *tol);
RT_EXPORT extern int nmg_ck_vert_on_fus(const struct vertex *v,
					const struct bn_tol *tol);
RT_EXPORT extern void nmg_make_faces_at_vert(struct vertex *new_v,
					     struct bu_ptbl *int_faces,
					     const struct bn_tol *tol);
RT_EXPORT extern void nmg_kill_cracks_at_vertex(const struct vertex *vp);
RT_EXPORT extern int nmg_complex_vertex_solve(struct vertex *new_v,
					      const struct bu_ptbl *faces,
					      const int free_edges,
					      const int approximate,
					      const struct bn_tol *tol);
RT_EXPORT extern int nmg_bad_face_normals(const struct shell *s,
					  const struct bn_tol *tol);
RT_EXPORT extern int nmg_faces_are_radial(const struct faceuse *fu1,
					  const struct faceuse *fu2);
RT_EXPORT extern int nmg_move_edge_thru_pt(struct edgeuse *mv_eu,
					   const point_t pt,
					   const struct bn_tol *tol);
RT_EXPORT extern void nmg_vlist_to_wire_edges(struct shell *s,
					      const struct bu_list *vhead);
RT_EXPORT extern void nmg_follow_free_edges_to_vertex(const struct vertex *vpa,
						      const struct vertex *vpb,
						      struct bu_ptbl *bad_verts,
						      const struct shell *s,
						      const struct edgeuse *eu,
						      struct bu_ptbl *verts,
						      int *found);
RT_EXPORT extern void nmg_glue_face_in_shell(const struct faceuse *fu,
					     struct shell *s,
					     const struct bn_tol *tol);
RT_EXPORT extern int nmg_open_shells_connect(struct shell *dst,
					     struct shell *src,
					     const long **copy_tbl,
					     const struct bn_tol *tol);
RT_EXPORT extern int nmg_in_vert(struct vertex *new_v,
				 const int approximate,
				 const struct bn_tol *tol);
RT_EXPORT extern void nmg_mirror_model(struct model *m);
RT_EXPORT extern int nmg_kill_cracks(struct shell *s);
RT_EXPORT extern int nmg_kill_zero_length_edgeuses(struct model *m);
RT_EXPORT extern void nmg_make_faces_within_tol(struct shell *s,
						const struct bn_tol *tol);
RT_EXPORT extern void nmg_intersect_loops_self(struct shell *s,
					       const struct bn_tol *tol);
RT_EXPORT extern struct edge_g_cnurb *rt_join_cnurbs(struct bu_list *crv_head);
RT_EXPORT extern struct edge_g_cnurb *rt_arc2d_to_cnurb(point_t i_center,
							point_t i_start,
							point_t i_end,
							int point_type,
							const struct bn_tol *tol);
RT_EXPORT extern int nmg_break_edge_at_verts(struct edge *e,
					     struct bu_ptbl *verts,
					     const struct bn_tol *tol);
RT_EXPORT extern void nmg_isect_shell_self(struct shell *s,
					   const struct bn_tol *tol);
RT_EXPORT extern fastf_t nmg_loop_plane_area(const struct loopuse *lu,
					     plane_t pl);
RT_EXPORT extern int nmg_break_edges(uint32_t *magic_p,
				     const struct bn_tol *tol);
RT_EXPORT extern int nmg_lu_is_convex(struct loopuse *lu,
				      const struct bn_tol *tol);
RT_EXPORT extern int nmg_to_arb(const struct model *m,
				struct rt_arb_internal *arb_int);
RT_EXPORT extern int nmg_to_tgc(const struct model *m,
				struct rt_tgc_internal *tgc_int,
				const struct bn_tol *tol);
RT_EXPORT extern int nmg_to_poly(const struct model *m,
				 struct rt_pg_internal *poly_int,
				 const struct bn_tol *tol);
RT_EXPORT extern struct rt_bot_internal *nmg_bot(struct shell *s,
						 const struct bn_tol *tol);

RT_EXPORT extern int nmg_simplify_shell_edges(struct shell *s,
					      const struct bn_tol *tol);
RT_EXPORT extern int nmg_edge_collapse(struct model *m,
				       const struct bn_tol *tol,
				       const fastf_t tol_coll,
				       const fastf_t min_angle);

/* From nmg_copy.c */
RT_EXPORT extern struct model *nmg_clone_model(const struct model *original);

/* bot.c */
RT_EXPORT extern size_t rt_bot_get_edge_list(const struct rt_bot_internal *bot,
					     size_t **edge_list);
RT_EXPORT extern int rt_bot_edge_in_list(const size_t v1,
					 const size_t v2,
					 const size_t edge_list[],
					 const size_t edge_count0);
RT_EXPORT extern int rt_bot_plot(struct bu_list		*vhead,
				 struct rt_db_internal	*ip,
				 const struct rt_tess_tol *ttol,
				 const struct bn_tol	*tol,
				 const struct rt_view_info *info);
RT_EXPORT extern int rt_bot_plot_poly(struct bu_list		*vhead,
				      struct rt_db_internal	*ip,
				      const struct rt_tess_tol *ttol,
				      const struct bn_tol	*tol);
RT_EXPORT extern int rt_bot_find_v_nearest_pt2(const struct rt_bot_internal *bot,
					       const point_t	pt2,
					       const mat_t	mat);
RT_EXPORT extern int rt_bot_find_e_nearest_pt2(int *vert1, int *vert2, const struct rt_bot_internal *bot, const point_t pt2, const mat_t mat);
RT_EXPORT extern fastf_t rt_bot_propget(struct rt_bot_internal *bot,
					const char *property);
RT_EXPORT extern int rt_bot_vertex_fuse(struct rt_bot_internal *bot,
					const struct bn_tol *tol);
RT_EXPORT extern int rt_bot_face_fuse(struct rt_bot_internal *bot);
RT_EXPORT extern int rt_bot_condense(struct rt_bot_internal *bot);
RT_EXPORT extern int rt_bot_smooth(struct rt_bot_internal *bot,
				   const char *bot_name,
				   struct db_i *dbip,
				   fastf_t normal_tolerance_angle);
RT_EXPORT extern int rt_bot_flip(struct rt_bot_internal *bot);
RT_EXPORT extern int rt_bot_sync(struct rt_bot_internal *bot);
RT_EXPORT extern struct rt_bot_list * rt_bot_split(struct rt_bot_internal *bot);
RT_EXPORT extern struct rt_bot_list * rt_bot_patches(struct rt_bot_internal *bot);
RT_EXPORT extern void rt_bot_list_free(struct rt_bot_list *headRblp,
				       int fbflag);

RT_EXPORT extern int rt_bot_same_orientation(const int *a,
					     const int *b);

RT_EXPORT extern int rt_bot_tess(struct nmgregion **r,
				 struct model *m,
				 struct rt_db_internal *ip,
				 const struct rt_tess_tol *ttol,
				 const struct bn_tol *tol);

/* BREP drawing routines */
RT_EXPORT extern int rt_brep_plot(struct bu_list		*vhead,
				 struct rt_db_internal		*ip,
				 const struct rt_tess_tol	*ttol,
				 const struct bn_tol		*tol,
				 const struct rt_view_info *info);
RT_EXPORT extern int rt_brep_plot_poly(struct bu_list		*vhead,
					  const struct db_full_path *pathp,
				      struct rt_db_internal	*ip,
				      const struct rt_tess_tol	*ttol,
				      const struct bn_tol	*tol,
				      const struct rt_view_info *info);

/* From nmg_tri.c */
RT_EXPORT extern void nmg_triangulate_shell(struct shell *s,
					    const struct bn_tol  *tol);


RT_EXPORT extern void nmg_triangulate_model(struct model *m,
					    const struct bn_tol *tol);
RT_EXPORT extern int nmg_triangulate_fu(struct faceuse *fu,
					const struct bn_tol *tol);
RT_EXPORT extern void nmg_dump_model(struct model *m);

/*  nmg_tri_mc.c */
RT_EXPORT extern void nmg_triangulate_model_mc(struct model *m,
					       const struct bn_tol *tol);
RT_EXPORT extern int nmg_mc_realize_cube(struct shell *s,
					 int pv,
					 point_t *edges,
					 const struct bn_tol *tol);
RT_EXPORT extern int nmg_mc_evaluate(struct shell *s,
				     struct rt_i *rtip,
				     const struct db_full_path *pathp,
				     const struct rt_tess_tol *ttol,
				     const struct bn_tol *tol);

/* nmg_manif.c */
RT_EXPORT extern int nmg_dangling_face(const struct faceuse *fu,
				       const char *manifolds);
/* static paint_face */
/* static set_edge_sub_manifold */
/* static set_loop_sub_manifold */
/* static set_face_sub_manifold */
RT_EXPORT extern char *nmg_shell_manifolds(struct shell *sp,
					   char *tbl);
RT_EXPORT extern char *nmg_manifolds(struct model *m);

/* nmg.c */
RT_EXPORT extern int nmg_ray_segs(struct ray_data	*rd);

/* torus.c */
RT_EXPORT extern int rt_num_circular_segments(double maxerr,
					      double radius);

/* tcl.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/tcl.c
 *
 * Tcl interfaces to LIBRT routines.
 *
 * LIBRT routines are not for casual command-line use; as a result,
 * the Tcl name for the function should be exactly the same as the C
 * name for the underlying function.  Typing "rt_" is no hardship when
 * writing Tcl procs, and clarifies the origin of the routine.
 *
 */


/**
 * Allow specification of a ray to trace, in two convenient formats.
 *
 * Examples -
 * {0 0 0} dir {0 0 -1}
 * {20 -13.5 20} at {10 .5 3}
 */
RT_EXPORT extern int rt_tcl_parse_ray(Tcl_Interp *interp,
				      struct xray *rp,
				      const char *const*argv);


/**
 * Format a "union cutter" structure in a TCL-friendly format.  Useful
 * for debugging space partitioning
 *
 * Examples -
 * type cutnode
 * type boxnode
 * type nugridnode
 */
RT_EXPORT extern void rt_tcl_pr_cutter(Tcl_Interp *interp,
				       const union cutter *cutp);
RT_EXPORT extern int rt_tcl_cutter(ClientData clientData,
				   Tcl_Interp *interp,
				   int argc,
				   const char *const*argv);


/**
 * Format a hit in a TCL-friendly format.
 *
 * It is possible that a solid may have been removed from the
 * directory after this database was prepped, so check pointers
 * carefully.
 *
 * It might be beneficial to use some format other than %g to give the
 * user more precision.
 */
RT_EXPORT extern void rt_tcl_pr_hit(Tcl_Interp *interp, struct hit *hitp, const struct seg *segp, int flipflag);


/**
 * Generic interface for the LIBRT_class manipulation routines.
 *
 * Usage:
 * procname dbCmdName ?args?
 * Returns: result of cmdName LIBRT operation.
 *
 * Objects of type 'procname' must have been previously created by the
 * 'rt_gettrees' operation performed on a database object.
 *
 * Example -
 *	.inmem rt_gettrees .rt all.g
 *	.rt shootray {0 0 0} dir {0 0 -1}
 */
RT_EXPORT extern int rt_tcl_rt(ClientData clientData,
			       Tcl_Interp *interp,
			       int argc,
			       const char **argv);


/************************************************************************************************
 *												*
 *			Tcl interface to the Database						*
 *												*
 ************************************************************************************************/

/**
 * Given the name of a database object or a full path to a leaf
 * object, obtain the internal form of that leaf.  Packaged separately
 * mainly to make available nice Tcl error handling.
 *
 * Returns -
 * TCL_OK
 * TCL_ERROR
 */
RT_EXPORT extern int rt_tcl_import_from_path(Tcl_Interp *interp,
					     struct rt_db_internal *ip,
					     const char *path,
					     struct rt_wdb *wdb);
RT_EXPORT extern void rt_generic_make(const struct rt_functab *ftp, struct rt_db_internal *intern);


/**
 * Add all the supported Tcl interfaces to LIBRT routines to the list
 * of commands known by the given interpreter.
 *
 * wdb_open creates database "objects" which appear as Tcl procs,
 * which respond to many operations.
 *
 * The "db rt_gettrees" operation in turn creates a ray-traceable
 * object as yet another Tcl proc, which responds to additional
 * operations.
 *
 * Note that the MGED mainline C code automatically runs "wdb_open db"
 * and "wdb_open .inmem" on behalf of the MGED user, which exposes all
 * of this power.
 */
RT_EXPORT extern void rt_tcl_setup(Tcl_Interp *interp);
RT_EXPORT extern int Sysv_Init(Tcl_Interp *interp);


/**
 * Allows LIBRT to be dynamically loaded to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbu.so"
 * "load /usr/brlcad/lib/libbn.so"
 * "load /usr/brlcad/lib/librt.so"
 */
RT_EXPORT extern int Rt_Init(Tcl_Interp *interp);


/**
 * Take a db_full_path and append it to the TCL result string.
 */
RT_EXPORT extern void db_full_path_appendresult(Tcl_Interp *interp,
						const struct db_full_path *pp);


/**
 * Expects the Tcl_obj argument (list) to be a Tcl list and extracts
 * list elements, converts them to int, and stores them in the passed
 * in array. If the array_len argument is zero, a new array of
 * appropriate length is allocated. The return value is the number of
 * elements converted.
 */
RT_EXPORT extern int tcl_obj_to_int_array(Tcl_Interp *interp,
					  Tcl_Obj *list,
					  int **array,
					  int *array_len);


/**
 * Expects the Tcl_obj argument (list) to be a Tcl list and extracts
 * list elements, converts them to fastf_t, and stores them in the
 * passed in array. If the array_len argument is zero, a new array of
 * appropriate length is allocated. The return value is the number of
 * elements converted.
 */
RT_EXPORT extern int tcl_obj_to_fastf_array(Tcl_Interp *interp,
					    Tcl_Obj *list,
					    fastf_t **array,
					    int *array_len);


/**
 * interface to above tcl_obj_to_int_array() routine. This routine
 * expects a character string instead of a Tcl_Obj.
 *
 * Returns the number of elements converted.
 */
RT_EXPORT extern int tcl_list_to_int_array(Tcl_Interp *interp,
					   char *char_list,
					   int **array,
					   int *array_len);


/**
 * interface to above tcl_obj_to_fastf_array() routine. This routine
 * expects a character string instead of a Tcl_Obj.
 *
 * returns the number of elements converted.
 */
RT_EXPORT extern int tcl_list_to_fastf_array(Tcl_Interp *interp,
					     const char *char_list,
					     fastf_t **array,
					     int *array_len);

/** @} */
/* rhc.c */
RT_EXPORT extern int rt_mk_hyperbola(struct rt_pt_node *pts,
				     fastf_t r,
				     fastf_t b,
				     fastf_t c,
				     fastf_t dtol,
				     fastf_t ntol);


/* nmg_class.c */
RT_EXPORT extern int nmg_classify_pt_loop(const point_t pt,
					  const struct loopuse *lu,
					  const struct bn_tol *tol);

RT_EXPORT extern int nmg_classify_s_vs_s(struct shell *s,
					 struct shell *s2,
					 const struct bn_tol *tol);

RT_EXPORT extern int nmg_classify_lu_lu(const struct loopuse *lu1,
					const struct loopuse *lu2,
					const struct bn_tol *tol);

RT_EXPORT extern int nmg_class_pt_f(const point_t pt,
				    const struct faceuse *fu,
				    const struct bn_tol *tol);
RT_EXPORT extern int nmg_class_pt_s(const point_t pt,
				    const struct shell *s,
				    const int in_or_out_only,
				    const struct bn_tol *tol);

/* From nmg_pt_fu.c */
RT_EXPORT extern int nmg_eu_is_part_of_crack(const struct edgeuse *eu);

RT_EXPORT extern int nmg_class_pt_lu_except(point_t		pt,
					    const struct loopuse	*lu,
					    const struct edge		*e_p,
					    const struct bn_tol	*tol);

RT_EXPORT extern int nmg_class_pt_fu_except(const point_t pt,
					    const struct faceuse *fu,
					    const struct loopuse *ignore_lu,
					    void (*eu_func)(struct edgeuse *, point_t, const char *),
					    void (*vu_func)(struct vertexuse *, point_t, const char *),
					    const char *priv,
					    const int call_on_hits,
					    const int in_or_out_only,
					    const struct bn_tol *tol);

/* From nmg_plot.c */
RT_EXPORT extern void nmg_pl_shell(FILE *fp,
				   const struct shell *s,
				   int fancy);

RT_EXPORT extern void nmg_vu_to_vlist(struct bu_list *vhead,
				      const struct vertexuse	*vu);
RT_EXPORT extern void nmg_eu_to_vlist(struct bu_list *vhead,
				      const struct bu_list	*eu_hd);
RT_EXPORT extern void nmg_lu_to_vlist(struct bu_list *vhead,
				      const struct loopuse	*lu,
				      int			poly_markers,
				      const vectp_t		norm);
RT_EXPORT extern void nmg_snurb_fu_to_vlist(struct bu_list		*vhead,
					    const struct faceuse	*fu,
					    int			poly_markers);
RT_EXPORT extern void nmg_s_to_vlist(struct bu_list		*vhead,
				     const struct shell	*s,
				     int			poly_markers);
RT_EXPORT extern void nmg_r_to_vlist(struct bu_list		*vhead,
				     const struct nmgregion	*r,
				     int			poly_markers);
RT_EXPORT extern void nmg_m_to_vlist(struct bu_list	*vhead,
				     struct model	*m,
				     int		poly_markers);
RT_EXPORT extern void nmg_offset_eu_vert(point_t			base,
					 const struct edgeuse	*eu,
					 const vect_t		face_normal,
					 int			tip);
/* plot */
RT_EXPORT extern void nmg_pl_v(FILE	*fp,
			       const struct vertex *v,
			       long *b);
RT_EXPORT extern void nmg_pl_e(FILE *fp,
			       const struct edge *e,
			       long *b,
			       int red,
			       int green,
			       int blue);
RT_EXPORT extern void nmg_pl_eu(FILE *fp,
				const struct edgeuse *eu,
				long *b,
				int red,
				int green,
				int blue);
RT_EXPORT extern void nmg_pl_lu(FILE *fp,
				const struct loopuse *fu,
				long *b,
				int red,
				int green,
				int blue);
RT_EXPORT extern void nmg_pl_fu(FILE *fp,
				const struct faceuse *fu,
				long *b,
				int red,
				int green,
				int blue);
RT_EXPORT extern void nmg_pl_s(FILE *fp,
			       const struct shell *s);
RT_EXPORT extern void nmg_pl_r(FILE *fp,
			       const struct nmgregion *r);
RT_EXPORT extern void nmg_pl_m(FILE *fp,
			       const struct model *m);
RT_EXPORT extern void nmg_vlblock_v(struct bn_vlblock *vbp,
				    const struct vertex *v,
				    long *tab);
RT_EXPORT extern void nmg_vlblock_e(struct bn_vlblock *vbp,
				    const struct edge *e,
				    long *tab,
				    int red,
				    int green,
				    int blue);
RT_EXPORT extern void nmg_vlblock_eu(struct bn_vlblock *vbp,
				     const struct edgeuse *eu,
				     long *tab,
				     int red,
				     int green,
				     int blue,
				     int fancy);
RT_EXPORT extern void nmg_vlblock_euleft(struct bu_list			*vh,
					 const struct edgeuse		*eu,
					 const point_t			center,
					 const mat_t			mat,
					 const vect_t			xvec,
					 const vect_t			yvec,
					 double				len,
					 const struct bn_tol		*tol);
RT_EXPORT extern void nmg_vlblock_around_eu(struct bn_vlblock		*vbp,
					    const struct edgeuse	*arg_eu,
					    long			*tab,
					    int			fancy,
					    const struct bn_tol	*tol);
RT_EXPORT extern void nmg_vlblock_lu(struct bn_vlblock *vbp,
				     const struct loopuse *lu,
				     long *tab,
				     int red,
				     int green,
				     int blue,
				     int fancy);
RT_EXPORT extern void nmg_vlblock_fu(struct bn_vlblock *vbp,
				     const struct faceuse *fu,
				     long *tab, int fancy);
RT_EXPORT extern void nmg_vlblock_s(struct bn_vlblock *vbp,
				    const struct shell *s,
				    int fancy);
RT_EXPORT extern void nmg_vlblock_r(struct bn_vlblock *vbp,
				    const struct nmgregion *r,
				    int fancy);
RT_EXPORT extern void nmg_vlblock_m(struct bn_vlblock *vbp,
				    const struct model *m,
				    int fancy);
/* visualization helper routines */
RT_EXPORT extern void nmg_pl_edges_in_2_shells(struct bn_vlblock	*vbp,
					       long			*b,
					       const struct edgeuse	*eu,
					       int			fancy,
					       const struct bn_tol	*tol);
RT_EXPORT extern void nmg_pl_isect(const char		*filename,
				   const struct shell	*s,
				   const struct bn_tol	*tol);
RT_EXPORT extern void nmg_pl_comb_fu(int num1,
				     int num2,
				     const struct faceuse *fu1);
RT_EXPORT extern void nmg_pl_2fu(const char *str,
				 const struct faceuse *fu1,
				 const struct faceuse *fu2,
				 int show_mates);
/* graphical display of classifier results */
RT_EXPORT extern void nmg_show_broken_classifier_stuff(uint32_t	*p,
						       char	**classlist,
						       int	all_new,
						       int	fancy,
						       const char	*a_string);
RT_EXPORT extern void nmg_face_plot(const struct faceuse *fu);
RT_EXPORT extern void nmg_2face_plot(const struct faceuse *fu1,
				     const struct faceuse *fu2);
RT_EXPORT extern void nmg_face_lu_plot(const struct loopuse *lu,
				       const struct vertexuse *vu1,
				       const struct vertexuse *vu2);
RT_EXPORT extern void nmg_plot_lu_ray(const struct loopuse		*lu,
				      const struct vertexuse		*vu1,
				      const struct vertexuse		*vu2,
				      const vect_t			left);
RT_EXPORT extern void nmg_plot_ray_face(const char *fname,
					point_t pt,
					const vect_t dir,
					const struct faceuse *fu);
RT_EXPORT extern void nmg_plot_lu_around_eu(const char		*prefix,
					    const struct edgeuse	*eu,
					    const struct bn_tol	*tol);
RT_EXPORT extern int nmg_snurb_to_vlist(struct bu_list		*vhead,
					const struct face_g_snurb	*fg,
					int			n_interior);
RT_EXPORT extern void nmg_cnurb_to_vlist(struct bu_list *vhead,
					 const struct edgeuse *eu,
					 int n_interior,
					 int cmd);

/**
 * global nmg animation plot callback
 */
RT_EXPORT extern void (*nmg_plot_anim_upcall)(void);

/**
 * global nmg animation vblock callback
 */
RT_EXPORT extern void (*nmg_vlblock_anim_upcall)(void);

/**
 * global nmg mged display debug callback
 */
RT_EXPORT extern void (*nmg_mged_debug_display_hack)(void);

/**
 * edge use distance tolerance
 */
RT_EXPORT extern double nmg_eue_dist;


/* from nmg_mesh.c */
RT_EXPORT extern int nmg_mesh_two_faces(struct faceuse *fu1,
					struct faceuse *fu2,
					const struct bn_tol	*tol);

RT_EXPORT extern void nmg_radial_join_eu(struct edgeuse *eu1,
					 struct edgeuse *eu2,
					 const struct bn_tol *tol);
RT_EXPORT extern void nmg_mesh_faces(struct faceuse *fu1,
				     struct faceuse *fu2,
				     const struct bn_tol *tol);
RT_EXPORT extern int nmg_mesh_face_shell(struct faceuse *fu1,
					 struct shell *s,
					 const struct bn_tol *tol);
RT_EXPORT extern int nmg_mesh_shell_shell(struct shell *s1,
					  struct shell *s2,
					  const struct bn_tol *tol);
RT_EXPORT extern double nmg_measure_fu_angle(const struct edgeuse *eu,
					     const vect_t xvec,
					     const vect_t yvec,
					     const vect_t zvec);

/* from nmg_bool.c */
RT_EXPORT extern struct nmgregion *nmg_do_bool(struct nmgregion *s1,
					       struct nmgregion *s2,
					       const int oper, const struct bn_tol *tol);
RT_EXPORT extern void nmg_shell_coplanar_face_merge(struct shell *s,
						    const struct bn_tol *tol,
						    const int simplify);
RT_EXPORT extern int nmg_two_region_vertex_fuse(struct nmgregion *r1,
						struct nmgregion *r2,
						const struct bn_tol *tol);
RT_EXPORT extern union tree *nmg_booltree_leaf_tess(struct db_tree_state *tsp,
						    const struct db_full_path *pathp,
						    struct rt_db_internal *ip,
						    void *client_data);
RT_EXPORT extern union tree *nmg_booltree_leaf_tnurb(struct db_tree_state *tsp,
						     const struct db_full_path *pathp,
						     struct rt_db_internal *ip,
						     void *client_data);
RT_EXPORT extern int nmg_bool_eval_silent;	/* quell output from nmg_booltree_evaluate */
RT_EXPORT extern union tree *nmg_booltree_evaluate(union tree *tp,
						   const struct bn_tol *tol,
						   struct resource *resp);
RT_EXPORT extern int nmg_boolean(union tree *tp,
				 struct model *m,
				 const struct bn_tol *tol,
				 struct resource *resp);

/* from nmg_class.c */
RT_EXPORT extern void nmg_class_shells(struct shell *sA,
				       struct shell *sB,
				       char **classlist,
				       const struct bn_tol *tol);

/* from nmg_fcut.c */
/* static void ptbl_vsort */
RT_EXPORT extern int nmg_ck_vu_ptbl(struct bu_ptbl	*p,
				    struct faceuse	*fu);
RT_EXPORT extern double nmg_vu_angle_measure(struct vertexuse	*vu,
					     vect_t x_dir,
					     vect_t y_dir,
					     int assessment,
					     int in);
RT_EXPORT extern int nmg_wedge_class(int	ass,	/* assessment of two edges forming wedge */
				     double	a,
				     double	b);
RT_EXPORT extern void nmg_sanitize_fu(struct faceuse	*fu);
RT_EXPORT extern void nmg_unlist_v(struct bu_ptbl	*b,
				   fastf_t *mag,
				   struct vertex	*v);
RT_EXPORT extern struct edge_g_lseg *nmg_face_cutjoin(struct bu_ptbl *b1,
						      struct bu_ptbl *b2,
						      fastf_t *mag1,
						      fastf_t *mag2,
						      struct faceuse *fu1,
						      struct faceuse *fu2,
						      point_t pt,
						      vect_t dir,
						      struct edge_g_lseg *eg,
						      const struct bn_tol *tol);
RT_EXPORT extern void nmg_fcut_face_2d(struct bu_ptbl *vu_list,
				       fastf_t *mag,
				       struct faceuse *fu1,
				       struct faceuse *fu2,
				       struct bn_tol *tol);
RT_EXPORT extern int nmg_insert_vu_if_on_edge(struct vertexuse *vu1,
					      struct vertexuse *vu2,
					      struct edgeuse *new_eu,
					      struct bn_tol *tol);
/* nmg_face_state_transition */

#define nmg_mev(_v, _u) nmg_me((_v), (struct vertex *)NULL, (_u))

/* From nmg_eval.c */
RT_EXPORT extern void nmg_ck_lu_orientation(struct loopuse *lu,
					    const struct bn_tol *tolp);
RT_EXPORT extern const char *nmg_class_name(int class_no);
RT_EXPORT extern void nmg_evaluate_boolean(struct shell	*sA,
					   struct shell	*sB,
					   int		op,
					   char		**classlist,
					   const struct bn_tol	*tol);

/* The following functions cannot be publicly declared because struct
 * nmg_bool_state is private to nmg_eval.c
 */
/* nmg_eval_shell */
/* nmg_eval_action */
/* nmg_eval_plot */


/* From nmg_rt_isect.c */
RT_EXPORT extern void nmg_rt_print_hitlist(struct bu_list *hd);

RT_EXPORT extern void nmg_rt_print_hitmiss(struct hitmiss *a_hit);

RT_EXPORT extern int nmg_class_ray_vs_shell(struct xray *rp,
					    const struct shell *s,
					    const int in_or_out_only,
					    const struct bn_tol *tol);

RT_EXPORT extern void nmg_isect_ray_model(struct ray_data *rd);

/* From nmg_ck.c */
RT_EXPORT extern void nmg_vvg(const struct vertex_g *vg);
RT_EXPORT extern void nmg_vvertex(const struct vertex *v,
				  const struct vertexuse *vup);
RT_EXPORT extern void nmg_vvua(const uint32_t *vua);
RT_EXPORT extern void nmg_vvu(const struct vertexuse *vu,
			      const uint32_t *up_magic_p);
RT_EXPORT extern void nmg_veg(const uint32_t *eg);
RT_EXPORT extern void nmg_vedge(const struct edge *e,
				const struct edgeuse *eup);
RT_EXPORT extern void nmg_veu(const struct bu_list	*hp,
			      const uint32_t *up_magic_p);
RT_EXPORT extern void nmg_vlg(const struct loop_g *lg);
RT_EXPORT extern void nmg_vloop(const struct loop *l,
				const struct loopuse *lup);
RT_EXPORT extern void nmg_vlu(const struct bu_list	*hp,
			      const uint32_t *up);
RT_EXPORT extern void nmg_vfg(const struct face_g_plane *fg);
RT_EXPORT extern void nmg_vface(const struct face *f,
				const struct faceuse *fup);
RT_EXPORT extern void nmg_vfu(const struct bu_list	*hp,
			      const struct shell *s);
RT_EXPORT extern void nmg_vsshell(const struct shell *s,
				 const struct nmgregion *r);
RT_EXPORT extern void nmg_vshell(const struct bu_list *hp,
				 const struct nmgregion *r);
RT_EXPORT extern void nmg_vregion(const struct bu_list *hp,
				  const struct model *m);
RT_EXPORT extern void nmg_vmodel(const struct model *m);

/* checking routines */
RT_EXPORT extern void nmg_ck_e(const struct edgeuse *eu,
			       const struct edge *e,
			       const char *str);
RT_EXPORT extern void nmg_ck_vu(const uint32_t *parent,
				const struct vertexuse *vu,
				const char *str);
RT_EXPORT extern void nmg_ck_eu(const uint32_t *parent,
				const struct edgeuse *eu,
				const char *str);
RT_EXPORT extern void nmg_ck_lg(const struct loop *l,
				const struct loop_g *lg,
				const char *str);
RT_EXPORT extern void nmg_ck_l(const struct loopuse *lu,
			       const struct loop *l,
			       const char *str);
RT_EXPORT extern void nmg_ck_lu(const uint32_t *parent,
				const struct loopuse *lu,
				const char *str);
RT_EXPORT extern void nmg_ck_fg(const struct face *f,
				const struct face_g_plane *fg,
				const char *str);
RT_EXPORT extern void nmg_ck_f(const struct faceuse *fu,
			       const struct face *f,
			       const char *str);
RT_EXPORT extern void nmg_ck_fu(const struct shell *s,
				const struct faceuse *fu,
				const char *str);
RT_EXPORT extern int nmg_ck_eg_verts(const struct edge_g_lseg *eg,
				     const struct bn_tol *tol);
RT_EXPORT extern int nmg_ck_geometry(const struct model *m,
				     const struct bn_tol *tol);
RT_EXPORT extern int nmg_ck_face_worthless_edges(const struct faceuse *fu);
RT_EXPORT extern void nmg_ck_lueu(const struct loopuse *lu, const char *s);
RT_EXPORT extern int nmg_check_radial(const struct edgeuse *eu, const struct bn_tol *tol);
RT_EXPORT extern int nmg_eu_2s_orient_bad(const struct edgeuse	*eu,
					  const struct shell	*s1,
					  const struct shell	*s2,
					  const struct bn_tol	*tol);
RT_EXPORT extern int nmg_ck_closed_surf(const struct shell *s,
					const struct bn_tol *tol);
RT_EXPORT extern int nmg_ck_closed_region(const struct nmgregion *r,
					  const struct bn_tol *tol);
RT_EXPORT extern void nmg_ck_v_in_2fus(const struct vertex *vp,
				       const struct faceuse *fu1,
				       const struct faceuse *fu2,
				       const struct bn_tol *tol);
RT_EXPORT extern void nmg_ck_vs_in_region(const struct nmgregion *r,
					  const struct bn_tol *tol);


/* From nmg_inter.c */
RT_EXPORT extern struct vertexuse *nmg_make_dualvu(struct vertex *v,
						   struct faceuse *fu,
						   const struct bn_tol *tol);
RT_EXPORT extern struct vertexuse *nmg_enlist_vu(struct nmg_inter_struct	*is,
						 const struct vertexuse *vu,
						 struct vertexuse *dualvu,
						 fastf_t dist);
RT_EXPORT extern void nmg_isect2d_prep(struct nmg_inter_struct *is,
				       const uint32_t *assoc_use);
RT_EXPORT extern void nmg_isect2d_cleanup(struct nmg_inter_struct *is);
RT_EXPORT extern void nmg_isect2d_final_cleanup(void);
RT_EXPORT extern int nmg_isect_2faceuse(point_t pt,
					vect_t dir,
					struct faceuse *fu1,
					struct faceuse *fu2,
					const struct bn_tol *tol);
RT_EXPORT extern void nmg_isect_vert2p_face2p(struct nmg_inter_struct *is,
					      struct vertexuse *vu1,
					      struct faceuse *fu2);
RT_EXPORT extern struct edgeuse *nmg_break_eu_on_v(struct edgeuse *eu1,
						   struct vertex *v2,
						   struct faceuse *fu,
						   struct nmg_inter_struct *is);
RT_EXPORT extern void nmg_break_eg_on_v(const struct edge_g_lseg	*eg,
					struct vertex		*v,
					const struct bn_tol	*tol);
RT_EXPORT extern int nmg_isect_2colinear_edge2p(struct edgeuse	*eu1,
						struct edgeuse	*eu2,
						struct faceuse		*fu,
						struct nmg_inter_struct	*is,
						struct bu_ptbl		*l1,
						struct bu_ptbl		*l2);
RT_EXPORT extern int nmg_isect_edge2p_edge2p(struct nmg_inter_struct	*is,
					     struct edgeuse		*eu1,
					     struct edgeuse		*eu2,
					     struct faceuse		*fu1,
					     struct faceuse		*fu2);
RT_EXPORT extern int nmg_isect_construct_nice_ray(struct nmg_inter_struct	*is,
						  struct faceuse		*fu2);
RT_EXPORT extern void nmg_enlist_one_vu(struct nmg_inter_struct	*is,
					const struct vertexuse	*vu,
					fastf_t			dist);
RT_EXPORT extern int	nmg_isect_line2_edge2p(struct nmg_inter_struct	*is,
					       struct bu_ptbl		*list,
					       struct edgeuse		*eu1,
					       struct faceuse		*fu1,
					       struct faceuse		*fu2);
RT_EXPORT extern void nmg_isect_line2_vertex2(struct nmg_inter_struct	*is,
					      struct vertexuse	*vu1,
					      struct faceuse		*fu1);
RT_EXPORT extern int nmg_isect_two_ptbls(struct nmg_inter_struct	*is,
					 const struct bu_ptbl	*t1,
					 const struct bu_ptbl	*t2);
RT_EXPORT extern struct edge_g_lseg	*nmg_find_eg_on_line(const uint32_t *magic_p,
							     const point_t		pt,
							     const vect_t		dir,
							     const struct bn_tol	*tol);
RT_EXPORT extern int nmg_k0eu(struct vertex	*v);
RT_EXPORT extern struct vertex *nmg_repair_v_near_v(struct vertex		*hit_v,
						    struct vertex		*v,
						    const struct edge_g_lseg	*eg1,
						    const struct edge_g_lseg	*eg2,
						    int			bomb,
						    const struct bn_tol	*tol);
RT_EXPORT extern struct vertex *nmg_search_v_eg(const struct edgeuse	*eu,
						int			second,
						const struct edge_g_lseg	*eg1,
						const struct edge_g_lseg	*eg2,
						struct vertex		*hit_v,
						const struct bn_tol	*tol);
RT_EXPORT extern struct vertex *nmg_common_v_2eg(struct edge_g_lseg	*eg1,
						 struct edge_g_lseg	*eg2,
						 const struct bn_tol	*tol);
RT_EXPORT extern int nmg_is_vertex_on_inter(struct vertex *v,
					    struct faceuse *fu1,
					    struct faceuse *fu2,
					    struct nmg_inter_struct *is);
RT_EXPORT extern void nmg_isect_eu_verts(struct edgeuse *eu,
					 struct vertex_g *vg1,
					 struct vertex_g *vg2,
					 struct bu_ptbl *verts,
					 struct bu_ptbl *inters,
					 const struct bn_tol *tol);
RT_EXPORT extern void nmg_isect_eu_eu(struct edgeuse *eu1,
				      struct vertex_g *vg1a,
				      struct vertex_g *vg1b,
				      vect_t dir1,
				      struct edgeuse *eu2,
				      struct bu_ptbl *verts,
				      struct bu_ptbl *inters,
				      const struct bn_tol *tol);
RT_EXPORT extern void nmg_isect_eu_fu(struct nmg_inter_struct *is,
				      struct bu_ptbl		*verts,
				      struct edgeuse		*eu,
				      struct faceuse          *fu);
RT_EXPORT extern void nmg_isect_fu_jra(struct nmg_inter_struct	*is,
				       struct faceuse		*fu1,
				       struct faceuse		*fu2,
				       struct bu_ptbl		*eu1_list,
				       struct bu_ptbl		*eu2_list);
RT_EXPORT extern void nmg_isect_line2_face2pNEW(struct nmg_inter_struct *is,
						struct faceuse *fu1, struct faceuse *fu2,
						struct bu_ptbl *eu1_list,
						struct bu_ptbl *eu2_list);
RT_EXPORT extern int	nmg_is_eu_on_line3(const struct edgeuse	*eu,
					   const point_t		pt,
					   const vect_t		dir,
					   const struct bn_tol	*tol);
RT_EXPORT extern struct edge_g_lseg	*nmg_find_eg_between_2fg(const struct faceuse	*ofu1,
								 const struct faceuse	*fu2,
								 const struct bn_tol	*tol);
RT_EXPORT extern struct edgeuse *nmg_does_fu_use_eg(const struct faceuse	*fu1,
						    const uint32_t	*eg);
RT_EXPORT extern int rt_line_on_plane(const point_t	pt,
				      const vect_t	dir,
				      const plane_t	plane,
				      const struct bn_tol	*tol);
RT_EXPORT extern void nmg_cut_lu_into_coplanar_and_non(struct loopuse *lu,
						       plane_t pl,
						       struct nmg_inter_struct *is);
RT_EXPORT extern void nmg_check_radial_angles(char *str,
					      struct shell *s,
					      const struct bn_tol *tol);
RT_EXPORT extern int nmg_faces_can_be_intersected(struct nmg_inter_struct *bs,
						  const struct faceuse *fu1,
						  const struct faceuse *fu2,
						  const struct bn_tol *tol);
RT_EXPORT extern void nmg_isect_two_generic_faces(struct faceuse		*fu1,
						  struct faceuse		*fu2,
						  const struct bn_tol	*tol);
RT_EXPORT extern void nmg_crackshells(struct shell *s1,
				      struct shell *s2,
				      const struct bn_tol *tol);
RT_EXPORT extern int nmg_fu_touchingloops(const struct faceuse *fu);


/* From nmg_index.c */
RT_EXPORT extern int nmg_index_of_struct(const uint32_t *p);
RT_EXPORT extern void nmg_m_set_high_bit(struct model *m);
RT_EXPORT extern void nmg_m_reindex(struct model *m, long newindex);
RT_EXPORT extern void nmg_vls_struct_counts(struct bu_vls *str,
					    const struct nmg_struct_counts *ctr);
RT_EXPORT extern void nmg_pr_struct_counts(const struct nmg_struct_counts *ctr,
					   const char *str);
RT_EXPORT extern uint32_t **nmg_m_struct_count(struct nmg_struct_counts *ctr,
						    const struct model *m);
RT_EXPORT extern void nmg_pr_m_struct_counts(const struct model	*m,
					     const char		*str);
RT_EXPORT extern void nmg_merge_models(struct model *m1,
				       struct model *m2);
RT_EXPORT extern long nmg_find_max_index(const struct model *m);

/* From nmg_rt.c */

/* From dspline.c */

/**
 * Initialize a spline matrix for a particular spline type.
 */
RT_EXPORT extern void rt_dspline_matrix(mat_t m, const char *type,
					const double	tension,
					const double	bias);

/**
 * m		spline matrix (see rt_dspline4_matrix)
 * a, b, c, d	knot values
 * alpha:	0..1	interpolation point
 *
 * Evaluate a 1-dimensional spline at a point "alpha" on knot values
 * a, b, c, d.
 */
RT_EXPORT extern double rt_dspline4(mat_t m,
				    double a,
				    double b,
				    double c,
				    double d,
				    double alpha);

/**
 * pt		vector to receive the interpolation result
 * m		spline matrix (see rt_dspline4_matrix)
 * a, b, c, d	knot values
 * alpha:	0..1 interpolation point
 *
 * Evaluate a spline at a point "alpha" between knot pts b & c
 * The knots and result are all vectors with "depth" values (length).
 *
 */
RT_EXPORT extern void rt_dspline4v(double *pt,
				   const mat_t m,
				   const double *a,
				   const double *b,
				   const double *c,
				   const double *d,
				   const int depth,
				   const double alpha);

/**
 * Interpolate n knot vectors over the range 0..1
 *
 * "knots" is an array of "n" knot vectors.  Each vector consists of
 * "depth" values.  They define an "n" dimensional surface which is
 * evaluated at the single point "alpha".  The evaluated point is
 * returned in "r"
 *
 * Example use:
 *
 * double result[MAX_DEPTH], knots[MAX_DEPTH*MAX_KNOTS];
 * mat_t m;
 * int knot_count, depth;
 *
 * knots = bu_malloc(sizeof(double) * knot_length * knot_count);
 * result = bu_malloc(sizeof(double) * knot_length);
 *
 * rt_dspline4_matrix(m, "Catmull", (double *)NULL, 0.0);
 *
 * for (i=0; i < knot_count; i++)
 *   get a knot(knots, i, knot_length);
 *
 * rt_dspline_n(result, m, knots, knot_count, knot_length, alpha);
 *
 */
RT_EXPORT extern void rt_dspline_n(double *r,
				   const mat_t m,
				   const double *knots,
				   const int n,
				   const int depth,
				   const double alpha);

/* From nmg_fuse.c */
RT_EXPORT extern int nmg_is_common_bigloop(const struct face *f1,
					   const struct face *f2);
RT_EXPORT extern void nmg_region_v_unique(struct nmgregion *r1,
					  const struct bn_tol *tol);
RT_EXPORT extern int nmg_ptbl_vfuse(struct bu_ptbl *t,
				    const struct bn_tol *tol);
RT_EXPORT extern int	nmg_region_both_vfuse(struct bu_ptbl *t1,
					      struct bu_ptbl *t2,
					      const struct bn_tol *tol);
/* nmg_two_region_vertex_fuse replaced with nmg_vertex_fuse */
RT_EXPORT extern int nmg_vertex_fuse(const uint32_t *magic_p,
				     const struct bn_tol *tol);
RT_EXPORT extern int nmg_cnurb_is_linear(const struct edge_g_cnurb *cnrb);
RT_EXPORT extern int nmg_snurb_is_planar(const struct face_g_snurb *srf,
					 const struct bn_tol *tol);
RT_EXPORT extern void nmg_eval_linear_trim_curve(const struct face_g_snurb *snrb,
						 const fastf_t uvw[3],
						 point_t xyz);
RT_EXPORT extern void nmg_eval_trim_curve(const struct edge_g_cnurb *cnrb,
					  const struct face_g_snurb *snrb,
					  const fastf_t t, point_t xyz);
/* nmg_split_trim */
RT_EXPORT extern void nmg_eval_trim_to_tol(const struct edge_g_cnurb *cnrb,
					   const struct face_g_snurb *snrb,
					   const fastf_t t0,
					   const fastf_t t1,
					   struct bu_list *head,
					   const struct bn_tol *tol);
/* nmg_split_linear_trim */
RT_EXPORT extern void nmg_eval_linear_trim_to_tol(const struct edge_g_cnurb *cnrb,
						  const struct face_g_snurb *snrb,
						  const fastf_t uvw1[3],
						  const fastf_t uvw2[3],
						  struct bu_list *head,
						  const struct bn_tol *tol);
RT_EXPORT extern int nmg_cnurb_lseg_coincident(const struct edgeuse *eu1,
					       const struct edge_g_cnurb *cnrb,
					       const struct face_g_snurb *snrb,
					       const point_t pt1,
					       const point_t pt2,
					       const struct bn_tol *tol);
RT_EXPORT extern int	nmg_cnurb_is_on_crv(const struct edgeuse *eu,
					    const struct edge_g_cnurb *cnrb,
					    const struct face_g_snurb *snrb,
					    const struct bu_list *head,
					    const struct bn_tol *tol);
RT_EXPORT extern int nmg_edge_fuse(const uint32_t *magic_p,
				   const struct bn_tol *tol);
RT_EXPORT extern int nmg_edge_g_fuse(const uint32_t *magic_p,
					   const struct bn_tol	*tol);
RT_EXPORT extern int nmg_ck_fu_verts(struct faceuse *fu1,
				     struct face *f2,
				     const struct bn_tol *tol);
RT_EXPORT extern int nmg_ck_fg_verts(struct faceuse *fu1,
				     struct face *f2,
				     const struct bn_tol *tol);
RT_EXPORT extern int	nmg_two_face_fuse(struct face	*f1,
					  struct face *f2,
					  const struct bn_tol *tol);
RT_EXPORT extern int nmg_model_face_fuse(struct model *m,
					 const struct bn_tol *tol);
RT_EXPORT extern int nmg_break_all_es_on_v(uint32_t *magic_p,
					   struct vertex *v,
					   const struct bn_tol *tol);
RT_EXPORT extern int nmg_break_e_on_v(const uint32_t *magic_p,
				      const struct bn_tol *tol);
/* DEPRECATED: use nmg_break_e_on_v */
RT_EXPORT extern int nmg_model_break_e_on_v(const uint32_t *magic_p,
					    const struct bn_tol *tol);
RT_EXPORT extern int nmg_model_fuse(struct model *m,
				    const struct bn_tol *tol);

/* radial routines */
RT_EXPORT extern void nmg_radial_sorted_list_insert(struct bu_list *hd,
						    struct nmg_radial *rad);
RT_EXPORT extern void nmg_radial_verify_pointers(const struct bu_list *hd,
						 const struct bn_tol *tol);
RT_EXPORT extern void nmg_radial_verify_monotone(const struct bu_list	*hd,
						 const struct bn_tol	*tol);
RT_EXPORT extern void nmg_insure_radial_list_is_increasing(struct bu_list	*hd,
							   fastf_t amin, fastf_t amax);
RT_EXPORT extern void nmg_radial_build_list(struct bu_list		*hd,
					    struct bu_ptbl		*shell_tbl,
					    int			existing,
					    struct edgeuse		*eu,
					    const vect_t		xvec,
					    const vect_t		yvec,
					    const vect_t		zvec,
					    const struct bn_tol	*tol);
RT_EXPORT extern void nmg_radial_merge_lists(struct bu_list		*dest,
					     struct bu_list		*src,
					     const struct bn_tol	*tol);
RT_EXPORT extern int	 nmg_is_crack_outie(const struct edgeuse	*eu,
					    const struct bn_tol	*tol);
RT_EXPORT extern struct nmg_radial	*nmg_find_radial_eu(const struct bu_list *hd,
							    const struct edgeuse *eu);
RT_EXPORT extern const struct edgeuse *nmg_find_next_use_of_2e_in_lu(const struct edgeuse	*eu,
								     const struct edge	*e1,
								     const struct edge	*e2);
RT_EXPORT extern void nmg_radial_mark_cracks(struct bu_list	*hd,
					     const struct edge	*e1,
					     const struct edge	*e2,
					     const struct bn_tol	*tol);
RT_EXPORT extern struct nmg_radial *nmg_radial_find_an_original(const struct bu_list	*hd,
								const struct shell	*s,
								const struct bn_tol	*tol);
RT_EXPORT extern int nmg_radial_mark_flips(struct bu_list		*hd,
					   const struct shell	*s,
					   const struct bn_tol	*tol);
RT_EXPORT extern int nmg_radial_check_parity(const struct bu_list	*hd,
					     const struct bu_ptbl	*shells,
					     const struct bn_tol	*tol);
RT_EXPORT extern void nmg_radial_implement_decisions(struct bu_list		*hd,
						     const struct bn_tol	*tol,
						     struct edgeuse		*eu1,
						     vect_t xvec,
						     vect_t yvec,
						     vect_t zvec);
RT_EXPORT extern void nmg_pr_radial(const char *title,
				    const struct nmg_radial	*rad);
RT_EXPORT extern void nmg_pr_radial_list(const struct bu_list *hd,
					 const struct bn_tol *tol);
RT_EXPORT extern void nmg_do_radial_flips(struct bu_list *hd);
RT_EXPORT extern void nmg_do_radial_join(struct bu_list *hd,
					 struct edgeuse *eu1ref,
					 vect_t xvec, vect_t yvec, vect_t zvec,
					 const struct bn_tol *tol);
RT_EXPORT extern void nmg_radial_join_eu_NEW(struct edgeuse *eu1,
					     struct edgeuse *eu2,
					     const struct bn_tol *tol);
RT_EXPORT extern void nmg_radial_exchange_marked(struct bu_list		*hd,
						 const struct bn_tol	*tol);
RT_EXPORT extern void nmg_s_radial_harmonize(struct shell		*s,
					     const struct bn_tol	*tol);
RT_EXPORT extern void nmg_s_radial_check(struct shell		*s,
					 const struct bn_tol	*tol);
RT_EXPORT extern void nmg_r_radial_check(const struct nmgregion	*r,
					 const struct bn_tol	*tol);


RT_EXPORT extern struct edge_g_lseg	*nmg_pick_best_edge_g(struct edgeuse *eu1,
							      struct edgeuse *eu2,
							      const struct bn_tol *tol);

/* nmg_visit.c */
RT_EXPORT extern void nmg_visit_vertex(struct vertex			*v,
				       const struct nmg_visit_handlers	*htab,
				       void *			state);
RT_EXPORT extern void nmg_visit_vertexuse(struct vertexuse		*vu,
					  const struct nmg_visit_handlers	*htab,
					  void *			state);
RT_EXPORT extern void nmg_visit_edge(struct edge			*e,
				     const struct nmg_visit_handlers	*htab,
				     void *			state);
RT_EXPORT extern void nmg_visit_edgeuse(struct edgeuse			*eu,
					const struct nmg_visit_handlers	*htab,
					void *			state);
RT_EXPORT extern void nmg_visit_loop(struct loop			*l,
				     const struct nmg_visit_handlers	*htab,
				     void *			state);
RT_EXPORT extern void nmg_visit_loopuse(struct loopuse			*lu,
					const struct nmg_visit_handlers	*htab,
					void *			state);
RT_EXPORT extern void nmg_visit_face(struct face			*f,
				     const struct nmg_visit_handlers	*htab,
				     void *			state);
RT_EXPORT extern void nmg_visit_faceuse(struct faceuse			*fu,
					const struct nmg_visit_handlers	*htab,
					void *			state);
RT_EXPORT extern void nmg_visit_shell(struct shell			*s,
				      const struct nmg_visit_handlers	*htab,
				      void *			state);
RT_EXPORT extern void nmg_visit_region(struct nmgregion		*r,
				       const struct nmg_visit_handlers	*htab,
				       void *			state);
RT_EXPORT extern void nmg_visit_model(struct model			*model,
				      const struct nmg_visit_handlers	*htab,
				      void *			state);
RT_EXPORT extern void nmg_visit(const uint32_t		*magicp,
				const struct nmg_visit_handlers	*htab,
				void *			state);

/* db5_types.c */
RT_EXPORT extern int db5_type_tag_from_major(char	**tag,
					     const int	major);

RT_EXPORT extern int db5_type_descrip_from_major(char	**descrip,
						 const int	major);

RT_EXPORT extern int db5_type_tag_from_codes(char	**tag,
					     const int	major,
					     const int	minor);

RT_EXPORT extern int db5_type_descrip_from_codes(char	**descrip,
						 const int	major,
						 const int	minor);

RT_EXPORT extern int db5_type_codes_from_tag(int	*major,
					     int	*minor,
					     const char	*tag);

RT_EXPORT extern int db5_type_codes_from_descrip(int	*major,
						 int	*minor,
						 const char	*descrip);

RT_EXPORT extern size_t db5_type_sizeof_h_binu(const int minor);

RT_EXPORT extern size_t db5_type_sizeof_n_binu(const int minor);


/* db5_attr.c */
/**
 * Define standard attribute types in BRL-CAD geometry.  (See the
 * attributes manual page.)  These should be a collective enumeration
 * starting from 0 and increasing without any gaps in the numbers so
 * db5_standard_attribute() can be used as an index-based iterator.
 */
enum {
    ATTR_REGION = 0,
    ATTR_REGION_ID,
    ATTR_MATERIAL_ID,
    ATTR_AIR,
    ATTR_LOS,
    ATTR_COLOR,
    ATTR_SHADER,
    ATTR_INHERIT,
    ATTR_TIMESTAMP, /* a binary attribute */
    ATTR_NULL
};

/* Enum to characterize status of attributes */
enum {
    ATTR_STANDARD = 0,
    ATTR_USER_DEFINED,
    ATTR_UNKNOWN_ORIGIN
};

struct db5_attr_ctype {
    int attr_type;    /* ID from the main attr enum list */
    int is_binary;   /* false for ASCII attributes; true for binary attributes */
    int attr_subtype; /* ID from attribute status enum list */

    /* names should be specified with alphanumeric characters
     * (lower-case letters, no white space) and will act as unique
     * keys to an object's attribute list */
    const char *name;         /* the "standard" name */
    const char *description;
    const char *examples;
    /* list of alternative names for this attribute: */
    const char *aliases;
    /* property name */
    const char *property;
    /* a longer description for lists outside a table */
    const char *long_description;
};

RT_EXPORT extern const struct db5_attr_ctype db5_attr_std[];

/* Container for holding user-registered attributes */
struct db5_registry{
    void *internal; /**< @brief implementation-specific container for holding information*/
};

/**
 * Initialize a user attribute registry
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_attr_registry_init(struct db5_registry *registry);

/**
 * Free a user attribute registry
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_attr_registry_free(struct db5_registry *registry);

/**
 * Register a user attribute
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_attr_create(struct db5_registry *registry,
	                             int attr_type,
				     int is_binary,
				     int attr_subtype,
				     const char *name,
				     const char *description,
				     const char *examples,
				     const char *aliases,
				     const char *property,
				     const char *long_description);

/**
 * Register a user attribute
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_attr_register(struct db5_registry *registry,
	                               struct db5_attr_ctype *attribute);

/**
 * De-register a user attribute
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_attr_deregister(struct db5_registry *registry,
	                                 const char *name);

/**
 * Look to see if a specific attribute is registered
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern struct db5_attr_ctype *db5_attr_get(struct db5_registry *registry,
	                                             const char *name);

/**
 * Get an array of pointers to all registered attributes
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern struct db5_attr_ctype **db5_attr_dump(struct db5_registry *registry);


/**
 * Function returns the string name for a given standard attribute
 * index.  Index values returned from db5_standardize_attribute()
 * correspond to the names returned from this function, returning the
 * "standard" name.  Callers may also iterate over all names starting
 * with an index of zero until a NULL is returned.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern const char *db5_standard_attribute(int idx);


/**
 * Function returns the string definition for a given standard
 * attribute index.  Index values returned from
 * db5_standardize_attribute_def() correspond to the definition
 * returned from this function, returning the "standard" definition.
 * Callers may also iterate over all names starting with an index of
 * zero until a NULL is returned.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern const char *db5_standard_attribute_def(int idx);

/**
 * Function for recognizing various versions of the DB5 standard
 * attribute names that have been used - returns the attribute type
 * of the supplied attribute name, or -1 if it is not a recognized
 * variation of the standard attributes.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_is_standard_attribute(const char *attrname);

/**
 * Ensures that an attribute set containing standard attributes with
 * non-standard/old/deprecated names gets the standard name added.  It
 * will update the first non-standard name encountered, but will leave
 * any subsequent matching attributes found unmodified if they have
 * different values.  Such "conflict" attributes must be resolved
 * manually.
 *
 * Returns the number of conflicting attributes.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern size_t db5_standardize_avs(struct bu_attribute_value_set *avs);

/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_standardize_attribute(const char *attr);
/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_sync_attr_to_comb(struct rt_comb_internal *comb, const struct bu_attribute_value_set *avs, struct directory *dp);
/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_sync_comb_to_attr(struct bu_attribute_value_set *avs, const struct rt_comb_internal *comb);

/* Convenience macros */
#define ATTR_STD(attr) db5_standard_attribute(db5_standardize_attribute(attr))


#endif

/* defined in binary_obj.c */
RT_EXPORT extern int rt_mk_binunif(struct rt_wdb *wdbp,
				   const char *obj_name,
				   const char *file_name,
				   unsigned int minor_type,
				   size_t max_count);


/* defined in db5_bin.c */

/**
 * Free the storage associated with a binunif_internal object
 */
RT_EXPORT extern void rt_binunif_free(struct rt_binunif_internal *bip);

/**
 * Diagnostic routine
 */
RT_EXPORT extern void rt_binunif_dump(struct rt_binunif_internal *bip);

/* defined in cline.c */

/**
 * radius of a FASTGEN cline element.
 *
 * shared with rt/do.c
 */
RT_EXPORT extern fastf_t rt_cline_radius;

/* defined in bot.c */
/* TODO - these global variables need to be rolled in to the rt_i structure */
RT_EXPORT extern size_t rt_bot_minpieces;
RT_EXPORT extern size_t rt_bot_tri_per_piece;
RT_EXPORT extern size_t rt_bot_mintie;
RT_EXPORT extern int rt_bot_sort_faces(struct rt_bot_internal *bot,
				       size_t tris_per_piece);
RT_EXPORT extern int rt_bot_decimate(struct rt_bot_internal *bot,
				     fastf_t max_chord_error,
				     fastf_t max_normal_error,
				     fastf_t min_edge_length);

/*
 *  Constants provided and used by the RT library.
 */

/**
 * initial tree start for db tree walkers.
 *
 * Also used by converters in conv/ directory.  Don't forget to
 * initialize ts_dbip before use.
 */
RT_EXPORT extern const struct db_tree_state rt_initial_tree_state;
RT_EXPORT extern const char *rt_vlist_cmd_descriptions[];


/** @file librt/vers.c
 *
 * version information about LIBRT
 *
 */

/**
 * report version information about LIBRT
 */
RT_EXPORT extern const char *rt_version(void);

__END_DECLS

#endif /* RAYTRACE_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
