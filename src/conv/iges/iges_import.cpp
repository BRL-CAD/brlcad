/*                  I G E S _ I M P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "iges_import.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "nmg/nurb.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/annot.h"
#include "rt/primitives/datum.h"
#include "wdb.h"

namespace brlcad {
namespace iges {
namespace {

constexpr size_t GLOBAL_MODEL_SCALE = 12;
constexpr size_t GLOBAL_UNITS_FLAG = 13;
constexpr size_t GLOBAL_LINE_GRADATIONS = 15;
constexpr size_t GLOBAL_MAX_LINE_WIDTH = 16;
constexpr double DEFAULT_MODEL_SCALE = 1.0;
constexpr double DEFAULT_UNIT_TO_MM = 1.0;
constexpr double MIN_VECTOR_LENGTH = 1.0e-12;
constexpr double COPLANAR_RELATIVE_TOLERANCE = 1.0e-9;
constexpr double ARC_RADIUS_REPAIR_LIMIT = 1.0e-6;
constexpr int MAX_ENTITY_LIST_COUNT = 10000000;

using Point2 = std::array<double, 2>;
using Point3 = std::array<double, 3>;

struct Matrix {
    double m[4][4] = {
	{1.0, 0.0, 0.0, 0.0},
	{0.0, 1.0, 0.0, 0.0},
	{0.0, 0.0, 1.0, 0.0},
	{0.0, 0.0, 0.0, 1.0}
    };
};

struct Plane {
    Point3 origin = {0.0, 0.0, 0.0};
    Point3 u = {1.0, 0.0, 0.0};
    Point3 v = {0.0, 1.0, 0.0};
};

struct Style {
    uint32_t role = RT_ANNOT_ROLE_GEOMETRY;
    uint32_t flags = 0;
    uint32_t line_pattern = RT_ANNOT_LINE_CONTINUOUS;
    double line_width = 0.0;
    std::array<unsigned char, 4> color = {0, 0, 0, 255};
    std::string font;
    std::string symbol;
    double x_scale = 1.0;
    double xy_scale = 0.0;
    double yx_scale = 0.0;
    double y_scale = 1.0;
};

enum class SegmentKind {
    Line,
    Arc,
    Nurbs,
    Text
};

struct Segment {
    SegmentKind kind = SegmentKind::Line;
    Style style;
    int start = 0;
    int end = 0;
    int center = 0;
    double radius = 0.0;
    bool center_is_left = false;
    bool clockwise = false;
    int order = 0;
    std::vector<double> knots;
    std::vector<int> controls;
    std::vector<double> weights;
    int reference = 0;
    int relative_position = RT_TXT_POS_BL;
    double text_size = 1.0;
    double text_rotation = 0.0;
    std::string text;
};

struct AnnotationData {
    Plane plane;
    std::vector<Point2> vertices;
    std::vector<Segment> segments;
};

double unit_scale(const GlobalSection &global);

class Translator {
public:
    Translator(const Document &document, struct rt_wdb *wdbp,
	const ImportOptions &options) : document_(document), wdbp_(wdbp),
	options_(options)
    {
	result_.statistics.entities_read = document.entities().size();
	unit_to_mm_ = unit_scale(document.global());
    }

    ImportResult run();

private:
    bool translate(const DirectoryEntry &entry, const std::string &name);
    bool translate_point(const DirectoryEntry &entry, const std::string &name);
    bool translate_line(const DirectoryEntry &entry, const std::string &name);
    bool translate_arc(const DirectoryEntry &entry, const std::string &name);
    bool translate_copious(const DirectoryEntry &entry, const std::string &name);
    bool translate_nurbs(const DirectoryEntry &entry, const std::string &name);
    bool translate_note(const DirectoryEntry &entry, const std::string &name);
    bool translate_leader(const DirectoryEntry &entry, const std::string &name);
    bool write_annotation(const DirectoryEntry &entry, const std::string &name,
	const AnnotationData &data);
    void write_entity_attributes(const DirectoryEntry &entry,
	const std::string &name);
    bool write_groups();
    bool write_subfigures();
    bool write_root(const std::vector<std::string> &members);
    Style style(const DirectoryEntry &entry, uint32_t role,
	const std::string &symbol = std::string()) const;
    Matrix transform(const DirectoryEntry &entry);
    Matrix transform(EntityId id, std::set<EntityId> &active);
    Point3 point(const DirectoryEntry &entry, const Point3 &value);
    Point3 vector(const DirectoryEntry &entry, const Point3 &value);
    void diagnose(Severity severity, const char *code, const std::string &message,
	const DirectoryEntry *entry = nullptr);
    std::string unique_name(const DirectoryEntry &entry, const char *suffix);
    std::string unique_name(const DirectoryEntry &entry,
	const std::string &source, const char *suffix);

    const Document &document_;
    struct rt_wdb *wdbp_ = nullptr;
    ImportOptions options_;
    ImportResult result_;
    double unit_to_mm_ = DEFAULT_UNIT_TO_MM;
    std::map<EntityId, Matrix> transforms_;
    std::map<EntityId, std::string> objects_;
    std::vector<std::string> groups_;
    std::set<EntityId> grouped_;
};

double
length(const Point3 &value)
{
    return std::sqrt(value[0] * value[0] + value[1] * value[1] +
	value[2] * value[2]);
}

Point3
subtract(const Point3 &left, const Point3 &right)
{
    return {left[0] - right[0], left[1] - right[1], left[2] - right[2]};
}

Point3
scale(const Point3 &value, double factor)
{
    return {value[0] * factor, value[1] * factor, value[2] * factor};
}

double
dot(const Point3 &left, const Point3 &right)
{
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

Point3
cross(const Point3 &left, const Point3 &right)
{
    return {
	left[1] * right[2] - left[2] * right[1],
	left[2] * right[0] - left[0] * right[2],
	left[0] * right[1] - left[1] * right[0]
    };
}

bool
normalize(Point3 &value)
{
    const double magnitude = length(value);
    if (!std::isfinite(magnitude) || magnitude <= MIN_VECTOR_LENGTH)
	return false;
    value = scale(value, 1.0 / magnitude);
    return true;
}

Point3
apply_point(const Matrix &matrix, const Point3 &point)
{
    Point3 result;
    for (size_t row = 0; row < 3; ++row)
	result[row] = matrix.m[row][0] * point[0] +
	    matrix.m[row][1] * point[1] + matrix.m[row][2] * point[2] +
	    matrix.m[row][3];
    return result;
}

Point3
apply_vector(const Matrix &matrix, const Point3 &vector)
{
    Point3 result;
    for (size_t row = 0; row < 3; ++row)
	result[row] = matrix.m[row][0] * vector[0] +
	    matrix.m[row][1] * vector[1] + matrix.m[row][2] * vector[2];
    return result;
}

Matrix
multiply(const Matrix &left, const Matrix &right)
{
    Matrix result;
    for (size_t row = 0; row < 4; ++row)
	for (size_t column = 0; column < 4; ++column) {
	    result.m[row][column] = 0.0;
	    for (size_t inner = 0; inner < 4; ++inner)
		result.m[row][column] +=
		    left.m[row][inner] * right.m[inner][column];
	}
    return result;
}

bool
parameter_real(const ParameterList *parameters, size_t index, double &value)
{
    return parameters && index < parameters->values.size() &&
	parameters->values[index].real(value);
}

bool
parameter_integer(const ParameterList *parameters, size_t index, int &value)
{
    int64_t parsed = 0;
    if (!parameters || index >= parameters->values.size() ||
	    !parameters->values[index].integer(parsed) ||
	    parsed < std::numeric_limits<int>::min() ||
	    parsed > std::numeric_limits<int>::max())
	return false;
    value = static_cast<int>(parsed);
    return true;
}

bool
parameter_entity(const ParameterList *parameters, size_t index, EntityId &value)
{
    return parameters && index < parameters->values.size() &&
	parameters->values[index].entity(value) && !value.empty();
}


bool
parameter_string(const ParameterList *parameters, size_t index,
    std::string &value)
{
    return parameters && index < parameters->values.size() &&
	parameters->values[index].string(value);
}

double
global_real(const GlobalSection &global, size_t index, double fallback)
{
    if (index >= global.parameters.size())
	return fallback;
    char *end = nullptr;
    std::string value = global.parameters[index];
    std::replace(value.begin(), value.end(), 'D', 'E');
    std::replace(value.begin(), value.end(), 'd', 'e');
    const double parsed = std::strtod(value.c_str(), &end);
    return end == value.c_str() + value.size() && std::isfinite(parsed) ?
	parsed : fallback;
}

int
global_integer(const GlobalSection &global, size_t index, int fallback)
{
    const double parsed = global_real(global, index, fallback);
    return parsed >= std::numeric_limits<int>::min() &&
	parsed <= std::numeric_limits<int>::max() ?
	static_cast<int>(parsed) : fallback;
}

double
unit_scale(const GlobalSection &global)
{
    static const double to_mm[] = {
	1.0, 25.4, 1.0, 1.0, 304.8, 1609344.0, 1000.0,
	1000000.0, 0.0254, 0.001, 10.0, 0.0000254
    };
    const double model_scale = global_real(global, GLOBAL_MODEL_SCALE,
	DEFAULT_MODEL_SCALE);
    const int units = global_integer(global, GLOBAL_UNITS_FLAG, 2);
    const double conversion = units > 0 &&
	static_cast<size_t>(units) < sizeof(to_mm) / sizeof(to_mm[0]) ?
	to_mm[units] : DEFAULT_UNIT_TO_MM;
    return conversion / (model_scale > 0.0 ? model_scale : DEFAULT_MODEL_SCALE);
}

bool
plane_from_points(const std::vector<Point3> &points, Plane &plane,
    std::vector<Point2> &projected)
{
    if (points.size() < 2)
	return false;
    plane.origin = points.front();
    plane.u = subtract(points[1], plane.origin);
    if (!normalize(plane.u))
	return false;

    Point3 normal = {0.0, 0.0, 0.0};
    double best = 0.0;
    for (size_t i = 2; i < points.size(); ++i) {
	Point3 candidate = cross(plane.u, subtract(points[i], plane.origin));
	const double candidate_length = length(candidate);
	if (candidate_length > best) {
	    normal = candidate;
	    best = candidate_length;
	}
    }
    if (!normalize(normal)) {
	const Point3 axis = std::fabs(plane.u[2]) < 0.9 ?
	    Point3{0.0, 0.0, 1.0} : Point3{0.0, 1.0, 0.0};
	normal = cross(plane.u, axis);
	if (!normalize(normal))
	    return false;
    }
    plane.v = cross(normal, plane.u);
    if (!normalize(plane.v))
	return false;

    double extent = 0.0;
    projected.clear();
    projected.reserve(points.size());
    for (const Point3 &point : points) {
	const Point3 delta = subtract(point, plane.origin);
	const double u = dot(delta, plane.u);
	const double v = dot(delta, plane.v);
	const double off_plane = std::fabs(dot(delta, normal));
	extent = std::max(extent, length(delta));
	if (off_plane > COPLANAR_RELATIVE_TOLERANCE * std::max(1.0, extent))
	    return false;
	projected.push_back({u, v});
    }
    return true;
}

bool
xy_plane_from_points(const std::vector<Point3> &points, Plane &plane,
    std::vector<Point2> &projected)
{
    if (points.size() < 2)
	return false;
    plane = Plane();
    projected.clear();
    projected.reserve(points.size());
    for (const Point3 &point : points)
	projected.push_back({point[0], point[1]});
    for (size_t i = 1; i < projected.size(); ++i)
	if (std::hypot(projected[i][0] - projected.front()[0],
		projected[i][1] - projected.front()[1]) > MIN_VECTOR_LENGTH)
	    return true;
    return false;
}

std::string
semantic_name(int type)
{
    switch (type) {
	case 202: return "angular_dimension";
	case 206: return "diameter_dimension";
	case 208: return "flag_note";
	case 210: return "general_label";
	case 216: return "linear_dimension";
	case 218: return "ordinate_dimension";
	case 220: return "point_dimension";
	case 222: return "radius_dimension";
	case 228: return "general_symbol";
	case 230: return "sectioned_area";
	default: return std::string();
    }
}

bool
is_drawable_type(int type)
{
    return type == 100 || type == 106 || type == 110 || type == 116 ||
	type == 126 || type == 212 || type == 214;
}

bool
is_dimension_type(int type)
{
    return !semantic_name(type).empty();
}

std::string
sanitize_name(const std::string &source)
{
    struct bu_vls sanitized = BU_VLS_INIT_ZERO;

    db_sanitize_name(&sanitized, source.c_str());
    const std::string result = bu_vls_cstr(&sanitized);
    bu_vls_free(&sanitized);
    return result;
}

std::string
json_escape(const std::string &value)
{
    std::ostringstream output;
    for (unsigned char character : value) {
	switch (character) {
	    case '\\': output << "\\\\"; break;
	    case '"': output << "\\\""; break;
	    case '\b': output << "\\b"; break;
	    case '\f': output << "\\f"; break;
	    case '\n': output << "\\n"; break;
	    case '\r': output << "\\r"; break;
	    case '\t': output << "\\t"; break;
	    default:
		if (character < 0x20)
		    output << "\\u" << std::hex << std::setw(4) <<
			std::setfill('0') << static_cast<unsigned int>(character) <<
			std::dec << std::setfill(' ');
		else
		    output << static_cast<char>(character);
	}
    }
    return output.str();
}

} /* namespace */

