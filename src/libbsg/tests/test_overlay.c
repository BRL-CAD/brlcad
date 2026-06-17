/*               T E S T _ O V E R L A Y . C
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
/** @file libbsg/tests/test_overlay.c
 *
 * Unit tests for bsg overlay node API.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/feature.h"
#include "bsg/hud.h"
#include "bsg/overlay.h"
#include "bsg/scene_builder.h"
#include "bsg/util.h"

static struct bsg_view *
_make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "test_overlay_view");
    (void)bsg_view_scene_root_ensure(v);
    return v;
}

static void
_free_view(struct bsg_view *v)
{
    if (!v)
return;
    bsg_hud_root_destroy(v);
    bsg_view_free(v);
    bu_free(v, "test_overlay view");
}

#define CHECK(_cond, _msg) do { if (!(_cond)) { printf("FAIL: %s\n", (_msg)); return 1; } } while (0)

static int
test_owner_replace_clear(void)
{
    struct bsg_view *v = _make_view();
    int owner = 7;
    bsg_feature_ref f1 = bsg_feature_create_lines(v, "overlay_a", 0);
    CHECK(!bsg_feature_ref_is_null(f1), "create overlay a");
    CHECK(bsg_feature_overlay_register_owner(f1, &owner, BSG_OVERLAY_ROLE_SCREEN,
BSG_OVERLAY_CLASS_MEASURE, BSG_OVERLAY_LC_PER_TOOL,
BSG_OVERLAY_ORDER_POST_TRANSPARENT, NULL, 0), "register overlay a");

    bsg_feature_ref f2 = bsg_feature_create_lines(v, "overlay_b", 0);
    CHECK(!bsg_feature_ref_is_null(f2), "create overlay b");
    CHECK(bsg_feature_overlay_register_owner(f2, &owner, BSG_OVERLAY_ROLE_SCREEN,
BSG_OVERLAY_CLASS_MEASURE, BSG_OVERLAY_LC_PER_TOOL,
BSG_OVERLAY_ORDER_POST_TRANSPARENT, NULL, 1), "register overlay b");

    CHECK(bsg_overlay_clear_owned(v, &owner) == 2, "clear owned overlays");
    CHECK(bsg_feature_ref_is_null(bsg_feature_find(v, "overlay_a")),
	    "overlay a removed by owner clear");
    CHECK(bsg_feature_ref_is_null(bsg_feature_find(v, "overlay_b")),
	    "overlay b removed by owner clear");
    _free_view(v);
    return 0;
}

static int
test_auto_remove(void)
{
    struct bsg_view *v = _make_view();
    bsg_feature_ref ref = bsg_feature_create_lines(v, "overlay_source", 0);
    CHECK(!bsg_feature_ref_is_null(ref), "create source overlay");
    CHECK(bsg_feature_overlay_register_owner(ref, v, BSG_OVERLAY_ROLE_MODEL,
BSG_OVERLAY_CLASS_COMMAND_RESULT, BSG_OVERLAY_LC_AUTO_REMOVE_ON_SOURCE,
BSG_OVERLAY_ORDER_MODEL, "db/path", 0), "register source overlay");
    CHECK(bsg_feature_overlay_auto_remove(v, "db/path") == 1,
"auto remove overlay by source");
    CHECK(bsg_feature_ref_is_null(bsg_feature_find(v, "overlay_source")),
	    "overlay source removed");
    _free_view(v);
    return 0;
}

static int
test_overlay_public_lookup_uses_scene_fields(void)
{
    struct bsg_view *v = _make_view();
    bsg_scene_ref root = bsg_view_scene_ref_ensure(v);
    CHECK(!bsg_scene_ref_is_null(root), "scene root exists");

    bsg_scene_ref child = bsg_scene_shape_create(v, "canonical-overlay");
    CHECK(!bsg_scene_ref_is_null(child), "create overlay child");
    CHECK(bsg_overlay_append_scene(v, child), "append overlay child");

    bsg_scene_ref ov = bsg_scene_find_child(root, "_overlays");
    CHECK(!bsg_scene_ref_is_null(ov), "overlay group exists");
    CHECK(bsg_scene_visible(ov), "overlay group visible");
    CHECK(bsg_scene_name(ov) && BU_STR_EQUAL(bsg_scene_name(ov), "_overlays"),
	    "overlay group field name");

    bsg_scene_ref found = bsg_overlay_find_scene(v, "canonical-overlay");
    CHECK(bsg_scene_ref_equal(found, child),
	    "overlay child lookup uses field-backed name");
    found = bsg_overlay_find_scene(v, "raw-overlay");
    CHECK(bsg_scene_ref_is_null(found), "overlay child lookup ignores unknown name");

    bsg_overlay_erase_name(v, "canonical-overlay");
    _free_view(v);
    return 0;
}

int
main(int argc, char **argv)
{
    int ret = 0;
    bu_setprogname(argv[0]);
    (void)argc;
    ret += test_owner_replace_clear();
    ret += test_auto_remove();
    ret += test_overlay_public_lookup_uses_scene_fields();
    if (!ret)
printf("ALL TESTS PASSED\n");
    return ret;
}
