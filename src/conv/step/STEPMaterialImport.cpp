/*                  S T E P M A T E R I A L I M P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "STEPMaterialImport.h"
#include "STEPDocument.h"
#include "STEPGeneratedAPI.h"
#include "STEPString.h"
#include "STEPWrapper.h"
#include "ap_schema.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace {

using brlcad::step::Material;
using brlcad::step::MetadataProperty;
using brlcad::step::UnitStructure;

std::string measure_type(SDAI_Select *value);

STEPentity *
material_relationship_definition(STEPWrapper &wrapper, STEPentity *relationship,
    bool relating)
{
    if (!relationship) return NULL;
    const char *name = relating ? "relating_product_definition" :
	"related_product_definition";
    SDAI_Application_instance *selected = brlcad::step::Entity(relationship, name);
    if (!selected) {
	SDAI_Select *select = wrapper.getSelectAttribute(relationship, name);
	selected = brlcad::step::SelectedEntity(select);
    }
    STEPentity *definition = dynamic_cast<STEPentity *>(selected);
    return definition && wrapper.IsSchemaEntity(definition, "PRODUCT_DEFINITION") ?
	definition : NULL;
}

std::string
lower_metadata_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
	return static_cast<char>(std::tolower(c));
    });
    return value;
}

STEPentity *
product_definition(STEPWrapper &wrapper, SDAI_Select *definition)
{
    if (!definition)
	return NULL;

    STEPentity *selected = dynamic_cast<STEPentity *>(
	brlcad::step::SelectedEntity(definition));
    if (!selected) return NULL;
    if (wrapper.IsSchemaEntity(selected, "PRODUCT_DEFINITION")) return selected;
    if (wrapper.IsSchemaEntity(selected, "PRODUCT_DEFINITION_RELATIONSHIP"))
	return material_relationship_definition(wrapper, selected, true);
    if (wrapper.IsSchemaEntity(selected, "PRODUCT_DEFINITION_SHAPE"))
	return product_definition(wrapper,
	    wrapper.getSelectAttribute(selected, "definition"));
    if (wrapper.IsSchemaEntity(selected, "SHAPE_ASPECT")) {
	STEPentity *shape = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(selected, "of_shape"));
	return shape ? product_definition(wrapper,
	    wrapper.getSelectAttribute(shape, "definition")) : NULL;
    }

    return NULL;
}

int64_t
product_id(STEPentity *definition)
{
    STEPentity *formation = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(definition, "formation"));
    STEPentity *product = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(formation, "of_product"));
    return product ? product->STEPfile_id : 0;
}

SDAI_Application_instance *
unit_component(STEPWrapper &wrapper, SDAI_Application_instance *unit,
    const char *name)
{
    if (!unit || !wrapper.SchemaEntity(name))
	return NULL;
    SDAI_Application_instance *head = unit->HeadEntity();
    if (!head) head = unit;
    const std::string expected = lower_metadata_ascii(name);

    for (SDAI_Application_instance *candidate = head; candidate;
	 candidate = candidate->GetNextMiEntity()) {
	SDAI_Application_instance *component = wrapper.getSuperType(candidate, name);
	if (component && lower_metadata_ascii(component->EntityName()) == expected) return component;
	if (lower_metadata_ascii(candidate->EntityName()) == expected) return candidate;
    }

    const int id = head->STEPfile_id > 0 ? head->STEPfile_id : unit->STEPfile_id;
    SDAI_Application_instance *component = id > 0 ? wrapper.getEntity(id, name) : NULL;
    return component && lower_metadata_ascii(component->EntityName()) == expected ? component : NULL;
}

std::string
si_unit_name(STEPWrapper &wrapper, SDAI_Application_instance *unit)
{
    SDAI_Application_instance *si = unit_component(wrapper, unit, "Si_Unit");
    if (!si)
	return std::string();
    const std::string prefix = wrapper.getEnumAttributeName(si, "prefix");
    const std::string name = wrapper.getEnumAttributeName(si, "name");
    std::string result;
    if (!prefix.empty() && prefix != "unset") result = prefix;
    if (!name.empty() && name != "unset") result += name;
    return result;
}

std::string
named_unit_name(STEPWrapper &wrapper, STEPentity *unit)
{
    if (!unit)
	return std::string();

    SDAI_Application_instance *context_dependent = unit_component(wrapper, unit,
	"Context_Dependent_Unit");
    if (context_dependent) {
	const std::string name = brlcad::step::decode_string(
	    wrapper.getStringAttribute(context_dependent, "name"));
	if (!name.empty()) return name;
    }
    SDAI_Application_instance *conversion = unit_component(wrapper, unit,
	"Conversion_Based_Unit");
    if (conversion) {
	const std::string name = conversion ? brlcad::step::decode_string(
	    wrapper.getStringAttribute(conversion, "name")) : std::string();
	if (!name.empty())
	    return name;
    }
    const std::string si_name = si_unit_name(wrapper, unit);
    if (!si_name.empty()) return si_name;

    if (wrapper.IsSchemaEntity(unit, "LENGTH_UNIT")) return "length_unit";
    if (wrapper.IsSchemaEntity(unit, "MASS_UNIT")) return "mass_unit";
    if (wrapper.IsSchemaEntity(unit, "AREA_UNIT")) return "area_unit";
    if (wrapper.IsSchemaEntity(unit, "VOLUME_UNIT")) return "volume_unit";
    if (wrapper.IsSchemaEntity(unit, "PLANE_ANGLE_UNIT")) return "plane_angle_unit";
    if (wrapper.IsSchemaEntity(unit, "SOLID_ANGLE_UNIT")) return "solid_angle_unit";

    return std::string("unit_step") + std::to_string(unit->STEPfile_id);
}

std::string
named_unit_subtype(STEPWrapper &wrapper, STEPentity *unit)
{
    if (!unit) return std::string();
    if (wrapper.IsSchemaEntity(unit, "LENGTH_UNIT")) return "length_unit";
    if (wrapper.IsSchemaEntity(unit, "MASS_UNIT")) return "mass_unit";
    if (wrapper.IsSchemaEntity(unit, "TIME_UNIT")) return "time_unit";
    if (wrapper.IsSchemaEntity(unit, "ELECTRIC_CURRENT_UNIT"))
	return "electric_current_unit";
    if (wrapper.IsSchemaEntity(unit, "THERMODYNAMIC_TEMPERATURE_UNIT"))
	return "thermodynamic_temperature_unit";
    if (wrapper.IsSchemaEntity(unit, "AMOUNT_OF_SUBSTANCE_UNIT"))
	return "amount_of_substance_unit";
    if (wrapper.IsSchemaEntity(unit, "LUMINOUS_INTENSITY_UNIT"))
	return "luminous_intensity_unit";
    if (wrapper.IsSchemaEntity(unit, "AREA_UNIT")) return "area_unit";
    if (wrapper.IsSchemaEntity(unit, "VOLUME_UNIT")) return "volume_unit";
    if (wrapper.IsSchemaEntity(unit, "PLANE_ANGLE_UNIT"))
	return "plane_angle_unit";
    if (wrapper.IsSchemaEntity(unit, "SOLID_ANGLE_UNIT"))
	return "solid_angle_unit";
    if (wrapper.IsSchemaEntity(unit, "RATIO_UNIT")) return "ratio_unit";
    return std::string();
}

std::string
exponent_string(double exponent)
{
    std::ostringstream out;
    const double integral = std::round(exponent);
    if (std::fabs(exponent - integral) < 1.0e-12)
	out << static_cast<long long>(integral);
    else
	out << std::setprecision(17) << exponent;
    return out.str();
}

std::string
unit_name(STEPWrapper &wrapper, SDAI_Select *unit)
{
    STEPentity *selected = dynamic_cast<STEPentity *>(
	brlcad::step::SelectedEntity(unit));
    if (!selected) return std::string();
    if (wrapper.IsSchemaEntity(selected, "NAMED_UNIT"))
	return named_unit_name(wrapper, selected);
    if (!wrapper.IsSchemaEntity(selected, "DERIVED_UNIT")) return std::string();

    const std::vector<SDAI_Application_instance *> elements =
	brlcad::step::Entities(selected, "elements");
    std::string result;
    for (std::vector<SDAI_Application_instance *>::const_iterator i = elements.begin();
	 i != elements.end(); ++i) {
	STEPentity *element = dynamic_cast<STEPentity *>(*i);
	STEPentity *named = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(element, "unit"));
	if (element && named) {
	    if (!result.empty()) result += '*';
	    result += named_unit_name(wrapper, named);
	    result += '^';
	    result += exponent_string(wrapper.getRealAttribute(element, "exponent"));
	}
    }
    return result;
}

bool
si_unit_dimensions(STEPWrapper &wrapper, STEPentity *unit,
    std::array<double, 7> &dimensions)
{
    SDAI_Application_instance *si = unit_component(wrapper, unit, "Si_Unit");
    if (!si) return false;
    const std::string name = lower_metadata_ascii(
	wrapper.getEnumAttributeName(si, "name"));
    dimensions = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    if (name == "metre") dimensions[0] = 1.0;
    else if (name == "gram") dimensions[1] = 1.0;
    else if (name == "second") dimensions[2] = 1.0;
    else if (name == "ampere") dimensions[3] = 1.0;
    else if (name == "kelvin" || name == "degree_celsius") dimensions[4] = 1.0;
    else if (name == "mole") dimensions[5] = 1.0;
    else if (name == "candela" || name == "lumen") dimensions[6] = 1.0;
    else if (name == "radian" || name == "steradian") {}
    else if (name == "hertz" || name == "becquerel") dimensions[2] = -1.0;
    else if (name == "newton")
	dimensions = {{1.0, 1.0, -2.0, 0.0, 0.0, 0.0, 0.0}};
    else if (name == "pascal")
	dimensions = {{-1.0, 1.0, -2.0, 0.0, 0.0, 0.0, 0.0}};
    else if (name == "joule")
	dimensions = {{2.0, 1.0, -2.0, 0.0, 0.0, 0.0, 0.0}};
    else if (name == "watt")
	dimensions = {{2.0, 1.0, -3.0, 0.0, 0.0, 0.0, 0.0}};
    else if (name == "coulomb")
	dimensions = {{0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0}};
    else if (name == "volt")
	dimensions = {{2.0, 1.0, -3.0, -1.0, 0.0, 0.0, 0.0}};
    else if (name == "farad")
	dimensions = {{-2.0, -1.0, 4.0, 1.0, 0.0, 0.0, 0.0}};
    else if (name == "ohm")
	dimensions = {{2.0, 1.0, -3.0, -2.0, 0.0, 0.0, 0.0}};
    else if (name == "siemens")
	dimensions = {{-2.0, -1.0, 3.0, 2.0, 0.0, 0.0, 0.0}};
    else if (name == "weber")
	dimensions = {{2.0, 1.0, -2.0, -1.0, 0.0, 0.0, 0.0}};
    else if (name == "tesla")
	dimensions = {{0.0, 1.0, -2.0, -1.0, 0.0, 0.0, 0.0}};
    else if (name == "henry")
	dimensions = {{2.0, 1.0, -2.0, -2.0, 0.0, 0.0, 0.0}};
    else if (name == "lux")
	dimensions = {{-2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0}};
    else if (name == "gray" || name == "sievert")
	dimensions = {{2.0, 0.0, -2.0, 0.0, 0.0, 0.0, 0.0}};
    else return false;
    return true;
}

bool unit_dimensions_impl(STEPWrapper &wrapper, SDAI_Select *unit,
    std::array<double, 7> &dimensions, std::set<int64_t> &active,
    size_t depth);

bool
named_unit_dimensions_impl(STEPWrapper &wrapper, STEPentity *unit,
    std::array<double, 7> &dimensions, std::set<int64_t> &active,
    size_t depth)
{
    dimensions = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    if (!unit || depth > 16) return false;
    const int64_t id = unit->STEPfile_id;
    if (id > 0 && !active.insert(id).second) return false;
    const auto finish = [&active, id](bool result) {
	if (id > 0) active.erase(id);
	return result;
    };

    if (si_unit_dimensions(wrapper, unit, dimensions))
	return finish(true);

    /* CONVERSION_BASED_UNIT inherits NAMED_UNIT, but dimensions is a derived
     * EXPRESS attribute and is encoded as '*' in Part 21.  Some generated
     * bindings nevertheless return a non-null default object from
     * dimensions_(); reading it yields uninitialized values.  Derive the
     * dimensions from conversion_factor.unit_component as EXPRESS specifies. */
    SDAI_Application_instance *conversion = unit_component(wrapper, unit,
	"Conversion_Based_Unit");
    if (conversion) {
	SDAI_Application_instance *factor = wrapper.getEntityAttribute(conversion,
	    "conversion_factor");
	SDAI_Select *factor_unit = factor ?
	    wrapper.getSelectAttribute(factor, "unit_component") : NULL;
	return finish(unit_dimensions_impl(wrapper, factor_unit, dimensions,
	    active, depth + 1));
    }

    STEPentity *source = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(unit, "dimensions"));
    if (!source) return finish(false);
    dimensions = {{wrapper.getRealAttribute(source, "length_exponent"),
	wrapper.getRealAttribute(source, "mass_exponent"),
	wrapper.getRealAttribute(source, "time_exponent"),
	wrapper.getRealAttribute(source, "electric_current_exponent"),
	wrapper.getRealAttribute(source, "thermodynamic_temperature_exponent"),
	wrapper.getRealAttribute(source, "amount_of_substance_exponent"),
	wrapper.getRealAttribute(source, "luminous_intensity_exponent")}};
    for (double exponent : dimensions)
	if (!std::isfinite(exponent)) return finish(false);
    return finish(true);
}

