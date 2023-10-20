/*                         A E T . C P P
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
/** @file aet.cpp
 *
 * Attempts to test the stability of views across various operations
 * such as resizing.
 *
 */

#include "common.h"

#include <stdio.h>
#include <fstream>
#include <bu.h>
#include <bg/lod.h>
#include <icv.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>

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
    dm_draw_objs(v, NULL, NULL);
    dm_draw_end(dmp);
}

void
img_cmp(int vnum, int id, struct ged *gedp, const char *cdir, int soft_fail)
{
    icv_image_t *ctrl, *timg;
    struct bu_vls tname = BU_VLS_INIT_ZERO;
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    if (id <= 0) {
	bu_vls_sprintf(&tname, "aet_clear.png");
	bu_vls_sprintf(&cname, "%s/empty.png", cdir);
    } else {
	bu_vls_sprintf(&tname, "aet_%02d_%03d.png", vnum, id);
	bu_vls_sprintf(&cname, "%s/aet_%02d_%03d_ctrl.png", cdir, vnum, id);
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
	bu_vls_free(&tname);
	return;
    }

    timg = icv_read(bu_vls_cstr(&tname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!timg) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&tname));
	    bu_vls_free(&tname);
	    return;
	}
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&tname));
    }
    ctrl = icv_read(bu_vls_cstr(&cname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!ctrl) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&cname));
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
	    return;
	}
	bu_exit(EXIT_FAILURE, "%d wireframe diff failed.  %d matching, %d off by 1, %d off by many\n", id, matching_cnt, off_by_1_cnt, off_by_many_cnt);
    }

    icv_destroy(ctrl);
    icv_destroy(timg);

    // Image comparison done and successful
    bu_file_delete(bu_vls_cstr(&tname));
    bu_vls_free(&tname);
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
    (void)bu_opt_parse(NULL, ac, (const char **)av, d);

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

    /* make a temporary copy of moss */
    bu_vls_sprintf(&fname, "%s/moss.g", av[1]);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmpg("moss_aet_tmp.g", std::ios::binary);
    tmpg << orig.rdbuf();
    orig.close();
    tmpg.close();

    /* Open the temp file */
    const char *s_av[15] = {NULL};
    dbp = ged_open("db", "moss_aet_tmp.g", 1);
    // We don't want the default GED views for this test
    bv_set_rm_view(&dbp->ged_views, NULL);

    // Set up the views.  Unlike the other drawing tests, we are explicitly
    // out to test the behavior of multiple views and dms, so we need to
    // set up multiples.  We'll start out with four non-independent views,
    // to mimic the most common multi-dm/view display - a Quad view widget.
    // Each view will get its own attached swrast DM.
    struct bview *views[4];
    for (size_t i = 0; i < 4; i++) {
	BU_GET(views[i], struct bview);
	if (!i)
	    dbp->ged_gvp = views[i];
	struct bview *v = views[i];
	bv_init(v, &dbp->ged_views);
	bu_vls_sprintf(&v->gv_name, "V%zd", i);
	bv_set_add_view(&dbp->ged_views, v);
	bu_ptbl_ins(&dbp->ged_free_views, (long *)v);

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

    /* Set distinct view az/el for each of the four quad views.  For
     * this test we are deliberately testing view settings that have
     * the potential to be challenging in "gimbal lock" positions in
     * multiples of 90 degrees and using non-zero twist components. */
    VSET(views[0]->gv_aet, 0, 0, 90);
    bv_mat_aet(views[0]);
    bv_update(views[0]);

    VSET(views[1]->gv_aet, 90, 90, 180);
    bv_mat_aet(views[1]);
    bv_update(views[1]);

    VSET(views[2]->gv_aet, -90, 270, -90);
    bv_mat_aet(views[2]);
    bv_update(views[2]);

    VSET(views[3]->gv_aet, 270, -180, 90);
    bv_mat_aet(views[3]);
    bv_update(views[3]);


    /************************************************************************/
    /* Draw a wireframe.  Because we're doing tests of view and dm
     * manipulation, this is the only draw command needed for this particular
     * setup - we just need some non-empty geometry displayed. */
    bu_log("Basic drawing test...\n");
    s_av[0] = "draw";
    s_av[1] = "-m0";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    ged_exec(dbp, 4, s_av);

    // Sanity
    img_cmp(0, 1, dbp, av[1], soft_fail);
    img_cmp(1, 1, dbp, av[1], soft_fail);
    img_cmp(2, 1, dbp, av[1], soft_fail);
    img_cmp(3, 1, dbp, av[1], soft_fail);

    // Resize dm to larger dimensions
    bu_log("Resize to 600x600...\n");
    int len = 600;
    for (size_t i = 0; i < 4; i++) {
	struct dm *dmp = (struct dm *)views[i]->dmp;
	dm_set_width(dmp, len);
	dm_set_height(dmp, len);
	views[i]->gv_width = len;
	views[i]->gv_height = len;
	dm_configure_win(dmp, 0);
	// NOTE:  deliberately not resetting aet here - we want to see if it is
	// stable without adjustment.
	bv_update(views[i]);
    }
    img_cmp(0, 2, dbp, av[1], soft_fail);
    img_cmp(1, 2, dbp, av[1], soft_fail);
    img_cmp(2, 2, dbp, av[1], soft_fail);
    img_cmp(3, 2, dbp, av[1], soft_fail);

    // Shrink back to default dimensions
    bu_log("Shrink to 512x512...\n");
    len = 512;
    for (size_t i = 0; i < 4; i++) {
	struct dm *dmp = (struct dm *)views[i]->dmp;
	dm_set_width(dmp, len);
	dm_set_height(dmp, len);
	views[i]->gv_width = len;
	views[i]->gv_height = len;
	dm_configure_win(dmp, 0);
	// NOTE:  deliberately not resetting aet here - we want to see if it is
	// stable without adjustment.
	bv_update(views[i]);
    }
    img_cmp(0, 1, dbp, av[1], soft_fail);
    img_cmp(1, 1, dbp, av[1], soft_fail);
    img_cmp(2, 1, dbp, av[1], soft_fail);
    img_cmp(3, 1, dbp, av[1], soft_fail);

    // Cycle through a bunch of resizes
    bu_log("Cycle through multiple resizes...\n");
    for (int i = 513; i < 600; i++) {
	for (size_t j = 0; j < 4; j++) {
	    struct dm *dmp = (struct dm *)views[j]->dmp;
	    dm_set_width(dmp, i);
	    dm_set_height(dmp, i);
	    views[j]->gv_width = i;
	    views[j]->gv_height = i;
	    dm_configure_win(dmp, 0);
	    // NOTE:  deliberately not resetting aet here - we want to see if it is
	    // stable without adjustment.
	    bv_update(views[j]);
	}
    }
    len = 512;
    for (size_t i = 0; i < 4; i++) {
	struct dm *dmp = (struct dm *)views[i]->dmp;
	dm_set_width(dmp, len);
	dm_set_height(dmp, len);
	views[i]->gv_width = len;
	views[i]->gv_height = len;
	dm_configure_win(dmp, 0);
	// NOTE:  deliberately not resetting aet here - we want to see if it is
	// stable without adjustment.
	bv_update(views[i]);
    }
    img_cmp(0, 1, dbp, av[1], soft_fail);
    img_cmp(1, 1, dbp, av[1], soft_fail);
    img_cmp(2, 1, dbp, av[1], soft_fail);
    img_cmp(3, 1, dbp, av[1], soft_fail);



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

