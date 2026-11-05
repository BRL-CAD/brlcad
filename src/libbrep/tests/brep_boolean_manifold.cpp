/*          B R E P _ B O O L E A N _ M A N I F O L D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file brep_boolean_manifold.cpp
 *
 * Verify conservative split-region decisions from the Manifold shadow
 * classifier.  Planar boxes exercise the direct exact proxy path and
 * nonconvex planar solids exercise the certified CDT fallback.  Curved
 * proxies remain unsupported until their geometric fidelity can be certified.
 */

#include "common.h"

#include <memory>

#include "bu/app.h"
#include "bu/env.h"
#include "bu/log.h"

#include "brep.h"
#include "boolean_manifold.h"


static std::unique_ptr<ON_Brep>
make_box(const ON_3dPoint &minimum, const ON_3dPoint &maximum)
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(minimum.x, minimum.y, minimum.z),
	ON_3dPoint(maximum.x, minimum.y, minimum.z),
	ON_3dPoint(maximum.x, maximum.y, minimum.z),
	ON_3dPoint(minimum.x, maximum.y, minimum.z),
	ON_3dPoint(minimum.x, minimum.y, maximum.z),
	ON_3dPoint(maximum.x, minimum.y, maximum.z),
	ON_3dPoint(maximum.x, maximum.y, maximum.z),
	ON_3dPoint(minimum.x, maximum.y, maximum.z)
    };
    return std::unique_ptr<ON_Brep>(ON_BrepBox(corners));
}


static std::unique_ptr<ON_Brep>
make_box(double minimum, double maximum)
{
    return make_box(ON_3dPoint(minimum, minimum, minimum),
	ON_3dPoint(maximum, maximum, maximum));
}


static std::unique_ptr<ON_Brep>
make_notched_prism(double notch_y)
{
    ON_3dPointArray points;
    points.Append(ON_3dPoint(0.0, 0.0, 0.0));
    points.Append(ON_3dPoint(2.0, 0.0, 0.0));
    points.Append(ON_3dPoint(2.0, 2.0, 0.0));
    points.Append(ON_3dPoint(1.0, notch_y, 0.0));
    points.Append(ON_3dPoint(0.0, 2.0, 0.0));
    points.Append(points[0]);
    const ON_PolylineCurve profile(points);
    std::unique_ptr<ON_Extrusion> extrusion(
	ON_Extrusion::CreateFrom3dCurve(profile, NULL, 1.0, true));
    if (!extrusion) {
	return std::unique_ptr<ON_Brep>();
    }
    return std::unique_ptr<ON_Brep>(extrusion->BrepForm(NULL, true));
}


static bool
uniform_decisions(const std::vector<BooleanManifoldDecision> &decisions,
    enum BooleanManifoldSelection selection, int orientation,
    const char *case_name, const char *operand_name)
{
    if (decisions.empty()) {
	bu_log("%s returned no %s decisions\n", case_name, operand_name);
	return false;
    }
    for (size_t index = 0; index < decisions.size(); ++index) {
	if (decisions[index].selection != selection ||
	    decisions[index].orientation != orientation)
	{
	    bu_log("%s %s region %zu returned selection %d/orientation %d, "
		"expected %d/%d (input area %.17g, output area %.17g)\n",
		case_name, operand_name, index, decisions[index].selection,
		decisions[index].orientation, selection, orientation,
		decisions[index].input_area, decisions[index].output_area);
	    return false;
	}
    }
    return true;
}


static int
check_containment(op_type operation, enum BooleanManifoldSelection first_choice,
    int first_orientation, enum BooleanManifoldSelection second_choice,
    int second_orientation, const char *case_name)
{
    std::unique_ptr<ON_Brep> outer = make_box(-2.0, 2.0);
    std::unique_ptr<ON_Brep> inner = make_box(-0.5, 0.5);
    if (!outer || !inner || !outer->IsValid() || !outer->IsSolid() ||
	!inner->IsValid() || !inner->IsSolid())
    {
	bu_log("%s inputs are invalid\n", case_name);
	return 1;
    }

    BooleanManifoldReport report;
    if (!boolean_manifold_classify(*outer, *inner, operation, report)) {
	bu_log("%s classification failed: %s\n", case_name,
	    report.detail.c_str());
	return 1;
    }
    if (report.input_triangles == 0 || report.output_triangles == 0 ||
	report.operand[0].size() != static_cast<size_t>(outer->m_F.Count()) ||
	report.operand[1].size() != static_cast<size_t>(inner->m_F.Count()))
    {
	bu_log("%s returned incomplete metrics or decisions\n", case_name);
	return 1;
    }
    return !uniform_decisions(report.operand[0], first_choice,
	first_orientation, case_name, "outer") ||
	!uniform_decisions(report.operand[1], second_choice,
	second_orientation, case_name, "inner");
}


