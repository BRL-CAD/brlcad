/*                 S T E P I M P O R T P I P E L I N E . C P P
 * BRL-CAD
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
/** @file step/STEPImportPipeline.cpp
 *
 * Schema-neutral STEP document traversal and geometry conversion pipeline.
 *
 */

#include "common.h"
#include <atomic>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <exception>
#include <iomanip>

#include "brep/cdt.h"
#include "brep/pullback.h"

#include <iostream>
#include <fstream>
#include <limits>
#include <sstream>
#include <set>
#include <thread>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/process.h"
#include "bu/units.h"

/* interface header */
#include "./STEPWrapper.h"
#include "STEPBudget.h"
#include "STEPBrepRepairInternal.h"
#include "STEPBrepValidation.h"
#include "STEPConversionStatus.h"
#include "STEPGeometricSet.h"
#include "STEPImportInternal.h"
#include "StepSchemaRuntime.h"
#include "Factory.h"
#include "step-g/OpenNurbsInterfaces.h"

/* implementation headers */
#include "AdvancedBrepShapeRepresentation.h"
#include "BrepWithVoids.h"
#include "BSplineSurfaceWithKnots.h"
#include "BoundedSurface.h"
#include "GeometricallyBoundedSurfaceShapeRepresentation.h"
#include "GeometricallyBoundedWireframeShapeRepresentation.h"
#include "GeometricSet.h"
#include "GeometricSetSelect.h"
#include "Axis2Placement3D.h"
#include "CartesianTransformationOperator3D.h"
#include "CartesianPoint.h"
#include "CompoundRepresentationItem.h"
#include "ConnectedFaceSet.h"
#include "Curve.h"
#include "CurveBoundedSurface.h"
#include "BoundaryCurve.h"
#include "CompositeCurve.h"
#include "CompositeCurveSegment.h"
#include "SurfaceCurve.h"
#include "PCurve.h"
#include "PCurveOrSurface.h"
#include "DefinitionalRepresentation.h"
#include "Direction.h"
#include "EdgeCurve.h"
#include "SurfacePatch.h"
#include "LocalUnits.h"
#include "FacetedBrep.h"
#include "Face.h"
#include "FaceSurface.h"
#include "MappedItem.h"
#include "ManifoldSolidBrep.h"
#include "ManifoldSurfaceShapeRepresentation.h"
#include "Plane.h"
#include "ContextDependentShapeRepresentation.h"
#include "Product.h"
#include "ProductDefinition.h"
#include "ProductDefinitionFormation.h"
#include "ShapeDefinitionRepresentation.h"
#include "ShapeRepresentationRelationship.h"
#include "RepresentationMap.h"
#include "ShellBasedSurfaceModel.h"
#include "SolidReplica.h"
#include "GlobalUncertaintyAssignedContext.h"
#include "GlobalUnitAssignedContext.h"
#include "STEPEntity.h"
#include "STEPString.h"
#ifdef HAVE_STEPCODE_LAZY
#  include "STEPLazySession.h"
#endif


using namespace step_import_detail;
using namespace step_brep_detail;

#include "STEPWrapperDocument.inc"


bool STEPWrapper::convert(BRLCADWrapper *dot_g)
{
#ifdef HAVE_STEPCODE_LAZY
    struct LazyBatchCleanup {
	STEPWrapper *wrapper;
	~LazyBatchCleanup()
	{
	    if (wrapper) wrapper->ReleaseSourceData();
	}
    } lazy_batch_cleanup = {this};
#endif
    MAP_OF_PRODUCT_NAME_TO_ENTITY_ID name2id_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME id2name_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID id2productid_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID process_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID shell2representation_map;

    if (!dot_g) {
	return false;
    }

    this->dotg = dot_g;


    geometry_results_recorded.clear();
    statistics.geometry_attempted = 0;
    statistics.geometry_written = 0;
    statistics.geometry_skipped = 0;
    statistics.geometry_filtered = 0;
    statistics.outcome.clear();
    statistics.skipped_items.clear();
    statistics.skipped_items_omitted = 0;
    statistics.invalid_breps = 0;
    statistics.invalid_breps_written = 0;
    statistics.invalid_breps_rejected = 0;
    statistics.output_failures = 0;
    statistics.styles_extracted = 0;
    statistics.styles_applied = 0;
    statistics.layers_extracted = 0;
    statistics.pmi_semantic_records = 0;
    statistics.pmi_presentation_records = 0;
    statistics.pmi_association_records = 0;
    statistics.pmi_dependency_records = 0;
    statistics.pmi_native_datums = 0;
    statistics.pmi_native_annotations = 0;
    statistics.pmi_invalid_records = 0;
    for (std::map<int64_t, brlcad::step::Product>::iterator product =
	    document.products.begin(); product != document.products.end(); ++product)
	product->second.invalid_geometry_count = 0;
    statistics.selected_entity_ids_encountered.clear();
    statistics.stage_timings.clear();
    statistics.slow_item_timings.clear();
    statistics.slow_item_timings_omitted = 0;
    statistics.pullback_closest_point_queries = 0;
    statistics.pullback_surfaces_prepared = 0;
    statistics.pullback_surface_cache_hits = 0;
    statistics.pullback_span_boxes_built = 0;
    statistics.pullback_span_boxes_tested = 0;
    statistics.pullback_primary_search_successes = 0;
    statistics.pullback_continuity_seed_searches = 0;
    statistics.pullback_continuity_seed_successes = 0;
    statistics.pullback_continuity_seed_failures = 0;
    statistics.pullback_continuity_seed_finite_candidates = 0;
    statistics.pullback_continuity_seed_iterations = 0;
    statistics.pullback_continuity_seed_line_searches = 0;
    statistics.pullback_maximum_continuity_seed_iterations = 0;
    statistics.pullback_maximum_continuity_seed_line_searches = 0;
    statistics.pullback_multiseed_fallbacks = 0;
    statistics.pullback_multiseed_successes = 0;
    statistics.pullback_multiseed_failures = 0;
    statistics.pullback_fallback_calls_with_finite_primary = 0;
    statistics.pullback_fallback_samples_evaluated = 0;
    statistics.pullback_fallback_seed_refinements = 0;
    statistics.pullback_fallback_refinement_improvements = 0;
    statistics.pullback_fallback_late_seed_improvements = 0;
    statistics.pullback_maximum_winning_seed_index = 0;
    statistics.pullback_subdivision_nodes = 0;
    statistics.pullback_maximum_subdivision_nodes = 0;
    statistics.pullback_preparation_us = 0;
    statistics.pullback_primary_search_us = 0;
    statistics.pullback_continuity_seed_us = 0;
    statistics.pullback_multiseed_us = 0;
    statistics.pullback_fallback_primary_improvement_total = 0.0;
    statistics.pullback_fallback_primary_improvement_maximum = 0.0;
    statistics.pullback_fallback_refinement_improvement_total = 0.0;
    statistics.pullback_fallback_refinement_improvement_maximum = 0.0;
    document.products.clear();
    document.representations.clear();
    document.representation_contexts.clear();
    document.representation_coverage.clear();
    document.occurrences.clear();
    document.assembly_usages.clear();
    document.product_alternatives.clear();
    document.usage_substitutes.clear();
    document.configuration_records.clear();
    document.pmi_records.clear();
    document.global_attributes.clear();
    {
	std::lock_guard<std::mutex> guard(progress_mutex);
	progress_state = brlcad::step::ImportProgress();
	active_geometry_job_progress.clear();
    }
    SetProgress("calibrating exact-geometry work budgets");
    configureImportBudgets();
    const std::chrono::steady_clock::time_point schema_preprocess_started =
	std::chrono::steady_clock::now();
    brlcad::step::CurrentStepSchemaRuntime().Preprocess(*this);
    RecordStageTiming("schema_preprocess", 0, "FILE_SCHEMA",
	static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
	    std::chrono::steady_clock::now() - schema_preprocess_started).count()));
    statistics.tolerance_mm = deriveTolerance();
    LocalUnits::tolerance = statistics.tolerance_mm;
    LocalUnits::representation_tolerance = statistics.tolerance_mm;
