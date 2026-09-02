/*                    A N N O T A T E . C P P
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
/** @file libged/annotate/annotate.cpp
 *
 * Creation and inspection of persistent annotation objects.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bu/color.h"
#include "bu/cmd.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/units.h"
#include "rt/geom.h"
#include "rt/primitives/annot.h"
#include "wdb.h"

#include "../dbi.h"
#include "../ged_private.h"


namespace {

constexpr fastf_t DEFAULT_TEXT_SCALE = 0.025;
constexpr fastf_t DEFAULT_OFFSET_SCALE = 0.05;
constexpr fastf_t DEFAULT_ARROW_WING_SCALE = 0.268; /* tan(15 degrees) */
constexpr fastf_t DEFAULT_EXTENSION_OVERSHOOT_SCALE = 0.5;
constexpr fastf_t DEFAULT_LEADER_X = 6.0;
constexpr fastf_t DEFAULT_LEADER_Y = 3.0;
constexpr fastf_t DEFAULT_LEADER_MARGIN_SCALE = 2.0;
constexpr fastf_t DEFAULT_SCREEN_TEXT_HEIGHT_MM = 3.0;
/* The display-plane renderer uses this nominal density.  --dpi makes the
 * physical size explicit for displays whose effective density differs. */
constexpr fastf_t DEFAULT_SCREEN_DPI = RT_ANNOT_SCREEN_DPI;
constexpr fastf_t DISPLAY_PIXELS_PER_MM = RT_ANNOT_SCREEN_PIXELS_PER_MM;
constexpr double AUTODIM_TEXT_WIDTH_SCALE = 0.65;
/* Layout penalties deliberately dominate the smaller rewards so avoiding
 * collisions and crossings wins over merely moving a label farther out. */
constexpr double AUTODIM_OUTWARD_WEIGHT = 4.0;
constexpr double AUTODIM_ORTHOGONALITY_WEIGHT = 2.0;
constexpr double AUTODIM_MODEL_OVERLAP_PENALTY = 8.0;
constexpr double AUTODIM_EDGE_ON_PENALTY = 20.0;
constexpr double AUTODIM_MIN_PROJECTED_AXIS_RATIO = 0.02;
constexpr double AUTODIM_LABEL_OVERLAP_PENALTY = 30.0;
constexpr double AUTODIM_LINE_CROSSING_PENALTY = 12.0;
constexpr double AUTODIM_MAX_SEPARATION_REWARD = 1.0;
constexpr double AUTODIM_SCORE_TOLERANCE = 1.0e-9;
constexpr int DEFAULT_PRECISION = 2;
constexpr int DEFAULT_ARC_SEGMENTS = 24;

const char *ATTR_KIND = "annotate:kind";
const char *ATTR_MEMBERS = "annotate:members";
const char *ATTR_SOURCES = "annotate:sources";
const char *ATTR_COLOR = "rgb";
const char *ATTR_BOUNDS = "annotate:bounds";
const char *ATTR_BOX_CORNERS = "annotate:box-corners";
const char *ATTR_AXES = "annotate:axes";
const char *ATTR_CORNER = "annotate:corner";
const char *ATTR_TIGHT = "annotate:tight";
const char *ATTR_NO_AIR = "annotate:no-air";
const char *ATTR_TEXT_HEIGHT = "annotate:text-height";
const char *ATTR_TEXT_HEIGHT_AUTO = "annotate:text-height-auto";
const char *ATTR_OFFSET = "annotate:offset";
const char *ATTR_OFFSET_AUTO = "annotate:offset-auto";
const char *ATTR_UNITS = "annotate:units";
const char *ATTR_PRECISION = "annotate:precision";
const char *ATTR_AXIS_LABELS = "annotate:axis-labels";
const char *ATTR_FONT = "annotate:font";
const char *ATTR_LINE_WIDTH = "annotate:line-width";
const char *ATTR_LINE_STYLE = "annotate:line-style";
const char *ATTR_BOLD = "annotate:bold";
const char *ATTR_ITALIC = "annotate:italic";
const char *ATTR_LEADER_TEXT = "annotate:leader-text";
const char *ATTR_LEADER_TARGET = "annotate:leader-target";
const char *ATTR_LEADER_TARGET_AUTO = "annotate:leader-target-auto";
const char *ATTR_LEADER_AT = "annotate:leader-at";
const char *ATTR_LEADER_AT_AUTO = "annotate:leader-at-auto";
const char *ATTR_LEADER_SCREEN_SPACE = "annotate:leader-screen-space";
const char *ATTR_LEADER_DPI = "annotate:leader-dpi";

#define ANNOT_OPT(_descs, _index, _so, _lo, _ahelp, _aprocess, _var, _help) do { \
    BU_OPT((_descs)[(_index)], _so, _lo, _ahelp, _aprocess, _var, _help); \
    (_index)++; \
} while (0)


struct color_option {
    struct bu_color value = BU_COLOR_INIT_ZERO;
    bool set = false;
};


static int
parse_color(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    color_option *color = static_cast<color_option *>(set_var);
    int consumed = bu_opt_color(msg, argc, argv, &color->value);
    if (consumed > 0)
	color->set = true;
    return consumed;
}


static int
parse_vls_value(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    if (!argc || !argv || !argv[0]) {
	bu_vls_printf(msg, "Option requires a value");
	return -1;
    }
    struct bu_vls *value = static_cast<struct bu_vls *>(set_var);
    if (value)
	bu_vls_strcpy(value, argv[0]);
    return 1;
}


struct create_options {
    int help = 0;
    int no_draw = 0;
    int bold = 0;
    int italic = 0;
    int frame = 0;
    int no_extension_lines = 0;
    int no_axis_labels = 0;
    int no_air = 0;
    int tight = 0;
    int screen_space = 0;
    int precision = DEFAULT_PRECISION;
    fastf_t text_height = 0.0;
    fastf_t dpi = DEFAULT_SCREEN_DPI;
    fastf_t line_width = 0.0;
    fastf_t offset = NAN;
    point_t at = {NAN, NAN, NAN};
    point_t target = {NAN, NAN, NAN};
    point_t from = {NAN, NAN, NAN};
    point_t to = {NAN, NAN, NAN};
    point_t vertex = {NAN, NAN, NAN};
    point_t center = {NAN, NAN, NAN};
    point_t origin = {NAN, NAN, NAN};
    struct bu_vls plane = BU_VLS_INIT_ZERO;
    struct bu_vls font = BU_VLS_INIT_ZERO;
    struct bu_vls line_style = BU_VLS_INIT_ZERO;
    struct bu_vls units = BU_VLS_INIT_ZERO;
    struct bu_vls prefix = BU_VLS_INIT_ZERO;
    struct bu_vls suffix = BU_VLS_INIT_ZERO;
    struct bu_vls axes = BU_VLS_INIT_ZERO;
    struct bu_vls corner = BU_VLS_INIT_ZERO;
    struct bu_vls axis = BU_VLS_INIT_ZERO;
    struct bu_vls bounds = BU_VLS_INIT_ZERO;
    struct bu_vls associated_object = BU_VLS_INIT_ZERO;
    color_option color;

    create_options()
    {
	bu_vls_strcpy(&plane, "xy");
	bu_vls_strcpy(&font, "osifont");
	bu_vls_strcpy(&line_style, "continuous");
	bu_vls_strcpy(&axes, "x,y,z");
	bu_vls_strcpy(&corner, "auto");
	bu_vls_strcpy(&bounds, "aabb");
    }

    ~create_options()
    {
	bu_vls_free(&plane);
	bu_vls_free(&font);
	bu_vls_free(&line_style);
	bu_vls_free(&units);
	bu_vls_free(&prefix);
	bu_vls_free(&suffix);
	bu_vls_free(&axes);
	bu_vls_free(&corner);
	bu_vls_free(&axis);
	bu_vls_free(&bounds);
	bu_vls_free(&associated_object);
    }
};


static bool
point_is_set(const point_t p)
{
    return std::isfinite(p[X]) && std::isfinite(p[Y]) && std::isfinite(p[Z]);
}


static void
to_base(point_t p, const struct ged *gedp)
{
    VSCALE(p, p, gedp->dbip->dbi_local2base);
}


static uint32_t
line_pattern(const create_options &opts)
{
    const char *pattern = bu_vls_cstr(&opts.line_style);
    if (BU_STR_EQUAL(pattern, "dashed"))
	return RT_ANNOT_LINE_DASHED;
    if (BU_STR_EQUAL(pattern, "dotted"))
	return RT_ANNOT_LINE_DOTTED;
    if (BU_STR_EQUAL(pattern, "center"))
	return RT_ANNOT_LINE_CENTER;
    if (BU_STR_EQUAL(pattern, "phantom"))
	return RT_ANNOT_LINE_PHANTOM;
    return RT_ANNOT_LINE_CONTINUOUS;
}


static bool
valid_line_pattern(const create_options &opts)
{
    const char *pattern = bu_vls_cstr(&opts.line_style);
    return BU_STR_EQUAL(pattern, "continuous") || BU_STR_EQUAL(pattern, "dashed") ||
	BU_STR_EQUAL(pattern, "dotted") || BU_STR_EQUAL(pattern, "center") ||
	BU_STR_EQUAL(pattern, "phantom");
}


static void
set_plane(vect_t u, vect_t v, const char *plane)
{
    if (BU_STR_EQUAL(plane, "xz")) {
	VSET(u, 1.0, 0.0, 0.0);
	VSET(v, 0.0, 0.0, 1.0);
    } else if (BU_STR_EQUAL(plane, "yz")) {
	VSET(u, 0.0, 1.0, 0.0);
	VSET(v, 0.0, 0.0, 1.0);
    } else {
	VSET(u, 1.0, 0.0, 0.0);
	VSET(v, 0.0, 1.0, 0.0);
    }
}


class annotation_builder {
public:
    annotation_builder(const point_t anchor, const vect_t u, const vect_t v,
		       bool screen_space = false)
    {
	VMOVE(anchor_, anchor);
	VMOVE(u_, u);
	VMOVE(v_, v);
	screen_space_ = screen_space;
    }

    ~annotation_builder()
    {
	for (void *segment : segments_) {
	    uint32_t magic = *static_cast<uint32_t *>(segment);
	    if (magic == ANN_TSEG_MAGIC) {
		struct txt_seg *text = static_cast<struct txt_seg *>(segment);
		bu_vls_free(&text->label);
		BU_PUT(text, struct txt_seg);
	    } else if (magic == CURVE_LSEG_MAGIC) {
		struct line_seg *line = static_cast<struct line_seg *>(segment);
		BU_PUT(line, struct line_seg);
	    }
	}
	for (struct rt_annot_seg_style &style : styles_) {
	    if (style.font)
		bu_free(style.font, "annotation font");
	    if (style.symbol)
		bu_free(style.symbol, "annotation symbol");
	}
    }

    int vertex(fastf_t x, fastf_t y)
    {
	vertices_.push_back({x, y});
	return static_cast<int>(vertices_.size() - 1);
    }

    void line(int start, int end, uint32_t role, const create_options &opts)
    {
	struct line_seg *segment;
	BU_ALLOC(segment, struct line_seg);
	segment->magic = CURVE_LSEG_MAGIC;
	segment->start = start;
	segment->end = end;
	segments_.push_back(segment);
	styles_.push_back(make_style(role, opts, false));
    }

    void text(int ref, const std::string &label, fastf_t height, int position,
	      const create_options &opts)
    {
	struct txt_seg *segment;
	BU_ALLOC(segment, struct txt_seg);
	segment->magic = ANN_TSEG_MAGIC;
	segment->ref_pt = ref;
	segment->rel_pos = position;
	bu_vls_init(&segment->label);
	bu_vls_strcpy(&segment->label, label.c_str());
	segment->txt_size = height;
	segment->txt_rot_angle = 0.0;
	segments_.push_back(segment);
	styles_.push_back(make_style(RT_ANNOT_ROLE_TEXT, opts, true));
    }

    int write(struct ged *gedp, const char *name)
    {
	struct rt_annot_internal annotation = {};
	std::vector<int> reverse(segments_.size(), 0);
	annotation.magic = RT_ANNOT_INTERNAL_MAGIC;
	VMOVE(annotation.V, anchor_);
	annotation.flags = screen_space_ ? RT_ANNOT_SCREEN_SPACE : RT_ANNOT_MODEL_SPACE;
	VMOVE(annotation.u_vec, u_);
	VMOVE(annotation.v_vec, v_);
	annotation.vert_count = vertices_.size();
	annotation.verts = reinterpret_cast<point2d_t *>(vertices_.data());
	annotation.ant.count = segments_.size();
	annotation.ant.reverse = reverse.data();
	annotation.ant.segments = segments_.data();
	annotation.styles = styles_.data();

	struct bu_vls validation = BU_VLS_INIT_ZERO;
	if (rt_annot_validate(&annotation, &validation)) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid annotation: %s",
		bu_vls_cstr(&validation));
	    bu_vls_free(&validation);
	    return BRLCAD_ERROR;
	}
	bu_vls_free(&validation);

	struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	return mk_annot(wdbp, name, &annotation) ? BRLCAD_ERROR : BRLCAD_OK;
    }

private:
    static struct rt_annot_seg_style
    make_style(uint32_t role, const create_options &opts, bool is_text)
    {
	struct rt_annot_seg_style style = {};
	style.role = role;
	style.line_pattern = line_pattern(opts);
	if (opts.line_width > 0.0) {
	    style.flags |= RT_ANNOT_STYLE_WIDTH;
	    style.line_width = opts.line_width;
	}
	if (opts.color.set) {
	    style.flags |= RT_ANNOT_STYLE_COLOR;
	    bu_color_to_rgb_chars(&opts.color.value, style.color);
	    style.color[3] = 255;
	}
	if (is_text) {
	    if (bu_vls_strlen(&opts.font))
		style.font = bu_strdup(bu_vls_cstr(&opts.font));
	    if (opts.bold)
		style.flags |= RT_ANNOT_STYLE_BOLD;
	    if (opts.italic)
		style.flags |= RT_ANNOT_STYLE_ITALIC;
	}
	return style;
    }

    point_t anchor_;
    vect_t u_;
    vect_t v_;
    bool screen_space_ = false;
    std::vector<std::array<fastf_t, 2>> vertices_;
    std::vector<void *> segments_;
    std::vector<struct rt_annot_seg_style> styles_;
};


static int
add_common_options(struct bu_opt_desc *descs, int index, create_options &opts)
{
    ANNOT_OPT(descs, index, "h", "help", "", NULL, &opts.help, "Print help");
    ANNOT_OPT(descs, index, "", "text-height", "size", &bu_opt_fastf_t,
	&opts.text_height, "Text height in current units");
    ANNOT_OPT(descs, index, "", "font", "name", &parse_vls_value, &opts.font,
	"TrueType font name or path");
    ANNOT_OPT(descs, index, "C", "color", "r/g/b", &parse_color, &opts.color,
	"Annotation color");
    ANNOT_OPT(descs, index, "", "line-width", "width", &bu_opt_fastf_t,
	&opts.line_width, "Line width");
    ANNOT_OPT(descs, index, "", "line-style", "style", &parse_vls_value,
	&opts.line_style, "continuous, dashed, dotted, center, or phantom");
    ANNOT_OPT(descs, index, "", "bold", "", NULL, &opts.bold, "Use bold text");
    ANNOT_OPT(descs, index, "", "italic", "", NULL, &opts.italic, "Use italic text");
    ANNOT_OPT(descs, index, "", "for", "object", &parse_vls_value,
	&opts.associated_object, "Associate the annotation with a geometry object");
    ANNOT_OPT(descs, index, "D", "no-draw", "", NULL, &opts.no_draw,
	"Do not draw the created annotation");
    return index;
}


