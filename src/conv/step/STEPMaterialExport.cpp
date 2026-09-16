/*                  S T E P M A T E R I A L E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "STEPMaterialExport.h"
#include "AP_Common.h"
#include "STEPGeneratedAPI.h"
#include "STEPString.h"
#include "StepExportPlan.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

static bool
step_material_set_attribute(STEPentity *entity, const char *name,
    const std::string &value, InstMgr *instances)
{
    if (!entity || !name) return false;
    bool found = false;
    bool valid = true;
    entity->ResetAttributes();
    for (STEPattribute *attribute = entity->NextAttribute(); attribute;
	 attribute = entity->NextAttribute()) {
	if (std::string(attribute->Name()) != name) continue;
	found = true;
	if (attribute->StrToVal(value.c_str(), instances) <= SEVERITY_WARNING)
	    valid = false;
    }
    return found && valid;
}

static bool
step_material_set_entity_attribute(STEPentity *entity, const char *name,
    STEPentity *value)
{
    return brlcad::step::SetEntity(entity, name, value);
}

static bool
step_material_set_unit_attribute(STEPentity *entity, const char *name,
    STEPentity *unit)
{
    return brlcad::step::SetEntity(entity, name, unit);
}

static bool
step_material_set_numeric_measure_attribute(STEPentity *entity,
    const char *name, double value)
{
    return brlcad::step::SetSelectReal(entity, name, "NUMERIC_MEASURE", value);
}

static STEPentity *
step_material_create(AP203_Contents *contents, const char *type)
{
    if (!contents || !contents->registry || !contents->instance_list)
	return NULL;
    return contents->registry->ObjCreate(type);
}

static STEPentity *
step_material_append(AP203_Contents *contents, STEPentity *entity)
{
    if (contents && entity) contents->instance_list->Append(entity, completeSE);
    return entity;
}

static std::string
step_material_reference(const STEPentity *entity)
{
    return entity ? "#" + std::to_string(entity->StepFileId()) : "$";
}

static std::string
step_material_contexts(STEPentity *product)
{
    std::ostringstream value;
    value << '(';
    bool first = true;
    STEPaggregate *contexts = product ?
	brlcad::step::Aggregate(product, "frame_of_reference") : NULL;
    for (EntityNode *node = contexts ?
	    static_cast<EntityNode *>(contexts->GetHead()) : NULL;
	 node; node = static_cast<EntityNode *>(node->NextNode())) {
	STEPentity *context = dynamic_cast<STEPentity *>(node->node);
	if (!context || context->StepFileId() <= 0) continue;
	if (!first) value << ',';
	first = false;
	value << step_material_reference(context);
    }
    value << ')';
    return first ? std::string() : value.str();
}

static std::string
step_material_lower_trim(std::string value)
{
    const std::string whitespace = " \t\r\n";
    const size_t begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos) return std::string();
    const size_t end = value.find_last_not_of(whitespace);
    value = value.substr(begin, end - begin + 1);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
	return static_cast<char>(std::tolower(c));
    });
    return value;
}

static bool
step_material_parse_numbers(const std::string &text,
    std::vector<double> &values)
{
    values.clear();
    const char *cursor = text.c_str();
    while (*cursor) {
	while (*cursor && (std::isspace(static_cast<unsigned char>(*cursor)) ||
		*cursor == ',' || *cursor == ';')) ++cursor;
	if (!*cursor) break;
	char *end = NULL;
	const double value = std::strtod(cursor, &end);
	if (!end || end == cursor || !std::isfinite(value)) return false;
	values.push_back(value);
	cursor = end;
	if (*cursor && !std::isspace(static_cast<unsigned char>(*cursor)) &&
		*cursor != ',' && *cursor != ';') return false;
    }
    return !values.empty();
}

static std::string
step_material_real(double value)
{
    std::ostringstream out;
    out << std::setprecision(17) << value;
    std::string result = out.str();
    if (result.find_first_of(".eE") == std::string::npos) result += '.';
    return result;
}

static std::string
step_material_real_aggregate(const std::vector<double> &values)
{
    std::ostringstream out;
    out << '(';
    for (size_t i = 0; i < values.size(); ++i) {
	if (i) out << ',';
	out << step_material_real(values[i]);
    }
    out << ')';
    return out.str();
}

static const char *
step_material_measure_type(const std::string &value_type)
{
    if (value_type == "volume_measure") return "VOLUME_MEASURE";
    if (value_type == "area_measure") return "AREA_MEASURE";
    if (value_type == "length_measure") return "LENGTH_MEASURE";
    if (value_type == "non_negative_length_measure")
	return "NON_NEGATIVE_LENGTH_MEASURE";
    if (value_type == "positive_length_measure")
	return "POSITIVE_LENGTH_MEASURE";
    if (value_type == "mass_measure") return "MASS_MEASURE";
    if (value_type == "time_measure") return "TIME_MEASURE";
    if (value_type == "electric_current_measure")
	return "ELECTRIC_CURRENT_MEASURE";
    if (value_type == "thermodynamic_temperature_measure")
	return "THERMODYNAMIC_TEMPERATURE_MEASURE";
    if (value_type == "celsius_temperature_measure")
	return "CELSIUS_TEMPERATURE_MEASURE";
    if (value_type == "amount_of_substance_measure")
	return "AMOUNT_OF_SUBSTANCE_MEASURE";
    if (value_type == "luminous_intensity_measure")
	return "LUMINOUS_INTENSITY_MEASURE";
    if (value_type == "plane_angle_measure") return "PLANE_ANGLE_MEASURE";
    if (value_type == "positive_plane_angle_measure")
	return "POSITIVE_PLANE_ANGLE_MEASURE";
    if (value_type == "solid_angle_measure") return "SOLID_ANGLE_MEASURE";
    if (value_type == "positive_ratio_measure") return "POSITIVE_RATIO_MEASURE";
    if (value_type == "ratio_measure") return "RATIO_MEASURE";
    if (value_type == "count_measure") return "COUNT_MEASURE";
    if (value_type == "parameter_value") return "PARAMETER_VALUE";
    if (value_type == "context_dependent_measure")
	return "CONTEXT_DEPENDENT_MEASURE";
    if (value_type == "numeric_measure") return "NUMERIC_MEASURE";
    return NULL;
}

static std::array<double, 7>
step_material_default_dimensions(const std::string &value_type)
{
    std::array<double, 7> result = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    brlcad::step::ConstrainedMeasureDimensions(value_type, result);
    return result;
}

struct StepMaterialNamedUnitSpec {
    std::string subtype;
    std::string prefix;
    std::string name;
    std::string conversion_name;
    std::string conversion_measure_type;
    std::string conversion_base;
    double conversion_factor = 0.0;
    std::array<double, 7> dimensions = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};

    bool conversion() const { return !conversion_name.empty(); }
};

struct StepMaterialDerivedUnitElement {
    StepMaterialNamedUnitSpec unit;
    double exponent = 1.0;
};

static std::array<double, 7>
step_material_base_dimension(size_t index)
{
    std::array<double, 7> result = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    if (index < result.size()) result[index] = 1.0;
    return result;
}

static bool
step_material_named_unit(const std::string &source,
    StepMaterialNamedUnitSpec &unit)
{
    const std::string name = step_material_lower_trim(source);
    unit = StepMaterialNamedUnitSpec();
    const auto si = [&unit](const char *subtype, const char *prefix,
	    const char *si_name, size_t dimension) {
	unit.subtype = subtype;
	unit.prefix = prefix ? prefix : "";
	unit.name = si_name;
	unit.dimensions = step_material_base_dimension(dimension);
    };

    if (name == "m" || name == "metre" || name == "meter")
	si("length_unit", "", "metre", 0);
    else if (name == "mm" || name == "millimetre" || name == "millimeter")
	si("length_unit", "milli", "metre", 0);
    else if (name == "cm" || name == "centimetre" || name == "centimeter")
	si("length_unit", "centi", "metre", 0);
    else if (name == "km" || name == "kilometre" || name == "kilometer")
	si("length_unit", "kilo", "metre", 0);
    else if (name == "g" || name == "gram")
	si("mass_unit", "", "gram", 1);
    else if (name == "kg" || name == "kilogram")
	si("mass_unit", "kilo", "gram", 1);
    else if (name == "mg" || name == "milligram")
	si("mass_unit", "milli", "gram", 1);
    else if (name == "s" || name == "second")
	si("time_unit", "", "second", 2);
    else if (name == "a" || name == "ampere")
	si("electric_current_unit", "", "ampere", 3);
    else if (name == "k" || name == "kelvin")
	si("thermodynamic_temperature_unit", "", "kelvin", 4);
    else if (name == "mol" || name == "mole")
	si("amount_of_substance_unit", "", "mole", 5);
    else if (name == "cd" || name == "candela")
	si("luminous_intensity_unit", "", "candela", 6);
    else if (name == "rad" || name == "radian") {
	unit.subtype = "plane_angle_unit";
	unit.name = "radian";
    } else if (name == "sr" || name == "steradian") {
	unit.subtype = "solid_angle_unit";
	unit.name = "steradian";
    } else if (name == "in" || name == "inch" || name == "inches") {
	unit.subtype = "length_unit";
	unit.conversion_name = "INCH";
	unit.conversion_measure_type = "LENGTH_MEASURE";
	unit.conversion_base = "metre";
	unit.conversion_factor = 0.0254;
	unit.dimensions = step_material_base_dimension(0);
    } else if (name == "ft" || name == "foot" || name == "feet") {
	unit.subtype = "length_unit";
	unit.conversion_name = "FOOT";
	unit.conversion_measure_type = "LENGTH_MEASURE";
	unit.conversion_base = "metre";
	unit.conversion_factor = 0.3048;
	unit.dimensions = step_material_base_dimension(0);
    } else if (name == "lb" || name == "pound" || name == "pounds") {
	unit.subtype = "mass_unit";
	unit.conversion_name = "POUND";
	unit.conversion_measure_type = "MASS_MEASURE";
	unit.conversion_base = "kilogram";
	unit.conversion_factor = 0.45359237;
	unit.dimensions = step_material_base_dimension(1);
    } else if (name == "deg" || name == "degree" || name == "degrees") {
	unit.subtype = "plane_angle_unit";
	unit.conversion_name = "DEGREE";
	unit.conversion_measure_type = "PLANE_ANGLE_MEASURE";
	unit.conversion_base = "radian";
	unit.conversion_factor = 0.017453292519943295;
    }
    return !unit.subtype.empty();
}

static bool
step_material_derived_unit(const std::string &source,
    std::vector<StepMaterialDerivedUnitElement> &elements)
{
    elements.clear();
    const std::string expression = step_material_lower_trim(source);
    if (expression.find_first_of("*/^") == std::string::npos) return false;
    size_t cursor = 0;
    bool first = true;
    while (cursor < expression.size()) {
	while (cursor < expression.size() &&
		std::isspace(static_cast<unsigned char>(expression[cursor]))) ++cursor;
	bool divisor = false;
	if (!first) {
	    if (cursor >= expression.size() ||
		    (expression[cursor] != '*' && expression[cursor] != '/'))
		return false;
	    divisor = expression[cursor++] == '/';
	    while (cursor < expression.size() &&
		    std::isspace(static_cast<unsigned char>(expression[cursor]))) ++cursor;
	}
	const size_t name_begin = cursor;
	while (cursor < expression.size() && expression[cursor] != '*' &&
		expression[cursor] != '/' && expression[cursor] != '^' &&
		!std::isspace(static_cast<unsigned char>(expression[cursor]))) ++cursor;
	if (cursor == name_begin) return false;
	StepMaterialDerivedUnitElement element;
	if (!step_material_named_unit(expression.substr(name_begin,
		cursor - name_begin), element.unit)) return false;
	while (cursor < expression.size() &&
		std::isspace(static_cast<unsigned char>(expression[cursor]))) ++cursor;
	if (cursor < expression.size() && expression[cursor] == '^') {
	    ++cursor;
	    while (cursor < expression.size() &&
		    std::isspace(static_cast<unsigned char>(expression[cursor]))) ++cursor;
	    const char *begin = expression.c_str() + cursor;
	    char *end = NULL;
	    element.exponent = std::strtod(begin, &end);
	    if (!end || end == begin || !std::isfinite(element.exponent)) return false;
	    cursor = static_cast<size_t>(end - expression.c_str());
	}
	if (divisor) element.exponent = -element.exponent;
	elements.push_back(element);
	first = false;
	while (cursor < expression.size() &&
		std::isspace(static_cast<unsigned char>(expression[cursor]))) ++cursor;
    }
    return elements.size() > 1 ||
	(!elements.empty() && std::fabs(elements.front().exponent - 1.0) > 1.0e-12);
}

