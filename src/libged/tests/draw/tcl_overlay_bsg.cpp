/*              T C L _ O V E R L A Y _ B S G . C P P
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
/** @file tcl_overlay_bsg.cpp
 *
 * Phase T1/T4 (drawing_stack_modernization) — structural regression test.
 *
 * Verifies that the BSG view-scope feature lifecycle used by the T1
 * sync helpers (arrows, lines, labels, axes, polygons) behaves
 * correctly:
 *
 *   • bsg_feature_create_lines creates a named local-scope feature
 *   • bsg_feature_find         locates it by name
 *   • bsg_feature_visit        reports stable feature records
 *   • bsg_feature_remove       deletes it; subsequent find returns NULL
 *   • dm_draw_objs              can be called headlessly (NULL dmp) without
 *                               crashing — the T2-final call site in
 *                               go_refresh_draw must be no-op safe.
 *
 * This test does NOT attach a display manager.  It is intentionally headless
 * so that the BSG feature lifecycle contract is enforced independently of
 * any rendering backend.
 *
 * Usage: ged_test_tcl_overlay_bsg <directory-with-moss.g>
 */

#include "common.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>

#include <bu.h>
#include <bsg.h>
#include "bsg/feature.h"
#include "bsg/geometry.h"
#include "bsg/tcl_data.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg/draw_source.h"
#include <dm.h>
#include <ged.h>

#define ASSERT(cond) do { \
    nchecks++; \
    if (!(cond)) { \
	bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
	nfails++; \
    } \
} while (0)

static int nchecks = 0;
static int nfails  = 0;

/* Visitor that counts features reached via bsg_feature_visit. */
static int
_count_visit_cb(bsg_feature_ref /*ref*/, const struct bsg_feature_record *record, void *ud)
{
    int *cnt = (int *)ud;
    ASSERT(record != NULL);
    (*cnt)++;
    return 1;
}

