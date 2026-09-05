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
#include "brep/pullback.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"
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
constexpr double CURVE_ENDPOINT_RELATIVE_TOLERANCE = 1.0e-6;
constexpr double CONIC_PARAMETER_TOLERANCE = 1.0e-12;
constexpr int COLLAPSED_BOUNDARY_VALIDATION_SEGMENTS = 64;
constexpr double SAFE_TRIM_REPAIR_TOLERANCE_FACTOR = 100.0;
constexpr double RELAXED_TRIM_TOLERANCE_STEP_FACTOR = 10.0;
constexpr double SAFE_TRIM_MODEL_GAP_FACTOR = 10.0;
constexpr int MAX_ENTITY_LIST_COUNT = 10000000;
constexpr double IGES_COLOR_PERCENTAGE_MAX = 100.0;
constexpr double COLOR_CHANNEL_MAX = 255.0;
const unsigned char IGES_STANDARD_COLORS[][3] = {
    {0, 0, 0}, {0, 0, 0}, {255, 0, 0}, {0, 255, 0},
    {0, 0, 255}, {255, 255, 0}, {255, 0, 255}, {0, 255, 255},
    {255, 255, 255}
};
constexpr double SINGULAR_CURVE_SAMPLES[] = {0.0, 0.5, 1.0};
constexpr double CURVE_ORIENTATION_SAMPLES[] = {0.0, 0.25, 0.5, 0.75, 1.0};

static bool
is_native_csg_entity_type(int type)
{
    switch (type) {
	case 150:
	case 152:
	case 154:
	case 156:
	case 158:
	case 160:
	case 162:
	case 164:
	case 168:
	    return true;
	default:
	    return false;
    }
}


using Point3 = std::array<double, 3>;

struct Matrix {
    double m[4][4] = {
	{1.0, 0.0, 0.0, 0.0},
	{0.0, 1.0, 0.0, 0.0},
	{0.0, 0.0, 1.0, 0.0},
	{0.0, 0.0, 0.0, 1.0}
    };
};

struct InstanceProperties {
    std::string shader_name;
    std::string shader_arguments;
    int region_flag = 0;
    int ident = 0;
    int air = 0;
    int material = 0;
    int line_of_sight = 0;
    int inherit = 0;
    std::array<unsigned char, 3> color = {0, 0, 0};
    bool has_color = false;
    int64_t source_entity = 0;
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

bool
is_surface_entity(int type)
{
    switch (type) {
	case 108:
	case 114:
	case 118:
	case 120:
	case 122:
	case 128:
	case 140:
	case 190:
	    return true;
	default:
	    return false;
    }
}

bool
is_supported_standalone_surface(int type)
{
    return type == 118 || type == 120 || type == 122 || type == 128;
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

std::vector<EntityId>
property_entities(const ParameterList *parameters,
    size_t associativity_parameter)
{
    std::vector<EntityId> properties;
    int associativity_count = 0;
    if (!parameter_integer(parameters, associativity_parameter,
	    associativity_count) ||
	    associativity_count < 0 ||
	    associativity_count > MAX_ENTITY_LIST_COUNT)
	return properties;

    size_t parameter = associativity_parameter + 1 +
	static_cast<size_t>(associativity_count);
    int property_count = 0;
    if (!parameter_integer(parameters, parameter++, property_count) ||
	    property_count < 0 || property_count > MAX_ENTITY_LIST_COUNT)
	return properties;
    properties.reserve(static_cast<size_t>(property_count));
    for (int i = 0; i < property_count; ++i) {
	EntityId property;
	if (!parameter_entity(parameters, parameter++, property))
	    break;
	properties.push_back(property);
    }
    return properties;
}


static bool
associativity_parameter(const ParameterList *parameters,
    const DirectoryEntry &entry, size_t &parameter)
{
    int count = 0;
    if (entry.type == 186 && parameter_integer(parameters, 3, count) &&
	    count >= 0 && count <= MAX_ENTITY_LIST_COUNT)
	parameter = 4 + static_cast<size_t>(count) * 2;
    else if (entry.type == 184 &&
	    parameter_integer(parameters, 1, count) && count >= 0 &&
	    count <= MAX_ENTITY_LIST_COUNT)
	parameter = 2 + static_cast<size_t>(count) * 2;
    else if (entry.type == 180 &&
	    parameter_integer(parameters, 1, count) && count >= 0 &&
	    count <= MAX_ENTITY_LIST_COUNT)
	parameter = 2 + static_cast<size_t>(count);
    else if (entry.type == 430)
	parameter = 2;
    else if (entry.type == 308 &&
	    parameter_integer(parameters, 3, count) && count >= 0 &&
	    count <= MAX_ENTITY_LIST_COUNT)
	parameter = 4 + static_cast<size_t>(count);
    else if (entry.type == 408)
	parameter = 6;
    else if (entry.type == 402 &&
	    parameter_integer(parameters, 1, count) && count >= 0 &&
	    count <= MAX_ENTITY_LIST_COUNT)
	parameter = 2 + static_cast<size_t>(count);
    else if (entry.type == 144) {
	int outer_boundary = 0;
	if (!parameter_integer(parameters, 2, outer_boundary) ||
		(outer_boundary != 0 && outer_boundary != 1) ||
		!parameter_integer(parameters, 3, count) || count < 0 ||
		count > MAX_ENTITY_LIST_COUNT)
	    return false;
	parameter = 4 + static_cast<size_t>(outer_boundary + count);
    } else {
	return false;
    }
    return true;
}

static std::string
name_property(const Document &document, const DirectoryEntry &entry)
{
    const ParameterList *parameters = document.parameters(entry.id);
    size_t parameter = 0;
    if (!associativity_parameter(parameters, entry, parameter))
	return std::string();

    for (EntityId property_id : property_entities(parameters, parameter)) {
	const DirectoryEntry *property = document.entity(property_id);
	const ParameterList *property_parameters = property ?
	    document.parameters(property_id) : nullptr;
	int value_count = 0;
	std::string candidate;
	if (property && property->type == 406 && property->form == 15 &&
		parameter_integer(property_parameters, 1, value_count) &&
		value_count == 1 &&
		parameter_string(property_parameters, 2, candidate) &&
		!candidate.empty())
	    return candidate;
    }
    return std::string();
}

static std::string
semantic_name(const Document &document, const DirectoryEntry &entry)
{
    const std::string property = name_property(document, entry);
    return property.empty() ? entry.label : property;
}

static std::string
sanitized_database_name(const std::string &source)
{
    struct bu_vls sanitized = BU_VLS_INIT_ZERO;

    db_sanitize_name(&sanitized, source.c_str());
    const std::string result = bu_vls_cstr(&sanitized);
    bu_vls_free(&sanitized);
    return result;
}

std::string
source_name(const Document &document, const DirectoryEntry &entry)
{
    std::string result = sanitized_database_name(semantic_name(document, entry));
    if (result.empty())
	result = "iges_brep_D" + std::to_string(entry.id.value());
    return result;
}

static bool
entity_color(const Document &document, const DirectoryEntry &entry,
    std::array<unsigned char, 3> &rgb)
{
    if (entry.color > 0 &&
	    static_cast<size_t>(entry.color) <
		sizeof(IGES_STANDARD_COLORS) / sizeof(IGES_STANDARD_COLORS[0])) {
	std::copy(IGES_STANDARD_COLORS[entry.color],
	    IGES_STANDARD_COLORS[entry.color] + 3, rgb.begin());
	return true;
    }
    if (entry.color >= 0)
	return false;

    const EntityId color_id(-static_cast<int64_t>(entry.color));
    const DirectoryEntry *color = document.entity(color_id);
    const ParameterList *parameters = color ?
	document.parameters(color_id) : nullptr;
    if (!color || color->type != 314)
	return false;
    for (size_t channel = 0; channel < rgb.size(); ++channel) {
	double percentage = 0.0;
	if (!parameter_real(parameters, channel + 1, percentage) ||
		!std::isfinite(percentage))
	    return false;
	const double bounded = std::max(0.0,
	    std::min(IGES_COLOR_PERCENTAGE_MAX, percentage));
	rgb[channel] = static_cast<unsigned char>(std::round(
	    bounded * COLOR_CHANNEL_MAX / IGES_COLOR_PERCENTAGE_MAX));
    }
    return true;
}

void
solid_instance_properties(const Document &document,
    const DirectoryEntry &entry, InstanceProperties &properties)
{
    constexpr size_t instance_associativity_parameter = 2;
    enum AttributeParameter : size_t {
	ShaderName = 1,
	ShaderArguments,
	RegionFlag,
	Ident,
	Air,
	Material,
	LineOfSight,
	Inherit,
	ColorDefined
    };

    const ParameterList *instance_parameters =
	document.parameters(entry.id);
    for (EntityId property_id : property_entities(instance_parameters,
	    instance_associativity_parameter)) {
	const DirectoryEntry *property = document.entity(property_id);
	if (!property || property->type != 422)
	    continue;
	const ParameterList *parameters = document.parameters(property_id);
	parameter_string(parameters, ShaderName, properties.shader_name);
	parameter_string(parameters, ShaderArguments,
	    properties.shader_arguments);
	parameter_integer(parameters, RegionFlag, properties.region_flag);
	parameter_integer(parameters, Ident, properties.ident);
	parameter_integer(parameters, Air, properties.air);
	parameter_integer(parameters, Material, properties.material);
	parameter_integer(parameters, LineOfSight, properties.line_of_sight);
	parameter_integer(parameters, Inherit, properties.inherit);
	properties.source_entity = property_id.value();

	int color_defined = 0;
	if (!parameter_integer(parameters, ColorDefined, color_defined) ||
		!color_defined)
	    return;
	properties.has_color = entity_color(document, entry, properties.color);
	return;
    }
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
    SolidBuilder(Importer &importer, const DirectoryEntry &solid,
	const Matrix &solid_transform);

    std::unique_ptr<ON_Brep> build();
    std::unique_ptr<ON_NurbsCurve> nurbs_curve(const DirectoryEntry &entry,
	bool model_space);
    std::unique_ptr<ON_NurbsCurve> nurbs_curve(const DirectoryEntry &entry,
	bool model_space, const Matrix &parent);
    std::unique_ptr<ON_NurbsCurve> curve(const DirectoryEntry &entry,
	bool model_space, const Matrix &parent);
    std::unique_ptr<ON_NurbsCurve> conic_arc(const DirectoryEntry &entry,
	bool model_space, const Matrix &parent);
    std::unique_ptr<ON_NurbsSurface> nurbs_surface(
	const DirectoryEntry &entry);
    std::unique_ptr<ON_NurbsSurface> analytic_surface(
	const DirectoryEntry &entry);

private:
    bool transform_curve(ON_NurbsCurve &curve, const DirectoryEntry &entry,
	const Matrix &parent);
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
    std::unique_ptr<ON_NurbsSurface> ruled_surface(
	const DirectoryEntry &entry, const Matrix &parent);
    std::unique_ptr<ON_NurbsSurface> revolution_surface(
	const DirectoryEntry &entry, const Matrix &parent);
    std::unique_ptr<ON_NurbsSurface> tabulated_surface(
	const DirectoryEntry &entry, const Matrix &parent);
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
    const std::map<EntityId, double> &relaxed_tolerances() const
    {
	return relaxed_tolerances_;
    }

private:
    struct CurvePair {
	std::unique_ptr<ON_Curve> parameter;
	std::unique_ptr<ON_Curve> model;
	double repair_tolerance = 0.0;
	bool singular = false;
	bool discard = false;
    };

    bool append_curve_entities(EntityId id, std::vector<EntityId> &curves,
	std::set<EntityId> &active);
    std::unique_ptr<ON_Curve> curve(SolidBuilder &geometry, EntityId id,
	bool model_space);
    bool curve_pairs(SolidBuilder &geometry, EntityId boundary,
	std::vector<CurvePair> &pairs);
    bool bounded_curve_pairs(SolidBuilder &geometry, EntityId boundary,
	EntityId surface, std::vector<CurvePair> &pairs);
    bool add_loop(ON_BrepFace &face, ON_BrepLoop::TYPE type,
	std::vector<CurvePair> &pairs, const DirectoryEntry &source);
    std::unique_ptr<ON_PlaneSurface> plane_surface(SolidBuilder &geometry,
	const DirectoryEntry &surface_entry,
	std::vector<std::vector<CurvePair> > &loops);
    void recover_parameter_curves(const ON_Surface &surface,
	std::vector<std::vector<CurvePair> > &loops,
	const DirectoryEntry &source);
    bool add_face(const DirectoryEntry &entry);

    Importer &importer_;
    const std::vector<const DirectoryEntry *> &faces_;
    std::unique_ptr<ON_Brep> brep_;
    std::map<EntityId, double> relaxed_tolerances_;
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
	source_resolution_declared_ = std::isfinite(source_resolution) &&
	    source_resolution > 0.0;
	tolerance_ = source_resolution_declared_ ? source_resolution :
	    DEFAULT_TOPOLOGY_TOLERANCE_MM;
    }

    BrepImportResult run();
    const Document &document() const { return document_; }
    double tolerance() const { return tolerance_; }
    double source_tolerance() const
    {
	return unit_to_mm_ > 0.0 ? tolerance_ / unit_to_mm_ : tolerance_;
    }
    bool safe_repairs() const
    {
	return options_.repair == RepairMode::Safe && !options_.exact &&
	    !options_.strict;
    }
    double maximum_trim_repair_tolerance() const
    {
	const double safe_limit = std::max(tolerance_, ON_ZERO_TOLERANCE) *
	    SAFE_TRIM_REPAIR_TOLERANCE_FACTOR;
	return std::isfinite(options_.maximum_repair_tolerance) &&
	    options_.maximum_repair_tolerance > 0.0 ?
	    options_.maximum_repair_tolerance : safe_limit;
    }
    void count_relaxed_face(double tolerance)
    {
	++result_.statistics.relaxed_faces_written;
	result_.statistics.maximum_repair_tolerance_used = std::max(
	    result_.statistics.maximum_repair_tolerance_used, tolerance);
    }
    void count_repair() { ++result_.statistics.repairs; }
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
    std::string unique_name(const DirectoryEntry &entry,
	const std::string &source) const;
    std::string unique_name(const std::string &stem) const;
    void write_entity_attributes(const std::string &name,
	const DirectoryEntry &entry);
    bool write_color_attribute(const std::string &name,
	const std::array<unsigned char, 3> &rgb,
	const DirectoryEntry &entry);
    bool write_entity_color_attribute(const std::string &name,
	const DirectoryEntry &entry);
    bool write_face_metadata(const std::string &name, const ON_Brep &brep,
	const std::vector<const DirectoryEntry *> &faces,
	const std::map<EntityId, double> &relaxed_tolerances);
    bool write_plate_mode_attributes(const std::string &name,
	const ON_Brep &brep, const DirectoryEntry &entry);
    bool write_trimmed_component(
	const std::vector<const DirectoryEntry *> &faces);
    void import_trimmed_components(
	const std::vector<const DirectoryEntry *> &faces);
    bool write_standalone_surface(const DirectoryEntry &entry);
    void combination_matrix(const Matrix &source, mat_t result) const;
    bool container_members(const DirectoryEntry &entry,
	std::vector<EntityId> &members) const;
    bool write_container(EntityId id, std::set<EntityId> &active);
    bool write_boolean_tree(EntityId id, std::set<EntityId> &active);
    bool write_instance(const DirectoryEntry &entry,
	std::set<EntityId> &active);
    bool write_solid_instance(const DirectoryEntry &entry,
	std::set<EntityId> &active);
    bool write_instance_combination(const DirectoryEntry &entry,
	EntityId definition_id, const Matrix &placement, const std::string &stem,
	const char *semantic);
    bool write_hierarchy();
    std::string hierarchy_name(const DirectoryEntry &entry) const;
    bool write_root();

    const Document &document_;
    struct rt_wdb *wdbp_ = nullptr;
    ImportOptions options_;
    BrepImportResult result_;
    double unit_to_mm_ = DEFAULT_UNIT_TO_MM;
    double tolerance_ = DEFAULT_TOPOLOGY_TOLERANCE_MM;
    bool source_resolution_declared_ = false;
    std::map<EntityId, Matrix> transforms_;
    std::map<EntityId, std::string> objects_;
    std::set<std::string> root_objects_;
    std::set<EntityId> deferred_boolean_trees_;
    std::set<EntityId> deferred_instances_;

    friend class SolidBuilder;
};

SolidBuilder::SolidBuilder(Importer &importer, const DirectoryEntry &solid) :
    SolidBuilder(importer, solid, importer.transform(solid.transform))
{
}

SolidBuilder::SolidBuilder(Importer &importer, const DirectoryEntry &solid,
    const Matrix &solid_transform) : importer_(importer), solid_(solid),
    solid_transform_(solid_transform), brep_(ON_Brep::New())
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
    return nurbs_curve(entry, model_space, solid_transform_);
}

