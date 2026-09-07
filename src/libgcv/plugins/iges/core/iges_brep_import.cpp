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
#include "iges_report.h"
#include "iges_parameters.h"
#include "iges_native.h"
#include "iges_runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <optional>
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
#include "rt/primitives/brep.h"
#include "brep/cdt.h"

namespace brlcad {
namespace iges {
namespace brep_import_detail {

constexpr size_t GLOBAL_MINIMUM_RESOLUTION = 18;
constexpr double DEFAULT_TOPOLOGY_TOLERANCE_MM = 1.0e-6;
constexpr double DEGENERATE_DOMAIN_TOLERANCE = 1.0e-12;
constexpr double CURVE_ENDPOINT_RELATIVE_TOLERANCE = 1.0e-6;
constexpr double CONIC_PARAMETER_TOLERANCE = 1.0e-12;
constexpr int BOUNDARY_VALIDATION_SEGMENTS = 64;
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


using Point3 = std::array<double, 3>;

struct Matrix {
    double m[4][4] = {
	{1.0, 0.0, 0.0, 0.0},
	{0.0, 1.0, 0.0, 0.0},
	{0.0, 0.0, 1.0, 0.0},
	{0.0, 0.0, 0.0, 1.0}
    };
};

struct SolidProperties {
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


struct PropertyList {
    std::vector<EntityId> properties;
    bool valid = true;
};

PropertyList
property_entities(const Document &document, const ParameterList *parameters,
    size_t parameter)
{
    PropertyList result;
    if (!parameters)
	return result;
    // Both trailing lists are optional.  A present count, however, must
    // fit its actual payload; malformed metadata must not shift geometry.
    for (bool properties : {false, true}) {
	if (parameter >= parameters->values.size())
	    return result;
	int count = 0;
	if ((!parameters->values[parameter].empty() && !parameter_integer(parameters, parameter, count)) ||
	    count < 0 || static_cast<size_t>(count) > parameters->values.size() - parameter - 1) {
	    result.valid = false;
	    return result;
	}
	++parameter;
	for (int index = 0; index < count; ++index) {
	    EntityId reference;
	    if (!parameter_entity(parameters, parameter++, reference) || !document.entity(reference)) {
		result.valid = false;
		continue;
	    }
	    if (properties)
		result.properties.push_back(reference);
	}
    }
    return result;
}


static bool
associativity_parameter(const ParameterList *parameters,
    const DirectoryEntry &entry, size_t &parameter)
{
    // Index of the optional associativity list after each fixed solid record.
    switch (entry.type) {
	case 150: case 168: parameter = 13; return true;
	case 152: parameter = 14; return true;
	case 154: case 160: case 162: parameter = 9; return true;
	case 156: parameter = 10; return true;
	case 158: parameter = 5; return true;
	case 164: parameter = 6; return true;
	default: break;
    }
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

    for (EntityId property_id : property_entities(document, parameters, parameter).properties) {
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
solid_properties(const Document &document,
    const DirectoryEntry &entry, SolidProperties &properties)
{
    size_t property_parameter = 0;
    if (!associativity_parameter(document.parameters(entry.id), entry, property_parameter))
	return;
    properties.has_color = entity_color(document, entry, properties.color);
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

    const ParameterList *entity_parameters =
	document.parameters(entry.id);
    for (EntityId property_id : property_entities(document, entity_parameters,
	    property_parameter).properties) {
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


class Importer;

struct CurvePair {
    std::unique_ptr<ON_Curve> parameter;
    std::unique_ptr<ON_Curve> model;
    double repair_tolerance = 0.0;
    bool singular = false;
    bool discard = false;
};

static ON_BoundingBox
model_bounds(const std::vector<CurvePair> &pairs)
{
    ON_BoundingBox bounds;
    for (const CurvePair &pair : pairs)
	if (pair.model)
	    bounds.Union(pair.model->BoundingBox());
    return bounds;
}

static void recover_parameter_curves(Importer &importer,
    const ON_Surface &surface, std::vector<std::vector<CurvePair> > &loops,
    const DirectoryEntry &source, std::map<EntityId, double> &relaxed_tolerances);
static void repair_parameter_loop(Importer &importer, const ON_Surface &surface,
    std::vector<CurvePair> &pairs, const DirectoryEntry &source,
    std::map<EntityId, double> &relaxed_tolerances, bool adaptive = false);

enum class FaceRecovery {
    None,
    ModelBoundaries,
    TrimEndpoints
};

static const char *
face_recovery_name(FaceRecovery recovery)
{
    switch (recovery) {
	case FaceRecovery::None: return "none";
	case FaceRecovery::ModelBoundaries: return "model_boundaries";
	case FaceRecovery::TrimEndpoints: return "adaptive_trim_endpoints";
    }
    return "unknown";
}

struct TrimmedComponent {
    std::unique_ptr<ON_Brep> brep;
    std::vector<const DirectoryEntry *> faces;
    std::map<EntityId, double> relaxed_tolerances;
    std::map<EntityId, FaceRecovery> recoveries;
};

class SolidBuilder {
public:
    SolidBuilder(Importer &importer, const DirectoryEntry &solid);
    SolidBuilder(Importer &importer, const DirectoryEntry &solid,
	const Matrix &solid_transform);

    std::unique_ptr<ON_Brep> build();
    std::unique_ptr<ON_Brep> preserve_faces();
    const std::string &shell_metadata() const { return shell_metadata_; }
    size_t missing_faces() const { return missing_faces_; }
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
    std::unique_ptr<ON_Surface> analytic_surface(
	const DirectoryEntry &entry);
    const std::map<EntityId, double> &relaxed_tolerances() const
    {
	return relaxed_tolerances_;
    }

private:
    bool finish_geometry();
    std::unique_ptr<ON_ArcCurve> circular_arc(const DirectoryEntry &entry);
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
	const ON_Xform *parameter_projection, const ON_Curve *model);
    std::unique_ptr<ON_Surface> face_surface(EntityId id,
	const std::vector<LoopRecord> &loops, ON_Xform &parameter_projection,
	bool &has_parameter_projection);
    bool analytic_frame(const DirectoryEntry &entry, ON_Plane &plane,
	ON_Xform &placement);
    bool loop_bounds(const std::vector<LoopRecord> &loops, ON_BoundingBox &bounds);
    std::unique_ptr<ON_NurbsSurface> ruled_surface(
	const DirectoryEntry &entry, const Matrix &parent);
    std::unique_ptr<ON_RevSurface> revolution_surface(
	const DirectoryEntry &entry, const Matrix &parent);
    std::unique_ptr<ON_NurbsSurface> tabulated_surface(
	const DirectoryEntry &entry, const Matrix &parent);
    std::unique_ptr<ON_Surface> plane_surface(EntityId id,
	const std::vector<LoopRecord> &loops, ON_Xform &parameter_projection);
    std::unique_ptr<ON_RevSurface> cylindrical_surface(EntityId id,
	const std::vector<LoopRecord> &loops);

    Importer &importer_;
    const DirectoryEntry &solid_;
    Matrix solid_transform_;
    std::unique_ptr<ON_Brep> brep_;
    std::map<VertexKey, int> vertices_;
    std::map<EdgeKey, int> edges_;
    std::map<EntityId, double> relaxed_tolerances_;
    std::string shell_metadata_;
    size_t missing_faces_ = 0;

    friend class TrimmedSurfaceBuilder;
};

class TrimmedSurfaceBuilder {
public:
    TrimmedSurfaceBuilder(Importer &importer,
	const std::vector<const DirectoryEntry *> &faces,
	FaceRecovery recovery = FaceRecovery::None);

    std::unique_ptr<ON_Brep> build(brep_assembly_result &assembly);
    const std::map<EntityId, double> &relaxed_tolerances() const
    {
	return relaxed_tolerances_;
    }

private:
    bool append_curve_entities(EntityId id, std::vector<EntityId> &curves,
	std::set<EntityId> &active);
    std::unique_ptr<ON_Curve> curve(SolidBuilder &geometry, EntityId id,
	bool model_space);
    bool curve_pairs(SolidBuilder &geometry, EntityId boundary,
	const ON_Surface *surface, std::vector<CurvePair> &pairs);
    bool singular_curve(CurvePair &pair, const ON_Surface &surface,
	const DirectoryEntry &source);
    bool match_curve_segments(SolidBuilder &geometry,
	const std::vector<EntityId> &model_entities, const ON_Surface &surface,
	std::vector<CurvePair> &pairs);
    void resolve_revolution_parameters(SolidBuilder &geometry, ON_Surface &surface,
	const std::vector<EntityId> &boundaries);
    bool bounded_curve_pairs(SolidBuilder &geometry, EntityId boundary,
	EntityId surface, std::vector<CurvePair> &pairs);
    bool add_loop(ON_BrepFace &face, ON_BrepLoop::TYPE type,
	std::vector<CurvePair> &pairs, const DirectoryEntry &source);
    std::unique_ptr<ON_PlaneSurface> plane_surface(SolidBuilder &geometry,
	const DirectoryEntry &surface_entry,
	std::vector<std::vector<CurvePair> > &loops);
    bool add_face(const DirectoryEntry &entry);

    Importer &importer_;
    const std::vector<const DirectoryEntry *> &faces_;
    FaceRecovery recovery_;
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
    bool read_profile(EntityId id, ProfileCurves &curves, const Matrix &parent,
	std::set<EntityId> &active);
    bool register_native(EntityId id, const char *name);
    BrepImportResult complete_hierarchy();
    void progress(const char *activity, EntityId entity = EntityId()) const
    {
	if (options_.progress)
	    options_.progress("geometry", activity, progress_completed_, progress_total_, entity.value());
    }
    const Document &document() const { return document_; }
    double tolerance() const { return tolerance_; }
    double source_tolerance() const
    {
	return unit_to_mm_ > 0.0 ? tolerance_ / unit_to_mm_ : tolerance_;
    }
    bool safe_repairs() const
    {
	return options_.repair != RepairMode::None && !options_.exact &&
	    !options_.strict;
    }
    bool best_effort_repairs() const
    {
	return safe_repairs() && options_.repair == RepairMode::BestEffort &&
	    options_.maximum_repair_tolerance <= 0.0;
    }
    double maximum_trim_repair_tolerance(const ON_BoundingBox &bounds = ON_BoundingBox()) const
    {
	const double safe_limit = std::max(tolerance_, ON_ZERO_TOLERANCE) *
	    SAFE_TRIM_REPAIR_TOLERANCE_FACTOR;
	if (options_.maximum_repair_tolerance > 0.0)
	    return options_.maximum_repair_tolerance;
	if (!safe_repairs() || !bounds.IsValid())
	    return safe_limit;
	const double relative_limit = bounds.Diagonal().Length() * options_.relative_tolerance;
	return std::isfinite(relative_limit) ? std::max(safe_limit, relative_limit) : safe_limit;
    }
    const char *repair_tolerance_description() const
    {
	return options_.maximum_repair_tolerance > 0.0 ?
	    "explicitly relaxed" : "object-relative";
    }
    void count_relaxed_face(double tolerance)
    {
	++result_.statistics.relaxed_faces_written;
	result_.statistics.maximum_repair_tolerance_used = std::max(
	    result_.statistics.maximum_repair_tolerance_used, tolerance);
    }
    void count_repair() { ++result_.statistics.repairs; }
    std::vector<EntityId> plane_holes(EntityId plane) const
    {
	const auto found = plane_holes_.find(plane);
	return found == plane_holes_.end() ? std::vector<EntityId>() : found->second;
    }
    Matrix transform(EntityId id);
    Point3 model_point(const DirectoryEntry &entry, const Point3 &point,
	const Matrix &parent);
    Point3 model_vector(const DirectoryEntry &entry, const Point3 &vector,
	const Matrix &parent);
    ON_Xform model_placement(const DirectoryEntry &entry, const Matrix &parent);
    void diagnose(Severity severity, const char *code,
	const std::string &message, const DirectoryEntry *entry = nullptr);

private:
    Matrix transform(EntityId id, std::set<EntityId> &active);
    BrepOrientationResult orient_geometry(const std::string &name, ON_Brep &brep);
    bool write_geometry(const std::string &name, ON_Brep &brep, bool incomplete_solid = false,
	BrepOrientationResult *prepared_orientation = nullptr);
    void count_geometry(const ON_Brep &brep, bool invalid_solid = false);
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
	const std::map<EntityId, double> &relaxed_tolerances,
	const std::map<EntityId, FaceRecovery> &recoveries = {});
    bool write_plate_mode_attributes(const std::string &name,
	const ON_Brep &brep, const DirectoryEntry &entry);
    bool write_repair_attributes(const std::string &name,
	const std::map<EntityId, double> &relaxed_tolerances,
	const DirectoryEntry &entry,
	const std::map<EntityId, FaceRecovery> &recoveries = {});
    bool build_trimmed_component(
	const std::vector<const DirectoryEntry *> &faces,
	TrimmedComponent &component,
	FaceRecovery recovery = FaceRecovery::None);
    bool write_trimmed_component(const TrimmedComponent &component);
    bool write_trimmed_geometry(const TrimmedComponent &component, const std::string &name);
    void collect_trimmed_components(
	const std::vector<const DirectoryEntry *> &faces,
	std::vector<TrimmedComponent> &components);
    void write_trimmed_components(TrimmedComponent &component);
    void import_trimmed_components(
	const std::vector<const DirectoryEntry *> &faces);
    void discard_failed_repairs(size_t first_diagnostic, size_t repair_count);
    bool write_standalone_surface(const DirectoryEntry &entry);
    void combination_matrix(const Matrix &source, mat_t result) const;
    bool container_members(const DirectoryEntry &entry,
	std::vector<EntityId> &members) const;
    bool write_container(EntityId id, std::set<EntityId> &active);
    bool resolve_hierarchy_object(EntityId id, std::set<EntityId> &active);
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
    size_t progress_completed_ = 0;
    size_t progress_total_ = 0;
    double unit_to_mm_ = DEFAULT_UNIT_TO_MM;
    double tolerance_ = DEFAULT_TOPOLOGY_TOLERANCE_MM;
    bool source_resolution_declared_ = false;
    std::map<EntityId, Matrix> transforms_;
    std::map<EntityId, std::vector<EntityId> > plane_holes_;
    std::map<EntityId, std::string> objects_;
    std::set<std::string> root_objects_;
    std::set<EntityId> unresolved_objects_;

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

ON_Xform
Importer::model_placement(const DirectoryEntry &entry, const Matrix &parent)
{
    const Matrix matrix = multiply(parent, transform(entry.transform));
    ON_Xform placement(ON_Xform::IdentityTransformation);
    for (int row = 0; row < 3; ++row)
	for (int column = 0; column < 4; ++column)
	    placement[row][column] = unit_to_mm_ * matrix.m[row][column];
    return placement;
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

    if (!plane.IsValid() || !ellipse.IsValid())
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

    const double closure_tolerance = std::max(DEGENERATE_DOMAIN_TOLERANCE,
	std::max(radius_x, radius_y) * CONIC_PARAMETER_TOLERANCE);
    const bool complete = start.DistanceTo(end) <= closure_tolerance;
    double sweep = complete ? 2.0 * ON_PI : end_angle - start_angle;
    if (sweep <= 0.0)
	sweep += 2.0 * ON_PI;
    /* Build just the requested arc.  Relocating a full ellipse's seam
     * near an existing knot can create an invalid microscopic span. */
    const ON_Arc arc(ON_Circle(ON_Plane::World_xy, 1.0),
	ON_Interval(start_angle, start_angle + sweep));
    std::unique_ptr<ON_NurbsCurve> result(new ON_NurbsCurve());
    ON_Xform placement(ON_Xform::IdentityTransformation);
    for (int coordinate = 0; coordinate < 3; ++coordinate) {
	placement[coordinate][0] = radius_x * x_axis[coordinate];
	placement[coordinate][1] = radius_y * y_axis[coordinate];
	placement[coordinate][3] = center[coordinate];
    }
    if (!arc.IsValid() || !arc.GetNurbForm(*result) ||
	!result->Transform(placement) || !result->IsValid())
	return nullptr;

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

    std::unique_ptr<ON_ArcCurve> arc = circular_arc(entry);
    std::unique_ptr<ON_NurbsCurve> result(ON_NurbsCurve::New());
    if (!arc || !result || !arc->GetNurbForm(*result))
	return nullptr;
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

std::unique_ptr<ON_ArcCurve>
SolidBuilder::circular_arc(const DirectoryEntry &entry)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
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
    if (!arc.IsValid()) {
	importer_.diagnose(Severity::Warning, "invalid_arc_curve",
	    "could not construct the exact rational circular arc", &entry);
	return nullptr;
    }
    double start_angle = std::atan2(start_y - center_y, start_x - center_x);
    if (start_angle < 0.0)
	start_angle += 2.0 * ON_PI;
    return std::unique_ptr<ON_ArcCurve>(new ON_ArcCurve(arc, start_angle,
	start_angle + angle));
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
    if (start_index < 0 || end_index < 0)
	return -1;
    const ON_3dPoint start = brep_->m_V[start_index].Point();
    const ON_3dPoint end = brep_->m_V[end_index].Point();
    std::unique_ptr<ON_Curve> curve = edge_curve(record.curve, start, end);
    if (!curve || !curve->IsValid())
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

bool
SolidBuilder::analytic_frame(const DirectoryEntry &entry, ON_Plane &plane,
    ON_Xform &placement)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    EntityId point_id;
    EntityId normal_id;
    if ((entry.form != 0 && entry.form != 1) ||
	!parameter_entity(parameters, 1, point_id) ||
	!parameter_entity(parameters, 2, normal_id))
	return false;
    const DirectoryEntry *point_entry = importer_.document().entity(point_id);
    const DirectoryEntry *normal_entry = importer_.document().entity(normal_id);
    if (!point_entry || point_entry->type != 116 ||
	!normal_entry || normal_entry->type != 123)
	return false;
    Point3 origin;
    Point3 normal;
    for (size_t coordinate = 0; coordinate < 3; ++coordinate)
	if (!parameter_real(importer_.document().parameters(point_id),
		coordinate + 1, origin[coordinate]) ||
	    !parameter_real(importer_.document().parameters(normal_id),
		coordinate + 1, normal[coordinate]))
	    return false;
    origin = apply_point(importer_.transform(point_entry->transform), origin);
    normal = apply_vector(importer_.transform(normal_entry->transform), normal);
    if (!normalize(normal))
	return false;
    plane = ON_Plane(ON_3dPoint(origin.data()), ON_3dVector(normal.data()));
    if (entry.form == 1) {
	EntityId reference_id;
	const size_t reference_parameter = entry.type == 190 ? 3 : 4;
	if (!parameter_entity(parameters, reference_parameter, reference_id))
	    return false;
	const DirectoryEntry *reference = importer_.document().entity(reference_id);
	if (!reference || reference->type != 123)
	    return false;
	Point3 direction;
	for (size_t coordinate = 0; coordinate < 3; ++coordinate)
	    if (!parameter_real(importer_.document().parameters(reference_id),
		    coordinate + 1, direction[coordinate]))
		return false;
	direction = apply_vector(importer_.transform(reference->transform), direction);
	ON_3dVector xaxis(direction.data());
	xaxis -= ON_DotProduct(xaxis, plane.zaxis) * plane.zaxis;
	if (!xaxis.Unitize())
	    return false;
	plane = ON_Plane(plane.origin, xaxis, ON_CrossProduct(plane.zaxis, xaxis));
    }
    placement = importer_.model_placement(entry, solid_transform_);
    return plane.IsValid() && placement.IsValid();
}

bool
SolidBuilder::loop_bounds(const std::vector<LoopRecord> &loops, ON_BoundingBox &bounds)
{
    bounds.Destroy();
    for (const LoopRecord &loop : loops)
	for (const EdgeUse &use : loop.uses) {
	    ON_3dPoint start;
	    ON_3dPoint end;
	    if (!edge_use_points(use, start, end))
		return false;
	    if (use.vertex_use) {
		bounds.Set(start, true);
	    } else {
		const int index = edge_index(use.edge);
		if (index < 0 || !bounds.Union(brep_->m_E[index].BoundingBox()))
		    return false;
	    }
	}
    return bounds.IsValid();
}

std::unique_ptr<ON_Surface>
SolidBuilder::plane_surface(EntityId id,
    const std::vector<LoopRecord> &loops, ON_Xform &parameter_projection)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    ON_Plane plane;
    ON_Xform placement;
    ON_BoundingBox bounds;
    if (!entry || !analytic_frame(*entry, plane, placement) || !loop_bounds(loops, bounds))
	return nullptr;
    ON_Xform inverse = placement;
    if (!inverse.Invert())
	return nullptr;
    ON_Xform projection(ON_Xform::IdentityTransformation);
    const ON_3dVector axes[3] = {plane.xaxis, plane.yaxis, plane.zaxis};
    for (int row = 0; row < 3; ++row) {
	for (int column = 0; column < 3; ++column)
	    projection[row][column] = axes[row][column];
	projection[row][3] = -ON_DotProduct(axes[row], plane.origin);
    }
    parameter_projection = projection * inverse;
    ON_3dPointArray corners;
    if (!bounds.GetCorners(corners))
	return nullptr;
    ON_BoundingBox parameter_bounds;
    for (int i = 0; i < corners.Count(); ++i)
	parameter_bounds.Set(parameter_projection * corners[i], true);
    const ON_Interval u(parameter_bounds.m_min.x, parameter_bounds.m_max.x);
    const ON_Interval v(parameter_bounds.m_min.y, parameter_bounds.m_max.y);
    if (!u.IsIncreasing() || !v.IsIncreasing())
	return nullptr;
    ON_PlaneSurface parameter_plane(plane);
    if (!parameter_plane.SetExtents(0, u, true) ||
	!parameter_plane.SetExtents(1, v, true))
	return nullptr;
    /* A skewed parameter frame cannot be represented by the orthogonal
     * axes of ON_PlaneSurface.  A bilinear NURBS preserves that mapping. */
    std::unique_ptr<ON_Surface> surface(placement.IsSimilarity(DEGENERATE_DOMAIN_TOLERANCE) ?
	parameter_plane.DuplicateSurface() : parameter_plane.NurbsSurface());
    if (!surface || !surface->Transform(placement) || !surface->IsValid())
	return nullptr;
    return surface;
}

std::unique_ptr<ON_RevSurface>
SolidBuilder::cylindrical_surface(EntityId id, const std::vector<LoopRecord> &loops)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    ON_Plane plane;
    ON_Xform placement;
    ON_BoundingBox bounds;
    double radius = 0.0;
    if (!entry || !parameter_real(importer_.document().parameters(id), 3, radius) ||
	radius <= 0.0 || !analytic_frame(*entry, plane, placement) ||
	!loop_bounds(loops, bounds))
	return nullptr;
    /* The angular parameter must remain exact.  A general affine image
     * would require an elliptic, not circular, cylinder. */
    if (!placement.IsSimilarity(DEGENERATE_DOMAIN_TOLERANCE)) {
	importer_.diagnose(Severity::Warning, "cylinder_transform",
	    "cylindrical surface has a non-similarity placement", entry);
	return nullptr;
    }
    ON_Xform inverse = placement;
    ON_3dPointArray corners;
    if (!inverse.Invert() || !bounds.GetCorners(corners))
	return nullptr;
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < corners.Count(); ++i) {
	const double height = ON_DotProduct(inverse * corners[i] - plane.origin, plane.zaxis);
	minimum = std::min(minimum, height);
	maximum = std::max(maximum, height);
    }
    ON_Cylinder cylinder;
    cylinder.circle = ON_Circle(plane, radius);
    cylinder.height[0] = minimum;
    cylinder.height[1] = maximum;
    std::unique_ptr<ON_RevSurface> surface(cylinder.RevSurfaceForm());
    constexpr double FULL_CIRCLE_DEGREES = 360.0;
    if (!surface || !surface->SetDomain(0, 0.0, FULL_CIRCLE_DEGREES) ||
	!surface->Transform(placement) || !surface->IsValid())
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

std::unique_ptr<ON_RevSurface>
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
    std::unique_ptr<ON_Curve> generatrix =
	curve(*generatrix_entry, true, parent);
    if (!axis || !generatrix)
	return nullptr;
    if (generatrix->BoundingBox().Diagonal().Length() <=
	    DEGENERATE_DOMAIN_TOLERANCE) {
	importer_.diagnose(Severity::Warning, "degenerate_revolution_generatrix",
	    "surface of revolution has a generating curve collapsed to a point",
	    &entry);
	return nullptr;
    }
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

    if (generatrix_entry->type == 100) {
	std::unique_ptr<ON_ArcCurve> arc = circular_arc(*generatrix_entry);
	const ON_ArcCurve original = arc ? *arc : ON_ArcCurve();
	const ON_Xform placement = importer_.model_placement(*generatrix_entry, parent);
	/* Transform the known circle directly.  Refitting an arc to its NURBS
	 * form loses accuracy for small radii at large model coordinates. */
	bool faithful = arc && arc->Transform(placement) && arc->IsValid();
	/* OpenNURBS permits an approximate circle under nonuniform scaling.
	 * Do not silently accept that approximation as an exact generatrix. */
	for (int i = 0; faithful && i <= BOUNDARY_VALIDATION_SEGMENTS; ++i) {
	    const double parameter = original.Domain().ParameterAt(
		static_cast<double>(i) / BOUNDARY_VALIDATION_SEGMENTS);
	    const ON_3dPoint expected = placement * original.PointAt(parameter);
	    faithful = expected.IsValid() &&
		arc->PointAt(parameter).DistanceTo(expected) <= importer_.tolerance();
	}
	if (!faithful) {
	    importer_.diagnose(Severity::Warning, "revolution_arc_parameters",
		"could not preserve the circular generatrix's angular parameterization",
		generatrix_entry);
	    return nullptr;
	}
	generatrix = std::move(arc);
    }

    /* IGES uses the generating curve parameter first and the rotation
     * angle second.  A NURBS conversion preserves the surface locus but
     * changes its angular parameterization, invalidating authored trims. */
    std::unique_ptr<ON_RevSurface> surface(ON_RevSurface::New());
    if (!surface)
	return nullptr;
    surface->m_curve = generatrix.release();
    surface->m_axis = ON_Line(axis_start, axis_end);
    surface->m_angle = ON_Interval(start_angle, end_angle);
    surface->m_t = surface->m_angle;
    surface->m_bTransposed = true;
    return surface->IsValid() ? std::move(surface) : nullptr;
}

std::unique_ptr<ON_Surface>
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
    const std::vector<LoopRecord> &loops, ON_Xform &parameter_projection,
    bool &has_parameter_projection)
{
    const DirectoryEntry *entry = importer_.document().entity(id);
    if (!entry)
	return nullptr;
    if (entry->type == 190) {
	std::unique_ptr<ON_Surface> surface =
	    plane_surface(id, loops, parameter_projection);
	has_parameter_projection = surface != nullptr;
	return surface;
    }
    has_parameter_projection = false;
    if (entry->type == 192)
	return cylindrical_surface(id, loops);
    if (entry->type == 118 || entry->type == 120 || entry->type == 122 ||
	    entry->type == 128) {
	std::unique_ptr<ON_Surface> surface = entry->type == 128 ?
	    nurbs_surface(*entry) : analytic_surface(*entry);
	return std::unique_ptr<ON_Surface>(surface.release());
    }
    importer_.diagnose(Severity::Warning, "unsupported_face_surface",
	"direct manifold import does not support this face surface geometry",
	entry);
    return nullptr;
}

std::unique_ptr<ON_Curve>
SolidBuilder::trim_curve(const EdgeUse &use,
    const ON_Xform *parameter_projection, const ON_Curve *model)
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
    if (!parameter_projection || !model)
	return nullptr;

