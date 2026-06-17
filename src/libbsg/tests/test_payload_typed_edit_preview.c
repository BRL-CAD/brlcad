/*      T E S T _ P A Y L O A D _ T Y P E D _ L I V E _ S O U R C E . C
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
/** @file libbsg/tests/test_payload_typed_edit_preview.c
 *
 * Phase D6 (drawing_modernization) regression tests for BSG_PL_EDIT_PREVIEW and the
 * edit-preview payload contract.
 *
 * Covered scenarios
 * -----------------
 *   test_preview_create        — basic create / get_data / free lifecycle
 *   test_preview_set_ops       — install callbacks; verify they fire
 *   test_preview_revision      — revision monotonicity with revision_cb
 *   test_preview_realize       — realize returns 1 on change, 0 on no-change
 *   test_preview_bounds        — bounds_cb path
 *   test_preview_invalid_bounds
 *                              — invalid bounds outputs do not dispatch
 *   test_preview_pick          — pick_cb path
 *   test_preview_snap          — snap_cb path
 *   test_preview_invalid_snap  — invalid snap inputs/outputs do not dispatch
 *   test_preview_teardown      — owns_preview_ctx free chain
 *   test_preview_replace_owned_ctx
 *                                — replacing owned callback state releases old state
 *   test_preview_partial_ops   — NULL update_cb is handled gracefully
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "vmath.h"
#include "../payload_typed_private.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)


/* ---- Stub callback state ------------------------------------------------ */

struct preview_stub {
    uint64_t revision;       /* value returned by revision_cb */
    int update_calls;
    int bounds_calls;
    int pick_calls;
    int snap_calls;
    int free_calls;
    int update_returns;      /* value returned by update_cb */
    point_t bmin;
    point_t bmax;
    int pick_hit;
    point_t snap_out;
};

static uint64_t
stub_revision_cb(void *preview_ctx)
{
    struct preview_stub *s = (struct preview_stub *)preview_ctx;
    return s->revision;
}

static int
stub_update_cb(void *preview_ctx, struct bsg_view *UNUSED(v))
{
    struct preview_stub *s = (struct preview_stub *)preview_ctx;
    s->update_calls++;
    s->revision++;  /* advance so revision_cb reflects the change */
    return s->update_returns;
}

static int
stub_bounds_cb(void *preview_ctx, point_t *bmin, point_t *bmax)
{
    struct preview_stub *s = (struct preview_stub *)preview_ctx;
    s->bounds_calls++;
    VMOVE(*bmin, s->bmin);
    VMOVE(*bmax, s->bmax);
    return 1;
}

static int
stub_pick_cb(void *preview_ctx, struct bsg_view *UNUSED(v), int UNUSED(x), int UNUSED(y), void *pick_out)
{
    struct preview_stub *s = (struct preview_stub *)preview_ctx;
    s->pick_calls++;
    if (pick_out)
	*(int *)pick_out = s->pick_hit;
    return (s->pick_hit != 0) ? 1 : 0;
}

static int
stub_snap_cb(void *preview_ctx, struct bsg_view *UNUSED(v), const point_t UNUSED(sample_pt), point_t out_pt)
{
    struct preview_stub *s = (struct preview_stub *)preview_ctx;
    s->snap_calls++;
    VMOVE(out_pt, s->snap_out);
    return 1;
}

static void
stub_free_cb(void *preview_ctx)
{
    struct preview_stub *s = (struct preview_stub *)preview_ctx;
    s->free_calls++;
}


/* ---- Tests --------------------------------------------------------------- */

