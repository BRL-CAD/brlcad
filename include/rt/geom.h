/*                        G E O M . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @addtogroup rt_geom
 *
 * @brief Functions and details of the internal forms used by the LIBRT geometry
 * routines for the different solids.
 *
 * These structures are what the struct rt_db_internal generic pointer
 * idb_ptr points at, based on idb_type indicating a solid id ID_xxx,
 * such as ID_TGC.
 *
 */
/** @{ */
/** @file rt/geom.h */
/** @} */

#ifndef RT_GEOM_H
#define RT_GEOM_H

#include "common.h"

#include "bu/bitv.h"
#include "bu/color.h"
#include "bu/list.h"
#include "bu/mapped_file.h"
#include "bu/vls.h"
#include "vmath.h"
#include "rt/defines.h"

#include "../nmg.h" /* (temporarily?) needed for knot_vector */
#include "brep.h"


__BEGIN_DECLS

#define NAMELEN 16	/* NAMESIZE from db.h (can't call it NAMESIZE!!!!!) */

/** @addtogroup rt_tor */
/** @{ */
/*
 * ID_TOR
 */
struct rt_tor_internal {
    uint32_t magic;
    point_t v;		/**< @brief center point */
    vect_t h;		/**< @brief normal, unit length */
    fastf_t r_h;	/**< @brief radius in H direction (r2) */
    fastf_t r_a;	/**< @brief radius in A direction (r1) */
    /* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
    vect_t a;		/**< @brief r_a length */
    vect_t b;		/**< @brief r_b length */
    fastf_t r_b;	/**< @brief radius in B direction (typ == r_a) */
};
#define RT_TOR_CK_MAGIC(_p) BU_CKMAG(_p, RT_TOR_INTERNAL_MAGIC, "rt_tor_internal")
/** @} */

/** @addtogroup rt_tgc */
/** @{ */
/**
 * ID_TGC and ID_REC
 */
struct rt_tgc_internal {
    uint32_t magic;
    point_t v;
    vect_t h;
    vect_t a;
    vect_t b;
    vect_t c;
    vect_t d;
};
#define RT_TGC_CK_MAGIC(_p) BU_CKMAG(_p, RT_TGC_INTERNAL_MAGIC, "rt_tgc_internal")
/** @} */

/** @addtogroup rt_ell */
/** @{ */
/*
 * ID_ELL, and ID_SPH
 */
struct rt_ell_internal {
    uint32_t magic;
    point_t v; /**< @brief center point */
    vect_t a;  /**< @brief axis a radial length */
    vect_t b;  /**< @brief axis b radial length */
    vect_t c;  /**< @brief axis c radial length */
};
#define RT_ELL_CK_MAGIC(_p) BU_CKMAG(_p, RT_ELL_INTERNAL_MAGIC, "rt_ell_internal")
/** @} */

/** @addtogroup rt_superell */
/** @{ */
/*
 * ID_SUPERELL
 */
struct rt_superell_internal {
    uint32_t magic;
    point_t v;
    vect_t a;
    vect_t b;
    vect_t c;
    double n;
    double e;
};
#define RT_SUPERELL_CK_MAGIC(_p) BU_CKMAG(_p, RT_SUPERELL_INTERNAL_MAGIC, "rt_superell_internal")
/** @} */

/** @addtogroup rt_metaball */
/** @{ */
/**
 * ID_METABALL
 *
 * The "metaball" primitive contains a method ID, threshold value, and
 * an unordered set of control points. Each control point contains a
 * 3d location, a "field strength", and possibly a "blobbiness" value
 * (called "goo" in rt_metaball_add_point).
 *
 * There are three method ID's defined:
 *
 * 1. "metaball", which is the Tokyo Metaball approximation of the
 * Blinn Blobby Surface. This method is not implemented yet.
 *
 * 2. "blob", the Blinn method.
 *
 * 3. "iso", which is a simple computation like you'd see for
 * computing gravitational magnitude or point charge in a basic
 * physics course.  Blending function in latex notation is:
 *
 @code
 \Sum_{i}\frac{f_{i}}{d^{2}}
 @endcode
 *
 * The surface of the primitive exists where the summation of the
 * points contribution is equal to the threshold, with the general
 * fldstr/distance^2 pattern.
 *
 * The blobbiness value is only used in the blob method, and modifies
 * the gusseting effect.
 *
 */
struct rt_metaball_internal {
    uint32_t magic;
    /* these three defines are used with the method field */
#define METABALL_METABALL 0
#define METABALL_ISOPOTENTIAL 1
#define METABALL_BLOB 2
    int method;
    fastf_t threshold;
    fastf_t initstep;
    fastf_t finalstep; /* for raytrace stepping. */
    struct bu_list metaball_ctrl_head;
};
#define RT_METABALL_CK_MAGIC(_p) BU_CKMAG(_p, RT_METABALL_INTERNAL_MAGIC, "rt_metaball_internal")
struct wdb_metaball_pnt {
    struct bu_list l;
    int type;
    fastf_t fldstr; /**< @brief  field strength */
    fastf_t sweat;  /**< @brief  beta value used for metaball and blob evaluation */
    point_t coord;
    point_t coord2;
};
#define WDB_METABALLPT_TYPE_POINT 0x0
#define WDB_METABALLPT_TYPE_LINE 0x1
#define WDB_METABALL_PNT_NULL	((struct wdb_metaball_pnt *)0)
/** @} */


