/* BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file step/STEPBrepFinalize.cpp
 *
 * Whole-BREP orientation, validation, bounded retries, and final repairs.
 * Compiled as one schema-neutral importer build unit.
 */

#include "common.h"
#include "STEPBrepRepairInternal.h"

namespace step_brep_detail {
using namespace step_import_detail;

void
repair_closed_trim_orientations(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	bool allow_surface_alignment)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return;
    if (brlcad::PullbackWorkCancelled())
	return;

    /* Track source edges which begin this pass as an ordinary same-loop seam.
     * Verbose reports emit only phase-to-phase topology changes, making it
     * possible to locate a repair that accidentally detaches one use without
     * hard-coding corpus entity IDs or flooding normal diagnostics. */
    std::set<int> tracked_same_loop_seam_sources;
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_edge_user.i <= 0 || edge.m_ti.Count() != 2)
	    continue;
	const ON_BrepTrim *first = brep->Trim(edge.m_ti[0]);
	const ON_BrepTrim *second = brep->Trim(edge.m_ti[1]);
	if (first && second && first->m_li >= 0 &&
		first->m_li == second->m_li)
	    tracked_same_loop_seam_sources.insert(edge.m_edge_user.i);
    }
    const auto source_topology_signature = [brep](int source_edge_id) {
	std::ostringstream signature;
	for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    if (edge.m_edge_user.i != source_edge_id)
		continue;
	    signature << " E" << ei << '[' << edge.m_vi[0] << ':'
		<< edge.m_vi[1] << ']';
	    for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
		const ON_BrepTrim *trim = brep->Trim(edge.m_ti[eti]);
		const ON_BrepLoop *loop = trim ? trim->Loop() : NULL;
		signature << "/T" << edge.m_ti[eti] << "(L"
		    << (trim ? trim->m_li : -1) << "/STEP"
		    << (loop ? loop->m_loop_user.i : 0) << ",type="
		    << (trim ? static_cast<int>(trim->m_type) : -1)
		    << ",rev=" << (trim && trim->m_bRev3d ? 1 : 0) << ')';
	    }
	}
	return signature.str();
    };
    std::map<int, std::string> tracked_source_signatures;
    for (std::set<int>::const_iterator source =
	    tracked_same_loop_seam_sources.begin();
	    source != tracked_same_loop_seam_sources.end(); ++source)
	tracked_source_signatures[*source] = source_topology_signature(*source);
    std::string previous_stage("initial topology");
    std::chrono::steady_clock::time_point repair_stage_started =
	std::chrono::steady_clock::now();
    bool repair_stage_active = false;
    std::map<std::string, double> previous_directed_endpoint_mismatches;
    bool have_directed_endpoint_audit = false;
    std::map<std::string, double> previous_parameter_join_mismatches;
    bool have_parameter_join_audit = false;
    std::map<int, std::string> previous_single_singular_signatures;
    bool have_single_singular_audit = false;
    class RepairStageTimingGuard {
    public:
	RepairStageTimingGuard(STEPWrapper *stage_wrapper, ON_Brep *stage_brep,
		int stage_entity_id, const std::string &stage_entity_type,
		std::string &stage_name,
		std::chrono::steady_clock::time_point &stage_started,
		bool &stage_active)
	    : wrapper(stage_wrapper), brep(stage_brep), entity_id(stage_entity_id),
	      entity_type(stage_entity_type), name(stage_name),
	      started(stage_started), active(stage_active)
	{
	}

	~RepairStageTimingGuard()
	{
	    Finish();
	}

	void Finish()
	{
	    if (!active)
		return;
	    const std::chrono::steady_clock::time_point now =
		std::chrono::steady_clock::now();
	    wrapper->RecordStageTiming("trim_orientation/" + name,
		entity_id, entity_type, static_cast<uint64_t>(
		    std::chrono::duration_cast<std::chrono::microseconds>(
			now - started).count()),
		static_cast<uint64_t>(brep->m_F.Count()),
		static_cast<uint64_t>(brep->m_E.Count()),
		static_cast<uint64_t>(brep->m_T.Count()));
	    active = false;
	}

    private:
	STEPWrapper *wrapper;
	ON_Brep *brep;
	int entity_id;
	const std::string &entity_type;
	std::string &name;
	std::chrono::steady_clock::time_point &started;
	bool &active;
    } repair_stage_timing(wrapper, brep, entity_id, entity_type,
	previous_stage, repair_stage_started, repair_stage_active);
    /* Each operation below is bounded, but the sequence is exploratory: a
     * later periodic alternative can regress a candidate which an earlier
     * set of repairs had already made structurally valid.  The caller can
     * retain a candidate which was valid before this function, but it cannot
     * see a valid intermediate.  Probe on private copies (ON_Brep::IsValid()
     * refreshes derived state), retain the first proven intermediate, and
     * restore it only when every later alternative finishes invalid. */
    class ValidRepairCheckpoint {
    public:
	ValidRepairCheckpoint(ON_Brep *checkpoint_brep,
		STEPWrapper *checkpoint_wrapper, int checkpoint_entity_id,
		const std::string &checkpoint_entity_type)
	    : brep(checkpoint_brep), wrapper(checkpoint_wrapper),
	      entity_id(checkpoint_entity_id),
	      entity_type(checkpoint_entity_type)
	{
	}

	~ValidRepairCheckpoint()
	{
	    if (!candidate || !brep || !wrapper ||
		    brlcad::PullbackWorkCancelled())
		return;
	    std::unique_ptr<ON_Brep> final_probe(new ON_Brep(*brep));
	    if (final_probe->IsValid())
		return;
	    *brep = *candidate;
	    wrapper->RecordRepair(entity_id, entity_type, "bounded_repair",
		"restored the first structurally valid intermediate bounded "
		"repair after later alternatives regressed it (after " +
		phase + ")");
	}

	void Check(const std::string &completed_phase)
	{
	    if (candidate || !brep || brlcad::PullbackWorkCancelled())
		return;
	    std::unique_ptr<ON_Brep> probe(new ON_Brep(*brep));
	    if (!probe->IsValid())
		return;
	    candidate = std::move(probe);
	    phase = completed_phase;
	}

    private:
	ON_Brep *brep;
	STEPWrapper *wrapper;
	int entity_id;
	const std::string &entity_type;
	std::unique_ptr<ON_Brep> candidate;
	std::string phase;
    } valid_repair_checkpoint(brep, wrapper, entity_id, entity_type);
    const auto report_stage = [wrapper, entity_id, &entity_type,
	brep, &previous_directed_endpoint_mismatches,
	&have_directed_endpoint_audit, &previous_parameter_join_mismatches,
	&have_parameter_join_audit, &previous_single_singular_signatures,
	&have_single_singular_audit,
	&tracked_same_loop_seam_sources, &tracked_source_signatures,
	&source_topology_signature, &previous_stage, &repair_stage_started,
	&repair_stage_active, &repair_stage_timing,
	&valid_repair_checkpoint](const char *phase) {
	repair_stage_timing.Finish();
	valid_repair_checkpoint.Check(previous_stage);
	repair_stage_started = std::chrono::steady_clock::now();
	repair_stage_active = true;
	if (wrapper->Verbose()) {
	    std::map<std::string, double> current_mismatches;
	    std::map<std::pair<int, int>, int> source_use_ordinals;
	    std::vector<std::pair<double, std::string> > added_details;
	    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = brep->m_T[ti];
		const ON_BrepEdge *edge = trim.Edge();
		const ON_BrepLoop *loop = trim.Loop();
		const ON_BrepFace *face = trim.Face();
		const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
		if (!edge || !surface)
		    continue;
		const int loop_id = loop && loop->m_loop_user.i > 0 ?
		    loop->m_loop_user.i : -(trim.m_li + 1);
		const int edge_id = edge->m_edge_user.i > 0 ?
		    edge->m_edge_user.i : -(trim.m_ei + 1);
		const std::pair<int, int> source_key(loop_id, edge_id);
		const int use_ordinal = source_use_ordinals[source_key]++;
		std::ostringstream stable_key_stream;
		stable_key_stream << "L" << loop_id << "/E" << edge_id
		    << "/U" << use_ordinal;
		const std::string stable_key = stable_key_stream.str();
		double tolerance = std::max(LocalUnits::tolerance,
		    std::max(edge->m_tolerance,
			std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
		const ON_3dPoint start_uv = trim.PointAtStart();
		const ON_3dPoint end_uv = trim.PointAtEnd();
		const ON_3dPoint start_lift = surface->PointAt(start_uv.x,
		    start_uv.y);
		const ON_3dPoint end_lift = surface->PointAt(end_uv.x, end_uv.y);
		const ON_Interval edge_domain = edge->Domain();
		const ON_3dPoint edge_start = edge->PointAt(
		    edge_domain[trim.m_bRev3d ? 1 : 0]);
		const ON_3dPoint edge_end = edge->PointAt(
		    edge_domain[trim.m_bRev3d ? 0 : 1]);
		const double start_distance = start_lift.IsValid() &&
		    edge_start.IsValid() ? start_lift.DistanceTo(edge_start) :
		    DBL_MAX;
		const double end_distance = end_lift.IsValid() && edge_end.IsValid() ?
		    end_lift.DistanceTo(edge_end) : DBL_MAX;
		if (start_distance <= tolerance && end_distance <= tolerance)
		    continue;
		const double mismatch_ratio = std::max(start_distance,
		    end_distance) / std::max(tolerance, DBL_EPSILON);
		current_mismatches[stable_key] = mismatch_ratio;
		const std::map<std::string, double>::const_iterator previous =
		    previous_directed_endpoint_mismatches.find(stable_key);
		/* Report a new semantic use, or an existing discrepancy which becomes
		 * two orders of magnitude worse.  STEP loop/edge identities survive
		 * ON_Brep::Compact(), unlike trim array indices. */
		if (have_directed_endpoint_audit &&
			(previous == previous_directed_endpoint_mismatches.end() ||
			 mismatch_ratio > 100.0 * std::max(1.0,
			     previous->second))) {
		    std::ostringstream detail;
		    detail << ' ' << stable_key << "/T" << ti << '('
			<< start_distance << '/' << end_distance << "/tol="
			<< tolerance << ",ratio=" << mismatch_ratio << ')';
		    added_details.push_back(std::make_pair(mismatch_ratio,
			detail.str()));
		}
	    }
	    std::sort(added_details.begin(), added_details.end(),
		[](const std::pair<double, std::string> &first,
		    const std::pair<double, std::string> &second) {
		    return first.first > second.first;
		});
	if (have_directed_endpoint_audit && !added_details.empty()) {
		std::cerr << entity_type << " #" << entity_id << ": "
		    << added_details.size()
		    << " directed trim endpoint mismatch(es) appeared or worsened "
		    << "after " << previous_stage << ':';
		for (size_t added = 0; added < std::min<size_t>(8,
			added_details.size()); ++added)
		    std::cerr << added_details[added].second;
		std::cerr << std::endl;
	    }
	    previous_directed_endpoint_mismatches.swap(current_mismatches);
	    have_directed_endpoint_audit = true;

	    /* A periodic branch error can leave two trim endpoints coincident in
	     * model space while separating them by one complete surface period in
	     * parameter space.  The directed edge-locus audit above deliberately
	     * accepts that exact 3-D coincidence, but OpenNURBS still requires the
	     * cyclic p-space loop joins to be continuous.  Track those joins as a
	     * separate verbose-only stage signature so the phase which introduces
	     * a wrong periodic branch is observable without corpus-specific IDs. */
	    std::map<std::string, double> current_parameter_joins;
	    std::vector<std::pair<double, std::string> > added_parameter_details;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		const ON_BrepLoop &loop = brep->m_L[li];
		const ON_BrepFace *face = loop.Face();
		const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
		if (!surface || loop.TrimCount() < 1)
		    continue;
		const int loop_id = loop.m_loop_user.i > 0 ?
		    loop.m_loop_user.i : -(li + 1);
		std::map<std::pair<int, int>, int> join_ordinals;
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *current = loop.Trim(lti);
		    const ON_BrepTrim *next = loop.Trim(
			(lti + 1) % loop.TrimCount());
		    if (!current || !next)
			continue;
		    const ON_BrepEdge *current_edge = current->Edge();
		    const ON_BrepEdge *next_edge = next->Edge();
		    const int current_source = current_edge &&
			current_edge->m_edge_user.i > 0 ?
			current_edge->m_edge_user.i : 0;
		    const int next_source = next_edge &&
			next_edge->m_edge_user.i > 0 ?
			next_edge->m_edge_user.i : 0;
		    const std::pair<int, int> edge_pair(current_source,
			next_source);
		    const int ordinal = join_ordinals[edge_pair]++;
		    std::ostringstream key_stream;
		    key_stream << "L" << loop_id << "/E" << current_source
			<< "->E" << next_source << "/U" << ordinal;
		    const std::string stable_key = key_stream.str();
		    const ON_3dPoint current_end = current->PointAtEnd();
		    const ON_3dPoint next_start = next->PointAtStart();
		    const double gap = current_end.IsValid() && next_start.IsValid() ?
			current_end.DistanceTo(next_start) : DBL_MAX;
		    if (gap <= ON_ZERO_TOLERANCE)
			continue;
		    current_parameter_joins[stable_key] = gap;
		    const std::map<std::string, double>::const_iterator previous =
			previous_parameter_join_mismatches.find(stable_key);
		    if (!have_parameter_join_audit ||
			(previous != previous_parameter_join_mismatches.end() &&
			 gap <= 100.0 * std::max(ON_ZERO_TOLERANCE,
			     previous->second)))
			continue;
		    const ON_3dPoint current_lift = surface->PointAt(
			current_end.x, current_end.y);
		    const ON_3dPoint next_lift = surface->PointAt(
			next_start.x, next_start.y);
		    const double lift_gap = current_lift.IsValid() &&
			next_lift.IsValid() ? current_lift.DistanceTo(next_lift) :
			DBL_MAX;
		    std::ostringstream detail;
		    detail << ' ' << stable_key << "/T" << current->m_trim_index
			<< "->T" << next->m_trim_index << "(uv=" << gap
			<< ",lift=" << lift_gap << ')';
		    added_parameter_details.push_back(std::make_pair(gap,
			detail.str()));
		}
	    }
	    std::sort(added_parameter_details.begin(),
		added_parameter_details.end(),
		[](const std::pair<double, std::string> &first,
		    const std::pair<double, std::string> &second) {
		    return first.first > second.first;
		});
	    if (have_parameter_join_audit && !added_parameter_details.empty()) {
		std::cerr << entity_type << " #" << entity_id << ": "
		    << added_parameter_details.size()
		    << " parameter-space loop join mismatch(es) appeared or "
		    << "worsened after " << previous_stage << ':';
		for (size_t added = 0; added < std::min<size_t>(8,
			added_parameter_details.size()); ++added)
		    std::cerr << added_parameter_details[added].second;
		std::cerr << std::endl;
	    }
	    previous_parameter_join_mismatches.swap(current_parameter_joins);
	    have_parameter_join_audit = true;

	    /* A one-trim VERTEX_LOOP is easy to damage without producing an edge
	     * or cyclic-join discrepancy: both of its topology vertices are the
	     * same by definition.  Track its pcurve and ISO as a separate verbose
	     * signature so a generic repair which contracts a legitimate collapsed
	     * surface side is attributable to the exact preceding phase. */
	    std::map<int, std::string> current_single_singular_signatures;
	    for (int li = 0; li < brep->m_L.Count(); ++li) {
		const ON_BrepLoop &loop = brep->m_L[li];
		if (loop.TrimCount() != 1)
		    continue;
		const ON_BrepTrim *trim = loop.Trim(0);
		if (!trim || trim->m_type != ON_BrepTrim::singular)
		    continue;
		const int loop_id = loop.m_loop_user.i > 0 ?
		    loop.m_loop_user.i : -(li + 1);
		const ON_3dPoint start = trim->PointAtStart();
		const ON_3dPoint end = trim->PointAtEnd();
		std::ostringstream signature;
		signature << "iso=" << static_cast<int>(trim->m_iso) << ",uv="
		    << start.x << ':' << start.y << "->" << end.x << ':' << end.y;
		current_single_singular_signatures[loop_id] = signature.str();
		const std::map<int, std::string>::const_iterator previous =
		    previous_single_singular_signatures.find(loop_id);
		if (have_single_singular_audit &&
			(previous == previous_single_singular_signatures.end() ||
			 previous->second != signature.str()))
		    std::cerr << entity_type << " #" << entity_id
			<< ": singleton singular STEP loop #" << loop_id
			<< " changed after " << previous_stage << ": "
			<< (previous == previous_single_singular_signatures.end() ?
			    std::string("absent") : previous->second)
			<< " -> " << signature.str() << std::endl;
	    }
	    previous_single_singular_signatures.swap(
		current_single_singular_signatures);
	    have_single_singular_audit = true;
	    for (std::set<int>::const_iterator source =
		    tracked_same_loop_seam_sources.begin();
		    source != tracked_same_loop_seam_sources.end(); ++source) {
		const std::string current = source_topology_signature(*source);
		if (current == tracked_source_signatures[*source])
		    continue;
		std::cerr << entity_type << " #" << entity_id
		    << ": same-loop seam STEP edge #" << *source
		    << " topology changed after " << previous_stage << ':'
		    << tracked_source_signatures[*source] << " ->" << current
		    << std::endl;
		tracked_source_signatures[*source] = current;
	    }
	}
	previous_stage = phase;
	wrapper->SetProgressDetail(phase, entity_id, 0, 0, std::string(),
	    entity_type);
    };

    /* STEP ORIENTED_EDGE and ADVANCED_FACE senses describe two distinct
     * invariants.  ON_BrepTrim::m_bRev3d records the edge traversal inside
     * this loop, while ON_BrepFace::m_bRev records which side of the surface
     * is the material side.  Face reversal participates in closed-shell
     * orientation tests below, but it must not be folded into the original
     * trim traversal: doing so makes a false-sense face "restore" every
     * closed pcurve to the opposite direction and opens otherwise exact
     * periodic loops.  Retain the raw STEP edge-use sense by edge identity
     * and stable BREP face index.  A same-face seam pair has both senses under
     * one key and is deliberately left to the seam solver. */
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
	    face->m_face_index)].insert(trim.m_bRev3d);
    }

    /* Moving a periodic surface seam is exact, but it can disturb an already
     * valid choice of parameter branches elsewhere on the same surface.  Keep
     * it out of the ordinary repair pass and retry with seam alignment only
     * when the bounded repairs below still leave structurally invalid
     * topology. */
    /* Topology inference must not trust an unvalidated periodic branch.  In
     * particular, a half-circle may arrive with a pcurve on the complementary
     * half of a closed rational revolution surface; its endpoints still lift
     * correctly, but its interior does not follow the authoritative 3-D STEP
     * edge.  Repair those exact edge loci before interpreting net pcurve
     * winding as an implicit band or pole cut. */
    report_stage("materializing exact supplied periodic boundary cuts");
    split_supplied_full_period_boundaries(brep, wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled())
	return;
    /* Resolve a proven one-period join while the supplied pcurves still
     * retain their exact endpoint images.  Regenerating an individually
     * invalid curve first can select the opposite periodic branch and erase
     * the only local evidence identifying the boundary-free surface seam.
     * The relocation itself is transactional over every face sharing the
     * surface and densely revalidates all immutable 3-D STEP edges. */
    report_stage("relocating supplied exact closed-surface seam crossings");
    repair_closed_surface_seam_crossings(brep, wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled())
	return;
    report_stage("validating exact pcurves before periodic topology inference");
    repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type, true);
    if (brlcad::PullbackWorkCancelled())
	return;

    /* Some invalid exporters concatenate a full-period surface lobe and a
     * contractible face boundary at one repeated topology vertex, without a
     * reciprocal bridge edge.  This is not an exact keyhole normalization:
     * removing the noncontractible lobe is permitted only in the tagged,
     * whole-object-validated inference transaction. */
    report_stage("checking for repeated-vertex noncontractible STEP lobes");
    bool removed_periodic_lobe = true;
    while (removed_periodic_lobe) {
	removed_periodic_lobe =
	    remove_one_noncontractible_repeated_vertex_lobe(brep, wrapper,
		entity_id, entity_type);
	if (removed_periodic_lobe) {
	    refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
		entity_id, &entity_type);
	    classify_exact_polyline_seams(brep);
	}
	if (!wrapper->CurveInferenceTrialEnabled())
	    break;
    }
    if (brlcad::PullbackWorkCancelled())
	return;

    /* A malformed closed shell can insert the same nonzero dangling edge in
     * both neighboring loops.  The ordinary pass only marks that proof as an
     * inference candidate.  The permissive transaction may remove it, after
     * which all normal BREP and solidness validation still applies. */
    report_stage("checking for proven shared dangling STEP spurs");
    bool removed_dangling_spur = true;
    while (removed_dangling_spur) {
	removed_dangling_spur = remove_one_proven_shared_dangling_spur(brep,
	    wrapper, entity_id, entity_type);
	if (removed_dangling_spur) {
	    refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
		entity_id, &entity_type);
	    classify_exact_polyline_seams(brep);
	}
	if (!wrapper->CurveInferenceTrialEnabled())
	    break;
    }
    if (brlcad::PullbackWorkCancelled())
	return;

    /* Resolve integral-period choices on already complete STEP loops before
     * looking for compact implicit bands.  A closed rational revolution
     * patch can have an ordinary rectangular four-edge boundary whose exact
     * pcurves use both 0 and 2*pi.  Treating its one net winding as a missing
     * band seam first triggers unnecessary edge pullback and can construct
     * the wrong cap topology. */
    report_stage("solving initial coherent exact periodic loop branches");
    repair_exact_periodic_loop_branches(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled())
	return;
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
    /* A constant full-period boundary whose arbitrary pcurve start is the
     * native seam can require an exact two-piece isocurve split.  Defer that
     * stronger topology transaction until the surface-alignment retry: the
     * ordinary pass first gets every opportunity to resolve the same model by
     * moving a private seam, which avoids preempting coherent sibling-loop
     * branch selection on doubly-periodic bands. */
    if (allow_surface_alignment) {
	report_stage("retrying deferred exact periodic isocurve splits");
	repair_implicit_periodic_face_bands(brep, wrapper, entity_id,
	    entity_type, NULL, true);
	if (brlcad::PullbackWorkCancelled()) return;
	/* A shared-edge split can complete a sibling implicit band and merge its
	 * former FACE_OUTER_BOUND/FACE_BOUND loops.  Reclassify that new complete
	 * loop before generic trim flags interpret its paired seam as manifold. */
	repair_face_bound_classification(brep, wrapper, entity_id, entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
	for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	    ON_BrepFace &deferred_face = brep->m_F[fi];
	    if (deferred_face.m_li.Count() != 1)
		continue;
	    const int li = deferred_face.m_li[0];
	    if (li < 0 || li >= brep->m_L.Count() ||
		    brep->m_L[li].m_type == ON_BrepLoop::outer)
		continue;
	    brep->m_L[li].m_type = ON_BrepLoop::outer;
	    wrapper->RecordRepair(entity_id, entity_type,
		"face_bound",
		"classified a completed single-loop periodic face as outer");
	}
	/* The deferred split may compact loop arrays and merge two source bounds.
	 * Replace the pre-split classification snapshot so the final
	 * authoritative-type restoration does not reapply an obsolete inner
	 * classification to the newly completed outer loop. */
	expected_loop_types.clear();
	expected_loop_types.reserve(brep->m_L.Count());
	for (int li = 0; li < brep->m_L.Count(); ++li)
	    expected_loop_types.push_back(brep->m_L[li].m_type);
    }
    report_stage("repairing adjacent exact BREP topology vertices");
    repair_adjacent_trim_vertices(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing invalid exact open pcurves");
    std::set<int> validated_trim_loci;
    repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type, false,
	&validated_trim_loci);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing ordinary closed-surface seam crossings");
    if (repair_closed_surface_seam_crossings(brep, wrapper, entity_id,
	    entity_type, &validated_trim_loci))
	repair_invalid_open_pcurves(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing paired exact seam boundaries");
    repair_paired_seam_boundaries(brep, wrapper, entity_id, entity_type,
	&aligned_surface_loops);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("repairing missing exact singular trims");
    repair_missing_singular_trims(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    report_stage("selecting exact singular-pole periodic branches");
    repair_ambiguous_singular_periodic_loop_branches(brep, wrapper,
	entity_id, entity_type);
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
    report_stage("repairing adjacent exact periodic trim endpoints");
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
	const double orientation_tolerance = std::max(LocalUnits::tolerance,
	    std::max(edge.m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1])));

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
	    if (edge_distance > orientation_tolerance)
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
		    double locus_distance = std::min(lifted.DistanceTo(forward),
			lifted.DistanceTo(reverse));
		    if (locus_distance > orientation_tolerance) {
			double closest_parameter = 0.0;
			if (ON_NurbsCurve_GetClosestPoint(&closest_parameter,
				&edge_nurbs, lifted))
			    locus_distance = std::min(locus_distance,
				lifted.DistanceTo(edge_nurbs.PointAt(
				    closest_parameter)));
		    }
		    valid_reversal = locus_distance <= orientation_tolerance;
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
	/* A closed topology edge can have an open algebraic pcurve whose two
	 * endpoint chords both turn backward even though every well-conditioned
	 * interior sample proves the authored STEP direction.  Toggling m_bRev3d
	 * would corrupt a valid opposite-use edge pair, and preserving the curve
	 * leaves an OpenNURBS-invalid representation.  Reconstruct that narrow
	 * endpoint defect from the complete directed 3-D edge.  The edge-driven
	 * pullback corrects the endpoint chords without changing source sense and
	 * still requires dense edge/surface locus and direction validation. */
	const bool open_algebraic_pcurve =
	    trim.PointAtStart().DistanceTo(trim.PointAtEnd()) >
	    ON_ZERO_TOLERANCE;
	if (negative_endpoint_alignment[0] && negative_endpoint_alignment[1] &&
		interior_proves_current_orientation && open_algebraic_pcurve) {
	    /* A zero-winding contractible loop can cross a periodic surface seam
	     * while its independently fitted start and end retain a small residual.
	     * Anchor both ends to one exact edge/surface image so the regenerated
	     * pcurve is algebraically closed.  Do not apply this to an authored
	     * full-period traversal: that requires explicit paired seam topology. */
	    const ON_3dPoint supplied_start = trim.PointAtStart();
	    const ON_3dPoint supplied_end = trim.PointAtEnd();
	    bool has_closed_direction = false;
	    bool short_periodic_residual = supplied_start.IsValid() &&
		supplied_end.IsValid();
	    for (int direction = 0; short_periodic_residual && direction < 2;
		    ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		has_closed_direction = true;
		const double period = surface->Domain(direction).Length();
		short_periodic_residual = period > ON_ZERO_TOLERANCE &&
		    fabs(supplied_end[direction] - supplied_start[direction]) <
		    0.5 * period;
	    }
	    bool regenerated = false;
	    if (has_closed_direction && short_periodic_residual &&
		    trim.m_vi[0] >= 0 && trim.m_vi[0] < brep->m_V.Count()) {
		const ON_3dPoint edge_endpoint = edge.PointAt(
		    edge.Domain()[trim.m_bRev3d ? 1 : 0]);
		brlcad::PullbackContext context;
		ON_2dPoint projected_uv(supplied_start.x, supplied_start.y);
		ON_3dPoint projected_lift;
		double projected_distance = DBL_MAX;
		if (edge_endpoint.IsValid() &&
			context.SurfaceClosestPoint(surface, edge_endpoint,
			    projected_uv, projected_lift, projected_distance, 0,
			    std::max(ON_ZERO_TOLERANCE,
				orientation_tolerance * 0.1),
			    orientation_tolerance) &&
			projected_distance <= orientation_tolerance) {
		    ON_3dPoint required(projected_uv.x, projected_uv.y, 0.0);
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction))
			    continue;
			const double period = surface->Domain(direction).Length();
			if (period > ON_ZERO_TOLERANCE)
			    required[direction] += round((supplied_start[direction] -
				required[direction]) / period) * period;
		    }
		    const ON_3dPoint required_lift = closed_surface_point_at(
			surface, required);
		    if (required_lift.IsValid() &&
			    required_lift.DistanceTo(edge_endpoint) <=
				orientation_tolerance &&
			    required_lift.DistanceTo(
				brep->m_V[trim.m_vi[0]].point) <=
				orientation_tolerance)
			regenerated = regenerate_trim_polyline(brep, trim,
			    surface, edge_nurbs, orientation_tolerance,
			    &regeneration_failure, NULL, &required, &required,
			    true, wrapper, true);
		}
	    }
	    if (!regenerated)
		regenerated = regenerate_trim_polyline(brep, trim, surface,
		    edge_nurbs, orientation_tolerance, &regeneration_failure,
		    NULL, NULL, NULL, true, wrapper);
	    if (regenerated) {
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "regenerated backward endpoint chords from the exact directed closed edge");
		continue;
	    }
	}
	if ((negative_endpoint_alignment[0] || negative_endpoint_alignment[1]) &&
		!interior_proves_current_orientation &&
		regenerate_trim_polyline(brep, trim, surface, edge_nurbs,
		    orientation_tolerance, &regeneration_failure, NULL, NULL, NULL,
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
    repair_ambiguous_singular_periodic_loop_branches(brep, wrapper,
	entity_id, entity_type);
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
    report_stage("normalizing exact paired-seam periodic loops");
    normalize_exact_paired_seam_loops_to_native_domain(brep, wrapper,
	entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    /* Closed-edge regeneration and the final coherent branch solve can be
     * the first stages which expose an authored noncontractible boundary as
     * one exact period of UV travel.  The initial implicit-band pass cannot
     * classify topology which is not visible yet.  Retry only the source
     * loops whose final join is proven one period apart in parameter space
     * and coincident at the immutable topology vertex in model space.  This
     * keeps the late pass scale-bounded while giving the transactional cap or
     * band constructor one opportunity to materialize the required cut. */
    std::set<int> final_intrinsic_periodic_source_loops;
    for (int li = 0; li < brep->m_L.Count(); ++li) {
	const ON_BrepLoop &loop = brep->m_L[li];
	if (loop.m_loop_user.i <= 0 || loop.TrimCount() < 1 ||
		brlcad::step::LoopHasCompletePeriodicPoleTopology(loop))
	    continue;
	const ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	const ON_BrepTrim *first = loop.Trim(0);
	const ON_BrepTrim *last = loop.Trim(loop.TrimCount() - 1);
	if (!surface || (!surface->IsClosed(0) && !surface->IsClosed(1)) ||
		!first || !last)
	    continue;
	double parameter_tolerance = ON_ZERO_TOLERANCE *
	    kNumericalToleranceScale;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    parameter_tolerance = std::max(parameter_tolerance,
		kPeriodicParameterSnapFraction * std::max(1.0,
		    surface->Domain(direction).Length()));
	}
	bool proven_periodic_join = periodic_loop_closure(brep, &loop, last,
	    first, parameter_tolerance);
	/* Selection for the transactional retry may use the same measured local
	 * topology tolerance that OpenNURBS will use.  Some authored edge chains
	 * close in model space while independently generated endpoint parameters
	 * drift slightly beyond the numerical period tolerance.  This does not
	 * authorize a cut: it only schedules the exact edge-chain proof below. */
	if (!proven_periodic_join && last->m_vi[1] >= 0 &&
		last->m_vi[1] == first->m_vi[0] &&
		last->m_vi[1] < brep->m_V.Count()) {
	    const ON_3dPoint last_uv = last->PointAtEnd();
	    const ON_3dPoint first_uv = first->PointAtStart();
	    bool nearest_single_period = false;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const double period = surface->Domain(direction).Length();
		const double travel = fabs(last_uv[direction] -
		    first_uv[direction]);
		nearest_single_period = nearest_single_period ||
		    (period > ON_ZERO_TOLERANCE && travel > 0.5 * period &&
		     travel < 1.5 * period);
	    }
	    double topology_tolerance = LocalUnits::tolerance;
	    if (last->Edge())
		topology_tolerance = std::max(topology_tolerance,
		    last->Edge()->m_tolerance);
	    if (first->Edge())
		topology_tolerance = std::max(topology_tolerance,
		    first->Edge()->m_tolerance);
	    topology_tolerance = std::max(topology_tolerance,
		std::max(last->m_tolerance[0], last->m_tolerance[1]));
	    topology_tolerance = std::max(topology_tolerance,
		std::max(first->m_tolerance[0], first->m_tolerance[1]));
	    topology_tolerance = std::max(topology_tolerance,
		brep->m_V[last->m_vi[1]].m_tolerance);
	    const ON_3dPoint last_lift = closed_surface_point_at(surface,
		last_uv);
	    const ON_3dPoint first_lift = closed_surface_point_at(surface,
		first_uv);
	    const ON_3dPoint &vertex = brep->m_V[last->m_vi[1]].point;
	    proven_periodic_join = nearest_single_period && last_lift.IsValid() &&
		first_lift.IsValid() && vertex.IsValid() &&
		last_lift.DistanceTo(vertex) <= topology_tolerance &&
		first_lift.DistanceTo(vertex) <= topology_tolerance &&
		last_lift.DistanceTo(first_lift) <= topology_tolerance;
	}
	if (proven_periodic_join)
	    final_intrinsic_periodic_source_loops.insert(loop.m_loop_user.i);
    }
    if (!final_intrinsic_periodic_source_loops.empty()) {
	report_stage("retrying final exact implicit periodic cuts");
	repair_implicit_periodic_face_bands(brep, wrapper, entity_id,
	    entity_type, &final_intrinsic_periodic_source_loops);
	if (brlcad::PullbackWorkCancelled()) return;
	repair_face_bound_classification(brep, wrapper, entity_id,
	    entity_type);
	repair_exact_periodic_loop_branches(brep, wrapper, entity_id,
	    entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
	/* A band transaction may merge and compact loops.  Refresh the
	 * authoritative classification snapshot so the later orientation pass
	 * cannot restore a pre-transaction type onto an unrelated loop index. */
	expected_loop_types.clear();
	expected_loop_types.reserve(brep->m_L.Count());
	for (int li = 0; li < brep->m_L.Count(); ++li)
	    expected_loop_types.push_back(brep->m_L[li].m_type);
    }
    repair_single_singular_periodic_loop_branches(brep, wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_multi_singular_periodic_loop_branches(brep, wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    /* A required pole connector can become a closed, zero-topology winding
     * after the final coherent branch solve brings its ordinary neighbours to
     * the same parameter image.  Remove that now-redundant connector before
     * OpenNURBS validates every trim in the array. */
    report_stage("removing redundant exact singular connectors");
    remove_redundant_closed_singular_trims(brep, wrapper, entity_id,
	entity_type);
    if (brlcad::PullbackWorkCancelled()) return;

    /* Pairwise endpoint sweeps can oscillate when independently supplied
     * periodic boundary edges disagree within a previously measured source
     * tolerance.  Close both uses of their shared seam transactionally after
     * all edge regeneration has settled. */
    repair_paired_seam_loop_endpoints(brep, wrapper, entity_id, entity_type);
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
    refresh_brep_flags_preserving_singular_isos(brep, true, wrapper,
	entity_id, &entity_type);
    if (brlcad::PullbackWorkCancelled()) return;
	report_stage("restoring exact periodic loop orientations");
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
		const bool edge_use = trim->m_bRev3d;
		before += edge_use != *expected->second.begin();
		after += !edge_use != *expected->second.begin();
	    }
	};
	const auto loop_maximum_parameter_join_gap = [brep](int loop_index) {
	    if (loop_index < 0 || loop_index >= brep->m_L.Count())
		return DBL_MAX;
	    const ON_BrepLoop &loop = brep->m_L[loop_index];
	    if (loop.TrimCount() < 1)
		return DBL_MAX;
	    double maximum_gap = 0.0;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *current = loop.Trim(lti);
		const ON_BrepTrim *next = loop.Trim(
		    (lti + 1) % loop.TrimCount());
		if (!current || !next)
		    return DBL_MAX;
		const ON_3dPoint current_end = current->PointAtEnd();
		const ON_3dPoint next_start = next->PointAtStart();
		if (!current_end.IsValid() || !next_start.IsValid())
		    return DBL_MAX;
		maximum_gap = std::max(maximum_gap,
		    current_end.DistanceTo(next_start));
	    }
	    return maximum_gap;
	};
	const auto loop_parameter_join_signature = [brep](int loop_index) {
	    std::map<std::string, double> joins;
	    if (loop_index < 0 || loop_index >= brep->m_L.Count())
		return joins;
	    const ON_BrepLoop &loop = brep->m_L[loop_index];
	    std::map<std::pair<int, int>, int> ordinals;
	    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *current = loop.Trim(lti);
		const ON_BrepTrim *next = loop.Trim(
		    (lti + 1) % loop.TrimCount());
		const ON_BrepEdge *current_edge =
		    current ? current->Edge() : NULL;
		const ON_BrepEdge *next_edge = next ? next->Edge() : NULL;
		if (!current || !next || !current_edge || !next_edge)
		    continue;
		const int current_source = current_edge->m_edge_user.i > 0 ?
		    current_edge->m_edge_user.i : -(current->m_ei + 1);
		const int next_source = next_edge->m_edge_user.i > 0 ?
		    next_edge->m_edge_user.i : -(next->m_ei + 1);
		const std::pair<int, int> edge_pair(current_source, next_source);
		const int ordinal = ordinals[edge_pair]++;
		const ON_3dPoint current_end = current->PointAtEnd();
		const ON_3dPoint next_start = next->PointAtStart();
		if (!current_end.IsValid() || !next_start.IsValid())
		    continue;
		std::ostringstream key;
		key << current_source << "->" << next_source << '/' << ordinal;
		joins[key.str()] = current_end.DistanceTo(next_start);
	    }
	    return joins;
	};
	bool loop_orientation_changed = false;
	std::set<StepClosedEdgeUseKey> shell_orientation_overrides;
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
	    /* A strict reduction in reciprocal shell conflicts is a global proof
	     * that the local STEP FACE_BOUND/ORIENTED_EDGE combination is
	     * inconsistent.  In that one case closed-shell orientability outranks
	     * the original edge-use bit; otherwise never increase authoritative
	     * STEP-use disagreement. */
	    const bool shell_orientation_improves = after < before;
	    if (after > before ||
		    (step_after > step_before && !shell_orientation_improves)) {
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
	    const double join_gap_before =
		loop_maximum_parameter_join_gap(li);
	    const std::map<std::string, double> joins_before =
		loop_parameter_join_signature(li);
	    brep->FlipLoop(loop);
	    const double join_gap_after =
		loop_maximum_parameter_join_gap(li);
	    const std::map<std::string, double> joins_after =
		loop_parameter_join_signature(li);
	    ON_wString flipped_loop_messages;
	    ON_TextLog flipped_loop_log(flipped_loop_messages);
	    const bool flipped_loop_valid = loop.IsValid(&flipped_loop_log);
	    const double permitted_join_gap = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		100.0 * join_gap_before);
	    bool stable_join_worsened = false;
	    std::string worsened_join;
	    for (std::map<std::string, double>::const_iterator join =
		    joins_after.begin(); join != joins_after.end(); ++join) {
		if (join->second <= ON_ZERO_TOLERANCE *
			kNumericalToleranceScale)
		    continue;
		const std::map<std::string, double>::const_iterator previous =
		    joins_before.find(join->first);
		if (previous != joins_before.end() &&
			join->second <= 100.0 * std::max(ON_ZERO_TOLERANCE,
			    previous->second))
		    continue;
		stable_join_worsened = true;
		worsened_join = join->first;
		break;
	    }
	    /* A loop reversal is a topology repair, not permission to move one
	     * trim onto a different periodic image.  FlipLoop can preserve every
	     * 3-D edge-use invariant while opening an otherwise exact UV cycle by
	     * one complete surface period.  Reject that candidate at the decision
	     * point; final OpenNURBS validation is too late to recover which
	     * orientation change caused the branch discontinuity. */
	    if (!flipped_loop_valid || stable_join_worsened ||
		    !std::isfinite(join_gap_after) ||
		    join_gap_after > permitted_join_gap) {
		brep->FlipLoop(loop);
		if (wrapper->Verbose()) {
		    std::cerr << entity_type << " #" << entity_id
			<< ": declined loop " << li << "/STEP"
			<< loop.m_loop_user.i << " orientation reversal because "
			<< (!flipped_loop_valid ?
			    "the reversed loop was structurally invalid" :
			    stable_join_worsened ?
			    "a stable STEP edge-pair UV join appeared or worsened" :
			    "its maximum parameter-space join gap would worsen")
			<< " (gap " << join_gap_before << "->" << join_gap_after
			<< (stable_join_worsened ?
			    ", join " + worsened_join : std::string())
			<< ')' << std::endl;
		}
		continue;
	    }
	    loop_orientation_changed = true;
	    if (shell_orientation_improves) {
		/* The trial proves the relationship between both uses of every
		 * affected edge, not just the use in the loop being flipped.  If
		 * later STEP-sense restoration is suppressed only on this side, it
		 * can toggle the reciprocal closed-edge use and recreate exactly the
		 * shell conflict the trial removed.  Protect both proven uses as one
		 * constraint. */
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = loop.Trim(lti);
		    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		    if (!edge || edge->m_edge_user.i <= 0)
			continue;
		    for (int eti = 0; eti < edge->m_ti.Count(); ++eti) {
			const ON_BrepTrim *edge_trim = brep->Trim(edge->m_ti[eti]);
			const ON_BrepFace *edge_face = edge_trim ?
			    edge_trim->Face() : NULL;
			if (!edge_trim || !edge_face || edge_face->m_face_index < 0)
			    continue;
			const StepClosedEdgeUseKey key(edge->m_edge_user.i,
			    edge_face->m_face_index);
			if (expected_closed_edge_uses.find(key) !=
				expected_closed_edge_uses.end())
			    shell_orientation_overrides.insert(key);
		    }
		}
	    }
	    wrapper->RecordRepair(entity_id, entity_type, "loop_orientation",
		shell_orientation_improves ?
		"corrected a demonstrably inconsistent STEP loop orientation from closed-shell edge-use constraints" :
		"restored a periodic loop orientation without increasing closed-shell edge-use conflicts");
	}
	if (loop_orientation_changed)
	    refresh_brep_flags_preserving_singular_isos(brep, true, wrapper,
		entity_id, &entity_type);
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
	report_stage("checking final exact closed-trim orientations");
	/* Endpoint repair can alter the endpoint tangent of a closed pcurve.
	 * Perform the final exact tangent check after all such repairs, then
	 * refresh the derived trim state once more. */
	if (!allow_surface_alignment)
	    repair_final_closed_trim_orientations(brep, wrapper, entity_id,
		entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
	report_stage("restoring authoritative closed STEP edge uses");
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
		    expected->second.size() != 1 ||
		    shell_orientation_overrides.find(expected->first) !=
			shell_orientation_overrides.end())
		continue;
	    const bool desired = *expected->second.begin();
	    if (trim.m_bRev3d == desired)
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
	report_stage("restoring authoritative split STEP edge-use chains");

	/* Splitting a closed STEP edge opens each child at the periodic seam, so the
	 * closed-trim restoration above no longer recognizes it.  Retain the same
	 * authoritative pre-repair sense for the complete child chain.  Periodic
	 * loop joining may embed that chain as one closed lobe of a larger keyhole
	 * loop; reversing the whole face or whole loop would disturb unrelated
	 * exact edges.  Reverse only a single contiguous, closed, all-mismatching
	 * source-edge subchain.  Reversing its pcurves, order, and edge-use flags
	 * together preserves every 3-D locus and both attachment points. */
	size_t restored_split_step_edge_uses = 0;
	std::unique_ptr<ON_Brep> split_step_use_rollback;
	std::set<int> split_step_use_faces;
	for (std::map<StepClosedEdgeUseKey, std::set<bool> >::const_iterator expected =
		expected_closed_edge_uses.begin();
		expected != expected_closed_edge_uses.end(); ++expected) {
	    if (expected->second.size() != 1 ||
		    shell_orientation_overrides.find(expected->first) !=
			shell_orientation_overrides.end())
		continue;
	    const int source_edge_id = expected->first.first;
	    const int face_index = expected->first.second;
	    size_t split_edge_count = 0;
	    size_t conflicting_split_edges = 0;
	    bool complete_reciprocal_split = true;
	    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
		const ON_BrepEdge &source_edge = brep->m_E[ei];
		if (source_edge.m_edge_user.i != source_edge_id)
		    continue;
		++split_edge_count;
		if (source_edge.m_ti.Count() != 2) {
		    complete_reciprocal_split = false;
		    continue;
		}
		const ON_BrepTrim *first = brep->Trim(source_edge.m_ti[0]);
		const ON_BrepTrim *second = brep->Trim(source_edge.m_ti[1]);
		const ON_BrepFace *first_face = first ? first->Face() : NULL;
		const ON_BrepFace *second_face = second ? second->Face() : NULL;
		if (!first || !second || !first_face || !second_face ||
			first_face == second_face) {
		    complete_reciprocal_split = false;
		    continue;
		}
		const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
		const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
		conflicting_split_edges += first_effective == second_effective;
	    }
	    /* Authoritative sense restoration is warranted only for the exact
	     * failure it repairs: every child of one closed STEP edge is a
	     * reciprocal two-face use and every child currently has equal effective
	     * directions.  A merely duplicated source id is not evidence that a
	     * valid periodic boundary should be reversed. */
	    if (split_edge_count < 2 || !complete_reciprocal_split ||
		    conflicting_split_edges != split_edge_count)
		continue;

	    int loop_index = -1;
	    std::vector<int> selected_offsets;
	    bool all_mismatch = true;
	    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = brep->m_T[ti];
		const ON_BrepEdge *edge = trim.Edge();
		const ON_BrepFace *face = trim.Face();
		if (!edge || !face || edge->m_edge_user.i != source_edge_id ||
			face->m_face_index != face_index)
		    continue;
		if (loop_index < 0)
		    loop_index = trim.m_li;
		if (trim.m_li != loop_index ||
			trim.m_bRev3d == *expected->second.begin()) {
		    all_mismatch = false;
		    break;
		}
		const ON_BrepLoop *loop = trim.Loop();
		if (!loop) {
		    all_mismatch = false;
		    break;
		}
		for (int lti = 0; lti < loop->TrimCount(); ++lti)
		    if (loop->m_ti[lti] == ti) {
			selected_offsets.push_back(lti);
			break;
		    }
	    }
	    if (!all_mismatch || loop_index < 0 ||
		    loop_index >= brep->m_L.Count() ||
		    selected_offsets.size() != split_edge_count)
		continue;

	    ON_BrepLoop &loop = brep->m_L[loop_index];
	    std::vector<unsigned char> selected(
		static_cast<size_t>(loop.TrimCount()), 0);
	    for (std::vector<int>::const_iterator offset = selected_offsets.begin();
		    offset != selected_offsets.end(); ++offset) {
		if (*offset < 0 || *offset >= loop.TrimCount()) {
		    all_mismatch = false;
		    break;
		}
		selected[static_cast<size_t>(*offset)] = 1;
	    }
	    if (!all_mismatch)
		continue;
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
	    if (selected_offsets.size() == static_cast<size_t>(loop.TrimCount())) {
		chain_start = 0;
		chain_starts = 1;
	    }
	    if (chain_starts != 1)
		continue;

	    std::vector<int> chain_offsets;
	    std::vector<int> chain_trims;
	    for (size_t member = 0; member < selected_offsets.size(); ++member) {
		const int offset = (chain_start + static_cast<int>(member)) %
		    loop.TrimCount();
		if (!selected[static_cast<size_t>(offset)]) {
		    all_mismatch = false;
		    break;
		}
		chain_offsets.push_back(offset);
		chain_trims.push_back(loop.m_ti[offset]);
	    }
	    if (!all_mismatch || chain_trims.empty())
		continue;
	    bool closed_chain = true;
	    for (size_t member = 0; member < chain_trims.size(); ++member) {
		const ON_BrepTrim *current = brep->Trim(chain_trims[member]);
		const ON_BrepTrim *next = brep->Trim(
		    chain_trims[(member + 1) % chain_trims.size()]);
		if (!current || !next || current->m_vi[1] != next->m_vi[0]) {
		    closed_chain = false;
		    break;
		}
	    }
	    if (!closed_chain)
		continue;

	    /* The first accepted reversal starts one transaction covering all
	     * split-edge sense restorations.  Reversing a closed child chain is
	     * topologically exact, but on a periodic face its coincident topology
	     * vertex can have two distinct UV images.  Later branch regeneration
	     * must not be allowed to exchange those images and open a face loop. */
	    if (!split_step_use_rollback)
		split_step_use_rollback.reset(new ON_Brep(*brep));
	    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	    bool reversed_chain = true;
	    for (std::vector<int>::const_iterator trim_index = chain_trims.begin();
		    trim_index != chain_trims.end(); ++trim_index) {
		ON_BrepTrim &trim = brep->m_T[*trim_index];
		ON_Curve *reversed = trim.DuplicateCurve();
		if (!reversed || !reversed->Reverse() ||
			!reversed->ChangeDimension(2) || !reversed->IsValid()) {
		    delete reversed;
		    reversed_chain = false;
		    break;
		}
		const int c2_index = brep->AddTrimCurve(reversed);
		if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index)) {
		    if (c2_index < 0)
			delete reversed;
		    reversed_chain = false;
		    break;
		}
		trim.m_bRev3d = !trim.m_bRev3d;
		std::swap(trim.m_vi[0], trim.m_vi[1]);
	    }
	    if (reversed_chain) {
		for (size_t member = 0; member < chain_offsets.size(); ++member)
		    loop.m_ti[chain_offsets[member]] =
			chain_trims[chain_trims.size() - member - 1];
		std::string unsafe_topology;
		reversed_chain = brep_topology_references_are_safe(brep,
		    &unsafe_topology);
	    }
	    if (!reversed_chain) {
		*brep = *rollback;
		continue;
	    }
	    if (wrapper->Verbose()) {
		const ON_BrepFace &source_face = brep->m_F[face_index];
		std::cerr << entity_type << " #" << entity_id
		    << ": restored split STEP edge #" << source_edge_id
		    << " chain on face F" << face_index;
		if (source_face.m_face_user.i > 0)
		    std::cerr << "/STEP" << source_face.m_face_user.i;
		std::cerr << " loop L" << loop_index;
		if (loop.m_loop_user.i > 0)
		    std::cerr << "/STEP" << loop.m_loop_user.i;
		std::cerr << " trims=" << chain_trims.size() << std::endl;
	    }
	    split_step_use_faces.insert(face_index);
	    ++restored_split_step_edge_uses;
	}
	if (restored_split_step_edge_uses) {
	    /* Reversing a closed lobe preserves its surface lift but exchanges the
	     * periodic images at its two attachment points.  Reconstruct the whole
	     * affected loop from its already detached exact edges so adjacent UVs
	     * select one coherent unwrapped branch. */
	    regenerate_periodic_loop_chains(brep, wrapper, entity_id, entity_type);
	    if (brlcad::PullbackWorkCancelled()) return;
	    repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
	    if (brlcad::PullbackWorkCancelled()) return;

	    /* Compare only the faces touched by this transaction.  The surrounding
	     * BREP may contain an independent source-invalid face, so whole-BREP
	     * validity is not a usable rollback criterion here.  Preserve any
	     * pre-existing invalidity, but reject a newly invalid affected face or
	     * any increase in its cyclic parameter-space join gaps. */
	    const auto face_join_gaps = [](const ON_Brep &candidate, int fi,
		    size_t &count, double &maximum) {
		count = 0;
		maximum = 0.0;
		if (fi < 0 || fi >= candidate.m_F.Count())
		    return false;
		const ON_BrepFace &face = candidate.m_F[fi];
		for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		    const int li = face.m_li[fli];
		    if (li < 0 || li >= candidate.m_L.Count())
			return false;
		    const ON_BrepLoop &loop = candidate.m_L[li];
		    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
			const ON_BrepTrim *current = loop.Trim(lti);
			const ON_BrepTrim *next = loop.Trim(
			    (lti + 1) % loop.TrimCount());
			if (!current || !next)
			    return false;
			const ON_3dPoint end = current->PointAtEnd();
			const ON_3dPoint start = next->PointAtStart();
			const double gap = end.IsValid() && start.IsValid() ?
			    end.DistanceTo(start) : DBL_MAX;
			if (gap > ON_ZERO_TOLERANCE) {
			    ++count;
			    maximum = std::max(maximum, gap);
			}
		    }
		}
		return true;
	    };
	    bool split_restoration_valid = split_step_use_rollback.get() != NULL;
	    for (std::set<int>::const_iterator fi = split_step_use_faces.begin();
		    split_restoration_valid && fi != split_step_use_faces.end(); ++fi) {
		if (*fi < 0 || *fi >= split_step_use_rollback->m_F.Count() ||
			*fi >= brep->m_F.Count()) {
		    split_restoration_valid = false;
		    break;
		}
		ON_wString before_messages;
		ON_TextLog before_log(before_messages);
		ON_wString after_messages;
		ON_TextLog after_log(after_messages);
		const bool before_valid =
		    split_step_use_rollback->m_F[*fi].IsValid(&before_log);
		const bool after_valid = brep->m_F[*fi].IsValid(&after_log);
		size_t before_gap_count = 0;
		size_t after_gap_count = 0;
		double before_maximum_gap = 0.0;
		double after_maximum_gap = 0.0;
		const bool before_audited = face_join_gaps(
		    *split_step_use_rollback, *fi, before_gap_count,
		    before_maximum_gap);
		const bool after_audited = face_join_gaps(*brep, *fi,
		    after_gap_count, after_maximum_gap);
		const bool join_regression = !before_audited || !after_audited ||
		    after_gap_count > before_gap_count ||
		    after_maximum_gap > std::max(100.0 * ON_ZERO_TOLERANCE,
			100.0 * before_maximum_gap);
		const bool trim_regression =
		    brlcad::step::FaceTrimValidationRegressed(
			*split_step_use_rollback, *brep, *fi,
			LocalUnits::tolerance);
		if (trim_regression && wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": split STEP edge-use restoration changed directed "
			<< "trim validation on face F" << *fi << "/STEP"
			<< brep->m_F[*fi].m_face_user.i << std::endl;
		if ((before_valid && !after_valid) || join_regression ||
			trim_regression)
		    split_restoration_valid = false;
	    }
	    if (!split_restoration_valid) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": rolled back split STEP edge-use restoration "
			<< "transaction" << std::endl;
		*brep = *split_step_use_rollback;
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Information, entity_id,
		    entity_type, "trim_orientation",
		    "declined split closed-edge sense restoration because it "
		    "degraded an exact periodic face loop");
		restored_split_step_edge_uses = 0;
	    }
	}
	for (size_t restored = 0; restored < restored_split_step_edge_uses;
		restored++)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_orientation",
		"restored an authoritative closed STEP edge-use orientation across an exact split subchain");
	report_stage("refreshing final exact seam topology");
	/* The completed STEP face-bound graph is authoritative when an exporter
	 * omitted FACE_OUTER_BOUND.  The final closed-edge tangent pass does not
	 * reverse pcurves, so refreshing loop types here would only discard the
	 * unambiguous bound classification restored above. */
	refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
	    entity_id, &entity_type);
	/* The generic OpenNURBS refresh derives seam ISO and trim type together.
	 * On an edge with additional uses elsewhere in the BREP it can downgrade
	 * one member of an otherwise exact same-loop seam pair to x_iso/y_iso.
	 * Re-run the transactional exact-edge seam construction in the final
	 * position so its densely validated complementary boundary flags are not
	 * overwritten before structural validation. */
	repair_seam_pair_from_exact_edge(brep, wrapper, entity_id, entity_type);
	normalize_exact_paired_seam_loops_to_native_domain(brep, wrapper,
	    entity_id, entity_type);
	/* Loop-orientation restoration can exchange the periodic image of one seam
	 * member after the earlier endpoint pass.  Reconcile the seam and all four
	 * adjacent endpoints transactionally here, after the final generic flag
	 * refresh, so no later branch propagation can put both uses on one side. */
	repair_paired_seam_loop_endpoints(brep, wrapper, entity_id, entity_type);
	/* Rebuilding that final seam pair can deliberately move it to the native
	 * opposite boundaries after the earlier loop-wide endpoint sweep.  Its
	 * ordinary neighbours may consequently remain on the old, lift-equivalent
	 * periodic image.  Give those non-seam curves one final integral-period
	 * translation and densely validated endpoint reconciliation before
	 * structural validation. */
	repair_adjacent_trim_endpoints(brep, wrapper, entity_id, entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
	repair_single_singular_periodic_loop_branches(brep, wrapper,
	    entity_id, entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
	repair_multi_singular_periodic_loop_branches(brep, wrapper,
	    entity_id, entity_type);
	if (brlcad::PullbackWorkCancelled()) return;
	/* The final seam and endpoint passes above can replace trim curves after the
	 * last generic OpenNURBS flag refresh.  Their topology and ISO flags are
	 * deliberately authoritative, but m_pbox is only a derived cache and must
	 * follow the installed curves.  Refresh boxes alone: rerunning the generic
	 * flag classifier here can downgrade a densely proven paired seam. */
	report_stage("refreshing final exact trim bounding boxes");
	brep->SetTrimBoundingBoxes(false);
	if (brlcad::PullbackWorkCancelled()) return;
	report_stage("validating completed bounded trim repair");
	if (!allow_surface_alignment) {
	    ON_wString validation_messages;
	    ON_TextLog validation_log(validation_messages);
	    /* This is an intermediate branch-selection probe, not the final
	     * diagnostic validation.  Do not retain a report which no caller will
	     * inspect unless verbose diagnostics were explicitly requested; the
	     * caller performs the authoritative logged validation after all repair
	     * alternatives have been exhausted.  This does not make the OpenNURBS
	     * validity walk cheaper - the horn-torus fixture shows that the walk
	     * can be expensive after dense pcurve regeneration. */
	    bool bounded_trim_valid = brep->IsValid(
		wrapper->Verbose() ? &validation_log : NULL);
	    if (brlcad::PullbackWorkCancelled()) return;
	    if (!bounded_trim_valid) {
		/* Orientation restoration and the final seam refresh intentionally
		 * run after the ordinary whole-loop branch solve.  Either can exchange
		 * one periodic endpoint image while preserving every 3-D edge and
		 * topology vertex.  Retry coherent edge-driven loop regeneration only
		 * after structural validation proves it is needed, apply candidates
		 * serially on a private BREP, and commit only when the complete BREP
		 * validates.  This avoids repeatedly perturbing already-valid models
		 * and makes the last repair transaction the one OpenNURBS actually
		 * certifies. */
		report_stage("retrying final exact periodic loop chains");
		std::unique_ptr<ON_Brep> final_chain_candidate(new ON_Brep(*brep));
		const size_t final_chains = regenerate_periodic_loop_chains(
		    final_chain_candidate.get(), wrapper, entity_id, entity_type,
		    -1, false);
		if (brlcad::PullbackWorkCancelled()) return;
		if (final_chains) {
			    repair_adjacent_trim_endpoints(final_chain_candidate.get(),
				wrapper, entity_id, entity_type, -1, false);
			    if (brlcad::PullbackWorkCancelled()) return;
			    final_chain_candidate->SetTrimBoundingBoxes(false);
			    report_stage("validating final exact periodic loop-chain retry");
		    ON_wString final_chain_messages;
		    ON_TextLog final_chain_log(final_chain_messages);
		    if (final_chain_candidate->IsValid(
			    wrapper->Verbose() ? &final_chain_log : NULL)) {
			*brep = *final_chain_candidate;
			bounded_trim_valid = true;
			for (size_t chain = 0; chain < final_chains; ++chain)
			    wrapper->RecordRepair(entity_id, entity_type,
				"edge_loop",
				"reclosed a final periodic loop chain after all "
				"orientation and seam repairs");
		    } else if (wrapper->Verbose()) {
			ON_String final_chain_text(final_chain_messages);
			std::cerr << entity_type << " #" << entity_id
			    << ": final periodic loop-chain transaction rejected:\n"
			    << final_chain_text.Array();
		    }
		    if (brlcad::PullbackWorkCancelled()) return;
		}
	    }
	    if (!bounded_trim_valid) {
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
		    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
			const ON_BrepEdge &edge = brep->m_E[ei];
			if (edge.m_vi[0] != edge.m_vi[1] || edge.IsClosed())
			    continue;
			const ON_3dPoint start = edge.PointAtStart();
			const ON_3dPoint end = edge.PointAtEnd();
			const ON_BrepVertex *vertex = edge.Vertex(0);
			std::cerr << entity_type << " #" << entity_id
			    << ": invalid same-vertex BREP edge E" << ei
			    << "/STEP" << edge.m_edge_user.i << " C3="
			    << edge.m_c3i << " endpoint-gap="
			    << (start.IsValid() && end.IsValid() ?
				start.DistanceTo(end) : DBL_MAX);
			if (vertex && start.IsValid() && end.IsValid())
			    std::cerr << " vertex-gaps="
				<< start.DistanceTo(vertex->point) << '/'
				<< end.DistanceTo(vertex->point);
			std::cerr << std::endl;
		    }
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
		report_stage("retrying exact periodic surface alignment");
		repair_closed_trim_orientations(brep, wrapper, entity_id,
		    entity_type, true);
		if (brlcad::PullbackWorkCancelled()) return;
		report_stage("validating exact periodic surface-alignment retry");
		ON_wString aligned_messages;
		ON_TextLog aligned_log(aligned_messages);
		if (brep->IsValid(wrapper->Verbose() ? &aligned_log : NULL)) {
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"selected a valid retry with exact periodic surface seam alignment");
		    return;
		}
		if (brlcad::PullbackWorkCancelled()) return;
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
	    }
	}
	repair_stage_timing.Finish();
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
		const ON_BrepTrim *first = brep->Trim(ti);
		const ON_BrepEdge *edge = first ? first->Edge() : NULL;
		if (!first || !edge || first->m_type != ON_BrepTrim::seam)
		    continue;
		const ON_BrepTrim *second = NULL;
		for (int use = 0; use < edge->m_ti.Count(); ++use) {
		    const ON_BrepTrim *candidate = brep->Trim(edge->m_ti[use]);
		    if (candidate && candidate != first &&
			    candidate->m_type == ON_BrepTrim::seam &&
			    candidate->m_li == first->m_li) {
			second = candidate;
			break;
		    }
		}
		if (!second || first->m_trim_index > second->m_trim_index)
		    continue;
		const bool complementary =
		    (first->m_iso == ON_Surface::W_iso &&
		     second->m_iso == ON_Surface::E_iso) ||
		    (first->m_iso == ON_Surface::E_iso &&
		     second->m_iso == ON_Surface::W_iso) ||
		    (first->m_iso == ON_Surface::S_iso &&
		     second->m_iso == ON_Surface::N_iso) ||
		    (first->m_iso == ON_Surface::N_iso &&
		     second->m_iso == ON_Surface::S_iso);
		if (complementary)
		    continue;
		const ON_BoundingBox first_box = first->BoundingBox();
		const ON_BoundingBox second_box = second->BoundingBox();
		const ON_Surface *surface = first->Face() ?
		    first->Face()->SurfaceOf() : NULL;
		std::cerr << entity_type << " #" << entity_id
		    << ": mismatched seam edge " << first->m_ei << " trims "
		    << first->m_trim_index << '/' << second->m_trim_index
		    << " iso=" << static_cast<int>(first->m_iso) << '/'
		    << static_cast<int>(second->m_iso) << " uv-boxes="
		    << first_box.m_min.x << ':' << first_box.m_max.x << ','
		    << first_box.m_min.y << ':' << first_box.m_max.y << " / "
		    << second_box.m_min.x << ':' << second_box.m_max.x << ','
		    << second_box.m_min.y << ':' << second_box.m_max.y;
		if (surface)
		    std::cerr << " domains=" << surface->Domain(0).Min() << ':'
			<< surface->Domain(0).Max() << ','
			<< surface->Domain(1).Min() << ':'
			<< surface->Domain(1).Max();
		std::cerr << std::endl;
	    }
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
    int face_source_tag = 0;
    int original_marker = 0;
    int detached_marker = 0;
    int original_source_tag = 0;
    int detached_source_tag = 0;
    int paired_periodic_direction = -1;
    std::vector<int> original_edge_tags;
    std::vector<int> detached_edge_tags;
};