static int
test_preview_create(void)
{
    printf("=== Test 1: BSG_PL_EDIT_PREVIEW create / get_data / free ===\n");

    int ctx_a = 1;
    int ctx_b = 2;
    struct bsg_payload *pl = bsg_payload_edit_preview_create(&ctx_a, &ctx_b);
    if (!pl) FAIL("bsg_payload_edit_preview_create returned NULL");
    if (pl->pl_type != BSG_PL_EDIT_PREVIEW) FAIL("wrong payload type");

    struct bsg_edit_preview_source *ls = bsg_payload_edit_preview_get_data(pl);
    if (!ls) FAIL("bsg_payload_edit_preview_get_data returned NULL");
    if (ls->editor_ctx != &ctx_a) FAIL("editor_ctx mismatch");
    if (ls->aux_ctx != &ctx_b) FAIL("aux_ctx mismatch");
    if (ls->owns_preview_ctx != 0) FAIL("initial owns_preview_ctx should be 0");
    if (ls->last_realized_revision != 0) FAIL("initial last_realized_revision should be 0");

    bsg_payload_free(pl);
    PASS("BSG_PL_EDIT_PREVIEW create / get_data / free");
    return 0;
}


static int
test_preview_set_ops(void)
{
    printf("=== Test 2: bsg_payload_edit_preview_set_ops ===\n");

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    stub.revision = 5;
    stub.update_returns = 1;
    VSET(stub.bmin, -1.0, -2.0, -3.0);
    VSET(stub.bmax, 4.0, 5.0, 6.0);
    stub.pick_hit = 99;
    VSET(stub.snap_out, 0.1, 0.2, 0.3);

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");

    /* set_ops with explicit preview_ctx = &stub */
    int rc = bsg_payload_edit_preview_set_ops(pl,
	    &stub, 0,                /* preview_ctx, !owns */
	    stub_revision_cb,
	    stub_update_cb,
	    stub_bounds_cb,
	    stub_pick_cb,
	    stub_snap_cb,
	    stub_free_cb);
    if (!rc) FAIL("bsg_payload_edit_preview_set_ops returned 0 (expected 1)");

    struct bsg_edit_preview_source *ls = bsg_payload_edit_preview_get_data(pl);
    if (!ls) FAIL("get_data after set_ops returned NULL");
    if (ls->preview_ctx != &stub) FAIL("preview_ctx not stored");
    if (ls->revision_cb != stub_revision_cb) FAIL("revision_cb not stored");
    if (ls->update_cb   != stub_update_cb)   FAIL("update_cb not stored");
    if (ls->bounds_cb   != stub_bounds_cb)   FAIL("bounds_cb not stored");
    if (ls->pick_cb     != stub_pick_cb)     FAIL("pick_cb not stored");
    if (ls->snap_cb     != stub_snap_cb)     FAIL("snap_cb not stored");
    if (ls->free_cb     != stub_free_cb)     FAIL("free_cb not stored");

    uint64_t ops_rev = pl->pl_revision;
    if (!bsg_payload_edit_preview_set_ops(pl,
		&stub, 0,
		stub_revision_cb,
		stub_update_cb,
		stub_bounds_cb,
		stub_pick_cb,
		stub_snap_cb,
		stub_free_cb))
	FAIL("second bsg_payload_edit_preview_set_ops returned 0");
    if (pl->pl_revision <= ops_rev)
	FAIL("set_ops should bump payload revision for callback changes");

    bsg_payload_free(pl);
    /* owns_preview_ctx == 0 so free_cb should NOT have been called */
    if (stub.free_calls != 0) FAIL("free_cb called despite !owns_preview_ctx");

    PASS("bsg_payload_edit_preview_set_ops");
    return 0;
}


