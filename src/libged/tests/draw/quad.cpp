/*                         Q U A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2025 United States Government as represented by
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

#include <bu.h>
#include <bv/lod.h>
#include <icv.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>

#include "../../dbi.h"

// In order to handle changes to .g geometry contents, we need to defined
// callbacks for the librt hooks that will update the working data structures.
// In Qt we have libqtcad handle this, but as we are not using a QgModel we
// need to do it ourselves.
extern "C" void
ged_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data)
{
    struct ged *gedp = (struct ged *)u_data;
    DbiState *ctx = (DbiState *)gedp->dbi_state;

    // Need to invalidate any LoD caches associated with this dp
    if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT && ctx->gedp)
	db_mesh_lod_update(ctx->gedp->dbip, dp->d_namep);

    switch(mode) {
	case 0:
	    ctx->changed.insert(dp);
	    break;
	case 1:
	    ctx->added.insert(dp);
	    break;
	case 2:
	    ctx->removed.insert(dp);
	    break;
	default:
	    bu_log("changed callback mode error: %d\n", mode);
    }
}

void
dm_refresh(struct ged *gedp, int vnum)
{
    struct bview *v = (struct bview *)BU_PTBL_GET(&gedp->ged_views, vnum);
    if (!v)
	return;
    BViewState *bvs = gedp->dbi_state->GetBViewState(v);
    gedp->dbi_state->Sync();

    struct dm *dmp = (struct dm *)v->dmp;
    unsigned char *dm_bg1;
    unsigned char *dm_bg2;
    dm_get_bg(&dm_bg1, &dm_bg2, dmp);
    dm_set_bg(dmp, dm_bg1[0], dm_bg1[1], dm_bg1[2], dm_bg2[0], dm_bg2[1], dm_bg2[2]);

    bvs->Redraw(false);
    bvs->Render();
}

void
scene_clear(struct ged *gedp, int vnum)
{
    const char *s_av[4] = {NULL};
    if (vnum < 0) {
	s_av[0] = "Z";
	s_av[1] = NULL;
	ged_exec_Z(gedp, 1, s_av);
    } else {
	struct bu_vls vname = BU_VLS_INIT_ZERO;
	s_av[0] = "Z";
	s_av[1] = "-V";
	bu_vls_sprintf(&vname, "V%d", vnum);
	s_av[2] = bu_vls_cstr(&vname);
	ged_exec_Z(gedp, 3, s_av);
	bu_vls_free(&vname);
    }
    dm_refresh(gedp, vnum);
}

int
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

    struct bview *v = (struct bview *)BU_PTBL_GET(&gedp->ged_views, vnum);
    if (!v)
	bu_exit(EXIT_FAILURE, "Invalid view specifier: %d\n", vnum);
    struct dm *dmp = (struct dm *)v->dmp;

    const char *s_av[4] = {NULL};
    s_av[0] = "screengrab";
    s_av[1] = "-D";
    s_av[2] = bu_vls_cstr(dm_get_pathname(dmp));
    s_av[3] = bu_vls_cstr(&tname);
    if (ged_exec_screengrab(gedp, 4, s_av) & BRLCAD_ERROR) {
	bu_log("Failed to grab screen for DM %s\n", bu_vls_cstr(dm_get_pathname(dmp)));
	if (clear)
	    scene_clear(gedp, vnum);
	bu_vls_free(&tname);
	return BRLCAD_ERROR;
    }

    timg = icv_read(bu_vls_cstr(&tname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!timg) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&tname));
	    if (clear)
		scene_clear(gedp, vnum);
	    bu_vls_free(&tname);
	    return BRLCAD_ERROR;
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
	    return BRLCAD_ERROR;
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
	    return BRLCAD_ERROR;
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

    return BRLCAD_OK;
}

/* Creates a view circle "c1" */
void
poly_circ(struct ged *gedp, int v_id, int local)
{
    const char *s_av[15] = {NULL};
    struct bu_vls vname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vname, "V%d", v_id);

    if (local) {
	s_av[0] = "view";
	s_av[1] = "-V";
	s_av[2] = bu_vls_cstr(&vname);
	s_av[3] = "obj";
	s_av[4] = "-L";
	s_av[5] = "c1";
	s_av[6] = "polygon";
	s_av[7] = "create";
	s_av[8] = "256";
	s_av[9] = "256";
	s_av[10] = "circle";
	s_av[11] = NULL;
	ged_exec_view(gedp, 11, s_av);
    } else {
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
	ged_exec_view(gedp, 10, s_av);
    }

    s_av[0] = "view";
    s_av[1] = "-V";
    s_av[2] = bu_vls_cstr(&vname);
    s_av[3] = "obj";
    s_av[4] = "c1";
    s_av[5] = "update";
    s_av[6] = "300";
    s_av[7] = "300";
    s_av[8] = NULL;
    ged_exec_view(gedp, 8, s_av);

    bu_vls_free(&vname);
}