static int
check_empty_intersection()
{
    std::unique_ptr<ON_Brep> first = make_box(-2.0, -1.0);
    std::unique_ptr<ON_Brep> second = make_box(1.0, 2.0);
    BooleanManifoldReport report;
    if (!first || !second || !boolean_manifold_classify(*first, *second,
	    BOOLEAN_INTERSECT, report))
    {
	bu_log("empty intersection classification failed: %s\n",
	    report.detail.c_str());
	return 1;
    }
    if (report.output_triangles != 0 ||
	!uniform_decisions(report.operand[0], BOOLEAN_MANIFOLD_DISCARD, 0,
	    "empty intersection", "first") ||
	!uniform_decisions(report.operand[1], BOOLEAN_MANIFOLD_DISCARD, 0,
	    "empty intersection", "second"))
    {
	return 1;
    }
    return 0;
}


static int
check_partial_regions_abstain()
{
    std::unique_ptr<ON_Brep> first = make_box(0.0, 2.0);
    std::unique_ptr<ON_Brep> second = make_box(1.0, 3.0);
    BooleanManifoldReport report;
    if (!first || !second || !boolean_manifold_classify(*first, *second,
	    BOOLEAN_UNION, report))
    {
	bu_log("partial-region classification failed: %s\n",
	    report.detail.c_str());
	return 1;
    }

    size_t abstentions = 0;
    for (int operand = 0; operand < 2; ++operand) {
	for (size_t region = 0; region < report.operand[operand].size(); ++region) {
	    if (report.operand[operand][region].selection ==
		    BOOLEAN_MANIFOLD_ABSTAIN)
	    {
		++abstentions;
	    }
	}
    }
    if (abstentions == 0) {
	bu_log("partial-region union returned no conservative abstentions\n");
	return 1;
    }
    return 0;
}


static int
check_nonconvex_cdt_fallback()
{
    std::unique_ptr<ON_Brep> box = make_box(4.0, 5.0);
    if (!box || !box->IsValid() || !box->IsSolid())
    {
	bu_log("unable to construct nonconvex proxy reference box\n");
	return 1;
    }

    const double notch_positions[] = {0.8, 1.0};
    const char *case_names[] = {"concave planar", "collinear planar"};
    for (size_t test = 0; test < 2; ++test) {
	std::unique_ptr<ON_Brep> operand =
	    make_notched_prism(notch_positions[test]);
	if (!operand || !operand->IsValid() || !operand->IsSolid()) {
	    bu_log("unable to construct valid %s proxy operand\n",
		case_names[test]);
	    return 1;
	}
	const ON__UINT32 operand_crc = operand->DataCRC(0);
	const ON__UINT32 box_crc = box->DataCRC(0);
	BooleanManifoldReport report;
	if (!boolean_manifold_classify(*operand, *box, BOOLEAN_UNION,
		report))
	{
	    bu_log("%s certified CDT fallback failed: %s\n",
		case_names[test], report.detail.c_str());
	    return 1;
	}
	if (operand->DataCRC(0) != operand_crc || box->DataCRC(0) != box_crc) {
	    bu_log("%s proxy construction modified a source B-Rep\n",
		case_names[test]);
	    return 1;
	}
	if (report.cdt_proxy_operands != 1 ||
		!uniform_decisions(report.operand[0], BOOLEAN_MANIFOLD_KEEP, 1,
		    case_names[test], "notched prism") ||
		!uniform_decisions(report.operand[1], BOOLEAN_MANIFOLD_KEEP, 1,
		    case_names[test], "box"))
	{
	    return 1;
	}
    }
    return 0;
}


static int
check_curved_cdt_abstention()
{
    std::unique_ptr<ON_Brep> outer(ON_BrepSphere(
	ON_Sphere(ON_3dPoint::Origin, 2.0)));
    std::unique_ptr<ON_Brep> inner = make_box(-0.05, 0.05);
    if (!outer || !inner || !outer->IsValid() || !outer->IsSolid() ||
	!inner->IsValid() || !inner->IsSolid())
    {
	bu_log("unable to construct curved CDT proxy operands\n");
	return 1;
    }

    BooleanManifoldReport report;
    if (boolean_manifold_classify(*outer, *inner, BOOLEAN_UNION, report) ||
	    report.detail.find("non-planar surface") == std::string::npos ||
	    !report.operand[0].empty() || !report.operand[1].empty() ||
	    report.input_triangles != 0 || report.output_triangles != 0)
    {
	bu_log("curved proxy did not abstain conservatively: %s\n",
	    report.detail.c_str());
	return 1;
    }
    return 0;
}


