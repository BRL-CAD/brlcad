/*             I G E S _ B R E P _ I M P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "iges_brep_import.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "brep.h"
#include "bu/log.h"
#include "bu/str.h"
#include "raytrace.h"
#include "wdb.h"

namespace brlcad {
namespace iges {
namespace brep_import_detail {

constexpr size_t GLOBAL_MODEL_SCALE = 12;
constexpr size_t GLOBAL_UNITS_FLAG = 13;
constexpr size_t GLOBAL_MINIMUM_RESOLUTION = 18;
constexpr double DEFAULT_MODEL_SCALE = 1.0;
constexpr double DEFAULT_UNIT_TO_MM = 1.0;
constexpr double DEFAULT_TOPOLOGY_TOLERANCE_MM = 1.0e-6;
constexpr double DEGENERATE_DOMAIN_TOLERANCE = 1.0e-12;
constexpr int MAX_ENTITY_LIST_COUNT = 10000000;
constexpr double SINGULAR_CURVE_SAMPLES[] = {0.0, 0.5, 1.0};

using Point3 = std::array<double, 3>;

struct Matrix {
    double m[4][4] = {
	{1.0, 0.0, 0.0, 0.0},
	{0.0, 1.0, 0.0, 0.0},
	{0.0, 0.0, 1.0, 0.0},
	{0.0, 0.0, 0.0, 1.0}
    };
};

struct VertexKey {
    EntityId list;
    int index = 0;

    bool operator<(const VertexKey &other) const
    {
	if (list == other.list)
	    return index < other.index;
	return list < other.list;
    }
};

struct EdgeKey {
    EntityId list;
    int index = 0;

    bool operator<(const EdgeKey &other) const
    {
	if (list == other.list)
	    return index < other.index;
	return list < other.list;
    }
};

struct EdgeRecord {
    EntityId curve;
    VertexKey start;
    VertexKey end;
};

struct EdgeUse {
    bool vertex_use = false;
    EdgeKey edge;
    VertexKey vertex;
    bool same_direction = true;
    std::vector<EntityId> parameter_curves;
};

struct LoopRecord {
    EntityId source;
    std::vector<EdgeUse> uses;
};

double
global_real(const GlobalSection &global, size_t index, double fallback)
{
    if (index >= global.parameters.size())
	return fallback;
    std::string text = global.parameters[index];
    std::replace(text.begin(), text.end(), 'D', 'E');
    std::replace(text.begin(), text.end(), 'd', 'e');
    char *end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    return end == text.c_str() + text.size() && std::isfinite(value) ?
	value : fallback;
}

int
global_integer(const GlobalSection &global, size_t index, int fallback)
{
    const double value = global_real(global, index, fallback);
    return value >= std::numeric_limits<int>::min() &&
	value <= std::numeric_limits<int>::max() ?
	static_cast<int>(value) : fallback;
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
    return conversion / (model_scale > 0.0 ? model_scale :
	DEFAULT_MODEL_SCALE);
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

Matrix
multiply(const Matrix &left, const Matrix &right)
{
    Matrix result;
    for (size_t row = 0; row < 4; ++row)
	for (size_t column = 0; column < 4; ++column) {
	    result.m[row][column] = 0.0;
	    for (size_t inner = 0; inner < 4; ++inner)
		result.m[row][column] += left.m[row][inner] *
		    right.m[inner][column];
	}
    return result;
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

Point3
cross(const Point3 &left, const Point3 &right)
{
    return {
	left[1] * right[2] - left[2] * right[1],
	left[2] * right[0] - left[0] * right[2],
	left[0] * right[1] - left[1] * right[0]
    };
}

double
length(const Point3 &value)
{
    return std::sqrt(value[0] * value[0] + value[1] * value[1] +
	value[2] * value[2]);
}

bool
normalize(Point3 &value)
{
    const double magnitude = length(value);
    if (!std::isfinite(magnitude) || magnitude <= DEGENERATE_DOMAIN_TOLERANCE)
	return false;
    for (double &coordinate : value)
	coordinate /= magnitude;
    return true;
}

bool
parameter_string(const ParameterList *parameters, size_t index,
    std::string &value)
{
    return parameters && index < parameters->values.size() &&
	parameters->values[index].string(value);
}

std::string
source_name(const Document &document, const DirectoryEntry &entry)
{
    std::string source = entry.label;
    const ParameterList *parameters = document.parameters(entry.id);
    int void_count = 0;
    if (entry.type == 186 && parameter_integer(parameters, 3, void_count) &&
	    void_count >= 0 && void_count <= MAX_ENTITY_LIST_COUNT) {
	size_t parameter = 4 + static_cast<size_t>(void_count) * 2;
	int associativity_count = 0;
	if (parameter_integer(parameters, parameter++, associativity_count) &&
		associativity_count >= 0 &&
		associativity_count <= MAX_ENTITY_LIST_COUNT) {
	    parameter += static_cast<size_t>(associativity_count);
	    int property_count = 0;
	    if (parameter_integer(parameters, parameter++, property_count) &&
		    property_count >= 0 && property_count <= MAX_ENTITY_LIST_COUNT) {
		for (int i = 0; i < property_count; ++i) {
		    EntityId property_id;
		    if (!parameter_entity(parameters, parameter++, property_id))
			break;
		    const DirectoryEntry *property = document.entity(property_id);
		    const ParameterList *property_parameters = property ?
			document.parameters(property_id) : nullptr;
		    int value_count = 0;
		    std::string candidate;
		    if (property && property->type == 406 && property->form == 15 &&
			    parameter_integer(property_parameters, 1, value_count) &&
			    value_count == 1 &&
			    parameter_string(property_parameters, 2, candidate) &&
			    !candidate.empty()) {
			source = candidate;
			break;
		    }
		}
	    }
	}
    }

    std::string result;
    for (unsigned char character : source) {
	if ((character >= 'a' && character <= 'z') ||
		(character >= 'A' && character <= 'Z') ||
		(character >= '0' && character <= '9') || character == '_' ||
		character == '-' || character == '.')
	    result.push_back(static_cast<char>(character));
	else
	    result.push_back('_');
    }
    if (result.empty())
	result = "iges_brep_D" + std::to_string(entry.id.value());
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

class Importer;

class SolidBuilder {
public:
    SolidBuilder(Importer &importer, const DirectoryEntry &solid);

    std::unique_ptr<ON_Brep> build();
    std::unique_ptr<ON_NurbsCurve> nurbs_curve(const DirectoryEntry &entry,
	bool model_space);
    std::unique_ptr<ON_NurbsSurface> nurbs_surface(
	const DirectoryEntry &entry);

private:
    bool add_shell(EntityId id, bool same_direction);
    bool add_face(EntityId id, bool same_direction,
	bool shell_same_direction);
    bool parse_loop(EntityId id, LoopRecord &loop);
    bool edge_record(const EdgeKey &key, EdgeRecord &record);
    bool vertex_point(const VertexKey &key, ON_3dPoint &point);
    int vertex_index(const VertexKey &key);
    int edge_index(const EdgeKey &key);
    bool edge_use_points(const EdgeUse &use, ON_3dPoint &start,
	ON_3dPoint &end);
    std::unique_ptr<ON_Curve> edge_curve(EntityId id,
	const ON_3dPoint &start, const ON_3dPoint &end);
    std::unique_ptr<ON_Curve> trim_curve(const EdgeUse &use,
	const ON_Plane *parameter_plane, const ON_3dPoint &start,
	const ON_3dPoint &end);
    std::unique_ptr<ON_Surface> face_surface(EntityId id,
	const std::vector<LoopRecord> &loops, ON_Plane &parameter_plane,
	bool &has_parameter_plane);
    std::unique_ptr<ON_PlaneSurface> plane_surface(EntityId id,
	const std::vector<LoopRecord> &loops, ON_Plane &plane);

    Importer &importer_;
    const DirectoryEntry &solid_;
    Matrix solid_transform_;
    std::unique_ptr<ON_Brep> brep_;
    std::map<VertexKey, int> vertices_;
    std::map<EdgeKey, int> edges_;

    friend class TrimmedSurfaceBuilder;
};

class TrimmedSurfaceBuilder {
public:
    TrimmedSurfaceBuilder(Importer &importer,
	const std::vector<const DirectoryEntry *> &faces);

    std::unique_ptr<ON_Brep> build(brep_assembly_result &assembly);

private:
    struct CurvePair {
	std::unique_ptr<ON_Curve> parameter;
	std::unique_ptr<ON_Curve> model;
	bool singular = false;
    };

    bool append_curve_entities(EntityId id, std::vector<EntityId> &curves,
	std::set<EntityId> &active);
    std::unique_ptr<ON_Curve> curve(SolidBuilder &geometry, EntityId id,
	bool model_space);
    bool curve_pairs(SolidBuilder &geometry, EntityId boundary,
	std::vector<CurvePair> &pairs);
    bool add_loop(ON_BrepFace &face, ON_BrepLoop::TYPE type,
	std::vector<CurvePair> &pairs, const DirectoryEntry &source);
    bool add_face(const DirectoryEntry &entry);

    Importer &importer_;
    const std::vector<const DirectoryEntry *> &faces_;
    std::unique_ptr<ON_Brep> brep_;
};

class Importer {
public:
    Importer(const Document &document, struct rt_wdb *wdbp,
	const ImportOptions &options) : document_(document), wdbp_(wdbp),
	options_(options), unit_to_mm_(unit_scale(document.global()))
    {
	result_.statistics.entities_read = document.entities().size();
	const double source_resolution = global_real(document.global(),
	    GLOBAL_MINIMUM_RESOLUTION, 0.0) * unit_to_mm_;
	tolerance_ = source_resolution > 0.0 ? source_resolution :
	    DEFAULT_TOPOLOGY_TOLERANCE_MM;
    }

    BrepImportResult run();
    const Document &document() const { return document_; }
    double tolerance() const { return tolerance_; }
    Matrix transform(EntityId id);
    Point3 model_point(const DirectoryEntry &entry, const Point3 &point,
	const Matrix &parent);
    Point3 model_vector(const DirectoryEntry &entry, const Point3 &vector,
	const Matrix &parent);
    void diagnose(Severity severity, const char *code,
	const std::string &message, const DirectoryEntry *entry = nullptr);

private:
    Matrix transform(EntityId id, std::set<EntityId> &active);
    std::string unique_name(const DirectoryEntry &entry) const;

    const Document &document_;
    struct rt_wdb *wdbp_ = nullptr;
    ImportOptions options_;
    BrepImportResult result_;
    double unit_to_mm_ = DEFAULT_UNIT_TO_MM;
    double tolerance_ = DEFAULT_TOPOLOGY_TOLERANCE_MM;
    std::map<EntityId, Matrix> transforms_;

    friend class SolidBuilder;
};

SolidBuilder::SolidBuilder(Importer &importer, const DirectoryEntry &solid) :
    importer_(importer), solid_(solid),
    solid_transform_(importer.transform(solid.transform)),
    brep_(ON_Brep::New())
{
}

void
Importer::diagnose(Severity severity, const char *code,
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
Importer::transform(EntityId id, std::set<EntityId> &active)
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

    Matrix result;
    const DirectoryEntry *entry = document_.entity(id);
    const ParameterList *parameters = entry ? document_.parameters(id) : nullptr;
    bool valid = entry && parameters;
    if (valid && entry->type == 124) {
	for (size_t row = 0; row < 3; ++row)
	    for (size_t column = 0; column < 4; ++column)
		valid = parameter_real(parameters, 1 + row * 4 + column,
		    result.m[row][column]) && valid;
    } else if (valid && entry->type == 700) {
	for (size_t row = 0; row < 4; ++row)
	    for (size_t column = 0; column < 4; ++column)
		valid = parameter_real(parameters, 1 + row * 4 + column,
		    result.m[row][column]) && valid;
    } else {
	valid = false;
    }
    if (!valid) {
	diagnose(Severity::Error, "invalid_transform_parameters",
	    "transformation matrix has invalid or missing parameters", entry);
	result = Matrix();
    } else if (entry && !entry->transform.empty()) {
	result = multiply(transform(entry->transform, active), result);
    }
    active.erase(id);
    transforms_[id] = result;
    return result;
}

Matrix
Importer::transform(EntityId id)
{
    std::set<EntityId> active;
    return transform(id, active);
}

Point3
Importer::model_point(const DirectoryEntry &entry, const Point3 &point,
    const Matrix &parent)
{
    Point3 result = apply_point(multiply(parent, transform(entry.transform)),
	point);
    for (double &coordinate : result)
	coordinate *= unit_to_mm_;
    return result;
}

Point3
Importer::model_vector(const DirectoryEntry &entry, const Point3 &vector,
    const Matrix &parent)
{
    Point3 result = apply_vector(multiply(parent, transform(entry.transform)),
	vector);
    for (double &coordinate : result)
	coordinate *= unit_to_mm_;
    return result;
}

bool
SolidBuilder::vertex_point(const VertexKey &key, ON_3dPoint &point)
{
    const DirectoryEntry *entry = importer_.document().entity(key.list);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(key.list) : nullptr;
    int count = 0;
    if (!entry || entry->type != 502 ||
	    !parameter_integer(parameters, 1, count) || key.index < 1 ||
	    key.index > count || count > MAX_ENTITY_LIST_COUNT) {
	importer_.diagnose(Severity::Warning, "vertex_list_reference",
	    "B-Rep topology references an invalid Vertex List entry", entry);
	return false;
    }
    Point3 source;
    const size_t offset = 2 + static_cast<size_t>(key.index - 1) * 3;
    for (size_t coordinate = 0; coordinate < 3; ++coordinate)
	if (!parameter_real(parameters, offset + coordinate, source[coordinate])) {
	    importer_.diagnose(Severity::Warning, "vertex_list_parameters",
		"Vertex List contains invalid coordinates", entry);
	    return false;
	}
    const Point3 model = importer_.model_point(*entry, source, solid_transform_);
    point.Set(model[0], model[1], model[2]);
    return point.IsValid();
}

int
SolidBuilder::vertex_index(const VertexKey &key)
{
    const auto found = vertices_.find(key);
    if (found != vertices_.end())
	return found->second;
    ON_3dPoint point;
    if (!vertex_point(key, point))
	return -1;
    ON_BrepVertex &vertex = brep_->NewVertex(point, importer_.tolerance());
    vertices_[key] = vertex.m_vertex_index;
    return vertex.m_vertex_index;
}

bool
SolidBuilder::edge_record(const EdgeKey &key, EdgeRecord &record)
{
    const DirectoryEntry *entry = importer_.document().entity(key.list);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(key.list) : nullptr;
    int count = 0;
    if (!entry || entry->type != 504 ||
	    !parameter_integer(parameters, 1, count) || key.index < 1 ||
	    key.index > count || count > MAX_ENTITY_LIST_COUNT) {
	importer_.diagnose(Severity::Warning, "edge_list_reference",
	    "B-Rep topology references an invalid Edge List entry", entry);
	return false;
    }
    const size_t offset = 2 + static_cast<size_t>(key.index - 1) * 5;
    EntityId start_list;
    EntityId end_list;
    int start_index = 0;
    int end_index = 0;
    if (!parameter_entity(parameters, offset, record.curve) ||
	    !parameter_entity(parameters, offset + 1, start_list) ||
	    !parameter_integer(parameters, offset + 2, start_index) ||
	    !parameter_entity(parameters, offset + 3, end_list) ||
	    !parameter_integer(parameters, offset + 4, end_index)) {
	importer_.diagnose(Severity::Warning, "edge_list_parameters",
	    "Edge List contains invalid topology references", entry);
	return false;
    }
    record.start = {start_list, start_index};
    record.end = {end_list, end_index};
    return true;
}

std::unique_ptr<ON_NurbsCurve>
SolidBuilder::nurbs_curve(const DirectoryEntry &entry, bool model_space)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    int upper_index = 0;
    int degree = 0;
    int polynomial = 0;
    if (!parameter_integer(parameters, 1, upper_index) ||
	    !parameter_integer(parameters, 2, degree) ||
	    !parameter_integer(parameters, 5, polynomial) || upper_index < 1 ||
	    upper_index > MAX_ENTITY_LIST_COUNT || degree < 1 ||
	    degree > upper_index)
	return nullptr;
    const int control_count = upper_index + 1;
    const int order = degree + 1;
    const int full_knot_count = control_count + order;
    const bool rational = polynomial == 0;
    std::unique_ptr<ON_NurbsCurve> curve(ON_NurbsCurve::New(
	model_space ? 3 : 2, rational, order, control_count));
    if (!curve)
	return nullptr;

    size_t parameter = 7;
    std::vector<double> knots(static_cast<size_t>(full_knot_count));
    for (double &knot : knots)
	if (!parameter_real(parameters, parameter++, knot))
	    return nullptr;
    for (int i = 0; i < curve->KnotCount(); ++i)
	if (!curve->SetKnot(i, knots[static_cast<size_t>(i + 1)]))
	    return nullptr;

    std::vector<double> weights(static_cast<size_t>(control_count));
    for (double &weight : weights)
	if (!parameter_real(parameters, parameter++, weight) || weight <= 0.0)
	    return nullptr;
    for (int i = 0; i < control_count; ++i) {
	Point3 source;
	for (double &coordinate : source)
	    if (!parameter_real(parameters, parameter++, coordinate))
		return nullptr;
	Point3 point = source;
	if (model_space)
	    point = importer_.model_point(entry, source, solid_transform_);
	const double weight = weights[static_cast<size_t>(i)];
	if (model_space && rational) {
	    double homogeneous[4] = {
		point[0] * weight, point[1] * weight,
		point[2] * weight, weight
	    };
	    curve->SetCV(i, ON::homogeneous_rational, homogeneous);
	} else if (model_space) {
	    curve->SetCV(i, ON_3dPoint(point.data()));
	} else if (rational) {
	    double homogeneous[3] = {
		point[0] * weight, point[1] * weight, weight
	    };
	    curve->SetCV(i, ON::homogeneous_rational, homogeneous);
	} else {
	    double coordinates[2] = {point[0], point[1]};
	    curve->SetCV(i, ON::not_rational, coordinates);
	}
    }
    double domain_start = 0.0;
    double domain_end = 0.0;
    if (!parameter_real(parameters, parameter++, domain_start) ||
	    !parameter_real(parameters, parameter++, domain_end) ||
	    domain_end <= domain_start ||
	    !curve->Trim(ON_Interval(domain_start, domain_end)))
	return nullptr;
    return curve->IsValid() ? std::move(curve) : nullptr;
}

std::unique_ptr<ON_Curve>
SolidBuilder::edge_curve(EntityId id, const ON_3dPoint &start,
    const ON_3dPoint &end)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(id) : nullptr;
    if (!entry) {
	importer_.diagnose(Severity::Warning, "edge_curve_reference",
	    "Edge List references a missing curve entity");
	return nullptr;
    }
    if (entry->type == 126) {
	std::unique_ptr<ON_NurbsCurve> curve = nurbs_curve(*entry, true);
	if (!curve) {
	    importer_.diagnose(Severity::Warning, "nurbs_curve_parameters",
		"B-Rep B-Spline edge has invalid parameters", entry);
	    return nullptr;
	}
	const ON_3dPoint curve_start = curve->PointAtStart();
	const ON_3dPoint curve_end = curve->PointAtEnd();
	const double forward = curve_start.DistanceTo(start) +
	    curve_end.DistanceTo(end);
	const double reverse = curve_start.DistanceTo(end) +
	    curve_end.DistanceTo(start);
	if (!curve_start.IsValid() || !curve_end.IsValid() ||
		!std::isfinite(forward) || !std::isfinite(reverse)) {
	    importer_.diagnose(Severity::Warning, "invalid_edge_curve",
		"B-Rep B-Spline edge has invalid endpoints", entry);
	    return nullptr;
	}
	if (reverse < forward)
	    curve->Reverse();
	return std::unique_ptr<ON_Curve>(curve.release());
    }
    if (entry->type != 110) {
	importer_.diagnose(Severity::Warning, "unsupported_edge_curve",
	    "direct manifold import requires Line or B-Spline edge geometry",
	    entry);
	return nullptr;
    }
    Point3 endpoints[2];
    for (size_t point_index = 0; point_index < 2; ++point_index)
	for (size_t coordinate = 0; coordinate < 3; ++coordinate)
	    if (!parameter_real(parameters, 1 + point_index * 3 + coordinate,
		    endpoints[point_index][coordinate])) {
		importer_.diagnose(Severity::Warning, "line_parameters",
		    "B-Rep Line edge has invalid endpoints", entry);
		return nullptr;
	    }
    const Point3 source_start = importer_.model_point(*entry, endpoints[0],
	solid_transform_);
    const Point3 source_end = importer_.model_point(*entry, endpoints[1],
	solid_transform_);
    const ON_3dPoint curve_start(source_start.data());
    const ON_3dPoint curve_end(source_end.data());
    const double forward = curve_start.DistanceTo(start) +
	curve_end.DistanceTo(end);
    const double reverse = curve_start.DistanceTo(end) +
	curve_end.DistanceTo(start);
    if (std::min(forward, reverse) > 2.0 * importer_.tolerance()) {
	importer_.diagnose(Severity::Warning, "edge_endpoint_mismatch",
	    "Line edge does not agree with its authored topology vertices",
	    entry);
	return nullptr;
    }
    std::unique_ptr<ON_Curve> curve(new ON_LineCurve(start, end));
    curve->SetDomain(0.0, 1.0);
    return curve;
}

int
SolidBuilder::edge_index(const EdgeKey &key)
{
    const auto found = edges_.find(key);
    if (found != edges_.end())
	return found->second;
    EdgeRecord record;
    if (!edge_record(key, record))
	return -1;
    const int start_index = vertex_index(record.start);
    const int end_index = vertex_index(record.end);
    if (start_index < 0 || end_index < 0 || start_index == end_index)
	return -1;
    const ON_3dPoint start = brep_->m_V[start_index].Point();
    const ON_3dPoint end = brep_->m_V[end_index].Point();
    std::unique_ptr<ON_Curve> curve = edge_curve(record.curve, start, end);
    if (!curve)
	return -1;
    const double endpoint_mismatch = std::max(
	curve->PointAtStart().DistanceTo(start),
	curve->PointAtEnd().DistanceTo(end));
    if (!std::isfinite(endpoint_mismatch))
	return -1;
    const int curve_index = brep_->AddEdgeCurve(curve.release());
    ON_BrepEdge &edge = brep_->NewEdge(brep_->m_V[start_index],
	brep_->m_V[end_index], curve_index);
    edge.m_tolerance = std::max(importer_.tolerance(), endpoint_mismatch);
    edges_[key] = edge.m_edge_index;
    return edge.m_edge_index;
}

bool
SolidBuilder::parse_loop(EntityId id, LoopRecord &loop)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(id) : nullptr;
    int count = 0;
    if (!entry || entry->type != 508 ||
	    !parameter_integer(parameters, 1, count) || count < 1 ||
	    count > MAX_ENTITY_LIST_COUNT) {
	importer_.diagnose(Severity::Warning, "loop_reference",
	    "Face references an invalid Loop entity", entry);
	return false;
    }
    loop.source = id;
    size_t parameter = 2;
    loop.uses.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
	int kind = 0;
	EntityId list;
	int index = 0;
	int orientation = 0;
	int parameter_curve_count = 0;
	if (!parameter_integer(parameters, parameter++, kind) ||
		!parameter_entity(parameters, parameter++, list) ||
		!parameter_integer(parameters, parameter++, index) ||
		!parameter_integer(parameters, parameter++, orientation) ||
		!parameter_integer(parameters, parameter++,
		    parameter_curve_count) || parameter_curve_count < 0 ||
		parameter_curve_count > MAX_ENTITY_LIST_COUNT) {
	    importer_.diagnose(Severity::Warning, "loop_parameters",
		"Loop contains invalid edge-use parameters", entry);
	    return false;
	}
	if (kind != 0 && kind != 1) {
	    importer_.diagnose(Severity::Warning, "loop_use_kind",
		"Loop contains an unknown edge-use kind", entry);
	    return false;
	}
	EdgeUse use;
	use.vertex_use = kind == 1;
	if (use.vertex_use)
	    use.vertex = {list, index};
	else
	    use.edge = {list, index};
	use.same_direction = orientation != 0;
	for (int curve_index = 0; curve_index < parameter_curve_count;
		++curve_index) {
	    int isoparametric = 0;
	    EntityId curve;
	    if (!parameter_integer(parameters, parameter++, isoparametric) ||
		    !parameter_entity(parameters, parameter++, curve)) {
		importer_.diagnose(Severity::Warning, "loop_parameter_curve",
		    "Loop contains an invalid parameter-curve reference", entry);
		return false;
	    }
	    use.parameter_curves.push_back(curve);
	}
	loop.uses.push_back(use);
    }
    return true;
}

bool
SolidBuilder::edge_use_points(const EdgeUse &use, ON_3dPoint &start,
    ON_3dPoint &end)
{
    if (use.vertex_use) {
	if (!vertex_point(use.vertex, start))
	    return false;
	end = start;
	return true;
    }
    EdgeRecord record;
    if (!edge_record(use.edge, record))
	return false;
    const VertexKey &start_key = use.same_direction ? record.start : record.end;
    const VertexKey &end_key = use.same_direction ? record.end : record.start;
    return vertex_point(start_key, start) && vertex_point(end_key, end);
}

std::unique_ptr<ON_PlaneSurface>
SolidBuilder::plane_surface(EntityId id,
    const std::vector<LoopRecord> &loops, ON_Plane &plane)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(id) : nullptr;
    EntityId point_id;
    EntityId normal_id;
    if (!entry || entry->type != 190 ||
	    !parameter_entity(parameters, 1, point_id) ||
	    !parameter_entity(parameters, 2, normal_id)) {
	importer_.diagnose(Severity::Warning, "unsupported_face_surface",
	    "direct manifold import currently requires Plane Surface faces",
	    entry);
	return nullptr;
    }
    const DirectoryEntry *point_entry = importer_.document().entity(point_id);
    const DirectoryEntry *normal_entry = importer_.document().entity(normal_id);
    const ParameterList *point_parameters = point_entry ?
	importer_.document().parameters(point_id) : nullptr;
    const ParameterList *normal_parameters = normal_entry ?
	importer_.document().parameters(normal_id) : nullptr;
    if (!point_entry || point_entry->type != 116 || !normal_entry ||
	    normal_entry->type != 123)
	return nullptr;
    Point3 source_origin;
    Point3 source_normal;
    for (size_t coordinate = 0; coordinate < 3; ++coordinate)
	if (!parameter_real(point_parameters, coordinate + 1,
		source_origin[coordinate]) ||
		!parameter_real(normal_parameters, coordinate + 1,
		    source_normal[coordinate]))
	    return nullptr;

    const Matrix surface_parent = multiply(solid_transform_,
	importer_.transform(entry->transform));
    Point3 origin = importer_.model_point(*point_entry, source_origin,
	surface_parent);
    Point3 normal = importer_.model_vector(*normal_entry, source_normal,
	surface_parent);
    if (!normalize(normal))
	return nullptr;
    const ON_Plane seed(ON_3dPoint(origin.data()), ON_3dVector(normal.data()));
    Point3 xaxis = {seed.xaxis.x, seed.xaxis.y, seed.xaxis.z};
    Point3 yaxis = cross(normal, xaxis);
    if (!normalize(xaxis) || !normalize(yaxis))
	return nullptr;
    plane = ON_Plane(ON_3dPoint(origin.data()), ON_3dVector(xaxis.data()),
	ON_3dVector(yaxis.data()));

    double u_min = std::numeric_limits<double>::max();
    double u_max = -std::numeric_limits<double>::max();
    double v_min = std::numeric_limits<double>::max();
    double v_max = -std::numeric_limits<double>::max();
    for (const LoopRecord &loop : loops)
	for (const EdgeUse &use : loop.uses) {
	    ON_3dPoint start;
	    ON_3dPoint end;
	    if (!edge_use_points(use, start, end))
		return nullptr;
	    const ON_3dPoint points[2] = {start, end};
	    for (const ON_3dPoint &point : points) {
		double u = 0.0;
		double v = 0.0;
		if (!plane.ClosestPointTo(point, &u, &v))
		    return nullptr;
		u_min = std::min(u_min, u);
		u_max = std::max(u_max, u);
		v_min = std::min(v_min, v);
		v_max = std::max(v_max, v);
	    }
	}
    if (u_max - u_min <= DEGENERATE_DOMAIN_TOLERANCE ||
	    v_max - v_min <= DEGENERATE_DOMAIN_TOLERANCE)
	return nullptr;
    std::unique_ptr<ON_PlaneSurface> surface(new ON_PlaneSurface(plane));
    if (!surface->SetExtents(0, ON_Interval(u_min, u_max), true) ||
	    !surface->SetExtents(1, ON_Interval(v_min, v_max), true))
	return nullptr;
    return surface;
}

std::unique_ptr<ON_NurbsSurface>
SolidBuilder::nurbs_surface(const DirectoryEntry &entry)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    int upper_u = 0;
    int upper_v = 0;
    int degree_u = 0;
    int degree_v = 0;
    int closed_u = 0;
    int closed_v = 0;
    int polynomial = 0;
    int periodic_u = 0;
    int periodic_v = 0;
    if (!parameter_integer(parameters, 1, upper_u) ||
	    !parameter_integer(parameters, 2, upper_v) ||
	    !parameter_integer(parameters, 3, degree_u) ||
	    !parameter_integer(parameters, 4, degree_v) ||
	    !parameter_integer(parameters, 5, closed_u) ||
	    !parameter_integer(parameters, 6, closed_v) ||
	    !parameter_integer(parameters, 7, polynomial) ||
	    !parameter_integer(parameters, 8, periodic_u) ||
	    !parameter_integer(parameters, 9, periodic_v) || upper_u < 1 ||
	    upper_v < 1 || upper_u > MAX_ENTITY_LIST_COUNT ||
	    upper_v > MAX_ENTITY_LIST_COUNT || degree_u < 1 || degree_v < 1 ||
	    degree_u > upper_u || degree_v > upper_v)
	return nullptr;
    const int controls_u = upper_u + 1;
    const int controls_v = upper_v + 1;
    if (static_cast<int64_t>(controls_u) * controls_v >
	MAX_ENTITY_LIST_COUNT)
	return nullptr;
    const int order_u = degree_u + 1;
    const int order_v = degree_v + 1;
    const bool rational = polynomial == 0;
    std::unique_ptr<ON_NurbsSurface> surface(ON_NurbsSurface::New(3,
	rational, order_u, order_v, controls_u, controls_v));
    if (!surface)
	return nullptr;

    size_t parameter = 10;
    std::vector<double> knots_u(static_cast<size_t>(controls_u + order_u));
    std::vector<double> knots_v(static_cast<size_t>(controls_v + order_v));
    for (double &knot : knots_u)
	if (!parameter_real(parameters, parameter++, knot))
	    return nullptr;
    for (double &knot : knots_v)
	if (!parameter_real(parameters, parameter++, knot))
	    return nullptr;
    for (int i = 0; i < surface->KnotCount(0); ++i)
	if (!surface->SetKnot(0, i, knots_u[static_cast<size_t>(i + 1)]))
	    return nullptr;
    for (int i = 0; i < surface->KnotCount(1); ++i)
	if (!surface->SetKnot(1, i, knots_v[static_cast<size_t>(i + 1)]))
	    return nullptr;

    const size_t control_count = static_cast<size_t>(controls_u) * controls_v;
    std::vector<double> weights(control_count);
    for (double &weight : weights)
	if (!parameter_real(parameters, parameter++, weight) || weight <= 0.0)
	    return nullptr;
    for (int v = 0; v < controls_v; ++v)
	for (int u = 0; u < controls_u; ++u) {
	    Point3 source;
	    for (double &coordinate : source)
		if (!parameter_real(parameters, parameter++, coordinate))
		    return nullptr;
	    const Point3 point = importer_.model_point(entry, source,
		solid_transform_);
	    const double weight = weights[static_cast<size_t>(v) *
		controls_u + u];
	    if (rational)
		surface->SetCV(u, v, ON_4dPoint(point[0] * weight,
		    point[1] * weight, point[2] * weight, weight));
	    else
		surface->SetCV(u, v, ON_3dPoint(point.data()));
	}
    double domain_u_start = 0.0;
    double domain_u_end = 0.0;
    double domain_v_start = 0.0;
    double domain_v_end = 0.0;
    if (!parameter_real(parameters, parameter++, domain_u_start) ||
	    !parameter_real(parameters, parameter++, domain_u_end) ||
	    !parameter_real(parameters, parameter++, domain_v_start) ||
	    !parameter_real(parameters, parameter++, domain_v_end) ||
	    domain_u_end <= domain_u_start || domain_v_end <= domain_v_start)
	return nullptr;
    if (!surface->Trim(0, ON_Interval(domain_u_start, domain_u_end)) ||
	    !surface->Trim(1, ON_Interval(domain_v_start, domain_v_end)) ||
	    !surface->IsValid())
	return nullptr;
    return surface;
}

std::unique_ptr<ON_Surface>
SolidBuilder::face_surface(EntityId id,
    const std::vector<LoopRecord> &loops, ON_Plane &parameter_plane,
    bool &has_parameter_plane)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    if (!entry)
	return nullptr;
    if (entry->type == 190) {
	std::unique_ptr<ON_PlaneSurface> surface =
	    plane_surface(id, loops, parameter_plane);
	has_parameter_plane = surface != nullptr;
	return std::unique_ptr<ON_Surface>(surface.release());
    }
    if (entry->type == 128) {
	std::unique_ptr<ON_NurbsSurface> surface = nurbs_surface(*entry);
	has_parameter_plane = false;
	return std::unique_ptr<ON_Surface>(surface.release());
    }
    importer_.diagnose(Severity::Warning, "unsupported_face_surface",
	"direct manifold import requires Plane or B-Spline face geometry",
	entry);
    return nullptr;
}

std::unique_ptr<ON_Curve>
SolidBuilder::trim_curve(const EdgeUse &use,
    const ON_Plane *parameter_plane,
    const ON_3dPoint &start, const ON_3dPoint &end)
{
    if (!use.parameter_curves.empty()) {
	const DirectoryEntry *curve_entry =
	    importer_.document().entity(use.parameter_curves.front());
	if (!curve_entry || curve_entry->type != 126) {
	    importer_.diagnose(Severity::Warning, "unsupported_parameter_curve",
		"B-Rep trim requires a B-Spline parameter curve", curve_entry);
	    return nullptr;
	}
	std::unique_ptr<ON_NurbsCurve> curve =
	    nurbs_curve(*curve_entry, false);
	if (!curve)
	    return nullptr;
	return std::unique_ptr<ON_Curve>(curve.release());
    }
    if (use.vertex_use) {
	importer_.diagnose(Severity::Warning, "missing_parameter_curve",
	    "B-Rep pole has no authored parameter-space curve");
	return nullptr;
    }
    if (!parameter_plane) {
	importer_.diagnose(Severity::Warning, "missing_parameter_curve",
	    "non-planar B-Rep trim has no authored parameter-space curve");
	return nullptr;
    }
    double start_u = 0.0;
    double start_v = 0.0;
    double end_u = 0.0;
    double end_v = 0.0;
    if (!parameter_plane->ClosestPointTo(start, &start_u, &start_v) ||
	    !parameter_plane->ClosestPointTo(end, &end_u, &end_v))
	return nullptr;
    std::unique_ptr<ON_Curve> curve(new ON_LineCurve(
	ON_2dPoint(start_u, start_v), ON_2dPoint(end_u, end_v)));
    curve->SetDomain(0.0, 1.0);
    return curve;
}

bool
SolidBuilder::add_face(EntityId id, bool same_direction,
    bool shell_same_direction)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(id) : nullptr;
    EntityId surface_id;
    int loop_count = 0;
    int has_outer_loop = 0;
    if (!entry || entry->type != 510 ||
	    !parameter_entity(parameters, 1, surface_id) ||
	    !parameter_integer(parameters, 2, loop_count) ||
	    !parameter_integer(parameters, 3, has_outer_loop) || loop_count < 1 ||
	    loop_count > MAX_ENTITY_LIST_COUNT) {
	importer_.diagnose(Severity::Warning, "face_parameters",
	    "Shell references an invalid Face entity", entry);
	return false;
    }
    std::vector<LoopRecord> loops(static_cast<size_t>(loop_count));
    for (int i = 0; i < loop_count; ++i) {
	EntityId loop_id;
	if (!parameter_entity(parameters, static_cast<size_t>(i + 4), loop_id) ||
		!parse_loop(loop_id, loops[static_cast<size_t>(i)]))
	    return false;
    }

    ON_Plane parameter_plane;
    bool has_parameter_plane = false;
    std::unique_ptr<ON_Surface> surface =
	face_surface(surface_id, loops, parameter_plane, has_parameter_plane);
    if (!surface)
	return false;
    ON_Surface *surface_geometry = surface.get();
    const int surface_index = brep_->AddSurface(surface.release());
    ON_BrepFace &face = brep_->NewFace(surface_index);
    face.m_bRev = !(same_direction == shell_same_direction);
    face.m_face_user.i = static_cast<int>(entry->id.value());

    for (size_t loop_index = 0; loop_index < loops.size(); ++loop_index) {
	const ON_BrepLoop::TYPE loop_type = has_outer_loop && loop_index == 0 ?
	    ON_BrepLoop::outer : ON_BrepLoop::inner;
	ON_BrepLoop &loop = brep_->NewLoop(loop_type, face);
	loop.m_loop_user.i =
	    static_cast<int>(loops[loop_index].source.value());
	for (const EdgeUse &use : loops[loop_index].uses) {
	    ON_3dPoint start;
	    ON_3dPoint end;
	    if (!edge_use_points(use, start, end))
		return false;
	    std::unique_ptr<ON_Curve> trim = trim_curve(use,
		has_parameter_plane ? &parameter_plane : nullptr, start, end);
	    if (!trim)
		return false;
	    ON_Curve *trim_geometry = trim.get();
	    const int trim_curve_index =
		brep_->AddTrimCurve(trim.release());
	    if (use.vertex_use) {
		const int vertex_index_value = vertex_index(use.vertex);
		if (vertex_index_value < 0)
		    return false;
		const ON_Interval domain = trim_geometry->Domain();
		const ON_Surface::ISO iso =
		    surface_geometry->IsIsoparametric(*trim_geometry, &domain);
		ON_BrepTrim &brep_trim = brep_->NewSingularTrim(
		    brep_->m_V[vertex_index_value], loop, iso,
		    trim_curve_index);
		brep_trim.m_tolerance[0] = importer_.tolerance();
		brep_trim.m_tolerance[1] = importer_.tolerance();
		continue;
	    }
	    const int edge_index_value = edge_index(use.edge);
	    if (edge_index_value < 0)
		return false;
	    ON_BrepEdge &edge = brep_->m_E[edge_index_value];
	    const int start_vertex = use.same_direction ? edge.m_vi[0] :
		edge.m_vi[1];
	    const bool reverse_edge = edge.m_vi[0] != start_vertex;
	    ON_BrepTrim &brep_trim = brep_->NewTrim(edge, reverse_edge, loop,
		trim_curve_index);
	    brep_trim.m_tolerance[0] = importer_.tolerance();
	    brep_trim.m_tolerance[1] = importer_.tolerance();
	    const ON_Interval domain = brep_trim.ProxyCurveDomain();
	    brep_trim.m_iso =
		surface_geometry->IsIsoparametric(*trim_geometry, &domain);
	}
    }
    return true;
}

bool
SolidBuilder::add_shell(EntityId id, bool same_direction)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(id) : nullptr;
    int count = 0;
    if (!entry || entry->type != 514 ||
	    !parameter_integer(parameters, 1, count) || count < 1 ||
	    count > MAX_ENTITY_LIST_COUNT) {
	importer_.diagnose(Severity::Warning, "shell_parameters",
	    "Manifold Solid references an invalid Shell entity", entry);
	return false;
    }
    size_t parameter = 2;
    for (int i = 0; i < count; ++i) {
	EntityId face;
	int orientation = 0;
	if (!parameter_entity(parameters, parameter++, face) ||
		!parameter_integer(parameters, parameter++, orientation) ||
		!add_face(face, orientation != 0, same_direction))
	    return false;
    }
    return true;
}

std::unique_ptr<ON_Brep>
SolidBuilder::build()
{
    if (!brep_)
	return nullptr;
    const ParameterList *parameters = importer_.document().parameters(solid_.id);
    EntityId outer_shell;
    int orientation = 0;
    int void_count = 0;
    if (!parameter_entity(parameters, 1, outer_shell) ||
	    !parameter_integer(parameters, 2, orientation) ||
	    !parameter_integer(parameters, 3, void_count) || void_count < 0 ||
	    void_count > MAX_ENTITY_LIST_COUNT ||
	    !add_shell(outer_shell, orientation != 0)) {
	importer_.diagnose(Severity::Warning, "solid_parameters",
	    "Manifold Solid B-Rep has invalid outer-shell parameters", &solid_);
	return nullptr;
    }
    size_t parameter = 4;
    for (int i = 0; i < void_count; ++i) {
	EntityId shell;
	int shell_orientation = 0;
	if (!parameter_entity(parameters, parameter++, shell) ||
		!parameter_integer(parameters, parameter++, shell_orientation) ||
		!add_shell(shell, shell_orientation != 0)) {
	    importer_.diagnose(Severity::Warning, "void_shell_parameters",
		"Manifold Solid B-Rep has invalid void-shell parameters", &solid_);
	    return nullptr;
	}
    }

    /* IGES topology is authoritative, but its curves and surfaces may differ
     * within the source system's modeling accuracy.  Measure those deviations
     * instead of rewriting the imported topology.  Loop types remain the
     * classifications supplied by the Face entities. */
    brep_->SetTrimTolerances(false);
    brep_->SetTrimIsoFlags();
    brep_->SetTrimTypeFlags();
    brep_->SetVertexTolerances(true);
    if (!brep_set_edge_endpoint_tolerances(*brep_, importer_.tolerance())) {
	importer_.diagnose(Severity::Warning, "edge_tolerance_derivation",
	    "could not derive B-Rep edge endpoint tolerances", &solid_);
	return nullptr;
    }
    brep_->SetTrimBoundingBoxes(false);
    for (int i = 0; i < brep_->m_V.Count(); ++i) {
	ON_BrepVertex &vertex = brep_->m_V[i];
	if (!std::isfinite(vertex.m_tolerance) || vertex.m_tolerance < 0.0)
	    vertex.m_tolerance = importer_.tolerance();
    }
    for (int i = 0; i < brep_->m_E.Count(); ++i) {
	ON_BrepEdge &edge = brep_->m_E[i];
	if (!std::isfinite(edge.m_tolerance) || edge.m_tolerance < 0.0)
	    edge.m_tolerance = importer_.tolerance();
    }
    for (int i = 0; i < brep_->m_T.Count(); ++i) {
	ON_BrepTrim &trim = brep_->m_T[i];
	for (int axis = 0; axis < 2; ++axis)
	    if (!std::isfinite(trim.m_tolerance[axis]) ||
		    trim.m_tolerance[axis] < 0.0)
		trim.m_tolerance[axis] = importer_.tolerance();
    }
    ON_wString validation_text;
    ON_TextLog validation_log(validation_text);
    if (!brep_->IsValid(&validation_log)) {
	ON_String text(validation_text);
	importer_.diagnose(Severity::Warning, "invalid_brep",
	    std::string("direct OpenNURBS topology validation failed: ") +
	    (text.Array() ? text.Array() : "no detail"), &solid_);
	return nullptr;
    }
    return std::move(brep_);
}