/** @addtogroup rt_arb */
/** @{ */
/*
 * ID_ARB8
 *
 * The internal (in memory) form of an ARB8 -- 8 points in space.
 * The first 4 form the "bottom" face, the second 4 form the "top" face.
 */
struct rt_arb_internal {
    uint32_t magic;
    point_t pt[8];
};
#define RT_ARB_CK_MAGIC(_p) BU_CKMAG(_p, RT_ARB_INTERNAL_MAGIC, "rt_arb_internal")
/** @} */

/** @addtogroup rt_ars */
/** @{ */
/*
 * ID_ARS
 */
struct rt_ars_internal {
    uint32_t magic;
    size_t ncurves;
    size_t pts_per_curve;
    fastf_t **curves;
};
#define RT_ARS_CK_MAGIC(_p) BU_CKMAG(_p, RT_ARS_INTERNAL_MAGIC, "rt_ars_internal")
/** @} */

/** @addtogroup rt_half */
/** @{ */
/*
 * ID_HALF
 */
struct rt_half_internal {
    uint32_t magic;
    plane_t eqn;
};
#define RT_HALF_CK_MAGIC(_p) BU_CKMAG(_p, RT_HALF_INTERNAL_MAGIC, "rt_half_internal")
/** @} */

/** @addtogroup rt_grip */
/** @{ */
/*
 * ID_GRIP
 */
struct rt_grip_internal {
    uint32_t magic;
    point_t center;
    /* Remaining elements are used for display purposes only */
    vect_t normal;
    fastf_t mag;
};
#define RT_GRIP_CK_MAGIC(_p) BU_CKMAG(_p, RT_GRIP_INTERNAL_MAGIC, "rt_grip_internal")
/** @} */

/** @addtogroup rt_joint */
/** @{ */
/*
 * ID_JOINT
 */
struct rt_joint_internal {
    uint32_t magic;
    point_t location;
    struct bu_vls reference_path_1;
    struct bu_vls reference_path_2;

    /* Remaining elements are used for display purposes only */
    vect_t vector1;
    vect_t vector2;
    fastf_t value;
};
#define RT_JOINT_CK_MAGIC(_p) BU_CKMAG(_p, RT_JOINT_INTERNAL_MAGIC, "rt_joint_internal")
/** @} */

/** @addtogroup rt_pg */
/** @{ */
/**
 * ID_POLY
 */
struct rt_pg_face_internal {
    size_t npts;		/**< @brief number of points for this polygon */
    fastf_t *verts;		/**< @brief has 3*npts elements */
    fastf_t *norms;		/**< @brief has 3*npts elements */
};			        /**< @brief has npoly elements */
struct rt_pg_internal {
    uint32_t magic;
    size_t npoly;
    struct rt_pg_face_internal *poly;	/**< @brief has npoly elements */
    /* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
    size_t max_npts;		/**< @brief maximum value of npts in poly[] */
};
#define RT_PG_CK_MAGIC(_p) BU_CKMAG(_p, RT_PG_INTERNAL_MAGIC, "rt_pg_internal")
/** @} */

/** @addtogroup rt_nurb */
/** @{ */
/* ID_BSPLINE */
struct rt_nurb_internal {
    uint32_t magic;
    int nsrf;			/**< @brief number of surfaces */
    struct face_g_snurb **srfs;	/**< @brief The surfaces themselves */
    ON_Brep *brep;
};


#define RT_NURB_CK_MAGIC(_p) BU_CKMAG(_p, RT_NURB_INTERNAL_MAGIC, "rt_nurb_internal");
#define RT_NURB_GET_CONTROL_POINT(_s, _u, _v)	((_s)->ctl_points[ \
						     ((_v)*(_s)->s_size[0]+(_u))*RT_NURB_EXTRACT_COORDS((_s)->pt_type)])
/** @} */

/** @addtogroup rt_brep */
/** @{ */
/* ID_BREP */
struct rt_brep_internal {
    uint32_t magic;

    ON_Brep* brep; /**< @brief An openNURBS brep object containing the solid */
};


#define RT_BREP_CK_MAGIC(_p) BU_CKMAG(_p, RT_BREP_INTERNAL_MAGIC, "rt_brep_internal");
#define RT_BREP_TEST_MAGIC(_p) ((_p) && (*((uint32_t *)(_p)) == (uint32_t)(RT_BREP_INTERNAL_MAGIC)))
/** @} */


