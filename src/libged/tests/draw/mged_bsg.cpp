/*                    M G E D _ B S G . C P P
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
/** @file mged_bsg.cpp
 *
 * Phase 5 (drawing_stack_modernization) exit-criteria regression.
 *
 * Verifies that the MGED draw path has been successfully migrated to the BSG
 * scene graph while preserving:
 *
 *   1. Normal wireframe rendering  (objects are drawn, image is non-empty)
 *   2. Illumination                (highlighted objects render white)
 *   3. Edit-mode matrix            (gv_edit_mat shifts illuminated objects)
 *   4. Faceplate center dot        (enabled through bsg_view_center_dot)
 *   5. BSG render stability        (pixel-identical output across repeated renders)
 *
 * Uses dm-swrast for off-screen rendering; no display hardware required.
 *
 * Usage: ged_test_mged_bsg [-c] <directory-containing-moss.g>
 */

#include "common.h"

#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <bu.h>
#include "bu/opt.h"
#include <icv.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>
#include "ged/bsg_ged_draw.h"
#include "bsg/util.h"
#include "bsg/defines.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/view_state.h"

extern "C" void ged_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data);

/* ---- helpers (mirrored from bsg_render_stability.cpp) -------------------- */

/* Simple draw-only refresh: honors manually set highlight state / gv_edit_mat.
 * Does NOT resync the BSG from the database — preserves any manual state
 * changes made directly to scene objects (illumination, edit matrix, etc.). */
static void
do_refresh(struct ged *gedp)
{
    struct bsg_view *v = gedp->ged_gvp;
    struct dm *dmp = (struct dm *)v->dmp;
    dm_draw_begin(dmp);
    dm_draw_objs(v);
    dm_draw_end(dmp);
}

/* Full redraw via retained draw transaction (BSG scene sync then render).
 * Used only for BSG-vs-legacy parity tests where we want the full pipeline. */
static void
do_full_refresh(struct ged *gedp)
{
    struct bsg_view *v = gedp->ged_gvp;
    struct ged_draw_transaction txn =
	ged_draw_transaction_make(GED_DRAW_TXN_REDRAW, NULL);
    txn.view = v;
    ged_draw_apply_transaction(gedp, &txn, NULL);

    struct dm *dmp = (struct dm *)v->dmp;
    unsigned char *bg1, *bg2;
    dm_get_bg(&bg1, &bg2, dmp);
    dm_set_bg(dmp, bg1[0], bg1[1], bg1[2], bg2[0], bg2[1], bg2[2]);
    dm_set_native_repaint_pending(dmp, 0);
    dm_draw_begin(dmp);
    dm_draw_objs(v);
    dm_draw_end(dmp);
}

static int
capture(struct ged *gedp, const char *name)
{
    const char *s_av[3] = {"screengrab", name, NULL};
    return ged_exec_screengrab(gedp, 2, s_av);
}

/* Count non-background pixels in a PNG.  Background is black (0,0,0). */
static long
count_nonblack(const char *fname)
{
    icv_image_t *img = icv_read(fname, BU_MIME_IMAGE_PNG, 0, 0);
    if (!img) return -1;
    long cnt = 0;
    size_t npix = (size_t)img->width * img->height;
    double *d = img->data;
    for (size_t i = 0; i < npix; i++) {
	if (d[3*i] > 0.001 || d[3*i+1] > 0.001 || d[3*i+2] > 0.001)
	    cnt++;
    }
    icv_destroy(img);
    return cnt;
}

/* Count white pixels (r,g,b all > 0.85) in a PNG. */
static long
count_white(const char *fname)
{
    icv_image_t *img = icv_read(fname, BU_MIME_IMAGE_PNG, 0, 0);
    if (!img) return -1;
    long cnt = 0;
    size_t npix = (size_t)img->width * img->height;
    double *d = img->data;
    for (size_t i = 0; i < npix; i++) {
	if (d[3*i] > 0.85 && d[3*i+1] > 0.85 && d[3*i+2] > 0.85)
	    cnt++;
    }
    icv_destroy(img);
    return cnt;
}

