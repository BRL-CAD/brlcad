/*                      C A L C . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @addtogroup raytrace */
/** @{ */
/** @file rt/calc.h
 * @brief
 * In memory format for non-geometry objects in BRL-CAD databases.
 */

#ifndef RT_CALC_H
#define RT_CALC_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "vmath.h"
#include "bu/vls.h"
#include "bn/poly.h"
#include "rt/defines.h"

__BEGIN_DECLS

/* apply a matrix transformation */
/**
 * apply a matrix transformation to a given input object, setting the
 * resultant transformed object as the output solid.  if freeflag is
 * set, the input object will be released.
 *
 * returns zero if matrix transform was applied, non-zero on failure.
 */
RT_EXPORT extern int rt_matrix_transform(struct rt_db_internal *output, const mat_t matrix, struct rt_db_internal *input, int free_input, struct db_i *dbip, struct resource *resource);

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
 * Compute the intersections of a ray with a rectangular
 * parallelepiped (RPP) that has faces parallel to the coordinate
 * planes
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

/* Find the bounding box given a struct rt_db_internal : bbox.c */

/**
 *
 * Calculate the bounding RPP of the internal format passed in 'ip'.
 * The bounding RPP is returned in rpp_min and rpp_max in mm FIXME:
 * This function needs to be modified to eliminate the rt_gettree()
 * call and the related parameters. In that case calling code needs to
 * call another function before calling this function That function
 * must create a union tree with tr_a.tu_op=OP_SOLID. It can look as
 * follows : union tree * rt_comb_tree(const struct db_i *dbip, const
 * struct rt_db_internal *ip). The tree is set in the struct
 * rt_db_internal * ip argument.  Once a suitable tree is set in the
 * ip, then this function can be called with the struct rt_db_internal
 * * to return the BB properly without getting stuck during tree
 * traversal in rt_bound_tree()
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

/**
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
RT_EXPORT extern int rt_shader_mat(mat_t                        model_to_shader,        /* result */
				   const struct rt_i    *rtip,
				   const struct region  *rp,
				   point_t                      p_min,  /* input/output: shader/region min point */
				   point_t                      p_max,  /* input/output: shader/region max point */
				   struct resource              *resp);

/* mirror.c */
RT_EXPORT extern struct rt_db_internal *rt_mirror(struct db_i *dpip,
						  struct rt_db_internal *ip,
						  point_t mirror_pt,
						  vect_t mirror_dir,
						  struct resource *resp);


RT_EXPORT extern void rt_plot_all_bboxes(FILE *fp,
					 struct rt_i *rtip);
RT_EXPORT extern void rt_plot_all_solids(FILE           *fp,
					 struct rt_i    *rtip,
					 struct resource        *resp);


/* pr.c */
/* TODO - do these belong in libbn? */
RT_EXPORT extern void rt_pr_fallback_angle(struct bu_vls *str,
					   const char *prefix,
					   const double angles[5]);
RT_EXPORT extern void rt_find_fallback_angle(double angles[5],
					     const vect_t vec);
RT_EXPORT extern void rt_pr_tol(const struct bn_tol *tol);


/**
 * Find the roots of a polynomial
 *
 * TODO - should this be moved to libbn?
 *
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



__END_DECLS

#endif /* RT_CALC_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
