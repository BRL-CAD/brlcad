/* BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file step/STEPBrepPeriodic.cpp
 *
 * Exact periodic-band reconstruction and topology finalization.
 * Compiled as one schema-neutral importer build unit.
 */

#include "common.h"
#include "STEPBrepRepairInternal.h"

namespace step_brep_detail {
using namespace step_import_detail;

/* A degenerate torus can have a collapsed isoparameter in the interior of
 * the analytic surface's native domain.  STEP may legitimately use one
 * keyhole FACE_BOUND to describe the sheet on one side of that collapse.
 * After the keyhole is separated, some loops already contain explicit pole
 * topology while the remaining full-period boundary is still expressed on
 * the equivalent opposite side of the surface's open-direction seam.
 *
 * OpenNURBS requires the pole to be a surface boundary.  Give this face a
 * private, exactly trimmed surface and move each of its pcurves by a uniform
 * periodic image shift.  The collapsed isoparameter, every affected pcurve
 * lift, and the uniquely occupied side are all proven before the transaction
 * commits.  This is a representation change only; no 3-D edge or surface
 * locus is approximated. */
bool
restrict_periodic_face_to_interior_pole_sheet(ON_Brep *brep, int face_index,
	int winding_direction, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type, bool record_repair)
{
    if (!brep || !wrapper || face_index < 0 ||
	    face_index >= brep->m_F.Count() ||
	    (winding_direction != 0 && winding_direction != 1))
	return false;
    const ON_BrepFace &source_face = brep->m_F[face_index];
    const ON_Surface *source_surface = source_face.SurfaceOf();
    const int sheet_direction = 1 - winding_direction;
    if (!source_surface || !source_surface->IsClosed(winding_direction) ||
	    !source_surface->IsClosed(sheet_direction) ||
	    source_face.m_li.Count() < 1)
	return false;
    const ON_Interval winding_domain =
	source_surface->Domain(winding_direction);
    const ON_Interval sheet_domain = source_surface->Domain(sheet_direction);
    if (!winding_domain.IsIncreasing() || !sheet_domain.IsIncreasing())
	return false;
    const double winding_period = winding_domain.Length();
    const double sheet_period = sheet_domain.Length();
    const double winding_parameter_tolerance =
	kPeriodicParameterSnapFraction * std::max(1.0, winding_period);
    const double sheet_parameter_tolerance =
	kPeriodicParameterSnapFraction * std::max(1.0, sheet_period);

    bool have_collapsed_parameter = false;
    double collapsed_parameter = 0.0;
    ON_3dPoint collapse_vertex = ON_3dPoint::UnsetPoint;
    for (int fli = 0; fli < source_face.m_li.Count(); ++fli) {
	const int li = source_face.m_li[fli];
	if (li < 0 || li >= brep->m_L.Count())
	    return false;
	const ON_BrepLoop &loop = brep->m_L[li];
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (!trim || trim->m_type != ON_BrepTrim::singular ||
		    trim->m_vi[0] < 0 || trim->m_vi[0] >= brep->m_V.Count())
		continue;
	    const ON_3dPoint start = trim->PointAtStart();
	    const ON_3dPoint end = trim->PointAtEnd();
	    if (!start.IsValid() || !end.IsValid() ||
		    fabs(start[sheet_direction] - end[sheet_direction]) >
		sheet_parameter_tolerance ||
		    fabs(start[winding_direction] - end[winding_direction]) <=
		winding_parameter_tolerance)
		continue;
	    const double candidate = 0.5 *
		(start[sheet_direction] + end[sheet_direction]);
	    const double domain_guard = std::max(ON_ZERO_TOLERANCE,
		sheet_period * kPeriodicParameterSnapFraction);
	    if (candidate <= sheet_domain.Min() + domain_guard ||
		    candidate >= sheet_domain.Max() - domain_guard)
		continue;
	    double candidate_tolerance = std::max(LocalUnits::tolerance,
		std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
	    candidate_tolerance = std::max(candidate_tolerance,
		brep->m_V[trim->m_vi[0]].m_tolerance);
	    const ON_3dPoint &candidate_vertex =
		brep->m_V[trim->m_vi[0]].point;
	    bool collapsed = candidate_vertex.IsValid();
	    for (int sample = 0; collapsed && sample <= 128; ++sample) {
		ON_3dPoint uv;
		uv[winding_direction] = winding_domain.ParameterAt(
		    static_cast<double>(sample) / 128.0);
		uv[sheet_direction] = candidate;
		uv.z = 0.0;
		const ON_3dPoint lift = source_surface->PointAt(uv.x, uv.y);
		collapsed = lift.IsValid() &&
		    lift.DistanceTo(candidate_vertex) <= candidate_tolerance;
	    }
	    if (!collapsed)
		continue;
	    if (have_collapsed_parameter &&
		    fabs(candidate - collapsed_parameter) >
		sheet_parameter_tolerance)
		return false;
	    have_collapsed_parameter = true;
	    collapsed_parameter = candidate;
	    collapse_vertex = candidate_vertex;
	}
    }
    if (!have_collapsed_parameter || !collapse_vertex.IsValid())
	return false;
    if (wrapper->Verbose())
	std::cerr << entity_type << " #" << entity_id
	    << ": interior-pole sheet candidate F" << face_index
	    << " directions=" << winding_direction << '/' << sheet_direction
	    << " pole=" << collapsed_parameter << " domains="
	    << winding_domain.Min() << ':' << winding_domain.Max() << ','
	    << sheet_domain.Min() << ':' << sheet_domain.Max() << std::endl;

    struct ShiftedTrim {
	int trim_index;
	std::unique_ptr<ON_Curve> curve;
	ShiftedTrim(int index, ON_Curve *candidate) :
	    trim_index(index), curve(candidate) {}
    };
    std::unique_ptr<ON_Brep> accepted;
    int accepted_side = -1;
    double accepted_nonperiodic_shift = 0.0;
    for (int side = 0; side < 2; ++side) {
	const ON_Interval retained = side == 0 ?
	    ON_Interval(sheet_domain.Min(), collapsed_parameter) :
	    ON_Interval(collapsed_parameter, sheet_domain.Max());
	if (!retained.IsIncreasing())
	    continue;
	std::unique_ptr<ON_Surface> replacement(
	    source_surface->DuplicateSurface());
	if (!replacement || !replacement->Trim(sheet_direction, retained) ||
		!replacement->IsValid()) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": interior-pole sheet F" << face_index << " side="
		    << side << " rejected: private surface trim failed"
		    << std::endl;
	    continue;
	}
	std::vector<ShiftedTrim> shifted;
	bool valid = true;
	std::string rejection_stage;
	double maximum_nonperiodic_shift = 0.0;
	for (int fli = 0; valid && fli < source_face.m_li.Count(); ++fli) {
	    const ON_BrepLoop &loop = brep->m_L[source_face.m_li[fli]];
	    for (int lti = 0; valid && lti < loop.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop.Trim(lti);
		const ON_Interval trim_domain = trim ? trim->Domain() :
		    ON_Interval::EmptyInterval;
		if (!trim || !trim->TrimCurveOf() ||
			!trim_domain.IsIncreasing()) {
		    rejection_stage = "invalid source trim";
		    valid = false;
		    break;
		}
		ON_NurbsCurve detached;
		if (!trim->GetNurbForm(detached) || !detached.IsValid()) {
		    rejection_stage = "could not detach a source pcurve";
		    valid = false;
		    break;
		}
		double minimum = DBL_MAX;
		double maximum = -DBL_MAX;
		for (int sample = 0; sample <= 128; ++sample) {
		    const ON_3dPoint uv = trim->PointAt(
			trim_domain.ParameterAt(static_cast<double>(sample) / 128.0));
		    if (!uv.IsValid()) {
			rejection_stage = "could not sample a source pcurve";
			valid = false;
			break;
		    }
		    minimum = std::min(minimum, uv[sheet_direction]);
		    maximum = std::max(maximum, uv[sheet_direction]);
		}
		if (!valid)
		    break;
		const double minimum_shift = retained.Min() - minimum;
		const double maximum_shift = retained.Max() - maximum;
		bool have_shift = false;
		double selected_shift = 0.0;
		double selected_nonperiodic = DBL_MAX;
		for (int turns = -2; turns <= 2; ++turns) {
		    const double periodic_shift = turns * sheet_period;
		    if (periodic_shift < minimum_shift -
			    sheet_parameter_tolerance ||
			periodic_shift > maximum_shift +
			    sheet_parameter_tolerance)
			continue;
		    const double candidate_shift = std::max(minimum_shift,
			std::min(maximum_shift, periodic_shift));
		    const double nonperiodic = fabs(candidate_shift - periodic_shift);
		    if (!have_shift || nonperiodic < selected_nonperiodic) {
			have_shift = true;
			selected_shift = candidate_shift;
			selected_nonperiodic = nonperiodic;
		    }
		}
		if (!have_shift) {
		    std::ostringstream detail;
		    detail << "no uniform periodic image for T"
			<< trim->m_trim_index << " range=" << minimum << ':'
			<< maximum << " retained=" << retained.Min() << ':'
			<< retained.Max();
		    rejection_stage = detail.str();
		    valid = false;
		    break;
		}
		ON_Xform translation(ON_Xform::IdentityTransformation);
		translation.m_xform[sheet_direction][3] = selected_shift;
		if (fabs(selected_shift) > ON_ZERO_TOLERANCE &&
			!detached.Transform(translation)) {
		    rejection_stage = "could not translate a detached pcurve";
		    valid = false;
		    break;
		}
		if (!detached.ChangeDimension(2) || !detached.IsValid()) {
		    rejection_stage = "translated pcurve was invalid";
		    valid = false;
		    break;
		}
		double topology_tolerance = std::max(LocalUnits::tolerance,
		    std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
		if (trim->Edge())
		    topology_tolerance = std::max(topology_tolerance,
			trim->Edge()->m_tolerance);
		for (int endpoint = 0; endpoint < 2; ++endpoint)
		    if (trim->m_vi[endpoint] >= 0 &&
			    trim->m_vi[endpoint] < brep->m_V.Count())
			topology_tolerance = std::max(topology_tolerance,
			    brep->m_V[trim->m_vi[endpoint]].m_tolerance);
		for (int sample = 0; valid && sample <= 128; ++sample) {
		    const double parameter = trim_domain.ParameterAt(
			static_cast<double>(sample) / 128.0);
		    const ON_3dPoint original_uv = trim->PointAt(parameter);
		    const ON_3dPoint shifted_uv = detached.PointAt(parameter);
		    const ON_3dPoint original_lift = source_surface->PointAt(
			original_uv.x, original_uv.y);
		    const ON_3dPoint shifted_lift = replacement->PointAt(
			shifted_uv.x, shifted_uv.y);
		    valid = shifted_uv.IsValid() &&
			shifted_uv[sheet_direction] >= retained.Min() -
			    sheet_parameter_tolerance &&
			shifted_uv[sheet_direction] <= retained.Max() +
			    sheet_parameter_tolerance &&
			original_lift.IsValid() && shifted_lift.IsValid() &&
			original_lift.DistanceTo(shifted_lift) <=
			    topology_tolerance;
		    if (!valid) {
			std::ostringstream detail;
			detail << "shifted pcurve T" << trim->m_trim_index
			    << " failed lift/domain validation at " << sample
			    << "/128 shift=" << selected_shift;
			rejection_stage = detail.str();
		    }
		}
		if (!valid)
		    break;
		maximum_nonperiodic_shift = std::max(maximum_nonperiodic_shift,
		    selected_nonperiodic);
		shifted.push_back(ShiftedTrim(trim->m_trim_index,
		    new ON_NurbsCurve(detached)));
	    }
	}
	if (!valid) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": interior-pole sheet F" << face_index << " side="
		    << side << " rejected: " << rejection_stage << std::endl;
	    continue;
	}

	std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
	ON_Surface *installed_surface = replacement.release();
	const int surface_index = candidate->AddSurface(installed_surface);
	if (surface_index < 0) {
	    delete installed_surface;
	    continue;
	}
	ON_BrepFace &candidate_face = candidate->m_F[face_index];
	candidate_face.m_si = surface_index;
	candidate_face.SetProxySurface(installed_surface);
	for (std::vector<ShiftedTrim>::iterator replacement_trim =
		shifted.begin(); replacement_trim != shifted.end();
		++replacement_trim) {
	    ON_Curve *curve = replacement_trim->curve.release();
	    const int c2 = candidate->AddTrimCurve(curve);
	    if (c2 < 0) {
		delete curve;
		valid = false;
		break;
	    }
	    if (replacement_trim->trim_index < 0 ||
		    replacement_trim->trim_index >= candidate->m_T.Count() ||
		    !candidate->SetTrimCurve(
			candidate->m_T[replacement_trim->trim_index], c2)) {
		valid = false;
		break;
	    }
	    candidate->SetTrimIsoFlags(
		candidate->m_T[replacement_trim->trim_index]);
	}
	if (!valid)
	    continue;
	if (accepted)
	    return false;
	accepted = std::move(candidate);
	accepted_side = side;
	accepted_nonperiodic_shift = maximum_nonperiodic_shift;
    }
    if (!accepted)
	return false;

    *brep = *accepted;
    std::ostringstream message;
    message << "restricted a degenerate periodic surface to the exact "
	<< (accepted_side == 0 ? "lower" : "upper")
	<< " sheet at its proven interior pole";

    if (record_repair)
	wrapper->RecordRepair(entity_id, entity_type, "face_surface",
	    message.str());
    if (record_repair &&
	    accepted_nonperiodic_shift > ON_ZERO_TOLERANCE)
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
	    entity_id, entity_type, "trim_pcurve",
	    "snapped a periodic pcurve image to the private pole-sheet boundary "
	    "after dense model-space validation");
    return true;
}


/* A STEP EDGE_LOOP on a horn torus can visit the same pole vertex twice,
 * describing one boundary chain on each sheet without an explicit geometric
 * bridge.  The two visits use lift-equivalent parameter images separated by
 * one period.  Split that loop at the pole, add reciprocal zero-length
 * singular connectors, and give the two resulting faces private lower and
 * upper surface sheets.  This is the pole analogue of keyhole separation:
 * every source edge remains unchanged and both resulting faces must validate
 * before the candidate is committed. */
