/*              S T E P E X P O R T M E T A D A T A . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP_Common.h"
#include "STEPConfigurationExport.h"
#include "STEPExportMetadata.h"
#include "STEPGeneratedAPI.h"
#include "STEPString.h"
#include "StepExportPlan.h"

#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
#  include "STEPMaterialExport.h"
#endif

#include "bu/log.h"
#include "raytrace.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using brlcad::step::ExportMetadataPlan;
using brlcad::step::ExportMetadataStatistics;
using brlcad::step::ExportMaterialPlan;
using brlcad::step::ExportObjectPlan;
using brlcad::step::ExportOccurrencePlan;
using brlcad::step::ExportPropertyPlan;
using brlcad::step::StepExportPlan;

static bool
set_occurrence_attribute(STEPentity *entity, const char *name,
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

static STEPentity *
metadata_mapped_product(struct directory *entry, AP203_Contents *contents)
{
    if (!entry || !contents) return NULL;
    const auto combination = contents->comb_to_step->find(entry);
    if (combination != contents->comb_to_step->end()) return combination->second;
    const auto solid = contents->solid_to_step->find(entry);
    return solid == contents->solid_to_step->end() ? NULL : solid->second;
}

static STEPentity *
mapped_presentation_item(struct directory *entry, AP203_Contents *contents)
{
    if (!entry || !contents) return NULL;
    const auto combination = contents->comb_to_step_manifold->find(entry);
    if (combination != contents->comb_to_step_manifold->end())
	return combination->second;
    const auto solid = contents->solid_to_step_manifold->find(entry);
    return solid == contents->solid_to_step_manifold->end() ? NULL : solid->second;
}

#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
static bool
set_attribute(STEPentity *entity, const char *name, const std::string &value,
    InstMgr *instances)
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

static std::string
entity_reference(STEPentity *entity)
{
    return entity ? "#" + std::to_string(entity->StepFileId()) : "$";
}

static std::string
entity_aggregate(const std::vector<STEPentity *> &entities)
{
    std::ostringstream value;
    value << '(';
    for (size_t i = 0; i < entities.size(); ++i) {
	if (i) value << ',';
	value << entity_reference(entities[i]);
    }
    value << ')';
    return value.str();
}

static STEPentity *
create_entity(AP203_Contents *contents, const char *type)
{
    if (!contents || !contents->registry || !contents->instance_list)
	return NULL;
    return contents->registry->ObjCreate(type);
}

static STEPentity *
append_entity(AP203_Contents *contents, STEPentity *entity)
{
    if (contents && entity) contents->instance_list->Append(entity, completeSE);
    return entity;
}
#endif

static void
choose_mapped_objects(const StepExportPlan &plan, AP203_Contents *contents,
    std::map<STEPentity *, const ExportObjectPlan *> &products,
    std::map<STEPentity *, const ExportObjectPlan *> &presentation)
{
    /* Primitive metadata is applied first.  A BRL-CAD wrapper combination can
     * intentionally share the child's STEP product and must win in that case. */
    for (int combinations = 0; combinations <= 1; ++combinations) {
	for (const ExportObjectPlan &object : plan.objects) {
	    if (object.combination != (combinations != 0)) continue;
	    struct directory *entry = db_lookup(contents->dbip,
		object.name.c_str(), LOOKUP_QUIET);
	    if (entry == RT_DIR_NULL) continue;
	    STEPentity *product = metadata_mapped_product(entry, contents);
	    if (product) products[product] = &object;
	    STEPentity *item = mapped_presentation_item(entry, contents);
	    if (item) presentation[item] = &object;
	}
    }
}