static int
test_preview_revision(void)
{
    printf("=== Test 3: revision monotonicity ===\n");

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    stub.revision = 10;
    stub.update_returns = 1;

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");
    bsg_payload_edit_preview_set_ops(pl, &stub, 0,
	    stub_revision_cb, stub_update_cb,
	    NULL, NULL, NULL, NULL);

    uint64_t rev = bsg_payload_edit_preview_revision(pl);
    if (rev != 10) FAIL("initial revision from revision_cb");

    /* Each realize call advances stub.revision by 1 (inside update_cb) */
    int changed = bsg_payload_edit_preview_realize(pl, NULL);
    if (!changed) FAIL("realize should report change (update_cb returns 1)");
    uint64_t rev2 = bsg_payload_edit_preview_revision(pl);
    if (rev2 <= 10) FAIL("revision should advance after realize");

    /* A second realize: stub.revision == 12 after first realize, now 13 */
    bsg_payload_edit_preview_realize(pl, NULL);
    uint64_t rev3 = bsg_payload_edit_preview_revision(pl);
    if (rev3 <= rev2) FAIL("revision not monotonic across second realize");

    bsg_payload_free(pl);
    PASS("revision monotonicity");
    return 0;
}


static int
test_preview_realize(void)
{
    printf("=== Test 4: realize semantics ===\n");

    /* Case A: no update_cb — realize returns 0 */
    {
	struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
	if (!pl) FAIL("create failed (case A)");
	int rc = bsg_payload_edit_preview_realize(pl, NULL);
	if (rc != 0) FAIL("realize with no update_cb should return 0");
	bsg_payload_free(pl);
    }

    /* Case B: update_cb returns 0 and stub.revision stays the same
     * (simulate: nothing changed, revision_cb still returns the same value).
     * In this case realize should return 0. */
    {
	struct preview_stub stub;
	memset(&stub, 0, sizeof(stub));
	stub.revision = 3;
	stub.update_returns = 0;

	/* Use a revision_cb that always returns the same value so that
	 * _edit_preview_payload_update sees no advancement and leaves pl_revision
	 * unchanged. */
	struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
	if (!pl) FAIL("create failed (case B)");

	/* Only set the update_cb (no revision_cb): if update_cb returns 0
	 * and there is no revision_cb, _edit_preview_payload_update should NOT
	 * bump pl_revision and realize should return 0. */
	bsg_payload_edit_preview_set_ops(pl, &stub, 0,
		NULL,             /* no revision_cb */
		stub_update_cb,   /* returns 0 */
		NULL, NULL, NULL, NULL);
	/* update_cb will increment stub.revision inside but that is not
	 * visible to _edit_preview_payload_update because there is no revision_cb. */
	stub.update_returns = 0;
	int rc = bsg_payload_edit_preview_realize(pl, NULL);
	/* update_cb returns 0, no revision_cb → live_rev stays at
	 * last_realized_revision → pl_revision unchanged → rc == 0. */
	if (rc != 0) FAIL("realize with no-change update should return 0");
	bsg_payload_free(pl);
    }

    /* Case C: NULL payload — realize returns -1 */
    {
	int rc = bsg_payload_edit_preview_realize(NULL, NULL);
	if (rc != -1) FAIL("realize(NULL) should return -1");
    }

    PASS("realize semantics");
    return 0;
}