static void
print_option_help(struct bu_vls *result, const char *usage,
		  const struct bu_opt_desc *descs, const char *description = NULL)
{
    bu_vls_printf(result, "%s", usage);
    if (description)
	bu_vls_printf(result, "\n\n%s", description);
    char *option_help = bu_opt_describe(descs, NULL);
    if (option_help) {
	bu_vls_printf(result, "\n\nOptions:\n%s", option_help);
	bu_free(option_help, "annotate option help");
    }
}


static bool
annotate_command_messages(struct ged *gedp, int argc, const char **argv,
			  const char *usage, const char *purpose)
{
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gedp->ged_result_str, "%s\n%s\n", usage, purpose);
	return true;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", purpose);
	return true;
    }
    return false;
}


static int
parse_options(struct ged *gedp, int argc, const char **argv,
	      struct bu_opt_desc *descs, create_options &opts)
{
    struct bu_vls message = BU_VLS_INIT_ZERO;
    int remaining = bu_opt_parse(&message, argc, argv, descs);
    if (remaining < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&message));
	bu_vls_free(&message);
	return -1;
    }
    bu_vls_free(&message);
    if (!valid_line_pattern(opts)) {
	bu_vls_printf(gedp->ged_result_str,
	    "Unknown line style '%s'", bu_vls_cstr(&opts.line_style));
	return -1;
    }
    if (opts.precision < 0 || opts.precision > 15) {
	bu_vls_printf(gedp->ged_result_str, "Precision must be between 0 and 15");
	return -1;
    }
    if (!std::isfinite(opts.text_height) || !std::isfinite(opts.line_width) ||
	opts.text_height < 0.0 || opts.line_width < 0.0 ||
	std::isinf(opts.offset) ||
	(std::isfinite(opts.offset) && opts.offset < 0.0)) {
	bu_vls_printf(gedp->ged_result_str,
	    "Text height and line width must be finite and non-negative; offset must be finite and non-negative when specified");
	return -1;
    }
    if (!std::isfinite(opts.dpi) || opts.dpi <= 0.0) {
	bu_vls_printf(gedp->ged_result_str, "DPI must be positive");
	return -1;
    }
    if (bu_vls_strlen(&opts.associated_object) &&
	db_lookup(gedp->dbip, bu_vls_cstr(&opts.associated_object),
	    LOOKUP_QUIET) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Associated object '%s' does not exist",
	    bu_vls_cstr(&opts.associated_object));
	return -1;
    }
    return remaining;
}


