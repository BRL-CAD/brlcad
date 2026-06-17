/*                       U T I L . C P P
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
/** @file util.cpp
 *
 * Utility testing for drawing routines
 *
 */

#include "common.h"

#include <stdio.h>
#include <inttypes.h>
#include <fstream>

#include <bu.h>
#include <bsg/lod.h>
#include <icv.h>
#define DM_WITH_RT
#include <dm.h>
#include <ged.h>
#include <ged/bsg_ged_draw.h>
#include <ged/event_txn.h>

// In order to handle changes to .g geometry contents, we need to defined
// callbacks for the librt hooks that will update the working data structures.
// In Qt we have libqtcad handle this, but as we are not using a QgModel we
// need to do it ourselves.
extern "C" void
ged_changed_callback(struct db_i *UNUSED(dbip), struct directory *dp, int mode, void *u_data)
{
    struct ged *gedp = (struct ged *)u_data;
    const char *name = (dp) ? dp->d_namep : NULL;

    // Need to invalidate any LoD caches associated with this dp
    if (dp && dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT && gedp->ged_lod) {
	unsigned long long key = bsg_mesh_lod_key_get(gedp->ged_lod, dp->d_namep);
	if (key) {
	    bsg_mesh_lod_clear_cache(gedp->ged_lod, key);
	    bsg_mesh_lod_key_put(gedp->ged_lod, dp->d_namep, 0);
	}
    }

    switch(mode) {
	case 0:
	    ged_event_notify_object_modified(gedp, name, 1, NULL);
	    break;
	case 1:
	    ged_event_notify_object_added(gedp, name, NULL);
	    break;
	case 2:
	    ged_event_notify_object_removed(gedp, name, NULL);
	    break;
	default:
	    bu_log("changed callback mode error: %d\n", mode);
    }
}

extern "C" void
dm_refresh(struct ged *gedp)
{
    struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
    struct bsg_view *v = NULL;
    if (views && BU_PTBL_LEN(views) > 0)
	v = (struct bsg_view *)BU_PTBL_GET(views, 0);
    if (!v)
	v = gedp->ged_gvp;
    if (!v)
	return;
    struct ged_draw_transaction txn =
	ged_draw_transaction_make(GED_DRAW_TXN_REDRAW, NULL);
    txn.view = v;
    ged_draw_apply_transaction(gedp, &txn, NULL);

    struct dm *dmp = (struct dm *)v->dmp;
    if (!dmp)
	return;
    dm_make_current(dmp);
    unsigned char *dm_bg1;
    unsigned char *dm_bg2;
    dm_get_bg(&dm_bg1, &dm_bg2, dmp);
    dm_set_bg(dmp, dm_bg1[0], dm_bg1[1], dm_bg1[2], dm_bg2[0], dm_bg2[1], dm_bg2[2]);
    dm_set_native_repaint_pending(dmp, 0);
    dm_draw_objs(v);
    dm_draw_end(dmp);
}

extern "C" void
scene_clear(struct ged *gedp)
{
    const char *s_av[1] = {"Z"};
    ged_exec_Z(gedp, 1, s_av);
    dm_refresh(gedp);
}

