/*               S T E P P R E S E N T A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "STEPPresentation.h"
#include "STEPString.h"
#include "STEPWrapper.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <set>
#include <string>
#include <vector>

namespace {

using brlcad::step::Layer;
using brlcad::step::Style;

std::string
trim_ascii(const std::string &value)
{
    size_t begin = 0;
    while (begin < value.size() &&
	std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    size_t end = value.size();
    while (end > begin &&
	std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}

/* Split only the outer argument list.  Nested aggregates, typed values,
 * strings, doubled apostrophes and comments remain intact. */
std::vector<std::string>
source_arguments(const std::string &source)
{
    std::vector<std::string> result;
    const size_t equal = source.find('=');
    const size_t open = equal == std::string::npos ? std::string::npos :
	source.find('(', equal + 1);
    if (open == std::string::npos) return result;

    size_t argument = open + 1;
    int depth = 1;
    bool string = false;
    bool comment = false;
    for (size_t i = open + 1; i < source.size(); ++i) {
	const char c = source[i];
	if (comment) {
	    if (c == '*' && i + 1 < source.size() && source[i + 1] == '/') {
		comment = false;
		++i;
	    }
	    continue;
	}
	if (string) {
	    if (c != '\'') continue;
	    if (i + 1 < source.size() && source[i + 1] == '\'') {
		++i;
		continue;
	    }
	    string = false;
	    continue;
	}
	if (c == '/' && i + 1 < source.size() && source[i + 1] == '*') {
	    comment = true;
	    ++i;
	    continue;
	}
	if (c == '\'') {
	    string = true;
	    continue;
	}
	if (c == '(') {
	    ++depth;
	    continue;
	}
	if (c == ')') {
	    if (--depth == 0) {
		result.push_back(trim_ascii(source.substr(argument, i - argument)));
		break;
	    }
	    continue;
	}
	if (c == ',' && depth == 1) {
	    result.push_back(trim_ascii(source.substr(argument, i - argument)));
	    argument = i + 1;
	}
    }
    return result;
}

std::vector<uint64_t>
source_references(const std::string &value)
{
    std::vector<uint64_t> result;
    bool string = false;
    bool comment = false;
    for (size_t i = 0; i < value.size(); ++i) {
	const char c = value[i];
	if (comment) {
	    if (c == '*' && i + 1 < value.size() && value[i + 1] == '/') {
		comment = false;
		++i;
	    }
	    continue;
	}
	if (string) {
	    if (c == '\'' && i + 1 < value.size() && value[i + 1] == '\'') ++i;
	    else if (c == '\'') string = false;
	    continue;
	}
	if (c == '/' && i + 1 < value.size() && value[i + 1] == '*') {
	    comment = true;
	    ++i;
	    continue;
	}
	if (c == '\'') {
	    string = true;
	    continue;
	}
	if (c != '#' || i + 1 >= value.size() ||
	    !std::isdigit(static_cast<unsigned char>(value[i + 1]))) continue;
	char *end = NULL;
	const unsigned long long id = std::strtoull(value.c_str() + i + 1, &end, 10);
	if (end && end != value.c_str() + i + 1) {
	    result.push_back(static_cast<uint64_t>(id));
	    i = static_cast<size_t>(end - value.c_str()) - 1;
	}
    }
    return result;
}

bool
source_real(const std::string &value, double &result)
{
    const std::string text = trim_ascii(value);
    if (text.empty()) return false;
    char *end = NULL;
    result = std::strtod(text.c_str(), &end);
    while (end && *end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    return end && end != text.c_str() && *end == '\0';
}

bool
is_presentation_graph_type(const std::string &type)
{
    return type.find("STYLE") != std::string::npos ||
	type.find("COLOUR") != std::string::npos ||
	type == "PRESENTATION_STYLE_ASSIGNMENT";
}

void
append_id(Style &style, int64_t id)
{
    if (id <= 0) return;
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
	if (name != colour.name) continue;
	style.has_rgb = true;
	style.rgb[0] = colour.r;
	style.rgb[1] = colour.g;
	style.rgb[2] = colour.b;
	return true;
    }
    return false;
}