std::unique_ptr<ON_NurbsCurve>
SolidBuilder::nurbs_curve(const DirectoryEntry &entry, bool model_space,
    const Matrix &parent)
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
	    point = importer_.model_point(entry, source, parent);
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

bool
SolidBuilder::transform_curve(ON_NurbsCurve &curve,
    const DirectoryEntry &entry, const Matrix &parent)
{
    for (int i = 0; i < curve.CVCount(); ++i) {
	ON_4dPoint control;
	if (!curve.GetCV(i, control) ||
		std::fabs(control.w) <= DEGENERATE_DOMAIN_TOLERANCE)
	    return false;
	const Point3 source = {
	    control.x / control.w,
	    control.y / control.w,
	    control.z / control.w
	};
	const Point3 transformed = importer_.model_point(entry, source, parent);
	if (!curve.SetCV(i, ON_4dPoint(transformed[0] * control.w,
		transformed[1] * control.w, transformed[2] * control.w,
		control.w)))
	    return false;
    }
    return curve.IsValid();
}


std::unique_ptr<ON_NurbsCurve>
SolidBuilder::conic_arc(const DirectoryEntry &entry, bool model_space,
    const Matrix &parent)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    double coefficients[6] = {0.0};
    for (size_t i = 0; i < 6; ++i)
	if (!parameter_real(parameters, i + 1, coefficients[i]))
	    return nullptr;

    double depth = 0.0;
    double start_x = 0.0;
    double start_y = 0.0;
    double end_x = 0.0;
    double end_y = 0.0;
    if (!parameter_real(parameters, 7, depth) ||
	    !parameter_real(parameters, 8, start_x) ||
	    !parameter_real(parameters, 9, start_y) ||
	    !parameter_real(parameters, 10, end_x) ||
	    !parameter_real(parameters, 11, end_y))
	return nullptr;

    if (coefficients[0] + coefficients[2] < 0.0)
	for (double &coefficient : coefficients)
	    coefficient = -coefficient;
    const double a = coefficients[0];
    const double b = coefficients[1];
    const double c = coefficients[2];
    const double d = coefficients[3];
    const double e = coefficients[4];
    const double f = coefficients[5];
    const double coefficient_scale = std::max(std::fabs(a),
	std::max(std::fabs(b), std::fabs(c)));
    const double determinant = 4.0 * a * c - b * b;
    if (!(coefficient_scale > 0.0) ||
	    determinant <= DEGENERATE_DOMAIN_TOLERANCE *
		coefficient_scale * coefficient_scale)
	return nullptr;

    const double center_x = (b * e - 2.0 * c * d) / determinant;
    const double center_y = (b * d - 2.0 * a * e) / determinant;
    const double center_value =
	a * center_x * center_x + b * center_x * center_y +
	c * center_y * center_y + d * center_x + e * center_y + f;
    const double radius_squared_scale = -center_value;
    if (!(radius_squared_scale > 0.0) ||
	    !std::isfinite(radius_squared_scale))
	return nullptr;

    const double rotation = 0.5 * std::atan2(b, a - c);
    const double cosine = std::cos(rotation);
    const double sine = std::sin(rotation);
    const double lambda_x = a * cosine * cosine +
	b * cosine * sine + c * sine * sine;
    const double lambda_y = a * sine * sine -
	b * cosine * sine + c * cosine * cosine;
    if (!(lambda_x > 0.0) || !(lambda_y > 0.0))
	return nullptr;
    const double radius_x = std::sqrt(radius_squared_scale / lambda_x);
    const double radius_y = std::sqrt(radius_squared_scale / lambda_y);
    if (!std::isfinite(radius_x) || !std::isfinite(radius_y) ||
	    radius_x <= DEGENERATE_DOMAIN_TOLERANCE ||
	    radius_y <= DEGENERATE_DOMAIN_TOLERANCE)
	return nullptr;

    const ON_3dPoint center(center_x, center_y, depth);
    const ON_3dVector x_axis(cosine, sine, 0.0);
    const ON_3dVector y_axis(-sine, cosine, 0.0);
    const ON_Plane plane(center, x_axis, y_axis);
    const ON_Ellipse ellipse(plane, radius_x, radius_y);
    std::unique_ptr<ON_NurbsCurve> result(new ON_NurbsCurve());
    if (!plane.IsValid() || !ellipse.IsValid() ||
	    ellipse.GetNurbForm(*result) != 2 || !result->IsValid() ||
	    !result->IsClosed())
	return nullptr;

    const auto ellipse_angle = [&](double x, double y) {
	const ON_3dVector offset = ON_3dPoint(x, y, depth) - center;
	double angle = std::atan2(ON_DotProduct(offset, y_axis) / radius_y,
	    ON_DotProduct(offset, x_axis) / radius_x);
	if (angle < 0.0)
	    angle += 2.0 * ON_PI;
	return angle;
    };
    const ON_3dPoint start(start_x, start_y, depth);
    const ON_3dPoint end(end_x, end_y, depth);
    const double start_angle = ellipse_angle(start_x, start_y);
    const double end_angle = ellipse_angle(end_x, end_y);
    const double endpoint_tolerance = std::max(DEGENERATE_DOMAIN_TOLERANCE,
	std::max(importer_.source_tolerance(), std::max(radius_x, radius_y) * CURVE_ENDPOINT_RELATIVE_TOLERANCE));
    if (ellipse.PointAt(start_angle).DistanceTo(start) > endpoint_tolerance ||
	    ellipse.PointAt(end_angle).DistanceTo(end) > endpoint_tolerance)
	return nullptr;

    ON_Circle parameter_circle(plane, 1.0);
    double nurbs_start = ON_UNSET_VALUE;
    double nurbs_end = ON_UNSET_VALUE;
    if (!parameter_circle.GetNurbFormParameterFromRadian(start_angle,
	    &nurbs_start) ||
	    !parameter_circle.GetNurbFormParameterFromRadian(end_angle,
		&nurbs_end))
	return nullptr;

    const ON_Interval original_domain = result->Domain();
    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	original_domain.Length() * CONIC_PARAMETER_TOLERANCE);
    if (!original_domain.IsIncreasing() ||
	    !result->ChangeClosedCurveSeam(nurbs_start) || !result->IsValid())
	return nullptr;

    const double closure_tolerance = std::max(DEGENERATE_DOMAIN_TOLERANCE,
	std::max(radius_x, radius_y) * CONIC_PARAMETER_TOLERANCE);
    const bool complete = start.DistanceTo(end) <= closure_tolerance;
    if (!complete) {
	const ON_Interval relocated_domain = result->Domain();
	while (nurbs_end <= relocated_domain.Min() + parameter_guard)
	    nurbs_end += original_domain.Length();
	if (nurbs_end > relocated_domain.Max() + parameter_guard ||
		!result->Trim(ON_Interval(relocated_domain.Min(),
		    std::min(nurbs_end, relocated_domain.Max()))) ||
		!result->IsValid())
	    return nullptr;
    }

    if (result->PointAtStart().DistanceTo(start) > endpoint_tolerance ||
	    result->PointAtEnd().DistanceTo(end) > endpoint_tolerance)
	return nullptr;
    if (!model_space)
	return result->ChangeDimension(2) && result->IsValid() ?
	    std::move(result) : nullptr;
    return transform_curve(*result, entry, parent) ?
	std::move(result) : nullptr;
}