static bool
step_material_dimensions_equal(const std::array<double, 7> &left,
    const std::array<double, 7> &right)
{
    return brlcad::step::MeasureDimensionsEqual(left, right);
}

static STEPentity *
step_material_dimensions(AP203_Contents *contents,
    const std::array<double, 7> &dimensions)
{
    STEPentity *entity = step_material_create(contents, "DIMENSIONAL_EXPONENTS");
    const char *exponents[] = {"length_exponent", "mass_exponent",
	"time_exponent", "electric_current_exponent",
	"thermodynamic_temperature_exponent", "amount_of_substance_exponent",
	"luminous_intensity_exponent"};
    bool valid = entity != NULL;
    for (size_t i = 0; i < dimensions.size(); ++i)
	valid = valid && step_material_set_attribute(entity, exponents[i],
	    step_material_real(dimensions[i]), contents->instance_list);
    if (!valid) return NULL;
    return step_material_append(contents, entity);
}

static STEPentity *
step_material_si_unit(AP203_Contents *contents,
    const StepMaterialNamedUnitSpec &unit)
{
    const char *types[5] = {NULL, NULL, NULL, NULL, NULL};
    size_t type_count = 0;
    if (!unit.subtype.empty()) types[type_count++] = unit.subtype.c_str();
    types[type_count++] = "named_unit";
    types[type_count++] = "si_unit";
    types[type_count++] = "*";
    STEPcomplex *complex = new STEPcomplex(contents->registry, types,
	contents->instance_list->InstanceCount() + 1);
    STEPcomplex *si = complex ? complex->EntityPart("si_unit") : NULL;
    bool valid = si && step_material_set_attribute(si, "name",
	'.' + unit.name + '.', contents->instance_list);
    if (valid && !unit.prefix.empty())
	valid = step_material_set_attribute(si, "prefix",
	    '.' + unit.prefix + '.', contents->instance_list);
    if (!valid) {
	delete complex;
	return NULL;
    }
    step_material_append(contents, static_cast<STEPentity *>(complex));
    return static_cast<STEPentity *>(complex);
}