bool
unit_dimensions_impl(STEPWrapper &wrapper, SDAI_Select *unit,
    std::array<double, 7> &dimensions, std::set<int64_t> &active,
    size_t depth)
{
    dimensions = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    STEPentity *selected = dynamic_cast<STEPentity *>(
	brlcad::step::SelectedEntity(unit));
    if (!selected || depth > 16) return false;
    if (wrapper.IsSchemaEntity(selected, "NAMED_UNIT"))
	return named_unit_dimensions_impl(wrapper, selected, dimensions, active, depth);
    if (!wrapper.IsSchemaEntity(selected, "DERIVED_UNIT")) return false;
    const int64_t id = selected->STEPfile_id;
    if (id > 0 && !active.insert(id).second) return false;
    const std::vector<SDAI_Application_instance *> elements =
	brlcad::step::Entities(selected, "elements");
    bool found = false;
    for (std::vector<SDAI_Application_instance *>::const_iterator i = elements.begin();
	 i != elements.end(); ++i) {
	STEPentity *element = dynamic_cast<STEPentity *>(*i);
	STEPentity *named = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(element, "unit"));
	const double exponent = wrapper.getRealAttribute(element, "exponent");
	std::array<double, 7> component;
	if (!element || !named || !std::isfinite(exponent) ||
		!named_unit_dimensions_impl(wrapper, named, component,
		    active, depth + 1)) {
	    if (id > 0) active.erase(id);
	    return false;
	}
	for (size_t dimension = 0; dimension < dimensions.size(); ++dimension)
	    dimensions[dimension] += component[dimension] * exponent;
	found = true;
    }
    if (id > 0) active.erase(id);
    return found;
}

