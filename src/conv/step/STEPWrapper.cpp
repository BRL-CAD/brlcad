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
#include <chrono>
#include <climits>
#include <condition_variable>
#include <deque>
#include <exception>

#include "brep/cdt.h"
#include "brep/pullback.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <thread>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/process.h"

/* interface header */
#include "./STEPWrapper.h"
#include "Factory.h"
#include "step-g/OpenNurbsInterfaces.h"

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
#include "ConnectedFaceSet.h"
#include "Curve.h"
#include "Direction.h"
#include "SurfacePatch.h"
#include "LocalUnits.h"
#include "FacetedBrep.h"
#include "Face.h"
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
#ifdef HAVE_STEPCODE_LAZY
#  include "STEPLazySession.h"
#endif

namespace {

/* Dense repair validation uses a power-of-two subdivision count so adjacent
 * passes sample the same deterministic parameter fractions.  1024 segments
 * proved sufficient to expose the narrow Mark V seam and pole failures while
 * keeping each candidate repair bounded; it is a validation budget, not a
 * tolerance or a value prescribed by STEP/openNURBS. */
constexpr int kDenseValidationSegments = 1024;

/* A parameter-correspondence polyline for a complete periodic boundary is
 * also checked at every segment midpoint.  Two additional binary refinement
 * levels keep the chord lift below the declared 1e-6 mm tolerance on the
 * largest tight-radius toroidal boundaries in the compatibility corpus.  This
 * remains a bounded construction budget; failure at this resolution skips
 * the exact item instead of silently coarsening it. */
constexpr int kPeriodicBoundaryConstructionSegments =
    4 * kDenseValidationSegments;

/* Surface derivatives at a collapsed NURBS pole can reverse arbitrarily close
 * to an otherwise exact edge endpoint.  Direction is therefore proven only
 * outside one interval of the minimum 64-segment regeneration mesh.  The
 * endpoint and every candidate chord remain subject to dense 3-D edge-locus
 * validation; this guard excludes only the ill-conditioned tangent test. */
constexpr double kEndpointDirectionGuardFraction = 1.0 / 64.0;

/* A relocated surface seam needs a genuinely boundary-free interval.  This
 * fraction avoids choosing a cut in numerical noise while remaining far
 * smaller than any interval used to interpolate a replacement pcurve. */
constexpr double kMinimumSeamRelocationGapFraction = 1.0e-3;

/* STEP pcurve endpoints at a periodic join can differ by a few measured
 * microradians even when both lift to the same topology vertex.  This
 * scale-relative parameter window only recognizes a whole-period branch
 * change after model-space coincidence is proven; the installed shift is an
 * exact integral period and remains subject to dense lift validation. */
constexpr double kPeriodicParameterSnapFraction = 1.0e-5;

/* A model tolerance larger than one percent of an edge can make every point
 * on a small but intentional feature look identical to a closest-point solver.
 * Once that relationship is detected, converge to one ten-thousandth of the
 * feature scale while retaining the model tolerance as the separate geometric
 * acceptance bound.  This resolves sub-tolerance circles without claiming
 * their source edge/surface mismatch is smaller than the exchange file says. */
constexpr double kPullbackLooseToleranceFeatureFraction = 1.0e-2;
constexpr double kPullbackSolverFeatureFraction = 1.0e-4;

/* Adaptive exact-edge regeneration may need several subdivisions inside one
 * dense validation interval near a narrow periodic seam.  Permit four times
 * the ordinary validation mesh plus its final endpoint.  This is used only
 * after a supplied pcurve has failed exact validation; cancellation and the
 * per-item elapsed-work budget still bound pathological cases. */
constexpr size_t kMaximumAdaptivePullbackPoints =
    4 * static_cast<size_t>(kDenseValidationSegments) + 1;

/* Reject compiler-hostile or corrupt pcurves before using their span count to
 * size bounded repair work.  This is deliberately separate from the dense
 * sampling budget even though the current limits have the same value. */
constexpr int kMaximumPcurveSpans = 1024;

/* Endpoint repair is a propagation problem: the first forward pass fixes the
 * eligible joins it encounters, the second handles a change propagated across
 * the cyclic wraparound, and a third proves stabilization.  Additional greedy
 * passes only move a periodic discontinuity around noncontractible loops; the
 * coherent branch solver or an explicit topology cut must handle those.  Every
 * proposed edit still undergoes the same dense lift and exact-edge proof. */
constexpr int kMaximumEndpointRepairSweeps = 3;

/* Exact pcurves occasionally arrive on adjacent periodic images of the same
 * closed surface.  Solving a complete cyclic loop may require moving a
 * contiguous group together, which a greedy one-trim repair cannot do without
 * temporarily worsening its other endpoint.  Search only a small integral
 * neighborhood around each supplied branch; every accepted translation is
 * still densely proven to preserve its surface lift. */
constexpr int kMaximumPeriodicBranchShift = 2;

/* Non-exact mode may reflect a densely measured source edge/surface mismatch
 * in the affected OpenNURBS edge tolerance.  A two-percent local-feature and
 * two-tenths-percent item-scale ceiling covers the smallest paired Mark V
 * curves whose independently supplied surfaces report the same 0.0164 mm
 * miss, while remaining far below a topology-changing gap.  This does not
 * alter either source curve; it only lets a later exact regeneration use a
 * tolerance justified by dense measurements.  --exact disables it. */
constexpr double kRegenerationMaximumRelativeMismatch = 2.0e-2;
constexpr double kRegenerationMaximumRelativeItemMismatch = 2.0e-3;
constexpr double kRegenerationToleranceSafety = 1.05;

/* Some exporters encode a surface boundary as topologically collapsed to one
 * STEP vertex even though the supplied NURBS boundary misses that vertex by
 * more than the declared uncertainty.  A singular trim is the exact
 * OpenNURBS representation of that asserted topology, but the affected local
 * tolerances must describe the supplied geometry.  Admit this only after a
 * dense proof over the complete boundary, and only when the mismatch is below
 * one half percent of that surface's model-space diagonal.  --exact keeps the
 * declared uncertainty and rejects the face. */
constexpr double kCollapsedBoundaryMaximumRelativeMismatch = 5.0e-3;

/* Regeneration validates the quarter points of every dense segment.  Measure
 * the exact 3-D edge against the surface at those same fractions before
 * deciding whether the source geometry itself justifies a tolerance
 * adjustment; measuring the supplied pcurve would incorrectly attribute a
 * bad trim association to the source edge/surface pair. */
constexpr int kRegenerationMeasurementSegments =
    4 * kDenseValidationSegments;

/* A coherent edge-driven loop regeneration may leave a few decimal-source
 * parameter units between adjacent endpoints even though both lifts match the
 * same STEP vertex.  Retain such a candidate only below this fraction of the
 * largest surface domain; the following densely validated endpoint pass must
 * still close the join exactly.  This is not a geometric tolerance. */
constexpr double kPeriodicRegenerationJoinParameterFraction = 1.0e-5;

/* A surface boundary is a one-dimensional curve, but a seed obtained from a
 * global surface closest-point query can lie on the wrong repeated branch of
 * a doubly periodic surface.  Sample a modest, fixed number of boundary
 * intervals before retrying the local Newton solve.  This is a search budget;
 * every accepted result is still checked against model uncertainty. */
constexpr int kBoundaryParameterSearchSegments = 256;

/* Scale ON_ZERO_TOLERANCE above floating-point noise for parameter/lift
 * equivalence checks.  Keep this distinct from model-space uncertainty. */
constexpr double kNumericalToleranceScale = 1024.0;

/* Keep reports useful for targeted retries without allowing a corrupt file to
 * grow JSON output without bound.  The total omitted count remains exact. */
constexpr size_t kMaximumReportedSkippedItems = 4096;

/* Updating the telemetry snapshot requires a short mutex acquisition.  Once
 * per 256 scanned entities keeps progress current without adding measurable
 * synchronization overhead to million-instance product-graph walks. */
constexpr int kProgressUpdateStride = 256;

/* Keep one queued detached job behind each active geometry worker so serial
 * STEP materialization can overlap conversion without accumulating an
 * unbounded number of live SDAI dependency arenas. */
constexpr unsigned int kRunnableGeometryJobsPerWorker = 2;

/* Completed BREP results may wait for a slow lower STEP ID before their
 * deterministic database write.  Reserve at most one quarter of the 2 GiB
 * Mark V process-memory acceptance gate for that reorder buffer.  OpenNURBS
 * SizeOf() and explicit BoT arrays account for the payload; telemetry reports
 * both the estimate and process peak RSS so this budget remains testable. */
constexpr uint64_t kMaximumReadyGeometryBytes = 512ULL * 1024ULL * 1024ULL;

/* A PID plus monotonic process-local suffix should be unique immediately.
 * Bound collision recovery so a hostile cache directory cannot hang import. */
constexpr uint64_t kMaximumGeometrySpoolNameAttempts = 1024;

/* A conversion-completion spool prevents deterministic STEP-ID output order
 * from retaining completed OpenNURBS models in memory.  BU_DIR_CACHE selects
 * the platform user cache and BRL-CAD application subdirectory; BU_DIR_TEMP is
 * a fallback for read-only or unavailable cache locations. */
class TemporaryGeometrySpool
{
public:
    ~TemporaryGeometrySpool()
    {
	if (!path.empty())
	    bu_file_delete(path.c_str());
    }

    bool CreatePath()
    {
	static std::atomic<uint64_t> sequence(0);
	const bu_dir_t locations[] = {BU_DIR_CACHE, BU_DIR_TEMP};
	for (const bu_dir_t location : locations) {
	    char directory[MAXPATHLEN] = {0};
	    if (!bu_dir(directory, sizeof(directory), location,
		    static_cast<const char *>(NULL)))
		continue;
	    bu_mkdir(directory);
	    if (!bu_file_directory(directory) || !bu_file_writable(directory))
		continue;

	    const uint64_t first_suffix = sequence.fetch_add(1,
		std::memory_order_relaxed);
	    for (uint64_t attempt = 0;
		    attempt < kMaximumGeometrySpoolNameAttempts; ++attempt) {
		std::ostringstream candidate;
		candidate << directory << "/step-import-" << bu_pid() << '-'
		    << (first_suffix + attempt) << ".g";
		if (!bu_file_exists(candidate.str().c_str(), NULL)) {
		    /* Directory mode checks can report writable for a path that a
		     * container policy or network filesystem rejects at open time.
		     * Probe the actual candidate before committing to this location so
		     * BU_DIR_TEMP remains a functional fallback. */
		    std::ofstream probe(candidate.str().c_str(),
			std::ios::out | std::ios::binary | std::ios::trunc);
		    if (!probe.is_open())
			continue;
		    probe.close();
		    if (!bu_file_delete(candidate.str().c_str()))
			continue;
		    path = candidate.str();
		    return true;
		}
	    }
	}
	return false;
    }

    std::string path;
};

/* A single invalid item must not monopolize an import indefinitely.  Thirty
 * seconds flags an exact solid as an investigation case before it consumes a
 * full minute; a shell surface model gets two minutes because one AP item may
 * intentionally contain many disconnected shells.  These are elapsed-work
 * ceilings, not acceptance tolerances. */
constexpr uint64_t kMaximumExactPullbackMilliseconds = 30000;
/* Large STEP solids may intentionally contain hundreds or thousands of faces
 * under one representation item.  Once topology is detached and countable,
 * add 500 ms per face to a 60-second complex-item overhead allowance, with a
 * 15-minute absolute ceiling.  The fixed allowance covers dependency
 * materialization, final topology validation, and bounded contention while
 * other top-level geometry workers occupy the shared helper pool.  Items at
 * or below 200 faces retain the strict 30-second investigation budget.
 * Crossing one minute is therefore limited to an explicitly reported,
 * measured-topology complexity exception; the ceiling still prevents a
 * malformed giant item from monopolizing an import indefinitely. */
constexpr uint64_t kMaximumComplexExactPullbackMilliseconds = 15 * 60 * 1000;
constexpr uint64_t kComplexExactOverheadMilliseconds =
    2 * kMaximumExactPullbackMilliseconds;
constexpr uint64_t kExactPullbackMillisecondsPerFace = 500;
constexpr size_t kComplexExactSolidFaceThreshold = 200;
constexpr uint64_t kMaximumSurfaceModelPullbackMilliseconds = 120000;

class PullbackWorkScope {
public:
    PullbackWorkScope(STEPWrapper *source, uint64_t maximum_elapsed_milliseconds)
	: wrapper(source)
    {
	brlcad::SetPullbackWorkLimit(CancellationRequested, wrapper,
	    maximum_elapsed_milliseconds);
    }
    ~PullbackWorkScope() { brlcad::ClearPullbackWorkLimit(); }
    bool DeadlineExpired() const { return brlcad::PullbackWorkDeadlineExpired(); }

private:
    static bool CancellationRequested(void *context)
    {
	STEPWrapper *source = static_cast<STEPWrapper *>(context);
	return source && source->CancellationRequested();
    }
    STEPWrapper *wrapper;
};

}

struct STEPWrapper::GeometryExecutor {
    struct Group {
	Group(const std::function<void(size_t)> &caller,
	    const std::function<void(size_t)> &helper, size_t task_count)
	    : caller_task(caller), helper_task(helper), count(task_count)
	{
	}

	bool Claim(size_t &index)
	{
	    std::lock_guard<std::mutex> guard(lock);
	    if (next >= count) return false;
	    index = next++;
	    ++running;
	    return true;
	}

	bool HasUnclaimed()
	{
	    std::lock_guard<std::mutex> guard(lock);
	    return next < count;
	}

	void Finish(std::exception_ptr failure = std::exception_ptr())
	{
	    {
		std::lock_guard<std::mutex> guard(lock);
		if (failure && !exception) exception = failure;
		if (running) --running;
	    }
	    finished.notify_all();
	}

	void Wait()
	{
	    std::unique_lock<std::mutex> guard(lock);
	    finished.wait(guard, [this]() { return next >= count && running == 0; });
	    if (exception) std::rethrow_exception(exception);
	}

	std::function<void(size_t)> caller_task;
	std::function<void(size_t)> helper_task;
	size_t count = 0;
	size_t next = 0;
	size_t running = 0;
	std::exception_ptr exception;
	std::mutex lock;
	std::condition_variable finished;
    };

    GeometryExecutor(STEPWrapper *source, unsigned int maximum_concurrency)
	: wrapper(source), capacity(std::max(1U, maximum_concurrency))
    {
	for (unsigned int i = 1; i < capacity; ++i)
	    helpers.emplace_back(&GeometryExecutor::RunHelper, this);
    }

    ~GeometryExecutor()
    {
	Shutdown();
    }

    void Shutdown()
    {
	{
	    std::lock_guard<std::mutex> guard(lock);
	    stopping = true;
	}
	changed.notify_all();
	for (std::thread &helper : helpers) {
	    if (helper.joinable()) helper.join();
	}
	helpers.clear();
	if (wrapper) wrapper->SetGeometryHelpersActive(0);
    }

    void WorkerStarted()
    {
	std::unique_lock<std::mutex> guard(lock);
	/* A helper may have borrowed a slot while the materializer was still
	 * filling the top-level queue.  Wait for that bounded helper task to
	 * return the slot before admitting another solid worker, keeping total
	 * geometry CPU occupancy at or below -j even during ramp-up. */
	changed.wait(guard, [this]() {
	    return stopping || top_level_workers + active_helpers < capacity;
	});
	if (!stopping) ++top_level_workers;
	guard.unlock();
	changed.notify_all();
    }

    void WorkerFinished()
    {
	{
	    std::lock_guard<std::mutex> guard(lock);
	    if (top_level_workers) --top_level_workers;
	}
	changed.notify_all();
    }

    void ParallelFor(size_t count, const std::function<void(size_t)> &caller_task,
	const std::function<void(size_t)> &helper_task)
    {
	if (count < 2 || capacity < 2) {
	    for (size_t index = 0; index < count; ++index) caller_task(index);
	    return;
	}
	std::shared_ptr<Group> group(new Group(caller_task, helper_task, count));
	{
	    std::lock_guard<std::mutex> guard(lock);
	    groups.push_back(group);
	}
	changed.notify_all();
	size_t index = 0;
	while (group->Claim(index)) {
	    try {
		group->caller_task(index);
		group->Finish();
	    } catch (...) {
		group->Finish(std::current_exception());
	    }
	}
	group->Wait();
    }

private:
    void RemoveCompletedGroupsLocked()
    {
	for (std::deque<std::shared_ptr<Group> >::iterator group = groups.begin();
		group != groups.end();) {
	    if (!*group || !(*group)->HasUnclaimed())
		group = groups.erase(group);
	    else
		++group;
	}
    }

    void RunHelper()
    {
	for (;;) {
	    std::shared_ptr<Group> group;
	    size_t index = 0;
	    uint64_t active_snapshot = 0;
	    {
		std::unique_lock<std::mutex> guard(lock);
		for (;;) {
		    RemoveCompletedGroupsLocked();
		    if (stopping) return;
		    if (!groups.empty() &&
			    top_level_workers + active_helpers < capacity)
			break;
		    changed.wait(guard);
		}
		group = groups.front();
		groups.pop_front();
		if (!group || !group->Claim(index)) continue;
		if (group->HasUnclaimed()) groups.push_back(group);
		++active_helpers;
		active_snapshot = active_helpers;
	    }
	    if (wrapper) wrapper->SetGeometryHelpersActive(active_snapshot);
	    std::exception_ptr failure;
	    try {
		group->helper_task(index);
	    } catch (...) {
		failure = std::current_exception();
	    }
	    group->Finish(failure);
	    {
		std::lock_guard<std::mutex> guard(lock);
		if (active_helpers) --active_helpers;
		active_snapshot = active_helpers;
	    }
	    if (wrapper) wrapper->SetGeometryHelpersActive(active_snapshot);
	    changed.notify_all();
	}
    }

    STEPWrapper *wrapper = NULL;
    unsigned int capacity = 1;
    std::mutex lock;
    std::condition_variable changed;
    std::deque<std::shared_ptr<Group> > groups;
    std::vector<std::thread> helpers;
    unsigned int top_level_workers = 0;
    unsigned int active_helpers = 0;
    bool stopping = false;
};


static bool
geometry_helper_cancelled(void *context)
{
    STEPWrapper *wrapper = static_cast<STEPWrapper *>(context);
    return wrapper && wrapper->CancellationRequested();
}

STEPWrapper::STEPWrapper()
    : registry(NULL), sfile(NULL), dotg(NULL), verbose(false)
{
    int ownsInstanceMemory = 1;
    instance_list = new InstMgr(ownsInstanceMemory);
}


STEPWrapper::~STEPWrapper()
{
    StopGeometryExecutor();
    ClearEntityCache();
#ifdef HAVE_STEPCODE_LAZY
    releaseLazyBatches();
    lazy_session.reset();
#endif
    delete sfile;
    delete instance_list;
    delete registry;
    dotg = NULL;
}


void
STEPWrapper::ConfigureGeometryExecutor(unsigned int concurrency)
{
    StopGeometryExecutor();
    if (concurrency > 1)
	geometry_executor.reset(new GeometryExecutor(this, concurrency));
}


void
STEPWrapper::StopGeometryExecutor()
{
    if (!geometry_executor) return;
    geometry_executor->Shutdown();
    geometry_executor.reset();
}


void
STEPWrapper::GeometryWorkerStarted()
{
    if (geometry_executor) geometry_executor->WorkerStarted();
}


void
STEPWrapper::GeometryWorkerFinished()
{
    if (geometry_executor) geometry_executor->WorkerFinished();
}


void
STEPWrapper::ParallelForGeometry(size_t count,
    const std::function<void(size_t)> &task)
{
    if (!geometry_executor || count < 2) {
	for (size_t index = 0; index < count; ++index) task(index);
	return;
    }
    const uint64_t remaining = brlcad::PullbackWorkRemainingMilliseconds();
    const double length = LocalUnits::length;
    const double planeangle = LocalUnits::planeangle;
    const double solidangle = LocalUnits::solidangle;
    const double tolerance = LocalUnits::tolerance;
    const std::function<void(size_t)> helper_task = [this, task, remaining,
	length, planeangle, solidangle, tolerance](size_t index) {
	LocalUnits::length = length;
	LocalUnits::planeangle = planeangle;
	LocalUnits::solidangle = solidangle;
	LocalUnits::tolerance = tolerance;
	const uint64_t helper_budget = remaining == UINT64_MAX ? 0 :
	    std::max<uint64_t>(1, remaining);
	brlcad::SetPullbackWorkLimit(geometry_helper_cancelled, this,
	    helper_budget);
	try {
	    task(index);
	} catch (...) {
	    brlcad::ClearPullbackWorkLimit();
	    throw;
	}
	brlcad::ClearPullbackWorkLimit();
    };
    geometry_executor->ParallelFor(count, task, helper_task);
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
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session)
	return static_cast<int>(lazy_filter_active ? lazy_iteration_ids.size() : lazy_instance_ids.size());
#endif
    return instance_list ? instance_list->InstanceCount() : 0;
}


SDAI_Application_instance *
STEPWrapper::InstanceAt(int index)
{
    if (index < 0 || index >= InstanceCount()) return NULL;
#ifdef HAVE_STEPCODE_LAZY
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
#ifdef HAVE_STEPCODE_LAZY
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
STEPWrapper::SetInstanceIds(const std::vector<uint64_t> &ids)
{
#ifdef HAVE_STEPCODE_LAZY
    if (!lazy_session) return;
    releaseLazyBatches();
    const std::set<uint64_t> selected(ids.begin(), ids.end());
    lazy_iteration_ids.clear();
    lazy_iteration_ids.reserve(selected.size());
    for (std::vector<uint64_t>::const_iterator id = lazy_instance_ids.begin();
	 id != lazy_instance_ids.end(); ++id) {
	if (selected.find(*id) != selected.end())
	    lazy_iteration_ids.push_back(*id);
    }
    lazy_filter_active = true;
#else
    (void)ids;
#endif
}


void
STEPWrapper::ResetInstanceTypes()
{
#ifdef HAVE_STEPCODE_LAZY
    if (!lazy_session) return;
    releaseLazyBatches();
    lazy_iteration_ids.clear();
    lazy_filter_active = false;
#endif
}


bool
STEPWrapper::HasLazyIndex() const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session.get() != NULL;
#else
    return false;
#endif
}


std::vector<uint64_t>
STEPWrapper::LazyInstancesByType(const std::string &type) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->InstancesByType(type) : std::vector<uint64_t>();
#else
    (void)type;
    return std::vector<uint64_t>();
#endif
}


std::string
STEPWrapper::LazyTypeName(uint64_t id) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->TypeName(id) : std::string();
#else
    (void)id;
    return std::string();
#endif
}


std::string
STEPWrapper::LazySourceRecord(uint64_t id) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->SourceRecord(id) : std::string();
#else
    (void)id;
    return std::string();
#endif
}


std::vector<uint64_t>
STEPWrapper::LazyForwardReferences(uint64_t id) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->ForwardReferences(id) : std::vector<uint64_t>();
#else
    (void)id;
    return std::vector<uint64_t>();
#endif
}


std::vector<uint64_t>
STEPWrapper::LazyReverseReferences(uint64_t id) const
{
#ifdef HAVE_STEPCODE_LAZY
    return lazy_session ? lazy_session->ReverseReferences(id) : std::vector<uint64_t>();
#else
    (void)id;
    return std::vector<uint64_t>();
#endif
}


InstMgrBase *
STEPWrapper::referenceManager() const
{
#ifdef HAVE_STEPCODE_LAZY
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


#ifdef HAVE_STEPCODE_LAZY
static void
update_lazy_statistics(brlcad::step::ImportStatistics &statistics,
    const brlcad::step::STEPLazyStatistics &cache)
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
STEPWrapper::recordLazyDiagnostic(const brlcad::step::STEPLazyDiagnostic &source)
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
    const std::vector<brlcad::step::STEPLazyDiagnostic> &source =
	lazy_session->Diagnostics();
    for (std::vector<brlcad::step::STEPLazyDiagnostic>::const_iterator diagnostic =
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
    const brlcad::step::STEPLazyStatistics cache = lazy_session->Statistics();
    update_lazy_statistics(statistics, cache);
    synchronizeLazyDiagnostics();
}


SDAI_Application_instance *
STEPWrapper::activateLazyRoot(uint64_t id)
{
    releaseLazyBatches();
    lazy_batch.reset(new brlcad::step::STEPLazyBatch(lazy_session->LoadBatch(id)));
    if (!lazy_batch->Valid()) return NULL;
    const brlcad::step::STEPLazyStatistics cache = lazy_session->Statistics();
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
#ifdef HAVE_STEPCODE_LAZY
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
STEPWrapper::SetProgress(const std::string &phase, uint64_t completed,
    uint64_t total, int64_t current_entity_id, uint64_t secondary_completed,
    const std::string &secondary_label, const std::string &detail)
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.phase = phase;
    progress_state.completed = completed;
    progress_state.total = total;
    progress_state.current_entity_id = current_entity_id;
    progress_state.secondary_completed = secondary_completed;
    progress_state.secondary_total = 0;
    progress_state.secondary_label = secondary_label;
    progress_state.detail = detail;
}


void
STEPWrapper::SetProgressDetail(const std::string &phase,
    int64_t current_entity_id, uint64_t secondary_completed,
    uint64_t secondary_total, const std::string &secondary_label,
    const std::string &detail)
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.phase = phase;
    progress_state.current_entity_id = current_entity_id;
    progress_state.secondary_completed = secondary_completed;
    progress_state.secondary_total = secondary_total;
    progress_state.secondary_label = secondary_label;
    progress_state.detail = detail;
}


void
STEPWrapper::SetGeometrySchedulerProgress(uint64_t queued, uint64_t active,
    uint64_t ready, uint64_t spooled, uint64_t finished,
    uint64_t materializing, uint64_t in_flight,
    uint64_t runnable_capacity, uint64_t ready_bytes,
    uint64_t ready_byte_budget)
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.geometry_jobs_queued = queued;
    progress_state.geometry_workers_active = active;
    progress_state.geometry_jobs_ready = ready;
    progress_state.geometry_jobs_spooled = spooled;
    progress_state.geometry_jobs_finished = finished;
    progress_state.geometry_jobs_materializing = materializing;
    progress_state.geometry_jobs_in_flight = in_flight;
    progress_state.geometry_runnable_capacity = runnable_capacity;
    progress_state.geometry_ready_bytes = ready_bytes;
    progress_state.geometry_ready_byte_budget = ready_byte_budget;
}


void
STEPWrapper::SetGeometryHelpersActive(uint64_t active)
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.geometry_helpers_active = active;
}


brlcad::step::ImportProgress
STEPWrapper::Progress() const
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    return progress_state;
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
    document.entity_counts_complete = true;
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
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) {
	/* --entity is explicitly a focused conversion/debugging request.  Do not
	 * add a second O(file-instances) type walk after indexing a million-item
	 * assembly merely to populate whole-file report coverage. */
	if (!import_options.selected_entity_ids.empty()) {
	    document.entity_counts_complete = false;
	    for (std::set<int64_t>::const_iterator id =
		    import_options.selected_entity_ids.begin();
		 id != import_options.selected_entity_ids.end(); ++id) {
		if (*id <= 0) continue;
		std::string type = lazy_session->TypeName(
		    static_cast<uint64_t>(*id));
		if (!type.empty()) record_type(type);
	    }
	    SetProgress("counted selected STEP entity types",
		import_options.selected_entity_ids.size(),
		import_options.selected_entity_ids.size());
	    return;
	}
	uint64_t completed = 0;
	for (std::vector<uint64_t>::const_iterator id = lazy_instance_ids.begin();
	     id != lazy_instance_ids.end(); ++id) {
	    std::string type = lazy_session->TypeName(*id);
	    if (type.empty()) type = "COMPLEX_ENTITY";
	    record_type(type);
	    ++completed;
	    if ((completed & 0x3fff) == 0)
		SetProgress("counting STEP entity types", completed,
		    statistics.input_instances, static_cast<int64_t>(*id));
	}
	SetProgress("counting STEP entity types", completed,
	    statistics.input_instances);
	return;
    }
#endif
    for (int i = 0; i < InstanceCount(); ++i) {
	SDAI_Application_instance *instance = InstanceAt(i);
	if (!instance || !instance->EntityName()) continue;
	record_type(instance->EntityName());
	if ((i & 0x3fff) == 0)
	    SetProgress("counting STEP entity types", static_cast<uint64_t>(i),
		statistics.input_instances, instance->STEPfile_id);
    }
	SetProgress("counting STEP entity types", statistics.input_instances,
	    statistics.input_instances);
}


double
STEPWrapper::deriveTolerance()
{
    if (import_options.absolute_tolerance_mm > 0.0)
	return import_options.absolute_tolerance_mm;

    double result = 0.0;
#ifdef HAVE_STEPCODE_LAZY
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
	/* Both trims use the same edge.  Leave the edge in place while removing
	 * the first trim, then let the final trim removal delete the now-unused
	 * edge.  DeleteTrim only marks topology; Compact performs the remap after
	 * both original trim indices have been consumed. */
	brep->DeleteTrim(brep->m_T[first_index], false);
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
	ON_BrepTrim &singular_trim = brep->NewSingularTrim(
	    brep->m_V[outside_singular.vertex], *loop,
	    outside_singular.iso, outside_singular_c2);
	singular_trim.m_tolerance[0] = LocalUnits::tolerance;
	singular_trim.m_tolerance[1] = LocalUnits::tolerance;
	loop = &brep->m_L[loop_index];
	new_loop = &brep->m_L[new_loop_index];
    }
    if (insert_inside_singular) {
	ON_BrepTrim &singular_trim = brep->NewSingularTrim(
	    brep->m_V[inside_singular_vertex], *new_loop,
	    inside_singular.iso, inside_singular_c2);
	singular_trim.m_tolerance[0] = LocalUnits::tolerance;
	singular_trim.m_tolerance[1] = LocalUnits::tolerance;
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
	if (brlcad::PullbackWorkCancelled())
	    return classified;
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


static void
refresh_brep_flags_preserving_singular_isos(ON_Brep *brep,
	bool set_loop_type)
{
    if (!brep)
	return;

    std::vector<ON_Surface::ISO> proven_singular_isos(
	brep->m_T.Count(), ON_Surface::not_iso);

    /* Recover or re-prove a boundary flag only from the singular trim itself:
     * its complete 2-D locus must be constant on one domain boundary, vary
     * along that boundary, and densely lift to its one topology vertex. */
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	if (brlcad::PullbackWorkCancelled())
	    return;
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_type != ON_BrepTrim::singular ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
		trim.m_vi[0] < 0 || trim.m_vi[0] >= brep->m_V.Count())
	    continue;
	const ON_BrepLoop &loop = brep->m_L[trim.m_li];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	const ON_Interval trim_domain = trim.Domain();
	if (!surface || !trim_domain.IsIncreasing())
	    continue;
	const ON_3dPoint vertex = brep->m_V[trim.m_vi[0]].point;
	ON_Surface::ISO recovered = ON_Surface::not_iso;
	int recovered_fixed_direction = -1;
	double recovered_boundary = 0.0;
	for (int fixed_direction = 0; fixed_direction < 2; ++fixed_direction) {
	    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	    const ON_Interval varying_domain = surface->Domain(1 - fixed_direction);
	    if (!fixed_domain.IsIncreasing() || !varying_domain.IsIncreasing())
		continue;
	    const double parameter_epsilon = std::max(ON_ZERO_TOLERANCE,
		fixed_domain.Length() * 1.0e-10);
	    for (int side = 0; side < 2; ++side) {
		const double boundary = fixed_domain[side];
		bool exact_boundary = true;
		double varying_minimum = DBL_MAX;
		double varying_maximum = -DBL_MAX;
		for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint uv = trim.PointAt(
			trim_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    if (!uv.IsValid() || fabs(uv[fixed_direction] - boundary) >
			    parameter_epsilon || !lift.IsValid() ||
			    lift.DistanceTo(vertex) > LocalUnits::tolerance) {
			exact_boundary = false;
			break;
		    }
		    varying_minimum = std::min(varying_minimum,
			uv[1 - fixed_direction]);
		    varying_maximum = std::max(varying_maximum,
			uv[1 - fixed_direction]);
		}
		if (!exact_boundary || varying_maximum - varying_minimum <=
			parameter_epsilon)
		    continue;
		const ON_Surface::ISO candidate = fixed_direction == 0 ?
		    (side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
		    (side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		if (recovered != ON_Surface::not_iso && recovered != candidate) {
		    recovered = ON_Surface::not_iso;
		    exact_boundary = false;
		    break;
		}
		recovered = candidate;
		recovered_fixed_direction = fixed_direction;
		recovered_boundary = boundary;
		}
	    }
	/* A singular trim created on a proven collapsed side can subsequently sit
	 * just outside OpenNURBS' very small parameter-boundary window after exact
	 * seam and domain normalization.  Use its existing side only as a proposal:
	 * both the supplied locus and the exact snapped boundary must densely lift
	 * to the same topology vertex before accepting it. */
	if (recovered == ON_Surface::not_iso &&
		(trim.m_iso == ON_Surface::W_iso ||
		 trim.m_iso == ON_Surface::S_iso ||
		 trim.m_iso == ON_Surface::E_iso ||
		 trim.m_iso == ON_Surface::N_iso)) {
	    const bool fixed_u = trim.m_iso == ON_Surface::W_iso ||
		trim.m_iso == ON_Surface::E_iso;
	    const int fixed_direction = fixed_u ? 0 : 1;
	    const int side = (trim.m_iso == ON_Surface::E_iso ||
		trim.m_iso == ON_Surface::N_iso) ? 1 : 0;
	    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	    ON_3dPoint start = trim.PointAtStart();
	    ON_3dPoint end = trim.PointAtEnd();
	    start[fixed_direction] = fixed_domain[side];
	    end[fixed_direction] = fixed_domain[side];
	    ON_LineCurve boundary_candidate(start, end);
	    bool exact = fixed_domain.IsIncreasing() &&
		boundary_candidate.ChangeDimension(2) &&
		boundary_candidate.SetDomain(trim_domain.Min(), trim_domain.Max()) &&
		boundary_candidate.IsValid() &&
		surface->IsIsoparametric(boundary_candidate, &trim_domain) == trim.m_iso;
	    for (int sample = 0; exact && sample <= kDenseValidationSegments;
		    sample++) {
		const double parameter = trim_domain.ParameterAt(
		    static_cast<double>(sample) / kDenseValidationSegments);
		const ON_3dPoint original_uv = trim.PointAt(parameter);
		const ON_3dPoint boundary_uv = boundary_candidate.PointAt(parameter);
		const ON_3dPoint original_lift = surface->PointAt(
		    original_uv.x, original_uv.y);
		const ON_3dPoint boundary_lift = surface->PointAt(
		    boundary_uv.x, boundary_uv.y);
		exact = original_lift.IsValid() && boundary_lift.IsValid() &&
		    original_lift.DistanceTo(vertex) <= LocalUnits::tolerance &&
		    boundary_lift.DistanceTo(vertex) <= LocalUnits::tolerance;
	    }
	    if (exact) {
		recovered = trim.m_iso;
		recovered_fixed_direction = fixed_direction;
		recovered_boundary = fixed_domain[side];
	    }
	}
	if (recovered != ON_Surface::not_iso) {
	    trim.m_iso = recovered;
	    proven_singular_isos[ti] = recovered;
	    /* A pcurve can be within our scale-aware parameter epsilon of a
	     * collapsed side while still missing OpenNURBS' stricter boundary
	     * classification.  Merely restoring the side flag then makes the trim
	     * self-inconsistent.  Once the complete source locus has been proven to
	     * lift to the one singular vertex, replace it with the exact boundary
	     * line between the same endpoints.  This changes no 3-D geometry and
	     * lets the subsequent derived-flag refresh independently recover the
	     * same W/S/E/N classification. */
	    const ON_Curve *trim_curve = trim.TrimCurveOf();
	    if (recovered_fixed_direction >= 0 && trim_curve &&
		    surface->IsIsoparametric(*trim_curve, &trim_domain) != recovered) {
		ON_3dPoint start = trim.PointAtStart();
		ON_3dPoint end = trim.PointAtEnd();
		start[recovered_fixed_direction] = recovered_boundary;
		end[recovered_fixed_direction] = recovered_boundary;
		std::unique_ptr<ON_LineCurve> boundary_curve(
		    new ON_LineCurve(start, end));
		bool exact = boundary_curve->ChangeDimension(2) &&
		    boundary_curve->SetDomain(trim_domain.Min(), trim_domain.Max()) &&
		    boundary_curve->IsValid() &&
		    surface->IsIsoparametric(*boundary_curve, &trim_domain) == recovered;
		for (int sample = 0; exact && sample <= kDenseValidationSegments;
			sample++) {
		    const ON_3dPoint uv = boundary_curve->PointAt(
			trim_domain.ParameterAt(static_cast<double>(sample) /
			    kDenseValidationSegments));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    exact = lift.IsValid() &&
			lift.DistanceTo(vertex) <= LocalUnits::tolerance;
		}
		if (exact) {
		    const int c2_index = brep->AddTrimCurve(boundary_curve.release());
		    if (c2_index >= 0)
			(void)brep->SetTrimCurve(trim, c2_index);
		}
	    }
	}
    }

    /* NewSingularTrim receives a boundary ISO only after its caller has
     * proved the complete pcurve lies on a collapsed surface side.  The
     * generic flag refresh cannot infer a direction from every degenerate
     * curve and may replace that boundary with not_iso. */
    if (brlcad::PullbackWorkCancelled())
	return;
    brep->SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, set_loop_type, true);
    const int count = std::min(brep->m_T.Count(),
	static_cast<int>(proven_singular_isos.size()));
    for (int ti = 0; ti < count; ++ti) {
	if (proven_singular_isos[ti] != ON_Surface::not_iso &&
		brep->m_T[ti].m_type == ON_BrepTrim::singular)
	    brep->m_T[ti].m_iso = proven_singular_isos[ti];
    }
}


/* OpenNURBS' derived-flag refresh assumes every topology array reference is
 * in range and reciprocal.  A half-built exact item must be rejected before
 * calling it: SetTrimIsoFlags() indexes face loops and loop trims without
 * bounds checks, so treating IsValid() as the first structural guard is too
 * late.  This is deliberately a proof-only preflight; it never repairs or
 * removes source topology. */
static bool
brep_topology_references_are_safe(const ON_Brep *brep, std::string *failure)
{
    if (failure)
	failure->clear();
    const auto reject = [failure](const std::string &message) {
	if (failure)
	    *failure = message;
	return false;
    };
    if (!brep)
	return reject("null BREP");

    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.m_face_index != fi)
	    return reject("face F" + std::to_string(fi) +
		" has inconsistent self index " +
		std::to_string(face.m_face_index));
	if (face.m_si < 0 || face.m_si >= brep->m_S.Count() ||
		!brep->m_S[face.m_si])
	    return reject("face F" + std::to_string(fi) +
		" references invalid surface " + std::to_string(face.m_si));
	if (face.ProxySurface() != brep->m_S[face.m_si])
	    return reject("face F" + std::to_string(fi) +
		" has a surface proxy inconsistent with surface " +
		std::to_string(face.m_si));
	for (int fli = 0; fli < face.m_li.Count(); ++fli) {
	    const int li = face.m_li[fli];
	    if (li < 0 || li >= brep->m_L.Count())
		return reject("face F" + std::to_string(fi) +
		    " references invalid loop " + std::to_string(li));
	    if (brep->m_L[li].m_fi != fi)
		return reject("face F" + std::to_string(fi) + " loop L" +
		    std::to_string(li) + " points to face F" +
		    std::to_string(brep->m_L[li].m_fi));
	}
    }

    for (int li = 0; li < brep->m_L.Count(); ++li) {
	const ON_BrepLoop &loop = brep->m_L[li];
	if (loop.m_loop_index != li)
	    return reject("loop L" + std::to_string(li) +
		" has inconsistent self index " +
		std::to_string(loop.m_loop_index));
	if (loop.m_fi < 0 || loop.m_fi >= brep->m_F.Count())
	    return reject("loop L" + std::to_string(li) +
		" references invalid face " + std::to_string(loop.m_fi));
	for (int lti = 0; lti < loop.m_ti.Count(); ++lti) {
	    const int ti = loop.m_ti[lti];
	    if (ti < 0 || ti >= brep->m_T.Count())
		return reject("loop L" + std::to_string(li) +
		    " references invalid trim " + std::to_string(ti));
	    if (brep->m_T[ti].m_li != li)
		return reject("loop L" + std::to_string(li) + " trim T" +
		    std::to_string(ti) + " points to loop L" +
		    std::to_string(brep->m_T[ti].m_li));
	}
    }

    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	const ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_trim_index != ti)
	    return reject("trim T" + std::to_string(ti) +
		" has inconsistent self index " +
		std::to_string(trim.m_trim_index));
	if (trim.m_li < 0 || trim.m_li >= brep->m_L.Count())
	    return reject("trim T" + std::to_string(ti) +
		" references invalid loop " + std::to_string(trim.m_li));
	if (trim.m_type != ON_BrepTrim::ptonsrf &&
		(trim.m_c2i < 0 || trim.m_c2i >= brep->m_C2.Count() ||
		 !brep->m_C2[trim.m_c2i]))
	    return reject("trim T" + std::to_string(ti) +
		" references invalid 2-D curve " + std::to_string(trim.m_c2i));
	for (int end = 0; end < 2; ++end) {
	    if (trim.m_vi[end] < 0 || trim.m_vi[end] >= brep->m_V.Count())
		return reject("trim T" + std::to_string(ti) +
		    " references invalid vertex " +
		    std::to_string(trim.m_vi[end]));
	}
	const bool edge_free = trim.m_type == ON_BrepTrim::singular ||
	    trim.m_type == ON_BrepTrim::ptonsrf;
	if (!edge_free && (trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count()))
	    return reject("trim T" + std::to_string(ti) +
		" references invalid edge " + std::to_string(trim.m_ei));
	if (trim.m_ei >= brep->m_E.Count())
	    return reject("trim T" + std::to_string(ti) +
		" references out-of-range edge " + std::to_string(trim.m_ei));
    }

    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_edge_index != ei)
	    return reject("edge E" + std::to_string(ei) +
		" has inconsistent self index " +
		std::to_string(edge.m_edge_index));
	if (edge.m_c3i < 0 || edge.m_c3i >= brep->m_C3.Count() ||
		!brep->m_C3[edge.m_c3i])
	    return reject("edge E" + std::to_string(ei) +
		" references invalid 3-D curve " + std::to_string(edge.m_c3i));
	for (int end = 0; end < 2; ++end) {
	    if (edge.m_vi[end] < 0 || edge.m_vi[end] >= brep->m_V.Count())
		return reject("edge E" + std::to_string(ei) +
		    " references invalid vertex " +
		    std::to_string(edge.m_vi[end]));
	}
	for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
	    const int ti = edge.m_ti[eti];
	    if (ti < 0 || ti >= brep->m_T.Count())
		return reject("edge E" + std::to_string(ei) +
		    " references invalid trim " + std::to_string(ti));
	    if (brep->m_T[ti].m_ei != ei)
		return reject("edge E" + std::to_string(ei) + " trim T" +
		    std::to_string(ti) + " points to edge E" +
		    std::to_string(brep->m_T[ti].m_ei));
	}
    }

    for (int vi = 0; vi < brep->m_V.Count(); ++vi) {
	const ON_BrepVertex &vertex = brep->m_V[vi];
	if (vertex.m_vertex_index != vi)
	    return reject("vertex V" + std::to_string(vi) +
		" has inconsistent self index " +
		std::to_string(vertex.m_vertex_index));
	for (int vei = 0; vei < vertex.m_ei.Count(); ++vei) {
	    const int ei = vertex.m_ei[vei];
	    if (ei < 0 || ei >= brep->m_E.Count())
		return reject("vertex V" + std::to_string(vi) +
		    " references invalid edge " + std::to_string(ei));
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    if (edge.m_vi[0] != vi && edge.m_vi[1] != vi)
		return reject("vertex V" + std::to_string(vi) + " edge E" +
		    std::to_string(ei) + " has different endpoint vertices");
	}
    }
    return true;
}


static bool
regenerate_collapsed_periodic_boundary(ON_Brep *brep, ON_BrepLoop &loop,
	const ON_Surface *surface, int closed_direction, double parameter_tolerance,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	double *proven_open_parameter = NULL)
{
    if (!brep || !surface || !wrapper || loop.TrimCount() != 1 ||
	    !surface->IsClosed(closed_direction))
	return false;
    ON_BrepTrim *trim = loop.Trim(0);
    ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
    if (!trim || !edge || edge->m_vi[0] != edge->m_vi[1] ||
	    trim->m_vi[0] != trim->m_vi[1] || trim->m_vi[0] < 0 ||
	    trim->m_vi[0] >= brep->m_V.Count() || !trim->TrimCurveOf())
	return false;
    const int open_direction = 1 - closed_direction;
    const ON_3dPoint supplied_start = trim->PointAtStart();
    const ON_3dPoint supplied_end = trim->PointAtEnd();
    if (!supplied_start.IsValid() || !supplied_end.IsValid() ||
	    fabs(supplied_start[open_direction] -
		supplied_end[open_direction]) > parameter_tolerance)
	return false;
    double open_parameter = 0.5 *
	(supplied_start[open_direction] + supplied_end[open_direction]);
    const ON_Interval closed_domain = surface->Domain(closed_direction);
    const auto reject = [wrapper, entity_id, &entity_type, &loop,
	    &open_parameter](const char *reason) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": collapsed periodic boundary L" << loop.m_loop_index
		<< "@" << open_parameter << " rejected: " << reason << std::endl;
	return false;
    };
    if (!closed_domain.IsIncreasing())
	return reject("the closed surface domain was invalid");

    ON_NurbsCurve edge_nurbs;
    if (!edge->GetNurbForm(edge_nurbs))
	return reject("the STEP edge had no NURBS form");

    const double existing_tolerance = std::max(LocalUnits::tolerance,
	std::max(edge->m_tolerance,
	    std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
    const ON_BoundingBox edge_bounds = edge_nurbs.BoundingBox();
    const double edge_scale = edge_bounds.IsValid() ?
	edge_bounds.Diagonal().Length() : 0.0;
    ON_BoundingBox item_bounds;
    const double item_scale = brep->GetBoundingBox(item_bounds, false) &&
	item_bounds.IsValid() ? item_bounds.Diagonal().Length() : 0.0;
    const double adjustment_limit = wrapper->ImportOptions().exact ?
	existing_tolerance : std::max(existing_tolerance,
	    std::max(edge_scale * kRegenerationMaximumRelativeMismatch,
		item_scale * kRegenerationMaximumRelativeItemMismatch));
    const ON_3dPoint &topology_vertex = brep->m_V[trim->m_vi[0]].point;

    const ON_Interval edge_domain = edge_nurbs.Domain();
    bool same_locus = edge_domain.IsIncreasing();
    double tangent_alignment = 0.0;
    bool have_tangent_alignment = false;
    brlcad::PullbackContext pullback_context;
    int failed_sample = -1;
    double failed_edge_to_surface = DBL_MAX;
    double maximum_edge_to_surface = 0.0;
    double first_closed_parameter = 0.0;
    double previous_closed_parameter = 0.0;
    double final_closed_parameter = 0.0;
    double minimum_closed_parameter = DBL_MAX;
    double maximum_closed_parameter = -DBL_MAX;
    bool have_closed_parameter = false;
    double maximum_parameter_error = parameter_tolerance;
    const ON_Interval open_domain = surface->Domain(open_direction);
    const bool open_is_closed = surface->IsClosed(open_direction) &&
	open_domain.IsIncreasing();
    const double open_period = open_is_closed ? open_domain.Length() : 0.0;
    double minimum_open_parameter = DBL_MAX;
    double maximum_open_parameter = -DBL_MAX;
    double maximum_open_parameter_error = parameter_tolerance;
    bool have_open_parameter = false;
    std::vector<ON_2dPoint> projected_parameters;
    std::vector<ON_3dPoint> edge_points;
    projected_parameters.reserve(kDenseValidationSegments + 1);
    edge_points.reserve(kDenseValidationSegments + 1);
    for (int sample = 0; same_locus && sample <= kDenseValidationSegments;
	    ++sample) {
	if (brlcad::PullbackWorkCancelled()) {
	    same_locus = false;
	    break;
	}
	const double fraction = static_cast<double>(sample) /
	    kDenseValidationSegments;
	const ON_3dPoint edge_point = edge_nurbs.PointAt(
	    edge_domain.ParameterAt(fraction));
	ON_2dPoint pulled_uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint pulled_lift;
	double pulled_distance = DBL_MAX;
	const double projection_tolerance = std::max(ON_ZERO_TOLERANCE,
	    existing_tolerance);
	bool pulled = edge_point.IsValid() &&
	    pullback_context.SurfaceClosestPoint(surface, edge_point,
		pulled_uv, pulled_lift, pulled_distance, 0,
		std::max(ON_ZERO_TOLERANCE,
		    projection_tolerance * 0.1), projection_tolerance);
	/* The closest-point routine uses within_distance_tol as a search-window
	 * and completion threshold, not merely as an acceptance bound.  A broad
	 * safe-repair limit can therefore stop at the wrong parameter branch on a
	 * small-radius periodic surface.  Search at model tolerance first; only a
	 * genuinely separated source edge gets the broader bounded second pass. */
	if (!pulled && edge_point.IsValid())
	    pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
		pulled_uv, pulled_lift, pulled_distance, 0,
		std::max(ON_ZERO_TOLERANCE,
		    projection_tolerance * 0.1), adjustment_limit);
	pulled = pulled && pulled_distance <= adjustment_limit;
	if (!pulled) {
	    failed_sample = sample;
	    failed_edge_to_surface = pulled_distance;
	    same_locus = false;
	    break;
	}
	projected_parameters.push_back(pulled_uv);
	edge_points.push_back(edge_point);
	maximum_edge_to_surface = std::max(maximum_edge_to_surface,
	    pulled_distance);
	double closed_parameter = pulled_uv[closed_direction];
	const double period = closed_domain.Length();
	if (have_closed_parameter)
	    closed_parameter += round((previous_closed_parameter -
		closed_parameter) / period) * period;
	else
	    first_closed_parameter = closed_parameter;
	previous_closed_parameter = closed_parameter;
	final_closed_parameter = closed_parameter;
	minimum_closed_parameter = std::min(minimum_closed_parameter,
	    closed_parameter);
	maximum_closed_parameter = std::max(maximum_closed_parameter,
	    closed_parameter);
	have_closed_parameter = true;
	double projected_open = pulled_uv[open_direction];
	minimum_open_parameter = std::min(minimum_open_parameter,
	    projected_open);
	maximum_open_parameter = std::max(maximum_open_parameter,
	    projected_open);
	have_open_parameter = true;
	ON_3dPoint derivative_point;
	ON_3dVector du;
	ON_3dVector dv;
	const bool derivative_valid = surface->Ev1Der(pulled_uv.x, pulled_uv.y,
	    derivative_point, du, dv);
	if (derivative_valid) {
	    ON_3dVector closed_derivative = closed_direction == 0 ? du : dv;
	    ON_3dVector open_derivative = open_direction == 0 ? du : dv;
	    const double derivative_length = closed_derivative.Length();
	    if (derivative_length > ON_ZERO_TOLERANCE)
		maximum_parameter_error = std::max(maximum_parameter_error,
		    adjustment_limit / derivative_length * 1.05);
	    const double open_derivative_length = open_derivative.Length();
	    if (open_derivative_length > ON_ZERO_TOLERANCE)
		maximum_open_parameter_error = std::max(
		    maximum_open_parameter_error,
		    adjustment_limit / open_derivative_length * 1.05);
	}
	if (derivative_valid && !have_tangent_alignment && sample > 0 &&
		sample < kDenseValidationSegments) {
	    ON_3dVector surface_tangent = closed_direction == 0 ? du : dv;
	    ON_3dVector edge_tangent = edge_nurbs.TangentAt(
		edge_domain.ParameterAt(fraction));
	    if (surface_tangent.Unitize() && edge_tangent.Unitize()) {
		tangent_alignment = surface_tangent * edge_tangent;
		have_tangent_alignment = fabs(tangent_alignment) > 0.5;
	    }
	}
    }
    if (same_locus && have_open_parameter) {
	double maximum_open_deviation = 0.0;
	if (open_is_closed) {
	    double sine_sum = 0.0;
	    double cosine_sum = 0.0;
	    for (std::vector<ON_2dPoint>::const_iterator uv =
		    projected_parameters.begin(); uv != projected_parameters.end();
		    ++uv) {
		const double angle = 2.0 * ON_PI *
		    ((*uv)[open_direction] - open_domain.Min()) / open_period;
		sine_sum += sin(angle);
		cosine_sum += cos(angle);
	    }
	    double mean_angle = atan2(sine_sum, cosine_sum);
	    if (mean_angle < 0.0)
		mean_angle += 2.0 * ON_PI;
	    open_parameter = open_domain.Min() + open_period * mean_angle /
		(2.0 * ON_PI);
	    for (std::vector<ON_2dPoint>::const_iterator uv =
		    projected_parameters.begin(); uv != projected_parameters.end();
		    ++uv) {
		double deviation = fabs((*uv)[open_direction] - open_parameter);
		deviation -= floor(deviation / open_period) * open_period;
		deviation = std::min(deviation, open_period - deviation);
		maximum_open_deviation = std::max(maximum_open_deviation,
		    deviation);
	    }
	} else {
	    open_parameter = 0.5 * (minimum_open_parameter +
		maximum_open_parameter);
	    maximum_open_deviation = 0.5 *
		(maximum_open_parameter - minimum_open_parameter);
	}
	if (maximum_open_deviation > maximum_open_parameter_error) {
	    same_locus = false;
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": collapsed boundary fixed-parameter proof measured deviation "
		    << maximum_open_deviation << " with parameter tolerance "
		    << maximum_open_parameter_error << std::endl;
	}
    }
    for (size_t sample = 0; same_locus &&
	    sample < projected_parameters.size(); ++sample) {
	ON_3dPoint boundary_uv(projected_parameters[sample].x,
	    projected_parameters[sample].y, 0.0);
	boundary_uv[open_direction] = open_parameter;
	const ON_3dPoint boundary_lift = surface->PointAt(
	    boundary_uv.x, boundary_uv.y);
	const double edge_to_surface = boundary_lift.IsValid() ?
	    boundary_lift.DistanceTo(edge_points[sample]) : DBL_MAX;
	maximum_edge_to_surface = std::max(maximum_edge_to_surface,
	    edge_to_surface);
	if (!boundary_lift.IsValid() || edge_to_surface > adjustment_limit) {
	    failed_sample = static_cast<int>(sample);
	    failed_edge_to_surface = edge_to_surface;
	    same_locus = false;
	}
    }
	if (same_locus && have_closed_parameter) {
	    const double period = closed_domain.Length();
	    const double net_travel = final_closed_parameter -
		first_closed_parameter;
	    const double covered_span = maximum_closed_parameter -
		minimum_closed_parameter;
	    if (fabs(fabs(net_travel) - period) > maximum_parameter_error ||
		    fabs(covered_span - period) > maximum_parameter_error) {
		same_locus = false;
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": collapsed boundary winding proof measured net/span "
			<< net_travel << '/' << covered_span << " for period "
			<< period << " with parameter tolerance "
			<< maximum_parameter_error << std::endl;
	    }
	}
	if (!same_locus && wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": collapsed boundary dense locus failure sample "
		<< failed_sample << " edge-to-surface=" << failed_edge_to_surface
		<< " tolerance=" << LocalUnits::tolerance << std::endl;
    if (!same_locus)
	return reject("the STEP edge failed dense surface-locus or full-period winding validation");
    if (!have_tangent_alignment)
	return reject("the exact curve orientation could not be determined");
    if (projected_parameters.empty())
	return reject("the STEP edge produced no projected boundary samples");
    ON_3dPoint boundary_start(projected_parameters.front().x,
	projected_parameters.front().y, 0.0);
    ON_3dPoint boundary_end(projected_parameters.back().x,
	projected_parameters.back().y, 0.0);
    boundary_start[open_direction] = open_parameter;
    boundary_end[open_direction] = open_parameter;
    if (surface->PointAt(boundary_start.x, boundary_start.y).DistanceTo(
	    topology_vertex) > adjustment_limit ||
	    surface->PointAt(boundary_end.x, boundary_end.y).DistanceTo(
	    topology_vertex) > adjustment_limit)
	return reject("the proven periodic boundary did not lift to its closed-edge vertex");

    if (maximum_edge_to_surface > existing_tolerance) {
	const double adjusted = maximum_edge_to_surface *
	    kRegenerationToleranceSafety;
	if (wrapper->ImportOptions().exact || !(adjusted <= adjustment_limit))
	    return reject("the measured source edge/surface mismatch exceeded the bounded safe-repair limit");
	edge->m_tolerance = std::max(edge->m_tolerance, adjusted);
	trim->m_tolerance[0] = std::max(trim->m_tolerance[0], adjusted);
	trim->m_tolerance[1] = std::max(trim->m_tolerance[1], adjusted);
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    entity_id, entity_type, "trim_pcurve",
	    "source edge/surface separation exceeded the declared tolerance; "
	    "used a densely measured tolerance for exact periodic-band reconstruction");
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "adjusted one periodic-band trim tolerance to measured source geometry");
    }

    /* The supplied UV point is not necessarily the STEP edge's chosen
     * topology vertex.  Installing a full-period line here would therefore
     * attach its endpoints to the wrong 3-D point.  The caller uses this
     * proof to split the shared closed edge at the native seam and construct
     * two exact boundary uses that preserve both vertices. */
    (void)tangent_alignment;
    if (proven_open_parameter)
	*proven_open_parameter = open_parameter;
    return true;
}


/* Once dense projection has proven that a closed STEP edge winds one complete
 * surface period, it can be represented directly as a single pcurve only when
 * its topology vertex is already on the native surface seam.  Construct a
 * parameter-corresponding polyline in UV, try both windings, and validate both
 * every projection sample and every interpolated midpoint against the exact
 * directed 3-D edge.  A non-native vertex is deliberately left for the edge
 * splitter below. */
static bool
regenerate_native_seam_periodic_boundary(ON_Brep *brep, ON_BrepLoop &loop,
	const ON_Surface *surface, int closed_direction, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, double fixed_parameter)
{
    if (!brep || !surface || !wrapper || loop.TrimCount() != 1 ||
	    !surface->IsClosed(closed_direction))
	return false;
    ON_BrepTrim *trim = loop.Trim(0);
    ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
    if (!trim || !edge || edge->m_vi[0] != edge->m_vi[1] ||
	    trim->m_vi[0] != trim->m_vi[1] ||
	    trim->m_vi[0] < 0 || trim->m_vi[0] >= brep->m_V.Count())
	return false;
    const double topology_tolerance = std::max(LocalUnits::tolerance,
	std::max(edge->m_tolerance,
	    std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
    const int fixed_direction = 1 - closed_direction;
    const ON_Interval closed_domain = surface->Domain(closed_direction);
    const ON_Interval trim_domain = trim->Domain();
    const ON_Interval edge_domain = edge->Domain();
    const ON_3dPoint supplied_uv = trim->PointAtStart();
    if (!closed_domain.IsIncreasing() || !trim_domain.IsIncreasing() ||
	    !edge_domain.IsIncreasing() || !supplied_uv.IsValid())
	return false;
    const ON_3dPoint &vertex = brep->m_V[trim->m_vi[0]].point;
    ON_3dPoint native_min_uv = supplied_uv;
    ON_3dPoint native_max_uv = supplied_uv;
    native_min_uv[closed_direction] = closed_domain.Min();
    native_max_uv[closed_direction] = closed_domain.Max();
    native_min_uv[fixed_direction] = fixed_parameter;
    native_max_uv[fixed_direction] = fixed_parameter;
    const ON_3dPoint native_min_lift = surface->PointAt(
	native_min_uv.x, native_min_uv.y);
    const ON_3dPoint native_max_lift = surface->PointAt(
	native_max_uv.x, native_max_uv.y);
    if (!native_min_lift.IsValid() || !native_max_lift.IsValid() ||
	    native_min_lift.DistanceTo(vertex) > topology_tolerance ||
	    native_max_lift.DistanceTo(vertex) > topology_tolerance)
	return false;

    const double period = closed_domain.Length();
    brlcad::PullbackContext pullback_context;
    for (int winding = 0; winding < 2; ++winding) {
	const double desired_start = winding == 0 ? closed_domain.Min() :
	    closed_domain.Max();
	const double desired_end = winding == 0 ? closed_domain.Max() :
	    closed_domain.Min();
	ON_3dPointArray points;
	ON_SimpleArray<double> parameters;
	points.Reserve(kPeriodicBoundaryConstructionSegments + 1);
	parameters.Reserve(kPeriodicBoundaryConstructionSegments + 1);
	double previous_closed = desired_start;
	bool exact = true;
	const char *failure_reason = "none";
	int failure_sample = -1;
	double failure_distance = 0.0;
	for (int sample = 0; sample <= kPeriodicBoundaryConstructionSegments;
		++sample) {
	    if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled())
		return false;
	    const double fraction = static_cast<double>(sample) /
		kPeriodicBoundaryConstructionSegments;
	    const ON_3dPoint edge_point = edge->PointAt(
		edge_domain.ParameterAt(trim->m_bRev3d ?
		    1.0 - fraction : fraction));
	    ON_2dPoint pulled_uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint pulled_lift;
	    double pulled_distance = DBL_MAX;
	    if (!edge_point.IsValid() ||
		    !pullback_context.SurfaceClosestPoint(surface, edge_point,
			pulled_uv, pulled_lift, pulled_distance, 0,
			std::max(ON_ZERO_TOLERANCE,
			    topology_tolerance * 0.1),
		    topology_tolerance)) {
		exact = false;
		failure_reason = "projecting the exact STEP edge";
		failure_sample = sample;
		failure_distance = pulled_distance;
		break;
	    }
	    double closed_parameter = pulled_uv[closed_direction];
	    closed_parameter += round((previous_closed - closed_parameter) /
		period) * period;
	    previous_closed = closed_parameter;
	    ON_3dPoint uv;
	    uv[closed_direction] = closed_parameter;
	    uv[fixed_direction] = fixed_parameter;
	    uv.z = 0.0;
	    if (sample == 0)
		uv[closed_direction] = desired_start;
	    else if (sample == kPeriodicBoundaryConstructionSegments)
		uv[closed_direction] = desired_end;
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    if (!lift.IsValid() ||
		    lift.DistanceTo(edge_point) > topology_tolerance) {
		exact = false;
		failure_reason = "validating the fixed-parameter lift";
		failure_sample = sample;
		failure_distance = lift.IsValid() ?
		    lift.DistanceTo(edge_point) : DBL_MAX;
		break;
	    }
	    points.Append(uv);
	    parameters.Append(trim_domain.ParameterAt(fraction));
	}
	const double parameter_tolerance = std::max(ON_ZERO_TOLERANCE *
	    kNumericalToleranceScale, kPeriodicParameterSnapFraction *
	    std::max(1.0, period));
	if (!exact || fabs(previous_closed - desired_end) > parameter_tolerance) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": native periodic boundary L" << loop.m_loop_index
		    << " winding=" << winding << " rejected: "
		    << (exact ? "projected parameter did not span one period" :
			failure_reason) << " sample=" << failure_sample
		    << " distance=" << failure_distance << " closed="
		    << previous_closed << " desired=" << desired_end
		    << " tolerance=" << topology_tolerance << std::endl;
	    continue;
	}
	std::unique_ptr<ON_PolylineCurve> candidate(
	    new ON_PolylineCurve(points, parameters));
	if (!candidate || !candidate->ChangeDimension(2) ||
		!candidate->IsValid())
	    continue;
	for (int sample = 0; exact &&
		sample < kPeriodicBoundaryConstructionSegments;
		sample++) {
	    const double fraction = (static_cast<double>(sample) + 0.5) /
		kPeriodicBoundaryConstructionSegments;
	    const ON_3dPoint uv = candidate->PointAt(
		trim_domain.ParameterAt(fraction));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint edge_point = edge->PointAt(
		edge_domain.ParameterAt(trim->m_bRev3d ?
		    1.0 - fraction : fraction));
	    exact = lift.IsValid() && edge_point.IsValid() &&
		lift.DistanceTo(edge_point) <= topology_tolerance;
	    if (!exact) {
		failure_reason = "validating an interpolated fixed-parameter lift";
		failure_sample = sample;
		failure_distance = lift.IsValid() && edge_point.IsValid() ?
		    lift.DistanceTo(edge_point) : DBL_MAX;
	    }
	}
	if (!exact) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": native periodic boundary L" << loop.m_loop_index
		    << " winding=" << winding << " rejected: " << failure_reason
		    << " sample=" << failure_sample << " distance="
		    << failure_distance << " tolerance=" << topology_tolerance
		    << std::endl;
	    continue;
	}
	const int c2 = brep->AddTrimCurve(candidate.release());
	if (c2 < 0 || !brep->SetTrimCurve(*trim, c2))
	    return false;
	brep->SetTrimIsoFlags(*trim);
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "regenerated a native-seam closed boundary as an exact full-period surface isocurve");
	return true;
    }
    return false;
}


static bool
split_periodic_boundary_at_native_seam(ON_Brep *brep,
	int target_loop_index, int target_trim_index, const ON_Surface *surface,
	int closed_direction, double open_parameter, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !surface || !wrapper || target_loop_index < 0 ||
	    target_loop_index >= brep->m_L.Count() || target_trim_index < 0 ||
	    target_trim_index >= brep->m_T.Count())
	return false;
    ON_BrepTrim &target_trim = brep->m_T[target_trim_index];
    const int original_edge_index = target_trim.m_ei;
    if (original_edge_index < 0 || original_edge_index >= brep->m_E.Count())
	return false;
    ON_BrepEdge &original_edge = brep->m_E[original_edge_index];
    if (original_edge.m_vi[0] < 0 || original_edge.m_vi[0] >= brep->m_V.Count() ||
	    original_edge.m_vi[1] < 0 || original_edge.m_vi[1] >= brep->m_V.Count() ||
	    original_edge.m_ti.Count() < 1 || !original_edge.EdgeCurveOf())
	return false;
    const bool original_edge_closed =
	original_edge.m_vi[0] == original_edge.m_vi[1];
    const double topology_tolerance = std::max(LocalUnits::tolerance,
	std::max(original_edge.m_tolerance,
	    std::max(target_trim.m_tolerance[0], target_trim.m_tolerance[1])));
    const int open_direction = 1 - closed_direction;
    const ON_Interval closed_domain = surface->Domain(closed_direction);
    const ON_Interval edge_domain = original_edge.Domain();
    if (!closed_domain.IsIncreasing() || !edge_domain.IsIncreasing())
	return false;
    const double period = closed_domain.Length();
    brlcad::PullbackContext pullback_context;
    const char *rejection_stage = "projecting the closed edge";
    const auto reject = [wrapper, entity_id, &entity_type,
	    target_loop_index, target_trim_index, &rejection_stage]() {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": exact native-seam split L" << target_loop_index << "/T"
		<< target_trim_index << " rejected while " << rejection_stage
		<< std::endl;
	return false;
    };

    struct ProjectedSample {
	double edge_parameter;
	double closed_parameter;
    };
    std::vector<ProjectedSample> projected;
    projected.reserve(kBoundaryParameterSearchSegments + 1);
    double previous_closed = 0.0;
    for (int sample = 0; sample <= kBoundaryParameterSearchSegments;
	    ++sample) {
	if (brlcad::PullbackWorkCancelled())
	    return reject();
	const double fraction = static_cast<double>(sample) /
	    kBoundaryParameterSearchSegments;
	const double edge_parameter = edge_domain.ParameterAt(fraction);
	const ON_3dPoint edge_point = original_edge.PointAt(edge_parameter);
	ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint lift;
	double distance = DBL_MAX;
	if (!edge_point.IsValid() ||
		!pullback_context.SurfaceClosestPoint(surface, edge_point, uv,
		    lift, distance, 0, std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.1), topology_tolerance) ||
		distance > topology_tolerance)
	    return reject();
	double closed = uv[closed_direction];
	if (!projected.empty())
	    closed += round((previous_closed - closed) / period) * period;
	previous_closed = closed;
	projected.push_back({edge_parameter, closed});
    }

    int crossing = -1;
    double seam_parameter = 0.0;
    bool sampled_seam_crossing = false;
    double sampled_edge_parameter = 0.0;
    for (size_t sample = 1; sample < projected.size(); ++sample) {
	const double first = projected[sample - 1].closed_parameter;
	const double second = projected[sample].closed_parameter;
	const double nearest_seam = closed_domain.Min() +
	    round((second - closed_domain.Min()) / period) * period;
	const double seam_hit_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    period * 1.0e-10);
	if (sample + 1 < projected.size() &&
		fabs(second - nearest_seam) <= seam_hit_tolerance) {
	    crossing = static_cast<int>(sample);
	    seam_parameter = nearest_seam;
	    sampled_seam_crossing = true;
	    sampled_edge_parameter = projected[sample].edge_parameter;
	    break;
	}
	if (fabs(second - first) <= ON_ZERO_TOLERANCE)
	    continue;
	const double minimum = std::min(first, second);
	const double maximum = std::max(first, second);
	const double first_seam = closed_domain.Min() +
	    ceil((minimum - closed_domain.Min()) / period) * period;
	if (first_seam > minimum + ON_ZERO_TOLERANCE &&
		first_seam < maximum - ON_ZERO_TOLERANCE) {
	    crossing = static_cast<int>(sample);
	    seam_parameter = first_seam;
	    break;
	}
    }
    if (crossing < 1) {
	rejection_stage = "locating the native periodic seam crossing";
	return reject();
    }

    rejection_stage = "refining the native periodic seam crossing";
    double parameter_a = projected[crossing - 1].edge_parameter;
    double parameter_b = projected[crossing].edge_parameter;
    double closed_a = projected[crossing - 1].closed_parameter;
    double closed_b = projected[crossing].closed_parameter;
    for (int iteration = 0; !sampled_seam_crossing && iteration < 64;
	    ++iteration) {
	const double parameter = 0.5 * (parameter_a + parameter_b);
	const ON_3dPoint edge_point = original_edge.PointAt(parameter);
	ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint lift;
	double distance = DBL_MAX;
	if (!edge_point.IsValid() ||
		!pullback_context.SurfaceClosestPoint(surface, edge_point, uv,
		    lift, distance, 0, std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.01), topology_tolerance) ||
		distance > topology_tolerance)
	    return reject();
	double closed = uv[closed_direction];
	closed += round((0.5 * (closed_a + closed_b) - closed) / period) *
	    period;
	if ((closed_a < closed_b) == (closed < seam_parameter)) {
	    parameter_a = parameter;
	    closed_a = closed;
	} else {
	    parameter_b = parameter;
	    closed_b = closed;
	}
	if (fabs(closed - seam_parameter) <= ON_ZERO_TOLERANCE *
		kNumericalToleranceScale)
	    break;
    }
    double edge_split_parameter = sampled_seam_crossing ?
	sampled_edge_parameter : 0.5 * (parameter_a + parameter_b);
    ON_3dPoint seam_uv;
    seam_uv[closed_direction] = closed_domain.Min();
    seam_uv[open_direction] = open_parameter;
    seam_uv.z = 0.0;
    const ON_3dPoint seam_point = surface->PointAt(seam_uv.x, seam_uv.y);
    ON_3dPoint split_point = original_edge.PointAt(edge_split_parameter);
    double seam_distance = seam_point.IsValid() && split_point.IsValid() ?
	seam_point.DistanceTo(split_point) : DBL_MAX;
    /* A periodic closest-point projection can switch branches inside the
     * bracketing interval even though every sample lies on the exact source
     * edge.  If that makes the UV bisection miss the native seam, solve the
     * already-known 3-D seam point directly on the immutable edge curve. */
    if (seam_distance > topology_tolerance && seam_point.IsValid()) {
	ON_NurbsCurve edge_nurbs;
	double closest_parameter = 0.0;
	if (original_edge.GetNurbForm(edge_nurbs) &&
		ON_NurbsCurve_GetClosestPoint(&closest_parameter, &edge_nurbs,
		    seam_point)) {
	    const ON_Interval closest_domain = edge_nurbs.Domain();
	    for (int iteration = 0; iteration < 24 &&
		    closest_domain.IsIncreasing(); ++iteration) {
		ON_3dPoint point;
		ON_3dVector first_derivative;
		ON_3dVector second_derivative;
		if (!edge_nurbs.Ev2Der(closest_parameter, point,
			first_derivative, second_derivative))
		    break;
		const ON_3dVector residual = point - seam_point;
		const double gradient = first_derivative * residual;
		const double curvature = second_derivative * residual +
		    first_derivative * first_derivative;
		if (!(fabs(curvature) > DBL_EPSILON))
		    break;
		const double next_parameter = std::max(closest_domain.Min(),
		    std::min(closest_domain.Max(), closest_parameter -
			gradient / curvature));
		if (fabs(next_parameter - closest_parameter) <=
			DBL_EPSILON * std::max(1.0, fabs(closest_parameter)))
		    break;
		closest_parameter = next_parameter;
	    }
	    const ON_3dPoint closest_point = edge_nurbs.PointAt(closest_parameter);
	    const double closest_distance = closest_point.IsValid() ?
		seam_point.DistanceTo(closest_point) : DBL_MAX;
	    if (closest_distance <= topology_tolerance) {
		edge_split_parameter = closest_parameter;
		split_point = closest_point;
		seam_distance = closest_distance;
	    }
	}
    }
    if (!seam_point.IsValid() || !split_point.IsValid() ||
	    seam_distance > topology_tolerance) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": native seam point missed exact edge by " << seam_distance
		<< " at parameter " << edge_split_parameter << " tolerance="
		<< topology_tolerance << std::endl;
	rejection_stage = "validating the exact 3-D seam point";
	return reject();
    }

    rejection_stage = "splitting the exact 3-D boundary curve";
    std::unique_ptr<ON_Curve> source_curve(
	original_edge.EdgeCurveOf()->DuplicateCurve());
    ON_Curve *first_curve_raw = NULL;
    ON_Curve *second_curve_raw = NULL;
    if (!source_curve || !source_curve->Split(edge_split_parameter,
	    first_curve_raw, second_curve_raw))
	return reject();
    std::unique_ptr<ON_Curve> first_curve(first_curve_raw);
    std::unique_ptr<ON_Curve> second_curve(second_curve_raw);
    const int original_start_vertex_index = original_edge.m_vi[0];
    const int original_end_vertex_index = original_edge.m_vi[1];
    const ON_3dPoint &original_start_vertex =
	brep->m_V[original_start_vertex_index].point;
    const ON_3dPoint &original_end_vertex =
	brep->m_V[original_end_vertex_index].point;
    if (!first_curve || !second_curve || !first_curve->IsValid() ||
	    !second_curve->IsValid() ||
	    first_curve->PointAtStart().DistanceTo(original_start_vertex) >
		topology_tolerance ||
	    first_curve->PointAtEnd().DistanceTo(seam_point) >
		topology_tolerance ||
	    second_curve->PointAtStart().DistanceTo(seam_point) >
		topology_tolerance ||
	    second_curve->PointAtEnd().DistanceTo(original_end_vertex) >
		topology_tolerance)
	return reject();
    /* Split() on a curve proxy may return child proxies which still point at
     * the original BREP curve.  Compacting away that original would leave the
     * new edges/trims with dangling proxy targets.  Materialize every child as
     * an independently owned NURBS curve before mutating topology. */
    const auto detach_curve = [](std::unique_ptr<ON_Curve> &curve) {
	if (!curve) return false;
	std::unique_ptr<ON_NurbsCurve> detached(new ON_NurbsCurve());
	if (!curve->GetNurbForm(*detached) || !detached->IsValid())
	    return false;
	curve.reset(detached.release());
	return true;
    };
    if (!detach_curve(first_curve) || !detach_curve(second_curve))
	return reject();

    struct TrimPiece {
	std::unique_ptr<ON_Curve> curve;
	int split_edge;
	bool reversed;
    };
    struct TrimReplacement {
	int old_trim;
	int loop;
	ON_BrepTrim::TYPE type;
	std::vector<TrimPiece> pieces;
    };
    std::vector<TrimReplacement> replacements;
    std::map<int, std::vector<int> > original_loop_trims;
    std::vector<int> original_trim_indices;
    for (int use = 0; use < original_edge.m_ti.Count(); ++use)
	original_trim_indices.push_back(original_edge.m_ti[use]);
    for (std::vector<int>::const_iterator trim_id =
	    original_trim_indices.begin(); trim_id != original_trim_indices.end();
	    ++trim_id) {
	if (*trim_id < 0 || *trim_id >= brep->m_T.Count())
	    return reject();
	ON_BrepTrim &trim = brep->m_T[*trim_id];
	if (trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
		!trim.TrimCurveOf())
	    return reject();
	if (original_loop_trims.find(trim.m_li) == original_loop_trims.end()) {
	    const ON_BrepLoop &use_loop = brep->m_L[trim.m_li];
	    original_loop_trims[trim.m_li] = std::vector<int>(
		use_loop.m_ti.Array(),
		use_loop.m_ti.Array() + use_loop.m_ti.Count());
	}
	TrimReplacement replacement;
	replacement.old_trim = *trim_id;
	replacement.loop = trim.m_li;
	replacement.type = trim.m_type;
	if (original_edge_closed && *trim_id == target_trim_index) {
	    rejection_stage = "constructing the target periodic pcurve split";
	    /* A closed STEP edge may retain an arbitrary curve/pcurve start which
	     * lies on the native parameter seam even though its single topology
	     * vertex is elsewhere on the same exact circle.  Splitting at that
	     * arbitrary pcurve start creates a zero-length trim piece.  Project the
	     * authoritative topology vertex and use its native-domain UV as the
	     * two-piece chain endpoint. */
	    ON_2dPoint topology_vertex_uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint topology_vertex_lift;
	    double topology_vertex_distance = DBL_MAX;
	    if (!pullback_context.SurfaceClosestPoint(surface, original_start_vertex,
		    topology_vertex_uv, topology_vertex_lift,
		    topology_vertex_distance, 0,
		    std::max(ON_ZERO_TOLERANCE, topology_tolerance * 0.1),
		    topology_tolerance) ||
		    topology_vertex_distance > topology_tolerance)
		return reject();
	    while (topology_vertex_uv[closed_direction] < closed_domain.Min())
		topology_vertex_uv[closed_direction] += period;
	    while (topology_vertex_uv[closed_direction] > closed_domain.Max())
		topology_vertex_uv[closed_direction] -= period;
	    const ON_3dPoint original_uv(topology_vertex_uv.x,
		topology_vertex_uv.y, 0.0);
	    if (!original_uv.IsValid() ||
		    fabs(original_uv[open_direction] - open_parameter) >
			topology_tolerance)
		return reject();
	    const ON_3dPoint first_midpoint = first_curve->PointAt(
		first_curve->Domain().Mid());
	    ON_2dPoint midpoint_uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint midpoint_lift;
	    double midpoint_distance = DBL_MAX;
	    if (!pullback_context.SurfaceClosestPoint(surface, first_midpoint,
		    midpoint_uv, midpoint_lift, midpoint_distance, 0,
		    std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.1), topology_tolerance) ||
		    midpoint_distance > topology_tolerance)
		return reject();
	    double midpoint_closed = midpoint_uv[closed_direction];
	    midpoint_closed += round((original_uv[closed_direction] -
		midpoint_closed) / period) * period;
	    const bool first_moves_increasing =
		midpoint_closed > original_uv[closed_direction];
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": native-seam split L" << target_loop_index << "/T"
		    << target_trim_index << "/STEP edge "
		    << original_edge.m_edge_user.i << " topology-uv="
		    << original_uv.x << ':' << original_uv.y << " midpoint-uv="
		    << midpoint_uv.x << ':' << midpoint_uv.y << " seam-uv="
		    << seam_uv.x << ':' << seam_uv.y << " direction="
		    << closed_direction << '/' << (first_moves_increasing ? 1 : -1)
		    << std::endl;
	    ON_3dPoint minimum_uv = original_uv;
	    ON_3dPoint maximum_uv = original_uv;
	    minimum_uv[closed_direction] = closed_domain.Min();
	    maximum_uv[closed_direction] = closed_domain.Max();
	    std::unique_ptr<ON_LineCurve> first_piece;
	    std::unique_ptr<ON_LineCurve> second_piece;
	    if (first_moves_increasing) {
		first_piece.reset(new ON_LineCurve(minimum_uv, original_uv));
		second_piece.reset(new ON_LineCurve(original_uv, maximum_uv));
		replacement.pieces.push_back({
		    std::unique_ptr<ON_Curve>(first_piece.release()), 1, false});
		replacement.pieces.push_back({
		    std::unique_ptr<ON_Curve>(second_piece.release()), 0, false});
	    } else {
		first_piece.reset(new ON_LineCurve(minimum_uv, original_uv));
		second_piece.reset(new ON_LineCurve(original_uv, maximum_uv));
		replacement.pieces.push_back({
		    std::unique_ptr<ON_Curve>(first_piece.release()), 0, true});
		replacement.pieces.push_back({
		    std::unique_ptr<ON_Curve>(second_piece.release()), 1, true});
	    }
	    for (std::vector<TrimPiece>::iterator piece =
		    replacement.pieces.begin(); piece != replacement.pieces.end();
		    ++piece)
		if (!piece->curve || !piece->curve->ChangeDimension(2) ||
			!piece->curve->IsValid())
		    return reject();
	} else {
	    rejection_stage = "finding the adjacent face for the seam vertex";
	    const ON_BrepLoop &use_loop = brep->m_L[trim.m_li];
	    const ON_BrepFace *use_face = use_loop.Face();
	    const ON_Surface *use_surface = use_face ? use_face->SurfaceOf() : NULL;
	    if (!use_surface)
		return reject();
	    ON_2dPoint cut_uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint cut_lift;
	    double cut_distance = DBL_MAX;
	    rejection_stage = "projecting the seam vertex onto an adjacent face";
	    if (!pullback_context.SurfaceClosestPoint(use_surface, seam_point,
		    cut_uv, cut_lift, cut_distance, 0,
		    std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.1), topology_tolerance) ||
		    cut_distance > topology_tolerance)
		return reject();
	    double trim_parameter = 0.0;
	    const ON_3dPoint cut_uv3(cut_uv.x, cut_uv.y, 0.0);
	    ON_NurbsCurve trim_nurbs;
	    rejection_stage = "locating the seam vertex on an adjacent pcurve";
	    if (!trim.GetNurbForm(trim_nurbs) ||
		    !ON_NurbsCurve_GetClosestPoint(&trim_parameter, &trim_nurbs,
		cut_uv3))
		return reject();
	    const ON_Interval trim_domain = trim.Domain();
	    const auto lifted_trim_distance = [&trim, use_surface,
		    &seam_point](double parameter) {
		const ON_3dPoint uv = trim.PointAt(parameter);
		const ON_3dPoint lift = uv.IsValid() ?
		    use_surface->PointAt(uv.x, uv.y) : ON_3dPoint::UnsetPoint;
		return lift.IsValid() ? lift.DistanceTo(seam_point) : DBL_MAX;
	    };
	    /* ON_NurbsCurve_GetClosestPoint supplies a good p-space seed, but its
	     * stopping tolerance can be much looser than a STEP model's asserted
	     * edge tolerance.  Refine that seed against the actual 3-D surface
	     * lift.  Also test the directed edge/trim parameter correspondence;
	     * exporters often retain it exactly even when the two curve domains
	     * differ.  The bounded one-dimensional search changes no geometry and
	     * the final shared-vertex lift remains the acceptance proof. */
	    const double edge_fraction = edge_domain.NormalizedParameterAt(
		edge_split_parameter);
	    const double directed_parameter = trim_domain.ParameterAt(
		trim.m_bRev3d ? 1.0 - edge_fraction : edge_fraction);
	    if (lifted_trim_distance(directed_parameter) <
		    lifted_trim_distance(trim_parameter))
		trim_parameter = directed_parameter;
	    const double refinement_radius = trim_domain.Length() /
		kBoundaryParameterSearchSegments;
	    double refinement_minimum = std::max(trim_domain.Min(),
		trim_parameter - refinement_radius);
	    double refinement_maximum = std::min(trim_domain.Max(),
		trim_parameter + refinement_radius);
	    for (int iteration = 0; iteration < 64; ++iteration) {
		const double first_parameter = refinement_minimum +
		    (refinement_maximum - refinement_minimum) / 3.0;
		const double second_parameter = refinement_maximum -
		    (refinement_maximum - refinement_minimum) / 3.0;
		if (lifted_trim_distance(first_parameter) <=
			lifted_trim_distance(second_parameter))
		    refinement_maximum = second_parameter;
		else
		    refinement_minimum = first_parameter;
	    }
	    const double refined_parameter = 0.5 *
		(refinement_minimum + refinement_maximum);
	    if (lifted_trim_distance(refined_parameter) <
		    lifted_trim_distance(trim_parameter))
		trim_parameter = refined_parameter;
	    const double domain_guard = trim_domain.Length() * 1.0e-10;
	    rejection_stage = "proving an interior adjacent-pcurve split parameter";
	    if (trim_parameter <= trim_domain.Min() + domain_guard ||
		    trim_parameter >= trim_domain.Max() - domain_guard) {
		/* A singleton adjacent use can have the same arbitrary seam-based
		 * pcurve start as the target use.  Its seam point is consequently an
		 * endpoint in parameter space even though the authoritative topology
		 * vertex is interior to the closed 3-D edge.  Rebuild the use as two
		 * native-domain isocurves from that projected topology vertex. */
		ON_2dPoint vertex_uv = ON_2dPoint::UnsetPoint;
		ON_3dPoint vertex_lift;
		double vertex_distance = DBL_MAX;
		bool rebuilt_singleton = false;
		if (original_edge_closed && use_loop.TrimCount() == 1 &&
			pullback_context.SurfaceClosestPoint(use_surface,
			    original_start_vertex, vertex_uv, vertex_lift,
			    vertex_distance, 0,
			    std::max(ON_ZERO_TOLERANCE,
				topology_tolerance * 0.1),
			    topology_tolerance) &&
			vertex_distance <= topology_tolerance) {
		    for (int adjacent_closed = 0; adjacent_closed < 2 &&
			    !rebuilt_singleton; ++adjacent_closed) {
			if (!use_surface->IsClosed(adjacent_closed))
			    continue;
			const int adjacent_open = 1 - adjacent_closed;
			const ON_Interval adjacent_domain = use_surface->Domain(
			    adjacent_closed);
			const double adjacent_period = adjacent_domain.Length();
			const double parameter_guard = std::max(
			    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			    adjacent_period * 1.0e-10);
			if (!adjacent_domain.IsIncreasing() ||
				!(adjacent_period > ON_ZERO_TOLERANCE) ||
				(fabs(cut_uv[adjacent_closed] -
				    adjacent_domain.Min()) > parameter_guard &&
				 fabs(cut_uv[adjacent_closed] -
				    adjacent_domain.Max()) > parameter_guard) ||
				fabs(vertex_uv[adjacent_open] -
				    cut_uv[adjacent_open]) > topology_tolerance)
			    continue;
			while (vertex_uv[adjacent_closed] < adjacent_domain.Min())
			    vertex_uv[adjacent_closed] += adjacent_period;
			while (vertex_uv[adjacent_closed] > adjacent_domain.Max())
			    vertex_uv[adjacent_closed] -= adjacent_period;
			const ON_3dPoint trim_start = trim.PointAtStart();
			const ON_3dPoint trim_midpoint = trim.PointAt(
			    trim_domain.Mid());
			double midpoint_parameter = trim_midpoint[adjacent_closed];
			midpoint_parameter += round((trim_start[adjacent_closed] -
			    midpoint_parameter) / adjacent_period) * adjacent_period;
			const bool increasing = midpoint_parameter >
			    trim_start[adjacent_closed];
			ON_3dPoint vertex_point(vertex_uv.x, vertex_uv.y, 0.0);
			ON_3dPoint minimum_point(vertex_point);
			ON_3dPoint maximum_point(vertex_point);
			minimum_point[adjacent_closed] = adjacent_domain.Min();
			maximum_point[adjacent_closed] = adjacent_domain.Max();
			std::unique_ptr<ON_Curve> minimum_to_vertex(
			    new ON_LineCurve(minimum_point, vertex_point));
			std::unique_ptr<ON_Curve> vertex_to_maximum(
			    new ON_LineCurve(vertex_point, maximum_point));
			if (!minimum_to_vertex || !vertex_to_maximum ||
				!minimum_to_vertex->ChangeDimension(2) ||
				!vertex_to_maximum->ChangeDimension(2) ||
				!minimum_to_vertex->IsValid() ||
				!vertex_to_maximum->IsValid() ||
				!detach_curve(minimum_to_vertex) ||
				!detach_curve(vertex_to_maximum))
			    continue;
			const bool edge_first_moves_increasing = trim.m_bRev3d ?
			    !increasing : increasing;
			if (edge_first_moves_increasing) {
			    replacement.pieces.push_back({
				std::move(minimum_to_vertex), 1, false});
			    replacement.pieces.push_back({
				std::move(vertex_to_maximum), 0, false});
			} else {
			    replacement.pieces.push_back({
				std::move(minimum_to_vertex), 0, true});
			    replacement.pieces.push_back({
				std::move(vertex_to_maximum), 1, true});
			}
			rebuilt_singleton = true;
		    }
		}
		if (!rebuilt_singleton)
		    return reject();
		replacements.push_back(std::move(replacement));
		continue;
	    }
	    ON_Curve *left_raw = NULL;
	    ON_Curve *right_raw = NULL;
	    rejection_stage = "splitting an adjacent pcurve";
	    if (!trim.Split(trim_parameter, left_raw, right_raw))
		return reject();
	    std::unique_ptr<ON_Curve> left(left_raw);
	    std::unique_ptr<ON_Curve> right(right_raw);
	    if (!left || !right || !left->IsValid() || !right->IsValid())
		return reject();
	    const ON_3dPoint left_end = left->PointAtEnd();
	    const ON_3dPoint right_start = right->PointAtStart();
	    const ON_3dPoint left_lift = use_surface->PointAt(
		left_end.x, left_end.y);
	    const ON_3dPoint right_lift = use_surface->PointAt(
		right_start.x, right_start.y);
	    rejection_stage = "validating the adjacent pcurve split lift";
	    if (!left_lift.IsValid() || !right_lift.IsValid() ||
		    left_lift.DistanceTo(seam_point) > topology_tolerance ||
		    right_lift.DistanceTo(seam_point) > topology_tolerance) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": adjacent split T" << trim.m_trim_index
			<< " parameter=" << trim_parameter
			<< " trim-domain=" << trim_domain.Min() << ':'
			<< trim_domain.Max() << " nurbs-domain="
			<< trim_nurbs.Domain().Min() << ':'
			<< trim_nurbs.Domain().Max() << " uv=" << cut_uv.x << ':'
			<< cut_uv.y << " split-uv=" << left_end.x << ':'
			<< left_end.y << " lift-distance="
			<< left_lift.DistanceTo(seam_point) << '/'
			<< right_lift.DistanceTo(seam_point) << " tolerance="
			<< topology_tolerance << std::endl;
		return reject();
	    }
	    rejection_stage = "materializing adjacent split pcurves";
	    if (!detach_curve(left) || !detach_curve(right))
		return reject();
	    if (!trim.m_bRev3d) {
		replacement.pieces.push_back({std::move(left), 0, false});
		replacement.pieces.push_back({std::move(right), 1, false});
	    } else {
		replacement.pieces.push_back({std::move(left), 1, true});
		replacement.pieces.push_back({std::move(right), 0, true});
	    }
	}
	replacements.push_back(std::move(replacement));
    }

    rejection_stage = "installing the split boundary topology";
    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
    const int first_c3 = brep->AddEdgeCurve(first_curve.release());
    const int second_c3 = brep->AddEdgeCurve(second_curve.release());
    if (first_c3 < 0 || second_c3 < 0) {
	*brep = *rollback;
	return false;
    }
    ON_BrepVertex &seam_vertex = brep->NewVertex(seam_point,
	topology_tolerance);
    const int seam_vertex_index = seam_vertex.m_vertex_index;
    const int first_edge_index = brep->NewEdge(
	brep->m_V[original_start_vertex_index], brep->m_V[seam_vertex_index],
	first_c3, NULL, topology_tolerance).m_edge_index;
    const int second_edge_index = brep->NewEdge(
	brep->m_V[seam_vertex_index], brep->m_V[original_end_vertex_index],
	second_c3, NULL, topology_tolerance).m_edge_index;
    brep->m_E[first_edge_index].m_edge_user =
	brep->m_E[original_edge_index].m_edge_user;
    brep->m_E[second_edge_index].m_edge_user =
	brep->m_E[original_edge_index].m_edge_user;
    brep->m_E[first_edge_index].m_tolerance = topology_tolerance;
    brep->m_E[second_edge_index].m_tolerance = topology_tolerance;

    std::map<int, std::vector<int> > replacement_trim_indices;
    for (std::vector<TrimReplacement>::iterator replacement =
	    replacements.begin(); replacement != replacements.end(); ++replacement) {
	for (std::vector<TrimPiece>::iterator piece = replacement->pieces.begin();
		piece != replacement->pieces.end(); ++piece) {
	    const int c2 = brep->AddTrimCurve(piece->curve.release());
	    const int edge_index = piece->split_edge == 0 ?
		first_edge_index : second_edge_index;
	    if (c2 < 0 || edge_index < 0 || edge_index >= brep->m_E.Count()) {
		*brep = *rollback;
		return false;
	    }
	    const int new_trim_index = brep->NewTrim(brep->m_E[edge_index],
		piece->reversed, brep->m_L[replacement->loop], c2).m_trim_index;
	    if (new_trim_index < 0) {
		*brep = *rollback;
		return false;
	    }
	    ON_BrepTrim &new_trim = brep->m_T[new_trim_index];
	    new_trim.m_type = replacement->type;
	    new_trim.m_tolerance[0] = topology_tolerance;
	    new_trim.m_tolerance[1] = topology_tolerance;
	    ON_Interval new_domain = new_trim.Domain();
	    const ON_Surface *use_surface = brep->m_L[replacement->loop].Face()->
		SurfaceOf();
	    new_trim.m_iso = use_surface ? use_surface->IsIsoparametric(
		*new_trim.TrimCurveOf(), &new_domain) : ON_Surface::not_iso;
	    /* The original closed edge has coincident topology endpoints, so its
	     * m_bRev3d flag cannot unambiguously determine the orientation of newly
	     * opened seam pieces.  Prove each replacement direction from its lifted
	     * pcurve endpoints and the exact subedge vertices. */
	    const ON_BrepEdge *replacement_edge = new_trim.Edge();
	    if (use_surface && replacement_edge &&
		    replacement_edge->m_vi[0] >= 0 &&
		    replacement_edge->m_vi[0] < brep->m_V.Count() &&
		    replacement_edge->m_vi[1] >= 0 &&
		    replacement_edge->m_vi[1] < brep->m_V.Count()) {
		const ON_3dPoint trim_start_uv = new_trim.PointAtStart();
		const ON_3dPoint trim_end_uv = new_trim.PointAtEnd();
		const ON_3dPoint trim_start_lift = use_surface->PointAt(
		    trim_start_uv.x, trim_start_uv.y);
		const ON_3dPoint trim_end_lift = use_surface->PointAt(
		    trim_end_uv.x, trim_end_uv.y);
		const ON_3dPoint &edge_start = brep->m_V[
		    replacement_edge->m_vi[0]].point;
		const ON_3dPoint &edge_end = brep->m_V[
		    replacement_edge->m_vi[1]].point;
		if (trim_start_lift.IsValid() && trim_end_lift.IsValid()) {
		    const double forward_error = std::max(
			trim_start_lift.DistanceTo(edge_start),
			trim_end_lift.DistanceTo(edge_end));
		    const double reverse_error = std::max(
			trim_start_lift.DistanceTo(edge_end),
			trim_end_lift.DistanceTo(edge_start));
		    if (std::min(forward_error, reverse_error) <=
			    topology_tolerance)
			new_trim.m_bRev3d = reverse_error < forward_error;
		}
	    }
	    replacement_trim_indices[replacement->old_trim].push_back(
		new_trim_index);
	}
    }

    for (std::map<int, std::vector<int> >::const_iterator loop_trims =
	    original_loop_trims.begin(); loop_trims != original_loop_trims.end();
	    ++loop_trims) {
	ON_BrepLoop &use_loop = brep->m_L[loop_trims->first];
	use_loop.m_ti.SetCount(0);
	for (std::vector<int>::const_iterator old_trim =
		loop_trims->second.begin(); old_trim != loop_trims->second.end();
		++old_trim) {
	    std::map<int, std::vector<int> >::const_iterator replacement =
		replacement_trim_indices.find(*old_trim);
	    if (replacement == replacement_trim_indices.end()) {
		use_loop.m_ti.Append(*old_trim);
		brep->m_T[*old_trim].m_li = loop_trims->first;
		continue;
	    }
	    for (std::vector<int>::const_iterator new_trim =
		    replacement->second.begin(); new_trim != replacement->second.end();
		    ++new_trim) {
		use_loop.m_ti.Append(*new_trim);
		brep->m_T[*new_trim].m_li = loop_trims->first;
	    }
	}
    }
    for (std::vector<int>::const_iterator old_trim =
	    original_trim_indices.begin(); old_trim != original_trim_indices.end();
	    ++old_trim)
	brep->DeleteTrim(brep->m_T[*old_trim], false);
    brep->DeleteEdge(brep->m_E[original_edge_index], false);
    if (!brep->Compact()) {
	*brep = *rollback;
	return false;
    }
    /* Each replacement trim initially inherits the source trim type while
     * the original closed edge and its complete use graph still exist.  Once
     * the edge is split and the BREP is compacted, that inherited value can
     * be stale (for example, a former seam use can now be a mated subedge on
     * two faces).  Let OpenNURBS derive the authoritative type from the final
     * reciprocal edge/trim references before structural validation. */
    if (!brep->SetTrimTypeFlags(false)) {
	*brep = *rollback;
	return false;
    }
    std::string unsafe_topology;
    if (!brep_topology_references_are_safe(brep, &unsafe_topology)) {
	*brep = *rollback;
	return false;
    }
    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	original_edge_closed ?
	"split a closed STEP boundary edge at an exact OpenNURBS periodic seam" :
	"split an open STEP boundary edge at an exact OpenNURBS periodic seam");
    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	"regenerated an implicit full-period boundary from exact split edge uses");
    return true;
}


/* Split an open edge whose continuous pcurve crosses a native periodic
 * boundary in its interior.  A closed face band needs a topology vertex at
 * that cut so its two physical boundaries can share one explicit OpenNURBS
 * seam.  The general splitter above propagates the same exact 3-D split to
 * every use of the STEP edge; this helper only locates a proven interior UV
 * crossing on the requesting face. */
static bool
split_open_periodic_boundary_crossing(ON_Brep *brep, ON_BrepFace &face,
    int loop_index, int closed_direction, double parameter_tolerance,
    STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || loop_index < 0 ||
	    loop_index >= brep->m_L.Count() || closed_direction < 0 ||
	    closed_direction > 1)
	return false;
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface || !surface->IsClosed(closed_direction))
	return false;

    ON_BrepLoop &loop = brep->m_L[loop_index];
    const ON_Interval surface_domain = surface->Domain(closed_direction);
    if (!surface_domain.IsIncreasing() || loop.TrimCount() < 1)
	return false;
    const double period = surface_domain.Length();

    /* Having one proven full-period boundary does not make every other loop
     * on the face a second band boundary.  In particular, an ordinary inner
     * loop can have fitted pcurves which briefly overshoot a native seam.  If
     * those incidental crossings are split one at a time, topology grows
     * without bound while the loop can never become the missing boundary.
     *
     * Unwrap the complete supplied pcurve chain first.  A legitimate second
     * band boundary has winding number +1 or -1 in the already-proven closed
     * direction; a contractible inner loop has winding number zero.  Dense
     * sampling uses the same documented boundary-search resolution as the
     * crossing locator below, so a curve cannot gain an unobserved winding
     * between the proof and the subsequent split search. */
    bool have_parameter = false;
    double first_unwrapped = 0.0;
    double previous_unwrapped = 0.0;
    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	const ON_BrepTrim *trim = loop.Trim(lti);
	const ON_Curve *curve = trim ? trim->TrimCurveOf() : NULL;
	if (!curve || !curve->Domain().IsIncreasing())
	    return false;
	const ON_Interval curve_domain = curve->Domain();
	for (int sample = 0; sample <= kBoundaryParameterSearchSegments;
		++sample) {
	    if (brlcad::PullbackWorkCancelled())
		return false;
	    const ON_3dPoint point = curve->PointAt(curve_domain.ParameterAt(
		static_cast<double>(sample) /
		kBoundaryParameterSearchSegments));
	    if (!point.IsValid() || !std::isfinite(point[closed_direction]))
		return false;
	    double unwrapped = point[closed_direction];
	    if (have_parameter)
		unwrapped += round((previous_unwrapped - unwrapped) / period) *
		    period;
	    else {
		first_unwrapped = unwrapped;
		have_parameter = true;
	    }
	    previous_unwrapped = unwrapped;
	}
    }
    const double net_winding = previous_unwrapped - first_unwrapped;
    if (!have_parameter || fabs(fabs(net_winding) - period) >
	    parameter_tolerance) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": open periodic boundary split L" << loop_index
		<< " rejected because net pcurve winding " << net_winding
		<< " does not equal one period " << period << std::endl;
	return false;
    }

    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	ON_BrepTrim *trim = loop.Trim(lti);
	const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	const ON_Curve *curve = trim ? trim->TrimCurveOf() : NULL;
	if (!trim || !edge || !curve || edge->m_vi[0] == edge->m_vi[1])
	    continue;
	const ON_Interval curve_domain = curve->Domain();
	if (!curve_domain.IsIncreasing())
	    continue;
	const double domain_guard = curve_domain.Length() * 1.0e-10;
	const double seam_hit_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale, period * 1.0e-10);
	const int target_trim_index = trim->m_trim_index;
	const int open_direction = 1 - closed_direction;
	const auto try_split = [brep, loop_index, target_trim_index, surface,
		closed_direction, open_direction, wrapper, entity_id,
		&entity_type, curve, &curve_domain, domain_guard](
		double crossing_parameter) {
	    if (crossing_parameter <= curve_domain.Min() + domain_guard ||
		    crossing_parameter >= curve_domain.Max() - domain_guard)
		return false;
	    const ON_3dPoint crossing = curve->PointAt(crossing_parameter);
	    return crossing.IsValid() &&
		split_periodic_boundary_at_native_seam(brep, loop_index,
		    target_trim_index, surface, closed_direction,
		    crossing[open_direction], wrapper, entity_id, entity_type);
	};

	double previous_parameter = curve_domain.Min();
	ON_3dPoint previous_point = curve->PointAt(previous_parameter);
	double previous_closed = previous_point[closed_direction];
	for (int sample = 1; sample <= kBoundaryParameterSearchSegments;
		++sample) {
	    const double parameter = curve_domain.ParameterAt(
		static_cast<double>(sample) /
		kBoundaryParameterSearchSegments);
	    const ON_3dPoint point = curve->PointAt(parameter);
	    const double current_closed = point[closed_direction];
	    if (!std::isfinite(previous_closed) ||
		    !std::isfinite(current_closed)) {
		previous_parameter = parameter;
		previous_closed = current_closed;
		continue;
	    }

	    /* The supplied pcurve may use any integral-period image of the
	     * surface domain.  Searching only Domain().Min()/Max() misses an
	     * otherwise exact arc such as 2.75P -> 3.25P, whose physical native
	     * seam lies at 3P.  Recognize a sampled seam hit first, then search
	     * every translated seam image strictly inside this sample interval.
	     * The lower-level splitter independently reprojects the immutable 3-D
	     * edge and proves the native seam point before changing topology. */
	    const double nearest_seam = surface_domain.Min() + round(
		(current_closed - surface_domain.Min()) / period) * period;
	    if (fabs(current_closed - nearest_seam) <= seam_hit_tolerance &&
		    sample < kBoundaryParameterSearchSegments) {
		const double next_parameter = curve_domain.ParameterAt(
		    static_cast<double>(sample + 1) /
		    kBoundaryParameterSearchSegments);
		const ON_3dPoint next_point = curve->PointAt(next_parameter);
		const double next_closed = next_point[closed_direction];
		const double previous_side = previous_closed - nearest_seam;
		const double next_side = next_closed - nearest_seam;
		/* A sample which lands exactly on the seam is a crossing only when
		 * its adjacent samples prove traversal from one side to the other.
		 * An isoparametric trim which lies along the seam otherwise presents
		 * hundreds of identical sample hits and must not be split. */
		const bool traverses_seam = std::isfinite(next_closed) &&
		    ((previous_side < -seam_hit_tolerance &&
		      next_side > seam_hit_tolerance) ||
		     (previous_side > seam_hit_tolerance &&
		      next_side < -seam_hit_tolerance));
		if (traverses_seam && try_split(parameter))
		    return true;
	    }

	    const double minimum = std::min(previous_closed, current_closed);
	    const double maximum = std::max(previous_closed, current_closed);
	    double boundary = surface_domain.Min() + ceil(
		(minimum - surface_domain.Min()) / period) * period;
	    if (boundary <= minimum + seam_hit_tolerance)
		boundary += period;
	    for (; boundary < maximum - seam_hit_tolerance;
		    boundary += period) {
		double lower = previous_parameter;
		double upper = parameter;
		double lower_value = previous_closed - boundary;
		for (int iteration = 0; iteration < 64; ++iteration) {
		    const double middle = 0.5 * (lower + upper);
		    const double middle_value = curve->PointAt(middle)[
			closed_direction] - boundary;
		    if ((lower_value < 0.0) == (middle_value < 0.0)) {
			lower = middle;
			lower_value = middle_value;
		    } else {
			upper = middle;
		    }
		}
		if (try_split(0.5 * (lower + upper)))
		    return true;
	    }
	    previous_parameter = parameter;
	    previous_closed = current_closed;
	}
    }
    return false;
}


/* Splitting a closed edge for one face also splits every adjacent use.  On a
 * doubly-periodic surface, the adjacent supplied pcurve may use a principal
 * branch which goes from its native seam to the opposite point and then back
 * over the same half-period.  Recover the full-period two-trim chain only when
 * two exact surface-isocurve lines, in one of the two possible windings, lift
 * densely to the directed 3-D subedges and their topology vertices. */
static bool
regenerate_split_periodic_boundary_chain(ON_Brep *brep, ON_BrepLoop &loop,
	const ON_Surface *surface, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type)
{
    if (!brep || !surface || !wrapper || loop.TrimCount() != 2)
	return false;
    ON_BrepTrim *first = loop.Trim(0);
    ON_BrepTrim *second = loop.Trim(1);
    if (!first || !second || !first->Edge() || !second->Edge() ||
	    first->m_vi[1] != second->m_vi[0] ||
	    second->m_vi[1] != first->m_vi[0] ||
	    first->m_vi[0] < 0 || first->m_vi[0] >= brep->m_V.Count() ||
	    first->m_vi[1] < 0 || first->m_vi[1] >= brep->m_V.Count())
	return false;
    const int first_source_edge = first->Edge()->m_edge_user.i;
    const int second_source_edge = second->Edge()->m_edge_user.i;
    if (first_source_edge <= 0 || first_source_edge != second_source_edge) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": split periodic chain L" << loop.m_loop_index
		<< " rejected because STEP edge identities differ ("
		<< first_source_edge << '/' << second_source_edge << ')'
		<< std::endl;
	return false;
    }
    const double topology_tolerance = std::max(LocalUnits::tolerance,
	std::max(std::max(first->Edge()->m_tolerance,
		second->Edge()->m_tolerance),
	    std::max(std::max(first->m_tolerance[0], first->m_tolerance[1]),
		std::max(second->m_tolerance[0], second->m_tolerance[1]))));

    const ON_3dPoint supplied_points[3] = {
	first->PointAtStart(), first->PointAtEnd(), second->PointAtEnd()
    };
    if (!supplied_points[0].IsValid() || !supplied_points[1].IsValid() ||
	    !supplied_points[2].IsValid())
	return false;

    for (int closed_direction = 0; closed_direction < 2; ++closed_direction) {
	if (!surface->IsClosed(closed_direction)) continue;
	const int fixed_direction = 1 - closed_direction;
	const ON_Interval closed_domain = surface->Domain(closed_direction);
	if (!closed_domain.IsIncreasing()) continue;
	const double fixed_parameter = (supplied_points[0][fixed_direction] +
	    supplied_points[1][fixed_direction] +
	    supplied_points[2][fixed_direction]) / 3.0;
	const double fixed_tolerance = std::max(ON_ZERO_TOLERANCE *
	    kNumericalToleranceScale, kPeriodicParameterSnapFraction *
	    std::max(1.0, surface->Domain(fixed_direction).Length()));
	if (fabs(supplied_points[0][fixed_direction] - fixed_parameter) >
		fixed_tolerance ||
	    fabs(supplied_points[1][fixed_direction] - fixed_parameter) >
		fixed_tolerance ||
	    fabs(supplied_points[2][fixed_direction] - fixed_parameter) >
		fixed_tolerance)
	    continue;

	double split_parameter = supplied_points[1][closed_direction];
	const double period = closed_domain.Length();
	split_parameter += round((closed_domain.Mid() - split_parameter) /
	    period) * period;
	if (split_parameter <= closed_domain.Min() + ON_ZERO_TOLERANCE ||
		split_parameter >= closed_domain.Max() - ON_ZERO_TOLERANCE)
	    continue;

	for (int winding = 0; winding < 2; ++winding) {
	    ON_3dPoint first_start;
	    ON_3dPoint split_uv;
	    ON_3dPoint second_end;
	    first_start[closed_direction] = winding == 0 ?
		closed_domain.Min() : closed_domain.Max();
	    first_start[fixed_direction] = fixed_parameter;
	    first_start.z = 0.0;
	    split_uv = first_start;
	    split_uv[closed_direction] = split_parameter;
	    second_end = first_start;
	    second_end[closed_direction] = winding == 0 ?
		closed_domain.Max() : closed_domain.Min();
	    ON_BrepTrim *trims[2] = {first, second};
	    const ON_3dPoint desired_endpoints[3] = {
		first_start, split_uv, second_end
	    };
	    std::unique_ptr<ON_PolylineCurve> curves[2];
	    brlcad::PullbackContext pullback_context;
	    bool exact = true;
	    double maximum_lift_distance = 0.0;
	    int failed_piece = -1;
	    int failed_sample = -1;
	    for (int piece = 0; exact && piece < 2; ++piece) {
		const ON_BrepEdge *edge = trims[piece]->Edge();
		const ON_Interval edge_domain = edge->Domain();
		const ON_Interval trim_domain = trims[piece]->Domain();
		ON_3dPointArray points;
		ON_SimpleArray<double> parameters;
		points.Reserve(kPeriodicBoundaryConstructionSegments + 1);
		parameters.Reserve(kPeriodicBoundaryConstructionSegments + 1);
		double previous_closed_parameter =
		    desired_endpoints[piece][closed_direction];
		for (int sample = 0;
			sample <= kPeriodicBoundaryConstructionSegments;
			sample++) {
		    if ((sample & 63) == 0 &&
			    brlcad::PullbackWorkCancelled())
			return false;
		    const double fraction = static_cast<double>(sample) /
			kPeriodicBoundaryConstructionSegments;
		    const ON_3dPoint edge_point = edge->PointAt(
			edge_domain.ParameterAt(trims[piece]->m_bRev3d ?
			    1.0 - fraction : fraction));
		    ON_2dPoint pulled_uv = ON_2dPoint::UnsetPoint;
		    ON_3dPoint pulled_lift;
		    double pulled_distance = DBL_MAX;
		    if (!edge_point.IsValid() ||
			    !pullback_context.SurfaceClosestPoint(surface, edge_point,
				pulled_uv, pulled_lift, pulled_distance, 0,
				std::max(ON_ZERO_TOLERANCE,
				    topology_tolerance * 0.1),
				topology_tolerance)) {
			failed_piece = piece;
			failed_sample = sample;
			exact = false;
			break;
		    }
		    double closed_parameter = pulled_uv[closed_direction];
		    closed_parameter += round((previous_closed_parameter -
			closed_parameter) / period) * period;
		    previous_closed_parameter = closed_parameter;
		    ON_3dPoint uv;
		    uv[closed_direction] = closed_parameter;
		    uv[fixed_direction] = fixed_parameter;
		    uv.z = 0.0;
		    if (sample == 0)
			uv = desired_endpoints[piece];
		    else if (sample == kPeriodicBoundaryConstructionSegments)
			uv = desired_endpoints[piece + 1];
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    const double lift_distance = lift.IsValid() ?
			lift.DistanceTo(edge_point) : DBL_MAX;
		    maximum_lift_distance = std::max(maximum_lift_distance,
			lift_distance);
		    if (lift_distance > topology_tolerance) {
			failed_piece = piece;
			failed_sample = sample;
			exact = false;
			break;
		    }
		    points.Append(uv);
		    parameters.Append(trim_domain.ParameterAt(fraction));
		}
		const double closed_parameter_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    kPeriodicParameterSnapFraction * std::max(1.0, period));
		if (exact && fabs(previous_closed_parameter -
			desired_endpoints[piece + 1][closed_direction]) >
		    closed_parameter_tolerance) {
		    failed_piece = piece;
		    failed_sample = kPeriodicBoundaryConstructionSegments;
		    exact = false;
		}
		if (!exact) continue;
		curves[piece].reset(new ON_PolylineCurve(points, parameters));
		if (!curves[piece] || !curves[piece]->ChangeDimension(2) ||
			!curves[piece]->IsValid()) {
		    exact = false;
		    failed_piece = piece;
		    continue;
		}
		/* Projection samples prove the vertices.  Mid-interval samples prove
		 * that linear UV interpolation stays on the exact edge locus within
		 * the asserted model tolerance. */
		for (int sample = 0;
			sample < kPeriodicBoundaryConstructionSegments;
			sample++) {
		    const double fraction = (static_cast<double>(sample) + 0.5) /
			kPeriodicBoundaryConstructionSegments;
		    const ON_3dPoint uv = curves[piece]->PointAt(
			trim_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    const ON_3dPoint edge_point = edge->PointAt(
			edge_domain.ParameterAt(trims[piece]->m_bRev3d ?
			    1.0 - fraction : fraction));
		    const double lift_distance = lift.IsValid() &&
			edge_point.IsValid() ? lift.DistanceTo(edge_point) : DBL_MAX;
		    maximum_lift_distance = std::max(maximum_lift_distance,
			lift_distance);
		    if (lift_distance > topology_tolerance) {
			failed_piece = piece;
			failed_sample = sample;
			exact = false;
			break;
		    }
		}
	    }
	    if (!exact) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": split periodic chain L" << loop.m_loop_index
			<< " direction/winding " << closed_direction << '/'
			<< winding << " rejected at piece/sample " << failed_piece
			<< '/' << failed_sample << " lift-distance="
			<< maximum_lift_distance << " tolerance="
			<< topology_tolerance << std::endl;
		continue;
	    }
	    const ON_3dPoint start_lift = surface->PointAt(
		first_start.x, first_start.y);
	    const ON_3dPoint split_lift = surface->PointAt(
		split_uv.x, split_uv.y);
	    const ON_3dPoint end_lift = surface->PointAt(
		second_end.x, second_end.y);
	    if (!start_lift.IsValid() || !split_lift.IsValid() ||
		    !end_lift.IsValid() ||
		    start_lift.DistanceTo(brep->m_V[first->m_vi[0]].point) >
			topology_tolerance ||
		    split_lift.DistanceTo(brep->m_V[first->m_vi[1]].point) >
			topology_tolerance ||
		    end_lift.DistanceTo(brep->m_V[second->m_vi[1]].point) >
			topology_tolerance) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": split periodic chain L" << loop.m_loop_index
			<< " direction/winding " << closed_direction << '/'
			<< winding << " rejected by topology vertex lifts "
			<< start_lift.DistanceTo(brep->m_V[first->m_vi[0]].point)
			<< '/' << split_lift.DistanceTo(
			    brep->m_V[first->m_vi[1]].point) << '/'
			<< end_lift.DistanceTo(
			    brep->m_V[second->m_vi[1]].point) << std::endl;
		continue;
	    }

	    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	    const int first_c2 = brep->AddTrimCurve(curves[0].release());
	    const int second_c2 = brep->AddTrimCurve(curves[1].release());
	    if (first_c2 < 0 || second_c2 < 0 ||
		    !brep->SetTrimCurve(*first, first_c2) ||
		    !brep->SetTrimCurve(*second, second_c2)) {
		*brep = *rollback;
		return false;
	    }
	    brep->SetTrimIsoFlags(*first);
	    brep->SetTrimIsoFlags(*second);
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"regenerated an adjacent split boundary as an exact full-period surface isocurve");
	    return true;
	}
    }
    return false;
}


static bool
validate_periodic_trim_translation(const ON_Surface *surface,
	const ON_BrepTrim &original, const ON_Curve &candidate,
	std::string *failure);

static bool
regenerate_full_period_boundary_chain(ON_Brep *brep, int loop_index,
	int closed_direction, double parameter_tolerance,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type);


/* STEP permits a compact representation of a band on a singly-periodic
 * surface: the two closed boundary edges are separate one-trim loops and the
 * identified surface sides supply the implicit connection between them.
 * OpenNURBS requires that same topology to be cut into a Euclidean UV loop,
 * with two opposite uses of an explicit seam edge.  Insert that cut only when
 * exactly two candidate loops exist and the complete surface isocurve is
 * densely proven to agree on both periodic sides.  No source edge or surface
 * is approximated or moved. */
static size_t
repair_implicit_periodic_face_bands(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair !=
	    brlcad::step::RepairMode::Safe)
	return 0;

    size_t repaired = 0;
    std::map<std::pair<int, int>, double> proven_full_period_boundaries;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	if (brlcad::PullbackWorkCancelled())
	    break;
	ON_BrepFace &face = brep->m_F[fi];
	const ON_Surface *surface = face.SurfaceOf();
	if (!surface || (!surface->IsClosed(0) && !surface->IsClosed(1)))
	    continue;
	int closed_direction = surface->IsClosed(0) ? 0 : 1;
	/* A toroidal face is closed in both parameter directions.  When its STEP
	 * bounds have already been split at a shared native seam, their complete
	 * pcurve chains prove which direction winds one full period.  Use that
	 * direction for the same exact band construction below.  Requiring two
	 * unambiguous winding witnesses avoids guessing which complementary patch
	 * a pair of arbitrary torus curves was intended to bound. */
	if (surface->IsClosed(0) && surface->IsClosed(1)) {
	    int winding_witnesses[2] = {0, 0};
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count()) continue;
		ON_BrepLoop &loop = brep->m_L[li];
		if (loop.TrimCount() != 1) continue;
		for (int direction = 0; direction < 2; ++direction) {
		    const ON_Interval domain = surface->Domain(direction);
		    const double proof_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			kPeriodicParameterSnapFraction *
			std::max(1.0, domain.Length()));
		    const std::pair<int, int> proof_key(
			loop.m_loop_user.i, direction);
		    double proven_open_parameter = 0.0;
		    const std::map<std::pair<int, int>, double>::const_iterator
			proof = proven_full_period_boundaries.find(proof_key);
		    const bool proven = loop.m_loop_user.i > 0 &&
			proof != proven_full_period_boundaries.end();
		    if (proven)
			proven_open_parameter = proof->second;
		    if (proven || regenerate_collapsed_periodic_boundary(brep,
			    loop, surface, direction, proof_tolerance, wrapper,
			    entity_id, entity_type, &proven_open_parameter)) {
			++winding_witnesses[direction];
			if (!proven && loop.m_loop_user.i > 0)
			    proven_full_period_boundaries[proof_key] =
				proven_open_parameter;
		    }
		}
	    }
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li >= 0 && li < brep->m_L.Count())
		    regenerate_split_periodic_boundary_chain(brep, brep->m_L[li],
			surface, wrapper, entity_id, entity_type);
	    }
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count()) continue;
		const ON_BrepLoop &loop = brep->m_L[li];
		const ON_BrepTrim *first = loop.Trim(0);
		const ON_BrepTrim *last = loop.Trim(loop.TrimCount() - 1);
		if (!first || !last) continue;
		const ON_3dPoint start = first->PointAtStart();
		const ON_3dPoint end = last->PointAtEnd();
		if (!start.IsValid() || !end.IsValid()) continue;
		for (int direction = 0; direction < 2; ++direction) {
		    const ON_Interval domain = surface->Domain(direction);
		    const ON_Interval other_domain = surface->Domain(1 - direction);
		    if (!domain.IsIncreasing() || !other_domain.IsIncreasing())
			continue;
		    const double direction_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			kPeriodicParameterSnapFraction *
			std::max(1.0, domain.Length()));
		    const double other_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			kPeriodicParameterSnapFraction *
			std::max(1.0, other_domain.Length()));
		    if (fabs(fabs(end[direction] - start[direction]) -
			    domain.Length()) <= direction_tolerance &&
			fabs(end[1 - direction] - start[1 - direction]) <=
			    other_tolerance)
			++winding_witnesses[direction];
		}
	    }
	    if (winding_witnesses[0] >= 2 &&
		    winding_witnesses[0] > winding_witnesses[1])
		closed_direction = 0;
	    else if (winding_witnesses[1] >= 2 &&
		    winding_witnesses[1] > winding_witnesses[0])
		closed_direction = 1;
	    else {
		if (wrapper->Verbose())
		{
		    std::cerr << entity_type << " #" << entity_id
			<< ": doubly periodic face F" << fi
			<< " lacked an unambiguous full-period boundary direction ("
			<< winding_witnesses[0] << '/' << winding_witnesses[1]
			<< " witnesses)";
		    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
			const int li = face.m_li[fli];
			if (li < 0 || li >= brep->m_L.Count()) continue;
			const ON_BrepLoop &loop = brep->m_L[li];
			const ON_BrepTrim *first = loop.Trim(0);
			const ON_BrepTrim *last = loop.Trim(loop.TrimCount() - 1);
			if (!first || !last) continue;
			const ON_3dPoint start = first->PointAtStart();
			const ON_3dPoint end = last->PointAtEnd();
			std::cerr << " L" << li << "/STEP" << loop.m_loop_user.i
			    << "(trims=" << loop.TrimCount() << ',' << start.x << ':'
			    << start.y << "->" << end.x << ':' << end.y << ')';
		    }
		    std::cerr << std::endl;
		}
		continue;
	    }
	}
	const int open_direction = 1 - closed_direction;
	const ON_Interval closed_domain = surface->Domain(closed_direction);
	if (!closed_domain.IsIncreasing())
	    continue;
	const double parameter_tolerance = std::max(ON_ZERO_TOLERANCE *
	    kNumericalToleranceScale, kPeriodicParameterSnapFraction *
	    std::max(1.0, closed_domain.Length()));

	/* STEP can bound a spherical cap intrinsically with two or more ordinary
	 * arcs which together wind once around the sphere.  OpenNURBS requires the
	 * equivalent rectangular parameter-space loop: the physical boundary,
	 * two opposite uses of an exact meridian seam, and a singular pole trim.
	 * Normalize the complete edge chain to one native period first, then use
	 * the existing densely validated pole-cut construction.  Requiring one
	 * face loop and a proven full-period chain avoids turning an ordinary inner
	 * loop or a partial latitude arc into an artificial pole cut. */
    if (face.m_li.Count() == 1) {
	const int only_loop = face.m_li[0];
	bool have_parameter = false;
	double first_unwrapped = 0.0;
	double previous_unwrapped = 0.0;
	if (only_loop >= 0 && only_loop < brep->m_L.Count()) {
	    const ON_BrepLoop &loop = brep->m_L[only_loop];
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		const ON_Curve *curve = trim ? trim->TrimCurveOf() : NULL;
		if (!curve || !curve->Domain().IsIncreasing()) {
		    have_parameter = false;
		    break;
		}
		const ON_Interval curve_domain = curve->Domain();
		for (int sample = 0;
			sample <= kBoundaryParameterSearchSegments; ++sample) {
		    if (brlcad::PullbackWorkCancelled()) {
			have_parameter = false;
			break;
		    }
		    const ON_3dPoint point = curve->PointAt(
			curve_domain.ParameterAt(static_cast<double>(sample) /
			kBoundaryParameterSearchSegments));
		    if (!point.IsValid() ||
			    !std::isfinite(point[closed_direction])) {
			have_parameter = false;
			break;
		    }
		    double unwrapped = point[closed_direction];
		    if (have_parameter)
			unwrapped += round((previous_unwrapped - unwrapped) /
			    closed_domain.Length()) * closed_domain.Length();
		    else {
			first_unwrapped = unwrapped;
			have_parameter = true;
		    }
		    previous_unwrapped = unwrapped;
		}
		if (!have_parameter)
		    break;
	    }
	}
	const double net_winding = have_parameter ?
	    previous_unwrapped - first_unwrapped : 0.0;
	const bool full_period_chain = have_parameter &&
	    fabs(fabs(net_winding) - closed_domain.Length()) <=
	    parameter_tolerance;
	/* A one-loop face which merely crosses the native seam is an ordinary
	 * bounded patch, not an implicit spherical cap.  Requiring one full
	 * pcurve winding prevents the exact-edge regeneration below from spending
	 * the item budget trying to force a contractible loop around the surface.
	 * Multi-loop periodic bands retain their independent exact-edge proof,
	 * since their supplied pcurves may be the malformed data being repaired. */
	if (!full_period_chain && wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": one-loop full-period boundary candidate L" << only_loop
		<< " rejected because net pcurve winding " << net_winding
		<< " does not equal one period " << closed_domain.Length()
		<< std::endl;
	if (full_period_chain && regenerate_full_period_boundary_chain(brep,
		only_loop,
		    closed_direction, parameter_tolerance, wrapper, entity_id,
		    entity_type)) {
		ON_BrepLoop &normalized_loop = brep->m_L[only_loop];
		ON_BrepTrim *first = normalized_loop.Trim(0);
		ON_BrepTrim *last = normalized_loop.Trim(
		    normalized_loop.TrimCount() - 1);
		const ON_BrepFace *normalized_face = normalized_loop.Face();
		const ON_Surface *normalized_surface = normalized_face ?
		    normalized_face->SurfaceOf() : NULL;
		double topology_tolerance = LocalUnits::tolerance;
		for (int lti = 0; lti < normalized_loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = normalized_loop.Trim(lti);
		    if (!trim) continue;
		    topology_tolerance = std::max(topology_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
		    if (trim->Edge())
			topology_tolerance = std::max(topology_tolerance,
			    trim->Edge()->m_tolerance);
		}
		const ON_3dPoint boundary_end_3d = last ?
		    last->PointAtEnd() : ON_3dPoint::UnsetPoint;
		const ON_3dPoint boundary_start_3d = first ?
		    first->PointAtStart() : ON_3dPoint::UnsetPoint;
		if (first && last && normalized_surface &&
			step_insert_periodic_pole_cut(brep, normalized_loop,
			normalized_surface, *last,
			ON_2dPoint(boundary_end_3d.x, boundary_end_3d.y),
			ON_2dPoint(boundary_start_3d.x, boundary_start_3d.y),
			topology_tolerance)) {
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"inserted an exact pole cut for a multi-edge full-period boundary");
		    ++repaired;
		    continue;
		}
		/* regenerate_full_period_boundary_chain installs a deep BREP copy,
		 * invalidating the face/surface references captured at the top of this
		 * iteration.  Even if the pole proof is rejected, leave the exact
		 * normalized boundary for later repair stages and advance without using
		 * those stale references. */
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": normalized multi-edge full-period loop L"
			<< only_loop
			<< " but no exact surface-pole cut was proven" << std::endl;
		continue;
	    }
	    /* A complete intrinsic boundary need not begin at the native seam.
	     * Materialize a translated seam crossing inside an open STEP edge,
	     * then restart with the compacted edge-use graph.  The next pass can
	     * rotate the chain at that proven seam vertex and insert the pole cut. */
	    if (split_open_periodic_boundary_crossing(brep, face, only_loop,
		    closed_direction, parameter_tolerance, wrapper, entity_id,
		    entity_type)) {
		--fi;
		continue;
	    }
	}

	struct BoundaryCandidate {
	    int loop_index;
	    std::vector<int> trim_indices;
	    std::vector<int> period_shifts;
	    int vertex_index;
	    double open_parameter;
	};
	std::vector<BoundaryCandidate> candidates;
	const auto proven_chain_join = [brep, surface, closed_direction,
		&closed_domain](const ON_BrepTrim *previous,
		const ON_BrepTrim *next, double previous_closed,
		double previous_open, double next_closed, double next_open) {
	    if (!previous || !next || previous->m_vi[1] < 0 ||
		    previous->m_vi[1] != next->m_vi[0] ||
		    previous->m_vi[1] >= brep->m_V.Count())
		return false;
	    double tolerance = LocalUnits::tolerance;
	    if (previous->Edge())
		tolerance = std::max(tolerance, previous->Edge()->m_tolerance);
	    if (next->Edge())
		tolerance = std::max(tolerance, next->Edge()->m_tolerance);
	    tolerance = std::max(tolerance,
		std::max(previous->m_tolerance[0], previous->m_tolerance[1]));
	    tolerance = std::max(tolerance,
		std::max(next->m_tolerance[0], next->m_tolerance[1]));
	    tolerance = std::max(tolerance,
		brep->m_V[previous->m_vi[1]].m_tolerance);
	    const double period = closed_domain.Length();
	    const auto native_parameter = [&closed_domain, period](double value) {
		value -= floor((value - closed_domain.Min()) / period) * period;
		if (value > closed_domain.Max()) value -= period;
		if (value < closed_domain.Min()) value += period;
		return std::max(closed_domain.Min(),
		    std::min(closed_domain.Max(), value));
	    };
	    ON_3dPoint first;
	    ON_3dPoint second;
	    first[closed_direction] = native_parameter(previous_closed);
	    first[1 - closed_direction] = previous_open;
	    first.z = 0.0;
	    second[closed_direction] = native_parameter(next_closed);
	    second[1 - closed_direction] = next_open;
	    second.z = 0.0;
	    /* Compare on one adjacent periodic image before taking the midpoint;
	     * averaging opposite native boundaries would test the antipode. */
	    second[closed_direction] += round((first[closed_direction] -
		second[closed_direction]) / period) * period;
	    ON_3dPoint middle = 0.5 * (first + second);
	    first[closed_direction] = native_parameter(first[closed_direction]);
	    second[closed_direction] = native_parameter(second[closed_direction]);
	    middle[closed_direction] = native_parameter(middle[closed_direction]);
	    const ON_3dPoint first_lift = surface->PointAt(first.x, first.y);
	    const ON_3dPoint second_lift = surface->PointAt(second.x, second.y);
	    const ON_3dPoint middle_lift = surface->PointAt(middle.x, middle.y);
	    const ON_3dPoint &vertex = brep->m_V[previous->m_vi[1]].point;
	    return first_lift.IsValid() && second_lift.IsValid() &&
		middle_lift.IsValid() &&
		first_lift.DistanceTo(vertex) <= tolerance &&
		second_lift.DistanceTo(vertex) <= tolerance &&
		middle_lift.DistanceTo(vertex) <= tolerance;
	};
	bool topology_split = false;
	for (int fli = 0; fli < face.m_li.Count(); ++fli) {
	    const int li = face.m_li[fli];
	    if (li < 0 || li >= brep->m_L.Count())
		continue;
	    ON_BrepLoop &loop = brep->m_L[li];
	    if (loop.TrimCount() < 1)
		continue;
	    ON_BrepTrim *trim = loop.Trim(0);
	    ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    bool periodic_closure = loop.TrimCount() == 1 && trim && edge &&
		periodic_loop_closure(brep, &loop, trim, trim,
		    parameter_tolerance);
	    const std::pair<int, int> proof_key(loop.m_loop_user.i,
		closed_direction);
	    double proven_open_parameter = trim ?
		trim->PointAtStart()[open_direction] : 0.0;
	    const std::map<std::pair<int, int>, double>::const_iterator proof =
		proven_full_period_boundaries.find(proof_key);
	    bool full_period_proven = loop.m_loop_user.i > 0 &&
		proof != proven_full_period_boundaries.end();
	    if (full_period_proven)
		proven_open_parameter = proof->second;
	    if (loop.TrimCount() == 1 && trim && edge &&
		    (full_period_proven ||
		     regenerate_collapsed_periodic_boundary(brep, loop, surface,
			 closed_direction, parameter_tolerance, wrapper, entity_id,
			 entity_type, &proven_open_parameter))) {
		if (!full_period_proven && loop.m_loop_user.i > 0)
		    proven_full_period_boundaries[proof_key] =
			proven_open_parameter;
		ON_3dPoint proven_min_uv;
		proven_min_uv[closed_direction] = closed_domain.Min();
		proven_min_uv[open_direction] = proven_open_parameter;
		proven_min_uv.z = 0.0;
		ON_3dPoint proven_max_uv = proven_min_uv;
		proven_max_uv[closed_direction] = closed_domain.Max();
		const ON_3dPoint proven_min_lift = surface->PointAt(
		    proven_min_uv.x, proven_min_uv.y);
		const ON_3dPoint proven_max_lift = surface->PointAt(
		    proven_max_uv.x, proven_max_uv.y);
		const ON_3dPoint &vertex = brep->m_V[trim->m_vi[0]].point;
		const double topology_tolerance = std::max(LocalUnits::tolerance,
		    std::max(edge->m_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
		const bool proven_vertex_on_native_seam =
		    proven_min_lift.IsValid() && proven_max_lift.IsValid() &&
		    proven_min_lift.DistanceTo(vertex) <= topology_tolerance &&
		    proven_max_lift.DistanceTo(vertex) <= topology_tolerance;
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": proven periodic boundary F" << fi << "/L" << li
			<< " direction=" << closed_direction << " open="
			<< proven_open_parameter << " native-vertex="
			<< (proven_vertex_on_native_seam ? "yes" : "no")
			<< " distances=" << proven_min_lift.DistanceTo(vertex)
			<< '/' << proven_max_lift.DistanceTo(vertex)
			<< " tolerance=" << topology_tolerance << std::endl;
		if (!proven_vertex_on_native_seam &&
			split_periodic_boundary_at_native_seam(brep, li,
			    trim->m_trim_index, surface, closed_direction,
			    proven_open_parameter, wrapper, entity_id,
			    entity_type)) {
		    topology_split = true;
		    break;
		}
		if (proven_vertex_on_native_seam &&
			regenerate_native_seam_periodic_boundary(brep, loop,
			    surface, closed_direction, wrapper, entity_id,
			    entity_type, proven_open_parameter)) {
		    trim = loop.Trim(0);
		    edge = trim ? trim->Edge() : NULL;
		    periodic_closure = trim && edge && periodic_loop_closure(
			brep, &loop, trim, trim, parameter_tolerance);
		    if (trim && periodic_closure) {
			const ON_3dPoint boundary_end_3d = trim->PointAtEnd();
			const ON_3dPoint boundary_start_3d = trim->PointAtStart();
			const ON_2dPoint boundary_end(boundary_end_3d.x,
			    boundary_end_3d.y);
			const ON_2dPoint boundary_start(boundary_start_3d.x,
			    boundary_start_3d.y);
			/* A full-period boundary on a multi-loop spherical face may
			 * be one side of an annular band.  Do not turn it into an
			 * OpenNURBS pole-cut seam before the other face bounds have
			 * been classified and paired below; OpenNURBS rejects a seam
			 * installed in the source FACE_BOUND inner loop. */
			if (face.m_li.Count() == 1 &&
				step_insert_periodic_pole_cut(brep, loop, surface,
				*trim, boundary_end, boundary_start,
				topology_tolerance)) {
			    wrapper->RecordRepair(entity_id, entity_type,
				"edge_loop",
				"inserted an exact paired seam cut for a proven full-period boundary and surface pole");
			    topology_split = true;
			    break;
			}
		    }
		}
	    }
	    if (wrapper->Verbose() && trim && edge &&
		    edge->m_vi[0] == edge->m_vi[1]) {
		const ON_3dPoint start = trim->PointAtStart();
		const ON_3dPoint end = trim->PointAtEnd();
		std::cerr << entity_type << " #" << entity_id
		    << ": implicit periodic singleton F" << fi << "/L" << li
		    << "/T" << trim->m_trim_index << " uv=" << start.x << ':'
		    << start.y << "->" << end.x << ':' << end.y
		    << " trim-vertices=" << trim->m_vi[0] << '/'
		    << trim->m_vi[1] << " edge-vertex=" << edge->m_vi[0]
		    << " periodic-closure=" << (periodic_closure ? "yes" : "no")
		    << std::endl;
	    }
	    if (loop.TrimCount() == 1) {
		if (!trim || !edge || edge->m_vi[0] != edge->m_vi[1] ||
			trim->m_vi[0] != trim->m_vi[1] ||
			trim->m_vi[0] != edge->m_vi[0] || !trim->TrimCurveOf() ||
			!periodic_closure)
		    continue;
		const ON_3dPoint start = trim->PointAtStart();
		const ON_3dPoint end = trim->PointAtEnd();
		if (!start.IsValid() || !end.IsValid() ||
			fabs(start[open_direction] - end[open_direction]) >
			    parameter_tolerance)
		    continue;
		BoundaryCandidate candidate;
		candidate.loop_index = li;
		candidate.trim_indices.push_back(trim->m_trim_index);
		candidate.period_shifts.push_back(0);
		candidate.vertex_index = trim->m_vi[0];
		candidate.open_parameter = 0.5 *
		    (start[open_direction] + end[open_direction]);
		candidates.push_back(candidate);
		continue;
	    }
	    ON_BrepTrim *first_trim = loop.Trim(0);
	    ON_BrepTrim *second_trim = loop.Trim(1);
	    if (loop.TrimCount() > 2) {
		/* A periodic band boundary need not be a single circle.  AP214
		 * models commonly use a scalloped or segmented inner boundary whose
		 * intermediate pcurve joins are continuous, with only the final-to-
		 * first join separated by one native period.  Preserve the complete
		 * STEP chain as one candidate; the band construction below will join
		 * it to the other physical boundary with the one exact shared seam
		 * OpenNURBS requires. */
		/* The split point can occur anywhere in the cyclic STEP edge order.
		 * Rotate the chain to that proven native-seam vertex, then translate
		 * each complete pcurve by an integral period so the same exact lifts
		 * form one Euclidean path from the native minimum to maximum (or the
		 * reverse).  No arbitrary seam angle or topology is inferred. */
		const double period = closed_domain.Length();
		bool added = false;
		for (int cut = 0; !added && cut < loop.TrimCount(); ++cut) {
		    ON_BrepTrim *before = loop.Trim(cut);
		    ON_BrepTrim *after = loop.Trim((cut + 1) % loop.TrimCount());
		    if (!before || !after || !before->TrimCurveOf() ||
			    !after->TrimCurveOf() ||
			    before->m_vi[1] != after->m_vi[0])
			continue;
		    const ON_3dPoint cut_end = before->PointAtEnd();
		    const ON_3dPoint cut_start = after->PointAtStart();
		    if (!cut_end.IsValid() || !cut_start.IsValid() ||
			    fabs(cut_end[open_direction] -
				cut_start[open_direction]) > parameter_tolerance)
			continue;
		    const double cut_closed_gap = fabs(
			cut_end[closed_direction] - cut_start[closed_direction]);
		    if (cut_closed_gap > parameter_tolerance &&
			    fabs(cut_closed_gap - period) > parameter_tolerance)
			continue;
		    const double native_image = closed_domain.Min() +
			round((cut_start[closed_direction] - closed_domain.Min()) /
			    period) * period;
		    if (fabs(cut_start[closed_direction] - native_image) >
			    parameter_tolerance)
			continue;

		    for (int direction = 0; !added && direction < 2; ++direction) {
			const bool increasing = direction == 0;
			const double target_start = increasing ?
			    closed_domain.Min() : closed_domain.Max();
			const double target_end = increasing ?
			    closed_domain.Max() : closed_domain.Min();
			std::vector<int> chain_trim_indices;
			std::vector<int> chain_period_shifts;
			double expected_closed = target_start;
			double expected_open = cut_start[open_direction];
			ON_BrepTrim *previous_trim = NULL;
			bool continuous_chain = true;
			for (int offset = 0; continuous_chain &&
				offset < loop.TrimCount(); ++offset) {
			    ON_BrepTrim *current = loop.Trim(
				(cut + 1 + offset) % loop.TrimCount());
			    if (!current || !current->TrimCurveOf()) {
				continuous_chain = false;
				break;
			    }
			    const ON_3dPoint start = current->PointAtStart();
			    const ON_3dPoint end = current->PointAtEnd();
			    if (!start.IsValid() || !end.IsValid()) {
				continuous_chain = false;
				break;
			    }
			    const int shift = static_cast<int>(llround(
				(expected_closed - start[closed_direction]) / period));
			    const double shifted_start = start[closed_direction] +
				shift * period;
			    const bool exact_join = fabs(shifted_start -
				expected_closed) <= parameter_tolerance &&
				fabs(start[open_direction] - expected_open) <=
				    parameter_tolerance;
			    if (!exact_join && (!previous_trim ||
				    !proven_chain_join(previous_trim, current,
					expected_closed, expected_open, shifted_start,
					start[open_direction]))) {
				continuous_chain = false;
				break;
			    }
			    chain_trim_indices.push_back(current->m_trim_index);
			    chain_period_shifts.push_back(shift);
			    expected_closed = end[closed_direction] + shift * period;
			    expected_open = end[open_direction];
			    previous_trim = current;
			}
			if (!continuous_chain || chain_trim_indices.empty() ||
				fabs(expected_closed - target_end) >
				    parameter_tolerance ||
				fabs(expected_open - cut_start[open_direction]) >
				    parameter_tolerance)
			    continue;
			ON_BrepTrim *chain_first = brep->m_T.At(
			    chain_trim_indices.front());
			ON_BrepTrim *chain_last = brep->m_T.At(
			    chain_trim_indices.back());
			if (!chain_first || !chain_last ||
				chain_first->m_vi[0] != chain_last->m_vi[1])
			    continue;
			BoundaryCandidate candidate;
			candidate.loop_index = li;
			candidate.trim_indices.swap(chain_trim_indices);
			candidate.period_shifts.swap(chain_period_shifts);
			candidate.vertex_index = chain_first->m_vi[0];
			candidate.open_parameter = 0.5 *
			    (cut_start[open_direction] + expected_open);
			candidates.push_back(candidate);
			added = true;
		    }
		}
		continue;
	    }
	    if (!first_trim || !second_trim || !first_trim->TrimCurveOf() ||
		    !second_trim->TrimCurveOf() ||
		    first_trim->m_vi[1] != second_trim->m_vi[0] ||
		    second_trim->m_vi[1] != first_trim->m_vi[0])
		continue;
	    const ON_3dPoint first_start = first_trim->PointAtStart();
	    const ON_3dPoint first_end = first_trim->PointAtEnd();
	    const ON_3dPoint second_start = second_trim->PointAtStart();
	    const ON_3dPoint second_end = second_trim->PointAtEnd();
	    const bool increasing_periodic_chain =
		fabs(first_start[closed_direction] - closed_domain.Min()) <=
		    parameter_tolerance &&
		fabs(second_end[closed_direction] - closed_domain.Max()) <=
		    parameter_tolerance;
	    const bool decreasing_periodic_chain =
		fabs(first_start[closed_direction] - closed_domain.Max()) <=
		    parameter_tolerance &&
		fabs(second_end[closed_direction] - closed_domain.Min()) <=
		    parameter_tolerance;
	    if (!first_start.IsValid() || !first_end.IsValid() ||
		    !second_start.IsValid() || !second_end.IsValid() ||
		    first_end.DistanceTo(second_start) > parameter_tolerance ||
		    (!increasing_periodic_chain &&
		     !decreasing_periodic_chain) ||
		    fabs(first_start[open_direction] - first_end[open_direction]) >
			parameter_tolerance ||
		    fabs(first_start[open_direction] - second_start[open_direction]) >
			parameter_tolerance ||
		    fabs(first_start[open_direction] - second_end[open_direction]) >
			parameter_tolerance)
		continue;
	    BoundaryCandidate candidate;
	    candidate.loop_index = li;
	    candidate.trim_indices.push_back(first_trim->m_trim_index);
	    candidate.trim_indices.push_back(second_trim->m_trim_index);
	    candidate.period_shifts.push_back(0);
	    candidate.period_shifts.push_back(0);
	    candidate.vertex_index = first_trim->m_vi[0];
	    candidate.open_parameter = first_start[open_direction];
	    candidates.push_back(candidate);
	}
	/* A supplied multi-edge boundary can cross the native parameter seam at
	 * an edge endpoint while its pcurve itself remains on the principal
	 * branch.  Whole-curve translations cannot unwrap that final edge.  Once
	 * the other physical boundary proves this is a periodic face band, rebuild
	 * the missing winding directly from the exact 3-D STEP edge chain.  The
	 * helper accepts only a native-seam topology vertex and densely validates
	 * every regenerated edge lift before changing the BREP. */
	const int other_loop = face.m_li.Count() == 2 && candidates.size() == 1 ?
	    (face.m_li[0] == candidates[0].loop_index ?
		face.m_li[1] : face.m_li[0]) : -1;
	if (!topology_split && other_loop >= 0) {
	    if (regenerate_full_period_boundary_chain(brep, other_loop,
		    closed_direction, parameter_tolerance, wrapper, entity_id,
		    entity_type))
		topology_split = true;
	}
	/* One boundary may already be a native full-period candidate while the
	 * other crosses the same private surface seam inside an open STEP edge.
	 * Materialize that exact crossing as a shared topology vertex, then restart
	 * this face with the compacted edge-use graph. */
	if (!topology_split && other_loop >= 0 &&
		split_open_periodic_boundary_crossing(brep, face, other_loop,
		    closed_direction, parameter_tolerance, wrapper, entity_id,
		    entity_type))
	    topology_split = true;
	if (topology_split) {
	    --fi;
	    continue;
	}
	const auto report_rejection = [brep, wrapper, entity_id, &entity_type, fi,
		&face, &candidates](const char *reason) {
	    if (!wrapper->Verbose() || candidates.empty())
		return;
	    std::cerr << entity_type << " #" << entity_id
		<< ": implicit periodic face-band candidate F" << fi
		<< "/STEP" << face.m_face_user.i << " rejected: " << reason
		<< " (" << candidates.size() << " boundary loops";
	    for (std::vector<BoundaryCandidate>::const_iterator candidate =
		    candidates.begin(); candidate != candidates.end(); ++candidate) {
		std::cerr << " L" << candidate->loop_index << "/T";
		for (size_t trim_offset = 0;
			trim_offset < candidate->trim_indices.size(); ++trim_offset) {
		    if (trim_offset) std::cerr << ',';
		    std::cerr << candidate->trim_indices[trim_offset];
		}
		std::cerr << '@' << candidate->open_parameter;
	    }
	    std::cerr << ')';
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count()) continue;
		const ON_BrepLoop &loop = brep->m_L[li];
		std::cerr << " loop L" << li << "/STEP" << loop.m_loop_user.i
		    << '[';
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = loop.Trim(lti);
		    if (lti) std::cerr << ',';
		    if (!trim) {
			std::cerr << "null";
			continue;
		    }
		    const ON_3dPoint start = trim->PointAtStart();
		    const ON_3dPoint end = trim->PointAtEnd();
		    std::cerr << 'T' << trim->m_trim_index << ':' << start.x << ':'
			<< start.y << "->" << end.x << ':' << end.y;
		}
		std::cerr << ']';
	    }
	    std::cerr << std::endl;
	};
	/* More than two periodic boundary loops can encode multiple disjoint
	 * bands.  Pairing those without an additional source relationship would
	 * be a topology guess, so leave such a face unchanged. */
	if (candidates.size() != 2) {
	    report_rejection("the candidate count was not exactly two");
	    continue;
	}
	if (candidates[1].open_parameter < candidates[0].open_parameter)
	    std::swap(candidates[0], candidates[1]);
	BoundaryCandidate lower = candidates[0];
	BoundaryCandidate upper = candidates[1];
	if (upper.open_parameter - lower.open_parameter <= parameter_tolerance ||
		lower.vertex_index == upper.vertex_index) {
	    report_rejection("the boundary intervals or vertices were not distinct");
	    continue;
	}
	const auto boundary_tolerance = [brep](const BoundaryCandidate &boundary) {
	    double tolerance = LocalUnits::tolerance;
	    for (std::vector<int>::const_iterator ti =
		    boundary.trim_indices.begin(); ti != boundary.trim_indices.end();
		    ++ti) {
		if (*ti < 0 || *ti >= brep->m_T.Count()) continue;
		const ON_BrepTrim &trim = brep->m_T[*ti];
		tolerance = std::max(tolerance,
		    std::max(trim.m_tolerance[0], trim.m_tolerance[1]));
		if (trim.m_ei >= 0 && trim.m_ei < brep->m_E.Count())
		    tolerance = std::max(tolerance,
			brep->m_E[trim.m_ei].m_tolerance);
	    }
	    return tolerance;
	};
	const double band_tolerance = std::max(boundary_tolerance(lower),
	    boundary_tolerance(upper));

	const ON_3dPoint &lower_vertex = brep->m_V[lower.vertex_index].point;
	const ON_3dPoint &upper_vertex = brep->m_V[upper.vertex_index].point;
	const double closed_min = closed_domain.Min();
	const double closed_max = closed_domain.Max();
	ON_3dPoint lower_uv_min;
	ON_3dPoint lower_uv_max;
	ON_3dPoint upper_uv_min;
	ON_3dPoint upper_uv_max;
	lower_uv_min[closed_direction] = closed_min;
	lower_uv_min[open_direction] = lower.open_parameter;
	lower_uv_min.z = 0.0;
	lower_uv_max = lower_uv_min;
	lower_uv_max[closed_direction] = closed_max;
	upper_uv_min[closed_direction] = closed_min;
	upper_uv_min[open_direction] = upper.open_parameter;
	upper_uv_min.z = 0.0;
	upper_uv_max = upper_uv_min;
	upper_uv_max[closed_direction] = closed_max;
	const ON_3dPoint lower_lifts[2] = {
	    surface->PointAt(lower_uv_min.x, lower_uv_min.y),
	    surface->PointAt(lower_uv_max.x, lower_uv_max.y)
	};
	const ON_3dPoint upper_lifts[2] = {
	    surface->PointAt(upper_uv_min.x, upper_uv_min.y),
	    surface->PointAt(upper_uv_max.x, upper_uv_max.y)
	};
	bool endpoint_proof = true;
	for (int side = 0; side < 2; ++side) {
	    endpoint_proof = endpoint_proof && lower_lifts[side].IsValid() &&
		upper_lifts[side].IsValid() &&
		lower_lifts[side].DistanceTo(lower_vertex) <=
		    band_tolerance &&
		upper_lifts[side].DistanceTo(upper_vertex) <=
		    band_tolerance;
	}
	if (!endpoint_proof) {
	    report_rejection("a periodic boundary did not lift to its STEP vertex");
	    continue;
	}

	std::unique_ptr<ON_Curve> seam_curve(surface->IsoCurve(open_direction,
	    closed_min));
	const ON_Interval seam_domain(lower.open_parameter,
	    upper.open_parameter);
	if (!seam_curve || !seam_domain.IsIncreasing() ||
		!seam_curve->Trim(seam_domain) || !seam_curve->IsValid()) {
	    report_rejection("the exact surface isocurve could not be trimmed");
	    continue;
	}
	if (seam_curve->PointAtStart().DistanceTo(lower_vertex) >
		seam_curve->PointAtEnd().DistanceTo(lower_vertex) &&
		!seam_curve->Reverse())
	    continue;
	bool seam_proof = true;
	const ON_Interval curve_domain = seam_curve->Domain();
	for (int sample = 0; seam_proof && sample <= kDenseValidationSegments;
		++sample) {
	    if (brlcad::PullbackWorkCancelled()) {
		seam_proof = false;
		break;
	    }
	    const double fraction = static_cast<double>(sample) /
		kDenseValidationSegments;
	    const double open_parameter = seam_domain.ParameterAt(fraction);
	    ON_3dPoint uv_min;
	    ON_3dPoint uv_max;
	    uv_min[closed_direction] = closed_min;
	    uv_min[open_direction] = open_parameter;
	    uv_min.z = 0.0;
	    uv_max = uv_min;
	    uv_max[closed_direction] = closed_max;
	    const ON_3dPoint curve_point = seam_curve->PointAt(
		curve_domain.ParameterAt(fraction));
	    const ON_3dPoint min_lift = surface->PointAt(uv_min.x, uv_min.y);
	    const ON_3dPoint max_lift = surface->PointAt(uv_max.x, uv_max.y);
	    seam_proof = curve_point.IsValid() && min_lift.IsValid() &&
		max_lift.IsValid() &&
		curve_point.DistanceTo(min_lift) <= band_tolerance &&
		curve_point.DistanceTo(max_lift) <= band_tolerance;
	}
	if (!seam_proof || seam_curve->PointAtStart().DistanceTo(lower_vertex) >
		band_tolerance ||
		seam_curve->PointAtEnd().DistanceTo(upper_vertex) >
		band_tolerance) {
	    report_rejection("the surface seam did not pass dense lift validation");
	    continue;
	}

	std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	const auto install_period_shifts = [brep, surface, closed_direction,
		&closed_domain](const BoundaryCandidate &boundary) {
	    if (boundary.trim_indices.size() != boundary.period_shifts.size())
		return false;
	    for (size_t offset = 0; offset < boundary.trim_indices.size(); ++offset) {
		const int ti = boundary.trim_indices[offset];
		const int period_shift = boundary.period_shifts[offset];
		if (period_shift == 0)
		    continue;
		if (ti < 0 || ti >= brep->m_T.Count())
		    return false;
		ON_BrepTrim &trim = brep->m_T[ti];
		std::unique_ptr<ON_Curve> translated(trim.DuplicateCurve());
		ON_Xform transform(ON_Xform::IdentityTransformation);
		transform.m_xform[closed_direction][3] =
		    period_shift * closed_domain.Length();
		std::string failure;
		if (!translated || !translated->Transform(transform) ||
			!translated->ChangeDimension(2) || !translated->IsValid() ||
			!validate_periodic_trim_translation(surface, trim,
			    *translated, &failure))
		    return false;
		const int c2 = brep->AddTrimCurve(translated.release());
		if (c2 < 0 || !brep->SetTrimCurve(trim, c2))
		    return false;
		brep->SetTrimIsoFlags(trim);
	    }
	    return true;
	};
	if (!install_period_shifts(lower) || !install_period_shifts(upper)) {
	    report_rejection("an integral-period boundary translation failed dense lift validation");
	    *brep = *rollback;
	    continue;
	}
	size_t snapped_band_joins = 0;
	const auto snap_boundary_joins = [brep, surface, &snapped_band_joins](
		const BoundaryCandidate &boundary) {
	    for (size_t offset = 1; offset < boundary.trim_indices.size(); ++offset) {
		const int previous_index = boundary.trim_indices[offset - 1];
		const int next_index = boundary.trim_indices[offset];
		if (previous_index < 0 || previous_index >= brep->m_T.Count() ||
			next_index < 0 || next_index >= brep->m_T.Count())
		    return false;
		ON_BrepTrim &previous = brep->m_T[previous_index];
		ON_BrepTrim &next = brep->m_T[next_index];
		const ON_3dPoint previous_uv = previous.PointAtEnd();
		const ON_3dPoint next_uv = next.PointAtStart();
		if (previous_uv.DistanceTo(next_uv) <= ON_ZERO_TOLERANCE)
		    continue;
		if (previous.m_vi[1] < 0 || previous.m_vi[1] != next.m_vi[0] ||
			previous.m_vi[1] >= brep->m_V.Count())
		    return false;
		double tolerance = LocalUnits::tolerance;
		if (previous.Edge())
		    tolerance = std::max(tolerance, previous.Edge()->m_tolerance);
		if (next.Edge())
		    tolerance = std::max(tolerance, next.Edge()->m_tolerance);
		tolerance = std::max(tolerance,
		    std::max(previous.m_tolerance[0], previous.m_tolerance[1]));
		tolerance = std::max(tolerance,
		    std::max(next.m_tolerance[0], next.m_tolerance[1]));
		tolerance = std::max(tolerance,
		    brep->m_V[previous.m_vi[1]].m_tolerance);
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		const ON_3dPoint common = 0.5 * (previous_uv + next_uv);
		const ON_3dPoint common_lift = surface->PointAt(common.x, common.y);
		const ON_3dPoint &vertex = brep->m_V[previous.m_vi[1]].point;
		if (!previous_lift.IsValid() || !next_lift.IsValid() ||
			!common_lift.IsValid() ||
			previous_lift.DistanceTo(vertex) > tolerance ||
			next_lift.DistanceTo(vertex) > tolerance ||
			common_lift.DistanceTo(vertex) > tolerance)
		    return false;

		std::unique_ptr<ON_Curve> previous_candidate(
		    previous.DuplicateCurve());
		std::unique_ptr<ON_Curve> next_candidate(next.DuplicateCurve());
		if (!previous_candidate || !next_candidate ||
			!previous_candidate->SetEndPoint(common) ||
			!next_candidate->SetStartPoint(common) ||
			!previous_candidate->ChangeDimension(2) ||
			!next_candidate->ChangeDimension(2) ||
			!previous_candidate->IsValid() || !next_candidate->IsValid())
		    return false;
		const auto preserves_lift = [surface, tolerance](
			const ON_BrepTrim &original, const ON_Curve &candidate) {
		    const ON_Interval domain = original.Domain();
		    const int samples = std::min(4096, std::max(64,
			std::max(original.SpanCount(), candidate.SpanCount()) * 8));
		    for (int sample = 0; sample <= samples; ++sample) {
			if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled())
			    return false;
			const double parameter = domain.ParameterAt(
			    static_cast<double>(sample) / samples);
			const ON_3dPoint original_uv = original.PointAt(parameter);
			const ON_3dPoint candidate_uv = candidate.PointAt(parameter);
			const ON_3dPoint original_lift = surface->PointAt(
			    original_uv.x, original_uv.y);
			const ON_3dPoint candidate_lift = surface->PointAt(
			    candidate_uv.x, candidate_uv.y);
			if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
				original_lift.DistanceTo(candidate_lift) > tolerance)
			    return false;
		    }
		    return true;
		};
		if (!preserves_lift(previous, *previous_candidate) ||
			!preserves_lift(next, *next_candidate))
		    return false;
		const int previous_c2 = brep->AddTrimCurve(
		    previous_candidate.release());
		const int next_c2 = brep->AddTrimCurve(next_candidate.release());
		if (previous_c2 < 0 || next_c2 < 0 ||
			!brep->SetTrimCurve(previous, previous_c2) ||
			!brep->SetTrimCurve(next, next_c2))
		    return false;
		brep->SetTrimIsoFlags(previous);
		brep->SetTrimIsoFlags(next);
		++snapped_band_joins;
	    }
	    return true;
	};
	if (!snap_boundary_joins(lower) || !snap_boundary_joins(upper)) {
	    report_rejection("a proven boundary join could not be closed within its measured topology tolerance");
	    *brep = *rollback;
	    continue;
	}
	const auto orient_boundary = [brep, closed_direction, closed_min,
		closed_max, parameter_tolerance](std::vector<int> &trims,
		bool increasing) {
	    if (trims.empty()) return false;
	    ON_3dPoint start = brep->m_T[trims.front()].PointAtStart();
	    ON_3dPoint end = brep->m_T[trims.back()].PointAtEnd();
	    const bool already_oriented = increasing ?
		(fabs(start[closed_direction] - closed_min) <= parameter_tolerance &&
		 fabs(end[closed_direction] - closed_max) <= parameter_tolerance) :
		(fabs(start[closed_direction] - closed_max) <= parameter_tolerance &&
		 fabs(end[closed_direction] - closed_min) <= parameter_tolerance);
	    if (already_oriented)
		return true;
	    const bool reversed_orientation = increasing ?
		(fabs(start[closed_direction] - closed_max) <= parameter_tolerance &&
		 fabs(end[closed_direction] - closed_min) <= parameter_tolerance) :
		(fabs(start[closed_direction] - closed_min) <= parameter_tolerance &&
		 fabs(end[closed_direction] - closed_max) <= parameter_tolerance);
	    if (!reversed_orientation)
		return false;
	    std::reverse(trims.begin(), trims.end());
	    for (std::vector<int>::iterator trim = trims.begin();
		    trim != trims.end(); ++trim)
		if (*trim < 0 || *trim >= brep->m_T.Count() ||
			!brep->m_T[*trim].Reverse())
		    return false;
	    return true;
	};
	if (!orient_boundary(lower.trim_indices, true) ||
		!orient_boundary(upper.trim_indices, false)) {
	    report_rejection("the supplied boundary directions were inconsistent");
	    *brep = *rollback;
	    continue;
	}

	const ON_3dPoint lower_start =
	    brep->m_T[lower.trim_indices.front()].PointAtStart();
	const ON_3dPoint lower_end =
	    brep->m_T[lower.trim_indices.back()].PointAtEnd();
	const ON_3dPoint upper_start =
	    brep->m_T[upper.trim_indices.front()].PointAtStart();
	const ON_3dPoint upper_end =
	    brep->m_T[upper.trim_indices.back()].PointAtEnd();
	if (lower_start.DistanceTo(lower_uv_min) > parameter_tolerance ||
		lower_end.DistanceTo(lower_uv_max) > parameter_tolerance ||
		upper_start.DistanceTo(upper_uv_max) > parameter_tolerance ||
		upper_end.DistanceTo(upper_uv_min) > parameter_tolerance) {
	    report_rejection("the supplied boundaries were not on the native periodic sides");
	    *brep = *rollback;
	    continue;
	}
	/* Use the existing exact boundary endpoints so all four UV joins are
	 * identical to parameter-space numerical precision. */
	std::unique_ptr<ON_LineCurve> maximum_trim_curve(new ON_LineCurve(
	    lower_end, upper_start));
	std::unique_ptr<ON_LineCurve> minimum_trim_curve(new ON_LineCurve(
	    upper_end, lower_start));
	if (!maximum_trim_curve->ChangeDimension(2) ||
		!minimum_trim_curve->ChangeDimension(2) ||
		!maximum_trim_curve->IsValid() ||
		!minimum_trim_curve->IsValid()) {
	    *brep = *rollback;
	    continue;
	}

	const int seam_c3 = brep->AddEdgeCurve(seam_curve.release());
	const int maximum_c2 = brep->AddTrimCurve(maximum_trim_curve.release());
	const int minimum_c2 = brep->AddTrimCurve(minimum_trim_curve.release());
	if (seam_c3 < 0 || maximum_c2 < 0 || minimum_c2 < 0) {
	    *brep = *rollback;
	    continue;
	}
	ON_BrepEdge &seam_edge = brep->NewEdge(
	    brep->m_V[lower.vertex_index], brep->m_V[upper.vertex_index],
	    seam_c3, NULL, band_tolerance);
	seam_edge.m_tolerance = band_tolerance;
	const int seam_edge_index = seam_edge.m_edge_index;
	const int maximum_trim_index = brep->NewTrim(seam_edge, false,
	    brep->m_L[lower.loop_index], maximum_c2).m_trim_index;
	const int minimum_trim_index = brep->NewTrim(
	    brep->m_E[seam_edge_index], true, brep->m_L[lower.loop_index],
	    minimum_c2).m_trim_index;
	if (maximum_trim_index < 0 || minimum_trim_index < 0) {
	    *brep = *rollback;
	    continue;
	}

	ON_BrepLoop &combined_loop = brep->m_L[lower.loop_index];
	ON_BrepLoop &discarded_loop = brep->m_L[upper.loop_index];
	combined_loop.m_ti.SetCount(0);
	std::vector<int> ordered_trims(lower.trim_indices.begin(),
	    lower.trim_indices.end());
	ordered_trims.push_back(maximum_trim_index);
	ordered_trims.insert(ordered_trims.end(), upper.trim_indices.begin(),
	    upper.trim_indices.end());
	ordered_trims.push_back(minimum_trim_index);
	for (std::vector<int>::const_iterator ordered = ordered_trims.begin();
		ordered != ordered_trims.end(); ++ordered) {
	    combined_loop.m_ti.Append(*ordered);
	    brep->m_T[*ordered].m_li = lower.loop_index;
	    brep->m_T[*ordered].m_tolerance[0] =
		band_tolerance;
	    brep->m_T[*ordered].m_tolerance[1] =
		band_tolerance;
	}
	combined_loop.m_type = ON_BrepLoop::outer;
	brep->m_T[maximum_trim_index].m_type = ON_BrepTrim::seam;
	brep->m_T[minimum_trim_index].m_type = ON_BrepTrim::seam;
	brep->m_T[maximum_trim_index].m_iso = closed_direction == 0 ?
	    ON_Surface::E_iso : ON_Surface::N_iso;
	brep->m_T[minimum_trim_index].m_iso = closed_direction == 0 ?
	    ON_Surface::W_iso : ON_Surface::S_iso;
	discarded_loop.m_ti.SetCount(0);
	brep->DeleteLoop(discarded_loop, false);
	if (!brep->Compact()) {
	    *brep = *rollback;
	    continue;
	}
	std::string unsafe_topology;
	if (!brep_topology_references_are_safe(brep, &unsafe_topology)) {
	    *brep = *rollback;
	    continue;
	}
	++repaired;
	for (size_t snapped_join = 0; snapped_join < snapped_band_joins;
		snapped_join++)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"closed a proven periodic-band boundary join within its measured topology tolerance");
	wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	    "inserted an exact OpenNURBS seam for an implicit periodic STEP face band");
    }
    return repaired;
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
    if (wrapper)
	wrapper->SetProgressDetail("refreshing exact BREP topology flags",
	    entity_id, 0, 0, std::string(), entity_type);
    refresh_brep_flags_preserving_singular_isos(brep, false);
    if (brlcad::PullbackWorkCancelled())
	return 0;
    if (wrapper)
	wrapper->SetProgressDetail("classifying exact BREP seams", entity_id,
	    0, 0, std::string(), entity_type);
    classify_exact_polyline_seams(brep);
    if (brlcad::PullbackWorkCancelled())
	return 0;
    size_t keyhole_splits = 0;
    size_t keyhole_rejections = 0;
    const size_t keyhole_diagnostic_limit = 16;
    if (normalize_keyholes) {
	if (wrapper)
	    wrapper->SetProgressDetail("normalizing exact BREP keyhole loops",
		entity_id, 0, static_cast<uint64_t>(brep->m_L.Count()), "loops",
		entity_type);
	bool removed_slit = true;
	while (removed_slit) {
	    if (brlcad::PullbackWorkCancelled())
		return keyhole_splits;
	    removed_slit = false;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		if (brlcad::PullbackWorkCancelled())
		    return keyhole_splits;
		if (!remove_adjacent_zero_area_slit(brep, li))
		    continue;
		if (wrapper)
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"removed an exact zero-area adjacent seam bridge");
		if (topology_changed) *topology_changed = true;
		refresh_brep_flags_preserving_singular_isos(brep, false);
		classify_exact_polyline_seams(brep);
		removed_slit = true;
		break;
	    }
	}
	bool changed = true;
	while (changed) {
	    if (brlcad::PullbackWorkCancelled())
		return keyhole_splits;
	    changed = false;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		if (brlcad::PullbackWorkCancelled())
		    return keyhole_splits;
		std::string split_failure;
		if (split_keyhole_loop(brep, li, &split_failure)) {
		    ++keyhole_splits;
		    if (topology_changed) *topology_changed = true;
		    refresh_brep_flags_preserving_singular_isos(brep, false);
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
    if (wrapper)
	wrapper->SetProgressDetail("sorting exact BREP face loops", entity_id,
	    0, static_cast<uint64_t>(brep->m_F.Count()), "faces", entity_type);
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	if (brlcad::PullbackWorkCancelled())
	    return keyhole_splits;
	brep->SortFaceLoops(brep->m_F[fi]);
    }
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
closed_trim_endpoint_alignment_is_valid(double alignment)
{
    /* ON_UNSET_VALUE is deliberately a large negative sentinel.  Comparing
     * it directly with zero mistakes an unevaluable tangent (most commonly
     * at a collapsed surface pole) for strong evidence of reversed geometry
     * and can launch an unnecessary dense pcurve regeneration. */
    return ON_IsValid(alignment) && alignment > ON_UNSET_VALUE;
}


static bool
refine_surface_pullback_seeded(const ON_Surface *surface, const ON_3dPoint &target,
    double tolerance, ON_3dPoint &uv, double *final_distance,
    bool confine_to_domain = false, double acceptance_tolerance = 0.0)
{
    if (final_distance)
	*final_distance = DBL_MAX;
    if (!surface || !target.IsValid() || !uv.IsValid() || !(tolerance > 0.0))
	return false;
    if (!(acceptance_tolerance > 0.0))
	acceptance_tolerance = tolerance;

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
    return best_distance <= acceptance_tolerance;
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
	bool prefer_edge_driven = false, STEPWrapper *wrapper = NULL,
	bool preserve_required_uv_images = false,
	ON_Curve **generated_curve = NULL)
{
    if (failure_reason)
	failure_reason->clear();
    if (periodic_crossing)
	*periodic_crossing = PeriodicPullbackCrossing();
    if (generated_curve)
	*generated_curve = NULL;
    if (!brep || !surface || !(tolerance > 0.0))
	return false;

    const ON_BrepEdge *edge = trim.Edge();
    if (!edge)
	return false;
    const ON_Interval trim_domain = trim.Domain();
    brlcad::PullbackContext pullback_context;
    const ON_BoundingBox edge_bounds = edge_nurbs.BoundingBox();
    const double edge_feature_scale = edge_bounds.IsValid() ?
	edge_bounds.Diagonal().Length() : 0.0;
    const auto pullback_solver_tolerance = [edge_feature_scale](
	    double acceptance_tolerance) {
	double solver_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    acceptance_tolerance * 0.1);
	if (edge_feature_scale > ON_ZERO_TOLERANCE &&
		acceptance_tolerance > edge_feature_scale *
		    kPullbackLooseToleranceFeatureFraction)
	    solver_tolerance = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		std::min(solver_tolerance, edge_feature_scale *
		    kPullbackSolverFeatureFraction));
	return solver_tolerance;
    };
    std::string edge_driven_failure;
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
	    if (brlcad::PullbackWorkCancelled()) {
		if (failure_reason)
		    *failure_reason = "regenerated-pcurve sampling was cancelled";
		return false;
	    }
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
		const double solver_tolerance = pullback_solver_tolerance(
		    pullback_tolerance);
		double pullback_distance = DBL_MAX;
		/* An exact surface point can have several parameter-space preimages at
		 * a pole or collapsed boundary.  Continue from the preceding exact
		 * sample before consulting the independently parameterized source
		 * pcurve, so adjacent samples remain on one lift-equivalent branch. */
		const bool have_continuation_seed = points.Count() > 0;
		if (have_continuation_seed)
		    uv = points[points.Count() - 1];
		bool pulled = refine_surface_pullback_seeded(surface, edge_point,
		    solver_tolerance, uv, &pullback_distance,
		    !has_unwrapped_closed_parameter(surface, uv),
		    pullback_tolerance);
		if (!pulled && have_continuation_seed) {
		    uv = source_uv;
		    pulled = refine_surface_pullback_seeded(surface, edge_point,
			solver_tolerance, uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, uv),
			pullback_tolerance);
		}
		if (!pulled) {
		    ON_2dPoint pulled_uv;
		    ON_3dPoint pulled_lift;
		    if (!pullback_context.SurfaceClosestPoint(surface, edge_point, pulled_uv,
			    pulled_lift, pullback_distance, 0,
			    solver_tolerance,
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
		const double solver_tolerance = pullback_solver_tolerance(
		    pullback_tolerance);
		ON_3dPoint refined_uv = uv;
		double pullback_distance = DBL_MAX;
		bool pulled = required_endpoint && source_endpoint_valid;
		if (pulled)
		    pullback_distance = endpoint_lift.DistanceTo(edge_point);
		else
		    pulled = refine_surface_pullback_seeded(surface, edge_point,
			solver_tolerance, refined_uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, refined_uv),
			pullback_tolerance);
		if (!pulled) {
		    ON_2dPoint pulled_uv;
		    ON_3dPoint pulled_lift;
		    pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
			pulled_uv, pulled_lift, pullback_distance, 0,
			solver_tolerance,
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
	    const bool preserve_required_image = preserve_required_uv_images &&
		((sample == 0 && required_start) ||
		 (sample == segment_count && required_end));
	    if (edge_driven && points.Count() > 0 && !preserve_required_image) {
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
		    /* A required loop endpoint is authoritative as a 3-D locus, not as
		     * a particular periodic image.  Prefer its lift-equivalent image
		     * adjacent to the regenerated interior; the subsequent bounded
		     * loop pass translates the neighboring complete pcurve to the same
		     * image and revalidates it. */
		    if (shifted_lift.IsValid() && shifted_lift.DistanceTo(edge_point) <=
			    tolerance)
			uv = shifted_uv;
		}
	    }
	    if (edge_driven && !required_endpoint) {
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
		const int endpoint_index = end == 0 ? 0 : points.Count() - 1;
		const int adjacent_index = end == 0 ? 1 : points.Count() - 2;
		const bool required_endpoint = end == 0 ? required_start != NULL :
		    required_end != NULL;
		if (required_endpoint && preserve_required_uv_images)
		    continue;
		if (required_endpoint && !surface->IsClosed(0) &&
			!surface->IsClosed(1))
		    continue;
		const double normalized = end == 0 ? 0.0 : 1.0;
		const ON_3dPoint edge_point = edge->PointAt(
		    edge->Domain().ParameterAt(trim.m_bRev3d ?
			1.0 - normalized : normalized));
		const double pullback_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		const double solver_tolerance = pullback_solver_tolerance(
		    pullback_tolerance);
		ON_3dPoint continuous_uv = points[adjacent_index];
		double pullback_distance = DBL_MAX;
		if (!refine_surface_pullback_seeded(surface, edge_point,
			solver_tolerance, continuous_uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, continuous_uv),
			pullback_tolerance))
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
		/* A required endpoint fixes the exact 3-D loop join, not a particular
		 * image of that point on a periodic parameter domain.  Adopt a newly
		 * refined image only when it is lift-equivalent and strictly nearer the
		 * exact interior branch.  This prevents a full-period final chord while
		 * leaving nonperiodic endpoint constraints untouched. */
		if (continuous_lift.IsValid() &&
			continuous_lift.DistanceTo(edge_point) <= pullback_tolerance &&
			(!required_endpoint || continuous_uv.DistanceTo(
			    points[adjacent_index]) < points[endpoint_index].DistanceTo(
			    points[adjacent_index])))
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
	    /* Inserting a midpoint changes only the interval being split.  Advance
	     * monotonically after an interval passes; after a split, retain the
	     * current index to prove its left child and then its right child.  A
	     * restart from segment zero (even with an ordered cache of validated
	     * intervals) makes a 4096-point refinement quadratic in both scans and
	     * tree lookups.  No acceptance test changes: every resulting interval
	     * still passes the same three quarter-point lift checks and the complete
	     * candidate still receives the dense validation below. */
	    int segment = 0;
	    while (valid && segment + 1 < points.Count()) {
		if (brlcad::PullbackWorkCancelled()) {
		    if (failure_reason)
			*failure_reason =
			    "adaptive exact-edge pullback was cancelled";
		    return false;
		}
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
		    if (!split_segment) {
			++segment;
			continue;
		    }
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
					pullback_solver_tolerance(tolerance), refined_shift,
					&refined_distance, false, tolerance) &&
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
			/* Recheck this interval after selecting the nearer exact
			 * periodic image.  Its following interval has not yet been
			 * visited, so no accepted prefix is invalidated. */
			continue;
		    }
		    const double parameter_midpoint = 0.5 *
			(normalized_parameters[segment] +
			 normalized_parameters[segment + 1]);
		    if (!(parameter_midpoint > normalized_parameters[segment] &&
			    parameter_midpoint < normalized_parameters[segment + 1])) {
			/* A required loop endpoint can name a different periodic UV image
			 * of the same exact 3-D vertex.  Adaptive edge pullback approaches
			 * the continuous image from the interior, but eventually no double
			 * remains between its normalized parameter and 0 or 1.  At that
			 * machine-precision limit, retain the interior-side image only when
			 * its lift independently matches both the exact edge endpoint and
			 * topology vertex under a tighter tolerance.  The complete candidate
			 * is still densely validated below, and the adjacent-loop repair moves
			 * the neighboring endpoint to the same lift-equivalent image. */
			const bool closed_surface = surface->IsClosed(0) ||
			    surface->IsClosed(1);
			const bool collapsed_required_start = closed_surface &&
			    required_start && segment == 0;
			const bool collapsed_required_end = closed_surface &&
			    required_end && segment + 1 == points.Count() - 1;
			if (collapsed_required_start || collapsed_required_end) {
			    const int end = collapsed_required_start ? 0 : 1;
			    const int limit_index = collapsed_required_start ?
				segment + 1 : segment;
			    const ON_3dPoint limit_lift = surface->PointAt(
				points[limit_index].x, points[limit_index].y);
			    const ON_3dPoint edge_endpoint = edge->PointAt(
				edge->Domain()[trim.m_bRev3d ? 1 - end : end]);
			    const int vertex_index = trim.m_vi[end];
			    const double endpoint_tolerance = std::max(
				ON_ZERO_TOLERANCE * kNumericalToleranceScale,
				tolerance * 0.1);
			    if (limit_lift.IsValid() && edge_endpoint.IsValid() &&
				vertex_index >= 0 && vertex_index < brep->m_V.Count() &&
				limit_lift.DistanceTo(edge_endpoint) <= endpoint_tolerance &&
				limit_lift.DistanceTo(brep->m_V[vertex_index].point) <=
				    tolerance) {
				if (collapsed_required_start) {
				    points.Remove(0);
				    normalized_parameters.erase(
					normalized_parameters.begin());
				    normalized_parameters.front() = 0.0;
				    segment = 0;
				} else {
				    points.Remove(points.Count() - 1);
				    normalized_parameters.pop_back();
				    normalized_parameters.back() = 1.0;
				    segment = std::max(0, segment - 1);
				}
				continue;
			    }
			}
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
			    if (brlcad::PullbackWorkCancelled()) {
				if (failure_reason)
				    *failure_reason =
					"singular connector search was cancelled";
				return false;
			    }
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
				if ((connector_sample & 63) == 0 &&
					brlcad::PullbackWorkCancelled()) {
				    if (failure_reason)
					*failure_reason =
					    "singular connector validation was cancelled";
				    return false;
				}
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
			    /* The complete connector was densely proven above. */
			    ++segment;
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
			const std::string periodic_shift_detail =
			    rejected_periodic_shift_distance < DBL_MAX ?
			    std::to_string(rejected_periodic_shift_distance) :
			    std::string("unavailable");
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
			    ", periodic shift distance " + periodic_shift_detail + ")";
			valid = false;
			break;
		    }
		    if (static_cast<size_t>(points.Count()) >= maximum_points) {
			const std::string periodic_shift_detail =
			    rejected_periodic_shift_distance < DBL_MAX ?
			    std::to_string(rejected_periodic_shift_distance) :
			    std::string("unavailable");
			rejection = "adaptive exact-edge pullback sample budget exceeded at " +
			    std::to_string(normalized_parameters[segment]) + ":" +
			    std::to_string(normalized_parameters[segment + 1]) +
			    " (uv " + std::to_string(points[segment].x) + ":" +
			    std::to_string(points[segment].y) + " -> " +
			    std::to_string(points[segment + 1].x) + ":" +
			    std::to_string(points[segment + 1].y) +
			    ", periodic shift distance " + periodic_shift_detail + ")";
			valid = false;
			break;
		    }

		    const double normalized = parameter_midpoint;
		    const double edge_parameter = edge->Domain().ParameterAt(
			trim.m_bRev3d ? 1.0 - normalized : normalized);
		    const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		    const double pullback_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		    const double solver_tolerance = pullback_solver_tolerance(
			pullback_tolerance);
		    const ON_3dPoint branch_reference = 0.5 *
			(points[segment] + points[segment + 1]);
		    ON_3dPoint uv = branch_reference;
		    double pullback_distance = DBL_MAX;
		    if (!refine_surface_pullback_seeded(surface, edge_point,
			    solver_tolerance, uv, &pullback_distance,
			    !has_unwrapped_closed_parameter(surface, uv),
			    pullback_tolerance)) {
			ON_2dPoint pulled_uv;
			ON_3dPoint pulled_lift;
			if (!pullback_context.SurfaceClosestPoint(surface, edge_point, pulled_uv,
				pulled_lift, pullback_distance, 0,
				solver_tolerance,
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
		    /* Prove the newly created left child before advancing. */
	    }
	    if (!valid) {
		if (failure_reason)
		    *failure_reason = rejection;
		continue;
	    }
	}
	/* A fitted closed pcurve, or an open pcurve whose endpoint was constrained
	 * to close a periodic loop, can have a poor one-sided derivative at that
	 * algebraic endpoint even when its interior follows the edge correctly.
	 * Map the exact 3D edge tangent into the surface parameter plane and add a
	 * short endpoint chord without moving an already validated interior sample.
	 * The validation below still
	 * bounds both the displacement from the source pcurve and the distance to
	 * the exact edge, so this cannot bridge an out-of-tolerance gap. */
	const bool closed_topology = trim.m_vi[0] == trim.m_vi[1] &&
	    edge->m_vi[0] == edge->m_vi[1];
	bool ill_conditioned_endpoint[2] = {false, false};
	for (int end = 0; valid && end < 2; ++end) {
	    if (!closed_topology && !(end == 0 ? required_start : required_end))
		continue;
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
		/* Some analytic revolution surfaces do not report their apex via
		 * IsAtSingularity(), but the rank-deficient Jacobian proves that an
		 * endpoint parameter tangent is not geometrically meaningful there.
		 * Preserve the exact endpoint and rely on the dense interior direction
		 * proof outside the guarded endpoint neighborhood. */
		ill_conditioned_endpoint[end] = true;
		continue;
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
		/* Periodic endpoint limiting can consume several successively refined
		 * samples whose normalized parameters differ but whose UV values have
		 * reached the same machine-precision limit.  Endpoint-tangent insertion
		 * may expose that duplicate pair.  Remove only zero-length consecutive
		 * samples, retaining a constrained algebraic endpoint when it is one of
		 * the pair; all remaining segments undergo the full validation below. */
		for (int point_index = 1; point_index < points.Count();) {
		    if (points[point_index - 1].DistanceTo(points[point_index]) >
			    ON_ZERO_TOLERANCE) {
			++point_index;
			continue;
		    }
		    const int remove_index = point_index == points.Count() - 1 ?
			point_index - 1 : point_index;
		    points.Remove(remove_index);
		    normalized_parameters.erase(normalized_parameters.begin() +
			remove_index);
		    point_index = std::max(1, remove_index);
		}
		if (points.Count() < 2) {
		    rejection = "periodic endpoint limiting collapsed the pcurve";
		    valid = false;
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
	 * source deviation can hide exactly at a sampling vertex.  Surface and
	 * curve evaluation dominates this proof and each sample is independent, so
	 * let an otherwise idle geometry helper evaluate samples in parallel.  All
	 * acceptance decisions remain in deterministic parameter order below. */
	struct DenseValidationSample {
	    double normalized = 0.0;
	    ON_3dPoint candidate_uv = ON_3dPoint::UnsetPoint;
	    ON_3dPoint candidate_lift = ON_3dPoint::UnsetPoint;
	    ON_3dPoint original_lift = ON_3dPoint::UnsetPoint;
	    ON_3dPoint edge_point = ON_3dPoint::UnsetPoint;
	    ON_3dVector du = ON_3dVector::UnsetVector;
	    ON_3dVector dv = ON_3dVector::UnsetVector;
	    bool evaluated = false;
	};
	const size_t dense_sample_count =
	    static_cast<size_t>(candidate_segment_count) * 4;
	std::vector<DenseValidationSample> dense_samples(dense_sample_count);
	const auto evaluate_dense_sample = [&](size_t dense_index) {
	    if (brlcad::PullbackWorkCancelled())
		return;
	    const int segment = static_cast<int>(dense_index / 4);
	    const int sub = static_cast<int>(dense_index % 4);
	    const double fraction = static_cast<double>(sub) / 4.0;
	    DenseValidationSample &sample = dense_samples[dense_index];
	    sample.normalized = (1.0 - fraction) *
		normalized_parameters[segment] + fraction *
		normalized_parameters[segment + 1];
	    sample.candidate_uv = (1.0 - fraction) * points[segment] +
		fraction * points[segment + 1];
	    if (!surface->Ev1Der(sample.candidate_uv.x, sample.candidate_uv.y,
		    sample.candidate_lift, sample.du, sample.dv))
		return;
	    const ON_3dPoint original_uv = trim.PointAt(
		trim_domain.ParameterAt(sample.normalized));
	    sample.original_lift = surface->PointAt(original_uv.x, original_uv.y);
	    sample.edge_point = edge->PointAt(edge->Domain().ParameterAt(
		trim.m_bRev3d ? 1.0 - sample.normalized : sample.normalized));
	    sample.evaluated = sample.candidate_lift.IsValid() &&
		sample.original_lift.IsValid() && sample.edge_point.IsValid();
	};
	if (wrapper)
	    wrapper->ParallelForGeometry(dense_sample_count, evaluate_dense_sample);
	else
	    for (size_t dense_index = 0; dense_index < dense_sample_count;
		    ++dense_index)
		evaluate_dense_sample(dense_index);
	if (brlcad::PullbackWorkCancelled()) {
	    if (failure_reason)
		*failure_reason = "dense regenerated-pcurve validation was cancelled";
	    return false;
	}
	for (int segment = 0; valid && segment < candidate_segment_count; ++segment) {
	    const ON_3dVector uv_tangent = points[segment + 1] - points[segment];
	    if (uv_tangent.Length() <= ON_ZERO_TOLERANCE) {
		rejection = "zero-length regenerated pcurve segment " +
		    std::to_string(segment) + " (normalized " +
		    std::to_string(normalized_parameters[segment]) + ":" +
		    std::to_string(normalized_parameters[segment + 1]) + ", uv " +
		    std::to_string(points[segment].x) + ":" +
		    std::to_string(points[segment].y) + " -> " +
		    std::to_string(points[segment + 1].x) + ":" +
		    std::to_string(points[segment + 1].y) + ")";
		valid = false;
		break;
	    }
	    for (int sub = 0; sub <= 3; ++sub) {
		const DenseValidationSample &sample = dense_samples[
		    static_cast<size_t>(segment) * 4 + sub];
		const double normalized = sample.normalized;
		const ON_3dPoint &candidate_uv = sample.candidate_uv;
		const ON_3dPoint &candidate_lift = sample.candidate_lift;
		const ON_3dPoint &original_lift = sample.original_lift;
		const ON_3dVector &du = sample.du;
		const ON_3dVector &dv = sample.dv;
		if (!sample.evaluated) {
		    rejection = "surface lift failed";
		    valid = false;
		    break;
		}
		const double source_lift_distance =
		    candidate_lift.DistanceTo(original_lift);
		if (!edge_driven && source_lift_distance > tolerance) {
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
		edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		edge_distance = candidate_lift.DistanceTo(sample.edge_point);
		bool edge_proxy_parameter = true;
		/* A trim and its 3-D edge describe the same directed locus, but are not
		 * required to advance at the same normalized parameter speed.  The
		 * regenerated polyline changes that speed locally, so a synchronized
		 * comparison can report a false geometric gap.  A synchronized point
		 * already inside the acceptance tolerance is also a constructive proof
		 * of edge-locus membership, however, and the global NURBS optimizer
		 * cannot change that decision.  Run the expensive closest-point solve
		 * only when the direct proof fails; the tangent test below still proves
		 * traversal direction. */
		if (edge_distance > tolerance) {
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
		if (normalized <= kEndpointDirectionGuardFraction ||
			normalized >= 1.0 - kEndpointDirectionGuardFraction)
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
	    /* At a collapsed surface boundary the endpoint lift is exact, but its
	     * differential is not unique.  Interior samples above already prove
	     * traversal direction outside the guarded pole neighborhood. */
	    if (surface->IsAtSingularity(uv.x, uv.y, false) ||
		    ill_conditioned_endpoint[end])
		continue;
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
		rejection = "regenerated endpoint direction disagreed with edge "
		    "(endpoint " + std::to_string(end) + ", alignment " +
		    std::to_string(alignment) + ", uv " + std::to_string(uv.x) +
		    ":" + std::to_string(uv.y) + ", singular " +
		    std::to_string(surface->IsAtSingularity(uv.x, uv.y, false) ?
			1 : 0) + ")";
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
	if (generated_curve) {
	    *generated_curve = candidate;
	    return true;
	}
	const int c2_index = brep->AddTrimCurve(candidate);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index))
	    return false;
	const ON_Interval proxy_domain = trim.ProxyCurveDomain();
	trim.m_iso = surface->IsIsoparametric(*candidate, &proxy_domain);
	return true;
    }
    if (prefer_edge_driven && edge_driven && failure_reason)
	edge_driven_failure = *failure_reason;
    }
    if (prefer_edge_driven && failure_reason && !edge_driven_failure.empty() &&
	    *failure_reason != edge_driven_failure)
	*failure_reason = "exact-edge mode: " + edge_driven_failure +
	    "; supplied-pcurve mode: " + *failure_reason;
    return false;
}


static bool
regenerate_full_period_boundary_chain(ON_Brep *brep, int loop_index,
	int closed_direction, double parameter_tolerance,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || loop_index < 0 ||
	    loop_index >= brep->m_L.Count())
	return false;
    const ON_BrepLoop &source_loop = brep->m_L[loop_index];
    const ON_BrepFace *source_face = source_loop.Face();
    const ON_Surface *source_surface = source_face ?
	source_face->SurfaceOf() : NULL;
    if (!source_surface || !source_surface->IsClosed(closed_direction) ||
	    source_loop.TrimCount() < 2)
	return false;
    const int open_direction = 1 - closed_direction;
    const ON_Interval closed_domain = source_surface->Domain(closed_direction);
    if (!closed_domain.IsIncreasing())
	return false;
    const double period = closed_domain.Length();

    for (int cut = 0; cut < source_loop.TrimCount(); ++cut) {
	const ON_BrepTrim *source_before = source_loop.Trim(cut);
	const ON_BrepTrim *source_after = source_loop.Trim(
	    (cut + 1) % source_loop.TrimCount());
	if (!source_before || !source_after || source_before->m_vi[1] < 0 ||
		source_before->m_vi[1] != source_after->m_vi[0] ||
		source_before->m_vi[1] >= brep->m_V.Count())
	    continue;
	const ON_3dPoint source_end = source_before->PointAtEnd();
	const ON_3dPoint source_start = source_after->PointAtStart();
	const double adjacent_start = source_start[closed_direction] + round(
	    (source_end[closed_direction] - source_start[closed_direction]) /
	    period) * period;
	if (!source_end.IsValid() || !source_start.IsValid() ||
		fabs(source_end[open_direction] - source_start[open_direction]) >
		    parameter_tolerance ||
		fabs(source_end[closed_direction] - adjacent_start) >
		    parameter_tolerance)
	    continue;
	const double native_image = closed_domain.Min() + round(
	    (source_start[closed_direction] - closed_domain.Min()) / period) *
	    period;
	if (fabs(source_start[closed_direction] - native_image) >
		parameter_tolerance)
	    continue;

	double cut_tolerance = LocalUnits::tolerance;
	if (source_before->Edge())
	    cut_tolerance = std::max(cut_tolerance,
		source_before->Edge()->m_tolerance);
	if (source_after->Edge())
	    cut_tolerance = std::max(cut_tolerance,
		source_after->Edge()->m_tolerance);
	cut_tolerance = std::max(cut_tolerance,
	    brep->m_V[source_before->m_vi[1]].m_tolerance);
	const ON_3dPoint &cut_vertex =
	    brep->m_V[source_before->m_vi[1]].point;
	const ON_3dPoint source_end_lift = source_surface->PointAt(
	    source_end.x, source_end.y);
	const ON_3dPoint source_start_lift = source_surface->PointAt(
	    source_start.x, source_start.y);
	if (!source_end_lift.IsValid() || !source_start_lift.IsValid() ||
		source_end_lift.DistanceTo(cut_vertex) > cut_tolerance ||
		source_start_lift.DistanceTo(cut_vertex) > cut_tolerance)
	    continue;

	for (int winding = 0; winding < 2; ++winding) {
	    ON_BrepLoop &loop = brep->m_L[loop_index];
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface)
		continue;
	    std::vector<std::unique_ptr<ON_Curve> > regenerated_curves(
		loop.TrimCount());
	    std::vector<ON_BrepTrim *> regenerated_trims;
	    regenerated_trims.reserve(loop.TrimCount());
	    ON_3dPoint required_start = source_start;
	    required_start[closed_direction] = winding == 0 ?
		closed_domain.Min() : closed_domain.Max();
	    ON_3dPoint required_end = required_start;
	    required_end[closed_direction] = winding == 0 ?
		closed_domain.Max() : closed_domain.Min();
	    const ON_3dPoint start_lift = surface->PointAt(required_start.x,
		required_start.y);
	    const ON_3dPoint end_lift = surface->PointAt(required_end.x,
		required_end.y);
	    if (!start_lift.IsValid() || !end_lift.IsValid() ||
		    start_lift.DistanceTo(cut_vertex) > cut_tolerance ||
		    end_lift.DistanceTo(cut_vertex) > cut_tolerance)
		continue;

	    bool regenerated = true;
	    std::string failure;
	    for (int offset = 0; regenerated && offset < loop.TrimCount();
		    ++offset) {
		ON_BrepTrim *trim = loop.Trim(
		    (cut + 1 + offset) % loop.TrimCount());
		ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		ON_NurbsCurve edge_nurbs;
		double tolerance = LocalUnits::tolerance;
		if (edge)
		    tolerance = std::max(tolerance, edge->m_tolerance);
		if (trim)
		    tolerance = std::max(tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
		if (trim && trim->m_vi[0] >= 0 &&
			trim->m_vi[0] < brep->m_V.Count())
		    tolerance = std::max(tolerance,
			brep->m_V[trim->m_vi[0]].m_tolerance);
		if (trim && trim->m_vi[1] >= 0 &&
			trim->m_vi[1] < brep->m_V.Count())
		    tolerance = std::max(tolerance,
			brep->m_V[trim->m_vi[1]].m_tolerance);
		const ON_3dPoint *last_endpoint =
		    offset + 1 == loop.TrimCount() ? &required_end : NULL;
		ON_Curve *regenerated_curve = NULL;
		regenerated = trim && edge && edge->GetNurbForm(edge_nurbs) &&
		    regenerate_trim_polyline(brep, *trim, surface,
			edge_nurbs, tolerance, &failure, NULL, &required_start,
			last_endpoint, true, wrapper, true, &regenerated_curve);
		if (regenerated) {
		    regenerated_curves[offset].reset(regenerated_curve);
		    regenerated_trims.push_back(trim);
		    required_start = regenerated_curve->PointAtEnd();
		}
	    }
	    if (!regenerated) {
		if (wrapper->Verbose() && !failure.empty())
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary regeneration L" << loop_index
			<< " cut/winding " << cut << '/' << winding
			<< " rejected: " << failure << std::endl;
		continue;
	    }

	    bool exact_chain = true;
	    for (int offset = 0; exact_chain && offset < loop.TrimCount() - 1;
		    ++offset) {
		const ON_BrepTrim *previous = regenerated_trims[offset];
		const ON_BrepTrim *next = regenerated_trims[offset + 1];
		exact_chain = previous && next && regenerated_curves[offset] &&
		    regenerated_curves[offset + 1] &&
		    previous->m_vi[1] == next->m_vi[0] &&
		    regenerated_curves[offset]->PointAtEnd().DistanceTo(
			regenerated_curves[offset + 1]->PointAtStart()) <=
			parameter_tolerance;
	    }
	    const ON_Curve *first_curve = regenerated_curves.empty() ? NULL :
		regenerated_curves.front().get();
	    const ON_Curve *last_curve = regenerated_curves.empty() ? NULL :
		regenerated_curves.back().get();
	    if (!exact_chain || !first_curve || !last_curve ||
		    fabs(first_curve->PointAtStart()[closed_direction] -
			(winding == 0 ? closed_domain.Min() :
			 closed_domain.Max())) > parameter_tolerance ||
		    fabs(last_curve->PointAtEnd()[closed_direction] -
			(winding == 0 ? closed_domain.Max() :
			 closed_domain.Min())) > parameter_tolerance ||
		    fabs(first_curve->PointAtStart()[open_direction] -
			last_curve->PointAtEnd()[open_direction]) > parameter_tolerance)
		continue;

	    /* Generate and prove every replacement before modifying the BREP.  The
	     * former transaction copied the complete solid for each candidate loop;
	     * a production CRM solid with roughly eleven thousand faces performed
	     * this copy more than a thousand times.  Curve ownership is committed
	     * only after every member of this one loop has validated. */
	    struct OriginalTrimCurveState {
		int c2_index;
		ON_Interval proxy_domain;
		ON_Interval trim_domain;
		ON_Surface::ISO iso;
		ON_BoundingBox pbox;
	    };
	    std::vector<OriginalTrimCurveState> original_states;
	    original_states.reserve(regenerated_trims.size());
	    for (std::vector<ON_BrepTrim *>::const_iterator trim =
		    regenerated_trims.begin(); trim != regenerated_trims.end(); ++trim) {
		OriginalTrimCurveState state = {
		    (*trim)->m_c2i, (*trim)->ProxyCurveDomain(), (*trim)->Domain(),
		    (*trim)->m_iso, (*trim)->m_pbox
		};
		original_states.push_back(state);
	    }
	    const int original_c2_count = brep->m_C2.Count();
	    std::vector<int> new_c2_indices;
	    new_c2_indices.reserve(regenerated_curves.size());
	    bool curves_added = true;
	    for (std::vector<std::unique_ptr<ON_Curve> >::iterator curve =
		    regenerated_curves.begin(); curve != regenerated_curves.end();
		    ++curve) {
		const int c2_index = curve->get() ?
		    brep->AddTrimCurve(curve->get()) : -1;
		if (c2_index < 0) {
		    curves_added = false;
		    break;
		}
		curve->release();
		new_c2_indices.push_back(c2_index);
	    }
	    const auto discard_added_curves = [brep, original_c2_count]() {
		for (int c2_index = original_c2_count;
			c2_index < brep->m_C2.Count(); ++c2_index)
		    delete brep->m_C2[c2_index];
		brep->m_C2.SetCount(original_c2_count);
	    };
	    if (!curves_added) {
		discard_added_curves();
		continue;
	    }
	    bool curves_installed = true;
	    size_t installed_count = 0;
	    for (; installed_count < regenerated_trims.size(); ++installed_count) {
		if (!brep->SetTrimCurve(*regenerated_trims[installed_count],
			new_c2_indices[installed_count])) {
		    curves_installed = false;
		    break;
		}
		brep->SetTrimIsoFlags(*regenerated_trims[installed_count]);
	    }
	    if (!curves_installed) {
		for (size_t restored = 0; restored < regenerated_trims.size();
			++restored) {
		    ON_BrepTrim &trim = *regenerated_trims[restored];
		    const OriginalTrimCurveState &state = original_states[restored];
		    brep->SetTrimCurve(trim, state.c2_index);
		    trim.SetProxyCurveDomain(state.proxy_domain);
		    trim.SetDomain(state.trim_domain);
		    trim.m_iso = state.iso;
		    trim.m_pbox = state.pbox;
		}
		discard_added_curves();
		continue;
	    }

	    /* The regenerated chain begins immediately after the proven native
	     * seam cut and ends immediately before it.  Preserve that traversal in
	     * the loop's cyclic storage as well: downstream pole-cut construction
	     * appends its two seam uses and singular trim at the last-to-first gap.
	     * Leaving the original rotation in place puts the one-period gap inside
	     * the array and makes an otherwise exact loop structurally invalid. */
	    std::vector<int> ordered_trim_indices;
	    ordered_trim_indices.reserve(loop.TrimCount());
	    for (int offset = 0; offset < loop.TrimCount(); ++offset)
		ordered_trim_indices.push_back(loop.m_ti[
		    (cut + 1 + offset) % loop.TrimCount()]);
	    loop.m_ti.SetCount(0);
	    for (std::vector<int>::const_iterator trim_index =
		    ordered_trim_indices.begin();
		    trim_index != ordered_trim_indices.end(); ++trim_index)
		loop.m_ti.Append(*trim_index);

	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"unwrapped an exact full-period boundary from its 3-D STEP edge chain");
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


static bool
relocate_ordinary_closed_nurbs_loop_seam(ON_Brep *brep, int loop_index,
	int direction, double tolerance, std::string *failure_reason)
{
    if (failure_reason)
	failure_reason->clear();
    if (!brep || loop_index < 0 || loop_index >= brep->m_L.Count() ||
	(direction != 0 && direction != 1) || !(tolerance > 0.0))
	return false;
    ON_BrepLoop &source_loop = brep->m_L[loop_index];
    ON_BrepFace *source_face = source_loop.Face();
    ON_NurbsSurface *surface = source_face ? ON_NurbsSurface::Cast(
	const_cast<ON_Surface *>(source_face->SurfaceOf())) : NULL;
    if (!surface || !surface->IsClosed(direction)) {
	if (failure_reason)
	    *failure_reason = "the loop surface is not a closed NURBS surface";
	return false;
    }
    const int surface_index = source_face->m_si;
    const ON_Interval old_domain = surface->Domain(direction);
    const double period = old_domain.Length();
    if (!(period > ON_ZERO_TOLERANCE))
	return false;

    std::vector<double> phases;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.m_si != surface_index)
	    continue;
	for (int fli = 0; fli < face.LoopCount(); ++fli) {
	    const ON_BrepLoop *loop = face.Loop(fli);
	    if (!loop)
		continue;
	    for (int lti = 0; lti < loop->TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop->Trim(lti);
		if (!trim || !trim->TrimCurveOf())
		    return false;
		const int samples = std::min(256,
		    std::max(32, trim->SpanCount() * 4));
		const ON_Interval trim_domain = trim->Domain();
		for (int sample = 0; sample <= samples; ++sample) {
		    const ON_3dPoint uv = trim->PointAt(trim_domain.ParameterAt(
			static_cast<double>(sample) / samples));
		    if (!uv.IsValid())
			return false;
		    double phase = fmod(uv[direction] - old_domain.Min(), period);
		    if (phase < 0.0)
			phase += period;
		    phases.push_back(phase);
		}
	    }
	}
    }
    if (phases.size() < 2)
	return false;
    std::sort(phases.begin(), phases.end());
    double largest_gap = -1.0;
    double gap_start = 0.0;
    for (size_t index = 0; index < phases.size(); ++index) {
	const double next = index + 1 < phases.size() ? phases[index + 1] :
	    phases[0] + period;
	const double gap = next - phases[index];
	if (gap > largest_gap) {
	    largest_gap = gap;
	    gap_start = phases[index];
	}
    }
    if (largest_gap < kMinimumSeamRelocationGapFraction * period) {
	if (failure_reason)
	    *failure_reason = "the affected boundary has no empty seam interval";
	return false;
    }
    double seam_phase = fmod(gap_start + 0.5 * largest_gap, period);
    if (seam_phase < 0.0)
	seam_phase += period;
    const double seam = old_domain.Min() + seam_phase;
    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	period * 1.0e-10);
    if (seam <= old_domain.Min() + parameter_guard ||
	    seam >= old_domain.Max() - parameter_guard)
	return false;

    ON_NurbsSurface candidate(*surface);
    if (!candidate.ChangeSurfaceSeam(direction, seam) || !candidate.IsValid()) {
	if (failure_reason)
	    *failure_reason = "openNURBS rejected the boundary-free seam";
	return false;
    }
    const ON_Interval candidate_domain = candidate.Domain(direction);
    struct Replacement {
	int trim_index;
	ON_Curve *original;
	ON_Curve *seed;
	ON_Surface::ISO iso;
	ON_BrepTrim::TYPE type;
    };
    std::vector<Replacement> replacements;
    bool valid = true;
    for (int fi = 0; valid && fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.m_si != surface_index)
	    continue;
	for (int fli = 0; valid && fli < face.LoopCount(); ++fli) {
	    const ON_BrepLoop *loop = face.Loop(fli);
	    if (!loop)
		continue;
	    for (int lti = 0; valid && lti < loop->TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop->Trim(lti);
		ON_Curve *source = trim ? trim->DuplicateCurve() : NULL;
		ON_BoundingBox box;
		if (!trim || !trim->Edge() || !source || !source->GetBoundingBox(box)) {
		    delete source;
		    valid = false;
		    break;
		}
		const double center = 0.5 *
		    (box.m_min[direction] + box.m_max[direction]);
		const double base_shift = round((candidate_domain.Mid() - center) /
		    period) * period;
		ON_Curve *seed = NULL;
		const int candidate_images[3] = {0, -1, 1};
		for (int image_index = 0; !seed && image_index < 3; ++image_index) {
		    const int image = candidate_images[image_index];
		    ON_Curve *curve = source->DuplicateCurve();
		    ON_Xform shift(ON_Xform::IdentityTransformation);
		    shift.m_xform[direction][3] = base_shift + image * period;
		    const bool in_domain = curve && curve->Transform(shift) &&
			curve->ChangeDimension(2) && curve->IsValid();
		    if (in_domain)
			seed = curve;
		    else
			delete curve;
		}
		if (!seed) {
		    delete source;
		    valid = false;
		    if (failure_reason)
			*failure_reason = "a pcurve could not be seeded inside the relocated surface domain";
		    break;
		}
		replacements.push_back({trim->m_trim_index, source, seed,
		    trim->m_iso, trim->m_type});
	    }
	}
    }
    if (!valid) {
	for (std::vector<Replacement>::iterator replacement = replacements.begin();
		replacement != replacements.end(); ++replacement)
	{
	    delete replacement->original;
	    delete replacement->seed;
	}
	return false;
    }

    std::vector<int> curve_indices(replacements.size(), -1);
    for (size_t index = 0; index < replacements.size(); ++index) {
	curve_indices[index] = brep->AddTrimCurve(replacements[index].seed);
	if (curve_indices[index] < 0)
	    return false;
	replacements[index].seed = NULL;
    }
    const ON_NurbsSurface original_surface(*surface);
    *surface = candidate;
    for (size_t index = 0; index < replacements.size(); ++index) {
	if (!brep->SetTrimCurve(brep->m_T[replacements[index].trim_index],
		curve_indices[index]))
	    return false;
	brep->SetTrimIsoFlags(brep->m_T[replacements[index].trim_index]);
    }
    bool regenerated = true;
    std::string regeneration_failure;
    for (size_t index = 0; regenerated && index < replacements.size(); ++index) {
	ON_BrepTrim &trim = brep->m_T[replacements[index].trim_index];
	ON_BrepEdge *edge = trim.Edge();
	ON_NurbsCurve edge_nurbs;
	const double trim_tolerance = edge ? std::max(tolerance,
	    std::max(edge->m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1]))) : tolerance;
	regenerated = edge && edge->GetNurbForm(edge_nurbs) &&
	    regenerate_trim_polyline(brep, trim, surface, edge_nurbs,
		trim_tolerance, &regeneration_failure, NULL, NULL, NULL, true);
    }
    if (!regenerated) {
	/* Restore the pre-relocation surface and pcurves.  Newly added candidate
	 * curves remain unreferenced and are compacted with the BREP later. */
	*surface = original_surface;
	if (failure_reason)
	    *failure_reason = "exact-edge regeneration on the relocated surface failed: " +
		regeneration_failure;
	for (size_t index = 0; index < replacements.size(); ++index) {
	    const int original_index = brep->AddTrimCurve(replacements[index].original);
	    if (original_index >= 0) {
		replacements[index].original = NULL;
		brep->SetTrimCurve(brep->m_T[replacements[index].trim_index],
		    original_index);
		brep->m_T[replacements[index].trim_index].m_iso = replacements[index].iso;
		brep->m_T[replacements[index].trim_index].m_type = replacements[index].type;
	    }
	    delete replacements[index].original;
	}
	return false;
    }
    for (size_t index = 0; index < replacements.size(); ++index)
	delete replacements[index].original;
    return true;
}


static size_t
repair_ordinary_closed_nurbs_seam_crossings(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;
    size_t repaired = 0;
    std::set<int> repaired_surfaces;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	ON_BrepLoop &loop = brep->m_L[li];
	ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (!surface || repaired_surfaces.find(face->m_si) != repaired_surfaces.end())
	    continue;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *current = loop.Trim(lti);
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!current || !next || current->m_type == ON_BrepTrim::seam ||
		    next->m_type == ON_BrepTrim::seam ||
		    current->m_vi[1] < 0 || current->m_vi[1] != next->m_vi[0] ||
		    current->m_vi[1] >= brep->m_V.Count())
		continue;
	    const ON_3dPoint end = current->PointAtEnd();
	    const ON_3dPoint start = next->PointAtStart();
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const double period = surface->Domain(direction).Length();
		const double guard = std::max(ON_ZERO_TOLERANCE, period * 1.0e-8);
		if (!(period > ON_ZERO_TOLERANCE) ||
			fabs(fabs(end[direction] - start[direction]) - period) > guard)
		    continue;
		const ON_3dPoint end_lift = surface->PointAt(end.x, end.y);
		const ON_3dPoint start_lift = surface->PointAt(start.x, start.y);
		const ON_3dPoint &vertex = brep->m_V[current->m_vi[1]].point;
		if (end_lift.IsValid() && start_lift.IsValid() &&
			end_lift.DistanceTo(vertex) <= LocalUnits::tolerance &&
			start_lift.DistanceTo(vertex) <= LocalUnits::tolerance)
		    continue;
		std::string failure;
		if (relocate_ordinary_closed_nurbs_loop_seam(brep, li, direction,
			LocalUnits::tolerance, &failure)) {
		    repaired_surfaces.insert(face->m_si);
		    ++repaired;
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"relocated a closed NURBS surface seam outside an ordinary face boundary");
		} else if (wrapper->Verbose() && !failure.empty()) {
		    std::cerr << entity_type << " #" << entity_id << ": loop " << li
			<< " ordinary seam relocation rejected: " << failure << std::endl;
		}
		break;
	    }
	    if (repaired_surfaces.find(face->m_si) != repaired_surfaces.end())
		break;
	}
    }
    return repaired;
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

	const auto is_boundary_iso = [](ON_Surface::ISO iso) {
	    return iso == ON_Surface::W_iso || iso == ON_Surface::E_iso ||
		iso == ON_Surface::S_iso || iso == ON_Surface::N_iso;
	};
	/* Generic x_iso/y_iso is not sufficient for a topological seam.
	 * OpenNURBS requires each member to lie on an explicit domain boundary. */
	bool needs_repair = !is_boundary_iso(first.m_iso) ||
	    !is_boundary_iso(second.m_iso);
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
		brep->m_T[singular_index].m_tolerance[0] = LocalUnits::tolerance;
		brep->m_T[singular_index].m_tolerance[1] = LocalUnits::tolerance;
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
			brep->m_T[first_singular].m_tolerance[0] = LocalUnits::tolerance;
			brep->m_T[first_singular].m_tolerance[1] = LocalUnits::tolerance;
			brep->m_T[second_singular].m_tolerance[0] = LocalUnits::tolerance;
			brep->m_T[second_singular].m_tolerance[1] = LocalUnits::tolerance;
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


static double
verified_regeneration_tolerance(ON_BrepTrim &trim, ON_BrepEdge &edge,
	const ON_Surface *surface, const ON_NurbsCurve &edge_nurbs,
	double tolerance, const ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type);


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
	/* This pass is entered only after the supplied pcurve has proved closed
	 * for distinct topology vertices or failed an exact endpoint test.  Start
	 * from the authoritative 3-D edge; retrying the known-invalid pcurve at
	 * 64, 128, ... 1024 segments merely repeats the same failure.  The helper
	 * still falls back to that source-driven mode if exact edge projection
	 * cannot establish a valid branch. */
	double regeneration_tolerance = LocalUnits::tolerance;
	const bool measured_repair_allowed = !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe;
	if (measured_repair_allowed)
	    regeneration_tolerance = std::max(regeneration_tolerance,
		std::max(edge.m_tolerance,
		    std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
	bool regenerated = regenerate_trim_polyline(brep, trim, surface,
	    edge_nurbs, regeneration_tolerance, &failure, NULL,
	    required_endpoint[0], required_endpoint[1], true, wrapper);
	/* Surface-seam alignment and other bounded repairs can expose a narrow
	 * source edge/surface mismatch that was not sampled when the edge was
	 * first constructed.  Do not repeatedly reject it at the declared file
	 * uncertainty.  Measure the complete exact 3-D edge against this surface
	 * at the same deterministic fractions used by final repair validation,
	 * accept only a scale-bounded result, and retry at the measured tolerance.
	 * --exact deliberately bypasses this path. */
	if (!regenerated && measured_repair_allowed) {
	    const double verified_tolerance = verified_regeneration_tolerance(
		trim, edge, surface, edge_nurbs, regeneration_tolerance, brep,
		wrapper, entity_id, entity_type);
	    if (verified_tolerance > regeneration_tolerance) {
		regeneration_tolerance = verified_tolerance;
		regenerated = regenerate_trim_polyline(brep, trim, surface,
		    edge_nurbs, regeneration_tolerance, &failure, NULL,
		    required_endpoint[0], required_endpoint[1], true, wrapper);
	    }
	}
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
		if (edge_distance > regeneration_tolerance)
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
			regeneration_tolerance && lift.DistanceTo(edge_point) <=
			regeneration_tolerance;
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
		<< ti << "/STEP edge " << edge.m_edge_user.i
		<< " in loop " << trim.m_li
		<< " exact-edge regeneration rejected: regenerated="
		<< (regenerated ? "yes" : "no") << ", candidate="
		<< (candidate ? (candidate_closed ? "closed" : "open") : "none")
		<< ", iso=" << regenerated_iso
		<< ", tolerance=" << regeneration_tolerance
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
	    ON_BrepFace *face = loop.m_fi >= 0 && loop.m_fi < brep->m_F.Count() ?
		&brep->m_F[loop.m_fi] : NULL;
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
		if (singular_iso == ON_Surface::not_iso) {
		    /* Analytic surfaces may reject an isoparametric connector whose
		     * varying coordinate is an unwrapped periodic image far outside the
		     * native domain.  Constant-coordinate structure is independent of
		     * that branch choice and will still be densely lift-validated below. */
		    const double x_guard = std::max(ON_ZERO_TOLERANCE,
			surface->Domain(0).Length() * 1.0e-10);
		    const double y_guard = std::max(ON_ZERO_TOLERANCE,
			surface->Domain(1).Length() * 1.0e-10);
		    if (fabs(first_end.x - second_start.x) <= x_guard &&
			    fabs(first_end.y - second_start.y) > y_guard)
			singular_iso = ON_Surface::x_iso;
		    else if (fabs(first_end.y - second_start.y) <= y_guard &&
			    fabs(first_end.x - second_start.x) > x_guard)
			singular_iso = ON_Surface::y_iso;
		}
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
		bool interior_connector_collapsed = false;
		if (surface_side < 0 && first->m_vi[1] >= 0 &&
			first->m_vi[1] < brep->m_V.Count() &&
			(singular_iso == ON_Surface::x_iso ||
			 singular_iso == ON_Surface::y_iso)) {
		    interior_connector_collapsed = true;
		    const ON_3dPoint &candidate_vertex =
			brep->m_V[first->m_vi[1]].point;
		    for (int sample = 0; interior_connector_collapsed &&
			    sample <= kDenseValidationSegments; ++sample) {
			const ON_3dPoint uv = singular_curve->PointAt(
			    singular_curve->Domain().ParameterAt(
				static_cast<double>(sample) /
				kDenseValidationSegments));
			const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			interior_connector_collapsed = lift.IsValid() &&
			    lift.DistanceTo(candidate_vertex) <= LocalUnits::tolerance;
		    }
		}
		if (surface_side < 0 &&
			interior_connector_collapsed &&
			(singular_iso == ON_Surface::x_iso ||
			 singular_iso == ON_Surface::y_iso)) {
		    /* Analytic cones can place their collapsed apex isoparameter in
		     * the interior of an overly broad generated surface domain.  A
		     * singular trim is valid only on a surface boundary, so restrict a
		     * private copy of this face's surface to the occupied side of the
		     * proven collapsed isoparameter.  Trimming a surface domain is exact;
		     * all existing pcurve lifts are checked before the face is switched. */
		    const int fixed_direction = singular_iso == ON_Surface::x_iso ? 0 : 1;
		    const ON_3dPoint vertex = brep->m_V[first->m_vi[1]].point;
		    /* The two supplied endpoints already lift to the shared STEP
		     * vertex within its asserted uncertainty.  Their common fixed
		     * parameter is therefore the authoritative collapsed side; a global
		     * closest-point solve on an extended analytic cone can select a
		     * different sheet of the same infinite surface. */
		    const double collapsed_parameter = 0.5 *
			(first_end[fixed_direction] + second_start[fixed_direction]);
		    const ON_Interval old_domain = surface->Domain(fixed_direction);
		    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
			old_domain.Length() * 1.0e-10);
		    bool occupied_low = false;
		    bool occupied_high = false;
		    for (int check_lti = 0; check_lti < loop.TrimCount(); ++check_lti) {
			const ON_BrepTrim *check = loop.Trim(check_lti);
			if (!check) continue;
			const ON_Interval check_domain = check->Domain();
			/* Invalid supplied pcurves can overshoot onto the opposite
			 * analytic sheet between their exact topology endpoints.  Use
			 * only those endpoints to choose the occupied side; all complete
			 * pcurve lifts are independently checked before installation. */
			for (int sample = 0; sample < 2; ++sample) {
			    const ON_3dPoint uv = check->PointAt(check_domain.ParameterAt(
				static_cast<double>(sample)));
			    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			    if (lift.IsValid() && lift.DistanceTo(vertex) <=
				    LocalUnits::tolerance)
				continue;
			    occupied_low = occupied_low || uv[fixed_direction] <
				collapsed_parameter - parameter_guard;
			    occupied_high = occupied_high || uv[fixed_direction] >
				collapsed_parameter + parameter_guard;
			}
		    }
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop " << li
			    << " interior collapsed isoparameter direction="
			    << fixed_direction << " parameter=" << collapsed_parameter
			    << " domain=" << old_domain.Min() << ':' << old_domain.Max()
			    << " occupied=" << (occupied_low ? 'L' : '-')
			    << (occupied_high ? 'H' : '-') << std::endl;
		    if (old_domain.Includes(collapsed_parameter, true) &&
			    occupied_low != occupied_high) {
			const int side = occupied_high ? 0 : 1;
			const ON_Interval retained = occupied_high ?
			    ON_Interval(collapsed_parameter, old_domain.Max()) :
			    ON_Interval(old_domain.Min(), collapsed_parameter);
			ON_Surface *candidate = surface->DuplicateSurface();
			bool exact_surface = candidate && retained.IsIncreasing() &&
			    candidate->Trim(fixed_direction, retained) &&
			    candidate->IsValid();
			/* A face can own several outer/inner loops that all use the same
			 * proxy surface.  Restricting that surface after checking only the
			 * loop containing this collapsed connector leaves every other loop
			 * in the old parameter domain; OpenNURBS then extrapolates their
			 * stale UVs, sometimes by model-scale distances.  Prove the private
			 * surface copy preserves every trim on the face before switching it. */
			for (int check_fli = 0; exact_surface &&
				check_fli < face->m_li.Count(); ++check_fli) {
			    const int check_loop_index = face->m_li[check_fli];
			    const ON_BrepLoop *check_loop =
				check_loop_index >= 0 &&
				check_loop_index < brep->m_L.Count() ?
				&brep->m_L[check_loop_index] : NULL;
			    if (!check_loop) {
				exact_surface = false;
				break;
			    }
			    for (int check_lti = 0; exact_surface &&
				    check_lti < check_loop->TrimCount(); ++check_lti) {
				const ON_BrepTrim *check = check_loop->Trim(check_lti);
				if (!check) {
				    exact_surface = false;
				    break;
				}
				const ON_Interval check_domain = check->Domain();
				for (int sample = 0; exact_surface && sample <= 64;
					++sample) {
				    const ON_3dPoint uv = check->PointAt(
					check_domain.ParameterAt(
					    static_cast<double>(sample) / 64.0));
				    const ON_Interval candidate_u = candidate->Domain(0);
				    const ON_Interval candidate_v = candidate->Domain(1);
				    if (!uv.IsValid() || !candidate_u.Includes(uv.x, true) ||
					    !candidate_v.Includes(uv.y, true)) {
					exact_surface = false;
					break;
				    }
				    const ON_3dPoint old_lift = surface->PointAt(uv.x, uv.y);
				    const ON_3dPoint new_lift = candidate->PointAt(uv.x, uv.y);
				    exact_surface = old_lift.IsValid() && new_lift.IsValid() &&
					old_lift.DistanceTo(new_lift) <=
					    std::max(ON_ZERO_TOLERANCE *
						kNumericalToleranceScale,
						LocalUnits::tolerance * 1.0e-8);
				}
			    }
			}
			if (exact_surface) {
				    const int surface_index = brep->AddSurface(candidate);
				    if (surface_index >= 0) {
					face->m_si = surface_index;
					face->SetProxySurface(candidate);
					surface = candidate;
				candidate = NULL;
				ON_3dPoint start = first_end;
				ON_3dPoint end = second_start;
				start[fixed_direction] = collapsed_parameter;
				end[fixed_direction] = collapsed_parameter;
				singular_curve.reset(new ON_LineCurve(start, end));
				singular_iso = fixed_direction == 0 ?
				    (side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
				    (side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
				surface_side = singular_side(singular_iso);
				wrapper->RecordRepair(entity_id, entity_type,
				    "face_surface",
				    "restricted an analytic surface to its exact collapsed apex");
			    }
			}
			if (!exact_surface && wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id << ": loop "
				<< li << " rejected exact analytic surface restriction at "
				<< collapsed_parameter << std::endl;
			delete candidate;
		    }
		}
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
		double collapse_tolerance = LocalUnits::tolerance;
		double measured_collapse_distance = 0.0;
		double collapse_adjustment_limit = LocalUnits::tolerance;
		bool measured_tolerance_adjustment = false;
		if (!surface->IsSingular(surface_side)) {
		    /* Only a supplied NURBS boundary can be geometrically collapsed
		     * without advertising ON_Surface::IsSingular().  Applying this
		     * inference to a long analytic cylinder lets an item-scale allowance
		     * misclassify its complete circular boundary as one vertex.  Analytic
		     * surfaces must expose a real singular side (or take the proven cone
		     * restriction path above). */
		    if (!ON_NurbsSurface::Cast(surface))
			continue;
		    const int fixed_direction =
			(surface_side == 1 || surface_side == 3) ? 0 : 1;
		    const int varying_direction = 1 - fixed_direction;
		    const int side =
			(surface_side == 1 || surface_side == 2) ? 1 : 0;
		    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
		    const ON_Interval varying_domain = surface->Domain(varying_direction);
		    bool collapsed_boundary = fixed_domain.IsIncreasing() &&
			varying_domain.IsIncreasing();
		    double measured_boundary_distance = 0.0;
		    for (int sample = 0; collapsed_boundary &&
			    sample <= kDenseValidationSegments; ++sample) {
			ON_3dPoint uv;
			uv[fixed_direction] = fixed_domain[side];
			uv[varying_direction] = varying_domain.ParameterAt(
			    static_cast<double>(sample) / kDenseValidationSegments);
			uv.z = 0.0;
			const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			collapsed_boundary = lift.IsValid();
			if (collapsed_boundary)
			    measured_boundary_distance = std::max(
				measured_boundary_distance, lift.DistanceTo(vertex));
		    }
		    if (collapsed_boundary && measured_boundary_distance >
			    LocalUnits::tolerance) {
			ON_BoundingBox surface_bounds;
			const double surface_scale = surface->GetBoundingBox(
			    surface_bounds, false) && surface_bounds.IsValid() ?
			    surface_bounds.Diagonal().Length() : 0.0;
			const double adjustment_limit = std::max(
			    LocalUnits::tolerance, surface_scale *
			    kCollapsedBoundaryMaximumRelativeMismatch);
			measured_collapse_distance = measured_boundary_distance;
			collapse_adjustment_limit = adjustment_limit;
			const double adjusted = measured_boundary_distance *
			    kRegenerationToleranceSafety;
			if (wrapper->ImportOptions().exact ||
				wrapper->ImportOptions().repair !=
				    brlcad::step::RepairMode::Safe ||
				!(adjusted <= adjustment_limit))
			    collapsed_boundary = false;
			else {
			    collapse_tolerance = adjusted;
			    measured_tolerance_adjustment = true;
			}
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
			    collapse_tolerance) {
			exact = false;
			break;
		    }
		}
		if (!exact)
		    continue;
		if (measured_tolerance_adjustment) {
		    ON_BrepVertex &adjusted_vertex = brep->m_V[vertex_index];
		    adjusted_vertex.m_tolerance = std::max(
			adjusted_vertex.m_tolerance, collapse_tolerance);
		    const int adjacent_trim_indices[2] = {
			first->m_trim_index, second->m_trim_index
		    };
		    for (int adjacent = 0; adjacent < 2; ++adjacent) {
			const int trim_index = adjacent_trim_indices[adjacent];
			if (trim_index < 0 || trim_index >= brep->m_T.Count())
			    continue;
			ON_BrepTrim &adjusted_trim = brep->m_T[trim_index];
			adjusted_trim.m_tolerance[0] = std::max(
			    adjusted_trim.m_tolerance[0], collapse_tolerance);
			adjusted_trim.m_tolerance[1] = std::max(
			    adjusted_trim.m_tolerance[1], collapse_tolerance);
			if (adjusted_trim.m_ei >= 0 &&
				adjusted_trim.m_ei < brep->m_E.Count())
			    brep->m_E[adjusted_trim.m_ei].m_tolerance = std::max(
				brep->m_E[adjusted_trim.m_ei].m_tolerance,
				collapse_tolerance);
		    }
		    std::string diagnostic =
			"source boundary asserted as one topology vertex exceeded the "
			"declared tolerance; used a densely measured OpenNURBS tolerance";
		    if (wrapper->Verbose()) {
			std::ostringstream detail;
			detail << diagnostic << "; measured boundary distance "
			    << measured_collapse_distance << ", effective tolerance "
			    << collapse_tolerance << ", bounded repair limit "
			    << collapse_adjustment_limit;
			diagnostic = detail.str();
		    }
		    wrapper->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Warning, entity_id,
			entity_type, "edge_loop", diagnostic);
		    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			"adjusted one source-collapsed boundary tolerance after dense validation");
		}
		std::vector<int> original_trims;
		original_trims.reserve(original_count);
		for (int offset = 0; offset < original_count; ++offset)
		    original_trims.push_back(loop.m_ti[offset]);
		const int c2_index = brep->AddTrimCurve(singular_curve.release());
		if (c2_index < 0)
		    continue;
		const int singular_index = brep->NewSingularTrim(
		    brep->m_V[vertex_index], loop, singular_iso, c2_index).m_trim_index;
		brep->m_T[singular_index].m_tolerance[0] = collapse_tolerance;
		brep->m_T[singular_index].m_tolerance[1] = collapse_tolerance;
		loop.m_ti.SetCount(0);
		for (int offset = 0; offset < original_count; ++offset) {
		    loop.m_ti.Append(original_trims[offset]);
		    if (offset == lti)
			loop.m_ti.Append(singular_index);
		}
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    measured_tolerance_adjustment ?
		    "inserted a measured-tolerance singular trim for a source-asserted collapsed boundary" :
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
	if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
	    if (failure)
		*failure = "periodic translation validation was cancelled";
	    return false;
	}
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


/* Return a private pcurve translated by exact integral surface periods so its
 * selected endpoint lies on the parameter branch of join.  A small residual
 * endpoint discrepancy is deliberately left for the ordinary, densely
 * validated endpoint repair; accepting it here would turn a parameter branch
 * recognition bound into a geometric tolerance. */
static ON_Curve *
translated_periodic_trim_for_join(const ON_Surface *surface,
    const ON_BrepTrim &trim, const ON_3dPoint &join, bool move_start,
    std::string *failure)
{
    if (failure)
	failure->clear();
    if (!surface || !join.IsValid())
	return NULL;

    const ON_3dPoint endpoint = move_start ? trim.PointAtStart() :
	trim.PointAtEnd();
    ON_3dVector translation = join - endpoint;
    bool shifted = false;
    for (int axis = 0; axis < 2; ++axis) {
	const double period = surface->Domain(axis).Length();
	const double parameter_window = std::max(ON_ZERO_TOLERANCE,
	    kPeriodicParameterSnapFraction * std::max(1.0, fabs(period)));
	if (fabs(translation[axis]) <= parameter_window) {
	    translation[axis] = 0.0;
	    continue;
	}
	if (!surface->IsClosed(axis) || !(period > ON_ZERO_TOLERANCE)) {
	    if (failure)
		*failure = "the endpoint difference was not on a closed surface direction";
	    return NULL;
	}
	const double periods = round(translation[axis] / period);
	if (fabs(periods) < 0.5 || fabs(translation[axis] - periods * period) >
		parameter_window) {
	    if (failure)
		*failure = "the endpoint difference was not within the periodic branch window";
	    return NULL;
	}
	/* Never incorporate the source discrepancy into the transform. */
	translation[axis] = periods * period;
	shifted = true;
    }
    if (!shifted)
	return NULL;

    ON_Curve *translated = trim.DuplicateCurve();
    ON_Xform transform(ON_Xform::IdentityTransformation);
    transform.m_xform[0][3] = translation.x;
    transform.m_xform[1][3] = translation.y;
    if (!translated || !translated->Transform(transform) ||
	    !translated->ChangeDimension(2) || !translated->IsValid() ||
	    !validate_periodic_trim_translation(surface, trim, *translated,
		failure)) {
	delete translated;
	return NULL;
    }
    return translated;
}


static double
verified_regeneration_tolerance(ON_BrepTrim &trim,
	ON_BrepEdge &edge, const ON_Surface *surface,
	const ON_NurbsCurve &edge_nurbs, double tolerance,
	const ON_Brep *brep, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type)
{
    if (!surface || !wrapper || wrapper->ImportOptions().exact ||
	    !(tolerance > 0.0))
	return tolerance;

    const ON_Interval trim_domain = trim.Domain();
    const ON_Interval edge_domain = edge_nurbs.Domain();
    const ON_BoundingBox bounds = edge_nurbs.BoundingBox();
    const double scale = bounds.IsValid() ? bounds.Diagonal().Length() : 0.0;
    ON_BoundingBox item_bounds;
    const double item_scale = brep && brep->GetBoundingBox(item_bounds, false) &&
	item_bounds.IsValid() ? item_bounds.Diagonal().Length() : 0.0;
    const double limit = std::max(tolerance,
	std::max(scale * kRegenerationMaximumRelativeMismatch,
	    item_scale * kRegenerationMaximumRelativeItemMismatch));
    const double solver_tolerance = std::max(
	ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance * 0.1);
    double measured = 0.0;
    for (int sample = 0;
	    sample <= kRegenerationMeasurementSegments; ++sample) {
	if (brlcad::PullbackWorkCancelled())
	    return tolerance;
	const double fraction = static_cast<double>(sample) /
	    kRegenerationMeasurementSegments;
	const ON_3dPoint uv = trim.PointAt(trim_domain.ParameterAt(fraction));
	const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	const ON_3dPoint edge_point = edge_nurbs.PointAt(
	    edge_domain.ParameterAt(trim.m_bRev3d ? 1.0 - fraction : fraction));
	if (!lift.IsValid() || !edge_point.IsValid())
	    return tolerance;
	const double corresponding_distance = lift.DistanceTo(edge_point);
	/* The supplied pcurve is a useful projection seed and an upper bound.  If
	 * that upper bound is already inside the tolerance established so far, no
	 * closest-surface solve can increase the measured source separation. */
	if (corresponding_distance <= std::max(tolerance, measured))
	    continue;
	ON_3dPoint projected_uv = uv;
	double surface_distance = DBL_MAX;
	if (!refine_surface_pullback_seeded(surface, edge_point,
		solver_tolerance, projected_uv, &surface_distance, false, limit))
	    return tolerance;
	measured = std::max(measured, surface_distance);
    }
    if (!(measured > tolerance))
	return tolerance;

    const double adjusted = measured * kRegenerationToleranceSafety;
    if (!(adjusted <= limit)) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": trim "
		<< trim.m_trim_index << "/STEP edge " << edge.m_edge_user.i
		<< " regeneration measured source "
		<< "mismatch " << measured << " but bounded adjustment "
		<< adjusted << " exceeds limit " << limit << std::endl;
	return tolerance;
    }

    edge.m_tolerance = std::max(edge.m_tolerance, adjusted);
    trim.m_tolerance[0] = std::max(trim.m_tolerance[0], adjusted);
    trim.m_tolerance[1] = std::max(trim.m_tolerance[1], adjusted);
    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	entity_id, entity_type, "trim_pcurve",
	"source edge/surface separation exceeded the declared tolerance; "
	"used a densely measured tolerance for exact pcurve regeneration");
    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	"adjusted one trim tolerance to measured source geometry");
    return adjusted;
}


static size_t
repair_exact_periodic_loop_branches(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    struct BranchState {
	int shift[2];
	ON_3dPoint start;
	ON_3dPoint end;
    };
    struct ShiftedCurve {
	ON_BrepTrim *trim;
	ON_Curve *curve;
    };

    size_t repaired_loops = 0;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if (brlcad::PullbackWorkCancelled())
	    return repaired_loops;
	ON_BrepLoop &loop = brep->m_L[li];
	const ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (!surface || loop.TrimCount() < 2 ||
		(!surface->IsClosed(0) && !surface->IsClosed(1)))
	    continue;

	const double period[2] = {
	    surface->IsClosed(0) ? surface->Domain(0).Length() : 0.0,
	    surface->IsClosed(1) ? surface->Domain(1).Length() : 0.0
	};
	double parameter_tolerance = ON_ZERO_TOLERANCE;
	for (int axis = 0; axis < 2; ++axis) {
	    if (period[axis] > ON_ZERO_TOLERANCE)
		parameter_tolerance = std::max(parameter_tolerance,
		    period[axis] * 1.0e-10);
	}

	/* Only solve a parameter-branch discontinuity whose source topology and
	 * 3-D surface lifts already prove that every adjacent pair shares the
	 * asserted STEP vertex.  This prevents periodicity from hiding a real gap. */
	bool exact_topology = true;
	double original_cost = 0.0;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *previous = loop.Trim(lti);
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!previous || !next || previous->m_vi[1] < 0 ||
		    previous->m_vi[1] != next->m_vi[0] ||
		    previous->m_vi[1] >= brep->m_V.Count()) {
		exact_topology = false;
		break;
	    }
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint next_uv = next->PointAtStart();
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
	    double tolerance = LocalUnits::tolerance;
	    if (previous->Edge())
		tolerance = std::max(tolerance, previous->Edge()->m_tolerance);
	    if (next->Edge())
		tolerance = std::max(tolerance, next->Edge()->m_tolerance);
	    tolerance = std::max(tolerance,
		brep->m_V[previous->m_vi[1]].m_tolerance);
	    const ON_3dPoint &vertex = brep->m_V[previous->m_vi[1]].point;
	    if (!previous_lift.IsValid() || !next_lift.IsValid() ||
		    previous_lift.DistanceTo(vertex) > tolerance ||
		    next_lift.DistanceTo(vertex) > tolerance) {
		exact_topology = false;
		break;
	    }
	    original_cost += previous_uv.DistanceTo(next_uv);
	}
	if (!exact_topology || original_cost <= parameter_tolerance)
	    continue;

	std::vector<ON_BrepTrim *> trims;
	std::vector<std::vector<BranchState> > states;
	trims.reserve(loop.TrimCount());
	states.resize(loop.TrimCount());
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    if (!trim) {
		exact_topology = false;
		break;
	    }
	    trims.push_back(trim);
	    int fixed_direction = -1;
	    if (trim->m_type == ON_BrepTrim::seam ||
		    trim->m_type == ON_BrepTrim::singular) {
		if (trim->m_iso == ON_Surface::W_iso ||
			trim->m_iso == ON_Surface::E_iso)
		    fixed_direction = 0;
		else if (trim->m_iso == ON_Surface::S_iso ||
			trim->m_iso == ON_Surface::N_iso)
		    fixed_direction = 1;
	    }
	    const bool special = trim->m_type == ON_BrepTrim::seam ||
		trim->m_type == ON_BrepTrim::singular;
	    const bool fixed0 = lti == 0 || !(period[0] > ON_ZERO_TOLERANCE) ||
		(special && fixed_direction != 1);
	    const bool fixed1 = lti == 0 || !(period[1] > ON_ZERO_TOLERANCE) ||
		(special && fixed_direction != 0);
	    const int minimum0 = fixed0 ? 0 : -kMaximumPeriodicBranchShift;
	    const int maximum0 = fixed0 ? 0 : kMaximumPeriodicBranchShift;
	    const int minimum1 = fixed1 ? 0 : -kMaximumPeriodicBranchShift;
	    const int maximum1 = fixed1 ? 0 : kMaximumPeriodicBranchShift;
	    for (int shift0 = minimum0; shift0 <= maximum0; ++shift0) {
		for (int shift1 = minimum1; shift1 <= maximum1; ++shift1) {
		    BranchState state;
		    state.shift[0] = shift0;
		    state.shift[1] = shift1;
		    state.start = trim->PointAtStart();
		    state.end = trim->PointAtEnd();
		    state.start.x += shift0 * period[0];
		    state.end.x += shift0 * period[0];
		    state.start.y += shift1 * period[1];
		    state.end.y += shift1 * period[1];
		    states[lti].push_back(state);
		}
	    }
	}
	if (!exact_topology)
	    continue;

	const size_t count = trims.size();
	std::vector<std::vector<double> > costs(count);
	std::vector<std::vector<int> > predecessors(count);
	for (size_t position = 0; position < count; ++position) {
	    costs[position].assign(states[position].size(), DBL_MAX);
	    predecessors[position].assign(states[position].size(), -1);
	}
	costs[0][0] = 0.0;
	for (size_t position = 1; position < count; ++position) {
	    for (size_t current = 0; current < states[position].size(); ++current) {
		for (size_t previous = 0;
			previous < states[position - 1].size(); ++previous) {
		    if (costs[position - 1][previous] >= DBL_MAX)
			continue;
		    const double cost = costs[position - 1][previous] +
			states[position - 1][previous].end.DistanceTo(
			    states[position][current].start);
		    if (cost < costs[position][current]) {
			costs[position][current] = cost;
			predecessors[position][current] =
			    static_cast<int>(previous);
		    }
		}
	    }
	}

	double best_cost = DBL_MAX;
	int best_last = -1;
	for (size_t state = 0; state < states[count - 1].size(); ++state) {
	    const double cost = costs[count - 1][state] +
		states[count - 1][state].end.DistanceTo(states[0][0].start);
	    if (cost < best_cost) {
		best_cost = cost;
		best_last = static_cast<int>(state);
	    }
	}
	if (best_last < 0 || best_cost > parameter_tolerance * count ||
		!(best_cost + parameter_tolerance < original_cost))
	    continue;

	std::vector<int> selected(count, 0);
	selected[count - 1] = best_last;
	bool complete = true;
	for (size_t position = count - 1; position > 0; --position) {
	    selected[position - 1] =
		predecessors[position][selected[position]];
	    if (selected[position - 1] < 0) {
		complete = false;
		break;
	    }
	}
	if (!complete)
	    continue;

	std::vector<ShiftedCurve> candidates;
	bool exact = true;
	for (size_t position = 0; position < count; ++position) {
	    const BranchState &state = states[position][selected[position]];
	    if (state.shift[0] == 0 && state.shift[1] == 0)
		continue;
	    ON_Curve *candidate = trims[position]->DuplicateCurve();
	    ON_Xform translation(ON_Xform::IdentityTransformation);
	    translation.m_xform[0][3] = state.shift[0] * period[0];
	    translation.m_xform[1][3] = state.shift[1] * period[1];
	    std::string failure;
	    if (!candidate || !candidate->Transform(translation) ||
		    !candidate->ChangeDimension(2) || !candidate->IsValid() ||
		    !validate_periodic_trim_translation(surface, *trims[position],
			*candidate, &failure)) {
		delete candidate;
		exact = false;
		break;
	    }
	    candidates.push_back({trims[position], candidate});
	}
	if (!exact || candidates.empty()) {
	    for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
		delete candidates[candidate].curve;
	    continue;
	}

	bool installed = true;
	for (size_t candidate = 0; candidate < candidates.size(); ++candidate) {
	    const int c2_index = brep->AddTrimCurve(candidates[candidate].curve);
	    if (c2_index < 0 || !brep->SetTrimCurve(*candidates[candidate].trim,
		    c2_index)) {
		if (c2_index < 0)
		    delete candidates[candidate].curve;
		installed = false;
		break;
	    }
	    brep->SetTrimIsoFlags(*candidates[candidate].trim);
	}
	if (!installed)
	    continue;
	++repaired_loops;
	wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	    "coherently unwrapped an exact periodic pcurve loop");
    }
    return repaired_loops;
}


static size_t
regenerate_periodic_loop_chains(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return 0;

    size_t repaired_loops = 0;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if (brlcad::PullbackWorkCancelled())
	    return repaired_loops;
	ON_BrepLoop &loop = brep->m_L[li];
	const ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (!surface || loop.TrimCount() < 2 ||
		(!surface->IsClosed(0) && !surface->IsClosed(1)))
	    continue;
	bool has_paired_edge_use = false;
	std::set<int> loop_edges;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (trim && trim->m_ei >= 0 &&
		    !loop_edges.insert(trim->m_ei).second)
		has_paired_edge_use = true;
	}

	/* A very large raw UV discontinuity can be a succession of equivalent
	 * periodic branch choices rather than a 3-D topology gap.  Rebuild only
	 * loops for which the two source pcurve lifts still meet the asserted STEP
	 * vertex within the already proven edge/vertex tolerances. */
	bool needs_regeneration = false;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *previous = loop.Trim(lti);
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    if (!previous || !next || previous->m_vi[1] < 0 ||
		    previous->m_vi[1] != next->m_vi[0] ||
		    previous->m_vi[1] >= brep->m_V.Count())
		continue;
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint next_uv = next->PointAtStart();
	    bool large_periodic_gap = false;
	    for (int direction = 0; direction < 2; ++direction) {
		const double period = surface->Domain(direction).Length();
		/* One period is an ordinary min/max discontinuity only at an explicit
		 * seam.  Two ordinary boundary trims one period apart leave an invalid
		 * p-space loop and require coherent whole-loop regeneration.  A paired
		 * edge use in this same loop is OpenNURBS' topology definition of a seam,
		 * including an interior keyhole/slit.  Such a loop must also close
		 * numerically at each join, even when an earlier derived m_iso happened
		 * to classify the pair as a surface-boundary seam. */
		const bool ordinary_join = previous->m_type != ON_BrepTrim::seam &&
		    next->m_type != ON_BrepTrim::seam &&
		    previous->m_type != ON_BrepTrim::singular &&
		    next->m_type != ON_BrepTrim::singular;
		const double gap_threshold = ordinary_join || has_paired_edge_use ?
		    0.5 : 1.5;
		if (surface->IsClosed(direction) && period > ON_ZERO_TOLERANCE &&
			fabs(previous_uv[direction] - next_uv[direction]) >
			    gap_threshold * period) {
		    large_periodic_gap = true;
		    break;
		}
	    }
	    if (!large_periodic_gap)
		continue;
	    double tolerance = LocalUnits::tolerance;
	    if (previous->Edge()) tolerance = std::max(tolerance,
		previous->Edge()->m_tolerance);
	    if (next->Edge()) tolerance = std::max(tolerance,
		next->Edge()->m_tolerance);
	    tolerance = std::max(tolerance,
		brep->m_V[previous->m_vi[1]].m_tolerance);
	    const ON_3dPoint &vertex = brep->m_V[previous->m_vi[1]].point;
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
	    if (previous_lift.IsValid() && next_lift.IsValid() &&
		    previous_lift.DistanceTo(vertex) <= tolerance &&
		    next_lift.DistanceTo(vertex) <= tolerance) {
		needs_regeneration = true;
		break;
	    }
	}
	if (!needs_regeneration)
	    continue;

	struct OriginalTrimCurve {
	    int trim_index;
	    ON_Curve *curve;
	    ON_Surface::ISO iso;
	};
	std::vector<OriginalTrimCurve> originals;
	originals.reserve(loop.TrimCount());
	bool saved = true;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
	    if (!trim || !curve) {
		delete curve;
		saved = false;
		break;
	    }
	    originals.push_back({trim->m_trim_index, curve, trim->m_iso});
	}
	if (!saved) {
	    for (std::vector<OriginalTrimCurve>::iterator original =
		    originals.begin(); original != originals.end(); ++original)
		delete original->curve;
	    continue;
	}

	ON_BrepTrim *first = loop.Trim(0);
	ON_BrepEdge *first_edge = first ? first->Edge() : NULL;
	double first_tolerance = LocalUnits::tolerance;
	if (first_edge)
	    first_tolerance = std::max(first_tolerance, first_edge->m_tolerance);
	ON_3dPoint loop_start = first ? first->PointAtStart() :
	    ON_3dPoint::UnsetPoint;
	if (first && first_edge && first->m_vi[0] >= 0 &&
		first->m_vi[0] < brep->m_V.Count())
	    first_tolerance = std::max(first_tolerance,
		brep->m_V[first->m_vi[0]].m_tolerance);
	if (first && first_edge)
	    normalize_closed_surface_parameter(surface, first_edge->PointAt(
		first_edge->Domain()[first->m_bRev3d ? 1 : 0]),
		first_tolerance, loop_start);

	bool regenerated = first && first_edge && loop_start.IsValid();
	ON_3dPoint required_start = loop_start;
	std::string failure;
	for (int lti = 0; regenerated && lti < loop.TrimCount(); ++lti) {
	    if (brlcad::PullbackWorkCancelled()) {
		regenerated = false;
		failure = "periodic loop-chain regeneration was cancelled";
		break;
	    }
	    ON_BrepTrim *trim = loop.Trim(lti);
	    ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    ON_NurbsCurve edge_nurbs;
	    double tolerance = LocalUnits::tolerance;
	    if (edge)
		tolerance = std::max(tolerance, edge->m_tolerance);
	    if (trim && trim->m_vi[0] >= 0 && trim->m_vi[0] < brep->m_V.Count())
		tolerance = std::max(tolerance,
		    brep->m_V[trim->m_vi[0]].m_tolerance);
	    if (trim && trim->m_vi[1] >= 0 && trim->m_vi[1] < brep->m_V.Count())
		tolerance = std::max(tolerance,
		    brep->m_V[trim->m_vi[1]].m_tolerance);
	    if (trim && edge && edge->GetNurbForm(edge_nurbs))
		tolerance = verified_regeneration_tolerance(*trim, *edge,
		    surface, edge_nurbs, tolerance, brep, wrapper, entity_id,
		    entity_type);
	    const ON_3dPoint *required_end = lti + 1 == loop.TrimCount() ?
		&loop_start : NULL;
	    regenerated = trim && edge && edge_nurbs.IsValid() &&
		regenerate_trim_polyline(brep, *trim, surface, edge_nurbs,
		    tolerance, &failure, NULL, &required_start, required_end, true,
		    wrapper, has_paired_edge_use);
	    if (regenerated)
		required_start = trim->PointAtEnd();
	}
	for (int lti = 0; regenerated && lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *previous = loop.Trim(lti);
	    const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
	    const double gap = previous && next ? previous->PointAtEnd().DistanceTo(
		next->PointAtStart()) : DBL_MAX;
	    regenerated = previous && next && gap <= ON_ZERO_TOLERANCE;
	    if (!regenerated && previous && next && previous->m_vi[1] >= 0 &&
		    previous->m_vi[1] == next->m_vi[0] &&
		    previous->m_vi[1] < brep->m_V.Count()) {
		const double parameter_limit = std::max(ON_ZERO_TOLERANCE,
		    std::max(surface->Domain(0).Length(),
			surface->Domain(1).Length()) *
			kPeriodicRegenerationJoinParameterFraction);
		const ON_3dPoint previous_uv = previous->PointAtEnd();
		const ON_3dPoint next_uv = next->PointAtStart();
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		double join_tolerance = LocalUnits::tolerance;
		if (previous->Edge()) join_tolerance = std::max(join_tolerance,
		    previous->Edge()->m_tolerance);
		if (next->Edge()) join_tolerance = std::max(join_tolerance,
		    next->Edge()->m_tolerance);
		join_tolerance = std::max(join_tolerance,
		    brep->m_V[previous->m_vi[1]].m_tolerance);
		const ON_3dPoint &vertex = brep->m_V[previous->m_vi[1]].point;
		regenerated = gap <= parameter_limit && previous_lift.IsValid() &&
		    next_lift.IsValid() && previous_lift.DistanceTo(vertex) <=
			join_tolerance && next_lift.DistanceTo(vertex) <=
			join_tolerance && previous_lift.DistanceTo(next_lift) <=
			join_tolerance;
	    }
	    if (!regenerated) {
		failure = "regenerated join " + std::to_string(lti) + "/" +
		    std::to_string((lti + 1) % loop.TrimCount()) +
		    " retained a parameter-space gap of " + std::to_string(gap);
		break;
	    }
	}

	if (!regenerated) {
	    for (std::vector<OriginalTrimCurve>::iterator original =
		    originals.begin(); original != originals.end(); ++original) {
		const int c2_index = brep->AddTrimCurve(original->curve);
		if (c2_index >= 0) {
		    original->curve = NULL;
		    if (original->trim_index >= 0 &&
			    original->trim_index < brep->m_T.Count() &&
			    brep->SetTrimCurve(brep->m_T[original->trim_index],
				c2_index))
			brep->m_T[original->trim_index].m_iso = original->iso;
		}
		delete original->curve;
	    }
	    if (wrapper->Verbose() && !failure.empty())
		std::cerr << entity_type << " #" << entity_id << ": loop " << li
		    << " periodic chain regeneration rejected: " << failure
		    << std::endl;
	    continue;
	}
	for (std::vector<OriginalTrimCurve>::iterator original = originals.begin();
		original != originals.end(); ++original)
	    delete original->curve;
	++repaired_loops;
	wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	    "regenerated an exact periodic loop chain from its 3-D STEP edges");
    }
    return repaired_loops;
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
    std::set<int> translated_trim_indices;
    std::map<int, std::set<std::vector<double> > > seen_loop_states;

    const auto loop_endpoint_state = [](const ON_BrepLoop &loop) {
	std::vector<double> state;
	state.reserve(static_cast<size_t>(loop.TrimCount()) * 8);
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (!trim) {
		state.push_back(ON_UNSET_VALUE);
		continue;
	    }
	    const ON_3dPoint start = trim->PointAtStart();
	    const ON_3dPoint end = trim->PointAtEnd();
	    state.push_back(start.x);
	    state.push_back(start.y);
	    state.push_back(end.x);
	    state.push_back(end.y);
	    state.push_back(static_cast<double>(trim->m_type));
	    state.push_back(static_cast<double>(trim->m_iso));
	    state.push_back(static_cast<double>(trim->m_vi[0]));
	    state.push_back(static_cast<double>(trim->m_vi[1]));
	}
	return state;
    };

    std::vector<bool> dirty(static_cast<size_t>(brep->m_L.Count()), true);

    for (int sweep = 0; sweep < kMaximumEndpointRepairSweeps; ++sweep) {
	if (brlcad::PullbackWorkCancelled())
	    return;
	wrapper->SetProgressDetail("repairing adjacent exact BREP trim endpoints",
	    entity_id, sweep, kMaximumEndpointRepairSweeps, "sweeps", entity_type);
	bool changed = false;
	std::vector<bool> next_dirty(static_cast<size_t>(brep->m_L.Count()), false);
	for (int li = 0; li < brep->m_L.Count(); ++li) {
	    if (!dirty[static_cast<size_t>(li)])
		continue;
	    if (brlcad::PullbackWorkCancelled())
		return;
	    ON_BrepLoop &loop = brep->m_L[li];
	    /* A sequence of endpoint-local repairs can otherwise oscillate between
	     * two equivalent periodic branches.  Exact endpoint state is a complete
	     * key for this propagation pass; revisiting one proves that another
	     * sweep cannot add information. */
	    const std::vector<double> starting_state = loop_endpoint_state(loop);
	    if (!seen_loop_states[li].insert(starting_state).second)
		continue;
	    bool loop_changed = false;
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface || loop.TrimCount() < 2)
		continue;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		if (brlcad::PullbackWorkCancelled())
		    return;
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
		double join_tolerance = LocalUnits::tolerance;
		if (previous->Edge())
		    join_tolerance = std::max(join_tolerance,
			previous->Edge()->m_tolerance);
		if (next->Edge())
		    join_tolerance = std::max(join_tolerance,
			next->Edge()->m_tolerance);
		join_tolerance = std::max(join_tolerance,
		    std::max(previous->m_tolerance[0], previous->m_tolerance[1]));
		join_tolerance = std::max(join_tolerance,
		    std::max(next->m_tolerance[0], next->m_tolerance[1]));
		join_tolerance = std::max(join_tolerance,
		    brep->m_V[previous->m_vi[1]].m_tolerance);
		const ON_3dPoint previous_lift = surface->PointAt(
		    previous_uv.x, previous_uv.y);
		const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
		if (!previous_lift.IsValid() || !next_lift.IsValid() ||
			previous_lift.DistanceTo(vertex) > join_tolerance ||
			next_lift.DistanceTo(vertex) > join_tolerance) {
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

		/* Adjacent ordinary trims can be returned on different periodic
		 * images of the same closed surface, including doubly periodic tori.
		 * Move the complete following pcurve by integral surface periods
		 * before considering any endpoint-local edit.  Dense lift validation
		 * proves this is a parameter-branch change only. */
		if (previous->m_type != ON_BrepTrim::seam &&
			next->m_type != ON_BrepTrim::seam &&
			previous->m_type != ON_BrepTrim::singular &&
			next->m_type != ON_BrepTrim::singular) {
		    std::string translation_failure;
		    ON_Curve *translated = translated_trim_indices.find(
			next->m_trim_index) == translated_trim_indices.end() ?
			translated_periodic_trim_for_join(surface, *next,
			    previous_uv, true, &translation_failure) : NULL;
		    if (translated) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*next, c2_index)) {
			    brep->SetTrimIsoFlags(*next);
			    wrapper->RecordRepair(entity_id, entity_type,
				"trim_pcurve",
				"translated an exact pcurve onto its adjacent periodic branch");
			    translated_trim_indices.insert(next->m_trim_index);
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (c2_index < 0)
			    delete translated;
		    }
		    if (!translated && wrapper->Verbose() &&
			    !translation_failure.empty())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " periodic ordinary join "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " was not translated: " << translation_failure
			    << std::endl;
		}

		/* A singular trim is a genuine zero-length boundary at a surface
		 * pole.  When its ordinary neighbor arrives on the opposite periodic
		 * branch, move that complete neighbor by an exact period before any
		 * endpoint-local edit. */
		if ((previous->m_type == ON_BrepTrim::singular) !=
			(next->m_type == ON_BrepTrim::singular)) {
		    ON_BrepTrim *movable = previous->m_type == ON_BrepTrim::singular ?
			next : previous;
		    const ON_3dPoint singular_join = previous->m_type ==
			ON_BrepTrim::singular ? previous_uv : next_uv;
		    const bool move_start = movable == next;
		    std::string translation_failure;
		    ON_Curve *translated = translated_trim_indices.find(
			movable->m_trim_index) == translated_trim_indices.end() ?
			translated_periodic_trim_for_join(surface, *movable,
			    singular_join, move_start, &translation_failure) : NULL;
		    if (translated) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*movable, c2_index)) {
			    brep->SetTrimIsoFlags(*movable);
			    wrapper->RecordRepair(entity_id, entity_type,
				"trim_pcurve",
				"shifted an exact pcurve onto a singular trim's periodic branch");
			    translated_trim_indices.insert(movable->m_trim_index);
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (c2_index < 0)
			    delete translated;
		    }
		    if (!translated && wrapper->Verbose() &&
			    !translation_failure.empty())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " periodic singular join "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " was not translated: " << translation_failure
			    << std::endl;
		}

		/* When exactly one side is a seam, first try moving the entire
		 * non-seam pcurve by integral closed-surface periods.  This preserves
		 * its shape and edge correspondence, unlike bending only its endpoint. */
		if ((previous->m_type == ON_BrepTrim::seam) !=
			(next->m_type == ON_BrepTrim::seam)) {
		    ON_BrepTrim *movable = previous->m_type == ON_BrepTrim::seam ?
			next : previous;
		    const ON_3dPoint seam_join = movable == previous ? next_uv :
			previous_uv;
		    std::string translation_failure;
		    ON_Curve *translated = translated_trim_indices.find(
			movable->m_trim_index) == translated_trim_indices.end() ?
			translated_periodic_trim_for_join(surface, *movable,
			    seam_join, movable == next, &translation_failure) : NULL;
		    if (translated) {
			const int c2_index = brep->AddTrimCurve(translated);
			if (c2_index >= 0 && brep->SetTrimCurve(*movable, c2_index)) {
			    brep->SetTrimIsoFlags(*movable);
			    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
				"shifted an exact non-seam pcurve onto an adjacent periodic branch");
			    translated_trim_indices.insert(movable->m_trim_index);
			    changed = true;
			    loop_changed = true;
			    continue;
			}
			if (c2_index < 0)
			    delete translated;
		    }
		    if (!translated && wrapper->Verbose() &&
			    !translation_failure.empty())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " periodic seam join "
			    << previous->m_trim_index << '/' << next->m_trim_index
			    << " was not translated: " << translation_failure
			    << std::endl;

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
			    &seeded_failure, NULL, required_start, required_end,
			    false, wrapper,
			    surface->IsClosed(0) && surface->IsClosed(1));
		    const ON_3dPoint seeded_join = seeded ?
			(movable == previous ? movable->PointAtEnd() :
			 movable->PointAtStart()) : ON_3dPoint::UnsetPoint;
		    if (seeded && seeded_join.IsValid() &&
			    seeded_join.DistanceTo(seam_join) <= ON_ZERO_TOLERANCE) {
			wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			    "regenerated a non-seam pcurve from an exact periodic seam endpoint");
			changed = true;
			loop_changed = true;
			continue;
		    }
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id << ": loop "
			    << li << " seam-seeded regeneration for trim "
			    << movable->m_trim_index << " rejected: regenerated="
			    << (seeded ? "yes" : "no") << " join="
			    << (seeded_join.IsValid() ?
				seeded_join.DistanceTo(seam_join) : DBL_MAX)
			    << (seeded_failure.empty() ? "" :
				", detail=" + seeded_failure) << std::endl;
		}

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
		/* Every UV along a collapsed side has the same 3-D lift, so a
		 * model-space-only endpoint test cannot detect moving a singular
		 * trim off its exact boundary.  Its densely proven pcurve is
		 * authoritative; close the ordinary neighbor onto that endpoint. */
		if ((previous->m_type == ON_BrepTrim::singular) !=
			(next->m_type == ON_BrepTrim::singular))
		    common = previous->m_type == ON_BrepTrim::singular ?
			previous_uv : next_uv;

		/* Parameter-space midpoint is not a model-space midpoint on a
		 * nonlinear surface.  When two independently supplied endpoints are
		 * each valid at their shared STEP vertex but their lifts differ by up
		 * to twice the file uncertainty, search the bounded UV chord for a
		 * common lift that remains within the declared tolerance of both.
		 * This changes only trim association; the exact 3-D edges are retained
		 * and the complete candidate curves are densely checked below. */
		if (previous->m_type != ON_BrepTrim::seam &&
			next->m_type != ON_BrepTrim::seam &&
			previous->m_type != ON_BrepTrim::singular &&
			next->m_type != ON_BrepTrim::singular) {
		    const ON_3dPoint initial_lift = surface->PointAt(common.x, common.y);
		    const bool initial_common_valid = initial_lift.IsValid() &&
			initial_lift.DistanceTo(vertex) <= join_tolerance &&
			initial_lift.DistanceTo(previous_lift) <= join_tolerance &&
			initial_lift.DistanceTo(next_lift) <= join_tolerance;
		    if (!initial_common_valid) {
			double best_score = DBL_MAX;
			ON_3dPoint best_common = ON_3dPoint::UnsetPoint;
			for (int sample = 0; sample <= kDenseValidationSegments;
				++sample) {
			    if ((sample & 63) == 0 &&
				    brlcad::PullbackWorkCancelled())
				return;
			    const double fraction = static_cast<double>(sample) /
				kDenseValidationSegments;
			    const ON_3dPoint candidate = (1.0 - fraction) * previous_uv +
				fraction * aligned_next;
			    const ON_3dPoint lift = surface->PointAt(candidate.x,
				candidate.y);
			    if (!lift.IsValid())
				continue;
			    const double vertex_distance = lift.DistanceTo(vertex);
			    const double previous_distance = lift.DistanceTo(previous_lift);
			    const double next_distance = lift.DistanceTo(next_lift);
			    if (vertex_distance > join_tolerance ||
				    previous_distance > join_tolerance ||
				    next_distance > join_tolerance)
				continue;
			    const double score = std::max(vertex_distance,
				std::max(previous_distance, next_distance));
			    if (score < best_score) {
				best_score = score;
				best_common = candidate;
			    }
			}
			if (best_common.IsValid())
			    common = best_common;
		    }
		}
		const ON_3dPoint common_lift = surface->PointAt(common.x, common.y);
		if (!common_lift.IsValid() ||
			common_lift.DistanceTo(vertex) > join_tolerance) {
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

		const auto validates = [brep, surface, join_tolerance](
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
			if ((sample & 63) == 0 &&
				brlcad::PullbackWorkCancelled()) {
			    if (failure)
				*failure = "endpoint candidate validation was cancelled";
			    return false;
			}
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
				    join_tolerance) {
			    if (failure)
				*failure = "surface lift changed at sample " +
				    std::to_string(sample) + " by " +
				    std::to_string(original_lift.DistanceTo(candidate_lift));
			    return false;
			}
			if (edge) {
			    const ON_3dPoint corresponding_edge_point = edge->PointAt(
				edge->Domain().ParameterAt(original.m_bRev3d ?
				    1.0 - fraction : fraction));
			    double edge_distance = corresponding_edge_point.IsValid() ?
				candidate_lift.DistanceTo(corresponding_edge_point) : DBL_MAX;
			    /* Endpoint candidates preserve the trim's parameterization in
			     * the overwhelmingly common case.  The corresponding exact edge
			     * point is therefore a sufficient, stronger validation and avoids
			     * thousands of expensive global NURBS closest-point searches on
			     * large solids.  Fall back to the locus query only when that direct
			     * correspondence does not already prove the candidate. */
			    if (edge_distance > join_tolerance) {
				double edge_parameter = 0.0;
				if (ON_NurbsCurve_GetClosestPoint(&edge_parameter,
					&edge_nurbs, candidate_lift))
				    edge_distance = std::min(edge_distance,
					candidate_lift.DistanceTo(
					    edge_nurbs.PointAt(edge_parameter)));
			    }
			    if (edge_distance > join_tolerance) {
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
				    join_tolerance) {
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
			if ((sample & 63) == 0 &&
				brlcad::PullbackWorkCancelled())
			    return NULL;
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
			if (brlcad::PullbackWorkCancelled()) {
			    delete previous_candidate;
			    delete next_candidate;
			    return;
			}
			const ON_3dPoint alternative_common = endpoint_choice == 0 ?
			    previous_uv : next_uv;
			if (alternative_common.DistanceTo(common) <= ON_ZERO_TOLERANCE)
			    continue;
			const bool alternative_change_previous =
			    previous_uv.DistanceTo(alternative_common) > ON_ZERO_TOLERANCE;
			const bool alternative_change_next =
			    next_uv.DistanceTo(alternative_common) > ON_ZERO_TOLERANCE;
			if ((previous->m_type == ON_BrepTrim::singular &&
				alternative_change_previous) ||
				(next->m_type == ON_BrepTrim::singular &&
				 alternative_change_next))
			    continue;
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
			    if (brlcad::PullbackWorkCancelled())
				break;
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
			if (brlcad::PullbackWorkCancelled()) {
			    delete previous_candidate;
			    delete next_candidate;
			    return;
			}
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
				    &regeneration_failure, NULL, NULL, NULL, false,
				    wrapper);
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
			    loop_changed = true;
			    continue;
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
		if (previous_candidate) {
		    const ON_Surface::ISO singular_iso = previous->m_iso;
		    brep->SetTrimIsoFlags(*previous);
		    if (previous->m_type == ON_BrepTrim::singular &&
			    (singular_iso == ON_Surface::S_iso ||
			     singular_iso == ON_Surface::E_iso ||
			     singular_iso == ON_Surface::N_iso ||
			     singular_iso == ON_Surface::W_iso))
			previous->m_iso = singular_iso;
		}
		if (next_candidate) {
		    const ON_Surface::ISO singular_iso = next->m_iso;
		    brep->SetTrimIsoFlags(*next);
		    if (next->m_type == ON_BrepTrim::singular &&
			    (singular_iso == ON_Surface::S_iso ||
			     singular_iso == ON_Surface::E_iso ||
			     singular_iso == ON_Surface::N_iso ||
			     singular_iso == ON_Surface::W_iso))
			next->m_iso = singular_iso;
		}
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    "snapped adjacent pcurve endpoints within validated edge tolerance");
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id << ": snapped loop "
			<< li << " trim endpoints " << previous->m_trim_index << '/'
			<< next->m_trim_index << " within tolerance "
			<< join_tolerance << std::endl;
		changed = true;
		loop_changed = true;
		continue;
	    }
	    next_dirty[static_cast<size_t>(li)] = loop_changed;
	}
	if (!changed)
	    break;
	dirty.swap(next_dirty);
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


static bool
trim_orientation_toggle_preserves_edge_pair(const ON_Brep *brep,
	const ON_BrepTrim &trim)
{
    if (!brep || trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count())
	return false;
    const ON_BrepEdge &edge = brep->m_E[trim.m_ei];
    if (edge.m_ti.Count() != 2)
	return true;

    const ON_BrepTrim *other = NULL;
    for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
	const int ti = edge.m_ti[eti];
	if (ti < 0 || ti >= brep->m_T.Count())
	    return false;
	if (ti != trim.m_trim_index)
	    other = &brep->m_T[ti];
    }
    const ON_BrepFace *face = trim.Face();
    const ON_BrepFace *other_face = other ? other->Face() : NULL;
    if (!other || !face || !other_face)
	return false;

    /* OpenNURBS requires the two uses of every mated or seam edge to have
     * opposite effective directions.  A closed pcurve has no distinct topology
     * vertices to expose an accidental flag reversal, so a tangent-only repair
     * used to be able to corrupt an already consistent STEP edge pair.  Permit
     * a flag change only when the completed edge topology proves that it keeps
     * (or restores) the required opposite-use invariant. */
    const bool toggled_effective = (!trim.m_bRev3d) ^ face->m_bRev;
    const bool other_effective = other->m_bRev3d ^ other_face->m_bRev;
    return toggled_effective != other_effective;
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
	const bool alignment_valid[2] = {
	    closed_trim_endpoint_alignment_is_valid(alignment[0]),
	    closed_trim_endpoint_alignment_is_valid(alignment[1])
	};
	const bool negative_alignment[2] = {
	    alignment_valid[0] && alignment[0] < 0.0,
	    alignment_valid[1] && alignment[1] < 0.0
	};
	if (!negative_alignment[0] && !negative_alignment[1])
	    continue;
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id << ": final closed trim "
		<< ti << " endpoint dots=" << alignment[0] << ',' << alignment[1]
		<< std::endl;

	if (alignment_valid[0] && alignment_valid[1] &&
		alignment[0] <= -0.9 && alignment[1] <= -0.9 &&
		trim_orientation_toggle_preserves_edge_pair(brep, trim)) {
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
		    required_start, required_end, true, wrapper)) {
	    closed_trim_endpoint_alignments(trim, edge, surface, alignment);
	    const bool regenerated_alignment_valid[2] = {
		closed_trim_endpoint_alignment_is_valid(alignment[0]),
		closed_trim_endpoint_alignment_is_valid(alignment[1])
	    };
		if (regenerated_alignment_valid[0] &&
			regenerated_alignment_valid[1] &&
			alignment[0] <= -0.9 && alignment[1] <= -0.9 &&
			trim_orientation_toggle_preserves_edge_pair(brep, trim)) {
		trim.m_bRev3d = !trim.m_bRev3d;
		trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
		trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
		wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		    "corrected a regenerated closed-edge trim orientation");
	    }
	    if ((!regenerated_alignment_valid[0] || alignment[0] >= 0.0) &&
		    (!regenerated_alignment_valid[1] || alignment[1] >= 0.0))
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
repair_face_bound_classification(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return;

    /* FACE_BOUND is the base type and does not itself say inner or outer.
     * Conforming files use FACE_OUTER_BOUND for exactly one member, but some
     * production exporters emit either only FACE_BOUND or more than one
     * FACE_OUTER_BOUND.  Recover or correct the classification only when it
     * is unambiguous: a face has one loop, or openNURBS computes exactly one
     * outer loop from the completed pcurves.  No curve or topology is changed. */
    if (wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	    ON_BrepFace &face = brep->m_F[fi];
	    int outer_count = 0;
	    bool classification_complete = true;
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count()) {
		    classification_complete = false;
		    continue;
		}
		const ON_BrepLoop::TYPE type = brep->m_L[li].m_type;
		if (type == ON_BrepLoop::outer)
		    ++outer_count;
		else if (type != ON_BrepLoop::inner && type != ON_BrepLoop::slit)
		    classification_complete = false;
	    }
	    if (face.m_li.Count() == 0)
		continue;
	    if (outer_count == 1 && classification_complete) {
		const int first_li = face.m_li[0];
		if (first_li >= 0 && first_li < brep->m_L.Count() &&
			brep->m_L[first_li].m_type == ON_BrepLoop::outer)
		    continue;
		brep->SortFaceLoops(face);
		wrapper->RecordRepair(entity_id, entity_type, "face_bound",
		    "restored the outer FACE_BOUND loop to the first face-loop position");
		continue;
	    }

	    if (outer_count == 0 && face.m_li.Count() == 1) {
		const int li = face.m_li[0];
		if (li < 0 || li >= brep->m_L.Count()) continue;
		brep->m_L[li].m_type = ON_BrepLoop::outer;
		wrapper->RecordRepair(entity_id, entity_type, "face_bound",
		    "classified the only FACE_BOUND loop as the outer boundary");
		continue;
	    }

	    std::vector<ON_BrepLoop::TYPE> computed;
	    computed.reserve(face.m_li.Count());
	    int computed_outer_count = 0;
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		const ON_BrepLoop::TYPE type = li >= 0 && li < brep->m_L.Count() ?
		    brep->ComputeLoopType(brep->m_L[li]) : ON_BrepLoop::unknown;
		computed.push_back(type);
		if (type == ON_BrepLoop::outer) ++computed_outer_count;
	    }
	    if (computed_outer_count != 1)
	    {
		if (wrapper->Verbose() && face.m_li.Count() > 1) {
		    std::cerr << entity_type << " #" << entity_id
			<< ": unresolved FACE_BOUND classification F" << fi
			<< " outer-count=" << outer_count
			<< " computed-outer-count=" << computed_outer_count;
		    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
			const int li = face.m_li[fli];
			if (li < 0 || li >= brep->m_L.Count()) continue;
			const ON_BrepLoop &loop = brep->m_L[li];
			std::cerr << " L" << li << "/STEP" << loop.m_loop_user.i
			    << "=" << static_cast<int>(loop.m_type) << '/'
			    << static_cast<int>(computed[fli]) << "(trims="
			    << loop.TrimCount() << ')';
		    }
		    std::cerr << std::endl;
		}
		continue;
	    }
	    if (wrapper->Verbose() && face.m_li.Count() > 1) {
		std::cerr << entity_type << " #" << entity_id
		    << ": resolved FACE_BOUND classification F" << fi;
		for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		    const int li = face.m_li[fli];
		    if (li < 0 || li >= brep->m_L.Count()) continue;
		    const ON_BrepLoop &loop = brep->m_L[li];
		    std::cerr << " L" << li << "/STEP" << loop.m_loop_user.i
			<< '=' << static_cast<int>(loop.m_type) << '/'
			<< static_cast<int>(computed[fli]) << "(trims="
			<< loop.TrimCount() << ')';
		}
		std::cerr << std::endl;
	    }
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li >= 0 && li < brep->m_L.Count() &&
			(computed[fli] == ON_BrepLoop::outer ||
			 computed[fli] == ON_BrepLoop::inner ||
			 computed[fli] == ON_BrepLoop::slit))
		    brep->m_L[li].m_type = computed[fli];
	    }
	    brep->SortFaceLoops(face);
	    wrapper->RecordRepair(entity_id, entity_type, "face_bound",
		outer_count == 0 ?
		"classified untyped FACE_BOUND loops from their exact pcurves" :
		outer_count > 1 ?
		"corrected multiple FACE_OUTER_BOUND loops from their exact pcurves" :
		"completed FACE_BOUND loop classification from exact pcurves");
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

    const auto report_stage = [wrapper, entity_id, &entity_type](
	const char *phase) {
	wrapper->SetProgressDetail(phase, entity_id, 0, 0, std::string(),
	    entity_type);
    };

    /* STEP ORIENTED_EDGE and ADVANCED_FACE senses are authoritative shell
     * topology.  Some exact periodic-band constructions must reverse a UV
     * boundary while joining formerly separate loops.  For a closed edge the
     * vertex sequence cannot reveal that this also toggled its 3-D edge use,
     * so retain the original effective sense by STEP edge identity and the
     * stable BREP face index created from the STEP face sequence.  A
     * same-face seam pair has both senses under one key and is deliberately
     * left to the seam solver. */
    typedef std::pair<int, int> StepClosedEdgeUseKey;
    std::map<StepClosedEdgeUseKey, std::set<bool> > expected_closed_edge_uses;
    report_stage("indexing authoritative closed STEP edge uses");
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	const ON_BrepTrim &trim = brep->m_T[ti];
	const ON_BrepEdge *edge = trim.Edge();
	const ON_BrepFace *face = trim.Face();
	if (!edge || !face || edge->m_vi[0] != edge->m_vi[1] ||
		trim.m_vi[0] != trim.m_vi[1] || edge->m_edge_user.i <= 0 ||
		face->m_face_index < 0)
	    continue;
	expected_closed_edge_uses[StepClosedEdgeUseKey(edge->m_edge_user.i,
	    face->m_face_index)].insert(trim.m_bRev3d ^ face->m_bRev);
    }

    /* Moving a periodic surface seam is exact, but it can disturb an already
     * valid choice of parameter branches elsewhere on the same surface.  Keep
     * it out of the ordinary repair pass and retry with seam alignment only
     * when the bounded repairs below still leave structurally invalid
     * topology. */
    report_stage("repairing implicit exact periodic face bands");
    repair_implicit_periodic_face_bands(brep, wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled())
	return;
    report_stage("classifying exact STEP face bounds");
    repair_face_bound_classification(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled())
	return;

    std::vector<ON_BrepLoop::TYPE> expected_loop_types;
    expected_loop_types.reserve(brep->m_L.Count());
    report_stage("capturing exact BREP loop classifications");
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	if ((li & 63) == 0 && brlcad::PullbackWorkCancelled())
	    return;
	expected_loop_types.push_back(brep->m_L[li].m_type);
    }
    std::vector<int> aligned_surface_loops;
    report_stage("repairing bounded periodic seam classifications");
    repair_bounded_seam_isos(brep, wrapper, entity_id, entity_type,
	allow_surface_alignment, &aligned_surface_loops);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing adjacent exact BREP topology vertices");
    repair_adjacent_trim_vertices(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing invalid exact open pcurves");
    repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing ordinary closed-surface seam crossings");
    if (repair_ordinary_closed_nurbs_seam_crossings(brep, wrapper, entity_id,
	    entity_type))
	repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing paired exact seam boundaries");
    repair_paired_seam_boundaries(brep, wrapper, entity_id, entity_type,
	&aligned_surface_loops);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing missing exact singular trims");
    repair_missing_singular_trims(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("solving coherent exact periodic loop branches");
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("regenerating exact periodic loop chains");
    regenerate_periodic_loop_chains(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("resolving regenerated exact periodic loop branches");
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing zero-length exact boundary edges");
    repair_zero_length_boundary_edges(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    /* Pole cuts are ordinary paired seams even when their surface seam was
     * aligned during initial trim construction rather than this repair pass.
     * Include every seam loop which still has a model-space exact p-space gap
     * in the cyclic branch solve; restricting this to freshly aligned surfaces
     * leaves otherwise valid pole loops one whole period open. */
    const auto periodic_branch_loops = [brep, &aligned_surface_loops]() {
	std::set<int> selected(aligned_surface_loops.begin(),
	    aligned_surface_loops.end());
	for (int li = 0; li < brep->m_L.Count(); ++li) {
	    const ON_BrepLoop &loop = brep->m_L[li];
	    const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	    if (!surface || loop.TrimCount() < 2)
		continue;
	    bool has_seam = false;
	    bool exact_parameter_gap = false;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *current = loop.Trim(lti);
		const ON_BrepTrim *next = loop.Trim((lti + 1) % loop.TrimCount());
		if (!current || !next)
		    continue;
		has_seam = has_seam || current->m_type == ON_BrepTrim::seam ||
		    next->m_type == ON_BrepTrim::seam;
		const ON_3dPoint current_end = current->PointAtEnd();
		const ON_3dPoint next_start = next->PointAtStart();
		if (current_end.DistanceTo(next_start) <= ON_ZERO_TOLERANCE)
		    continue;
		const ON_3dPoint current_lift = surface->PointAt(
		    current_end.x, current_end.y);
		const ON_3dPoint next_lift = surface->PointAt(
		    next_start.x, next_start.y);
		if (current_lift.IsValid() && next_lift.IsValid() &&
			current_lift.DistanceTo(next_lift) <= LocalUnits::tolerance)
		    exact_parameter_gap = true;
	    }
	    if (has_seam && exact_parameter_gap)
		selected.insert(li);
	}
	return std::vector<int>(selected.begin(), selected.end());
    };
    std::vector<int> branch_loops = periodic_branch_loops();
    if (!branch_loops.empty()) {
	report_stage("solving exact paired-seam loop branches");
	repair_aligned_surface_loop_branches(brep, wrapper, entity_id, entity_type,
	    branch_loops);
	repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
    }

    /* A surface-alignment retry starts from the completed bounded repairs.
     * Relocating a periodic seam translates pcurves without changing their
     * 3-D lifts or tangent direction, so the dense closed-edge orientation
     * proof remains valid and must not be repeated. */
    if (!allow_surface_alignment) {
    report_stage("validating exact closed-edge trim orientations");
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	if ((ti & 63) == 0 && brlcad::PullbackWorkCancelled())
	    return;
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
	    if (brlcad::PullbackWorkCancelled())
		return;
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
	    /* The trim/edge use supplies a directed parameter correspondence.  If
	     * its exact edge point already proves locus membership, a global NURBS
	     * closest-point solve cannot improve the acceptance result.  Retain the
	     * optimizer only for legitimately reparameterized pcurves whose direct
	     * correspondence falls outside the model tolerance. */
	    double edge_parameter = edge_nurbs.Domain().ParameterAt(
		trim.m_bRev3d ? 1.0 - normalized : normalized);
	    double edge_distance = lifted_point.DistanceTo(
		edge_nurbs.PointAt(edge_parameter));
	    /* This pass only gathers evidence for changing orientation.  A global
	     * NURBS closest-point solve here used to run as many as fifteen times
	     * per closed trim and has no bounded/cancellable openNURBS interface.
	     * When the directed STEP correspondence is outside tolerance, treating
	     * the sample as inconclusive is both safer and substantially cheaper:
	     * endpoint proof and the densely validated regeneration path below
	     * remain available, while an unrelated closest locus cannot be used as
	     * evidence for reversing an edge use. */
	    if (edge_distance > LocalUnits::tolerance)
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
	const bool endpoint_alignment_valid[2] = {
	    closed_trim_endpoint_alignment_is_valid(endpoint_alignment[0]),
	    closed_trim_endpoint_alignment_is_valid(endpoint_alignment[1])
	};
	const bool negative_endpoint_alignment[2] = {
	    endpoint_alignment_valid[0] && endpoint_alignment[0] < 0.0,
	    endpoint_alignment_valid[1] && endpoint_alignment[1] < 0.0
	};
	if (negative_endpoint_alignment[0] && negative_endpoint_alignment[1] &&
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
		if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
		    delete reversed;
		    return;
		}
		const double fraction = static_cast<double>(sample) /
		    kDenseValidationSegments;
		const ON_3dPoint uv = reversed->PointAt(
		    reversed_domain.ParameterAt(fraction));
		const ON_3dPoint lifted = surface->PointAt(uv.x, uv.y);
		valid_reversal = lifted.IsValid();
		if (valid_reversal) {
		    const ON_Interval edge_domain = edge_nurbs.Domain();
		    const ON_3dPoint forward = edge_nurbs.PointAt(
			edge_domain.ParameterAt(fraction));
		    const ON_3dPoint reverse = edge_nurbs.PointAt(
			edge_domain.ParameterAt(1.0 - fraction));
		    /* Reversal validation needs geometric-locus membership, not a
		     * particular parameterization.  Either directed correspondence is
		     * a sufficient exact-edge witness; consult the global optimizer only
		     * when both direct witnesses fail. */
		    valid_reversal = std::min(lifted.DistanceTo(forward),
			lifted.DistanceTo(reverse)) <= LocalUnits::tolerance;
		}
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
	if (endpoint_alignment_valid[0] && endpoint_alignment_valid[1] &&
		endpoint_alignment[0] < -0.9 && endpoint_alignment[1] < -0.9 &&
		trim_orientation_toggle_preserves_edge_pair(brep, trim)) {
	    trim.m_bRev3d = !trim.m_bRev3d;
	    trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
	    trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"corrected a closed-edge trim orientation proven at both endpoints");
	    continue;
	}
	/* A tangent at a collapsed pole can oppose the well-conditioned interior
	 * solely because the surface derivative is singular there.  Do not launch
	 * the expensive dense regeneration when at least five interior samples all
	 * prove the existing orientation.  Regeneration remains mandatory for a
	 * negative endpoint accompanied by mixed, opposing, or insufficient
	 * interior evidence. */
	const bool interior_proves_current_orientation =
	    matching_samples >= 5 && opposing_samples == 0;
	std::string regeneration_failure;
	if ((negative_endpoint_alignment[0] || negative_endpoint_alignment[1]) &&
		!interior_proves_current_orientation &&
		regenerate_trim_polyline(brep, trim, surface, edge_nurbs,
		    LocalUnits::tolerance, &regeneration_failure, NULL, NULL, NULL,
		    false, wrapper)) {
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
	if (wrapper->Verbose() && (negative_endpoint_alignment[0] ||
		negative_endpoint_alignment[1] || opposing_samples > 0 ||
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
	if (opposing_samples >= 5 && matching_samples == 0 &&
		trim_orientation_toggle_preserves_edge_pair(brep, trim)) {
	    trim.m_bRev3d = !trim.m_bRev3d;
	    trim.m_vi[0] = edge.m_vi[trim.m_bRev3d ? 1 : 0];
	    trim.m_vi[1] = edge.m_vi[trim.m_bRev3d ? 0 : 1];
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"corrected closed-edge trim orientation");
	}
    }
    }

    /* Closed-edge regeneration above can replace a pcurve after the ordinary
     * endpoint pass.  Reclose only the newly exposed, model-space exact joins
     * before recomputing derived topology state. */
    report_stage("reclosing joins after exact closed-edge repair");
    repair_missing_singular_trims(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    branch_loops = periodic_branch_loops();
    if (!branch_loops.empty())
	repair_aligned_surface_loop_branches(brep, wrapper, entity_id, entity_type,
	    branch_loops);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    /* Surface-seam alignment and whole-period loop unwrapping can expose a
     * seam pair that was valid when first installed but no longer agrees with
     * the final shared surface parameterization.  Reconcile exact seam pairs
     * once more without permitting another surface seam move, then re-run the
     * bounded open-curve and join checks against that stable surface. */
    report_stage("reconciling final exact periodic seam pairs");
    repair_seam_pair_from_exact_edge(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_paired_seam_boundaries(brep, wrapper, entity_id, entity_type, NULL);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    regenerate_periodic_loop_chains(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    report_stage("refreshing exact OpenNURBS trim topology");
	/* Curve replacement and orientation repair invalidate derived p-space
	 * state.  Refresh it once, after all bounded repairs, using the same
	 * routines ON_Brep::IsValid() uses.  Do not call ON_Brep::FlipLoop merely
	 * because ComputeLoopType() changes: FlipLoop also reverses loop order,
	 * every trim pcurve, and every m_bRev3d edge use.  That would replace the
	 * authoritative STEP oriented-edge topology with a derived 2-D area
	 * classification and can make an otherwise consistent closed shell
	 * non-orientable. */
    refresh_brep_flags_preserving_singular_isos(brep, true);
    if (brlcad::PullbackWorkCancelled()) return;
	/* Reversing a p-space loop also toggles every trim's 3-D edge use.  It is
	 * appropriate when exact periodic reconstruction reversed a complete loop,
	 * but unsafe when only ComputeLoopType() chose a different periodic branch.
	 * Predict the completed shell consequence and accept a reversal only when
	 * it does not increase the number of shared-edge orientation conflicts. */
	/* Only edge uses belonging to the candidate loop can change when that
	 * loop is flipped.  The former implementation recomputed the complete
	 * shell and closed-STEP-use conflict counts before and after every
	 * candidate, making this pass quadratic in the number of loops.  Large
	 * imported assemblies can contain tens of thousands of loops in one
	 * representation.  Compare the affected edge/trim subset instead; the
	 * unchanged remainder contributes the same value to both totals. */
	const auto loop_shell_orientation_conflicts = [brep](int toggled_loop,
		size_t &before, size_t &after) {
	    before = 0;
	    after = 0;
	    if (toggled_loop < 0 || toggled_loop >= brep->m_L.Count())
		return;
	    std::set<int> affected_edges;
	    const ON_BrepLoop &loop = brep->m_L[toggled_loop];
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		if (trim && trim->m_ei >= 0)
		    affected_edges.insert(trim->m_ei);
	    }
	    for (std::set<int>::const_iterator eit = affected_edges.begin();
		    eit != affected_edges.end(); ++eit) {
		if (*eit < 0 || *eit >= brep->m_E.Count())
		    continue;
		const ON_BrepEdge &edge = brep->m_E[*eit];
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
		before += first_effective == second_effective;
		const bool first_after = first->m_li == toggled_loop ?
		    !first_effective : first_effective;
		const bool second_after = second->m_li == toggled_loop ?
		    !second_effective : second_effective;
		after += first_after == second_after;
	    }
	};
	const auto loop_closed_step_use_conflicts = [brep,
		&expected_closed_edge_uses](int toggled_loop, size_t &before,
		    size_t &after) {
	    before = 0;
	    after = 0;
	    if (toggled_loop < 0 || toggled_loop >= brep->m_L.Count())
		return;
	    const ON_BrepLoop &loop = brep->m_L[toggled_loop];
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		const ON_BrepFace *face = trim ? trim->Face() : NULL;
		if (!trim || !edge || !face || edge->m_vi[0] != edge->m_vi[1] ||
			trim->m_vi[0] != trim->m_vi[1] ||
			edge->m_edge_user.i <= 0 || face->m_face_index < 0)
		    continue;
		const std::map<StepClosedEdgeUseKey, std::set<bool> >::const_iterator
		    expected = expected_closed_edge_uses.find(StepClosedEdgeUseKey(
			edge->m_edge_user.i, face->m_face_index));
		if (expected == expected_closed_edge_uses.end() ||
			expected->second.size() != 1)
		    continue;
		const bool effective = trim->m_bRev3d ^ face->m_bRev;
		before += effective != *expected->second.begin();
		after += !effective != *expected->second.begin();
	    }
	};
	bool loop_orientation_changed = false;
	for (int li = 0; li < brep->m_L.Count() &&
		static_cast<size_t>(li) < expected_loop_types.size(); ++li) {
	    if ((li & 63) == 0 && brlcad::PullbackWorkCancelled())
		return;
	    const ON_BrepLoop::TYPE expected = expected_loop_types[li];
	    ON_BrepLoop &loop = brep->m_L[li];
	    if ((expected != ON_BrepLoop::outer &&
		    expected != ON_BrepLoop::inner) ||
		    (loop.m_type != ON_BrepLoop::outer &&
		     loop.m_type != ON_BrepLoop::inner) || expected == loop.m_type)
		continue;
	    size_t before = 0;
	    size_t after = 0;
	    size_t step_before = 0;
	    size_t step_after = 0;
	    loop_shell_orientation_conflicts(li, before, after);
	    loop_closed_step_use_conflicts(li, step_before, step_after);
	    if (after > before || step_after > step_before) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": declined loop " << li << "/STEP"
			<< loop.m_loop_user.i << " orientation reversal because "
			<< "shared-edge conflicts would change " << before << "->"
			<< after << " and authoritative closed STEP edge-use "
			<< "conflicts " << step_before << "->" << step_after
			<< std::endl;
		continue;
	    }
	    brep->FlipLoop(loop);
	    loop_orientation_changed = true;
	    wrapper->RecordRepair(entity_id, entity_type, "loop_orientation",
		"restored a periodic loop orientation without increasing closed-shell edge-use conflicts");
	}
	if (loop_orientation_changed)
	    refresh_brep_flags_preserving_singular_isos(brep, true);
	/* The refresh above may still report an unknown loop type on a periodic
	 * face, or a different winding from a periodic branch choice.  Preserve
	 * the authoritative FACE_OUTER_BOUND/FACE_BOUND classification (or the
	 * unambiguous pre-repair classification recovered above), including slit
	 * loops proven by ComputeLoopType(); derived loop flags are not allowed to
	 * erase it. */
    for (int li = 0; li < brep->m_L.Count() &&
	static_cast<size_t>(li) < expected_loop_types.size(); ++li) {
	if ((li & 63) == 0 && brlcad::PullbackWorkCancelled())
	    return;
	const ON_BrepLoop::TYPE expected = expected_loop_types[li];
	if (expected == ON_BrepLoop::outer || expected == ON_BrepLoop::inner ||
		expected == ON_BrepLoop::slit)
	    brep->m_L[li].m_type = expected;
	}
	for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	    if ((fi & 63) == 0 && brlcad::PullbackWorkCancelled())
		return;
	    brep->SortFaceLoops(brep->m_F[fi]);
	}
	/* Endpoint repair can alter the endpoint tangent of a closed pcurve.
	 * Perform the final exact tangent check after all such repairs, then
	 * refresh the derived trim state once more. */
	if (!allow_surface_alignment)
	    repair_final_closed_trim_orientations(brep, wrapper, entity_id,
		entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
	size_t restored_step_edge_uses = 0;
	for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	    ON_BrepTrim &trim = brep->m_T[ti];
	    const ON_BrepEdge *edge = trim.Edge();
	    const ON_BrepFace *face = trim.Face();
	    if (!edge || !face || edge->m_vi[0] != edge->m_vi[1] ||
		    trim.m_vi[0] != trim.m_vi[1] || edge->m_edge_user.i <= 0 ||
		    face->m_face_index < 0)
		continue;
	    const std::map<StepClosedEdgeUseKey, std::set<bool> >::const_iterator
		expected = expected_closed_edge_uses.find(StepClosedEdgeUseKey(
		    edge->m_edge_user.i, face->m_face_index));
	    if (expected == expected_closed_edge_uses.end() ||
		    expected->second.size() != 1)
		continue;
	    const bool desired = *expected->second.begin();
	    if ((trim.m_bRev3d ^ face->m_bRev) == desired)
		continue;

	    /* A bounded repair may have regenerated a closed pcurve in the
	     * direction which makes its lifted tangents agree with the 3-D edge,
	     * then corrected m_bRev3d accordingly.  Restoring the authoritative
	     * STEP edge-use sense by changing only m_bRev3d would make those
	     * tangents oppose the edge and fails ON_Brep::IsValid().  Closed trim
	     * endpoints cannot distinguish the two directions, so use the same
	     * endpoint-tangent proof as openNURBS and reverse the exact pcurve
	     * together with the flag when necessary.  Reversal preserves the curve
	     * locus; the coincident endpoints preserve loop connectivity. */
	    double alignment[2];
	    closed_trim_endpoint_alignments(trim, *edge, face->SurfaceOf(),
		alignment);
	    const bool toggle_would_oppose =
		closed_trim_endpoint_alignment_is_valid(alignment[0]) &&
		closed_trim_endpoint_alignment_is_valid(alignment[1]) &&
		alignment[0] > 0.0 && alignment[1] > 0.0;
	    if (toggle_would_oppose) {
		ON_Curve *reversed = trim.DuplicateCurve();
		const ON_3dPoint original_start = trim.PointAtStart();
		const ON_3dPoint original_end = trim.PointAtEnd();
		if (!reversed || !reversed->Reverse() ||
			!reversed->ChangeDimension(2) || !reversed->IsValid() ||
			reversed->PointAtStart().DistanceTo(original_start) >
			    ON_ZERO_TOLERANCE ||
			reversed->PointAtEnd().DistanceTo(original_end) >
			    ON_ZERO_TOLERANCE) {
		    delete reversed;
		    continue;
		}
		const int c2_index = brep->AddTrimCurve(reversed);
		if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
		    if (c2_index < 0)
			delete reversed;
		    continue;
		}
		brep->SetTrimIsoFlags(trim);
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "reversed a regenerated closed pcurve while restoring its authoritative STEP edge-use sense");
	    }
	    trim.m_bRev3d = !trim.m_bRev3d;
	    ++restored_step_edge_uses;
	}
	for (size_t restored = 0; restored < restored_step_edge_uses; ++restored)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"restored an authoritative closed STEP edge-use orientation after periodic loop repair");
	/* The completed STEP face-bound graph is authoritative when an exporter
	 * omitted FACE_OUTER_BOUND.  The final closed-edge tangent pass does not
	 * reverse pcurves, so refreshing loop types here would only discard the
	 * unambiguous bound classification restored above. */
	refresh_brep_flags_preserving_singular_isos(brep, false);
	if (!allow_surface_alignment) {
	    ON_wString validation_messages;
	    ON_TextLog validation_log(validation_messages);
	    if (!brep->IsValid(&validation_log)) {
		bool unresolved_open_surface_join = false;
		for (int li = 0; li < brep->m_L.Count() &&
			!unresolved_open_surface_join; ++li) {
		    const ON_BrepLoop &loop = brep->m_L[li];
		    const ON_Surface *surface = loop.Face() ?
			loop.Face()->SurfaceOf() : NULL;
		    if (!surface)
			continue;
		    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
			const ON_BrepTrim *current = loop.Trim(lti);
			const ON_BrepTrim *next = loop.Trim(
			    (lti + 1) % loop.TrimCount());
			if (!current || !next) continue;
			const ON_3dPoint current_end = current->PointAtEnd();
			const ON_3dPoint next_start = next->PointAtStart();
			for (int direction = 0; direction < 2; ++direction) {
			    if (fabs(current_end[direction] - next_start[direction]) >
				    ON_ZERO_TOLERANCE && !surface->IsClosed(direction)) {
				unresolved_open_surface_join = true;
				break;
			    }
			}
			if (unresolved_open_surface_join) break;
		    }
		}
		if (wrapper->Verbose()) {
		    ON_String validation_text(validation_messages);
		    std::cerr << entity_type << " #" << entity_id
			<< ": bounded repair pass remained structurally invalid before "
			   "periodic surface alignment retry:\n"
			<< validation_text.Array();
		}
		/* A periodic surface seam move cannot alter an unresolved join on an
		 * open surface.  Repeating every repair on a full BREP in that case is
		 * both deterministic wasted work and can consume the entire item
		 * budget before the actionable open-surface diagnostic is emitted. */
		if (unresolved_open_surface_join) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": skipping periodic surface alignment retry because an "
			       "open-surface p-space join remains unresolved" << std::endl;
		    return;
		}
		std::unique_ptr<ON_Brep> bounded_repairs(new ON_Brep(*brep));
		/* Apply seam relocation to the already completed bounded repairs.
		 * Restarting from retry_source repeated every dense regeneration and
		 * routinely exhausted an item's budget before validation. */
		repair_closed_trim_orientations(brep, wrapper, entity_id,
		    entity_type, true);
		ON_wString aligned_messages;
		ON_TextLog aligned_log(aligned_messages);
	if (brep->IsValid(&aligned_log)) {
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"selected a valid retry with exact periodic surface seam alignment");
	    return;
	}
	if (wrapper->Verbose()) {
	    ON_String aligned_text(aligned_messages);
	    std::cerr << entity_type << " #" << entity_id
		<< ": periodic surface seam retry validation failed:\n"
		<< aligned_text.Array();
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


struct TopologicalKeyholeSplit {
    int original_loop = -1;
    int detached_loop = -1;
    int face = -1;
};


/* STEP permits an edge to be used forward and backward as a zero-area bridge
 * from an outer boundary to another closed boundary component.  OpenNURBS
 * calls two uses of one edge in one loop a seam and consequently requires
 * native N/E/S/W pcurves.  For a non-isoparametric bridge, its own source-edge
 * identity proves exact cancellation; split the two remaining topology cycles
 * and discard only the opposite bridge uses.  This helper is intentionally
 * candidate-only: the caller retains the result only if bounded pcurve repair
 * and complete OpenNURBS structural validation subsequently succeed. */
static bool
split_topological_keyhole_candidate(ON_Brep *brep,
    TopologicalKeyholeSplit *split)
{
    if (split)
	*split = TopologicalKeyholeSplit();
    if (!brep || !split)
	return false;

    const auto boundary_iso = [](ON_Surface::ISO iso) {
	return iso == ON_Surface::W_iso || iso == ON_Surface::E_iso ||
	    iso == ON_Surface::S_iso || iso == ON_Surface::N_iso;
    };
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	ON_BrepLoop *loop = &brep->m_L[li];
	ON_BrepFace *face = loop->Face();
	const int trim_count = loop->TrimCount();
	/* Restrict the transactional retry to one source outer loop.  Assigning
	 * pre-existing holes between two newly separated faces would require a
	 * containment decision outside this exact topology normalization. */
	if (!face || face->LoopCount() != 1 ||
		loop->m_type != ON_BrepLoop::outer || trim_count < 4)
	    continue;
	for (int first_offset = 0; first_offset < trim_count; ++first_offset) {
	    ON_BrepTrim *first = loop->Trim(first_offset);
	    if (!first || first->m_type != ON_BrepTrim::seam ||
		    boundary_iso(first->m_iso) || first->m_ei < 0 ||
		    first->m_ei >= brep->m_E.Count())
		continue;
	    const ON_BrepEdge &edge = brep->m_E[first->m_ei];
	    if (edge.m_ti.Count() != 2)
		continue;
	    for (int second_offset = first_offset + 2;
		    second_offset < trim_count; ++second_offset) {
		if (first_offset == 0 && second_offset == trim_count - 1)
		    continue;
		ON_BrepTrim *second = loop->Trim(second_offset);
		if (!second || second->m_type != ON_BrepTrim::seam ||
			second->m_ei != first->m_ei ||
			second->m_bRev3d == first->m_bRev3d)
		    continue;

		std::vector<int> inside;
		std::vector<int> outside;
		for (int offset = first_offset + 1; offset < second_offset; ++offset)
		    inside.push_back(loop->m_ti[offset]);
		for (int offset = second_offset + 1; offset < trim_count; ++offset)
		    outside.push_back(loop->m_ti[offset]);
		for (int offset = 0; offset < first_offset; ++offset)
		    outside.push_back(loop->m_ti[offset]);
		if (inside.empty() || outside.empty())
		    continue;
		/* The shared edge supplies the bridge endpoints.  Each exposed chain
		 * must already close at the same immutable topology vertex; only its
		 * p-space image is eligible for the bounded repair retry. */
		if (brep->m_T[inside.back()].m_vi[1] !=
			brep->m_T[inside.front()].m_vi[0] ||
			brep->m_T[outside.back()].m_vi[1] !=
			brep->m_T[outside.front()].m_vi[0])
		    continue;

		const int face_index = face->m_face_index;
		const int first_trim_index = first->m_trim_index;
		const int second_trim_index = second->m_trim_index;
		const int detached_loop_index =
		    brep->NewLoop(ON_BrepLoop::unknown).m_loop_index;
		loop = &brep->m_L[li];
		ON_BrepLoop *detached = &brep->m_L[detached_loop_index];
		loop->m_ti.SetCount(0);
		for (std::vector<int>::const_iterator trim = outside.begin();
			trim != outside.end(); ++trim) {
		    loop->m_ti.Append(*trim);
		    brep->m_T[*trim].m_li = li;
		}
		for (std::vector<int>::const_iterator trim = inside.begin();
			trim != inside.end(); ++trim) {
		    detached->m_ti.Append(*trim);
		    brep->m_T[*trim].m_li = detached_loop_index;
		}
		/* Preserve the authoritative FACE_OUTER_BOUND classification on the
		 * source chain.  The detached chain has no independent STEP bound and
		 * must earn its type from its repaired pcurves. */
		loop->m_type = ON_BrepLoop::outer;
		loop->m_fi = face_index;
		detached->m_type = ON_BrepLoop::unknown;
		detached->m_fi = face_index;
		brep->m_F[face_index].m_li.Append(detached_loop_index);

		brep->m_T[first_trim_index].m_li = -1;
		brep->m_T[second_trim_index].m_li = -1;
		const int higher_trim = std::max(first_trim_index, second_trim_index);
		const int lower_trim = std::min(first_trim_index, second_trim_index);
		brep->DeleteTrim(brep->m_T[higher_trim], false);
		brep->DeleteTrim(brep->m_T[lower_trim], true);
		if (!brep->Compact())
		    return false;
		split->original_loop = li;
		split->detached_loop = detached_loop_index;
		split->face = face_index;
		return true;
	    }
	}
    }
    return false;
}


static bool
retry_topological_keyhole_normalization(ON_Brep *brep, STEPWrapper *wrapper,
    int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return false;
    std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
    std::vector<TopologicalKeyholeSplit> splits;
    for (int attempt = 0; attempt < 32; ++attempt) {
	refresh_brep_flags_preserving_singular_isos(candidate.get(), false);
	TopologicalKeyholeSplit split;
	if (!split_topological_keyhole_candidate(candidate.get(), &split))
	    break;
	splits.push_back(split);
    }
    if (splits.empty()) {
	if (wrapper->Verbose()) {
	    for (int ti = 0; ti < candidate->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = candidate->m_T[ti];
		if (trim.m_type != ON_BrepTrim::seam ||
			trim.m_iso == ON_Surface::W_iso ||
			trim.m_iso == ON_Surface::E_iso ||
			trim.m_iso == ON_Surface::S_iso ||
			trim.m_iso == ON_Surface::N_iso || trim.m_li < 0 ||
			trim.m_li >= candidate->m_L.Count())
		    continue;
		const ON_BrepLoop &loop = candidate->m_L[trim.m_li];
		const ON_BrepFace *face = loop.Face();
		std::cerr << entity_type << " #" << entity_id
		    << ": topological keyhole candidate trim " << ti
		    << " not selected (loop " << trim.m_li << " type "
		    << static_cast<int>(loop.m_type) << " trims "
		    << loop.TrimCount() << ", face loops "
		    << (face ? face->LoopCount() : 0) << ", edge uses "
		    << ((trim.m_ei >= 0 && trim.m_ei < candidate->m_E.Count()) ?
			candidate->m_E[trim.m_ei].m_ti.Count() : 0) << ')'
		    << std::endl;
		break;
	    }
	}
	return false;
    }
    const auto reject = [wrapper, entity_id, &entity_type](const char *reason) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": topological keyhole normalization retry rejected: "
		<< reason << std::endl;
	return false;
    };

    repair_closed_trim_orientations(candidate.get(), wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled())
	return reject("the item work budget expired");
    /* The repair pass has already computed the detached loop type and restored
     * the authoritative source FACE_OUTER_BOUND type.  Refreshing loop types
     * again here would erase that source classification on a periodic face. */
    refresh_brep_flags_preserving_singular_isos(candidate.get(), false);

    for (std::vector<TopologicalKeyholeSplit>::const_iterator split =
	    splits.begin(); split != splits.end(); ++split) {
	if (split->original_loop < 0 ||
		split->original_loop >= candidate->m_L.Count() ||
		split->detached_loop < 0 ||
		split->detached_loop >= candidate->m_L.Count() ||
		split->face < 0 || split->face >= candidate->m_F.Count())
	    return reject("a split topology index changed unexpectedly");
	ON_BrepLoop &original = candidate->m_L[split->original_loop];
	ON_BrepLoop &detached = candidate->m_L[split->detached_loop];
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": topological keyhole split loops "
		<< split->original_loop << '/' << split->detached_loop
		<< " acquired types " << static_cast<int>(original.m_type) << '/'
		<< static_cast<int>(detached.m_type) << std::endl;
	if ((original.m_type != ON_BrepLoop::outer &&
		original.m_type != ON_BrepLoop::inner) ||
		(detached.m_type != ON_BrepLoop::outer &&
		 detached.m_type != ON_BrepLoop::inner))
	    return reject("a separated boundary did not acquire an outer/inner type");
	if (original.m_type == ON_BrepLoop::outer &&
		detached.m_type == ON_BrepLoop::outer) {
	    ON_BrepFace &source_face = candidate->m_F[split->face];
	    int detached_offset = -1;
	    for (int fli = 0; fli < source_face.m_li.Count(); ++fli)
		if (source_face.m_li[fli] == split->detached_loop) {
		    detached_offset = fli;
		    break;
		}
	    if (detached_offset < 0)
		return reject("the detached loop was absent from its source face");
	    const int surface_index = source_face.m_si;
	    const bool reversed = source_face.m_bRev;
	    source_face.m_li.Remove(detached_offset);
	    ON_BrepFace &new_face = candidate->NewFace(surface_index);
	    new_face.m_bRev = reversed;
	    new_face.m_li.Append(split->detached_loop);
	    candidate->m_L[split->detached_loop].m_fi = new_face.m_face_index;
	} else if (original.m_type != ON_BrepLoop::outer &&
		detached.m_type != ON_BrepLoop::outer) {
	    return reject("neither separated boundary was an outer loop");
	}
    }
    for (int fi = 0; fi < candidate->m_F.Count(); ++fi)
	candidate->SortFaceLoops(candidate->m_F[fi]);
    refresh_brep_flags_preserving_singular_isos(candidate.get(), false);
    repair_face_bound_classification(candidate.get(), wrapper, entity_id,
	entity_type);

    ON_wString candidate_messages;
    ON_TextLog candidate_log(candidate_messages);
    if (!candidate->IsValid(&candidate_log)) {
	if (wrapper->Verbose()) {
	    ON_String text(candidate_messages);
	    std::cerr << entity_type << " #" << entity_id
		<< ": topological keyhole normalization retry rejected:\n"
		<< text.Array();
	}
	return reject("the structurally complete separated BREP remained invalid");
    }
    *brep = *candidate;
    for (size_t split = 0; split < splits.size(); ++split)
	wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	    "removed an exact opposite-use STEP keyhole bridge after validated topology separation");
    return true;
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


static size_t repair_closed_shell_face_orientations(ON_Brep *brep,
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
    PullbackWorkScope item_work(wrapper, kMaximumExactPullbackMilliseconds);

	/* Keep this phase separate from topology detachment and pullback.  Large
	 * dependency closures can otherwise make a long materialization look like
	 * an expensive curve-on-surface solve in the periodic telemetry. */
    wrapper->SetProgressDetail("materializing STEP dependency closure",
	job.entity_id, 0, 0, std::string(), job.entity_type);
    SDAI_Application_instance *source = wrapper->getEntity(job.entity_id);
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
		if (job.topology_face_count > kComplexExactSolidFaceThreshold) {
		    job.work_budget_milliseconds = std::min(
			kMaximumComplexExactPullbackMilliseconds,
			kComplexExactOverheadMilliseconds +
			static_cast<uint64_t>(job.topology_face_count) *
			kExactPullbackMillisecondsPerFace);
		    wrapper->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Information,
			job.entity_id, job.entity_type, "work_budget",
			"extended exact-item work budget to " +
			std::to_string(job.work_budget_milliseconds / 1000) +
			" seconds for " + std::to_string(job.topology_face_count) +
			" topology faces");
		}
	    }
	} else if (job.surface_model) {
	    if (job.original_name.empty())
		job.original_name = job.surface_model->Name();
	    job.entity_type = "SHELL_BASED_SURFACE_MODEL";
	    job.work_budget_milliseconds =
		kMaximumSurfaceModelPullbackMilliseconds;
	} else if (job.face_set) {
	    job.topology_face_count = job.face_set->FaceCount();
	    if (job.entity_type.empty())
		job.entity_type = wrapper->LazyTypeName(job.entity_id);
	    if (job.topology_face_count > kComplexExactSolidFaceThreshold) {
		job.work_budget_milliseconds = std::min(
		    kMaximumComplexExactPullbackMilliseconds,
		    kComplexExactOverheadMilliseconds +
		    static_cast<uint64_t>(job.topology_face_count) *
		    kExactPullbackMillisecondsPerFace);
	    }
	} else if (job.face) {
	    job.topology_face_count = 1;
	    if (job.entity_type.empty())
		job.entity_type = wrapper->LazyTypeName(job.entity_id);
	}
    }
    brlcad::PullbackWorkCancelled();
    if (item_work.DeadlineExpired()) {
	job.validation_message =
	    "dependency materialization exceeded the 30-second per-item work budget";
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    job.entity_id, job.entity_type, "dependency_closure", job.validation_message);
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
    job.ready = (job.solid != NULL || job.surface_model != NULL ||
	job.face_set != NULL || job.face != NULL) &&
	job.entity_arena != NULL;
    const uint64_t elapsed = static_cast<uint64_t>(
	std::chrono::duration_cast<std::chrono::milliseconds>(
	    std::chrono::steady_clock::now() - started).count());
    job.remaining_work_milliseconds = elapsed < job.work_budget_milliseconds ?
	job.work_budget_milliseconds - elapsed : 1;
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
    PullbackWorkScope pullback_work(wrapper, job.remaining_work_milliseconds);
    const auto work_expired = [&job, &pullback_work, wrapper](
	const std::string &stage) {
	if (!pullback_work.DeadlineExpired())
	    return false;
	job.validation_message = "exact pullback exceeded the " +
	    std::to_string((job.work_budget_milliseconds + 999) / 1000) +
	    "-second per-item work budget during " + stage;
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    job.entity_id, job.entity_type, "trim_pcurve",
	    job.validation_message);
	job.brep.reset();
	job.status = BREP_CONVERSION_FAILED;
	return true;
    };

    LocalUnits::length = job.length_factor;
    LocalUnits::planeangle = job.planeangle_factor;
    LocalUnits::solidangle = job.solidangle_factor;
    LocalUnits::tolerance = job.tolerance;

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
    /* A valid periodic band can initially place both uses of its seam edge on
     * one parameter branch.  At that point it resembles a zero-area keyhole,
     * but the exact seam-pair repair below will move the uses to opposite
     * surface boundaries.  Refresh and classify first; normalize keyholes only
     * if the completed pcurve repair still leaves invalid topology. */
    size_t keyhole_splits = finalize_brep_topology(job.brep.get(), false,
	wrapper, job.entity_id, job.entity_type);
    if (work_expired("topology finalization"))
	return;
    if (!job.faceted) {
	wrapper->SetProgressDetail("repairing exact BREP trim orientations",
	    job.entity_id, 0, 0, std::string(), job.entity_type);
	repair_closed_trim_orientations(job.brep.get(), wrapper, job.entity_id,
	    job.entity_type);
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
    if (work_expired("preliminary OpenNURBS validation"))
	return;
    if (needs_post_repair_topology &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	bool topology_changed = false;
	const size_t post_repair_splits = finalize_brep_topology(job.brep.get(), true,
	    wrapper, job.entity_id, job.entity_type, &topology_changed);
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
    if (!structurally_valid) {
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
	for (std::set<int>::const_iterator li = detailed_loops.begin();
		li != detailed_loops.end(); ++li) {
	    if (*li < 0 || *li >= job.brep->m_L.Count())
		continue;
	    const ON_BrepLoop &loop = job.brep->m_L[*li];
	    joins << "\nloop L" << *li << "/STEP" << loop.m_loop_user.i
		<< " trims=";
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
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
		    << (trim->m_bRev3d ? 1 : 0) << ',' << start.x << ':'
		    << start.y << "->" << end.x << ':' << end.y << ')';
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
    bool solid = job.brep->IsSolid();
    if (work_expired("BREP solidness validation"))
	return;
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
	    wrapper->GeometryWorkerStarted();
	    try {
		convert_detached_brep_job(*jobs[index], wrapper);
	    } catch (...) {
		jobs[index]->status = BREP_CONVERSION_FAILED;
		jobs[index]->validation_message =
		    "exception while validating detached OpenNURBS geometry";
	    }
	    wrapper->GeometryWorkerFinished();
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
	    if (job.status == BREP_WRITE_SUCCESS &&
		!write_detached_brep_geometry(job, &spool, dry_run))
		job.status = BREP_OUTPUT_FAILED;
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
	    commit_spooled_brep_job(*jobs[next_commit], wrapper, &spool, dotg,
		dry_run, process_map);
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

    statistics.geometry_attempted = 0;
    statistics.geometry_written = 0;
    statistics.geometry_skipped = 0;
    statistics.skipped_items.clear();
    statistics.skipped_items_omitted = 0;
    statistics.invalid_breps = 0;
    statistics.output_failures = 0;
    statistics.styles_extracted = 0;
    statistics.styles_applied = 0;
    statistics.layers_extracted = 0;
    statistics.selected_entity_ids_encountered.clear();
    document.products.clear();
    document.representations.clear();
    document.occurrences.clear();
    SetProgress("counting STEP entity types", 0, statistics.input_instances);
    collectEntityCounts();
#ifdef AP214e3
    SetProgress("extracting AP214 styles and metadata");
    ExtractAP214Presentation(*this);
    if (HasLazyIndex() &&
	(!LazyInstancesByType("DESIGN_CONTEXT").empty() ||
	 !LazyInstancesByType("MECHANICAL_CONTEXT").empty())) {
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning, 0, "FILE_SCHEMA",
	    std::string(),
	    "AP214 file uses legacy AP203 DESIGN_CONTEXT/MECHANICAL_CONTEXT names; "
	    "imported as their attribute-compatible AP214 parent contexts");
    }
#endif
    statistics.tolerance_mm = deriveTolerance();
    LocalUnits::tolerance = statistics.tolerance_mm;
#ifdef HAVE_STEPCODE_LAZY
    const LazySTEPExactGraph lazy_exact_graph = build_lazy_exact_graph(this);
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
	statistics.products = static_cast<uint64_t>(document.products.size());
	statistics.occurrences = static_cast<uint64_t>(document.occurrences.size());
	return statistics.output_failures == 0 &&
	    (!import_options.strict || statistics.geometry_skipped == 0);
    }
	if (lazy_selection_contains_only_topology_roots(this)) {
	convert_lazy_selected_topology_roots(lazy_exact_graph, this, dotg, dry_run,
	    process_map);
	statistics.products = static_cast<uint64_t>(document.products.size());
	statistics.occurrences = static_cast<uint64_t>(document.occurrences.size());
	return statistics.output_failures == 0 &&
	    (!import_options.strict || statistics.geometry_skipped == 0);
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
    SetProgress("product and representation structure indexed",
	static_cast<uint64_t>(num_ents), static_cast<uint64_t>(num_ents));
#ifdef HAVE_STEPCODE_LAZY
    index_lazy_exact_graph(lazy_exact_graph, this, dotg, id2name_map,
	id2productid_map, shell2representation_map);
#endif
#ifdef AP214e3
    /* Product and representation identity must be established before
     * represented properties and make-from material links are detached into
     * the schema-neutral document. */
    SetProgress("extracting AP214 metadata");
    ExtractAP214Metadata(*this);
    SetProgress("converting AP214 CSG and swept solids");
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
#ifdef HAVE_STEPCODE_LAZY
    convert_lazy_exact_graph(lazy_exact_graph, this, dotg, dry_run,
	id2name_map, process_map);
#endif
    /* Convert exact solid and wire representations before potentially large,
     * monolithic shell-based surface models.  This lets bounded solid jobs
     * make deterministic output progress even when a supplemental plate model
     * needs expensive serial pullback work. */
    SetInstanceTypes({"SHAPE_DEFINITION_REPRESENTATION"}, lazy_handled_sdrs);
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
		!sse->IsA(SCHEMA_NAMESPACE::e_shape_representation_relationship))
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
	}
	ClearEntityCache();
    }

    SetInstanceTypes({"SHELL_BASED_SURFACE_MODEL"});
    num_ents = InstanceCount();
    SetProgress("converting shell-based surface models", 0,
	static_cast<uint64_t>(num_ents), 0, statistics.geometry_written, "written");
    std::vector<std::unique_ptr<DetachedBrepJob> > surface_jobs;
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

	    std::vector<std::pair<uint64_t, std::string> > shells;
	    size_t model_shell_count = 0;
	    for (std::vector<uint64_t>::const_iterator boundary = boundaries.begin();
		 boundary != boundaries.end(); ++boundary) {
		const std::string type = LazyTypeName(*boundary);
		if (type == "CLOSED_SHELL" || type == "OPEN_SHELL")
		    ++model_shell_count;
		if (*boundary <= static_cast<uint64_t>(INT_MAX) &&
		    (type == "CLOSED_SHELL" || type == "OPEN_SHELL") &&
		    (selected_model || selected.find(static_cast<int64_t>(*boundary)) !=
			selected.end())) {
		    shells.push_back(std::make_pair(*boundary, type));
		    if (!selected.empty())
			ShouldConvertEntity(static_cast<int64_t>(*boundary));
		}
	    }
	    for (size_t shell_number = 0; shell_number < shells.size(); ++shell_number) {
		const int shell_id = static_cast<int>(shells[shell_number].first);
		const std::string &type = shells[shell_number].second;
		std::string shell_name = model_shell_count == 1 ? surface_name :
		    dotg->StableBRLCADName(surface_name + "_shell" +
			std::to_string(shell_number + 1), shell_id);
		id2name_map[shell_id] = shell_name;
		id2productid_map[shell_id] = product_id;
		mat_t identity;
		MAT_IDN(identity);
		std::unique_ptr<DetachedBrepJob> job = detach_brep_job_data(
		    shell_id, type, std::string(), shell_name, product_id,
		    LocalUnits::length, LocalUnits::planeangle, LocalUnits::solidangle,
		    identity, style);
		if (model_shell_count == 1) job->output_source_id = surface_id;
		job->is_region = type == "CLOSED_SHELL";
		if (product_id > 0) {
		    DetachedBrepMember member;
		    member.combination = id2name_map[product_id];
		    MAT_COPY(member.matrix, identity);
		    job->members.push_back(member);
		}
		surface_jobs.push_back(std::move(job));
	    }
	    if (shells.empty()) {
		RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, surface_id,
		    "SHELL_BASED_SURFACE_MODEL", "sbsm_boundary",
		    "lazy index found no OPEN_SHELL or CLOSED_SHELL boundary references");
		RecordSkippedItem(surface_id, "SHELL_BASED_SURFACE_MODEL",
		    "surface model has no indexed shell boundaries");
		++statistics.geometry_attempted;
		++statistics.geometry_skipped;
	    }
	}
	SetProgress("shell boundaries indexed", surface_ids.size(),
	    surface_ids.size(), 0, surface_jobs.size(), "jobs");
    } else for (int i = 0; i < num_ents; ++i) {
	if (CancellationRequested()) return false;
	SDAI_Application_instance *sse = InstanceAt(i);
	if (!sse || sse->STEPfile_id <= 0 ||
		!sse->IsA(SCHEMA_NAMESPACE::e_shell_based_surface_model))
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
	    write_matrix, style);
	job->is_region = false;
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
#ifdef HAVE_STEPCODE_LAZY
    releaseLazyBatches();
#endif
    return statistics.output_failures == 0 && (!import_options.strict || statistics.geometry_skipped == 0);
}


void
STEPWrapper::RecordSkippedItem(int64_t entity_id, const std::string &entity_type,
    const std::string &reason)
{
    std::lock_guard<std::mutex> lock(diagnostic_mutex);
    if (statistics.skipped_items.size() >= kMaximumReportedSkippedItems) {
	++statistics.skipped_items_omitted;
	return;
    }
    brlcad::step::SkippedItem item;
    item.entity_id = entity_id;
    item.entity_type = entity_type;
    item.reason = reason;
    statistics.skipped_items.push_back(item);
}


SDAI_Application_instance *
STEPWrapper::getEntity(int STEPid)
{
    if (STEPid <= 0)
	return NULL;
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) {
	SDAI_Application_instance *instance = lazy_batch ? lazy_batch->Get(STEPid) : NULL;
	if (instance) return instance;
	for (std::vector<std::unique_ptr<brlcad::step::STEPLazyBatch> >::const_iterator batch =
		 lazy_supplemental_batches.begin(); batch != lazy_supplemental_batches.end(); ++batch) {
	    instance = (*batch)->Get(STEPid);
	    if (instance) return instance;
	}
	std::unique_ptr<brlcad::step::STEPLazyBatch> supplemental(
	    new brlcad::step::STEPLazyBatch(lazy_session->LoadBatch(static_cast<uint64_t>(STEPid))));
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
    SetProgress("reading and indexing STEP file");
#ifdef HAVE_STEPCODE_LAZY
    stepfile = step_file;
    lazy_session.reset(new brlcad::step::STEPLazySession);
    lazy_session->SetProgressCallback([this](const brlcad::step::STEPLazyProgress &progress) {
	SetProgress("reading and indexing STEP file", progress.offset,
	    progress.file_size, 0, progress.instances_scanned, "instances");
    });
    lazy_session->SetCancellationCallback([this]() {
	return CancellationRequested() || brlcad::PullbackWorkCancelled();
    });
    lazy_session->SetDiagnosticCallback([this](const brlcad::step::STEPLazyDiagnostic &source) {
	recordLazyDiagnostic(source);
    });
    if (!lazy_session->Open(stepfile)) {
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
	    std::string(), "STEPcode could not lazily index the exchange file");
	lazy_session.reset();
	return false;
    }
    lazy_instance_ids = lazy_session->AllInstances();
    const brlcad::step::STEPLazyStatistics cache = lazy_session->Statistics();
    statistics.input_instances = cache.instances_scanned;
    update_lazy_statistics(statistics, cache);
    synchronizeLazyDiagnostics();
    if (cache.cancelled || lazy_instance_ids.size() != cache.instances_scanned) {
	RecordDiagnostic(brlcad::step::DiagnosticSeverity::Fatal, 0, "PART21_FILE",
	    std::string(), cache.cancelled ? "STEP lazy scan was cancelled" :
	    "STEP lazy index did not retain every DATA instance");
	return false;
    }
    SetProgress("STEP index complete", statistics.input_instances,
	statistics.input_instances, 0, statistics.input_instances, "instances");
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
    SetProgress("STEP read complete", statistics.input_instances,
	statistics.input_instances, 0, statistics.input_instances, "instances");
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
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) {
	std::map<std::string, uint64_t> type_counts;
	if (verbose) {
	    for (std::vector<uint64_t>::const_iterator id = lazy_instance_ids.begin();
		 id != lazy_instance_ids.end(); ++id) {
		std::string type = lazy_session->TypeName(*id);
		if (type.empty()) type = "COMPLEX_ENTITY";
		++type_counts[type];
	    }
	}
	std::cout << "Indexed " << lazy_instance_ids.size() << " instances from ";
	if (BU_STR_EQUAL(stepfile.c_str(), "-"))
	    std::cout << "standard input" << std::endl;
	else
	    std::cout << "STEP file \"" << stepfile << "\"" << std::endl;
	if (verbose) {
	    for (std::map<std::string, uint64_t>::const_iterator type = type_counts.begin();
		 type != type_counts.end(); ++type)
		std::cout << '\t' << type->first << " " << type->second << std::endl;
	}
	const brlcad::step::STEPLazyStatistics cache = lazy_session->Statistics();
	std::cout << "Lazy index";
	if (verbose) std::cout << " contains " << type_counts.size() << " entity types";
	std::cout << "; loaded=" << cache.instances_loaded
	    << ", pinned=" << cache.instances_pinned
	    << ", source-cache-bytes=" << cache.resident_source_bytes << std::endl;
	return;
    }
#endif
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