static bool
step_material_unit_subtype(const std::string &subtype)
{
    static const char *subtypes[] = {
	"length_unit", "mass_unit", "time_unit", "electric_current_unit",
	"thermodynamic_temperature_unit", "amount_of_substance_unit",
	"luminous_intensity_unit", "area_unit", "volume_unit",
	"plane_angle_unit", "solid_angle_unit", "ratio_unit"
    };
    if (subtype.empty()) return true;
    for (const char *candidate : subtypes)
	if (subtype == candidate) return true;
    return false;
}

static bool
step_material_si_symbol(const std::string &symbol, bool prefix)
{
    static const char *prefixes[] = {
	"exa", "peta", "tera", "giga", "mega", "kilo", "hecto",
	"deca", "deci", "centi", "milli", "micro", "nano", "pico",
	"femto", "atto"
    };
    static const char *names[] = {
	"metre", "gram", "second", "ampere", "kelvin", "mole",
	"candela", "radian", "steradian", "hertz", "newton", "pascal",
	"joule", "watt", "coulomb", "volt", "farad", "ohm", "siemens",
	"weber", "tesla", "henry", "degree_celsius", "lumen", "lux",
	"becquerel", "gray", "sievert"
    };
    if (prefix && symbol.empty()) return true;
    if (prefix) {
	for (const char *candidate : prefixes)
	    if (symbol == candidate) return true;
	return false;
    }
    for (const char *candidate : names)
	if (symbol == candidate) return true;
    return false;
}

static bool
step_material_structure_dimensions(const brlcad::step::UnitStructure &unit,
    std::array<double, 7> &dimensions, std::string &error, size_t depth)
{
    if (depth > 16) {
	error = "the retained STEP unit graph exceeds 16 nested levels";
	return false;
    }
    dimensions = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    if (unit.kind == "derived") {
	if (unit.components.empty() ||
		unit.exponents.size() != unit.components.size()) {
	    error = "the retained derived unit has inconsistent elements and exponents";
	    return false;
	}
	for (size_t i = 0; i < unit.components.size(); ++i) {
	    if (unit.components[i].kind == "derived") {
		error = "a STEP derived-unit element must be a named unit";
		return false;
	    }
	    std::array<double, 7> component;
	    if (!step_material_structure_dimensions(unit.components[i], component,
		    error, depth + 1)) return false;
	    for (size_t j = 0; j < dimensions.size(); ++j)
		dimensions[j] += component[j] * unit.exponents[i];
	}
    } else if (unit.kind == "conversion") {
	if (unit.components.size() != 1) {
	    error = "a retained conversion unit must have exactly one factor unit";
	    return false;
	}
	if (!step_material_structure_dimensions(unit.components.front(),
		dimensions, error, depth + 1)) return false;
    } else if (unit.has_dimensions) {
	dimensions = unit.dimension_exponents;
    } else {
	error = "the retained named unit has no dimensional exponents";
	return false;
    }
    if (unit.has_dimensions &&
	    !step_material_dimensions_equal(dimensions, unit.dimension_exponents)) {
	error = "the retained STEP unit dimensions disagree with its component graph";
	return false;
    }
    return true;
}