TrimmedSurfaceBuilder::TrimmedSurfaceBuilder(Importer &importer,
    const std::vector<const DirectoryEntry *> &faces) :
    importer_(importer), faces_(faces), brep_(ON_Brep::New())
{
}

bool
TrimmedSurfaceBuilder::append_curve_entities(EntityId id,
    std::vector<EntityId> &curves, std::set<EntityId> &active)
{
    if (!active.insert(id).second) {
	importer_.diagnose(Severity::Warning, "composite_curve_cycle",
	    "trimmed-surface boundary contains a cyclic Composite Curve");
	return false;
    }
    const DirectoryEntry *entry = importer_.document().entity(id);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(id) : nullptr;
    if (!entry) {
	importer_.diagnose(Severity::Warning, "boundary_curve_reference",
	    "trimmed-surface boundary references a missing curve");
	return false;
    }
    if (entry->type != 102) {
	curves.push_back(id);
	active.erase(id);
	return true;
    }

    int count = 0;
    if (!parameter_integer(parameters, 1, count) || count < 1 ||
	    count > MAX_ENTITY_LIST_COUNT) {
	importer_.diagnose(Severity::Warning, "composite_curve_parameters",
	    "Composite Curve has an invalid member count", entry);
	return false;
    }
    for (int i = 0; i < count; ++i) {
	EntityId member;
	if (!parameter_entity(parameters, static_cast<size_t>(i + 2), member) ||
		!append_curve_entities(member, curves, active))
	    return false;
    }
    active.erase(id);
    return true;
}