bool
split_repeated_interior_pole_face(ON_Brep *brep, int face_index,
	int winding_direction, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type)
{
    if (!brep || !wrapper || face_index < 0 ||
	    face_index >= brep->m_F.Count())
	return false;
    const ON_BrepFace &source_face = brep->m_F[face_index];
    const ON_Surface *surface = source_face.SurfaceOf();
    const int sheet_direction = 1 - winding_direction;
    if (!surface || !surface->IsClosed(winding_direction) ||
	    !surface->IsClosed(sheet_direction))
	return false;
    const ON_Interval winding_domain = surface->Domain(winding_direction);
    const ON_Interval sheet_domain = surface->Domain(sheet_direction);
    if (!winding_domain.IsIncreasing() || !sheet_domain.IsIncreasing())
	return false;
    const double winding_period = winding_domain.Length();
    const double sheet_period = sheet_domain.Length();
    const double winding_tolerance = kPeriodicParameterSnapFraction *
	std::max(1.0, winding_period);
    const double sheet_tolerance = kPeriodicParameterSnapFraction *
	std::max(1.0, sheet_period);

    const auto trim_sequence_fits_side = [brep, &sheet_domain, sheet_period,
	    sheet_direction, sheet_tolerance](const std::vector<int> &trims,
	    int side) {
	const ON_Interval retained = side == 0 ?
	    ON_Interval(sheet_domain.Min(), sheet_domain.Mid()) :
	    ON_Interval(sheet_domain.Mid(), sheet_domain.Max());
	if (trims.empty() || !retained.IsIncreasing())
	    return false;
	for (std::vector<int>::const_iterator trim_index = trims.begin();
		trim_index != trims.end(); ++trim_index) {
	    if (*trim_index < 0 || *trim_index >= brep->m_T.Count())
		return false;
	    const ON_BrepTrim &trim = brep->m_T[*trim_index];
	    if (!trim.TrimCurveOf() || !trim.Domain().IsIncreasing())
		return false;
	    double minimum = DBL_MAX;
	    double maximum = -DBL_MAX;
	    for (int sample = 0; sample <= 64; ++sample) {
		const ON_3dPoint uv = trim.PointAt(trim.Domain().ParameterAt(
		    static_cast<double>(sample) / 64.0));
		if (!uv.IsValid())
		    return false;
		minimum = std::min(minimum, uv[sheet_direction]);
		maximum = std::max(maximum, uv[sheet_direction]);
	    }
	    const double minimum_shift = retained.Min() - minimum;
	    const double maximum_shift = retained.Max() - maximum;
	    bool fits = false;
	    for (int turns = -2; turns <= 2 && !fits; ++turns) {
		const double shift = turns * sheet_period;
		fits = shift >= minimum_shift - sheet_tolerance &&
		    shift <= maximum_shift + sheet_tolerance;
	    }
	    if (!fits)
		return false;
	}
	return true;
    };

    for (int target_fli = 0; target_fli < source_face.m_li.Count();
	    ++target_fli) {
	const int target_li = source_face.m_li[target_fli];
	if (target_li < 0 || target_li >= brep->m_L.Count())
	    continue;
	const ON_BrepLoop &target_loop = brep->m_L[target_li];
	const int trim_count = target_loop.TrimCount();
	if (trim_count < 4)
	    continue;
	for (int first = 0; first < trim_count; ++first) {
	    const ON_BrepTrim *first_trim = target_loop.Trim(first);
	    if (!first_trim || first_trim->m_vi[0] < 0 ||
		    first_trim->m_vi[0] >= brep->m_V.Count())
		continue;
	    for (int second = first + 2; second < trim_count; ++second) {
		if (first == 0 && second == trim_count - 1)
		    continue;
		const ON_BrepTrim *second_trim = target_loop.Trim(second);
		if (!second_trim || second_trim->m_vi[0] !=
			first_trim->m_vi[0])
		    continue;
		const int pole_vertex_index = first_trim->m_vi[0];
		const ON_3dPoint first_uv = first_trim->PointAtStart();
		const ON_3dPoint second_uv = second_trim->PointAtStart();
		if (!first_uv.IsValid() || !second_uv.IsValid() ||
			fabs(fabs(second_uv[winding_direction] -
			    first_uv[winding_direction]) - winding_period) >
			winding_tolerance ||
			fabs(second_uv[sheet_direction] -
			    first_uv[sheet_direction]) > sheet_tolerance)
		    continue;
		const double pole_parameter = 0.5 *
		    (first_uv[sheet_direction] + second_uv[sheet_direction]);
		if (fabs(pole_parameter - sheet_domain.Mid()) >
			sheet_tolerance)
		    continue;
		double pole_tolerance = std::max(LocalUnits::tolerance,
		    brep->m_V[pole_vertex_index].m_tolerance);
		const ON_3dPoint &pole_vertex =
		    brep->m_V[pole_vertex_index].point;
		bool collapsed = pole_vertex.IsValid();
		for (int sample = 0; collapsed && sample <= 128; ++sample) {
		    ON_3dPoint uv;
		    uv[winding_direction] = winding_domain.ParameterAt(
			static_cast<double>(sample) / 128.0);
		    uv[sheet_direction] = pole_parameter;
		    uv.z = 0.0;
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    collapsed = lift.IsValid() &&
			lift.DistanceTo(pole_vertex) <= pole_tolerance;
		}
		if (!collapsed)
		    continue;

		std::vector<int> first_chain;
		std::vector<int> second_chain;
		for (int position = first; position < second; ++position)
		    first_chain.push_back(target_loop.m_ti[position]);
		for (int position = second; position < trim_count; ++position)
		    second_chain.push_back(target_loop.m_ti[position]);
		for (int position = 0; position < first; ++position)
		    second_chain.push_back(target_loop.m_ti[position]);
		if (first_chain.size() < 2 || second_chain.size() < 2)
		    continue;
		const bool first_low = trim_sequence_fits_side(first_chain, 0);
		const bool first_high = trim_sequence_fits_side(first_chain, 1);
		const bool second_low = trim_sequence_fits_side(second_chain, 0);
		const bool second_high = trim_sequence_fits_side(second_chain, 1);
		if (first_low == first_high || second_low == second_high ||
			first_low == second_low)
		    continue;
		const int first_side = first_low ? 0 : 1;
		const int second_side = second_low ? 0 : 1;

		std::map<int, int> sibling_sides;
		bool siblings_assigned = true;
		for (int sibling_fli = 0; siblings_assigned &&
			sibling_fli < source_face.m_li.Count(); ++sibling_fli) {
		    const int sibling_li = source_face.m_li[sibling_fli];
		    if (sibling_li == target_li)
			continue;
		    if (sibling_li < 0 || sibling_li >= brep->m_L.Count()) {
			siblings_assigned = false;
			break;
		    }
		    const ON_BrepLoop &sibling = brep->m_L[sibling_li];
		    std::vector<int> sibling_trims;
		    for (int lti = 0; lti < sibling.TrimCount(); ++lti)
			sibling_trims.push_back(sibling.m_ti[lti]);
		    const bool low = trim_sequence_fits_side(sibling_trims, 0);
		    const bool high = trim_sequence_fits_side(sibling_trims, 1);
		    if (low == high)
			siblings_assigned = false;
		    else
			sibling_sides[sibling_li] = low ? 0 : 1;
		}
		if (!siblings_assigned)
		    continue;

		std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
		ON_BrepFace &candidate_source_face = candidate->m_F[face_index];
		const ON_BrepLoop::TYPE original_type =
		    candidate->m_L[target_li].m_type;
		const int source_loop_tag =
		    candidate->m_L[target_li].m_loop_user.i;
		const int new_loop_index =
		    candidate->NewLoop(original_type).m_loop_index;
		ON_BrepLoop &original_loop = candidate->m_L[target_li];
		ON_BrepLoop &new_loop = candidate->m_L[new_loop_index];
		new_loop.m_loop_user.i = source_loop_tag;
		original_loop.m_ti.SetCount(0);
		new_loop.m_ti.SetCount(0);
		/* Preserve the chain containing cyclic position zero in the source
		 * loop.  This keeps stable topology indices wherever possible. */
		const bool first_contains_zero = first == 0;
		const std::vector<int> &original_chain = first_contains_zero ?
		    first_chain : second_chain;
		const std::vector<int> &detached_chain = first_contains_zero ?
		    second_chain : first_chain;
		const int original_side = first_contains_zero ?
		    first_side : second_side;
		const int detached_side = 1 - original_side;
		for (std::vector<int>::const_iterator trim = original_chain.begin();
			trim != original_chain.end(); ++trim) {
		    original_loop.m_ti.Append(*trim);
		    candidate->m_T[*trim].m_li = target_li;
		}
		for (std::vector<int>::const_iterator trim = detached_chain.begin();
			trim != detached_chain.end(); ++trim) {
		    new_loop.m_ti.Append(*trim);
		    candidate->m_T[*trim].m_li = new_loop_index;
		}

		ON_Brep *candidate_brep = candidate.get();
		const auto add_pole_connector = [candidate_brep,
			pole_vertex_index, sheet_direction, pole_tolerance](int loop_index,
			int side) {
		    ON_BrepLoop &loop = candidate_brep->m_L[loop_index];
		    const ON_BrepTrim *chain_first = loop.Trim(0);
		    const ON_BrepTrim *chain_last =
			loop.Trim(loop.TrimCount() - 1);
		    if (!chain_first || !chain_last ||
			    chain_first->m_vi[0] != pole_vertex_index ||
			    chain_last->m_vi[1] != pole_vertex_index)
			return false;
		    std::unique_ptr<ON_LineCurve> connector(new ON_LineCurve(
			chain_last->PointAtEnd(), chain_first->PointAtStart()));
		    if (!connector->ChangeDimension(2) || !connector->IsValid())
			return false;
		    const int c2 = candidate_brep->AddTrimCurve(connector.release());
		    if (c2 < 0)
			return false;
		    const ON_Surface::ISO iso = sheet_direction == 0 ?
			(side == 0 ? ON_Surface::E_iso : ON_Surface::W_iso) :
			(side == 0 ? ON_Surface::N_iso : ON_Surface::S_iso);
		    ON_BrepTrim &singular = candidate_brep->NewSingularTrim(
			candidate_brep->m_V[pole_vertex_index], loop, iso, c2);
		    singular.m_tolerance[0] = pole_tolerance;
		    singular.m_tolerance[1] = pole_tolerance;
		    return true;
		};
		if (!add_pole_connector(target_li, original_side) ||
			!add_pole_connector(new_loop_index, detached_side))
		    continue;

		const int source_surface_index = candidate_source_face.m_si;
		const bool source_reversed = candidate_source_face.m_bRev;
		const int source_face_tag = candidate_source_face.m_face_user.i;
		ON_BrepFace &detached_face =
		    candidate->NewFace(source_surface_index);
		const int detached_face_index = detached_face.m_face_index;
		detached_face.m_bRev = source_reversed;
		detached_face.m_face_user.i = source_face_tag;
		ON_BrepFace &installed_source_face = candidate->m_F[face_index];
		ON_BrepFace &installed_detached_face =
		    candidate->m_F[detached_face_index];
		installed_source_face.m_li.SetCount(0);
		installed_detached_face.m_li.SetCount(0);
		installed_source_face.m_li.Append(target_li);
		candidate->m_L[target_li].m_fi = face_index;
		installed_detached_face.m_li.Append(new_loop_index);
		candidate->m_L[new_loop_index].m_fi = detached_face_index;
		for (std::map<int, int>::const_iterator sibling =
			sibling_sides.begin(); sibling != sibling_sides.end();
			++sibling) {
		    const bool on_original = sibling->second == original_side;
		    ON_BrepFace &owner = on_original ? installed_source_face :
			candidate->m_F[detached_face_index];
		    owner.m_li.Append(sibling->first);
		    candidate->m_L[sibling->first].m_fi = owner.m_face_index;
		}

		if (!restrict_periodic_face_to_interior_pole_sheet(
			candidate.get(), face_index, winding_direction, wrapper,
			entity_id, entity_type, false) ||
		    !restrict_periodic_face_to_interior_pole_sheet(
			candidate.get(), detached_face_index, winding_direction,
			wrapper, entity_id, entity_type, false))
		    continue;

		bool valid = true;
		const int face_indices[2] = {face_index, detached_face_index};
		const int primary_loops[2] = {target_li, new_loop_index};
		for (int split_face = 0; valid && split_face < 2; ++split_face) {
		    ON_BrepFace &check_face = candidate->m_F[
			face_indices[split_face]];
		    for (int fli = 0; fli < check_face.m_li.Count(); ++fli) {
			ON_BrepLoop &check_loop = candidate->m_L[
			    check_face.m_li[fli]];
			check_loop.m_type = check_loop.m_loop_index ==
			    primary_loops[split_face] ? ON_BrepLoop::outer :
			    ON_BrepLoop::inner;
			ON_wString loop_messages;
			ON_TextLog loop_log(loop_messages);
			valid = check_loop.IsValid(&loop_log);
			if (!valid)
			    break;
		    }
		    ON_wString face_messages;
		    ON_TextLog face_log(face_messages);
		    valid = valid && check_face.IsValid(&face_log);
		}
		if (!valid)
		    continue;

		*brep = *candidate;
		wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		    "split a repeated STEP pole vertex into two exact singular loop connectors");
		wrapper->RecordRepair(entity_id, entity_type, "face_surface",
		    "separated a degenerate periodic face onto exact lower and upper pole sheets");
		return true;
	    }
	}
    }
    return false;
}


/* STEP permits a compact representation of a band on a singly-periodic
 * surface: the two closed boundary edges are separate one-trim loops and the
 * identified surface sides supply the implicit connection between them.
 * OpenNURBS requires that same topology to be cut into a Euclidean UV loop,
 * with two opposite uses of an explicit seam edge.  Insert that cut only when
 * exactly two candidate loops exist and the complete surface isocurve is
 * densely proven to agree on both periodic sides.  No source edge or surface
 * is approximated or moved. */
