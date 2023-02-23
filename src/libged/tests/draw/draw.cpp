/*                       D R A W . C P P
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
/** @file draw.cpp
 *
 * Testing routines for new drawing logic
 *
 */

#include "common.h"

#include <stdio.h>
#include <fstream>

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

#include <bu.h>
#include <bg/lod.h>
#include <icv.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>

// In order to handle changes to .g geometry contents, we need to defined
// callbacks for the librt hooks that will update the working data structures.
// In Qt we have libqtcad handle this, but as we are not using a QgModel we
// need to do it ourselves.
//
// TODO - can probably make this a "standard" callback in libged, set up by
// default but something that can be overridden/augmented if the app needs to
// do extra work.
extern "C" void
ged_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data)
{
    XXH64_state_t h_state;
    unsigned long long hash;
    struct ged *gedp = (struct ged *)u_data;
    DbiState *ctx = gedp->dbi_state;

    // Clear cached GED drawing data and update
    ctx->clear_cache(dp);

    // Need to invalidate any LoD caches associated with this dp
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT && ctx->gedp) {
	unsigned long long key = bg_mesh_lod_key_get(ctx->gedp->ged_lod, dp->d_namep);
	if (key) {
	    bg_mesh_lod_clear_cache(ctx->gedp->ged_lod, key);
	    bg_mesh_lod_key_put(ctx->gedp->ged_lod, dp->d_namep, 0);
	}
    }

    switch(mode) {
	case 0:
	    ctx->changed.insert(dp);
	    break;
	case 1:
	    ctx->added.insert(dp);
	    break;
	case 2:
	    // When this callback is made, dp is still valid, but in subsequent
	    // processing it will not be.  We need to capture everything we
	    // will need from this dp now, for later use when updating state
	    XXH64_reset(&h_state, 0);
	    XXH64_update(&h_state, dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	    hash = (unsigned long long)XXH64_digest(&h_state);
	    ctx->removed.insert(hash);
	    ctx->old_names[hash] = std::string(dp->d_namep);
	    break;
	default:
	    bu_log("changed callback mode error: %d\n", mode);
    }
}

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
img_cmp(int id, struct ged *gedp, const char *cdir, bool clear, int soft_fail)
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

    timg = icv_read(bu_vls_cstr(&tname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!timg) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&tname));
	    if (clear)
		scene_clear(gedp);
	    bu_vls_free(&tname);
	    return;
	}
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&tname));
    }
    ctrl = icv_read(bu_vls_cstr(&cname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!ctrl) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&cname));
	    if (clear)
		scene_clear(gedp);
	    bu_vls_free(&tname);
	    bu_vls_free(&cname);
	    return;
	}
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&cname));
    }
    bu_vls_free(&cname);
    int matching_cnt = 0;
    int off_by_1_cnt = 0;
    int off_by_many_cnt = 0;
    int iret = icv_diff(&matching_cnt, &off_by_1_cnt, &off_by_many_cnt, ctrl,timg);
    if (iret) {
	if (soft_fail) {
	    bu_log("%d wireframe diff failed.  %d matching, %d off by 1, %d off by many\n", id, matching_cnt, off_by_1_cnt, off_by_many_cnt);
	    icv_destroy(ctrl);
	    icv_destroy(timg);
	    if (clear)
		scene_clear(gedp);
	    return;
	}
	bu_exit(EXIT_FAILURE, "%d wireframe diff failed.  %d matching, %d off by 1, %d off by many\n", id, matching_cnt, off_by_1_cnt, off_by_many_cnt);
    }

    icv_destroy(ctrl);
    icv_destroy(timg);

    // Image comparison done and successful - clear image
    bu_file_delete(bu_vls_cstr(&tname));
    bu_vls_free(&tname);

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
    std::ofstream tmpg("moss_tmp.g", std::ios::binary);
    tmpg << orig.rdbuf();
    orig.close();
    tmpg.close();

    /* Open the temp file */
    const char *s_av[15] = {NULL};
    dbp = ged_open("db", "moss_tmp.g", 1);

    // Set callback so database changes will update dbi_state
    db_add_changed_clbk(dbp->dbip, &ged_changed_callback, (void *)dbp);

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

    v->gv_base2local = dbp->dbip->dbi_base2local;
    v->gv_local2base = dbp->dbip->dbi_local2base;

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
    img_cmp(1, dbp, av[1], true, soft_fail);

    // Check that everything is in fact cleared
    img_cmp(0, dbp, av[1], false, soft_fail);
    bu_log("Done.\n");

    /***** Polygon circle *****/
    bu_log("Testing view polygon circle draw...\n");
    poly_circ(dbp);
    img_cmp(2, dbp, av[1], true, soft_fail);

    // Check that everything is in fact cleared
    img_cmp(0, dbp, av[1], false, soft_fail);
    bu_log("Done.\n");

    /***** Polygon ellipse *****/
    bu_log("Testing view polygon ellipse draw...\n");
    poly_ell(dbp);
    img_cmp(3, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    /***** Polygon square *****/
    bu_log("Testing view polygon square draw...\n");
    poly_sq(dbp);
    img_cmp(4, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    /***** Polygon rectangle *****/
    bu_log("Testing view polygon rectangle draw...\n");
    poly_rect(dbp);
    img_cmp(5, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    /***** Polygon general *****/
    bu_log("Testing view general polygon draw...\n");
    poly_general(dbp);
    img_cmp(6, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    /***** Test draw UP and DOWN *****/
    bu_log("Testing UP and DOWN visibility control of drawn objects...\n");
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
    img_cmp(0, dbp, av[1], false, soft_fail);

    s_av[4] = "UP";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);
    // Enabling the draw should produce the same visual as the general polygon
    // draw test above, so we can check using the same image
    img_cmp(6, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    /***** Test view polygon booleans: union ****/
    bu_log("Testing view polygon boolean operation: union...\n");
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
    img_cmp(7, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    /***** Test view polygon booleans: subtraction ****/
    bu_log("Testing view polygon boolean operation: subtraction...\n");
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
    img_cmp(8, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    /***** Test view polygon booleans: intersection ****/
    bu_log("Testing view polygon boolean operation: intersection...\n");
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
    img_cmp(9, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");


    /***** Test color ****/
    bu_log("Testing setting view object color...\n");
    poly_general(dbp);
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "g1";
    s_av[3] = "color";
    s_av[4] = "0/255/0";
    s_av[5] = NULL;
    ged_exec(dbp, 5, s_av);

    // See if we got what we expected
    img_cmp(10, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    /***** Test fill ****/
    bu_log("Testing enabling polygon fill...\n");
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
    img_cmp(11, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    if (run_unstable_tests) {
	/***** Test label ****/
	bu_log("Testing label with leader line...\n");
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

	img_cmp(12, dbp, av[1], false, soft_fail);

	s_av[0] = "ae";
	s_av[1] = "10";
	s_av[2] = "4";
	s_av[3] = "11";
	s_av[4] = NULL;
	ged_exec(dbp, 4, s_av);
	img_cmp(13, dbp, av[1], false, soft_fail);

	s_av[0] = "ae";
	s_av[1] = "270";
	s_av[2] = "0";
	s_av[3] = "0";
	s_av[4] = NULL;
	ged_exec(dbp, 4, s_av);
	img_cmp(14, dbp, av[1], false, soft_fail);

	s_av[0] = "ae";
	s_av[1] = "48";
	s_av[2] = "16";
	s_av[3] = "143";
	s_av[4] = NULL;
	ged_exec(dbp, 4, s_av);
	img_cmp(15, dbp, av[1], false, soft_fail);

	s_av[0] = "ae";
	s_av[1] = "40";
	s_av[2] = "-15";
	s_av[3] = "180";
	s_av[4] = NULL;
	ged_exec(dbp, 4, s_av);
	img_cmp(16, dbp, av[1], false, soft_fail);

	s_av[0] = "ae";
	s_av[1] = "250";
	s_av[2] = "5";
	s_av[3] = "-140";
	s_av[4] = NULL;
	ged_exec(dbp, 4, s_av);
	img_cmp(17, dbp, av[1], true, soft_fail);

	// Restore view to ae 35/25
	s_av[0] = "ae";
	s_av[1] = "35";
	s_av[2] = "25";
	s_av[3] = "0";
	s_av[4] = NULL;
	ged_exec(dbp, 4, s_av);
	bu_log("Done.\n");
    }

    /***** Test axes ****/
    bu_log("Testing simple data axes drawing...\n");
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

    img_cmp(18, dbp, av[1], false, soft_fail);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "a1";
    s_av[3] = "axes";
    s_av[4] = "axes_color";
    s_av[5] = "0/0/255";
    s_av[6] = NULL;
    ged_exec(dbp, 6, s_av);

    img_cmp(19, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    /***** Test shaded modes ****/
    bu_log("Testing shaded mode 1 (triangle only) drawing, Level-of-Detail disabled...\n");
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
    dbp->dbi_state->update();


    s_av[0] = "draw";
    s_av[1] = "-m1";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(20, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    bu_log("Testing shaded mode 2 drawing (unevaluated primitive shading). (Note: does not use Level-of-Detail)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m2";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(21, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    if (run_unstable_tests) {
	bu_log("Testing mode 3 drawing (evaluated wireframe)...\n");
	s_av[0] = "draw";
	s_av[1] = "-m3";
	s_av[2] = "all.g";
	s_av[3] = NULL;
	ged_exec(dbp, 3, s_av);

	s_av[0] = "autoview";
	s_av[1] = NULL;
	ged_exec(dbp, 1, s_av);

	img_cmp(22, dbp, av[1], true, soft_fail);
	bu_log("Done.\n");
    }

    bu_log("Testing mode 4 drawing (hidden lines)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m4";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(23, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");

    if (run_unstable_tests) {
	bu_log("Testing mode 5 drawing (point based triangles)...\n");
	s_av[0] = "draw";
	s_av[1] = "-m5";
	s_av[2] = "all.g";
	s_av[3] = NULL;
	ged_exec(dbp, 3, s_av);

	s_av[0] = "autoview";
	s_av[1] = NULL;
	ged_exec(dbp, 1, s_av);

	img_cmp(24, dbp, av[1], true, soft_fail);
	bu_log("Done.\n");
    }


    bu_log("Test clearing of previous drawing mode (shaded and wireframe)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m2";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 4, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(1, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");


    bu_log("Testing mixed drawing (shaded and wireframe)...\n");
    s_av[0] = "draw";
    s_av[1] = "-m2";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 3, s_av);

    s_av[0] = "draw";
    s_av[1] = "-A";
    s_av[2] = "-m0";
    s_av[3] = "all.g";
    s_av[4] = NULL;
    ged_exec(dbp, 4, s_av);

    s_av[0] = "autoview";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);

    img_cmp(25, dbp, av[1], true, soft_fail);
    bu_log("Done.\n");


    // Done with moss.g
    s_av[0] = "close";
    s_av[1] = NULL;
    ged_exec(dbp, 1, s_av);
    bu_file_delete("moss_tmp.g");

    if (run_unstable_tests) {
	/* The rook model is a more appropriate test case for mode 3, since its
	 * wireframe is dramatically different when evaluated.*/
	bu_vls_sprintf(&fname, "%s/rook.g", av[1]);
	s_av[0] = "open";
	s_av[1] = bu_vls_cstr(&fname);
	s_av[2] = NULL;
	ged_exec(dbp, 2, s_av);

	bu_log("Testing mode 3 drawing (evaluated wireframe)...\n");
	s_av[0] = "draw";
	s_av[1] = "-m3";
	s_av[2] = "scene.g";
	s_av[3] = NULL;
	ged_exec(dbp, 3, s_av);

	s_av[0] = "autoview";
	s_av[1] = NULL;
	ged_exec(dbp, 1, s_av);

	img_cmp(26, dbp, av[1], true, soft_fail);
	bu_log("Done.\n");
    }

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

