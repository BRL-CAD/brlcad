/*        S T E P C O N F I G U R A T I O N E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "STEPConfigurationExport.h"
#include "STEPConfigurationIdentityExport.h"
#include "STEPExportContext.h"
#include "STEPGeneratedAPI.h"
#include "StepExportPlan.h"
#if defined(AP203)
#  include "ap203/AP203ConfigurationExport.h"
#endif

#include "raytrace.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using brlcad::step::ConfigurationExportStatistics;
using brlcad::step::ConfigurationIdentityEntities;
using brlcad::step::ExportConfigurationRecordPlan;
using brlcad::step::StepExportPlan;

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
    for (size_t i = 0; i < left.size(); ++i) {
	if (std::toupper(static_cast<unsigned char>(left[i])) !=
		std::toupper(static_cast<unsigned char>(right[i]))) return false;
    }
    return true;
}

static bool
type_prefix(const std::string &value, const char *prefix)
{
    if (!prefix) return false;
    const size_t length = std::char_traits<char>::length(prefix);
    return value.size() >= length && equal_type(value.substr(0, length), prefix);
}

static bool
part21_arguments(const ExportConfigurationRecordPlan &record,
    std::vector<std::string> &arguments, std::string &error)
{
    const std::string value = trim(record.value);
    const size_t open = value.find('(');
    if (open == std::string::npos || value.empty() || value.back() != ')' ||
	!equal_type(trim(value.substr(0, open)), record.type.c_str())) {
	error = "Part 21 value does not match the retained entity type";
	return false;
    }
    const std::string body = value.substr(open + 1, value.size() - open - 2);
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
part21_string(const std::string &value)
{
    return value.size() >= 2 && value.front() == '\'' && value.back() == '\'';
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
part21_reference_aggregate(const std::string &value,
    std::vector<int64_t> &references)
{
    const std::string aggregate = trim(value);
    if (aggregate.size() < 3 || aggregate.front() != '(' ||
	    aggregate.back() != ')') return false;
    const std::string body = aggregate.substr(1, aggregate.size() - 2);
    size_t begin = 0;
    for (size_t i = 0; i <= body.size(); ++i) {
	if (i != body.size() && body[i] != ',') continue;
	int64_t reference = 0;
	if (!part21_reference(trim(body.substr(begin, i - begin)), reference))
	    return false;
	references.push_back(reference);
	begin = i + 1;
    }
    return !references.empty();
}

static bool
optional_part21_string(const std::string &value)
{
    return value == "$" || part21_string(value);
}

static bool
part21_integer(const std::string &value, int64_t &number)
{
    if (value.empty()) return false;
    errno = 0;
    char *end = NULL;
    const long long parsed = std::strtoll(value.c_str(), &end, 10);
    if (errno == ERANGE || !end || end == value.c_str() || *end) return false;
    number = static_cast<int64_t>(parsed);
    return true;
}

static bool
optional_part21_integer(const std::string &value, int64_t &number)
{
    return value == "$" || part21_integer(value, number);
}

static bool
optional_part21_real(const std::string &value, double &number)
{
    if (value == "$") return true;
    errno = 0;
    char *end = NULL;
    const double parsed = std::strtod(value.c_str(), &end);
    if (errno == ERANGE || !end || end == value.c_str() || *end ||
	!std::isfinite(parsed))
	return false;
    number = parsed;
    return true;
}

static bool
part21_real(const std::string &value, double &number)
{
    return value != "$" && optional_part21_real(value, number);
}

static bool
part21_reference_or_omitted(const std::string &value, int64_t &reference)
{
    if (value == "$") {
	reference = 0;
	return true;
    }
    return part21_reference(value, reference);
}

static bool
part21_time_sense(const std::string &value)
{
    return equal_type(value, ".AHEAD.") || equal_type(value, ".EXACT.") ||
	equal_type(value, ".BEHIND.");
}

static void
omit(ExportConfigurationRecordPlan &record, const std::string &status,
    const std::string &reason, ConfigurationExportStatistics &statistics)
{
    record.export_status = status;
    record.export_reason = reason;
    ++statistics.omitted;
}

static STEPentity *
create_unregistered(AP203_Contents *contents, const char *type)
{
    STEPentity *entity = contents && contents->registry ?
	contents->registry->ObjCreate(type) : NULL;
    return entity && !isNilSTEPentity(entity) ? entity : NULL;
}

static void
emitted(ExportConfigurationRecordPlan &record, const std::string &reason,
    ConfigurationExportStatistics &statistics)
{
    record.export_status = "emitted";
    record.export_reason = reason;
    ++statistics.emitted;
}

static void
insert_unique(std::map<int64_t, STEPentity *> &entities, int64_t source_id,
    STEPentity *entity)
{
    if (source_id <= 0 || !entity) return;
    const auto previous = entities.find(source_id);
    if (previous == entities.end()) {
	entities[source_id] = entity;
    } else if (previous->second != entity) {
	previous->second = NULL;
    }
}

static STEPentity *
mapped_definition(const brlcad::step::ExportObjectPlan &object,
    AP203_Contents *contents)
{
    if (!contents || !contents->dbip || !contents->comb_to_step ||
	    !contents->solid_to_step) return NULL;
    struct directory *entry = db_lookup(contents->dbip, object.name.c_str(),
	LOOKUP_QUIET);
    if (entry == RT_DIR_NULL) return NULL;
    const auto combination = contents->comb_to_step->find(entry);
    if (combination != contents->comb_to_step->end()) return combination->second;
    const auto solid = contents->solid_to_step->find(entry);
    return solid == contents->solid_to_step->end() ? NULL : solid->second;
}

static STEPentity *
definition_product(STEPentity *definition)
{
    STEPentity *formation = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(definition, "formation"));
    return dynamic_cast<STEPentity *>(
	brlcad::step::Entity(formation, "of_product"));
}

static bool
source_attribute_id(const brlcad::step::ExportObjectPlan &object,
    const char *name, int64_t &source_id)
{
    const auto source = object.attributes.find(name ? name : "");
    return source != object.attributes.end() &&
	part21_reference("#" + source->second, source_id);
}

static void
source_entity_maps(const StepExportPlan &plan, AP203_Contents *contents,
    std::map<int64_t, STEPentity *> &products,
    std::map<int64_t, STEPentity *> &formations,
    std::map<int64_t, STEPentity *> &definitions,
    std::map<int64_t, STEPentity *> &usages)
{
    for (const brlcad::step::ExportObjectPlan &object : plan.objects) {
	STEPentity *definition = mapped_definition(object, contents);
	STEPentity *formation = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(definition, "formation"));
	STEPentity *product = definition_product(definition);
	int64_t source_id = 0;
	if (source_attribute_id(object, "step:source_id", source_id))
	    insert_unique(products, source_id, product);
	if (source_attribute_id(object, "step:formation_source_id", source_id))
	    insert_unique(formations, source_id, formation);
	if (source_attribute_id(object, "step:definition_source_id", source_id))
	    insert_unique(definitions, source_id, definition);
    }
    if (!contents || !contents->occurrence_to_step || !contents->dbip) return;
    for (const brlcad::step::ExportOccurrencePlan &occurrence : plan.occurrences) {
	if (occurrence.source_entity_id <= 0 || occurrence.parent >= plan.objects.size())
	    continue;
	struct directory *parent = db_lookup(contents->dbip,
	    plan.objects[occurrence.parent].name.c_str(), LOOKUP_QUIET);
	if (parent == RT_DIR_NULL) continue;
	const auto mapped = contents->occurrence_to_step->find(
	    std::make_pair(parent, occurrence.ordinal));
	if (mapped != contents->occurrence_to_step->end())
	    insert_unique(usages, occurrence.source_entity_id, mapped->second);
    }
}

#if !defined(AP203)
static STEPentity *
mapped_configuration_item(int64_t source_id,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages)
{
    STEPentity *result = NULL;
    const std::map<int64_t, STEPentity *> *maps[] = {
	&products, &formations, &definitions, &usages
    };
    for (const std::map<int64_t, STEPentity *> *entities : maps) {
	const auto mapped = entities->find(source_id);
	if (mapped == entities->end() || !mapped->second) continue;
	if (result && result != mapped->second) return NULL;
	result = mapped->second;
    }
    return result;
}
#endif

enum class ManagementItemKind {
    None,
    Product,
    Formation,
    Definition,
    Usage,
    Classification
};

struct MappedManagementItem {
    STEPentity *entity = NULL;
    ManagementItemKind kind = ManagementItemKind::None;
    bool ambiguous = false;
};

static void
consider_management_item(MappedManagementItem &result, STEPentity *entity,
    ManagementItemKind kind)
{
    if (!entity || result.ambiguous) return;
    if (result.entity && result.entity != entity) {
	result.entity = NULL;
	result.kind = ManagementItemKind::None;
	result.ambiguous = true;
	return;
    }
    result.entity = entity;
    result.kind = kind;
}

static MappedManagementItem
mapped_management_item(int64_t source_id, const std::string &schema,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    const ConfigurationIdentityEntities &identities)
{
    MappedManagementItem result;
    const auto product = products.find(source_id);
    if (product != products.end())
	consider_management_item(result, product->second,
	    ManagementItemKind::Product);
    const auto formation = formations.find(source_id);
    if (formation != formations.end())
	consider_management_item(result, formation->second,
	    ManagementItemKind::Formation);
    const auto definition = definitions.find(source_id);
    if (definition != definitions.end())
	consider_management_item(result, definition->second,
	    ManagementItemKind::Definition);
    const auto usage = usages.find(source_id);
    if (usage != usages.end())
	consider_management_item(result, usage->second, ManagementItemKind::Usage);
    const auto classification = identities.classifications.find(
	ConfigurationIdentityEntities::SourceKey(schema, source_id));
    if (classification != identities.classifications.end())
	consider_management_item(result, classification->second,
	    ManagementItemKind::Classification);
    return result;
}

static STEPentity *
mapped_temporal_entity(int64_t source_id, const std::string &schema,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &dates,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &times,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &date_times,
    bool date_and_time_only)
{
    STEPentity *result = NULL;
    const std::pair<std::string, int64_t> key(schema, source_id);
    const std::map<std::pair<std::string, int64_t>, STEPentity *> *maps[] = {
	&date_times, &dates, &times
    };
    const size_t map_count = date_and_time_only ? 1 : 3;
    for (size_t i = 0; i < map_count; ++i) {
	const auto entity = maps[i]->find(key);
	if (entity == maps[i]->end() || !entity->second) continue;
	if (result && result != entity->second) return NULL;
	result = entity->second;
    }
    return result;
}

static STEPentity *
mapped_person_organization(int64_t source_id, const std::string &schema,
    const ConfigurationIdentityEntities &identities, bool combined_only)
{
    STEPentity *result = NULL;
    const ConfigurationIdentityEntities::SourceKey key(schema, source_id);
    const std::map<ConfigurationIdentityEntities::SourceKey, STEPentity *>
	*maps[] = {
	    &identities.person_organizations, &identities.people,
	    &identities.organizations
	};
    const size_t map_count = combined_only ? 1 : 3;
    for (size_t i = 0; i < map_count; ++i) {
	const auto entity = maps[i]->find(key);
	if (entity == maps[i]->end() || !entity->second) continue;
	if (result && result != entity->second) return NULL;
	result = entity->second;
    }
    return result;
}

static bool
emit_calendar_date(ExportConfigurationRecordPlan &record,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &dates,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t year = 0;
    int64_t day = 0;
    int64_t month = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
	    !part21_integer(arguments.empty() ? std::string() : arguments[0],
		year) ||
	    !part21_integer(arguments.size() < 2 ? std::string() : arguments[1],
		day) ||
	    !part21_integer(arguments.size() < 3 ? std::string() : arguments[2],
		month) || !record.references.empty()) {
	if (error.empty()) error = "CALENDAR_DATE has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    static const int month_days[] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month < 1 || month > 12 || day < 1) {
	omit(record, "malformed", "CALENDAR_DATE is outside its legal range",
	    statistics);
	return false;
    }
    int maximum_day = month_days[month];
    const bool leap = !(year % 4) && ((year % 100) || !(year % 400));
    if (month == 2 && leap) ++maximum_day;
    if (day > maximum_day) {
	omit(record, "malformed", "CALENDAR_DATE is not a valid calendar day",
	    statistics);
	return false;
    }
    STEPentity *date = create_unregistered(contents, "CALENDAR_DATE");
    if (!date) {
	omit(record, "unsupported", "the target schema has no CALENDAR_DATE entity",
	    statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(date, "year_component",
	arguments[0], contents->instance_list) &&
	brlcad::step::SetPart21(date, "day_component", arguments[1],
	    contents->instance_list) &&
	brlcad::step::SetPart21(date, "month_component", arguments[2],
	    contents->instance_list);
    if (!valid) {
	delete date;
	omit(record, "failed", "the target calendar-date layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(date, completeSE);
    dates[std::make_pair(record.schema, record.entity_id)] = date;
    emitted(record, "authored as a validated calendar date", statistics);
    return true;
}

static bool
emit_time_offset(ExportConfigurationRecordPlan &record,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &offsets,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t hour = 0;
    int64_t minute = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
	    !part21_integer(arguments.empty() ? std::string() : arguments[0],
		hour) ||
	    !optional_part21_integer(arguments.size() < 2 ? std::string() :
		arguments[1], minute) ||
	    !part21_time_sense(arguments.size() < 3 ? std::string() :
		arguments[2]) || !record.references.empty()) {
	if (error.empty())
	    error = "COORDINATED_UNIVERSAL_TIME_OFFSET has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    if (hour < 0 || hour >= 24 ||
	    (arguments[1] != "$" && (minute < 0 || minute > 59))) {
	omit(record, "malformed",
	    "COORDINATED_UNIVERSAL_TIME_OFFSET is outside its legal range",
	    statistics);
	return false;
    }
    STEPentity *offset = create_unregistered(contents,
	"COORDINATED_UNIVERSAL_TIME_OFFSET");
    if (!offset) {
	omit(record, "unsupported",
	    "the target schema has no COORDINATED_UNIVERSAL_TIME_OFFSET entity",
	    statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(offset, "hour_offset",
	arguments[0], contents->instance_list) &&
	brlcad::step::SetPart21(offset, "minute_offset", arguments[1],
	    contents->instance_list) &&
	brlcad::step::SetPart21(offset, "sense", arguments[2],
	    contents->instance_list);
    if (!valid) {
	delete offset;
	omit(record, "failed", "the target time-offset layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(offset, completeSE);
    offsets[std::make_pair(record.schema, record.entity_id)] = offset;
    emitted(record, "authored as a validated UTC offset", statistics);
    return true;
}

static bool
emit_local_time(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &offsets,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &times,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t hour = 0;
    int64_t minute = 0;
    double second = 0.0;
    int64_t offset_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 4 ||
	    !part21_integer(arguments.empty() ? std::string() : arguments[0],
		hour) ||
	    !optional_part21_integer(arguments.size() < 2 ? std::string() :
		arguments[1], minute) ||
	    !optional_part21_real(arguments.size() < 3 ? std::string() :
		arguments[2], second) ||
	    !part21_reference(arguments.size() < 4 ? std::string() : arguments[3],
		offset_id) || record.references != std::vector<int64_t>({offset_id})) {
	if (error.empty()) error = "LOCAL_TIME has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    if (hour < 0 || hour >= 24 ||
	    (arguments[1] != "$" && (minute < 0 || minute > 59)) ||
	    (arguments[2] != "$" && (second < 0.0 || second > 60.0)) ||
	    (arguments[2] != "$" && arguments[1] == "$")) {
	omit(record, "malformed", "LOCAL_TIME is outside its legal range",
	    statistics);
	return false;
    }
    const auto offset = offsets.find(std::make_pair(record.schema, offset_id));
    if (offset == offsets.end() || !offset->second) {
	omit(record, "unsupported", "the retained UTC-offset dependency was not emitted",
	    statistics);
	return false;
    }
    STEPentity *time = create_unregistered(contents, "LOCAL_TIME");
    if (!time) {
	omit(record, "unsupported", "the target schema has no LOCAL_TIME entity",
	    statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(time, "hour_component",
	arguments[0], contents->instance_list) &&
	brlcad::step::SetPart21(time, "minute_component", arguments[1],
	    contents->instance_list) &&
	brlcad::step::SetPart21(time, "second_component", arguments[2],
	    contents->instance_list) &&
	brlcad::step::SetEntity(time, "zone", offset->second);
    if (!valid) {
	delete time;
	omit(record, "failed", "the target local-time layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(time, completeSE);
    times[std::make_pair(record.schema, record.entity_id)] = time;
    emitted(record, "authored with a remapped UTC-offset reference", statistics);
    return true;
}

static bool
emit_date_and_time(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &dates,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &times,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &date_times,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t date_id = 0;
    int64_t time_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
	    !part21_reference(arguments.empty() ? std::string() : arguments[0],
		date_id) ||
	    !part21_reference(arguments.size() < 2 ? std::string() : arguments[1],
		time_id) ||
	    record.references != std::vector<int64_t>({date_id, time_id})) {
	if (error.empty()) error = "DATE_AND_TIME has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    const auto date = dates.find(std::make_pair(record.schema, date_id));
    const auto time = times.find(std::make_pair(record.schema, time_id));
    if (date == dates.end() || time == times.end() || !date->second ||
	    !time->second) {
	omit(record, "unsupported",
	    "a retained DATE_AND_TIME dependency was not emitted", statistics);
	return false;
    }
    STEPentity *date_time = create_unregistered(contents, "DATE_AND_TIME");
    if (!date_time) {
	omit(record, "unsupported", "the target schema has no DATE_AND_TIME entity",
	    statistics);
	return false;
    }
    const bool valid = brlcad::step::SetEntity(date_time, "date_component",
	date->second) && brlcad::step::SetEntity(date_time, "time_component",
	    time->second);
    if (!valid) {
	delete date_time;
	omit(record, "failed", "the target date-and-time layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(date_time, completeSE);
    date_times[std::make_pair(record.schema, record.entity_id)] = date_time;
    emitted(record, "authored with remapped date and local-time references",
	statistics);
    return true;
}

static bool
emit_dimensional_exponents(ExportConfigurationRecordPlan &record,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &dimensions,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 7 ||
	    !record.references.empty()) {
	if (error.empty())
	    error = "DIMENSIONAL_EXPONENTS has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    for (const std::string &argument : arguments) {
	double exponent = 0.0;
	if (part21_real(argument, exponent)) continue;
	omit(record, "malformed",
	    "DIMENSIONAL_EXPONENTS requires seven finite numbers", statistics);
	return false;
    }
    STEPentity *entity = create_unregistered(contents,
	"DIMENSIONAL_EXPONENTS");
    if (!entity) {
	omit(record, "unsupported",
	    "the target schema has no DIMENSIONAL_EXPONENTS entity", statistics);
	return false;
    }
    static const char *const names[] = {
	"length_exponent", "mass_exponent", "time_exponent",
	"electric_current_exponent", "thermodynamic_temperature_exponent",
	"amount_of_substance_exponent", "luminous_intensity_exponent"
    };
    bool valid = true;
    for (size_t i = 0; i < arguments.size(); ++i)
	valid = brlcad::step::SetPart21(entity, names[i], arguments[i],
	    contents->instance_list) && valid;
    if (!valid) {
	delete entity;
	omit(record, "failed",
	    "the target dimensional-exponents layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(entity, completeSE);
    dimensions[std::make_pair(record.schema, record.entity_id)] = entity;
    emitted(record, "authored as validated dimensional exponents", statistics);
    return true;
}

static bool
emit_context_dependent_unit(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &dimensions,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &units,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t dimensions_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
	    !part21_reference(arguments.empty() ? std::string() : arguments[0],
		dimensions_id) ||
	    !part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
	    record.references != std::vector<int64_t>({dimensions_id})) {
	if (error.empty())
	    error = "CONTEXT_DEPENDENT_UNIT has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    const auto dimensional_exponents = dimensions.find(
	std::make_pair(record.schema, dimensions_id));
    if (dimensional_exponents == dimensions.end() ||
	    !dimensional_exponents->second) {
	omit(record, "unsupported",
	    "the retained unit dimensions were not emitted", statistics);
	return false;
    }
    STEPentity *unit = create_unregistered(contents, "CONTEXT_DEPENDENT_UNIT");
    if (!unit) {
	omit(record, "unsupported",
	    "the target schema has no CONTEXT_DEPENDENT_UNIT entity", statistics);
	return false;
    }
    const bool valid = brlcad::step::SetEntity(unit, "dimensions",
	dimensional_exponents->second) && brlcad::step::SetPart21(unit, "name",
	    arguments[1], contents->instance_list);
    if (!valid) {
	delete unit;
	omit(record, "failed",
	    "the target context-dependent-unit layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(unit, completeSE);
    units[std::make_pair(record.schema, record.entity_id)] = unit;
    emitted(record, "authored with remapped unit dimensions", statistics);
    return true;
}

static bool
emit_measure_with_unit(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &units,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &measures,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t unit_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
	    arguments.empty() || arguments[0] == "$" ||
	    !part21_reference(arguments.size() < 2 ? std::string() : arguments[1],
		unit_id) || record.references != std::vector<int64_t>({unit_id})) {
	if (error.empty()) error = "MEASURE_WITH_UNIT has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    const auto unit = units.find(std::make_pair(record.schema, unit_id));
    if (unit == units.end() || !unit->second) {
	omit(record, "unsupported", "the retained measure unit was not emitted",
	    statistics);
	return false;
    }
    STEPentity *measure = create_unregistered(contents, "MEASURE_WITH_UNIT");
    if (!measure) {
	omit(record, "unsupported",
	    "the target schema has no MEASURE_WITH_UNIT entity", statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(measure, "value_component",
	arguments[0], contents->instance_list) && brlcad::step::SetEntity(measure,
	    "unit_component", unit->second);
    if (!valid) {
	delete measure;
	omit(record, "unsupported",
	    "the retained measure value is not legal in the target schema",
	    statistics);
	return false;
    }
    contents->instance_list->Append(measure, completeSE);
    measures[std::make_pair(record.schema, record.entity_id)] = measure;
    emitted(record, "authored with its retained typed value and remapped unit",
	statistics);
    return true;
}

static bool
emit_lot_effectivity(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &measures,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &effectivities,
    std::map<std::pair<std::string, int64_t>, std::string> &effectivity_types,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t measure_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
	    !part21_string(arguments.empty() ? std::string() : arguments[0]) ||
	    !part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
	    !part21_reference(arguments.size() < 3 ? std::string() : arguments[2],
		measure_id) ||
	    record.references != std::vector<int64_t>({measure_id})) {
	if (error.empty()) error = "LOT_EFFECTIVITY has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
#if defined(AP203)
    (void)measures;
    (void)contents;
    (void)effectivities;
    (void)effectivity_types;
    omit(record, "unsupported",
	"AP203 edition 1 requires effectivity as a complex "
	"CONFIGURATION_EFFECTIVITY instance", statistics);
    return false;
#else
    const auto measure = measures.find(std::make_pair(record.schema, measure_id));
    if (measure == measures.end() || !measure->second) {
	omit(record, "unsupported",
	    "the retained lot-size measure was not emitted", statistics);
	return false;
    }
    STEPentity *effectivity = create_unregistered(contents, "LOT_EFFECTIVITY");
    if (!effectivity) {
	omit(record, "unsupported",
	    "the target schema has no LOT_EFFECTIVITY entity", statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(effectivity, "id", arguments[0],
	contents->instance_list) && brlcad::step::SetPart21(effectivity,
	    "effectivity_lot_id", arguments[1], contents->instance_list) &&
	brlcad::step::SetEntity(effectivity, "effectivity_lot_size",
	    measure->second);
    if (!valid) {
	delete effectivity;
	omit(record, "failed", "the target lot-effectivity layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(effectivity, completeSE);
    effectivities[std::make_pair(record.schema, record.entity_id)] = effectivity;
    effectivity_types[std::make_pair(record.schema, record.entity_id)] =
	record.type;
    emitted(record, "authored with a remapped retained lot-size measure",
	statistics);
    return true;
#endif
}

static void
emit_date_time_assignments(StepExportPlan &plan, AP203_Contents *contents,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    const ConfigurationIdentityEntities &identities,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &dates,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &times,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &date_times,
    ConfigurationExportStatistics &statistics)
{
    typedef ConfigurationIdentityEntities::SourceKey SourceKey;
    std::map<SourceKey, STEPentity *> roles;
    std::map<SourceKey, std::string> role_names;
    std::map<SourceKey, ExportConfigurationRecordPlan *> role_records;
    std::set<SourceKey> used_roles;

#if defined(AP203)
    static const std::set<std::string> allowed_roles = {
	"'creation_date'", "'request_date'", "'release_date'", "'start_date'",
	"'contract_date'", "'certification_date'", "'sign_off_date'",
	"'classification_date'", "'declassification_date'"
    };
#endif

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "DATE_TIME_ROLE")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 1 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!record.references.empty()) {
	    if (error.empty()) error =
		"DATE_TIME_ROLE has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
#if defined(AP203)
	if (allowed_roles.find(arguments[0]) == allowed_roles.end()) {
	    omit(record, "unsupported",
		"AP203 edition 1 forbids the retained date-and-time role",
		statistics);
	    continue;
	}
#endif
	STEPentity *role = create_unregistered(contents, "DATE_TIME_ROLE");
	if (!role) {
	    omit(record, "unsupported",
		"the target schema has no DATE_TIME_ROLE entity", statistics);
	    continue;
	}
	if (!brlcad::step::SetPart21(role, "name", arguments[0],
		contents->instance_list)) {
	    delete role;
	    omit(record, "failed", "the target date-time role is incompatible",
		statistics);
	    continue;
	}
	const SourceKey key(record.schema, record.entity_id);
	roles[key] = role;
	role_names[key] = arguments[0];
	role_records[key] = &record;
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		(!equal_type(record.type, "CC_DESIGN_DATE_AND_TIME_ASSIGNMENT") &&
		 !equal_type(record.type,
		    "APPLIED_DATE_AND_TIME_ASSIGNMENT"))) continue;
	std::vector<std::string> arguments;
	std::vector<int64_t> item_ids;
	std::string error;
	int64_t date_time_id = 0;
	int64_t role_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
		!part21_reference(arguments.empty() ? std::string() : arguments[0],
		    date_time_id) ||
		!part21_reference(arguments.size() < 2 ? std::string() :
		    arguments[1], role_id) ||
		!part21_reference_aggregate(arguments.size() < 3 ? std::string() :
		    arguments[2], item_ids)) {
	    if (error.empty()) error = record.type +
		" has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	std::vector<int64_t> references({date_time_id, role_id});
	references.insert(references.end(), item_ids.begin(), item_ids.end());
	if (record.references != references) {
	    omit(record, "malformed", record.type +
		" references disagree with its retained Part 21 value", statistics);
	    continue;
	}
	const SourceKey role_key(record.schema, role_id);
	const auto role = roles.find(role_key);
	const auto role_name = role_names.find(role_key);
	STEPentity *assigned = mapped_temporal_entity(date_time_id, record.schema,
	    dates, times, date_times,
#if defined(AP203)
	    true
#else
	    false
#endif
	    );
	if (!assigned || role == roles.end() || !role->second ||
		role_name == role_names.end()) {
	    omit(record, "unsupported",
		"a retained date-and-time assignment dependency was not emitted",
		statistics);
	    continue;
	}
	std::set<int64_t> unique_items;
	std::vector<MappedManagementItem> items;
	bool valid_items = true;
	for (int64_t item_id : item_ids) {
	    if (!unique_items.insert(item_id).second) {
		omit(record, "malformed",
		    "a date-and-time assignment item appears more than once",
		    statistics);
		valid_items = false;
		break;
	    }
	    MappedManagementItem item = mapped_management_item(item_id,
		record.schema, products, formations, definitions, usages, identities);
	    if (!item.entity || item.ambiguous) {
		omit(record, "unsupported",
		    "a date-and-time assignment item has no unambiguous emitted entity",
		    statistics);
		valid_items = false;
		break;
	    }
#if defined(AP203)
	    const bool legal =
		(role_name->second == "'creation_date'" &&
		 item.kind == ManagementItemKind::Definition) ||
		((role_name->second == "'classification_date'" ||
		  role_name->second == "'declassification_date'") &&
		 item.kind == ManagementItemKind::Classification);
	    if (!legal) {
		omit(record, "unsupported",
		    "the retained date-time role and item are not legal in AP203",
		    statistics);
		valid_items = false;
		break;
	    }
#endif
	    items.push_back(item);
	}
	if (!valid_items) continue;
#if defined(AP203)
	const char *target_type = "CC_DESIGN_DATE_AND_TIME_ASSIGNMENT";
#else
	const char *target_type = "APPLIED_DATE_AND_TIME_ASSIGNMENT";
#endif
	STEPentity *assignment = create_unregistered(contents, target_type);
	if (!assignment) {
	    omit(record, "unsupported",
		std::string("the target schema has no ") + target_type + " entity",
		statistics);
	    continue;
	}
	bool valid = brlcad::step::SetEntity(assignment,
	    "assigned_date_and_time", assigned) &&
	    brlcad::step::SetEntity(assignment, "role", role->second);
	for (const MappedManagementItem &item : items)
	    valid = brlcad::step::AddEntity(assignment, "items", item.entity) &&
		valid;
	if (!valid) {
	    delete assignment;
	    omit(record, "unsupported",
		"the retained date-and-time assignment items are not legal in the "
		"target assignment SELECT", statistics);
	    continue;
	}
	if (used_roles.insert(role_key).second) {
	    contents->instance_list->Append(role->second, completeSE);
	    emitted(*role_records[role_key], "authored as a retained date-time role",
		statistics);
	}
	contents->instance_list->Append(assignment, completeSE);
	emitted(record, std::string("authored as ") + target_type +
	    " with remapped date-time, role, and item references", statistics);
    }

    for (const auto &entry : role_records) {
	if (used_roles.find(entry.first) != used_roles.end()) continue;
#if defined(AP203)
	delete roles[entry.first];
	omit(*entry.second, "unsupported",
	    "AP203 date-time roles must be used by an emitted assignment",
	    statistics);
#else
	contents->instance_list->Append(roles[entry.first], completeSE);
	emitted(*entry.second, "authored as a retained date-time role",
	    statistics);
#endif
    }
}

static bool
emit_approval_assignment(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &approvals,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    const ConfigurationIdentityEntities &identities,
    AP203_Contents *contents, ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::vector<int64_t> item_ids;
    std::string error;
    int64_t approval_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
	    !part21_reference(arguments.empty() ? std::string() : arguments[0],
		approval_id) ||
	    !part21_reference_aggregate(arguments.size() < 2 ? std::string() :
		arguments[1], item_ids)) {
	if (error.empty())
	    error = record.type + " has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    std::vector<int64_t> references(1, approval_id);
    references.insert(references.end(), item_ids.begin(), item_ids.end());
    if (record.references != references) {
	omit(record, "malformed",
	    record.type + " references disagree with its retained Part 21 value",
	    statistics);
	return false;
    }
    const auto approval = approvals.find(
	std::make_pair(record.schema, approval_id));
    if (approval == approvals.end() || !approval->second) {
	omit(record, "unsupported",
	    "the retained APPROVAL dependency was not emitted", statistics);
	return false;
    }
    std::vector<MappedManagementItem> items;
    for (int64_t item_id : item_ids) {
	if (std::count(item_ids.begin(), item_ids.end(), item_id) != 1) {
	    omit(record, "malformed",
		"an approval-assignment item appears more than once", statistics);
	    return false;
	}
	MappedManagementItem item = mapped_management_item(item_id, record.schema,
	    products, formations, definitions, usages, identities);
	if (!item.entity || item.ambiguous) {
	    omit(record, "unsupported",
		"an approval-assignment item has no unambiguous emitted entity",
		statistics);
	    return false;
	}
#if defined(AP203)
	if (item.kind != ManagementItemKind::Formation &&
		item.kind != ManagementItemKind::Definition &&
		item.kind != ManagementItemKind::Classification) {
	    omit(record, "unsupported",
		"the retained approval item is not legal in AP203", statistics);
	    return false;
	}
#endif
	items.push_back(item);
    }
#if defined(AP203)
    const char *target_type = "CC_DESIGN_APPROVAL";
#else
    const char *target_type = "APPLIED_APPROVAL_ASSIGNMENT";
#endif
    STEPentity *assignment = create_unregistered(contents, target_type);
    if (!assignment) {
	omit(record, "unsupported",
	    std::string("the target schema has no ") + target_type + " entity",
	    statistics);
	return false;
    }
    bool valid = brlcad::step::SetEntity(assignment, "assigned_approval",
	approval->second);
    for (const MappedManagementItem &item : items)
	valid = brlcad::step::AddEntity(assignment, "items", item.entity) && valid;
    if (!valid) {
	delete assignment;
	omit(record, "unsupported",
	    "the retained approval items are not legal in the target assignment SELECT",
	    statistics);
	return false;
    }
    contents->instance_list->Append(assignment, completeSE);
    emitted(record, std::string("authored as ") + target_type +
	" with remapped approval and item references", statistics);
    return true;
}

static bool
emit_approval_relationship(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &approvals,
    AP203_Contents *contents, ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t relating_id = 0;
    int64_t related_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 4 ||
	    !part21_string(arguments.empty() ? std::string() : arguments[0]) ||
	    !optional_part21_string(arguments.size() < 2 ? std::string() :
		arguments[1]) ||
	    !part21_reference(arguments.size() < 3 ? std::string() : arguments[2],
		relating_id) ||
	    !part21_reference(arguments.size() < 4 ? std::string() : arguments[3],
		related_id) ||
	    record.references != std::vector<int64_t>({relating_id, related_id})) {
	if (error.empty())
	    error = "APPROVAL_RELATIONSHIP has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
#if defined(AP203)
    if (arguments[1] == "$") {
	omit(record, "unsupported",
	    "AP203 edition 1 requires an approval-relationship description",
	    statistics);
	return false;
    }
#endif
    const auto relating = approvals.find(
	std::make_pair(record.schema, relating_id));
    const auto related = approvals.find(
	std::make_pair(record.schema, related_id));
    if (relating == approvals.end() || related == approvals.end() ||
	    !relating->second || !related->second) {
	omit(record, "unsupported",
	    "a retained APPROVAL_RELATIONSHIP dependency was not emitted",
	    statistics);
	return false;
    }
    STEPentity *relationship = create_unregistered(contents,
	"APPROVAL_RELATIONSHIP");
    if (!relationship) {
	omit(record, "unsupported",
	    "the target schema has no APPROVAL_RELATIONSHIP entity", statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(relationship, "name",
	arguments[0], contents->instance_list) &&
	brlcad::step::SetPart21(relationship, "description", arguments[1],
	    contents->instance_list) &&
	brlcad::step::SetEntity(relationship, "relating_approval",
	    relating->second) &&
	brlcad::step::SetEntity(relationship, "related_approval", related->second);
    if (!valid) {
	delete relationship;
	omit(record, "failed",
	    "the target approval-relationship layout is incompatible", statistics);
	return false;
    }
    contents->instance_list->Append(relationship, completeSE);
    emitted(record, "authored with remapped approval references", statistics);
    return true;
}

static void
emit_approval_provenance(StepExportPlan &plan, AP203_Contents *contents,
    const ConfigurationIdentityEntities &identities,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &approvals,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &dates,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &times,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &date_times,
    ConfigurationExportStatistics &statistics)
{
    typedef ConfigurationIdentityEntities::SourceKey SourceKey;
    std::map<SourceKey, STEPentity *> roles;
    std::map<SourceKey, ExportConfigurationRecordPlan *> role_records;
    std::set<SourceKey> used_roles;

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "APPROVAL_ROLE")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 1 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!record.references.empty()) {
	    if (error.empty()) error = "APPROVAL_ROLE has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	STEPentity *role = create_unregistered(contents, "APPROVAL_ROLE");
	if (!role) {
	    omit(record, "unsupported",
		"the target schema has no APPROVAL_ROLE entity", statistics);
	    continue;
	}
	if (!brlcad::step::SetPart21(role, "role", arguments[0],
		contents->instance_list)) {
	    delete role;
	    omit(record, "failed", "the target approval-role layout is incompatible",
		statistics);
	    continue;
	}
	const SourceKey key(record.schema, record.entity_id);
	roles[key] = role;
	role_records[key] = &record;
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "APPROVAL_PERSON_ORGANIZATION")) continue;
	std::vector<std::string> arguments;
	std::string error;
	int64_t identity_id = 0;
	int64_t approval_id = 0;
	int64_t role_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
		!part21_reference(arguments.empty() ? std::string() : arguments[0],
		    identity_id) ||
		!part21_reference(arguments.size() < 2 ? std::string() :
		    arguments[1], approval_id) ||
		!part21_reference(arguments.size() < 3 ? std::string() :
		    arguments[2], role_id) ||
		record.references !=
		    std::vector<int64_t>({identity_id, approval_id, role_id})) {
	    if (error.empty()) error =
		"APPROVAL_PERSON_ORGANIZATION has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	const SourceKey approval_key(record.schema, approval_id);
	const SourceKey role_key(record.schema, role_id);
	const auto approval = approvals.find(approval_key);
	const auto role = roles.find(role_key);
	STEPentity *identity = mapped_person_organization(identity_id,
	    record.schema, identities,
#if defined(AP203)
	    true
#else
	    false
#endif
	    );
	if (!identity || approval == approvals.end() || !approval->second ||
		role == roles.end() || !role->second) {
	    omit(record, "unsupported",
		"an approval person/organization dependency was not emitted",
		statistics);
	    continue;
	}
	STEPentity *provenance = create_unregistered(contents,
	    "APPROVAL_PERSON_ORGANIZATION");
	if (!provenance) {
	    omit(record, "unsupported",
		"the target schema has no APPROVAL_PERSON_ORGANIZATION entity",
		statistics);
	    continue;
	}
	const bool valid = brlcad::step::SetEntity(provenance,
	    "person_organization", identity) &&
	    brlcad::step::SetEntity(provenance, "authorized_approval",
		approval->second) &&
	    brlcad::step::SetEntity(provenance, "role", role->second);
	if (!valid) {
	    delete provenance;
	    omit(record, "unsupported",
		"the retained approval identity is not legal in the target SELECT",
		statistics);
	    continue;
	}
	if (used_roles.insert(role_key).second) {
	    contents->instance_list->Append(role->second, completeSE);
	    emitted(*role_records[role_key], "authored as a retained approval role",
		statistics);
	}
	contents->instance_list->Append(provenance, completeSE);
	emitted(record,
	    "authored with remapped identity, approval, and role references",
	    statistics);
    }

    for (const auto &entry : role_records) {
	if (used_roles.find(entry.first) != used_roles.end()) continue;
	contents->instance_list->Append(roles[entry.first], completeSE);
	emitted(*entry.second, "authored as a retained approval role", statistics);
    }

    struct ApprovalDateCandidate {
	ExportConfigurationRecordPlan *record = NULL;
	STEPentity *date_time = NULL;
	STEPentity *approval = NULL;
	SourceKey approval_key;
    };
    std::vector<ApprovalDateCandidate> candidates;
    std::map<SourceKey, size_t> candidate_counts;
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "APPROVAL_DATE_TIME")) continue;
	std::vector<std::string> arguments;
	std::string error;
	int64_t date_time_id = 0;
	int64_t approval_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
		!part21_reference(arguments.empty() ? std::string() : arguments[0],
		    date_time_id) ||
		!part21_reference(arguments.size() < 2 ? std::string() :
		    arguments[1], approval_id) ||
		record.references != std::vector<int64_t>({date_time_id, approval_id})) {
	    if (error.empty()) error =
		"APPROVAL_DATE_TIME has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	const SourceKey approval_key(record.schema, approval_id);
	const auto approval = approvals.find(approval_key);
	STEPentity *date_time = mapped_temporal_entity(date_time_id, record.schema,
	    dates, times, date_times,
#if defined(AP203)
	    true
#else
	    false
#endif
	    );
	if (!date_time || approval == approvals.end() || !approval->second) {
	    omit(record, "unsupported",
		"an approval-date dependency was not emitted", statistics);
	    continue;
	}
	ApprovalDateCandidate candidate;
	candidate.record = &record;
	candidate.date_time = date_time;
	candidate.approval = approval->second;
	candidate.approval_key = approval_key;
	candidates.push_back(candidate);
	++candidate_counts[approval_key];
    }

    for (const ApprovalDateCandidate &candidate : candidates) {
#if defined(AP203)
	if (candidate_counts[candidate.approval_key] != 1) {
	    omit(*candidate.record, "unsupported",
		"AP203 requires exactly one date-and-time record per approval",
		statistics);
	    continue;
	}
#endif
	STEPentity *provenance = create_unregistered(contents,
	    "APPROVAL_DATE_TIME");
	if (!provenance) {
	    omit(*candidate.record, "unsupported",
		"the target schema has no APPROVAL_DATE_TIME entity", statistics);
	    continue;
	}
	const bool valid = brlcad::step::SetEntity(provenance, "date_time",
	    candidate.date_time) && brlcad::step::SetEntity(provenance,
		"dated_approval", candidate.approval);
	if (!valid) {
	    delete provenance;
	    omit(*candidate.record, "unsupported",
		"the retained approval date is not legal in the target SELECT",
		statistics);
	    continue;
	}
	contents->instance_list->Append(provenance, completeSE);
	emitted(*candidate.record,
	    "authored with remapped date-and-time and approval references",
	    statistics);
    }
}

static bool
emit_product_definition_effectivity(ExportConfigurationRecordPlan &record,
    const std::map<int64_t, STEPentity *> &usages, AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &effectivities,
    std::map<std::pair<std::string, int64_t>, std::string> &effectivity_types,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t usage_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
	    !part21_string(arguments.empty() ? std::string() : arguments[0]) ||
	    !part21_reference(arguments.size() < 2 ? std::string() : arguments[1],
		usage_id) || record.references != std::vector<int64_t>({usage_id})) {
	if (error.empty())
	    error = "PRODUCT_DEFINITION_EFFECTIVITY has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
#if defined(AP203)
    (void)usages;
    (void)contents;
    (void)effectivities;
    (void)effectivity_types;
    omit(record, "unsupported",
	"AP203 edition 1 requires effectivity as a complex "
	"CONFIGURATION_EFFECTIVITY instance", statistics);
    return false;
#else
    const auto usage = usages.find(usage_id);
    if (usage == usages.end() || !usage->second) {
	omit(record, "unsupported",
	    "the effectivity's source usage has no unambiguous emitted usage",
	    statistics);
	return false;
    }
    STEPentity *effectivity = create_unregistered(contents,
	"PRODUCT_DEFINITION_EFFECTIVITY");
    if (!effectivity) {
	omit(record, "unsupported",
	    "the target schema has no PRODUCT_DEFINITION_EFFECTIVITY entity",
	    statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(effectivity, "id", arguments[0],
	contents->instance_list) &&
	brlcad::step::SetEntity(effectivity, "usage", usage->second);
    if (!valid) {
	delete effectivity;
	omit(record, "failed",
	    "the target product-definition-effectivity layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(effectivity, completeSE);
    effectivities[std::make_pair(record.schema, record.entity_id)] = effectivity;
    effectivity_types[std::make_pair(record.schema, record.entity_id)] =
	record.type;
    emitted(record, "authored with a remapped source-usage reference",
	statistics);
    return true;
#endif
}

static bool
emit_dated_effectivity(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &date_times,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &effectivities,
    std::map<std::pair<std::string, int64_t>, std::string> &effectivity_types,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
	    !part21_string(arguments.empty() ? std::string() : arguments[0])) {
	if (error.empty()) error = "DATED_EFFECTIVITY has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }

    int64_t start_id = 0;
    int64_t end_id = 0;
    std::vector<int64_t> references;
    if (equal_type(record.schema, "AP203")) {
	if (!part21_reference(arguments[1], start_id) ||
		!part21_reference_or_omitted(arguments[2], end_id)) {
	    omit(record, "malformed",
		"AP203 DATED_EFFECTIVITY requires start then optional end",
		statistics);
	    return false;
	}
	references.push_back(start_id);
	if (end_id) references.push_back(end_id);
    } else if (equal_type(record.schema, "AP203e2") ||
	    equal_type(record.schema, "AP214") ||
	    type_prefix(record.schema, "AP242")) {
	if (!part21_reference_or_omitted(arguments[1], end_id) ||
		!part21_reference_or_omitted(arguments[2], start_id) ||
		(!start_id && !end_id)) {
	    omit(record, "malformed",
		"modern DATED_EFFECTIVITY requires optional end then start, "
		"with at least one bound", statistics);
	    return false;
	}
	if ((equal_type(record.schema, "AP203e2") ||
		equal_type(record.schema, "AP214")) && !start_id) {
	    omit(record, "malformed",
		"the retained source schema requires a dated-effectivity start",
		statistics);
	    return false;
	}
	if (end_id) references.push_back(end_id);
	if (start_id) references.push_back(start_id);
    } else {
	omit(record, "unsupported",
	    "the retained DATED_EFFECTIVITY source schema is not recognized",
	    statistics);
	return false;
    }
    if (record.references != references) {
	omit(record, "malformed",
	    "DATED_EFFECTIVITY references disagree with its retained "
	    "source-schema layout", statistics);
	return false;
    }

#if defined(AP203)
    (void)date_times;
    (void)contents;
    (void)effectivities;
    (void)effectivity_types;
    omit(record, "unsupported",
	"AP203 edition 1 requires effectivity as a complex "
	"CONFIGURATION_EFFECTIVITY instance", statistics);
    return false;
#else
#if defined(AP203e2) || defined(AP214e3)
    if (!start_id) {
	omit(record, "unsupported",
	    "the target schema requires a dated-effectivity start bound",
	    statistics);
	return false;
    }
#endif
    STEPentity *start = NULL;
    STEPentity *end = NULL;
    if (start_id) {
	const auto found = date_times.find(
	    std::make_pair(record.schema, start_id));
	if (found != date_times.end()) start = found->second;
    }
    if (end_id) {
	const auto found = date_times.find(std::make_pair(record.schema, end_id));
	if (found != date_times.end()) end = found->second;
    }
    if ((start_id && !start) || (end_id && !end)) {
	omit(record, "unsupported",
	    "a retained dated-effectivity bound was not emitted", statistics);
	return false;
    }
    STEPentity *effectivity = create_unregistered(contents, "DATED_EFFECTIVITY");
    if (!effectivity) {
	omit(record, "unsupported", "the target schema has no DATED_EFFECTIVITY entity",
	    statistics);
	return false;
    }
    bool valid = brlcad::step::SetPart21(effectivity, "id", arguments[0],
	contents->instance_list);
    valid = (end ? brlcad::step::SetEntity(effectivity,
	"effectivity_end_date", end) : brlcad::step::SetPart21(effectivity,
	"effectivity_end_date", "$", contents->instance_list)) && valid;
    valid = (start ? brlcad::step::SetEntity(effectivity,
	"effectivity_start_date", start) : brlcad::step::SetPart21(effectivity,
	"effectivity_start_date", "$", contents->instance_list)) && valid;
    if (!valid) {
	delete effectivity;
	omit(record, "failed",
	    "the target dated-effectivity layout is incompatible", statistics);
	return false;
    }
    contents->instance_list->Append(effectivity, completeSE);
    effectivities[std::make_pair(record.schema, record.entity_id)] = effectivity;
    effectivity_types[std::make_pair(record.schema, record.entity_id)] =
	record.type;
    emitted(record,
	"authored by target attribute name with remapped date bounds", statistics);
    return true;
#endif
}

static bool
emit_serial_numbered_effectivity(ExportConfigurationRecordPlan &record,
    AP203_Contents *contents,
    std::map<std::pair<std::string, int64_t>, STEPentity *> &effectivities,
    std::map<std::pair<std::string, int64_t>, std::string> &effectivity_types,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
	    !part21_string(arguments.empty() ? std::string() : arguments[0]) ||
	    !part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
	    !optional_part21_string(arguments.size() < 3 ? std::string() :
		arguments[2]) || !record.references.empty()) {
	if (error.empty())
	    error = "SERIAL_NUMBERED_EFFECTIVITY has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
#if defined(AP203)
    (void)contents;
    (void)effectivities;
    (void)effectivity_types;
    omit(record, "unsupported",
	"AP203 edition 1 requires effectivity as a complex "
	"CONFIGURATION_EFFECTIVITY instance", statistics);
    return false;
#else
    STEPentity *effectivity = create_unregistered(contents,
	"SERIAL_NUMBERED_EFFECTIVITY");
    if (!effectivity) {
	omit(record, "unsupported",
	    "the target schema has no SERIAL_NUMBERED_EFFECTIVITY entity",
	    statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(effectivity, "id",
	arguments[0], contents->instance_list) &&
	brlcad::step::SetPart21(effectivity, "effectivity_start_id",
	    arguments[1], contents->instance_list) &&
	brlcad::step::SetPart21(effectivity, "effectivity_end_id",
	    arguments[2], contents->instance_list);
    if (!valid) {
	delete effectivity;
	omit(record, "failed",
	    "the target serial-numbered-effectivity layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(effectivity, completeSE);
    effectivities[std::make_pair(record.schema, record.entity_id)] = effectivity;
    effectivity_types[std::make_pair(record.schema, record.entity_id)] =
	record.type;
    emitted(record, "authored with retained serial bounds", statistics);
    return true;
#endif
}

static bool
emit_effectivity_assignment(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &effectivities,
    const std::map<std::pair<std::string, int64_t>, std::string> &effectivity_types,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    AP203_Contents *contents, ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::vector<int64_t> item_ids;
    std::string error;
    int64_t effectivity_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
	    !part21_reference(arguments.empty() ? std::string() : arguments[0],
		effectivity_id) ||
	    !part21_reference_aggregate(arguments.size() < 2 ? std::string() :
		arguments[1], item_ids)) {
	if (error.empty())
	    error = "APPLIED_EFFECTIVITY_ASSIGNMENT has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    std::vector<int64_t> references(1, effectivity_id);
    references.insert(references.end(), item_ids.begin(), item_ids.end());
    if (record.references != references) {
	omit(record, "malformed",
	    "APPLIED_EFFECTIVITY_ASSIGNMENT references disagree with its "
	    "retained Part 21 value", statistics);
	return false;
    }
#if defined(AP203)
    (void)effectivities;
    (void)effectivity_types;
    (void)products;
    (void)formations;
    (void)definitions;
    (void)usages;
    (void)contents;
    omit(record, "unsupported",
	"AP203 edition 1 has no APPLIED_EFFECTIVITY_ASSIGNMENT entity",
	statistics);
    return false;
#else
    const std::pair<std::string, int64_t> source_key(record.schema,
	effectivity_id);
    const auto effectivity = effectivities.find(source_key);
    const auto source_type = effectivity_types.find(source_key);
    if (effectivity == effectivities.end() || !effectivity->second ||
	    source_type == effectivity_types.end()) {
	omit(record, "unsupported",
	    "the retained effectivity dependency was not emitted", statistics);
	return false;
    }
#if defined(AP214e3)
    if (equal_type(source_type->second, "SERIAL_NUMBERED_EFFECTIVITY") ||
	    equal_type(source_type->second, "LOT_EFFECTIVITY") ||
	    equal_type(source_type->second, "PRODUCT_DEFINITION_EFFECTIVITY")) {
	omit(record, "unsupported",
	    "AP214 forbids serial, lot, and product-definition effectivities "
	    "in APPLIED_EFFECTIVITY_ASSIGNMENT", statistics);
	return false;
    }
#endif
    std::vector<STEPentity *> items;
    for (int64_t item_id : item_ids) {
	if (std::count(item_ids.begin(), item_ids.end(), item_id) != 1) {
	    omit(record, "malformed",
		"an effectivity-assignment item appears more than once",
		statistics);
	    return false;
	}
	STEPentity *item = mapped_configuration_item(item_id, products,
	    formations, definitions, usages);
	if (!item) {
	    omit(record, "unsupported",
		"an effectivity-assignment item has no unambiguous emitted entity",
		statistics);
	    return false;
	}
	items.push_back(item);
    }
    STEPentity *assignment = create_unregistered(contents,
	"APPLIED_EFFECTIVITY_ASSIGNMENT");
    if (!assignment) {
	omit(record, "unsupported",
	    "the target schema has no APPLIED_EFFECTIVITY_ASSIGNMENT entity",
	    statistics);
	return false;
    }
    bool valid = brlcad::step::SetEntity(assignment,
	"assigned_effectivity", effectivity->second);
    for (STEPentity *item : items)
	valid = brlcad::step::AddEntity(assignment, "items", item) && valid;
    if (!valid) {
	delete assignment;
	omit(record, "unsupported",
	    "the retained effectivity items are not legal in the target "
	    "assignment SELECT", statistics);
	return false;
    }
    contents->instance_list->Append(assignment, completeSE);
    emitted(record,
	"authored with remapped effectivity and item references", statistics);
    return true;
#endif
}

static bool
emit_effectivity_relationship(ExportConfigurationRecordPlan &record,
    const std::map<std::pair<std::string, int64_t>, STEPentity *> &effectivities,
    const std::map<std::pair<std::string, int64_t>, std::string> &effectivity_types,
    AP203_Contents *contents, ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t related_id = 0;
    int64_t relating_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 4 ||
	    !part21_string(arguments.empty() ? std::string() : arguments[0]) ||
	    !optional_part21_string(arguments.size() < 2 ? std::string() :
		arguments[1]) ||
	    !part21_reference(arguments.size() < 3 ? std::string() : arguments[2],
		related_id) ||
	    !part21_reference(arguments.size() < 4 ? std::string() : arguments[3],
		relating_id) ||
	    record.references != std::vector<int64_t>({related_id, relating_id})) {
	if (error.empty())
	    error = "EFFECTIVITY_RELATIONSHIP has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
#if defined(AP203)
    if (arguments[1] == "$") {
	omit(record, "unsupported",
	    "AP203 edition 1 requires an effectivity-relationship description",
	    statistics);
	return false;
    }
#endif
    const auto related = effectivities.find(
	std::make_pair(record.schema, related_id));
    const auto relating = effectivities.find(
	std::make_pair(record.schema, relating_id));
    if (related == effectivities.end() || relating == effectivities.end() ||
	    !related->second || !relating->second) {
	omit(record, "unsupported",
	    "an EFFECTIVITY_RELATIONSHIP dependency was not emitted", statistics);
	return false;
    }
#if defined(AP214e3)
    const auto related_type = effectivity_types.find(
	std::make_pair(record.schema, related_id));
    const auto relating_type = effectivity_types.find(
	std::make_pair(record.schema, relating_id));
    const auto restricted = [](const std::string &type) {
	return equal_type(type, "SERIAL_NUMBERED_EFFECTIVITY") ||
	    equal_type(type, "LOT_EFFECTIVITY") ||
	    equal_type(type, "PRODUCT_DEFINITION_EFFECTIVITY");
    };
    if (related_type == effectivity_types.end() ||
	    relating_type == effectivity_types.end()) {
	omit(record, "unsupported",
	    "an effectivity endpoint has no retained subtype", statistics);
	return false;
    }
    if (restricted(related_type->second) || restricted(relating_type->second)) {
	omit(record, "unsupported",
	    "AP214 forbids serial, lot, and product-definition effectivities "
	    "in EFFECTIVITY_RELATIONSHIP", statistics);
	return false;
    }
#else
    (void)effectivity_types;
#endif
    STEPentity *relationship = create_unregistered(contents,
	"EFFECTIVITY_RELATIONSHIP");
    if (!relationship) {
	omit(record, "unsupported",
	    "the target schema has no EFFECTIVITY_RELATIONSHIP entity", statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(relationship, "name",
	arguments[0], contents->instance_list) &&
	brlcad::step::SetPart21(relationship, "description", arguments[1],
	    contents->instance_list) &&
	brlcad::step::SetEntity(relationship, "related_effectivity",
	    related->second) &&
	brlcad::step::SetEntity(relationship, "relating_effectivity",
	    relating->second);
    if (!valid) {
	delete relationship;
	omit(record, "failed",
	    "the target effectivity-relationship layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(relationship, completeSE);
    emitted(record, "authored with remapped effectivity references", statistics);
    return true;
}

static bool
emit_alternate_product(ExportConfigurationRecordPlan &record,
    const std::map<int64_t, STEPentity *> &products, AP203_Contents *contents,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t alternate_id = 0;
    int64_t base_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 5 ||
	    !part21_string(arguments.empty() ? std::string() : arguments[0]) ||
	    !optional_part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
	    !part21_reference(arguments.size() < 3 ? std::string() : arguments[2],
		alternate_id) ||
	    !part21_reference(arguments.size() < 4 ? std::string() : arguments[3],
		base_id) ||
	    !part21_string(arguments.size() < 5 ? std::string() : arguments[4]) ||
	    record.references != std::vector<int64_t>({alternate_id, base_id})) {
	if (error.empty()) error = "ALTERNATE_PRODUCT_RELATIONSHIP has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
#if defined(AP203)
    if (arguments[1] == "$") {
	omit(record, "unsupported",
	    "AP203 edition 1 requires an alternate-product definition", statistics);
	return false;
    }
#endif
    const auto alternate = products.find(alternate_id);
    const auto base = products.find(base_id);
    if (alternate == products.end() || base == products.end() ||
	    !alternate->second || !base->second || alternate->second == base->second) {
	omit(record, "unsupported",
	    "the referenced source products do not map to distinct emitted products",
	    statistics);
	return false;
    }
    STEPentity *relationship = create_unregistered(contents,
	"ALTERNATE_PRODUCT_RELATIONSHIP");
    if (!relationship) {
	omit(record, "unsupported",
	    "the target schema has no ALTERNATE_PRODUCT_RELATIONSHIP entity",
	    statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(relationship, "name", arguments[0],
	contents->instance_list) &&
	brlcad::step::SetPart21(relationship, "definition", arguments[1],
	    contents->instance_list) &&
	brlcad::step::SetEntity(relationship, "alternate", alternate->second) &&
	brlcad::step::SetEntity(relationship, "base", base->second) &&
	brlcad::step::SetPart21(relationship, "basis", arguments[4],
	    contents->instance_list);
    if (!valid) {
	delete relationship;
	omit(record, "failed", "the target alternate-product layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(relationship, completeSE);
    emitted(record, "authored with remapped source-product references", statistics);
    return true;
}

static bool
emit_usage_substitute(ExportConfigurationRecordPlan &record,
    const std::map<int64_t, STEPentity *> &usages, AP203_Contents *contents,
    ConfigurationExportStatistics &statistics)
{
    std::vector<std::string> arguments;
    std::string error;
    int64_t base_id = 0;
    int64_t substitute_id = 0;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 4 ||
	    !part21_string(arguments.empty() ? std::string() : arguments[0]) ||
	    !optional_part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
	    !part21_reference(arguments.size() < 3 ? std::string() : arguments[2],
		base_id) ||
	    !part21_reference(arguments.size() < 4 ? std::string() : arguments[3],
		substitute_id) ||
	    record.references != std::vector<int64_t>({base_id, substitute_id})) {
	if (error.empty()) error = "ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
#if defined(AP203)
    if (arguments[1] == "$") {
	omit(record, "unsupported",
	    "AP203 edition 1 requires a usage-substitute definition", statistics);
	return false;
    }
#endif
    const auto base = usages.find(base_id);
    const auto substitute = usages.find(substitute_id);
    STEPentity *base_parent = base == usages.end() || !base->second ? NULL :
	dynamic_cast<STEPentity *>(brlcad::step::Entity(base->second,
	    "relating_product_definition"));
    STEPentity *substitute_parent = substitute == usages.end() ||
	!substitute->second ? NULL : dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(substitute->second, "relating_product_definition"));
    if (!base_parent || !substitute_parent || base_parent != substitute_parent ||
	    base->second == substitute->second) {
	omit(record, "unsupported",
	    "the source usages do not map to distinct emitted usages under one parent",
	    statistics);
	return false;
    }
    STEPentity *relationship = create_unregistered(contents,
	"ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE");
    if (!relationship) {
	omit(record, "unsupported",
	    "the target schema has no ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE entity",
	    statistics);
	return false;
    }
    const bool valid = brlcad::step::SetPart21(relationship, "name", arguments[0],
	contents->instance_list) &&
	brlcad::step::SetPart21(relationship, "definition", arguments[1],
	    contents->instance_list) &&
	brlcad::step::SetEntity(relationship, "base", base->second) &&
	brlcad::step::SetEntity(relationship, "substitute", substitute->second);
    if (!valid) {
	delete relationship;
	omit(record, "failed", "the target usage-substitute layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(relationship, completeSE);
    emitted(record, "authored with remapped source-usage references", statistics);
    return true;
}

static bool
emit_product_related_category(ExportConfigurationRecordPlan &record,
    const std::map<int64_t, STEPentity *> &products, AP203_Contents *contents,
    ConfigurationExportStatistics &statistics,
    STEPentity **emitted_category = NULL)
{
    std::vector<std::string> arguments;
    std::vector<int64_t> product_ids;
    std::string error;
    if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
	    !part21_string(arguments.empty() ? std::string() : arguments[0]) ||
	    !optional_part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
	    !part21_reference_aggregate(arguments.size() < 3 ? std::string() :
		arguments[2], product_ids) || record.references != product_ids) {
	if (error.empty()) error = "PRODUCT_RELATED_PRODUCT_CATEGORY has an invalid retained layout";
	omit(record, "malformed", error, statistics);
	return false;
    }
    std::vector<STEPentity *> mapped_products;
    for (int64_t source_id : product_ids) {
	const auto mapped = products.find(source_id);
	if (mapped == products.end() || !mapped->second) {
	    omit(record, "unsupported",
		"a categorized source product has no unambiguous emitted product",
		statistics);
	    return false;
	}
	mapped_products.push_back(mapped->second);
    }
    STEPentity *category = create_unregistered(contents,
	"PRODUCT_RELATED_PRODUCT_CATEGORY");
    if (!category) {
	omit(record, "unsupported",
	    "the target schema has no PRODUCT_RELATED_PRODUCT_CATEGORY entity",
	    statistics);
	return false;
    }
    bool valid = brlcad::step::SetPart21(category, "name", arguments[0],
	contents->instance_list) &&
	brlcad::step::SetPart21(category, "description", arguments[1],
	    contents->instance_list);
    for (STEPentity *product : mapped_products)
	valid = brlcad::step::AddEntity(category, "products", product) && valid;
    if (!valid) {
	delete category;
	omit(record, "failed", "the target product-category layout is incompatible",
	    statistics);
	return false;
    }
    contents->instance_list->Append(category, completeSE);
    if (emitted_category) *emitted_category = category;
    emitted(record, "authored with remapped source-product members", statistics);
    return true;
}

static bool
category_path(const ConfigurationIdentityEntities::SourceKey &from,
    const ConfigurationIdentityEntities::SourceKey &target,
    const std::map<ConfigurationIdentityEntities::SourceKey,
	std::vector<ConfigurationIdentityEntities::SourceKey> > &edges,
    std::set<ConfigurationIdentityEntities::SourceKey> &visited)
{
    if (from == target) return true;
    if (!visited.insert(from).second) return false;
    const auto next = edges.find(from);
    if (next == edges.end()) return false;
    for (const ConfigurationIdentityEntities::SourceKey &candidate :
	    next->second) {
	if (category_path(candidate, target, edges, visited)) return true;
    }
    return false;
}

static void
emit_product_category_graph(StepExportPlan &plan,
    const std::map<int64_t, STEPentity *> &products, AP203_Contents *contents,
    ConfigurationExportStatistics &statistics)
{
    typedef ConfigurationIdentityEntities::SourceKey SourceKey;
    std::map<SourceKey, STEPentity *> categories;

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "PRODUCT_CATEGORY")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!optional_part21_string(arguments.size() < 2 ? std::string() :
		    arguments[1]) || !record.references.empty()) {
	    if (error.empty()) error =
		"PRODUCT_CATEGORY has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	STEPentity *category = create_unregistered(contents, "PRODUCT_CATEGORY");
	if (!category) {
	    omit(record, "unsupported",
		"the target schema has no PRODUCT_CATEGORY entity", statistics);
	    continue;
	}
	const bool valid = brlcad::step::SetPart21(category, "name", arguments[0],
	    contents->instance_list) && brlcad::step::SetPart21(category,
		"description", arguments[1], contents->instance_list);
	if (!valid) {
	    delete category;
	    omit(record, "failed", "the target product-category layout is incompatible",
		statistics);
	    continue;
	}
	contents->instance_list->Append(category, completeSE);
	categories[SourceKey(record.schema, record.entity_id)] = category;
	emitted(record, "authored as a retained product-category identity",
	    statistics);
    }

    /* PRODUCT_RELATED_PRODUCT_CATEGORY is also a PRODUCT_CATEGORY subtype
     * and can therefore be either endpoint of a retained hierarchy edge.
     * Emit product membership before resolving those edges and retain the
     * resulting subtype entity in the same category map. */
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "PRODUCT_RELATED_PRODUCT_CATEGORY"))
	    continue;
	STEPentity *category = NULL;
	if (emit_product_related_category(record, products, contents, statistics,
		&category) && category)
	    categories[SourceKey(record.schema, record.entity_id)] = category;
    }

    struct RelationshipCandidate {
	ExportConfigurationRecordPlan *record = NULL;
	std::vector<std::string> arguments;
	SourceKey category;
	SourceKey sub_category;
    };
    std::vector<RelationshipCandidate> candidates;
    std::map<SourceKey, std::vector<SourceKey> > edges;
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "PRODUCT_CATEGORY_RELATIONSHIP")) continue;
	std::vector<std::string> arguments;
	std::string error;
	int64_t category_id = 0;
	int64_t sub_category_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 4 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!optional_part21_string(arguments.size() < 2 ? std::string() :
		    arguments[1]) ||
		!part21_reference(arguments.size() < 3 ? std::string() :
		    arguments[2], category_id) ||
		!part21_reference(arguments.size() < 4 ? std::string() :
		    arguments[3], sub_category_id) ||
		record.references !=
		    std::vector<int64_t>({category_id, sub_category_id})) {
	    if (error.empty()) error =
		"PRODUCT_CATEGORY_RELATIONSHIP has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
#if defined(AP203)
	if (arguments[1] == "$") {
	    omit(record, "unsupported",
		"AP203 edition 1 requires a product-category relationship description",
		statistics);
	    continue;
	}
#endif
	RelationshipCandidate candidate;
	candidate.record = &record;
	candidate.arguments = arguments;
	candidate.category = SourceKey(record.schema, category_id);
	candidate.sub_category = SourceKey(record.schema, sub_category_id);
	candidates.push_back(candidate);
	edges[candidate.category].push_back(candidate.sub_category);
    }

    for (RelationshipCandidate &candidate : candidates) {
	const auto category = categories.find(candidate.category);
	const auto sub_category = categories.find(candidate.sub_category);
	if (category == categories.end() || sub_category == categories.end() ||
		!category->second || !sub_category->second) {
	    omit(*candidate.record, "unsupported",
		"a retained product-category relationship endpoint was not emitted",
		statistics);
	    continue;
	}
	std::set<SourceKey> visited;
	if (category_path(candidate.sub_category, candidate.category, edges,
		visited)) {
	    omit(*candidate.record, "malformed",
		"the retained product-category relationships contain a cycle",
		statistics);
	    continue;
	}
	STEPentity *relationship = create_unregistered(contents,
	    "PRODUCT_CATEGORY_RELATIONSHIP");
	if (!relationship) {
	    omit(*candidate.record, "unsupported",
		"the target schema has no PRODUCT_CATEGORY_RELATIONSHIP entity",
		statistics);
	    continue;
	}
	const bool valid = brlcad::step::SetPart21(relationship, "name",
	    candidate.arguments[0], contents->instance_list) &&
	    brlcad::step::SetPart21(relationship, "description",
		candidate.arguments[1], contents->instance_list) &&
	    brlcad::step::SetEntity(relationship, "category", category->second) &&
	    brlcad::step::SetEntity(relationship, "sub_category",
		sub_category->second);
	if (!valid) {
	    delete relationship;
	    omit(*candidate.record, "failed",
		"the target product-category relationship layout is incompatible",
		statistics);
	    continue;
	}
	contents->instance_list->Append(relationship, completeSE);
	emitted(*candidate.record,
	    "authored with remapped product-category references", statistics);
    }
}

} // namespace