/* Creates a shared line */
void
vline(struct ged *gedp, int l_id, int x0, int y0, int z0, int x1, int y1, int z1)
{
    const char *s_av[15] = {NULL};
    struct bu_vls lname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&lname, "l%d", l_id);

    struct bu_vls vx0 = BU_VLS_INIT_ZERO;
    struct bu_vls vy0 = BU_VLS_INIT_ZERO;
    struct bu_vls vz0 = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vx0, "%d", x0);
    bu_vls_sprintf(&vy0, "%d", y0);
    bu_vls_sprintf(&vz0, "%d", z0);

    struct bu_vls vx1 = BU_VLS_INIT_ZERO;
    struct bu_vls vy1 = BU_VLS_INIT_ZERO;
    struct bu_vls vz1 = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vx1, "%d", x1);
    bu_vls_sprintf(&vy1, "%d", y1);
    bu_vls_sprintf(&vz1, "%d", z1);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = bu_vls_cstr(&lname);
    s_av[3] = "line";
    s_av[4] = "create";
    s_av[5] = bu_vls_cstr(&vx0);
    s_av[6] = bu_vls_cstr(&vy0);
    s_av[7] = bu_vls_cstr(&vz0);
    s_av[8] = NULL;
    ged_exec_view(gedp, 8, s_av);

    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = bu_vls_cstr(&lname);
    s_av[3] = "line";
    s_av[4] = "append";
    s_av[5] = bu_vls_cstr(&vx1);
    s_av[6] = bu_vls_cstr(&vy1);
    s_av[7] = bu_vls_cstr(&vz1);
    s_av[8] = NULL;
    ged_exec_view(gedp, 8, s_av);

    bu_vls_free(&lname);
    bu_vls_free(&vx0);
    bu_vls_free(&vy0);
    bu_vls_free(&vz0);
    bu_vls_free(&vx1);
    bu_vls_free(&vy1);
    bu_vls_free(&vz1);
}

/* View local line */
void
l_line(struct ged *gedp, int v_id, int l_id, int x0, int y0, int z0, int x1, int y1, int z1)
{
    const char *s_av[15] = {NULL};
    struct bu_vls vname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vname, "V%d", v_id);

    struct bu_vls lname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&lname, "l%d", l_id);

    struct bu_vls vx0 = BU_VLS_INIT_ZERO;
    struct bu_vls vy0 = BU_VLS_INIT_ZERO;
    struct bu_vls vz0 = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vx0, "%d", x0);
    bu_vls_sprintf(&vy0, "%d", y0);
    bu_vls_sprintf(&vz0, "%d", z0);

    struct bu_vls vx1 = BU_VLS_INIT_ZERO;
    struct bu_vls vy1 = BU_VLS_INIT_ZERO;
    struct bu_vls vz1 = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&vx1, "%d", x1);
    bu_vls_sprintf(&vy1, "%d", y1);
    bu_vls_sprintf(&vz1, "%d", z1);

    s_av[0] = "view";
    s_av[1] = "-V";
    s_av[2] = bu_vls_cstr(&vname);
    s_av[3] = "obj";
    s_av[4] = "-L";
    s_av[5] = bu_vls_cstr(&lname);
    s_av[6] = "line";
    s_av[7] = "create";
    s_av[8] = bu_vls_cstr(&vx0);
    s_av[9] = bu_vls_cstr(&vy0);
    s_av[10] = bu_vls_cstr(&vz0);
    s_av[11] = NULL;
    ged_exec_view(gedp, 11, s_av);

    s_av[0] = "view";
    s_av[1] = "-V";
    s_av[2] = bu_vls_cstr(&vname);
    s_av[3] = "obj";
    s_av[4] = "-L";
    s_av[5] = bu_vls_cstr(&lname);
    s_av[6] = "line";
    s_av[7] = "append";
    s_av[8] = bu_vls_cstr(&vx1);
    s_av[9] = bu_vls_cstr(&vy1);
    s_av[10] = bu_vls_cstr(&vz1);
    s_av[11] = NULL;
    ged_exec_view(gedp, 11, s_av);

    bu_vls_free(&vname);
    bu_vls_free(&lname);
    bu_vls_free(&vx0);
    bu_vls_free(&vy0);
    bu_vls_free(&vz0);
    bu_vls_free(&vx1);
    bu_vls_free(&vy1);
    bu_vls_free(&vz1);
}


