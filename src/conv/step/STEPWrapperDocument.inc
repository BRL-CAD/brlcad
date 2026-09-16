/* BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/STEPWrapperDocument.inc
 *
 * Schema-neutral document extraction, style resolution, detached geometry
 * jobs, and product write-pipeline helpers.  Included only by
 * STEPImportPipeline.cpp.
 */

static const brlcad::step::Style *
style_for_item(STEPWrapper *wrapper, int64_t item_id)
{
    const std::map<int64_t, brlcad::step::Style> &styles = wrapper->Document().styles;
    std::map<int64_t, brlcad::step::Style>::const_iterator found = styles.find(item_id);
    return found == styles.end() ? NULL : &found->second;
}


struct FlattenedRepresentationItem {
    RepresentationItem *item = NULL;
    int64_t inherited_style_id = 0;
};


static void
flatten_representation_item(RepresentationItem *item, STEPWrapper *wrapper,
    int64_t inherited_style_id, std::set<int64_t> &active,
    std::vector<FlattenedRepresentationItem> &result)
{
    if (!item || !wrapper)
	return;

    const int64_t item_id = item->GetId();
    int64_t effective_style_id = inherited_style_id;
    if (style_for_item(wrapper, item_id))
	effective_style_id = item_id;

    CompoundRepresentationItem *compound = dynamic_cast<CompoundRepresentationItem *>(item);
    if (!compound) {
	FlattenedRepresentationItem entry;
	entry.item = item;
	entry.inherited_style_id = effective_style_id == item_id ? 0 : effective_style_id;
	result.push_back(entry);
	return;
    }

    if (!active.insert(item_id).second) {
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, item_id,
	    "COMPOUND_REPRESENTATION_ITEM", "item_element",
	    "cyclic compound representation item");
	return;
    }
    const std::vector<RepresentationItem *> &elements = compound->Elements();
    for (std::vector<RepresentationItem *>::const_iterator element = elements.begin();
	 element != elements.end(); ++element)
	flatten_representation_item(*element, wrapper, effective_style_id, active, result);
    if (!elements.empty())
	wrapper->RecordRepresentationItemCoverage(item_id,
	    brlcad::step::RepresentationCoverageStatus::Handled,
	    "compound representation container expanded recursively");
    active.erase(item_id);
}


static std::vector<FlattenedRepresentationItem>
flatten_representation_items(LIST_OF_REPRESENTATION_ITEMS *items, STEPWrapper *wrapper)
{
    std::vector<FlattenedRepresentationItem> result;
    std::set<int64_t> active;
    if (!items)
	return result;
    for (LIST_OF_REPRESENTATION_ITEMS::iterator item = items->begin(); item != items->end(); ++item)
	flatten_representation_item(*item, wrapper, 0, active, result);
    return result;
}


static const brlcad::step::Style *
style_for_flattened_item(STEPWrapper *wrapper, const FlattenedRepresentationItem &item)
{
    const brlcad::step::Style *style = item.item ? style_for_item(wrapper, item.item->GetId()) : NULL;
    return style ? style : style_for_item(wrapper, item.inherited_style_id);
}


static void
axis_representation_matrix(Axis2Placement3D *axis, double length_factor, mat_t mat)
{
    MAT_IDN(mat);
    if (!axis)
	return;

    double translate_to[3];
    const double *toXaxis = axis->GetXAxis();
    const double *toYaxis = axis->GetYAxis();
    const double *toZaxis = axis->GetZAxis();
    mat_t rot_mat;

    VMOVE(translate_to, axis->GetOrigin());
    VSCALE(translate_to, translate_to, length_factor);

    MAT_IDN(rot_mat);
    VMOVE(&rot_mat[0], toXaxis);
    VMOVE(&rot_mat[4], toYaxis);
    VMOVE(&rot_mat[8], toZaxis);
    bn_mat_inv(mat, rot_mat);
    MAT_DELTAS_VEC(mat, translate_to);
}


static void
representation_matrix(Representation *representation, mat_t mat)
{
    MAT_IDN(mat);
    if (!representation)
	return;

    Axis2Placement3D *axis = NULL;
    LIST_OF_REPRESENTATION_ITEMS *items = representation->items_();
    if (items) {
	for (LIST_OF_REPRESENTATION_ITEMS::iterator i = items->begin(); i != items->end(); ++i) {
	    axis = dynamic_cast<Axis2Placement3D *>(*i);
	    if (axis) break;
	}
    }
    axis_representation_matrix(axis, representation->GetLengthConversionFactor(), mat);
}


static void
orthonormal_axes(Direction *axis1, Direction *axis2, Direction *axis3,
	vect_t xaxis, vect_t yaxis, vect_t zaxis)
{
    VSET(xaxis, 1.0, 0.0, 0.0);
    VSET(yaxis, 0.0, 1.0, 0.0);
    VSET(zaxis, 0.0, 0.0, 1.0);
    if (axis1) VMOVE(xaxis, axis1->DirectionRatios());
    if (axis2) VMOVE(yaxis, axis2->DirectionRatios());
    if (axis3) VMOVE(zaxis, axis3->DirectionRatios());

    if (MAGNITUDE(xaxis) <= SMALL_FASTF)
	VSET(xaxis, 1.0, 0.0, 0.0);
    else
	VUNITIZE(xaxis);

    if (axis3 && !axis2) {
	if (MAGNITUDE(zaxis) <= SMALL_FASTF)
	    VSET(zaxis, 0.0, 0.0, 1.0);
	else
	    VUNITIZE(zaxis);
	VCROSS(yaxis, zaxis, xaxis);
	if (MAGNITUDE(yaxis) <= SMALL_FASTF) {
	    vect_t fallback;
	    VSET(fallback, 0.0, 1.0, 0.0);
	    VCROSS(yaxis, fallback, xaxis);
	    VUNITIZE(yaxis);
	} else
	    VUNITIZE(yaxis);
	VCROSS(zaxis, xaxis, yaxis);
	VUNITIZE(zaxis);
	return;
    }

    VJOIN1(yaxis, yaxis, -(VDOT(xaxis, yaxis)), xaxis);
    if (MAGNITUDE(yaxis) <= SMALL_FASTF) {
	vect_t fallback;
	VSET(fallback, 0.0, 1.0, 0.0);
	if (fabs(VDOT(fallback, xaxis)) > 0.9)
	    VSET(fallback, 0.0, 0.0, 1.0);
	VJOIN1(yaxis, fallback, -(VDOT(xaxis, fallback)), xaxis);
	VUNITIZE(yaxis);
    } else
	VUNITIZE(yaxis);
    VCROSS(zaxis, xaxis, yaxis);
    VUNITIZE(zaxis);
    if (axis3 && VDOT(zaxis, axis3->DirectionRatios()) < 0.0) {
	VREVERSE(yaxis, yaxis);
	VREVERSE(zaxis, zaxis);
    }
}


static void
placement_frame(Axis2Placement3D *placement, double length_factor, mat_t mat)
{
    MAT_IDN(mat);
    if (!placement)
	return;
    VMOVE(&mat[0], placement->GetXAxis());
    VMOVE(&mat[4], placement->GetYAxis());
    VMOVE(&mat[8], placement->GetZAxis());
    vect_t origin;
    VMOVE(origin, placement->GetOrigin());
    VSCALE(origin, origin, length_factor);
    MAT_DELTAS_VEC(mat, origin);
}


static bool
mapped_item_matrix(MappedItem *mapped, double length_factor, mat_t mat)
{
    MAT_IDN(mat);
    if (!mapped || !mapped->MappingSource())
	return false;
    CartesianTransformationOperator3D *target =
	dynamic_cast<CartesianTransformationOperator3D *>(mapped->MappingTarget());
    Axis2Placement3D *axis_target =
	dynamic_cast<Axis2Placement3D *>(mapped->MappingTarget());
    Axis2Placement3D *origin = dynamic_cast<Axis2Placement3D *>(
	mapped->MappingSource()->MappingOrigin());
    if ((!target && !axis_target) || (target && !target->LocalOrigin()) || !origin)
	return false;

    mat_t target_matrix;
    if (axis_target) {
	placement_frame(axis_target, length_factor, target_matrix);
    } else {
	vect_t xaxis, yaxis, zaxis;
	orthonormal_axes(target->Axis1(), target->Axis2(), target->Axis3(),
	    xaxis, yaxis, zaxis);
	MAT_IDN(target_matrix);
	const double scale = target->Scale();
	VSCALE(&target_matrix[0], xaxis, scale);
	VSCALE(&target_matrix[4], yaxis, scale);
	VSCALE(&target_matrix[8], zaxis, scale);
	vect_t target_origin;
	VMOVE(target_origin, target->LocalOrigin()->Point3d());
	VSCALE(target_origin, target_origin, length_factor);
	MAT_DELTAS_VEC(target_matrix, target_origin);
    }

    mat_t origin_matrix;
    mat_t origin_inverse;
    placement_frame(origin, length_factor, origin_matrix);
    bn_mat_inv(origin_inverse, origin_matrix);
    bn_mat_mul(mat, target_matrix, origin_inverse);
    return true;
}


static SolidModel *
exact_brep_solid(RepresentationItem *item)
{
    if (ManifoldSolidBrep *manifold = dynamic_cast<ManifoldSolidBrep *>(item))
	return manifold;
    return dynamic_cast<SolidReplica *>(item);
}


static const char *
exact_brep_solid_type(SolidModel *solid)
{
    if (dynamic_cast<SolidReplica *>(solid)) return "SOLID_REPLICA";
    if (dynamic_cast<FacetedBrep *>(solid)) return "FACETED_BREP";
    if (dynamic_cast<BrepWithVoids *>(solid)) return "BREP_WITH_VOIDS";
    return "MANIFOLD_SOLID_BREP";
}


#ifdef HAVE_STEPCODE_LAZY
struct LazySTEPOccurrence {
    uint64_t entity_id = 0;
    uint64_t usage_id = 0;
    uint64_t relationship_id = 0;
    uint64_t parent_representation_id = 0;
    uint64_t child_representation_id = 0;
    uint64_t parent_product_id = 0;
    uint64_t child_product_id = 0;
    uint64_t parent_axis_id = 0;
    uint64_t child_axis_id = 0;
    bool mapped = false;
    std::string shape_method;
};


struct LazySTEPRepresentationTransform {
    uint64_t relationship_id = 0;
    uint64_t parent_axis_id = 0;
    uint64_t child_axis_id = 0;
};


struct LazySTEPExactGraph {
    std::map<uint64_t, uint64_t> representation_product;
    std::map<uint64_t, std::vector<uint64_t> > representation_items;
    std::map<uint64_t, std::vector<uint64_t> > representation_invalid_references;
    std::map<uint64_t, std::vector<uint64_t> > representation_solids;
    std::map<uint64_t, std::vector<uint64_t> > representation_surface_models;
    std::map<uint64_t, std::vector<uint64_t> > representation_geometric_sets;
    std::map<uint64_t, std::vector<uint64_t> > representation_wireframe_models;
    std::map<uint64_t, uint64_t> representation_context;
    std::map<uint64_t, LazySTEPRepresentationTransform>
	representation_product_transforms;
    std::set<uint64_t> property_representations;
    std::set<uint64_t> handled_representations;
    std::set<uint64_t> handled_sdrs;
    std::set<uint64_t> handled_relationships;
    std::set<uint64_t> handled_cdsrs;
    std::set<uint64_t> geometrically_realized_usages;
    std::vector<LazySTEPOccurrence> occurrences;
};


static std::string
part21_trim(const std::string &value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::string();
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}


static std::vector<std::string>
part21_arguments(const std::string &source, const std::string &component)
{
    std::vector<std::string> result;
    size_t keyword = source.find(component);
    if (keyword == std::string::npos) return result;
    size_t open = source.find('(', keyword + component.size());
    if (open == std::string::npos) return result;
    size_t start = open + 1;
    int depth = 1;
    bool quoted = false;
    for (size_t i = start; i < source.size(); ++i) {
	const char c = source[i];
	if (quoted) {
	    if (c == '\'' && i + 1 < source.size() && source[i + 1] == '\'') {
		++i;
		continue;
	    }
	    if (c == '\'') quoted = false;
	    continue;
	}
	if (c == '\'') {
	    quoted = true;
	    continue;
	}
	if (c == '(') {
	    ++depth;
	    continue;
	}
	if (c == ')') {
	    if (--depth == 0) {
		result.push_back(part21_trim(source.substr(start, i - start)));
		return result;
	    }
	    continue;
	}
	if (c == ',' && depth == 1) {
	    result.push_back(part21_trim(source.substr(start, i - start)));
	    start = i + 1;
	}
    }
    result.clear();
    return result;
}


static std::string
part21_text(const std::string &token)
{
    const std::string value = part21_trim(token);
    if (value.size() < 2 || value.front() != '\'' || value.back() != '\'')
	return value == "$" || value == "*" ? std::string() : value;
    std::string decoded;
    for (size_t i = 1; i + 1 < value.size(); ++i) {
	if (value[i] == '\'' && i + 2 < value.size() && value[i + 1] == '\'') {
	    decoded.push_back('\'');
	    ++i;
	} else {
	    decoded.push_back(value[i]);
	}
    }
    return brlcad::step::decode_string(decoded);
}


static std::string
part21_record_value(const std::string &source)
{
    const size_t equal = source.find('=');
    if (equal == std::string::npos) return part21_trim(source);
    size_t end = source.find_last_not_of(" \t\r\n");
    if (end != std::string::npos && source[end] == ';') --end;
    if (end == std::string::npos || end <= equal) return std::string();
    return part21_trim(source.substr(equal + 1, end - equal));
}