size_t
repair_implicit_periodic_face_bands(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	const std::set<int> *only_source_loops,
	bool allow_deferred_isocurve_split)
{
    if (!brep || !wrapper || wrapper->ImportOptions().repair !=
	    brlcad::step::RepairMode::Safe)
	return 0;

    size_t repaired = 0;
    const std::chrono::steady_clock::time_point pass_started =
	std::chrono::steady_clock::now();
    size_t periodic_faces_visited = 0;
    size_t closed_singletons_seen = 0;
    size_t periodic_singletons_seen = 0;
    size_t supplied_winding_candidates = 0;
    size_t arbitrary_split_attempts = 0;
    size_t arbitrary_split_successes = 0;
    size_t segmented_chain_scans = 0;
    long long arbitrary_split_microseconds = 0;
    size_t collapsed_boundary_attempts = 0;
    size_t collapsed_boundary_successes = 0;
    long long collapsed_boundary_microseconds = 0;
    size_t full_period_chain_attempts = 0;
    size_t full_period_chain_successes = 0;
    long long full_period_chain_microseconds = 0;
    size_t native_seam_split_attempts = 0;
    size_t native_seam_split_successes = 0;
    long long native_seam_split_microseconds = 0;
    size_t open_boundary_split_attempts = 0;
    size_t open_boundary_split_successes = 0;
    long long open_boundary_split_microseconds = 0;
    size_t edge_chain_winding_attempts = 0;
    size_t edge_chain_winding_successes = 0;
    long long edge_chain_winding_microseconds = 0;
    size_t split_chain_regeneration_attempts = 0;
    size_t split_chain_regeneration_successes = 0;
    long long split_chain_regeneration_microseconds = 0;
    std::map<std::pair<int, int>, double> proven_full_period_boundaries;
    /* A segmented boundary regenerated from its immutable STEP edge chain is
     * a persistent proof for the remainder of this repair transaction.  The
     * pcurve classifier can remain inconclusive for a scalloped two-edge
     * boundary even after its exact chain has been put on the native period.
     * Forgetting the stronger edge-chain proof made the fallback regenerate
     * the same pcurves, restart the face, and repeat without changing
     * topology. */
    std::set<std::pair<int, int> > regenerated_full_period_boundaries;
    std::set<int> aligned_band_surface_seams;
    const auto full_period_boundary_key = [](const ON_BrepLoop &loop,
	    int loop_index, int direction) {
	/* STEP loop identities survive the pcurve-only transaction.  Retain a
	 * private fallback for generated loops which have no source identity. */
	const int identity = loop.m_loop_user.i > 0 ? loop.m_loop_user.i :
	    -(loop_index + 1);
	return std::make_pair(identity, direction);
    };
    const auto loop_has_singular_topology = [](const ON_BrepLoop &loop) {
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (trim && (trim->m_type == ON_BrepTrim::singular ||
		    trim->m_ei < 0))
		return true;
	}
	return false;
    };
    /* A one-trim arbitrary full-period boundary becomes a two-trim chain
     * after split_periodic_boundary_at_native_seam().  Remember only those
     * proven chains so the general segmented-boundary solver does not scan
     * every ordinary two-edge loop in the model. */
    std::set<std::pair<int, int> > arbitrary_split_boundaries;
    const auto prove_collapsed_boundary = [brep, wrapper, entity_id,
	    &entity_type, &collapsed_boundary_attempts,
	    &collapsed_boundary_successes, &collapsed_boundary_microseconds](
	    ON_BrepLoop &loop, const ON_Surface *surface,
	    int closed_direction, double parameter_tolerance,
	    double *proven_open_parameter) {
	++collapsed_boundary_attempts;
	const std::chrono::steady_clock::time_point started =
	    std::chrono::steady_clock::now();
	const bool proven = regenerate_collapsed_periodic_boundary(brep, loop,
	    surface, closed_direction, parameter_tolerance, wrapper, entity_id,
	    entity_type, proven_open_parameter);
	collapsed_boundary_microseconds +=
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count();
	if (proven) ++collapsed_boundary_successes;
	return proven;
    };
    const auto regenerate_full_period_chain = [brep, wrapper, entity_id,
	    &entity_type, &full_period_chain_attempts,
	    &full_period_chain_successes, &full_period_chain_microseconds,
	    &regenerated_full_period_boundaries, &full_period_boundary_key](
	    int loop_index, int closed_direction, double parameter_tolerance) {
	++full_period_chain_attempts;
	const std::chrono::steady_clock::time_point started =
	    std::chrono::steady_clock::now();
	const bool regenerated = regenerate_full_period_boundary_chain(brep,
	    loop_index, closed_direction, parameter_tolerance, wrapper, entity_id,
	    entity_type);
	full_period_chain_microseconds +=
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count();
	if (regenerated) {
	    ++full_period_chain_successes;
	    if (loop_index >= 0 && loop_index < brep->m_L.Count())
		regenerated_full_period_boundaries.insert(
		    full_period_boundary_key(brep->m_L[loop_index], loop_index,
			closed_direction));
	}
	return regenerated;
    };
    const auto try_native_seam_split = [brep, wrapper, entity_id,
	    &entity_type, &native_seam_split_attempts,
	    &native_seam_split_successes,
	    &native_seam_split_microseconds](int loop_index, int trim_index,
	    const ON_Surface *surface, int closed_direction,
	    double open_parameter, bool record_repair, int *split_step_edge_id,
	    bool require_valid_affected_loops, bool proof_only,
	    bool isolated_candidate) {
	++native_seam_split_attempts;
	const std::chrono::steady_clock::time_point started =
	    std::chrono::steady_clock::now();
	const bool split = split_periodic_boundary_at_native_seam(brep,
	    loop_index, trim_index, surface, closed_direction, open_parameter,
	    wrapper, entity_id, entity_type, record_repair, split_step_edge_id,
	    require_valid_affected_loops, proof_only, isolated_candidate);
	native_seam_split_microseconds +=
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count();
	if (split)
	    ++native_seam_split_successes;
	return split;
    };
    const auto try_split_chain_regeneration = [brep, wrapper, entity_id,
	    &entity_type,
	    &split_chain_regeneration_attempts,
	    &split_chain_regeneration_successes,
	    &split_chain_regeneration_microseconds](int loop_index,
	    const ON_Surface *surface) {
	if (loop_index < 0 || loop_index >= brep->m_L.Count())
	    return false;
	++split_chain_regeneration_attempts;
	const std::chrono::steady_clock::time_point started =
	    std::chrono::steady_clock::now();
	const bool regenerated = regenerate_split_periodic_boundary_chain(brep,
	    brep->m_L[loop_index], surface, wrapper, entity_id, entity_type);
	split_chain_regeneration_microseconds +=
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count();
	if (regenerated)
	    ++split_chain_regeneration_successes;
	return regenerated;
    };
    const auto try_open_boundary_split = [brep, wrapper, entity_id,
	    &entity_type, &open_boundary_split_attempts,
	    &open_boundary_split_successes,
	    &open_boundary_split_microseconds](ON_BrepFace &face,
	    int loop_index, int closed_direction, double parameter_tolerance,
	    bool record_repair, const std::set<int> *excluded_step_edges,
	    int *split_step_edge_id, bool allow_exact_edge_fallback,
	    bool allow_topology_proven_winding_fallback) {
	++open_boundary_split_attempts;
	const std::chrono::steady_clock::time_point started =
	    std::chrono::steady_clock::now();
	const bool split = split_open_periodic_boundary_crossing(brep, face,
	    loop_index, closed_direction, parameter_tolerance, wrapper,
	    entity_id, entity_type, record_repair, excluded_step_edges,
	    split_step_edge_id, allow_exact_edge_fallback,
	    allow_topology_proven_winding_fallback);
	open_boundary_split_microseconds +=
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count();
	if (split)
	    ++open_boundary_split_successes;
	return split;
    };
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	if (brlcad::PullbackWorkCancelled())
	    break;
	ON_BrepFace &face = brep->m_F[fi];
	const ON_Surface *surface = face.SurfaceOf();
	if (!surface || (!surface->IsClosed(0) && !surface->IsClosed(1)))
	    continue;
	if (only_source_loops && !only_source_loops->empty()) {
	    bool selected = false;
	    for (int fli = 0; fli < face.m_li.Count() && !selected; ++fli) {
		const int li = face.m_li[fli];
		selected = li >= 0 && li < brep->m_L.Count() &&
		    only_source_loops->find(brep->m_L[li].m_loop_user.i) !=
			only_source_loops->end();
	    }
	    if (!selected)
		continue;
	}
	++periodic_faces_visited;
	{
	    std::ostringstream detail;
	    detail << entity_type << " periodic-face=F" << fi;
	    if (face.m_face_user.i > 0)
		detail << "/shell#" << face.m_face_user.i;
	    detail << " loops=" << face.m_li.Count();
	    wrapper->SetProgressDetail("repairing implicit exact periodic face bands",
		entity_id, static_cast<uint64_t>(fi + 1),
		static_cast<uint64_t>(brep->m_F.Count()), "faces", detail.str());
	}
	int closed_direction = surface->IsClosed(0) ? 0 : 1;
	/* A toroidal face is closed in both parameter directions.  When its STEP
	 * bounds have already been split at a shared native seam, their complete
	 * pcurve chains prove which direction winds one full period.  Use that
	 * direction for the same exact band construction below.  A unique witness
	 * may select the direction, but does not authorize a topology change: the
	 * band constructor subsequently projects the complete immutable edge chain
	 * of the other boundary and requires a second exact full-period winding.
	 * This admits an asymmetric pair in which one supplied boundary already
	 * exposes its winding and the other is closed on a private periodic image,
	 * without guessing from two arbitrary torus curves. */
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
		    if (proven || prove_collapsed_boundary(loop, surface,
			    direction, proof_tolerance, &proven_open_parameter)) {
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
		    try_split_chain_regeneration(li, surface);
	    }
	    for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		const int li = face.m_li[fli];
		if (li < 0 || li >= brep->m_L.Count()) continue;
		const ON_BrepLoop &loop = brep->m_L[li];
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
		    bool have_sample = false;
		    bool valid_chain = loop.TrimCount() > 0;
		    double first_closed = 0.0;
		    double previous_closed = 0.0;
		    double minimum_closed = DBL_MAX;
		    double maximum_closed = -DBL_MAX;
		    double first_other = 0.0;
		    double final_other = 0.0;
		    for (int lti = 0; valid_chain && lti < loop.TrimCount(); ++lti) {
			const ON_BrepTrim *trim = loop.Trim(lti);
			const ON_Interval trim_domain = trim ? trim->Domain() :
			    ON_Interval::EmptyInterval;
			if (!trim || !trim_domain.IsIncreasing()) {
			    valid_chain = false;
			    break;
			}
			for (int sample = 0; sample <=
				kPcurveLocusScreeningSegments; ++sample) {
			    const ON_3dPoint point = trim->PointAt(
				trim_domain.ParameterAt(static_cast<double>(sample) /
				    kPcurveLocusScreeningSegments));
			    if (!point.IsValid() || !std::isfinite(point[direction]) ||
				    !std::isfinite(point[1 - direction])) {
				valid_chain = false;
				break;
			    }
			    double unwrapped = point[direction];
			    if (have_sample)
				unwrapped += round((previous_closed - unwrapped) /
				    domain.Length()) * domain.Length();
			    else {
				first_closed = unwrapped;
				first_other = point[1 - direction];
				have_sample = true;
			    }
			    previous_closed = unwrapped;
			    final_other = point[1 - direction];
			    minimum_closed = std::min(minimum_closed, unwrapped);
			    maximum_closed = std::max(maximum_closed, unwrapped);
			}
		    }
		    const bool exact_period_travel = valid_chain && have_sample &&
			fabs(fabs(previous_closed - first_closed) -
			    domain.Length()) <= direction_tolerance &&
			fabs((maximum_closed - minimum_closed) -
			    domain.Length()) <= direction_tolerance;
		    const ON_BrepTrim *closing_first = loop.Trim(0);
		    const ON_BrepTrim *closing_last =
			loop.Trim(loop.TrimCount() - 1);
		    const bool model_space_closure = exact_period_travel &&
			closing_first && closing_last &&
			periodic_loop_closure(brep, &loop, closing_last,
			    closing_first, direction_tolerance);
		    const bool full_winding = exact_period_travel &&
			(fabs(final_other - first_other) <= other_tolerance ||
			    model_space_closure);
		    if (wrapper->Verbose() && loop.TrimCount() == 2) {
			const ON_BrepTrim *first_trim = loop.Trim(0);
			const ON_BrepTrim *second_trim = loop.Trim(1);
			const ON_BrepEdge *first_edge = first_trim ?
			    first_trim->Edge() : NULL;
			const ON_BrepEdge *second_edge = second_trim ?
			    second_trim->Edge() : NULL;
			if (first_edge && second_edge &&
				first_edge->m_edge_user.i > 0 &&
				first_edge->m_edge_user.i ==
				    second_edge->m_edge_user.i)
			    std::cerr << entity_type << " #" << entity_id
				<< ": split-chain winding witness F" << fi
				<< "/L" << li << "/STEP edge "
				<< first_edge->m_edge_user.i << " direction="
				<< direction << " valid="
				<< (valid_chain && have_sample ? "yes" : "no")
				<< " net/span="
				<< (have_sample ?
				    previous_closed - first_closed : DBL_MAX)
				<< '/' << (have_sample ?
				    maximum_closed - minimum_closed : DBL_MAX)
				<< " open-residual="
				<< (have_sample ?
				    final_other - first_other : DBL_MAX)
				<< " period=" << domain.Length()
				<< " accepted=" << (full_winding ? "yes" : "no")
				<< std::endl;
		    }
		    if (full_winding)
			++winding_witnesses[direction];
		}
	    }
	    if (winding_witnesses[0] >= 1 &&
		    winding_witnesses[0] > winding_witnesses[1])
		closed_direction = 0;
	    else if (winding_witnesses[1] >= 1 &&
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
	/* A repeated pole vertex can divide one STEP EDGE_LOOP into boundary
	 * chains on the two sheets of a horn torus.  Separate that exact topology
	 * before ordinary cap/band classification; after the split each private
	 * face is singly periodic and follows the standard repair path. */
	if (surface->IsClosed(open_direction) &&
		split_repeated_interior_pole_face(brep, fi, closed_direction,
		    wrapper, entity_id, entity_type)) {
	    --fi;
	    continue;
	}
	/* A multi-loop spherical face is a cap only when its other bounds are
	 * contractible holes.  If another complete loop also winds once around
	 * the closed parameter direction, the two loops bound an annular band and
	 * must be paired by the band repair below.  Sample the supplied pcurves in
	 * loop order and unwrap only integral-period images.  This is deliberately
	 * a conservative exclusion proof: an uncertain sibling prevents a pole
	 * cut, but can never authorize one. */
	/* Return -1 or +1 for a proven full-period winding, zero for a proven
	 * contractible loop, and 2 when the supplied chain is inconclusive.  The
	 * small parameter-space test handles ordinary exact data.  Its relaxed
	 * path is still exact in model space: it requires the established STEP
	 * topology vertex and both periodic surface lifts to agree within their
	 * local OpenNURBS tolerance through periodic_loop_closure().  This avoids
	 * rejecting a real spherical-cap boundary merely because independently
	 * regenerated pcurves differ by a few additional parameter ulps. */
	const auto loop_periodic_winding = [brep, surface, closed_direction, open_direction,
		&closed_domain, parameter_tolerance](const ON_BrepLoop &candidate) {
	    if (candidate.TrimCount() < 1)
		return 2;
	    bool have_sample = false;
	    double first_closed = 0.0;
	    double previous_closed = 0.0;
	    double minimum_closed = 0.0;
	    double maximum_closed = 0.0;
	    double first_open = 0.0;
	    double previous_open = 0.0;
	    const double period = closed_domain.Length();
	    for (int lti = 0; lti < candidate.TrimCount(); ++lti) {
		const ON_BrepTrim *candidate_trim = candidate.Trim(lti);
		if (!candidate_trim || !candidate_trim->Domain().IsIncreasing())
		    return 2;
		const ON_Interval trim_domain = candidate_trim->Domain();
		for (int sample = 0; sample <= kPcurveLocusScreeningSegments;
			sample++) {
		    const ON_3dPoint point = candidate_trim->PointAt(
			trim_domain.ParameterAt(static_cast<double>(sample) /
			kPcurveLocusScreeningSegments));
		    if (!point.IsValid() ||
			    !std::isfinite(point[closed_direction]) ||
			    !std::isfinite(point[open_direction]))
			return 2;
		    double unwrapped = point[closed_direction];
		    if (have_sample) {
			unwrapped += round((previous_closed - unwrapped) /
			    period) * period;
		    } else {
			first_closed = unwrapped;
			minimum_closed = unwrapped;
			maximum_closed = unwrapped;
			first_open = point[open_direction];
			have_sample = true;
		    }
		    previous_closed = unwrapped;
		    previous_open = point[open_direction];
		    minimum_closed = std::min(minimum_closed, unwrapped);
		    maximum_closed = std::max(maximum_closed, unwrapped);
		}
	    }
	    if (!have_sample)
		return 2;
	    const double net_travel = previous_closed - first_closed;
	    const double parameter_span = maximum_closed - minimum_closed;
	    const double open_tolerance = std::max(ON_ZERO_TOLERANCE *
		kNumericalToleranceScale, kPeriodicParameterSnapFraction *
		std::max(1.0, fabs(first_open)));
	    const int winding = net_travel < 0.0 ? -1 : 1;
	    if (fabs(fabs(net_travel) - period) <= parameter_tolerance &&
		fabs(parameter_span - period) <= parameter_tolerance &&
		fabs(previous_open - first_open) <= open_tolerance)
		return winding;
	    const double closure_parameter_tolerance = parameter_tolerance *
		std::max(1.0, period);
	    const ON_BrepTrim *first = candidate.Trim(0);
	    const ON_BrepTrim *last = candidate.Trim(
		candidate.TrimCount() - 1);
	    /* Winding number is determined by the directed net travel, not by
	     * monotonicity.  A legitimate scalloped cap boundary may briefly
	     * backtrack across its starting longitude, so its covered parameter
	     * span can be slightly greater than one period even though the closed
	     * chain has exactly one winding.  Requiring span == period rejected
	     * such faces and left their final UV join open by one period.  Admit a
	     * bounded non-monotone chain only as a classification witness: the
	     * full-period regenerator below still projects every immutable STEP
	     * edge and validates the complete replacement transactionally before
	     * any pole topology is installed. */
	    const bool bounded_single_winding_span =
		parameter_span > 0.5 * period &&
		parameter_span < 1.5 * period;
	    if (fabs(fabs(net_travel) - period) <=
		    closure_parameter_tolerance &&
		    bounded_single_winding_span &&
		    periodic_loop_closure(brep, &candidate, last, first,
			parameter_tolerance))
		return winding;
	    bool topology_lift_closure = false;
	    if (first && last && last->m_vi[1] >= 0 &&
		    last->m_vi[1] == first->m_vi[0] &&
		    last->m_vi[1] < brep->m_V.Count()) {
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
		const ON_3dPoint first_lift = closed_surface_point_at(
		    surface, first->PointAtStart());
		const ON_3dPoint last_lift = closed_surface_point_at(
		    surface, last->PointAtEnd());
		const ON_3dPoint &vertex =
		    brep->m_V[last->m_vi[1]].point;
		topology_lift_closure = first_lift.IsValid() &&
		    last_lift.IsValid() &&
		    first_lift.DistanceTo(vertex) <= topology_tolerance &&
		    last_lift.DistanceTo(vertex) <= topology_tolerance &&
		    first_lift.DistanceTo(last_lift) <= topology_tolerance;
	    }
	    if (fabs(net_travel) <= closure_parameter_tolerance &&
		    parameter_span < period - closure_parameter_tolerance &&
		    (fabs(previous_open - first_open) <= open_tolerance ||
			topology_lift_closure))
		return 0;
	    return 2;
	};
	const auto loop_winds_full_period = [&loop_periodic_winding](
		const ON_BrepLoop &candidate) {
	    const int winding = loop_periodic_winding(candidate);
	    return winding == -1 || winding == 1;
	};
	const auto compute_exact_edge_chain_winding_seam = [brep, surface, wrapper,
		entity_id, &entity_type,
		closed_direction, open_direction, &closed_domain,
		parameter_tolerance](const ON_BrepLoop &candidate,
		double *seam_parameter, double *seam_tolerance) {
	    if (!seam_parameter || !seam_tolerance ||
		    candidate.TrimCount() < 2)
		return false;
	    const double period = closed_domain.Length();
	    if (!(period > ON_ZERO_TOLERANCE))
		return false;
	    const bool surface_closed[2] = {
		surface->IsClosed(0), surface->IsClosed(1)
	    };
	    const ON_Interval surface_domains[2] = {
		surface->Domain(0), surface->Domain(1)
	    };
	    brlcad::PullbackContext context;
	    bool have_sample = false;
	    double first_closed = 0.0;
	    double previous_closed = 0.0;
	    double minimum_closed = DBL_MAX;
	    double maximum_closed = -DBL_MAX;
	    double first_open = 0.0;
	    double previous_open = 0.0;
	    ON_3dPoint first_lift = ON_3dPoint::UnsetPoint;
	    ON_3dPoint final_lift = ON_3dPoint::UnsetPoint;
	    double chain_tolerance = std::max(LocalUnits::tolerance,
		ON_ZERO_TOLERANCE * kNumericalToleranceScale);
	    for (int lti = 0; lti < candidate.TrimCount(); ++lti) {
		const ON_BrepTrim *trim = candidate.Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (!trim || !edge || !edge->Domain().IsIncreasing() ||
			!trim->Domain().IsIncreasing()) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": exact boundary-chain seam proof L"
			    << candidate.m_loop_index << " rejected invalid trim "
			    << lti << std::endl;
		    return false;
		}
		/* This proof authorizes only a topology-preserving private surface
		 * reparameterization.  In safe mode an earlier dense edge/surface or
		 * edge/vertex proof may have established that the source itself
		 * exceeds its declared uncertainty.  Use that measured local
		 * OpenNURBS tolerance here as well; --exact deliberately retains only
		 * the asserted model tolerance.  The complete chain winding and common
		 * topology vertex are independently proven below, and the seam move
		 * transactionally revalidates every affected pcurve. */
		double proof_tolerance = std::max(LocalUnits::tolerance,
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale);
		if (!wrapper->ImportOptions().exact) {
		    proof_tolerance = std::max(proof_tolerance,
			std::max(edge->m_tolerance,
			    std::max(trim->m_tolerance[0],
				trim->m_tolerance[1])));
		    if (trim->m_vi[0] >= 0 &&
			    trim->m_vi[0] < brep->m_V.Count())
			proof_tolerance = std::max(proof_tolerance,
			    brep->m_V[trim->m_vi[0]].m_tolerance);
		    if (trim->m_vi[1] >= 0 &&
			    trim->m_vi[1] < brep->m_V.Count())
			proof_tolerance = std::max(proof_tolerance,
			    brep->m_V[trim->m_vi[1]].m_tolerance);
		}
		chain_tolerance = std::max(chain_tolerance, proof_tolerance);
		const double solver_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    proof_tolerance * 0.1);
		const int samples = std::min(256,
		    std::max(64, edge->SpanCount() * 4));
		for (int sample = 0; sample <= samples; ++sample) {
		    if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
			return false;
		    const double fraction = static_cast<double>(sample) / samples;
		    const ON_3dPoint edge_point = edge->PointAt(
			edge->Domain().ParameterAt(trim->m_bRev3d ?
			    1.0 - fraction : fraction));
		    const ON_3dPoint supplied = trim->PointAt(
			trim->Domain().ParameterAt(fraction));
		    ON_2dPoint seed(supplied.x, supplied.y);
		    if (have_sample) {
			seed[closed_direction] = previous_closed;
			seed[open_direction] = previous_open;
		    }
		    ON_2dPoint uv = ON_2dPoint::UnsetPoint;
		    ON_3dPoint lift;
		    double distance = DBL_MAX;
		    bool projected = edge_point.IsValid() && seed.IsValid() &&
			context.SurfaceClosestPointFromSeed(surface, edge_point,
			    seed, uv, lift, distance, proof_tolerance,
			    surface_closed, surface_domains, solver_tolerance) &&
			distance <= proof_tolerance;
		    if (!projected)
			projected = edge_point.IsValid() &&
			    context.SurfaceClosestPoint(surface, edge_point, uv,
				lift, distance, 0, solver_tolerance,
				proof_tolerance) &&
			    distance <= proof_tolerance;
		    if (!projected) {
			if (wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id
				<< ": exact boundary-chain seam proof L"
				<< candidate.m_loop_index << "/T"
				<< trim->m_trim_index << "/STEP edge "
				<< edge->m_edge_user.i << " rejected projection "
				<< sample << '/' << samples << " distance="
				<< distance << " tolerance=" << proof_tolerance
				<< std::endl;
			return false;
		    }
		    double closed = uv[closed_direction];
		    double open = uv[open_direction];
		    if (have_sample) {
			closed += round((previous_closed - closed) / period) * period;
			if (surface->IsClosed(open_direction)) {
			    const double open_period =
				surface_domains[open_direction].Length();
			    if (open_period > ON_ZERO_TOLERANCE)
				open += round((previous_open - open) /
				    open_period) * open_period;
			}
		    } else {
			first_closed = closed;
			first_open = open;
			first_lift = lift;
			have_sample = true;
		    }
		    previous_closed = closed;
		    previous_open = open;
		    final_lift = lift;
		    minimum_closed = std::min(minimum_closed, closed);
		    maximum_closed = std::max(maximum_closed, closed);
		}
	    }
	    if (!have_sample)
		return false;
	    double open_closure = previous_open - first_open;
	    if (surface->IsClosed(open_direction)) {
		const double open_period = surface_domains[open_direction].Length();
		if (open_period > ON_ZERO_TOLERANCE)
		    open_closure -= round(open_closure / open_period) * open_period;
	    }
	    const double open_tolerance = std::max(parameter_tolerance,
		kPeriodicParameterSnapFraction * std::max(1.0,
		    surface_domains[open_direction].Length()));
	    const double net_travel = previous_closed - first_closed;
	    const double covered_span = maximum_closed - minimum_closed;
	    bool full_period_proven =
		fabs(fabs(net_travel) - period) <= parameter_tolerance &&
		fabs(covered_span - period) <= parameter_tolerance &&
		fabs(open_closure) <= open_tolerance;
	    if (!full_period_proven) {
		const ON_BrepTrim *first_trim = candidate.Trim(0);
		const ON_BrepTrim *last_trim = candidate.Trim(
		    candidate.TrimCount() - 1);
		const int closure_vertex = first_trim ?
		    first_trim->m_vi[0] : -1;
		const bool closed_topology = first_trim && last_trim &&
		    closure_vertex >= 0 && closure_vertex < brep->m_V.Count() &&
		    last_trim->m_vi[1] == closure_vertex;
		const ON_3dPoint topology_vertex = closed_topology ?
		    brep->m_V[closure_vertex].point :
		    ON_3dPoint::UnsetPoint;
		const long long winding = llround(net_travel / period);
		const bool single_winding = (winding == -1 || winding == 1) &&
		    fabs(net_travel) > 0.5 * period &&
		    fabs(net_travel) < 1.5 * period &&
		    covered_span > 0.5 * period &&
		    covered_span < 1.5 * period;
		full_period_proven = closed_topology && single_winding &&
		    first_lift.IsValid() && final_lift.IsValid() &&
		    topology_vertex.IsValid() &&
		    first_lift.DistanceTo(topology_vertex) <= chain_tolerance &&
		    final_lift.DistanceTo(topology_vertex) <= chain_tolerance &&
		    first_lift.DistanceTo(final_lift) <= chain_tolerance;
	    }
	    if (!full_period_proven) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": exact boundary-chain seam proof L"
			<< candidate.m_loop_index << "/STEP"
			<< candidate.m_loop_user.i << " rejected winding net/span="
			<< net_travel << '/' << covered_span << " period="
			<< period << " open=" << open_closure << " tolerance="
			<< chain_tolerance << std::endl;
		return false;
	    }
	    double native_seam = first_closed -
		floor((first_closed - closed_domain.Min()) / period) * period;
	    if (native_seam > closed_domain.Max())
		native_seam -= period;
	    if (native_seam < closed_domain.Min())
		native_seam += period;
	    *seam_parameter = native_seam;
	    *seam_tolerance = chain_tolerance;
	    return true;
	};
	const auto exact_edge_chain_winding_seam = [
		&compute_exact_edge_chain_winding_seam,
		&edge_chain_winding_attempts, &edge_chain_winding_successes,
		&edge_chain_winding_microseconds](const ON_BrepLoop &candidate,
		double *seam_parameter, double *seam_tolerance) {
	    if (!seam_parameter || !seam_tolerance)
		return false;
	    ++edge_chain_winding_attempts;
	    const std::chrono::steady_clock::time_point started =
		std::chrono::steady_clock::now();
	    const bool proven = compute_exact_edge_chain_winding_seam(candidate,
		seam_parameter, seam_tolerance);
	    edge_chain_winding_microseconds +=
		std::chrono::duration_cast<std::chrono::microseconds>(
		    std::chrono::steady_clock::now() - started).count();
	    if (proven)
		++edge_chain_winding_successes;
	    return proven;
	};

	/* STEP can bound a spherical cap intrinsically with two or more ordinary
	 * arcs which together wind once around the sphere.  OpenNURBS requires the
	 * equivalent rectangular parameter-space loop: the physical boundary,
	 * two opposite uses of an exact meridian seam, and a singular pole trim.
	 * Normalize the complete edge chain to one native period first, then use
	 * the existing densely validated pole-cut construction.  A cap may also
	 * contain inner holes.  Admit that form only when exactly one outer loop
	 * has a proven full-period winding and every sibling is independently
	 * proven contractible; an inconclusive sibling is left for the annular-band
	 * logic rather than authorizing a topology change. */
	int cap_boundary_loop = -1;
	bool cap_boundary_proven = true;
	bool explicit_periodic_topology = false;
	std::map<int, std::pair<double, double> > cap_boundary_seams;
	for (int fli = 0; fli < face.m_li.Count(); ++fli) {
	    const int li = face.m_li[fli];
	    if (li < 0 || li >= brep->m_L.Count()) {
		cap_boundary_proven = false;
		break;
	    }
	    const ON_BrepLoop &loop = brep->m_L[li];
	    /* A prior bounded pass may already have materialized a STEP cap as
	     * the explicit OpenNURBS rectangle: ordinary boundary trims, paired
	     * seam uses, and a singular pole trim.  Such a loop is no longer an
	     * implicit full-period boundary.  Feeding it back into the edge-only
	     * regenerator necessarily fails at the singular trim (which correctly
	     * has no 3-D edge); the former fallback then split one unrelated open
	     * edge and retried the entire loop indefinitely.  A singular trim by
	     * itself is not enough evidence: a partially materialized cap still
	     * needs this pass to construct its missing reciprocal seam.  Require
	     * both parts of the explicit pole topology before skipping the face. */
	    if (brlcad::step::LoopHasCompletePeriodicPoleTopology(loop)) {
		explicit_periodic_topology = true;
		/* A keyhole EDGE_LOOP may be separated into several loops on the
		 * same face.  Completing the pole topology of one component does not
		 * prove that its siblings are complete: another component can still
		 * be an intrinsic full-period cap boundary.  Ignore only this already
		 * explicit loop and continue classifying the remaining components.
		 * The later band scan independently skips every loop with singular
		 * topology, so doing this cannot feed a completed pole loop back into
		 * periodic-boundary reconstruction. */
		continue;
	    }
	    /* A partial pole loop must not enter the edge-only full-period
	     * regenerator: its singular trim correctly has no 3-D edge, so every
	     * candidate fails and the old retry path kept splitting unrelated
	     * ordinary edges without making progress.  Leave that loop for the
	     * dedicated singular/seam completion passes, but continue examining
	     * its siblings so a partial pole on one loop does not suppress a
	     * separately provable band or cap on the same face. */
	    if (loop_has_singular_topology(loop)) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": deferred partial periodic-pole loop L" << li
			<< "/STEP" << loop.m_loop_user.i
			<< " to singular/seam completion" << std::endl;
		continue;
	    }
	    int winding = loop_periodic_winding(loop);
	    /* Independently generated pcurves can make the inexpensive UV
	     * classifier inconclusive even though the immutable directed STEP
	     * edge chain proves one complete turn.  Use that stronger projection
	     * proof for classification only; the transactional full-period
	     * regenerator and pole-cut constructor below still have to validate
	     * the actual topology before anything is committed. */
	    if (loop.TrimCount() > 1 &&
		    (winding == -1 || winding == 1 || winding == 2)) {
		double seam_parameter = 0.0;
		double seam_tolerance = 0.0;
		if (exact_edge_chain_winding_seam(loop, &seam_parameter,
			&seam_tolerance)) {
		    if (winding == 2)
			winding = 1;
		    cap_boundary_seams[li] =
			std::make_pair(seam_parameter, seam_tolerance);
		}
	    }
	    if (wrapper->Verbose() && winding != 0)
		std::cerr << entity_type << " #" << entity_id
		    << ": periodic cap classification F" << fi << "/L" << li
		    << " winding=" << winding << " type=" << loop.m_type
		    << " trims=" << loop.TrimCount() << std::endl;
	    if (winding == -1 || winding == 1) {
		/* Face sense may reverse the preliminary OpenNURBS loop
		 * classification before the final orientation pass.  Exactly one
		 * winding boundary plus only proven contractible siblings is the
		 * invariant cap topology; the exact pole-cut constructor and final
		 * loop classifier remain authoritative about outer/inner sense. */
		if (cap_boundary_loop >= 0) {
		    cap_boundary_proven = false;
		    continue;
		}
		cap_boundary_loop = li;
	    } else if (winding != 0) {
		cap_boundary_proven = false;
		continue;
	    }
	}
	if (explicit_periodic_topology && cap_boundary_loop < 0) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": implicit periodic reconstruction skipped for F" << fi
		    << " because every periodic component already has explicit "
		    << "singular/pole topology" << std::endl;
	    continue;
	}
	/* A STEP keyhole on a horn torus can expose an ordinary full-period
	 * boundary on one side of an interior pole while sibling loops already
	 * carry densely proven singular trims on the other side.  The source
	 * face then occupies one uniquely provable sheet, but its analytic support
	 * surface remains closed across both sheets.  Put that pole on the boundary
	 * of a private exact surface before asking the ordinary cap constructor to
	 * add the remaining seam. */
	if (wrapper->Verbose() && cap_boundary_proven &&
		cap_boundary_loop >= 0 && face.m_li.Count() > 1 &&
		surface->IsClosed(open_direction))
	    std::cerr << entity_type << " #" << entity_id
		<< ": testing interior-pole sheet restriction F" << fi
		<< "/L" << cap_boundary_loop << " directions="
		<< closed_direction << '/' << open_direction << std::endl;
	if (cap_boundary_proven && cap_boundary_loop >= 0 &&
		face.m_li.Count() > 1 && surface->IsClosed(open_direction) &&
		restrict_periodic_face_to_interior_pole_sheet(brep, fi,
		    closed_direction, wrapper, entity_id, entity_type)) {
	    --fi;
	    continue;
	}
    /* A noncontractible full-period loop is a cap boundary only when the
     * surface actually collapses to a pole in the transverse direction.  A
     * torus or cylinder can temporarily expose just one of two band rings
     * while the other ring is being regenerated from a split STEP edge.  The
     * former code normalized that first ring, failed to insert a pole cut, and
     * then skipped the band candidate pass.  On the next repair pass the same
     * ring was mistaken for a cap again, leaving two FACE_OUTER_BOUND loops
     * structurally invalid.
     *
     * Some valid closed NURBS surfaces have a collapsed side without
     * advertising it through IsSingular().  Prove the complete side directly;
     * a torus side remains a finite circle and cannot pass. */
    const auto collapsed_transverse_side =
	[surface, closed_direction, open_direction](int side) {
	    const ON_Interval closed = surface->Domain(closed_direction);
	    const ON_Interval open = surface->Domain(open_direction);
	    if (!closed.IsIncreasing() || !open.IsIncreasing())
		return false;
	    ON_3dPoint reference = ON_3dPoint::UnsetPoint;
	    for (int sample = 0; sample <= 64; ++sample) {
		ON_3dPoint uv;
		uv[closed_direction] = closed.ParameterAt(
		    static_cast<double>(sample) / 64.0);
		uv[open_direction] = open[side];
		uv.z = 0.0;
		const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		if (!lift.IsValid())
		    return false;
		if (sample == 0)
		    reference = lift;
		else if (lift.DistanceTo(reference) >
			std::max(ON_ZERO_TOLERANCE *
			    kNumericalToleranceScale,
			    LocalUnits::tolerance))
		    return false;
	    }
	    return reference.IsValid();
	};
    const bool transverse_pole =
	surface->IsSingular(open_direction == 0 ? 0 : 1) ||
	surface->IsSingular(open_direction == 0 ? 2 : 3) ||
	collapsed_transverse_side(0) ||
	collapsed_transverse_side(1);

    if (cap_boundary_proven && cap_boundary_loop >= 0 && transverse_pole) {
	const int only_loop = cap_boundary_loop;
	if (regenerate_full_period_chain(only_loop,
		closed_direction, parameter_tolerance)) {
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
	    if (try_open_boundary_split(face, only_loop, closed_direction,
		    parameter_tolerance, true, NULL, NULL, false, false)) {
		--fi;
		continue;
	    }
	    /* A spherical or conical cap can wind exactly once around a private
	     * NURBS surface whose native seam is not incident on any supplied STEP
	     * vertex.  Splitting an arbitrary 3-D edge merely to satisfy that
	     * private parameterization is unnecessary.  The complete directed edge
	     * chain above already proves both the winding and a topology-vertex
	     * seam phase.  Move the private seam there transactionally, regenerate
	     * every affected pcurve from its immutable edge, and reconsider the
	     * face.  Restrict this retry to surfaces with a pole in the transverse
	     * direction; a noncontractible torus boundary is a face band, not a
	     * cap. */
	    if (transverse_pole && face.m_si >= 0 &&
		    aligned_band_surface_seams.find(face.m_si) ==
			aligned_band_surface_seams.end()) {
		double topology_seam = 0.0;
		double topology_seam_tolerance = LocalUnits::tolerance;
		std::string relocation_failure;
		const std::map<int, std::pair<double, double> >::const_iterator
		    saved_seam = cap_boundary_seams.find(only_loop);
		bool seam_proven = saved_seam != cap_boundary_seams.end();
		if (seam_proven) {
		    topology_seam = saved_seam->second.first;
		    topology_seam_tolerance = saved_seam->second.second;
		} else {
		    seam_proven = exact_edge_chain_winding_seam(
			brep->m_L[only_loop], &topology_seam,
			&topology_seam_tolerance);
		}
		if (seam_proven && relocate_closed_surface_loop_seam(brep,
			only_loop, closed_direction, topology_seam_tolerance,
			wrapper, entity_id, entity_type, &relocation_failure,
			topology_seam)) {
		    aligned_band_surface_seams.insert(face.m_si);
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"aligned a cap surface seam with its proven periodic "
			"boundary vertex");
		    --fi;
		    continue;
		}
		aligned_band_surface_seams.insert(face.m_si);
		if (wrapper->Verbose() && !relocation_failure.empty())
		    std::cerr << entity_type << " #" << entity_id
			<< ": periodic cap surface-seam alignment F" << fi
			<< "/L" << only_loop << " rejected: "
			<< relocation_failure << std::endl;
	    }
	}

	struct BoundaryCandidate {
	    int loop_index;
	    std::vector<int> trim_indices;
	    std::vector<int> period_shifts;
	    int vertex_index;
	    double open_parameter;
	    int open_period_shift;
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
	    if (loop_has_singular_topology(loop))
		continue;
	    /* A doubly-periodic boundary may already have been split in the
	     * surface's other closed direction by an adjacent face use.  Once the
	     * complete chain proves one winding in this face's selected direction,
	     * materialize its interior native-seam crossing as well.  Without that
	     * second exact cut the chain remains intrinsically closed (its first and
	     * last UV endpoints differ by one period), so it cannot participate in
	     * the Euclidean OpenNURBS band loop below.  The splitter changes every
	     * use of the affected 3-D edge and independently validates the crossing;
	     * if no open edge contains the native seam, it is a no-op. */
	    if (face.m_li.Count() > 1 && loop.TrimCount() > 1 &&
		    loop_winds_full_period(loop) &&
		    try_open_boundary_split(face, li, closed_direction,
			parameter_tolerance, true, NULL, NULL, true, false)) {
		topology_split = true;
		break;
	    }
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
		    (full_period_proven || prove_collapsed_boundary(loop, surface,
			 closed_direction, parameter_tolerance,
			 &proven_open_parameter))) {
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
		if (!proven_vertex_on_native_seam) {
		    /* A proven isoparametric boundary gives the splitter an exact
		     * open coordinate and lets it rebuild a closed edge whose
		     * arbitrary pcurve start already lies at the native seam.  On a
		     * doubly-periodic surface that supplied coordinate can be an
		     * equivalent but wrong branch, so the finite attempt remains
		     * transactional and falls back to deriving the seam height from
		     * the immutable 3-D edge. */
		    bool native_seam_split = false;
		    /* Only a doubly-periodic surface makes the supplied open
		     * coordinate branch ambiguous.  On a singly-periodic cap or
		     * band it is the exact, uniquely bounded coordinate and must
		     * be tried in the ordinary pass when this boundary owns its STEP
		     * edge.  A boundary edge repeated in a sibling loop is different,
		     * as are the two singleton components separated from one STEP
		     * keyhole loop: splitting either use changes the paired band
		     * topology, so defer it until the coordinated retry has
		     * established final loop ownership.  The AP203 repair-order guard
		     * covers a singleton plus a compound component that needs the
		     * ordinary pass; the paired-singleton guard covers two
		     * singleton components that must be deferred. */
		    const bool open_direction_is_periodic =
			surface->IsClosed(1 - closed_direction);
		    bool source_loop_shared_by_singleton_sibling = false;
		    bool step_edge_shared_by_sibling_loop = false;
		    const int step_edge_id = edge->m_edge_user.i;
		    if (step_edge_id > 0) {
			for (int sibling_offset = 0;
				!step_edge_shared_by_sibling_loop &&
				    sibling_offset < face.m_li.Count();
				++sibling_offset) {
			    const int sibling_li = face.m_li[sibling_offset];
			    if (sibling_li == li || sibling_li < 0 ||
				    sibling_li >= brep->m_L.Count())
				continue;
			    const ON_BrepLoop &sibling = brep->m_L[sibling_li];
			    /* A keyhole split creates two loops with the same stable
			     * STEP loop identity even when compaction has already
			     * replaced one shared edge object.  That identity alone
			     * is sufficient to require coordinated processing. */
			    if (loop.TrimCount() == 1 &&
				    loop.m_loop_user.i > 0 &&
				    sibling.m_loop_user.i ==
					loop.m_loop_user.i &&
				    sibling.TrimCount() == 1) {
				source_loop_shared_by_singleton_sibling = true;
				step_edge_shared_by_sibling_loop = true;
				break;
			    }
			    for (int sibling_trim_offset = 0;
				    sibling_trim_offset < sibling.TrimCount();
				    ++sibling_trim_offset) {
				const ON_BrepTrim *sibling_trim =
				    sibling.Trim(sibling_trim_offset);
				const ON_BrepEdge *sibling_edge =
				    sibling_trim ? sibling_trim->Edge() : NULL;
				if (sibling_edge &&
					sibling_edge->m_edge_user.i ==
					    step_edge_id) {
				    step_edge_shared_by_sibling_loop = true;
				    break;
				}
			    }
			}
		    }
		    if (wrapper->Verbose() &&
			    (!open_direction_is_periodic ||
				allow_deferred_isocurve_split))
			std::cerr << entity_type << " #" << entity_id
			    << ": periodic boundary ownership F" << fi << "/L"
			    << li << "/STEP edge " << step_edge_id
			    << " singleton-source-loop-shared="
			    << (source_loop_shared_by_singleton_sibling ?
				"yes" : "no")
			    << " edge-shared="
			    << (step_edge_shared_by_sibling_loop ? "yes" : "no")
			    << " deferred-pass="
			    << (allow_deferred_isocurve_split ? "yes" : "no")
			    << std::endl;
		    if ((!open_direction_is_periodic &&
			    !step_edge_shared_by_sibling_loop) ||
			    allow_deferred_isocurve_split)
			native_seam_split = try_native_seam_split(li,
			    trim->m_trim_index, surface, closed_direction,
			    proven_open_parameter, true, NULL, false, false,
			    false);
		    if (!native_seam_split)
			native_seam_split = try_native_seam_split(li,
			    trim->m_trim_index, surface, closed_direction,
			    std::numeric_limits<double>::quiet_NaN(), true, NULL,
			    false, false, false);
		    if (native_seam_split) {
			topology_split = true;
			break;
		    }
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
			bool competing_full_period_boundary = false;
			if (loop.m_type == ON_BrepLoop::outer) {
			    for (int sibling_index = 0;
				    sibling_index < face.m_li.Count(); ++sibling_index) {
				const int sibling_li = face.m_li[sibling_index];
				if (sibling_li == li || sibling_li < 0 ||
					sibling_li >= brep->m_L.Count())
				    continue;
				const ON_BrepLoop &sibling = brep->m_L[sibling_li];
				bool sibling_winds =
				    loop_winds_full_period(sibling) ||
				    cap_boundary_seams.find(sibling_li) !=
					cap_boundary_seams.end();
				/* Independently generated pcurves may not yet share
				 * a coherent periodic branch, even though the
				 * immutable directed STEP edge chain already proves
				 * one complete winding.  Use the same stronger
				 * classifier as the multi-edge cap preflight above.
				 * Otherwise the first simple boundary can be turned
				 * into explicit pole topology before its sibling is
				 * recognized, and a valid annular band is then
				 * skipped on every later pass.  This is classification
				 * evidence only; the band constructor still has to
				 * validate both complete edge chains transactionally. */
				if (!sibling_winds && sibling.TrimCount() > 1) {
				    double sibling_seam = 0.0;
				    double sibling_tolerance = 0.0;
				    sibling_winds = exact_edge_chain_winding_seam(
					sibling, &sibling_seam,
					&sibling_tolerance);
				}
				if (sibling_winds) {
				    competing_full_period_boundary = true;
				    if (wrapper->Verbose())
					std::cerr << entity_type << " #" << entity_id
					    << ": periodic band classification F" << fi
					    << "/L" << li << "/STEP"
					    << loop.m_loop_user.i << " found winding "
					    << "sibling L" << sibling_li << "/STEP"
					    << sibling.m_loop_user.i << " type="
					    << static_cast<int>(sibling.m_type)
					    << " trims=" << sibling.TrimCount()
					    << " proof="
					    << (loop_winds_full_period(sibling) ?
						"pcurve" : "exact-edge-chain")
					    << std::endl;
				    break;
				}
			    }
			}
			/* A full-period outer boundary plus only contractible inner
			 * loops is a cap with holes.  A second winding boundary is an
			 * annular band and needs two paired seam uses instead.  Scan
			 * every sibling: a valid band may also contain any number of
			 * ordinary contractible holes between its two winding
			 * boundaries. */
		    std::string pole_cut_failure;
		    if ((face.m_li.Count() == 1 ||
			    (loop.m_type == ON_BrepLoop::outer &&
			     !competing_full_period_boundary)) &&
			step_insert_periodic_pole_cut(brep, loop, surface,
			*trim, boundary_end, boundary_start,
			topology_tolerance, &pole_cut_failure)) {
			    wrapper->RecordRepair(entity_id, entity_type,
				"edge_loop",
				"inserted an exact paired seam cut for a proven full-period boundary and surface pole");
			    topology_split = true;
			    break;
			}
		    if (wrapper->Verbose() && !pole_cut_failure.empty())
			std::cerr << entity_type << " #" << entity_id
			    << ": periodic pole cut F" << fi << "/L" << li
			    << " rejected: " << pole_cut_failure << std::endl;
		    }
		}
	    }
	    /* A full-period boundary need not be an isocurve.  STEP also permits a
	     * single closed spline which winds once around a periodic surface while
	     * varying in the other parameter direction.  Its endpoint topology and
	     * pcurve establish the candidate cut; the splitter independently
	     * projects the complete immutable 3-D edge, proves exactly one winding,
	     * and densely validates both resulting edge/trim fragments before any
	     * topology is committed. */
	    const bool fixed_full_period_proved = full_period_proven ||
		(loop.m_loop_user.i > 0 && proven_full_period_boundaries.find(
		    proof_key) != proven_full_period_boundaries.end());
	    const ON_3dPoint supplied_start = trim ? trim->PointAtStart() :
		ON_3dPoint::UnsetPoint;
	    const ON_3dPoint supplied_end = trim ? trim->PointAtEnd() :
		ON_3dPoint::UnsetPoint;
	    const bool supplied_periodic_winding = supplied_start.IsValid() &&
		supplied_end.IsValid() &&
		fabs(fabs(supplied_end[closed_direction] -
		    supplied_start[closed_direction]) - closed_domain.Length()) <=
		    parameter_tolerance &&
		fabs(supplied_end[open_direction] -
		    supplied_start[open_direction]) <= parameter_tolerance;
	    const bool closed_singleton = loop.TrimCount() == 1 && trim && edge &&
		    !fixed_full_period_proved &&
		    edge->m_vi[0] == edge->m_vi[1] &&
		    trim->m_vi[0] == trim->m_vi[1];
	    /* Independently fitted endpoint parameters can be slightly
	     * contradictory even when both endpoint lifts satisfy their shared
	     * STEP vertex.  That is not enough to accept a boundary, but it is
	     * enough to ask the transactional exact-edge splitter for the real
	     * proof.  Bound the probe to the nearest one-period pcurve image.  The
	     * splitter then projects the complete immutable 3-D edge, proves one
	     * full winding, finds an exact native-seam point, and densely validates
	     * both child pcurves before committing any topology. */
	    bool topology_winding_probe = false;
	    if (closed_singleton && supplied_start.IsValid() &&
		    supplied_end.IsValid() && trim->m_vi[0] >= 0 &&
		    trim->m_vi[0] < brep->m_V.Count()) {
		const double period = closed_domain.Length();
		const double supplied_travel = fabs(
		    supplied_end[closed_direction] -
		    supplied_start[closed_direction]);
		double topology_tolerance = std::max(LocalUnits::tolerance,
		    std::max(edge->m_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
		topology_tolerance = std::max(topology_tolerance,
		    brep->m_V[trim->m_vi[0]].m_tolerance);
		const ON_3dPoint start_lift = surface->PointAt(
		    supplied_start.x, supplied_start.y);
		const ON_3dPoint end_lift = surface->PointAt(
		    supplied_end.x, supplied_end.y);
		const ON_3dPoint &vertex = brep->m_V[trim->m_vi[0]].point;
		topology_winding_probe = supplied_travel > 0.5 * period &&
		    supplied_travel < 1.5 * period && start_lift.IsValid() &&
		    end_lift.IsValid() && vertex.IsValid() &&
		    start_lift.DistanceTo(vertex) <= topology_tolerance &&
		    end_lift.DistanceTo(vertex) <= topology_tolerance;
	    }
	    if (closed_singleton) ++closed_singletons_seen;
	    if (closed_singleton && periodic_closure) ++periodic_singletons_seen;
	    if (closed_singleton && periodic_closure && supplied_periodic_winding)
		++supplied_winding_candidates;
	    bool arbitrary_split = false;
	    if (closed_singleton &&
		    ((periodic_closure && supplied_periodic_winding) ||
		     topology_winding_probe)) {
		++arbitrary_split_attempts;
		const std::chrono::steady_clock::time_point split_started =
		    std::chrono::steady_clock::now();
		arbitrary_split = try_native_seam_split(li, trim->m_trim_index,
		    surface, closed_direction,
		    std::numeric_limits<double>::quiet_NaN(), true, NULL, false,
		    false, false);
		arbitrary_split_microseconds +=
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - split_started).count();
	    }
	    if (arbitrary_split) {
		++arbitrary_split_successes;
		arbitrary_split_boundaries.insert(proof_key);
		topology_split = true;
		break;
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
		const bool proven_periodic_closure = periodic_closure ||
		    fixed_full_period_proved;
		if (!trim || !edge || edge->m_vi[0] != edge->m_vi[1] ||
			trim->m_vi[0] != trim->m_vi[1] ||
			trim->m_vi[0] != edge->m_vi[0] || !trim->TrimCurveOf() ||
			!proven_periodic_closure)
		    continue;
		const ON_3dPoint start = trim->PointAtStart();
		const ON_3dPoint end = trim->PointAtEnd();
		if (!start.IsValid() || !end.IsValid() ||
			(fabs(start[open_direction] - end[open_direction]) >
			    parameter_tolerance && !periodic_closure))
		    continue;
		BoundaryCandidate candidate;
		candidate.loop_index = li;
		candidate.trim_indices.push_back(trim->m_trim_index);
		/* Normalize a complete singleton winding onto the native surface
		 * interval before the two physical boundaries are oriented as the
		 * lower and upper sides of the band.  A decreasing exact boundary is
		 * commonly supplied as 0 -> -period rather than max -> min.  Those
		 * parameterizations have identical lifts, but treating the former as
		 * an unshifted candidate makes the later native-side check reject an
		 * otherwise exact band and leaves a one-trim loop open in Euclidean
		 * UV.  Record only the integral-period image implied by the proven
		 * winding; install_period_shifts() independently validates every
		 * translated sample before committing it. */
		const double period = closed_domain.Length();
		const double winding = end[closed_direction] -
		    start[closed_direction];
		const double native_start = winding >= 0.0 ?
		    closed_domain.Min() : closed_domain.Max();
		candidate.period_shifts.push_back(static_cast<int>(llround(
		    (native_start - start[closed_direction]) / period)));
		candidate.vertex_index = trim->m_vi[0];
		candidate.open_parameter = 0.5 *
		    (start[open_direction] + end[open_direction]);
		candidate.open_period_shift = 0;
		candidates.push_back(candidate);
		continue;
	    }
	    ON_BrepTrim *first_trim = loop.Trim(0);
	    ON_BrepTrim *second_trim = loop.Trim(1);
	    const bool proven_segmented_full_period = loop.TrimCount() > 1 &&
		(loop_winds_full_period(loop) ||
		 regenerated_full_period_boundaries.find(
		     full_period_boundary_key(loop, li, closed_direction)) !=
		     regenerated_full_period_boundaries.end());
	    if (loop.TrimCount() > 2 || proven_segmented_full_period ||
		    arbitrary_split_boundaries.find(proof_key) !=
			arbitrary_split_boundaries.end()) {
		++segmented_chain_scans;
		/* A periodic band boundary need not be a single circle.  AP214
		 * models commonly use a scalloped or segmented inner boundary whose
		 * intermediate pcurve joins are continuous, with only the final-to-
		 * first join separated by one native period.  Preserve the complete
		 * STEP chain as one candidate; the band construction below will join
		 * it to the other physical boundary with the one exact shared seam
		 * OpenNURBS requires.  Two-edge chains enter this general path only
		 * after loop_winds_full_period() has proved their complete winding, so
		 * ordinary two-edge face loops retain the cheaper specialized test. */
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
		    if (!cut_end.IsValid() || !cut_start.IsValid())
			continue;
		    const double cut_closed_gap = fabs(
			cut_end[closed_direction] - cut_start[closed_direction]);
		    const double cut_closed_residual = std::min(
			cut_closed_gap, fabs(cut_closed_gap - period));
		    const double cut_open_residual = fabs(
			cut_end[open_direction] - cut_start[open_direction]);
		    const double aligned_cut_start =
			cut_start[closed_direction] +
			round((cut_end[closed_direction] -
			    cut_start[closed_direction]) / period) * period;
		    const double cut_open_parameter = 0.5 *
			(cut_end[open_direction] + cut_start[open_direction]);
		    const bool exact_cut =
			fabs(cut_end[open_direction] -
			    cut_start[open_direction]) <= parameter_tolerance &&
			(cut_closed_gap <= parameter_tolerance ||
			 fabs(cut_closed_gap - period) <= parameter_tolerance);
		    /* A declared STEP vertex can be represented by independently
		     * supplied pcurve endpoints whose small residual exceeds a
		     * parameter-space heuristic while both edges have already
		     * established a tighter, model-space measured tolerance.  The
		     * band is the only Euclidean representation of this intrinsic
		     * winding, so permit that residual only after the endpoint,
		     * its adjacent periodic image, and their midpoint all lift to the
		     * shared topology vertex.  The endpoints are snapped
		     * transactionally and densely checked below. */
		    const bool bounded_cut = !exact_cut &&
			proven_chain_join(before, after,
			    cut_end[closed_direction], cut_end[open_direction],
			    aligned_cut_start, cut_start[open_direction]);
		    if (!exact_cut && !bounded_cut)
			continue;
		    const double native_image = closed_domain.Min() +
			round((cut_start[closed_direction] - closed_domain.Min()) /
			    period) * period;
		    const bool exact_native_cut =
			fabs(cut_start[closed_direction] - native_image) <=
			    parameter_tolerance &&
			fabs(cut_start[open_direction] - cut_open_parameter) <=
			    parameter_tolerance;
		    const bool bounded_native_cut = !exact_native_cut &&
			proven_chain_join(before, after, native_image,
			    cut_open_parameter, aligned_cut_start,
			    cut_start[open_direction]);
		    if (!exact_native_cut && !bounded_native_cut)
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
			double expected_open = cut_open_parameter;
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
			    const bool bounded_first_join = offset == 0 &&
				bounded_native_cut;
			    if (!exact_join && !bounded_first_join &&
				    (!previous_trim ||
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
			const bool exact_final_join =
			    fabs(expected_closed - target_end) <=
				parameter_tolerance &&
			    fabs(expected_open - cut_open_parameter) <=
				parameter_tolerance;
			/* bounded_cut proves only the small discrepancy at this
			 * cyclic cut.  It must not authorize the opposite winding
			 * direction, whose final endpoint can differ from the target
			 * by one or more complete periods. */
			const bool bounded_final_join = bounded_cut &&
			    fabs(expected_closed - target_end) <=
				parameter_tolerance + cut_closed_residual +
				    fabs(cut_start[closed_direction] -
					native_image) &&
			    fabs(expected_open - cut_open_parameter) <=
				parameter_tolerance + cut_open_residual;
			if (!continuous_chain || chain_trim_indices.empty() ||
				(!exact_final_join && !bounded_final_join)) {
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id
				    << ": segmented periodic boundary L" << li
				    << "/STEP" << loop.m_loop_user.i << " cut "
				    << cut << " direction="
				    << (increasing ? "increasing" : "decreasing")
				    << " rejected: continuous="
				    << (continuous_chain ? "yes" : "no")
				    << " trims=" << chain_trim_indices.size() << '/'
				    << loop.TrimCount() << " final="
				    << expected_closed << ':' << expected_open
				    << " target=" << target_end << ':'
				    << cut_open_parameter << " exact="
				    << (exact_final_join ? "yes" : "no")
				    << " bounded="
				    << (bounded_final_join ? "yes" : "no")
				    << " tolerance=" << parameter_tolerance
				    << std::endl;
			    continue;
			}
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
			candidate.open_parameter = cut_open_parameter;
			candidate.open_period_shift = 0;
			candidates.push_back(candidate);
			added = true;
		    }
		}
		if (added)
		    continue;
		if (loop.TrimCount() > 2)
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
	    const double chain_winding = second_end[closed_direction] -
		first_start[closed_direction];
	    const double native_chain_start = chain_winding >= 0.0 ?
		closed_domain.Min() : closed_domain.Max();
	    const int chain_period_shift = static_cast<int>(llround(
		(native_chain_start - first_start[closed_direction]) /
		closed_domain.Length()));
	    const double shifted_first_start = first_start[closed_direction] +
		chain_period_shift * closed_domain.Length();
	    const double shifted_second_end = second_end[closed_direction] +
		chain_period_shift * closed_domain.Length();
	    const bool increasing_periodic_chain =
		fabs(shifted_first_start - closed_domain.Min()) <=
		    parameter_tolerance &&
		fabs(shifted_second_end - closed_domain.Max()) <=
		    parameter_tolerance;
	    const bool decreasing_periodic_chain =
		fabs(shifted_first_start - closed_domain.Max()) <=
		    parameter_tolerance &&
		fabs(shifted_second_end - closed_domain.Min()) <=
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
	    /* Keep both pieces on the same exact periodic image.  In particular,
	     * a supplied 0 -> -period chain must become max -> min as a unit;
	     * independently normalizing its pieces would destroy their valid
	     * interior join.  install_period_shifts() densely proves that this
	     * integral translation preserves both surface lifts. */
	    candidate.period_shifts.push_back(chain_period_shift);
	    candidate.period_shifts.push_back(chain_period_shift);
	    candidate.vertex_index = first_trim->m_vi[0];
	    candidate.open_parameter = first_start[open_direction];
	    candidate.open_period_shift = 0;
	    candidates.push_back(candidate);
	}
	/* A supplied multi-edge boundary can cross the native parameter seam at
	 * an edge endpoint while its pcurve itself remains on the principal
	 * branch.  Whole-curve translations cannot unwrap that final edge.  Once
	 * the other physical boundary proves this is a periodic face band, rebuild
	 * the missing winding directly from the exact 3-D STEP edge chain.  The
	 * helper accepts only a native-seam topology vertex and densely validates
	 * every regenerated edge lift before changing the BREP. */
	/* Two complete winding chains may both close at private parameter-space
	 * phases rather than at the surface's native seam.  In that form neither
	 * is yet an OpenNURBS band candidate, although the immutable directed STEP
	 * edges independently prove both physical boundaries.  Align the private
	 * surface seam with one proven topology vertex; the next pass can retain
	 * that boundary on the native interval and materialize the exact crossing
	 * in its sibling.  This changes no model-space geometry and is attempted
	 * only when both loops have passed the dense edge/surface winding proof. */
	if (!topology_split && candidates.empty() && face.m_li.Count() == 2 &&
		face.m_si >= 0 && aligned_band_surface_seams.find(face.m_si) ==
		    aligned_band_surface_seams.end()) {
	    const int first_loop = face.m_li[0];
	    const int second_loop = face.m_li[1];
	    const std::map<int, std::pair<double, double> >::const_iterator
		first_seam = cap_boundary_seams.find(first_loop);
	    const std::map<int, std::pair<double, double> >::const_iterator
		second_seam = cap_boundary_seams.find(second_loop);
	    if (first_seam != cap_boundary_seams.end() &&
		    second_seam != cap_boundary_seams.end()) {
		std::string relocation_failure;
		const double seam_tolerance = std::max(first_seam->second.second,
		    second_seam->second.second);
		if (relocate_closed_surface_loop_seam(brep, first_loop,
			closed_direction, seam_tolerance, wrapper, entity_id,
			entity_type, &relocation_failure, first_seam->second.first)) {
		    aligned_band_surface_seams.insert(face.m_si);
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"aligned a closed surface seam with one of two densely proven periodic boundary vertices");
		    --fi;
		    continue;
		}
		aligned_band_surface_seams.insert(face.m_si);
		if (wrapper->Verbose() && !relocation_failure.empty())
		    std::cerr << entity_type << " #" << entity_id
			<< ": paired periodic boundary surface-seam alignment F"
			<< fi << " rejected: " << relocation_failure << std::endl;
	    }
	}

	const int other_loop = face.m_li.Count() == 2 && candidates.size() == 1 ?
	    (face.m_li[0] == candidates[0].loop_index ?
		face.m_li[1] : face.m_li[0]) : -1;
	/* If the exact boundary winds once but no existing topology vertex lies
	 * on the private OpenNURBS seam, prefer changing that private
	 * parameterization over splitting a shared STEP edge and every adjacent
	 * face use.  The complete immutable edge chain independently proves the
	 * winding and supplies the requested seam phase.  The relocation helper
	 * regenerates every pcurve on the affected surface transactionally and
	 * densely validates its lift before this face is reconsidered. */
	if (!topology_split && other_loop >= 0 &&
		other_loop < brep->m_L.Count() && face.m_si >= 0 &&
		aligned_band_surface_seams.find(face.m_si) ==
		    aligned_band_surface_seams.end()) {
	    double topology_seam = std::numeric_limits<double>::quiet_NaN();
	    double topology_seam_tolerance = LocalUnits::tolerance;
	    if (exact_edge_chain_winding_seam(brep->m_L[other_loop],
		    &topology_seam, &topology_seam_tolerance)) {
		std::string relocation_failure;
		if (relocate_closed_surface_loop_seam(brep, other_loop,
			closed_direction, topology_seam_tolerance, wrapper,
			entity_id, entity_type, &relocation_failure,
			topology_seam)) {
		    aligned_band_surface_seams.insert(face.m_si);
		    wrapper->RecordRepair(entity_id, entity_type,
			"trim_pcurve",
			"aligned a closed surface seam with a proven periodic boundary topology vertex");
		    if (topology_seam_tolerance > LocalUnits::tolerance)
			wrapper->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Warning,
			    entity_id, entity_type, "trim_pcurve",
			    "source periodic boundary exceeded the declared "
			    "tolerance; aligned the private surface seam using its "
			    "densely measured local OpenNURBS tolerance");
		    --fi;
		    continue;
		}
		if (wrapper->Verbose() && !relocation_failure.empty())
		    std::cerr << entity_type << " #" << entity_id
			<< ": periodic band surface-seam alignment F" << fi
			<< "/L" << other_loop << " rejected: "
			<< relocation_failure << std::endl;
	    }
	    aligned_band_surface_seams.insert(face.m_si);
	}
	if (!topology_split && other_loop >= 0) {
	    const std::pair<int, int> other_key =
		full_period_boundary_key(brep->m_L[other_loop], other_loop,
		    closed_direction);
	    if (regenerated_full_period_boundaries.find(other_key) ==
		    regenerated_full_period_boundaries.end() &&
		regenerate_full_period_chain(other_loop, closed_direction,
		    parameter_tolerance))
		topology_split = true;
	}
	/* Endpoint normalization can turn a valid arbitrary full-period pcurve
	 * into a closed native-domain image before this pass runs.  In that form
	 * the pcurve no longer proves its winding, even though its immutable closed
	 * 3-D STEP edge still does.  A sibling candidate already proves that this
	 * two-loop face is a periodic band.  Let the native-seam splitter project
	 * the other exact edge and independently require one complete winding
	 * before it changes topology.  This recovers the proof without treating a
	 * merely contractible closed curve as a band boundary. */
	if (!topology_split && other_loop >= 0 &&
		other_loop < brep->m_L.Count()) {
	    ON_BrepLoop &collapsed_loop = brep->m_L[other_loop];
	    ON_BrepTrim *collapsed_trim = collapsed_loop.TrimCount() == 1 ?
		collapsed_loop.Trim(0) : NULL;
	    const ON_BrepEdge *collapsed_edge = collapsed_trim ?
		collapsed_trim->Edge() : NULL;
	    if (collapsed_trim && collapsed_edge &&
		    collapsed_trim->m_vi[0] == collapsed_trim->m_vi[1] &&
		    collapsed_edge->m_vi[0] == collapsed_edge->m_vi[1] &&
		    try_native_seam_split(other_loop,
			collapsed_trim->m_trim_index, surface, closed_direction,
			std::numeric_limits<double>::quiet_NaN(), true, NULL,
			false, false, false))
		topology_split = true;
	}
	/* One boundary may already be a native full-period candidate while the
	 * other crosses the same private surface seam inside an open STEP edge.
	 * Materialize that exact crossing as a shared topology vertex, then restart
	 * this face with the compacted edge-use graph. */
	if (!topology_split && other_loop >= 0 &&
		try_open_boundary_split(face, other_loop, closed_direction,
		    parameter_tolerance, true, NULL, NULL, true, true))
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
		<< "/shell" << face.m_face_user.i << " rejected: " << reason
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
	/* Exact pullbacks on a closed surface can finish a few floating-point
	 * ulps outside a native parameter boundary.  Candidate discovery already
	 * proved the complete boundary lift and winding; snap only that numerical
	 * boundary residue before trimming the analytic seam isocurve.  Without
	 * this normalization ON_Curve::Trim rejects, for example, [-3e-13, 25.9]
	 * against a [0, 52.8] torus profile even though the first endpoint is the
	 * exact native minimum in model space. */
	const ON_Interval open_domain = surface->Domain(open_direction);
	if (!open_domain.IsIncreasing()) {
	    report_rejection("the open surface parameter domain was invalid");
	    continue;
	}
	for (std::vector<BoundaryCandidate>::iterator candidate =
		candidates.begin(); candidate != candidates.end(); ++candidate) {
	    /* On a torus the parameter direction transverse to the boundary is
	     * periodic too.  A valid supplied loop may therefore meet the chosen
	     * cut on an adjacent image (for example -0.1 instead of max-0.1).
	     * Move the complete boundary by an integral period onto the native
	     * image; install_period_shifts() densely proves that exact translation
	     * before any topology is committed. */
	    if (surface->IsClosed(open_direction)) {
		const double open_period = open_domain.Length();
		if (open_period > ON_ZERO_TOLERANCE) {
		    while (candidate->open_parameter < open_domain.Min() -
			    parameter_tolerance) {
			candidate->open_parameter += open_period;
			++candidate->open_period_shift;
		    }
		    while (candidate->open_parameter > open_domain.Max() +
			    parameter_tolerance) {
			candidate->open_parameter -= open_period;
			--candidate->open_period_shift;
		    }
		}
	    }
	    if (fabs(candidate->open_parameter - open_domain.Min()) <=
		    parameter_tolerance)
		candidate->open_parameter = open_domain.Min();
	    else if (fabs(candidate->open_parameter - open_domain.Max()) <=
		    parameter_tolerance)
		candidate->open_parameter = open_domain.Max();
	}
	if (candidates[1].open_parameter < candidates[0].open_parameter)
	    std::swap(candidates[0], candidates[1]);
	BoundaryCandidate lower = candidates[0];
	BoundaryCandidate upper = candidates[1];
	/* A doubly-periodic surface can represent its complete parameter-domain
	 * cut with two opposite uses of the same closed STEP edge.  Both pcurves
	 * then project to one physical circle and the same topology vertex, but
	 * they are the lift-equivalent minimum and maximum sides in the transverse
	 * periodic direction.  This is not a zero-width band: the reciprocal edge
	 * identity and opposite senses prove the two domain sides exactly, and the
	 * seam constructor below supplies the other reciprocal pair. */
	bool reciprocal_full_domain_cut = false;
	if (surface->IsClosed(open_direction) &&
		lower.vertex_index == upper.vertex_index &&
		lower.trim_indices.size() == 1 &&
		upper.trim_indices.size() == 1) {
	    const ON_BrepTrim *lower_trim = brep->m_T.At(
		lower.trim_indices.front());
	    const ON_BrepTrim *upper_trim = brep->m_T.At(
		upper.trim_indices.front());
	    const ON_BrepEdge *lower_edge = lower_trim ?
		lower_trim->Edge() : NULL;
	    const ON_BrepEdge *upper_edge = upper_trim ?
		upper_trim->Edge() : NULL;
	    const double open_period = open_domain.Length();
	    if (lower_trim && upper_trim && lower_edge && upper_edge &&
		    lower_trim->m_ei == upper_trim->m_ei &&
		    lower_edge->m_edge_user.i > 0 &&
		    lower_edge->m_edge_user.i == upper_edge->m_edge_user.i &&
		    lower_trim->m_bRev3d != upper_trim->m_bRev3d &&
		    open_period > ON_ZERO_TOLERANCE) {
		const auto increasing_alignment = [brep, surface, closed_direction,
			&closed_domain](const BoundaryCandidate &boundary) {
		    if (boundary.trim_indices.empty())
			return -DBL_MAX;
		    const ON_BrepTrim *trim = brep->m_T.At(
			boundary.trim_indices.front());
		    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		    ON_NurbsCurve edge_nurbs;
		    ON_3dPoint uv;
		    uv[closed_direction] = closed_domain.Mid();
		    uv[1 - closed_direction] = boundary.open_parameter;
		    uv.z = 0.0;
		    ON_3dPoint lift;
		    ON_3dVector du;
		    ON_3dVector dv;
		    double edge_parameter = 0.0;
		    if (!trim || !edge || !edge->GetNurbForm(edge_nurbs) ||
			    !surface->Ev1Der(uv.x, uv.y, lift, du, dv) ||
			    !ON_NurbsCurve_GetClosestPoint(&edge_parameter,
				&edge_nurbs, lift))
			return -DBL_MAX;
		    ON_3dVector surface_tangent = closed_direction == 0 ?
			du : dv;
		    ON_3dVector edge_tangent = edge_nurbs.TangentAt(
			edge_parameter);
		    if (trim->m_bRev3d)
			edge_tangent.Reverse();
		    return surface_tangent.Unitize() && edge_tangent.Unitize() ?
			surface_tangent * edge_tangent : -DBL_MAX;
		};
		if (increasing_alignment(lower) < 0.0 &&
			increasing_alignment(upper) > 0.0)
		    std::swap(lower, upper);
		lower.open_period_shift = static_cast<int>(llround(
		    (open_domain.Min() - lower.open_parameter) / open_period));
		upper.open_period_shift = static_cast<int>(llround(
		    (open_domain.Max() - upper.open_parameter) / open_period));
		lower.open_parameter = open_domain.Min();
		upper.open_parameter = open_domain.Max();
		reciprocal_full_domain_cut = true;
	    }
	}
	/* Two full-period boundaries on a horn torus may meet at its collapsed
	 * pole even though their transverse parameter intervals are distinct.
	 * Do not reject that singular band solely because both cycles use the same
	 * exact STEP vertex.  The endpoint and seam proofs below densely require
	 * both native parameter sides to lift to that vertex, and the completed
	 * topology remains transactional through OpenNURBS validation. */
	if (!reciprocal_full_domain_cut &&
		upper.open_parameter - lower.open_parameter <=
		    parameter_tolerance) {
	    report_rejection("the boundary intervals were not distinct");
	    continue;
	}
	/* Exact periodic boundaries supplied as several source edges can describe
	 * one circle whose 3-D locus is measurably separated from the associated
	 * surface.  Earlier single-edge regeneration measures this condition, but
	 * a valid segmented chain may never enter that path.  Measure each selected
	 * boundary trim against its immutable 3-D STEP edge before using its local
	 * tolerance for the native-side proof.  The shared verifier is dense,
	 * scale-bounded, and disabled by --exact; it changes only OpenNURBS
	 * tolerances and never moves source geometry or topology. */
	const auto verify_boundary_source_tolerances =
	    [brep, surface, wrapper, entity_id, &entity_type](
		const BoundaryCandidate &boundary) {
		for (std::vector<int>::const_iterator ti =
			boundary.trim_indices.begin();
			ti != boundary.trim_indices.end(); ++ti) {
		    if (*ti < 0 || *ti >= brep->m_T.Count())
			continue;
		    ON_BrepTrim &trim = brep->m_T[*ti];
		    ON_BrepEdge *edge = trim.Edge();
		    ON_NurbsCurve edge_nurbs;
		    if (!edge || !edge->GetNurbForm(edge_nurbs))
			continue;
		    const double tolerance = std::max(LocalUnits::tolerance,
			std::max(edge->m_tolerance,
			    std::max(trim.m_tolerance[0],
				trim.m_tolerance[1])));
		    (void)verified_source_pcurve_tolerance(trim, *edge, surface,
			edge_nurbs, tolerance, brep, wrapper, entity_id,
			entity_type);
		}
	    };
	verify_boundary_source_tolerances(lower);
	verify_boundary_source_tolerances(upper);
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
	bool lower_endpoint_proof = true;
	bool upper_endpoint_proof = true;
	for (int side = 0; side < 2; ++side) {
	    lower_endpoint_proof = lower_endpoint_proof &&
		lower_lifts[side].IsValid() &&
		lower_lifts[side].DistanceTo(lower_vertex) <=
		    band_tolerance;
	    upper_endpoint_proof = upper_endpoint_proof &&
		upper_lifts[side].IsValid() &&
		upper_lifts[side].DistanceTo(upper_vertex) <=
		    band_tolerance;
	}
	const bool endpoint_proof =
	    lower_endpoint_proof && upper_endpoint_proof;
	if (!endpoint_proof) {
	    /* A complete periodic boundary may be cut at an existing topology
	     * vertex opposite the private OpenNURBS surface seam.  Its supplied
	     * pcurve is exact, but testing the same open parameter at the native
	     * domain sides necessarily lifts to the antipode.  Move the private
	     * seam to that proven boundary vertex and transactionally regenerate
	     * every pcurve on the shared surface.  The relocation helper rejects a
	     * cut through an ordinary edge and densely validates all regenerated
	     * lifts, so this changes only parameterization, never geometry or STEP
	     * topology. */
	    const BoundaryCandidate &non_native_boundary =
		!lower_endpoint_proof ? lower : upper;
	    const ON_BrepTrim *boundary_start =
		non_native_boundary.trim_indices.empty() ? NULL :
		brep->m_T.At(non_native_boundary.trim_indices.front());
	    const ON_3dPoint boundary_start_uv = boundary_start ?
		boundary_start->PointAtStart() : ON_3dPoint::UnsetPoint;
	    const ON_3dPoint boundary_start_lift =
		boundary_start_uv.IsValid() ?
		surface->PointAt(boundary_start_uv.x, boundary_start_uv.y) :
		ON_3dPoint::UnsetPoint;
	    if (face.m_si >= 0 && boundary_start &&
		    boundary_start->m_vi[0] == non_native_boundary.vertex_index &&
		    boundary_start_lift.IsValid() &&
		    boundary_start_lift.DistanceTo(
			brep->m_V[non_native_boundary.vertex_index].point) <=
			band_tolerance &&
		    aligned_band_surface_seams.find(face.m_si) ==
			aligned_band_surface_seams.end()) {
		std::string relocation_failure;
		if (relocate_closed_surface_loop_seam(brep,
			non_native_boundary.loop_index, closed_direction,
			band_tolerance, wrapper, entity_id, entity_type,
			&relocation_failure,
			boundary_start_uv[closed_direction])) {
		    aligned_band_surface_seams.insert(face.m_si);
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"aligned a closed surface seam with a proven periodic boundary topology vertex");
		    --fi;
		    continue;
		}
		aligned_band_surface_seams.insert(face.m_si);
		if (wrapper->Verbose() && !relocation_failure.empty())
		    std::cerr << entity_type << " #" << entity_id
			<< ": periodic face-band topology-seam alignment F" << fi
			<< "/L" << non_native_boundary.loop_index
			<< " rejected: " << relocation_failure << std::endl;
	    }
	    if (wrapper->Verbose()) {
		std::cerr << entity_type << " #" << entity_id
		    << ": periodic face-band endpoint proof F" << fi
		    << " closed-direction=" << closed_direction
		    << " closed-domain=" << closed_domain.Min() << ':'
		    << closed_domain.Max()
		    << " open-direction=" << open_direction
		    << " open-domain=" << open_domain.Min() << ':'
		    << open_domain.Max()
		    << " lower=L" << lower.loop_index << "/V"
		    << lower.vertex_index << '@' << lower.open_parameter
		    << " distances="
		    << lower_lifts[0].DistanceTo(lower_vertex) << '/'
		    << lower_lifts[1].DistanceTo(lower_vertex)
		    << " upper=L" << upper.loop_index << "/V"
		    << upper.vertex_index << '@' << upper.open_parameter
		    << " distances="
		    << upper_lifts[0].DistanceTo(upper_vertex) << '/'
		    << upper_lifts[1].DistanceTo(upper_vertex)
		    << " tolerance=" << band_tolerance << std::endl;
	    }
	    report_rejection("a periodic boundary did not lift to its STEP vertex");
	    continue;
	}

	const ON_Interval seam_domain(lower.open_parameter,
	    upper.open_parameter);
	if (!seam_domain.IsIncreasing()) {
	    report_rejection("the exact surface seam domain was invalid");
	    continue;
	}
	/* At the horn of a degenerate torus, both native sides of the periodic
	 * cut collapse to the same topology vertex throughout the transverse
	 * interval.  OpenNURBS cannot trim a zero-length 3-D isocurve into an edge;
	 * it represents the two sides with singular trims instead.  Prove that
	 * collapse densely before selecting that representation. */
	bool collapsed_singular_seam =
	    lower.vertex_index == upper.vertex_index;
	for (int sample = 0; collapsed_singular_seam &&
		sample <= kDenseValidationSegments; ++sample) {
	    if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
		collapsed_singular_seam = false;
		break;
	    }
	    const double open_parameter = seam_domain.ParameterAt(
		static_cast<double>(sample) / kDenseValidationSegments);
	    ON_3dPoint uv_min;
	    ON_3dPoint uv_max;
	    uv_min[closed_direction] = closed_min;
	    uv_min[open_direction] = open_parameter;
	    uv_min.z = 0.0;
	    uv_max = uv_min;
	    uv_max[closed_direction] = closed_max;
	    const ON_3dPoint min_lift = surface->PointAt(uv_min.x, uv_min.y);
	    const ON_3dPoint max_lift = surface->PointAt(uv_max.x, uv_max.y);
	    collapsed_singular_seam = min_lift.IsValid() && max_lift.IsValid() &&
		min_lift.DistanceTo(lower_vertex) <= band_tolerance &&
		max_lift.DistanceTo(lower_vertex) <= band_tolerance;
	}

	std::unique_ptr<ON_Curve> seam_curve;
	if (!collapsed_singular_seam) {
	    seam_curve.reset(surface->IsoCurve(open_direction, closed_min));
	    if (!seam_curve || !seam_curve->Trim(seam_domain) ||
		    !seam_curve->IsValid()) {
		report_rejection("the exact surface isocurve could not be trimmed");
		continue;
	    }
	    if (seam_curve->PointAtStart().DistanceTo(lower_vertex) >
		    seam_curve->PointAtEnd().DistanceTo(lower_vertex) &&
		    !seam_curve->Reverse())
		continue;
	    bool seam_proof = true;
	    const ON_Interval curve_domain = seam_curve->Domain();
	    for (int sample = 0; seam_proof &&
		    sample <= kDenseValidationSegments; ++sample) {
		if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
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
	    if (!seam_proof ||
		    seam_curve->PointAtStart().DistanceTo(lower_vertex) >
			band_tolerance ||
		    seam_curve->PointAtEnd().DistanceTo(upper_vertex) >
			band_tolerance) {
		report_rejection(
		    "the surface seam did not pass dense lift validation");
		continue;
	    }
	}

	std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	const auto install_period_shifts = [brep, surface, closed_direction,
		open_direction, &closed_domain, &open_domain](
		const BoundaryCandidate &boundary) {
	    if (boundary.trim_indices.size() != boundary.period_shifts.size())
		return false;
	    for (size_t offset = 0; offset < boundary.trim_indices.size(); ++offset) {
		const int ti = boundary.trim_indices[offset];
		const int period_shift = boundary.period_shifts[offset];
		if (period_shift == 0 && boundary.open_period_shift == 0)
		    continue;
		if (ti < 0 || ti >= brep->m_T.Count())
		    return false;
		ON_BrepTrim &trim = brep->m_T[ti];
		std::unique_ptr<ON_Curve> translated(trim.DuplicateCurve());
		ON_Xform transform(ON_Xform::IdentityTransformation);
		transform.m_xform[closed_direction][3] =
		    period_shift * closed_domain.Length();
		transform.m_xform[open_direction][3] =
		    boundary.open_period_shift * open_domain.Length();
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
		const ON_3dPoint previous_lift =
		    closed_surface_point_at(surface, previous_uv);
		const ON_3dPoint next_lift =
		    closed_surface_point_at(surface, next_uv);
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
			const ON_3dPoint candidate_lift = closed_surface_point_at(
			    surface, candidate_uv);
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
	std::string boundary_side_failure;
	const auto snap_boundary_sides = [brep, surface, closed_direction,
		closed_min, closed_max, &boundary_tolerance,
		&snapped_band_joins,
		&boundary_side_failure](const BoundaryCandidate &boundary) {
	    const auto reject = [&boundary_side_failure, &boundary](
		    const char *reason) {
		std::ostringstream stream;
		stream << "L" << boundary.loop_index << ": " << reason;
		boundary_side_failure = stream.str();
		return false;
	    };
	    if (boundary.trim_indices.empty() || boundary.vertex_index < 0 ||
		    boundary.vertex_index >= brep->m_V.Count())
		return reject("invalid boundary trim or topology vertex");
	    const int first_index = boundary.trim_indices.front();
	    const int last_index = boundary.trim_indices.back();
	    if (first_index < 0 || first_index >= brep->m_T.Count() ||
		    last_index < 0 || last_index >= brep->m_T.Count())
		return reject("boundary trim index is outside the BREP");
	    ON_BrepTrim &first = brep->m_T[first_index];
	    ON_BrepTrim &last = brep->m_T[last_index];
	    if (first.m_vi[0] != boundary.vertex_index ||
		    last.m_vi[1] != boundary.vertex_index)
		return reject("boundary chain does not close at its proven STEP vertex");

	    const ON_3dPoint current_start = first.PointAtStart();
	    const ON_3dPoint current_end = last.PointAtEnd();
	    if (!current_start.IsValid() || !current_end.IsValid())
		return reject("boundary curve has an invalid endpoint");
	    const double increasing_cost =
		fabs(current_start[closed_direction] - closed_min) +
		fabs(current_end[closed_direction] - closed_max);
	    const double decreasing_cost =
		fabs(current_start[closed_direction] - closed_max) +
		fabs(current_end[closed_direction] - closed_min);
	    ON_3dPoint target_start;
	    ON_3dPoint target_end;
	    target_start[closed_direction] =
		increasing_cost <= decreasing_cost ? closed_min : closed_max;
	    target_end[closed_direction] =
		increasing_cost <= decreasing_cost ? closed_max : closed_min;
	    target_start[1 - closed_direction] = boundary.open_parameter;
	    target_end[1 - closed_direction] = boundary.open_parameter;
	    target_start.z = 0.0;
	    target_end.z = 0.0;

	    const double tolerance = boundary_tolerance(boundary);
	    const ON_3dPoint &vertex =
		brep->m_V[boundary.vertex_index].point;
	    const auto endpoint_maximum_distance = [surface, &vertex](
		    const ON_3dPoint &source, const ON_3dPoint &target) {
		const ON_3dPoint source_lift =
		    closed_surface_point_at(surface, source);
		const ON_3dPoint target_lift = surface->PointAt(
		    target.x, target.y);
		const ON_3dPoint middle = 0.5 * (source + target);
		const ON_3dPoint middle_lift =
		    closed_surface_point_at(surface, middle);
		if (!source_lift.IsValid() || !target_lift.IsValid() ||
			!middle_lift.IsValid())
		    return DBL_MAX;
		return std::max(source_lift.DistanceTo(vertex),
		    std::max(target_lift.DistanceTo(vertex),
			middle_lift.DistanceTo(vertex)));
	    };
	    const double start_distance =
		endpoint_maximum_distance(current_start, target_start);
	    const double end_distance =
		endpoint_maximum_distance(current_end, target_end);
	    if (start_distance > tolerance || end_distance > tolerance) {
		std::ostringstream reason;
		reason << "native-side endpoint image exceeds the measured "
		    "topology tolerance (start/end=" << start_distance << '/'
		    << end_distance << ", tolerance=" << tolerance
		    << ", current=" << current_start.x << ':'
		    << current_start.y << "->" << current_end.x << ':'
		    << current_end.y << ", target=" << target_start.x << ':'
		    << target_start.y << "->" << target_end.x << ':'
		    << target_end.y << ')';
		return reject(reason.str().c_str());
	    }
	    const bool change_start =
		current_start.DistanceTo(target_start) > ON_ZERO_TOLERANCE;
	    const bool change_end =
		current_end.DistanceTo(target_end) > ON_ZERO_TOLERANCE;
	    /* Surface-seam relocation or full-chain regeneration may already
	     * have placed both endpoints exactly on the native sides.  Some
	     * valid proxy/polycurve types deliberately refuse SetStartPoint() or
	     * SetEndPoint() even when asked to set the value they already have.
	     * Treat the existing, independently lift-proven endpoint as complete
	     * instead of turning that no-op into a repair failure. */
	    if (!change_start && !change_end)
		return true;

	    struct EndpointCandidate {
		int trim_index;
		std::unique_ptr<ON_Curve> curve;
	    };
	    std::vector<EndpointCandidate> endpoint_candidates;
	    endpoint_candidates.push_back({first_index,
		std::unique_ptr<ON_Curve>(first.DuplicateCurve())});
	    if (last_index != first_index)
		endpoint_candidates.push_back({last_index,
		    std::unique_ptr<ON_Curve>(last.DuplicateCurve())});
	    if (!endpoint_candidates[0].curve ||
		    (last_index != first_index &&
		     !endpoint_candidates[1].curve))
		return reject("could not duplicate a boundary pcurve");
	    if (change_start &&
		    !endpoint_candidates[0].curve->SetStartPoint(target_start))
		return reject("could not set the first pcurve endpoint to the native side");
	    EndpointCandidate &last_candidate = last_index == first_index ?
		endpoint_candidates[0] : endpoint_candidates[1];
	    if (change_end && !last_candidate.curve->SetEndPoint(target_end))
		return reject("could not set the last pcurve endpoint to the native side");

	    for (std::vector<EndpointCandidate>::iterator candidate =
		    endpoint_candidates.begin();
		    candidate != endpoint_candidates.end(); ++candidate) {
		ON_BrepTrim &original = brep->m_T[candidate->trim_index];
		if (!candidate->curve->ChangeDimension(2) ||
			!candidate->curve->IsValid() ||
			!original.Domain().IsIncreasing())
		    return reject("edited boundary pcurve or parameter domain is invalid");
		const ON_Interval domain = original.Domain();
		const int samples = std::min(4096, std::max(64,
		    std::max(original.SpanCount(),
			candidate->curve->SpanCount()) * 8));
		for (int sample = 0; sample <= samples; ++sample) {
		    if ((sample & 63) == 0 &&
			    brlcad::PullbackWorkCancelled())
			return reject("boundary endpoint validation was cancelled");
		    const double parameter = domain.ParameterAt(
			static_cast<double>(sample) / samples);
		    const ON_3dPoint original_uv =
			original.PointAt(parameter);
		    const ON_3dPoint candidate_uv =
			candidate->curve->PointAt(parameter);
		    const ON_3dPoint original_lift =
			closed_surface_point_at(surface, original_uv);
		    const ON_3dPoint candidate_lift =
			closed_surface_point_at(surface, candidate_uv);
		    if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
			    original_lift.DistanceTo(candidate_lift) >
				tolerance)
			return reject("edited boundary pcurve exceeds the measured lift tolerance");
		}
	    }
	    for (std::vector<EndpointCandidate>::iterator candidate =
		    endpoint_candidates.begin();
		    candidate != endpoint_candidates.end(); ++candidate) {
		ON_BrepTrim &trim = brep->m_T[candidate->trim_index];
		const int c2 = brep->AddTrimCurve(
		    candidate->curve.release());
		if (c2 < 0 || !brep->SetTrimCurve(trim, c2))
		    return reject("could not install the edited boundary pcurve");
		brep->SetTrimIsoFlags(trim);
	    }
	    if (change_start)
		++snapped_band_joins;
	    if (change_end)
		++snapped_band_joins;
	    return true;
	};
	if (!snap_boundary_joins(lower) || !snap_boundary_joins(upper)) {
	    report_rejection("a proven boundary join could not be closed within its measured topology tolerance");
	    *brep = *rollback;
	    continue;
	}
	if (!snap_boundary_sides(lower) || !snap_boundary_sides(upper)) {
	    const std::string reason =
		"a proven boundary seam endpoint could not be closed: " +
		boundary_side_failure;
	    report_rejection(reason.c_str());
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
	if (!reciprocal_full_domain_cut &&
		(!orient_boundary(lower.trim_indices, true) ||
		 !orient_boundary(upper.trim_indices, false))) {
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
	if (!reciprocal_full_domain_cut &&
		(lower_start.DistanceTo(lower_uv_min) > parameter_tolerance ||
		lower_end.DistanceTo(lower_uv_max) > parameter_tolerance ||
		upper_start.DistanceTo(upper_uv_max) > parameter_tolerance ||
		upper_end.DistanceTo(upper_uv_min) > parameter_tolerance)) {
	    report_rejection("the supplied boundaries were not on the native periodic sides");
	    *brep = *rollback;
	    continue;
	}
	/* The reciprocal full-domain pair must be literal OpenNURBS seam sides,
	 * not merely pcurves whose small transverse fitting residual lifts within
	 * tolerance.  Rebuild both from their exact native-domain endpoints and
	 * accept each line only after dense comparison with its directed immutable
	 * STEP edge.  This also gives later seam refreshes unambiguous S/N or W/E
	 * ISO classifications on a doubly-periodic surface. */
	if (reciprocal_full_domain_cut) {
	    const auto install_exact_side = [brep, surface, band_tolerance,
		    wrapper, entity_id, &entity_type](
		    int trim_index, const ON_3dPoint &start,
		    const ON_3dPoint &end) {
		if (trim_index < 0 || trim_index >= brep->m_T.Count())
		    return false;
		ON_BrepTrim &trim = brep->m_T[trim_index];
		const ON_BrepEdge *edge = trim.Edge();
		ON_NurbsCurve edge_nurbs;
		if (!edge || !edge->Domain().IsIncreasing() ||
			!edge->GetNurbForm(edge_nurbs))
		    return false;
		ON_Arc exact_arc;
		const bool have_exact_arc = edge->IsArc(NULL, &exact_arc,
		    band_tolerance);
		std::unique_ptr<ON_LineCurve> candidate(
		    new ON_LineCurve(start, end));
		if (!candidate || !candidate->ChangeDimension(2) ||
			!candidate->IsValid() ||
			!candidate->Domain().IsIncreasing())
		    return false;
		const ON_Interval candidate_domain = candidate->Domain();
		for (int sample = 0; sample <= kDenseValidationSegments;
			++sample) {
		    if ((sample & 63) == 0 &&
			    brlcad::PullbackWorkCancelled())
			return false;
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint uv = candidate->PointAt(
			candidate_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    double closest_parameter = 0.0;
		    double locus_distance = DBL_MAX;
		    if (lift.IsValid() && ON_NurbsCurve_GetClosestPoint(
			    &closest_parameter, &edge_nurbs, lift))
			locus_distance = lift.DistanceTo(
			    edge_nurbs.PointAt(closest_parameter));
		    if (lift.IsValid() && have_exact_arc)
			locus_distance = std::min(locus_distance,
			    lift.DistanceTo(exact_arc.ClosestPointTo(lift)));
		    if (!lift.IsValid() || locus_distance > band_tolerance) {
			if (wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id
				<< ": reciprocal full-domain side T" << trim_index
				<< " locus failed at " << sample << '/'
				<< kDenseValidationSegments << " distance="
				<< locus_distance << " tolerance="
				<< band_tolerance << " exact-arc="
				<< have_exact_arc << std::endl;
			return false;
		    }
		}
		const double midpoint = candidate_domain.Mid();
		const ON_3dPoint midpoint_uv = candidate->PointAt(midpoint);
		ON_3dPoint midpoint_lift;
		ON_3dVector du;
		ON_3dVector dv;
		double closest_parameter = 0.0;
		if (!surface->Ev1Der(midpoint_uv.x, midpoint_uv.y,
			midpoint_lift, du, dv) ||
			!ON_NurbsCurve_GetClosestPoint(&closest_parameter,
			    &edge_nurbs, midpoint_lift))
		    return false;
		const ON_3dVector parameter_direction = end - start;
		ON_3dVector lifted_tangent =
		    parameter_direction.x * du + parameter_direction.y * dv;
		ON_3dVector edge_tangent = edge_nurbs.TangentAt(
		    closest_parameter);
		if (trim.m_bRev3d)
		    edge_tangent.Reverse();
		const double alignment = lifted_tangent.Unitize() &&
		    edge_tangent.Unitize() ? lifted_tangent * edge_tangent : -1.0;
		if (alignment <= 0.0) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": reciprocal full-domain side T" << trim_index
			    << " direction alignment=" << alignment
			    << " reversed=" << trim.m_bRev3d << std::endl;
		    return false;
		}
		const int c2 = brep->AddTrimCurve(candidate.release());
		return c2 >= 0 && brep->SetTrimCurve(trim, c2);
	    };
	    if (!install_exact_side(lower.trim_indices.front(), lower_uv_min,
		    lower_uv_max) ||
		!install_exact_side(upper.trim_indices.front(), upper_uv_max,
		    upper_uv_min)) {
		report_rejection("the reciprocal full-domain sides did not densely match their exact STEP edge");
		*brep = *rollback;
		continue;
	    }
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

	const int seam_c3 = collapsed_singular_seam ? -1 :
	    brep->AddEdgeCurve(seam_curve.release());
	const int maximum_c2 = brep->AddTrimCurve(maximum_trim_curve.release());
	const int minimum_c2 = brep->AddTrimCurve(minimum_trim_curve.release());
	if ((!collapsed_singular_seam && seam_c3 < 0) ||
		maximum_c2 < 0 || minimum_c2 < 0) {
	    *brep = *rollback;
	    continue;
	}
	const ON_Surface::ISO maximum_iso = closed_direction == 0 ?
	    ON_Surface::E_iso : ON_Surface::N_iso;
	const ON_Surface::ISO minimum_iso = closed_direction == 0 ?
	    ON_Surface::W_iso : ON_Surface::S_iso;
	int maximum_trim_index = -1;
	int minimum_trim_index = -1;
	if (collapsed_singular_seam) {
	    ON_BrepLoop &target_loop = brep->m_L[lower.loop_index];
	    maximum_trim_index = brep->NewSingularTrim(
		brep->m_V[lower.vertex_index], target_loop, maximum_iso,
		maximum_c2).m_trim_index;
	    minimum_trim_index = brep->NewSingularTrim(
		brep->m_V[lower.vertex_index], target_loop, minimum_iso,
		minimum_c2).m_trim_index;
	} else {
	    ON_BrepEdge &seam_edge = brep->NewEdge(
		brep->m_V[lower.vertex_index],
		brep->m_V[upper.vertex_index], seam_c3, NULL,
		band_tolerance);
	    seam_edge.m_tolerance = band_tolerance;
	    const int seam_edge_index = seam_edge.m_edge_index;
	    maximum_trim_index = brep->NewTrim(seam_edge, false,
		brep->m_L[lower.loop_index], maximum_c2).m_trim_index;
	    minimum_trim_index = brep->NewTrim(
		brep->m_E[seam_edge_index], true,
		brep->m_L[lower.loop_index], minimum_c2).m_trim_index;
	}
	if (maximum_trim_index < 0 || minimum_trim_index < 0) {
	    *brep = *rollback;
	    continue;
	}

	ON_BrepLoop &combined_loop = brep->m_L[lower.loop_index];
	ON_BrepLoop &discarded_loop = brep->m_L[upper.loop_index];
	const bool merged_supplied_outer_bounds =
	    combined_loop.m_type == ON_BrepLoop::outer &&
	    discarded_loop.m_type == ON_BrepLoop::outer;
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
	if (reciprocal_full_domain_cut) {
	    const ON_Surface::ISO lower_side = open_direction == 0 ?
		ON_Surface::W_iso : ON_Surface::S_iso;
	    const ON_Surface::ISO upper_side = open_direction == 0 ?
		ON_Surface::E_iso : ON_Surface::N_iso;
	    for (std::vector<int>::const_iterator ti = lower.trim_indices.begin();
		    ti != lower.trim_indices.end(); ++ti) {
		brep->m_T[*ti].m_type = ON_BrepTrim::seam;
		brep->m_T[*ti].m_iso = lower_side;
	    }
	    for (std::vector<int>::const_iterator ti = upper.trim_indices.begin();
		    ti != upper.trim_indices.end(); ++ti) {
		brep->m_T[*ti].m_type = ON_BrepTrim::seam;
		brep->m_T[*ti].m_iso = upper_side;
	    }
	}
	brep->m_T[maximum_trim_index].m_type = collapsed_singular_seam ?
	    ON_BrepTrim::singular : ON_BrepTrim::seam;
	brep->m_T[minimum_trim_index].m_type = collapsed_singular_seam ?
	    ON_BrepTrim::singular : ON_BrepTrim::seam;
	brep->m_T[maximum_trim_index].m_iso = maximum_iso;
	brep->m_T[minimum_trim_index].m_iso = minimum_iso;
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
	/* Candidate discovery proves periodic winding and model-space lift, but a
	 * boundary reversal also changes each trim's directed 3-D edge use.  Make
	 * the face modification transactional with respect to OpenNURBS' own trim,
	 * loop, and face invariants; otherwise an ambiguous torus band can survive
	 * this pass and fail much later with a pcurve endpoint attached to the wrong
	 * end of an otherwise exact edge. */
	ON_wString band_messages;
	ON_TextLog band_log(band_messages);
	bool valid_band = fi >= 0 && fi < brep->m_F.Count();
	const ON_BrepFace *installed_face = valid_band ? &brep->m_F[fi] : NULL;
	for (int fli = 0; valid_band && fli < installed_face->m_li.Count(); ++fli) {
	    const int installed_loop_index = installed_face->m_li[fli];
	    if (installed_loop_index < 0 ||
		    installed_loop_index >= brep->m_L.Count()) {
		valid_band = false;
		break;
	    }
	    const ON_BrepLoop &installed_loop =
		brep->m_L[installed_loop_index];
	    for (int lti = 0; valid_band && lti < installed_loop.TrimCount();
		    ++lti) {
		const ON_BrepTrim *installed_trim = installed_loop.Trim(lti);
		valid_band = installed_trim && installed_trim->IsValid(&band_log);
	    }
	    valid_band = valid_band && installed_loop.IsValid(&band_log);
	}
	valid_band = valid_band && installed_face->IsValid(&band_log);
	if (!valid_band) {
	    if (wrapper->Verbose()) {
		ON_String text(band_messages);
		std::cerr << entity_type << " #" << entity_id
		    << ": periodic face-band candidate F" << fi
		    << " failed transactional OpenNURBS validation:\n"
		    << text.Array();
	    }
	    *brep = *rollback;
	    continue;
	}
	++repaired;
	for (size_t snapped_join = 0; snapped_join < snapped_band_joins;
		snapped_join++)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"closed a proven periodic-band boundary join within its measured topology tolerance");
	for (int translated_boundary = 0;
		translated_boundary < (lower.open_period_shift != 0 ? 1 : 0) +
		    (upper.open_period_shift != 0 ? 1 : 0);
		++translated_boundary)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"translated a complete band boundary onto an exact adjacent periodic surface image");
	if (collapsed_singular_seam)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"inserted exact singular trims at a collapsed periodic-band pole");
	else
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"inserted an exact OpenNURBS seam for an implicit periodic STEP face band");
	if (reciprocal_full_domain_cut)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"materialized opposite uses of one closed STEP edge as the lift-equivalent sides of a doubly periodic surface-domain boundary");
	if (merged_supplied_outer_bounds)
	    wrapper->RecordRepair(entity_id, entity_type, "face_bound",
		"merged multiple supplied FACE_OUTER_BOUND components into one exact periodic face boundary");
	}
    const long long elapsed_microseconds =
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - pass_started).count();
    wrapper->RecordStageTiming("periodic_band_repair", entity_id,
	entity_type, static_cast<uint64_t>(std::max<long long>(0,
	    elapsed_microseconds)), static_cast<uint64_t>(brep->m_F.Count()),
	static_cast<uint64_t>(brep->m_E.Count()),
	static_cast<uint64_t>(brep->m_T.Count()));
	/* Preserve the already measured proof costs in normal structured reports.
	 * These distinguish surface/loop scanning from exact curve reconstruction
	 * without requiring a second verbose corpus run. */
    if (arbitrary_split_attempts)
	wrapper->RecordStageTiming("periodic_band_repair/arbitrary_split",
	    entity_id, entity_type, static_cast<uint64_t>(std::max<long long>(0,
		arbitrary_split_microseconds)), static_cast<uint64_t>(brep->m_F.Count()),
	    static_cast<uint64_t>(brep->m_E.Count()),
	    static_cast<uint64_t>(brep->m_T.Count()));
    if (collapsed_boundary_attempts)
	wrapper->RecordStageTiming("periodic_band_repair/collapsed_boundary",
	    entity_id, entity_type, static_cast<uint64_t>(std::max<long long>(0,
		collapsed_boundary_microseconds)), static_cast<uint64_t>(brep->m_F.Count()),
	    static_cast<uint64_t>(brep->m_E.Count()),
	    static_cast<uint64_t>(brep->m_T.Count()));
    if (full_period_chain_attempts)
	wrapper->RecordStageTiming("periodic_band_repair/full_period_chain",
	    entity_id, entity_type, static_cast<uint64_t>(std::max<long long>(0,
		full_period_chain_microseconds)), static_cast<uint64_t>(brep->m_F.Count()),
	    static_cast<uint64_t>(brep->m_E.Count()),
	    static_cast<uint64_t>(brep->m_T.Count()));
    if (native_seam_split_attempts)
	wrapper->RecordStageTiming("periodic_band_repair/native_seam_split",
	    entity_id, entity_type, static_cast<uint64_t>(std::max<long long>(0,
		native_seam_split_microseconds)),
	    static_cast<uint64_t>(brep->m_F.Count()),
	    static_cast<uint64_t>(brep->m_E.Count()),
	    static_cast<uint64_t>(brep->m_T.Count()));
    if (open_boundary_split_attempts)
	wrapper->RecordStageTiming("periodic_band_repair/open_boundary_split",
	    entity_id, entity_type, static_cast<uint64_t>(std::max<long long>(0,
		open_boundary_split_microseconds)),
	    static_cast<uint64_t>(brep->m_F.Count()),
	    static_cast<uint64_t>(brep->m_E.Count()),
	    static_cast<uint64_t>(brep->m_T.Count()));
    if (edge_chain_winding_attempts)
	wrapper->RecordStageTiming("periodic_band_repair/edge_chain_winding",
	    entity_id, entity_type, static_cast<uint64_t>(std::max<long long>(0,
		edge_chain_winding_microseconds)),
	    static_cast<uint64_t>(brep->m_F.Count()),
	    static_cast<uint64_t>(brep->m_E.Count()),
	    static_cast<uint64_t>(brep->m_T.Count()));
    if (split_chain_regeneration_attempts)
	wrapper->RecordStageTiming(
	    "periodic_band_repair/split_chain_regeneration", entity_id,
	    entity_type, static_cast<uint64_t>(std::max<long long>(0,
		split_chain_regeneration_microseconds)),
	    static_cast<uint64_t>(brep->m_F.Count()),
	    static_cast<uint64_t>(brep->m_E.Count()),
	    static_cast<uint64_t>(brep->m_T.Count()));
    if (wrapper->Verbose()) {
	std::cerr << entity_type << " #" << entity_id
	    << ": periodic-band pass telemetry elapsed="
	    << elapsed_microseconds / 1000.0 << "ms faces="
	    << periodic_faces_visited << " closed-singletons="
	    << closed_singletons_seen << " periodic-singletons="
	    << periodic_singletons_seen << " supplied-winding="
	    << supplied_winding_candidates << " arbitrary-splits="
	    << arbitrary_split_successes << '/' << arbitrary_split_attempts
	    << " arbitrary-time=" << arbitrary_split_microseconds / 1000.0
	    << "ms segmented-scans=" << segmented_chain_scans
	    << " collapsed=" << collapsed_boundary_successes << '/'
	    << collapsed_boundary_attempts << " collapsed-time="
	    << collapsed_boundary_microseconds / 1000.0 << "ms full-period="
	    << full_period_chain_successes << '/' << full_period_chain_attempts
	    << " full-period-time=" << full_period_chain_microseconds / 1000.0
	    << "ms native-seam=" << native_seam_split_successes << '/'
	    << native_seam_split_attempts << " native-time="
	    << native_seam_split_microseconds / 1000.0
	    << "ms open-boundary=" << open_boundary_split_successes << '/'
	    << open_boundary_split_attempts << " open-time="
	    << open_boundary_split_microseconds / 1000.0 << "ms"
	    << " edge-chain=" << edge_chain_winding_successes << '/'
	    << edge_chain_winding_attempts << " edge-chain-time="
	    << edge_chain_winding_microseconds / 1000.0 << "ms"
	    << " split-chain=" << split_chain_regeneration_successes << '/'
	    << split_chain_regeneration_attempts << " split-chain-time="
	    << split_chain_regeneration_microseconds / 1000.0 << "ms"
	    << " cancelled=" << (brlcad::PullbackWorkCancelled() ? "yes" : "no")
	    << std::endl;
    }
    return repaired;
}


