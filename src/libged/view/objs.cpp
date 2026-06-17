/*                        O B J S . C P P
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
/** @file libged/view/objs.c
 *
 * Commands for view features.
 *
 */

#include "common.h"

#include <ctype.h>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

extern "C" {
#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/vls.h"
#include "bsg.h"
#include "bsg/defines.h"
#include "bsg/draw_intent.h"
#include "bsg/draw_source.h"
#include "bsg/export.h"
#include "bsg/render.h"
#include "bsg/scene_object.h"
#include "bsg/field.h"
#include "raytrace.h"
#include "ged/bsg_ged_draw.h"
}
#include "./ged_view.h"
#include "../ged_private.h"

static void
_view_obj_list(struct bu_vls *out, struct bsg_view *v, int list_view, int list_db, int local_only, const char *glob)
{
    if (!out || !v || !ged_draw_view_has_scene_root(v))
	return;

    struct bsg_export_request request;
    bsg_export_request_init(&request, v);
    request.query_flags = 0;
    if (list_view)
	request.query_flags |= BSG_EXPORT_QUERY_VIEW_OBJECTS;
    if (list_db)
	request.query_flags |= BSG_EXPORT_QUERY_DB_OBJECTS;
    if (local_only)
	request.query_flags |= BSG_EXPORT_QUERY_LOCAL_ONLY;
    request.render_flags = BSG_RENDER_FLAG_PAYLOAD_PREPARE;
    request.glob = glob;

    std::set<std::string> names;
    struct bsg_export_result *result = bsg_export_query(&request);
    if (!result)
	return;
    for (size_t i = 0; i < bsg_export_result_count(result); i++) {
	const struct bsg_export_record *rec = bsg_export_result_get(result, i);
	const char *name = rec ? bu_vls_cstr(&rec->path) : NULL;
	if (name && *name)
	    names.insert(std::string(name));
    }
    for (std::set<std::string>::iterator it = names.begin(); it != names.end(); ++it)
	bu_vls_printf(out, "%s\n", it->c_str());
    bsg_export_result_free(result);
}

static const char *
_view_obj_type_from_role(bsg_render_geometry_role role)
{
    switch (role) {
	case BSG_RENDER_GEOMETRY_ROLE_AXES_WIDGET:
	    return "axes";
	case BSG_RENDER_GEOMETRY_ROLE_LINE_SET:
	    return "line";
	case BSG_RENDER_GEOMETRY_ROLE_TEXT_LABEL:
	    return "label";
	case BSG_RENDER_GEOMETRY_ROLE_POLYGON_REGION:
	    return "polygon";
	case BSG_RENDER_GEOMETRY_ROLE_DATABASE_OBJECT:
	    return "gobj";
	default:
	    break;
    }
    return "object";
}

static void
_view_obj_mode_value_string(struct bu_vls *out, int mode)
{
    if (!out) {
	bu_vls_printf(out, "unknown");
	return;
    }
    switch (mode) {
	case BSG_DRAW_MODE_WIRE:
	    bu_vls_printf(out, "wireframe");
	    break;
	case BSG_DRAW_MODE_SHADED_BOTS:
	case BSG_DRAW_MODE_SHADED:
	    bu_vls_printf(out, "shaded");
	    break;
	case BSG_DRAW_MODE_EVAL_WIRE:
	    bu_vls_printf(out, "evaluated_wireframe");
	    break;
	case BSG_DRAW_MODE_HIDDEN_LINE:
	    bu_vls_printf(out, "hidden_line");
	    break;
	case BSG_DRAW_MODE_EVAL_POINTS:
	    bu_vls_printf(out, "evaluated_points");
	    break;
	default:
	    bu_vls_printf(out, "unknown");
	    break;
    }
}