bool
named_unit_dimensions(STEPWrapper &wrapper, STEPentity *unit,
    std::array<double, 7> &dimensions)
{
    std::set<int64_t> active;
    return named_unit_dimensions_impl(wrapper, unit, dimensions, active, 0);
}

bool
unit_dimensions(STEPWrapper &wrapper, SDAI_Select *unit,
    std::array<double, 7> &dimensions)
{
    std::set<int64_t> active;
    return unit_dimensions_impl(wrapper, unit, dimensions, active, 0);
}

bool extract_unit_structure(STEPWrapper &wrapper, SDAI_Select *selected,
    UnitStructure &unit, std::set<int64_t> &active, size_t depth,
    std::string &error);

bool
extract_named_unit_structure(STEPWrapper &wrapper, STEPentity *named,
    UnitStructure &unit, std::set<int64_t> &active, size_t depth,
    std::string &error)
{
    if (!named) {
	error = "missing named unit";
	return false;
    }
    if (depth > 16) {
	error = "unit graph exceeds 16 nested conversion levels";
	return false;
    }
    const int64_t id = named->STEPfile_id;
    if (id > 0 && !active.insert(id).second) {
	error = "cyclic conversion-based unit graph";
	return false;
    }
    const auto finish = [&active, id](bool result) {
	if (id > 0) active.erase(id);
	return result;
    };

    unit = UnitStructure();
    unit.entity_id = id;
    unit.subtype = named_unit_subtype(wrapper, named);
    unit.has_dimensions = named_unit_dimensions(wrapper, named,
	unit.dimension_exponents);

    SDAI_Application_instance *si = unit_component(wrapper, named, "Si_Unit");
    if (si) {
	unit.kind = "si";
	unit.prefix = lower_metadata_ascii(wrapper.getEnumAttributeName(si, "prefix"));
	if (unit.prefix == "unset") unit.prefix.clear();
	unit.name = lower_metadata_ascii(wrapper.getEnumAttributeName(si, "name"));
	if (unit.name.empty() || unit.name == "unset") {
	    error = "SI unit has no name";
	    return finish(false);
	}
	return finish(true);
    }

    SDAI_Application_instance *conversion = unit_component(wrapper, named,
	"Conversion_Based_Unit");
    if (conversion) {
	unit.kind = "conversion";
	unit.name = brlcad::step::decode_string(
	    wrapper.getStringAttribute(conversion, "name"));
	SDAI_Application_instance *factor = wrapper.getEntityAttribute(conversion,
	    "conversion_factor");
	SDAI_Select *value = factor ?
	    wrapper.getSelectAttribute(factor, "value_component") : NULL;
	SDAI_Select *factor_unit = factor ?
	    wrapper.getSelectAttribute(factor, "unit_component") : NULL;
	if (unit.name.empty() || !value ||
		brlcad::step::SelectIs(value, "DESCRIPTIVE_MEASURE") || !factor_unit) {
	    error = "conversion-based unit has no usable name or typed factor";
	    return finish(false);
	}
	unit.has_conversion_value = true;
	const SDAI_Real *real_value = brlcad::step::SelectedReal(value);
	if (!real_value) {
	    error = "conversion-based unit factor has no real value";
	    return finish(false);
	}
	unit.conversion_value = *real_value;
	unit.conversion_value_type = measure_type(value);
	if (!std::isfinite(unit.conversion_value) ||
		unit.conversion_value_type.empty() ||
		unit.conversion_value_type == "measure") {
	    error = "conversion-based unit factor is unsupported or non-finite";
	    return finish(false);
	}
	UnitStructure base;
	if (!extract_unit_structure(wrapper, factor_unit, base, active,
		depth + 1, error)) return finish(false);
	unit.components.push_back(base);
	return finish(true);
    }

    SDAI_Application_instance *context = unit_component(wrapper, named,
	"Context_Dependent_Unit");
    if (context) {
	unit.kind = "context";
	unit.name = brlcad::step::decode_string(
	    wrapper.getStringAttribute(context, "name"));
	if (unit.name.empty()) {
	    error = "context-dependent unit has no name";
	    return finish(false);
	}
	return finish(true);
    }

    unit.kind = "named";
    if (!unit.has_dimensions) {
	error = "plain named unit has no dimensional exponents";
	return finish(false);
    }
    return finish(true);
}

