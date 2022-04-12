/*                         V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/view.c
 *
 * The view command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/vls.h"

#include "../ged_private.h"
#include "./ged_view.h"

int
_view_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}

int
_view_cmd_aet(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] aet [vals]";
    const char *purpose_string = "get/set azimuth/elevation/twist of view";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    return ged_aet_core(gd->gedp, argc, argv);
}

int
_view_cmd_center(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] center [vals]";
    const char *purpose_string = "get/set view center";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    return ged_center_core(gd->gedp, argc, argv);
}

int
_view_cmd_eye(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] eye [vals]";
    const char *purpose_string = "get/set view eye point";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    return ged_eye_core(gd->gedp, argc, argv);
}

int
_view_cmd_faceplate(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] faceplate [vals]";
    const char *purpose_string = "manage faceplate view elements";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    return ged_faceplate_core(gd->gedp, argc, argv);
}

/* When a view is "independent", it displays only those objects when have been
 * added to its individual scene storage - the shared objects common to all
 * views will not be drawn.  When shifting a view from shared to independent
 * its local storage is populated with copies of the shared objects to prevent
 * an abrupt change of displayed contents, but once this setup is complete
 * further draw or erase operations in shared views will no longer alter the
 * scene object lists in the independent view.  To modify the independent
 * view's scene, it must be specifically set as the current view in libged.
 * Note also that when a view ceases to be independent, it's local object set
 * is compared to the shared object set and any objects in both are removed
 * from the local set.  However, any object in the independent list that are
 * not present in the shared set will remain, since there is no way for the
 * library to know if the intent is to preserve or remove such objects from the
 * scene.  Removal, as the destructive option, is the responsibility of the
 * application.
 *
 * Note that views may have localized scene objects even when not independent,
 * but they must be defined as view objects rather than database objects. */
int
_view_cmd_independent(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] independent <view> [0|1]";
    const char *purpose_string = "make a view independent (1) or part of the default view set (0)";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    // We know we're the independent command - start processing args
    argc--; argv++;

    struct ged *gedp = gd->gedp;
    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "no view specified\n");
	return BRLCAD_ERROR;
    }

    struct bview *v = bv_set_find_view(&gedp->ged_views, argv[0]);
    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "view %s not found\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", v->independent);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[1], "1")) {
	v->independent = 1;
	// Initialize local containers with current shared grps
	struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS);
	if (!sg)
	    return BRLCAD_OK;
	for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	    struct bu_vls opath = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&opath, "%s", bu_vls_cstr(&cg->s_name));
	    const char *av[6];
	    av[0] = "draw";
	    av[1] = "-R";
	    av[2] = "-V";
	    av[3] = bu_vls_cstr(&v->gv_name);
	    av[4] = bu_vls_cstr(&opath);
	    av[5] = NULL;
	    ged_exec(gedp, 5, av);
	    bu_vls_free(&opath);
	}
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[1], "0")) {
	v->independent = 0;
	// Clear local containers
	struct bu_ptbl *sg = v->gv_objs.db_objs;
	if (sg) {
	    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		bv_obj_put(cg);
	    }
	    bu_ptbl_reset(sg);
	}
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid value supplied: %s (need 0 or 1)\n", argv[1]);
    return BRLCAD_ERROR;
}

int
_view_cmd_list(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] ";
    const char *purpose_string = "list available views";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct ged *gedp = gd->gedp;
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	if (v != gedp->ged_gvp) {
	    bu_vls_printf(gedp->ged_result_str, "  %s\n", bu_vls_cstr(&v->gv_name));
	} else {
	    bu_vls_printf(gedp->ged_result_str, "* %s\n", bu_vls_cstr(&v->gv_name));
	}
    }

    return BRLCAD_OK;
}

