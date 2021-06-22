/*                        S K E T C H . H
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
/** @addtogroup rt_sketch */
/** @{ */
/** @file rt/primitives/sketch.h */

#ifndef RT_PRIMITIVES_SKETCH_H
#define RT_PRIMITIVES_SKETCH_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bv/defines.h"
#include "rt/defines.h"
#include "rt/directory.h"
#include "rt/db_instance.h"

__BEGIN_DECLS

RT_EXPORT extern int curve_to_vlist(struct bu_list              *vlfree,
	                            struct bu_list              *vhead,
				    const struct bg_tess_tol    *ttol,
				    point_t                     V,
				    vect_t                      u_vec,
				    vect_t                      v_vec,
				    struct rt_sketch_internal *sketch_ip,
				    struct rt_curve             *crv);

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

RT_EXPORT extern struct bv_scene_obj *
db_sketch_to_scene_obj(const char *sname, struct db_i *dbip, struct directory *dp, struct bview *sv, struct bv_scene_obj *free_scene_obj);

RT_EXPORT extern struct directory *
db_scene_obj_to_sketch(struct db_i *dbip, const char *sname, struct bv_scene_obj *s);

__END_DECLS

/** @} */

#endif /* RT_PRIMITIVES_SKETCH_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