bool
extract_unit_structure(STEPWrapper &wrapper, SDAI_Select *selected,
    UnitStructure &unit, std::set<int64_t> &active, size_t depth,
    std::string &error)
{
    if (!selected) {
	error = "missing unit select";
	return false;
    }
	STEPentity *unit_entity = dynamic_cast<STEPentity *>(
	    brlcad::step::SelectedEntity(selected));
    if (unit_entity && wrapper.IsSchemaEntity(unit_entity, "NAMED_UNIT")) {
	return extract_named_unit_structure(wrapper, unit_entity, unit, active,
	    depth, error);
    }
    if (!unit_entity || !wrapper.IsSchemaEntity(unit_entity, "DERIVED_UNIT")) {
	error = "unit select is neither a named nor a derived unit";
	return false;
    }
    if (depth > 16) {
	error = "unit graph exceeds 16 nested levels";
	return false;
    }
    const int64_t id = unit_entity->STEPfile_id;
    if (id > 0 && !active.insert(id).second) {
	error = "cyclic derived-unit graph";
	return false;
    }
    unit = UnitStructure();
    unit.entity_id = id;
    unit.kind = "derived";
    unit.has_dimensions = unit_dimensions(wrapper, selected,
	unit.dimension_exponents);
    const std::vector<SDAI_Application_instance *> elements =
	brlcad::step::Entities(unit_entity, "elements");
    for (std::vector<SDAI_Application_instance *>::const_iterator i = elements.begin();
	 i != elements.end(); ++i) {
	STEPentity *element = dynamic_cast<STEPentity *>(*i);
	STEPentity *named = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(element, "unit"));
	const double exponent = wrapper.getRealAttribute(element, "exponent");
	if (!element || !named || !std::isfinite(exponent)) {
	    error = "derived unit has a malformed element";
	    if (id > 0) active.erase(id);
	    return false;
	}
	UnitStructure component;
	if (!extract_named_unit_structure(wrapper, named, component,
		active, depth + 1, error)) {
	    if (id > 0) active.erase(id);
	    return false;
	}
	unit.components.push_back(component);
	unit.exponents.push_back(exponent);
    }
    if (id > 0) active.erase(id);
    if (unit.components.empty()) {
	error = "derived unit has no elements";
	return false;
    }
    return true;
}

