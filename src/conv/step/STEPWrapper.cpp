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

/* interface header */
#include "./STEPWrapper.h"
#include "STEPBudget.h"
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
#include "FaceSurface.h"
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

/* A surface which declares itself closed can still differ slightly under raw
 * OpenNURBS evaluation at parameter values separated by one complete period.
 * Safe repair may reflect a densely measured closure discrepancy in the local
 * trim tolerance, but never by more than twice the previously established
 * edge/vertex tolerance.  --exact does not use this allowance. */
constexpr double kPeriodicClosureToleranceMaximumScale = 2.0;

/* A seeded solve on a closed spline can converge to an extrapolated image
 * many periods from the supplied pcurve.  More than one and a half periods
 * cannot be the nearest equivalent image; retry in the native domain before
 * allowing continuity logic to consume that result. */
constexpr double kMaximumSeededPullbackPeriodDrift = 1.5;

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
 * per-item CPU-work budget still bounds pathological cases. */
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

/* A localized endpoint candidate is a polyline sampled from the unchanged
 * pcurve with only its first or last point replaced.  Limit it to twice the
 * dense validation mesh so the fixed 4096-sample candidate audit still tests
 * at least every segment midpoint.  Finer candidates could hide a nonlinear
 * surface excursion between validation samples instead of proving a safe
 * endpoint association. */
constexpr int kMaximumLocalizedEndpointSamples =
    2 * kDenseValidationSegments;

/* Exact pcurves occasionally arrive on adjacent periodic images of the same
 * closed surface.  Solving a complete cyclic loop may require moving a
 * contiguous group together, which a greedy one-trim repair cannot do without
 * temporarily worsening its other endpoint.  Search only a small integral
 * neighborhood around each supplied branch; every accepted translation is
 * still densely proven to preserve its surface lift. */
constexpr int kMaximumPeriodicBranchShift = 2;

/* Non-exact mode may reflect a densely measured mismatch between paired
 * representations (3-D edge/surface or 3-D edge/existing pcurve) in only the
 * affected OpenNURBS edge and trim tolerances.  A two-percent local-feature
 * and two-tenths-percent item-scale ceiling covers the smallest paired Mark V
 * curves whose independently supplied surfaces report the same 0.0164 mm
 * miss, while remaining far below a topology-changing gap.  This does not
 * alter either source curve; it only lets later exact processing use a
 * tolerance justified by dense measurements.  --exact disables it. */
constexpr double kRegenerationMaximumRelativeMismatch = 2.0e-2;
constexpr double kRegenerationMaximumRelativeItemMismatch = 2.0e-3;
constexpr double kRegenerationToleranceSafety = 1.05;

/* A same-vertex STEP edge asserts zero topological extent.  If its sole
 * boundary use instead carries a short, geometrically open spline spur, safe
 * mode may remove that contradiction only after the complete curve stays
 * within five ten-thousandths of the finished item's diagonal.  This is four
 * times tighter than the general item-scale edge/surface mismatch ceiling and
 * is used only for open, single-use, zero-length topology edges; --exact and
 * genuinely closed small features never enter the repair. */
constexpr double kZeroLengthTopologyMaximumRelativeItemMismatch = 5.0e-4;

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

/* A surface boundary is a one-dimensional curve, but a seed obtained from a
 * global surface closest-point query can lie on the wrong repeated branch of
 * a doubly periodic surface.  Sample a modest, fixed number of boundary
 * intervals before retrying the local Newton solve.  This is a search budget;
 * every accepted result is still checked against model uncertainty. */
constexpr int kBoundaryParameterSearchSegments = 256;

/* Screen every supplied open pcurve at a modest deterministic resolution
 * before using its winding to infer periodic topology.  Endpoint-only checks
 * miss complementary circle arcs whose ends are exact but whose interiors
 * lie on the opposite half of a closed surface.  Direct edge/trim parameter
 * correspondence is tested first; closest-point work is needed only for a
 * sample which exceeds tolerance. */
constexpr int kPcurveLocusScreeningSegments = 64;

/* Scale ON_ZERO_TOLERANCE above floating-point noise for parameter/lift
 * equivalence checks.  Keep this distinct from model-space uncertainty. */
constexpr double kNumericalToleranceScale = 1024.0;

/* Keep reports useful for targeted retries without allowing a corrupt file to
 * grow JSON output without bound.  The total omitted count remains exact. */
constexpr size_t kMaximumReportedSkippedItems = 4096;

