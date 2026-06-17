/*        G E D _ T E S T _ D M _ B A C K E N D _ B E N C H . C P P
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
/** @file ged_test_dm_backend_bench.cpp
 *
 * Phase B benchmark: measure dm_draw_objs throughput for dm-swrast and
 * dm-qtgl backends so that an evidence-based default-backend decision can
 * be made.
 *
 * Design
 * ------
 * Both backends are tested through their respective Qt widget paths so that
 * the measured time includes the same per-frame overhead that production qged
 * experiences:
 *
 *   dm-swrast path: QgSW::paintEvent  → dm_draw_objs (Mesa OSMesa CPU path)
 *   dm-qtgl  path:  QgGL::paintGL     → dm_draw_objs (hardware OpenGL path)
 *
 * After each widget's DM is initialised by the first paint call, the timing
 * loop calls dm_draw_objs + dm_draw_end directly on the widget's dmp pointer,
 * bypassing the Qt repaint event overhead so we measure only the DM cost.
 *
 * The swrast section always runs (no display hardware required; uses Qt's
 * offscreen platform).  The qtgl section requires a working OpenGL context;
 * on headless / CI machines it is skipped gracefully with a diagnostic note.
 *
 * Exit status: always 0 — this is a benchmark, not a pass/fail test.
 *
 * Usage: ged_test_dm_backend_bench [-n <iters>] <dir-containing-moss.g>
 */

#include "common.h"

#include <fstream>

#include <QApplication>
#include <QImage>
#include <QTimer>

#include <bu.h>
#include "bu/opt.h"
#include "bu/time.h"
#include <bsg/view_state.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>

#include "qtcad/QgSW.h"
#ifdef BRLCAD_OPENGL
#  include "qtcad/QgGL.h"
#endif

/* ------------------------------------------------------------------ */
/* Print a timing result in a human-readable, script-parseable form   */
static void
print_bench_result(const char *backend, int n, int64_t elapsed_us)
{
    double total_ms  = elapsed_us / 1000.0;
    double avg_ms    = total_ms / n;
    double fps       = (n * 1.0e6) / elapsed_us;
    bu_log("BENCH %-8s  iters=%d  total=%.1f ms  avg=%.2f ms/frame  fps=%.1f\n",
	   backend, n, total_ms, avg_ms, fps);
}

/* ------------------------------------------------------------------ */
/* Set up GED for drawing: open file, bind the widget view, draw all.g */
static struct ged *
open_and_draw(const char *gfile, struct bsg_view *view)
{
    struct ged *gedp = ged_open("db", gfile, 1);
    if (!gedp)
	return NULL;
    if (view) {
	gedp->ged_gvp = view;
	bsg_set_add_view(&gedp->ged_views, view);
	view->gv_base2local = gedp->dbip->dbi_base2local;
	view->gv_local2base = gedp->dbip->dbi_local2base;
    }

    const char *ae_av[4] = {"ae", "35", "25", NULL};
    ged_exec_ae(gedp, 3, ae_av);

    const char *draw_av[3] = {"draw", "all.g", NULL};
    ged_exec_draw(gedp, 2, draw_av);
    const char *av_av[2] = {"autoview", NULL};
    ged_exec_autoview(gedp, 1, av_av);

    return gedp;
}

/* ------------------------------------------------------------------ */
/* Clean up GED state                                                  */
static void
close_ged(struct ged *gedp)
{
    if (!gedp)
	return;
    ged_close(gedp);
}