brlcad::step::ConfigurationExportStatistics
brlcad::step::EmitSTEPConfigurationRecords(StepExportPlan &plan,
    AP203_Contents *contents, std::vector<std::string> &diagnostics)
{
    ConfigurationExportStatistics statistics;
    std::map<std::pair<std::string, int64_t>, STEPentity *> statuses;
    std::map<std::pair<std::string, int64_t>, STEPentity *> approvals;
    std::map<std::pair<std::string, int64_t>, STEPentity *> effectivities;
    std::map<std::pair<std::string, int64_t>, std::string> effectivity_types;
    std::map<std::pair<std::string, int64_t>, STEPentity *> dates;
    std::map<std::pair<std::string, int64_t>, STEPentity *> offsets;
    std::map<std::pair<std::string, int64_t>, STEPentity *> times;
    std::map<std::pair<std::string, int64_t>, STEPentity *> date_times;
    std::map<std::pair<std::string, int64_t>, STEPentity *> dimensions;
    std::map<std::pair<std::string, int64_t>, STEPentity *> units;
    std::map<std::pair<std::string, int64_t>, STEPentity *> measures;
    std::map<int64_t, STEPentity *> products;
    std::map<int64_t, STEPentity *> formations;
    std::map<int64_t, STEPentity *> definitions;
    std::map<int64_t, STEPentity *> usages;
    source_entity_maps(plan, contents, products, formations, definitions,
	usages);

    const ConfigurationIdentityEntities identities =
	EmitSTEPConfigurationIdentities(plan, contents, products, formations,
	    definitions, usages, statistics);

    emit_product_category_graph(plan, products, contents, statistics);

#if defined(AP203)
    /* APPROVAL is not independently instantiable in AP203 edition 1: every
     * approval must have at least one approval assignment.  Preflight the
     * retained assignments before allocating either the approval or its
     * status, so incomplete retained fragments cannot make otherwise valid
     * AP203 output fail a global rule. */
    std::set<std::pair<std::string, int64_t> > assigned_approval_keys;
    std::set<std::pair<std::string, int64_t> > assigned_status_keys;
    for (const ExportConfigurationRecordPlan &record :
	    plan.configuration_records) {
	if (!record.valid || (!equal_type(record.type, "CC_DESIGN_APPROVAL") &&
		!equal_type(record.type, "APPLIED_APPROVAL_ASSIGNMENT")))
	    continue;
	std::vector<std::string> arguments;
	std::vector<int64_t> item_ids;
	std::string error;
	int64_t approval_id = 0;
	if (!part21_arguments(record, arguments, error) ||
		arguments.size() != 2 || !part21_reference(arguments[0],
		    approval_id) || !part21_reference_aggregate(arguments[1],
		    item_ids)) continue;
	std::vector<int64_t> references(1, approval_id);
	references.insert(references.end(), item_ids.begin(), item_ids.end());
	if (record.references != references) continue;
	bool mappable = true;
	for (int64_t item_id : item_ids) {
	    const auto formation = formations.find(item_id);
	    const auto definition = definitions.find(item_id);
	    const auto classification = identities.classifications.find(
		ConfigurationIdentityEntities::SourceKey(record.schema, item_id));
	    const bool valid_item =
		(formation != formations.end() && formation->second) ||
		(definition != definitions.end() && definition->second) ||
		(classification != identities.classifications.end() &&
		 classification->second);
	    if (!valid_item || std::count(item_ids.begin(), item_ids.end(),
		    item_id) != 1) {
		mappable = false;
		break;
	    }
	}
	if (mappable)
	    assigned_approval_keys.insert(std::make_pair(record.schema,
		approval_id));
    }
    for (const ExportConfigurationRecordPlan &record :
	    plan.configuration_records) {
	const std::pair<std::string, int64_t> key(record.schema,
	    record.entity_id);
	if (!record.valid || !equal_type(record.type, "APPROVAL") ||
		assigned_approval_keys.find(key) == assigned_approval_keys.end())
	    continue;
	std::vector<std::string> arguments;
	std::string error;
	int64_t status_id = 0;
	if (part21_arguments(record, arguments, error) && arguments.size() == 2 &&
		part21_reference(arguments[0], status_id))
	    assigned_status_keys.insert(std::make_pair(record.schema, status_id));
    }
#endif

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (record.valid && equal_type(record.type, "DIMENSIONAL_EXPONENTS"))
	    emit_dimensional_exponents(record, contents, dimensions, statistics);
    }
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (record.valid && equal_type(record.type, "CONTEXT_DEPENDENT_UNIT"))
	    emit_context_dependent_unit(record, dimensions, contents, units,
		statistics);
    }
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (record.valid && equal_type(record.type, "MEASURE_WITH_UNIT"))
	    emit_measure_with_unit(record, units, contents, measures, statistics);
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid) continue;
	if (equal_type(record.type, "CALENDAR_DATE")) {
	    emit_calendar_date(record, contents, dates, statistics);
	} else if (equal_type(record.type,
		"COORDINATED_UNIVERSAL_TIME_OFFSET")) {
	    emit_time_offset(record, contents, offsets, statistics);
	}
    }
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (record.valid && equal_type(record.type, "LOCAL_TIME"))
	    emit_local_time(record, offsets, contents, times, statistics);
    }
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (record.valid && equal_type(record.type, "DATE_AND_TIME"))
	    emit_date_and_time(record, dates, times, contents, date_times,
		statistics);
    }

    emit_date_time_assignments(plan, contents, products, formations,
	definitions, usages, identities, dates, times, date_times, statistics);