void
Translator::diagnose(Severity severity, const char *code,
    const std::string &message, const DirectoryEntry *entry)
{
    ImportDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.code = code;
    diagnostic.message = message;
    if (entry) {
	diagnostic.entity_id = entry->id.value();
	diagnostic.entity_type = entry->type;
    }
    result_.diagnostics.push_back(std::move(diagnostic));
}

Matrix
Translator::transform(EntityId id, std::set<EntityId> &active)
{
    if (id.empty())
	return Matrix();
    const auto cached = transforms_.find(id);
    if (cached != transforms_.end())
	return cached->second;
    if (!active.insert(id).second) {
	diagnose(Severity::Error, "transform_cycle",
	    "cyclic IGES transformation reference");
	return Matrix();
    }

    Matrix matrix;
    const DirectoryEntry *entry = document_.entity(id);
    const ParameterList *parameters = entry ? document_.parameters(id) : nullptr;
    bool valid = entry && parameters;
    if (valid && entry->type == 124) {
	for (size_t row = 0; row < 3; ++row)
	    for (size_t column = 0; column < 4; ++column) {
		double value = 0.0;
		if (!parameter_real(parameters, 1 + row * 4 + column, value))
		    valid = false;
		else
		    matrix.m[row][column] = value;
	    }
    } else if (valid && entry->type == 700) {
	for (size_t row = 0; row < 4; ++row)
	    for (size_t column = 0; column < 4; ++column) {
		double value = 0.0;
		if (!parameter_real(parameters, 1 + row * 4 + column, value))
		    valid = false;
		else
		    matrix.m[row][column] = value;
	    }
    } else {
	valid = false;
    }
    if (!valid) {
	diagnose(Severity::Error, "invalid_transform_parameters",
	    "transformation matrix has invalid or missing parameters", entry);
	matrix = Matrix();
    } else if (entry && !entry->transform.empty()) {
	matrix = multiply(transform(entry->transform, active), matrix);
    }
    active.erase(id);
    transforms_[id] = matrix;
    return matrix;
}

Matrix
Translator::transform(const DirectoryEntry &entry)
{
    std::set<EntityId> active;
    return transform(entry.transform, active);
}

Point3
Translator::point(const DirectoryEntry &entry, const Point3 &value)
{
    Point3 transformed = apply_point(transform(entry), value);
    return scale(transformed, unit_to_mm_);
}

Point3
Translator::vector(const DirectoryEntry &entry, const Point3 &value)
{
    return scale(apply_vector(transform(entry), value), unit_to_mm_);
}

Style
Translator::style(const DirectoryEntry &entry, uint32_t role,
    const std::string &symbol) const
{
    Style result;
    result.role = role;
    result.symbol = symbol;
    switch (std::abs(entry.line_font)) {
	case 2: result.line_pattern = RT_ANNOT_LINE_DASHED; break;
	case 3: result.line_pattern = RT_ANNOT_LINE_PHANTOM; break;
	case 4: result.line_pattern = RT_ANNOT_LINE_CENTER; break;
	case 5: result.line_pattern = RT_ANNOT_LINE_DOTTED; break;
	default: result.line_pattern = RT_ANNOT_LINE_CONTINUOUS; break;
    }

    const int color = entry.color;
    static const unsigned char standard[][3] = {
	{0, 0, 0}, {0, 0, 0}, {255, 0, 0}, {0, 255, 0},
	{0, 0, 255}, {255, 255, 0}, {255, 0, 255}, {0, 255, 255},
	{255, 255, 255}
    };
    if (color > 0 && static_cast<size_t>(color) <
	    sizeof(standard) / sizeof(standard[0])) {
	result.flags |= RT_ANNOT_STYLE_COLOR;
	std::copy(standard[color], standard[color] + 3, result.color.begin());
    } else if (color < 0) {
	const DirectoryEntry *definition = document_.entity(EntityId(-color));
	const ParameterList *parameters = definition ?
	    document_.parameters(definition->id) : nullptr;
	if (definition && definition->type == 314 && parameters) {
	    bool valid = true;
	    for (size_t component = 0; component < 3; ++component) {
		double percent = 0.0;
		valid = parameter_real(parameters, component + 1, percent) && valid;
		result.color[component] = static_cast<unsigned char>(
		    std::max(0.0, std::min(100.0, percent)) * 2.55 + 0.5);
	    }
	    if (valid)
		result.flags |= RT_ANNOT_STYLE_COLOR;
	}
    }

    const int gradations = global_integer(document_.global(),
	GLOBAL_LINE_GRADATIONS, 0);
    const double maximum_width = global_real(document_.global(),
	GLOBAL_MAX_LINE_WIDTH, 0.0);
    if (entry.line_weight > 0 && gradations > 0 && maximum_width > 0.0) {
	result.flags |= RT_ANNOT_STYLE_WIDTH;
	result.line_width = maximum_width * unit_to_mm_ * entry.line_weight /
	    gradations;
    }
    return result;
}