static int
test_preview_bounds(void)
{
    printf("=== Test 5: bounds_cb ===\n");

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    VSET(stub.bmin, -5.0, -6.0, -7.0);
    VSET(stub.bmax,  8.0,  9.0, 10.0);

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");
    bsg_payload_edit_preview_set_ops(pl, &stub, 0,
	    NULL, NULL, stub_bounds_cb, NULL, NULL, NULL);

    point_t bmin = VINIT_ZERO, bmax = VINIT_ZERO;
    int rc = bsg_payload_edit_preview_bounds(pl, &bmin, &bmax);
    if (!rc) FAIL("bounds_cb should return non-zero");
    if (stub.bounds_calls != 1) FAIL("bounds_cb not called exactly once");
    if (!NEAR_EQUAL(bmin[0], -5.0, SMALL_FASTF)) FAIL("bmin[0] wrong");
    if (!NEAR_EQUAL(bmax[2], 10.0, SMALL_FASTF)) FAIL("bmax[2] wrong");

    /* NULL bounds_cb — should return 0 gracefully */
    bsg_payload_edit_preview_set_ops(pl, &stub, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    rc = bsg_payload_edit_preview_bounds(pl, &bmin, &bmax);
    if (rc != 0) FAIL("bounds with no bounds_cb should return 0");

    bsg_payload_free(pl);
    PASS("bounds_cb");
    return 0;
}


static int
test_preview_invalid_bounds(void)
{
    printf("=== Test 5b: invalid bounds output is rejected ===\n");

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    VSET(stub.bmin, -5.0, -6.0, -7.0);
    VSET(stub.bmax,  8.0,  9.0, 10.0);

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");
    bsg_payload_edit_preview_set_ops(pl, &stub, 0,
	    NULL, NULL, stub_bounds_cb, NULL, NULL, NULL);

    point_t bmin = VINIT_ZERO, bmax = VINIT_ZERO;
    int rc = bsg_payload_edit_preview_bounds(pl, NULL, &bmax);
    if (rc != 0) FAIL("bounds with NULL bmin should return 0");
    rc = bsg_payload_edit_preview_bounds(pl, &bmin, NULL);
    if (rc != 0) FAIL("bounds with NULL bmax should return 0");
    if (stub.bounds_calls != 0)
	FAIL("invalid bounds output should not call bounds_cb");

    bsg_payload_free(pl);
    PASS("invalid bounds output is rejected");
    return 0;
}


static int
test_preview_pick(void)
{
    printf("=== Test 6: pick_cb ===\n");

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    stub.pick_hit = 42;

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");
    bsg_payload_edit_preview_set_ops(pl, &stub, 0,
	    NULL, NULL, NULL, stub_pick_cb, NULL, NULL);

    int pick_out = -1;
    int rc = bsg_payload_edit_preview_pick(pl, NULL, 10, 20, &pick_out);
    if (!rc) FAIL("pick_cb hit should return non-zero");
    if (stub.pick_calls != 1) FAIL("pick_cb not called");
    if (pick_out != 42) FAIL("pick_out not set by callback");

    /* NULL pick_cb — should return 0 gracefully */
    bsg_payload_edit_preview_set_ops(pl, &stub, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    rc = bsg_payload_edit_preview_pick(pl, NULL, 0, 0, NULL);
    if (rc != 0) FAIL("pick with no pick_cb should return 0");

    bsg_payload_free(pl);
    PASS("pick_cb");
    return 0;
}


static int
test_preview_snap(void)
{
    printf("=== Test 7: snap_cb ===\n");

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    VSET(stub.snap_out, 1.5, 2.5, 3.5);

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");
    bsg_payload_edit_preview_set_ops(pl, &stub, 0,
	    NULL, NULL, NULL, NULL, stub_snap_cb, NULL);

    point_t sample = VINIT_ZERO;
    point_t snapped = VINIT_ZERO;
    int rc = bsg_payload_edit_preview_snap(pl, NULL, sample, snapped);
    if (!rc) FAIL("snap_cb should return non-zero");
    if (stub.snap_calls != 1) FAIL("snap_cb not called");
    if (!NEAR_EQUAL(snapped[1], 2.5, SMALL_FASTF)) FAIL("snapped[1] wrong");

    /* NULL snap_cb — should return 0 gracefully */
    bsg_payload_edit_preview_set_ops(pl, &stub, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    rc = bsg_payload_edit_preview_snap(pl, NULL, sample, snapped);
    if (rc != 0) FAIL("snap with no snap_cb should return 0");

    bsg_payload_free(pl);
    PASS("snap_cb");
    return 0;
}


static int
test_preview_invalid_snap(void)
{
    printf("=== Test 7b: invalid snap input/output is rejected ===\n");

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    VSET(stub.snap_out, 1.5, 2.5, 3.5);

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");
    bsg_payload_edit_preview_set_ops(pl, &stub, 0,
	    NULL, NULL, NULL, NULL, stub_snap_cb, NULL);

    point_t sample = VINIT_ZERO;
    point_t snapped = VINIT_ZERO;
    int rc = bsg_payload_edit_preview_snap(pl, NULL, NULL, snapped);
    if (rc != 0) FAIL("snap with NULL sample should return 0");
    rc = bsg_payload_edit_preview_snap(pl, NULL, sample, NULL);
    if (rc != 0) FAIL("snap with NULL output should return 0");
    if (stub.snap_calls != 0)
	FAIL("invalid snap arguments should not call snap_cb");

    bsg_payload_free(pl);
    PASS("invalid snap input/output is rejected");
    return 0;
}


static int
test_preview_teardown(void)
{
    printf("=== Test 8: owns_preview_ctx teardown ===\n");

    struct preview_stub *stub = (struct preview_stub *)bu_calloc(1, sizeof(struct preview_stub), "teardown stub");
    stub->revision = 1;

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) {
	bu_free(stub, "teardown stub");
	FAIL("create failed");
    }

    /* owns_preview_ctx = 1 means free_cb is called on teardown */
    bsg_payload_edit_preview_set_ops(pl, stub, 1,
	    stub_revision_cb, NULL, NULL, NULL, NULL,
	    stub_free_cb);

    /* free_cb is not called yet */
    if (stub->free_calls != 0) FAIL("free_cb called too early");

    bsg_payload_free(pl);
    if (stub->free_calls != 1) FAIL("free_cb not called on teardown");

    /* stub was freed by the callback — don't double-free */
    PASS("owns_preview_ctx teardown");
    return 0;
}


static int
test_preview_replace_owned_ctx(void)
{
    printf("=== Test 8b: replacing owned preview context ===\n");

    struct preview_stub first;
    struct preview_stub second;
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    first.revision = 11;
    second.revision = 22;

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl)
	FAIL("create failed");

    if (!bsg_payload_edit_preview_set_ops(pl, &first, 1,
		stub_revision_cb, NULL, NULL, NULL, NULL,
		stub_free_cb))
	FAIL("initial owned set_ops failed");
    if (first.free_calls != 0)
	FAIL("first context freed too early");

    if (!bsg_payload_edit_preview_set_ops(pl, &second, 1,
		stub_revision_cb, NULL, NULL, NULL, NULL,
		stub_free_cb))
	FAIL("replacement owned set_ops failed");
    if (first.free_calls != 1)
	FAIL("replacing owned preview context should free old context");
    if (second.free_calls != 0)
	FAIL("new preview context freed during replacement");

    struct bsg_edit_preview_source *d = bsg_payload_edit_preview_get_data(pl);
    if (!d || d->preview_ctx != &second)
	FAIL("replacement preview context not stored");
    if (bsg_payload_edit_preview_revision(pl) != 22)
	FAIL("replacement revision callback should use new context");

    bsg_payload_free(pl);
    if (second.free_calls != 1)
	FAIL("replacement preview context should be freed on teardown");

    PASS("replacing owned preview context");
    return 0;
}


static int
test_preview_partial_ops(void)
{
    printf("=== Test 9: NULL update_cb is handled gracefully ===\n");

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    stub.revision = 99;

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");

    /* Only revision_cb is set — update is not available */
    bsg_payload_edit_preview_set_ops(pl, &stub, 0,
	    stub_revision_cb, NULL, NULL, NULL, NULL, NULL);

    /* realize without update_cb should return 0 */
    int rc = bsg_payload_edit_preview_realize(pl, NULL);
    if (rc != 0) FAIL("realize with NULL update_cb should return 0");

    /* revision_cb should still be readable */
    uint64_t rev = bsg_payload_edit_preview_revision(pl);
    if (rev != 99) FAIL("revision_cb should still work without update_cb");

    bsg_payload_free(pl);
    PASS("NULL update_cb handled gracefully");
    return 0;
}

static int
test_preview_payload_realization_failure(void)
{
    printf("=== Test 11: edit-preview payload realization failure ===\n");

    {
	struct preview_stub stub;
	memset(&stub, 0, sizeof(stub));
	stub.revision = 3;
	stub.update_returns = -1;

	struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
	if (!pl) FAIL("direct create failed");
	bsg_payload_edit_preview_set_ops(pl, &stub, 0,
		stub_revision_cb, stub_update_cb,
		NULL, NULL, NULL, NULL);

	bsg_payload_edit_preview_mark_inputs_revision(pl, 8, BSG_EDIT_PREVIEW_STALE_VIEW_INPUT_CHANGED);
	int rc = bsg_payload_edit_preview_realize(pl, NULL);
	if (rc != -1)
	    FAIL("direct failed edit-preview update should return -1");
	const struct bsg_payload_realization *state =
	    bsg_payload_realization_state(pl);
	if (!state || state->status != BSG_REALIZE_STATUS_FAILED)
	    FAIL("direct failed edit-preview update should mark payload failed");
	if (state->stale_reason != BSG_PAYLOAD_STALE_UPDATE_FAILED)
	    FAIL("direct failed edit-preview update stale reason");
	if (!bsg_payload_is_stale(pl))
	    FAIL("direct failed realization should remain stale");

	bsg_payload_free(pl);
    }

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    stub.revision = 4;
    stub.update_returns = -1;

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");
    bsg_payload_edit_preview_set_ops(pl, &stub, 0,
	    stub_revision_cb, stub_update_cb,
	    NULL, NULL, NULL, NULL);

    bsg_payload_edit_preview_mark_inputs_revision(pl, 9, BSG_EDIT_PREVIEW_STALE_VIEW_INPUT_CHANGED);
    int rc = bsg_payload_realize(pl, NULL);
    if (rc != -1)
	FAIL("failed edit-preview update should return -1");
    const struct bsg_payload_realization *state = bsg_payload_realization_state(pl);
    if (!state || state->status != BSG_REALIZE_STATUS_FAILED)
	FAIL("failed edit-preview update should mark payload failed");
    if (state->stale_reason != BSG_PAYLOAD_STALE_UPDATE_FAILED)
	FAIL("failed edit-preview update stale reason");
    if (!bsg_payload_is_stale(pl))
	FAIL("failed realization should remain stale");

    bsg_payload_free(pl);
    PASS("edit-preview payload realization failure");
    return 0;
}


static int
test_preview_stale_reason(void)
{
    printf("=== Test 10: edit-preview stale/current explanation ===\n");

    struct preview_stub stub;
    memset(&stub, 0, sizeof(stub));
    stub.revision = 10;
    stub.update_returns = 1;

    struct bsg_payload *pl = bsg_payload_edit_preview_create(NULL, NULL);
    if (!pl) FAIL("create failed");
    bsg_payload_edit_preview_set_ops(pl, &stub, 0,
	    stub_revision_cb, stub_update_cb,
	    NULL, NULL, NULL, NULL);

    if (bsg_payload_edit_preview_is_stale(pl))
	FAIL("fresh edit-preview payload should be current");
    if (bsg_payload_edit_preview_stale_reason(pl) != BSG_EDIT_PREVIEW_STALE_NONE)
	FAIL("fresh stale reason should be NONE");
    const struct bsg_payload_realization *state =
	bsg_payload_realization_state(pl);
    if (!state || state->status != BSG_REALIZE_STATUS_CURRENT ||
	    state->source_revision != 10 ||
	    state->realized_source_revision != 10)
	FAIL("fresh generic realization source revision not synchronized");

    uint64_t mark_rev = pl->pl_revision;
    bsg_payload_edit_preview_mark_source_revision(pl, 12, BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED);
    if (pl->pl_revision <= mark_rev)
	FAIL("source revision mark should bump payload revision");
    if (!bsg_payload_edit_preview_is_stale(pl))
	FAIL("source revision change should make payload stale");
    if (bsg_payload_edit_preview_stale_reason(pl) != BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED)
	FAIL("source stale reason not reported");
    state = bsg_payload_realization_state(pl);
    if (!state || state->status != BSG_REALIZE_STATUS_STALE ||
	    state->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED ||
	    state->source_revision != 12 ||
	    state->realized_source_revision != 10)
	FAIL("source mark should update generic realization stale state");
    if (!BU_STR_EQUAL(bsg_payload_edit_preview_stale_reason_name(
		    bsg_payload_edit_preview_stale_reason(pl)), "source-changed"))
	FAIL("source stale reason name mismatch");

    int rc = bsg_payload_edit_preview_realize(pl, NULL);
    if (rc != 1)
	FAIL("realize should report payload revision change");
    if (bsg_payload_edit_preview_is_stale(pl))
	FAIL("realize should clear stale state");
    state = bsg_payload_realization_state(pl);
    if (!state || state->status != BSG_REALIZE_STATUS_CURRENT ||
	    state->stale_reason != BSG_PAYLOAD_STALE_NONE ||
	    state->realized_source_revision != 12)
	FAIL("source realize should clear generic stale state");
    if (bsg_payload_edit_preview_last_realized_source_revision(pl) != 12)
	FAIL("realized source revision should be stamped");

    mark_rev = pl->pl_revision;
    bsg_payload_edit_preview_mark_inputs_revision(pl, 3, BSG_EDIT_PREVIEW_STALE_VIEW_INPUT_CHANGED);
    if (pl->pl_revision <= mark_rev)
	FAIL("input revision mark should bump payload revision");
    if (!bsg_payload_edit_preview_is_stale(pl))
	FAIL("input revision change should make payload stale");
    if (bsg_payload_edit_preview_stale_reason(pl) != BSG_EDIT_PREVIEW_STALE_VIEW_INPUT_CHANGED)
	FAIL("input stale reason not reported");
    state = bsg_payload_realization_state(pl);
    if (!state || state->status != BSG_REALIZE_STATUS_STALE ||
	    state->stale_reason != BSG_PAYLOAD_STALE_VIEW_INPUT_CHANGED ||
	    state->inputs_revision != 3 ||
	    state->realized_inputs_revision != 0)
	FAIL("input mark should update generic realization stale state");
    (void)bsg_payload_edit_preview_realize(pl, NULL);
    if (bsg_payload_edit_preview_last_realized_inputs_revision(pl) != 3)
	FAIL("realized input revision should be stamped");
    if (bsg_payload_edit_preview_is_stale(pl))
	FAIL("input realize should clear stale state");
    state = bsg_payload_realization_state(pl);
    if (!state || state->status != BSG_REALIZE_STATUS_CURRENT ||
	    state->stale_reason != BSG_PAYLOAD_STALE_NONE ||
	    state->realized_inputs_revision != 3)
	FAIL("input realize should clear generic stale state");

    bsg_payload_free(pl);
    PASS("edit-preview stale/current explanation");
    return 0;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc > 1)
	fprintf(stderr, "Unexpected arguments\n");

    int ret = 0;
    ret |= test_preview_create();
    ret |= test_preview_set_ops();
    ret |= test_preview_revision();
    ret |= test_preview_realize();
    ret |= test_preview_bounds();
    ret |= test_preview_invalid_bounds();
    ret |= test_preview_pick();
    ret |= test_preview_snap();
    ret |= test_preview_invalid_snap();
    ret |= test_preview_teardown();
    ret |= test_preview_replace_owned_ctx();
    ret |= test_preview_partial_ops();
    ret |= test_preview_stale_reason();
    ret |= test_preview_payload_realization_failure();

    return ret;
}
