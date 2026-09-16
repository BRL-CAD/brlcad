/*                 T E S T _ L O D _ C A C H E . C P P
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
/** @file libbv/tests/test_lod_cache.cpp
 *
 * Phase 3-A/3-B regression: unit tests for the bv_mesh_lod bu_cache backend
 * (lod.cpp migration from raw LMDB to bu_cache) and the format-file
 * invalidation mechanism.
 *
 * Tests (no display manager, no .g file required):
 *   1. context_create_destroy  — bv_mesh_lod_context_create / _destroy
 *      with a synthetic filename; verifies the context is non-NULL and
 *      can be destroyed without error.
 *   2. key_roundtrip           — bv_mesh_lod_key_put + bv_mesh_lod_key_get
 *      stores and retrieves a non-zero key for a given mesh name.
 *   3. cache_create_lod        — bv_mesh_lod_cache stores geometry and
 *      bv_mesh_lod_create retrieves a non-NULL struct bv_mesh_lod *.
 *   4. format_invalidation     — simulate an old-format cache by writing a
 *      wrong version number into the format file; verify that a new context
 *      creation sees the mismatch and the name key is gone after the wipe.
 *   5. null_guards             — NULL-pointer inputs to key_get/put and
 *      context_create must not crash and must return sensible values.
 *
 * Usage: test_bv_lod_cache
 *   Returns 0 on success, non-zero on failure.
 */

#include "common.h"

#include <fstream>
#include <cstring>
#include <cstdlib>

#include "vmath.h"
#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "bv/lod.h"

