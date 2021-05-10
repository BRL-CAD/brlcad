/*                        B R E P . H
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
/** @addtogroup rt_brep */
/** @{ */
/** @file rt/primitives/brep.h */

#ifndef RT_PRIMITIVES_BREP_H
#define RT_PRIMITIVES_BREP_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "rt/defines.h"

__BEGIN_DECLS

/* BREP drawing routines */
RT_EXPORT extern int rt_brep_plot(struct bu_list                *vhead,
				  struct rt_db_internal          *ip,
				  const struct bg_tess_tol       *ttol,
				  const struct bn_tol            *tol,
				  const struct bview *info);
RT_EXPORT extern int rt_brep_plot_poly(struct bu_list           *vhead,
				       const struct db_full_path *pathp,
				       struct rt_db_internal     *ip,
				       const struct bg_tess_tol  *ttol,
				       const struct bn_tol       *tol,
				       const struct bview *info);
/* BREP validity test */
#define RT_BREP_OPENNURBS    0x1    /**< @brief OpenNURBS tests (default)*/
#define RT_BREP_UV_PARAM     0x2    /**< @brief sanity checks for UV parameterization bounds */
#define RT_BREP_EDGE_CRACK   0x4    /**< @brief check for trim geometry at edges that isn't closely aligned */
RT_EXPORT extern int rt_brep_valid(struct bu_vls *log, struct rt_db_internal *ip, int flags);

/* BREP function to make sure all curve and surface parameterizations range
 * from either 0 to their 3D length or from 0 to pmax (if pmax is nonzero.)
 */
RT_EXPORT extern int rt_brep_normalize(struct rt_db_internal *ip, double pmax);


/* Report if the brep is a plate mode object: returns 1 if plate mode, otherwise 0.
 * (Invalid openNURBS objects are not considered plate mode) */
RT_EXPORT extern int rt_brep_plate_mode(const struct rt_db_internal *ip);

/* Get plate mode settings if  brep is a plate mode object. Note: default
 * returns are 0 regardless of the object type - use rt_brep_plate_mode to test
 * if an object is or isn't plate mode*/
RT_EXPORT extern void rt_brep_plate_mode_getvals(double *pthickness, int *nocos, const struct rt_db_internal *ip);

/** @} */

__END_DECLS

#endif /* RT_PRIMITIVES_BREP_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
