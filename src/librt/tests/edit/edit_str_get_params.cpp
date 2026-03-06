/*          E D I T _ S T R _ G E T _ P A R A M S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file edit_str_get_params.cpp
 *
 * Unit tests for:
 *   1. rt_edit_set_str()  / struct rt_edit e_str[] / e_nstr
 *   2. EDOBJ[type].ft_edit_get_params()  for TOR and ELL
 */

#include "common.h"

#include <string.h>
#include <math.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/magic.h"
#include "raytrace.h"
#include "rt/functab.h"   /* EDOBJ */
#include "rt/geom.h"      /* rt_tor_internal, rt_ell_internal */

#define ECMD_TOR_R1 1021
#define ECMD_TOR_R2 1022

#define ECMD_ELL_SCALE_A 3039
#define ECMD_ELL_SCALE_B 3040
#define ECMD_ELL_SCALE_C 3041


/* ======================================================================
 * Helpers
 * ====================================================================== */

static int fail_count = 0;

#define PASS(msg) bu_log("PASS: %s\n", (msg))
#define FAIL(msg) do { bu_log("FAIL: %s\n", (msg)); fail_count++; } while (0)
#define CHECK(cond, msg) do { if (cond) { PASS(msg); } else { FAIL(msg); } } while (0)


/* ======================================================================
 * Test 1: rt_edit_set_str() basic behaviour
 * ====================================================================== */

static void
test_set_str(void)
{
    struct rt_edit *s;

    /* rt_edit_create with NULL path/dbip gives a bare struct */
    s = rt_edit_create(NULL, NULL, NULL, NULL);
    if (!s) {
	FAIL("rt_edit_create returned NULL");
	return;
    }

    /* Initially no strings */
    CHECK(s->e_nstr == 0, "set_str: e_nstr initially 0");

    /* Set index 0 */
    rt_edit_set_str(s, 0, "hello");
    CHECK(s->e_nstr == 1, "set_str: e_nstr == 1 after setting index 0");
    CHECK(strcmp(s->e_str[0], "hello") == 0,
	  "set_str: e_str[0] == 'hello'");

    /* Set index 2 (skip index 1) */
    rt_edit_set_str(s, 2, "world");
    CHECK(s->e_nstr == 3, "set_str: e_nstr == 3 after setting index 2");
    CHECK(strcmp(s->e_str[2], "world") == 0,
	  "set_str: e_str[2] == 'world'");

    /* Overwrite index 0 */
    rt_edit_set_str(s, 0, "replaced");
    CHECK(strcmp(s->e_str[0], "replaced") == 0,
	  "set_str: overwrite e_str[0]");
    CHECK(s->e_nstr == 3, "set_str: e_nstr unchanged after overwrite");

    /* NULL str: should be silently ignored */
    rt_edit_set_str(s, 0, NULL);
    CHECK(strcmp(s->e_str[0], "replaced") == 0,
	  "set_str: NULL str ignored (value unchanged)");

    /* Out-of-range index: should be silently ignored */
    rt_edit_set_str(s, RT_EDIT_MAXSTR, "overflow");
    CHECK(s->e_nstr == 3, "set_str: out-of-range index ignored");

    /* Long string: should be silently truncated (no crash) */
    /* Size to more than RT_EDIT_MAXSTR_LEN to guarantee truncation
     * regardless of any future changes to RT_EDIT_MAXSTR_LEN. */
    char longbuf[RT_EDIT_MAXSTR_LEN * 2 + 4];
    memset(longbuf, 'x', sizeof(longbuf) - 1);
    longbuf[sizeof(longbuf) - 1] = '\0';
    rt_edit_set_str(s, 1, longbuf);
    CHECK(strlen(s->e_str[1]) == RT_EDIT_MAXSTR_LEN - 1,
	  "set_str: long string truncated to RT_EDIT_MAXSTR_LEN-1");

    rt_edit_destroy(s);
    PASS("set_str: rt_edit_destroy after set_str");
}


/* ======================================================================
 * Test 2: EDOBJ[TOR].ft_edit_get_params
 * ====================================================================== */