/* Retain entity-specific stage timings above one second.  Aggregate timing is
 * recorded for every call; this bounded detail list exists to identify the
 * smaller set of jobs worth a focused --entity retry. */
constexpr uint64_t kSlowStageTimingMicroseconds = 1000000;
constexpr size_t kMaximumSlowItemTimings = 4096;

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

/* Reference-host CPU-work ceilings.  The startup geometry calibration
 * scales these values for the current machine unless the user supplies an
 * explicit scale or per-item limit.  They are not acceptance tolerances. */
constexpr uint64_t kMaximumExactPullbackMilliseconds = 60000;
/* Large STEP solids may intentionally contain hundreds or thousands of faces
 * under one representation item.  Once topology is detached and countable,
 * add 500 ms per face to a 60-second complex-item overhead allowance, with a
 * 15-minute absolute ceiling.  The fixed allowance covers dependency
 * materialization, final topology validation, and bounded contention while
 * the dynamic scheduler transfers workers from other roots to nested
 * face/edge groups.  Items at or below 64 faces retain the one-minute
 * investigation budget; larger items have enough independently countable
 * topology to justify the reported complexity exception.
 * Crossing one minute is therefore limited to an explicitly reported,
 * measured-topology complexity exception; the ceiling still prevents a
 * malformed giant item from monopolizing an import indefinitely. */
constexpr uint64_t kMaximumComplexExactPullbackMilliseconds = 15 * 60 * 1000;
constexpr uint64_t kExactPullbackMillisecondsPerFace = 500;
constexpr size_t kComplexExactSolidFaceThreshold = 64;
constexpr uint64_t kMaximumSurfaceModelPullbackMilliseconds = 120000;
/* Completed MAZ measurements show that each 1334-face detached solid can
 * require roughly 600 MiB while exact pullbacks and repair candidates coexist.
 * Running five such roots concurrently exceeded the 2 GiB importer gate even
 * though each root made steady progress.  At this explicit power-of-two
 * topology threshold, admit one root at a time and lend the remaining worker
 * capacity to its face-level work.  Smaller roots retain ordinary root-level
 * parallelism. */
constexpr size_t kExclusivePullbackTopologyFaceThreshold = 1024;
/* A surface with thousands of tensor-product spans makes each global
 * closest-point query materially different from ordinary analytic and small
 * spline faces.  The CRM problem face has 34,277 spans; reserving the machine
 * only above this power-of-two preflight threshold preserves AP214 assembly
 * throughput while ensuring those measured outliers receive nested workers
 * before their item clock starts. */
constexpr size_t kExclusivePullbackSurfaceSpanThreshold = 4096;
class PullbackWorkScope {
public:
    PullbackWorkScope(STEPWrapper *source, uint64_t maximum_elapsed_milliseconds,
	uint64_t maximum_stall_milliseconds)
	: wrapper(source)
    {
	brlcad::SetPullbackWorkLimit(CancellationRequested, wrapper,
	    maximum_elapsed_milliseconds, maximum_stall_milliseconds);
    }
    ~PullbackWorkScope() { brlcad::ClearPullbackWorkLimit(); }
    bool DeadlineExpired() const { return brlcad::PullbackWorkDeadlineExpired(); }
    bool Stalled() const { return brlcad::PullbackWorkStalled(); }

private:
    static bool CancellationRequested(void *context)
    {
	STEPWrapper *source = static_cast<STEPWrapper *>(context);
	return source && source->CancellationRequested();
    }
    STEPWrapper *wrapper;
};


class PullbackWorkStopped : public std::exception {
public:
    PullbackWorkStopped(bool elapsed_deadline, bool no_progress)
	: deadline_expired(elapsed_deadline), stalled(no_progress)
    {
    }

    const char *what() const noexcept override
    {
	return stalled ? "nested pullback work stalled" :
	    "nested pullback work exceeded its deadline";
    }

    bool deadline_expired;
    bool stalled;
};

}


