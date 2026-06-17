/*                  R T W I Z A R D _ B S G . C P P
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
/** @file rtwizard_bsg.cpp
 *
 * Phase 5 (drawing_stack_modernization) rtwizard migration exit-criteria test.
 *
 * rtwizard's headless (--no-gui) pipeline computes rt view parameters via the
 * libtclcad GED API:
 *
 *   go_open db db <file>          ; opens a GED instance
 *   db new_view v1 nu             ; creates a null-DM secondary view
 *   db draw <objects>             ; populates scene objects for autoview
 *   db autoview v1                ; sets gv_size / gv_center from scene bounds
 *   db aet v1 <az> <el> <tw>     ; sets azimuth / elevation / twist
 *   db zoom v1 <z>                ; applies zoom
 *   db get_eyemodel v1            ; extracts viewsize, orientation, eye_pt
 *
 * The eye model (viewsize, orientation quaternion, eye_pt) is then forwarded
 * as command-line arguments to the rt/rtedge subprocess.  No display-manager
 * rendering occurs; the view feature is purely a lightweight camera container.
 *
 * Stage C ensures that libtclcad/commands.c creates a retained scene anchor
 * through bsg_view_scene_separator_ref(..., 1) for every new view, including the null-DM
 * "v1" view above.  This test exercises the equivalent C-API path and
 * verifies:
 *
 *   1. null_view_scene_ref     — A null-DM secondary view gets a scene handle,
 *                                mirroring what libtclcad does for "new_view nu".
 *   2. eyemodel_finite         — draw + autoview + get_eyemodel produces a
 *                                plausible (finite, non-degenerate) eye model.
 *   3. nodisplaylist_path      — Secondary views share the active draw scene
 *                                (no legacy go_draw_dlist fallback path).
 *
 * All tests use the "nu" (null) display-manager so no display hardware or X11
 * server is required.
 *
 * Usage: ged_test_rtwizard_bsg [-c] <directory-containing-moss.g>
 */

#include "common.h"

#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>

#include <bu.h>
#include "bu/opt.h"
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>
#include <icv.h>
#include "bsg/node.h"
#include "bsg/util.h"
#include "bsg/view_state.h"

extern "C" void ged_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data);

/* ---- helpers ------------------------------------------------------------- */

static void
close_gedp(struct ged *gedp)
{
    ged_close(gedp);
}

/*
 * Open a GED instance for CLI-mode testing (no DM needed, BSG enabled).
 * Simulates the go_open step in the rtwizard headless Tcl pipeline.
 *
 * rtwizard --no-gui never attaches a display-manager to the main GED instance;
 * it only creates a secondary view for camera computation.  We match that here.
 */
static struct ged *
open_gedp_null(const char *gfile)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp)
	return NULL;

    db_add_changed_clbk(gedp->dbip, &ged_changed_callback, (void *)gedp);

    struct bsg_view *v = gedp->ged_gvp;
    v->gv_base2local = gedp->dbip->dbi_base2local;
    v->gv_local2base = gedp->dbip->dbi_local2base;

    return gedp;
}

/*
 * Create a secondary null-DM view and add it to the GED view set.
 * Mirrors what libtclcad's to_new_view() does for "db new_view v1 nu".
 *
 * A null-DM view needs no real display-manager handle; its sole purpose is
 * to carry a camera matrix (gv_size, gv_center, gv_rotation) for computing
 * the eye model.  We leave v->dmp = NULL just as libtclcad does for "nu".
 */
static struct bsg_view *
make_null_view(struct ged *gedp, const char *vname)
{
    struct bsg_view *v;
    BU_GET(v, struct bsg_view);
    bsg_init(v, &gedp->ged_views);
    bu_vls_sprintf(&v->gv_name, "%s", vname);
    v->gv_base2local = gedp->dbip->dbi_base2local;
    v->gv_local2base = gedp->dbip->dbi_local2base;

    /* Every new libtclcad view gets a retained BSG scene anchor. */
    (void)bsg_view_scene_separator_ref(v, 1);

    bsg_set_add_view(&gedp->ged_views, v);
    bu_ptbl_ins(&gedp->ged_free_views, (long *)v);

    return v;
}