void
update_predefined_colour_coverage(STEPWrapper &wrapper)
{
    const char *types[] = {
	"DRAUGHTING_PRE_DEFINED_COLOUR", "PRE_DEFINED_COLOUR"
    };
    for (size_t type_index = 0;
	    type_index < sizeof(types) / sizeof(types[0]); ++type_index) {
	const std::string type(types[type_index]);
	const std::vector<uint64_t> ids = wrapper.LazyInstancesByType(type);
	if (ids.empty()) continue;
	uint64_t unsupported = 0;
	for (std::vector<uint64_t>::const_iterator id = ids.begin();
		id != ids.end(); ++id) {
	    const std::vector<std::string> arguments =
		source_arguments(wrapper.LazySourceRecord(*id));
	    Style probe;
	    if (arguments.empty() ||
		    !set_predefined_colour(arguments.front(), probe))
		++unsupported;
	}
	if (unsupported)
	    wrapper.Document().unsupported_counts[type] = unsupported;
	else
	    wrapper.Document().unsupported_counts.erase(type);
    }
}

int64_t
styled_target(STEPWrapper &wrapper, uint64_t styled_id)
{
    const std::vector<std::string> arguments =
	source_arguments(wrapper.LazySourceRecord(styled_id));
    if (arguments.size() < 3) return 0;
    const std::vector<uint64_t> targets = source_references(arguments[2]);
    return targets.empty() ? 0 : static_cast<int64_t>(targets.front());
}

void
extract_style_graph(STEPWrapper &wrapper, const std::vector<uint64_t> &roots,
    Style &style)
{
    std::set<uint64_t> visited;
    std::deque<uint64_t> pending(roots.begin(), roots.end());
    while (!pending.empty()) {
	if (wrapper.CancellationRequested()) return;
	const uint64_t id = pending.front();
	pending.pop_front();
	if (!visited.insert(id).second) continue;
	const std::string type = wrapper.LazyTypeName(id);
	if (!is_presentation_graph_type(type)) continue;
	append_id(style, static_cast<int64_t>(id));

	const std::vector<std::string> arguments =
	    source_arguments(wrapper.LazySourceRecord(id));
	if (type == "COLOUR_RGB" && arguments.size() >= 4) {
	    double rgb[3] = {0.0, 0.0, 0.0};
	    if (source_real(arguments[1], rgb[0]) &&
		source_real(arguments[2], rgb[1]) &&
		source_real(arguments[3], rgb[2])) {
		style.has_rgb = true;
		for (size_t component = 0; component < 3; ++component)
		    style.rgb[component] = std::max(0.0, std::min(1.0, rgb[component]));
	    }
	} else if (type.find("PRE_DEFINED_COLOUR") != std::string::npos &&
	    !arguments.empty()) {
	    set_predefined_colour(arguments.front(), style);
	} else if (type == "SURFACE_STYLE_TRANSPARENT" && !arguments.empty()) {
	    double transparency = 0.0;
	    if (source_real(arguments.front(), transparency)) {
		style.has_transparency = true;
		style.transparency = std::max(0.0, std::min(1.0, transparency));
	    }
	}

	const std::vector<uint64_t> references = wrapper.LazyForwardReferences(id);
	for (std::vector<uint64_t>::const_iterator reference = references.begin();
	     reference != references.end(); ++reference) {
	    if (is_presentation_graph_type(wrapper.LazyTypeName(*reference)))
		pending.push_back(*reference);
	}
    }
}

void
extract_styled_item(STEPWrapper &wrapper, uint64_t id)
{
    const std::vector<std::string> arguments =
	source_arguments(wrapper.LazySourceRecord(id));
    if (arguments.size() < 3) return;
    const std::vector<uint64_t> targets = source_references(arguments[2]);
    if (targets.empty() || targets.front() == 0) return;

    Style &style = wrapper.Document().styles[static_cast<int64_t>(targets.front())];
    style.item_entity_id = static_cast<int64_t>(targets.front());
    append_id(style, static_cast<int64_t>(id));
    const std::string name = brlcad::step::decode_string(arguments.front());
    if (!name.empty()) style.name = name;

    std::vector<uint64_t> assignments = source_references(arguments[1]);
    if (assignments.empty()) {
	const std::vector<uint64_t> references = wrapper.LazyForwardReferences(id);
	for (std::vector<uint64_t>::const_iterator reference = references.begin();
	     reference != references.end(); ++reference) {
	    if (wrapper.LazyTypeName(*reference) == "PRESENTATION_STYLE_ASSIGNMENT")
		assignments.push_back(*reference);
	}
    }
    extract_style_graph(wrapper, assignments, style);
}