std::unique_ptr<ON_Curve>
TrimmedSurfaceBuilder::curve(SolidBuilder &geometry, EntityId id,
    bool model_space)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(id) : nullptr;
    if (!entry)
	return nullptr;
    if (entry->type == 126) {
	std::unique_ptr<ON_NurbsCurve> nurbs =
	    geometry.nurbs_curve(*entry, model_space);
	return std::unique_ptr<ON_Curve>(nurbs.release());
    }
    if (entry->type != 110)
	return nullptr;

    Point3 points[2];
    for (size_t point = 0; point < 2; ++point)
	for (size_t coordinate = 0; coordinate < 3; ++coordinate)
	    if (!parameter_real(parameters, 1 + point * 3 + coordinate,
		    points[point][coordinate]))
		return nullptr;
    if (model_space) {
	const Matrix &parent = geometry.solid_transform_;
	const Point3 start = importer_.model_point(*entry, points[0], parent);
	const Point3 end = importer_.model_point(*entry, points[1], parent);
	return std::unique_ptr<ON_Curve>(new ON_LineCurve(
	    ON_3dPoint(start.data()), ON_3dPoint(end.data())));
    }
    return std::unique_ptr<ON_Curve>(new ON_LineCurve(
	ON_2dPoint(points[0][0], points[0][1]),
	ON_2dPoint(points[1][0], points[1][1])));
}

