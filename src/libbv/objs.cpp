/*                      O B J S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file util.cpp
 *
 * Utility functions for operating on BRL-CAD views
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bu/vls.h"
#include "bn/mat.h"
#include "bg/plane.h"
#include "bv/defines.h"
#include "bv/objs.h"
#include "bv/snap.h"
#include "bv/util.h"
#include "bv/vlist.h"
#include "./bv_private.h"

int
bv_obj_settings_sync(struct bv_obj_settings *dest, struct bv_obj_settings *src)
{
    int ret = 0;
    if (!dest || !src)
	return ret;

    if (dest->s_line_width != src->s_line_width) {
	dest->s_line_width = src->s_line_width;
	ret = 1;
    }
    if (!NEAR_EQUAL(dest->s_arrow_tip_length, src->s_arrow_tip_length, SMALL_FASTF)) {
	dest->s_arrow_tip_length = src->s_arrow_tip_length;
	ret = 1;
    }
    if (!NEAR_EQUAL(dest->s_arrow_tip_width, src->s_arrow_tip_width, SMALL_FASTF)) {
	dest->s_arrow_tip_width = src->s_arrow_tip_width;
	ret = 1;
    }
    if (!NEAR_EQUAL(dest->transparency, src->transparency, SMALL_FASTF)) {
	dest->transparency = src->transparency;
	ret = 1;
    }
    if (dest->s_dmode != src->s_dmode) {
	dest->s_dmode = src->s_dmode;
	ret = 1;
    }
    if (dest->color_override != src->color_override) {
	dest->color_override = src->color_override;
	ret = 1;
    }
    if (!VNEAR_EQUAL(dest->color, src->color, SMALL_FASTF)) {
	VMOVE(dest->color, src->color);
	ret = 1;
    }
    if (dest->draw_solid_lines_only != src->draw_solid_lines_only) {
	dest->draw_solid_lines_only = src->draw_solid_lines_only;
	ret = 1;
    }
    if (dest->draw_non_subtract_only != src->draw_non_subtract_only) {
	dest->draw_non_subtract_only = src->draw_non_subtract_only;
	ret = 1;
    }

    return ret;
}

struct bv_obj_pool *
bv_obj_pool_create()
{
    struct bv_obj_pool *p;
    BU_GET(p, struct bv_obj_pool);
    BU_GET(p->i, struct bv_obj_pool_internal);
    BU_LIST_INIT(&p->i->head.l);
    BU_LIST_INIT(&p->i->vlfree);
    return p;
}

void
bv_obj_pool_destroy(struct bv_obj_pool *p)
{
    if (!p)
	return;

    /* Free scene objects */
    struct bv_scene_obj *sp = BU_LIST_NEXT(bv_scene_obj, &p->i->head.l);
    while (BU_LIST_NOT_HEAD(sp, &p->i->head.l)) {
	struct bv_scene_obj *nsp = BU_LIST_PNEXT(bv_scene_obj, sp);
	BU_LIST_DEQUEUE(&((sp)->l));
	sp->obj_pool = NULL;
	bv_obj_put(sp);
	sp = nsp;
    }
    BU_PUT(p->i, struct bv_obj_pool_internal);
    BU_PUT(p, struct bv_obj_pool);

    /* Free vlists */
    struct bu_list *vp;
    while (BU_LIST_WHILE(vp, bu_list, &p->i->vlfree)) {
	BU_LIST_DEQUEUE(vp);
	struct bv_vlist *pv = (struct bv_vlist *)vp;
	BU_FREE(pv, struct bv_vlist);
    }
}

struct bv_scene_obj *
bv_obj_pool_get(struct bv_obj_pool *p)
{
    if (UNLIKELY(!p))
	return NULL;

    struct bv_scene_obj *s = BU_LIST_FIRST(bv_scene_obj, &p->i->head.l);
    if (BU_LIST_IS_HEAD(&s->l, &p->i->head.l)) {
	s = bv_obj_get(NULL);
    } else {
	BU_LIST_DEQUEUE(&(s->l));
    }

    s->obj_pool = p;
    s->vlfree = &p->i->vlfree;
    return s;
}

void
bv_obj_pool_put(struct bv_obj_pool *p, struct bv_scene_obj *s)
{
    if (UNLIKELY(!p || !s))
	return;

    s->obj_pool = NULL;
    bv_obj_reset(s);
    BU_LIST_APPEND(&s->l, &(p->i->head.l));
}

