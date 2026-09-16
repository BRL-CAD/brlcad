/* S T E P C O N F I G U R A T I O N I D E N T I T Y E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "STEPConfigurationIdentityExport.h"

#include "AP_Common.h"
#include "STEPConfigurationExport.h"
#include "STEPExportContext.h"
#include "STEPGeneratedAPI.h"
#include "StepExportPlan.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace step_configuration_identity {

using brlcad::step::ConfigurationExportStatistics;
using brlcad::step::ExportConfigurationRecordPlan;
using brlcad::step::StepExportPlan;

typedef std::pair<std::string, int64_t> SourceKey;

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
optional_part21_string(const std::string &value)
{
    return value == "$" || part21_string(value);
}

static bool
optional_part21_string_list(const std::string &value)
{
    if (value == "$") return true;
    const std::string aggregate = trim(value);
    if (aggregate.size() < 4 || aggregate.front() != '(' ||
	    aggregate.back() != ')') return false;
    const std::string body = aggregate.substr(1, aggregate.size() - 2);
    size_t begin = 0;
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
	if (i != body.size() && (quoted || body[i] != ',')) continue;
	if (!part21_string(trim(body.substr(begin, i - begin)))) return false;
	begin = i + 1;
    }
    return !quoted;
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

static STEPentity *
create_unregistered(AP203_Contents *contents, const char *type)
{
    STEPentity *entity = contents && contents->registry ?
	contents->registry->ObjCreate(type) : NULL;
    return entity && !isNilSTEPentity(entity) ? entity : NULL;
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
    record.export_status = "emitted";
    record.export_reason = reason;
    ++statistics.emitted;
}

#if defined(AP203)
static bool
ap203_security_level(const std::string &value)
{
    static const std::set<std::string> allowed = {
	"'unclassified'", "'classified'", "'proprietary'",
	"'confidential'", "'secret'", "'top_secret'"
    };
    return allowed.find(value) != allowed.end();
}
#endif

#if defined(AP203)
static bool
ap203_document_type(const std::string &value)
{
    static const std::set<std::string> allowed = {
	"'material_specification'", "'process_specification'",
	"'design_specification'", "'surface_finish_specification'",
	"'cad_filename'", "'drawing'"
    };
    return allowed.find(value) != allowed.end();
}
#endif

enum class ItemKind {
    None,
    Product,
    Formation,
    Definition,
    Usage,
    Classification
};

struct MappedItem {
    STEPentity *entity = NULL;
    ItemKind kind = ItemKind::None;
    bool ambiguous = false;
};

static void
consider_item(MappedItem &result, STEPentity *entity, ItemKind kind)
{
    if (!entity || result.ambiguous) return;
    if (result.entity && result.entity != entity) {
	result.entity = NULL;
	result.kind = ItemKind::None;
	result.ambiguous = true;
	return;
    }
    result.entity = entity;
    result.kind = kind;
}

static MappedItem
mapped_item(int64_t source_id, const std::string &schema,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    const std::map<SourceKey, STEPentity *> &classifications)
{
    MappedItem result;
    const auto product = products.find(source_id);
    if (product != products.end())
	consider_item(result, product->second, ItemKind::Product);
    const auto formation = formations.find(source_id);
    if (formation != formations.end())
	consider_item(result, formation->second, ItemKind::Formation);
    const auto definition = definitions.find(source_id);
    if (definition != definitions.end())
	consider_item(result, definition->second, ItemKind::Definition);
    const auto usage = usages.find(source_id);
    if (usage != usages.end())
	consider_item(result, usage->second, ItemKind::Usage);
    const auto classification = classifications.find(SourceKey(schema,
	source_id));
    if (classification != classifications.end())
	consider_item(result, classification->second, ItemKind::Classification);
    return result;
}

#if defined(AP203)
static bool
ap203_person_role(const std::string &role)
{
    static const std::set<std::string> allowed = {
	"'request_recipient'", "'initiator'", "'part_supplier'",
	"'design_supplier'", "'configuration_manager'", "'contractor'",
	"'classification_officer'", "'creator'", "'design_owner'"
    };
    return allowed.find(role) != allowed.end();
}

static bool
ap203_person_role_item(const std::string &role, ItemKind kind)
{
    if (role == "'design_owner'") return kind == ItemKind::Product;
    if (role == "'creator'")
	return kind == ItemKind::Formation || kind == ItemKind::Definition;
    if (role == "'design_supplier'" || role == "'part_supplier'")
	return kind == ItemKind::Formation;
    if (role == "'classification_officer'")
	return kind == ItemKind::Classification;
    return false;
}
#endif

static void
emit_people_and_organizations(StepExportPlan &plan, AP203_Contents *contents,
    std::map<SourceKey, STEPentity *> &organizations,
    std::map<SourceKey, STEPentity *> &people,
    std::map<SourceKey, STEPentity *> &person_organizations,
    ConfigurationExportStatistics &statistics)
{
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "ORGANIZATION")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
		!optional_part21_string(arguments.empty() ? std::string() :
		    arguments[0]) ||
		!part21_string(arguments.size() < 2 ? std::string() :
		    arguments[1]) ||
		!optional_part21_string(arguments.size() < 3 ? std::string() :
		    arguments[2]) || !record.references.empty()) {
	    if (error.empty()) error = "ORGANIZATION has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
#if defined(AP203)
	if (arguments[2] == "$") {
	    omit(record, "unsupported",
		"AP203 edition 1 requires an organization description",
		statistics);
	    continue;
	}
#endif
	STEPentity *organization = create_unregistered(contents, "ORGANIZATION");
	if (!organization) {
	    omit(record, "unsupported",
		"the target schema has no ORGANIZATION entity", statistics);
	    continue;
	}
	const bool valid = brlcad::step::SetPart21(organization, "id",
	    arguments[0], contents->instance_list) &&
	    brlcad::step::SetPart21(organization, "name", arguments[1],
		contents->instance_list) &&
	    brlcad::step::SetPart21(organization, "description", arguments[2],
		contents->instance_list);
	if (!valid) {
	    delete organization;
	    omit(record, "failed",
		"the target organization layout is incompatible", statistics);
	    continue;
	}
	contents->instance_list->Append(organization, completeSE);
	organizations[SourceKey(record.schema, record.entity_id)] = organization;
	emitted(record, "authored as a retained organization identity", statistics);
    }

    std::map<std::string, size_t> person_id_counts;
#if defined(AP203)
    for (const ExportConfigurationRecordPlan &record :
	    plan.configuration_records) {
	if (!record.valid || !equal_type(record.type, "PERSON")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (part21_arguments(record, arguments, error) && arguments.size() == 6 &&
		part21_string(arguments[0]) &&
		optional_part21_string(arguments[1]) &&
		optional_part21_string(arguments[2]) &&
		optional_part21_string_list(arguments[3]) &&
		optional_part21_string_list(arguments[4]) &&
		optional_part21_string_list(arguments[5]) &&
		(arguments[1] != "$" || arguments[2] != "$") &&
		record.references.empty()) ++person_id_counts[arguments[0]];
    }
#endif
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "PERSON")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 6 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!optional_part21_string(arguments.size() < 2 ? std::string() :
		    arguments[1]) ||
		!optional_part21_string(arguments.size() < 3 ? std::string() :
		    arguments[2]) ||
		!optional_part21_string_list(arguments.size() < 4 ? std::string() :
		    arguments[3]) ||
		!optional_part21_string_list(arguments.size() < 5 ? std::string() :
		    arguments[4]) ||
		!optional_part21_string_list(arguments.size() < 6 ? std::string() :
		    arguments[5]) ||
		(arguments.size() >= 3 && arguments[1] == "$" &&
		 arguments[2] == "$") || !record.references.empty()) {
	    if (error.empty()) error = "PERSON has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
#if defined(AP203)
	if (person_id_counts[arguments[0]] != 1) {
	    omit(record, "unsupported",
		"AP203 edition 1 requires every person identifier to be unique",
		statistics);
	    continue;
	}
#endif
	STEPentity *person = create_unregistered(contents, "PERSON");
	if (!person) {
	    omit(record, "unsupported", "the target schema has no PERSON entity",
		statistics);
	    continue;
	}
	static const char *const attributes[] = {
	    "id", "last_name", "first_name", "middle_names",
	    "prefix_titles", "suffix_titles"
	};
	bool valid = true;
	for (size_t i = 0; i < 6; ++i)
	    valid = brlcad::step::SetPart21(person, attributes[i], arguments[i],
		contents->instance_list) && valid;
	if (!valid) {
	    delete person;
	    omit(record, "failed", "the target person layout is incompatible",
		statistics);
	    continue;
	}
	contents->instance_list->Append(person, completeSE);
	people[SourceKey(record.schema, record.entity_id)] = person;
	emitted(record, "authored as a retained person identity", statistics);
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "PERSON_AND_ORGANIZATION")) continue;
	std::vector<std::string> arguments;
	std::string error;
	int64_t person_id = 0;
	int64_t organization_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
		!part21_reference(arguments.empty() ? std::string() : arguments[0],
		    person_id) ||
		!part21_reference(arguments.size() < 2 ? std::string() :
		    arguments[1], organization_id) ||
		record.references !=
		    std::vector<int64_t>({person_id, organization_id})) {
	    if (error.empty())
		error = "PERSON_AND_ORGANIZATION has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	const auto person = people.find(SourceKey(record.schema, person_id));
	const auto organization = organizations.find(
	    SourceKey(record.schema, organization_id));
	if (person == people.end() || organization == organizations.end() ||
		!person->second || !organization->second) {
	    omit(record, "unsupported",
		"a retained person-and-organization dependency was not emitted",
		statistics);
	    continue;
	}
	STEPentity *identity = create_unregistered(contents,
	    "PERSON_AND_ORGANIZATION");
	if (!identity) {
	    omit(record, "unsupported",
		"the target schema has no PERSON_AND_ORGANIZATION entity",
		statistics);
	    continue;
	}
	const bool valid = brlcad::step::SetEntity(identity, "the_person",
	    person->second) && brlcad::step::SetEntity(identity,
		"the_organization", organization->second);
	if (!valid) {
	    delete identity;
	    omit(record, "failed",
		"the target person-and-organization layout is incompatible",
		statistics);
	    continue;
	}
	contents->instance_list->Append(identity, completeSE);
	person_organizations[SourceKey(record.schema, record.entity_id)] = identity;
	emitted(record, "authored with remapped person and organization references",
	    statistics);
    }
}

static bool
valid_address_fields(const std::vector<std::string> &arguments)
{
    if (arguments.size() < 12) return false;
    bool populated = false;
    for (size_t i = 0; i < 12; ++i) {
	if (!optional_part21_string(arguments[i])) return false;
	if (arguments[i] != "$") populated = true;
    }
    return populated;
}

static bool
set_address_fields(STEPentity *address,
    const std::vector<std::string> &arguments, AP203_Contents *contents)
{
    static const char *const attributes[] = {
	"internal_location", "street_number", "street", "postal_box", "town",
	"region", "postal_code", "country", "facsimile_number",
	"telephone_number", "electronic_mail_address", "telex_number"
    };
    bool valid = address && contents;
    for (size_t i = 0; i < 12; ++i)
	valid = brlcad::step::SetPart21(address, attributes[i], arguments[i],
	    contents->instance_list) && valid;
    return valid;
}

static void
emit_addresses(StepExportPlan &plan, AP203_Contents *contents,
    const std::map<SourceKey, STEPentity *> &organizations,
    const std::map<SourceKey, STEPentity *> &people,
    ConfigurationExportStatistics &statistics)
{
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "ADDRESS")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) ||
		arguments.size() != 12 || !valid_address_fields(arguments) ||
		!record.references.empty()) {
	    if (error.empty()) error = "ADDRESS has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	STEPentity *address = create_unregistered(contents, "ADDRESS");
	if (!address) {
	    omit(record, "unsupported", "the target schema has no ADDRESS entity",
		statistics);
	    continue;
	}
	if (!set_address_fields(address, arguments, contents)) {
	    delete address;
	    omit(record, "failed", "the target address layout is incompatible",
		statistics);
	    continue;
	}
	contents->instance_list->Append(address, completeSE);
	emitted(record, "authored as a retained address", statistics);
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	const bool personal = equal_type(record.type, "PERSONAL_ADDRESS");
	const bool organizational = equal_type(record.type,
	    "ORGANIZATIONAL_ADDRESS");
	if (!record.valid || record.export_status != "pending" ||
		(!personal && !organizational)) continue;
	std::vector<std::string> arguments;
	std::vector<int64_t> references;
	std::string error;
	if (!part21_arguments(record, arguments, error) ||
		arguments.size() != 14 || !valid_address_fields(arguments) ||
		!part21_reference_aggregate(arguments.size() < 13 ?
		    std::string() : arguments[12], references) ||
		!optional_part21_string(arguments.size() < 14 ?
		    std::string() : arguments[13]) || record.references != references ||
		std::set<int64_t>(references.begin(), references.end()).size() !=
		    references.size()) {
	    if (error.empty()) error = std::string(personal ? "PERSONAL_ADDRESS" :
		"ORGANIZATIONAL_ADDRESS") + " has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
#if defined(AP203)
	if (arguments[13] == "$") {
	    omit(record, "unsupported", std::string("AP203 edition 1 requires ") +
		(personal ? "a personal" : "an organizational") +
		" address description", statistics);
	    continue;
	}
#endif
	std::vector<STEPentity *> owners;
	bool mapped = true;
	for (int64_t reference : references) {
	    const SourceKey key(record.schema, reference);
	    if (personal) {
		const auto owner = people.find(key);
		if (owner == people.end() || !owner->second) {
		    mapped = false;
		    break;
		}
		owners.push_back(owner->second);
	    } else {
		const auto owner = organizations.find(key);
		if (owner == organizations.end() || !owner->second) {
		    mapped = false;
		    break;
		}
		owners.push_back(owner->second);
	    }
	}
	if (!mapped) {
	    omit(record, "unsupported", std::string("a retained ") +
		(personal ? "personal-address person" :
		 "organizational-address organization") +
		" dependency was not emitted", statistics);
	    continue;
	}
	const char *type = personal ? "PERSONAL_ADDRESS" :
	    "ORGANIZATIONAL_ADDRESS";
	STEPentity *address = create_unregistered(contents, type);
	if (!address) {
	    omit(record, "unsupported", std::string("the target schema has no ") +
		type + " entity", statistics);
	    continue;
	}
	bool valid = set_address_fields(address, arguments, contents);
	const char *owner_attribute = personal ? "people" : "organizations";
	for (STEPentity *owner : owners)
	    valid = brlcad::step::AddEntity(address, owner_attribute, owner) && valid;
	valid = brlcad::step::SetPart21(address, "description", arguments[13],
	    contents->instance_list) && valid;
	if (!valid) {
	    delete address;
	    omit(record, "failed", std::string("the target ") +
		(personal ? "personal-address" : "organizational-address") +
		" layout is incompatible", statistics);
	    continue;
	}
	contents->instance_list->Append(address, completeSE);
	emitted(record, std::string("authored with remapped ") +
	    (personal ? "person" : "organization") + " references", statistics);
    }
}

static void
emit_security_identities(StepExportPlan &plan, AP203_Contents *contents,
    std::map<SourceKey, STEPentity *> &classifications,
    ConfigurationExportStatistics &statistics)
{
    std::map<SourceKey, STEPentity *> levels;
    std::map<SourceKey, ExportConfigurationRecordPlan *> level_records;
    std::set<SourceKey> used_levels;

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "SECURITY_CLASSIFICATION_LEVEL")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 1 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!record.references.empty()) {
	    if (error.empty()) error =
		"SECURITY_CLASSIFICATION_LEVEL has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
#if defined(AP203)
	if (!ap203_security_level(arguments[0])) {
	    omit(record, "unsupported",
		"AP203 edition 1 forbids the retained security level name",
		statistics);
	    continue;
	}
#endif
	STEPentity *level = create_unregistered(contents,
	    "SECURITY_CLASSIFICATION_LEVEL");
	if (!level) {
	    omit(record, "unsupported",
		"the target schema has no SECURITY_CLASSIFICATION_LEVEL entity",
		statistics);
	    continue;
	}
	if (!brlcad::step::SetPart21(level, "name", arguments[0],
		contents->instance_list)) {
	    delete level;
	    omit(record, "failed",
		"the target security-level layout is incompatible", statistics);
	    continue;
	}
	const SourceKey key(record.schema, record.entity_id);
	levels[key] = level;
	level_records[key] = &record;
    }

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "SECURITY_CLASSIFICATION")) continue;
	std::vector<std::string> arguments;
	std::string error;
	int64_t level_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
		!part21_reference(arguments.size() < 3 ? std::string() :
		    arguments[2], level_id) ||
		record.references != std::vector<int64_t>({level_id})) {
	    if (error.empty())
		error = "SECURITY_CLASSIFICATION has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	const SourceKey level_key(record.schema, level_id);
	const auto level = levels.find(level_key);
	if (level == levels.end() || !level->second) {
	    omit(record, "unsupported",
		"the retained security-level dependency was not emitted",
		statistics);
	    continue;
	}
	STEPentity *classification = create_unregistered(contents,
	    "SECURITY_CLASSIFICATION");
	if (!classification) {
	    omit(record, "unsupported",
		"the target schema has no SECURITY_CLASSIFICATION entity",
		statistics);
	    continue;
	}
	const bool valid = brlcad::step::SetPart21(classification, "name",
	    arguments[0], contents->instance_list) &&
	    brlcad::step::SetPart21(classification, "purpose", arguments[1],
		contents->instance_list) &&
	    brlcad::step::SetEntity(classification, "security_level",
		level->second);
	if (!valid) {
	    delete classification;
	    omit(record, "failed",
		"the target security-classification layout is incompatible",
		statistics);
	    continue;
	}
	contents->instance_list->Append(classification, completeSE);
	classifications[SourceKey(record.schema, record.entity_id)] = classification;
	used_levels.insert(level_key);
	emitted(record, "authored with a remapped security-level reference",
	    statistics);
    }

    for (const auto &entry : level_records) {
	STEPentity *level = levels[entry.first];
#if defined(AP203)
	if (used_levels.find(entry.first) == used_levels.end()) {
	    delete level;
	    omit(*entry.second, "unsupported",
		"AP203 security levels must be used by an emitted classification",
		statistics);
	    continue;
	}
#endif
	contents->instance_list->Append(level, completeSE);
	emitted(*entry.second, "authored as a retained security level", statistics);
    }
}

static void
emit_document_identities(StepExportPlan &plan, AP203_Contents *contents,
    std::map<SourceKey, STEPentity *> &documents,
    ConfigurationExportStatistics &statistics)
{
    std::map<SourceKey, STEPentity *> types;
    std::map<SourceKey, ExportConfigurationRecordPlan *> type_records;
    std::set<SourceKey> used_types;

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "DOCUMENT_TYPE")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 1 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!record.references.empty()) {
	    if (error.empty()) error = "DOCUMENT_TYPE has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
#if defined(AP203)
	if (!ap203_document_type(arguments[0])) {
	    omit(record, "unsupported",
		"AP203 edition 1 forbids the retained document type name",
		statistics);
	    continue;
	}
#endif
	STEPentity *type = create_unregistered(contents, "DOCUMENT_TYPE");
	if (!type) {
	    omit(record, "unsupported",
		"the target schema has no DOCUMENT_TYPE entity", statistics);
	    continue;
	}
	if (!brlcad::step::SetPart21(type, "product_data_type", arguments[0],
		contents->instance_list)) {
	    delete type;
	    omit(record, "failed", "the target document-type layout is incompatible",
		statistics);
	    continue;
	}
	const SourceKey key(record.schema, record.entity_id);
	types[key] = type;
	type_records[key] = &record;
    }

    struct DocumentCandidate {
	ExportConfigurationRecordPlan *record = NULL;
	std::vector<std::string> arguments;
	SourceKey type_key;
    };
    std::vector<DocumentCandidate> candidates;
    std::map<std::string, size_t> document_id_counts;
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "DOCUMENT")) continue;
	std::vector<std::string> arguments;
	std::string error;
	int64_t type_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 4 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
		!optional_part21_string(arguments.size() < 3 ? std::string() :
		    arguments[2]) ||
		!part21_reference(arguments.size() < 4 ? std::string() :
		    arguments[3], type_id) ||
		record.references != std::vector<int64_t>({type_id})) {
	    if (error.empty()) error = "DOCUMENT has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
#if defined(AP203)
	if (arguments[2] == "$") {
	    omit(record, "unsupported",
		"AP203 edition 1 requires a document description", statistics);
	    continue;
	}
#endif
	const SourceKey type_key(record.schema, type_id);
	const auto type = types.find(type_key);
	if (type == types.end() || !type->second) {
	    omit(record, "unsupported",
		"the retained document-type dependency was not emitted",
		statistics);
	    continue;
	}
	DocumentCandidate candidate;
	candidate.record = &record;
	candidate.arguments = arguments;
	candidate.type_key = type_key;
	candidates.push_back(candidate);
#if defined(AP203)
	++document_id_counts[arguments[0]];
#endif
    }

    for (const DocumentCandidate &candidate : candidates) {
	ExportConfigurationRecordPlan &record = *candidate.record;
#if defined(AP203)
	if (document_id_counts[candidate.arguments[0]] != 1) {
	    omit(record, "unsupported",
		"AP203 edition 1 requires every document identifier to be unique",
		statistics);
	    continue;
	}
#endif
	STEPentity *document = create_unregistered(contents, "DOCUMENT");
	if (!document) {
	    omit(record, "unsupported", "the target schema has no DOCUMENT entity",
		statistics);
	    continue;
	}
	const bool valid = brlcad::step::SetPart21(document, "id",
	    candidate.arguments[0], contents->instance_list) &&
	    brlcad::step::SetPart21(document, "name", candidate.arguments[1],
		contents->instance_list) &&
	    brlcad::step::SetPart21(document, "description",
		candidate.arguments[2], contents->instance_list) &&
	    brlcad::step::SetEntity(document, "kind", types[candidate.type_key]);
	if (!valid) {
	    delete document;
	    omit(record, "failed", "the target document layout is incompatible",
		statistics);
	    continue;
	}
	contents->instance_list->Append(document, completeSE);
	documents[SourceKey(record.schema, record.entity_id)] = document;
	used_types.insert(candidate.type_key);
	emitted(record, "authored with a remapped document-type reference",
	    statistics);
    }

    for (const auto &entry : type_records) {
	STEPentity *type = types[entry.first];
#if defined(AP203)
	if (used_types.find(entry.first) == used_types.end()) {
	    delete type;
	    omit(*entry.second, "unsupported",
		"AP203 document types must be used by an emitted document",
		statistics);
	    continue;
	}
#endif
	contents->instance_list->Append(type, completeSE);
	emitted(*entry.second, "authored as a retained document type", statistics);
    }
}

static void
emit_person_assignments(StepExportPlan &plan, AP203_Contents *contents,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    const std::map<SourceKey, STEPentity *> &classifications,
    const std::map<SourceKey, STEPentity *> &person_organizations,
    ConfigurationExportStatistics &statistics)
{
    std::map<SourceKey, STEPentity *> roles;
    std::map<SourceKey, std::string> role_names;
    std::map<SourceKey, ExportConfigurationRecordPlan *> role_records;
    std::set<SourceKey> used_roles;

    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		!equal_type(record.type, "PERSON_AND_ORGANIZATION_ROLE")) continue;
	std::vector<std::string> arguments;
	std::string error;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 1 ||
		!part21_string(arguments.empty() ? std::string() : arguments[0]) ||
		!record.references.empty()) {
	    if (error.empty()) error =
		"PERSON_AND_ORGANIZATION_ROLE has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
#if defined(AP203)
	if (!ap203_person_role(arguments[0])) {
	    omit(record, "unsupported",
		"AP203 edition 1 forbids the retained person-and-organization role",
		statistics);
	    continue;
	}
#endif
	STEPentity *role = create_unregistered(contents,
	    "PERSON_AND_ORGANIZATION_ROLE");
	if (!role) {
	    omit(record, "unsupported",
		"the target schema has no PERSON_AND_ORGANIZATION_ROLE entity",
		statistics);
	    continue;
	}
	if (!brlcad::step::SetPart21(role, "name", arguments[0],
		contents->instance_list)) {
	    delete role;
	    omit(record, "failed",
		"the target person-and-organization role layout is incompatible",
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
		(!equal_type(record.type,
		    "CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT") &&
		 !equal_type(record.type,
		    "APPLIED_PERSON_AND_ORGANIZATION_ASSIGNMENT"))) continue;
	std::vector<std::string> arguments;
	std::vector<int64_t> item_ids;
	std::string error;
	int64_t identity_id = 0;
	int64_t role_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
		!part21_reference(arguments.empty() ? std::string() : arguments[0],
		    identity_id) ||
		!part21_reference(arguments.size() < 2 ? std::string() :
		    arguments[1], role_id) ||
		!part21_reference_aggregate(arguments.size() < 3 ? std::string() :
		    arguments[2], item_ids)) {
	    if (error.empty()) error = record.type +
		" has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	std::vector<int64_t> references({identity_id, role_id});
	references.insert(references.end(), item_ids.begin(), item_ids.end());
	if (record.references != references) {
	    omit(record, "malformed", record.type +
		" references disagree with its retained Part 21 value", statistics);
	    continue;
	}
	const SourceKey identity_key(record.schema, identity_id);
	const SourceKey role_key(record.schema, role_id);
	const auto identity = person_organizations.find(identity_key);
	const auto role = roles.find(role_key);
	const auto role_name = role_names.find(role_key);
	if (identity == person_organizations.end() || !identity->second ||
		role == roles.end() || !role->second ||
		role_name == role_names.end()) {
	    omit(record, "unsupported",
		"a retained person-assignment dependency was not emitted",
		statistics);
	    continue;
	}
	std::set<int64_t> unique_items;
	std::vector<MappedItem> items;
	bool valid_items = true;
	for (int64_t item_id : item_ids) {
	    if (!unique_items.insert(item_id).second) {
		omit(record, "malformed",
		    "a person-assignment item appears more than once", statistics);
		valid_items = false;
		break;
	    }
	    MappedItem item = mapped_item(item_id, record.schema, products,
		formations, definitions, usages, classifications);
	    if (!item.entity || item.ambiguous) {
		omit(record, "unsupported",
		    "a person-assignment item has no unambiguous emitted entity",
		    statistics);
		valid_items = false;
		break;
	    }
#if defined(AP203)
	    if (!ap203_person_role_item(role_name->second, item.kind)) {
		omit(record, "unsupported",
		    "the retained role and item combination is not legal in AP203",
		    statistics);
		valid_items = false;
		break;
	    }
#endif
	    items.push_back(item);
	}
	if (!valid_items) continue;
#if defined(AP203)
	const char *target_type =
	    "CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT";
#else
	const char *target_type =
	    "APPLIED_PERSON_AND_ORGANIZATION_ASSIGNMENT";
#endif
	STEPentity *assignment = create_unregistered(contents, target_type);
	if (!assignment) {
	    omit(record, "unsupported",
		std::string("the target schema has no ") + target_type + " entity",
		statistics);
	    continue;
	}
	bool valid = brlcad::step::SetEntity(assignment,
	    "assigned_person_and_organization", identity->second) &&
	    brlcad::step::SetEntity(assignment, "role", role->second);
	for (const MappedItem &item : items)
	    valid = brlcad::step::AddEntity(assignment, "items", item.entity) &&
		valid;
	if (!valid) {
	    delete assignment;
	    omit(record, "unsupported",
		"the retained person-assignment items are not legal in the target "
		"assignment SELECT", statistics);
	    continue;
	}
	if (used_roles.insert(role_key).second) {
	    contents->instance_list->Append(role->second, completeSE);
	    emitted(*role_records[role_key],
		"authored as a retained person-and-organization role", statistics);
	}
	contents->instance_list->Append(assignment, completeSE);
	emitted(record, std::string("authored as ") + target_type +
	    " with remapped identity, role, and item references", statistics);
    }

    for (const auto &entry : role_records) {
	if (used_roles.find(entry.first) != used_roles.end()) continue;
#if defined(AP203)
	delete roles[entry.first];
	omit(*entry.second, "unsupported",
	    "AP203 person-and-organization roles must be used by an emitted "
	    "assignment", statistics);
#else
	contents->instance_list->Append(roles[entry.first], completeSE);
	emitted(*entry.second,
	    "authored as a retained person-and-organization role", statistics);
#endif
    }
}

static void
emit_security_assignments(StepExportPlan &plan, AP203_Contents *contents,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    const std::map<SourceKey, STEPentity *> &classifications,
    ConfigurationExportStatistics &statistics)
{
    const std::map<SourceKey, STEPentity *> no_classifications;
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		(!equal_type(record.type, "CC_DESIGN_SECURITY_CLASSIFICATION") &&
		 !equal_type(record.type,
		    "APPLIED_SECURITY_CLASSIFICATION_ASSIGNMENT"))) continue;
	std::vector<std::string> arguments;
	std::vector<int64_t> item_ids;
	std::string error;
	int64_t classification_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 2 ||
		!part21_reference(arguments.empty() ? std::string() : arguments[0],
		    classification_id) ||
		!part21_reference_aggregate(arguments.size() < 2 ? std::string() :
		    arguments[1], item_ids)) {
	    if (error.empty()) error = record.type +
		" has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	std::vector<int64_t> references(1, classification_id);
	references.insert(references.end(), item_ids.begin(), item_ids.end());
	if (record.references != references) {
	    omit(record, "malformed", record.type +
		" references disagree with its retained Part 21 value", statistics);
	    continue;
	}
	const auto classification = classifications.find(
	    SourceKey(record.schema, classification_id));
	if (classification == classifications.end() || !classification->second) {
	    omit(record, "unsupported",
		"the retained security-classification dependency was not emitted",
		statistics);
	    continue;
	}
	std::set<int64_t> unique_items;
	std::vector<MappedItem> items;
	bool valid_items = true;
	for (int64_t item_id : item_ids) {
	    if (!unique_items.insert(item_id).second) {
		omit(record, "malformed",
		    "a security-classification item appears more than once",
		    statistics);
		valid_items = false;
		break;
	    }
	    MappedItem item = mapped_item(item_id, record.schema, products,
		formations, definitions, usages, no_classifications);
	    if (!item.entity || item.ambiguous) {
		omit(record, "unsupported",
		    "a security-classification item has no unambiguous emitted entity",
		    statistics);
		valid_items = false;
		break;
	    }
#if defined(AP203)
	    if (item.kind != ItemKind::Formation && item.kind != ItemKind::Usage) {
		omit(record, "unsupported",
		    "the retained security-classification item is not legal in AP203",
		    statistics);
		valid_items = false;
		break;
	    }
#endif
	    items.push_back(item);
	}
	if (!valid_items) continue;
#if defined(AP203)
	const char *target_type = "CC_DESIGN_SECURITY_CLASSIFICATION";
#else
	const char *target_type = "APPLIED_SECURITY_CLASSIFICATION_ASSIGNMENT";
#endif
	STEPentity *assignment = create_unregistered(contents, target_type);
	if (!assignment) {
	    omit(record, "unsupported",
		std::string("the target schema has no ") + target_type + " entity",
		statistics);
	    continue;
	}
	bool valid = brlcad::step::SetEntity(assignment,
	    "assigned_security_classification", classification->second);
	for (const MappedItem &item : items)
	    valid = brlcad::step::AddEntity(assignment, "items", item.entity) &&
		valid;
	if (!valid) {
	    delete assignment;
	    omit(record, "unsupported",
		"the retained security-classification items are not legal in the "
		"target assignment SELECT", statistics);
	    continue;
	}
	contents->instance_list->Append(assignment, completeSE);
	emitted(record, std::string("authored as ") + target_type +
	    " with remapped classification and item references", statistics);
    }
}

static void
emit_document_references(StepExportPlan &plan, AP203_Contents *contents,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    const std::map<SourceKey, STEPentity *> &documents,
    ConfigurationExportStatistics &statistics)
{
    const std::map<SourceKey, STEPentity *> no_classifications;
    for (ExportConfigurationRecordPlan &record : plan.configuration_records) {
	if (!record.valid || record.export_status != "pending" ||
		(!equal_type(record.type, "CC_DESIGN_SPECIFICATION_REFERENCE") &&
		 !equal_type(record.type, "APPLIED_DOCUMENT_REFERENCE"))) continue;
	std::vector<std::string> arguments;
	std::vector<int64_t> item_ids;
	std::string error;
	int64_t document_id = 0;
	if (!part21_arguments(record, arguments, error) || arguments.size() != 3 ||
		!part21_reference(arguments.empty() ? std::string() : arguments[0],
		    document_id) ||
		!part21_string(arguments.size() < 2 ? std::string() : arguments[1]) ||
		!part21_reference_aggregate(arguments.size() < 3 ? std::string() :
		    arguments[2], item_ids)) {
	    if (error.empty()) error = record.type +
		" has an invalid retained layout";
	    omit(record, "malformed", error, statistics);
	    continue;
	}
	std::vector<int64_t> references(1, document_id);
	references.insert(references.end(), item_ids.begin(), item_ids.end());
	if (record.references != references) {
	    omit(record, "malformed", record.type +
		" references disagree with its retained Part 21 value", statistics);
	    continue;
	}
	const auto document = documents.find(SourceKey(record.schema, document_id));
	if (document == documents.end() || !document->second) {
	    omit(record, "unsupported",
		"the retained document dependency was not emitted", statistics);
	    continue;
	}
	std::set<int64_t> unique_items;
	std::vector<MappedItem> items;
	bool valid_items = true;
	for (int64_t item_id : item_ids) {
	    if (!unique_items.insert(item_id).second) {
		omit(record, "malformed",
		    "a document-reference item appears more than once", statistics);
		valid_items = false;
		break;
	    }
	    MappedItem item = mapped_item(item_id, record.schema, products,
		formations, definitions, usages, no_classifications);
	    if (!item.entity || item.ambiguous) {
		omit(record, "unsupported",
		    "a document-reference item has no unambiguous emitted entity",
		    statistics);
		valid_items = false;
		break;
	    }
#if defined(AP203)
	    if (item.kind != ItemKind::Definition) {
		omit(record, "unsupported",
		    "the retained document-reference item is not legal in AP203",
		    statistics);
		valid_items = false;
		break;
	    }
#endif
	    items.push_back(item);
	}
	if (!valid_items) continue;
#if defined(AP203)
	const char *target_type = "CC_DESIGN_SPECIFICATION_REFERENCE";
#else
	const char *target_type = "APPLIED_DOCUMENT_REFERENCE";
#endif
	STEPentity *reference = create_unregistered(contents, target_type);
	if (!reference) {
	    omit(record, "unsupported",
		std::string("the target schema has no ") + target_type + " entity",
		statistics);
	    continue;
	}
	bool valid = brlcad::step::SetEntity(reference, "assigned_document",
	    document->second) && brlcad::step::SetPart21(reference, "source",
		arguments[1], contents->instance_list);
	for (const MappedItem &item : items)
	    valid = brlcad::step::AddEntity(reference, "items", item.entity) &&
		valid;
	if (!valid) {
	    delete reference;
	    omit(record, "unsupported",
		"the retained document-reference items are not legal in the target "
		"assignment SELECT", statistics);
	    continue;
	}
	contents->instance_list->Append(reference, completeSE);
	emitted(record, std::string("authored as ") + target_type +
	    " with remapped document and item references", statistics);
    }
}

} // namespace step_configuration_identity

brlcad::step::ConfigurationIdentityEntities
brlcad::step::EmitSTEPConfigurationIdentities(StepExportPlan &plan,
    AP203_Contents *contents,
    const std::map<int64_t, STEPentity *> &products,
    const std::map<int64_t, STEPentity *> &formations,
    const std::map<int64_t, STEPentity *> &definitions,
    const std::map<int64_t, STEPentity *> &usages,
    ConfigurationExportStatistics &statistics)
{
    ConfigurationIdentityEntities result;

    step_configuration_identity::emit_people_and_organizations(plan, contents,
	result.organizations, result.people, result.person_organizations,
	statistics);
    step_configuration_identity::emit_addresses(plan, contents,
	result.organizations, result.people, statistics);
    step_configuration_identity::emit_security_identities(plan, contents,
	result.classifications, statistics);
    step_configuration_identity::emit_document_identities(plan, contents,
	result.documents, statistics);
    step_configuration_identity::emit_person_assignments(plan, contents,
	products, formations, definitions, usages, result.classifications,
	result.person_organizations, statistics);
    step_configuration_identity::emit_security_assignments(plan, contents,
	products, formations, definitions, usages, result.classifications,
	statistics);
    step_configuration_identity::emit_document_references(plan, contents,
	products, formations, definitions, usages, result.documents, statistics);
    return result;
}