std::string
Translator::unique_name(const DirectoryEntry &entry, const char *suffix)
{
    return unique_name(entry, entry.label, suffix);
}

std::string
Translator::unique_name(const DirectoryEntry &entry,
    const std::string &source, const char *suffix)
{
    std::string stem = sanitize_name(source);
    if (stem.empty())
	stem = "iges_" + std::to_string(entry.type) + "_d" +
	    std::to_string(entry.id.value());
    if (entry.subscript > 0)
	stem += "_" + std::to_string(entry.subscript);
    stem += suffix;

    if (db_lookup(wdbp_->dbip, stem.c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
	return stem;

    const std::string collision_stem = stem + ".D" +
	std::to_string(entry.id.value());
    std::string candidate = collision_stem;
    size_t serial = 1;
    while (db_lookup(wdbp_->dbip, candidate.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	candidate = collision_stem + "." + std::to_string(serial++);
    return candidate;
}

bool
Translator::write_annotation(const DirectoryEntry &entry,
    const std::string &name, const AnnotationData &data)
{
    if (data.vertices.empty() || data.segments.empty())
	return false;

    struct NurbStorage {
	struct nurb_seg segment = {};
	std::vector<int> controls;
	std::vector<fastf_t> knots;
	std::vector<fastf_t> weights;
    };
    size_t line_count = 0;
    size_t arc_count = 0;
    size_t nurb_count = 0;
    size_t text_count = 0;
    for (const Segment &segment : data.segments) {
	switch (segment.kind) {
	    case SegmentKind::Line: ++line_count; break;
	    case SegmentKind::Arc: ++arc_count; break;
	    case SegmentKind::Nurbs: ++nurb_count; break;
	    case SegmentKind::Text: ++text_count; break;
	}
    }

    std::vector<struct line_seg> lines(line_count);
    std::vector<struct carc_seg> arcs(arc_count);
    std::vector<NurbStorage> nurbs(nurb_count);
    std::vector<struct txt_seg> texts(text_count);
    struct rt_annot_internal annotation = {};
    annotation.magic = RT_ANNOT_INTERNAL_MAGIC;
    annotation.flags = RT_ANNOT_MODEL_SPACE;
    VSET(annotation.V, data.plane.origin[0], data.plane.origin[1],
	data.plane.origin[2]);
    VSET(annotation.u_vec, data.plane.u[0], data.plane.u[1], data.plane.u[2]);
    VSET(annotation.v_vec, data.plane.v[0], data.plane.v[1], data.plane.v[2]);
    annotation.vert_count = data.vertices.size();
    annotation.verts = static_cast<point2d_t *>(bu_calloc(
	annotation.vert_count, sizeof(point2d_t), "IGES annotation vertices"));
    for (size_t i = 0; i < data.vertices.size(); ++i)
	V2SET(annotation.verts[i], data.vertices[i][0], data.vertices[i][1]);

    annotation.ant.count = data.segments.size();
    annotation.ant.reverse = static_cast<int *>(bu_calloc(
	annotation.ant.count, sizeof(int), "IGES annotation reverse flags"));
    annotation.ant.segments = static_cast<void **>(bu_calloc(
	annotation.ant.count, sizeof(void *), "IGES annotation segments"));
    annotation.styles = static_cast<struct rt_annot_seg_style *>(bu_calloc(
	annotation.ant.count, sizeof(struct rt_annot_seg_style),
	"IGES annotation styles"));

    size_t line_index = 0;
    size_t arc_index = 0;
    size_t nurb_index = 0;
    size_t text_index = 0;
    for (size_t i = 0; i < data.segments.size(); ++i) {
	const Segment &source = data.segments[i];
	struct rt_annot_seg_style &destination_style = annotation.styles[i];
	destination_style.role = source.style.role;
	destination_style.flags = source.style.flags;
	destination_style.line_pattern = source.style.line_pattern;
	destination_style.line_width = source.style.line_width;
	std::copy(source.style.color.begin(), source.style.color.end(),
	    destination_style.color);
	destination_style.font = source.style.font.empty() ? nullptr :
	    const_cast<char *>(source.style.font.c_str());
	destination_style.symbol = source.style.symbol.empty() ? nullptr :
	    const_cast<char *>(source.style.symbol.c_str());
	destination_style.x_scale = source.style.x_scale;
	destination_style.xy_scale = source.style.xy_scale;
	destination_style.yx_scale = source.style.yx_scale;
	destination_style.y_scale = source.style.y_scale;

	switch (source.kind) {
	    case SegmentKind::Line: {
		struct line_seg &line = lines[line_index++];
		line.magic = CURVE_LSEG_MAGIC;
		line.start = source.start;
		line.end = source.end;
		annotation.ant.segments[i] = &line;
		break;
	    }
	    case SegmentKind::Arc: {
		struct carc_seg &arc = arcs[arc_index++];
		arc.magic = CURVE_CARC_MAGIC;
		arc.start = source.start;
		arc.end = source.end;
		arc.center = source.center;
		arc.radius = source.radius;
		arc.center_is_left = source.center_is_left ? 1 : 0;
		arc.orientation = source.clockwise ? 1 : 0;
		annotation.ant.segments[i] = &arc;
		break;
	    }
	    case SegmentKind::Nurbs: {
		NurbStorage &nurb = nurbs[nurb_index++];
		nurb.controls = source.controls;
		nurb.knots.assign(source.knots.begin(), source.knots.end());
		nurb.weights.assign(source.weights.begin(), source.weights.end());
		nurb.segment.magic = CURVE_NURB_MAGIC;
		nurb.segment.order = source.order;
		const int rational = nurb.weights.empty() ?
		    RT_NURB_PT_NONRAT : RT_NURB_PT_RATIONAL;
		nurb.segment.pt_type = RT_NURB_MAKE_PT_TYPE(
		    (nurb.weights.empty() ? 2 : 3), RT_NURB_PT_XY,
		    rational);
		nurb.segment.k.k_size = static_cast<int>(nurb.knots.size());
		nurb.segment.k.knots = nurb.knots.data();
		nurb.segment.c_size = static_cast<int>(nurb.controls.size());
		nurb.segment.ctl_points = nurb.controls.data();
		nurb.segment.weights = nurb.weights.empty() ? nullptr :
		    nurb.weights.data();
		annotation.ant.segments[i] = &nurb.segment;
		break;
	    }
	    case SegmentKind::Text: {
		struct txt_seg &text = texts[text_index++];
		text.magic = ANN_TSEG_MAGIC;
		text.ref_pt = source.reference;
		text.rel_pos = source.relative_position;
		text.txt_size = source.text_size;
		text.txt_rot_angle = source.text_rotation;
		bu_vls_init(&text.label);
		bu_vls_strcpy(&text.label, source.text.c_str());
		annotation.ant.segments[i] = &text;
		break;
	    }
	}
    }

    struct bu_vls validation = BU_VLS_INIT_ZERO;
    const int invalid = rt_annot_validate(&annotation, &validation);
    const int written = invalid ? -1 : mk_annot(wdbp_, name.c_str(), &annotation);
    if (invalid)
	diagnose(Severity::Error, "invalid_annotation",
	    bu_vls_cstr(&validation), &entry);
    else if (written < 0)
	diagnose(Severity::Error, "annotation_write",
	    "failed to write BRL-CAD annotation object", &entry);

    for (struct txt_seg &text : texts)
	bu_vls_free(&text.label);
    bu_vls_free(&validation);
    bu_free(annotation.styles, "IGES annotation styles");
    bu_free(annotation.ant.segments, "IGES annotation segments");
    bu_free(annotation.ant.reverse, "IGES annotation reverse flags");
    bu_free(annotation.verts, "IGES annotation vertices");
    if (invalid || written < 0)
	return false;

    write_entity_attributes(entry, name);
    ++result_.statistics.objects_written;
    ++result_.statistics.annotations_written;
    return true;
}

void
Translator::write_entity_attributes(const DirectoryEntry &entry,
    const std::string &name)
{
    const std::string entity_id = std::to_string(entry.id.value());
    const std::string entity_type = std::to_string(entry.type);
    const std::string entity_form = std::to_string(entry.form);
    const std::string entity_level = std::to_string(entry.level);
    const std::string entity_color = std::to_string(entry.color);
    const std::string entity_subscript = std::to_string(entry.subscript);
    db5_update_attribute(name.c_str(), "importer", "iges-g", wdbp_->dbip);
    db5_update_attribute(name.c_str(), "source_format", "iges", wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.entity", entity_id.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.type", entity_type.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.form", entity_form.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.level", entity_level.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.color", entity_color.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.subscript",
	entity_subscript.c_str(), wdbp_->dbip);
    if (!entry.label.empty())
	db5_update_attribute(name.c_str(), "iges.label", entry.label.c_str(),
	    wdbp_->dbip);
}

bool
Translator::translate_point(const DirectoryEntry &entry,
    const std::string &name)
{
    const ParameterList *parameters = document_.parameters(entry.id);
    Point3 local;
    for (size_t coordinate = 0; coordinate < local.size(); ++coordinate)
	if (!parameter_real(parameters, coordinate + 1, local[coordinate])) {
	    diagnose(Severity::Error, "point_parameters",
		"Point entity has invalid coordinates", &entry);
	    return false;
	}
    Point3 model = point(entry, local);
    if (options_.project_drawings)
	model[2] = 0.0;

    struct rt_datum_internal datum = {};
    datum.magic = RT_DATUM_INTERNAL_MAGIC;
    datum.type = RT_DATUM_POINT;
    datum.role = RT_DATUM_ROLE_REFERENCE;
    VSET(datum.pnt, model[0], model[1], model[2]);
    if (rt_datum_validate(&datum, nullptr) ||
	    mk_datums(wdbp_, name.c_str(), &datum) < 0) {
	diagnose(Severity::Error, "point_write",
	    "failed to write Point entity as a BRL-CAD datum", &entry);
	return false;
    }
    write_entity_attributes(entry, name);
    db5_update_attribute(name.c_str(), "iges.semantic", "point",
	wdbp_->dbip);
    ++result_.statistics.objects_written;
    ++result_.statistics.datums_written;
    return true;
}

bool
Translator::translate_line(const DirectoryEntry &entry, const std::string &name)
{
    const ParameterList *parameters = document_.parameters(entry.id);
    std::vector<Point3> points(2);
    for (size_t point_index = 0; point_index < 2; ++point_index) {
	Point3 local;
	for (size_t coordinate = 0; coordinate < 3; ++coordinate) {
	    if (!parameter_real(parameters,
		    1 + point_index * 3 + coordinate, local[coordinate])) {
		diagnose(Severity::Error, "line_parameters",
		    "Line entity has invalid endpoint coordinates", &entry);
		return false;
	    }
	}
	points[point_index] = point(entry, local);
	if (options_.project_drawings)
	    points[point_index][2] = 0.0;
    }

    AnnotationData data;
    const bool have_plane = options_.project_drawings ?
	xy_plane_from_points(points, data.plane, data.vertices) :
	plane_from_points(points, data.plane, data.vertices);
    if (!have_plane) {
	diagnose(Severity::Warning, "degenerate_line",
	    "Line entity has coincident endpoints and was omitted", &entry);
	return false;
    }
    Segment segment;
    segment.kind = SegmentKind::Line;
    segment.start = 0;
    segment.end = 1;
    segment.style = style(entry, RT_ANNOT_ROLE_GEOMETRY);
    data.segments.push_back(std::move(segment));
    return write_annotation(entry, name, data);
}

bool
Translator::translate_arc(const DirectoryEntry &entry, const std::string &name)
{
    const ParameterList *parameters = document_.parameters(entry.id);
    double z = 0.0;
    Point2 center;
    Point2 start;
    Point2 end;
    if (!parameter_real(parameters, 1, z) ||
	    !parameter_real(parameters, 2, center[0]) ||
	    !parameter_real(parameters, 3, center[1]) ||
	    !parameter_real(parameters, 4, start[0]) ||
	    !parameter_real(parameters, 5, start[1]) ||
	    !parameter_real(parameters, 6, end[0]) ||
	    !parameter_real(parameters, 7, end[1])) {
	diagnose(Severity::Error, "arc_parameters",
	    "Circular Arc entity has invalid parameters", &entry);
	return false;
    }
    double start_radius = std::hypot(start[0] - center[0],
	start[1] - center[1]);
    const double end_radius = std::hypot(end[0] - center[0],
	end[1] - center[1]);
    if (start_radius <= MIN_VECTOR_LENGTH || end_radius <= MIN_VECTOR_LENGTH) {
	diagnose(Severity::Warning, "degenerate_arc",
	    "Circular Arc has a zero-radius endpoint and was omitted", &entry);
	return false;
    }
    const double relative_radius_error = std::fabs(start_radius - end_radius) /
	std::max(start_radius, end_radius);
    if (relative_radius_error > COPLANAR_RELATIVE_TOLERANCE) {
	if (relative_radius_error <= ARC_RADIUS_REPAIR_LIMIT &&
		options_.repair == RepairMode::Safe && !options_.exact &&
		!options_.strict) {
	    start_radius = 0.5 * (start_radius + end_radius);
	    ++result_.statistics.repairs;
	    diagnose(Severity::Information, "normalized_arc_radius",
		"normalized endpoint roundoff on a Circular Arc", &entry);
	} else {
	    diagnose(Severity::Warning, "arc_radius",
		"Circular Arc endpoints do not define one radius and were omitted",
		&entry);
	    return false;
	}
    }

    AnnotationData data;
    data.plane.origin = point(entry, {0.0, 0.0, z});
    data.plane.u = vector(entry, {1.0, 0.0, 0.0});
    data.plane.v = vector(entry, {0.0, 1.0, 0.0});
    if (options_.project_drawings) {
	data.plane.origin[2] = 0.0;
	data.plane.u[2] = 0.0;
	data.plane.v[2] = 0.0;
    }
    if (length(cross(data.plane.u, data.plane.v)) <= MIN_VECTOR_LENGTH) {
	diagnose(Severity::Warning, "edge_on_arc",
	    "Circular Arc collapses under the requested drawing projection",
	    &entry);
	return false;
    }
    data.vertices = {start, end, center};
    Segment segment;
    segment.kind = SegmentKind::Arc;
    segment.start = 0;
    segment.end = 1;
    segment.center = 2;
    segment.radius = start_radius;
    const double side = (end[0] - start[0]) * (center[1] - start[1]) -
	(end[1] - start[1]) * (center[0] - start[0]);
    segment.center_is_left = side > 0.0;
    segment.clockwise = false;
    if (std::hypot(start[0] - end[0], start[1] - end[1]) <=
	    COPLANAR_RELATIVE_TOLERANCE * start_radius) {
	segment.end = 2;
	segment.radius = -start_radius;
    }
    segment.style = style(entry, RT_ANNOT_ROLE_GEOMETRY);
    data.segments.push_back(std::move(segment));
    return write_annotation(entry, name, data);
}

bool
Translator::translate_copious(const DirectoryEntry &entry,
    const std::string &name)
{
    const ParameterList *parameters = document_.parameters(entry.id);
    int interpretation = 0;
    int count = 0;
    if (!parameter_integer(parameters, 1, interpretation) ||
	    !parameter_integer(parameters, 2, count) || count < 2 ||
	    count > MAX_ENTITY_LIST_COUNT) {
	diagnose(Severity::Error, "copious_parameters",
	    "Copious Data entity has an invalid point count", &entry);
	return false;
    }

    std::vector<Point3> points;
    points.reserve(static_cast<size_t>(count));
    size_t parameter = 3;
    double common_z = 0.0;
    if (interpretation == 1 && !parameter_real(parameters, parameter++, common_z)) {
	diagnose(Severity::Error, "copious_parameters",
	    "Copious Data entity has an invalid common Z coordinate", &entry);
	return false;
    }
    for (int i = 0; i < count; ++i) {
	Point3 local = {0.0, 0.0, common_z};
	const size_t coordinate_count = interpretation == 1 ? 2 : 3;
	if (interpretation != 1 && interpretation != 2 && interpretation != 3) {
	    diagnose(Severity::Warning, "copious_interpretation",
		"unsupported Copious Data interpretation", &entry);
	    return false;
	}
	bool valid = true;
	for (size_t coordinate = 0; coordinate < coordinate_count; ++coordinate)
	    valid = parameter_real(parameters, parameter++, local[coordinate]) && valid;
	if (interpretation == 3)
	    parameter += 3;
	if (!valid) {
	    diagnose(Severity::Error, "copious_parameters",
		"Copious Data entity has invalid coordinates", &entry);
	    return false;
	}
	Point3 transformed = point(entry, local);
	if (options_.project_drawings)
	    transformed[2] = 0.0;
	points.push_back(transformed);
    }

    AnnotationData data;
    const bool have_plane = options_.project_drawings ?
	xy_plane_from_points(points, data.plane, data.vertices) :
	plane_from_points(points, data.plane, data.vertices);
    if (!have_plane) {
	diagnose(Severity::Warning, "nonplanar_copious_data",
	    "non-planar Copious Data requires the wire-geometry fallback", &entry);
	return false;
    }
    for (int i = 1; i < count; ++i) {
	Segment segment;
	segment.kind = SegmentKind::Line;
	segment.start = i - 1;
	segment.end = i;
	segment.style = style(entry, RT_ANNOT_ROLE_GEOMETRY);
	data.segments.push_back(std::move(segment));
    }
    return write_annotation(entry, name, data);
}

bool
Translator::translate_nurbs(const DirectoryEntry &entry, const std::string &name)
{
    const ParameterList *parameters = document_.parameters(entry.id);
    int upper_index = 0;
    int degree = 0;
    int planar = 0;
    int closed = 0;
    int polynomial = 0;
    int periodic = 0;
    if (!parameter_integer(parameters, 1, upper_index) ||
	    !parameter_integer(parameters, 2, degree) ||
	    !parameter_integer(parameters, 3, planar) ||
	    !parameter_integer(parameters, 4, closed) ||
	    !parameter_integer(parameters, 5, polynomial) ||
	    !parameter_integer(parameters, 6, periodic) || upper_index < 1 ||
	    upper_index > MAX_ENTITY_LIST_COUNT || degree < 1 ||
	    degree > upper_index) {
	diagnose(Severity::Error, "nurbs_parameters",
	    "Rational B-Spline Curve has invalid degree or flags", &entry);
	return false;
    }
    const size_t control_count = static_cast<size_t>(upper_index + 1);
    const size_t knot_count = static_cast<size_t>(upper_index + degree + 2);
    size_t parameter = 7;
    std::vector<double> knots(knot_count);
    for (double &knot : knots)
	if (!parameter_real(parameters, parameter++, knot)) {
	    diagnose(Severity::Error, "nurbs_knots",
		"Rational B-Spline Curve has an invalid knot vector", &entry);
	    return false;
	}
    std::vector<double> weights(control_count);
    for (double &weight : weights)
	if (!parameter_real(parameters, parameter++, weight) || weight <= 0.0) {
	    diagnose(Severity::Error, "nurbs_weights",
		"Rational B-Spline Curve has a non-positive weight", &entry);
	    return false;
	}
    std::vector<Point3> controls(control_count);
    for (Point3 &control : controls) {
	Point3 local;
	for (double &coordinate : local)
	    if (!parameter_real(parameters, parameter++, coordinate)) {
		diagnose(Severity::Error, "nurbs_controls",
		    "Rational B-Spline Curve has invalid control points", &entry);
		return false;
	    }
	control = point(entry, local);
	if (options_.project_drawings)
	    control[2] = 0.0;
    }

    AnnotationData data;
    const bool have_plane = options_.project_drawings ?
	xy_plane_from_points(controls, data.plane, data.vertices) :
	plane_from_points(controls, data.plane, data.vertices);
    if (!have_plane) {
	diagnose(Severity::Warning, "nonplanar_nurbs",
	    "non-planar B-Spline Curve requires the wire-geometry fallback", &entry);
	return false;
    }
    Segment segment;
    segment.kind = SegmentKind::Nurbs;
    segment.order = degree + 1;
    segment.knots = std::move(knots);
    segment.controls.resize(control_count);
    for (size_t i = 0; i < control_count; ++i)
	segment.controls[i] = static_cast<int>(i);
    if (!polynomial)
	segment.weights = std::move(weights);
    segment.style = style(entry, RT_ANNOT_ROLE_GEOMETRY,
	periodic ? "iges-periodic-bspline" :
	(closed ? "iges-closed-bspline" : std::string()));
    data.segments.push_back(std::move(segment));
    return write_annotation(entry, name, data);
}

bool
Translator::translate_note(const DirectoryEntry &entry, const std::string &name)
{
    const ParameterList *parameters = document_.parameters(entry.id);
    int string_count = 0;
    if (!parameter_integer(parameters, 1, string_count) || string_count < 1 ||
	    string_count > MAX_ENTITY_LIST_COUNT) {
	diagnose(Severity::Error, "note_parameters",
	    "General Note has an invalid string count", &entry);
	return false;
    }

    AnnotationData data;
    bool have_plane = false;
    double plane_z = 0.0;
    size_t parameter = 2;
    for (int string_index = 0; string_index < string_count; ++string_index) {
	int declared_length = 0;
	double width = 0.0;
	double height = 0.0;
	int font_code = 0;
	double slant = M_PI_2;
	double rotation = 0.0;
	int mirror = 0;
	int internal_rotation = 0;
	Point3 location;
	std::string text;
	bool valid = parameter_integer(parameters, parameter++, declared_length) &&
	    parameter_real(parameters, parameter++, width) &&
	    parameter_real(parameters, parameter++, height) &&
	    parameter_integer(parameters, parameter++, font_code) &&
	    parameter_real(parameters, parameter++, slant) &&
	    parameter_real(parameters, parameter++, rotation) &&
	    parameter_integer(parameters, parameter++, mirror) &&
	    parameter_integer(parameters, parameter++, internal_rotation);
	for (double &coordinate : location)
	    valid = parameter_real(parameters, parameter++, coordinate) && valid;
	valid = parameters && parameter < parameters->values.size() &&
	    parameters->values[parameter++].string(text) && valid;
	if (!valid || declared_length < 0 || width <= 0.0 || height <= 0.0 ||
		text.empty()) {
	    diagnose(Severity::Error, "note_string",
		"General Note contains an invalid text string", &entry);
	    return false;
	}
	if (!have_plane) {
	    plane_z = location[2];
	    data.plane.origin = point(entry, {0.0, 0.0, plane_z});
	    data.plane.u = vector(entry, {1.0, 0.0, 0.0});
	    data.plane.v = vector(entry, {0.0, 1.0, 0.0});
	    if (options_.project_drawings) {
		data.plane.origin[2] = 0.0;
		data.plane.u[2] = 0.0;
		data.plane.v[2] = 0.0;
	    }
	    have_plane = true;
	} else if (std::fabs(location[2] - plane_z) >
		COPLANAR_RELATIVE_TOLERANCE * std::max(1.0, std::fabs(plane_z))) {
	    diagnose(Severity::Warning, "multiplane_note",
		"General Note strings occupy multiple planes", &entry);
	    return false;
	}

	const double glyph_count = std::max<size_t>(1, text.size());
	const double text_size = std::min(height, width / glyph_count);
	Point2 reference = {location[0], location[1]};
	if (text_size < height)
	    reference[1] += (height - text_size) * 0.5;
	if (text_size * glyph_count < width)
	    reference[0] += (width - text_size * glyph_count) * 0.5;
	const int reference_index = static_cast<int>(data.vertices.size());
	data.vertices.push_back(reference);
	Segment segment;
	segment.kind = SegmentKind::Text;
	segment.reference = reference_index;
	segment.relative_position = RT_TXT_POS_BL;
	segment.text_size = text_size;
	segment.text_rotation = rotation * RAD2DEG;
	segment.text = text;
	segment.style = style(entry, RT_ANNOT_ROLE_TEXT,
	    "iges-general-note");
	segment.style.font = font_code > 0 ?
	    "iges-font-" + std::to_string(font_code) : std::string();
	if (mirror == 1 || mirror == 3) {
	    segment.style.flags |= RT_ANNOT_STYLE_SCALE;
	    segment.style.x_scale = -1.0;
	}
	if (mirror == 2 || mirror == 3) {
	    segment.style.flags |= RT_ANNOT_STYLE_SCALE;
	    segment.style.y_scale = -1.0;
	}
	if (std::fabs(slant - M_PI_2) > SMALL_FASTF) {
	    segment.style.flags |= RT_ANNOT_STYLE_SCALE;
	    segment.style.xy_scale = 1.0 / std::tan(slant);
	}
	if (internal_rotation)
	    segment.style.symbol += ":vertical";
	data.segments.push_back(std::move(segment));
    }
    if (length(cross(data.plane.u, data.plane.v)) <= MIN_VECTOR_LENGTH) {
	diagnose(Severity::Warning, "edge_on_note",
	    "General Note collapses under the requested drawing projection",
	    &entry);
	return false;
    }
    return write_annotation(entry, name, data);
}

bool
Translator::translate_leader(const DirectoryEntry &entry, const std::string &name)
{
    const ParameterList *parameters = document_.parameters(entry.id);
    int point_count = 0;
    double arrow_height = 0.0;
    double arrow_width = 0.0;
    double z = 0.0;
    Point2 tip;
    Point2 next;
    if (!parameter_integer(parameters, 1, point_count) || point_count < 1 ||
	    point_count > MAX_ENTITY_LIST_COUNT ||
	    !parameter_real(parameters, 2, arrow_height) || arrow_height < 0.0 ||
	    !parameter_real(parameters, 3, arrow_width) || arrow_width < 0.0 ||
	    !parameter_real(parameters, 4, z) ||
	    !parameter_real(parameters, 5, tip[0]) ||
	    !parameter_real(parameters, 6, tip[1]) ||
	    !parameter_real(parameters, 7, next[0]) ||
	    !parameter_real(parameters, 8, next[1])) {
	diagnose(Severity::Error, "leader_parameters",
	    "Leader entity has invalid parameters", &entry);
	return false;
    }

    AnnotationData data;
    data.plane.origin = point(entry, {0.0, 0.0, z});
    data.plane.u = vector(entry, {1.0, 0.0, 0.0});
    data.plane.v = vector(entry, {0.0, 1.0, 0.0});
    if (options_.project_drawings) {
	data.plane.origin[2] = 0.0;
	data.plane.u[2] = 0.0;
	data.plane.v[2] = 0.0;
    }
    if (length(cross(data.plane.u, data.plane.v)) <= MIN_VECTOR_LENGTH) {
	diagnose(Severity::Warning, "edge_on_leader",
	    "Leader collapses under the requested drawing projection", &entry);
	return false;
    }

    const Point2 arrow_center = tip;
    Point2 direction = {next[0] - tip[0], next[1] - tip[1]};
    const double direction_length = std::hypot(direction[0], direction[1]);
    if (direction_length <= MIN_VECTOR_LENGTH) {
	diagnose(Severity::Error, "degenerate_leader",
	    "Leader arrow and first segment point coincide", &entry);
	return false;
    }
    direction[0] /= direction_length;
    direction[1] /= direction_length;
    if (entry.form == 5 || entry.form == 6) {
	tip[0] += arrow_height * direction[0];
	tip[1] += arrow_height * direction[1];
    }

    data.vertices.push_back(tip);
    data.vertices.push_back(next);
    size_t parameter = 9;
    for (int i = 1; i < point_count; ++i) {
	Point2 following;
	if (!parameter_real(parameters, parameter++, following[0]) ||
		!parameter_real(parameters, parameter++, following[1])) {
	    diagnose(Severity::Error, "leader_points",
		"Leader entity has invalid polyline points", &entry);
	    return false;
	}
	data.vertices.push_back(following);
    }
    for (size_t i = 1; i < data.vertices.size(); ++i) {
	Segment line;
	line.kind = SegmentKind::Line;
	line.start = static_cast<int>(i - 1);
	line.end = static_cast<int>(i);
	line.style = style(entry, RT_ANNOT_ROLE_LEADER, "iges-leader");
	data.segments.push_back(std::move(line));
    }

    const std::string arrow_symbol = "iges-arrowhead-form-" +
	std::to_string(entry.form);
    const Point2 perpendicular = {-direction[1], direction[0]};
    const auto add_arrow_line = [&](const Point2 &first, const Point2 &second) {
	const int first_index = static_cast<int>(data.vertices.size());
	data.vertices.push_back(first);
	const int second_index = static_cast<int>(data.vertices.size());
	data.vertices.push_back(second);
	Segment line;
	line.kind = SegmentKind::Line;
	line.start = first_index;
	line.end = second_index;
	line.style = style(entry, RT_ANNOT_ROLE_ARROWHEAD, arrow_symbol);
	data.segments.push_back(std::move(line));
    };

    switch (entry.form) {
	case 4:
	    break;
	case 5:
	case 6: {
	    const Point2 circle_point = {
		arrow_center[0] + arrow_height * direction[0],
		arrow_center[1] + arrow_height * direction[1]
	    };
	    const int start_index = static_cast<int>(data.vertices.size());
	    data.vertices.push_back(circle_point);
	    const int center_index = static_cast<int>(data.vertices.size());
	    data.vertices.push_back(arrow_center);
	    Segment circle;
	    circle.kind = SegmentKind::Arc;
	    circle.start = start_index;
	    circle.end = center_index;
	    circle.center = center_index;
	    circle.radius = -arrow_height;
	    circle.style = style(entry, RT_ANNOT_ROLE_ARROWHEAD, arrow_symbol);
	    data.segments.push_back(std::move(circle));
	    break;
	}
	case 7:
	case 8: {
	    const Point2 left_tip = {
		arrow_center[0] + 0.5 * arrow_width * perpendicular[0],
		arrow_center[1] + 0.5 * arrow_width * perpendicular[1]
	    };
	    const Point2 right_tip = {
		arrow_center[0] - 0.5 * arrow_width * perpendicular[0],
		arrow_center[1] - 0.5 * arrow_width * perpendicular[1]
	    };
	    const Point2 left_back = {
		left_tip[0] + arrow_height * direction[0],
		left_tip[1] + arrow_height * direction[1]
	    };
	    const Point2 right_back = {
		right_tip[0] + arrow_height * direction[0],
		right_tip[1] + arrow_height * direction[1]
	    };
	    add_arrow_line(left_tip, left_back);
	    add_arrow_line(left_back, right_back);
	    add_arrow_line(right_back, right_tip);
	    add_arrow_line(right_tip, left_tip);
	    break;
	}
	case 9:
	case 10: {
	    const Point2 first = {
		arrow_center[0] + 0.5 * (arrow_height * direction[0] +
		    arrow_width * perpendicular[0]),
		arrow_center[1] + 0.5 * (arrow_height * direction[1] +
		    arrow_width * perpendicular[1])
	    };
	    const Point2 second = {
		arrow_center[0] - 0.5 * (arrow_height * direction[0] +
		    arrow_width * perpendicular[0]),
		arrow_center[1] - 0.5 * (arrow_height * direction[1] +
		    arrow_width * perpendicular[1])
	    };
	    add_arrow_line(first, second);
	    break;
	}
	default: {
	    const Point2 left = {
		arrow_center[0] + arrow_height * direction[0] +
		    arrow_width * perpendicular[0],
		arrow_center[1] + arrow_height * direction[1] +
		    arrow_width * perpendicular[1]
	    };
	    const Point2 right = {
		arrow_center[0] + arrow_height * direction[0] -
		    arrow_width * perpendicular[0],
		arrow_center[1] + arrow_height * direction[1] -
		    arrow_width * perpendicular[1]
	    };
	    add_arrow_line(arrow_center, left);
	    add_arrow_line(arrow_center, right);
	    break;
	}
    }
    return write_annotation(entry, name, data);
}

bool
Translator::translate(const DirectoryEntry &entry, const std::string &name)
{
    switch (entry.type) {
	case 100: return translate_arc(entry, name);
	case 106: return translate_copious(entry, name);
	case 110: return translate_line(entry, name);
	case 116: return translate_point(entry, name);
	case 126: return translate_nurbs(entry, name);
	case 212: return translate_note(entry, name);
	case 214: return translate_leader(entry, name);
	default: return false;
    }
}

bool
Translator::write_groups()
{
    for (const DirectoryEntry &entry : document_.entities()) {
	if (!is_dimension_type(entry.type))
	    continue;
	const ParameterList *parameters = document_.parameters(entry.id);
	if (!parameters)
	    continue;
	struct wmember members;
	BU_LIST_INIT(&members.l);
	size_t member_count = 0;
	for (size_t i = 1; i < parameters->values.size(); ++i) {
	    EntityId reference;
	    if (!parameter_entity(parameters, i, reference))
		continue;
	    const auto object = objects_.find(reference);
	    if (object == objects_.end() || grouped_.find(reference) != grouped_.end())
		continue;
	    if (mk_addmember(object->second.c_str(), &members.l, nullptr,
		    WMOP_UNION) == WMEMBER_NULL) {
		diagnose(Severity::Error, "dimension_member",
		    "failed to add an annotation to its semantic group", &entry);
		return false;
	    }
	    grouped_.insert(reference);
	    ++member_count;
	}
	if (!member_count)
	    continue;
	const std::string name = unique_name(entry, ".annot_group");
	const int write_status = mk_lfcomb(wdbp_, name.c_str(), &members, 0)
	if (write_status < 0) {
	    diagnose(Severity::Error, "dimension_write",
		"failed to write semantic annotation group", &entry);
	    return false;
	}
	const std::string entity_id = std::to_string(entry.id.value());
	const std::string entity_type = std::to_string(entry.type);
	const std::string entity_form = std::to_string(entry.form);
	const std::string semantic = semantic_name(entry.type);
	db5_update_attribute(name.c_str(), "iges.entity", entity_id.c_str(),
	    wdbp_->dbip);
	db5_update_attribute(name.c_str(), "iges.type", entity_type.c_str(),
	    wdbp_->dbip);
	db5_update_attribute(name.c_str(), "iges.form", entity_form.c_str(),
	    wdbp_->dbip);
	db5_update_attribute(name.c_str(), "iges.semantic", semantic.c_str(),
	    wdbp_->dbip);
	groups_.push_back(name);
	++result_.statistics.objects_written;
	++result_.statistics.semantic_groups_written;
    }
    return true;
}

bool
Translator::write_subfigures()
{
    for (const DirectoryEntry &entry : document_.entities()) {
	if (entry.type != 308)
	    continue;
	const ParameterList *parameters = document_.parameters(entry.id);
	int member_count = 0;
	if (!parameter_integer(parameters, 3, member_count) || member_count < 0 ||
		member_count > MAX_ENTITY_LIST_COUNT) {
	    diagnose(Severity::Warning, "subfigure_definition_parameters",
		"Subfigure Definition has an invalid member count", &entry);
	    ++result_.statistics.omitted;
	    continue;
	}

	std::vector<EntityId> source_members;
	bool valid_definition = true;
	for (int i = 0; i < member_count; ++i) {
	    EntityId member_id;
	    if (!parameter_entity(parameters, static_cast<size_t>(i + 4),
		    member_id)) {
		diagnose(Severity::Warning, "subfigure_definition_parameters",
		    "Subfigure Definition has an invalid member reference", &entry);
		valid_definition = false;
		break;
	    }
	    source_members.push_back(member_id);
	}
	if (!valid_definition) {
	    ++result_.statistics.omitted;
	    continue;
	}

	struct wmember members;
	BU_LIST_INIT(&members.l);
	std::vector<EntityId> imported_members;
	for (EntityId member_id : source_members) {
	    const auto object = objects_.find(member_id);
	    if (object == objects_.end())
		continue;
	    if (mk_addmember(object->second.c_str(), &members.l, nullptr,
		    WMOP_UNION) == WMEMBER_NULL) {
		diagnose(Severity::Error, "subfigure_definition_member",
		    "failed to add an annotation to a Subfigure Definition", &entry);
		return false;
	    }
	    imported_members.push_back(member_id);
	}
	if (imported_members.empty())
	    continue;

	std::string source_name;
	parameter_string(parameters, 2, source_name);
	const std::string name = unique_name(entry, source_name, ".annot_def");
	const int write_status = mk_lfcomb(wdbp_, name.c_str(), &members, 0)
	if (write_status < 0) {
	    diagnose(Severity::Error, "subfigure_definition_write",
		"failed to write an annotation Subfigure Definition", &entry);
	    return false;
	}
	write_entity_attributes(entry, name);
	if (!source_name.empty())
	    db5_update_attribute(name.c_str(), "iges.name", source_name.c_str(),
		wdbp_->dbip);
	db5_update_attribute(name.c_str(), "iges.semantic",
	    "subfigure_definition", wdbp_->dbip);
	objects_[entry.id] = name;
	grouped_.insert(imported_members.begin(), imported_members.end());
	++result_.statistics.objects_written;
	++result_.statistics.semantic_groups_written;
    }

    for (const DirectoryEntry &entry : document_.entities()) {
	if (entry.type != 408)
	    continue;
	const ParameterList *parameters = document_.parameters(entry.id);
	EntityId definition_id;
	if (!parameter_entity(parameters, 1, definition_id))
	    continue;
	const auto definition = objects_.find(definition_id);
	if (definition == objects_.end())
	    continue;

	double scale_factor = 1.0;
	Point3 translation = {0.0, 0.0, 0.0};
	double value = 0.0;
	for (size_t coordinate = 0; coordinate < translation.size(); ++coordinate)
	    if (parameter_real(parameters, coordinate + 2, value))
		translation[coordinate] = value;
	if (parameter_real(parameters, 5, value))
	    scale_factor = value;
	if (!std::isfinite(scale_factor) ||
		std::fabs(scale_factor) <= MIN_VECTOR_LENGTH) {
	    diagnose(Severity::Warning, "subfigure_instance_parameters",
		"Subfigure Instance has an invalid placement", &entry);
	    ++result_.statistics.omitted;
	    continue;
	}

	Matrix placement;
	for (size_t axis = 0; axis < 3; ++axis) {
	    placement.m[axis][axis] = scale_factor;
	    placement.m[axis][3] = translation[axis];
	}
	placement = multiply(transform(entry), placement);
	mat_t member_matrix;
	MAT_IDN(member_matrix);
	for (size_t row = 0; row < 3; ++row)
	    for (size_t column = 0; column < 4; ++column)
		member_matrix[row * 4 + column] = column == 3 ?
		    placement.m[row][column] * unit_to_mm_ :
		    placement.m[row][column];

	struct wmember members;
	BU_LIST_INIT(&members.l);
	struct wmember *definition_member = mk_addmember(
	    definition->second.c_str(), &members.l, nullptr, WMOP_UNION);
	if (definition_member == WMEMBER_NULL) {
	    diagnose(Severity::Error, "subfigure_instance_member",
		"failed to add a Subfigure Definition to its instance", &entry);
	    return false;
	}
	MAT_COPY(definition_member->wm_mat, member_matrix);

	int associated_count = 0;
	if (parameter_integer(parameters, 6, associated_count) &&
		associated_count >= 0 && associated_count <= MAX_ENTITY_LIST_COUNT) {
	    for (int i = 0; i < associated_count; ++i) {
		EntityId associated_id;
		if (!parameter_entity(parameters, static_cast<size_t>(i + 7),
			associated_id))
		    break;
		const auto associated = objects_.find(associated_id);
		if (associated == objects_.end())
		    continue;
		if (mk_addmember(associated->second.c_str(), &members.l, nullptr,
			WMOP_UNION) == WMEMBER_NULL) {
		    diagnose(Severity::Error, "subfigure_instance_member",
			"failed to add associated annotation content to an instance",
			&entry);
		    return false;
		}
		grouped_.insert(associated_id);
	    }
	}

	const DirectoryEntry *definition_entry = document_.entity(definition_id);
	std::string source_name;
	if (definition_entry)
	    parameter_string(document_.parameters(definition_id), 2, source_name);
	if (source_name.empty())
	    source_name = "iges_subfigure";
	source_name += ".instance_D" + std::to_string(entry.id.value());
	const std::string name = unique_name(entry, source_name, ".annot_instance");
	const int write_status = mk_lfcomb(wdbp_, name.c_str(), &members, 0)
	if (write_status < 0) {
	    diagnose(Severity::Error, "subfigure_instance_write",
		"failed to write an annotation Subfigure Instance", &entry);
	    return false;
	}
	write_entity_attributes(entry, name);
	db5_update_attribute(name.c_str(), "iges.semantic",
	    "subfigure_instance", wdbp_->dbip);
	const std::string definition_entity =
	    std::to_string(definition_id.value());
	db5_update_attribute(name.c_str(), "iges.definition",
	    definition_entity.c_str(), wdbp_->dbip);
	objects_[entry.id] = name;
	grouped_.insert(definition_id);
	++result_.statistics.objects_written;
	++result_.statistics.semantic_groups_written;
    }
    return true;
}

bool
Translator::write_root(const std::vector<std::string> &members)
{
    if (members.empty())
	return false;
    std::string stem = sanitize_name(options_.root_name);
    if (stem.empty())
	stem = "iges_drawing";
    std::string name = stem;
    size_t serial = 1;
    while (db_lookup(wdbp_->dbip, name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	name = stem + "." + std::to_string(serial++);

    struct wmember head;
    BU_LIST_INIT(&head.l);
    for (const std::string &member : members)
	if (mk_addmember(member.c_str(), &head.l, nullptr, WMOP_UNION) ==
		WMEMBER_NULL) {
	    diagnose(Severity::Error, "root_member",
		"failed to add an object to the drawing group");
	    return false;
	}
    const int write_status = mk_lfcomb(wdbp_, name.c_str(), &head, 0)
    if (write_status < 0) {
	diagnose(Severity::Error, "root_write",
	    "failed to write the IGES drawing group");
	return false;
    }
    db5_update_attribute(name.c_str(), "importer", "iges-g", wdbp_->dbip);
    db5_update_attribute(name.c_str(), "source_format", "iges", wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.semantic", "drawing",
	wdbp_->dbip);
    ++result_.statistics.objects_written;
    return true;
}

ImportResult
Translator::run()
{
    if (!wdbp_ || !wdbp_->dbip) {
	diagnose(Severity::Fatal, "output_database",
	    "no writable BRL-CAD database was supplied");
	return result_;
    }
    if (!document_.valid()) {
	diagnose(Severity::Fatal, "invalid_document",
	    "IGES structural validation failed");
	return result_;
    }
    for (const Diagnostic &diagnostic : document_.diagnostics()) {
	const bool repair = diagnostic.code == "inferred_parameter_count" ||
	    diagnostic.code == "parameter_owner_repaired" ||
	    diagnostic.code == "record_data_too_long";
	if (repair)
	    ++result_.statistics.repairs;
	if (repair && (options_.repair == RepairMode::None || options_.exact ||
		options_.strict)) {
	    diagnose(Severity::Error, "repair_disallowed",
		"strict import does not permit: " + diagnostic.message);
	}
    }
    if (!result_.diagnostics.empty())
	return result_;

    std::set<EntityId> referenced;
    for (const DirectoryEntry &entry : document_.entities()) {
	if (!is_dimension_type(entry.type) && entry.type != 308 &&
		entry.type != 408)
	    continue;
	const ParameterList *parameters = document_.parameters(entry.id);
	if (!parameters)
	    continue;
	for (size_t i = 1; i < parameters->values.size(); ++i) {
	    EntityId reference;
	    if (parameter_entity(parameters, i, reference)) {
		const DirectoryEntry *target = document_.entity(reference);
		if (target && is_drawable_type(target->type))
		    referenced.insert(reference);
	    }
	}
    }

    for (const DirectoryEntry &entry : document_.entities()) {
	if (!is_drawable_type(entry.type))
	    continue;
	const int subordinate = (std::abs(entry.status) / 10000) % 100;
	if (subordinate != 0 && subordinate != 2 &&
		referenced.find(entry.id) == referenced.end())
	    continue;
	const char *suffix = entry.type == 116 ? ".datum" : ".annot";
	const std::string name = unique_name(entry, suffix);
	if (translate(entry, name))
	    objects_[entry.id] = name;
	else
	    ++result_.statistics.omitted;
    }
    if (!write_groups())
	return result_;
    if (!write_subfigures())
	return result_;

    std::vector<std::string> root_members = groups_;
    for (const auto &object : objects_)
	if (grouped_.find(object.first) == grouped_.end())
	    root_members.push_back(object.second);
    if (!root_members.empty() && !write_root(root_members))
	return result_;

    const bool has_errors = std::any_of(result_.diagnostics.begin(),
	result_.diagnostics.end(), [](const ImportDiagnostic &diagnostic) {
	    return diagnostic.severity == Severity::Error ||
		diagnostic.severity == Severity::Fatal;
	});
    result_.success = !has_errors &&
	(result_.statistics.annotations_written > 0 ||
	 result_.statistics.datums_written > 0) &&
	(!options_.strict || result_.statistics.omitted == 0);
    return result_;
}

ImportResult
import_annotations(const Document &document, struct rt_wdb *wdbp,
    const ImportOptions &options)
{
    Translator translator(document, wdbp, options);
    return translator.run();
}

namespace {

const char *
severity_name(Severity severity)
{
    switch (severity) {
	case Severity::Information: return "information";
	case Severity::Warning: return "warning";
	case Severity::Error: return "error";
	case Severity::Fatal: return "fatal";
    }
    return "error";
}

} /* namespace */

bool
write_import_report(const std::string &path, const Document &document,
    const ImportOptions &options, const ImportResult &result)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
	return false;

    output << "{\n"
	<< "  \"format\": \"iges\",\n"
	<< "  \"source\": \"" << json_escape(document.source_name()) << "\",\n"
	<< "  \"success\": " << (result.success ? "true" : "false") << ",\n"
	<< "  \"options\": {\"repair\": \""
	<< (options.repair == RepairMode::Safe ? "safe" : "none")
	<< "\", \"exact\": " << (options.exact ? "true" : "false")
	<< ", \"strict\": " << (options.strict ? "true" : "false")
	<< ", \"project_drawings\": "
	<< (options.project_drawings ? "true" : "false") << "},\n"
	<< "  \"statistics\": {\"entities_read\": "
	<< result.statistics.entities_read << ", \"objects_written\": "
	<< result.statistics.objects_written << ", \"annotations_written\": "
	<< result.statistics.annotations_written
	<< ", \"datums_written\": "
	<< result.statistics.datums_written
	<< ", \"semantic_groups_written\": "
	<< result.statistics.semantic_groups_written << ", \"omitted\": "
	<< result.statistics.omitted << ", \"repairs\": "
	<< result.statistics.repairs << "},\n"
	<< "  \"diagnostics\": [";

    bool first = true;
    const auto write_diagnostic = [&](Severity severity, const std::string &code,
	const std::string &message, int64_t entity_id, int entity_type,
	size_t record, size_t column) {
	if (!first)
	    output << ',';
	first = false;
	output << "\n    {\"severity\": \"" << severity_name(severity)
	    << "\", \"code\": \"" << json_escape(code)
	    << "\", \"message\": \"" << json_escape(message) << '"';
	if (entity_id)
	    output << ", \"entity\": " << entity_id;
	if (entity_type)
	    output << ", \"entity_type\": " << entity_type;
	if (record)
	    output << ", \"record\": " << record;
	if (column)
	    output << ", \"column\": " << column;
	output << '}';
    };
    for (const Diagnostic &diagnostic : document.diagnostics())
	write_diagnostic(diagnostic.severity, diagnostic.code, diagnostic.message,
	    diagnostic.entity_id, diagnostic.entity_type,
	    diagnostic.location.record, diagnostic.location.column);
    for (const ImportDiagnostic &diagnostic : result.diagnostics)
	write_diagnostic(diagnostic.severity, diagnostic.code, diagnostic.message,
	    diagnostic.entity_id, diagnostic.entity_type, 0, 0);
    if (!first)
	output << '\n';
    output << "  ]\n}\n";
    return output.good();
}

} /* namespace iges */
} /* namespace brlcad */

extern "C" int
iges_import_annotations(const char *path, struct rt_wdb *wdbp,
    int project_to_xy, int exact, int strict, const char *repair_mode,
    const char *root_name, const char *report_path)
{
    if (!path || !wdbp)
	return -1;
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_file(path);
    brlcad::iges::ImportOptions options;
    options.project_drawings = project_to_xy != 0;
    options.exact = exact != 0;
    options.strict = strict != 0;
    if (repair_mode && BU_STR_EQUAL(repair_mode, "none"))
	options.repair = brlcad::iges::RepairMode::None;
    if (root_name && root_name[0] != '\0')
	options.root_name = root_name;
    const brlcad::iges::ImportResult result =
	brlcad::iges::import_annotations(document, wdbp, options);

    struct LogSummary {
	size_t count = 0;
	std::string message;
	int64_t entity_id = 0;
	size_t record = 0;
    };
    std::map<std::string, LogSummary> summaries;
    for (const brlcad::iges::Diagnostic &diagnostic : document.diagnostics()) {
	if (diagnostic.severity == brlcad::iges::Severity::Information)
	    continue;
	LogSummary &summary = summaries[diagnostic.code];
	++summary.count;
	if (summary.message.empty()) {
	    summary.message = diagnostic.message;
	    summary.entity_id = diagnostic.entity_id;
	    summary.record = diagnostic.location.record;
	}
    }
    for (const brlcad::iges::ImportDiagnostic &diagnostic : result.diagnostics) {
	LogSummary &summary = summaries[diagnostic.code];
	++summary.count;
	if (summary.message.empty()) {
	    summary.message = diagnostic.message;
	    summary.entity_id = diagnostic.entity_id;
	}
    }
    for (const auto &item : summaries) {
	const LogSummary &summary = item.second;
	bu_log("IGES %s%s%s%s%s%s: %s\n", item.first.c_str(),
	    summary.count > 1 ? " (" : "",
	    summary.count > 1 ? std::to_string(summary.count).c_str() : "",
	    summary.count > 1 ? " occurrences)" : "",
	    summary.record ? " first at record " :
		(summary.entity_id ? " first for D" : ""),
	    summary.record ? std::to_string(summary.record).c_str() :
		(summary.entity_id ? std::to_string(summary.entity_id).c_str() : ""),
	    summary.message.c_str());
    }
    if (report_path && report_path[0] != '\0' &&
	    !brlcad::iges::write_import_report(report_path, document, options, result)) {
	bu_log("IGES: unable to write import report %s\n", report_path);
	return -1;
    }
    if (result.success)
	return 1;
    const bool document_error = !document.valid();
    const bool import_error = std::any_of(result.diagnostics.begin(),
	result.diagnostics.end(), [](const brlcad::iges::ImportDiagnostic &diagnostic) {
	    return diagnostic.severity == brlcad::iges::Severity::Error ||
		diagnostic.severity == brlcad::iges::Severity::Fatal;
	});
    return document_error || import_error ||
	result.statistics.annotations_written || result.statistics.datums_written ?
	-1 : 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
