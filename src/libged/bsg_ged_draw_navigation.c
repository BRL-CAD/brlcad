/*          B S G _ G E D _ D R A W _ N A V I G A T I O N . C
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
/** @file bsg_ged_draw_navigation.c
 *
 * Draw-tree shape and group navigation helpers.
 */

#include "common.h"

#include "bu/ptbl.h"
#include "bsg/appearance.h"
#include "bsg/draw_intent.h"
#include "bsg/scene_builder.h"
#include "ged/bsg_ged_draw.h"
#include "./ged_private.h"
#include "./bsg_ged_draw_private.h"


static bsg_scene_ref
_scene_ref_from_ptr(void *ptr)
{
    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    ref.opaque = ptr;
    return ref;
}


static int
_draw_shape_dfs(bsg_scene_ref ref,
		int (*cb)(bsg_scene_ref, void *),
		void *userdata)
{
    if (bsg_scene_ref_is_null(ref))
	return 1;

    if (bsg_scene_ref_type(ref) == BSG_SCENE_ELEMENT_SHAPE) {
	if (ged_draw_shape_state_get_scene_ref(ref))
	    return (*cb)(ref, userdata);
	return 1;
    }

    for (size_t i = 0; i < bsg_scene_child_count(ref); i++) {
	if (!_draw_shape_dfs(bsg_scene_child_at(ref, i), cb, userdata))
	    return 0;
    }
    return 1;
}


static int
_any_shape_cb(bsg_scene_ref UNUSED(ref), void *ud)
{
    *(int *)ud = 1;
    return 0;
}


int
ged_draw_has_shapes(struct ged *gedp)
{
    if (!gedp)
	return 0;
    int found = 0;
    _draw_shape_dfs(ged_scene_root_ref(gedp), _any_shape_cb, &found);
    return found;
}


struct _first_shape_data {
    bsg_scene_ref result;
};


static int
_first_shape_cb(bsg_scene_ref ref, void *ud)
{
    struct _first_shape_data *d = (struct _first_shape_data *)ud;
    d->result = ref;
    return 0;
}


static bsg_scene_ref
_first_shape_in_group(bsg_scene_ref group_ref)
{
    struct _first_shape_data d;
    d.result = bsg_scene_ref_null();
    _draw_shape_dfs(group_ref, _first_shape_cb, &d);
    return d.result;
}


bsg_scene_ref
ged_draw_first_shape_scene_ref(struct ged *gedp)
{
    if (!gedp)
	return bsg_scene_ref_null();

    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    for (size_t gi = 0; gi < bsg_scene_child_count(root_ref); gi++) {
	bsg_scene_ref group_ref = bsg_scene_child_at(root_ref, gi);
	if (ged_draw_group_scene_ref_is_overlay(group_ref))
	    continue;
	bsg_scene_ref shape_ref = _first_shape_in_group(group_ref);
	if (!bsg_scene_ref_is_null(shape_ref))
	    return shape_ref;
    }
    return bsg_scene_ref_null();
}


static int
_snap_shape_cb(bsg_scene_ref ref, void *ud)
{
    bu_ptbl_ins((struct bu_ptbl *)ud, (long *)ref.opaque);
    return 1;
}


static void
_sg_build_shape_snapshot(struct ged *gedp, struct bu_ptbl *out)
{
    bsg_scene_ref root_ref = ged_scene_root_ref(gedp);
    for (size_t gi = 0; gi < bsg_scene_child_count(root_ref); gi++) {
	bsg_scene_ref group_ref = bsg_scene_child_at(root_ref, gi);
	if (ged_draw_group_scene_ref_is_overlay(group_ref))
	    continue;
	_draw_shape_dfs(group_ref, _snap_shape_cb, (void *)out);
    }
}


int
ged_draw_shape_count(struct ged *gedp)
{
    if (!gedp)
	return 0;
    struct bu_ptbl snap = BU_PTBL_INIT_ZERO;
    _sg_build_shape_snapshot(gedp, &snap);
    int n = (int)BU_PTBL_LEN(&snap);
    bu_ptbl_free(&snap);
    return n;
}


static bsg_scene_ref
_shape_scene_ref_at(struct ged *gedp, int idx)
{
    if (!gedp)
	return bsg_scene_ref_null();
    struct bu_ptbl snap = BU_PTBL_INIT_ZERO;
    _sg_build_shape_snapshot(gedp, &snap);
    bsg_scene_ref shape_ref = bsg_scene_ref_null();
    int n = (int)BU_PTBL_LEN(&snap);
    if (n > 0) {
	idx = ((idx % n) + n) % n;
	shape_ref = _scene_ref_from_ptr((void *)BU_PTBL_GET(&snap, idx));
    }
    bu_ptbl_free(&snap);
    return shape_ref;
}


ged_draw_shape_ref
ged_draw_first_shape_ref(struct ged *gedp)
{
    return ged_draw_shape_ref_from_scene_ref(gedp,
	    ged_draw_first_shape_scene_ref(gedp));
}