/*
 * The internal form of the NMG is not rt_nmg_internal, but just a
 * "struct model", from nmg.h.  e.g.:
 *
 * if (intern.idb_type == ID_NMG)
 * m = (struct model *)intern.idb_ptr;
 */

/** @addtogroup rt_ebm */
/** @{ */
/*
 * ID_EBM
 */
#define RT_EBM_NAME_LEN 256
struct rt_ebm_internal {
    uint32_t magic;
    char name[RT_EBM_NAME_LEN];
    /* NOTE: xdim/ydim cannot be size_t until rel8 as they are
     * written out to disk via bu_vls_struct_print() as 32-bit ints.
     */
    uint32_t xdim;	/**< @brief X dimension (w cells) */
    uint32_t ydim;	/**< @brief Y dimension (n cells) */
    fastf_t tallness;	/**< @brief Z dimension (mm) */
    mat_t mat;		/**< @brief convert local coords to model space */
    unsigned char *buf; /**< @brief actual data */
    struct bu_mapped_file *mp;	/**< @brief mapped file for data */
    struct rt_db_internal *bip; /**< @brief db object for data */
#define RT_EBM_SRC_FILE 'f'
#define RT_EBM_SRC_OBJ 'o'
    char datasrc; /**< @brief which type of data source */
};
#define RT_EBM_CK_MAGIC(_p) BU_CKMAG(_p, RT_EBM_INTERNAL_MAGIC, "rt_ebm_internal")
/** @} */

/** @addtogroup rt_vol */
/** @{ */
/*
 * ID_VOL
 */