/* STEP permits an edge to be used forward and backward as a zero-area bridge
 * from an outer boundary to another closed boundary component.  OpenNURBS
 * calls two uses of one edge in one loop a seam and consequently requires
 * native N/E/S/W pcurves.  For a non-isoparametric bridge, its own source-edge
 * identity proves exact cancellation; split the two remaining topology cycles
 * and discard only the opposite bridge uses.  This helper is intentionally
 * candidate-only: the caller retains the result only if bounded pcurve repair
 * and OpenNURBS validation of the complete affected face neighborhood
 * subsequently succeed. */
bool
split_topological_keyhole_candidate(ON_Brep *brep,
    TopologicalKeyholeSplit *split)
{
    if (split)
	*split = TopologicalKeyholeSplit();
    if (!brep || !split)
	return false;

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
		    first->m_ei < 0 ||
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
		/* A damaged pair often has one member misclassified on a native
		 * boundary and the reciprocal member left as an interior seam, but
		 * both members can also be incorrectly projected onto nominal W/E
		 * boundaries.  Pair discovery must therefore be symmetric in loop
		 * order and ISO classification.  The independent endpoint and
		 * neighboring-lift proof below—not the cached ISO flags—excludes a
		 * healthy periodic-band connector. */
		const ON_Surface *surface = face->SurfaceOf();
		if (!surface)
		    continue;
		const auto endpoint_misses_topology = [brep, surface](
			const ON_BrepTrim &trim, int endpoint) {
		    const int vertex_index = trim.m_vi[endpoint];
		    if (vertex_index < 0 || vertex_index >= brep->m_V.Count())
			return true;
		    double tolerance = LocalUnits::tolerance;
		    const ON_BrepEdge *trim_edge = trim.Edge();
		    if (trim_edge)
			tolerance = std::max(tolerance,
			    trim_edge->m_tolerance);
		    tolerance = std::max(tolerance,
			std::max(trim.m_tolerance[0], trim.m_tolerance[1]));
		    tolerance = std::max(tolerance,
			brep->m_V[vertex_index].m_tolerance);
		    const ON_3dPoint uv = endpoint == 0 ?
			trim.PointAtStart() : trim.PointAtEnd();
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    return !lift.IsValid() || lift.DistanceTo(
			brep->m_V[vertex_index].point) > tolerance;
		};
		const auto bridge_join_misses = [brep, loop, surface](
			int offset) {
		    const ON_BrepTrim *bridge = loop->Trim(offset);
		    const ON_BrepTrim *previous = loop->Trim(
			(offset + loop->TrimCount() - 1) % loop->TrimCount());
		    const ON_BrepTrim *next = loop->Trim(
			(offset + 1) % loop->TrimCount());
		    if (!bridge || !previous || !next)
			return true;
		    const ON_3dPoint bridge_start = bridge->PointAtStart();
		    const ON_3dPoint bridge_end = bridge->PointAtEnd();
		    const ON_3dPoint previous_end = previous->PointAtEnd();
		    const ON_3dPoint next_start = next->PointAtStart();
		    const ON_3dPoint bridge_start_lift = surface->PointAt(
			bridge_start.x, bridge_start.y);
		    const ON_3dPoint bridge_end_lift = surface->PointAt(
			bridge_end.x, bridge_end.y);
		    const ON_3dPoint previous_lift = surface->PointAt(
			previous_end.x, previous_end.y);
		    const ON_3dPoint next_lift = surface->PointAt(
			next_start.x, next_start.y);
		    double start_tolerance = LocalUnits::tolerance;
		    double end_tolerance = LocalUnits::tolerance;
		    if (previous->Edge())
			start_tolerance = std::max(start_tolerance,
			    previous->Edge()->m_tolerance);
		    if (next->Edge())
			end_tolerance = std::max(end_tolerance,
			    next->Edge()->m_tolerance);
		    if (bridge->m_vi[0] >= 0 &&
			    bridge->m_vi[0] < brep->m_V.Count())
			start_tolerance = std::max(start_tolerance,
			    brep->m_V[bridge->m_vi[0]].m_tolerance);
		    if (bridge->m_vi[1] >= 0 &&
			    bridge->m_vi[1] < brep->m_V.Count())
			end_tolerance = std::max(end_tolerance,
			    brep->m_V[bridge->m_vi[1]].m_tolerance);
		    return !bridge_start_lift.IsValid() ||
			!bridge_end_lift.IsValid() || !previous_lift.IsValid() ||
			!next_lift.IsValid() ||
			bridge_start_lift.DistanceTo(previous_lift) >
			    start_tolerance ||
			bridge_end_lift.DistanceTo(next_lift) > end_tolerance;
		};
		const int inside_count = second_offset - first_offset - 1;
		const int outside_count = trim_count - inside_count - 2;
		const ON_BrepTrim *inside_singleton =
		    inside_count == 1 ? loop->Trim(first_offset + 1) : NULL;
		const ON_BrepTrim *outside_singleton = outside_count == 1 ?
		    loop->Trim((second_offset + 1) % trim_count) : NULL;
		const bool interior_bridge =
		    first->m_iso != ON_Surface::W_iso &&
		    first->m_iso != ON_Surface::E_iso &&
		    first->m_iso != ON_Surface::S_iso &&
		    first->m_iso != ON_Surface::N_iso &&
		    second->m_iso != ON_Surface::W_iso &&
		    second->m_iso != ON_Surface::E_iso &&
		    second->m_iso != ON_Surface::S_iso &&
		    second->m_iso != ON_Surface::N_iso;
		const bool two_closed_singleton_components = interior_bridge &&
		    inside_singleton && outside_singleton &&
		    inside_singleton->m_vi[0] == inside_singleton->m_vi[1] &&
		    outside_singleton->m_vi[0] == outside_singleton->m_vi[1];
		/* A healthy repeated edge may be the zero-area connector between
		 * two noncontractible boundaries of one periodic face band.
		 * Removing it and treating both cycles as independent faces is
		 * topologically wrong.  This late fallback is only for a bridge
		 * whose generated pcurve demonstrably misses its own immutable
		 * topology vertex; exact coincident bridges were already handled by
		 * the ordinary keyhole normalizer. */
		if (!two_closed_singleton_components &&
			!endpoint_misses_topology(*first, 0) &&
			!endpoint_misses_topology(*first, 1) &&
			!endpoint_misses_topology(*second, 0) &&
			!endpoint_misses_topology(*second, 1) &&
			!bridge_join_misses(first_offset) &&
			!bridge_join_misses(second_offset))
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
		const auto source_edge_tags = [brep](const std::vector<int> &trims) {
		    std::vector<int> tags;
		    for (std::vector<int>::const_iterator trim = trims.begin();
			    trim != trims.end(); ++trim) {
			if (*trim < 0 || *trim >= brep->m_T.Count())
			    continue;
			const ON_BrepEdge *source_edge =
			    brep->m_T[*trim].Edge();
			if (source_edge && source_edge->m_edge_user.i > 0)
			    tags.push_back(source_edge->m_edge_user.i);
		    }
		    std::sort(tags.begin(), tags.end());
		    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
		    return tags;
		};
		const std::vector<int> original_edge_tags =
		    source_edge_tags(outside);
		const std::vector<int> detached_edge_tags =
		    source_edge_tags(inside);
		if (original_edge_tags.empty() || detached_edge_tags.empty())
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

		/* DeleteTrim() and Compact() can renumber every loop after an
		 * already orphaned source loop.  Array indices captured before
		 * compaction are therefore not stable identities: using them in
		 * the transactional validation below can find the right geometry
		 * under the wrong face, or fail to find the detached loop at all.
		 * Give both candidate loops temporary, collision-free negative
		 * source tags, recover their compacted indices, then restore the
		 * externally meaningful source tags before returning. */
		int original_marker = -1;
		int detached_marker = -2;
		const auto marker_in_use = [brep](int marker) {
		    for (int marker_loop = 0; marker_loop < brep->m_L.Count();
			    ++marker_loop)
			if (brep->m_L[marker_loop].m_loop_user.i == marker)
			    return true;
		    return false;
		};
		while (marker_in_use(original_marker))
		    --original_marker;
		detached_marker = original_marker - 1;
		while (marker_in_use(detached_marker))
		    --detached_marker;
		const int original_source_tag = loop->m_loop_user.i;
		/* Both separated cycles came from the same authoritative STEP
		 * EDGE_LOOP.  Carry that ID on the synthetic detached cycle as
		 * well; the two immutable edge-ID sets distinguish the cycles,
		 * while the source-loop ID distinguishes either one from an
		 * adjacent face using the same solid edges. */
		const int detached_source_tag = original_source_tag;
		const int face_source_tag = face->m_face_user.i;
		loop->m_loop_user.i = original_marker;
		detached->m_loop_user.i = detached_marker;

		brep->m_T[first_trim_index].m_li = -1;
		brep->m_T[second_trim_index].m_li = -1;
		const int higher_trim = std::max(first_trim_index, second_trim_index);
		const int lower_trim = std::min(first_trim_index, second_trim_index);
		brep->DeleteTrim(brep->m_T[higher_trim], false);
		brep->DeleteTrim(brep->m_T[lower_trim], true);
		if (!brep->Compact())
		    return false;
		int compacted_original = -1;
		int compacted_detached = -1;
		for (int compacted_loop = 0; compacted_loop < brep->m_L.Count();
			++compacted_loop) {
		    if (brep->m_L[compacted_loop].m_loop_user.i == original_marker)
			compacted_original = compacted_loop;
		    else if (brep->m_L[compacted_loop].m_loop_user.i ==
			    detached_marker)
			compacted_detached = compacted_loop;
		}
		if (compacted_original < 0 || compacted_detached < 0)
		    return false;
		const int compacted_face = brep->m_L[compacted_original].m_fi;
		if (compacted_face < 0 || compacted_face >= brep->m_F.Count())
		    return false;
		/* The candidate precondition proved this face had exactly the one
		 * source loop.  Some OpenNURBS Compact() versions retain the newly
		 * created loop but drop its face-list entry while deleting the two
		 * orphaned bridge trims.  Rebuild this two-loop association from the
		 * recovered identities; no pre-existing face loop can be displaced
		 * under the one-loop precondition above. */
		brep->m_L[compacted_original].m_fi = compacted_face;
		brep->m_L[compacted_detached].m_fi = compacted_face;
		brep->m_F[compacted_face].m_li.SetCount(0);
		brep->m_F[compacted_face].m_li.Append(compacted_original);
		brep->m_F[compacted_face].m_li.Append(compacted_detached);
		split->original_loop = compacted_original;
		split->detached_loop = compacted_detached;
		split->face = compacted_face;
		split->face_source_tag = face_source_tag;
		split->original_marker = original_marker;
		split->detached_marker = detached_marker;
		split->original_source_tag = original_source_tag;
		split->detached_source_tag = detached_source_tag;
		split->original_edge_tags = original_edge_tags;
		split->detached_edge_tags = detached_edge_tags;
		return true;
	    }
	}
    }
    return false;
}