static STEPentity *
step_material_emit_structured_unit(const brlcad::step::UnitStructure &unit,
    AP203_Contents *contents, std::string &error, size_t depth)
{
    if (depth > 16 || !step_material_unit_subtype(unit.subtype)) {
	error = depth > 16 ? "the retained STEP unit graph exceeds 16 nested levels" :
	    "the retained STEP unit subtype is not common to the enabled schemas";
	return NULL;
    }
    if (unit.kind == "si") {
	const std::string prefix = step_material_lower_trim(unit.prefix);
	const std::string name = step_material_lower_trim(unit.name);
	if (!step_material_si_symbol(prefix, true) ||
		!step_material_si_symbol(name, false)) {
	    error = "the retained SI prefix or name is not an EXPRESS-defined symbol";
	    return NULL;
	}
	StepMaterialNamedUnitSpec spec;
	spec.subtype = unit.subtype;
	spec.prefix = prefix;
	spec.name = name;
	spec.dimensions = unit.dimension_exponents;
	STEPentity *result = step_material_si_unit(contents, spec);
	if (!result) error = "could not create the retained SI unit";
	return result;
    }
    if (unit.kind == "context" || unit.kind == "named") {
	if (!unit.has_dimensions ||
		(unit.kind == "context" && unit.name.empty()) ||
		(unit.kind == "named" && unit.subtype.empty())) {
	    error = "the retained context/named unit is incomplete";
	    return NULL;
	}
	STEPentity *dimension_entity = step_material_dimensions(contents,
	    unit.dimension_exponents);
	if (!dimension_entity) {
	    error = "could not create retained named-unit dimensions";
	    return NULL;
	}
	if (unit.kind == "named") {
	    STEPentity *named = step_material_create(contents, unit.subtype.c_str());
	    if (!named || !step_material_set_entity_attribute(named, "dimensions",
		    dimension_entity)) {
		error = "could not create the retained named unit";
		return NULL;
	    }
	    return step_material_append(contents, named);
	}
	if (unit.subtype.empty()) {
	    STEPentity *context = step_material_create(contents,
		"CONTEXT_DEPENDENT_UNIT");
	    if (!context || !step_material_set_entity_attribute(context, "dimensions",
		    dimension_entity) || !step_material_set_attribute(context, "name",
		    brlcad::step::encode_string(unit.name), contents->instance_list)) {
		error = "could not create the retained context-dependent unit";
		return NULL;
	    }
	    return step_material_append(contents, context);
	}
	const char *types[5] = {"context_dependent_unit", unit.subtype.c_str(),
	    "named_unit", "*", NULL};
	STEPcomplex *complex = new STEPcomplex(contents->registry, types,
	    contents->instance_list->InstanceCount() + 1);
	STEPcomplex *context = complex ?
	    complex->EntityPart("context_dependent_unit") : NULL;
	STEPcomplex *named = complex ? complex->EntityPart("named_unit") : NULL;
	if (!context || !named || !step_material_set_attribute(context, "name",
		brlcad::step::encode_string(unit.name), contents->instance_list) ||
		!step_material_set_entity_attribute(named, "dimensions", dimension_entity)) {
	    delete complex;
	    error = "could not create the retained typed context-dependent unit";
	    return NULL;
	}
	return step_material_append(contents, static_cast<STEPentity *>(complex));
    }
    if (unit.kind == "conversion") {
	if (unit.name.empty() || !unit.has_conversion_value ||
		unit.conversion_value_type.empty() || unit.components.size() != 1) {
	    error = "the retained conversion-based unit is incomplete";
	    return NULL;
	}
	const std::string factor_value_type = step_material_lower_trim(
	    unit.conversion_value_type);
	const char *factor_type = step_material_measure_type(factor_value_type);
	if (!factor_type) {
	    error = "the retained conversion factor has an unsupported measure type";
	    return NULL;
	}
	STEPentity *base = step_material_emit_structured_unit(
	    unit.components.front(), contents, error, depth + 1);
	STEPentity *factor = step_material_create(contents, "MEASURE_WITH_UNIT");
	const std::string typed_factor = std::string(factor_type) + '(' +
	    step_material_real(unit.conversion_value) + ')';
	if (!base || !factor || !step_material_set_attribute(factor,
		"value_component", typed_factor, contents->instance_list) ||
		!step_material_set_unit_attribute(factor, "unit_component", base)) {
	    error = "could not create the retained conversion-unit factor";
	    return NULL;
	}
	step_material_append(contents, factor);
	STEPentity *dimension_entity = step_material_dimensions(contents,
	    unit.dimension_exponents);
	if (!dimension_entity) {
	    error = "could not create retained conversion-unit dimensions";
	    return NULL;
	}
	const char *types[5] = {"conversion_based_unit", NULL, "named_unit", "*", NULL};
	if (!unit.subtype.empty()) types[1] = unit.subtype.c_str();
	else {
	    types[1] = "named_unit";
	    types[2] = "*";
	    types[3] = NULL;
	}
	STEPcomplex *complex = new STEPcomplex(contents->registry, types,
	    contents->instance_list->InstanceCount() + 1);
	STEPcomplex *conversion = complex ?
	    complex->EntityPart("conversion_based_unit") : NULL;
	STEPcomplex *named = complex ? complex->EntityPart("named_unit") : NULL;
	if (!conversion || !named || !step_material_set_attribute(conversion,
		"name", brlcad::step::encode_string(unit.name), contents->instance_list) ||
		!step_material_set_entity_attribute(conversion, "conversion_factor", factor) ||
		!step_material_set_entity_attribute(named, "dimensions", dimension_entity)) {
	    delete complex;
	    error = "could not create the retained conversion-based unit";
	    return NULL;
	}
	return step_material_append(contents, static_cast<STEPentity *>(complex));
    }
    if (unit.kind != "derived" || unit.components.empty() ||
	    unit.components.size() != unit.exponents.size()) {
	error = "the retained derived unit is incomplete";
	return NULL;
    }
	std::vector<STEPentity *> element_entities;
	for (size_t i = 0; i < unit.components.size(); ++i) {
	    if (unit.components[i].kind == "derived") {
		error = "a STEP derived-unit element cannot itself be a derived unit";
		return NULL;
	    }
	    STEPentity *component = step_material_emit_structured_unit(
		unit.components[i], contents, error, depth + 1);
	    STEPentity *element = step_material_create(contents,
		"DERIVED_UNIT_ELEMENT");
	    if (!component || !element || !step_material_set_attribute(element,
		    "unit", step_material_reference(component), contents->instance_list) ||
		    !step_material_set_attribute(element, "exponent",
		    step_material_real(unit.exponents[i]), contents->instance_list)) {
		error = "could not create a retained derived-unit element";
		return NULL;
	    }
	    step_material_append(contents, element);
	    element_entities.push_back(element);
	}
	std::ostringstream references;
	references << '(';
	for (size_t i = 0; i < element_entities.size(); ++i) {
	    if (i) references << ',';
	    references << step_material_reference(element_entities[i]);
	}
	references << ')';
	STEPentity *derived = step_material_create(contents, "DERIVED_UNIT");
	if (!derived || !step_material_set_attribute(derived, "elements",
		references.str(), contents->instance_list)) {
	    error = "could not create the retained derived unit";
	    return NULL;
	}
	return step_material_append(contents, derived);
}

