/*                       D R A W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file draw.cpp
 *
 * Testing routines for new drawing logic
 *
 */

#include "common.h"

#include <stdio.h>
#include <bu.h>
#include <icv.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>

void
dm_refresh(struct ged *gedp)
{
    gedp->dbi_state->update();

    struct bview *v= gedp->ged_gvp;
    struct dm *dmp = (struct dm *)v->dmp;
    unsigned char *dm_bg1;
    unsigned char *dm_bg2;
    dm_get_bg(&dm_bg1, &dm_bg2, dmp);
    dm_set_bg(dmp, dm_bg1[0], dm_bg1[1], dm_bg1[2], dm_bg2[0], dm_bg2[1], dm_bg2[2]);
    dm_set_dirty(dmp, 0);
    dm_draw_objs(v, v->gv_base2local, v->gv_local2base, NULL, NULL);
    dm_draw_end(dmp);
}

void
scene_clear(struct ged *gedp)
{
    const char *s_av[2] = {NULL};
    s_av[0] = "Z";
    ged_exec(gedp, 1, s_av);
    dm_refresh(gedp);
}

void
img_cmp(int id, struct ged *gedp, const char *cdir, bool clear)
{
    icv_image_t *ctrl, *timg;
    struct bu_vls tname = BU_VLS_INIT_ZERO;
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    if (id <= 0) {
	bu_vls_sprintf(&tname, "clear.png");
	bu_vls_sprintf(&cname, "%s/empty.png", cdir);
    } else {
	bu_vls_sprintf(&tname, "v%03d.png", id);
	bu_vls_sprintf(&cname, "%s/v%03d_ctrl.png", cdir, id);
    }

    dm_refresh(gedp);
    const char *s_av[2] = {NULL};
    s_av[0] = "screengrab";
    s_av[1] = bu_vls_cstr(&tname);
    ged_exec(gedp, 2, s_av);

    timg = icv_read(bu_vls_cstr(&tname), BU_MIME_IMAGE_AUTO, 0, 0);
    if (!timg)
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&tname));
    ctrl = icv_read(bu_vls_cstr(&cname), BU_MIME_IMAGE_AUTO, 0, 0);
    if (!ctrl)
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&cname));
    if (icv_diff(NULL, NULL, NULL, ctrl,timg)) {
	bu_exit(EXIT_FAILURE, "%d wireframe diff failed\n", id);
    }
    icv_destroy(ctrl);
    icv_destroy(timg);

    // Image comparison done and successful - clear image
    bu_file_delete(bu_vls_cstr(&tname));

    bu_vls_free(&tname);
    bu_vls_free(&cname);
    if (clear)
	scene_clear(gedp);
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
    s_av[2] = "c1";
    s_av[3] = "polygon";
    s_av[4] = "create";
    s_av[5] = "256";
    s_av[6] = "256";
    s_av[7] = "circle";
    s_av[8] = NULL;
    ged_exec(gedp, 8, s_av);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "c1";
    s_av[3] = "update";
    s_av[4] = "300";
    s_av[5] = "300";
    s_av[6] = NULL;
    ged_exec(gedp, 6, s_av);
}

/* Creates a view ellipse "e1" */
void
poly_ell(struct ged *gedp)
{
    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "e1";
    s_av[3] = "polygon";
    s_av[4] = "create";
    s_av[5] = "300";
    s_av[6] = "256";
    s_av[7] = "ellipse";
    s_av[8] = NULL;
    ged_exec(gedp, 8, s_av);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "e1";
    s_av[3] = "update";
    s_av[4] = "400";
    s_av[5] = "300";
    s_av[6] = NULL;
    ged_exec(gedp, 6, s_av);
}

/* Creates a view square "s1" */
void
poly_sq(struct ged *gedp)
{
    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "s1";
    s_av[3] = "polygon";
    s_av[4] = "create";
    s_av[5] = "200";
    s_av[6] = "200";
    s_av[7] = "square";
    s_av[8] = NULL;
    ged_exec(gedp, 8, s_av);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "s1";
    s_av[3] = "update";
    s_av[4] = "310";
    s_av[5] = "310";
    s_av[6] = NULL;
    ged_exec(gedp, 6, s_av);
}