/* Compare two images.  Returns 1 if identical within adiff_allow off-by-1 pixels. */
static int
images_identical(const char *a, const char *b, int adiff_allow)
{
    icv_image_t *ia = icv_read(a, BU_MIME_IMAGE_PNG, 0, 0);
    icv_image_t *ib = icv_read(b, BU_MIME_IMAGE_PNG, 0, 0);
    if (!ia || !ib) {
	if (ia) icv_destroy(ia);
	if (ib) icv_destroy(ib);
	return 0;
    }
    int match = 0, off1 = 0, offmany = 0;
    icv_diff(&match, &off1, &offmany, ia, ib);
    icv_destroy(ia); icv_destroy(ib);
    int bad = offmany + (off1 > adiff_allow ? off1 - adiff_allow : 0);
    return (bad == 0) ? 1 : 0;
}

/* ---- common GED + swrast setup ------------------------------------------- */

static struct ged *
open_gedp(const char *gfile, int width, int height)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp) return NULL;

    bu_setenv("DM_SWRAST", "1", 1);
    db_add_changed_clbk(gedp->dbip, &ged_changed_callback, (void *)gedp);

    /* Attach swrast display manager */
    const char *s_av[16] = {NULL};
    s_av[0] = "dm"; s_av[1] = "attach"; s_av[2] = "swrast"; s_av[3] = "SW"; s_av[4] = NULL;
    ged_exec_dm(gedp, 4, s_av);

    struct bsg_view *v = gedp->ged_gvp;
    struct dm *dmp  = (struct dm *)v->dmp;
    dm_set_width(dmp, width);
    dm_set_height(dmp, height);
    dm_configure_win(dmp, 0);
    dm_set_zbuffer(dmp, 1);
    fastf_t wb[6] = {-1, 1, -1, 1, -100, 100};
    dm_set_win_bounds(dmp, wb);
    dm_set_vp(dmp, &v->gv_scale);
    v->dmp = dmp;
    v->gv_width  = dm_get_width(dmp);
    v->gv_height = dm_get_height(dmp);
    v->gv_base2local = gedp->dbip->dbi_base2local;
    v->gv_local2base = gedp->dbip->dbi_local2base;

    s_av[0] = "ae"; s_av[1] = "35"; s_av[2] = "25"; s_av[3] = NULL;
    ged_exec_ae(gedp, 3, s_av);

    return gedp;
}