#define RT_VOL_NAME_LEN 128
struct rt_vol_internal {
    uint32_t magic;
    char name[RT_VOL_NAME_LEN];
    /* NOTE: [xyz]dim/lo/hi cannot be size_t until rel8 as they are
     * written out to disk via bu_vls_struct_print() as 32-bit ints.
     */
    uint32_t xdim;	/**< @brief X dimension */
    uint32_t ydim;	/**< @brief Y dimension */
    uint32_t zdim;	/**< @brief Z dimension */
    uint32_t lo;	/**< @brief Low threshold */
    uint32_t hi;	/**< @brief High threshold */
    vect_t cellsize;	/**< @brief ideal coords: size of each cell */
    mat_t mat;		/**< @brief convert local coords to model space */
    struct rt_db_internal *bip; /* @brief db object for data*/
#define RT_VOL_SRC_FILE 'f'
#define RT_VOL_SRC_OBJ 'o'
    char datasrc;/**< @brief which type of data source */
    /* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
    unsigned char *map;
};
#define RT_VOL_CK_MAGIC(_p) BU_CKMAG(_p, RT_VOL_INTERNAL_MAGIC, "rt_vol_internal")
/** @} */

/** @addtogroup rt_hf */
/** @{ */
/*
 * ID_HF
 */
struct rt_hf_internal {
    uint32_t magic;
    /* BEGIN USER SETTABLE VARIABLES */
    char cfile[128];	/**< @brief name of control file (optional) */
    char dfile[128];	/**< @brief name of data file */
    char fmt[8];	/**< @brief CV style file format descriptor */
    /* NOTE: w/n/shorts cannot be size_t until rel8 as they are
     * written out to disk via bu_vls_struct_print() as 32-bit ints.
     */
    uint32_t w;		/**< @brief # samples wide of data file.  ("i", "x") */
    uint32_t n;		/**< @brief nlines of data file.  ("j", "y") */
    uint32_t shorts;	/**< @brief !0 --> memory array is short, not float */
    fastf_t file2mm;	/**< @brief scale factor to cvt file units to mm */
    vect_t v;		/**< @brief origin of HT in model space */
    vect_t x;		/**< @brief model vect corresponding to "w" dir (will be unitized) */
    vect_t y;		/**< @brief model vect corresponding to "n" dir (will be unitized) */
    fastf_t xlen;	/**< @brief model len of HT rpp in "w" dir */
    fastf_t ylen;	/**< @brief model len of HT rpp in "n" dir */
    fastf_t zscale;	/**< @brief scale of data in ''up'' dir (after file2mm is applied) */
    /* END USER SETTABLE VARIABLES, BEGIN INTERNAL STUFF */
    struct bu_mapped_file *mp;	/**< @brief actual data */
};
#define RT_HF_CK_MAGIC(_p) BU_CKMAG(_p, RT_HF_INTERNAL_MAGIC, "rt_hf_internal")
/** @} */

/** @addtogroup rt_arbn */
/** @{ */
/*
 * ID_ARBN
 */
struct rt_arbn_internal {
    uint32_t magic;
    size_t neqn;
    plane_t *eqn;
};
#define RT_ARBN_CK_MAGIC(_p) BU_CKMAG(_p, RT_ARBN_INTERNAL_MAGIC, "rt_arbn_internal")
/** @} */

/** @addtogroup rt_pipe */
/** @{ */
/*
 * ID_PIPE
 */
struct rt_pipe_internal {
    uint32_t pipe_magic;
    struct bu_list pipe_segs_head;
    /* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
    int pipe_count;
};
#define RT_PIPE_CK_MAGIC(_p) BU_CKMAG(_p, RT_PIPE_INTERNAL_MAGIC, "rt_pipe_internal")
struct wdb_pipe_pnt {
    struct bu_list l;      /**< @brief  doubly linked list support */
    point_t pp_coord;      /**< @brief  "control" point for pipe solid */
    fastf_t pp_id;         /**< @brief  inner diam, <=0 if solid (wire) */
    fastf_t pp_od;         /**< @brief  pipe outer diam */
    fastf_t pp_bendradius; /**< @brief  bend radius to use for a bend at this point */
};
/** @} */


/** @addtogroup rt_part */
/** @{ */
/*
 * ID_PARTICLE
 */
struct rt_part_internal {
    uint32_t part_magic;
    point_t part_V;
    vect_t part_H;
    fastf_t part_vrad;
    fastf_t part_hrad;
    /* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
    int part_type;	/**< @brief sphere, cylinder, cone */
};
#define RT_PART_CK_MAGIC(_p) BU_CKMAG(_p, RT_PART_INTERNAL_MAGIC, "rt_part_internal")

#define RT_PARTICLE_TYPE_SPHERE 1
#define RT_PARTICLE_TYPE_CYLINDER 2
#define RT_PARTICLE_TYPE_CONE 3
/** @} */

/** @addtogroup rt_rpc */
/** @{ */
/*
 * ID_RPC
 */
struct rt_rpc_internal {
    uint32_t rpc_magic;
    point_t rpc_V;	/**< @brief rpc vertex */
    vect_t rpc_H;	/**< @brief height vector */
    vect_t rpc_B;	/**< @brief breadth vector */
    fastf_t rpc_r;	/**< @brief scalar half-width of rectangular face */
};
#define RT_RPC_CK_MAGIC(_p) BU_CKMAG(_p, RT_RPC_INTERNAL_MAGIC, "rt_rpc_internal")
/** @} */

/** @addtogroup rt_rhc */
/** @{ */
/*
 * ID_RHC
 */
struct rt_rhc_internal {
    uint32_t rhc_magic;
    point_t rhc_V;	/**< @brief rhc vertex */
    vect_t rhc_H;	/**< @brief height vector */
    vect_t rhc_B;	/**< @brief breadth vector */
    fastf_t rhc_r;	/**< @brief scalar half-width of rectangular face */
    fastf_t rhc_c;	/**< @brief dist from hyperbola to vertex of asymptotes */
};
#define RT_RHC_CK_MAGIC(_p) BU_CKMAG(_p, RT_RHC_INTERNAL_MAGIC, "rt_rhc_internal")
/** @} */

/** @addtogroup rt_epa */
/** @{ */
/*
 * ID_EPA
 */
struct rt_epa_internal {
    uint32_t epa_magic;
    point_t epa_V;	/**< @brief epa vertex */
    vect_t epa_H;	/**< @brief height vector */
    vect_t epa_Au;	/**< @brief unit vector along semi-major axis */
    fastf_t epa_r1;	/**< @brief scalar semi-major axis length */
    fastf_t epa_r2;	/**< @brief scalar semi-minor axis length */
};
#define RT_EPA_CK_MAGIC(_p) BU_CKMAG(_p, RT_EPA_INTERNAL_MAGIC, "rt_epa_internal")
/** @} */

/** @addtogroup rt_ehy */
/** @{ */
/*
 * ID_EHY
 */
struct rt_ehy_internal {
    uint32_t ehy_magic;
    point_t ehy_V;	/**< @brief ehy vertex */
    vect_t ehy_H;	/**< @brief height vector */
    vect_t ehy_Au;	/**< @brief unit vector along semi-major axis */
    fastf_t ehy_r1;	/**< @brief scalar semi-major axis length */
    fastf_t ehy_r2;	/**< @brief scalar semi-minor axis length */
    fastf_t ehy_c;	/**< @brief dist from hyperbola to vertex of asymptotes */
};
#define RT_EHY_CK_MAGIC(_p) BU_CKMAG(_p, RT_EHY_INTERNAL_MAGIC, "rt_ehy_internal")
/** @} */

/** @addtogroup rt_hyp */
/** @{ */
/*
 * ID_HYP
 */
struct rt_hyp_internal {
    uint32_t hyp_magic;
    point_t hyp_Vi;	/**< @brief hyp vertex */
    vect_t hyp_Hi;	/**< @brief full height vector */
    vect_t hyp_A;	/**< @brief semi-major axis */
    fastf_t hyp_b;	/**< @brief scalar semi-minor axis length */
    fastf_t hyp_bnr;/**< @brief ratio of minimum neck width to base width */
};
#define RT_HYP_CK_MAGIC(_p) BU_CKMAG(_p, RT_HYP_INTERNAL_MAGIC, "rt_hyp_internal")
/** @} */

/** @addtogroup rt_eto */
/** @{ */
/*
 * ID_ETO
 */
struct rt_eto_internal {
    uint32_t eto_magic;
    point_t eto_V;	/**< @brief eto vertex */
    vect_t eto_N;	/**< @brief vector normal to plane of torus */
    vect_t eto_C;	/**< @brief vector along semi-major axis of ellipse */
    fastf_t eto_r;	/**< @brief scalar radius of rotation */
    fastf_t eto_rd;	/**< @brief scalar length of semi-minor of ellipse */
};
#define RT_ETO_CK_MAGIC(_p) BU_CKMAG(_p, RT_ETO_INTERNAL_MAGIC, "rt_eto_internal")
/** @} */

/** @addtogroup rt_dsp */
/** @{ */
/*
 * ID_DSP
 */
#define DSP_NAME_LEN 128
struct rt_dsp_internal{
    uint32_t magic;
#define dsp_file dsp_name 	/**< @brief for backwards compatibility */
    struct bu_vls dsp_name;	/**< TODO: make this a pointer, name of data file */