#if defined(AP203)
    brlcad::step::EmitAP203ComplexConfiguration(plan, contents, formations,
	usages, date_times, measures, effectivities, effectivity_types,
	statistics);
#endif

    /* APPROVAL_STATUS has no entity references, so it can be validated and
     * appended before any dependent APPROVAL.  This is the first complete,
     * schema-identical configuration subgraph shared by all supported APs. */
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid) {
	    omit(record, "malformed", record.error, statistics);
	    continue;
	}
	if (!equal_type(record.type, "APPROVAL_STATUS")) continue;
#if defined(AP203)
	if (assigned_status_keys.find(std::make_pair(record.schema,
		record.entity_id)) == assigned_status_keys.end()) {
	    omit(record, "unsupported",
		"AP203 APPROVAL_STATUS has no approval with an authorable "
		"CC_DESIGN_APPROVAL assignment", statistics);
	    continue;
	}
#endif
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 1 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!record.references.empty()) {
	    if (error.empty()) error = "APPROVAL_STATUS requires one label and no references";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	STEPentity *status = create_unregistered(contents, "APPROVAL_STATUS");
	if (!status) {
	    omit(record, "unsupported", "the target schema has no APPROVAL_STATUS entity",
		statistics);
	    continue;
	}
	if (!brlcad::step::SetString(status, "name", arguments[0].c_str())) {
	    delete status;
	    omit(record, "failed", "the target APPROVAL_STATUS layout is incompatible",
		statistics);
	    continue;
	}
	contents->instance_list->Append(status, completeSE);
	statuses[std::make_pair(record.schema, record.entity_id)] = status;
	emitted(record, "authored as a schema-common approval status", statistics);
    }

    /* Resolve approvals in a separate pass so assignment instance ordering in
     * the retained graph does not affect whether a dependency can be mapped. */
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || equal_type(record.type, "APPROVAL_STATUS")) continue;
	if (!equal_type(record.type, "APPROVAL")) continue;