size_t
finalize_brep_topology(ON_Brep *brep, bool normalize_keyholes,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	bool *topology_changed)
{
    if (topology_changed) *topology_changed = false;
    if (!brep)
	return 0;
    if (wrapper && wrapper->ImportOptions().exact)
	normalize_keyholes = false;

    /* FACE_OUTER_BOUND and FACE_BOUND are authoritative.  Recomputing loop
     * types from still-unrepaired pcurves can turn a declared outer loop into
     * an inner loop and make an otherwise exact face structurally invalid.
     * Trim types still need the complete edge-use graph. */
    if (wrapper)
	wrapper->SetProgressDetail("refreshing exact BREP topology flags",
	    entity_id, 0, 0, std::string(), entity_type);
    refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
	entity_id, &entity_type);
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
    size_t slit_rejections = 0;
    const size_t slit_diagnostic_limit = 16;
    if (normalize_keyholes) {
	bool removed_face = true;
	while (removed_face) {
	    if (brlcad::PullbackWorkCancelled())
		return keyhole_splits;
	    removed_face = remove_one_exact_redundant_zero_area_face(brep,
		wrapper, entity_id, entity_type);
	    if (!removed_face)
		break;
	    if (wrapper)
		wrapper->RecordRepair(entity_id, entity_type, "advanced_face",
		    "removed an exact zero-area face whose cancelling edge uses "
		    "duplicated the two neighboring shell uses");
	    if (topology_changed) *topology_changed = true;
	    refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
		entity_id, &entity_type);
	    classify_exact_polyline_seams(brep);
	}
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
		std::string slit_failure;
		int removed_loop_step_id = 0;
		int removed_edge_step_id = 0;
		double source_edge_mismatch = 0.0;
		bool removed_source_slit = false;
		if (!remove_adjacent_zero_area_slit(brep, li, &slit_failure,
			&removed_loop_step_id, &removed_edge_step_id,
			&source_edge_mismatch, &removed_source_slit)) {
		    if (wrapper && wrapper->Verbose() && !slit_failure.empty() &&
			    slit_rejections < slit_diagnostic_limit) {
			std::cerr << entity_type << " #" << entity_id << ": "
			    << slit_failure << std::endl;
			++slit_rejections;
		    }
		    continue;
		}
		if (wrapper) {
		    const int repair_id = removed_loop_step_id > 0 ?
			removed_loop_step_id : entity_id;
		    const std::string repair_type = removed_loop_step_id > 0 ?
			"EDGE_LOOP" : entity_type;
		    std::ostringstream repair;
		    repair << (removed_source_slit ?
			"removed a source-authored zero-width manifold slit" :
			"removed an exact zero-area reciprocal seam bound");
		    if (removed_edge_step_id > 0)
			repair << " using STEP edge #" << removed_edge_step_id;
		    wrapper->RecordRepair(repair_id, repair_type, "edge_list",
			repair.str());
		    if (source_edge_mismatch > LocalUnits::tolerance)
			wrapper->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Warning, repair_id,
			    repair_type, "edge_list",
			    "reciprocal pcurves cancelled exactly but both missed "
			    "their private 3-D STEP edge; removed only the "
			    "zero-area bound");
		    if (removed_source_slit)
			wrapper->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Warning, repair_id,
			    repair_type, "edge_list",
			    "OpenNURBS cannot represent a manifold inner slit; "
			    "removed only the source-authored zero-width topology");
		}
		if (topology_changed) *topology_changed = true;
		refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
		    entity_id, &entity_type);
		classify_exact_polyline_seams(brep);
		removed_slit = true;
		break;
	    }
	}
	/* Distinct reciprocal source edges are not provably redundant until the
	 * neighboring zero-width excursions above have been removed.  At that
	 * point each edge must have only this face use left. */
	bool removed_distinct_edge_face = true;
	while (removed_distinct_edge_face) {
	    if (brlcad::PullbackWorkCancelled())
		return keyhole_splits;
	    int removed_face_step_id = 0;
	    int first_edge_step_id = 0;
	    int second_edge_step_id = 0;
	    removed_distinct_edge_face =
		remove_one_exact_distinct_edge_zero_area_face(brep, wrapper,
		    entity_id, entity_type, &removed_face_step_id,
		    &first_edge_step_id, &second_edge_step_id);
	    if (!removed_distinct_edge_face)
		break;
	    if (wrapper) {
		std::ostringstream repair;
		repair << "removed an exact zero-area open-surface face bounded "
		    << "by reciprocal distinct STEP edges #" << first_edge_step_id
		    << " and #" << second_edge_step_id;
		wrapper->RecordRepair(removed_face_step_id > 0 ?
		    removed_face_step_id : entity_id,
		    removed_face_step_id > 0 ? "EDGE_LOOP" : entity_type,
		    "face_bound", repair.str());
	    }
	    if (topology_changed) *topology_changed = true;
	    refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
		entity_id, &entity_type);
	    classify_exact_polyline_seams(brep);
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
		    if (wrapper && wrapper->Verbose() &&
			    !split_failure.empty())
			std::cerr << entity_type << " #" << entity_id << ": "
			    << split_failure << std::endl;
		    if (topology_changed) *topology_changed = true;
		    refresh_brep_flags_preserving_singular_isos(brep, false, wrapper,
			entity_id, &entity_type);
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

} /* namespace step_brep_detail */
