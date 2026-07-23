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
 * jobs, and product write-pipeline helpers.  Included only by STEPWrapper.cpp.
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
    Axis2Placement3D *origin = dynamic_cast<Axis2Placement3D *>(
	mapped->MappingSource()->MappingOrigin());
    if (!target || !target->LocalOrigin() || !origin)
	return false;

    vect_t xaxis, yaxis, zaxis;
    orthonormal_axes(target->Axis1(), target->Axis2(), target->Axis3(),
	xaxis, yaxis, zaxis);
    mat_t target_matrix;
    MAT_IDN(target_matrix);
    const double scale = target->Scale();
    VSCALE(&target_matrix[0], xaxis, scale);
    VSCALE(&target_matrix[4], yaxis, scale);
    VSCALE(&target_matrix[8], zaxis, scale);
    vect_t target_origin;
    VMOVE(target_origin, target->LocalOrigin()->Point3d());
    VSCALE(target_origin, target_origin, length_factor);
    MAT_DELTAS_VEC(target_matrix, target_origin);

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
    uint64_t relationship_id = 0;
    uint64_t parent_representation_id = 0;
    uint64_t child_representation_id = 0;
    uint64_t parent_product_id = 0;
    uint64_t child_product_id = 0;
    uint64_t parent_axis_id = 0;
    uint64_t child_axis_id = 0;
};