#ifdef HAVE_STEPCODE_LAZY
    SetProgress("indexing lazy STEP document graph");
    const std::chrono::steady_clock::time_point lazy_graph_started =
	std::chrono::steady_clock::now();
    const LazySTEPExactGraph lazy_exact_graph = build_lazy_exact_graph(this);
    RecordStageTiming("lazy_document_graph_index", 0, "FILE_SCHEMA",
	static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
	    std::chrono::steady_clock::now() - lazy_graph_started).count()));
    const std::vector<uint64_t> lazy_handled_sdrs = lazy_ids(lazy_exact_graph.handled_sdrs);
    const std::vector<uint64_t> lazy_handled_relationships =
	lazy_ids(lazy_exact_graph.handled_relationships);
    const std::vector<uint64_t> lazy_handled_cdsrs = lazy_ids(lazy_exact_graph.handled_cdsrs);
    std::set<uint64_t> lazy_handled_structure_set = lazy_exact_graph.handled_sdrs;
    lazy_handled_structure_set.insert(lazy_exact_graph.handled_representations.begin(),
	lazy_exact_graph.handled_representations.end());
    const std::vector<uint64_t> lazy_handled_structure =
	lazy_ids(lazy_handled_structure_set);

    /* A focused exact-root request is a diagnostic/conversion job, not a
     * request to rebuild every unrelated product and relationship in a large
     * assembly.  The zero-copy graph above supplies its units; load only the
     * selected dependency closures and write the roots in entity order. */
    if (lazy_selection_contains_only_exact_roots(this)) {
	convert_lazy_selected_exact_roots(lazy_exact_graph, this, dotg, dry_run,
	    process_map);
	FinalizeRepresentationCoverage();
	statistics.products = static_cast<uint64_t>(document.products.size());
	statistics.occurrences = static_cast<uint64_t>(document.occurrences.size());
	return statistics.output_failures == 0 &&
	    (!import_options.strict ||
	     (statistics.geometry_skipped == 0 && statistics.properties_invalid == 0 &&
	      statistics.pmi_invalid_records == 0));
    }
	if (lazy_selection_contains_only_topology_roots(this)) {
	convert_lazy_selected_topology_roots(lazy_exact_graph, this, dotg, dry_run,
	    process_map);
	FinalizeRepresentationCoverage();
	statistics.products = static_cast<uint64_t>(document.products.size());
	statistics.occurrences = static_cast<uint64_t>(document.occurrences.size());
	return statistics.output_failures == 0 &&
	    (!import_options.strict ||
	     (statistics.geometry_skipped == 0 && statistics.properties_invalid == 0 &&
	      statistics.pmi_invalid_records == 0));
    }
#else
    const std::vector<uint64_t> lazy_handled_sdrs;
    const std::vector<uint64_t> lazy_handled_relationships;
    const std::vector<uint64_t> lazy_handled_cdsrs;
    const std::vector<uint64_t> lazy_handled_structure;