/* ---- tiny assertion helper ------------------------------------------ */
static int g_fail = 0;
#define LODCHECK(cond, msg) \
    do { \
	if (!(cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
	    g_fail++; \
	} \
    } while (0)

/* ---- minimal unit-cube BoT ------------------------------------------ */
/* 8 vertices, 12 triangles (two per face) */
static point_t cube_verts[8] = {
    {0,0,0},{1,0,0},{1,1,0},{0,1,0},
    {0,0,1},{1,0,1},{1,1,1},{0,1,1}
};
static int cube_faces[36] = {
    0,1,2, 0,2,3,   /* bottom */
    4,6,5, 4,7,6,   /* top */
    0,4,5, 0,5,1,   /* front */
    2,6,7, 2,7,3,   /* back */
    0,3,7, 0,7,4,   /* left */
    1,5,6, 1,6,2    /* right */
};

/* ---- helpers -------------------------------------------------------- */

/** Point at a private temp cache directory so we don't collide with real data. */
static char g_cache_dir[MAXPATHLEN] = {0};

static void
set_test_cache(void)
{
    bu_dir(g_cache_dir, MAXPATHLEN, BU_DIR_CURR, "test_lod_cache_dir", NULL);
    bu_mkdir(g_cache_dir);
    bu_setenv("BU_DIR_CACHE", g_cache_dir, 1);
}

static void
clear_test_cache(void)
{
    if (g_cache_dir[0])
	bu_dirclear(g_cache_dir);
}

/* ---- Test 1: context create / destroy ------------------------------ */
static void
test_context_create_destroy(void)
{
    bu_log("=== Test 1: context_create_destroy ===\n");

    struct bv_mesh_lod_context *ctx =
	bv_mesh_lod_context_create("test_model_ABCDEFGH.g");
    LODCHECK(ctx != NULL, "bv_mesh_lod_context_create returned non-NULL");

    if (ctx) {
	bv_mesh_lod_context_destroy(ctx);
	bu_log("  PASS: create/destroy cycle complete\n");
    }
}

/* ---- Test 2: key round-trip ---------------------------------------- */
static void
test_key_roundtrip(void)
{
    bu_log("=== Test 2: key_roundtrip ===\n");

    struct bv_mesh_lod_context *ctx =
	bv_mesh_lod_context_create("test_model_key_rt.g");
    if (!ctx) {
	bu_log("FAIL: bv_mesh_lod_context_create failed — skipping\n");
	g_fail++;
	return;
    }

    unsigned long long key_in = 0xDEADBEEF12345678ULL;
    int ret = bv_mesh_lod_key_put(ctx, "my_mesh_ABCDEFGHIJ", key_in);
    LODCHECK(ret == 0, "bv_mesh_lod_key_put returned 0 (success)");

    unsigned long long key_out = bv_mesh_lod_key_get(ctx, "my_mesh_ABCDEFGHIJ");
    LODCHECK(key_out == key_in, "bv_mesh_lod_key_get returned stored key");

    /* name not present must return 0 */
    unsigned long long missing = bv_mesh_lod_key_get(ctx, "no_such_mesh_XYZ");
    LODCHECK(missing == 0, "key_get for unknown name returns 0");

    if (ret == 0 && key_out == key_in && missing == 0)
	bu_log("  PASS: key round-trip\n");

    bv_mesh_lod_context_destroy(ctx);
}

/* ---- Test 3: cache + create round-trip ----------------------------- */
static void
test_cache_create_lod(void)
{
    bu_log("=== Test 3: cache_create_lod ===\n");

    struct bv_mesh_lod_context *ctx =
	bv_mesh_lod_context_create("test_model_cache_create.g");
    if (!ctx) {
	bu_log("FAIL: bv_mesh_lod_context_create failed — skipping\n");
	g_fail++;
	return;
    }

    /* Store LoD data for the unit cube. */
    unsigned long long key = bv_mesh_lod_cache(
	ctx,
	cube_verts, 8,
	NULL,   /* no normals */
	cube_faces, 12,
	0,      /* user key — 0 means auto-assign */
	0.66);  /* face-count threshold ratio */

    LODCHECK(key != 0, "bv_mesh_lod_cache returns non-zero key for unit cube");

    if (key) {
	struct bv_mesh_lod *lod = bv_mesh_lod_create(ctx, key);
	LODCHECK(lod != NULL, "bv_mesh_lod_create returns non-NULL for cached key");

	if (lod) {
	    bv_mesh_lod_destroy(lod);
	    bu_log("  PASS: cache + create + free cycle\n");
	}
    }

    bv_mesh_lod_context_destroy(ctx);
}

/* ---- Test 4: format-file invalidation ------------------------------ */
/*
 * Strategy:
 *   a) Create context, store a name/key mapping.
 *   b) Destroy context.
 *   c) Overwrite the .POPLoD/format file with version 0 (invalid).
 *   d) Create a new context — this triggers the mismatch check; the
 *      cache directory should be cleared.
 *   e) The previously stored name/key mapping must no longer exist.
 */
static void
test_format_invalidation(void)
{
    bu_log("=== Test 4: format_invalidation ===\n");

    /* ---- step a/b: prime the cache ---------------------------------- */
    {
	struct bv_mesh_lod_context *ctx =
	    bv_mesh_lod_context_create("test_model_format_inv.g");
	if (!ctx) {
	    bu_log("FAIL: context_create for prime step\n");
	    g_fail++;
	    return;
	}
	bv_mesh_lod_key_put(ctx, "sentinel_mesh_ABCDEFGHI", 0xCAFEBABEULL);
	bv_mesh_lod_context_destroy(ctx);
    }

    /* ---- step c: corrupt the format file ---------------------------- */
    {
	char fmt_path[MAXPATHLEN];
	bu_dir(fmt_path, MAXPATHLEN, BU_DIR_CACHE, ".POPLoD", "format", NULL);
	FILE *fp = fopen(fmt_path, "w");
	if (fp) {
	    fprintf(fp, "1\n");   /* version 1 — older than CACHE_CURRENT_FORMAT (2), forces invalidation */
	    fclose(fp);
	    bu_log("  Corrupted format file at %s\n", fmt_path);
	} else {
	    bu_log("WARNING: could not write format file at %s — "
		   "format_invalidation test inconclusive\n", fmt_path);
	    /* Not a hard failure — the file may not exist yet on a fresh run */
	}
    }

    /* ---- step d/e: verify key is gone after new context ------------- */
    {
	struct bv_mesh_lod_context *ctx =
	    bv_mesh_lod_context_create("test_model_format_inv.g");
	if (!ctx) {
	    bu_log("FAIL: context_create after format corruption\n");
	    g_fail++;
	    return;
	}
	unsigned long long key = bv_mesh_lod_key_get(ctx, "sentinel_mesh_ABCDEFGHI");
	LODCHECK(key == 0,
		 "key vanished from name cache after format-version mismatch wipe");
	if (key == 0)
	    bu_log("  PASS: old cache entries cleared on format mismatch\n");
	bv_mesh_lod_context_destroy(ctx);
    }
}

/* ---- Test 5: null guards ------------------------------------------- */
static void
test_null_guards(void)
{
    bu_log("=== Test 5: null_guards ===\n");
    int fails_before = g_fail;

    /* NULL context input to key_get/put */
    unsigned long long k = bv_mesh_lod_key_get(NULL, "anything_AAAA");
    LODCHECK(k == 0, "key_get(NULL ctx) returns 0");

    int r = bv_mesh_lod_key_put(NULL, "anything_AAAA", 42ULL);
    LODCHECK(r != 0, "key_put(NULL ctx) returns failure");

    /* NULL filename to context_create */
    struct bv_mesh_lod_context *ctx = bv_mesh_lod_context_create(NULL);
    LODCHECK(ctx == NULL, "context_create(NULL) returns NULL");

    /* NULL key to bv_mesh_lod_create */
    struct bv_mesh_lod_context *ctx2 =
	bv_mesh_lod_context_create("test_model_null_guards.g");
    if (ctx2) {
	struct bv_mesh_lod *lod = bv_mesh_lod_create(ctx2, 0);
	LODCHECK(lod == NULL, "bv_mesh_lod_create(key=0) returns NULL");
	bv_mesh_lod_context_destroy(ctx2);
    }

    if (g_fail == fails_before)
	bu_log("  PASS: all null guards\n");
}

/* -------------------------------------------------------------------- */
int
main(int UNUSED(argc), char *argv[])
{
    bu_setprogname(argv[0]);

    set_test_cache();

    test_context_create_destroy();
    test_key_roundtrip();
    test_cache_create_lod();
    test_format_invalidation();
    test_null_guards();

    clear_test_cache();

    if (g_fail) {
	bu_log("RESULT: %d failure(s)\n", g_fail);
	return 1;
    }
    bu_log("RESULT: all LoD cache tests PASSED\n");
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