struct LazySTEPExactGraph {
    std::map<uint64_t, uint64_t> representation_product;
    std::map<uint64_t, std::vector<uint64_t> > representation_solids;
    std::map<uint64_t, std::vector<uint64_t> > representation_surface_models;
    std::map<uint64_t, std::vector<uint64_t> > representation_geometric_sets;
    std::map<uint64_t, uint64_t> representation_context;
    std::set<uint64_t> handled_representations;
    std::set<uint64_t> handled_sdrs;
    std::set<uint64_t> handled_relationships;
    std::set<uint64_t> handled_cdsrs;
    std::vector<LazySTEPOccurrence> occurrences;
};


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
    std::set<uint64_t> representations;
    std::set<uint64_t> contexts;
    std::set<uint64_t> exact_solids;
    std::set<uint64_t> surface_models;
    std::set<uint64_t> geometric_sets;
    std::set<uint64_t> axes;
    std::set<uint64_t> relationships;
    std::set<uint64_t> transforming_relationships;
    std::set<uint64_t> assembly_usages;

    lazy_add_type_ids(wrapper, products, {"PRODUCT"});
    lazy_add_type_ids(wrapper, formations, {"PRODUCT_DEFINITION_FORMATION",
	"PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE"});
    lazy_add_type_ids(wrapper, definitions, {"PRODUCT_DEFINITION",
	"PRODUCT_DEFINITION_WITH_ASSOCIATED_DOCUMENTS"});
    lazy_add_type_ids(wrapper, definition_shapes, {"PRODUCT_DEFINITION_SHAPE"});
    lazy_add_type_ids(wrapper, representations, {
	"SHAPE_REPRESENTATION", "ADVANCED_BREP_SHAPE_REPRESENTATION",
	"FACETED_BREP_SHAPE_REPRESENTATION", "MANIFOLD_SURFACE_SHAPE_REPRESENTATION",
	"GEOMETRICALLY_BOUNDED_SURFACE_SHAPE_REPRESENTATION",
	"GEOMETRICALLY_BOUNDED_WIREFRAME_SHAPE_REPRESENTATION",
	"CSG_SHAPE_REPRESENTATION"});
    lazy_add_type_ids(wrapper, contexts, {"REPRESENTATION_CONTEXT",
	"GEOMETRIC_REPRESENTATION_CONTEXT", "GLOBAL_UNIT_ASSIGNED_CONTEXT",
	"GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT", "PARAMETRIC_REPRESENTATION_CONTEXT"});
    lazy_add_type_ids(wrapper, exact_solids, {"MANIFOLD_SOLID_BREP",
	"BREP_WITH_VOIDS", "FACETED_BREP", "SOLID_REPLICA"});
    lazy_add_type_ids(wrapper, surface_models, {"SHELL_BASED_SURFACE_MODEL"});
    lazy_add_type_ids(wrapper, geometric_sets, {"GEOMETRIC_SET"});
    lazy_add_type_ids(wrapper, axes, {"AXIS2_PLACEMENT_2D", "AXIS2_PLACEMENT_3D"});
    lazy_add_type_ids(wrapper, relationships, {"SHAPE_REPRESENTATION_RELATIONSHIP",
	"REPRESENTATION_RELATIONSHIP", "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION"});
    lazy_add_type_ids(wrapper, transforming_relationships,
	{"REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION"});
    lazy_add_type_ids(wrapper, assembly_usages, {"NEXT_ASSEMBLY_USAGE_OCCURRENCE",
	"ASSEMBLY_COMPONENT_USAGE", "PRODUCT_DEFINITION_USAGE"});

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

    std::map<uint64_t, uint64_t> shape_product;
    for (std::set<uint64_t>::const_iterator shape = definition_shapes.begin();
	 shape != definition_shapes.end(); ++shape) {
	const uint64_t definition = lazy_reference_in_set(wrapper, *shape, definitions);
	std::map<uint64_t, uint64_t>::const_iterator product = definition_product.find(definition);
	if (product != definition_product.end()) shape_product[*shape] = product->second;
    }

    std::set<uint64_t> sdrs;
    lazy_add_type_ids(wrapper, sdrs, {"SHAPE_DEFINITION_REPRESENTATION"});
    std::map<uint64_t, uint64_t> sdr_representation;
    for (std::set<uint64_t>::const_iterator sdr = sdrs.begin(); sdr != sdrs.end(); ++sdr) {
	const uint64_t shape = lazy_reference_in_set(wrapper, *sdr, definition_shapes);
	const uint64_t representation = lazy_reference_in_set(wrapper, *sdr, representations);
	std::map<uint64_t, uint64_t>::const_iterator product = shape_product.find(shape);
	if (!representation || product == shape_product.end()) continue;
	sdr_representation[*sdr] = representation;
	graph.representation_product[representation] = product->second;
    }

    /* A non-transforming representation relationship joins alternate
     * representations of the same product.  A transforming relationship is
     * an occurrence edge and must never collapse the two products. */
    LazyDisjointSets components;
    for (std::set<uint64_t>::const_iterator representation = representations.begin();
	 representation != representations.end(); ++representation)
	components.add(*representation);
    for (std::set<uint64_t>::const_iterator relationship = relationships.begin();
	 relationship != relationships.end(); ++relationship) {
	if (transforming_relationships.find(*relationship) != transforming_relationships.end())
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
	bool has_lazy_geometry = false;
	bool lazy_safe = true;
	const std::vector<uint64_t> references = wrapper->LazyForwardReferences(*representation);
	for (std::vector<uint64_t>::const_iterator reference = references.begin();
	     reference != references.end(); ++reference) {
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
	    if (contexts.find(*reference) != contexts.end()) {
		/* context_of_items is singular; retain the first scanner-order hit. */
		if (!graph.representation_context[*representation])
		    graph.representation_context[*representation] = *reference;
		continue;
	    }
	    if (axes.find(*reference) != axes.end()) continue;
	    lazy_safe = false;
	}
	if (has_lazy_geometry && lazy_safe &&
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
	if (transforming_relationships.find(*relationship) != transforming_relationships.end())
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
    std::set<uint64_t> cdsrs;
    std::set<uint64_t> transformations;
    lazy_add_type_ids(wrapper, cdsrs, {"CONTEXT_DEPENDENT_SHAPE_REPRESENTATION"});
    lazy_add_type_ids(wrapper, transformations, {"ITEM_DEFINED_TRANSFORMATION"});
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
	const uint64_t rep1_product = graph.representation_product[relation_representations[0]];
	const uint64_t rep2_product = graph.representation_product[relation_representations[1]];
	LazySTEPOccurrence occurrence;
	occurrence.entity_id = *cdsr;
	occurrence.relationship_id = relationship;
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
	graph.handled_cdsrs.insert(*cdsr);
	graph.handled_relationships.insert(relationship);
    }
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
	const std::string &entity_type)
{
    brlcad::step::ImportStatistics &stats = wrapper->Statistics();
    ++stats.geometry_attempted;
    if (status == BREP_WRITE_SUCCESS) {
	++stats.geometry_written;
	return;
    }

    ++stats.geometry_skipped;
    std::string message;
    switch (status) {
	case BREP_CONVERSION_FAILED:
	    message = "exact OpenNURBS conversion failed";
	    break;
	case BREP_INVALID_STRUCTURE:
	    ++stats.invalid_breps;
	    message = "OpenNURBS structural validation failed";
	    break;
	case BREP_NOT_SOLID:
	    ++stats.invalid_breps;
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
    std::string validation_message;

    DetachedBrepJob()
    {
	MAT_IDN(write_matrix);
    }
};


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
	std::string *failure = NULL);