#endif

    std::vector<std::string> product_structure_types = {"PRODUCT",
	"PRODUCT_DEFINITION_FORMATION",
	"PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE", "PRODUCT_DEFINITION",
	"PRODUCT_DEFINITION_WITH_ASSOCIATED_DOCUMENTS", "SHAPE_DEFINITION_REPRESENTATION",
	"PROPERTY_DEFINITION_REPRESENTATION",
	"MANIFOLD_SURFACE_SHAPE_REPRESENTATION"};
    /* The lazy graph supplies geometric-set identity and ownership without
     * loading every member curve or surface.  Eager parsing still needs the
     * legacy naming pass. */
    if (!HasLazyIndex()) product_structure_types.push_back("GEOMETRIC_SET");
    SetInstanceTypes(product_structure_types, lazy_handled_structure);
    int num_ents = InstanceCount();
    SetProgress("indexing product and representation structure", 0,
	static_cast<uint64_t>(num_ents));
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	if (i % kProgressUpdateStride == 0)
	    SetProgress("indexing product and representation structure",
		static_cast<uint64_t>(i), static_cast<uint64_t>(num_ents));
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "PRODUCT"))) {
	    Product *source_product = dynamic_cast<Product *>(Factory::CreateObject(
		this, (SDAI_Application_instance *)sse));
	    if (source_product) {
		brlcad::step::Product &product = document.products[source_product->GetId()];
		product.entity_id = source_product->GetId();
		product.identifier = brlcad::step::decode_string(source_product->Ident());
		product.original_name = brlcad::step::decode_string(source_product->Name());
		product.description = brlcad::step::decode_string(source_product->Description());
	    }
	    ClearEntityCache();
	}

	if ((sse->STEPfile_id > 0) &&
	    (IsSchemaEntity(sse, "PRODUCT_DEFINITION_FORMATION"))) {
	    ProductDefinitionFormation *formation = dynamic_cast<ProductDefinitionFormation *>(
		Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (formation && formation->GetProductId() > 0) {
		brlcad::step::Product &product = document.products[formation->GetProductId()];
		product.entity_id = formation->GetProductId();
		if (std::find(product.formation_entity_ids.begin(),
			product.formation_entity_ids.end(), sse->STEPfile_id) ==
			product.formation_entity_ids.end())
		    product.formation_entity_ids.push_back(sse->STEPfile_id);
		product.revision = brlcad::step::decode_string(formation->Ident());
		product.revision_description = brlcad::step::decode_string(formation->Description());
	    }
	    ClearEntityCache();
	}

	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "PRODUCT_DEFINITION"))) {
	    ProductDefinition *definition = dynamic_cast<ProductDefinition *>(Factory::CreateObject(
		this, (SDAI_Application_instance *)sse));
	    if (definition && definition->GetProductId() > 0) {
		brlcad::step::Product &product = document.products[definition->GetProductId()];
		product.entity_id = definition->GetProductId();
		if (std::find(product.definition_entity_ids.begin(),
			product.definition_entity_ids.end(), sse->STEPfile_id) ==
			product.definition_entity_ids.end())
		    product.definition_entity_ids.push_back(sse->STEPfile_id);
		product.definition_identifier = brlcad::step::decode_string(definition->Ident());
		product.definition_description = brlcad::step::decode_string(definition->Description());
	    }
	    ClearEntityCache();
	}

	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "PROPERTY_DEFINITION_REPRESENTATION"))) {
	    /* PROPERTY_DEFINITION_REPRESENTATION is also the generic link used
	     * for PMI, validation properties, and other non-geometric metadata.
	     * The legacy wrapper class is geometry-specific and recursively asks
	     * Factory to materialize representation items, so invoking it for a
	     * general REPRESENTATION both emits spurious "not mapped" messages
	     * and turns intentionally unsupported metadata into an import error. */
	    SDAI_Application_instance *used_representation =
		getEntityAttribute(sse, "used_representation");
	    if (!IsSchemaEntity(used_representation, "SHAPE_REPRESENTATION")) {
		ClearEntityCache();
		continue;
	    }
	    PropertyDefinitionRepresentation *sdr = dynamic_cast<PropertyDefinitionRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));

	    if (!sdr) {
		RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, sse->STEPfile_id,
		    "PROPERTY_DEFINITION_REPRESENTATION", std::string(), "entity materialization failed");
		ClearEntityCache();
		continue;
	    } else {
		int sdr_id = sdr->GetId();
		std::string original_name = sdr->GetProductName();
		int product_id = sdr->GetProductId();
		/* AP214 also uses SHAPE_DEFINITION_REPRESENTATION for shape
		 * aspects such as PMI anchors.  They do not define products and
		 * must not create a synthetic product with entity id zero. */
		if (product_id <= 0) {
		    ClearEntityCache();
		    continue;
		}
		std::string decoded_name = brlcad::step::decode_string(original_name);
		std::string pname = dotg->StableBRLCADName(
		    decoded_name.empty() ? std::string("Product_step") + std::to_string(product_id) : original_name,
		    product_id);

		brlcad::step::Product &product = document.products[product_id];
		product.entity_id = product_id;
		product.original_name = decoded_name;
		product.output_name = pname;

		id2productid_map[sdr_id] = product_id;

		AdvancedBrepShapeRepresentation *aBrep = sdr->GetAdvancedBrepShapeRepresentation();
		if (aBrep) {
		    id2name_map[aBrep->GetId()] = pname;
		    id2name_map[product_id] = pname;
		    id2productid_map[aBrep->GetId()] = product_id;
		    brlcad::step::Representation &representation = document.representations[aBrep->GetId()];
		    representation.entity_id = aBrep->GetId();
		    representation.product_id = product_id;
		    representation.type = "ADVANCED_BREP_SHAPE_REPRESENTATION";
		    representation.output_name = pname;
		    index_representation_geometry(aBrep, product_id, this, dotg,
			id2name_map, id2productid_map);
		    /* This length is used in the hierarchy build - this is how
		     * it was getting set when the Brep build came before the
		     * hierarchy build, so leave it for now, but should there be
		     * a look-up in the hierarchy build instead of here?*/
		    LocalUnits::length = aBrep->GetLengthConversionFactor();

		} else { // must be an assembly
		    ShapeRepresentation *aSR = sdr->GetShapeRepresentation();
		    if (aSR) {
			int sr_id = aSR->GetId();
			id2name_map[sr_id] = pname;
			id2name_map[product_id] = pname;
			id2productid_map[sr_id] = product_id;
			brlcad::step::Representation &representation = document.representations[sr_id];
			representation.entity_id = sr_id;
			representation.product_id = product_id;
			representation.type = "SHAPE_REPRESENTATION";
			representation.output_name = pname;

			/* Some AP214 writers place exact manifold solids directly in a
			 * plain SHAPE_REPRESENTATION, often beside PMI items.  Give each
			 * solid its own deterministic region name while retaining the
			 * representation-to-product mapping used by assemblies. */
			LIST_OF_REPRESENTATION_ITEMS *items = aSR->items_();
			if (items) {
			    const std::vector<FlattenedRepresentationItem> flattened =
				flatten_representation_items(items, this);
			    for (std::vector<FlattenedRepresentationItem>::const_iterator item =
				     flattened.begin(); item != flattened.end(); ++item) {
				RepresentationItem *geometry_item = item->item;
				SolidModel *solid = exact_brep_solid(geometry_item);
				ShellBasedSurfaceModel *shell = dynamic_cast<ShellBasedSurfaceModel *>(geometry_item);
				MappedItem *mapped = dynamic_cast<MappedItem *>(geometry_item);
				GeometricSet *geometric_set = dynamic_cast<GeometricSet *>(geometry_item);
				if (!solid && !shell && !mapped && !geometric_set)
				    continue;
				const int item_id = geometry_item->GetId();
				const std::string item_name = dotg->StableBRLCADName(pname + "_item", item_id);
				id2name_map[item_id] = item_name;
				id2productid_map[item_id] = product_id;
				if (shell)
				    shell2representation_map[item_id] = sr_id;
				brlcad::step::Representation &item_representation = document.representations[item_id];
				item_representation.entity_id = item_id;
				item_representation.product_id = product_id;
				item_representation.type = mapped ? "MAPPED_ITEM" :
				    (solid ? exact_brep_solid_type(solid) :
				    (shell ? "SHELL_BASED_SURFACE_MODEL" : "GEOMETRIC_SET"));
				item_representation.output_name = item_name;
			    }
			}
		    }
		}
		ClearEntityCache();
	    }
	}

	// Manifold Surface representations define a group of shells
	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "MANIFOLD_SURFACE_SHAPE_REPRESENTATION"))) {
	    ShapeRepresentation *sr = dynamic_cast<ShapeRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (!sr) {
		RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, sse->STEPfile_id,
		    "MANIFOLD_SURFACE_SHAPE_REPRESENTATION", std::string(), "entity materialization failed");
		ClearEntityCache();
		continue;
	    }
	    int id = sr->GetId();
	    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::const_iterator represented_product = id2productid_map.find(id);
	    const int product_id = represented_product == id2productid_map.end() ? 0 : represented_product->second;
	    std::string pname = product_id > 0 ? id2name_map[product_id] : dotg->StableBRLCADName(sr->Name(), id);
	    if (Verbose()) std::cout << "pname(" << id << "): " << pname << "\n";
	    if (pname.empty() || (pname.compare("''") == 0)) {
		std::string str = "ManifoldSurfaces@";
		pname = dotg->GetBRLCADName(str);
	    }
	    id2name_map[id] = pname;
	    // Find out which shell(s) are part of this manifold and add them to the map
	    LIST_OF_REPRESENTATION_ITEMS *items = sr->items_();
	    const std::vector<FlattenedRepresentationItem> flattened =
		flatten_representation_items(items, this);
	    for (std::vector<FlattenedRepresentationItem>::const_iterator ii = flattened.begin();
		 ii != flattened.end(); ++ii) {
		ShellBasedSurfaceModel *sm = dynamic_cast<ShellBasedSurfaceModel *>(ii->item);
		if (sm != NULL) {
		    int iid = ii->item->GetId();
		    if (Verbose()) std::cout << "iid: " << iid << "\n";
		    shell2representation_map[iid] = id;
		    id2productid_map[iid] = product_id > 0 ? product_id : id;
		    if (id2name_map[iid].empty())
			id2name_map[iid] = dotg->StableBRLCADName(pname + "_item", iid);
		}
	    }
	    ClearEntityCache();
	}

	// Geometric Sets define collections of surfaces
	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "GEOMETRIC_SET"))) {
	    GeometricSet *gs = dynamic_cast<GeometricSet*>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (!gs) {
		RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, sse->STEPfile_id,
		    "GEOMETRIC_SET", std::string(), "entity materialization failed");
		ClearEntityCache();
		continue;
	    }
	    int id = gs->GetId();
	    std::string pname = dotg->StableBRLCADName(gs->Name(), id);
	    if (Verbose()) std::cout << "pname(" << id << "): " << pname << "\n";
	    if (pname.empty() || (pname.compare("''") == 0)) {
		std::string str = "GeometricSet@";
		pname = dotg->GetBRLCADName(str);
	    }
	    id2name_map[id] = pname;
	    ClearEntityCache();
	}
    }
    SetProgress("product and representation structure indexed",
	static_cast<uint64_t>(num_ents), static_cast<uint64_t>(num_ents));
#ifdef HAVE_STEPCODE_LAZY
    index_lazy_exact_graph(lazy_exact_graph, this, dotg, id2name_map,
	id2productid_map, shell2representation_map);
    retain_lazy_representation_contexts(lazy_exact_graph, this);
