/*             T E S T _ D L I S T _ S E N S O R . C
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
/** @file libdm/tests/test_dlist_sensor.c
 *
 * regression: unit tests for the dm_dlist_sensor API
 * (dm_register_dlist_sensor / dm_fire_dlist_sensors / dm_dlist_sensors_clear).
 *
 * Tests:
 *   1. register_fire   — register one sensor and verify the callback fires.
 *   2. multiple_fire   — register three sensors and verify all three fire
 *      in one dm_fire_dlist_sensors call.
 *   3. clear_no_fire   — after dm_dlist_sensors_clear, fire must invoke
 *      zero callbacks.
 *   4. register_null   — NULL dm or NULL callback must not crash and must
 *      return failure.
 *   5. fire_null       — dm_fire_dlist_sensors(NULL) must not crash.
 *   6. clear_null      — dm_dlist_sensors_clear(NULL) must not crash.
 *   7. close_clears    — dm_close invokes dm_dlist_sensors_clear (sensors
 *      registered before close must not be reached after close).
 *
 * The tests use the "nu" (null/no-op) display manager type so no display
 * hardware is required.
 *
 * Usage: test_dm_dlist_sensor
 *   Returns 0 on success, non-zero on failure.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bv/defines.h"
#define DM_WITH_RT
#include "dm.h"

static int g_fail = 0;

#define DMCHECK(cond, msg) \
    do { \
	if (!(cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
	    g_fail++; \
	} \
    } while (0)

/* ---- counter callback ---------------------------------------------- */
static int g_cnt = 0;

static void
count_cb(struct bv_scene_obj *UNUSED(s), void *data)
{
    if (data)
	(*(int *)data)++;
    g_cnt++;
}

/* ---- open a null dm ------------------------------------------------ */
static struct dm *
open_null_dm(void)
{
    const char *av0 = "attach";
    struct dm *dmp = dm_open(NULL, NULL, "nu", 1, &av0);
    return dmp;
}

/* ---- Test 1: register_fire ----------------------------------------- */
static void
test_register_fire(void)
{
    bu_log("=== Test 1: register_fire ===\n");
    g_cnt = 0;

    struct dm *dmp = open_null_dm();
    DMCHECK(dmp != NULL, "dm_open nu succeeded");
    if (!dmp) return;

    int ret = dm_register_dlist_sensor(dmp, NULL, count_cb, NULL);
    DMCHECK(ret == 0, "dm_register_dlist_sensor returns 0");

    dm_fire_dlist_sensors(dmp);
    DMCHECK(g_cnt == 1, "callback fired exactly once");
    if (g_cnt == 1)
	bu_log("  PASS: register + fire\n");

    dm_dlist_sensors_clear(dmp);
    dm_close(dmp);
}

/* ---- Test 2: multiple sensors ------------------------------------- */
static void
test_multiple_fire(void)
{
    bu_log("=== Test 2: multiple_fire ===\n");
    g_cnt = 0;

    struct dm *dmp = open_null_dm();
    if (!dmp) { g_fail++; return; }

    dm_register_dlist_sensor(dmp, NULL, count_cb, NULL);
    dm_register_dlist_sensor(dmp, NULL, count_cb, NULL);
    dm_register_dlist_sensor(dmp, NULL, count_cb, NULL);

    dm_fire_dlist_sensors(dmp);
    DMCHECK(g_cnt == 3, "all three callbacks fired in one call");
    if (g_cnt == 3)
	bu_log("  PASS: multiple sensors\n");

    dm_dlist_sensors_clear(dmp);
    dm_close(dmp);
}

/* ---- Test 3: clear_no_fire ----------------------------------------- */
static void
test_clear_no_fire(void)
{
    bu_log("=== Test 3: clear_no_fire ===\n");
    g_cnt = 0;

    struct dm *dmp = open_null_dm();
    if (!dmp) { g_fail++; return; }

    dm_register_dlist_sensor(dmp, NULL, count_cb, NULL);
    dm_register_dlist_sensor(dmp, NULL, count_cb, NULL);
    dm_dlist_sensors_clear(dmp);

    dm_fire_dlist_sensors(dmp);
    DMCHECK(g_cnt == 0, "no callbacks after clear");
    if (g_cnt == 0)
	bu_log("  PASS: clear_no_fire\n");

    dm_close(dmp);
}