static void
apply_product(const ExportObjectPlan &object, STEPentity *entity,
    ExportMetadataStatistics &statistics)
{
    STEPentity *definition = entity;
    STEPentity *formation = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(definition, "formation"));
    STEPentity *product = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(formation, "of_product"));
    if (!definition || !formation || !product) return;
    const ExportMetadataPlan &metadata = object.metadata;

    const std::string name = brlcad::step::encode_string(metadata.product_name);
    const std::string identifier =
	brlcad::step::encode_string(metadata.product_identifier);
    const std::string product_description =
	brlcad::step::encode_string(metadata.product_description);
    brlcad::step::SetString(product, "name", name.c_str());
    brlcad::step::SetString(product, "id", identifier.c_str());
    brlcad::step::SetString(product, "description", product_description.c_str());

    if (!metadata.revision.empty()) {
	const std::string value = brlcad::step::encode_string(metadata.revision);
	brlcad::step::SetString(formation, "id", value.c_str());
    }
    if (!metadata.revision_description.empty()) {
	const std::string value =
	    brlcad::step::encode_string(metadata.revision_description);
	brlcad::step::SetString(formation, "description", value.c_str());
    }
    if (!metadata.definition_identifier.empty()) {
	const std::string value =
	    brlcad::step::encode_string(metadata.definition_identifier);
	brlcad::step::SetString(definition, "id", value.c_str());
    }
    if (!metadata.definition_description.empty()) {
	const std::string value =
	    brlcad::step::encode_string(metadata.definition_description);
	brlcad::step::SetString(definition, "description", value.c_str());
    }
    ++statistics.products_updated;
}

#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
static bool
emit_surface_style(const ExportObjectPlan &object, STEPentity *target,
    AP203_Contents *contents)
{
    const ExportMetadataPlan &metadata = object.metadata;
    if (!metadata.has_rgb) return false;

    STEPentity *colour = create_entity(contents, "COLOUR_RGB");
    if (!colour) return false;
    const std::string colour_name = brlcad::step::encode_string(
	metadata.style_name.empty() ? object.name + " colour" : metadata.style_name);
    std::ostringstream red, green, blue;
    red << std::setprecision(17) << metadata.rgb[0];
    green << std::setprecision(17) << metadata.rgb[1];
    blue << std::setprecision(17) << metadata.rgb[2];
    if (!set_attribute(colour, "name", colour_name, contents->instance_list) ||
	!set_attribute(colour, "red", red.str(), contents->instance_list) ||
	!set_attribute(colour, "green", green.str(), contents->instance_list) ||
	!set_attribute(colour, "blue", blue.str(), contents->instance_list))
	return false;
    append_entity(contents, colour);

    STEPentity *transparent = NULL;
    if (metadata.has_transparency) {
	transparent = create_entity(contents, "SURFACE_STYLE_TRANSPARENT");
	std::ostringstream value;
	value << std::setprecision(17) << metadata.transparency;
	if (!transparent || !set_attribute(transparent, "transparency", value.str(),
		contents->instance_list)) return false;
	append_entity(contents, transparent);
    }

    STEPentity *rendering = create_entity(contents,
	transparent ? "SURFACE_STYLE_RENDERING_WITH_PROPERTIES" :
	"SURFACE_STYLE_RENDERING");
    if (!rendering ||
	!set_attribute(rendering, "rendering_method", ".NORMAL_SHADING.",
	    contents->instance_list) ||
	!set_attribute(rendering, "surface_colour", entity_reference(colour),
	    contents->instance_list)) return false;
    if (transparent && !set_attribute(rendering, "properties",
	    entity_aggregate({transparent}), contents->instance_list)) return false;
    append_entity(contents, rendering);

    STEPentity *side_style = create_entity(contents, "SURFACE_SIDE_STYLE");
    const std::string style_name = brlcad::step::encode_string(
	metadata.style_name.empty() ? object.name + " surface style" :
	metadata.style_name);
    if (!side_style ||
	!set_attribute(side_style, "name", style_name, contents->instance_list) ||
	!set_attribute(side_style, "styles", entity_aggregate({rendering}),
	    contents->instance_list)) return false;
    append_entity(contents, side_style);

    STEPentity *usage = create_entity(contents, "SURFACE_STYLE_USAGE");
    if (!usage ||
	!set_attribute(usage, "side", ".BOTH.", contents->instance_list) ||
	!set_attribute(usage, "style", entity_reference(side_style),
	    contents->instance_list)) return false;
    append_entity(contents, usage);

    STEPentity *assignment = create_entity(contents,
	"PRESENTATION_STYLE_ASSIGNMENT");
    if (!assignment || !set_attribute(assignment, "styles",
	    entity_aggregate({usage}), contents->instance_list)) return false;
    append_entity(contents, assignment);

    STEPentity *styled = create_entity(contents, "STYLED_ITEM");
    const bool valid = styled &&
	set_attribute(styled, "name", style_name, contents->instance_list) &&
	set_attribute(styled, "styles", entity_aggregate({assignment}),
	    contents->instance_list) &&
	set_attribute(styled, "item", entity_reference(target),
	    contents->instance_list);
    if (valid) append_entity(contents, styled);
    return valid;
}