std::unique_ptr<ON_NurbsCurve>
SolidBuilder::curve(const DirectoryEntry &entry, bool model_space,
    const Matrix &parent)
{
    if (entry.type == 126)
	return nurbs_curve(entry, model_space, parent);
    if (entry.type == 104)
	return conic_arc(entry, model_space, parent);

    const ParameterList *parameters = importer_.document().parameters(entry.id);
    if (entry.type == 110) {
	Point3 endpoints[2];
	for (size_t point = 0; point < 2; ++point)
	    for (size_t coordinate = 0; coordinate < 3; ++coordinate)
		if (!parameter_real(parameters, 1 + point * 3 + coordinate,
			endpoints[point][coordinate]))
		    return nullptr;
	if (model_space) {
	    endpoints[0] = importer_.model_point(entry, endpoints[0], parent);
	    endpoints[1] = importer_.model_point(entry, endpoints[1], parent);
	}
	std::unique_ptr<ON_NurbsCurve> line(ON_NurbsCurve::New(
	    model_space ? 3 : 2, false, 2, 2));
	if (!line)
	    return nullptr;
	line->SetKnot(0, 0.0);
	line->SetKnot(1, 1.0);
	if (model_space) {
	    line->SetCV(0, ON_3dPoint(endpoints[0].data()));
	    line->SetCV(1, ON_3dPoint(endpoints[1].data()));
	} else {
	    double start[2] = {endpoints[0][0], endpoints[0][1]};
	    double end[2] = {endpoints[1][0], endpoints[1][1]};
	    line->SetCV(0, ON::not_rational, start);
	    line->SetCV(1, ON::not_rational, end);
	}
	return line->IsValid() ? std::move(line) : nullptr;
    }
    if (entry.type != 100)
	return nullptr;

    double depth = 0.0;
    double center_x = 0.0;
    double center_y = 0.0;
    double start_x = 0.0;
    double start_y = 0.0;
    double end_x = 0.0;
    double end_y = 0.0;
    if (!parameter_real(parameters, 1, depth) ||
	    !parameter_real(parameters, 2, center_x) ||
	    !parameter_real(parameters, 3, center_y) ||
	    !parameter_real(parameters, 4, start_x) ||
	    !parameter_real(parameters, 5, start_y) ||
	    !parameter_real(parameters, 6, end_x) ||
	    !parameter_real(parameters, 7, end_y))
	return nullptr;

    const ON_3dPoint center(center_x, center_y, depth);
    const ON_3dPoint start(start_x, start_y, depth);
    const ON_3dPoint end(end_x, end_y, depth);
    const double radius = center.DistanceTo(start);
    const double end_radius = center.DistanceTo(end);
    if (!std::isfinite(radius) || radius <= DEGENERATE_DOMAIN_TOLERANCE ||
	    !std::isfinite(end_radius) ||
	    std::fabs(radius - end_radius) >
		std::max(importer_.source_tolerance(),
		    std::max(radius, end_radius) *
			CURVE_ENDPOINT_RELATIVE_TOLERANCE))
	return nullptr;

    ON_3dVector x_axis = start - center;
    if (!x_axis.Unitize())
	return nullptr;
    const ON_3dVector y_axis = ON_CrossProduct(ON_zaxis, x_axis);
    const ON_3dVector end_vector = end - center;
    double angle = std::atan2(ON_DotProduct(end_vector, y_axis),
	ON_DotProduct(end_vector, x_axis));
    if (start.DistanceTo(end) <= DEGENERATE_DOMAIN_TOLERANCE)
	angle = 2.0 * ON_PI;
    else if (angle <= ON_ZERO_TOLERANCE)
	angle += 2.0 * ON_PI;
    const ON_Arc arc(ON_Circle(ON_Plane(center, x_axis, y_axis), radius),
	angle);
    std::unique_ptr<ON_NurbsCurve> result(ON_NurbsCurve::New());
    if (!result || !arc.IsValid() || !arc.GetNurbForm(*result)) {
	importer_.diagnose(Severity::Warning, "invalid_arc_curve",
	    "could not construct the exact rational circular arc", &entry);
	return nullptr;
    }

    if (!model_space)
	return result->ChangeDimension(2) && result->IsValid() ?
	    std::move(result) : nullptr;

    if (!transform_curve(*result, entry, parent)) {
	importer_.diagnose(Severity::Warning, "invalid_arc_curve",
	    "could not transform the exact rational circular arc", &entry);
	return nullptr;
    }
    return result;
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
    if (entry->type == 100 || entry->type == 104 || entry->type == 126) {
	std::unique_ptr<ON_NurbsCurve> curve = this->curve(*entry, true,
	    solid_transform_);
	if (!curve) {
	    importer_.diagnose(Severity::Warning, "edge_curve_parameters",
		"B-Rep edge curve has invalid parameters", entry);
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
		"B-Rep edge curve has invalid endpoints", entry);
	    return nullptr;
	}
	if (reverse < forward)
	    curve->Reverse();
	return std::unique_ptr<ON_Curve>(curve.release());
    }
    if (entry->type != 110) {
	importer_.diagnose(Severity::Warning, "unsupported_edge_curve",
	    "direct manifold import requires Arc, Conic Arc, Line, or B-Spline edge geometry",
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

bool
synchronize_curve_knots(ON_NurbsCurve &first, ON_NurbsCurve &second)
{
    const int desired_degree = std::max(first.Degree(), second.Degree());
    if ((first.Degree() < desired_degree &&
	    !first.IncreaseDegree(desired_degree)) ||
	    (second.Degree() < desired_degree &&
	     !second.IncreaseDegree(desired_degree)))
	return false;
    if (!first.SetDomain(0.0, 1.0) || !second.SetDomain(0.0, 1.0))
	return false;

    const auto insert_missing = [](const ON_NurbsCurve &source,
	    ON_NurbsCurve &target) {
	const ON_Interval domain = source.Domain();
	for (int i = 0; i < source.KnotCount();) {
	    const double knot = source.Knot(i);
	    const int multiplicity = source.KnotMultiplicity(i);
	    if (knot > domain.Min() + ON_ZERO_TOLERANCE &&
		    knot < domain.Max() - ON_ZERO_TOLERANCE &&
		    !target.InsertKnot(knot, multiplicity))
		return false;
	    i += std::max(1, multiplicity);
	}
	return true;
    };
    if (!insert_missing(second, first) || !insert_missing(first, second) ||
	    first.CVCount() != second.CVCount() ||
	    first.KnotCount() != second.KnotCount())
	return false;
    for (int i = 0; i < first.KnotCount(); ++i)
	if (std::fabs(first.Knot(i) - second.Knot(i)) >
		DEGENERATE_DOMAIN_TOLERANCE)
	    return false;
    return true;
}

std::unique_ptr<ON_NurbsSurface>
SolidBuilder::tabulated_surface(const DirectoryEntry &entry,
    const Matrix &parent)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    EntityId directrix_id;
    Point3 line_end;
    if (!parameter_entity(parameters, 1, directrix_id))
	return nullptr;
    for (size_t coordinate = 0; coordinate < line_end.size(); ++coordinate)
	if (!parameter_real(parameters, coordinate + 2, line_end[coordinate]))
	    return nullptr;
    const DirectoryEntry *directrix = importer_.document().entity(directrix_id);
    if (!directrix)
	return nullptr;
    std::unique_ptr<ON_NurbsCurve> base = curve(*directrix, true, parent);
    if (!base)
	return nullptr;

    const Point3 transformed_end = importer_.model_point(entry, line_end,
	solid_transform_);
    const ON_3dVector direction =
	ON_3dPoint(transformed_end.data()) - base->PointAtStart();
    if (!direction.IsValid() || direction.Length() <= DEGENERATE_DOMAIN_TOLERANCE)
	return nullptr;

    const bool rational = base->IsRational();
    std::unique_ptr<ON_NurbsSurface> surface(ON_NurbsSurface::New(3,
	rational, base->Order(), 2, base->CVCount(), 2));
    if (!surface)
	return nullptr;
    for (int i = 0; i < base->KnotCount(); ++i)
	surface->SetKnot(0, i, base->Knot(i));
    surface->SetKnot(1, 0, 0.0);
    surface->SetKnot(1, 1, 1.0);
    for (int u = 0; u < base->CVCount(); ++u) {
	if (rational) {
	    ON_4dPoint control;
	    if (!base->GetCV(u, control) ||
		    std::fabs(control.w) <= DEGENERATE_DOMAIN_TOLERANCE)
		return nullptr;
	    surface->SetCV(u, 0, control);
	    surface->SetCV(u, 1, ON_4dPoint(
		control.x + direction.x * control.w,
		control.y + direction.y * control.w,
		control.z + direction.z * control.w, control.w));
	} else {
	    ON_3dPoint control;
	    if (!base->GetCV(u, control))
		return nullptr;
	    surface->SetCV(u, 0, control);
	    surface->SetCV(u, 1, control + direction);
	}
    }
    return surface->IsValid() ? std::move(surface) : nullptr;
}

std::unique_ptr<ON_NurbsSurface>
SolidBuilder::ruled_surface(const DirectoryEntry &entry, const Matrix &parent)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    EntityId first_id;
    EntityId second_id;
    int reverse_second = 0;
    int developable = 0;
    if (!parameter_entity(parameters, 1, first_id) ||
	    !parameter_entity(parameters, 2, second_id) ||
	    !parameter_integer(parameters, 3, reverse_second) ||
	    !parameter_integer(parameters, 4, developable) ||
	    (developable != 0 && developable != 1))
	return nullptr;
    const DirectoryEntry *first_entry = importer_.document().entity(first_id);
    const DirectoryEntry *second_entry = importer_.document().entity(second_id);
    if (!first_entry || !second_entry)
	return nullptr;
    std::unique_ptr<ON_NurbsCurve> first =
	curve(*first_entry, true, parent);
    std::unique_ptr<ON_NurbsCurve> second =
	curve(*second_entry, true, parent);
    if (!first || !second)
	return nullptr;
    if (reverse_second != 0 && !second->Reverse())
	return nullptr;
    if ((first->IsRational() || second->IsRational()) &&
	    ((!first->IsRational() && !first->MakeRational()) ||
	     (!second->IsRational() && !second->MakeRational())))
	return nullptr;
    if (!synchronize_curve_knots(*first, *second))
	return nullptr;

    const bool rational = first->IsRational();
    std::unique_ptr<ON_NurbsSurface> surface(ON_NurbsSurface::New(3,
	rational, first->Order(), 2, first->CVCount(), 2));
    if (!surface)
	return nullptr;
    for (int i = 0; i < first->KnotCount(); ++i)
	surface->SetKnot(0, i, first->Knot(i));
    surface->SetKnot(1, 0, 0.0);
    surface->SetKnot(1, 1, 1.0);
    for (int u = 0; u < first->CVCount(); ++u) {
	if (rational) {
	    ON_4dPoint first_control;
	    ON_4dPoint second_control;
	    if (!first->GetCV(u, first_control) ||
		    !second->GetCV(u, second_control))
		return nullptr;
	    surface->SetCV(u, 0, first_control);
	    surface->SetCV(u, 1, second_control);
	} else {
	    ON_3dPoint first_control;
	    ON_3dPoint second_control;
	    if (!first->GetCV(u, first_control) ||
		    !second->GetCV(u, second_control))
		return nullptr;
	    surface->SetCV(u, 0, first_control);
	    surface->SetCV(u, 1, second_control);
	}
    }
    return surface->IsValid() ? std::move(surface) : nullptr;
}

std::unique_ptr<ON_NurbsSurface>
SolidBuilder::revolution_surface(const DirectoryEntry &entry,
    const Matrix &parent)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    EntityId axis_id;
    EntityId generatrix_id;
    double start_angle = 0.0;
    double end_angle = 0.0;
    if (!parameter_entity(parameters, 1, axis_id) ||
	    !parameter_entity(parameters, 2, generatrix_id) ||
	    !parameter_real(parameters, 3, start_angle) ||
	    !parameter_real(parameters, 4, end_angle))
	return nullptr;
    const DirectoryEntry *axis_entry = importer_.document().entity(axis_id);
    const DirectoryEntry *generatrix_entry =
	importer_.document().entity(generatrix_id);
    if (!axis_entry || axis_entry->type != 110 || !generatrix_entry)
	return nullptr;
    std::unique_ptr<ON_NurbsCurve> axis = curve(*axis_entry, true, parent);
    std::unique_ptr<ON_NurbsCurve> generatrix =
	curve(*generatrix_entry, true, parent);
    if (!axis || !generatrix)
	return nullptr;
    const ON_3dPoint axis_start = axis->PointAtStart();
    const ON_3dPoint axis_end = axis->PointAtEnd();
    if (!axis_start.IsValid() || !axis_end.IsValid() ||
	    axis_start.DistanceTo(axis_end) <= DEGENERATE_DOMAIN_TOLERANCE)
	return nullptr;

    while (end_angle <= start_angle + ON_ZERO_TOLERANCE)
	end_angle += 2.0 * ON_PI;
    if (end_angle - start_angle > 2.0 * ON_PI + ON_ZERO_TOLERANCE)
	return nullptr;
    end_angle = std::min(end_angle, start_angle + 2.0 * ON_PI);

    ON_RevSurface revolution;
    revolution.m_curve = generatrix.release();
    revolution.m_axis = ON_Line(axis_start, axis_end);
    revolution.m_angle = ON_Interval(start_angle, end_angle);
    revolution.m_t = revolution.m_angle;
    revolution.m_bTransposed = false;
    std::unique_ptr<ON_NurbsSurface> surface(ON_NurbsSurface::New());
    if (!surface || !revolution.IsValid() ||
	    !revolution.GetNurbForm(*surface, importer_.tolerance()))
	return nullptr;
    return surface->IsValid() ? std::move(surface) : nullptr;
}