void
STEPWrapper::configureImportBudgets()
{
    const bool automatic_scale = !(import_options.budget_scale > 0.0);
    const bool calibration_needed = automatic_scale &&
	(!import_options.item_budget_milliseconds ||
	 !import_options.stall_timeout_milliseconds);
    brlcad::step::BudgetCalibration calibration;
    if (calibration_needed)
	calibration = brlcad::step::CalibrateImportBudgets(
	    import_options.effective_jobs);

    statistics.budget_calibration_ran = calibration_needed;
    statistics.budget_calibration_valid = calibration.valid;
    statistics.budget_calibration_queries = calibration.scalar_queries;
    statistics.budget_calibration_microseconds =
	calibration.scalar_microseconds;
    statistics.budget_calibration_parallel_workers =
	calibration.parallel_workers;
    statistics.budget_calibration_scalar_queries_per_second =
	calibration.scalar_queries_per_second;
    statistics.budget_calibration_parallel_queries_per_second =
	calibration.parallel_queries_per_second;
    statistics.budget_calibration_parallel_cpu_queries_per_second =
	calibration.parallel_cpu_queries_per_second;

    import_options.effective_budget_scale = automatic_scale ?
	(calibration.valid ? calibration.scale : 1.0) :
	import_options.budget_scale;

    const auto scaled = [this](uint64_t reference) {
	const long double value = static_cast<long double>(reference) *
	    static_cast<long double>(import_options.effective_budget_scale);
	if (value >= static_cast<long double>(UINT64_MAX)) return UINT64_MAX;
	return std::max<uint64_t>(1, static_cast<uint64_t>(value + 0.5L));
    };
    import_options.effective_item_budget_milliseconds =
	import_options.disable_item_budgets ? 0 :
	(import_options.item_budget_milliseconds ?
	 import_options.item_budget_milliseconds :
	 scaled(kMaximumExactPullbackMilliseconds));
    import_options.effective_stall_timeout_milliseconds =
	import_options.stall_timeout_milliseconds ?
	import_options.stall_timeout_milliseconds :
	scaled(kMaximumExactPullbackMilliseconds);
}

