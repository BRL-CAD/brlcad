/*                    F A C E P L A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2023 United States Government as represented by
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
/** @file faceplate.cpp
 *
 * Testing routines for drawing "built-in" faceplate view elements
 *
 */

#include "common.h"

#include <stdio.h>
#include <fstream>

#include <bu.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>

extern "C" void dm_refresh(struct ged *gedp);
extern "C" void scene_clear(struct ged *gedp);
extern "C" void img_cmp(int id, struct ged *gedp, const char *cdir, bool clear, int soft_fail, int approximate_check, const char *clear_root, const char *img_root);

int
main(int ac, char *av[]) {
    struct ged *dbp;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    int need_help = 0;
    int run_unstable_tests = 0;
    int soft_fail = 0;

    bu_setprogname(av[0]);

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",            "", NULL,          &need_help, "Print help and exit");
    BU_OPT(d[1], "U", "enable-unstable", "", NULL, &run_unstable_tests, "Test drawing routines known to differ between build configs/platforms.");
    BU_OPT(d[2], "c", "continue",        "", NULL,          &soft_fail, "Continue testing if a failure is encountered.");
    BU_OPT_NULL(d[3]);

    /* Done with program name */
    int uac = bu_opt_parse(NULL, ac, (const char **)av, d);
    if (uac == -1 || need_help)

    if (ac != 2)
	bu_exit(EXIT_FAILURE, "%s [-h] [-U] <directory>", av[0]);

    if (!bu_file_directory(av[1])) {
	printf("ERROR: [%s] is not a directory.  Expecting control image directory\n", av[1]);
	return 2;
    }

    /* Enable all the experimental logic */
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);
    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    bu_setenv("LIBGED_DBI_STATE", "1", 1);

    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    /* FIXME: To draw, we need to init this LIBRT global */
    BU_LIST_INIT(&RTG.rtg_vlfree);

    /* Open the temp file, then dbconcat argv[1] into it */
    bu_vls_sprintf(&fname, "%s/moss.g", av[1]);
    dbp = ged_open("db", bu_vls_cstr(&fname), 1);

    // Set up a basic view and set view name
    BU_ALLOC(dbp->ged_gvp, struct bview);
    bv_init(dbp->ged_gvp, &dbp->ged_views);
    bv_set_add_view(&dbp->ged_views, dbp->ged_gvp);
    bu_ptbl_ins(&dbp->ged_free_views, (long *)dbp->ged_gvp);
    bu_vls_sprintf(&dbp->ged_gvp->gv_name, "default");

    /* To generate images that will allow us to check if the drawing
     * is proceeding as expected, we use the swrast off-screen dm. */
    const char *s_av[15] = {NULL};
    s_av[0] = "dm";
    s_av[1] = "attach";
    s_av[2] = "swrast";
    s_av[3] = "SW";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    struct bview *v = dbp->ged_gvp;
    struct dm *dmp = (struct dm *)v->dmp;
    dm_set_width(dmp, 512);
    dm_set_height(dmp, 512);

    dm_configure_win(dmp, 0);
    dm_set_zbuffer(dmp, 1);

    // See QtSW.cpp...
    fastf_t windowbounds[6] = { -1, 1, -1, 1, -100, 100 };
    dm_set_win_bounds(dmp, windowbounds);

    dm_set_vp(dmp, &v->gv_scale);
    v->dmp = dmp;
    v->gv_width = dm_get_width(dmp);
    v->gv_height = dm_get_height(dmp);
    v->gv_base2local = dbp->dbip->dbi_base2local;
    v->gv_local2base = dbp->dbip->dbi_local2base;

    /***** Sanity - basic wireframe draw *****/
    bu_log("Testing basic db wireframe draw...\n");
    s_av[0] = "draw";
    s_av[1] = "all.g";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    s_av[0] = "ae";
    s_av[1] = "35";
    s_av[2] = "25";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);
    img_cmp(1, dbp, av[1], true, soft_fail, 0, "faceplate_clear", "fp");

    // Check that everything is in fact cleared
    img_cmp(0, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");
    bu_log("Done.\n");

    /***** Center Dot *****/
    bu_log("Testing center dot...\n");
    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "center_dot";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(2, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    // Check that turning off works
    s_av[3] = "0";
    ged_exec(dbp, 4, s_av);
    img_cmp(0, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");
    bu_log("Done.\n");

    /***** Grid *****/
    bu_log("Testing grid...\n");

    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "grid";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(3, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    // Check that turning off works
    s_av[3] = "0";
    ged_exec(dbp, 4, s_av);
    img_cmp(0, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");
    bu_log("Done.\n");

    /***** Params *****/
    bu_log("Testing parameters reporting...\n");

    // First make the font smaller so we can see all params
    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "params";
    s_av[3] = "font_size";
    s_av[4] = "10";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);

    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "params";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(4, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    bu_log("Testing turning on frames per second reporting...\n");

    // So we don't get random values here, override the timing variable values
    ((struct dm *)v->dmp)->start_time = 0;
    v->gv_s->gv_frametime = 1000000000;

    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "params";
    s_av[3] = "fps";
    s_av[4] = "0";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);
    img_cmp(5, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    // Check that turning off works
    s_av[3] = "0";
    ged_exec(dbp, 4, s_av);
    img_cmp(0, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");
    bu_log("Done.\n");

    // Restore default font size
    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "params";
    s_av[3] = "font_size";
    s_av[4] = "20";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);



    /***** Scale *****/
    bu_log("Testing scale reporting...\n");
    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "scale";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(6, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    // Check that turning off works
    s_av[3] = "0";
    ged_exec(dbp, 4, s_av);
    img_cmp(0, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");
    bu_log("Done.\n");


    /***** View axes *****/
    bu_log("Testing view axes drawing...\n");
    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "view_axes";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(7, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    // Check that turning off works
    s_av[3] = "0";
    ged_exec(dbp, 4, s_av);
    img_cmp(0, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");
    bu_log("Done.\n");

    /***** Model axes *****/
    bu_log("Testing model axes drawing...\n");
    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "model_axes";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(8, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    // Check that turning off works
    s_av[3] = "0";
    ged_exec(dbp, 4, s_av);
    img_cmp(0, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");
    bu_log("Done.\n");

    /***** Framebuffer *****/
    bu_log("Testing framebuffer...\n");
    struct bu_vls fb_img = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fb_img, "%s/moss.png", av[1]);
    s_av[0] = "png2fb";
    s_av[1] = bu_vls_cstr(&fb_img);
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "fb";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(9, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    // Check that turning off works
    s_av[3] = "0";
    ged_exec(dbp, 4, s_av);
    img_cmp(0, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    // Re-enable and make sure clear works
    s_av[3] = "1";
    ged_exec(dbp, 4, s_av);
    img_cmp(9, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    s_av[0] = "fbclear";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);
    img_cmp(0, dbp, av[1], false, soft_fail, 0, "faceplate_clear", "fp");

    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "fb";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    bu_log("Done.\n");


    ged_close(dbp);

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

