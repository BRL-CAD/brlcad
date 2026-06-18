/*                    C L A S S I C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file classic.cpp
 *
 * Headless rendering tests for the classic display-list drawing path used by
 * MGED.  This deliberately avoids MGED/Tcl setup: libged builds the classic
 * ged_dl() display list, and libdm renders that list with dm_draw_head_dl().
 */

#include "common.h"

#include <fstream>
#include <inttypes.h>

#include <bu.h>
#include <icv.h>
#define DM_WITH_RT
#include <dm.h>
#include <dm/view.h>
#include <ged.h>

struct classic_dlist_ctx {
    struct dm *dmp;
};

static void
classic_create_dlist_solid(void *data, struct bv_scene_obj *sp)
{
    struct classic_dlist_ctx *ctx = (struct classic_dlist_ctx *)data;
    struct dm *dmp = ctx ? ctx->dmp : NULL;
    if (!dmp || !sp || !dm_get_displaylist(dmp))
	return;

    if (sp->s_dlist == 0)
	sp->s_dlist = dm_gen_dlists(dmp, 1);

    dm_set_dirty(dmp, 1);
    (void)dm_make_current(dmp);
    (void)dm_begin_dlist(dmp, sp->s_dlist);
    if (sp->s_iflag == UP) {
	(void)dm_set_fg(dmp, 255, 255, 255, 0, sp->s_os->transparency);
    } else {
	(void)dm_set_fg(dmp,
		(unsigned char)sp->s_color[0],
		(unsigned char)sp->s_color[1],
		(unsigned char)sp->s_color[2],
		0, sp->s_os->transparency);
    }
    (void)dm_draw_vlist(dmp, (struct bv_vlist *)&sp->s_vlist);
    (void)dm_end_dlist(dmp);
}

static void
classic_create_dlist_all(void *data, struct display_list *gdlp)
{
    struct bv_scene_obj *sp;
    for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	classic_create_dlist_solid(data, sp);
    }
}

static void
classic_destroy_dlist(void *data, unsigned int dlist, int range)
{
    struct classic_dlist_ctx *ctx = (struct classic_dlist_ctx *)data;
    if (ctx && ctx->dmp && dlist)
	(void)dm_free_dlists(ctx->dmp, dlist, range);
}

static void
classic_refresh(struct ged *gedp)
{
    struct bview *v = gedp->ged_gvp;
    struct dm *dmp = (struct dm *)v->dmp;
    unsigned char geometry_default_color[] = {255, 0, 0};

    dm_make_current(dmp);
    dm_configure_win(dmp, 0);
    dm_draw_begin(dmp);
    dm_loadmatrix(dmp, v->gv_model2view, 0);

    int use_dlist = dm_get_displaylist(dmp);
    (void)dm_draw_head_dl(dmp, (struct bu_list *)ged_dl(gedp), 1.0, v->gv_isize,
	    -1, -1, -1, 1, use_dlist, 0, geometry_default_color, 1, use_dlist);

    (void)dm_hud_begin(dmp);
    dm_draw_faceplate(v);
    (void)dm_hud_end(dmp);

    dm_draw_end(dmp);
    dm_set_dirty(dmp, 0);
}

static void
classic_clear(struct ged *gedp)
{
    const char *zap_av[1] = {"Z"};
    ged_exec_Z(gedp, 1, zap_av);
    classic_refresh(gedp);
}

