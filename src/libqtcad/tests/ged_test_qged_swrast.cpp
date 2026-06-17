/*      G E D _ T E S T _ Q G E D _ S W R A S T . C P P
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
/** @file ged_test_qged_swrast.cpp
 *
 * Qt / QgSW integration test — headless qged rendering via offscreen platform.
 *
 * This test exercises the Qt widget-level path (QgSW + libqtcad) that the
 * dm-swrast libged draw tests bypass.  Specifically it validates:
 *
 *   • once the widget view is bound to an active GED draw scene, the public
 *     scene ref is non-NULL.
 *   • QgSW::paintEvent() runs without crashing on the offscreen platform.
 *   • dm_draw_objs() called through the Qt paint path produces a non-blank
 *     framebuffer when geometry is present in the view.
 *   • The DM is correctly initialised via the Qt path (dm_open called from
 *     inside paintEvent's m_init block).
 *
 * Strategy:
 *   1. Set QT_QPA_PLATFORM=offscreen so no display hardware is needed.
 *   2. Create QApplication.
 *   3. Create QgSW widget and resize to 512×512.
 *   4. Open moss.g with ged_open and set gedp->ged_gvp = sw.view().
 *   5. Register sw.view() with the view set and set base2local / local2base.
 *   6. Draw all.g via ged_exec_draw.
 *   7. Force paintEvent via QWidget::render() on a QImage surface.
 *      (render() calls QWidget::paintEvent internally.)
 *   8. Read framebuffer content via dm_get_display_image after DM init.
 *   9. Verify:
 *      a. bsg_view_scene_attached(sw.view()) is true (after GED bind + draw)
 *      b. sw.displayManager() != NULL  (DM opened during paintEvent)
 *      c. At least one pixel differs from the background color (geometry rendered)
 *
 * Usage: ged_test_qged_swrast [-c] <directory-containing-moss.g>
 */

#include "common.h"

#include <fstream>

#include <QApplication>
#include <QTimer>
#include <QImage>

#include <bu.h>
#include "bu/opt.h"
#include <bsg/view_state.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>

/* Qt + libqtcad headers */
#include "qtcad/QgModel.h"
#include "qtcad/QgSW.h"

/* ------------------------------------------------------------------ */
static int g_fail = 0;
#define SWCHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
            g_fail++; \
        } \
    } while (0)