int
main(int ac, char *av[]) {
    struct ged *gedp;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    int need_help = 0;
    int run_unstable_tests = 0;
    int soft_fail = 0;
    int ret = BRLCAD_OK;

    bu_setprogname(av[0]);

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",            "", NULL,          &need_help, "Print help and exit");
    BU_OPT(d[1], "U", "enable-unstable", "", NULL, &run_unstable_tests, "Test drawing routines known to differ between build configs/platforms.");
    BU_OPT(d[2], "c", "continue",        "", NULL,          &soft_fail, "Continue testing if a failure is encountered.");
    BU_OPT_NULL(d[3]);

    /* Done with program name */
    (void)bu_opt_parse(NULL, ac, (const char **)av, d);

    if (!bu_file_directory(av[1])) {
	printf("ERROR: [%s] is not a directory.  Expecting control image directory\n", av[1]);
	return 2;
    }

    /* Enable all the experimental logic */
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);

    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    /* We want a local working dir cache */
    char lcache[MAXPATHLEN] = {0};
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CURR, "ged_draw_test_quad_cache", NULL);
    bu_mkdir(lcache);
    bu_setenv("BU_DIR_CACHE", lcache, 1);

    /* We are going to generate geometry from the basic moss data,
     * so we make a temporary copy */
    bu_vls_sprintf(&fname, "%s/moss.g", av[1]);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmpg("moss_quad_tmp.g", std::ios::binary);
    tmpg << orig.rdbuf();
    orig.close();
    tmpg.close();

    /* Open the temp file */
    const char *s_av[15] = {NULL};
    gedp = ged_open("db", "moss_quad_tmp.g", 1);

    // Set up new cmd data (not yet done by default in ged_open
    gedp->dbi_state = new DbiState(gedp);
    gedp->new_cmd_forms = 1;
    bu_setenv("DM_SWRAST", "1", 1);

    // Set callback so database changes will update dbi_state
    db_add_changed_clbk(gedp->dbip, &ged_changed_callback, (void *)gedp);

    // Set up the extra views.  Unlike the other drawing tests, we are explicitly
    // out to test the behavior of multiple views and dms, so we need to
    // set up multiples.  We'll start out with four non-independent views,
    // to mimic the most common multi-dm/view display - a Quad view widget.
    // Each view will get its own attached swrast DM.
    //
    // TODO - properly populate BViewStates in DbiState...
    for (size_t i = 1; i < 4; i++) {
	struct bview *v;
	BU_GET(v, struct bview);
	if (!i)
	    gedp->ged_gvp = v;
	bv_init(v);
	bu_vls_sprintf(&v->gv_name, "V%zd", i);
	bu_ptbl_ins(&gedp->ged_views, (long *)v);
	bu_ptbl_ins(&gedp->ged_free_views, (long *)v);

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
	ged_exec_dm(gedp, 6, s_av);
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
	v->gv_base2local = gedp->dbip->dbi_base2local;
	v->gv_local2base = gedp->dbip->dbi_local2base;
    }

    /* Set distinct view az/el for each of the four quad views */
    s_av[0] = "view";
    s_av[1] = "-V";
    s_av[2] = "V0";
    s_av[3] = "aet";
    s_av[4] = "35";
    s_av[5] = "25";
    s_av[6] = "0";
    s_av[7] = NULL;
    ged_exec_view(gedp, 7, s_av);

    s_av[2] = "V1";
    s_av[4] = "90";
    s_av[5] = "0";
    s_av[6] = "0";
    ged_exec_view(gedp, 7, s_av);

    s_av[2] = "V2";
    s_av[4] = "0";
    s_av[5] = "90";
    s_av[6] = "0";
    ged_exec_view(gedp, 7, s_av);

    s_av[2] = "V3";
    s_av[4] = "0";
    s_av[5] = "0";
    s_av[6] = "90";
    ged_exec_view(gedp, 7, s_av);


    /************************************/
    /* Set up a basic wireframe */
    bu_log("Basic shared view drawing test...\n");
    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 4, s_av);

    ret += img_cmp(0, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], true, soft_fail);

    /* Make sure "Z" clears everything */
    s_av[0] = "Z";
    ged_exec_Z(gedp, 1, s_av);

    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    /************************************/
    /* Check behavior of a view element */
    bu_log("View object, shared views drawing test...\n");
    poly_circ(gedp, 1, 0);
    ret += img_cmp(0, 2, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 2, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 2, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 2, gedp, av[1], true, soft_fail);

    /* Make sure "Z" clears everything */
    s_av[0] = "Z";
    ged_exec_Z(gedp, 1, s_av);

    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    /***************************************************/
    /* Check view independent behavior - basic drawing
     *
     * TODO - rework for new scheme Link/UnLink */
    bu_log("Basic independent views drawing test - V1 active\n");

    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	//struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	//v->independent = 1;
    }

    s_av[0] = "draw";
    s_av[1] = "-V";
    s_av[2] = "V1";
    s_av[3] = "-m0";
    s_av[4] = "all.g";
    s_av[5] = NULL;
    ged_exec_draw(gedp, 5, s_av);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    bu_log("Basic independent views drawing test - V0, V1 active\n");
    s_av[0] = "draw";
    s_av[1] = "-V";
    s_av[2] = "V0";
    s_av[3] = "-m0";
    s_av[4] = "all.g";
    s_av[5] = NULL;
    ged_exec_draw(gedp, 5, s_av);

    ret += img_cmp(0, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    bu_log("Basic independent views drawing test - V0, V1, V3 active\n");
    s_av[0] = "draw";
    s_av[1] = "-V";
    s_av[2] = "V3";
    s_av[3] = "-m0";
    s_av[4] = "all.g";
    s_av[5] = NULL;
    ged_exec_draw(gedp, 5, s_av);

    ret += img_cmp(0, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], false, soft_fail);

    bu_log("Basic independent views drawing test - all active\n");
    s_av[0] = "draw";
    s_av[1] = "-V";
    s_av[2] = "V2";
    s_av[3] = "-m0";
    s_av[4] = "all.g";
    s_av[5] = NULL;
    ged_exec_draw(gedp, 5, s_av);

    ret += img_cmp(0, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], false, soft_fail);

    /* Make sure "Z" clears in the expected way */
    bu_log("Independent views - clear.\n");
    s_av[0] = "Z";
    s_av[1] = "-V";
    s_av[2] = "V0";
    s_av[3] = NULL;
    ged_exec_Z(gedp, 3, s_av);

    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], false, soft_fail);

    s_av[0] = "Z";
    s_av[1] = "-V";
    s_av[2] = "V2";
    s_av[3] = NULL;
    ged_exec_Z(gedp, 3, s_av);

    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], false, soft_fail);


    s_av[0] = "Z";
    s_av[1] = "-V";
    s_av[2] = "V3";
    s_av[3] = NULL;
    ged_exec_Z(gedp, 3, s_av);

    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    s_av[0] = "Z";
    s_av[1] = "-V";
    s_av[2] = "V1";
    s_av[3] = NULL;
    ged_exec_Z(gedp, 3, s_av);

    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    /**************************************************/
    /* Check view independent behavior - view element
     *
     * TODO - will need to redo this for new scheme - 
     * BViewState containers will have their
     * own local objects in addition to (potentially)
     * linking to a shared view - link/unlink and per
     * view paths/objects are what to test here... */
    bu_log("Independent views - view object drawing test. V2\n");
    poly_circ(gedp, 2, 1);
    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    bu_log("Independent views - view object drawing test. V3\n");
    poly_circ(gedp, 3, 1);
    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 3, gedp, av[1], false, soft_fail);

    bu_log("Independent views - view object drawing test. V0\n");
    poly_circ(gedp, 0, 1);
    ret += img_cmp(0, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 3, gedp, av[1], false, soft_fail);

    bu_log("Independent views - view object drawing test. V1\n");
    poly_circ(gedp, 1, 1);
    ret += img_cmp(0, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 3, gedp, av[1], false, soft_fail);


    /* Make sure "Z" clears in the expected way */
    bu_log("Independent views - view obj clearing - default\n");
    s_av[0] = "Z";
    s_av[1] = "-V";
    s_av[2] = "V0";
    s_av[3] = NULL;
    ged_exec_Z(gedp, 3, s_av);

    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 3, gedp, av[1], false, soft_fail);

    bu_log("Independent views - view obj clearing - V2\n");
    s_av[0] = "Z";
    s_av[1] = "-V";
    s_av[2] = "V2";
    s_av[3] = NULL;
    ged_exec_Z(gedp, 3, s_av);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 3, gedp, av[1], false, soft_fail);


    bu_log("Independent views - view obj clearing - V3\n");
    s_av[0] = "Z";
    s_av[1] = "-V";
    s_av[2] = "V3";
    s_av[3] = NULL;
    ged_exec_Z(gedp, 3, s_av);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 3, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    bu_log("Independent views - view obj clearing - V1\n");
    s_av[0] = "Z";
    s_av[1] = "-V";
    s_av[2] = "V1";
    s_av[3] = NULL;
    ged_exec_Z(gedp, 3, s_av);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    // Scrub all data out of the views, switch back to non-independent
    scene_clear(gedp, 0);
    scene_clear(gedp, 1);
    scene_clear(gedp, 2);
    scene_clear(gedp, 3);