struct bv_scene_obj *
bv_obj_get(struct bv_obj_pool *p)
{
    struct bv_scene_obj *s = NULL;
    if (!p) {
	BU_ALLOC(s, struct bv_scene_obj);
	s->l.magic = BV_SOBJ_MAGIC;

	// Zero out callback pointers
	s->s_type_flags = 0;
	s->s_free_callback = NULL;
	s->s_dlist_free_callback = NULL;

	// Use reset to do most of the initialization
	bv_obj_reset(s);
    } else {
	s = bv_obj_pool_get(p);
    }

    return s;
}

struct bv_scene_obj *
bv_obj_get_child(struct bv_scene_obj *sp)
{
    if (!sp)
	return NULL;

    bv_log(1, "bv_obj_create_child %s(%s)", bu_vls_cstr(&sp->s_name), bu_vls_cstr(&sp->s_v->gv_name));

    // Children use their parent's info
    struct bv_scene_obj *s = bv_obj_pool_get(sp->obj_pool);
    s->s_v = sp->s_v;
    s->dp = sp->dp;

    // Assign child name
    bu_vls_sprintf(&s->s_name, "child:%s:%zd", bu_vls_cstr(&sp->s_name), BU_PTBL_LEN(&sp->children));

    // Tell sp about the child
    bu_ptbl_ins(&sp->children, (long *)s);

    return s;
}

struct bv_scene_obj *
bv_obj_find_child(struct bv_scene_obj *s, const char *vname)
{
    if (!s || !vname || !BU_PTBL_IS_INITIALIZED(&s->children))
	return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	if (!bu_path_match(vname, bu_vls_cstr(&s_c->s_name), 0))
	    return s_c;
    }

    return NULL;
}

void
bv_obj_sync(struct bv_scene_obj *dest, struct bv_scene_obj *src)
{
    bv_obj_settings_sync(dest->s_os, src->s_os);
    VMOVE(dest->s_center, src->s_center);
    VMOVE(dest->s_color, src->s_color);
    VMOVE(dest->bmin, src->bmin);
    VMOVE(dest->bmax, src->bmax);
    MAT_COPY(dest->s_mat, src->s_mat);
    dest->s_size = src->s_size;
    dest->s_soldash = src->s_soldash;
    dest->s_arrow = src->s_arrow;
    dest->adaptive_wireframe = src->adaptive_wireframe;
    dest->view_scale = src->view_scale;
    dest->bot_threshold = src->bot_threshold;
    dest->curve_scale = src->curve_scale;
    dest->point_scale = src->point_scale;
}

void
bv_obj_reset(struct bv_scene_obj *s)
{
    // handle children
    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	    bv_obj_put(s_c);
	}
    } else {
	BU_PTBL_INIT(&s->children);
    }
    bu_ptbl_reset(&s->children);

    // If we have a callback for the internal data, use it
    if (s->s_free_callback)
	(*s->s_free_callback)(s);
    s->s_free_callback = NULL;

    // If we have a callback for the display list data, use it
    if (s->s_dlist_free_callback)
	(*s->s_dlist_free_callback)(s);
    s->s_dlist_free_callback = NULL;

    // If we have a label, do the label freeing steps
    // TODO - this should be using the free callback rather
    // than special casing...
    if (s->s_type_flags & BV_LABELS) {
	struct bv_label *la = (struct bv_label *)s->s_i_data;
	bu_vls_free(&la->label);
	BU_PUT(la, struct bv_label);
    }

    // Free vlist data.  If we have an associated vlfree list for
    // reuse use that, otherwise free it for real.
    if (BU_LIST_IS_INITIALIZED(&s->s_vlist)) {
	if (s->vlfree) {
	    BV_FREE_VLIST(s->vlfree, &s->s_vlist);
	} else {
	    struct bu_list *p;
	    while (BU_LIST_WHILE(p, bu_list, &s->s_vlist)) {
		BU_LIST_DEQUEUE(p);
		struct bv_vlist *pv = (struct bv_vlist *)p;
		BU_FREE(pv, struct bv_vlist);
	    }

	}
    }
    BU_LIST_INIT(&(s->s_vlist));

    if (!BU_VLS_IS_INITIALIZED(&s->s_name))
	BU_VLS_INIT(&s->s_name);
    bu_vls_trunc(&s->s_name, 0);

    struct bv_obj_settings defaults = BV_OBJ_SETTINGS_INIT;
    bv_obj_settings_sync(&s->s_local_os, &defaults);
    s->s_os = &s->s_local_os;
    s->s_override_child_settings = 0;

    MAT_IDN(s->s_mat);
    VSET(s->s_color, 255, 0, 0);
    VSETALL(s->bmax, -INFINITY);
    VSETALL(s->bmin, INFINITY);
    VSETALL(s->s_center, 0);
    s->adaptive_wireframe = 0;
    s->bot_threshold = 0;
    s->csg_obj = 0;
    s->current = 0;
    s->curve_scale = 0;
    s->dp = NULL;
    s->draw_data = NULL;
    s->have_bbox = 0;
    s->mesh_obj = 0;
    s->point_scale = 0;
    s->s_arrow = 0;
    s->s_csize = 0;
    s->s_flag = UP;
    s->s_force_draw = 0;
    s->s_i_data = NULL;
    s->s_iflag = DOWN;
    s->s_path = NULL;
    s->s_size = 0;
    s->s_soldash = 0;
    s->s_update_callback = NULL;
    s->s_v = NULL;
    s->view_scale = 0;

    // Set timestamp
    s->timestamp = bu_gettime();
}