static int
prepare_name(struct ged *gedp, const char *name)
{
    if (!name || !name[0]) {
	bu_vls_printf(gedp->ged_result_str, "An annotation name is required");
	return BRLCAD_ERROR;
    }
    if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Object '%s' already exists", name);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


static void
remove_created(struct ged *gedp, const std::vector<std::string> &names)
{
    for (const std::string &name : names) {
	struct directory *dp = db_lookup(gedp->dbip, name.c_str(), LOOKUP_QUIET);
	if (dp != RT_DIR_NULL) {
	    (void)db_delete(gedp->dbip, dp);
	    (void)db_dirdelete(gedp->dbip, dp);
	}
    }
}


static int
set_kind(struct ged *gedp, const char *name, const char *kind,
	 const create_options &opts)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str,
	    "Unable to find newly created annotation '%s'", name);
	return BRLCAD_ERROR;
    }
    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    int ret = db5_get_attributes(gedp->dbip, &avs, dp);
    if (!ret) {
	ret = bu_avs_add(&avs, ATTR_KIND, kind) < 0;
	if (!ret && opts.color.set) {
	    unsigned char rgb[3];
	    struct bu_vls value = BU_VLS_INIT_ZERO;
	    bu_color_to_rgb_chars(&opts.color.value, rgb);
	    bu_vls_sprintf(&value, "%u/%u/%u", rgb[0], rgb[1], rgb[2]);
	    ret = bu_avs_add(&avs, ATTR_COLOR, bu_vls_cstr(&value)) < 0;
	    bu_vls_free(&value);
	}
	if (!ret && bu_vls_strlen(&opts.associated_object))
	    ret = bu_avs_add(&avs, ATTR_SOURCES,
		bu_vls_cstr(&opts.associated_object)) < 0;
	if (!ret)
	    ret = db5_update_attributes(dp, &avs, gedp->dbip);
    }
    bu_avs_free(&avs);
    if (ret) {
	bu_vls_printf(gedp->ged_result_str,
	    "Unable to tag annotation '%s'", name);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


static std::string
format_point(const point_t point)
{
    std::ostringstream value;
    value << std::setprecision(17) << point[X] << ' ' << point[Y] << ' ' << point[Z];
    return value.str();
}


static bool
parse_point(point_t point, const char *value)
{
    if (!value)
	return false;
    std::istringstream input(value);
    std::string trailing;
    return (input >> point[X] >> point[Y] >> point[Z]) && !(input >> trailing);
}


static std::string
format_local_point(const point_t point, const struct ged *gedp)
{
    point_t local;
    VSCALE(local, point, gedp->dbip->dbi_base2local);
    return format_point(local);
}


static int
draw_created(struct ged *gedp, const char *name, const create_options &opts)
{
    if (opts.no_draw)
	return BRLCAD_OK;

    /* A newly written directory entry must reach the view-state index before
     * draw can resolve it.  Applications without a DbiState use the legacy
     * display-list path and need no explicit synchronization here. */
    if (gedp->dbi_state)
	static_cast<DbiState *>(gedp->dbi_state)->update();

    struct bu_vls color = BU_VLS_INIT_ZERO;
    const char *draw_argv[5] = {"draw", name, NULL, NULL, NULL};
    int draw_argc = 2;
    if (opts.color.set) {
	unsigned char rgb[3];
	bu_color_to_rgb_chars(&opts.color.value, rgb);
	bu_vls_sprintf(&color, "%u/%u/%u", rgb[0], rgb[1], rgb[2]);
	draw_argv[1] = "-C";
	draw_argv[2] = bu_vls_cstr(&color);
	draw_argv[3] = name;
	draw_argc = 4;
    }
    int ret = ged_exec(gedp, draw_argc, draw_argv);

    /* On success the creation result is more useful than draw's result. */
    if (ret == BRLCAD_OK)
	bu_vls_trunc(gedp->ged_result_str, 0);
    bu_vls_free(&color);
    return ret;
}


static fastf_t
resolved_text_height(const create_options &opts, fastf_t reference_length,
		     const struct ged *gedp)
{
    if (opts.text_height > 0.0)
	return opts.text_height * gedp->dbip->dbi_local2base;
    return std::max(reference_length * DEFAULT_TEXT_SCALE,
	gedp->dbip->dbi_local2base);
}


static fastf_t
scene_reference_length(const struct ged *gedp)
{
    if (gedp->ged_gvp && gedp->ged_gvp->gv_scale > SMALL_FASTF)
	return gedp->ged_gvp->gv_scale * 2.0;
    return 40.0 * gedp->dbip->dbi_local2base;
}


static void
add_arrow(annotation_builder &builder, fastf_t tip_x, fastf_t tip_y,
	  fastf_t direction, fastf_t size, const create_options &opts)
{
    int tip = builder.vertex(tip_x, tip_y);
    int wing1 = builder.vertex(tip_x + direction * size,
	tip_y + size * DEFAULT_ARROW_WING_SCALE);
    int wing2 = builder.vertex(tip_x + direction * size,
	tip_y - size * DEFAULT_ARROW_WING_SCALE);
    builder.line(tip, wing1, RT_ANNOT_ROLE_ARROWHEAD, opts);
    builder.line(tip, wing2, RT_ANNOT_ROLE_ARROWHEAD, opts);
}


static void
add_arrow_direction(annotation_builder &builder, fastf_t tip_x, fastf_t tip_y,
		    fastf_t direction_x, fastf_t direction_y, fastf_t size,
		    const create_options &opts)
{
    fastf_t magnitude = hypot(direction_x, direction_y);
    if (magnitude <= SMALL_FASTF)
	return;
    direction_x /= magnitude;
    direction_y /= magnitude;
    fastf_t perpendicular_x = -direction_y;
    fastf_t perpendicular_y = direction_x;
    int tip = builder.vertex(tip_x, tip_y);
    int wing1 = builder.vertex(
	tip_x + direction_x * size + perpendicular_x * size * DEFAULT_ARROW_WING_SCALE,
	tip_y + direction_y * size + perpendicular_y * size * DEFAULT_ARROW_WING_SCALE);
    int wing2 = builder.vertex(
	tip_x + direction_x * size - perpendicular_x * size * DEFAULT_ARROW_WING_SCALE,
	tip_y + direction_y * size - perpendicular_y * size * DEFAULT_ARROW_WING_SCALE);
    builder.line(tip, wing1, RT_ANNOT_ROLE_ARROWHEAD, opts);
    builder.line(tip, wing2, RT_ANNOT_ROLE_ARROWHEAD, opts);
}


static std::string
format_measurement(fastf_t base_value, const create_options &opts,
		   const struct ged *gedp, const char *default_prefix = "")
{
    const char *unit_name = bu_vls_strlen(&opts.units) ?
	bu_vls_cstr(&opts.units) : bu_units_string(gedp->dbip->dbi_local2base);
    double unit_to_base = bu_units_conversion(unit_name);
    if (unit_to_base <= 0.0)
	unit_to_base = gedp->dbip->dbi_local2base;
    std::ostringstream label;
    label << bu_vls_cstr(&opts.prefix) << default_prefix << std::fixed
	<< std::setprecision(opts.precision) << base_value / unit_to_base;
    if (unit_name && unit_name[0])
	label << " " << unit_name;
    label << bu_vls_cstr(&opts.suffix);
    return label.str();
}


static void
stable_dimension_basis(vect_t u, vect_t v, const point_t from, const point_t to)
{
    vect_t normal;
    VSUB2(u, to, from);
    VUNITIZE(u);
    VSET(normal, 0.0, 0.0, 1.0);
    if (fabs(VDOT(u, normal)) > 0.95)
	VSET(normal, 0.0, 1.0, 0.0);
    VCROSS(v, normal, u);
    VUNITIZE(v);
}


static int
write_linear_dimension(struct ged *gedp, const char *name, const point_t from,
		       const point_t to, const vect_t supplied_v,
		       const create_options &opts, const char *kind,
		       const char *label_prefix = "", fastf_t offset_sign = 1.0)
{
    vect_t u, v;
    fastf_t length = DIST_PNT_PNT(from, to);
    if (length <= SMALL_FASTF) {
	bu_vls_printf(gedp->ged_result_str, "Dimension endpoints must be distinct");
	return BRLCAD_ERROR;
    }
    if (supplied_v && MAGNITUDE(supplied_v) > SMALL_FASTF) {
	VSUB2(u, to, from);
	VUNITIZE(u);
	VMOVE(v, supplied_v);
	VUNITIZE(v);
    } else {
	stable_dimension_basis(u, v, from, to);
    }

    fastf_t text_height = resolved_text_height(opts, length, gedp);
    fastf_t offset = std::isfinite(opts.offset) ?
	opts.offset * gedp->dbip->dbi_local2base : text_height * 2.0;
    offset *= offset_sign;
    fastf_t arrow = std::min(text_height, length * 0.2);
    annotation_builder builder(from, u, v);
    int base0 = builder.vertex(0.0, 0.0);
    int base1 = builder.vertex(length, 0.0);
    int dim0 = builder.vertex(0.0, offset);
    int dim1 = builder.vertex(length, offset);
    if (!opts.no_extension_lines) {
	int extension0 = builder.vertex(0.0,
	    offset + offset_sign * text_height * DEFAULT_EXTENSION_OVERSHOOT_SCALE);
	int extension1 = builder.vertex(length,
	    offset + offset_sign * text_height * DEFAULT_EXTENSION_OVERSHOOT_SCALE);
	builder.line(base0, extension0, RT_ANNOT_ROLE_EXTENSION, opts);
	builder.line(base1, extension1, RT_ANNOT_ROLE_EXTENSION, opts);
    }
    builder.line(dim0, dim1, RT_ANNOT_ROLE_DIMENSION, opts);
    add_arrow(builder, 0.0, offset, 1.0, arrow, opts);
    add_arrow(builder, length, offset, -1.0, arrow, opts);
    int text = builder.vertex(length * 0.5,
	offset + offset_sign * text_height * 0.25);
    const int text_position = offset_sign > 0.0 ? RT_TXT_POS_BC : RT_TXT_POS_TC;
    builder.text(text, format_measurement(length, opts, gedp, label_prefix),
	text_height, text_position, opts);
    if (builder.write(gedp, name) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (set_kind(gedp, name, kind, opts) != BRLCAD_OK) {
	remove_created(gedp, {name});
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


static int
write_radial_dimension(struct ged *gedp, const char *name, const point_t center,
		       const point_t edge, bool diameter,
		       const create_options &opts)
{
    vect_t u, v;
    fastf_t radius = DIST_PNT_PNT(center, edge);
    if (radius <= SMALL_FASTF) {
	bu_vls_printf(gedp->ged_result_str,
	    "Radial dimension center and edge point must be distinct");
	return BRLCAD_ERROR;
    }
    stable_dimension_basis(u, v, center, edge);
    fastf_t text_height = resolved_text_height(opts, radius * 2.0, gedp);
    fastf_t arrow = std::min(text_height, radius * 0.2);
    annotation_builder builder(center, u, v);
    int middle = builder.vertex(0.0, 0.0);
    int positive = builder.vertex(radius, 0.0);
    if (diameter) {
	int negative = builder.vertex(-radius, 0.0);
	builder.line(negative, positive, RT_ANNOT_ROLE_DIMENSION, opts);
	add_arrow(builder, -radius, 0.0, 1.0, arrow, opts);
	add_arrow(builder, radius, 0.0, -1.0, arrow, opts);
    } else {
	builder.line(middle, positive, RT_ANNOT_ROLE_DIMENSION, opts);
	add_arrow(builder, radius, 0.0, -1.0, arrow, opts);
    }
    int text = builder.vertex(0.0, text_height * 0.35);
    builder.text(text,
	format_measurement(diameter ? radius * 2.0 : radius, opts, gedp,
	    diameter ? "\xE2\x8C\x80" : "R"),
	text_height, RT_TXT_POS_BC, opts);
    if (builder.write(gedp, name) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (set_kind(gedp, name, diameter ? "diameter" : "radius", opts) != BRLCAD_OK) {
	remove_created(gedp, {name});
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


static int
write_angular_dimension(struct ged *gedp, const char *name, const point_t vertex,
			const point_t from, const point_t to,
			const create_options &opts)
{
    vect_t first_ray, second_ray, u, v, normal;
    VSUB2(first_ray, from, vertex);
    VSUB2(second_ray, to, vertex);
    fastf_t first_length = MAGNITUDE(first_ray);
    fastf_t second_length = MAGNITUDE(second_ray);
    if (first_length <= SMALL_FASTF || second_length <= SMALL_FASTF) {
	bu_vls_printf(gedp->ged_result_str,
	    "Angular dimension rays must have nonzero length");
	return BRLCAD_ERROR;
    }
    VSCALE(u, first_ray, 1.0 / first_length);
    VSCALE(second_ray, second_ray, 1.0 / second_length);
    VCROSS(normal, u, second_ray);
    if (MAGNITUDE(normal) <= SMALL_FASTF) {
	bu_vls_printf(gedp->ged_result_str,
	    "Angular dimension rays must not be collinear");
	return BRLCAD_ERROR;
    }
    VUNITIZE(normal);
    VCROSS(v, normal, u);
    VUNITIZE(v);
    fastf_t cosine = std::max<fastf_t>(-1.0,
	std::min<fastf_t>(1.0, VDOT(u, second_ray)));
    fastf_t angle = acos(cosine);
    fastf_t reference_length = std::min(first_length, second_length);
    fastf_t text_height = resolved_text_height(opts, reference_length, gedp);
    fastf_t radius = std::isfinite(opts.offset) ?
	opts.offset * gedp->dbip->dbi_local2base : reference_length * 0.6;
    if (radius <= SMALL_FASTF) {
	bu_vls_printf(gedp->ged_result_str,
	    "Angular dimension offset must be greater than zero");
	return BRLCAD_ERROR;
    }

    annotation_builder builder(vertex, u, v);
    int center = builder.vertex(0.0, 0.0);
    int arc_start = builder.vertex(radius, 0.0);
    int arc_end = builder.vertex(radius * cos(angle), radius * sin(angle));
    if (!opts.no_extension_lines) {
	builder.line(center, arc_start, RT_ANNOT_ROLE_EXTENSION, opts);
	builder.line(center, arc_end, RT_ANNOT_ROLE_EXTENSION, opts);
    }
    int previous = arc_start;
    for (int segment = 1; segment <= DEFAULT_ARC_SEGMENTS; ++segment) {
	fastf_t theta = angle * segment / DEFAULT_ARC_SEGMENTS;
	int current = segment == DEFAULT_ARC_SEGMENTS ? arc_end :
	    builder.vertex(radius * cos(theta), radius * sin(theta));
	builder.line(previous, current, RT_ANNOT_ROLE_DIMENSION, opts);
	previous = current;
    }
    fastf_t arrow = std::min(text_height, radius * angle * 0.2);
    add_arrow_direction(builder, radius, 0.0, 0.0, 1.0, arrow, opts);
    add_arrow_direction(builder, radius * cos(angle), radius * sin(angle),
	sin(angle), -cos(angle), arrow, opts);

    std::ostringstream label;
    label << bu_vls_cstr(&opts.prefix) << std::fixed
	<< std::setprecision(opts.precision) << angle * RAD2DEG
	<< "\xC2\xB0" << bu_vls_cstr(&opts.suffix);
    fastf_t label_radius = radius + text_height * 0.4;
    int text = builder.vertex(label_radius * cos(angle * 0.5),
	label_radius * sin(angle * 0.5));
    builder.text(text, label.str(), text_height, RT_TXT_POS_BC, opts);
    if (builder.write(gedp, name) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (set_kind(gedp, name, "angular", opts) != BRLCAD_OK) {
	remove_created(gedp, {name});
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}


static int
cmd_text(void *data, int argc, const char **argv)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    if (annotate_command_messages(gedp, argc, argv,
	"annotate text [options] name text", "Create styled model-space text"))
	return BRLCAD_OK;
    create_options opts;
    struct bu_opt_desc descs[20];
    int dcnt = add_common_options(descs, 0, opts);
    ANNOT_OPT(descs, dcnt, "", "at", "x y z", &bu_opt_vect_t, &opts.at,
	"Text anchor point");
    ANNOT_OPT(descs, dcnt, "", "plane", "xy|xz|yz", &parse_vls_value, &opts.plane,
	"Model-space text plane");
    ANNOT_OPT(descs, dcnt, "", "frame", "", NULL, &opts.frame,
	"Draw a rectangular frame around the text");
    BU_OPT_NULL(descs[dcnt]);
    argc--; argv++;
    int remaining = parse_options(gedp, argc, argv, descs, opts);
    if (remaining < 0)
	return BRLCAD_ERROR;
    if (opts.help || remaining != 2) {
	print_option_help(gedp->ged_result_str,
	    "Usage: annotate text [options] name text", descs,
	    "Creates styled text in a model plane.");
	return opts.help ? GED_HELP : BRLCAD_ERROR;
    }
    if (prepare_name(gedp, argv[0]) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (!point_is_set(opts.at))
	VSETALL(opts.at, 0.0);
    else
	to_base(opts.at, gedp);
    vect_t u, v;
    set_plane(u, v, bu_vls_cstr(&opts.plane));
    if (!BU_STR_EQUAL(bu_vls_cstr(&opts.plane), "xy") &&
	!BU_STR_EQUAL(bu_vls_cstr(&opts.plane), "xz") &&
	!BU_STR_EQUAL(bu_vls_cstr(&opts.plane), "yz")) {
	bu_vls_printf(gedp->ged_result_str, "Plane must be xy, xz, or yz");
	return BRLCAD_ERROR;
    }
    fastf_t text_height = resolved_text_height(opts, scene_reference_length(gedp), gedp);
    annotation_builder builder(opts.at, u, v);
    int text_ref = builder.vertex(0.0, 0.0);
    builder.text(text_ref, argv[1], text_height, RT_TXT_POS_BL, opts);
    if (opts.frame) {
	fastf_t width = std::max<size_t>(1, strlen(argv[1])) * text_height * 0.65;
	fastf_t pad = text_height * 0.25;
	int p0 = builder.vertex(-pad, -pad);
	int p1 = builder.vertex(width + pad, -pad);
	int p2 = builder.vertex(width + pad, text_height + pad);
	int p3 = builder.vertex(-pad, text_height + pad);
	builder.line(p0, p1, RT_ANNOT_ROLE_TEXT_DECORATION, opts);
	builder.line(p1, p2, RT_ANNOT_ROLE_TEXT_DECORATION, opts);
	builder.line(p2, p3, RT_ANNOT_ROLE_TEXT_DECORATION, opts);
	builder.line(p3, p0, RT_ANNOT_ROLE_TEXT_DECORATION, opts);
    }
    if (builder.write(gedp, argv[0]) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (set_kind(gedp, argv[0], "text", opts) != BRLCAD_OK) {
	remove_created(gedp, {argv[0]});
	return BRLCAD_ERROR;
    }
    if (draw_created(gedp, argv[0], opts) != BRLCAD_OK)
	return BRLCAD_ERROR;
    bu_vls_printf(gedp->ged_result_str, "Created text annotation '%s'", argv[0]);
    return BRLCAD_OK;
}


struct leader_bounds {
    point_t center;
    vect_t axes[3];
    fastf_t half_extent[3];
};


static int
prepare_leader_bounds(struct leader_bounds &bounds, const point_t corners[8])
{
    const int adjacent[3] = {4, 1, 3};
    VADD2SCALE(bounds.center, corners[0], corners[6], 0.5);
    for (int axis = 0; axis < 3; ++axis) {
	VSUB2(bounds.axes[axis], corners[adjacent[axis]], corners[0]);
	bounds.half_extent[axis] = 0.5 * MAGNITUDE(bounds.axes[axis]);
	if (bounds.half_extent[axis] <= SMALL_FASTF)
	    return BRLCAD_ERROR;
	VUNITIZE(bounds.axes[axis]);
    }
    return BRLCAD_OK;
}


static bool
intersect_leader_bounds(fastf_t &entry, fastf_t &exit, const point_t origin,
			const vect_t direction, const struct leader_bounds &bounds)
{
    vect_t relative;
    VSUB2(relative, origin, bounds.center);
    entry = -INFINITY;
    exit = INFINITY;
    for (int axis = 0; axis < 3; ++axis) {
	const fastf_t local_origin = VDOT(relative, bounds.axes[axis]);
	const fastf_t local_direction = VDOT(direction, bounds.axes[axis]);
	if (fabs(local_direction) <= SMALL_FASTF) {
	    if (fabs(local_origin) > bounds.half_extent[axis])
		return false;
	    continue;
	}
	fastf_t first = (-bounds.half_extent[axis] - local_origin) / local_direction;
	fastf_t second = (bounds.half_extent[axis] - local_origin) / local_direction;
	if (first > second)
	    std::swap(first, second);
	entry = std::max(entry, first);
	exit = std::min(exit, second);
	if (entry > exit)
	    return false;
    }
    return true;
}


static fastf_t
readable_leader_basis(vect_t u, vect_t v, const point_t target,
		      const point_t label, const struct bview *view)
{
    const fastf_t length = DIST_PNT_PNT(target, label);
    if (!view) {
	stable_dimension_basis(u, v, target, label);
	return length;
    }

    point_t target_view, label_view;
    MAT4X3PNT(target_view, view->gv_model2view, target);
    MAT4X3PNT(label_view, view->gv_model2view, label);
    const fastf_t dx = label_view[X] - target_view[X];
    const fastf_t dy = label_view[Y] - target_view[Y];
    const bool reverse_baseline = dx < -SMALL_FASTF ||
	(fabs(dx) <= SMALL_FASTF && dy < 0.0);
    if (reverse_baseline)
	VSUB2(u, target, label);
    else
	VSUB2(u, label, target);
    VUNITIZE(u);

    const vect_t view_up = {0.0, 1.0, 0.0};
    MAT4X3VEC(v, view->gv_view2model, view_up);
    VJOIN1(v, v, -VDOT(v, u), u);
    if (MAGNITUDE(v) <= SMALL_FASTF) {
	const vect_t view_right = {1.0, 0.0, 0.0};
	MAT4X3VEC(v, view->gv_view2model, view_right);
	VJOIN1(v, v, -VDOT(v, u), u);
    }
    VUNITIZE(v);

    point_t u_model, v_model, u_view, v_view;
    VADD2(u_model, target, u);
    VADD2(v_model, target, v);
    MAT4X3PNT(u_view, view->gv_model2view, u_model);
    MAT4X3PNT(v_view, view->gv_model2view, v_model);
    const fastf_t ux = u_view[X] - target_view[X];
    const fastf_t uy = u_view[Y] - target_view[Y];
    const fastf_t vx = v_view[X] - target_view[X];
    const fastf_t vy = v_view[Y] - target_view[Y];
    if (ux * vy - uy * vx < 0.0)
	VREVERSE(v, v);
    return reverse_baseline ? -length : length;
}


static int
cmd_leader(void *data, int argc, const char **argv)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    if (annotate_command_messages(gedp, argc, argv,
	"annotate leader [options] name text", "Create a text callout with a leader"))
	return BRLCAD_OK;
    create_options opts;
    struct bu_opt_desc descs[20];
    int dcnt = add_common_options(descs, 0, opts);
    ANNOT_OPT(descs, dcnt, "", "target", "x y z", &bu_opt_vect_t, &opts.target,
	"Leader target point");
    ANNOT_OPT(descs, dcnt, "", "at", "x y z", &bu_opt_vect_t, &opts.at,
	"Text anchor point");
    ANNOT_OPT(descs, dcnt, "", "screen-space", "", NULL, &opts.screen_space,
	"Keep leader and text in the display plane");
    ANNOT_OPT(descs, dcnt, "", "dpi", "value", &bu_opt_fastf_t, &opts.dpi,
	"Physical display density for --screen-space text (default: 96)");
    BU_OPT_NULL(descs[dcnt]);
    argc--; argv++;
    int remaining = parse_options(gedp, argc, argv, descs, opts);
    if (remaining < 0)
	return BRLCAD_ERROR;
    if (opts.help || remaining != 2) {
	print_option_help(gedp->ged_result_str,
	    "Usage: annotate leader [options] name text", descs,
	    "Creates a callout with an arrowhead at --target or an associated object.");
	return opts.help ? GED_HELP : BRLCAD_ERROR;
    }
    if (prepare_name(gedp, argv[0]) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (opts.screen_space &&
	(!gedp->ged_gvp || gedp->ged_gvp->gv_width <= 0 ||
	 gedp->ged_gvp->gv_height <= 0)) {
	bu_vls_printf(gedp->ged_result_str,
	    "Screen-space leaders require an active view");
	return BRLCAD_ERROR;
    }
    if (!opts.screen_space &&
	std::fabs(opts.dpi - DEFAULT_SCREEN_DPI) > SMALL_FASTF) {
	bu_vls_printf(gedp->ged_result_str,
	    "--dpi is only valid with --screen-space");
	return BRLCAD_ERROR;
    }

    const bool automatic_target = !point_is_set(opts.target);
    struct leader_bounds bounds;
    if (automatic_target) {
	if (!bu_vls_strlen(&opts.associated_object)) {
	    bu_vls_printf(gedp->ged_result_str,
		"Leader requires --target or an associated object selected with --for");
	    return BRLCAD_ERROR;
	}
	point_t corners[8];
	const char *source = bu_vls_cstr(&opts.associated_object);
	if (_ged_obj_oriented_bounds(gedp, 1, &source, 1, 0, corners) != BRLCAD_OK ||
	    prepare_leader_bounds(bounds, corners) != BRLCAD_OK) {
	    bu_vls_printf(gedp->ged_result_str,
		"Unable to calculate oriented bounds for '%s'", source);
	    return BRLCAD_ERROR;
	}
	VMOVE(opts.target, bounds.center);
    } else {
	to_base(opts.target, gedp);
    }

    bool automatic_position = !point_is_set(opts.at);
    if (!automatic_position)
	to_base(opts.at, gedp);
    fastf_t reference_length = automatic_position ? scene_reference_length(gedp) :
	DIST_PNT_PNT(opts.target, opts.at);
    /* A display-plane label has no meaningful model extent from which to
     * derive a relative text size.  Its default is instead a readable
     * physical height, while an explicit value still uses current units. */
    fastf_t text_height = opts.screen_space && opts.text_height <= 0.0 ?
	DEFAULT_SCREEN_TEXT_HEIGHT_MM : resolved_text_height(opts, reference_length, gedp);
    if (automatic_position) {
	vect_t outward = {DEFAULT_LEADER_X, DEFAULT_LEADER_Y, 0.0};
	VUNITIZE(outward);
	if (automatic_target) {
	    fastf_t entry, exit;
	    if (!intersect_leader_bounds(entry, exit, bounds.center, outward, bounds)) {
		bu_vls_printf(gedp->ged_result_str, "Unable to place leader label");
		return BRLCAD_ERROR;
	    }
	    VJOIN1(opts.at, bounds.center,
		exit + DEFAULT_LEADER_MARGIN_SCALE * text_height, outward);
	} else {
	    VJOIN1(opts.at, opts.target, DEFAULT_LEADER_X * text_height, outward);
	}
    }

    if (automatic_target) {
	vect_t direction;
	VSUB2(direction, bounds.center, opts.at);
	fastf_t entry, exit;
	if (MAGNITUDE(direction) <= SMALL_FASTF ||
	    !intersect_leader_bounds(entry, exit, opts.at, direction, bounds) ||
	    entry < 0.0 || entry > 1.0) {
	    bu_vls_printf(gedp->ged_result_str,
		"Unable to intersect leader with oriented bounds");
	    return BRLCAD_ERROR;
	}
	VJOIN1(opts.target, opts.at, entry, direction);
    }
    vect_t u, v;
    fastf_t label_x, label_y, text_height_in_plane;
    if (opts.screen_space) {
	point_t target_view, label_view;
	MAT4X3PNT(target_view, gedp->ged_gvp->gv_model2view, opts.target);
	MAT4X3PNT(label_view, gedp->ged_gvp->gv_model2view, opts.at);
	label_x = (label_view[X] - target_view[X]) *
	    gedp->ged_gvp->gv_width / (2.0 * DISPLAY_PIXELS_PER_MM);
	label_y = (label_view[Y] - target_view[Y]) *
	    gedp->ged_gvp->gv_height / (2.0 * DISPLAY_PIXELS_PER_MM);
	text_height_in_plane = text_height * opts.dpi / DEFAULT_SCREEN_DPI;
	VSET(u, 1.0, 0.0, 0.0);
	VSET(v, 0.0, 1.0, 0.0);
    } else {
	label_x = readable_leader_basis(u, v, opts.target, opts.at, gedp->ged_gvp);
	label_y = 0.0;
	text_height_in_plane = text_height;
    }
    const fastf_t length = hypot(label_x, label_y);
    annotation_builder builder(opts.target, u, v, opts.screen_space);
    int target = builder.vertex(0.0, 0.0);
    int label = builder.vertex(label_x, label_y);
    builder.line(target, label, RT_ANNOT_ROLE_LEADER, opts);
    add_arrow_direction(builder, 0.0, 0.0, label_x, label_y,
	std::min(text_height_in_plane, length * 0.2), opts);
    const int text_position = label_x < 0.0 ? RT_TXT_POS_MR : RT_TXT_POS_ML;
    builder.text(label, argv[1], text_height_in_plane, text_position, opts);
    if (builder.write(gedp, argv[0]) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (set_kind(gedp, argv[0], "leader", opts) != BRLCAD_OK) {
	remove_created(gedp, {argv[0]});
	return BRLCAD_ERROR;
    }
    struct directory *leader_dp = db_lookup(gedp->dbip, argv[0], LOOKUP_QUIET);
    struct bu_attribute_value_set leader_avs = BU_AVS_INIT_ZERO;
    int attributes_ret = leader_dp == RT_DIR_NULL ? BRLCAD_ERROR :
	db5_get_attributes(gedp->dbip, &leader_avs, leader_dp);
    if (!attributes_ret) {
	const std::string target_value = format_point(opts.target);
	const std::string at_value = format_point(opts.at);
	const std::string text_height_value = std::to_string(text_height *
	    gedp->dbip->dbi_base2local);
	const std::string dpi_value = std::to_string(opts.dpi);
	auto add_attr = [&](const char *key, const char *value) {
	    if (!attributes_ret && bu_avs_add(&leader_avs, key, value) < 0)
		attributes_ret = BRLCAD_ERROR;
	};
	add_attr(ATTR_LEADER_TEXT, argv[1]);
	add_attr(ATTR_LEADER_TARGET, target_value.c_str());
	add_attr(ATTR_LEADER_TARGET_AUTO, automatic_target ? "1" : "0");
	add_attr(ATTR_LEADER_AT, at_value.c_str());
	add_attr(ATTR_LEADER_AT_AUTO, automatic_position ? "1" : "0");
	add_attr(ATTR_LEADER_SCREEN_SPACE, opts.screen_space ? "1" : "0");
	add_attr(ATTR_LEADER_DPI, dpi_value.c_str());
	add_attr(ATTR_TEXT_HEIGHT, text_height_value.c_str());
	add_attr(ATTR_TEXT_HEIGHT_AUTO, opts.text_height > 0.0 ? "0" : "1");
	add_attr(ATTR_FONT, bu_vls_cstr(&opts.font));
	if (opts.line_width > 0.0) {
	    const std::string line_width_value = std::to_string(opts.line_width);
	    add_attr(ATTR_LINE_WIDTH, line_width_value.c_str());
	}
	add_attr(ATTR_LINE_STYLE, bu_vls_cstr(&opts.line_style));
	add_attr(ATTR_BOLD, opts.bold ? "1" : "0");
	add_attr(ATTR_ITALIC, opts.italic ? "1" : "0");
	if (!attributes_ret && db5_update_attributes(leader_dp, &leader_avs, gedp->dbip))
	    attributes_ret = BRLCAD_ERROR;
    }
    bu_avs_free(&leader_avs);
    if (attributes_ret) {
	remove_created(gedp, {argv[0]});
	bu_vls_printf(gedp->ged_result_str,
	    "Unable to store leader update data for '%s'", argv[0]);
	return BRLCAD_ERROR;
    }
    if (draw_created(gedp, argv[0], opts) != BRLCAD_OK)
	return BRLCAD_ERROR;
    bu_vls_printf(gedp->ged_result_str, "Created leader annotation '%s'", argv[0]);
    return BRLCAD_OK;
}


static int
add_dimension_options(struct bu_opt_desc *descs, int index, create_options &opts)
{
    ANNOT_OPT(descs, index, "", "from", "x y z", &bu_opt_vect_t, &opts.from,
	"First measured point");
    ANNOT_OPT(descs, index, "", "to", "x y z", &bu_opt_vect_t, &opts.to,
	"Second measured point");
    ANNOT_OPT(descs, index, "", "vertex", "x y z", &bu_opt_vect_t, &opts.vertex,
	"Angular dimension vertex");
    ANNOT_OPT(descs, index, "", "center", "x y z", &bu_opt_vect_t, &opts.center,
	"Circle center");
    ANNOT_OPT(descs, index, "", "origin", "x y z", &bu_opt_vect_t, &opts.origin,
	"Ordinate origin");
    ANNOT_OPT(descs, index, "", "axis", "x|y|z", &parse_vls_value, &opts.axis,
	"Ordinate measurement axis");
    ANNOT_OPT(descs, index, "", "offset", "distance", &bu_opt_fastf_t, &opts.offset,
	"Dimension-line offset in current units");
    ANNOT_OPT(descs, index, "", "units", "unit", &parse_vls_value, &opts.units,
	"Display units");
    ANNOT_OPT(descs, index, "", "precision", "digits", &bu_opt_int, &opts.precision,
	"Digits after the decimal point");
    ANNOT_OPT(descs, index, "", "prefix", "text", &parse_vls_value, &opts.prefix,
	"Text before the measured value");
    ANNOT_OPT(descs, index, "", "suffix", "text", &parse_vls_value, &opts.suffix,
	"Text after the measured value and units");
    ANNOT_OPT(descs, index, "", "no-extension-lines", "", NULL,
	&opts.no_extension_lines, "Suppress extension lines");
    return index;
}


static int
parse_dimension_options(struct ged *gedp, int argc, const char **argv,
			create_options &opts, const char *usage)
{
    struct bu_opt_desc descs[32];
    const int dcnt = add_dimension_options(descs, add_common_options(descs, 0, opts), opts);
    BU_OPT_NULL(descs[dcnt]);
    int remaining = parse_options(gedp, argc, argv, descs, opts);
    if (remaining < 0)
	return -1;
    if (opts.help) {
	bu_vls_printf(gedp->ged_result_str, "%s", usage);
	return -2;
    }
    if (bu_vls_strlen(&opts.units) && bu_units_conversion(bu_vls_cstr(&opts.units)) <= 0.0) {
	bu_vls_printf(gedp->ged_result_str, "Unknown unit '%s'", bu_vls_cstr(&opts.units));
	return -1;
    }
    return remaining;
}


static const char *
dimension_help_description(const std::string &type)
{
    if (type == "linear")
	return "Measures the distance between --from and --to.";
    if (type == "angular")
	return "Measures the angle from --vertex through --from to --to.";
    if (type == "radius")
	return "Measures the radius from --center to --to.";
    if (type == "diameter")
	return "Measures the diameter through --center and --to.";
    if (type == "ordinate")
	return "Measures the --axis displacement from --origin to --to.";
    return NULL;
}


static void
print_dimension_help(struct ged *gedp, const std::string &type)
{
    create_options opts;
    struct bu_opt_desc descs[32];
    const int dcnt = add_dimension_options(descs, add_common_options(descs, 0, opts), opts);
    BU_OPT_NULL(descs[dcnt]);
    const std::string usage = "Usage: annotate dimension " + type + " [options] name";
    print_option_help(gedp->ged_result_str, usage.c_str(), descs,
	dimension_help_description(type));
}


static int
cmd_dimension_type(void *data, int argc, const char **argv)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    if (argc < 1)
	return BRLCAD_ERROR;
    const std::string type(argv[0]);
    const char *purpose = dimension_help_description(type);
    if (!purpose) {
	bu_vls_printf(gedp->ged_result_str, "Unknown dimension type '%s'", type.c_str());
	return BRLCAD_ERROR;
    }
    const std::string type_usage = "annotate dimension " + type + " [options] name";
    if (annotate_command_messages(gedp, argc, argv, type_usage.c_str(), purpose))
	return BRLCAD_OK;
    argc--; argv++;
    create_options opts;
    const std::string usage = "Usage: " + type_usage;
    int remaining = parse_dimension_options(gedp, argc, argv, opts, usage.c_str());
    if (remaining == -2) {
	print_dimension_help(gedp, type);
	return GED_HELP;
    }
    if (remaining < 0 || remaining != 1) {
	if (remaining >= 0)
	    bu_vls_printf(gedp->ged_result_str, "%s", usage.c_str());
	return BRLCAD_ERROR;
    }
    const char *name = argv[0];
    if (prepare_name(gedp, name) != BRLCAD_OK)
	return BRLCAD_ERROR;
    point_t from, to;
    bool angular = false;
    bool circular = false;
    const char *kind = NULL;
    if (type == "linear") {
	if (!point_is_set(opts.from) || !point_is_set(opts.to)) {
	    bu_vls_printf(gedp->ged_result_str, "Linear dimensions require --from and --to");
	    return BRLCAD_ERROR;
	}
	VMOVE(from, opts.from);
	VMOVE(to, opts.to);
	kind = "linear";
    } else if (type == "angular") {
	if (!point_is_set(opts.vertex) || !point_is_set(opts.from) ||
	    !point_is_set(opts.to)) {
	    bu_vls_printf(gedp->ged_result_str,
		"Angular dimensions require --vertex, --from, and --to");
	    return BRLCAD_ERROR;
	}
	if (bu_vls_strlen(&opts.units)) {
	    bu_vls_printf(gedp->ged_result_str,
		"Angular dimensions are displayed in degrees; omit --units");
	    return BRLCAD_ERROR;
	}
	VMOVE(from, opts.from);
	VMOVE(to, opts.to);
	angular = true;
	kind = "angular";
    } else if (type == "radius" || type == "diameter") {
	if (!point_is_set(opts.center) || !point_is_set(opts.to)) {
	    bu_vls_printf(gedp->ged_result_str, "%s dimensions require --center and --to", type.c_str());
	    return BRLCAD_ERROR;
	}
	VMOVE(from, opts.center);
	VMOVE(to, opts.to);
	circular = true;
	kind = type.c_str();
    } else if (type == "ordinate") {
	if (!point_is_set(opts.origin) || !point_is_set(opts.to) || !bu_vls_strlen(&opts.axis)) {
	    bu_vls_printf(gedp->ged_result_str, "Ordinate dimensions require --origin, --to, and --axis");
	    return BRLCAD_ERROR;
	}
	VMOVE(from, opts.origin);
	VMOVE(to, opts.origin);
	const char axis = bu_vls_cstr(&opts.axis)[0];
	if ((axis != 'x' && axis != 'y' && axis != 'z') || bu_vls_strlen(&opts.axis) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Ordinate axis must be x, y, or z");
	    return BRLCAD_ERROR;
	}
	to[axis == 'x' ? X : axis == 'y' ? Y : Z] =
	    opts.to[axis == 'x' ? X : axis == 'y' ? Y : Z];
	kind = "ordinate";
    } else {
	bu_vls_printf(gedp->ged_result_str, "Unknown dimension type '%s'", type.c_str());
	return BRLCAD_ERROR;
    }
    to_base(from, gedp);
    to_base(to, gedp);
    int ret;
    if (angular) {
	to_base(opts.vertex, gedp);
	ret = write_angular_dimension(gedp, name, opts.vertex, from, to, opts);
    } else if (circular) {
	ret = write_radial_dimension(gedp, name, from, to, type == "diameter",
	    opts);
    } else {
	ret = write_linear_dimension(gedp, name, from, to, NULL, opts, kind,
	    "");
    }
    if (ret != BRLCAD_OK)
	return ret;
    if (draw_created(gedp, name, opts) != BRLCAD_OK)
	return BRLCAD_ERROR;
    bu_vls_printf(gedp->ged_result_str, "Created %s dimension '%s'", kind, name);
    return BRLCAD_OK;
}


static const struct bu_cmdtab dimension_commands[] = {
    {"linear", cmd_dimension_type},
    {"angular", cmd_dimension_type},
    {"radius", cmd_dimension_type},
    {"diameter", cmd_dimension_type},
    {"ordinate", cmd_dimension_type},
    {NULL, NULL}
};


static int
cmd_dimension(void *data, int argc, const char **argv)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    if (annotate_command_messages(gedp, argc, argv,
	"annotate dimension TYPE [options] name", "Create a geometric dimension"))
	return BRLCAD_OK;
    argc--; argv++;
    int help = 0;
    struct bu_opt_desc descs[2];
    BU_OPT(descs[0], "h", "help", "", NULL, &help, "Print help");
    BU_OPT_NULL(descs[1]);
    const char *usage = "[options] TYPE [args]";
    if (!argc) {
	_ged_subcmd_help(gedp, descs, dimension_commands, "annotate dimension", usage,
	    gedp, 0, NULL);
	return GED_HELP;
    }
    if (BU_STR_EQUAL(argv[0], "help") || BU_STR_EQUAL(argv[0], "--help") ||
	BU_STR_EQUAL(argv[0], "-h")) {
	if (argc > 1)
	    _ged_subcmd_help(gedp, descs, dimension_commands, "annotate dimension", usage,
		gedp, argc - 1, argv + 1);
	else
	    _ged_subcmd_help(gedp, descs, dimension_commands, "annotate dimension", usage,
		gedp, 0, NULL);
	return GED_HELP;
    }
    int ret = BRLCAD_ERROR;
    if (bu_cmd(dimension_commands, argc, argv, 0, gedp, &ret) == BRLCAD_OK)
	return ret;
    bu_vls_printf(gedp->ged_result_str, "Unknown dimension type '%s'\n", argv[0]);
    _ged_subcmd_help(gedp, descs, dimension_commands, "annotate dimension", usage,
	gedp, 0, NULL);
    return BRLCAD_ERROR;
}


static bool
axis_selected(const char *axes, char axis)
{
    for (const char *cursor = axes; *cursor; ++cursor)
	if (*cursor == axis)
	    return true;
    return false;
}


static bool
normalize_axes(struct bu_vls *axes)
{
    bool selected[3] = {false, false, false};
    for (const char *cursor = bu_vls_cstr(axes); *cursor; ++cursor) {
	if (*cursor == ',' || isspace(static_cast<unsigned char>(*cursor)))
	    continue;
	const int axis = *cursor == 'x' ? X : *cursor == 'y' ? Y :
	    *cursor == 'z' ? Z : -1;
	if (axis < 0 || selected[axis])
	    return false;
	selected[axis] = true;
    }
    if (!selected[X] && !selected[Y] && !selected[Z])
	return false;
    bu_vls_trunc(axes, 0);
    const char axis_names[] = {'x', 'y', 'z'};
    for (int axis = 0; axis < 3; ++axis) {
	if (!selected[axis])
	    continue;
	if (bu_vls_strlen(axes))
	    bu_vls_putc(axes, ',');
	bu_vls_putc(axes, axis_names[axis]);
    }
    return true;
}


static int
corner_bits(const char *corner, int bits[3])
{
    if (BU_STR_EQUAL(corner, "auto")) {
	bits[0] = bits[1] = bits[2] = 0;
	return 0;
    }
    std::string value(corner);
    std::replace(value.begin(), value.end(), ',', ' ');
    std::istringstream input(value);
    std::string token;
    int count = 0;
    const char axis_names[] = {'x', 'y', 'z'};

    while (input >> token) {
	if (count >= 3)
	    return -1;
	if (token.size() < 2 || token[0] != axis_names[count])
	    return -1;
	if (token == std::string(1, axis_names[count]) + "min")
	    bits[count++] = 0;
	else if (token == std::string(1, axis_names[count]) + "max")
	    bits[count++] = 1;
	else
	    return -1;
    }
    return count == 3 ? 0 : -1;
}


struct autodim_box {
    point_t corners[8];
    point_t center;
    vect_t axes[3];
    fastf_t half_extent[3];
};


struct screen_rect {
    double xmin = INFINITY;
    double xmax = -INFINITY;
    double ymin = INFINITY;
    double ymax = -INFINITY;
};


struct autodim_placement {
    point_t from;
    point_t to;
    vect_t outward;
    point_t dim_from;
    point_t dim_to;
    point_t label;
    screen_rect label_rect;
    fastf_t offset_sign = 1.0;
    double score = -INFINITY;
};


static void
set_aabb_corners(struct autodim_box &box, const point_t bmin, const point_t bmax)
{
    VSET(box.corners[0], bmin[X], bmin[Y], bmin[Z]);
    VSET(box.corners[1], bmin[X], bmax[Y], bmin[Z]);
    VSET(box.corners[2], bmin[X], bmax[Y], bmax[Z]);
    VSET(box.corners[3], bmin[X], bmin[Y], bmax[Z]);
    VSET(box.corners[4], bmax[X], bmin[Y], bmin[Z]);
    VSET(box.corners[5], bmax[X], bmax[Y], bmin[Z]);
    VSET(box.corners[6], bmax[X], bmax[Y], bmax[Z]);
    VSET(box.corners[7], bmax[X], bmin[Y], bmax[Z]);
}


static int
prepare_autodim_box(struct autodim_box &box)
{
    VADD2SCALE(box.center, box.corners[0], box.corners[6], 0.5);
    const int adjacent[3] = {4, 1, 3};
    for (int axis = 0; axis < 3; ++axis) {
	VSUB2(box.axes[axis], box.corners[adjacent[axis]], box.corners[0]);
	box.half_extent[axis] = 0.5 * MAGNITUDE(box.axes[axis]);
	if (box.half_extent[axis] <= SMALL_FASTF)
	    return BRLCAD_ERROR;
	VUNITIZE(box.axes[axis]);
    }
    return BRLCAD_OK;
}


static void
autodim_corner(point_t point, const struct autodim_box &box, const int bits[3])
{
    VMOVE(point, box.center);
    for (int axis = 0; axis < 3; ++axis) {
	const fastf_t direction = bits[axis] ? 1.0 : -1.0;
	VJOIN1(point, point, direction * box.half_extent[axis], box.axes[axis]);
    }
}


static void
project_xy(double projected[2], const struct bview *view, const point_t point)
{
    point_t view_point;
    MAT4X3PNT(view_point, view->gv_model2view, point);
    projected[X] = view_point[X];
    projected[Y] = view_point[Y];
}


static void
extend_rect(screen_rect &rect, const double point[2])
{
    rect.xmin = std::min(rect.xmin, point[X]);
    rect.xmax = std::max(rect.xmax, point[X]);
    rect.ymin = std::min(rect.ymin, point[Y]);
    rect.ymax = std::max(rect.ymax, point[Y]);
}


static double
rect_overlap(const screen_rect &a, const screen_rect &b)
{
    const double width = std::max(0.0, std::min(a.xmax, b.xmax) -
	std::max(a.xmin, b.xmin));
    const double height = std::max(0.0, std::min(a.ymax, b.ymax) -
	std::max(a.ymin, b.ymin));
    return width * height;
}


static double
orient_2d(const double a[2], const double b[2], const double c[2])
{
    return (b[X] - a[X]) * (c[Y] - a[Y]) -
	(b[Y] - a[Y]) * (c[X] - a[X]);
}


static bool
segments_cross(const double a0[2], const double a1[2],
	       const double b0[2], const double b1[2])
{
    const double ab0 = orient_2d(a0, a1, b0);
    const double ab1 = orient_2d(a0, a1, b1);
    const double ba0 = orient_2d(b0, b1, a0);
    const double ba1 = orient_2d(b0, b1, a1);
    return ((ab0 < 0.0 && ab1 > 0.0) || (ab0 > 0.0 && ab1 < 0.0)) &&
	((ba0 < 0.0 && ba1 > 0.0) || (ba0 > 0.0 && ba1 < 0.0));
}


static screen_rect
projected_box_rect(const struct autodim_box &box, const struct bview *view)
{
    screen_rect rect;
    for (const point_t &corner : box.corners) {
	double projected[2];
	project_xy(projected, view, corner);
	extend_rect(rect, projected);
    }
    return rect;
}


static void
orient_autodim_text(struct autodim_placement &placement, const struct bview *view)
{
    double from_2d[2], to_2d[2], outward_2d[2];
    project_xy(from_2d, view, placement.from);
    project_xy(to_2d, view, placement.to);
    const double projected_dx = to_2d[X] - from_2d[X];
    const double projected_dy = to_2d[Y] - from_2d[Y];
    if (projected_dx < -SMALL_FASTF ||
	(fabs(projected_dx) <= SMALL_FASTF && projected_dy < 0.0)) {
	point_t swap_point;
	VMOVE(swap_point, placement.from);
	VMOVE(placement.from, placement.to);
	VMOVE(placement.to, swap_point);
	project_xy(from_2d, view, placement.from);
	project_xy(to_2d, view, placement.to);
    }

    point_t outward_point;
    VADD2(outward_point, placement.from, placement.outward);
    project_xy(outward_2d, view, outward_point);
    const double ux = to_2d[X] - from_2d[X];
    const double uy = to_2d[Y] - from_2d[Y];
    const double vx = outward_2d[X] - from_2d[X];
    const double vy = outward_2d[Y] - from_2d[Y];
    if (ux * vy - uy * vx < 0.0) {
	VREVERSE(placement.outward, placement.outward);
	placement.offset_sign = -1.0;
    }
}


static autodim_placement
make_autodim_placement(const struct autodim_box &box, int dimension_axis,
		       const int edge_bits[3], int offset_axis,
		       fastf_t offset, fastf_t text_height,
		       size_t label_length, const struct bview *view,
		       const screen_rect &box_rect)
{
    autodim_placement placement;
    int from_bits[3] = {edge_bits[X], edge_bits[Y], edge_bits[Z]};
    int to_bits[3] = {edge_bits[X], edge_bits[Y], edge_bits[Z]};
    from_bits[dimension_axis] = 0;
    to_bits[dimension_axis] = 1;
    autodim_corner(placement.from, box, from_bits);
    autodim_corner(placement.to, box, to_bits);

    const fastf_t outward_sign = edge_bits[offset_axis] ? 1.0 : -1.0;
    VSCALE(placement.outward, box.axes[offset_axis], outward_sign);
    orient_autodim_text(placement, view);
    vect_t physical_outward;
    VSCALE(physical_outward, placement.outward, placement.offset_sign);
    VJOIN1(placement.dim_from, placement.from, offset, physical_outward);
    VJOIN1(placement.dim_to, placement.to, offset, physical_outward);
    VADD2SCALE(placement.label, placement.dim_from, placement.dim_to, 0.5);
    VJOIN1(placement.label, placement.label, text_height * 0.25,
	physical_outward);

    point_t text_corners[4];
    const fastf_t half_width = 0.5 * AUTODIM_TEXT_WIDTH_SCALE *
	std::max<size_t>(1, label_length) * text_height;
    for (int i = 0; i < 4; ++i) {
	VMOVE(text_corners[i], placement.label);
	VJOIN1(text_corners[i], text_corners[i], (i & 1) ? half_width : -half_width,
	    box.axes[dimension_axis]);
	VJOIN1(text_corners[i], text_corners[i], (i & 2) ? text_height : 0.0,
	    physical_outward);
	double projected[2];
	project_xy(projected, view, text_corners[i]);
	extend_rect(placement.label_rect, projected);
    }

    double center_2d[2], edge_2d[2], label_2d[2];
    point_t edge_midpoint;
    VADD2SCALE(edge_midpoint, placement.from, placement.to, 0.5);
    project_xy(center_2d, view, box.center);
    project_xy(edge_2d, view, edge_midpoint);
    project_xy(label_2d, view, placement.label);
    const double box_diagonal = std::max(SMALL_FASTF,
	hypot(box_rect.xmax - box_rect.xmin, box_rect.ymax - box_rect.ymin));
    const double edge_distance = hypot(edge_2d[X] - center_2d[X],
	edge_2d[Y] - center_2d[Y]);
    const double label_distance = hypot(label_2d[X] - center_2d[X],
	label_2d[Y] - center_2d[Y]);

    double axis_0[2], axis_1[2], outward_1[2];
    project_xy(axis_0, view, placement.from);
    project_xy(axis_1, view, placement.to);
    point_t outward_point;
    VJOIN1(outward_point, placement.from, text_height, physical_outward);
    project_xy(outward_1, view, outward_point);
    const double ux = axis_1[X] - axis_0[X];
    const double uy = axis_1[Y] - axis_0[Y];
    const double vx = outward_1[X] - axis_0[X];
    const double vy = outward_1[Y] - axis_0[Y];
    const double projected_u = hypot(ux, uy);
    const double projected_v = hypot(vx, vy);
    const double orthogonality = (projected_u > SMALL_FASTF && projected_v > SMALL_FASTF) ?
	fabs(ux * vy - uy * vx) / (projected_u * projected_v) : 0.0;
    const double projected_axis_ratio = projected_u / box_diagonal;
    const double edge_on_penalty = projected_axis_ratio <
	AUTODIM_MIN_PROJECTED_AXIS_RATIO ? AUTODIM_EDGE_ON_PENALTY *
	(AUTODIM_MIN_PROJECTED_AXIS_RATIO - projected_axis_ratio) /
	AUTODIM_MIN_PROJECTED_AXIS_RATIO : 0.0;

    const double label_area = std::max(SMALL_FASTF,
	(placement.label_rect.xmax - placement.label_rect.xmin) *
	(placement.label_rect.ymax - placement.label_rect.ymin));
    const double model_overlap = rect_overlap(placement.label_rect, box_rect) / label_area;
    placement.score = AUTODIM_OUTWARD_WEIGHT *
	(label_distance - edge_distance) / box_diagonal +
	AUTODIM_ORTHOGONALITY_WEIGHT * orthogonality -
	AUTODIM_MODEL_OVERLAP_PENALTY * model_overlap - edge_on_penalty;
    return placement;
}


static double
placement_pair_score(const autodim_placement &a, const autodim_placement &b,
		     const struct bview *view, double box_diagonal)
{
    const double area_a = std::max(SMALL_FASTF,
	(a.label_rect.xmax - a.label_rect.xmin) *
	(a.label_rect.ymax - a.label_rect.ymin));
    const double area_b = std::max(SMALL_FASTF,
	(b.label_rect.xmax - b.label_rect.xmin) *
	(b.label_rect.ymax - b.label_rect.ymin));
    double score = -AUTODIM_LABEL_OVERLAP_PENALTY *
	rect_overlap(a.label_rect, b.label_rect) / std::min(area_a, area_b);

    double a0[2], a1[2], b0[2], b1[2], al[2], bl[2];
    project_xy(a0, view, a.dim_from);
    project_xy(a1, view, a.dim_to);
    project_xy(b0, view, b.dim_from);
    project_xy(b1, view, b.dim_to);
    project_xy(al, view, a.label);
    project_xy(bl, view, b.label);
    if (segments_cross(a0, a1, b0, b1))
	score -= AUTODIM_LINE_CROSSING_PENALTY;
    score += std::min(AUTODIM_MAX_SEPARATION_REWARD,
	hypot(al[X] - bl[X], al[Y] - bl[Y]) / box_diagonal);
    return score;
}


static std::vector<autodim_placement>
select_autodim_placements(const struct autodim_box &box,
			  const std::vector<int> &dimension_axes,
			  const std::array<size_t, 3> &label_lengths,
			  fastf_t offset, fastf_t text_height,
			  const struct bview *view)
{
    std::vector<std::vector<autodim_placement>> candidates(dimension_axes.size());
    const screen_rect box_rect = projected_box_rect(box, view);
    for (size_t selected = 0; selected < dimension_axes.size(); ++selected) {
	const int dimension_axis = dimension_axes[selected];
	for (int edge = 0; edge < 4; ++edge) {
	    int edge_bits[3] = {0, 0, 0};
	    int edge_bit = 0;
	    for (int axis = 0; axis < 3; ++axis) {
		if (axis != dimension_axis)
		    edge_bits[axis] = (edge >> edge_bit++) & 1;
	    }
	    for (int offset_axis = 0; offset_axis < 3; ++offset_axis) {
		if (offset_axis == dimension_axis)
		    continue;
		candidates[selected].push_back(make_autodim_placement(box,
		    dimension_axis, edge_bits, offset_axis, offset, text_height,
		    label_lengths[dimension_axis], view, box_rect));
	    }
	}
    }

    const double box_diagonal = std::max(SMALL_FASTF,
	hypot(box_rect.xmax - box_rect.xmin, box_rect.ymax - box_rect.ymin));
    std::vector<autodim_placement> best;
    double best_score = -INFINITY;
    std::vector<autodim_placement> current;
    const auto search = [&](const auto &self, size_t selected, double score) -> void {
	if (selected == candidates.size()) {
	    if (score > best_score + AUTODIM_SCORE_TOLERANCE) {
		best_score = score;
		best = current;
	    }
	    return;
	}
	for (const autodim_placement &candidate : candidates[selected]) {
	    double candidate_score = score + candidate.score;
	    for (const autodim_placement &placed : current)
		candidate_score += placement_pair_score(candidate, placed, view, box_diagonal);
	    current.push_back(candidate);
	    self(self, selected + 1, candidate_score);
	    current.pop_back();
	}
    };
    search(search, 0, 0.0);
    return best;
}


static int
cmd_autodim_impl(void *data, int argc, const char **argv,
		 const point_t cached_corners[8])
{
    struct ged *gedp = static_cast<struct ged *>(data);
    if (annotate_command_messages(gedp, argc, argv,
	"annotate autodim [options] name [object ...]",
	"Create dimensions on selected bounding-box edges"))
	return BRLCAD_OK;
    create_options opts;
    struct bu_opt_desc descs[29];
    int dcnt = add_common_options(descs, 0, opts);
    ANNOT_OPT(descs, dcnt, "", "axes", "x,y,z", &parse_vls_value, &opts.axes,
	"Axes to dimension");
    ANNOT_OPT(descs, dcnt, "", "corner", "xmin,ymin,zmin", &parse_vls_value,
	&opts.corner, "Bounding-box corner carrying the dimensions");
    ANNOT_OPT(descs, dcnt, "", "bounds", "aabb|obb", &parse_vls_value, &opts.bounds,
	"Use axis-aligned or oriented bounds");
    ANNOT_OPT(descs, dcnt, "", "offset", "distance", &bu_opt_fastf_t, &opts.offset,
	"Dimension-line offset in current units");
    ANNOT_OPT(descs, dcnt, "", "units", "unit", &parse_vls_value, &opts.units,
	"Display units");
    ANNOT_OPT(descs, dcnt, "", "precision", "digits", &bu_opt_int, &opts.precision,
	"Digits after the decimal point");
    ANNOT_OPT(descs, dcnt, "", "no-axis-labels", "", NULL, &opts.no_axis_labels,
	"Do not prefix values with X, Y, and Z");
    ANNOT_OPT(descs, dcnt, "u", "no-air", "", NULL, &opts.no_air,
	"Ignore air regions");
    ANNOT_OPT(descs, dcnt, "t", "tight", "", NULL, &opts.tight,
	"Use evaluated geometry bounds");
    BU_OPT_NULL(descs[dcnt]);
    argc--; argv++;
    int remaining = parse_options(gedp, argc, argv, descs, opts);
    if (remaining < 0)
	return BRLCAD_ERROR;
    if (opts.help || remaining < 1) {
	print_option_help(gedp->ged_result_str,
	    "Usage: annotate autodim [options] name [object ...]", descs,
	    "Creates persistent dimensions on AABB or OBB edges.");
	return opts.help ? GED_HELP : BRLCAD_ERROR;
    }
    const char *name = argv[0];
    if (prepare_name(gedp, name) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (bu_vls_strlen(&opts.units) && bu_units_conversion(bu_vls_cstr(&opts.units)) <= 0.0) {
	bu_vls_printf(gedp->ged_result_str, "Unknown unit '%s'", bu_vls_cstr(&opts.units));
	return BRLCAD_ERROR;
    }
	if (!normalize_axes(&opts.axes)) {
	bu_vls_printf(gedp->ged_result_str,
	    "Axes must be a non-repeating selection of x, y, and z");
	return BRLCAD_ERROR;
    }
    const char *axes = bu_vls_cstr(&opts.axes);
    const bool oriented = BU_STR_EQUAL(bu_vls_cstr(&opts.bounds), "obb");
    if (!oriented && !BU_STR_EQUAL(bu_vls_cstr(&opts.bounds), "aabb")) {
	bu_vls_printf(gedp->ged_result_str,
	    "Bounds must be 'aabb' or 'obb', not '%s'", bu_vls_cstr(&opts.bounds));
	return BRLCAD_ERROR;
    }
    int bits[3];
    const bool automatic_corner = BU_STR_EQUAL(bu_vls_cstr(&opts.corner), "auto");
    if (corner_bits(bu_vls_cstr(&opts.corner), bits)) {
	bu_vls_printf(gedp->ged_result_str,
	    "Corner must be auto or an x/y/z min/max triple");
	return BRLCAD_ERROR;
    }

    std::vector<const char *> objects;
    std::vector<std::string> displayed;
    for (int i = 1; i < remaining; ++i)
	objects.push_back(argv[i]);
    if (objects.empty()) {
	struct display_list *gdlp;
	for (BU_LIST_FOR(gdlp, display_list, (struct bu_list *)ged_dl(gedp))) {
	    if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    displayed.emplace_back(bu_vls_cstr(&gdlp->dl_path));
	}
	for (const std::string &object : displayed)
	    objects.push_back(object.c_str());
    }
    if (objects.empty()) {
	bu_vls_printf(gedp->ged_result_str,
	    "No objects were specified and no database objects are displayed");
	return BRLCAD_ERROR;
    }

    struct autodim_box box;
    if (cached_corners) {
	for (int corner = 0; corner < 8; ++corner)
	    VMOVE(box.corners[corner], cached_corners[corner]);
    } else if (oriented) {
	if (_ged_obj_oriented_bounds(gedp, static_cast<int>(objects.size()),
		objects.data(), !opts.no_air, opts.tight, box.corners) != BRLCAD_OK) {
	    bu_vls_printf(gedp->ged_result_str,
		"Unable to calculate evaluated oriented bounds");
	    return BRLCAD_ERROR;
	}
    } else {
	point_t bmin, bmax;
	const int bounds_ret = opts.tight ?
	    _ged_obj_tight_bounds(gedp, static_cast<int>(objects.size()), objects.data(),
		!opts.no_air, bmin, bmax) :
	    rt_obj_bounds(gedp->ged_result_str, gedp->dbip,
		static_cast<int>(objects.size()), objects.data(), !opts.no_air, bmin, bmax);
	if (bounds_ret & BRLCAD_ERROR)
	    return BRLCAD_ERROR;
	set_aabb_corners(box, bmin, bmax);
    }
    if (prepare_autodim_box(box) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "Cannot dimension degenerate bounds");
	return BRLCAD_ERROR;
    }

    const bool automatic_text_height = opts.text_height <= 0.0;
    const bool automatic_offset = !std::isfinite(opts.offset);
    fastf_t diagonal = 2.0 * sqrt(box.half_extent[X] * box.half_extent[X] +
	box.half_extent[Y] * box.half_extent[Y] +
	box.half_extent[Z] * box.half_extent[Z]);
    if (opts.text_height <= 0.0)
	opts.text_height = diagonal * DEFAULT_TEXT_SCALE * gedp->dbip->dbi_base2local;
    if (!std::isfinite(opts.offset))
	opts.offset = diagonal * DEFAULT_OFFSET_SCALE * gedp->dbip->dbi_base2local;

    const char axis_names[] = {'x', 'y', 'z'};
    std::vector<int> dimension_axes;
    std::array<size_t, 3> label_lengths = {0, 0, 0};
    for (int axis_index = 0; axis_index < 3; ++axis_index) {
	if (!axis_selected(axes, axis_names[axis_index]))
	    continue;
	dimension_axes.push_back(axis_index);
	std::string axis_prefix;
	if (!opts.no_axis_labels) {
	    axis_prefix.assign(1, static_cast<char>(toupper(axis_names[axis_index])));
	    axis_prefix += ": ";
	}
	label_lengths[axis_index] = format_measurement(2.0 * box.half_extent[axis_index],
	    opts, gedp, axis_prefix.c_str()).size();
    }

    std::vector<autodim_placement> placements;
    if (gedp->ged_gvp && automatic_corner) {
	placements = select_autodim_placements(box, dimension_axes, label_lengths,
	    opts.offset * gedp->dbip->dbi_local2base,
	    opts.text_height * gedp->dbip->dbi_local2base, gedp->ged_gvp);
    }
    if (placements.size() != dimension_axes.size()) {
	placements.clear();
	for (int dimension_axis : dimension_axes) {
	    autodim_placement placement;
	    int edge_bits[3] = {bits[X], bits[Y], bits[Z]};
	    int opposite_bits[3] = {bits[X], bits[Y], bits[Z]};
	    opposite_bits[dimension_axis] = !opposite_bits[dimension_axis];
	    autodim_corner(placement.from, box, edge_bits);
	    autodim_corner(placement.to, box, opposite_bits);
	    const int offset_axis = dimension_axis == X ? Y : X;
	    VSCALE(placement.outward, box.axes[offset_axis],
		edge_bits[offset_axis] ? 1.0 : -1.0);
	    if (gedp->ged_gvp)
		orient_autodim_text(placement, gedp->ged_gvp);
	    placements.push_back(placement);
	}
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct wmember members = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&members.l);
    struct bu_vls member_names = BU_VLS_INIT_ZERO;
    std::vector<std::string> created;
    for (int axis_index = 0; axis_index < 3; ++axis_index) {
	if (!axis_selected(axes, axis_names[axis_index]))
	    continue;
	std::string child = std::string(name) + "." + axis_names[axis_index];
	if (db_lookup(gedp->dbip, child.c_str(), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Object '%s' already exists", child.c_str());
	    bu_vls_free(&member_names);
	    return BRLCAD_ERROR;
	}
    }
    size_t placement_index = 0;
    for (int axis_index = 0; axis_index < 3; ++axis_index) {
	char axis = axis_names[axis_index];
	if (!axis_selected(axes, axis))
	    continue;
	const autodim_placement &placement = placements[placement_index++];
	std::string child = std::string(name) + "." + axis;
	std::string saved_prefix(bu_vls_cstr(&opts.prefix));
	if (!opts.no_axis_labels) {
	    std::string prefix(1, static_cast<char>(toupper(axis)));
	    prefix += ": ";
	    bu_vls_strcpy(&opts.prefix, prefix.c_str());
	}
	int ret = write_linear_dimension(gedp, child.c_str(), placement.from,
	    placement.to, placement.outward, opts, "autodim-member", "",
	    placement.offset_sign);
	bu_vls_strcpy(&opts.prefix, saved_prefix.c_str());
	if (ret != BRLCAD_OK) {
	    remove_created(gedp, created);
	    bu_vls_free(&member_names);
	    return ret;
	}
	created.push_back(child);
	(void)mk_addmember(child.c_str(), &members.l, NULL, WMOP_UNION);
	if (bu_vls_strlen(&member_names))
	    bu_vls_putc(&member_names, ' ');
	bu_vls_strcat(&member_names, child.c_str());
    }
    if (mk_lcomb(wdbp, name, &members, 0, NULL, NULL, NULL, 0)) {
	bu_vls_printf(gedp->ged_result_str, "Unable to create annotation group '%s'", name);
	remove_created(gedp, created);
	bu_vls_free(&member_names);
	return BRLCAD_ERROR;
    }
    created.push_back(name);
    struct directory *group_dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    int attr_ret = group_dp == RT_DIR_NULL ? BRLCAD_ERROR :
	db5_get_attributes(gedp->dbip, &avs, group_dp);
    if (!attr_ret) {
	struct bu_vls sources = BU_VLS_INIT_ZERO;
	struct bu_vls number = BU_VLS_INIT_ZERO;
	for (const char *object : objects) {
	    if (bu_vls_strlen(&sources))
		bu_vls_putc(&sources, ' ');
	    bu_vls_strcat(&sources, object);
	}
	unsigned char rgb[3] = {0, 0, 0};
	if (opts.color.set)
	    bu_color_to_rgb_chars(&opts.color.value, rgb);
	auto add_attr = [&](const char *key, const char *value) {
	    if (!attr_ret && bu_avs_add(&avs, key, value) < 0)
		attr_ret = BRLCAD_ERROR;
	};
	add_attr(ATTR_KIND, "autodim");
	add_attr(ATTR_MEMBERS, bu_vls_cstr(&member_names));
	add_attr(ATTR_SOURCES, bu_vls_cstr(&sources));
	add_attr(ATTR_BOUNDS, bu_vls_cstr(&opts.bounds));
	std::ostringstream serialized_corners;
	serialized_corners << std::setprecision(17);
	for (int corner = 0; corner < 8; ++corner)
	    for (int coordinate = 0; coordinate < 3; ++coordinate)
		serialized_corners << (corner || coordinate ? " " : "")
		    << box.corners[corner][coordinate];
	const std::string corners_value = serialized_corners.str();
	add_attr(ATTR_BOX_CORNERS, corners_value.c_str());
	add_attr(ATTR_AXES, bu_vls_cstr(&opts.axes));
	add_attr(ATTR_CORNER, bu_vls_cstr(&opts.corner));
	add_attr(ATTR_TIGHT, opts.tight ? "1" : "0");
	add_attr(ATTR_NO_AIR, opts.no_air ? "1" : "0");
	bu_vls_sprintf(&number, "%.17g", opts.text_height);
	add_attr(ATTR_TEXT_HEIGHT, bu_vls_cstr(&number));
	add_attr(ATTR_TEXT_HEIGHT_AUTO, automatic_text_height ? "1" : "0");
	bu_vls_sprintf(&number, "%.17g", opts.offset);
	add_attr(ATTR_OFFSET, bu_vls_cstr(&number));
	add_attr(ATTR_OFFSET_AUTO, automatic_offset ? "1" : "0");
	add_attr(ATTR_UNITS, bu_vls_cstr(&opts.units));
	bu_vls_sprintf(&number, "%d", opts.precision);
	add_attr(ATTR_PRECISION, bu_vls_cstr(&number));
	add_attr(ATTR_AXIS_LABELS, opts.no_axis_labels ? "0" : "1");
	add_attr(ATTR_FONT, bu_vls_cstr(&opts.font));
	bu_vls_sprintf(&number, "%.17g", opts.line_width);
	add_attr(ATTR_LINE_WIDTH, bu_vls_cstr(&number));
	add_attr(ATTR_LINE_STYLE, bu_vls_cstr(&opts.line_style));
	add_attr(ATTR_BOLD, opts.bold ? "1" : "0");
	add_attr(ATTR_ITALIC, opts.italic ? "1" : "0");
	if (opts.color.set) {
	    bu_vls_sprintf(&number, "%u/%u/%u", rgb[0], rgb[1], rgb[2]);
	    add_attr(ATTR_COLOR, bu_vls_cstr(&number));
	}
	if (!attr_ret)
	    attr_ret = db5_update_attributes(group_dp, &avs, gedp->dbip);
	bu_vls_free(&number);
	bu_vls_free(&sources);
    }
    bu_avs_free(&avs);
    bu_vls_free(&member_names);
    if (attr_ret) {
	bu_vls_printf(gedp->ged_result_str,
	    "Unable to tag bounding-box annotation '%s'", name);
	remove_created(gedp, created);
	return BRLCAD_ERROR;
    }
    if (draw_created(gedp, name, opts) != BRLCAD_OK)
	return BRLCAD_ERROR;
    bu_vls_printf(gedp->ged_result_str, "Created bounding-box annotation '%s' for",
	name);
    for (const char *object : objects)
	bu_vls_printf(gedp->ged_result_str, " %s", object);
    return BRLCAD_OK;
}


static int
cmd_autodim(void *data, int argc, const char **argv)
{
    return cmd_autodim_impl(data, argc, argv, NULL);
}


static bool
attribute_enabled(const char *value)
{
    return value && BU_STR_EQUAL(value, "1");
}


static void
append_option(std::vector<std::string> &args, const char *option, const char *value)
{
    if (!value)
	return;
    args.emplace_back(option);
    args.emplace_back(value);
}


static int
replace_from_temp(struct ged *gedp, const std::string &destination,
		  const std::string &temporary)
{
    struct directory *destination_dp = db_lookup(gedp->dbip, destination.c_str(),
	LOOKUP_QUIET);
    struct directory *temporary_dp = db_lookup(gedp->dbip, temporary.c_str(),
	LOOKUP_QUIET);
    if (destination_dp == RT_DIR_NULL || temporary_dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, temporary_dp, gedp->dbip, NULL) < 0)
	return BRLCAD_ERROR;
    if (rt_db_put_internal(destination_dp, gedp->dbip, &intern) < 0) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    int ret = db5_get_attributes(gedp->dbip, &avs, temporary_dp) ||
	db5_replace_attributes(destination_dp, &avs, gedp->dbip);
    bu_avs_free(&avs);
    return ret ? BRLCAD_ERROR : BRLCAD_OK;
}


static bool
replacement_is_ready(struct ged *gedp, const std::string &destination,
		     const std::string &temporary)
{
    struct directory *destination_dp = db_lookup(gedp->dbip, destination.c_str(),
	LOOKUP_QUIET);
    struct directory *temporary_dp = db_lookup(gedp->dbip, temporary.c_str(),
	LOOKUP_QUIET);
    if (destination_dp == RT_DIR_NULL || temporary_dp == RT_DIR_NULL)
	return false;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, temporary_dp, gedp->dbip, NULL) < 0)
	return false;
    rt_db_free_internal(&intern);

    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    const bool ready = !db5_get_attributes(gedp->dbip, &avs, temporary_dp);
    bu_avs_free(&avs);
    return ready;
}


static void
remove_update_temporary(struct ged *gedp, const std::vector<std::string> &names)
{
    struct bu_vls saved_result = BU_VLS_INIT_ZERO;
    bu_vls_strcpy(&saved_result, bu_vls_cstr(gedp->ged_result_str));
    for (const std::string &name : names) {
	const char *kill_argv[] = {"kill", "-f", "-q", name.c_str(), NULL};
	(void)ged_exec_kill(gedp, 4, kill_argv);
    }
    bu_vls_strcpy(gedp->ged_result_str, bu_vls_cstr(&saved_result));
    bu_vls_free(&saved_result);
}


static bool
parse_stored_corners(point_t corners[8], const char *value)
{
    if (!value)
	return false;
    std::istringstream input(value);
    for (int corner = 0; corner < 8; ++corner)
	for (int coordinate = 0; coordinate < 3; ++coordinate)
	    if (!(input >> corners[corner][coordinate]))
		return false;
    std::string trailing;
    return !(input >> trailing);
}


static bool
display_path_matches(struct db_i *dbip, const std::string &path,
		     struct directory *annotation_dp)
{
    struct db_full_path full_path;
    if (db_string_to_path(&full_path, dbip, path.c_str()))
	return false;
    const bool matches = full_path.fp_len > 0 &&
	DB_FULL_PATH_GET(&full_path, full_path.fp_len - 1) == annotation_dp;
    db_free_full_path(&full_path);
    return matches;
}


static bool
annotation_is_displayed(struct ged *gedp, struct directory *annotation_dp)
{
    if (gedp->dbi_state && gedp->ged_gvp) {
	DbiState *dbis = static_cast<DbiState *>(gedp->dbi_state);
	BViewState *view_state = dbis->get_view_state(gedp->ged_gvp);
	if (view_state) {
	    for (const std::string &path : view_state->list_drawn_paths(-1, true))
		if (display_path_matches(gedp->dbip, path, annotation_dp))
		    return true;
	}
    }
    struct display_list *displayed;
    for (BU_LIST_FOR(displayed, display_list, (struct bu_list *)ged_dl(gedp))) {
	struct directory *display_dp = static_cast<struct directory *>(displayed->dl_dp);
	if (display_dp == annotation_dp ||
	    display_path_matches(gedp->dbip, bu_vls_cstr(&displayed->dl_path),
		annotation_dp))
	    return true;
    }
    return false;
}


static int
update_leader(struct ged *gedp, const char *name, bool view_only)
{
    struct directory *leader_dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (leader_dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Object '%s' does not exist", name);
	return BRLCAD_ERROR;
    }
    const bool was_displayed = annotation_is_displayed(gedp, leader_dp);
    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    if (db5_get_attributes(gedp->dbip, &avs, leader_dp)) {
	bu_vls_printf(gedp->ged_result_str, "Unable to read annotation '%s'", name);
	return BRLCAD_ERROR;
    }
    const char *kind = bu_avs_get(&avs, ATTR_KIND);
    const char *text = bu_avs_get(&avs, ATTR_LEADER_TEXT);
    point_t stored_target, stored_at;
    if (!kind || !BU_STR_EQUAL(kind, "leader") || !text ||
	!parse_point(stored_target, bu_avs_get(&avs, ATTR_LEADER_TARGET)) ||
	!parse_point(stored_at, bu_avs_get(&avs, ATTR_LEADER_AT))) {
	bu_avs_free(&avs);
	bu_vls_printf(gedp->ged_result_str,
	    "Annotation '%s' is not an updatable leader", name);
	return BRLCAD_ERROR;
    }

    const std::string temporary_name = std::string(name) + ".annotate-update-tmp";
    if (db_lookup(gedp->dbip, temporary_name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_avs_free(&avs);
	bu_vls_printf(gedp->ged_result_str, "Temporary object '%s' already exists",
	    temporary_name.c_str());
	return BRLCAD_ERROR;
    }
    std::vector<std::string> args = {"leader", "--no-draw"};
    const char *source = bu_avs_get(&avs, ATTR_SOURCES);
    const bool automatic_target = attribute_enabled(bu_avs_get(&avs,
	ATTR_LEADER_TARGET_AUTO));
    const bool automatic_at = attribute_enabled(bu_avs_get(&avs, ATTR_LEADER_AT_AUTO));
    if (source) {
	args.emplace_back("--for");
	args.emplace_back(source);
    }
    if (view_only || !automatic_target) {
	args.emplace_back("--target");
	args.emplace_back(format_local_point(stored_target, gedp));
    }
    if (view_only || !automatic_at) {
	args.emplace_back("--at");
	args.emplace_back(format_local_point(stored_at, gedp));
    }
    if (!attribute_enabled(bu_avs_get(&avs, ATTR_TEXT_HEIGHT_AUTO)))
	append_option(args, "--text-height", bu_avs_get(&avs, ATTR_TEXT_HEIGHT));
    append_option(args, "--font", bu_avs_get(&avs, ATTR_FONT));
    append_option(args, "--line-width", bu_avs_get(&avs, ATTR_LINE_WIDTH));
    append_option(args, "--line-style", bu_avs_get(&avs, ATTR_LINE_STYLE));
    append_option(args, "--color", bu_avs_get(&avs, ATTR_COLOR));
    if (attribute_enabled(bu_avs_get(&avs, ATTR_BOLD)))
	args.emplace_back("--bold");
    if (attribute_enabled(bu_avs_get(&avs, ATTR_ITALIC)))
	args.emplace_back("--italic");
    if (attribute_enabled(bu_avs_get(&avs, ATTR_LEADER_SCREEN_SPACE)))
	args.emplace_back("--screen-space");
    append_option(args, "--dpi", bu_avs_get(&avs, ATTR_LEADER_DPI));
    args.emplace_back(temporary_name);
    args.emplace_back(text);

    std::vector<const char *> argv;
    argv.reserve(args.size());
    for (const std::string &arg : args)
	argv.push_back(arg.c_str());
    if (cmd_leader(gedp, static_cast<int>(argv.size()), argv.data()) != BRLCAD_OK) {
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }
    if (gedp->dbi_state)
	static_cast<DbiState *>(gedp->dbi_state)->update();
    if (!replacement_is_ready(gedp, name, temporary_name)) {
	bu_avs_free(&avs);
	remove_update_temporary(gedp, {temporary_name});
	bu_vls_printf(gedp->ged_result_str,
	    "Unable to prepare replacement annotation '%s'", name);
	return BRLCAD_ERROR;
    }
    const char *erase_argv[] = {"erase", name, NULL};
    (void)ged_exec_erase(gedp, 2, erase_argv);
    int ret = replace_from_temp(gedp, name, temporary_name);
    if (ret == BRLCAD_OK && view_only &&
	db5_replace_attributes(leader_dp, &avs, gedp->dbip))
	ret = BRLCAD_ERROR;
    bu_avs_free(&avs);
    remove_update_temporary(gedp, {temporary_name});
    if (ret != BRLCAD_OK) {
	if (was_displayed) {
	    create_options draw_opts;
	    (void)draw_created(gedp, name, draw_opts);
	}
	bu_vls_printf(gedp->ged_result_str, "Unable to replace annotation '%s'", name);
	return BRLCAD_ERROR;
    }
    if (was_displayed) {
	create_options draw_opts;
	if (draw_created(gedp, name, draw_opts) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }
    bu_vls_printf(gedp->ged_result_str, view_only ?
	"Updated leader '%s' for the current view" :
	"Updated leader '%s' from current geometry and view", name);
    return BRLCAD_OK;
}


static int
update_autodim(struct ged *gedp, const char *name, bool view_only)
{
    struct directory *group_dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (group_dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Object '%s' does not exist", name);
	return BRLCAD_ERROR;
    }
    const bool was_displayed = annotation_is_displayed(gedp, group_dp);

    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    if (db5_get_attributes(gedp->dbip, &avs, group_dp)) {
	bu_vls_printf(gedp->ged_result_str, "Unable to read annotation '%s'", name);
	return BRLCAD_ERROR;
    }
    const char *kind = bu_avs_get(&avs, ATTR_KIND);
    const char *sources_attr = bu_avs_get(&avs, ATTR_SOURCES);
    const char *axes_attr = bu_avs_get(&avs, ATTR_AXES);
    if (!kind || !BU_STR_EQUAL(kind, "autodim") || !sources_attr || !axes_attr) {
	bu_avs_free(&avs);
	bu_vls_printf(gedp->ged_result_str,
	    "Annotation '%s' is not an updatable autodim group", name);
	return BRLCAD_ERROR;
    }
    point_t cached_corners[8];
    if (view_only &&
	!parse_stored_corners(cached_corners, bu_avs_get(&avs, ATTR_BOX_CORNERS))) {
	bu_avs_free(&avs);
	bu_vls_printf(gedp->ged_result_str,
	    "Annotation '%s' does not contain reusable bounding-box data", name);
	return BRLCAD_ERROR;
    }

    const std::string temporary_name = std::string(name) + ".annotate-update-tmp";
    if (db_lookup(gedp->dbip, temporary_name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_avs_free(&avs);
	bu_vls_printf(gedp->ged_result_str, "Temporary object '%s' already exists",
	    temporary_name.c_str());
	return BRLCAD_ERROR;
    }

    std::vector<std::string> args = {"autodim", "--no-draw"};
    append_option(args, "--bounds", bu_avs_get(&avs, ATTR_BOUNDS));
    append_option(args, "--axes", axes_attr);
    append_option(args, "--corner", bu_avs_get(&avs, ATTR_CORNER));
    append_option(args, "--units", bu_avs_get(&avs, ATTR_UNITS));
    append_option(args, "--precision", bu_avs_get(&avs, ATTR_PRECISION));
    append_option(args, "--font", bu_avs_get(&avs, ATTR_FONT));
    append_option(args, "--line-style", bu_avs_get(&avs, ATTR_LINE_STYLE));
    const char *line_width = bu_avs_get(&avs, ATTR_LINE_WIDTH);
    if (line_width && atof(line_width) > 0.0)
	append_option(args, "--line-width", line_width);
    if (!attribute_enabled(bu_avs_get(&avs, ATTR_TEXT_HEIGHT_AUTO)))
	append_option(args, "--text-height", bu_avs_get(&avs, ATTR_TEXT_HEIGHT));
    if (!attribute_enabled(bu_avs_get(&avs, ATTR_OFFSET_AUTO)))
	append_option(args, "--offset", bu_avs_get(&avs, ATTR_OFFSET));
    if (attribute_enabled(bu_avs_get(&avs, ATTR_TIGHT)))
	args.emplace_back("--tight");
    if (attribute_enabled(bu_avs_get(&avs, ATTR_NO_AIR)))
	args.emplace_back("--no-air");
    if (!attribute_enabled(bu_avs_get(&avs, ATTR_AXIS_LABELS)))
	args.emplace_back("--no-axis-labels");
    if (attribute_enabled(bu_avs_get(&avs, ATTR_BOLD)))
	args.emplace_back("--bold");
    if (attribute_enabled(bu_avs_get(&avs, ATTR_ITALIC)))
	args.emplace_back("--italic");
    append_option(args, "--color", bu_avs_get(&avs, ATTR_COLOR));
    args.push_back(temporary_name);

    std::istringstream sources_input(sources_attr);
    std::string source;
    while (sources_input >> source)
	args.push_back(source);
    const std::string axes(axes_attr);
    bu_avs_free(&avs);

    std::vector<const char *> argv;
    argv.reserve(args.size());
    for (const std::string &arg : args)
	argv.push_back(arg.c_str());
    if (cmd_autodim_impl(gedp, static_cast<int>(argv.size()), argv.data(),
	view_only ? cached_corners : NULL) != BRLCAD_OK)
	return BRLCAD_ERROR;
    /* Creation callbacks retain directory pointers until the view-state index
     * consumes them.  Flush additions before removing the temporary objects. */
    if (gedp->dbi_state)
	static_cast<DbiState *>(gedp->dbi_state)->update();

    const char axis_names[] = {'x', 'y', 'z'};
    bool replacements_ready = replacement_is_ready(gedp, name, temporary_name);
    for (char axis : axis_names) {
	if (!axis_selected(axes.c_str(), axis))
	    continue;
	const std::string real_member = std::string(name) + "." + axis;
	const std::string temporary_member = temporary_name + "." + axis;
	replacements_ready = replacements_ready && replacement_is_ready(gedp,
	    real_member, temporary_member);
    }
    if (!replacements_ready) {
	std::vector<std::string> temporary_objects = {temporary_name};
	for (char axis : axis_names)
	    if (axis_selected(axes.c_str(), axis))
		temporary_objects.push_back(temporary_name + "." + axis);
	remove_update_temporary(gedp, temporary_objects);
	bu_vls_printf(gedp->ged_result_str,
	    "Unable to prepare replacement annotation '%s'", name);
	return BRLCAD_ERROR;
    }
    const char *erase_argv[] = {"erase", name, NULL};
    (void)ged_exec_erase(gedp, 2, erase_argv);

    std::vector<std::string> temporary_objects = {temporary_name};
    struct bu_vls real_members = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_OK;
    for (char axis : axis_names) {
	if (!axis_selected(axes.c_str(), axis))
	    continue;
	const std::string real_member = std::string(name) + "." + axis;
	const std::string temporary_member = temporary_name + "." + axis;
	temporary_objects.push_back(temporary_member);
	if (bu_vls_strlen(&real_members))
	    bu_vls_putc(&real_members, ' ');
	bu_vls_strcat(&real_members, real_member.c_str());
	if (replace_from_temp(gedp, real_member, temporary_member) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }

    struct directory *temporary_group = db_lookup(gedp->dbip, temporary_name.c_str(),
	LOOKUP_QUIET);
    struct bu_attribute_value_set updated_avs = BU_AVS_INIT_ZERO;
    if (ret == BRLCAD_OK &&
	(db5_get_attributes(gedp->dbip, &updated_avs, temporary_group) ||
	 bu_avs_add(&updated_avs, ATTR_MEMBERS, bu_vls_cstr(&real_members)) < 0 ||
	 db5_replace_attributes(group_dp, &updated_avs, gedp->dbip)))
	ret = BRLCAD_ERROR;
    bu_avs_free(&updated_avs);
    bu_vls_free(&real_members);

    remove_update_temporary(gedp, temporary_objects);
    if (ret != BRLCAD_OK) {
	if (was_displayed) {
	    create_options draw_opts;
	    (void)draw_created(gedp, name, draw_opts);
	}
	bu_vls_printf(gedp->ged_result_str, "Unable to replace annotation '%s'", name);
	return BRLCAD_ERROR;
    }

    if (was_displayed) {
	create_options draw_opts;
	if (draw_created(gedp, name, draw_opts) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }
    if (view_only)
	bu_vls_printf(gedp->ged_result_str,
	    "Updated autodim '%s' from its stored bounds and current view", name);
    else
	bu_vls_printf(gedp->ged_result_str,
	    "Updated autodim '%s' from current geometry and view", name);
    return BRLCAD_OK;
}


static int
cmd_update(void *data, int argc, const char **argv)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    if (annotate_command_messages(gedp, argc, argv,
	"annotate update [--view-only] name ...",
	"Refresh autodim and leader annotations"))
	return BRLCAD_OK;
    argc--; argv++;
    int help = 0;
    int view_only = 0;
    struct bu_opt_desc desc[] = {
	{"h", "help", "", NULL, &help, "Print help"},
	{"", "view-only", "", NULL, &view_only,
	    "Reuse stored bounds and update view-dependent presentation"},
	BU_OPT_DESC_NULL
    };
    struct bu_vls message = BU_VLS_INIT_ZERO;
    const int remaining = bu_opt_parse(&message, argc, argv, desc);
    if (remaining < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&message));
	bu_vls_free(&message);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&message);
    if (help || remaining < 1) {
	print_option_help(gedp->ged_result_str,
	    "Usage: annotate update [--view-only] name ...", desc,
	    "Refreshes autodim and leader annotations from stored creation settings.");
	return help ? GED_HELP : BRLCAD_ERROR;
    }
    for (int i = 0; i < remaining; ++i) {
	struct directory *dp = db_lookup(gedp->dbip, argv[i], LOOKUP_QUIET);
	struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	const char *kind = NULL;
	if (dp != RT_DIR_NULL && !db5_get_attributes(gedp->dbip, &avs, dp))
	    kind = bu_avs_get(&avs, ATTR_KIND);
	const bool is_autodim = kind && BU_STR_EQUAL(kind, "autodim");
	const bool is_leader = kind && BU_STR_EQUAL(kind, "leader");
	bu_avs_free(&avs);
	if ((!is_autodim && !is_leader) ||
	    (is_autodim && update_autodim(gedp, argv[i], view_only) != BRLCAD_OK) ||
	    (is_leader && update_leader(gedp, argv[i], view_only) != BRLCAD_OK)) {
	    if (!is_autodim && !is_leader)
		bu_vls_printf(gedp->ged_result_str,
		    "Annotation '%s' is not an updatable autodim group or leader", argv[i]);
	    return BRLCAD_ERROR;
	}
    }
    return BRLCAD_OK;
}


static int
annotation_kind(struct db_i *dbip, struct directory *dp, std::string *kind)
{
    if (!dp)
	return 0;
    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    const char *value = NULL;
    if (!db5_get_attributes(dbip, &avs, dp))
	value = bu_avs_get(&avs, ATTR_KIND);
    if (kind)
	*kind = value ? value : (dp->d_minor_type == ID_ANNOT ? "annotation" : "");
    int ret = value || dp->d_minor_type == ID_ANNOT;
    bu_avs_free(&avs);
    return ret;
}


static void
collect_tree_annotations(struct ged *gedp, struct directory *root,
			 std::set<std::string> &annotations)
{
    if (annotation_kind(gedp->dbip, root, NULL)) {
	annotations.insert(root->d_namep);
	return;
    }

    struct bu_ptbl contained = BU_PTBL_INIT_ZERO;
    (void)db_search(&contained, DB_SEARCH_TREE | DB_SEARCH_RETURN_UNIQ_DP,
	"-type annot", 1, &root, gedp->dbip, NULL, NULL, NULL);
    for (size_t i = 0; i < BU_PTBL_LEN(&contained); ++i) {
	struct directory *dp = reinterpret_cast<struct directory *>(BU_PTBL_GET(&contained, i));
	annotations.insert(dp->d_namep);
    }
    db_search_free(&contained);

    std::set<std::string> tree_names;
    tree_names.insert(root->d_namep);
    struct bu_ptbl descendants = BU_PTBL_INIT_ZERO;
    (void)db_search(&descendants, DB_SEARCH_TREE | DB_SEARCH_RETURN_UNIQ_DP,
	"-name *", 1, &root, gedp->dbip, NULL, NULL, NULL);
    for (size_t i = 0; i < BU_PTBL_LEN(&descendants); ++i) {
	struct directory *dp = reinterpret_cast<struct directory *>(BU_PTBL_GET(&descendants, i));
	tree_names.insert(dp->d_namep);
    }
    db_search_free(&descendants);

    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	    bu_avs_free(&avs);
	    continue;
	}
	const char *kind = bu_avs_get(&avs, ATTR_KIND);
	const char *sources = bu_avs_get(&avs, ATTR_SOURCES);
	if (kind && sources) {
	    std::istringstream input(sources);
	    std::string source;
	    while (input >> source) {
		if (tree_names.find(source) != tree_names.end()) {
		    annotations.insert(dp->d_namep);
		    break;
		}
	    }
	}
	bu_avs_free(&avs);
    } FOR_ALL_DIRECTORY_END;
}


static int
cmd_visibility(void *data, int argc, const char **argv, bool visible)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    const char *command = visible ? "show" : "hide";
    const char *purpose = visible ?
	"Draw annotations by name or associated geometry tree" :
	"Erase annotations by name or associated geometry tree";
    const std::string usage = std::string("annotate ") + command +
	" [--all] name|tree ...";
    if (annotate_command_messages(gedp, argc, argv, usage.c_str(), purpose))
	return BRLCAD_OK;
    argc--; argv++;
    bool all = argc == 1 && BU_STR_EQUAL(argv[0], "--all");
    bool help = argc > 0 && (BU_STR_EQUAL(argv[0], "--help") ||
	BU_STR_EQUAL(argv[0], "-h"));
    bool invalid_all = !all && argc > 0 && BU_STR_EQUAL(argv[0], "--all");
    if ((!all && argc < 1) || help || invalid_all) {
	bu_vls_printf(gedp->ged_result_str, "Usage: annotate %s [--all] name|tree ...",
	    visible ? "show" : "hide");
	return help ? GED_HELP : BRLCAD_ERROR;
    }

    std::set<std::string> annotations;
    if (all) {
	struct directory *dp;
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	    std::string kind;
	    if (annotation_kind(gedp->dbip, dp, &kind) && kind != "autodim-member")
		annotations.insert(dp->d_namep);
	} FOR_ALL_DIRECTORY_END;
    } else {
	for (int i = 0; i < argc; ++i) {
	    struct directory *root = db_lookup(gedp->dbip, argv[i], LOOKUP_QUIET);
	    if (root == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "Object '%s' does not exist", argv[i]);
		return BRLCAD_ERROR;
	    }
	    collect_tree_annotations(gedp, root, annotations);
	}
    }

    if (annotations.empty()) {
	bu_vls_printf(gedp->ged_result_str, "No annotations found");
	return BRLCAD_OK;
    }
    if (visible) {
	create_options opts;
	for (const std::string &name : annotations) {
	    if (draw_created(gedp, name.c_str(), opts) != BRLCAD_OK)
		return BRLCAD_ERROR;
	}
    } else {
	std::vector<const char *> erase_argv = {"erase"};
	for (const std::string &name : annotations)
	    erase_argv.push_back(name.c_str());
	if (ged_exec(gedp, static_cast<int>(erase_argv.size()), erase_argv.data()) != BRLCAD_OK)
	    return BRLCAD_ERROR;
	bu_vls_trunc(gedp->ged_result_str, 0);
    }
    bu_vls_printf(gedp->ged_result_str, "%s", visible ? "Shown:" : "Hidden:");
    for (const std::string &name : annotations)
	bu_vls_printf(gedp->ged_result_str, " %s", name.c_str());
    return BRLCAD_OK;
}


static int
cmd_show(void *data, int argc, const char **argv)
{
    return cmd_visibility(data, argc, argv, true);
}


static int
cmd_hide(void *data, int argc, const char **argv)
{
    return cmd_visibility(data, argc, argv, false);
}


static int
cmd_list(void *data, int argc, const char **argv)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    if (annotate_command_messages(gedp, argc, argv,
	"annotate list [pattern ...]", "List persistent annotation objects"))
	return BRLCAD_OK;
    if (argc == 2 && (BU_STR_EQUAL(argv[1], "--help") ||
	BU_STR_EQUAL(argv[1], "-h"))) {
	bu_vls_printf(gedp->ged_result_str,
	    "Usage: annotate list [pattern ...]");
	return GED_HELP;
    }
    argc--; argv++;
    struct directory *dp;
    bool first = true;
    FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	bool matched = argc == 0;
	for (int i = 0; i < argc && !matched; ++i)
	    matched = !bu_path_match(argv[i], dp->d_namep, 0);
	if (!annotation_kind(gedp->dbip, dp, NULL) || !matched)
	    continue;
	if (!first)
	    bu_vls_putc(gedp->ged_result_str, '\n');
	bu_vls_strcat(gedp->ged_result_str, dp->d_namep);
	first = false;
    } FOR_ALL_DIRECTORY_END;
    return BRLCAD_OK;
}


static int
cmd_info(void *data, int argc, const char **argv)
{
    struct ged *gedp = static_cast<struct ged *>(data);
    if (annotate_command_messages(gedp, argc, argv,
	"annotate info name", "Describe a persistent annotation object"))
	return BRLCAD_OK;
    if (argc == 2 && (BU_STR_EQUAL(argv[1], "--help") ||
	BU_STR_EQUAL(argv[1], "-h"))) {
	bu_vls_printf(gedp->ged_result_str, "Usage: annotate info name");
	return GED_HELP;
    }
    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: annotate info name");
	return argc == 1 ? GED_HELP : BRLCAD_ERROR;
    }
    struct directory *dp = db_lookup(gedp->dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Annotation '%s' does not exist", argv[1]);
	return BRLCAD_ERROR;
    }
    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str,
	    "Unable to read attributes for '%s'", argv[1]);
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }
    const char *kind = bu_avs_get(&avs, ATTR_KIND);
    if (!kind && dp->d_minor_type != ID_ANNOT) {
	bu_vls_printf(gedp->ged_result_str, "Object '%s' is not an annotation", argv[1]);
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gedp->ged_result_str, "name: %s\nkind: %s", argv[1],
	kind ? kind : "annotation");
    const char *sources = bu_avs_get(&avs, ATTR_SOURCES);
    const char *members = bu_avs_get(&avs, ATTR_MEMBERS);
    const char *bounds = bu_avs_get(&avs, ATTR_BOUNDS);
    const char *axes = bu_avs_get(&avs, ATTR_AXES);
    const char *corner = bu_avs_get(&avs, ATTR_CORNER);
    const char *target = bu_avs_get(&avs, ATTR_LEADER_TARGET);
    const char *at = bu_avs_get(&avs, ATTR_LEADER_AT);
    const char *screen_space = bu_avs_get(&avs, ATTR_LEADER_SCREEN_SPACE);
    const char *dpi = bu_avs_get(&avs, ATTR_LEADER_DPI);
    if (sources)
	bu_vls_printf(gedp->ged_result_str, "\nsources: %s", sources);
    if (members)
	bu_vls_printf(gedp->ged_result_str, "\nmembers: %s", members);
    if (bounds)
	bu_vls_printf(gedp->ged_result_str, "\nbounds: %s", bounds);
    if (axes)
	bu_vls_printf(gedp->ged_result_str, "\naxes: %s", axes);
    if (corner)
	bu_vls_printf(gedp->ged_result_str, "\ncorner: %s", corner);
    if (target)
	bu_vls_printf(gedp->ged_result_str, "\ntarget: %s", target);
    if (at)
	bu_vls_printf(gedp->ged_result_str, "\nlabel: %s", at);
    if (screen_space)
	bu_vls_printf(gedp->ged_result_str, "\nscreen-space: %s", screen_space);
    if (dpi)
	bu_vls_printf(gedp->ged_result_str, "\ndpi: %s", dpi);
    if (dp->d_minor_type == ID_ANNOT) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) == ID_ANNOT) {
	    struct rt_annot_internal *annotation =
		static_cast<struct rt_annot_internal *>(intern.idb_ptr);
	    bu_vls_printf(gedp->ged_result_str,
		"\nspace: %s\nsegments: %zu",
		(annotation->flags & RT_ANNOT_MODEL_SPACE) ? "model" : "view",
		annotation->ant.count);
	    rt_db_free_internal(&intern);
	} else {
	    bu_vls_printf(gedp->ged_result_str,
		"\nwarning: unable to read annotation geometry");
	}
    }
    bu_avs_free(&avs);
    return BRLCAD_OK;
}