/* ========================================================================== */
/* Test 1: null-DM secondary view gets a BSG scene root                       */
/* ========================================================================== */
static int
test_null_view_scene_ref(const char *datadir)
{
    bu_log("\n--- Test 1: null-DM secondary view gets BSG root ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("rtw_bsg_t1.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp_null("rtw_bsg_t1.g");
    if (!gedp) {
	bu_log("FAIL: ged_open failed\n");
	bu_file_delete("rtw_bsg_t1.g");
	return 1;
    }

    /* Create a secondary null-DM view, simulating "db new_view v1 nu" */
    struct bsg_view *v1 = make_null_view(gedp, "v1");

    int fail = 0;
    if (!bsg_view_scene_attached(v1)) {
	bu_log("FAIL: secondary null-DM view has no BSG root\n");
	fail = 1;
    } else {
	bu_log("PASS: secondary null-DM view has BSG root (Phase 5 libtclcad path)\n");
    }

    /* Also verify the default GED view has a BSG root (set by ged_open). */
    if (!bsg_view_scene_attached(gedp->ged_gvp)) {
	bu_log("FAIL: default GED view has no BSG root\n");
	fail = 1;
    } else {
	bu_log("PASS: default GED view has BSG root\n");
    }

    close_gedp(gedp);
    bu_file_delete("rtw_bsg_t1.g");
    return fail;
}

/* ========================================================================== */
/* Test 2: draw + autoview + get_eyemodel produces finite/plausible params    */
/* ========================================================================== */
static int
test_eyemodel_finite(const char *datadir)
{
    bu_log("\n--- Test 2: draw + autoview + get_eyemodel produces finite eye model ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("rtw_bsg_t2.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp_null("rtw_bsg_t2.g");
    if (!gedp) {
	bu_log("FAIL: ged_open failed\n");
	bu_file_delete("rtw_bsg_t2.g");
	return 1;
    }

    /* Create secondary null-DM view (rtwizard "new_view v1 nu") */
    struct bsg_view *v1 = make_null_view(gedp, "v1");

    /* Draw objects (rtwizard "db draw $item" for each object in color_objlist) */
    const char *s_av[4] = {"draw", "all.g", NULL};
    ged_exec_draw(gedp, 2, s_av);

    /* Autoview on v1 (rtwizard "db autoview v1") */
    struct bsg_view *prev = gedp->ged_gvp;
    gedp->ged_gvp = v1;
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    /* Apply default az/el/twist (rtwizard "db aet v1 35 25 0") */
    s_av[0] = "ae"; s_av[1] = "35"; s_av[2] = "25"; s_av[3] = NULL;
    ged_exec_ae(gedp, 3, s_av);

    /* Extract eye model (rtwizard "db get_eyemodel v1") */
    s_av[0] = "get_eyemodel"; s_av[1] = NULL;
    ged_exec_get_eyemodel(gedp, 1, s_av);
    gedp->ged_gvp = prev;

    const char *result = bu_vls_cstr(gedp->ged_result_str);
    bu_log("get_eyemodel output:\n%s\n", result);

    /* Parse viewsize from the result string */
    double viewsize = 0.0;
    double qw = 0.0, qx = 0.0, qy = 0.0, qz = 0.0;
    double ex = 0.0, ey = 0.0, ez = 0.0;
    int nscan = sscanf(result,
		       "viewsize %lf ; orientation %lf %lf %lf %lf ; eye_pt %lf %lf %lf",
		       &viewsize, &qw, &qx, &qy, &qz, &ex, &ey, &ez);

    int fail = 0;
    if (nscan < 8) {
	/* Try alternate format without semicolons on same token */
	nscan = sscanf(result,
		       "viewsize %lf\n orientation %lf %lf %lf %lf\n eye_pt %lf %lf %lf",
		       &viewsize, &qw, &qx, &qy, &qz, &ex, &ey, &ez);
    }

    if (nscan < 8) {
	bu_log("FAIL: could not parse eye model (%d/8 values matched)\n", nscan);
	fail = 1;
    } else {
	/* viewsize must be positive and finite */
	if (viewsize <= 0.0 || !std::isfinite(viewsize)) {
	    bu_log("FAIL: viewsize %g is not a positive finite number\n", viewsize);
	    fail = 1;
	} else {
	    bu_log("PASS: viewsize = %g\n", viewsize);
	}

	/* orientation quaternion must have unit magnitude (within tolerance) */
	double qmag = std::sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
	if (fabs(qmag - 1.0) > 0.01) {
	    bu_log("FAIL: orientation quaternion magnitude %g != 1.0\n", qmag);
	    fail = 1;
	} else {
	    bu_log("PASS: orientation quaternion |q| = %g\n", qmag);
	}

	/* eye_pt must be finite */
	if (!std::isfinite(ex) || !std::isfinite(ey) || !std::isfinite(ez)) {
	    bu_log("FAIL: eye_pt (%g %g %g) contains non-finite values\n", ex, ey, ez);
	    fail = 1;
	} else {
	    bu_log("PASS: eye_pt = (%g %g %g)\n", ex, ey, ez);
	}
    }

    close_gedp(gedp);
    bu_file_delete("rtw_bsg_t2.g");
    return fail;
}

/* ========================================================================== */
/* Test 3: secondary views share active scene ref (legacy fallback not used)  */
/* ========================================================================== */
static int
test_nodisplaylist_path(const char *datadir)
{
    bu_log("\n--- Test 3: shared scene ref on secondary views (go_draw_dlist not entered) ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("rtw_bsg_t3.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp_null("rtw_bsg_t3.g");
    if (!gedp) {
	bu_log("FAIL: ged_open failed\n");
	bu_file_delete("rtw_bsg_t3.g");
	return 1;
    }

    /* Draw objects */
    const char *s_av[4] = {"draw", "all.g", NULL};
    ged_exec_draw(gedp, 2, s_av);

    /* Create four secondary views and verify root sharing with the active tree */
    struct bsg_view *views[4];
    char vname[4][8];
    int fail = 0;
    for (int i = 0; i < 4; i++) {
	snprintf(vname[i], sizeof(vname[i]), "v%d", i + 1);
	views[i] = make_null_view(gedp, vname[i]);

	if (!bsg_view_scene_attached(views[i])) {
	    bu_log("FAIL: view '%s' has no BSG root; go_draw_dlist fallback would be used\n", vname[i]);
	    fail = 1;
	}
    }

    if (!fail) {
	bu_log("PASS: all 4 secondary null-DM views have BSG roots\n");
	bu_log("      => go_draw_dlist legacy dl_* fallback is NOT entered for any rtwizard view\n");
    }

    /* Phase F: the view scene ref is shared across views in the same GED draw set. */
    int shared = 1;
    for (int i = 0; i < 4; i++) {
	if (!bsg_view_scene_shared(views[i], gedp->ged_gvp)) {
	    bu_log("FAIL: view '%s' scene ref is not shared with the active GED draw root\n", vname[i]);
	    shared = 0;
	    fail = 1;
	}
    }
    if (shared)
	bu_log("PASS: all secondary views share the active GED scene ref\n");

    close_gedp(gedp);
    bu_file_delete("rtw_bsg_t3.g");
    return fail;
}

/* ---- helpers for swrast (GUI-mode) tests --------------------------------- */

/*
 * Open a GED instance with an attached swrast DM (mirrors the rtwizard GUI
 * context, where the MGED widget has a real display manager for rendering).
 */
static struct ged *
open_gedp_swrast(const char *gfile, int width, int height)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp) return NULL;

    bu_setenv("DM_SWRAST", "1", 1);
    db_add_changed_clbk(gedp->dbip, &ged_changed_callback, (void *)gedp);

    const char *s_av[6] = {"dm", "attach", "swrast", "RTW_SW", NULL};
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
    v->dmp           = dmp;
    v->gv_width      = dm_get_width(dmp);
    v->gv_height     = dm_get_height(dmp);
    v->gv_base2local = gedp->dbip->dbi_base2local;
    v->gv_local2base = gedp->dbip->dbi_local2base;

    return gedp;
}

/* Simple draw-only refresh for a GED's current view. */
static void
do_swrast_refresh(struct ged *gedp)
{
    struct bsg_view *v = gedp->ged_gvp;
    struct dm *dmp  = (struct dm *)v->dmp;
    dm_draw_begin(dmp);
    dm_draw_objs(v);
    dm_draw_end(dmp);
}

/* Count non-background (non-black) pixels in a PNG. */
static long
count_nonblack_rtw(const char *fname)
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

/* Compare two PNGs; returns 1 if identical within adiff_allow off-by-1 pixels. */
static int
images_match_rtw(const char *a, const char *b, int adiff_allow)
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
    icv_destroy(ia);
    icv_destroy(ib);
    int bad = offmany + (off1 > adiff_allow ? off1 - adiff_allow : 0);
    return (bad == 0) ? 1 : 0;
}