#endif
    /* Schema-specific metadata and geometry hooks run only after product and
     * representation identity has been established. */
    brlcad::step::CurrentStepSchemaRuntime().PostIndex(
	*this, *dot_g, lazy_handled_sdrs);
    /* Assembly relationships are indexed before geometry conversion.  Make
     * every represented product a concrete database object now so a product
     * whose geometry is later skipped remains a resolvable (empty) assembly
     * member instead of becoming a dangling db_lookup reference. */
    ensure_product_combinations(this, dotg, dry_run, id2name_map);
    SetProgress("writing retained STEP metadata", 0,
	document.global_attributes.size());
    write_global_step_attributes(this, dotg, dry_run);
    SetProgress("retained STEP metadata written",
	document.global_attributes.size(), document.global_attributes.size());
    /*
     * Pickup BREP related to SHAPE_REPRESENTATION through SHAPE_REPRESENTATION_RELATIONSHIP
     *
     * like the following found in OpenBook Part 'C':
     *    #21281=SHAPE_DEFINITION_REPRESENTATION(#21280,#21270);
     *        #21280=PRODUCT_DEFINITION_SHAPE('','SHAPE FOR C.',#21279);
     *            #21279=PRODUCT_DEFINITION('design','',#21278,#21275);
     *                #21278=PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('1','LAST_VERSION',#21277,.MADE.);
     *                    #21277=PRODUCT('C','C','NOT SPECIFIED',(#21276));
     *        #21270=SHAPE_REPRESENTATION('',(#21259),#21267);
     *            #21259=AXIS2_PLACEMENT_3D('DANTE_BX_CPU_TOP_1',#21256,#21257,#21258);
     *            #21267=(GEOMETRIC_REPRESENTATION_CONTEXT(3)GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#21266))
     *                GLOBAL_UNIT_ASSIGNED_CONTEXT((#21260,#21264,#21265))REPRESENTATION_CONTEXT('ID1','3'));
     *
     *    #21271=SHAPE_REPRESENTATION_RELATIONSHIP('','',#21270,#21268);
     *        #21268=ADVANCED_BREP_SHAPE_REPRESENTATION('',(#21254),#21267);
     *    #21272=SHAPE_REPRESENTATION_RELATIONSHIP('','',#21270,#21269);
     *        #21269=MANIFOLD_SURFACE_SHAPE_REPRESENTATION('',(#21255),#21267);
     *
     */
    SetInstanceTypes({"SHAPE_REPRESENTATION_RELATIONSHIP",
	"REPRESENTATION_RELATIONSHIP", "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION"},
	lazy_handled_relationships);
    num_ents = InstanceCount();
    SetProgress("indexing representation relationships", 0,
	static_cast<uint64_t>(num_ents));
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	if (i % kProgressUpdateStride == 0)
	    SetProgress("indexing representation relationships",
		static_cast<uint64_t>(i), static_cast<uint64_t>(num_ents));
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "SHAPE_REPRESENTATION_RELATIONSHIP"))) {
	    ShapeRepresentationRelationship *srr = dynamic_cast<ShapeRepresentationRelationship *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));

	    if (srr) {
		ShapeRepresentation *aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());

		// First thing to try - Brep
		AdvancedBrepShapeRepresentation *aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		if (!aBrep) { //try rep_1
		    aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());
		    aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		}
		if ((aSR) && (aBrep)) {
		    int sr_id = aSR->GetId();
		    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator it = id2productid_map.find(sr_id);
		    if (it != id2productid_map.end()) { // product found
			int product_id = (*it).second;
			int brep_id = aBrep->GetId();
			const std::string product_name = id2name_map[product_id];
			id2name_map[brep_id] = product_name;
			id2productid_map[brep_id] = product_id;
			brlcad::step::Representation &representation = document.representations[brep_id];
			representation.entity_id = brep_id;
			representation.product_id = product_id;
			representation.type = "ADVANCED_BREP_SHAPE_REPRESENTATION";
			representation.output_name = product_name;
			index_representation_geometry(aBrep, product_id, this, dotg,
			    id2name_map, id2productid_map);
			LocalUnits::length = aBrep->GetLengthConversionFactor();
		    }
		    ClearEntityCache();
		    continue;
		}

		/* A product's placement representation is frequently related to a
		 * separate bounded wireframe representation, just as it is to a
		 * separate advanced BRep.  Preserve that ownership for curves and
		 * isolated Cartesian points. */
		GeometricallyBoundedWireframeShapeRepresentation *aWire =
		    dynamic_cast<GeometricallyBoundedWireframeShapeRepresentation *>(
			srr->GetRepresentationRelationshipRep_2());
		ShapeRepresentation *wire_anchor = dynamic_cast<ShapeRepresentation *>(
		    srr->GetRepresentationRelationshipRep_1());
		if (!aWire) {
		    aWire = dynamic_cast<GeometricallyBoundedWireframeShapeRepresentation *>(
			srr->GetRepresentationRelationshipRep_1());
		    wire_anchor = dynamic_cast<ShapeRepresentation *>(
			srr->GetRepresentationRelationshipRep_2());
		}
		if (wire_anchor && aWire) {
		    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator product =
			id2productid_map.find(wire_anchor->GetId());
		    if (product != id2productid_map.end() && product->second > 0) {
			const int product_id = product->second;
			const int wire_id = aWire->GetId();
			const std::string product_name = id2name_map[product_id];
			id2name_map[wire_id] = product_name;
			id2productid_map[wire_id] = product_id;
			brlcad::step::Representation &representation =
			    document.representations[wire_id];
			representation.entity_id = wire_id;
			representation.product_id = product_id;
			representation.type =
			    "GEOMETRICALLY_BOUNDED_WIREFRAME_SHAPE_REPRESENTATION";
			representation.output_name = product_name;
			index_representation_geometry(aWire, product_id, this, dotg,
			    id2name_map, id2productid_map);
		    }
		    ClearEntityCache();
		    continue;
		}

		/* A bounded surface representation has the same product-ownership
		 * relationship as a wireframe representation.  Index it here and
		 * leave all geometry work to the common representation converter;
		 * the historical in-line path only recognized bare B-spline
		 * surfaces and performed output during the indexing pass. */
		GeometricallyBoundedSurfaceShapeRepresentation *aBS =
		    dynamic_cast<GeometricallyBoundedSurfaceShapeRepresentation *>(
			srr->GetRepresentationRelationshipRep_2());
		ShapeRepresentation *surface_anchor = dynamic_cast<ShapeRepresentation *>(
		    srr->GetRepresentationRelationshipRep_1());
		if (!aBS) {
		    aBS = dynamic_cast<GeometricallyBoundedSurfaceShapeRepresentation *>(
			srr->GetRepresentationRelationshipRep_1());
		    surface_anchor = dynamic_cast<ShapeRepresentation *>(
			srr->GetRepresentationRelationshipRep_2());
		}
		if (surface_anchor && aBS) {
		    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator product =
			id2productid_map.find(surface_anchor->GetId());
		    if (product != id2productid_map.end() && product->second > 0) {
			const int product_id = product->second;
			const int surface_id = aBS->GetId();
			const std::string product_name = id2name_map[product_id];
			id2name_map[surface_id] = product_name;
			id2productid_map[surface_id] = product_id;
			brlcad::step::Representation &representation =
			    document.representations[surface_id];
			representation.entity_id = surface_id;
			representation.product_id = product_id;
			representation.type =
			    "GEOMETRICALLY_BOUNDED_SURFACE_SHAPE_REPRESENTATION";
			representation.output_name = product_name;
			index_representation_geometry(aBS, product_id, this, dotg,
			    id2name_map, id2productid_map);
		    }
		    ClearEntityCache();
		    continue;
		}

		ClearEntityCache();

	    }
	}
    }

    /* The lazy index already supplies shell-model identity.  Materializing a
     * model here follows every boundary shell and can load an entire plate
     * assembly just to choose a name. */
#ifdef HAVE_STEPCODE_LAZY
    if (HasLazyIndex()) {
	const std::vector<uint64_t> shell_models =
	    LazyInstancesByType("SHELL_BASED_SURFACE_MODEL");
	for (std::vector<uint64_t>::const_iterator id = shell_models.begin();
	     id != shell_models.end(); ++id) {
	    if (*id > static_cast<uint64_t>(INT_MAX) ||
		!ShouldConvertEntity(static_cast<int64_t>(*id))) continue;
	    const int model_id = static_cast<int>(*id);
	    if (id2name_map[model_id].empty())
		id2name_map[model_id] = dotg->StableBRLCADName(
		    std::string("SurfaceModel_step") + std::to_string(model_id), model_id);
	}
    }
#endif
    std::vector<std::string> surface_relationship_types = {
	"SHAPE_REPRESENTATION_RELATIONSHIP", "REPRESENTATION_RELATIONSHIP",
	"REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION"};
    if (!HasLazyIndex())
	surface_relationship_types.insert(surface_relationship_types.begin(),
	    "SHELL_BASED_SURFACE_MODEL");
    SetInstanceTypes(surface_relationship_types, lazy_handled_relationships);
    num_ents = InstanceCount();
    SetProgress("indexing surface-model relationships", 0,
	static_cast<uint64_t>(num_ents));
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	if (i % kProgressUpdateStride == 0)
	    SetProgress("indexing surface-model relationships",
		static_cast<uint64_t>(i), static_cast<uint64_t>(num_ents));
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}

	// Find plate mode Brep objects through e_shell_based_surface_model
	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "SHELL_BASED_SURFACE_MODEL"))) {
	    ShellBasedSurfaceModel *gr = dynamic_cast<ShellBasedSurfaceModel *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (!gr) {
		RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, sse->STEPfile_id,
		    "SHELL_BASED_SURFACE_MODEL", std::string(), "entity materialization failed");
		ClearEntityCache();
		continue;
	    }

	    int id = gr->GetId();
	    if (!ShouldConvertEntity(id)) {
		ClearEntityCache();
		continue;
	    }
	    std::string pname = id2name_map[id];
	    if (pname.empty())
		pname = dotg->StableBRLCADName(gr->Name(), id);
	    id2name_map[id] = pname;
	    if (Verbose()) std::cout << "\n" << pname << "(" << id << "): shell based surface model\n";

	    ClearEntityCache();
	}


	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "SHAPE_REPRESENTATION_RELATIONSHIP"))) {
	    ShapeRepresentationRelationship *srr = dynamic_cast<ShapeRepresentationRelationship *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));

	    if (srr) {
		ShapeRepresentation *aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());
		AdvancedBrepShapeRepresentation *aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		if (!aBrep) { //try rep_1
		    aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(srr->GetRepresentationRelationshipRep_1());
		    aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		}
		if ((aSR) && (aBrep)) {
		    int sr_id = aSR->GetId();
		    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator it = id2productid_map.find(sr_id);
		    if (it != id2productid_map.end()) { // product found
			int product_id = (*it).second;
			int brep_id = aBrep->GetId();

			it = id2productid_map.find(brep_id);
			if (it == id2productid_map.end()) { // brep not loaded yet so lets do that here.
			    string pname = id2name_map[brep_id];
			    if (pname.empty() || (pname.compare("''") == 0)) {
				std::string str = "Brep_@";
				pname = dotg->GetBRLCADName(str);
				id2name_map[aBrep->GetId()] = pname;
			    } else {
				id2name_map[aBrep->GetId()] = pname;
			    }
			    id2productid_map[brep_id] = product_id;
			    /* This length is used in the hierarchy build - this is how
			     * it was getting set when the Brep build came before the
			     * hierarchy build, so leave it for now, but should there be
			     * a look-up in the hierarchy build instead of here?*/
			    LocalUnits::length = aBrep->GetLengthConversionFactor();

			}
		    }
		}
		ClearEntityCache();
	    }
	}
    }


    if (Verbose()) {
	std::cerr << std::endl << "     Generating BRL-CAD hierarchy." << std::endl;
    }