void
bv_obj_put(struct bv_scene_obj *s)
{
    bv_log(1, "bv_obj_put %s[%s]", bu_vls_cstr(&s->s_name), (s->s_v) ? bu_vls_cstr(&s->s_v->gv_name) : "NULL");
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(&s->children, i);
	bv_obj_put(cg);
    }

    // If this object was selected for snapping, it is no longer a valid candidate
    // TODO - manage this at a higher level
    if (s->s_v)
	bu_ptbl_rm(&s->s_v->gv_s->gv_snap_objs, (long *)s);

    if (s->obj_pool) {
	bv_obj_pool_put(s->obj_pool, s);
    } else {
	bv_obj_reset(s);
	bu_vls_free(&s->s_name);
	BU_FREE(s, struct bv_scene_obj);
    }
}

int
bv_obj_bound(struct bv_scene_obj *s)
{
    int cmd;
    VSET(s->bmin, INFINITY, INFINITY, INFINITY);
    VSET(s->bmax, -INFINITY, -INFINITY, -INFINITY);
    int calc = 0;
    if (s->s_type_flags & BV_MESH_LOD) {
	struct bv_mesh_lod *i = (struct bv_mesh_lod *)s->draw_data;
	if (i) {
	    point_t obmin, obmax;
	    VMOVE(obmin, i->bmin);
	    VMOVE(obmax, i->bmax);
	    // Apply the scene matrix to the bounding box values to bound this
	    // instance, since the BV_MESH_LOD data is based on the
	    // non-instanced mesh.
	    MAT4X3PNT(s->bmin, s->s_mat, obmin);
	    MAT4X3PNT(s->bmax, s->s_mat, obmax);
	    calc = 1;
	}
    } else if (bu_list_len(&s->s_vlist)) {
	int dismode;
	cmd = bv_vlist_bbox(&s->s_vlist, &s->bmin, &s->bmax, NULL, &dismode);
	if (cmd) {
	    bu_log("unknown vlist op %d\n", cmd);
	}
	s->s_displayobj = dismode;
	calc = 1;
    }
    if (calc) {
	s->s_center[X] = (s->bmin[X] + s->bmax[X]) * 0.5;
	s->s_center[Y] = (s->bmin[Y] + s->bmax[Y]) * 0.5;
	s->s_center[Z] = (s->bmin[Z] + s->bmax[Z]) * 0.5;

	s->s_size = s->bmax[X] - s->bmin[X];
	V_MAX(s->s_size, s->bmax[Y] - s->bmin[Y]);
	V_MAX(s->s_size, s->bmax[Z] - s->bmin[Z]);

	return 1;
    }

    return 0;
}

int
bv_illum_obj(struct bv_scene_obj *s, char ill_state)
{
    bool changed = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	int cchanged = bv_illum_obj(s_c, ill_state);
	if (cchanged)
	    changed = 1;
    }
    if (ill_state != s->s_iflag) {
	changed = 1;
	s->s_iflag = ill_state;
    }

    return changed;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