static bool
part21_first_number(const std::string &source, double &value)
{
    for (size_t i = 0; i < source.size(); ++i) {
	const char c = source[i];
	if (!(isdigit(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.'))
	    continue;
	char *end = NULL;
	value = strtod(source.c_str() + i, &end);
	if (end && end != source.c_str() + i) return true;
    }
    return false;
}


static bool
configuration_management_type(const std::string &type)
{
    /* Later AP editions use APPLIED_* assignment names for the same
     * approval, date, person/organization, document, classification, and
     * effectivity associations expressed as CC_DESIGN_* in AP203 edition 1. */
    if (type.compare(0, 8, "APPLIED_") == 0 &&
	    (type.find("_ASSIGNMENT") != std::string::npos ||
	     type.find("DOCUMENT_REFERENCE") != std::string::npos))
	return true;
    static const char *const exact[] = {
	"ACTION", "ACTION_DIRECTIVE", "ACTION_METHOD", "ACTION_REQUEST_SOLUTION",
	"ACTION_REQUEST_STATUS", "ACTION_STATUS", "ALTERNATE_PRODUCT_RELATIONSHIP",
	"ADDRESS",
	"APPROVAL", "APPROVAL_DATE_TIME", "APPROVAL_PERSON_ORGANIZATION",
	"APPROVAL_RELATIONSHIP", "APPROVAL_ROLE", "APPROVAL_STATUS",
	"ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE", "CALENDAR_DATE", "CC_DESIGN_APPROVAL",
	"CC_DESIGN_CERTIFICATION", "CC_DESIGN_CONTRACT",
	"CC_DESIGN_DATE_AND_TIME_ASSIGNMENT", "CC_DESIGN_PERSON_AND_ORGANIZATION_ASSIGNMENT",
	"CC_DESIGN_SECURITY_CLASSIFICATION", "CC_DESIGN_SPECIFICATION_REFERENCE",
	"CERTIFICATION", "CERTIFICATION_TYPE", "CHANGE", "CHANGE_REQUEST",
	"CONFIGURATION_DESIGN", "CONFIGURATION_EFFECTIVITY", "CONFIGURATION_ITEM",
	"CONTRACT", "CONTRACT_TYPE", "COORDINATED_UNIVERSAL_TIME_OFFSET",
	"DATE_AND_TIME", "DATE_TIME_ROLE",
	"DATED_EFFECTIVITY", "DIRECTED_ACTION", "DOCUMENT", "DOCUMENT_REFERENCE",
	"DOCUMENT_RELATIONSHIP", "DOCUMENT_TYPE", "DOCUMENT_USAGE_CONSTRAINT",
	"DOCUMENT_WITH_CLASS", "EFFECTIVITY", "EFFECTIVITY_RELATIONSHIP",
	"EXECUTED_ACTION", "LOCAL_TIME",
	"LOT_EFFECTIVITY", "ORGANIZATION", "ORGANIZATIONAL_ADDRESS", "PERSON",
	"PERSONAL_ADDRESS", "PERSON_AND_ORGANIZATION",
	"PERSON_AND_ORGANIZATION_ROLE", "PRODUCT_CATEGORY",
	"PRODUCT_CATEGORY_RELATIONSHIP",
	"PRODUCT_DEFINITION_EFFECTIVITY",
	"PRODUCT_DEFINITION_WITH_ASSOCIATED_DOCUMENTS",
	"PRODUCT_RELATED_PRODUCT_CATEGORY",
	"SECURITY_CLASSIFICATION", "SECURITY_CLASSIFICATION_LEVEL",
	"SERIAL_NUMBERED_EFFECTIVITY", "START_REQUEST", "START_WORK",
	"VERSIONED_ACTION_REQUEST"
    };
    for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); ++i)
	if (type == exact[i]) return true;
    return false;
}


static std::string
configuration_attribute_prefix()
{
    const std::string schema = brlcad::step::CurrentStepSchemaRuntime().Name();
    if (schema == "ap214") return "STEP::AP214::";
    if (schema == "ap203e2") return "STEP::AP203e2::";
    if (schema.compare(0, 5, "ap242") == 0) return "STEP::AP242::";
    return "STEP::AP203::";
}


static bool
retain_configuration_record(STEPWrapper *wrapper,
    const std::string &metadata_prefix, uint64_t id)
{
    if (!wrapper || !id || id > static_cast<uint64_t>(INT64_MAX)) return false;
    const std::vector<std::string> component_types =
	wrapper->LazyComponentTypes(id);
    const std::string type = component_types.empty() ?
	wrapper->LazyTypeName(id) : "COMPLEX";
    const std::string value = part21_record_value(wrapper->LazySourceRecord(id));
    if (type.empty() || value.empty()) return false;
    brlcad::step::ConfigurationRecord &record =
	wrapper->Document().configuration_records[id];
    record.entity_id = static_cast<int64_t>(id);
    record.type = type;
    record.component_types = component_types;
    record.value = value;
    record.references.clear();
    const std::vector<uint64_t> references = wrapper->LazyForwardReferences(id);
    for (std::vector<uint64_t>::const_iterator reference = references.begin();
	    reference != references.end(); ++reference) {
	if (*reference <= static_cast<uint64_t>(INT64_MAX))
	    record.references.push_back(static_cast<int64_t>(*reference));
    }
    wrapper->Document().global_attributes[metadata_prefix + type + "::#" +
	std::to_string(id)] = value;
    const std::string structured = metadata_prefix + "CONFIGURATION::#" +
	std::to_string(id) + "::";
    wrapper->Document().global_attributes[structured + "TYPE"] = type;
    if (!component_types.empty()) {
	std::ostringstream component_list;
	for (size_t i = 0; i < component_types.size(); ++i) {
	    if (i) component_list << ' ';
	    component_list << component_types[i];
	}
	wrapper->Document().global_attributes[structured + "COMPONENT_TYPES"] =
	    component_list.str();
    }
    wrapper->Document().global_attributes[structured + "VALUE"] = value;
    std::ostringstream reference_list;
    for (size_t i = 0; i < record.references.size(); ++i) {
	if (i) reference_list << ' ';
	reference_list << record.references[i];
    }
    wrapper->Document().global_attributes[structured + "REFERENCES"] =
	reference_list.str();
    return true;
}


static bool
lot_effectivity_dependency_type(const std::string &type)
{
    if (type == "MEASURE_WITH_UNIT" || type == "DIMENSIONAL_EXPONENTS" ||
	    type == "DERIVED_UNIT_ELEMENT") return true;
    const std::string unit_suffix = "_UNIT";
    const std::string measure_suffix = "_MEASURE_WITH_UNIT";
    return (type.size() >= unit_suffix.size() && type.compare(
	type.size() - unit_suffix.size(), unit_suffix.size(), unit_suffix) == 0) ||
	(type.size() >= measure_suffix.size() && type.compare(
	 type.size() - measure_suffix.size(), measure_suffix.size(),
	 measure_suffix) == 0);
}


static bool
configuration_item_dependency_type(const std::string &type)
{
    return type == "PRODUCT_CONCEPT" || type == "PRODUCT_CONCEPT_CONTEXT" ||
	type == "APPLICATION_CONTEXT";
}


static void
lazy_add_type_ids(STEPWrapper *wrapper, std::set<uint64_t> &ids,
    const std::vector<std::string> &types)
{
    for (std::vector<std::string>::const_iterator type = types.begin();
	 type != types.end(); ++type) {
	const std::vector<uint64_t> found = wrapper->LazyInstancesByType(*type);
	ids.insert(found.begin(), found.end());
    }
}


static void
lazy_add_schema_type_ids(STEPWrapper *wrapper, std::set<uint64_t> &ids,
    const std::string &type)
{
    const std::vector<uint64_t> found =
	wrapper->LazyInstancesBySchemaType(type);
    ids.insert(found.begin(), found.end());
}


static uint64_t
lazy_reference_in_set(STEPWrapper *wrapper, uint64_t source,
    const std::set<uint64_t> &candidates)
{
    const std::vector<uint64_t> references = wrapper->LazyForwardReferences(source);
    for (std::vector<uint64_t>::const_iterator reference = references.begin();
	 reference != references.end(); ++reference) {
	if (candidates.find(*reference) != candidates.end())
	    return *reference;
    }
    return 0;
}


class LazyDisjointSets {
public:
    void add(uint64_t id)
    {
	if (parent.find(id) == parent.end()) parent[id] = id;
    }

    uint64_t find(uint64_t id)
    {
	add(id);
	uint64_t &next = parent[id];
	if (next != id) next = find(next);
	return next;
    }

    void unite(uint64_t left, uint64_t right)
    {
	left = find(left);
	right = find(right);
	if (left != right) parent[right] = left;
    }

private:
    std::map<uint64_t, uint64_t> parent;
};


static LazySTEPExactGraph
build_lazy_exact_graph(STEPWrapper *wrapper)
{
    LazySTEPExactGraph graph;
    if (!wrapper || !wrapper->HasLazyIndex())
	return graph;

    std::set<uint64_t> products;
    std::set<uint64_t> formations;
    std::set<uint64_t> definitions;
    std::set<uint64_t> definition_shapes;
    std::set<uint64_t> shape_aspects;
    std::set<uint64_t> property_definitions;
    std::set<uint64_t> representations;
    std::set<uint64_t> contexts;
    std::set<uint64_t> exact_solids;
    std::set<uint64_t> surface_models;
    std::set<uint64_t> geometric_sets;
    std::set<uint64_t> wireframe_models;
    std::set<uint64_t> axes;
    std::set<uint64_t> relationships;
    std::set<uint64_t> transforming_relationships;
    std::set<uint64_t> assembly_usages;
    std::set<uint64_t> next_assembly_usages;
    std::set<uint64_t> specified_higher_usages;
    std::set<uint64_t> quantified_usages;
    std::set<uint64_t> promissory_usages;
    std::set<uint64_t> representation_maps;
    std::set<uint64_t> mapped_items;
    std::set<uint64_t> cdsrs;
    std::set<uint64_t> transformations;

    lazy_add_type_ids(wrapper, products, {"PRODUCT"});
    lazy_add_type_ids(wrapper, formations, {"PRODUCT_DEFINITION_FORMATION",
	"PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE"});
    lazy_add_type_ids(wrapper, definitions, {"PRODUCT_DEFINITION",
	"PRODUCT_DEFINITION_WITH_ASSOCIATED_DOCUMENTS"});
    lazy_add_type_ids(wrapper, definition_shapes, {"PRODUCT_DEFINITION_SHAPE"});
    lazy_add_type_ids(wrapper, shape_aspects, {"SHAPE_ASPECT"});
    lazy_add_type_ids(wrapper, property_definitions, {"PROPERTY_DEFINITION"});
    /* Coverage discovery is schema-driven rather than converter-driven.  In
     * particular this includes AP242 tessellated and future representation
     * subtypes even when no conversion route exists yet. */
    lazy_add_schema_type_ids(wrapper, representations, "REPRESENTATION");
    lazy_add_schema_type_ids(wrapper, contexts, "REPRESENTATION_CONTEXT");
    lazy_add_type_ids(wrapper, exact_solids, {"MANIFOLD_SOLID_BREP",
	"BREP_WITH_VOIDS", "FACETED_BREP", "SOLID_REPLICA"});
    lazy_add_type_ids(wrapper, surface_models, {"SHELL_BASED_SURFACE_MODEL"});
    /* GEOMETRIC_CURVE_SET narrows GEOMETRIC_SET without changing the
     * inherited element graph.  Discover schema subtypes as well as the base
     * entity so their ownership is available to the common converter. */
    lazy_add_schema_type_ids(wrapper, geometric_sets, "GEOMETRIC_SET");
    lazy_add_type_ids(wrapper, wireframe_models, {"EDGE_BASED_WIREFRAME_MODEL",
	"SHELL_BASED_WIREFRAME_MODEL"});
    lazy_add_schema_type_ids(wrapper, axes, "PLACEMENT");
    lazy_add_schema_type_ids(wrapper, relationships, "REPRESENTATION_RELATIONSHIP");
    lazy_add_schema_type_ids(wrapper, transforming_relationships,
	"REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION");
    lazy_add_type_ids(wrapper, next_assembly_usages,
	{"NEXT_ASSEMBLY_USAGE_OCCURRENCE"});
    lazy_add_type_ids(wrapper, specified_higher_usages,
	{"SPECIFIED_HIGHER_USAGE_OCCURRENCE"});
    lazy_add_type_ids(wrapper, quantified_usages,
	{"QUANTIFIED_ASSEMBLY_COMPONENT_USAGE"});
    lazy_add_type_ids(wrapper, promissory_usages,
	{"PROMISSORY_USAGE_OCCURRENCE"});
    lazy_add_type_ids(wrapper, assembly_usages, {"NEXT_ASSEMBLY_USAGE_OCCURRENCE",
	"SPECIFIED_HIGHER_USAGE_OCCURRENCE", "QUANTIFIED_ASSEMBLY_COMPONENT_USAGE",
	"PROMISSORY_USAGE_OCCURRENCE", "ASSEMBLY_COMPONENT_USAGE",
	"PRODUCT_DEFINITION_USAGE"});
    lazy_add_type_ids(wrapper, representation_maps, {"REPRESENTATION_MAP"});
    lazy_add_type_ids(wrapper, mapped_items, {"MAPPED_ITEM"});
    lazy_add_type_ids(wrapper, cdsrs,
	{"CONTEXT_DEPENDENT_SHAPE_REPRESENTATION"});
    lazy_add_type_ids(wrapper, transformations,
	{"ITEM_DEFINED_TRANSFORMATION"});

    /* A transforming representation relationship is an assembly edge only
     * when a CONTEXT_DEPENDENT_SHAPE_REPRESENTATION actually uses it.  The
     * same legal complex entity also places a secondary representation of
     * one product, including g-step's imported-product layering output. */
    std::set<uint64_t> occurrence_relationships;
    for (std::set<uint64_t>::const_iterator cdsr = cdsrs.begin();
	 cdsr != cdsrs.end(); ++cdsr) {
	const uint64_t relationship = lazy_reference_in_set(wrapper, *cdsr,
	    transforming_relationships);
	if (relationship) occurrence_relationships.insert(relationship);
    }

    std::map<uint64_t, uint64_t> formation_product;
    for (std::set<uint64_t>::const_iterator formation = formations.begin();
	 formation != formations.end(); ++formation) {
	const uint64_t product = lazy_reference_in_set(wrapper, *formation, products);
	if (product) formation_product[*formation] = product;
    }

    std::map<uint64_t, uint64_t> definition_product;
    for (std::set<uint64_t>::const_iterator definition = definitions.begin();
	 definition != definitions.end(); ++definition) {
	const uint64_t formation = lazy_reference_in_set(wrapper, *definition, formations);
	std::map<uint64_t, uint64_t>::const_iterator product = formation_product.find(formation);
	if (product != formation_product.end()) definition_product[*definition] = product->second;
    }

    /* Retain the complete AP203 product-usage graph.  Geometry occurrence
     * construction below consumes the same records, while SHUO, quantity,
     * promissory, alternate, and substitute semantics remain available even
     * when they do not define another geometric placement. */
    for (std::set<uint64_t>::const_iterator usage_id = assembly_usages.begin();
	 usage_id != assembly_usages.end(); ++usage_id) {
	const std::vector<uint64_t> refs = wrapper->LazyForwardReferences(*usage_id);
	std::vector<uint64_t> usage_definitions;
	for (std::vector<uint64_t>::const_iterator ref = refs.begin(); ref != refs.end(); ++ref)
	    if (definitions.find(*ref) != definitions.end()) usage_definitions.push_back(*ref);
	if (usage_definitions.size() < 2) continue;
	const uint64_t parent_product = definition_product[usage_definitions[0]];
	const uint64_t child_product = definition_product[usage_definitions[1]];
	if (!parent_product || !child_product) continue;

	brlcad::step::AssemblyUsage &usage = wrapper->Document().assembly_usages[*usage_id];
	usage.entity_id = static_cast<int64_t>(*usage_id);
	usage.parent_product_id = static_cast<int64_t>(parent_product);
	usage.child_product_id = static_cast<int64_t>(child_product);
	usage.quantified = quantified_usages.find(*usage_id) != quantified_usages.end();
	usage.promissory = promissory_usages.find(*usage_id) != promissory_usages.end();
	if (specified_higher_usages.find(*usage_id) != specified_higher_usages.end())
	    usage.type = "SPECIFIED_HIGHER_USAGE_OCCURRENCE";
	else if (usage.promissory)
	    usage.type = "PROMISSORY_USAGE_OCCURRENCE";
	else if (usage.quantified)
	    usage.type = "QUANTIFIED_ASSEMBLY_COMPONENT_USAGE";
	else if (next_assembly_usages.find(*usage_id) != next_assembly_usages.end())
	    usage.type = "NEXT_ASSEMBLY_USAGE_OCCURRENCE";
	else
	    usage.type = wrapper->LazyTypeName(*usage_id);
	usage.source_record = wrapper->LazySourceRecord(*usage_id);

	std::vector<std::string> base = part21_arguments(usage.source_record,
	    "PRODUCT_DEFINITION_RELATIONSHIP");
	if (base.empty()) base = part21_arguments(usage.source_record, usage.type);
	if (base.size() >= 3) {
	    usage.identifier = part21_text(base[0]);
	    usage.name = part21_text(base[1]);
	    usage.description = part21_text(base[2]);
	}
	std::vector<std::string> assembly = part21_arguments(usage.source_record,
	    "ASSEMBLY_COMPONENT_USAGE");
	if (!assembly.empty()) usage.reference_designator = part21_text(assembly[0]);
	else if (base.size() >= 6) usage.reference_designator = part21_text(base[5]);

	if (specified_higher_usages.find(*usage_id) != specified_higher_usages.end()) {
	    for (std::vector<uint64_t>::const_iterator ref = refs.begin(); ref != refs.end(); ++ref) {
		if (*ref == usage_definitions[0] || *ref == usage_definitions[1] ||
			assembly_usages.find(*ref) == assembly_usages.end()) continue;
		if (!usage.upper_usage_id) usage.upper_usage_id = static_cast<int64_t>(*ref);
		else if (!usage.next_usage_id) usage.next_usage_id = static_cast<int64_t>(*ref);
	    }
	}

	if (usage.quantified) {
	    for (std::vector<uint64_t>::const_reverse_iterator ref = refs.rbegin();
		 ref != refs.rend(); ++ref) {
		if (definitions.find(*ref) != definitions.end() ||
			assembly_usages.find(*ref) != assembly_usages.end()) continue;
		double quantity = 0.0;
		if (!part21_first_number(part21_record_value(wrapper->LazySourceRecord(*ref)),
			quantity)) continue;
		usage.quantity = quantity;
		const std::vector<uint64_t> measure_refs = wrapper->LazyForwardReferences(*ref);
		if (!measure_refs.empty()) {
		    usage.quantity_unit = wrapper->LazyTypeName(measure_refs.back());
		    if (usage.quantity_unit.empty())
			usage.quantity_unit = part21_record_value(
			    wrapper->LazySourceRecord(measure_refs.back()));
		}
		break;
	    }
	}
    }

    std::set<uint64_t> alternate_relationships;
    lazy_add_type_ids(wrapper, alternate_relationships, {"ALTERNATE_PRODUCT_RELATIONSHIP"});
    for (std::set<uint64_t>::const_iterator relationship = alternate_relationships.begin();
	 relationship != alternate_relationships.end(); ++relationship) {
	const std::vector<uint64_t> refs = wrapper->LazyForwardReferences(*relationship);
	std::vector<uint64_t> related_products;
	for (std::vector<uint64_t>::const_iterator ref = refs.begin(); ref != refs.end(); ++ref)
	    if (products.find(*ref) != products.end()) related_products.push_back(*ref);
	if (related_products.size() < 2) continue;
	brlcad::step::ProductAlternative &alternate =
	    wrapper->Document().product_alternatives[*relationship];
	alternate.entity_id = static_cast<int64_t>(*relationship);
	/* EXPRESS order is alternate, base. */
	alternate.alternate_product_id = static_cast<int64_t>(related_products[0]);
	alternate.base_product_id = static_cast<int64_t>(related_products[1]);
	alternate.source_record = wrapper->LazySourceRecord(*relationship);
	const std::vector<std::string> args = part21_arguments(alternate.source_record,
	    "ALTERNATE_PRODUCT_RELATIONSHIP");
	if (args.size() >= 5) alternate.basis = part21_text(args[4]);
    }

    std::set<uint64_t> substitute_relationships;
    lazy_add_type_ids(wrapper, substitute_relationships,
	{"ASSEMBLY_COMPONENT_USAGE_SUBSTITUTE"});
    for (std::set<uint64_t>::const_iterator relationship = substitute_relationships.begin();
	 relationship != substitute_relationships.end(); ++relationship) {
	const std::vector<uint64_t> refs = wrapper->LazyForwardReferences(*relationship);
	std::vector<uint64_t> related_usages;
	for (std::vector<uint64_t>::const_iterator ref = refs.begin(); ref != refs.end(); ++ref)
	    if (assembly_usages.find(*ref) != assembly_usages.end()) related_usages.push_back(*ref);
	if (related_usages.size() < 2) continue;
	brlcad::step::UsageSubstitute &substitute =
	    wrapper->Document().usage_substitutes[*relationship];
	substitute.entity_id = static_cast<int64_t>(*relationship);
	substitute.base_usage_id = static_cast<int64_t>(related_usages[0]);
	substitute.substitute_usage_id = static_cast<int64_t>(related_usages[1]);
	substitute.source_record = wrapper->LazySourceRecord(*relationship);
    }

    std::map<uint64_t, uint64_t> shape_product;
    for (std::set<uint64_t>::const_iterator shape = definition_shapes.begin();
	 shape != definition_shapes.end(); ++shape) {
	const uint64_t definition = lazy_reference_in_set(wrapper, *shape, definitions);
	std::map<uint64_t, uint64_t>::const_iterator product = definition_product.find(definition);
	if (product != definition_product.end()) shape_product[*shape] = product->second;
    }

    /* Appendix J of RP8 documents the pre-July-1997 convention for a part
     * composed from several shape representations: only the primary SDR is
     * attached directly to PRODUCT_DEFINITION_SHAPE; secondary SDRs reach the
     * same product through SHAPE_ASPECT (and, in some schema encodings, an
     * intervening PROPERTY_DEFINITION).  Accept both historical encodings so
     * all representations contribute geometry to the owning product. */
    std::map<uint64_t, uint64_t> aspect_product;
    for (std::set<uint64_t>::const_iterator aspect = shape_aspects.begin();
	 aspect != shape_aspects.end(); ++aspect) {
	const uint64_t shape = lazy_reference_in_set(wrapper, *aspect,
	    definition_shapes);
	std::map<uint64_t, uint64_t>::const_iterator product = shape_product.find(shape);
	if (product != shape_product.end()) aspect_product[*aspect] = product->second;
    }
    std::map<uint64_t, uint64_t> property_product;
    for (std::set<uint64_t>::const_iterator property = property_definitions.begin();
	 property != property_definitions.end(); ++property) {
	uint64_t product_id = 0;
	const uint64_t aspect = lazy_reference_in_set(wrapper, *property, shape_aspects);
	std::map<uint64_t, uint64_t>::const_iterator aspect_owner =
	    aspect_product.find(aspect);
	if (aspect_owner != aspect_product.end()) product_id = aspect_owner->second;
	if (!product_id) {
	    const uint64_t shape = lazy_reference_in_set(wrapper, *property,
		definition_shapes);
	    std::map<uint64_t, uint64_t>::const_iterator shape_owner =
		shape_product.find(shape);
	    if (shape_owner != shape_product.end()) product_id = shape_owner->second;
	}
	if (!product_id) {
	    const uint64_t definition = lazy_reference_in_set(wrapper, *property,
		definitions);
	    std::map<uint64_t, uint64_t>::const_iterator definition_owner =
		definition_product.find(definition);
	    if (definition_owner != definition_product.end())
		product_id = definition_owner->second;
	}
	if (product_id) property_product[*property] = product_id;
    }

    std::set<uint64_t> sdrs;
    lazy_add_schema_type_ids(wrapper, sdrs,
	"PROPERTY_DEFINITION_REPRESENTATION");
    std::map<uint64_t, uint64_t> sdr_representation;
    for (std::set<uint64_t>::const_iterator sdr = sdrs.begin(); sdr != sdrs.end(); ++sdr) {
	const uint64_t shape = lazy_reference_in_set(wrapper, *sdr, definition_shapes);
	const uint64_t aspect = lazy_reference_in_set(wrapper, *sdr, shape_aspects);
	const uint64_t property = lazy_reference_in_set(wrapper, *sdr,
	    property_definitions);
	const uint64_t representation = lazy_reference_in_set(wrapper, *sdr, representations);
	uint64_t product_id = 0;
	std::map<uint64_t, uint64_t>::const_iterator product = shape_product.find(shape);
	if (product != shape_product.end()) product_id = product->second;
	if (!product_id) {
	    std::map<uint64_t, uint64_t>::const_iterator owner =
		aspect_product.find(aspect);
	    if (owner != aspect_product.end()) product_id = owner->second;
	}
	if (!product_id) {
	    std::map<uint64_t, uint64_t>::const_iterator owner =
		property_product.find(property);
	    if (owner != property_product.end()) product_id = owner->second;
	}
	if (!representation || !product_id) continue;
	sdr_representation[*sdr] = representation;
	graph.representation_product[representation] = product_id;
	/* SHAPE_DEFINITION_REPRESENTATION is a subtype of
	 * PROPERTY_DEFINITION_REPRESENTATION.  AP203 RP8 Appendix J also permits
	 * its definition to be a PROPERTY_DEFINITION wrapping a SHAPE_ASPECT, so
	 * the direct-reference tests alone cannot distinguish that geometric
	 * hierarchy route from a material/property representation. */
	if (property && !shape && !aspect &&
		wrapper->LazyTypeName(*sdr) != "SHAPE_DEFINITION_REPRESENTATION") {
	    graph.property_representations.insert(representation);
	    /* This link describes validation, material, or other product
	     * properties, never shape geometry.  Claim it even if its companion
	     * schema item cannot be materialized by the selected AP registry; the
	     * schema-specific metadata importer may process supported forms later. */
	    graph.handled_sdrs.insert(*sdr);
	}
    }

    const std::map<uint64_t, uint64_t> directly_owned_representations =
	graph.representation_product;

    /* A representation relationship joins alternate representations of the
     * same product unless a CDSR identifies it as an occurrence edge.  A
     * transforming relationship without that occurrence context must still
     * propagate product ownership, while conflicting directly owned products
     * remain isolated below. */
    LazyDisjointSets components;
    for (std::set<uint64_t>::const_iterator representation = representations.begin();
	 representation != representations.end(); ++representation)
	components.add(*representation);
    for (std::set<uint64_t>::const_iterator relationship = relationships.begin();
	 relationship != relationships.end(); ++relationship) {
	if (occurrence_relationships.find(*relationship) !=
		occurrence_relationships.end())
	    continue;
	const std::vector<uint64_t> references = wrapper->LazyForwardReferences(*relationship);
	uint64_t first = 0;
	for (std::vector<uint64_t>::const_iterator reference = references.begin();
	     reference != references.end(); ++reference) {
	    if (representations.find(*reference) == representations.end()) continue;
	    if (!first) first = *reference;
	    else components.unite(first, *reference);
	}
    }

    std::map<uint64_t, uint64_t> component_product;
    std::set<uint64_t> conflicting_components;
    for (std::map<uint64_t, uint64_t>::const_iterator mapping = graph.representation_product.begin();
	 mapping != graph.representation_product.end(); ++mapping) {
	const uint64_t component = components.find(mapping->first);
	std::map<uint64_t, uint64_t>::iterator known = component_product.find(component);
	if (known == component_product.end()) component_product[component] = mapping->second;
	else if (known->second != mapping->second) conflicting_components.insert(component);
    }
    for (std::set<uint64_t>::const_iterator representation = representations.begin();
	 representation != representations.end(); ++representation) {
	const uint64_t component = components.find(*representation);
	std::map<uint64_t, uint64_t>::const_iterator product = component_product.find(component);
	if (product != component_product.end() &&
	    conflicting_components.find(component) == conflicting_components.end())
	    graph.representation_product[*representation] = product->second;
    }

    /* Retain the coordinate mapping for a secondary representation which was
     * not itself attached by an SDR.  EXPRESS attribute order associates the
     * two ITEM_DEFINED_TRANSFORMATION placements with rep_1 and rep_2. */
    for (std::set<uint64_t>::const_iterator relationship =
	    transforming_relationships.begin();
	 relationship != transforming_relationships.end(); ++relationship) {
	if (occurrence_relationships.find(*relationship) !=
		occurrence_relationships.end())
	    continue;
	std::vector<uint64_t> relation_representations;
	uint64_t transformation = 0;
	const std::vector<uint64_t> relation_refs =
	    wrapper->LazyForwardReferences(*relationship);
	for (std::vector<uint64_t>::const_iterator reference =
		relation_refs.begin(); reference != relation_refs.end(); ++reference) {
	    if (representations.find(*reference) != representations.end())
		relation_representations.push_back(*reference);
	    if (transformations.find(*reference) != transformations.end())
		transformation = *reference;
	}
	if (relation_representations.size() != 2 || !transformation) continue;
	std::vector<uint64_t> transform_axes;
	const std::vector<uint64_t> transform_refs =
	    wrapper->LazyForwardReferences(transformation);
	for (std::vector<uint64_t>::const_iterator reference =
		transform_refs.begin(); reference != transform_refs.end(); ++reference) {
	    if (axes.find(*reference) != axes.end() &&
		    wrapper->LazyTypeName(*reference) == "AXIS2_PLACEMENT_3D")
		transform_axes.push_back(*reference);
	}
	if (transform_axes.size() != 2) continue;
	const uint64_t rep1 = relation_representations[0];
	const uint64_t rep2 = relation_representations[1];
	const std::map<uint64_t, uint64_t>::const_iterator direct1 =
	    directly_owned_representations.find(rep1);
	const std::map<uint64_t, uint64_t>::const_iterator direct2 =
	    directly_owned_representations.find(rep2);
	const std::map<uint64_t, uint64_t>::const_iterator owner1 =
	    graph.representation_product.find(rep1);
	const std::map<uint64_t, uint64_t>::const_iterator owner2 =
	    graph.representation_product.find(rep2);
	uint64_t secondary = 0;
	LazySTEPRepresentationTransform placement;
	placement.relationship_id = *relationship;
	if (direct1 != directly_owned_representations.end() &&
		direct2 == directly_owned_representations.end() &&
		owner2 != graph.representation_product.end() &&
		owner2->second == direct1->second) {
	    secondary = rep2;
	    placement.parent_axis_id = transform_axes[0];
	    placement.child_axis_id = transform_axes[1];
	} else if (direct2 != directly_owned_representations.end() &&
		direct1 == directly_owned_representations.end() &&
		owner1 != graph.representation_product.end() &&
		owner1->second == direct2->second) {
	    secondary = rep1;
	    placement.parent_axis_id = transform_axes[1];
	    placement.child_axis_id = transform_axes[0];
	}
	if (secondary)
	    graph.representation_product_transforms[secondary] = placement;
    }

    /* Decide whether a representation's supported geometry can be routed
     * entirely from the zero-copy index.  This is deliberately a
     * per-representation decision, not a component-wide one.  A common STEP
     * layout relates a small product placement to separate advanced-BREP,
     * manifold-surface, and bounded-surface representations.  Treating the
     * unsupported bounded-surface sibling as making the entire relationship
     * component unsafe forces materialization of every face in the other two
     * representations merely to rediscover their product identity. */
    for (std::set<uint64_t>::const_iterator representation = representations.begin();
	 representation != representations.end(); ++representation) {
	const uint64_t component = components.find(*representation);
	const bool delegated_tessellation =
	    wrapper->LazyTypeName(*representation) ==
	    "TESSELLATED_SHAPE_REPRESENTATION";
	const bool property_metadata = graph.property_representations.find(
	    *representation) != graph.property_representations.end();
	bool has_lazy_geometry = false;
	bool lazy_safe = true;
	const std::vector<uint64_t> references = wrapper->LazyForwardReferences(*representation);
	for (std::vector<uint64_t>::const_iterator reference = references.begin();
	     reference != references.end(); ++reference) {
	    if (wrapper->LazyIsSchemaEntity(*reference, "REPRESENTATION_ITEM")) {
		graph.representation_items[*representation].push_back(*reference);
		if (property_metadata) continue;
	    }
	    /* AP242 tessellations are decoded by the schema post-index hook.  The
	     * common lazy graph still owns their product, item, context, coverage,
	     * and occurrence routing, so suppress the legacy generated-class walk
	     * that otherwise tries (and fails) to materialize this representation
	     * through the BRep-oriented Factory. */
	    if (delegated_tessellation &&
		    wrapper->LazyIsSchemaEntity(*reference, "TESSELLATED_ITEM")) {
		has_lazy_geometry = true;
		continue;
	    }
	    if (exact_solids.find(*reference) != exact_solids.end()) {
		graph.representation_solids[*representation].push_back(*reference);
		has_lazy_geometry = true;
		continue;
	    }
	    if (surface_models.find(*reference) != surface_models.end()) {
		graph.representation_surface_models[*representation].push_back(*reference);
		has_lazy_geometry = true;
		continue;
	    }
	    if (geometric_sets.find(*reference) != geometric_sets.end()) {
		/* Geometric sets still use their schema adapter for bounded
		 * surfaces and wires.  Index their ownership and name here, but do
		 * not claim their containing representation is fully lazy. */
		graph.representation_geometric_sets[*representation].push_back(*reference);
		lazy_safe = false;
		continue;
	    }
	    if (wireframe_models.find(*reference) != wireframe_models.end()) {
		graph.representation_wireframe_models[*representation].push_back(*reference);
		has_lazy_geometry = true;
		continue;
	    }
	    if (contexts.find(*reference) != contexts.end()) {
		/* context_of_items is singular; retain the first scanner-order hit. */
		if (!graph.representation_context[*representation])
		    graph.representation_context[*representation] = *reference;
		continue;
	    }
	    if (axes.find(*reference) != axes.end()) continue;
	    if (!wrapper->LazyIsSchemaEntity(*reference, "REPRESENTATION_ITEM")) {
		/* Some STEPcode registries do not report an IsA result for a Part 21
		 * complex representation context.  REPRESENTATION has no other
		 * entity-valued attribute after its item aggregate, so a resolved
		 * non-item reference is still unambiguously context_of_items. */
		if (!wrapper->LazyTypeName(*reference).empty() &&
			!graph.representation_context[*representation])
		    graph.representation_context[*representation] = *reference;
		else if (wrapper->LazyTypeName(*reference).empty())
		    graph.representation_invalid_references[*representation].push_back(*reference);
		continue;
	    }
	    lazy_safe = false;
	}
	if (property_metadata &&
	    graph.representation_product.find(*representation) !=
		graph.representation_product.end())
	    graph.handled_representations.insert(*representation);
	else if (has_lazy_geometry && lazy_safe &&
	    graph.representation_product.find(*representation) !=
		graph.representation_product.end() &&
	    conflicting_components.find(component) == conflicting_components.end())
	    graph.handled_representations.insert(*representation);
    }
    for (std::map<uint64_t, uint64_t>::const_iterator link = sdr_representation.begin();
	 link != sdr_representation.end(); ++link) {
	if (graph.handled_representations.find(link->second) !=
		graph.handled_representations.end())
	    graph.handled_sdrs.insert(link->first);
    }
    for (std::set<uint64_t>::const_iterator relationship = relationships.begin();
	 relationship != relationships.end(); ++relationship) {
	if (occurrence_relationships.find(*relationship) !=
		occurrence_relationships.end())
	    continue;
	bool has_handled_representation = false;
	bool has_conflict = false;
	const std::vector<uint64_t> references = wrapper->LazyForwardReferences(*relationship);
	for (std::vector<uint64_t>::const_iterator reference = references.begin();
	     reference != references.end(); ++reference) {
	    if (representations.find(*reference) == representations.end()) continue;
	    if (conflicting_components.find(components.find(*reference)) !=
		    conflicting_components.end())
		has_conflict = true;
	    if (graph.handled_representations.find(*reference) !=
		    graph.handled_representations.end())
		has_handled_representation = true;
	}
	if (has_handled_representation && !has_conflict)
	    graph.handled_relationships.insert(*relationship);
    }

    /* Detach static assembly identity without materializing the relationship
     * (whose dependency closure includes both complete product shapes). */
    for (std::set<uint64_t>::const_iterator cdsr = cdsrs.begin(); cdsr != cdsrs.end(); ++cdsr) {
	const uint64_t relationship = lazy_reference_in_set(wrapper, *cdsr,
	    transforming_relationships);
	const uint64_t shape = lazy_reference_in_set(wrapper, *cdsr, definition_shapes);
	const uint64_t usage = lazy_reference_in_set(wrapper, shape, assembly_usages);
	if (!relationship || !usage) continue;

	std::vector<uint64_t> relation_representations;
	uint64_t transformation = 0;
	const std::vector<uint64_t> relation_refs = wrapper->LazyForwardReferences(relationship);
	for (std::vector<uint64_t>::const_iterator reference = relation_refs.begin();
	     reference != relation_refs.end(); ++reference) {
	    if (representations.find(*reference) != representations.end())
		relation_representations.push_back(*reference);
	    if (transformations.find(*reference) != transformations.end())
		transformation = *reference;
	}
	if (relation_representations.size() != 2 || !transformation) continue;

	std::vector<uint64_t> usage_definitions;
	const std::vector<uint64_t> usage_refs = wrapper->LazyForwardReferences(usage);
	for (std::vector<uint64_t>::const_iterator reference = usage_refs.begin();
	     reference != usage_refs.end(); ++reference) {
	    if (definitions.find(*reference) != definitions.end())
		usage_definitions.push_back(*reference);
	}
	std::vector<uint64_t> transform_axes;
	const std::vector<uint64_t> transform_refs = wrapper->LazyForwardReferences(transformation);
	for (std::vector<uint64_t>::const_iterator reference = transform_refs.begin();
	     reference != transform_refs.end(); ++reference) {
	    if (axes.find(*reference) != axes.end() &&
		wrapper->LazyTypeName(*reference) == "AXIS2_PLACEMENT_3D")
		transform_axes.push_back(*reference);
	}
	if (usage_definitions.size() != 2 || transform_axes.size() != 2) continue;

	const uint64_t parent_product = definition_product[usage_definitions[0]];
	const uint64_t child_product = definition_product[usage_definitions[1]];
	if (!parent_product || !child_product) continue;
	const std::map<uint64_t, uint64_t>::const_iterator rep1_owner =
	    graph.representation_product.find(relation_representations[0]);
	const std::map<uint64_t, uint64_t>::const_iterator rep2_owner =
	    graph.representation_product.find(relation_representations[1]);
	const uint64_t rep1_product = rep1_owner == graph.representation_product.end() ?
	    0 : rep1_owner->second;
	const uint64_t rep2_product = rep2_owner == graph.representation_product.end() ?
	    0 : rep2_owner->second;
	LazySTEPOccurrence occurrence;
	occurrence.entity_id = *cdsr;
	occurrence.usage_id = usage;
	occurrence.relationship_id = relationship;
	occurrence.shape_method = "referenced";
	occurrence.parent_product_id = parent_product;
	occurrence.child_product_id = child_product;
	if (rep1_product == parent_product && rep2_product == child_product) {
	    occurrence.parent_representation_id = relation_representations[0];
	    occurrence.child_representation_id = relation_representations[1];
	    occurrence.parent_axis_id = transform_axes[0];
	    occurrence.child_axis_id = transform_axes[1];
	} else if (rep2_product == parent_product && rep1_product == child_product) {
	    occurrence.parent_representation_id = relation_representations[1];
	    occurrence.child_representation_id = relation_representations[0];
	    occurrence.parent_axis_id = transform_axes[1];
	    occurrence.child_axis_id = transform_axes[0];
	} else {
	    continue;
	}
	graph.occurrences.push_back(occurrence);
	graph.geometrically_realized_usages.insert(usage);
	graph.handled_cdsrs.insert(*cdsr);
	graph.handled_relationships.insert(relationship);
    }

    /* AP203 mapped assembly shape: a MAPPED_ITEM in the parent's
     * representation points through a REPRESENTATION_MAP to the child's
     * represented shape.  Treat a cross-product map as an assembly
     * occurrence; same-product maps remain ordinary geometry reuse. */
    for (std::set<uint64_t>::const_iterator mapped = mapped_items.begin();
	 mapped != mapped_items.end(); ++mapped) {
	const uint64_t map_id = lazy_reference_in_set(wrapper, *mapped, representation_maps);
	if (!map_id) continue;
	const uint64_t child_representation = lazy_reference_in_set(wrapper, map_id,
	    representations);
	const std::vector<uint64_t> parents = wrapper->LazyReverseReferences(*mapped);
	uint64_t parent_representation = 0;
	for (std::vector<uint64_t>::const_iterator parent = parents.begin();
	     parent != parents.end(); ++parent) {
	    if (representations.find(*parent) != representations.end()) {
		parent_representation = *parent;
		break;
	    }
	}
	const std::map<uint64_t, uint64_t>::const_iterator parent_owner =
	    graph.representation_product.find(parent_representation);
	const std::map<uint64_t, uint64_t>::const_iterator child_owner =
	    graph.representation_product.find(child_representation);
	const uint64_t parent_product = parent_owner == graph.representation_product.end() ?
	    0 : parent_owner->second;
	const uint64_t child_product = child_owner == graph.representation_product.end() ?
	    0 : child_owner->second;
	if (!parent_product || !child_product || parent_product == child_product) continue;

	uint64_t usage_id = 0;
	for (std::set<uint64_t>::const_iterator usage = next_assembly_usages.begin();
	     usage != next_assembly_usages.end(); ++usage) {
	    if (graph.geometrically_realized_usages.find(*usage) !=
		    graph.geometrically_realized_usages.end()) continue;
	    std::map<int64_t, brlcad::step::AssemblyUsage>::const_iterator retained =
		wrapper->Document().assembly_usages.find(static_cast<int64_t>(*usage));
	    if (retained != wrapper->Document().assembly_usages.end() &&
		    retained->second.parent_product_id == static_cast<int64_t>(parent_product) &&
		    retained->second.child_product_id == static_cast<int64_t>(child_product)) {
		usage_id = *usage;
		break;
	    }
	}
	if (!usage_id) continue;

	const uint64_t origin_axis = lazy_reference_in_set(wrapper, map_id, axes);
	const uint64_t target_axis = lazy_reference_in_set(wrapper, *mapped, axes);
	if (!origin_axis || !target_axis) continue;
	LazySTEPOccurrence occurrence;
	occurrence.entity_id = usage_id;
	occurrence.usage_id = usage_id;
	occurrence.parent_representation_id = parent_representation;
	occurrence.child_representation_id = child_representation;
	occurrence.parent_product_id = parent_product;
	occurrence.child_product_id = child_product;
	occurrence.parent_axis_id = target_axis;
	occurrence.child_axis_id = origin_axis;
	occurrence.mapped = true;
	occurrence.shape_method = "mapped";
	graph.occurrences.push_back(occurrence);
	graph.geometrically_realized_usages.insert(usage_id);
    }

    /* Replicated assembly shape carries no separate transformation
     * relationship.  Its NAUO still defines the product occurrence, so retain
     * it as an identity child member rather than dropping the hierarchy. */
    for (std::set<uint64_t>::const_iterator usage_id = next_assembly_usages.begin();
	 usage_id != next_assembly_usages.end(); ++usage_id) {
	if (graph.geometrically_realized_usages.find(*usage_id) !=
		graph.geometrically_realized_usages.end()) continue;
	std::map<int64_t, brlcad::step::AssemblyUsage>::const_iterator usage =
	    wrapper->Document().assembly_usages.find(static_cast<int64_t>(*usage_id));
	if (usage == wrapper->Document().assembly_usages.end() ||
		usage->second.parent_product_id <= 0 || usage->second.child_product_id <= 0)
	    continue;
	LazySTEPOccurrence occurrence;
	occurrence.entity_id = *usage_id;
	occurrence.usage_id = *usage_id;
	occurrence.parent_product_id = static_cast<uint64_t>(usage->second.parent_product_id);
	occurrence.child_product_id = static_cast<uint64_t>(usage->second.child_product_id);
	occurrence.shape_method = "replicated";
	graph.occurrences.push_back(occurrence);
	graph.geometrically_realized_usages.insert(*usage_id);
    }

    const std::string metadata_prefix = configuration_attribute_prefix();
    for (std::map<std::string, uint64_t>::const_iterator type =
	    wrapper->Document().entity_counts.begin();
	 type != wrapper->Document().entity_counts.end(); ++type) {
	if (!configuration_management_type(type->first)) continue;
	/* Retention on _GLOBAL is an implemented import outcome, not an
	 * unsupported entity.  Leave unrecognized approval/document families in
	 * unsupported_counts instead of suppressing them categorically. */
	wrapper->Document().unsupported_counts.erase(type->first);
	const std::vector<uint64_t> ids = wrapper->LazyInstancesByType(type->first);
	for (std::vector<uint64_t>::const_iterator id = ids.begin(); id != ids.end(); ++id)
	    retain_configuration_record(wrapper, metadata_prefix, *id);
    }

    /* AP203 CONFIGURATION_ITEM reaches its PRODUCT_CONCEPT, then the
     * PRODUCT_CONCEPT_CONTEXT and APPLICATION_CONTEXT.  These are ordinary
     * product/application records elsewhere in a model, so retain only the
     * dependency closure of actual configuration items.  Without this
     * closure a preserved complex CONFIGURATION_EFFECTIVITY cannot later be
     * authored without inventing its configuration semantics. */
    std::set<uint64_t> pending_configuration_dependencies;
    const std::vector<uint64_t> configuration_items =
	wrapper->LazyInstancesByType("CONFIGURATION_ITEM");
    for (std::vector<uint64_t>::const_iterator item = configuration_items.begin();
	    item != configuration_items.end(); ++item) {
	const std::vector<uint64_t> references =
	    wrapper->LazyForwardReferences(*item);
	pending_configuration_dependencies.insert(references.begin(),
	    references.end());
    }
    std::set<uint64_t> visited_configuration_dependencies;
    while (!pending_configuration_dependencies.empty()) {
	const uint64_t id = *pending_configuration_dependencies.begin();
	pending_configuration_dependencies.erase(
	    pending_configuration_dependencies.begin());
	if (!visited_configuration_dependencies.insert(id).second) continue;
	const std::string type = wrapper->LazyTypeName(id);
	if (!configuration_item_dependency_type(type)) continue;
	if (!retain_configuration_record(wrapper, metadata_prefix, id)) continue;
	wrapper->Document().unsupported_counts.erase(type);
	const std::vector<uint64_t> references =
	    wrapper->LazyForwardReferences(id);
	pending_configuration_dependencies.insert(references.begin(),
	    references.end());
    }

    /* LOT_EFFECTIVITY owns a measure-with-unit subgraph that is not itself
     * configuration-management data.  Retain only the dependency closure of
     * actual lot records; retaining every measure and unit in the file would
     * incorrectly turn unrelated geometry/property data into configuration
     * export requests. */
    std::set<uint64_t> pending_lot_dependencies;
    const std::vector<uint64_t> lots =
	wrapper->LazyInstancesByType("LOT_EFFECTIVITY");
    for (std::vector<uint64_t>::const_iterator lot = lots.begin();
	    lot != lots.end(); ++lot) {
	const std::vector<uint64_t> references =
	    wrapper->LazyForwardReferences(*lot);
	pending_lot_dependencies.insert(references.begin(), references.end());
    }
    std::set<uint64_t> visited_lot_dependencies;
    while (!pending_lot_dependencies.empty()) {
	const uint64_t id = *pending_lot_dependencies.begin();
	pending_lot_dependencies.erase(pending_lot_dependencies.begin());
	if (!visited_lot_dependencies.insert(id).second) continue;
	const std::string type = wrapper->LazyTypeName(id);
	if (!lot_effectivity_dependency_type(type)) continue;
	if (!retain_configuration_record(wrapper, metadata_prefix, id)) continue;
	const std::vector<uint64_t> references =
	    wrapper->LazyForwardReferences(id);
	pending_lot_dependencies.insert(references.begin(), references.end());
    }
    const std::string runtime_schema =
	brlcad::step::CurrentStepSchemaRuntime().Name();
    std::string file_schema = "CONFIG_CONTROL_DESIGN";
    if (runtime_schema == "ap214") file_schema = "AUTOMOTIVE_DESIGN";
    else if (runtime_schema == "ap203e2")
	file_schema = "AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF";
    else if (runtime_schema.compare(0, 5, "ap242") == 0)
	file_schema = "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF";
    wrapper->Document().global_attributes[metadata_prefix + "FILE_SCHEMA"] =
	file_schema;
    return graph;
}


static std::vector<uint64_t>
lazy_ids(const std::set<uint64_t> &source)
{
    return std::vector<uint64_t>(source.begin(), source.end());
}
#endif


static void
record_brep_result(STEPWrapper *wrapper, BrepWriteStatus status, int entity_id,
	const std::string &entity_type, bool preserved_invalid = false,
	const std::string &invalidity = std::string())
{
    if (!wrapper || !wrapper->BeginGeometryResult(entity_id))
	return;
    brlcad::step::ImportStatistics &stats = wrapper->Statistics();
    ++stats.geometry_attempted;
    const bool invalid = !invalidity.empty();
    if (invalid)
	++stats.invalid_breps;
    if (status == BREP_WRITE_SUCCESS || preserved_invalid) {
	++stats.geometry_written;
	wrapper->RecordRepresentationItemCoverage(entity_id,
	    preserved_invalid ?
	    brlcad::step::RepresentationCoverageStatus::PreservedInvalid :
	    brlcad::step::RepresentationCoverageStatus::Handled,
	    preserved_invalid ?
	    "preserved as an unresolved invalid import after valid recovery paths failed" :
	    "converted successfully");
	if (preserved_invalid)
	    ++stats.invalid_breps_written;
	if (preserved_invalid) {
	    const std::string message = invalidity == "not_solid" ?
		"preserved unresolved importer BREP which did not validate as a solid" :
		invalidity == "authored_open_topology" ?
		"preserved a storage-safe open importer BREP after proving every "
		"remaining boundary edge has exactly one authored source use" :
		invalidity == "partial_construction" ?
		"preserved a storage-safe partial importer BREP after exact construction and valid recovery paths failed" :
		invalidity == "repair_timeout" ?
		"preserved a storage-safe pre-repair importer BREP after bounded repair was interrupted" :
		"preserved unresolved importer BREP which failed OpenNURBS structural validation";
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		entity_id, entity_type, invalidity, message);
	}
	return;
    }

    ++stats.geometry_skipped;
    if (invalid && (status == BREP_INVALID_STRUCTURE || status == BREP_NOT_SOLID))
	++stats.invalid_breps_rejected;
    std::string message;
    switch (status) {
	case BREP_CONVERSION_FAILED:
	    message = "exact OpenNURBS conversion failed";
	    break;
	case BREP_INVALID_STRUCTURE:
	    message = "OpenNURBS structural validation failed";
	    break;
	case BREP_NOT_SOLID:
	    message = "closed STEP BREP did not validate as a solid";
	    break;
	case BREP_OUTPUT_FAILED:
	    ++stats.output_failures;
	    message = "BRL-CAD database write failed";
	    break;
	default:
	    message = "unknown BREP conversion failure";
	    break;
    }
    wrapper->RecordSkippedItem(entity_id, entity_type, message);
    wrapper->RecordRepresentationItemCoverage(entity_id,
	brlcad::step::RepresentationCoverageStatus::Skipped, message);
    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, entity_id,
	entity_type, std::string(), message);
}


struct DetachedBrepMember {
    std::string combination;
    mat_t matrix;
    int representation_map_id = 0;

    DetachedBrepMember()
    {
	MAT_IDN(matrix);
    }
};


struct DetachedBrepJob {
    int entity_id = 0;
    /** Product-bound representation item accounted by this job.  This may
     * differ from entity_id when one shell is detached from a surface model. */
    int coverage_entity_id = 0;
    int output_source_id = 0;
    int product_id = 0;
    std::string entity_type;
    std::string original_name;
    std::string output_name;
    mat_t write_matrix;
    double length_factor = 1.0;
    double planeangle_factor = 1.0;
    double solidangle_factor = 1.0;
    double tolerance = 1.0e-6;
    double representation_tolerance = 1.0e-6;
    bool faceted = false;
    bool is_region = true;
    bool ready = false;
    uint64_t remaining_work_milliseconds = kMaximumExactPullbackMilliseconds;
    uint64_t work_budget_milliseconds = kMaximumExactPullbackMilliseconds;
    size_t topology_face_count = 0;
    size_t maximum_pullback_span_estimate = 1;
    bool has_style = false;
    brlcad::step::Style style;
    SolidModel *solid = NULL;
    ShellBasedSurfaceModel *surface_model = NULL;
    ConnectedFaceSet *face_set = NULL;
    Face *face = NULL;
    std::unique_ptr<STEPDetachedEntityArena> entity_arena;
    std::unique_ptr<ON_Brep> brep;
    std::vector<fastf_t> bot_vertices;
    std::vector<int> bot_faces;
    std::vector<DetachedBrepMember> members;
    BrepWriteStatus status = BREP_CONVERSION_FAILED;
    bool preserved_invalid = false;
    std::string invalidity;
    std::string invalid_reason;
    std::vector<brlcad::step::InferredCurve> inferred_curves;
    std::shared_ptr<STEPWrapper::CurveInferenceTransaction>
	accepted_inference_transaction;
    std::string validation_message;

    DetachedBrepJob()
    {
	MAT_IDN(write_matrix);
    }
};


static bool
invalid_brep_status(BrepWriteStatus status)
{
    return status == BREP_INVALID_STRUCTURE || status == BREP_NOT_SOLID;
}


/* Validation failures are intentionally allowed here, but candidates with
 * missing owned geometry or broken array references are not safe to hand to
 * the database serializer or downstream diagnostic commands. */
static bool
brep_candidate_storage_safe(const ON_Brep *brep)
{
    if (!brep || brep->m_F.Count() == 0 || brep->m_S.Count() == 0)
	return false;
    /* The database serializer and later diagnostic commands traverse the
     * reciprocal face/loop/trim/edge/vertex lists, not just their immediate
     * array indices.  Reuse the same proof-only audit required before derived
     * OpenNURBS flag refresh so a half-built candidate with a stale back
     * reference cannot be mistaken for a serializable invalid BREP. */
    std::string unsafe_topology;
    if (!brep_topology_references_are_safe(brep, &unsafe_topology))
	return false;
    for (int i = 0; i < brep->m_C2.Count(); ++i)
	if (!brep->m_C2[i]) return false;
    for (int i = 0; i < brep->m_C3.Count(); ++i)
	if (!brep->m_C3[i]) return false;
    for (int i = 0; i < brep->m_S.Count(); ++i)
	if (!brep->m_S[i]) return false;
    for (int i = 0; i < brep->m_F.Count(); ++i) {
	const ON_BrepFace &face = brep->m_F[i];
	if (face.m_si < 0 || face.m_si >= brep->m_S.Count()) return false;
	for (int j = 0; j < face.m_li.Count(); ++j)
	    if (face.m_li[j] < 0 || face.m_li[j] >= brep->m_L.Count()) return false;
    }
    for (int i = 0; i < brep->m_L.Count(); ++i) {
	const ON_BrepLoop &loop = brep->m_L[i];
	if (loop.m_fi < 0 || loop.m_fi >= brep->m_F.Count()) return false;
	for (int j = 0; j < loop.m_ti.Count(); ++j)
	    if (loop.m_ti[j] < 0 || loop.m_ti[j] >= brep->m_T.Count()) return false;
    }
    for (int i = 0; i < brep->m_T.Count(); ++i) {
	const ON_BrepTrim &trim = brep->m_T[i];
	if (trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
		trim.m_c2i < 0 || trim.m_c2i >= brep->m_C2.Count() ||
		trim.m_ei >= brep->m_E.Count())
	    return false;
    }
    for (int i = 0; i < brep->m_E.Count(); ++i) {
	const ON_BrepEdge &edge = brep->m_E[i];
	if (edge.m_c3i < 0 || edge.m_c3i >= brep->m_C3.Count() ||
		edge.m_vi[0] < 0 || edge.m_vi[0] >= brep->m_V.Count() ||
		edge.m_vi[1] < 0 || edge.m_vi[1] >= brep->m_V.Count())
	    return false;
    }
    return true;
}


/* A completed inference transaction may expose an open boundary which was
 * already present in an object asserted by STEP to be a solid.  Preserve that
 * result as invalid diagnostic geometry only when every OpenNURBS boundary
 * edge maps one-to-one to an EDGE_CURVE having exactly one authored
 * ORIENTED_EDGE use in the source graph.  This distinguishes a genuinely
 * omitted source face from a boundary introduced by an importer failure or by
 * splitting one source edge into several output edges. */
static bool
authored_open_boundary_proof(const ON_Brep *brep, STEPWrapper *wrapper,
    std::vector<int> *source_boundary_edges)
{
    if (source_boundary_edges)
	source_boundary_edges->clear();
    if (!brep || !wrapper)
	return false;

    std::map<int, size_t> source_trim_uses;
    std::vector<int> boundary_sources;
    for (int edge_index = 0; edge_index < brep->m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep->m_E[edge_index];
	if (edge.m_ti.Count() == 0)
	    return false;
	const int source_edge = edge.m_edge_user.i;
	if (source_edge > 0)
	    source_trim_uses[source_edge] +=
		static_cast<size_t>(edge.m_ti.Count());
	if (edge.m_ti.Count() != 1)
	    continue;
	if (source_edge <= 0)
	    return false;
	const ON_BrepTrim *trim = brep->Trim(edge.m_ti[0]);
	if (!trim || trim->m_type != ON_BrepTrim::boundary)
	    return false;
	boundary_sources.push_back(source_edge);
    }
    if (boundary_sources.empty())
	return false;

    std::sort(boundary_sources.begin(), boundary_sources.end());
    boundary_sources.erase(std::unique(boundary_sources.begin(),
	boundary_sources.end()), boundary_sources.end());
    for (std::vector<int>::const_iterator source = boundary_sources.begin();
	    source != boundary_sources.end(); ++source) {
	if (source_trim_uses[*source] != 1)
	    return false;
	size_t oriented_edge_references = 0;
	const std::vector<uint64_t> reverse =
	    wrapper->LazyReverseReferences(static_cast<uint64_t>(*source));
	for (std::vector<uint64_t>::const_iterator reference = reverse.begin();
		reference != reverse.end(); ++reference)
	    if (wrapper->LazyIsSchemaEntity(*reference, "ORIENTED_EDGE"))
		++oriented_edge_references;
	if (oriented_edge_references != 1)
	    return false;
    }
    if (source_boundary_edges)
	*source_boundary_edges = boundary_sources;
    return true;
}


static void
classify_invalid_brep_candidate(DetachedBrepJob &job, STEPWrapper *wrapper)
{
    if (!invalid_brep_status(job.status))
	return;
    if (job.invalidity.empty()) {
	if (job.status == BREP_INVALID_STRUCTURE)
	    job.invalidity = "opennurbs_structure";
	else
	    job.invalidity = "not_solid";
    }
    if (job.invalid_reason.empty()) {
	if (job.status == BREP_INVALID_STRUCTURE)
	    job.invalid_reason = "OpenNURBS structural validation failed";
	else
	    job.invalid_reason =
		"closed STEP BREP did not validate as a solid";
    }
    job.preserved_invalid =
	wrapper->ImportOptions().effective_invalid_brep_policy ==
	    brlcad::step::InvalidBrepPolicy::Preserve &&
	brep_candidate_storage_safe(job.brep.get());
}


static uint64_t
scaled_item_budget(uint64_t base, uint64_t numerator, uint64_t denominator = 1)
{
    if (!base || !numerator || !denominator) return 0;
    if (base > UINT64_MAX / numerator) return UINT64_MAX;
    const uint64_t product = base * numerator;
    return std::max<uint64_t>(1, product / denominator +
	(product % denominator ? 1 : 0));
}


static uint64_t
complex_item_budget(uint64_t base, size_t faces)
{
    if (!base) return 0;
    const uint64_t maximum = scaled_item_budget(base,
	kMaximumComplexExactPullbackMilliseconds,
	kMaximumExactPullbackMilliseconds);
    const uint64_t per_face = scaled_item_budget(base,
	kExactPullbackMillisecondsPerFace,
	kMaximumExactPullbackMilliseconds);
    const uint64_t face_count = static_cast<uint64_t>(faces);
    const uint64_t face_allowance = per_face &&
	face_count > (UINT64_MAX - base) / per_face ? UINT64_MAX :
	base + face_count * per_face;
    return std::min(maximum, face_allowance);
}


static size_t repair_closed_shell_face_orientations(ON_Brep *brep,
	std::string *failure = NULL);