static int
check_shadow_integration(bool coplanar)
{
    std::unique_ptr<ON_Brep> first = make_box(ON_3dPoint(0.0, 0.0, 0.0),
	ON_3dPoint(2.0, 2.0, 2.0));
    std::unique_ptr<ON_Brep> second = coplanar ?
	make_box(ON_3dPoint(1.0, 0.0, 0.0),
	    ON_3dPoint(3.0, 2.0, 2.0)) :
	make_box(ON_3dPoint(1.0, 1.0, 1.0),
	    ON_3dPoint(3.0, 3.0, 3.0));
    if (!first || !second || !first->IsValid() || !first->IsSolid() ||
	!second->IsValid() || !second->IsSolid())
    {
	bu_log("unable to construct %s shadow integration inputs\n",
	    coplanar ? "coplanar" : "transverse");
	return 1;
    }

    const ON__UINT32 first_crc = first->DataCRC(0);
    const ON__UINT32 second_crc = second->DataCRC(0);
    bu_setenv("LIBBREP_BOOLEAN_MANIFOLD_SHADOW", "1", 1);
    boolean_manifold_shadow_metrics_reset();
    ON_Brep result;
    const int status = ON_Boolean(&result, first.get(), second.get(),
	BOOLEAN_UNION);
    const BooleanManifoldShadowMetrics metrics =
	boolean_manifold_shadow_metrics();
    bu_setenv("LIBBREP_BOOLEAN_MANIFOLD_SHADOW", "0", 1);

    if (status != BREP_BOOLEAN_SUCCESS || !result.IsValid() ||
	!result.IsSolid())
    {
	bu_log("%s shadow integration Boolean failed with status %d\n",
	    coplanar ? "coplanar" : "transverse", status);
	return 1;
    }
    if (first->DataCRC(0) != first_crc || second->DataCRC(0) != second_crc) {
	bu_log("%s shadow integration modified a source B-Rep\n",
	    coplanar ? "coplanar" : "transverse");
	return 1;
    }
    if (!coplanar) {
	if (metrics.attempted_operations != 1 ||
	    metrics.classified_operations != 0 ||
	    metrics.abstained_operations != 1 ||
	    metrics.certified_shared_boundaries == 0 ||
	    metrics.selection_disagreements != 0 ||
	    metrics.orientation_disagreements != 0)
	{
	    bu_log("transverse shadow integration did not preserve its "
		"certified-boundary abstention\n");
	    return 1;
	}
	return 0;
    }
    if (metrics.attempted_operations != 1 ||
	metrics.classified_operations != 1 ||
	metrics.abstained_operations != 0 || metrics.decisions == 0 ||
	metrics.selection_agreements != metrics.decisions ||
	metrics.selection_disagreements != 0 ||
	metrics.orientation_disagreements != 0 ||
	metrics.certified_shared_boundaries == 0)
    {
	bu_log("coplanar shadow integration returned incomplete or contradictory "
	    "metrics (%zu/%zu/%zu operations, %zu decisions, %zu/%zu "
	    "disagreements)\n",
	    metrics.attempted_operations, metrics.classified_operations,
	    metrics.abstained_operations, metrics.decisions,
	    metrics.selection_disagreements,
	    metrics.orientation_disagreements);
	return 1;
    }
    if (metrics.coplanar_operations != 1 ||
	metrics.coplanar_classified_operations != 1 ||
	metrics.coplanar_region_abstentions == 0)
    {
	bu_log("coplanar shadow integration did not exclude ambiguous "
	    "coplanar regions\n");
	return 1;
    }
    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    int failures = 0;
    failures += check_containment(BOOLEAN_UNION, BOOLEAN_MANIFOLD_KEEP, 1,
	BOOLEAN_MANIFOLD_DISCARD, 0, "contained union");
    failures += check_containment(BOOLEAN_INTERSECT,
	BOOLEAN_MANIFOLD_DISCARD, 0, BOOLEAN_MANIFOLD_KEEP, 1,
	"contained intersection");
    failures += check_containment(BOOLEAN_DIFF, BOOLEAN_MANIFOLD_KEEP, 1,
	BOOLEAN_MANIFOLD_KEEP, -1, "contained difference");
    failures += check_containment(BOOLEAN_XOR, BOOLEAN_MANIFOLD_KEEP, 1,
	BOOLEAN_MANIFOLD_KEEP, -1, "contained exclusive-or");
    failures += check_empty_intersection();
    failures += check_partial_regions_abstain();
    failures += check_nonconvex_cdt_fallback();
    failures += check_curved_cdt_abstention();
    failures += check_shadow_integration(false);
    failures += check_shadow_integration(true);
    return failures;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