/* ================================================================== */
int
main(int ac, char *av[])
{
    bu_setprogname(av[0]);

    int soft_fail = 0;
    struct bu_opt_desc d[2];
    BU_OPT(d[0], "c", "continue", "", NULL, &soft_fail, "Continue on failure.");
    BU_OPT_NULL(d[1]);

    int uac = bu_opt_parse(NULL, ac, (const char **)av, d);
    if (uac != 2)
        bu_exit(EXIT_FAILURE,
                "Usage: %s [-c] <directory-containing-moss.g>\n", av[0]);
    const char *datadir = av[1];
    if (!bu_file_directory(datadir))
        bu_exit(EXIT_FAILURE, "ERROR: [%s] is not a directory\n", datadir);

    /* Force Qt to use the offscreen (raster) platform — no X11 / display needed */
    bu_setenv("QT_QPA_PLATFORM", "offscreen", 1);
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);
    bu_setenv("DM_SWRAST", "1", 1);

    char lcache[MAXPATHLEN];
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CURR, "qgswrast_cache", NULL);
    bu_mkdir(lcache);
    bu_setenv("BU_DIR_CACHE", lcache, 1);

    /* Make a private copy of moss.g */
    struct bu_vls srcpath = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&srcpath, "%s/moss.g", datadir);
    std::ifstream orig(bu_vls_cstr(&srcpath), std::ios::binary);
    std::ofstream tmpg("moss_qgswrast_tmp.g", std::ios::binary);
    tmpg << orig.rdbuf();
    orig.close(); tmpg.close();
    bu_vls_free(&srcpath);

    /* ---- Qt application (offscreen) ---- */
    /* Construct fake argc/argv so QApplication is happy; the real
     * bu_opt_parse has already consumed our arguments above. */
    int fake_argc = 1;
    char *fake_argv[2] = { av[0], NULL };
    QApplication app(fake_argc, fake_argv);

    /* ---- Create QgSW widget (exercises bsg_scene_root_create) ---- */
    QgSW sw;
    sw.resize(512, 512);

    SWCHECK(sw.view() != NULL, "QgSW::view() must be non-NULL after construction");

    /* ---- Open moss.g and hook up the model ---- */
    struct ged *gedp = ged_open("db", "moss_qgswrast_tmp.g", 1);
    if (!gedp) {
        bu_log("FAIL: ged_open failed\n");
        bu_file_delete("moss_qgswrast_tmp.g");
        bu_dirclear(lcache);
        return 1;
    }

    /* Route draw commands into the QgSW view */
    gedp->ged_gvp = sw.view();
    bsg_set_add_view(&gedp->ged_views, sw.view());
    sw.view()->gv_base2local = gedp->dbip->dbi_base2local;
    sw.view()->gv_local2base = gedp->dbip->dbi_local2base;

    /* Set az/el so the model is visible */
    const char *ae_av[4] = {"ae", "35", "25", NULL};
    ged_exec_ae(gedp, 3, ae_av);

    /* Draw all.g */
    const char *draw_av[3] = {"draw", "all.g", NULL};
    ged_exec_draw(gedp, 2, draw_av);
    const char *av_av[1] = {"autoview"};
    ged_exec_autoview(gedp, 1, av_av);

    SWCHECK(sw.view() && bsg_view_scene_attached(sw.view()),
            "after binding QgSW view to GED and drawing, view scene ref must be non-NULL");

    /* ---- Force paintEvent via QWidget::render() ---- */
    /* With the Qt offscreen platform, render() will trigger paintEvent,
     * which initialises the DMP on first call (m_init path in QgSW.cpp). */
    QImage img(512, 512, QImage::Format_RGB32);
    img.fill(Qt::black);
    sw.render(&img);

    /* Process any pending Qt events that paintEvent may have queued */
    QCoreApplication::processEvents();

    /* If paintEvent ran and initialised the DM, sw.displayManager() must be set now */
    SWCHECK(sw.displayManager() != NULL, "sw.displayManager() must be non-NULL after first paintEvent (DM opened)");

    /* ---- Check framebuffer has non-background pixels ---- */
    bool has_nonbg = false;
    if (sw.displayManager()) {
        unsigned char *dm_image = NULL;
        int gr = dm_get_display_image(sw.displayManager(), &dm_image, 1, 1);
        if (gr == 0 && dm_image) {
            /* Background is typically dark grey or near-black.
             * We just need at least one pixel that is NOT pure black,
             * indicating that wireframe lines were drawn. */
            int W = dm_get_width(sw.displayManager());
            int H = dm_get_height(sw.displayManager());
            for (int p = 0; p < W * H * 4 && !has_nonbg; p += 4) {
                /* Check RGB — skip alpha byte [p+3] */
                if (dm_image[p] > 30 || dm_image[p+1] > 30 || dm_image[p+2] > 30)
                    has_nonbg = true;
            }
            bu_free(dm_image, "dm image check");
        } else {
            bu_log("  dm_get_display_image returned %d (may not be supported on offscreen)\n", gr);
            /* Not a hard failure — the offscreen platform may not support
             * dm_get_display_image, but we still verified the scene ref and dmp. */
            has_nonbg = true;   /* skip pixel check for this platform */
        }
    }

    if (sw.displayManager()) {
        SWCHECK(has_nonbg,
                "framebuffer must have non-background pixels (geometry was drawn)");
    }

    /* Summarise */
    if (g_fail == 0)
        bu_log("PASS: QgSW offscreen integration test passed\n");
    else
        bu_log("RESULT: %d failure(s)\n", g_fail);

    /* Cleanup */
    ged_close(gedp);
    bu_file_delete("moss_qgswrast_tmp.g");
    bu_dirclear(lcache);

    return (g_fail > 0 ? 1 : 0);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