    /* Project the whole curve.  Endpoint chords erase circular plane
     * boundaries, including the single closed edge of a disk. */
    std::unique_ptr<ON_Curve> curve(model->DuplicateCurve());
    if (!curve || !curve->Transform(*parameter_projection) ||
	    !curve->ChangeDimension(2) || !curve->IsValid())
	return nullptr;
    return curve;
}

bool
SolidBuilder::add_face(EntityId id, bool same_direction,
    bool shell_same_direction)
{
    importer_.progress("constructing explicit solid face", id);
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
	(has_outer_loop != 0 && has_outer_loop != 1) ||
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

    ON_Xform parameter_projection;
    bool has_parameter_projection = false;
    std::unique_ptr<ON_Surface> surface =
	face_surface(surface_id, loops, parameter_projection, has_parameter_projection);
    if (!surface)
	return false;
    std::vector<std::vector<CurvePair> > boundaries(loops.size());
    for (size_t i = 0; i < loops.size(); ++i) {
	boundaries[i].resize(loops[i].uses.size());
	for (size_t j = 0; j < loops[i].uses.size(); ++j) {
	    const EdgeUse &use = loops[i].uses[j];
	    CurvePair &pair = boundaries[i][j];
	    pair.singular = use.vertex_use;
	    if (!use.vertex_use) {
		const int index = edge_index(use.edge);
		if (index < 0)
		    return false;
		pair.model.reset(brep_->m_E[index].DuplicateCurve());
		if (!pair.model || (!use.same_direction && !pair.model->Reverse()))
		    return false;
	    }
	    pair.parameter = trim_curve(use,
		has_parameter_projection ? &parameter_projection : nullptr, pair.model.get());
	    if (!pair.parameter && !use.parameter_curves.empty())
		return false;
	}
    }
    recover_parameter_curves(importer_, *surface, boundaries, *entry,
	relaxed_tolerances_);
    for (std::vector<CurvePair> &boundary : boundaries)
	if (std::all_of(boundary.begin(), boundary.end(),
		[](const CurvePair &pair) { return pair.parameter != nullptr; }))
	    repair_parameter_loop(importer_, *surface, boundary, *entry,
		relaxed_tolerances_);
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
	for (size_t use_index = 0; use_index < loops[loop_index].uses.size(); ++use_index) {
	    const EdgeUse &use = loops[loop_index].uses[use_index];
	    CurvePair &pair = boundaries[loop_index][use_index];
	    if (!pair.parameter || pair.discard || pair.singular != use.vertex_use) {
		importer_.diagnose(Severity::Warning, "missing_parameter_curve",
		    "could not construct a trim consistent with the authored manifold topology", entry);
		return false;
	    }
	    const double tolerance = std::max(importer_.tolerance(), pair.repair_tolerance);
	    std::unique_ptr<ON_Curve> trim = std::move(pair.parameter);
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
		brep_trim.m_tolerance[0] = tolerance;
		brep_trim.m_tolerance[1] = tolerance;
		continue;
	    }
	    const int edge_index_value = edge_index(use.edge);
	    if (edge_index_value < 0)
		return false;
	    ON_BrepEdge &edge = brep_->m_E[edge_index_value];
	    /* A closed edge has the same vertex at both ends; its direction
	     * must come from the edge use, not vertex identity. */
	    const bool reverse_edge = !use.same_direction;
	    ON_BrepTrim &brep_trim = brep_->NewTrim(edge, reverse_edge, loop,
		trim_curve_index);
	    brep_trim.m_tolerance[0] = tolerance;
	    brep_trim.m_tolerance[1] = tolerance;
	    const ON_Interval domain = brep_trim.ProxyCurveDomain();
	    brep_trim.m_iso =
		surface_geometry->IsIsoparametric(*trim_geometry, &domain);
	}
    }
    if (!has_outer_loop) {
	/* OF=false leaves the outer boundary unidentified.  A finite,
	 * connected face needs one enclosing loop in this parameter chart. */
	std::vector<ON_BoundingBox> bounds(face.m_li.Count());
	int outer = 0;
	double largest_area = -1.0;
	for (int i = 0; i < face.m_li.Count(); ++i) {
	    const ON_BrepLoop &loop = brep_->m_L[face.m_li[i]];
	    for (int j = 0; j < loop.m_ti.Count(); ++j)
		bounds[i].Union(brep_->m_T[loop.m_ti[j]].BoundingBox());
	    const ON_3dVector diagonal = bounds[i].Diagonal();
	    const double area = diagonal.x * diagonal.y;
	    if (!bounds[i].IsValid() || !std::isfinite(area))
		return false;
	    if (area > largest_area) {
		largest_area = area;
		outer = i;
	    }
	}
	const int outer_direction = brep_->LoopDirection(brep_->m_L[face.m_li[outer]]);
	if (!outer_direction)
	    return false;
	for (int i = 0; i < face.m_li.Count(); ++i) {
	    ON_BrepLoop &loop = brep_->m_L[face.m_li[i]];
	    const int expected = i == outer ? outer_direction : -outer_direction;
	    if (brep_->LoopDirection(loop) != expected ||
		!bounds[outer].Includes(bounds[i])) {
		importer_.diagnose(Severity::Warning, "unclassified_face_loop",
		    "unidentified face boundaries do not form one enclosing loop", entry);
		return false;
	    }
	    loop.m_type = i == outer ? ON_BrepLoop::outer : ON_BrepLoop::inner;
	    if (outer_direction < 0)
		brep_->FlipLoop(loop);
	}
	/* Reverse both representations to retain the source edge/face sense. */
	if (outer_direction < 0)
	    face.m_bRev = !face.m_bRev;
	if (!brep_->SortFaceLoops(face))
	    return false;
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
	    void_count > MAX_ENTITY_LIST_COUNT) {
	importer_.diagnose(Severity::Warning, "solid_parameters",
	    "Manifold Solid B-Rep has invalid outer-shell parameters", &solid_);
	return nullptr;
    }
    if (!add_shell(outer_shell, orientation != 0)) {
	importer_.diagnose(Severity::Warning, "solid_outer_shell",
	    "could not construct the manifold solid's outer shell", &solid_);
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

    if (!finish_geometry())
	return nullptr;
    return std::move(brep_);
}

bool
SolidBuilder::finish_geometry()
{
    importer_.progress("validating explicit solid topology", solid_.id);
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
	return false;
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
	return false;
    }
    return true;
}