bool
TrimmedSurfaceBuilder::curve_pairs(SolidBuilder &geometry, EntityId boundary,
    std::vector<CurvePair> &pairs)
{
    const DirectoryEntry *entry = importer_.document().entity(boundary);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(boundary) : nullptr;
    EntityId parameter_curve;
    EntityId model_curve;
    if (!entry || entry->type != 142 ||
	    !parameter_entity(parameters, 3, parameter_curve) ||
	    !parameter_entity(parameters, 4, model_curve)) {
	importer_.diagnose(Severity::Warning, "curve_on_surface_parameters",
	    "trimmed-surface boundary requires parameter- and model-space curves",
	    entry);
	return false;
    }

    std::vector<EntityId> parameter_entities;
    std::vector<EntityId> model_entities;
    std::set<EntityId> active;
    if (!append_curve_entities(parameter_curve, parameter_entities, active))
	return false;
    active.clear();
    if (!append_curve_entities(model_curve, model_entities, active))
	return false;
    if (parameter_entities.size() != model_entities.size()) {
	importer_.diagnose(Severity::Warning, "boundary_curve_cardinality",
	    "parameter- and model-space Composite Curves have different member counts",
	    entry);
	return false;
    }

    pairs.reserve(parameter_entities.size());
    for (size_t i = 0; i < parameter_entities.size(); ++i) {
	CurvePair pair;
	pair.singular = parameter_entities[i] == model_entities[i];
	pair.parameter = curve(geometry, parameter_entities[i], false);
	if (!pair.singular)
	    pair.model = curve(geometry, model_entities[i], true);
	if (!pair.parameter || pair.parameter->Dimension() != 2 ||
		(!pair.singular && (!pair.model || pair.model->Dimension() != 3))) {
	    importer_.diagnose(Severity::Warning, "unsupported_boundary_curve",
		"direct trimmed-surface import requires Line or B-Spline boundary curves",
		entry);
	    return false;
	}
	pairs.push_back(std::move(pair));
    }
    return !pairs.empty();
}