/* Creates a view rectangle "r1" */
void
poly_rect(struct ged *gedp)
{
    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "r1";
    s_av[3] = "polygon";
    s_av[4] = "create";
    s_av[5] = "190";
    s_av[6] = "190";
    s_av[7] = "rectangle";
    s_av[8] = NULL;
    ged_exec(gedp, 8, s_av);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "r1";
    s_av[3] = "update";
    s_av[4] = "380";
    s_av[5] = "290";
    s_av[6] = NULL;
    ged_exec(gedp, 6, s_av);
}


/* Creates a general polygon "g1" */
void
poly_general(struct ged *gedp)
{
    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "g1";
    s_av[3] = "polygon";
    s_av[4] = "create";
    s_av[5] = "190";
    s_av[6] = "350";
    s_av[7] = NULL;
    ged_exec(gedp, 7, s_av);

    s_av[4] = "append";
    s_av[5] = "400";
    s_av[6] = "300";
    ged_exec(gedp, 7, s_av);

    s_av[4] = "append";
    s_av[5] = "380";
    s_av[6] = "300";
    ged_exec(gedp, 7, s_av);

    s_av[4] = "append";
    s_av[5] = "230";
    s_av[6] = "245";
    ged_exec(gedp, 7, s_av);

    s_av[4] = "append";
    s_av[5] = "180";
    s_av[6] = "150";
    ged_exec(gedp, 7, s_av);

    s_av[4] = "append";
    s_av[5] = "210";
    s_av[6] = "300";
    ged_exec(gedp, 7, s_av);

    s_av[4] = "close";
    s_av[5] = NULL;
    ged_exec(gedp, 5, s_av);
}