std::unique_ptr<ON_Brep>
SolidBuilder::preserve_faces()
{
    /* A failed builder can contain half-created loops.  Recover each face
     * in isolation and retain only complete, valid face topology.  The
     * source shell map, not inferred closure, defines this object's intent. */
    std::unique_ptr<ON_Brep> retained(ON_Brep::New());
    relaxed_tolerances_.clear();
    missing_faces_ = 0;
    std::ostringstream metadata;
    const ParameterList *parameters = importer_.document().parameters(solid_.id);
    int void_count = 0;
    if (!parameter_integer(parameters, 3, void_count) || void_count < 0 || void_count > MAX_ENTITY_LIST_COUNT)
	void_count = 0;
    metadata << "{\"declared_void_shells\":" << void_count << ",\"shells\":[";
    const size_t available_voids = parameters && parameters->values.size() > 4 ?
	(parameters->values.size() - 4) / 2 : 0;
    void_count = static_cast<int>(std::min(static_cast<size_t>(void_count), available_voids));
    for (int shell_index = 0; shell_index <= void_count; ++shell_index) {
	const size_t shell_parameter = shell_index == 0 ? 1 : 2 + 2 * static_cast<size_t>(shell_index);
	EntityId shell;
	int shell_orientation = 0;
	parameter_entity(parameters, shell_parameter, shell);
	parameter_integer(parameters, shell_parameter + 1, shell_orientation);
	const ParameterList *shell_parameters = importer_.document().parameters(shell);
	const DirectoryEntry *shell_entry = importer_.document().entity(shell);
	int count = 0;
	const bool valid_shell = shell_entry && shell_entry->type == 514 &&
	    parameter_integer(shell_parameters, 1, count) && count >= 0 && count <= MAX_ENTITY_LIST_COUNT;
	if (!valid_shell)
	    count = 0;
	if (shell_index)
	    metadata << ',';
	metadata << "{\"entity\":" << shell.value() << ",\"role\":\""
	    << (shell_index == 0 ? "outer" : "void") << "\",\"orientation\":"
	    << shell_orientation << ",\"available\":" << (valid_shell ? "true" : "false")
	    << ",\"declared_faces\":" << count << ",\"faces\":[";
	const size_t available_faces = shell_parameters && shell_parameters->values.size() > 2 ?
	    (shell_parameters->values.size() - 2) / 2 : 0;
	const int listed_faces = static_cast<int>(std::min(static_cast<size_t>(count), available_faces));
	missing_faces_ += count - listed_faces;
	count = listed_faces;
	for (int i = 0; i < count; ++i) {
	    EntityId face;
	    int orientation = 0;
	    const size_t face_parameter = 2 + 2 * static_cast<size_t>(i);
	    parameter_entity(shell_parameters, face_parameter, face);
	    parameter_integer(shell_parameters, face_parameter + 1, orientation);
	    SolidBuilder isolated(importer_, solid_, solid_transform_);
	    const size_t diagnostic_count = importer_.result_.diagnostics.size();
	    const size_t repair_count = importer_.result_.statistics.repairs;
	    const bool complete = !face.empty() &&
		isolated.add_face(face, orientation != 0, shell_orientation != 0) && isolated.finish_geometry();
	    if (i)
		metadata << ',';
	    metadata << "{\"entity\":" << face.value() << ",\"orientation\":" << orientation
		<< ",\"retained\":" << (complete ? "true" : "false");
	    if (complete) {
		metadata << ",\"face\":" << retained->m_F.Count();
		retained->Append(*isolated.brep_);
		relaxed_tolerances_.insert(isolated.relaxed_tolerances_.begin(), isolated.relaxed_tolerances_.end());
	    } else {
		importer_.discard_failed_repairs(diagnostic_count, repair_count);
		++missing_faces_;
	    }
	    metadata << '}';
	}
	metadata << "]}";
    }
    metadata << "]}";
    shell_metadata_ = metadata.str();
    /* Even an empty B-Rep can retain an explicit solid's identity and its
     * shell map.  Analysis must reject its invalid-solid marker. */
    return retained;
}

TrimmedSurfaceBuilder::TrimmedSurfaceBuilder(Importer &importer,
    const std::vector<const DirectoryEntry *> &faces, FaceRecovery recovery) :
    importer_(importer), faces_(faces), recovery_(recovery), brep_(ON_Brep::New())
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

void
TrimmedSurfaceBuilder::resolve_revolution_parameters(SolidBuilder &geometry,
    ON_Surface &surface, const std::vector<EntityId> &boundaries)
{
    ON_RevSurface *revolution = ON_RevSurface::Cast(&surface);
    if (!revolution || !ON_ArcCurve::Cast(revolution->m_curve))
	return;
    const ON_Interval domain = revolution->Domain(0);
    if (std::fabs(domain.Min()) <= DEGENERATE_DOMAIN_TOLERANCE)
	return;

    ON_RevSurface relative(*revolution);
    if (!relative.SetDomain(0, 0.0, domain.Length()))
	return;
    /* Some exporters measure the circular generatrix parameter from its
     * start point instead of the definition-space X axis.  Resolve that
     * convention only when the authored model boundary supplies evidence. */
    for (EntityId boundary : boundaries) {
	const DirectoryEntry *entry = importer_.document().entity(boundary);
	if (!entry || entry->type != 142)
	    continue;
	const ParameterList *parameters = importer_.document().parameters(boundary);
	EntityId parameter_id;
	EntityId model_id;
	if (!parameter_entity(parameters, 3, parameter_id) ||
		!parameter_entity(parameters, 4, model_id))
	    continue;
	std::vector<EntityId> parameter_entities;
	std::vector<EntityId> model_entities;
	std::set<EntityId> active;
	if (!append_curve_entities(parameter_id, parameter_entities, active) ||
		!append_curve_entities(model_id, model_entities, active))
	    continue;
	for (EntityId parameter_entity : parameter_entities) {
	    std::unique_ptr<ON_Curve> parameter = curve(geometry, parameter_entity, false);
	    if (!parameter)
		continue;
	    const ON_3dPoint first = parameter->PointAtStart();
	    const ON_3dPoint last = parameter->PointAtEnd();
	    const auto endpoint_error = [&](const ON_Surface &candidate,
		const ON_Curve &model) {
		const ON_3dPoint start = candidate.PointAt(first.x, first.y);
		const ON_3dPoint end = candidate.PointAt(last.x, last.y);
		return std::min(std::max(start.DistanceTo(model.PointAtStart()),
			end.DistanceTo(model.PointAtEnd())),
		    std::max(start.DistanceTo(model.PointAtEnd()),
			end.DistanceTo(model.PointAtStart())));
	    };
	    for (EntityId model_entity : model_entities) {
		std::unique_ptr<ON_Curve> model = curve(geometry, model_entity, true);
		if (!model)
		    continue;
		const double tolerance = std::max(importer_.tolerance(),
		    model->BoundingBox().Diagonal().Length() *
			CURVE_ENDPOINT_RELATIVE_TOLERANCE);
		const double original_error = endpoint_error(*revolution, *model);
		const double relative_error = endpoint_error(relative, *model);
		if (!std::isfinite(original_error) || !std::isfinite(relative_error))
		    continue;
		if (original_error <= tolerance)
		    return;
		if (relative_error <= tolerance) {
		    if (revolution->SetDomain(0, 0.0, domain.Length()))
			importer_.diagnose(Severity::Information,
			    "relative_revolution_parameters",
			    "model boundaries identify a circular parameter measured from the arc start",
			    &geometry.solid_);
		    return;
		}
	    }
	}
    }
}

bool
TrimmedSurfaceBuilder::singular_curve(CurvePair &pair,
    const ON_Surface &surface, const DirectoryEntry &source)
{
    if (!pair.parameter)
	return false;
    int side = -1;
    switch (surface.IsIsoparametric(*pair.parameter)) {
	case ON_Surface::S_iso: side = 0; break;
	case ON_Surface::E_iso: side = 1; break;
	case ON_Surface::N_iso: side = 2; break;
	case ON_Surface::W_iso: side = 3; break;
	default: return false;
    }
    const bool exact_pole = surface.IsSingular(side);
    if (!exact_pole && !importer_.safe_repairs())
	return false;

    const int fixed_direction = side == 0 || side == 2 ? 1 : 0;
    const ON_Interval domain = surface.Domain(fixed_direction);
    const double boundary = side == 0 || side == 3 ? domain.Min() : domain.Max();
    const double safe_limit = std::max(importer_.tolerance(),
	ON_ZERO_TOLERANCE) * SAFE_TRIM_REPAIR_TOLERANCE_FACTOR;
    double pole_diameter = 0.0;
    if (!exact_pole) {
	/* Rounded control points can defeat the exact pole test.  Bound the
	 * entire side, not just coincident endpoints, before collapsing it. */
	std::unique_ptr<ON_Curve> isocurve(
	    surface.IsoCurve(1 - fixed_direction, boundary));
	if (!isocurve || !isocurve->IsValid())
	    return false;
	pole_diameter = isocurve->BoundingBox().Diagonal().Length();
	if (!std::isfinite(pole_diameter) ||
		pole_diameter > std::min(safe_limit,
		    importer_.maximum_trim_repair_tolerance()))
	    return false;
    }
    ON_Xform projection(ON_Xform::IdentityTransformation);
    projection[fixed_direction][fixed_direction] = 0.0;
    projection[fixed_direction][3] = boundary;
    std::unique_ptr<ON_Curve> candidate(pair.parameter->DuplicateCurve());
    if (!candidate || !candidate->Transform(projection) || !candidate->IsValid())
	return false;

    double maximum_movement = pole_diameter;
    bool changed = false;
    const ON_Interval curve_domain = pair.parameter->Domain();
    for (int i = 0; i <= BOUNDARY_VALIDATION_SEGMENTS; ++i) {
	const double parameter = curve_domain.ParameterAt(
	    static_cast<double>(i) / BOUNDARY_VALIDATION_SEGMENTS);
	const ON_3dPoint before = pair.parameter->PointAt(parameter);
	const ON_3dPoint after = candidate->PointAt(parameter);
	const ON_3dPoint before_model = surface.PointAt(before.x, before.y);
	const ON_3dPoint after_model = surface.PointAt(after.x, after.y);
	if (!before.IsValid() || !after.IsValid() ||
		!before_model.IsValid() || !after_model.IsValid())
	    return false;
	changed = changed || std::fabs(before[fixed_direction] - boundary) >
	    DEGENERATE_DOMAIN_TOLERANCE;
	maximum_movement = std::max(maximum_movement,
	    before_model.DistanceTo(after_model) + pole_diameter);
    }
    if (changed || !exact_pole) {
	if (!importer_.safe_repairs() ||
		maximum_movement > importer_.maximum_trim_repair_tolerance())
	    return false;
	importer_.count_repair();
	if (exact_pole)
	    importer_.diagnose(Severity::Information, "snapped_singular_boundary",
		"aligned a near-pole parameter boundary with the exact surface pole",
		&source);
	else
	    importer_.diagnose(Severity::Information, "approximated_singular_boundary",
		"recognized a rounded surface pole using a bounded isocurve diameter",
		&source);
	pair.repair_tolerance = maximum_movement;
	if (maximum_movement > safe_limit) {
	    relaxed_tolerances_[source.id] = std::max(
		relaxed_tolerances_[source.id], maximum_movement);
	}
	pair.parameter = std::move(candidate);
    }
    return true;
}

bool
TrimmedSurfaceBuilder::match_curve_segments(SolidBuilder &geometry,
    const std::vector<EntityId> &model_entities, const ON_Surface &surface,
    std::vector<CurvePair> &pairs)
{
    if (!importer_.safe_repairs())
	return false;
    std::vector<std::unique_ptr<ON_NurbsCurve> > models;
    ON_BoundingBox bounds;
    for (EntityId id : model_entities) {
	const DirectoryEntry *entry = importer_.document().entity(id);
	if (!entry)
	    return false;
	models.push_back(geometry.curve(*entry, true, geometry.solid_transform_));
	if (!models.back())
	    return false;
	bounds.Union(models.back()->BoundingBox());
    }
    const double tolerance = importer_.maximum_trim_repair_tolerance(bounds);
    std::vector<std::vector<ON_Interval> > coverage(models.size());
    double maximum_deviation = 0.0;
    for (CurvePair &pair : pairs) {
	if (pair.singular)
	    continue;
	if (!pair.parameter)
	    return false;
	const ON_3dPoint start = pair.parameter->PointAtStart();
	const ON_3dPoint end = pair.parameter->PointAtEnd();
	const ON_3dPoint model_start = surface.PointAt(start.x, start.y);
	const ON_3dPoint model_end = surface.PointAt(end.x, end.y);
	if (!model_start.IsValid() || !model_end.IsValid())
	    return false;
	int matched = -1;
	bool ambiguous = false;
	ON_Interval interval;
	bool reversed = false;
	double matched_deviation = 0.0;
	for (size_t i = 0; i < models.size(); ++i) {
	    double first = 0.0;
	    double last = 0.0;
	    if (!ON_NurbsCurve_GetClosestPoint(&first, models[i].get(),
		    model_start, tolerance) ||
		    !ON_NurbsCurve_GetClosestPoint(&last, models[i].get(),
			model_end, tolerance) ||
		    std::fabs(first - last) <= DEGENERATE_DOMAIN_TOLERANCE)
		continue;
	    const ON_Interval candidate(std::min(first, last), std::max(first, last));
	    bool coincident = true;
	    double deviation = 0.0;
	    double previous_parameter = first;
	    const ON_Interval parameter_domain = pair.parameter->Domain();
	    for (int sample = 0; sample <= BOUNDARY_VALIDATION_SEGMENTS; ++sample) {
		const ON_3dPoint uv = pair.parameter->PointAt(parameter_domain.ParameterAt(
		    static_cast<double>(sample) / BOUNDARY_VALIDATION_SEGMENTS));
		const ON_3dPoint lifted = surface.PointAt(uv.x, uv.y);
		double parameter = 0.0;
		if (!lifted.IsValid() || !ON_NurbsCurve_GetClosestPoint(&parameter,
			models[i].get(), lifted, tolerance, &candidate)) {
		    coincident = false;
		    break;
		}
		const double distance = lifted.DistanceTo(models[i]->PointAt(parameter));
		const double progress = last > first ? parameter - previous_parameter :
		    previous_parameter - parameter;
		if (!std::isfinite(distance) || distance > tolerance ||
			progress < -DEGENERATE_DOMAIN_TOLERANCE * candidate.Length()) {
		    coincident = false;
		    break;
		}
		previous_parameter = parameter;
		deviation = std::max(deviation, distance);
	    }
	    if (!coincident)
		continue;
	    /* A larger allowance may admit nearby but less accurate curves.
	     * Select the uniquely closest match instead of losing a boundary
	     * that was unambiguous at a smaller tolerance. */
	    if (matched >= 0) {
		if (std::fabs(deviation - matched_deviation) <= importer_.tolerance()) {
		    ambiguous = true;
		    continue;
		}
		if (deviation > matched_deviation)
		    continue;
	    }
	    matched = static_cast<int>(i);
	    ambiguous = false;
	    interval = candidate;
	    reversed = last < first;
	    matched_deviation = deviation;
	}
	if (matched < 0 || ambiguous)
	    return false;
	pair.model.reset(models[matched]->DuplicateCurve());
	if (!pair.model || !pair.model->Trim(interval) ||
		(reversed && !pair.model->Reverse()) || !pair.model->IsValid())
	    return false;
	coverage[matched].push_back(interval);
	pair.repair_tolerance = std::max(pair.repair_tolerance, matched_deviation);
	maximum_deviation = std::max(maximum_deviation, matched_deviation);
    }
    /* Every authored model curve must be covered once.  Endpoint proximity
     * alone must not silently drop a span or introduce duplicate geometry. */
    for (size_t i = 0; i < models.size(); ++i) {
	auto &intervals = coverage[i];
	if (intervals.empty())
	    return false;
	std::sort(intervals.begin(), intervals.end(),
	    [](const ON_Interval &left, const ON_Interval &right) {
		return left.Min() < right.Min();
	    });
	double previous = models[i]->Domain().Min();
	for (const ON_Interval &interval : intervals) {
	    if (models[i]->PointAt(previous).DistanceTo(
		    models[i]->PointAt(interval.Min())) > tolerance)
		return false;
	    previous = interval.Max();
	}
	if (models[i]->PointAt(previous).DistanceTo(models[i]->PointAtEnd()) > tolerance)
	    return false;
    }
    const DirectoryEntry &source = geometry.solid_;
    const double safe_limit = std::max(importer_.tolerance(),
	ON_ZERO_TOLERANCE) * SAFE_TRIM_REPAIR_TOLERANCE_FACTOR;
    if (maximum_deviation > safe_limit)
	relaxed_tolerances_[source.id] = std::max(
	    relaxed_tolerances_[source.id], maximum_deviation);
    importer_.count_repair();
    importer_.diagnose(Severity::Information, "matched_boundary_segments",
	"matched differently segmented boundaries by bounded model-space comparison",
	&source);
    return true;
}

