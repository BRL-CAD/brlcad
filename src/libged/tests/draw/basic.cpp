/*                      B A S I C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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
/** @file basic.cpp
 *
 * Testing routines for new drawing logic
 */

#include "common.h"
#include <cstring>
#include <fstream>

#include <bu.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>
#include <ged/db_index.h>
#include <ged/event_txn.h>

#define ADIFF_THRES 20

extern "C" void ged_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data);
extern "C" int img_cmp(int id, struct ged *gedp, const char *cdir, bool clear_scene, bool clear_image, int soft_fail, int approximate_check, const char *clear_root, const char *img_root);
extern "C" void scene_clear(struct ged *gedp);

static int
view_polygon_csg_semantic_check(struct ged *gedp, const char *name)
{
    const char *s_av[7] = {"view", "obj", "list", name, NULL};
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (ged_exec_view(gedp, 4, s_av) != BRLCAD_OK) {
	bu_log("view polygon semantic check failed: list command error for %s: %s\n",
		name, bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }
    if (!strstr(bu_vls_cstr(gedp->ged_result_str), name)) {
	bu_log("view polygon semantic check failed: %s not listed after CSG\n", name);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

/* We will often want to do multiple different operations with
 * similar shapes - to make this easier, we encapsulate the
 * creation calls for some reusable cases */

/* Creates a view circle "c1" */
void
poly_circ(struct ged *gedp)
{
    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "c1";
    s_av[4] = "polygon";
    s_av[5] = "create";
    s_av[6] = "256";
    s_av[7] = "256";
    s_av[8] = "circle";
    s_av[9] = NULL;
    ged_exec_view(gedp, 9, s_av);

    s_av[2] = "set";
    s_av[3] = "c1";
    s_av[4] = "update";
    s_av[5] = "300";
    s_av[6] = "300";
    s_av[7] = NULL;
    ged_exec_view(gedp, 7, s_av);
}

/* Creates a view ellipse "e1" */
void
poly_ell(struct ged *gedp)
{
    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "e1";
    s_av[4] = "polygon";
    s_av[5] = "create";
    s_av[6] = "300";
    s_av[7] = "256";
    s_av[8] = "ellipse";
    s_av[9] = NULL;
    ged_exec_view(gedp, 9, s_av);

    s_av[2] = "set";
    s_av[3] = "e1";
    s_av[4] = "update";
    s_av[5] = "400";
    s_av[6] = "300";
    s_av[7] = NULL;
    ged_exec_view(gedp, 7, s_av);
}

/* Creates a view square "s1" */
void
poly_sq(struct ged *gedp)
{
    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "s1";
    s_av[4] = "polygon";
    s_av[5] = "create";
    s_av[6] = "200";
    s_av[7] = "200";
    s_av[8] = "square";
    s_av[9] = NULL;
    ged_exec_view(gedp, 9, s_av);

    s_av[2] = "set";
    s_av[3] = "s1";
    s_av[4] = "update";
    s_av[5] = "310";
    s_av[6] = "310";
    s_av[7] = NULL;
    ged_exec_view(gedp, 7, s_av);
}

/* Creates a view rectangle "r1" */
void
poly_rect(struct ged *gedp)
{
    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "r1";
    s_av[4] = "polygon";
    s_av[5] = "create";
    s_av[6] = "190";
    s_av[7] = "190";
    s_av[8] = "rectangle";
    s_av[9] = NULL;
    ged_exec_view(gedp, 9, s_av);

    s_av[2] = "set";
    s_av[3] = "r1";
    s_av[4] = "update";
    s_av[5] = "380";
    s_av[6] = "290";
    s_av[7] = NULL;
    ged_exec_view(gedp, 7, s_av);
}


/* Creates a general polygon "g1" */
void
poly_general(struct ged *gedp)
{
    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "g1";
    s_av[4] = "polygon";
    s_av[5] = "create";
    s_av[6] = "190";
    s_av[7] = "350";
    s_av[8] = NULL;
    ged_exec_view(gedp, 8, s_av);

    s_av[5] = "append";
    s_av[6] = "400";
    s_av[7] = "300";
    ged_exec_view(gedp, 8, s_av);

    s_av[6] = "380";
    s_av[7] = "300";
    ged_exec_view(gedp, 8, s_av);

    s_av[6] = "230";
    s_av[7] = "245";
    ged_exec_view(gedp, 8, s_av);

    s_av[6] = "180";
    s_av[7] = "150";
    ged_exec_view(gedp, 8, s_av);

    s_av[6] = "210";
    s_av[7] = "300";
    ged_exec_view(gedp, 8, s_av);

    s_av[5] = "close";
    s_av[6] = NULL;
    ged_exec_view(gedp, 6, s_av);
}

int
main(int ac, char *av[]) {
    struct ged *gedp;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    int need_help = 0;
    int soft_fail = 0;
    int keep_images = 0;
    int ret = BRLCAD_OK;

    bu_setprogname(av[0]);

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",            "", NULL,          &need_help, "Print help and exit");
    BU_OPT(d[1], "c", "continue",        "", NULL,          &soft_fail, "Continue testing if a failure is encountered.");
    BU_OPT(d[2], "k", "keep",            "", NULL,        &keep_images, "Keep images generated by the run.");
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

    bool clear_images = !keep_images;

    /* Enable all the experimental logic */
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);

    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    /* We want a local working dir cache */
    char lcache[MAXPATHLEN] = {0};
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CURR, "ged_draw_test_cache", NULL);
    bu_mkdir(lcache);
    bu_setenv("BU_DIR_CACHE", lcache, 1);

    /* We are going to generate geometry from the basic moss data,
     * so we make a temporary copy */
    bu_vls_sprintf(&fname, "%s/moss.g", av[1]);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmpg("moss_tmp.g", std::ios::binary);
    tmpg << orig.rdbuf();
    orig.close();
    tmpg.close();

    /* Open the temp file */
    const char *s_av[15] = {NULL};
    gedp = ged_open("db", "moss_tmp.g", 1);

    bu_setenv("DM_SWRAST", "1", 1);

    // Set callback so database changes notify public GED services.
    db_add_changed_clbk(gedp->dbip, &ged_changed_callback, (void *)gedp);

    /* To generate images that will allow us to check if the drawing
     * is proceeding as expected, we use the swrast off-screen dm. */
    s_av[0] = "dm";
    s_av[1] = "attach";
    s_av[2] = "swrast";
    s_av[3] = "SW";
    s_av[4] = NULL;
    ged_exec_dm(gedp, 4, s_av);

    struct bsg_view *v = gedp->ged_gvp;
    struct dm *dmp = (struct dm *)v->dmp;
    dm_set_width(dmp, 512);
    dm_set_height(dmp, 512);

    dm_configure_win(dmp, 0);
    dm_set_zbuffer(dmp, 1);

    // See QtSW.cpp... TODO - if we need this consistently,
    // it should be part of the dm setup.
    fastf_t windowbounds[6] = { -1, 1, -1, 1, -100, 100 };
    dm_set_win_bounds(dmp, windowbounds);

    // TODO - these syncing operations need to happen whenever the dm size
    // changes - can they be done in dm_set_width/dm_set_height?
    v->gv_width = dm_get_width(dmp);
    v->gv_height = dm_get_height(dmp);
    dm_set_vp(dmp, &v->gv_scale);

    v->gv_base2local = gedp->dbip->dbi_base2local;
    v->gv_local2base = gedp->dbip->dbi_local2base;

    // The default (fast) wireframe has some differences from
    // the slower full OpenGL draw path - disable it for the
    // purposes of these tests.
    s_av[0] = "dm";
    s_av[1] = "set";
    s_av[2] = "fast_wireframe";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec_dm(gedp, 4, s_av);

    /***** Basic wireframe draw *****/
    bu_log("Testing basic db wireframe draw...\n");
    s_av[0] = "draw";
    s_av[1] = "all.g";
    s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    s_av[0] = "ae";
    s_av[1] = "35";
    s_av[2] = "25";
    s_av[3] = NULL;
    ged_exec_ae(gedp, 3, s_av);
    ret += img_cmp(1, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");

    // Check that everything is in fact cleared
    ret += img_cmp(0, gedp, av[1], false, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Polygon circle *****/
    bu_log("Testing view polygon circle draw...\n");
    poly_circ(gedp);
    ret += img_cmp(2, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");

    // Check that everything is in fact cleared
    ret += img_cmp(0, gedp, av[1], false, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Polygon ellipse *****/
    bu_log("Testing view polygon ellipse draw...\n");
    poly_ell(gedp);
    ret += img_cmp(3, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Polygon square *****/
    bu_log("Testing view polygon square draw...\n");
    poly_sq(gedp);
    ret += img_cmp(4, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Polygon rectangle *****/
    bu_log("Testing view polygon rectangle draw...\n");
    poly_rect(gedp);
    ret += img_cmp(5, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Polygon general *****/
    bu_log("Testing view general polygon draw...\n");
    poly_general(gedp);
    ret += img_cmp(6, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Test draw UP and DOWN *****/
    bu_log("Testing UP and DOWN visibility control of drawn objects...\n");
    poly_general(gedp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "set";
    s_av[3] = "g1";
    s_av[4] = "draw";
    s_av[5] = "DOWN";
    s_av[6] = NULL;
    ged_exec_view(gedp, 6, s_av);
    // Should be an empty scene - make sure we don't clear after this
    // comparison, as we want to re-enable the drawing of this object.
    ret += img_cmp(0, gedp, av[1], false, clear_images, soft_fail, 0, "clear", "v");

    s_av[5] = "UP";
    s_av[6] = NULL;
    ged_exec_view(gedp, 6, s_av);
    // Enabling the draw should produce the same visual as the general polygon
    // draw test above, so we can check using the same image
    ret += img_cmp(6, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Test view polygon booleans: union ****/
    bu_log("Testing view polygon boolean operation: union...\n");
    poly_circ(gedp);
    poly_ell(gedp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "c1";
    s_av[4] = "polygon";
    s_av[5] = "csg";
    s_av[6] = "u";
    s_av[7] = "e1";
    s_av[8] = NULL;
    ged_exec_view(gedp, 8, s_av);

    // Result is stored in c1 - turn off e1
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "set";
    s_av[3] = "e1";
    s_av[4] = "draw";
    s_av[5] = "DOWN";
    s_av[6] = NULL;
    ged_exec_view(gedp, 6, s_av);

    ret += view_polygon_csg_semantic_check(gedp, "c1");
    scene_clear(gedp);
    bu_log("Done.\n");

    /***** Test view polygon booleans: subtraction ****/
    bu_log("Testing view polygon boolean operation: subtraction...\n");
    poly_circ(gedp);
    poly_ell(gedp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "c1";
    s_av[4] = "polygon";
    s_av[5] = "csg";
    s_av[6] = "-";
    s_av[7] = "e1";
    s_av[8] = NULL;
    ged_exec_view(gedp, 8, s_av);

    // Result is stored in c1 - turn off e1
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "set";
    s_av[3] = "e1";
    s_av[4] = "draw";
    s_av[5] = "DOWN";
    s_av[6] = NULL;
    ged_exec_view(gedp, 6, s_av);

    ret += view_polygon_csg_semantic_check(gedp, "c1");
    scene_clear(gedp);
    bu_log("Done.\n");

    /***** Test view polygon booleans: intersection ****/
    bu_log("Testing view polygon boolean operation: intersection...\n");
    poly_circ(gedp);
    poly_ell(gedp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "c1";
    s_av[4] = "polygon";
    s_av[5] = "csg";
    s_av[6] = "+";
    s_av[7] = "e1";
    s_av[8] = NULL;
    ged_exec_view(gedp, 8, s_av);

    // Result is stored in c1 - turn off e1
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "set";
    s_av[3] = "e1";
    s_av[4] = "draw";
    s_av[5] = "DOWN";
    s_av[6] = NULL;
    ged_exec_view(gedp, 6, s_av);

    ret += view_polygon_csg_semantic_check(gedp, "c1");
    scene_clear(gedp);
    bu_log("Done.\n");


    /***** Test color ****/
    bu_log("Testing setting view feature color...\n");
    poly_general(gedp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "set";
    s_av[3] = "g1";
    s_av[4] = "color";
    s_av[5] = "0/255/0";
    s_av[6] = NULL;
    ged_exec_view(gedp, 6, s_av);

    // See if we got what we expected
    ret += img_cmp(10, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Test fill ****/
    bu_log("Testing enabling polygon fill...\n");
    poly_general(gedp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "g1";
    s_av[4] = "polygon";
    s_av[5] = "fill";
    s_av[6] = "1";
    s_av[7] = "10";
    s_av[8] = "3";
    s_av[9] = NULL;
    ged_exec_view(gedp, 9, s_av);

    // See if we got what we expected
    ret += img_cmp(11, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Test label ****/
    bu_log("Testing label with leader line...\n");
    s_av[0] = "draw";
    s_av[1] = "all.g";
    s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "lbl1";
    s_av[4] = "label";
    s_av[5] = "create";
    s_av[6] = "LIGHT";
    s_av[7] = "110.41";
    s_av[8] = "-32.2352";
    s_av[9] = "90.4497";
    s_av[10] = "20.1576";
    s_av[11] = "-13.526";
    s_av[12] = "8";
    s_av[13] = NULL;
    ged_exec_view(gedp, 13, s_av);

    ret += img_cmp(12, gedp, av[1], false, clear_images, soft_fail, ADIFF_THRES, "clear", "v");

    s_av[0] = "ae";
    s_av[1] = "10";
    s_av[2] = "4";
    s_av[3] = "11";
    s_av[4] = NULL;
    ged_exec_ae(gedp, 4, s_av);
    ret += img_cmp(13, gedp, av[1], false, clear_images, soft_fail, ADIFF_THRES, "clear", "v");

    s_av[0] = "ae";
    s_av[1] = "270";
    s_av[2] = "0";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec_ae(gedp, 4, s_av);
    ret += img_cmp(14, gedp, av[1], false, clear_images, soft_fail, ADIFF_THRES, "clear", "v");

    s_av[0] = "ae";
    s_av[1] = "48";
    s_av[2] = "16";
    s_av[3] = "143";
    s_av[4] = NULL;
    ged_exec_ae(gedp, 4, s_av);
    ret += img_cmp(15, gedp, av[1], false, clear_images, soft_fail, 50, "clear", "v");

    s_av[0] = "ae";
    s_av[1] = "40";
    s_av[2] = "-15";
    s_av[3] = "180";
    s_av[4] = NULL;
    ged_exec_ae(gedp, 4, s_av);
    ret += img_cmp(16, gedp, av[1], false, clear_images, soft_fail, 60, "clear", "v");

    s_av[0] = "ae";
    s_av[1] = "250";
    s_av[2] = "5";
    s_av[3] = "-140";
    s_av[4] = NULL;
    ged_exec_ae(gedp, 4, s_av);
    ret += img_cmp(17, gedp, av[1], true, clear_images, soft_fail, 35, "clear", "v");

    // Restore view to ae 35/25
    s_av[0] = "ae";
    s_av[1] = "35";
    s_av[2] = "25";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec_ae(gedp, 4, s_av);
    bu_log("Done.\n");

    /***** Test axes ****/
    bu_log("Testing simple data axes drawing...\n");
    s_av[0] = "draw";
    s_av[1] = "all.g";
    s_av[2] = NULL;
    ged_exec_draw(gedp, 2, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "a1";
    s_av[4] = "axes";
    s_av[5] = "create";
    s_av[6] = "1";
    s_av[7] = "1";
    s_av[8] = "1";
    s_av[9] = NULL;
    ged_exec_view(gedp, 9, s_av);

    ret += img_cmp(18, gedp, av[1], false, clear_images, soft_fail, 0, "clear", "v");

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "create";
    s_av[3] = "a1";
    s_av[4] = "axes";
    s_av[5] = "axes_color";
    s_av[6] = "0/0/255";
    s_av[7] = NULL;
    ged_exec_view(gedp, 7, s_av);

    ret += img_cmp(19, gedp, av[1], true, clear_images, soft_fail, 0, "clear", "v");
    bu_log("Done.\n");

    /***** Test shaded modes ****/
    bu_log("Testing shaded mode 1 (triangle only) drawing, Level-of-Detail disabled...\n");
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec_view(gedp, 4, s_av);

    s_av[0] = "facetize";
    s_av[1] = "-r";
    s_av[2] = "all.g";
    s_av[3] = "all.bot";
    s_av[4] = NULL;
    ged_exec_facetize(gedp, 4, s_av);
    ged_db_index_refresh(gedp);
    ged_event_notify_batch_rebuild(gedp, NULL);


    s_av[0] = "draw";
    s_av[1] = "-m1";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    ret += img_cmp(20, gedp, av[1], true, clear_images, soft_fail, ADIFF_THRES, "clear", "v");
    bu_log("Done.\n");

    bu_log("Testing shaded mode 2 drawing (unevaluated primitive shading). (Note: does not use Level-of-Detail)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m2";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    ret += img_cmp(21, gedp, av[1], true, clear_images, soft_fail, ADIFF_THRES, "clear", "v");
    bu_log("Done.\n");

    bu_log("Testing mode 3 drawing (evaluated wireframe)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m3";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    ret += img_cmp(22, gedp, av[1], true, clear_images, soft_fail, ADIFF_THRES, "clear", "v");
    bu_log("Done.\n");

    bu_log("Testing mode 4 drawing (hidden lines)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m4";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    ret += img_cmp(23, gedp, av[1], true, clear_images, soft_fail, 35, "clear", "v");
    bu_log("Done.\n");

    bu_log("Testing mode 5 drawing (point based triangles)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m5";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    // The point based sampling can vary quite a bit visually, so this has a
    // looser tolerance - we just want to be sure we're getting a rendering,
    // not that the rendering is exactly the same.
    ret += img_cmp(24, gedp, av[1], true, clear_images, soft_fail, 70, "clear", "v");
    bu_log("Done.\n");

    bu_log("Test clearing of previous drawing mode (shaded and wireframe)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m2";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 4, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    ret += img_cmp(1, gedp, av[1], true, clear_images, soft_fail, ADIFF_THRES, "clear", "v");
    bu_log("Done.\n");


    bu_log("Testing mixed drawing (shaded and wireframe)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m2";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);

    s_av[0] = "draw";
    s_av[1] = "-A";
    s_av[2] = "-m0";
    s_av[3] = "all.g";
    s_av[4] = NULL;
    ged_exec_draw(gedp, 4, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    ret += img_cmp(25, gedp, av[1], true, clear_images, soft_fail, ADIFF_THRES, "clear", "v");
    bu_log("Done.\n");


    // Done with moss.g
    s_av[0] = "closedb";
    s_av[1] = NULL;
    ged_exec_closedb(gedp, 1, s_av);
    bu_file_delete("moss_tmp.g");

    /* The rook model is a more appropriate test case for mode 3, since its
     * wireframe is dramatically different when evaluated.*/
    bu_vls_sprintf(&fname, "%s/rook.g", av[1]);
    s_av[0] = "opendb";
    s_av[1] = bu_vls_cstr(&fname);
    s_av[2] = NULL;
    ged_exec_opendb(gedp, 2, s_av);

    bu_log("Testing mode 3 drawing (evaluated wireframe)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m3";
    s_av[2] = "scene.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec_autoview(gedp, 1, s_av);

    ret += img_cmp(26, gedp, av[1], true, clear_images, soft_fail, ADIFF_THRES, "clear", "v");
    bu_log("Done.\n");

    ged_close(gedp);

    /* Remove the local cache files */
    bu_dirclear(lcache);

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
