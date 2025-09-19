/*                       V I E W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/view.cpp
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
    BViewState *dv = gedp->dbi_state->GetBViewState();
    bu_vls_printf(gedp->ged_result_str, "  %s\n", dv->Name().c_str());
    std::vector<BViewState *> views = gedp->dbi_state->FindBViewState();
    std::set<std::string> names;
    for (size_t i = 0; i < views.size(); i++) {
	if (views[i] == dv)
	    continue;
	names.insert(views[i]->Name());
    }

    std::set<std::string>::iterator n_it;
    for (n_it = names.begin(); n_it != names.end(); ++n_it)
	bu_vls_printf(gedp->ged_result_str, "* %s\n", (*n_it).c_str());

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
	bu_vls_printf(gedp->ged_result_str, "[WARNING] this command is deprecated - vZ values should be set on data objects\n\nUsage:\n%s", usage_string);
	return GED_HELP;
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
	struct bview *v = gd->cv;
	BViewState *vs = gedp->dbi_state->GetBViewState(v);
	if (bu_vls_strlen(&calc_target)) {
	    // User has specified a view object to use - try to find it
	    std::vector<struct bv_scene_obj *> wobjs = vs->FindSceneObjs(bu_vls_cstr(&calc_target));
	    if (wobjs.size() == 1) {
		fastf_t vZ = bv_vZ_calc(wobjs[0], gd->cv, calc_mode);
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
	    std::vector<struct bv_scene_obj *> wobjs = vs->FindSceneObjs(bu_vls_cstr(&calc_target));
	    double vZ = (calc_mode) ? -DBL_MAX : DBL_MAX;
	    int have_vz = 0;
	    for (size_t i = 0; i < wobjs.size(); i++) {
		struct bv_scene_obj *cg = wobjs[i];
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
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&val) == 1) {
	    gd->cv->gv_tcl.gv_data_vZ = val;
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

const struct bu_cmdtab _view_cmds[] = {
    { "ae",         _view_cmd_aet},
    { "aet",        _view_cmd_aet},
    { "center",     _view_cmd_center},
    { "eye",        _view_cmd_eye},
    { "faceplate",  _view_cmd_faceplate},
    { "gobjs",      _view_cmd_gobjs},
    { "height",     _view_cmd_height},
    { "knob",       _view_cmd_knob},
    { "list",       _view_cmd_list},
    { "lod",        _view_cmd_lod},
    { "obj",        _view_cmd_objs},
    { "objs",       _view_cmd_objs},
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
    gd.cv = NULL;
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
    struct bu_vls vname = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",    "",      NULL,               &help,         "Print help");
    BU_OPT(d[1], "v", "verbose", "",      &bu_opt_incr_long,  &gd.verbosity, "Verbose output");
    BU_OPT(d[2], "V", "view",    "name",  &bu_opt_vls,        &vname,        "Specified view (default is GED current)");
    BU_OPT_NULL(d[3]);

    gd.gopts = d;

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_view_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    // Clear out any high level opts prior to subcommand
    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    int ac_ret = bu_opt_parse(NULL, acnt, argv, d);
    if (ac_ret) {
	help = 1;
    } else {
	for (int i = 0; i < acnt; i++) {
	    argc--; argv++;
	}
    }

    if (help) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_view_cmds, "view", "[options] subcommand [args]", &gd, argc, argv);
	} else {
	    _ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_view_cmds, "view", "[options] subcommand [args]", &gd, 0, NULL);
	}
	bu_vls_free(&vname);
	return BRLCAD_OK;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_view_cmds, "view", "[options] subcommand [args]", &gd, 0, NULL);
	bu_vls_free(&vname);
	return BRLCAD_ERROR;
    }

    // Either a view was specified, or we use the current view
    if (bu_vls_strlen(&vname)) {
	struct bu_ptbl *views = &gedp->ged_views;
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	    if (BU_STR_EQUAL(bu_vls_cstr(&vname), bu_vls_cstr(&v->gv_name))) {
		gd.cv = v;
		break;
	    }
	}
	if (!gd.cv) {
	    bu_vls_printf(gedp->ged_result_str, ": invalid view name: %s", bu_vls_cstr(&vname));
	    bu_vls_free(&vname);
	    return BRLCAD_ERROR;
	}
    } else {
	gd.cv = gedp->ged_gvp;
    }

    if (!gd.cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view specified and no view listed as current in GED");
	bu_vls_free(&vname);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_view_cmds, argc, argv, 0, (void *)&gd, &ret) == BRLCAD_OK) {
	bu_vls_free(&vname);
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    bu_vls_free(&vname);
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


extern "C" {
#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl view_func_cmd_impl = {"view_func", ged_view_func_core, GED_CMD_DEFAULT};
const struct ged_cmd view_func_cmd = { &view_func_cmd_impl };

struct ged_cmd_impl view_cmd_impl = {"view", ged_view_func_core, GED_CMD_DEFAULT};
const struct ged_cmd view_cmd = { &view_cmd_impl };

struct ged_cmd_impl view2_cmd_impl = {"view2", ged_view_core, GED_CMD_DEFAULT};
const struct ged_cmd view2_cmd = { &view2_cmd_impl };

struct ged_cmd_impl ae_cmd_impl = {"ae", ged_aet_core, GED_CMD_DEFAULT};
const struct ged_cmd ae_cmd = { &ae_cmd_impl };

struct ged_cmd_impl aet_cmd_impl = {"aet", ged_aet_core, GED_CMD_DEFAULT};
const struct ged_cmd aet_cmd = { &aet_cmd_impl };

struct ged_cmd_impl autoview_cmd_impl = { "autoview", ged_autoview_core, GED_CMD_DEFAULT };
const struct ged_cmd autoview_cmd = { &autoview_cmd_impl };

struct ged_cmd_impl center_cmd_impl = {"center", ged_center_core, GED_CMD_DEFAULT};
const struct ged_cmd center_cmd = { &center_cmd_impl };

struct ged_cmd_impl data_lines_cmd_impl = {"data_lines", ged_view_data_lines, GED_CMD_DEFAULT};
const struct ged_cmd data_lines_cmd = { &data_lines_cmd_impl };

struct ged_cmd_impl eye_cmd_impl = {"eye", ged_eye_core, GED_CMD_DEFAULT};
const struct ged_cmd eye_cmd = { &eye_cmd_impl };

struct ged_cmd_impl eye_pt_cmd_impl = {"eye_pt", ged_eye_core, GED_CMD_DEFAULT};
const struct ged_cmd eye_pt_cmd = { &eye_pt_cmd_impl };

struct ged_cmd_impl lookat_cmd_impl = {"lookat", ged_lookat_core, GED_CMD_DEFAULT};
const struct ged_cmd lookat_cmd = { &lookat_cmd_impl };

struct ged_cmd_impl print_cmd_impl = {"print", _view_cmd_print, GED_CMD_DEFAULT};
const struct ged_cmd print_cmd = { &print_cmd_impl };

struct ged_cmd_impl quat_cmd_impl = {"quat", ged_quat_core, GED_CMD_DEFAULT};
const struct ged_cmd quat_cmd = { &quat_cmd_impl };

struct ged_cmd_impl qvrot_cmd_impl = {"qvrot", ged_qvrot_core, GED_CMD_DEFAULT};
const struct ged_cmd qvrot_cmd = { &qvrot_cmd_impl };

struct ged_cmd_impl saveview_cmd_impl = {"saveview", ged_saveview_core, GED_CMD_DEFAULT};
const struct ged_cmd saveview_cmd = { &saveview_cmd_impl };

struct ged_cmd_impl sdata_lines_cmd_impl = {"sdata_lines", ged_view_data_lines, GED_CMD_DEFAULT};
const struct ged_cmd sdata_lines_cmd = { &sdata_lines_cmd_impl };

struct ged_cmd_impl size_cmd_impl = {"size", ged_size_core, GED_CMD_DEFAULT};
const struct ged_cmd size_cmd = { &size_cmd_impl };

struct ged_cmd_impl viewdir_cmd_impl = {"viewdir", ged_viewdir_core, GED_CMD_DEFAULT};
const struct ged_cmd viewdir_cmd = { &viewdir_cmd_impl };

struct ged_cmd_impl ypr_cmd_impl = {"ypr", ged_ypr_core, GED_CMD_DEFAULT};
const struct ged_cmd ypr_cmd = { &ypr_cmd_impl };

const struct ged_cmd *view_cmds[] = {
    &view_func_cmd,
    &view_cmd,
    &view2_cmd,
    &ae_cmd,
    &aet_cmd,
    &autoview_cmd,
    &center_cmd,
    &data_lines_cmd,
    &eye_cmd,
    &eye_pt_cmd,
    &lookat_cmd,
    &print_cmd,
    &quat_cmd,
    &qvrot_cmd,
    &saveview_cmd,
    &sdata_lines_cmd,
    &size_cmd,
    &viewdir_cmd,
    &ypr_cmd,
    NULL
};

static const struct ged_plugin pinfo = { GED_API,  view_cmds, 19 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