bool
TrimmedSurfaceBuilder::curve_pairs(SolidBuilder &geometry, EntityId boundary,
    const ON_Surface *surface, std::vector<CurvePair> &pairs)
{
    const DirectoryEntry *entry = importer_.document().entity(boundary);
    const ParameterList *parameters = entry ?
	importer_.document().parameters(boundary) : nullptr;
    EntityId parameter_curve;
    EntityId model_curve;
    const bool have_parameter_curve = recovery_ != FaceRecovery::ModelBoundaries &&
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
    const bool different_counts = have_parameter_curve &&
	parameter_entities.size() != model_entities.size();
    const auto cardinality_error = [&]() {
	importer_.diagnose(Severity::Warning, "boundary_curve_cardinality",
	    "parameter- and model-space Composite Curves have different member counts",
	    entry);
	return false;
    };
    if (different_counts &&
	    (!surface || parameter_entities.size() < model_entities.size()))
	return cardinality_error();

    const size_t count = have_parameter_curve ?
	parameter_entities.size() : model_entities.size();
    pairs.resize(count);
    size_t singular_count = 0;
    for (size_t i = 0; i < count; ++i) {
	CurvePair &pair = pairs[i];
	if (have_parameter_curve)
	    pair.parameter = curve(geometry, parameter_entities[i], false);
	if (different_counts && pair.parameter) {
	    /* A parameter boundary along a surface pole has no 3D edge.
	     * Some writers omit it from the model Composite Curve entirely. */
	    pair.singular = singular_curve(pair, *surface, geometry.solid_);
	    if (pair.singular)
		++singular_count;
	}
    }
    if (different_counts && count - singular_count != model_entities.size())
	return match_curve_segments(geometry, model_entities, *surface, pairs) ||
	    cardinality_error();

    size_t model_index = 0;
    for (size_t i = 0; i < count; ++i) {
	CurvePair &pair = pairs[i];
	if (pair.singular)
	    continue;
	if (model_index >= model_entities.size())
	    return cardinality_error();
	const EntityId model_id = model_entities[model_index++];
	pair.singular = have_parameter_curve &&
	    parameter_entities[i] == model_id;
	if (!pair.singular)
	    pair.model = curve(geometry, model_id, true);
	const bool invalid_parameter =
	    pair.parameter && pair.parameter->Dimension() != 2;
	const bool invalid_model = !pair.singular &&
	    (!pair.model || pair.model->Dimension() != 3);
	if (invalid_parameter || invalid_model) {
	    const EntityId failed_id = invalid_parameter ?
		parameter_entities[i] : model_id;
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
    }
    if (model_index != model_entities.size())
	return cardinality_error();
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
	    if (recovery_ == FaceRecovery::ModelBoundaries)
		continue;
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

static bool
curve_pair_points(const CurvePair &pair, const ON_Surface &surface,
    double tolerance, ON_3dPoint &start, ON_3dPoint &end)
{
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
	const ON_3dPoint point = surface.PointAt(parameter.x, parameter.y);
	if (!parameter.IsValid() || !point.IsValid())
	    return false;
	if (!have_point) {
	    collapsed = point;
	    have_point = true;
	} else if (collapsed.DistanceTo(point) >
		std::max(tolerance, pair.repair_tolerance)) {
	    return false;
	}
    }
    start = collapsed;
    end = collapsed;
    return have_point;
}

static double
parameter_curve_movement(const ON_Surface &surface, const ON_Curve &original,
    const ON_Curve &candidate)
{
    double movement = 0.0;
    for (int i = 0; i <= BOUNDARY_VALIDATION_SEGMENTS; ++i) {
	const double fraction = static_cast<double>(i) / BOUNDARY_VALIDATION_SEGMENTS;
	const ON_3dPoint before = original.PointAt(original.Domain().ParameterAt(fraction));
	const ON_3dPoint after = candidate.PointAt(candidate.Domain().ParameterAt(fraction));
	const ON_3dPoint before_model = surface.PointAt(before.x, before.y);
	const ON_3dPoint after_model = surface.PointAt(after.x, after.y);
	if (!before.IsValid() || !after.IsValid() ||
	    !before_model.IsValid() || !after_model.IsValid())
	    return std::numeric_limits<double>::infinity();
	movement = std::max(movement, before_model.DistanceTo(after_model));
    }
    return movement;
}

static void
repair_parameter_loop(Importer &importer, const ON_Surface &surface,
    std::vector<CurvePair> &pairs, const DirectoryEntry &source,
    std::map<EntityId, double> &relaxed_tolerances, bool adaptive)
{
    if (!importer.safe_repairs())
	return;
    const double tolerance = importer.tolerance();
    const bool closed[2] = {surface.IsClosed(0), surface.IsClosed(1)};
    const double maximum_repair = importer.maximum_trim_repair_tolerance(model_bounds(pairs));
    const auto pair_points = [&](const CurvePair &pair, ON_3dPoint &start,
	    ON_3dPoint &end) {
	return curve_pair_points(pair, surface, tolerance, start, end);
    };
    for (size_t i = 0; i < pairs.size(); ++i) {
	const size_t next = (i + 1) % pairs.size();
	if (pairs[i].singular && pairs[next].singular)
	    continue;
	const ON_3dPoint parameter_end = pairs[i].parameter->PointAtEnd();
	const ON_3dPoint parameter_start =
	    pairs[next].parameter->PointAtStart();
	if (!parameter_end.IsValid() || !parameter_start.IsValid() ||
		parameter_end.DistanceTo(parameter_start) <= ON_ZERO_TOLERANCE)
	    continue;
	ON_3dPoint model_end;
	ON_3dPoint model_start;
	ON_3dPoint ignored_endpoint;
	if (!pair_points(pairs[i], ignored_endpoint, model_end) ||
		!pair_points(pairs[next], model_start, ignored_endpoint))
	    continue;
	const double model_gap = model_end.DistanceTo(model_start);
	const ON_3dPoint end_surface =
	    surface.PointAt(parameter_end.x, parameter_end.y);
	const ON_3dPoint start_surface =
	    surface.PointAt(parameter_start.x, parameter_start.y);
	const double end_cost = std::max(end_surface.DistanceTo(model_end),
	    end_surface.DistanceTo(model_start));
	const double start_cost =
	    std::max(start_surface.DistanceTo(model_end),
		start_surface.DistanceTo(model_start));
	const bool prefer_end = pairs[i].singular ||
	    (!pairs[next].singular && end_cost <= start_cost);
	const std::array<ON_3dPoint, 2> targets = {
	    prefer_end ? parameter_end : parameter_start,
	    prefer_end ? parameter_start : parameter_end
	};
	const std::array<double, 2> repair_costs = {
	    prefer_end ? end_cost : start_cost,
	    prefer_end ? start_cost : end_cost
	};
	const double safe_limit = std::max(
	    SAFE_TRIM_REPAIR_TOLERANCE_FACTOR * tolerance,
	    model_gap + SAFE_TRIM_MODEL_GAP_FACTOR * tolerance);
	const double repair_limit = std::max(safe_limit,
	    maximum_repair);
	if (!std::isfinite(repair_limit))
	    continue;
	bool crosses_seam = false;
	for (int direction = 0; direction < 2; ++direction)
	    if (closed[direction] &&
		std::fabs(parameter_end[direction] - parameter_start[direction]) >
		    surface.Domain(direction).Length() / 2.0)
		crosses_seam = true;
	bool repaired = false;
	for (size_t candidate = 0;
		candidate < targets.size() && !repaired; ++candidate) {
	    /* A singular trim must remain on its surface pole. */
	    if (candidate > 0 && (pairs[i].singular || pairs[next].singular))
		break;
	    if (!std::isfinite(repair_costs[candidate]) ||
		    (!adaptive && repair_costs[candidate] > repair_limit))
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
	    if (!endpoints_set || !current_candidate->IsValid() || !next_curve->IsValid())
		continue;
	    /* Across a periodic seam, coincident lifted endpoints can conceal
	     * a large change to the intervening curve.  Small local endpoint
	     * corrections do not have this parameter-branch ambiguity. */
	    double used = repair_costs[candidate];
	    if (crosses_seam || adaptive) {
		const double movement = std::max(
		    parameter_curve_movement(surface, *pairs[i].parameter, *current_candidate),
		    parameter_curve_movement(surface, *pairs[next].parameter, *next_curve));
		if (!std::isfinite(movement) || (crosses_seam && movement > repair_limit))
		    continue;
		used = std::max(used, movement);
	    }
	    repaired = true;
	    pairs[i].parameter = std::move(current_candidate);
	    if (i != next)
		pairs[next].parameter = std::move(next_candidate);
	    if (used > safe_limit) {
		pairs[i].repair_tolerance = std::max(
		    pairs[i].repair_tolerance, used);
		pairs[next].repair_tolerance = std::max(
		    pairs[next].repair_tolerance, used);
		relaxed_tolerances[source.id] = std::max(
		    relaxed_tolerances[source.id], used);
		std::ostringstream message;
		message << "closed a parameter-space trim gap using "
		    << (adaptive ? "adaptive" : importer.repair_tolerance_description())
		    << " tolerance of " << used << " mm";
		importer.diagnose(Severity::Warning,
		    "relaxed_parameter_loop", message.str(), &source);
	    }
	}
	if (!repaired)
	    continue;
	importer.count_repair();
	importer.diagnose(Severity::Information,
	    "closed_parameter_loop",
	    "closed a bounded parameter-space trim gap using model geometry",
	    &source);
    }
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

    const auto pair_points = [&](const CurvePair &pair, ON_3dPoint &start,
	    ON_3dPoint &end) {
	return curve_pair_points(pair, *surface, tolerance, start, end);
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
	std::vector<std::pair<size_t, CurvePair> > missing_poles;
	for (size_t i = 0; i < pairs.size(); ++i) {
	    const size_t next = (i + 1) % pairs.size();
	    if (pairs[i].singular || pairs[next].singular)
		continue;
	    const ON_3dPoint end = pairs[i].parameter->PointAtEnd();
	    const ON_3dPoint start = pairs[next].parameter->PointAtStart();
	    if (end.DistanceTo(start) <= ON_ZERO_TOLERANCE)
		continue;
	    /* A missing side at a pole is a singular trim, not an endpoint
	     * gap.  Moving a side edge across the pole changes its interior
	     * geometry and destroys the matching seam classification. */
	    CurvePair pole;
	    pole.parameter.reset(new ON_LineCurve(ON_2dPoint(end.x, end.y),
		ON_2dPoint(start.x, start.y)));
	    const ON_3dPoint lifted = surface->PointAt(end.x, end.y);
	    if (!lifted.IsValid() ||
		lifted.DistanceTo(pairs[i].model->PointAtEnd()) > tolerance ||
		lifted.DistanceTo(pairs[next].model->PointAtStart()) > tolerance ||
		!singular_curve(pole, *surface, source))
		continue;
	    pole.singular = true;
	    missing_poles.emplace_back(i + 1, std::move(pole));
	}
	for (auto pole = missing_poles.rbegin(); pole != missing_poles.rend(); ++pole) {
	    pairs.insert(pairs.begin() + pole->first, std::move(pole->second));
	    importer_.count_repair();
	    importer_.diagnose(Severity::Information, "inserted_singular_boundary",
		"completed a missing parameter-space side at a surface pole", &source);
	}

	repair_parameter_loop(importer_, *surface, pairs, source, relaxed_tolerances_,
	    recovery_ == FaceRecovery::TrimEndpoints);
    }


    /* Start at a pole so the final non-singular edge closes onto its
     * existing vertex rather than creating a second vertex there. */
    for (size_t i = 0; i < pairs.size(); ++i) {
	const size_t previous = i == 0 ? pairs.size() - 1 : i - 1;
	if (pairs[i].singular && !pairs[previous].singular) {
	    std::rotate(pairs.begin(), pairs.begin() + i, pairs.end());
	    break;
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
    // Type 144 defines the retained parameter region by outer/inner
    // membership, not by curve winding.  Normalize OpenNURBS trim uses
    // before adjacency propagation, without changing the surface normal.
    // Otherwise a clockwise outer loop makes face-sense propagation wrong.
    if (importer_.safe_repairs()) {
	const int direction = parameter_loop_direction(loop);
	if (direction && direction != (type == ON_BrepLoop::outer ? 1 : -1)) {
	    brep_->FlipLoop(loop);
	    importer_.count_repair();
	    importer_.diagnose(Severity::Information, "parameter_loop_reoriented",
		"normalized parameter boundary winding before face assembly", &source);
	}
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
    ON_Plane plane(ON_3dPoint(model_origin.data()),
	ON_3dVector(x_axis.data()), ON_3dVector(y_axis.data()));
    if (!plane.IsValid())
	return nullptr;

    /* Older bounded-plane exports sometimes use Ax+By+Cz+D=0.  Only
     * accept that convention when every authored boundary supports it. */
    if (importer_.safe_repairs()) {
	const Point3 transformed_zero = importer_.model_point(surface_entry,
	    {0.0, 0.0, 0.0}, geometry.solid_transform_);
	const ON_3dPoint alternate_origin = ON_3dPoint(transformed_zero.data()) +
	    (ON_3dPoint(transformed_zero.data()) - plane.origin);
	const ON_Plane alternate(alternate_origin, plane.xaxis, plane.yaxis);
	double original_error = 0.0;
	double alternate_error = 0.0;
	for (const auto &loop : loops)
	    for (const CurvePair &pair : loop) {
		if (!pair.model)
		    return nullptr;
		for (int i = 0; i <= BOUNDARY_VALIDATION_SEGMENTS; ++i) {
		    const ON_3dPoint point = pair.model->PointAt(pair.model->Domain().ParameterAt(
			static_cast<double>(i) / BOUNDARY_VALIDATION_SEGMENTS));
		    if (!point.IsValid())
			return nullptr;
		    original_error = std::max(original_error, std::fabs(plane.DistanceTo(point)));
		    alternate_error = std::max(alternate_error, std::fabs(alternate.DistanceTo(point)));
		}
	    }
	if (original_error > importer_.tolerance() && alternate_error <= importer_.tolerance()) {
	    plane = alternate;
	    importer_.count_repair();
	    importer_.diagnose(Severity::Information, "repaired_plane_constant",
		"authored boundaries identify the opposite plane-constant sign", &surface_entry);
	}
    }

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
		    sample <= BOUNDARY_VALIDATION_SEGMENTS; ++sample) {
		ON_2dPoint parameter;
		parameter[fixed_direction] = fixed_domain[side];
		parameter[1 - fixed_direction] = varying_domain.ParameterAt(
		    static_cast<double>(sample) /
			BOUNDARY_VALIDATION_SEGMENTS);
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


static bool
place_periodic_parameter_curve(const ON_Surface &surface, ON_Curve &curve)
{
    const ON_BoundingBox bounds = curve.BoundingBox();
    if (!bounds.IsValid())
	return false;
    const ON_RevSurface *revolution = ON_RevSurface::Cast(&surface);
    ON_3dVector offset(0.0, 0.0, 0.0);
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface.IsClosed(direction))
	    continue;
	/* Native revolution angles evaluate periodically, unlike NURBS
	 * extrapolation, and may safely straddle their domain seam. */
	if (revolution && direction == (revolution->m_bTransposed ? 1 : 0))
	    continue;
	const ON_Interval domain = surface.Domain(direction);
	const double period = domain.Length();
	if (!domain.IsIncreasing())
	    return false;
	const double parameter_tolerance = DEGENERATE_DOMAIN_TOLERANCE *
	    std::max(1.0, std::max(std::fabs(domain.Min()), std::fabs(domain.Max())));
	if (bounds.m_min[direction] >= domain.Min() - parameter_tolerance &&
	    bounds.m_max[direction] <= domain.Max() + parameter_tolerance)
	    continue;
	const double center = bounds.Center()[direction];
	offset[direction] = std::floor((domain.Mid() - center) / period + 0.5) * period;
	if (bounds.m_min[direction] + offset[direction] < domain.Min() - parameter_tolerance ||
	    bounds.m_max[direction] + offset[direction] > domain.Max() + parameter_tolerance)
	    return false;
    }
    return curve.Translate(offset) && curve.IsValid();
}

static void
recover_parameter_curves(Importer &importer,
    const ON_Surface &surface, std::vector<std::vector<CurvePair> > &loops,
    const DirectoryEntry &source, std::map<EntityId, double> &relaxed_tolerances)
{
    if (!importer.safe_repairs())
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
    const double safe_tolerance = std::max(importer.tolerance(),
	ON_ZERO_TOLERANCE) * SAFE_TRIM_REPAIR_TOLERANCE_FACTOR;
    ON_BoundingBox bounds;
    for (const std::vector<CurvePair> &loop : loops)
	bounds.Union(model_bounds(loop));
    const double maximum_tolerance = importer.maximum_trim_repair_tolerance(bounds);
    /* Start at source accuracy.  Starting at the full repair allowance
     * can collapse real small features before a tighter pullback is tried. */
    const double tolerance = std::min(std::max(importer.tolerance(),
	ON_ZERO_TOLERANCE), maximum_tolerance);
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
		    importer.count_repair();
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
		    importer.count_repair();
		    continue;
		}
		pair.discard = true;
		++discarded_recoveries;
		importer.count_repair();
		continue;
	    }

	    double attempted_tolerance = tolerance;
	    while (!pair.parameter &&
		    (failure == PullbackFailureReason::ProjectionFailed ||
		     failure == PullbackFailureReason::SurfaceDistanceExceeded) &&
		    attempted_tolerance < maximum_tolerance) {
		const double next_tolerance = std::min(maximum_tolerance,
		    attempted_tolerance * RELAXED_TRIM_TOLERANCE_STEP_FACTOR);
		if (!(next_tolerance > attempted_tolerance))
		    break;
		attempted_tolerance = next_tolerance;
		failure_reason.clear();
		pair.parameter.reset(brlcad::pullback_curve(&surface,
		    pair.model.get(), attempted_tolerance, tolerance,
		    &failure_reason, &failure));
	    }
	    /* Pullback returns a continuous periodic image.  Closed NURBS
	     * surfaces do not evaluate that image periodically outside their
	     * native domain.  Translate a whole branch when possible; crossing
	     * a seam otherwise requires splitting the authored topology. */
	    if (pair.parameter && !place_periodic_parameter_curve(surface, *pair.parameter)) {
		pair.parameter.reset();
		failure_reason = "periodic boundary requires a seam split to fit the surface domain";
	    }
	    if (!pair.parameter) {
		++pullback_failures;
		if (first_pullback_failure.empty())
		    first_pullback_failure = failure_reason;
		continue;
	    }
	    if (attempted_tolerance > safe_tolerance) {
		pair.repair_tolerance = attempted_tolerance;
		maximum_relaxed_tolerance = std::max(maximum_relaxed_tolerance,
		    attempted_tolerance);
		relaxed_tolerances[source.id] = std::max(
		    relaxed_tolerances[source.id], attempted_tolerance);
		++relaxed_recoveries;
	    }
	    ++pullback_recoveries;
	    importer.count_repair();
	}
    }
    if (isoparametric_recoveries > 0) {
	std::ostringstream message;
	message << "recovered " << isoparametric_recoveries
	    << " missing parameter-space boundaries from base-surface isocurves";
	importer.diagnose(Severity::Information,
	    "recovered_isoparametric_boundary", message.str(), &source);
    }
    if (pullback_recoveries > 0) {
	std::ostringstream message;
	message << "recovered " << pullback_recoveries
	    << " missing parameter-space boundaries by bounded pullback";
	importer.diagnose(Severity::Information,
	    "recovered_parameter_curve", message.str(), &source);
    }
    if (relaxed_recoveries > 0) {
	std::ostringstream message;
	message << "recovered " << relaxed_recoveries
	    << " boundary curves using " << importer.repair_tolerance_description()
	    << " tolerance up to "
	    << maximum_relaxed_tolerance << " mm";
	importer.diagnose(Severity::Warning, "relaxed_parameter_curve",
	    message.str(), &source);
    }
    if (singular_recoveries > 0) {
	std::ostringstream message;
	message << "recovered " << singular_recoveries
	    << " collapsed model-space boundaries as singular trims";
	importer.diagnose(Severity::Information,
	    "recovered_singular_boundary", message.str(), &source);
    }
    if (discarded_recoveries > 0) {
	std::ostringstream message;
	message << "discarded " << discarded_recoveries
	    << " model-space boundary segments whose validated pullbacks "
	    << "collapsed within the safe repair tolerance";
	importer.diagnose(Severity::Information,
	    "discarded_collapsed_boundary", message.str(), &source);
    }
    if (pullback_failures > 0) {
	std::ostringstream message;
	message << "bounded pullback could not recover " << pullback_failures
	    << " parameter-space boundaries";
	if (!first_pullback_failure.empty())
	    message << "; first failure: " << first_pullback_failure;
	importer.diagnose(Severity::Warning,
	    "parameter_curve_pullback", message.str(), &source);
    }
}