    /* NOTE: dsp_xcnt/dsp_ycnt cannot be size_t until rel8 as they are
     * written out to disk via bu_vls_struct_print() as 32-bit ints.
     */
    uint32_t dsp_xcnt;		/**< @brief # samples in row of data */
    uint32_t dsp_ycnt;		/**< @brief # of columns in data */
    unsigned short dsp_smooth;	/**< @brief bool: surf normal interp */
#define DSP_CUT_DIR_ADAPT 'a'
#define DSP_CUT_DIR_llUR 'l'
#define DSP_CUT_DIR_ULlr 'L'
    unsigned char dsp_cuttype;	/**< @brief type of cut to make */

    mat_t dsp_mtos;		/**< @brief model to solid space */
    /* END OF USER SETTABLE VARIABLES, BEGIN INTERNAL STUFF */
    mat_t dsp_stom;		/**< @brief solid to model space
				 * computed from dsp_mtos */
    unsigned short *dsp_buf;	/**< @brief actual data */
    struct bu_mapped_file *dsp_mp;	/**< @brief mapped file for data */
    struct rt_db_internal *dsp_bip;	/**< @brief db object for data */
#define RT_DSP_SRC_V4_FILE '4'
#define RT_DSP_SRC_FILE 'f'
#define RT_DSP_SRC_OBJ 'o'
    char dsp_datasrc;		/**< @brief which type of data source */
};
#define RT_DSP_CK_MAGIC(_p) BU_CKMAG(_p, RT_DSP_INTERNAL_MAGIC, "rt_dsp_internal")
/** @} */


/** @addtogroup rt_sketch */
/** @{ */
/*
 * ID_SKETCH
 */

/**
 * container for a set of sketch segments
 */
struct rt_curve {
    size_t count;	/**< number of segments in this curve */
    int *reverse;	/**< array of boolean flags indicating if
			 * segment should be reversed
			 */
    void **segment;	/**< array of curve segment pointers */
};


/**
 * used by the sketch, solid of extrusion and the annotation primitive
 */
struct line_seg		/**< @brief line segment */
{
    uint32_t magic;
    int start, end;	/**< @brief indices into sketch's array of vertices */
};


struct carc_seg		/**< @brief circular arc segment */
{
    uint32_t magic;
    int start, end;	/**< @brief indices */
    fastf_t radius;	/**< @brief radius < 0.0 -> full circle with start point on
			 * circle and "end" at center */
    int center_is_left;	/**< @brief flag indicating where center of curvature is.
			 * If non-zero, then center is to left of vector
			 * from start to end */
    int orientation;	/**< @brief 0 -> ccw, !0 -> cw */
    int center;		/**< @brief index of vertex at center of arc (only used by rt_extrude_prep and rt_extrude_shot) */
};


struct nurb_seg		/**< @brief NURB curve segment */
{
    uint32_t magic;
    int order;		/**< @brief order of NURB curve (degree - 1) */
    int pt_type;	/**< @brief type of NURB curve */
    struct knot_vector k; /**< @brief knot vector for NURB curve */
    int c_size;		/**< @brief number of control points */
    int *ctl_points;	/**< @brief array of indices for control points */
    fastf_t *weights;	/**< @brief array of weights for control points (NULL if non_rational) */
};


struct bezier_seg	/**< @brief Bezier curve segment */
{
    uint32_t magic;
    int degree;		/**< @brief degree of curve (number of control points - 1) */
    int *ctl_points;	/**< @brief array of indices for control points */
};


#define SKETCH_NAME_LEN 16
struct rt_sketch_internal
{
    uint32_t magic;
    point_t V;			/**< default embedding of sketch */
    vect_t u_vec;		/**< unit vector 'u' component
				 * defining the sketch plane
				 */
    vect_t v_vec;		/**< unit vector 'v' component
				 * defining the sketch plane
				 */
    size_t vert_count;		/**< number of sketch vertices */
    point2d_t *verts;		/**< array of 2D vertices that may be
				 * used as endpoints, centers, or
				 * spline control points
				 */
    struct rt_curve curve;	/**< the curves of this sketch */
};
#define RT_SKETCH_CK_MAGIC(_p) BU_CKMAG(_p, RT_SKETCH_INTERNAL_MAGIC, "rt_sketch_internal")
/** @} */

/** @addtogroup rt_submodel */
/** @{ */
/*
 * ID_SUBMODEL
 */
struct rt_submodel_internal {
    uint32_t magic;
    struct bu_vls file;		/**< @brief  .g filename, 0-len --> this database. */
    struct bu_vls treetop;	/**< @brief one treetop only */
    int meth;			/**< @brief space partitioning method */
    /* other option flags (lazy prep, etc.)?? */
    /* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
    mat_t root2leaf;
    const struct db_i *dbip;
};
#define RT_SUBMODEL_CK_MAGIC(_p) BU_CKMAG(_p, RT_SUBMODEL_INTERNAL_MAGIC, "rt_submodel_internal")
/** @} */

/** @addtogroup rt_extrude */
/** @{ */
/*
 * ID_EXTRUDE
 */

struct rt_extrude_internal
{
    uint32_t magic;
    point_t V;		/**< @brief vertex, start and end point of loop to be extruded */
    vect_t h;		/**< @brief extrusion vector, may not be in (u_vec X v_vec) plane */
    vect_t u_vec;	/**< @brief vector in U parameter direction */
    vect_t v_vec;	/**< @brief vector in V parameter direction */
    int keypoint;	/**< @brief DEPRECATED (UNUSED): index of keypoint vertex */
    char *sketch_name;	/**< @brief name of sketch object that defines the curve to be extruded */
    struct rt_sketch_internal *skt;	/**< @brief pointer to referenced sketch */
};


/**
 * Note that the u_vec and v_vec are not unit vectors, their magnitude
 * and direction are used for scaling and rotation.
 */
#define RT_EXTRUDE_CK_MAGIC(_p) BU_CKMAG(_p, RT_EXTRUDE_INTERNAL_MAGIC, "rt_extrude_internal")
/** @} */

/** @addtogroup rt_revolve */
/** @{ */
/*
 * ID_REVOLVE
 */
struct rt_revolve_internal {
    uint32_t magic;
    point_t v3d;		/**< @brief vertex in 3d space */
    vect_t axis3d;		/**< @brief revolve axis in 3d space, y axis */

