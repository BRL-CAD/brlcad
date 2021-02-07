/*                         H I T . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @addtogroup rt_hit
 * @brief Information about where a ray hits the surface.
 *
 * The hit structure contains information about an
 * individual boundary/ray intersection.
 *
 * This is the lowest level of intersection information returned by
 * LIBRT's intersection logic.  Generally, multiple hits are used
 * by higher level routines to construct segments (per shape)
 * and partitions (boolean evaluations of multiple segments which
 * constitute the actual portion of the ray deemed to have passed
 * through solid geometry.)
 *
 */
/** @{ */
/** @file rt/hit.h */

#ifndef RT_HIT_H
#define RT_HIT_H

#include "common.h"
#include "vmath.h"
#include "bu/magic.h"
#include "bu/vls.h"
#include "rt/defines.h"
#include "rt/xray.h"

__BEGIN_DECLS

/**
 * @brief
 * Information about where a ray hits the surface.
 *
 *
 * Important Note:  Surface Normals always point OUT of a solid.
 *
 * DEPRECATED: The hit_point and hit_normal elements will be removed
 * from this structure, so as to separate the concept of the solid's
 * normal at the hit point from the post-boolean normal at the hit
 * point.
 */
struct hit {
    uint32_t            hit_magic;
    fastf_t             hit_dist;       /**< @brief dist from r_pt to hit_point */
    point_t             hit_point;      /**< @brief DEPRECATED: Intersection point, use VJOIN1 hit_dist */
    vect_t              hit_normal;     /**< @brief DEPRECATED: Surface Normal at hit_point, use RT_HIT_NORMAL */
    vect_t              hit_vpriv;      /**< @brief PRIVATE vector for xxx_*() */
    void *              hit_private;    /**< @brief PRIVATE handle for xxx_shot() */
    int                 hit_surfno;     /**< @brief solid-specific surface indicator */
    struct xray *       hit_rayp;       /**< @brief pointer to defining ray */
};
#define HIT_NULL        ((struct hit *)0)
#define RT_CK_HIT(_p) BU_CKMAG(_p, RT_HIT_MAGIC, "struct hit")
#define RT_HIT_INIT_ZERO { RT_HIT_MAGIC, 0.0, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, NULL, 0, NULL }

/**
 * Compute normal into (_hitp)->hit_normal.
 *
 * Set flip-flag accordingly depending on boolean logic, as one hit
 * may be shared between multiple partitions with different flip
 * status.
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
    vect_t      crv_pdir;       /**< @brief Principle direction */
    fastf_t     crv_c1;         /**< @brief curvature in principle dir */
    fastf_t     crv_c2;         /**< @brief curvature in other direction */
};
#define CURVE_NULL      ((struct curvature *)0)
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
    fastf_t uv_u;       /**< @brief Range 0..1 */
    fastf_t uv_v;       /**< @brief Range 0..1 */
    fastf_t uv_du;      /**< @brief delta in u */
    fastf_t uv_dv;      /**< @brief delta in v */
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

/* Print a bit vector */
RT_EXPORT extern void rt_pr_hit(const char *str,
				const struct hit *hitp);
RT_EXPORT extern void rt_pr_hit_vls(struct bu_vls *v,
				    const char *str,
				    const struct hit *hitp);
RT_EXPORT extern void rt_pr_hitarray_vls(struct bu_vls *v,
					 const char *str,
					 const struct hit *hitp,
					 int count);

__END_DECLS

#endif /* RT_HIT_H */
/** @} */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
