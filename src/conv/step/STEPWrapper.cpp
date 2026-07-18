/*                 S T E P W R A P P E R . C P P
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
/** @file step/STEPWrapper.cpp
 *
 * C++ wrapper to NIST STEP parser/database functions.
 *
 */

#include "common.h"
#include <atomic>
#include <cctype>
#include <algorithm>
#include <climits>

#include "brep/cdt.h"
#include "brep/pullback.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <thread>
#include <vector>

/* interface header */
#include "./STEPWrapper.h"
#include "Factory.h"

/* implementation headers */
#include "AdvancedBrepShapeRepresentation.h"
#include "BrepWithVoids.h"
#include "BSplineSurfaceWithKnots.h"
#include "GeometricallyBoundedSurfaceShapeRepresentation.h"
#include "GeometricSet.h"
#include "GeometricSetSelect.h"
#include "Axis2Placement3D.h"
#include "CartesianTransformationOperator3D.h"
#include "CartesianPoint.h"
#include "CompoundRepresentationItem.h"
#include "Curve.h"
#include "Direction.h"
#include "SurfacePatch.h"
#include "LocalUnits.h"
#include "FacetedBrep.h"
#include "MappedItem.h"
#include "ManifoldSolidBrep.h"
#include "ManifoldSurfaceShapeRepresentation.h"
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
#ifdef AP214e3
#  include "AP214CSG.h"
#  include "AP214Metadata.h"
#  include "AP214Presentation.h"
#  include "AP214SweptSolid.h"
#endif
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
#  include "AP214LazySession.h"
#endif

namespace {

/* Dense repair validation uses a power-of-two subdivision count so adjacent
 * passes sample the same deterministic parameter fractions.  1024 segments
 * proved sufficient to expose the narrow Mark V seam and pole failures while
 * keeping each candidate repair bounded; it is a validation budget, not a
 * tolerance or a value prescribed by STEP/openNURBS. */
constexpr int kDenseValidationSegments = 1024;

/* Adaptive pullback may add one UV vertex per validation segment.  The extra
 * endpoint makes the point budget correspond exactly to the dense segment
 * budget and prevents one pathological seam from dominating import time. */
constexpr size_t kMaximumAdaptivePullbackPoints =
    static_cast<size_t>(kDenseValidationSegments) + 1;

/* Reject compiler-hostile or corrupt pcurves before using their span count to
 * size bounded repair work.  This is deliberately separate from the dense
 * sampling budget even though the current limits have the same value. */
constexpr int kMaximumPcurveSpans = 1024;

/* A surface boundary is a one-dimensional curve, but a seed obtained from a
 * global surface closest-point query can lie on the wrong repeated branch of
 * a doubly periodic surface.  Sample a modest, fixed number of boundary
 * intervals before retrying the local Newton solve.  This is a search budget;
 * every accepted result is still checked against model uncertainty. */
constexpr int kBoundaryParameterSearchSegments = 256;

/* Scale ON_ZERO_TOLERANCE above floating-point noise for parameter/lift
 * equivalence checks.  Keep this distinct from model-space uncertainty. */
constexpr double kNumericalToleranceScale = 1024.0;

}

STEPWrapper::STEPWrapper()
    : registry(NULL), sfile(NULL), dotg(NULL), verbose(false)
{
    int ownsInstanceMemory = 1;
    instance_list = new InstMgr(ownsInstanceMemory);
}


STEPWrapper::~STEPWrapper()
{
    ClearEntityCache();
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    releaseLazyBatches();
    lazy_session.reset();
#endif
    delete sfile;
    delete instance_list;
    delete registry;
    dotg = NULL;
}


STEPDetachedEntityArena::STEPDetachedEntityArena() = default;


STEPDetachedEntityArena::~STEPDetachedEntityArena()
{
    for (std::map<int, STEPEntity *>::iterator object = objects.begin();
	 object != objects.end(); ++object)
	delete object->second;
    for (std::list<STEPEntity *>::iterator object = unmapped_objects.begin();
	 object != unmapped_objects.end(); ++object)
	delete *object;
}


STEPEntity *
STEPDetachedEntityArena::FindObject(int id) const
{
    std::map<int, STEPEntity *>::const_iterator found = objects.find(id);
    return found == objects.end() ? NULL : found->second;
}


void
STEPDetachedEntityArena::ResetOpenNURBSState()
{
    for (std::map<int, STEPEntity *>::iterator object = objects.begin();
	 object != objects.end(); ++object) {
	if (object->second)
	    object->second->ResetONState();
    }
    for (std::list<STEPEntity *>::iterator object = unmapped_objects.begin();
	 object != unmapped_objects.end(); ++object) {
	if (*object)
	    (*object)->ResetONState();
    }
}


int
STEPWrapper::InstanceCount() const
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    if (lazy_session)
	return static_cast<int>(lazy_filter_active ? lazy_iteration_ids.size() : lazy_instance_ids.size());
#endif
    return instance_list ? instance_list->InstanceCount() : 0;
}


SDAI_Application_instance *
STEPWrapper::InstanceAt(int index)
{
    if (index < 0 || index >= InstanceCount()) return NULL;
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    if (lazy_session) {
	const std::vector<uint64_t> &ids = lazy_filter_active ? lazy_iteration_ids : lazy_instance_ids;
	return activateLazyRoot(ids[static_cast<size_t>(index)]);
    }
#endif
    return instance_list ? instance_list->GetSTEPentity(index) : NULL;
}


void
STEPWrapper::SetInstanceTypes(const std::vector<std::string> &types,
    const std::vector<uint64_t> &excluded_ids)
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    if (!lazy_session) return;
    releaseLazyBatches();
    std::set<uint64_t> selected;
    const std::set<uint64_t> excluded(excluded_ids.begin(), excluded_ids.end());
    for (std::vector<std::string>::const_iterator type = types.begin(); type != types.end(); ++type) {
	const std::vector<uint64_t> ids = lazy_session->InstancesByType(*type);
	selected.insert(ids.begin(), ids.end());
    }
    lazy_iteration_ids.clear();
    lazy_iteration_ids.reserve(selected.size());
    for (std::vector<uint64_t>::const_iterator id = lazy_instance_ids.begin();
	 id != lazy_instance_ids.end(); ++id) {
	if (selected.find(*id) != selected.end() && excluded.find(*id) == excluded.end())
	    lazy_iteration_ids.push_back(*id);
    }
    lazy_filter_active = true;
#else
    (void)types;
    (void)excluded_ids;
#endif
}


void
STEPWrapper::ResetInstanceTypes()
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    if (!lazy_session) return;
    releaseLazyBatches();
    lazy_iteration_ids.clear();
    lazy_filter_active = false;
#endif
}


bool
STEPWrapper::HasLazyIndex() const
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    return lazy_session.get() != NULL;
#else
    return false;
#endif
}


std::vector<uint64_t>
STEPWrapper::LazyInstancesByType(const std::string &type) const
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    return lazy_session ? lazy_session->InstancesByType(type) : std::vector<uint64_t>();
#else
    (void)type;
    return std::vector<uint64_t>();
#endif
}


std::string
STEPWrapper::LazyTypeName(uint64_t id) const
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    return lazy_session ? lazy_session->TypeName(id) : std::string();
#else
    (void)id;
    return std::string();
#endif
}


std::vector<uint64_t>
STEPWrapper::LazyForwardReferences(uint64_t id) const
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    return lazy_session ? lazy_session->ForwardReferences(id) : std::vector<uint64_t>();
#else
    (void)id;
    return std::vector<uint64_t>();
#endif
}


std::vector<uint64_t>
STEPWrapper::LazyReverseReferences(uint64_t id) const
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    return lazy_session ? lazy_session->ReverseReferences(id) : std::vector<uint64_t>();
#else
    (void)id;
    return std::vector<uint64_t>();
#endif
}


InstMgrBase *
STEPWrapper::referenceManager() const
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    if (lazy_session) return lazy_session->ReferenceManager();
#endif
    return instance_list;
}


static bool
diagnostic_less(const brlcad::step::Diagnostic &left,
    const brlcad::step::Diagnostic &right)
{
    if (left.entity_id != right.entity_id)
	return left.entity_id < right.entity_id;
    if (left.severity != right.severity)
	return static_cast<int>(left.severity) < static_cast<int>(right.severity);
    if (left.entity_type != right.entity_type)
	return left.entity_type < right.entity_type;
    if (left.attribute != right.attribute)
	return left.attribute < right.attribute;
    return left.message < right.message;
}


#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
static void
update_lazy_statistics(brlcad::step::ImportStatistics &statistics,
    const brlcad::step::AP214LazyStatistics &cache)
{
    statistics.lazy_indexed_instances = cache.instances_scanned;
    statistics.lazy_current_loaded_instances = cache.instances_loaded;
    statistics.lazy_loaded_instances = std::max(statistics.lazy_loaded_instances,
	cache.cache_high_water);
    statistics.lazy_pinned_instances = cache.instances_pinned;
    statistics.lazy_cache_hits = cache.cache_hits;
    statistics.lazy_cache_misses = cache.cache_misses;
    statistics.lazy_materializations = cache.materializations;
    statistics.lazy_evictions = cache.evictions;
    statistics.lazy_active_batches = cache.active_batches;
    statistics.lazy_data_sections = cache.data_sections;
    statistics.lazy_cache_bytes = cache.resident_source_bytes;
    statistics.lazy_cache_byte_high_water = std::max(
	statistics.lazy_cache_byte_high_water, cache.source_bytes_high_water);
    statistics.lazy_cache_bytes_available = true;
}

void
STEPWrapper::recordLazyDiagnostic(const brlcad::step::AP214LazyDiagnostic &source)
{
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    brlcad::step::DiagnosticSeverity severity = brlcad::step::DiagnosticSeverity::Information;
    if (source.severity == 1) severity = brlcad::step::DiagnosticSeverity::Warning;
    if (source.severity == 2) severity = brlcad::step::DiagnosticSeverity::Error;
    if (source.severity >= 3) severity = brlcad::step::DiagnosticSeverity::Fatal;
    for (std::vector<brlcad::step::Diagnostic>::iterator diagnostic = diagnostics.begin();
	 diagnostic != diagnostics.end(); ++diagnostic) {
	if (diagnostic->severity == severity && diagnostic->entity_id ==
		static_cast<int64_t>(source.entity_id) &&
	    diagnostic->entity_type == source.entity_type &&
	    diagnostic->attribute == source.attribute &&
	    diagnostic->message == source.message) {
	    diagnostic->file_offset = source.file_offset;
	    diagnostic->line = source.line;
	    diagnostic->repeat_count = std::max(diagnostic->repeat_count,
		source.occurrences);
	    return;
	}
    }
    brlcad::step::Diagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.entity_id = static_cast<int64_t>(source.entity_id);
    diagnostic.entity_type = source.entity_type;
    diagnostic.file_offset = source.file_offset;
    diagnostic.line = source.line;
    diagnostic.attribute = source.attribute;
    diagnostic.message = source.message;
    diagnostic.repeat_count = source.occurrences;
    diagnostics.insert(std::lower_bound(diagnostics.begin(), diagnostics.end(),
	diagnostic, diagnostic_less), diagnostic);
}

void
STEPWrapper::synchronizeLazyDiagnostics()
{
    if (!lazy_session) return;
    const std::vector<brlcad::step::AP214LazyDiagnostic> &source =
	lazy_session->Diagnostics();
    for (std::vector<brlcad::step::AP214LazyDiagnostic>::const_iterator diagnostic =
	     source.begin(); diagnostic != source.end(); ++diagnostic)
	recordLazyDiagnostic(*diagnostic);
}

void
STEPWrapper::releaseLazyBatches()
{
    if (!lazy_session) return;
    ClearEntityCache();
    lazy_supplemental_batches.clear();
    lazy_batch.reset();
    const brlcad::step::AP214LazyStatistics cache = lazy_session->Statistics();
    update_lazy_statistics(statistics, cache);
    synchronizeLazyDiagnostics();
}


SDAI_Application_instance *
STEPWrapper::activateLazyRoot(uint64_t id)
{
    releaseLazyBatches();
    lazy_batch.reset(new brlcad::step::AP214LazyBatch(lazy_session->LoadBatch(id)));
    if (!lazy_batch->Valid()) return NULL;
    const brlcad::step::AP214LazyStatistics cache = lazy_session->Statistics();
    update_lazy_statistics(statistics, cache);
    return lazy_batch->Get(id);
}
#endif


STEPEntity *
STEPWrapper::FindObject(int id) const
{
    std::map<int, STEPEntity *>::const_iterator found = entity_objects.find(id);
    return found == entity_objects.end() ? NULL : found->second;
}


void
STEPWrapper::AddObject(STEPEntity *object)
{
    if (!object) return;
    if (object->STEPid() > 0)
	entity_objects[object->STEPid()] = object;
    else
	unmapped_objects.push_back(object);
}


void
STEPWrapper::ClearEntityCache()
{
    for (std::map<int, STEPEntity *>::iterator i = entity_objects.begin(); i != entity_objects.end(); ++i)
	delete i->second;
    entity_objects.clear();
    for (std::list<STEPEntity *>::iterator i = unmapped_objects.begin(); i != unmapped_objects.end(); ++i)
	delete *i;
    unmapped_objects.clear();
}


std::unique_ptr<STEPDetachedEntityArena>
STEPWrapper::DetachEntityCache()
{
    std::unique_ptr<STEPDetachedEntityArena> arena(new STEPDetachedEntityArena());
    arena->objects.swap(entity_objects);
    arena->unmapped_objects.swap(unmapped_objects);
    return arena;
}


void
STEPWrapper::ReleaseSourceData()
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    if (lazy_session) {
	releaseLazyBatches();
	return;
    }
#endif
    ClearEntityCache();
}


bool
STEPWrapper::ShouldConvertEntity(int64_t entity_id)
{
    if (import_options.selected_entity_ids.empty()) return true;
    if (import_options.selected_entity_ids.find(entity_id) ==
	import_options.selected_entity_ids.end()) return false;
    statistics.selected_entity_ids_encountered.insert(entity_id);
    return true;
}


void
STEPWrapper::ResetOpenNURBSState()
{
    for (std::map<int, STEPEntity *>::iterator i = entity_objects.begin();
	 i != entity_objects.end(); ++i) {
	if (i->second)
	    i->second->ResetONState();
    }
    for (std::list<STEPEntity *>::iterator i = unmapped_objects.begin();
	 i != unmapped_objects.end(); ++i) {
	if (*i)
	    (*i)->ResetONState();
    }
}


void
STEPWrapper::RecordDiagnostic(brlcad::step::DiagnosticSeverity severity, int64_t entity_id,
    const std::string &entity_type, const std::string &attribute, const std::string &message)
{
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    for (std::vector<brlcad::step::Diagnostic>::iterator i = diagnostics.begin(); i != diagnostics.end(); ++i) {
	if (i->severity == severity && i->entity_id == entity_id && i->entity_type == entity_type &&
	    i->attribute == attribute && i->message == message) {
	    ++i->repeat_count;
	    return;
	}
    }
    brlcad::step::Diagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.entity_id = entity_id;
    diagnostic.entity_type = entity_type;
    diagnostic.attribute = attribute;
    diagnostic.message = message;
    diagnostics.insert(std::lower_bound(diagnostics.begin(), diagnostics.end(),
	diagnostic, diagnostic_less), diagnostic);
}


void
STEPWrapper::RecordRepair(int64_t entity_id, const std::string &entity_type,
    const std::string &attribute, const std::string &message)
{
    std::lock_guard<std::mutex> guard(diagnostic_mutex);
    ++statistics.repairs;
    for (std::vector<brlcad::step::Diagnostic>::iterator i = diagnostics.begin();
	 i != diagnostics.end(); ++i) {
	if (i->severity == brlcad::step::DiagnosticSeverity::Information &&
	    i->entity_id == entity_id && i->entity_type == entity_type &&
	    i->attribute == attribute && i->message == message) {
	    ++i->repeat_count;
	    return;
	}
    }
    brlcad::step::Diagnostic diagnostic;
    diagnostic.severity = brlcad::step::DiagnosticSeverity::Information;
    diagnostic.entity_id = entity_id;
    diagnostic.entity_type = entity_type;
    diagnostic.attribute = attribute;
    diagnostic.message = message;
    diagnostics.insert(std::lower_bound(diagnostics.begin(), diagnostics.end(),
	diagnostic, diagnostic_less), diagnostic);
}


void
STEPWrapper::collectEntityCounts()
{
    document.entity_counts.clear();
    document.unsupported_counts.clear();
    statistics.input_instances = static_cast<uint64_t>(InstanceCount());
    const auto record_type = [this](std::string type) {
	std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
	    return static_cast<char>(std::toupper(c));
	});
	++document.entity_counts[type];
	if (type.find("ANNOTATION") != std::string::npos || type.find("CALLOUT") != std::string::npos ||
	    type.find("CAMERA") != std::string::npos || type.find("DRAUGHTING") != std::string::npos ||
	    type.find("KINEMATIC") != std::string::npos || type.find("LIGHT_SOURCE") != std::string::npos ||
	    type.find("SECURITY_CLASSIFICATION") != std::string::npos || type.find("APPROVAL") != std::string::npos ||
	    type.find("DOCUMENT") != std::string::npos || type.find("DATUM") != std::string::npos ||
	    type.find("SYMBOL") != std::string::npos || type == "COMPOSITE_TEXT" ||
	    type.compare(0, 5, "TEXT_") == 0 || type.find("TOLERANCE") != std::string::npos)
	    ++document.unsupported_counts[type];
    };
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    if (lazy_session) {
	for (std::vector<uint64_t>::const_iterator id = lazy_instance_ids.begin();
	     id != lazy_instance_ids.end(); ++id) {
	    std::string type = lazy_session->TypeName(*id);
	    if (type.empty()) type = "COMPLEX_ENTITY";
	    record_type(type);
	}
	return;
    }
#endif
    for (int i = 0; i < InstanceCount(); ++i) {
	SDAI_Application_instance *instance = InstanceAt(i);
	if (!instance || !instance->EntityName()) continue;
	record_type(instance->EntityName());
    }
}


double
STEPWrapper::deriveTolerance()
{
    if (import_options.absolute_tolerance_mm > 0.0)
	return import_options.absolute_tolerance_mm;

    double result = 0.0;
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    if (lazy_session) {
	const std::vector<uint64_t> uncertainty_ids =
	    lazy_session->InstancesByType("GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT");
	for (std::vector<uint64_t>::const_iterator id = uncertainty_ids.begin();
	     id != uncertainty_ids.end(); ++id) {
	    SDAI_Application_instance *instance = activateLazyRoot(*id);
	    if (!instance) continue;
	    GlobalUncertaintyAssignedContext context(this, instance->STEPfile_id);
	    if (!context.Load(this, instance)) continue;
	    double candidate = context.GetLengthUncertainty();
	    if (candidate > 0.0 && (result <= 0.0 || candidate < result)) result = candidate;
	}
	releaseLazyBatches();
    } else
#endif
    for (int i = 0; i < InstanceCount(); ++i) {
	SDAI_Application_instance *instance = InstanceAt(i);
	if (!instance || instance->STEPfile_id <= 0 ||
	    !instance->IsA(SCHEMA_NAMESPACE::e_global_uncertainty_assigned_context)) continue;
	/* A representation context is commonly a complex entity that is also a
	 * geometric and unit context.  Factory selection intentionally chooses the
	 * geometric view for conversion, so materialize this uncertainty view
	 * explicitly instead of depending on complex-supertype dispatch order. */
	GlobalUncertaintyAssignedContext context(this, instance->STEPfile_id);
	if (!context.Load(this, instance)) continue;
	double candidate = context.GetLengthUncertainty();
	if (candidate > 0.0 && (result <= 0.0 || candidate < result)) result = candidate;
    }
    ClearEntityCache();

    // Until geometry has been detached there is no reliable model bounding box.
    // Use a millimetre-space numerical floor and retain the file's larger stated
    // uncertainty verbatim.
    const double numerical_floor = 1.0e-9;
    if (result <= 0.0) return 1.0e-6;
    return result > numerical_floor ? result : numerical_floor;
}

enum BrepWriteStatus {
    BREP_WRITE_SUCCESS = 0,
    BREP_CONVERSION_FAILED,
    BREP_INVALID_STRUCTURE,
    BREP_NOT_SOLID,
    BREP_OUTPUT_FAILED
};


static bool
remove_adjacent_zero_area_slit(ON_Brep *brep, int loop_index)
{
    if (!brep || loop_index < 0 || loop_index >= brep->m_L.Count())
	return false;
    ON_BrepLoop &loop = brep->m_L[loop_index];
    const int trim_count = loop.TrimCount();
    if (trim_count < 3)
	return false;

    for (int first_offset = 0; first_offset < trim_count; ++first_offset) {
	const int second_offset = (first_offset + 1) % trim_count;
	const int previous_offset = (first_offset + trim_count - 1) % trim_count;
	const int next_offset = (second_offset + 1) % trim_count;
	ON_BrepTrim *first = loop.Trim(first_offset);
	ON_BrepTrim *second = loop.Trim(second_offset);
	const ON_BrepTrim *previous = loop.Trim(previous_offset);
	const ON_BrepTrim *next = loop.Trim(next_offset);
	if (!first || !second || !previous || !next ||
		first->m_type != ON_BrepTrim::seam ||
		second->m_type != ON_BrepTrim::seam || first->m_ei < 0 ||
		first->m_ei != second->m_ei ||
		first->m_bRev3d == second->m_bRev3d ||
	first->m_ei >= brep->m_E.Count() ||
	brep->m_E[first->m_ei].m_ti.Count() != 2)
	    continue;
	/* The two exact uses cancel topologically.  Prefer an already exact
	 * exposed p-space join; otherwise admit only a shared topology vertex whose
	 * two surface lifts already agree within the asserted model uncertainty.
	 * The ordinary bounded endpoint pass must still close that join later. */
	if (previous->m_vi[1] != next->m_vi[0] || previous->m_vi[1] < 0 ||
		previous->m_vi[1] >= brep->m_V.Count())
	    continue;
	const double exposed_gap = previous->PointAtEnd().DistanceTo(
	    next->PointAtStart());
	if (exposed_gap > ON_ZERO_TOLERANCE) {
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface || !(LocalUnits::tolerance > 0.0))
		continue;
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint next_uv = next->PointAtStart();
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
	    const ON_3dPoint vertex = brep->m_V[previous->m_vi[1]].point;
	    if (!previous_lift.IsValid() || !next_lift.IsValid() ||
		    previous_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
		    next_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
		    previous_lift.DistanceTo(next_lift) > LocalUnits::tolerance)
		continue;
	}
	const int first_index = first->m_trim_index;
	const int second_index = second->m_trim_index;
	brep->DeleteTrim(brep->m_T[first_index], true);
	brep->DeleteTrim(brep->m_T[second_index], true);
	return brep->Compact();
    }
    return false;
}


struct SingularLoopClosure {
    bool valid = false;
    bool vertices_available = false;
    bool direct_collapse_tested = false;
    bool direct_collapses = false;
    ON_Surface::ISO iso = ON_Surface::not_iso;
    int vertex = -1;
    int other_vertex = -1;
    double topology_vertex_distance = DBL_MAX;
    double maximum_lift_distance = DBL_MAX;
    ON_3dPoint start = ON_3dPoint::UnsetPoint;
    ON_3dPoint end = ON_3dPoint::UnsetPoint;
};


static SingularLoopClosure
collapsed_singular_loop_closure(ON_Brep *brep, const ON_BrepLoop *loop,
    const ON_BrepTrim *last, const ON_BrepTrim *first)
{
    SingularLoopClosure result;
    if (!brep || !loop || !last || !first || last->m_vi[1] < 0 ||
	    last->m_vi[1] >= brep->m_V.Count() || first->m_vi[0] < 0 ||
	    first->m_vi[0] >= brep->m_V.Count())
	return result;

    result.vertices_available = true;
    result.vertex = last->m_vi[1];
    result.other_vertex = first->m_vi[0];
    const ON_3dPoint &topology_vertex = brep->m_V[result.vertex].point;
    result.topology_vertex_distance = topology_vertex.DistanceTo(
	brep->m_V[result.other_vertex].point);
    if (result.topology_vertex_distance > LocalUnits::tolerance)
	return result;

    const ON_BrepFace *face = loop->Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!surface) return result;

    result.start = last->PointAtEnd();
    result.end = first->PointAtStart();
    std::unique_ptr<ON_LineCurve> singular_curve(new ON_LineCurve(
	result.start, result.end));
    if (!singular_curve->ChangeDimension(2) || !singular_curve->IsValid())
	return result;

    result.iso = surface->IsIsoparametric(*singular_curve);
    bool boundary_iso = result.iso == ON_Surface::W_iso ||
	result.iso == ON_Surface::E_iso || result.iso == ON_Surface::S_iso ||
	result.iso == ON_Surface::N_iso;
    if (!boundary_iso) {
	const int varying_direction = fabs(result.end.y - result.start.y) >
	    fabs(result.end.x - result.start.x) ? 1 : 0;
	const int fixed_direction = 1 - varying_direction;
	const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	const double fixed_parameter = 0.5 *
	    (result.start[fixed_direction] + result.end[fixed_direction]);
	const int side = fabs(fixed_parameter - fixed_domain.Min()) <=
	    fabs(fixed_parameter - fixed_domain.Max()) ? 0 : 1;
	const ON_Surface::ISO expected_iso = fixed_direction == 0 ?
	    (side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
	    (side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);

	result.direct_collapse_tested = true;
	result.direct_collapses = true;
	for (int sample = 0; result.direct_collapses &&
		sample <= kDenseValidationSegments; ++sample) {
	    const ON_3dPoint uv = singular_curve->PointAt(
		singular_curve->Domain().ParameterAt(
		    static_cast<double>(sample) / kDenseValidationSegments));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    result.direct_collapses = lift.IsValid() &&
		lift.DistanceTo(topology_vertex) <= LocalUnits::tolerance;
	}
	if (result.direct_collapses) {
	    result.iso = expected_iso;
	    boundary_iso = true;
	} else {
	    result.start[fixed_direction] = fixed_domain[side];
	    result.end[fixed_direction] = fixed_domain[side];
	    std::unique_ptr<ON_LineCurve> boundary_curve(new ON_LineCurve(
		result.start, result.end));
	    if (boundary_curve->ChangeDimension(2) && boundary_curve->IsValid() &&
		    surface->IsIsoparametric(*boundary_curve) == expected_iso) {
		singular_curve = std::move(boundary_curve);
		result.iso = expected_iso;
		boundary_iso = true;
	    }
	}
    }
    if (!boundary_iso) return result;

    result.maximum_lift_distance = 0.0;
    result.valid = true;
    for (int sample = 0; result.valid && sample <= kDenseValidationSegments;
	    ++sample) {
	const ON_3dPoint uv = singular_curve->PointAt(
	    singular_curve->Domain().ParameterAt(
		static_cast<double>(sample) / kDenseValidationSegments));
	const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	const double distance = lift.IsValid() ?
	    lift.DistanceTo(topology_vertex) : DBL_MAX;
	result.maximum_lift_distance = std::max(
	    result.maximum_lift_distance, distance);
	result.valid = distance <= LocalUnits::tolerance;
    }
    return result;
}


static bool
periodic_loop_closure(const ON_Brep *brep, const ON_BrepLoop *loop,
    const ON_BrepTrim *last, const ON_BrepTrim *first,
    double parameter_tolerance)
{
    if (!brep || !loop || !last || !first || last->m_vi[1] < 0 ||
	    last->m_vi[1] >= brep->m_V.Count() || first->m_vi[0] < 0 ||
	    first->m_vi[0] >= brep->m_V.Count())
	return false;
    const ON_BrepFace *face = loop->Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!surface) return false;

    const ON_3dPoint last_uv = last->PointAtEnd();
    const ON_3dPoint first_uv = first->PointAtStart();
    bool one_period_apart = false;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction)) continue;
	const int other = 1 - direction;
	const double period = surface->Domain(direction).Length();
	const double period_tolerance = parameter_tolerance *
	    std::max(1.0, fabs(period));
	if (fabs(fabs(last_uv[direction] - first_uv[direction]) - period) <=
		period_tolerance &&
		fabs(last_uv[other] - first_uv[other]) <= parameter_tolerance) {
	    one_period_apart = true;
	    break;
	}
    }
    if (!one_period_apart) return false;

    const ON_3dPoint &last_vertex = brep->m_V[last->m_vi[1]].point;
    const ON_3dPoint &first_vertex = brep->m_V[first->m_vi[0]].point;
    if (last_vertex.DistanceTo(first_vertex) > LocalUnits::tolerance)
	return false;
    const ON_3dPoint last_lift = surface->PointAt(last_uv.x, last_uv.y);
    const ON_3dPoint first_lift = surface->PointAt(first_uv.x, first_uv.y);
    return last_lift.IsValid() && first_lift.IsValid() &&
	last_lift.DistanceTo(last_vertex) <= LocalUnits::tolerance &&
	first_lift.DistanceTo(first_vertex) <= LocalUnits::tolerance &&
	last_lift.DistanceTo(first_lift) <= LocalUnits::tolerance;
}


static bool
opposite_trim_curves_coincide(const ON_BrepTrim *first,
    const ON_BrepTrim *second, double parameter_tolerance)
{
    if (!first || !second) return false;
    const ON_Interval first_domain = first->Domain();
    const ON_Interval second_domain = second->Domain();
    for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	const double fraction = static_cast<double>(sample) /
	    kDenseValidationSegments;
	const ON_3dPoint first_uv = first->PointAt(
	    first_domain.ParameterAt(fraction));
	const ON_3dPoint second_uv = second->PointAt(
	    second_domain.ParameterAt(1.0 - fraction));
	if (!first_uv.IsValid() || !second_uv.IsValid() ||
		first_uv.DistanceTo(second_uv) > parameter_tolerance)
	    return false;
    }
    return true;
}


static bool
split_keyhole_loop(ON_Brep *brep, int loop_index, std::string *failure_reason)
{
    if (failure_reason)
	failure_reason->clear();
    if (!brep || loop_index < 0 || loop_index >= brep->m_L.Count())
	return false;
    ON_BrepLoop *loop = &brep->m_L[loop_index];
    if ((loop->m_type != ON_BrepLoop::outer && loop->m_type != ON_BrepLoop::inner) ||
	loop->TrimCount() < 4)
	return false;

    /* Pcurve endpoints are computed independently and may differ by a few
     * ulps even when they describe the same parameter-space point.  This
     * floor is deliberately parameter-space numerical noise, not the model
     * uncertainty (which is measured in output-space millimetres). */
    const double parameter_tolerance =
	ON_ZERO_TOLERANCE * kNumericalToleranceScale;

    int first_offset = -1;
    int second_offset = -1;
    bool insert_inside_singular = false;
    bool insert_outside_singular = false;
    bool inside_periodic = false;
    bool outside_periodic = false;
    bool remove_periodic_duplicate_only = false;
    SingularLoopClosure inside_singular;
    SingularLoopClosure outside_singular;
    double best_failed_inside_gap = DBL_MAX;
    double best_failed_outside_gap = DBL_MAX;
    std::ostringstream rejected_candidates;
    size_t rejected_candidate_count = 0;
    const int trim_count = loop->TrimCount();
    for (int first = 0; first < trim_count && first_offset < 0; ++first) {
	const ON_BrepTrim *first_trim = loop->Trim(first);
	if (!first_trim ||
	    (first_trim->m_type != ON_BrepTrim::seam &&
	     first_trim->m_type != ON_BrepTrim::slit) ||
	    first_trim->m_ei < 0 || first_trim->m_ei >= brep->m_E.Count())
	    continue;
	for (int second = first + 2; second < trim_count; ++second) {
	    if (first == 0 && second == trim_count - 1)
		continue;
	    const ON_BrepTrim *second_trim = loop->Trim(second);
	    if (!second_trim ||
		(second_trim->m_type != ON_BrepTrim::seam &&
		 second_trim->m_type != ON_BrepTrim::slit) ||
		second_trim->m_ei != first_trim->m_ei ||
		second_trim->m_bRev3d == first_trim->m_bRev3d)
		continue;
	    if (!opposite_trim_curves_coincide(first_trim, second_trim,
		    parameter_tolerance))
		continue;

	    /* Removing the oppositely traversed bridge must expose two loops that
	     * are already closed to parameter-space numerical precision.  This
	     * proves the bridge is a zero-area keyhole connector; no model-space
	     * gap is synthesized. */
	    const ON_BrepTrim *inside_first = loop->Trim(first + 1);
	    const ON_BrepTrim *inside_last = loop->Trim(second - 1);
	    const ON_BrepTrim *outside_first = loop->Trim((second + 1) % trim_count);
	    const ON_BrepTrim *outside_last = loop->Trim((first + trim_count - 1) % trim_count);
	    const double inside_gap = inside_first && inside_last ?
		inside_last->PointAtEnd().DistanceTo(inside_first->PointAtStart()) : DBL_MAX;
	    const double outside_gap = outside_first && outside_last ?
		outside_last->PointAtEnd().DistanceTo(outside_first->PointAtStart()) : DBL_MAX;
	    SingularLoopClosure candidate_inside_singular;
	    SingularLoopClosure candidate_outside_singular;
	    const bool candidate_inside_periodic = inside_gap > parameter_tolerance &&
		periodic_loop_closure(brep, loop, inside_last, inside_first,
		    parameter_tolerance);
	    const bool candidate_outside_periodic = outside_gap > parameter_tolerance &&
		periodic_loop_closure(brep, loop, outside_last, outside_first,
		    parameter_tolerance);
	    if (inside_gap > parameter_tolerance && !candidate_inside_periodic)
		candidate_inside_singular = collapsed_singular_loop_closure(
		    brep, loop, inside_last, inside_first);
	    if (outside_gap > parameter_tolerance && !candidate_outside_periodic)
		candidate_outside_singular = collapsed_singular_loop_closure(
		    brep, loop, outside_last, outside_first);
	    const bool inside_closed = inside_first && inside_last &&
		(inside_gap <= parameter_tolerance || candidate_inside_periodic ||
		 candidate_inside_singular.valid);
	    const bool outside_closed = outside_first && outside_last &&
		(outside_gap <= parameter_tolerance || candidate_outside_periodic ||
		 candidate_outside_singular.valid);
	    if (inside_closed && outside_closed) {
		first_offset = first;
		second_offset = second;
		insert_inside_singular = candidate_inside_singular.valid;
		insert_outside_singular = candidate_outside_singular.valid;
		inside_periodic = candidate_inside_periodic;
		outside_periodic = candidate_outside_periodic;
		remove_periodic_duplicate_only = candidate_inside_periodic &&
		    candidate_outside_periodic;
		inside_singular = candidate_inside_singular;
		outside_singular = candidate_outside_singular;
		break;
	    }
	    const bool better_failure = inside_gap < best_failed_inside_gap ||
		(fabs(inside_gap - best_failed_inside_gap) <= parameter_tolerance &&
		 outside_gap < best_failed_outside_gap);
	    if (failure_reason && better_failure) {
		best_failed_inside_gap = inside_gap;
		best_failed_outside_gap = outside_gap;
		*failure_reason = "best candidate bridge trims " +
		    std::to_string(first_trim->m_trim_index) + "/" +
		    std::to_string(second_trim->m_trim_index) +
		    " left p-space gaps " + std::to_string(inside_gap) + "/" +
		    std::to_string(outside_gap) + ", periodic closure " +
		    (candidate_inside_periodic ? "yes" : "no") + "/" +
		    (candidate_outside_periodic ? "yes" : "no") +
		    ", singular closure " +
		    (candidate_inside_singular.valid ? "yes" : "no") + "/" +
		    (candidate_outside_singular.valid ? "yes" : "no") +
		    ", maximum singular lift " +
		    std::to_string(candidate_inside_singular.maximum_lift_distance) +
		    "/" +
		    std::to_string(candidate_outside_singular.maximum_lift_distance);
	    }
	    if (rejected_candidate_count < 8) {
		rejected_candidates
		    << (rejected_candidate_count ? "; " : "")
		    << first_trim->m_trim_index << "/"
		    << second_trim->m_trim_index
		    << " inside=" << inside_gap
		    << " outside=" << outside_gap
		    << " periodic=" << (candidate_inside_periodic ? "yes" : "no")
		    << "/" << (candidate_outside_periodic ? "yes" : "no")
		    << " singular=" << (candidate_inside_singular.valid ? "yes" : "no")
		    << "/" << (candidate_outside_singular.valid ? "yes" : "no");
		++rejected_candidate_count;
	    }
	    continue;
	}
    }

    if (first_offset < 0 || second_offset < 0) {
	if (failure_reason && rejected_candidate_count)
	    *failure_reason += "; rejected candidates: " +
		rejected_candidates.str();
	return false;
    }

    if (remove_periodic_duplicate_only) {
	const int first_trim_index = loop->m_ti[first_offset];
	const int second_trim_index = loop->m_ti[second_offset];
	const int higher_trim = std::max(first_trim_index, second_trim_index);
	const int lower_trim = std::min(first_trim_index, second_trim_index);
	/* Each STEP use of a surface-seam edge may have produced a trim on both
	 * sides of the parameter domain.  When the two coincident reverse trims
	 * are only a duplicate bridge between periodic chains, remove that pair
	 * from the original loop.  The remaining seam pair still owns the edge. */
	brep->DeleteTrim(brep->m_T[higher_trim], false);
	brep->DeleteTrim(brep->m_T[lower_trim], false);
	return brep->Compact();
    }

    const ON_BrepLoop::TYPE original_type = loop->m_type;
    const int face_index = loop->m_fi;
    if (face_index < 0 || face_index >= brep->m_F.Count())
	return false;
    const int bridge_trim_1 = loop->m_ti[first_offset];
    const int bridge_trim_2 = loop->m_ti[second_offset];

    std::vector<int> inside_trims;
    std::vector<int> outside_trims;
    for (int offset = first_offset + 1; offset < second_offset; ++offset)
	inside_trims.push_back(loop->m_ti[offset]);
    for (int offset = second_offset + 1; offset < trim_count; ++offset)
	outside_trims.push_back(loop->m_ti[offset]);
    for (int offset = 0; offset < first_offset; ++offset)
	outside_trims.push_back(loop->m_ti[offset]);
    if (inside_trims.empty() || outside_trims.empty()) {
	if (failure_reason)
	    *failure_reason = "candidate keyhole bridge did not enclose two trim chains";
	return false;
    }

    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));

    const auto combine_periodic_vertices = [&](const std::vector<int> &trims) {
	if (trims.empty()) return false;
	const int last_vertex = brep->m_T[trims.back()].m_vi[1];
	const int first_vertex = brep->m_T[trims.front()].m_vi[0];
	if (last_vertex < 0 || last_vertex >= brep->m_V.Count() ||
		first_vertex < 0 || first_vertex >= brep->m_V.Count())
	    return false;
	return last_vertex == first_vertex || brep->CombineCoincidentVertices(
	    brep->m_V[last_vertex], brep->m_V[first_vertex]);
    };
    if ((inside_periodic && !combine_periodic_vertices(inside_trims)) ||
	    (outside_periodic && !combine_periodic_vertices(outside_trims))) {
	*brep = *rollback;
	return false;
    }

    int inside_singular_c2 = -1;
    int inside_singular_vertex = -1;
    if (insert_inside_singular) {
	std::unique_ptr<ON_LineCurve> singular_curve(new ON_LineCurve(
	    inside_singular.start, inside_singular.end));
	if (!singular_curve->ChangeDimension(2) || !singular_curve->IsValid()) {
	    *brep = *rollback;
	    return false;
	}
	inside_singular_c2 = brep->AddTrimCurve(singular_curve.release());
	if (inside_singular_c2 < 0) {
	    *brep = *rollback;
	    return false;
	}
	const int inside_last_trim = inside_trims.back();
	const int inside_first_trim = inside_trims.front();
	inside_singular_vertex = brep->m_T[inside_last_trim].m_vi[1];
	const int other_vertex = brep->m_T[inside_first_trim].m_vi[0];
	if (other_vertex != inside_singular_vertex &&
		!brep->CombineCoincidentVertices(
		    brep->m_V[inside_singular_vertex],
		    brep->m_V[other_vertex])) {
	    *brep = *rollback;
	    return false;
	}
	inside_singular_vertex = brep->m_T[inside_last_trim].m_vi[1];
    }

    int outside_singular_c2 = -1;
    if (insert_outside_singular) {
	std::unique_ptr<ON_LineCurve> singular_curve(new ON_LineCurve(
	    outside_singular.start, outside_singular.end));
	if (!singular_curve->ChangeDimension(2) || !singular_curve->IsValid()) {
	    *brep = *rollback;
	    return false;
	}
	outside_singular_c2 = brep->AddTrimCurve(singular_curve.release());
	if (outside_singular_c2 < 0) {
	    *brep = *rollback;
	    return false;
	}
	int outside_singular_vertex = brep->m_T[outside_trims.back()].m_vi[1];
	const int outside_singular_other_vertex =
	    brep->m_T[outside_trims.front()].m_vi[0];
	if (outside_singular_other_vertex != outside_singular_vertex &&
		!brep->CombineCoincidentVertices(
		    brep->m_V[outside_singular_vertex],
		    brep->m_V[outside_singular_other_vertex])) {
	    *brep = *rollback;
	    return false;
	}
	outside_singular_vertex = brep->m_T[outside_trims.back()].m_vi[1];
	outside_singular.vertex = outside_singular_vertex;
    }

    const int new_loop_index = brep->NewLoop(original_type).m_loop_index;
    loop = &brep->m_L[loop_index];
    ON_BrepLoop *new_loop = &brep->m_L[new_loop_index];
    loop->m_ti.SetCount(0);
    for (std::vector<int>::const_iterator trim = outside_trims.begin();
	 trim != outside_trims.end(); ++trim) {
	loop->m_ti.Append(*trim);
	brep->m_T[*trim].m_li = loop_index;
    }
    for (std::vector<int>::const_iterator trim = inside_trims.begin();
	 trim != inside_trims.end(); ++trim) {
	new_loop->m_ti.Append(*trim);
	brep->m_T[*trim].m_li = new_loop_index;
    }
    loop->m_fi = face_index;
    new_loop->m_fi = face_index;
    if (insert_outside_singular) {
	brep->NewSingularTrim(
	    brep->m_V[outside_singular.vertex], *loop,
	    outside_singular.iso, outside_singular_c2);
	loop = &brep->m_L[loop_index];
	new_loop = &brep->m_L[new_loop_index];
    }
    if (insert_inside_singular) {
	brep->NewSingularTrim(
	    brep->m_V[inside_singular_vertex], *new_loop,
	    inside_singular.iso, inside_singular_c2);
	loop = &brep->m_L[loop_index];
	new_loop = &brep->m_L[new_loop_index];
    }
    loop->m_type = brep->ComputeLoopType(*loop);
    new_loop->m_type = brep->ComputeLoopType(*new_loop);

    const bool original_loop_valid = loop->m_type == ON_BrepLoop::outer ||
	loop->m_type == ON_BrepLoop::inner;
    const bool new_loop_valid = new_loop->m_type == ON_BrepLoop::outer ||
	new_loop->m_type == ON_BrepLoop::inner;
    const bool face_topology_valid = original_type == ON_BrepLoop::outer ?
	(loop->m_type == ON_BrepLoop::outer || new_loop->m_type == ON_BrepLoop::outer) :
	(loop->m_type == ON_BrepLoop::inner && new_loop->m_type == ON_BrepLoop::inner);
    if (!original_loop_valid || !new_loop_valid || !face_topology_valid) {
	if (failure_reason) {
	    std::ostringstream details;
	    const ON_BrepFace &candidate_face = brep->m_F[face_index];
	    const ON_Surface *candidate_surface = candidate_face.SurfaceOf();
	    details << "candidate split produced loop types "
		<< static_cast<int>(loop->m_type) << "/"
		<< static_cast<int>(new_loop->m_type)
		<< " from original type " << static_cast<int>(original_type)
		<< ", directions " << brep->LoopDirection(*loop) << "/"
		<< brep->LoopDirection(*new_loop)
		<< ", face " << face_index
		<< ", STEP loop " << loop->m_loop_user.i
		<< ", face loops " << candidate_face.LoopCount()
		<< ", face reversed " << (candidate_face.m_bRev ? "yes" : "no");
	    if (candidate_surface) {
		const ON_ClassId *class_id = candidate_surface->ClassId();
		details << ", surface " << (class_id ? class_id->ClassName() : "unknown")
		    << " closed=" << (candidate_surface->IsClosed(0) ? "1" : "0")
		    << (candidate_surface->IsClosed(1) ? "1" : "0");
	    }
	    const ON_BrepLoop *candidate_loops[2] = {loop, new_loop};
	    for (int candidate = 0; candidate < 2; ++candidate) {
		details << ", chain" << candidate << "=[";
		const ON_BrepLoop *candidate_loop = candidate_loops[candidate];
		for (int lti = 0; lti < candidate_loop->TrimCount(); ++lti) {
		    const ON_BrepTrim *candidate_trim = candidate_loop->Trim(lti);
		    if (lti > 0)
			details << " ";
		    if (!candidate_trim) {
			details << "invalid";
			continue;
		    }
		    const ON_3dPoint start = candidate_trim->PointAtStart();
		    const ON_3dPoint end = candidate_trim->PointAtEnd();
		    const ON_Curve *trim_curve = candidate_trim->TrimCurveOf();
		    const ON_BrepEdge *candidate_edge = candidate_trim->Edge();
		    double sampled_area = 0.0;
		    if (trim_curve) {
			const ON_Interval domain = candidate_trim->Domain();
			ON_3dPoint previous = candidate_trim->PointAt(domain.Min());
			for (int sample = 1; sample <= 128; ++sample) {
			    const ON_3dPoint current = candidate_trim->PointAt(
				domain.ParameterAt(static_cast<double>(sample) / 128.0));
			    sampled_area += previous.x * current.y - current.x * previous.y;
			    previous = current;
			}
			sampled_area *= 0.5;
		    }
		    details << candidate_trim->m_trim_index
			<< "(rev=" << (candidate_trim->m_bRev3d ? "1" : "0")
			<< ",type=" << static_cast<int>(candidate_trim->m_type)
			<< ",iso=" << static_cast<int>(candidate_trim->m_iso)
			<< ",edge=" << candidate_trim->m_ei
			<< "/STEP" << (candidate_edge ? candidate_edge->m_edge_user.i : 0)
			<< ",c2=" << (trim_curve && trim_curve->ClassId() ?
			    trim_curve->ClassId()->ClassName() : "none")
			<< ",box=" << candidate_trim->m_pbox.m_min.x << ":"
			<< candidate_trim->m_pbox.m_max.x << ","
			<< candidate_trim->m_pbox.m_min.y << ":"
			<< candidate_trim->m_pbox.m_max.y
			<< ",area=" << sampled_area
			<< "," << start.x << ":" << start.y
			<< "->" << end.x << ":" << end.y << ")";
		}
		details << "]";
	    }
	    *failure_reason = details.str();
	}
	*brep = *rollback;
	return false;
    }

    ON_BrepFace *face = &brep->m_F[face_index];
    if (loop->m_type == original_type && new_loop->m_type == ON_BrepLoop::inner) {
	face->m_li.Append(new_loop_index);
    } else if (original_type == ON_BrepLoop::outer &&
	loop->m_type == ON_BrepLoop::inner && new_loop->m_type == ON_BrepLoop::outer) {
	face->m_li.Insert(0, new_loop_index);
    } else if (loop->m_type == original_type && new_loop->m_type == ON_BrepLoop::outer) {
	const int surface_index = face->m_si;
	const bool reversed = face->m_bRev;
	ON_BrepFace &new_face = brep->NewFace(surface_index);
	new_face.m_bRev = reversed;
	new_face.m_li.Append(new_loop_index);
	brep->m_L[new_loop_index].m_fi = new_face.m_face_index;
    } else {
	/* This is the remaining valid keyhole arrangement: the original loop
	 * becomes outer and the detached loop remains inner on the same face. */
	face->m_li.Append(new_loop_index);
    }

    brep->m_T[bridge_trim_1].m_li = -1;
    brep->m_T[bridge_trim_2].m_li = -1;
    const int bridge_edge = brep->m_T[bridge_trim_1].m_ei;
    bool bridge_owns_edge = bridge_edge >= 0 && bridge_edge < brep->m_E.Count();
    for (std::vector<int>::const_iterator trim = outside_trims.begin();
	 bridge_owns_edge && trim != outside_trims.end(); ++trim)
	if (brep->m_T[*trim].m_ei == bridge_edge) bridge_owns_edge = false;
    for (std::vector<int>::const_iterator trim = inside_trims.begin();
	 bridge_owns_edge && trim != inside_trims.end(); ++trim)
	if (brep->m_T[*trim].m_ei == bridge_edge) bridge_owns_edge = false;
    /* A closed-surface keyhole can contribute two extra trims to an otherwise
     * legitimate seam edge.  Preserve that edge and its remaining seam pair;
     * delete the edge only when the bridge pair are its final uses. */
    const int higher_bridge_trim = std::max(bridge_trim_1, bridge_trim_2);
    const int lower_bridge_trim = std::min(bridge_trim_1, bridge_trim_2);
    /* Delete the higher array index first because openNURBS compacts the trim
     * array immediately and renumbers the remaining topology references. */
    brep->DeleteTrim(brep->m_T[higher_bridge_trim], false);
    brep->DeleteTrim(brep->m_T[lower_bridge_trim], bridge_owns_edge);
    return brep->Compact();
}


static size_t
classify_exact_polyline_seams(ON_Brep *brep)
{
    if (!brep)
	return 0;
    size_t classified = 0;
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_type != ON_BrepTrim::seam ||
		(trim.m_iso == ON_Surface::W_iso || trim.m_iso == ON_Surface::E_iso ||
		 trim.m_iso == ON_Surface::S_iso || trim.m_iso == ON_Surface::N_iso) ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count())
	    continue;
	const int face_index = brep->m_L[trim.m_li].m_fi;
	if (face_index < 0 || face_index >= brep->m_F.Count())
	    continue;
	const ON_Surface *surface = brep->m_F[face_index].SurfaceOf();
	const ON_PolylineCurve *polyline = ON_PolylineCurve::Cast(trim.TrimCurveOf());
	if (!surface || !polyline || polyline->m_pline.Count() < 2)
	    continue;
	for (int direction = 0; direction < 2 && trim.m_iso == ON_Surface::not_iso;
	     ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = surface->Domain(direction);
	    for (int side = 0; side < 2; ++side) {
		const double boundary = domain[side];
		bool exact = true;
		for (int point = 0; point < polyline->m_pline.Count(); ++point) {
		    if (fabs(polyline->m_pline[point][direction] - boundary) >
			    ON_ZERO_TOLERANCE) {
			exact = false;
			break;
		    }
		}
		if (!exact)
		    continue;
		if (direction == 0)
		    trim.m_iso = side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso;
		else
		    trim.m_iso = side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso;
		++classified;
		break;
	    }
	}
    }
    return classified;
}


static size_t
finalize_brep_topology(ON_Brep *brep, bool normalize_keyholes,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	bool *topology_changed = NULL)
{
    if (topology_changed) *topology_changed = false;
    if (!brep)
	return 0;

    /* FACE_OUTER_BOUND and FACE_BOUND are authoritative.  Recomputing loop
     * types from still-unrepaired pcurves can turn a declared outer loop into
     * an inner loop and make an otherwise exact face structurally invalid.
     * Trim types still need the complete edge-use graph. */
    brep->SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, false, true);
    classify_exact_polyline_seams(brep);
    size_t keyhole_splits = 0;
    size_t keyhole_rejections = 0;
    const size_t keyhole_diagnostic_limit = 16;
    if (normalize_keyholes) {
	bool removed_slit = true;
	while (removed_slit) {
	    removed_slit = false;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		if (!remove_adjacent_zero_area_slit(brep, li))
		    continue;
		if (wrapper)
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"removed an exact zero-area adjacent seam bridge");
		if (topology_changed) *topology_changed = true;
		brep->SetTolerancesBoxesAndFlags(false, false, false, false,
		    true, true, false, true);
		classify_exact_polyline_seams(brep);
		removed_slit = true;
		break;
	    }
	}
	bool changed = true;
	while (changed) {
	    changed = false;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		std::string split_failure;
		if (split_keyhole_loop(brep, li, &split_failure)) {
		    ++keyhole_splits;
		    if (topology_changed) *topology_changed = true;
		    brep->SetTolerancesBoxesAndFlags(false, false, false, false,
			true, true, false, true);
		    classify_exact_polyline_seams(brep);
		    changed = true;
		    break;
		}
		if (wrapper && wrapper->Verbose() && !split_failure.empty()) {
		    ++keyhole_rejections;
		    if (keyhole_rejections <= keyhole_diagnostic_limit) {
			std::cerr << entity_type << " #" << entity_id << ": keyhole loop "
			    << li << " split rejected: " << split_failure << std::endl;
			wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
			    entity_id, entity_type, "edge_loop",
			    "keyhole split rejected: " + split_failure);
		    }
		}
	    }
	}
    }
    if (wrapper && wrapper->Verbose() && keyhole_rejections > keyhole_diagnostic_limit)
	std::cerr << entity_type << " #" << entity_id << ": suppressed "
	    << keyhole_rejections - keyhole_diagnostic_limit
	    << " additional exact keyhole split rejections ("
	    << keyhole_rejections << " total)" << std::endl;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi)
	brep->SortFaceLoops(brep->m_F[fi]);
    return keyhole_splits;
}


static void
closed_trim_endpoint_alignments(const ON_BrepTrim &trim, const ON_BrepEdge &edge,
	const ON_Surface *surface, double alignment[2])
{
    alignment[0] = ON_UNSET_VALUE;
    alignment[1] = ON_UNSET_VALUE;
    if (!surface)
	return;
    const ON_Interval trim_domain = trim.Domain();
    for (int end = 0; end < 2; ++end) {
	ON_3dPoint uv, lifted_point;
	ON_3dVector uv_tangent, du, dv;
	const double trim_parameter = trim_domain[end];
	const double edge_parameter = edge.Domain()[trim.m_bRev3d ? 1 - end : end];
	if (!trim.Ev1Der(trim_parameter, uv, uv_tangent) ||
		!surface->Ev1Der(uv.x, uv.y, lifted_point, du, dv))
	    continue;
	ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	ON_3dVector edge_tangent = edge.TangentAt(edge_parameter);
	if (!lifted_tangent.Unitize() || !edge_tangent.Unitize())
	    continue;
	alignment[end] = lifted_tangent * edge_tangent;
	if (trim.m_bRev3d)
	    alignment[end] = -alignment[end];
    }
}


static bool
refine_surface_pullback_seeded(const ON_Surface *surface, const ON_3dPoint &target,
	double tolerance, ON_3dPoint &uv, double *final_distance,
	bool confine_to_domain = false)
{
    if (final_distance)
	*final_distance = DBL_MAX;
    if (!surface || !target.IsValid() || !uv.IsValid() || !(tolerance > 0.0))
	return false;

    if (confine_to_domain) {
	for (int direction = 0; direction < 2; ++direction) {
	    const ON_Interval domain = surface->Domain(direction);
	    uv[direction] = std::max(domain.Min(),
		std::min(domain.Max(), uv[direction]));
	}
    }

    double best_distance = DBL_MAX;
    for (int iteration = 0; iteration < 32; ++iteration) {
	ON_3dPoint lifted;
	ON_3dVector du, dv;
	if (!surface->Ev1Der(uv.x, uv.y, lifted, du, dv) || !lifted.IsValid())
	    break;
	const ON_3dVector residual = target - lifted;
	best_distance = residual.Length();
	if (best_distance <= tolerance) {
	    if (final_distance)
		*final_distance = best_distance;
	    return true;
	}

	const double a = du * du;
	const double b = du * dv;
	const double c = dv * dv;
	const double r0 = du * residual;
	const double r1 = dv * residual;
	const double metric_scale = std::max(1.0, std::max(a, c));
	const double damping = metric_scale * 1.0e-14;
	const double aa = a + damping;
	const double cc = c + damping;
	const double determinant = aa * cc - b * b;
	if (fabs(determinant) <= DBL_EPSILON * metric_scale * metric_scale)
	    break;
	double delta[2] = {(cc * r0 - b * r1) / determinant,
	    (aa * r1 - b * r0) / determinant};
	double trust_scale = 1.0;
	for (int direction = 0; direction < 2; ++direction) {
	    const double maximum_step = 0.125 * surface->Domain(direction).Length();
	    if (maximum_step > ON_ZERO_TOLERANCE && fabs(delta[direction]) > maximum_step)
		trust_scale = std::min(trust_scale, maximum_step / fabs(delta[direction]));
	}

	bool improved = false;
	for (int reduction = 0; reduction < 12; ++reduction) {
	    const double scale = trust_scale * std::ldexp(1.0, -reduction);
	    ON_3dPoint candidate(uv.x + scale * delta[0],
		uv.y + scale * delta[1], 0.0);
	    for (int direction = 0; direction < 2; ++direction) {
		if (!confine_to_domain && surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		candidate[direction] = std::max(domain.Min(),
		    std::min(domain.Max(), candidate[direction]));
	    }
	    const ON_3dPoint candidate_lift = surface->PointAt(candidate.x, candidate.y);
	    if (!candidate_lift.IsValid())
		continue;
	    const double candidate_distance = candidate_lift.DistanceTo(target);
	    if (candidate_distance < best_distance) {
		uv = candidate;
		best_distance = candidate_distance;
		improved = true;
		break;
	    }
	}
	if (!improved)
	    break;
    }
    if (final_distance)
	*final_distance = best_distance;
    return best_distance <= tolerance;
}


static void
normalize_closed_surface_parameter(const ON_Surface *surface,
	const ON_3dPoint &target, double tolerance, ON_3dPoint &uv)
{
    if (!surface || !target.IsValid() || !uv.IsValid() || !(tolerance > 0.0))
	return;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		(uv[direction] >= domain.Min() && uv[direction] <= domain.Max()))
	    continue;
	double wrapped = fmod(uv[direction] - domain.Min(), period);
	if (wrapped < 0.0)
	    wrapped += period;
	ON_3dPoint candidate = uv;
	candidate[direction] = domain.Min() + wrapped;
	const ON_3dPoint lifted = surface->PointAt(candidate.x, candidate.y);
	if (lifted.IsValid() && lifted.DistanceTo(target) <= tolerance)
	    uv = candidate;
    }
}


static bool
has_unwrapped_closed_parameter(const ON_Surface *surface, const ON_3dPoint &uv)
{
    if (!surface || !uv.IsValid())
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double guard = std::max(ON_ZERO_TOLERANCE,
	    domain.Length() * 1.0e-12);
	if (uv[direction] < domain.Min() - guard ||
		uv[direction] > domain.Max() + guard)
	    return true;
    }
    return false;
}


struct PeriodicPullbackCrossing {
    bool detected = false;
    double trim_fraction = 0.0;
    int surface_direction = -1;
};


static bool
regenerate_trim_polyline(ON_Brep *brep, ON_BrepTrim &trim,
	const ON_Surface *surface, const ON_NurbsCurve &edge_nurbs, double tolerance,
	std::string *failure_reason, PeriodicPullbackCrossing *periodic_crossing = NULL,
	const ON_3dPoint *required_start = NULL,
	const ON_3dPoint *required_end = NULL,
	bool prefer_edge_driven = false)
{
    if (failure_reason)
	failure_reason->clear();
    if (periodic_crossing)
	*periodic_crossing = PeriodicPullbackCrossing();
    if (!brep || !surface || !(tolerance > 0.0))
	return false;

    const ON_BrepEdge *edge = trim.Edge();
    if (!edge)
	return false;
    const ON_Interval trim_domain = trim.Domain();
    brlcad::PullbackContext pullback_context;
    for (int regeneration_mode = 0; regeneration_mode < 2; ++regeneration_mode) {
    const bool edge_driven = prefer_edge_driven ? regeneration_mode == 0 :
	regeneration_mode == 1;
    const int maximum_segments = edge_driven ? 64 : kDenseValidationSegments;
    for (int segment_count = 64; segment_count <= maximum_segments; segment_count *= 2) {
	std::string rejection = "unknown rejection";
	ON_3dPointArray points;
	points.Reserve(segment_count + 1);
	std::vector<double> normalized_parameters;
	normalized_parameters.reserve(segment_count + 3);
	bool valid = true;
	for (int sample = 0; sample <= segment_count; ++sample) {
	    const double normalized = static_cast<double>(sample) /
		static_cast<double>(segment_count);
	    const ON_3dPoint *required_endpoint = sample == 0 ? required_start :
		(sample == segment_count ? required_end : NULL);
	    const ON_3dPoint source_uv = required_endpoint ? *required_endpoint :
		trim.PointAt(trim_domain.ParameterAt(normalized));
	    ON_3dPoint uv = source_uv;
	    if (edge_driven && sample > 0 && sample < segment_count) {
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		const double pullback_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		double pullback_distance = DBL_MAX;
		/* An exact surface point can have several parameter-space preimages at
		 * a pole or collapsed boundary.  Continue from the preceding exact
		 * sample before consulting the independently parameterized source
		 * pcurve, so adjacent samples remain on one lift-equivalent branch. */
		const bool have_continuation_seed = points.Count() > 0;
		if (have_continuation_seed)
		    uv = points[points.Count() - 1];
		bool pulled = refine_surface_pullback_seeded(surface, edge_point,
		    pullback_tolerance, uv, &pullback_distance,
		    !has_unwrapped_closed_parameter(surface, uv));
		if (!pulled && have_continuation_seed) {
		    uv = source_uv;
		    pulled = refine_surface_pullback_seeded(surface, edge_point,
			pullback_tolerance, uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, uv));
		}
		if (!pulled) {
		    ON_2dPoint pulled_uv;
		    ON_3dPoint pulled_lift;
		    if (!pullback_context.SurfaceClosestPoint(surface, edge_point, pulled_uv,
			    pulled_lift, pullback_distance, 0,
			    std::max(ON_ZERO_TOLERANCE, pullback_tolerance * 0.1),
			    pullback_tolerance) || pullback_distance > pullback_tolerance) {
			rejection = "exact edge pullback failed";
			valid = false;
			break;
		    }
		    uv.Set(pulled_uv.x, pulled_uv.y, 0.0);
		    /* Select the whole-domain branch nearest the supplied pcurve, but
		     * retain a shift only when its lift proves it is the same exact edge
		     * point.  This also handles imported periodic geometry whose closure
		     * flag was not retained by the NURBS representation. */
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round(
			    (source_uv[direction] - uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = surface->PointAt(
			    shifted_uv.x, shifted_uv.y);
			if (shifted_lift.IsValid() &&
				shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			    uv = shifted_uv;
		    }
		}
		/* Keep consecutive exact samples on one lift-equivalent domain branch.
		 * The supplied curve may legitimately cross a parameter seam; using its
		 * independently wrapped values would create a full-domain chord. */
		if (points.Count() > 0) {
		    const ON_3dPoint previous_uv = points[points.Count() - 1];
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round((previous_uv[direction] -
			    uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = surface->PointAt(
			    shifted_uv.x, shifted_uv.y);
			if (shifted_lift.IsValid() &&
				shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			    uv = shifted_uv;
		    }
		}
	    } else if (edge_driven) {
		/* Endpoints are shared with adjacent trims in the same p-space loop.
		 * Refine them to the authoritative edge endpoint when possible; a
		 * merely in-tolerance source endpoint can otherwise make the final
		 * polyline chord backtrack.  If exact refinement is unavailable,
		 * preserve an already valid source endpoint for the bounded join pass. */
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		const ON_3dPoint endpoint_lift = surface->PointAt(uv.x, uv.y);
		const bool source_endpoint_valid = endpoint_lift.IsValid() &&
		    endpoint_lift.DistanceTo(edge_point) <= tolerance;
		const double pullback_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance * 0.1);
		ON_3dPoint refined_uv = uv;
		double pullback_distance = DBL_MAX;
		bool pulled = required_endpoint && source_endpoint_valid;
		if (pulled)
		    pullback_distance = endpoint_lift.DistanceTo(edge_point);
		else
		    pulled = refine_surface_pullback_seeded(surface, edge_point,
			pullback_tolerance, refined_uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, refined_uv));
		if (!pulled) {
		    ON_2dPoint pulled_uv;
		    ON_3dPoint pulled_lift;
		    pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
			pulled_uv, pulled_lift, pullback_distance, 0,
			std::max(ON_ZERO_TOLERANCE, pullback_tolerance * 0.1),
			pullback_tolerance) && pullback_distance <= pullback_tolerance;
		    if (pulled)
			refined_uv.Set(pulled_uv.x, pulled_uv.y, 0.0);
		}
		if (pulled)
		    uv = refined_uv;
		else if (!source_endpoint_valid) {
		    rejection = "exact edge endpoint pullback failed";
		    valid = false;
		    break;
		}
		if (pulled) {
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round((source_uv[direction] -
			    uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = surface->PointAt(
			    shifted_uv.x, shifted_uv.y);
			if (shifted_lift.IsValid() && shifted_lift.DistanceTo(
				edge_point) <= pullback_tolerance)
			    uv = shifted_uv;
		    }
		}
	    }
	    if (edge_driven && !required_endpoint && points.Count() > 0) {
		const ON_3dPoint previous_uv = points[points.Count() - 1];
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		for (int direction = 0; direction < 2; ++direction) {
		    const double period = surface->Domain(direction).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint shifted_uv = uv;
		    shifted_uv[direction] += std::round((previous_uv[direction] -
			uv[direction]) / period) * period;
		    const ON_3dPoint shifted_lift = surface->PointAt(
			shifted_uv.x, shifted_uv.y);
		    if (shifted_lift.IsValid() && shifted_lift.DistanceTo(edge_point) <=
			    tolerance)
			uv = shifted_uv;
		}
	    }
	    if (edge_driven) {
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		normalize_closed_surface_parameter(surface,
		    edge->PointAt(edge_parameter), tolerance, uv);
	    }
	    if (!uv.IsValid()) {
		rejection = edge_driven ? "invalid exact-edge pullback sample" :
		    "invalid source pcurve sample";
		valid = false;
		break;
	    }
	    points.Append(ON_3dPoint(uv.x, uv.y, 0.0));
	    normalized_parameters.push_back(normalized);
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " while sampling " +
		    std::to_string(segment_count) + " pcurve segments";
	    continue;
	}
	if (edge_driven && points.Count() >= 3) {
	    /* A surface singularity can give an edge endpoint several exact UV
	     * preimages.  Pulling it independently from the supplied endpoint may
	     * select a different branch than the exact interior samples, leaving an
	     * unsplittable parameter-space jump.  Re-pull from the adjacent exact
	     * sample and retain it only under a tighter edge-lift bound. */
	    for (int end = 0; end < 2; ++end) {
		if ((end == 0 && required_start) || (end == 1 && required_end))
		    continue;
		const int endpoint_index = end == 0 ? 0 : points.Count() - 1;
		const int adjacent_index = end == 0 ? 1 : points.Count() - 2;
		const double normalized = end == 0 ? 0.0 : 1.0;
		const ON_3dPoint edge_point = edge->PointAt(
		    edge->Domain().ParameterAt(trim.m_bRev3d ?
			1.0 - normalized : normalized));
		const double pullback_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		ON_3dPoint continuous_uv = points[adjacent_index];
		double pullback_distance = DBL_MAX;
		if (!refine_surface_pullback_seeded(surface, edge_point,
			pullback_tolerance, continuous_uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, continuous_uv)))
		    continue;
		for (int direction = 0; direction < 2; ++direction) {
		    const double period = surface->Domain(direction).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint shifted_uv = continuous_uv;
		    shifted_uv[direction] += std::round((points[adjacent_index][direction] -
			continuous_uv[direction]) / period) * period;
		    const ON_3dPoint shifted_lift = surface->PointAt(
			shifted_uv.x, shifted_uv.y);
		    if (shifted_lift.IsValid() &&
			    shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			continuous_uv = shifted_uv;
		}
		const ON_3dPoint continuous_lift = surface->PointAt(
		    continuous_uv.x, continuous_uv.y);
		if (continuous_lift.IsValid() &&
			continuous_lift.DistanceTo(edge_point) <= pullback_tolerance)
		    points[endpoint_index] = continuous_uv;
	    }
	    for (int point_index = 1; point_index < points.Count();) {
		if (points[point_index - 1].DistanceTo(points[point_index]) >
			ON_ZERO_TOLERANCE) {
		    ++point_index;
		    continue;
		}
		const int remove_index = point_index == points.Count() - 1 ?
		    point_index - 1 : point_index;
		points.Remove(remove_index);
		normalized_parameters.erase(normalized_parameters.begin() + remove_index);
		point_index = std::max(1, remove_index);
	    }
	    if (points.Count() < 2) {
		if (failure_reason)
		    *failure_reason = "continuous endpoint pullback collapsed the pcurve";
		continue;
	    }
	}

	/* Refine only the exact-edge pullback segments whose straight UV chord
	 * misses the edge.  Uniformly raising the global sample count performs
	 * poorly near surface singularities and wastes thousands of samples on
	 * already-flat portions of the curve. */
	std::set<std::pair<double, double> > singular_parameter_connectors;
	if (edge_driven) {
	    const size_t maximum_points = kMaximumAdaptivePullbackPoints;
	    bool refined = true;
	    while (valid && refined) {
		refined = false;
		for (int segment = 0; valid && segment + 1 < points.Count(); ++segment) {
		    bool split_segment = false;
		    for (int sub = 1; sub <= 3; ++sub) {
			const double fraction = static_cast<double>(sub) / 4.0;
			const double normalized = (1.0 - fraction) *
			    normalized_parameters[segment] + fraction *
			    normalized_parameters[segment + 1];
			const ON_3dPoint candidate_uv = (1.0 - fraction) * points[segment] +
			    fraction * points[segment + 1];
			const ON_3dPoint candidate_lift = surface->PointAt(
			    candidate_uv.x, candidate_uv.y);
			const double edge_parameter = edge->Domain().ParameterAt(
			    trim.m_bRev3d ? 1.0 - normalized : normalized);
			if (!candidate_lift.IsValid() || candidate_lift.DistanceTo(
				edge->PointAt(edge_parameter)) > tolerance) {
			    split_segment = true;
			    break;
			}
		    }
		    if (!split_segment)
			continue;
		    bool collapsed_periodic_branch = false;
		    double rejected_periodic_shift_distance = DBL_MAX;
		    const double second_normalized =
			normalized_parameters[segment + 1];
		    const ON_3dPoint second_edge_point = edge->PointAt(
			edge->Domain().ParameterAt(trim.m_bRev3d ?
			    1.0 - second_normalized : second_normalized));
		    const bool preserve_required_end = required_end &&
			segment + 1 == points.Count() - 1;
		    for (int direction = 0; !preserve_required_end && direction < 2;
			    ++direction) {
			if (!surface->IsClosed(direction))
			    continue;
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted = points[segment + 1];
			shifted[direction] += std::round((points[segment][direction] -
			    shifted[direction]) / period) * period;
			if (fabs(shifted[direction] -
				points[segment + 1][direction]) <= ON_ZERO_TOLERANCE ||
				shifted.DistanceTo(points[segment]) >=
				points[segment + 1].DistanceTo(points[segment]))
			    continue;
			const ON_3dPoint shifted_lift = surface->PointAt(
			    shifted.x, shifted.y);
			double shifted_distance = shifted_lift.IsValid() &&
			    second_edge_point.IsValid() ?
			    shifted_lift.DistanceTo(second_edge_point) : DBL_MAX;
			if (shifted_distance > tolerance && second_edge_point.IsValid()) {
			    ON_3dPoint refined_shift = shifted;
			    double refined_distance = DBL_MAX;
			    if (refine_surface_pullback_seeded(surface, second_edge_point,
				    tolerance, refined_shift, &refined_distance) &&
				    refined_shift.DistanceTo(points[segment]) <
					points[segment + 1].DistanceTo(points[segment])) {
				shifted = refined_shift;
				shifted_distance = refined_distance;
			    }
			}
			if (shifted_distance < rejected_periodic_shift_distance) {
			    rejected_periodic_shift_distance = shifted_distance;
			}
			if (shifted_distance <= tolerance) {
			    points[segment + 1] = shifted;
			    collapsed_periodic_branch = true;
			}
		    }
		    if (collapsed_periodic_branch) {
			refined = true;
			break;
		    }
		    const double parameter_midpoint = 0.5 *
			(normalized_parameters[segment] +
			 normalized_parameters[segment + 1]);
		    if (!(parameter_midpoint > normalized_parameters[segment] &&
			    parameter_midpoint < normalized_parameters[segment + 1])) {
			const int varying_direction = fabs(points[segment + 1].y -
			    points[segment].y) > fabs(points[segment + 1].x -
			    points[segment].x) ? 1 : 0;
			const int fixed_direction = 1 - varying_direction;
			const ON_Interval fixed_domain = surface->Domain(fixed_direction);
			std::vector<double> fixed_candidates;
			fixed_candidates.reserve(134);
			const double fixed_first = points[segment][fixed_direction];
			const double fixed_second = points[segment + 1][fixed_direction];
			fixed_candidates.push_back(fixed_first);
			fixed_candidates.push_back(fixed_second);
			fixed_candidates.push_back(0.5 * (fixed_first + fixed_second));
			fixed_candidates.push_back(fixed_domain.Min());
			fixed_candidates.push_back(fixed_domain.Max());
			const double fixed_min = std::min(fixed_first, fixed_second);
			const double fixed_max = std::max(fixed_first, fixed_second);
			const double fixed_span = fixed_max - fixed_min;
			if (fixed_span > ON_ZERO_TOLERANCE) {
			    const double search_min = fixed_min - 4.0 * fixed_span;
			    const double search_max = fixed_max + 4.0 * fixed_span;
			    for (int search_sample = 0; search_sample <= 128;
				    ++search_sample)
				fixed_candidates.push_back(search_min +
				    (search_max - search_min) * search_sample / 128.0);
			}
			bool connected = false;
			double best_cost = DBL_MAX;
			ON_3dPoint best_start;
			ON_3dPoint best_end;
			for (std::vector<double>::const_iterator fixed_candidate =
				fixed_candidates.begin(); fixed_candidate !=
				fixed_candidates.end(); ++fixed_candidate) {
			    ON_3dPoint connector_start = points[segment];
			    ON_3dPoint connector_end = points[segment + 1];
			    connector_start[fixed_direction] =
				*fixed_candidate;
			    connector_end[fixed_direction] =
				*fixed_candidate;
			    if (connector_start.DistanceTo(connector_end) <=
				    ON_ZERO_TOLERANCE)
				continue;
			    bool exact_connector = true;
			for (int connector_sample = 0;
				    exact_connector &&
				    connector_sample <= kDenseValidationSegments;
				    ++connector_sample) {
				const double fraction = static_cast<double>(connector_sample) /
				    kDenseValidationSegments;
				const double normalized = (1.0 - fraction) *
				    normalized_parameters[segment] + fraction *
				    normalized_parameters[segment + 1];
				const ON_3dPoint connector_uv = (1.0 - fraction) *
				    connector_start + fraction * connector_end;
				const ON_3dPoint connector_lift = surface->PointAt(
				    connector_uv.x, connector_uv.y);
				const ON_3dPoint connector_edge = edge->PointAt(
				    edge->Domain().ParameterAt(trim.m_bRev3d ?
					1.0 - normalized : normalized));
				const double connector_tolerance =
				    (connector_sample == 0 ||
				     connector_sample == kDenseValidationSegments) ?
				    std::max(ON_ZERO_TOLERANCE * kNumericalToleranceScale,
					tolerance * 0.1) : tolerance;
				exact_connector = connector_lift.IsValid() &&
				    connector_edge.IsValid() && connector_lift.DistanceTo(
					connector_edge) <= connector_tolerance;
			    }
			    const double cost = fabs(*fixed_candidate -
				points[segment][fixed_direction]) +
				fabs(*fixed_candidate -
				points[segment + 1][fixed_direction]);
			    if (exact_connector && cost < best_cost) {
				connected = true;
				best_cost = cost;
				best_start = connector_start;
				best_end = connector_end;
			    }
			}
			if (connected) {
			    points[segment] = best_start;
			    points[segment + 1] = best_end;
			    singular_parameter_connectors.insert(std::make_pair(
				normalized_parameters[segment],
				normalized_parameters[segment + 1]));
			    continue;
			}
			if (periodic_crossing && !periodic_crossing->detected) {
			    for (int direction = 0; direction < 2; ++direction) {
				if (!surface->IsClosed(direction))
				    continue;
				const double period = surface->Domain(direction).Length();
				if (!(period > ON_ZERO_TOLERANCE) || fabs(
					points[segment + 1][direction] -
					points[segment][direction]) <= 0.5 * period)
				    continue;
				periodic_crossing->detected = true;
				periodic_crossing->trim_fraction = 0.5 *
				    (normalized_parameters[segment] +
				     normalized_parameters[segment + 1]);
				periodic_crossing->surface_direction = direction;
				break;
			    }
			}
			rejection = "adaptive exact-edge pullback made no parameter progress at " +
			    std::to_string(normalized_parameters[segment]) + ":" +
			    std::to_string(normalized_parameters[segment + 1]) +
			    " (uv " + std::to_string(points[segment].x) + ":" +
			    std::to_string(points[segment].y) + " -> " +
			    std::to_string(points[segment + 1].x) + ":" +
			    std::to_string(points[segment + 1].y) + ", domains " +
			    std::to_string(surface->Domain(0).Min()) + ":" +
			    std::to_string(surface->Domain(0).Max()) + "," +
			    std::to_string(surface->Domain(1).Min()) + ":" +
			    std::to_string(surface->Domain(1).Max()) + ", closed " +
			    std::to_string(surface->IsClosed(0) ? 1 : 0) +
			    std::to_string(surface->IsClosed(1) ? 1 : 0) +
			    ", periodic " +
			    std::to_string(surface->IsPeriodic(0) ? 1 : 0) +
			    std::to_string(surface->IsPeriodic(1) ? 1 : 0) +
			    ", singular " +
			    std::to_string(surface->IsSingular(0) ? 1 : 0) +
			    std::to_string(surface->IsSingular(1) ? 1 : 0) +
			    std::to_string(surface->IsSingular(2) ? 1 : 0) +
			    std::to_string(surface->IsSingular(3) ? 1 : 0) +
			    ", periodic shift distance " +
			    std::to_string(rejected_periodic_shift_distance) + ")";
			valid = false;
			break;
		    }
		    if (static_cast<size_t>(points.Count()) >= maximum_points) {
			rejection = "adaptive exact-edge pullback sample budget exceeded at " +
			    std::to_string(normalized_parameters[segment]) + ":" +
			    std::to_string(normalized_parameters[segment + 1]) +
			    " (uv " + std::to_string(points[segment].x) + ":" +
			    std::to_string(points[segment].y) + " -> " +
			    std::to_string(points[segment + 1].x) + ":" +
			    std::to_string(points[segment + 1].y) +
			    ", periodic shift distance " +
			    std::to_string(rejected_periodic_shift_distance) + ")";
			valid = false;
			break;
		    }

		    const double normalized = parameter_midpoint;
		    const double edge_parameter = edge->Domain().ParameterAt(
			trim.m_bRev3d ? 1.0 - normalized : normalized);
		    const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		    const double pullback_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		    const ON_3dPoint branch_reference = 0.5 *
			(points[segment] + points[segment + 1]);
		    ON_3dPoint uv = branch_reference;
		    double pullback_distance = DBL_MAX;
		    if (!refine_surface_pullback_seeded(surface, edge_point,
			    pullback_tolerance, uv, &pullback_distance,
			    !has_unwrapped_closed_parameter(surface, uv))) {
			ON_2dPoint pulled_uv;
			ON_3dPoint pulled_lift;
			if (!pullback_context.SurfaceClosestPoint(surface, edge_point, pulled_uv,
				pulled_lift, pullback_distance, 0,
				std::max(ON_ZERO_TOLERANCE, pullback_tolerance * 0.1),
				pullback_tolerance) || pullback_distance > pullback_tolerance) {
			    rejection = "adaptive exact-edge pullback failed";
			    valid = false;
			    break;
			}
			uv.Set(pulled_uv.x, pulled_uv.y, 0.0);
		    }
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round((branch_reference[direction] -
			    uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = surface->PointAt(
			    shifted_uv.x, shifted_uv.y);
			if (shifted_lift.IsValid() &&
				shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			    uv = shifted_uv;
		    }
		    normalize_closed_surface_parameter(surface, edge_point,
			pullback_tolerance, uv);
		    points.Insert(segment + 1, uv);
		    normalized_parameters.insert(normalized_parameters.begin() +
			segment + 1, normalized);
		    refined = true;
		    break;
		}
	    }
	    if (!valid) {
		if (failure_reason)
		    *failure_reason = rejection;
		continue;
	    }
	}
	/* A fitted closed pcurve can have a poor one-sided derivative at its
	 * algebraic endpoint even when its interior follows the edge correctly.
	 * Map the exact 3D edge tangent into the surface parameter plane and add
	 * a short endpoint chord without moving an already validated interior
	 * sample.  The validation below still
	 * bounds both the displacement from the source pcurve and the distance to
	 * the exact edge, so this cannot bridge an out-of-tolerance gap. */
	const bool closed_topology = trim.m_vi[0] == trim.m_vi[1] &&
	    edge->m_vi[0] == edge->m_vi[1];
	for (int end = 0; valid && closed_topology && end < 2; ++end) {
	    const int current_segment_count = points.Count() - 1;
	    const int endpoint_index = end == 0 ? 0 : current_segment_count;
	    const int adjacent_index = end == 0 ? 1 : current_segment_count - 1;
	    const ON_3dPoint endpoint_uv = points[endpoint_index];
	    ON_3dPoint lifted;
	    ON_3dVector du, dv;
	    if (!surface->Ev1Der(endpoint_uv.x, endpoint_uv.y, lifted, du, dv)) {
		rejection = "endpoint surface derivative evaluation failed";
		valid = false;
		break;
	    }
	    ON_3dVector target = edge->TangentAt(edge->Domain()[
		trim.m_bRev3d ? 1 - end : end]);
	    if (trim.m_bRev3d)
		target.Reverse();
	    if (!target.Unitize()) {
		rejection = "endpoint edge tangent evaluation failed";
		valid = false;
		break;
	    }
	    const ON_3dVector current_uv_tangent = end == 0 ?
		points[adjacent_index] - endpoint_uv :
		endpoint_uv - points[adjacent_index];
	    ON_3dVector current_lifted_tangent = current_uv_tangent.x * du +
		current_uv_tangent.y * dv;
	    if (current_lifted_tangent.Unitize()) {
		const double current_alignment = current_lifted_tangent * target;
		if (current_alignment >= 0.0)
		    continue;
		/* OpenNURBS requires a nonnegative endpoint tangent alignment for a
		 * closed edge use.  Rotate only far enough to cross that sign boundary;
		 * replacing a nearly perpendicular supplied tangent with the complete
		 * edge tangent can move an otherwise exact pcurve beyond tolerance. */
		const double positive_margin = std::max(1.0e-6,
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale);
		target = current_lifted_tangent +
		    (-current_alignment + positive_margin) * target;
		if (!target.Unitize()) {
		    rejection = "endpoint tangent correction failed";
		    valid = false;
		    break;
		}
	    }
	    const double a = du * du;
	    const double b = du * dv;
	    const double c = dv * dv;
	    const double r0 = du * target;
	    const double r1 = dv * target;
	    const double determinant = a * c - b * b;
	    const double numerical_floor = ON_ZERO_TOLERANCE *
		std::max(1.0, a * c);
	    if (fabs(determinant) <= numerical_floor) {
		rejection = "singular endpoint surface Jacobian";
		valid = false;
		break;
	    }
	    ON_3dVector uv_tangent((c * r0 - b * r1) / determinant,
		(a * r1 - b * r0) / determinant, 0.0);
	    const ON_3dPoint adjacent_lift = surface->PointAt(
		points[adjacent_index].x, points[adjacent_index].y);
	    const double chord_length = lifted.DistanceTo(adjacent_lift);
	    const ON_3dVector lifted_uv_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	    const double tangent_scale = lifted_uv_tangent.Length();
	    if (!adjacent_lift.IsValid() || chord_length <= ON_ZERO_TOLERANCE ||
		    tangent_scale <= ON_ZERO_TOLERANCE) {
		rejection = "degenerate endpoint parameter tangent";
		valid = false;
		break;
	    }
	    uv_tangent *= 0.25 * chord_length / tangent_scale;
	    const ON_3dPoint tangent_point = end == 0 ?
		endpoint_uv + uv_tangent : endpoint_uv - uv_tangent;
	    const double tangent_parameter = end == 0 ?
		0.75 * normalized_parameters[endpoint_index] +
		    0.25 * normalized_parameters[adjacent_index] :
		0.25 * normalized_parameters[adjacent_index] +
		    0.75 * normalized_parameters[endpoint_index];
	    const int insertion_index = end == 0 ? 1 : endpoint_index;
	    points.Insert(insertion_index, tangent_point);
	    normalized_parameters.insert(normalized_parameters.begin() +
		insertion_index, tangent_parameter);
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " + std::to_string(segment_count) +
		    " segments";
	    continue;
	}
	const int candidate_segment_count = points.Count() - 1;

	/* Validate both geometry and direction at every new vertex and at interior
	 * points of every segment.  Comparison with the original lift bounds the
	 * repair itself; comparison with the edge verifies that no narrow invalid
	 * source deviation can hide exactly at a sampling vertex. */
	for (int segment = 0; valid && segment < candidate_segment_count; ++segment) {
	    const ON_3dVector uv_tangent = points[segment + 1] - points[segment];
	    if (uv_tangent.Length() <= ON_ZERO_TOLERANCE) {
		rejection = "zero-length regenerated pcurve segment";
		valid = false;
		break;
	    }
	    for (int sub = 0; sub <= 3; ++sub) {
		const double fraction = static_cast<double>(sub) / 4.0;
		const double normalized = (1.0 - fraction) *
		    normalized_parameters[segment] + fraction *
		    normalized_parameters[segment + 1];
		const ON_3dPoint candidate_uv = (1.0 - fraction) * points[segment] +
		    fraction * points[segment + 1];
		const ON_3dPoint original_uv = trim.PointAt(
		    trim_domain.ParameterAt(normalized));
		ON_3dPoint candidate_lift, original_lift;
		ON_3dVector du, dv;
		if (!surface->Ev1Der(candidate_uv.x, candidate_uv.y,
			candidate_lift, du, dv)) {
		    rejection = "surface lift failed";
		    valid = false;
		    break;
		}
		original_lift = surface->PointAt(original_uv.x, original_uv.y);
		const double source_lift_distance = original_lift.IsValid() ?
		    candidate_lift.DistanceTo(original_lift) : DBL_MAX;
		if (!original_lift.IsValid() ||
			(!edge_driven && source_lift_distance > tolerance)) {
		    rejection = "regenerated lift exceeded source pcurve tolerance"
			" (distance " + std::to_string(source_lift_distance) +
			", tolerance " + std::to_string(tolerance) +
			", normalized " + std::to_string(normalized) +
			", segment uv " + std::to_string(points[segment].x) + ":" +
			std::to_string(points[segment].y) + " -> " +
			std::to_string(points[segment + 1].x) + ":" +
			std::to_string(points[segment + 1].y) + ")";
		    valid = false;
		    break;
		}
		double edge_parameter = 0.0;
		double edge_distance = DBL_MAX;
		bool edge_proxy_parameter = false;
		if (edge_driven) {
		    edge_parameter = edge->Domain().ParameterAt(
			trim.m_bRev3d ? 1.0 - normalized : normalized);
		    edge_distance = candidate_lift.DistanceTo(edge->PointAt(edge_parameter));
		    edge_proxy_parameter = true;
		} else {
		    edge_parameter = edge->Domain().ParameterAt(
			trim.m_bRev3d ? 1.0 - normalized : normalized);
		    edge_distance = candidate_lift.DistanceTo(edge->PointAt(edge_parameter));
		    edge_proxy_parameter = true;
		    double closest_parameter = 0.0;
		    if (ON_NurbsCurve_GetClosestPoint(&closest_parameter, &edge_nurbs,
			    candidate_lift)) {
			const double closest_distance = candidate_lift.DistanceTo(
			    edge_nurbs.PointAt(closest_parameter));
			if (closest_distance < edge_distance) {
			    edge_parameter = closest_parameter;
			    edge_distance = closest_distance;
			    edge_proxy_parameter = false;
			}
		    }
		}
		if (edge_distance > tolerance) {
		    double original_parameter = 0.0;
		    double original_edge_distance = ON_UNSET_VALUE;
		    if (ON_NurbsCurve_GetClosestPoint(&original_parameter, &edge_nurbs,
			    original_lift))
		original_edge_distance = original_lift.DistanceTo(
			edge_nurbs.PointAt(original_parameter));
		    rejection = "regenerated lift exceeded edge tolerance (distance " +
			std::to_string(edge_distance) + ", source " +
			std::to_string(original_edge_distance) + ", tolerance " +
			std::to_string(tolerance) + ", normalized " +
			std::to_string(normalized) + ", segment uv " +
			std::to_string(points[segment].x) + ":" +
			std::to_string(points[segment].y) + " -> " +
			std::to_string(points[segment + 1].x) + ":" +
			std::to_string(points[segment + 1].y) + ", candidate uv " +
			std::to_string(candidate_uv.x) + ":" +
			std::to_string(candidate_uv.y) + ")";
		    valid = false;
		    break;
		}
		if (singular_parameter_connectors.find(std::make_pair(
			normalized_parameters[segment],
			normalized_parameters[segment + 1])) !=
			singular_parameter_connectors.end())
		    continue;
		ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
		if (!edge_driven) {
		    ON_3dPoint source_uv, source_lift;
		    ON_3dVector source_uv_tangent, source_du, source_dv;
		    const double source_parameter = trim_domain.ParameterAt(normalized);
		    if (!trim.Ev1Der(source_parameter, source_uv, source_uv_tangent) ||
			    !surface->Ev1Der(source_uv.x, source_uv.y, source_lift,
				source_du, source_dv)) {
			rejection = "source pcurve tangent evaluation failed";
			valid = false;
			break;
		    }
		    ON_3dVector source_tangent = source_uv_tangent.x * source_du +
			source_uv_tangent.y * source_dv;
		    if (!lifted_tangent.Unitize() || !source_tangent.Unitize() ||
			    lifted_tangent * source_tangent <= 0.0) {
			rejection = "regenerated interior direction disagreed with source pcurve";
			valid = false;
			break;
		    }
		    continue;
		}
		ON_3dVector edge_tangent = edge_proxy_parameter ?
		    edge->TangentAt(edge_parameter) : edge_nurbs.TangentAt(edge_parameter);
		if (!lifted_tangent.Unitize() || !edge_tangent.Unitize()) {
		    rejection = "regenerated tangent evaluation failed";
		    valid = false;
		    break;
		}
		double alignment = lifted_tangent * edge_tangent;
		if (trim.m_bRev3d)
		    alignment = -alignment;
		if (alignment <= 0.0) {
		    rejection = "regenerated interior direction disagreed with edge"
			" (segment " + std::to_string(segment) + ", normalized " +
			std::to_string(normalized) + ", alignment " +
			std::to_string(alignment) + ", reversed " +
			std::to_string(trim.m_bRev3d ? 1 : 0) + ")";
		    valid = false;
		    break;
		}
	    }
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " + std::to_string(segment_count) +
		    " initial segments (" + std::to_string(candidate_segment_count) +
		    " final segments)";
	    continue;
	}

	ON_SimpleArray<double> candidate_parameters;
	candidate_parameters.Reserve(static_cast<int>(normalized_parameters.size()));
	for (std::vector<double>::const_iterator normalized =
		normalized_parameters.begin(); normalized != normalized_parameters.end();
	     ++normalized)
	    candidate_parameters.Append(trim_domain.ParameterAt(*normalized));
	ON_PolylineCurve *candidate = new ON_PolylineCurve(points,
	    candidate_parameters);
	if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
	    if (failure_reason)
		*failure_reason = "regenerated polyline is invalid";
	    delete candidate;
	    continue;
	}
	if (trim.m_vi[0] != trim.m_vi[1] && candidate->IsClosed()) {
	    if (failure_reason)
		*failure_reason = "regenerated open-topology pcurve remained closed";
	    delete candidate;
	    continue;
	}
	const ON_Interval candidate_domain = candidate->Domain();
	for (int end = 0; valid && end < 2; ++end) {
	    ON_3dPoint uv, lifted_point;
	    ON_3dVector uv_tangent, du, dv;
	    if (!candidate->Ev1Der(candidate_domain[end], uv, uv_tangent) ||
		    !surface->Ev1Der(uv.x, uv.y, lifted_point, du, dv)) {
		rejection = "regenerated endpoint evaluation failed";
		valid = false;
		break;
	    }
	    const int vertex_index = trim.m_vi[end];
	    const ON_3dPoint edge_endpoint = edge->PointAt(
		edge->Domain()[trim.m_bRev3d ? 1 - end : end]);
	    if (!edge_endpoint.IsValid() ||
		    lifted_point.DistanceTo(edge_endpoint) > tolerance ||
		    vertex_index < 0 || vertex_index >= brep->m_V.Count() ||
		    lifted_point.DistanceTo(brep->m_V[vertex_index].point) > tolerance) {
		rejection = "regenerated endpoint exceeded exact topology tolerance";
		valid = false;
		break;
	    }
	    ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	    ON_3dVector edge_tangent = edge->TangentAt(
		edge->Domain()[trim.m_bRev3d ? 1 - end : end]);
	    if (!lifted_tangent.Unitize() || !edge_tangent.Unitize()) {
		rejection = "regenerated endpoint tangent evaluation failed";
		valid = false;
		break;
	    }
	    double alignment = lifted_tangent * edge_tangent;
	    if (trim.m_bRev3d)
		alignment = -alignment;
	    if (alignment < 0.0) {
		rejection = "regenerated endpoint direction disagreed with edge";
		valid = false;
	    }
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " + std::to_string(segment_count) +
		    " initial segments (" + std::to_string(candidate_segment_count) +
		    " final segments)";
	    delete candidate;
	    continue;
	}
	const int c2_index = brep->AddTrimCurve(candidate);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index))
	    return false;
	const ON_Interval proxy_domain = trim.ProxyCurveDomain();
	trim.m_iso = surface->IsIsoparametric(*candidate, &proxy_domain);
	return true;
    }
    }
    return false;
}


static bool
align_closed_surface_seam_from_trim_pair(ON_Brep *brep,
	const ON_BrepEdge &edge, const ON_BrepLoop &loop,
	const ON_Surface *surface, double tolerance,
	std::string *failure_reason = NULL)
{
    if (failure_reason)
	*failure_reason = "no closed surface direction admitted the seam pair";
    ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(
	const_cast<ON_Surface *>(surface));
    ON_RevSurface *revolution = ON_RevSurface::Cast(
	const_cast<ON_Surface *>(surface));
    const ON_BrepFace *source_face = loop.Face();
	if (!brep || (!nurbs && !revolution) || !source_face ||
	    edge.m_ti.Count() != 2 ||
	    !(tolerance > 0.0))
	return false;

    const ON_BrepTrim *pair[2] = {
	brep->Trim(edge.m_ti[0]), brep->Trim(edge.m_ti[1])
    };
    if (!pair[0] || !pair[1] || pair[0]->m_li != loop.m_loop_index ||
	    pair[1]->m_li != loop.m_loop_index)
	return false;

    const auto fixed_iso_direction = [](ON_Surface::ISO iso) {
	if (iso == ON_Surface::W_iso || iso == ON_Surface::E_iso)
	    return 0;
	if (iso == ON_Surface::S_iso || iso == ON_Surface::N_iso)
	    return 1;
	return -1;
    };
    int preferred_direction = -1;
    {
	/* Invalid pcurves can report the wrong iso direction.  Infer the seam's
	 * fixed coordinate from immutable 3D edge samples pulled back onto the
	 * surface.  Whole-period unwrapping prevents a legitimate seam crossing
	 * from looking like a varying coordinate. */
	brlcad::PullbackContext context;
	double parameters[5][2];
	bool pulled = true;
	for (int sample = 0; sample < 5; ++sample) {
	    const double fraction = (static_cast<double>(sample) + 1.0) / 6.0;
	    const ON_3dPoint target = edge.PointAt(
		edge.Domain().ParameterAt(fraction));
	    ON_2dPoint uv(surface->Domain(0).Mid(), surface->Domain(1).Mid());
	    ON_3dPoint lift;
	    double distance = DBL_MAX;
	    pulled = context.SurfaceClosestPoint(surface, target, uv, lift,
		distance, 0, std::max(1.0e-10, tolerance * 1.0e-6),
		tolerance) && distance <= tolerance;
	    if (!pulled)
		break;
	    for (int direction = 0; direction < 2; ++direction) {
		double parameter = uv[direction];
		if (sample > 0 && surface->IsClosed(direction)) {
		    const double period = surface->Domain(direction).Length();
		    if (period > ON_ZERO_TOLERANCE)
			parameter += round((parameters[0][direction] - parameter) /
			    period) * period;
		}
		parameters[sample][direction] = parameter;
	    }
	}
	if (pulled) {
	    double normalized_range[2] = {DBL_MAX, DBL_MAX};
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		double minimum = parameters[0][direction];
		double maximum = parameters[0][direction];
		for (int sample = 1; sample < 5; ++sample) {
		    minimum = std::min(minimum, parameters[sample][direction]);
		    maximum = std::max(maximum, parameters[sample][direction]);
		}
		const double period = surface->Domain(direction).Length();
		if (period > ON_ZERO_TOLERANCE)
		    normalized_range[direction] = (maximum - minimum) / period;
	    }
	    const int exact_direction = normalized_range[0] <=
		normalized_range[1] ? 0 : 1;
	    const int varying_direction = 1 - exact_direction;
	    const double numerical_range = std::max(1.0e-8,
		1.0e-4 * normalized_range[varying_direction]);
	    if (normalized_range[exact_direction] <= numerical_range)
		preferred_direction = exact_direction;
	}
    }

    if (preferred_direction < 0) {
	preferred_direction = fixed_iso_direction(pair[0]->m_iso);
	const int second_iso_direction = fixed_iso_direction(pair[1]->m_iso);
	if (preferred_direction < 0)
	    preferred_direction = second_iso_direction;
	else if (second_iso_direction >= 0 &&
		second_iso_direction != preferred_direction)
	    return false;
    }

    /* Before iso flags have been recovered, a short seam can look nearly
     * constant in both coordinates on a doubly closed surface.  Its true
     * fixed direction is the coordinate with the smaller normalized span;
     * considering the other direction can rotate an unrelated surface seam
     * and invalidate every adjacent pcurve. */
    if (preferred_direction < 0) {
	double normalized_span[2] = {DBL_MAX, DBL_MAX};
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const double period = surface->Domain(direction).Length();
	    if (!(period > ON_ZERO_TOLERANCE))
		continue;
	    double maximum_span = 0.0;
	    for (int member = 0; member < 2; ++member) {
		const ON_Interval trim_domain = pair[member]->Domain();
		const int samples = std::min(256,
		    std::max(32, pair[member]->SpanCount() * 4));
		double minimum = DBL_MAX;
		double maximum = -DBL_MAX;
		for (int sample = 0; sample <= samples; ++sample) {
		    const ON_3dPoint uv = pair[member]->PointAt(
			trim_domain.ParameterAt(static_cast<double>(sample) / samples));
		    minimum = std::min(minimum, uv[direction]);
		    maximum = std::max(maximum, uv[direction]);
		}
		maximum_span = std::max(maximum_span, maximum - minimum);
	    }
	    normalized_span[direction] = maximum_span / period;
	}
	if (normalized_span[0] < DBL_MAX || normalized_span[1] < DBL_MAX)
	    preferred_direction = normalized_span[0] <= normalized_span[1] ? 0 : 1;
    }

    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	if (preferred_direction >= 0 && direction != preferred_direction)
	    continue;
	if (revolution) {
	    const int profile_direction = revolution->m_bTransposed ? 0 : 1;
	    if (direction != profile_direction || !revolution->m_curve ||
		    !revolution->m_curve->IsClosed())
		continue;
	}
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;

	double minimum[2] = {DBL_MAX, DBL_MAX};
	double maximum[2] = {-DBL_MAX, -DBL_MAX};
	double center[2] = {0.0, 0.0};
	for (int member = 0; member < 2; ++member) {
	    const ON_Interval trim_domain = pair[member]->Domain();
	    const int samples = std::min(256,
		std::max(32, pair[member]->SpanCount() * 4));
	    for (int sample = 0; sample <= samples; ++sample) {
		const ON_3dPoint uv = pair[member]->PointAt(
		    trim_domain.ParameterAt(static_cast<double>(sample) / samples));
		minimum[member] = std::min(minimum[member], uv[direction]);
		maximum[member] = std::max(maximum[member], uv[direction]);
		center[member] += uv[direction];
	    }
	    center[member] /= static_cast<double>(samples + 1);
	}
	const double branch_tolerance = 0.05 * period;
	if (maximum[0] - minimum[0] > branch_tolerance ||
		maximum[1] - minimum[1] > branch_tolerance ||
		fabs(fabs(center[0] - center[1]) - period) > branch_tolerance) {
	    if (failure_reason) {
		std::ostringstream reason;
		reason << "trim pair did not describe opposite branches in direction "
		    << direction << " (spans " << maximum[0] - minimum[0]
		    << "/" << maximum[1] - minimum[1] << ", centers "
		    << center[0] << "/" << center[1] << ", period " << period
		    << ")";
		*failure_reason = reason.str();
	    }
	    continue;
	}

	double seam = std::max(center[0], center[1]);
	/* The supplied pcurves can be displaced by nearly the full asserted model
	 * uncertainty.  Using their average as a new surface seam then places the
	 * exact STEP edge just outside that boundary.  Refine interior edge samples
	 * from the supplied branch and use their constant exact surface parameter
	 * when the independent samples agree numerically. */
	double exact_parameters[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
	bool exact_constant = true;
	const double numerical_tolerance = std::max(1.0e-10,
	    std::min(1.0e-7, tolerance * 1.0e-6));
	brlcad::PullbackContext pullback_context;
	const ON_Interval first_domain = pair[0]->Domain();
	const ON_Interval exact_edge_domain = edge.Domain();
	for (int sample = 0; sample < 5; ++sample) {
	    const double fraction = (static_cast<double>(sample) + 1.0) / 6.0;
	    ON_3dPoint uv = pair[0]->PointAt(first_domain.ParameterAt(fraction));
	    const double edge_fraction = pair[0]->m_bRev3d ?
		1.0 - fraction : fraction;
	    const ON_3dPoint target = edge.PointAt(
		exact_edge_domain.ParameterAt(edge_fraction));
	    double distance = DBL_MAX;
	    bool pulled = refine_surface_pullback_seeded(surface, target,
		numerical_tolerance, uv, &distance);
	    if (!pulled) {
		ON_2dPoint global_uv;
		ON_3dPoint global_lift;
		pulled = pullback_context.SurfaceClosestPoint(surface, target,
		    global_uv, global_lift, distance, 0, numerical_tolerance,
		    tolerance) && distance <= tolerance;
		if (pulled) {
		    uv.Set(global_uv.x, global_uv.y, 0.0);
		    double refined_distance = DBL_MAX;
		    if (refine_surface_pullback_seeded(surface, target,
			    numerical_tolerance, uv, &refined_distance))
			distance = refined_distance;
		}
	    }
	    if (!pulled) {
		exact_constant = false;
		break;
	    }
	    exact_parameters[sample] = uv[direction] +
		round((center[0] - uv[direction]) / period) * period;
	}
	if (exact_constant) {
	    double exact_minimum = exact_parameters[0];
	    double exact_maximum = exact_parameters[0];
	    double exact_sum = exact_parameters[0];
	    for (int sample = 1; sample < 5; ++sample) {
		exact_minimum = std::min(exact_minimum, exact_parameters[sample]);
		exact_maximum = std::max(exact_maximum, exact_parameters[sample]);
		exact_sum += exact_parameters[sample];
	    }
	    const double constant_tolerance = std::max(1.0e-9,
		1.0e-7 * period);
	    if (exact_maximum - exact_minimum <= constant_tolerance)
		seam = exact_sum / 5.0;
	    else if (failure_reason)
		*failure_reason = "exact edge pullbacks were not constant in the fixed direction";
	} else if (failure_reason) {
	    *failure_reason = "exact edge pullback could not locate the candidate surface seam";
	}
	while (seam < domain.Min()) seam += period;
	while (seam > domain.Max()) seam -= period;
	const double endpoint_guard = 1.0e-7 * std::max(1.0, period);
	if (seam <= domain.Min() + endpoint_guard ||
		seam >= domain.Max() - endpoint_guard) {
	    if (failure_reason)
		*failure_reason = "candidate seam was already on the surface domain boundary";
	    continue;
	}

	ON_NurbsSurface nurbs_candidate;
	ON_RevSurface revolution_candidate;
	const ON_Surface *candidate = NULL;
	if (nurbs) {
	    nurbs_candidate = *nurbs;
	    if (!nurbs_candidate.ChangeSurfaceSeam(direction, seam)) {
		if (failure_reason)
		    *failure_reason = "NURBS surface rejected the candidate seam parameter";
		continue;
	    }
	    candidate = &nurbs_candidate;
	} else {
	    revolution_candidate = *revolution;
	    if (!revolution_candidate.m_curve ||
		    !revolution_candidate.m_curve->ChangeClosedCurveSeam(seam))
		continue;
	    candidate = &revolution_candidate;
	}
	const ON_Interval candidate_domain = candidate->Domain(direction);
	ON_Xform shift(ON_Xform::IdentityTransformation);
	shift.m_xform[direction][3] = period;

	std::vector<int> trim_indices;
	std::vector<ON_Curve *> transformed_curves;
	bool all_valid = true;
	for (int fi = 0; fi < brep->m_F.Count() && all_valid; ++fi) {
	    const ON_BrepFace &face = brep->m_F[fi];
	    if (face.m_si != source_face->m_si)
		continue;
	    for (int fli = 0; fli < face.LoopCount() && all_valid; ++fli) {
		const ON_BrepLoop *affected_loop = face.Loop(fli);
		if (!affected_loop)
		    continue;
		for (int lti = 0; lti < affected_loop->TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = affected_loop->Trim(lti);
		    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
		    ON_BoundingBox curve_box;
		    if (!trim || !curve || !curve->Transform(shift) ||
			    !curve->GetBoundingBox(curve_box)) {
			delete curve;
			all_valid = false;
			break;
		    }
		    const double curve_center = 0.5 *
			(curve_box.m_min[direction] + curve_box.m_max[direction]);
		    if (curve_center < candidate_domain.Min() - endpoint_guard ||
			    curve_center > candidate_domain.Max() + endpoint_guard) {
			const double wrap = round((candidate_domain.Mid() - curve_center) /
			    period) * period;
			if (fabs(wrap) > ON_ZERO_TOLERANCE) {
			    ON_Xform wrap_transform(ON_Xform::IdentityTransformation);
			    wrap_transform.m_xform[direction][3] = wrap;
			    if (!curve->Transform(wrap_transform)) {
				delete curve;
				all_valid = false;
				break;
			    }
			}
		    }
		    if (!curve->ChangeDimension(2) || !curve->IsValid()) {
			delete curve;
			all_valid = false;
			break;
		    }

		    const ON_Interval trim_domain = trim->Domain();
		    const int samples = std::min(256,
			std::max(32, trim->SpanCount() * 4));
		    for (int sample = 0; sample <= samples; ++sample) {
			const double parameter = trim_domain.ParameterAt(
			    static_cast<double>(sample) / samples);
			const ON_3dPoint original_uv = trim->PointAt(parameter);
			const ON_3dPoint transformed_uv = curve->PointAt(parameter);
			const ON_3dPoint original_lift = surface->PointAt(
			    original_uv.x, original_uv.y);
			const ON_3dPoint transformed_lift = candidate->PointAt(
			    transformed_uv.x, transformed_uv.y);
			if (!original_lift.IsValid() || !transformed_lift.IsValid() ||
				original_lift.DistanceTo(transformed_lift) > tolerance) {
			    all_valid = false;
			    break;
			}
		    }
		    if (!all_valid) {
			delete curve;
			break;
		    }
		    trim_indices.push_back(trim->m_trim_index);
		    transformed_curves.push_back(curve);
		}
	    }
	}
	if (!all_valid) {
	    if (failure_reason)
		*failure_reason = "surface seam change did not preserve every affected pcurve lift";
	    for (std::vector<ON_Curve *>::iterator curve = transformed_curves.begin();
		    curve != transformed_curves.end(); ++curve)
		delete *curve;
	    continue;
	}

	std::vector<int> c2_indices;
	c2_indices.reserve(transformed_curves.size());
	for (std::vector<ON_Curve *>::iterator curve = transformed_curves.begin();
		curve != transformed_curves.end(); ++curve) {
	    const int c2_index = brep->AddTrimCurve(*curve);
	    if (c2_index < 0)
		return false;
	    c2_indices.push_back(c2_index);
	}
	if (nurbs)
	    *nurbs = nurbs_candidate;
	else
	    *revolution = revolution_candidate;
	for (size_t i = 0; i < trim_indices.size(); ++i) {
	    if (!brep->SetTrimCurve(brep->m_T[trim_indices[i]], c2_indices[i]))
		return false;
	    brep->SetTrimIsoFlags(brep->m_T[trim_indices[i]]);
	}
	return true;
    }
    return false;
}


static void
repair_seam_pair_from_exact_edge(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::vector<int> *additional_repaired_loops = NULL,
	std::vector<int> *aligned_surface_loops = NULL,
	bool allow_surface_alignment = false)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const auto solve_boundary_parameter = [](const ON_Surface *surface,
	int fixed_direction, double boundary, const ON_3dPoint &target,
	double seed, double tolerance, double *result) {
	if (!surface || !result)
	    return false;
	const int varying_direction = 1 - fixed_direction;
	const ON_Interval varying_domain = surface->Domain(varying_direction);
	if (!varying_domain.IsIncreasing())
	    return false;
	const auto refine = [surface, fixed_direction, varying_direction, boundary, &target,
	    tolerance, &varying_domain](double initial, double *refined,
	    double *refined_distance) {
	    double parameter = initial;
	    double best_distance = DBL_MAX;
	    for (int iteration = 0; iteration < 32; ++iteration) {
	    const double u = fixed_direction == 0 ? boundary : parameter;
	    const double v = fixed_direction == 0 ? parameter : boundary;
	    ON_3dPoint point;
	    ON_3dVector du, dv;
	    if (!surface->Ev1Der(u, v, point, du, dv))
		break;
	    const double distance = point.DistanceTo(target);
	    if (distance < best_distance)
		best_distance = distance;
	    if (distance <= tolerance) {
		*refined = parameter;
		*refined_distance = distance;
		return true;
	    }
	    const ON_3dVector tangent = fixed_direction == 0 ? dv : du;
	    const double denominator = tangent * tangent;
	    if (!(denominator > ON_ZERO_TOLERANCE))
		break;
	    const ON_3dVector residual = point - target;
	    double step = -(residual * tangent) / denominator;
	    const double trust = 0.25 * varying_domain.Length();
	    step = std::max(-trust, std::min(trust, step));
	    bool accepted = false;
	    for (int line_search = 0; line_search < 8; ++line_search) {
		double candidate = parameter + step;
		if (surface->IsClosed(varying_direction)) {
		    const double period = varying_domain.Length();
		    candidate += round((parameter - candidate) / period) * period;
		} else {
		    candidate = std::max(varying_domain.Min(),
			std::min(varying_domain.Max(), candidate));
		}
		const ON_3dPoint candidate_point = fixed_direction == 0 ?
		    surface->PointAt(boundary, candidate) :
		    surface->PointAt(candidate, boundary);
		if (candidate_point.IsValid() &&
			candidate_point.DistanceTo(target) < distance) {
		    parameter = candidate;
		    accepted = true;
		    break;
		}
		step *= 0.5;
	    }
	    if (!accepted)
		break;
	}
	const ON_3dPoint final_point = fixed_direction == 0 ?
	    surface->PointAt(boundary, parameter) :
	    surface->PointAt(parameter, boundary);
	const double final_distance = final_point.IsValid() ?
	    final_point.DistanceTo(target) : DBL_MAX;
	if (final_distance < best_distance)
	    best_distance = final_distance;
	*refined = parameter;
	*refined_distance = best_distance;
	return final_distance <= tolerance;
	};

	double parameter = seed;
	double distance = DBL_MAX;
	if (refine(seed, &parameter, &distance)) {
	    *result = parameter;
	    return true;
	}

	/* The local seed may be on a different periodic branch.  Find the best
	 * boundary interval deterministically, then give Newton one bounded retry. */
	double boundary_seed = varying_domain.Min();
	double boundary_distance = DBL_MAX;
	for (int sample = 0; sample <= kBoundaryParameterSearchSegments; ++sample) {
	    const double candidate = varying_domain.ParameterAt(
		static_cast<double>(sample) / kBoundaryParameterSearchSegments);
	    const ON_3dPoint point = fixed_direction == 0 ?
		surface->PointAt(boundary, candidate) :
		surface->PointAt(candidate, boundary);
	    const double candidate_distance = point.IsValid() ?
		point.DistanceTo(target) : DBL_MAX;
	    if (candidate_distance < boundary_distance) {
		boundary_distance = candidate_distance;
		boundary_seed = candidate;
	    }
	}
	if (refine(boundary_seed, &parameter, &distance)) {
	    *result = parameter;
	    return true;
	}
	return false;
    };

    const auto make_candidate = [brep, &solve_boundary_parameter](
	const ON_BrepTrim &trim, const ON_BrepEdge &edge, const ON_Surface *surface,
	int fixed_direction, double boundary, double tolerance,
	const ON_3dPoint *desired_start, const ON_3dPoint *desired_end,
	int sample_count, double *score, std::string *failure_reason) -> ON_Curve * {
	if (failure_reason)
	    failure_reason->clear();
	if (sample_count < 2 || ((desired_start == NULL) != (desired_end == NULL)))
	    return NULL;
	ON_3dPointArray points;
	ON_SimpleArray<double> parameters;
	points.Reserve(sample_count + 1);
	parameters.Reserve(sample_count + 1);
	const ON_Interval trim_domain = trim.Domain();
	const ON_Interval edge_domain = edge.Domain();
	const ON_Interval varying_domain = surface->Domain(1 - fixed_direction);
	brlcad::PullbackContext fallback_context;
	double previous_parameter = ON_UNSET_VALUE;
	*score = 0.0;
	for (int sample = 0; sample <= sample_count; ++sample) {
	    const double fraction = static_cast<double>(sample) / sample_count;
	    const double edge_fraction = trim.m_bRev3d ? 1.0 - fraction : fraction;
	    const ON_3dPoint target = edge.PointAt(edge_domain.ParameterAt(edge_fraction));
	    const ON_3dPoint original_uv = trim.PointAt(
		trim_domain.ParameterAt(fraction));
	    ON_3dPoint exact_endpoint;
	    const bool use_exact_endpoint = desired_start &&
		(sample == 0 || sample == sample_count);
	    if (use_exact_endpoint) {
		exact_endpoint = sample == 0 ? *desired_start : *desired_end;
		const ON_3dPoint endpoint_lift = surface->PointAt(exact_endpoint.x,
		    exact_endpoint.y);
		if (!endpoint_lift.IsValid() || endpoint_lift.DistanceTo(target) > tolerance ||
			fabs(exact_endpoint[fixed_direction] - boundary) > ON_ZERO_TOLERANCE) {
		    if (failure_reason)
			*failure_reason = "adjacent exact endpoint failed boundary validation";
		    return NULL;
		}
	    }
	    const double expected_parameter = desired_start ?
		(1.0 - fraction) * (*desired_start)[1 - fixed_direction] +
		fraction * (*desired_end)[1 - fixed_direction] :
		original_uv[1 - fixed_direction];
	    double seed = expected_parameter;
	    if (!desired_start && sample > 0 && surface->IsClosed(1 - fixed_direction)) {
		const double period = varying_domain.Length();
		seed += round((previous_parameter - seed) / period) * period;
	    }
	    double varying_parameter = use_exact_endpoint ?
		exact_endpoint[1 - fixed_direction] : seed;
	    if (use_exact_endpoint && sample > 0 &&
		    surface->IsClosed(1 - fixed_direction)) {
		const double period = varying_domain.Length();
		if (period > ON_ZERO_TOLERANCE)
		    varying_parameter += round((previous_parameter - varying_parameter) /
			period) * period;
	    }
	    const double solve_tolerance = std::max(1.0e-10, 0.1 * tolerance);
	    if (!use_exact_endpoint && !solve_boundary_parameter(surface, fixed_direction, boundary,
		    target, seed, solve_tolerance, &varying_parameter)) {
		ON_2dPoint fallback_uv(seed, seed);
		ON_3dPoint fallback_lift;
		double fallback_distance = DBL_MAX;
		if (!fallback_context.SurfaceClosestPoint(surface, target, fallback_uv,
			fallback_lift, fallback_distance, 0, solve_tolerance,
			tolerance) || fallback_distance > tolerance ||
			!solve_boundary_parameter(surface, fixed_direction, boundary,
			    target, fallback_uv[1 - fixed_direction], solve_tolerance,
			    &varying_parameter)) {
		    if (failure_reason)
			*failure_reason = "boundary solve failed at sample " +
			    std::to_string(sample);
		    return NULL;
		}
	    }
	    if (!use_exact_endpoint && surface->IsClosed(1 - fixed_direction)) {
		const double period = varying_domain.Length();
		if (sample > 0)
		    varying_parameter += round((previous_parameter - varying_parameter) /
			period) * period;
	    }
	    ON_3dPoint uv;
	    uv[fixed_direction] = boundary;
	    uv[1 - fixed_direction] = varying_parameter;
	    uv.z = 0.0;
	    const double curve_parameter = trim_domain.ParameterAt(fraction);
	    if (points.Count() > 0 && uv.IsCoincident(points[points.Count() - 1])) {
		/* ON_PolylineCurve rejects coincident adjacent points.  Retain the
		 * latest edge parameter so a run of solver-equivalent samples does
		 * not change the remaining samples' edge/UV correspondence. */
		points[points.Count() - 1] = uv;
		parameters[parameters.Count() - 1] = curve_parameter;
	    } else {
		points.Append(uv);
		parameters.Append(curve_parameter);
	    }
	    *score += fabs(original_uv[fixed_direction] - boundary) /
		std::max(surface->Domain(fixed_direction).Length(), ON_ZERO_TOLERANCE);
	    previous_parameter = varying_parameter;
	}
	/* Closest-point and boundary solves may return equivalent values on
	 * opposite sides of a periodic parameter cut.  Normalize the completed
	 * chain cumulatively before monotonic iso classification; this changes no
	 * surface lift and removes a single artificial full-period jump. */
	if (surface->IsClosed(1 - fixed_direction)) {
	    const int varying_direction = 1 - fixed_direction;
	    const double period = varying_domain.Length();
	    if (period > ON_ZERO_TOLERANCE) {
		for (int point = 1; point < points.Count(); ++point)
		    points[point][varying_direction] += round((
			points[point - 1][varying_direction] -
			points[point][varying_direction]) / period) * period;
	    }
	}
	/* A closest-point solve on a highly compressed boundary can oscillate by a
	 * few parameter ulps even though the exact edge proceeds monotonically.
	 * Drop only non-progressing interior samples; the dense lift validation
	 * below proves that the resulting interpolation remains on the exact edge
	 * within model uncertainty. */
	if (points.Count() > 2) {
	    const int varying_direction = 1 - fixed_direction;
	    const double overall_delta = points[points.Count() - 1][varying_direction] -
		points[0][varying_direction];
	    if (fabs(overall_delta) > ON_ZERO_TOLERANCE) {
		ON_3dPointArray monotone_points;
		ON_SimpleArray<double> monotone_parameters;
		monotone_points.Reserve(points.Count());
		monotone_parameters.Reserve(parameters.Count());
		monotone_points.Append(points[0]);
		monotone_parameters.Append(parameters[0]);
		for (int point = 1; point + 1 < points.Count(); ++point) {
		    const double progress = (points[point][varying_direction] -
			monotone_points[monotone_points.Count() - 1][varying_direction]) *
			overall_delta;
		    if (progress <= ON_ZERO_TOLERANCE)
			continue;
		    monotone_points.Append(points[point]);
		    monotone_parameters.Append(parameters[point]);
		}
		monotone_points.Append(points[points.Count() - 1]);
		monotone_parameters.Append(parameters[parameters.Count() - 1]);
		points = monotone_points;
		parameters = monotone_parameters;
	    }
	}
	if (points.Count() < 2 || parameters.Count() != points.Count()) {
	    if (failure_reason)
		*failure_reason = "candidate samples collapsed to an invalid domain";
	    return NULL;
	}
	parameters[0] = trim_domain.Min();
	parameters[parameters.Count() - 1] = trim_domain.Max();
	ON_PolylineCurve *candidate = new ON_PolylineCurve(points, parameters);
	if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
	    if (failure_reason)
		*failure_reason = "candidate polyline was invalid";
	    delete candidate;
	    return NULL;
	}
	if (trim.m_vi[0] != trim.m_vi[1] && candidate->IsClosed()) {
	    if (failure_reason)
		*failure_reason = "open-topology seam candidate was closed";
	    delete candidate;
	    return NULL;
	}
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const int vertex_index = trim.m_vi[endpoint];
	    const ON_3dPoint uv = candidate->PointAt(
		trim_domain[endpoint]);
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint edge_endpoint = edge.PointAt(edge_domain[
		trim.m_bRev3d ? 1 - endpoint : endpoint]);
	    if (vertex_index < 0 || vertex_index >= brep->m_V.Count() ||
		!lift.IsValid() || !edge_endpoint.IsValid() ||
		lift.DistanceTo(brep->m_V[vertex_index].point) > tolerance ||
		lift.DistanceTo(edge_endpoint) > tolerance) {
		if (failure_reason)
		    *failure_reason = "seam endpoint did not match its exact topology vertex";
		delete candidate;
		return NULL;
	    }
	}
	const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	ON_Surface::ISO expected = fixed_direction == 0 ?
	    ON_Surface::x_iso : ON_Surface::y_iso;
	if (fabs(boundary - fixed_domain.Min()) <= ON_ZERO_TOLERANCE)
	    expected = fixed_direction == 0 ? ON_Surface::W_iso : ON_Surface::S_iso;
	else if (fabs(boundary - fixed_domain.Max()) <= ON_ZERO_TOLERANCE)
	    expected = fixed_direction == 0 ? ON_Surface::E_iso : ON_Surface::N_iso;
	const ON_Surface::ISO derived_iso = surface->IsIsoparametric(
	    *candidate, &trim_domain);
	if (derived_iso != expected) {
	    if (failure_reason) {
		int reversals = 0;
		int first_reversal = -1;
		double reversal_start = ON_UNSET_VALUE;
		double reversal_end = ON_UNSET_VALUE;
		const int varying_direction = 1 - fixed_direction;
		const double overall_delta = points[points.Count() - 1][varying_direction] -
		    points[0][varying_direction];
		for (int point = 1; point < points.Count(); ++point) {
		    const double delta = points[point][varying_direction] -
			points[point - 1][varying_direction];
		    if (delta * overall_delta < -ON_ZERO_TOLERANCE) {
			if (first_reversal < 0) {
			    first_reversal = point;
			    reversal_start = points[point - 1][varying_direction];
			    reversal_end = points[point][varying_direction];
			}
			++reversals;
		    }
		}
		*failure_reason = "candidate was not monotone isoparametric (derived " +
		    std::to_string(static_cast<int>(derived_iso)) + ", expected " +
		    std::to_string(static_cast<int>(expected)) + ", varying " +
		    std::to_string(points[0][varying_direction]) + "->" +
		    std::to_string(points[points.Count() - 1][varying_direction]) +
		    ", reversals " + std::to_string(reversals) +
		    (first_reversal >= 0 ? ", first " +
			std::to_string(first_reversal) + " " +
			std::to_string(reversal_start) + "->" +
			std::to_string(reversal_end) : std::string()) + ")";
	    }
	    delete candidate;
	    return NULL;
	}
	for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		kDenseValidationSegments;
	    const double edge_fraction = trim.m_bRev3d ? 1.0 - fraction : fraction;
	    const ON_3dPoint uv = candidate->PointAt(trim_domain.ParameterAt(fraction));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint target = edge.PointAt(edge_domain.ParameterAt(edge_fraction));
	    if (!lift.IsValid() || lift.DistanceTo(target) > tolerance) {
		if (failure_reason)
		    *failure_reason = "dense lift failed at sample " +
			std::to_string(sample) + " with distance " +
			std::to_string(lift.IsValid() ? lift.DistanceTo(target) : DBL_MAX);
		delete candidate;
		return NULL;
	    }
	}
	return candidate;
    };

    const auto align_revolution_seam = [brep](const ON_BrepEdge &edge,
	const ON_BrepLoop &loop, const ON_Surface *surface, double tolerance,
	std::string *failure_reason) {
	if (failure_reason)
	    *failure_reason = "surface was not a full closed revolution";
	ON_RevSurface *revolution = ON_RevSurface::Cast(
	    const_cast<ON_Surface *>(surface));
	const ON_BrepFace *source_face = loop.Face();
	if (!revolution || !source_face ||
		fabs(revolution->m_angle.Length() - ON_2PI) > ON_ZERO_TOLERANCE)
	    return false;
	const int angle_direction = revolution->m_bTransposed ? 1 : 0;
	if (!surface->IsClosed(angle_direction)) {
	    if (failure_reason)
		*failure_reason = "revolution angle direction was not closed";
	    return false;
	}
	const ON_Interval domain = surface->Domain(angle_direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    return false;

	brlcad::PullbackContext context;
	double parameters[3] = {0.0, 0.0, 0.0};
	for (int sample = 0; sample < 3; ++sample) {
	    const double fraction = 0.25 * (sample + 1);
	    const ON_3dPoint target = edge.PointAt(edge.Domain().ParameterAt(fraction));
	    ON_2dPoint uv(domain.Mid(), surface->Domain(1 - angle_direction).Mid());
	    ON_3dPoint lift;
	    double distance = DBL_MAX;
	    if (!context.SurfaceClosestPoint(surface, target, uv, lift, distance, 0,
		    std::max(1.0e-10, 0.1 * tolerance), tolerance) ||
		    distance > tolerance) {
		if (failure_reason)
		    *failure_reason = "surface closest point failed at interior sample " +
			std::to_string(sample) + " with distance " +
			std::to_string(distance);
		return false;
	    }
	    parameters[sample] = uv[angle_direction];
	    if (sample > 0)
		parameters[sample] += round((parameters[0] - parameters[sample]) /
		    period) * period;
	}
	const double minimum = std::min(parameters[0],
	    std::min(parameters[1], parameters[2]));
	const double maximum = std::max(parameters[0],
	    std::max(parameters[1], parameters[2]));
	if (maximum - minimum > std::max(tolerance, 1.0e-7 * period)) {
	    if (failure_reason)
		*failure_reason = "exact edge was not constant in revolution angle (" +
		    std::to_string(parameters[0]) + "/" +
		    std::to_string(parameters[1]) + "/" +
		    std::to_string(parameters[2]) + ")";
	    return false;
	}
	const double constant_tolerance = std::max(tolerance, 1.0e-7 * period);
	const double interior_seam = (parameters[0] + parameters[1] + parameters[2]) /
	    3.0;
	double endpoint_sum = 0.0;
	int endpoint_samples = 0;
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const ON_3dPoint target = edge.PointAt(edge.Domain()[endpoint]);
	    ON_2dPoint uv(domain.Mid(), surface->Domain(1 - angle_direction).Mid());
	    ON_3dPoint lift;
	    double distance = DBL_MAX;
	    if (!context.SurfaceClosestPoint(surface, target, uv, lift, distance, 0,
		    std::max(1.0e-10, 0.1 * tolerance), tolerance) ||
		    distance > tolerance)
		continue;
	    double endpoint_parameter = uv[angle_direction];
	    endpoint_parameter += round((interior_seam - endpoint_parameter) / period) *
		period;
	    if (fabs(endpoint_parameter - interior_seam) > constant_tolerance)
		continue;
	    endpoint_sum += endpoint_parameter;
	    ++endpoint_samples;
	}
	double seam = endpoint_samples == 2 ?
	    (endpoint_sum + parameters[1]) / 3.0 : interior_seam;
	while (seam < domain.Min()) seam += period;
	while (seam > domain.Max()) seam -= period;
	if (fabs(seam - domain.Min()) <= ON_ZERO_TOLERANCE ||
		fabs(seam - domain.Max()) <= ON_ZERO_TOLERANCE) {
	    if (failure_reason)
		*failure_reason = "exact revolution seam was already on the domain boundary";
	    return false;
	}

	ON_RevSurface candidate = *revolution;
	const double angle = revolution->m_angle.ParameterAt(
	    domain.NormalizedParameterAt(seam));
	candidate.m_angle.Set(angle, angle + ON_2PI);
	candidate.m_t.Set(domain.Min(), domain.Max());
	double shifts[2] = {0.0, 0.0};
	shifts[angle_direction] = domain.Min() - seam;
	ON_Xform transform(ON_Xform::IdentityTransformation);
	transform.m_xform[0][3] = shifts[0];
	transform.m_xform[1][3] = shifts[1];

	std::vector<int> trim_indices;
	std::vector<ON_Curve *> transformed_curves;
	for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	    const ON_BrepFace &face = brep->m_F[fi];
	    if (face.m_si != source_face->m_si)
		continue;
	    for (int fli = 0; fli < face.LoopCount(); ++fli) {
		const ON_BrepLoop *affected_loop = face.Loop(fli);
		if (!affected_loop)
		    continue;
		for (int lti = 0; lti < affected_loop->TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = affected_loop->Trim(lti);
		    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
		    ON_BoundingBox curve_box;
		    if (!trim || !curve || !curve->Transform(transform) ||
			    !curve->GetBoundingBox(curve_box)) {
			delete curve;
			for (std::vector<ON_Curve *>::iterator cleanup =
				transformed_curves.begin(); cleanup != transformed_curves.end();
			     ++cleanup)
			    delete *cleanup;
			return false;
		    }
		    ON_Xform wrap_transform(ON_Xform::IdentityTransformation);
		    bool wrap_required = false;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!candidate.IsClosed(direction) ||
				fabs(shifts[direction]) <= ON_ZERO_TOLERANCE)
			    continue;
			const ON_Interval candidate_domain = candidate.Domain(direction);
			const double candidate_period = candidate_domain.Length();
			if (!(candidate_period > ON_ZERO_TOLERANCE))
			    continue;
			const double curve_center = 0.5 *
			    (curve_box.m_min[direction] + curve_box.m_max[direction]);
			const double wrap = round((candidate_domain.Mid() - curve_center) /
			    candidate_period) * candidate_period;
			if (fabs(wrap) <= ON_ZERO_TOLERANCE)
			    continue;
			wrap_transform.m_xform[direction][3] = wrap;
			wrap_required = true;
		    }
		    if (wrap_required) {
			if (!curve->Transform(wrap_transform)) {
			    delete curve;
			    for (std::vector<ON_Curve *>::iterator cleanup =
				    transformed_curves.begin(); cleanup != transformed_curves.end();
				 ++cleanup)
				delete *cleanup;
			    return false;
			}
		    }
		    if (!curve->ChangeDimension(2) || !curve->IsValid()) {
			delete curve;
			for (std::vector<ON_Curve *>::iterator cleanup =
				transformed_curves.begin(); cleanup != transformed_curves.end();
			     ++cleanup)
			    delete *cleanup;
			return false;
		    }
		    const ON_Interval trim_domain = trim->Domain();
		    const int samples = std::min(256,
			std::max(32, trim->SpanCount() * 4));
		    bool valid = true;
		    for (int sample = 0; sample <= samples; ++sample) {
			const double parameter = trim_domain.ParameterAt(
			    static_cast<double>(sample) / samples);
			const ON_3dPoint original_uv = trim->PointAt(parameter);
			const ON_3dPoint transformed_uv = curve->PointAt(parameter);
			const ON_3dPoint original_lift = surface->PointAt(
			    original_uv.x, original_uv.y);
			const ON_3dPoint transformed_lift = candidate.PointAt(
			    transformed_uv.x, transformed_uv.y);
			if (!original_lift.IsValid() || !transformed_lift.IsValid() ||
				original_lift.DistanceTo(transformed_lift) > tolerance) {
			    if (failure_reason)
				*failure_reason = "revolution seam transform changed an affected pcurve lift by " +
				    std::to_string(original_lift.DistanceTo(transformed_lift));
			    valid = false;
			    break;
			}
		    }
		    if (!valid) {
			delete curve;
			for (std::vector<ON_Curve *>::iterator cleanup =
				transformed_curves.begin(); cleanup != transformed_curves.end();
			     ++cleanup)
			    delete *cleanup;
			return false;
		    }
		    trim_indices.push_back(trim->m_trim_index);
		    transformed_curves.push_back(curve);
		}
	    }
	}
	std::vector<int> c2_indices;
	c2_indices.reserve(transformed_curves.size());
	for (std::vector<ON_Curve *>::iterator curve = transformed_curves.begin();
	     curve != transformed_curves.end(); ++curve) {
	    const int c2_index = brep->AddTrimCurve(*curve);
	    if (c2_index < 0)
		return false;
	    c2_indices.push_back(c2_index);
	}
	*revolution = candidate;
	for (size_t i = 0; i < trim_indices.size(); ++i) {
	    if (!brep->SetTrimCurve(brep->m_T[trim_indices[i]], c2_indices[i]))
		return false;
	    brep->SetTrimIsoFlags(brep->m_T[trim_indices[i]]);
	}
	return true;
    };

    const auto mirror_candidate = [](const ON_Curve &source,
	const ON_BrepTrim &source_trim, const ON_BrepTrim &target_trim,
	const ON_BrepEdge &edge, const ON_Surface *surface, int fixed_direction,
	double boundary, double tolerance) -> ON_Curve * {
	ON_Curve *candidate = source.DuplicateCurve();
	if (!candidate)
	    return NULL;
	if (source_trim.m_bRev3d != target_trim.m_bRev3d && !candidate->Reverse()) {
	    delete candidate;
	    return NULL;
	}
	ON_Xform projection(ON_Xform::IdentityTransformation);
	for (int column = 0; column < 4; ++column)
	    projection.m_xform[fixed_direction][column] = 0.0;
	projection.m_xform[fixed_direction][3] = boundary;
	const ON_Interval target_domain = target_trim.Domain();
	if (!candidate->Transform(projection) || !candidate->ChangeDimension(2) ||
		!candidate->SetDomain(target_domain.Min(), target_domain.Max()) ||
		!candidate->IsValid()) {
	    delete candidate;
	    return NULL;
	}
	const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	ON_Surface::ISO expected = fixed_direction == 0 ?
	    ON_Surface::x_iso : ON_Surface::y_iso;
	if (fabs(boundary - fixed_domain.Min()) <= ON_ZERO_TOLERANCE)
	    expected = fixed_direction == 0 ? ON_Surface::W_iso : ON_Surface::S_iso;
	else if (fabs(boundary - fixed_domain.Max()) <= ON_ZERO_TOLERANCE)
	    expected = fixed_direction == 0 ? ON_Surface::E_iso : ON_Surface::N_iso;
	if (surface->IsIsoparametric(*candidate, &target_domain) != expected) {
	    delete candidate;
	    return NULL;
	}
	const ON_Interval edge_domain = edge.Domain();
	for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		kDenseValidationSegments;
	    const double edge_fraction = target_trim.m_bRev3d ? 1.0 - fraction : fraction;
	    const ON_3dPoint uv = candidate->PointAt(
		target_domain.ParameterAt(fraction));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint target = edge.PointAt(edge_domain.ParameterAt(edge_fraction));
	    if (!lift.IsValid() || lift.DistanceTo(target) > tolerance) {
		delete candidate;
		return NULL;
	    }
	}
	return candidate;
    };

    std::vector<int> repaired_seam_loops;
    if (additional_repaired_loops) {
	for (std::vector<int>::const_iterator loop = additional_repaired_loops->begin();
		loop != additional_repaired_loops->end(); ++loop) {
	    if (*loop >= 0 && *loop < brep->m_L.Count() &&
		    std::find(repaired_seam_loops.begin(), repaired_seam_loops.end(),
			*loop) == repaired_seam_loops.end())
		repaired_seam_loops.push_back(*loop);
	}
    }
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const int first_index = edge.m_ti[0];
	const int second_index = edge.m_ti[1];
	if (first_index < 0 || first_index >= brep->m_T.Count() ||
		second_index < 0 || second_index >= brep->m_T.Count())
	    continue;
	ON_BrepTrim &first = brep->m_T[first_index];
	ON_BrepTrim &second = brep->m_T[second_index];
	if (first.m_type != ON_BrepTrim::seam || second.m_type != ON_BrepTrim::seam ||
		first.m_li < 0 || first.m_li != second.m_li ||
		first.m_li >= brep->m_L.Count())
	    continue;
	const ON_BrepLoop &loop = brep->m_L[first.m_li];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;

	bool needs_repair = first.m_iso == ON_Surface::not_iso ||
	    second.m_iso == ON_Surface::not_iso;
	const ON_BrepTrim *pair[2] = {&first, &second};
	for (int member = 0; member < 2 && !needs_repair; ++member) {
	    const ON_Interval trim_domain = pair[member]->Domain();
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		const ON_3dPoint uv = pair[member]->PointAt(trim_domain[endpoint]);
		const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		const ON_3dPoint target = edge.PointAt(edge.Domain()[
		    pair[member]->m_bRev3d ? 1 - endpoint : endpoint]);
		if (!lift.IsValid() || lift.DistanceTo(target) > LocalUnits::tolerance) {
		    needs_repair = true;
		    break;
		}
	    }
	}
	if (!needs_repair)
	    continue;
	std::string revolution_alignment_failure;
	if (align_revolution_seam(edge, loop, surface, LocalUnits::tolerance,
		&revolution_alignment_failure)) {
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"aligned a periodic revolution surface seam with an exact edge");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": aligned revolution seam with STEP edge "
		    << edge.m_edge_user.i << std::endl;
	} else if (wrapper->Verbose()) {
	    std::cerr << entity_type << " #" << entity_id
		<< ": revolution seam alignment rejected for STEP edge "
		<< edge.m_edge_user.i << ": " << revolution_alignment_failure
		<< std::endl;
	}
	std::string surface_alignment_failure;
	if (allow_surface_alignment &&
		align_closed_surface_seam_from_trim_pair(brep, edge, loop, surface,
		LocalUnits::tolerance, &surface_alignment_failure)) {
	    if (aligned_surface_loops && std::find(aligned_surface_loops->begin(),
		    aligned_surface_loops->end(), first.m_li) ==
		    aligned_surface_loops->end())
		aligned_surface_loops->push_back(first.m_li);
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"aligned a periodic surface seam with an exact edge");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": aligned periodic surface seam with STEP edge "
		    << edge.m_edge_user.i << std::endl;
	} else if (allow_surface_alignment && wrapper->Verbose()) {
	    std::cerr << entity_type << " #" << entity_id
		<< ": periodic surface seam alignment rejected for STEP edge "
		<< edge.m_edge_user.i << ": " << surface_alignment_failure
		<< std::endl;
	}

	ON_Curve *best_first = NULL;
	ON_Curve *best_second = NULL;
	double best_score = DBL_MAX;
	std::ostringstream candidate_failures;
	const auto loop_closure_score = [brep, &loop, first_index, second_index](
	    const ON_Curve *first_candidate, const ON_Curve *second_candidate) {
	    double score = 0.0;
	    for (int lti = 0; lti < loop.m_ti.Count(); ++lti) {
		const int trim_index = loop.m_ti[lti];
		if (trim_index != first_index && trim_index != second_index)
		    continue;
		const int previous_index = loop.m_ti[
		    (lti + loop.m_ti.Count() - 1) % loop.m_ti.Count()];
		const int next_index = loop.m_ti[(lti + 1) % loop.m_ti.Count()];
		const ON_Curve *candidate = trim_index == first_index ?
		    first_candidate : second_candidate;
		if (!candidate)
		    return DBL_MAX;
		const ON_3dPoint previous_end = previous_index == first_index ?
		    first_candidate->PointAtEnd() : previous_index == second_index ?
		    second_candidate->PointAtEnd() :
		    brep->m_T[previous_index].PointAtEnd();
		const ON_3dPoint next_start = next_index == first_index ?
		    first_candidate->PointAtStart() : next_index == second_index ?
		    second_candidate->PointAtStart() :
		    brep->m_T[next_index].PointAtStart();
		score += candidate->PointAtStart().DistanceTo(previous_end);
		score += candidate->PointAtEnd().DistanceTo(next_start);
	    }
	    return score;
	};
	const auto adjacent_endpoints = [brep, &loop, surface](int trim_index,
	    int fixed_direction, double boundary, ON_3dPoint *start,
	    ON_3dPoint *end) {
	    if (!start || !end)
		return false;
	    int offset = -1;
	    for (int lti = 0; lti < loop.m_ti.Count(); ++lti) {
		if (loop.m_ti[lti] == trim_index) {
		    offset = lti;
		    break;
		}
	    }
	    if (offset < 0)
		return false;
	    const ON_BrepTrim *previous = loop.Trim(
		(offset + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((offset + 1) % loop.TrimCount());
	    const ON_BrepTrim *trim = brep->Trim(trim_index);
	    if (!previous || !next || !trim ||
		previous->m_vi[1] != trim->m_vi[0] ||
		trim->m_vi[1] != next->m_vi[0])
		return false;
	    *start = previous->PointAtEnd();
	    *end = next->PointAtStart();
	    const double parameter_tolerance = ON_ZERO_TOLERANCE *
		kNumericalToleranceScale;
	    if (fabs((*start)[fixed_direction] - boundary) <= parameter_tolerance &&
		    fabs((*end)[fixed_direction] - boundary) <= parameter_tolerance)
		return true;

	    /* An adjacent exact-edge pullback may stop a few parameter ulps inside
	     * a newly moved surface seam.  Project just that fixed coordinate to
	     * the boundary, and admit it only when both projected endpoints still
	     * lift to their authoritative topology vertices within model
	     * uncertainty. */
	    ON_3dPoint projected_start = *start;
	    ON_3dPoint projected_end = *end;
	    projected_start[fixed_direction] = boundary;
	    projected_end[fixed_direction] = boundary;
	    const ON_3dPoint start_lift = surface->PointAt(projected_start.x,
		projected_start.y);
	    const ON_3dPoint end_lift = surface->PointAt(projected_end.x,
		projected_end.y);
	    if (!start_lift.IsValid() || !end_lift.IsValid() ||
		trim->m_vi[0] < 0 || trim->m_vi[0] >= brep->m_V.Count() ||
		trim->m_vi[1] < 0 || trim->m_vi[1] >= brep->m_V.Count() ||
		start_lift.DistanceTo(brep->m_V[trim->m_vi[0]].point) >
		    LocalUnits::tolerance ||
		end_lift.DistanceTo(brep->m_V[trim->m_vi[1]].point) >
		    LocalUnits::tolerance)
		return false;
	    *start = projected_start;
	    *end = projected_end;
	    return true;
	};
	const auto exact_edge_endpoints = [brep, surface, &edge,
	    &solve_boundary_parameter](int trim_index, int fixed_direction,
	    double boundary, ON_3dPoint *start, ON_3dPoint *end) {
	    if (!start || !end)
		return false;
	    const ON_BrepTrim *trim = brep->Trim(trim_index);
	    if (!trim)
		return false;
	    ON_3dPoint *endpoint_uv[2] = {start, end};
	    brlcad::PullbackContext context;
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		const int vertex_index = trim->m_vi[endpoint];
		if (vertex_index < 0 || vertex_index >= brep->m_V.Count())
		    return false;
		const ON_3dPoint edge_target = edge.PointAt(edge.Domain()[
		    trim->m_bRev3d ? 1 - endpoint : endpoint]);
		const ON_3dPoint target = brep->m_V[vertex_index].point;
		if (!edge_target.IsValid() || edge_target.DistanceTo(target) >
			LocalUnits::tolerance)
		    return false;
		ON_2dPoint pulled_uv(surface->Domain(0).Mid(),
		    surface->Domain(1).Mid());
		ON_3dPoint pulled_lift;
		double distance = DBL_MAX;
		if (!context.SurfaceClosestPoint(surface, target, pulled_uv,
			pulled_lift, distance, 0,
			std::max(1.0e-10, LocalUnits::tolerance * 1.0e-6),
			LocalUnits::tolerance) || distance > LocalUnits::tolerance)
		    return false;
		double varying_parameter = pulled_uv[1 - fixed_direction];
		if (!solve_boundary_parameter(surface, fixed_direction, boundary,
			target, varying_parameter,
			std::max(1.0e-10, 0.1 * LocalUnits::tolerance),
			&varying_parameter))
		    return false;
		(*endpoint_uv[endpoint])[fixed_direction] = boundary;
		(*endpoint_uv[endpoint])[1 - fixed_direction] = varying_parameter;
		(*endpoint_uv[endpoint]).z = 0.0;
		const ON_3dPoint lift = surface->PointAt(endpoint_uv[endpoint]->x,
		    endpoint_uv[endpoint]->y);
		if (!lift.IsValid() || lift.DistanceTo(target) >
			LocalUnits::tolerance || lift.DistanceTo(
			edge_target) > LocalUnits::tolerance)
		    return false;
	    }
	    const int varying_direction = 1 - fixed_direction;
	    if (surface->IsClosed(varying_direction)) {
		const double period = surface->Domain(varying_direction).Length();
		if (period > ON_ZERO_TOLERANCE) {
		    /* The two equivalent end branches can be exactly half a period
		     * from the start.  round() then chooses a branch arbitrarily and
		     * can introduce a single reversal at the final sample.  Pull back
		     * the immutable edge midpoint and choose the end branch that
		     * continues through it monotonically. */
		    const ON_3dPoint midpoint_target = edge.PointAt(
			edge.Domain().ParameterAt(0.5));
		    ON_2dPoint midpoint_uv(surface->Domain(0).Mid(),
			surface->Domain(1).Mid());
		    ON_3dPoint midpoint_lift;
		    double midpoint_distance = DBL_MAX;
		    double midpoint_parameter = ON_UNSET_VALUE;
		    const bool have_midpoint = context.SurfaceClosestPoint(surface,
			midpoint_target, midpoint_uv, midpoint_lift,
			midpoint_distance, 0,
			std::max(1.0e-10, LocalUnits::tolerance * 1.0e-6),
			LocalUnits::tolerance) &&
			midpoint_distance <= LocalUnits::tolerance &&
			solve_boundary_parameter(surface, fixed_direction, boundary,
			    midpoint_target, midpoint_uv[varying_direction],
			    std::max(1.0e-10, 0.1 * LocalUnits::tolerance),
			    &midpoint_parameter);
		    if (have_midpoint) {
			midpoint_parameter += round(((*start)[varying_direction] -
			    midpoint_parameter) / period) * period;
			double best_end = (*end)[varying_direction];
			double best_branch_score = DBL_MAX;
			for (int shift = -2; shift <= 2; ++shift) {
			    const double candidate_end = (*end)[varying_direction] +
				shift * period;
			    const double first_delta = midpoint_parameter -
				(*start)[varying_direction];
			    const double second_delta = candidate_end - midpoint_parameter;
			    const bool reversal = first_delta * second_delta <
				-ON_ZERO_TOLERANCE;
			    const double score = (reversal ? 10.0 * period : 0.0) +
				fabs(first_delta - second_delta) +
				1.0e-9 * (fabs(first_delta) + fabs(second_delta));
			    if (score < best_branch_score) {
				best_branch_score = score;
				best_end = candidate_end;
			    }
			}
			(*end)[varying_direction] = best_end;
		    } else {
			(*end)[varying_direction] += round(((*start)[varying_direction] -
			    (*end)[varying_direction]) / period) * period;
		    }
		}
	    }
	    return true;
	};
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = surface->Domain(direction);
	    for (int assignment = 0; assignment < 2; ++assignment) {
		double first_score = 0.0;
		double second_score = 0.0;
		std::string first_failure;
		std::string second_failure;
		ON_3dPoint first_start, first_end, second_start, second_end;
		bool have_first_endpoints = exact_edge_endpoints(first_index,
		    direction, domain[assignment], &first_start, &first_end);
		bool have_second_endpoints = exact_edge_endpoints(second_index,
		    direction, domain[1 - assignment], &second_start, &second_end);
		if (!have_first_endpoints)
		    have_first_endpoints = adjacent_endpoints(first_index,
			direction, domain[assignment], &first_start, &first_end);
		if (!have_second_endpoints)
		    have_second_endpoints = adjacent_endpoints(second_index,
			direction, domain[1 - assignment], &second_start, &second_end);
		ON_Curve *first_candidate = make_candidate(first, edge, surface,
		    direction, domain[assignment], LocalUnits::tolerance,
		    have_first_endpoints ? &first_start : NULL,
		    have_first_endpoints ? &first_end : NULL,
		    256, &first_score, &first_failure);
		ON_Curve *second_candidate = make_candidate(second, edge, surface,
		    direction, domain[1 - assignment], LocalUnits::tolerance,
		    have_second_endpoints ? &second_start : NULL,
		    have_second_endpoints ? &second_end : NULL,
		    256, &second_score, &second_failure);
		if (!first_candidate && second_candidate) {
		    first_candidate = mirror_candidate(*second_candidate, second, first,
			edge, surface, direction, domain[assignment],
			LocalUnits::tolerance);
		    if (first_candidate) {
			first_failure.clear();
			first_score = second_score;
		    }
		}
		if (!second_candidate && first_candidate) {
		    second_candidate = mirror_candidate(*first_candidate, first, second,
			edge, surface, direction, domain[1 - assignment],
			LocalUnits::tolerance);
		    if (second_candidate) {
			second_failure.clear();
			second_score = first_score;
		    }
		}
		if (!first_candidate || !second_candidate)
		    candidate_failures << " d" << direction << 'a' << assignment
			<< " first=" << (first_failure.empty() ? "ok" : first_failure)
			<< " second=" << (second_failure.empty() ? "ok" : second_failure);
		const double closure_score = first_candidate && second_candidate ?
		    loop_closure_score(first_candidate, second_candidate) : DBL_MAX;
		const double candidate_score = closure_score +
		    1.0e-9 * (first_score + second_score);
		if (!first_candidate || !second_candidate || candidate_score >= best_score) {
		    delete first_candidate;
		    delete second_candidate;
		    continue;
		}
		delete best_first;
		delete best_second;
		best_first = first_candidate;
		best_second = second_candidate;
		best_score = candidate_score;
	    }
	}
	if (!best_first || !best_second) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": exact seam regeneration rejected for edge " << ei
		    << "/STEP" << edge.m_edge_user.i << candidate_failures.str()
		    << std::endl;
	    delete best_first;
	    delete best_second;
	    continue;
	}
	const int first_c2 = brep->AddTrimCurve(best_first);
	const int second_c2 = brep->AddTrimCurve(best_second);
	if (first_c2 < 0 || second_c2 < 0 ||
		!brep->SetTrimCurve(first, first_c2) ||
		!brep->SetTrimCurve(second, second_c2)) {
	    if (first_c2 < 0)
		delete best_first;
	    if (second_c2 < 0)
		delete best_second;
	    continue;
	}
	brep->SetTrimIsoFlags(first);
	brep->SetTrimIsoFlags(second);
	if (std::find(repaired_seam_loops.begin(), repaired_seam_loops.end(),
		first.m_li) == repaired_seam_loops.end())
	    repaired_seam_loops.push_back(first.m_li);
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "regenerated paired seam pcurves from the exact edge");
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": regenerated seam pair "
		<< first_index << '/' << second_index << " from exact STEP edge "
		<< edge.m_edge_user.i << std::endl;
    }

    /* An exact isoparametric 3D edge on a periodic surface is frequently
     * supplied with a diagonal or slightly displaced pcurve.  Do not rely on
     * ON_BrepEdge::IsClosed(): imported circle proxies can have a closed STEP
     * topology without reporting closed here, and open isoparametric edges
     * need the same treatment.  Limit this repair to loops whose seam pair was
     * regenerated, require an actual p-space closure defect (or an unclassified
     * pcurve), prove a constant surface parameter with independent interior
     * samples, and densely validate the replacement against the exact edge. */
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_type == ON_BrepTrim::seam ||
		trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
		std::find(repaired_seam_loops.begin(), repaired_seam_loops.end(),
		    trim.m_li) == repaired_seam_loops.end())
	    continue;
	ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	ON_BrepLoop &loop = brep->m_L[trim.m_li];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	int trim_offset = -1;
	for (int lti = 0; lti < loop.m_ti.Count(); ++lti) {
	    if (loop.m_ti[lti] == ti) {
		trim_offset = lti;
		break;
	    }
	}
	if (trim_offset < 0)
	    continue;
	const ON_BrepTrim *previous = loop.Trim(
	    (trim_offset + loop.TrimCount() - 1) % loop.TrimCount());
	const ON_BrepTrim *next = loop.Trim(
	    (trim_offset + 1) % loop.TrimCount());
	if (!previous || !next || previous->m_vi[1] != trim.m_vi[0] ||
		trim.m_vi[1] != next->m_vi[0])
	    continue;
	const bool has_start_gap = trim.PointAtStart().DistanceTo(
	    previous->PointAtEnd()) > ON_ZERO_TOLERANCE;
	const bool has_end_gap = trim.PointAtEnd().DistanceTo(
	    next->PointAtStart()) > ON_ZERO_TOLERANCE;
	if (trim.m_iso != ON_Surface::not_iso && !has_start_gap && !has_end_gap)
	    continue;
	brlcad::PullbackContext context;
	double uv_parameters[3][2];
	bool pulled = true;
	for (int sample = 0; sample < 3; ++sample) {
	    const double fraction = 0.125 + 0.375 * sample;
	    const ON_3dPoint target = edge.PointAt(edge.Domain().ParameterAt(fraction));
	    ON_2dPoint uv(surface->Domain(0).Mid(), surface->Domain(1).Mid());
	    ON_3dPoint lift;
	    double distance = DBL_MAX;
	    if (!context.SurfaceClosestPoint(surface, target, uv, lift, distance, 0,
		    std::max(1.0e-10, 0.1 * LocalUnits::tolerance),
		    LocalUnits::tolerance) || distance > LocalUnits::tolerance) {
		pulled = false;
		break;
	    }
	    ON_3dPoint refined_uv(uv.x, uv.y, 0.0);
	    double refined_distance = DBL_MAX;
	    const double numerical_tolerance = std::max(1.0e-11,
		std::min(1.0e-8, 1.0e-8 * LocalUnits::tolerance));
	    if (!refine_surface_pullback_seeded(surface, target,
		    numerical_tolerance, refined_uv, &refined_distance))
		refined_uv.Set(uv.x, uv.y, 0.0);
	    uv_parameters[sample][0] = refined_uv.x;
	    uv_parameters[sample][1] = refined_uv.y;
	}
	if (!pulled)
	    continue;

	ON_Curve *best = NULL;
	double best_score = DBL_MAX;
	std::string candidate_failures;
	for (int fixed_direction = 0; fixed_direction < 2; ++fixed_direction) {
	    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	    const double fixed_period = fixed_domain.Length();
	    double coordinates[3] = {uv_parameters[0][fixed_direction],
		uv_parameters[1][fixed_direction], uv_parameters[2][fixed_direction]};
	    if (surface->IsClosed(fixed_direction) && fixed_period > ON_ZERO_TOLERANCE) {
		for (int sample = 1; sample < 3; ++sample)
		    coordinates[sample] += round((coordinates[0] - coordinates[sample]) /
			fixed_period) * fixed_period;
	    }
	    const double minimum = std::min(coordinates[0],
		std::min(coordinates[1], coordinates[2]));
	    const double maximum = std::max(coordinates[0],
		std::max(coordinates[1], coordinates[2]));
	    if (maximum - minimum > std::max(LocalUnits::tolerance,
		    1.0e-7 * fixed_period))
		continue;
	    double fixed_parameter = (coordinates[0] + coordinates[1] +
		coordinates[2]) / 3.0;
	    /* When both topologically adjacent trims meet this edge at the same
	     * constant parameter, use that exact p-space coordinate.  The dense
	     * edge validation below still proves it represents the 3D edge, while
	     * exact reuse prevents a numerical closest-point residue from opening
	     * the loop. */
	    double adjacent_fixed[2] = {previous->PointAtEnd()[fixed_direction],
		next->PointAtStart()[fixed_direction]};
	    if (surface->IsClosed(fixed_direction) && fixed_period > ON_ZERO_TOLERANCE)
		adjacent_fixed[1] += round((adjacent_fixed[0] - adjacent_fixed[1]) /
		    fixed_period) * fixed_period;
	    if (fabs(adjacent_fixed[0] - adjacent_fixed[1]) <= ON_ZERO_TOLERANCE)
		fixed_parameter = adjacent_fixed[0];
	    if (surface->IsClosed(fixed_direction) && fixed_period > ON_ZERO_TOLERANCE) {
		while (fixed_parameter < fixed_domain.Min()) fixed_parameter += fixed_period;
		while (fixed_parameter > fixed_domain.Max()) fixed_parameter -= fixed_period;
	    }
	    const ON_3dPoint desired_start = previous->PointAtEnd();
	    const ON_3dPoint desired_end = next->PointAtStart();
	    const bool exact_adjacent_coordinates =
		fabs(desired_start[fixed_direction] - fixed_parameter) <= ON_ZERO_TOLERANCE &&
		fabs(desired_end[fixed_direction] - fixed_parameter) <= ON_ZERO_TOLERANCE;
	    double original_score = 0.0;
	    std::string failure;
	    ON_Curve *candidate = NULL;
	    for (int resolution = 256; !candidate &&
		    resolution <= kDenseValidationSegments;
		    resolution *= 2) {
		candidate = make_candidate(trim, edge, surface, fixed_direction,
		    fixed_parameter, LocalUnits::tolerance,
		    exact_adjacent_coordinates ? &desired_start : NULL,
		    exact_adjacent_coordinates ? &desired_end : NULL,
		    resolution, &original_score, &failure);
	    }
	    if (!candidate) {
		candidate_failures += " direction " + std::to_string(fixed_direction) +
		    "=" + failure;
		continue;
	    }

	    const int varying_direction = 1 - fixed_direction;
	    const double varying_period = surface->Domain(varying_direction).Length();
	    const bool varying_closed = surface->IsClosed(varying_direction) &&
		varying_period > ON_ZERO_TOLERANCE;
	    int best_period_shift = 0;
	    double candidate_score = DBL_MAX;
	    for (int period_shift = varying_closed ? -2 : 0;
		 period_shift <= (varying_closed ? 2 : 0); ++period_shift) {
		ON_3dPoint start = candidate->PointAtStart();
		ON_3dPoint end = candidate->PointAtEnd();
		start[varying_direction] += period_shift * varying_period;
		end[varying_direction] += period_shift * varying_period;
		const double closure = start.DistanceTo(previous->PointAtEnd()) +
		    end.DistanceTo(next->PointAtStart());
		if (closure < candidate_score) {
		    candidate_score = closure;
		    best_period_shift = period_shift;
		}
	    }
	    if (best_period_shift != 0) {
		ON_Xform shift(ON_Xform::IdentityTransformation);
		shift.m_xform[varying_direction][3] =
		    best_period_shift * varying_period;
		if (!candidate->Transform(shift) || !candidate->IsValid()) {
		    delete candidate;
		    continue;
		}
	    }
	    if (candidate_score >= best_score) {
		delete candidate;
		continue;
	    }
	    delete best;
	    best = candidate;
	    best_score = candidate_score;
	}
	if (!best) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": exact adjacent isoparametric regeneration rejected for trim "
		    << ti << "/STEP edge " << edge.m_edge_user.i << " in loop "
		    << trim.m_li << ": " << candidate_failures << std::endl;
	    continue;
	}
	const int c2_index = brep->AddTrimCurve(best);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
	    if (c2_index < 0)
		delete best;
	    continue;
	}
	brep->SetTrimIsoFlags(trim);
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "regenerated an isoparametric pcurve from the exact edge");
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": regenerated "
		"isoparametric trim " << ti << " from exact STEP edge "
		<< edge.m_edge_user.i << std::endl;
    }

    for (std::vector<int>::const_iterator repaired_loop = repaired_seam_loops.begin();
	 repaired_loop != repaired_seam_loops.end(); ++repaired_loop) {
	if (*repaired_loop < 0 || *repaired_loop >= brep->m_L.Count())
	    continue;
	ON_BrepLoop &loop = brep->m_L[*repaired_loop];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    if (!trim || trim->m_type != ON_BrepTrim::seam)
		continue;
	    int fixed_direction = -1;
	    if (trim->m_iso == ON_Surface::W_iso || trim->m_iso == ON_Surface::E_iso)
		fixed_direction = 0;
	    else if (trim->m_iso == ON_Surface::S_iso || trim->m_iso == ON_Surface::N_iso)
		fixed_direction = 1;
	    if (fixed_direction < 0)
		continue;
	    const int varying_direction = 1 - fixed_direction;
	    const ON_Interval varying_domain = surface->Domain(varying_direction);
	    const double period = varying_domain.Length();
	    if (!surface->IsClosed(varying_direction) || !(period > ON_ZERO_TOLERANCE))
		continue;
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!previous || !next)
		continue;
	    int best_shift = 0;
	    double best_closure = trim->PointAtStart().DistanceTo(previous->PointAtEnd()) +
		trim->PointAtEnd().DistanceTo(next->PointAtStart());
	    for (int shift = -2; shift <= 2; ++shift) {
		ON_3dPoint start = trim->PointAtStart();
		ON_3dPoint end = trim->PointAtEnd();
		start[varying_direction] += shift * period;
		end[varying_direction] += shift * period;
		const double closure = start.DistanceTo(previous->PointAtEnd()) +
		    end.DistanceTo(next->PointAtStart());
		if (closure < best_closure) {
		    best_closure = closure;
		    best_shift = shift;
		}
	    }
	    if (best_shift == 0)
		continue;
	    ON_Curve *shifted = trim->DuplicateCurve();
	    ON_Xform transform(ON_Xform::IdentityTransformation);
	    transform.m_xform[varying_direction][3] = best_shift * period;
	    if (!shifted || !shifted->Transform(transform) ||
		    !shifted->ChangeDimension(2) || !shifted->IsValid()) {
		delete shifted;
		continue;
	    }
	    bool exact = true;
	    const ON_Interval trim_domain = trim->Domain();
	    for (int sample = 0; sample <= 64; ++sample) {
		const double parameter = trim_domain.ParameterAt(
		    static_cast<double>(sample) / 64.0);
		const ON_3dPoint original_uv = trim->PointAt(parameter);
		const ON_3dPoint shifted_uv = shifted->PointAt(parameter);
		const ON_3dPoint original_lift = surface->PointAt(
		    original_uv.x, original_uv.y);
		const ON_3dPoint shifted_lift = surface->PointAt(
		    shifted_uv.x, shifted_uv.y);
		if (!original_lift.IsValid() || !shifted_lift.IsValid() ||
			original_lift.DistanceTo(shifted_lift) > ON_ZERO_TOLERANCE) {
		    exact = false;
		    break;
		}
	    }
	    if (!exact) {
		delete shifted;
		continue;
	    }
	    const int c2_index = brep->AddTrimCurve(shifted);
	    if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		if (c2_index < 0)
		    delete shifted;
		continue;
	    }
	    brep->SetTrimIsoFlags(*trim);
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"unwrapped an exact seam pcurve into its loop's periodic branch");
	}
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!trim || trim->m_type != ON_BrepTrim::seam || !previous || !next)
		continue;
	    const ON_3dPoint start = previous->PointAtEnd();
	    const ON_3dPoint end = next->PointAtStart();
	    if (trim->PointAtStart().DistanceTo(start) <= ON_ZERO_TOLERANCE &&
		    trim->PointAtEnd().DistanceTo(end) <= ON_ZERO_TOLERANCE)
		continue;
	    ON_Curve *candidate = trim->DuplicateCurve();
	    const ON_Interval domain = trim->Domain();
	    bool candidate_valid = candidate && candidate->SetStartPoint(start) &&
		candidate->SetEndPoint(end) && candidate->ChangeDimension(2) &&
		candidate->IsValid() &&
		surface->IsIsoparametric(*candidate, &domain) == trim->m_iso;
	    if (!candidate_valid) {
		delete candidate;
		candidate = new ON_LineCurve(start, end);
		candidate_valid = candidate->ChangeDimension(2) &&
		    candidate->SetDomain(domain.Min(), domain.Max()) &&
		    candidate->IsValid() &&
		    surface->IsIsoparametric(*candidate, &domain) == trim->m_iso;
	    }
	    if (!candidate_valid) {
		delete candidate;
		continue;
	    }
	    const ON_BrepEdge *edge = trim->Edge();
	    ON_NurbsCurve edge_nurbs;
	    if (!edge || !edge->GetNurbForm(edge_nurbs)) {
		delete candidate;
		continue;
	    }
	    bool valid = true;
	    for (int sample = 0; sample <= 512; ++sample) {
		const double parameter = domain.ParameterAt(
		    static_cast<double>(sample) / 512.0);
		const ON_3dPoint original_uv = trim->PointAt(parameter);
		const ON_3dPoint candidate_uv = candidate->PointAt(parameter);
		const ON_3dPoint original_lift = surface->PointAt(
		    original_uv.x, original_uv.y);
		const ON_3dPoint candidate_lift = surface->PointAt(
		    candidate_uv.x, candidate_uv.y);
		double edge_parameter = 0.0;
		if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
			original_lift.DistanceTo(candidate_lift) > LocalUnits::tolerance ||
			!ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs,
			    candidate_lift) || candidate_lift.DistanceTo(
				edge_nurbs.PointAt(edge_parameter)) > LocalUnits::tolerance) {
		    valid = false;
		    break;
		}
	    }
	    if (!valid) {
		delete candidate;
		continue;
	    }
	    const int c2_index = brep->AddTrimCurve(candidate);
	    if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		if (c2_index < 0)
		    delete candidate;
		continue;
	    }
	    brep->SetTrimIsoFlags(*trim);
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"matched a regenerated seam to exact adjacent pcurve endpoints");
	}
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!trim || trim->m_type == ON_BrepTrim::seam || !previous || !next ||
		    previous->m_vi[1] != trim->m_vi[0] ||
		    trim->m_vi[1] != next->m_vi[0])
		continue;
	    const ON_3dPoint start = previous->PointAtEnd();
	    const ON_3dPoint end = next->PointAtStart();
	    if (trim->PointAtStart().DistanceTo(start) <= ON_ZERO_TOLERANCE &&
		    trim->PointAtEnd().DistanceTo(end) <= ON_ZERO_TOLERANCE)
		continue;
	    const ON_Interval domain = trim->Domain();
	    ON_Curve *candidate = trim->DuplicateCurve();
	    bool candidate_valid = candidate && candidate->SetStartPoint(start) &&
		candidate->SetEndPoint(end) && candidate->ChangeDimension(2) &&
		candidate->IsValid();
	    if (!candidate_valid) {
		delete candidate;
		candidate = new ON_LineCurve(start, end);
		candidate_valid = candidate->ChangeDimension(2) &&
		    candidate->SetDomain(domain.Min(), domain.Max()) && candidate->IsValid();
	    }
	    if (!candidate_valid) {
		delete candidate;
		continue;
	    }
	    const ON_BrepEdge *edge = trim->Edge();
	    ON_NurbsCurve edge_nurbs;
	    if (!edge || !edge->GetNurbForm(edge_nurbs)) {
		delete candidate;
		continue;
	    }
	    bool valid = true;
	    for (int sample = 0; sample <= 256; ++sample) {
		const double fraction = static_cast<double>(sample) / 256.0;
		const double parameter = domain.ParameterAt(fraction);
		const ON_3dPoint old_uv = trim->PointAt(parameter);
		const ON_3dPoint new_uv = candidate->PointAt(parameter);
		const ON_3dPoint old_lift = surface->PointAt(old_uv.x, old_uv.y);
		const ON_3dPoint new_lift = surface->PointAt(new_uv.x, new_uv.y);
		double edge_parameter = 0.0;
		const double lift_delta = old_lift.IsValid() && new_lift.IsValid() ?
		    old_lift.DistanceTo(new_lift) : DBL_MAX;
		const bool closest = ON_NurbsCurve_GetClosestPoint(&edge_parameter,
		    &edge_nurbs, new_lift);
		const ON_3dPoint corresponding_edge_point = edge->PointAt(
		    edge->Domain().ParameterAt(trim->m_bRev3d ? 1.0 - fraction :
			fraction));
		double edge_distance = new_lift.IsValid() && corresponding_edge_point.IsValid() ?
		    new_lift.DistanceTo(corresponding_edge_point) : DBL_MAX;
		if (closest)
		    edge_distance = std::min(edge_distance, new_lift.DistanceTo(
			edge_nurbs.PointAt(edge_parameter)));
		if (!old_lift.IsValid() || !new_lift.IsValid() ||
			lift_delta > LocalUnits::tolerance ||
			edge_distance > LocalUnits::tolerance) {
		    valid = false;
		    break;
		}
	    }
	    if (!valid) {
		delete candidate;
		continue;
	    }
	    const int c2_index = brep->AddTrimCurve(candidate);
	    if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		if (c2_index < 0)
		    delete candidate;
		continue;
	    }
	    brep->SetTrimIsoFlags(*trim);
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"matched a non-seam trim to exact adjacent endpoints");
	}
	/* A loop with ordinary trims can still pass directly from one side of a
	 * periodic seam to the complementary side at a collapsed surface pole.
	 * The two seam endpoints lift to the same topological vertex, but cannot
	 * coincide in p-space.  Insert the exact singular boundary trim that STEP
	 * leaves implicit instead of moving either seam off its boundary. */
	if (loop.TrimCount() > 2) {
	    const int original_count = loop.TrimCount();
	    for (int lti = 0; lti < original_count; ++lti) {
		const ON_BrepTrim *first = loop.Trim(lti);
		const ON_BrepTrim *second = loop.Trim((lti + 1) % original_count);
		if (!first || !second || first->m_type != ON_BrepTrim::seam ||
			second->m_type != ON_BrepTrim::seam ||
			first->m_vi[1] < 0 || first->m_vi[1] != second->m_vi[0] ||
			first->m_vi[1] >= brep->m_V.Count() ||
			first->PointAtEnd().DistanceTo(second->PointAtStart()) <=
			    ON_ZERO_TOLERANCE)
		    continue;
		int seam_direction = -1;
		if ((first->m_iso == ON_Surface::W_iso &&
		     second->m_iso == ON_Surface::E_iso) ||
		    (first->m_iso == ON_Surface::E_iso &&
		     second->m_iso == ON_Surface::W_iso))
		    seam_direction = 0;
		else if ((first->m_iso == ON_Surface::S_iso &&
			  second->m_iso == ON_Surface::N_iso) ||
			 (first->m_iso == ON_Surface::N_iso &&
			  second->m_iso == ON_Surface::S_iso))
		    seam_direction = 1;
		if (seam_direction < 0 || !surface->IsClosed(seam_direction))
		    continue;
		const int singular_direction = 1 - seam_direction;
		const ON_Interval singular_domain = surface->Domain(singular_direction);
		const ON_3dPoint first_end = first->PointAtEnd();
		const ON_3dPoint second_start = second->PointAtStart();
		const double parameter = 0.5 * (first_end[singular_direction] +
		    second_start[singular_direction]);
		const int side = fabs(parameter - singular_domain.Min()) <=
		    fabs(parameter - singular_domain.Max()) ? 0 : 1;
		const ON_Surface::ISO singular_iso = singular_direction == 0 ?
		    (side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
		    (side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		ON_3dPoint start = first_end;
		ON_3dPoint end = second_start;
		start[singular_direction] = singular_domain[side];
		end[singular_direction] = singular_domain[side];
		std::unique_ptr<ON_LineCurve> singular_curve(new ON_LineCurve(start, end));
		if (!singular_curve->ChangeDimension(2) || !singular_curve->IsValid() ||
			surface->IsIsoparametric(*singular_curve) != singular_iso)
		    continue;
		const ON_3dPoint vertex = brep->m_V[first->m_vi[1]].point;
		bool exact = true;
		for (int sample = 0; sample <= 64; ++sample) {
		    const ON_3dPoint uv = singular_curve->PointAt(
			singular_curve->Domain().ParameterAt(
			    static_cast<double>(sample) / 64.0));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    if (!lift.IsValid() || lift.DistanceTo(vertex) >
			    LocalUnits::tolerance) {
			exact = false;
			break;
		    }
		}
		if (!exact)
		    continue;
		std::vector<int> original_trims;
		original_trims.reserve(original_count);
		for (int offset = 0; offset < original_count; ++offset)
		    original_trims.push_back(loop.m_ti[offset]);
		const int c2_index = brep->AddTrimCurve(singular_curve.release());
		if (c2_index < 0)
		    continue;
		const int singular_index = brep->NewSingularTrim(
		    brep->m_V[first->m_vi[1]], loop, singular_iso,
		    c2_index).m_trim_index;
		loop.m_ti.SetCount(0);
		for (int offset = 0; offset < original_count; ++offset) {
		    loop.m_ti.Append(original_trims[offset]);
		    if (offset == lti)
			loop.m_ti.Append(singular_index);
		}
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    "inserted an exact singular trim at a periodic surface pole");
		break;
	    }
	}
	if (loop.TrimCount() == 2) {
	    ON_BrepTrim *first = loop.Trim(0);
	    ON_BrepTrim *second = loop.Trim(1);
	    int seam_direction = -1;
	    if (first && second && first->m_type == ON_BrepTrim::seam &&
		    second->m_type == ON_BrepTrim::seam &&
		    ((first->m_iso == ON_Surface::W_iso &&
		      second->m_iso == ON_Surface::E_iso) ||
		     (first->m_iso == ON_Surface::E_iso &&
		      second->m_iso == ON_Surface::W_iso)))
		seam_direction = 0;
	    else if (first && second && first->m_type == ON_BrepTrim::seam &&
		    second->m_type == ON_BrepTrim::seam &&
		    ((first->m_iso == ON_Surface::S_iso &&
		      second->m_iso == ON_Surface::N_iso) ||
		     (first->m_iso == ON_Surface::N_iso &&
		      second->m_iso == ON_Surface::S_iso)))
		seam_direction = 1;
	    const int singular_direction = 1 - seam_direction;
	    if (seam_direction >= 0 && surface->IsClosed(seam_direction) &&
		    first->m_vi[1] == second->m_vi[0] &&
		    second->m_vi[1] == first->m_vi[0]) {
		const ON_Interval singular_domain = surface->Domain(singular_direction);
		const ON_3dPoint junction_start[2] = {
		    first->PointAtEnd(), second->PointAtEnd()};
		const ON_3dPoint junction_end[2] = {
		    second->PointAtStart(), first->PointAtStart()};
		const int vertex_index[2] = {first->m_vi[1], second->m_vi[1]};
		ON_Surface::ISO singular_iso[2] = {
		    ON_Surface::not_iso, ON_Surface::not_iso};
		ON_LineCurve *singular_curve[2] = {NULL, NULL};
		bool valid = true;
		for (int junction = 0; junction < 2 && valid; ++junction) {
		    const double parameter = 0.5 *
			(junction_start[junction][singular_direction] +
			 junction_end[junction][singular_direction]);
		    const int side = fabs(parameter - singular_domain.Min()) <=
			fabs(parameter - singular_domain.Max()) ? 0 : 1;
		    const int surface_side = singular_direction == 0 ?
			(side == 0 ? 3 : 1) : (side == 0 ? 0 : 2);
		    singular_iso[junction] = singular_direction == 0 ?
			(side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
			(side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		    if (!surface->IsSingular(surface_side) || vertex_index[junction] < 0 ||
			    vertex_index[junction] >= brep->m_V.Count()) {
			valid = false;
			break;
		    }
		    ON_3dPoint start = junction_start[junction];
		    ON_3dPoint end = junction_end[junction];
		    start[singular_direction] = singular_domain[side];
		    end[singular_direction] = singular_domain[side];
		    singular_curve[junction] = new ON_LineCurve(start, end);
		    if (!singular_curve[junction]->ChangeDimension(2) ||
			    !singular_curve[junction]->IsValid() ||
			    surface->IsIsoparametric(*singular_curve[junction]) !=
				singular_iso[junction]) {
			valid = false;
			break;
		    }
		    const ON_3dPoint vertex = brep->m_V[vertex_index[junction]].point;
		    for (int sample = 0; sample <= 64; ++sample) {
			const ON_3dPoint uv = singular_curve[junction]->PointAt(
			    singular_curve[junction]->Domain().ParameterAt(
				static_cast<double>(sample) / 64.0));
			const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			if (!lift.IsValid() || lift.DistanceTo(vertex) >
				    LocalUnits::tolerance) {
			    valid = false;
			    break;
			}
		    }
		}
		if (valid) {
		    const int first_index = first->m_trim_index;
		    const int second_index = second->m_trim_index;
		    const int first_c2 = brep->AddTrimCurve(singular_curve[0]);
		    const int second_c2 = brep->AddTrimCurve(singular_curve[1]);
		    if (first_c2 >= 0 && second_c2 >= 0) {
			const int first_singular = brep->NewSingularTrim(
			    brep->m_V[vertex_index[0]], loop, singular_iso[0],
			    first_c2).m_trim_index;
			const int second_singular = brep->NewSingularTrim(
			    brep->m_V[vertex_index[1]], loop, singular_iso[1],
			    second_c2).m_trim_index;
			loop.m_ti.SetCount(0);
			loop.m_ti.Append(first_index);
			loop.m_ti.Append(first_singular);
			loop.m_ti.Append(second_index);
			loop.m_ti.Append(second_singular);
			const auto adjusted_seam = [surface](const ON_BrepTrim &trim,
			    const ON_3dPoint &start, const ON_3dPoint &end) -> ON_Curve * {
			    ON_Curve *candidate = trim.DuplicateCurve();
			    const ON_Interval domain = trim.Domain();
			    if (!candidate || !candidate->SetStartPoint(start) ||
				    !candidate->SetEndPoint(end) ||
				    !candidate->ChangeDimension(2) || !candidate->IsValid() ||
				    surface->IsIsoparametric(*candidate, &domain) != trim.m_iso) {
				delete candidate;
				candidate = new ON_LineCurve(start, end);
				if (!candidate->ChangeDimension(2) ||
					!candidate->SetDomain(domain.Min(), domain.Max()) ||
					!candidate->IsValid() ||
					surface->IsIsoparametric(*candidate, &domain) != trim.m_iso) {
				    delete candidate;
				    return NULL;
				}
			    }
			    const ON_BrepEdge *edge = trim.Edge();
			    if (!edge) {
				delete candidate;
				return NULL;
			    }
			    const ON_Interval edge_domain = edge->Domain();
			    for (int sample = 0; sample <= 256; ++sample) {
				const double fraction = static_cast<double>(sample) / 256.0;
				const double parameter = domain.ParameterAt(fraction);
				const ON_3dPoint candidate_uv = candidate->PointAt(parameter);
				const ON_3dPoint candidate_lift = surface->PointAt(
				    candidate_uv.x, candidate_uv.y);
				const double edge_fraction = trim.m_bRev3d ?
				    1.0 - fraction : fraction;
				const ON_3dPoint edge_point = edge->PointAt(
				    edge_domain.ParameterAt(edge_fraction));
				if (!candidate_lift.IsValid() || !edge_point.IsValid() ||
					candidate_lift.DistanceTo(edge_point) >
					    LocalUnits::tolerance) {
				    delete candidate;
				    return NULL;
				}
			    }
			    return candidate;
			};
			ON_BrepTrim &installed_first = brep->m_T[first_index];
			ON_BrepTrim &installed_second = brep->m_T[second_index];
			const ON_BrepTrim &north_singular = brep->m_T[first_singular];
			const ON_BrepTrim &south_singular = brep->m_T[second_singular];
			ON_Curve *first_adjusted = adjusted_seam(installed_first,
			    south_singular.PointAtEnd(), north_singular.PointAtStart());
			ON_Curve *second_adjusted = adjusted_seam(installed_second,
			    north_singular.PointAtEnd(), south_singular.PointAtStart());
			if (first_adjusted && second_adjusted) {
			    const int first_adjusted_c2 = brep->AddTrimCurve(first_adjusted);
			    const int second_adjusted_c2 = brep->AddTrimCurve(second_adjusted);
			    if (first_adjusted_c2 >= 0 && second_adjusted_c2 >= 0) {
				brep->SetTrimCurve(installed_first, first_adjusted_c2);
				brep->SetTrimCurve(installed_second, second_adjusted_c2);
				brep->SetTrimIsoFlags(installed_first);
				brep->SetTrimIsoFlags(installed_second);
			    }
			} else {
			    delete first_adjusted;
			    delete second_adjusted;
			}
			wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			    "inserted exact singular trims at periodic surface poles");
		    }
		} else {
		    delete singular_curve[0];
		    delete singular_curve[1];
		}
	    }
	}
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *previous = loop.Trim(lti);
	    ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!previous || !next || previous->m_vi[1] < 0 ||
		    previous->m_vi[1] != next->m_vi[0] ||
		    previous->m_vi[1] >= brep->m_V.Count() ||
		    previous->PointAtEnd().DistanceTo(next->PointAtStart()) <=
			ON_ZERO_TOLERANCE)
		continue;
	    const ON_3dPoint vertex = brep->m_V[previous->m_vi[1]].point;
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint next_uv = next->PointAtStart();
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
	    if (!previous_lift.IsValid() || !next_lift.IsValid() ||
		    previous_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
		    next_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
		    previous_lift.DistanceTo(next_lift) > LocalUnits::tolerance)
		continue;
	    ON_Curve *previous_original = previous->DuplicateCurve();
	    ON_Curve *next_original = next->DuplicateCurve();
	    if (!previous_original || !next_original) {
		delete previous_original;
		delete next_original;
		continue;
	    }
	    const ON_Surface::ISO previous_iso = previous->m_iso;
	    const ON_Surface::ISO next_iso = next->m_iso;
	    if (!brep->MatchTrimEnds(*previous, *next)) {
		delete previous_original;
		delete next_original;
		continue;
	    }
	    const auto matched_valid = [surface](const ON_BrepTrim &trim,
		const ON_Curve &original, ON_Surface::ISO expected_iso) {
		const ON_Interval domain = trim.Domain();
		if (trim.m_type == ON_BrepTrim::seam &&
			surface->IsIsoparametric(trim, &domain) != expected_iso)
		    return false;
		const ON_BrepEdge *edge = trim.Edge();
		ON_NurbsCurve edge_nurbs;
		if (edge && !edge->GetNurbForm(edge_nurbs))
		    return false;
		for (int sample = 0; sample <= 256; ++sample) {
		    const double parameter = domain.ParameterAt(
			static_cast<double>(sample) / 256.0);
		    const ON_3dPoint old_uv = original.PointAt(parameter);
		    const ON_3dPoint new_uv = trim.PointAt(parameter);
		    const ON_3dPoint old_lift = surface->PointAt(old_uv.x, old_uv.y);
		    const ON_3dPoint new_lift = surface->PointAt(new_uv.x, new_uv.y);
		    if (!old_lift.IsValid() || !new_lift.IsValid() ||
			    old_lift.DistanceTo(new_lift) > LocalUnits::tolerance)
			return false;
		    if (edge) {
			double edge_parameter = 0.0;
			if (!ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs,
				new_lift) || new_lift.DistanceTo(
				    edge_nurbs.PointAt(edge_parameter)) > LocalUnits::tolerance)
			    return false;
		    }
		}
		return true;
	    };
	    const bool valid = previous->PointAtEnd().DistanceTo(next->PointAtStart()) <=
		ON_ZERO_TOLERANCE && matched_valid(*previous, *previous_original,
		    previous_iso) && matched_valid(*next, *next_original, next_iso);
	    if (!valid) {
		const int previous_c2 = brep->AddTrimCurve(previous_original);
		const int next_c2 = brep->AddTrimCurve(next_original);
		if (previous_c2 >= 0)
		    brep->SetTrimCurve(*previous, previous_c2);
		else
		    delete previous_original;
		if (next_c2 >= 0)
		    brep->SetTrimCurve(*next, next_c2);
		else
		    delete next_original;
		previous->m_iso = previous_iso;
		next->m_iso = next_iso;
		continue;
	    }
	    delete previous_original;
	    delete next_original;
	    brep->SetTrimIsoFlags(*previous);
	    brep->SetTrimIsoFlags(*next);
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"matched exact loop endpoints within model tolerance");
	}
    }
}


static void
repair_paired_seam_boundaries(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::vector<int> *aligned_surface_loops)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const auto complement = [](ON_Surface::ISO iso) {
	switch (iso) {
	    case ON_Surface::W_iso: return ON_Surface::E_iso;
	    case ON_Surface::E_iso: return ON_Surface::W_iso;
	    case ON_Surface::S_iso: return ON_Surface::N_iso;
	    case ON_Surface::N_iso: return ON_Surface::S_iso;
	    default: return ON_Surface::not_iso;
	}
    };
    const auto project_to = [brep](ON_BrepTrim &trim, const ON_Surface *surface,
	const ON_BrepEdge &edge, ON_Surface::ISO target, double tolerance,
	bool aligned_surface_loop) {
	int direction = -1;
	double boundary = ON_UNSET_VALUE;
	if (target == ON_Surface::W_iso || target == ON_Surface::E_iso) {
	    direction = 0;
	    boundary = surface->Domain(0)[target == ON_Surface::E_iso ? 1 : 0];
	} else if (target == ON_Surface::S_iso || target == ON_Surface::N_iso) {
	    direction = 1;
	    boundary = surface->Domain(1)[target == ON_Surface::N_iso ? 1 : 0];
	}
	if (direction < 0 || !surface->IsClosed(direction))
	    return false;
	ON_Curve *projected = trim.DuplicateCurve();
	if (!projected)
	    return false;
	ON_Xform projection(ON_Xform::IdentityTransformation);
	for (int column = 0; column < 4; ++column)
	    projection.m_xform[direction][column] = 0.0;
	projection.m_xform[direction][3] = boundary;
	if (!projected->Transform(projection) || !projected->ChangeDimension(2) ||
		!projected->IsValid()) {
	    delete projected;
	    return false;
	}
	const ON_Interval domain = trim.Domain();
	ON_Surface::ISO derived = surface->IsIsoparametric(*projected, &domain);
	if (derived != target) {
	    ON_LineCurve *line = new ON_LineCurve(projected->PointAtStart(),
		projected->PointAtEnd());
	    if (line->ChangeDimension(2) && line->SetDomain(domain.Min(), domain.Max()) &&
		    line->IsValid()) {
		delete projected;
		projected = line;
		derived = surface->IsIsoparametric(*projected, &domain);
	    } else {
		delete line;
	    }
	}
	if (derived != target) {
	    delete projected;
	    return false;
	}
	ON_NurbsCurve edge_nurbs;
	if (!edge.GetNurbForm(edge_nurbs)) {
	    delete projected;
	    return false;
	}
	const int samples = std::min(kDenseValidationSegments,
	    std::max(64, trim.SpanCount() * 8));
	for (int sample = 0; sample <= samples; ++sample) {
	    const double fraction = static_cast<double>(sample) / samples;
	    const double parameter = domain.ParameterAt(fraction);
	    const ON_3dPoint original_uv = trim.PointAt(parameter);
	    const ON_3dPoint projected_uv = projected->PointAt(parameter);
	    const ON_3dPoint original_lift = surface->PointAt(original_uv.x, original_uv.y);
	    const ON_3dPoint projected_lift = surface->PointAt(projected_uv.x, projected_uv.y);
	    double edge_parameter = 0.0;
	    const bool have_closest = ON_NurbsCurve_GetClosestPoint(&edge_parameter,
		&edge_nurbs, projected_lift);
	    const double closest_distance = have_closest ? projected_lift.DistanceTo(
		edge_nurbs.PointAt(edge_parameter)) : DBL_MAX;
	    double edge_distance = closest_distance;
	    const ON_Interval edge_domain = edge_nurbs.Domain();
	    const ON_3dPoint forward = edge_nurbs.PointAt(
		edge_domain.ParameterAt(fraction));
	    const ON_3dPoint reverse = edge_nurbs.PointAt(
		edge_domain.ParameterAt(1.0 - fraction));
	    if (forward.IsValid())
		edge_distance = std::min(edge_distance,
		    projected_lift.DistanceTo(forward));
	    if (reverse.IsValid())
		edge_distance = std::min(edge_distance,
		    projected_lift.DistanceTo(reverse));
	    const double source_distance = original_lift.DistanceTo(projected_lift);
	    const bool doubly_closed = surface->IsClosed(0) && surface->IsClosed(1);
	    if (!original_lift.IsValid() || !projected_lift.IsValid() ||
		    (doubly_closed && !aligned_surface_loop ?
			(source_distance > tolerance || closest_distance > tolerance) :
			(source_distance > tolerance && edge_distance > tolerance))) {
		delete projected;
		return false;
	    }
	}
	const int c2_index = brep->AddTrimCurve(projected);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
	    if (c2_index < 0)
		delete projected;
	    return false;
	}
	brep->SetTrimIsoFlags(trim);
	return trim.m_iso == target;
    };

    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const int first_index = edge.m_ti[0];
	const int second_index = edge.m_ti[1];
	if (first_index < 0 || first_index >= brep->m_T.Count() ||
		second_index < 0 || second_index >= brep->m_T.Count())
	    continue;
	ON_BrepTrim &first = brep->m_T[first_index];
	ON_BrepTrim &second = brep->m_T[second_index];
	if (first.m_type != ON_BrepTrim::seam || second.m_type != ON_BrepTrim::seam ||
		first.m_li < 0 || first.m_li != second.m_li ||
		first.m_li >= brep->m_L.Count() || complement(first.m_iso) == second.m_iso)
	    continue;
	const ON_BrepLoop &loop = brep->m_L[first.m_li];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	const bool aligned_surface_loop = aligned_surface_loops &&
	    std::find(aligned_surface_loops->begin(), aligned_surface_loops->end(),
		first.m_li) != aligned_surface_loops->end();
	bool repaired = false;
	const ON_Surface::ISO second_target = complement(first.m_iso);
	if (second_target != ON_Surface::not_iso)
	    repaired = project_to(second, surface, edge, second_target,
		LocalUnits::tolerance, aligned_surface_loop);
	if (!repaired) {
	    const ON_Surface::ISO first_target = complement(second.m_iso);
	    if (first_target != ON_Surface::not_iso)
		repaired = project_to(first, surface, edge, first_target,
		    LocalUnits::tolerance, aligned_surface_loop);
	}
	if (!repaired)
	    continue;
	wrapper->RecordRepair(entity_id, entity_type, "trim_iso",
	    "placed paired seam pcurves on opposite periodic boundaries");
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": paired seam trims "
		<< first_index << '/' << second_index << " on opposite boundaries"
		<< std::endl;
    }
}


static void
repair_aligned_surface_loop_branches(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::vector<int> &aligned_surface_loops)
{
    if (!brep || !wrapper || aligned_surface_loops.empty() ||
	    !(LocalUnits::tolerance > 0.0))
	return;

    const double lift_tolerance = std::max(1.0e-10,
	std::min(1.0e-7, LocalUnits::tolerance * 1.0e-6));
    for (std::vector<int>::const_iterator loop_index =
	    aligned_surface_loops.begin();
	 loop_index != aligned_surface_loops.end(); ++loop_index) {
	if (*loop_index < 0 || *loop_index >= brep->m_L.Count())
	    continue;
	ON_BrepLoop &loop = brep->m_L[*loop_index];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	const double period[2] = {
	    surface->IsClosed(0) ? surface->Domain(0).Length() : 0.0,
	    surface->IsClosed(1) ? surface->Domain(1).Length() : 0.0
	};
	if (!(period[0] > ON_ZERO_TOLERANCE) &&
		!(period[1] > ON_ZERO_TOLERANCE))
	    continue;

	for (int pass = 0; pass < loop.TrimCount(); ++pass) {
	    bool changed = false;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		ON_BrepTrim *trim = loop.Trim(lti);
		const ON_BrepTrim *previous = loop.Trim(
		    (lti + loop.TrimCount() - 1) % loop.TrimCount());
		const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!trim || !previous || !next ||
			trim->m_type == ON_BrepTrim::seam)
		    continue;
		const ON_3dPoint original_start = trim->PointAtStart();
		const ON_3dPoint original_end = trim->PointAtEnd();
		double best_score = original_start.DistanceTo(previous->PointAtEnd()) +
		    original_end.DistanceTo(next->PointAtStart());
		int best_shift[2] = {0, 0};
		for (int shift0 = period[0] > ON_ZERO_TOLERANCE ? -1 : 0;
		     shift0 <= (period[0] > ON_ZERO_TOLERANCE ? 1 : 0); ++shift0) {
		    for (int shift1 = period[1] > ON_ZERO_TOLERANCE ? -1 : 0;
			 shift1 <= (period[1] > ON_ZERO_TOLERANCE ? 1 : 0); ++shift1) {
			if (shift0 == 0 && shift1 == 0)
			    continue;
			ON_3dPoint start = original_start;
			ON_3dPoint end = original_end;
			start.x += shift0 * period[0];
			end.x += shift0 * period[0];
			start.y += shift1 * period[1];
			end.y += shift1 * period[1];
			const double score = start.DistanceTo(previous->PointAtEnd()) +
			    end.DistanceTo(next->PointAtStart());
			if (score < best_score) {
			    best_score = score;
			    best_shift[0] = shift0;
			    best_shift[1] = shift1;
			}
		    }
		}
		if (best_shift[0] == 0 && best_shift[1] == 0)
		    continue;

		ON_Curve *candidate = trim->DuplicateCurve();
		ON_Xform translation(ON_Xform::IdentityTransformation);
		translation.m_xform[0][3] = best_shift[0] * period[0];
		translation.m_xform[1][3] = best_shift[1] * period[1];
		if (!candidate || !candidate->Transform(translation) ||
			!candidate->ChangeDimension(2) || !candidate->IsValid()) {
		    delete candidate;
		    continue;
		}

		const ON_Interval trim_domain = trim->Domain();
		bool exact = true;
		for (int sample = 0; sample <= 64; ++sample) {
		    const double parameter = trim_domain.ParameterAt(
			static_cast<double>(sample) / 64.0);
		    const ON_3dPoint original_uv = trim->PointAt(parameter);
		    const ON_3dPoint candidate_uv = candidate->PointAt(parameter);
		    const ON_3dPoint original_lift = surface->PointAt(
			original_uv.x, original_uv.y);
		    const ON_3dPoint candidate_lift = surface->PointAt(
			candidate_uv.x, candidate_uv.y);
		    if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
			    original_lift.DistanceTo(candidate_lift) > lift_tolerance) {
			exact = false;
			break;
		    }
		}
		const ON_3dPoint candidate_start = candidate->PointAtStart();
		const ON_3dPoint candidate_end = candidate->PointAtEnd();
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		const ON_3dPoint candidate_start_lift = surface->PointAt(
		    candidate_start.x, candidate_start.y);
		const ON_3dPoint candidate_end_lift = surface->PointAt(
		    candidate_end.x, candidate_end.y);
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		exact = exact && candidate_start_lift.IsValid() &&
		    candidate_end_lift.IsValid() && previous_lift.IsValid() &&
		    next_lift.IsValid() && candidate_start_lift.DistanceTo(previous_lift) <=
		    LocalUnits::tolerance && candidate_end_lift.DistanceTo(next_lift) <=
		    LocalUnits::tolerance;
		if (!exact) {
		    delete candidate;
		    continue;
		}

		const int c2_index = brep->AddTrimCurve(candidate);
		if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		    if (c2_index < 0)
			delete candidate;
		    continue;
		}
		brep->SetTrimIsoFlags(*trim);
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "unwrapped an exact non-seam pcurve after periodic surface alignment");
		changed = true;
	    }
	    if (!changed)
		break;
	}

	/* A greedy single-trim shift cannot move a contiguous branch without
	 * temporarily making its other endpoint worse.  Anchor one seam and solve
	 * the remaining whole-period choices for the complete cyclic loop.  Other
	 * seams may move only in their varying direction, preserving their fixed
	 * boundary coordinate and iso classification.  The shifts are installed
	 * only after their surface lifts and resulting joins have been verified. */
	int anchor = -1;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (trim && trim->m_type == ON_BrepTrim::seam) {
		anchor = lti;
		break;
	    }
	}
	if (anchor < 0 || loop.TrimCount() < 2)
	    continue;

	std::vector<ON_BrepTrim *> ordered_trims;
	ordered_trims.reserve(loop.TrimCount());
	bool joined_in_model_space = true;
	for (int position = 0; position < loop.TrimCount(); ++position) {
	    ON_BrepTrim *trim = loop.Trim((anchor + position) % loop.TrimCount());
	    ON_BrepTrim *previous = loop.Trim(
		(anchor + position + loop.TrimCount() - 1) % loop.TrimCount());
	    if (!trim || !previous) {
		joined_in_model_space = false;
		break;
	    }
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint current_uv = trim->PointAtStart();
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint current_lift = surface->PointAt(
		current_uv.x, current_uv.y);
	    if (!previous_lift.IsValid() || !current_lift.IsValid() ||
		    previous_lift.DistanceTo(current_lift) > LocalUnits::tolerance) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id << ": aligned loop "
			<< *loop_index << " chain solve skipped at join " << position
			<< " lift=" << previous_lift.DistanceTo(current_lift)
			<< std::endl;
		joined_in_model_space = false;
		break;
	    }
	    ordered_trims.push_back(trim);
	}
	if (!joined_in_model_space)
	    continue;

	struct PeriodicBranchState {
	    int shift[2];
	    ON_3dPoint start;
	    ON_3dPoint end;
	};
	std::vector<std::vector<PeriodicBranchState> > states(ordered_trims.size());
	for (size_t position = 0; position < ordered_trims.size(); ++position) {
	    ON_BrepTrim *trim = ordered_trims[position];
	    int seam_fixed_direction = -1;
	    if (trim->m_type == ON_BrepTrim::seam) {
		if (trim->m_iso == ON_Surface::W_iso || trim->m_iso == ON_Surface::E_iso)
		    seam_fixed_direction = 0;
		else if (trim->m_iso == ON_Surface::S_iso || trim->m_iso == ON_Surface::N_iso)
		    seam_fixed_direction = 1;
	    }
	    const bool fixed0 = position == 0 || !(period[0] > ON_ZERO_TOLERANCE) ||
		(trim->m_type == ON_BrepTrim::seam && seam_fixed_direction != 1);
	    const bool fixed1 = position == 0 || !(period[1] > ON_ZERO_TOLERANCE) ||
		(trim->m_type == ON_BrepTrim::seam && seam_fixed_direction != 0);
	    const int minimum_shift0 = fixed0 ? 0 : -1;
	    const int maximum_shift0 = fixed0 ? 0 : 1;
	    const int minimum_shift1 = fixed1 ? 0 : -1;
	    const int maximum_shift1 = fixed1 ? 0 : 1;
	    for (int shift0 = minimum_shift0; shift0 <= maximum_shift0; ++shift0) {
		for (int shift1 = minimum_shift1; shift1 <= maximum_shift1; ++shift1) {
		    PeriodicBranchState state;
		    state.shift[0] = shift0;
		    state.shift[1] = shift1;
		    state.start = trim->PointAtStart();
		    state.end = trim->PointAtEnd();
		    state.start.x += shift0 * period[0];
		    state.end.x += shift0 * period[0];
		    state.start.y += shift1 * period[1];
		    state.end.y += shift1 * period[1];
		    states[position].push_back(state);
		}
	    }
	}

	const size_t position_count = ordered_trims.size();
	std::vector<std::vector<double> > costs(position_count);
	std::vector<std::vector<int> > predecessors(position_count);
	for (size_t position = 0; position < position_count; ++position) {
	    costs[position].assign(states[position].size(), DBL_MAX);
	    predecessors[position].assign(states[position].size(), -1);
	}
	costs[0][0] = 0.0;
	for (size_t position = 1; position < position_count; ++position) {
	    for (size_t current = 0; current < states[position].size(); ++current) {
		for (size_t previous = 0; previous < states[position - 1].size(); ++previous) {
		    if (costs[position - 1][previous] >= DBL_MAX)
			continue;
		    const double candidate_cost = costs[position - 1][previous] +
			states[position - 1][previous].end.DistanceTo(
			    states[position][current].start);
		    if (candidate_cost < costs[position][current]) {
			costs[position][current] = candidate_cost;
			predecessors[position][current] = static_cast<int>(previous);
		    }
		}
	    }
	}

	double original_cost = 0.0;
	for (size_t position = 0; position < position_count; ++position) {
	    const size_t next = (position + 1) % position_count;
	    original_cost += ordered_trims[position]->PointAtEnd().DistanceTo(
		ordered_trims[next]->PointAtStart());
	}
	double best_cost = DBL_MAX;
	int best_last = -1;
	for (size_t state = 0; state < states[position_count - 1].size(); ++state) {
	    if (costs[position_count - 1][state] >= DBL_MAX)
		continue;
	    const double cyclic_cost = costs[position_count - 1][state] +
		states[position_count - 1][state].end.DistanceTo(states[0][0].start);
	    if (cyclic_cost < best_cost) {
		best_cost = cyclic_cost;
		best_last = static_cast<int>(state);
	    }
	}
	const double improvement_floor = std::max(ON_ZERO_TOLERANCE,
	    original_cost * 1.0e-12);
	if (best_last < 0 || !(best_cost + improvement_floor < original_cost)) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": aligned loop "
		    << *loop_index << " chain solve retained cost " << original_cost
		    << " (best " << best_cost << ")" << std::endl;
	    continue;
	}

	std::vector<int> selected(position_count, 0);
	selected[position_count - 1] = best_last;
	bool complete_path = true;
	for (size_t reverse_position = position_count - 1;
		reverse_position > 0; --reverse_position) {
	    selected[reverse_position - 1] =
		predecessors[reverse_position][selected[reverse_position]];
	    if (selected[reverse_position - 1] < 0) {
		complete_path = false;
		break;
	    }
	}
	if (!complete_path)
	    continue;

	struct ShiftedTrimCurve {
	    ON_BrepTrim *trim;
	    ON_Curve *curve;
	};
	std::vector<ShiftedTrimCurve> candidates;
	bool exact = true;
	for (size_t position = 0; position < position_count; ++position) {
	    const PeriodicBranchState &state =
		states[position][selected[position]];
	    if (state.shift[0] == 0 && state.shift[1] == 0)
		continue;
	    ON_BrepTrim *trim = ordered_trims[position];
	    ON_Curve *candidate = trim->DuplicateCurve();
	    ON_Xform translation(ON_Xform::IdentityTransformation);
	    translation.m_xform[0][3] = state.shift[0] * period[0];
	    translation.m_xform[1][3] = state.shift[1] * period[1];
	    if (!candidate || !candidate->Transform(translation) ||
		    !candidate->ChangeDimension(2) || !candidate->IsValid()) {
		delete candidate;
		exact = false;
		break;
	    }
	    const ON_Interval trim_domain = trim->Domain();
	    for (int sample = 0; sample <= 64; ++sample) {
		const double parameter = trim_domain.ParameterAt(
		    static_cast<double>(sample) / 64.0);
		const ON_3dPoint original_uv = trim->PointAt(parameter);
		const ON_3dPoint candidate_uv = candidate->PointAt(parameter);
		const ON_3dPoint original_lift = surface->PointAt(
		    original_uv.x, original_uv.y);
		const ON_3dPoint candidate_lift = surface->PointAt(
		    candidate_uv.x, candidate_uv.y);
		if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
			original_lift.DistanceTo(candidate_lift) > lift_tolerance) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": aligned loop " << *loop_index
			    << " chain shift rejected for trim " << trim->m_trim_index
			    << " at sample " << sample << " lift="
			    << original_lift.DistanceTo(candidate_lift)
			    << " tolerance=" << lift_tolerance << std::endl;
		    exact = false;
		    break;
		}
	    }
	    if (!exact) {
		delete candidate;
		break;
	    }
	    ShiftedTrimCurve shifted = {trim, candidate};
	    candidates.push_back(shifted);
	}

	for (size_t position = 0; exact && position < position_count; ++position) {
	    const size_t next = (position + 1) % position_count;
	    const ON_3dPoint end_uv =
		states[position][selected[position]].end;
	    const ON_3dPoint start_uv = states[next][selected[next]].start;
	    const ON_3dPoint end_lift = surface->PointAt(end_uv.x, end_uv.y);
	    const ON_3dPoint start_lift = surface->PointAt(start_uv.x, start_uv.y);
	    exact = end_lift.IsValid() && start_lift.IsValid() &&
		end_lift.DistanceTo(start_lift) <= LocalUnits::tolerance;
	}
	if (!exact || candidates.empty()) {
	    for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
		delete candidates[candidate].curve;
	    continue;
	}

	std::vector<int> curve_indices(candidates.size(), -1);
	bool added = true;
	for (size_t candidate = 0; candidate < candidates.size(); ++candidate) {
	    curve_indices[candidate] = brep->AddTrimCurve(candidates[candidate].curve);
	    if (curve_indices[candidate] < 0) {
		delete candidates[candidate].curve;
		candidates[candidate].curve = NULL;
		added = false;
		break;
	    }
	}
	if (!added) {
	    for (size_t candidate = 0; candidate < candidates.size(); ++candidate) {
		if (curve_indices[candidate] < 0 && candidates[candidate].curve)
		    delete candidates[candidate].curve;
	    }
	    continue;
	}
	bool installed = true;
	for (size_t candidate = 0; candidate < candidates.size(); ++candidate) {
	    if (!brep->SetTrimCurve(*candidates[candidate].trim,
		    curve_indices[candidate])) {
		installed = false;
		break;
	    }
	    brep->SetTrimIsoFlags(*candidates[candidate].trim);
	}
	if (installed)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"unwrapped an exact pcurve chain after periodic surface alignment");
    }
}


static void
repair_bounded_seam_isos(ON_Brep *brep, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type, bool allow_surface_alignment,
	std::vector<int> *aligned_surface_loops_out = NULL)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return;
    const double tolerance = LocalUnits::tolerance;
    std::vector<int> aligned_surface_loops;
    repair_seam_pair_from_exact_edge(brep, wrapper, entity_id, entity_type,
	NULL, &aligned_surface_loops, allow_surface_alignment);
    std::vector<int> projected_seam_loops;
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_type != ON_BrepTrim::seam ||
		trim.m_iso == ON_Surface::W_iso || trim.m_iso == ON_Surface::E_iso ||
		trim.m_iso == ON_Surface::S_iso || trim.m_iso == ON_Surface::N_iso ||
		trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count())
	    continue;
	const int face_index = brep->m_L[trim.m_li].m_fi;
	if (face_index < 0 || face_index >= brep->m_F.Count())
	    continue;
	const ON_Surface *surface = brep->m_F[face_index].SurfaceOf();
	const ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	ON_NurbsCurve edge_nurbs;
	if (!surface || !trim.TrimCurveOf() || !edge.GetNurbForm(edge_nurbs))
	    continue;
	const ON_Interval edge_domain = edge_nurbs.Domain();
	std::vector<ON_3dPoint> uv_points;
	const ON_PolylineCurve *polyline = ON_PolylineCurve::Cast(trim.TrimCurveOf());
	if (polyline && polyline->m_pline.Count() >= 2) {
	    uv_points.reserve(polyline->m_pline.Count());
	    for (int point = 0; point < polyline->m_pline.Count(); ++point)
		uv_points.push_back(polyline->m_pline[point]);
	} else {
	    const int span_count = trim.SpanCount();
	    if (span_count <= 0 || span_count > kMaximumPcurveSpans)
		continue;
	    std::vector<double> spans(span_count + 1);
	    if (!trim.GetSpanVector(&spans[0]))
		continue;
	    uv_points.reserve(span_count * 4 + 1);
	    for (int span = 0; span < span_count; ++span) {
		for (int sub = 0; sub < 4; ++sub) {
		    const double fraction = static_cast<double>(sub) / 4.0;
		    uv_points.push_back(trim.PointAt(
			(1.0 - fraction) * spans[span] + fraction * spans[span + 1]));
		}
	    }
	    uv_points.push_back(trim.PointAt(spans[span_count]));
	}
	if (uv_points.size() < 2)
	    continue;

	double best_score = DBL_MAX;
	ON_Surface::ISO best_iso = ON_Surface::not_iso;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = surface->Domain(direction);
	    for (int side = 0; side < 2; ++side) {
		const double boundary = domain[side];
		bool valid = true;
		double score = 0.0;
		for (size_t segment = 0; valid && segment + 1 < uv_points.size(); ++segment) {
		    for (int sub = 0; sub < 3; ++sub) {
			const double fraction = static_cast<double>(sub) / 2.0;
			ON_3dPoint uv = (1.0 - fraction) * uv_points[segment] +
			    fraction * uv_points[segment + 1];
			ON_3dPoint boundary_uv = uv;
			boundary_uv[direction] = boundary;
			const ON_3dPoint original_lift = surface->PointAt(uv.x, uv.y);
			const ON_3dPoint boundary_lift = surface->PointAt(
			    boundary_uv.x, boundary_uv.y);
			if (!original_lift.IsValid() || !boundary_lift.IsValid()) {
			    valid = false;
			    break;
			}
			double edge_parameter = 0.0;
			const bool have_closest = ON_NurbsCurve_GetClosestPoint(
			    &edge_parameter, &edge_nurbs, boundary_lift);
			double edge_distance = have_closest ? boundary_lift.DistanceTo(
			    edge_nurbs.PointAt(edge_parameter)) : DBL_MAX;
			const double normalized = (static_cast<double>(segment) + fraction) /
			    static_cast<double>(uv_points.size() - 1);
			const ON_3dPoint forward = edge_nurbs.PointAt(
			    edge_domain.ParameterAt(normalized));
			const ON_3dPoint reverse = edge_nurbs.PointAt(
			    edge_domain.ParameterAt(1.0 - normalized));
			if (forward.IsValid())
			    edge_distance = std::min(edge_distance,
				boundary_lift.DistanceTo(forward));
			if (reverse.IsValid())
			    edge_distance = std::min(edge_distance,
				boundary_lift.DistanceTo(reverse));
			const double source_distance = original_lift.DistanceTo(boundary_lift);
			if (source_distance > tolerance && edge_distance > tolerance) {
			    valid = false;
			    break;
			}
			score += source_distance;
		    }
		}
		if (!valid || score >= best_score)
		    continue;
		best_score = score;
		if (direction == 0)
		    best_iso = side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso;
		else
		    best_iso = side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso;
	    }
	}
	if (best_iso == ON_Surface::not_iso) {
	    if (wrapper->Verbose()) {
		ON_BoundingBox box = trim.BoundingBox();
		std::cerr << entity_type << " #" << entity_id << ": unresolved seam trim "
		    << ti << " (loop " << trim.m_li << ", edge " << trim.m_ei
		    << ", reversed " << (trim.m_bRev3d ? "yes" : "no")
		    << ", edge uses";
		for (int use = 0; use < edge.m_ti.Count(); ++use) {
		    const int other_index = edge.m_ti[use];
		    if (other_index < 0 || other_index >= brep->m_T.Count())
			continue;
		    const ON_BrepTrim &other = brep->m_T[other_index];
		    std::cerr << ' ' << other_index << "/L" << other.m_li << '/'
			<< (other.m_bRev3d ? 'R' : 'F');
		}
		std::cerr << ", surface closed=" << (surface->IsClosed(0) ? '1' : '0')
		    << (surface->IsClosed(1) ? '1' : '0') << ", domains="
		    << surface->Domain(0).Min() << ':' << surface->Domain(0).Max() << ','
		    << surface->Domain(1).Min() << ':' << surface->Domain(1).Max()
		    << ", uv box=" << box.m_min.x << ':' << box.m_max.x << ','
		    << box.m_min.y << ':' << box.m_max.y << ')' << std::endl;
	    }
	    continue;
	}

	/* m_iso is derived state.  Merely assigning a boundary flag leaves an
	 * invalid brep because ON_Brep::IsValid() independently derives the flag
	 * from the pcurve.  Project the accepted pcurve itself onto the periodic
	 * boundary.  This affine projection preserves its parameterization and
	 * direction in the varying coordinate. */
	/* Duplicate the trim proxy, rather than its underlying C2 curve.  This
	 * preserves any active subdomain and reversal as ordinary curve geometry,
	 * allowing SetTrimCurve() to install it without protected proxy state. */
	ON_Curve *projected = trim.DuplicateCurve();
	if (!projected)
	    continue;
	int direction = -1;
	double boundary = ON_UNSET_VALUE;
	if (best_iso == ON_Surface::W_iso || best_iso == ON_Surface::E_iso) {
	    direction = 0;
	    boundary = surface->Domain(0)[best_iso == ON_Surface::E_iso ? 1 : 0];
	} else if (best_iso == ON_Surface::S_iso || best_iso == ON_Surface::N_iso) {
	    direction = 1;
	    boundary = surface->Domain(1)[best_iso == ON_Surface::N_iso ? 1 : 0];
	}
	ON_Xform projection(ON_Xform::IdentityTransformation);
	if (direction < 0) {
	    delete projected;
	    continue;
	}
	for (int column = 0; column < 4; ++column)
	    projection.m_xform[direction][column] = 0.0;
	projection.m_xform[direction][3] = boundary;
	if (!projected->Transform(projection) || !projected->ChangeDimension(2) ||
		!projected->IsValid()) {
	    delete projected;
	    continue;
	}

	const ON_Interval trim_domain = trim.Domain();
	ON_Surface::ISO projected_iso = surface->IsIsoparametric(*projected,
	    &trim_domain);
	if (projected_iso != best_iso) {
	    /* Some imported pullbacks are geometrically on the boundary but retain
	     * a tiny backtracking wiggle in their varying coordinate.  openNURBS
	     * correctly refuses to call that an isoparametric curve.  A boundary
	     * line is an admissible regeneration only if the dense lift/edge check
	     * below proves it remains within the asserted model tolerance. */
	    ON_LineCurve *line = new ON_LineCurve(projected->PointAtStart(),
		projected->PointAtEnd());
	    if (line->ChangeDimension(2) && line->SetDomain(trim_domain.Min(),
		    trim_domain.Max()) && line->IsValid()) {
		delete projected;
		projected = line;
		projected_iso = surface->IsIsoparametric(*projected, &trim_domain);
	    } else {
		delete line;
	    }
	}
	if (projected_iso != best_iso) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": seam trim " << ti
		    << " projection rejected: derived iso "
		    << static_cast<int>(projected_iso) << " != boundary "
		    << static_cast<int>(best_iso) << std::endl;
	    delete projected;
	    continue;
	}
	const int validation_samples = std::min(kDenseValidationSegments,
	    std::max(64, trim.SpanCount() * 8));
	bool projected_valid = true;
	double rejected_fraction = 0.0;
	    double rejected_lift_distance = 0.0;
	double rejected_edge_distance = 0.0;
	for (int sample = 0; sample <= validation_samples; ++sample) {
	    const double fraction = static_cast<double>(sample) / validation_samples;
	    const double trim_parameter = trim_domain.ParameterAt(fraction);
	    const ON_3dPoint original_uv = trim.PointAt(trim_parameter);
	    const ON_3dPoint projected_uv = projected->PointAt(trim_parameter);
	    const ON_3dPoint original_lift = surface->PointAt(original_uv.x, original_uv.y);
	    const ON_3dPoint projected_lift = surface->PointAt(projected_uv.x, projected_uv.y);
	    double edge_parameter = 0.0;
	    rejected_lift_distance = original_lift.DistanceTo(projected_lift);
	    bool closest = ON_NurbsCurve_GetClosestPoint(&edge_parameter,
		&edge_nurbs, projected_lift);
	    rejected_edge_distance = closest ? projected_lift.DistanceTo(
		edge_nurbs.PointAt(edge_parameter)) : DBL_MAX;
	    const ON_3dPoint forward = edge_nurbs.PointAt(
		edge_domain.ParameterAt(fraction));
	    const ON_3dPoint reverse = edge_nurbs.PointAt(
		edge_domain.ParameterAt(1.0 - fraction));
	    if (forward.IsValid()) {
		rejected_edge_distance = std::min(rejected_edge_distance,
		    projected_lift.DistanceTo(forward));
		closest = true;
	    }
	    if (reverse.IsValid()) {
		rejected_edge_distance = std::min(rejected_edge_distance,
		    projected_lift.DistanceTo(reverse));
		closest = true;
	    }
	    if (!original_lift.IsValid() || !projected_lift.IsValid() ||
		    (rejected_lift_distance > tolerance &&
		     (!closest || rejected_edge_distance > tolerance))) {
		rejected_fraction = fraction;
		projected_valid = false;
		break;
	    }
	}
	if (!projected_valid) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": seam trim " << ti
		    << " projection rejected at normalized " << rejected_fraction
		    << ": lift delta=" << rejected_lift_distance
		    << ", edge distance=" << rejected_edge_distance
		    << ", tolerance=" << tolerance << std::endl;
	    delete projected;
	    continue;
	}

	const int c2_index = brep->AddTrimCurve(projected);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
	    if (c2_index < 0)
		delete projected;
	    continue;
	}
	brep->SetTrimIsoFlags(trim);
	if (std::find(projected_seam_loops.begin(), projected_seam_loops.end(),
		trim.m_li) == projected_seam_loops.end())
	    projected_seam_loops.push_back(trim.m_li);
	wrapper->RecordRepair(entity_id, entity_type, "trim_iso",
	    "projected a seam pcurve onto a periodic boundary within model tolerance");
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": projected seam trim "
		<< ti << " onto periodic boundary " << static_cast<int>(best_iso)
		<< std::endl;
    }
    repair_paired_seam_boundaries(brep, wrapper, entity_id, entity_type,
	&aligned_surface_loops);
    repair_aligned_surface_loop_branches(brep, wrapper, entity_id, entity_type,
	aligned_surface_loops);
    for (std::vector<int>::const_iterator projected_loop =
	    projected_seam_loops.begin(); projected_loop != projected_seam_loops.end();
	 ++projected_loop) {
	if (*projected_loop < 0 || *projected_loop >= brep->m_L.Count())
	    continue;
	ON_BrepLoop &loop = brep->m_L[*projected_loop];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	if (!surface)
	    continue;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!trim || !previous || !next || trim->m_type == ON_BrepTrim::seam ||
		    previous->m_type != ON_BrepTrim::seam ||
		    next->m_type != ON_BrepTrim::seam || trim->m_ei < 0 ||
		    trim->m_ei >= brep->m_E.Count())
		continue;
	    ON_BrepEdge &edge = brep->m_E[trim->m_ei];
	    if (edge.m_vi[0] < 0 || edge.m_vi[0] != edge.m_vi[1])
		continue;
	    const ON_3dPoint start = previous->PointAtEnd();
	    const ON_3dPoint end = next->PointAtStart();
	    if (trim->PointAtStart().DistanceTo(start) <= ON_ZERO_TOLERANCE &&
		    trim->PointAtEnd().DistanceTo(end) <= ON_ZERO_TOLERANCE)
		continue;
	    ON_LineCurve *candidate = new ON_LineCurve(start, end);
	    const ON_Interval trim_domain = trim->Domain();
	    bool valid = candidate->ChangeDimension(2) &&
		candidate->SetDomain(trim_domain.Min(), trim_domain.Max()) &&
		candidate->IsValid();
	    const ON_Surface::ISO candidate_iso = valid ?
		surface->IsIsoparametric(*candidate, &trim_domain) :
		ON_Surface::not_iso;
	    const bool candidate_is_constant_parameter =
		candidate_iso != ON_Surface::not_iso;
	    const bool same_iso_direction =
		(candidate_is_constant_parameter &&
		 (trim->m_iso == ON_Surface::not_iso ||
		  (static_cast<int>(candidate_iso) % 2) ==
		  (static_cast<int>(trim->m_iso) % 2))) ||
		(candidate_iso == ON_Surface::not_iso &&
		 trim->m_iso == ON_Surface::not_iso);
	    valid = valid && same_iso_direction;
	    ON_NurbsCurve edge_nurbs;
	    const bool have_edge_nurbs = edge.GetNurbForm(edge_nurbs);
	    double rejected_edge_distance = 0.0;
	    double rejected_fraction = 0.0;
	    bool rejected_closest_point = false;
	    for (int sample = 0; valid && sample <= kDenseValidationSegments; ++sample) {
		const double fraction = static_cast<double>(sample) /
		    kDenseValidationSegments;
		const ON_3dPoint uv = candidate->PointAt(
		    trim_domain.ParameterAt(fraction));
		const ON_3dPoint lifted = surface->PointAt(uv.x, uv.y);
		double edge_parameter = 0.0;
		bool closest = false;
		if (lifted.IsValid() &&
			(sample == 0 || sample == kDenseValidationSegments) &&
			edge.m_vi[0] >= 0 && edge.m_vi[0] < brep->m_V.Count()) {
		    rejected_edge_distance = lifted.DistanceTo(
			brep->m_V[edge.m_vi[0]].point);
		    closest = true;
		} else if (have_edge_nurbs) {
		    closest = lifted.IsValid() &&
			ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs, lifted);
		    rejected_edge_distance = closest ?
			lifted.DistanceTo(edge_nurbs.PointAt(edge_parameter)) : DBL_MAX;
		    if (lifted.IsValid()) {
			const ON_Interval edge_domain = edge_nurbs.Domain();
			const ON_3dPoint forward = edge_nurbs.PointAt(
			    edge_domain.ParameterAt(fraction));
			const ON_3dPoint reverse = edge_nurbs.PointAt(
			    edge_domain.ParameterAt(1.0 - fraction));
			if (forward.IsValid()) {
			    rejected_edge_distance = std::min(rejected_edge_distance,
				lifted.DistanceTo(forward));
			    closest = true;
			}
			if (reverse.IsValid()) {
			    rejected_edge_distance = std::min(rejected_edge_distance,
				lifted.DistanceTo(reverse));
			    closest = true;
			}
		    }
		} else if (lifted.IsValid()) {
		    const ON_3dPoint original_uv = trim->PointAt(
			trim_domain.ParameterAt(fraction));
		    const ON_3dPoint original_lift = surface->PointAt(
			original_uv.x, original_uv.y);
		    if (original_lift.IsValid()) {
			rejected_edge_distance = lifted.DistanceTo(original_lift);
			closest = true;
		    }
		}
		if (!closest || rejected_edge_distance > tolerance) {
		    rejected_fraction = fraction;
		    rejected_closest_point = !closest;
		    valid = false;
		}
	    }
	    if (!valid) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": projected-loop closed iso candidate rejected for trim "
			<< trim->m_trim_index << "/STEP edge " << edge.m_edge_user.i
			<< " candidate iso=" << static_cast<int>(candidate_iso)
			<< " original iso=" << static_cast<int>(trim->m_iso)
			<< " at normalized " << rejected_fraction
			<< (rejected_closest_point ? " (closest point failed)" : "")
			<< " edge distance=" << rejected_edge_distance
			<< " tolerance=" << tolerance << std::endl;
		delete candidate;
		continue;
	    }
	    const int c2_index = brep->AddTrimCurve(candidate);
	    if (c2_index < 0 || !brep->SetTrimCurve(*trim, c2_index)) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": projected-loop closed iso candidate installation failed for trim "
			<< trim->m_trim_index << "/STEP edge " << edge.m_edge_user.i
			<< std::endl;
		if (c2_index < 0)
		    delete candidate;
		continue;
	    }
	    brep->SetTrimIsoFlags(*trim);
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		candidate_iso == ON_Surface::not_iso ?
		"regenerated a closed pcurve between exact seam boundaries" :
		"unwrapped a closed isoparametric edge between exact seam boundaries");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << (candidate_iso == ON_Surface::not_iso ?
			": regenerated closed trim " :
			": unwrapped closed isoparametric trim ")
		    << trim->m_trim_index << "/STEP edge " << edge.m_edge_user.i
		    << " between projected seam boundaries" << std::endl;
	}
    }
    /* Boundary projection can establish the exact periodic branches only
     * after the first seam-pair pass.  Run the bounded exact matcher again so
     * adjacent closed isoparametric edges inherit those proven endpoints. */
    if (!projected_seam_loops.empty())
	repair_seam_pair_from_exact_edge(brep, wrapper, entity_id, entity_type,
	    &projected_seam_loops, NULL, allow_surface_alignment);
    if (aligned_surface_loops_out)
	*aligned_surface_loops_out = aligned_surface_loops;
}


static void
repair_adjacent_trim_vertices(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    bool compact_needed = false;
    const int repair_budget = brep->m_V.Count();
    for (int repair = 0; repair < repair_budget; ++repair) {
	bool changed = false;
	for (int li = 0; li < brep->m_L.Count() && !changed; ++li) {
	    ON_BrepLoop &loop = brep->m_L[li];
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface)
		continue;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		ON_BrepTrim *current = loop.Trim(lti);
		ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!current || !next || current->m_vi[1] == next->m_vi[0] ||
			current->m_vi[1] < 0 || current->m_vi[1] >= brep->m_V.Count() ||
			next->m_vi[0] < 0 || next->m_vi[0] >= brep->m_V.Count())
		    continue;
		ON_BrepVertex &keep = brep->m_V[current->m_vi[1]];
		ON_BrepVertex &remove = brep->m_V[next->m_vi[0]];
		if (keep.m_vertex_index < 0 || remove.m_vertex_index < 0 ||
			keep.point.DistanceTo(remove.point) > LocalUnits::tolerance)
		    continue;
		const ON_3dPoint current_uv = current->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		const ON_3dPoint current_lift = surface->PointAt(current_uv.x, current_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		if (!current_lift.IsValid() || !next_lift.IsValid() ||
			current_lift.DistanceTo(keep.point) > LocalUnits::tolerance ||
			next_lift.DistanceTo(remove.point) > LocalUnits::tolerance ||
			current_lift.DistanceTo(next_lift) > LocalUnits::tolerance)
		    continue;
		const int removed_vertex = remove.m_vertex_index;
		if (!brep->CombineCoincidentVertices(keep, remove))
		    continue;
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    "merged adjacent topology vertices within model tolerance");
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id << ": merged loop "
			<< li << " vertex " << removed_vertex << " into "
			<< keep.m_vertex_index << " within tolerance "
			<< LocalUnits::tolerance << std::endl;
		compact_needed = true;
		changed = true;
		break;
	    }
	}
	if (!changed)
	    break;
    }
    if (compact_needed)
	brep->Compact();
}


static void
repair_invalid_open_pcurves(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const int trim_count = brep->m_T.Count();
    for (int ti = 0; ti < trim_count; ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	const ON_Curve *source_curve = trim.TrimCurveOf();
	if (trim.m_vi[0] < 0 || trim.m_vi[1] < 0 ||
		trim.m_vi[0] == trim.m_vi[1] ||
		trim.m_vi[0] >= brep->m_V.Count() ||
		trim.m_vi[1] >= brep->m_V.Count() || !source_curve ||
		trim.m_ei < 0 ||
		trim.m_ei >= brep->m_E.Count() || trim.m_li < 0 ||
		trim.m_li >= brep->m_L.Count())
	    continue;
	const ON_BrepFace *face = brep->m_L[trim.m_li].Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	ON_NurbsCurve edge_nurbs;
	if (!surface || !edge.GetNurbForm(edge_nurbs))
	    continue;
	const bool closed_open_topology = source_curve->IsClosed();
	bool invalid_exact_endpoint = false;
	bool endpoint_exact[2] = {false, false};
	const ON_Interval trim_domain = trim.Domain();
	const ON_Interval edge_domain = edge.Domain();
	for (int end = 0; end < 2; ++end) {
	    const ON_3dPoint uv = trim.PointAt(trim_domain[end]);
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint edge_point = edge.PointAt(
		edge_domain[trim.m_bRev3d ? 1 - end : end]);
	    const ON_3dPoint &vertex = brep->m_V[trim.m_vi[end]].point;
	    endpoint_exact[end] = uv.IsValid() && lift.IsValid() &&
		edge_point.IsValid() && lift.DistanceTo(vertex) <=
		    LocalUnits::tolerance && lift.DistanceTo(edge_point) <=
		    LocalUnits::tolerance;
	    invalid_exact_endpoint = invalid_exact_endpoint || !endpoint_exact[end];
	}
	if (!closed_open_topology && !invalid_exact_endpoint)
	    continue;
	std::unique_ptr<ON_Curve> original(trim.DuplicateCurve());
	if (!original)
	    continue;
	const ON_Surface::ISO original_iso = trim.m_iso;
	const ON_BrepTrim::TYPE original_type = trim.m_type;
	ON_3dPoint required_uv[2] = {ON_3dPoint::UnsetPoint,
	    ON_3dPoint::UnsetPoint};
	const ON_3dPoint *required_endpoint[2] = {NULL, NULL};
	ON_BrepLoop &loop = brep->m_L[trim.m_li];
	const int loop_offset = loop.IndexOfTrim(trim);
	for (int end = 0; end < 2; ++end) {
	    const ON_BrepTrim *adjacent = NULL;
	    if (loop_offset >= 0 && loop.TrimCount() > 1)
		adjacent = end == 0 ? loop.Trim((loop_offset +
		    loop.TrimCount() - 1) % loop.TrimCount()) :
		    loop.Trim((loop_offset + 1) % loop.TrimCount());
	    const bool same_vertex = adjacent && (end == 0 ?
		adjacent->m_vi[1] == trim.m_vi[0] :
		adjacent->m_vi[0] == trim.m_vi[1]);
	    if (same_vertex) {
		required_uv[end] = end == 0 ? adjacent->PointAtEnd() :
		    adjacent->PointAtStart();
		const ON_3dPoint adjacent_lift = surface->PointAt(
		    required_uv[end].x, required_uv[end].y);
		if (required_uv[end].IsValid() && adjacent_lift.IsValid() &&
			adjacent_lift.DistanceTo(brep->m_V[trim.m_vi[end]].point) <=
			    LocalUnits::tolerance)
		    required_endpoint[end] = &required_uv[end];
	    }
	    if (!required_endpoint[end] && endpoint_exact[end]) {
		required_uv[end] = trim.PointAt(trim_domain[end]);
		required_endpoint[end] = &required_uv[end];
	    }
	}
	std::string failure;
	bool regenerated = regenerate_trim_polyline(brep, trim, surface,
	    edge_nurbs, LocalUnits::tolerance, &failure, NULL,
	    required_endpoint[0], required_endpoint[1]);
	const ON_Curve *candidate = regenerated ? trim.TrimCurveOf() : NULL;
	bool boundary_iso = trim.m_iso == ON_Surface::W_iso ||
	    trim.m_iso == ON_Surface::S_iso || trim.m_iso == ON_Surface::E_iso ||
	    trim.m_iso == ON_Surface::N_iso;
	if (regenerated && candidate && !candidate->IsClosed() &&
		original_type == ON_BrepTrim::seam && !boundary_iso) {
	    int fixed_direction = -1;
	    int fixed_side = -1;
	    double best_deviation = DBL_MAX;
	    const ON_Interval candidate_domain = candidate->Domain();
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval surface_domain = surface->Domain(direction);
		for (int side = 0; side < 2; ++side) {
		    double maximum_deviation = 0.0;
		    for (int sample = 0; sample <= 64; ++sample) {
			const ON_3dPoint uv = candidate->PointAt(
			    candidate_domain.ParameterAt(
				static_cast<double>(sample) / 64.0));
			maximum_deviation = std::max(maximum_deviation,
			    fabs(uv[direction] - surface_domain[side]));
		    }
		    if (maximum_deviation < best_deviation) {
			best_deviation = maximum_deviation;
			fixed_direction = direction;
			fixed_side = side;
		    }
		}
	    }
	    std::unique_ptr<ON_Curve> projected(trim.DuplicateCurve());
	    ON_Surface::ISO target_iso = ON_Surface::not_iso;
	    if (fixed_direction == 0)
		target_iso = fixed_side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso;
	    else if (fixed_direction == 1)
		target_iso = fixed_side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso;
	    if (projected && target_iso != ON_Surface::not_iso) {
		ON_Xform projection(ON_Xform::IdentityTransformation);
		for (int column = 0; column < 4; ++column)
		    projection.m_xform[fixed_direction][column] = 0.0;
		projection.m_xform[fixed_direction][3] =
		    surface->Domain(fixed_direction)[fixed_side];
		const ON_Interval projected_domain = projected->Domain();
		bool exact = projected->Transform(projection) &&
		    projected->ChangeDimension(2) && projected->IsValid() &&
		    !projected->IsClosed() &&
		    surface->IsIsoparametric(*projected, &projected_domain) ==
			target_iso;
		for (int sample = 0; exact && sample <= kDenseValidationSegments;
			++sample) {
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint uv = projected->PointAt(
			projected_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    const ON_3dPoint corresponding = edge.PointAt(
			edge.Domain().ParameterAt(trim.m_bRev3d ?
			    1.0 - fraction : fraction));
		    double edge_parameter = 0.0;
		    double edge_distance = lift.IsValid() && corresponding.IsValid() ?
			lift.DistanceTo(corresponding) : DBL_MAX;
		    if (ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs, lift))
			edge_distance = std::min(edge_distance, lift.DistanceTo(
			    edge_nurbs.PointAt(edge_parameter)));
		    if (edge_distance > LocalUnits::tolerance)
			exact = false;
		}
		if (exact) {
		    const int projected_c2 = brep->AddTrimCurve(projected.release());
		    if (projected_c2 >= 0 && brep->SetTrimCurve(trim, projected_c2)) {
			trim.m_iso = target_iso;
			candidate = trim.TrimCurveOf();
			boundary_iso = true;
		    }
		}
	    }
	}
	bool endpoints_exact = regenerated && candidate && !candidate->IsClosed();
	if (endpoints_exact) {
	    const ON_Interval candidate_domain = candidate->Domain();
	    for (int end = 0; end < 2 && endpoints_exact; ++end) {
		const ON_3dPoint uv = candidate->PointAt(candidate_domain[end]);
		const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		const ON_3dPoint edge_point = edge.PointAt(
		    edge_domain[trim.m_bRev3d ? 1 - end : end]);
		const ON_3dPoint &vertex = brep->m_V[trim.m_vi[end]].point;
		endpoints_exact = uv.IsValid() && lift.IsValid() &&
		    edge_point.IsValid() && lift.DistanceTo(vertex) <=
			LocalUnits::tolerance && lift.DistanceTo(edge_point) <=
			LocalUnits::tolerance;
	    }
	}
	if (endpoints_exact &&
		(original_type != ON_BrepTrim::seam || boundary_iso)) {
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		closed_open_topology ?
		"regenerated a closed pcurve for an open topology edge" :
		(original_type == ON_BrepTrim::seam ?
		 "regenerated an invalid seam pcurve from its exact edge" :
		 "regenerated an invalid open pcurve from its exact edge"));
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id << ": regenerated "
		    << (closed_open_topology ? "closed pcurve " :
			(original_type == ON_BrepTrim::seam ?
			 "invalid seam pcurve " : "invalid open pcurve ")) << ti
		    << " for distinct topology vertices" << std::endl;
	    continue;
	}
	const bool candidate_closed = candidate && candidate->IsClosed();
	const int regenerated_iso = static_cast<int>(trim.m_iso);
	const int original_c2 = brep->AddTrimCurve(original.release());
	if (original_c2 >= 0)
	    brep->SetTrimCurve(trim, original_c2);
	trim.m_iso = original_iso;
	trim.m_type = original_type;
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": "
		<< (closed_open_topology ? "closed pcurve " :
		    (original_type == ON_BrepTrim::seam ?
		     "invalid seam pcurve " : "invalid open pcurve "))
		<< ti << " exact-edge regeneration rejected: regenerated="
		<< (regenerated ? "yes" : "no") << ", candidate="
		<< (candidate ? (candidate_closed ? "closed" : "open") : "none")
		<< ", iso=" << regenerated_iso
		<< (failure.empty() ? "" : ", detail=" + failure) << std::endl;
    }
}


static void
repair_missing_singular_trims(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const int repair_budget = brep->m_T.Count();
    for (int repair = 0; repair < repair_budget; ++repair) {
	bool changed = false;
	for (int li = 0; li < brep->m_L.Count() && !changed; ++li) {
	    ON_BrepLoop &loop = brep->m_L[li];
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    const int original_count = loop.TrimCount();
	    if (!surface || original_count < 2)
		continue;
	    for (int lti = 0; lti < original_count; ++lti) {
		const ON_BrepTrim *first = loop.Trim(lti);
		const ON_BrepTrim *second = loop.Trim((lti + 1) % original_count);
		if (!first || !second || first->m_vi[1] < 0 ||
			first->m_vi[1] != second->m_vi[0] ||
			first->m_vi[1] >= brep->m_V.Count() ||
			first->PointAtEnd().DistanceTo(second->PointAtStart()) <=
			    ON_ZERO_TOLERANCE)
		    continue;
		const ON_3dPoint first_end = first->PointAtEnd();
		const ON_3dPoint second_start = second->PointAtStart();
		std::unique_ptr<ON_LineCurve> singular_curve(
		    new ON_LineCurve(first_end, second_start));
		ON_Surface::ISO singular_iso = ON_Surface::not_iso;
		if (singular_curve->ChangeDimension(2) && singular_curve->IsValid())
		    singular_iso = surface->IsIsoparametric(*singular_curve);
		const auto singular_side = [](ON_Surface::ISO iso) {
		    switch (iso) {
			case ON_Surface::S_iso: return 0;
			case ON_Surface::E_iso: return 1;
			case ON_Surface::N_iso: return 2;
			case ON_Surface::W_iso: return 3;
			default: return -1;
		    }
		};
		int surface_side = singular_side(singular_iso);
		if (surface_side < 0) {
		    /* Some imported NURBS surfaces collapse a boundary without
		     * advertising it through IsSingular().  Project the straight
		     * connector onto the nearest exact boundary; the dense model-space
		     * vertex test below, not the missing flag, is authoritative. */
		    const int varying_direction = fabs(second_start.y - first_end.y) >
			fabs(second_start.x - first_end.x) ? 1 : 0;
		    const int fixed_direction = 1 - varying_direction;
		    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
		    const double fixed_parameter = 0.5 *
			(first_end[fixed_direction] + second_start[fixed_direction]);
		    const int side = fabs(fixed_parameter - fixed_domain.Min()) <=
			fabs(fixed_parameter - fixed_domain.Max()) ? 0 : 1;
		    ON_3dPoint start = first_end;
		    ON_3dPoint end = second_start;
		    start[fixed_direction] = fixed_domain[side];
		    end[fixed_direction] = fixed_domain[side];
		    std::unique_ptr<ON_LineCurve> boundary_curve(
			new ON_LineCurve(start, end));
		    const ON_Surface::ISO boundary_iso = fixed_direction == 0 ?
			(side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
			(side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		    if (boundary_curve->ChangeDimension(2) && boundary_curve->IsValid() &&
			    surface->IsIsoparametric(*boundary_curve) == boundary_iso) {
			singular_curve = std::move(boundary_curve);
			singular_iso = boundary_iso;
			surface_side = singular_side(singular_iso);
		    }
		}
		if (surface_side < 0) {
		    /* A paired periodic seam can be a few parameter-space ulps away
		     * from the exact singular boundary.  Project only that established
		     * seam configuration to the boundary before testing its 3D lift. */
		    int seam_direction = -1;
		    if (first->m_type == ON_BrepTrim::seam &&
			    second->m_type == ON_BrepTrim::seam &&
			    ((first->m_iso == ON_Surface::W_iso &&
			      second->m_iso == ON_Surface::E_iso) ||
			     (first->m_iso == ON_Surface::E_iso &&
			      second->m_iso == ON_Surface::W_iso)))
			seam_direction = 0;
		    else if (first->m_type == ON_BrepTrim::seam &&
			    second->m_type == ON_BrepTrim::seam &&
			    ((first->m_iso == ON_Surface::S_iso &&
			      second->m_iso == ON_Surface::N_iso) ||
			     (first->m_iso == ON_Surface::N_iso &&
			      second->m_iso == ON_Surface::S_iso)))
			seam_direction = 1;
		    if (seam_direction < 0 || !surface->IsClosed(seam_direction))
			continue;
		    const int singular_direction = 1 - seam_direction;
		    const ON_Interval singular_domain =
			surface->Domain(singular_direction);
		    const double parameter = 0.5 *
			(first_end[singular_direction] +
			 second_start[singular_direction]);
		    const int side = fabs(parameter - singular_domain.Min()) <=
			fabs(parameter - singular_domain.Max()) ? 0 : 1;
		    surface_side = singular_direction == 0 ?
			(side == 0 ? 3 : 1) : (side == 0 ? 0 : 2);
		    if (!surface->IsSingular(surface_side))
			continue;
		    singular_iso = singular_direction == 0 ?
			(side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
			(side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		    ON_3dPoint start = first_end;
		    ON_3dPoint end = second_start;
		    start[singular_direction] = singular_domain[side];
		    end[singular_direction] = singular_domain[side];
		    singular_curve.reset(new ON_LineCurve(start, end));
		    if (!singular_curve->ChangeDimension(2) ||
			    !singular_curve->IsValid() ||
			    surface->IsIsoparametric(*singular_curve) != singular_iso)
			continue;
		}
		const int vertex_index = first->m_vi[1];
		const ON_3dPoint vertex = brep->m_V[vertex_index].point;
		if (!surface->IsSingular(surface_side)) {
		    const int fixed_direction =
			(surface_side == 1 || surface_side == 3) ? 0 : 1;
		    const int varying_direction = 1 - fixed_direction;
		    const int side =
			(surface_side == 1 || surface_side == 2) ? 1 : 0;
		    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
		    const ON_Interval varying_domain = surface->Domain(varying_direction);
		    bool collapsed_boundary = fixed_domain.IsIncreasing() &&
			varying_domain.IsIncreasing();
		    for (int sample = 0; collapsed_boundary &&
			    sample <= kDenseValidationSegments; ++sample) {
			ON_3dPoint uv;
			uv[fixed_direction] = fixed_domain[side];
			uv[varying_direction] = varying_domain.ParameterAt(
			    static_cast<double>(sample) / kDenseValidationSegments);
			uv.z = 0.0;
			const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			collapsed_boundary = lift.IsValid() &&
			    lift.DistanceTo(vertex) <= LocalUnits::tolerance;
		    }
		    if (!collapsed_boundary)
			continue;
		}
		bool exact = true;
		for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
		    const ON_3dPoint uv = singular_curve->PointAt(
			singular_curve->Domain().ParameterAt(
			    static_cast<double>(sample) / kDenseValidationSegments));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    if (!lift.IsValid() || lift.DistanceTo(vertex) >
			    LocalUnits::tolerance) {
			exact = false;
			break;
		    }
		}
		if (!exact)
		    continue;
		std::vector<int> original_trims;
		original_trims.reserve(original_count);
		for (int offset = 0; offset < original_count; ++offset)
		    original_trims.push_back(loop.m_ti[offset]);
		const int c2_index = brep->AddTrimCurve(singular_curve.release());
		if (c2_index < 0)
		    continue;
		const int singular_index = brep->NewSingularTrim(
		    brep->m_V[vertex_index], loop, singular_iso, c2_index).m_trim_index;
		loop.m_ti.SetCount(0);
		for (int offset = 0; offset < original_count; ++offset) {
		    loop.m_ti.Append(original_trims[offset]);
		    if (offset == lti)
			loop.m_ti.Append(singular_index);
		}
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    "inserted an exact singular trim at a surface pole");
		changed = true;
		break;
	    }
	}
	if (!changed)
	    break;
    }
}


static bool
validate_periodic_trim_translation(const ON_Surface *surface,
	const ON_BrepTrim &original, const ON_Curve &candidate,
	std::string *failure)
{
    if (failure)
	failure->clear();
    if (!surface)
	return false;
    const ON_Interval domain = original.Domain();
    const int validation_spans = std::max(original.SpanCount(),
	candidate.SpanCount());
    const int samples = std::min(4096, std::max(64, validation_spans * 8));
    const double exact_lift_tolerance = std::max(
	ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	LocalUnits::tolerance * 1.0e-8);
    for (int sample = 0; sample <= samples; ++sample) {
	const double fraction = static_cast<double>(sample) / samples;
	const double parameter = domain.ParameterAt(fraction);
	const ON_3dPoint original_uv = original.PointAt(parameter);
	const ON_3dPoint candidate_uv = candidate.PointAt(parameter);
	const ON_3dPoint original_lift = surface->PointAt(
	    original_uv.x, original_uv.y);
	const ON_3dPoint candidate_lift = surface->PointAt(
	    candidate_uv.x, candidate_uv.y);
	if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
		original_lift.DistanceTo(candidate_lift) > exact_lift_tolerance) {
	    if (failure)
		*failure = "surface lift changed at sample " +
		    std::to_string(sample) + " by " +
		    std::to_string(original_lift.DistanceTo(candidate_lift));
	    return false;
	}
    }
    return true;
}


static void
repair_adjacent_trim_endpoints(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const auto boundary_axis = [](ON_Surface::ISO iso, int axis) {
	if (iso <= ON_Surface::not_iso)
	    return false;
	return axis == 0 ? (static_cast<int>(iso) % 2 == 1) :
	    (static_cast<int>(iso) % 2 == 0);
    };
    std::set<int> attempted_edge_regeneration;
    const int repair_budget = brep->m_T.Count();
    for (int repair = 0; repair < repair_budget; ++repair) {
	bool changed = false;
	for (int li = 0; li < brep->m_L.Count() && !changed; ++li) {
	    ON_BrepLoop &loop = brep->m_L[li];
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface || loop.TrimCount() < 2)
		continue;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		ON_BrepTrim *previous = loop.Trim(lti);
		ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!previous || !next || previous == next ||
			previous->m_vi[1] < 0 ||
			previous->m_vi[1] != next->m_vi[0] ||
			previous->m_vi[1] >= brep->m_V.Count())
		    continue;
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		if (previous_uv.DistanceTo(next_uv) <= ON_ZERO_TOLERANCE)
		    continue;
		const ON_3dPoint vertex = brep->m_V[previous->m_vi[1]].point;
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		if (!previous_lift.IsValid() || !next_lift.IsValid() ||
			previous_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
			next_lift.DistanceTo(vertex) > LocalUnits::tolerance) {
		    /* The shared topology vertex is the authoritative join.  Two
		     * independently generated pcurve endpoints can lie on opposite
		     * sides of that vertex and therefore be as much as twice the model
		     * uncertainty apart even though each endpoint is individually
		     * valid.  Candidate construction and dense validation below still
		     * require the installed common endpoint to stay within the model
		     * uncertainty of both the exact edge and its original surface lift. */
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop " << li
			    << " endpoint precheck rejected trims "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " vertex distances=" << previous_lift.DistanceTo(vertex)
			    << '/' << next_lift.DistanceTo(vertex) << " lift="
			    << previous_lift.DistanceTo(next_lift) << std::endl;
		    continue;
		}

		/* When exactly one side is a seam, first try moving the entire
		 * non-seam pcurve by integral closed-surface periods.  This preserves
		 * its shape and edge correspondence, unlike bending only its endpoint. */
		if ((previous->m_type == ON_BrepTrim::seam) !=
			(next->m_type == ON_BrepTrim::seam)) {
		    ON_BrepTrim *movable = previous->m_type == ON_BrepTrim::seam ?
			next : previous;
		    const ON_3dPoint movable_join = movable == previous ?
			previous_uv : next_uv;
		    const ON_3dPoint seam_join = movable == previous ? next_uv :
			previous_uv;
		    ON_3dVector translation = seam_join - movable_join;
		    bool integral_periods = true;
		    bool shifted = false;
		    for (int axis = 0; axis < 2; ++axis) {
			const double period = surface->Domain(axis).Length();
			const double parameter_epsilon = std::max(ON_ZERO_TOLERANCE,
			    fabs(period) * 1.0e-10);
			if (fabs(translation[axis]) <= parameter_epsilon) {
			    translation[axis] = 0.0;
			    continue;
			}
			if (!surface->IsClosed(axis) || !(period > ON_ZERO_TOLERANCE)) {
			    integral_periods = false;
			    break;
			}
			const double periods = std::round(translation[axis] / period);
			if (fabs(periods) < 0.5 || fabs(translation[axis] - periods * period) >
				parameter_epsilon) {
			    integral_periods = false;
			    break;
			}
			translation[axis] = periods * period;
			shifted = true;
		    }
		    ON_Curve *translated = integral_periods && shifted ?
			movable->DuplicateCurve() : NULL;
		    ON_Xform transform(ON_Xform::IdentityTransformation);
		    transform.m_xform[0][3] = translation.x;
		    transform.m_xform[1][3] = translation.y;
		    std::string translation_failure;
		    const bool translated_valid = translated &&
			translated->Transform(transform) && translated->ChangeDimension(2) &&
			translated->IsValid() &&
			(movable == previous ? translated->PointAtEnd() :
			 translated->PointAtStart()).DistanceTo(seam_join) <=
			    std::max(ON_ZERO_TOLERANCE, surface->Domain(0).Length() * 1.0e-10) &&
			validate_periodic_trim_translation(surface, *movable,
			    *translated, &translation_failure);
		    if (translated_valid) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*movable, c2_index)) {
			    brep->SetTrimIsoFlags(*movable);
			    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
				"shifted an exact non-seam pcurve onto an adjacent periodic branch");
			    changed = true;
			    break;
			}
			if (c2_index < 0)
			    delete translated;
		    } else {
			delete translated;
		    }

		    /* A non-seam edge can meet a periodic seam at a parameter that is
		     * lift-equivalent, but not an integral-period translation of its
		     * supplied endpoint.  Seed an exact edge pullback at the seam's UV
		     * endpoint so the regenerated curve leaves the correct side of the
		     * domain and remains continuous with the loop. */
		    ON_BrepEdge *movable_edge = movable->Edge();
		    ON_NurbsCurve movable_edge_nurbs;
		    std::string seeded_failure;
		    ON_3dPoint other_seam_join = ON_3dPoint::UnsetPoint;
		    const ON_3dPoint *required_start = movable == next ?
			&seam_join : NULL;
		    const ON_3dPoint *required_end = movable == previous ?
			&seam_join : NULL;
		    if (movable == next) {
			ON_BrepTrim *following = loop.Trim(
			    (lti + 2) % loop.TrimCount());
			if (following && following->m_type == ON_BrepTrim::seam &&
				movable->m_vi[1] >= 0 &&
				movable->m_vi[1] == following->m_vi[0] &&
				movable->m_vi[1] < brep->m_V.Count()) {
			    other_seam_join = following->PointAtStart();
			    const ON_3dPoint other_lift = surface->PointAt(
				other_seam_join.x, other_seam_join.y);
			    if (other_lift.IsValid() && other_lift.DistanceTo(
				    brep->m_V[movable->m_vi[1]].point) <=
					LocalUnits::tolerance)
				required_end = &other_seam_join;
			}
		    } else {
			ON_BrepTrim *preceding = loop.Trim((lti +
			    loop.TrimCount() - 1) % loop.TrimCount());
			if (preceding && preceding->m_type == ON_BrepTrim::seam &&
				movable->m_vi[0] >= 0 &&
				preceding->m_vi[1] == movable->m_vi[0] &&
				movable->m_vi[0] < brep->m_V.Count()) {
			    other_seam_join = preceding->PointAtEnd();
			    const ON_3dPoint other_lift = surface->PointAt(
				other_seam_join.x, other_seam_join.y);
			    if (other_lift.IsValid() && other_lift.DistanceTo(
				    brep->m_V[movable->m_vi[0]].point) <=
					LocalUnits::tolerance)
				required_start = &other_seam_join;
			}
		    }
		    const bool seeded = movable_edge &&
			movable_edge->GetNurbForm(movable_edge_nurbs) &&
			regenerate_trim_polyline(brep, *movable, surface,
			    movable_edge_nurbs, LocalUnits::tolerance,
			    &seeded_failure, NULL, required_start, required_end);
		    const ON_3dPoint seeded_join = seeded ?
			(movable == previous ? movable->PointAtEnd() :
			 movable->PointAtStart()) : ON_3dPoint::UnsetPoint;
		    if (seeded && seeded_join.IsValid() &&
			    seeded_join.DistanceTo(seam_join) <= ON_ZERO_TOLERANCE) {
			wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			    "regenerated a non-seam pcurve from an exact periodic seam endpoint");
			changed = true;
			break;
		    }
		}
		if (changed)
		    break;

		ON_3dPoint aligned_next = next_uv;
		for (int axis = 0; axis < 2; ++axis) {
		    if (!surface->IsClosed(axis))
			continue;
		    const double period = surface->Domain(axis).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint candidate = aligned_next;
		    candidate[axis] += round((previous_uv[axis] - candidate[axis]) /
			period) * period;
		    const ON_3dPoint candidate_lift = surface->PointAt(candidate.x,
			candidate.y);
		    if (candidate_lift.IsValid() && candidate_lift.DistanceTo(next_lift) <=
			    std::max(ON_ZERO_TOLERANCE * kNumericalToleranceScale,
				LocalUnits::tolerance * 1.0e-8))
			aligned_next = candidate;
		}
		ON_3dPoint common = 0.5 * (previous_uv + aligned_next);
		for (int axis = 0; axis < 2; ++axis) {
		    const bool previous_boundary = boundary_axis(previous->m_iso, axis);
		    const bool next_boundary = boundary_axis(next->m_iso, axis);
		    if (previous_boundary != next_boundary)
			common[axis] = previous_boundary ? previous_uv[axis] : aligned_next[axis];
		}
		if ((previous->m_type == ON_BrepTrim::seam) !=
			(next->m_type == ON_BrepTrim::seam))
		    common = previous->m_type == ON_BrepTrim::seam ?
			previous_uv : next_uv;
		const ON_3dPoint common_lift = surface->PointAt(common.x, common.y);
		if (!common_lift.IsValid() ||
			common_lift.DistanceTo(vertex) > LocalUnits::tolerance) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop " << li
			    << " common endpoint rejected for trims "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " at " << common.x << ':' << common.y << " vertex distance="
			    << common_lift.DistanceTo(vertex) << std::endl;
		    continue;
		}

		bool change_previous = previous_uv.DistanceTo(common) >
		    ON_ZERO_TOLERANCE;
		bool change_next = next_uv.DistanceTo(common) > ON_ZERO_TOLERANCE;
		ON_Curve *previous_candidate = change_previous ?
		    previous->DuplicateCurve() : NULL;
		ON_Curve *next_candidate = change_next ? next->DuplicateCurve() : NULL;
		const bool previous_constructed = !change_previous ||
		    (previous_candidate && previous_candidate->SetEndPoint(common) &&
		     previous_candidate->ChangeDimension(2) &&
		     previous_candidate->IsValid());
		const bool next_constructed = !change_next ||
		    (next_candidate && next_candidate->SetStartPoint(common) &&
		     next_candidate->ChangeDimension(2) && next_candidate->IsValid());
		if (!change_previous && !change_next) {
		    delete previous_candidate;
		    delete next_candidate;
		    continue;
		}

		const auto validates = [brep, surface](
		    const ON_BrepTrim &original,
		    const ON_Curve &candidate, std::string *failure) {
		    if (failure)
			failure->clear();
		    const ON_Interval domain = original.Domain();
		    if (original.m_type == ON_BrepTrim::seam) {
			const ON_Surface::ISO derived = surface->IsIsoparametric(
			    candidate, &domain);
			if (derived != original.m_iso) {
			    if (failure)
				*failure = "seam iso classification changed";
			    return false;
			}
		    }
		    ON_NurbsCurve edge_nurbs;
		    const ON_BrepEdge *edge = original.Edge();
		    if (edge && !edge->GetNurbForm(edge_nurbs)) {
			if (failure)
			    *failure = "edge NURBS conversion failed";
			return false;
		    }
		    const int validation_spans = std::max(original.SpanCount(),
			candidate.SpanCount());
		    const int samples = std::min(4096,
			std::max(64, validation_spans * 8));
		    for (int sample = 0; sample <= samples; ++sample) {
			const double fraction = static_cast<double>(sample) / samples;
			const double parameter = domain.ParameterAt(fraction);
			const ON_3dPoint original_uv = original.PointAt(parameter);
			const ON_3dPoint candidate_uv = candidate.PointAt(parameter);
			const ON_3dPoint original_lift = surface->PointAt(
			    original_uv.x, original_uv.y);
			const ON_3dPoint candidate_lift = surface->PointAt(
			    candidate_uv.x, candidate_uv.y);
			if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
				original_lift.DistanceTo(candidate_lift) >
				    LocalUnits::tolerance) {
			    if (failure)
				*failure = "surface lift changed at sample " +
				    std::to_string(sample) + " by " +
				    std::to_string(original_lift.DistanceTo(candidate_lift));
			    return false;
			}
			if (edge) {
			    double edge_parameter = 0.0;
			    const bool closest = ON_NurbsCurve_GetClosestPoint(&edge_parameter,
				&edge_nurbs, candidate_lift);
			    const ON_3dPoint corresponding_edge_point = edge->PointAt(
				edge->Domain().ParameterAt(original.m_bRev3d ?
				    1.0 - fraction : fraction));
			    double edge_distance = corresponding_edge_point.IsValid() ?
				candidate_lift.DistanceTo(corresponding_edge_point) : DBL_MAX;
			    if (closest)
				edge_distance = std::min(edge_distance,
				    candidate_lift.DistanceTo(edge_nurbs.PointAt(edge_parameter)));
			    if (edge_distance > LocalUnits::tolerance) {
				if (failure)
				    *failure = "edge distance at sample " +
					std::to_string(sample) + " was " +
					std::to_string(edge_distance);
				return false;
			    }
			} else if (original.m_vi[0] >= 0 &&
				original.m_vi[0] < brep->m_V.Count() &&
				candidate_lift.DistanceTo(
				    brep->m_V[original.m_vi[0]].point) >
				    LocalUnits::tolerance) {
			    if (failure)
				*failure = "singular trim lift left its vertex";
			    return false;
			}
		    }
		    return true;
		};
		const auto localized_candidate = [](const ON_BrepTrim &original,
		    const ON_3dPoint &endpoint, bool replace_start,
		    int sample_count) -> ON_Curve * {
		    if (sample_count < 2)
			return NULL;
		    const ON_Interval domain = original.Domain();
		    ON_3dPointArray points;
		    ON_SimpleArray<double> parameters;
		    points.Reserve(sample_count + 1);
		    parameters.Reserve(sample_count + 1);
		    for (int sample = 0; sample <= sample_count; ++sample) {
			const double fraction = static_cast<double>(sample) / sample_count;
			const double parameter = domain.ParameterAt(fraction);
			ON_3dPoint point = original.PointAt(parameter);
			if ((replace_start && sample == 0) ||
				(!replace_start && sample == sample_count))
			    point = endpoint;
			point.z = 0.0;
			if (points.Count() > 0 &&
				point.DistanceTo(points[points.Count() - 1]) <=
				    ON_ZERO_TOLERANCE) {
			    if (sample == sample_count) {
				points[points.Count() - 1] = point;
				parameters[parameters.Count() - 1] = parameter;
			    }
			    continue;
			}
			points.Append(point);
			parameters.Append(parameter);
		    }
		    if (points.Count() < 2 || points.Count() != parameters.Count())
			return NULL;
		    parameters[0] = domain.Min();
		    parameters[parameters.Count() - 1] = domain.Max();
		    ON_PolylineCurve *candidate = new ON_PolylineCurve(points, parameters);
		    if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
			delete candidate;
			return NULL;
		    }
		    return candidate;
		};
		std::string previous_failure = previous_constructed ? std::string() :
		    "endpoint construction failed";
		std::string next_failure = next_constructed ? std::string() :
		    "endpoint construction failed";
		bool previous_valid = previous_constructed && (!change_previous ||
		    validates(*previous, *previous_candidate, &previous_failure));
		bool next_valid = next_constructed && (!change_next ||
		    validates(*next, *next_candidate, &next_failure));
		if (!previous_valid || !next_valid) {
		    /* A midpoint is not always the best parameter-space join near a
		     * singularity or strongly nonlinear edge.  Prefer either supplied
		     * endpoint when moving only the other curve remains within the exact
		     * surface and edge bounds. */
		    for (int endpoint_choice = 0;
			    (!previous_valid || !next_valid) && endpoint_choice < 2;
			    ++endpoint_choice) {
			const ON_3dPoint alternative_common = endpoint_choice == 0 ?
			    previous_uv : next_uv;
			if (alternative_common.DistanceTo(common) <= ON_ZERO_TOLERANCE)
			    continue;
			const bool alternative_change_previous =
			    previous_uv.DistanceTo(alternative_common) > ON_ZERO_TOLERANCE;
			const bool alternative_change_next =
			    next_uv.DistanceTo(alternative_common) > ON_ZERO_TOLERANCE;
			ON_Curve *previous_alternative = alternative_change_previous ?
			    previous->DuplicateCurve() : NULL;
			ON_Curve *next_alternative = alternative_change_next ?
			    next->DuplicateCurve() : NULL;
			const bool alternative_previous_constructed =
			    !alternative_change_previous ||
			    (previous_alternative &&
			     previous_alternative->SetEndPoint(alternative_common) &&
			     previous_alternative->ChangeDimension(2) &&
			     previous_alternative->IsValid());
			const bool alternative_next_constructed =
			    !alternative_change_next ||
			    (next_alternative &&
			     next_alternative->SetStartPoint(alternative_common) &&
			     next_alternative->ChangeDimension(2) &&
			     next_alternative->IsValid());
			std::string previous_alternative_failure;
			std::string next_alternative_failure;
			bool previous_alternative_valid =
			    alternative_previous_constructed &&
			    (!alternative_change_previous || validates(*previous,
				*previous_alternative, &previous_alternative_failure));
			bool next_alternative_valid =
			    alternative_next_constructed &&
			    (!alternative_change_next || validates(*next,
				*next_alternative, &next_alternative_failure));
			for (int endpoint_samples = 64;
				(!previous_alternative_valid || !next_alternative_valid) &&
				endpoint_samples <= 4096; endpoint_samples *= 2) {
			    delete previous_alternative;
			    delete next_alternative;
			    previous_alternative = alternative_change_previous ?
				localized_candidate(*previous, alternative_common, false,
				    endpoint_samples) : NULL;
			    next_alternative = alternative_change_next ?
				localized_candidate(*next, alternative_common, true,
				    endpoint_samples) : NULL;
			    previous_alternative_failure.clear();
			    next_alternative_failure.clear();
			    previous_alternative_valid = !alternative_change_previous ||
				(previous_alternative && validates(*previous,
				    *previous_alternative, &previous_alternative_failure));
			    next_alternative_valid = !alternative_change_next ||
				(next_alternative && validates(*next, *next_alternative,
				    &next_alternative_failure));
			}
			if (previous_alternative_valid && next_alternative_valid &&
				(alternative_change_previous || alternative_change_next)) {
			    delete previous_candidate;
			    delete next_candidate;
			    previous_candidate = previous_alternative;
			    next_candidate = next_alternative;
			    common = alternative_common;
			    change_previous = alternative_change_previous;
			    change_next = alternative_change_next;
			    previous_valid = true;
			    next_valid = true;
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id << ": loop "
				    << li << " selected existing endpoint for trims "
				    << previous->m_trim_index << '/' << next->m_trim_index
				    << std::endl;
			} else {
			    delete previous_alternative;
			    delete next_alternative;
			    previous_failure = previous_alternative_failure;
			    next_failure = next_alternative_failure;
			}
		    }
		    bool localized = previous_valid && next_valid;
		    for (int samples = 64; !localized && samples <= 4096; samples *= 2) {
			ON_Curve *previous_alternative = change_previous ?
			    localized_candidate(*previous, common, false, samples) : NULL;
			ON_Curve *next_alternative = change_next ?
			    localized_candidate(*next, common, true, samples) : NULL;
			std::string previous_alternative_failure;
			std::string next_alternative_failure;
			previous_valid = !change_previous || (previous_alternative &&
			    validates(*previous, *previous_alternative,
				&previous_alternative_failure));
			next_valid = !change_next || (next_alternative && validates(*next,
			    *next_alternative, &next_alternative_failure));
			if (previous_valid && next_valid) {
			    delete previous_candidate;
			    delete next_candidate;
			    previous_candidate = previous_alternative;
			    next_candidate = next_alternative;
			    localized = true;
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id << ": loop "
				    << li << " localized endpoint repair for trims "
				    << previous->m_trim_index << '/' << next->m_trim_index
				    << " with " << samples << " pcurve samples" << std::endl;
			} else {
			    delete previous_alternative;
			    delete next_alternative;
			    previous_failure = previous_alternative_failure;
			    next_failure = next_alternative_failure;
			}
		    }
		    if (!localized) {
			/* Endpoint repair must not preserve a supplied pcurve that is
			 * already farther from its exact 3D edge than the model
			 * uncertainty.  Regenerate that non-seam pcurve from the edge,
			 * retaining its shared endpoints, and retry this join on the next
			 * bounded pass. */
			ON_BrepTrim *invalid_trim = NULL;
			if (!previous_valid && previous->m_type != ON_BrepTrim::seam &&
				previous_failure.compare(0, 13, "edge distance") == 0)
			    invalid_trim = previous;
			else if (!next_valid && next->m_type != ON_BrepTrim::seam &&
				next_failure.compare(0, 13, "edge distance") == 0)
			    invalid_trim = next;
			bool regenerated = false;
			std::string regeneration_failure;
			if (invalid_trim && attempted_edge_regeneration.insert(
				invalid_trim->m_trim_index).second) {
			    const ON_BrepEdge *invalid_edge = invalid_trim->Edge();
			    ON_NurbsCurve edge_nurbs;
			    if (invalid_edge && invalid_edge->GetNurbForm(edge_nurbs))
				regenerated = regenerate_trim_polyline(brep, *invalid_trim,
				    surface, edge_nurbs, LocalUnits::tolerance,
				    &regeneration_failure);
			}
			if (regenerated) {
			    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
				"regenerated an invalid pcurve from the exact edge");
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id << ": loop "
				    << li << " regenerated trim "
				    << invalid_trim->m_trim_index
				    << " from its exact edge before endpoint repair" << std::endl;
			    delete previous_candidate;
			    delete next_candidate;
			    changed = true;
			    break;
			}
			if (wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id << ": loop "
				<< li << " endpoint candidate "
				<< previous->m_trim_index << '/' << next->m_trim_index
				<< " rejected: previous="
				<< (previous_valid ? "valid" : previous_failure)
				<< ", next="
				<< (next_valid ? "valid" : next_failure)
				<< (regeneration_failure.empty() ? "" :
				    ", exact-edge regeneration=" + regeneration_failure)
				<< std::endl;
			delete previous_candidate;
			delete next_candidate;
			continue;
		    }
		}
		const int previous_c2 = previous_candidate ?
		    brep->AddTrimCurve(previous_candidate) : -1;
		const int next_c2 = next_candidate ? brep->AddTrimCurve(next_candidate) : -1;
		const bool previous_installed = !previous_candidate ||
		    (previous_c2 >= 0 && brep->SetTrimCurve(*previous, previous_c2));
		const bool next_installed = !next_candidate ||
		    (next_c2 >= 0 && brep->SetTrimCurve(*next, next_c2));
		if (!previous_installed || !next_installed) {
		    if (previous_candidate && previous_c2 < 0)
			delete previous_candidate;
		    if (next_candidate && next_c2 < 0)
			delete next_candidate;
		    continue;
		}
		if (previous_candidate)
		    brep->SetTrimIsoFlags(*previous);
		if (next_candidate)
		    brep->SetTrimIsoFlags(*next);
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    "snapped adjacent pcurve endpoints within model tolerance");
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id << ": snapped loop "
			<< li << " trim endpoints " << previous->m_trim_index << '/'
			<< next->m_trim_index << " within tolerance "
			<< LocalUnits::tolerance << std::endl;
		changed = true;
		break;
	    }
	}
	if (!changed)
	    break;
    }
}


static void
repair_zero_length_boundary_edges(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return;

    const int repair_budget = brep->m_E.Count();
    for (int repair = 0; repair < repair_budget; ++repair) {
	bool changed = false;
	for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	    ON_BrepTrim &trim = brep->m_T[ti];
	    if (trim.m_type != ON_BrepTrim::boundary || trim.m_ei < 0 ||
		    trim.m_ei >= brep->m_E.Count() || trim.m_li < 0 ||
		    trim.m_li >= brep->m_L.Count())
		continue;
	    ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	    ON_BrepLoop &loop = brep->m_L[trim.m_li];
	    if (edge.m_ti.Count() != 1 || edge.m_vi[0] < 0 ||
		    edge.m_vi[0] >= brep->m_V.Count() || edge.m_vi[1] < 0 ||
		    edge.m_vi[1] >= brep->m_V.Count() || loop.TrimCount() < 2)
		continue;
	    int trim_offset = -1;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		if (loop.m_ti[lti] == ti) {
		    trim_offset = lti;
		    break;
		}
	    }
	    if (trim_offset < 0)
		continue;
	    ON_BrepTrim *previous = loop.Trim(
		(trim_offset + loop.TrimCount() - 1) % loop.TrimCount());
	    ON_BrepTrim *next = loop.Trim((trim_offset + 1) % loop.TrimCount());
	    if (!previous || !next)
		continue;
	    const ON_Curve *curve = edge.EdgeCurveOf();
	    if (!curve)
		continue;
	    const ON_Interval domain = edge.Domain();
	    const ON_3dPoint vertex = brep->m_V[edge.m_vi[0]].point;
	    bool zero_length = true;
	    double maximum_radius = 0.0;
	    for (int sample = 0; sample <= 64; ++sample) {
		const ON_3dPoint point = edge.PointAt(domain.ParameterAt(
		    static_cast<double>(sample) / 64.0));
		const double radius = point.IsValid() ? point.DistanceTo(vertex) : DBL_MAX;
		maximum_radius = std::max(maximum_radius, radius);
		if (!point.IsValid() || radius > LocalUnits::tolerance) {
		    zero_length = false;
		    break;
		}
	    }
	    double exposed_gap = previous->PointAtEnd().DistanceTo(
		next->PointAtStart());
	    if (wrapper->Verbose() && (edge.m_vi[0] == edge.m_vi[1] ||
		    brep->m_V[edge.m_vi[0]].point.DistanceTo(
			brep->m_V[edge.m_vi[1]].point) <= LocalUnits::tolerance))
		std::cerr << entity_type << " #" << entity_id
		    << ": zero-length boundary candidate trim " << ti
		    << "/STEP edge " << edge.m_edge_user.i << " vertices "
		    << edge.m_vi[0] << '/' << edge.m_vi[1] << " surrounding "
		    << previous->m_vi[1] << '/' << next->m_vi[0]
		    << " p-gap " << exposed_gap << " max-radius " << maximum_radius
		    << std::endl;
	    if (edge.m_vi[0] != edge.m_vi[1] ||
		    previous->m_vi[1] != next->m_vi[0])
		continue;
	    if (!zero_length)
		continue;
	    if (exposed_gap > ON_ZERO_TOLERANCE) {
		const ON_BrepFace *face = loop.Face();
		const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
		if (!surface)
		    continue;
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		const ON_3dPoint common_uv = 0.5 * (previous_uv + next_uv);
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		const ON_3dPoint common_lift = surface->PointAt(common_uv.x, common_uv.y);
		if (!previous_lift.IsValid() || !next_lift.IsValid() ||
			!common_lift.IsValid() ||
			previous_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
			next_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
			common_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
			previous_lift.DistanceTo(next_lift) > LocalUnits::tolerance)
		    continue;
		if (!brep->MatchTrimEnds(*previous, *next))
		    continue;
		exposed_gap = previous->PointAtEnd().DistanceTo(next->PointAtStart());
		if (exposed_gap > ON_ZERO_TOLERANCE)
		    continue;
	    }
	    const int source_edge = edge.m_edge_user.i;
	    brep->DeleteTrim(trim, true);
	    if (!brep->Compact())
		return;
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"removed a zero-length boundary edge within model tolerance");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": removed zero-length boundary STEP edge " << source_edge
		    << " within tolerance " << LocalUnits::tolerance << std::endl;
	    changed = true;
	    break;
	}
	if (!changed)
	    break;
    }
}


static void
repair_final_closed_trim_orientations(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper)
	return;

    /* The final adjacent-endpoint pass can replace the endpoint control point
     * of a closed pcurve after the main orientation pass.  Verify the exact
     * endpoint tangents again at that point.  A flag reversal is allowed only
     * when both endpoint tangents strongly prove the same correction;
     * ambiguous or mixed directions are left for structural validation. */
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
		trim.m_vi[0] != trim.m_vi[1])
	    continue;
	ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	if (edge.m_vi[0] != edge.m_vi[1])
	    continue;
	const int face_index = brep->m_L[trim.m_li].m_fi;
	if (face_index < 0 || face_index >= brep->m_F.Count())
	    continue;
	const ON_Surface *surface = brep->m_F[face_index].SurfaceOf();
	if (!surface)
	    continue;

	double alignment[2];
	closed_trim_endpoint_alignments(trim, edge, surface, alignment);
	if (alignment[0] >= 0.0 && alignment[1] >= 0.0)
	    continue;
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": final closed trim "
		<< ti << " endpoint dots=" << alignment[0] << ',' << alignment[1]
		<< std::endl;

	if (alignment[0] <= -0.9 && alignment[1] <= -0.9) {
	    trim.m_bRev3d = !trim.m_bRev3d;
	    trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
	    trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"corrected a closed-edge trim orientation after endpoint repair");
	    continue;
	}

	const ON_3dPoint *required_start = NULL;
	const ON_3dPoint *required_end = NULL;
	ON_3dPoint adjacent_start = ON_3dPoint::UnsetPoint;
	ON_3dPoint adjacent_end = ON_3dPoint::UnsetPoint;
	ON_BrepLoop &loop = brep->m_L[trim.m_li];
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    if (loop.m_ti[lti] != ti)
		continue;
	    const ON_BrepTrim *previous = loop.Trim(
		(lti + loop.TrimCount() - 1) % loop.TrimCount());
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (previous && previous->m_vi[1] == trim.m_vi[0]) {
		adjacent_start = previous->PointAtEnd();
		required_start = &adjacent_start;
	    }
	    if (next && trim.m_vi[1] == next->m_vi[0]) {
		adjacent_end = next->PointAtStart();
		required_end = &adjacent_end;
	    }
	    break;
	}

	ON_NurbsCurve edge_nurbs;
	std::string regeneration_failure;
	if (edge.GetNurbForm(edge_nurbs) &&
		regenerate_trim_polyline(brep, trim, surface, edge_nurbs,
		    LocalUnits::tolerance, &regeneration_failure, NULL,
		    required_start, required_end, true)) {
	    closed_trim_endpoint_alignments(trim, edge, surface, alignment);
	    if (alignment[0] <= -0.9 && alignment[1] <= -0.9) {
		trim.m_bRev3d = !trim.m_bRev3d;
		trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
		trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
		wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		    "corrected a regenerated closed-edge trim orientation");
	    }
	    if (alignment[0] >= 0.0 && alignment[1] >= 0.0)
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "regenerated a closed-edge pcurve after endpoint repair");
	} else if (wrapper->Verbose() && !regeneration_failure.empty()) {
	    std::cerr << entity_type << " #" << entity_id << ": final closed trim "
		<< ti << " pcurve regeneration rejected: "
		<< regeneration_failure << std::endl;
	}
    }
}


static void
repair_closed_trim_orientations(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	bool allow_surface_alignment = false)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return;

    /* Moving a periodic surface seam is exact, but it can disturb an already
     * valid choice of parameter branches elsewhere on the same surface.  Keep
     * it out of the ordinary repair pass and retry with seam alignment only
     * when the bounded repairs below still leave structurally invalid
     * topology. */
    std::unique_ptr<ON_Brep> retry_source;
    if (!allow_surface_alignment)
	retry_source.reset(new ON_Brep(*brep));

    std::vector<ON_BrepLoop::TYPE> expected_loop_types;
    expected_loop_types.reserve(brep->m_L.Count());
    for (int li = 0; li < brep->m_L.Count(); ++li)
	expected_loop_types.push_back(brep->m_L[li].m_type);

    std::vector<int> aligned_surface_loops;
    repair_bounded_seam_isos(brep, wrapper, entity_id, entity_type,
	allow_surface_alignment, &aligned_surface_loops);
    repair_adjacent_trim_vertices(brep, wrapper, entity_id, entity_type);
    repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type);
    repair_paired_seam_boundaries(brep, wrapper, entity_id, entity_type,
	&aligned_surface_loops);
    repair_missing_singular_trims(brep, wrapper, entity_id, entity_type);
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
    repair_zero_length_boundary_edges(brep, wrapper, entity_id, entity_type);
    if (!aligned_surface_loops.empty()) {
	repair_aligned_surface_loop_branches(brep, wrapper, entity_id, entity_type,
	    aligned_surface_loops);
	repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
    }

    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count() ||
	    trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
	    trim.m_vi[0] != trim.m_vi[1])
	    continue;
	const ON_BrepEdge &edge = brep->m_E[trim.m_ei];
	if (edge.m_vi[0] != edge.m_vi[1])
	    continue;
	const int face_index = brep->m_L[trim.m_li].m_fi;
	if (face_index < 0 || face_index >= brep->m_F.Count())
	    continue;
	const ON_Surface *surface = brep->m_F[face_index].SurfaceOf();
	if (!surface)
	    continue;

	ON_NurbsCurve edge_nurbs;
	if (!edge.GetNurbForm(edge_nurbs))
	    continue;
	const ON_Interval trim_domain = trim.Domain();
	int matching_samples = 0;
	int opposing_samples = 0;
	for (int sample = 1; sample < 16; ++sample) {
	    const double normalized = static_cast<double>(sample) / 16.0;
	    ON_3dPoint uv;
	    ON_3dVector uv_tangent;
	    if (!trim.Ev1Der(trim_domain.ParameterAt(normalized), uv, uv_tangent))
		continue;
	    ON_3dPoint lifted_point;
	    ON_3dVector du, dv;
	    if (!surface->Ev1Der(uv.x, uv.y, lifted_point, du, dv))
		continue;
	    ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	    if (!lifted_tangent.Unitize())
		continue;
	    double edge_parameter = 0.0;
	    if (!ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs, lifted_point))
		continue;
	    if (lifted_point.DistanceTo(edge_nurbs.PointAt(edge_parameter)) >
		    LocalUnits::tolerance)
		continue;
	    ON_3dVector edge_tangent = edge_nurbs.TangentAt(edge_parameter);
	    if (!edge_tangent.Unitize())
		continue;
	    double alignment = lifted_tangent * edge_tangent;
	    if (trim.m_bRev3d) alignment = -alignment;
	    if (alignment > 0.5)
		++matching_samples;
	    else if (alignment < -0.5)
		++opposing_samples;
	}
	double endpoint_alignment[2];
	closed_trim_endpoint_alignments(trim, edge, surface, endpoint_alignment);
	if (endpoint_alignment[0] < 0.0 && endpoint_alignment[1] < 0.0 &&
		trim.PointAtStart().DistanceTo(trim.PointAtEnd()) <= ON_ZERO_TOLERANCE) {
	    ON_Curve *reversed = trim.DuplicateCurve();
	    bool valid_reversal = reversed && reversed->Reverse() &&
		reversed->ChangeDimension(2) && reversed->IsValid() &&
		reversed->PointAtStart().DistanceTo(trim.PointAtStart()) <=
		    ON_ZERO_TOLERANCE &&
		reversed->PointAtEnd().DistanceTo(trim.PointAtEnd()) <=
		    ON_ZERO_TOLERANCE;
	    const ON_Interval reversed_domain = reversed ? reversed->Domain() :
		ON_Interval::EmptyInterval;
	    for (int end = 0; valid_reversal && end < 2; ++end) {
		ON_3dPoint uv, lifted_point;
		ON_3dVector uv_tangent, du, dv;
		if (!reversed->Ev1Der(reversed_domain[end], uv, uv_tangent) ||
			!surface->Ev1Der(uv.x, uv.y, lifted_point, du, dv)) {
		    valid_reversal = false;
		    break;
		}
		ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
		ON_3dVector edge_tangent = edge.TangentAt(edge.Domain()[
		    trim.m_bRev3d ? 1 - end : end]);
		if (!lifted_tangent.Unitize() || !edge_tangent.Unitize()) {
		    valid_reversal = false;
		    break;
		}
		double alignment = lifted_tangent * edge_tangent;
		if (trim.m_bRev3d)
		    alignment = -alignment;
		valid_reversal = alignment >= 0.0;
	    }
	    for (int sample = 0; valid_reversal &&
		    sample <= kDenseValidationSegments; ++sample) {
		const double fraction = static_cast<double>(sample) /
		    kDenseValidationSegments;
		const ON_3dPoint uv = reversed->PointAt(
		    reversed_domain.ParameterAt(fraction));
		const ON_3dPoint lifted = surface->PointAt(uv.x, uv.y);
		double edge_parameter = 0.0;
		valid_reversal = lifted.IsValid() &&
		    ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs, lifted) &&
		    lifted.DistanceTo(edge_nurbs.PointAt(edge_parameter)) <=
			LocalUnits::tolerance;
	    }
	    if (valid_reversal) {
		const int c2_index = brep->AddTrimCurve(reversed);
		if (c2_index >= 0 && brep->SetTrimCurve(trim, c2_index)) {
		    brep->SetTrimIsoFlags(trim);
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"reversed a closed pcurve whose endpoint tangents opposed its edge use");
		    continue;
		}
		if (c2_index < 0)
		    delete reversed;
	    } else {
		delete reversed;
	    }
	}
	if (endpoint_alignment[0] < -0.9 && endpoint_alignment[1] < -0.9) {
	    trim.m_bRev3d = !trim.m_bRev3d;
	    trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
	    trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"corrected a closed-edge trim orientation proven at both endpoints");
	    continue;
	}
	std::string regeneration_failure;
	if ((endpoint_alignment[0] < 0.0 || endpoint_alignment[1] < 0.0) &&
		regenerate_trim_polyline(brep, trim, surface, edge_nurbs,
		    LocalUnits::tolerance, &regeneration_failure)) {
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"regenerated a closed-edge pcurve with consistent endpoint tangents");
	    continue;
	}
	if (wrapper->Verbose() && !regeneration_failure.empty())
	    std::cerr << entity_type << " #" << entity_id << ": closed trim " << ti
		<< " pcurve regeneration rejected: " << regeneration_failure << std::endl;
	/* The interior correspondence must demonstrate one direction consistently.
	 * Mixed or weak samples indicate a kink, self-intersection, or ambiguous
	 * closest point and are deliberately left for validation to reject. */
	if (wrapper->Verbose() && (endpoint_alignment[0] < 0.0 ||
		endpoint_alignment[1] < 0.0 || opposing_samples > 0 ||
		matching_samples + opposing_samples < 5)) {
	    std::cerr << entity_type << " #" << entity_id << ": closed trim " << ti
		<< " (loop " << trim.m_li << ", edge " << trim.m_ei
		<< ", reversed " << (trim.m_bRev3d ? "yes" : "no")
		<< ", loop trims " << (trim.Loop() ? trim.Loop()->TrimCount() : 0)
		<< ") orientation samples matching=" << matching_samples
		<< ", opposing=" << opposing_samples
		<< ", endpoint dots=" << endpoint_alignment[0] << ","
		<< endpoint_alignment[1] << ", pcurve closure="
		<< trim.PointAtStart().DistanceTo(trim.PointAtEnd()) << std::endl;
	}
	if (opposing_samples >= 5 && matching_samples == 0) {
	    trim.m_bRev3d = !trim.m_bRev3d;
	    trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
	    trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"corrected closed-edge trim orientation");
	}
    }

    /* Closed-edge regeneration above can replace a pcurve after the ordinary
     * endpoint pass.  Reclose only the newly exposed, model-space exact joins
     * before recomputing derived topology state. */
    repair_missing_singular_trims(brep, wrapper, entity_id, entity_type);
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);

    /* Surface-seam alignment and whole-period loop unwrapping can expose a
     * seam pair that was valid when first installed but no longer agrees with
     * the final shared surface parameterization.  Reconcile exact seam pairs
     * once more without permitting another surface seam move, then re-run the
     * bounded open-curve and join checks against that stable surface. */
    repair_seam_pair_from_exact_edge(brep, wrapper, entity_id, entity_type);
    repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type);
    repair_paired_seam_boundaries(brep, wrapper, entity_id, entity_type, NULL);
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);

	/* Curve replacement and orientation repair invalidate derived p-space
	 * state.  Refresh it once, after all bounded repairs, using the same
	 * routines ON_Brep::IsValid() uses. */
	brep->SetTolerancesBoxesAndFlags(false, false, false, false,
	    true, true, true, true);
	bool loop_orientation_changed = false;
	for (int li = 0; li < brep->m_L.Count() &&
		static_cast<size_t>(li) < expected_loop_types.size(); ++li) {
	    const ON_BrepLoop::TYPE expected = expected_loop_types[li];
	    ON_BrepLoop &loop = brep->m_L[li];
	    if ((expected != ON_BrepLoop::outer && expected != ON_BrepLoop::inner) ||
		(loop.m_type != ON_BrepLoop::outer && loop.m_type != ON_BrepLoop::inner) ||
		expected == loop.m_type)
		continue;
	    brep->FlipLoop(loop);
	    loop_orientation_changed = true;
	    wrapper->RecordRepair(entity_id, entity_type, "loop_orientation",
		"restored a loop orientation changed by exact pcurve regeneration");
	}
	if (loop_orientation_changed) {
	    brep->SetTolerancesBoxesAndFlags(false, false, false, false,
		true, true, true, true);
	    for (int fi = 0; fi < brep->m_F.Count(); ++fi)
		brep->SortFaceLoops(brep->m_F[fi]);
	}
	/* FlipLoop reverses every pcurve in a restored loop.  Perform the final
	 * closed-edge tangent check only after those reversals, and then refresh
	 * the derived trim state once more. */
	repair_final_closed_trim_orientations(brep, wrapper, entity_id, entity_type);
	brep->SetTolerancesBoxesAndFlags(false, false, false, false,
	    true, true, true, true);
	if (!allow_surface_alignment && retry_source) {
	    ON_wString validation_messages;
	    ON_TextLog validation_log(validation_messages);
	    if (!brep->IsValid(&validation_log)) {
		std::unique_ptr<ON_Brep> bounded_repairs(new ON_Brep(*brep));
		*brep = *retry_source;
		repair_closed_trim_orientations(brep, wrapper, entity_id,
		    entity_type, true);
		ON_wString aligned_messages;
		ON_TextLog aligned_log(aligned_messages);
		if (brep->IsValid(&aligned_log)) {
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"selected a valid retry with exact periodic surface seam alignment");
		    return;
		}
		/* Seam alignment is exact, but it is a whole-surface operation and can
		 * disturb unrelated periodic branches.  Never discard the ordinary
		 * bounded repairs unless the retry actually validates. */
		*brep = *bounded_repairs;
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": periodic surface seam retry remained invalid; "
			<< "retaining the bounded pcurve repairs" << std::endl;
		return;
	    }
	}
	if (wrapper->Verbose()) {
	    int pspace_gap_diagnostics = 0;
	    for (int li = 0; li < brep->m_L.Count() && pspace_gap_diagnostics < 16; ++li) {
		const ON_BrepLoop &loop = brep->m_L[li];
		const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
		if (!surface)
		    continue;
		for (int lti = 0; lti < loop.TrimCount() && pspace_gap_diagnostics < 16;
		     ++lti) {
		    const ON_BrepTrim *current = loop.Trim(lti);
		    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		    if (!current || !next)
			continue;
		    const ON_3dPoint current_uv = current->PointAtEnd();
		    const ON_3dPoint next_uv = next->PointAtStart();
		    const double pspace_gap = current_uv.DistanceTo(next_uv);
		    if (pspace_gap <= ON_ZERO_TOLERANCE)
			continue;
		    const ON_3dPoint current_lift = surface->PointAt(
			current_uv.x, current_uv.y);
		    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		    double current_vertex_distance = DBL_MAX;
		    double next_vertex_distance = DBL_MAX;
		    if (current->m_vi[1] >= 0 && current->m_vi[1] < brep->m_V.Count())
			current_vertex_distance = current_lift.DistanceTo(
			    brep->m_V[current->m_vi[1]].point);
		    if (next->m_vi[0] >= 0 && next->m_vi[0] < brep->m_V.Count())
			next_vertex_distance = next_lift.DistanceTo(
			    brep->m_V[next->m_vi[0]].point);
		    std::cerr << entity_type << " #" << entity_id << ": loop " << li
			<< "/STEP" << loop.m_loop_user.i << " p-space gap "
			<< current->m_trim_index << "(type="
			<< static_cast<int>(current->m_type) << ",iso="
			<< static_cast<int>(current->m_iso) << ",e=" << current->m_ei
			<< ",rev=" << (current->m_bRev3d ? 1 : 0) << ",v="
			<< current->m_vi[1] << ")->" << next->m_trim_index
			<< "(type=" << static_cast<int>(next->m_type) << ",iso="
			<< static_cast<int>(next->m_iso) << ",e=" << next->m_ei
			<< ",rev=" << (next->m_bRev3d ? 1 : 0) << ",v="
			<< next->m_vi[0]
			<< ") uv=" << pspace_gap << " lift="
			<< current_lift.DistanceTo(next_lift) << " vertex distances="
			<< current_vertex_distance << '/' << next_vertex_distance
			<< " domains=" << surface->Domain(0).Min() << ':'
			<< surface->Domain(0).Max() << ',' << surface->Domain(1).Min()
			<< ':' << surface->Domain(1).Max() << " closed="
			<< (surface->IsClosed(0) ? 1 : 0)
			<< (surface->IsClosed(1) ? 1 : 0)
			<< std::endl;
		    ++pspace_gap_diagnostics;
		}
	    }
	    size_t trim_vertex_mismatches = 0;
	    const size_t trim_vertex_diagnostic_limit = 16;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		const ON_BrepLoop &loop = brep->m_L[li];
		const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *current = loop.Trim(lti);
		    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		    if (!current || !next || current->m_vi[1] == next->m_vi[0])
			continue;
		    ++trim_vertex_mismatches;
		    if (trim_vertex_mismatches > trim_vertex_diagnostic_limit)
			continue;
		    const ON_BrepEdge *current_edge = current->Edge();
		    const ON_BrepEdge *next_edge = next->Edge();
		    double vertex_distance = DBL_MAX;
		    if (current->m_vi[1] >= 0 && current->m_vi[1] < brep->m_V.Count() &&
			next->m_vi[0] >= 0 && next->m_vi[0] < brep->m_V.Count())
			vertex_distance = brep->m_V[current->m_vi[1]].point.DistanceTo(
			    brep->m_V[next->m_vi[0]].point);
		    const ON_3dPoint current_uv = current->PointAtEnd();
		    const ON_3dPoint next_uv = next->PointAtStart();
		    double lift_distance = DBL_MAX;
		    if (surface) {
			const ON_3dPoint current_lift = surface->PointAt(current_uv.x,
			    current_uv.y);
			const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
			if (current_lift.IsValid() && next_lift.IsValid())
			    lift_distance = current_lift.DistanceTo(next_lift);
		    }
		    std::cerr << entity_type << " #" << entity_id << ": loop " << li
			<< " (STEP loop " << loop.m_loop_user.i << ") trim vertex mismatch "
			<< current->m_trim_index << "[" << current->m_vi[0] << ','
			<< current->m_vi[1] << "]/edge " << current->m_ei << "/STEP"
			<< (current_edge ? current_edge->m_edge_user.i : 0) << " -> "
			<< next->m_trim_index << "[" << next->m_vi[0] << ','
			<< next->m_vi[1] << "]/edge " << next->m_ei << "/STEP"
			<< (next_edge ? next_edge->m_edge_user.i : 0)
			<< " vertex distance=" << vertex_distance
			<< " p-space/lift distance=" << current_uv.DistanceTo(next_uv)
			<< '/' << lift_distance << std::endl;
		}
	    }
	    if (trim_vertex_mismatches > trim_vertex_diagnostic_limit)
		std::cerr << entity_type << " #" << entity_id << ": suppressed "
		    << trim_vertex_mismatches - trim_vertex_diagnostic_limit
		    << " additional trim vertex mismatches ("
		    << trim_vertex_mismatches << " total)" << std::endl;
	    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = brep->m_T[ti];
		if (trim.m_type == ON_BrepTrim::seam &&
			trim.m_iso != ON_Surface::W_iso && trim.m_iso != ON_Surface::E_iso &&
			trim.m_iso != ON_Surface::S_iso && trim.m_iso != ON_Surface::N_iso) {
		    const ON_BoundingBox box = trim.BoundingBox();
		    const ON_BrepLoop *loop = trim.Loop();
		    const ON_Surface *surface = loop && loop->Face() ?
			loop->Face()->SurfaceOf() : NULL;
		    std::cerr << entity_type << " #" << entity_id
			<< ": unresolved seam after bounded repair trim " << ti
			<< " iso=" << static_cast<int>(trim.m_iso) << " uv box="
			<< box.m_min.x << ':' << box.m_max.x << ','
			<< box.m_min.y << ':' << box.m_max.y;
		    if (surface)
			std::cerr << " surface closed=" << (surface->IsClosed(0) ? '1' : '0')
			    << (surface->IsClosed(1) ? '1' : '0') << " domains="
			    << surface->Domain(0).Min() << ':' << surface->Domain(0).Max()
			    << ',' << surface->Domain(1).Min() << ':'
			    << surface->Domain(1).Max();
		    if (trim.m_ei >= 0 && trim.m_ei < brep->m_E.Count()) {
			const ON_BrepEdge &edge = brep->m_E[trim.m_ei];
			std::cerr << " edge=" << trim.m_ei << " uses=";
			for (int use = 0; use < edge.m_ti.Count(); ++use)
			    std::cerr << (use ? "," : "") << edge.m_ti[use];
		    }
		    std::cerr << std::endl;
		}
	    }
	}
}


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


#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
struct LazyAP214Occurrence {
    uint64_t entity_id = 0;
    uint64_t relationship_id = 0;
    uint64_t parent_representation_id = 0;
    uint64_t child_representation_id = 0;
    uint64_t parent_product_id = 0;
    uint64_t child_product_id = 0;
    uint64_t parent_axis_id = 0;
    uint64_t child_axis_id = 0;
};


struct LazyAP214ExactGraph {
    std::map<uint64_t, uint64_t> representation_product;
    std::map<uint64_t, std::vector<uint64_t> > representation_solids;
    std::map<uint64_t, uint64_t> representation_context;
    std::set<uint64_t> handled_representations;
    std::set<uint64_t> handled_sdrs;
    std::set<uint64_t> handled_relationships;
    std::set<uint64_t> handled_cdsrs;
    std::vector<LazyAP214Occurrence> occurrences;
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


static LazyAP214ExactGraph
build_lazy_ap214_exact_graph(STEPWrapper *wrapper)
{
    LazyAP214ExactGraph graph;
    if (!wrapper || !wrapper->HasLazyIndex())
	return graph;

    std::set<uint64_t> products;
    std::set<uint64_t> formations;
    std::set<uint64_t> definitions;
    std::set<uint64_t> definition_shapes;
    std::set<uint64_t> representations;
    std::set<uint64_t> contexts;
    std::set<uint64_t> exact_solids;
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

    std::map<uint64_t, bool> component_has_exact;
    std::map<uint64_t, bool> component_safe;
    for (std::set<uint64_t>::const_iterator representation = representations.begin();
	 representation != representations.end(); ++representation) {
	const uint64_t component = components.find(*representation);
	if (component_safe.find(component) == component_safe.end()) component_safe[component] = true;
	const std::vector<uint64_t> references = wrapper->LazyForwardReferences(*representation);
	for (std::vector<uint64_t>::const_iterator reference = references.begin();
	     reference != references.end(); ++reference) {
	    if (exact_solids.find(*reference) != exact_solids.end()) {
		graph.representation_solids[*representation].push_back(*reference);
		component_has_exact[component] = true;
		continue;
	    }
	    if (contexts.find(*reference) != contexts.end()) {
		/* context_of_items is singular; retain the first scanner-order hit. */
		if (!graph.representation_context[*representation])
		    graph.representation_context[*representation] = *reference;
		continue;
	    }
	    if (axes.find(*reference) != axes.end()) continue;
	    component_safe[component] = false;
	}
    }

    std::set<uint64_t> safe_exact_components;
    for (std::map<uint64_t, bool>::const_iterator component = component_has_exact.begin();
	 component != component_has_exact.end(); ++component) {
	if (component->second && component_safe[component->first] &&
	    conflicting_components.find(component->first) == conflicting_components.end())
	    safe_exact_components.insert(component->first);
    }
    for (std::set<uint64_t>::const_iterator representation = representations.begin();
	 representation != representations.end(); ++representation) {
	if (safe_exact_components.find(components.find(*representation)) !=
		safe_exact_components.end())
	    graph.handled_representations.insert(*representation);
    }
    for (std::map<uint64_t, uint64_t>::const_iterator link = sdr_representation.begin();
	 link != sdr_representation.end(); ++link) {
	if (safe_exact_components.find(components.find(link->second)) != safe_exact_components.end())
	    graph.handled_sdrs.insert(link->first);
    }
    for (std::set<uint64_t>::const_iterator relationship = relationships.begin();
	 relationship != relationships.end(); ++relationship) {
	if (transforming_relationships.find(*relationship) != transforming_relationships.end())
	    continue;
	const uint64_t representation = lazy_reference_in_set(wrapper, *relationship, representations);
	if (representation && safe_exact_components.find(components.find(representation)) !=
		safe_exact_components.end())
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
	LazyAP214Occurrence occurrence;
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
	    message = "closed AP214 BREP did not validate as a solid";
	    break;
	case BREP_OUTPUT_FAILED:
	    ++stats.output_failures;
	    message = "BRL-CAD database write failed";
	    break;
	default:
	    message = "unknown BREP conversion failure";
	    break;
    }
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
    bool ready = false;
    bool has_style = false;
    brlcad::step::Style style;
    SolidModel *solid = NULL;
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


static size_t repair_closed_shell_face_orientations(ON_Brep *brep);


static std::unique_ptr<DetachedBrepJob>
detach_brep_job_data(int entity_id, const std::string &entity_type,
    const std::string &original_name, const std::string &output_name,
    int product_id, double length_factor, double planeangle_factor,
    double solidangle_factor, const fastf_t *write_matrix,
    const brlcad::step::Style *style)
{
    std::unique_ptr<DetachedBrepJob> job(new DetachedBrepJob());
    job->entity_id = entity_id;
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
    job.entity_arena.reset();
    if (!wrapper || job.entity_id <= 0)
	return;

    SDAI_Application_instance *source = wrapper->getEntity(job.entity_id);
    if (source) {
	RepresentationItem *item = dynamic_cast<RepresentationItem *>(
	    Factory::CreateObject(wrapper, source));
	job.solid = exact_brep_solid(item);
	if (job.solid) {
	    if (job.original_name.empty()) job.original_name = job.solid->Name();
	    if (job.entity_type.empty()) job.entity_type = exact_brep_solid_type(job.solid);
	    job.faceted = dynamic_cast<FacetedBrep *>(job.solid) != NULL;
	}
    }
    job.entity_arena = wrapper->DetachEntityCache();
    wrapper->ReleaseSourceData();
    job.ready = job.solid != NULL && job.entity_arena != NULL;
}


static void
convert_detached_brep_job(DetachedBrepJob &job, STEPWrapper *wrapper)
{
    if (!job.ready || !job.solid || !job.entity_arena || !wrapper) {
	job.status = BREP_CONVERSION_FAILED;
	return;
    }

    LocalUnits::length = job.length_factor;
    LocalUnits::planeangle = job.planeangle_factor;
    LocalUnits::solidangle = job.solidangle_factor;
    LocalUnits::tolerance = job.tolerance;

    job.entity_arena->ResetOpenNURBSState();
    job.brep.reset(new ON_Brep());
    if (!job.solid->LoadONBrep(job.brep.get())) {
	job.brep.reset();
	job.status = BREP_CONVERSION_FAILED;
	return;
    }
    size_t keyhole_splits = finalize_brep_topology(job.brep.get(),
	wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe,
	wrapper, job.entity_id, job.entity_type);
    if (!job.faceted)
	repair_closed_trim_orientations(job.brep.get(), wrapper, job.entity_id,
	    job.entity_type);
    /* Exact pcurve repair may unwrap a periodic join or prove a duplicate
     * seam bridge that was not visible in the initial topology.  Do not touch
     * a BREP which is already valid; retry topology only for the remaining
     * structural failure, then repair any joins exposed by that normalization. */
    ON_wString preliminary_messages;
    ON_TextLog preliminary_log(preliminary_messages);
    const bool needs_post_repair_topology = !job.brep->IsValid(&preliminary_log);
    if (needs_post_repair_topology &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	bool post_repair_topology_changed = false;
	const size_t post_repair_splits = finalize_brep_topology(job.brep.get(), true,
	    wrapper, job.entity_id, job.entity_type, &post_repair_topology_changed);
	keyhole_splits += post_repair_splits;
	if (!job.faceted && post_repair_topology_changed)
	    repair_closed_trim_orientations(job.brep.get(), wrapper, job.entity_id,
		job.entity_type);
    }
    for (size_t split = 0; split < keyhole_splits; ++split)
	wrapper->RecordRepair(job.entity_id, job.entity_type, "edge_loop",
	    "normalized a zero-area seam bridge into valid closed trim topology");

    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!job.brep->IsValid(&validation_log)) {
	ON_String text(validation_messages);
	job.validation_message = text.Array();
	std::ostringstream joins;
	int reported_joins = 0;
	std::set<int> detailed_loops;
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
		joins << "\njoin L" << li << "/T" << previous->m_trim_index
		    << "(type=" << static_cast<int>(previous->m_type)
		    << ",iso=" << static_cast<int>(previous->m_iso)
		    << ",edge=" << previous->m_ei << ")->T"
		    << next->m_trim_index << "(type="
		    << static_cast<int>(next->m_type) << ",iso="
		    << static_cast<int>(next->m_iso) << ",edge=" << next->m_ei
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
	for (std::set<int>::const_iterator li = detailed_loops.begin();
		li != detailed_loops.end(); ++li) {
	    if (*li < 0 || *li >= job.brep->m_L.Count())
		continue;
	    const ON_BrepLoop &loop = job.brep->m_L[*li];
	    joins << "\nloop L" << *li << " trims=";
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		if (!trim)
		    continue;
		const ON_3dPoint start = trim->PointAtStart();
		const ON_3dPoint end = trim->PointAtEnd();
		joins << (lti ? ";" : "") << 'T' << trim->m_trim_index
		    << "(type=" << static_cast<int>(trim->m_type)
		    << ",iso=" << static_cast<int>(trim->m_iso)
		    << ",edge=" << trim->m_ei << ",rev="
		    << (trim->m_bRev3d ? 1 : 0) << ',' << start.x << ':'
		    << start.y << "->" << end.x << ':' << end.y << ')';
	    }
	}
	job.validation_message += joins.str();
	job.status = BREP_INVALID_STRUCTURE;
	return;
    }
    bool solid = job.brep->IsSolid();
    if (!solid && wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	const size_t orientation_repairs = repair_closed_shell_face_orientations(
	    job.brep.get());
	for (size_t repair = 0; repair < orientation_repairs; ++repair)
	    wrapper->RecordRepair(job.entity_id, job.entity_type, "face_orientation",
		"corrected a face orientation from closed-shell edge-use constraints");
	if (orientation_repairs)
	    solid = job.brep->IsSolid();
    }
    if (!solid) {
	if (wrapper->Verbose()) {
	    bool oriented = false;
	    bool has_boundary = false;
	    const bool manifold = job.brep->IsManifold(&oriented, &has_boundary);
	    size_t boundary_trims = 0;
	    size_t inconsistent_edges = 0;
	    std::ostringstream boundary_examples;
	    std::ostringstream examples;
	    for (int ti = 0; ti < job.brep->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = job.brep->m_T[ti];
		if (trim.m_type != ON_BrepTrim::boundary)
		    continue;
		if (boundary_trims < 12) {
		    const ON_BrepEdge *edge = trim.Edge();
		    const ON_BrepLoop *loop = trim.Loop();
		    boundary_examples << ' ' << ti << "/E" << trim.m_ei << "/STEP"
			<< (edge ? edge->m_edge_user.i : 0) << "/L" << trim.m_li
			<< "/STEP" << (loop ? loop->m_loop_user.i : 0);
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
			<< first->Face()->m_face_index << "/r" << first_effective
			<< ",T" << second->m_trim_index << "/F"
			<< second->Face()->m_face_index << "/r" << second_effective << ')';
		++inconsistent_edges;
	    }
	    std::cerr << job.entity_type << " #" << job.entity_id
		<< ": solid audit manifold=" << (manifold ? "yes" : "no")
		<< " oriented=" << (oriented ? "yes" : "no")
		<< " boundary=" << (has_boundary ? "yes" : "no")
		<< " boundary trims=" << boundary_trims
		<< boundary_examples.str()
		<< " orientation conflicts=" << inconsistent_edges
		<< examples.str() << std::endl;
	}
	job.status = BREP_NOT_SOLID;
	return;
    }

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


static void
run_detached_brep_workers(std::vector<DetachedBrepJob *> &jobs,
    STEPWrapper *wrapper, unsigned int requested_workers)
{
    if (jobs.empty())
	return;
    const unsigned int worker_count = std::max(1U, std::min(requested_workers,
	static_cast<unsigned int>(jobs.size())));
    if (worker_count == 1) {
	for (std::vector<DetachedBrepJob *>::iterator job = jobs.begin();
	     job != jobs.end() && !wrapper->CancellationRequested(); ++job)
	    convert_detached_brep_job(**job, wrapper);
	return;
    }

    std::atomic<size_t> next(0);
    const auto worker = [&jobs, &next, wrapper]() {
	for (;;) {
	    if (wrapper->CancellationRequested()) return;
	    const size_t index = next.fetch_add(1);
	    if (index >= jobs.size())
		return;
	    try {
		convert_detached_brep_job(*jobs[index], wrapper);
	    } catch (...) {
		jobs[index]->status = BREP_CONVERSION_FAILED;
		jobs[index]->validation_message =
		    "exception while validating detached OpenNURBS geometry";
	    }
	}
    };

    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (unsigned int i = 0; i < worker_count; ++i)
	workers.emplace_back(worker);
    for (std::thread &thread : workers)
	thread.join();
}


static size_t
repair_closed_shell_face_orientations(ON_Brep *brep)
{
    if (!brep || brep->m_F.Count() < 2)
	return 0;

    bool oriented = false;
    bool has_boundary = false;
    if (!brep->IsManifold(&oriented, &has_boundary) || oriented || has_boundary)
	return 0;

    struct Constraint {
	int other;
	int parity;
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
	graph[static_cast<size_t>(first_index)].push_back({second_index, parity});
	graph[static_cast<size_t>(second_index)].push_back({first_index, parity});
    }

    std::vector<int> flip(static_cast<size_t>(brep->m_F.Count()), -1);
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
		    pending.push_back(constraint->other);
		} else if (assigned != required) {
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


static void
write_detached_brep_job(DetachedBrepJob &job, STEPWrapper *wrapper,
    BRLCADWrapper *dotg, int dry_run, MAP_OF_ENTITY_ID_TO_PRODUCT_ID &process_map)
{
    if (job.status == BREP_INVALID_STRUCTURE && wrapper->Verbose() &&
	!job.validation_message.empty())
	std::cerr << job.validation_message;
    if (job.status == BREP_INVALID_STRUCTURE && !job.validation_message.empty())
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
	    job.entity_id, job.entity_type, "openNURBS",
	    job.validation_message.substr(0, 4096));
    if (job.status == BREP_WRITE_SUCCESS) {
	const brlcad::step::Style *style = job.has_style ? &job.style : NULL;
	bool written = dry_run;
	if (!dry_run && job.faceted) {
	    written = dotg->WriteBot(job.output_name,
		job.bot_vertices.size() / 3, job.bot_faces.size() / 3,
		job.bot_vertices.data(), job.bot_faces.data(), job.write_matrix,
		job.entity_id, job.original_name, style);
	} else if (!dry_run) {
	    written = dotg->WriteBrep(job.output_name, job.brep.get(),
		job.write_matrix, true, job.entity_id, job.original_name, style);
	}
	if (!written)
	    job.status = BREP_OUTPUT_FAILED;
	else if (style)
	    ++wrapper->Statistics().styles_applied;
    }

    record_brep_result(wrapper, job.status, job.entity_id, job.entity_type);
    if (job.status != BREP_WRITE_SUCCESS)
	return;
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

    const size_t batch_limit = std::max<size_t>(1,
	static_cast<size_t>(wrapper->ImportOptions().effective_jobs));
    for (size_t begin = 0; begin < jobs.size(); begin += batch_limit) {
	if (wrapper->CancellationRequested()) return;
	const size_t end = std::min(jobs.size(), begin + batch_limit);
	std::vector<DetachedBrepJob *> batch;
	batch.reserve(end - begin);
	for (size_t index = begin; index < end; ++index) {
	    if (wrapper->CancellationRequested()) break;
	    materialize_detached_brep_job(*jobs[index], wrapper);
	    batch.push_back(jobs[index].get());
	}
	if (batch.empty()) return;
	run_detached_brep_workers(batch, wrapper,
	    wrapper->ImportOptions().effective_jobs);
	if (wrapper->CancellationRequested()) return;
	for (size_t index = begin; index < end; ++index) {
	    write_detached_brep_job(*jobs[index], wrapper, dotg, dry_run,
		process_map);
	    jobs[index]->brep.reset();
	    jobs[index]->entity_arena.reset();
	    jobs[index]->solid = NULL;
	}
    }
}


#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
struct LazyRepresentationUnits {
    double length = 1000.0;
    double planeangle = 1.0;
    double solidangle = 1.0;
};


static LazyRepresentationUnits
lazy_representation_units(STEPWrapper *wrapper, const LazyAP214ExactGraph &graph,
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
lazy_occurrence_matrix(STEPWrapper *wrapper, const LazyAP214Occurrence &occurrence,
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
index_lazy_exact_graph(const LazyAP214ExactGraph &graph, STEPWrapper *wrapper,
    BRLCADWrapper *database, MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map,
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID &id2productid_map)
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
}


static void
convert_lazy_occurrences(const LazyAP214ExactGraph &graph, STEPWrapper *wrapper,
    BRLCADWrapper *database, int dry_run,
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME &id2name_map)
{
    for (std::vector<LazyAP214Occurrence>::const_iterator source = graph.occurrences.begin();
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


static void
convert_lazy_exact_graph(const LazyAP214ExactGraph &graph, STEPWrapper *wrapper,
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
    write_detached_brep_jobs(jobs, wrapper, database, dry_run, process_map);
}
#endif

static BrepWriteStatus
convert_WritePlateBrep(
	ShellBasedSurfaceModel *sBrep,
	ShapeRepresentation *representation,
	STEPWrapper *wrapper,
	BRLCADWrapper *dot_g,
	std::string *name,
	int dry_run)
{
    if (representation) {
	LocalUnits::length = representation->GetLengthConversionFactor();
	LocalUnits::planeangle = representation->GetPlaneAngleConversionFactor();
	LocalUnits::solidangle = representation->GetSolidAngleConversionFactor();
    }
    wrapper->ResetOpenNURBSState();
    ON_Brep *onBrep = sBrep->GetONBrep();
    if (!onBrep)
	return BREP_CONVERSION_FAILED;

    ON_wString validation_messages;
    ON_TextLog tl(validation_messages);
    const size_t keyhole_splits = finalize_brep_topology(onBrep,
	wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe,
	wrapper, sBrep->GetId(), "SHELL_BASED_SURFACE_MODEL");
    for (size_t split = 0; split < keyhole_splits; ++split)
	wrapper->RecordRepair(sBrep->GetId(), "SHELL_BASED_SURFACE_MODEL", "edge_loop",
	    "split a zero-area keyhole bridge into closed trim loops");
    repair_closed_trim_orientations(onBrep, wrapper, sBrep->GetId(),
	"SHELL_BASED_SURFACE_MODEL");
    if (!onBrep->IsValid(&tl)) {
	delete onBrep;
	return BREP_INVALID_STRUCTURE;
    }

	mat_t mat;
	representation_matrix(representation, mat);

    /* A shell-based surface model is intentionally non-region geometry. */
    const brlcad::step::Style *style = style_for_item(wrapper, sBrep->GetId());
    if (!style && representation)
	style = style_for_item(wrapper, representation->GetId());
    const bool written = dry_run || dot_g->WriteBrep(*name, onBrep, mat, false,
	sBrep->GetId(), sBrep->Name(), style);
    if (written && style)
	++wrapper->Statistics().styles_applied;
    delete onBrep;
    return written ? BREP_WRITE_SUCCESS : BREP_OUTPUT_FAILED;
}

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

bool STEPWrapper::convert(BRLCADWrapper *dot_g)
{
    MAP_OF_PRODUCT_NAME_TO_ENTITY_ID name2id_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME id2name_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID id2productid_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID process_map;
    MAP_OF_ENTITY_ID_TO_PRODUCT_ID shell2representation_map;

    if (!dot_g) {
	return false;
    }

    this->dotg = dot_g;

    statistics.geometry_attempted = 0;
    statistics.geometry_written = 0;
    statistics.geometry_skipped = 0;
    statistics.invalid_breps = 0;
    statistics.output_failures = 0;
    statistics.styles_extracted = 0;
    statistics.styles_applied = 0;
    statistics.layers_extracted = 0;
    statistics.selected_entity_ids_encountered.clear();
    document.products.clear();
    document.representations.clear();
    document.occurrences.clear();
    collectEntityCounts();
#ifdef AP214e3
    ExtractAP214Presentation(*this);
#endif
    statistics.tolerance_mm = deriveTolerance();
    LocalUnits::tolerance = statistics.tolerance_mm;
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    const LazyAP214ExactGraph lazy_exact_graph = build_lazy_ap214_exact_graph(this);
    const std::vector<uint64_t> lazy_handled_sdrs = lazy_ids(lazy_exact_graph.handled_sdrs);
    const std::vector<uint64_t> lazy_handled_relationships =
	lazy_ids(lazy_exact_graph.handled_relationships);
    const std::vector<uint64_t> lazy_handled_cdsrs = lazy_ids(lazy_exact_graph.handled_cdsrs);
#else
    const std::vector<uint64_t> lazy_handled_sdrs;
    const std::vector<uint64_t> lazy_handled_relationships;
    const std::vector<uint64_t> lazy_handled_cdsrs;
#endif

    SetInstanceTypes({"PRODUCT", "PRODUCT_DEFINITION_FORMATION",
	"PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE", "PRODUCT_DEFINITION",
	"PRODUCT_DEFINITION_WITH_ASSOCIATED_DOCUMENTS", "SHAPE_DEFINITION_REPRESENTATION",
	"MANIFOLD_SURFACE_SHAPE_REPRESENTATION", "GEOMETRIC_SET"}, lazy_handled_sdrs);
    int num_ents = InstanceCount();
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_product))) {
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
	    (sse->IsA(SCHEMA_NAMESPACE::e_product_definition_formation))) {
	    ProductDefinitionFormation *formation = dynamic_cast<ProductDefinitionFormation *>(
		Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (formation && formation->GetProductId() > 0) {
		brlcad::step::Product &product = document.products[formation->GetProductId()];
		product.entity_id = formation->GetProductId();
		product.revision = brlcad::step::decode_string(formation->Ident());
		product.revision_description = brlcad::step::decode_string(formation->Description());
	    }
	    ClearEntityCache();
	}

	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_product_definition))) {
	    ProductDefinition *definition = dynamic_cast<ProductDefinition *>(Factory::CreateObject(
		this, (SDAI_Application_instance *)sse));
	    if (definition && definition->GetProductId() > 0) {
		brlcad::step::Product &product = document.products[definition->GetProductId()];
		product.entity_id = definition->GetProductId();
		product.definition_identifier = brlcad::step::decode_string(definition->Ident());
		product.definition_description = brlcad::step::decode_string(definition->Description());
	    }
	    ClearEntityCache();
	}

	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_definition_representation))) {
	    ShapeDefinitionRepresentation *sdr = dynamic_cast<ShapeDefinitionRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));

	    if (!sdr) {
		RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, sse->STEPfile_id,
		    "SHAPE_DEFINITION_REPRESENTATION", std::string(), "entity materialization failed");
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
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_manifold_surface_shape_representation))) {
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
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_geometric_set))) {
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
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    index_lazy_exact_graph(lazy_exact_graph, this, dotg, id2name_map,
	id2productid_map);
#endif
#ifdef AP214e3
    /* Product and representation identity must be established before
     * represented properties and make-from material links are detached into
     * the schema-neutral document. */
    ExtractAP214Metadata(*this);
    ConvertAP214CSG(*this, *dot_g, lazy_handled_sdrs);
    ConvertAP214SweptSolids(*this, *dot_g, lazy_handled_sdrs);
#endif
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
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_representation_relationship))) {
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

		// If not a Brep, try a bounded surface shape
		// NOTE:  The modeling intent may be for the set of surfaces to
		// bound a closed volume, but the topological information
		// defining the BRep doesn't appear to be encoded explicitly in
		// this representation.  We will import it as plate mode
		// surfaces under a comb - it will be up to some sort of
		// "welding" operation to deduce and construct the actual
		// OpenNURBS BRep from the surface set, if they do in fact
		// define such a closed volume.
		GeometricallyBoundedSurfaceShapeRepresentation *aBS = dynamic_cast<GeometricallyBoundedSurfaceShapeRepresentation*>(srr->GetRepresentationRelationshipRep_2());
		if (!aBS) { //try rep_1
		    aBS = dynamic_cast<GeometricallyBoundedSurfaceShapeRepresentation*>(srr->GetRepresentationRelationshipRep_1());
		    aSR = dynamic_cast<ShapeRepresentation *>(srr->GetRepresentationRelationshipRep_2());
		}
		if (aSR && aBS) {
		    mat_t mat;
		    MAT_IDN(mat);
		    //std::cout << "GeometricallyBoundedSurfaceShapeRepresentation\n";
		    LIST_OF_REPRESENTATION_ITEMS *items = aBS->items_();
		    const std::vector<FlattenedRepresentationItem> flattened =
			flatten_representation_items(items, this);
		    for (std::vector<FlattenedRepresentationItem>::const_iterator ii = flattened.begin();
			 ii != flattened.end(); ++ii) {
			GeometricSet *gs = dynamic_cast<GeometricSet*>(ii->item);
			if (!gs) continue;
			string comb = id2name_map[gs->GetId()];
			bool has_valid_surface = false;
			LIST_OF_GEOMETRIC_SET_SELECT *sitems = gs->GetElements();
			LIST_OF_GEOMETRIC_SET_SELECT::iterator sii;
			for (sii = sitems->begin(); sii != sitems->end(); ++sii) {
			    BSplineSurfaceWithKnots *ks = dynamic_cast<BSplineSurfaceWithKnots*>((*sii)->GetSurfaceElement());
			    if (ks) {
				int kd = ks->GetId();
				const bool selected_set = ShouldConvertEntity(gs->GetId());
				const bool selected_surface = ShouldConvertEntity(kd);
				if (!selected_set && !selected_surface) continue;
				std::string kname  = ks->Name() + std::string("_") + std::to_string(kd);
				kname = dotg->StableBRLCADName(kname, kd);
				BrepWriteStatus status = convert_WriteBSpline(ks, this, dotg, &kname, dry_run);
				record_brep_result(this, status, kd, "B_SPLINE_SURFACE_WITH_KNOTS");
				if (status == BREP_WRITE_SUCCESS && !dry_run)
				    dotg->AddMember(comb,kname,mat);
				if (status == BREP_WRITE_SUCCESS) has_valid_surface = true;
			    }
			    if (has_valid_surface && !dry_run)
				dotg->AddMember(std::string("shells"), comb, mat);
			}
		    }

		    ClearEntityCache();
		    continue;
		}

		ClearEntityCache();

	    }
	}
    }

    SetInstanceTypes({"SHELL_BASED_SURFACE_MODEL", "SHAPE_REPRESENTATION_RELATIONSHIP",
	"REPRESENTATION_RELATIONSHIP", "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION"},
	lazy_handled_relationships);
    num_ents = InstanceCount();
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}

	// Find plate mode Brep objects through e_shell_based_surface_model
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shell_based_surface_model))) {
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


	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_representation_relationship))) {
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

#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    convert_lazy_occurrences(lazy_exact_graph, this, dotg, dry_run, id2name_map);
#endif
    SetInstanceTypes({"CONTEXT_DEPENDENT_SHAPE_REPRESENTATION"}, lazy_handled_cdsrs);
    num_ents = InstanceCount();
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_context_dependent_shape_representation))) {
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
		    occurrence.parent_product_id = parent_product_id;
		    occurrence.child_product_id = child_product_id;
		    for (size_t mi = 0; mi < 16; ++mi)
			occurrence.matrix[mi] = mat[mi];
		    if (!dry_run)
			dotg->AddMember(comb,member,mat);
		}
		ClearEntityCache();
	    }
	}
    }
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    convert_lazy_exact_graph(lazy_exact_graph, this, dotg, dry_run,
	id2name_map, process_map);
#endif
    SetInstanceTypes({"SHELL_BASED_SURFACE_MODEL", "SHAPE_DEFINITION_REPRESENTATION"},
	lazy_handled_sdrs);
    num_ents = InstanceCount();
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);

	/* Plate mode solid */
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shell_based_surface_model))) {
	    ShellBasedSurfaceModel *gr = dynamic_cast<ShellBasedSurfaceModel *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (gr) {
		int id = gr->GetId();
		if (!ShouldConvertEntity(id)) {
		    ClearEntityCache();
		    continue;
		}
		name = id2name_map[id];
		ShapeRepresentation *surface_representation = NULL;
		MAP_OF_ENTITY_ID_TO_PRODUCT_ID::const_iterator sri = shell2representation_map.find(id);
		if (sri != shell2representation_map.end()) {
		    SDAI_Application_instance *representation_entity = getEntity(sri->second);
		    if (representation_entity)
			surface_representation = dynamic_cast<ShapeRepresentation *>(
			    Factory::CreateObject(this, representation_entity));
		}
		BrepWriteStatus status = convert_WritePlateBrep(gr, surface_representation,
		    this, dot_g, &name, dry_run);
		record_brep_result(this, status, id, "SHELL_BASED_SURFACE_MODEL");
		MAP_OF_ENTITY_ID_TO_PRODUCT_ID::iterator pit = id2productid_map.find(id);
		if (status == BREP_WRITE_SUCCESS && !dry_run && pit != id2productid_map.end()) {
		    std::string comb = id2name_map[pit->second];
		    mat_t mat;
		    MAT_IDN(mat);
		    dotg->AddMember(comb, name, mat);
		}
	    }
	    ClearEntityCache();
	}

	/* Shape Definition Representation */
	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_definition_representation))) {
	    ShapeDefinitionRepresentation *sdr = dynamic_cast<ShapeDefinitionRepresentation *>(Factory::CreateObject(this, (SDAI_Application_instance *)sse));
	    if (!sdr) {
		RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, sse->STEPfile_id,
		    "SHAPE_DEFINITION_REPRESENTATION", std::string(), "entity materialization failed");
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
				if (geometric_set_has_curves(wire_set)) {
				    const int set_id = wire_set->GetId();
				    if (!ShouldConvertEntity(set_id)) continue;
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
				    BrepWriteStatus status = convert_WriteWireSet(wire_set, aSR, this,
					dot_g, &wire_name, dry_run, style_for_flattened_item(this, *item));
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
    SetInstanceTypes({"SHAPE_REPRESENTATION_RELATIONSHIP",
	"REPRESENTATION_RELATIONSHIP", "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION"},
	lazy_handled_relationships);
    num_ents = InstanceCount();
    for (int i = 0; i < num_ents; i++) {
	if (CancellationRequested()) return false;
	/* Shape Representation Relationship */
	SDAI_Application_instance *sse = InstanceAt(i);
	if (sse == NULL) {
	    continue;
	}
	std::string name = sse->EntityName();
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))std::tolower);


	if ((sse->STEPfile_id > 0) && (sse->IsA(SCHEMA_NAMESPACE::e_shape_representation_relationship))) {
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
			convert_representation_geometry(aBrep, product_id, this, dotg, dry_run,
			    id2name_map, process_map);
		    }/**/
		}
		ClearEntityCache();
	    }
	}
    }

    if (!dry_run && !dotg->WriteCombs()) {
	++statistics.output_failures;
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, 0, "BRLCAD_DATABASE",
	    std::string(), "failed writing one or more hierarchy combinations");
    }

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
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
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
    for (std::set<int64_t>::const_iterator requested = import_options.selected_entity_ids.begin();
	 requested != import_options.selected_entity_ids.end(); ++requested) {
	if (statistics.selected_entity_ids_encountered.find(*requested) !=
	    statistics.selected_entity_ids_encountered.end()) continue;
	++statistics.geometry_attempted;
	++statistics.geometry_skipped;
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, *requested,
	    "ENTITY_SELECTION", std::string(),
	    "selected entity was not found as a supported representation-item root");
    }
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    releaseLazyBatches();
#endif
    return statistics.output_failures == 0 && (!import_options.strict || statistics.geometry_skipped == 0);
}


SDAI_Application_instance *
STEPWrapper::getEntity(int STEPid)
{
    if (STEPid <= 0)
	return NULL;
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    if (lazy_session) {
	SDAI_Application_instance *instance = lazy_batch ? lazy_batch->Get(STEPid) : NULL;
	if (instance) return instance;
	for (std::vector<std::unique_ptr<brlcad::step::AP214LazyBatch> >::const_iterator batch =
		 lazy_supplemental_batches.begin(); batch != lazy_supplemental_batches.end(); ++batch) {
	    instance = (*batch)->Get(STEPid);
	    if (instance) return instance;
	}
	std::unique_ptr<brlcad::step::AP214LazyBatch> supplemental(
	    new brlcad::step::AP214LazyBatch(lazy_session->LoadBatch(static_cast<uint64_t>(STEPid))));
	instance = supplemental->Get(STEPid);
	if (instance) lazy_supplemental_batches.push_back(std::move(supplemental));
	return instance;
    }
#endif
    if (!instance_list) return NULL;
    MgrNode *node = instance_list->FindFileId(STEPid);
    return node ? node->GetSTEPentity() : NULL;
}


SDAI_Application_instance *
STEPWrapper::getEntity(int STEPid, const char *name)
{
    SDAI_Application_instance *se = getEntity(STEPid);

    if (se && se->IsComplex()) {
	se = getSuperType(STEPid, name);
    }
    return se;
}


SDAI_Application_instance *
STEPWrapper::getEntity(SDAI_Application_instance *sse, const char *name)
{
    if (sse && sse->IsComplex()) {
	sse = getSuperType(sse, name);
    }
    return sse;
}


STEPattribute *
STEPWrapper::getAttribute(int STEPid, const char *name)
{
    STEPattribute *retValue = NULL;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();
	if (attrname.compare(name) == 0) {
	    retValue = attr;
	    break;
	}
    }

    return retValue;
}


LIST_OF_STRINGS *
STEPWrapper::getAttributes(int STEPid)
{
    LIST_OF_STRINGS *l = new LIST_OF_STRINGS;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string name = attr->Name();

	l->push_back(name);
    }

    return l;
}


Boolean
STEPWrapper::getBooleanAttribute(int STEPid, const char *name)
{
    Boolean retValue = BUnset;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Boolean)(*attr->ptr.e).asInt();
	    if (retValue > BUnset) {
		retValue = BUnset;
	    }
	    break;
	}
    }
    return retValue;
}


int
STEPWrapper::getEnumAttribute(int STEPid, const char *name)
{
    int retValue = 0;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (*attr->ptr.e).asInt();
	    break;
	}
    }
    return retValue;
}


Logical
STEPWrapper::getLogicalAttribute(int STEPid, const char *name)
{
    Logical retValue = LUnknown;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Logical)(*attr->ptr.e).asInt();
	    if (retValue > LUnknown) {
		retValue = LUnknown;
	    }
	    break;
	}
    }
    return retValue;
}


std::string
STEPWrapper::getLogicalString(Logical v)
{
    std::string retValue = "Unknown";

    switch (v) {
	case LFalse:
	    retValue = "LFalse";
	    break;
	case LTrue:
	    retValue = "LTrue";
	    break;
	case LUnset:
	    retValue = "LUnset";
	    break;
	case LUnknown:
	    retValue = "LUnknown";
	    break;
    }
    return retValue;
}


std::string
STEPWrapper::getBooleanString(Boolean v)
{
    std::string retValue = "Unknown";

    switch (v) {
	case BFalse:
	    retValue = "BFalse";
	    break;
	case BTrue:
	    retValue = "BTrue";
	    break;
	case BUnset:
	    retValue = "BUnset";
	    break;
    }
    return retValue;
}


SDAI_Application_instance *
STEPWrapper::getEntityAttribute(int STEPid, const char *name)
{
    SDAI_Application_instance *retValue = NULL;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (SDAI_Application_instance *)*attr->ptr.c;
	    break;
	}
    }
    return retValue;
}


int
STEPWrapper::getIntegerAttribute(int STEPid, const char *name)
{
    int retValue = 0;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.i;
	    break;
	}
    }
    return retValue;
}


double
STEPWrapper::getRealAttribute(int STEPid, const char *name)
{
    double retValue = 0.0;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.r;
	    break;
	}
    }
    return retValue;
}


LIST_OF_ENTITIES *
STEPWrapper::getListOfEntities(int STEPid, const char *name)
{
    LIST_OF_ENTITIES *l = new LIST_OF_ENTITIES;

    SDAI_Application_instance *sse = getEntity(STEPid);
    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    STEPaggregate *sa = (STEPaggregate *)attr->ptr.a;

	    EntityNode *sn = (EntityNode *)sa->GetHead();
	    SDAI_Application_instance *se;
	    while (sn != NULL) {
		se = (SDAI_Application_instance *)sn->node;

		l->push_back(se);
		sn = (EntityNode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_LIST_OF_POINTS *
STEPWrapper::getListOfListOfPoints(int STEPid, const char *attrName)
{
    LIST_OF_LIST_OF_POINTS *l = new LIST_OF_LIST_OF_POINTS;

    SDAI_Application_instance *sse = getEntity(STEPid);
    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string name = attr->Name();

	if (name.compare(attrName) == 0) {
	    ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    //EntityAggregate *ag = new EntityAggregate();


	    const char *eaStr;

	    LIST_OF_POINTS *points;
	    while (sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
		eaStr = sn->asStr(attrval);
		points = parseListOfPointEntities(eaStr);
		l->push_back(points);
		sn = (STEPnode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


MAP_OF_SUPERTYPES *
STEPWrapper::getMapOfSuperTypes(int STEPid)
{
    MAP_OF_SUPERTYPES *m = new MAP_OF_SUPERTYPES;
    SDAI_Application_instance *sse = getEntity(STEPid);

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    (*m)[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }

    return m;
}


void
STEPWrapper::getSuperTypes(int STEPid, MAP_OF_SUPERTYPES &m)
{
    SDAI_Application_instance *sse = getEntity(STEPid);

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    m[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }
}


SDAI_Application_instance *
STEPWrapper::getSuperType(int STEPid, const char *name)
{
    SDAI_Application_instance *sse = getEntity(STEPid);

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    std::string ename = sc->EntityName();

	    if (ename.compare(name) == 0) {
		return sc;
	    }
	    sc = sc->sc;
	}
    }
    return NULL;
}


std::string
STEPWrapper::getStringAttribute(int STEPid, const char *name)
{
    SDAI_Application_instance *sse = getEntity(STEPid);
    return getStringAttribute(sse, name);
}


STEPattribute *
STEPWrapper::getAttribute(SDAI_Application_instance *sse, const char *name)
{
    STEPattribute *retValue = NULL;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();
	if (attrname.compare(name) == 0) {
	    retValue = attr;
	    break;
	}
    }

    return retValue;
}


LIST_OF_STRINGS *
STEPWrapper::getAttributes(SDAI_Application_instance *sse)
{
    LIST_OF_STRINGS *l = new LIST_OF_STRINGS;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string name = attr->Name();

	l->push_back(name);
    }

    return l;
}


Boolean
STEPWrapper::getBooleanAttribute(SDAI_Application_instance *sse, const char *name)
{
    Boolean retValue = BUnset;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Boolean)(*attr->ptr.e).asInt();
	    if (retValue > BUnset) {
		retValue = BUnset;
	    }
	    break;
	}
    }
    return retValue;
}


int
STEPWrapper::getEnumAttribute(SDAI_Application_instance *sse, const char *name)
{
    int retValue = 0;
    std::string attrval;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (*attr->ptr.e).asInt();
	    //std::cout << "debug enum: " << (*attr->ptr.e).asStr(attrval) << std::endl;
	    break;
	}
    }
    return retValue;
}


SDAI_Application_instance *
STEPWrapper::getEntityAttribute(SDAI_Application_instance *sse, const char *name)
{
    SDAI_Application_instance *retValue = NULL;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attr->Nullable() && attr->is_null() && !attr->IsDerived()) {
	    continue;
	}
	if (attrname.compare(name) == 0) {
	    std::string attrval;
	    //std::cout << "attr:" << name << ":" << attr->TypeName() << ":" << attr->Name() << std::endl;
	    //std::cout << "attr:" << attr->asStr(attrval) << std::endl;
	    retValue = (SDAI_Application_instance *)*attr->ptr.c;
	    break;
	}
    }
    return retValue;
}


Logical
STEPWrapper::getLogicalAttribute(SDAI_Application_instance *sse, const char *name)
{
    Logical retValue = LUnknown;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Logical)(*attr->ptr.e).asInt();
	    if (retValue > LUnknown) {
		retValue = LUnknown;
	    }
	    break;
	}
    }
    return retValue;
}


int
STEPWrapper::getIntegerAttribute(SDAI_Application_instance *sse, const char *name)
{
    int retValue = 0;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.i;
	    break;
	}
    }
    return retValue;
}


double
STEPWrapper::getRealAttribute(SDAI_Application_instance *sse, const char *name)
{
    double retValue = 0.0;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.r;
	    break;
	}
    }
    return retValue;
}


SDAI_Select *
STEPWrapper::getSelectAttribute(SDAI_Application_instance *sse, const char *name)
{
    SDAI_Select *retValue = NULL;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (SDAI_Select *)attr->ptr.sh;
	    break;
	}
    }
    return retValue;
}


LIST_OF_ENTITIES *
STEPWrapper::getListOfEntities(SDAI_Application_instance *sse, const char *name)
{
    LIST_OF_ENTITIES *l = new LIST_OF_ENTITIES;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    STEPaggregate *sa = (STEPaggregate *)attr->ptr.a;

	    EntityNode *sn = (EntityNode *)sa->GetHead();
	    SDAI_Application_instance *se;
	    while (sn != NULL) {
		se = (SDAI_Application_instance *)sn->node;

		l->push_back(se);
		sn = (EntityNode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_SELECTS *
STEPWrapper::getListOfSelects(SDAI_Application_instance *sse, const char *name)
{
    LIST_OF_SELECTS *l = new LIST_OF_SELECTS;
    std::string attrval;

    sse->ResetAttributes();
    STEPattribute *attr;

    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {

	    SelectAggregate *sa = (SelectAggregate *)attr->ptr.a;
	    SelectNode *sn = (SelectNode *)sa->GetHead();
	    while (sn) {
		l->push_back(sn->node);
		sn = (SelectNode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_LIST_OF_PATCHES *
STEPWrapper::getListOfListOfPatches(SDAI_Application_instance *sse, const char *attrName)
{
    LIST_OF_LIST_OF_PATCHES *l = new LIST_OF_LIST_OF_PATCHES;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string name = attr->Name();

	if (name.compare(attrName) == 0) {
	    ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    //EntityAggregate *ag = new EntityAggregate();


	    const char *eaStr;

	    LIST_OF_PATCHES *patches;
	    while (sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
		eaStr = sn->asStr(attrval);
		patches = parseListOfPatchEntities(eaStr);
		l->push_back(patches);
		sn = (STEPnode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_LIST_OF_POINTS *
STEPWrapper::getListOfListOfPoints(SDAI_Application_instance *sse, const char *attrName)
{
    LIST_OF_LIST_OF_POINTS *l = new LIST_OF_LIST_OF_POINTS;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string name = attr->Name();

	if (name.compare(attrName) == 0) {
	    ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    //EntityAggregate *ag = new EntityAggregate();


	    const char *eaStr;

	    LIST_OF_POINTS *points;
	    while (sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
		eaStr = sn->asStr(attrval);
		points = parseListOfPointEntities(eaStr);
		l->push_back(points);
		sn = (STEPnode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


MAP_OF_SUPERTYPES *
STEPWrapper::getMapOfSuperTypes(SDAI_Application_instance *sse)
{
    MAP_OF_SUPERTYPES *m = new MAP_OF_SUPERTYPES;

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    (*m)[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }

    return m;
}


void
STEPWrapper::getSuperTypes(SDAI_Application_instance *sse, MAP_OF_SUPERTYPES &m)
{
    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    m[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }
}


SDAI_Application_instance *
STEPWrapper::getSuperType(SDAI_Application_instance *sse, const char *name)
{
    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    std::string ename = sc->EntityName();

	    if (ename.compare(name) == 0) {
		return sc;
	    }
	    sc = sc->sc;
	}
    }
    return NULL;
}


std::string
STEPWrapper::getStringAttribute(SDAI_Application_instance *sse, const char *name)
{
    std::string retValue = "";

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    const char *str = attr->asStr(attrval);
	    if (str != NULL) {
		retValue = str;
	    }
	    break;
	}
    }
    return retValue;
}


bool
STEPWrapper::load(std::string &step_file)
{
#if defined(AP214e3) && defined(HAVE_STEPCODE_LAZY)
    stepfile = step_file;
    lazy_session.reset(new brlcad::step::AP214LazySession);
    lazy_session->SetProgressCallback([this](const brlcad::step::AP214LazyProgress &progress) {
	if (verbose && progress.file_size)
	    std::cerr << "STEP lazy index: " << progress.instances_scanned << " instances, "
		<< progress.offset << '/' << progress.file_size << " bytes\n";
    });
    lazy_session->SetCancellationCallback([this]() {
	return CancellationRequested();
    });
    lazy_session->SetDiagnosticCallback([this](const brlcad::step::AP214LazyDiagnostic &source) {
	recordLazyDiagnostic(source);
    });
    if (!lazy_session->Open(stepfile)) {
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
	    std::string(), "STEPcode could not lazily index the exchange file");
	lazy_session.reset();
	return false;
    }
    lazy_instance_ids = lazy_session->AllInstances();
    const brlcad::step::AP214LazyStatistics cache = lazy_session->Statistics();
    statistics.input_instances = cache.instances_scanned;
    update_lazy_statistics(statistics, cache);
    synchronizeLazyDiagnostics();
    if (cache.cancelled || lazy_instance_ids.size() != cache.instances_scanned) {
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
	    std::string(), cache.cancelled ? "STEP lazy scan was cancelled" :
	    "STEP lazy index did not retain every DATA instance");
	return false;
    }
    return true;
#else
    registry = new Registry(SchemaInit);
    sfile = new STEPfile(*registry, *instance_list);

    stepfile = step_file;
    try {
	/* load STEP file */
	Severity severity = sfile->ReadExchangeFile(stepfile.c_str());
	statistics.input_instances = static_cast<uint64_t>(instance_list->InstanceCount());
	if (severity < SEVERITY_WARNING) {
	    std::string message = sfile->Error().UserMsg();
	    if (message.empty()) message = sfile->Error().DetailMsg();
	    if (message.empty()) message = "STEPcode rejected the exchange file";
	    RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
		std::string(), message);
	    return false;
	}
	if (severity == SEVERITY_WARNING) {
	    std::string message = sfile->Error().UserMsg();
	    if (message.empty()) message = "STEPcode reported recoverable input errors";
	    RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning, 0, "PART21_FILE",
		std::string(), message);
	}

    } catch (std::exception &e) {
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
	    std::string(), e.what());
	return false;
    }
    return true;
#endif

}


LIST_OF_REALS *
STEPWrapper::parseListOfReals(const char *in)
{
    LIST_OF_REALS *l = new LIST_OF_REALS;
    ErrorDescriptor errdesc;
    RealAggregate *ra = new RealAggregate();

    //ra->StrToVal(in, &errdesc, SDAI_Real, instance_list, 0);
    ra->StrToVal(in, &errdesc, SCHEMA_NAMESPACE::t_parameter_value, referenceManager(), 0);
    RealNode *rn = (RealNode *)ra->GetHead();
    while (rn != NULL) {
	l->push_back(rn->value);
	rn = (RealNode *)rn->NextNode();
    }
    /*
      EntityNode *sn = (EntityNode *)ra->GetHead();

      SDAI_Application_instance *sse;
      while (sn != NULL) {
      sse = (SDAI_Application_instance *)sn->node;
      CartesianPoint *aCP = new CartesianPoint(this, sse->STEPfile_id);
      if (aCP->Load(this, sse)) {
      l->push_back(aCP);
      } else {
      std::cout << "Error loading Real list." << std::endl;
      }
      sn = (EntityNode *)sn->NextNode();
      }*/
    delete ra;

    return l;
}


LIST_OF_POINTS *
STEPWrapper::parseListOfPointEntities(const char *in)
{
    LIST_OF_POINTS *l = new LIST_OF_POINTS;
    ErrorDescriptor errdesc;
    EntityAggregate *ag = new EntityAggregate();

    ag->StrToVal(in, &errdesc, SCHEMA_NAMESPACE::e_cartesian_point, referenceManager(), 0);
    EntityNode *sn = (EntityNode *)ag->GetHead();

    SDAI_Application_instance *sse;
    while (sn != NULL) {
	sse = (SDAI_Application_instance *)sn->node;
	CartesianPoint *aCP = dynamic_cast<CartesianPoint *>(Factory::CreateObject(this,sse));
	if (aCP != NULL) {
	    l->push_back(aCP);
	} else {
	    std::cout << "Error loading CartesianPoint." << std::endl;
	}
	sn = (EntityNode *)sn->NextNode();
    }
    delete ag;

    return l;
}


LIST_OF_PATCHES *
STEPWrapper::parseListOfPatchEntities(const char *in)
{
    LIST_OF_PATCHES *l = new LIST_OF_PATCHES;
    ErrorDescriptor errdesc;
    EntityAggregate *ag = new EntityAggregate();

    ag->StrToVal(in, &errdesc, SCHEMA_NAMESPACE::e_cartesian_point, referenceManager(), 0);
    EntityNode *sn = (EntityNode *)ag->GetHead();

    SDAI_Application_instance *sse;
    while (sn != NULL) {
	sse = (SDAI_Application_instance *)sn->node;
	SurfacePatch *aCP = dynamic_cast<SurfacePatch *>(Factory::CreateObject(this,sse));
	if (aCP != NULL) {
	    l->push_back(aCP);
	} else {
	    std::cout << "Error loading SurfacePatch." << std::endl;
	}
	sn = (EntityNode *)sn->NextNode();
    }
    delete ag;

    return l;
}


void
STEPWrapper::printEntity(SDAI_Application_instance *se, int level)
{
    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Entity:" << se->EntityName() << "(" << se->STEPfile_id << ")" << std::endl;
    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Description:" << se->eDesc->Description() << std::endl;
    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Entity Type:" << se->eDesc->Type() << std::endl;
    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Attributes:" << std::endl;

    STEPattribute *attr;
    se->ResetAttributes();
    while ((attr = se->NextAttribute()) != NULL) {
	std::string attrval;

	for (int i = 0; i <= level; i++) {
	    std::cout << "    ";
	}
	std::cout << attr->Name() << ": " << attr->asStr(attrval) << " TypeName: " << attr->TypeName() << " Type: " << attr->Type() << std::endl;
	if (attr->Type() == 256) {
	    if (attr->IsDerived()) {
		for (int i = 0; i <= level; i++) {
		    std::cout << "    ";
		}
		std::cout << "        ********* DERIVED *********" << std::endl;
	    } else {
		printEntity(*(attr->ptr.c), level + 2);
	    }
	} else if ((attr->Type() == SET_TYPE) || (attr->Type() == LIST_TYPE)) {
	    STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);

	    // std::cout << "aggr:" << sa->asStr(attrval) << "  BaseType:" << attr->BaseType() << std::endl;

	    if (attr->BaseType() == ENTITY_TYPE) {
		printEntityAggregate(sa, level + 2);
	    }
	}

    }
    //std::cout << std::endl << std::endl;
}


void
STEPWrapper::printEntityAggregate(STEPaggregate *sa, int level)
{
    std::string strVal;

    for (int i = 0; i < level; i++) {
	std::cout << "    ";
    }
    std::cout << "Aggregate:" << sa->asStr(strVal) << std::endl;

    EntityNode *sn = (EntityNode *)sa->GetHead();
    SDAI_Application_instance *sse;
    while (sn != NULL) {
	sse = (SDAI_Application_instance *)sn->node;

	if (((sse->eDesc->Type() == SET_TYPE) || (sse->eDesc->Type() == LIST_TYPE)) && (sse->eDesc->BaseType() == ENTITY_TYPE)) {
	    printEntityAggregate((STEPaggregate *)sse, level + 2);
	} else if (sse->eDesc->Type() == ENTITY_TYPE) {
	    printEntity(sse, level + 2);
	} else {
	    std::cout << "Instance Type not handled:" << std::endl;
	}
	//std::cout << "sn - " << sn->asStr(attrval) << std::endl;

	sn = (EntityNode *)sn->NextNode();
    }
    //std::cout << std::endl << std::endl;
}


void
STEPWrapper::printLoadStatistics()
{
    int num_ents = instance_list->InstanceCount();
    int num_schma_ents = registry->GetEntityCnt();

    // "Reset" the Schema and Entity hash tables... this sets things up
    // so we can walk through the table using registry->NextEntity()

    registry->ResetSchemas();
    registry->ResetEntities();

    // Print out what schema we're running through.

    const SchemaDescriptor *schema = registry->NextSchema();

    // "Loop" through the schema, building one of each entity type.

    const EntityDescriptor *ent;   // needs to be declared const...
    std::string filler = ".....................................................................";
    std::cout << "Loaded " << num_ents << " instances from ";
    if (BU_STR_EQUAL(stepfile.c_str(), "-")) {
	std::cout << "standard input" << std::endl;
    } else {
	std::cout << "STEP file \"" << stepfile << "\"" << std::endl;
    }

    int numEntitiesUsed = 0;
    for (int i = 0; i < num_schma_ents; i++) {
	ent = registry->NextEntity();

	int entCount = instance_list->EntityKeywordCount(ent->Name());
	// fix below with boost string formatter when available
	if (entCount > 0) {
	    std::cout << "\t" << ent->Name() << filler.substr(0, filler.length() - ((std::string)ent->Name()).length()) << entCount << std::endl;
	    numEntitiesUsed++;
	}
    }
    std::cout << "Used " << numEntitiesUsed << " entities of the available " << num_schma_ents << " in schema \"" << schema->Name() << std::endl;
}


const char *
STEPWrapper::getBaseType(int type)
{
    const char *retValue = NULL;

    switch (type) {
	case sdaiINSTANCE:
	    retValue = "sdaiINSTANCE";
	    break;
	case sdaiSELECT: // The name of a select is never written DAS 1/31/97
	    retValue = "sdaiSELECT";
	    break;
	case sdaiNUMBER:
	    retValue = "sdaiNUMBER";
	    break;
	case sdaiREAL:
	    retValue = "sdaiREAL";
	    break;
	case sdaiINTEGER:
	    retValue = "sdaiINTEGER";
	    break;
	case sdaiSTRING:
	    retValue = "sdaiSTRING";
	    break;
	case sdaiBOOLEAN:
	    retValue = "sdaiBOOLEAN";
	    break;
	case sdaiLOGICAL:
	    retValue = "sdaiLOGICAL";
	    break;
	case sdaiBINARY:
	    retValue = "sdaiBINARY";
	    break;
	case sdaiENUMERATION:
	    retValue = "sdaiENUMERATION";
	    break;
	case sdaiAGGR:
	    retValue = "sdaiAGGR";
	    break;
	case ARRAY_TYPE:
	    retValue = "ARRAY_TYPE";
	    break;
	case BAG_TYPE:
	    retValue = "BAG_TYPE";
	    break;
	case SET_TYPE:
	    retValue = "SET_TYPE";
	    break;
	case LIST_TYPE:
	    retValue = "LIST_TYPE";
	    break;
	case REFERENCE_TYPE: // this should never happen? DAS
	    retValue = "REFERENCE_TYPE";
	    break;
	default:
	    retValue = "Unknown";
	    break;
    }
    return retValue;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
