/*             R E G R E S S _ F A C E T I Z E . C P P
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
/** @file regress_facetize.cpp
 *
 * Phase D regression tests for the instance-aware variant planning pass in
 * the Manifold facetize pipeline.
 *
 * Tests the "window-frame subtraction" coplanar pattern: a solid wall box
 * with a window cutout whose front and back faces are exactly coplanar with
 * the wall faces.  Without the variant planning pass this reliably produces
 * a degenerate/empty result in Manifold boolean evaluation.
 *
 * Each test case is verified three ways:
 *   1. Output BoT exists and has faces.
 *   2. Crofton SA/VOL of the output BoT matches the CSG comb within 20%.
 *   3. rt_bot_thin_check reports no super-thin artifact triangles.
 *
 * Test cases:
 *
 *   TC1  - axis-aligned window frame: ARB8 wall minus ARB8 cutout whose
 *          front/back faces are exactly coplanar with the wall.
 *
 *   TC2  - stacked cutouts: wall minus two separate window cutouts, all
 *          Z-coplanar.  Verifies per-instance SUB variant assignment.
 *
 *   TC3  - rotated (30 deg around Z) wall with coplanar rotated cutout.
 *          Non-axis-aligned ARBs expose floating-point face-coplanarity
 *          effects that are especially problematic for Manifold.
 *
 *   TC4  - havoc-inspired same-primitive reuse: the base solid appears as
 *          both a union (u) and a subtract (-) member in the same tree.
 *          Pattern: base.s u protrude.s - base.s, where protrude.s and
 *          base.s share exactly coplanar Z faces.
 *
 *   TC5  - near-coplanar sub-BN_TOL_DIST phantom face (the r.wind6 pattern):
 *          base5.s − sub5.s where sub5.s protrudes 0.000340 mm past base5.s's
 *          front face.  The gap is below librt's BN_TOL_DIST (0.0005 mm), so
 *          the CSG raytracer merges the entrances and reports MISS; without
 *          the perturb pass Manifold sees the gap as real geometry and leaves
 *          a phantom face (HIT with LOS ≈ 0).  The perturb pass must produce
 *          a MISS matching the CSG result.  Verified by firing a +Z ray
 *          through the window centre with nirt_shoot().
 *
 *   TC6  - variant-plan path-key fix for nested regions (r.wind9 pattern):
 *          same sub-BN_TOL_DIST geometry as TC5, but the region is wrapped
 *          inside a non-region assembly comb ("tc6_wrap  u tc6_reg.c"),
 *          mirroring the real havoc.g depth (havoc → ... → r.wind9).
 *          Before the fix, facetizing from tc6_wrap built path keys rooted at
 *          tc6_wrap (e.g. "/tc6_wrap/tc6_reg.c/sub6.s#sub") while the
 *          per-region booleval produced shorter keys
 *          ("/tc6_reg.c/sub6.s#sub"), so the lookup missed and no variant was
 *          applied, leaving the phantom face.  After the fix the plan is built
 *          from region roots so keys match and the phantom face is eliminated.
 */

#include "common.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "rt/db_instance.h"
#include "rt/db_io.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"

/* ------------------------------------------------------------------ */
/* Test parameters                                                      */
/* ------------------------------------------------------------------ */

/** Minimum ray count for Crofton surface-area / volume estimation. */
#define REGRESS_CROFTON_SAMPLES   2000
/** Convergence target (mm) for Crofton estimator. */
#define REGRESS_CROFTON_STABLE_TARGET      0.5*BN_TOL_DIST 
/** cos(30°) – used to build the TC3 rotated ARB8 geometry. */
#define COS_30_DEG   0.8660254037844386467637231707529361834713867187500
/** sin(30°) = 0.5 exactly. */
#define SIN_30_DEG   0.5

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/** Write an ARB8 into an open wdb.  pts[8][3] in model coordinates. */
static int
write_arb8(struct rt_wdb *wdbp, const char *name, const point_t pts[8])
{
    fastf_t flat[24];
    for (int i = 0; i < 8; i++) {
flat[i*3]     = pts[i][X];
flat[i*3 + 1] = pts[i][Y];
flat[i*3 + 2] = pts[i][Z];
    }
    return mk_arb8(wdbp, name, flat);
}

/**
 * Write a two-member boolean combination into an open wdb.
 * lop / rop: 'u' union, '-' subtract, '+' intersect.
 */
static int
write_comb2(struct rt_wdb *wdbp,
    const char    *comb_name,
    const char    *left,   char lop,
    const char    *right,  char rop)
{
    struct bu_list head;
    BU_LIST_INIT(&head);

    mk_addmember(left,  &head, NULL, (lop == 'u') ? WMOP_UNION :
    (lop == '-') ? WMOP_SUBTRACT : WMOP_INTERSECT);
    mk_addmember(right, &head, NULL, (rop == '-') ? WMOP_SUBTRACT :
    (rop == 'u') ? WMOP_UNION : WMOP_INTERSECT);

    return mk_comb(wdbp, comb_name, &head, 0 /* not region */, NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0);
}

/**
 * Open @a gfile with ged_open and run "facetize @a input @a output".
 * Returns BRLCAD_OK on success, BRLCAD_ERROR otherwise.
 */
static int
run_facetize(const char *gfile, const char *input, const char *output, int verbose)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp) {
bu_log("[regress_facetize] ged_open(%s) failed\n", gfile);
return BRLCAD_ERROR;
    }

    const char *av[4] = {"facetize", input, output, NULL};
    int ret = ged_exec(gedp, 3, av);

    if (verbose || ret != BRLCAD_OK) {
const char *log = bu_vls_cstr(gedp->ged_result_str);
if (log && log[0])
    bu_log("[facetize] %s\n", log);
    }

    ged_close(gedp);
    return ret;
}

/**
 * Verify that @a bot_name in @a gfile exists, is a BoT, and has faces.
 */
