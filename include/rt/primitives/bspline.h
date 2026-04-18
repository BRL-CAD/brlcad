/*                     B S P L I N E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2025 United States Government as represented by
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
/** @addtogroup rt_nurb */
/** @{ */
/** @file rt/primitives/bspline.h */

#ifndef RT_PRIMITIVES_BSPLINE_H
#define RT_PRIMITIVES_BSPLINE_H

#include "common.h"
#include "rt/defines.h"

__BEGIN_DECLS

/**
 * Convert a BSPLINE (ID_BSPLINE) rt_db_internal to a BREP (ID_BREP)
 * rt_db_internal in-place.
 *
 * The caller must already hold a loaded ID_BSPLINE internal (e.g. from
 * rt_db_get_internal()).  On success the internal is freed and replaced
 * with an equivalent ID_BREP internal; the caller owns the result and
 * must eventually call rt_db_free_internal().
 *
 * Returns 0 on success, -1 on failure (the original internal is
 * unchanged on failure).
 */
RT_EXPORT extern int rt_nurb_to_brep(struct rt_db_internal *ip);

/** @} */

__END_DECLS

#endif /* RT_PRIMITIVES_BSPLINE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