static STEPentity *
step_material_named_unit_entity(AP203_Contents *contents,
    const StepMaterialNamedUnitSpec &unit, std::string &error)
{
    if (!unit.conversion()) {
	STEPentity *result = step_material_si_unit(contents, unit);
	if (!result) error = "could not create the property SI unit";
	return result;
    }

    StepMaterialNamedUnitSpec base;
    if (!step_material_named_unit(unit.conversion_base, base) || base.conversion()) {
	error = "the conversion unit has no supported SI base";
	return NULL;
    }
    STEPentity *base_unit = step_material_si_unit(contents, base);
    STEPentity *factor = step_material_create(contents, "MEASURE_WITH_UNIT");
    const std::string typed_factor = unit.conversion_measure_type + '(' +
	step_material_real(unit.conversion_factor) + ')';
    if (!base_unit || !factor || !step_material_set_attribute(factor,
	    "value_component", typed_factor, contents->instance_list) ||
	    !step_material_set_unit_attribute(factor, "unit_component", base_unit)) {
	error = "could not create the conversion-unit factor";
	return NULL;
    }
    step_material_append(contents, factor);
    STEPentity *dimension_entity = step_material_dimensions(contents,
	unit.dimensions);
    if (!dimension_entity) {
	error = "could not create the conversion-unit dimensions";
	return NULL;
    }

    const char *types[5] = {"conversion_based_unit", unit.subtype.c_str(),
	"named_unit", "*", NULL};
    STEPcomplex *complex = new STEPcomplex(contents->registry, types,
	contents->instance_list->InstanceCount() + 1);
    STEPcomplex *conversion = complex ?
	complex->EntityPart("conversion_based_unit") : NULL;
    STEPcomplex *named = complex ? complex->EntityPart("named_unit") : NULL;
    const bool valid = conversion && named &&
	step_material_set_attribute(conversion, "name",
	    brlcad::step::encode_string(unit.conversion_name), contents->instance_list) &&
	step_material_set_entity_attribute(conversion, "conversion_factor", factor) &&
	step_material_set_entity_attribute(named, "dimensions", dimension_entity);
    if (!valid) {
	delete complex;
	error = "could not create the property conversion-based unit";
	return NULL;
    }
    step_material_append(contents, static_cast<STEPentity *>(complex));
    return static_cast<STEPentity *>(complex);
}