bool
retry_topological_keyhole_normalization(ON_Brep *brep, STEPWrapper *wrapper,
    int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper ||
	    wrapper->ImportOptions().exact ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe)
	return false;
    std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
    std::vector<TopologicalKeyholeSplit> splits;

    /* Select every independent bridge before bounded pcurve repair begins.
     * Later exact seam reconciliation can legitimately reclassify an
     * unsplit bridge, so discovery and topology separation are one phase.
     * All expensive repair below is restricted to this affected face set. */
    for (int attempt = 0; attempt < 32; ++attempt) {
	refresh_brep_flags_preserving_singular_isos(candidate.get(), false);
	TopologicalKeyholeSplit selected_split;
	if (!split_topological_keyhole_candidate(candidate.get(),
		&selected_split))
	    break;
	splits.push_back(selected_split);
    }
    if (!splits.empty() && wrapper->Verbose()) {
	int reported_candidates = 0;
	for (int ti = 0; ti < candidate->m_T.Count() &&
		reported_candidates < 16; ++ti) {
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
		<< ": remaining topological keyhole candidate trim " << ti
		<< " not selected after " << splits.size() << " split(s) (loop "
		<< trim.m_li << " type " << static_cast<int>(loop.m_type)
		<< " trims " << loop.TrimCount() << ", face loops "
		<< (face ? face->LoopCount() : 0) << ", edge uses "
		<< ((trim.m_ei >= 0 && trim.m_ei < candidate->m_E.Count()) ?
		    candidate->m_E[trim.m_ei].m_ti.Count() : 0)
		<< ", STEP edge "
		<< ((trim.m_ei >= 0 && trim.m_ei < candidate->m_E.Count()) ?
		    candidate->m_E[trim.m_ei].m_edge_user.i : 0) << ')'
		<< std::endl;
	    ++reported_candidates;
	}
    }
    if (splits.empty()) {
	if (wrapper->Verbose()) {
	    int reported_candidates = 0;
	    for (int ti = 0; ti < candidate->m_T.Count() &&
		    reported_candidates < 16; ++ti) {
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
			candidate->m_E[trim.m_ei].m_ti.Count() : 0)
		    << ", STEP edge "
		    << ((trim.m_ei >= 0 && trim.m_ei < candidate->m_E.Count()) ?
			candidate->m_E[trim.m_ei].m_edge_user.i : 0) << ')'
		    << std::endl;
		++reported_candidates;
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
    const auto report_open_topology_closed_pcurves =
	[&candidate, wrapper, entity_id, &entity_type](const char *stage) {
	    if (!wrapper->Verbose())
		return;
	    for (int ti = 0; ti < candidate->m_T.Count(); ++ti) {
		const ON_BrepTrim &trim = candidate->m_T[ti];
		const ON_Curve *curve = trim.TrimCurveOf();
		if (!curve || trim.m_vi[0] < 0 || trim.m_vi[1] < 0 ||
			trim.m_vi[0] == trim.m_vi[1] || !curve->IsClosed())
		    continue;
		const ON_BrepEdge *edge = trim.Edge();
		const ON_3dPoint start = curve->PointAtStart();
		const ON_3dPoint end = curve->PointAtEnd();
		std::cerr << entity_type << " #" << entity_id
		    << ": topological keyhole stage " << stage
		    << " produced closed pcurve T" << ti << "/STEP edge "
		    << (edge ? edge->m_edge_user.i : 0)
		    << " with distinct vertices " << trim.m_vi[0] << '/'
		    << trim.m_vi[1] << " in L" << trim.m_li << " endpoints "
		    << start.x << ':' << start.y << " -> "
		    << end.x << ':' << end.y << std::endl;
	    }
	};

    /* Every later split compacts the complete BREP and can renumber loops
     * created by every earlier split.  Resolve the temporary source tags only
     * after the complete split phase. */
    for (std::vector<TopologicalKeyholeSplit>::iterator split =
	    splits.begin(); split != splits.end(); ++split) {
	split->original_loop = -1;
	split->detached_loop = -1;
	for (int li = 0; li < candidate->m_L.Count(); ++li) {
	    if (candidate->m_L[li].m_loop_user.i == split->original_marker)
		split->original_loop = li;
	    else if (candidate->m_L[li].m_loop_user.i ==
		    split->detached_marker)
		split->detached_loop = li;
	}
	const int resolved_face = split->original_loop >= 0 ?
	    candidate->m_L[split->original_loop].m_fi : -1;
	if (split->original_loop < 0 || split->detached_loop < 0 ||
		resolved_face < 0 || resolved_face >= candidate->m_F.Count() ||
		candidate->m_L[split->detached_loop].m_fi != resolved_face ||
		candidate->m_F[resolved_face].m_face_user.i !=
		    split->face_source_tag)
	    return reject("a split loop identity was lost during compaction");
	split->face = resolved_face;
	candidate->m_L[split->original_loop].m_loop_user.i =
	    split->original_source_tag;
	candidate->m_L[split->detached_loop].m_loop_user.i =
	    split->detached_source_tag;
	candidate->m_L[split->original_loop].m_fi = split->face;
	candidate->m_L[split->detached_loop].m_fi = split->face;
	/* Each selected source face was proven to contain exactly one loop
	 * before this split.  Re-establish its two exact post-split members after
	 * the final global compaction, which may have discarded or failed to
	 * renumber the newly added face-list entry. */
	ON_BrepFace &source_face = candidate->m_F[split->face];
	source_face.m_li.SetCount(0);
	source_face.m_li.Append(split->original_loop);
	source_face.m_li.Append(split->detached_loop);
    }

    std::set<int> affected_face_source_tags;
    std::set<int> split_source_loops;
    for (std::vector<TopologicalKeyholeSplit>::const_iterator split =
	    splits.begin(); split != splits.end(); ++split) {
	if (split->face_source_tag > 0)
	    affected_face_source_tags.insert(split->face_source_tag);
	if (split->original_source_tag > 0)
	    split_source_loops.insert(split->original_source_tag);
    }
    if (affected_face_source_tags.empty())
	return reject("a split face lacked an authoritative STEP identity");

    /* The source candidate has already completed the ordinary bounded BREP
     * repair.  Re-running that whole workflow after removing keyhole bridges
     * can reinterpret the newly separated cycles as implicit periodic bands
     * and merge them back together.  Reconcile only the exposed cyclic joins:
     * choose coherent exact periodic images, match adjacent endpoints, then
     * repeat the branch proof once on the resulting curves. */
    repair_exact_periodic_loop_branches(candidate.get(), wrapper, entity_id,
	entity_type, &affected_face_source_tags);
    repair_adjacent_trim_endpoints(candidate.get(), wrapper, entity_id,
	entity_type, -1, true, false, &affected_face_source_tags);
    repair_exact_periodic_loop_branches(candidate.get(), wrapper, entity_id,
	entity_type, &affected_face_source_tags);
    if (brlcad::PullbackWorkCancelled())
	return reject("the item work budget expired");
    /* Re-identify each exact component by the unique set of immutable STEP
     * edge IDs that survived bridge removal.  This keeps the transaction
     * independent of array-index stability if endpoint reconciliation ever
     * gains an internal compaction step. */
    const auto loop_edge_tags = [&candidate](int loop_index) {
	std::vector<int> tags;
	if (loop_index < 0 || loop_index >= candidate->m_L.Count())
	    return tags;
	const ON_BrepLoop &loop = candidate->m_L[loop_index];
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    if (edge && edge->m_edge_user.i > 0)
		tags.push_back(edge->m_edge_user.i);
	}
	std::sort(tags.begin(), tags.end());
	tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
	return tags;
    };
    std::set<int> resolved_loops;
    for (std::vector<TopologicalKeyholeSplit>::iterator split =
	    splits.begin(); split != splits.end(); ++split) {
	split->original_loop = -1;
	split->detached_loop = -1;
	for (int li = 0; li < candidate->m_L.Count(); ++li) {
	    if (resolved_loops.find(li) != resolved_loops.end())
		continue;
	    const ON_BrepFace *loop_face = candidate->m_L[li].Face();
	    if (!loop_face ||
		    loop_face->m_face_user.i != split->face_source_tag ||
		    candidate->m_L[li].m_loop_user.i !=
			split->original_source_tag)
		continue;
	    const std::vector<int> tags = loop_edge_tags(li);
	    if (tags == split->original_edge_tags)
		split->original_loop = li;
	    else if (tags == split->detached_edge_tags)
		split->detached_loop = li;
	}
	if (split->original_loop < 0 || split->detached_loop < 0) {
	    if (wrapper->Verbose()) {
		const auto print_tags = [](const std::vector<int> &tags) {
		    std::ostringstream text;
		    for (size_t tag = 0; tag < tags.size(); ++tag)
			text << (tag ? "," : "") << tags[tag];
		    return text.str();
		};
		std::cerr << entity_type << " #" << entity_id
		    << ": keyhole identity expected original=["
		    << print_tags(split->original_edge_tags) << "] detached=["
		    << print_tags(split->detached_edge_tags) << "] candidates=";
		int reported = 0;
		for (int li = 0; li < candidate->m_L.Count() && reported < 12;
			++li) {
		    const ON_BrepFace *loop_face = candidate->m_L[li].Face();
		    if (!loop_face ||
			    loop_face->m_face_user.i != split->face_source_tag ||
			    candidate->m_L[li].m_loop_user.i !=
				split->original_source_tag)
			continue;
		    const std::vector<int> tags = loop_edge_tags(li);
		    bool related = false;
		    for (std::vector<int>::const_iterator tag = tags.begin();
			    tag != tags.end() && !related; ++tag)
			related = std::binary_search(
			    split->original_edge_tags.begin(),
			    split->original_edge_tags.end(), *tag) ||
			    std::binary_search(split->detached_edge_tags.begin(),
				split->detached_edge_tags.end(), *tag);
		    if (!related)
			continue;
		    std::cerr << " L" << li << "=[" << print_tags(tags) << ']';
		    ++reported;
		}
		std::cerr << std::endl;
	    }
	    return reject("a split loop identity was lost during bounded repair");
	}
	resolved_loops.insert(split->original_loop);
	resolved_loops.insert(split->detached_loop);
	candidate->m_L[split->original_loop].m_loop_user.i =
	    split->original_source_tag;
	candidate->m_L[split->detached_loop].m_loop_user.i =
	    split->detached_source_tag;
    }
    /* The repair pass has already computed the detached loop type and restored
     * the authoritative source FACE_OUTER_BOUND type.  Refreshing loop types
     * again here would erase that source classification on a periodic face. */
    refresh_brep_flags_preserving_singular_isos(candidate.get(), false);

    const auto sampled_winding_closure = [&candidate](
	    const ON_BrepLoop &loop, const ON_Surface *surface,
	    const ON_BrepTrim *last, const ON_BrepTrim *first,
	    double parameter_tolerance) {
	if (!surface || !last || !first || last->m_vi[1] < 0 ||
		last->m_vi[1] >= candidate->m_V.Count() ||
		first->m_vi[0] < 0 ||
		first->m_vi[0] >= candidate->m_V.Count())
	    return false;
	if (periodic_loop_closure(candidate.get(), &loop, last, first,
		parameter_tolerance))
	    return true;

	/* The sampled chain below can prove one complete winding even when its
	 * period jump occurs at an interior trim join.  In that arrangement the
	 * raw first and last UVs coincide rather than lying one period apart, so
	 * periodic_loop_closure() intentionally does not recognize them.  Accept
	 * that alternate closure only when both raw parameters coincide and the
	 * immutable topology vertices plus both lifted endpoints agree within the
	 * exact local OpenNURBS tolerances. */
	const ON_3dPoint last_uv = last->PointAtEnd();
	const ON_3dPoint first_uv = first->PointAtStart();
	if (!last_uv.IsValid() || !first_uv.IsValid())
	    return false;
	for (int direction = 0; direction < 2; ++direction) {
	    const ON_Interval domain = surface->Domain(direction);
	    const double coordinate_tolerance = parameter_tolerance *
		std::max(1.0, domain.IsIncreasing() ? domain.Length() : 1.0);
	    if (fabs(last_uv[direction] - first_uv[direction]) >
		    coordinate_tolerance)
		return false;
	}
	double topology_tolerance = LocalUnits::tolerance;
	if (last->Edge())
	    topology_tolerance = std::max(topology_tolerance,
		last->Edge()->m_tolerance);
	if (first->Edge())
	    topology_tolerance = std::max(topology_tolerance,
		first->Edge()->m_tolerance);
	topology_tolerance = std::max(topology_tolerance,
	    std::max(last->m_tolerance[0], last->m_tolerance[1]));
	topology_tolerance = std::max(topology_tolerance,
	    std::max(first->m_tolerance[0], first->m_tolerance[1]));
	topology_tolerance = std::max(topology_tolerance,
	    candidate->m_V[last->m_vi[1]].m_tolerance);
	topology_tolerance = std::max(topology_tolerance,
	    candidate->m_V[first->m_vi[0]].m_tolerance);
	const ON_3dPoint &last_vertex =
	    candidate->m_V[last->m_vi[1]].point;
	const ON_3dPoint &first_vertex =
	    candidate->m_V[first->m_vi[0]].point;
	const ON_3dPoint last_lift = closed_surface_point_at(surface, last_uv);
	const ON_3dPoint first_lift = closed_surface_point_at(surface, first_uv);
	return last_vertex.IsValid() && first_vertex.IsValid() &&
	    last_lift.IsValid() && first_lift.IsValid() &&
	    last_vertex.DistanceTo(first_vertex) <= topology_tolerance &&
	    last_lift.DistanceTo(last_vertex) <= topology_tolerance &&
	    first_lift.DistanceTo(first_vertex) <= topology_tolerance &&
	    last_lift.DistanceTo(first_lift) <= topology_tolerance;
    };

    const auto periodic_winding = [&candidate, &sampled_winding_closure](
	    const ON_BrepLoop &loop,
	    const ON_Surface *surface, int direction) {
	if (!surface || !surface->IsClosed(direction) ||
		loop.TrimCount() < 1)
	    return 0;
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    return 0;
	const double parameter_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    kPeriodicParameterSnapFraction * std::max(1.0, period));
	const ON_BrepTrim *first_trim = loop.Trim(0);
	const ON_BrepTrim *last_trim = loop.Trim(loop.TrimCount() - 1);
	if (first_trim && last_trim) {
	    const double direct_travel =
		last_trim->PointAtEnd()[direction] -
		first_trim->PointAtStart()[direction];
	    const double direct_tolerance =
		parameter_tolerance * std::max(1.0, period);
	    if (fabs(fabs(direct_travel) - period) <= direct_tolerance &&
		    periodic_loop_closure(candidate.get(), &loop, last_trim,
			first_trim, parameter_tolerance))
		return direct_travel < 0.0 ? -1 : 1;
	}
	bool have_sample = false;
	double first = 0.0;
	double previous = 0.0;
	double minimum = DBL_MAX;
	double maximum = -DBL_MAX;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (!trim || !trim->Domain().IsIncreasing())
		return 0;
	    const ON_Interval trim_domain = trim->Domain();
	    for (int sample = 0; sample <= kPcurveLocusScreeningSegments;
		    ++sample) {
		const ON_3dPoint uv = trim->PointAt(trim_domain.ParameterAt(
		    static_cast<double>(sample) /
		    kPcurveLocusScreeningSegments));
		if (!uv.IsValid() || !std::isfinite(uv[direction]))
		    return 0;
		double unwrapped = uv[direction];
		if (have_sample)
		    unwrapped += round((previous - unwrapped) / period) * period;
		else {
		    first = unwrapped;
		    have_sample = true;
		}
		previous = unwrapped;
		minimum = std::min(minimum, unwrapped);
		maximum = std::max(maximum, unwrapped);
	    }
	}
	const double closure_tolerance =
	    parameter_tolerance * std::max(1.0, period);
	const double net = previous - first;
	if (!have_sample ||
		fabs(fabs(net) - period) > closure_tolerance ||
		fabs((maximum - minimum) - period) > closure_tolerance)
	    return 0;
	if (!sampled_winding_closure(loop, surface, last_trim, first_trim,
		parameter_tolerance))
	    return 0;
	return net < 0.0 ? -1 : 1;
    };

    for (std::vector<TopologicalKeyholeSplit>::iterator split =
	    splits.begin(); split != splits.end(); ++split) {
	if (split->original_loop < 0 ||
		split->original_loop >= candidate->m_L.Count() ||
		split->detached_loop < 0 ||
		split->detached_loop >= candidate->m_L.Count() ||
		split->face < 0 || split->face >= candidate->m_F.Count())
	    return reject("a split topology index changed unexpectedly");
	ON_BrepLoop &original = candidate->m_L[split->original_loop];
	ON_BrepLoop &detached = candidate->m_L[split->detached_loop];
	/* The original FACE_OUTER_BOUND classification remains authoritative.
	 * The detached cycle has no independent STEP bound, so derive its type
	 * only after its periodic joins have been reconciled above. */
	original.m_type = ON_BrepLoop::outer;
	const ON_Surface *split_surface = original.Face() ?
	    original.Face()->SurfaceOf() : NULL;
	bool paired_periodic_band = false;
	int original_winding[2] = {0, 0};
	int detached_winding[2] = {0, 0};
	for (int direction = 0; split_surface && direction < 2; ++direction) {
	    original_winding[direction] = periodic_winding(original,
		split_surface, direction);
	    detached_winding[direction] = periodic_winding(detached,
		split_surface, direction);
	    if (original_winding[direction] *
		    detached_winding[direction] == -1) {
		paired_periodic_band = true;
		split->paired_periodic_direction = direction;
		break;
	    }
	}
	detached.m_type = paired_periodic_band ? ON_BrepLoop::inner :
	    candidate->ComputeLoopType(detached);
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": topological keyhole split loops "
		<< split->original_loop << '/' << split->detached_loop
		<< " acquired types " << static_cast<int>(original.m_type) << '/'
		<< static_cast<int>(detached.m_type) << " windings "
		<< original_winding[0] << '/' << detached_winding[0] << ','
		<< original_winding[1] << '/' << detached_winding[1]
		<< std::endl;
	if ((original.m_type != ON_BrepLoop::outer &&
		original.m_type != ON_BrepLoop::inner) ||
		(detached.m_type != ON_BrepLoop::outer &&
		 detached.m_type != ON_BrepLoop::inner))
	    return reject("a separated boundary did not acquire an outer/inner type");
	if (original.m_type == ON_BrepLoop::outer &&
		detached.m_type == ON_BrepLoop::outer) {
	    ON_BrepFace &source_face = candidate->m_F[split->face];
	    const int surface_index = source_face.m_si;
	    const bool reversed = source_face.m_bRev;
	    const int source_face_tag = source_face.m_face_user.i;
	    /* A bounded topology refresh may clear m_fi after Compact() has
	     * already dropped the detached loop from the source-face list.  The
	     * temporary source tags above, rather than those redundant array
	     * references, are the stable proof of which exact split loop is being
	     * moved.  Remove any stale face-list occurrence before assigning its
	     * new face; the completed candidate must still pass whole-BREP
	     * structural validation before it is retained. */
	    for (int candidate_face = 0; candidate_face < candidate->m_F.Count();
		    ++candidate_face) {
		ON_BrepFace &listed_face = candidate->m_F[candidate_face];
		for (int fli = listed_face.m_li.Count() - 1; fli >= 0; --fli)
		    if (listed_face.m_li[fli] == split->detached_loop)
			listed_face.m_li.Remove(fli);
	    }
	    ON_BrepFace &new_face = candidate->NewFace(surface_index);
	    new_face.m_bRev = reversed;
	    new_face.m_face_user.i = source_face_tag;
	    new_face.m_li.Append(split->detached_loop);
	    candidate->m_L[split->detached_loop].m_fi = new_face.m_face_index;
	} else if (original.m_type != ON_BrepLoop::outer &&
		detached.m_type != ON_BrepLoop::outer) {
	    return reject("neither separated boundary was an outer loop");
	}
    }
    /* A repeated STEP bridge can connect the two full-period boundaries of an
     * annular face on a doubly periodic surface.  Removing the bridge exposes
     * two model-space-closed chains which are intentionally one period open in
     * UV; trying to close either chain as a contractible loop merely moves the
     * period jump from one join to another.  Prove and unwrap both chains in
     * the same closed direction as one transaction.  The established
     * implicit-band repair can then replace the STEP bridge with the explicit
     * complementary seam topology OpenNURBS requires.
     *
     * Failed direction probes do not alter either the BREP or repair counts.
     * The immutable STEP edge-ID sets remain the stable identities if the
     * band constructor subsequently adds or compacts topology. */
    std::set<std::vector<int> > full_period_component_tags;
    for (std::vector<TopologicalKeyholeSplit>::const_iterator split =
	    splits.begin(); split != splits.end(); ++split) {
	if (split->original_loop < 0 ||
		split->original_loop >= candidate->m_L.Count() ||
		split->detached_loop < 0 ||
		split->detached_loop >= candidate->m_L.Count())
	    continue;
	const ON_BrepLoop &original = candidate->m_L[split->original_loop];
	const ON_Surface *surface = original.Face() ?
	    original.Face()->SurfaceOf() : NULL;
	for (int direction = 0; surface && direction < 2; ++direction) {
	    /* periodic_winding already proved the only direction in which these
	     * exact components form an opposite-winding pair.  Probing the other
	     * closed direction launches exact-edge pullbacks which cannot establish
	     * the required band and can dominate the complete item budget on large
	     * doubly periodic BREPs. */
	    if (direction != split->paired_periodic_direction)
		continue;
	    if (!surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = surface->Domain(direction);
	    if (!domain.IsIncreasing())
		continue;
	    const double parameter_tolerance = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		kPeriodicParameterSnapFraction *
		    std::max(1.0, domain.Length()));
	    std::unique_ptr<ON_Brep> band_candidate(new ON_Brep(*candidate));
	    bool original_unwrapped =
		regenerate_full_period_boundary_chain(band_candidate.get(),
		    split->original_loop, direction, parameter_tolerance,
		    wrapper, entity_id, entity_type, false);
	    bool detached_unwrapped =
		regenerate_full_period_boundary_chain(band_candidate.get(),
		    split->detached_loop, direction, parameter_tolerance,
		    wrapper, entity_id, entity_type, false);
	    const bool direct_original_unwrapped = original_unwrapped;
	    const bool direct_detached_unwrapped = detached_unwrapped;
	    bool native_cut[2] = {false, false};
	    int final_component_loop[2] = {
		split->original_loop, split->detached_loop
	    };
	    if (!original_unwrapped || !detached_unwrapped) {
		/* Neither chain is required to contain a STEP vertex at the
		 * OpenNURBS native seam.  When the supplied winding proves an
		 * interior crossing, materialize that exact 3-D subdivision and
		 * retry the same full-period proof.  Start from the unchanged
		 * candidate so a successful direct probe cannot influence the
		 * other component's crossing search. */
		band_candidate.reset(new ON_Brep(*candidate));
		const auto trial_loop_edge_tags = [&band_candidate](int loop_index) {
		    std::vector<int> tags;
		    if (loop_index < 0 ||
			    loop_index >= band_candidate->m_L.Count())
			return tags;
		    const ON_BrepLoop &loop =
			band_candidate->m_L[loop_index];
		    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
			const ON_BrepTrim *trim = loop.Trim(lti);
			const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
			if (edge && edge->m_edge_user.i > 0)
			    tags.push_back(edge->m_edge_user.i);
		    }
		    std::sort(tags.begin(), tags.end());
		    tags.erase(std::unique(tags.begin(), tags.end()),
			tags.end());
		    return tags;
		};
		const auto find_trial_loop = [&band_candidate,
			&trial_loop_edge_tags, split](
			const std::vector<int> &tags) {
		    for (int li = 0; li < band_candidate->m_L.Count(); ++li) {
			const ON_BrepFace *loop_face =
			    band_candidate->m_L[li].Face();
			if (loop_face &&
				loop_face->m_face_user.i ==
				    split->face_source_tag &&
				band_candidate->m_L[li].m_loop_user.i ==
				    split->original_source_tag &&
				trial_loop_edge_tags(li) == tags)
			    return li;
		    }
		    return -1;
		};
		const std::vector<int> component_tags[2] = {
		    split->original_edge_tags, split->detached_edge_tags
		};
		const bool needs_native_cut[2] = {
		    !original_unwrapped, !detached_unwrapped
		};
		for (int component = 0; component < 2; ++component) {
		    if (!needs_native_cut[component])
			continue;
		    const int loop_index = find_trial_loop(
			component_tags[component]);
		    ON_BrepFace *trial_face = loop_index >= 0 &&
			loop_index < band_candidate->m_L.Count() ?
			band_candidate->m_L[loop_index].Face() : NULL;
		    if (trial_face) {
			native_cut[component] =
			    split_open_periodic_boundary_crossing(
			    band_candidate.get(), *trial_face, loop_index,
			    direction, parameter_tolerance, wrapper, entity_id,
			    entity_type, false);
			/* Removing a repeated keyhole connector can expose one
			 * closed STEP edge as an intentionally one-period-open,
			 * single-trim boundary.  The multi-trim crossing helper
			 * above cannot select it, and an arbitrary (NaN-height)
			 * split may reject its supplied pcurve because the native
			 * seam is already that pcurve's endpoint.  Prove that the
			 * remaining coordinate is constant over the complete
			 * supplied pcurve, then give that exact band coordinate to
			 * the transactional splitter.  The splitter independently
			 * proves the immutable 3-D edge winding, seam point, every
			 * adjacent use, and directed child endpoints before it
			 * changes topology. */
			if (!native_cut[component] &&
				loop_index >= 0 &&
				loop_index < band_candidate->m_L.Count()) {
			    const ON_BrepLoop &singleton =
				band_candidate->m_L[loop_index];
			    const ON_BrepTrim *singleton_trim =
				singleton.TrimCount() == 1 ?
				singleton.Trim(0) : NULL;
			    const ON_BrepEdge *singleton_edge =
				singleton_trim ? singleton_trim->Edge() : NULL;
			    const ON_Curve *singleton_pcurve =
				singleton_trim ?
				singleton_trim->TrimCurveOf() : NULL;
			    const ON_Surface *singleton_surface =
				singleton.Face() ?
				singleton.Face()->SurfaceOf() : NULL;
			    double constant_open_parameter = 0.0;
			    bool constant_open_boundary = singleton_edge &&
				singleton_edge->m_vi[0] ==
				    singleton_edge->m_vi[1] &&
				singleton_pcurve && singleton_surface &&
				singleton_pcurve->Domain().IsIncreasing();
			    if (constant_open_boundary) {
				const int open_direction = 1 - direction;
				const ON_Interval pcurve_domain =
				    singleton_pcurve->Domain();
				double minimum_open = DBL_MAX;
				double maximum_open = -DBL_MAX;
				for (int sample = 0;
					sample <= kPcurveLocusScreeningSegments;
					++sample) {
				    if ((sample & 31) == 0 &&
					    brlcad::PullbackWorkCancelled()) {
					constant_open_boundary = false;
					break;
				    }
				    const ON_3dPoint uv =
					singleton_pcurve->PointAt(
					    pcurve_domain.ParameterAt(
						static_cast<double>(sample) /
						kPcurveLocusScreeningSegments));
				    if (!uv.IsValid() ||
					    !std::isfinite(uv[open_direction])) {
					constant_open_boundary = false;
					break;
				    }
				    minimum_open = std::min(minimum_open,
					uv[open_direction]);
				    maximum_open = std::max(maximum_open,
					uv[open_direction]);
				}
				constant_open_parameter =
				    0.5 * (minimum_open + maximum_open);
				/* Surface parameters are not necessarily measured in
				 * model units.  A literal UV range threshold can
				 * therefore reject a genuine isocurve with a
				 * sub-tolerance supplied-pcurve wobble, or accept a
				 * damaging shift on a highly scaled surface.  Prove
				 * the proposed constant coordinate in 3-D instead. */
				double locus_tolerance = std::max(
				    LocalUnits::tolerance,
				    singleton_edge->m_tolerance);
				locus_tolerance = std::max(locus_tolerance,
				    std::max(singleton_trim->m_tolerance[0],
					singleton_trim->m_tolerance[1]));
				for (int sample = 0;
					constant_open_boundary &&
					sample <= kPcurveLocusScreeningSegments;
					++sample) {
				    const ON_3dPoint supplied_uv =
					singleton_pcurve->PointAt(
					    pcurve_domain.ParameterAt(
						static_cast<double>(sample) /
						kPcurveLocusScreeningSegments));
				    ON_3dPoint constant_uv(supplied_uv);
				    constant_uv[open_direction] =
					constant_open_parameter;
				    const ON_3dPoint supplied_lift =
					supplied_uv.IsValid() ?
					singleton_surface->PointAt(
					    supplied_uv.x, supplied_uv.y) :
					ON_3dPoint::UnsetPoint;
				    const ON_3dPoint constant_lift =
					constant_uv.IsValid() ?
					singleton_surface->PointAt(
					    constant_uv.x, constant_uv.y) :
					ON_3dPoint::UnsetPoint;
				    if (!supplied_lift.IsValid() ||
					    !constant_lift.IsValid() ||
					    supplied_lift.DistanceTo(
						constant_lift) >
					    locus_tolerance)
					constant_open_boundary = false;
				}
			    }
			    if (constant_open_boundary) {
				/* Re-resolve after every rejected transaction:
				 * rollback replaces the BREP arrays, so neither a
				 * saved face nor surface address is stable. */
				ON_BrepFace *constant_face =
				    loop_index >= 0 &&
				    loop_index < band_candidate->m_L.Count() ?
				    band_candidate->m_L[loop_index].Face() :
				    NULL;
				const ON_Surface *constant_surface =
				    constant_face ?
				    constant_face->SurfaceOf() : NULL;
				const ON_BrepTrim *constant_trim =
				    band_candidate->m_L[loop_index].
					TrimCount() == 1 ?
				    band_candidate->m_L[loop_index].Trim(0) :
				    NULL;
				if (constant_surface && constant_trim)
				    native_cut[component] =
					split_periodic_boundary_at_native_seam(
					    band_candidate.get(), loop_index,
					    constant_trim->m_trim_index,
					    constant_surface, direction,
					    constant_open_parameter, wrapper,
					    entity_id, entity_type, false);
			    }
			}
			/* Generic branch repair may have hidden the supplied
			 * winding even though the immutable edge chain still
			 * crosses the native seam.  Probe each exact edge
			 * transactionally; the lower-level splitter derives the
			 * seam height from the edge itself, and the subsequent
			 * full-chain regeneration remains the winding proof. */
			if (!native_cut[component]) {
			    std::vector<int> trim_indices;
			    const ON_BrepLoop &trial_loop =
				band_candidate->m_L[loop_index];
			    trim_indices.reserve(trial_loop.TrimCount());
			    for (int lti = 0; lti < trial_loop.TrimCount();
				    ++lti) {
				const ON_BrepTrim *trim =
				    trial_loop.Trim(lti);
				if (trim)
				    trim_indices.push_back(
					trim->m_trim_index);
			    }
			    for (std::vector<int>::const_iterator trim_index =
				    trim_indices.begin();
				    trim_index != trim_indices.end() &&
					!native_cut[component];
				    ++trim_index) {
				/* A rejected split restores a deep BREP
				 * snapshot, invalidating the surface pointer
				 * supplied to that attempt. */
				ON_BrepFace *probe_face = loop_index >= 0 &&
				    loop_index < band_candidate->m_L.Count() ?
				    band_candidate->m_L[loop_index].Face() :
				    NULL;
				const ON_Surface *probe_surface =
				    probe_face ? probe_face->SurfaceOf() : NULL;
				if (probe_surface)
				    native_cut[component] =
					split_periodic_boundary_at_native_seam(
					    band_candidate.get(), loop_index,
					    *trim_index, probe_surface, direction,
					    std::numeric_limits<double>::quiet_NaN(),
					    wrapper, entity_id, entity_type,
					    false);
			    }
			}
		    }
		}
		const int trial_original =
		    find_trial_loop(split->original_edge_tags);
		const int trial_detached =
		    find_trial_loop(split->detached_edge_tags);
		original_unwrapped = trial_original >= 0 &&
		    regenerate_full_period_boundary_chain(
			band_candidate.get(), trial_original, direction,
			parameter_tolerance, wrapper, entity_id, entity_type,
			false);
		/* The first regeneration changes only pcurves and cyclic trim
		 * order, so immutable edge-set identity remains valid. */
		const int refreshed_detached =
		    find_trial_loop(split->detached_edge_tags);
		final_component_loop[0] = trial_original;
		final_component_loop[1] = refreshed_detached;
		detached_unwrapped = trial_detached >= 0 &&
		    refreshed_detached >= 0 &&
		    regenerate_full_period_boundary_chain(
			band_candidate.get(), refreshed_detached, direction,
			parameter_tolerance, wrapper, entity_id, entity_type,
			false);
	    }
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": topological keyhole full-period probe direction "
		    << direction << " direct="
		    << (direct_original_unwrapped ? "yes" : "no") << '/'
		    << (direct_detached_unwrapped ? "yes" : "no")
		    << " native-cuts=" << (native_cut[0] ? "yes" : "no")
		    << '/' << (native_cut[1] ? "yes" : "no")
		    << " final-loops=" << final_component_loop[0] << '/'
		    << final_component_loop[1] << " unwrapped="
		    << (original_unwrapped ? "yes" : "no") << '/'
		    << (detached_unwrapped ? "yes" : "no") << std::endl;
	    if (!original_unwrapped || !detached_unwrapped)
		continue;
	    *candidate = *band_candidate;
	    report_open_topology_closed_pcurves(
		"paired full-period regeneration");
	    full_period_component_tags.insert(split->original_edge_tags);
	    full_period_component_tags.insert(split->detached_edge_tags);
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"unwrapped an exact full-period boundary from its 3-D STEP edge chain");
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"unwrapped an exact full-period boundary from its 3-D STEP edge chain");
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": topological keyhole components "
		    << split->original_loop << '/' << split->detached_loop
		    << " proved a paired full-period boundary in direction "
		    << direction << std::endl;
	    break;
	}
    }
    if (!full_period_component_tags.empty()) {
	/* A separated keyhole face with two proven full-period components is
	 * already a legal OpenNURBS face when its loops validate.  Running the
	 * generic implicit-band constructor in that state is both unnecessary
	 * and harmful: it searches every exact edge for a new native-surface cut
	 * even though the completed loops already supply the required topology.
	 * The focused horn-torus face exposed the practical consequence -- a
	 * redundant closest-point search on one ambiguity-heavy edge could use
	 * the complete item allowance.  Reserve implicit-band construction for
	 * a component whose separated face still fails structural validation. */
	bool separated_faces_valid = true;
	for (int fi = 0; separated_faces_valid &&
		fi < candidate->m_F.Count(); ++fi) {
	    const ON_BrepFace &face = candidate->m_F[fi];
	    if (affected_face_source_tags.find(face.m_face_user.i) ==
		    affected_face_source_tags.end())
		continue;
	    const int face_index = fi;
	    std::unique_ptr<ON_Brep> face_candidate(
		candidate->DuplicateFaces(1, &face_index, false));
	    ON_wString face_messages;
	    ON_TextLog face_log(face_messages);
	    separated_faces_valid = face_candidate.get() &&
		face_candidate->IsValid(&face_log);
	}
	if (!separated_faces_valid) {
	    repair_implicit_periodic_face_bands(candidate.get(), wrapper,
		entity_id, entity_type, &split_source_loops);
	    report_open_topology_closed_pcurves("implicit periodic face bands");
	} else if (wrapper->Verbose()) {
	    std::cerr << entity_type << " #" << entity_id
		<< ": skipped redundant implicit-band reconstruction for "
		<< "structurally valid separated keyhole faces" << std::endl;
	}
    }

    /* Contractible components still use ordinary cyclic branch
     * regeneration.  Re-resolve by immutable source-edge identity because the
     * full-period band constructor may have inserted or compacted loops. */
    for (std::vector<TopologicalKeyholeSplit>::const_iterator split =
	    splits.begin(); split != splits.end(); ++split) {
	const std::vector<int> component_tags[2] = {
	    split->original_edge_tags, split->detached_edge_tags
	};
	for (int component = 0; component < 2; ++component) {
	    if (full_period_component_tags.find(component_tags[component]) !=
		    full_period_component_tags.end())
		continue;
	    int loop_index = -1;
	    for (int li = 0; li < candidate->m_L.Count(); ++li) {
		const ON_BrepFace *loop_face = candidate->m_L[li].Face();
		if (loop_face &&
			loop_face->m_face_user.i == split->face_source_tag &&
			candidate->m_L[li].m_loop_user.i ==
			    split->original_source_tag &&
			loop_edge_tags(li) == component_tags[component]) {
		    loop_index = li;
		    break;
		}
	    }
	    if (loop_index >= 0)
		regenerate_periodic_loop_chains(candidate.get(), wrapper,
		    entity_id, entity_type, loop_index, true, true);
	}
    }
    std::set<int> validated_trim_loci;
    repair_invalid_open_pcurves(candidate.get(), wrapper, entity_id,
	entity_type, false, &validated_trim_loci, &affected_face_source_tags);
    report_open_topology_closed_pcurves("invalid open pcurve repair");
    if (repair_closed_surface_seam_crossings(candidate.get(), wrapper,
	    entity_id, entity_type, &validated_trim_loci,
	    &affected_face_source_tags)) {
	report_open_topology_closed_pcurves("closed surface seam crossings");
	repair_invalid_open_pcurves(candidate.get(), wrapper, entity_id,
	    entity_type, false, NULL, &affected_face_source_tags);
	report_open_topology_closed_pcurves(
	    "post-seam invalid open pcurve repair");
    }
    if (full_period_component_tags.empty())
	repair_implicit_periodic_face_bands(candidate.get(), wrapper, entity_id,
	    entity_type, &split_source_loops);
    repair_exact_periodic_loop_branches(candidate.get(), wrapper, entity_id,
	entity_type, &affected_face_source_tags);
    report_open_topology_closed_pcurves("exact periodic loop branches");
    repair_adjacent_trim_endpoints(candidate.get(), wrapper, entity_id,
	entity_type, -1, true, false, &affected_face_source_tags);
    report_open_topology_closed_pcurves("adjacent trim endpoints");
    for (int fi = 0; fi < candidate->m_F.Count(); ++fi)
	if (affected_face_source_tags.find(
		candidate->m_F[fi].m_face_user.i) !=
		    affected_face_source_tags.end())
	    candidate->SortFaceLoops(candidate->m_F[fi]);
    refresh_brep_flags_preserving_singular_isos(candidate.get(), false);
    repair_face_bound_classification(candidate.get(), wrapper, entity_id,
	entity_type, &affected_face_source_tags);

    /* The generic flag refresh above can downgrade one member of an unrelated
     * same-loop seam pair after the keyhole transaction has compacted or
     * replaced topology elsewhere in the BREP.  Finish this candidate with
     * the same authoritative exact-seam sequence used by the ordinary repair
     * pipeline.  Each helper is transactional and the complete candidate is
     * still rejected below unless OpenNURBS validates it. */
    repair_seam_pair_from_exact_edge(candidate.get(), wrapper, entity_id,
	entity_type, NULL, NULL, false, &affected_face_source_tags);
    normalize_exact_paired_seam_loops_to_native_domain(candidate.get(),
	wrapper, entity_id, entity_type, &affected_face_source_tags);
    repair_paired_seam_loop_endpoints(candidate.get(), wrapper, entity_id,
	entity_type, &affected_face_source_tags);
    repair_adjacent_trim_endpoints(candidate.get(), wrapper, entity_id,
	entity_type, -1, true, false, &affected_face_source_tags);
    repair_single_singular_periodic_loop_branches(candidate.get(), wrapper,
	entity_id, entity_type, true, &affected_face_source_tags);
    repair_multi_singular_periodic_loop_branches(candidate.get(), wrapper,
	entity_id, entity_type, true, &affected_face_source_tags);
    candidate->SetTrimBoundingBoxes(false);

    ON_wString candidate_messages;
    ON_TextLog candidate_log(candidate_messages);
    const bool complete_candidate_valid = candidate->IsValid(&candidate_log);
    bool local_candidate_valid = complete_candidate_valid;
    if (!complete_candidate_valid) {
	/* The source BREP can contain another independently invalid periodic
	 * face.  Requiring whole-BREP validity here rolls back a fully proven
	 * keyhole split merely because that unrelated face has not reached its
	 * own repair yet.  Validate the affected faces and every face sharing one
	 * of their edges as a standalone OpenNURBS BREP, and independently reject
	 * any valid-to-invalid regression elsewhere.  DuplicateFaces preserves
	 * seam/mated trim context in this one-ring neighborhood. */
	std::set<int> affected_source_faces;
	for (std::vector<TopologicalKeyholeSplit>::const_iterator split =
		splits.begin(); split != splits.end(); ++split) {
	    if (split->face_source_tag > 0)
		affected_source_faces.insert(split->face_source_tag);
	}
	std::set<int> neighborhood_faces;
	for (int fi = 0; fi < candidate->m_F.Count(); ++fi) {
	    const ON_BrepFace &face = candidate->m_F[fi];
	    if (affected_source_faces.find(face.m_face_user.i) ==
		    affected_source_faces.end())
		continue;
	    neighborhood_faces.insert(fi);
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= candidate->m_L.Count())
		    continue;
		const ON_BrepLoop &loop = candidate->m_L[li];
		for (int lti = 0; lti < loop.TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = loop.Trim(lti);
		    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		    if (!edge)
			continue;
		    for (int eti = 0; eti < edge->m_ti.Count(); ++eti) {
			const ON_BrepTrim *use = candidate->Trim(
			    edge->m_ti[eti]);
			const ON_BrepFace *use_face = use ? use->Face() : NULL;
			if (use_face && use_face->m_face_index >= 0)
			    neighborhood_faces.insert(use_face->m_face_index);
		    }
		}
	    }
	}

	std::map<int, size_t> source_face_counts;
	std::map<int, bool> source_face_validity;
	bool comparable_unaffected_faces = !affected_source_faces.empty() &&
	    !neighborhood_faces.empty();
	for (int fi = 0; comparable_unaffected_faces && fi < brep->m_F.Count();
		++fi) {
	    const ON_BrepFace &face = brep->m_F[fi];
	    const int source_tag = face.m_face_user.i;
	    if (source_tag <= 0 || affected_source_faces.find(source_tag) !=
		    affected_source_faces.end())
		continue;
	    ++source_face_counts[source_tag];
	    ON_wString face_messages;
	    ON_TextLog face_log(face_messages);
	    const bool valid = face.IsValid(&face_log);
	    std::map<int, bool>::iterator recorded =
		source_face_validity.find(source_tag);
	    if (recorded == source_face_validity.end())
		source_face_validity[source_tag] = valid;
	    else
		recorded->second = recorded->second && valid;
	}
	std::map<int, size_t> candidate_face_counts;
	for (int fi = 0; comparable_unaffected_faces &&
		fi < candidate->m_F.Count(); ++fi) {
	    const ON_BrepFace &face = candidate->m_F[fi];
	    const int source_tag = face.m_face_user.i;
	    if (affected_source_faces.find(source_tag) !=
		    affected_source_faces.end())
		continue;
	    if (source_tag <= 0 || source_face_counts.find(source_tag) ==
		    source_face_counts.end()) {
		comparable_unaffected_faces = false;
		break;
	    }
	    ++candidate_face_counts[source_tag];
	    const std::map<int, bool>::const_iterator was_valid =
		source_face_validity.find(source_tag);
	    if (was_valid != source_face_validity.end() && was_valid->second) {
		ON_wString face_messages;
		ON_TextLog face_log(face_messages);
		if (!face.IsValid(&face_log)) {
		    comparable_unaffected_faces = false;
		    break;
		}
	    }
	}
	if (candidate_face_counts != source_face_counts)
	    comparable_unaffected_faces = false;

	std::vector<int> local_faces(neighborhood_faces.begin(),
	    neighborhood_faces.end());
	std::unique_ptr<ON_Brep> local_candidate(
	    comparable_unaffected_faces && !local_faces.empty() ?
	    candidate->DuplicateFaces(static_cast<int>(local_faces.size()),
		local_faces.data(), false) : NULL);
	std::string unsafe_topology;
	ON_wString local_messages;
	ON_TextLog local_log(local_messages);
	local_candidate_valid = local_candidate.get() &&
	    brep_topology_references_are_safe(candidate.get(), &unsafe_topology) &&
	    brep_topology_references_are_safe(local_candidate.get(),
		&unsafe_topology) && local_candidate->IsValid(&local_log);
	if (local_candidate_valid) {
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"validated a keyhole split in its complete local face neighborhood");
	} else if (wrapper->Verbose() && !local_messages.IsEmpty()) {
	    ON_String text(local_messages);
	    std::cerr << entity_type << " #" << entity_id
		<< ": topological keyhole local validation rejected:\n"
		<< text.Array();
	}
    }
    if (!local_candidate_valid) {
	if (wrapper->Verbose()) {
	    ON_String text(candidate_messages);
	    std::cerr << entity_type << " #" << entity_id
		<< ": topological keyhole normalization retry rejected:\n"
		<< text.Array();
	    int reported_joins = 0;
	    for (int li = 0; li < candidate->m_L.Count() &&
		    reported_joins < 12; ++li) {
		const ON_BrepLoop &loop = candidate->m_L[li];
		for (int lti = 0; lti < loop.TrimCount() &&
			reported_joins < 12; ++lti) {
		    const ON_BrepTrim *previous = loop.Trim(lti);
		    const ON_BrepTrim *next = loop.Trim(
			(lti + 1) % loop.TrimCount());
		    if (!previous || !next)
			continue;
		    const ON_3dPoint previous_end =
			previous->PointAtEnd();
		    const ON_3dPoint next_start =
			next->PointAtStart();
		    const double gap =
			previous_end.DistanceTo(next_start);
		    if (gap <= ON_ZERO_TOLERANCE)
			continue;
		    const ON_BrepEdge *previous_edge =
			previous->Edge();
		    const ON_BrepEdge *next_edge = next->Edge();
		    const ON_BrepFace *face = loop.Face();
		    const ON_Surface *surface = face ?
			face->SurfaceOf() : NULL;
		    std::cerr << entity_type << " #" << entity_id
			<< ": topological retry residual join L" << li
			<< "/STEP" << loop.m_loop_user.i << "/T"
			<< previous->m_trim_index << "/STEP edge "
			<< (previous_edge ?
			    previous_edge->m_edge_user.i : 0) << " -> T"
			<< next->m_trim_index << "/STEP edge "
			<< (next_edge ? next_edge->m_edge_user.i : 0)
			<< " gap=" << gap << " at " << previous_end.x << ':'
			<< previous_end.y << " -> " << next_start.x << ':'
			<< next_start.y << " loop-type="
			<< static_cast<int>(loop.m_type) << " trims="
			<< loop.TrimCount() << " face="
			<< (face ? face->m_face_index : -1) << " face-loops="
			<< (face ? face->LoopCount() : 0);
		    if (surface)
			std::cerr << " domains=" << surface->Domain(0).Min()
			    << ':' << surface->Domain(0).Max() << ','
			    << surface->Domain(1).Min() << ':'
			    << surface->Domain(1).Max() << " closed="
			    << (surface->IsClosed(0) ? 1 : 0)
			    << (surface->IsClosed(1) ? 1 : 0)
			    << " singular="
			    << (surface->IsSingular(0) ? 1 : 0)
			    << (surface->IsSingular(1) ? 1 : 0)
			    << (surface->IsSingular(2) ? 1 : 0)
			    << (surface->IsSingular(3) ? 1 : 0) << " surface="
			    << (surface->ClassId() ?
				surface->ClassId()->ClassName() : "unknown");
		    std::cerr << std::endl;
		    ++reported_joins;
		}
	    }
	}
	return reject("the structurally complete separated BREP remained invalid");
    }
    *brep = *candidate;
    for (size_t split = 0; split < splits.size(); ++split)
	wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	    "removed an exact opposite-use STEP keyhole bridge after validated topology separation");
    return true;
}

} /* namespace step_brep_detail */