#ifdef HAVE_STEPCODE_LAZY
    SetProgress("building assembly occurrences");
    convert_lazy_occurrences(lazy_exact_graph, this, dotg, dry_run, id2name_map);
#endif
    SetInstanceTypes({"CONTEXT_DEPENDENT_SHAPE_REPRESENTATION"}, lazy_handled_cdsrs);
    num_ents = InstanceCount();
    SetProgress("building assembly occurrences", 0,
	static_cast<uint64_t>(num_ents));
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	if (i % kProgressUpdateStride == 0)
	    SetProgress("building assembly occurrences", static_cast<uint64_t>(i),
		static_cast<uint64_t>(num_ents));
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION"))) {
	    ContextDependentShapeRepresentation *aCDSR = dynamic_cast<ContextDependentShapeRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (aCDSR && aCDSR->GetRepresentationRelationshipRep_1() && aCDSR->GetRepresentationRelationshipRep_2()) {
		int rep_1_id = aCDSR->GetRepresentationRelationshipRep_1()->GetId();
		int rep_2_id = aCDSR->GetRepresentationRelationshipRep_2()->GetId();
		int pid_1 = id2productid_map[rep_1_id];
		int pid_2 = id2productid_map[rep_2_id];
		Axis2Placement3D *axis1 = NULL;
		Axis2Placement3D *axis2 = NULL;
		if ((id2name_map.find(rep_1_id) != id2name_map.end()) && (id2name_map.find(rep_2_id) != id2name_map.end())) {
		    string comb = id2name_map[rep_1_id];
		    string member = id2name_map[rep_2_id];
		    int parent_product_id = pid_1;
		    int child_product_id = pid_2;
		    mat_t mat;
		    MAT_IDN(mat);

		    ProductDefinition *relatingProduct = aCDSR->GetRelatingProductDefinition();
		    ProductDefinition *relatedProduct = aCDSR->GetRelatedProductDefinition();
		    if (relatingProduct && relatedProduct) {
			int relatingID = relatingProduct->GetProductId();
			int relatedID = relatedProduct->GetProductId();

			if ((relatingID == pid_1) && (relatedID == pid_2)) {
			    axis1 = aCDSR->GetTransformItem_1();
			    axis2 = aCDSR->GetTransformItem_2();
			    comb = id2name_map[rep_1_id];
			    member = id2name_map[rep_2_id];
			    parent_product_id = pid_1;
			    child_product_id = pid_2;
			} else if ((relatingID == pid_2) && (relatedID == pid_1)) {
			    axis1 = aCDSR->GetTransformItem_2();
			    axis2 = aCDSR->GetTransformItem_1();
			    comb = id2name_map[rep_2_id];
			    member = id2name_map[rep_1_id];
			    parent_product_id = pid_2;
			    child_product_id = pid_1;
			} else {
			    RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning, aCDSR->GetId(),
				"CONTEXT_DEPENDENT_SHAPE_REPRESENTATION", "representation_relation",
				"product definitions do not match the referenced representations");
			}
		    }

		    if ((axis1 != NULL) && (axis2 != NULL)) {
			mat_t to_mat;
			mat_t from_mat;
			mat_t toinv_mat;

			//assign matrix values
			double translate_to[3];
			double translate_from[3];
			const double *toXaxis = axis1->GetXAxis();
			const double *toYaxis = axis1->GetYAxis();
			const double *toZaxis = axis1->GetZAxis();
			const double *fromXaxis = axis2->GetXAxis();
			const double *fromYaxis = axis2->GetYAxis();
			const double *fromZaxis = axis2->GetZAxis();
			VMOVE(translate_to,axis1->GetOrigin());
			VSCALE(translate_to,translate_to,LocalUnits::length);

			VMOVE(translate_from,axis2->GetOrigin());
			VSCALE(translate_from,translate_from,-LocalUnits::length);

			// undo from trans/rot
			MAT_IDN(from_mat);
			VMOVE(&from_mat[0], fromXaxis);
			VMOVE(&from_mat[4], fromYaxis);
			VMOVE(&from_mat[8], fromZaxis);
			MAT_DELTAS_VEC(from_mat, translate_from);

			// do to trans/rot
			MAT_IDN(to_mat);
			VMOVE(&to_mat[0], toXaxis);
			VMOVE(&to_mat[4], toYaxis);
			VMOVE(&to_mat[8], toZaxis);
			bn_mat_inv(toinv_mat, to_mat);
			MAT_DELTAS_VEC(toinv_mat, translate_to);

			bn_mat_mul(mat, toinv_mat, from_mat);
		    }
		    brlcad::step::Occurrence &occurrence = document.occurrences[aCDSR->GetId()];
		    occurrence.entity_id = aCDSR->GetId();
		    occurrence.usage_entity_id = 0;
		    occurrence.parent_product_id = parent_product_id;
		    occurrence.child_product_id = child_product_id;
		    occurrence.shape_method = "referenced";
		    for (size_t mi = 0; mi < 16; ++mi)
			occurrence.matrix[mi] = mat[mi];
		    if (!dry_run)
			dotg->AddMember(comb,member,mat);
		}
		ClearEntityCache();
	    }
	}
    }
#ifdef HAVE_STEPCODE_LAZY
    convert_lazy_exact_graph(lazy_exact_graph, this, dotg, dry_run,
	id2name_map, process_map);
    convert_lazy_auxiliary_geometry(lazy_exact_graph, this, dotg, dry_run,
	id2name_map, id2productid_map, process_map);
