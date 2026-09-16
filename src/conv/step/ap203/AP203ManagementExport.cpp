/*          A P 2 0 3 M A N A G E M E N T E X P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP203ManagementExport.h"

#include "AP_Common.h"
#include "STEPExportContext.h"
#include "STEPGeneratedAPI.h"
#include "STEPString.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

using brlcad::step::AddEntity;
using brlcad::step::Entities;
using brlcad::step::Entity;
using brlcad::step::SetEntity;
using brlcad::step::SetEnum;
using brlcad::step::SetInteger;
using brlcad::step::SetString;

static bool
is_a(AP203_Contents *contents, const SDAI_Application_instance *instance,
    const char *type)
{
    if (!contents || !contents->registry || !instance || !type) return false;
    const EntityDescriptor *descriptor = contents->registry->FindEntity(type);
    return descriptor && instance->IsA(descriptor);
}

static std::vector<STEPentity *>
instances_of(AP203_Contents *contents, const char *type)
{
    std::vector<STEPentity *> result;
    if (!contents || !contents->instance_list) return result;
    const int count = contents->instance_list->InstanceCount();
    for (int i = 0; i < count; ++i) {
	STEPentity *instance = dynamic_cast<STEPentity *>(
	    contents->instance_list->GetSTEPentity(i));
	if (instance && !isNilSTEPentity(instance) && is_a(contents, instance, type))
	    result.push_back(instance);
    }
    return result;
}

static bool
contains_member(STEPentity *assignment, const char *attribute,
    STEPentity *item)
{
    const std::vector<SDAI_Application_instance *> items =
	Entities(assignment, attribute);
    return std::find(items.begin(), items.end(), item) != items.end();
}

static bool
contains_item(STEPentity *assignment, STEPentity *item)
{
    return contains_member(assignment, "items", item);
}

static size_t
item_assignment_count(const std::vector<STEPentity *> &assignments,
    STEPentity *item)
{
    size_t count = 0;
    for (STEPentity *assignment : assignments)
	if (contains_item(assignment, item)) ++count;
    return count;
}

static std::string
entity_string(STEPentity *entity, const char *attribute_name)
{
    STEPattribute *attribute = brlcad::step::Attribute(entity, attribute_name);
    const SDAI_String *value = attribute ? attribute->String() : NULL;
    return value ? brlcad::step::decode_string(value->c_str()) : std::string();
}

static bool
is_principal_product_category(STEPentity *category)
{
    const std::string name = entity_string(category, "name");
    return name == "assembly" || name == "inseparable_assembly" ||
	name == "detail" || name == "customer_furnished_equipment";
}

static size_t
principal_category_count(const std::vector<STEPentity *> &categories,
    STEPentity *product)
{
    size_t count = 0;
    for (STEPentity *category : categories)
	if (is_principal_product_category(category) &&
		contains_member(category, "products", product)) ++count;
    return count;
}

static std::string
assignment_role(STEPentity *assignment)
{
    STEPentity *role = dynamic_cast<STEPentity *>(Entity(assignment, "role"));
    return entity_string(role, "name");
}

static size_t
role_assignment_count(const std::vector<STEPentity *> &assignments,
    STEPentity *item, const char *role)
{
    size_t count = 0;
    for (STEPentity *assignment : assignments) {
	if (role && assignment_role(assignment) != role) continue;
	if (contains_item(assignment, item)) ++count;
    }
    return count;
}

static size_t
approval_assignment_count(const std::vector<STEPentity *> &assignments,
    STEPentity *approval)
{
    size_t count = 0;
    for (STEPentity *assignment : assignments)
	if (Entity(assignment, "assigned_approval") == approval) ++count;
    return count;
}

static size_t
approval_date_count(const std::vector<STEPentity *> &dates,
    STEPentity *approval)
{
    size_t count = 0;
    for (STEPentity *date : dates)
	if (Entity(date, "dated_approval") == approval) ++count;
    return count;
}

static size_t
approval_person_count(const std::vector<STEPentity *> &people,
    STEPentity *approval)
{
    size_t count = 0;
    for (STEPentity *person : people)
	if (Entity(person, "authorized_approval") == approval) ++count;
    return count;
}

static size_t
application_definition_count(const std::vector<STEPentity *> &definitions,
    STEPentity *application)
{
    size_t count = 0;
    for (STEPentity *definition : definitions)
	if (Entity(definition, "application") == application) ++count;
    return count;
}

static bool
append_items(STEPentity *assignment, const std::vector<STEPentity *> &items)
{
    bool valid = assignment && !items.empty();
    for (STEPentity *item : items)
	valid = AddEntity(assignment, "items", item) && valid;
    return valid;
}

static void
destroy_pending(std::vector<STEPentity *> &pending)
{
    for (STEPentity *entity : pending) delete entity;
    pending.clear();
}

} // namespace

bool
brlcad::step::FinalizeAP203ManagementGraph(AP203_Contents *contents,
    std::vector<std::string> &diagnostics)
{
    if (!contents || !contents->registry || !contents->instance_list ||
	    !contents->application_context) {
	diagnostics.push_back("AP203 administrative closure has no export context");
	return false;
    }

    std::vector<STEPentity *> products = instances_of(contents, "PRODUCT");
    std::vector<STEPentity *> formations = instances_of(contents,
	"PRODUCT_DEFINITION_FORMATION");
    std::vector<STEPentity *> definitions = instances_of(contents,
	"PRODUCT_DEFINITION");
    std::vector<STEPentity *> usages = instances_of(contents,
	"ASSEMBLY_COMPONENT_USAGE");
    std::vector<STEPentity *> configuration_items = instances_of(contents,
	"CONFIGURATION_ITEM");
    std::vector<STEPentity *> effectivities = instances_of(contents,
	"EFFECTIVITY");
    std::vector<STEPentity *> approvals = instances_of(contents, "APPROVAL");
    std::vector<STEPentity *> approval_assignments = instances_of(contents,
	"CC_DESIGN_APPROVAL");
    std::vector<STEPentity *> approval_dates = instances_of(contents,
	"APPROVAL_DATE_TIME");
    std::vector<STEPentity *> approval_people = instances_of(contents,
	"APPROVAL_PERSON_ORGANIZATION");
    std::vector<STEPentity *> person_assignments = instances_of(contents,
	"CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT");
    std::vector<STEPentity *> date_assignments = instances_of(contents,
	"CC_DESIGN_DATE_AND_TIME_ASSIGNMENT");
    std::vector<STEPentity *> classifications = instances_of(contents,
	"SECURITY_CLASSIFICATION");
    std::vector<STEPentity *> classification_assignments = instances_of(
	contents, "CC_DESIGN_SECURITY_CLASSIFICATION");
    std::vector<STEPentity *> categories = instances_of(contents,
	"PRODUCT_RELATED_PRODUCT_CATEGORY");
    std::vector<STEPentity *> protocol_definitions = instances_of(contents,
	"APPLICATION_PROTOCOL_DEFINITION");

    for (STEPentity *formation : formations) {
	if (!is_a(contents, formation,
		"PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE")) {
	    diagnostics.push_back("AP203 PRODUCT_DEFINITION_FORMATION is not the "
		"mandatory specified-source subtype");
	    return false;
	}
    }

    const auto require_at_most_one = [&diagnostics](size_t count,
	    const char *description) {
	if (count <= 1) return true;
	diagnostics.push_back(std::string("AP203 administrative closure found more "
	    "than one ") + description);
	return false;
    };

    for (STEPentity *product : products) {
	if (!require_at_most_one(principal_category_count(categories, product),
		"product category for a product") ||
		!require_at_most_one(item_assignment_count(person_assignments,
		    product), "person/organization assignment for a product"))
	    return false;
    }
    for (STEPentity *definition : definitions) {
	if (!require_at_most_one(item_assignment_count(approval_assignments,
		definition), "approval for a product definition") ||
		!require_at_most_one(item_assignment_count(person_assignments,
		    definition), "person/organization assignment for a product "
			"definition") ||
		!require_at_most_one(item_assignment_count(date_assignments,
		    definition), "date/time assignment for a product definition"))
	    return false;
    }
    for (STEPentity *formation : formations) {
	if (!require_at_most_one(item_assignment_count(approval_assignments,
		formation), "approval for a product formation") ||
		!require_at_most_one(role_assignment_count(person_assignments,
		    formation, "creator"), "creator assignment for a product "
			"formation") ||
		!require_at_most_one(item_assignment_count(classification_assignments,
		    formation), "security classification for a product formation"))
	    return false;
    }
    for (STEPentity *usage : usages) {
	if (!require_at_most_one(item_assignment_count(classification_assignments,
		usage), "security classification for an assembly usage"))
	    return false;
    }
    for (STEPentity *item : configuration_items) {
	if (!require_at_most_one(item_assignment_count(approval_assignments, item),
		"approval for a configuration item") ||
		!require_at_most_one(item_assignment_count(person_assignments, item),
		    "person/organization assignment for a configuration item"))
	    return false;
    }
    for (STEPentity *effectivity : effectivities) {
	if (!require_at_most_one(item_assignment_count(approval_assignments,
		effectivity), "approval for an effectivity")) return false;
    }
    for (STEPentity *classification : classifications) {
	if (!require_at_most_one(item_assignment_count(approval_assignments,
		classification), "approval for a security classification") ||
		!require_at_most_one(item_assignment_count(person_assignments,
		    classification), "person/organization assignment for a "
			"security classification") ||
		!require_at_most_one(role_assignment_count(date_assignments,
		    classification, "classification_date"),
		    "classification date for a security classification") ||
		!require_at_most_one(role_assignment_count(date_assignments,
		    classification, "declassification_date"),
		    "declassification date for a security classification"))
	    return false;
    }
    for (STEPentity *approval : approvals) {
	STEPentity *status = dynamic_cast<STEPentity *>(
	    Entity(approval, "status"));
	const std::string status_name = entity_string(status, "name");
	static const std::set<std::string> allowed_statuses = {
	    "approved", "not_yet_approved", "disapproved", "withdrawn"
	};
	if (allowed_statuses.find(status_name) == allowed_statuses.end()) {
	    diagnostics.push_back("AP203 APPROVAL_STATUS has a value forbidden "
		"by restrict_approval_status");
	    return false;
	}
	if (!approval_assignment_count(approval_assignments, approval)) {
	    diagnostics.push_back("AP203 retained APPROVAL has no authorable "
		"CC_DESIGN_APPROVAL assignment");
	    return false;
	}
	if (!require_at_most_one(approval_date_count(approval_dates, approval),
		"approval date for an approval")) return false;
    }
    static const std::set<std::string> allowed_categories = {
	"assembly", "detail", "customer_furnished_equipment",
	"inseparable_assembly", "cast", "coined", "drawn", "extruded",
	"forged", "formed", "machined", "molded", "rolled", "sheared"
    };
    for (STEPentity *category : categories) {
	if (allowed_categories.find(entity_string(category, "name")) ==
		allowed_categories.end()) {
	    diagnostics.push_back("AP203 product category has a value forbidden "
		"by restrict_product_category_value");
	    return false;
	}
    }
    for (STEPentity *definition : protocol_definitions) {
	if (entity_string(definition,
		"application_interpreted_model_schema_name") !=
		    "config_control_design") {
	    diagnostics.push_back("AP203 application protocol definition names "
		"a different interpreted model schema");
	    return false;
	}
    }

    std::vector<STEPentity *> pending;
    bool valid = true;
    const auto make = [&pending, contents](const char *type) -> STEPentity * {
	STEPentity *entity = contents->registry->ObjCreate(type);
	if (!entity || isNilSTEPentity(entity)) return NULL;
	pending.push_back(entity);
	return entity;
    };
    const auto checkpoint = [&valid, &pending, &diagnostics](
	const char *phase) {
	if (valid) return true;
	destroy_pending(pending);
	diagnostics.push_back(std::string("could not author AP203 administrative ") +
	    phase + " with the active schema bindings");
	return false;
    };

    /* One deterministic neutral identity and timestamp close all generated
	* mandatory associations.  They are intentionally explicit in the file:
	* unlike retained source metadata, these values do not pretend to be
	* recovered authoring provenance. */
    STEPentity *person = make("PERSON");
    STEPentity *organization = make("ORGANIZATION");
    STEPentity *person_organization = make("PERSON_AND_ORGANIZATION");
    valid = person && organization && person_organization &&
	SetString(person, "id", "BRL-CAD") &&
	SetString(person, "last_name", "BRL-CAD") &&
	SetString(organization, "id", "BRL-CAD") &&
	SetString(organization, "name", "BRL-CAD") &&
	SetString(organization, "description",
	    "generated AP203 administrative identity") &&
	SetEntity(person_organization, "the_person", person) &&
	SetEntity(person_organization, "the_organization", organization) && valid;
    if (!checkpoint("identity")) return false;

    STEPentity *date = make("CALENDAR_DATE");
    STEPentity *offset = make("COORDINATED_UNIVERSAL_TIME_OFFSET");
    STEPentity *time = make("LOCAL_TIME");
    STEPentity *date_time = make("DATE_AND_TIME");
    valid = date && offset && time && date_time &&
	SetInteger(date, "year_component", 1970) &&
	SetInteger(date, "day_component", 1) &&
	SetInteger(date, "month_component", 1) &&
	SetInteger(offset, "hour_offset", 0) &&
	SetEnum(offset, "sense", "AHEAD") &&
	SetInteger(time, "hour_component", 0) &&
	SetEntity(time, "zone", offset) &&
	SetEntity(date_time, "date_component", date) &&
	SetEntity(date_time, "time_component", time) && valid;
    if (!checkpoint("timestamp")) return false;

    for (STEPentity *application : instances_of(contents,
	    "APPLICATION_CONTEXT")) {
	const size_t count = application_definition_count(protocol_definitions,
	    application);
	if (!require_at_most_one(count,
		"application protocol definition for an application context")) {
	    destroy_pending(pending);
	    return false;
	}
	if (count) continue;
	STEPentity *definition = make("APPLICATION_PROTOCOL_DEFINITION");
	valid = definition && SetString(definition, "status",
	    "international standard") && SetString(definition,
	    "application_interpreted_model_schema_name",
	    "config_control_design") && SetInteger(definition,
	    "application_protocol_year", 1994) && SetEntity(definition,
	    "application", application) && valid;
    }
    if (!checkpoint("application protocol definition")) return false;

    std::vector<STEPentity *> unclassified;
    for (STEPentity *formation : formations)
	if (!item_assignment_count(classification_assignments, formation))
	    unclassified.push_back(formation);
    for (STEPentity *usage : usages)
	if (!item_assignment_count(classification_assignments, usage))
	    unclassified.push_back(usage);
    if (!unclassified.empty()) {
	STEPentity *level = make("SECURITY_CLASSIFICATION_LEVEL");
	STEPentity *classification = make("SECURITY_CLASSIFICATION");
	STEPentity *assignment = make("CC_DESIGN_SECURITY_CLASSIFICATION");
	valid = level && classification && assignment &&
	    SetString(level, "name", "unclassified") &&
	    SetString(classification, "name", "unclassified") &&
	    SetString(classification, "purpose",
		"generated AP203 administrative classification") &&
	    SetEntity(classification, "security_level", level) &&
	    SetEntity(assignment, "assigned_security_classification",
		classification) && append_items(assignment, unclassified) && valid;
	classifications.push_back(classification);
	classification_assignments.push_back(assignment);
    }
    if (!checkpoint("security classification")) return false;

    std::vector<STEPentity *> missing_approvals;
    for (STEPentity *definition : definitions)
	if (!item_assignment_count(approval_assignments, definition))
	    missing_approvals.push_back(definition);
    for (STEPentity *formation : formations)
	if (!item_assignment_count(approval_assignments, formation))
	    missing_approvals.push_back(formation);
    for (STEPentity *classification : classifications)
	if (!item_assignment_count(approval_assignments, classification))
	    missing_approvals.push_back(classification);
    for (STEPentity *item : configuration_items)
	if (!item_assignment_count(approval_assignments, item))
	    missing_approvals.push_back(item);
    for (STEPentity *effectivity : effectivities)
	if (!item_assignment_count(approval_assignments, effectivity))
	    missing_approvals.push_back(effectivity);
    if (!missing_approvals.empty()) {
	STEPentity *status = make("APPROVAL_STATUS");
	STEPentity *approval = make("APPROVAL");
	STEPentity *assignment = make("CC_DESIGN_APPROVAL");
	valid = status && approval && assignment &&
	    SetString(status, "name", "not_yet_approved") &&
	    SetEntity(approval, "status", status) &&
	    SetString(approval, "level", "generated administrative closure") &&
	    SetEntity(assignment, "assigned_approval", approval) &&
	    append_items(assignment, missing_approvals) && valid;
	approvals.push_back(approval);
	approval_assignments.push_back(assignment);
    }
    if (!checkpoint("approval assignment")) return false;

    STEPentity *approval_role = NULL;
    for (STEPentity *approval : approvals) {
	if (!approval_date_count(approval_dates, approval)) {
	    STEPentity *approval_date = make("APPROVAL_DATE_TIME");
	    valid = approval_date && SetEntity(approval_date, "date_time",
		date_time) && SetEntity(approval_date, "dated_approval", approval) &&
		valid;
	}
	if (!approval_person_count(approval_people, approval)) {
	    if (!approval_role) {
		approval_role = make("APPROVAL_ROLE");
		valid = approval_role && SetString(approval_role, "role",
		    "authorizer") && valid;
	    }
	    STEPentity *approval_person = make("APPROVAL_PERSON_ORGANIZATION");
	    valid = approval_person && SetEntity(approval_person,
		"person_organization", person_organization) &&
		SetEntity(approval_person, "authorized_approval", approval) &&
		SetEntity(approval_person, "role", approval_role) && valid;
	}
    }
    if (!checkpoint("approval provenance")) return false;

    const auto add_person_assignment = [&make, &valid, person_organization](
	const char *role_name,
	    const std::vector<STEPentity *> &items) {
	if (items.empty()) return;
	STEPentity *role = make("PERSON_AND_ORGANIZATION_ROLE");
	STEPentity *assignment =
	    make("CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT");
	valid = role && assignment && SetString(role, "name", role_name) &&
	    SetEntity(assignment, "assigned_person_and_organization",
		person_organization) && SetEntity(assignment, "role", role) &&
	    append_items(assignment, items) && valid;
    };

    std::vector<STEPentity *> design_owners;
    for (STEPentity *product : products)
	if (!item_assignment_count(person_assignments, product))
	    design_owners.push_back(product);
    add_person_assignment("design_owner", design_owners);

    std::vector<STEPentity *> creators;
    for (STEPentity *formation : formations)
	if (!role_assignment_count(person_assignments, formation, "creator"))
	    creators.push_back(formation);
    for (STEPentity *definition : definitions)
	if (!item_assignment_count(person_assignments, definition))
	    creators.push_back(definition);
    add_person_assignment("creator", creators);

    std::vector<STEPentity *> suppliers;
    for (STEPentity *formation : formations)
	if (!role_assignment_count(person_assignments, formation,
		"design_supplier") &&
		!role_assignment_count(person_assignments, formation,
		    "part_supplier")) suppliers.push_back(formation);
    add_person_assignment("design_supplier", suppliers);

    std::vector<STEPentity *> officers;
    for (STEPentity *classification : classifications)
	if (!item_assignment_count(person_assignments, classification))
	    officers.push_back(classification);
    add_person_assignment("classification_officer", officers);

    std::vector<STEPentity *> configuration_managers;
    for (STEPentity *item : configuration_items)
	if (!item_assignment_count(person_assignments, item))
	    configuration_managers.push_back(item);
    add_person_assignment("configuration_manager", configuration_managers);
    if (!checkpoint("person and organization assignment")) return false;

    const auto add_date_assignment = [&make, &valid, date_time](
	const char *role_name, const std::vector<STEPentity *> &items) {
	if (items.empty()) return;
	STEPentity *role = make("DATE_TIME_ROLE");
	STEPentity *assignment = make("CC_DESIGN_DATE_AND_TIME_ASSIGNMENT");
	valid = role && assignment && SetString(role, "name", role_name) &&
	    SetEntity(assignment, "assigned_date_and_time", date_time) &&
	    SetEntity(assignment, "role", role) &&
	    append_items(assignment, items) && valid;
    };

    std::vector<STEPentity *> undated_definitions;
    for (STEPentity *definition : definitions)
	if (!item_assignment_count(date_assignments, definition))
	    undated_definitions.push_back(definition);
    add_date_assignment("creation_date", undated_definitions);

    std::vector<STEPentity *> undated_classifications;
    for (STEPentity *classification : classifications)
	if (!role_assignment_count(date_assignments, classification,
		"classification_date"))
	    undated_classifications.push_back(classification);
    add_date_assignment("classification_date", undated_classifications);
    if (!checkpoint("date and time assignment")) return false;

    std::set<STEPentity *> assembly_products;
    for (STEPentity *usage : usages) {
	STEPentity *parent = dynamic_cast<STEPentity *>(
	    Entity(usage, "relating_product_definition"));
	STEPentity *formation = dynamic_cast<STEPentity *>(
	    Entity(parent, "formation"));
	STEPentity *product = dynamic_cast<STEPentity *>(
	    Entity(formation, "of_product"));
	if (product) assembly_products.insert(product);
    }
    std::vector<STEPentity *> assemblies;
    std::vector<STEPentity *> details;
    for (STEPentity *product : products) {
	if (principal_category_count(categories, product)) continue;
	(assembly_products.find(product) == assembly_products.end() ? details :
	    assemblies).push_back(product);
    }
    const auto add_category = [&make, &valid](const char *name,
	const std::vector<STEPentity *> &items) {
	if (items.empty()) return;
	STEPentity *category = make("PRODUCT_RELATED_PRODUCT_CATEGORY");
	valid = category && SetString(category, "name", name) &&
	    SetString(category, "description",
		"generated AP203 product classification") && valid;
	for (STEPentity *item : items)
	    valid = AddEntity(category, "products", item) && valid;
    };
    add_category("assembly", assemblies);
    add_category("detail", details);
    if (!checkpoint("product category")) return false;
    for (STEPentity *entity : pending)
	contents->instance_list->Append(entity, completeSE);
    diagnostics.push_back("authored deterministic AP203 administrative "
	"defaults for mandatory associations absent from the BRL-CAD database");
    return true;
}
