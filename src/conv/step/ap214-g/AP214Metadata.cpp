/*              A P 2 1 4 M E T A D A T A . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP214Metadata.h"
#include "STEPDocument.h"
#include "STEPString.h"
#include "STEPWrapper.h"

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

std::string
lower_metadata_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
	return static_cast<char>(std::tolower(c));
    });
    return value;
}

SdaiProduct_definition *
product_definition(SdaiCharacterized_definition *definition)
{
    if (!definition)
	return NULL;

    if (definition->IsCharacterized_product_definition() == LTrue) {
	SdaiCharacterized_product_definition *product_select = *definition;
	if (product_select && product_select->IsProduct_definition() == LTrue) {
	    SdaiProduct_definition *product = *product_select;
	    return product;
	}
	if (product_select && product_select->IsProduct_definition_relationship() == LTrue) {
	    SdaiProduct_definition_relationship *relationship = *product_select;
	    return relationship ? relationship->relating_product_definition_() : NULL;
	}
    }

    if (definition->IsShape_definition() == LTrue) {
	SdaiShape_definition *shape_select = *definition;
	if (shape_select && shape_select->IsProduct_definition_shape() == LTrue) {
	    SdaiProduct_definition_shape *shape = *shape_select;
	    return shape ? product_definition(shape->definition_()) : NULL;
	}
	if (shape_select && shape_select->IsShape_aspect() == LTrue) {
	    SdaiShape_aspect *aspect = *shape_select;
	    SdaiProduct_definition_shape *shape = aspect ? aspect->of_shape_() : NULL;
	    return shape ? product_definition(shape->definition_()) : NULL;
	}
    }

    return NULL;
}

int64_t
product_id(SdaiProduct_definition *definition)
{
    SdaiProduct_definition_formation *formation = definition ? definition->formation_() : NULL;
    SdaiProduct *product = formation ? formation->of_product_() : NULL;
    return product ? product->STEPfile_id : 0;
}

SDAI_Application_instance *
unit_component(STEPWrapper &wrapper, SDAI_Application_instance *unit,
    EntityDescriptor *descriptor, const char *name)
{
    if (!unit || !descriptor)
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
    static const char *prefixes[] = {
	"exa", "peta", "tera", "giga", "mega", "kilo", "hecto", "deca",
	"deci", "centi", "milli", "micro", "nano", "pico", "femto", "atto"
    };
    static const char *names[] = {
	"metre", "gram", "second", "ampere", "kelvin", "mole", "candela",
	"radian", "steradian", "hertz", "newton", "pascal", "joule", "watt",
	"coulomb", "volt", "farad", "ohm", "siemens", "weber", "tesla", "henry",
	"degree_celsius", "lumen", "lux", "becquerel", "gray", "sievert"
    };

    SDAI_Application_instance *si = unit_component(
	wrapper, unit, SCHEMA_NAMESPACE::e_si_unit, "Si_Unit");
    if (!si)
	return std::string();
    const int prefix = wrapper.getEnumAttribute(si, "prefix");
    const int name = wrapper.getEnumAttribute(si, "name");
    std::string result;
    if (prefix >= 0 && static_cast<size_t>(prefix) < sizeof(prefixes) / sizeof(prefixes[0]))
	result = prefixes[prefix];
    if (name >= 0 && static_cast<size_t>(name) < sizeof(names) / sizeof(names[0]))
	result += names[name];
    return result;
}

std::string
named_unit_name(STEPWrapper &wrapper, SdaiNamed_unit *unit)
{
    if (!unit)
	return std::string();

    SDAI_Application_instance *conversion = unit_component(
	wrapper, unit, SCHEMA_NAMESPACE::e_conversion_based_unit, "Conversion_Based_Unit");
    if (conversion) {
	const std::string name = conversion ? brlcad::step::decode_string(
	    wrapper.getStringAttribute(conversion, "name")) : std::string();
	if (!name.empty())
	    return name;
    }
    const std::string si_name = si_unit_name(wrapper, unit);
    if (!si_name.empty()) return si_name;

    if (unit->IsA(SCHEMA_NAMESPACE::e_length_unit)) return "length_unit";
    if (unit->IsA(SCHEMA_NAMESPACE::e_mass_unit)) return "mass_unit";
    if (unit->IsA(SCHEMA_NAMESPACE::e_area_unit)) return "area_unit";
    if (unit->IsA(SCHEMA_NAMESPACE::e_volume_unit)) return "volume_unit";
    if (unit->IsA(SCHEMA_NAMESPACE::e_plane_angle_unit)) return "plane_angle_unit";
    if (unit->IsA(SCHEMA_NAMESPACE::e_solid_angle_unit)) return "solid_angle_unit";

    return std::string("unit_step") + std::to_string(unit->STEPfile_id);
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
unit_name(STEPWrapper &wrapper, SdaiUnit *unit)
{
    if (!unit)
	return std::string();
    if (unit->IsNamed_unit() == LTrue) {
	SdaiNamed_unit *named = *unit;
	return named_unit_name(wrapper, named);
    }
    if (unit->IsDerived_unit() != LTrue)
	return std::string();

    SdaiDerived_unit *derived = *unit;
    EntityAggregate *elements = derived ? derived->elements_() : NULL;
    EntityNode *node = elements ? static_cast<EntityNode *>(elements->GetHead()) : NULL;
    std::string result;
    while (node) {
	SdaiDerived_unit_element *element = dynamic_cast<SdaiDerived_unit_element *>(node->node);
	if (element) {
	    if (!result.empty()) result += '*';
	    result += named_unit_name(wrapper, element->unit_());
	    result += '^';
	    result += exponent_string(element->exponent_());
	}
	node = static_cast<EntityNode *>(node->NextNode());
    }
    return result;
}

std::string
measure_type(SdaiMeasure_value *value)
{
    if (!value) return std::string();
    if (value->IsVolume_measure() == LTrue) return "volume_measure";
    if (value->IsArea_measure() == LTrue) return "area_measure";
    if (value->IsLength_measure() == LTrue) return "length_measure";
    if (value->IsMass_measure() == LTrue) return "mass_measure";
    if (value->IsPositive_ratio_measure() == LTrue) return "positive_ratio_measure";
    if (value->IsRatio_measure() == LTrue) return "ratio_measure";
    if (value->IsCount_measure() == LTrue) return "count_measure";
    if (value->IsNumeric_measure() == LTrue) return "numeric_measure";
    return "measure";
}

bool
extract_property_item(STEPWrapper &wrapper, SDAI_Application_instance *item,
    MetadataProperty &property)
{
    if (!item)
	return false;
    property.item_entity_id = item->STEPfile_id;

    SdaiMeasure_representation_item *measure =
	dynamic_cast<SdaiMeasure_representation_item *>(item);
    if (measure) {
	/* MEASURE_REPRESENTATION_ITEM has MEASURE_WITH_UNIT as a second
	 * supertype.  STEPcode stores those parsed attributes on the head
	 * instance; the generated convenience members on the primary class
	 * remain default initialized. */
	SdaiMeasure_value *value = dynamic_cast<SdaiMeasure_value *>(
	    wrapper.getSelectAttribute(item, "value_component"));
	SdaiUnit *unit = dynamic_cast<SdaiUnit *>(
	    wrapper.getSelectAttribute(item, "unit_component"));
	if (!value || value->IsDescriptive_measure() == LTrue)
	    return false;
	property.name = brlcad::step::decode_string(wrapper.getStringAttribute(item, "name"));
	property.value_type = measure_type(value);
	property.values.push_back(static_cast<SDAI_Real>(*value));
	property.units = unit_name(wrapper, unit);
	return true;
    }

    SdaiDescriptive_representation_item *descriptive =
	dynamic_cast<SdaiDescriptive_representation_item *>(item);
    if (descriptive) {
	property.name = brlcad::step::decode_string(wrapper.getStringAttribute(item, "name"));
	property.value_type = "descriptive";
	property.text = brlcad::step::decode_string(
	    wrapper.getStringAttribute(item, "description"));
	return !property.text.empty();
    }

    SdaiCartesian_point *point = dynamic_cast<SdaiCartesian_point *>(item);
    RealAggregate *coordinates = point ? point->coordinates_() : NULL;
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
	    !instance->IsA(SCHEMA_NAMESPACE::e_property_definition_representation))
	    continue;

	SdaiProperty_definition_representation *link =
	    dynamic_cast<SdaiProperty_definition_representation *>(instance);
	SdaiRepresented_definition *represented = link ? link->definition_() : NULL;
	if (!represented || represented->IsProperty_definition() != LTrue)
	    continue;
	SdaiProperty_definition *definition = *represented;
	SdaiProduct_definition *product_definition_entity = definition ?
	    product_definition(definition->definition_()) : NULL;
	SdaiRepresentation *representation = link ? link->used_representation_() : NULL;
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

	EntityAggregate *items = representation->items_();
	EntityNode *node = items ? static_cast<EntityNode *>(items->GetHead()) : NULL;
	while (node) {
	    MetadataProperty property = base;
	    if (extract_property_item(wrapper, node->node, property)) {
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
	    node = static_cast<EntityNode *>(node->NextNode());
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
	    !instance->IsA(SCHEMA_NAMESPACE::e_make_from_usage_option))
	    continue;

	SdaiMake_from_usage_option *usage = dynamic_cast<SdaiMake_from_usage_option *>(instance);
	SdaiProduct_definition *relating = usage ? usage->relating_product_definition_() : NULL;
	SdaiProduct_definition *related = usage ? usage->related_product_definition_() : NULL;
	SdaiProduct_definition_formation *formation = related ? related->formation_() : NULL;
	SdaiProduct *source = formation ? formation->of_product_() : NULL;
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
ExtractAP214Metadata(STEPWrapper &wrapper)
{
    wrapper.Statistics().materials_extracted = 0;
    wrapper.Statistics().properties_extracted = 0;
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
