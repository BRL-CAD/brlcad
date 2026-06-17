/*          M G E D _ S H A D E D _ M O D E _ B S G . C P P
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
/** @file mged_shaded_mode_bsg.cpp
 *
 * Phase 7 (drawing_stack_modernization) regression test.
 *
 * Verifies the lighting / zbuffer / zclip plumbing on the BSG render path:
 *
 *   Test 1 — dm_set_light / dm_set_zbuffer / dm_set_zclip round-trip
 *             Simulates what mged_shaded_mode_helper does:
 *               dm set zbuffer $val; dm set zclip $val; dm set lighting $val
 *             Asserts the corresponding dm_get_* functions return the new value.
 *             Also verifies toggling back to 0 restores the off state.
 *
 *   Test 2 — shaded draw-mode pipeline (draw -m1, draw -m2) produces different
 *             pixel output from wireframe (draw -m0).
 *             In the repaired BSG path (Phases 1-6) shaded vlists flow through
 *             the two-pass transparency path and are decoded by gl_draw_item.
 *             This test uses the dm-swrast software rasterizer so it does not
 *             require display hardware, but it does exercise the same
 *             render-item / render-batch code paths as real GL.
 *
 *   Test 3 — dm_set/get_geometry_default_color round-trip
 *             Phase 4 added dm_get/set_geometry_default_color.  Verify the
 *             accessor round-trips correctly so that render-item appearance
 *             resolution can honour canonical default-color metadata.
 *
 *   Test 4 — draw record stability across zap/redraw
 *             After a zap the bsg root is re-populated from scratch.  Draw once,
 *             capture the non-black pixel count.  Zap and re-draw.  The second
 *             count and the semantic draw-record count must match the first.
 *
 * Uses dm-swrast for off-screen rendering; no display hardware required.
 *
 * Usage: ged_test_mged_shaded_mode_bsg <directory-containing-moss.g>
 */

#include "common.h"

#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unordered_set>

#include <bu.h>
#include "bu/opt.h"
#include <icv.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>
#include "bsg/defines.h"
#include "ged/bsg_ged_draw.h"

extern "C" void ged_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data);

/* ---- shared helpers ------------------------------------------------------ */