/* ================================================================== */
int
main(int ac, char *av[])
{
    bu_setprogname(av[0]);

    int n_iters = 30;
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "n", "iters", "#", bu_opt_int, &n_iters,
	   "Number of draw iterations per backend (default 30)");
    BU_OPT_NULL(d[1]);

    int uac = bu_opt_parse(NULL, ac, (const char **)av, d);
    if (uac != 2) {
	bu_exit(EXIT_FAILURE,
		"Usage: %s [-n <iters>] <dir-containing-moss.g>\n", av[0]);
    }
    const char *datadir = av[1];
    if (!bu_file_directory(datadir))
	bu_exit(EXIT_FAILURE, "ERROR: [%s] is not a directory\n", datadir);
    if (n_iters < 1)
	n_iters = 1;

    /* Qt requires the offscreen platform so no display is needed. */
    bu_setenv("QT_QPA_PLATFORM", "offscreen", 1);
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);
    bu_setenv("DM_SWRAST", "1", 1);

    char lcache[MAXPATHLEN];
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CURR, "dmbench_cache", NULL);
    bu_mkdir(lcache);
    bu_setenv("BU_DIR_CACHE", lcache, 1);

    /* ---- Create single QApplication (required for any Qt widget) -- */
    int fake_argc = 1;
    char *fake_argv[2] = { av[0], NULL };
    QApplication app(fake_argc, fake_argv);

    /* ================================================================
     * Part 1: dm-swrast via QgSW (always runs)
     * ================================================================ */
    bu_log("\n--- dm-swrast benchmark (%d iterations) ---\n", n_iters);

    /* Private working copy of moss.g */
    struct bu_vls srcpath = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&srcpath, "%s/moss.g", datadir);
    std::ifstream orig_sw(bu_vls_cstr(&srcpath), std::ios::binary);
    std::ofstream tmpg_sw("dmbench_swrast_tmp.g", std::ios::binary);
    tmpg_sw << orig_sw.rdbuf();
    orig_sw.close(); tmpg_sw.close();

    QgSW sw;
    sw.resize(512, 512);
    struct ged *gedp_sw = open_and_draw("dmbench_swrast_tmp.g", sw.view());
    if (!gedp_sw) {
	bu_log("SKIP swrast: ged_open failed\n");
    } else {
	/* Force first paintEvent → DM init */
	QImage img_sw(512, 512, QImage::Format_RGB32);
	img_sw.fill(Qt::black);
	sw.render(&img_sw);
	QCoreApplication::processEvents();

	if (!sw.displayManager()) {
	    bu_log("SKIP swrast: DM did not initialise after first paint\n");
	} else {
	    /* Warm-up: one extra draw before timing */
	    {
		unsigned char *bg1, *bg2;
		dm_get_bg(&bg1, &bg2, sw.displayManager());
		dm_set_bg(sw.displayManager(), bg1[0], bg1[1], bg1[2], bg2[0], bg2[1], bg2[2]);
		dm_draw_objs(sw.view());
		dm_draw_end(sw.displayManager());
	    }

	    /* Timed loop */
	    int64_t t0 = bu_gettime();
	    for (int i = 0; i < n_iters; i++) {
		unsigned char *bg1, *bg2;
		dm_get_bg(&bg1, &bg2, sw.displayManager());
		dm_set_bg(sw.displayManager(), bg1[0], bg1[1], bg1[2], bg2[0], bg2[1], bg2[2]);
		dm_draw_objs(sw.view());
		dm_draw_end(sw.displayManager());
	    }
	    int64_t elapsed_sw = bu_gettime() - t0;
	    print_bench_result("swrast", n_iters, elapsed_sw);
	}

	close_ged(gedp_sw);
    }
    bu_file_delete("dmbench_swrast_tmp.g");

    /* ================================================================
     * Part 2: dm-qtgl via QgGL (requires hardware OpenGL; skipped if
     * the QOpenGLWidget context cannot be created on this platform)
     * ================================================================ */
#ifdef BRLCAD_OPENGL
    bu_log("\n--- dm-qtgl benchmark (%d iterations) ---\n", n_iters);

    std::ifstream orig_gl(bu_vls_cstr(&srcpath), std::ios::binary);
    std::ofstream tmpg_gl("dmbench_qtgl_tmp.g", std::ios::binary);
    tmpg_gl << orig_gl.rdbuf();
    orig_gl.close(); tmpg_gl.close();

    QgGL gl;
    gl.resize(512, 512);
    struct ged *gedp_gl = open_and_draw("dmbench_qtgl_tmp.g", gl.view());
    if (!gedp_gl) {
	bu_log("SKIP qtgl: ged_open failed\n");
    } else {
	/* QgGL uses paintGL triggered by show() + processEvents().
	 * On the offscreen platform a QOpenGLWidget context may not be
	 * available — we detect that by gl.displayManager() remaining NULL. */
	gl.show();
	QCoreApplication::processEvents();
	/* Give Qt a second attempt in case init is asynchronous */
	QCoreApplication::processEvents();

	if (!gl.displayManager()) {
	    bu_log("SKIP qtgl: OpenGL context not available on this platform "
		   "(expected in headless / CI environments — run on hardware "
		   "with a GPU to collect qtgl timing data)\n");
	} else {
	    /* Warm-up */
	    {
		unsigned char *bg1, *bg2;
		dm_get_bg(&bg1, &bg2, gl.displayManager());
		dm_set_bg(gl.displayManager(), bg1[0], bg1[1], bg1[2], bg2[0], bg2[1], bg2[2]);
		dm_draw_objs(gl.view());
		dm_draw_end(gl.displayManager());
	    }

	    int64_t t0 = bu_gettime();
	    for (int i = 0; i < n_iters; i++) {
		unsigned char *bg1, *bg2;
		dm_get_bg(&bg1, &bg2, gl.displayManager());
		dm_set_bg(gl.displayManager(), bg1[0], bg1[1], bg1[2], bg2[0], bg2[1], bg2[2]);
		dm_draw_objs(gl.view());
		dm_draw_end(gl.displayManager());
	    }
	    int64_t elapsed_gl = bu_gettime() - t0;
	    print_bench_result("qtgl", n_iters, elapsed_gl);
	}

	close_ged(gedp_gl);
    }
    bu_file_delete("dmbench_qtgl_tmp.g");
#else
    bu_log("\n--- dm-qtgl benchmark ---\n");
    bu_log("SKIP qtgl: BRL-CAD built without OpenGL support\n");
#endif /* BRLCAD_OPENGL */

    bu_vls_free(&srcpath);
    bu_dirclear(lcache);

    bu_log("\nBenchmark complete.\n");
    /* Always exit 0 — this is a timing benchmark, not a pass/fail test. */
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