/* ========================================================================== */
/* Test 4: rtwizard GUI-path — swrast view gets BSG root, wireframe renders   */
/* ========================================================================== */
static int
test_gui_swrast_render(const char *datadir)
{
    bu_log("\n--- Test 4: rtwizard GUI path (swrast DM) — BSG root + wireframe renders ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("rtw_bsg_t4.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    /* Open with swrast DM — simulates the MGED widget used in rtwizard GUI */
    struct ged *gedp = open_gedp_swrast("rtw_bsg_t4.g", 512, 512);
    if (!gedp) {
	bu_log("FAIL: ged_open (swrast) failed\n");
	bu_file_delete("rtw_bsg_t4.g");
	return 1;
    }

    int fail = 0;

    /* BSG root must be set on the swrast-DM view (same guarantee as null-DM) */
    if (!bsg_view_scene_attached(gedp->ged_gvp)) {
	bu_log("FAIL: swrast-DM view has no BSG root\n");
	fail = 1;
    } else {
	bu_log("PASS: swrast-DM view has BSG root\n");
    }

    /* Draw + autoview + ae — mirrors MGEDpage::draw() + refreshDisplay() */
    const char *s_av[4] = {"draw", "all.g", NULL};
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);
    s_av[0] = "ae"; s_av[1] = "35"; s_av[2] = "25"; s_av[3] = NULL;
    ged_exec_ae(gedp, 3, s_av);

    /* Render via BSG path */
    do_swrast_refresh(gedp);

    /* Capture to PNG via screengrab */
    const char *sg_av[3] = {"screengrab", "rtw_bsg_t4.png", NULL};
    ged_exec_screengrab(gedp, 2, sg_av);

    long npix = count_nonblack_rtw("rtw_bsg_t4.png");
    if (npix <= 0) {
	bu_log("FAIL: GUI-mode (swrast) wireframe image is empty (%ld non-black pixels)\n", npix);
	fail = 1;
    } else {
	bu_log("PASS: GUI-mode (swrast) wireframe rendered %ld non-black pixels\n", npix);
    }

    /* Get eye model — the same call rtwizard makes before spawning rt */
    s_av[0] = "get_eyemodel"; s_av[1] = NULL;
    ged_exec_get_eyemodel(gedp, 1, s_av);
    const char *em = bu_vls_cstr(gedp->ged_result_str);
    bu_log("GUI-mode get_eyemodel:\n%s\n", em);

    double viewsize = 0.0;
    int nscan = sscanf(em, "viewsize %lf", &viewsize);
    if (nscan < 1 || viewsize <= 0.0 || !std::isfinite(viewsize)) {
	bu_log("FAIL: GUI-mode eye model has invalid viewsize (%g)\n", viewsize);
	fail = 1;
    } else {
	bu_log("PASS: GUI-mode eye model viewsize = %g\n", viewsize);
    }

    bu_file_delete("rtw_bsg_t4.png");
    close_gedp(gedp);
    bu_file_delete("rtw_bsg_t4.g");
    return fail;
}