#if 0
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	v->independent = 0;
    }
    scene_clear(gedp, -1);
#endif

    /***************************************************/
    /* Check shared view behavior - non-local line drawing */
    bu_log("Shared views drawing test - non-local view line\n");

    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 4, s_av);

    vline(gedp, 1, -200, -200, -200, 200, 200, 200);
    ret += img_cmp(0, 4, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 4, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 4, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 4, gedp, av[1], false, soft_fail);

    /* Make sure we've cleared everything */
    s_av[0] = "Z";
    ged_exec_Z(gedp, 1, s_av);

    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    /***************************************************/
    /* Check shared view behavior - local line drawing.
     * This combines shared and non-shared elements,
     * the most complex of the structural scenarios. */
    bu_log("Shared views drawing test - local view line\n");

    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 4, s_av);

    l_line(gedp, 0, 0, -200, -100, -100, 200, 100, 100);
    ret += img_cmp(0, 5, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], false, soft_fail);


    l_line(gedp, 2, 2, -50, -50, -30, 100, -70, 80);
    ret += img_cmp(0, 5, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 5, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], false, soft_fail);


    l_line(gedp, 1, 1, 50, -20, 10, 130, 70, -80);
    ret += img_cmp(0, 5, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 5, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 5, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], false, soft_fail);


    l_line(gedp, 3, 3, 0, 0, 0, 100, 100, 100);
    ret += img_cmp(0, 5, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 5, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 5, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 5, gedp, av[1], false, soft_fail);


    // Add a shared line to all four views
    vline(gedp, 4, 0, 0, 0, 10, 10, -100);
    // Turn the shared line green
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "l4";
    s_av[3] = "color";
    s_av[4] = "0/255/0";
    s_av[5] = NULL;
    ged_exec_view(gedp, 5, s_av);
    ret += img_cmp(0, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 6, gedp, av[1], false, soft_fail);

    // Scrub view specific data
    bu_log("Clearing view-specific data only...\n");
    scene_clear(gedp, 0);
    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, 7, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 6, gedp, av[1], false, soft_fail);

    scene_clear(gedp, 1);
    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, 7, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 7, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 6, gedp, av[1], false, soft_fail);


    scene_clear(gedp, 2);
    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, 7, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 7, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 7, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 6, gedp, av[1], false, soft_fail);


    scene_clear(gedp, 3);
    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, 7, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 7, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 7, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 7, gedp, av[1], false, soft_fail);


    scene_clear(gedp, -1);
    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    // Reset the stage - clearing only shared
    bu_log("Restore drawn data for shared-only clearing test...\n");
    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 4, s_av);
    l_line(gedp, 0, 0, -200, -100, -100, 200, 100, 100);
    l_line(gedp, 2, 2, -50, -50, -30, 100, -70, 80);
    l_line(gedp, 1, 1, 50, -20, 10, 130, 70, -80);
    l_line(gedp, 3, 3, 0, 0, 0, 100, 100, 100);
    vline(gedp, 4, 0, 0, 0, 10, 10, -100);
    // Turn the shared line green
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "l4";
    s_av[3] = "color";
    s_av[4] = "0/255/0";
    s_av[5] = NULL;
    ged_exec_view(gedp, 5, s_av);

    ret += img_cmp(0, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 6, gedp, av[1], false, soft_fail);

    s_av[0] = "Z";
    s_av[1] = "-S";
    s_av[2] = NULL;
    ged_exec_Z(gedp, 2, s_av);

    ret += img_cmp(0, 8, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 8, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 8, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 8, gedp, av[1], false, soft_fail);

    scene_clear(gedp, 0);
    scene_clear(gedp, 1);
    scene_clear(gedp, 2);
    scene_clear(gedp, 3);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);


    // Reset the stage - clearing everything test
    bu_log("Restore drawn data for clear all test...\n");
    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 4, s_av);
    l_line(gedp, 0, 0, -200, -100, -100, 200, 100, 100);
    l_line(gedp, 2, 2, -50, -50, -30, 100, -70, 80);
    l_line(gedp, 1, 1, 50, -20, 10, 130, 70, -80);
    l_line(gedp, 3, 3, 0, 0, 0, 100, 100, 100);
    vline(gedp, 4, 0, 0, 0, 10, 10, -100);
    // Turn the shared line green
    s_av[0] = "view";
    s_av[1] = "obj";
    s_av[2] = "l4";
    s_av[3] = "color";
    s_av[4] = "0/255/0";
    s_av[5] = NULL;
    ged_exec_view(gedp, 5, s_av);

    ret += img_cmp(0, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, 6, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 6, gedp, av[1], false, soft_fail);

    bu_log("Clearing everything...\n");
    scene_clear(gedp, 0);
    scene_clear(gedp, 1);
    scene_clear(gedp, 2);
    scene_clear(gedp, 3);
    scene_clear(gedp, -1);
    for (int i = 0; i < 4; i++)
	dm_refresh(gedp, i);
    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    // Next, test a mix of shared and independent views
    bu_log("Testing mixed shared and independent views\n");