std::string
measure_type(SDAI_Select *value)
{
    if (!value) return std::string();
    if (brlcad::step::SelectIs(value, "VOLUME_MEASURE")) return "volume_measure";
    if (brlcad::step::SelectIs(value, "AREA_MEASURE")) return "area_measure";
    if (brlcad::step::SelectIs(value, "LENGTH_MEASURE")) return "length_measure";
    if (brlcad::step::SelectIs(value, "NON_NEGATIVE_LENGTH_MEASURE"))
	return "non_negative_length_measure";
    if (brlcad::step::SelectIs(value, "POSITIVE_LENGTH_MEASURE"))
	return "positive_length_measure";
    if (brlcad::step::SelectIs(value, "MASS_MEASURE")) return "mass_measure";
    if (brlcad::step::SelectIs(value, "TIME_MEASURE")) return "time_measure";
    if (brlcad::step::SelectIs(value, "ELECTRIC_CURRENT_MEASURE"))
	return "electric_current_measure";
    if (brlcad::step::SelectIs(value, "THERMODYNAMIC_TEMPERATURE_MEASURE"))
	return "thermodynamic_temperature_measure";
    if (brlcad::step::SelectIs(value, "CELSIUS_TEMPERATURE_MEASURE"))
	return "celsius_temperature_measure";
    if (brlcad::step::SelectIs(value, "AMOUNT_OF_SUBSTANCE_MEASURE"))
	return "amount_of_substance_measure";
    if (brlcad::step::SelectIs(value, "LUMINOUS_INTENSITY_MEASURE"))
	return "luminous_intensity_measure";
    if (brlcad::step::SelectIs(value, "PLANE_ANGLE_MEASURE")) return "plane_angle_measure";
    if (brlcad::step::SelectIs(value, "POSITIVE_PLANE_ANGLE_MEASURE"))
	return "positive_plane_angle_measure";
    if (brlcad::step::SelectIs(value, "SOLID_ANGLE_MEASURE")) return "solid_angle_measure";
    if (brlcad::step::SelectIs(value, "POSITIVE_RATIO_MEASURE")) return "positive_ratio_measure";
    if (brlcad::step::SelectIs(value, "RATIO_MEASURE")) return "ratio_measure";
    if (brlcad::step::SelectIs(value, "COUNT_MEASURE")) return "count_measure";
    if (brlcad::step::SelectIs(value, "PARAMETER_VALUE")) return "parameter_value";
    if (brlcad::step::SelectIs(value, "CONTEXT_DEPENDENT_MEASURE"))
	return "context_dependent_measure";
    if (brlcad::step::SelectIs(value, "NUMERIC_MEASURE")) return "numeric_measure";
    return "measure";
}