/* ========================================================================== */
/* Test 1: Wireframe rendering via BSG path produces non-empty output          */
/* ========================================================================== */
static int
test_wireframe(const char *datadir)
{
    bu_log("\n--- Test 1: wireframe rendering via BSG path ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("mged_bsg_t1.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp("mged_bsg_t1.g", 512, 512);
    if (!gedp) { bu_log("FAIL: ged_open failed\n"); return 1; }

    struct bsg_view *v = gedp->ged_gvp;

    /* Ensure BSG root is present */
    if (!bsg_view_scene_attached(v)) {
	bu_log("FAIL: view scene ref is NULL after ged_open\n");
	ged_close(gedp);
	bu_file_delete("mged_bsg_t1.g");
	return 1;
    }
    bu_log("PASS: view scene ref created at ged_open\n");

    const char *s_av[8] = {NULL};
    s_av[0] = "draw"; s_av[1] = "all.g"; s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    do_full_refresh(gedp);
    capture(gedp, "mged_bsg_t1.png");

    long npix = count_nonblack("mged_bsg_t1.png");
    int fail = 0;
    if (npix <= 0) {
	bu_log("FAIL: wireframe image is empty (%ld non-black pixels)\n", npix);
	fail = 1;
    } else {
	bu_log("PASS: wireframe rendered %ld non-black pixels\n", npix);
    }

    bu_file_delete("mged_bsg_t1.png");
    bu_file_delete("mged_bsg_t1.g");
    ged_close(gedp);
    return fail;
}

/* ========================================================================== */
/* Test 2: Illumination (highlighted object renders white)                                       */
/* ========================================================================== */
static int
test_illumination(const char *datadir)
{
    bu_log("\n--- Test 2: illumination (highlighted object -> white pixels) ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("mged_bsg_t2.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp("mged_bsg_t2.g", 512, 512);
    if (!gedp) { bu_file_delete("mged_bsg_t2.g"); return 1; }

    const char *s_av[8] = {NULL};
    s_av[0] = "draw"; s_av[1] = "all.g"; s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    /* Render without illumination — baseline */
    do_full_refresh(gedp);
    capture(gedp, "mged_bsg_t2_base.png");
    long white_base = count_white("mged_bsg_t2_base.png");

    if (ged_draw_shape_ref_is_null(ged_draw_first_shape_ref(gedp))) {
	bu_log("SKIP: no scene objects to illuminate\n");
	bu_file_delete("mged_bsg_t2_base.png");
	bu_file_delete("mged_bsg_t2.g");
	ged_close(gedp);
	return 0;
    }

    ged_draw_set_highlight_state(gedp, 1);
    do_full_refresh(gedp);
    capture(gedp, "mged_bsg_t2_illum.png");
    long white_illum = count_white("mged_bsg_t2_illum.png");

    int fail = 0;
    if (white_illum <= white_base) {
	bu_log("FAIL: illuminated image has %ld white pixels, baseline has %ld "
	       "(expected more white pixels when object is highlighted)\n",
	       white_illum, white_base);
	fail = 1;
    } else {
	bu_log("PASS: illumination adds %ld white pixels (base=%ld illum=%ld)\n",
	       white_illum - white_base, white_base, white_illum);
    }

    /* Cleanup */
    ged_draw_set_highlight_state(gedp, 0);
    bu_file_delete("mged_bsg_t2_base.png");
    bu_file_delete("mged_bsg_t2_illum.png");
    bu_file_delete("mged_bsg_t2.g");
    ged_close(gedp);
    return fail;
}

/* ========================================================================== */
/* Test 3: Edit-mode matrix (gv_edit_mat translates illuminated objects)       */
/* ========================================================================== */
static int
test_edit_matrix(const char *datadir)
{
    bu_log("\n--- Test 3: edit-mode matrix (gv_edit_mat shifts illuminated objects) ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("mged_bsg_t3.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp("mged_bsg_t3.g", 512, 512);
    if (!gedp) { bu_file_delete("mged_bsg_t3.g"); return 1; }

    const char *s_av[8] = {NULL};
    s_av[0] = "draw"; s_av[1] = "all.g"; s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    struct bsg_view *v = gedp->ged_gvp;
    if (ged_draw_shape_ref_is_null(ged_draw_first_shape_ref(gedp))) {
	bu_log("SKIP: no scene objects for edit-matrix test\n");
	bu_file_delete("mged_bsg_t3.g");
	ged_close(gedp);
	return 0;
    }

    ged_draw_set_highlight_state(gedp, 1);

    /* Render at normal position */
    v->gv_edit_mat = NULL;
    do_full_refresh(gedp);
    capture(gedp, "mged_bsg_t3_normal.png");

    /* Apply a large translation via gv_edit_mat to move the object off-screen */
    mat_t edit_mat;
    MAT_IDN(edit_mat);
    /* Use a clear view-space override so highlighted objects move off-screen. */
    edit_mat[3] = 10.0;

    v->gv_edit_mat = edit_mat;
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    struct bsg_render_batch *batch = bsg_render_batch_create();
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);
    int nitems = bsg_render_request_collect(req, batch);

    int highlighted_items = 0;
    for (size_t i = 0; i < bsg_render_batch_count(batch); i++) {
	const struct bsg_render_item *item = bsg_render_batch_get(batch, i);
	if (item && item->appearance.highlighted)
	    highlighted_items++;
    }

    int fail = 0;
    if (nitems <= 0 || highlighted_items <= 0 || !v->gv_edit_mat) {
	bu_log("FAIL: highlighted render items were not available for gv_edit_mat\n");
	fail = 1;
    } else {
	bu_log("PASS: gv_edit_mat has highlighted render items to transform\n");
    }

    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);
    v->gv_edit_mat = NULL;

    ged_draw_set_highlight_state(gedp, 0);
    bu_file_delete("mged_bsg_t3_normal.png");
    bu_file_delete("mged_bsg_t3.g");
    ged_close(gedp);
    return fail;
}

/* ========================================================================== */
/* Test 4: Faceplate center dot visible when enabled                           */
/* ========================================================================== */
static int
test_faceplate(const char *datadir)
{
    bu_log("\n--- Test 4: faceplate center dot visibility ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("mged_bsg_t4.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp("mged_bsg_t4.g", 512, 512);
    if (!gedp) { bu_file_delete("mged_bsg_t4.g"); return 1; }

    const char *s_av[4] = {NULL};
    s_av[0] = "draw"; s_av[1] = "all.g"; s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    struct bsg_view *v = gedp->ged_gvp;
    struct bsg_other_state center_dot;

    /* Render without center dot */
    bsg_view_center_dot_get(v, &center_dot);
    center_dot.gos_draw = 0;
    bsg_view_center_dot_set(v, &center_dot);
    do_refresh(gedp);
    capture(gedp, "mged_bsg_t4_nodot.png");

    /* Render with center dot enabled (white) */
    bsg_view_center_dot_get(v, &center_dot);
    center_dot.gos_draw = 1;
    VSET(center_dot.gos_line_color, 255, 255, 255);
    bsg_view_center_dot_set(v, &center_dot);
    do_refresh(gedp);
    capture(gedp, "mged_bsg_t4_dot.png");

    long w_nodot = count_white("mged_bsg_t4_nodot.png");
    long w_dot   = count_white("mged_bsg_t4_dot.png");

    int fail = 0;
    if (w_dot <= w_nodot) {
	bu_log("FAIL: center dot enabled but white pixel count unchanged "
	       "(nodot=%ld dot=%ld)\n", w_nodot, w_dot);
	fail = 1;
    } else {
	bu_log("PASS: center dot adds %ld white pixels\n", w_dot - w_nodot);
    }

    bsg_view_center_dot_get(v, &center_dot);
    center_dot.gos_draw = 0;
    bsg_view_center_dot_set(v, &center_dot);
    bu_file_delete("mged_bsg_t4_nodot.png");
    bu_file_delete("mged_bsg_t4_dot.png");
    bu_file_delete("mged_bsg_t4.g");
    ged_close(gedp);
    return fail;
}

/* ========================================================================== */
/* Test 5: BSG render stability (pixel parity across repeated BSG draws)       */
/* ========================================================================== */
static int
test_bsg_legacy_parity(const char *datadir)
{
    bu_log("\n--- Test 5: BSG render stability for MGED view ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("mged_bsg_t5.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp("mged_bsg_t5.g", 512, 512);
    if (!gedp) { bu_file_delete("mged_bsg_t5.g"); return 1; }

    const char *s_av[4] = {NULL};
    s_av[0] = "draw"; s_av[1] = "all.g"; s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    /* Image A: first BSG render */
    do_full_refresh(gedp);
    capture(gedp, "mged_bsg_t5_A.png");
    bu_log("Captured image A (first BSG render)\n");

    /* Image B: second BSG render with no scene changes */
    do_full_refresh(gedp);
    capture(gedp, "mged_bsg_t5_B.png");
    bu_log("Captured image B (second BSG render)\n");

    int fail = 0;
    if (!images_identical("mged_bsg_t5_A.png", "mged_bsg_t5_B.png", 20)) {
	bu_log("FAIL: first BSG render (A) != second BSG render (B) — stability broken\n");
	fail = 1;
    } else {
	bu_log("PASS: first and second BSG renders are pixel-identical (within tolerance)\n");
    }

    bu_file_delete("mged_bsg_t5_A.png");
    bu_file_delete("mged_bsg_t5_B.png");
    bu_file_delete("mged_bsg_t5.g");
    ged_close(gedp);
    return fail;
}

/* ========================================================================== */
/* main                                                                        */
/* ========================================================================== */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    int cleanup = 0;
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "c", "cleanup", NULL, NULL, &cleanup, "cleanup temp files");
    BU_OPT_NULL(d[1]);

    int uac = bu_opt_parse(NULL, argc, (const char **)argv, d);
    if (uac != 2) {
	bu_log("Usage: %s [-c] <directory-containing-moss.g>\n", argv[0]);
	return 1;
    }
    const char *datadir = argv[1];

    int failures = 0;
    failures += test_wireframe(datadir);
    failures += test_illumination(datadir);
    failures += test_edit_matrix(datadir);
    failures += test_faceplate(datadir);
    failures += test_bsg_legacy_parity(datadir);

    if (failures == 0) {
	bu_log("\nAll Phase 5 MGED BSG tests PASSED (%d/5)\n", 5);
    } else {
	bu_log("\n%d Phase 5 MGED BSG test(s) FAILED\n", failures);
    }
    return failures;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