static size_t repair_split_closed_edge_loop_orientations(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	std::string *failure = NULL);
static size_t split_reused_edges_by_shell_component(ON_Brep *brep,
	std::string *failure = NULL, int *unresolved_step_edge = NULL,
	int *unresolved_use_count = NULL);
static size_t reconcile_pre_split_step_edge_uses(ON_Brep *brep,
	std::string *failure = NULL);


static std::unique_ptr<DetachedBrepJob>
detach_brep_job_data(int entity_id, const std::string &entity_type,
    const std::string &original_name, const std::string &output_name,
    int product_id, double length_factor, double planeangle_factor,
    double solidangle_factor, double representation_tolerance,
    const fastf_t *write_matrix,
    const brlcad::step::Style *style)
{
    std::unique_ptr<DetachedBrepJob> job(new DetachedBrepJob());
    job->entity_id = entity_id;
    job->coverage_entity_id = entity_id;
    job->output_source_id = entity_id;
    job->product_id = product_id;
    job->entity_type = entity_type;
    job->original_name = original_name;
    job->output_name = output_name;
    job->length_factor = length_factor;
    job->planeangle_factor = planeangle_factor;
    job->solidangle_factor = solidangle_factor;
    job->tolerance = LocalUnits::tolerance;
    job->representation_tolerance =
	std::max(job->tolerance, representation_tolerance);
    job->faceted = entity_type == "FACETED_BREP";
    if (write_matrix) MAT_COPY(job->write_matrix, write_matrix);
    if (style) {
	job->has_style = true;
	job->style = *style;
    }
    return job;
}


static std::unique_ptr<DetachedBrepJob>
detach_brep_job(SolidModel *solid, Representation *representation,
    STEPWrapper *wrapper, const std::string &name, int product_id,
    const fastf_t *matrix_override, const brlcad::step::Style *style_override)
{
    if (!solid || !representation || !wrapper)
	return std::unique_ptr<DetachedBrepJob>(new DetachedBrepJob());

    mat_t matrix;
    if (matrix_override) MAT_COPY(matrix, matrix_override);
    else representation_matrix(representation, matrix);

    const brlcad::step::Style *style = style_override ? style_override :
	style_for_item(wrapper, solid->GetId());
    if (!style)
	style = style_for_item(wrapper, representation->GetId());
    const double representation_tolerance =
	wrapper->ImportOptions().absolute_tolerance_mm > 0.0 ?
	LocalUnits::tolerance : representation->GetLengthUncertainty();
    return detach_brep_job_data(solid->GetId(), exact_brep_solid_type(solid),
	solid->Name(), name, product_id,
	representation->GetLengthConversionFactor(),
	representation->GetPlaneAngleConversionFactor(),
	representation->GetSolidAngleConversionFactor(),
	representation_tolerance, matrix, style);
}


static void
materialize_detached_brep_job(DetachedBrepJob &job, STEPWrapper *wrapper)
{
    job.ready = false;
    job.solid = NULL;
    job.surface_model = NULL;
    job.face_set = NULL;
    job.face = NULL;
    job.entity_arena.reset();
    if (!wrapper || job.entity_id <= 0)
	return;

    const std::chrono::steady_clock::time_point started =
	std::chrono::steady_clock::now();
    job.work_budget_milliseconds =
	wrapper->ImportOptions().effective_item_budget_milliseconds;
    job.remaining_work_milliseconds = job.work_budget_milliseconds;
    std::chrono::steady_clock::time_point stage_started = started;
    const auto finish_stage = [&job, wrapper, &stage_started](
	const std::string &stage) {
	const std::chrono::steady_clock::time_point now =
	    std::chrono::steady_clock::now();
	const uint64_t elapsed_us = static_cast<uint64_t>(
	    std::chrono::duration_cast<std::chrono::microseconds>(
		now - stage_started).count());
	wrapper->RecordStageTiming(stage, job.entity_id, job.entity_type,
	    elapsed_us, static_cast<uint64_t>(job.topology_face_count));
	stage_started = now;
    };
    PullbackWorkScope item_work(wrapper, job.work_budget_milliseconds,
	wrapper->ImportOptions().effective_stall_timeout_milliseconds);

	/* Keep this phase separate from topology detachment and pullback.  Large
	 * dependency closures can otherwise make a long materialization look like
	 * an expensive curve-on-surface solve in the periodic telemetry. */
    wrapper->SetProgressDetail("materializing STEP dependency closure",
	job.entity_id, 0, 0, std::string(), job.entity_type);
    SDAI_Application_instance *source = wrapper->getEntity(job.entity_id);
    finish_stage("dependency_materialization");
    if (source) {
	wrapper->SetProgressDetail("detaching exact STEP topology",
	    job.entity_id, 0, 0, std::string(), job.entity_type);
	STEPEntity *object = Factory::CreateObject(wrapper, source);
	RepresentationItem *item = dynamic_cast<RepresentationItem *>(object);
	job.solid = exact_brep_solid(item);
	job.surface_model = dynamic_cast<ShellBasedSurfaceModel *>(item);
	job.face_set = dynamic_cast<ConnectedFaceSet *>(object);
	job.face = dynamic_cast<Face *>(object);
	if (job.solid) {
	    if (job.original_name.empty()) job.original_name = job.solid->Name();
	    if (job.entity_type.empty()) job.entity_type = exact_brep_solid_type(job.solid);
	    job.faceted = dynamic_cast<FacetedBrep *>(job.solid) != NULL;
	    ManifoldSolidBrep *manifold = dynamic_cast<ManifoldSolidBrep *>(job.solid);
	    if (manifold) {
		job.topology_face_count = manifold->FaceCount();
		job.maximum_pullback_span_estimate =
		    manifold->MaximumPullbackSpanEstimate();
		if (job.topology_face_count > kComplexExactSolidFaceThreshold) {
		    job.work_budget_milliseconds = complex_item_budget(
			wrapper->ImportOptions().effective_item_budget_milliseconds,
			job.topology_face_count);
		    wrapper->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Information,
			job.entity_id, job.entity_type, "work_budget",
			(job.work_budget_milliseconds ?
			 "extended exact-item work budget to " +
			 std::to_string(job.work_budget_milliseconds / 1000) +
			 " seconds for " :
			 "CPU-work exact-item budget disabled for ") +
			std::to_string(job.topology_face_count) +
			" topology faces");
		}
	    }
	} else if (job.surface_model) {
	    if (job.original_name.empty())
		job.original_name = job.surface_model->Name();
	    job.entity_type = "SHELL_BASED_SURFACE_MODEL";
	    job.maximum_pullback_span_estimate =
		job.surface_model->MaximumPullbackSpanEstimate();
	    job.work_budget_milliseconds = scaled_item_budget(
		wrapper->ImportOptions().effective_item_budget_milliseconds,
		kMaximumSurfaceModelPullbackMilliseconds,
		kMaximumExactPullbackMilliseconds);
	} else if (job.face_set) {
	    job.topology_face_count = job.face_set->FaceCount();
	    job.maximum_pullback_span_estimate =
		job.face_set->MaximumPullbackSpanEstimate();
	    if (job.entity_type.empty())
		job.entity_type = wrapper->LazyTypeName(job.entity_id);
	    if (job.topology_face_count > kComplexExactSolidFaceThreshold) {
		job.work_budget_milliseconds = complex_item_budget(
		    wrapper->ImportOptions().effective_item_budget_milliseconds,
		    job.topology_face_count);
	    }
	} else if (job.face) {
	    job.topology_face_count = 1;
	    FaceSurface *surface_face = dynamic_cast<FaceSurface *>(job.face);
	    if (surface_face)
		job.maximum_pullback_span_estimate =
		    surface_face->PullbackSpanEstimate();
	    if (job.entity_type.empty())
		job.entity_type = wrapper->LazyTypeName(job.entity_id);
	}
    }
    finish_stage("topology_detach");
    brlcad::PullbackWorkCancelled();
    if (item_work.DeadlineExpired() || item_work.Stalled()) {
	job.validation_message = item_work.Stalled() ?
	    "dependency materialization made no measurable progress for " +
	    std::to_string((wrapper->ImportOptions().effective_stall_timeout_milliseconds +
		999) / 1000) + " seconds" :
	    "dependency materialization exceeded the " +
	    std::to_string((job.work_budget_milliseconds + 999) / 1000) +
	    "-second per-item work budget";
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    job.entity_id, job.entity_type,
	    item_work.Stalled() ? "progress_stall" : "dependency_closure",
	    job.validation_message);
	job.solid = NULL;
	job.surface_model = NULL;
	job.face_set = NULL;
	job.face = NULL;
	wrapper->ReleaseSourceData();
	job.status = BREP_CONVERSION_FAILED;
	return;
    }
    job.entity_arena = wrapper->DetachEntityCache();
    wrapper->ReleaseSourceData();
    finish_stage("source_release");
    job.ready = (job.solid != NULL || job.surface_model != NULL ||
	job.face_set != NULL || job.face != NULL) &&
	job.entity_arena != NULL;
    const uint64_t initial_budget =
	wrapper->ImportOptions().effective_item_budget_milliseconds;
    const uint64_t initial_remaining =
	brlcad::PullbackWorkRemainingMilliseconds();
    const uint64_t consumed = !initial_budget ||
	initial_remaining == UINT64_MAX ? 0 :
	(initial_remaining < initial_budget ?
	 initial_budget - initial_remaining : 0);
    job.remaining_work_milliseconds = !job.work_budget_milliseconds ? 0 :
	(consumed < job.work_budget_milliseconds ?
	 job.work_budget_milliseconds - consumed : 1);
}


static void convert_detached_brep_job(DetachedBrepJob &job,
    STEPWrapper *wrapper);


/* Curve inference is deliberately not part of the ordinary permissive build.
 * It has a locally bounded reconstruction tier and, for asserted volumes, a
 * tighter item-relative whole-solid tier.  Both are attempted only after the
 * complete source-faithful/safe candidate has failed.  Suppress provisional
 * diagnostics, collect inference provenance across nested edge workers, and
 * publish it only when the finished BREP passes the applicable structure and
 * solidness gates. */
static bool
retry_detached_brep_with_curve_inference(DetachedBrepJob &job,
    STEPWrapper *wrapper)
{
    if (!wrapper || wrapper->CurveInferenceTrialEnabled())
	return false;
    const brlcad::step::ImportOptions &options = wrapper->ImportOptions();
    if (options.exact || options.strict ||
	    options.repair != brlcad::step::RepairMode::Safe ||
	    options.effective_invalid_brep_policy !=
		brlcad::step::InvalidBrepPolicy::Preserve)
	return false;
    /* Rebuilding a complete solid is potentially very expensive.  The
     * ordinary pass records this signal only after finding a curve choice
     * whose measured discrepancy is inside the permissive catastrophe guard.
     * Missing references, timeouts, and mismatches already beyond that guard
     * cannot benefit from inference and must not trigger a speculative second
     * construction pass. */
    const bool candidate_observed =
	wrapper->CurveInferenceCandidateObserved();
    const bool retry_blocked = wrapper->CurveInferenceRetryBlocked();
    if (!candidate_observed || retry_blocked) {
	if (wrapper->Verbose())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": skipped permissive curve-inference trial (candidate="
		<< (candidate_observed ? "yes" : "no") << ", blocked="
		<< (retry_blocked ? "yes" : "no") << ')' << std::endl;
	return false;
    }

    std::shared_ptr<STEPWrapper::CurveInferenceTransaction> transaction(
	new STEPWrapper::CurveInferenceTransaction());
    {
	STEPWrapper::RepairModeScope repair_scope(wrapper,
	    brlcad::step::RepairMode::Safe, false);
	STEPWrapper::CurveInferenceScope inference_scope(wrapper, transaction,
	    true);
	job.brep.reset();
	job.bot_vertices.clear();
	job.bot_faces.clear();
	job.status = BREP_CONVERSION_FAILED;
	job.validation_message.clear();
	convert_detached_brep_job(job, wrapper);
    }

    bool inferred = false;
    size_t inference_diagnostic_count = 0;
    size_t inference_error_count = 0;
    size_t inferred_curve_count = 0;
    {
	std::lock_guard<std::mutex> guard(transaction->mutex);
	inferred = !transaction->curves.empty();
	inference_diagnostic_count = transaction->diagnostics.size();
	inferred_curve_count = transaction->curves.size();
	for (std::vector<brlcad::step::Diagnostic>::const_iterator diagnostic =
		transaction->diagnostics.begin();
		diagnostic != transaction->diagnostics.end(); ++diagnostic)
	    if (diagnostic->severity ==
		    brlcad::step::DiagnosticSeverity::Error)
		++inference_error_count;
    }

    std::vector<int> authored_boundary_edges;
    const bool preserve_authored_open =
	job.status == BREP_NOT_SOLID && inferred &&
	inference_error_count == 0 && job.is_region &&
	brep_candidate_storage_safe(job.brep.get()) &&
	authored_open_boundary_proof(job.brep.get(), wrapper,
	    &authored_boundary_edges);

    if ((job.status != BREP_WRITE_SUCCESS && !preserve_authored_open) ||
	    !inferred) {
	/* A locally large displacement can still be well determined at the
	 * whole-part scale.  Give asserted volumes one stronger, explicitly
	 * labeled transaction: every replacement remains below the item-relative
	 * catastrophe guard and the transaction is published only when the
	 * completed object is a structurally valid, closed, oriented manifold
	 * solid with no unresolved construction diagnostics. */
	if (job.is_region && !brlcad::PullbackWorkCancelled()) {
	    std::shared_ptr<STEPWrapper::CurveInferenceTransaction>
		whole_item_transaction(
		    new STEPWrapper::CurveInferenceTransaction());
	    {
		STEPWrapper::RepairModeScope repair_scope(wrapper,
		    brlcad::step::RepairMode::Safe, false);
		STEPWrapper::CurveInferenceScope inference_scope(wrapper,
		    whole_item_transaction, true, true);
		job.brep.reset();
		job.bot_vertices.clear();
		job.bot_faces.clear();
		job.status = BREP_CONVERSION_FAILED;
		job.validation_message.clear();
		convert_detached_brep_job(job, wrapper);
	    }

	    bool whole_item_inferred = false;
	    size_t whole_item_diagnostic_count = 0;
	    size_t whole_item_error_count = 0;
	    size_t whole_item_curve_count = 0;
	    {
		std::lock_guard<std::mutex> guard(
		    whole_item_transaction->mutex);
		whole_item_inferred = !whole_item_transaction->curves.empty();
		whole_item_diagnostic_count =
		    whole_item_transaction->diagnostics.size();
		whole_item_curve_count = whole_item_transaction->curves.size();
		for (std::vector<brlcad::step::Diagnostic>::const_iterator
			diagnostic = whole_item_transaction->diagnostics.begin();
			diagnostic != whole_item_transaction->diagnostics.end();
			++diagnostic)
		    if (diagnostic->severity ==
			    brlcad::step::DiagnosticSeverity::Error)
			++whole_item_error_count;
	    }
	    if (job.status == BREP_WRITE_SUCCESS && whole_item_inferred &&
		    whole_item_error_count == 0) {
		STEPWrapper::CurveInferenceScope inference_scope(wrapper,
		    whole_item_transaction, true, true);
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, job.entity_id,
		    job.entity_type, "geometry_inference",
		    "accepted globally bounded whole-solid curve inference only "
		    "after the completed replacement geometry passed structural, "
		    "closed-manifold, solidness, and orientation validation; "
		    "affected STEP edges are recorded in inferred_curves");
		job.accepted_inference_transaction = whole_item_transaction;
		return true;
	    }
	    /* If the stronger transaction also fails, retain its candidate for
	     * normal invalid-BREP preservation and report its own provisional
	     * explanation in verbose mode. */
	    transaction = whole_item_transaction;
	    inferred = whole_item_inferred;
	    inference_diagnostic_count = whole_item_diagnostic_count;
	    inference_error_count = whole_item_error_count;
	    inferred_curve_count = whole_item_curve_count;
	}
	/* A rejected inference transaction must not contaminate the ordinary
	 * report.  In verbose mode, however, retaining no explanation makes it
	 * impossible to distinguish a construction bug from an authored
	 * contradiction that simply has no valid inferred solution.  Print a
	 * bounded diagnostic summary before discarding the transaction. */
	if (wrapper->Verbose()) {
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": rejected permissive curve-inference trial (status "
		<< static_cast<int>(job.status) << ", " << inferred_curve_count
		<< " inferred curve choices, " << inference_diagnostic_count
		<< " provisional diagnostics, " << inference_error_count
		<< " errors)" << std::endl;
	    if (!job.validation_message.empty())
		std::cerr << "  final validation: " << job.validation_message
		    << std::endl;
	    const size_t maximum_verbose_diagnostics = 64;
	    std::lock_guard<std::mutex> guard(transaction->mutex);
	    for (std::map<std::string,
		    brlcad::step::InferredCurve>::const_iterator inference =
		    transaction->curves.begin();
		    inference != transaction->curves.end(); ++inference) {
		const brlcad::step::InferredCurve &choice = inference->second;
		std::cerr << "  inferred #" << choice.edge_entity_id << ' '
		    << choice.kind << ": discrepancy=" << choice.discrepancy_mm
		    << ", safe=" << choice.safe_limit_mm
		    << ", ceiling=" << choice.inference_limit_mm << std::endl;
	    }
	    size_t printed = 0;
	    for (size_t i = 0; i < transaction->diagnostics.size() &&
		    printed < maximum_verbose_diagnostics; ++i) {
		const brlcad::step::Diagnostic &diagnostic =
		    transaction->diagnostics[i];
		if (inference_error_count && diagnostic.severity !=
			brlcad::step::DiagnosticSeverity::Error)
		    continue;
		std::cerr << "  trial #" << diagnostic.entity_id;
		if (!diagnostic.entity_type.empty())
		    std::cerr << ' ' << diagnostic.entity_type;
		if (!diagnostic.attribute.empty())
		    std::cerr << '.' << diagnostic.attribute;
		std::cerr << ": " << diagnostic.message;
		if (diagnostic.repeat_count > 1)
		    std::cerr << " (repeated " << diagnostic.repeat_count
			<< " times)";
		std::cerr << std::endl;
		++printed;
	    }
	    const size_t relevant_diagnostics = inference_error_count ?
		inference_error_count : transaction->diagnostics.size();
	    if (printed < relevant_diagnostics)
		std::cerr << "  ... "
		    << relevant_diagnostics - printed
		    << " additional provisional diagnostics omitted"
		    << std::endl;
	}
	return false;
    }

    if (preserve_authored_open) {
	std::ostringstream edge_list;
	for (size_t edge = 0; edge < authored_boundary_edges.size(); ++edge) {
	    if (edge)
		edge_list << ',';
	    edge_list << '#' << authored_boundary_edges[edge];
	}
	job.invalidity = "authored_open_topology";
	job.invalid_reason =
	    "source asserted a solid but every remaining boundary edge has "
	    "exactly one authored ORIENTED_EDGE use";
	job.validation_message = job.invalid_reason +
	    "; source boundary edges " + edge_list.str();
	STEPWrapper::CurveInferenceScope inference_scope(wrapper, transaction,
	    true);
	wrapper->RecordDiagnostic(
	    brlcad::step::DiagnosticSeverity::Warning, job.entity_id,
	    job.entity_type, "outer",
	    "permissive curve inference produced storage-safe open geometry; "
	    "preserved it as invalid because every remaining boundary edge is "
	    "independently proven to have one authored source use");
    }

    job.accepted_inference_transaction = transaction;
    return true;
}


