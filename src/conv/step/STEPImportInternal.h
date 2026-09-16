/*                 S T E P I M P O R T I N T E R N A L . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
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
 *
 * Private policy constants and bounded-work helpers shared by the STEP
 * importer core and geometry pipeline.  Nothing in this header is plugin ABI.
 */

#ifndef CONV_STEP_STEPIMPORTINTERNAL_H
#define CONV_STEP_STEPIMPORTINTERNAL_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <sstream>
#include <string>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/process.h"
#include "brep/pullback.h"

#include "STEPWrapper.h"

namespace step_import_detail {

/* Dense repair validation uses a power-of-two subdivision count so adjacent
 * passes sample the same deterministic parameter fractions.  1024 segments
 * proved sufficient to expose narrow periodic seam and pole failures while
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
 * and two-tenths-percent item-scale ceiling covers the smallest paired curves
 * in the focused fixtures whose independently supplied surfaces report the
 * same 0.0164 mm miss, while remaining far below a topology-changing gap.
 * This does not alter either source curve; it only lets later exact processing use a
 * tolerance justified by dense measurements.  --exact disables it. */
constexpr double kRegenerationMaximumRelativeMismatch = 2.0e-2;
constexpr double kRegenerationMaximumRelativeItemMismatch = 2.0e-3;
constexpr double kRegenerationToleranceSafety = 1.05;

/* A same-vertex STEP edge asserts zero topological extent.  If its sole
 * boundary use instead carries a short, geometrically open spline spur,
 * permissive mode may remove that contradiction only after the complete curve
 * stays within five ten-thousandths of the finished item's diagonal.  This is
 * four times tighter than the general item-scale edge/surface mismatch ceiling
 * and is used only for open, single-use, zero-length topology edges; --exact
 * and genuinely closed small features never enter the repair. */
constexpr double kZeroLengthTopologyMaximumRelativeItemMismatch = 5.0e-4;

/* A contradictory one-vertex edge is still a local feature even when the
 * complete item is large.  Permissive inference may remove its sole open
 * boundary spur only when that spur is also below one quarter of each
 * adjacent nondegenerate edge scale.  Together with the much tighter item
 * ceiling above, this prevents an assembly-sized bounding box from
 * authorizing deletion of an intended small loop. */
constexpr double kZeroLengthTopologyMaximumRelativeNeighborMismatch = 2.5e-1;

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
 * importer process-memory acceptance gate for that reorder buffer.  OpenNURBS
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
/* Structural validation can expose an exact zero-area keyhole only after the
 * ordinary bounded repair has consumed a complex item's primary allowance.
 * Reserve one short, separately bounded transaction for that topology-only
 * recovery instead of discarding the already constructed BREP at the primary
 * deadline. */
constexpr uint64_t kTopologicalKeyholeRetryMilliseconds = 45000;
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
/* Measurements of the 1334-face AP203 fixtures show that each detached solid
 * can require roughly 600 MiB while exact pullbacks and repair candidates coexist.
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
    void Restart(uint64_t maximum_elapsed_milliseconds,
	uint64_t maximum_stall_milliseconds)
    {
	brlcad::SetPullbackWorkLimit(CancellationRequested, wrapper,
	    maximum_elapsed_milliseconds, maximum_stall_milliseconds);
    }

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

} // namespace step_import_detail

#endif /* CONV_STEP_STEPIMPORTINTERNAL_H */