bool
TrimmedSurfaceBuilder::add_face(const DirectoryEntry &entry)
{
    const ParameterList *parameters = importer_.document().parameters(entry.id);
    EntityId surface_id;
    SolidBuilder geometry(importer_, entry, entry.type == 108 ?
	Matrix() : importer_.transform(entry.transform));
    std::vector<EntityId> boundaries;
    std::vector<EntityId> plane_sources;
    bool natural_outer_boundary = false;
    if (entry.type == 144) {
	int outer_boundary = 0;
	int inner_count = 0;
	EntityId outer_id;
	if (!parameter_entity(parameters, 1, surface_id) ||
		!parameter_integer(parameters, 2, outer_boundary) ||
		!parameter_integer(parameters, 3, inner_count) ||
		(outer_boundary != 0 && outer_boundary != 1) || inner_count < 0 ||
		inner_count > MAX_ENTITY_LIST_COUNT ||
		(outer_boundary == 1 && !parameter_entity(parameters, 4, outer_id))) {
	    importer_.diagnose(Severity::Warning, "trimmed_surface_parameters",
		"Trimmed Surface has invalid boundary parameters",
		&entry);
	    return false;
	}
	natural_outer_boundary = outer_boundary == 0;
	if (!natural_outer_boundary)
	    boundaries.push_back(outer_id);
	for (int i = 0; i < inner_count; ++i) {
	    EntityId inner_id;
	    if (!parameter_entity(parameters, static_cast<size_t>(i + 5),
		    inner_id))
		return false;
	    boundaries.push_back(inner_id);
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
	for (int i = 0; i < boundary_count; ++i) {
	    EntityId boundary_id;
	    if (!parameter_entity(parameters, static_cast<size_t>(i + 4),
		    boundary_id))
		return false;
	    boundaries.push_back(boundary_id);
	}
    } else if (entry.type == 108 && entry.form == 1) {
	EntityId boundary;
	if (!parameter_entity(parameters, 5, boundary))
	    return false;
	surface_id = entry.id;
	boundaries.push_back(boundary);
	plane_sources.push_back(entry.id);
	for (EntityId hole : importer_.plane_holes(entry.id)) {
	    if (!parameter_entity(importer_.document().parameters(hole), 5, boundary))
		return false;
	    boundaries.push_back(boundary);
	    plane_sources.push_back(hole);
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
    if (natural_outer_boundary && surface_entry->type == 108) {
	importer_.diagnose(Severity::Warning, "unbounded_natural_boundary",
	    "an infinite plane has no finite natural outer boundary", &entry);
	return false;
    }
    if (surface_entry->type == 128) {
	surface = geometry.nurbs_surface(*surface_entry);
    } else if (surface_entry->type == 118 || surface_entry->type == 120 ||
	    surface_entry->type == 122) {
	surface = geometry.analytic_surface(*surface_entry);
    } else if (surface_entry->type != 108) {
	importer_.diagnose(Severity::Warning, "unsupported_trimmed_surface",
	    "direct trimmed-surface import does not support this base surface",
	    surface_entry);
	return false;
    }
    if (surface)
	resolve_revolution_parameters(geometry, *surface, boundaries);
    std::vector<std::vector<CurvePair> > loops(boundaries.size());
    for (size_t i = 0; i < boundaries.size(); ++i) {
	if (entry.type == 108) {
	    std::vector<EntityId> members;
	    std::set<EntityId> active;
	    if (!append_curve_entities(boundaries[i], members, active))
		return false;
	    for (EntityId member : members) {
		const DirectoryEntry *curve_entry = importer_.document().entity(member);
		CurvePair pair;
		if (!curve_entry)
		    return false;
		pair.model = geometry.curve(*curve_entry, true,
		    importer_.transform(importer_.document().entity(plane_sources[i])->transform));
		if (!pair.model)
		    return false;
		loops[i].push_back(std::move(pair));
	    }
	    continue;
	}
	const bool valid = entry.type == 144 ?
	    curve_pairs(geometry, boundaries[i], surface.get(), loops[i]) :
	    bounded_curve_pairs(geometry, boundaries[i], surface_id, loops[i]);
	if (!valid)
	    return false;
    }
    if (surface_entry->type == 108)
	surface = plane_surface(geometry, *surface_entry, loops);
    if (!surface) {
	importer_.diagnose(Severity::Warning, "invalid_trimmed_surface_geometry",
	    "could not construct the trimmed face's base surface", surface_entry);
	return false;
    }

    recover_parameter_curves(importer_, *surface, loops, entry, relaxed_tolerances_);
    for (std::vector<CurvePair> &loop : loops)
	loop.erase(std::remove_if(loop.begin(), loop.end(),
	    [](const CurvePair &pair) { return pair.discard; }), loop.end());
    for (const std::vector<CurvePair> &loop : loops)
	for (const CurvePair &pair : loop)
	    if (!pair.parameter) {
		importer_.diagnose(Severity::Warning,
		    "missing_parameter_curve",
		    "non-planar trimmed face has no parameter-space boundary",
		    &entry);
		return false;
	    }

    ON_BrepFace *face = natural_outer_boundary ? brep_->NewFace(*surface) :
	&brep_->NewFace(brep_->AddSurface(surface.release()));
    if (!face)
	return false;
    face->m_face_user.i = static_cast<int>(entry.id.value());
    for (size_t i = 0; i < loops.size(); ++i)
	if (!add_loop(*face, !natural_outer_boundary && i == 0 ?
		ON_BrepLoop::outer : ON_BrepLoop::inner,
		loops[i], entry))
	    return false;
    return true;
}

static bool
periodic_trims_in_domain(const ON_Brep &brep)
{
    /* A closed NURBS surface does not evaluate periodically outside its
     * domain.  Joining and orientation must not bypass this trim check. */
    for (int i = 0; i < brep.m_T.Count(); ++i) {
	const ON_BrepTrim &trim = brep.m_T[i];
	const ON_Surface *surface = trim.SurfaceOf();
	if (!ON_NurbsSurface::Cast(surface) ||
	    (!surface->IsClosed(0) && !surface->IsClosed(1)))
	    continue;
	std::unique_ptr<ON_Curve> parameter(trim.DuplicateCurve());
	if (!parameter || !place_periodic_parameter_curve(*surface, *parameter))
	    return false;
    }
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
	importer_.progress(recovery_ == FaceRecovery::None ?
	    "constructing trimmed faces" : "recovering trimmed faces", entry->id);
	if (!add_face(*entry)) {
	    importer_.diagnose(Severity::Warning, "trimmed_surface_face",
		"could not construct an OpenNURBS face", entry);
	    return nullptr;
	}
    }
    /* Preserve the original use count through joining and extraction so
     * per-object merge diagnostics exclude pre-existing natural seams. */
    for (int i = 0; i < brep_->m_E.Count(); ++i)
	brep_->m_E[i].m_edge_user.i = brep_->m_E[i].m_ti.Count();
    importer_.progress("stitching and validating face batch", faces_.front()->id);
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
    if (!periodic_trims_in_domain(*brep_)) {
	importer_.diagnose(Severity::Warning, "trimmed_surface_loop",
	    "assembled periodic trim requires a surface seam split", faces_.front());
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

BrepOrientationResult
Importer::orient_geometry(const std::string &name, ON_Brep &brep)
{
    const auto update = [&](size_t face, size_t total) {
	if (options_.progress) {
	    const std::string activity = name + " (face " + std::to_string(face) + "/" + std::to_string(total) + ")";
	    options_.progress("orientation", activity.c_str(), progress_completed_, progress_total_, 0);
	}
    };
    return safe_repairs() ? repair_brep_orientation(brep, update) :
	check_brep_orientation(brep, ORIENTATION_EVALUATION_LIMIT, update);
}

bool
Importer::write_geometry(const std::string &name, ON_Brep &brep, bool incomplete_solid,
    BrepOrientationResult *prepared_orientation)
{
    size_t reversed_faces = 0;
    BrepOrientationResult orientation;
    if (incomplete_solid) {
	orientation.detail = "preserved incomplete IGES solid; orientation not evaluated";
    } else {
	orientation = prepared_orientation ? std::move(*prepared_orientation) : orient_geometry(name, brep);
	for (const auto &shell : orientation.shells) {
	    if (shell.corrected) {
		reversed_faces += shell.faces_to_flip.size();
	    } else if (!shell.faces_to_flip.empty()) {
		diagnose(options_.strict ? Severity::Error : Severity::Warning, "orientation_repair_required",
		    name + ": closed shell has an outward-orientation correction, but was left unchanged");
	    } else if (shell.orientation != ShellOrientation::Outward || shell.context != ShellContext::Isolated) {
		diagnose(Severity::Warning, "orientation_needs_review", name +
		    ": shell orientation left unchanged (open, nested, overlapping, or numerically uncertain); see orientation report");
	    }
	}
    }
    orientation.object = name;
    result_.orientation.objects.push_back(std::move(orientation));
    if (reversed_faces) {
	result_.statistics.orientation_faces_reversed += reversed_faces;
	result_.statistics.repairs += reversed_faces;
	diagnose(Severity::Warning, "orientation_corrected", name + ": reversed " +
	    std::to_string(reversed_faces) + " faces to orient isolated closed shells outward");
    }
    progress(options_.output == GeometryOutput::Brep ?
	"writing B-Rep geometry" : "tessellating geometry");
    if (options_.output == GeometryOutput::Brep) {
	if (mk_brep(wdbp_, name.c_str(), &brep) < 0)
	    return false;
	if (reversed_faces && db5_update_attribute(name.c_str(), "iges.orientation_faces_reversed",
		std::to_string(reversed_faces).c_str(), wdbp_->dbip) < 0) {
	    diagnose(Severity::Error, "orientation_attribute", "could not record orientation correction for " + name);
	    return false;
	}
	return true;
    }
    if (!brep.IsValid() || !brep.m_F.Count())
	return false;
    /* Both polygonal modes use the same validated OpenNURBS import as
     * native output.  Never send NURBS edge geometry into legacy planar
     * NMG glue operations.  Tessellation may modify its private copy. */
    ON_Brep copy(brep);
    std::unique_ptr<ON_Brep_CDT_State, decltype(&ON_Brep_CDT_Destroy)> tessellation(
	ON_Brep_CDT_Create(&copy, name.c_str()), ON_Brep_CDT_Destroy);
    if (!tessellation)
	return false;
    const int tessellated = ON_Brep_CDT_Tessellate(tessellation.get(), 0, nullptr);
    if (tessellated < 0 || (brep.IsSolid() && tessellated != 0)) {
	diagnose(Severity::Warning, "tessellation_failed",
	    "could not produce a complete mesh with the source B-Rep's solid status");
	return false;
    }
    struct MeshArrays {
	int *faces = nullptr;
	fastf_t *vertices = nullptr;
	~MeshArrays() { bu_free(faces, "IGES mesh faces"); bu_free(vertices, "IGES mesh vertices"); }
    } mesh;
    int face_count = 0;
    int vertex_count = 0;
    if (ON_Brep_CDT_Mesh(&mesh.faces, &face_count, &mesh.vertices, &vertex_count,
	nullptr, nullptr, nullptr, nullptr, tessellation.get(), 0, nullptr) < 0 ||
	face_count <= 0 || vertex_count <= 0)
	return false;
    const unsigned char mode = brep.IsSolid() ? RT_BOT_SOLID : RT_BOT_SURFACE;
    if (options_.output == GeometryOutput::Mesh)
	return mk_bot(wdbp_, name.c_str(), mode, RT_BOT_CCW, 0, vertex_count, face_count,
	    mesh.vertices, mesh.faces, nullptr, nullptr) >= 0;

    struct rt_bot_internal bot = {};
    bot.magic = RT_BOT_INTERNAL_MAGIC;
    bot.mode = mode;
    bot.orientation = RT_BOT_CCW;
    bot.num_vertices = vertex_count;
    bot.num_faces = face_count;
    bot.vertices = mesh.vertices;
    bot.faces = mesh.faces;
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];
    internal.idb_ptr = &bot;
    struct bg_tess_tol tess_tol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tolerance = BN_TOL_INIT_TOL;
    tolerance.dist = tolerance_;
    tolerance.dist_sq = tolerance_ * tolerance_;
    std::unique_ptr<struct model, decltype(&nmg_km)> model(nmg_mm(), nmg_km);
    struct nmgregion *region = nullptr;
    if (!model || rt_bot_tess(&region, model.get(), &internal, &tess_tol, &tolerance) < 0)
	return false;
    return mk_nmg(wdbp_, name.c_str(), model.release()) >= 0;
}

void
Importer::count_geometry(const ON_Brep &brep, bool invalid_solid)
{
    if (options_.output == GeometryOutput::Brep) {
	++result_.statistics.breps_written;
	result_.statistics.solid_breps_written += !invalid_solid && brep.IsSolid();
    } else if (options_.output == GeometryOutput::Mesh) {
	++result_.statistics.meshes_written;
    } else {
	++result_.statistics.polygons_written;
    }
}

bool
Importer::write_face_metadata(const std::string &name, const ON_Brep &brep,
    const std::vector<const DirectoryEntry *> &faces,
    const std::map<EntityId, double> &relaxed_tolerances,
    const std::map<EntityId, FaceRecovery> &recoveries)
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
	const auto recovery = recoveries.find(face->id);
	if (recovery != recoveries.end())
	    metadata << ",\"recovery\":\"" << face_recovery_name(recovery->second) << '"';
	metadata << '}';
    }
    metadata << ']';
    const char *attribute = options_.output == GeometryOutput::Brep ?
	"iges.face_metadata" : "iges.source_face_metadata";
    if (db5_update_attribute(name.c_str(), attribute,
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
Importer::write_repair_attributes(const std::string &name,
    const std::map<EntityId, double> &relaxed_tolerances,
    const DirectoryEntry &entry, const std::map<EntityId, FaceRecovery> &recoveries)
{
    if (relaxed_tolerances.empty())
	return true;
    double maximum_tolerance = 0.0;
    bool adaptive = false;
    bool bounded = false;
    for (const auto &repair : relaxed_tolerances) {
	maximum_tolerance = std::max(maximum_tolerance, repair.second);
	const auto recovery = recoveries.find(repair.first);
	if (recovery != recoveries.end() && recovery->second == FaceRecovery::TrimEndpoints)
	    adaptive = true;
	else
	    bounded = true;
    }
    const char *basis = adaptive ? (bounded ? "mixed" : "adaptive") :
	options_.maximum_repair_tolerance > 0.0 ? "absolute_override" : "object_relative";
    std::ostringstream maximum;
    maximum << std::setprecision(std::numeric_limits<double>::max_digits10)
	<< maximum_tolerance;
    std::ostringstream nominal;
    nominal << std::setprecision(std::numeric_limits<double>::max_digits10)
	<< tolerance_;
    if (db5_update_attribute(name.c_str(), "iges.tolerance_status", "relaxed", wdbp_->dbip) < 0 ||
	db5_update_attribute(name.c_str(), "iges.maximum_repair_tolerance_mm",
	    maximum.str().c_str(), wdbp_->dbip) < 0 ||
	db5_update_attribute(name.c_str(), "iges.nominal_tolerance_mm",
	    nominal.str().c_str(), wdbp_->dbip) < 0 ||
	db5_update_attribute(name.c_str(), "iges.repair_tolerance_basis",
	    basis, wdbp_->dbip) < 0 ||
	db5_update_attribute(name.c_str(), "iges.tolerance_basis",
	    source_resolution_declared_ ? "source_resolution" : "import_default", wdbp_->dbip) < 0) {
	diagnose(Severity::Error, "repair_attributes",
	    "could not record the relaxed repair tolerance", &entry);
	return false;
    }
    for (const auto &repair : relaxed_tolerances)
	count_relaxed_face(repair.second);
    return true;
}

bool
Importer::build_trimmed_component(
    const std::vector<const DirectoryEntry *> &faces, TrimmedComponent &component,
    FaceRecovery recovery)
{
    brep_assembly_result assembly;
    TrimmedSurfaceBuilder builder(*this, faces, recovery);
    std::unique_ptr<ON_Brep> brep = builder.build(assembly);
    if (!brep)
	return false;

    component.brep = std::move(brep);
    component.faces = faces;
    component.relaxed_tolerances = builder.relaxed_tolerances();
    if (recovery != FaceRecovery::None)
	for (const DirectoryEntry *face : faces)
	    component.recoveries.emplace(face->id, recovery);
    return true;
}

bool
Importer::write_trimmed_geometry(const TrimmedComponent &component, const std::string &name)
{
    const auto &faces = component.faces;
    const auto &brep = component.brep;
    const auto &relaxed_tolerances = component.relaxed_tolerances;
    const auto &recoveries = component.recoveries;

    const DirectoryEntry &source = *faces.front();
    if (!write_geometry(name, *brep)) {
	diagnose(Severity::Error, "brep_write",
	    "failed to write assembled OpenNURBS B-Rep", &source);
	return false;
    }
    if (!write_plate_mode_attributes(name, *brep, source))
	return false;
    write_entity_attributes(name, source);
    if (!write_face_metadata(name, *brep, faces, relaxed_tolerances, recoveries))
	return false;
    const std::string face_count = std::to_string(faces.size());
    size_t merged = 0;
    size_t naked = 0;
    for (int i = 0; i < brep->m_E.Count(); ++i) {
	const ON_BrepEdge &edge = brep->m_E[i];
	merged += edge.m_edge_user.i == 1 && edge.m_ti.Count() == 2;
	naked += edge.m_ti.Count() == 1;
    }
    const std::string merged_edges = std::to_string(merged);
    const std::string naked_edges = std::to_string(naked);
    if (db5_update_attribute(name.c_str(), "iges.face_count", face_count.c_str(), wdbp_->dbip) < 0 ||
	db5_update_attribute(name.c_str(), "iges.merged_edges", merged_edges.c_str(), wdbp_->dbip) < 0 ||
	db5_update_attribute(name.c_str(), "iges.naked_edges", naked_edges.c_str(), wdbp_->dbip) < 0 ||
	db5_update_attribute(name.c_str(), "iges.topology", options_.output == GeometryOutput::Brep ?
	    "direct-opennurbs-component" : "tessellated-opennurbs-component", wdbp_->dbip) < 0) {
	diagnose(Severity::Error, "component_attributes", "failed to record component topology", &source);
	return false;
    }
    if (!write_repair_attributes(name, relaxed_tolerances, source, recoveries))
	return false;
    if (!recoveries.empty()) {
	const FaceRecovery first = recoveries.begin()->second;
	const bool uniform = std::all_of(recoveries.begin(), recoveries.end(),
	    [first](const std::pair<const EntityId, FaceRecovery> &item) {
		return item.second == first;
	    });
	const char *method = uniform ? face_recovery_name(first) : "mixed";
	if (db5_update_attribute(name.c_str(), "iges.import_status", "needs_review", wdbp_->dbip) < 0 ||
	    db5_update_attribute(name.c_str(), "iges.recovery", method, wdbp_->dbip) < 0) {
	    diagnose(Severity::Error, "recovery_attributes",
		"could not record the best-effort face recovery", &source);
	    return false;
	}
	for (const auto &recovery : recoveries)
	    diagnose(Severity::Warning, "recovered_trimmed_face",
		recovery.second == FaceRecovery::ModelBoundaries ?
		    "retained face by rebuilding parameter boundaries from authored model curves; review required" :
		    "retained face using adaptive parameter endpoint repairs; review required",
		document_.entity(recovery.first));
	result_.statistics.recovered_faces_written += recoveries.size();
    }
    for (const DirectoryEntry *face : faces)
	objects_[face->id] = name;
    root_objects_.insert(name);
    count_geometry(*brep);
    ++result_.statistics.components_written;
    return true;
}

void
Importer::discard_failed_repairs(size_t first_diagnostic, size_t repair_count)
{
    result_.statistics.repairs = repair_count;
    result_.diagnostics.erase(std::remove_if(
	result_.diagnostics.begin() + static_cast<std::ptrdiff_t>(first_diagnostic),
	result_.diagnostics.end(), [](const ImportDiagnostic &diagnostic) {
	    return diagnostic.severity == Severity::Information;
	}), result_.diagnostics.end());
}

void
Importer::collect_trimmed_components(
    const std::vector<const DirectoryEntry *> &faces,
    std::vector<TrimmedComponent> &components)
{
    if (faces.empty())
	return;
    const size_t diagnostic_count = result_.diagnostics.size();
    const size_t repair_count = result_.statistics.repairs;
    TrimmedComponent component;
    if (build_trimmed_component(faces, component)) {
	components.push_back(std::move(component));
	progress_completed_ += faces.size();
	return;
    }

    const auto write_error = [&]() {
	return std::any_of(result_.diagnostics.begin() +
	    static_cast<std::ptrdiff_t>(diagnostic_count),
	    result_.diagnostics.end(), [](const ImportDiagnostic &diagnostic) {
		return diagnostic.severity == Severity::Error ||
		    diagnostic.severity == Severity::Fatal;
	    });
    };
    if (faces.size() == 1 && !write_error() && best_effort_repairs()) {
	const bool boundary_failure = std::any_of(result_.diagnostics.begin() +
	    static_cast<std::ptrdiff_t>(diagnostic_count), result_.diagnostics.end(),
	    [](const ImportDiagnostic &diagnostic) {
		return diagnostic.code == "boundary_curve_cardinality" ||
		    diagnostic.code == "unsupported_boundary_curve" ||
		    diagnostic.code == "missing_parameter_curve" ||
		    diagnostic.code == "trimmed_surface_loop" ||
		    diagnostic.code == "trimmed_surface_assembly";
	    });
	if (boundary_failure) {
	    /* Isolate recovery to a face that would otherwise be omitted.
	     * Successful source-accurate neighbors must not be rebuilt. */
	    discard_failed_repairs(diagnostic_count, repair_count);
	    const size_t recovery_diagnostics = result_.diagnostics.size();
	    for (FaceRecovery recovery : {FaceRecovery::ModelBoundaries,
		FaceRecovery::TrimEndpoints}) {
		if (build_trimmed_component(faces, component, recovery)) {
		    components.push_back(std::move(component));
		    progress_completed_ += faces.size();
		    return;
		}
		if (write_error())
		    break;
		result_.diagnostics.resize(recovery_diagnostics);
		result_.statistics.repairs = repair_count;
	    }
	}
    }
    if (faces.size() == 1 || write_error()) {
	discard_failed_repairs(diagnostic_count, repair_count);
	result_.statistics.omitted += faces.size();
	progress_completed_ += faces.size();
	return;
    }

    result_.diagnostics.resize(diagnostic_count);
    result_.statistics.repairs = repair_count;
    const size_t middle = faces.size() / 2;
    const std::vector<const DirectoryEntry *> first(faces.begin(),
	faces.begin() + static_cast<std::ptrdiff_t>(middle));
    const std::vector<const DirectoryEntry *> second(
	faces.begin() + static_cast<std::ptrdiff_t>(middle), faces.end());
    collect_trimmed_components(first, components);
    collect_trimmed_components(second, components);
}

static int
component_root(std::vector<int> &parents, int index)
{
    while (parents[index] != index) {
	parents[index] = parents[parents[index]];
	index = parents[index];
    }
    return index;
}

static void
join_components(std::vector<int> &parents, int first, int second)
{
    first = component_root(parents, first);
    second = component_root(parents, second);
    parents[std::max(first, second)] = std::min(first, second);
}

static bool
append_component(TrimmedComponent &destination, const TrimmedComponent &source)
{
    if (!destination.brep)
	destination.brep.reset(ON_Brep::New());
    if (!destination.brep || !source.brep)
	return false;
    destination.brep->Append(*source.brep);
    destination.faces.insert(destination.faces.end(), source.faces.begin(), source.faces.end());
    destination.relaxed_tolerances.insert(source.relaxed_tolerances.begin(),
	source.relaxed_tolerances.end());
    destination.recoveries.insert(source.recoveries.begin(), source.recoveries.end());
    return destination.brep->m_F.Count() == static_cast<int>(destination.faces.size());
}

static bool
extract_component(const TrimmedComponent &source, const std::vector<int> &indices,
    TrimmedComponent &destination)
{
    destination = TrimmedComponent();
    destination.brep.reset(source.brep->SubBrep(static_cast<int>(indices.size()), indices.data()));
    if (!destination.brep || !destination.brep->IsValid())
	return false;
    for (int index : indices) {
	const DirectoryEntry *face = source.faces[index];
	destination.faces.push_back(face);
	const auto relaxed = source.relaxed_tolerances.find(face->id);
	if (relaxed != source.relaxed_tolerances.end())
	    destination.relaxed_tolerances.insert(*relaxed);
	const auto recovery = source.recoveries.find(face->id);
	if (recovery != source.recoveries.end())
	    destination.recoveries.insert(*recovery);
    }
    return periodic_trims_in_domain(*destination.brep);
}

struct FaceComponent {
    std::vector<int> faces;
    bool closed = false;
};

static std::map<int, FaceComponent>
connected_face_components(const ON_Brep &brep)
{
    std::vector<int> parents(brep.m_F.Count());
    for (size_t i = 0; i < parents.size(); ++i)
	parents[i] = static_cast<int>(i);
    /* Do not use LabelConnectedComponents: it overwrites the per-face
     * source IDs and the edge provenance used by merge diagnostics. */
    for (int i = 0; i < brep.m_E.Count(); ++i) {
	const ON_BrepEdge &edge = brep.m_E[i];
	for (int use = 1; use < edge.m_ti.Count(); ++use) {
	    const int first = brep.m_L[brep.m_T[edge.m_ti[0]].m_li].m_fi;
	    const int second = brep.m_L[brep.m_T[edge.m_ti[use]].m_li].m_fi;
	    join_components(parents, first, second);
	}
    }
    std::map<int, FaceComponent> connected;
    for (size_t i = 0; i < parents.size(); ++i)
	connected[component_root(parents, static_cast<int>(i))].faces.push_back(static_cast<int>(i));

    std::set<int> closed_components;
    std::set<int> open_components;
    for (int i = 0; i < brep.m_E.Count(); ++i) {
	const ON_BrepEdge &edge = brep.m_E[i];
	if (edge.m_ti.Count() == 0)
	    continue;
	const int face = brep.m_L[brep.m_T[edge.m_ti[0]].m_li].m_fi;
	const int root = component_root(parents, face);
	if (edge.m_ti.Count() == 2)
	    closed_components.insert(root);
	else
	    open_components.insert(root);
    }
    for (auto &item : connected)
	item.second.closed = closed_components.count(item.first) && !open_components.count(item.first);
    return connected;
}

bool
Importer::write_trimmed_component(const TrimmedComponent &component)
{
    const DirectoryEntry &source = *component.faces.front();
    const std::string name = unique_name(source);
    const auto connected = connected_face_components(*component.brep);
    // Nested closed shells and mixed validation fallbacks must retain their
    // original relationships.  Only a remainder consisting entirely of open
    // components is represented as independently addressable sheets.
    if (connected.size() < 2 || std::any_of(connected.begin(), connected.end(),
	[](const auto &item) { return item.second.closed; }))
	return write_trimmed_geometry(component, name);

    std::vector<TrimmedComponent> sheets;
    sheets.reserve(connected.size());
    for (const auto &item : connected) {
	progress("extracting connected open sheet", component.faces[item.second.faces.front()]->id);
	TrimmedComponent sheet;
	// SubBrep copies only the selected geometry, preserving the source face
	// and edge user fields.  Validate every extraction before writing children.
	if (!extract_component(component, item.second.faces, sheet)) {
	    diagnose(Severity::Warning, "sheet_extraction",
		"could not extract connected sheets; retained their validated aggregate", &source);
	    return write_trimmed_geometry(component, name);
	}
	sheets.push_back(std::move(sheet));
    }

    std::vector<std::string> children;
    for (const auto &sheet : sheets) {
	const std::string child = unique_name(name + "_sheet_D" +
	    std::to_string(sheet.faces.front()->id.value()));
	if (!write_trimmed_geometry(sheet, child))
	    return false;
	children.push_back(child);
    }
    struct wmember members;
    BU_LIST_INIT(&members.l);
    for (const auto &child : children) {
	if (mk_addmember(child.c_str(), &members.l, nullptr, WMOP_UNION) == WMEMBER_NULL) {
	    mk_freemembers(&members.l);
	    diagnose(Severity::Error, "sheet_group_member", "failed to group connected sheets", &source);
	    return false;
	}
    }
    // A non-region, uncolored comb preserves the aggregate's hierarchy role
    // without imposing a display color or solid semantics on its children.
    const int status = mk_lfcomb(wdbp_, name.c_str(), &members, 0)
    if (status < 0) {
	diagnose(Severity::Error, "sheet_group_write", "failed to write connected-sheet group", &source);
	return false;
    }
    write_entity_attributes(name, source);
    if (db5_update_attribute(name.c_str(), "iges.semantic", "open_sheets", wdbp_->dbip) < 0 ||
	db5_update_attribute(name.c_str(), "iges.sheet_count", std::to_string(sheets.size()).c_str(), wdbp_->dbip) < 0 ||
	db5_update_attribute(name.c_str(), "iges.face_count", std::to_string(component.faces.size()).c_str(), wdbp_->dbip) < 0) {
	diagnose(Severity::Error, "sheet_group_attributes", "failed to record connected-sheet group", &source);
	return false;
    }
    // Ownership partitioning ensures these faces share the original aggregate's
    // source containers.  Keep those references on the comb, not on each child.
    for (const auto *face : component.faces)
	objects_[face->id] = name;
    for (const auto &child : children)
	root_objects_.erase(child);
    root_objects_.insert(name);
    ++result_.statistics.groups_written;
    return true;
}

void
Importer::write_trimmed_components(TrimmedComponent &component)
{
    progress("separating closed solids and retained sheets", component.faces.front()->id);
    const auto write = [&](const TrimmedComponent &part) {
	if (!write_trimmed_component(part))
	    result_.statistics.omitted += part.faces.size();
    };
    const auto connected = connected_face_components(*component.brep);
    if (connected.size() == 1 || std::none_of(connected.begin(), connected.end(),
	[](const auto &item) { return item.second.closed; })) {
	write(component);
	return;
    }

    // Separate solids and possible cavities before the open remainder is
    // partitioned into sheet children.  Color never drives this partition.
    std::vector<std::vector<int> > groups;
    std::vector<int> open_faces;
    for (const auto &item : connected) {
	if (item.second.closed)
	    groups.push_back(item.second.faces);
	else
	    open_faces.insert(open_faces.end(), item.second.faces.begin(), item.second.faces.end());
    }
    if (!open_faces.empty()) {
	std::sort(open_faces.begin(), open_faces.end());
	groups.push_back(std::move(open_faces));
    }

    std::vector<TrimmedComponent> parts;
    std::vector<ON_BoundingBox> closed_bounds;
    bool extracted = true;
    for (const auto &indices : groups) {
	TrimmedComponent part;
	if (!extract_component(component, indices, part)) {
	    extracted = false;
	    break;
	}
	bool closed = part.brep->m_E.Count() > 0;
	for (int i = 0; i < part.brep->m_E.Count(); ++i)
	    closed = closed && part.brep->m_E[i].m_ti.Count() == 2;
	if (closed && (!brep_orient_faces(*part.brep) || !part.brep->IsValid() ||
		!part.brep->IsSolid() || !periodic_trims_in_domain(*part.brep))) {
	    diagnose(Severity::Warning, "closed_component_orientation",
		"could not orient a closed component; retained its original geometry",
		part.faces.front());
	    if (!extract_component(component, indices, part)) {
		extracted = false;
		break;
	    }
	}
	const ON_BoundingBox bounds = closed ? part.brep->BoundingBox() : ON_BoundingBox();
	if (closed && !bounds.IsValid()) {
	    extracted = false;
	    break;
	}
	closed_bounds.push_back(bounds);
	parts.push_back(std::move(part));
    }
    if (!extracted) {
	diagnose(Severity::Warning, "component_extraction",
	    "could not separate connected components; retained the validated assembly",
	    component.faces.front());
	write(component);
	return;
    }

    /* Disjoint closed shells can be written independently.  Overlapping
     * boxes may denote a cavity or intersecting shells, so keep those
     * shells together with their relative orientation, without guessing
     * containment or forcing every shell to enclose positive material. */
    std::vector<int> parents(parts.size());
    ON_RTree tree;
    bool grouped = true;
    for (size_t i = 0; i < parts.size() && grouped; ++i) {
	parents[i] = static_cast<int>(i);
	if (closed_bounds[i].IsValid())
	    grouped = tree.Insert(closed_bounds[i].m_min, closed_bounds[i].m_max, static_cast<int>(i));
    }
    for (size_t i = 0; i < parts.size() && grouped; ++i) {
	if (!closed_bounds[i].IsValid())
	    continue;
	ON_BoundingBox bounds = closed_bounds[i];
	const ON_3dVector padding(tolerance_, tolerance_, tolerance_);
	bounds.m_min -= padding;
	bounds.m_max += padding;
	ON_SimpleArray<int> candidates;
	grouped = tree.Search(bounds.m_min, bounds.m_max, candidates);
	for (int candidate = 0; candidate < candidates.Count(); ++candidate)
	    join_components(parents, static_cast<int>(i), candidates[candidate]);
    }
    std::map<int, TrimmedComponent> outputs;
    for (size_t i = 0; i < parts.size() && grouped; ++i) {
	TrimmedComponent &output = outputs[component_root(parents, static_cast<int>(i))];
	if (!output.brep)
	    output = std::move(parts[i]);
	else if (!append_component(output, parts[i])) {
	    grouped = false;
	    break;
	}
    }
    for (const auto &output : outputs)
	grouped = grouped && output.second.brep->IsValid() &&
	    periodic_trims_in_domain(*output.second.brep);
    if (!grouped) {
	diagnose(Severity::Warning, "component_grouping",
	    "could not preserve closed-shell grouping; retained the validated assembly",
	    component.faces.front());
	write(component);
	return;
    }
    for (const auto &output : outputs)
	write(output.second);
}

void
Importer::import_trimmed_components(const std::vector<const DirectoryEntry *> &faces)
{
    std::vector<TrimmedComponent> components;
    collect_trimmed_components(faces, components);
    if (components.empty())
	return;
    if (components.size() == 1) {
	write_trimmed_components(components.front());
	return;
    }

    TrimmedComponent joined;
    bool appended = true;
    for (const TrimmedComponent &component : components)
	appended = appended && append_component(joined, component);
    brep_assembly_result assembly;
    progress("reassembling recovered face batches", faces.front()->id);
    if (appended && brep_assemble(*joined.brep, tolerance_, &assembly) &&
	periodic_trims_in_domain(*joined.brep)) {
	result_.statistics.reassembly_edges_merged += assembly.merged_edges;
	write_trimmed_components(joined);
	return;
    }
    std::string reason = "periodic trim requires a surface seam split";
    if (!appended)
	reason = "face append failed";
    else if (!assembly.valid)
	reason = assembly.validation_log.empty() ?
	    "topology assembly failed (error " + std::to_string(assembly.error) + ")" :
	    assembly.validation_log;
    diagnose(Severity::Warning, "component_reassembly",
	"could not reassemble retained faces; preserved the validated batches: " + reason,
	faces.front());
    for (TrimmedComponent &component : components)
	write_trimmed_components(component);
}

bool
Importer::write_standalone_surface(const DirectoryEntry &entry)
{
    SolidBuilder geometry(*this, entry, Matrix());
    std::unique_ptr<ON_Surface> surface = entry.type == 128 ?
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
    if (!write_geometry(name, *brep)) {
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
	options_.output == GeometryOutput::Brep ? "direct-opennurbs-surface" : "tessellated-opennurbs-surface",
	wdbp_->dbip);
    objects_[entry.id] = name;
    root_objects_.insert(name);
    count_geometry(*brep);
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
    if (entry.type == 402 && entry.form == 9) {
	int parent_count = 0;
	if (!parameter_integer(parameters, 1, parent_count) || parent_count != 1 ||
		!parameter_integer(parameters, 2, count) || count < 0 ||
		count >= MAX_ENTITY_LIST_COUNT)
	    return false;
	++count;
	first_member = 3;
    } else if (entry.type == 402 &&
	    (entry.form == 1 || entry.form == 7)) {
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
Importer::resolve_hierarchy_object(EntityId id, std::set<EntityId> &active)
{
    if (objects_.count(id) || unresolved_objects_.count(id))
	return true;
    const DirectoryEntry *entry = document_.entity(id);
    if (!entry)
	return true;
    switch (entry->type) {
	case 180: return write_boolean_tree(id, active);
	case 184: case 308: return write_container(id, active);
	case 408: return write_instance(*entry, active);
	case 430: return write_solid_instance(*entry, active);
	case 402:
	    if (entry->form == 1 || entry->form == 7 || entry->form == 9)
		return write_container(id, active);
	    break;
    }
    // Missing and unsupported leaves are diagnosed by their referring parent.
    return true;
}

bool
Importer::write_boolean_tree(EntityId id, std::set<EntityId> &active)
{
    if (objects_.find(id) != objects_.end() || unresolved_objects_.count(id))
	return true;
    const DirectoryEntry *entry = document_.entity(id);
    const ParameterList *parameters = entry ? document_.parameters(id) : nullptr;
    int token_count = 0;
    if (!entry || entry->type != 180 ||
	    !parameter_integer(parameters, 1, token_count) || token_count < 1 ||
	    token_count > MAX_ENTITY_LIST_COUNT) {
	diagnose(Severity::Warning, "boolean_tree_parameters",
	    "Boolean Tree has an invalid postfix token count", entry);
	unresolved_objects_.insert(id);
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
	unresolved_objects_.insert(id);
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
	    if (object == objects_.end()) {
		if (!resolve_hierarchy_object(operand_id, active)) {
		    release_stack();
		    active.erase(id);
		    return false;
		}
		object = objects_.find(operand_id);
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
    SolidProperties properties;
    solid_properties(document_, *entry, properties);
    combination->region_flag = properties.source_entity ? properties.region_flag : 1;
    combination->region_id = properties.ident;
    combination->aircode = properties.air;
    combination->GIFTmater = properties.material;
    combination->los = properties.line_of_sight;
    combination->inherit = properties.inherit;
    combination->rgb_valid = properties.has_color;
    std::copy(properties.color.begin(), properties.color.end(), combination->rgb);
    if (!properties.shader_name.empty()) {
	bu_vls_strcpy(&combination->shader, properties.shader_name.c_str());
	if (!properties.shader_arguments.empty())
	    bu_vls_printf(&combination->shader, " %s", properties.shader_arguments.c_str());
    }
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
    if (objects_.find(id) != objects_.end() || unresolved_objects_.count(id))
	return true;
    const DirectoryEntry *entry = document_.entity(id);
    if (!entry)
	return true;
    if (!active.insert(id).second) {
	diagnose(Severity::Warning, "hierarchy_cycle",
	    "cyclic IGES group or subfigure reference was omitted", entry);
	return true;
    }

    std::vector<EntityId> source_members;
    if (!container_members(*entry, source_members)) {
	active.erase(id);
	diagnose(Severity::Warning, "hierarchy_parameters",
	    "IGES group or subfigure has invalid member parameters", entry);
	unresolved_objects_.insert(id);
	++result_.statistics.omitted;
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
	    if (!resolve_hierarchy_object(member_id, active)) {
		active.erase(id);
		mk_freemembers(&members.l);
		return false;
	    }
	    object = objects_.find(member_id);
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
	Matrix member_placement = transform(entry->transform);
	if (entry->type == 184) {
	    const ParameterList *parameters = document_.parameters(entry->id);
	    EntityId matrix_id;
	    const size_t matrix_parameter =
		2 + source_members.size() + member_index;
	    if (parameter_entity(parameters, matrix_parameter, matrix_id)) {
		member_placement = multiply(member_placement, transform(matrix_id));
	    }
	}
	combination_matrix(member_placement, output_member->wm_mat);
    }
    active.erase(id);
    if (unresolved) {
	result_.statistics.unresolved_members += unresolved;
	diagnose(Severity::Warning, "unresolved_container_members",
	    "group or subfigure has " + std::to_string(unresolved) + " unresolved members", entry);
    }
    if (output_members.empty()) {
	/* Repeated instances of an empty definition must not repeat its
	 * entire traversal or multiply the count of missing source members. */
	unresolved_objects_.insert(id);
	return true;
    }

    const std::string name = unique_name(*entry, hierarchy_name(*entry));
    SolidProperties properties;
    solid_properties(document_, *entry, properties);
    const int write_status = mk_lrcomb(wdbp_, name.c_str(), &members,
	properties.region_flag, properties.shader_name.c_str(),
	properties.shader_arguments.c_str(), properties.has_color ? properties.color.data() : nullptr,
	properties.ident, properties.air, properties.material, properties.line_of_sight, properties.inherit);
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
    SolidProperties properties;
    solid_properties(document_, entry, properties);
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
    if (objects_.find(entry.id) != objects_.end() || unresolved_objects_.count(entry.id))
	return true;
    if (!active.insert(entry.id).second) {
	diagnose(Severity::Warning, "hierarchy_cycle",
	    "cyclic IGES Subfigure Instance reference was omitted", &entry);
	++result_.statistics.omitted;
	return true;
    }
    /* Every exit, including recursive construction failures, must release
     * this instance from the active dependency path. */
    struct ActiveInstance {
	std::set<EntityId> &active;
	EntityId id;
	~ActiveInstance() { active.erase(id); }
    } active_instance{active, entry.id};
    const ParameterList *parameters = document_.parameters(entry.id);
    EntityId definition_id;
    if (!parameter_entity(parameters, 1, definition_id)) {
	diagnose(Severity::Warning, "subfigure_entity_parameters",
	    "Subfigure Instance has no valid definition reference", &entry);
	unresolved_objects_.insert(entry.id);
	++result_.statistics.omitted;
	return true;
    }
    if (!write_container(definition_id, active))
	return false;
    const auto definition = objects_.find(definition_id);
    if (definition == objects_.end()) {
	diagnose(Severity::Warning, "subfigure_instance_unresolved",
	    "Subfigure Instance definition contains no imported geometry", &entry);
	unresolved_objects_.insert(entry.id);
	++result_.statistics.omitted;
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
	unresolved_objects_.insert(entry.id);
	++result_.statistics.omitted;
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
    if (objects_.find(entry.id) != objects_.end() || unresolved_objects_.count(entry.id))
	return true;
    const ParameterList *parameters = document_.parameters(entry.id);
    EntityId definition_id;
    if (entry.type != 430 ||
	    !parameter_entity(parameters, 1, definition_id)) {
	diagnose(Severity::Warning, "solid_entity_parameters",
	    "Solid Instance has no valid definition reference", &entry);
	unresolved_objects_.insert(entry.id);
	++result_.statistics.omitted;
	return true;
    }
    if (!active.insert(entry.id).second) {
	diagnose(Severity::Warning, "hierarchy_cycle",
	    "cyclic IGES Solid Instance reference was omitted", &entry);
	++result_.statistics.omitted;
	return true;
    }

    auto definition = objects_.find(definition_id);
    if (definition == objects_.end()) {
	if (!resolve_hierarchy_object(definition_id, active)) {
	    active.erase(entry.id);
	    return false;
	}
	definition = objects_.find(definition_id);
    }

    if (definition == objects_.end()) {
	active.erase(entry.id);
	diagnose(Severity::Warning, "solid_instance_unresolved",
	    "Solid Instance definition contains no imported geometry", &entry);
	unresolved_objects_.insert(entry.id);
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
    if (options_.progress)
	options_.progress("hierarchy", "resolving groups, instances, and Boolean trees", 0, 0, 0);
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
    if (options_.progress)
	options_.progress("hierarchy", "writing geometry root", 0, 0, 0);
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


bool
Importer::read_profile(EntityId id, ProfileCurves &curves, const Matrix &parent,
    std::set<EntityId> &active)
{
    // Cycles and very deep composite definitions must not exhaust the stack.
    constexpr size_t MAX_PROFILE_DEPTH = 512;
    const DirectoryEntry *entry = document_.entity(id);
    if (!entry || active.size() >= MAX_PROFILE_DEPTH || !active.insert(id).second)
	return false;
    bool success = false;
    const ParameterList *parameters = document_.parameters(id);
    if (entry->type == 102) {
	int count = 0;
	success = parameter_integer(parameters, 1, count) && count > 0 &&
	    static_cast<size_t>(count) <= parameters->values.size() - 2;
	const Matrix placement = multiply(parent, transform(entry->transform));
	for (int index = 0; success && index < count; ++index) {
	    EntityId child;
	    success = parameter_entity(parameters, index + 2, child) &&
		read_profile(child, curves, placement, active);
	}
    } else if (entry->type == 112) {
	int dimensions = 0;
	int count = 0;
	constexpr size_t CUBIC_COEFFICIENTS = 4;
	constexpr size_t COEFFICIENTS_PER_SEGMENT = 3 * CUBIC_COEFFICIENTS;
	success = parameter_integer(parameters, 3, dimensions) &&
	    (dimensions == 2 || dimensions == 3) &&
	    parameter_integer(parameters, 4, count) && count > 0 &&
	    parameters->values.size() >= 6 &&
	    static_cast<size_t>(count) <= (parameters->values.size() - 6) / (COEFFICIENTS_PER_SEGMENT + 1);
	for (int segment = 0; success && segment < count; ++segment) {
	    double start = 0.0;
	    double end = 0.0;
	    success = parameter_real(parameters, 5 + segment, start) &&
		parameter_real(parameters, 6 + segment, end) && end > start;
	    const double span = end - start;
	    std::array<Point3, CUBIC_COEFFICIENTS> controls;
	    for (size_t coordinate = 0; success && coordinate < 3; ++coordinate) {
		std::array<double, CUBIC_COEFFICIENTS> coefficient;
		for (size_t power = 0; success && power < coefficient.size(); ++power)
		    success = parameter_real(parameters, 6 + count +
			COEFFICIENTS_PER_SEGMENT * segment + CUBIC_COEFFICIENTS * coordinate + power,
			coefficient[power]);
		if (!success)
		    break;
		// Convert power-basis coefficients on the segment interval to
		// cubic Bernstein controls, preserving the polynomial exactly.
		const double a = coefficient[0];
		const double b = coefficient[1] * span;
		const double c = coefficient[2] * span * span;
		const double d = coefficient[3] * span * span * span;
		controls[0][coordinate] = a;
		controls[1][coordinate] = a + b / 3.0;
		controls[2][coordinate] = a + (2.0 * b + c) / 3.0;
		controls[3][coordinate] = a + b + c + d;
	    }
	    if (!success)
		break;
	    ON_BezierCurve bezier(3, false, CUBIC_COEFFICIENTS);
	    for (size_t cv = 0; cv < controls.size(); ++cv) {
		const Point3 placed = model_point(*entry, controls[cv], parent);
		success = bezier.SetCV(cv, ON_3dPoint(placed.data())) && success;
	    }
	    auto curve = std::make_unique<ON_NurbsCurve>();
	    success = success && bezier.GetNurbForm(*curve) && curve->IsValid();
	    if (success)
		curves.push_back(std::move(curve));
	}
    } else if (entry->type == 106) {
	int dimensions = 0;
	int count = 0;
	success = parameter_integer(parameters, 1, dimensions) &&
	    (dimensions == 1 || dimensions == 2 || dimensions == 3) &&
	    parameter_integer(parameters, 2, count) && count >= 2;
	const size_t stride = dimensions == 1 ? 2 : dimensions == 2 ? 3 : 6;
	size_t parameter = dimensions == 1 ? 4 : 3;
	double depth = 0.0;
	if (dimensions == 1)
	    success = success && parameter_real(parameters, 3, depth);
	success = success && parameters && parameter <= parameters->values.size() &&
	    static_cast<size_t>(count) <= (parameters->values.size() - parameter) / stride;
	ON_3dPoint previous;
	for (int index = 0; success && index < count; ++index, parameter += stride) {
	    Point3 point = {0.0, 0.0, depth};
	    success = parameter_real(parameters, parameter, point[0]) &&
		parameter_real(parameters, parameter + 1, point[1]) &&
		(dimensions == 1 || parameter_real(parameters, parameter + 2, point[2]));
	    if (!success)
		break;
	    const Point3 placed = model_point(*entry, point, parent);
	    const ON_3dPoint current(placed.data());
	    if (index > 0 && current.DistanceTo(previous) > ON_ZERO_TOLERANCE) {
		auto line = std::make_unique<ON_NurbsCurve>();
		success = ON_LineCurve(previous, current).GetNurbForm(*line) > 0;
		if (success)
		    curves.push_back(std::move(line));
	    }
	    previous = current;
	}
    } else {
	SolidBuilder geometry(*this, *entry);
	auto curve = geometry.curve(*entry, true, parent);
	success = curve != nullptr;
	if (success)
	    curves.push_back(std::move(curve));
    }
    active.erase(id);
    return success;
}

BrepImportResult
Importer::run()
{
    if (!wdbp_ || !wdbp_->dbip) {
	diagnose(Severity::Fatal, "output_database",
	    "no writable BRL-CAD database was supplied");
	return result_;
    }
    if (options_.output != GeometryOutput::Brep && options_.output != GeometryOutput::Mesh &&
	options_.output != GeometryOutput::Polygon) {
	diagnose(Severity::Fatal, "invalid_output_mode", "unknown geometry output mode");
	return result_;
    }
    if (!std::isfinite(options_.default_plate_thickness) ||
	    options_.default_plate_thickness < 0.0) {
	diagnose(Severity::Fatal, "plate_mode_thickness",
	    "default plate thickness must be a finite non-negative value");
	return result_;
    }
    if (!std::isfinite(options_.relative_tolerance) || options_.relative_tolerance < 0.0 ||
	!std::isfinite(options_.maximum_repair_tolerance) || options_.maximum_repair_tolerance < 0.0) {
	diagnose(Severity::Fatal, "invalid_repair_tolerance",
	    "repair tolerances must be finite non-negative values");
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
    size_t bounded_plane_count = 0;
    for (const DirectoryEntry *association : document_.find(402)) {
	std::vector<EntityId> members;
	if (association->form != 9 || !container_members(*association, members) || members.empty())
	    continue;
	const DirectoryEntry *parent = document_.entity(members.front());
	if (!parent || parent->type != 108 || parent->form != 1)
	    continue;
	for (size_t i = 1; i < members.size(); ++i) {
	    const DirectoryEntry *hole = document_.entity(members[i]);
	    if (hole && hole->type == 108 && hole->form == -1) {
		plane_holes_[parent->id].push_back(hole->id);
		referenced_surfaces.insert(hole->id);
	    }
	}
    }
    for (const DirectoryEntry &entry : document_.entities())
	if (is_surface_entity(entry.type) &&
		referenced_surfaces.find(entry.id) == referenced_surfaces.end()) {
	    EntityId boundary;
	    if (entry.type == 108 && entry.form == 1 &&
		    parameter_entity(document_.parameters(entry.id), 5, boundary)) {
		bounded_faces.push_back(&entry);
		++bounded_plane_count;
	    } else {
		standalone_surfaces.push_back(&entry);
	    }
	}

    result_.statistics.solids_seen = solids.size();
    result_.statistics.trimmed_surfaces_seen = trimmed_surfaces.size();
    result_.statistics.bounded_surfaces_seen = bounded_surfaces.size();
    result_.statistics.standalone_surfaces_seen =
	standalone_surfaces.size() + bounded_plane_count;
    for (const auto &entry : document_.entities())
	if (native_solid_name(entry.type))
	    ++result_.statistics.native_solids_seen;
    progress_total_ = solids.size() + standalone_surfaces.size() + bounded_faces.size() +
	result_.statistics.native_solids_seen;

    for (size_t index = 0; index < solids.size(); ++index, ++progress_completed_) {
	const DirectoryEntry *solid = solids[index];
	progress("constructing explicit solid", solid->id);
	const size_t diagnostic_count = result_.diagnostics.size();
	const size_t repair_count = result_.statistics.repairs;
	SolidBuilder builder(*this, *solid);
	std::unique_ptr<ON_Brep> brep = builder.build();
	const std::string name = unique_name(*solid);
	std::optional<BrepOrientationResult> prepared_orientation;
	// A complete manifold with inconsistent face senses is not IsSolid().
	// Resolve that case before deciding that source faces must be recovered.
	if (brep && !brep->IsSolid() && safe_repairs())
	    prepared_orientation = orient_geometry(name, *brep);
	const bool invalid_solid = !brep || !brep->IsSolid();
	if (invalid_solid) {
	    discard_failed_repairs(diagnostic_count, repair_count);
	    if (options_.invalid_brep == InvalidBrepPolicy::Reject || options_.strict || options_.exact ||
		options_.output != GeometryOutput::Brep) {
		diagnose(Severity::Warning, "rejected_invalid_solid",
		    "explicit manifold solid could not be reconstructed as a valid solid", solid);
		++result_.statistics.omitted;
		continue;
	    }
	    brep = builder.preserve_faces();
	    if (!brep || (brep->m_F.Count() && !brep->IsValid())) {
		diagnose(Severity::Error, "invalid_solid_preservation",
		    "could not construct a safely serializable partial solid", solid);
		++result_.statistics.omitted;
		continue;
	    }
	}
	if (!write_geometry(name, *brep, invalid_solid, prepared_orientation ? &*prepared_orientation : nullptr)) {
	    diagnose(Severity::Error, "brep_write",
		"failed to write direct OpenNURBS B-Rep", solid);
	    ++result_.statistics.omitted;
	    continue;
	}
	if (!invalid_solid && !write_plate_mode_attributes(name, *brep, *solid)) {
	    ++result_.statistics.omitted;
	    continue;
	}
	write_entity_attributes(name, *solid);
	if (!write_entity_color_attribute(name, *solid)) {
	    ++result_.statistics.omitted;
	    continue;
	}
	{
	    std::vector<const DirectoryEntry *> faces;
	    for (int i = 0; i < brep->m_F.Count(); ++i) {
		const DirectoryEntry *face = document_.entity(EntityId(brep->m_F[i].m_face_user.i));
		if (face)
		    faces.push_back(face);
	    }
	    if (!write_face_metadata(name, *brep, faces, builder.relaxed_tolerances()) ||
		    !write_repair_attributes(name, builder.relaxed_tolerances(), *solid)) {
		++result_.statistics.omitted;
		continue;
	    }
	}
	if (invalid_solid) {
	    if (db5_update_attribute(name.c_str(), RT_BREP_INVALID_SOLID_ATTRIBUTE, "1", wdbp_->dbip) < 0 ||
		db5_update_attribute(name.c_str(), "iges.import_status", "invalid_solid", wdbp_->dbip) < 0 ||
		db5_update_attribute(name.c_str(), "iges.shell_metadata", builder.shell_metadata().c_str(), wdbp_->dbip) < 0) {
		diagnose(Severity::Error, "invalid_solid_attributes",
		    "could not preserve the explicit solid's invalid status and shell map", solid);
		++result_.statistics.omitted;
		continue;
	    }
	    diagnose(Severity::Warning, "preserved_invalid_solid",
		"retained explicit solid intent with " + std::to_string(brep->m_F.Count()) +
		" reconstructed faces and " + std::to_string(builder.missing_faces()) +
		" unreconstructed faces; repair required before analysis", solid);
	    ++result_.statistics.invalid_solids_written;
	    result_.statistics.unreconstructed_faces += builder.missing_faces();
	}
	db5_update_attribute(name.c_str(), "iges.topology",
	    options_.output == GeometryOutput::Brep ? "direct-opennurbs" : "tessellated-opennurbs", wdbp_->dbip);
	objects_[solid->id] = name;
	root_objects_.insert(name);
	count_geometry(*brep, invalid_solid);
    }
    for (size_t index = 0; index < standalone_surfaces.size(); ++index, ++progress_completed_) {
	const DirectoryEntry *surface = standalone_surfaces[index];
	progress("constructing standalone surface", surface->id);
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
    progress("partitioning faces by source ownership");
    for (const DirectoryEntry &entry : document_.entities()) {
	std::vector<EntityId> members;
	if (!container_members(entry, members))
	    continue;
	for (EntityId member : members) {
	    const DirectoryEntry *member_entry = document_.entity(member);
	    if (member_entry &&
		    (member_entry->type == 108 || member_entry->type == 143 || member_entry->type == 144))
		owners[member].push_back(entry.id);
	}
    }
    std::map<std::vector<EntityId>,
	std::vector<const DirectoryEntry *> > partitions;
    for (const DirectoryEntry *face : bounded_faces)
	partitions[owners[face->id]].push_back(face);
    for (const auto &partition : partitions)
	import_trimmed_components(partition.second);

    for (const auto &entry : document_.entities()) {
	const char *stem = native_solid_name(entry.type);
	if (!stem)
	    continue;
	progress("constructing native solid", entry.id);
	++progress_completed_;
	const std::string source = name_property(document_, entry);
	const std::string native_stem = source.empty() ? std::string(stem) + '.' +
	    std::to_string((entry.id.value() - 1) / 2) : sanitized_database_name(source);
	std::string name = native_stem;
	for (size_t suffix = 1; db_lookup(wdbp_->dbip, name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL; ++suffix)
	    name = native_stem + '_' + std::to_string(suffix);
	// Native object names and collision suffixes are retained for scripts
	// that refer to the established names of unlabeled CSG entities.
	size_t metadata_parameter = 0;
	if (associativity_parameter(document_.parameters(entry.id), entry, metadata_parameter)) {
	    if (!property_entities(document_, document_.parameters(entry.id), metadata_parameter).valid) {
		++result_.statistics.repairs;
		diagnose(options_.strict || options_.exact || options_.repair == RepairMode::None ?
		    Severity::Error : Severity::Warning, "invalid_property_reference",
		    "optional metadata has invalid counts or entity references", &entry);
	    }
	}
	std::string error;
	const ProfileReader read = [&](EntityId profile, ProfileCurves &curves) {
	    std::set<EntityId> active;
	    return read_profile(profile, curves, Matrix(), active);
	};
	const NativeBrepWriter write = [&](const std::string &object, ON_Brep &brep) {
	    if (!write_geometry(object, brep))
		return false;
	    count_geometry(brep, false);
	    return true;
	};
	if (!write_native_solid(document_, entry, wdbp_, name, unit_to_mm_,
		tolerance_, read, write, error)) {
	    diagnose(Severity::Warning, "native_solid_import",
		error.empty() ? "failed to write native solid" : error, &entry);
	    ++result_.statistics.omitted;
	    continue;
	}
	if (register_native(entry.id, name.c_str()))
	    ++result_.statistics.native_solids_written;
	else
	    ++result_.statistics.omitted;
    }

    progress("geometry processing complete");
    return complete_hierarchy();
}

bool
Importer::register_native(EntityId id, const char *name)
{
    const DirectoryEntry *entry = document_.entity(id);
    struct directory *dp = db_lookup(wdbp_->dbip, name, LOOKUP_QUIET);
    if (!entry || !dp)
	return false;
    /* Native writers emit local geometry.  Bake the directory transform
     * exactly once, as the BRep path does, before sharing hierarchy code. */
    if (entry->transform.value()) {
	mat_t matrix;
	combination_matrix(transform(entry->transform), matrix);
	Internal internal;
	if (rt_db_get_internal(&internal.value, dp, wdbp_->dbip, matrix) < 0 ||
	    rt_db_put_internal(dp, wdbp_->dbip, &internal.value) < 0) {
	    diagnose(Severity::Error, "native_transform",
		"failed to apply the native primitive's directory transform", entry);
	    return false;
	}
    }
    write_entity_attributes(name, *entry);
    if (!write_entity_color_attribute(name, *entry))
	return false;
    objects_[id] = name;
    root_objects_.insert(name);
    return true;
}

BrepImportResult
Importer::complete_hierarchy()
{
    result_.success = false;
    if (!write_hierarchy())
	return result_;
    /* Reconcile roots against the final database so a completed instance is not
     * accompanied by an unplaced copy of its definition. */
    db_update_nref(wdbp_->dbip);
    for (auto root = root_objects_.begin(); root != root_objects_.end();) {
	const struct directory *entry = db_lookup(wdbp_->dbip, root->c_str(), LOOKUP_QUIET);
	if (!entry || entry->d_nref)
	    root = root_objects_.erase(root);
	else
	    ++root;
    }
    if (!write_root())
	return result_;
    const bool errors = std::any_of(result_.diagnostics.begin(), result_.diagnostics.end(),
	[](const ImportDiagnostic &diagnostic) {
	    return diagnostic.severity == Severity::Error || diagnostic.severity == Severity::Fatal;
	});
    result_.success = !errors && !root_objects_.empty() && (!options_.strict ||
	(!result_.statistics.omitted && !result_.statistics.unresolved_members));
    return result_;
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
read_model_curves(const Document &document, EntityId id, ProfileCurves &curves,
    std::string &error)
{
    ImportOptions options;
    brep_import_detail::Importer reader(document, nullptr, options);
    std::set<EntityId> active;
    if (reader.read_profile(id, curves, brep_import_detail::Matrix(), active))
	return true;
    error = "unsupported, cyclic, or invalid curve definition";
    return false;
}


} /* namespace iges */
} /* namespace brlcad */