    point2d_t v2d;		/**< @brief vertex in 2d sketch */
    vect2d_t axis2d;		/**< @brief revolve axis in 2d sketch */

    vect_t r;			/**< @brief vector in start plane, x axis */
    fastf_t ang;		/**< @brief angle to revolve*/
    struct bu_vls sketch_name;	/**< @brief name of sketch */
    struct rt_sketch_internal *skt;	/**< @brief pointer to sketch */
};
#define RT_REVOLVE_CK_MAGIC(_p) BU_CKMAG(_p, RT_REVOLVE_INTERNAL_MAGIC, "rt_revolve_internal")
/** @} */

/** @addtogroup rt_cline */
/** @{ */
/*
 * ID_CLINE
 *
 * Implementation of FASTGEN CLINE element
 */

struct rt_cline_internal
{
    uint32_t magic;
    point_t v;
    vect_t h;
    fastf_t radius;
    fastf_t thickness; 	/**< @brief zero thickness means volume mode */
};
#define RT_CLINE_CK_MAGIC(_p) BU_CKMAG(_p, RT_CLINE_INTERNAL_MAGIC, "rt_cline_internal")
/** @} */

/** @addtogroup rt_bot */
/** @{ */
/*
 * ID_BOT
 */

struct rt_bot_internal
{
    uint32_t magic;
    unsigned char mode;
    unsigned char orientation;
    unsigned char bot_flags;	/**< @brief flags, (indicates surface
				 * normals available, for example)
				 */

    size_t num_faces;
    int *faces;			/**< @brief array of ints for faces
				 * [num_faces*3]
				 */

    size_t num_vertices;
    fastf_t *vertices;		/**< @brief array of floats for
				 * vertices [num_vertices*3]
				 */

    fastf_t *thickness;		/**< @brief array of plate mode
				 * thicknesses (corresponds to array
				 * of faces) NULL for modes
				 * RT_BOT_SURFACE and RT_BOT_SOLID.
				 */
    struct bu_bitv *face_mode;	/**< @brief a flag for each face
				 * indicating thickness is appended to
				 * hit point in ray direction (if bit
				 * is set), otherwise thickness is
				 * centered about hit point (NULL for
				 * modes RT_BOT_SURFACE and
				 * RT_BOT_SOLID).
				 */

    size_t num_normals;
    fastf_t *normals;		/**< @brief array of unit surface
				 * normals [num_normals*3]
				 */

    size_t num_face_normals;	/**< @brief current size of the
				 * face_normals array below (number of
				 * faces in the array)
				 */
    int *face_normals;		/**< @brief array of indices into the
				 * "normals" array, one per face
				 * vertex [num_face_normals*3]
				 */