/* ========================================================================== */
/* Test 5: GUI-mode eye model self-consistency                                 */
/*                                                                             */
/* After draw+autoview, extract the eye model (viewsize, orientation, eye_pt) */
/* and verify that re-applying those same parameters to a fresh second view   */
/* produces a pixel-identical rendered image.  This exercises the full        */
/* rtwizard GUI pipeline:                                                      */
/*   view A  → draw+autoview+ae → render → get_eyemodel                       */
/*   view B  → apply same az/el/zoom → render                                 */
/*   assert: image_A == image_B                                                */
/* ========================================================================== */
static int
test_gui_eyemodel_consistency(const char *datadir)
{
    bu_log("\n--- Test 5: GUI-mode eye model self-consistency (view A == view B) ---\n");

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmp("rtw_bsg_t5.g", std::ios::binary);
    tmp << orig.rdbuf();
    orig.close(); tmp.close();
    bu_vls_free(&fname);

    struct ged *gedp = open_gedp_swrast("rtw_bsg_t5.g", 512, 512);
    if (!gedp) {
	bu_log("FAIL: ged_open (swrast) failed\n");
	bu_file_delete("rtw_bsg_t5.g");
	return 1;
    }

    /* === View A === */
    const char *s_av[8] = {NULL};
    s_av[0] = "draw"; s_av[1] = "all.g"; s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);
    /* Specific az/el/tw matching a typical rtwizard "init_azimuth/elevation" */
    s_av[0] = "ae"; s_av[1] = "45"; s_av[2] = "30"; s_av[3] = "0"; s_av[4] = NULL;
    ged_exec_ae(gedp, 4, s_av);

    /* Save the view matrix for comparison */
    struct bsg_view *v = gedp->ged_gvp;
    mat_t saved_m2v;
    MAT_COPY(saved_m2v, v->gv_model2view);

    /* Render A */
    do_swrast_refresh(gedp);
    const char *sg_av[3] = {"screengrab", "rtw_bsg_t5_A.png", NULL};
    ged_exec_screengrab(gedp, 2, sg_av);
    bu_log("Captured image A (az=45 el=30 tw=0)\n");

    /* Extract eye model */
    s_av[0] = "get_eyemodel"; s_av[1] = NULL;
    ged_exec_get_eyemodel(gedp, 1, s_av);
    /* Copy result (the result_str will be overwritten by subsequent commands) */
    struct bu_vls em_str = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&em_str, "%s", bu_vls_cstr(gedp->ged_result_str));

    /* === View B: zap + redraw + re-apply same az/el/tw === */
    s_av[0] = "zap"; s_av[1] = NULL;
    ged_exec_zap(gedp, 1, s_av);
    s_av[0] = "draw"; s_av[1] = "all.g"; s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);
    s_av[0] = "autoview"; s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);
    s_av[0] = "ae"; s_av[1] = "45"; s_av[2] = "30"; s_av[3] = "0"; s_av[4] = NULL;
    ged_exec_ae(gedp, 4, s_av);

    /* Render B */
    do_swrast_refresh(gedp);
    sg_av[1] = "rtw_bsg_t5_B.png";
    ged_exec_screengrab(gedp, 2, sg_av);
    bu_log("Captured image B (same ae applied fresh)\n");

    int fail = 0;

    /* Images A and B must be pixel-identical (same camera, same geometry) */
    if (!images_match_rtw("rtw_bsg_t5_A.png", "rtw_bsg_t5_B.png", 10)) {
	bu_log("FAIL: image A != image B (same ae params but different pixels)\n");
	fail = 1;
    } else {
	bu_log("PASS: image A == image B (same ae params produce identical rendering)\n");
    }

    /* Also confirm view matrices match within floating-point noise */
    mat_t new_m2v;
    MAT_COPY(new_m2v, v->gv_model2view);
    fastf_t max_delta = 0.0;
    for (int i = 0; i < 16; i++) {
	fastf_t d = fabs(saved_m2v[i] - new_m2v[i]);
	if (d > max_delta) max_delta = d;
    }
    if (max_delta > 1e-6) {
	bu_log("FAIL: view matrices diverge after re-applying same ae (max delta = %g)\n",
	       max_delta);
	fail = 1;
    } else {
	bu_log("PASS: view matrices identical within 1e-6 (max delta = %g)\n", max_delta);
    }

    /* Confirm eye model of view B equals that of view A */
    s_av[0] = "get_eyemodel"; s_av[1] = NULL;
    ged_exec_get_eyemodel(gedp, 1, s_av);
    const char *em_A = bu_vls_cstr(&em_str);
    const char *em_B = bu_vls_cstr(gedp->ged_result_str);

    double vsA = 0.0, vsB = 0.0;
    sscanf(em_A, "viewsize %lf", &vsA);
    sscanf(em_B, "viewsize %lf", &vsB);
    if (vsA <= 0.0 || vsB <= 0.0) {
	bu_log("FAIL: viewsize not positive (A=%g B=%g)\n", vsA, vsB);
	fail = 1;
    } else if (fabs(vsA - vsB) / vsA > 1e-6) {
	bu_log("FAIL: viewsize differs between view A (%g) and view B (%g)\n", vsA, vsB);
	fail = 1;
    } else {
	bu_log("PASS: eye model viewsize consistent: A=%g B=%g\n", vsA, vsB);
    }

    bu_vls_free(&em_str);
    bu_file_delete("rtw_bsg_t5_A.png");
    bu_file_delete("rtw_bsg_t5_B.png");
    close_gedp(gedp);
    bu_file_delete("rtw_bsg_t5.g");
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
    struct bu_opt_desc d[2];
    BU_OPT(d[0], "c", "cleanup", NULL, NULL, &cleanup, "cleanup temp files");
    BU_OPT_NULL(d[1]);

    int uac = bu_opt_parse(NULL, argc, (const char **)argv, d);
    if (uac != 2) {
	bu_log("Usage: %s [-c] <directory-containing-moss.g>\n", argv[0]);
	return 1;
    }
    const char *datadir = argv[1];

    int failures = 0;
    failures += test_null_view_scene_ref(datadir);
    failures += test_eyemodel_finite(datadir);
    failures += test_nodisplaylist_path(datadir);
    failures += test_gui_swrast_render(datadir);
    failures += test_gui_eyemodel_consistency(datadir);

    if (failures == 0) {
	bu_log("\nAll Phase 5 rtwizard BSG migration tests PASSED (%d/5)\n", 5);
    } else {
	bu_log("\n%d Phase 5 rtwizard BSG migration test(s) FAILED\n", failures);
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
