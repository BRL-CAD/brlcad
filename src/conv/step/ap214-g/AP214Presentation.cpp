/*            A P 2 1 4 P R E S E N T A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP214Presentation.h"
#include "STEPString.h"
#include "STEPWrapper.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <vector>

namespace {

using brlcad::step::Layer;
using brlcad::step::Style;

void
append_id(Style &style, const SDAI_Application_instance *entity)
{
    if (!entity || entity->STEPfile_id <= 0)
	return;
    const int64_t id = entity->STEPfile_id;
    if (std::find(style.source_entity_ids.begin(), style.source_entity_ids.end(), id) ==
	style.source_entity_ids.end())
	style.source_entity_ids.push_back(id);
}

void
append_layer(Style &style, const std::string &name)
{
    if (std::find(style.layers.begin(), style.layers.end(), name) == style.layers.end())
	style.layers.push_back(name);
}

std::string
lower_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
	return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool
set_predefined_colour(const std::string &input, Style &style)
{
    const std::string name = lower_ascii(brlcad::step::decode_string(input));
    struct NamedColour { const char *name; double r; double g; double b; };
    static const NamedColour colours[] = {
	{"black", 0.0, 0.0, 0.0}, {"red", 1.0, 0.0, 0.0},
	{"green", 0.0, 1.0, 0.0}, {"blue", 0.0, 0.0, 1.0},
	{"yellow", 1.0, 1.0, 0.0}, {"magenta", 1.0, 0.0, 1.0},
	{"cyan", 0.0, 1.0, 1.0}, {"white", 1.0, 1.0, 1.0},
	{"grey", 0.5, 0.5, 0.5}, {"gray", 0.5, 0.5, 0.5}
    };
    for (const NamedColour &colour : colours) {
	if (name != colour.name)
	    continue;
	style.has_rgb = true;
	style.rgb[0] = colour.r;
	style.rgb[1] = colour.g;
	style.rgb[2] = colour.b;
	return true;
    }
    return false;
}

bool
extract_colour(STEPWrapper &wrapper, SDAI_Application_instance *colour, Style &style)
{
    if (!colour)
	return false;
    append_id(style, colour);
    if (colour->IsA(SCHEMA_NAMESPACE::e_colour_rgb)) {
	style.has_rgb = true;
	style.rgb[0] = wrapper.getRealAttribute(colour, "red");
	style.rgb[1] = wrapper.getRealAttribute(colour, "green");
	style.rgb[2] = wrapper.getRealAttribute(colour, "blue");
	return true;
    }
    if (colour->IsA(SCHEMA_NAMESPACE::e_pre_defined_colour))
	return set_predefined_colour(wrapper.getStringAttribute(colour, "name"), style);
    return false;
}

void
extract_rendering_properties(SdaiSurface_style_rendering *rendering, Style &style)
{
    SdaiSurface_style_rendering_with_properties *with_properties =
	dynamic_cast<SdaiSurface_style_rendering_with_properties *>(rendering);
    if (!with_properties || !with_properties->properties_())
	return;
    SelectNode *node = static_cast<SelectNode *>(with_properties->properties_()->GetHead());
    while (node) {
	SdaiRendering_properties_select *property =
	    dynamic_cast<SdaiRendering_properties_select *>(node->node);
	if (property && property->IsSurface_style_transparent() == LTrue) {
	    SdaiSurface_style_transparent *transparent = *property;
	    append_id(style, transparent);
	    style.has_transparency = true;
	    style.transparency = transparent->transparency_();
	    if (style.transparency < 0.0) style.transparency = 0.0;
	    if (style.transparency > 1.0) style.transparency = 1.0;
	}
	node = static_cast<SelectNode *>(node->NextNode());
    }
}

void
extract_fill_area(STEPWrapper &wrapper, SdaiFill_area_style *fill_area, Style &style)
{
    if (!fill_area || !fill_area->fill_styles_())
	return;
    append_id(style, fill_area);
    SelectNode *node = static_cast<SelectNode *>(fill_area->fill_styles_()->GetHead());
    while (node) {
	SdaiFill_style_select *fill = dynamic_cast<SdaiFill_style_select *>(node->node);
	if (fill && fill->IsFill_area_style_colour() == LTrue) {
	    SdaiFill_area_style_colour *area_colour = *fill;
	    append_id(style, area_colour);
	    extract_colour(wrapper, wrapper.getEntityAttribute(area_colour, "fill_colour"), style);
	}
	node = static_cast<SelectNode *>(node->NextNode());
    }
}

void
extract_surface_side(STEPWrapper &wrapper, SdaiSurface_side_style *side, Style &style)
{
    if (!side || !side->styles_())
	return;
    append_id(style, side);
    SelectNode *node = static_cast<SelectNode *>(side->styles_()->GetHead());
    while (node) {
	SdaiSurface_style_element_select *element =
	    dynamic_cast<SdaiSurface_style_element_select *>(node->node);
	if (element && element->IsSurface_style_fill_area() == LTrue) {
	    SdaiSurface_style_fill_area *fill = *element;
	    append_id(style, fill);
	    extract_fill_area(wrapper, fill->fill_area_(), style);
	} else if (element && element->IsSurface_style_rendering() == LTrue) {
	    SdaiSurface_style_rendering *rendering = *element;
	    append_id(style, rendering);
	    extract_colour(wrapper, wrapper.getEntityAttribute(rendering, "surface_colour"), style);
	    extract_rendering_properties(rendering, style);
	}
	node = static_cast<SelectNode *>(node->NextNode());
    }
}

void
extract_assignment(STEPWrapper &wrapper, SdaiPresentation_style_assignment *assignment, Style &style)
{
    if (!assignment || !assignment->styles_())
	return;
    append_id(style, assignment);
    SelectNode *node = static_cast<SelectNode *>(assignment->styles_()->GetHead());
    while (node) {
	SdaiPresentation_style_select *presentation =
	    dynamic_cast<SdaiPresentation_style_select *>(node->node);
	if (presentation && presentation->IsSurface_style_usage() == LTrue) {
	    SdaiSurface_style_usage *usage = *presentation;
	    append_id(style, usage);
	    SdaiSurface_side_style_select *side_select = usage->style_();
	    if (side_select && side_select->IsSurface_side_style() == LTrue) {
		SdaiSurface_side_style *side = *side_select;
		extract_surface_side(wrapper, side, style);
	    }
	} else if (presentation && presentation->IsFill_area_style() == LTrue) {
	    SdaiFill_area_style *fill_area = *presentation;
	    extract_fill_area(wrapper, fill_area, style);
	} else if (presentation && presentation->IsCurve_style() == LTrue) {
	    SdaiCurve_style *curve = *presentation;
	    append_id(style, curve);
	    extract_colour(wrapper, wrapper.getEntityAttribute(curve, "curve_colour"), style);
	} else if (presentation && presentation->IsPoint_style() == LTrue) {
	    SdaiPoint_style *point = *presentation;
	    append_id(style, point);
	    extract_colour(wrapper, wrapper.getEntityAttribute(point, "marker_colour"), style);
	}
	node = static_cast<SelectNode *>(node->NextNode());
    }
}

void
extract_styled_item(STEPWrapper &wrapper, SDAI_Application_instance *instance)
{
    SdaiStyled_item *styled = dynamic_cast<SdaiStyled_item *>(instance);
    if (!styled || !styled->item_())
	return;
    const int64_t item_id = styled->item_()->STEPfile_id;
    if (item_id <= 0)
	return;

    Style &style = wrapper.Document().styles[item_id];
    style.item_entity_id = item_id;
    append_id(style, styled);
    const std::string name = brlcad::step::decode_string(wrapper.getStringAttribute(styled, "name"));
    if (!name.empty()) style.name = name;

    EntityAggregate *assignments = static_cast<EntityAggregate *>(styled->styles_());
    EntityNode *node = assignments ? static_cast<EntityNode *>(assignments->GetHead()) : NULL;
    while (node) {
	extract_assignment(wrapper, dynamic_cast<SdaiPresentation_style_assignment *>(node->node), style);
	node = static_cast<EntityNode *>(node->NextNode());
    }
}

SDAI_Application_instance *
layered_item_entity(SdaiLayered_item *item)
{
    if (!item)
	return NULL;
    if (item->IsRepresentation_item() == LTrue) {
	SdaiRepresentation_item *representation_item = *item;
	return representation_item;
    }
    if (item->IsPresentation_representation() == LTrue) {
	SdaiPresentation_representation *representation = *item;
	return representation;
    }
    return NULL;
}

void
extract_layer(STEPWrapper &wrapper, SDAI_Application_instance *instance)
{
    SdaiPresentation_layer_assignment *assignment =
	dynamic_cast<SdaiPresentation_layer_assignment *>(instance);
    if (!assignment || !assignment->assigned_items_())
	return;

    Layer &layer = wrapper.Document().layers[instance->STEPfile_id];
    layer.entity_id = instance->STEPfile_id;
    layer.name = brlcad::step::decode_string(wrapper.getStringAttribute(instance, "name"));
    layer.description = brlcad::step::decode_string(wrapper.getStringAttribute(instance, "description"));

    SelectNode *node = static_cast<SelectNode *>(assignment->assigned_items_()->GetHead());
    while (node) {
	SdaiLayered_item *item = dynamic_cast<SdaiLayered_item *>(node->node);
	SDAI_Application_instance *entity = layered_item_entity(item);
	if (entity && entity->STEPfile_id > 0) {
	    const int64_t id = entity->STEPfile_id;
	    if (std::find(layer.item_entity_ids.begin(), layer.item_entity_ids.end(), id) ==
		layer.item_entity_ids.end())
		layer.item_entity_ids.push_back(id);
	    Style &style = wrapper.Document().styles[id];
	    style.item_entity_id = id;
	    append_layer(style, layer.name);
	}
	node = static_cast<SelectNode *>(node->NextNode());
    }
}

} // namespace

static std::vector<uint64_t>
selected_presentation_ids(STEPWrapper &wrapper)
{
    const std::set<int64_t> &roots = wrapper.ImportOptions().selected_entity_ids;
    if (roots.empty() || !wrapper.HasLazyIndex())
	return std::vector<uint64_t>();

    /* A style is an inverse relationship: the STYLED_ITEM points at geometry,
     * rather than being in the geometry root's forward dependency closure.
     * Build that closure without materializing SDAI objects, then retain only
     * presentation roots that refer to something in it.  This preserves face,
     * edge, and solid styles for targeted conversion without loading every
     * style in a million-instance assembly. */
    std::set<uint64_t> closure;
    std::vector<uint64_t> pending;
    for (std::set<int64_t>::const_iterator root = roots.begin(); root != roots.end(); ++root) {
	if (*root <= 0) continue;
	const uint64_t id = static_cast<uint64_t>(*root);
	if (closure.insert(id).second) pending.push_back(id);
    }
    for (size_t i = 0; i < pending.size(); ++i) {
	const std::vector<uint64_t> references = wrapper.LazyForwardReferences(pending[i]);
	for (std::vector<uint64_t>::const_iterator reference = references.begin();
	     reference != references.end(); ++reference) {
	    if (closure.insert(*reference).second) pending.push_back(*reference);
	}
    }

    std::set<uint64_t> presentation;
    const char *types[] = {"STYLED_ITEM", "OVER_RIDING_STYLED_ITEM",
	"PRESENTATION_LAYER_ASSIGNMENT"};
    for (size_t t = 0; t < sizeof(types) / sizeof(types[0]); ++t) {
	const std::vector<uint64_t> ids = wrapper.LazyInstancesByType(types[t]);
	for (std::vector<uint64_t>::const_iterator id = ids.begin(); id != ids.end(); ++id) {
	    const std::vector<uint64_t> references = wrapper.LazyForwardReferences(*id);
	    for (std::vector<uint64_t>::const_iterator reference = references.begin();
		 reference != references.end(); ++reference) {
		if (closure.find(*reference) == closure.end()) continue;
		presentation.insert(*id);
		break;
	    }
	}
    }
    return std::vector<uint64_t>(presentation.begin(), presentation.end());
}