static void
convert_detached_brep_job(DetachedBrepJob &job, STEPWrapper *wrapper)
{
	if (!job.ready || (!job.solid && !job.surface_model && !job.face_set &&
	    !job.face) ||
	!job.entity_arena || !wrapper) {
	job.status = BREP_CONVERSION_FAILED;
	return;
    }

	/* LoadONBrep includes exact curve-on-surface construction. */
    wrapper->SetProgressDetail("building exact BREP and pullbacks",
	job.entity_id, 0, 0, std::string(), job.entity_type);
    PullbackWorkScope pullback_work(wrapper, job.remaining_work_milliseconds,
	wrapper->ImportOptions().effective_stall_timeout_milliseconds);
    /* Whole-item repair mutates reciprocal OpenNURBS topology in place.  An
     * interrupted candidate is therefore not necessarily safe to serialize,
     * even when the source-derived BREP immediately before repair was.  Keep
     * a deep, storage-audited checkpoint once construction and initial
     * validation complete.  It also serves the older correctness guarantee
     * that bounded repair cannot demote an already valid exact candidate. */
    std::unique_ptr<ON_Brep> pre_repair_checkpoint;
    BrepWriteStatus pre_repair_checkpoint_status = BREP_CONVERSION_FAILED;
    bool pre_repair_checkpoint_preservable = false;
    const auto work_expired = [&job, &pullback_work, wrapper,
	&pre_repair_checkpoint, &pre_repair_checkpoint_status,
	&pre_repair_checkpoint_preservable](
	const std::string &stage) {
	if (!pullback_work.DeadlineExpired() && !pullback_work.Stalled())
	    return false;
	const std::string expiration_message = pullback_work.Stalled() ?
	    "exact conversion made no measurable progress for " +
	    std::to_string((wrapper->ImportOptions().effective_stall_timeout_milliseconds +
		999) / 1000) + " seconds during " + stage :
	    "exact pullback exceeded the " +
	    std::to_string((job.work_budget_milliseconds + 999) / 1000) +
	    "-second per-item work budget during " + stage;
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    job.entity_id, job.entity_type,
	    pullback_work.Stalled() ? "progress_stall" : "trim_pcurve",
	    expiration_message);
	if (pre_repair_checkpoint &&
		(pre_repair_checkpoint_status == BREP_WRITE_SUCCESS ||
		pre_repair_checkpoint_preservable)) {
	    job.brep = std::move(pre_repair_checkpoint);
	    job.bot_vertices.clear();
	    job.bot_faces.clear();
	    job.status = pre_repair_checkpoint_status;
	    if (job.status == BREP_WRITE_SUCCESS) {
		job.invalidity.clear();
		job.invalid_reason.clear();
		job.validation_message = expiration_message +
		    "; restored the valid pre-repair BREP checkpoint";
	    } else {
		job.invalidity = "repair_timeout";
		job.invalid_reason = expiration_message +
		    "; restored the storage-safe pre-repair BREP checkpoint";
		job.validation_message = job.invalid_reason;
	    }
	    if (job.status == BREP_WRITE_SUCCESS)
		wrapper->RecordRepair(job.entity_id, job.entity_type,
		    "bounded_repair",
		    "restored the valid pre-repair BREP checkpoint after "
		    "bounded repair was interrupted");
	    return true;
	}
	job.validation_message = expiration_message;
	job.brep.reset();
	job.status = BREP_CONVERSION_FAILED;
	return true;
    };

    LocalUnits::length = job.length_factor;
    LocalUnits::planeangle = job.planeangle_factor;
    LocalUnits::solidangle = job.solidangle_factor;
    LocalUnits::tolerance = job.tolerance;
    LocalUnits::representation_tolerance = job.representation_tolerance;

    std::chrono::steady_clock::time_point stage_started =
	std::chrono::steady_clock::now();
    const auto finish_stage = [&job, wrapper, &stage_started](
	const std::string &stage) {
	const std::chrono::steady_clock::time_point now =
	    std::chrono::steady_clock::now();
	const uint64_t elapsed_us = static_cast<uint64_t>(
	    std::chrono::duration_cast<std::chrono::microseconds>(
		now - stage_started).count());
	const uint64_t faces = job.brep ?
	    static_cast<uint64_t>(job.brep->m_F.Count()) :
	    static_cast<uint64_t>(job.topology_face_count);
	const uint64_t edges = job.brep ?
	    static_cast<uint64_t>(job.brep->m_E.Count()) : 0;
	const uint64_t trims = job.brep ?
	    static_cast<uint64_t>(job.brep->m_T.Count()) : 0;
	wrapper->RecordStageTiming(stage, job.entity_id, job.entity_type,
	    elapsed_us, faces, edges, trims);
	stage_started = now;
    };

    const auto load_brep = [&job]() {
	bool loaded = false;
	if (job.surface_model) {
	    job.brep.reset(job.surface_model->GetONBrep());
	    loaded = job.brep.get() != NULL;
	} else {
	    job.brep.reset(new ON_Brep());
	    loaded = job.face_set ? job.face_set->LoadONBrep(job.brep.get()) :
		(job.face ? job.face->LoadONBrep(job.brep.get()) :
		job.solid->LoadONBrep(job.brep.get()));
	}
	return loaded;
    };

    bool loaded = false;
    bool accepted_unrepaired = false;
    if (!job.faceted && !wrapper->CurveInferenceTrialEnabled() &&
	    !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair ==
		brlcad::step::RepairMode::Safe) {
	/* Safe repair is a fallback, not a license to alter a valid exact BREP.
	 * Construct the same candidate requested by --repair none and retain it
	 * when its complete OpenNURBS structure (and, for a region, solidness)
	 * already proves that no repair is needed.  An inference transaction is
	 * entered only after this ordinary transaction has already failed, so its
	 * reconstruction must proceed directly to the distinct inference-enabled
	 * safe candidate instead of repeating the identical raw preflight.  The
	 * override is thread-local and propagated to face helpers by
	 * ParallelForGeometry(). */
	STEPWrapper::RepairModeScope raw_scope(wrapper,
	    brlcad::step::RepairMode::None, true);
	job.entity_arena->ResetOpenNURBSState();
	loaded = load_brep();
	if (loaded) {
	    std::string raw_unsafe_topology;
	    const bool raw_topology_safe =
		brep_topology_references_are_safe(job.brep.get(),
		    &raw_unsafe_topology);
	    if (raw_topology_safe)
		finalize_brep_topology(job.brep.get(), false, wrapper,
		    job.entity_id, job.entity_type);
	    accepted_unrepaired = raw_topology_safe &&
		job.brep->IsValid() &&
		(!job.is_region || job.brep->IsSolid());
	}
	finish_stage("brep_unrepaired_preflight");
	if (work_expired("unrepaired BREP preflight"))
	    return;
	if (accepted_unrepaired) {
	    /* The source-faithful candidate has already passed complete
	     * OpenNURBS structural validation and, for a STEP region, solidness.
	     * It is the highest-priority result.  Do not run it through the common
	     * safe-repair tail: besides being semantically unnecessary, that tail
	     * repeated several full validity walks and could alter derived periodic
	     * classifications on a candidate already proven correct. */
	    job.status = BREP_WRITE_SUCCESS;
	    return;
	}
	if (!accepted_unrepaired)
	    job.brep.reset();
    }
    if (!accepted_unrepaired) {
	job.entity_arena->ResetOpenNURBSState();
	loaded = load_brep();
	finish_stage("brep_construction_pullback");
    }
    if (work_expired("BREP construction"))
	return;
    if (!loaded) {
	/* A constructor may reject one trim after having produced a partial BREP
	 * whose owned geometry and array references are nevertheless safe to
	 * serialize.  That candidate is not valid geometry and must never outrank
	 * a successful inference retry, but permissive import prefers an explicitly
	 * tagged diagnostic object to complete omission.  A cancelled/expired build
	 * was cleared by work_expired() above and cannot enter this fallback. */
	std::unique_ptr<ON_Brep> partial_brep;
	if (brep_candidate_storage_safe(job.brep.get()))
	    partial_brep = std::move(job.brep);
	else
	    job.brep.reset();
	job.status = BREP_CONVERSION_FAILED;
	if (retry_detached_brep_with_curve_inference(job, wrapper))
	    return;
	/* A failed inferred reconstruction is never itself the fallback.  Restore
	 * only the ordinary, non-inference partial candidate retained above. */
	job.brep.reset();
	job.bot_vertices.clear();
	job.bot_faces.clear();
	if (partial_brep) {
	    job.brep = std::move(partial_brep);
	    job.status = BREP_INVALID_STRUCTURE;
	    job.invalidity = "partial_construction";
	    job.invalid_reason =
		"exact STEP BREP construction failed after producing a storage-safe partial BREP";
	    job.validation_message = job.invalid_reason;
	    return;
	}
	job.status = BREP_CONVERSION_FAILED;
	return;
    }
    /* Capture the exporter-supplied FACE_OUTER_BOUND choices before topology
     * finalization computes replacement loop types from pcurves.  A later
     * bounded whole-solid trial must test the actual malformed STEP inputs,
     * not derived flags left by an earlier repair pass. */
    const std::vector<SuppliedFaceOuterBounds> supplied_outer_ambiguities =
	supplied_face_outer_bound_ambiguities(job.brep.get());
    if (wrapper->Verbose()) {
	for (std::vector<SuppliedFaceOuterBounds>::const_iterator ambiguity =
		supplied_outer_ambiguities.begin();
		ambiguity != supplied_outer_ambiguities.end(); ++ambiguity) {
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": captured supplied FACE.wr2 alternatives on F"
		<< ambiguity->face_index << " STEP loops";
	    for (std::vector<SuppliedOuterBoundCandidate>::const_iterator loop =
		    ambiguity->candidates.begin();
		    loop != ambiguity->candidates.end(); ++loop)
		std::cerr << " #" << loop->loop_step_id;
	    std::cerr << std::endl;
	}
    }
    std::string unsafe_topology;
    if (!brep_topology_references_are_safe(job.brep.get(), &unsafe_topology)) {
	job.validation_message = "unsafe OpenNURBS topology before derived-flag "
	    "refresh: " + unsafe_topology;
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
	    job.entity_id, job.entity_type, "topology", job.validation_message);
	job.brep.reset();
	job.status = BREP_CONVERSION_FAILED;
	return;
    }
    finish_stage("topology_reference_audit");
    /* A valid periodic band can initially place both uses of its seam edge on
     * one parameter branch.  At that point it resembles a zero-area keyhole,
     * but the exact seam-pair repair below will move the uses to opposite
     * surface boundaries.  Refresh and classify first; normalize keyholes only
     * if the completed pcurve repair still leaves invalid topology. */
    size_t keyhole_splits = finalize_brep_topology(job.brep.get(), false,
	wrapper, job.entity_id, job.entity_type);
    finish_stage("topology_initial_finalize");
    if (work_expired("topology finalization"))
	return;
    /* Keep an explicit pre-repair structural measurement.  A raw candidate
     * accepted above needs no repair at all.  Once raw validation fails,
     * however, successful safe-mode construction is not enough to skip the
     * bounded whole-item pass: NIST6, for example, is structurally valid at
     * this point but still needs exact singular-trim and periodic-branch
     * reconciliation for correct shaded geometry. */
    wrapper->SetProgressDetail("prevalidating exact OpenNURBS structure",
	job.entity_id, 0, 0, std::string(), job.entity_type);
    ON_wString initial_validation_messages;
    ON_TextLog initial_validation_log(initial_validation_messages);
    const bool initial_structurally_valid =
	job.brep->IsValid(&initial_validation_log);
    if (!initial_structurally_valid && wrapper->Verbose()) {
	ON_String initial_text(initial_validation_messages);
	std::cerr << job.entity_type << " #" << job.entity_id
	    << ": exact BREP before whole-item repair is invalid:\n"
	    << initial_text.Array() << std::endl;
    }
    /* Bounded whole-item repair is allowed to improve an already valid BREP,
     * but it must never make that source-faithful candidate unavailable.  A
     * second construction is not an equivalent rollback: independent face
     * scheduling can select a different numerically valid pullback branch.
     * In permissive mode retain a storage-safe invalid candidate as well, so
     * a timeout cannot turn usable diagnostic geometry into an omission.
     * Inference trials are deliberately excluded: invented geometry which
     * did not pass the complete validity gates must never become fallback. */
    const bool checkpoint_storage_safe =
	brep_candidate_storage_safe(job.brep.get());
    const bool checkpoint_valid_solid = initial_structurally_valid &&
	(!job.is_region || job.brep->IsSolid());
    pre_repair_checkpoint_preservable =
	!wrapper->CurveInferenceTrialEnabled() && checkpoint_storage_safe &&
	wrapper->ImportOptions().effective_invalid_brep_policy ==
	    brlcad::step::InvalidBrepPolicy::Preserve;
    if (!wrapper->CurveInferenceTrialEnabled() && checkpoint_storage_safe &&
	    (initial_structurally_valid || pre_repair_checkpoint_preservable)) {
	pre_repair_checkpoint.reset(new ON_Brep(*job.brep));
	pre_repair_checkpoint_status = checkpoint_valid_solid ?
	    BREP_WRITE_SUCCESS : initial_structurally_valid ?
	    BREP_NOT_SOLID : BREP_INVALID_STRUCTURE;
    }
    const bool needs_initial_trim_repair = !accepted_unrepaired;
    finish_stage("opennurbs_initial_validation");
    if (work_expired("initial OpenNURBS validation"))
	return;
    if (!job.faceted && needs_initial_trim_repair) {
	wrapper->SetProgressDetail("repairing exact BREP trim orientations",
	    job.entity_id, 0, 0, std::string(), job.entity_type);
	repair_closed_trim_orientations(job.brep.get(), wrapper, job.entity_id,
	    job.entity_type);
	finish_stage("trim_orientation_repair");
	if (work_expired("trim orientation repair"))
	    return;
    }
    /* Exact pcurve repair may unwrap a periodic join or prove a duplicate
     * seam bridge that was not visible in the initial topology.  Retry safe
     * topology proofs for a remaining structural failure or for a requested
     * solid which is only a valid open shell, then repair any joins exposed
     * by that normalization. */
    wrapper->SetProgressDetail("validating bounded OpenNURBS repairs",
	job.entity_id, 0, 0, std::string(), job.entity_type);
    const bool preliminary_valid = job.brep->IsValid();
    /* A structurally valid BREP may still fail to represent the requested
     * MANIFOLD_SOLID_BREP because an exact zero-area source face leaves
     * boundary edges.  Safe topology normalization contains its own geometric
     * proofs and transactional validity checks, so give it first refusal before
     * accepting a merely valid open shell or attempting curve inference. */
    const bool needs_post_repair_topology = !preliminary_valid ||
	(job.is_region && !job.brep->IsSolid());
    finish_stage("opennurbs_prevalidation");
    if (work_expired("preliminary OpenNURBS validation"))
	return;
    if (needs_post_repair_topology &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	bool topology_changed = false;
	const size_t post_repair_splits = finalize_brep_topology(job.brep.get(), true,
	    wrapper, job.entity_id, job.entity_type, &topology_changed);
	finish_stage("topology_post_repair_finalize");
	keyhole_splits += post_repair_splits;
	if (work_expired("post-repair topology finalization"))
	    return;
	/* Re-run geometric trim repair only if keyhole/slit normalization actually
	 * changed topology.  A flags-only refresh leaves every repaired pcurve and
	 * edge use intact; repeating the complete dense regeneration pass is both
	 * redundant and capable of consuming the entire per-item budget. */
	if (!job.faceted && topology_changed)
	    repair_closed_trim_orientations(job.brep.get(), wrapper, job.entity_id,
		job.entity_type);
	else if (!job.faceted)
	    repair_face_bound_classification(job.brep.get(), wrapper, job.entity_id,
		job.entity_type);
	finish_stage("post_repair_trim_orientation");
	if (work_expired("post-repair trim orientation repair"))
	    return;
    }
    /* Some closed-surface faces become structurally valid only after all
     * independently authored copies of their periodic seam edge are merged.
     * This must precede the final structural gate: the later solidness repair
     * is unreachable while OpenNURBS still sees boundary trims on a closed
     * surface.  The helper is whole-BREP transactional and commits only when
     * the complete exact merge set validates. */
    if (!job.faceted &&
	    !preliminary_valid &&
	    !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	/* A structurally valid preliminary candidate cannot benefit from an edge
	 * merge whose sole purpose is structural recovery.  Avoid immediately
	 * repeating the same complete validity walk; the authoritative logged
	 * validation below still certifies the final candidate. */
	if (!job.brep->IsValid()) {
	    wrapper->SetProgressDetail(
		"reconciling structurally required periodic seam edges",
		job.entity_id, 0, 0, std::string(), job.entity_type);
	    std::string structural_merge_details;
	    const size_t structural_merges =
		merge_exact_periodic_boundary_edges_for_structural_validation(
		    job.brep.get(), &structural_merge_details);
	    for (size_t repair = 0; repair < structural_merges; ++repair)
		wrapper->RecordRepair(job.entity_id, job.entity_type,
		    "edge_topology",
		    "merged two coincident periodic STEP boundary edges into "
		    "one exact seam edge");
	    if (wrapper->Verbose() && structural_merges)
		std::cerr << job.entity_type << " #" << job.entity_id
		    << ": merged structurally required periodic boundary edge "
		    << structural_merge_details << std::endl;
	    else if (wrapper->Verbose() &&
		    !structural_merge_details.empty())
		std::cerr << job.entity_type << " #" << job.entity_id
		    << ": structural periodic boundary edge reconciliation "
		    << "rejected: " << structural_merge_details << std::endl;
	}
	finish_stage("topology_structural_periodic_edge_merge");
	if (work_expired("structurally required periodic edge reconciliation"))
	    return;
    }
    for (size_t split = 0; split < keyhole_splits; ++split)
	wrapper->RecordRepair(job.entity_id, job.entity_type, "edge_loop",
	    "split a zero-area keyhole bridge into closed trim loops");

    /* ON_Brep::IsValid() refreshes loop-derived state while diagnosing a
     * candidate.  Restore any unambiguous STEP bound classification after
     * the preliminary check and immediately before authoritative validation. */
    if (!job.faceted)
	interpret_closed_periodic_face_bounds(job.brep.get(), wrapper,
	    job.entity_id, job.entity_type);
    if (!job.faceted &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe)
	repair_face_bound_classification(job.brep.get(), wrapper, job.entity_id,
	    job.entity_type);

    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    wrapper->SetProgressDetail("validating exact OpenNURBS structure",
	job.entity_id, 0, 0, std::string(), job.entity_type);
    bool structurally_valid = job.brep->IsValid(&validation_log);

    const uint64_t structural_recovery_budget = scaled_item_budget(
	wrapper->ImportOptions().effective_item_budget_milliseconds,
	kTopologicalKeyholeRetryMilliseconds,
	kMaximumExactPullbackMilliseconds);
    const uint64_t structural_recovery_remaining =
	brlcad::PullbackWorkRemainingMilliseconds();
    if (!structurally_valid && !job.faceted &&
	    !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
	    structural_recovery_budget &&
	    structural_recovery_remaining != UINT64_MAX) {
	const uint64_t extended_remaining =
	    structural_recovery_remaining >
		UINT64_MAX - structural_recovery_budget ? UINT64_MAX :
	    structural_recovery_remaining + structural_recovery_budget;
	pullback_work.Restart(extended_remaining,
	    wrapper->ImportOptions().effective_stall_timeout_milliseconds);
	job.work_budget_milliseconds =
	    job.work_budget_milliseconds > UINT64_MAX - structural_recovery_budget ?
	    UINT64_MAX : job.work_budget_milliseconds + structural_recovery_budget;
	wrapper->RecordDiagnostic(
		brlcad::step::DiagnosticSeverity::Information, job.entity_id,
		job.entity_type, "work_budget",
		"added " + std::to_string(
		    (structural_recovery_budget + 999) / 1000) +
		" seconds of bounded structural recovery after the exact "
		"candidate failed validation");
    }
    if (work_expired("OpenNURBS structural validation"))
	return;
    if (!structurally_valid && !job.faceted &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	/* Some openNURBS validity checks normalize derived loop flags before
	 * reporting a later face.  A single bounded retry restores only the
	 * classification proven above; it does not alter geometry or topology. */
	repair_face_bound_classification(job.brep.get(), wrapper, job.entity_id,
	    job.entity_type);
	validation_messages = ON_wString();
	ON_TextLog repaired_validation_log(validation_messages);
	structurally_valid = job.brep->IsValid(&repaired_validation_log);
	if (work_expired("repaired OpenNURBS structural validation"))
	    return;
    }
    if (!job.faceted &&
	    (!structurally_valid || (job.is_region && !job.brep->IsSolid())) &&
	    retry_unique_face_bound_classification(job.brep.get(), wrapper,
		job.entity_id, job.entity_type, job.is_region,
		supplied_outer_ambiguities)) {
	validation_messages = ON_wString();
	ON_TextLog face_bound_validation_log(validation_messages);
	structurally_valid = job.brep->IsValid(&face_bound_validation_log);
	if (work_expired("ambiguous FACE_BOUND interpretation retry"))
	    return;
    }
    if (!structurally_valid && !job.faceted &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
	    retry_topological_keyhole_normalization(job.brep.get(), wrapper,
		job.entity_id, job.entity_type)) {
	    validation_messages = ON_wString();
	    ON_TextLog keyhole_validation_log(validation_messages);
	    structurally_valid = job.brep->IsValid(&keyhole_validation_log);
	    if (work_expired("topological keyhole normalization retry"))
		return;
    }
    finish_stage("opennurbs_structural_validation");
    if (!structurally_valid && pre_repair_checkpoint &&
	    initial_structurally_valid &&
	    needs_initial_trim_repair && !wrapper->CurveInferenceTrialEnabled()) {
	/* A whole-BREP semantic repair must never demote an already valid exact
	 * candidate to invalid fallback status.  A valid initial candidate cannot
	 * normally skip the bounded repair pass, while a focused periodic fixture
	 * demonstrates the converse: a repair which helps one construction can
	 * regress another.  Restore the exact candidate whose validity was already
	 * proved rather than reconstructing a potentially
	 * different set of parallel pullback branches. */
	job.brep.reset(new ON_Brep(*pre_repair_checkpoint));
	structurally_valid = true;
	validation_messages = ON_wString();
	job.validation_message.clear();
	wrapper->RecordRepair(job.entity_id, job.entity_type,
	    "bounded_repair",
	    "rolled back bounded trim repairs because they regressed an "
	    "already valid exact OpenNURBS candidate");
	finish_stage("brep_pre_repair_rollback_restore");
    }
    if (!structurally_valid) {
	/* Preserve the ordinary bounded candidate as the diagnostic fallback.
	 * The inference retry reconstructs from detached STEP data so it has the
	 * authored curve/surface associations, and replaces this candidate only
	 * when the entire inferred result validates. */
	std::unique_ptr<ON_Brep> ordinary_brep = std::move(job.brep);
	const std::string ordinary_validation_message = job.validation_message;
	if (retry_detached_brep_with_curve_inference(job, wrapper))
	    return;
	job.brep = std::move(ordinary_brep);
	job.validation_message = ordinary_validation_message;
	ON_String text(validation_messages);
	job.validation_message = text.Array();
	std::ostringstream joins;
	int reported_edge_closure_mismatches = 0;
	for (int ei = 0; ei < job.brep->m_E.Count() &&
		reported_edge_closure_mismatches < 12; ++ei) {
	    const ON_BrepEdge &edge = job.brep->m_E[ei];
	    const bool topology_closed = edge.m_vi[0] >= 0 &&
		edge.m_vi[0] == edge.m_vi[1];
	    const bool geometry_closed = edge.IsClosed();
	    if (topology_closed == geometry_closed)
		continue;
	    const ON_3dPoint start = edge.PointAtStart();
	    const ON_3dPoint end = edge.PointAtEnd();
	    double start_vertex_distance = DBL_MAX;
	    double end_vertex_distance = DBL_MAX;
	    if (edge.m_vi[0] >= 0 && edge.m_vi[0] < job.brep->m_V.Count())
		start_vertex_distance = start.DistanceTo(
		    job.brep->m_V[edge.m_vi[0]].point);
	    if (edge.m_vi[1] >= 0 && edge.m_vi[1] < job.brep->m_V.Count())
		end_vertex_distance = end.DistanceTo(
		    job.brep->m_V[edge.m_vi[1]].point);
	    joins << "\nedge closure E" << ei << "/STEP" << edge.m_edge_user.i
		<< " vertices=" << edge.m_vi[0] << '/' << edge.m_vi[1]
		<< " topology=" << (topology_closed ? "closed" : "open")
		<< " geometry=" << (geometry_closed ? "closed" : "open")
		<< " endpoint-gap=" << start.DistanceTo(end)
		<< " vertex-distances=" << start_vertex_distance << '/'
		<< end_vertex_distance << " domain=" << edge.Domain().Min() << ':'
		<< edge.Domain().Max() << " uses=";
	    for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
		const ON_BrepTrim *use = job.brep->Trim(edge.m_ti[eti]);
		joins << (eti ? "," : "") << 'T' << edge.m_ti[eti] << "/L"
		    << (use ? use->m_li : -1) << "/STEP"
		    << (use && use->Loop() ? use->Loop()->m_loop_user.i : 0);
	    }
	    ++reported_edge_closure_mismatches;
	}
	std::set<int> detailed_loops;
	/* Always prioritize the loop indices named by OpenNURBS itself.  Earlier
	 * diagnostics listed the first twelve closure mismatches in BREP order,
	 * which could omit the later loop that actually caused validation to stop
	 * on a large solid.  Preserve the bounded summaries below, but include the
	 * rejected loop's STEP and trim identities regardless of its array index. */
	for (size_t search = 0;
		(search = job.validation_message.find("brep.m_L[", search)) !=
		    std::string::npos;) {
	    search += 9;
	    int loop_index = 0;
	    bool have_digit = false;
	    while (search < job.validation_message.size() &&
		    std::isdigit(static_cast<unsigned char>(
			job.validation_message[search]))) {
		have_digit = true;
		loop_index = 10 * loop_index +
		    (job.validation_message[search] - '0');
		++search;
	    }
	    if (have_digit && loop_index >= 0 &&
		    loop_index < job.brep->m_L.Count())
		detailed_loops.insert(loop_index);
	}
	for (size_t search = 0;
		(search = job.validation_message.find("m_T[", search)) !=
		    std::string::npos;) {
	    search += 4;
	    int trim_index = 0;
	    bool have_digit = false;
	    while (search < job.validation_message.size() &&
		    std::isdigit(static_cast<unsigned char>(
			job.validation_message[search]))) {
		have_digit = true;
		trim_index = 10 * trim_index +
		    (job.validation_message[search] - '0');
		++search;
	    }
	    if (have_digit && trim_index >= 0 &&
		    trim_index < job.brep->m_T.Count() &&
		job.brep->m_T[trim_index].m_li >= 0)
		detailed_loops.insert(job.brep->m_T[trim_index].m_li);
	}
	/* A periodic loop is often only meaningful together with the other bound
	 * on its face.  Include those siblings before emitting the prioritized
	 * detail so a failed annular-band pairing can be diagnosed in one run. */
	const std::vector<int> rejected_loops(detailed_loops.begin(),
	    detailed_loops.end());
	for (std::vector<int>::const_iterator rejected = rejected_loops.begin();
		rejected != rejected_loops.end(); ++rejected) {
	    if (*rejected < 0 || *rejected >= job.brep->m_L.Count())
		continue;
	    const ON_BrepFace *face = job.brep->m_L[*rejected].Face();
	    if (!face)
		continue;
	    for (int fli = 0; fli < face->m_li.Count(); ++fli)
		if (face->m_li[fli] >= 0 &&
			face->m_li[fli] < job.brep->m_L.Count())
		    detailed_loops.insert(face->m_li[fli]);
	}
	/* Put the exact loop named by OpenNURBS first.  Structured diagnostics are
	 * deliberately bounded, and a large BREP can otherwise fill that bound
	 * with unrelated earlier closure warnings before reaching the actionable
	 * loop. */
	std::vector<int> ordered_detailed_loops = rejected_loops;
	for (std::set<int>::const_iterator sibling = detailed_loops.begin();
		sibling != detailed_loops.end(); ++sibling)
	    if (std::find(rejected_loops.begin(), rejected_loops.end(),
		    *sibling) == rejected_loops.end())
		ordered_detailed_loops.push_back(*sibling);
	for (std::vector<int>::const_iterator li = ordered_detailed_loops.begin();
		li != ordered_detailed_loops.end(); ++li) {
	    if (*li < 0 || *li >= job.brep->m_L.Count())
		continue;
	    const ON_BrepLoop &loop = job.brep->m_L[*li];
	    const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	    joins << "\nrejected loop L" << *li << "/STEP"
		<< loop.m_loop_user.i;
	    if (surface)
		joins << " domains=" << surface->Domain(0).Min() << ':'
		    << surface->Domain(0).Max() << ','
		    << surface->Domain(1).Min() << ':'
		    << surface->Domain(1).Max() << " closed="
		    << (surface->IsClosed(0) ? 1 : 0)
		    << (surface->IsClosed(1) ? 1 : 0);
	    joins << " trims=";
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!trim)
		    continue;
		const ON_3dPoint start = trim->PointAtStart();
		const ON_3dPoint end = trim->PointAtEnd();
		const ON_BrepEdge *edge = trim->Edge();
		joins << (lti ? ";" : "") << 'T' << trim->m_trim_index
		    << "(type=" << static_cast<int>(trim->m_type)
		    << ",iso=" << static_cast<int>(trim->m_iso)
		    << ",edge=" << trim->m_ei << "/STEP"
		    << (edge ? edge->m_edge_user.i : 0) << ",rev="
		    << (trim->m_bRev3d ? 1 : 0) << ",v=" << trim->m_vi[0]
		    << ':' << trim->m_vi[1];
		if (edge)
		    joins << ",edge-v=" << edge->m_vi[0] << ':' << edge->m_vi[1]
			<< ",uses=" << edge->m_ti.Count();
		joins << ',' << start.x << ':' << start.y
		    << "->" << end.x << ':' << end.y;
		if (surface && next) {
		    const ON_3dPoint next_start = next->PointAtStart();
		    const ON_3dPoint end_lift = surface->PointAt(end.x, end.y);
		    const ON_3dPoint next_lift = surface->PointAt(next_start.x,
			next_start.y);
		    joins << ",join-uv=" << end.DistanceTo(next_start)
			<< ",join-lift=" << end_lift.DistanceTo(next_lift);
		}
		joins << ')';
	    }
	}
	int reported_trim_closure_mismatches = 0;
	for (int ti = 0; ti < job.brep->m_T.Count() &&
		reported_trim_closure_mismatches < 12; ++ti) {
	    const ON_BrepTrim &trim = job.brep->m_T[ti];
	    const ON_Curve *curve = trim.TrimCurveOf();
	    if (!curve)
		continue;
	    const bool topology_closed = trim.m_vi[0] >= 0 &&
		trim.m_vi[0] == trim.m_vi[1];
	    const bool geometry_closed = curve->IsClosed();
	    if (topology_closed == geometry_closed)
		continue;
	    const ON_BrepEdge *edge = trim.Edge();
	    joins << "\ntrim closure T" << ti << "/L" << trim.m_li
		<< "/STEP" << (trim.Loop() ? trim.Loop()->m_loop_user.i : 0)
		<< "/E" << trim.m_ei << "/STEP"
		<< (edge ? edge->m_edge_user.i : 0)
		<< " vertices=" << trim.m_vi[0] << '/' << trim.m_vi[1]
		<< " topology=" << (topology_closed ? "closed" : "open")
		<< " pcurve=" << (geometry_closed ? "closed" : "open")
		<< " endpoints=" << trim.PointAtStart().x << ':'
		<< trim.PointAtStart().y << "->" << trim.PointAtEnd().x << ':'
		<< trim.PointAtEnd().y << " type=" << static_cast<int>(trim.m_type)
		<< " iso=" << static_cast<int>(trim.m_iso)
		<< " rev=" << (trim.m_bRev3d ? 1 : 0);
	    if (trim.m_li >= 0)
		detailed_loops.insert(trim.m_li);
	    ++reported_trim_closure_mismatches;
	}
	int reported_joins = 0;
	for (int li = 0; li < job.brep->m_L.Count() && reported_joins < 12; ++li) {
	    const ON_BrepLoop &loop = job.brep->m_L[li];
	    const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	    if (!surface)
		continue;
	    for (int lti = 0; lti < loop.TrimCount() && reported_joins < 12; ++lti) {
		const ON_BrepTrim *previous = loop.Trim(lti);
		const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!previous || !next || previous->PointAtEnd().DistanceTo(
			next->PointAtStart()) <= ON_ZERO_TOLERANCE)
		    continue;
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		double previous_vertex_distance = DBL_MAX;
		double next_vertex_distance = DBL_MAX;
		if (previous->m_vi[1] >= 0 && previous->m_vi[1] < job.brep->m_V.Count())
		    previous_vertex_distance = previous_lift.DistanceTo(
			job.brep->m_V[previous->m_vi[1]].point);
		if (next->m_vi[0] >= 0 && next->m_vi[0] < job.brep->m_V.Count())
		    next_vertex_distance = next_lift.DistanceTo(
			job.brep->m_V[next->m_vi[0]].point);
		const ON_BrepEdge *previous_edge = previous->Edge();
		const ON_BrepEdge *next_edge = next->Edge();
		joins << "\njoin L" << li << "/STEP" << loop.m_loop_user.i
		    << "/T" << previous->m_trim_index
		    << "(type=" << static_cast<int>(previous->m_type)
		    << ",iso=" << static_cast<int>(previous->m_iso)
		    << ",edge=" << previous->m_ei << "/STEP"
		    << (previous_edge ? previous_edge->m_edge_user.i : 0) << ")->T"
		    << next->m_trim_index << "(type="
		    << static_cast<int>(next->m_type) << ",iso="
		    << static_cast<int>(next->m_iso) << ",edge=" << next->m_ei
		    << "/STEP" << (next_edge ? next_edge->m_edge_user.i : 0)
		    << ") pspace=" << previous_uv.DistanceTo(next_uv)
		    << " lift=" << previous_lift.DistanceTo(next_lift)
		    << " vertex=" << previous_vertex_distance << "/"
		    << next_vertex_distance << " closed="
		    << (surface->IsClosed(0) ? 1 : 0)
		    << (surface->IsClosed(1) ? 1 : 0) << " domains="
		    << surface->Domain(0).Min() << ':' << surface->Domain(0).Max()
		    << ',' << surface->Domain(1).Min() << ':'
		    << surface->Domain(1).Max();
		detailed_loops.insert(li);
		++reported_joins;
	    }
	}
	/* Parameter-space closure is not the only useful topology detail when
	 * OpenNURBS rejects a loop.  In particular, a seam use in a non-outer
	 * loop means both references to one edge landed in that loop.  Include
	 * those loops and their reciprocal edge uses in the structured diagnostic
	 * so the originating STEP topology can be identified without a debugger. */
	for (int ti = 0; ti < job.brep->m_T.Count(); ++ti) {
	    const ON_BrepTrim &trim = job.brep->m_T[ti];
	    if (trim.m_type != ON_BrepTrim::seam || trim.m_li < 0 ||
		    trim.m_li >= job.brep->m_L.Count() ||
		    job.brep->m_L[trim.m_li].m_type == ON_BrepLoop::outer)
		continue;
	    detailed_loops.insert(trim.m_li);
	    const ON_BrepEdge *edge = trim.Edge();
	    joins << "\nnon-outer seam T" << ti << "/L" << trim.m_li
		<< "/STEP" << job.brep->m_L[trim.m_li].m_loop_user.i
		<< "/E" << trim.m_ei << "/STEP"
		<< (edge ? edge->m_edge_user.i : 0) << " uses=";
	    if (edge) {
		for (int eti = 0; eti < edge->m_ti.Count(); ++eti) {
		    const ON_BrepTrim *use = job.brep->Trim(edge->m_ti[eti]);
		    joins << (eti ? "," : "") << 'T' << edge->m_ti[eti]
			<< "/L" << (use ? use->m_li : -1) << "/F"
			<< (use && use->Face() ? use->Face()->m_face_index : -1);
		}
	    }
	}
	int singular_diagnostics = 0;
	for (int ti = 0; ti < job.brep->m_T.Count() && singular_diagnostics < 8;
		++ti) {
	    const ON_BrepTrim &trim = job.brep->m_T[ti];
	    if (trim.m_type != ON_BrepTrim::singular)
		continue;
	    const ON_BrepLoop *loop = trim.Loop();
	    const ON_Surface *surface = loop && loop->Face() ?
		loop->Face()->SurfaceOf() : NULL;
	    if (!surface)
		continue;
	    const ON_Interval domain = trim.Domain();
	    const ON_Surface::ISO derived = surface->IsIsoparametric(trim, &domain);
	    if (derived == trim.m_iso)
		continue;
	    const ON_BoundingBox box = trim.BoundingBox();
	    joins << "\nsingular T" << ti << "/L" << trim.m_li << "/STEP"
		<< (loop ? loop->m_loop_user.i : 0) << " iso="
		<< static_cast<int>(trim.m_iso) << " derived="
		<< static_cast<int>(derived) << " uvbox=" << box.m_min.x << ':'
		<< box.m_max.x << ',' << box.m_min.y << ':' << box.m_max.y
		<< " domains=" << surface->Domain(0).Min() << ':'
		<< surface->Domain(0).Max() << ',' << surface->Domain(1).Min()
		<< ':' << surface->Domain(1).Max();
	    ++singular_diagnostics;
	}
	job.validation_message += joins.str();
	job.status = BREP_INVALID_STRUCTURE;
	return;
    }
    if (!wrapper->CurveInferenceTrialEnabled() &&
	    wrapper->CurveInferenceRequired()) {
	/* A topology proof can establish a source contradiction which
	 * OpenNURBS structural validation does not express.  In particular, a
	 * repeated-vertex loop may concatenate a contractible boundary with a
	 * detached noncontractible winding and still become structurally valid
	 * after an ordinary periodic seam relocation.  Do not publish that
	 * semantically invalid result as exact geometry.  Rebuild in the isolated
	 * inference transaction and accept it only through the same complete
	 * structure/solidness gates; if it fails, retain the ordinary candidate as
	 * explicitly invalid diagnostic geometry. */
	std::unique_ptr<ON_Brep> ordinary_brep = std::move(job.brep);
	const std::string ordinary_validation_message = job.validation_message;
	if (retry_detached_brep_with_curve_inference(job, wrapper))
	    return;
	job.brep = std::move(ordinary_brep);
	job.validation_message = ordinary_validation_message;
	job.status = BREP_INVALID_STRUCTURE;
	job.invalidity = "proven_semantic_topology_contradiction";
	job.invalid_reason =
	    "source loop topology was proven semantically contradictory and "
	    "the permissive inference transaction did not produce a valid result";
	job.validation_message = job.invalid_reason;
	return;
    }
    if (!job.is_region) {
	job.status = BREP_WRITE_SUCCESS;
	return;
    }
    wrapper->SetProgressDetail("validating exact BREP solidness",
	job.entity_id, 0, 0, std::string(), job.entity_type);
    if (wrapper->Verbose())
	std::cerr << job.entity_type << " #" << job.entity_id
	    << ": solidness audit topology faces=" << job.brep->m_F.Count()
	    << " loops=" << job.brep->m_L.Count()
	    << " trims=" << job.brep->m_T.Count()
	    << " edges=" << job.brep->m_E.Count() << std::endl;
    const auto one_use_edge_count = [](const ON_Brep *brep) {
	size_t count = 0;
	if (!brep)
	    return count;
	for (int ei = 0; ei < brep->m_E.Count(); ++ei)
	    if (brep->m_E[ei].m_ti.Count() == 1)
		++count;
	return count;
    };
    bool solid = job.brep->IsSolid();
    if (work_expired("BREP solidness validation"))
	return;
    if (!solid && !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	std::string duplicate_edge_details;
	std::string accepted_duplicate_edge_details;
	size_t duplicate_edge_repairs = 0;
	size_t remaining_one_use_edges = one_use_edge_count(job.brep.get());
	const size_t maximum_attempts = remaining_one_use_edges;
	for (size_t attempt = 0;
		attempt < maximum_attempts && remaining_one_use_edges > 0;
		++attempt) {
	    std::ostringstream progress_detail;
	    progress_detail << "remaining one-use edges="
		<< remaining_one_use_edges;
	    wrapper->SetProgressDetail(
		"rejoining exact duplicate STEP edge fragments",
		job.entity_id, static_cast<uint64_t>(attempt),
		static_cast<uint64_t>(maximum_attempts), "attempts",
		progress_detail.str());
	    const size_t repaired =
		merge_one_exact_duplicate_step_boundary_edge_pair(
		    job.brep.get(), &duplicate_edge_details);
	    if (!repaired)
		break;
	    accepted_duplicate_edge_details = duplicate_edge_details;
	    duplicate_edge_repairs += repaired;
	    const size_t next_one_use_edges =
		one_use_edge_count(job.brep.get());
	    if (next_one_use_edges >= remaining_one_use_edges) {
		duplicate_edge_details =
		    "an accepted duplicate edge merge did not reduce the one-use edge count";
		break;
	    }
	    remaining_one_use_edges = next_one_use_edges;
	}
	if (duplicate_edge_repairs)
	    solid = job.brep->IsSolid();
	for (size_t repair = 0; repair < duplicate_edge_repairs; ++repair)
	    wrapper->RecordRepair(job.entity_id, job.entity_type, "edge_topology",
		"rejoined two independently split uses of one exact STEP edge");
	if (wrapper->Verbose() && duplicate_edge_repairs)
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": rejoined exact duplicate STEP edge pair "
		<< accepted_duplicate_edge_details << std::endl;
	else if (wrapper->Verbose() && !duplicate_edge_details.empty())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": duplicate STEP edge-pair reconciliation rejected: "
		<< duplicate_edge_details << std::endl;
	if (work_expired("duplicate STEP edge-fragment reconciliation"))
	    return;
    }
    if (!solid && !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	std::string keyhole_edge_details;
	std::string accepted_keyhole_edge_details;
	size_t keyhole_edge_repairs = 0;
	size_t remaining_one_use_edges = one_use_edge_count(job.brep.get());
	/* A face-local zero-area keyhole can contain two distinct but exact
	 * opposite STEP edges.  Removing the cancelling face uses leaves their
	 * reciprocal uses as two coincident boundary edges.  Reconcile only the
	 * unique authoritative endpoint pair proved by the whole shell; each
	 * accepted transaction consumes two one-use edges. */
	const size_t maximum_attempts = remaining_one_use_edges;
	for (size_t attempt = 0;
		attempt < maximum_attempts && remaining_one_use_edges > 0;
		++attempt) {
	    std::ostringstream progress_detail;
	    progress_detail << "remaining one-use edges="
		<< remaining_one_use_edges;
	    wrapper->SetProgressDetail(
		"reconciling exact distinct-edge keyholes",
		job.entity_id, static_cast<uint64_t>(attempt),
		static_cast<uint64_t>(maximum_attempts), "attempts",
		progress_detail.str());
	    const size_t repaired =
		merge_one_exact_duplicate_step_boundary_edge_pair(
		    job.brep.get(), &keyhole_edge_details, true);
	    if (!repaired)
		break;
	    accepted_keyhole_edge_details = keyhole_edge_details;
	    keyhole_edge_repairs += repaired;
	    const size_t next_one_use_edges =
		one_use_edge_count(job.brep.get());
	    if (next_one_use_edges >= remaining_one_use_edges) {
		keyhole_edge_details =
		    "an accepted distinct-edge keyhole merge did not reduce the one-use edge count";
		break;
	    }
	    remaining_one_use_edges = next_one_use_edges;
	}
	if (keyhole_edge_repairs)
	    solid = job.brep->IsSolid();
	for (size_t repair = 0; repair < keyhole_edge_repairs; ++repair)
	    wrapper->RecordRepair(job.entity_id, job.entity_type, "edge_topology",
		"rejoined the reciprocal uses of one exact distinct-edge STEP keyhole");
	if (wrapper->Verbose() && keyhole_edge_repairs)
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": rejoined exact distinct-edge keyhole pair "
		<< accepted_keyhole_edge_details << std::endl;
	else if (wrapper->Verbose() && !keyhole_edge_details.empty())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": distinct-edge keyhole reconciliation rejected: "
		<< keyhole_edge_details << std::endl;
	if (work_expired("distinct-edge STEP keyhole reconciliation"))
	    return;
    }
    if (!solid && !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	std::string merged_edge_details;
	size_t merged_edge_repairs = 0;
	size_t remaining_one_use_edges = one_use_edge_count(job.brep.get());
	/* Each accepted merge consumes two one-use boundary edges and is
	 * transactionally validated.  The initial boundary count is therefore a
	 * topology-derived upper bound which works for both small fixtures and
	 * large patterned solids without an arbitrary iteration limit. */
	const size_t maximum_attempts = remaining_one_use_edges;
	for (size_t attempt = 0;
		attempt < maximum_attempts && remaining_one_use_edges > 0;
		++attempt) {
	    std::ostringstream progress_detail;
	    progress_detail << "remaining one-use edges="
		<< remaining_one_use_edges;
	    wrapper->SetProgressDetail(
		"reconciling exact periodic boundary edge pairs",
		job.entity_id, static_cast<uint64_t>(attempt),
		static_cast<uint64_t>(maximum_attempts), "attempts",
		progress_detail.str());
	    const size_t repaired = merge_one_exact_periodic_boundary_edge_pair(
		job.brep.get(), &merged_edge_details);
	    if (!repaired)
		break;
	    merged_edge_repairs += repaired;
	    const size_t next_one_use_edges =
		one_use_edge_count(job.brep.get());
	    if (next_one_use_edges >= remaining_one_use_edges) {
		merged_edge_details =
		    "an accepted periodic edge merge did not reduce the one-use edge count";
		break;
	    }
	    remaining_one_use_edges = next_one_use_edges;
	}
	if (merged_edge_repairs)
	    solid = job.brep->IsSolid();
	for (size_t repair = 0; repair < merged_edge_repairs; ++repair)
	    wrapper->RecordRepair(job.entity_id, job.entity_type, "edge_topology",
		"merged two coincident periodic STEP boundary edges into one exact seam edge");
	if (wrapper->Verbose() && merged_edge_repairs)
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": merged exact periodic boundary edge pair "
		<< merged_edge_details << std::endl;
	else if (wrapper->Verbose() && !merged_edge_details.empty())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": periodic boundary edge-pair reconciliation rejected: "
		<< merged_edge_details << std::endl;
	if (work_expired("periodic STEP boundary edge reconciliation"))
	    return;
    }
    if (!solid && !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	std::string split_use_failure;
	size_t split_use_repairs = 0;
	size_t remaining_one_use_edges = one_use_edge_count(job.brep.get());
	/* A successful subdivision propagation deletes one unsplit one-use edge
	 * and turns its exact child path into reciprocal two-use edges.  Hence the
	 * current one-use edge count is a strict termination bound.  This replaces
	 * the former 16-pass cap, which left large repeated solids artificially
	 * open after only a fraction of their proven subdivisions were repaired. */
	const size_t maximum_attempts = remaining_one_use_edges;
	for (size_t attempt = 0;
		attempt < maximum_attempts && remaining_one_use_edges > 0;
		++attempt) {
	    std::ostringstream progress_detail;
	    progress_detail << "remaining one-use edges="
		<< remaining_one_use_edges;
	    wrapper->SetProgressDetail(
		"propagating exact shared-edge subdivisions",
		job.entity_id, static_cast<uint64_t>(attempt),
		static_cast<uint64_t>(maximum_attempts), "attempts",
		progress_detail.str());
	    const size_t repaired = reconcile_pre_split_step_edge_uses(
		job.brep.get(), &split_use_failure);
	    if (!repaired)
		break;
	    split_use_repairs += repaired;
	    const size_t next_one_use_edges =
		one_use_edge_count(job.brep.get());
	    if (next_one_use_edges >= remaining_one_use_edges) {
		split_use_failure =
		    "an accepted edge subdivision did not reduce the one-use edge count";
		break;
	    }
	    remaining_one_use_edges = next_one_use_edges;
	}
	if (split_use_repairs)
	    solid = job.brep->IsSolid();
	for (size_t repair = 0; repair < split_use_repairs; ++repair)
	    wrapper->RecordRepair(job.entity_id, job.entity_type, "edge_topology",
		"propagated one exact periodic edge subdivision to its adjacent STEP face");
	if (wrapper->Verbose() && !split_use_failure.empty())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": shared periodic-edge subdivision retained unresolved input: "
		<< split_use_failure << std::endl;
	if (work_expired("shared STEP edge subdivision propagation"))
	    return;
    }
    if (!solid && !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	std::string reused_edge_failure;
	int unresolved_reused_step_edge = 0;
	int unresolved_reused_use_count = 0;
	const size_t reused_edge_repairs = split_reused_edges_by_shell_component(
	    job.brep.get(), &reused_edge_failure, &unresolved_reused_step_edge,
	    &unresolved_reused_use_count);
	for (size_t repair = 0; repair < reused_edge_repairs; ++repair)
	    wrapper->RecordRepair(job.entity_id, job.entity_type, "edge_topology",
		"separated one STEP edge reused by distinct manifold shell components");
	if (reused_edge_repairs)
	    solid = job.brep->IsSolid();
	if (wrapper->Verbose() && !reused_edge_failure.empty())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": reused-edge shell separation retained unresolved input: "
		<< reused_edge_failure << std::endl;
	if (unresolved_reused_step_edge > 0 && !reused_edge_failure.empty())
	    wrapper->RecordDiagnostic(
		brlcad::step::DiagnosticSeverity::Error,
		unresolved_reused_step_edge, "EDGE_CURVE", "edge_topology",
		reused_edge_failure);
	if (work_expired("reused STEP edge shell separation"))
	    return;
    }
    if (!solid &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	/* General hole filling is not a safe STEP import repair.  A complete
	 * three-edge planar boundary is a narrow exception: when it is the only
	 * boundary of an asserted MANIFOLD_SOLID_BREP, the absent triangular face
	 * is uniquely determined.  The helper merely marks this possibility in the
	 * ordinary pass and may mutate only inside the already isolated permissive
	 * inference transaction.  The orientation and final IsSolid() gates below
	 * remain authoritative. */
	std::string cap_details;
	const bool capped = infer_missing_planar_triangular_cap(job.brep.get(),
	    wrapper, job.entity_id, job.entity_type, &cap_details);
	if (capped)
	    solid = job.brep->IsSolid();
	if (wrapper->Verbose() && capped)
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": inferred missing planar triangular cap "
		<< cap_details << std::endl;
	else if (wrapper->Verbose() && !cap_details.empty())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": missing planar triangular cap inference rejected: "
		<< cap_details << std::endl;
	if (work_expired("missing planar triangular cap inference"))
	    return;
    }
    if (!solid && wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	std::string orientation_failure;
	const size_t orientation_repairs = repair_closed_shell_face_orientations(
	    job.brep.get(), &orientation_failure);
	for (size_t repair = 0; repair < orientation_repairs; ++repair)
	    wrapper->RecordRepair(job.entity_id, job.entity_type, "face_orientation",
		"corrected a face orientation from closed-shell edge-use constraints");
	if (orientation_repairs)
	    solid = job.brep->IsSolid();
	else if (wrapper->Verbose() && !orientation_failure.empty())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": closed-shell orientation repair rejected: "
		<< orientation_failure << std::endl;
	if (work_expired("repaired BREP solidness validation"))
	    return;
    }
    if (!solid && wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	std::string split_orientation_failure;
	const size_t split_orientation_repairs =
	    repair_split_closed_edge_loop_orientations(job.brep.get(), wrapper,
		job.entity_id, job.entity_type, &split_orientation_failure);
	for (size_t repair = 0; repair < split_orientation_repairs; ++repair)
	    wrapper->RecordRepair(job.entity_id, job.entity_type, "loop_orientation",
		"corrected a complete split closed-edge loop from closed-shell edge-use constraints");
	if (split_orientation_repairs)
	    solid = job.brep->IsSolid();
	else if (wrapper->Verbose() && !split_orientation_failure.empty())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": split closed-edge loop orientation repair rejected: "
		<< split_orientation_failure << std::endl;
	if (work_expired("split closed-edge loop orientation repair"))
	    return;
    }
    if (!solid) {
	/* Structural validity alone is insufficient for a region.  A permissive
	 * curve choice is accepted only if rebuilding also proves solidness; on
	 * failure retain the fully repaired ordinary candidate below. */
	std::unique_ptr<ON_Brep> ordinary_brep = std::move(job.brep);
	const std::string ordinary_validation_message = job.validation_message;
	if (retry_detached_brep_with_curve_inference(job, wrapper))
	    return;
	job.brep = std::move(ordinary_brep);
	job.validation_message = ordinary_validation_message;
	if (wrapper->Verbose()) {
	    bool oriented = false;
	    bool has_boundary = false;
	    const bool manifold = job.brep->IsManifold(&oriented, &has_boundary);
	    size_t boundary_trims = 0;
	    size_t inconsistent_edges = 0;
	    size_t empty_faces = 0;
	    size_t empty_loops = 0;
	    size_t unclassified_trims = 0;
	    size_t nonmanifold_edge_uses = 0;
	    std::ostringstream boundary_examples;
	    std::ostringstream examples;
	    std::ostringstream nonmanifold_examples;
	    std::set<int> inconsistent_loops;
	    for (int fi = 0; fi < job.brep->m_F.Count(); ++fi)
		if (job.brep->m_F[fi].m_li.Count() < 1)
		    ++empty_faces;
	    for (int li = 0; li < job.brep->m_L.Count(); ++li)
		if (job.brep->m_L[li].m_ti.Count() < 1)
		    ++empty_loops;
	    for (int ti = 0; ti < job.brep->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = job.brep->m_T[ti];
		if (trim.m_type != ON_BrepTrim::boundary &&
			trim.m_type != ON_BrepTrim::mated &&
			trim.m_type != ON_BrepTrim::seam &&
			trim.m_type != ON_BrepTrim::singular) {
		    if (unclassified_trims < 12)
			nonmanifold_examples << " T" << ti << "/type"
			    << static_cast<int>(trim.m_type) << "/E" << trim.m_ei;
		    ++unclassified_trims;
		}
		if ((trim.m_type == ON_BrepTrim::mated ||
			trim.m_type == ON_BrepTrim::seam) &&
			(trim.m_ei < 0 || trim.m_ei >= job.brep->m_E.Count() ||
			 job.brep->m_E[trim.m_ei].m_ti.Count() != 2)) {
		    if (nonmanifold_edge_uses < 12)
			nonmanifold_examples << " T" << ti << "/type"
			    << static_cast<int>(trim.m_type) << "/E" << trim.m_ei
			    << "/STEP" << ((trim.m_ei >= 0 &&
				trim.m_ei < job.brep->m_E.Count()) ?
				job.brep->m_E[trim.m_ei].m_edge_user.i : 0)
			    << "/uses" << ((trim.m_ei >= 0 &&
				trim.m_ei < job.brep->m_E.Count()) ?
				job.brep->m_E[trim.m_ei].m_ti.Count() : -1);
		    ++nonmanifold_edge_uses;
		}
		if (trim.m_type != ON_BrepTrim::boundary)
		    continue;
		if (boundary_trims < 12) {
		    const ON_BrepEdge *edge = trim.Edge();
		    const ON_BrepLoop *loop = trim.Loop();
		    boundary_examples << ' ' << ti << "/E" << trim.m_ei << "/STEP"
			<< (edge ? edge->m_edge_user.i : 0) << "/L" << trim.m_li
			<< "/STEP" << (loop ? loop->m_loop_user.i : 0)
			<< "/iso" << static_cast<int>(trim.m_iso)
			<< "/trim-v" << trim.m_vi[0] << ':' << trim.m_vi[1];
		    if (edge)
			boundary_examples << "/edge-v" << edge->m_vi[0] << ':'
			    << edge->m_vi[1];
		    if (trim.m_vi[0] >= 0 &&
			    trim.m_vi[0] < job.brep->m_V.Count() &&
			    trim.m_vi[1] >= 0 &&
			    trim.m_vi[1] < job.brep->m_V.Count()) {
			const ON_BrepVertex &start_vertex =
			    job.brep->m_V[trim.m_vi[0]];
			const ON_BrepVertex &end_vertex =
			    job.brep->m_V[trim.m_vi[1]];
			boundary_examples << "/step-v"
			    << start_vertex.m_vertex_user.i << ':'
			    << end_vertex.m_vertex_user.i << "/xyz"
			    << start_vertex.point.x << ','
			    << start_vertex.point.y << ','
			    << start_vertex.point.z << ':'
			    << end_vertex.point.x << ','
			    << end_vertex.point.y << ','
			    << end_vertex.point.z;
		    }
		    const ON_Surface *surface = trim.Face() ?
			trim.Face()->SurfaceOf() : NULL;
		    const ON_Interval trim_domain = trim.Domain();
		    if (surface && trim_domain.IsIncreasing())
			boundary_examples << "/derived-iso"
			    << static_cast<int>(surface->IsIsoparametric(
				trim, &trim_domain)) << "/uv"
			    << trim.PointAtStart().x << ':' << trim.PointAtStart().y
			    << "->" << trim.PointAtEnd().x << ':'
			    << trim.PointAtEnd().y << "/closed"
			    << (surface->IsClosed(0) ? 1 : 0)
			    << (surface->IsClosed(1) ? 1 : 0);
		}
		++boundary_trims;
	    }
	    for (int ei = 0; ei < job.brep->m_E.Count(); ++ei) {
		const ON_BrepEdge &edge = job.brep->m_E[ei];
		if (edge.m_ti.Count() != 2)
		    continue;
		const ON_BrepTrim *first = job.brep->Trim(edge.m_ti[0]);
		const ON_BrepTrim *second = job.brep->Trim(edge.m_ti[1]);
		if (!first || !second || !first->Face() || !second->Face())
		    continue;
		const bool first_effective = first->m_bRev3d ^ first->Face()->m_bRev;
		const bool second_effective = second->m_bRev3d ^ second->Face()->m_bRev;
		if (first_effective != second_effective)
		    continue;
		if (inconsistent_edges < 12)
		    examples << ' ' << ei << "/STEP" << edge.m_edge_user.i
			<< "(T" << first->m_trim_index << "/F"
			<< first->Face()->m_face_index << "/L" << first->m_li
			<< "/STEP" << (first->Loop() ?
			    first->Loop()->m_loop_user.i : 0) << "/n"
			<< (first->Loop() ? first->Loop()->TrimCount() : 0)
			<< "/r" << first_effective
			<< ",T" << second->m_trim_index << "/F"
			<< second->Face()->m_face_index << "/L" << second->m_li
			<< "/STEP" << (second->Loop() ?
			    second->Loop()->m_loop_user.i : 0) << "/n"
			<< (second->Loop() ? second->Loop()->TrimCount() : 0)
			<< "/r" << second_effective << ')';
		inconsistent_loops.insert(first->m_li);
		inconsistent_loops.insert(second->m_li);
		++inconsistent_edges;
	    }
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": solid audit manifold=" << (manifold ? "yes" : "no")
		<< " oriented=" << (oriented ? "yes" : "no")
		<< " boundary=" << (has_boundary ? "yes" : "no")
		<< " boundary trims=" << boundary_trims
		<< boundary_examples.str()
		<< " empty faces/loops=" << empty_faces << '/' << empty_loops
		<< " unclassified trims=" << unclassified_trims
		<< " nonmanifold edge uses=" << nonmanifold_edge_uses
		<< nonmanifold_examples.str()
		<< " orientation conflicts=" << inconsistent_edges
		<< examples.str() << std::endl;
	    for (std::set<int>::const_iterator loop_index =
		    inconsistent_loops.begin(); loop_index != inconsistent_loops.end();
		    ++loop_index) {
		if (*loop_index < 0 || *loop_index >= job.brep->m_L.Count())
		    continue;
		const ON_BrepLoop &loop = job.brep->m_L[*loop_index];
		const ON_BrepFace *face = loop.Face();
		std::cerr << job.entity_type << " #" << job.entity_id
		    << ": conflicting loop L" << *loop_index << "/STEP"
		    << loop.m_loop_user.i << "/F" << (face ? face->m_face_index : -1)
		    << "/face-rev" << (face ? face->m_bRev : false) << " trims=";
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = loop.Trim(lti);
		    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		    if (!trim)
			continue;
		    std::cerr << (lti ? ";" : "") << 'T' << trim->m_trim_index
			<< "/E" << trim->m_ei << "/STEP"
			<< (edge ? edge->m_edge_user.i : 0) << "/rev"
			<< trim->m_bRev3d << "/vi" << trim->m_vi[0] << ':'
			<< trim->m_vi[1];
		}
		std::cerr << std::endl;
	    }
	}
	job.status = BREP_NOT_SOLID;
	finish_stage("brep_solidness_validation");
	return;
    }
    finish_stage("brep_solidness_validation");

    if (!job.faceted) {
	job.status = BREP_WRITE_SUCCESS;
	return;
    }

    struct bg_tess_tol tessellation_tolerance = BG_TESS_TOL_INIT_ZERO;
    tessellation_tolerance.abs = job.tolerance;
    ON_Brep_CDT_State *cdt = ON_Brep_CDT_Create(job.brep.get(),
	job.output_name.c_str());
    if (!cdt) {
	job.status = BREP_CONVERSION_FAILED;
	return;
    }
    ON_Brep_CDT_Tol_Set(cdt, &tessellation_tolerance);
    const int tessellation_status = ON_Brep_CDT_Tessellate(cdt, 0, NULL);
    if (tessellation_status != 0) {
	ON_Brep_CDT_Destroy(cdt);
	job.status = tessellation_status > 0 ? BREP_NOT_SOLID :
	    BREP_CONVERSION_FAILED;
	return;
    }

    int *faces = NULL;
    int face_count = 0;
    fastf_t *vertices = NULL;
    int vertex_count = 0;
    if (ON_Brep_CDT_Mesh(&faces, &face_count, &vertices, &vertex_count,
	    NULL, NULL, NULL, NULL, cdt, 0, NULL) < 0 || !faces || !vertices ||
	    face_count <= 0 || vertex_count <= 0) {
	ON_Brep_CDT_Destroy(cdt);
	if (vertices) bu_free(vertices, "detached faceted BREP CDT vertices");
	if (faces) bu_free(faces, "detached faceted BREP CDT faces");
	job.status = BREP_CONVERSION_FAILED;
	return;
    }
    ON_Brep_CDT_Destroy(cdt);
    job.bot_vertices.assign(vertices, vertices + static_cast<size_t>(vertex_count) * 3);
    job.bot_faces.assign(faces, faces + static_cast<size_t>(face_count) * 3);
    bu_free(vertices, "detached faceted BREP CDT vertices");
    bu_free(faces, "detached faceted BREP CDT faces");
    job.status = BREP_WRITE_SUCCESS;
}