struct STEPWrapper::GeometryExecutor {
    struct Group {
	Group(const std::function<void(size_t)> &caller,
	    const std::function<void(size_t)> &helper, size_t task_count,
	    uint64_t remaining_milliseconds, uint64_t creation_order)
	    : caller_task(caller), helper_task(helper), count(task_count),
	      order(creation_order)
	{
	    deadline = remaining_milliseconds == UINT64_MAX ?
		std::chrono::steady_clock::time_point::max() :
		std::chrono::steady_clock::now() +
		std::chrono::milliseconds(remaining_milliseconds);
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
	std::chrono::steady_clock::time_point deadline;
	size_t count = 0;
	size_t next = 0;
	size_t running = 0;
	std::exception_ptr exception;
	uint64_t order = 0;
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

    void WorkerStarted(bool exclusive_pullback)
    {
	std::unique_lock<std::mutex> guard(lock);
	if (exclusive_pullback) ++exclusive_waiters;
	/* Admit roots freely while no nested work exists.  As soon as any active
	 * root publishes face/edge tasks, stop replacing completed root workers;
	 * their slots transfer to the helper pool until the nested queue drains.
	 * This keeps all CPUs useful without starting more wall-clock item
	 * deadlines than the machine can service. */
	changed.wait(guard, [this, exclusive_pullback]() {
	    RemoveCompletedGroupsLocked();
	    if (stopping) return true;
	    if (exclusive_pullback)
		return !exclusive_root_active && top_level_workers == 0 &&
		    active_helpers == 0 && groups.empty();
	    return !exclusive_root_active && exclusive_waiters == 0 &&
		groups.empty() && top_level_workers + active_helpers < capacity;
	});
	if (exclusive_pullback && exclusive_waiters) --exclusive_waiters;
	if (!stopping) {
	    ++top_level_workers;
	    if (exclusive_pullback) exclusive_root_active = true;
	}
	guard.unlock();
	changed.notify_all();
    }

    void WorkerFinished(bool exclusive_pullback)
    {
	{
	    std::lock_guard<std::mutex> guard(lock);
	    if (top_level_workers) --top_level_workers;
	    if (exclusive_pullback) exclusive_root_active = false;
	}
	changed.notify_all();
    }

    void ParallelFor(size_t count, const std::function<void(size_t)> &caller_task,
	const std::function<void(size_t)> &helper_task,
	uint64_t remaining_milliseconds)
    {
	if (count < 2 || capacity < 2) {
	    for (size_t index = 0; index < count; ++index) caller_task(index);
	    return;
	}
	std::shared_ptr<Group> group;
	{
	    std::lock_guard<std::mutex> guard(lock);
	    group.reset(new Group(caller_task, helper_task, count,
		remaining_milliseconds, next_group_order++));
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
	changed.notify_all();
    }

    unsigned int AvailableHelperCapacity()
    {
	std::lock_guard<std::mutex> guard(lock);
	const unsigned int occupied = top_level_workers + active_helpers;
	return occupied < capacity ? capacity - occupied : 0;
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
		/* Give helpers to the job with the least remaining calibrated work
		 * first.  Creation order keeps equal and unlimited budgets
		 * deterministic. */
		std::deque<std::shared_ptr<Group> >::iterator selected =
		    std::min_element(groups.begin(), groups.end(),
			[](const std::shared_ptr<Group> &left,
			   const std::shared_ptr<Group> &right) {
			    if (left->deadline != right->deadline)
				return left->deadline < right->deadline;
			    return left->order < right->order;
			});
		group = *selected;
		groups.erase(selected);
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
	unsigned int exclusive_waiters = 0;
	bool exclusive_root_active = false;
	uint64_t next_group_order = 0;
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
STEPWrapper::GeometryWorkerStarted(int64_t entity_id,
    const std::string &entity_type, bool exclusive_pullback)
{
    if (geometry_executor) geometry_executor->WorkerStarted(exclusive_pullback);
    std::lock_guard<std::mutex> guard(progress_mutex);
    ActiveGeometryJobProgress &job =
	active_geometry_job_progress[std::this_thread::get_id()];
    job = ActiveGeometryJobProgress();
    job.started = std::chrono::steady_clock::now();
    job.root_entity_id = entity_id;
    job.current_entity_id = entity_id;
    job.entity_type = entity_type;
    job.phase = "starting detached exact geometry";
}


void
STEPWrapper::GeometryWorkerFinished(bool exclusive_pullback)
{
    {
	std::lock_guard<std::mutex> guard(progress_mutex);
	active_geometry_job_progress.erase(std::this_thread::get_id());
    }
    if (geometry_executor) geometry_executor->WorkerFinished(exclusive_pullback);
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
    const brlcad::PullbackWorkBudgetHandle work_budget =
	brlcad::CurrentPullbackWorkBudget();
    const double length = LocalUnits::length;
    const double planeangle = LocalUnits::planeangle;
    const double solidangle = LocalUnits::solidangle;
    const double tolerance = LocalUnits::tolerance;
    const std::function<void(size_t)> helper_task = [this, task, work_budget,
	length, planeangle, solidangle, tolerance](size_t index) {
	LocalUnits::length = length;
	LocalUnits::planeangle = planeangle;
	LocalUnits::solidangle = solidangle;
	LocalUnits::tolerance = tolerance;
	brlcad::SetPullbackWorkLimit(geometry_helper_cancelled, this,
	    work_budget,
	    import_options.effective_stall_timeout_milliseconds);
	try {
	    task(index);
	    const bool deadline_expired =
		brlcad::PullbackWorkDeadlineExpired();
	    const bool stalled = brlcad::PullbackWorkStalled();
	    brlcad::ClearPullbackWorkLimit();
	    if (deadline_expired || stalled)
		throw PullbackWorkStopped(deadline_expired, stalled);
	} catch (...) {
	    brlcad::ClearPullbackWorkLimit();
	    throw;
	}
    };
    try {
	geometry_executor->ParallelFor(count, task, helper_task, remaining);
    } catch (const PullbackWorkStopped &stopped) {
	brlcad::PropagatePullbackWorkStop(stopped.deadline_expired,
	    stopped.stalled);
    }
    /* Helper completion is forward progress for the waiting parent.  This
     * refreshes its stall heartbeat without clearing a propagated reason. */
    brlcad::PullbackWorkProgress(count);
}


unsigned int
STEPWrapper::AvailableGeometryHelperCapacity()
{
    return geometry_executor ? geometry_executor->AvailableHelperCapacity() : 0;
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
    brlcad::PullbackWorkProgress();
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.phase = phase;
    progress_state.current_entity_id = current_entity_id;
    progress_state.secondary_completed = secondary_completed;
    progress_state.secondary_total = secondary_total;
    progress_state.secondary_label = secondary_label;
    progress_state.detail = detail;
    std::map<std::thread::id, ActiveGeometryJobProgress>::iterator active =
	active_geometry_job_progress.find(std::this_thread::get_id());
    if (active != active_geometry_job_progress.end()) {
	active->second.phase = phase;
	/* A face counter is the stable outer item context.  Nested pullback,
	 * seam, and loop calls may update the operation detail without erasing
	 * which face out of the complete solid is being processed. */
	if (secondary_label == "faces" && secondary_total) {
	    active->second.item_entity_id = current_entity_id;
	    active->second.item_completed = secondary_completed;
	    active->second.item_total = secondary_total;
	    active->second.item_label = secondary_label;
	} else {
	    if (current_entity_id > 0)
		active->second.current_entity_id = current_entity_id;
	    active->second.secondary_completed = secondary_completed;
	    active->second.secondary_total = secondary_total;
	    active->second.secondary_label = secondary_label;
	}
	active->second.detail = detail;
    }
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
STEPWrapper::SetGeometryOverallProgress(uint64_t processed, uint64_t total)
{
    std::lock_guard<std::mutex> guard(progress_mutex);
    progress_state.geometry_items_processed = std::max(
	progress_state.geometry_items_processed, processed);
    progress_state.geometry_items_total = std::max(
	progress_state.geometry_items_total, total);
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
    brlcad::step::ImportProgress snapshot = progress_state;
    if (active_geometry_job_progress.empty())
	return snapshot;

    std::map<std::thread::id, ActiveGeometryJobProgress>::const_iterator oldest =
	active_geometry_job_progress.begin();
    for (std::map<std::thread::id, ActiveGeometryJobProgress>::const_iterator job =
	    active_geometry_job_progress.begin();
	    job != active_geometry_job_progress.end(); ++job) {
	if (job->second.started < oldest->second.started)
	    oldest = job;
    }
    const ActiveGeometryJobProgress &job = oldest->second;
    snapshot.phase = job.phase;
    snapshot.current_entity_id = job.root_entity_id;
    snapshot.geometry_root_entity_id = job.root_entity_id;
    snapshot.geometry_item_entity_id = job.item_entity_id;
    snapshot.geometry_item_completed = job.item_completed;
    snapshot.geometry_item_total = job.item_total;
    snapshot.geometry_item_label = job.item_label;
    snapshot.geometry_subentity_id = job.current_entity_id;
    snapshot.secondary_completed = job.secondary_completed;
    snapshot.secondary_total = job.secondary_total;
    snapshot.secondary_label = job.secondary_label;
    std::ostringstream detail;
    if (!job.entity_type.empty())
	detail << job.entity_type;
    if (job.current_entity_id > 0 &&
	    job.current_entity_id != job.root_entity_id)
	detail << (detail.tellp() > 0 ? " " : "") << "subentity=#"
	    << job.current_entity_id;
    if (!job.detail.empty() && job.detail != job.entity_type)
	detail << (detail.tellp() > 0 ? " " : "") << job.detail;
    const uint64_t active_seconds = static_cast<uint64_t>(
	std::chrono::duration_cast<std::chrono::seconds>(
	    std::chrono::steady_clock::now() - job.started).count());
    detail << (detail.tellp() > 0 ? " " : "")
	<< "oldest-active=" << active_seconds << 's';
    snapshot.detail = detail.str();
    return snapshot;
}


void
STEPWrapper::RecordStageTiming(const std::string &stage, int64_t entity_id,
    const std::string &entity_type, uint64_t elapsed_us, uint64_t faces,
    uint64_t edges, uint64_t trims)
{
    if (stage.empty()) return;
    std::lock_guard<std::mutex> guard(telemetry_mutex);
    brlcad::step::StageTiming &timing = statistics.stage_timings[stage];
    ++timing.calls;
    timing.total_us += elapsed_us;
    if (elapsed_us > timing.maximum_us) {
	timing.maximum_us = elapsed_us;
	timing.maximum_entity_id = entity_id;
    }
    if (elapsed_us < kSlowStageTimingMicroseconds) return;
    if (statistics.slow_item_timings.size() >= kMaximumSlowItemTimings) {
	++statistics.slow_item_timings_omitted;
	return;
    }
    brlcad::step::ItemTiming item;
    item.entity_id = entity_id;
    item.entity_type = entity_type;
    item.stage = stage;
    item.elapsed_us = elapsed_us;
    item.faces = faces;
    item.edges = edges;
    item.trims = trims;
    statistics.slow_item_timings.push_back(item);
}


void
STEPWrapper::RecordPullbackStatistics(const brlcad::PullbackStatistics &source)
{
    std::lock_guard<std::mutex> guard(telemetry_mutex);
    statistics.pullback_closest_point_queries += source.closest_point_queries;
    statistics.pullback_surfaces_prepared += source.surfaces_prepared;
    statistics.pullback_surface_cache_hits += source.surface_cache_hits;
    statistics.pullback_span_boxes_built += source.span_boxes_built;
    statistics.pullback_span_boxes_tested += source.span_boxes_tested;
    statistics.pullback_primary_search_successes +=
	source.primary_search_successes;
    statistics.pullback_continuity_seed_searches +=
	source.continuity_seed_searches;
    statistics.pullback_continuity_seed_successes +=
	source.continuity_seed_successes;
    statistics.pullback_continuity_seed_failures +=
	source.continuity_seed_failures;
    statistics.pullback_continuity_seed_finite_candidates +=
	source.continuity_seed_finite_candidates;
    statistics.pullback_continuity_seed_iterations +=
	source.continuity_seed_iterations;
    statistics.pullback_continuity_seed_line_searches +=
	source.continuity_seed_line_searches;
    statistics.pullback_maximum_continuity_seed_iterations = std::max(
	statistics.pullback_maximum_continuity_seed_iterations,
	source.maximum_continuity_seed_iterations);
    statistics.pullback_maximum_continuity_seed_line_searches = std::max(
	statistics.pullback_maximum_continuity_seed_line_searches,
	source.maximum_continuity_seed_line_searches);
    statistics.pullback_multiseed_fallbacks += source.multiseed_fallbacks;
    statistics.pullback_multiseed_successes += source.multiseed_successes;
    statistics.pullback_multiseed_failures += source.multiseed_failures;
    statistics.pullback_fallback_calls_with_finite_primary +=
	source.fallback_calls_with_finite_primary;
    statistics.pullback_fallback_samples_evaluated +=
	source.fallback_samples_evaluated;
    statistics.pullback_fallback_seed_refinements +=
	source.fallback_seed_refinements;
    statistics.pullback_fallback_refinement_improvements +=
	source.fallback_refinement_improvements;
    statistics.pullback_fallback_late_seed_improvements +=
	source.fallback_late_seed_improvements;
    statistics.pullback_maximum_winning_seed_index = std::max(
	statistics.pullback_maximum_winning_seed_index,
	source.maximum_winning_seed_index);
    statistics.pullback_subdivision_nodes += source.subdivision_nodes;
    statistics.pullback_maximum_subdivision_nodes = std::max(
	statistics.pullback_maximum_subdivision_nodes,
	source.maximum_subdivision_nodes);
    statistics.pullback_preparation_us += source.preparation_us;
    statistics.pullback_primary_search_us += source.primary_search_us;
    statistics.pullback_continuity_seed_us += source.continuity_seed_us;
    statistics.pullback_multiseed_us += source.multiseed_us;
    statistics.pullback_fallback_primary_improvement_total +=
	source.fallback_primary_improvement_total;
    statistics.pullback_fallback_primary_improvement_maximum = std::max(
	statistics.pullback_fallback_primary_improvement_maximum,
	source.fallback_primary_improvement_maximum);
    statistics.pullback_fallback_refinement_improvement_total +=
	source.fallback_refinement_improvement_total;
    statistics.pullback_fallback_refinement_improvement_maximum = std::max(
	statistics.pullback_fallback_refinement_improvement_maximum,
	source.fallback_refinement_improvement_maximum);
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



#include "STEPWrapperBrep.inc"
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
    document.occurrences.clear();
    {
	std::lock_guard<std::mutex> guard(progress_mutex);
	progress_state = brlcad::step::ImportProgress();
	active_geometry_job_progress.clear();
    }
    SetProgress("calibrating exact-geometry work budgets");
    configureImportBudgets();
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
    /* Assembly relationships are indexed before geometry conversion.  Make
     * every represented product a concrete database object now so a product
     * whose geometry is later skipped remains a resolvable (empty) assembly
     * member instead of becoming a dangling db_lookup reference. */
    ensure_product_combinations(this, dotg, dry_run, id2name_map);
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
	for (std::map<int, uint64_t>::const_iterator skipped = skipped_by_product.begin();
		skipped != skipped_by_product.end(); ++skipped) {
	    MAP_OF_ENTITY_ID_TO_PRODUCT_NAME::const_iterator product_name =
		id2name_map.find(skipped->first);
	    if (product_name == id2name_map.end() || product_name->second.empty()) continue;
	    dotg->SetCombinationAttribute(product_name->second, "step:import_status", "partial");
	    dotg->SetCombinationAttribute(product_name->second,
		"step:skipped_geometry_count", std::to_string(skipped->second));
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


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
