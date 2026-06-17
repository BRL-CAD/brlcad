/*  T E S T _ A P P E A R A N C E _ R E S O L V E . C
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
/** @file libbsg/tests/test_appearance_resolve.c
 *
 * Phase D5 unit tests: bsg_appearance_resolve — layer composition,
 * path accumulation, highlight, and inheritance.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg/material.h"
#include "bsg/node.h"
#include "bsg/render_settings.h"
#include "../node_private.h"
#include "bsg/appearance.h"
#include "bsg/appearance_action.h"
#include "../appearance_private.h"
#include "../material_private.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)

static struct bsg_view *
_make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    return v;
}

static void
_free_view(struct bsg_view *v)
{
    if (!v) return;
    bsg_view_free(v);
    bu_free(v, "bsg_view");
}


/* -----------------------------------------------------------------------
 * Test 1: null inputs return 0
 * ----------------------------------------------------------------------- */
static int
test_null_inputs(void)
{
    printf("=== Test 1: null inputs ===\n");

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));

    /* NULL node */
    int r = bsg_appearance_resolve(NULL, NULL, NULL, &ra);
    if (r != 0) FAIL("NULL node should return 0");

    /* NULL out */
    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);
    r = bsg_appearance_resolve(v, s, NULL, NULL);
    if (r != 0) FAIL("NULL out should return 0");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("null inputs");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 2: base layer - canonical material color, no override
 * ----------------------------------------------------------------------- */