static int
classic_img_cmp(int id, struct ged *gedp, const char *cdir, bool clear_scene, bool clear_image, int soft_fail, int approximate_check = 0)
{
    struct bu_vls tname = BU_VLS_INIT_ZERO;
    struct bu_vls cname = BU_VLS_INIT_ZERO;

    if (id <= 0) {
	bu_vls_sprintf(&tname, "classic_clear.png");
	bu_vls_sprintf(&cname, "%s/empty.png", cdir);
    } else {
	bu_vls_sprintf(&tname, "classic%03d.png", id);
	bu_vls_sprintf(&cname, "%s/classic%03d_ctrl.png", cdir, id);
    }

    classic_refresh(gedp);
    const char *sg_av[2] = {"screengrab", bu_vls_cstr(&tname)};
    ged_exec_screengrab(gedp, 2, sg_av);

    icv_image_t *timg = icv_read(bu_vls_cstr(&tname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!timg) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&tname));
	    if (clear_scene)
		classic_clear(gedp);
	    bu_vls_free(&tname);
	    bu_vls_free(&cname);
	    return BRLCAD_ERROR;
	}
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&tname));
    }

    icv_image_t *ctrl = icv_read(bu_vls_cstr(&cname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!ctrl) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&cname));
	    if (clear_scene)
		classic_clear(gedp);
	    icv_destroy(timg);
	    bu_vls_free(&tname);
	    bu_vls_free(&cname);
	    return BRLCAD_ERROR;
	}
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&cname));
    }

    int matching_cnt = 0;
    int off_by_1_cnt = 0;
    int off_by_many_cnt = 0;
    int ret = icv_diff(&matching_cnt, &off_by_1_cnt, &off_by_many_cnt, ctrl, timg);
    if (ret) {
	uint32_t pret = icv_pdiff(ctrl, timg);
	if (approximate_check) {
	    if (!off_by_many_cnt || pret < (uint32_t)approximate_check)
		ret = 0;
	}
    }

    if (ret) {
	uint32_t pret = icv_pdiff(ctrl, timg);
	bu_log("%d classic diff failed.  %d matching, %d off by 1, %d off by many\n",
		id, matching_cnt, off_by_1_cnt, off_by_many_cnt);
	bu_log("icv_pdiff Hamming distance(%d): %" PRIu32 "\n", id, pret);

	icv_image_t *diff_img = icv_diffimg(ctrl, timg);
	if (diff_img) {
	    struct bu_vls diff_name = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&diff_name, "classic_%d_diff.png", id);
	    icv_write(diff_img, bu_vls_cstr(&diff_name), BU_MIME_IMAGE_PNG);
	    bu_vls_free(&diff_name);
	    icv_destroy(diff_img);
	}

	if (!soft_fail)
	    bu_exit(EXIT_FAILURE, "Diff failure found, test aborted.\n");
    }

    if (clear_image)
	bu_file_delete(bu_vls_cstr(&tname));
    if (clear_scene)
	classic_clear(gedp);

    bu_vls_free(&tname);
    bu_vls_free(&cname);
    icv_destroy(ctrl);
    icv_destroy(timg);

    return ret ? BRLCAD_ERROR : BRLCAD_OK;
}

static void
classic_view_moss(struct ged *gedp)
{
    const char *s_av[16] = {NULL};

    s_av[0] = "autoview";
    ged_exec_autoview(gedp, 1, s_av);

    s_av[0] = "ae";
    s_av[1] = "35";
    s_av[2] = "25";
    ged_exec_ae(gedp, 3, s_av);
}

static int
classic_exec(struct ged *gedp, int ac, const char *av[])
{
    int ret = BRLCAD_OK;

    if (BU_STR_EQUAL(av[0], "adjust"))
	ret = ged_exec_adjust(gedp, ac, av);
    else if (BU_STR_EQUAL(av[0], "autoview"))
	ret = ged_exec_autoview(gedp, ac, av);
    else if (BU_STR_EQUAL(av[0], "draw"))
	ret = ged_exec_draw(gedp, ac, av);
    else if (BU_STR_EQUAL(av[0], "erase"))
	ret = ged_exec_erase(gedp, ac, av);
    else if (BU_STR_EQUAL(av[0], "facetize"))
	ret = ged_exec_facetize(gedp, ac, av);
    else if (BU_STR_EQUAL(av[0], "make"))
	ret = ged_exec_make(gedp, ac, av);
    else if (BU_STR_EQUAL(av[0], "redraw"))
	ret = ged_exec_redraw(gedp, ac, av);
    else if (BU_STR_EQUAL(av[0], "view"))
	ret = ged_exec_view(gedp, ac, av);
    else
	bu_exit(EXIT_FAILURE, "unhandled test command %s\n", av[0]);

    if (ret != BRLCAD_OK) {
	bu_exit(EXIT_FAILURE, "%s failed: %s\n", av[0], bu_vls_cstr(gedp->ged_result_str));
    }

    return ret;
}