#endif
    /* Convert exact solid and wire representations before potentially large,
     * monolithic shell-based surface models.  This lets bounded solid jobs
     * make deterministic output progress even when a supplemental plate model
     * needs expensive serial pullback work. */
    SetInstanceTypes({"SHAPE_DEFINITION_REPRESENTATION", "PROPERTY_DEFINITION_REPRESENTATION"}, lazy_handled_sdrs);
    num_ents = InstanceCount();
    SetProgress("converting exact representation items", 0,
	static_cast<uint64_t>(num_ents), 0, statistics.geometry_written, "written");
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	if (i % kProgressUpdateStride == 0)
	    SetProgress("converting exact representation items",
		static_cast<uint64_t>(i), static_cast<uint64_t>(num_ents), 0,
		statistics.geometry_written, "written");
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}
	/* Shape Definition Representation */
	if ((sse->STEPfile_id > 0) && (IsSchemaEntity(sse, "PROPERTY_DEFINITION_REPRESENTATION"))) {
	    /* Non-shape property representations are handled by the metadata
	     * importers.  Do not feed their PMI/validation items to the
	     * geometry-only legacy factory. */
	    SDAI_Application_instance *used_representation =
		getEntityAttribute(sse, "used_representation");
	    if (!IsSchemaEntity(used_representation, "SHAPE_REPRESENTATION")) {
		ClearEntityCache();
		continue;
	    }
	    PropertyDefinitionRepresentation *sdr = dynamic_cast<PropertyDefinitionRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (!sdr) {
		RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, sse->STEPfile_id,
		    "PROPERTY_DEFINITION_REPRESENTATION", std::string(), "entity materialization failed");
		ClearEntityCache();
		continue;
	    } else {
		AdvancedBrepShapeRepresentation *aBrep = sdr->GetAdvancedBrepShapeRepresentation();
		if (aBrep) {
		    const int product_id = id2productid_map[aBrep->GetId()];
		    convert_representation_geometry(aBrep, product_id, this, dot_g, dry_run,
			id2name_map, process_map);
		} else {
		    ShapeRepresentation *aSR = sdr->GetShapeRepresentation();
		    if (aSR) {
			LIST_OF_REPRESENTATION_ITEMS *items = aSR->items_();
			if (items) {
			    const std::vector<FlattenedRepresentationItem> flattened =
				flatten_representation_items(items, this);
			    for (std::vector<FlattenedRepresentationItem>::const_iterator item =
				     flattened.begin(); item != flattened.end(); ++item) {
				GeometricSet *wire_set = dynamic_cast<GeometricSet *>(item->item);
				if (wire_set && (step_geometric_set_has_curves(wire_set) ||
					step_geometric_set_has_points(wire_set) ||
					step_geometric_set_has_surfaces(wire_set))) {
				    const int set_id = wire_set->GetId();
				    if (!ShouldConvertEntity(set_id)) continue;
				    if (process_map.find(set_id) != process_map.end())
					continue;
				    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator product = id2productid_map.find(set_id);
				    if (product == id2productid_map.end() || product->second <= 0) {
					record_brep_result(this, BREP_CONVERSION_FAILED, set_id, "GEOMETRIC_SET");
				    continue;
				    }
				    const int product_id = product->second;
				    std::string wire_name = id2name_map[set_id];
				    if (wire_name.empty()) {
					wire_name = dotg->StableBRLCADName(id2name_map[product_id] + "_wire", set_id);
					id2name_map[set_id] = wire_name;
				    }
				    BrepWriteStatus status = step_convert_geometric_set(wire_set,
					aSR, this, dot_g, &wire_name, dry_run,
					style_for_flattened_item(this, *item));
				    record_brep_result(this, status, set_id, "GEOMETRIC_SET");
				    if (status == BREP_WRITE_SUCCESS) {
					process_map[set_id] = product_id;
					if (!dry_run) {
					    mat_t mat;
					    MAT_IDN(mat);
					    dotg->AddMember(id2name_map[product_id], wire_name, mat);
					}
				    }
				    continue;
				}
			    }

			    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::const_iterator product =
				id2productid_map.find(aSR->GetId());
			    if (product != id2productid_map.end() && product->second > 0)
				convert_representation_geometry(aSR, product->second, this, dot_g,
				    dry_run, id2name_map, process_map);
			}
		    }
		}
		ClearEntityCache();
	    }
	}
    }

    /* Relationship-backed exact BREPs are the common AP203 assembly form.
     * Convert them before monolithic supplemental surface models so useful
     * solid output is not blocked by one expensive shell pullback. */
    SetInstanceTypes({"SHAPE_REPRESENTATION_RELATIONSHIP",
	"REPRESENTATION_RELATIONSHIP", "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION"},
	lazy_handled_relationships);
    num_ents = InstanceCount();
    SetProgress("converting relationship-backed exact geometry", 0,
	static_cast<uint64_t>(num_ents), 0, statistics.geometry_written, "written");
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	if (i % kProgressUpdateStride == 0)
	    SetProgress("converting relationship-backed exact geometry",
		static_cast<uint64_t>(i), static_cast<uint64_t>(num_ents), 0,
		statistics.geometry_written, "written");
	SDAI_Application_instance *sse = InstanceAt(i);
	if (!sse || sse->STEPfile_id <= 0 ||
		!IsSchemaEntity(sse, "SHAPE_REPRESENTATION_RELATIONSHIP"))
	    continue;
	ShapeRepresentationRelationship *srr = dynamic_cast<ShapeRepresentationRelationship *>(
	    Factory::CreateObject(this, sse));
	if (!srr) continue;
	ShapeRepresentation *aSR = dynamic_cast<ShapeRepresentation *>(
	    srr->GetRepresentationRelationshipRep_1());
	AdvancedBrepShapeRepresentation *aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(
	    srr->GetRepresentationRelationshipRep_2());
	if (!aBrep) {
	    aBrep = dynamic_cast<AdvancedBrepShapeRepresentation *>(
		srr->GetRepresentationRelationshipRep_1());
	    aSR = dynamic_cast<ShapeRepresentation *>(
		srr->GetRepresentationRelationshipRep_2());
	}
	if (aSR && aBrep) {
	    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator product =
		id2productid_map.find(aSR->GetId());
	    if (product != id2productid_map.end())
		convert_representation_geometry(aBrep, product->second, this, dotg,
		    dry_run, id2name_map, process_map);
	    ClearEntityCache();
	    continue;
	}
	GeometricallyBoundedWireframeShapeRepresentation *aWire =
	    dynamic_cast<GeometricallyBoundedWireframeShapeRepresentation *>(
		srr->GetRepresentationRelationshipRep_2());
	ShapeRepresentation *wire_anchor = dynamic_cast<ShapeRepresentation *>(
	    srr->GetRepresentationRelationshipRep_1());
	if (!aWire) {
	    aWire = dynamic_cast<GeometricallyBoundedWireframeShapeRepresentation *>(
		srr->GetRepresentationRelationshipRep_1());
	    wire_anchor = dynamic_cast<ShapeRepresentation *>(
		srr->GetRepresentationRelationshipRep_2());
	}
	if (wire_anchor && aWire) {
	    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator product =
		id2productid_map.find(wire_anchor->GetId());
	    if (product != id2productid_map.end() && product->second > 0)
		convert_representation_geometry(aWire, product->second, this, dotg,
		    dry_run, id2name_map, process_map);
	    ClearEntityCache();
	    continue;
	}
	if (IsSchemaEntity(sse,
		"REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION")) {
	    ClearEntityCache();
	    continue;
	}

	/* RP8 permits secondary, non-transforming shape representations to
	 * contribute geometry to the same product as the primary SDR.  The lazy
	 * document graph has already propagated product ownership across that
	 * relationship.  Route any remaining shape-representation subtype through
	 * the common item converter; this includes bounded geometric point and
	 * surface sets, in addition to future schema subtypes with supported
	 * representation items. */
	ShapeRepresentation *related[] = {
	    dynamic_cast<ShapeRepresentation *>(
		srr->GetRepresentationRelationshipRep_1()),
		dynamic_cast<ShapeRepresentation *>(
		srr->GetRepresentationRelationshipRep_2())
	};
	int related_ids[] = {
	    related[0] ? related[0]->GetId() : 0,
	    related[1] ? related[1]->GetId() : 0
	};
	int related_product_id = 0;
	bool conflicting_products = false;
	for (size_t related_index = 0; related_index < 2; ++related_index) {
	    if (related_ids[related_index] <= 0) continue;
	    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator product =
		id2productid_map.find(related_ids[related_index]);
	    if (product == id2productid_map.end() || product->second <= 0) continue;
	    if (related_product_id && related_product_id != product->second)
		conflicting_products = true;
	    else
		related_product_id = product->second;
	}
	if (related_product_id && !conflicting_products) {
	    /* Getter results are backed by the current lazy materialization batch.
	     * Converting one side may release that batch, so retain only IDs and
	     * rematerialize each representation immediately before use. */
	    ClearEntityCache();
	    for (size_t related_index = 0; related_index < 2; ++related_index) {
		if (related_ids[related_index] <= 0) continue;
		SDAI_Application_instance *related_instance =
		    getEntity(related_ids[related_index]);
		ShapeRepresentation *related_representation =
		    dynamic_cast<ShapeRepresentation *>(Factory::CreateObject(
			    this, related_instance));
		if (!related_representation) {
		    ClearEntityCache();
		    continue;
		}
		id2productid_map[related_ids[related_index]] = related_product_id;
		convert_representation_geometry(related_representation, related_product_id,
		this, dotg, dry_run, id2name_map, process_map);
		ClearEntityCache();
	    }
	}
	ClearEntityCache();
    }

    SetInstanceTypes({"SHELL_BASED_SURFACE_MODEL"});
    num_ents = InstanceCount();
    SetProgress("converting shell-based surface models", 0,
	static_cast<uint64_t>(num_ents), 0, statistics.geometry_written, "written");
    std::vector<std::unique_ptr<DetachedBrepJob> > surface_jobs;