static void
test_get_params_tor(void)
{
    CHECK(EDOBJ[ID_TOR].ft_edit_get_params != NULL,
	  "TOR ft_edit_get_params: slot is non-NULL");
    if (!EDOBJ[ID_TOR].ft_edit_get_params)
	return;

    /* Build a minimal rt_tor_internal directly (no database needed). */
    struct rt_tor_internal *tor =
	(struct rt_tor_internal *)bu_malloc(sizeof(struct rt_tor_internal), "tor");
    tor->magic = RT_TOR_INTERNAL_MAGIC;
    VSET(tor->v, 0, 0, 0);
    VSET(tor->h, 0, 0, 1);
    tor->r_a = 100.0;   /* major radius = 100 mm */
    tor->r_h = 25.0;    /* minor radius = 25 mm  */
    VSET(tor->a, 100, 0, 0);
    VSET(tor->b, 0, 100, 0);
    tor->r_b = 100.0;

    struct rt_edit *s = rt_edit_create(NULL, NULL, NULL, NULL);
    if (!s) {
	FAIL("TOR get_params: rt_edit_create returned NULL");
	bu_free(tor, "tor");
	return;
    }

    /* Attach the internal to the edit struct (no database, so we do it
     * manually; caller owns the memory until destroy cleans it up). */
    s->es_int.idb_type     = ID_TOR;
    s->es_int.idb_meth     = &OBJ[ID_TOR];
    s->es_int.idb_ptr      = tor;
    s->es_int.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    s->es_int.idb_minor_type = ID_TOR;
    s->base2local = 1.0;
    s->local2base = 1.0;

    fastf_t vals[RT_EDIT_MAXPARA];
    memset(vals, 0, sizeof(vals));

    /* Query R1 */
    int n = EDOBJ[ID_TOR].ft_edit_get_params(s, ECMD_TOR_R1, vals);
    CHECK(n == 1, "TOR get_params(ECMD_TOR_R1): returned 1");
    CHECK(fabs(vals[0] - 100.0) < 1e-6,
	  "TOR get_params(ECMD_TOR_R1): vals[0] == 100.0");
    bu_log("  TOR R1 = %g (expected 100)\n", vals[0]);

    /* Query R2 */
    memset(vals, 0, sizeof(vals));
    n = EDOBJ[ID_TOR].ft_edit_get_params(s, ECMD_TOR_R2, vals);
    CHECK(n == 1, "TOR get_params(ECMD_TOR_R2): returned 1");
    CHECK(fabs(vals[0] - 25.0) < 1e-6,
	  "TOR get_params(ECMD_TOR_R2): vals[0] == 25.0");
    bu_log("  TOR R2 = %g (expected 25)\n", vals[0]);

    /* Unknown cmd_id should return 0 */
    memset(vals, 0, sizeof(vals));
    n = EDOBJ[ID_TOR].ft_edit_get_params(s, 99999, vals);
    CHECK(n == 0, "TOR get_params(unknown cmd_id): returned 0");

    /* NULL vals should return -1 */
    n = EDOBJ[ID_TOR].ft_edit_get_params(s, ECMD_TOR_R1, NULL);
    CHECK(n == -1, "TOR get_params(NULL vals): returned -1");

    /* Detach internal before destroy so we free it ourselves. */
    RT_DB_INTERNAL_INIT(&s->es_int);
    rt_edit_destroy(s);
    bu_free(tor, "tor");
}


/* ======================================================================
 * Test 3: EDOBJ[ELL].ft_edit_get_params
 * ====================================================================== */

static void
test_get_params_ell(void)
{
    CHECK(EDOBJ[ID_ELL].ft_edit_get_params != NULL,
	  "ELL ft_edit_get_params: slot is non-NULL");
    if (!EDOBJ[ID_ELL].ft_edit_get_params)
	return;

    struct rt_ell_internal *ell =
	(struct rt_ell_internal *)bu_malloc(sizeof(struct rt_ell_internal), "ell");
    ell->magic = RT_ELL_INTERNAL_MAGIC;
    VSET(ell->v, 0, 0, 0);
    VSET(ell->a, 50, 0, 0);   /* |A| = 50 */
    VSET(ell->b, 0, 30, 0);   /* |B| = 30 */
    VSET(ell->c, 0, 0, 20);   /* |C| = 20 */

    struct rt_edit *s = rt_edit_create(NULL, NULL, NULL, NULL);
    if (!s) {
	FAIL("ELL get_params: rt_edit_create returned NULL");
	bu_free(ell, "ell");
	return;
    }

    s->es_int.idb_type      = ID_ELL;
    s->es_int.idb_meth      = &OBJ[ID_ELL];
    s->es_int.idb_ptr       = ell;
    s->es_int.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    s->es_int.idb_minor_type = ID_ELL;
    s->base2local = 1.0;
    s->local2base = 1.0;

    fastf_t vals[RT_EDIT_MAXPARA];
    memset(vals, 0, sizeof(vals));

    int n = EDOBJ[ID_ELL].ft_edit_get_params(s, ECMD_ELL_SCALE_A, vals);
    CHECK(n == 1, "ELL get_params(ECMD_ELL_SCALE_A): returned 1");
    CHECK(fabs(vals[0] - 50.0) < 1e-6,
	  "ELL get_params(ECMD_ELL_SCALE_A): vals[0] == 50");
    bu_log("  ELL |A| = %g (expected 50)\n", vals[0]);

    memset(vals, 0, sizeof(vals));
    n = EDOBJ[ID_ELL].ft_edit_get_params(s, ECMD_ELL_SCALE_B, vals);
    CHECK(n == 1, "ELL get_params(ECMD_ELL_SCALE_B): returned 1");
    CHECK(fabs(vals[0] - 30.0) < 1e-6,
	  "ELL get_params(ECMD_ELL_SCALE_B): vals[0] == 30");

    memset(vals, 0, sizeof(vals));
    n = EDOBJ[ID_ELL].ft_edit_get_params(s, ECMD_ELL_SCALE_C, vals);
    CHECK(n == 1, "ELL get_params(ECMD_ELL_SCALE_C): returned 1");
    CHECK(fabs(vals[0] - 20.0) < 1e-6,
	  "ELL get_params(ECMD_ELL_SCALE_C): vals[0] == 20");

    RT_DB_INTERNAL_INIT(&s->es_int);
    rt_edit_destroy(s);
    bu_free(ell, "ell");
}


/* ======================================================================
 * main
 * ====================================================================== */

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);

    if (argc > 1) {
	bu_log("Usage: %s\n", argv[0]);
	return 1;
    }

    bu_log("=== Test: rt_edit_set_str / e_str / e_nstr ===\n");
    test_set_str();

    bu_log("=== Test: ft_edit_get_params (TOR) ===\n");
    test_get_params_tor();

    bu_log("=== Test: ft_edit_get_params (ELL) ===\n");
    test_get_params_ell();

    if (fail_count) {
	bu_log("edit_str_get_params: %d test(s) FAILED\n", fail_count);
	return 1;
    }

    bu_log("edit_str_get_params: all tests PASSED\n");
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