bool
TrimmedSurfaceBuilder::add_loop(ON_BrepFace &face, ON_BrepLoop::TYPE type,
    std::vector<CurvePair> &pairs, const DirectoryEntry &source)
{
    const double tolerance = importer_.tolerance();
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface || pairs.empty())
	return false;

    /* Start at a pole when one is present so the final non-singular edge can
     * close onto the first vertex without creating two vertices at the pole. */
    for (size_t i = 0; i < pairs.size(); ++i) {
	const size_t previous = i == 0 ? pairs.size() - 1 : i - 1;
	if (pairs[i].singular && !pairs[previous].singular) {
	    std::rotate(pairs.begin(), pairs.begin() + i, pairs.end());
	    break;
	}
    }

    const auto pair_points = [&](const CurvePair &pair, ON_3dPoint &start,
	    ON_3dPoint &end) {
	if (!pair.singular) {
	    if (!pair.model)
		return false;
	    start = pair.model->PointAtStart();
	    end = pair.model->PointAtEnd();
	    return start.IsValid() && end.IsValid();
	}

	const ON_Interval domain = pair.parameter->Domain();
	ON_3dPoint collapsed;
	bool have_point = false;
	for (double fraction : SINGULAR_CURVE_SAMPLES) {
	    const ON_3dPoint parameter = pair.parameter->PointAt(
		domain.ParameterAt(fraction));
	    const ON_3dPoint point = surface->PointAt(parameter.x, parameter.y);
	    if (!parameter.IsValid() || !point.IsValid())
		return false;
	    if (!have_point) {
		collapsed = point;
		have_point = true;
	    } else if (collapsed.DistanceTo(point) > tolerance) {
		importer_.diagnose(Severity::Warning,
		    "noncollapsed_singular_boundary",
		    "shared parameter/model boundary does not map to a surface pole",
		    &source);
		return false;
	    }
	}
	start = collapsed;
	end = collapsed;
	return have_point;
    };

    ON_3dPoint loop_start;
    ON_3dPoint ignored;
    if (!pair_points(pairs.front(), loop_start, ignored))
	return false;
    ON_BrepVertex &first_vertex = brep_->NewVertex(loop_start, tolerance);
    const int first_vertex_index = first_vertex.m_vertex_index;
    int current_vertex_index = first_vertex_index;
    ON_BrepLoop &loop = brep_->NewLoop(type, face);

    for (size_t i = 0; i < pairs.size(); ++i) {
	ON_3dPoint curve_start;
	ON_3dPoint curve_end;
	if (!pair_points(pairs[i], curve_start, curve_end))
	    return false;
	ON_BrepVertex &current_vertex = brep_->m_V[current_vertex_index];
	const double start_gap = current_vertex.Point().DistanceTo(curve_start);
	if (!std::isfinite(start_gap))
	    return false;
	/* Preserve finite gaps as tolerance metadata instead of moving either
	 * curve; the ordered IGES boundary supplies the topology. */
	current_vertex.m_tolerance = std::max(current_vertex.m_tolerance,
	    start_gap);

	int next_vertex_index = -1;
	if (pairs[i].singular) {
	    next_vertex_index = current_vertex_index;
	} else if (i + 1 == pairs.size()) {
	    const double closure_gap = curve_end.DistanceTo(loop_start);
	    if (!std::isfinite(closure_gap))
		return false;
	    next_vertex_index = first_vertex_index;
	    ON_BrepVertex &next_vertex = brep_->m_V[next_vertex_index];
	    next_vertex.m_tolerance = std::max(next_vertex.m_tolerance,
		closure_gap);
	} else {
	    next_vertex_index = brep_->NewVertex(curve_end,
		tolerance).m_vertex_index;
	}

	const int parameter_index =
	    brep_->AddTrimCurve(pairs[i].parameter.release());
	ON_BrepTrim *trim = nullptr;
	if (pairs[i].singular) {
	    ON_Curve *parameter = brep_->m_C2[parameter_index];
	    const ON_Interval domain = parameter->Domain();
	    const ON_Surface::ISO iso =
		surface->IsIsoparametric(*parameter, &domain);
	    if (iso < ON_Surface::W_iso || iso > ON_Surface::N_iso) {
		importer_.diagnose(Severity::Warning,
		    "nonisoparametric_singular_boundary",
		    "surface pole boundary is not a boundary isoparametric curve",
		    &source);
		return false;
	    }
	    trim = &brep_->NewSingularTrim(brep_->m_V[current_vertex_index],
		loop, iso, parameter_index);
	} else {
	    const int model_index = brep_->AddEdgeCurve(pairs[i].model.release());
	    ON_BrepEdge &edge = brep_->NewEdge(brep_->m_V[current_vertex_index],
		brep_->m_V[next_vertex_index], model_index);
	    edge.m_tolerance = tolerance;
	    trim = &brep_->NewTrim(edge, false, loop, parameter_index);
	}
	trim->m_tolerance[0] = tolerance;
	trim->m_tolerance[1] = tolerance;
	current_vertex_index = next_vertex_index;
    }
    return true;
}