#if defined(AP203)
	if (assigned_approval_keys.find(std::make_pair(record.schema,
		record.entity_id)) == assigned_approval_keys.end()) {
	    omit(record, "unsupported",
		"AP203 APPROVAL has no authorable CC_DESIGN_APPROVAL assignment",
		statistics);
	    continue;
	}
#endif
	std::vector<std::string> arguments;
	std::string error;
	int64_t status_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
		!part21_reference(arguments.empty() ? std::string() : arguments[0],
		    status_id) ||
		!part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
		record.references.size() != 1 || record.references[0] != status_id) {
	    if (error.empty())
		error = "APPROVAL requires one retained APPROVAL_STATUS reference and one label";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	const auto mapped = statuses.find(std::make_pair(record.schema, status_id));
	if (mapped == statuses.end()) {
	    omit(record, "unsupported",
		"the retained APPROVAL_STATUS dependency was not emitted", statistics);
	    continue;
	}
	STEPentity *approval = create_unregistered(contents, "APPROVAL");
	if (!approval) {
	    omit(record, "unsupported", "the target schema has no APPROVAL entity",
		statistics);
	    continue;
	}
	if (!brlcad::step::SetEntity(approval, "status", mapped->second) ||
		!brlcad::step::SetString(approval, "level", arguments[1].c_str())) {
	    delete approval;
	    omit(record, "failed", "the target APPROVAL layout is incompatible",
		statistics);
	    continue;
	}
	contents->instance_list->Append(approval, completeSE);
	approvals[std::make_pair(record.schema, record.entity_id)] = approval;
	emitted(record, "authored with a remapped APPROVAL_STATUS reference",
	    statistics);
    }

    emit_approval_provenance(plan, contents, identities, approvals, dates,
	times, date_times, statistics);

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid ||
		!equal_type(record.type, "PRODUCT_DEFINITION_EFFECTIVITY"))
	    continue;
	emit_product_definition_effectivity(record, usages, contents,
	    effectivities, effectivity_types, statistics);
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid ||
		!equal_type(record.type, "SERIAL_NUMBERED_EFFECTIVITY"))
	    continue;
	emit_serial_numbered_effectivity(record, contents, effectivities,
	    effectivity_types, statistics);
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || !equal_type(record.type, "DATED_EFFECTIVITY"))
	    continue;
	emit_dated_effectivity(record, date_times, contents, effectivities,
	    effectivity_types, statistics);
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || !equal_type(record.type, "LOT_EFFECTIVITY"))
	    continue;
	emit_lot_effectivity(record, measures, contents, effectivities,
	    effectivity_types, statistics);
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || equal_type(record.type, "APPROVAL_STATUS") ||
		equal_type(record.type, "APPROVAL") ||
		equal_type(record.type, "PRODUCT_DEFINITION_EFFECTIVITY") ||
		equal_type(record.type, "SERIAL_NUMBERED_EFFECTIVITY") ||
		equal_type(record.type, "DATED_EFFECTIVITY") ||
		equal_type(record.type, "LOT_EFFECTIVITY") ||
		equal_type(record.type, "CALENDAR_DATE") ||
		equal_type(record.type, "COORDINATED_UNIVERSAL_TIME_OFFSET") ||
		equal_type(record.type, "LOCAL_TIME") ||
		equal_type(record.type, "DATE_AND_TIME") ||
		equal_type(record.type, "DIMENSIONAL_EXPONENTS") ||
		equal_type(record.type, "CONTEXT_DEPENDENT_UNIT") ||
		equal_type(record.type, "MEASURE_WITH_UNIT")) continue;
	if (record.export_status != "pending") continue;
	if (equal_type(record.type, "ALTERNATE_PRODUCT_RELATIONSHIP")) {
	    emit_alternate_product(record, products, contents, statistics);
	    continue;
	}
	if (equal_type(record.type, "ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE")) {
	    emit_usage_substitute(record, usages, contents, statistics);
	    continue;
	}
	if (equal_type(record.type, "PRODUCT_RELATED_PRODUCT_CATEGORY")) {
	    emit_product_related_category(record, products, contents, statistics);
	    continue;
	}
	if (equal_type(record.type, "CC_DESIGN_APPROVAL") ||
		equal_type(record.type, "APPLIED_APPROVAL_ASSIGNMENT")) {
	    emit_approval_assignment(record, approvals, products, formations,
		definitions, usages, identities, contents, statistics);
	    continue;
	}
	if (equal_type(record.type, "APPROVAL_RELATIONSHIP")) {
	    emit_approval_relationship(record, approvals, contents, statistics);
	    continue;
	}
	if (equal_type(record.type, "EFFECTIVITY_RELATIONSHIP")) {
	    emit_effectivity_relationship(record, effectivities, effectivity_types,
		contents, statistics);
	    continue;
	}
	if (equal_type(record.type, "APPLIED_EFFECTIVITY_ASSIGNMENT")) {
	    emit_effectivity_assignment(record, effectivities, effectivity_types,
		products, formations, definitions, usages, contents, statistics);
	    continue;
	}
	omit(record, "unsupported",
	    "no lossless schema-aware authoring path is enabled for this entity type",
	    statistics);
    }

    if (statistics.omitted) {
	diagnostics.push_back(std::to_string(statistics.omitted) + " of " +
	    std::to_string(plan.configuration_records.size()) +
	    " retained STEP configuration record(s) have no enabled lossless "
	    "authoring path");
    }
    return statistics;
}