static const struct bu_cmdtab annotate_commands[] = {
    {"text", cmd_text},
    {"leader", cmd_leader},
    {"dimension", cmd_dimension},
    {"autodim", cmd_autodim},
    {"update", cmd_update},
    {"show", cmd_show},
    {"hide", cmd_hide},
    {"list", cmd_list},
    {"info", cmd_info},
    {NULL, NULL}
};


} // namespace


extern "C" int
ged_annotate_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    argc--; argv++;
    int help = 0;
    struct bu_opt_desc descs[2];
    BU_OPT(descs[0], "h", "help", "", NULL, &help, "Print help");
    BU_OPT_NULL(descs[1]);
    const char *usage = "[options] subcommand [args]";
    if (!argc) {
	_ged_subcmd_help(gedp, descs, annotate_commands, "annotate", usage, gedp, 0, NULL);
	return GED_HELP;
    }
    if (BU_STR_EQUAL(argv[0], "help")) {
	if (argc > 1)
	    _ged_subcmd_help(gedp, descs, annotate_commands, "annotate", usage,
		gedp, argc - 1, argv + 1);
	else
	    _ged_subcmd_help(gedp, descs, annotate_commands, "annotate", usage,
		gedp, 0, NULL);
	return GED_HELP;
    }
    if (BU_STR_EQUAL(argv[0], "-h") || BU_STR_EQUAL(argv[0], "--help")) {
	if (argc > 1)
	    _ged_subcmd_help(gedp, descs, annotate_commands, "annotate", usage,
		gedp, argc - 1, argv + 1);
	else
	    _ged_subcmd_help(gedp, descs, annotate_commands, "annotate", usage,
		gedp, 0, NULL);
	return GED_HELP;
    }
    if (!BU_STR_EQUAL(argv[0], "list") && !BU_STR_EQUAL(argv[0], "info") &&
	!BU_STR_EQUAL(argv[0], "show") && !BU_STR_EQUAL(argv[0], "hide"))
	GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    int ret = BRLCAD_ERROR;
	if (bu_cmd(annotate_commands, argc, argv, 0, gedp, &ret) == BRLCAD_OK)
	return ret;
    bu_vls_printf(gedp->ged_result_str, "Unknown annotate subcommand '%s'\n", argv[0]);
    _ged_subcmd_help(gedp, descs, annotate_commands, "annotate", usage, gedp, 0, NULL);
    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

#define GED_ANNOTATE_COMMANDS(X, XID) \
    X(annotate, ged_annotate_core, GED_CMD_DEFAULT)

GED_DECLARE_COMMAND_SET(GED_ANNOTATE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_annotate", 1, GED_ANNOTATE_COMMANDS)

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