static struct bsg_export_result *
_view_obj_export_find(struct bsg_view *v,
		      const char *name,
		      int list_view,
		      int list_db,
		      int local_only,
		      const struct bsg_export_record **out_rec)
{
    if (out_rec)
	*out_rec = NULL;
    if (!v || !ged_draw_view_has_scene_root(v) || !name || !name[0])
	return NULL;

    struct bsg_export_request request;
    bsg_export_request_init(&request, v);
    request.query_flags = 0;
    if (list_view)
	request.query_flags |= BSG_EXPORT_QUERY_VIEW_OBJECTS;
    if (list_db)
	request.query_flags |= BSG_EXPORT_QUERY_DB_OBJECTS;
    if (local_only)
	request.query_flags |= BSG_EXPORT_QUERY_LOCAL_ONLY;
    request.render_flags = BSG_RENDER_FLAG_PAYLOAD_PREPARE;

    struct bsg_export_result *result = bsg_export_query(&request);
    if (!result)
	return NULL;
    for (size_t i = 0; i < bsg_export_result_count(result); i++) {
	const struct bsg_export_record *rec = bsg_export_result_get(result, i);
	const char *path = rec ? bu_vls_cstr(&rec->path) : NULL;
	if (path && BU_STR_EQUAL(path, name)) {
	    if (out_rec)
		*out_rec = rec;
	    return result;
	}
    }

    bsg_export_result_free(result);
    return NULL;
}

struct view_obj_shape_ref_find_state {
    struct ged *gedp;
    const char *name;
    ged_draw_shape_ref ref;
};

static int
_view_obj_name_matches_record(const struct ged_draw_shape_record *rec,
			      const char *name)
{
    if (!rec || !name || !name[0])
	return 0;
    if (rec->display_name && BU_STR_EQUAL(rec->display_name, name))
	return 1;
    if (rec->leaf_name && BU_STR_EQUAL(rec->leaf_name, name))
	return 1;
    if (rec->fullpath) {
	char *path = db_path_to_string(rec->fullpath);
	if (path) {
	    int match = BU_STR_EQUAL(path, name) ||
		BU_STR_EQUAL(ged_draw_dbpath_skip_lead_slash(path), name);
	    bu_free(path, "view obj fullpath string");
	    if (match)
		return 1;
	}
    }
    return 0;
}

static int
_view_obj_shape_ref_find_cb(const struct ged_draw_shape_record *rec, void *ud)
{
    struct view_obj_shape_ref_find_state *ctx =
	(struct view_obj_shape_ref_find_state *)ud;
    if (!ctx || !ged_draw_shape_ref_is_null(ctx->ref))
	return 0;
    if (_view_obj_name_matches_record(rec, ctx->name)) {
	ctx->ref = rec->ref;
	return 0;
    }
    return 1;
}

static ged_draw_shape_ref
_view_obj_shape_ref_find(struct ged *gedp,
			 struct bsg_view *v,
			 const char *name,
			 int list_view,
			 int list_db,
			 int local_only)
{
    ged_draw_shape_ref null_ref = GED_DRAW_SHAPE_REF_NULL;
    const struct bsg_export_record *export_rec = NULL;
    struct bsg_export_result *export_result =
	_view_obj_export_find(v, name, list_view, list_db, local_only, &export_rec);
    if (!export_rec) {
	bsg_export_result_free(export_result);
	return null_ref;
    }
    bsg_export_result_free(export_result);

    struct view_obj_shape_ref_find_state ctx;
    ctx.gedp = gedp;
    ctx.name = name;
    ctx.ref = null_ref;
    ged_draw_foreach_shape_record(gedp, _view_obj_shape_ref_find_cb, &ctx);
    return ctx.ref;
}

struct view_obj_color_prefix_state {
    struct ged *gedp;
    const struct db_full_path *prefix;
    const unsigned char *rgb;
    int report;
    struct bu_vls *out;
};

static int
_view_obj_color_prefix_cb(const struct ged_draw_shape_record *rec, void *ud)
{
    struct view_obj_color_prefix_state *ctx =
	(struct view_obj_color_prefix_state *)ud;
    if (!ctx || !rec || !rec->fullpath || !ctx->prefix ||
	    !db_full_path_match_top(ctx->prefix, rec->fullpath))
	return 1;

    if (ctx->rgb) {
	(void)ged_draw_shape_ref_set_color(ctx->gedp, rec->ref, ctx->rgb);
	return 1;
    }

    if (ctx->report && ctx->out) {
	unsigned char rgb[3] = {0, 0, 0};
	(void)ged_draw_shape_ref_get_color(ctx->gedp, rec->ref, rgb);
	const char *name = rec->display_name ? rec->display_name : rec->leaf_name;
	if (!name && rec->fullpath)
	    name = "";
	bu_vls_printf(ctx->out, "%s: %d/%d/%d\n", name ? name : "",
		rgb[0], rgb[1], rgb[2]);
    }
    return 1;
}