bool
unit_has_only_dimension(const UnitStructure &unit, size_t dimension)
{
    if (!unit.has_dimensions || dimension >= unit.dimension_exponents.size())
	return false;
    for (size_t i = 0; i < unit.dimension_exponents.size(); ++i) {
	const double expected = i == dimension ? 1.0 : 0.0;
	if (std::fabs(unit.dimension_exponents[i] - expected) > 1.0e-12)
	    return false;
    }
    return true;
}

std::string
unit_structure_name(const UnitStructure &unit)
{
    if (unit.kind == "derived") {
	if (unit.components.size() != unit.exponents.size())
	    return std::string();
	std::string result;
	for (size_t i = 0; i < unit.components.size(); ++i) {
	    const std::string component = unit_structure_name(unit.components[i]);
	    if (component.empty())
		return std::string();
	    if (!result.empty())
		result += '*';
	    result += component + '^' + exponent_string(unit.exponents[i]);
	}
	return result;
    }
    if (!unit.name.empty()) {
	if (unit.kind == "si" && !unit.prefix.empty())
	    return unit.prefix + unit.name;
	return unit.name;
    }
    return unit.subtype;
}

bool
repair_explicit_density_property(STEPWrapper &wrapper,
    MetadataProperty &property)
{
    const brlcad::step::ImportOptions &options = wrapper.ImportOptions();
    if (options.exact || options.strict ||
	    options.repair != brlcad::step::RepairMode::Safe ||
	    property.values.size() != 1 || !std::isfinite(property.values[0]) ||
	    !(property.values[0] > 0.0) || !property.has_dimensions ||
	    !property.unit_structure.has_dimensions ||
	    property.unit_structure.kind != "derived" ||
	    property.unit_structure.components.size() != 2 ||
	    property.unit_structure.exponents.size() != 2)
	return false;

    const std::string label = lower_metadata_ascii(property.category + ' ' +
	property.description + ' ' + property.name);
    if (label.find("density") == std::string::npos)
	return false;
    const bool ratio_measure = property.value_type == "ratio_measure" ||
	property.value_type == "positive_ratio_measure";
    if (!ratio_measure && property.value_type != "numeric_measure")
	return false;

    int length_index = -1;
    int mass_index = -1;
    for (size_t i = 0; i < property.unit_structure.components.size(); ++i) {
	const UnitStructure &component = property.unit_structure.components[i];
	if (unit_has_only_dimension(component, 0)) {
	    if (length_index >= 0)
		return false;
	    length_index = static_cast<int>(i);
	} else if (unit_has_only_dimension(component, 1)) {
	    if (mass_index >= 0)
		return false;
	    mass_index = static_cast<int>(i);
	} else {
	    return false;
	}
    }
    if (length_index < 0 || mass_index < 0 ||
	    std::fabs(property.unit_structure.exponents[mass_index] - 1.0) >
		1.0e-12)
	return false;
    const double length_exponent =
	property.unit_structure.exponents[length_index];
    if (std::fabs(std::fabs(length_exponent) - 3.0) > 1.0e-12)
	return false;

    std::array<double, 7> source_dimensions = {{length_exponent, 1.0, 0.0,
	0.0, 0.0, 0.0, 0.0}};
    if (!brlcad::step::MeasureDimensionsEqual(property.dimension_exponents,
	    source_dimensions))
	return false;

    const bool corrected_exponent = length_exponent > 0.0;
    if (corrected_exponent) {
	UnitStructure corrected_structure = property.unit_structure;
	corrected_structure.exponents[length_index] = -3.0;
	corrected_structure.dimension_exponents[0] = -3.0;
	const std::string corrected_units = unit_structure_name(corrected_structure);
	if (corrected_units.empty())
	    return false;
	property.unit_structure = corrected_structure;
	property.dimension_exponents[0] = -3.0;
	property.units = corrected_units;
    }
    if (ratio_measure)
	property.value_type = "numeric_measure";

    if (corrected_exponent) {
	wrapper.RecordRepair(property.item_entity_id,
	    "MEASURE_REPRESENTATION_ITEM", "unit_component",
	    "corrected an explicit density unit from mass*length^3 to "
	    "mass*length^-3 without changing its numeric value");
    }
    if (ratio_measure) {
	wrapper.RecordRepair(property.item_entity_id,
	    "MEASURE_REPRESENTATION_ITEM", "value_component",
	    "normalized an explicit dimensioned density from a ratio measure "
	    "to NUMERIC_MEASURE");
    }
    return corrected_exponent || ratio_measure;
}