int
_view_cmd_lod(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] lod [subcommand] [vals]";
    const char *purpose_string = "manage Level of Detail drawing settings";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct ged *gedp = gd->gedp;
    struct bview *gvp;
    int print_help = 0;
    static const char *usage = "view lod enabled [0|1]\n"
			       "view lod redraw_on_zoom [0|1]>\n"
			       "view lod point_scale [factor]\n"
			       "view lod curve_scale [factor]\n"
			       "view lod bot_threshold [face_cnt]\n";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "h", "help",        "",  NULL,  &print_help,      "Print help");
    BU_OPT_NULL(d[1]);

    // We know we're the lod command - start processing args
    argc--; argv++;

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return BRLCAD_HELP;
    }

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return BRLCAD_ERROR;
    }

    gvp = gedp->ged_gvp;
    if (gvp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "no current view defined\n");
	return BRLCAD_ERROR;
    }

    /* Print current state if no args are supplied */
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "enabled: %d\n", gvp->gv_s->adaptive_plot);
	bu_vls_printf(gedp->ged_result_str, "redraw_on_zoom: %d\n", gvp->gv_s->redraw_on_zoom);
	bu_vls_printf(gedp->ged_result_str, "point_scale: %g\n", gvp->gv_s->point_scale);
	bu_vls_printf(gedp->ged_result_str, "curve_scale: %g\n", gvp->gv_s->curve_scale);
	bu_vls_printf(gedp->ged_result_str, "bot_threshold: %zd\n", gvp->gv_s->bot_threshold);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "enabled")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%d\n", gvp->gv_s->adaptive_plot);
	    return BRLCAD_OK;
	}
	if (bu_str_true(argv[1])) {
	    gvp->gv_s->adaptive_plot = 1;
	    return BRLCAD_OK;
	}
	if (bu_str_false(argv[1])) {
	    gvp->gv_s->adaptive_plot = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "unknown argument to enabled: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "redraw_on_zoom")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%d\n", gvp->gv_s->redraw_on_zoom);
	    return BRLCAD_OK;
	}
	if (bu_str_true(argv[1])) {
	    gvp->gv_s->redraw_on_zoom = 1;
	    return BRLCAD_OK;
	}
	if (bu_str_false(argv[1])) {
	    gvp->gv_s->redraw_on_zoom = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "unknown argument to redraw_on_zoom: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "point_scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->point_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&scale) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to point_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->point_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "curve_scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->curve_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&scale) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to curve_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->curve_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "bot_threshold")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%zd\n", gvp->gv_s->bot_threshold);
	    return BRLCAD_OK;
	}
	int bcnt = 0;
	if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&bcnt) != 1 || bcnt < 0) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to bot_threshold: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->bot_threshold = bcnt;
	return BRLCAD_OK;

    }

    bu_vls_printf(gedp->ged_result_str, "unknown subcommand: %s\n", argv[0]);
    return BRLCAD_ERROR;
}

int
_view_cmd_quat(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] quat [vals]";
    const char *purpose_string = "get/set quaternion of view";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    return ged_quat_core(gd->gedp, argc, argv);
}

int
_view_cmd_selections(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] selections [options] [args]";
    const char *purpose_string = "manipulate view selections";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct bview *v = gd->gedp->ged_gvp;
    if (!v) {
	bu_vls_printf(gd->gedp->ged_result_str, "no current view selected\n");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gd->gedp->ged_result_str, "%zd", BU_PTBL_LEN(v->gv_s->gv_selected));

    return BRLCAD_OK;
}

int
_view_cmd_size(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] size [vals]";
    const char *purpose_string = "get/set view size";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    return ged_size_core(gd->gedp, argc, argv);
}

int
_view_cmd_snap(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] snap [vals]";
    const char *purpose_string = "snap to view elements";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    return ged_view_snap(gd->gedp, argc, argv);
}

int
_view_cmd_ypr(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] ypr [vals]";
    const char *purpose_string = "get/set yaw/pitch/roll of view";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    return ged_ypr_core(gd->gedp, argc, argv);
}


struct vZ_opt {
    int set;
    struct bu_vls vn;
};

