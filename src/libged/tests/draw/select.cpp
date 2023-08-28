/*                       S E L E C T . C P P
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
/** @file select.cpp
 *
 * Testing routines for selection logic, with a focus on verifying
 * drawing response
 *
 */

#include "common.h"

#include <stdio.h>
#include <fstream>

#include <bu.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>

extern "C" void ged_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data);
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

    /* We are going to generate geometry from the basic moss data,
     * so we make a temporary copy */
    bu_vls_sprintf(&fname, "%s/moss.g", av[1]);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmpg("moss_select_tmp.g", std::ios::binary);
    tmpg << orig.rdbuf();
    orig.close();
    tmpg.close();

    /* Open the temp file */
    const char *s_av[15] = {NULL};
    dbp = ged_open("db", "moss_select_tmp.g", 1);

    // Set callback so database changes will update dbi_state
    db_add_changed_clbk(dbp->dbip, &ged_changed_callback, (void *)dbp);

    // Set up a basic view and set view name
    BU_ALLOC(dbp->ged_gvp, struct bview);
    bv_init(dbp->ged_gvp, &dbp->ged_views);
    bv_set_add_view(&dbp->ged_views, dbp->ged_gvp);
    bu_ptbl_ins(&dbp->ged_free_views, (long *)dbp->ged_gvp);
    bu_vls_sprintf(&dbp->ged_gvp->gv_name, "default");

    /* To generate images that will allow us to check if the drawing
     * is proceeding as expected, we use the swrast off-screen dm. */
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

    s_av[0] = "ae";
    s_av[1] = "35";
    s_av[2] = "25";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    /***** Basic CSG wireframe ****/
    bu_log("Sanity - basic wireframe, no selection...\n");

    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "csg";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(1, dbp, av[1], false, soft_fail, 0, "select_clear", "select");
    bu_log("Done.\n");

    bu_log("Selecting a single object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g/tor.r/tor";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(2, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("De-selected object...\n");
    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.g/tor.r/tor";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(1, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Select higher level object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g/tor.r";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(2, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Select object below selected object (should be no-op)...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g/tor.r/tor";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(2, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Select second object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g/box.r";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(3, dbp, av[1], false, soft_fail, 0, "select_clear", "select");

    bu_log("Select top level object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(4, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Expand selection list to solid objects...\n");
    s_av[0] = "select";
    s_av[1] = "expand";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(4, dbp, av[1], false, soft_fail, 0, "select_clear", "select");

    bu_log("De-select one object...\n");
    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.g/tor.r/tor";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(5, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Collapse selected paths...\n");
    s_av[0] = "select";
    s_av[1] = "collapse";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(5, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Check correct highlighting after Z, selection change and redraw...\n");

    s_av[0] = "Z";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.g/box.r";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(6, dbp, av[1], false, soft_fail, 0, "select_clear", "select");

    bu_log("Check correct highlighting after clear...\n");

    s_av[0] = "select";
    s_av[1] = "clear";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(1, dbp, av[1], true, soft_fail, 0, "select_clear", "select");

    /***** Basic Mesh wireframe, no LoD ****/

    /* Generate mesh geometry */
    s_av[0] = "tol";
    s_av[1] = "rel";
    s_av[2] = "0.0001";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "facetize";
    s_av[1] = "-r";
    s_av[2] = "all.g";
    s_av[3] = "all.bot";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    dbp->dbi_state->update();


    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m1";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(7, dbp, av[1], false, soft_fail, 0, "select_clear", "select");
    bu_log("Done.\n");

    bu_log("Selecting a single object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot/all.g-1/tor.r-1/tor.r.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(8, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("De-selected object...\n");
    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.bot/all.g-1/tor.r-1/tor.r.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(7, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Select higher level object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot/all.g-1/tor.r-1";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(8, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Select object below selected object (should be no-op)...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot/all.g-1/tor.r-1/tor.r.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(8, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Select second object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot/all.g-1/box.r-1";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(9, dbp, av[1], false, soft_fail, 0, "select_clear", "select");

    bu_log("Select top level object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(10, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Expand selection list to solid objects...\n");
    s_av[0] = "select";
    s_av[1] = "expand";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(10, dbp, av[1], false, soft_fail, 0, "select_clear", "select");

    bu_log("De-select one object...\n");
    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.bot/all.g-1/tor.r-1/tor.r.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(11, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Collapse selected paths...\n");
    s_av[0] = "select";
    s_av[1] = "collapse";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(11, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Check correct highlighting after Z, selection change and redraw...\n");

    s_av[0] = "Z";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.bot/all.g-1/box.r-1";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m1";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(12, dbp, av[1], false, soft_fail, 0, "select_clear", "select");

    bu_log("Check correct highlighting after clear...\n");

    s_av[0] = "select";
    s_av[1] = "clear";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(7, dbp, av[1], true, soft_fail, 0, "select_clear", "select");


    /***** LoD CSG wireframe ****/
    s_av[0] = "tol";
    s_av[1] = "rel";
    s_av[2] = "0.01";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    bu_log("Sanity - basic LoD wireframe, no selection...\n");

    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "csg";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(13, dbp, av[1], false, soft_fail, 0, "select_clear", "select");
    bu_log("Done.\n");

    bu_log("Selecting a single object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g/tor.r/tor";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(14, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("De-selected object...\n");
    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.g/tor.r/tor";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(13, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Select higher level object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g/tor.r";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(14, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Select object below selected object (should be no-op)...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g/tor.r/tor";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(14, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Select second object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g/box.r";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(15, dbp, av[1], false, soft_fail, 0, "select_clear", "select");

    bu_log("Select top level object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(16, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Expand selection list to solid objects...\n");
    s_av[0] = "select";
    s_av[1] = "expand";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(16, dbp, av[1], false, soft_fail, 0, "select_clear", "select");

    bu_log("De-select one object...\n");
    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.g/tor.r/tor";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(17, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Collapse selected paths...\n");
    s_av[0] = "select";
    s_av[1] = "collapse";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(17, dbp, av[1], false, soft_fail, 0, "select_clear", "select");


    bu_log("Check correct highlighting after Z, selection change and redraw...\n");

    s_av[0] = "Z";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.g/box.r";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(18, dbp, av[1], false, soft_fail, 0, "select_clear", "select");

    bu_log("Check correct highlighting after clear...\n");

    s_av[0] = "select";
    s_av[1] = "clear";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(13, dbp, av[1], true, soft_fail, 0, "select_clear", "select");

    /***** LoD Mesh wireframe ****/

    // Temporary - remove when above CSG tests can be enabled
    s_av[0] = "Z";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "scale";
    s_av[3] = "0.8";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m1";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(19, dbp, av[1], false, soft_fail, 10, "select_clear", "select");
    bu_log("Done.\n");

    bu_log("Selecting a single object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot/all.g-1/tor.r-1/tor.r.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(20, dbp, av[1], false, soft_fail, 10, "select_clear", "select");


    bu_log("De-selected object...\n");
    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.bot/all.g-1/tor.r-1/tor.r.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(19, dbp, av[1], false, soft_fail, 10, "select_clear", "select");


    bu_log("Select higher level object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot/all.g-1/tor.r-1";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(20, dbp, av[1], false, soft_fail, 10, "select_clear", "select");


    bu_log("Select object below selected object (should be no-op)...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot/all.g-1/tor.r-1/tor.r.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(20, dbp, av[1], false, soft_fail, 10, "select_clear", "select");


    bu_log("Select second object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot/all.g-1/box.r-1";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(21, dbp, av[1], false, soft_fail, 10, "select_clear", "select");

    bu_log("Select top level object...\n");
    s_av[0] = "select";
    s_av[1] = "add";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(22, dbp, av[1], false, soft_fail, 10, "select_clear", "select");


    bu_log("Expand selection list to solid objects...\n");
    s_av[0] = "select";
    s_av[1] = "expand";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(22, dbp, av[1], false, soft_fail, 10, "select_clear", "select");

    bu_log("De-select one object...\n");
    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.bot/all.g-1/tor.r-1/tor.r.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(23, dbp, av[1], false, soft_fail, 10, "select_clear", "select");


    bu_log("Collapse selected paths...\n");
    s_av[0] = "select";
    s_av[1] = "collapse";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(23, dbp, av[1], false, soft_fail, 10, "select_clear", "select");


    bu_log("Check correct highlighting after Z, selection change and redraw...\n");

    s_av[0] = "Z";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    s_av[0] = "select";
    s_av[1] = "rm";
    s_av[2] = "all.bot/all.g-1/box.r-1";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m1";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(24, dbp, av[1], false, soft_fail, 10, "select_clear", "select");

    bu_log("Check correct highlighting after clear...\n");

    s_av[0] = "select";
    s_av[1] = "clear";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    img_cmp(19, dbp, av[1], true, soft_fail, 10, "select_clear", "select");


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