int
main(int ac, char *av[])
{
    bu_setprogname(av[0]);

    if (ac != 2) {
	bu_exit(EXIT_FAILURE, "Usage: %s <directory-containing-moss.g>\n", av[0]);
    }

    /* ------------------------------------------------------------------ *
     * Open a headless GED session.                                        *
     * ------------------------------------------------------------------ */
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct bu_vls moss = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&moss, "%s/moss.g", av[1]);
    char tmpname[MAXPATHLEN];
    FILE *fp = bu_temp_file(tmpname, MAXPATHLEN);
    if (!fp) {
	bu_log("failed to create temp db path: %s\n", std::strerror(errno));
	bu_vls_free(&moss);
	bu_vls_free(&fname);
	return 1;
    }
    fclose(fp);
    bu_vls_sprintf(&fname, "%s", tmpname);
    {
	/* This test is headless, but ged_open still requires a valid .g file. */
	std::ifstream orig(bu_vls_cstr(&moss), std::ios::binary);
	std::ofstream tmpg(bu_vls_cstr(&fname), std::ios::binary);
	if (!orig.good() || !tmpg.good()) {
	    bu_log("failed to prepare tmp db: %s\n", bu_vls_cstr(&fname));
	    bu_vls_free(&moss);
	    bu_vls_free(&fname);
	    return 1;
	}
	tmpg << orig.rdbuf();
	orig.close();
	tmpg.close();
    }
    struct ged *gedp = ged_open("db", bu_vls_cstr(&fname), 1);
    bu_vls_free(&moss);
    if (!gedp) {
	bu_log("ged_open failed\n");
	bu_file_delete(bu_vls_cstr(&fname));
	bu_vls_free(&fname);
	return 1;
    }

    bu_log("=== TCL feature BSG lifecycle ===\n");

    struct bsg_view *v = gedp->ged_gvp;
    ASSERT(v != NULL);
    ASSERT(bsg_view_scene_attached(v));

    /* ------------------------------------------------------------------ *
 * [1] create: bsg_feature_create_lines must return a non-NULL scene  *
     *     ref and register it in the local scope.                        *
     * ------------------------------------------------------------------ */
    bu_log("[1] bsg_feature_create_lines...\n");
    const char *tname = "_tcl_test_feature";
    bsg_feature_ref obj_ref = bsg_feature_create_lines(v, tname, 1 /*local*/);
    ASSERT(!bsg_feature_ref_is_null(obj_ref));
    if (bsg_feature_ref_is_null(obj_ref)) goto done;

    /* ------------------------------------------------------------------ *
     * [2] find: bsg_feature_find must locate the object by name.         *
     * ------------------------------------------------------------------ */
    bu_log("[2] bsg_feature_find...\n");
    {
	bsg_feature_ref found_ref = bsg_feature_find(v, tname);
	ASSERT(!bsg_feature_ref_is_null(found_ref));
	ASSERT(found_ref.token == obj_ref.token);
	struct bsg_feature_record record;
	ASSERT(bsg_feature_record_get(found_ref, &record) == 1);
	ASSERT(record.family == BSG_FEATURE_LINES);
	ASSERT(record.scope == BSG_FEATURE_SCOPE_LOCAL);
    }

    /* ------------------------------------------------------------------ *
     * [3] feature geometry: add typed line data and verify non-empty.     *
     * ------------------------------------------------------------------ */
    bu_log("[3] bsg_feature_points_replace...\n");
    {
	point_t points[2] = {{0, 0, 0}, {1, 0, 0}};
	int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
	ASSERT(bsg_feature_points_replace(obj_ref, BSG_FEATURE_LINES, points, cmds, 2) == 1);
	point_t *copied_points = NULL;
	int *copied_cmds = NULL;
	size_t point_count = 0;
	ASSERT(bsg_feature_points_copy(obj_ref, &copied_points, &copied_cmds, &point_count) == 1);
	ASSERT(point_count == 2);
	if (point_count == 2 && copied_points && copied_cmds) {
	    ASSERT(VNEAR_EQUAL(copied_points[0], points[0], VUNITIZE_TOL));
	    ASSERT(VNEAR_EQUAL(copied_points[1], points[1], VUNITIZE_TOL));
	    ASSERT(copied_cmds[0] == BSG_GEOMETRY_LINE_MOVE);
	    ASSERT(copied_cmds[1] == BSG_GEOMETRY_LINE_DRAW);
	}
	if (copied_points)
	    bu_free(copied_points, "feature test copied points");
	if (copied_cmds)
	    bu_free(copied_cmds, "feature test copied commands");
    }

    /* ------------------------------------------------------------------ *
     * [4] set_color / set_line_width / set_visible typed setters.        *
     * ------------------------------------------------------------------ */
    bu_log("[4] typed setters...\n");
    {
	bsg_feature_set_color(obj_ref, 255, 128, 0);
	bsg_feature_set_line_width(obj_ref, 2);
	bsg_feature_set_visible(obj_ref, 1);
	struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
	ASSERT(bsg_feature_style_get(obj_ref, &style) == 1);
	ASSERT(style.color_valid == 1);
	ASSERT(style.color[0] == 255 && style.color[1] == 128 && style.color[2] == 0);
	ASSERT(style.line_width == 2);
	ASSERT(style.visible == 1);
    }

    /* ------------------------------------------------------------------ *
     * [5] visit: bsg_feature_visit with BSG_FEATURE_SCOPE_LOCAL must      *
     *     reach at least the one object we created.                      *
     * ------------------------------------------------------------------ */
    bu_log("[5] bsg_feature_visit (local scope)...\n");
    {
	int cnt = 0;
	bsg_feature_visit(v, BSG_FEATURE_SCOPE_LOCAL, _count_visit_cb, &cnt);
	ASSERT(cnt >= 1);
    }

    /* ------------------------------------------------------------------ *
     * [6] dm_draw_objs headless: must not crash when dmp is NULL.        *
     *     This mirrors the T2-final call in go_refresh_draw.             *
     * ------------------------------------------------------------------ */
    bu_log("[6] dm_draw_objs headless (NULL dmp)...\n");
    {
	struct dm *saved_dmp = (struct dm *)v->dmp;
	v->dmp = NULL;
	dm_draw_objs(v);   /* must be a no-op, not a crash */
	v->dmp = saved_dmp;
    }

    /* ------------------------------------------------------------------ *
     * [7] remove: bsg_feature_remove must delete the named object;       *
     *     a subsequent find must return NULL.                            *
     * ------------------------------------------------------------------ */
    bu_log("[7] bsg_feature_remove...\n");
    {
	int r = bsg_feature_remove(v, tname);
	ASSERT(r == 1);
	ASSERT(bsg_feature_ref_is_null(bsg_feature_find(v, tname)));
    }

    /* ------------------------------------------------------------------ *
     * [8] remove idempotency: removing a non-existent name is safe.      *
     * ------------------------------------------------------------------ */
    bu_log("[8] remove idempotency...\n");
    {
	int r = bsg_feature_remove(v, "_tcl_test_nonexistent");
	(void)r; /* return value may differ by impl; the call must not crash */
    }

    /* ------------------------------------------------------------------ *
     * [9] multi-object: create data/sdata objects for all four overlay *
     *     slots (arrows, lines, labels, polygons) and verify they are    *
     *     independently addressable and removable.                       *
     * ------------------------------------------------------------------ */
    bu_log("[9] multi-object feature slots...\n");
    {
	const char *slots[] = {
	    "_tcl_data_arrows",    "_tcl_sdata_arrows",
	    "_tcl_data_lines",     "_tcl_sdata_lines",
	    "_tcl_data_labels",    "_tcl_sdata_labels",
	    "_tcl_data_axes",      "_tcl_sdata_axes",
	    "_tcl_data_polygons",  "_tcl_sdata_polygons",
	    NULL
	};
	/* Create all slots */
	for (int k = 0; slots[k]; k++) {
	    bsg_feature_ref ref = bsg_feature_create_lines(v, slots[k], 1);
	    ASSERT(!bsg_feature_ref_is_null(ref));
	}
	/* Verify all are findable */
	for (int k = 0; slots[k]; k++) {
	    ASSERT(!bsg_feature_ref_is_null(bsg_feature_find(v, slots[k])));
	}
	/* Remove all slots */
	for (int k = 0; slots[k]; k++) {
	    bsg_feature_remove(v, slots[k]);
	    ASSERT(bsg_feature_ref_is_null(bsg_feature_find(v, slots[k])));
	}
    }

done:
    ged_close(gedp);
    bu_file_delete(bu_vls_cstr(&fname));
    bu_vls_free(&fname);

    bu_log("Result: %d checks, %d failures\n", nchecks, nfails);
    return (nfails > 0) ? 1 : 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