/* Some exporters reuse one EDGE_CURVE topology entity in otherwise disjoint
 * CLOSED_SHELL components.  OpenNURBS correctly treats the resulting edge
 * with four or more trims as non-manifold.  When the ordinary two-use edges
 * independently partition those trims into exact pairs, give each shell
 * component its own topological edge while duplicating the unchanged 3-D
 * curve.  Authoritative CLOSED_SHELL labels prevent pairing uses across
 * shells, while the ordinary two-use adjacency graph refines each label.
 * The refinement matters for exporters which place multiple otherwise
 * disjoint manifold components in one nominal CLOSED_SHELL and reuse one
 * EDGE_CURVE between them.  This is transactional and cannot guess a pairing:
 * every use must belong to a distinct, proven two-use component and the
 * completed BREP must remain structurally valid. */
static size_t
split_reused_edges_by_shell_component(ON_Brep *brep, std::string *failure,
	int *unresolved_step_edge, int *unresolved_use_count)
{
    if (failure)
	failure->clear();
    if (unresolved_step_edge)
	*unresolved_step_edge = 0;
    if (unresolved_use_count)
	*unresolved_use_count = 0;
    if (!brep || brep->m_F.Count() < 1)
	return 0;

    std::vector<int> parent(static_cast<size_t>(brep->m_F.Count()));
    for (int fi = 0; fi < brep->m_F.Count(); ++fi)
	parent[static_cast<size_t>(fi)] = fi;
    const auto find_root = [&parent](int face) {
	int root = face;
	while (parent[static_cast<size_t>(root)] != root)
	    root = parent[static_cast<size_t>(root)];
	while (parent[static_cast<size_t>(face)] != face) {
	    const int next = parent[static_cast<size_t>(face)];
	    parent[static_cast<size_t>(face)] = root;
	    face = next;
	}
	return root;
    };
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const ON_BrepTrim *first = brep->Trim(edge.m_ti[0]);
	const ON_BrepTrim *second = brep->Trim(edge.m_ti[1]);
	const ON_BrepFace *first_face = first ? first->Face() : NULL;
	const ON_BrepFace *second_face = second ? second->Face() : NULL;
	if (!first_face || !second_face)
	    continue;
	const int first_index = first_face->m_face_index;
	const int second_index = second_face->m_face_index;
	if (first_index < 0 || first_index >= brep->m_F.Count() ||
		second_index < 0 || second_index >= brep->m_F.Count())
	    continue;
	const int first_root = find_root(first_index);
	const int second_root = find_root(second_index);
	if (first_root != second_root)
	    parent[static_cast<size_t>(second_root)] = first_root;
    }

    struct SplitPlan {
	int edge_index;
	std::vector<std::vector<int> > trim_groups;
    };
    std::vector<SplitPlan> plans;
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_ti.Count() <= 2)
	    continue;
	std::map<std::pair<int, int>, std::vector<int> > by_component;
	bool complete = true;
	for (int use = 0; use < edge.m_ti.Count(); ++use) {
	    const ON_BrepTrim *trim = brep->Trim(edge.m_ti[use]);
	    const ON_BrepFace *face = trim ? trim->Face() : NULL;
	    if (!trim || !face || face->m_face_index < 0 ||
		    face->m_face_index >= brep->m_F.Count()) {
		complete = false;
		break;
	    }
	    const int shell_label = face->m_face_user.i > 0 ?
		face->m_face_user.i : 0;
	    const int adjacency_component = find_root(face->m_face_index);
	    by_component[std::make_pair(shell_label, adjacency_component)].push_back(
		trim->m_trim_index);
	}
	if (!complete || by_component.size() < 2) {
	    if (failure && failure->empty()) {
		std::ostringstream reason;
		reason << "STEP edge #" << edge.m_edge_user.i << " has "
		    << edge.m_ti.Count() << " uses in one authoritative "
		       "CLOSED_SHELL/ordinary-adjacency component; no exact "
		       "two-use manifold pairing is provable";
		*failure = reason.str();
		if (unresolved_step_edge)
		    *unresolved_step_edge = edge.m_edge_user.i;
		if (unresolved_use_count)
		    *unresolved_use_count = edge.m_ti.Count();
	    }
	    continue;
	}
	SplitPlan plan;
	plan.edge_index = ei;
	for (std::map<std::pair<int, int>, std::vector<int> >::const_iterator group =
		by_component.begin(); group != by_component.end(); ++group) {
	    if (group->second.size() != 2) {
		complete = false;
		break;
	    }
	    plan.trim_groups.push_back(group->second);
	}
	if (complete)
	    plans.push_back(plan);
	else if (failure && failure->empty()) {
	    std::ostringstream reason;
	    reason << "STEP edge #" << edge.m_edge_user.i
		<< " did not have exactly two uses in every shell component";
	    *failure = reason.str();
	    if (unresolved_step_edge)
		*unresolved_step_edge = edge.m_edge_user.i;
	    if (unresolved_use_count)
		*unresolved_use_count = edge.m_ti.Count();
	}
    }
    if (plans.empty())
	return 0;

    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
    size_t separated_edges = 0;
    for (std::vector<SplitPlan>::const_iterator plan = plans.begin();
	    plan != plans.end(); ++plan) {
	if (plan->edge_index < 0 || plan->edge_index >= brep->m_E.Count() ||
		plan->trim_groups.size() < 2) {
	    if (failure) *failure = "a reused-edge split plan became invalid";
	    *brep = *rollback;
	    return 0;
	}
	/* Adding an edge can grow m_E, so copy every source property before the
	 * first append and address the original again only by its stable index. */
	const ON_BrepEdge &source = brep->m_E[plan->edge_index];
	const int source_id = source.m_edge_user.i;
	const int first_vertex = source.m_vi[0];
	const int second_vertex = source.m_vi[1];
	const double tolerance = source.m_tolerance;
	if (first_vertex < 0 || first_vertex >= brep->m_V.Count() ||
		second_vertex < 0 || second_vertex >= brep->m_V.Count()) {
	    if (failure) *failure = "a reused edge had invalid topology vertices";
	    *brep = *rollback;
	    return 0;
	}
	brep->m_E[plan->edge_index].m_ti.SetCount(0);
	for (std::vector<int>::const_iterator trim =
		plan->trim_groups.front().begin();
		trim != plan->trim_groups.front().end(); ++trim)
	    brep->m_E[plan->edge_index].m_ti.Append(*trim);
	for (size_t group = 1; group < plan->trim_groups.size(); ++group) {
	    ON_Curve *curve = brep->m_E[plan->edge_index].DuplicateCurve();
	    const int curve_index = curve ? brep->AddEdgeCurve(curve) : -1;
	    if (curve_index < 0) {
		delete curve;
		if (failure) *failure = "a reused edge curve could not be duplicated";
		*brep = *rollback;
		return 0;
	    }
	    ON_BrepEdge &copy = brep->NewEdge(brep->m_V[first_vertex],
		brep->m_V[second_vertex], curve_index, NULL, tolerance);
	    copy.m_edge_user.i = source_id;
	    copy.m_tolerance = tolerance;
	    for (std::vector<int>::const_iterator trim =
		    plan->trim_groups[group].begin();
		    trim != plan->trim_groups[group].end(); ++trim) {
		if (*trim < 0 || *trim >= brep->m_T.Count()) {
		    if (failure) *failure = "a reused-edge trim index became invalid";
		    *brep = *rollback;
		    return 0;
		}
		brep->m_T[*trim].m_ei = copy.m_edge_index;
		copy.m_ti.Append(*trim);
	    }
	}
	++separated_edges;
    }

    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    std::string unsafe_topology;
    if (!brep_topology_references_are_safe(brep, &unsafe_topology) ||
	    !brep->IsValid(&validation_log)) {
	if (failure) {
	    ON_String text(validation_messages);
	    *failure = !unsafe_topology.empty() ? unsafe_topology : text.Array();
	}
	*brep = *rollback;
	return 0;
    }
    return separated_edges;
}


/* A native-domain pcurve may require an exact topological subdivision even
 * though the same STEP EDGE_CURVE remains continuous on its adjacent face.
 * Face-local construction can therefore leave two one-use child edges beside
 * one one-use original edge.  Propagate that already-proved 3-D subdivision
 * to the adjacent pcurve as one transaction.  This changes neither curve nor
 * surface geometry: every child is proved to partition the original edge,
 * every new pcurve is cut from the existing adjacent trim, and the completed
 * reciprocal topology must pass OpenNURBS validation before commit. */
static size_t
reconcile_pre_split_step_edge_uses(ON_Brep *brep, std::string *failure)
{
    if (failure)
	failure->clear();
    if (!brep || brep->m_E.Count() < 3)
	return 0;

    struct PathUse {
	int edge = -1;
	bool reversed = false;
    };
    struct SearchState {
	int vertex = -1;
	std::vector<PathUse> path;
    };

    for (int original_index = 0; original_index < brep->m_E.Count();
	    ++original_index) {
	const ON_BrepEdge &original = brep->m_E[original_index];
	if (original.m_edge_user.i <= 0 || original.m_ti.Count() != 1 ||
		original.m_vi[0] < 0 || original.m_vi[1] < 0 ||
		original.m_vi[0] >= brep->m_V.Count() ||
		original.m_vi[1] >= brep->m_V.Count() ||
		!original.EdgeCurveOf())
	    continue;
	const int old_trim_index = original.m_ti[0];
	if (old_trim_index < 0 || old_trim_index >= brep->m_T.Count())
	    continue;
	const ON_BrepTrim &old_trim = brep->m_T[old_trim_index];
	const ON_BrepLoop *old_loop = old_trim.Loop();
	const ON_Surface *old_surface = old_trim.SurfaceOf();
	if (!old_loop || !old_surface || !old_trim.TrimCurveOf() ||
		old_trim.m_type != ON_BrepTrim::boundary)
	    continue;

	std::vector<int> candidates;
	for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    if (ei == original_index ||
		    edge.m_edge_user.i != original.m_edge_user.i ||
		    edge.m_ti.Count() != 1 || edge.m_vi[0] < 0 ||
		    edge.m_vi[1] < 0 ||
		    edge.m_vi[0] >= brep->m_V.Count() ||
		    edge.m_vi[1] >= brep->m_V.Count() || !edge.EdgeCurveOf())
		continue;
	    const ON_BrepTrim *use = brep->Trim(edge.m_ti[0]);
	    if (use && use->m_type == ON_BrepTrim::boundary)
		candidates.push_back(ei);
	}
	if (candidates.size() < 2)
	    continue;
	double topology_tolerance = std::max(LocalUnits::tolerance,
	    std::max(original.m_tolerance,
		std::max(old_trim.m_tolerance[0], old_trim.m_tolerance[1])));
	for (std::vector<int>::const_iterator candidate = candidates.begin();
		candidate != candidates.end(); ++candidate)
	    topology_tolerance = std::max(topology_tolerance,
		brep->m_E[*candidate].m_tolerance);

	std::deque<SearchState> search;
	search.push_back({original.m_vi[0], std::vector<PathUse>()});
	std::vector<PathUse> path;
	while (!search.empty() && path.empty()) {
	    SearchState state = search.front();
	    search.pop_front();
	    if (state.path.size() >= 8)
		continue;
	    for (std::vector<int>::const_iterator candidate = candidates.begin();
		    candidate != candidates.end(); ++candidate) {
		bool used = false;
		for (std::vector<PathUse>::const_iterator existing =
			state.path.begin(); existing != state.path.end(); ++existing)
		    if (existing->edge == *candidate) {
			used = true;
			break;
		    }
		if (used)
		    continue;
		const ON_BrepEdge &edge = brep->m_E[*candidate];
		bool reversed = false;
		int next_vertex = -1;
		if (edge.m_vi[0] == state.vertex) {
		    next_vertex = edge.m_vi[1];
		} else if (edge.m_vi[1] == state.vertex) {
		    next_vertex = edge.m_vi[0];
		    reversed = true;
		} else {
		    continue;
		}
		SearchState next = state;
		next.vertex = next_vertex;
		next.path.push_back({*candidate, reversed});
		if (next_vertex == original.m_vi[1] && next.path.size() >= 2) {
		    path.swap(next.path);
		    break;
		}
		bool revisits_vertex = next_vertex == original.m_vi[0];
		for (std::vector<PathUse>::const_iterator existing =
			next.path.begin(); !revisits_vertex &&
			existing + 1 != next.path.end(); ++existing) {
		    const ON_BrepEdge &prior = brep->m_E[existing->edge];
		    const int prior_end = existing->reversed ? prior.m_vi[0] :
			prior.m_vi[1];
		    revisits_vertex = prior_end == next_vertex;
		}
		if (!revisits_vertex)
		    search.push_back(next);
	    }
	}
	/* A split point within the validated edge tolerance can collapse onto an
	 * existing topology vertex.  The exact parameter partition is then a
	 * lollipop path: one ordinary child followed or preceded by a closed child
	 * at the same vertex.  Vertex-only BFS deliberately rejects such revisits,
	 * so recover the ordered path from the immutable child curve domains and
	 * still require exact endpoint connectivity and the dense locus proof
	 * below. */
	if (path.size() < 2) {
	    const ON_Interval source_domain = original.Domain();
	    const double parameter_guard = source_domain.IsIncreasing() ?
		std::max(ON_ZERO_TOLERANCE,
		    source_domain.Length() * 1.0e-10) : ON_ZERO_TOLERANCE;
	    const auto parameter_ordered_path = [&](bool descending,
		    std::vector<PathUse> *ordered) {
		if (!ordered || !source_domain.IsIncreasing()) return false;
		ordered->clear();
		std::vector<int> sorted(candidates);
		std::sort(sorted.begin(), sorted.end(),
		    [brep, descending](int left, int right) {
			const double l = brep->m_E[left].Domain().Min();
			const double r = brep->m_E[right].Domain().Min();
			return descending ? l > r : l < r;
		    });
		if (sorted.size() < 2) return false;
		const ON_Interval first_domain = brep->m_E[sorted.front()].Domain();
		const ON_Interval last_domain = brep->m_E[sorted.back()].Domain();
		if (!first_domain.IsIncreasing() || !last_domain.IsIncreasing())
		    return false;
		if ((!descending && (fabs(first_domain.Min() - source_domain.Min()) >
			parameter_guard || fabs(last_domain.Max() -
			source_domain.Max()) > parameter_guard)) ||
		    (descending && (fabs(first_domain.Max() - source_domain.Max()) >
			parameter_guard || fabs(last_domain.Min() -
			source_domain.Min()) > parameter_guard)))
		    return false;
		for (size_t child = 1; child < sorted.size(); ++child) {
		    const ON_Interval previous = brep->m_E[sorted[child - 1]].Domain();
		    const ON_Interval current = brep->m_E[sorted[child]].Domain();
		    if (!previous.IsIncreasing() || !current.IsIncreasing() ||
			fabs((descending ? previous.Min() : previous.Max()) -
			    (descending ? current.Max() : current.Min())) >
			parameter_guard)
			return false;
		}
		int vertex = original.m_vi[0];
		for (std::vector<int>::const_iterator child = sorted.begin();
			child != sorted.end(); ++child) {
		    const ON_BrepEdge &edge = brep->m_E[*child];
		    bool reversed = descending;
		    int next_vertex = -1;
		    if (edge.m_vi[0] == edge.m_vi[1] && edge.m_vi[0] == vertex) {
			next_vertex = vertex;
		    } else if (edge.m_vi[descending ? 1 : 0] == vertex) {
			next_vertex = edge.m_vi[descending ? 0 : 1];
		    } else {
			return false;
		    }
		    ordered->push_back({*child, reversed});
		    vertex = next_vertex;
		}
		return vertex == original.m_vi[1];
	    };
	    if (!parameter_ordered_path(false, &path))
		(void)parameter_ordered_path(true, &path);
	}
	if (path.size() < 2) {
	    if (failure && failure->empty()) {
		std::ostringstream reason;
		reason << "STEP edge " << original.m_edge_user.i << "/E"
		    << original_index << " vertices " << original.m_vi[0] << ':'
		    << original.m_vi[1] << " had " << candidates.size()
		    << " child edges but no matching vertex-index path";
		for (std::vector<int>::const_iterator candidate = candidates.begin();
			candidate != candidates.end(); ++candidate)
		    reason << " E" << *candidate << '(' <<
			brep->m_E[*candidate].m_vi[0] << ':' <<
			brep->m_E[*candidate].m_vi[1] << ')';
		*failure = reason.str();
	    }
	    continue;
	}

	for (std::vector<PathUse>::const_iterator use = path.begin();
		use != path.end(); ++use)
	    topology_tolerance = std::max(topology_tolerance,
		brep->m_E[use->edge].m_tolerance);

	const ON_Interval original_domain = original.Domain();
	struct PartitionWitness {
	    ON_3dPoint point;
	    double direct_distance;
	    double child_parameter;
	    int child_edge;
	    int child_sample;
	};
	std::vector<PartitionWitness> unresolved_partition_points;
	bool exact_partition = original_domain.IsIncreasing();
	double rejected_direct_distance = DBL_MAX;
	double rejected_closest_distance = DBL_MAX;
	double rejected_child_parameter = ON_UNSET_VALUE;
	int rejected_child_edge = -1;
	int rejected_child_sample = -1;
	for (std::vector<PathUse>::const_iterator use = path.begin();
		exact_partition && use != path.end(); ++use) {
	    const ON_BrepEdge &child = brep->m_E[use->edge];
	    const ON_Interval child_domain = child.Domain();
	    for (int sample = 0; exact_partition && sample <= 64; ++sample) {
		const double child_parameter = child_domain.ParameterAt(
		    static_cast<double>(sample) / 64.0);
		const ON_3dPoint point = child.PointAt(child_parameter);
		/* split_pullback_segment_edge() constructs every child by trimming
		 * the immutable source STEP curve and deliberately preserves that
		 * curve's native parameter interval.  Prove that invariant directly
		 * before consulting a global closest-point solve.  The latter can
		 * converge to the wrong local branch on the high-order closed curves
		 * which motivate this topology reconciliation. */
		double direct_distance = DBL_MAX;
		const double domain_guard = std::max(ON_ZERO_TOLERANCE,
		    original_domain.Length() * 1.0e-12);
		if (point.IsValid() && original_domain.IsIncreasing() &&
			child_parameter >= original_domain.Min() - domain_guard &&
			child_parameter <= original_domain.Max() + domain_guard) {
		    const double original_parameter = std::max(original_domain.Min(),
			std::min(original_domain.Max(), child_parameter));
		    const ON_3dPoint corresponding = original.PointAt(
			original_parameter);
		    if (corresponding.IsValid())
			direct_distance = point.DistanceTo(corresponding);
		}
		if (direct_distance > topology_tolerance)
		    unresolved_partition_points.push_back({point, direct_distance,
			child_parameter, use->edge, sample});
	    }
	}
	if (exact_partition && !unresolved_partition_points.empty()) {
	    std::vector<ON_3dPoint> points;
	    points.reserve(unresolved_partition_points.size());
	    for (std::vector<PartitionWitness>::const_iterator witness =
		    unresolved_partition_points.begin();
		    witness != unresolved_partition_points.end(); ++witness)
		points.push_back(witness->point);
	    size_t rejected = 0;
	    exact_partition = step_curve_locus_contains_points(&original,
		points.data(), points.size(), topology_tolerance, &rejected,
		&rejected_closest_distance);
	    if (!exact_partition && rejected < unresolved_partition_points.size()) {
		const PartitionWitness &witness =
		    unresolved_partition_points[rejected];
		rejected_direct_distance = witness.direct_distance;
		rejected_child_parameter = witness.child_parameter;
		rejected_child_edge = witness.child_edge;
		rejected_child_sample = witness.child_sample;
	    }
	}
	if (!exact_partition) {
	    if (failure && failure->empty()) {
		std::ostringstream reason;
		reason << "one-use child edge chain did not densely partition its "
		    "original STEP edge: E" << rejected_child_edge << " sample "
		    << rejected_child_sample << " parameter "
		    << rejected_child_parameter << " direct/closest distance "
		    << rejected_direct_distance << '/' << rejected_closest_distance
		    << " tolerance " << topology_tolerance << " original domain "
		    << original_domain.Min() << ':' << original_domain.Max()
		    << " original E" << original_index << " vertices "
		    << original.m_vi[0] << ':' << original.m_vi[1]
		    << " c3=" << original.m_c3i;
		const ON_Curve *original_curve = original.EdgeCurveOf();
		if (original_curve)
		    reason << " c3-domain=" << original_curve->Domain().Min()
			<< ':' << original_curve->Domain().Max();
		for (std::vector<PathUse>::const_iterator use = path.begin();
			use != path.end(); ++use) {
		    const ON_BrepEdge &child = brep->m_E[use->edge];
		    reason << " child E" << use->edge << " vertices "
			<< child.m_vi[0] << ':' << child.m_vi[1]
			<< " domain=" << child.Domain().Min() << ':'
			<< child.Domain().Max() << " c3=" << child.m_c3i;
		    const ON_Curve *child_curve = child.EdgeCurveOf();
		    if (child_curve)
			reason << " c3-domain=" << child_curve->Domain().Min()
			    << ':' << child_curve->Domain().Max();
		    reason << " path-reversed=" << (use->reversed ? 1 : 0);
		}
		*failure = reason.str();
	    }
	    continue;
	}

	std::vector<PathUse> trim_path(path);
	if (old_trim.m_bRev3d) {
	    std::reverse(trim_path.begin(), trim_path.end());
	    for (std::vector<PathUse>::iterator use = trim_path.begin();
		    use != trim_path.end(); ++use)
		use->reversed = !use->reversed;
	}
	std::vector<ON_3dPoint> split_points;
	for (size_t use = 0; use + 1 < trim_path.size(); ++use) {
	    const ON_BrepEdge &edge = brep->m_E[trim_path[use].edge];
	    const ON_BrepEdge &next_edge = brep->m_E[trim_path[use + 1].edge];
	    const int end_vertex = trim_path[use].reversed ? edge.m_vi[0] :
		edge.m_vi[1];
	    if (end_vertex < 0 || end_vertex >= brep->m_V.Count()) {
		exact_partition = false;
		break;
	    }
	    const ON_3dPoint curve_end = trim_path[use].reversed ?
		edge.PointAtStart() : edge.PointAtEnd();
	    const ON_3dPoint next_start = trim_path[use + 1].reversed ?
		next_edge.PointAtEnd() : next_edge.PointAtStart();
	    if (curve_end.IsValid() && next_start.IsValid() &&
		    curve_end.DistanceTo(next_start) <= topology_tolerance)
		split_points.push_back(0.5 * (curve_end + next_start));
	    else
		split_points.push_back(brep->m_V[end_vertex].point);
	}
	if (!exact_partition || split_points.empty()) {
	    if (failure && failure->empty())
		*failure = "one-use child edge chain did not define valid interior split vertices";
	    continue;
	}

	const ON_Interval trim_domain = old_trim.Domain();
	if (!trim_domain.IsIncreasing())
	    continue;
	std::vector<double> split_parameters;
	for (std::vector<ON_3dPoint>::const_iterator target =
		split_points.begin(); target != split_points.end(); ++target) {
	    double best_parameter = trim_domain.Min();
	    double best_distance = DBL_MAX;
	    int best_sample = 0;
	    for (int sample = 0; sample <= kBoundaryParameterSearchSegments;
		    ++sample) {
		const double parameter = trim_domain.ParameterAt(
		    static_cast<double>(sample) /
		    kBoundaryParameterSearchSegments);
		const ON_3dPoint uv = old_trim.PointAt(parameter);
		const ON_3dPoint lift = uv.IsValid() ? old_surface->PointAt(
		    uv.x, uv.y) : ON_3dPoint::UnsetPoint;
		const double distance = lift.IsValid() ? lift.DistanceTo(*target) :
		    DBL_MAX;
		if (distance < best_distance) {
		    best_distance = distance;
		    best_parameter = parameter;
		    best_sample = sample;
		}
	    }
	    double minimum = trim_domain.ParameterAt(
		static_cast<double>(std::max(0, best_sample - 1)) /
		kBoundaryParameterSearchSegments);
	    double maximum = trim_domain.ParameterAt(
		static_cast<double>(std::min(kBoundaryParameterSearchSegments,
		    best_sample + 1)) / kBoundaryParameterSearchSegments);
	    const auto lifted_distance = [&old_trim, old_surface, target](
		    double parameter) {
		const ON_3dPoint uv = old_trim.PointAt(parameter);
		const ON_3dPoint lift = uv.IsValid() ? old_surface->PointAt(
		    uv.x, uv.y) : ON_3dPoint::UnsetPoint;
		return lift.IsValid() ? lift.DistanceTo(*target) : DBL_MAX;
	    };
	    for (int iteration = 0; iteration < 64; ++iteration) {
		const double first = minimum + (maximum - minimum) / 3.0;
		const double second = maximum - (maximum - minimum) / 3.0;
		if (lifted_distance(first) <= lifted_distance(second))
		    maximum = second;
		else
		    minimum = first;
	    }
	    const double refined = 0.5 * (minimum + maximum);
	    const double refined_distance = lifted_distance(refined);
	    if (refined_distance < best_distance) {
		best_parameter = refined;
		best_distance = refined_distance;
	    }
	    const double domain_guard = std::max(ON_ZERO_TOLERANCE,
		trim_domain.Length() * 1.0e-10);
	    if (best_distance > topology_tolerance ||
		    best_parameter <= trim_domain.Min() + domain_guard ||
		    best_parameter >= trim_domain.Max() - domain_guard) {
		exact_partition = false;
		break;
	    }
	    split_parameters.push_back(best_parameter);
	}
	if (!exact_partition || split_parameters.size() + 1 != trim_path.size()) {
	    if (failure && failure->empty())
		*failure = "adjacent pcurve did not locate every exact child-edge split vertex";
	    continue;
	}
	for (size_t parameter = 1; parameter < split_parameters.size(); ++parameter)
	    if (split_parameters[parameter] <= split_parameters[parameter - 1] +
		    trim_domain.Length() * 1.0e-10)
		exact_partition = false;
	if (!exact_partition) {
	    if (failure && failure->empty())
		*failure = "adjacent pcurve split parameters were not strictly ordered";
	    continue;
	}

	std::unique_ptr<ON_Curve> remaining(old_trim.DuplicateCurve());
	std::vector<std::unique_ptr<ON_Curve> > pieces;
	for (std::vector<double>::const_iterator parameter =
		split_parameters.begin(); parameter != split_parameters.end();
		++parameter) {
	    ON_Curve *left_raw = NULL;
	    ON_Curve *right_raw = NULL;
	    if (!remaining || !remaining->Split(*parameter, left_raw, right_raw)) {
		delete left_raw;
		delete right_raw;
		exact_partition = false;
		break;
	    }
	    pieces.push_back(std::unique_ptr<ON_Curve>(left_raw));
	    remaining.reset(right_raw);
	}
	if (!exact_partition || !remaining) {
	    if (failure && failure->empty())
		*failure = "adjacent pcurve rejected an exact child-edge split parameter";
	    continue;
	}
	pieces.push_back(std::move(remaining));
	for (std::vector<std::unique_ptr<ON_Curve> >::iterator piece =
		pieces.begin(); exact_partition && piece != pieces.end(); ++piece) {
	    std::unique_ptr<ON_NurbsCurve> detached(new ON_NurbsCurve());
	    if (!*piece || !(*piece)->GetNurbForm(*detached) ||
		    !detached->IsValid()) {
		exact_partition = false;
		break;
	    }
	    piece->reset(detached.release());
	}
	if (!exact_partition || pieces.size() != trim_path.size()) {
	    if (failure && failure->empty())
		*failure = "adjacent split pcurves could not be detached as exact NURBS curves";
	    continue;
	}

	const auto pieces_follow_path = [&](const std::vector<PathUse> &candidate_path) {
	    if (candidate_path.size() != pieces.size())
		return false;
	    bool follows = true;
	    rejected_child_edge = -1;
	    rejected_child_sample = -1;
	    rejected_direct_distance = DBL_MAX;
	    rejected_closest_distance = DBL_MAX;
	    for (size_t piece_index = 0; follows &&
		    piece_index < pieces.size(); ++piece_index) {
		const ON_BrepEdge &child =
		    brep->m_E[candidate_path[piece_index].edge];
		const bool reversed = candidate_path[piece_index].reversed;
		const ON_Interval piece_domain = pieces[piece_index]->Domain();
		const ON_Interval child_domain = child.Domain();
		if (!piece_domain.IsIncreasing() || !child_domain.IsIncreasing())
		    return false;
		std::vector<ON_3dPoint> unmatched_lifts;
		std::vector<int> unmatched_samples;
		double rejected_piece_direct_distance = DBL_MAX;
		for (int sample = 0; follows &&
			sample <= kDenseValidationSegments; ++sample) {
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint uv = pieces[piece_index]->PointAt(
			piece_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = uv.IsValid() ? old_surface->PointAt(
			uv.x, uv.y) : ON_3dPoint::UnsetPoint;
		    const ON_3dPoint corresponding = child.PointAt(
			child_domain.ParameterAt(reversed ? 1.0 - fraction : fraction));
		    const double distance = lift.IsValid() && corresponding.IsValid() ?
			lift.DistanceTo(corresponding) : DBL_MAX;
		    if (!lift.IsValid()) {
			follows = false;
			rejected_child_edge = candidate_path[piece_index].edge;
			rejected_piece_direct_distance = distance;
			rejected_child_sample = sample;
			break;
		    }
		    if (distance > topology_tolerance) {
			unmatched_lifts.push_back(lift);
			unmatched_samples.push_back(sample);
			rejected_piece_direct_distance = distance;
		    }
		}
		if (follows && !unmatched_lifts.empty()) {
		    size_t rejected = 0;
		    double locus_distance = DBL_MAX;
		    follows = step_curve_locus_contains_points(
			child.EdgeCurveOf(), unmatched_lifts.data(),
			unmatched_lifts.size(), topology_tolerance, &rejected,
			&locus_distance);
		    if (!follows) {
			rejected_child_edge = candidate_path[piece_index].edge;
			rejected_child_sample = rejected < unmatched_samples.size() ?
			    unmatched_samples[rejected] : -1;
			rejected_direct_distance = rejected_piece_direct_distance;
			rejected_closest_distance = locus_distance;
		    }
		}
	    }
	    return follows;
	};
	exact_partition = pieces_follow_path(trim_path);
	/* A closed original edge has the same topology vertex at both ends, so
	 * graph connectivity alone cannot distinguish the two directions around
	 * its child cycle.  Prove the opposite traversal against the immutable
	 * adjacent pcurve before rejecting it. */
	if (!exact_partition && original.m_vi[0] == original.m_vi[1]) {
	    std::vector<PathUse> opposite_path(trim_path);
	    std::reverse(opposite_path.begin(), opposite_path.end());
	    for (std::vector<PathUse>::iterator use = opposite_path.begin();
		    use != opposite_path.end(); ++use)
		use->reversed = !use->reversed;
	    if (pieces_follow_path(opposite_path)) {
		trim_path.swap(opposite_path);
		exact_partition = true;
	    }
	}
	if (!exact_partition) {
	    if (failure && failure->empty()) {
		std::ostringstream reason;
		reason << "an adjacent split pcurve did not densely follow its child "
		    "STEP edge E" << rejected_child_edge << " at sample "
		    << rejected_child_sample << " direct/locus distance "
		    << rejected_direct_distance << '/' << rejected_closest_distance
		    << " tolerance " << topology_tolerance;
		*failure = reason.str();
	    }
	    continue;
	}

	std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	const int loop_index = old_trim.m_li;
	const ON_BrepTrim::TYPE inherited_type = old_trim.m_type;
	std::vector<int> original_loop_trims(old_loop->m_ti.Array(),
	    old_loop->m_ti.Array() + old_loop->m_ti.Count());
	std::vector<int> new_trim_indices;
	bool installed = loop_index >= 0 && loop_index < brep->m_L.Count();
	for (size_t piece_index = 0; installed &&
		piece_index < pieces.size(); ++piece_index) {
	    const int edge_index = trim_path[piece_index].edge;
	    const int c2 = brep->AddTrimCurve(pieces[piece_index].release());
	    if (c2 < 0 || edge_index < 0 || edge_index >= brep->m_E.Count()) {
		installed = false;
		break;
	    }
	    ON_BrepTrim &created = brep->NewTrim(brep->m_E[edge_index],
		trim_path[piece_index].reversed, brep->m_L[loop_index], c2);
	    if (created.m_trim_index < 0) {
		installed = false;
		break;
	    }
	    created.m_type = inherited_type;
	    created.m_tolerance[0] = topology_tolerance;
	    created.m_tolerance[1] = topology_tolerance;
	    const ON_Interval created_domain = created.Domain();
	    created.m_iso = old_surface->IsIsoparametric(created,
		&created_domain);
	    new_trim_indices.push_back(created.m_trim_index);
	}
	if (!installed || new_trim_indices.size() != trim_path.size()) {
	    *brep = *rollback;
	    continue;
	}
	ON_BrepLoop &replacement_loop = brep->m_L[loop_index];
	replacement_loop.m_ti.SetCount(0);
	for (std::vector<int>::const_iterator trim = original_loop_trims.begin();
		trim != original_loop_trims.end(); ++trim) {
	    if (*trim != old_trim_index) {
		replacement_loop.m_ti.Append(*trim);
		brep->m_T[*trim].m_li = loop_index;
		continue;
	    }
	    for (std::vector<int>::const_iterator replacement =
		    new_trim_indices.begin(); replacement != new_trim_indices.end();
		    ++replacement) {
		replacement_loop.m_ti.Append(*replacement);
		brep->m_T[*replacement].m_li = loop_index;
	    }
	}
	brep->DeleteTrim(brep->m_T[old_trim_index], false);
	brep->DeleteEdge(brep->m_E[original_index], false);
	std::string unsafe_topology;
	ON_wString validation_messages;
	ON_TextLog validation_log(validation_messages);
	if (!brep->Compact() || !brep->SetTrimTypeFlags(false) ||
		!brep_topology_references_are_safe(brep, &unsafe_topology) ||
		!brep->IsValid(&validation_log)) {
	    if (failure && failure->empty()) {
		ON_String text(validation_messages);
		*failure = !unsafe_topology.empty() ? unsafe_topology : text.Array();
	    }
	    *brep = *rollback;
	    continue;
	}
	if (failure)
	    failure->clear();
	return 1;
    }
    if (failure && failure->empty())
	*failure = "no exact one-use STEP edge subdivision matched an adjacent boundary use";
    return 0;
}