static bool
emit_layer(const std::string &name, const std::vector<STEPentity *> &targets,
    AP203_Contents *contents)
{
    STEPentity *layer = create_entity(contents, "PRESENTATION_LAYER_ASSIGNMENT");
    const bool valid = layer &&
	set_attribute(layer, "name", brlcad::step::encode_string(name),
	    contents->instance_list) &&
	set_attribute(layer, "description", "''", contents->instance_list) &&
	set_attribute(layer, "assigned_items", entity_aggregate(targets),
	    contents->instance_list);
    if (valid) append_entity(contents, layer);
    return valid;
}
#endif

} // namespace

bool
brlcad::step::ApplySTEPExportMetadata(StepExportPlan &plan,
    AP203_Contents *contents, std::vector<std::string> &diagnostics,
    ExportMetadataStatistics &statistics)
{
    if (!contents || !contents->dbip) return false;
    statistics.opaque_global_attributes_seen = plan.global_attributes.size();
    statistics.configuration_records_seen = plan.configuration_records.size();
    const ConfigurationExportStatistics configuration =
	EmitSTEPConfigurationRecords(plan, contents, diagnostics);
    statistics.configuration_records_emitted = configuration.emitted;
    statistics.configuration_records_omitted = configuration.omitted;

    std::map<STEPentity *, const ExportObjectPlan *> products;
    std::map<STEPentity *, const ExportObjectPlan *> presentation;
    choose_mapped_objects(plan, contents, products, presentation);
    for (const auto &entry : products) {
	const ExportObjectPlan &object = *entry.second;
	apply_product(object, entry.first, statistics);
	if (!object.metadata.material_requested &&
	    !object.metadata.property_requested) continue;
#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
	for (const ExportMaterialPlan &material : object.metadata.materials) {
	    const MaterialExportResult result = EmitSTEPMaterialAssignment(
		material, entry.first, contents);
	    statistics.material_properties_emitted += result.properties_emitted;
	    statistics.material_properties_omitted += result.properties_omitted;
	    if (result.material_emitted) {
		++statistics.materials_emitted;
	    } else {
		++statistics.materials_omitted;
	    }
	    for (const std::string &message : result.diagnostics)
		diagnostics.push_back("could not fully emit STEP material " +
		    std::to_string(material.ordinal) + " for '" + object.name +
		    "': " + message);
	}
	for (const ExportPropertyPlan &property : object.metadata.properties) {
	    std::string error;
	    if (EmitSTEPProductProperty(property, entry.first, contents, error)) {
		++statistics.product_properties_emitted;
	    } else {
		++statistics.product_properties_omitted;
		diagnostics.push_back("could not emit STEP product property " +
		    std::to_string(property.ordinal) + " for '" + object.name +
		    "': " + error);
	    }
	}
#else
	statistics.materials_omitted += object.metadata.materials.size();
	for (const ExportMaterialPlan &material : object.metadata.materials)
	    statistics.material_properties_omitted += material.properties.size();
	statistics.product_properties_omitted += object.metadata.properties.size();
	if (!object.metadata.materials.empty())
	    diagnostics.push_back("the selected schema has no enabled material mapping for '" +
		object.name + "'");
	if (!object.metadata.properties.empty())
	    diagnostics.push_back("the selected schema has no enabled product-property mapping for '" +
		object.name + "'");
#endif
    }

    for (const ExportOccurrencePlan &occurrence : plan.occurrences) {
	if (!occurrence.metadata_requested) continue;
	if (!occurrence.metadata_valid || occurrence.parent >= plan.objects.size()) {
	    ++statistics.occurrences_omitted;
	    diagnostics.push_back("stale occurrence metadata was not emitted: " +
		occurrence.metadata_error);
	    continue;
	}
	struct directory *parent = db_lookup(contents->dbip,
	    plan.objects[occurrence.parent].name.c_str(), LOOKUP_QUIET);
	if (parent == RT_DIR_NULL || !contents->occurrence_to_step) {
	    ++statistics.occurrences_omitted;
	    diagnostics.push_back("could not locate the parent for occurrence " +
		std::to_string(occurrence.ordinal) + " of '" +
		plan.objects[occurrence.parent].name + "'");
	    continue;
	}
	const auto mapped = contents->occurrence_to_step->find(
	    std::make_pair(parent, occurrence.ordinal));
	if (mapped == contents->occurrence_to_step->end() ||
		!mapped->second) {
	    ++statistics.occurrences_omitted;
	    diagnostics.push_back("could not locate emitted occurrence " +
		std::to_string(occurrence.ordinal) + " of '" +
		plan.objects[occurrence.parent].name + "'");
	    continue;
	}
	STEPentity *usage = mapped->second;
	bool valid = true;
	const auto set_text = [&valid, usage, contents](const char *name,
		const std::string &value) {
	    if (value.empty()) return;
	    valid = set_occurrence_attribute(usage, name, encode_string(value),
		contents->instance_list) && valid;
	};
	set_text("id", occurrence.identifier);
	set_text("name", occurrence.name);
	set_text("description", occurrence.description);
	set_text("reference_designator", occurrence.reference_designator);
	if (valid) {
	    ++statistics.occurrences_updated;
	} else {
	    ++statistics.occurrences_omitted;
	    diagnostics.push_back("could not emit occurrence metadata for occurrence " +
		std::to_string(occurrence.ordinal) + " of '" +
		plan.objects[occurrence.parent].name + "'");
	}
    }

#if defined(AP203e2) || defined(AP214e3) || defined(AP242)
    std::map<std::string, std::vector<STEPentity *> > layers;
    for (const auto &entry : presentation) {
	const ExportObjectPlan &object = *entry.second;
	if (object.metadata.presentation_malformed) {
	    ++statistics.presentation_omitted;
	    diagnostics.push_back("malformed presentation metadata on '" +
		object.name + "': " + object.metadata.presentation_error);
	} else if (object.metadata.has_rgb || object.metadata.has_transparency) {
	    if (!object.metadata.has_rgb) {
		++statistics.presentation_omitted;
		diagnostics.push_back("presentation transparency on '" + object.name +
		    "' has no source colour and was not emitted");
	    } else if (!emit_surface_style(object, entry.first, contents)) {
		++statistics.presentation_omitted;
		diagnostics.push_back("could not emit a schema-valid presentation style for '" +
		    object.name + "'");
	    } else {
		++statistics.styled_items_emitted;
	    }
	}
	for (const std::string &layer : object.metadata.layers)
	    layers[layer].push_back(entry.first);
    }
    for (auto &entry : layers) {
	std::sort(entry.second.begin(), entry.second.end(),
	    [](const STEPentity *left, const STEPentity *right) {
		return left->StepFileId() < right->StepFileId();
	    });
	entry.second.erase(std::unique(entry.second.begin(), entry.second.end()),
	    entry.second.end());
	if (emit_layer(entry.first, entry.second, contents)) {
	    ++statistics.layers_emitted;
	} else {
	    ++statistics.presentation_omitted;
	    diagnostics.push_back("could not emit presentation layer '" +
		entry.first + "'");
	}
    }
#else
    for (const auto &entry : presentation) {
	const ExportMetadataPlan &metadata = entry.second->metadata;
	if (!metadata.presentation_requested)
	    continue;
	++statistics.presentation_omitted;
	diagnostics.push_back("AP203 edition 1 has no enabled presentation mapping for '" +
	    entry.second->name + "'");
    }
#endif

    return true;
}
