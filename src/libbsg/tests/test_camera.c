/*            T E S T _ C A M E R A . C
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
/** @file libbsg/tests/test_camera.c
 *
 * Phase 7 unit tests: camera snapshot / apply.
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/util.h"
#include "bsg/camera.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)

#define VNEAREQ(a,b,tol) \
    (fabs((a)[0]-(b)[0]) <= (tol) && \
     fabs((a)[1]-(b)[1]) <= (tol) && \
     fabs((a)[2]-(b)[2]) <= (tol))


static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "cam_test_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v) return;
    bsg_view_free(v);
    bu_free(v, "cam_test_view");
}


/* ------------------------------------------------------------------ */
/* Test 1: create / destroy                                             */
/* ------------------------------------------------------------------ */

static int
test_create_destroy(void)
{
    printf("=== Test 1: create_destroy ===\n");

    struct bsg_camera *cam = bsg_camera_create();
    if (!cam) FAIL("bsg_camera_create returned NULL");

    if (!NEAR_EQUAL(cam->scale, 1.0, SMALL_FASTF)) FAIL("default scale should be 1.0");
    if (!NEAR_ZERO(cam->perspective, SMALL_FASTF))  FAIL("default perspective should be 0");

    bsg_camera_destroy(cam);
    bsg_camera_destroy(NULL);  /* must not crash */
    PASS("create_destroy");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 2: snapshot captures view fields                               */
/* ------------------------------------------------------------------ */

static int
test_snapshot(void)
{
    printf("=== Test 2: snapshot ===\n");

    struct bsg_view *v = make_view();
    v->gv_scale       = 3.14;
    v->gv_perspective = 45.0;
    VSET(v->gv_aet,     30.0, 60.0, 0.0);
    VSET(v->gv_eye_pos, 1.0, 2.0, 3.0);

    struct bsg_camera *cam = bsg_camera_snapshot(v);
    if (!cam) FAIL("snapshot returned NULL");

    if (!NEAR_EQUAL(cam->scale, 3.14, 1e-6))      FAIL("scale not captured");
    if (!NEAR_EQUAL(cam->perspective, 45.0, 1e-6)) FAIL("perspective not captured");

    vect_t aet_ref;
    VSET(aet_ref, 30.0, 60.0, 0.0);
    if (!VNEAREQ(cam->aet, aet_ref, 1e-6)) FAIL("aet not captured");

    vect_t eye_ref;
    VSET(eye_ref, 1.0, 2.0, 3.0);
    if (!VNEAREQ(cam->eye_pos, eye_ref, 1e-6)) FAIL("eye_pos not captured");

    bsg_camera_destroy(cam);
    free_view(v);
    PASS("snapshot");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 3: apply restores view fields                                   */
/* ------------------------------------------------------------------ */

static int
test_apply(void)
{
    printf("=== Test 3: apply ===\n");

    struct bsg_view *v = make_view();
    v->gv_scale       = 7.0;
    v->gv_perspective = 20.0;
    VSET(v->gv_aet,     10.0, 20.0, 5.0);
    VSET(v->gv_eye_pos, 4.0, 5.0, 6.0);

    /* Take snapshot, then modify view, then apply snapshot back */
    struct bsg_camera *cam = bsg_camera_snapshot(v);

    v->gv_scale       = 99.0;
    v->gv_perspective = 0.0;
    VSET(v->gv_aet,     0.0, 0.0, 0.0);

    bsg_camera_apply(cam, v);

    if (!NEAR_EQUAL(v->gv_scale, 7.0, 1e-6))       FAIL("scale not restored");
    if (!NEAR_EQUAL(v->gv_perspective, 20.0, 1e-6)) FAIL("perspective not restored");

    vect_t aet_ref;
    VSET(aet_ref, 10.0, 20.0, 5.0);
    if (!VNEAREQ(v->gv_aet, aet_ref, 1e-6)) FAIL("aet not restored");

    bsg_camera_destroy(cam);
    free_view(v);
    PASS("apply");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 4: NULL guards                                                  */
/* ------------------------------------------------------------------ */

static int
test_null_guards(void)
{
    printf("=== Test 4: null_guards ===\n");

    if (bsg_camera_snapshot(NULL) != NULL) FAIL("snapshot(NULL) should return NULL");
    bsg_camera_apply(NULL, NULL);          /* must not crash */

    struct bsg_view *v = make_view();
    bsg_camera_apply(NULL, v);  /* NULL cam + valid view: no-op */
    free_view(v);

    struct bsg_camera *cam = bsg_camera_create();
    bsg_camera_apply(cam, NULL);  /* valid cam + NULL view: no-op */
    bsg_camera_destroy(cam);

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
    failures += test_snapshot();
    failures += test_apply();
    failures += test_null_guards();

    if (failures == 0)
	printf("RESULT: all Phase 7 camera tests PASSED\n");
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