int
main(int ac, char *av[]) {
    struct ged *dbp;

    bu_setprogname(av[0]);

    if (ac != 3) {
	printf("Usage: %s file.g ctrl_img_dir\n", av[0]);
	return 1;
    }

    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    if (!bu_file_directory(av[2])) {
	printf("ERROR: [%s] is not a directory.  Expecting control image directory\n", av[2]);
	return 2;
    }

    /* We are going to generate geometry from the basic moss data,
     * so we make a temporary copy */
    char draw_tmpfile[MAXPATHLEN];
    FILE *fp = bu_temp_file(draw_tmpfile, MAXPATHLEN);
    if (fp == NULL) {
	bu_exit(EXIT_FAILURE, "Error: unable to create temporary .g file\n");
	return 1;
    } else {
	bu_log("Working .g file copy: %s\n", draw_tmpfile);
    }

    /* Manually turn the temp file into a .g */
    if (db5_fwrite_ident(fp, "libged drawing test file", 1.0) < 0) {
	bu_log("Error: failed to prepare .g file %s\n", draw_tmpfile);
	return 1;
    }
    (void)fclose(fp);

    /* Enable all the experimental logic */
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);
    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);
    bu_setenv("LIBGED_DBI_STATE", "1", 1);

    /* FIXME: To draw, we need to init this LIBRT global */
    BU_LIST_INIT(&RTG.rtg_vlfree);

    /* Open the temp file, then dbconcat argv[1] into it */
    const char *s_av[15] = {NULL};
    dbp = ged_open("db", draw_tmpfile, 1);
    s_av[0] = "dbconcat";
    s_av[1] = "-u";
    s_av[2] = "-c";
    s_av[3] = av[1];
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    // TODO - dbconcat and dbi_state aren't playing nice right now - work
    // around this by closing and re-opening the file to make a new gedp, but
    // need to run down why dbconcat is managing to alter the .g without
    // dbi_state hearing about it...
    ged_close(dbp);
    dbp = ged_open("db", draw_tmpfile, 1);

    // Set up the view
    BU_GET(dbp->ged_gvp, struct bview);
    bv_init(dbp->ged_gvp, &dbp->ged_views);
    bu_vls_sprintf(&dbp->ged_gvp->gv_name, "default");
    bv_set_add_view(&dbp->ged_views, dbp->ged_gvp);

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
    dm_set_pathname(dmp, "SWDM");
    dm_set_zbuffer(dmp, 1);

    // See QtSW.cpp...
    fastf_t windowbounds[6] = { -1, 1, -1, 1, -100, 100 };
    dm_set_win_bounds(dmp, windowbounds);

    dm_set_vp(dmp, &v->gv_scale);
    v->dmp = dmp;
    v->gv_width = dm_get_width(dmp);
    v->gv_height = dm_get_height(dmp);


    /***** Basic wireframe draw *****/
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
    img_cmp(1, dbp, av[2], true);

    // Check that everything is in fact cleared
    img_cmp(0, dbp, av[2], false);
    bu_log("Done.\n");

    /***** Polygon circle *****/
    bu_log("Testing view polygon circle draw...\n");
    poly_circ(dbp);
    img_cmp(2, dbp, av[2], true);

    // Check that everything is in fact cleared
    img_cmp(0, dbp, av[2], false);
    bu_log("Done.\n");

    /***** Polygon ellipse *****/
    bu_log("Testing view polygon ellipse draw...\n");
    poly_ell(dbp);
    img_cmp(3, dbp, av[2], true);
    bu_log("Done.\n");

    /***** Polygon square *****/
    bu_log("Testing view polygon square draw...\n");
    poly_sq(dbp);
    img_cmp(4, dbp, av[2], true);
    bu_log("Done.\n");

    /***** Polygon rectangle *****/
    bu_log("Testing view polygon rectangle draw...\n");
    poly_rect(dbp);
    img_cmp(5, dbp, av[2], true);
    bu_log("Done.\n");

    /***** Polygon general *****/
    bu_log("Testing view general polygon draw...\n");
    poly_general(dbp);
    img_cmp(6, dbp, av[2], true);
    bu_log("Done.\n");

    /***** Test draw UP and DOWN *****/
    poly_general(dbp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "g1";
    s_av[3] = "draw";
    s_av[4] = "DOWN";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);
    // Should be an empty scene - make sure we don't clear after this
    // comparison, as we want to re-enable the drawing of this object.
    img_cmp(0, dbp, av[2], false);

    s_av[4] = "UP";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);
    // Enabling the draw should produce the same visual as the general polygon
    // draw test above, so we can check using the same image
    img_cmp(6, dbp, av[2], true);

    /***** Test view polygon booleans: union ****/
    poly_circ(dbp);
    poly_ell(dbp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "c1";
    s_av[3] = "polygon";
    s_av[4] = "csg";
    s_av[5] = "u";
    s_av[6] = "e1";
    s_av[7] = NULL;
    ged_exec(dbp, 7, s_av);

    // Result is stored in c1 - turn off e1
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "e1";
    s_av[3] = "draw";
    s_av[4] = "DOWN";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);

    // See if we got what we expected
    img_cmp(7, dbp, av[2], true);

    /***** Test view polygon booleans: subtraction ****/
    poly_circ(dbp);
    poly_ell(dbp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "c1";
    s_av[3] = "polygon";
    s_av[4] = "csg";
    s_av[5] = "-";
    s_av[6] = "e1";
    s_av[7] = NULL;
    ged_exec(dbp, 7, s_av);

    // Result is stored in c1 - turn off e1
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "e1";
    s_av[3] = "draw";
    s_av[4] = "DOWN";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);

    // See if we got what we expected
    img_cmp(8, dbp, av[2], true);

    /***** Test view polygon booleans: intersection ****/
    poly_circ(dbp);
    poly_ell(dbp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "c1";
    s_av[3] = "polygon";
    s_av[4] = "csg";
    s_av[5] = "+";
    s_av[6] = "e1";
    s_av[7] = NULL;
    ged_exec(dbp, 7, s_av);

    // Result is stored in c1 - turn off e1
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "e1";
    s_av[3] = "draw";
    s_av[4] = "DOWN";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);

    // See if we got what we expected
    img_cmp(9, dbp, av[2], true);


    /***** Test color ****/
    poly_general(dbp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "g1";
    s_av[3] = "color";
    s_av[4] = "0/255/0";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);

    // See if we got what we expected
    img_cmp(10, dbp, av[2], true);

    /***** Test fill ****/
    poly_general(dbp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "g1";
    s_av[3] = "polygon";
    s_av[4] = "fill";
    s_av[5] = "1";
    s_av[6] = "10";
    s_av[7] = "3";
    s_av[8] = NULL;
    ged_exec(dbp, 8, s_av);

    // See if we got what we expected
    img_cmp(11, dbp, av[2], true);

    /***** Test label ****/
    s_av[0] = "draw";
    s_av[1] = "all.g";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "lbl1";
    s_av[3] = "label";
    s_av[4] = "create";
    s_av[5] = "LIGHT";
    s_av[6] = "110.41";
    s_av[7] = "-32.2352";
    s_av[8] = "90.4497";
    s_av[9] = "20.1576";
    s_av[10] = "-13.526";
    s_av[11] = "8";
    s_av[12] = NULL;
    ged_exec(dbp, 12, s_av);

    img_cmp(12, dbp, av[2], false);

    s_av[0] = "ae";
    s_av[1] = "10";
    s_av[2] = "4";
    s_av[3] = "11";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(13, dbp, av[2], false);

    s_av[0] = "ae";
    s_av[1] = "270";
    s_av[2] = "0";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(14, dbp, av[2], false);

    s_av[0] = "ae";
    s_av[1] = "48";
    s_av[2] = "16";
    s_av[3] = "143";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(15, dbp, av[2], false);

    s_av[0] = "ae";
    s_av[1] = "40";
    s_av[2] = "-15";
    s_av[3] = "180";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(16, dbp, av[2], false);

    s_av[0] = "ae";
    s_av[1] = "250";
    s_av[2] = "5";
    s_av[3] = "-140";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);
    img_cmp(17, dbp, av[2], true);

    // Restore view to ae 35/25
    s_av[0] = "ae";
    s_av[1] = "35";
    s_av[2] = "25";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    /***** Test axes ****/
    s_av[0] = "draw";
    s_av[1] = "all.g";
    s_av[2] = NULL;
    ged_exec(dbp, 2, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "a1";
    s_av[3] = "axes";
    s_av[4] = "create";
    s_av[5] = "1";
    s_av[6] = "1";
    s_av[7] = "1";
    s_av[8] = NULL;
    ged_exec(dbp, 8, s_av);

    img_cmp(18, dbp, av[2], false);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "a1";
    s_av[3] = "axes";
    s_av[4] = "axes_color";
    s_av[5] = "0/0/255";
    s_av[6] = NULL;
    ged_exec(dbp, 6, s_av);

    img_cmp(19, dbp, av[2], true);

    /***** Test shaded modes ****/
    s_av[0] = "view";
    s_av[1] = "lod";
    s_av[2] = "mesh";
    s_av[3] = "0";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    s_av[0] = "facetize";
    s_av[1] = "-r";
    s_av[2] = "all.g";
    s_av[3] = "all.bot";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    // TODO - -m1 is broken in draw_rework branch??
    //s_av[0] = "draw";
    //s_av[1] = "-m1";
    //s_av[2] = "all.bot";
    //s_av[3] = NULL;
    //ged_exec(dbp, 3, s_av);

    //img_cmp(20, dbp, av[2], true);

    s_av[0] = "draw";
    s_av[1] = "-m2";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    img_cmp(21, dbp, av[2], true);


    ged_close(dbp);
    bu_file_delete(draw_tmpfile);

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