static STEPentity *
step_material_emit_unit(const brlcad::step::ExportPropertyPlan &property,
    const std::string &value_type, const std::array<double, 7> &dimensions,
    AP203_Contents *contents, std::string &error)
{
    StepMaterialNamedUnitSpec named;
    std::vector<StepMaterialDerivedUnitElement> derived;
    const bool has_named = step_material_named_unit(property.units, named);
    const bool has_derived = !has_named &&
	step_material_derived_unit(property.units, derived);
    std::array<double, 7> unit_dimensions = {{0.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 0.0}};
    if (has_named) {
	unit_dimensions = named.dimensions;
    } else if (has_derived) {
	for (const StepMaterialDerivedUnitElement &element : derived) {
	    if (!element.unit.conversion() && element.unit.name == "gram" &&
		    element.unit.prefix != "kilo") {
		error = "an SI mass unit used in a STEP derived unit must be kilogram";
		return NULL;
	    }
	    for (size_t i = 0; i < unit_dimensions.size(); ++i)
		unit_dimensions[i] += element.unit.dimensions[i] * element.exponent;
	}
    }
    const bool arbitrary_numeric = value_type == "numeric_measure" ||
	value_type == "context_dependent_measure" || value_type == "parameter_value";
    std::string expected_subtype;
    if (value_type == "length_measure" ||
	    value_type == "non_negative_length_measure" ||
	    value_type == "positive_length_measure") expected_subtype = "length_unit";
    else if (value_type == "mass_measure") expected_subtype = "mass_unit";
    else if (value_type == "time_measure") expected_subtype = "time_unit";
    else if (value_type == "electric_current_measure")
	expected_subtype = "electric_current_unit";
    else if (value_type == "thermodynamic_temperature_measure" ||
	    value_type == "celsius_temperature_measure")
	expected_subtype = "thermodynamic_temperature_unit";
    else if (value_type == "amount_of_substance_measure")
	expected_subtype = "amount_of_substance_unit";
    else if (value_type == "luminous_intensity_measure")
	expected_subtype = "luminous_intensity_unit";
    else if (value_type == "plane_angle_measure" ||
	    value_type == "positive_plane_angle_measure")
	expected_subtype = "plane_angle_unit";
    else if (value_type == "solid_angle_measure")
	expected_subtype = "solid_angle_unit";
    else if (value_type == "area_measure") expected_subtype = "area_unit";
    else if (value_type == "volume_measure") expected_subtype = "volume_unit";
    else if (value_type == "ratio_measure" ||
	    value_type == "positive_ratio_measure") expected_subtype = "ratio_unit";

    if (property.unit_structure_requested) {
	if (property.unit_structure_malformed || property.unit_structure.empty()) {
	    error = property.unit_structure_error.empty() ?
		"the retained STEP unit graph is malformed" :
		property.unit_structure_error;
	    return NULL;
	}
	std::array<double, 7> retained_dimensions;
	if (!step_material_structure_dimensions(property.unit_structure,
		retained_dimensions, error, 0)) return NULL;
	if (property.unit_structure.kind != "derived" && !arbitrary_numeric &&
		(expected_subtype.empty() ||
		 property.unit_structure.subtype != expected_subtype)) {
	    error = "the retained STEP unit kind does not match value_type '" +
		value_type + "'";
	    return NULL;
	}
	if (!arbitrary_numeric &&
		!step_material_dimensions_equal(retained_dimensions, dimensions)) {
	    error = "the retained STEP unit dimensions do not match value_type '" +
		value_type + "'";
	    return NULL;
	}
	if (!property.dimensions.empty() &&
		!step_material_dimensions_equal(retained_dimensions, dimensions)) {
	    error = "the retained STEP unit dimensions do not match the explicit exponents";
	    return NULL;
	}
	return step_material_emit_structured_unit(property.unit_structure,
	    contents, error, 0);
    }
    if (has_named && !arbitrary_numeric &&
	    (expected_subtype.empty() || named.subtype != expected_subtype)) {
	error = "the standardized property unit kind does not match value_type '" +
	    value_type + "'";
	return NULL;
    }
    if ((has_named || has_derived) && !arbitrary_numeric &&
	    !step_material_dimensions_equal(unit_dimensions, dimensions)) {
	error = "the standardized property unit dimensions do not match value_type '" +
	    value_type + "'";
	return NULL;
    }
    if ((has_named || has_derived) && !property.dimensions.empty() &&
	    !step_material_dimensions_equal(unit_dimensions, dimensions)) {
	error = "the standardized property unit dimensions do not match the explicit exponents";
	return NULL;
    }

    if (has_named)
	return step_material_named_unit_entity(contents, named, error);
    if (has_derived) {
	std::vector<STEPentity *> element_entities;
	for (const StepMaterialDerivedUnitElement &element : derived) {
	    STEPentity *unit = step_material_named_unit_entity(contents,
		element.unit, error);
	    STEPentity *unit_element = step_material_create(contents,
		"DERIVED_UNIT_ELEMENT");
	    if (!unit || !unit_element || !step_material_set_attribute(unit_element,
		    "unit", step_material_reference(unit), contents->instance_list) ||
		    !step_material_set_attribute(unit_element, "exponent",
		    step_material_real(element.exponent), contents->instance_list)) {
		error = "could not create a derived-unit element";
		return NULL;
	    }
	    step_material_append(contents, unit_element);
	    element_entities.push_back(unit_element);
	}
	std::ostringstream references;
	references << '(';
	for (size_t i = 0; i < element_entities.size(); ++i) {
	    if (i) references << ',';
	    references << step_material_reference(element_entities[i]);
	}
	references << ')';
	STEPentity *unit = step_material_create(contents, "DERIVED_UNIT");
	if (!unit || !step_material_set_attribute(unit, "elements",
		references.str(), contents->instance_list)) {
	    error = "could not create the property derived unit";
	    return NULL;
	}
	return step_material_append(contents, unit);
    }

    if (property.units.find_first_of("*/^") != std::string::npos) {
	error = "unsupported standardized property unit expression '" +
	    property.units + "'";
	return NULL;
    }
    STEPentity *dimension_entity = step_material_dimensions(contents, dimensions);
    STEPentity *unit = step_material_create(contents, "CONTEXT_DEPENDENT_UNIT");
    const std::string unit_name = property.units.empty() ?
	value_type + "_unit" : property.units;
    if (!dimension_entity || !unit || !step_material_set_attribute(unit,
	    "dimensions", step_material_reference(dimension_entity),
	    contents->instance_list) || !step_material_set_attribute(unit, "name",
	    brlcad::step::encode_string(unit_name), contents->instance_list)) {
	error = "could not create the property context-dependent unit";
	return NULL;
    }
    return step_material_append(contents, unit);
}