int
_objs_cmd_draw(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj set <name> draw [0|1|UP|DOWN]";
    const char *purpose_string = "toggle view polygons";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bsg_feature_ref ref = bsg_feature_find(gd->cv, gd->vobj);
    if (!bsg_feature_ref_is_null(ref)) {
	struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
	if (!bsg_feature_style_get(ref, &style)) {
	    bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	    return BRLCAD_ERROR;
	}
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", style.visible ? "UP" : "DOWN");
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[0], "DOWN")) {
	    style.visible = 0;
	    bsg_feature_style_apply(ref, &style);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[0], "UP")) {
	    style.visible = 1;
	    bsg_feature_style_apply(ref, &style);
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (ged_draw_shape_ref_is_null(gd->shape_ref)) {
	bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (argc == 0) {
	struct ged_draw_shape_record rec;
	if (!ged_draw_shape_record_get(gedp, gd->shape_ref, &rec)) {
	    bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	    return BRLCAD_ERROR;
	}
	bu_vls_printf(gedp->ged_result_str, "%s\n", rec.visible ? "UP" : "DOWN");
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "DOWN")) {
	ged_draw_shape_ref_set_visible(gedp, gd->shape_ref, 0);
	return BRLCAD_OK;
    }
    if (BU_STR_EQUAL(argv[0], "UP")) {
	ged_draw_shape_ref_set_visible(gedp, gd->shape_ref, 1);
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
    const char *usage_string = "view obj remove <name>";
    const char *purpose_string = "delete view feature";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bsg_feature_ref ref = bsg_feature_find(gd->cv, gd->vobj);
    if (!bsg_feature_ref_is_null(ref)) {
	if (!bsg_feature_remove(gd->cv, gd->vobj)) {
	    bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	    return BRLCAD_ERROR;
	}
	return BRLCAD_OK;
    }

    if (ged_draw_shape_ref_is_null(gd->shape_ref)) {
	bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    struct ged_draw_shape_record rec;
    if (!ged_draw_shape_record_get(gedp, gd->shape_ref, &rec)) {
	bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    if (rec.fullpath) {
	bu_vls_printf(gedp->ged_result_str, "View feature %s is associated with a database object - use 'erase' cmd to clear\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    ged_draw_shape_ref_release(gedp, gd->shape_ref);

    return BRLCAD_OK;
}

int
_objs_cmd_color(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj set <name> color [r/g/b]";
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

    bsg_feature_ref ref = bsg_feature_find(gd->cv, gd->vobj);
    if (!bsg_feature_ref_is_null(ref) && !(recurse && ac == 0)) {
	struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
	if (!bsg_feature_style_get(ref, &style)) {
	    bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	    return BRLCAD_ERROR;
	}
	if (ac == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n",
		    style.color[0], style.color[1], style.color[2]);
	    return BRLCAD_OK;
	}

	struct bu_color val;
	if (bu_opt_color(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	bu_color_to_rgb_chars(&val, style.color);
	style.color_valid = 1;
	if (recurse)
	    bsg_feature_style_apply_recursive(ref, &style);
	else
	    bsg_feature_style_apply(ref, &style);
	return BRLCAD_OK;
    }

    if (ged_draw_shape_ref_is_null(gd->shape_ref)) {
	bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    struct ged_draw_shape_record shape_rec;
    if (!ged_draw_shape_record_get(gedp, gd->shape_ref, &shape_rec)) {
	bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    if (ac == 0) {
	unsigned char rgb[3] = {0, 0, 0};
	(void)ged_draw_shape_ref_get_color(gedp, gd->shape_ref, rgb);
	bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", rgb[0], rgb[1], rgb[2]);
	if (recurse && shape_rec.fullpath) {
	    struct view_obj_color_prefix_state ctx;
	    ctx.gedp = gedp;
	    ctx.prefix = shape_rec.fullpath;
	    ctx.rgb = NULL;
	    ctx.report = 1;
	    ctx.out = gedp->ged_result_str;
	    ged_draw_foreach_shape_record(gedp, _view_obj_color_prefix_cb, &ctx);
	}
	return BRLCAD_OK;
    }
    struct bu_color val;
    if (bu_opt_color(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    unsigned char rgb[3] = {0, 0, 0};
    bu_color_to_rgb_chars(&val, rgb);
    (void)ged_draw_shape_ref_set_color(gedp, gd->shape_ref, rgb);
    if (recurse && shape_rec.fullpath) {
	struct view_obj_color_prefix_state ctx;
	ctx.gedp = gedp;
	ctx.prefix = shape_rec.fullpath;
	ctx.rgb = rgb;
	ctx.report = 0;
	ctx.out = NULL;
	ged_draw_foreach_shape_record(gedp, _view_obj_color_prefix_cb, &ctx);
    }
    return BRLCAD_OK;
}

int
_objs_cmd_arrow(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj set <name> arrow [0|1] [width [#]] [length [#]]";
    const char *purpose_string = "toggle arrow drawing, for those objects that support it";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bsg_feature_ref ref = bsg_feature_find(gd->cv, gd->vobj);
    if (!bsg_feature_ref_is_null(ref)) {
	struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
	if (!bsg_feature_style_get(ref, &style)) {
	    bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	    return BRLCAD_ERROR;
	}
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d\n", style.arrow > 0 ? 1 : 0);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[0], "0") || BU_STR_EQUAL(argv[0], "1")) {
	    style.arrow = BU_STR_EQUAL(argv[0], "1") ? 1 : 0;
	    bsg_feature_style_apply(ref, &style);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[0], "width"))  {
	    if (argc == 2) {
		fastf_t width = 0.0;
		if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&width) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
		    return BRLCAD_ERROR;
		}
		style.arrow_tip_width = width;
		bsg_feature_style_apply(ref, &style);
		return BRLCAD_OK;
	    }
	    bu_vls_printf(gedp->ged_result_str, "%f\n", style.arrow_tip_width);
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[0], "length"))  {
	    if (argc == 2) {
		fastf_t length = 0.0;
		if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&length) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
		    return BRLCAD_ERROR;
		}
		style.arrow_tip_length = length;
		bsg_feature_style_apply(ref, &style);
		return BRLCAD_OK;
	    }
	    bu_vls_printf(gedp->ged_result_str, "%f\n", style.arrow_tip_length);
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str,
	    "View feature %s does not support arrow settings\n", gd->vobj);
    return BRLCAD_ERROR;
}

int
_objs_cmd_lcnt(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj info <name> lcnt";
    const char *purpose_string = "print the number of vlist entities";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    const struct bsg_export_record *rec = NULL;
    struct bsg_export_result *export_result =
	_view_obj_export_find(gd->cv, gd->vobj, 1, 1, gd->local_obj, &rec);
    if (rec) {
	bu_vls_printf(gedp->ged_result_str, "%zu\n", rec->vlist_structure_count);
	bsg_export_result_free(export_result);
	return BRLCAD_OK;
    }
    bsg_export_result_free(export_result);
    bu_vls_printf(gedp->ged_result_str, "0\n");
    return BRLCAD_OK;
}

int
_objs_cmd_update(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view obj set <name> update [x y]";
    const char *purpose_string = "update object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc && (argc != 2)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return BRLCAD_ERROR;
    }

    struct bsg_view *v = gd->cv;
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
	bsg_screen_pt(&v->gv_point, x, y, v);
    }

    bsg_feature_ref ref = bsg_feature_find(gd->cv, gd->vobj);
    if (!bsg_feature_ref_is_null(ref)) {
	bsg_feature_realize(ref, v, 1);
	return BRLCAD_OK;
    }

    if (ged_draw_shape_ref_is_null(gd->shape_ref)) {
	bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    ged_draw_shape_ref_realize(gedp, gd->shape_ref, v);

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
    int not_shared = 0;
    struct bu_vls gobj_path = BU_VLS_INIT_ZERO;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view [options] obj [options] [args]";
    const char *purpose_string = "manipulate view features";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
    }
    if (!ged_draw_view_has_scene_root(gd->cv)) {
	struct bsg_view *cv = gedp->ged_gvp;
	gedp->ged_gvp = gd->cv;
	ged_draw_ensure_root(gedp);
	gedp->ged_gvp = cv;
    }

    // See if we have any high level options set
    struct bu_opt_desc d[6];
    BU_OPT(d[0], "h", "help",        "",  NULL,  &help,       "Print help");
    BU_OPT(d[1], "L", "local",       "",  NULL,  &not_shared, "Object is scoped only to current/specified view");
    BU_OPT(d[2], "G", "geom_only",   "",  NULL,  &list_db,    "List view scene objects representing .g database objs");
    BU_OPT(d[3],  "", "view_only",   "",  NULL,  &list_view,  "List view-scoped features (default)");
    BU_OPT(d[4], "g", "gobj",        "dbpath",  &bu_opt_vls, &gobj_path, "Use geometry path for gobj create");
    BU_OPT_NULL(d[5]);

    gd->gopts = d;

    // We know we're the obj command - start processing args
    argc--; argv++;

    std::set<std::string> unified_cmds = {"create", "remove", "list", "info", "set"};

    // High level options are only defined prior to the subcommand.  Find
    // the first non-option argument to check against the unified subcommand set.
    int first_pos = -1;
    int arg_idx = 0;
    while (arg_idx < argc) {
	if (argv[arg_idx][0] == '-') {
	    if ((BU_STR_EQUAL(argv[arg_idx], "-g") || BU_STR_EQUAL(argv[arg_idx], "--gobj")) && arg_idx + 1 < argc) {
		arg_idx += 2;
		continue;
	    }
	    arg_idx++;
	    continue;
	}
	first_pos = arg_idx;
	break;
    }

    int cmd_pos = -1;
    if (first_pos >= 0 && unified_cmds.find(std::string(argv[first_pos])) != unified_cmds.end())
	cmd_pos = first_pos;

    int acnt = (first_pos >= 0) ? first_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    if (!list_db && !list_view)
	list_view = 1;

    gd->local_obj = not_shared;
    gd->gobj_dbpath = bu_vls_strlen(&gobj_path) ? bu_vls_cstr(&gobj_path) : NULL;

    struct bsg_view *v = gd->cv;
    if (help) {
	int hargc = (cmd_pos >= 0) ? argc - cmd_pos : 0;
	const char **hargv = (cmd_pos >= 0) ? &argv[cmd_pos] : NULL;
	_ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_obj_cmds, "view obj", "[options] subcommand [args]", gd, hargc, hargv);
	bu_vls_free(&gobj_path);
	return BRLCAD_OK;
    }

    // No subcommand: default list (only when there are no positional args at all)
    if (cmd_pos < 0) {
	if (first_pos >= 0) {
	    bu_vls_free(&gobj_path);
	    bu_vls_printf(gd->gedp->ged_result_str,
		    "Unsupported subcommand '%s' (valid: create, remove, list, info, set)",
		    argv[first_pos]);
	    return BRLCAD_ERROR;
	}
	_view_obj_list(gd->gedp->ged_result_str, v, list_view, list_db, gd->local_obj, NULL);
	bu_vls_free(&gobj_path);
	return BRLCAD_OK;
    }

    if (cmd_pos >= argc || cmd_pos < 0) {
	bu_vls_free(&gobj_path);
	bu_vls_printf(gd->gedp->ged_result_str, "need subcommand");
	return BRLCAD_ERROR;
    }

    int subcmd_argc = argc - cmd_pos;
    const char **subcmd_argv = argv + cmd_pos;

    // Unified grammar
    if (unified_cmds.find(std::string(subcmd_argv[0])) != unified_cmds.end()) {
	const char *ucmd = subcmd_argv[0];
	if (BU_STR_EQUAL(ucmd, "list")) {
	    const char *glob = (subcmd_argc > 1) ? subcmd_argv[1] : NULL;
	    if (subcmd_argc > 2) {
		bu_vls_free(&gobj_path);
		bu_vls_printf(gd->gedp->ged_result_str, "Usage: view obj [-V view] [-L] list [glob_pattern]");
		return BRLCAD_ERROR;
	    }
	    _view_obj_list(gd->gedp->ged_result_str, v, list_view, list_db, gd->local_obj, glob);
	    bu_vls_free(&gobj_path);
	    return BRLCAD_OK;
	}

	if (BU_STR_EQUAL(ucmd, "create")) {
	    if (gd->gobj_dbpath) {
		if (subcmd_argc != 2) {
		    bu_vls_free(&gobj_path);
		    bu_vls_printf(gd->gedp->ged_result_str, "Usage: view obj [-V view] [-L] -g <dbpath> create <name>");
		    return BRLCAD_ERROR;
		}
		const char *gargv[4] = {"create", gd->gobj_dbpath, subcmd_argv[1], NULL};
		int ret = _gobjs_cmd_create(bs, 3, gargv);
		bu_vls_free(&gobj_path);
		return ret;
	    }
	    if (subcmd_argc < 3) {
		bu_vls_free(&gobj_path);
		bu_vls_printf(gd->gedp->ged_result_str, "Usage: view obj [-V view] [-L] create <name> <type> <args...>");
		return BRLCAD_ERROR;
	    }
	    gd->vobj = subcmd_argv[1];
	    const int find_view_objs = 1;
	    const int find_db_objs = 1;
	    gd->shape_ref = _view_obj_shape_ref_find(gedp, v, gd->vobj, find_view_objs, find_db_objs, gd->local_obj);
	    const char *otype = subcmd_argv[2];
	    const char **cargv = subcmd_argv + 2;
	    int cargc = subcmd_argc - 2;
	    int ret = BRLCAD_ERROR;
	    if (BU_STR_EQUAL(otype, "line")) {
		ret = _view_cmd_lines(bs, cargc, cargv);
	    } else if (BU_STR_EQUAL(otype, "axes")) {
		ret = _view_cmd_axes(bs, cargc, cargv);
	    } else if (BU_STR_EQUAL(otype, "label")) {
		ret = _view_cmd_labels(bs, cargc, cargv);
	    } else if (BU_STR_EQUAL(otype, "polygon")) {
		ret = _view_cmd_polygons(bs, cargc, cargv);
	    } else if (BU_STR_EQUAL(otype, "arrow")) {
		if (cargc < 5) {
		    bu_vls_printf(gd->gedp->ged_result_str, "Usage: view obj [-V view] [-L] create <name> arrow x y z");
		    bu_vls_free(&gobj_path);
		    return BRLCAD_ERROR;
		}
		ret = _view_cmd_lines(bs, cargc, cargv);
		if (ret == BRLCAD_OK) {
		    gd->shape_ref = _view_obj_shape_ref_find(gedp, v, gd->vobj, 1, 1, gd->local_obj);
		    const char *aargv[3] = {"arrow", "1", NULL};
		    ret = _objs_cmd_arrow(bs, 2, aargv);
		}
	    } else {
		bu_vls_printf(gd->gedp->ged_result_str, "Unsupported view feature type %s", otype);
		bu_vls_free(&gobj_path);
		return BRLCAD_ERROR;
	    }
	    bu_vls_free(&gobj_path);
	    return ret;
	}

	if (subcmd_argc < 2) {
	    bu_vls_free(&gobj_path);
	    bu_vls_printf(gd->gedp->ged_result_str, "Usage: view obj [-V view] [-L] %s <name>", ucmd);
	    return BRLCAD_ERROR;
	}
	gd->vobj = subcmd_argv[1];
	gd->shape_ref = _view_obj_shape_ref_find(gedp, v, gd->vobj, list_view, list_db, gd->local_obj);

	if (BU_STR_EQUAL(ucmd, "remove")) {
	    const char *rargv[2] = {"del", NULL};
	    int ret = _objs_cmd_delete(bs, 1, rargv);
	    bu_vls_free(&gobj_path);
	    return ret;
	}

	if (BU_STR_EQUAL(ucmd, "info")) {
	    const struct bsg_export_record *rec = NULL;
	    struct bsg_export_result *export_result =
		_view_obj_export_find(v, gd->vobj, list_view, list_db, gd->local_obj, &rec);
	    if (!rec) {
		bu_vls_free(&gobj_path);
		bu_vls_printf(gd->gedp->ged_result_str, "No view feature named %s\n", gd->vobj);
		return BRLCAD_ERROR;
	    }
	    if (subcmd_argc == 2) {
		bu_vls_printf(gedp->ged_result_str, "%s %s\n", gd->vobj, _view_obj_type_from_role(rec->source.geometry_role));
		bsg_export_result_free(export_result);
		bu_vls_free(&gobj_path);
		return BRLCAD_OK;
	    }
	    if (BU_STR_EQUAL(subcmd_argv[2], "mode")) {
		_view_obj_mode_value_string(gedp->ged_result_str, rec->draw_mode);
		bsg_export_result_free(export_result);
		bu_vls_free(&gobj_path);
		return BRLCAD_OK;
	    }
	    if (BU_STR_EQUAL(subcmd_argv[2], "color")) {
		bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n",
			rec->color[0], rec->color[1], rec->color[2]);
		bsg_export_result_free(export_result);
		bu_vls_free(&gobj_path);
		return BRLCAD_OK;
	    }
	    if (BU_STR_EQUAL(subcmd_argv[2], "draw")) {
		bu_vls_printf(gedp->ged_result_str, "%s\n", rec->visible ? "UP" : "DOWN");
		bsg_export_result_free(export_result);
		bu_vls_free(&gobj_path);
		return BRLCAD_OK;
	    }
	    if (BU_STR_EQUAL(subcmd_argv[2], "lcnt")) {
		bu_vls_printf(gedp->ged_result_str, "%zu\n", rec->vlist_structure_count);
		bsg_export_result_free(export_result);
		bu_vls_free(&gobj_path);
		return BRLCAD_OK;
	    }
	    if (BU_STR_EQUAL(subcmd_argv[2], "type")) {
		bu_vls_printf(gedp->ged_result_str, "%s\n", _view_obj_type_from_role(rec->source.geometry_role));
		bsg_export_result_free(export_result);
		bu_vls_free(&gobj_path);
		return BRLCAD_OK;
	    }
	    bsg_export_result_free(export_result);
	    bu_vls_free(&gobj_path);
	    bu_vls_printf(gd->gedp->ged_result_str, "Unsupported info field %s", subcmd_argv[2]);
	    return BRLCAD_ERROR;
	}

	if (BU_STR_EQUAL(ucmd, "set")) {
	    bsg_feature_ref set_ref = bsg_feature_find(v, gd->vobj);
	    if ((bsg_feature_ref_is_null(set_ref) && ged_draw_shape_ref_is_null(gd->shape_ref)) || subcmd_argc < 4) {
		bu_vls_free(&gobj_path);
		bu_vls_printf(gd->gedp->ged_result_str, "Usage: view obj [-V view] [-L] set <name> <field> <value>");
		return BRLCAD_ERROR;
	    }
	    if (BU_STR_EQUAL(subcmd_argv[2], "draw")) {
		if (subcmd_argc == 4 && (BU_STR_EQUAL(subcmd_argv[3], "0") || BU_STR_EQUAL(subcmd_argv[3], "1"))) {
		    const char *dargv[3] = {"draw", BU_STR_EQUAL(subcmd_argv[3], "1") ? "UP" : "DOWN", NULL};
		    int ret = _objs_cmd_draw(bs, 2, dargv);
		    bu_vls_free(&gobj_path);
		    return ret;
		}
		int ret = _objs_cmd_draw(bs, subcmd_argc - 2, subcmd_argv + 2);
		bu_vls_free(&gobj_path);
		return ret;
	    }
	    if (BU_STR_EQUAL(subcmd_argv[2], "color")) {
		int ret = _objs_cmd_color(bs, subcmd_argc - 2, subcmd_argv + 2);
		bu_vls_free(&gobj_path);
		return ret;
	    }
	    if (BU_STR_EQUAL(subcmd_argv[2], "arrow")) {
		int ret = _objs_cmd_arrow(bs, subcmd_argc - 2, subcmd_argv + 2);
		bu_vls_free(&gobj_path);
		return ret;
	    }
	    if (BU_STR_EQUAL(subcmd_argv[2], "update")) {
		int ret = _objs_cmd_update(bs, subcmd_argc - 2, subcmd_argv + 2);
		bu_vls_free(&gobj_path);
		return ret;
	    }
	    bu_vls_free(&gobj_path);
	    bu_vls_printf(gd->gedp->ged_result_str, "Unsupported set field %s", subcmd_argv[2]);
	    return BRLCAD_ERROR;
	}
    }

    bu_vls_free(&gobj_path);
    const char *bad_subcmd = (cmd_pos >= 0 && cmd_pos < argc) ? argv[cmd_pos] : "(none)";
    bu_vls_printf(gd->gedp->ged_result_str,
	    "Unsupported subcommand %s (valid: create, remove, list, info, set)",
	    bad_subcmd);
    return BRLCAD_ERROR;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