bool
TrimmedSurfaceBuilder::add_face(const DirectoryEntry &entry)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    EntityId surface_id;
    int outer_boundary = 0;
    int inner_count = 0;
    EntityId outer_id;
    if (!parameter_entity(parameters, 1, surface_id) ||
	    !parameter_integer(parameters, 2, outer_boundary) ||
	    !parameter_integer(parameters, 3, inner_count) ||
	    outer_boundary != 1 || inner_count < 0 ||
	    inner_count > MAX_ENTITY_LIST_COUNT ||
	    !parameter_entity(parameters, 4, outer_id)) {
	importer_.diagnose(Severity::Warning, "trimmed_surface_parameters",
	    "direct import requires a Trimmed Surface with an explicit outer boundary",
	    &entry);
	return false;
    }

    const DirectoryEntry *surface_entry = importer_.document().entity(surface_id);
    SolidBuilder geometry(importer_, entry);
    if (!surface_entry || surface_entry->type != 128) {
	importer_.diagnose(Severity::Warning, "unsupported_trimmed_surface",
	    "direct trimmed-surface import currently requires a B-Spline surface",
	    surface_entry);
	return false;
    }
    std::unique_ptr<ON_NurbsSurface> surface =
	geometry.nurbs_surface(*surface_entry);
    if (!surface) {
	importer_.diagnose(Severity::Warning, "invalid_trimmed_surface_geometry",
	    "Trimmed Surface has invalid B-Spline surface geometry", surface_entry);
	return false;
    }

    std::vector<std::vector<CurvePair> > loops(
	static_cast<size_t>(inner_count) + 1);
    if (!curve_pairs(geometry, outer_id, loops[0]))
	return false;
    for (int i = 0; i < inner_count; ++i) {
	EntityId inner_id;
	if (!parameter_entity(parameters, static_cast<size_t>(i + 5), inner_id) ||
		!curve_pairs(geometry, inner_id,
		    loops[static_cast<size_t>(i + 1)]))
	    return false;
    }

    const int surface_index = brep_->AddSurface(surface.release());
    ON_BrepFace &face = brep_->NewFace(surface_index);
    face.m_face_user.i = static_cast<int>(entry.id.value());
    for (size_t i = 0; i < loops.size(); ++i)
	if (!add_loop(face, i == 0 ? ON_BrepLoop::outer : ON_BrepLoop::inner,
		loops[i], entry))
	    return false;
    return true;
}

