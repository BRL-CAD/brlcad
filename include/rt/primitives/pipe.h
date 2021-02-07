/*                        P I P E . H
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
/** @addtogroup rt_pipe */
/** @{ */
/** @file rt/primitives/pipe.h */

#ifndef RT_PRIMITIVES_PIPE_H
#define RT_PRIMITIVES_PIPE_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

RT_EXPORT extern void rt_vls_pipe_pnt(struct bu_vls *vp,
				    int seg_no,
				    const struct rt_db_internal *ip,
				    double mm2local);
RT_EXPORT extern void rt_pipe_pnt_print(const struct wdb_pipe_pnt *pipe_pnt, double mm2local);
RT_EXPORT extern int rt_pipe_ck(const struct bu_list *headp);

/** @} */

__END_DECLS

#endif /* RT_PRIMITIVES_PIPE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