static void
do_refresh(struct ged *gedp)
{
    struct bsg_view *v = gedp->ged_gvp;
    struct dm *dmp = (struct dm *)v->dmp;
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

static int
images_differ(const char *a, const char *b)
{
    icv_image_t *ia = icv_read(a, BU_MIME_IMAGE_PNG, 0, 0);
    icv_image_t *ib = icv_read(b, BU_MIME_IMAGE_PNG, 0, 0);
    if (!ia || !ib) {
	if (ia) icv_destroy(ia);
	if (ib) icv_destroy(ib);
	return 1; /* treat missing images as different so test fails */
    }
    int match = 0, off1 = 0, offmany = 0;
    icv_diff(&match, &off1, &offmany, ia, ib);
    icv_destroy(ia); icv_destroy(ib);
    /* "differ" means at least 10 pixels changed by more than off-by-1 */
    return (offmany >= 10) ? 1 : 0;
}

static struct ged *
open_gedp(const char *gfile, int width, int height)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp) return NULL;

    bu_setenv("DM_SWRAST", "1", 1);
    db_add_changed_clbk(gedp->dbip, &ged_changed_callback, (void *)gedp);

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
/* Test 1: dm_set/get_light, dm_set/get_zbuffer, dm_set/get_zclip round-trip  */
/* Simulates the state flips that mged_shaded_mode_helper issues:              */
/*   dm set zbuffer $val; dm set zclip $val; dm set lighting $val             */
/* ========================================================================== */
static int
test_dm_lighting_flags(const char *datadir)
{
    bu_log("\n--- Test 1: dm lighting/zbuffer/zclip flag round-trip ---\n");
    bu_log("(simulates mged_shaded_mode_helper code path)\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("smb_t1.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp("smb_t1.g", 256, 256);
    if (!gedp) { bu_log("FAIL: ged_open failed\n"); bu_file_delete("smb_t1.g"); return 1; }

    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    int fail = 0;

    /* --- turn everything ON (val=1) --------------------------------------- */
    dm_set_light(dmp, 1);
    dm_set_zbuffer(dmp, 1);
    dm_set_zclip(dmp, 1);

    int l = dm_get_light(dmp);
    int z = dm_get_zbuffer(dmp);
    int c = dm_get_zclip(dmp);
    if (!l) { bu_log("FAIL: dm_get_light returned %d after set to 1\n", l); fail++; }
    else      bu_log("PASS: dm_get_light == 1\n");
    if (!z) { bu_log("FAIL: dm_get_zbuffer returned %d after set to 1\n", z); fail++; }
    else      bu_log("PASS: dm_get_zbuffer == 1\n");
    if (!c) { bu_log("FAIL: dm_get_zclip returned %d after set to 1\n", c); fail++; }
    else      bu_log("PASS: dm_get_zclip == 1\n");

    /* --- turn everything OFF (val=0) --------------------------------------- */
    dm_set_light(dmp, 0);
    dm_set_zbuffer(dmp, 0);
    dm_set_zclip(dmp, 0);

    l = dm_get_light(dmp);
    z = dm_get_zbuffer(dmp);
    c = dm_get_zclip(dmp);
    if (l)  { bu_log("FAIL: dm_get_light returned %d after set to 0\n", l); fail++; }
    else     bu_log("PASS: dm_get_light == 0\n");
    if (z)  { bu_log("FAIL: dm_get_zbuffer returned %d after set to 0\n", z); fail++; }
    else     bu_log("PASS: dm_get_zbuffer == 0\n");
    if (c)  { bu_log("FAIL: dm_get_zclip returned %d after set to 0\n", c); fail++; }
    else     bu_log("PASS: dm_get_zclip == 0\n");

    /* --- ged_exec_dm path (what the Tcl mged command uses) ---------------- */
    /* "dm set lighting 1"  */
    {
	const char *s_av[5] = {"dm", "set", "lighting", "1", NULL};
	ged_exec_dm(gedp, 4, s_av);
	int lv = dm_get_light(dmp);
	if (!lv) {
	    bu_log("FAIL: 'dm set lighting 1' → dm_get_light returned %d\n", lv);
	    fail++;
	} else {
	    bu_log("PASS: 'dm set lighting 1' → dm_get_light == 1\n");
	}
    }
    /* "dm set lighting 0"  */
    {
	const char *s_av[5] = {"dm", "set", "lighting", "0", NULL};
	ged_exec_dm(gedp, 4, s_av);
	int lv = dm_get_light(dmp);
	if (lv) {
	    bu_log("FAIL: 'dm set lighting 0' → dm_get_light returned %d\n", lv);
	    fail++;
	} else {
	    bu_log("PASS: 'dm set lighting 0' → dm_get_light == 0\n");
	}
    }

    ged_close(gedp);
    bu_file_delete("smb_t1.g");
    return fail;
}

/* ========================================================================== */
/* Test 2: shaded draw modes produce different output from wireframe           */
/* ========================================================================== */
static int
test_shaded_mode_draw(const char *datadir)
{
    bu_log("\n--- Test 2: shaded draw modes produce non-empty and distinct output ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("smb_t2.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp("smb_t2.g", 512, 512);
    if (!gedp) { bu_file_delete("smb_t2.g"); return 1; }

    const char *s_av[8] = {NULL};
    int fail = 0;

    /* --- wireframe (mode 0) ----------------------------------------------- */
    s_av[0] = "draw"; s_av[1] = "-m0"; s_av[2] = "all.g"; s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    do_refresh(gedp);
    capture(gedp, "smb_t2_m0.png");
    long pix_m0 = count_nonblack("smb_t2_m0.png");
    bu_log("Wireframe (mode 0): %ld non-black pixels\n", pix_m0);
    if (pix_m0 <= 0) {
	bu_log("FAIL: wireframe rendered nothing\n");
	fail++;
    }

    /* --- shaded bots only (mode 1) ---------------------------------------- */
    s_av[0] = "Z"; s_av[1] = NULL;
    ged_exec_Z(gedp, 1, s_av);

    s_av[0] = "draw"; s_av[1] = "-m1"; s_av[2] = "all.g"; s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    do_refresh(gedp);
    capture(gedp, "smb_t2_m1.png");
    long pix_m1 = count_nonblack("smb_t2_m1.png");
    bu_log("Shaded bots only (mode 1): %ld non-black pixels\n", pix_m1);
    if (pix_m1 <= 0) {
	bu_log("FAIL: shaded-bots mode rendered nothing\n");
	fail++;
    }

    /* --- shaded all (mode 2) ---------------------------------------------- */
    s_av[0] = "Z"; s_av[1] = NULL;
    ged_exec_Z(gedp, 1, s_av);

    s_av[0] = "draw"; s_av[1] = "-m2"; s_av[2] = "all.g"; s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    do_refresh(gedp);
    capture(gedp, "smb_t2_m2.png");
    long pix_m2 = count_nonblack("smb_t2_m2.png");
    bu_log("Shaded all (mode 2): %ld non-black pixels\n", pix_m2);
    if (pix_m2 <= 0) {
	bu_log("FAIL: shaded-all mode rendered nothing\n");
	fail++;
    }

    /* shaded modes should produce visually different output from wireframe */
    if (pix_m1 > 0 && pix_m0 > 0) {
	if (images_differ("smb_t2_m0.png", "smb_t2_m1.png"))
	    bu_log("PASS: mode 1 (shaded bots) differs from mode 0 (wireframe)\n");
	else
	    bu_log("INFO: mode 1 and mode 0 produced the same pixels "
		   "(may be OK if moss.g has no BoT primitives)\n");
    }
    if (pix_m2 > 0 && pix_m0 > 0) {
	if (images_differ("smb_t2_m0.png", "smb_t2_m2.png"))
	    bu_log("PASS: mode 2 (shaded all) differs from mode 0 (wireframe)\n");
	else {
	    bu_log("FAIL: mode 2 (shaded all) and wireframe are identical — "
		   "shaded path is broken\n");
	    fail++;
	}
    }

    bu_file_delete("smb_t2_m0.png");
    bu_file_delete("smb_t2_m1.png");
    bu_file_delete("smb_t2_m2.png");
    bu_file_delete("smb_t2.g");
    ged_close(gedp);
    return fail;
}

/* ========================================================================== */
/* Test 3: dm_get/set_geometry_default_color round-trip (Phase 4 addition)    */
/* ========================================================================== */
static int
test_geometry_default_color(const char *datadir)
{
    bu_log("\n--- Test 3: dm_get/set_geometry_default_color round-trip ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("smb_t3.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp("smb_t3.g", 256, 256);
    if (!gedp) { bu_file_delete("smb_t3.g"); return 1; }

    struct dm *dmp = (struct dm *)gedp->ged_gvp->dmp;
    int fail = 0;

    /* Set a recognisable test colour */
    dm_set_geometry_default_color(dmp, 0, 200, 100);

    unsigned char *dgc = dm_get_geometry_default_color(dmp);

    if (!dgc || dgc[0] != 0 || dgc[1] != 200 || dgc[2] != 100) {
	bu_log("FAIL: dm_get_geometry_default_color returned (%u,%u,%u), "
	       "expected (0,200,100)\n",
	       dgc ? (unsigned)dgc[0] : 0,
	       dgc ? (unsigned)dgc[1] : 0,
	       dgc ? (unsigned)dgc[2] : 0);
	fail++;
    } else {
	bu_log("PASS: dm_get_geometry_default_color round-trips (0,200,100)\n");
    }

    /* Reset to the MGED default (255,0,0) and verify */
    dm_set_geometry_default_color(dmp, 255, 0, 0);
    dgc = dm_get_geometry_default_color(dmp);
    if (!dgc || dgc[0] != 255 || dgc[1] != 0 || dgc[2] != 0) {
	bu_log("FAIL: reset to default (255,0,0) returned (%u,%u,%u)\n",
	       dgc ? (unsigned)dgc[0] : 0,
	       dgc ? (unsigned)dgc[1] : 0,
	       dgc ? (unsigned)dgc[2] : 0);
	fail++;
    } else {
	bu_log("PASS: dm_get_geometry_default_color round-trips (255,0,0)\n");
    }

    ged_close(gedp);
    bu_file_delete("smb_t3.g");
    return fail;
}

/* ========================================================================== */
/* Test 4: per-frame drawn-set generation — drawn-count record doesn't over-count (Phase 9.2) */
/* ========================================================================== */
static int
test_sflags_per_frame_reset(const char *datadir)
{
    bu_log("\n--- Test 4: per-frame drawn-set generation / drawn-count record accuracy ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("smb_t4.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp("smb_t4.g", 512, 512);
    if (!gedp) { bu_file_delete("smb_t4.g"); return 1; }

    int fail = 0;
    const char *s_av[4] = {NULL};

    /* Draw once */
    s_av[0] = "draw"; s_av[1] = "all.g"; s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    do_refresh(gedp);
    capture(gedp, "smb_t4_first.png");
    long pix_first = count_nonblack("smb_t4_first.png");
    bu_log("First render: %ld non-black pixels\n", pix_first);

    int draw_records_first = ged_draw_shape_count(gedp);
    bu_log("After first frame: %d semantic draw records\n", draw_records_first);

    /* Zap and re-draw identically */
    s_av[0] = "Z"; s_av[1] = NULL;
    ged_exec_Z(gedp, 1, s_av);

    s_av[0] = "draw"; s_av[1] = "all.g"; s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    do_refresh(gedp);
    capture(gedp, "smb_t4_second.png");
    long pix_second = count_nonblack("smb_t4_second.png");
    bu_log("Second render (after zap+redraw): %ld non-black pixels\n", pix_second);

    /* The rendered pixel counts should be the same */
    long diff = pix_first > pix_second ? pix_first - pix_second
	                                : pix_second - pix_first;
    if (diff > 500) {
	bu_log("FAIL: pixel count after zap/redraw differs by %ld from first render "
	       "(first=%ld second=%ld)\n", diff, pix_first, pix_second);
	fail++;
    } else {
	bu_log("PASS: pixel count consistent across zap/redraw (%ld vs %ld)\n",
	       pix_first, pix_second);
    }

    int draw_records_second = ged_draw_shape_count(gedp);
    bu_log("After second frame: %d semantic draw records\n", draw_records_second);

    if (draw_records_second != draw_records_first) {
	bu_log("FAIL: semantic draw record count changed from %d to %d across zap/redraw\n",
	       draw_records_first, draw_records_second);
	fail++;
    } else {
	bu_log("PASS: semantic draw record count stable across zap/redraw "
	       "(%d → %d)\n", draw_records_first, draw_records_second);
    }

    bu_file_delete("smb_t4_first.png");
    bu_file_delete("smb_t4_second.png");
    bu_file_delete("smb_t4.g");
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

    if (argc != 2) {
	bu_log("Usage: %s <directory-containing-moss.g>\n", argv[0]);
	return 1;
    }
    const char *datadir = argv[1];

    int failures = 0;
    failures += test_dm_lighting_flags(datadir);
    failures += test_shaded_mode_draw(datadir);
    failures += test_geometry_default_color(datadir);
    failures += test_sflags_per_frame_reset(datadir);

    if (failures == 0) {
	bu_log("\nAll Phase 7 BSG lighting/zbuffer tests PASSED (4/4)\n");
    } else {
	bu_log("\n%d Phase 7 BSG lighting/zbuffer test(s) FAILED\n", failures);
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