static size_t reconcile_pre_split_step_edge_uses(ON_Brep *brep,
	std::string *failure = NULL);


static std::unique_ptr<DetachedBrepJob>
detach_brep_job_data(int entity_id, const std::string &entity_type,
    const std::string &original_name, const std::string &output_name,
    int product_id, double length_factor, double planeangle_factor,
    double solidangle_factor, const fastf_t *write_matrix,
    const brlcad::step::Style *style)
{
    std::unique_ptr<DetachedBrepJob> job(new DetachedBrepJob());
    job->entity_id = entity_id;
    job->output_source_id = entity_id;
    job->product_id = product_id;
    job->entity_type = entity_type;
    job->original_name = original_name;
    job->output_name = output_name;
    job->length_factor = length_factor;
    job->planeangle_factor = planeangle_factor;
    job->solidangle_factor = solidangle_factor;
    job->tolerance = LocalUnits::tolerance;
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
    return detach_brep_job_data(solid->GetId(), exact_brep_solid_type(solid),
	solid->Name(), name, product_id,
	representation->GetLengthConversionFactor(),
	representation->GetPlaneAngleConversionFactor(),
	representation->GetSolidAngleConversionFactor(), matrix, style);
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
    const auto work_expired = [&job, &pullback_work, wrapper](
	const std::string &stage) {
	if (!pullback_work.DeadlineExpired() && !pullback_work.Stalled())
	    return false;
	job.validation_message = pullback_work.Stalled() ?
	    "exact conversion made no measurable progress for " +
	    std::to_string((wrapper->ImportOptions().effective_stall_timeout_milliseconds +
		999) / 1000) + " seconds during " + stage :
	    "exact pullback exceeded the " +
	    std::to_string((job.work_budget_milliseconds + 999) / 1000) +
	    "-second per-item work budget during " + stage;
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    job.entity_id, job.entity_type,
	    pullback_work.Stalled() ? "progress_stall" : "trim_pcurve",
	    job.validation_message);
	job.brep.reset();
	job.status = BREP_CONVERSION_FAILED;
	return true;
    };

    LocalUnits::length = job.length_factor;
    LocalUnits::planeangle = job.planeangle_factor;
    LocalUnits::solidangle = job.solidangle_factor;
    LocalUnits::tolerance = job.tolerance;

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

    job.entity_arena->ResetOpenNURBSState();
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
    finish_stage("brep_construction_pullback");
    if (work_expired("BREP construction"))
	return;
    if (!loaded) {
	job.brep.reset();
	job.status = BREP_CONVERSION_FAILED;
	return;
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
    if (!job.faceted) {
	wrapper->SetProgressDetail("repairing exact BREP trim orientations",
	    job.entity_id, 0, 0, std::string(), job.entity_type);
	repair_closed_trim_orientations(job.brep.get(), wrapper, job.entity_id,
	    job.entity_type);
	finish_stage("trim_orientation_repair");
	if (work_expired("trim orientation repair"))
	    return;
    }
    /* Exact pcurve repair may unwrap a periodic join or prove a duplicate
     * seam bridge that was not visible in the initial topology.  Do not touch
     * a BREP which is already valid; retry topology only for the remaining
     * structural failure, then repair any joins exposed by that normalization. */
    ON_wString preliminary_messages;
    ON_TextLog preliminary_log(preliminary_messages);
    wrapper->SetProgressDetail("prevalidating exact OpenNURBS structure",
	job.entity_id, 0, 0, std::string(), job.entity_type);
    const bool needs_post_repair_topology = !job.brep->IsValid(&preliminary_log);
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
    for (size_t split = 0; split < keyhole_splits; ++split)
	wrapper->RecordRepair(job.entity_id, job.entity_type, "edge_loop",
	    "split a zero-area keyhole bridge into closed trim loops");

    /* ON_Brep::IsValid() refreshes loop-derived state while diagnosing a
     * candidate.  Restore any unambiguous STEP bound classification after
     * the preliminary check and immediately before authoritative validation. */
    if (!job.faceted &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe)
	repair_face_bound_classification(job.brep.get(), wrapper, job.entity_id,
	    job.entity_type);

    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    wrapper->SetProgressDetail("validating exact OpenNURBS structure",
	job.entity_id, 0, 0, std::string(), job.entity_type);
    bool structurally_valid = job.brep->IsValid(&validation_log);
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
    if (!structurally_valid) {
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
	for (std::set<int>::const_iterator li = detailed_loops.begin();
		li != detailed_loops.end(); ++li) {
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
    bool solid = job.brep->IsSolid();
    if (work_expired("BREP solidness validation"))
	return;
    if (!solid && !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	std::string merged_edge_details;
	size_t merged_edge_repairs = 0;
	for (size_t attempt = 0; attempt < 16 && !solid; ++attempt) {
	    const size_t repaired = merge_one_exact_periodic_boundary_edge_pair(
		job.brep.get(), &merged_edge_details);
	    if (!repaired)
		break;
	    merged_edge_repairs += repaired;
	    solid = job.brep->IsSolid();
	}
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
	for (size_t attempt = 0; attempt < 16 && !solid; ++attempt) {
	    const size_t repaired = reconcile_pre_split_step_edge_uses(
		job.brep.get(), &split_use_failure);
	    if (!repaired)
		break;
	    split_use_repairs += repaired;
	    solid = job.brep->IsSolid();
	}
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
	const size_t reused_edge_repairs = split_reused_edges_by_shell_component(
	    job.brep.get(), &reused_edge_failure);
	for (size_t repair = 0; repair < reused_edge_repairs; ++repair)
	    wrapper->RecordRepair(job.entity_id, job.entity_type, "edge_topology",
		"separated one STEP edge reused by distinct manifold shell components");
	if (reused_edge_repairs)
	    solid = job.brep->IsSolid();
	if (wrapper->Verbose() && !reused_edge_failure.empty())
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": reused-edge shell separation retained unresolved input: "
		<< reused_edge_failure << std::endl;
	if (work_expired("reused STEP edge shell separation"))
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
 * curve.  Authoritative CLOSED_SHELL labels select the components; the
 * ordinary two-use adjacency graph is a fallback for older callers without
 * labels.  This is transactional and cannot guess a pairing: every use must
 * belong to a distinct, proven two-use component and the completed BREP must
 * remain structurally valid. */
static size_t
split_reused_edges_by_shell_component(ON_Brep *brep, std::string *failure)
{
    if (failure)
	failure->clear();
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
	std::map<int, std::vector<int> > by_component;
	bool complete = true;
	for (int use = 0; use < edge.m_ti.Count(); ++use) {
	    const ON_BrepTrim *trim = brep->Trim(edge.m_ti[use]);
	    const ON_BrepFace *face = trim ? trim->Face() : NULL;
	    if (!trim || !face || face->m_face_index < 0 ||
		    face->m_face_index >= brep->m_F.Count()) {
		complete = false;
		break;
	    }
	    const int shell_component = face->m_face_user.i > 0 ?
		face->m_face_user.i : -(find_root(face->m_face_index) + 1);
	    by_component[shell_component].push_back(
		trim->m_trim_index);
	}
	if (!complete || by_component.size() < 2) {
	    if (failure && failure->empty()) {
		std::ostringstream reason;
		reason << "STEP edge " << edge.m_edge_user.i << " has "
		    << edge.m_ti.Count()
		    << " uses which were not separated by authoritative shell "
		       "membership or ordinary face adjacency";
		*failure = reason.str();
	    }
	    continue;
	}
	SplitPlan plan;
	plan.edge_index = ei;
	for (std::map<int, std::vector<int> >::const_iterator group =
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
	    reason << "STEP edge " << edge.m_edge_user.i
		<< " did not have exactly two uses in every shell component";
	    *failure = reason.str();
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

    /* First handle the simplest locally provable inconsistency: every
     * reciprocal edge used by one complete loop is currently in conflict.
     * Reversing that loop cannot break an already-correct reciprocal use.
     * Keep the operation transactional and require structural validity,
     * closed manifold topology, and a strict reduction in the complete-shell
     * conflict count.  This covers both ordinary singleton closed boundaries
     * and complete periodic loops without assuming a particular STEP edge id
     * or surface family. */
    bool local_progress = true;
    while (local_progress) {
	local_progress = false;
	const size_t before = shared_edge_orientation_conflicts(brep);
	for (int li = 0; li < brep->m_L.Count(); ++li) {
	    const ON_BrepLoop &candidate = brep->m_L[li];
	    bool has_reciprocal_conflict = false;
	    bool all_reciprocal_uses_conflict = true;
	    std::set<int> inspected_edges;
	    for (int lti = 0; lti < candidate.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = candidate.Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (!trim || !edge || edge->m_ti.Count() != 2 ||
			!inspected_edges.insert(edge->m_edge_index).second)
		    continue;
		const ON_BrepTrim *first = brep->Trim(edge->m_ti[0]);
		const ON_BrepTrim *second = brep->Trim(edge->m_ti[1]);
		const ON_BrepFace *first_face = first ? first->Face() : NULL;
		const ON_BrepFace *second_face = second ? second->Face() : NULL;
		if (!first || !second || !first_face || !second_face ||
			first_face == second_face)
		    continue;
		const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
		const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
		if (first_effective != second_effective) {
		    all_reciprocal_uses_conflict = false;
		    break;
		}
		has_reciprocal_conflict = true;
	    }
	    if (!has_reciprocal_conflict || !all_reciprocal_uses_conflict)
		continue;

	    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	    const ON_BrepLoop::TYPE declared_type = brep->m_L[li].m_type;
	    const int face_index = brep->m_L[li].m_fi;
	    brep->FlipLoop(brep->m_L[li]);
	    refresh_brep_flags_preserving_singular_isos(brep, false);
	    brep->m_L[li].m_type = declared_type;
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
		++repaired;
		local_progress = true;
		break;
	    }
	    *brep = *rollback;
	}
    }

    if (repaired) {
	oriented = false;
	has_boundary = false;
	if (brep->IsManifold(&oriented, &has_boundary) && oriented &&
		!has_boundary)
	    return repaired;
    }

    /* A periodic face band can contain two physical boundary loops joined by
     * artificial OpenNURBS seams.  Reversing only one source-edge subchain
     * makes both physical boundaries traverse the same parameter direction
     * and necessarily opens the UV rectangle.  Solve the reciprocal edge-use
     * constraints over complete loops instead.  This can propagate the
     * correction across neighboring bands while FlipLoop preserves every
     * pcurve locus and join.  The complete operation is transactional: accept
     * it only when OpenNURBS confirms structural validity and a closed,
     * oriented manifold shell. */
    struct LoopConstraint {
	int other;
	int parity;
	int edge;
    };
    std::vector<std::vector<LoopConstraint> > loop_graph(
	static_cast<size_t>(brep->m_L.Count()));
    bool loop_graph_valid = true;
    for (int ei = 0; loop_graph_valid && ei < brep->m_E.Count(); ++ei) {
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
	if (first->m_li < 0 || first->m_li >= brep->m_L.Count() ||
		second->m_li < 0 || second->m_li >= brep->m_L.Count() ||
		first->m_li == second->m_li) {
	    loop_graph_valid = false;
	    break;
	}
	const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
	const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
	const int parity = 1 ^ static_cast<int>(first_effective) ^
	    static_cast<int>(second_effective);
	loop_graph[static_cast<size_t>(first->m_li)].push_back(
	    {second->m_li, parity, ei});
	loop_graph[static_cast<size_t>(second->m_li)].push_back(
	    {first->m_li, parity, ei});
    }

    std::vector<int> loop_flip(static_cast<size_t>(brep->m_L.Count()), -1);
    std::string loop_graph_failure;
    if (loop_graph_valid) {
	for (int seed = 0; seed < brep->m_L.Count() && loop_graph_valid; ++seed) {
	    if (loop_flip[static_cast<size_t>(seed)] >= 0 ||
		    loop_graph[static_cast<size_t>(seed)].empty())
		continue;
	    std::vector<int> component;
	    std::vector<int> pending(1, seed);
	    loop_flip[static_cast<size_t>(seed)] = 0;
	    while (!pending.empty() && loop_graph_valid) {
		const int loop_index = pending.back();
		pending.pop_back();
		component.push_back(loop_index);
		const std::vector<LoopConstraint> &constraints =
		    loop_graph[static_cast<size_t>(loop_index)];
		for (std::vector<LoopConstraint>::const_iterator constraint =
			constraints.begin(); constraint != constraints.end();
			++constraint) {
		    const int required = loop_flip[static_cast<size_t>(loop_index)] ^
			constraint->parity;
		    int &assigned = loop_flip[static_cast<size_t>(constraint->other)];
		    if (assigned < 0) {
			assigned = required;
			pending.push_back(constraint->other);
		    } else if (assigned != required) {
			const ON_BrepEdge *conflict = brep->Edge(constraint->edge);
			std::ostringstream reason;
			reason << "loop parity conflict at STEP edge "
			    << (conflict ? conflict->m_edge_user.i : 0)
			    << " between L" << loop_index << " and L"
			    << constraint->other;
			loop_graph_failure = reason.str();
			loop_graph_valid = false;
			break;
		    }
		}
	    }
	    size_t flipped = 0;
	    for (std::vector<int>::const_iterator loop_index = component.begin();
		    loop_index != component.end(); ++loop_index)
		flipped += loop_flip[static_cast<size_t>(*loop_index)] != 0;
	    if (flipped > component.size() - flipped)
		for (std::vector<int>::const_iterator loop_index = component.begin();
			loop_index != component.end(); ++loop_index)
		    loop_flip[static_cast<size_t>(*loop_index)] ^= 1;
	}
    }

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
	    const bool valid = brep->IsValid(&validation_log) &&
		brep->IsManifold(&trial_oriented, &trial_boundary) &&
		trial_oriented && !trial_boundary && after < before;
	    if (valid)
		return repaired + loop_flip_count;

	    if (failure) {
		ON_String text(validation_messages);
		std::ostringstream reason;
		reason << "complete loop-parity trial changed shell orientation "
		    << "conflicts " << before << "->" << after
		    << " but failed structural/oriented-manifold validation";
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
    if (job.status == BREP_INVALID_STRUCTURE && !job.validation_message.empty())
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
	    job.entity_id, job.entity_type, "openNURBS",
	    job.validation_message.substr(0, 4096));
    record_brep_result(wrapper, job.status, job.entity_id, job.entity_type);
    if (job.status != BREP_WRITE_SUCCESS)
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
    if (job.status != BREP_WRITE_SUCCESS)
	return false;
    if (dry_run)
	return true;
    const brlcad::step::Style *style = job.has_style ? &job.style : NULL;
    if (job.faceted) {
	return database->WriteBot(job.output_name,
	    job.bot_vertices.size() / 3, job.bot_faces.size() / 3,
	    job.bot_vertices.data(), job.bot_faces.data(), job.write_matrix,
	    job.output_source_id, job.original_name, style);
    }
    return database->WriteBrep(job.output_name, job.brep.get(),
	job.write_matrix, job.is_region, job.output_source_id, job.original_name, style);
}


static void
commit_spooled_brep_job(DetachedBrepJob &job, STEPWrapper *wrapper,
    BRLCADWrapper *spool, BRLCADWrapper *dotg, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    if (job.status == BREP_WRITE_SUCCESS && !dry_run) {
	const bool solid_copied = dotg->CopyObjectFrom(*spool,
	    job.output_name + ".s");
	const bool region_copied = solid_copied &&
	    dotg->CopyObjectFrom(*spool, job.output_name);
	if (!region_copied)
	    job.status = BREP_OUTPUT_FAILED;
    }
    if (job.status == BREP_WRITE_SUCCESS && job.has_style)
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
	    const bool exclusive_pullback =
		jobs[index]->maximum_pullback_span_estimate >=
		kExclusivePullbackSurfaceSpanThreshold;
	    wrapper->GeometryWorkerStarted(jobs[index]->entity_id,
		jobs[index]->entity_type, exclusive_pullback);
	    if (exclusive_pullback)
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Information,
		    jobs[index]->entity_id, jobs[index]->entity_type,
		    "scheduler", "reserved nested geometry workers for a surface "
		    "with an estimated " +
		    std::to_string(jobs[index]->maximum_pullback_span_estimate) +
		    " tensor-product spans");
	    try {
		convert_detached_brep_job(*jobs[index], wrapper);
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
	    if (job.status == BREP_WRITE_SUCCESS &&
		!write_detached_brep_geometry(job, &spool, dry_run))
		job.status = BREP_OUTPUT_FAILED;
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
};


static LazyRepresentationUnits
lazy_representation_units(STEPWrapper *wrapper, const LazySTEPExactGraph &graph,
    uint64_t representation_id)
{
    LazyRepresentationUnits result;
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
    wrapper->ReleaseSourceData();
    return result;
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
	if (!lazy_occurrence_matrix(wrapper, *source, units.length, matrix)) {
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		occurrence_id, "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION",
		"representation_relation", "could not materialize the indexed occurrence transform");
	    continue;
	}
	brlcad::step::Occurrence &occurrence = wrapper->Document().occurrences[occurrence_id];
	occurrence.entity_id = occurrence_id;
	occurrence.parent_product_id = parent_product;
	occurrence.child_product_id = child_product;
	for (size_t i = 0; i < 16; ++i) occurrence.matrix[i] = matrix[i];
	if (!dry_run && !database->AddMember(id2name_map[parent_product],
		id2name_map[child_product], matrix))
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		occurrence_id, "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION",
		std::string(), "could not add the occurrence to its parent product");
    }
}


#endif

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
    }
}