static
int vZ_opt_read(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    struct vZ_opt *vZ = (struct vZ_opt *)set_var;
    vZ->set = 1;
    if (bu_opt_vls(msg, argc, argv, (void *)&vZ->vn) == 1) {
	return 1;
    }
    return 0;
}

int
_view_cmd_vZ(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view [options] vZ [opts] [val|x y z]";
    const char *purpose_string = "get/set/calc view data vZ value";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    int print_help = 0;
    struct vZ_opt calc_near = { 0, BU_VLS_INIT_ZERO };
    struct vZ_opt calc_far = { 0, BU_VLS_INIT_ZERO };
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help", "",       NULL,  &print_help, "Print help");
    BU_OPT(d[1], "N", "near", "[obj]",  &vZ_opt_read,  &calc_near,  "Find vZ value of closest view obj vertex");
    BU_OPT(d[2], "F", "far",  "[obj]",  &vZ_opt_read,  &calc_far,   "Find vZ value of furthest view obj vertex");
    BU_OPT_NULL(d[3]);

    // We know we're the vZ command - start processing args
    argc--; argv++;

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help || (calc_near.set && calc_far.set)) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage_string);
	return BRLCAD_HELP;
    }

    int calc_mode = -1;
    struct bu_vls calc_target = BU_VLS_INIT_ZERO;
    if (calc_near.set) {
	calc_mode = 0;
	bu_vls_sprintf(&calc_target, "%s", bu_vls_cstr(&calc_near.vn));
	bu_vls_free(&calc_near.vn);
    }
    if (calc_far.set) {
	calc_mode = 1;
	bu_vls_sprintf(&calc_target, "%s", bu_vls_cstr(&calc_far.vn));
	bu_vls_free(&calc_far.vn);
    }

    if (calc_mode != -1) {
	struct bview *v = gedp->ged_gvp;
	if (bu_vls_strlen(&calc_target)) {
	    // User has specified a view object to use - try to find it
struct bv_scene_obj *wobj = NULL;
	    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.view_objs, i);
		if (!bu_vls_strcmp(&s->s_uuid, &calc_target)) {
		    wobj = s;
		    break;
		}
	    }
	    if (!wobj) {
		struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS);
		for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		    if (bu_list_len(&cg->s_vlist)) {
			if (!bu_vls_strcmp(&cg->s_name, &calc_target)) {
			    wobj = cg;
			    break;
			}
		    } else {
			for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
			    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
			    if (!bu_vls_strcmp(&s->s_name, &calc_target)) {
				wobj = s;
				break;
			    }
			}
		    }
		}
	    }
	    if (wobj) {
		fastf_t vZ = bv_vZ_calc(wobj, gedp->ged_gvp, calc_mode);
		bu_vls_sprintf(gedp->ged_result_str, "%0.15f", vZ);
		return BRLCAD_OK;
	    } else {
		bu_vls_sprintf(gedp->ged_result_str, "View object %s not found", bu_vls_cstr(&calc_target));
		bu_vls_free(&calc_target);
		return BRLCAD_ERROR;
	    }
	} else {
	    // No specific view object to use - check all drawn
	    // view objects.
	    double vZ = (calc_mode) ? -DBL_MAX : DBL_MAX;
	    int have_vz = 0;
	    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.view_objs, i);
		fastf_t calc_val = bv_vZ_calc(s, gedp->ged_gvp, calc_mode);
		if (calc_mode) {
		    if (calc_val > vZ) {
			vZ = calc_mode;
			have_vz = 1;
		    }
		} else {
		    if (calc_val < vZ) {
			vZ = calc_mode;
			have_vz = 1;
		    }
		}
	    }
	    struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS);
	    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		if (bu_list_len(&cg->s_vlist)) {
		    fastf_t calc_val = bv_vZ_calc(cg, gedp->ged_gvp, calc_mode);
		    if (calc_mode) {
			if (calc_val > vZ) {
			    vZ = calc_mode;
			    have_vz = 1;
			}
		    } else {
			if (calc_val < vZ) {
			    vZ = calc_mode;
			    have_vz = 1;
			}
		    }
		} else {
		    for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
			struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
			fastf_t calc_val = bv_vZ_calc(s, gedp->ged_gvp, calc_mode);
			if (calc_mode) {
			    if (calc_val > vZ) {
				vZ = calc_mode;
				have_vz = 1;
			    }
			} else {
			    if (calc_val < vZ) {
				vZ = calc_mode;
				have_vz = 1;
			    }
			}
		    }
		}
	    }
	    if (have_vz) {
		bu_vls_sprintf(gedp->ged_result_str, "%0.15f", vZ);
	    }
	}
	return BRLCAD_OK;
    }


    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "%g\n", gedp->ged_gvp->gv_data_vZ);
	return BRLCAD_OK;
    }

    // First, see if it's a direct low level view space Z value
    if (argc == 1) {
	fastf_t val;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&val) == 1) {
	    gedp->ged_gvp->gv_data_vZ = val;
	    return BRLCAD_OK;
	}
    }

    // If not, try it as a model space point
    if (argc == 1 || argc == 3) {
	vect_t mpt;
	int acnt = bu_opt_vect_t(NULL, argc, (const char **)argv, (void *)&mpt);
	if (acnt != 1 && acnt != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	vect_t vpt;
	MAT4X3PNT(vpt, gedp->ged_gvp->gv_model2view, mpt);
	gedp->ged_gvp->gv_data_vZ = vpt[Z];
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage_string);
    return BRLCAD_ERROR;
}
int
_view_cmd_width(void *ds, int argc, const char **argv)
{
    const char *usage_string = "view [options] width";
    const char *purpose_string = "report current width in pixels of view.";
    if (_view_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_view_info *gd = (struct _ged_view_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;
    bu_vls_printf(gd->gedp->ged_result_str, "%d\n", v->gv_width);
    return BRLCAD_OK;
}

int
_view_cmd_height(void *ds, int argc, const char **argv)
{
    const char *usage_string = "view [options] height";
    const char *purpose_string = "report current height in pixels of view.";
    if (_view_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_view_info *gd = (struct _ged_view_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;
    bu_vls_printf(gd->gedp->ged_result_str, "%d\n", v->gv_height);
    return BRLCAD_OK;
}


const struct bu_cmdtab _view_cmds[] = {
    { "ae",         _view_cmd_aet},
    { "aet",        _view_cmd_aet},
    { "center",     _view_cmd_center},
    { "eye",        _view_cmd_eye},
    { "faceplate",  _view_cmd_faceplate},
    { "gobjs",      _view_cmd_gobjs},
    { "height",     _view_cmd_height},
    { "independent",_view_cmd_independent},
    { "list",       _view_cmd_list},
    { "lod",        _view_cmd_lod},
    { "obj",        _view_cmd_objs},
    { "quat",       _view_cmd_quat},
    { "selections", _view_cmd_selections},
    { "size",       _view_cmd_size},
    { "snap",       _view_cmd_snap},
    { "vZ",         _view_cmd_vZ},
    { "width",      _view_cmd_width},
    { "ypr",        _view_cmd_ypr},
    { (char *)NULL,      NULL}
};

int
ged_view_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_view_info gd;
    gd.gedp = gedp;
    gd.cmds = _view_cmds;
    gd.verbosity = 0;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the dm command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",    "",  NULL,               &help,         "Print help");
    BU_OPT(d[1], "v", "verbose", "",  &bu_opt_incr_long,  &gd.verbosity, "Verbose output");
    BU_OPT_NULL(d[2]);

    gd.gopts = d;

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_view_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    if (help) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_view_cmds, "view", "[options] subcommand [args]", &gd, argc, argv);
	} else {
	    _ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_view_cmds, "view", "[options] subcommand [args]", &gd, 0, NULL);
	}
	return BRLCAD_OK;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_view_cmds, "view", "[options] subcommand [args]", &gd, 0, NULL);
	return BRLCAD_ERROR;
    }

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_view_cmds, argc, argv, 0, (void *)&gd, &ret) == BRLCAD_OK) {
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    return BRLCAD_ERROR;
}