#if 0
    ((struct bview *)BU_PTBL_GET(views, 0))->independent = 1;
    ((struct bview *)BU_PTBL_GET(views, 1))->independent = 0;
    ((struct bview *)BU_PTBL_GET(views, 2))->independent = 1;
    ((struct bview *)BU_PTBL_GET(views, 3))->independent = 0;
#endif

    // First, draw without specifying any particular view.
    // This should result in the non-independent views being
    // populated.
    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec_draw(gedp, 3, s_av);

    ret += img_cmp(0, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], false, soft_fail);

    // Populate an independent view
    s_av[0] = "draw";
    s_av[1] = "-V";
    s_av[2] = "V0";
    s_av[3] = "-m0";
    s_av[4] = "all.g";
    s_av[5] = NULL;
    ged_exec_draw(gedp, 5, s_av);

    ret += img_cmp(0, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, 1, gedp, av[1], false, soft_fail);

    // Clear shared views
    s_av[0] = "Z";
    s_av[1] = NULL;
    ged_exec_Z(gedp, 1, s_av);

    ret += img_cmp(0, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    // Draw specifying a view, but this time specifying a shared view.
    // Should be a no-op - we can mix shared and independent view
    // objects, but db objects are either independent or shared.
    // TODO - should we support mixing them?  Would complicate the
    // who, B and Z command usage somewhat - right now, for geometry
    // objects, those are consistent across shared views.
    s_av[0] = "draw";
    s_av[1] = "-V";
    s_av[2] = "V1";
    s_av[3] = "-m0";
    s_av[4] = "all.g";
    s_av[5] = NULL;
    ged_exec_draw(gedp, 5, s_av);

    ret += img_cmp(0, 1, gedp, av[1], false, soft_fail);
    ret += img_cmp(1, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(2, -1, gedp, av[1], false, soft_fail);
    ret += img_cmp(3, -1, gedp, av[1], false, soft_fail);

    //bu_setenv("BV_LOG", "1", 1);

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