    size_t num_uvs;		/**< @brief current size of the vertex
				 * uv mappings (corresponds to number
				 * of vertices)
				 */
    fastf_t *uvs;		/**< @brief array of floats for uv
				 * texturing coordinates [num_uvs*3]
				 */

    size_t num_face_uvs;	/**< @brief current size of the
				 * face_uvs array below (number of
				 * faces in the array)
				 */
    int *face_uvs;		/**< @brief array of indices into the
				 * "uvs" array, one per face vertex
				 * [num_uvs*3] */

    void *tie;	/* FIXME: blind casting. TIE needs to move from TIE_FUNC to XGLUE before this can not suck. */
};


struct rt_bot_list {
    struct bu_list l;
    struct rt_bot_internal *bot;
};


/* orientations for BOT */
#define RT_BOT_UNORIENTED 1	/**< @brief unoriented triangles */
#define RT_BOT_CCW 2 		/**< @brief oriented counter-clockwise */
#define RT_BOT_CW 3		/**< @brief oriented clockwise */

/* modes for BOT */
#define RT_BOT_SURFACE 1	/**< @brief triangles represent a surface (no volume) */
#define RT_BOT_SOLID 2 		/**< @brief triangles represent the boundary of a solid object */

/**
 * triangles represent plates. Thicknesses are specified in
 * "thickness" array, and face mode is specified in "face_mode" bit
 * vector.  This is the FASTGEN "plate" mode. Orientation is ignored.
 */
#define RT_BOT_PLATE 3

/**
 * same as plate mode, but LOS is set equal to face thickness, not the
 * thickness divided by the cosine of the obliquity angle.
 */
#define RT_BOT_PLATE_NOCOS 4

/* flags for bot_flags */
#define RT_BOT_HAS_SURFACE_NORMALS 0x01 /**< @brief Has surface normals at each face vertex */
#define RT_BOT_USE_NORMALS         0x02 /**< @brief Use the surface normals if they exist */
#define RT_BOT_USE_FLOATS          0x04 /**< @brief Use the single precision version of "tri_specific" during prep */
#define RT_BOT_HAS_TEXTURE_UVS     0x08 /**< @brief Has uv texture coordinates at each face vertex */
#define RT_BOT_HAS_UNUSED1         0x10 /**< @brief TBD */
#define RT_BOT_HAS_UNUSED2         0x20 /**< @brief TBD */
#define RT_BOT_HAS_UNUSED3         0x40 /**< @brief TBD */
#define RT_BOT_HAS_UNUSED4         0x80 /**< @brief WARNING: use this flag to denote more bits in the export serialization */

#define RT_BOT_CK_MAGIC(_p) BU_CKMAG(_p, RT_BOT_INTERNAL_MAGIC, "rt_bot_internal")
/** @} */


/** @addtogroup rt_pnt */
/** @{ */
/**
 * ID_PNTS
 *
 * Points are represented to a structure that contains exactly the
 * data that it needs for that 'type' of point.  The reason this was
 * done over using something like a union was to fully optimize memory
 * usage so that the maximum number of points could be stored without
 * resorting to out-of-core techniques.  A union is at least the size
 * of the largest type and would have wasted memory.
 *
 * By using this data-driven approach of type identification, it does
 * result in needing to have a switching table for all supported types
 * in order to access data.  This could be avoided by storing them as
 * multiple lists (wasting a few bytes for unused pointers) but is
 * left as an exercise to the reader.
 */

typedef enum {
    RT_PNT_TYPE_PNT = 0,
    RT_PNT_TYPE_COL = 0+1,
    RT_PNT_TYPE_SCA = 0+2,
    RT_PNT_TYPE_NRM = 0+4,
    RT_PNT_TYPE_COL_SCA = 0+1+2,
    RT_PNT_TYPE_COL_NRM = 0+1+4,
    RT_PNT_TYPE_SCA_NRM = 0+2+4,
    RT_PNT_TYPE_COL_SCA_NRM = 0+1+2+4,
    RT_PNT_UNKNOWN = 8
} rt_pnt_type;

struct pnt {
    struct bu_list l;
    point_t v;
};
struct pnt_color {
    struct bu_list l;
    point_t v;
    struct bu_color c;
};
struct pnt_scale {
    struct bu_list l;
    point_t v;
    fastf_t s;
};
struct pnt_normal {
    struct bu_list l;
    point_t v;
    vect_t n;
};
struct pnt_color_scale {
    struct bu_list l;
    point_t v;
    struct bu_color c;
    fastf_t s;
};
struct pnt_color_normal {
    struct bu_list l;
    point_t v;
    struct bu_color c;
    vect_t n;
};
struct pnt_scale_normal {
    struct bu_list l;
    point_t v;
    fastf_t s;
    vect_t n;
};
struct pnt_color_scale_normal {
    struct bu_list l;
    point_t v;
    struct bu_color c;
    fastf_t s;
    vect_t n;
};


struct rt_pnts_internal {
    uint32_t magic;
    double scale;
    rt_pnt_type type;
    unsigned long count;
    void *point;
};
#define RT_PNTS_CK_MAGIC(_p) BU_CKMAG(_p, RT_PNTS_INTERNAL_MAGIC, "rt_pnts_internal")
/** @} */

/** @addtogroup rt_annotation */
/** @{ */
/*
 * ID_ANNO
 *
 * Annotations are used to provide labels in-scene when viewing
 * geometry. Leaders connect labels to geometry objects or fixed
 * points in space.
 *
 */

/**
 * container for the annotation primitive
 */
struct rt_ant {
    size_t count;			/**< @brief number of segments in the annotation */
    int *reverse;			/**< array of boolean flags indicating if the
					 * segment should be reversed */
    void **segments;			/**< @brief array of annotation segment pointer */
};

/**
 * text labels used by the annotation primitive
 */
struct txt_seg {
    uint32_t magic;
    int ref_pt;				/** reference point */
    int rel_pos;			/** flag describing position relative to ref_pt */
    struct bu_vls label;
};

/**
 * placement flags
 */
#define RT_TXT_POS_BL 1
#define RT_TXT_POS_BC 2
#define RT_TXT_POS_BR 3
#define RT_TXT_POS_ML 4
#define RT_TXT_POS_MC 5
#define RT_TXT_POS_MR 6
#define RT_TXT_POS_TL 7
#define RT_TXT_POS_TC 8
#define RT_TXT_POS_TR 9

/**
 * set a position flag to the corresponding placement value given
 * numeric settings (1=left/top, 2=middle/center, 3=right/bottom).
 */
RT_EXPORT int rt_txt_pos_flag(int *pos_flag, int horizontal, int vertical);

/**
 * internal representation of an annotation object
 */
struct rt_annot_internal
{
    uint32_t magic;
    point_t V;				/**< @brief vertex, maps to the origin in the 2D system */
    size_t vert_count;			/**< @brief number of vertices */
    point2d_t *verts;			/**< @brief array of vertices that serve as control points */
    struct rt_ant ant;			/**< @brief segments in the annotation */
};



#define RT_ANNOT_CK_MAGIC(_p) BU_CKMAG(_p, RT_ANNOT_INTERNAL_MAGIC, "rt_annot_internal")
/** @} */

/*
 * ID_DATUM
 *
 * Datums provide basic geometric reference entities such as reference
 * frames, planes, lines, and/or points.  A datum reference frame is
 * generally a Cartesian coordinate system that consists of three
 * mutually perpendicular axes, three mutually perpendicular base
 * planes, and a point representing the origin.
 *
 * Planes distinguished by (!ZERO(w)) utilize 'pnt', 'dir', and 'w' to
 * define an unoriented plane.  The plane is defined by the vector
 * from 'pnt' in 'dir' direction multiplied by the 'w' scalar value.
 *
 * Lines distinguished by (MAGNITUDE(dir) > 0.0 && ZERO(w)) utilize
 * 'pnt' and 'dir' to define a line.
 *
 * Points only use the 'pnt' field to define an unoriented point.
 *
 * Datum references are stored as a simple NULL-terminated linked list
 * manually accessed through the 'next' pointer.
 *
 * This characterization is derived from ASME Y14.5M
 *
 * TODO:
 * - edsol needs to do more than move the first datum
 * - tedit is untested
 * - wdb needs to support more than one datum
 * - validate the datum during prep
 * - add CK validation checks to all loops
 */
struct rt_datum_internal
{
    uint32_t magic;

