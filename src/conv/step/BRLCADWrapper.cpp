/*                 BRLCADWrapper.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
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
/** @file step/BRLCADWrapper.cpp
 *
 * C++ wrapper to BRL-CAD database functions.
 *
 */

#include "common.h"
#include <stdlib.h>

/* interface header */
#include "./BRLCADWrapper.h"
#include "STEPMetadata.h"
#include "STEPString.h"
#include "rt/primitives/annot.h"
#include "rt/primitives/datum.h"

/* system headers */
#include <cmath>
#include <sstream>
#include <iostream>
#include <climits>
#include <vector>


BRLCADWrapper::BRLCADWrapper()
    : outfp(NULL), dbip(NULL), anonymous_name_counter(0), dry_run(false)
{
    /* This object is converter infrastructure, never an imported product. */
    allocated_names[brlcad::step::STEP_METADATA_OBJECT] = INT64_MIN;
}


BRLCADWrapper::~BRLCADWrapper()
{
    Close();
}

bool
BRLCADWrapper::load(std::string &flnm)
{

    if (dry_run)
	return true;

    /* open brlcad instance */
    if ((dbip = db_open(flnm.c_str(), DB_OPEN_READONLY)) == DBI_NULL) {
	bu_log("Cannot open input file (%s)\n", flnm.c_str());
	return false;
    }
    if (db_dirbuild(dbip)) {
	bu_log("ERROR: db_dirbuild failed: (%s)\n", flnm.c_str());
	return false;
    }

    return true;
}


bool
BRLCADWrapper::OpenFile(std::string &flnm)
{
    //TODO: need to check to make sure we aren't overwriting

    if (dry_run)
	return true;

    /* open brlcad instance */
    if ((outfp = wdb_fopen(flnm.c_str())) == NULL) {
	bu_log("Cannot open output file (%s)\n", flnm.c_str());
	return false;
    }

    // hold on to output filename
    filename = flnm.c_str();

    mk_id(outfp, "Output from STEP converter step-g.");

    return true;
}


bool
BRLCADWrapper::WriteHeader()
{
    if (dry_run)
	return true;

    db5_update_attribute("_GLOBAL", "HEADERINFO", "test header attributes", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERCLASS", "test header classification", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERAPPROVED", "test header approval", outfp->dbip);
    return true;
}


bool
BRLCADWrapper::WriteSphere(const std::string &name, const double *center, double radius,
	int64_t step_id, const std::string &original_name)
{
    if (dry_run)
	return true;
    if (!outfp || !center || radius <= 0.0)
	return false;
    point_t pnt = {center[0], center[1], center[2]};
    if (mk_sph(outfp, name.c_str(), pnt, radius) < 0)
	return false;
    SetAttribute(name, "step:object_role", "representation_item");
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteEllipsoid(const std::string &name, const double *center,
    const double *a, const double *b, const double *c,
    int64_t step_id, const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !center || !a || !b || !c || MAGNITUDE(a) <= SMALL_FASTF ||
	MAGNITUDE(b) <= SMALL_FASTF || MAGNITUDE(c) <= SMALL_FASTF)
	return false;
    point_t v = {center[0], center[1], center[2]};
    vect_t av = {a[0], a[1], a[2]};
    vect_t bv = {b[0], b[1], b[2]};
    vect_t cv = {c[0], c[1], c[2]};
    if (mk_ell(outfp, name.c_str(), v, av, bv, cv) < 0) return false;
    SetAttribute(name, "step:object_role", "representation_item");
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteRcc(const std::string &name, const double *base, const double *height,
	double radius, int64_t step_id, const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !base || !height || radius <= 0.0) return false;
    point_t b = {base[0], base[1], base[2]};
    vect_t h = {height[0], height[1], height[2]};
    if (mk_rcc(outfp, name.c_str(), b, h, radius) < 0) return false;
    SetAttribute(name, "step:object_role", "representation_item");
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteTgc(const std::string &name, const double *base, const double *height,
	const double *a, const double *b, const double *c, const double *d,
	int64_t step_id, const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !base || !height || !a || !b || !c || !d) return false;
    point_t bp = {base[0], base[1], base[2]};
    vect_t hv = {height[0], height[1], height[2]};
    vect_t av = {a[0], a[1], a[2]};
    vect_t bv = {b[0], b[1], b[2]};
    vect_t cv = {c[0], c[1], c[2]};
    vect_t dv = {d[0], d[1], d[2]};
    if (mk_tgc(outfp, name.c_str(), bp, hv, av, bv, cv, dv) < 0) return false;
    SetAttribute(name, "step:object_role", "representation_item");
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteTorus(const std::string &name, const double *center, const double *normal,
	double major_radius, double minor_radius, int64_t step_id,
	const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !center || !normal || major_radius <= 0.0 || minor_radius <= 0.0 ||
	minor_radius >= major_radius) return false;
    point_t c = {center[0], center[1], center[2]};
    vect_t n = {normal[0], normal[1], normal[2]};
    if (mk_tor(outfp, name.c_str(), c, n, major_radius, minor_radius) < 0) return false;
    SetAttribute(name, "step:object_role", "representation_item");
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteHalf(const std::string &name, const double *normal, double distance,
	int64_t step_id, const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !normal) return false;
    vect_t outward = {normal[0], normal[1], normal[2]};
    if (MAGNITUDE(outward) <= SMALL_FASTF) return false;
    VUNITIZE(outward);
    if (mk_half(outfp, name.c_str(), outward, distance) < 0) return false;
    SetAttribute(name, "step:object_role", "representation_item");
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}

bool
BRLCADWrapper::WriteArb8(const std::string &name, const double *points, int64_t step_id,
	const std::string &original_name)
{
    if (dry_run) return true;
    if (!outfp || !points) return false;
    fastf_t vertices[24];
    for (size_t i = 0; i < 24; ++i) vertices[i] = points[i];
    if (mk_arb8(outfp, name.c_str(), vertices) < 0) return false;
    SetAttribute(name, "step:object_role", "representation_item");
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}