#ifdef HAVE_STEPCODE_LAZY
static uint64_t
lazy_indexed_surface_geometry_job_count(STEPWrapper *wrapper)
{
    if (!wrapper || !wrapper->HasLazyIndex()) return 0;
    const std::set<int64_t> &selected =
	wrapper->ImportOptions().selected_entity_ids;
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
	for (std::vector<uint64_t>::const_iterator boundary =
		boundaries.begin(); boundary != boundaries.end(); ++boundary) {
	    const std::string type = wrapper->LazyTypeName(*boundary);
	    if (*boundary <= static_cast<uint64_t>(INT_MAX) &&
		    (type == "CLOSED_SHELL" || type == "OPEN_SHELL") &&
		    (selected_model || selected.find(
			static_cast<int64_t>(*boundary)) != selected.end()))
		++model_jobs;
	}
	/* A selected malformed model with no shell boundary is still one unit of
	 * conversion work: it is attempted, diagnosed, and skipped below. */
	count += model_jobs ? model_jobs : 1;
    }
    return count;
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
	if (!lazy_representation_matrix(wrapper, represented->first, units.length,
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
		    units.solidangle, identity, style);
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
	    units.length, units.planeangle, units.solidangle, identity, style);
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
	    units.solidangle, identity, style);
	job->is_region = source_type == "CLOSED_SHELL";
	wrapper->ShouldConvertEntity(entity_id);
	jobs.push_back(std::move(job));
    }
    wrapper->ReleaseSourceData();
    write_detached_brep_jobs(jobs, wrapper, database, dry_run, process_map);
}
#endif