bool
extract_property_item(STEPWrapper &wrapper, SDAI_Application_instance *item,
    MetadataProperty &property)
{
    if (!item)
	return false;
    property.item_entity_id = item->STEPfile_id;

    if (wrapper.IsSchemaEntity(item, "MEASURE_REPRESENTATION_ITEM")) {
	/* MEASURE_REPRESENTATION_ITEM has MEASURE_WITH_UNIT as a second
	 * supertype.  STEPcode stores those parsed attributes on the head
	 * instance; the generated convenience members on the primary class
	 * remain default initialized. */
	SDAI_Select *value = wrapper.getSelectAttribute(item, "value_component");
	SDAI_Select *unit = wrapper.getSelectAttribute(item, "unit_component");
	const SDAI_Real *real_value = brlcad::step::SelectedReal(value);
	if (!value || !real_value ||
		brlcad::step::SelectIs(value, "DESCRIPTIVE_MEASURE"))
	    return false;
	property.name = brlcad::step::decode_string(wrapper.getStringAttribute(item, "name"));
	property.value_type = measure_type(value);
	property.values.push_back(*real_value);
	property.units = unit_name(wrapper, unit);
	property.has_dimensions = unit_dimensions(wrapper, unit,
	    property.dimension_exponents);
	std::set<int64_t> active_units;
	std::string unit_error;
	if (!extract_unit_structure(wrapper, unit, property.unit_structure,
		active_units, 0, unit_error)) {
	    wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		item->STEPfile_id, "MEASURE_REPRESENTATION_ITEM",
		"unit_component", "could not retain structured unit graph: " +
		unit_error);
	}
	repair_explicit_density_property(wrapper, property);
	std::array<double, 7> required_dimensions;
	if (property.has_dimensions &&
	    brlcad::step::ConstrainedMeasureDimensions(property.value_type,
		required_dimensions) &&
	    !brlcad::step::MeasureDimensionsEqual(property.dimension_exponents,
		required_dimensions)) {
	    property.valid = false;
	    property.error = "value type '" + property.value_type +
		"' is incompatible with unit dimensions '" + property.units + "'";
	    ++wrapper.Statistics().properties_invalid;
	    wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		item->STEPfile_id, "MEASURE_REPRESENTATION_ITEM",
		"unit_component", property.error);
	}
	return true;
    }

    if (wrapper.IsSchemaEntity(item, "DESCRIPTIVE_REPRESENTATION_ITEM")) {
	property.name = brlcad::step::decode_string(wrapper.getStringAttribute(item, "name"));
	property.value_type = "descriptive";
	property.text = brlcad::step::decode_string(
	    wrapper.getStringAttribute(item, "description"));
	return !property.text.empty();
    }

    STEPentity *point = dynamic_cast<STEPentity *>(item);
    STEPaggregate *coordinates = point && wrapper.IsSchemaEntity(point,
	"CARTESIAN_POINT") ? brlcad::step::Aggregate(point, "coordinates") : NULL;
    RealNode *node = coordinates ? static_cast<RealNode *>(coordinates->GetHead()) : NULL;
    while (node) {
	property.values.push_back(node->value);
	node = static_cast<RealNode *>(node->NextNode());
    }
    if (!property.values.empty()) {
	property.name = brlcad::step::decode_string(wrapper.getStringAttribute(item, "name"));
	property.value_type = "cartesian_point";
	return true;
    }
    return false;
}

typedef std::map<int64_t, std::vector<MetadataProperty> > PropertiesByDefinition;
typedef std::map<int64_t, int64_t> DefinitionProducts;

void
extract_properties(STEPWrapper &wrapper, PropertiesByDefinition &by_definition,
    DefinitionProducts &definition_products)
{
    wrapper.SetInstanceTypes({"PROPERTY_DEFINITION_REPRESENTATION"});
    for (int i = 0; i < wrapper.InstanceCount(); ++i) {
	SDAI_Application_instance *instance = wrapper.InstanceAt(i);
	if (!instance || instance->STEPfile_id <= 0 ||
	    !wrapper.IsSchemaEntity(instance, "PROPERTY_DEFINITION_REPRESENTATION"))
	    continue;

	STEPentity *link = dynamic_cast<STEPentity *>(instance);
	SDAI_Select *represented = wrapper.getSelectAttribute(link, "definition");
	STEPentity *definition = dynamic_cast<STEPentity *>(
	    brlcad::step::SelectedEntity(represented));
	if (!definition || !wrapper.IsSchemaEntity(definition, "PROPERTY_DEFINITION"))
	    continue;
	STEPentity *product_definition_entity = product_definition(wrapper,
	    wrapper.getSelectAttribute(definition, "definition"));
	STEPentity *representation = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(link, "used_representation"));
	if (!definition || !product_definition_entity || !representation)
	    continue;
	definition_products[product_definition_entity->STEPfile_id] =
	    product_id(product_definition_entity);

	MetadataProperty base;
	base.entity_id = definition->STEPfile_id;
	base.representation_id = representation->STEPfile_id;
	base.category = brlcad::step::decode_string(wrapper.getStringAttribute(definition, "name"));
	base.description = brlcad::step::decode_string(wrapper.getStringAttribute(definition, "description"));
	const std::string representation_name = brlcad::step::decode_string(
	    wrapper.getStringAttribute(representation, "name"));

	const std::vector<SDAI_Application_instance *> items =
	    brlcad::step::Entities(representation, "items");
	for (std::vector<SDAI_Application_instance *>::const_iterator item =
		items.begin(); item != items.end(); ++item) {
	    MetadataProperty property = base;
	    if (extract_property_item(wrapper, *item, property)) {
		if (property.name.empty()) property.name = representation_name;
		by_definition[product_definition_entity->STEPfile_id].push_back(property);
		++wrapper.Statistics().properties_extracted;
		const std::string category = lower_metadata_ascii(property.category);
		if (category.find("material") == std::string::npos) {
		    const int64_t id = product_id(product_definition_entity);
		    std::map<int64_t, brlcad::step::Product>::iterator product =
			wrapper.Document().products.find(id);
		    if (product != wrapper.Document().products.end())
			product->second.validation_properties.push_back(property);
		}
	    }
	}
    }
    wrapper.ResetInstanceTypes();
}