static bool
step_material_emit_property(
    const brlcad::step::ExportPropertyPlan &property,
    STEPentity *material_definition, AP203_Contents *contents,
    std::string &error)
{
    error.clear();
    if (!material_definition || !contents || !contents->default_context) {
	error = "missing property definition target or representation context";
	return false;
    }

    const std::string value_type = step_material_lower_trim(property.value_type);
    std::vector<double> values;
    if (value_type == "descriptive") {
	if (property.text.empty()) {
	    error = "a descriptive property has no text";
	    return false;
	}
	if (!property.values.empty() || !property.units.empty() ||
		!property.dimensions.empty()) {
	    error = "a descriptive property also specifies numeric semantics";
	    return false;
	}
    } else if (value_type == "cartesian_point") {
	if (!step_material_parse_numbers(property.values, values) ||
		values.size() > 3) {
	    error = "a Cartesian property requires one through three finite values";
	    return false;
	}
	if (!property.text.empty() || !property.units.empty() ||
		!property.dimensions.empty()) {
	    error = "a Cartesian property specifies unsupported text or unit semantics";
	    return false;
	}
    } else {
	if (!step_material_measure_type(value_type)) {
	    error = "unsupported or missing property value_type '" +
		property.value_type + "'";
	    return false;
	}
	if (!step_material_parse_numbers(property.values, values) ||
		values.size() != 1) {
	    error = "a measure property requires exactly one finite value";
	    return false;
	}
	if (!property.text.empty()) {
	    error = "a measure property also specifies descriptive text";
	    return false;
	}
    }

    const std::string item_name = property.name.empty() ?
	"material property" : property.name;
    STEPentity *item = NULL;
    if (value_type == "descriptive") {
	item = step_material_create(contents, "DESCRIPTIVE_REPRESENTATION_ITEM");
	if (!item || !step_material_set_attribute(item, "name",
		brlcad::step::encode_string(item_name), contents->instance_list) ||
		!step_material_set_attribute(item, "description",
		brlcad::step::encode_string(property.text), contents->instance_list)) {
	    error = "could not create DESCRIPTIVE_REPRESENTATION_ITEM";
	    return false;
	}
	step_material_append(contents, item);
    } else if (value_type == "cartesian_point") {
	item = step_material_create(contents, "CARTESIAN_POINT");
	if (!item || !step_material_set_attribute(item, "name",
		brlcad::step::encode_string(item_name), contents->instance_list) ||
		!step_material_set_attribute(item, "coordinates",
		step_material_real_aggregate(values), contents->instance_list)) {
	    error = "could not create CARTESIAN_POINT property item";
	    return false;
	}
	step_material_append(contents, item);
    } else {
	std::array<double, 7> dimensions =
	    step_material_default_dimensions(value_type);
	if (!property.dimensions.empty()) {
	    std::vector<double> parsed_dimensions;
	    if (!step_material_parse_numbers(property.dimensions,
		    parsed_dimensions) || parsed_dimensions.size() != 7) {
		error = "measure dimensions must contain seven finite exponents";
		return false;
	    }
	    std::copy(parsed_dimensions.begin(), parsed_dimensions.end(),
		dimensions.begin());
	}

	STEPentity *unit = step_material_emit_unit(property, value_type,
	    dimensions, contents, error);
	if (!unit) return false;

	item = step_material_create(contents, "MEASURE_REPRESENTATION_ITEM");
	const std::string typed_value = std::string(
	    step_material_measure_type(value_type)) + '(' +
	    step_material_real(values.front()) + ')';
	if (!item) {
	    error = "could not create MEASURE_REPRESENTATION_ITEM entity";
	    return false;
	}
	const bool name_set = step_material_set_attribute(item, "name",
	    brlcad::step::encode_string(item_name), contents->instance_list);
	const bool value_set = value_type == "numeric_measure" ?
	    step_material_set_numeric_measure_attribute(item, "value_component",
		values.front()) :
	    step_material_set_attribute(item, "value_component",
		typed_value, contents->instance_list);
	const bool unit_set = step_material_set_unit_attribute(item,
	    "unit_component", unit);
	if (!name_set || !value_set || !unit_set) {
	    error = "could not set MEASURE_REPRESENTATION_ITEM ";
	    if (!name_set) error += "name";
	    else if (!value_set) error += "value_component";
	    else error += "unit_component";
	    return false;
	}
	step_material_append(contents, item);
    }

    STEPentity *representation = step_material_create(contents, "REPRESENTATION");
    if (!representation || !step_material_set_attribute(representation, "name",
	    brlcad::step::encode_string(item_name), contents->instance_list) ||
	    !step_material_set_attribute(representation, "items",
	    '(' + step_material_reference(item) + ')', contents->instance_list) ||
	    !step_material_set_attribute(representation, "context_of_items",
	    step_material_reference(contents->default_context), contents->instance_list)) {
	error = "could not create the property REPRESENTATION";
	return false;
    }
    step_material_append(contents, representation);

    STEPentity *definition = step_material_create(contents, "PROPERTY_DEFINITION");
    const std::string category = property.category.empty() ?
	"material property" : property.category;
    if (!definition || !step_material_set_attribute(definition, "name",
	    brlcad::step::encode_string(category), contents->instance_list) ||
	    !step_material_set_attribute(definition, "description",
	    brlcad::step::encode_string(property.description), contents->instance_list) ||
	    !step_material_set_attribute(definition, "definition",
	    step_material_reference(material_definition), contents->instance_list)) {
	error = "could not create the property PROPERTY_DEFINITION";
	return false;
    }
    step_material_append(contents, definition);

    STEPentity *link = step_material_create(contents,
	"PROPERTY_DEFINITION_REPRESENTATION");
    if (!link || !step_material_set_attribute(link, "definition",
	    step_material_reference(definition), contents->instance_list) ||
	    !step_material_set_attribute(link, "used_representation",
	    step_material_reference(representation), contents->instance_list)) {
	error = "could not create PROPERTY_DEFINITION_REPRESENTATION";
	return false;
    }
    step_material_append(contents, link);
    return true;
}

} // namespace

bool
brlcad::step::EmitSTEPProductProperty(const ExportPropertyPlan &property,
    STEPentity *shape_product_definition, AP203_Contents *contents,
    std::string &error)
{
    return step_material_emit_property(property, shape_product_definition,
	contents, error);
}

