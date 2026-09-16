/*       A P 2 0 3 C O N F I G U R A T I O N E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP203ConfigurationExport.h"

#include "AP_Common.h"
#include "STEPConfigurationExport.h"
#include "STEPExportContext.h"
#include "STEPGeneratedAPI.h"
#include "StepExportPlan.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using brlcad::step::ConfigurationExportStatistics;
using brlcad::step::ExportConfigurationRecordPlan;

typedef std::pair<std::string, int64_t> SourceKey;

struct Component {
    std::string type;
    std::vector<std::string> arguments;
};

static std::string
trim(const std::string &value)
{
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return std::string();
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

static bool
equal_type(const std::string &left, const char *right)
{
    if (!right || left.size() != std::char_traits<char>::length(right))
	return false;
    for (size_t i = 0; i < left.size(); ++i)
	if (std::toupper(static_cast<unsigned char>(left[i])) !=
		std::toupper(static_cast<unsigned char>(right[i]))) return false;
    return true;
}

static bool
part21_arguments(const std::string &value, const std::string &type,
    std::vector<std::string> &arguments, std::string &error)
{
    const std::string source = trim(value);
    const size_t open = source.find('(');
    if (open == std::string::npos || source.empty() || source.back() != ')' ||
	    !equal_type(trim(source.substr(0, open)), type.c_str())) {
	error = "Part 21 value does not match its entity type";
	return false;
    }
    const std::string body = source.substr(open + 1,
	source.size() - open - 2);
    size_t begin = 0;
    int depth = 0;
    bool quoted = false;
    for (size_t i = 0; i <= body.size(); ++i) {
	if (i < body.size() && body[i] == '\'') {
	    if (quoted && i + 1 < body.size() && body[i + 1] == '\'') {
		++i;
		continue;
	    }
	    quoted = !quoted;
	    continue;
	}
	if (!quoted && i < body.size() && body[i] == '(') {
	    ++depth;
	    continue;
	}
	if (!quoted && i < body.size() && body[i] == ')') {
	    if (--depth < 0) {
		error = "Part 21 value has unbalanced parentheses";
		return false;
	    }
	    continue;
	}
	if (i != body.size() && (quoted || depth || body[i] != ',')) continue;
	const std::string argument = trim(body.substr(begin, i - begin));
	if (argument.empty()) {
	    error = "Part 21 value has an empty argument";
	    return false;
	}
	arguments.push_back(argument);
	begin = i + 1;
    }
    if (quoted || depth) {
	error = "Part 21 value has an unterminated string or aggregate";
	return false;
    }
    return true;
}

static bool
part21_reference(const std::string &value, int64_t &reference)
{
    if (value.size() < 2 || value[0] != '#') return false;
    errno = 0;
    char *end = NULL;
    const long long parsed = std::strtoll(value.c_str() + 1, &end, 10);
    if (errno == ERANGE || !end || *end || parsed <= 0) return false;
    reference = static_cast<int64_t>(parsed);
    return true;
}

static bool
part21_string(const std::string &value)
{
    return value.size() >= 2 && value.front() == '\'' && value.back() == '\'';
}

static bool
optional_part21_string(const std::string &value)
{
    return value == "$" || part21_string(value);
}

static std::vector<int64_t>
part21_references(const std::string &value)
{
    std::vector<int64_t> references;
    bool quoted = false;
    for (size_t i = 0; i < value.size(); ++i) {
	if (value[i] == '\'') {
	    if (quoted && i + 1 < value.size() && value[i + 1] == '\'') {
		++i;
		continue;
	    }
	    quoted = !quoted;
	    continue;
	}
	if (quoted || value[i] != '#') continue;
	size_t end = i + 1;
	while (end < value.size() &&
		std::isdigit(static_cast<unsigned char>(value[end]))) ++end;
	int64_t reference = 0;
	if (end > i + 1 && part21_reference(value.substr(i, end - i), reference))
	    references.push_back(reference);
	i = end ? end - 1 : end;
    }
    return references;
}

static bool
complex_components(const ExportConfigurationRecordPlan &record,
    std::vector<Component> &components, std::string &error)
{
    const std::string value = trim(record.value);
    if (value.size() < 3 || value.front() != '(' || value.back() != ')') {
	error = "complex Part 21 value has no enclosing parentheses";
	return false;
    }
    const std::string body = value.substr(1, value.size() - 2);
    size_t position = 0;
    while (position < body.size()) {
	while (position < body.size() &&
		std::isspace(static_cast<unsigned char>(body[position]))) ++position;
	if (position == body.size()) break;
	const size_t type_begin = position;
	while (position < body.size() &&
		(std::isalnum(static_cast<unsigned char>(body[position])) ||
		 body[position] == '_')) ++position;
	if (position == type_begin) {
	    error = "complex Part 21 value has an invalid component keyword";
	    return false;
	}
	const std::string type = body.substr(type_begin, position - type_begin);
	while (position < body.size() &&
		std::isspace(static_cast<unsigned char>(body[position]))) ++position;
	if (position == body.size() || body[position] != '(') {
	    error = "complex Part 21 component has no argument list";
	    return false;
	}
	const size_t value_begin = type_begin;
	int depth = 0;
	bool quoted = false;
	for (; position < body.size(); ++position) {
	    if (body[position] == '\'') {
		if (quoted && position + 1 < body.size() &&
			body[position + 1] == '\'') {
		    ++position;
		    continue;
		}
		quoted = !quoted;
		continue;
	    }
	    if (quoted) continue;
	    if (body[position] == '(') ++depth;
	    if (body[position] == ')' && --depth == 0) {
		++position;
		break;
	    }
	}
	if (quoted || depth) {
	    error = "complex Part 21 component is unterminated";
	    return false;
	}
	Component component;
	component.type = type;
	if (!part21_arguments(body.substr(value_begin, position - value_begin),
		type, component.arguments, error)) return false;
	components.push_back(component);
    }
    if (components.size() != record.component_types.size()) {
	error = "retained complex component inventory disagrees with its value";
	return false;
    }
    for (size_t i = 0; i < components.size(); ++i)
	if (!equal_type(components[i].type,
		record.component_types[i].c_str())) {
	    error = "retained complex component order disagrees with its value";
	    return false;
	}
    return true;
}

static ExportConfigurationRecordPlan *
record_by_id(brlcad::step::StepExportPlan &plan, const std::string &schema,
    int64_t id, const char *type)
{
    for (ExportConfigurationRecordPlan &record : plan.configuration_records)
	if (record.schema == schema && record.entity_id == id && record.valid &&
		equal_type(record.type, type)) return &record;
    return NULL;
}

static void
omit(ExportConfigurationRecordPlan &record, const std::string &status,
    const std::string &reason, ConfigurationExportStatistics &statistics)
{
    record.export_status = status;
    record.export_reason = reason;
    ++statistics.omitted;
}

static void
emitted(ExportConfigurationRecordPlan &record, const std::string &reason,
    ConfigurationExportStatistics &statistics)
{
    if (record.export_status == "emitted") return;
    record.export_status = "emitted";
    record.export_reason = reason;
    ++statistics.emitted;
}

static bool
validate_simple_record(ExportConfigurationRecordPlan &record,
    size_t argument_count, const std::vector<int64_t> &references,
    std::vector<std::string> &arguments, std::string &error)
{
    return part21_arguments(record.value, record.type, arguments, error) &&
	arguments.size() == argument_count && record.references == references;
}

static bool
set_optional(STEPentity *entity, const char *name, const std::string &value,
    InstMgr *instances)
{
    return value == "$" || brlcad::step::SetPart21(entity, name, value,
	instances);
}

static void
destroy_configuration_pending(std::vector<STEPentity *> &pending)
{
    for (STEPentity *entity : pending) delete entity;
    pending.clear();
}

} // namespace

void
brlcad::step::EmitAP203ComplexConfiguration(StepExportPlan &plan,
    AP203_Contents *contents,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &usages,
    const std::map<SourceKey, STEPentity *> &date_times,
    const std::map<SourceKey, STEPentity *> &measures,
    std::map<SourceKey, STEPentity *> &effectivities,
    std::map<SourceKey, std::string> &effectivity_types,
    ConfigurationExportStatistics &statistics)
{
    if (!contents || !contents->registry || !contents->instance_list ||
	    !contents->application_context) return;

    std::map<SourceKey, STEPentity *> authored;
    bool primary_application_claimed = false;
    for (ExportConfigurationRecordPlan &complex_record :
	    plan.configuration_records) {
	if (!complex_record.valid || !equal_type(complex_record.type, "COMPLEX") ||
		complex_record.export_status != "pending") continue;

	std::vector<Component> components;
	std::string error;
	if (!complex_components(complex_record, components, error) ||
		complex_record.references != part21_references(complex_record.value)) {
	    if (error.empty()) error = "complex references disagree with its value";
	    omit(complex_record, "malformed", error, statistics);
	    continue;
	}
	std::map<std::string, const Component *> by_type;
	for (const Component &component : components) {
	    std::string type = component.type;
	    std::transform(type.begin(), type.end(), type.begin(),
		[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	    if (!by_type.insert(std::make_pair(type, &component)).second) {
		error = "complex effectivity repeats a component type";
		break;
	    }
	}
	const auto component = [&by_type](const char *type) -> const Component * {
	    const auto found = by_type.find(type);
	    return found == by_type.end() ? NULL : found->second;
	};
	const Component *configuration = component("CONFIGURATION_EFFECTIVITY");
	const Component *effectivity = component("EFFECTIVITY");
	const Component *product_effectivity =
	    component("PRODUCT_DEFINITION_EFFECTIVITY");
	const Component *dated = component("DATED_EFFECTIVITY");
	const Component *serial = component("SERIAL_NUMBERED_EFFECTIVITY");
	const Component *lot = component("LOT_EFFECTIVITY");
	const size_t variant_count = (dated ? 1 : 0) + (serial ? 1 : 0) +
	    (lot ? 1 : 0);
	if (!error.empty() || !configuration || !effectivity ||
		!product_effectivity || variant_count != 1 ||
		by_type.size() != 4) {
	    if (error.empty()) error = "AP203 complex effectivity requires exactly "
		"CONFIGURATION_EFFECTIVITY, EFFECTIVITY, "
		"PRODUCT_DEFINITION_EFFECTIVITY, and one mandatory subtype";
	    omit(complex_record, "malformed", error, statistics);
	    continue;
	}

	int64_t design_id = 0;
	int64_t usage_id = 0;
	if (configuration->arguments.size() != 1 ||
		!part21_reference(configuration->arguments[0], design_id) ||
		effectivity->arguments.size() != 1 ||
		!part21_string(effectivity->arguments[0]) ||
		product_effectivity->arguments.size() != 1 ||
		!part21_reference(product_effectivity->arguments[0], usage_id)) {
	    omit(complex_record, "malformed",
		"AP203 complex effectivity has an invalid component layout",
		statistics);
	    continue;
	}
	const auto usage = usages.find(usage_id);
	if (usage == usages.end() || !usage->second) {
	    omit(complex_record, "unsupported",
		"the complex effectivity usage has no emitted occurrence",
		statistics);
	    continue;
	}

	ExportConfigurationRecordPlan *design_record = record_by_id(plan,
	    complex_record.schema, design_id, "CONFIGURATION_DESIGN");
	std::vector<std::string> design_arguments;
	int64_t item_id = 0;
	int64_t formation_id = 0;
	if (!design_record || !part21_arguments(design_record->value,
		design_record->type, design_arguments, error) ||
		design_arguments.size() != 2 ||
		!part21_reference(design_arguments[0], item_id) ||
		!part21_reference(design_arguments[1], formation_id) ||
		design_record->references !=
		    std::vector<int64_t>({item_id, formation_id})) {
	    omit(complex_record, "unsupported",
		"the complex effectivity has no complete CONFIGURATION_DESIGN",
		statistics);
	    continue;
	}
	const auto formation = formations.find(formation_id);
	if (formation == formations.end() || !formation->second) {
	    omit(complex_record, "unsupported",
		"the configuration design has no emitted product formation",
		statistics);
	    continue;
	}

	ExportConfigurationRecordPlan *item_record = record_by_id(plan,
	    complex_record.schema, item_id, "CONFIGURATION_ITEM");
	std::vector<std::string> item_arguments;
	int64_t concept_id = 0;
	if (!item_record || !part21_arguments(item_record->value,
		item_record->type, item_arguments, error) ||
		item_arguments.size() != 5 || !part21_string(item_arguments[0]) ||
		!part21_string(item_arguments[1]) ||
		!optional_part21_string(item_arguments[2]) ||
		!part21_reference(item_arguments[3], concept_id) ||
		!optional_part21_string(item_arguments[4]) ||
		item_record->references != std::vector<int64_t>({concept_id})) {
	    omit(complex_record, "unsupported",
		"the complex effectivity has no complete CONFIGURATION_ITEM",
		statistics);
	    continue;
	}

	ExportConfigurationRecordPlan *concept_record = record_by_id(plan,
	    complex_record.schema, concept_id, "PRODUCT_CONCEPT");
	std::vector<std::string> concept_arguments;
	int64_t context_id = 0;
	if (!concept_record || !part21_arguments(concept_record->value,
		concept_record->type, concept_arguments, error) ||
		concept_arguments.size() != 4 ||
		!part21_string(concept_arguments[0]) ||
		!part21_string(concept_arguments[1]) ||
		!part21_string(concept_arguments[2]) ||
		!part21_reference(concept_arguments[3], context_id) ||
		concept_record->references != std::vector<int64_t>({context_id})) {
	    omit(complex_record, "unsupported",
		"the complex effectivity has no complete PRODUCT_CONCEPT",
		statistics);
	    continue;
	}

	ExportConfigurationRecordPlan *context_record = record_by_id(plan,
	    complex_record.schema, context_id, "PRODUCT_CONCEPT_CONTEXT");
	std::vector<std::string> context_arguments;
	int64_t application_id = 0;
	if (!context_record || !part21_arguments(context_record->value,
		context_record->type, context_arguments, error) ||
		context_arguments.size() != 3 ||
		!part21_string(context_arguments[0]) ||
		!part21_reference(context_arguments[1], application_id) ||
		!part21_string(context_arguments[2]) ||
		context_record->references != std::vector<int64_t>({application_id})) {
	    omit(complex_record, "unsupported",
		"the complex effectivity has no complete PRODUCT_CONCEPT_CONTEXT",
		statistics);
	    continue;
	}

	ExportConfigurationRecordPlan *application_record = record_by_id(plan,
	    complex_record.schema, application_id, "APPLICATION_CONTEXT");
	std::vector<std::string> application_arguments;
	if (!application_record || !validate_simple_record(*application_record, 1,
		std::vector<int64_t>(), application_arguments, error) ||
		!part21_string(application_arguments[0])) {
	    omit(complex_record, "unsupported",
		"the complex effectivity has no complete APPLICATION_CONTEXT",
		statistics);
	    continue;
	}

	STEPentity *start = NULL;
	STEPentity *end = NULL;
	STEPentity *measure = NULL;
	if (dated) {
	    int64_t start_id = 0;
	    int64_t end_id = 0;
	    if (dated->arguments.size() != 2 ||
		    !part21_reference(dated->arguments[0], start_id) ||
		    (dated->arguments[1] != "$" &&
		     !part21_reference(dated->arguments[1], end_id))) {
		error = "AP203 DATED_EFFECTIVITY component has invalid bounds";
	    } else {
		const auto start_found = date_times.find(SourceKey(
		    complex_record.schema, start_id));
		const auto end_found = end_id ? date_times.find(SourceKey(
		    complex_record.schema, end_id)) : date_times.end();
		start = start_found == date_times.end() ? NULL : start_found->second;
		end = !end_id ? NULL : (end_found == date_times.end() ? NULL :
		    end_found->second);
		if (!start || (end_id && !end))
		    error = "an AP203 dated-effectivity bound was not emitted";
	    }
	} else if (serial) {
	    if (serial->arguments.size() != 2 ||
		    !part21_string(serial->arguments[0]) ||
		    !optional_part21_string(serial->arguments[1]))
		error = "AP203 serial-effectivity component has invalid bounds";
	} else {
	    int64_t measure_id = 0;
	    if (lot->arguments.size() != 2 ||
		    !part21_string(lot->arguments[0]) ||
		    !part21_reference(lot->arguments[1], measure_id)) {
		error = "AP203 lot-effectivity component has an invalid lot value";
	    } else {
		const auto found = measures.find(SourceKey(complex_record.schema,
		    measure_id));
		measure = found == measures.end() ? NULL : found->second;
		if (!measure) error = "the AP203 lot-size measure was not emitted";
	    }
	}
	if (!error.empty()) {
	    omit(complex_record, "unsupported", error, statistics);
	    continue;
	}

	std::vector<STEPentity *> pending;
	std::vector<SourceKey> pending_keys;
	const auto make = [&pending, contents](const char *type) -> STEPentity * {
	    STEPentity *entity = contents->registry->ObjCreate(type);
	    if (!entity || isNilSTEPentity(entity)) return NULL;
	    pending.push_back(entity);
	    return entity;
	};
	const auto get_or_make = [&authored, &make, &pending_keys](
	    const SourceKey &key,
	    const char *type) -> STEPentity * {
	    const auto found = authored.find(key);
	    if (found != authored.end()) return found->second;
	    STEPentity *entity = make(type);
	    if (entity) {
		authored[key] = entity;
		pending_keys.push_back(key);
	    }
	    return entity;
	};
	const SourceKey application_key(complex_record.schema, application_id);
	STEPentity *application = NULL;
	bool claimed_primary_now = false;
	const auto application_found = authored.find(application_key);
	if (application_found != authored.end()) {
	    application = application_found->second;
	} else if (!primary_application_claimed) {
	    application = contents->application_context;
	    authored[application_key] = application;
	    primary_application_claimed = true;
	    claimed_primary_now = true;
	} else {
	    application = get_or_make(application_key, "APPLICATION_CONTEXT");
	}
	bool valid = application && brlcad::step::SetPart21(application,
	    "application", application_arguments[0], contents->instance_list);
	STEPentity *context = get_or_make(SourceKey(complex_record.schema,
	    context_id), "PRODUCT_CONCEPT_CONTEXT");
	STEPentity *concept_entity = get_or_make(SourceKey(complex_record.schema,
	    concept_id), "PRODUCT_CONCEPT");
	STEPentity *item = get_or_make(SourceKey(complex_record.schema, item_id),
	    "CONFIGURATION_ITEM");
	STEPentity *design = get_or_make(SourceKey(complex_record.schema,
	    design_id), "CONFIGURATION_DESIGN");
	valid = context && concept_entity && item && design &&
	    brlcad::step::SetPart21(context, "name", context_arguments[0],
		contents->instance_list) &&
	    brlcad::step::SetEntity(context, "frame_of_reference",
		application) &&
	    brlcad::step::SetPart21(context, "market_segment_type",
		context_arguments[2], contents->instance_list) &&
	    brlcad::step::SetPart21(concept_entity, "id", concept_arguments[0],
		contents->instance_list) &&
	    brlcad::step::SetPart21(concept_entity, "name", concept_arguments[1],
		contents->instance_list) &&
	    brlcad::step::SetPart21(concept_entity, "description", concept_arguments[2],
		contents->instance_list) &&
	    brlcad::step::SetEntity(concept_entity, "market_context", context) &&
	    brlcad::step::SetPart21(item, "id", item_arguments[0],
		contents->instance_list) &&
	    brlcad::step::SetPart21(item, "name", item_arguments[1],
		contents->instance_list) &&
	    set_optional(item, "description", item_arguments[2],
		contents->instance_list) &&
	    brlcad::step::SetEntity(item, "item_concept", concept_entity) &&
	    set_optional(item, "purpose", item_arguments[4],
		contents->instance_list) &&
	    brlcad::step::SetEntity(design, "configuration", item) &&
	    brlcad::step::SetEntity(design, "design", formation->second) && valid;

	std::vector<const char *> type_names;
	for (const std::string &type : complex_record.component_types)
	    type_names.push_back(type.c_str());
	type_names.push_back("*");
	/* A zero identifier asks InstMgr::Append to allocate the next unused Part
	 * 21 identifier.  InstanceCount is not an identifier generator and may be
	 * smaller than MaxFileId after complex or filtered instance creation. */
	STEPcomplex *complex = valid ? new STEPcomplex(contents->registry,
	    type_names.data(), 0) : NULL;
	STEPcomplex *configuration_part = complex ? complex->EntityPart(
	    "configuration_effectivity") : NULL;
	STEPcomplex *effectivity_part = complex ? complex->EntityPart(
	    "effectivity") : NULL;
	STEPcomplex *product_part = complex ? complex->EntityPart(
	    "product_definition_effectivity") : NULL;
	STEPcomplex *variant_part = complex ? complex->EntityPart(dated ?
	    "dated_effectivity" : (serial ? "serial_numbered_effectivity" :
	    "lot_effectivity")) : NULL;
	valid = complex && configuration_part && effectivity_part && product_part &&
	    variant_part && brlcad::step::SetEntity(configuration_part,
		"configuration", design) &&
	    brlcad::step::SetPart21(effectivity_part, "id",
		effectivity->arguments[0], contents->instance_list) &&
	    brlcad::step::SetEntity(product_part, "usage", usage->second) && valid;
	if (dated) {
	    valid = brlcad::step::SetEntity(variant_part,
		"effectivity_start_date", start) &&
		(end ? brlcad::step::SetEntity(variant_part,
		    "effectivity_end_date", end) : true) && valid;
	} else if (serial) {
	    valid = brlcad::step::SetPart21(variant_part,
		"effectivity_start_id", serial->arguments[0],
		contents->instance_list) && set_optional(variant_part,
		"effectivity_end_id", serial->arguments[1],
		contents->instance_list) && valid;
	} else {
	    valid = brlcad::step::SetPart21(variant_part,
		"effectivity_lot_id", lot->arguments[0],
		contents->instance_list) && brlcad::step::SetEntity(variant_part,
		"effectivity_lot_size", measure) && valid;
	}
	if (!valid) {
	    delete complex;
	    for (const SourceKey &key : pending_keys) authored.erase(key);
	    if (claimed_primary_now) {
		authored.erase(application_key);
		primary_application_claimed = false;
	    }
	    destroy_configuration_pending(pending);
	    omit(complex_record, "failed",
		"the AP203 complex effectivity layout is incompatible",
		statistics);
	    continue;
	}
	for (STEPentity *entity : pending)
	    contents->instance_list->Append(entity, completeSE);
	contents->instance_list->Append(complex, completeSE);
	effectivities[SourceKey(complex_record.schema,
	    complex_record.entity_id)] = complex;
	effectivity_types[SourceKey(complex_record.schema,
	    complex_record.entity_id)] = "CONFIGURATION_EFFECTIVITY";
	emitted(*application_record,
	    "reused as the AP203 export application context", statistics);
	emitted(*context_record,
	    "authored as AP203 configuration-item context", statistics);
	emitted(*concept_record,
	    "authored as AP203 configuration-item concept", statistics);
	emitted(*item_record,
	    "authored as AP203 configuration item", statistics);
	emitted(*design_record,
	    "authored with a remapped product formation", statistics);
	emitted(complex_record,
	    "authored as an AP203 complex configuration effectivity",
	    statistics);
    }
}