ged_draw_shape_ref
ged_draw_shape_ref_at(struct ged *gedp, int idx)
{
    return ged_draw_shape_ref_from_scene_ref(gedp,
	    _shape_scene_ref_at(gedp, idx));
}


int
ged_draw_shape_ref_index(struct ged *gedp, ged_draw_shape_ref ref)
{
    if (!gedp || ged_draw_shape_ref_is_null(ref))
	return -1;
    bsg_scene_ref target = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(target))
	return -1;

    struct bu_ptbl snap = BU_PTBL_INIT_ZERO;
    _sg_build_shape_snapshot(gedp, &snap);
    int found = -1;
    for (int i = 0; i < (int)BU_PTBL_LEN(&snap); i++) {
	if (bsg_scene_ref_equal(_scene_ref_from_ptr((void *)BU_PTBL_GET(&snap, i)),
		target)) {
	    found = i;
	    break;
	}
    }
    bu_ptbl_free(&snap);
    return found;
}


ged_draw_shape_ref
ged_draw_advance_shape_ref(struct ged *gedp, ged_draw_shape_ref ref, int delta)
{
    if (!gedp)
	return GED_DRAW_SHAPE_REF_NULL;

    bsg_scene_ref target = ged_draw_registry_shape_scene_ref(gedp, ref);
    struct bu_ptbl snap = BU_PTBL_INIT_ZERO;
    _sg_build_shape_snapshot(gedp, &snap);

    ged_draw_shape_ref result = GED_DRAW_SHAPE_REF_NULL;
    int n = (int)BU_PTBL_LEN(&snap);
    if (n > 0) {
	int idx = 0;
	if (!bsg_scene_ref_is_null(target)) {
	    for (int i = 0; i < n; i++) {
		if (bsg_scene_ref_equal(
			_scene_ref_from_ptr((void *)BU_PTBL_GET(&snap, i)),
			target)) {
		    idx = i;
		    break;
		}
	    }
	}
	int new_idx = (((idx + delta) % n) + n) % n;
	result = ged_draw_shape_ref_from_scene_ref(gedp,
		_scene_ref_from_ptr((void *)BU_PTBL_GET(&snap, new_idx)));
    }

    bu_ptbl_free(&snap);
    return result;
}


static bsg_scene_ref
_group_scene_ref_of_shape(bsg_scene_ref shape_ref)
{
    if (bsg_scene_ref_is_null(shape_ref))
	return bsg_scene_ref_null();

    bsg_scene_ref group_ref = bsg_scene_parent(shape_ref);
    while (!bsg_scene_ref_is_null(group_ref)) {
	bsg_scene_ref parent_ref = bsg_scene_parent(group_ref);
	if (bsg_scene_ref_is_null(parent_ref))
	    return group_ref;
	bsg_scene_ref grandparent_ref = bsg_scene_parent(parent_ref);
	if (bsg_scene_ref_is_null(grandparent_ref))
	    return group_ref;
	group_ref = parent_ref;
    }
    return bsg_scene_ref_null();
}


ged_draw_group_ref
ged_draw_group_ref_of_shape(struct ged *gedp, ged_draw_shape_ref ref)
{
    return ged_draw_group_ref_from_scene_ref(gedp,
	    _group_scene_ref_of_shape(ged_draw_registry_shape_scene_ref(gedp, ref)));
}


const char *
ged_draw_group_scene_ref_path(bsg_scene_ref group_ref)
{
    const struct bsg_draw_intent *di = bsg_scene_draw_intent(group_ref);
    return bsg_draw_intent_path(di);
}


bsg_draw_mode
ged_draw_group_scene_ref_mode(bsg_scene_ref group_ref)
{
    const struct bsg_draw_intent *di = bsg_scene_draw_intent(group_ref);
    if (di)
	return bsg_draw_intent_mode(di);

    bsg_scene_ref shape_ref = _first_shape_in_group(group_ref);
    if (!bsg_scene_ref_is_null(shape_ref)) {
	switch (bsg_scene_dmode(shape_ref)) {
	    case BSG_DRAW_MODE_WIRE:
		return BSG_DRAW_MODE_WIRE;
	    case BSG_DRAW_MODE_SHADED_BOTS:
		return BSG_DRAW_MODE_SHADED_BOTS;
	    case BSG_DRAW_MODE_SHADED:
		return BSG_DRAW_MODE_SHADED;
	    case BSG_DRAW_MODE_EVAL_WIRE:
		return BSG_DRAW_MODE_EVAL_WIRE;
	    case BSG_DRAW_MODE_HIDDEN_LINE:
		return BSG_DRAW_MODE_HIDDEN_LINE;
	    case BSG_DRAW_MODE_EVAL_POINTS:
		return BSG_DRAW_MODE_EVAL_POINTS;
	    default:
		break;
	}
    }
    return BSG_DRAW_MODE_WIRE;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