static int
test_base_color(void)
{
    printf("=== Test 2: base layer color ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);
    bsg_material_set_rgb(s, 200, 100, 50);

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    int r = bsg_appearance_resolve(v, s, NULL, &ra);
    if (!r)          FAIL("should succeed for valid node");
    if (ra.color[0] != 200) FAIL("color[0] should be 200");
    if (ra.color[1] != 100) FAIL("color[1] should be 100");
    if (ra.color[2] != 50)  FAIL("color[2] should be 50");
    if (!(ra.active_layers & BSG_ALAY_BASE)) FAIL("BSG_ALAY_BASE should be set");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("base layer color");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 3: command override
 * ----------------------------------------------------------------------- */
static int
test_command_override(void)
{
    printf("=== Test 3: command override ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);
    bsg_material_set_rgb(s, 10, 20, 30);

    unsigned char override_color[3] = {255, 0, 0};
    bsg_appearance_set_color_override(s, override_color, 1);
    bsg_appearance_set_transparency(s, 1.0);
    bsg_appearance_set_line_width(s, 1);

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve(v, s, NULL, &ra);

    if (ra.color[0] != 255) FAIL("override color[0] should be 255");
    if (ra.color[1] != 0)   FAIL("override color[1] should be 0");
    if (ra.color[2] != 0)   FAIL("override color[2] should be 0");
    if (!(ra.active_layers & BSG_ALAY_COMMAND)) FAIL("BSG_ALAY_COMMAND should be set");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("command override");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 4: renderer-default color request uses canonical metadata
 * ----------------------------------------------------------------------- */
static int
test_default_color_metadata(void)
{
    printf("=== Test 4: default color metadata ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);
    bsg_material_set_rgb(s, 10, 20, 30);

    struct bsg_render_settings settings;
    bsg_render_settings_init_defaults(&settings);
    settings.geometry_default_color[0] = 9;
    settings.geometry_default_color[1] = 8;
    settings.geometry_default_color[2] = 7;

    bsg_appearance_set_legacy_uses_default_color(s, 1);

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve_with_settings(&settings, s, NULL, &ra);

    if (ra.color[0] != 9) FAIL("default color[0] should be metadata-backed");
    if (ra.color[1] != 8) FAIL("default color[1] should be metadata-backed");
    if (ra.color[2] != 7) FAIL("default color[2] should be metadata-backed");
    if (!(ra.active_layers & BSG_ALAY_GEOM_DEFAULT))
	FAIL("default-color layer should be recorded");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("default color metadata");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 5: highlight state enabled
 * ----------------------------------------------------------------------- */
static int
test_highlight(void)
{
    printf("=== Test 5: highlight state ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);
    bsg_appearance_set_highlighted(s, 1);

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve(v, s, NULL, &ra);

    if (!ra.highlighted) FAIL("highlighted should be non-zero when highlight state is enabled");
    if (!(ra.active_layers & BSG_ALAY_HIGHLIGHT)) FAIL("BSG_ALAY_HIGHLIGHT should be set");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("highlight state");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 6: transparency layer
 * ----------------------------------------------------------------------- */
static int
test_transparency(void)
{
    printf("=== Test 6: transparency layer ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);

    bsg_appearance_set_transparency(s, 0.5);
    bsg_appearance_set_line_width(s, 1);

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve(v, s, NULL, &ra);

    if (fabs(ra.transparency - 0.5) > 1e-6)
	FAIL("transparency should be 0.5");
    if (!(ra.active_layers & BSG_ALAY_TRANSPARENCY))
	FAIL("BSG_ALAY_TRANSPARENCY should be set for transparency < 1");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("transparency layer");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 7: fully opaque node has transparency == 1.0
 * ----------------------------------------------------------------------- */
static int
test_opaque_default(void)
{
    printf("=== Test 7: opaque default ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve(v, s, NULL, &ra);

    if (fabs(ra.transparency - 1.0) > 1e-6)
	FAIL("default transparency should be 1.0 (fully opaque)");
    if (ra.active_layers & BSG_ALAY_TRANSPARENCY)
	FAIL("BSG_ALAY_TRANSPARENCY should NOT be set for fully opaque node");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("opaque default");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 8: line style / draw mode from fields
 * ----------------------------------------------------------------------- */
static int
test_line_style_dmode(void)
{
    printf("=== Test 8: line style and draw mode ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);

    bsg_appearance_set_transparency(s, 0.75);
    bsg_appearance_set_dmode(s, 3);   /* arbitrary non-zero draw mode */
    bsg_appearance_set_line_width(s, 2);
    bsg_appearance_set_line_style(s, 1);

    if (fabs(bsg_appearance_transparency(s) - 0.75) > 1e-6)
	FAIL("transparency accessor should use canonical field");
    if (bsg_appearance_dmode(s) != 3)
	FAIL("draw mode accessor should use canonical field");
    if (bsg_appearance_line_width(s) != 2)
	FAIL("line width accessor should use canonical field");
    if (bsg_appearance_line_style(s) != 1)
	FAIL("line style accessor should use canonical field");

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve(v, s, NULL, &ra);

    if (ra.draw_mode != 3)      FAIL("draw_mode should be 3");
    if (ra.line_width != 2) FAIL("line_width should be 2");
    if (ra.line_style != 1) FAIL("line_style should be 1");
    if (fabs(ra.transparency - 0.75) > 1e-6)
	FAIL("transparency should be 0.75");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("line style and draw mode");
    return 0;
}

/* -----------------------------------------------------------------------
 * Test 9: appearance defaults are canonical
 * ----------------------------------------------------------------------- */
static int
test_raw_appearance_mirrors_not_authoritative(void)
{
    printf("=== Test 9: appearance defaults ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);

    unsigned char basecolor[3] = {1, 1, 1};
    bsg_appearance_legacy_basecolor(s, basecolor);
    if (bsg_appearance_force_draw(s)) FAIL("force-draw should default off");
    if (bsg_appearance_inherited_by_children(s)) FAIL("inheritance should default off");
    if (bsg_appearance_uses_default_color(s)) FAIL("default-color request should default off");
    if (bsg_appearance_legacy_user_color(s)) FAIL("user-color metadata should default off");
    if (bsg_appearance_legacy_default_color(s)) FAIL("default-color metadata should default off");
    if (basecolor[0] || basecolor[1] || basecolor[2]) FAIL("basecolor metadata should default black");
    if (bsg_appearance_legacy_eval_flag(s)) FAIL("eval flag should default off");
    if (bsg_appearance_legacy_region_id(s)) FAIL("region-id should default zero");
    if (bsg_appearance_color_override(s, NULL)) FAIL("color override should default off");
    if (fabs(bsg_appearance_transparency(s) - 1.0) > 1e-6)
	FAIL("transparency should default opaque");
    if (bsg_appearance_dmode(s) != 0) FAIL("draw mode should default zero");
    if (bsg_appearance_line_width(s) != 1) FAIL("line width should default one");
    if (bsg_appearance_line_style(s) != 0) FAIL("line style should default zero");

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve(v, s, NULL, &ra);
    if (ra.draw_mode != 0) FAIL("resolved draw mode should use default");
    if (ra.line_width != 1) FAIL("resolved line width should use default");
    if (ra.line_style != 0) FAIL("resolved line style should use default");
    if (fabs(ra.transparency - 1.0) > 1e-6) FAIL("resolved transparency should use default");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("appearance defaults");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 10: inherited_os color override
 * ----------------------------------------------------------------------- */
static int
test_inherited_override(void)
{
    printf("=== Test 9: inherited_os color override ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);
    /* Node has its own color */
    bsg_material_set_rgb(s, 50, 50, 50);

    /* Inherited settings provide a group override */
    struct bsg_appearance_settings inherited;
    memset(&inherited, 0, sizeof(inherited));
    inherited.color_override = 1;
    inherited.color[0] = 100;
    inherited.color[1] = 150;
    inherited.color[2] = 200;
    inherited.transparency = 1.0;
    inherited.s_line_width = 1;

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve(v, s, &inherited, &ra);

    /* Inherited color should win over node's base color */
    if (ra.color[0] != 100) FAIL("inherited color[0] should be 100");
    if (ra.color[1] != 150) FAIL("inherited color[1] should be 150");
    if (ra.color[2] != 200) FAIL("inherited color[2] should be 200");
    if (!(ra.active_layers & BSG_ALAY_INHERITED)) FAIL("BSG_ALAY_INHERITED should be set");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("inherited_os color override");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 11: local node settings beat inherited group settings
 * ----------------------------------------------------------------------- */
static int
test_local_override_beats_inherited(void)
{
    printf("=== Test 10: local override beats inherited ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);
    bsg_material_set_rgb(s, 50, 50, 50);

    struct bsg_appearance_settings inherited;
    memset(&inherited, 0, sizeof(inherited));
    inherited.color_override = 1;
    inherited.color[0] = 100;
    inherited.color[1] = 150;
    inherited.color[2] = 200;
    inherited.transparency = 0.75;
    inherited.draw_mode = 2;
    inherited.s_line_width = 3;

    unsigned char local_color[3] = {5, 10, 15};
    bsg_appearance_set_color_override(s, local_color, 1);
    bsg_appearance_set_transparency(s, 0.25);
    bsg_appearance_set_dmode(s, 4);
    bsg_appearance_set_line_width(s, 7);

    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve(v, s, &inherited, &ra);

    if (ra.color[0] != 5) FAIL("local color[0] should win");
    if (ra.color[1] != 10) FAIL("local color[1] should win");
    if (ra.color[2] != 15) FAIL("local color[2] should win");
    if (fabs(ra.transparency - 0.25) > 1e-6) FAIL("local transparency should win");
    if (ra.draw_mode != 4) FAIL("local draw mode should win");
    if (ra.line_width != 7) FAIL("local line width should win");
    if (!(ra.active_layers & BSG_ALAY_INHERITED)) FAIL("inherited layer should be recorded");
    if (!(ra.active_layers & BSG_ALAY_COMMAND)) FAIL("local layer should be recorded");

    bsg_shape_destroy(s);
    _free_view(v);
    PASS("local override beats inherited");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 12: inheritance flag is public appearance state
 * ----------------------------------------------------------------------- */
static int
test_inheritance_flag(void)
{
    printf("=== Test 11: inheritance flag ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *g = bsg_group_create(v);

    if (bsg_appearance_inherited_by_children(g))
	FAIL("inheritance should default off");
    bsg_appearance_set_inherited_by_children(g, 1);
    if (!bsg_appearance_inherited_by_children(g))
	FAIL("inheritance should turn on");
    bsg_appearance_set_inherited_by_children(g, 0);
    if (bsg_appearance_inherited_by_children(g))
	FAIL("inheritance should turn off");

    bsg_group_destroy(g);
    _free_view(v);
    PASS("inheritance flag");
    return 0;
}


/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    (void)argc;

    int failures = 0;
    failures += test_null_inputs();
    failures += test_base_color();
    failures += test_command_override();
    failures += test_default_color_metadata();
    failures += test_highlight();
    failures += test_transparency();
    failures += test_opaque_default();
    failures += test_line_style_dmode();
    failures += test_raw_appearance_mirrors_not_authoritative();
    failures += test_inherited_override();
    failures += test_local_override_beats_inherited();
    failures += test_inheritance_flag();

    if (failures) {
	printf("\n%d test(s) FAILED\n", failures);
	return 1;
    }
    printf("\nAll appearance_resolve tests PASSED\n");
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