brlcad::step::MaterialExportResult
brlcad::step::EmitSTEPMaterialAssignment(const ExportMaterialPlan &material,
    STEPentity *shape_product_definition, AP203_Contents *contents)
{
    MaterialExportResult result;
    const auto fail_material = [&result, &material](const std::string &message) {
	result.diagnostics.push_back(message);
	result.properties_omitted += material.properties.size();
	return result;
    };
    if (!contents || !contents->instance_list || !shape_product_definition)
	return fail_material("missing STEP export context or shape product definition");

    STEPentity *shape = shape_product_definition;
    STEPentity *shape_formation = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(shape, "formation"));
    STEPentity *shape_product = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(shape_formation, "of_product"));
    STEPentity *definition_context = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(shape, "frame_of_reference"));
    const std::string product_contexts = step_material_contexts(shape_product);
    if (!shape || !shape_product || !definition_context || product_contexts.empty())
	return fail_material("shape product does not supply reusable STEP product contexts");

    std::string identifier = material.identifier;
    std::string name = material.name;
    if (identifier.empty()) identifier = name.empty() ? "material" : name;
    if (name.empty()) name = identifier;

    STEPentity *product = step_material_create(contents, "PRODUCT");
    if (!product ||
	!step_material_set_attribute(product, "id", encode_string(identifier),
	    contents->instance_list) ||
	!step_material_set_attribute(product, "name", encode_string(name),
	    contents->instance_list) ||
	!step_material_set_attribute(product, "description",
	    encode_string(material.description), contents->instance_list) ||
	!step_material_set_attribute(product, "frame_of_reference", product_contexts,
	    contents->instance_list))
	return fail_material("could not create the material PRODUCT");
    step_material_append(contents, product);

    STEPentity *formation = step_material_create(contents,
	"PRODUCT_DEFINITION_FORMATION");
    if (!formation ||
	!step_material_set_attribute(formation, "id", "'1'",
	    contents->instance_list) ||
	!step_material_set_attribute(formation, "description",
	    "'material revision'", contents->instance_list) ||
	!step_material_set_attribute(formation, "of_product",
	    step_material_reference(product), contents->instance_list))
	return fail_material("could not create the material formation");
    step_material_append(contents, formation);

    STEPentity *definition = step_material_create(contents,
	"PRODUCT_DEFINITION");
    if (!definition ||
	!step_material_set_attribute(definition, "id", "'material definition'",
	    contents->instance_list) ||
	!step_material_set_attribute(definition, "description", "''",
	    contents->instance_list) ||
	!step_material_set_attribute(definition, "formation",
	    step_material_reference(formation), contents->instance_list) ||
	!step_material_set_attribute(definition, "frame_of_reference",
	    step_material_reference(definition_context), contents->instance_list))
	return fail_material("could not create the material definition");
    step_material_append(contents, definition);

    STEPentity *dimensions = step_material_create(contents,
	"DIMENSIONAL_EXPONENTS");
    const char *exponents[] = {"length_exponent", "mass_exponent",
	"time_exponent", "electric_current_exponent",
	"thermodynamic_temperature_exponent", "amount_of_substance_exponent",
	"luminous_intensity_exponent"};
    bool dimensions_valid = dimensions != NULL;
    for (const char *exponent : exponents)
	dimensions_valid = dimensions_valid && step_material_set_attribute(
	    dimensions, exponent, "0.", contents->instance_list);
    if (!dimensions_valid)
	return fail_material("could not create the material count-unit dimensions");
    step_material_append(contents, dimensions);

    STEPentity *unit = step_material_create(contents, "NAMED_UNIT");
    if (!unit || !step_material_set_attribute(unit, "dimensions",
	    step_material_reference(dimensions), contents->instance_list))
	return fail_material("could not create the material count unit");
    step_material_append(contents, unit);

    STEPentity *quantity = step_material_create(contents, "MEASURE_WITH_UNIT");
    if (!quantity ||
	!step_material_set_attribute(quantity, "value_component",
	    "COUNT_MEASURE(1.)", contents->instance_list) ||
	!step_material_set_attribute(quantity, "unit_component",
	    step_material_reference(unit), contents->instance_list))
	return fail_material("could not create the material quantity");
    step_material_append(contents, quantity);

    STEPentity *usage = step_material_create(contents,
	"MAKE_FROM_USAGE_OPTION");
    const std::string assignment_identifier = material.ordinal <= 1 ?
	"material assignment" :
	"material assignment " + std::to_string(material.ordinal);
    if (!usage ||
	!step_material_set_attribute(usage, "id",
	    encode_string(assignment_identifier), contents->instance_list) ||
	!step_material_set_attribute(usage, "name", "'make from'",
	    contents->instance_list) ||
	!step_material_set_attribute(usage, "description", "''",
	    contents->instance_list) ||
	!step_material_set_attribute(usage, "relating_product_definition",
	    step_material_reference(shape), contents->instance_list) ||
	!step_material_set_attribute(usage, "related_product_definition",
	    step_material_reference(definition), contents->instance_list) ||
	!step_material_set_attribute(usage, "ranking",
	    std::to_string(material.ordinal ? material.ordinal : 1),
	    contents->instance_list) ||
	!step_material_set_attribute(usage, "ranking_rationale",
	    encode_string(name), contents->instance_list) ||
	!step_material_set_attribute(usage, "quantity",
	    step_material_reference(quantity), contents->instance_list))
	return fail_material("could not create the material assignment");
    step_material_append(contents, usage);
    result.material_emitted = true;

    for (const ExportPropertyPlan &property : material.properties) {
	std::string error;
	if (step_material_emit_property(property, definition, contents, error)) {
	    ++result.properties_emitted;
	} else {
	    ++result.properties_omitted;
	    result.diagnostics.push_back("property " +
		std::to_string(property.ordinal) + ": " + error);
	}
    }
    return result;
}
