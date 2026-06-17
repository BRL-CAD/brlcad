/*              T E S T _ V I E W _ S T A T E . C
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
/** @file libbsg/tests/test_view_state.c
 *
 * Unit tests for bsg/view_state.h accessors.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bsg/selection.h"
#include "bsg/util.h"
#include "bsg/view_state.h"

static struct bsg_view *
make_view(const char *name)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "%s", name);
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "test_view_state view");
}

static int
test_local_accessors(void)
{
    struct bsg_view *v = make_view("local");
    if (!bsg_view_state_active(v)) {
	printf("FAIL: local active state\n");
	return 1;
    }
    if (!bsg_view_selection(v) || bsg_selection_count(bsg_view_selection(v)) != 0) {
	printf("FAIL: selection accessor\n");
	return 1;
    }

    bsg_view_set_framebuffer_mode(v, 2);
    if (bsg_view_framebuffer_mode(v) != 2) {
	printf("FAIL: framebuffer mode\n");
	return 1;
    }
    bsg_view_set_zclip(v, 1);
    if (!bsg_view_zclip(v)) {
	printf("FAIL: zclip\n");
	return 1;
    }
    struct bsg_lod_source_policy_settings lod_policy;
    if (!bsg_view_lod_source_policy_get(v, &lod_policy)) {
	printf("FAIL: lod policy get\n");
	return 1;
    }
    lod_policy.mesh_enabled = 1;
    lod_policy.csg_enabled = 0;
    lod_policy.zoom_refresh = 1;
    bsg_view_lod_source_policy_set(v, &lod_policy);
    if (!bsg_view_lod_source_policy_get(v, &lod_policy) ||
	    !lod_policy.mesh_enabled || lod_policy.csg_enabled ||
	    !lod_policy.zoom_refresh) {
	printf("FAIL: lod policy set\n");
	return 1;
    }

    struct bsg_interactive_rect_state rect;
    if (!bsg_view_interactive_rect_get(v, &rect)) {
	printf("FAIL: rect accessor\n");
	return 1;
    }
    rect.draw = 1;
    bsg_view_interactive_rect_set(v, &rect);
    memset(&rect, 0, sizeof(rect));
    bsg_view_interactive_rect_get(v, &rect);
    if (!rect.draw) {
	printf("FAIL: rect storage\n");
	return 1;
    }

    struct bsg_grid_state grid;
    bsg_view_grid_get(v, &grid);
    grid.snap = 1;
    bsg_view_grid_set(v, &grid);
    bsg_view_set_snap_lines(v, 1);
    bsg_view_set_snap_source_flags(v, BSG_SNAP_TCL);
    if (bsg_view_snap_source_flags(v) != BSG_SNAP_TCL) {
	printf("FAIL: snap source flags\n");
	return 1;
    }
    if ((bsg_view_snap_kind_mask(v) & (BSG_SNAP_KIND_GRID | BSG_SNAP_KIND_ENDPOINT)) !=
	    (BSG_SNAP_KIND_GRID | BSG_SNAP_KIND_ENDPOINT)) {
	printf("FAIL: snap kind mask\n");
	return 1;
    }
    bsg_feature_ref null_feature = BSG_FEATURE_REF_NULL_INIT;
    bsg_view_snap_exclude_feature_set(v, null_feature);
    if (bsg_view_snap_exclude_feature_get(v, &null_feature)) {
	printf("FAIL: null snap exclusion\n");
	return 1;
    }

    free_view(v);
    return 0;
}

static int
test_shared_accessors(void)
{
    struct bsg_view *v = make_view("shared");
    struct bsg_view *other = make_view("other");

    if (bsg_view_settings_shared(v, other)) {
	printf("FAIL: independent views report shared settings\n");
	return 1;
    }

    free_view(other);
    free_view(v);
    return 0;
}

static int
test_state_sync_apply(void)
{
    struct bsg_view *v = make_view("state");
    struct bsg_view_state *state = bsg_view_state_create();

    if (!state) {
	printf("FAIL: state create\n");
	return 1;
    }

    v->gv_scale = 42.0;
    bsg_view_set_framebuffer_mode(v, 1);
    bsg_view_set_zclip(v, 1);
    bsg_view_state_sync_from_view(state, v);

    struct bsg_camera *cam = bsg_view_state_camera(state);
    struct bsg_render_settings *rs = bsg_view_state_render_settings(state);
    if (!cam || !rs || !NEAR_EQUAL(cam->scale, 42.0, SMALL_FASTF) ||
	    rs->fb_mode != BSG_FB_OVERLAY || !rs->zclip) {
	printf("FAIL: state sync\n");
	return 1;
    }

    cam->scale = 84.0;
    rs->fb_mode = BSG_FB_UNDERLAY;
    rs->zclip = 0;
    rs->lod_source_policy.csg_enabled = 1;
    rs->lod_source_policy.point_scale = 3.0;
    bsg_view_state_apply_to_view(state, v);
    struct bsg_lod_source_policy_settings applied_lod_policy;
    (void)bsg_view_lod_source_policy_get(v, &applied_lod_policy);

    if (!NEAR_EQUAL(v->gv_scale, 84.0, SMALL_FASTF) ||
	    bsg_view_framebuffer_mode(v) != 2 ||
	    bsg_view_zclip(v) ||
	    !applied_lod_policy.csg_enabled ||
	    !NEAR_EQUAL(applied_lod_policy.point_scale, 3.0, SMALL_FASTF)) {
	printf("FAIL: state apply\n");
	return 1;
    }

    bsg_view_state_destroy(state);
    free_view(v);
    return 0;
}

static int
test_refresh_state(void)
{
    struct bsg_view *v = make_view("refresh");
    struct bsg_view_refresh_state refresh;

    if (!bsg_view_refresh_get(v, &refresh) || !refresh.enabled ||
	    refresh.dirty_flags || refresh.request_revision || refresh.completed_revision) {
	printf("FAIL: refresh defaults\n");
	return 1;
    }

    bsg_view_refresh_request(v, BSG_VIEW_REFRESH_VIEW);
    if (!bsg_view_refresh_dirty(v)) {
	printf("FAIL: refresh dirty after request\n");
	return 1;
    }

    uint32_t flags = bsg_view_refresh_consume(v);
    if (!(flags & BSG_VIEW_REFRESH_VIEW) || bsg_view_refresh_dirty(v)) {
	printf("FAIL: refresh consume\n");
	return 1;
    }

    bsg_view_refresh_suppress_begin(v);
    bsg_view_refresh_request(v, BSG_VIEW_REFRESH_DRAW);
    if (bsg_view_refresh_dirty(v)) {
	printf("FAIL: refresh suppression\n");
	return 1;
    }
    bsg_view_refresh_suppress_end(v);
    if (!bsg_view_refresh_dirty(v)) {
	printf("FAIL: refresh unsuppression\n");
	return 1;
    }

    bsg_view_refresh_set_enabled(v, 0);
    if (bsg_view_refresh_dirty(v)) {
	printf("FAIL: refresh disabled\n");
	return 1;
    }
    bsg_view_refresh_set_enabled(v, 1);
    bsg_view_refresh_complete(v);

    bsg_view_refresh_set_drawn_count(v, 3);
    if (bsg_view_refresh_drawn_count(v) != 3) {
	printf("FAIL: refresh drawn count\n");
	return 1;
    }

    free_view(v);
    return 0;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    (void)argc;
    if (test_local_accessors())
	return 1;
    if (test_shared_accessors())
	return 1;
    if (test_state_sync_apply())
	return 1;
    if (test_refresh_state())
	return 1;
    printf("PASS test_view_state\n");
    return 0;
}