static size_t
repair_closed_shell_face_orientations(ON_Brep *brep, std::string *failure)
{
    if (failure)
	failure->clear();
    if (!brep || brep->m_F.Count() < 2)
	return 0;

    bool oriented = false;
    bool has_boundary = false;
    if (!brep->IsManifold(&oriented, &has_boundary) || oriented || has_boundary)
	return 0;

    struct Constraint {
	int other;
	int parity;
	int edge;
    };
    std::vector<std::vector<Constraint> > graph(
	static_cast<size_t>(brep->m_F.Count()));
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const ON_BrepTrim *first = brep->Trim(edge.m_ti[0]);
	const ON_BrepTrim *second = brep->Trim(edge.m_ti[1]);
	const ON_BrepFace *first_face = first ? first->Face() : NULL;
	const ON_BrepFace *second_face = second ? second->Face() : NULL;
	if (!first || !second || !first_face || !second_face ||
		first_face == second_face)
	    continue;
	const int first_index = first_face->m_face_index;
	const int second_index = second_face->m_face_index;
	if (first_index < 0 || first_index >= brep->m_F.Count() ||
		second_index < 0 || second_index >= brep->m_F.Count())
	    return 0;
	const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
	const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
	const int parity = 1 ^ static_cast<int>(first_effective) ^
	    static_cast<int>(second_effective);
	graph[static_cast<size_t>(first_index)].push_back(
	    {second_index, parity, ei});
	graph[static_cast<size_t>(second_index)].push_back(
	    {first_index, parity, ei});
    }

    std::vector<int> flip(static_cast<size_t>(brep->m_F.Count()), -1);
    std::vector<int> parent(static_cast<size_t>(brep->m_F.Count()), -1);
    std::vector<int> parent_edge(static_cast<size_t>(brep->m_F.Count()), -1);
    for (int seed = 0; seed < brep->m_F.Count(); ++seed) {
	if (flip[static_cast<size_t>(seed)] >= 0 ||
		graph[static_cast<size_t>(seed)].empty())
	    continue;
	std::vector<int> component;
	std::vector<int> pending(1, seed);
	flip[static_cast<size_t>(seed)] = 0;
	while (!pending.empty()) {
	    const int face = pending.back();
	    pending.pop_back();
	    component.push_back(face);
	    const std::vector<Constraint> &constraints =
		graph[static_cast<size_t>(face)];
	    for (std::vector<Constraint>::const_iterator constraint =
		    constraints.begin(); constraint != constraints.end(); ++constraint) {
		const int required = flip[static_cast<size_t>(face)] ^
		    constraint->parity;
		int &assigned = flip[static_cast<size_t>(constraint->other)];
		if (assigned < 0) {
		    assigned = required;
		    parent[static_cast<size_t>(constraint->other)] = face;
		    parent_edge[static_cast<size_t>(constraint->other)] =
			constraint->edge;
		    pending.push_back(constraint->other);
		} else if (assigned != required) {
		    if (failure) {
			const ON_BrepEdge *edge = brep->Edge(constraint->edge);
			std::ostringstream reason;
			reason << "STEP edge " << (edge ? edge->m_edge_user.i : 0)
			    << " between faces F" << face << " and F"
			    << constraint->other << " requires flip parity "
			    << constraint->parity << ", but their established flips "
			    << flip[static_cast<size_t>(face)] << "/" << assigned
			    << " imply parity "
			    << (flip[static_cast<size_t>(face)] ^ assigned);
			if (parent_edge[static_cast<size_t>(face)] >= 0) {
			    const ON_BrepEdge *incoming = brep->Edge(
				parent_edge[static_cast<size_t>(face)]);
			    reason << "; F" << face << " was reached from F"
				<< parent[static_cast<size_t>(face)] << " through STEP edge "
				<< (incoming ? incoming->m_edge_user.i : 0);
			}
			if (parent_edge[static_cast<size_t>(constraint->other)] >= 0) {
			    const ON_BrepEdge *incoming = brep->Edge(
				parent_edge[static_cast<size_t>(constraint->other)]);
			    reason << "; F" << constraint->other << " was reached from F"
				<< parent[static_cast<size_t>(constraint->other)]
				<< " through STEP edge "
				<< (incoming ? incoming->m_edge_user.i : 0);
			}
			*failure = reason.str();
		    }
		    return 0;
		}
	    }
	}
	size_t flipped = 0;
	for (std::vector<int>::const_iterator face = component.begin();
		face != component.end(); ++face)
	    flipped += flip[static_cast<size_t>(*face)] != 0;
	if (flipped > component.size() - flipped) {
	    for (std::vector<int>::const_iterator face = component.begin();
		    face != component.end(); ++face)
		flip[static_cast<size_t>(*face)] ^= 1;
	}
    }

    size_t repaired = 0;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	if (flip[static_cast<size_t>(fi)] != 1)
	    continue;
	brep->FlipFace(brep->m_F[fi]);
	++repaired;
    }
    return repaired;
}


static size_t
shared_edge_orientation_conflicts(const ON_Brep *brep)
{
    if (!brep)
	return 0;
    size_t conflicts = 0;
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const ON_BrepTrim *first = brep->Trim(edge.m_ti[0]);
	const ON_BrepTrim *second = brep->Trim(edge.m_ti[1]);
	const ON_BrepFace *first_face = first ? first->Face() : NULL;
	const ON_BrepFace *second_face = second ? second->Face() : NULL;
	if (!first || !second || !first_face || !second_face ||
		first_face == second_face)
	    continue;
	const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
	const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
	conflicts += first_effective == second_effective;
    }
    return conflicts;
}


/* A closed STEP edge sometimes has to be split at an OpenNURBS periodic seam.
 * Each resulting edge inherits the original STEP id.  If both complete uses
 * were singleton periodic bounds, an exporter inconsistency between the
 * oriented-edge, face sense, and pcurve winding can make every split member
 * conflict even though all other shell edges are oriented.  A face flip is
 * too broad in that situation.  Reverse one complete split-edge loop instead,
 * but only when a transactional trial remains structurally valid, closed,
 * manifold, and strictly reduces the complete shell's orientation-conflict
 * count.  A periodic band may embed the complete split edge as an independently
 * closed lobe of a larger keyhole loop.  In that case reverse only the
 * contiguous closed source-edge subchain; unrelated exact edge uses must not
 * change.  Reversing the pcurves, order, and edge-use flags together preserves
 * every exact 3-D locus. */
static size_t
repair_split_closed_edge_loop_orientations(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, std::string *failure)
{
    if (failure)
	failure->clear();
    if (!brep)
	return 0;

    bool oriented = false;
    bool has_boundary = false;
    if (!brep->IsManifold(&oriented, &has_boundary) || oriented || has_boundary)
	return 0;

    size_t repaired = 0;

    /* Discover the complete reciprocal loop-parity graph before changing the
     * BREP.  The former fast-looking pre-pass flipped one locally conflicting
     * loop, deep-copied and fully validated the entire BREP, then repeated
     * that transaction for every candidate.  Patterned solids with thousands
     * of mutually compatible loops consequently spent a minute or more in a
     * quadratic single-core pass.
     *
     * The graph below represents the same reciprocal edge-use constraints,
     * chooses one coherent set of loop flips, applies that set serially, and
     * validates the complete BREP once.  This also avoids accepting a local
     * choice which a later neighboring loop would have to undo.
     *
     * A periodic face band can contain two physical boundary loops joined by
     * artificial OpenNURBS seams.  Reversing only one source-edge subchain
     * makes both physical boundaries traverse the same parameter direction
     * and necessarily opens the UV rectangle.  Solve the reciprocal edge-use
     * constraints over complete loops instead.  This can propagate the
     * correction across neighboring bands while FlipLoop preserves every
     * pcurve locus and join.  The complete operation is transactional: accept
     * it only when OpenNURBS confirms structural validity and a closed,
     * oriented manifold shell. */
    std::vector<int> loop_flip(static_cast<size_t>(brep->m_L.Count()), -1);
    std::string loop_graph_failure;
    const bool loop_graph_valid =
	brlcad::step::LoopOrientationFlipPlan(*brep, loop_flip,
	    &loop_graph_failure);

    if (loop_graph_valid) {
	size_t loop_flip_count = 0;
	for (std::vector<int>::const_iterator value = loop_flip.begin();
		value != loop_flip.end(); ++value)
	    loop_flip_count += *value == 1;
	if (loop_flip_count) {
	    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	    std::vector<ON_BrepLoop::TYPE> declared_types;
	    declared_types.reserve(static_cast<size_t>(brep->m_L.Count()));
	    for (int li = 0; li < brep->m_L.Count(); ++li)
		declared_types.push_back(brep->m_L[li].m_type);
	    for (int li = 0; li < brep->m_L.Count(); ++li)
		if (loop_flip[static_cast<size_t>(li)] == 1)
		    brep->FlipLoop(brep->m_L[li]);
	    refresh_brep_flags_preserving_singular_isos(brep, false);
	    for (int li = 0; li < brep->m_L.Count(); ++li)
		brep->m_L[li].m_type = declared_types[static_cast<size_t>(li)];
	    for (int fi = 0; fi < brep->m_F.Count(); ++fi)
		brep->SortFaceLoops(brep->m_F[fi]);

	    ON_wString validation_messages;
	    ON_TextLog validation_log(validation_messages);
	    bool trial_oriented = false;
	    bool trial_boundary = false;
	    const size_t before = shared_edge_orientation_conflicts(rollback.get());
	    const size_t after = shared_edge_orientation_conflicts(brep);
	    const bool structurally_valid = brep->IsValid(&validation_log);
	    const bool trial_manifold =
		brep->IsManifold(&trial_oriented, &trial_boundary);
	    const bool valid = structurally_valid && trial_manifold &&
		trial_oriented && !trial_boundary && after < before;
	    if (valid) {
		if (wrapper)
		    wrapper->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Information,
			entity_id, entity_type, "loop_orientation",
			"resolved compatible split-edge loop orientations in one "
			"parity-graph transaction");
		return repaired + loop_flip_count;
	    }

	    if (failure) {
		ON_String text(validation_messages);
		std::ostringstream reason;
		reason << "complete loop-parity trial changed shell orientation "
		    << "conflicts " << before << "->" << after
		    << " but failed validation (structure="
		    << (structurally_valid ? "yes" : "no")
		    << ", manifold=" << (trial_manifold ? "yes" : "no")
		    << ", oriented=" << (trial_oriented ? "yes" : "no")
		    << ", boundary=" << (trial_boundary ? "yes" : "no")
		    << ')';
		if (text.Array() && text.Array()[0])
		    reason << "; " << text.Array();
		*failure = reason.str();
	    }
	    *brep = *rollback;
	}
    } else if (failure && failure->empty()) {
	*failure = loop_graph_failure.empty() ?
	    "closed-shell loop parity graph was incomplete" : loop_graph_failure;
    }

    std::map<int, std::vector<int> > source_edges;
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const int source_id = brep->m_E[ei].m_edge_user.i;
	if (source_id > 0)
	    source_edges[source_id].push_back(ei);
    }

    for (std::map<int, std::vector<int> >::const_iterator source =
	    source_edges.begin(); source != source_edges.end(); ++source) {
	if (source->second.size() < 2)
	    continue;
	std::set<int> loops;
	std::set<int> faces;
	bool complete_conflicting_pair = true;
	for (std::vector<int>::const_iterator edge_index = source->second.begin();
		edge_index != source->second.end(); ++edge_index) {
	    const ON_BrepEdge &edge = brep->m_E[*edge_index];
	    if (edge.m_ti.Count() != 2) {
		complete_conflicting_pair = false;
		break;
	    }
	    const ON_BrepTrim *first = brep->Trim(edge.m_ti[0]);
	    const ON_BrepTrim *second = brep->Trim(edge.m_ti[1]);
	    const ON_BrepFace *first_face = first ? first->Face() : NULL;
	    const ON_BrepFace *second_face = second ? second->Face() : NULL;
	    if (!first || !second || !first_face || !second_face ||
		    first_face == second_face || first->m_li < 0 || second->m_li < 0 ||
		    (first->m_bRev3d ^ first_face->m_bRev) !=
		    (second->m_bRev3d ^ second_face->m_bRev)) {
		complete_conflicting_pair = false;
		break;
	    }
	    loops.insert(first->m_li);
	    loops.insert(second->m_li);
	    faces.insert(first_face->m_face_index);
	    faces.insert(second_face->m_face_index);
	}
	if (!complete_conflicting_pair || loops.size() != 2 || faces.size() != 2)
	    continue;

	std::map<int, std::vector<int> > loop_subchains;
	for (std::set<int>::const_iterator loop_index = loops.begin();
		loop_index != loops.end(); ++loop_index) {
	    if (*loop_index < 0 || *loop_index >= brep->m_L.Count()) {
		complete_conflicting_pair = false;
		break;
	    }
	    const ON_BrepLoop &loop = brep->m_L[*loop_index];
	    std::vector<unsigned char> selected(
		static_cast<size_t>(loop.TrimCount()), 0);
	    std::set<int> selected_edges;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (!edge || edge->m_edge_user.i != source->first)
		    continue;
		selected[static_cast<size_t>(lti)] = 1;
		selected_edges.insert(edge->m_edge_index);
	    }
	    if (selected_edges.size() != source->second.size()) {
		complete_conflicting_pair = false;
		break;
	    }

	    int chain_start = -1;
	    int chain_starts = 0;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		if (selected[static_cast<size_t>(lti)] &&
			!selected[static_cast<size_t>((lti + loop.TrimCount() - 1) %
			    loop.TrimCount())]) {
		    chain_start = lti;
		    ++chain_starts;
		}
	    }
	    if (selected_edges.size() == static_cast<size_t>(loop.TrimCount())) {
		chain_start = 0;
		chain_starts = 1;
	    }
	    if (chain_starts != 1) {
		complete_conflicting_pair = false;
		break;
	    }

	    std::vector<int> chain_offsets;
	    std::vector<int> chain_trims;
	    for (size_t member = 0; member < source->second.size(); ++member) {
		const int offset = (chain_start + static_cast<int>(member)) %
		    loop.TrimCount();
		if (!selected[static_cast<size_t>(offset)]) {
		    complete_conflicting_pair = false;
		    break;
		}
		chain_offsets.push_back(offset);
		chain_trims.push_back(loop.m_ti[offset]);
	    }
	    if (!complete_conflicting_pair)
		break;

	    /* Reversing an independently closed lobe cannot move either attachment
	     * of the surrounding keyhole loop. */
	    for (size_t member = 0; member < chain_trims.size(); ++member) {
		const ON_BrepTrim *current = brep->Trim(chain_trims[member]);
		const ON_BrepTrim *next = brep->Trim(
		    chain_trims[(member + 1) % chain_trims.size()]);
		if (!current || !next || current->m_vi[1] != next->m_vi[0]) {
		    complete_conflicting_pair = false;
		    break;
		}
	    }
	    if (!complete_conflicting_pair)
		break;
	    loop_subchains[*loop_index] = chain_offsets;
	}
	if (!complete_conflicting_pair)
	    continue;

	const size_t before = shared_edge_orientation_conflicts(brep);
	bool accepted = false;
	for (std::set<int>::const_iterator loop_index = loops.begin();
		loop_index != loops.end() && !accepted; ++loop_index) {
	    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	    const ON_BrepLoop::TYPE declared_type = brep->m_L[*loop_index].m_type;
	    const int face_index = brep->m_L[*loop_index].m_fi;
	    ON_BrepLoop &loop = brep->m_L[*loop_index];
	    const auto report_loop = [wrapper, entity_id, &entity_type,
		    &source](const char *stage, const ON_BrepLoop &candidate) {
		if (!wrapper || !wrapper->Verbose())
		    return;
		const ON_BrepFace *face = candidate.Face();
		const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
		std::cerr << entity_type << " #" << entity_id << ": " << stage
		    << " split STEP edge " << source->first << " loop L"
		    << candidate.m_loop_index << "/STEP" << candidate.m_loop_user.i
		    << " face F" << candidate.m_fi << " trims="
		    << candidate.TrimCount();
		if (surface)
		    std::cerr << " domains=" << surface->Domain(0).Min() << ':'
			<< surface->Domain(0).Max() << ','
			<< surface->Domain(1).Min() << ':'
			<< surface->Domain(1).Max() << " closed="
			<< surface->IsClosed(0) << surface->IsClosed(1);
		std::cerr << std::endl;
		for (int lti = 0; lti < candidate.TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = candidate.Trim(lti);
		    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		    if (!trim)
			continue;
		    const ON_3dPoint start = trim->PointAtStart();
		    const ON_3dPoint end = trim->PointAtEnd();
		    std::cerr << "  [" << lti << "] T" << trim->m_trim_index
			<< "/E" << trim->m_ei << "/STEP"
			<< (edge ? edge->m_edge_user.i : 0) << " v="
			<< trim->m_vi[0] << "->" << trim->m_vi[1]
			<< " r=" << trim->m_bRev3d << " type=" << trim->m_type
			<< " iso=" << trim->m_iso << " uv=" << start.x << ':'
			<< start.y << "->" << end.x << ':' << end.y << std::endl;
		}
	    };
	    report_loop("before", loop);
	    const std::vector<int> &chain_offsets = loop_subchains[*loop_index];
	    std::vector<int> chain_trims;
	    chain_trims.reserve(chain_offsets.size());
	    for (std::vector<int>::const_iterator offset = chain_offsets.begin();
		    offset != chain_offsets.end(); ++offset)
		chain_trims.push_back(loop.m_ti[*offset]);

	    bool reversed = !chain_trims.empty();
	    for (std::vector<int>::const_iterator trim_index = chain_trims.begin();
		    reversed && trim_index != chain_trims.end(); ++trim_index) {
		ON_BrepTrim &trim = brep->m_T[*trim_index];
		ON_Curve *curve = trim.DuplicateCurve();
		if (!curve || !curve->Reverse() || !curve->ChangeDimension(2) ||
			!curve->IsValid()) {
		    delete curve;
		    reversed = false;
		    break;
		}
		const int c2_index = brep->AddTrimCurve(curve);
		if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
		    if (c2_index < 0)
			delete curve;
		    reversed = false;
		    break;
		}
		trim.m_bRev3d = !trim.m_bRev3d;
		std::swap(trim.m_vi[0], trim.m_vi[1]);
	    }
	    if (reversed) {
		for (size_t member = 0; member < chain_offsets.size(); ++member)
		    loop.m_ti[chain_offsets[member]] =
			chain_trims[chain_trims.size() - member - 1];
		std::string unsafe_topology;
		reversed = brep_topology_references_are_safe(brep,
		    &unsafe_topology);
	    }
	    if (!reversed) {
		*brep = *rollback;
		continue;
	    }

	    /* A closed pcurve lobe can traverse a periodic direction.  Reversing
	     * its exact curves exchanges the two UV images at the attachment
	     * vertex; regenerate this one loop from the already detached 3-D edges
	     * so the inverse traversal selects a coherent periodic branch. */
	    regenerate_periodic_loop_chains(brep, wrapper, entity_id, entity_type,
		*loop_index, false);
	    refresh_brep_flags_preserving_singular_isos(brep, false);
	    report_loop("after", brep->m_L[*loop_index]);
	    brep->m_L[*loop_index].m_type = declared_type;
	    if (face_index >= 0 && face_index < brep->m_F.Count())
		brep->SortFaceLoops(brep->m_F[face_index]);

	    ON_wString validation_messages;
	    ON_TextLog validation_log(validation_messages);
	    bool trial_oriented = false;
	    bool trial_boundary = false;
	    const size_t after = shared_edge_orientation_conflicts(brep);
	    const bool valid = brep->IsValid(&validation_log) &&
		brep->IsManifold(&trial_oriented, &trial_boundary) &&
		!trial_boundary && after < before;
	    if (valid) {
		accepted = true;
		++repaired;
	    } else {
		if (failure) {
		    ON_String text(validation_messages);
		    std::ostringstream reason;
		    if (!failure->empty())
			reason << *failure << " | ";
		    reason << "STEP edge " << source->first << " loop L"
			<< *loop_index << " closed-subchain reversal changed shell "
			   "orientation conflicts " << before << "->" << after;
		    if (text.Array() && text.Array()[0])
			reason << "; " << text.Array();
		    *failure = reason.str();
		}
		*brep = *rollback;
	    }
	}
	if (!accepted && failure && failure->empty()) {
	    std::ostringstream reason;
	    reason << "STEP edge " << source->first
		<< " has a complete split-loop conflict, but neither exact closed "
		   "source-edge subchain reversal passed structural and manifold "
		   "validation";
	    *failure = reason.str();
	}
    }
    return repaired;
}


static void
record_detached_brep_job(DetachedBrepJob &job, STEPWrapper *wrapper,
    BRLCADWrapper *dotg, int dry_run, MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    if (job.status == BREP_INVALID_STRUCTURE && wrapper->Verbose() &&
	!job.validation_message.empty())
	std::cerr << job.validation_message;
    if (job.status == BREP_INVALID_STRUCTURE &&
	    job.invalidity != "repair_timeout" &&
	    !job.validation_message.empty())
	wrapper->RecordDiagnostic(job.preserved_invalid ?
		brlcad::step::DiagnosticSeverity::Warning :
		brlcad::step::DiagnosticSeverity::Error,
	    job.entity_id, job.entity_type, "openNURBS",
	    job.validation_message.substr(0, 4096));
    record_brep_result(wrapper, job.status, job.entity_id, job.entity_type,
	job.preserved_invalid, job.invalidity);
    if (job.coverage_entity_id > 0 &&
	    job.coverage_entity_id != job.entity_id) {
	wrapper->RecordRepresentationItemCoverage(job.coverage_entity_id,
	    job.preserved_invalid ?
		brlcad::step::RepresentationCoverageStatus::PreservedInvalid :
	    job.status == BREP_WRITE_SUCCESS ?
		brlcad::step::RepresentationCoverageStatus::Handled :
		brlcad::step::RepresentationCoverageStatus::Skipped,
	    job.preserved_invalid ?
		"a contained shell was preserved as an unresolved invalid import" :
	    job.status == BREP_WRITE_SUCCESS ?
		"one or more contained shells converted successfully" :
		"a contained shell could not be converted");
    }
    if (job.preserved_invalid && job.product_id > 0) {
	std::map<int64_t, brlcad::step::Product>::iterator product =
	    wrapper->Document().products.find(job.product_id);
	if (product != wrapper->Document().products.end())
	    ++product->second.invalid_geometry_count;
    }
    if (job.status != BREP_WRITE_SUCCESS && !job.preserved_invalid)
	return;
    if (job.product_id > 0)
	process_map[job.entity_id] = job.product_id;
    if (dry_run)
	return;
    for (std::vector<DetachedBrepMember>::iterator member = job.members.begin();
	 member != job.members.end(); ++member) {
	dotg->AddMember(member->combination, job.output_name, member->matrix);
	if (member->representation_map_id > 0)
	    dotg->SetAttribute(job.output_name, "step:representation_map_id",
		std::to_string(member->representation_map_id));
    }
}


static bool
write_detached_brep_geometry(DetachedBrepJob &job, BRLCADWrapper *database,
    int dry_run)
{
    if (job.status != BREP_WRITE_SUCCESS && !job.preserved_invalid)
	return false;
    if (dry_run)
	return true;
    const brlcad::step::Style *style = job.has_style ? &job.style : NULL;
    if (job.faceted && !job.preserved_invalid) {
	return database->WriteBot(job.output_name,
	    job.bot_vertices.size() / 3, job.bot_faces.size() / 3,
	    job.bot_vertices.data(), job.bot_faces.data(), job.write_matrix,
	    job.output_source_id, job.original_name, style);
    }
    if (!database->WriteBrep(job.output_name, job.brep.get(),
	    job.write_matrix, job.preserved_invalid ? false : job.is_region,
	    job.output_source_id, job.original_name, style))
	return false;
    const std::string primitive = job.output_name + ".s";
    bool attributes_written = true;
    if (!job.inferred_curves.empty()) {
	std::ostringstream ids;
	std::ostringstream kinds;
	std::ostringstream details;
	for (size_t i = 0; i < job.inferred_curves.size(); ++i) {
	    const brlcad::step::InferredCurve &inference =
		job.inferred_curves[i];
	    if (i) {
		ids << ' ';
		kinds << ';';
		details << ';';
	    }
	    ids << inference.edge_entity_id;
	    kinds << inference.edge_entity_id << '=' << inference.kind;
	    details << '#' << inference.edge_entity_id << ' '
		<< inference.kind << " discrepancy_mm="
		<< inference.discrepancy_mm << " safe_limit_mm="
		<< inference.safe_limit_mm << " inference_limit_mm="
		<< inference.inference_limit_mm << " declared_tolerance_mm="
		<< inference.declared_tolerance_mm;
	    if (!inference.detail.empty())
		details << " (" << inference.detail << ')';
	}
	attributes_written =
	    database->SetAttribute(primitive, "step:geometry_status", "inferred") &&
	    database->SetAttribute(job.output_name, "step:geometry_status", "inferred") &&
	    database->SetAttribute(primitive, "step:inferred_curve_ids", ids.str()) &&
	    database->SetAttribute(job.output_name, "step:inferred_curve_ids", ids.str()) &&
	    database->SetAttribute(primitive, "step:inferred_curve_kinds", kinds.str()) &&
	    database->SetAttribute(job.output_name, "step:inferred_curve_kinds", kinds.str()) &&
	    database->SetAttribute(primitive, "step:inferred_curve_details", details.str()) &&
	    database->SetAttribute(job.output_name, "step:inferred_curve_details", details.str());
    }
    if (!job.preserved_invalid)
	return attributes_written;
    return attributes_written &&
	database->SetAttribute(primitive, "step:geometry_status",
	    "invalid_preserved") &&
	database->SetAttribute(job.output_name, "step:geometry_status",
	    "invalid_preserved") &&
	database->SetAttribute(primitive, "step:import_status", "invalid") &&
	database->SetAttribute(job.output_name, "step:import_status", "invalid") &&
	database->SetAttribute(primitive, "step:source_validity", "unresolved") &&
	database->SetAttribute(job.output_name, "step:source_validity",
	    "unresolved") &&
	database->SetAttribute(primitive, "step:invalidity", job.invalidity) &&
	database->SetAttribute(job.output_name, "step:invalidity", job.invalidity) &&
	database->SetAttribute(primitive, "step:invalid_reason", job.invalid_reason) &&
	database->SetAttribute(job.output_name, "step:invalid_reason", job.invalid_reason);
}


static void
commit_spooled_brep_job(DetachedBrepJob &job, STEPWrapper *wrapper,
    BRLCADWrapper *spool, BRLCADWrapper *dotg, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    if ((job.status == BREP_WRITE_SUCCESS || job.preserved_invalid) && !dry_run) {
	const bool solid_copied = dotg->CopyObjectFrom(*spool,
	    job.output_name + ".s");
	const bool region_copied = solid_copied &&
	    dotg->CopyObjectFrom(*spool, job.output_name);
	if (!region_copied) {
	    job.status = BREP_OUTPUT_FAILED;
	    job.preserved_invalid = false;
	}
    }
    if ((job.status == BREP_WRITE_SUCCESS || job.preserved_invalid) && job.has_style)
	++wrapper->Statistics().styles_applied;
    record_detached_brep_job(job, wrapper, dotg, dry_run, process_map);
}