std::unique_ptr<ON_NurbsSurface>
SolidBuilder::analytic_surface(const DirectoryEntry &entry)
{
    const Matrix parent = multiply(solid_transform_,
	importer_.transform(entry.transform));
    switch (entry.type) {
	case 118:
	    return ruled_surface(entry, parent);
	case 120:
	    return revolution_surface(entry, parent);
	case 122:
	    return tabulated_surface(entry, parent);
	default:
	    return nullptr;
    }
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
    if (entry->type == 118 || entry->type == 120 || entry->type == 122 ||
	    entry->type == 128) {
	std::unique_ptr<ON_NurbsSurface> surface = entry->type == 128 ?
	    nurbs_surface(*entry) : analytic_surface(*entry);
	has_parameter_plane = false;
	return std::unique_ptr<ON_Surface>(surface.release());
    }
    importer_.diagnose(Severity::Warning, "unsupported_face_surface",
	"direct manifold import does not support this face surface geometry",
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
	if (!curve_entry || (curve_entry->type != 100 &&
		curve_entry->type != 110 && curve_entry->type != 126)) {
	    importer_.diagnose(Severity::Warning, "unsupported_parameter_curve",
		"B-Rep trim requires an Arc, Line, or B-Spline parameter curve",
		curve_entry);
	    return nullptr;
	}
	std::unique_ptr<ON_NurbsCurve> curve = this->curve(*curve_entry, false,
	    solid_transform_);
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
    if (!entry)
	return nullptr;
    std::unique_ptr<ON_NurbsCurve> result = geometry.curve(*entry,
	model_space, geometry.solid_transform_);
    return std::unique_ptr<ON_Curve>(result.release());
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
    const bool have_parameter_curve =
	parameter_entity(parameters, 3, parameter_curve);
    if (!entry || entry->type != 142 ||
	    !parameter_entity(parameters, 4, model_curve)) {
	importer_.diagnose(Severity::Warning, "curve_on_surface_parameters",
	    "trimmed-surface boundary requires a model-space curve", entry);
	return false;
    }

    std::vector<EntityId> parameter_entities;
    std::vector<EntityId> model_entities;
    std::set<EntityId> active;
    if (have_parameter_curve &&
	    !append_curve_entities(parameter_curve, parameter_entities, active))
	return false;
    active.clear();
    if (!append_curve_entities(model_curve, model_entities, active))
	return false;
    if (have_parameter_curve &&
	    parameter_entities.size() != model_entities.size()) {
	importer_.diagnose(Severity::Warning, "boundary_curve_cardinality",
	    "parameter- and model-space Composite Curves have different member counts",
	    entry);
	return false;
    }

    pairs.reserve(model_entities.size());
    for (size_t i = 0; i < model_entities.size(); ++i) {
	CurvePair pair;
	pair.singular = have_parameter_curve &&
	    parameter_entities[i] == model_entities[i];
	if (have_parameter_curve)
	    pair.parameter = curve(geometry, parameter_entities[i], false);
	if (!pair.singular)
	    pair.model = curve(geometry, model_entities[i], true);
	const bool invalid_parameter =
	    pair.parameter && pair.parameter->Dimension() != 2;
	const bool invalid_model = !pair.singular &&
	    (!pair.model || pair.model->Dimension() != 3);
	if (invalid_parameter || invalid_model) {
	    const EntityId failed_id = invalid_parameter ?
		parameter_entities[i] : model_entities[i];
	    const DirectoryEntry *failed =
		importer_.document().entity(failed_id);
	    std::ostringstream message;
	    message << "direct trimmed-surface import could not construct the "
		<< (invalid_parameter ? "parameter" : "model")
		<< "-space boundary curve D" << failed_id.value();
	    if (failed)
		message << " (IGES type " << failed->type << ')';
	    importer_.diagnose(Severity::Warning, "unsupported_boundary_curve",
		message.str(), failed ? failed : entry);
	    return false;
	}
	pairs.push_back(std::move(pair));
    }
    return !pairs.empty();
}


bool
TrimmedSurfaceBuilder::bounded_curve_pairs(SolidBuilder &geometry,
    EntityId boundary, EntityId surface, std::vector<CurvePair> &pairs)
{
    const DirectoryEntry *entry = importer_.document().entity(boundary);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(boundary) : nullptr;
    EntityId boundary_surface;
    int curve_count = 0;
    if (!entry || entry->type != 141 ||
	    !parameter_entity(parameters, 3, boundary_surface) ||
	    !(boundary_surface == surface) ||
	    !parameter_integer(parameters, 4, curve_count) || curve_count < 1 ||
	    curve_count > MAX_ENTITY_LIST_COUNT) {
	importer_.diagnose(Severity::Warning, "boundary_parameters",
	    "Bounded Surface requires a valid Boundary entity for its base surface",
	    entry);
	return false;
    }

    size_t parameter = 5;
    for (int curve_index = 0; curve_index < curve_count; ++curve_index) {
	EntityId model_curve;
	int sense = 0;
	int parameter_curve_count = 0;
	if (!parameter_entity(parameters, parameter++, model_curve) ||
		!parameter_integer(parameters, parameter++, sense) ||
		(sense != 1 && sense != 2) ||
		!parameter_integer(parameters, parameter++, parameter_curve_count) ||
		parameter_curve_count < 0 ||
		parameter_curve_count > MAX_ENTITY_LIST_COUNT) {
	    importer_.diagnose(Severity::Warning, "boundary_curve_parameters",
		"Boundary entity has an invalid model/parameter curve record",
		entry);
	    return false;
	}

	std::vector<EntityId> model_entities;
	std::vector<EntityId> parameter_entities;
	std::set<EntityId> active;
	if (!append_curve_entities(model_curve, model_entities, active))
	    return false;
	for (int i = 0; i < parameter_curve_count; ++i) {
	    EntityId parameter_curve;
	    if (!parameter_entity(parameters, parameter++, parameter_curve)) {
		importer_.diagnose(Severity::Warning,
		    "boundary_curve_parameters",
		    "Boundary entity has an invalid parameter-space curve reference",
		    entry);
		return false;
	    }
	    active.clear();
	    if (!append_curve_entities(parameter_curve, parameter_entities,
		    active))
		return false;
	}
	if (!parameter_entities.empty() &&
		parameter_entities.size() != model_entities.size()) {
	    importer_.diagnose(Severity::Warning,
		"boundary_curve_cardinality",
		"Boundary entity has different model- and parameter-space curve counts",
		entry);
	    return false;
	}

	for (size_t i = 0; i < model_entities.size(); ++i) {
	    CurvePair pair;
	    pair.model = curve(geometry, model_entities[i], true);
	    if (!parameter_entities.empty())
		pair.parameter = curve(geometry, parameter_entities[i], false);
	    if (!pair.model || pair.model->Dimension() != 3 ||
		    (pair.parameter && pair.parameter->Dimension() != 2)) {
		const EntityId failed_id = !pair.model ? model_entities[i] :
		    parameter_entities.empty() ? model_entities[i] :
		    parameter_entities[i];
		const DirectoryEntry *failed =
		    importer_.document().entity(failed_id);
		importer_.diagnose(Severity::Warning,
		    "unsupported_boundary_curve",
		    "direct bounded-surface import could not construct a boundary curve",
		    failed ? failed : entry);
		return false;
	    }
	    if (sense == 2 && (!pair.model->Reverse() ||
		    (pair.parameter && !pair.parameter->Reverse()))) {
		importer_.diagnose(Severity::Warning,
		    "boundary_curve_orientation",
		    "could not apply a Boundary entity curve orientation", entry);
		return false;
	    }
	    pairs.push_back(std::move(pair));
	}
    }
    return !pairs.empty();
}

bool
TrimmedSurfaceBuilder::add_loop(ON_BrepFace &face, ON_BrepLoop::TYPE type,
    std::vector<CurvePair> &pairs, const DirectoryEntry &source)
{
    const double tolerance = importer_.tolerance();
    const ON_Surface *surface = face.SurfaceOf();
    const auto reject = [&](const char *message) {
	importer_.diagnose(Severity::Warning, "trimmed_surface_loop",
	    message, &source);
	return false;
    };
    if (!surface || pairs.empty())
	return reject("trimmed-surface loop has no usable boundary members");

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

    /* Type 142 does not guarantee that the parameter and model curves use
     * the same parameter direction.  Reversing a curve changes neither its
     * locus nor the authored member order, and lets the OpenNURBS trim carry
     * the direction IGES intended. */
    for (CurvePair &pair : pairs) {
	if (pair.singular || !pair.parameter || !pair.model)
	    continue;
	const ON_Interval parameter_domain = pair.parameter->Domain();
	const ON_Interval model_domain = pair.model->Domain();
	double forward = 0.0;
	double reverse = 0.0;
	for (double fraction : CURVE_ORIENTATION_SAMPLES) {
	    const ON_3dPoint parameter = pair.parameter->PointAt(
		parameter_domain.ParameterAt(fraction));
	    const ON_3dPoint lifted = surface->PointAt(parameter.x, parameter.y);
	    const ON_3dPoint model_forward = pair.model->PointAt(
		model_domain.ParameterAt(fraction));
	    const ON_3dPoint model_reverse = pair.model->PointAt(
		model_domain.ParameterAt(1.0 - fraction));
	    if (!parameter.IsValid() || !lifted.IsValid() ||
		    !model_forward.IsValid() || !model_reverse.IsValid())
		return reject("could not compare parameter/model curve orientations");
	    forward += lifted.DistanceTo(model_forward);
	    reverse += lifted.DistanceTo(model_reverse);
	}
	if (std::isfinite(forward) && std::isfinite(reverse) &&
		reverse < forward && !pair.parameter->Reverse())
		    return reject("could not reverse a parameter curve to match its model curve");
    }

    /* Choose all curve directions together.  The two-state dynamic program
     * minimizes model-space gaps around the complete cycle in linear time. */
    struct OrientedEndpoints {
	ON_3dPoint start[2];
	ON_3dPoint end[2];
    };
    std::vector<OrientedEndpoints> endpoints(pairs.size());
    for (size_t i = 0; i < pairs.size(); ++i) {
	if (!pair_points(pairs[i], endpoints[i].start[0], endpoints[i].end[0]))
		return reject("could not evaluate boundary-curve endpoints");
	endpoints[i].start[1] = endpoints[i].end[0];
	endpoints[i].end[1] = endpoints[i].start[0];
    }
    std::vector<int> best_directions(pairs.size(), 0);
    double best_cost = std::numeric_limits<double>::infinity();
    for (int first_direction = 0; first_direction < 2; ++first_direction) {
	std::vector<std::array<double, 2> > costs(pairs.size());
	std::vector<std::array<int, 2> > previous(pairs.size());
	costs[0][0] = costs[0][1] =
	    std::numeric_limits<double>::infinity();
	costs[0][first_direction] = 0.0;
	for (size_t i = 1; i < pairs.size(); ++i) {
	    for (int direction = 0; direction < 2; ++direction) {
		costs[i][direction] = std::numeric_limits<double>::infinity();
		previous[i][direction] = 0;
		for (int prior = 0; prior < 2; ++prior) {
		    const double candidate = costs[i - 1][prior] +
			endpoints[i - 1].end[prior].DistanceTo(
			    endpoints[i].start[direction]);
		    if (candidate < costs[i][direction]) {
			costs[i][direction] = candidate;
			previous[i][direction] = prior;
		    }
		}
	    }
	}
	for (int last_direction = 0; last_direction < 2; ++last_direction) {
	    const double cost = costs.back()[last_direction] +
		endpoints.back().end[last_direction].DistanceTo(
		    endpoints.front().start[first_direction]);
	    if (cost >= best_cost)
		continue;
	    best_cost = cost;
	    int direction = last_direction;
	    for (size_t i = pairs.size(); i-- > 0;) {
		best_directions[i] = direction;
		if (i > 0)
		    direction = previous[i][direction];
	    }
	}
    }
    if (!std::isfinite(best_cost))
	return reject("could not determine finite boundary-curve orientations");
    for (size_t i = 0; i < pairs.size(); ++i) {
	if (!best_directions[i])
	    continue;
	if (!pairs[i].parameter->Reverse() ||
		(pairs[i].model && !pairs[i].model->Reverse()))
		return reject("could not reverse an oriented boundary curve");
    }

    if (importer_.safe_repairs()) {
	for (size_t i = 0; i < pairs.size(); ++i) {
	    const size_t next = (i + 1) % pairs.size();
	    if (pairs[i].singular || pairs[next].singular ||
		    !pairs[i].model || !pairs[next].model)
		continue;
	    const ON_3dPoint parameter_end = pairs[i].parameter->PointAtEnd();
	    const ON_3dPoint parameter_start =
		pairs[next].parameter->PointAtStart();
	    if (!parameter_end.IsValid() || !parameter_start.IsValid() ||
		    parameter_end.DistanceTo(parameter_start) <= ON_ZERO_TOLERANCE)
		continue;
	    const ON_3dPoint model_end = pairs[i].model->PointAtEnd();
	    const ON_3dPoint model_start = pairs[next].model->PointAtStart();
	    const double model_gap = model_end.DistanceTo(model_start);
	    const ON_3dPoint end_surface =
		surface->PointAt(parameter_end.x, parameter_end.y);
	    const ON_3dPoint start_surface =
		surface->PointAt(parameter_start.x, parameter_start.y);
	    const double end_cost = std::max(end_surface.DistanceTo(model_end),
		end_surface.DistanceTo(model_start));
	    const double start_cost =
		std::max(start_surface.DistanceTo(model_end),
		    start_surface.DistanceTo(model_start));
	    const bool prefer_end = end_cost <= start_cost;
	    const std::array<ON_3dPoint, 2> targets = {
		prefer_end ? parameter_end : parameter_start,
		prefer_end ? parameter_start : parameter_end
	    };
	    const std::array<double, 2> repair_costs = {
		prefer_end ? end_cost : start_cost,
		prefer_end ? start_cost : end_cost
	    };
	    const double repair_limit = std::max(
		SAFE_TRIM_REPAIR_TOLERANCE_FACTOR * tolerance,
		model_gap + SAFE_TRIM_MODEL_GAP_FACTOR * tolerance);
	    if (!std::isfinite(repair_limit))
		continue;
	    bool repaired = false;
	    for (size_t candidate = 0;
		    candidate < targets.size() && !repaired; ++candidate) {
		if (!std::isfinite(repair_costs[candidate]) ||
			repair_costs[candidate] > repair_limit)
		    continue;
		std::unique_ptr<ON_Curve> current_candidate(
		    pairs[i].parameter->DuplicateCurve());
		std::unique_ptr<ON_Curve> next_candidate;
		ON_Curve *next_curve = current_candidate.get();
		if (i != next) {
		    next_candidate.reset(pairs[next].parameter->DuplicateCurve());
		    next_curve = next_candidate.get();
		}
		if (!current_candidate || !next_curve)
		    continue;
		const bool endpoints_set =
		    current_candidate->SetEndPoint(targets[candidate]) &&
		    next_curve->SetStartPoint(targets[candidate]);
		repaired = endpoints_set && current_candidate->IsValid() &&
		    next_curve->IsValid();
		if (!repaired)
		    continue;
		pairs[i].parameter = std::move(current_candidate);
		if (i != next)
		    pairs[next].parameter = std::move(next_candidate);
	    }
	    if (!repaired)
		continue;
	    importer_.count_repair();
	    importer_.diagnose(Severity::Information,
		"closed_parameter_loop",
		"closed a bounded parameter-space trim gap using model geometry",
		&source);
	}
    }

    ON_3dPoint loop_start;
    ON_3dPoint ignored;
    if (!pair_points(pairs.front(), loop_start, ignored))
	return reject("could not evaluate the first model-space boundary member");
    ON_BrepVertex &first_vertex = brep_->NewVertex(loop_start, tolerance);
    const int first_vertex_index = first_vertex.m_vertex_index;
    int current_vertex_index = first_vertex_index;
    ON_BrepLoop &loop = brep_->NewLoop(type, face);

    for (size_t i = 0; i < pairs.size(); ++i) {
	const double pair_tolerance = std::max(tolerance,
	    pairs[i].repair_tolerance);
	ON_3dPoint curve_start;
	ON_3dPoint curve_end;
	if (!pair_points(pairs[i], curve_start, curve_end))
		return reject("could not evaluate a model-space boundary member");
	ON_BrepVertex &current_vertex = brep_->m_V[current_vertex_index];
	const double start_gap = current_vertex.Point().DistanceTo(curve_start);
	if (!std::isfinite(start_gap))
		return reject("model-space edge start distance was non-finite");
	/* Preserve finite gaps as tolerance metadata instead of moving either
	 * curve; the ordered IGES boundary supplies the topology. */
	current_vertex.m_tolerance = std::max(current_vertex.m_tolerance,
	    std::max(pair_tolerance, start_gap));

	int next_vertex_index = -1;
	if (pairs[i].singular) {
	    next_vertex_index = current_vertex_index;
	} else if (i + 1 == pairs.size()) {
	    const double closure_gap = curve_end.DistanceTo(loop_start);
	    if (!std::isfinite(closure_gap))
		return reject("model-space loop closure distance was non-finite");
	    next_vertex_index = first_vertex_index;
	    ON_BrepVertex &next_vertex = brep_->m_V[next_vertex_index];
	    next_vertex.m_tolerance = std::max(next_vertex.m_tolerance,
		std::max(pair_tolerance, closure_gap));
	} else {
	    next_vertex_index = brep_->NewVertex(curve_end,
		pair_tolerance).m_vertex_index;
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
	    edge.m_tolerance = pair_tolerance;
	    trim = &brep_->NewTrim(edge, false, loop, parameter_index);
	}
	trim->m_tolerance[0] = pair_tolerance;
	trim->m_tolerance[1] = pair_tolerance;
	current_vertex_index = next_vertex_index;
    }
    return true;
}

std::unique_ptr<ON_PlaneSurface>
TrimmedSurfaceBuilder::plane_surface(SolidBuilder &geometry,
    const DirectoryEntry &surface_entry,
    std::vector<std::vector<CurvePair> > &loops)
{
    const ParameterList *parameters =
	importer_.document().parameters(surface_entry.id);
    Point3 normal;
    double distance = 0.0;
    for (size_t coordinate = 0; coordinate < normal.size(); ++coordinate)
	if (!parameter_real(parameters, coordinate + 1, normal[coordinate]))
	    return nullptr;
    if (!parameter_real(parameters, 4, distance))
	return nullptr;
    const double normal_squared = normal[0] * normal[0] +
	normal[1] * normal[1] + normal[2] * normal[2];
    if (!std::isfinite(normal_squared) ||
	    normal_squared <= DEGENERATE_DOMAIN_TOLERANCE)
	return nullptr;
    Point3 origin = {
	normal[0] * distance / normal_squared,
	normal[1] * distance / normal_squared,
	normal[2] * distance / normal_squared
    };
    if (!normalize(normal))
	return nullptr;
    const ON_Plane local_plane(ON_3dPoint(origin.data()),
	ON_3dVector(normal.data()));
    Point3 local_x = {
	origin[0] + local_plane.xaxis.x,
	origin[1] + local_plane.xaxis.y,
	origin[2] + local_plane.xaxis.z
    };
    Point3 local_y = {
	origin[0] + local_plane.yaxis.x,
	origin[1] + local_plane.yaxis.y,
	origin[2] + local_plane.yaxis.z
    };
    const Point3 model_origin = importer_.model_point(surface_entry, origin,
	geometry.solid_transform_);
    const Point3 model_x = importer_.model_point(surface_entry, local_x,
	geometry.solid_transform_);
    const Point3 model_y = importer_.model_point(surface_entry, local_y,
	geometry.solid_transform_);
    Point3 x_axis = {
	model_x[0] - model_origin[0],
	model_x[1] - model_origin[1],
	model_x[2] - model_origin[2]
    };
    Point3 y_seed = {
	model_y[0] - model_origin[0],
	model_y[1] - model_origin[1],
	model_y[2] - model_origin[2]
    };
    Point3 model_normal = cross(x_axis, y_seed);
    if (!normalize(x_axis) || !normalize(model_normal))
	return nullptr;
    Point3 y_axis = cross(model_normal, x_axis);
    if (!normalize(y_axis))
	return nullptr;
    const ON_Plane plane(ON_3dPoint(model_origin.data()),
	ON_3dVector(x_axis.data()), ON_3dVector(y_axis.data()));
    if (!plane.IsValid())
	return nullptr;

    double u_min = std::numeric_limits<double>::infinity();
    double u_max = -std::numeric_limits<double>::infinity();
    double v_min = std::numeric_limits<double>::infinity();
    double v_max = -std::numeric_limits<double>::infinity();
    for (std::vector<CurvePair> &loop : loops) {
	for (CurvePair &pair : loop) {
	    if (pair.singular || !pair.model)
		return nullptr;
	    ON_NurbsCurve model_curve;
	    if (!pair.model->GetNurbForm(model_curve) ||
		    model_curve.Dimension() != 3)
		return nullptr;
	    const ON_Interval domain = model_curve.Domain();
	    for (double fraction : SINGULAR_CURVE_SAMPLES) {
		const ON_3dPoint point =
		    model_curve.PointAt(domain.ParameterAt(fraction));
		if (!point.IsValid() ||
			std::fabs(plane.DistanceTo(point)) > importer_.tolerance())
		    return nullptr;
	    }

	    std::unique_ptr<ON_NurbsCurve> parameter_curve(
		ON_NurbsCurve::New(2, model_curve.IsRational(),
		    model_curve.Order(), model_curve.CVCount()));
	    if (!parameter_curve)
		return nullptr;
	    for (int i = 0; i < model_curve.KnotCount(); ++i)
		parameter_curve->SetKnot(i, model_curve.Knot(i));
	    for (int i = 0; i < model_curve.CVCount(); ++i) {
		ON_4dPoint control;
		if (!model_curve.GetCV(i, control) ||
			std::fabs(control.w) <= DEGENERATE_DOMAIN_TOLERANCE)
		    return nullptr;
		const ON_3dPoint point(control.x / control.w,
		    control.y / control.w, control.z / control.w);
		double u = 0.0;
		double v = 0.0;
		if (!plane.ClosestPointTo(point, &u, &v))
		    return nullptr;
		if (model_curve.IsRational()) {
		    double homogeneous[3] = {
			u * control.w, v * control.w, control.w
		    };
		    parameter_curve->SetCV(i, ON::homogeneous_rational,
			homogeneous);
		} else {
		    double coordinates[2] = {u, v};
		    parameter_curve->SetCV(i, ON::not_rational, coordinates);
		}
		u_min = std::min(u_min, u);
		u_max = std::max(u_max, u);
		v_min = std::min(v_min, v);
		v_max = std::max(v_max, v);
	    }
	    if (!parameter_curve->IsValid())
		return nullptr;
	    pair.parameter = std::move(parameter_curve);
	}
    }
    if (!std::isfinite(u_min) || !std::isfinite(u_max) ||
	    !std::isfinite(v_min) || !std::isfinite(v_max) ||
	    u_max - u_min <= DEGENERATE_DOMAIN_TOLERANCE ||
	    v_max - v_min <= DEGENERATE_DOMAIN_TOLERANCE)
	return nullptr;
    std::unique_ptr<ON_PlaneSurface> surface(new ON_PlaneSurface(plane));
    if (!surface->SetExtents(0, ON_Interval(u_min, u_max), true) ||
	    !surface->SetExtents(1, ON_Interval(v_min, v_max), true) ||
	    !surface->IsValid())
	return nullptr;
    return surface;
}

static std::unique_ptr<ON_LineCurve>
collapsed_singular_parameter_curve(const ON_Surface &surface,
    const ON_Curve &model_curve, double tolerance)
{
    const ON_3dPoint collapsed_point = model_curve.PointAtStart();
    if (!collapsed_point.IsValid())
	return nullptr;

    std::unique_ptr<ON_LineCurve> result;
    for (int fixed_direction = 0; fixed_direction < 2; ++fixed_direction) {
	const ON_Interval fixed_domain = surface.Domain(fixed_direction);
	const ON_Interval varying_domain = surface.Domain(1 - fixed_direction);
	if (!fixed_domain.IsIncreasing() || !varying_domain.IsIncreasing())
	    continue;
	for (int side = 0; side < 2; ++side) {
	    const int surface_side = fixed_direction == 0 ?
		(side == 0 ? 3 : 1) : (side == 0 ? 0 : 2);
	    if (!surface.IsSingular(surface_side))
		continue;

	    bool collapsed = true;
	    for (int sample = 0;
		    sample <= COLLAPSED_BOUNDARY_VALIDATION_SEGMENTS; ++sample) {
		ON_2dPoint parameter;
		parameter[fixed_direction] = fixed_domain[side];
		parameter[1 - fixed_direction] = varying_domain.ParameterAt(
		    static_cast<double>(sample) /
			COLLAPSED_BOUNDARY_VALIDATION_SEGMENTS);
		const ON_3dPoint lifted =
		    surface.PointAt(parameter.x, parameter.y);
		if (!lifted.IsValid() ||
			lifted.DistanceTo(collapsed_point) > tolerance) {
		    collapsed = false;
		    break;
		}
	    }
	    if (!collapsed)
		continue;
	    if (result)
		return nullptr;

	    ON_2dPoint start;
	    ON_2dPoint end;
	    start[fixed_direction] = fixed_domain[side];
	    end[fixed_direction] = fixed_domain[side];
	    start[1 - fixed_direction] = varying_domain.Min();
	    end[1 - fixed_direction] = varying_domain.Max();
	    std::unique_ptr<ON_LineCurve> candidate(
		new ON_LineCurve(start, end));
	    const ON_Interval model_domain = model_curve.Domain();
	    const ON_Surface::ISO expected = fixed_direction == 0 ?
		(side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
		(side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
	    if (!candidate->SetDomain(model_domain.Min(), model_domain.Max()) ||
		    !candidate->IsValid() ||
		    surface.IsIsoparametric(*candidate) != expected)
		continue;
	    result = std::move(candidate);
	}
    }
    return result;
}


void
TrimmedSurfaceBuilder::recover_parameter_curves(
    const ON_Surface &surface, std::vector<std::vector<CurvePair> > &loops,
    const DirectoryEntry &source)
{
    if (!importer_.safe_repairs())
	return;

    const ON_Interval domains[2] = {
	surface.Domain(0), surface.Domain(1)
    };
    if (!domains[0].IsIncreasing() || !domains[1].IsIncreasing())
	return;

    size_t isoparametric_recoveries = 0;
    size_t pullback_recoveries = 0;
    size_t relaxed_recoveries = 0;
    size_t singular_recoveries = 0;
    size_t discarded_recoveries = 0;
    size_t pullback_failures = 0;
    std::string first_pullback_failure;
    const double safe_tolerance = std::max(importer_.tolerance(),
	ON_ZERO_TOLERANCE) * SAFE_TRIM_REPAIR_TOLERANCE_FACTOR;
    const double maximum_tolerance =
	importer_.maximum_trim_repair_tolerance();
    const double tolerance = std::min(safe_tolerance, maximum_tolerance);
    double maximum_relaxed_tolerance = 0.0;
    for (std::vector<CurvePair> &loop : loops) {
	std::array<size_t, 2> periodic_seam_uses = {0, 0};
	for (CurvePair &pair : loop) {
	    if (pair.parameter || pair.singular || !pair.model)
		continue;

	    int matched_direction = -1;
	    int matched_side = -1;
	    bool reverse_parameter = false;
	    bool ambiguous = false;
	    for (int direction = 0; direction < 2 && !ambiguous; ++direction) {
		const ON_Interval &constant_domain = domains[1 - direction];
		for (int side = 0; side < 2; ++side) {
		    const double constant = side == 0 ?
			constant_domain.Min() : constant_domain.Max();
		    std::unique_ptr<ON_Curve> isocurve(
			surface.IsoCurve(direction, constant));
		    bool reversed = false;
		    if (!isocurve || !brep_curves_coincident(*pair.model,
			    *isocurve, tolerance, &reversed))
			continue;
		    if (matched_direction >= 0) {
			const int fixed_direction = 1 - direction;
			const bool periodic_seam_pair =
			    matched_direction == direction && matched_side == 0 &&
			    side == 1 && surface.IsClosed(fixed_direction);
			if (!periodic_seam_pair) {
			    ambiguous = true;
			    break;
			}
			/* Both parameter-domain sides lift to the same closed-surface
			 * seam.  Alternating exact sides preserves the two authored
			 * boundary uses needed by OpenNURBS seam topology. */
			if (periodic_seam_uses[fixed_direction]++ % 2 == 0)
			    continue;
		    }
		    matched_direction = direction;
		    matched_side = side;
		    reverse_parameter = reversed;
		}
	    }
	    if (!ambiguous && matched_direction >= 0) {
		const double constant = matched_side == 0 ?
		    domains[1 - matched_direction].Min() :
		    domains[1 - matched_direction].Max();
		ON_2dPoint start;
		ON_2dPoint end;
		if (matched_direction == 0) {
		    start.Set(domains[0].Min(), constant);
		    end.Set(domains[0].Max(), constant);
		} else {
		    start.Set(constant, domains[1].Min());
		    end.Set(constant, domains[1].Max());
		}
		if (reverse_parameter)
		    std::swap(start, end);
		std::unique_ptr<ON_LineCurve> parameter(
		    new ON_LineCurve(start, end));
		const ON_Interval model_domain = pair.model->Domain();
		if (parameter->SetDomain(model_domain.Min(), model_domain.Max()) &&
			parameter->IsValid()) {
		    pair.parameter = std::move(parameter);
		    ++isoparametric_recoveries;
		    importer_.count_repair();
		    continue;
		}
	    }

	    std::string failure_reason;
	    PullbackFailureReason failure = PullbackFailureReason::None;
	    pair.parameter.reset(brlcad::pullback_curve(&surface,
		pair.model.get(), tolerance, tolerance, &failure_reason,
		&failure));
	    if (!pair.parameter &&
		    failure == PullbackFailureReason::ParameterCurveCollapsed) {
		pair.parameter = collapsed_singular_parameter_curve(surface,
		    *pair.model, tolerance);
		if (pair.parameter) {
		    pair.singular = true;
		    ++singular_recoveries;
		    importer_.count_repair();
		    continue;
		}
		pair.discard = true;
		++discarded_recoveries;
		importer_.count_repair();
		continue;
	    }

	    double attempted_tolerance = tolerance;
	    while (!pair.parameter &&
		    failure == PullbackFailureReason::ProjectionFailed &&
		    attempted_tolerance < maximum_tolerance) {
		const double next_tolerance = std::min(maximum_tolerance,
		    attempted_tolerance * RELAXED_TRIM_TOLERANCE_STEP_FACTOR);
		if (!(next_tolerance > attempted_tolerance))
		    break;
		attempted_tolerance = next_tolerance;
		failure_reason.clear();
		pair.parameter.reset(brlcad::pullback_curve(&surface,
		    pair.model.get(), attempted_tolerance, attempted_tolerance,
		    &failure_reason, &failure));
	    }
	    if (!pair.parameter) {
		++pullback_failures;
		if (first_pullback_failure.empty())
		    first_pullback_failure = failure_reason;
		continue;
	    }
	    if (attempted_tolerance > tolerance) {
		pair.repair_tolerance = attempted_tolerance;
		maximum_relaxed_tolerance = std::max(maximum_relaxed_tolerance,
		    attempted_tolerance);
		relaxed_tolerances_[source.id] = std::max(
		    relaxed_tolerances_[source.id], attempted_tolerance);
		++relaxed_recoveries;
	    }
	    ++pullback_recoveries;
	    importer_.count_repair();
	}
	loop.erase(std::remove_if(loop.begin(), loop.end(),
	    [](const CurvePair &pair) { return pair.discard; }), loop.end());
    }
    if (isoparametric_recoveries > 0) {
	std::ostringstream message;
	message << "recovered " << isoparametric_recoveries
	    << " missing parameter-space boundaries from base-surface isocurves";
	importer_.diagnose(Severity::Information,
	    "recovered_isoparametric_boundary", message.str(), &source);
    }
    if (pullback_recoveries > 0) {
	std::ostringstream message;
	message << "recovered " << pullback_recoveries
	    << " missing parameter-space boundaries by bounded pullback";
	importer_.diagnose(Severity::Information,
	    "recovered_parameter_curve", message.str(), &source);
    }
    if (relaxed_recoveries > 0) {
	std::ostringstream message;
	message << "recovered " << relaxed_recoveries
	    << " boundary curves using an explicitly relaxed tolerance up to "
	    << maximum_relaxed_tolerance << " mm";
	importer_.diagnose(Severity::Warning, "relaxed_parameter_curve",
	    message.str(), &source);
    }
    if (singular_recoveries > 0) {
	std::ostringstream message;
	message << "recovered " << singular_recoveries
	    << " collapsed model-space boundaries as singular trims";
	importer_.diagnose(Severity::Information,
	    "recovered_singular_boundary", message.str(), &source);
    }
    if (discarded_recoveries > 0) {
	std::ostringstream message;
	message << "discarded " << discarded_recoveries
	    << " model-space boundary segments whose validated pullbacks "
	    << "collapsed within the safe repair tolerance";
	importer_.diagnose(Severity::Information,
	    "discarded_collapsed_boundary", message.str(), &source);
    }
    if (pullback_failures > 0) {
	std::ostringstream message;
	message << "bounded pullback could not recover " << pullback_failures
	    << " parameter-space boundaries";
	if (!first_pullback_failure.empty())
	    message << "; first failure: " << first_pullback_failure;
	importer_.diagnose(Severity::Warning,
	    "parameter_curve_pullback", message.str(), &source);
    }
}

bool
TrimmedSurfaceBuilder::add_face(const DirectoryEntry &entry)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    EntityId surface_id;
    SolidBuilder geometry(importer_, entry);
    std::vector<std::vector<CurvePair> > loops;
    if (entry.type == 144) {
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
	loops.resize(static_cast<size_t>(inner_count) + 1);
	if (!curve_pairs(geometry, outer_id, loops[0]))
	    return false;
	for (int i = 0; i < inner_count; ++i) {
	    EntityId inner_id;
	    if (!parameter_entity(parameters, static_cast<size_t>(i + 5),
		    inner_id) ||
		    !curve_pairs(geometry, inner_id,
			loops[static_cast<size_t>(i + 1)]))
		return false;
	}
    } else if (entry.type == 143) {
	int boundary_count = 0;
	if (!parameter_entity(parameters, 2, surface_id) ||
		!parameter_integer(parameters, 3, boundary_count) ||
		boundary_count < 1 || boundary_count > MAX_ENTITY_LIST_COUNT) {
	    importer_.diagnose(Severity::Warning, "bounded_surface_parameters",
		"Bounded Surface requires a base surface and at least one boundary",
		&entry);
	    return false;
	}
	loops.resize(static_cast<size_t>(boundary_count));
	for (int i = 0; i < boundary_count; ++i) {
	    EntityId boundary_id;
	    if (!parameter_entity(parameters, static_cast<size_t>(i + 4),
		    boundary_id) ||
		    !bounded_curve_pairs(geometry, boundary_id, surface_id,
			loops[static_cast<size_t>(i)]))
		return false;
	}
    } else {
	importer_.diagnose(Severity::Warning, "bounded_surface_type",
	    "direct surface-boundary import received an unsupported entity",
	    &entry);
	return false;
    }

    const DirectoryEntry *surface_entry = importer_.document().entity(surface_id);
    if (!surface_entry) {
	importer_.diagnose(Severity::Warning, "trimmed_surface_reference",
	    "Trimmed Surface references a missing base surface", &entry);
	return false;
    }
    std::unique_ptr<ON_Surface> surface;
    if (surface_entry->type == 108) {
	std::unique_ptr<ON_PlaneSurface> plane = plane_surface(geometry,
	    *surface_entry, loops);
	surface.reset(plane.release());
    } else if (surface_entry->type == 128) {
	std::unique_ptr<ON_NurbsSurface> nurbs =
	    geometry.nurbs_surface(*surface_entry);
	surface.reset(nurbs.release());
    } else if (surface_entry->type == 118 || surface_entry->type == 120 ||
	    surface_entry->type == 122) {
	std::unique_ptr<ON_NurbsSurface> analytic =
	    geometry.analytic_surface(*surface_entry);
	surface.reset(analytic.release());
    } else {
	importer_.diagnose(Severity::Warning, "unsupported_trimmed_surface",
	    "direct trimmed-surface import does not support this base surface",
	    surface_entry);
	return false;
    }
    if (!surface) {
	importer_.diagnose(Severity::Warning, "invalid_trimmed_surface_geometry",
	    "could not construct the trimmed face's base surface", surface_entry);
	return false;
    }

    recover_parameter_curves(*surface, loops, entry);
    for (const std::vector<CurvePair> &loop : loops)
	for (const CurvePair &pair : loop)
	    if (!pair.parameter) {
		importer_.diagnose(Severity::Warning,
		    "missing_parameter_curve",
		    "non-planar trimmed face has no parameter-space boundary",
		    &entry);
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
	if (!assembly.validation_log.empty())
	    detail << ": " << assembly.validation_log;
	importer_.diagnose(Severity::Warning, "trimmed_surface_assembly",
	    detail.str(), faces_.front());
	return nullptr;
    }
    return std::move(brep_);
}

std::string
Importer::unique_name(const DirectoryEntry &entry) const
{
    return unique_name(entry, source_name(document_, entry));
}

std::string
Importer::unique_name(const DirectoryEntry &entry,
    const std::string &source) const
{
    const auto imported = objects_.find(entry.id);
    if (imported != objects_.end())
	return imported->second;

    std::string stem = sanitized_database_name(source);
    if (stem.empty())
	stem = "iges_geometry_D" + std::to_string(entry.id.value());
    if (db_lookup(wdbp_->dbip, stem.c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
	return stem;

    const std::string collision_stem = stem + ".D" +
	std::to_string(entry.id.value());
    std::string result = collision_stem;
    size_t serial = 1;
    while (db_lookup(wdbp_->dbip, result.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	result = collision_stem + "." + std::to_string(serial++);
    return result;
}

std::string
Importer::unique_name(const std::string &source) const
{
    std::string stem = sanitized_database_name(source);
    if (stem.empty())
	stem = "iges_geometry";

    std::string result = stem;
    size_t serial = 1;
    while (db_lookup(wdbp_->dbip, result.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	result = stem + "." + std::to_string(serial++);
    return result;
}

void
Importer::write_entity_attributes(const std::string &name,
    const DirectoryEntry &entry)
{
    const std::string entity = std::to_string(entry.id.value());
    const std::string type = std::to_string(entry.type);
    const std::string form = std::to_string(entry.form);
    const std::string level = std::to_string(entry.level);
    const std::string color = std::to_string(entry.color);
    const std::string line_font = std::to_string(entry.line_font);
    const std::string line_weight = std::to_string(entry.line_weight);
    const std::string status = std::to_string(entry.status);
    const std::string subscript = std::to_string(entry.subscript);
    db5_update_attribute(name.c_str(), "importer", "iges-g", wdbp_->dbip);
    db5_update_attribute(name.c_str(), "source_format", "iges", wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.entity", entity.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.type", type.c_str(), wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.form", form.c_str(), wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.level", level.c_str(), wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.color", color.c_str(), wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.line_font", line_font.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.line_weight", line_weight.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.status", status.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.subscript", subscript.c_str(),
	wdbp_->dbip);
    const std::string semantic_name = name_property(document_, entry);
    if (!semantic_name.empty())
	db5_update_attribute(name.c_str(), "iges.name", semantic_name.c_str(),
	    wdbp_->dbip);
    if (!entry.label.empty())
	db5_update_attribute(name.c_str(), "iges.label", entry.label.c_str(),
	    wdbp_->dbip);
}

bool
Importer::write_color_attribute(const std::string &name,
    const std::array<unsigned char, 3> &rgb, const DirectoryEntry &entry)
{
    std::ostringstream value;
    value << static_cast<unsigned int>(rgb[0]) << '/'
	<< static_cast<unsigned int>(rgb[1]) << '/'
	<< static_cast<unsigned int>(rgb[2]);
    if (db5_update_attribute(name.c_str(),
	    db5_standard_attribute(ATTR_COLOR), value.str().c_str(),
	    wdbp_->dbip) < 0) {
	diagnose(Severity::Error, "color_attribute",
	    "failed to write the resolved IGES object color", &entry);
	return false;
    }
    return true;
}

bool
Importer::write_entity_color_attribute(const std::string &name,
    const DirectoryEntry &entry)
{
    std::array<unsigned char, 3> rgb;
    return !entity_color(document_, entry, rgb) ||
	write_color_attribute(name, rgb, entry);
}

bool
Importer::write_face_metadata(const std::string &name, const ON_Brep &brep,
    const std::vector<const DirectoryEntry *> &faces,
    const std::map<EntityId, double> &relaxed_tolerances)
{
    const DirectoryEntry *source = faces.empty() ? nullptr : faces.front();
    if (brep.m_F.Count() != static_cast<int>(faces.size())) {
	diagnose(Severity::Error, "face_metadata_mapping",
	    "assembled B-Rep face count does not match its IGES source map",
	    source);
	return false;
    }

    std::ostringstream metadata;
    metadata << '[' << std::setprecision(
	std::numeric_limits<double>::max_digits10);
    bool uniform_color = true;
    bool have_uniform_color = false;
    std::array<unsigned char, 3> uniform_rgb = {0, 0, 0};
    for (size_t face_index = 0; face_index < faces.size(); ++face_index) {
	const DirectoryEntry *face = faces[face_index];
	if (!face ||
		brep.m_F[static_cast<int>(face_index)].m_face_user.i !=
		    face->id.value()) {
	    diagnose(Severity::Error, "face_metadata_mapping",
		"assembled B-Rep face order does not match its IGES source map",
		source);
	    return false;
	}

	std::array<unsigned char, 3> rgb;
	const bool have_color = entity_color(document_, *face, rgb);
	if (face_index == 0) {
	    have_uniform_color = have_color;
	    if (have_color)
		uniform_rgb = rgb;
	} else if (have_color != have_uniform_color ||
		(have_color && rgb != uniform_rgb)) {
	    uniform_color = false;
	}

	if (face_index > 0)
	    metadata << ',';
	metadata << "{\"face\":" << face_index
	    << ",\"entity\":" << face->id.value()
	    << ",\"type\":" << face->type
	    << ",\"form\":" << face->form
	    << ",\"level\":" << face->level
	    << ",\"color\":" << face->color
	    << ",\"subscript\":" << face->subscript;
	const std::string face_name = semantic_name(document_, *face);
	if (!face_name.empty())
	    metadata << ",\"name\":\"" << json_escape(face_name) << '"';
	if (have_color)
	    metadata << ",\"rgb\":["
		<< static_cast<unsigned int>(rgb[0]) << ','
		<< static_cast<unsigned int>(rgb[1]) << ','
		<< static_cast<unsigned int>(rgb[2]) << ']';
	const auto relaxed = relaxed_tolerances.find(face->id);
	if (relaxed != relaxed_tolerances.end())
	    metadata << ",\"repair_tolerance_mm\":" << relaxed->second;
	metadata << '}';
    }
    metadata << ']';
    if (db5_update_attribute(name.c_str(), "iges.face_metadata",
	    metadata.str().c_str(), wdbp_->dbip) < 0) {
	diagnose(Severity::Error, "face_metadata_attribute",
	    "failed to preserve IGES per-face metadata", source);
	return false;
    }
    return !uniform_color || !have_uniform_color ||
	write_color_attribute(name, uniform_rgb, *source);
}

bool
Importer::write_plate_mode_attributes(const std::string &name,
    const ON_Brep &brep, const DirectoryEntry &entry)
{
    if (options_.default_plate_thickness <= 0.0 || brep.IsSolid())
	return true;

    std::ostringstream value;
    value << std::setprecision(std::numeric_limits<double>::max_digits10)
	<< options_.default_plate_thickness;
    if (db5_update_attribute(name.c_str(), "_plate_mode_thickness",
	    value.str().c_str(), wdbp_->dbip) < 0) {
	diagnose(Severity::Error, "plate_mode_attribute",
	    "failed to assign the requested default plate thickness", &entry);
	return false;
    }
    ++result_.statistics.plate_mode_objects_thickened;
    return true;
}

bool
Importer::write_trimmed_component(
    const std::vector<const DirectoryEntry *> &faces)
{
    brep_assembly_result assembly;
    TrimmedSurfaceBuilder builder(*this, faces);
    std::unique_ptr<ON_Brep> brep = builder.build(assembly);
    if (!brep)
	return false;

    const DirectoryEntry &source = *faces.front();
    const std::string name = unique_name(source);
    if (mk_brep(wdbp_, name.c_str(), brep.get()) < 0) {
	diagnose(Severity::Error, "brep_write",
	    "failed to write assembled OpenNURBS B-Rep", &source);
	return false;
    }
    if (!write_plate_mode_attributes(name, *brep, source))
	return false;
    write_entity_attributes(name, source);
    const std::map<EntityId, double> &relaxed_tolerances =
	builder.relaxed_tolerances();
    if (!write_face_metadata(name, *brep, faces, relaxed_tolerances))
	return false;
    const std::string face_count = std::to_string(faces.size());
    const std::string merged_edges = std::to_string(assembly.merged_edges);
    const std::string naked_edges =
	std::to_string(assembly.remaining_naked_edges);
    db5_update_attribute(name.c_str(), "iges.face_count", face_count.c_str(),
	wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.merged_edges",
	merged_edges.c_str(), wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.naked_edges",
	naked_edges.c_str(), wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.topology",
	"direct-opennurbs-component", wdbp_->dbip);
    if (!relaxed_tolerances.empty()) {
	double maximum_tolerance = 0.0;
	for (const auto &repair : relaxed_tolerances) {
	    maximum_tolerance = std::max(maximum_tolerance, repair.second);
	    count_relaxed_face(repair.second);
	}
	std::ostringstream value;
	value << std::setprecision(std::numeric_limits<double>::max_digits10)
	    << maximum_tolerance;
	db5_update_attribute(name.c_str(), "iges.tolerance_status", "relaxed",
	    wdbp_->dbip);
	db5_update_attribute(name.c_str(), "iges.maximum_repair_tolerance_mm",
	    value.str().c_str(), wdbp_->dbip);
	value.str(std::string());
	value.clear();
	value << std::setprecision(std::numeric_limits<double>::max_digits10)
	    << tolerance_;
	db5_update_attribute(name.c_str(), "iges.nominal_tolerance_mm",
	    value.str().c_str(), wdbp_->dbip);
	db5_update_attribute(name.c_str(), "iges.tolerance_basis",
	    source_resolution_declared_ ? "source_resolution" : "import_default",
	    wdbp_->dbip);
    }
    for (const DirectoryEntry *face : faces)
	objects_[face->id] = name;
    root_objects_.insert(name);
    ++result_.statistics.breps_written;
    ++result_.statistics.components_written;
    return true;
}

void
Importer::import_trimmed_components(
    const std::vector<const DirectoryEntry *> &faces)
{
    if (faces.empty())
	return;
    const size_t diagnostic_count = result_.diagnostics.size();
    const size_t repair_count = result_.statistics.repairs;
    if (write_trimmed_component(faces))
	return;

    const bool write_error = std::any_of(
	result_.diagnostics.begin() +
	    static_cast<std::ptrdiff_t>(diagnostic_count),
	result_.diagnostics.end(), [](const ImportDiagnostic &diagnostic) {
	    return diagnostic.severity == Severity::Error ||
		diagnostic.severity == Severity::Fatal;
	});
    if (faces.size() == 1 || write_error) {
	result_.statistics.repairs = repair_count;
	result_.diagnostics.erase(std::remove_if(
	    result_.diagnostics.begin() +
		static_cast<std::ptrdiff_t>(diagnostic_count),
	    result_.diagnostics.end(), [](const ImportDiagnostic &diagnostic) {
		return diagnostic.severity == Severity::Information;
	    }), result_.diagnostics.end());
	result_.statistics.omitted += faces.size();
	return;
    }

    result_.diagnostics.resize(diagnostic_count);
    result_.statistics.repairs = repair_count;
    const size_t middle = faces.size() / 2;
    const std::vector<const DirectoryEntry *> first(faces.begin(),
	faces.begin() + static_cast<std::ptrdiff_t>(middle));
    const std::vector<const DirectoryEntry *> second(
	faces.begin() + static_cast<std::ptrdiff_t>(middle), faces.end());
    import_trimmed_components(first);
    import_trimmed_components(second);
}

bool
Importer::write_standalone_surface(const DirectoryEntry &entry)
{
    SolidBuilder geometry(*this, entry, Matrix());
    std::unique_ptr<ON_NurbsSurface> surface = entry.type == 128 ?
	geometry.nurbs_surface(entry) : geometry.analytic_surface(entry);
    if (!surface) {
	diagnose(Severity::Warning, "invalid_standalone_surface",
	    "could not construct a finite OpenNURBS surface", &entry);
	return false;
    }

    std::unique_ptr<ON_Brep> brep(ON_Brep::New());
    if (!brep) {
	diagnose(Severity::Error, "brep_allocation",
	    "could not allocate an OpenNURBS B-Rep", &entry);
	return false;
    }
    brep->NewFace(*surface);
    brep->SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, true, true);
    ON_wString messages;
    ON_TextLog log(messages);
    if (!brep->IsValid(&log)) {
	ON_String text(messages);
	diagnose(Severity::Warning, "invalid_standalone_brep",
	    std::string("OpenNURBS rejected the finite surface domain: ") +
	    (text.Array() ? text.Array() : "no detail"), &entry);
	return false;
    }

    const std::string name = unique_name(entry);
    if (mk_brep(wdbp_, name.c_str(), brep.get()) < 0) {
	diagnose(Severity::Error, "brep_write",
	    "failed to write a standalone OpenNURBS surface", &entry);
	return false;
    }
    if (!write_plate_mode_attributes(name, *brep, entry))
	return false;
    write_entity_attributes(name, entry);
    if (!write_entity_color_attribute(name, entry))
	return false;
    db5_update_attribute(name.c_str(), "iges.topology",
	"direct-opennurbs-surface", wdbp_->dbip);
    objects_[entry.id] = name;
    root_objects_.insert(name);
    ++result_.statistics.breps_written;
    return true;
}

void
Importer::combination_matrix(const Matrix &source, mat_t result) const
{
    MAT_IDN(result);
    for (size_t row = 0; row < 3; ++row)
	for (size_t column = 0; column < 4; ++column)
	    result[row * 4 + column] = column == 3 ?
		source.m[row][column] * unit_to_mm_ : source.m[row][column];
}

bool
Importer::container_members(const DirectoryEntry &entry,
    std::vector<EntityId> &members) const
{
    const ParameterList *parameters = document_.parameters(entry.id);
    int count = 0;
    size_t first_member = 0;
    if (entry.type == 402 &&
	    (entry.form == 1 || entry.form == 7 || entry.form == 9)) {
	if (!parameter_integer(parameters, 1, count))
	    return false;
	first_member = 2;
    } else if (entry.type == 308) {
	if (!parameter_integer(parameters, 3, count))
	    return false;
	first_member = 4;
    } else if (entry.type == 184) {
	if (!parameter_integer(parameters, 1, count))
	    return false;
	first_member = 2;
    } else {
	return false;
    }
    if (count < 0 || count > MAX_ENTITY_LIST_COUNT)
	return false;
    members.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
	EntityId member;
	if (!parameter_entity(parameters, first_member + static_cast<size_t>(i),
		member))
	    return false;
	members.push_back(member);
    }
    return true;
}

std::string
Importer::hierarchy_name(const DirectoryEntry &entry) const
{
    const std::string property = name_property(document_, entry);
    std::string source = property;
    if (source.empty() && entry.type == 308) {
	const ParameterList *parameters = document_.parameters(entry.id);
	parameter_string(parameters, 2, source);
    }
    if (source.empty())
	source = entry.label;
    if (!source.empty() && property.empty() && entry.subscript != 0 &&
	    entry.type != 180 && entry.type != 184 &&
	    (entry.type != 430 || source == entry.label))
	source += "." + std::to_string(entry.subscript);
    if (source.empty()) {
	const char *kind = entry.type == 308 ? "subfigure" :
	    (entry.type == 184 ? "assembly" :
	     (entry.type == 430 ? "solid_instance" : "group"));
	source = std::string("iges_") + kind + "_D" +
	    std::to_string(entry.id.value());
    }
    return source;
}

bool
Importer::write_boolean_tree(EntityId id, std::set<EntityId> &active)
{
    if (objects_.find(id) != objects_.end() ||
	    deferred_boolean_trees_.find(id) != deferred_boolean_trees_.end())
	return true;
    const DirectoryEntry *entry = document_.entity(id);
    const ParameterList *parameters = entry ? document_.parameters(id) : nullptr;
    int token_count = 0;
    if (!entry || entry->type != 180 ||
	    !parameter_integer(parameters, 1, token_count) || token_count < 1 ||
	    token_count > MAX_ENTITY_LIST_COUNT) {
	diagnose(Severity::Warning, "boolean_tree_parameters",
	    "Boolean Tree has an invalid postfix token count", entry);
	++result_.statistics.omitted;
	return true;
    }
    if (!active.insert(id).second) {
	diagnose(Severity::Warning, "hierarchy_cycle",
	    "cyclic IGES Boolean Tree reference was omitted", entry);
	++result_.statistics.omitted;
	return true;
    }

    std::vector<union tree *> stack;
    std::set<std::string> operands;
    const auto release_stack = [&]() {
	for (union tree *node : stack)
	    db_free_tree(node);
	stack.clear();
    };
    const auto abandon = [&](const char *message) {
	release_stack();
	active.erase(id);
	diagnose(Severity::Warning, "boolean_tree_structure", message, entry);
	++result_.statistics.omitted;
	return true;
    };

    mat_t leaf_matrix;
    combination_matrix(transform(entry->transform), leaf_matrix);
    for (int i = 0; i < token_count; ++i) {
	int token = 0;
	if (!parameter_integer(parameters, static_cast<size_t>(i + 2), token) ||
		token == 0)
	    return abandon("Boolean Tree contains an invalid token");
	if (token < 0) {
	    const EntityId operand_id(-static_cast<int64_t>(token));
	    auto object = objects_.find(operand_id);
	    const DirectoryEntry *operand = document_.entity(operand_id);
	    if (object == objects_.end() && operand &&
		    (operand->type == 180 || operand->type == 430)) {
		const bool written = operand->type == 180 ?
		    write_boolean_tree(operand_id, active) :
		    write_solid_instance(*operand, active);
		if (!written) {
		    release_stack();
		    active.erase(id);
		    return false;
		}
		object = objects_.find(operand_id);
	    }
	    if (object == objects_.end() && operand &&
		    (is_native_csg_entity_type(operand->type) ||
		     (operand->type == 180 &&
		      deferred_boolean_trees_.find(operand_id) !=
			  deferred_boolean_trees_.end()) ||
		     (operand->type == 430 &&
		      deferred_instances_.find(operand_id) !=
			  deferred_instances_.end()))) {
		/* The legacy solid converter constructs exact native primitives and
		 * will subsequently emit this Boolean tree. */
		release_stack();
		active.erase(id);
		deferred_boolean_trees_.insert(id);
		return true;
	    }
	    if (object == objects_.end())
		return abandon("Boolean Tree references geometry that was not imported");
	    union tree *leaf;
	    BU_ALLOC(leaf, union tree);
	    RT_TREE_INIT(leaf);
	    leaf->tr_l.tl_op = OP_DB_LEAF;
	    leaf->tr_l.tl_name = bu_strdup(object->second.c_str());
	    leaf->tr_l.tl_mat = static_cast<matp_t>(
		bu_malloc(sizeof(mat_t), "IGES Boolean Tree leaf matrix"));
	    MAT_COPY(leaf->tr_l.tl_mat, leaf_matrix);
	    stack.push_back(leaf);
	    operands.insert(object->second);
	    continue;
	}
	if ((token != 1 && token != 2 && token != 3) || stack.size() < 2)
	    return abandon("Boolean Tree postfix operators are unbalanced");
	union tree *operation;
	BU_ALLOC(operation, union tree);
	RT_TREE_INIT(operation);
	operation->tr_b.tb_op = token == 1 ? OP_UNION :
	    (token == 2 ? OP_INTERSECT : OP_SUBTRACT);
	operation->tr_b.tb_right = stack.back();
	stack.pop_back();
	operation->tr_b.tb_left = stack.back();
	stack.pop_back();
	stack.push_back(operation);
    }
    if (stack.size() != 1)
	return abandon("Boolean Tree postfix expression has unused operands");

    struct rt_comb_internal *combination;
    BU_ALLOC(combination, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(combination);
    combination->tree = stack.back();
    stack.clear();
    const std::string name = unique_name(*entry, hierarchy_name(*entry));
    if (wdb_export(wdbp_, name.c_str(), combination, ID_COMBINATION, 1.0)) {
	active.erase(id);
	diagnose(Severity::Error, "boolean_tree_write",
	    "failed to write an IGES Boolean Tree", entry);
	return false;
    }
    active.erase(id);
    write_entity_attributes(name, *entry);
    db5_update_attribute(name.c_str(), "iges.semantic", "boolean_tree",
	wdbp_->dbip);
    objects_[id] = name;
    for (const std::string &operand : operands)
	root_objects_.erase(operand);
    root_objects_.insert(name);
    ++result_.statistics.groups_written;
    return true;
}

bool
Importer::write_container(EntityId id, std::set<EntityId> &active)
{
    if (objects_.find(id) != objects_.end())
	return true;
    const DirectoryEntry *entry = document_.entity(id);
    if (!entry)
	return false;
    if (!active.insert(id).second) {
	diagnose(Severity::Warning, "hierarchy_cycle",
	    "cyclic IGES group or subfigure reference was omitted", entry);
	return false;
    }

    std::vector<EntityId> source_members;
    if (!container_members(*entry, source_members)) {
	active.erase(id);
	diagnose(Severity::Warning, "hierarchy_parameters",
	    "IGES group or subfigure has invalid member parameters", entry);
	return true;
    }

    struct wmember members;
    BU_LIST_INIT(&members.l);
    std::set<std::string> output_members;
    std::ostringstream member_order;
    size_t unresolved = 0;
    for (size_t member_index = 0; member_index < source_members.size();
	    ++member_index) {
	const EntityId member_id = source_members[member_index];
	if (member_order.tellp() > 0)
	    member_order << ',';
	member_order << member_id.value();
	auto object = objects_.find(member_id);
	if (object == objects_.end()) {
	    const DirectoryEntry *member_entry = document_.entity(member_id);
	    if (member_entry && (member_entry->type == 180 ||
		    member_entry->type == 184 ||
		    member_entry->type == 308 ||
		    (member_entry->type == 402 &&
		     (member_entry->form == 1 || member_entry->form == 7 ||
		      member_entry->form == 9)))) {
		const bool written = member_entry->type == 180 ?
		    write_boolean_tree(member_id, active) :
		    write_container(member_id, active);
		if (!written) {
		    active.erase(id);
		    return false;
		}
		object = objects_.find(member_id);
	    }
	}
	if (object == objects_.end()) {
	    ++unresolved;
	    continue;
	}
	const bool new_output_member =
	    output_members.insert(object->second).second;
	if (entry->type != 184 && !new_output_member)
	    continue;
	struct wmember *output_member = mk_addmember(object->second.c_str(),
	    &members.l, nullptr, WMOP_UNION);
	if (output_member == WMEMBER_NULL) {
	    active.erase(id);
	    diagnose(Severity::Error, "hierarchy_member",
		"failed to add a member to an IGES group", entry);
	    return false;
	}
	if (entry->type == 184) {
	    const ParameterList *parameters = document_.parameters(entry->id);
	    EntityId matrix_id;
	    const size_t matrix_parameter =
		2 + source_members.size() + member_index;
	    if (parameter_entity(parameters, matrix_parameter, matrix_id)) {
		mat_t placement;
		combination_matrix(transform(matrix_id), placement);
		MAT_COPY(output_member->wm_mat, placement);
	    }
	}
    }
    active.erase(id);
    if (output_members.empty())
	return true;

    const std::string name = unique_name(*entry, hierarchy_name(*entry));
    const int write_status = mk_lfcomb(wdbp_, name.c_str(), &members, 0);
    if (write_status < 0) {
	diagnose(Severity::Error, "hierarchy_write",
	    "failed to write an IGES group or subfigure", entry);
	return false;
    }
    write_entity_attributes(name, *entry);
    const char *semantic = entry->type == 184 ? "solid_assembly" :
	(entry->type == 308 ? "subfigure_definition" :
	 (entry->form == 9 ? "ordered_group" : "unordered_group"));
    const std::string unresolved_count = std::to_string(unresolved);
    db5_update_attribute(name.c_str(), "iges.semantic", semantic, wdbp_->dbip);
    if (entry->type == 308) {
	std::string source_name;
	if (parameter_string(document_.parameters(entry->id), 2, source_name) &&
		!source_name.empty())
	    db5_update_attribute(name.c_str(), "iges.name", source_name.c_str(),
		wdbp_->dbip);
    }
    db5_update_attribute(name.c_str(), "iges.member_order",
	member_order.str().c_str(), wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.unresolved_members",
	unresolved_count.c_str(), wdbp_->dbip);
    objects_[id] = name;
    for (const std::string &member : output_members)
	root_objects_.erase(member);
    root_objects_.insert(name);
    ++result_.statistics.groups_written;
    return true;
}

bool
Importer::write_instance_combination(const DirectoryEntry &entry,
    EntityId definition_id, const Matrix &placement, const std::string &stem,
    const char *semantic)
{
    const auto definition = objects_.find(definition_id);
    if (definition == objects_.end())
	return false;

    mat_t matrix;
    combination_matrix(placement, matrix);
    struct wmember members;
    BU_LIST_INIT(&members.l);
    struct wmember *member = mk_addmember(definition->second.c_str(),
	&members.l, nullptr, WMOP_UNION);
    if (member == WMEMBER_NULL) {
	diagnose(Severity::Error, "instance_member",
	    "failed to add an IGES instance member", &entry);
	return false;
    }
    MAT_COPY(member->wm_mat, matrix);

    const std::string name = unique_name(entry, stem);
    InstanceProperties properties;
    if (entry.type == 430)
	solid_instance_properties(document_, entry, properties);
    const char *shader_name = properties.shader_name.empty() ? nullptr :
	properties.shader_name.c_str();
    const char *shader_arguments = properties.shader_arguments.empty() ?
	nullptr : properties.shader_arguments.c_str();
    const unsigned char *color = properties.has_color ?
	properties.color.data() : nullptr;
    const int write_status = mk_lrcomb(wdbp_, name.c_str(), &members,
	properties.region_flag, shader_name, shader_arguments, color,
	properties.ident, properties.air, properties.material,
	properties.line_of_sight, properties.inherit);
    if (write_status < 0) {
	diagnose(Severity::Error, "instance_write",
	    "failed to write an IGES instance", &entry);
	return false;
    }
    write_entity_attributes(name, entry);
    const std::string definition_entity =
	std::to_string(definition_id.value());
    db5_update_attribute(name.c_str(), "iges.semantic", semantic, wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.definition",
	definition_entity.c_str(), wdbp_->dbip);
    if (properties.source_entity > 0) {
	const std::string property_entity =
	    std::to_string(properties.source_entity);
	db5_update_attribute(name.c_str(), "iges.attribute_entity",
	    property_entity.c_str(), wdbp_->dbip);
    }
    objects_[entry.id] = name;
    root_objects_.erase(definition->second);
    root_objects_.insert(name);
    ++result_.statistics.groups_written;
    return true;
}


bool
Importer::write_instance(const DirectoryEntry &entry,
    std::set<EntityId> &active)
{
    const ParameterList *parameters = document_.parameters(entry.id);
    EntityId definition_id;
    if (!parameter_entity(parameters, 1, definition_id)) {
	diagnose(Severity::Warning, "subfigure_instance_parameters",
	    "Subfigure Instance has no valid definition reference", &entry);
	return true;
    }
    if (!write_container(definition_id, active))
	return false;
    const auto definition = objects_.find(definition_id);
    if (definition == objects_.end()) {
	diagnose(Severity::Warning, "subfigure_instance_unresolved",
	    "Subfigure Instance definition contains no imported geometry", &entry);
	return true;
    }

    double scale_factor = 1.0;
    Point3 translation = {0.0, 0.0, 0.0};
    double value = 0.0;
    for (size_t coordinate = 0; coordinate < translation.size(); ++coordinate)
	if (parameter_real(parameters, coordinate + 2, value))
	    translation[coordinate] = value;
    if (parameter_real(parameters, 5, value))
	scale_factor = value;
    if (!std::isfinite(scale_factor) ||
	    std::fabs(scale_factor) <= DEGENERATE_DOMAIN_TOLERANCE) {
	diagnose(Severity::Warning, "subfigure_instance_scale",
	    "Subfigure Instance has an invalid scale", &entry);
	return true;
    }

    Matrix placement;
    for (size_t axis = 0; axis < 3; ++axis) {
	placement.m[axis][axis] = scale_factor;
	placement.m[axis][3] = translation[axis];
    }
    placement = multiply(transform(entry.transform), placement);
    std::string stem = hierarchy_name(entry);
    if (entry.label.empty())
	stem = definition->second + ".instance_D" +
	    std::to_string(entry.id.value());
    return write_instance_combination(entry, definition_id, placement, stem,
	"subfigure_instance");
}

bool
Importer::write_solid_instance(const DirectoryEntry &entry,
    std::set<EntityId> &active)
{
    if (objects_.find(entry.id) != objects_.end() ||
	    deferred_instances_.find(entry.id) != deferred_instances_.end())
	return true;
    const ParameterList *parameters = document_.parameters(entry.id);
    EntityId definition_id;
    if (entry.type != 430 ||
	    !parameter_entity(parameters, 1, definition_id)) {
	diagnose(Severity::Warning, "solid_instance_parameters",
	    "Solid Instance has no valid definition reference", &entry);
	++result_.statistics.omitted;
	return true;
    }
    if (!active.insert(entry.id).second) {
	diagnose(Severity::Warning, "hierarchy_cycle",
	    "cyclic IGES Solid Instance reference was omitted", &entry);
	++result_.statistics.omitted;
	return true;
    }

    const DirectoryEntry *definition_entry = document_.entity(definition_id);
    auto definition = objects_.find(definition_id);
    if (definition == objects_.end() && definition_entry) {
	bool resolved = true;
	if (definition_entry->type == 180)
	    resolved = write_boolean_tree(definition_id, active);
	else if (definition_entry->type == 184 ||
		definition_entry->type == 308 ||
		(definition_entry->type == 402 &&
		 (definition_entry->form == 1 || definition_entry->form == 7 ||
		  definition_entry->form == 9)))
	    resolved = write_container(definition_id, active);
	else if (definition_entry->type == 408)
	    resolved = write_instance(*definition_entry, active);
	else if (definition_entry->type == 430)
	    resolved = write_solid_instance(*definition_entry, active);
	if (!resolved) {
	    active.erase(entry.id);
	    return false;
	}
	definition = objects_.find(definition_id);
    }

    const bool definition_deferred = definition_entry &&
	(is_native_csg_entity_type(definition_entry->type) ||
	 (definition_entry->type == 180 &&
	  deferred_boolean_trees_.find(definition_id) !=
	      deferred_boolean_trees_.end()) ||
	 (definition_entry->type == 430 &&
	  deferred_instances_.find(definition_id) != deferred_instances_.end()));
    if (definition == objects_.end() && definition_deferred) {
	active.erase(entry.id);
	deferred_instances_.insert(entry.id);
	return true;
    }
    if (definition == objects_.end()) {
	active.erase(entry.id);
	diagnose(Severity::Warning, "solid_instance_unresolved",
	    "Solid Instance definition contains no imported geometry", &entry);
	++result_.statistics.omitted;
	return true;
    }

    active.erase(entry.id);
    return write_instance_combination(entry, definition_id,
	transform(entry.transform), hierarchy_name(entry), "solid_instance");
}


bool
Importer::write_hierarchy()
{
    std::set<EntityId> active;
    for (const DirectoryEntry &entry : document_.entities())
	if (entry.type == 180 && !write_boolean_tree(entry.id, active))
	    return false;

    for (const DirectoryEntry &entry : document_.entities()) {
	if (entry.type != 184 && entry.type != 308 &&
		(entry.type != 402 ||
		(entry.form != 1 && entry.form != 7 && entry.form != 9)))
	    continue;
	if (!write_container(entry.id, active))
	    return false;
    }
    for (const DirectoryEntry &entry : document_.entities())
	if (entry.type == 408 && !write_instance(entry, active))
	    return false;
    for (const DirectoryEntry &entry : document_.entities())
	if (entry.type == 430 && !write_solid_instance(entry, active))
	    return false;
    return true;
}

bool
Importer::write_root()
{
    if (root_objects_.empty())
	return true;
    const std::string root_stem = options_.root_name.empty() ?
	"iges_geometry" : options_.root_name;
    const std::string name = unique_name(root_stem);
    struct wmember members;
    BU_LIST_INIT(&members.l);
    for (const std::string &member : root_objects_)
	if (mk_addmember(member.c_str(), &members.l, nullptr, WMOP_UNION) ==
		WMEMBER_NULL) {
	    diagnose(Severity::Error, "root_member",
		"failed to add an imported object to the IGES root");
	    return false;
	}
    const int write_status = mk_lfcomb(wdbp_, name.c_str(), &members, 0)
    if (write_status < 0) {
	diagnose(Severity::Error, "root_write",
	    "failed to write the IGES geometry root");
	return false;
    }
    db5_update_attribute(name.c_str(), "importer", "iges-g", wdbp_->dbip);
    db5_update_attribute(name.c_str(), "source_format", "iges", wdbp_->dbip);
    db5_update_attribute(name.c_str(), "iges.semantic", "geometry",
	wdbp_->dbip);
    ++result_.statistics.groups_written;
    return true;
}


BrepImportResult
Importer::run()
{
    if (!wdbp_ || !wdbp_->dbip) {
	diagnose(Severity::Fatal, "output_database",
	    "no writable BRL-CAD database was supplied");
	return result_;
    }
    if (!std::isfinite(options_.default_plate_thickness) ||
	    options_.default_plate_thickness < 0.0) {
	diagnose(Severity::Fatal, "plate_mode_thickness",
	    "default plate thickness must be a finite non-negative value");
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
    const std::vector<const DirectoryEntry *> bounded_surfaces =
	document_.find(143);
    std::vector<const DirectoryEntry *> bounded_faces = trimmed_surfaces;
    bounded_faces.insert(bounded_faces.end(), bounded_surfaces.begin(),
	bounded_surfaces.end());
    std::set<EntityId> referenced_surfaces;
    for (const DirectoryEntry &entry : document_.entities()) {
	if (entry.type != 143 && entry.type != 144 && entry.type != 510)
	    continue;
	EntityId surface;
	const size_t surface_parameter = entry.type == 143 ? 2 : 1;
	if (parameter_entity(document_.parameters(entry.id), surface_parameter,
		surface))
	    referenced_surfaces.insert(surface);
    }
    std::vector<const DirectoryEntry *> standalone_surfaces;
    for (const DirectoryEntry &entry : document_.entities())
	if (is_surface_entity(entry.type) &&
		referenced_surfaces.find(entry.id) == referenced_surfaces.end())
	    standalone_surfaces.push_back(&entry);

    result_.statistics.solids_seen = solids.size();
    result_.statistics.trimmed_surfaces_seen = trimmed_surfaces.size();
    result_.statistics.bounded_surfaces_seen = bounded_surfaces.size();
    result_.statistics.standalone_surfaces_seen = standalone_surfaces.size();
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
	if (!write_plate_mode_attributes(name, *brep, *solid)) {
	    ++result_.statistics.omitted;
	    continue;
	}
	write_entity_attributes(name, *solid);
	if (!write_entity_color_attribute(name, *solid)) {
	    ++result_.statistics.omitted;
	    continue;
	}
	db5_update_attribute(name.c_str(), "iges.topology",
	    "direct-opennurbs", wdbp_->dbip);
	objects_[solid->id] = name;
	root_objects_.insert(name);
	++result_.statistics.breps_written;
    }
    for (const DirectoryEntry *surface : standalone_surfaces) {
	if (!is_supported_standalone_surface(surface->type)) {
	    const bool missing_extent =
		surface->type == 108 || surface->type == 190;
	    diagnose(Severity::Warning, "unsupported_standalone_surface",
		missing_extent ?
		"standalone plane surface has no finite trim extent" :
		"direct import does not yet support this standalone surface representation",
		surface);
	    ++result_.statistics.omitted;
	    continue;
	}
	if (!write_standalone_surface(*surface))
	    ++result_.statistics.omitted;
    }

    std::map<EntityId, std::vector<EntityId> > owners;
    for (const DirectoryEntry &entry : document_.entities()) {
	std::vector<EntityId> members;
	if (!container_members(entry, members))
	    continue;
	for (EntityId member : members) {
	    const DirectoryEntry *member_entry = document_.entity(member);
	    if (member_entry &&
		    (member_entry->type == 143 || member_entry->type == 144))
		owners[member].push_back(entry.id);
	}
    }
    std::map<std::vector<EntityId>,
	std::vector<const DirectoryEntry *> > partitions;
    for (const DirectoryEntry *face : bounded_faces)
	partitions[owners[face->id]].push_back(face);
    for (const auto &partition : partitions)
	import_trimmed_components(partition.second);

    if (!write_hierarchy())
	return result_;
    if (!write_root())
	return result_;
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
	<< ", \"strict\": " << (options.strict ? "true" : "false")
	<< ", \"default_plate_thickness\": "
	<< options.default_plate_thickness
	<< ", \"maximum_repair_tolerance\": "
	<< options.maximum_repair_tolerance << "},\n"
	<< "  \"statistics\": {\"entities_read\": "
	<< result.statistics.entities_read << ", \"solids_seen\": "
	<< result.statistics.solids_seen << ", \"trimmed_surfaces_seen\": "
	<< result.statistics.trimmed_surfaces_seen
	<< ", \"bounded_surfaces_seen\": "
	<< result.statistics.bounded_surfaces_seen
	<< ", \"standalone_surfaces_seen\": "
	<< result.statistics.standalone_surfaces_seen << ", \"breps_written\": "
	<< result.statistics.breps_written << ", \"components_written\": "
	<< result.statistics.components_written << ", \"groups_written\": "
	<< result.statistics.groups_written
	<< ", \"plate_mode_objects_thickened\": "
	<< result.statistics.plate_mode_objects_thickened
	<< ", \"relaxed_faces_written\": "
	<< result.statistics.relaxed_faces_written
	<< ", \"maximum_repair_tolerance_used\": "
	<< result.statistics.maximum_repair_tolerance_used << ", \"omitted\": "
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
    int strict, const char *repair_mode, double default_plate_thickness,
    double maximum_repair_tolerance, const char *root_name,
    const char *report_path)
{
    if (!path || !wdbp)
	return -1;
    ON::Begin();
    const brlcad::iges::Document document =
	brlcad::iges::Document::parse_file(path);
    const bool mixed_csg = std::any_of(document.entities().begin(),
	document.entities().end(), [](const brlcad::iges::DirectoryEntry &entry) {
	    return brlcad::iges::brep_import_detail::
		is_native_csg_entity_type(entry.type);
	});
    brlcad::iges::ImportOptions options;
    options.exact = exact != 0;
    options.strict = strict != 0;
    options.default_plate_thickness = default_plate_thickness;
    options.maximum_repair_tolerance = maximum_repair_tolerance;
    if (repair_mode && BU_STR_EQUAL(repair_mode, "none"))
	options.repair = brlcad::iges::RepairMode::None;
    options.root_name = root_name && root_name[0] != '\0' ?
	root_name : "iges_geometry";
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
	 result.statistics.trimmed_surfaces_seen > 0 ||
	 result.statistics.bounded_surfaces_seen > 0 ||
	 result.statistics.standalone_surfaces_seen > 0) &&
	!brlcad::iges::write_brep_import_report(report_path, document, options,
	    result)) {
	bu_log("IGES: unable to write import report %s\n", report_path);
	return -1;
    }
    if (result.success)
	return mixed_csg ? 2 : 1;
    if (!document.valid())
	return -1;
    const bool import_error = std::any_of(result.diagnostics.begin(),
	result.diagnostics.end(),
	[](const brlcad::iges::ImportDiagnostic &diagnostic) {
	    return diagnostic.severity == brlcad::iges::Severity::Error ||
		diagnostic.severity == brlcad::iges::Severity::Fatal;
	});
    const bool direct_entities_seen = result.statistics.solids_seen > 0 ||
	result.statistics.trimmed_surfaces_seen > 0 ||
	result.statistics.bounded_surfaces_seen > 0 ||
	result.statistics.standalone_surfaces_seen > 0;
    return import_error || direct_entities_seen ? -1 : 0;
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
