/*                         Q U A D . C P P
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
/** @file quad.cpp
 *
 * Testing drawing routines and view management with multiple shared
 * and independent views.
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
dm_refresh(struct ged *gedp, int vnum)
{
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    struct bview *v = (struct bview *)BU_PTBL_GET(views, vnum);
    if (!v)
	return;
    BViewState *bvs = gedp->dbi_state->get_view_state(v);
    gedp->dbi_state->update();
    std::unordered_set<struct bview *> uset;
    uset.insert(v);
    bvs->redraw(NULL, uset, 1);

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
scene_clear(struct ged *gedp, int vnum)
{
    const char *s_av[2] = {NULL};
    s_av[0] = "Z";
    ged_exec(gedp, 1, s_av);
    dm_refresh(gedp, vnum);
}

void
img_cmp(int vnum, int id, struct ged *gedp, const char *cdir, bool clear, int soft_fail)
{
    icv_image_t *ctrl, *timg;
    struct bu_vls tname = BU_VLS_INIT_ZERO;
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    if (id <= 0) {
	bu_vls_sprintf(&tname, "quad_clear.png");
	bu_vls_sprintf(&cname, "%s/empty.png", cdir);
    } else {
	bu_vls_sprintf(&tname, "quad_%02d_%03d.png", vnum, id);
	bu_vls_sprintf(&cname, "%s/quad_%02d_%03d_ctrl.png", cdir, vnum, id);
    }

    dm_refresh(gedp, vnum);

    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    struct bview *v = (struct bview *)BU_PTBL_GET(views, vnum);
    if (!v)
	bu_exit(EXIT_FAILURE, "Invalid view specifier: %d\n", vnum);
    struct dm *dmp = (struct dm *)v->dmp;

    const char *s_av[4] = {NULL};
    s_av[0] = "screengrab";
    s_av[1] = "-D";
    s_av[2] = bu_vls_cstr(dm_get_pathname(dmp));
    s_av[3] = bu_vls_cstr(&tname);
    if (ged_exec(gedp, 4, s_av) & BRLCAD_ERROR) {
	bu_log("Failed to grab screen for DM %s\n", bu_vls_cstr(dm_get_pathname(dmp)));
	if (clear)
	    scene_clear(gedp, vnum);
	bu_vls_free(&tname);
	return;
    }

    timg = icv_read(bu_vls_cstr(&tname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!timg) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&tname));
	    if (clear)
		scene_clear(gedp, vnum);
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
		scene_clear(gedp, vnum);
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
		scene_clear(gedp, vnum);
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
	scene_clear(gedp, vnum);
}

/* Creates a view circle "c1" */
void
poly_circ(struct ged *gedp, int v_id)
{
    struct bu_vls vname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vname, "V%d", v_id);

    const char *s_av[15] = {NULL};
    s_av[0] = "view";
    s_av[1] = "-V";
    s_av[2] = bu_vls_cstr(&vname);
    s_av[3] = "obj";
    s_av[4] = "c1";
    s_av[5] = "polygon";
    s_av[6] = "create";
    s_av[7] = "256";
    s_av[8] = "256";
    s_av[9] = "circle";
    s_av[10] = NULL;
    ged_exec(gedp, 10, s_av);

    s_av[0] = "view";
    s_av[1] = "-V";
    s_av[2] = bu_vls_cstr(&vname);
    s_av[3] = "obj";
    s_av[4] = "c1";
    s_av[5] = "update";
    s_av[6] = "300";
    s_av[7] = "300";
    s_av[8] = NULL;
    ged_exec(gedp, 8, s_av);

    bu_vls_free(&vname);
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
    std::ofstream tmpg("moss_lod_tmp.g", std::ios::binary);
    tmpg << orig.rdbuf();
    orig.close();
    tmpg.close();

    /* Open the temp file */
    const char *s_av[15] = {NULL};
    dbp = ged_open("db", "moss_lod_tmp.g", 1);

    // Set callback so database changes will update dbi_state
    db_add_changed_clbk(dbp->dbip, &ged_changed_callback, (void *)dbp);

    // Set up the views.  Unlike the other drawing tests, we are explicitly
    // out to test the behavior of multiple views and dms, so we need to
    // set up multiples.  We'll start out with four non-independent views,
    // to mimic the most common multi-dm/view display - a Quad view widget.
    // Each view will get its own attached swrast DM.
    for (size_t i = 0; i < 4; i++) {
	struct bview *v;
	BU_GET(v, struct bview);
	if (!i)
	    dbp->ged_gvp = v;
	bv_init(v, &dbp->ged_views);
	bu_vls_sprintf(&v->gv_name, "V%zd", i);
	bv_set_add_view(&dbp->ged_views, v);

	/* To generate images that will allow us to check if the drawing
	 * is proceeding as expected, we use the swrast off-screen dm. */
	struct bu_vls dm_name = BU_VLS_INIT_ZERO;
	s_av[0] = "dm";
	s_av[1] = "attach";
	s_av[2] = "-V";
	s_av[3] = bu_vls_cstr(&v->gv_name);
	s_av[4] = "swrast";
	bu_vls_sprintf(&dm_name, "SW%zd", i);
	s_av[5] = bu_vls_cstr(&dm_name);
	s_av[6] = NULL;
	ged_exec(dbp, 6, s_av);
	bu_vls_free(&dm_name);

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
    }

    /* Set up a basic wireframe */
    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 4, s_av);


    /* Set distinct view az/el for each of the four quad views */
    s_av[0] = "view";
    s_av[1] = "-V";
    s_av[2] = "V0";
    s_av[3] = "aet";
    s_av[4] = "35";
    s_av[5] = "25";
    s_av[6] = "0";
    s_av[7] = NULL;
    ged_exec(dbp, 7, s_av);

    s_av[2] = "V1";
    s_av[4] = "90";
    s_av[5] = "0";
    s_av[6] = "0";
    ged_exec(dbp, 7, s_av);

    s_av[2] = "V2";
    s_av[4] = "0";
    s_av[5] = "90";
    s_av[6] = "0";
    ged_exec(dbp, 7, s_av);

    s_av[2] = "V3";
    s_av[4] = "0";
    s_av[5] = "0";
    s_av[6] = "90";
    ged_exec(dbp, 7, s_av);

    img_cmp(0, 1, dbp, av[1], false, soft_fail);
    img_cmp(1, 1, dbp, av[1], false, soft_fail);
    img_cmp(2, 1, dbp, av[1], false, soft_fail);
    img_cmp(3, 1, dbp, av[1], true, soft_fail);

    s_av[0] = "Z";
    ged_exec(dbp, 1, s_av);

    for (int i = 0; i < 4; i++)
	dm_refresh(dbp, i);

    img_cmp(0, -1, dbp, av[1], false, soft_fail);
    img_cmp(1, -1, dbp, av[1], false, soft_fail);
    img_cmp(2, -1, dbp, av[1], false, soft_fail);
    img_cmp(3, -1, dbp, av[1], false, soft_fail);

    /* Check behavior of a view element */
    poly_circ(dbp, 1);
    img_cmp(0, 2, dbp, av[1], false, soft_fail);
    img_cmp(1, 2, dbp, av[1], false, soft_fail);
    img_cmp(2, 2, dbp, av[1], false, soft_fail);
    img_cmp(3, 2, dbp, av[1], true, soft_fail);



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