extern "C" int
img_cmp(int id, struct ged *gedp, const char *cdir, bool clear_scene, bool clear_image, int soft_fail, int approximate_check, const char *clear_root, const char *img_root)
{
    icv_image_t *ctrl, *timg;
    struct bu_vls tname = BU_VLS_INIT_ZERO;
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    if (id <= 0) {
	bu_vls_sprintf(&tname, "%s.png", clear_root);
	bu_vls_sprintf(&cname, "%s/empty.png", cdir);
    } else {
	bu_vls_sprintf(&tname, "%s%03d.png", img_root, id);
	bu_vls_sprintf(&cname, "%s/%s%03d_ctrl.png", cdir, img_root, id);
    }

    dm_refresh(gedp);
    const char *s_av[2] = {"screengrab", NULL};
    s_av[1] = bu_vls_cstr(&tname);
    ged_exec_screengrab(gedp, 2, s_av);

    timg = icv_read(bu_vls_cstr(&tname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!timg) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&tname));
	    if (clear_scene)
		scene_clear(gedp);
	    bu_vls_free(&tname);
	    return BRLCAD_ERROR;
	}
	bu_exit(EXIT_FAILURE, "failed to read %s\n", bu_vls_cstr(&tname));
    }
    ctrl = icv_read(bu_vls_cstr(&cname), BU_MIME_IMAGE_PNG, 0, 0);
    if (!ctrl) {
	if (soft_fail) {
	    bu_log("Failed to read %s\n", bu_vls_cstr(&cname));
	    if (clear_scene)
		scene_clear(gedp);
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
    int iret = icv_diff(&matching_cnt, &off_by_1_cnt, &off_by_many_cnt, ctrl, timg);
    if (iret) {
	if (approximate_check) {
	    // First, if we're allowing approximate and all we have are off by one errors,
	    // allow it.
	    if (!off_by_many_cnt) {
		bu_log("%d approximate matching enabled, no off by many - passing.  %d matching, %d off by 1\n", id, matching_cnt, off_by_1_cnt);
		iret = 0;
		// We don't need it as a pass/fail, but for information report the
		// Hamming distance on the approximate match
		uint32_t pret = icv_pdiff(ctrl, timg);
		bu_log("icv_pdiff Hamming distance(%d): %" PRIu32 "\n", id, pret);
	    } else {
		// We have off by many - do perceptual hashing difference calculation
		uint32_t pret = icv_pdiff(ctrl, timg);
		// The return is a Hamming distance .  The scale of possible
		// returns ranges from 0 (same) to ~500 (completely different)
		bu_log("icv_pdiff Hamming distance(%d): %" PRIu32 "\n", id, pret);
		if (pret < (uint32_t)approximate_check) {
		    iret = 0;
		}
	    }
	}

	if (iret) {

	    bu_log("%d %s diff failed.  %d matching, %d off by 1, %d off by many\n", id, img_root, matching_cnt, off_by_1_cnt, off_by_many_cnt);

	    // Generate a diff image for debugging work
	    icv_image_t *diff_img = icv_diffimg(ctrl, timg);
	    if (diff_img) {
		struct bu_vls diff_name = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&diff_name, "%s_%d_diff.png", img_root, id);
		icv_write(diff_img, bu_vls_cstr(&diff_name), BU_MIME_IMAGE_PNG);
		bu_vls_free(&diff_name);
	    }

	    // If we're in soft fail mode, we're not keeping the image unless
	    // the user requested it. In hard fail, we leave the last image for
	    // inspection.
	    if (soft_fail && clear_image)
		bu_file_delete(bu_vls_cstr(&tname));
#if 0
	    // Dump an ascii rendering of the difference image to the log.  In
	    // scenarios such as CI systems, where we don't have a way to
	    // readily inspect the difference images, this output can still
	    // give us at least some idea of what is going on as long as the
	    // user can get their terminal font small enough.  We deliberately
	    // don't do any resizing of the image - we want to leave diff
	    // pixels intact, so we accept the terminal aspect ratio distortion
	    // as a trade-off for pixel rendering accuracy.
	    if (diff_img) {
		struct icv_ascii_art_params iparams = ICV_ASCII_ART_PARAMS_DEFAULT;
		iparams.output_color = 1;
		char *aart = icv_ascii_art(diff_img, &iparams);
		bu_log("%s\n", aart);
	    }
#endif
	    // Done with images
	    bu_vls_free(&tname);
	    icv_destroy(ctrl);
	    icv_destroy(timg);
	    icv_destroy(diff_img);

	    // If we're in soft fail, return
	    if (soft_fail) {
		if (clear_scene)
		    scene_clear(gedp);
		return BRLCAD_ERROR;
	    }

	    // Hard fail
	    bu_exit(EXIT_FAILURE, "Diff failure found, test aborted.\n");

	}
    }

    // Clean up
    if (clear_image)
	bu_file_delete(bu_vls_cstr(&tname));
    bu_vls_free(&tname);
    icv_destroy(ctrl);
    icv_destroy(timg);

    // If we're supposed to clear the scene, do so now.
    if (clear_scene)
	scene_clear(gedp);

    return BRLCAD_OK;
}

extern "C" int
img_not_empty(int id, struct ged *gedp, const char *cdir, bool clear_scene, bool clear_image, int soft_fail, const char *UNUSED(clear_root), const char *img_root)
{
    icv_image_t *ctrl, *timg;
    struct bu_vls tname = BU_VLS_INIT_ZERO;
    struct bu_vls cname = BU_VLS_INIT_ZERO;

    bu_vls_sprintf(&tname, "%s%03d_semantic.png", img_root, id);
    bu_vls_sprintf(&cname, "%s/empty.png", cdir);

    dm_refresh(gedp);
    const char *s_av[2] = {"screengrab", NULL};
    s_av[1] = bu_vls_cstr(&tname);
    ged_exec_screengrab(gedp, 2, s_av);

    timg = icv_read(bu_vls_cstr(&tname), BU_MIME_IMAGE_PNG, 0, 0);
    ctrl = icv_read(bu_vls_cstr(&cname), BU_MIME_IMAGE_PNG, 0, 0);
    bu_vls_free(&cname);

    if (!timg || !ctrl) {
	if (timg) icv_destroy(timg);
	if (ctrl) icv_destroy(ctrl);
	if (clear_image)
	    bu_file_delete(bu_vls_cstr(&tname));
	bu_vls_free(&tname);
	if (clear_scene)
	    scene_clear(gedp);
	if (soft_fail)
	    return BRLCAD_ERROR;
	bu_exit(EXIT_FAILURE, "failed to read semantic image comparison inputs\n");
    }

    int matching_cnt = 0;
    int off_by_1_cnt = 0;
    int off_by_many_cnt = 0;
    int diff = icv_diff(&matching_cnt, &off_by_1_cnt, &off_by_many_cnt, ctrl, timg);
    icv_destroy(ctrl);
    icv_destroy(timg);

    if (clear_image)
	bu_file_delete(bu_vls_cstr(&tname));
    bu_vls_free(&tname);

    if (!diff) {
	bu_log("%d %s semantic non-empty check failed: image matches empty scene\n", id, img_root);
	if (clear_scene)
	    scene_clear(gedp);
	if (soft_fail)
	    return BRLCAD_ERROR;
	bu_exit(EXIT_FAILURE, "Semantic empty-image failure found, test aborted.\n");
    }

    bu_log("%d %s semantic non-empty check passed against empty scene.  %d empty pixels matching, %d nearly-empty pixels, %d visibly drawn pixels\n",
	    id, img_root, matching_cnt, off_by_1_cnt, off_by_many_cnt);

    if (clear_scene)
	scene_clear(gedp);

    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
