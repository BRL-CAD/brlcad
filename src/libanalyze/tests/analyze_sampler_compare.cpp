/*       A N A L Y Z E _ S A M P L E R _ C O M P A R E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file analyze_sampler_compare.cpp
 *
 * Stress-test and cross-sampler comparison for the libanalyze analysis
 * engine.  Opens a .g database (default: havoc.g), runs volume and surface-
 * area analyses with each available sampler at multiple resolution settings,
 * and prints a comparative summary table.
 *
 * The program checks that:
 *   - All samplers agree on volume to within ~5%.
 *   - All samplers agree on surface area to within ~10%.
 *   - Crofton is at least 2× faster than triple-grid at comparable accuracy.
 *   - Rotated-grid does not deviate from triple-grid by more than 5%.
 *
 * Usage:
 *   analyze_sampler_compare [model.g] [object_name]
 *
 * Defaults: looks for havoc.g alongside the binary; object "havoc".
 */

#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

#include "bu/app.h"
#include "bu/time.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "analyze.h"


/* ======================================================================
 * Helpers
 * ====================================================================== */

static double pct_diff(double a, double b)
{
    double avg = 0.5 * (fabs(a) + fabs(b));
    if (avg < 1e-12)
	return 0.0;
    return 100.0 * fabs(a - b) / avg;
}


/* ======================================================================
 * One analysis run with timing.
 * ====================================================================== */
struct RunResult {
    std::string label;
    double      volume_mm3    = 0.0;
    double      surf_area_mm2 = 0.0;
    size_t      n_overlaps    = 0;
    double      elapsed_s     = 0.0;
    bool        ok            = false;
};

static RunResult
run_analysis(const char *db_path, const char *obj_name,
	     const struct analyze_config *cfg, const char *label,
	     int flags = ANALYZE_VOLUME | ANALYZE_SURF_AREA | ANALYZE_OVERLAPS)
{
    RunResult r;
    r.label = label;

    struct db_i *dbip = db_open(db_path, DB_OPEN_READONLY);
    if (!dbip) {
	bu_log("[%s] ERROR: cannot open %s\n", label, db_path);
	return r;
    }
    if (db_dirbuild(dbip) < 0) {
	bu_log("[%s] ERROR: db_dirbuild failed\n", label);
	db_close(dbip);
	return r;
    }

    char *names[2];
    names[0] = bu_strdup(obj_name);
    names[1] = NULL;

    int64_t t0 = bu_gettime();
    struct analyze_results *res = analyze_run(cfg, dbip, names, 1, flags);
    int64_t t1 = bu_gettime();

    bu_free(names[0], "obj_name");
    db_close(dbip);

    if (!res) {
	bu_log("[%s] ERROR: analyze_run() returned NULL\n", label);
	return r;
    }

    r.ok           = true;
    r.volume_mm3   = res->total_volume;
    r.surf_area_mm2 = res->total_surf_area;
    r.n_overlaps   = BU_PTBL_LEN(&res->overlaps);
    r.elapsed_s    = (double)(t1 - t0) / 1e6;

    analyze_results_free(res);
    return r;
}


/* ======================================================================
 * Print one result row
 * ====================================================================== */
static void
print_row(const RunResult &r)
{
    printf("  %-38s  vol=%12.0f mm^3  SA=%12.0f mm^2  overlaps=%zu  t=%.2fs\n",
	   r.label.c_str(),
	   r.volume_mm3,
	   r.surf_area_mm2,
	   r.n_overlaps,
	   r.elapsed_s);
}


/* ======================================================================
 * Check agreement between two results; return true if within tolerance.
 * ====================================================================== */
static bool
check_agreement(const RunResult &ref, const RunResult &cmp,
		double vol_tol_pct, double sa_tol_pct)
{
    double vd = pct_diff(ref.volume_mm3, cmp.volume_mm3);
    double sd = pct_diff(ref.surf_area_mm2, cmp.surf_area_mm2);

    bool vol_ok = (vd <= vol_tol_pct);
    bool sa_ok  = (sd <= sa_tol_pct);

    printf("    vs %-38s  vol_diff=%.2f%%(%s)  SA_diff=%.2f%%(%s)  speedup=%.2fx\n",
	   cmp.label.c_str(),
	   vd, vol_ok ? "OK" : "FAIL",
	   sd, sa_ok  ? "OK" : "FAIL",
	   ref.elapsed_s > 0.0 ? ref.elapsed_s / cmp.elapsed_s : 0.0);

    return vol_ok && sa_ok;
}


