/*             T E S T _ L I G H T . C
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
/** @file libbsg/tests/test_light.c
 *
 * Phase 7 unit tests: renderer-neutral light model.
 */

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "bu/app.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/light.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)


/* ------------------------------------------------------------------ */
/* Test 1: create / destroy                                             */
/* ------------------------------------------------------------------ */

static int
test_create_destroy(void)
{
    printf("=== Test 1: create_destroy ===\n");

    struct bsg_light *light = bsg_light_create(BSG_LIGHT_POINT);
    if (!light) FAIL("bsg_light_create returned NULL");

    if (bsg_light_get_type(light) != BSG_LIGHT_POINT) FAIL("type should be POINT");
    if (!NEAR_EQUAL(bsg_light_get_intensity(light), 1.0, SMALL_FASTF))
	FAIL("default intensity should be 1.0");
    if (!bsg_light_get_active(light)) FAIL("default active should be non-zero");

    bsg_light_destroy(light);
    bsg_light_destroy(NULL);  /* must not crash */
    PASS("create_destroy");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 2: type constants                                               */
/* ------------------------------------------------------------------ */

static int
test_types(void)
{
    printf("=== Test 2: types ===\n");

    struct bsg_light *a = bsg_light_create(BSG_LIGHT_AMBIENT);
    struct bsg_light *d = bsg_light_create(BSG_LIGHT_DIRECTIONAL);
    struct bsg_light *p = bsg_light_create(BSG_LIGHT_POINT);
    struct bsg_light *s = bsg_light_create(BSG_LIGHT_SPOT);

    if (bsg_light_get_type(a) != BSG_LIGHT_AMBIENT)     FAIL("ambient type");
    if (bsg_light_get_type(d) != BSG_LIGHT_DIRECTIONAL) FAIL("directional type");
    if (bsg_light_get_type(p) != BSG_LIGHT_POINT)       FAIL("point type");
    if (bsg_light_get_type(s) != BSG_LIGHT_SPOT)        FAIL("spot type");

    bsg_light_destroy(a); bsg_light_destroy(d);
    bsg_light_destroy(p); bsg_light_destroy(s);
    PASS("types");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 3: position / direction accessors                               */
/* ------------------------------------------------------------------ */

static int
test_position_direction(void)
{
    printf("=== Test 3: position_direction ===\n");

    struct bsg_light *light = bsg_light_create(BSG_LIGHT_SPOT);
    point_t pos, got_pos;
    vect_t  dir, got_dir;

    VSET(pos, 1.0, 2.0, 3.0);
    bsg_light_set_position(light, pos);
    bsg_light_get_position(light, got_pos);
    if (!VEQUAL(pos, got_pos)) FAIL("position round-trip");

    VSET(dir, 0.0, -1.0, 0.0);
    bsg_light_set_direction(light, dir);
    bsg_light_get_direction(light, got_dir);
    if (!VEQUAL(dir, got_dir)) FAIL("direction round-trip");

    bsg_light_destroy(light);
    PASS("position_direction");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 4: color accessors                                              */
/* ------------------------------------------------------------------ */

static int
test_colors(void)
{
    printf("=== Test 4: colors ===\n");

    struct bsg_light *light = bsg_light_create(BSG_LIGHT_DIRECTIONAL);
    float r, g, b;

    bsg_light_set_diffuse(light, 0.5f, 0.6f, 0.7f);
    bsg_light_get_diffuse(light, &r, &g, &b);
    if (fabs(r - 0.5f) > 1e-5f || fabs(g - 0.6f) > 1e-5f || fabs(b - 0.7f) > 1e-5f)
	FAIL("diffuse round-trip");

    bsg_light_set_specular(light, 0.1f, 0.2f, 0.3f);
    bsg_light_get_specular(light, &r, &g, &b);
    if (fabs(r - 0.1f) > 1e-5f || fabs(g - 0.2f) > 1e-5f || fabs(b - 0.3f) > 1e-5f)
	FAIL("specular round-trip");

    bsg_light_set_ambient(light, 0.05f, 0.05f, 0.05f);
    bsg_light_get_ambient(light, &r, &g, &b);
    if (fabs(r - 0.05f) > 1e-5f) FAIL("ambient round-trip");

    bsg_light_destroy(light);
    PASS("colors");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 5: intensity / spot cutoff / active                             */
/* ------------------------------------------------------------------ */

static int
test_properties(void)
{
    printf("=== Test 5: properties ===\n");

    struct bsg_light *light = bsg_light_create(BSG_LIGHT_SPOT);

    bsg_light_set_intensity(light, 2.5);
    if (!NEAR_EQUAL(bsg_light_get_intensity(light), 2.5, 1e-6)) FAIL("intensity");

    bsg_light_set_spot_cutoff(light, 30.0);
    if (!NEAR_EQUAL(bsg_light_get_spot_cutoff(light), 30.0, 1e-6)) FAIL("spot_cutoff");

    bsg_light_set_active(light, 0);
    if (bsg_light_get_active(light)) FAIL("active=0 not stored");

    bsg_light_set_active(light, 1);
    if (!bsg_light_get_active(light)) FAIL("active=1 not restored");

    bsg_light_destroy(light);
    PASS("properties");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 6: NULL guards                                                  */
/* ------------------------------------------------------------------ */

static int
test_null_guards(void)
{
    printf("=== Test 6: null_guards ===\n");

    bsg_light_destroy(NULL);
    bsg_light_set_intensity(NULL, 1.0);
    bsg_light_set_active(NULL, 1);

    if (bsg_light_get_type(NULL) != -1)          FAIL("get_type(NULL) should be -1");
    if (!NEAR_ZERO(bsg_light_get_intensity(NULL), SMALL_FASTF))
	FAIL("get_intensity(NULL) should be 0");
    if (bsg_light_get_active(NULL) != 0)         FAIL("get_active(NULL) should be 0");

    PASS("null_guards");
    return 0;
}


/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    int failures = 0;

    failures += test_create_destroy();
    failures += test_types();
    failures += test_position_direction();
    failures += test_colors();
    failures += test_properties();
    failures += test_null_guards();

    if (failures == 0)
	printf("RESULT: all Phase 7 light tests PASSED\n");
    else
	printf("RESULT: %d test(s) FAILED\n", failures);

    return failures;
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
