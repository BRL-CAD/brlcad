/*                       D M / V L I S T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @addtogroup libdm */
/** @{ */
/** @file dm/vlist.h
 *
 * Legacy display-manager vlist drawing API.
 *
 * New drawing code should publish typed BSG render records and render them
 * through dm_backend_draw_item().  This header is isolated so modern dm.h
 * clients do not inherit the legacy bsg_vlist dependency.
 */

#ifndef DM_VLIST_H
#define DM_VLIST_H

#include "common.h"

#include "bsg/vlist.h"
#include "dm/defines.h"

__BEGIN_DECLS

struct dm;

DM_EXPORT extern int dm_draw(struct dm *dmp,
			     bsg_vlist *(*callback)(void *),
			     void **data);

__END_DECLS

#endif /* DM_VLIST_H */

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