int
main(int ac, char *av[])
{
    int need_help = 0;
    int soft_fail = 0;
    int keep_images = 0;
    int ret = BRLCAD_OK;

    bu_setprogname(av[0]);

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",     "", NULL, &need_help,  "Print help and exit");
    BU_OPT(d[1], "c", "continue", "", NULL, &soft_fail,  "Continue testing if a failure is encountered.");
    BU_OPT(d[2], "k", "keep",     "", NULL, &keep_images, "Keep images generated by the run.");
    BU_OPT_NULL(d[3]);

    int uac = bu_opt_parse(NULL, ac, (const char **)av, d);
    if (uac != 2 || need_help)
	bu_exit(EXIT_FAILURE, "%s [-h] [-c] [-k] <directory>", av[0]);

    const char *ctrl_dir = av[1];
    if (!bu_file_directory(ctrl_dir)) {
	printf("ERROR: [%s] is not a directory. Expecting control image directory\n", ctrl_dir);
	return 2;
    }

    char lcache[MAXPATHLEN] = {0};
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CURR, "ged_classic_draw_test_cache", NULL);
    bu_mkdir(lcache);
    bu_setenv("BU_DIR_CACHE", lcache, 1);
    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);
    bu_setenv("DM_SWRAST", "1", 1);

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", ctrl_dir);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    std::ofstream tmpg("moss_classic_tmp.g", std::ios::binary);
    tmpg << orig.rdbuf();
    orig.close();
    tmpg.close();
    bu_vls_free(&fname);

    struct ged *gedp = ged_open("db", "moss_classic_tmp.g", 1);
    if (!gedp)
	bu_exit(EXIT_FAILURE, "failed to open moss_classic_tmp.g\n");

    const char *s_av[5] = {NULL};
    s_av[0] = "dm";
    s_av[1] = "attach";
    s_av[2] = "swrast";
    s_av[3] = "SW";
    ged_exec_dm(gedp, 4, s_av);

    struct bview *v = gedp->ged_gvp;
    struct dm *dmp = (struct dm *)v->dmp;
    if (!dmp)
	bu_exit(EXIT_FAILURE, "failed to attach swrast dm\n");

    struct classic_dlist_ctx dlist_ctx;
    dlist_ctx.dmp = dmp;
    gedp->vlist_ctx = (void *)&dlist_ctx;
    gedp->ged_create_vlist_scene_obj_callback = classic_create_dlist_solid;
    gedp->ged_create_vlist_display_list_callback = classic_create_dlist_all;
    gedp->ged_destroy_vlist_callback = classic_destroy_dlist;

    dm_set_width(dmp, 512);
    dm_set_height(dmp, 512);
    dm_configure_win(dmp, 0);
    dm_set_zbuffer(dmp, 1);
    fastf_t windowbounds[6] = {-1, 1, -1, 1, -100, 100};
    dm_set_win_bounds(dmp, windowbounds);
    v->gv_width = dm_get_width(dmp);
    v->gv_height = dm_get_height(dmp);
    dm_set_vp(dmp, &v->gv_scale);
    v->gv_base2local = gedp->dbip->dbi_base2local;
    v->gv_local2base = gedp->dbip->dbi_local2base;

    s_av[0] = "dm";
    s_av[1] = "set";
    s_av[2] = "fast_wireframe";
    s_av[3] = "0";
    ged_exec_dm(gedp, 4, s_av);

    bu_log("Testing classic display-list wireframe draw...\n");
    s_av[0] = "draw";
    s_av[1] = "all.g";
    s_av[2] = NULL;
    classic_exec(gedp, 2, s_av);
    classic_view_moss(gedp);

    ret += classic_img_cmp(1, gedp, ctrl_dir, true, !keep_images, soft_fail);
    ret += classic_img_cmp(0, gedp, ctrl_dir, false, !keep_images, soft_fail);

    bu_log("Testing classic color override draw...\n");
    s_av[0] = "draw";
    s_av[1] = "-C0/255/255";
    s_av[2] = "all.g";
    s_av[3] = NULL;
    classic_exec(gedp, 3, s_av);
    classic_view_moss(gedp);
    ret += classic_img_cmp(2, gedp, ctrl_dir, true, !keep_images, soft_fail);

    bu_log("Testing classic erase...\n");
    s_av[0] = "draw";
    s_av[1] = "all.g";
    s_av[2] = NULL;
    classic_exec(gedp, 2, s_av);
    classic_view_moss(gedp);
    s_av[0] = "erase";
    s_av[1] = "all.g";
    s_av[2] = NULL;
    classic_exec(gedp, 2, s_av);
    ret += classic_img_cmp(0, gedp, ctrl_dir, false, !keep_images, soft_fail);

    bu_log("Testing classic redraw after primitive edit...\n");
    s_av[0] = "make";
    s_av[1] = "-o";
    s_av[2] = "0 0 0";
    s_av[3] = "-s";
    s_av[4] = "400";
    s_av[5] = "classic_edit.s";
    s_av[6] = "sph";
    s_av[7] = NULL;
    classic_exec(gedp, 7, s_av);

    s_av[0] = "draw";
    s_av[1] = "-C255/255/0";
    s_av[2] = "classic_edit.s";
    s_av[3] = NULL;
    classic_exec(gedp, 3, s_av);
    s_av[0] = "autoview";
    s_av[1] = NULL;
    classic_exec(gedp, 1, s_av);
    ret += classic_img_cmp(3, gedp, ctrl_dir, false, !keep_images, soft_fail);

    s_av[0] = "adjust";
    s_av[1] = "classic_edit.s";
    s_av[2] = "V";
    s_av[3] = "250 0 0";
    s_av[4] = "A";
    s_av[5] = "650 0 0";
    s_av[6] = "B";
    s_av[7] = "0 250 0";
    s_av[8] = "C";
    s_av[9] = "0 0 250";
    s_av[10] = NULL;
    classic_exec(gedp, 10, s_av);
    s_av[0] = "redraw";
    s_av[1] = NULL;
    classic_exec(gedp, 1, s_av);
    s_av[0] = "autoview";
    s_av[1] = NULL;
    classic_exec(gedp, 1, s_av);
    ret += classic_img_cmp(4, gedp, ctrl_dir, false, !keep_images, soft_fail);

    s_av[0] = "adjust";
    s_av[1] = "classic_edit.s";
    s_av[2] = "V";
    s_av[3] = "0 0 0";
    s_av[4] = "A";
    s_av[5] = "200 0 0";
    s_av[6] = "B";
    s_av[7] = "0 200 0";
    s_av[8] = "C";
    s_av[9] = "0 0 200";
    s_av[10] = NULL;
    classic_exec(gedp, 10, s_av);
    s_av[0] = "redraw";
    s_av[1] = NULL;
    classic_exec(gedp, 1, s_av);
    s_av[0] = "autoview";
    s_av[1] = NULL;
    classic_exec(gedp, 1, s_av);
    ret += classic_img_cmp(10, gedp, ctrl_dir, true, !keep_images, soft_fail);

    bu_log("Testing classic faceplate rendering...\n");
    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "center_dot";
    s_av[3] = "1";
    s_av[4] = NULL;
    classic_exec(gedp, 4, s_av);
    ret += classic_img_cmp(5, gedp, ctrl_dir, false, !keep_images, soft_fail);
    s_av[3] = "0";
    classic_exec(gedp, 4, s_av);
    ret += classic_img_cmp(0, gedp, ctrl_dir, false, !keep_images, soft_fail);

    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "grid";
    s_av[3] = "color";
    s_av[4] = "255/255/255";
    s_av[5] = NULL;
    classic_exec(gedp, 5, s_av);
    s_av[3] = "res_h";
    s_av[4] = "25";
    classic_exec(gedp, 5, s_av);
    s_av[3] = "res_v";
    s_av[4] = "25";
    classic_exec(gedp, 5, s_av);
    s_av[3] = "res_major_h";
    s_av[4] = "5";
    classic_exec(gedp, 5, s_av);
    s_av[3] = "res_major_v";
    s_av[4] = "5";
    classic_exec(gedp, 5, s_av);
    s_av[3] = "1";
    s_av[4] = NULL;
    classic_exec(gedp, 4, s_av);
    ret += classic_img_cmp(6, gedp, ctrl_dir, false, !keep_images, soft_fail);
    s_av[3] = "0";
    classic_exec(gedp, 4, s_av);
    ret += classic_img_cmp(0, gedp, ctrl_dir, false, !keep_images, soft_fail);

    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "scale";
    s_av[3] = "1";
    s_av[4] = NULL;
    classic_exec(gedp, 4, s_av);
    ret += classic_img_cmp(7, gedp, ctrl_dir, false, !keep_images, soft_fail);
    s_av[3] = "0";
    classic_exec(gedp, 4, s_av);

    s_av[0] = "view";
    s_av[1] = "faceplate";
    s_av[2] = "view_axes";
    s_av[3] = "1";
    s_av[4] = NULL;
    classic_exec(gedp, 4, s_av);
    ret += classic_img_cmp(8, gedp, ctrl_dir, true, !keep_images, soft_fail);
    s_av[3] = "0";
    classic_exec(gedp, 4, s_av);

    bu_log("Testing classic shaded modes...\n");
    s_av[0] = "facetize";
    s_av[1] = "-r";
    s_av[2] = "all.g";
    s_av[3] = "all.bot";
    s_av[4] = NULL;
    classic_exec(gedp, 4, s_av);

    s_av[0] = "draw";
    s_av[1] = "-m1";
    s_av[2] = "all.bot";
    s_av[3] = NULL;
    classic_exec(gedp, 3, s_av);
    classic_view_moss(gedp);
    ret += classic_img_cmp(9, gedp, ctrl_dir, true, !keep_images, soft_fail, 20);

    ged_close(gedp);
    bu_file_delete("moss_classic_tmp.g");

    return ret ? 1 : 0;
}