void
extract_layer(STEPWrapper &wrapper, uint64_t id)
{
    const std::vector<std::string> arguments =
	source_arguments(wrapper.LazySourceRecord(id));
    if (arguments.size() < 3) return;

    Layer &layer = wrapper.Document().layers[static_cast<int64_t>(id)];
    layer.entity_id = static_cast<int64_t>(id);
    layer.name = brlcad::step::decode_string(arguments[0]);
    layer.description = brlcad::step::decode_string(arguments[1]);
    const std::vector<uint64_t> assigned = source_references(arguments[2]);
    for (std::vector<uint64_t>::const_iterator item = assigned.begin();
	 item != assigned.end(); ++item) {
	if (*item == 0) continue;
	const int64_t source_id = static_cast<int64_t>(*item);
	if (std::find(layer.item_entity_ids.begin(), layer.item_entity_ids.end(), source_id) ==
	    layer.item_entity_ids.end())
	    layer.item_entity_ids.push_back(source_id);

	int64_t effective_id = source_id;
	const std::string type = wrapper.LazyTypeName(*item);
	if (type == "STYLED_ITEM" || type == "OVER_RIDING_STYLED_ITEM") {
	    const int64_t target = styled_target(wrapper, *item);
	    if (target > 0) effective_id = target;
	}
	Style &style = wrapper.Document().styles[effective_id];
	style.item_entity_id = effective_id;
	append_layer(style, layer.name);
    }
}

std::vector<uint64_t>
selected_presentation_ids(STEPWrapper &wrapper)
{
    const std::set<int64_t> &roots = wrapper.ImportOptions().selected_entity_ids;
    if (roots.empty()) return std::vector<uint64_t>();

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
		closure.insert(*id);
		break;
	    }
	}
    }
    return std::vector<uint64_t>(presentation.begin(), presentation.end());
}

} // namespace

bool
ExtractSTEPPresentation(STEPWrapper &wrapper)
{
    if (!wrapper.HasLazyIndex()) return false;

    wrapper.Document().styles.clear();
    wrapper.Document().layers.clear();
    std::vector<uint64_t> ids = selected_presentation_ids(wrapper);
    if (ids.empty() && wrapper.ImportOptions().selected_entity_ids.empty()) {
	const char *types[] = {"STYLED_ITEM", "OVER_RIDING_STYLED_ITEM",
	    "PRESENTATION_LAYER_ASSIGNMENT"};
	std::set<uint64_t> all;
	for (size_t type = 0; type < sizeof(types) / sizeof(types[0]); ++type) {
	    const std::vector<uint64_t> found = wrapper.LazyInstancesByType(types[type]);
	    all.insert(found.begin(), found.end());
	}
	ids.assign(all.begin(), all.end());
    }

    wrapper.SetProgress("extracting STEP presentation metadata", 0, ids.size());
    for (size_t i = 0; i < ids.size(); ++i) {
	if (wrapper.CancellationRequested()) return true;
	if ((i % 32) == 0)
	    wrapper.SetProgress("extracting STEP presentation metadata", i, ids.size());
	const std::string type = wrapper.LazyTypeName(ids[i]);
	if (type == "STYLED_ITEM" || type == "OVER_RIDING_STYLED_ITEM")
	    extract_styled_item(wrapper, ids[i]);
	else if (type == "PRESENTATION_LAYER_ASSIGNMENT")
	    extract_layer(wrapper, ids[i]);
    }
    wrapper.SetProgress("STEP presentation metadata extracted", ids.size(), ids.size());
    update_predefined_colour_coverage(wrapper);
    wrapper.Statistics().styles_extracted = wrapper.Document().styles.size();
    wrapper.Statistics().layers_extracted = wrapper.Document().layers.size();
    return true;
}