static BrepWriteStatus
convert_WriteBSpline(
	BSplineSurfaceWithKnots *sB,
	STEPWrapper *wrapper,
	BRLCADWrapper *dot_g,
	std::string *name,
	int dry_run)
{
    wrapper->ResetOpenNURBSState();
    ON_Brep *onBrep = sB->GetONBrep();
    if (!onBrep)
	return BREP_CONVERSION_FAILED;

    ON_wString validation_messages;
    ON_TextLog tl(validation_messages);
    const size_t keyhole_splits = finalize_brep_topology(onBrep,
	wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe,
	wrapper, sB->GetId(), "B_SPLINE_SURFACE_WITH_KNOTS");
    for (size_t split = 0; split < keyhole_splits; ++split)
	wrapper->RecordRepair(sB->GetId(), "B_SPLINE_SURFACE_WITH_KNOTS", "edge_loop",
	    "split a zero-area keyhole bridge into closed trim loops");
    repair_closed_trim_orientations(onBrep, wrapper, sB->GetId(),
	"B_SPLINE_SURFACE_WITH_KNOTS");
    if (!onBrep->IsValid(&tl)) {
	delete onBrep;
	return BREP_INVALID_STRUCTURE;
    }

	mat_t mat;
	MAT_IDN(mat);

#if 0
	// TODO - manifold surface container has an axis...
	Axis2Placement3D *axis = aBrep->GetAxis2Placement3d();
	if (axis != NULL) {
	    //assign matrix values
	    double translate_to[3];
	    const double *toXaxis = axis->GetXAxis();
	    const double *toYaxis = axis->GetYAxis();
	    const double *toZaxis = axis->GetZAxis();
	    mat_t rot_mat;

	    VMOVE(translate_to,axis->GetOrigin());
	    VSCALE(translate_to,translate_to,LocalUnits::length);

	    MAT_IDN(rot_mat);
	    VMOVE(&rot_mat[0], toXaxis);
	    VMOVE(&rot_mat[4], toYaxis);
	    VMOVE(&rot_mat[8], toZaxis);
	    bn_mat_inv(mat, rot_mat);
	    MAT_DELTAS_VEC(mat, translate_to);
	}
#endif

    /* A geometrically bounded surface has no asserted solid semantics. */
    const brlcad::step::Style *style = style_for_item(wrapper, sB->GetId());
    const bool written = dry_run || dot_g->WriteBrep(*name, onBrep, mat, false,
	sB->GetId(), sB->Name(), style);
    if (written && style)
	++wrapper->Statistics().styles_applied;
    delete onBrep;
    return written ? BREP_WRITE_SUCCESS : BREP_OUTPUT_FAILED;
}