std::unique_ptr<ON_Brep>
TrimmedSurfaceBuilder::build(brep_assembly_result &assembly)
{
    if (!brep_ || faces_.empty())
	return nullptr;

    for (const DirectoryEntry *entry : faces_) {
	if (!entry) {
	    importer_.diagnose(Severity::Warning, "trimmed_surface_reference",
		"trimmed-surface collection contains a missing face");
	    return nullptr;
	}
	if (!add_face(*entry)) {
	    importer_.diagnose(Severity::Warning, "trimmed_surface_face",
		"could not construct an OpenNURBS face", entry);
	    return nullptr;
	}
    }
    if (!brep_assemble(*brep_, importer_.tolerance(), &assembly)) {
	std::ostringstream detail;
	detail << "OpenNURBS face assembly failed (error " << assembly.error
	    << ", " << assembly.merged_edges << " edges merged, "
	    << assembly.remaining_naked_edges << " naked, "
	    << assembly.ambiguous_edges << " ambiguous)";
	importer_.diagnose(Severity::Warning, "trimmed_surface_assembly",
	    detail.str(), faces_.front());
	return nullptr;
    }
    return std::move(brep_);
}

std::string
Importer::unique_name(const DirectoryEntry &entry) const
{
    const std::string stem = source_name(document_, entry);
    std::string result = stem;
    size_t serial = 1;
    while (db_lookup(wdbp_->dbip, result.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	result = stem + "." + std::to_string(serial++);
    return result;
}

BrepImportResult
Importer::run()
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
	const bool repaired = diagnostic.code == "inferred_parameter_count" ||
	    diagnostic.code == "parameter_owner_repaired" ||
	    diagnostic.code == "record_data_too_long";
	if (!repaired)
	    continue;
	++result_.statistics.repairs;
	if (options_.repair == RepairMode::None || options_.exact ||
		options_.strict)
	    diagnose(Severity::Error, "repair_disallowed",
		"strict import does not permit: " + diagnostic.message);
    }
    if (!result_.diagnostics.empty())
	return result_;

    const std::vector<const DirectoryEntry *> solids = document_.find(186);
    const std::vector<const DirectoryEntry *> trimmed_surfaces =
	document_.find(144);
    result_.statistics.solids_seen = solids.size();
    result_.statistics.trimmed_surfaces_seen = trimmed_surfaces.size();
    for (const DirectoryEntry *solid : solids) {
	SolidBuilder builder(*this, *solid);
	std::unique_ptr<ON_Brep> brep = builder.build();
	if (!brep) {
	    ++result_.statistics.omitted;
	    continue;
	}
	const std::string name = unique_name(*solid);
	if (mk_brep(wdbp_, name.c_str(), brep.get()) < 0) {
	    diagnose(Severity::Error, "brep_write",
		"failed to write direct OpenNURBS B-Rep", solid);
	    ++result_.statistics.omitted;
	    continue;
	}
	const std::string entity_id = std::to_string(solid->id.value());
	db5_update_attribute(name.c_str(), "importer", "iges-g", wdbp_->dbip);
	db5_update_attribute(name.c_str(), "source_format", "iges", wdbp_->dbip);
	db5_update_attribute(name.c_str(), "iges.entity", entity_id.c_str(),
	    wdbp_->dbip);
	db5_update_attribute(name.c_str(), "iges.type", "186", wdbp_->dbip);
	db5_update_attribute(name.c_str(), "iges.topology", "direct-opennurbs",
	    wdbp_->dbip);
	++result_.statistics.breps_written;
    }
    if (!trimmed_surfaces.empty()) {
	brep_assembly_result assembly;
	TrimmedSurfaceBuilder builder(*this, trimmed_surfaces);
	std::unique_ptr<ON_Brep> brep = builder.build(assembly);
	if (!brep) {
	    result_.statistics.omitted += trimmed_surfaces.size();
	} else {
	    const DirectoryEntry &source = *trimmed_surfaces.front();
	    const std::string name = unique_name(source);
	    if (mk_brep(wdbp_, name.c_str(), brep.get()) < 0) {
		diagnose(Severity::Error, "brep_write",
		    "failed to write assembled OpenNURBS B-Rep", &source);
		result_.statistics.omitted += trimmed_surfaces.size();
	    } else {
		const std::string entity_id = std::to_string(source.id.value());
		const std::string face_count =
		    std::to_string(trimmed_surfaces.size());
		const std::string merged_edges =
		    std::to_string(assembly.merged_edges);
		db5_update_attribute(name.c_str(), "importer", "iges-g",
		    wdbp_->dbip);
		db5_update_attribute(name.c_str(), "source_format", "iges",
		    wdbp_->dbip);
		db5_update_attribute(name.c_str(), "iges.entity",
		    entity_id.c_str(), wdbp_->dbip);
		db5_update_attribute(name.c_str(), "iges.type", "144",
		    wdbp_->dbip);
		db5_update_attribute(name.c_str(), "iges.face_count",
		    face_count.c_str(), wdbp_->dbip);
		db5_update_attribute(name.c_str(), "iges.merged_edges",
		    merged_edges.c_str(), wdbp_->dbip);
		db5_update_attribute(name.c_str(), "iges.topology",
		    "direct-opennurbs-assembled", wdbp_->dbip);
		++result_.statistics.breps_written;
	    }
	}
    }
    const bool has_errors = std::any_of(result_.diagnostics.begin(),
	result_.diagnostics.end(), [](const ImportDiagnostic &diagnostic) {
	    return diagnostic.severity == Severity::Error ||
		diagnostic.severity == Severity::Fatal;
	});
    result_.success = !has_errors && result_.statistics.breps_written > 0 &&
	(!options_.strict || result_.statistics.omitted == 0);
    return result_;
}

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

} /* namespace brep_import_detail */