int
ged_view_func_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(cmd2, "1"))
	return ged_view_core(gedp, argc, argv);


    static const char *usage = "quat|ypr|aet|center|eye|size [args]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "quat")) {
	return ged_quat_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "ypr")) {
	return ged_ypr_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "aet")) {
	return ged_aet_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "center")) {
	return ged_center_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "eye")) {
	return ged_eye_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "faceplate")) {
	return ged_faceplate_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "size")) {
	return ged_size_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "data_lines") || BU_STR_EQUAL(argv[1], "sdata_lines")) {
	return ged_view_data_lines(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "snap")) {
	return ged_view_snap(gedp, argc-1, argv+1);
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}



#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl view_func_cmd_impl = {"view_func", ged_view_func_core, GED_CMD_DEFAULT};
const struct ged_cmd view_func_cmd = { &view_func_cmd_impl };

struct ged_cmd_impl view_cmd_impl = {"view", ged_view_func_core, GED_CMD_DEFAULT};
const struct ged_cmd view_cmd = { &view_cmd_impl };

struct ged_cmd_impl view2_cmd_impl = {"view2", ged_view_core, GED_CMD_DEFAULT};
const struct ged_cmd view2_cmd = { &view2_cmd_impl };

struct ged_cmd_impl quat_cmd_impl = {"quat", ged_quat_core, GED_CMD_DEFAULT};
const struct ged_cmd quat_cmd = { &quat_cmd_impl };

