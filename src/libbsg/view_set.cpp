/*                   V I E W _ S E T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
/** @file view_set.cpp
 *
 * Utility functions for operating on BRL-CAD view sets
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "bsg/defines.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg/view_set.h"
#include "./bsg_private.h"
#include "node_private.h"
#include "scene_store_private.h"
#include "view_state_private.h"

void
bsg_set_init(struct bsg_view_set *s)
{
    BU_GET(s->i, struct bsg_view_set_internal);
    BU_PTBL_INIT(&s->i->views);
    bsg_scene_view_set_store_init(s->i);
    BU_ALLOC(s->settings, struct bsg_view_settings);
    bsg_settings_init(s->settings);
}

void
bsg_set_free(struct bsg_view_set *s)
{
    if (s->i) {
	bu_ptbl_free(&s->i->views);
	bsg_scene_view_set_store_free(s->i);
	BU_PUT(s->i, struct bsg_view_set_internal);
    }
    if (s->settings) {
	if (s->settings->gv_selected)
	    bsg_selection_destroy(s->settings->gv_selected);
	BU_PUT(s->settings, struct bsg_view_settings);
	s->settings = NULL;
    }
}

void
bsg_set_add_view(struct bsg_view_set *s, struct bsg_view *v){
    if (!s || !v)
	return;

    bu_ptbl_ins_unique(&s->i->views, (long *)v);

    v->vset = s;

    // By default, when we add a view to a set it is no longer independent;
    // remove any existing independent scope from the BSG tree.
    bsg_view_independent_scope_destroy(v);
}

void
bsg_set_rm_view(struct bsg_view_set *s, struct bsg_view *v){
    if (!s)
	return;

    if (!v) {
	bu_ptbl_reset(&s->i->views);
	return;
    }

    bu_ptbl_rm(&s->i->views, (long int *)v);

    v->vset = NULL;

    // By default, when we remove a view from a set it is independent;
    // create an independent scope in the BSG tree when possible.
    bsg_view_independent_scope_ref(v, 1 /*create*/);
}


struct bu_ptbl *
bsg_set_views(struct bsg_view_set *s){
    if (!s)
	return NULL;

    return &s->i->views;
}

struct bsg_view *
bsg_set_find_view(struct bsg_view_set *s, const char *vname)
{
    struct bsg_view *v = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->i->views); i++) {
	struct bsg_view *tv = (struct bsg_view *)BU_PTBL_GET(&s->i->views, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&tv->gv_name), vname)) {
	    v = tv;
	    break;
	}
    }

    return v;
}

void *
bsg_set_fsos(struct bsg_view_set *s)
{
    return bsg_scene_view_set_recycle_pool(s ? s->i : NULL);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