#ifdef HAVE_STEPCODE_LAZY
    if (HasLazyIndex()) {
	const std::vector<uint64_t> surface_ids =
	    LazyInstancesByType("SHELL_BASED_SURFACE_MODEL");
	SetProgress("indexing shell boundaries", 0, surface_ids.size(), 0,
	    statistics.geometry_written, "written");
	for (size_t i = 0; i < surface_ids.size(); ++i) {
	    if (CancellationRequested()) return false;
	    if (surface_ids[i] > static_cast<uint64_t>(INT_MAX)) continue;
	    const int surface_id = static_cast<int>(surface_ids[i]);
	    SetProgress("indexing shell boundaries", i, surface_ids.size(),
		surface_id, statistics.geometry_written, "written");
	    const std::set<int64_t> &selected = ImportOptions().selected_entity_ids;
	    const bool selected_model = selected.empty() ||
		selected.find(surface_id) != selected.end();
	    const std::vector<uint64_t> boundaries = LazyForwardReferences(surface_id);
	    bool selected_boundary = false;
	    if (!selected_model) {
		for (std::vector<uint64_t>::const_iterator boundary = boundaries.begin();
		     boundary != boundaries.end(); ++boundary) {
		    if (*boundary <= static_cast<uint64_t>(INT64_MAX) &&
			selected.find(static_cast<int64_t>(*boundary)) != selected.end()) {
			selected_boundary = true;
			break;
		    }
		}
	    }
	    if (!selected_model && !selected_boundary) continue;
	    if (selected.find(surface_id) != selected.end())
		ShouldConvertEntity(surface_id);

	    std::string surface_name = id2name_map[surface_id];
	    if (surface_name.empty())
		surface_name = dotg->StableBRLCADName(
		    std::string("SurfaceModel_step") + std::to_string(surface_id),
		    surface_id);
	    id2name_map[surface_id] = surface_name;
	    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::const_iterator product =
		id2productid_map.find(surface_id);
	    const int product_id = product == id2productid_map.end() ? 0 :
		product->second;
	    const brlcad::step::Style *style = style_for_item(this, surface_id);
	    uint64_t representation_id = 0;
	    for (std::map<uint64_t, std::vector<uint64_t> >::const_iterator
		    represented =
			lazy_exact_graph.representation_surface_models.begin();
		 represented !=
		    lazy_exact_graph.representation_surface_models.end();
		 ++represented) {
		if (std::find(represented->second.begin(),
			represented->second.end(),
			static_cast<uint64_t>(surface_id)) !=
			represented->second.end()) {
		    representation_id = represented->first;
		    break;
		}
	    }
	    const LazyRepresentationUnits units = lazy_representation_units(this,
		lazy_exact_graph, representation_id);

	    std::vector<std::pair<uint64_t, std::string> > shells;
	    std::vector<size_t> shell_ordinals;
	    size_t model_shell_count = 0;
	    size_t filtered_open_shell_count = 0;
	    for (std::vector<uint64_t>::const_iterator boundary = boundaries.begin();
		 boundary != boundaries.end(); ++boundary) {
		const std::string type = LazyTypeName(*boundary);
		size_t shell_ordinal = 0;
		if (type == "CLOSED_SHELL" || type == "OPEN_SHELL")
		    shell_ordinal = ++model_shell_count;
		const bool selected_shell = *boundary <= static_cast<uint64_t>(INT_MAX) &&
		    (type == "CLOSED_SHELL" || type == "OPEN_SHELL") &&
		    (selected_model || selected.find(static_cast<int64_t>(*boundary)) !=
			selected.end());
		if (selected_shell && type == "OPEN_SHELL" &&
			ImportOptions().skip_open_shells) {
		    ++filtered_open_shell_count;
		    if (!selected.empty())
			ShouldConvertEntity(static_cast<int64_t>(*boundary));
		    continue;
		}
		if (selected_shell) {
		    shells.push_back(std::make_pair(*boundary, type));
		    shell_ordinals.push_back(shell_ordinal);
		    if (!selected.empty())
			ShouldConvertEntity(static_cast<int64_t>(*boundary));
		}
	    }
	    if (filtered_open_shell_count) {
		statistics.geometry_filtered += filtered_open_shell_count;
		RecordRepresentationItemCoverage(surface_id,
		    brlcad::step::RepresentationCoverageStatus::Filtered,
		    std::to_string(filtered_open_shell_count) +
		    (filtered_open_shell_count == 1 ?
			" OPEN_SHELL boundary excluded by --skip-open-shells" :
			" OPEN_SHELL boundaries excluded by --skip-open-shells"));
	    }
	    for (size_t shell_number = 0; shell_number < shells.size(); ++shell_number) {
		const int shell_id = static_cast<int>(shells[shell_number].first);
		const std::string &type = shells[shell_number].second;
		std::string shell_name = model_shell_count == 1 ? surface_name :
		    dotg->StableBRLCADName(surface_name + "_shell" +
			std::to_string(shell_ordinals[shell_number]), shell_id);
		id2name_map[shell_id] = shell_name;
		id2productid_map[shell_id] = product_id;
		mat_t identity;
		MAT_IDN(identity);
		std::unique_ptr<DetachedBrepJob> job = detach_brep_job_data(
		    shell_id, type, std::string(), shell_name, product_id,
		    units.length, units.planeangle, units.solidangle,
		    units.tolerance, identity, style);
		if (model_shell_count == 1) job->output_source_id = surface_id;
		job->coverage_entity_id = surface_id;
		job->is_region = type == "CLOSED_SHELL";
		if (product_id > 0) {
		    DetachedBrepMember member;
		    member.combination = id2name_map[product_id];
		    MAT_COPY(member.matrix, identity);
		    job->members.push_back(member);
		}
		surface_jobs.push_back(std::move(job));
	    }
	    if (shells.empty() && !filtered_open_shell_count) {
		RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, surface_id,
		    "SHELL_BASED_SURFACE_MODEL", "sbsm_boundary",
		    "lazy index found no OPEN_SHELL or CLOSED_SHELL boundary references");
		RecordSkippedItem(surface_id, "SHELL_BASED_SURFACE_MODEL",
		    "surface model has no indexed shell boundaries");
		++statistics.geometry_attempted;
		++statistics.geometry_skipped;
		RecordRepresentationItemCoverage(surface_id,
		    brlcad::step::RepresentationCoverageStatus::Malformed,
		    "surface model has no indexed OPEN_SHELL or CLOSED_SHELL boundaries");
	    }
	}
	SetProgress("shell boundaries indexed", surface_ids.size(),
	    surface_ids.size(), 0, surface_jobs.size(), "jobs");
    } else
