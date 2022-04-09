/*                      V I E W _ S E T S . H
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
/** @addtogroup bv_util
 *
 */
/** @{ */
/** @file bv/view_sets.h */

#ifndef BV_VIEW_SETS_H
#define BV_VIEW_SETS_H

#include "common.h"
#include "bv/defines.h"

__BEGIN_DECLS

BV_EXPORT void
bv_set_init(struct bview_set *s);

BV_EXPORT void
bv_set_free(struct bview_set *s);

BV_EXPORT void
bv_set_add_view(struct bview_set *s, struct bview *v);

BV_EXPORT void
bv_set_rm_view(struct bview_set *s, struct bview *v);

BV_EXPORT struct bu_ptbl *
bv_set_views(struct bview_set *s);

BV_EXPORT struct bview *
bv_set_find_view(struct bview_set *s, const char *vname);

// Expose free_scene_obj for older codes
BV_EXPORT struct bv_scene_obj *
bv_set_fsos(struct bview_set *s);


__END_DECLS

/** @} */

#endif /* BV_VIEW_SETS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