    point_t pnt;
    vect_t dir;
    fastf_t w;

    struct rt_datum_internal *next;
};

/**
 * validation macro to make sure an rt_datum_internal has the proper magic identifier.
 */
#define RT_DATUM_CK_MAGIC(_p) BU_CKMAG(_p, RT_DATUM_INTERNAL_MAGIC, "rt_datum_internal")


/** @addtogroup rt_hrt */
/** @{ */
struct rt_hrt_internal
{
    uint32_t hrt_magic;
    point_t v;		/**< @brief center point */
    vect_t xdir;	/**< @brief unit vector in x direction */
    vect_t ydir;     	/**< @brief unit vector in y direction */
    vect_t zdir;      	/**< @brief unit vector in z direction */
    fastf_t d;          /**< @brief distance to cusps */
};
#define RT_HRT_CK_MAGIC(_p) BU_CKMAG(_p, RT_HRT_INTERNAL_MAGIC, "rt_hrt_internal")


/** @addtogroup rt_script */
/** @{ */
struct rt_script_internal {
    uint32_t script_magic;
    struct bu_vls s_type;
};
#define RT_SCRIPT_CK_MAGIC(_p) BU_CKMAG(_p, RT_SCRIPT_INTERNAL_MAGIC, "rt_script_internal")
/** @} */



__END_DECLS

#endif /* RT_GEOM_H */

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