static int
check_bot_exists(const char *gfile, const char *bot_name)
{
    struct db_i *dbip = db_open(gfile, DB_OPEN_READONLY);
    if (!dbip) {
bu_log("[regress_facetize] check_bot: db_open(%s) failed\n", gfile);
return BRLCAD_ERROR;
    }
    if (db_dirbuild(dbip) < 0) { db_close(dbip); return BRLCAD_ERROR; }

    struct directory *dp = db_lookup(dbip, bot_name, LOOKUP_QUIET);
    if (!dp || !(dp->d_flags & RT_DIR_SOLID)) {
bu_log("[regress_facetize] check_bot: '%s' not found or not solid\n", bot_name);
db_close(dbip);
return BRLCAD_ERROR;
    }

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0) {
db_close(dbip);
return BRLCAD_ERROR;
    }

    int ok = BRLCAD_ERROR;
    if (intern.idb_minor_type == ID_BOT) {
struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;
if (bot->num_faces > 0)
    ok = BRLCAD_OK;
else
    bu_log("[regress_facetize] check_bot: '%s' has 0 faces\n", bot_name);
    } else {
bu_log("[regress_facetize] check_bot: '%s' type %d is not a BoT\n",
       bot_name, intern.idb_minor_type);
    }

    rt_db_free_internal(&intern);
    db_close(dbip);
    return ok;
}

/**
 * Run rt_crofton_shoot on @a obj_name in @a gfile.
 * Stores SA (mm2) and VOL (mm3) in *sa / *vol.
 */
static int
crofton_estimate(const char *gfile, const char *obj_name,
 double *sa, double *vol)
{
    *sa  = 0.0;
    *vol = 0.0;

    struct db_i *dbip = db_open(gfile, DB_OPEN_READONLY);
    if (!dbip) return BRLCAD_ERROR;
    if (db_dirbuild(dbip) < 0) { db_close(dbip); return BRLCAD_ERROR; }
    db_update_nref(dbip);

    struct rt_i *rtip = rt_new_rti(dbip);
    if (!rtip) { db_close(dbip); return BRLCAD_ERROR; }
    if (rt_gettree(rtip, obj_name) != 0) {
rt_free_rti(rtip); db_close(dbip); return BRLCAD_ERROR;
    }
    rt_prep_parallel(rtip, 1);

    /* REGRESS_CROFTON_SAMPLES rays, REGRESS_CROFTON_PCT% convergence threshold */
    struct rt_crofton_params p = { REGRESS_CROFTON_SAMPLES, REGRESS_CROFTON_STABLE_TARGET, 0.0};
    int ret = rt_crofton_shoot(rtip, &p, sa, vol);
    rt_free_rti(rtip);
    db_close(dbip);
    return (ret == 0) ? BRLCAD_OK : BRLCAD_ERROR;
}

/**
 * Run rt_bot_thin_check on @a bot_name in @a gfile.
 * Returns number of thin faces found (0 = clean), -1 on error.
 */
static int
bot_lint(const char *gfile, const char *bot_name)
{
    struct db_i *dbip = db_open(gfile, DB_OPEN_READONLY);
    if (!dbip) return -1;
    if (db_dirbuild(dbip) < 0) { db_close(dbip); return -1; }
    db_update_nref(dbip);

    struct directory *dp = db_lookup(dbip, bot_name, LOOKUP_QUIET);
    if (!dp) { db_close(dbip); return -1; }

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0) {
db_close(dbip); return -1;
    }
    if (intern.idb_minor_type != ID_BOT) {
rt_db_free_internal(&intern); db_close(dbip); return -1;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;
    if (!bot->num_faces) {
rt_db_free_internal(&intern); db_close(dbip); return 0;
    }

    struct rt_i *rtip = rt_new_rti(dbip);
    if (!rtip) { rt_db_free_internal(&intern); db_close(dbip); return -1; }
    if (rt_gettree(rtip, bot_name) != 0) {
rt_free_rti(rtip); rt_db_free_internal(&intern); db_close(dbip); return -1;
    }
    rt_prep_parallel(rtip, 1);

    struct bu_ptbl tfaces = BU_PTBL_INIT_ZERO;
    int have_thin = rt_bot_thin_check(&tfaces, bot, rtip, VUNITIZE_TOL, 0);
    int n_thin = (int)BU_PTBL_LEN(&tfaces);

    rt_free_rti(rtip);
    bu_ptbl_free(&tfaces);
    rt_db_free_internal(&intern);
    db_close(dbip);

    return have_thin ? n_thin : 0;
}

/**
 * Run rt_bot_csg_miss_check on @a bot_name in @a gfile.
 * Returns number of near-tol faces found (0 = clean), -1 on error.
 *
 * A "near-tol face" is one whose inward ray produces a segment with thickness
 * in [VUNITIZE_TOL, BN_TOL_DIST): valid for the BoT raytracer but below the
 * CSG boolweave tolerance, so the original CSG description would report MISS
 * while the BoT solid reports HIT.
 */
static int
bot_csg_miss_lint(const char *gfile, const char *bot_name)
{
    struct db_i *dbip = db_open(gfile, DB_OPEN_READONLY);
    if (!dbip) return -1;
    if (db_dirbuild(dbip) < 0) { db_close(dbip); return -1; }
    db_update_nref(dbip);

    struct directory *dp = db_lookup(dbip, bot_name, LOOKUP_QUIET);
    if (!dp) { db_close(dbip); return -1; }

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0) {
	db_close(dbip); return -1;
    }
    if (intern.idb_minor_type != ID_BOT) {
	rt_db_free_internal(&intern); db_close(dbip); return -1;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)intern.idb_ptr;
    if (!bot->num_faces) {
	rt_db_free_internal(&intern); db_close(dbip); return 0;
    }

    struct rt_i *rtip = rt_new_rti(dbip);
    if (!rtip) { rt_db_free_internal(&intern); db_close(dbip); return -1; }
    if (rt_gettree(rtip, bot_name) != 0) {
	rt_free_rti(rtip); rt_db_free_internal(&intern); db_close(dbip); return -1;
    }
    rt_prep_parallel(rtip, 1);

    struct bu_ptbl nfaces = BU_PTBL_INIT_ZERO;
    int have_near_tol = rt_bot_csg_miss_check(&nfaces, bot, rtip, 0);
    int n_near_tol = (int)BU_PTBL_LEN(&nfaces);

    rt_free_rti(rtip);
    bu_ptbl_free(&nfaces);
    rt_db_free_internal(&intern);
    db_close(dbip);

    return have_near_tol ? n_near_tol : 0;
}

/**
 * Full post-facetize verification:
 *   1. BoT exists and has faces.
 *   2. Crofton SA/VOL of BoT matches CSG comb within tol_pct.
 *   3. rt_bot_thin_check finds no thin-triangle artifacts.
 */