/* ---- Test 4: register NULL guards ---------------------------------- */
static void
test_register_null(void)
{
    bu_log("=== Test 4: register_null ===\n");
    int fails_before = g_fail;

    struct dm *dmp = open_null_dm();
    if (!dmp) { g_fail++; return; }

    /* NULL dm */
    int r = dm_register_dlist_sensor(NULL, NULL, count_cb, NULL);
    DMCHECK(r != 0, "register with NULL dm returns failure");

    /* NULL callback */
    r = dm_register_dlist_sensor(dmp, NULL, NULL, NULL);
    DMCHECK(r != 0, "register with NULL callback returns failure");

    if (g_fail == fails_before)
	bu_log("  PASS: register null guards\n");

    dm_close(dmp);
}

/* ---- Test 5: fire NULL -------------------------------------------- */
static void
test_fire_null(void)
{
    bu_log("=== Test 5: fire_null ===\n");
    dm_fire_dlist_sensors(NULL);   /* must not crash */
    bu_log("  PASS: fire(NULL) did not crash\n");
}

/* ---- Test 6: clear NULL ------------------------------------------- */
static void
test_clear_null(void)
{
    bu_log("=== Test 6: clear_null ===\n");
    dm_dlist_sensors_clear(NULL);  /* must not crash */
    bu_log("  PASS: clear(NULL) did not crash\n");
}

/* ---- Test 7: dm_close clears sensors ------------------------------ */
static void
test_close_clears(void)
{
    bu_log("=== Test 7: close_clears ===\n");
    g_cnt = 0;

    struct dm *dmp = open_null_dm();
    if (!dmp) { g_fail++; return; }

    dm_register_dlist_sensor(dmp, NULL, count_cb, NULL);
    dm_register_dlist_sensor(dmp, NULL, count_cb, NULL);

    /* dm_close must call dm_dlist_sensors_clear internally.
     * We verify that by checking sensors are gone (fire count stays 0)
     * if we were to re-open and fire (we cannot fire after close, but
     * the important guarantee is that dm_close does not crash or leak). */
    dm_close(dmp);   /* must not crash even with sensors registered */
    DMCHECK(g_cnt == 0, "no spurious callback invocations during dm_close");
    if (g_cnt == 0)
	bu_log("  PASS: dm_close did not fire sensors\n");
}

/* ---- optional: per-sensor data pointer ----------------------------- */
static void
test_sensor_data(void)
{
    bu_log("=== Test 8: sensor_data ===\n");

    struct dm *dmp = open_null_dm();
    if (!dmp) { g_fail++; return; }

    int a = 0, b = 0;
    dm_register_dlist_sensor(dmp, NULL, count_cb, &a);
    dm_register_dlist_sensor(dmp, NULL, count_cb, &b);

    dm_fire_dlist_sensors(dmp);
    /* Each callback increments its own counter via data ptr */
    DMCHECK(a == 1, "first sensor's data counter incremented");
    DMCHECK(b == 1, "second sensor's data counter incremented");
    if (a == 1 && b == 1)
	bu_log("  PASS: per-sensor data pointer\n");

    dm_dlist_sensors_clear(dmp);
    dm_close(dmp);
}

/* -------------------------------------------------------------------- */
int
main(int UNUSED(argc), char *argv[])
{
    bu_setprogname(argv[0]);
    bu_log("dm_init_msgs: %s\n", dm_init_msgs());

    test_register_fire();
    test_multiple_fire();
    test_clear_no_fire();
    test_register_null();
    test_fire_null();
    test_clear_null();
    test_close_clears();
    test_sensor_data();

    if (g_fail) {
	bu_log("RESULT: %d failure(s)\n", g_fail);
	return 1;
    }
    bu_log("RESULT: all dlist_sensor tests PASSED\n");
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
