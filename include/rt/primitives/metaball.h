/*                        M E T A B A L L . H
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
/** @addtogroup rt_metaball */
/** @{ */
/** @file rt/primitives/metaball.h */

#ifndef RT_PRIMITIVES_METABALL_H
#define RT_PRIMITIVES_METABALL_H

#include "common.h"
#include "vmath.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

struct rt_metaball_internal;
RT_EXPORT extern void rt_vls_metaball_pnt(struct bu_vls *vp,
					const int pt_no,
					const struct rt_db_internal *ip,
					const double mm2local);
RT_EXPORT extern void rt_metaball_pnt_print(const struct wdb_metaball_pnt *metaball, double mm2local);
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

/** @} */

__END_DECLS

#endif /* RT_PRIMITIVES_METABALL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