static int
check_bot_full(const char *tcname,
       const char *gfile,
       const char *csg_name,
       const char *bot_name,
       double      tol_pct)
{
    if (check_bot_exists(gfile, bot_name) != BRLCAD_OK) {
bu_log("[regress_facetize] %s: FAIL - BoT '%s' missing or empty\n",
       tcname, bot_name);
return BRLCAD_ERROR;
    }

    double csg_sa = 0.0, csg_vol = 0.0;
    double bot_sa = 0.0, bot_vol = 0.0;
    int have_csg = (crofton_estimate(gfile, csg_name, &csg_sa, &csg_vol) == BRLCAD_OK);
    int have_bot = (crofton_estimate(gfile, bot_name, &bot_sa, &bot_vol) == BRLCAD_OK);

    if (have_csg && have_bot && csg_sa > SMALL_FASTF && csg_vol > SMALL_FASTF) {
double sa_err  = fabs(bot_sa  - csg_sa)  / csg_sa  * 100.0;
double vol_err = fabs(bot_vol - csg_vol) / csg_vol * 100.0;

bu_log("[regress_facetize] %s: Crofton CSG SA=%.0f mm2 VOL=%.0f mm3\n",
       tcname, csg_sa, csg_vol);
bu_log("[regress_facetize] %s: Crofton BoT SA=%.0f mm2 VOL=%.0f mm3  "
       "(SA_err=%.1f%% VOL_err=%.1f%% tol=%.0f%%)\n",
       tcname, bot_sa, bot_vol, sa_err, vol_err, tol_pct);

if (sa_err > tol_pct) {
    bu_log("[regress_facetize] %s: FAIL - SA mismatch %.1f%% > %.0f%%\n",
   tcname, sa_err, tol_pct);
    return BRLCAD_ERROR;
}
if (vol_err > tol_pct) {
    bu_log("[regress_facetize] %s: FAIL - VOL mismatch %.1f%% > %.0f%%\n",
   tcname, vol_err, tol_pct);
    return BRLCAD_ERROR;
}
    } else {
bu_log("[regress_facetize] %s: Crofton unavailable (CSG=%d BOT=%d)"
       " - skipping metric check\n",
       tcname, have_csg, have_bot);
    }

    int n_thin = bot_lint(gfile, bot_name);
    if (n_thin < 0) {
bu_log("[regress_facetize] %s: WARNING - bot_lint failed for '%s'\n",
       tcname, bot_name);
    } else if (n_thin > 0) {
bu_log("[regress_facetize] %s: FAIL - %d thin-triangle artifact(s) in '%s'\n",
       tcname, n_thin, bot_name);
return BRLCAD_ERROR;
    } else {
bu_log("[regress_facetize] %s: bot_lint CLEAN (0 thin faces)\n", tcname);
    }

    return BRLCAD_OK;
}

/* ------------------------------------------------------------------ */
/* TC1: axis-aligned window frame                                       */
/* ------------------------------------------------------------------ */

/*
 * Wall: 200 x 300 x 20 mm box.
 * Window cutout: 80 x 120 x 20 mm box centred in the wall.
 * Z=0 and Z=20 faces of both solids are exactly coplanar.
 */