static void
write_detached_brep_jobs(std::vector<std::unique_ptr<DetachedBrepJob> > &jobs,
    STEPWrapper *wrapper, BRLCADWrapper *dotg, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    std::stable_sort(jobs.begin(), jobs.end(),
	[](const std::unique_ptr<DetachedBrepJob> &left,
	   const std::unique_ptr<DetachedBrepJob> &right) {
	    return left->entity_id < right->entity_id;
	});
    wrapper->SetProgress("converting exact geometry", 0,
	static_cast<uint64_t>(jobs.size()), jobs.empty() ? 0 : jobs.front()->entity_id,
	wrapper->Statistics().geometry_written, "written");
    const uint64_t geometry_base = wrapper->Statistics().geometry_attempted;
    wrapper->SetGeometryOverallProgress(geometry_base,
	geometry_base + static_cast<uint64_t>(jobs.size()));

    const unsigned int concurrency = std::max(1U,
	wrapper->ImportOptions().effective_jobs);
    const unsigned int worker_count = std::max(1U, std::min(
	concurrency,
	static_cast<unsigned int>(jobs.size())));

    TemporaryGeometrySpool spool_file;
    BRLCADWrapper spool;
    spool.dry_run = dry_run;
    if (!dry_run && (!spool_file.CreatePath() ||
	    !spool.OpenFile(spool_file.path))) {
	++wrapper->Statistics().output_failures;
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0,
	    "IMPORT_SESSION", "geometry_spool",
	    "could not create the temporary BRL-CAD geometry spool");
	return;
    }

    /* STEPcode materialization remains single-threaded.  Once a job owns a
     * detached entity arena, feed it to persistent geometry workers.  A
     * bounded look-ahead window prevents a slow low-ID item from starving all
     * workers while results are retired strictly in sorted STEP-id order. */
    std::mutex queue_mutex;
    std::condition_variable queue_changed;
    std::deque<size_t> pending;
    std::deque<size_t> ready_results;
    std::vector<unsigned char> spooled(jobs.size(), 0);
    std::vector<uint64_t> result_bytes(jobs.size(), 0);
    size_t active_workers = 0;
    size_t completed_waiting = 0;
    size_t spooled_waiting = 0;
    size_t finished_jobs = 0;
    uint64_t ready_bytes = 0;
    bool materializing = false;
    bool stop_workers = false;
    const size_t runnable_capacity = std::min(jobs.size(),
	static_cast<size_t>(worker_count) * kRunnableGeometryJobsPerWorker);
    const auto publish_scheduler_locked = [&]() {
	const size_t observed_in_flight = pending.size() + active_workers +
	    completed_waiting + spooled_waiting + (materializing ? 1 : 0);
	wrapper->SetGeometrySchedulerProgress(
	    static_cast<uint64_t>(pending.size()),
	    static_cast<uint64_t>(active_workers),
	    static_cast<uint64_t>(completed_waiting),
	    static_cast<uint64_t>(spooled_waiting),
	    static_cast<uint64_t>(finished_jobs), materializing ? 1 : 0,
	    static_cast<uint64_t>(observed_in_flight),
	    static_cast<uint64_t>(runnable_capacity), ready_bytes,
	    kMaximumReadyGeometryBytes);
	wrapper->SetGeometryOverallProgress(
	    geometry_base + static_cast<uint64_t>(finished_jobs),
	    geometry_base + static_cast<uint64_t>(jobs.size()));
    };
    const auto worker = [&]() {
	for (;;) {
	    size_t index = jobs.size();
	    {
		std::unique_lock<std::mutex> lock(queue_mutex);
		queue_changed.wait(lock, [&]() {
		    return stop_workers || !pending.empty();
		});
		if (stop_workers)
		    return;
		index = pending.front();
		pending.pop_front();
		++active_workers;
		publish_scheduler_locked();
	    }
	    const bool exclusive_for_memory = jobs.size() > 1 &&
		jobs[index]->topology_face_count >=
		    kExclusivePullbackTopologyFaceThreshold;
	    const bool exclusive_pullback = exclusive_for_memory ||
		jobs[index]->maximum_pullback_span_estimate >=
		    kExclusivePullbackSurfaceSpanThreshold;
	    wrapper->GeometryWorkerStarted(jobs[index]->entity_id,
		jobs[index]->entity_type, exclusive_pullback);
	    if (exclusive_pullback) {
		std::ostringstream reason;
		reason << "reserved nested geometry workers for a complex item";
		if (jobs[index]->topology_face_count >=
			kExclusivePullbackTopologyFaceThreshold)
		    reason << " with " << jobs[index]->topology_face_count
			<< " topology faces";
		if (jobs[index]->maximum_pullback_span_estimate >=
			kExclusivePullbackSurfaceSpanThreshold)
		    reason << (jobs[index]->topology_face_count >=
			    kExclusivePullbackTopologyFaceThreshold ?
			    " and a surface with an estimated " :
			    " with a surface having an estimated ")
			<< jobs[index]->maximum_pullback_span_estimate
			<< " tensor-product spans";
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Information,
		    jobs[index]->entity_id, jobs[index]->entity_type,
		    "scheduler", reason.str());
	    }
	    try {
		std::shared_ptr<STEPWrapper::CurveInferenceTransaction>
		    ordinary_transaction(
			new STEPWrapper::CurveInferenceTransaction());
		{
		    STEPWrapper::CurveInferenceScope transaction_scope(wrapper,
			ordinary_transaction, false);
		    convert_detached_brep_job(*jobs[index], wrapper);
		    classify_invalid_brep_candidate(*jobs[index], wrapper);
		}
		if (jobs[index]->accepted_inference_transaction)
		    wrapper->CommitCurveInferenceTransaction(
			jobs[index]->accepted_inference_transaction);
		else
		    wrapper->CommitCurveInferenceTransaction(
			ordinary_transaction);
	if (jobs[index]->brep) {
	    if (jobs[index]->accepted_inference_transaction) {
		/* The transaction belongs to this detached BREP.  Preserve all of
		 * its accepted provenance, including a source edge which a
		 * topology inference deliberately removed and which therefore no
		 * longer appears in the finished BREP edge array. */
		std::lock_guard<std::mutex> inference_guard(
		    jobs[index]->accepted_inference_transaction->mutex);
		jobs[index]->inferred_curves.clear();
		for (std::map<std::string,
			brlcad::step::InferredCurve>::const_iterator inference =
			jobs[index]->accepted_inference_transaction->curves.begin();
			inference != jobs[index]->accepted_inference_transaction->
			    curves.end(); ++inference)
		    jobs[index]->inferred_curves.push_back(inference->second);
	    } else {
		std::set<int64_t> edge_ids;
		for (int edge_index = 0;
			edge_index < jobs[index]->brep->m_E.Count();
			++edge_index) {
		    const int64_t edge_id =
			jobs[index]->brep->m_E[edge_index].m_edge_user.i;
		    if (edge_id > 0)
			edge_ids.insert(edge_id);
		}
		jobs[index]->inferred_curves =
		    wrapper->InferredCurvesForEdges(edge_ids);
	    }
	}
	    } catch (...) {
		jobs[index]->status = BREP_CONVERSION_FAILED;
		jobs[index]->validation_message =
		    "exception while validating detached OpenNURBS geometry";
	    }
	    wrapper->GeometryWorkerFinished(exclusive_pullback);
	    /* Conversion has copied all accepted curves, surfaces, topology, and
	     * metadata into the job result.  Do not retain a STEP dependency arena
	     * merely because a lower-ID result has not reached the database yet. */
	    jobs[index]->solid = NULL;
	    jobs[index]->surface_model = NULL;
	    jobs[index]->face_set = NULL;
	    jobs[index]->entity_arena.reset();
	    uint64_t bytes = sizeof(DetachedBrepJob);
	    if (jobs[index]->brep)
		bytes += static_cast<uint64_t>(jobs[index]->brep->SizeOf());
	    bytes += static_cast<uint64_t>(jobs[index]->bot_vertices.capacity()) *
		sizeof(fastf_t);
	    bytes += static_cast<uint64_t>(jobs[index]->bot_faces.capacity()) *
		sizeof(int);
	    {
		std::lock_guard<std::mutex> lock(queue_mutex);
		--active_workers;
		ready_results.push_back(index);
		result_bytes[index] = bytes;
		ready_bytes += bytes;
		++completed_waiting;
		++finished_jobs;
		publish_scheduler_locked();
	    }
	    queue_changed.notify_all();
	}
    };

    std::vector<std::thread> workers;
    wrapper->ConfigureGeometryExecutor(concurrency);
    workers.reserve(worker_count);
    for (unsigned int worker_index = 0; worker_index < worker_count;
	    ++worker_index)
	workers.emplace_back(worker);

    size_t next_materialization = 0;
    size_t next_commit = 0;
    {
	std::lock_guard<std::mutex> lock(queue_mutex);
	publish_scheduler_locked();
    }
    while (next_commit < jobs.size() && !wrapper->CancellationRequested()) {
	size_t ready_index = jobs.size();
	{
	    std::lock_guard<std::mutex> lock(queue_mutex);
	    if (!ready_results.empty()) {
		ready_index = ready_results.front();
		ready_results.pop_front();
	    }
	}
	if (ready_index < jobs.size()) {
	    DetachedBrepJob &job = *jobs[ready_index];
	    const std::chrono::steady_clock::time_point spool_started =
		std::chrono::steady_clock::now();
	    if ((job.status == BREP_WRITE_SUCCESS || job.preserved_invalid) &&
		!write_detached_brep_geometry(job, &spool, dry_run))
	    {
		job.status = BREP_OUTPUT_FAILED;
		job.preserved_invalid = false;
	    }
	    wrapper->RecordStageTiming("spool_write", job.entity_id,
		job.entity_type, static_cast<uint64_t>(
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - spool_started).count()),
		job.brep ? static_cast<uint64_t>(job.brep->m_F.Count()) : 0,
		job.brep ? static_cast<uint64_t>(job.brep->m_E.Count()) : 0,
		job.brep ? static_cast<uint64_t>(job.brep->m_T.Count()) : 0);
	    job.brep.reset();
	    job.bot_vertices.clear();
	    job.bot_vertices.shrink_to_fit();
	    job.bot_faces.clear();
	    job.bot_faces.shrink_to_fit();
	    {
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (ready_bytes >= result_bytes[ready_index])
		    ready_bytes -= result_bytes[ready_index];
		else
		    ready_bytes = 0;
		result_bytes[ready_index] = 0;
		if (completed_waiting)
		    --completed_waiting;
		spooled[ready_index] = 1;
		++spooled_waiting;
		publish_scheduler_locked();
	    }
	    queue_changed.notify_all();
	    continue;
	}

	bool head_spooled = false;
	{
	    std::lock_guard<std::mutex> lock(queue_mutex);
	    head_spooled = spooled[next_commit] != 0;
	}
	if (head_spooled) {
	    const std::chrono::steady_clock::time_point commit_started =
		std::chrono::steady_clock::now();
	    commit_spooled_brep_job(*jobs[next_commit], wrapper, &spool, dotg,
		dry_run, process_map);
	    wrapper->RecordStageTiming("database_commit",
		jobs[next_commit]->entity_id, jobs[next_commit]->entity_type,
		static_cast<uint64_t>(
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - commit_started).count()),
		static_cast<uint64_t>(jobs[next_commit]->topology_face_count));
	    {
		std::lock_guard<std::mutex> lock(queue_mutex);
		if (spooled_waiting)
		    --spooled_waiting;
		publish_scheduler_locked();
	    }
	    ++next_commit;
	    wrapper->SetProgress("converting exact geometry",
		static_cast<uint64_t>(next_commit),
		static_cast<uint64_t>(jobs.size()), 0,
		wrapper->Statistics().geometry_written, "written");
	    continue;
	}

	bool can_materialize = false;
	{
	    std::lock_guard<std::mutex> lock(queue_mutex);
	    const size_t runnable = pending.size() + active_workers +
		(materializing ? 1 : 0);
	    can_materialize = next_materialization < jobs.size() &&
		runnable < runnable_capacity &&
		ready_bytes < kMaximumReadyGeometryBytes;
	    if (can_materialize) {
		materializing = true;
		publish_scheduler_locked();
	    }
	}
	if (can_materialize) {
	    DetachedBrepJob &job = *jobs[next_materialization];
	    wrapper->SetProgress("converting exact geometry",
		static_cast<uint64_t>(next_commit),
		static_cast<uint64_t>(jobs.size()), job.entity_id,
		wrapper->Statistics().geometry_written, "written",
		"scheduling=" + std::to_string(next_materialization + 1));
	    materialize_detached_brep_job(job, wrapper);
	    {
		std::lock_guard<std::mutex> lock(queue_mutex);
		materializing = false;
		pending.push_back(next_materialization);
		publish_scheduler_locked();
	    }
	    ++next_materialization;
	    queue_changed.notify_one();
	    continue;
	}

	std::unique_lock<std::mutex> lock(queue_mutex);
	queue_changed.wait(lock, [&]() {
	    if (wrapper->CancellationRequested() || !ready_results.empty() ||
		spooled[next_commit] != 0)
		return true;
	    const size_t runnable = pending.size() + active_workers +
		(materializing ? 1 : 0);
	    return next_materialization < jobs.size() &&
		runnable < runnable_capacity &&
		ready_bytes < kMaximumReadyGeometryBytes;
	});
    }

    {
	std::lock_guard<std::mutex> lock(queue_mutex);
	stop_workers = true;
    }
    queue_changed.notify_all();
    for (std::thread &thread : workers)
	thread.join();
    wrapper->StopGeometryExecutor();
    spool.Close();
    wrapper->SetGeometrySchedulerProgress(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    wrapper->SetGeometryOverallProgress(wrapper->Statistics().geometry_attempted,
	wrapper->Statistics().geometry_attempted);
}


#ifdef HAVE_STEPCODE_LAZY
struct LazyRepresentationUnits {
    double length = 1000.0;
    double planeangle = 1.0;
    double solidangle = 1.0;
    double tolerance = 1.0e-6;
    bool has_source_tolerance = false;
    double source_tolerance = 0.0;
};


static LazyRepresentationUnits
lazy_representation_units(STEPWrapper *wrapper, const LazySTEPExactGraph &graph,
    uint64_t representation_id)
{
    LazyRepresentationUnits result;
    result.tolerance = LocalUnits::tolerance;
    std::map<uint64_t, uint64_t>::const_iterator context =
	graph.representation_context.find(representation_id);
    if (!wrapper || context == graph.representation_context.end() || !context->second ||
	context->second > static_cast<uint64_t>(INT_MAX))
	return result;

    wrapper->ReleaseSourceData();
    SDAI_Application_instance *source = wrapper->getEntity(static_cast<int>(context->second));
    SDAI_Application_instance *component = source ?
	wrapper->getEntity(source, "Global_Unit_Assigned_Context") : NULL;
    if (component) {
	GlobalUnitAssignedContext units;
	if (units.Load(wrapper, component)) {
	    result.length = units.GetLengthConversionFactor();
	    result.planeangle = units.GetPlaneAngleConversionFactor();
	    result.solidangle = units.GetSolidAngleConversionFactor();
	    if (wrapper->Verbose())
		std::cerr << "REPRESENTATION #" << representation_id
		    << ": unit factors length=" << result.length
		    << " mm, plane-angle=" << result.planeangle
		    << " rad, solid-angle=" << result.solidangle << std::endl;
	}
    }
    component = source ?
	wrapper->getEntity(source, "Global_Uncertainty_Assigned_Context") : NULL;
    if (component) {
	GlobalUncertaintyAssignedContext uncertainty;
	if (uncertainty.Load(wrapper, component)) {
	    const double value = uncertainty.GetLengthUncertainty();
	    if (value > 0.0) {
		result.has_source_tolerance = true;
		result.source_tolerance = value;
		if (wrapper->ImportOptions().absolute_tolerance_mm <= 0.0)
		    result.tolerance = std::max(1.0e-9, value);
	    }
	    if (wrapper->Verbose() && result.has_source_tolerance)
		std::cerr << "REPRESENTATION #" << representation_id
		    << ": asserted uncertainty=" << result.source_tolerance
		    << " mm" << std::endl;
        }
    }
    wrapper->ReleaseSourceData();
    return result;
}


static void
retain_lazy_representation_contexts(const LazySTEPExactGraph &graph,
    STEPWrapper *wrapper)
{
    if (!wrapper) return;
    std::map<uint64_t, LazyRepresentationUnits> context_units;
    for (std::map<uint64_t, uint64_t>::const_iterator mapping =
	    graph.representation_product.begin();
	 mapping != graph.representation_product.end(); ++mapping) {
	if (!mapping->first || mapping->first > static_cast<uint64_t>(INT64_MAX) ||
		!mapping->second || mapping->second > static_cast<uint64_t>(INT64_MAX))
	    continue;
	std::map<uint64_t, uint64_t>::const_iterator context =
	    graph.representation_context.find(mapping->first);
	if (context == graph.representation_context.end() || !context->second)
	    continue;
	std::map<uint64_t, LazyRepresentationUnits>::iterator cached =
	    context_units.find(context->second);
	if (cached == context_units.end()) {
	    const LazyRepresentationUnits units = lazy_representation_units(wrapper,
		graph, mapping->first);
	    cached = context_units.insert(std::make_pair(context->second, units)).first;
	}
	const LazyRepresentationUnits &units = cached->second;
	brlcad::step::RepresentationContextProvenance &record =
	    wrapper->Document().representation_contexts[
		static_cast<int64_t>(mapping->first)];
	record.representation_id = static_cast<int64_t>(mapping->first);
	record.product_id = static_cast<int64_t>(mapping->second);
	record.length_unit_mm = units.length;
	record.plane_angle_unit_radians = units.planeangle;
	record.solid_angle_unit_steradians = units.solidangle;
	record.has_uncertainty = units.has_source_tolerance;
	record.uncertainty_mm = units.source_tolerance;
    }
}


static bool
lazy_representation_matrix(STEPWrapper *wrapper, uint64_t representation_id,
    double length_factor, mat_t matrix)
{
    MAT_IDN(matrix);
    if (!wrapper) return false;
    const std::vector<uint64_t> references = wrapper->LazyForwardReferences(representation_id);
    uint64_t axis_id = 0;
    for (std::vector<uint64_t>::const_iterator reference = references.begin();
	 reference != references.end(); ++reference) {
	if (wrapper->LazyTypeName(*reference) == "AXIS2_PLACEMENT_3D") {
	    axis_id = *reference;
	    break;
	}
    }
    if (!axis_id) return true;
    if (axis_id > static_cast<uint64_t>(INT_MAX)) return false;

    wrapper->ReleaseSourceData();
    SDAI_Application_instance *source = wrapper->getEntity(static_cast<int>(axis_id));
    Axis2Placement3D *axis = source ? dynamic_cast<Axis2Placement3D *>(
	Factory::CreateObject(wrapper, source)) : NULL;
    if (axis) axis_representation_matrix(axis, length_factor, matrix);
    const bool valid = axis != NULL;
    wrapper->ReleaseSourceData();
    return valid;
}


static bool
lazy_occurrence_matrix(STEPWrapper *wrapper, const LazySTEPOccurrence &occurrence,
    double length_factor, mat_t matrix)
{
    MAT_IDN(matrix);
    if (!wrapper || occurrence.parent_axis_id > static_cast<uint64_t>(INT_MAX) ||
	occurrence.child_axis_id > static_cast<uint64_t>(INT_MAX))
	return false;

    wrapper->ReleaseSourceData();
    SDAI_Application_instance *parent_source = wrapper->getEntity(
	static_cast<int>(occurrence.parent_axis_id));
    SDAI_Application_instance *child_source = wrapper->getEntity(
	static_cast<int>(occurrence.child_axis_id));
    Axis2Placement3D *parent = parent_source ? dynamic_cast<Axis2Placement3D *>(
	Factory::CreateObject(wrapper, parent_source)) : NULL;
    Axis2Placement3D *child = child_source ? dynamic_cast<Axis2Placement3D *>(
	Factory::CreateObject(wrapper, child_source)) : NULL;
    if (!parent || !child) {
	wrapper->ReleaseSourceData();
	return false;
    }

    mat_t parent_rotation;
    mat_t parent_inverse;
    mat_t child_frame;
    vect_t parent_translation;
    vect_t child_translation;
    MAT_IDN(parent_rotation);
    VMOVE(&parent_rotation[0], parent->GetXAxis());
    VMOVE(&parent_rotation[4], parent->GetYAxis());
    VMOVE(&parent_rotation[8], parent->GetZAxis());
    bn_mat_inv(parent_inverse, parent_rotation);
    VMOVE(parent_translation, parent->GetOrigin());
    VSCALE(parent_translation, parent_translation, length_factor);
    MAT_DELTAS_VEC(parent_inverse, parent_translation);

    MAT_IDN(child_frame);
    VMOVE(&child_frame[0], child->GetXAxis());
    VMOVE(&child_frame[4], child->GetYAxis());
    VMOVE(&child_frame[8], child->GetZAxis());
    VMOVE(child_translation, child->GetOrigin());
    VSCALE(child_translation, child_translation, -length_factor);
    MAT_DELTAS_VEC(child_frame, child_translation);
    bn_mat_mul(matrix, parent_inverse, child_frame);
    wrapper->ReleaseSourceData();
    return true;
}


static bool
lazy_product_representation_matrix(const LazySTEPExactGraph &graph,
    STEPWrapper *wrapper, uint64_t representation_id, double length_factor,
    mat_t matrix)
{
    mat_t representation_matrix;
    if (!lazy_representation_matrix(wrapper, representation_id, length_factor,
	    representation_matrix))
	return false;

    std::map<uint64_t, LazySTEPRepresentationTransform>::const_iterator source =
	graph.representation_product_transforms.find(representation_id);
    if (source == graph.representation_product_transforms.end()) {
	MAT_COPY(matrix, representation_matrix);
	return true;
    }

    LazySTEPOccurrence placement;
    placement.parent_axis_id = source->second.parent_axis_id;
    placement.child_axis_id = source->second.child_axis_id;
    mat_t relationship_matrix;
    if (!lazy_occurrence_matrix(wrapper, placement, length_factor,
	    relationship_matrix))
	return false;
    bn_mat_mul(matrix, relationship_matrix, representation_matrix);
    return true;
}


static bool
lazy_mapped_occurrence_matrix(STEPWrapper *wrapper,
    const LazySTEPOccurrence &occurrence, double length_factor, mat_t matrix)
{
    MAT_IDN(matrix);
    if (!wrapper || occurrence.parent_axis_id > static_cast<uint64_t>(INT_MAX) ||
	occurrence.child_axis_id > static_cast<uint64_t>(INT_MAX))
	return false;
    wrapper->ReleaseSourceData();
    SDAI_Application_instance *target_source = wrapper->getEntity(
	static_cast<int>(occurrence.parent_axis_id));
    SDAI_Application_instance *origin_source = wrapper->getEntity(
	static_cast<int>(occurrence.child_axis_id));
    Axis2Placement3D *target = target_source ? dynamic_cast<Axis2Placement3D *>(
	Factory::CreateObject(wrapper, target_source)) : NULL;
    Axis2Placement3D *origin = origin_source ? dynamic_cast<Axis2Placement3D *>(
	Factory::CreateObject(wrapper, origin_source)) : NULL;
    if (!target || !origin) {
	wrapper->ReleaseSourceData();
	return false;
    }
    mat_t target_frame;
    mat_t origin_frame;
    mat_t origin_inverse;
    placement_frame(target, length_factor, target_frame);
    placement_frame(origin, length_factor, origin_frame);
    bn_mat_inv(origin_inverse, origin_frame);
    bn_mat_mul(matrix, target_frame, origin_inverse);
    wrapper->ReleaseSourceData();
    return true;
}


static void
index_lazy_exact_graph(const LazySTEPExactGraph &graph, STEPWrapper *wrapper,
    BRLCADWrapper *database, MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &id2productid_map,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &shell2representation_map)
{
    if (!wrapper || !database) return;
    for (std::map<uint64_t, uint64_t>::const_iterator mapping =
	 graph.representation_product.begin();
	 mapping != graph.representation_product.end(); ++mapping) {
	if (mapping->first > static_cast<uint64_t>(INT_MAX) ||
	    mapping->second > static_cast<uint64_t>(INT_MAX)) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		static_cast<int64_t>(mapping->first), wrapper->LazyTypeName(mapping->first),
		std::string(), "entity identifier exceeds the current conversion-map range");
	    continue;
	}
	const int representation_id = static_cast<int>(mapping->first);
	const int product_id = static_cast<int>(mapping->second);
	brlcad::step::Product &product = wrapper->Document().products[product_id];
	product.entity_id = product_id;
	std::string product_label = product.original_name;
	if (product_label.empty()) product_label = product.identifier;
	if (product.output_name.empty())
	    product.output_name = database->StableBRLCADName(product_label.empty() ?
		std::string("Product_step") + std::to_string(product_id) : product_label,
		product_id);
	id2name_map[product_id] = product.output_name;
	id2name_map[representation_id] = product.output_name;
	id2productid_map[representation_id] = product_id;
	brlcad::step::Representation &representation =
	    wrapper->Document().representations[representation_id];
	representation.entity_id = representation_id;
	representation.product_id = product_id;
	representation.type = wrapper->LazyTypeName(mapping->first);
	representation.output_name = product.output_name;

	brlcad::step::RepresentationCoverage &coverage =
	    wrapper->Document().representation_coverage[representation_id];
	coverage.entity_id = representation_id;
	coverage.product_id = product_id;
	coverage.type = representation.type;
	std::map<uint64_t, std::vector<uint64_t> >::const_iterator invalid =
	    graph.representation_invalid_references.find(mapping->first);
	if (invalid != graph.representation_invalid_references.end() &&
		!invalid->second.empty()) {
	    coverage.status = brlcad::step::RepresentationCoverageStatus::Malformed;
	    coverage.reason = "representation has an item/context reference which "
		"does not resolve to the schema-declared attribute type";
	} else if (graph.representation_context.find(mapping->first) ==
		graph.representation_context.end()) {
	    coverage.status = brlcad::step::RepresentationCoverageStatus::Malformed;
	    coverage.reason = "representation has no resolvable context_of_items";
	}
	std::map<uint64_t, std::vector<uint64_t> >::const_iterator items =
	    graph.representation_items.find(mapping->first);
	if (items != graph.representation_items.end()) {
	    std::set<uint64_t> recorded;
	    for (std::vector<uint64_t>::const_iterator source = items->second.begin();
		 source != items->second.end(); ++source) {
		if (!recorded.insert(*source).second ||
			*source > static_cast<uint64_t>(INT64_MAX)) continue;
		brlcad::step::RepresentationItemCoverage item;
		item.entity_id = static_cast<int64_t>(*source);
		item.type = wrapper->LazyTypeName(*source);
		if (item.type.empty()) item.type = "UNRESOLVED_REFERENCE";
		if (graph.property_representations.find(mapping->first) !=
			graph.property_representations.end() ||
			wrapper->LazyIsSchemaEntity(*source, "PLACEMENT") ||
			wrapper->LazyIsSchemaEntity(*source,
			    "DESCRIPTIVE_REPRESENTATION_ITEM") ||
			wrapper->LazyIsSchemaEntity(*source,
			    "MEASURE_REPRESENTATION_ITEM") ||
			wrapper->LazyIsSchemaEntity(*source,
			    "VALUE_REPRESENTATION_ITEM") ||
			wrapper->LazyIsSchemaEntity(*source, "STYLED_ITEM") ||
			wrapper->LazyIsSchemaEntity(*source, "TEXT_LITERAL") ||
			wrapper->LazyIsSchemaEntity(*source,
			    "ANNOTATION_OCCURRENCE") ||
			wrapper->LazyIsSchemaEntity(*source, "DRAUGHTING_CALLOUT")) {
		    item.status = brlcad::step::RepresentationCoverageStatus::
			IntentionallyNonGeometric;
		    item.reason = wrapper->LazyIsSchemaEntity(*source, "PLACEMENT") &&
			graph.property_representations.find(mapping->first) ==
			graph.property_representations.end() ?
			"representation placement establishes a frame, not a geometry object" :
			"property or presentation item is intentionally not imported as geometry";
		}
		coverage.items.push_back(item);
	    }
	}
    }

    for (std::map<uint64_t, std::vector<uint64_t> >::const_iterator represented =
	 graph.representation_solids.begin();
	 represented != graph.representation_solids.end(); ++represented) {
	if (graph.handled_representations.find(represented->first) ==
	    graph.handled_representations.end()) continue;
	std::map<uint64_t, uint64_t>::const_iterator product_mapping =
	    graph.representation_product.find(represented->first);
	if (product_mapping == graph.representation_product.end() ||
	    product_mapping->second > static_cast<uint64_t>(INT_MAX)) continue;
	const int product_id = static_cast<int>(product_mapping->second);
	for (std::vector<uint64_t>::const_iterator solid = represented->second.begin();
	     solid != represented->second.end(); ++solid) {
	    if (*solid > static_cast<uint64_t>(INT_MAX)) continue;
	    const int solid_id = static_cast<int>(*solid);
	    if (id2name_map[solid_id].empty())
		id2name_map[solid_id] = database->StableBRLCADName(
		    id2name_map[product_id] + "_item", solid_id);
	    id2productid_map[solid_id] = product_id;
	    brlcad::step::Representation &item = wrapper->Document().representations[solid_id];
	    item.entity_id = solid_id;
	    item.product_id = product_id;
	    item.type = wrapper->LazyTypeName(*solid);
	    item.output_name = id2name_map[solid_id];
	}
    }

    for (std::map<uint64_t, std::vector<uint64_t> >::const_iterator represented =
	 graph.representation_surface_models.begin();
	 represented != graph.representation_surface_models.end(); ++represented) {
	if (graph.handled_representations.find(represented->first) ==
	    graph.handled_representations.end()) continue;
	std::map<uint64_t, uint64_t>::const_iterator product_mapping =
	    graph.representation_product.find(represented->first);
	if (product_mapping == graph.representation_product.end() ||
	    product_mapping->second > static_cast<uint64_t>(INT_MAX) ||
	    represented->first > static_cast<uint64_t>(INT_MAX)) continue;
	const int product_id = static_cast<int>(product_mapping->second);
	const int representation_id = static_cast<int>(represented->first);
	for (std::vector<uint64_t>::const_iterator model = represented->second.begin();
	     model != represented->second.end(); ++model) {
	    if (*model > static_cast<uint64_t>(INT_MAX)) continue;
	    const int model_id = static_cast<int>(*model);
	    if (id2name_map[model_id].empty())
		id2name_map[model_id] = database->StableBRLCADName(
		    id2name_map[product_id] + "_item", model_id);
	    id2productid_map[model_id] = product_id;
	    shell2representation_map[model_id] = representation_id;
	    brlcad::step::Representation &item = wrapper->Document().representations[model_id];
	    item.entity_id = model_id;
	    item.product_id = product_id;
	    item.type = "SHELL_BASED_SURFACE_MODEL";
	    item.output_name = id2name_map[model_id];
	}
    }

    /* Geometric sets still materialize only when their existing bounded-
     * surface/wire adapter converts them.  Their names and ownership do not
     * require loading the set's complete element closure. */
    for (std::map<uint64_t, std::vector<uint64_t> >::const_iterator represented =
	 graph.representation_geometric_sets.begin();
	 represented != graph.representation_geometric_sets.end(); ++represented) {
	std::map<uint64_t, uint64_t>::const_iterator product_mapping =
	    graph.representation_product.find(represented->first);
	const int product_id = product_mapping != graph.representation_product.end() &&
	    product_mapping->second <= static_cast<uint64_t>(INT_MAX) ?
	    static_cast<int>(product_mapping->second) : 0;
	for (std::vector<uint64_t>::const_iterator set = represented->second.begin();
	     set != represented->second.end(); ++set) {
	    if (*set > static_cast<uint64_t>(INT_MAX)) continue;
	    const int set_id = static_cast<int>(*set);
	    if (id2name_map[set_id].empty())
		id2name_map[set_id] = database->StableBRLCADName(product_id > 0 ?
		    id2name_map[product_id] + "_item" : "GeometricSet", set_id);
	    if (product_id > 0) id2productid_map[set_id] = product_id;
	    brlcad::step::Representation &item = wrapper->Document().representations[set_id];
	    item.entity_id = set_id;
	    item.product_id = product_id;
	    item.type = "GEOMETRIC_SET";
	    item.output_name = id2name_map[set_id];
	}
    }

    for (std::map<uint64_t, std::vector<uint64_t> >::const_iterator represented =
	 graph.representation_wireframe_models.begin();
	 represented != graph.representation_wireframe_models.end(); ++represented) {
	std::map<uint64_t, uint64_t>::const_iterator product_mapping =
	    graph.representation_product.find(represented->first);
	if (product_mapping == graph.representation_product.end() ||
		product_mapping->second > static_cast<uint64_t>(INT_MAX)) continue;
	const int product_id = static_cast<int>(product_mapping->second);
	for (std::vector<uint64_t>::const_iterator model = represented->second.begin();
	     model != represented->second.end(); ++model) {
	    if (*model > static_cast<uint64_t>(INT_MAX)) continue;
	    const int model_id = static_cast<int>(*model);
	    if (id2name_map[model_id].empty())
		id2name_map[model_id] = database->StableBRLCADName(
		    id2name_map[product_id] + "_wire", model_id);
	    id2productid_map[model_id] = product_id;
	    brlcad::step::Representation &item = wrapper->Document().representations[model_id];
	    item.entity_id = model_id;
	    item.product_id = product_id;
	    item.type = wrapper->LazyTypeName(*model);
	    item.output_name = id2name_map[model_id];
	}
    }
    const std::vector<uint64_t> geometric_sets =
	wrapper->LazyInstancesByType("GEOMETRIC_SET");
    for (std::vector<uint64_t>::const_iterator set = geometric_sets.begin();
	 set != geometric_sets.end(); ++set) {
	if (*set > static_cast<uint64_t>(INT_MAX)) continue;
	const int set_id = static_cast<int>(*set);
	if (id2name_map[set_id].empty())
	    id2name_map[set_id] = database->StableBRLCADName("GeometricSet", set_id);
    }
}


static void
convert_lazy_occurrences(const LazySTEPExactGraph &graph, STEPWrapper *wrapper,
    BRLCADWrapper *database, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map)
{
    std::map<int, size_t> parent_occurrence_ordinals;
    for (std::vector<LazySTEPOccurrence>::const_iterator source = graph.occurrences.begin();
	 source != graph.occurrences.end(); ++source) {
	if (wrapper->CancellationRequested()) return;
	if (source->entity_id > static_cast<uint64_t>(INT_MAX) ||
	    source->parent_product_id > static_cast<uint64_t>(INT_MAX) ||
	    source->child_product_id > static_cast<uint64_t>(INT_MAX))
	    continue;
	const int occurrence_id = static_cast<int>(source->entity_id);
	const int parent_product = static_cast<int>(source->parent_product_id);
	const int child_product = static_cast<int>(source->child_product_id);
	const LazyRepresentationUnits units = lazy_representation_units(wrapper, graph,
	    source->parent_representation_id);
	mat_t matrix;
	if ((source->mapped && !lazy_mapped_occurrence_matrix(wrapper, *source,
		units.length, matrix)) ||
	    (!source->mapped && source->shape_method != "replicated" &&
		!lazy_occurrence_matrix(wrapper, *source, units.length, matrix))) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		occurrence_id, source->mapped ? "MAPPED_ITEM" :
		"CONTEXT_DEPENDENT_SHAPE_REPRESENTATION",
		"representation_relation", "could not materialize the indexed occurrence transform");
	    continue;
	}
	brlcad::step::Occurrence &occurrence = wrapper->Document().occurrences[occurrence_id];
	occurrence.entity_id = occurrence_id;
	occurrence.usage_entity_id = static_cast<int64_t>(source->usage_id);
	occurrence.parent_product_id = parent_product;
	occurrence.child_product_id = child_product;
	occurrence.shape_method = source->shape_method;
	for (size_t i = 0; i < 16; ++i) occurrence.matrix[i] = matrix[i];
	const size_t ordinal = ++parent_occurrence_ordinals[parent_product];
	if (dry_run) continue;
	const std::string &parent_name = id2name_map[parent_product];
	const std::string &child_name = id2name_map[child_product];
	if (!database->AddMember(parent_name, child_name, matrix)) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		occurrence_id, "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION",
		std::string(), "could not add the occurrence to its parent product");
	    continue;
	}
	const std::string prefix = "step:occurrence:" +
	    std::to_string(ordinal) + ':';
	bool attributes_written = database->SetCombinationAttribute(parent_name,
	    prefix + "child", child_name);
	attributes_written = database->SetCombinationAttribute(parent_name,
	    prefix + "source_id", std::to_string(source->usage_id)) &&
	    attributes_written;
	std::map<int64_t, brlcad::step::AssemblyUsage>::const_iterator usage =
	    wrapper->Document().assembly_usages.find(
		static_cast<int64_t>(source->usage_id));
	if (usage == wrapper->Document().assembly_usages.end() ||
		(usage->second.identifier.empty() && usage->second.name.empty() &&
		 usage->second.description.empty() &&
		 usage->second.reference_designator.empty())) {
	    if (!attributes_written)
		wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		    occurrence_id, "NEXT_ASSEMBLY_USAGE_OCCURRENCE", std::string(),
		    "could not retain occurrence source identity on the parent combination");
	    continue;
	}
	const auto retain = [&database, &parent_name, &prefix, &attributes_written](
		const char *key, const std::string &value) {
	    if (value.empty()) return;
	    attributes_written = database->SetCombinationAttribute(parent_name,
		prefix + key, value) && attributes_written;
	};
	retain("id", usage->second.identifier);
	retain("name", usage->second.name);
	retain("description", usage->second.description);
	retain("reference_designator", usage->second.reference_designator);
	if (!attributes_written)
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		occurrence_id, "NEXT_ASSEMBLY_USAGE_OCCURRENCE", std::string(),
		"could not retain occurrence metadata on the parent combination");
    }
}


#endif

static bool
retain_unit_structure(BRLCADWrapper *database, const std::string &object_name,
    const std::string &prefix, const brlcad::step::UnitStructure &unit,
    size_t depth)
{
    if (!database || unit.empty() || depth > 16) return false;
    bool valid = database->SetCombinationAttribute(object_name,
	prefix + "kind", unit.kind);
    const auto retain = [&database, &object_name, &prefix, &valid](
	    const char *field, const std::string &value) {
	if (value.empty()) return;
	valid = database->SetCombinationAttribute(object_name,
	    prefix + field, value) && valid;
    };
    retain("subtype", unit.subtype);
    retain("name", unit.name);
    retain("prefix", unit.prefix);
    if (unit.has_dimensions) {
	std::ostringstream dimensions;
	dimensions << std::setprecision(17);
	for (size_t i = 0; i < unit.dimension_exponents.size(); ++i) {
	    if (i) dimensions << ' ';
	    dimensions << unit.dimension_exponents[i];
	}
	retain("dimensions", dimensions.str());
    }
    if (unit.has_conversion_value) {
	std::ostringstream conversion_value;
	conversion_value << std::setprecision(17) << unit.conversion_value;
	retain("conversion_value", conversion_value.str());
	retain("conversion_value_type", unit.conversion_value_type);
    }
    for (size_t i = 0; i < unit.components.size(); ++i) {
	const std::string component_prefix = prefix + "component:" +
	    std::to_string(i + 1) + ':';
	if (unit.kind == "derived" && i < unit.exponents.size()) {
	    std::ostringstream exponent;
	    exponent << std::setprecision(17) << unit.exponents[i];
	    valid = database->SetCombinationAttribute(object_name,
		component_prefix + "exponent", exponent.str()) && valid;
	}
	valid = retain_unit_structure(database, object_name, component_prefix,
	    unit.components[i], depth + 1) && valid;
    }
    return valid;
}

static bool
retain_metadata_property(STEPWrapper *wrapper, BRLCADWrapper *database,
    const std::string &object_name, const std::string &prefix,
    const brlcad::step::MetadataProperty &property)
{
    if (!wrapper || !database) return false;
    if (!property.valid) return false;
    bool valid = true;
    const auto retain = [&database, &object_name, &prefix, &valid](
	    const char *field, const std::string &value) {
	if (value.empty()) return;
	valid = database->SetCombinationAttribute(object_name,
	    prefix + field, value) && valid;
    };
    retain("category", property.category);
    retain("name", property.name);
    retain("description", property.description);
    retain("value_type", property.value_type);
    retain("units", property.units);
    retain("text", property.text);
    if (!property.values.empty()) {
	std::ostringstream values;
	values << std::setprecision(17);
	for (size_t i = 0; i < property.values.size(); ++i) {
	    if (i) values << ' ';
	    values << property.values[i];
	}
	retain("values", values.str());
    }
    if (property.has_dimensions) {
	std::ostringstream dimensions;
	dimensions << std::setprecision(17);
	for (size_t i = 0; i < property.dimension_exponents.size(); ++i) {
	    if (i) dimensions << ' ';
	    dimensions << property.dimension_exponents[i];
	}
	retain("dimensions", dimensions.str());
    }
    if (!property.unit_structure.empty() &&
	    !retain_unit_structure(database, object_name, prefix + "unit:",
		property.unit_structure, 0)) {
	valid = false;
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
	    property.item_entity_id, "MEASURE_REPRESENTATION_ITEM",
	    "unit_component",
	    "could not retain structured unit attributes on the product combination");
    }
    return valid;
}