BrepImportResult
import_breps(const Document &document, struct rt_wdb *wdbp,
    const ImportOptions &options)
{
    brep_import_detail::Importer importer(document, wdbp, options);
    return importer.run();
}

bool
write_brep_import_report(const std::string &path, const Document &document,
    const ImportOptions &options, const BrepImportResult &result)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
	return false;
    output << "{\n"
	<< "  \"format\": \"iges\",\n"
	<< "  \"source\": \"" << brep_import_detail::json_escape(document.source_name()) << "\",\n"
	<< "  \"success\": " << (result.success ? "true" : "false") << ",\n"
	<< "  \"options\": {\"repair\": \""
	<< (options.repair == RepairMode::Safe ? "safe" : "none")
	<< "\", \"exact\": " << (options.exact ? "true" : "false")
	<< ", \"strict\": " << (options.strict ? "true" : "false") << "},\n"
	<< "  \"statistics\": {\"entities_read\": "
	<< result.statistics.entities_read << ", \"solids_seen\": "
	<< result.statistics.solids_seen << ", \"trimmed_surfaces_seen\": "
	<< result.statistics.trimmed_surfaces_seen << ", \"breps_written\": "
	<< result.statistics.breps_written << ", \"omitted\": "
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
	output << "\n    {\"severity\": \"" << brep_import_detail::severity_name(severity)
	    << "\", \"code\": \"" << brep_import_detail::json_escape(code)
	    << "\", \"message\": \"" << brep_import_detail::json_escape(message) << '"';
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
iges_import_breps(const char *path, struct rt_wdb *wdbp, int exact,
    int strict, const char *repair_mode, const char *report_path)
{
    if (!path || !wdbp)
	return -1;
    ON::Begin();
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_file(path);
    const bool mixed_csg = std::any_of(document.entities().begin(),
	document.entities().end(), [](const brlcad::iges::DirectoryEntry &entry) {
	    return entry.type >= 150 && entry.type <= 184;
	});
    const bool has_direct_brep = !document.find(186).empty() ||
	!document.find(144).empty();
    if (mixed_csg && has_direct_brep)
	return 0;
    brlcad::iges::ImportOptions options;
    options.exact = exact != 0;
    options.strict = strict != 0;
    if (repair_mode && BU_STR_EQUAL(repair_mode, "none"))
	options.repair = brlcad::iges::RepairMode::None;
    const brlcad::iges::BrepImportResult result =
	brlcad::iges::import_breps(document, wdbp, options);

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
	if (diagnostic.severity == brlcad::iges::Severity::Information)
	    continue;
	LogSummary &summary = summaries[diagnostic.code];
	++summary.count;
	if (summary.message.empty()) {
	    summary.message = diagnostic.message;
	    summary.entity_id = diagnostic.entity_id;
	}
    }
    for (const auto &item : summaries) {
	const LogSummary &summary = item.second;
	const std::string count = std::to_string(summary.count);
	const std::string location = summary.record ?
	    std::to_string(summary.record) : std::to_string(summary.entity_id);
	bu_log("IGES %s%s%s%s%s%s: %s\n", item.first.c_str(),
	    summary.count > 1 ? " (" : "",
	    summary.count > 1 ? count.c_str() : "",
	    summary.count > 1 ? " occurrences)" : "",
	    summary.record ? " first at record " :
		(summary.entity_id ? " first for D" : ""),
	    summary.record || summary.entity_id ? location.c_str() : "",
	    summary.message.c_str());
    }
    if (report_path && report_path[0] != '\0' &&
	(result.statistics.solids_seen > 0 ||
	 result.statistics.trimmed_surfaces_seen > 0) &&
	!brlcad::iges::write_brep_import_report(report_path, document, options,
	    result)) {
	bu_log("IGES: unable to write import report %s\n", report_path);
	return -1;
    }
    if (result.success)
	return 1;
    if (!document.valid())
	return -1;
    const bool import_error = std::any_of(result.diagnostics.begin(),
	result.diagnostics.end(),
	[](const brlcad::iges::ImportDiagnostic &diagnostic) {
	    return diagnostic.severity == brlcad::iges::Severity::Error ||
		diagnostic.severity == brlcad::iges::Severity::Fatal;
	});
	const bool direct_entities_seen = result.statistics.solids_seen > 0 ||
	    result.statistics.trimmed_surfaces_seen > 0;
    return import_error || (strict && direct_entities_seen) ?
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
