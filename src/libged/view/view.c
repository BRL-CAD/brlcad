/*                         V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
#include "bu/cmdschema.h"
#include "bu/vls.h"

#include "../ged_private.h"
#include "./ged_view.h"
#include "./faceplate/faceplate.h"

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

    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = ged_aet_core(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
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

    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = ged_center_core(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
}

int
_view_cmd_dir(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] dir [-i] [x y z [twist]]";
    const char *purpose_string = "get/set view direction and optional twist";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    /* A direction query is viewdir; a supplied vector and optional twist are
     * qvrot.  Both use the eye-from-center convention, and -i consistently
     * selects its inverse. */
    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = (argc < 4) ?
	ged_viewdir_core(gd->gedp, argc, argv) :
	ged_qvrot_core(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
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

    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = ged_eye_core(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
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

    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = ged_faceplate_core(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
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
	    const char *av[5] = {"draw", "-R", "-V", NULL, NULL};
	    av[3] = bu_vls_cstr(&v->gv_name);
	    av[4] = bu_vls_cstr(&opath);
	    ged_exec_draw(gedp, 5, av);
	    bu_vls_free(&opath);
	}
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[1], "0")) {
	v->independent = 0;
	// Clear local containers
	struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
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
_view_cmd_quat(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] quat [vals]";
    const char *purpose_string = "get/set quaternion of view";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = ged_quat_core(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
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

    struct bview *v = gd->cv;
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

    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = ged_size_core(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
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

    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = ged_view_snap(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
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

    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = ged_ypr_core(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
}


struct view_vZ_args {
    int help;
    const char *near;
    const char *far;
};

static const struct bu_cmd_option view_vZ_options[] = {
    BU_CMD_FLAG("h", "help", struct view_vZ_args, help, "Print help"),
    BU_CMD_OPTIONAL_STRING("N", "near", struct view_vZ_args, near, "obj",
	"Find the closest view-object vertex"),
    BU_CMD_OPTIONAL_STRING("F", "far", struct view_vZ_args, far, "obj",
	"Find the furthest view-object vertex"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand view_vZ_operands[] = {
    BU_CMD_OPERAND("value_or_point", BU_CMD_VALUE_RAW, 0, 3,
	"A view-space Z value, or a model-space point", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema view_vZ_native_schema = {
    "vZ", "Query or set view-space depth", view_vZ_options, view_vZ_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

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

    // We know we're the vZ command - start processing args
    argc--; argv++;

    struct view_vZ_args args = {0, NULL, NULL};
    int calc_near = bu_cmd_schema_option_present(&view_vZ_native_schema,
	(size_t)argc, argv, "near");
    int calc_far = bu_cmd_schema_option_present(&view_vZ_native_schema,
	(size_t)argc, argv, "far");
    struct bu_vls parse_msg = BU_VLS_INIT_ZERO;
    int operand_index = bu_cmd_schema_parse(&view_vZ_native_schema, &args,
	&parse_msg, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&parse_msg));
    bu_vls_free(&parse_msg);
	return BRLCAD_ERROR;
    }
	bu_vls_free(&parse_msg);
    argc -= operand_index;
    argv += operand_index;

    if (args.help || (calc_near && calc_far)) {
	bu_vls_printf(gedp->ged_result_str, "[WARNING] this command is deprecated - vZ values should be set on data objects\n\nUsage:\n%s", usage_string);
	return GED_HELP;
    }

    int calc_mode = -1;
    struct bu_vls calc_target = BU_VLS_INIT_ZERO;
    if (calc_near) {
	calc_mode = 0;
	if (args.near)
	    bu_vls_sprintf(&calc_target, "%s", args.near);
    }
    if (calc_far) {
	calc_mode = 1;
	if (args.far)
	    bu_vls_sprintf(&calc_target, "%s", args.far);
    }

    if (calc_mode != -1) {
	struct bview *v = gd->cv;
	if (bu_vls_strlen(&calc_target)) {
	    // User has specified a view object to use - try to find it
	    struct bv_scene_obj *wobj = bv_find_obj(v, bu_vls_cstr(&calc_target));
	    if (wobj) {
		fastf_t vZ = bv_vZ_calc(wobj, gd->cv, calc_mode);
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
	    struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
	    struct bu_ptbl *local_view_objs = bv_view_objs(v, BV_VIEW_OBJS | BV_LOCAL_OBJS);
	    struct bu_ptbl *db_objs = bv_view_objs(v, BV_DB_OBJS);
	    struct bu_ptbl *local_db_objs = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
	    double vZ = (calc_mode) ? -DBL_MAX : DBL_MAX;
	    int have_vz = 0;
	    if (view_objs) {
		for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		    fastf_t calc_val = bv_vZ_calc(s, gd->cv, calc_mode);
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
	    if (local_view_objs) {
		for (size_t i = 0; i < BU_PTBL_LEN(local_view_objs); i++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(local_view_objs, i);
		    fastf_t calc_val = bv_vZ_calc(s, gd->cv, calc_mode);
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

	    if (db_objs) {
		for (size_t i = 0; i < BU_PTBL_LEN(db_objs); i++) {
		    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(db_objs, i);
		    if (bu_list_len(&cg->s_vlist)) {
			fastf_t calc_val = bv_vZ_calc(cg, gd->cv, calc_mode);
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
			    fastf_t calc_val = bv_vZ_calc(s, gd->cv, calc_mode);
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
	    }
	    if (local_db_objs) {
		for (size_t i = 0; i < BU_PTBL_LEN(local_db_objs); i++) {
		    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(local_db_objs, i);
		    if (bu_list_len(&cg->s_vlist)) {
			fastf_t calc_val = bv_vZ_calc(cg, gd->cv, calc_mode);
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
			    fastf_t calc_val = bv_vZ_calc(s, gd->cv, calc_mode);
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
	    }
	    if (have_vz) {
		bu_vls_sprintf(gedp->ged_result_str, "%0.15f", vZ);
	    }
	}
	return BRLCAD_OK;
    }


    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "%g\n", gd->cv->gv_tcl.gv_data_vZ);
	return BRLCAD_OK;
    }

    // First, see if it's a direct low level view space Z value
    if (argc == 1) {
	fastf_t val;
	if (bu_cmd_number_from_str(&val, argv[0])) {
	    gd->cv->gv_tcl.gv_data_vZ = val;
	    return BRLCAD_OK;
	}
    }

    // If not, try it as a model space point
    if (argc == 1 || argc == 3) {
	vect_t mpt;
	int acnt = bu_cmd_vector3_from_argv(mpt, (size_t)argc,
	    (const char * const *)argv);
	if (acnt != 1 && acnt != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	vect_t vpt;
	MAT4X3PNT(vpt, gd->cv->gv_model2view, mpt);
	gd->cv->gv_tcl.gv_data_vZ = vpt[Z];
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
    struct bview *v = gd->cv;
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
    struct bview *v = gd->cv;
    bu_vls_printf(gd->gedp->ged_result_str, "%d\n", v->gv_height);
    return BRLCAD_OK;
}

int
_view_cmd_print(struct ged *gedp, int argc, const char **argv)
{
    // ae
    ged_aet_core(gedp, argc, argv);
    char* ae = bu_vls_strdup(gedp->ged_result_str);

    // dir
    ged_viewdir_core(gedp, argc, argv);
    char* dir = bu_vls_strdup(gedp->ged_result_str);

    // center
    ged_center_core(gedp, argc, argv);
    char* center = bu_vls_strdup(gedp->ged_result_str);

    // eye
    ged_eye_core(gedp, argc, argv);
    char* eye = bu_vls_strdup(gedp->ged_result_str);

    // size
    ged_size_core(gedp, argc, argv);
    char* size = bu_vls_strdup(gedp->ged_result_str);

    // print
    bu_vls_trunc(gedp->ged_result_str, 0);
    bu_vls_printf(gedp->ged_result_str, "    ae: %s\n    dir: <%s>\n    center: (%s)\n    eye: (%s)\n    size: %s\n", ae, dir, center, eye, size);

    // cleanup
    bu_free(ae, "ae str free");
    bu_free(dir, "dir str free");
    bu_free(center, "center str free");
    bu_free(eye, "eye str free");
    bu_free(size, "size str free");

    return BRLCAD_OK;
}

int
_view_cmd_knob(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] knob [vals]";
    const char *purpose_string = "low level view rotate/translate/scale operations";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct bview *cv = gd->gedp->ged_gvp;
    gd->gedp->ged_gvp = gd->cv;
    int ret = ged_knob_core(gd->gedp, argc, argv);
    gd->gedp->ged_gvp = cv;
    return ret;
}

struct view_root_args {
    int help;
    long verbosity;
    struct bu_vls vname;
};

static const struct bu_cmd_option view_root_options[] = {
    BU_CMD_FLAG("h", "help", struct view_root_args, help, "Print help"),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbose", struct view_root_args, verbosity,
	"Increase output detail"),
    {"V", "view", "view", "name", "Target view name", BU_CMD_VALUE_VLS,
	offsetof(struct view_root_args, vname), NULL, NULL, "ged.view", NULL, 0,
	0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL},
    BU_CMD_OPTION_NULL
};
GED_EXPORT const struct bu_cmd_schema ged_view_native_schema = {
    "view", "Inspect and manipulate named views", view_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
GED_EXPORT const struct bu_cmd_schema ged_view2_native_schema = {
    "view2", "Inspect and manipulate named views", view_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static const struct bu_cmd_operand view_tree_raw_operands[] = {
    BU_CMD_OPERAND("arguments", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"View operation arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const char *view_ae_aliases[] = {"aet", NULL};
static const char *view_obj_aliases[] = {"objs", NULL};

/* The root tree replaces the old command table.  Most child schemas remain
 * deliberately raw until their executors are migrated; native leaves such as
 * vZ attach directly below.  This preserves one canonical child vocabulary
 * and dispatch path while keeping the still-unmigrated syntax conservative. */
#define GED_VIEW_TREE_CHILDREN(X) \
    X(ae, _view_cmd_aet, "Query or set azimuth, elevation, and twist", view_ae_aliases, &view_tree_ae_schema) \
    X(center, _view_cmd_center, "Query or set the view center", NULL, &view_tree_center_schema) \
    X(dir, _view_cmd_dir, "Query or set view direction and optional twist", NULL, &view_tree_dir_schema) \
    X(eye, _view_cmd_eye, "Query or set the eye point", NULL, &view_tree_eye_schema) \
    X(gobjs, _view_cmd_gobjs, "Manage graphical view objects", NULL, &view_tree_gobjs_schema) \
    X(height, _view_cmd_height, "Report view height", NULL, &view_tree_height_schema) \
    X(independent, _view_cmd_independent, "Query or set independent-view state", NULL, &view_tree_independent_schema) \
    X(knob, _view_cmd_knob, "Apply low-level view controls", NULL, &ged_view_knob_schema) \
    X(list, _view_cmd_list, "List views", NULL, &view_tree_list_schema) \
    X(lod, _view_cmd_lod, "Manage level-of-detail settings", NULL, &ged_view_lod_schema) \
    X(obj, _view_cmd_objs, "Manage view objects", view_obj_aliases, &ged_view_obj_schema) \
    X(quat, _view_cmd_quat, "Query or set view quaternion", NULL, &view_tree_quat_schema) \
    X(selections, _view_cmd_selections, "Manage view selections", NULL, &view_tree_selections_schema) \
    X(size, _view_cmd_size, "Query or set view size", NULL, &view_tree_size_schema) \
    X(snap, _view_cmd_snap, "Snap view coordinates", NULL, &view_tree_snap_schema) \
    X(width, _view_cmd_width, "Report view width", NULL, &view_tree_width_schema) \
    X(ypr, _view_cmd_ypr, "Set yaw, pitch, and roll", NULL, &view_tree_ypr_schema)

#define GED_VIEW_TREE_SCHEMA(_name, _func, _help, _aliases, _schema_ptr) \
    static const struct bu_cmd_schema view_tree_##_name##_schema = { \
	#_name, _help, NULL, view_tree_raw_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, \
	BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL) \
    };
GED_VIEW_TREE_CHILDREN(GED_VIEW_TREE_SCHEMA)
#undef GED_VIEW_TREE_SCHEMA

#define GED_VIEW_TREE_NODE(_name, _func, _help, _aliases, _schema_ptr) \
    BU_CMD_TREE_NODE(_schema_ptr, _aliases, NULL, \
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _func),
static const struct bu_cmd_tree_node view_tree_subcommands[] = {
    GED_VIEW_TREE_CHILDREN(GED_VIEW_TREE_NODE)
    BU_CMD_TREE_NODE(&ged_faceplate_native_schema, NULL, ged_faceplate_subcommands,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _view_cmd_faceplate),
    BU_CMD_TREE_NODE(&view_vZ_native_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _view_cmd_vZ),
    BU_CMD_TREE_NODE_NULL
};
#undef GED_VIEW_TREE_NODE

GED_EXPORT const struct bu_cmd_tree ged_view_tree = {
    &ged_view_native_schema, view_tree_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};
GED_EXPORT const struct bu_cmd_tree ged_view2_tree = {
    &ged_view2_native_schema, view_tree_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};
#undef GED_VIEW_TREE_CHILDREN

static void
view_tree_show_help(struct ged *gedp, const struct bu_cmd_tree *tree)
{
    char *help = bu_cmd_tree_describe(tree);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "view native tree help");
    }
}

int
ged_view_core(struct ged *gedp, int argc, const char *argv[])
{
    struct _ged_view_info gd;
    struct view_root_args args = {0, 0, BU_VLS_INIT_ZERO};
    const struct bu_cmd_tree *tree = NULL;
    const char *subcommand = NULL;
    int cmd_pos = -1;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    tree = BU_STR_EQUAL(argv[0], "view2") ? &ged_view2_tree : &ged_view_tree;
    gd.gedp = gedp;
    gd.cmds = NULL;
    gd.gopts = NULL;
    gd.cv = NULL;
    gd.verbosity = 0;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the dm command - start processing args
    argc--; argv++;

    // High level options are only defined prior to the subcommand
    for (int i = 0; i < argc; i++) {
	int option_span = bu_cmd_schema_option_span(tree->root_schema,
	    (size_t)(argc - i), argv + i);
	if (option_span > 0) {
	    i += option_span - 1;
	    continue;
	}
	/* Keep a malformed or unknown option in the root phase so its native
	 * parse error wins over a misleading "subcommand" diagnosis. */
	if (option_span < 0 || (argv[i][0] == '-' && argv[i][1]))
	    break;
	cmd_pos = i;
	subcommand = argv[i];
	break;
    }

    // Clear out any high level opts prior to subcommand
    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    int ac_ret = bu_cmd_schema_parse(tree->root_schema, &args,
	gedp->ged_result_str, acnt, argv);
    if (ac_ret < 0 || ac_ret != acnt) {
	args.help = 1;
    } else {
	argc -= acnt;
	argv += acnt;
    }
    gd.verbosity = args.verbosity;

    if (args.help) {
	if (subcommand) {
	    const char *sub_help[] = {subcommand, HELPFLAG};
	    int ignored = BRLCAD_OK;
	    (void)bu_cmd_tree_dispatch(tree, &gd, 2, sub_help, &ignored);
	} else {
	    view_tree_show_help(gedp, tree);
	}
	bu_vls_free(&args.vname);
	return BRLCAD_OK;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	view_tree_show_help(gedp, tree);
	bu_vls_free(&args.vname);
	return BRLCAD_ERROR;
    }

    // Either a view was specified, or we use the current view
    if (bu_vls_strlen(&args.vname)) {
	struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	    if (BU_STR_EQUAL(bu_vls_cstr(&args.vname), bu_vls_cstr(&v->gv_name))) {
		gd.cv = v;
		break;
	    }
	}
	if (!gd.cv) {
	    bu_vls_printf(gedp->ged_result_str, ": invalid view name: %s", bu_vls_cstr(&args.vname));
	    bu_vls_free(&args.vname);
	    return BRLCAD_ERROR;
	}
    } else {
	gd.cv = gedp->ged_gvp;
    }

    if (!gd.cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view specified and no view listed as current in GED");
	bu_vls_free(&args.vname);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd_tree_dispatch(tree, &gd, argc, argv, &ret) == 0) {
	bu_vls_free(&args.vname);
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    bu_vls_free(&args.vname);
    return BRLCAD_ERROR;
}

int
ged_view_func_core(struct ged *gedp, int argc, const char *argv[])
{
    if (gedp->new_cmd_forms)
	return ged_view_core(gedp, argc, argv);


    static const char *usage = "ae|aet|auto|center|eye|knob|lookat|print|quat|save|size|ypr [args]";

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[1], "aet") || BU_STR_EQUAL(argv[1], "ae")) {
	return ged_aet_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "align")) {
	return ged_align_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "autoview") || BU_STR_EQUAL(argv[1], "auto")) {
	return ged_autoview_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "center")) {
	return ged_center_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "data_lines") || BU_STR_EQUAL(argv[1], "sdata_lines")) {
	return ged_view_data_lines(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "eye")) {
	return ged_eye_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "faceplate")) {
	return ged_faceplate_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "knob")) {
	return ged_knob_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "lookat")) {
	return ged_lookat_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "obj")) {
	return _view_cmd_old_obj(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "print")) {
	return _view_cmd_print(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "quat")) {
	return ged_quat_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "qvrot") || BU_STR_EQUAL(argv[1], "dir")) {
	if (argc < 4)
	    return ged_viewdir_core(gedp, argc-1, argv+1);
	return ged_qvrot_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "saveview") || BU_STR_EQUAL(argv[1], "save")) {
	return ged_saveview_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "size")) {
	return ged_size_core(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "snap")) {
	return ged_view_snap(gedp, argc-1, argv+1);
    }

    if (BU_STR_EQUAL(argv[1], "ypr")) {
	return ged_ypr_core(gedp, argc-1, argv+1);
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

static const struct bu_cmd_operand view_child_args[] = {
    BU_CMD_OPERAND("arguments", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"View operation arguments", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_option view_increment_options[] = {
    BU_CMD_FLAG_UNBOUND("i", NULL, "i", "Apply an incremental change"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand view_aet_operands[] = {
    BU_CMD_OPERAND("angles", BU_CMD_VALUE_RAW, 0, 3,
	"Packed AET vector, or azimuth elevation and optional twist", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand view_center_operands[] = {
    BU_CMD_OPERAND("center", BU_CMD_VALUE_RAW, 0, 3,
	"Packed center point, XYZ components, or -v", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand view_point_operands[] = {
    BU_CMD_OPERAND("point", BU_CMD_VALUE_RAW, 1, 3, "Packed point or XYZ components", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand view_xyz_operands[] = {
    BU_CMD_OPERAND("components", BU_CMD_VALUE_NUMBER, 3, 3, "XYZ components", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand view_quat_operands[] = {
    BU_CMD_OPERAND("quaternion", BU_CMD_VALUE_NUMBER, 4, 4, "Quaternion components", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand view_size_operands[] = {
    BU_CMD_OPERAND("size", BU_CMD_VALUE_NUMBER, 0, 1, "Optional new view size", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand view_qvrot_operands[] = {
    BU_CMD_OPERAND("axis_and_angle", BU_CMD_VALUE_NUMBER, 3, 4,
	"Rotation axis and optional angle", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_option autoview_schema_options[] = {
    BU_CMD_FLAG_UNBOUND("h", "help", "help", "Print help"),
    BU_CMD_FLAG_UNBOUND(NULL, "all-objs", "all-objs", "Bound all non-faceplate view objects"),
    BU_CMD_VALUE_UNBOUND("s", "scale", "scale", BU_CMD_VALUE_NUMBER, "number", "View scale"),
    {"V", "view", "view", "name", "Target view", BU_CMD_VALUE_STRING,
	BU_CMD_STORAGE_NONE, NULL, NULL, "ged.view", NULL, 0, 0, NULL,
	BU_CMD_ARG_REQUIRED, NULL, NULL, NULL},
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand autoview_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 0, BU_CMD_COUNT_UNLIMITED,
	"Objects to bound", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_option saveview_options[] = {
    BU_CMD_VALUE_UNBOUND("e", NULL, "e", BU_CMD_VALUE_FILE, "program", "Raytrace executable"),
    BU_CMD_VALUE_UNBOUND("i", NULL, "i", BU_CMD_VALUE_FILE, "file", "Input database file"),
    BU_CMD_VALUE_UNBOUND("l", NULL, "l", BU_CMD_VALUE_FILE, "file", "Log output file"),
    BU_CMD_VALUE_UNBOUND("o", NULL, "o", BU_CMD_VALUE_FILE, "file", "Pixel output file"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand saveview_operands[] = {
    BU_CMD_OPERAND("script", BU_CMD_VALUE_FILE, 1, 1, "Output shell script", "ged.file_path"),
    BU_CMD_OPERAND("rt_arguments", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Additional raytrace arguments", NULL),
    BU_CMD_OPERAND_NULL
};
#define VIEW_SCHEMA(_id, _name, _help, _opts, _ops, _policy) \
    static const struct bu_cmd_schema _id##_cmd_schema = { \
	_name, _help, _opts, _ops, _policy, {NULL} \
    }
VIEW_SCHEMA(ae, "ae", "Query or set azimuth, elevation, and twist", view_increment_options, view_aet_operands, BU_CMD_PARSE_INTERSPERSED);
VIEW_SCHEMA(aet, "aet", "Query or set azimuth, elevation, and twist", view_increment_options, view_aet_operands, BU_CMD_PARSE_INTERSPERSED);
VIEW_SCHEMA(autoview, "autoview", "Fit the view to objects", autoview_schema_options, autoview_operands, BU_CMD_PARSE_INTERSPERSED);
VIEW_SCHEMA(center, "center", "Query or set the view center", NULL, view_center_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND);
VIEW_SCHEMA(eye, "eye", "Query or set the eye point", NULL, view_point_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND);
VIEW_SCHEMA(eye_pt, "eye_pt", "Query or set the eye point", NULL, view_point_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND);
VIEW_SCHEMA(lookat, "lookat", "Aim the view at a point", NULL, view_xyz_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND);
VIEW_SCHEMA(print, "print", "Print view parameters", NULL, NULL, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND);
VIEW_SCHEMA(quat, "quat", "Set the view quaternion", NULL, view_quat_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND);
VIEW_SCHEMA(qvrot, "qvrot", "Rotate the view about an axis", view_increment_options, view_qvrot_operands, BU_CMD_PARSE_INTERSPERSED);
VIEW_SCHEMA(saveview, "saveview", "Write a raytrace shell script for the view", saveview_options, saveview_operands, BU_CMD_PARSE_INTERSPERSED);
VIEW_SCHEMA(size, "size", "Query or set view size", NULL, view_size_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND);
VIEW_SCHEMA(viewdir, "viewdir", "Report the view direction", view_increment_options, NULL, BU_CMD_PARSE_INTERSPERSED);
VIEW_SCHEMA(ypr, "ypr", "Set yaw, pitch, and roll", NULL, view_xyz_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND);
#undef VIEW_SCHEMA

#define VIEW_LINES_SCHEMA(_id, _name, _help) \
    static const struct bu_cmd_schema _id##_schema = { \
	_name, _help, NULL, view_child_args, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL} \
    }
VIEW_LINES_SCHEMA(view_lines_draw, "draw", "Query or set line drawing");
VIEW_LINES_SCHEMA(view_lines_color, "color", "Query or set line color");
VIEW_LINES_SCHEMA(view_lines_width, "line_width", "Query or set line width");
VIEW_LINES_SCHEMA(view_lines_points, "points", "Query or set line points");
VIEW_LINES_SCHEMA(view_lines_snap, "snap", "Query or set line snapping");
#undef VIEW_LINES_SCHEMA
static const struct bu_cmd_tree_node view_lines_subcommands[] = {
    BU_CMD_TREE_NODE(&view_lines_draw_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&view_lines_color_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&view_lines_width_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&view_lines_points_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&view_lines_snap_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_schema data_lines_root_schema = {
    "data_lines", "Manage transient view lines", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_schema sdata_lines_root_schema = {
    "sdata_lines", "Manage shared transient view lines", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_tree data_lines_tree = {
    &data_lines_root_schema, view_lines_subcommands, BU_CMD_TREE_CHILD_FIRST
};
static const struct bu_cmd_tree sdata_lines_tree = {
    &sdata_lines_root_schema, view_lines_subcommands, BU_CMD_TREE_CHILD_FIRST
};
static const char * const autoview_aliases[] = {"auto", NULL};
static const char * const saveview_aliases[] = {"save", NULL};
static const struct bu_cmd_schema view_func_root_schema = {
    "view_func", "Legacy view operation dispatcher", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_tree_node view_func_subcommands[] = {
    BU_CMD_TREE_NODE(&ae_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&autoview_cmd_schema, autoview_aliases, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&center_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&eye_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&lookat_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&print_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&quat_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&qvrot_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&saveview_cmd_schema, saveview_aliases, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&size_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&ypr_cmd_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree view_func_tree = {
    &view_func_root_schema, view_func_subcommands, BU_CMD_TREE_CHILD_FIRST
};

#define VIEW_TREE_GRAMMAR(_id, _tree, _name, _help) \
    static int _id##_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos, struct ged_cmd_validate_result *result) \
    { return ged_cmd_tree_validate(gedp, &_tree, input, cursor_pos, result); } \
    static int _id##_grammar_analyze(struct ged *gedp, const char *input, struct ged_cmd_analysis *analysis) \
    { return ged_cmd_tree_analyze(gedp, &_tree, input, analysis); } \
    static char *_id##_grammar_json(void) { return bu_cmd_tree_describe_json(&_tree); } \
    static int _id##_grammar_lint(struct bu_vls *msgs) { return bu_cmd_tree_lint(&_tree, msgs); } \
    static const struct ged_cmd_grammar _id##_grammar = { \
	_name, _help, _id##_grammar_validate, _id##_grammar_analyze, _id##_grammar_json, _id##_grammar_lint \
    }
VIEW_TREE_GRAMMAR(data_lines, data_lines_tree, "data_lines", "Manage transient view lines");
VIEW_TREE_GRAMMAR(sdata_lines, sdata_lines_tree, "sdata_lines", "Manage shared transient view lines");
VIEW_TREE_GRAMMAR(view_func, view_func_tree, "view_func", "Legacy view operation dispatcher");
#undef VIEW_TREE_GRAMMAR

#define GED_VIEW_COMMANDS(X, XID, N, NID, G, GID) \
    N(ae, ged_aet_core, GED_CMD_DEFAULT, &ae_cmd_schema) \
    N(aet, ged_aet_core, GED_CMD_DEFAULT, &aet_cmd_schema) \
    N(autoview, ged_autoview_core, GED_CMD_DEFAULT, &autoview_cmd_schema) \
    N(center, ged_center_core, GED_CMD_DEFAULT, &center_cmd_schema) \
    G(data_lines, ged_view_data_lines, GED_CMD_DEFAULT, &data_lines_grammar) \
    N(eye, ged_eye_core, GED_CMD_DEFAULT, &eye_cmd_schema) \
    N(eye_pt, ged_eye_core, GED_CMD_DEFAULT, &eye_pt_cmd_schema) \
    N(lookat, ged_lookat_core, GED_CMD_DEFAULT, &lookat_cmd_schema) \
    N(print, _view_cmd_print, GED_CMD_DEFAULT, &print_cmd_schema) \
    N(quat, ged_quat_core, GED_CMD_DEFAULT, &quat_cmd_schema) \
    N(qvrot, ged_qvrot_core, GED_CMD_DEFAULT, &qvrot_cmd_schema) \
    N(saveview, ged_saveview_core, GED_CMD_DEFAULT, &saveview_cmd_schema) \
    G(sdata_lines, ged_view_data_lines, GED_CMD_DEFAULT, &sdata_lines_grammar) \
    N(size, ged_size_core, GED_CMD_DEFAULT, &size_cmd_schema) \
    G(view, ged_view_core, GED_CMD_DEFAULT, &ged_view_grammar) \
    G(view2, ged_view_core, GED_CMD_DEFAULT, &ged_view2_grammar) \
    G(view_func, ged_view_func_core, GED_CMD_DEFAULT, &view_func_grammar) \
    N(viewdir, ged_viewdir_core, GED_CMD_DEFAULT, &viewdir_cmd_schema) \
    N(ypr, ged_ypr_core, GED_CMD_DEFAULT, &ypr_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_VIEW_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_view", 1, GED_VIEW_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