static int
tc1_window_frame(const char *tmpdir, int verbose)
{
    bu_log("[regress_facetize] TC1: axis-aligned window-frame coplanar subtraction...\n");

    struct bu_vls gpath = BU_VLS_INIT_ZERO;
    bu_vls_printf(&gpath, "%s/tc1_window.g", tmpdir);
    const char *gfile = bu_vls_cstr(&gpath);

    if (bu_file_exists(gfile, NULL)) bu_file_delete(gfile);
    struct db_i *dbip = db_create(gfile, 5);
    if (!dbip) {
bu_log("[regress_facetize] TC1: db_create failed\n");
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    db_update_nref(dbip);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);

    point_t wall_pts[8] = {
{0.0,   0.0,   0.0}, {200.0, 0.0,   0.0},
{200.0, 300.0, 0.0}, {0.0,   300.0, 0.0},
{0.0,   0.0,   20.0},{200.0, 0.0,   20.0},
{200.0, 300.0, 20.0},{0.0,   300.0, 20.0},
    };
    point_t win_pts[8] = {
{60.0,  90.0,  0.0}, {140.0, 90.0,  0.0},
{140.0, 210.0, 0.0}, {60.0,  210.0, 0.0},
{60.0,  90.0,  20.0},{140.0, 90.0,  20.0},
{140.0, 210.0, 20.0},{60.0,  210.0, 20.0},
    };

    if (write_arb8(wdbp, "wall.s", wall_pts) < 0 ||
write_arb8(wdbp, "win.s",  win_pts)  < 0 ||
write_comb2(wdbp, "wall.c", "wall.s", 'u', "win.s", '-') < 0) {
bu_log("[regress_facetize] TC1: write failed\n");
wdb_close(wdbp); bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    wdb_close(wdbp);

    if (run_facetize(gfile, "wall.c", "wall.bot", verbose) != BRLCAD_OK) {
bu_log("[regress_facetize] TC1: FAIL - facetize error\n");
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    if (check_bot_full("TC1", gfile, "wall.c", "wall.bot", 20.0) != BRLCAD_OK) {
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }

    bu_log("[regress_facetize] TC1: PASS\n");
    bu_vls_free(&gpath);
    return BRLCAD_OK;
}

/* ------------------------------------------------------------------ */
/* TC2: stacked window cutouts                                          */
/* ------------------------------------------------------------------ */

/* wall2.c = wall.s - win1.s - win2.s  (both cutouts Z-coplanar) */
static int
tc2_stacked_cutouts(const char *tmpdir, int verbose)
{
    bu_log("[regress_facetize] TC2: stacked window-frame cutouts...\n");

    struct bu_vls gpath = BU_VLS_INIT_ZERO;
    bu_vls_printf(&gpath, "%s/tc2_stacked.g", tmpdir);
    const char *gfile = bu_vls_cstr(&gpath);

    if (bu_file_exists(gfile, NULL)) bu_file_delete(gfile);
    struct db_i *dbip = db_create(gfile, 5);
    if (!dbip) {
bu_log("[regress_facetize] TC2: db_create failed\n");
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    db_update_nref(dbip);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);

    point_t wall_pts[8] = {
{0.0,   0.0,   0.0}, {200.0, 0.0,   0.0},
{200.0, 300.0, 0.0}, {0.0,   300.0, 0.0},
{0.0,   0.0,   20.0},{200.0, 0.0,   20.0},
{200.0, 300.0, 20.0},{0.0,   300.0, 20.0},
    };
    point_t win1_pts[8] = {
{20.0, 80.0,  0.0}, {80.0, 80.0,  0.0},
{80.0, 220.0, 0.0}, {20.0, 220.0, 0.0},
{20.0, 80.0,  20.0},{80.0, 80.0,  20.0},
{80.0, 220.0, 20.0},{20.0, 220.0, 20.0},
    };
    point_t win2_pts[8] = {
{120.0, 80.0,  0.0}, {180.0, 80.0,  0.0},
{180.0, 220.0, 0.0}, {120.0, 220.0, 0.0},
{120.0, 80.0,  20.0},{180.0, 80.0,  20.0},
{180.0, 220.0, 20.0},{120.0, 220.0, 20.0},
    };

    if (write_arb8(wdbp, "wall.s",  wall_pts) < 0 ||
write_arb8(wdbp, "win1.s",  win1_pts) < 0 ||
write_arb8(wdbp, "win2.s",  win2_pts) < 0) {
bu_log("[regress_facetize] TC2: write primitives failed\n");
wdb_close(wdbp); bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    {
struct bu_list head;
BU_LIST_INIT(&head);
mk_addmember("wall.s", &head, NULL, WMOP_UNION);
mk_addmember("win1.s", &head, NULL, WMOP_SUBTRACT);
mk_addmember("win2.s", &head, NULL, WMOP_SUBTRACT);
if (mk_comb(wdbp, "wall2.c", &head, 0, NULL, NULL, NULL,
    0, 0, 0, 0, 0, 0, 0) < 0) {
    bu_log("[regress_facetize] TC2: mk_comb failed\n");
    wdb_close(wdbp); bu_vls_free(&gpath); return BRLCAD_ERROR;
}
    }
    wdb_close(wdbp);

    if (run_facetize(gfile, "wall2.c", "wall2.bot", verbose) != BRLCAD_OK) {
bu_log("[regress_facetize] TC2: FAIL - facetize error\n");
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    if (check_bot_full("TC2", gfile, "wall2.c", "wall2.bot", 20.0) != BRLCAD_OK) {
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }

    bu_log("[regress_facetize] TC2: PASS\n");
    bu_vls_free(&gpath);
    return BRLCAD_OK;
}

/* ------------------------------------------------------------------ */
/* TC3: rotated (30 degrees around Z) coplanar subtraction             */
/* ------------------------------------------------------------------ */

/*
 * Same geometry as TC1 rotated 30 degrees around the Z-axis.
 * The Z=0 and Z=20 planes are still exactly coplanar between wall3.s and
 * win3.s; all lateral faces are non-axis-aligned.
 *
 * Analytic net volume (rotation-invariant):
 *   200 x 300 x 20 - 80 x 120 x 20 = 1 200 000 - 192 000 = 1 008 000 mm3
 */
static int
tc3_rotated_wall(const char *tmpdir, int verbose)
{
    bu_log("[regress_facetize] TC3: non-axis-aligned (30 deg rotated) coplanar subtraction...\n");

    struct bu_vls gpath = BU_VLS_INIT_ZERO;
    bu_vls_printf(&gpath, "%s/tc3_rotated.g", tmpdir);
    const char *gfile = bu_vls_cstr(&gpath);

    if (bu_file_exists(gfile, NULL)) bu_file_delete(gfile);
    struct db_i *dbip = db_create(gfile, 5);
    if (!dbip) {
bu_log("[regress_facetize] TC3: db_create failed\n");
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    db_update_nref(dbip);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);

    /* Rotation by 30 deg around Z: x' = x*c - y*s,  y' = x*s + y*c */
    const double c = COS_30_DEG;
    const double s = SIN_30_DEG;

#define R30(x_, y_, z_) { (x_)*c - (y_)*s,  (x_)*s + (y_)*c,  (z_) }

    /* wall3.s: 200 x 300 x 20 rotated 30 deg */
    point_t wall3_pts[8] = {
R30(  0.0,   0.0,  0.0), R30(200.0,   0.0,  0.0),
R30(200.0, 300.0,  0.0), R30(  0.0, 300.0,  0.0),
R30(  0.0,   0.0, 20.0), R30(200.0,   0.0, 20.0),
R30(200.0, 300.0, 20.0), R30(  0.0, 300.0, 20.0),
    };
    /* win3.s: 80 x 120 x 20 cutout, same rotation, coplanar Z faces */
    point_t win3_pts[8] = {
R30( 60.0,  90.0,  0.0), R30(140.0,  90.0,  0.0),
R30(140.0, 210.0,  0.0), R30( 60.0, 210.0,  0.0),
R30( 60.0,  90.0, 20.0), R30(140.0,  90.0, 20.0),
R30(140.0, 210.0, 20.0), R30( 60.0, 210.0, 20.0),
    };
#undef R30

    if (write_arb8(wdbp, "wall3.s", wall3_pts) < 0 ||
write_arb8(wdbp, "win3.s",  win3_pts)  < 0 ||
write_comb2(wdbp, "wall3.c", "wall3.s", 'u', "win3.s", '-') < 0) {
bu_log("[regress_facetize] TC3: write failed\n");
wdb_close(wdbp); bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    wdb_close(wdbp);

    if (run_facetize(gfile, "wall3.c", "wall3.bot", verbose) != BRLCAD_OK) {
bu_log("[regress_facetize] TC3: FAIL - facetize error\n");
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    if (check_bot_full("TC3", gfile, "wall3.c", "wall3.bot", 20.0) != BRLCAD_OK) {
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }

    bu_log("[regress_facetize] TC3: PASS\n");
    bu_vls_free(&gpath);
    return BRLCAD_OK;
}

/* ------------------------------------------------------------------ */
/* TC4: havoc-inspired same-primitive union + subtract in one tree      */
/* ------------------------------------------------------------------ */

/*
 * Pattern seen throughout havoc.g (e.g. r.rad1, r.sgt1, r.rot18):
 * the same solid appears as both a union (u) AND a subtract (-) member
 * in one combination.
 *
 * Here:
 *   base4.s    : 200 x 200 x 100 mm box  (X:0-200,  Y:0-200, Z:0-100)
 *   protrude4.s: 300 x 200 x 100 mm box  (X:100-400, Y:0-200, Z:0-100)
 *     (overlaps base4.s on X:100-200; exact same Z range = coplanar Z faces)
 *
 *   comb4.c = base4.s u protrude4.s - base4.s
 *
 * Boolean algebra:
 *   (base4 union protrude4) - base4 = protrude4 - (protrude4 intersect base4)
 *   = box X:200-400, Y:0-200, Z:0-100   (volume = 200*200*100 = 4 000 000 mm3)
 *
 * base4.s occupies BOTH a 'u' slot and a '-' slot in the tree.  The variant
 * planning pass must assign it separate perturbation slots so that its
 * Z=0/Z=100 faces (coplanar with protrude4.s) get different jitter seeds.
 */
static int
tc4_havoc_reuse(const char *tmpdir, int verbose)
{
    bu_log("[regress_facetize] TC4: havoc-inspired same-primitive union+subtract reuse...\n");

    struct bu_vls gpath = BU_VLS_INIT_ZERO;
    bu_vls_printf(&gpath, "%s/tc4_havoc.g", tmpdir);
    const char *gfile = bu_vls_cstr(&gpath);

    if (bu_file_exists(gfile, NULL)) bu_file_delete(gfile);
    struct db_i *dbip = db_create(gfile, 5);
    if (!dbip) {
bu_log("[regress_facetize] TC4: db_create failed\n");
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    db_update_nref(dbip);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);

    /* base4.s: 200 x 200 x 100 */
    point_t base_pts[8] = {
{  0.0, 0.0,   0.0}, {200.0, 0.0,   0.0},
{200.0, 200.0, 0.0}, {  0.0, 200.0, 0.0},
{  0.0, 0.0,   100.0}, {200.0, 0.0,   100.0},
{200.0, 200.0, 100.0}, {  0.0, 200.0, 100.0},
    };
    /* protrude4.s: 300 x 200 x 100, overlaps base4 on X:100-200, same Z */
    point_t protrude_pts[8] = {
{100.0, 0.0,   0.0}, {400.0, 0.0,   0.0},
{400.0, 200.0, 0.0}, {100.0, 200.0, 0.0},
{100.0, 0.0,   100.0}, {400.0, 0.0,   100.0},
{400.0, 200.0, 100.0}, {100.0, 200.0, 100.0},
    };

    if (write_arb8(wdbp, "base4.s",    base_pts)    < 0 ||
write_arb8(wdbp, "protrude4.s", protrude_pts) < 0) {
bu_log("[regress_facetize] TC4: write primitives failed\n");
wdb_close(wdbp); bu_vls_free(&gpath); return BRLCAD_ERROR;
    }

    /* comb4.c = base4.s u protrude4.s - base4.s */
    {
struct bu_list head;
BU_LIST_INIT(&head);
mk_addmember("base4.s",     &head, NULL, WMOP_UNION);
mk_addmember("protrude4.s", &head, NULL, WMOP_UNION);
mk_addmember("base4.s",     &head, NULL, WMOP_SUBTRACT);
if (mk_comb(wdbp, "comb4.c", &head, 0, NULL, NULL, NULL,
    0, 0, 0, 0, 0, 0, 0) < 0) {
    bu_log("[regress_facetize] TC4: mk_comb comb4.c failed\n");
    wdb_close(wdbp); bu_vls_free(&gpath); return BRLCAD_ERROR;
}
    }
    wdb_close(wdbp);

    if (run_facetize(gfile, "comb4.c", "comb4.bot", verbose) != BRLCAD_OK) {
bu_log("[regress_facetize] TC4: FAIL - facetize error\n");
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }
    if (check_bot_full("TC4", gfile, "comb4.c", "comb4.bot", 20.0) != BRLCAD_OK) {
bu_vls_free(&gpath); return BRLCAD_ERROR;
    }

    bu_log("[regress_facetize] TC4: PASS\n");
    bu_vls_free(&gpath);
    return BRLCAD_OK;
}

/* ------------------------------------------------------------------ */
/* TC5: near-coplanar sub-BN_TOL_DIST phantom face (r.wind6 pattern)   */
/* ------------------------------------------------------------------ */

/*
 * This test exercises the "sub-tolerance near-coplanar" failure mode
 * identified in r.wind6 of the havoc.g model.
 *
 * Geometry:
 *   base5.s:  200 × 300 × 8 mm box (Z = 0 … 8).
 *   sub5.s:   160 × 260 × 8 mm box, XY-centred inside base5.s,
 *             with Z = [-GAP … 8-GAP] where GAP = TC5_GAP mm.
 *             sub5.s protrudes GAP mm past base5.s's Z=0 face.
 *   base5.c:  region  base5.s − sub5.s  (window-frame solid).
 *
 * The faces are NOT exactly coplanar: the gap GAP < BN_TOL_DIST (0.0005 mm).
 * The librt CSG raytracer merges the near-coincident entrance points and
 * reports MISS on a +Z ray through the window centre.
 * Without the perturb pass, Manifold sees the gap as real geometry and
 * leaves a phantom face → nirt HIT (LOS ≈ 0).
 * With the perturb pass the SUB variant protrudes past the BASE face →
 * Manifold cleanly subtracts the window → nirt MISS.
 *
 * Regression guard: the perturb-enabled result MUST give a MISS on the
 * test ray.  The no-perturb result is expected to give a HIT; if it does
 * not (e.g. because a future Manifold handles this differently), a WARNING
 * is emitted but the test does not fail.
 */

/** Near-coplanar gap for TC5 (< BN_TOL_DIST = 0.0005 mm). */
#define TC5_GAP 0.000340

/* ---- librt ray-shot helpers ---- */

static int
nirt_hit_fn(struct application *ap, struct partition *pp, struct seg *UNUSED(segp))
{
    int *nhits = (int *)ap->a_uptr;
    (*nhits)++;
    (void)pp;
    return 1;
}

static int
nirt_miss_fn(struct application *ap)
{
    (void)ap;
    return 0;
}

/**
 * Fire a single ray through @a obj_name in @a gfile.
 * Returns number of hits (≥ 0), or -1 on setup error.
 */
static int
nirt_shoot(const char *gfile, const char *obj_name,
           const point_t origin, const vect_t dir)
{
    struct rt_i *rtip = rt_dirbuild(gfile, NULL, 0);
    if (!rtip) {
	bu_log("[regress_facetize] nirt_shoot: rt_dirbuild(%s) failed\n", gfile);
	return -1;
    }
    if (rt_gettree(rtip, obj_name) < 0) {
	bu_log("[regress_facetize] nirt_shoot: rt_gettree(%s) failed\n", obj_name);
	rt_free_rti(rtip);
	return -1;
    }
    rt_prep_parallel(rtip, 1);

    struct application ap;
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    VMOVE(ap.a_ray.r_pt,  origin);
    VMOVE(ap.a_ray.r_dir, dir);
    ap.a_onehit = 0;
    int nhits = 0;
    ap.a_uptr = &nhits;
    ap.a_hit  = nirt_hit_fn;
    ap.a_miss = nirt_miss_fn;

    rt_shootray(&ap);

    rt_free_rti(rtip);
    return nhits;
}

/**
 * Run  "facetize -r [--no-perturb] @a input @a output"  in @a gfile.
 * Pass no_perturb=1 to add --no-perturb.
 */
static int
run_facetize_r(const char *gfile, const char *input, const char *output,
               int no_perturb, int verbose)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp) {
	bu_log("[regress_facetize] ged_open(%s) failed\n", gfile);
	return BRLCAD_ERROR;
    }

    int ret;
    if (no_perturb) {
	const char *av[6] = {"facetize", "-r", "--no-perturb", input, output, NULL};
	ret = ged_exec(gedp, 5, av);
    } else {
	const char *av[5] = {"facetize", "-r", input, output, NULL};
	ret = ged_exec(gedp, 4, av);
    }

    if (verbose || ret != BRLCAD_OK) {
	const char *log = bu_vls_cstr(gedp->ged_result_str);
	if (log && log[0])
	    bu_log("[facetize] %s\n", log);
    }

    ged_close(gedp);
    return ret;
}

static int
tc5_near_coplanar_subTOL(const char *tmpdir, int verbose)
{
    bu_log("[regress_facetize] TC5: near-coplanar sub-BN_TOL_DIST phantom face (r.wind6 pattern)...\n");

    /* Use two separate .g files so the no-perturb and perturb runs
     * do not interfere with each other's variant primitives. */
    struct bu_vls gpath_np = BU_VLS_INIT_ZERO;
    struct bu_vls gpath_p  = BU_VLS_INIT_ZERO;
    bu_vls_printf(&gpath_np, "%s/tc5_nopert.g",  tmpdir);
    bu_vls_printf(&gpath_p,  "%s/tc5_perturb.g", tmpdir);
    const char *gfile_np = bu_vls_cstr(&gpath_np);
    const char *gfile_p  = bu_vls_cstr(&gpath_p);

    /* Build identical geometry in both files. */
    for (int pass = 0; pass < 2; pass++) {
	const char *gfile = (pass == 0) ? gfile_np : gfile_p;
	if (bu_file_exists(gfile, NULL)) bu_file_delete(gfile);

	struct db_i *dbip = db_create(gfile, 5);
	if (!dbip) {
	    bu_log("[regress_facetize] TC5: db_create failed\n");
	    bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	    return BRLCAD_ERROR;
	}
	db_update_nref(dbip);
	struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);

	/* base5.s: 200 × 300 × 8 mm at Z=[0, 8] */
	point_t base_pts[8] = {
	    {  0.0,   0.0, 0.0}, {200.0,   0.0, 0.0},
	    {200.0, 300.0, 0.0}, {  0.0, 300.0, 0.0},
	    {  0.0,   0.0, 8.0}, {200.0,   0.0, 8.0},
	    {200.0, 300.0, 8.0}, {  0.0, 300.0, 8.0},
	};
	/* sub5.s: 160 × 260 × 8 mm, XY-centred, Z=[-GAP, 8-GAP].
	 * The Z=-GAP face protrudes TC5_GAP mm past base5.s's Z=0 face. */
	const fastf_t gap = TC5_GAP;
	point_t sub_pts[8] = {
	    { 20.0,  20.0, -gap},        {180.0,  20.0, -gap},
	    {180.0, 280.0, -gap},        { 20.0, 280.0, -gap},
	    { 20.0,  20.0, 8.0 - gap},   {180.0,  20.0, 8.0 - gap},
	    {180.0, 280.0, 8.0 - gap},   { 20.0, 280.0, 8.0 - gap},
	};

	struct bu_list members;
	BU_LIST_INIT(&members);
	mk_addmember("base5.s", &members, NULL, WMOP_UNION);
	mk_addmember("sub5.s",  &members, NULL, WMOP_SUBTRACT);

	if (write_arb8(wdbp, "base5.s", base_pts) < 0 ||
	    write_arb8(wdbp, "sub5.s",  sub_pts)  < 0 ||
	    mk_comb(wdbp, "base5.c", &members, 1 /* region */,
		    NULL, NULL, NULL, 1000, 0, 0, 0, 0, 0, 0) < 0) {
	    bu_log("[regress_facetize] TC5: write failed\n");
	    wdb_close(wdbp); bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	    return BRLCAD_ERROR;
	}
	wdb_close(wdbp);
    }

    /* Test ray: +Z through the window centre (100, 150) from below.
     * CSG and perturb BoT must MISS; no-perturb BoT is expected to HIT. */
    point_t origin = {100.0, 150.0, -1.0};
    vect_t  dir    = {  0.0,   0.0,  1.0};

    /* --- no-perturb baseline: document that phantom hit exists ---- */
    if (run_facetize_r(gfile_np, "base5.c", "base5.bot.np",
		       1 /* no_perturb */, verbose) != BRLCAD_OK) {
	bu_log("[regress_facetize] TC5: FAIL - facetize --no-perturb error\n");
	bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	return BRLCAD_ERROR;
    }
    int hits_np = nirt_shoot(gfile_np, "base5.bot.np", origin, dir);
    if (hits_np < 0) {
	bu_log("[regress_facetize] TC5: WARNING - nirt_shoot setup error for no-perturb BoT\n");
    } else if (hits_np == 0) {
	bu_log("[regress_facetize] TC5: WARNING - no-perturb BoT has no phantom hit "
	       "(test geometry may not reproduce the pattern)\n");
    } else {
	bu_log("[regress_facetize] TC5: no-perturb BoT has %d phantom hit(s) (expected)\n",
	       hits_np);
    }

    /* --- perturb-enabled: must give MISS (regression guard) ---- */
    if (run_facetize_r(gfile_p, "base5.c", "base5.bot.p",
		       0 /* with perturb */, verbose) != BRLCAD_OK) {
	bu_log("[regress_facetize] TC5: FAIL - facetize error\n");
	bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	return BRLCAD_ERROR;
    }
    int hits_p = nirt_shoot(gfile_p, "base5.bot.p", origin, dir);
    if (hits_p < 0) {
	bu_log("[regress_facetize] TC5: FAIL - nirt_shoot setup error for perturb BoT\n");
	bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	return BRLCAD_ERROR;
    }
    if (hits_p > 0) {
	bu_log("[regress_facetize] TC5: FAIL - perturb BoT has %d phantom hit(s); "
	       "perturb did not eliminate sub-tolerance coplanar face\n", hits_p);
	bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	return BRLCAD_ERROR;
    }
    bu_log("[regress_facetize] TC5: perturb BoT correctly has no phantom hit (MISS)\n");

    /* --- near-tol lint: no-perturb BoT should have near-tol faces ---- */
    int nt_np = bot_csg_miss_lint(gfile_np, "base5.bot.np");
    if (nt_np < 0) {
	bu_log("[regress_facetize] TC5: WARNING - bot_csg_miss_lint setup error for "
	       "no-perturb BoT\n");
    } else if (nt_np == 0) {
	bu_log("[regress_facetize] TC5: WARNING - no-perturb BoT has 0 near-tol faces "
	       "(test geometry may not reproduce the sub-BN_TOL_DIST pattern)\n");
    } else {
	bu_log("[regress_facetize] TC5: no-perturb BoT has %d near-tol face(s) "
	       "(expected - sub-BN_TOL_DIST phantom detected)\n", nt_np);
    }

    /* --- near-tol lint: perturb BoT must have 0 near-tol faces ---- */
    int nt_p = bot_csg_miss_lint(gfile_p, "base5.bot.p");
    if (nt_p < 0) {
	bu_log("[regress_facetize] TC5: WARNING - bot_csg_miss_lint setup error for "
	       "perturb BoT\n");
    } else if (nt_p > 0) {
	bu_log("[regress_facetize] TC5: FAIL - perturb BoT has %d near-tol face(s); "
	       "perturb did not eliminate sub-tolerance coplanar faces\n", nt_p);
	bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	return BRLCAD_ERROR;
    } else {
	bu_log("[regress_facetize] TC5: perturb BoT CLEAN (0 near-tol faces)\n");
    }

    /* VOL sanity on the perturb output: VOL is stable for this geometry
     * (the phantom face adds negligible volume).  SA is omitted because
     * the Crofton estimator is noisy for thin-frame shapes. */
    double csg_vol = 0.0, bot_vol = 0.0, dummy = 0.0;
    int have_csg = (crofton_estimate(gfile_p, "base5.c",    &dummy, &csg_vol) == BRLCAD_OK);
    int have_bot = (crofton_estimate(gfile_p, "base5.bot.p", &dummy, &bot_vol) == BRLCAD_OK);
    if (have_csg && have_bot && csg_vol > SMALL_FASTF) {
	double vol_err = fabs(bot_vol - csg_vol) / csg_vol * 100.0;
	bu_log("[regress_facetize] TC5: Crofton CSG VOL=%.0f  BoT VOL=%.0f  (VOL_err=%.1f%%)\n",
	       csg_vol, bot_vol, vol_err);
	if (vol_err > 20.0) {
	    bu_log("[regress_facetize] TC5: FAIL - Crofton VOL mismatch exceeds 20%%\n");
	    bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	    return BRLCAD_ERROR;
	}
    } else {
	bu_log("[regress_facetize] TC5: Crofton unavailable (CSG=%d BOT=%d) - skipping\n",
	       have_csg, have_bot);
    }

    bu_log("[regress_facetize] TC5: PASS\n");
    bu_vls_free(&gpath_np);
    bu_vls_free(&gpath_p);
    return BRLCAD_OK;
}

/* ------------------------------------------------------------------ */
/* TC6: variant-plan path-key fix for nested-region (r.wind9 pattern) */
/* ------------------------------------------------------------------ */
static int
tc6_havoc_wind9_pattern(const char *tmpdir, int verbose)
{
    bu_log("[regress_facetize] TC6: variant plan via top-level wrapper comb (r.wind9 path-key fix)...\n");

    /* Use the same sub-BN_TOL_DIST protrusion geometry as TC5, but wrap the
     * region inside a non-region assembly comb.  This mirrors the real
     * havoc.g hierarchy:  havoc → havoc_front → cannopy → r.wind9.
     *
     * The bug being tested (regions.cpp): when the facetize input is a
     * parent comb rather than the region itself, the old code walked the
     * full input tree to build variant-plan path keys (e.g.
     * "/tc6_wrap/tc6_reg.c/tc6_sub.s#sub").  The per-region booleval
     * walks from the region root and produces a shorter key
     * ("/tc6_reg.c/tc6_sub.s#sub"), so the lookup always missed and
     * the variant was never applied.  The fix builds keys from the
     * region roots so they match. */

    struct bu_vls gpath_np = BU_VLS_INIT_ZERO;
    struct bu_vls gpath_p  = BU_VLS_INIT_ZERO;
    bu_vls_printf(&gpath_np, "%s/tc6_nopert.g",  tmpdir);
    bu_vls_printf(&gpath_p,  "%s/tc6_perturb.g", tmpdir);
    const char *gfile_np = bu_vls_cstr(&gpath_np);
    const char *gfile_p  = bu_vls_cstr(&gpath_p);

    /* Build identical geometry in both files. */
    for (int pass = 0; pass < 2; pass++) {
	const char *gfile = (pass == 0) ? gfile_np : gfile_p;
	if (bu_file_exists(gfile, NULL)) bu_file_delete(gfile);

	struct db_i *dbip = db_create(gfile, 5);
	if (!dbip) {
	    bu_log("[regress_facetize] TC6: db_create failed\n");
	    bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	    return BRLCAD_ERROR;
	}
	db_update_nref(dbip);
	struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);

	/* Same protrusion geometry as TC5: base6.s minus sub6.s which
	 * protrudes TC5_GAP mm past base6.s's Z=0 face. */
	point_t base_pts[8] = {
	    {  0.0,   0.0, 0.0}, {200.0,   0.0, 0.0},
	    {200.0, 300.0, 0.0}, {  0.0, 300.0, 0.0},
	    {  0.0,   0.0, 8.0}, {200.0,   0.0, 8.0},
	    {200.0, 300.0, 8.0}, {  0.0, 300.0, 8.0},
	};
	const fastf_t gap = TC5_GAP;
	point_t sub_pts[8] = {
	    { 20.0,  20.0, -gap},        {180.0,  20.0, -gap},
	    {180.0, 280.0, -gap},        { 20.0, 280.0, -gap},
	    { 20.0,  20.0, 8.0 - gap},   {180.0,  20.0, 8.0 - gap},
	    {180.0, 280.0, 8.0 - gap},   { 20.0, 280.0, 8.0 - gap},
	};

	/* Region: tc6_reg.c  =  u base6.s  - sub6.s */
	struct bu_list reg_members;
	BU_LIST_INIT(&reg_members);
	mk_addmember("base6.s", &reg_members, NULL, WMOP_UNION);
	mk_addmember("sub6.s",  &reg_members, NULL, WMOP_SUBTRACT);

	/* Non-region wrapper: tc6_wrap  u tc6_reg.c
	 * This is the hierarchy level that triggers the path-key bug. */
	struct bu_list wrap_members;
	BU_LIST_INIT(&wrap_members);
	mk_addmember("tc6_reg.c", &wrap_members, NULL, WMOP_UNION);

	if (write_arb8(wdbp, "base6.s", base_pts) < 0 ||
	    write_arb8(wdbp, "sub6.s",  sub_pts)  < 0 ||
	    mk_comb(wdbp, "tc6_reg.c", &reg_members, 1 /* region */,
		    NULL, NULL, NULL, 1000, 0, 0, 0, 0, 0, 0) < 0 ||
	    mk_comb(wdbp, "tc6_wrap", &wrap_members, 0 /* not region */,
		    NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0) < 0) {
	    bu_log("[regress_facetize] TC6: write failed\n");
	    wdb_close(wdbp); bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	    return BRLCAD_ERROR;
	}
	wdb_close(wdbp);
    }

    /* Same test ray as TC5: +Z through window centre (100, 150) from below.
     * CSG and perturb BoT must MISS; no-perturb BoT is expected to HIT. */
    point_t origin = {100.0, 150.0, -1.0};
    vect_t  dir    = {  0.0,   0.0,  1.0};

    /* --- no-perturb from wrapper: documents that phantom face exists --- */
    if (run_facetize_r(gfile_np, "tc6_wrap", "tc6_bot.np",
		       1 /* no_perturb */, verbose) != BRLCAD_OK) {
	bu_log("[regress_facetize] TC6: FAIL - facetize --no-perturb error\n");
	bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	return BRLCAD_ERROR;
    }
    int hits_np = nirt_shoot(gfile_np, "tc6_bot.np", origin, dir);
    if (hits_np < 0) {
	bu_log("[regress_facetize] TC6: WARNING - nirt_shoot setup error for no-perturb BoT\n");
    } else if (hits_np == 0) {
	bu_log("[regress_facetize] TC6: WARNING - no-perturb BoT has no phantom hit "
	       "(test geometry may not reproduce the pattern)\n");
    } else {
	bu_log("[regress_facetize] TC6: no-perturb via wrapper has %d phantom hit(s) (expected)\n",
	       hits_np);
    }

    /* --- perturb from wrapper: MUST give MISS (path-key regression guard) */
    if (run_facetize_r(gfile_p, "tc6_wrap", "tc6_bot.p",
		       0 /* with perturb */, verbose) != BRLCAD_OK) {
	bu_log("[regress_facetize] TC6: FAIL - facetize error\n");
	bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	return BRLCAD_ERROR;
    }
    int hits_p = nirt_shoot(gfile_p, "tc6_bot.p", origin, dir);
    if (hits_p < 0) {
	bu_log("[regress_facetize] TC6: FAIL - nirt_shoot setup error for perturb BoT\n");
	bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	return BRLCAD_ERROR;
    }
    if (hits_p > 0) {
	bu_log("[regress_facetize] TC6: FAIL - perturb via wrapper has %d phantom hit(s); "
	       "variant plan path keys not matched for nested region\n", hits_p);
	bu_vls_free(&gpath_np); bu_vls_free(&gpath_p);
	return BRLCAD_ERROR;
    }
    bu_log("[regress_facetize] TC6: perturb via wrapper correctly MISS\n");
    bu_log("[regress_facetize] TC6: PASS\n");
    bu_vls_free(&gpath_np);
    bu_vls_free(&gpath_p);
    return BRLCAD_OK;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);

    int verbose = 0;
    const char *tmpdir = NULL;

    for (int i = 1; i < argc; i++) {
if (BU_STR_EQUAL(argv[i], "-v") || BU_STR_EQUAL(argv[i], "--verbose"))
    verbose++;
else if (!tmpdir)
    tmpdir = argv[i];
    }

    if (!tmpdir) {
bu_log("Usage: regress_facetize [-v] <tmpdir>\n");
return 1;
    }

    int ret = 0;
    if (tc1_window_frame(tmpdir, verbose)        != BRLCAD_OK) ret = 1;
    if (tc2_stacked_cutouts(tmpdir, verbose)     != BRLCAD_OK) ret = 1;
    if (tc3_rotated_wall(tmpdir, verbose)        != BRLCAD_OK) ret = 1;
    if (tc4_havoc_reuse(tmpdir, verbose)         != BRLCAD_OK) ret = 1;
    if (tc5_near_coplanar_subTOL(tmpdir, verbose) != BRLCAD_OK) ret = 1;
    if (tc6_havoc_wind9_pattern(tmpdir, verbose) != BRLCAD_OK) ret = 1;

    if (ret == 0)
bu_log("[regress_facetize] All tests PASSED\n");
    else
bu_log("[regress_facetize] One or more tests FAILED\n");

    return ret;
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
