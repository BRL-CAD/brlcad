/*                         L O D . C P P
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
/** @file lod.cpp
 *
 * Testing routines for Level of Detail (LoD) logic
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
    std::ofstream tmpg("moss_lod_tmp.g", std::ios::binary);
    tmpg << orig.rdbuf();
    orig.close();
    tmpg.close();

    /* Open the temp file */
    const char *s_av[15] = {NULL};
    dbp = ged_open("db", "moss_lod_tmp.g", 1);

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

    /* Fully clear any prior cached LoD data */
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "cache";
    s_av[3] = "clear";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    /* Generate geometry appropriate for mesh LoD */
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

    s_av[0] = "ae";
    s_av[1] = "35";
    s_av[2] = "25";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);


    /***** Mesh LoD ****/
    bu_log("Sanity - testing shaded mode 1 (triangle only) drawing, Level-of-Detail disabled...\n");
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

    img_cmp(1, dbp, av[1], false, soft_fail, 0, "lod_clear", "lod");
    bu_log("Done.\n");

    bu_log("Enable LoD, using coarse scale to enhance visual change...\n");
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

    img_cmp(2, dbp, av[1], false, soft_fail, 0, "lod_clear", "lod");

    bu_log("Disable LoD\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    img_cmp(1, dbp, av[1], false, soft_fail, 0, "lod_clear", "lod");

    bu_log("Re-enable LoD\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    img_cmp(2, dbp, av[1], true, soft_fail, 0, "lod_clear", "lod");

    bu_log("Done.\n");

    /***** Brep LoD ****/

    s_av[0] = "tol";
    s_av[1] = "rel";
    s_av[2] = "0.01";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "brep";
    s_av[1] = "all.g";
    s_av[2] = "brep";
    s_av[3] = "all.brep";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    dbp->dbi_state->update();

    bu_log("Sanity - testing shaded mode 1 (triangle only) drawing, Level-of-Detail disabled...\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m1";
    s_av[2] = "all.brep";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(3, dbp, av[1], false, soft_fail, 0, "lod_clear", "lod");
    bu_log("Done.\n");

    bu_log("Enable LoD, keeping above coarse scale to enhance visual change...\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    img_cmp(4, dbp, av[1], false, soft_fail, 0, "lod_clear", "lod");

    bu_log("Disable LoD\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    img_cmp(3, dbp, av[1], false, soft_fail, 0, "lod_clear", "lod");

    bu_log("Re-enable LoD\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    img_cmp(4, dbp, av[1], true, soft_fail, 0, "lod_clear", "lod");

    bu_log("Done.\n");


    /***** CSG LoD ****/
    bu_log("Sanity - testing wireframe, Level-of-Detail disabled...\n");
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

    img_cmp(5, dbp, av[1], false, soft_fail, 0, "lod_clear", "lod");
    bu_log("Done.\n");

    bu_log("Enable LoD...\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "csg";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    img_cmp(6, dbp, av[1], false, soft_fail, 0, "lod_clear", "lod");

    bu_log("Disable LoD\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "csg";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    img_cmp(5, dbp, av[1], false, soft_fail, 0, "lod_clear", "lod");

    bu_log("Re-enable LoD\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "csg";
    s_av[3] = "1";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    img_cmp(6, dbp, av[1], true, soft_fail, 0, "lod_clear", "lod");

    bu_log("Done.\n");

    /* Check cache behavior */

    /* Fully clear any cached LoD data */
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "cache";
    s_av[3] = "clear";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    /* Generate cached LoD data */
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "cache";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    /* Enable LoD and check drawing of meshes and breps */
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

    img_cmp(2, dbp, av[1], true, soft_fail, 10, "lod_clear", "lod");

    s_av[0] = "draw";
    s_av[1] = "-m1";
    s_av[2] = "all.brep";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(4, dbp, av[1], true, soft_fail, 10, "lod_clear", "lod");

    /* Fully clear any cached LoD data */
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "cache";
    s_av[3] = "clear";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

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