std::set<int64_t>
extract_materials(STEPWrapper &wrapper, const PropertiesByDefinition &by_definition)
{
    std::set<int64_t> source_definitions;
    wrapper.SetInstanceTypes({"MAKE_FROM_USAGE_OPTION"});
    for (int i = 0; i < wrapper.InstanceCount(); ++i) {
	SDAI_Application_instance *instance = wrapper.InstanceAt(i);
	if (!instance || instance->STEPfile_id <= 0 ||
	    !wrapper.IsSchemaEntity(instance, "MAKE_FROM_USAGE_OPTION"))
	    continue;

	STEPentity *usage = dynamic_cast<STEPentity *>(instance);
	STEPentity *relating = material_relationship_definition(wrapper, usage, true);
	STEPentity *related = material_relationship_definition(wrapper, usage, false);
	STEPentity *formation = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(related, "formation"));
	STEPentity *source = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(formation, "of_product"));
	const int64_t target_id = product_id(relating);
	std::map<int64_t, brlcad::step::Product>::iterator target =
	    wrapper.Document().products.find(target_id);
	if (!usage || !related || !source || target == wrapper.Document().products.end())
	    continue;
	source_definitions.insert(related->STEPfile_id);

	Material material;
	material.usage_entity_id = usage->STEPfile_id;
	material.definition_entity_id = related->STEPfile_id;
	material.product_entity_id = source->STEPfile_id;
	material.identifier = brlcad::step::decode_string(wrapper.getStringAttribute(source, "id"));
	material.name = brlcad::step::decode_string(wrapper.getStringAttribute(source, "name"));
	material.description = brlcad::step::decode_string(wrapper.getStringAttribute(source, "description"));
	PropertiesByDefinition::const_iterator properties = by_definition.find(related->STEPfile_id);
	if (properties != by_definition.end())
	    material.properties = properties->second;
	target->second.materials.push_back(material);
	++wrapper.Statistics().materials_extracted;
    }
    wrapper.ResetInstanceTypes();
    return source_definitions;
}

void
extract_direct_materials(STEPWrapper &wrapper, const PropertiesByDefinition &by_definition,
    const DefinitionProducts &definition_products,
    const std::set<int64_t> &make_from_sources)
{
    for (PropertiesByDefinition::const_iterator entry = by_definition.begin();
	 entry != by_definition.end(); ++entry) {
	if (make_from_sources.find(entry->first) != make_from_sources.end())
	    continue;
	DefinitionProducts::const_iterator product_id_entry = definition_products.find(entry->first);
	if (product_id_entry == definition_products.end())
	    continue;
	std::map<int64_t, brlcad::step::Product>::iterator product =
	    wrapper.Document().products.find(product_id_entry->second);
	if (product == wrapper.Document().products.end())
	    continue;

	bool is_material = false;
	std::string material_name;
	std::vector<MetadataProperty> material_properties;
	for (std::vector<MetadataProperty>::const_iterator property = entry->second.begin();
	     property != entry->second.end(); ++property) {
	    const std::string category = lower_metadata_ascii(property->category);
	    if (category.find("material") == std::string::npos)
		continue;
	    is_material = true;
	    material_properties.push_back(*property);
	    const std::string label = lower_metadata_ascii(
		property->category + ' ' + property->description + ' ' + property->name);
	    if (!property->text.empty() &&
		(material_name.empty() || label.find("material name") != std::string::npos))
		material_name = property->text;
	}
	if (!is_material)
	    continue;

	Material material;
	material.definition_entity_id = entry->first;
	material.product_entity_id = product_id_entry->second;
	material.name = material_name;
	material.properties = material_properties;
	product->second.materials.push_back(material);
	++wrapper.Statistics().materials_extracted;
    }
}

} // namespace

void
ExtractSTEPMaterialMetadata(STEPWrapper &wrapper)
{
    wrapper.Statistics().materials_extracted = 0;
    wrapper.Statistics().properties_extracted = 0;
    wrapper.Statistics().properties_invalid = 0;
    for (std::map<int64_t, brlcad::step::Product>::value_type &entry : wrapper.Document().products) {
	entry.second.materials.clear();
	entry.second.validation_properties.clear();
    }

    PropertiesByDefinition properties;
    DefinitionProducts definition_products;
    extract_properties(wrapper, properties, definition_products);
    const std::set<int64_t> make_from_sources = extract_materials(wrapper, properties);
    extract_direct_materials(wrapper, properties, definition_products, make_from_sources);
}
