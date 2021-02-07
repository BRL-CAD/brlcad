/*                        P G . H
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
/** @addtogroup rt_pg */
/** @{ */
/** @file rt/primitives/pg.h */

#ifndef RT_PRIMITIVES_PG_H
#define RT_PRIMITIVES_PG_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bn/tol.h"
#include "rt/defines.h"

__BEGIN_DECLS

RT_EXPORT extern int rt_pg_to_bot(struct rt_db_internal *ip,
				  const struct bn_tol *tol,
				  struct resource *resp0);
RT_EXPORT extern int rt_pg_plot(struct bu_list          *vhead,
				struct rt_db_internal   *ip,
				const struct bg_tess_tol *ttol,
				const struct bn_tol     *tol,
				const struct rt_view_info *info);
RT_EXPORT extern int rt_pg_plot_poly(struct bu_list             *vhead,
				     struct rt_db_internal      *ip,
				     const struct bg_tess_tol   *ttol,
				     const struct bn_tol        *tol);

/** @} */

__END_DECLS

#endif /* RT_PRIMITIVES_PG_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