static void
ensure_product_combinations(STEPWrapper *wrapper, BRLCADWrapper *database,
    int dry_run, const MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map)
{
    if (!wrapper || !database || dry_run) return;

    brlcad::step::Document &document = wrapper->Document();
    for (std::map<int64_t, brlcad::step::Product>::iterator entry =
	 document.products.begin(); entry != document.products.end(); ++entry) {
	if (entry->first <= 0 || entry->first > INT_MAX) continue;
	brlcad::step::Product &product = entry->second;
	std::string output_name = product.output_name;
	MAP_OF_ENTITY_ID_TO_PRODUCT_NAME::const_iterator mapped =
	    id2name_map.find(static_cast<int>(entry->first));
	if (output_name.empty() && mapped != id2name_map.end())
	    output_name = mapped->second;
	if (output_name.empty()) continue;
	product.output_name = output_name;
	if (!database->EnsureCombination(output_name)) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		product.entity_id, "PRODUCT", std::string(),
		"could not create the product combination");
	    continue;
	}
	database->SetCombinationProperties(output_name, false, product.entity_id,
	    product.original_name);
	database->SetCombinationAttribute(output_name, "step:source_id",
	    std::to_string(product.entity_id));
	database->SetCombinationAttribute(output_name, "step:object_role", "product");
	if (product.formation_entity_ids.size() == 1)
	    database->SetCombinationAttribute(output_name,
		"step:formation_source_id",
		std::to_string(product.formation_entity_ids.front()));
	if (product.definition_entity_ids.size() == 1)
	    database->SetCombinationAttribute(output_name,
		"step:definition_source_id",
		std::to_string(product.definition_entity_ids.front()));
	if (!product.identifier.empty())
	    database->SetCombinationAttribute(output_name, "step:product_id", product.identifier);
	if (!product.description.empty())
	    database->SetCombinationAttribute(output_name, "step:description", product.description);
	if (!product.revision.empty())
	    database->SetCombinationAttribute(output_name, "step:revision", product.revision);
	if (!product.revision_description.empty())
	    database->SetCombinationAttribute(output_name, "step:revision_description",
		product.revision_description);
	if (!product.definition_identifier.empty())
	    database->SetCombinationAttribute(output_name, "step:definition_id",
		product.definition_identifier);
	if (!product.definition_description.empty())
	    database->SetCombinationAttribute(output_name, "step:definition_description",
		product.definition_description);
	size_t context_ordinal = 0;
	for (const auto &context_entry :
		wrapper->Document().representation_contexts) {
	    const brlcad::step::RepresentationContextProvenance &context =
		context_entry.second;
	    if (context.product_id != product.entity_id) continue;
	    const std::string prefix = "step:source_context:" +
		std::to_string(++context_ordinal) + ':';
	    std::ostringstream value;
	    value << std::setprecision(17) << context.length_unit_mm;
	    database->SetCombinationAttribute(output_name,
		prefix + "length_unit_mm", value.str());
	    const char *length_name = bu_units_string(context.length_unit_mm);
	    if (length_name)
		database->SetCombinationAttribute(output_name,
		    prefix + "length_unit", length_name);
	    value.str(std::string());
	    value.clear();
	    value << std::setprecision(17) << context.plane_angle_unit_radians;
	    database->SetCombinationAttribute(output_name,
		prefix + "plane_angle_unit_radians", value.str());
	    const char *angle_name = NULL;
	    if (std::fabs(context.plane_angle_unit_radians - DEG2RAD) <= 1.0e-14)
		angle_name = "degree";
	    else if (std::fabs(context.plane_angle_unit_radians - 1.0) <= 1.0e-14)
		angle_name = "radian";
	    if (angle_name)
		database->SetCombinationAttribute(output_name,
		    prefix + "plane_angle_unit", angle_name);
	    value.str(std::string());
	    value.clear();
	    value << std::setprecision(17) << context.solid_angle_unit_steradians;
	    database->SetCombinationAttribute(output_name,
		prefix + "solid_angle_unit_steradians", value.str());
	    database->SetCombinationAttribute(output_name,
		prefix + "representation_id",
		std::to_string(context.representation_id));
	    if (context.has_uncertainty) {
		value.str(std::string());
		value.clear();
		value << std::setprecision(17) << context.uncertainty_mm;
		database->SetCombinationAttribute(output_name,
		    prefix + "uncertainty_mm", value.str());
	    }
	}
	for (size_t material_index = 0; material_index < product.materials.size();
	     ++material_index) {
	    const brlcad::step::Material &material = product.materials[material_index];
	    const std::string prefix = "step:material:" +
		std::to_string(material_index + 1) + ':';
	    if (!material.identifier.empty())
		database->SetCombinationAttribute(output_name, prefix + "id",
		    material.identifier);
	    if (!material.name.empty())
		database->SetCombinationAttribute(output_name, prefix + "name",
		    material.name);
	    if (!material.description.empty())
		database->SetCombinationAttribute(output_name, prefix + "description",
		    material.description);
	    if (material_index == 0) {
		if (!material.identifier.empty())
		    database->SetCombinationAttribute(output_name, "step:material_id",
			material.identifier);
		if (!material.name.empty())
		    database->SetCombinationAttribute(output_name, "step:material_name",
			material.name);
		if (!material.description.empty())
		    database->SetCombinationAttribute(output_name,
			"step:material_description", material.description);
	    }
	    for (size_t property_index = 0;
		 property_index < material.properties.size(); ++property_index) {
		const brlcad::step::MetadataProperty &property =
		    material.properties[property_index];
		const std::string property_prefix = prefix + "property:" +
		    std::to_string(property_index + 1) + ':';
		retain_metadata_property(wrapper, database, output_name,
		    property_prefix, property);
	    }
	}
	for (size_t property_index = 0;
	     property_index < product.validation_properties.size();
	     ++property_index) {
	    const std::string prefix = "step:property:" +
		std::to_string(property_index + 1) + ':';
	    retain_metadata_property(wrapper, database, output_name, prefix,
		product.validation_properties[property_index]);
	}
    }
}


static void
write_global_step_attributes(STEPWrapper *wrapper, BRLCADWrapper *database,
    int dry_run)
{
    if (!wrapper || !database || dry_run) return;
    if (!database->WriteSTEPMetadata(wrapper->Document().global_attributes))
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, 0,
	    "PART21_FILE", "_GLOBAL",
	    "could not retain STEP configuration and PMI metadata");
}


#ifdef HAVE_STEPCODE_LAZY
static uint64_t
lazy_indexed_surface_geometry_job_count(STEPWrapper *wrapper)
{
    if (!wrapper || !wrapper->HasLazyIndex()) return 0;
    const std::set<int64_t> &selected =
	wrapper->ImportOptions().selected_entity_ids;
    const bool skip_open_shells =
	wrapper->ImportOptions().skip_open_shells;
    uint64_t count = 0;
    const std::vector<uint64_t> surface_ids =
	wrapper->LazyInstancesByType("SHELL_BASED_SURFACE_MODEL");
    for (std::vector<uint64_t>::const_iterator surface_id =
	    surface_ids.begin(); surface_id != surface_ids.end(); ++surface_id) {
	if (*surface_id > static_cast<uint64_t>(INT_MAX)) continue;
	const bool selected_model = selected.empty() || selected.find(
	    static_cast<int64_t>(*surface_id)) != selected.end();
	const std::vector<uint64_t> boundaries =
	    wrapper->LazyForwardReferences(*surface_id);
	bool selected_boundary = false;
	if (!selected_model) {
	    for (std::vector<uint64_t>::const_iterator boundary =
		    boundaries.begin(); boundary != boundaries.end(); ++boundary) {
		if (*boundary <= static_cast<uint64_t>(INT64_MAX) &&
			selected.find(static_cast<int64_t>(*boundary)) !=
			selected.end()) {
		    selected_boundary = true;
		    break;
		}
	    }
	}
	if (!selected_model && !selected_boundary) continue;
	uint64_t model_jobs = 0;
	bool has_selected_shell = false;
	for (std::vector<uint64_t>::const_iterator boundary =
		boundaries.begin(); boundary != boundaries.end(); ++boundary) {
	    const std::string type = wrapper->LazyTypeName(*boundary);
	    if (*boundary <= static_cast<uint64_t>(INT_MAX) &&
		    (type == "CLOSED_SHELL" || type == "OPEN_SHELL") &&
		    (selected_model || selected.find(
			static_cast<int64_t>(*boundary)) != selected.end())) {
		has_selected_shell = true;
		if (type == "OPEN_SHELL" && skip_open_shells) continue;
		++model_jobs;
	    }
	}
	/* A selected malformed model with no shell boundary is still one unit of
	 * conversion work: it is attempted, diagnosed, and skipped below. */
	count += model_jobs ? model_jobs : (has_selected_shell ? 0 : 1);
    }
    return count;
}


static void
lazy_collect_edge_curves(STEPWrapper *wrapper, uint64_t root,
    std::set<uint64_t> &visited, std::set<uint64_t> &edges)
{
    if (!wrapper || !visited.insert(root).second) return;
    if (wrapper->LazyTypeName(root) == "EDGE_CURVE") {
	edges.insert(root);
	return;
    }
    const std::vector<uint64_t> refs = wrapper->LazyForwardReferences(root);
    for (std::vector<uint64_t>::const_iterator ref = refs.begin(); ref != refs.end(); ++ref)
	lazy_collect_edge_curves(wrapper, *ref, visited, edges);
}


static void
convert_lazy_wireframe_models(const LazySTEPExactGraph &graph,
    STEPWrapper *wrapper, BRLCADWrapper *database, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    for (std::map<uint64_t, std::vector<uint64_t> >::const_iterator represented =
	    graph.representation_wireframe_models.begin();
	 represented != graph.representation_wireframe_models.end(); ++represented) {
	std::map<uint64_t, uint64_t>::const_iterator product_mapping =
	    graph.representation_product.find(represented->first);
	if (product_mapping == graph.representation_product.end() ||
		product_mapping->second > static_cast<uint64_t>(INT_MAX)) continue;
	const int product_id = static_cast<int>(product_mapping->second);
	const LazyRepresentationUnits units = lazy_representation_units(wrapper, graph,
	    represented->first);
	mat_t representation_transform;
	if (!lazy_product_representation_matrix(graph, wrapper,
		represented->first, units.length,
		representation_transform)) continue;

	for (std::vector<uint64_t>::const_iterator model = represented->second.begin();
	     model != represented->second.end(); ++model) {
	    if (*model > static_cast<uint64_t>(INT_MAX) ||
		    !wrapper->ShouldConvertEntity(static_cast<int64_t>(*model))) continue;
	    const int model_id = static_cast<int>(*model);
	    const std::string model_type = wrapper->LazyTypeName(*model);
	    std::set<uint64_t> visited;
	    std::set<uint64_t> edge_ids;
	    lazy_collect_edge_curves(wrapper, *model, visited, edge_ids);
	    LocalUnits::length = units.length;
	    LocalUnits::planeangle = units.planeangle;
	    LocalUnits::solidangle = units.solidangle;
	    LocalUnits::tolerance = units.tolerance;
	    wrapper->ResetOpenNURBSState();
	    std::unique_ptr<ON_Brep> wire(new ON_Brep());
	    bool converted = !edge_ids.empty();
	    for (std::set<uint64_t>::const_iterator edge_id = edge_ids.begin();
		 edge_id != edge_ids.end() && converted; ++edge_id) {
		if (*edge_id > static_cast<uint64_t>(INT_MAX)) {
		    converted = false;
		    break;
		}
		SDAI_Application_instance *source = wrapper->getEntity(static_cast<int>(*edge_id));
		EdgeCurve *edge = source ? dynamic_cast<EdgeCurve *>(
		    Factory::CreateObject(wrapper, source)) : NULL;
		converted = edge && edge->LoadONBrep(wire.get());
	    }
	    if (converted) {
		wire->Compact();
		wire->SetTolerancesBoxesAndFlags(false, false, false, false,
		    true, true, true, true);
		ON_wString messages;
		ON_TextLog log(messages);
		converted = wire->m_E.Count() > 0 && wire->IsValid(&log);
	    }
	    BrepWriteStatus status = converted ? BREP_WRITE_SUCCESS :
		BREP_CONVERSION_FAILED;
	    const brlcad::step::Style *style = style_for_item(wrapper, model_id);
	    if (!style) style = style_for_item(wrapper,
		static_cast<int>(represented->first));
	    if (converted && !dry_run && !database->WriteBrep(id2name_map[model_id],
		    wire.get(), representation_transform, false, model_id,
		    std::string(), style))
		status = BREP_OUTPUT_FAILED;
	    record_brep_result(wrapper, status, model_id, model_type);
	    if (status == BREP_WRITE_SUCCESS) {
		process_map[model_id] = product_id;
		if (style) ++wrapper->Statistics().styles_applied;
		if (!dry_run) {
		    mat_t identity;
		    MAT_IDN(identity);
		    database->AddMember(id2name_map[product_id], id2name_map[model_id], identity);
		}
	    }
	    wrapper->ReleaseSourceData();
	}
    }
}


static void
convert_lazy_exact_graph(const LazySTEPExactGraph &graph, STEPWrapper *wrapper,
    BRLCADWrapper *database, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    std::vector<std::unique_ptr<DetachedBrepJob> > jobs;
    std::map<int, DetachedBrepJob *> scheduled;
    for (std::map<uint64_t, std::vector<uint64_t> >::const_iterator represented =
	 graph.representation_solids.begin();
	 represented != graph.representation_solids.end(); ++represented) {
	if (graph.handled_representations.find(represented->first) ==
	    graph.handled_representations.end()) continue;
	std::map<uint64_t, uint64_t>::const_iterator product_mapping =
	    graph.representation_product.find(represented->first);
	if (product_mapping == graph.representation_product.end() ||
	    product_mapping->second > static_cast<uint64_t>(INT_MAX)) continue;
	const int product_id = static_cast<int>(product_mapping->second);
	if (id2name_map[product_id].empty()) continue;
	const LazyRepresentationUnits units = lazy_representation_units(wrapper, graph,
	    represented->first);
	mat_t member_matrix;
	if (!lazy_product_representation_matrix(graph, wrapper,
		represented->first, units.length,
		member_matrix)) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		static_cast<int64_t>(represented->first),
		wrapper->LazyTypeName(represented->first), "items",
		"could not materialize the indexed representation placement");
	    continue;
	}
	for (std::vector<uint64_t>::const_iterator source_id = represented->second.begin();
	     source_id != represented->second.end(); ++source_id) {
	    if (wrapper->CancellationRequested()) return;
	    if (*source_id > static_cast<uint64_t>(INT_MAX)) continue;
	    const int solid_id = static_cast<int>(*source_id);
	    if (!wrapper->ShouldConvertEntity(solid_id)) continue;
	    DetachedBrepJob *job = NULL;
	    std::map<int, DetachedBrepJob *>::iterator existing = scheduled.find(solid_id);
	    if (existing != scheduled.end()) {
		job = existing->second;
	    } else {
		const brlcad::step::Style *style = style_for_item(wrapper, solid_id);
		if (!style) style = style_for_item(wrapper, static_cast<int>(represented->first));
		mat_t identity;
		MAT_IDN(identity);
		std::unique_ptr<DetachedBrepJob> detached = detach_brep_job_data(
		    solid_id, wrapper->LazyTypeName(*source_id), std::string(),
		    id2name_map[solid_id], product_id, units.length, units.planeangle,
		    units.solidangle, units.tolerance, identity, style);
		job = detached.get();
		scheduled[solid_id] = job;
		jobs.push_back(std::move(detached));
	    }
	    DetachedBrepMember member;
	    member.combination = id2name_map[product_id];
	    MAT_COPY(member.matrix, member_matrix);
	    job->members.push_back(member);
	}
    }
    wrapper->ReleaseSourceData();
    /* The lazy exact-solid and supplemental surface passes use separate
     * bounded job vectors.  Publish their combined finalized denominator
     * before the first worker starts so the user-facing processed/total count
     * does not grow when the second pass begins. */
    const uint64_t geometry_base = wrapper->Statistics().geometry_attempted;
    wrapper->SetGeometryOverallProgress(geometry_base,
	geometry_base + static_cast<uint64_t>(jobs.size()) +
	    lazy_indexed_surface_geometry_job_count(wrapper));
    write_detached_brep_jobs(jobs, wrapper, database, dry_run, process_map);
    convert_lazy_wireframe_models(graph, wrapper, database, dry_run,
	id2name_map, process_map);
}


static bool
lazy_selection_contains_only_exact_roots(STEPWrapper *wrapper)
{
    if (!wrapper || !wrapper->HasLazyIndex() ||
	wrapper->ImportOptions().selected_entity_ids.empty())
	return false;
    static const std::set<std::string> exact_types = {
	"MANIFOLD_SOLID_BREP", "BREP_WITH_VOIDS", "FACETED_BREP", "SOLID_REPLICA"
    };
    for (std::set<int64_t>::const_iterator id =
	    wrapper->ImportOptions().selected_entity_ids.begin();
	 id != wrapper->ImportOptions().selected_entity_ids.end(); ++id) {
	if (*id <= 0 || exact_types.find(wrapper->LazyTypeName(
		static_cast<uint64_t>(*id))) == exact_types.end())
	    return false;
    }
    return true;
}


static bool
lazy_selection_contains_only_topology_roots(STEPWrapper *wrapper)
{
    if (!wrapper || !wrapper->HasLazyIndex() ||
	wrapper->ImportOptions().selected_entity_ids.empty())
	return false;
    for (std::set<int64_t>::const_iterator id =
	    wrapper->ImportOptions().selected_entity_ids.begin();
	 id != wrapper->ImportOptions().selected_entity_ids.end(); ++id) {
	if (*id <= 0)
	    return false;
	const std::string type = wrapper->LazyTypeName(
	    static_cast<uint64_t>(*id));
	if (type != "CLOSED_SHELL" && type != "OPEN_SHELL" &&
		type != "ADVANCED_FACE")
	    return false;
    }
    return true;
}


static void
convert_lazy_selected_exact_roots(const LazySTEPExactGraph &graph,
    STEPWrapper *wrapper, BRLCADWrapper *database, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    std::vector<std::unique_ptr<DetachedBrepJob> > jobs;
    for (std::set<int64_t>::const_iterator selected =
	    wrapper->ImportOptions().selected_entity_ids.begin();
	 selected != wrapper->ImportOptions().selected_entity_ids.end(); ++selected) {
	if (*selected <= 0 || *selected > INT_MAX) continue;
	const uint64_t source_id = static_cast<uint64_t>(*selected);
	const std::string source_type = wrapper->LazyTypeName(source_id);
	/* Supplemental open surface models have their own product/representation
	 * mapping below.  Do not also schedule them as selected solid roots. */
	if (source_type == "SHELL_BASED_SURFACE_MODEL")
	    continue;
	uint64_t representation_id = 0;
	for (std::map<uint64_t, std::vector<uint64_t> >::const_iterator represented =
		graph.representation_solids.begin();
	     represented != graph.representation_solids.end() && !representation_id;
	     ++represented) {
	    if (std::find(represented->second.begin(), represented->second.end(),
		    source_id) != represented->second.end())
		representation_id = represented->first;
	}
	const LazyRepresentationUnits units = lazy_representation_units(wrapper, graph,
	    representation_id);
	const int entity_id = static_cast<int>(*selected);
	int product_id = 0;
	std::map<uint64_t, uint64_t>::const_iterator product =
	    graph.representation_product.find(representation_id);
	if (product != graph.representation_product.end() && product->second <= INT_MAX)
	    product_id = static_cast<int>(product->second);
	mat_t identity;
	MAT_IDN(identity);
	const brlcad::step::Style *style = style_for_item(wrapper, entity_id);
	std::unique_ptr<DetachedBrepJob> job = detach_brep_job_data(entity_id,
	    source_type, std::string(),
	    database->StableBRLCADName("step_item", entity_id), product_id,
	    units.length, units.planeangle, units.solidangle, units.tolerance,
	    identity, style);
	wrapper->ShouldConvertEntity(entity_id);
	jobs.push_back(std::move(job));
    }
    wrapper->ReleaseSourceData();
    write_detached_brep_jobs(jobs, wrapper, database, dry_run, process_map);
}


static void
convert_lazy_selected_topology_roots(const LazySTEPExactGraph &graph,
    STEPWrapper *wrapper, BRLCADWrapper *database, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    std::vector<std::unique_ptr<DetachedBrepJob> > jobs;
    for (std::set<int64_t>::const_iterator selected =
	    wrapper->ImportOptions().selected_entity_ids.begin();
	 selected != wrapper->ImportOptions().selected_entity_ids.end(); ++selected) {
	if (*selected <= 0 || *selected > INT_MAX) continue;
	const uint64_t source_id = static_cast<uint64_t>(*selected);
	const std::string source_type = wrapper->LazyTypeName(source_id);
	if (source_type == "OPEN_SHELL" &&
		wrapper->ImportOptions().skip_open_shells) {
	    wrapper->ShouldConvertEntity(*selected);
	    ++wrapper->Statistics().geometry_filtered;
	    wrapper->RecordRepresentationItemCoverage(*selected,
		brlcad::step::RepresentationCoverageStatus::Filtered,
		"OPEN_SHELL excluded by --skip-open-shells");
	    continue;
	}

	/* A shell is normally referenced by a SHELL_BASED_SURFACE_MODEL, which is
	 * in turn referenced by its shape representation.  Follow only these
	 * indexed reverse edges so a focused shell request can recover units and
	 * style without materializing the surrounding product and every sibling
	 * shell. */
	uint64_t surface_model_id = 0;
	uint64_t solid_root_id = 0;
	uint64_t representation_id = 0;
	uint64_t ownership_root = source_id;
	if (source_type == "ADVANCED_FACE") {
	    const std::vector<uint64_t> face_parents =
		wrapper->LazyReverseReferences(source_id);
	    for (std::vector<uint64_t>::const_iterator parent =
		    face_parents.begin(); parent != face_parents.end(); ++parent) {
		const std::string parent_type = wrapper->LazyTypeName(*parent);
		if (parent_type == "CLOSED_SHELL" || parent_type == "OPEN_SHELL") {
		    ownership_root = *parent;
		    break;
		}
	    }
	}
	const std::vector<uint64_t> shell_parents =
	    wrapper->LazyReverseReferences(ownership_root);
	for (std::vector<uint64_t>::const_iterator parent = shell_parents.begin();
	     parent != shell_parents.end(); ++parent) {
	    const std::string parent_type = wrapper->LazyTypeName(*parent);
	    if (parent_type == "MANIFOLD_SOLID_BREP" ||
		    parent_type == "BREP_WITH_VOIDS" ||
		    parent_type == "FACETED_BREP" ||
		    parent_type == "SOLID_REPLICA") {
		solid_root_id = *parent;
		for (std::map<uint64_t, std::vector<uint64_t> >::const_iterator
			represented = graph.representation_solids.begin();
		     represented != graph.representation_solids.end(); ++represented) {
		    if (std::find(represented->second.begin(),
			    represented->second.end(), solid_root_id) !=
			    represented->second.end()) {
			representation_id = represented->first;
			break;
		    }
		}
		if (representation_id)
		    break;
		continue;
	    }
	    if (parent_type != "SHELL_BASED_SURFACE_MODEL")
		continue;
	    surface_model_id = *parent;
	    const std::vector<uint64_t> model_parents =
		wrapper->LazyReverseReferences(*parent);
	    for (std::vector<uint64_t>::const_iterator representation =
		    model_parents.begin(); representation != model_parents.end();
		 representation++) {
		if (graph.representation_context.find(*representation) !=
			graph.representation_context.end() ||
		    graph.representation_product.find(*representation) !=
			graph.representation_product.end()) {
		    representation_id = *representation;
		    break;
		}
	    }
	    if (representation_id) break;
	}

	const LazyRepresentationUnits units = lazy_representation_units(wrapper,
	    graph, representation_id);
	int product_id = 0;
	std::map<uint64_t, uint64_t>::const_iterator product =
	    graph.representation_product.find(representation_id);
	if (product != graph.representation_product.end() &&
		product->second <= static_cast<uint64_t>(INT_MAX))
	    product_id = static_cast<int>(product->second);

	const int entity_id = static_cast<int>(*selected);
	const brlcad::step::Style *style = style_for_item(wrapper, entity_id);
	if (!style && ownership_root <= static_cast<uint64_t>(INT_MAX))
	    style = style_for_item(wrapper, static_cast<int>(ownership_root));
	if (!style && solid_root_id <= static_cast<uint64_t>(INT_MAX))
	    style = style_for_item(wrapper, static_cast<int>(solid_root_id));
	if (!style && surface_model_id <= static_cast<uint64_t>(INT_MAX))
	    style = style_for_item(wrapper, static_cast<int>(surface_model_id));
	if (!style && representation_id <= static_cast<uint64_t>(INT_MAX))
	    style = style_for_item(wrapper, static_cast<int>(representation_id));
	mat_t identity;
	MAT_IDN(identity);
	std::unique_ptr<DetachedBrepJob> job = detach_brep_job_data(entity_id,
	    source_type, std::string(), database->StableBRLCADName(
		(source_type == "CLOSED_SHELL" ? "closed_shell" :
		(source_type == "OPEN_SHELL" ? "open_shell" : "advanced_face")),
		entity_id), product_id, units.length, units.planeangle,
	    units.solidangle, units.tolerance, identity, style);
	job->is_region = source_type == "CLOSED_SHELL";
	wrapper->ShouldConvertEntity(entity_id);
	jobs.push_back(std::move(job));
    }
    wrapper->ReleaseSourceData();
    write_detached_brep_jobs(jobs, wrapper, database, dry_run, process_map);
}
#endif

static void
index_representation_geometry(Representation *representation, int product_id,
    STEPWrapper *wrapper, BRLCADWrapper *dotg,
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &id2productid_map)
{
    if (!representation || product_id <= 0 || !wrapper || !dotg)
	return;

    const std::string product_name = id2name_map[product_id];
    LIST_OF_REPRESENTATION_ITEMS *items = representation->items_();
    if (!items)
	return;
    const std::vector<FlattenedRepresentationItem> flattened =
	flatten_representation_items(items, wrapper);
    for (std::vector<FlattenedRepresentationItem>::const_iterator entry = flattened.begin();
	 entry != flattened.end(); ++entry) {
	if (wrapper->CancellationRequested()) return;
	RepresentationItem *item = entry->item;
	SolidModel *solid = exact_brep_solid(item);
	MappedItem *mapped = dynamic_cast<MappedItem *>(item);
	GeometricSet *geometric_set = dynamic_cast<GeometricSet *>(item);
	Plane *plane = dynamic_cast<Plane *>(item);
	if (!solid && !mapped && !geometric_set && !plane)
	    continue;
	const int item_id = item->GetId();
	if (id2name_map[item_id].empty())
	    id2name_map[item_id] = dotg->StableBRLCADName(product_name + "_item", item_id);
	id2productid_map[item_id] = product_id;
	brlcad::step::Representation &item_record = wrapper->Document().representations[item_id];
	item_record.entity_id = item_id;
	item_record.product_id = product_id;
	item_record.type = mapped ? "MAPPED_ITEM" :
	    (geometric_set ? "GEOMETRIC_SET" :
	    (plane ? "PLANE" : exact_brep_solid_type(solid)));
	item_record.output_name = id2name_map[item_id];
    }
}


static void
convert_representation_geometry(Representation *representation, int product_id,
    STEPWrapper *wrapper, BRLCADWrapper *dotg, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    if (!representation || product_id <= 0 || !wrapper || !dotg)
	return;

    LIST_OF_REPRESENTATION_ITEMS *items = representation->items_();
    if (!items)
	return;
    const std::vector<FlattenedRepresentationItem> flattened =
	flatten_representation_items(items, wrapper);
    std::vector<std::unique_ptr<DetachedBrepJob> > jobs;
    std::map<int, DetachedBrepJob *> scheduled;
    for (std::vector<FlattenedRepresentationItem>::const_iterator entry = flattened.begin();
	 entry != flattened.end(); ++entry) {
	if (wrapper->CancellationRequested()) return;
	RepresentationItem *item = entry->item;
	MappedItem *mapped = dynamic_cast<MappedItem *>(item);
	if (mapped) {
	    if (!wrapper->ShouldConvertEntity(mapped->GetId())) continue;
	    RepresentationMap *source_map = mapped->MappingSource();
	    Representation *source = source_map ? source_map->MappedRepresentation() : NULL;
	    if (source) {
		std::map<int64_t, brlcad::step::Representation>::const_iterator represented =
		    wrapper->Document().representations.find(source->GetId());
		if (represented != wrapper->Document().representations.end() &&
			represented->second.product_id > 0 &&
			represented->second.product_id != product_id) {
		    /* Cross-product mappings are AP203 assembly occurrences.  The
		     * child representation is converted under its own product and
		     * convert_lazy_occurrences installs this transform once. */
		    wrapper->RecordRepresentationItemCoverage(mapped->GetId(),
			brlcad::step::RepresentationCoverageStatus::Handled,
			"mapped item is represented by a cross-product assembly occurrence");
		    continue;
		}
	    }
	    mat_t mapped_matrix;
	    if (!source || !mapped_item_matrix(mapped, source->GetLengthConversionFactor(), mapped_matrix)) {
		record_brep_result(wrapper, BREP_CONVERSION_FAILED, mapped->GetId(), "MAPPED_ITEM");
		continue;
	    }

	    const brlcad::step::Style *mapped_style = style_for_flattened_item(wrapper, *entry);
	    LIST_OF_REPRESENTATION_ITEMS *source_items = source->items_();
	    bool supported_source = false;
	    if (source_items) {
		const std::vector<FlattenedRepresentationItem> flattened_source =
		    flatten_representation_items(source_items, wrapper);
		for (std::vector<FlattenedRepresentationItem>::const_iterator source_entry =
			 flattened_source.begin(); source_entry != flattened_source.end(); ++source_entry) {
		    if (wrapper->CancellationRequested()) return;
		    SolidModel *source_solid = exact_brep_solid(source_entry->item);
		    if (!source_solid)
			continue;
		    supported_source = true;
		    const int solid_id = source_solid->GetId();
		    std::string source_name = id2name_map[solid_id];
		    if (source_name.empty()) {
			source_name = dotg->StableBRLCADName(id2name_map[product_id] + "_mapped_item", solid_id);
			id2name_map[solid_id] = source_name;
		    }

		    bool source_available = process_map.find(solid_id) != process_map.end();
		    DetachedBrepJob *job = NULL;
		    std::map<int, DetachedBrepJob *>::iterator pending = scheduled.find(solid_id);
		    if (pending != scheduled.end())
			job = pending->second;
		    if (!source_available && !job) {
			mat_t identity;
			MAT_IDN(identity);
			const brlcad::step::Style *source_style = mapped_style ? mapped_style :
			    style_for_flattened_item(wrapper, *source_entry);
			std::unique_ptr<DetachedBrepJob> detached = detach_brep_job(
			    source_solid, source, wrapper, source_name, product_id,
			    identity, source_style);
			job = detached.get();
			scheduled[solid_id] = job;
			jobs.push_back(std::move(detached));
		    }
		    if (job) {
			DetachedBrepMember member;
			member.combination = id2name_map[product_id];
			MAT_COPY(member.matrix, mapped_matrix);
			member.representation_map_id = source_map->GetId();
			job->members.push_back(member);
		    } else if (source_available && !dry_run) {
			dotg->AddMember(id2name_map[product_id], source_name, mapped_matrix);
			dotg->SetAttribute(source_name, "step:representation_map_id",
			    std::to_string(source_map->GetId()));
		    }
		}
	    }
	    if (!supported_source)
		record_brep_result(wrapper, BREP_CONVERSION_FAILED, mapped->GetId(), "MAPPED_ITEM");
	    else
		wrapper->RecordRepresentationItemCoverage(mapped->GetId(),
		    brlcad::step::RepresentationCoverageStatus::Handled,
		    "mapped exact geometry and transform were resolved");
	    continue;
	}

	Plane *plane = dynamic_cast<Plane *>(item);
	if (plane) {
	    const int plane_id = plane->GetId();
	    if (!wrapper->ShouldConvertEntity(plane_id) ||
		    process_map.find(plane_id) != process_map.end())
		continue;
	    std::string plane_name = id2name_map[plane_id];
	    if (plane_name.empty()) {
		plane_name = dotg->StableBRLCADName(
		    id2name_map[product_id] + "_datum_plane", plane_id);
		id2name_map[plane_id] = plane_name;
	    }
	    const BrepWriteStatus status = step_convert_datum_plane(plane,
		representation, dotg, plane_name, dry_run);
	    record_brep_result(wrapper, status, plane_id, "PLANE");
	    if (status == BREP_WRITE_SUCCESS) {
		process_map[plane_id] = product_id;
		if (!dry_run) {
		    mat_t identity;
		    MAT_IDN(identity);
		    dotg->AddMember(id2name_map[product_id], plane_name, identity);
		}
	    }
	    continue;
	}

	GeometricSet *geometric_set = dynamic_cast<GeometricSet *>(item);
	if (geometric_set &&
		(step_geometric_set_has_curves(geometric_set) ||
		 step_geometric_set_has_points(geometric_set) ||
		 step_geometric_set_has_surfaces(geometric_set))) {
	    const int set_id = geometric_set->GetId();
	    if (!wrapper->ShouldConvertEntity(set_id) ||
		    process_map.find(set_id) != process_map.end())
		continue;
	    std::string set_name = id2name_map[set_id];
	    if (set_name.empty()) {
		set_name = dotg->StableBRLCADName(
		    id2name_map[product_id] + "_geometric_set", set_id);
		id2name_map[set_id] = set_name;
	    }
	    const brlcad::step::Style *item_style =
		style_for_flattened_item(wrapper, *entry);
	    const BrepWriteStatus status = step_convert_geometric_set(geometric_set,
		representation, wrapper, dotg, &set_name, dry_run, item_style);
	    record_brep_result(wrapper, status, set_id, "GEOMETRIC_SET");
	    if (status == BREP_WRITE_SUCCESS) {
		process_map[set_id] = product_id;
		if (!dry_run) {
		    mat_t identity;
		    MAT_IDN(identity);
		    dotg->AddMember(id2name_map[product_id], set_name, identity);
		}
	    }
	    continue;
	}

	SolidModel *solid = exact_brep_solid(item);
	if (!solid || !wrapper->ShouldConvertEntity(solid->GetId()) ||
	    process_map.find(solid->GetId()) != process_map.end())
	    continue;
	const int solid_id = solid->GetId();
	std::string name = id2name_map[solid_id];
	if (name.empty()) {
	    name = dotg->StableBRLCADName(id2name_map[product_id] + "_item", solid_id);
	    id2name_map[solid_id] = name;
	}

	DetachedBrepJob *job = NULL;
	std::map<int, DetachedBrepJob *>::iterator pending = scheduled.find(solid_id);
	if (pending != scheduled.end()) {
	    job = pending->second;
	} else {
	    const brlcad::step::Style *item_style = style_for_flattened_item(wrapper, *entry);
	    std::unique_ptr<DetachedBrepJob> detached = detach_brep_job(solid,
		representation, wrapper, name, product_id, NULL, item_style);
	    job = detached.get();
	    scheduled[solid_id] = job;
	    jobs.push_back(std::move(detached));
	}
	DetachedBrepMember member;
	member.combination = id2name_map[product_id];
	job->members.push_back(member);
    }

    /* No worker may retain or access an SDAI object.  ON_Brep and copied
     * metadata above are complete, so evict the source graph before starting
     * bounded validation/tessellation workers. */
    wrapper->ReleaseSourceData();
    write_detached_brep_jobs(jobs, wrapper, dotg, dry_run, process_map);
}


#ifdef HAVE_STEPCODE_LAZY
/* Construction geometry and bounded geometric sets are commonly attached to
 * a product through a representation relationship rather than its primary
 * SDR.  The lazy graph already resolves that ownership without materializing
 * both complete relationship sides.  Materialize only these product-owned
 * representations and route their supported items through the common exact
 * reference-geometry converters. */
static void
convert_lazy_auxiliary_geometry(const LazySTEPExactGraph &graph,
    STEPWrapper *wrapper, BRLCADWrapper *dotg, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &id2productid_map,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    if (!wrapper || !dotg)
	return;

    for (std::map<uint64_t, uint64_t>::const_iterator mapping =
	     graph.representation_product.begin();
	 mapping != graph.representation_product.end(); ++mapping) {
	if (wrapper->CancellationRequested()) return;
	if (mapping->first > static_cast<uint64_t>(INT_MAX) ||
		mapping->second > static_cast<uint64_t>(INT_MAX))
	    continue;
	const std::string representation_type =
	    wrapper->LazyTypeName(mapping->first);
	const bool constructive = representation_type ==
	    "CONSTRUCTIVE_GEOMETRY_REPRESENTATION";
	const bool geometric_set = graph.representation_geometric_sets.find(
	    mapping->first) != graph.representation_geometric_sets.end();
	if (!constructive && !geometric_set) continue;

	const int representation_id = static_cast<int>(mapping->first);
	const int product_id = static_cast<int>(mapping->second);
	if (product_id <= 0 || id2name_map[product_id].empty())
	    continue;

	wrapper->SetInstanceIds(std::vector<uint64_t>(1, mapping->first));
	SDAI_Application_instance *source = wrapper->InstanceAt(0);
	Representation *representation = source ?
	    dynamic_cast<Representation *>(Factory::CreateObject(wrapper, source)) : NULL;
	if (!representation) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		representation_id, representation_type,
		std::string(), "entity materialization failed");
	    wrapper->ClearEntityCache();
	    continue;
	}

	id2name_map[representation_id] = id2name_map[product_id];
	id2productid_map[representation_id] = product_id;
	brlcad::step::Representation &record =
	    wrapper->Document().representations[representation_id];
	record.entity_id = representation_id;
	record.product_id = product_id;
	record.type = representation_type;
	record.output_name = id2name_map[product_id];
	index_representation_geometry(representation, product_id, wrapper, dotg,
	    id2name_map, id2productid_map);
	convert_representation_geometry(representation, product_id, wrapper, dotg,
	    dry_run, id2name_map, process_map);
	wrapper->ClearEntityCache();
    }
}
#endif
