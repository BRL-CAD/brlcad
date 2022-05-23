/*                        O B J S . C P P
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
/** @file libged/view/objs.c
 *
 * Commands for view objects.
 *
 */

#include "common.h"

#include <ctype.h>
#include <cstdlib>
#include <cstring>
#include <queue>

extern "C" {
#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "bv.h"

#include "../ged_private.h"
#include "./ged_view.h"
}

int
_objs_cmd_draw(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name draw [0|1]";
    const char *purpose_string = "toggle view polygons";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (argc == 0) {
	if (s->s_flag == UP) {
	    bu_vls_printf(gedp->ged_result_str, "UP\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "DOWN\n");
	}
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "DOWN")) {
	s->s_flag = DOWN;
	return BRLCAD_OK;
    }
    if (BU_STR_EQUAL(argv[0], "UP")) {
	s->s_flag = UP;
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
    return BRLCAD_ERROR;
}

int
_objs_cmd_delete(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name delete";
    const char *purpose_string = "delete view object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (!(s->s_type_flags & BV_VIEWONLY)) {
	bu_vls_printf(gedp->ged_result_str, "View object %s is associated with a database object - use 'erase' cmd to clear\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    bv_obj_put(s);

    return BRLCAD_OK;
}

int
_objs_cmd_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name color [r/g/b]";
    const char *purpose_string = "show/set obj color";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    int recurse = 0;

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "r", "recursive",       "",  NULL,  &recurse,  "Report (or set) color of all child objects");
    BU_OPT_NULL(d[1]);

    int ac = bu_opt_parse(NULL, argc, argv, d);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (ac == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", s->s_color[0], s->s_color[1], s->s_color[2]);
	if (recurse) {
	    std::queue<struct bv_scene_obj *> sobjs;
	    sobjs.push(s);
	    while (!sobjs.empty()) {
		struct bv_scene_obj *sc = sobjs.front();
		sobjs.pop();
		bu_vls_printf(gedp->ged_result_str, "%s: %d/%d/%d\n", bu_vls_cstr(&sc->s_uuid), sc->s_color[0], sc->s_color[1], sc->s_color[2]);
		for (size_t i = 0; i < BU_PTBL_LEN(&sc->children); i++) {
		    struct bv_scene_obj *scn = (struct bv_scene_obj *)BU_PTBL_GET(&sc->children, i);
		    sobjs.push(scn);
		}
	    }
	}
	return BRLCAD_OK;
    }
    struct bu_color val;
    if (bu_opt_color(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    bu_color_to_rgb_chars(&val, s->s_color);
    if (recurse) {
	if (recurse) {
	    std::queue<struct bv_scene_obj *> sobjs;
	    sobjs.push(s);
	    while (!sobjs.empty()) {
		struct bv_scene_obj *sc = sobjs.front();
		sobjs.pop();
		bu_color_to_rgb_chars(&val, sc->s_color);
		for (size_t i = 0; i < BU_PTBL_LEN(&sc->children); i++) {
		    struct bv_scene_obj *scn = (struct bv_scene_obj *)BU_PTBL_GET(&sc->children, i);
		    sobjs.push(scn);
		}
	    }
	}
    }
    return BRLCAD_OK;
}

int
_objs_cmd_arrow(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name arrow [0|1] [width [#]] [length [#]]";
    const char *purpose_string = "toggle arrow drawing, for those objects that support it";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", s->s_arrow);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "0")) {
	s->s_arrow = 0;
	return BRLCAD_OK;
    }
    if (BU_STR_EQUAL(argv[0], "1")) {
	s->s_arrow = 1;
	return BRLCAD_OK;
    }
    if (BU_STR_EQUAL(argv[0], "width"))  {
	if (argc == 2) {
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&s->s_os.s_arrow_tip_width) != 1) {
		bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%f\n", s->s_os.s_arrow_tip_width);
	    return BRLCAD_OK;
	}
    }

    if (BU_STR_EQUAL(argv[0], "length"))  {
	if (argc == 2) {
	    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&s->s_os.s_arrow_tip_length) != 1) {
		bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
		return BRLCAD_ERROR;
	    }
	    return BRLCAD_OK;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%f\n", s->s_os.s_arrow_tip_length);
	    return BRLCAD_OK;
	}
    }

    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
    return BRLCAD_ERROR;
}