bool
BRLCADWrapper::WritePoints(const std::string &name, size_t count,
    const double *points, double display_scale, int64_t step_id,
    const std::string &original_name)
{
    if (dry_run) return count > 0 && points;
    if (!outfp || !points || !count || display_scale <= 0.0) return false;
    std::vector<fastf_t> vertices(count * 3);
    for (size_t i = 0; i < vertices.size(); ++i) vertices[i] = points[i];
    if (mk_pnts(outfp, name.c_str(), RT_PNT_TYPE_PNT, display_scale,
	    count, vertices.data(), NULL, NULL, NULL) < 0)
	return false;
    SetAttribute(name, "step:object_role", "representation_item");
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}


bool
BRLCADWrapper::WriteDatum(const std::string &name, rt_datum_type type,
    const double *origin, const double *direction, const double *x_axis,
    const double *y_axis, double display_scale, int64_t step_id,
    const std::string &original_name, const std::string &identifier,
    const std::string &description, rt_datum_role role)
{
    if (!origin || display_scale <= 0.0 || type < RT_DATUM_POINT ||
	    type > RT_DATUM_TARGET_AREA)
	return false;
    struct rt_datum_internal datum = {};
    datum.magic = RT_DATUM_INTERNAL_MAGIC;
    VSET(datum.pnt, origin[0], origin[1], origin[2]);
    datum.type = type;
    datum.role = role;
    if (direction) {
	VSET(datum.dir, direction[0], direction[1], direction[2]);
	if (MAGNITUDE(datum.dir) <= SMALL_FASTF) return false;
	VUNITIZE(datum.dir);
	VSCALE(datum.dir, datum.dir, display_scale);
    }
    if (x_axis) {
	VSET(datum.xdir, x_axis[0], x_axis[1], x_axis[2]);
	if (MAGNITUDE(datum.xdir) <= SMALL_FASTF) return false;
	VUNITIZE(datum.xdir);
	VSCALE(datum.xdir, datum.xdir, display_scale);
    }
    if (y_axis) {
	VSET(datum.ydir, y_axis[0], y_axis[1], y_axis[2]);
	if (MAGNITUDE(datum.ydir) <= SMALL_FASTF) return false;
	VUNITIZE(datum.ydir);
	VSCALE(datum.ydir, datum.ydir, display_scale);
    }
    datum.w = (type == RT_DATUM_PLANE || type == RT_DATUM_TARGET_AREA) ?
	1.0 : 0.0;
    datum.identifier = identifier.empty() ? NULL :
	const_cast<char *>(identifier.c_str());
    datum.description = description.empty() ? NULL :
	const_cast<char *>(description.c_str());
    datum.next = NULL;
    if (rt_datum_validate(&datum, NULL)) return false;
    if (dry_run) return true;
    if (!outfp) return false;
    if (mk_datums(outfp, name.c_str(), &datum) < 0) return false;
    SetAttribute(name, "step:object_role", "representation_item");
    SetAttribute(name, "step:pmi:native_kind", "datum");
    if (step_id > 0) SetAttribute(name, "step:source_id", std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    if (!identifier.empty()) SetAttribute(name, "step:pmi:datum_identifier",
	identifier);
    return true;
}


bool
BRLCADWrapper::WriteDatumPlane(const std::string &name, const double *origin,
    const double *normal, double display_scale, int64_t step_id,
    const std::string &original_name)
{
    return WriteDatum(name, RT_DATUM_PLANE, origin, normal, NULL, NULL,
	display_scale, step_id, original_name);
}


bool
BRLCADWrapper::WriteAnnotation(const std::string &name, const double *origin,
    const double *u_axis, const double *v_axis,
    const std::vector<std::array<double, 2> > &vertices,
    const std::vector<std::pair<size_t, size_t> > &lines,
    int64_t step_id, const std::string &original_name)
{
    return WriteAnnotation(name, origin, u_axis, v_axis, vertices, lines,
	std::vector<brlcad::step::AnnotationText>(), step_id, original_name);
}


bool
BRLCADWrapper::WriteAnnotation(const std::string &name, const double *origin,
    const double *u_axis, const double *v_axis,
    const std::vector<std::array<double, 2> > &vertices,
    const std::vector<std::pair<size_t, size_t> > &lines,
    const std::vector<brlcad::step::AnnotationText> &texts,
    int64_t step_id, const std::string &original_name)
{
    return WriteAnnotation(name, origin, u_axis, v_axis, vertices, lines,
	texts, std::vector<brlcad::step::AnnotationFill>(), step_id,
	original_name);
}


bool
BRLCADWrapper::WriteAnnotation(const std::string &name, const double *origin,
    const double *u_axis, const double *v_axis,
    const std::vector<std::array<double, 2> > &vertices,
    const std::vector<std::pair<size_t, size_t> > &lines,
    const std::vector<brlcad::step::AnnotationText> &texts,
    const std::vector<brlcad::step::AnnotationFill> &fills,
    int64_t step_id, const std::string &original_name)
{
    return WriteAnnotation(name, origin, u_axis, v_axis, vertices, lines,
	std::vector<brlcad::step::AnnotationLineStyle>(), texts, fills,
	step_id, original_name);
}


bool
BRLCADWrapper::WriteAnnotation(const std::string &name, const double *origin,
    const double *u_axis, const double *v_axis,
    const std::vector<std::array<double, 2> > &vertices,
    const std::vector<std::pair<size_t, size_t> > &lines,
    const std::vector<brlcad::step::AnnotationLineStyle> &line_styles,
    const std::vector<brlcad::step::AnnotationText> &texts,
    const std::vector<brlcad::step::AnnotationFill> &fills,
    int64_t step_id, const std::string &original_name)
{
    if (!origin || !u_axis || !v_axis || vertices.empty() ||
	(lines.empty() && texts.empty() && fills.empty()) ||
	(!line_styles.empty() && line_styles.size() != lines.size()) ||
	vertices.size() > static_cast<size_t>(INT_MAX))
	return false;
    vect_t u = {u_axis[0], u_axis[1], u_axis[2]};
    vect_t v = {v_axis[0], v_axis[1], v_axis[2]};
    vect_t normal;
    VCROSS(normal, u, v);
    if (MAGNITUDE(u) <= SMALL_FASTF || MAGNITUDE(v) <= SMALL_FASTF ||
	MAGNITUDE(normal) <= SMALL_FASTF)
	return false;
    VUNITIZE(u);
    /* Enforce an orthonormal display basis without changing its handedness. */
    VUNITIZE(normal);
    VCROSS(v, normal, u);
    VUNITIZE(v);
    for (const auto &line : lines)
	if (line.first >= vertices.size() || line.second >= vertices.size() ||
		line.first == line.second || line.first > static_cast<size_t>(INT_MAX) ||
		line.second > static_cast<size_t>(INT_MAX))
	    return false;
    for (const auto &text : texts)
	if (text.reference_vertex >= vertices.size() ||
		text.reference_vertex > static_cast<size_t>(INT_MAX) ||
		text.label.empty() || !std::isfinite(text.size) || text.size <= 0.0 ||
		!std::isfinite(text.rotation) ||
		!std::isfinite(text.x_scale) ||
		!std::isfinite(text.xy_scale) ||
		!std::isfinite(text.yx_scale) ||
		!std::isfinite(text.y_scale) ||
		std::fabs(text.x_scale * text.y_scale -
		    text.xy_scale * text.yx_scale) <= SMALL_FASTF ||
	text.role > RT_ANNOT_ROLE_TEXT_DECORATION ||
		(text.style_flags & ~(RT_ANNOT_STYLE_UNDERLINE |
		    RT_ANNOT_STYLE_OVERLINE | RT_ANNOT_STYLE_STRIKETHROUGH |
		    RT_ANNOT_STYLE_BOLD | RT_ANNOT_STYLE_ITALIC)) ||
		text.relative_position < RT_TXT_POS_BL ||
		text.relative_position > RT_TXT_POS_TR)
	    return false;
    size_t fill_outline_count = 0;
    for (const auto &line_style : line_styles)
	if (line_style.role > RT_ANNOT_ROLE_TEXT_DECORATION ||
		line_style.line_pattern > RT_ANNOT_LINE_PHANTOM ||
		(line_style.has_width && (!std::isfinite(line_style.width) ||
		    line_style.width <= 0.0)))
	    return false;
    for (const auto &fill : fills) {
	if (fill.loops.empty() || fill.role > RT_ANNOT_ROLE_TEXT_DECORATION)
	    return false;
	for (const auto &loop : fill.loops) {
	    if (loop.size() < 3 || fill_outline_count > SIZE_MAX - loop.size())
		return false;
	    fill_outline_count += loop.size();
	    for (size_t vertex : loop)
		if (vertex >= vertices.size() || vertex > static_cast<size_t>(INT_MAX))
		    return false;
	}
    }
    if (dry_run) return true;
    if (!outfp) return false;

    struct rt_annot_internal annotation;
    memset(&annotation, 0, sizeof(annotation));
    annotation.magic = RT_ANNOT_INTERNAL_MAGIC;
    VSET(annotation.V, origin[0], origin[1], origin[2]);
    VMOVE(annotation.u_vec, u);
    VMOVE(annotation.v_vec, v);
    annotation.flags = RT_ANNOT_MODEL_SPACE;
    annotation.vert_count = vertices.size();
    annotation.verts = static_cast<point2d_t *>(bu_calloc(vertices.size(),
	sizeof(point2d_t), "STEP annotation vertices"));
    for (size_t i = 0; i < vertices.size(); ++i)
	V2SET(annotation.verts[i], vertices[i][0], vertices[i][1]);

    const size_t segment_count = lines.size() + texts.size() +
	fill_outline_count + fills.size();
    std::vector<struct line_seg> line_segments(lines.size() +
	fill_outline_count);
    std::vector<struct txt_seg> text_segments(texts.size());
    std::vector<struct fill_seg> fill_segments(fills.size());
    annotation.ant.count = segment_count;
    annotation.ant.reverse = static_cast<int *>(bu_calloc(segment_count,
	sizeof(int), "STEP annotation reverse flags"));
    annotation.ant.segments = static_cast<void **>(bu_calloc(segment_count,
	sizeof(void *), "STEP annotation segments"));
    annotation.styles = static_cast<struct rt_annot_seg_style *>(bu_calloc(
	segment_count, sizeof(struct rt_annot_seg_style),
	"STEP annotation styles"));
    for (size_t i = 0; i < lines.size(); ++i) {
	const brlcad::step::AnnotationLineStyle *line_style =
	    line_styles.empty() ? NULL : &line_styles[i];
	line_segments[i].magic = CURVE_LSEG_MAGIC;
	line_segments[i].start = static_cast<int>(lines[i].first);
	line_segments[i].end = static_cast<int>(lines[i].second);
	annotation.ant.segments[i] = &line_segments[i];
	annotation.styles[i].role = line_style ? line_style->role :
	    static_cast<uint32_t>(RT_ANNOT_ROLE_GEOMETRY);
	annotation.styles[i].line_pattern = line_style ?
	    line_style->line_pattern :
	    static_cast<uint32_t>(RT_ANNOT_LINE_CONTINUOUS);
	if (line_style && line_style->has_width) {
	    annotation.styles[i].flags |= RT_ANNOT_STYLE_WIDTH;
	    annotation.styles[i].line_width = line_style->width;
	}
	if (line_style && line_style->has_color) {
	    annotation.styles[i].flags |= RT_ANNOT_STYLE_COLOR;
	    memcpy(annotation.styles[i].color, line_style->color.data(), 4);
	}
	if (line_style && !line_style->symbol.empty())
	    annotation.styles[i].symbol =
		bu_strdup(line_style->symbol.c_str());
    }
    for (size_t i = 0; i < texts.size(); ++i) {
	const size_t segment = lines.size() + i;
	text_segments[i].magic = ANN_TSEG_MAGIC;
	text_segments[i].ref_pt = static_cast<int>(texts[i].reference_vertex);
	text_segments[i].rel_pos = texts[i].relative_position;
	text_segments[i].txt_size = texts[i].size;
	text_segments[i].txt_rot_angle = texts[i].rotation;
	bu_vls_init(&text_segments[i].label);
	bu_vls_strcpy(&text_segments[i].label, texts[i].label.c_str());
	annotation.ant.segments[segment] = &text_segments[i];
	annotation.styles[segment].role = texts[i].role;
	annotation.styles[segment].flags |= texts[i].style_flags;
	annotation.styles[segment].line_pattern = RT_ANNOT_LINE_CONTINUOUS;
	if (!texts[i].font.empty())
	    annotation.styles[segment].font = bu_strdup(texts[i].font.c_str());
	if (!texts[i].symbol.empty())
	    annotation.styles[segment].symbol = bu_strdup(texts[i].symbol.c_str());
	if (texts[i].has_color) {
	    annotation.styles[segment].flags |= RT_ANNOT_STYLE_COLOR;
	    memcpy(annotation.styles[segment].color, texts[i].color.data(), 4);
	}
	if (std::fabs(texts[i].x_scale - 1.0) > SMALL_FASTF ||
		std::fabs(texts[i].xy_scale) > SMALL_FASTF ||
		std::fabs(texts[i].yx_scale) > SMALL_FASTF ||
		std::fabs(texts[i].y_scale - 1.0) > SMALL_FASTF) {
	    annotation.styles[segment].flags |= RT_ANNOT_STYLE_SCALE;
	    annotation.styles[segment].x_scale = texts[i].x_scale;
	    annotation.styles[segment].xy_scale = texts[i].xy_scale;
	    annotation.styles[segment].yx_scale = texts[i].yx_scale;
	    annotation.styles[segment].y_scale = texts[i].y_scale;
	}
    }

    size_t next_segment = lines.size() + texts.size();
    size_t next_line = lines.size();
    for (size_t i = 0; i < fills.size(); ++i) {
	const size_t outline_start = next_segment;
	std::vector<int> loop_ends;
	std::vector<int> points;
	for (const auto &loop : fills[i].loops) {
	    for (size_t j = 0; j < loop.size(); ++j) {
		line_segments[next_line].magic = CURVE_LSEG_MAGIC;
		line_segments[next_line].start = static_cast<int>(loop[j]);
		line_segments[next_line].end = static_cast<int>(
		    loop[(j + 1) % loop.size()]);
		annotation.ant.segments[next_segment] = &line_segments[next_line];
		annotation.styles[next_segment].role = fills[i].role;
		annotation.styles[next_segment].line_pattern =
		    RT_ANNOT_LINE_CONTINUOUS;
		if (fills[i].has_color) {
		    annotation.styles[next_segment].flags |= RT_ANNOT_STYLE_COLOR;
		    memcpy(annotation.styles[next_segment].color,
			fills[i].color.data(), 4);
		}
		if (!fills[i].symbol.empty())
		    annotation.styles[next_segment].symbol =
			bu_strdup(fills[i].symbol.c_str());
		++next_segment;
		++next_line;
		points.push_back(static_cast<int>(loop[j]));
	    }
	    loop_ends.push_back(static_cast<int>(points.size()));
	}
	struct fill_seg &fill_segment = fill_segments[i];
	fill_segment.magic = ANN_FSEG_MAGIC;
	fill_segment.loop_count = static_cast<int>(loop_ends.size());
	fill_segment.point_count = static_cast<int>(points.size());
	fill_segment.loop_ends = loop_ends.data();
	fill_segment.points = points.data();
	fill_segment.legacy_start = static_cast<int>(outline_start);
	fill_segment.legacy_count = static_cast<int>(next_segment - outline_start);
	annotation.ant.segments[next_segment] = &fill_segment;
	annotation.styles[next_segment] = annotation.styles[outline_start];
	annotation.styles[next_segment].font = annotation.styles[outline_start].font ?
	    bu_strdup(annotation.styles[outline_start].font) : NULL;
	annotation.styles[next_segment].symbol = annotation.styles[outline_start].symbol ?
	    bu_strdup(annotation.styles[outline_start].symbol) : NULL;
	annotation.styles[next_segment].flags |= RT_ANNOT_STYLE_FILLED;
	/* mk_annot copies synchronously, so the loop arrays remain valid through
	 * the write below. */
	++next_segment;
	/* The temporary vectors must outlive validation and mk_annot.  Replace
	 * them with owned arrays for the remainder of this function. */
	fill_segment.loop_ends = static_cast<int *>(bu_calloc(loop_ends.size(),
	    sizeof(int), "STEP annotation fill loop ends"));
	fill_segment.points = static_cast<int *>(bu_calloc(points.size(),
	    sizeof(int), "STEP annotation fill points"));
	std::copy(loop_ends.begin(), loop_ends.end(), fill_segment.loop_ends);
	std::copy(points.begin(), points.end(), fill_segment.points);
    }

    const bool written = !rt_annot_validate(&annotation, NULL) &&
	mk_annot(outfp, name.c_str(), &annotation) >= 0;
    for (size_t i = 0; i < lines.size(); ++i)
	if (annotation.styles[i].symbol)
	    bu_free(annotation.styles[i].symbol, "STEP annotation line symbol");
    for (size_t i = 0; i < texts.size(); ++i) {
	const size_t segment = lines.size() + i;
	bu_vls_free(&text_segments[i].label);
	if (annotation.styles[segment].font)
	    bu_free(annotation.styles[segment].font, "STEP annotation font");
	if (annotation.styles[segment].symbol)
	    bu_free(annotation.styles[segment].symbol, "STEP annotation symbol");
    }
    for (size_t i = lines.size() + texts.size(); i < segment_count; ++i) {
	if (annotation.styles[i].font)
	    bu_free(annotation.styles[i].font, "STEP annotation font");
	if (annotation.styles[i].symbol)
	    bu_free(annotation.styles[i].symbol, "STEP annotation symbol");
    }
    for (auto &fill : fill_segments) {
	bu_free(fill.loop_ends, "STEP annotation fill loop ends");
	bu_free(fill.points, "STEP annotation fill points");
    }
    bu_free(annotation.styles, "STEP annotation styles");
    bu_free(annotation.ant.segments, "STEP annotation segments");
    bu_free(annotation.ant.reverse, "STEP annotation reverse flags");
    bu_free(annotation.verts, "STEP annotation vertices");
    if (!written) return false;

    SetAttribute(name, "step:object_role", "presentation_annotation");
    SetAttribute(name, "step:pmi:native_kind", "annotation");
    if (step_id > 0) SetAttribute(name, "step:source_id",
	std::to_string(step_id));
    if (!original_name.empty()) SetAttribute(name, "step:original_name",
	brlcad::step::decode_string(original_name));
    return true;
}


/* This simple routine will replace diacritic characters(code >= 192) from the extended
 * ASCII set with a specific mapping from the standard ASCII set. This code was copied
 * and modified from a solution provided on stackoverflow.com at:
 *     (http://stackoverflow.com/questions/14094621/)
 */
std::string
BRLCADWrapper::ReplaceAccented( std::string &str ) {
    std::string retStr = "";
    const char *p = str.c_str();
    while ( (*p)!=0 ) {
        const char*
        //   "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ"
        tr = "AAAAAAECEEEEIIIIDNOOOOOx0UUUUYPsaaaaaaeceeeeiiiiOnooooo/0uuuuypy";
        unsigned char ch = (*p);
        if ( ch >=192 ) {
            retStr += tr[ ch-192 ];
        } else {
            retStr += *p;
        }
        ++p;
    }
    return retStr;
}


/*
 * Simplifying names for better behavior under our Tcl based tools. This routine
 * replaces spaces and non-alphanumeric characters with underscores. It also replaces
 * ASCII extended characters representing diacritics (code >= 192)  with specific
 * mapped ASCII characters below ASCII code 128.
 */
std::string
BRLCADWrapper::CleanBRLCADName(std::string &inname)
{
    return brlcad::step::sanitize_name(inname);
}


std::string
BRLCADWrapper::StableBRLCADName(const std::string &inname, int64_t step_id)
{
    std::string base = brlcad::step::sanitize_name(inname);
    std::map<std::string, int64_t>::const_iterator found = allocated_names.find(base);
    if (found == allocated_names.end() || found->second == step_id) {
	allocated_names[base] = step_id;
	return base;
    }

    const std::string stem = base + "_step" + std::to_string(step_id);
    std::string result = stem;
    while (allocated_names.find(result) != allocated_names.end() && allocated_names[result] != step_id)
	result = stem + "_" + std::to_string(++anonymous_name_counter);
    allocated_names[result] = step_id;
    return result;
}


std::string
BRLCADWrapper::GetBRLCADName(std::string &name)
{
    struct bu_vls obj_name = BU_VLS_INIT_ZERO;
    int len = 0;
    char *cp,*tp;
    int start = static_cast<int>(++anonymous_name_counter);

    for (cp = (char *)name.c_str(), len = 0; *cp != '\0'; ++cp, ++len) {
	if (*cp == '@') {
	    if (*(cp + 1) == '@')
		++cp;
	    else
		break;
	}
	if (*cp == '\'') {
	    // remove single quotes
	    continue;
	}
	if (*cp == ' ') {
	    // simply replace spaces with underscores
	    bu_vls_putc(&obj_name, '_');
	} else {
	    bu_vls_putc(&obj_name, *cp);
	}
    }
    bu_vls_putc(&obj_name, '\0');

    tp = (char *)((*cp == '\0') ? "" : cp + 1);

    /* TODO - We don't have db_lookup in a dry run */
    if (dry_run) {
	std::string result = bu_vls_cstr(&obj_name);
	bu_vls_free(&obj_name);
	return result;
    }

    do {
	bu_vls_trunc(&obj_name, len);
	bu_vls_printf(&obj_name, "%d", start++);
	bu_vls_strcat(&obj_name, tp);
    }
    while (db_lookup(outfp->dbip, bu_vls_addr(&obj_name), LOOKUP_QUIET) != RT_DIR_NULL);

    std::string rstr(bu_vls_cstr(&obj_name));
    bu_vls_free(&obj_name);
    return rstr;
}

bool
BRLCADWrapper::EnsureCombination(const std::string &combname)
{
    if (combname.empty())
	return false;
    if (heads.find(combname) != heads.end())
	return true;

    struct bu_list *head = NULL;
    BU_ALLOC(head, struct bu_list);
    BU_LIST_INIT(head);
    heads[combname] = head;
    return true;
}


bool
BRLCADWrapper::AddMember(const std::string &combname, const std::string &member, mat_t mat,
	int operation)
{
    if (combname.empty() || member.empty())
	return false;
    MAP_OF_BU_LIST_HEADS::iterator i = heads.find(combname);
    if (i != heads.end()) {
	struct bu_list *head = (*i).second;
	if (mk_addmember(member.c_str(), head, mat, operation) == WMEMBER_NULL)
	    return false;
    } else {
	struct bu_list *head = NULL;

	BU_ALLOC(head, struct bu_list);

	BU_LIST_INIT(head);
	if (mk_addmember(member.c_str(), head, mat, operation) == WMEMBER_NULL) {
	    BU_FREE(head, struct bu_list);
	    return false;
	}
	heads[combname] = head;
    }

    return true;
}

bool
BRLCADWrapper::SetCombinationProperties(const std::string &combname, bool is_region,
	int64_t step_id, const std::string &original_name,
	const brlcad::step::Style *style)
{
    if (combname.empty())
	return false;
    CombinationProperties &properties = combination_properties[combname];
    properties.is_region = is_region;
    properties.step_id = step_id;
    properties.original_name = original_name;
    properties.has_style = style != NULL;
    if (style)
	properties.style = *style;
    return true;
}


bool
BRLCADWrapper::SetCombinationAttribute(const std::string &combname,
	const std::string &key, const std::string &value)
{
    if (combname.empty() || key.empty())
	return false;
    combination_properties[combname].attributes[key] = value;
    return true;
}

bool
BRLCADWrapper::WriteCombs()
{
    MAP_OF_BU_LIST_HEADS::iterator i = heads.begin();
    bool success = true;

    if (dry_run) {
	while (i != heads.end()) {
	    struct bu_list *head = (i++)->second;
	    mk_freemembers(head);
	    BU_FREE(head, struct bu_list);
	}
	heads.clear();
	combination_properties.clear();
	return true;
    }

    while (i != heads.end()) {
	std::string combname = (*i).first;
	struct bu_list *head = (*i++).second;

	std::map<std::string, CombinationProperties>::const_iterator property =
	    combination_properties.find(combname);
	const CombinationProperties *properties = property == combination_properties.end() ?
	    NULL : &property->second;
	unsigned char rgb[3] = {200, 180, 180};
	std::string shader_args;
	const char *shader = NULL;
	unsigned char *colour = NULL;
	if (properties && properties->is_region) {
	    shader = "plastic";
	    colour = rgb;
	    if (properties->has_style && properties->style.has_rgb) {
		for (size_t component = 0; component < 3; ++component) {
		    double value = properties->style.rgb[component];
		    if (value < 0.0) value = 0.0;
		    if (value > 1.0) value = 1.0;
		    rgb[component] = static_cast<unsigned char>(value * 255.0 + 0.5);
		}
	    } else {
		getRandomColor(rgb);
	    }
	    if (properties->has_style && properties->style.has_transparency) {
		std::ostringstream args;
		args << "tr " << properties->style.transparency;
		shader_args = args.str();
	    }
	}

	/* Product, assembly, and intermediate Boolean combinations remain
	 * non-regions.  Exact CSG roots explicitly opt into region semantics. */
	if (mk_comb(outfp, combname.c_str(), head,
	    properties && properties->is_region ? 1 : 0, shader,
	    shader_args.empty() ? NULL : shader_args.c_str(), colour,
	    0, 0, 0, 0, 0, 0, 0) < 0) {
	    success = false;
	} else if (properties) {
	    if (properties->step_id > 0)
		SetAttribute(combname, "step:source_id", std::to_string(properties->step_id));
	    if (!properties->original_name.empty())
		SetAttribute(combname, "step:original_name",
		    brlcad::step::decode_string(properties->original_name));
	    if (properties->has_style) {
		const brlcad::step::Style &style = properties->style;
		if (!style.name.empty()) SetAttribute(combname, "step:style_name", style.name);
		if (style.has_rgb) {
		    std::ostringstream value;
		    value << style.rgb[0] << ' ' << style.rgb[1] << ' ' << style.rgb[2];
		    SetAttribute(combname, "step:color_rgb", value.str());
		}
		if (style.has_transparency)
		    SetAttribute(combname, "step:transparency",
			std::to_string(style.transparency));
		if (!style.layers.empty()) {
		    std::ostringstream value;
		    for (size_t layer = 0; layer < style.layers.size(); ++layer) {
			if (layer) value << ';';
			value << style.layers[layer];
		    }
		    SetAttribute(combname, "step:layers", value.str());
		}
		if (!style.source_entity_ids.empty()) {
		    std::ostringstream value;
		    for (size_t source = 0; source < style.source_entity_ids.size(); ++source) {
			if (source) value << ' ';
			value << style.source_entity_ids[source];
		    }
		    SetAttribute(combname, "step:style_source_ids", value.str());
		}
	    }
	    for (std::map<std::string, std::string>::const_iterator attribute =
		 properties->attributes.begin(); attribute != properties->attributes.end();
		 ++attribute)
		SetAttribute(combname, attribute->first, attribute->second);
	}

	BU_FREE(head, struct bu_list);

    }
    heads.clear();
    combination_properties.clear();
    return success;
}


void
BRLCADWrapper::getRandomColor(unsigned char *rgb)
{
    /* golden ratio */
    static fastf_t hsv[3] = { 0.0, 0.5, 0.95 };
    static double golden_ratio_conjugate = 0.618033988749895;
    static fastf_t h = drand48();

    h = fmod(h+golden_ratio_conjugate,1.0);
    *hsv = h * 360.0;
    bu_hsv_to_rgb(hsv,rgb);
}


static void
getStableEntityColor(unsigned char *rgb, int64_t step_id)
{
    if (step_id <= 0) {
	BRLCADWrapper::getRandomColor(rgb);
	return;
    }
    /* Preserve the historical pleasant saturation/value while deriving hue
     * from source identity.  Completion-order spooling must not make an
     * otherwise deterministic database assign different fallback colors. */
    uint64_t hash = 1469598103934665603ULL;
    uint64_t value = static_cast<uint64_t>(step_id);
    for (size_t byte = 0; byte < sizeof(value); ++byte) {
	hash ^= value & 0xffU;
	hash *= 1099511628211ULL;
	value >>= 8;
    }
    fastf_t hsv[3] = {
	static_cast<fastf_t>(hash % 360U), 0.5, 0.95
    };
    bu_hsv_to_rgb(hsv, rgb);
}


bool
BRLCADWrapper::WriteBrep(std::string name, ON_Brep *brep, mat_t &mat, bool is_region,
	int64_t step_id, const std::string &original_name, const brlcad::step::Style *style)
{
    std::string sol = name + ".s";
    std::string reg = name;
    if (dry_run)
	return true;
    if (!outfp || !brep)
	return false;

    if (mk_brep(outfp, sol.c_str(), (void *)brep) < 0)
	return false;
    unsigned char rgb[] = {200, 180, 180};

    if (style && style->has_rgb) {
	for (size_t i = 0; i < 3; ++i) {
	    double component = style->rgb[i];
	    if (component < 0.0) component = 0.0;
	    if (component > 1.0) component = 1.0;
	    rgb[i] = static_cast<unsigned char>(component * 255.0 + 0.5);
	}
    } else {
	getStableEntityColor(rgb, step_id);
    }

    std::string shader_args;
    if (style && style->has_transparency) {
	std::ostringstream args;
	args << "tr " << style->transparency;
	shader_args = args.str();
    }

    struct bu_list head;
    BU_LIST_INIT(&head);
    if (mk_addmember(sol.c_str(), &head, mat, WMOP_UNION) == WMEMBER_NULL)
	return false;

    if (mk_comb(outfp, reg.c_str(), &head, is_region ? 1 : 0, "plastic",
	shader_args.c_str(), rgb, 0, 0, 0, 0, 0, 0, 0) < 0)
	return false;

    SetAttribute(sol, "step:object_role", "representation_item");
    SetAttribute(reg, "step:object_role", "representation_item");

    if (step_id > 0) {
	const std::string id = std::to_string(step_id);
	SetAttribute(sol, "step:source_id", id);
	SetAttribute(reg, "step:source_id", id);
    }
    if (!original_name.empty()) {
	const std::string decoded = brlcad::step::decode_string(original_name);
	SetAttribute(sol, "step:original_name", decoded);
	SetAttribute(reg, "step:original_name", decoded);
    }
    if (style) {
	if (!style->name.empty()) {
	    SetAttribute(sol, "step:style_name", style->name);
	    SetAttribute(reg, "step:style_name", style->name);
	}
	if (style->has_rgb) {
	    std::ostringstream value;
	    value << style->rgb[0] << ' ' << style->rgb[1] << ' ' << style->rgb[2];
	    SetAttribute(sol, "step:color_rgb", value.str());
	    SetAttribute(reg, "step:color_rgb", value.str());
	}
	if (style->has_transparency) {
	    const std::string value = std::to_string(style->transparency);
	    SetAttribute(sol, "step:transparency", value);
	    SetAttribute(reg, "step:transparency", value);
	}
	if (!style->layers.empty()) {
	    std::ostringstream value;
	    for (size_t i = 0; i < style->layers.size(); ++i) {
		if (i) value << ';';
		value << style->layers[i];
	    }
	    SetAttribute(sol, "step:layers", value.str());
	    SetAttribute(reg, "step:layers", value.str());
	}
	if (!style->source_entity_ids.empty()) {
	    std::ostringstream value;
	    for (size_t i = 0; i < style->source_entity_ids.size(); ++i) {
		if (i) value << ' ';
		value << style->source_entity_ids[i];
	    }
	    SetAttribute(sol, "step:style_source_ids", value.str());
	    SetAttribute(reg, "step:style_source_ids", value.str());
	}
    }
    return true;
}


bool
BRLCADWrapper::WriteBot(std::string name, size_t num_vertices, size_t num_faces,
	fastf_t *vertices, int *faces, mat_t &mat, int64_t step_id,
	const std::string &original_name, const brlcad::step::Style *style,
	int mode, int orientation, const std::string &representation)
{
    if (dry_run)
	return true;
    if (!outfp || !vertices || !faces || num_vertices < 3 || num_faces < 1 ||
	(mode != RT_BOT_SOLID && mode != RT_BOT_SURFACE) ||
	(orientation != RT_BOT_CCW && orientation != RT_BOT_CW &&
	 orientation != RT_BOT_UNORIENTED))
	return false;

    const std::string sol = name + ".s";
    if (mk_bot(outfp, sol.c_str(), mode, orientation, 0,
	num_vertices, num_faces, vertices, faces, NULL, NULL) < 0)
	return false;

    unsigned char rgb[] = {200, 180, 180};
    if (style && style->has_rgb) {
	for (size_t i = 0; i < 3; ++i) {
	    double component = style->rgb[i];
	    if (component < 0.0) component = 0.0;
	    if (component > 1.0) component = 1.0;
	    rgb[i] = static_cast<unsigned char>(component * 255.0 + 0.5);
	}
    } else {
	getStableEntityColor(rgb, step_id);
    }

    std::string shader_args;
    if (style && style->has_transparency) {
	std::ostringstream args;
	args << "tr " << style->transparency;
	shader_args = args.str();
    }

    struct bu_list head;
    BU_LIST_INIT(&head);
    if (mk_addmember(sol.c_str(), &head, mat, WMOP_UNION) == WMEMBER_NULL)
	return false;
    if (mk_comb(outfp, name.c_str(), &head, mode == RT_BOT_SOLID ? 1 : 0,
	"plastic", shader_args.c_str(),
	rgb, 0, 0, 0, 0, 0, 0, 0) < 0)
	return false;

    SetAttribute(sol, "step:object_role", "representation_item");
    SetAttribute(name, "step:object_role", "representation_item");

    if (step_id > 0) {
	const std::string id = std::to_string(step_id);
	SetAttribute(sol, "step:source_id", id);
	SetAttribute(name, "step:source_id", id);
    }
    if (!original_name.empty()) {
	const std::string decoded = brlcad::step::decode_string(original_name);
	SetAttribute(sol, "step:original_name", decoded);
	SetAttribute(name, "step:original_name", decoded);
    }
    if (style) {
	if (!style->name.empty()) {
	    SetAttribute(sol, "step:style_name", style->name);
	    SetAttribute(name, "step:style_name", style->name);
	}
	if (style->has_rgb) {
	    std::ostringstream value;
	    value << style->rgb[0] << ' ' << style->rgb[1] << ' ' << style->rgb[2];
	    SetAttribute(sol, "step:color_rgb", value.str());
	    SetAttribute(name, "step:color_rgb", value.str());
	}
	if (style->has_transparency) {
	    const std::string value = std::to_string(style->transparency);
	    SetAttribute(sol, "step:transparency", value);
	    SetAttribute(name, "step:transparency", value);
	}
	if (!style->layers.empty()) {
	    std::ostringstream value;
	    for (size_t i = 0; i < style->layers.size(); ++i) {
		if (i) value << ';';
		value << style->layers[i];
	    }
	    SetAttribute(sol, "step:layers", value.str());
	    SetAttribute(name, "step:layers", value.str());
	}
	if (!style->source_entity_ids.empty()) {
	    std::ostringstream value;
	    for (size_t i = 0; i < style->source_entity_ids.size(); ++i) {
		if (i) value << ' ';
		value << style->source_entity_ids[i];
	    }
	    SetAttribute(sol, "step:style_source_ids", value.str());
	    SetAttribute(name, "step:style_source_ids", value.str());
	}
    }
    if (!representation.empty()) {
	SetAttribute(sol, "step:representation", representation);
	SetAttribute(name, "step:representation", representation);
    }
    return true;
}


bool
BRLCADWrapper::SetAttribute(const std::string &object, const std::string &key, const std::string &value)
{
    if (dry_run)
	return true;
    if (!outfp)
	return false;
    if (db_lookup(outfp->dbip, object.c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
	return false;
    return db5_update_attribute(object.c_str(), key.c_str(), value.c_str(), outfp->dbip) == 0;
}


bool
BRLCADWrapper::SetAttributes(const std::string &object,
    const std::map<std::string, std::string> &attributes)
{
    if (dry_run || attributes.empty()) return true;
    if (!outfp) return false;
    struct directory *dp = db_lookup(outfp->dbip, object.c_str(), LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) return false;
    std::map<std::string, std::string> merged;
    struct bu_attribute_value_set existing = BU_AVS_INIT_ZERO;
    if (db5_get_attributes(outfp->dbip, &existing, dp) < 0) return false;
    struct bu_attribute_value_pair *pair = NULL;
    for (BU_AVS_FOR(pair, &existing)) {
	if (pair->name && pair->value) merged[pair->name] = pair->value;
    }
    bu_avs_free(&existing);
    for (const auto &attribute : attributes)
	merged[attribute.first] = attribute.second;

    struct bu_attribute_value_set values;
    bu_avs_init(&values, merged.size(), "STEP bulk attributes");
    for (const auto &attribute : merged)
	bu_avs_add_nonunique(&values, attribute.first.c_str(),
	    attribute.second.c_str());
    /* The input map and existing database AVS are unique.  add_nonunique is
     * therefore safe here and avoids bu_avs_add's quadratic duplicate scan.
     * db5_replace_attributes consumes values as documented. */
    return db5_replace_attributes(dp, &values, outfp->dbip) == 0;
}


bool
BRLCADWrapper::WriteSTEPMetadata(
    const std::map<std::string, std::string> &attributes)
{
    if (dry_run || attributes.empty()) return true;
    if (!outfp) return false;

    std::vector<unsigned char> payload;
    std::string error;
    if (!brlcad::step::EncodeSTEPMetadata(attributes, payload, error)) {
	bu_log("Unable to encode retained STEP metadata: %s\n", error.c_str());
	return false;
    }
    if (payload.size() > static_cast<size_t>(LONG_MAX)) {
	bu_log("Unable to retain STEP metadata: binary object is too large\n");
	return false;
    }
    if (mk_binunif(outfp, brlcad::step::STEP_METADATA_OBJECT,
	    payload.data(), WDB_BINUNIF_UINT8,
	    static_cast<long>(payload.size())) != 0)
	return false;

    std::map<std::string, std::string> locator;
    locator[brlcad::step::STEP_METADATA_OBJECT_ATTRIBUTE] =
	brlcad::step::STEP_METADATA_OBJECT;
    locator[brlcad::step::STEP_METADATA_FORMAT_ATTRIBUTE] =
	brlcad::step::STEP_METADATA_FORMAT;
    locator[brlcad::step::STEP_METADATA_RECORDS_ATTRIBUTE] =
	std::to_string(attributes.size());
    if (!SetAttributes(brlcad::step::STEP_METADATA_OBJECT, locator))
	return false;

    /* Preserve the historical, directly editable representation when it is
     * modest.  Large graphs use only a locator on _GLOBAL, avoiding database
     * attribute algorithms and tools whose costs grow poorly at that scale. */
    const size_t global_attribute_limit = 4096;
    const size_t global_payload_limit = 1024 * 1024;
    std::map<std::string, std::string> global = locator;
    if (attributes.size() <= global_attribute_limit &&
	    payload.size() <= global_payload_limit)
	global.insert(attributes.begin(), attributes.end());
    return SetAttributes(DB5_GLOBAL_OBJECT_NAME, global);
}


bool
BRLCADWrapper::CopyObjectFrom(BRLCADWrapper &source, const std::string &object)
{
    if (dry_run)
	return true;
    if (!outfp)
	return false;
    struct db_i *source_dbip = source.dbip;
    if (!source_dbip && source.outfp)
	source_dbip = source.outfp->dbip;
    if (!source_dbip)
	return false;
    struct directory *source_dp = db_lookup(source_dbip, object.c_str(), LOOKUP_QUIET);
    if (source_dp == RT_DIR_NULL)
	return false;

    struct bu_external external;
    if (db_get_external(&external, source_dp, source_dbip) < 0)
	return false;
    const int result = wdb_export_external(outfp, &external, object.c_str(),
	source_dp->d_flags, source_dp->d_minor_type);
    bu_free_external(&external);
    return result >= 0;
}

struct db_i *
BRLCADWrapper::GetDBIP()
{
    return dbip;
}


bool
BRLCADWrapper::Close()
{
    if (dry_run)
	return true;

    if (outfp) {
	db_close(outfp->dbip);
	outfp = NULL;
    }
    if (dbip) {
	db_close(dbip);
	dbip = NULL;
    }

    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