static bool
geometric_set_has_curves(GeometricSet *set)
{
    if (!set || !set->GetElements())
	return false;
    for (LIST_OF_GEOMETRIC_SET_SELECT::const_iterator item = set->GetElements()->begin();
	 item != set->GetElements()->end(); ++item) {
	if (*item && (*item)->GetCurveElement())
	    return true;
    }
    return false;
}


static BrepWriteStatus
convert_WriteWireSet(GeometricSet *set, Representation *representation,
    STEPWrapper *wrapper, BRLCADWrapper *dotg, std::string *name, int dry_run,
    const brlcad::step::Style *style_override = NULL)
{
    if (!set || !representation || !wrapper || !dotg || !name)
	return BREP_CONVERSION_FAILED;

    LocalUnits::length = representation->GetLengthConversionFactor();
    LocalUnits::planeangle = representation->GetPlaneAngleConversionFactor();
    LocalUnits::solidangle = representation->GetSolidAngleConversionFactor();

    wrapper->ResetOpenNURBSState();
    ON_Brep *wire = new ON_Brep();
    size_t curve_count = 0;
    LIST_OF_GEOMETRIC_SET_SELECT *elements = set->GetElements();
    if (elements) {
	for (LIST_OF_GEOMETRIC_SET_SELECT::iterator element = elements->begin();
	     element != elements->end(); ++element) {
	    Curve *curve = *element ? (*element)->GetCurveElement() : NULL;
	    if (!curve)
		continue;
	    if (!curve->LoadONBrep(wire)) {
		delete wire;
		return BREP_CONVERSION_FAILED;
	    }
	    const int curve_index = curve->GetONId();
	    if (curve_index < 0 || curve_index >= wire->m_C3.Count() || !wire->m_C3[curve_index]) {
		delete wire;
		return BREP_CONVERSION_FAILED;
	    }
	    ON_Curve *geometry = wire->m_C3[curve_index];
	    const ON_Interval domain = geometry->Domain();
	    const ON_3dPoint start = geometry->PointAt(domain.Min());
	    const ON_3dPoint end = geometry->PointAt(domain.Max());
	    const int start_vertex = wire->NewVertex(start, LocalUnits::tolerance).m_vertex_index;
	    int end_vertex = start_vertex;
	    if (start.DistanceTo(end) > LocalUnits::tolerance)
		end_vertex = wire->NewVertex(end, LocalUnits::tolerance).m_vertex_index;
	    ON_BrepEdge &edge = wire->NewEdge(wire->m_V[start_vertex],
		wire->m_V[end_vertex], curve_index);
	    edge.m_tolerance = LocalUnits::tolerance;
	    ++curve_count;
	}
    }
    if (!curve_count) {
	delete wire;
	return BREP_CONVERSION_FAILED;
    }

    /* Curve replicas and composite curves may materialize dependency curves
     * before adding their final exact curve.  Only representation-item curves
     * receive edges; discard the unreferenced dependency copies. */
    if (!wire->Compact()) {
	delete wire;
	return BREP_CONVERSION_FAILED;
    }

    wire->SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, true, true);
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!wire->IsValid(&validation_log)) {
	if (wrapper->Verbose()) {
	    ON_String validation_text(validation_messages);
	    std::cerr << validation_text.Array();
	}
	delete wire;
	return BREP_INVALID_STRUCTURE;
    }

    mat_t mat;
    representation_matrix(representation, mat);
    const brlcad::step::Style *style = style_override ? style_override : style_for_item(wrapper, set->GetId());
    if (!style)
	style = style_for_item(wrapper, representation->GetId());
    const bool written = dry_run || dotg->WriteBrep(*name, wire, mat, false,
	set->GetId(), set->Name(), style);
    if (written && style)
	++wrapper->Statistics().styles_applied;
    delete wire;
    return written ? BREP_WRITE_SUCCESS : BREP_OUTPUT_FAILED;
}


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
	if (!solid && !mapped)
	    continue;
	const int item_id = item->GetId();
	if (id2name_map[item_id].empty())
	    id2name_map[item_id] = dotg->StableBRLCADName(product_name + "_item", item_id);
	id2productid_map[item_id] = product_id;
	brlcad::step::Representation &item_record = wrapper->Document().representations[item_id];
	item_record.entity_id = item_id;
	item_record.product_id = product_id;
	item_record.type = mapped ? "MAPPED_ITEM" : exact_brep_solid_type(solid);
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