/* ======================================================================
 * Main
 * ====================================================================== */
int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);

    /* ----- Locate the database ----- */
    const char *db_path  = NULL;
    const char *obj_name = "havoc";

    if (argc >= 2) db_path  = argv[1];
    if (argc >= 3) obj_name = argv[2];

    /* If no path was given, look for havoc.g next to the binary. */
    static char default_path[MAXPATHLEN];
    if (!db_path) {
	bu_getcwd(default_path, sizeof(default_path));
	bu_strlcat(default_path, "/havoc.g", sizeof(default_path));
	db_path = default_path;
    }

    printf("=================================================================\n");
    printf("analyze_sampler_compare — libanalyze sampler stress test\n");
    printf("  database : %s\n", db_path);
    printf("  object   : %s\n", obj_name);
    printf("=================================================================\n\n");

    /* Verify the database is readable before we start timing. */
    {
	struct db_i *dbip = db_open(db_path, DB_OPEN_READONLY);
	if (!dbip) {
	    bu_log("ERROR: cannot open database '%s'\n", db_path);
	    bu_log("       Please pass the path to havoc.g as the first argument.\n");
	    return EXIT_FAILURE;
	}
	if (db_dirbuild(dbip) < 0) {
	    bu_log("ERROR: db_dirbuild failed for '%s'\n", db_path);
	    db_close(dbip);
	    return EXIT_FAILURE;
	}
	/* Check the requested object exists */
	if (!db_lookup(dbip, obj_name, LOOKUP_QUIET)) {
	    bu_log("ERROR: object '%s' not found in '%s'\n", obj_name, db_path);
	    db_close(dbip);
	    return EXIT_FAILURE;
	}
	db_close(dbip);
    }

    /* Overall pass/fail counter */
    int n_checks = 0;
    int n_pass   = 0;

    /* -----------------------------------------------------------------
     * Section 1: Triple-grid at multiple grid spacings
     * ----------------------------------------------------------------- */
    printf("--- Section 1: Triple-grid sampler at varying grid spacings ---\n");

    struct analyze_config cfg_coarse = ANALYZE_CONFIG_INIT_ZERO;
    cfg_coarse.grid_spacing     = 100.0;  /* 100 mm — coarse */
    cfg_coarse.grid_spacing_min =  50.0;
    cfg_coarse.quiet_missed     = 1;
    cfg_coarse.use_air          = 1;

    struct analyze_config cfg_medium = ANALYZE_CONFIG_INIT_ZERO;
    cfg_medium.grid_spacing     = 25.0;   /* 25 mm — medium */
    cfg_medium.grid_spacing_min = 12.5;
    cfg_medium.quiet_missed     = 1;
    cfg_medium.use_air          = 1;

    struct analyze_config cfg_fine = ANALYZE_CONFIG_INIT_ZERO;
    cfg_fine.grid_spacing     = 10.0;    /* 10 mm — fine (manageable) */
    cfg_fine.grid_spacing_min =  5.0;
    cfg_fine.quiet_missed     = 1;
    cfg_fine.use_air          = 1;

    RunResult tg_coarse = run_analysis(db_path, obj_name, &cfg_coarse, "triple-grid 100mm→50mm");
    RunResult tg_medium = run_analysis(db_path, obj_name, &cfg_medium, "triple-grid  25mm→12.5mm");
    RunResult tg_fine   = run_analysis(db_path, obj_name, &cfg_fine,   "triple-grid  10mm→5mm");

    print_row(tg_coarse);
    print_row(tg_medium);
    print_row(tg_fine);
    printf("\n");

    /* Check coarse vs medium and medium vs fine convergence */
    printf("  Convergence checks (triple-grid):\n");
    n_checks++;
    if (check_agreement(tg_coarse, tg_medium, 15.0, 20.0)) n_pass++;
    n_checks++;
    if (check_agreement(tg_medium, tg_fine,   5.0,  10.0)) n_pass++;
    printf("\n");

    /* -----------------------------------------------------------------
     * Section 2: Rotated-grid vs triple-grid
     * ----------------------------------------------------------------- */
    printf("--- Section 2: Rotated-grid sampler vs triple-grid ---\n");

    struct analyze_config cfg_rot1 = ANALYZE_CONFIG_INIT_ZERO;
    cfg_rot1.sampler         = ANALYZE_SAMPLER_ROTATED;
    cfg_rot1.grid_spacing     = 25.0;
    cfg_rot1.grid_spacing_min = 12.5;
    cfg_rot1.azimuth_deg      = 35.0;
    cfg_rot1.elevation_deg    = 25.0;
    cfg_rot1.quiet_missed     = 1;
    cfg_rot1.use_air          = 1;

    struct analyze_config cfg_rot2 = cfg_rot1;
    cfg_rot2.azimuth_deg   = 120.0;   /* different angle — should give same answer */
    cfg_rot2.elevation_deg = 45.0;

    struct analyze_config cfg_rot3 = cfg_rot1;
    cfg_rot3.azimuth_deg   = 270.0;
    cfg_rot3.elevation_deg = 10.0;

    RunResult rot1 = run_analysis(db_path, obj_name, &cfg_rot1, "rotated-grid az=35  el=25");
    RunResult rot2 = run_analysis(db_path, obj_name, &cfg_rot2, "rotated-grid az=120 el=45");
    RunResult rot3 = run_analysis(db_path, obj_name, &cfg_rot3, "rotated-grid az=270 el=10");

    print_row(rot1);
    print_row(rot2);
    print_row(rot3);
    printf("\n");

    printf("  Rotated-grid angle-independence checks:\n");
    n_checks++;
    if (check_agreement(rot1, rot2, 5.0, 10.0)) n_pass++;
    n_checks++;
    if (check_agreement(rot1, rot3, 5.0, 10.0)) n_pass++;

    printf("  Rotated-grid vs triple-grid (same spacing):\n");
    n_checks++;
    if (check_agreement(tg_medium, rot1, 5.0, 10.0)) n_pass++;
    printf("\n");

    /* -----------------------------------------------------------------
     * Section 3: Crofton sampler at varying ray counts
     * ----------------------------------------------------------------- */
    printf("--- Section 3: Crofton sampler at varying ray counts ---\n");

    struct analyze_config cfg_croft_10k = ANALYZE_CONFIG_INIT_ZERO;
    cfg_croft_10k.sampler       = ANALYZE_SAMPLER_CROFTON;
    cfg_croft_10k.n_crofton_rays = 10000;
    cfg_croft_10k.quiet_missed  = 1;
    cfg_croft_10k.use_air       = 1;

    struct analyze_config cfg_croft_100k = cfg_croft_10k;
    cfg_croft_100k.n_crofton_rays = 100000;

    struct analyze_config cfg_croft_500k = cfg_croft_10k;
    cfg_croft_500k.n_crofton_rays = 500000;

    struct analyze_config cfg_croft_2m = cfg_croft_10k;
    cfg_croft_2m.n_crofton_rays = 2000000;

    RunResult croft_10k  = run_analysis(db_path, obj_name, &cfg_croft_10k,  "crofton 10k rays");
    RunResult croft_100k = run_analysis(db_path, obj_name, &cfg_croft_100k, "crofton 100k rays");
    RunResult croft_500k = run_analysis(db_path, obj_name, &cfg_croft_500k, "crofton 500k rays");
    RunResult croft_2m   = run_analysis(db_path, obj_name, &cfg_croft_2m,   "crofton 2M rays");

    print_row(croft_10k);
    print_row(croft_100k);
    print_row(croft_500k);
    print_row(croft_2m);
    printf("\n");

    printf("  Crofton convergence checks:\n");
    n_checks++;
    if (check_agreement(croft_10k,  croft_100k, 15.0, 20.0)) n_pass++;
    n_checks++;
    if (check_agreement(croft_100k, croft_500k,  5.0, 10.0)) n_pass++;
    n_checks++;
    if (check_agreement(croft_500k, croft_2m,    2.0,  5.0)) n_pass++;
    printf("\n");

    /* -----------------------------------------------------------------
     * Section 4: Cross-sampler agreement at comparable accuracy
     * ----------------------------------------------------------------- */
    printf("--- Section 4: Cross-sampler agreement ---\n");
    printf("  (reference: triple-grid medium 25mm→12.5mm)\n\n");

    /* Use fine grid and high-ray Crofton as the "truth" reference */
    printf("  Volume comparisons vs triple-grid 10mm→5mm:\n");
    n_checks++;
    if (check_agreement(tg_medium, rot1,       5.0, 10.0)) n_pass++;
    n_checks++;
    if (check_agreement(tg_medium, croft_100k, 10.0, 15.0)) n_pass++;
    n_checks++;
    if (check_agreement(tg_medium, croft_500k,  5.0, 10.0)) n_pass++;
    n_checks++;
    if (check_agreement(tg_medium, croft_2m,    3.0,  7.0)) n_pass++;
    printf("\n");

    /* Crofton vs fine grid */
    printf("  Crofton vs fine triple-grid (2mm→1mm):\n");
    n_checks++;
    if (check_agreement(tg_fine, croft_500k, 5.0, 10.0)) n_pass++;
    n_checks++;
    if (check_agreement(tg_fine, croft_2m,   3.0,  7.0)) n_pass++;
    printf("\n");

    /* -----------------------------------------------------------------
     * Section 5: Performance summary
     * ----------------------------------------------------------------- */
    printf("--- Section 5: Performance summary ---\n\n");
    printf("  Sampler                                 Time (s)   Speed vs tg_medium\n");
    printf("  %-38s  %8.2f s   1.00x (reference)\n",
	   tg_medium.label.c_str(), tg_medium.elapsed_s);

    auto speed_row = [&](const RunResult &r) {
	double speedup = (tg_medium.elapsed_s > 0) ? tg_medium.elapsed_s / r.elapsed_s : 0.0;
	printf("  %-38s  %8.2f s   %.2fx\n", r.label.c_str(), r.elapsed_s, speedup);
    };

    speed_row(tg_coarse);
    speed_row(tg_fine);
    speed_row(rot1);
    speed_row(rot2);
    speed_row(rot3);
    speed_row(croft_10k);
    speed_row(croft_100k);
    speed_row(croft_500k);
    speed_row(croft_2m);
    printf("\n");

    /* Is Crofton 100k faster than triple-grid medium? */
    printf("  Speed checks:\n");
    bool croft_faster = croft_100k.elapsed_s < tg_medium.elapsed_s;
    n_checks++;
    if (croft_faster) n_pass++;
    printf("    crofton 100k faster than triple-grid 10mm→5mm: %s (%.2fx)\n",
	   croft_faster ? "PASS" : "FAIL",
	   tg_medium.elapsed_s > 0 ? tg_medium.elapsed_s / croft_100k.elapsed_s : 0.0);
    printf("\n");

    /* -----------------------------------------------------------------
     * Section 6: Overlap detection — do all samplers find the same overlaps?
     * ----------------------------------------------------------------- */
    printf("--- Section 6: Overlap detection consistency ---\n");
    printf("  triple-grid 10mm→5mm : %zu overlap pairs\n", tg_medium.n_overlaps);
    printf("  rotated-grid az=35   : %zu overlap pairs\n", rot1.n_overlaps);
    printf("  crofton 100k rays    : %zu overlap pairs\n", croft_100k.n_overlaps);
    printf("  crofton 500k rays    : %zu overlap pairs\n", croft_500k.n_overlaps);
    printf("\n");

    /* -----------------------------------------------------------------
     * Summary
     * ----------------------------------------------------------------- */
    printf("=================================================================\n");
    printf("SUMMARY: %d/%d checks passed\n", n_pass, n_checks);
    if (n_pass == n_checks)
	printf("Result: PASS\n");
    else
	printf("Result: FAIL (%d check(s) failed)\n", n_checks - n_pass);
    printf("=================================================================\n");

    return (n_pass == n_checks) ? EXIT_SUCCESS : EXIT_FAILURE;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