int
_objs_cmd_lcnt(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj name lcnt";
    const char *purpose_string = "print the number of vlist entities";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gedp->ged_result_str, "%d\n", bu_list_len(&s->s_vlist));
    return BRLCAD_OK;
}

static void
update_recurse(struct bv_scene_obj *s, struct bview *v, int flags)
{
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *sc = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	update_recurse(sc, v, flags);
    }
    s->s_changed = 0;
    s->s_v = v;
    if (s->s_update_callback)
	(*s->s_update_callback)(s, v, 0);
}

int
_objs_cmd_update(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj update name [x y]";
    const char *purpose_string = "update object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!gd->s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }


    if (argc && (argc != 2)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct bview *v = gd->cv;
    if (argc) {
	int x, y;
	if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&x) != 1 || x < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&y) != 1 || y < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	v->gv_mouse_x = x;
	v->gv_mouse_y = y;
    }

    update_recurse(s, v, 0);

    return BRLCAD_OK;
}

const struct bu_cmdtab _obj_cmds[] = {
    { "draw",       _objs_cmd_draw},
    { "del",        _objs_cmd_delete},
    //{ "info",       _objs_cmd_info},
    { "update",     _objs_cmd_update},
    { "color",      _objs_cmd_color},
    { "axes",       _view_cmd_axes},
    { "arrow",      _objs_cmd_arrow},
    { "label",      _view_cmd_labels},
    { "lcnt",       _objs_cmd_lcnt},
    { "line",       _view_cmd_lines},
    { "polygon",    _view_cmd_polygons},
    { (char *)NULL,      NULL}
};

extern "C" int
_view_cmd_objs(void *bs, int argc, const char **argv)
{
    int help = 0;
    int list_view = 0;
    int list_db = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view [options] obj [options] [args]";
    const char *purpose_string = "manipulate view objects";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
    }

    // See if we have any high level options set
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",        "",  NULL,  &help,      "Print help");
    BU_OPT(d[1], "G", "geom_only",       "",  NULL,  &list_db,  "List view scene objects representing .g database objs");
    BU_OPT(d[2],  "", "view_only",   "",  NULL,  &list_view, "List view-only scene objects (default)");
    BU_OPT_NULL(d[3]);

    gd->gopts = d;

    // We know we're the obj command - start processing args
    argc--; argv++;

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_obj_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    int ac = bu_opt_parse(NULL, acnt, argv, d);

    if (!list_db && !list_view)
	list_view = 1;

    // If we're not wanting help and we have no subcommand, list defined view objects
    struct bview *v = gd->cv;
    if (!ac && cmd_pos < 0 && !help) {
	if (list_db) {
	    struct bu_ptbl *db_objs = bv_view_objs(v, BV_DB_OBJS);
	    for (size_t i = 0; i < BU_PTBL_LEN(db_objs); i++) {
		struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(db_objs, i);
		if (bu_list_len(&cg->s_vlist)) {
		    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&cg->s_name));
		} else {
		    for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
			struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
			bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_name));
		    }
		}
	    }
	}
	if (list_view) {
	    struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
	    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_uuid));
	    }

	    if (view_objs != v->gv_objs.view_objs) {
		for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.view_objs); i++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.view_objs, i);
		    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_uuid));
		}
	    }
	}
	return BRLCAD_OK;
    }

    // We need a name, even if it doesn't exist yet.  Check if it does, since subcommands
    // will react differently based on that status.
    if (ac < 1) {
	bu_vls_printf(gd->gedp->ged_result_str, "need view object name");
	return BRLCAD_ERROR;
    }
    gd->vobj = argv[0];
    gd->s = NULL;
    argc--; argv++;

    // View-only objects come first, unless we're explicitly excluding them by only specifying -G
    if (list_view) {
	struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	    if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_uuid))) {
		gd->s = s;
		break;
	    }
	}
    }

    if (!gd->s) {
	struct bu_ptbl *db_objs = bv_view_objs(v, BV_DB_OBJS);
	for (size_t i = 0; i < BU_PTBL_LEN(db_objs); i++) {
	    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(db_objs, i);
	    if (bu_list_len(&cg->s_vlist)) {
		if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&cg->s_name))) {
		    gd->s = cg;
		    break;
		}
	    } else {
		for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
		    if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_name))) {
			gd->s = s;
			break;
		    }
		}
	    }
	}
    }

    return _ged_subcmd_exec(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_obj_cmds,
			    "view obj", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