#endif
    for (int i = 0; i < num_ents; ++i) {
	if (CancellationRequested()) return false;
	SDAI_Application_instance *sse = InstanceAt(i);
	if (!sse || sse->STEPfile_id <= 0 ||
		!IsSchemaEntity(sse, "SHELL_BASED_SURFACE_MODEL"))
	    continue;
	ShellBasedSurfaceModel *surface_model = dynamic_cast<ShellBasedSurfaceModel *>(
	    Factory::CreateObject(this, sse));
	if (!surface_model) {
	    RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		sse->STEPfile_id, "SHELL_BASED_SURFACE_MODEL", std::string(),
		"entity materialization failed");
	    ClearEntityCache();
	    continue;
	}
	const int surface_id = surface_model->GetId();
	SetProgress("converting shell-based surface models", static_cast<uint64_t>(i),
	    static_cast<uint64_t>(num_ents), surface_id,
	    statistics.geometry_written, "written");
	if (!ShouldConvertEntity(surface_id)) {
	    ClearEntityCache();
	    continue;
	}
	const size_t open_shell_count = surface_model->OpenShellCount();
	if (ImportOptions().skip_open_shells && open_shell_count) {
	    statistics.geometry_filtered += open_shell_count;
	    RecordRepresentationItemCoverage(surface_id,
		brlcad::step::RepresentationCoverageStatus::Filtered,
		std::to_string(open_shell_count) +
		(open_shell_count == 1 ?
		    " OPEN_SHELL boundary excluded by --skip-open-shells" :
		    " OPEN_SHELL boundaries excluded by --skip-open-shells"));
	    if (open_shell_count == surface_model->BoundaryCount()) {
		ClearEntityCache();
		continue;
	    }
	}
	std::string surface_name = id2name_map[surface_id];
	if (surface_name.empty())
	    surface_name = dotg->StableBRLCADName(surface_model->Name(), surface_id);
	id2name_map[surface_id] = surface_name;
	ShapeRepresentation *surface_representation = NULL;
	MAP_OF_ENTITY_ID_TO_PRODUCT_ID::const_iterator representation =
	    shell2representation_map.find(surface_id);
	if (representation != shell2representation_map.end()) {
	    SDAI_Application_instance *representation_entity =
		getEntity(representation->second);
	    if (representation_entity)
		surface_representation = dynamic_cast<ShapeRepresentation *>(
		    Factory::CreateObject(this, representation_entity));
	}
	mat_t write_matrix;
	representation_matrix(surface_representation, write_matrix);
	const brlcad::step::Style *style = style_for_item(this, surface_id);
	if (!style && surface_representation)
	    style = style_for_item(this, surface_representation->GetId());
	MAP_OF_ENTITY_ID_TO_PRODUCT_ID::const_iterator product =
	    id2productid_map.find(surface_id);
	const int product_id = product == id2productid_map.end() ? 0 :
	    product->second;
	std::unique_ptr<DetachedBrepJob> job = detach_brep_job_data(surface_id,
	    "SHELL_BASED_SURFACE_MODEL", surface_model->Name(), surface_name,
	    product_id,
	    surface_representation ? surface_representation->GetLengthConversionFactor() : 1.0,
	    surface_representation ? surface_representation->GetPlaneAngleConversionFactor() : 1.0,
	    surface_representation ? surface_representation->GetSolidAngleConversionFactor() : 1.0,
	    ImportOptions().absolute_tolerance_mm > 0.0 ?
		LocalUnits::tolerance :
		(surface_representation ?
		    surface_representation->GetLengthUncertainty() :
		    LocalUnits::representation_tolerance),
	    write_matrix, style);
	job->is_region = ImportOptions().skip_open_shells && open_shell_count > 0;
	job->coverage_entity_id = surface_id;
	if (product_id > 0) {
	    mat_t identity;
	    MAT_IDN(identity);
	    DetachedBrepMember member;
	    member.combination = id2name_map[product_id];
	    MAT_COPY(member.matrix, identity);
	    job->members.push_back(member);
	}
	surface_jobs.push_back(std::move(job));
	ClearEntityCache();
    }
    write_detached_brep_jobs(surface_jobs, this, dot_g, dry_run, process_map);

    for (std::set<int64_t>::const_iterator requested = import_options.selected_entity_ids.begin();
	 requested != import_options.selected_entity_ids.end(); ++requested) {
	if (statistics.selected_entity_ids_encountered.find(*requested) !=
		statistics.selected_entity_ids_encountered.end()) continue;
	++statistics.geometry_attempted;
	++statistics.geometry_skipped;
	RecordSkippedItem(*requested, "ENTITY_SELECTION",
	    "selected entity was not found as a supported representation-item root");
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, *requested,
	    "ENTITY_SELECTION", std::string(),
	    "selected entity was not found as a supported representation-item root");
    }
    FinalizeRepresentationCoverage();

    /* Retain a product-level explanation for partial children.  The complete
     * entity-specific reasons remain in the JSON report. */
    if (!dry_run) {
	std::map<int, uint64_t> skipped_by_product;
	for (std::vector<brlcad::step::SkippedItem>::const_iterator skipped =
		statistics.skipped_items.begin(); skipped != statistics.skipped_items.end();
		++skipped) {
	    if (skipped->entity_id <= 0 || skipped->entity_id > INT_MAX) continue;
	    MAP_OF_ENTITY_ID_TO_PRODUCT_ID::const_iterator product =
		id2productid_map.find(static_cast<int>(skipped->entity_id));
	    if (product != id2productid_map.end() && product->second > 0)
		++skipped_by_product[product->second];
	}
	std::set<int> affected_products;
	for (std::map<int, uint64_t>::const_iterator skipped = skipped_by_product.begin();
		skipped != skipped_by_product.end(); ++skipped)
	    affected_products.insert(skipped->first);
	for (std::map<int64_t, brlcad::step::Product>::const_iterator product =
		document.products.begin(); product != document.products.end(); ++product)
	    if (product->second.invalid_geometry_count && product->first > 0 &&
		    product->first <= INT_MAX)
		affected_products.insert(static_cast<int>(product->first));
	for (std::set<int>::const_iterator affected = affected_products.begin();
		affected != affected_products.end(); ++affected) {
	    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME::const_iterator product_name =
		id2name_map.find(*affected);
	    if (product_name == id2name_map.end() || product_name->second.empty()) continue;
	    const uint64_t skipped = skipped_by_product[*affected];
	    uint64_t invalid = 0;
	    std::map<int64_t, brlcad::step::Product>::const_iterator product =
		document.products.find(*affected);
	    if (product != document.products.end())
		invalid = product->second.invalid_geometry_count;
	    const char *status = skipped && invalid ? "partial_invalid" :
		(skipped ? "partial" : "invalid");
	    dotg->SetCombinationAttribute(product_name->second,
		"step:import_status", status);
	    if (skipped)
		dotg->SetCombinationAttribute(product_name->second,
		    "step:skipped_geometry_count", std::to_string(skipped));
	    if (invalid)
		dotg->SetCombinationAttribute(product_name->second,
		    "step:invalid_geometry_count", std::to_string(invalid));
	}
    }

    SetProgress("writing BRL-CAD hierarchy", statistics.geometry_written,
	statistics.geometry_attempted, 0, statistics.geometry_skipped, "skipped");
    if (!dry_run && !dotg->WriteCombs()) {
	++statistics.output_failures;
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, 0, "BRLCAD_DATABASE",
	    std::string(), "failed writing one or more hierarchy combinations");
    }

    SetProgress("conversion complete", statistics.geometry_attempted,
	statistics.geometry_attempted, 0, statistics.geometry_written, "written");

    if (summary_log_file) {
	ofstream step_log;
	step_log.open(summary_log_file);
	auto write_summary_row = [&](uint64_t source_id, const std::string &entity_name) {
	    std::string pname;
	    std::map<int, int>::iterator e_it = entity_status.end();
	    if (source_id <= static_cast<uint64_t>(INT_MAX)) {
		const int id = static_cast<int>(source_id);
		pname = id2name_map[id];
		e_it = entity_status.find(id);
	    }
	    if (!pname.empty() && pname.compare("''") != 0)
		step_log << pname << ',';
	    else
		step_log << "'',";
	    step_log << source_id << ',' << entity_name << ',';
	    if (e_it == entity_status.end())
		step_log << "NOT_PROCESSED\n";
	    else if (e_it->second == STEP_LOADED)
		step_log << "SUCCESS\n";
	    else if (e_it->second == STEP_LOAD_ERROR)
		step_log << "LOAD_ERROR\n";
	    else
		step_log << "UNKNOWN_STATUS\n";
	};
#ifdef HAVE_STEPCODE_LAZY
	if (HasLazyIndex()) {
	    for (std::vector<uint64_t>::const_iterator id = lazy_instance_ids.begin();
		 id != lazy_instance_ids.end(); ++id) {
		if (CancellationRequested()) return false;
		write_summary_row(*id, LazyTypeName(*id));
	    }
	    step_log.close();
	} else
#endif
	{
	ResetInstanceTypes();
	num_ents = InstanceCount();
	for (int i = 0; i < num_ents; i++) {
	    if (CancellationRequested()) return false;
	    SDAI_Application_instance *sse = InstanceAt(i);
	    if (sse == NULL) {
		continue;
	    }
	    write_summary_row(static_cast<uint64_t>(sse->StepFileId()), sse->EntityName());
	}
	step_log.close();
	}
    }

    statistics.products = static_cast<uint64_t>(document.products.size());
    statistics.occurrences = static_cast<uint64_t>(document.occurrences.size());
#ifdef HAVE_STEPCODE_LAZY
    releaseLazyBatches();
#endif
    return statistics.output_failures == 0 &&
	(!import_options.strict ||
	 (statistics.geometry_skipped == 0 &&
	  statistics.invalid_breps_written == 0 &&
	  statistics.properties_invalid == 0 &&
	  statistics.pmi_invalid_records == 0));
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