void
ExtractAP214Presentation(STEPWrapper &wrapper)
{
    wrapper.Document().styles.clear();
    wrapper.Document().layers.clear();
    const std::vector<uint64_t> selected = selected_presentation_ids(wrapper);
    if (!wrapper.ImportOptions().selected_entity_ids.empty() && wrapper.HasLazyIndex())
	wrapper.SetInstanceIds(selected);
    else
	wrapper.SetInstanceTypes({"STYLED_ITEM", "OVER_RIDING_STYLED_ITEM",
	    "PRESENTATION_LAYER_ASSIGNMENT"});
    for (int i = 0; i < wrapper.InstanceCount(); ++i) {
	SDAI_Application_instance *instance = wrapper.InstanceAt(i);
	if (!instance || instance->STEPfile_id <= 0)
	    continue;
	if (instance->IsA(SCHEMA_NAMESPACE::e_styled_item))
	    extract_styled_item(wrapper, instance);
	if (instance->IsA(SCHEMA_NAMESPACE::e_presentation_layer_assignment))
	    extract_layer(wrapper, instance);
    }
    wrapper.Statistics().styles_extracted = wrapper.Document().styles.size();
    wrapper.Statistics().layers_extracted = wrapper.Document().layers.size();
    wrapper.ResetInstanceTypes();
}
