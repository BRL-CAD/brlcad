/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

#include "brep.h"
#include "brep/assembly.h"
#include "bu/app.h"
#include "bu/log.h"
#include "rt/primitives/brep.h"
#include "wdb.h"

#include "iges_orientation.h"
#include "iges_runtime.h"
#include "iges_writer.h"

namespace {
using namespace brlcad::iges;

void
require(bool condition, const std::string &message)
{
    if (!condition)
	throw std::runtime_error(message);
}

std::unique_ptr<ON_Brep>
box(double minimum = 0.0, double maximum = 1.0)
{
    const ON_3dPoint corners[] = {
	ON_3dPoint(minimum, minimum, minimum), ON_3dPoint(maximum, minimum, minimum),
	ON_3dPoint(maximum, maximum, minimum), ON_3dPoint(minimum, maximum, minimum),
	ON_3dPoint(minimum, minimum, maximum), ON_3dPoint(maximum, minimum, maximum),
	ON_3dPoint(maximum, maximum, maximum), ON_3dPoint(minimum, maximum, maximum)
    };
    std::unique_ptr<ON_Brep> result(ON_BrepBox(corners));
    require(result && result->IsValid() && result->IsSolid(), "could not construct box");
    return result;
}

void
expect_volume(const ON_Brep &brep, double volume)
{
    const auto result = check_brep_orientation(brep);
    require(result.shells.size() == 1, "expected one shell: " + result.detail);
    const auto &shell = result.shells.front();
    require(shell.orientation == (volume > 0.0 ? ShellOrientation::Outward : ShellOrientation::Inward),
	"incorrect orientation (expected volume " + std::to_string(volume) + ", box minimum x " +
	std::to_string(brep.BoundingBox().m_min.x) + "): " + shell.detail);
    require(shell.signed_volume_mm3 &&
	NEAR_EQUAL(*shell.signed_volume_mm3, volume, std::abs(volume) * 1.0e-6),
	"incorrect volume estimate");
}

void
test_boxes()
{
    auto brep = box();
    expect_volume(*brep, 1.0);
    require(!repair_brep_orientation(*brep).shells.front().corrected, "outward box was modified");
    brep->Flip();
    const auto original_crc = brep->DataCRC(0);
    expect_volume(*brep, -1.0);
    require(original_crc == brep->DataCRC(0), "read-only check changed B-Rep data");
    const auto repaired = repair_brep_orientation(*brep);
    require(repaired.shells.front().corrected && repaired.shells.front().faces_to_flip.size() == 6,
	"inward box was not corrected");
    expect_volume(*brep, 1.0);
    brep->FlipFace(brep->m_F[2]);
    const auto inconsistent = check_brep_orientation(*brep);
    require(inconsistent.shells.front().orientation == ShellOrientation::Inconsistent &&
	inconsistent.shells.front().faces_to_flip.size() == 1, "reversed face was not identified");
    require(repair_brep_orientation(*brep).shells.front().corrected, "reversed face was not corrected");
    expect_volume(*brep, 1.0);
    brep->FlipFace(brep->m_F[0]);
    require(repair_brep_orientation(*brep).shells.front().faces_to_flip.size() == 1,
	"reversed seed face caused the wrong correction");
    expect_volume(*brep, 1.0);
    for (double scale : {1.0e-6, 1.0e6}) {
	auto scaled = box(0.0, scale);
	expect_volume(*scaled, scale * scale * scale);
    }
    auto translated = box(1.0e9, 1.0e9 + 1.0);
    expect_volume(*translated, 1.0);
}

void
test_curved()
{
    std::unique_ptr<ON_Brep> sphere(ON_BrepSphere(ON_Sphere(ON_3dPoint::Origin, 1.0)));
    require(sphere && sphere->IsValid(), "could not construct sphere");
    expect_volume(*sphere, 4.0 * ON_PI / 3.0);
    sphere->Flip();
    require(repair_brep_orientation(*sphere).shells.front().corrected, "inward sphere was not corrected");
    expect_volume(*sphere, 4.0 * ON_PI / 3.0);
    std::unique_ptr<ON_Brep> cylinder(ON_BrepCylinder(
	ON_Cylinder(ON_Circle(ON_xy_plane, 1.0), 2.0), true, true));
    require(cylinder && cylinder->IsValid(), "could not construct cylinder");
    expect_volume(*cylinder, 2.0 * ON_PI);
    std::unique_ptr<ON_Brep> torus(ON_BrepTorus(ON_Torus(ON_xy_plane, 3.0, 1.0)));
    require(torus && torus->IsValid(), "could not construct torus");
    expect_volume(*torus, 6.0 * ON_PI * ON_PI);

    // An annular prism exercises actual inner trim loops, not just a
    // multiply connected untrimmed surface such as a torus.
    std::unique_ptr<ON_Brep> pipe(ON_BrepCylinder(
	ON_Cylinder(ON_Circle(ON_xy_plane, 2.0), 2.0), false, false));
    std::unique_ptr<ON_Brep> bore(ON_BrepCylinder(
	ON_Cylinder(ON_Circle(ON_xy_plane, 1.0), 2.0), false, false));
    require(pipe && bore, "could not construct annular prism surfaces");
    for (double height : {0.0, 2.0}) {
	const ON_Plane plane(ON_3dPoint(0.0, 0.0, height), ON_3dVector::ZAxis);
	auto surface = std::make_unique<ON_PlaneSurface>(plane);
	for (int axis = 0; axis < 2; ++axis) {
	    surface->SetDomain(axis, -2.0, 2.0);
	    surface->SetExtents(axis, ON_Interval(-2.0, 2.0));
	}
	const int face = pipe->NewFace(pipe->AddSurface(surface.release())).m_face_index;
	for (double radius : {2.0, 1.0}) {
	    ON_ArcCurve boundary(ON_Circle(plane, radius));
	    ON_SimpleArray<ON_Curve *> curves;
	    curves.Append(&boundary);
	    require(pipe->NewPlanarFaceLoop(face, radius > 1.0 ? ON_BrepLoop::outer : ON_BrepLoop::inner,
		curves, true), "could not add annular trim");
	}
    }
    bore->Flip();
    pipe->Append(*bore);
    brep_assembly_result assembly;
    require(brep_assemble(*pipe, 1.0e-6, &assembly) && pipe->IsSolid(), "could not assemble annular prism");
    expect_volume(*pipe, 6.0 * ON_PI);
}

void
test_components()
{
    auto outer = box(0.0, 3.0);
    auto inner = box(1.0, 2.0);
    inner->Flip();
    outer->Append(*inner);
    auto result = repair_brep_orientation(*outer);
    require(result.shells.size() == 2 && !result.shells[0].corrected && !result.shells[1].corrected &&
	result.shells[1].orientation == ShellOrientation::Inward &&
	result.shells[1].context == ShellContext::PossibleCavity, "legitimate cavity was reversed");
    auto separate = box(4.0, 5.0);
    separate->Flip();
    outer->Append(*separate);
    result = repair_brep_orientation(*outer);
    require(result.shells.size() == 3 && result.shells[2].corrected && !result.shells[1].corrected,
	"disconnected inward solid was not corrected independently of a cavity");
    auto overlap = box(2.5, 4.5);
    overlap->Flip();
    auto first = box(0.0, 3.0);
    first->Append(*overlap);
    result = repair_brep_orientation(*first);
    require(!result.shells[1].corrected && result.shells[1].context == ShellContext::Overlapping,
	"overlapping shells were modified");
}

void
test_uncertain()
{
    ON_Brep empty;
    require(check_brep_orientation(empty).shells.empty(), "invalid empty geometry was classified");
    auto open = box();
    open->DeleteFace(open->m_F[0], true);
    open->Compact();
    require(open->IsValid(), "could not construct open box");
    auto result = repair_brep_orientation(*open);
    require(result.shells.front().orientation == ShellOrientation::Indeterminate &&
	!result.shells.front().corrected && result.shells.front().evaluations == 0,
	"open sheet was classified or modified");
    auto limited = box();
    result = check_brep_orientation(*limited, 1);
    require(result.shells.front().orientation == ShellOrientation::Indeterminate &&
	result.shells.front().evaluations <= 1, "evaluation budget was not honored");
    auto wrong_loops = box();
    for (int loop = 0; loop < wrong_loops->m_L.Count(); ++loop)
	wrong_loops->FlipLoop(wrong_loops->m_L[loop]);
    result = repair_brep_orientation(*wrong_loops);
    require(result.shells.empty() || (result.shells.front().orientation == ShellOrientation::Indeterminate &&
	!result.shells.front().corrected), "invalid parameter-loop senses caused an incorrect face repair");
    auto flat = box();
    ON_Xform flatten;
    flatten.DiagonalTransformation(1.0);
    flatten[2][2] = 1.0e-13;
    require(flat->Transform(flatten), "could not construct thin shell");
    result = repair_brep_orientation(*flat);
    require(result.shells.empty() || (result.shells.front().orientation == ShellOrientation::Indeterminate &&
	!result.shells.front().corrected), "near-zero volume shell was modified");
}

void
test_nonmanifold()
{
    auto brep = box();
    const int original_loop = brep->m_F[0].m_li[0];
    const int added_face = brep->NewFace(brep->m_F[0].m_si).m_face_index;
    const int added_loop = brep->NewLoop(ON_BrepLoop::outer, brep->m_F[added_face]).m_loop_index;
    for (int t = 0; t < brep->m_L[original_loop].TrimCount(); ++t) {
	const int original = brep->m_L[original_loop].m_ti[t];
	const int edge = brep->m_T[original].m_ei;
	const int curve = brep->m_T[original].m_c2i;
	const bool reverse = brep->m_T[original].m_bRev3d;
	const int added = brep->NewTrim(brep->m_E[edge], reverse, brep->m_L[added_loop], curve).m_trim_index;
	brep->m_T[added].m_iso = brep->m_T[original].m_iso;
	for (int axis = 0; axis < 2; ++axis)
	    brep->m_T[added].m_tolerance[axis] = brep->m_T[original].m_tolerance[axis];
    }
    brep->SetTrimTypeFlags();
    brep->SetTrimBoundingBoxes(false);
    require(brep->IsValid(), "could not construct valid non-manifold topology");
    const auto result = repair_brep_orientation(*brep);
    require(result.shells.size() == 1 && result.shells.front().orientation == ShellOrientation::Indeterminate &&
	result.shells.front().evaluations == 0 && !result.shells.front().corrected,
	"non-manifold geometry was classified or modified");
}

void
write_fixtures(const std::string &database_path, const std::string &iges_path)
{
    std::unique_ptr<struct rt_wdb, decltype(&wdb_close)> database(wdb_fopen(database_path.c_str()), wdb_close);
    require(database && mk_id(database.get(), "Orientation regression") == 0, "cannot create test database");
    auto inward = box();
    inward->Flip();
    require(mk_brep(database.get(), "inward", inward.get()) == 0, "cannot write inward box");
    auto outward = box(2.0, 3.0);
    require(mk_brep(database.get(), "outward", outward.get()) == 0, "cannot write outward box");
    // Even a geometrically closed surviving shell must not override the
    // explicit marker that an authored manifold lost other source faces.
    require(mk_brep(database.get(), "zz-incomplete", inward.get()) == 0 &&
	db5_update_attribute("zz-incomplete", RT_BREP_INVALID_SOLID_ATTRIBUTE, "1", database->dbip) == 0,
	"cannot write preserved incomplete solid");
    File file(std::fopen(iges_path.c_str(), "wb"));
    require(bool(file), "cannot create IGES fixture");
    ExportOptions options;
    Writer writer(options, {"inward"});
    Internal internal;
    const auto *entry = db_lookup(database->dbip, "inward", LOOKUP_QUIET);
    require(entry && rt_db_get_internal(&internal.value, entry, database->dbip, nullptr) >= 0,
	"cannot read inward box");
    require(bool(export_brep(writer, internal.value, "inward")), "cannot export inward box");
    writer.finish(file.get(), database_path, iges_path);
    close_file(file);
}
} // namespace

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    ON::Begin();
    try {
	test_boxes();
	test_curved();
	test_components();
	test_uncertain();
	test_nonmanifold();
	if (argc == 3)
	    write_fixtures(argv[1], argv[2]);
	else
	    require(argc == 1, "expected optional database and IGES fixture paths");
	bu_log("IGES orientation tests passed\n");
	return 0;
    } catch (const std::exception &error) {
	bu_log("IGES orientation test: %s\n", error.what());
	return 1;
    }
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