struct ged_cmd_impl ypr_cmd_impl = {"ypr", ged_ypr_core, GED_CMD_DEFAULT};
const struct ged_cmd ypr_cmd = { &ypr_cmd_impl };

struct ged_cmd_impl aet_cmd_impl = {"aet", ged_aet_core, GED_CMD_DEFAULT};
const struct ged_cmd aet_cmd = { &aet_cmd_impl };

struct ged_cmd_impl ae_cmd_impl = {"ae", ged_aet_core, GED_CMD_DEFAULT};
const struct ged_cmd ae_cmd = { &ae_cmd_impl };

struct ged_cmd_impl center_cmd_impl = {"center", ged_center_core, GED_CMD_DEFAULT};
const struct ged_cmd center_cmd = { &center_cmd_impl };

struct ged_cmd_impl eye_cmd_impl = {"eye", ged_eye_core, GED_CMD_DEFAULT};
const struct ged_cmd eye_cmd = { &eye_cmd_impl };

struct ged_cmd_impl eye_pt_cmd_impl = {"eye_pt", ged_eye_core, GED_CMD_DEFAULT};
const struct ged_cmd eye_pt_cmd = { &eye_pt_cmd_impl };

struct ged_cmd_impl size_cmd_impl = {"size", ged_size_core, GED_CMD_DEFAULT};
const struct ged_cmd size_cmd = { &size_cmd_impl };

struct ged_cmd_impl data_lines_cmd_impl = {"data_lines", ged_view_data_lines, GED_CMD_DEFAULT};
const struct ged_cmd data_lines_cmd = { &data_lines_cmd_impl };

struct ged_cmd_impl sdata_lines_cmd_impl = {"sdata_lines", ged_view_data_lines, GED_CMD_DEFAULT};
const struct ged_cmd sdata_lines_cmd = { &sdata_lines_cmd_impl };

const struct ged_cmd *view_cmds[] = {
    &view_func_cmd,
    &view_cmd,
    &view2_cmd,
    &quat_cmd,
    &ypr_cmd,
    &aet_cmd,
    &ae_cmd,
    &center_cmd,
    &eye_cmd,
    &eye_pt_cmd,
    &size_cmd,
    &data_lines_cmd,
    &sdata_lines_cmd,
    NULL
};

static const struct ged_plugin pinfo = { GED_API,  view_cmds, 13 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
