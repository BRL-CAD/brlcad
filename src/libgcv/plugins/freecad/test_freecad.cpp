/*                 T E S T _ F R E E C A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BinTools.hxx>
#include <Geom2dConvert.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <GeomAPI.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

#include "bu/app.h"
#include "raytrace.h"
#include "rt/geom.h"

static const char *document = R"xml(
<Document SchemaVersion="4">
  <Properties><Property name="Label"><String value="Native assembly"/></Property></Properties>
  <Objects Count="13">
    <Object name="Assembly" type="App::Part"/>
    <Object name="Definition" type="Part::Feature"/>
    <Object name="First" type="App::Link"/>
    <Object name="Second" type="App::Link"/>
    <Object name="Cylinder" type="Part::Feature"/>
    <Object name="TorusPatch" type="Part::Feature"/>
    <Object name="PeriodicEdges" type="Part::Feature"/>
    <Object name="Hidden" type="Part::Feature"/>
    <Object name="Projection" type="App::LinkGroup"/>
    <Object name="ProjectedLink" type="App::Link"/>
    <Object name="OffsetDefinition" type="Part::Feature"/>
    <Object name="OffsetLink" type="App::Link"/>
    <Object name="PlacedOffsetLink" type="App::Link"/>
  </Objects>
  <ObjectData Count="13">
    <Object name="Assembly"><Properties>
      <Property name="Placement"><PropertyPlacement Px="10" Py="20"
        Q2="0.7071067811865475" Q3="0.7071067811865476"/></Property>
      <Property name="Group"><LinkList count="10">
        <Link value="First"/><Link value="Second"/><Link value="Cylinder"/><Link value="Hidden"/>
        <Link value="TorusPatch"/><Link value="PeriodicEdges"/><Link value="Projection"/>
        <Link value="OffsetDefinition"/><Link value="OffsetLink"/><Link value="PlacedOffsetLink"/>
      </LinkList></Property>
    </Properties></Object>
    <Object name="Definition"><Properties>
      <Property name="Label"><String value="Box &amp; definition"/></Property>
      <Property name="Catalog"><String value="B-102"/></Property>
      <Property name="Shape"><Part file="box.brp"/></Property>
      <Property name="Placement"><PropertyPlacement Px="100"/></Property>
    </Properties></Object>
    <Object name="First"><Properties>
      <Property name="LinkedObject"><XLink file="" name="Definition"/></Property>
      <Property name="LinkPlacement"><PropertyPlacement Px="5"/></Property>
      <Property name="LinkTransform"><Bool value="false"/></Property>
    </Properties></Object>
    <Object name="Second"><Properties>
      <Property name="LinkedObject"><Link value="Definition"/></Property>
      <Property name="LinkPlacement"><PropertyPlacement Px="15"/></Property>
      <Property name="LinkTransform"><Bool value="true"/></Property>
      <Property name="ScaleVector"><PropertyVector valueX="2" valueY="2" valueZ="2"/></Property>
    </Properties></Object>
    <Object name="Cylinder"><Properties>
      <Property name="Shape"><Part file="cylinder.bin"/></Property>
    </Properties></Object>
    <Object name="TorusPatch"><Properties>
      <Property name="Shape"><Part file="torus.brp"/></Property>
    </Properties></Object>
    <Object name="PeriodicEdges"><Properties>
      <Property name="Shape"><Part file="periodic.brp"/></Property>
    </Properties></Object>
    <Object name="Hidden"><Properties>
      <Property name="Visibility"><Bool value="false"/></Property>
      <Property name="Shape"><Part file="box.brp"/></Property>
    </Properties></Object>
    <Object name="Projection"><Properties>
      <Property name="Placement"><PropertyPlacement Px="30"
        Q2="0.7071067811865475" Q3="0.7071067811865476"/></Property>
      <Property name="ElementList"><LinkList count="3">
        <Link value="Hidden"/><Link value=""/><Link value="ProjectedLink"/>
      </LinkList></Property>
      <Property name="VisibilityList"><BoolList value="100"/></Property>
    </Properties></Object>
    <Object name="ProjectedLink"><Properties>
      <Property name="Visibility"><Bool value="false"/></Property>
      <Property name="LinkedObject"><Link value="Definition"/></Property>
      <Property name="LinkPlacement"><PropertyPlacement Py="4"/></Property>
    </Properties></Object>
    <Object name="OffsetDefinition"><Properties>
      <Property name="Shape"><Part file="box.brp"/></Property>
      <Property name="Placement"><PropertyPlacement Px="20"/></Property>
    </Properties></Object>
    <Object name="OffsetLink"><Properties>
      <Property name="LinkedObject"><Link value="OffsetDefinition"/></Property>
      <Property name="LinkTransform"><Bool value="false"/></Property>
    </Properties></Object>
    <Object name="PlacedOffsetLink"><Properties>
      <Property name="LinkedObject"><Link value="OffsetDefinition"/></Property>
      <Property name="LinkTransform"><Bool value="true"/></Property>
    </Properties></Object>
  </ObjectData>
</Document>
)xml";

static const char *gui_document = R"xml(
<Document SchemaVersion="1" xmlns="urn:freecad:fixture"><ViewProviderData>
  <ViewProvider name="Definition"><Properties>
    <Property name="Visibility"><Bool value="false"/></Property>
    <Property name="ShapeColor"><Color value="287453952"/></Property>
  </Properties></ViewProvider>
  <ViewProvider name="First"><Properties>
    <Property name="ShapeMaterial"><PropertyMaterial diffuseColor="2857697535"/></Property>
    <Property name="OverrideMaterial"><Bool value="true"/></Property>
  </Properties></ViewProvider>
  <ViewProvider name="Second"><Properties>
    <Property name="OverrideMaterial"><Bool value="false"/></Property>
    <Property name="ShapeMaterial"><PropertyMaterial diffuseColor="580496127"/></Property>
  </Properties></ViewProvider>
</ViewProviderData></Document>
)xml";

static const char *array_document = R"xml(
<Document SchemaVersion="4">
  <Objects Count="6">
    <Object name="Definition" type="Part::Feature"/>
    <Object name="Compact" type="App::Link"/>
    <Object name="Expanded" type="App::Link"/>
    <Object name="VisibleElement" type="App::LinkElement"/>
    <Object name="SuppressedElement" type="App::LinkElement"/>
    <Object name="MaskedElement" type="App::LinkElement"/>
  </Objects>
  <ObjectData Count="6">
    <Object name="Definition"><Properties>
      <Property name="Visibility"><Bool value="false"/></Property>
      <Property name="Placement"><PropertyPlacement Px="100"/></Property>
      <Property name="Shape"><Part file="box.brp"/></Property>
    </Properties></Object>
    <Object name="Compact"><Properties>
      <Property name="LinkedObject"><Link value="Definition"/></Property>
      <Property name="ElementCount"><Integer value="3"/></Property>
      <Property name="ShowElement"><Bool value="false"/></Property>
      <Property name="LinkPlacement"><PropertyPlacement Px="5"/></Property>
      <Property name="LinkTransform"><Bool value="true"/></Property>
      <Property name="ScaleVector"><PropertyVector valueX="2" valueY="1" valueZ="1"/></Property>
      <Property name="PlacementList"><PlacementList file="placements.bin"/></Property>
      <Property name="ScaleList"><VectorList file="scales.bin"/></Property>
      <Property name="VisibilityList"><BoolList value="101"/></Property>
    </Properties></Object>
    <Object name="Expanded"><Properties>
      <Property name="LinkedObject"><Link value="Definition"/></Property>
      <Property name="ElementCount"><Integer value="3"/></Property>
      <Property name="ShowElement"><Bool value="true"/></Property>
      <Property name="LinkTransform"><Bool value="true"/></Property>
      <Property name="Scale"><Float value="3"/></Property>
      <Property name="ElementList"><LinkList count="3">
        <Link value="VisibleElement"/><Link value="SuppressedElement"/><Link value="MaskedElement"/>
      </LinkList></Property>
      <Property name="VisibilityList"><BoolList value="011"/></Property>
    </Properties></Object>
    <Object name="VisibleElement"><Properties>
      <Property name="LinkedObject"><Link value="Definition"/></Property>
      <Property name="LinkPlacement"><PropertyPlacement Px="6"/></Property>
      <Property name="ScaleVector"><PropertyVector valueX="-1" valueY="1" valueZ="1"/></Property>
      <Property name="Visibility"><Bool value="false"/></Property>
    </Properties></Object>
    <Object name="SuppressedElement"><Properties>
      <Property name="LinkedObject"><Link value="Definition"/></Property>
      <Property name="Suppressed"><Bool value="true"/></Property>
    </Properties></Object>
    <Object name="MaskedElement"><Properties>
      <Property name="LinkedObject"><Link value="Definition"/></Property>
    </Properties></Object>
  </ObjectData>
</Document>
)xml";

static void
write_entry(const char *name, const std::string &data)
{
    std::ofstream file(name, std::ios::binary);
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    file.close();
    if (!file)
	throw std::runtime_error(std::string("could not write fixture member ") + name);
}

static constexpr double fixture_tolerance = 1.0e-7;
static constexpr double torus_major_radius = 8.0;
static constexpr double torus_minor_radius = 2.0;

static TopoDS_Face
make_torus_patch()
{
    const Handle(Geom_Surface) torus =
	new Geom_ToroidalSurface(gp_Ax3(), torus_major_radius, torus_minor_radius);
    // The minor arc crosses the canonical cut in its parameter interval.
    constexpr double v_start = 5.0;
    constexpr double v_end = 7.0;
    const Handle(Geom_Surface) trimmed =
	new Geom_RectangularTrimmedSurface(torus, 0.0, ON_PI, v_start, v_end);
    return BRepBuilderAPI_MakeFace(trimmed, fixture_tolerance).Face();
}

static TopoDS_Face
make_periodic_edge_face()
{
    const Handle(Geom2d_Curve) circle = new Geom2d_Circle(gp_Ax2d(), 4.0);
    const auto periodic = Geom2dConvert::CurveToBSplineCurve(circle);
    periodic->SetPeriodic();
    const gp_Pln plane{gp_Ax3()};
    const auto spatial_curve = GeomAPI::To3d(periodic, plane);
    const double first = periodic->FirstParameter();
    const double last = periodic->LastParameter();
    const double middle = (first + last) / 2.0;
    const auto first_edge = BRepBuilderAPI_MakeEdge(spatial_curve, first, middle).Edge();
    const auto second_edge = BRepBuilderAPI_MakeEdge(spatial_curve, middle, last).Edge();
    const auto wire = BRepBuilderAPI_MakeWire(first_edge, second_edge).Wire();
    auto face = BRepBuilderAPI_MakeFace(plane, wire).Face();
    BRep_Builder builder;
    for (TopExp_Explorer use(face, TopAbs_EDGE); use.More(); use.Next()) {
	const auto edge = TopoDS::Edge(use.Current());
	double start, end;
	BRep_Tool::Range(edge, start, end);
	builder.UpdateEdge(edge, periodic, face, fixture_tolerance);
	builder.Range(edge, face, start, end);
    }
    return face;
}

static void
write_box()
{
    TopoDS_Shape box = BRepPrimAPI_MakeBox(2, 3, 4).Shape();
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(100, 0, 0));
    box.Move(TopLoc_Location(translation));
    std::ostringstream box_stream;
    BRepTools::Write(box, box_stream);
    write_entry("box.brp", box_stream.str());
}

static void
generate_fixture()
{
    write_box();
    const TopoDS_Shape cylinder = BRepPrimAPI_MakeCylinder(5, 9).Shape();
    std::ostringstream cylinder_stream;
    BinTools::Write(cylinder, cylinder_stream);
    std::ostringstream torus_stream, periodic_stream;
    BRepTools::Write(make_torus_patch(), torus_stream);
    BRepTools::Write(make_periodic_edge_face(), periodic_stream);
    write_entry("GuiDocument.xml", gui_document);
    write_entry("cylinder.bin", cylinder_stream.str());
    write_entry("torus.brp", torus_stream.str());
    write_entry("periodic.brp", periodic_stream.str());
    write_entry("Document.xml", document);
}

template <typename UInt, typename Value>
static void
append_binary(std::string &out, Value value)
{
    static_assert(sizeof(UInt) == sizeof(Value), "binary fixture value width");
    UInt bits;
    std::memcpy(&bits, &value, sizeof(bits));
    for (size_t byte = 0; byte < sizeof(bits); ++byte)
	out.push_back(static_cast<char>((bits >> (byte * 8)) & 0xff));
}

static void
generate_arrays()
{
    write_box();
    write_entry("Document.xml", array_document);
    constexpr double quarter_turn = 0.7071067811865476;
    const std::array<double, 21> placement_values = {
	10, 0, 0, 0, 0, 0, 1,
	700, 0, 0, 0, 0, 0, 1,
	0, 20, 0, 0, 0, quarter_turn, quarter_turn
    };
    std::string placements;
    append_binary<uint32_t>(placements, uint32_t(3));
    for (double value : placement_values)
	append_binary<uint64_t>(placements, value);
    write_entry("placements.bin", placements);

    const std::array<float, 9> scale_values = {1, 1, 1, 1, 1, 1, 3, 4, 1};
    std::string scales;
    append_binary<uint32_t>(scales, uint32_t(3));
    for (float value : scale_values)
	append_binary<uint32_t>(scales, value);
    write_entry("scales.bin", scales);
}

class Internal {
public:
    Internal(struct db_i *db, const char *name, int type)
    {
	RT_DB_INTERNAL_INIT(&object);
	const struct directory *entry = db_lookup(db, name, LOOKUP_QUIET);
	if (!entry || rt_db_get_internal(&object, entry, db, nullptr) < 0)
	    throw std::runtime_error(std::string("missing fixture object ") + name);
	if (object.idb_type != type) {
	    rt_db_free_internal(&object);
	    throw std::runtime_error(std::string("unexpected fixture object type ") + name);
	}
    }

    ~Internal() { rt_db_free_internal(&object); }

    struct rt_db_internal object;
};

static void
check_solid(struct db_i *db, const char *name, int faces)
{
    const Internal item(db, name, ID_BREP);
    auto *brep = static_cast<struct rt_brep_internal *>(item.object.idb_ptr)->brep;
    if (!brep->IsValid() || !brep->IsSolid() || brep->m_F.Count() != faces)
	throw std::runtime_error(std::string("incorrect BRep in fixture object ") + name);
}

static const union tree *
find_member(const union tree *node, const char *name)
{
    if (!node)
	return nullptr;
    if (node->tr_op == OP_DB_LEAF)
	return BU_STR_EQUAL(node->tr_l.tl_name, name) ? node : nullptr;
    if (node->tr_op != OP_UNION)
	return nullptr;
    const union tree *left = find_member(node->tr_b.tb_left, name);
    return left ? left : find_member(node->tr_b.tb_right, name);
}

static const struct rt_comb_internal *
combination(const Internal &value)
{
    return static_cast<const struct rt_comb_internal *>(value.object.idb_ptr);
}

static void
check_member(const Internal &parent, const char *name, const point_t input,
	const point_t expected)
{
    const union tree *leaf = find_member(combination(parent)->tree, name);
    if (!leaf)
	throw std::runtime_error(std::string("missing fixture member ") + name);
    point_t actual;
    if (leaf->tr_l.tl_mat)
	MAT4X3PNT(actual, leaf->tr_l.tl_mat, input);
    else
	VMOVE(actual, input);
    constexpr double position_tolerance = 1.0e-8;
    if (!VNEAR_EQUAL(actual, expected, position_tolerance))
	throw std::runtime_error(std::string("wrong fixture placement for ") + name);
}

static void
check_attribute(const Internal &object, const char *key, const char *expected)
{
    const char *value = bu_avs_get(&object.object.idb_avs, key);
    if (!value || !BU_STR_EQUAL(value, expected))
	throw std::runtime_error(std::string("wrong fixture metadata for ") + key);
}

static void
check_fixture(const char *path)
{
    std::unique_ptr<struct db_i, decltype(&db_close)> database(
	db_open(path, DB_OPEN_READONLY), db_close);
    struct db_i *db = database.get();
    if (!db || db_dirbuild(db) < 0)
	throw std::runtime_error("could not read converted fixture");

    check_solid(db, "Definition.s", 6);
    check_solid(db, "Cylinder.s", 3);
    const Internal torus(db, "TorusPatch.s", ID_BREP);
    const auto *torus_brep = static_cast<struct rt_brep_internal *>(torus.object.idb_ptr)->brep;
    const ON_Surface *torus_surface = torus_brep->m_F[0].SurfaceOf();
    constexpr double u = 0.4;
    constexpr double v = 5.7;
    const double radial = torus_major_radius + torus_minor_radius * cos(v);
    const ON_3dPoint expected_torus(radial * cos(u), radial * sin(u), torus_minor_radius * sin(v));
    if (!torus_brep->IsValid() || torus_brep->m_F.Count() != 1 ||
	    !ON_RevSurface::Cast(torus_surface) ||
	    torus_surface->PointAt(u, v).DistanceTo(expected_torus) > fixture_tolerance)
	throw std::runtime_error("trimmed torus patch lost its analytic parameterization");

    const Internal periodic(db, "PeriodicEdges.s", ID_BREP);
    const auto *periodic_brep = static_cast<struct rt_brep_internal *>(periodic.object.idb_ptr)->brep;
    if (!periodic_brep->IsValid() || periodic_brep->m_T.Count() != 2)
	throw std::runtime_error("periodic pcurve fixture lost a trim");
    for (int i = 0; i < periodic_brep->m_T.Count(); ++i) {
	const auto &trim = periodic_brep->m_T[i];
	if (trim.IsClosed() || trim.m_vi[0] == trim.m_vi[1])
	    throw std::runtime_error("a periodic pcurve subinterval became closed");
    }
    const Internal box(db, "Definition.s", ID_BREP);
    const auto *box_brep = static_cast<struct rt_brep_internal *>(box.object.idb_ptr)->brep;
    const ON_BoundingBox box_bounds = box_brep->BoundingBox();
    if (box_bounds.m_min.DistanceTo(ON_3dPoint(0, 0, 0)) > ON_ZERO_TOLERANCE ||
	    box_bounds.m_max.DistanceTo(ON_3dPoint(2, 3, 4)) > ON_ZERO_TOLERANCE)
	throw std::runtime_error("FreeCAD box definition is not in local coordinates");
    const Internal root(db, "hierarchy", ID_COMBINATION);
    const Internal assembly(db, "Assembly", ID_COMBINATION);
    const Internal first(db, "First", ID_COMBINATION);
    const Internal second(db, "Second", ID_COMBINATION);
    const Internal definition(db, "Definition", ID_COMBINATION);
    const Internal projection(db, "Projection", ID_COMBINATION);
    const Internal projected_link(db, "ProjectedLink", ID_COMBINATION);
    const Internal offset_definition(db, "OffsetDefinition", ID_COMBINATION);
    const Internal offset_link(db, "OffsetLink", ID_COMBINATION);
    const Internal placed_offset_link(db, "PlacedOffsetLink", ID_COMBINATION);
    const point_t origin = {0, 0, 0};
    const point_t unit_x = {1, 0, 0};
    const point_t in_assembly = {10, 21, 0};
    const point_t first_offset = {5, 0, 0};
    const point_t second_offset = {15, 0, 0};
    const point_t scaled_definition = {202, 0, 0};
    const point_t in_projection = {30, 1, 0};
    const point_t projected_offset = {0, 4, 0};
    const point_t object_placement = {20, 0, 0};
    const point_t shape_offset = {80, 0, 0};
    check_member(root, "Assembly", unit_x, in_assembly);
    check_member(assembly, "First", origin, first_offset);
    check_member(assembly, "Second", origin, second_offset);
    check_member(assembly, "Cylinder", origin, origin);
    check_member(assembly, "Projection", unit_x, in_projection);
    check_member(projection, "ProjectedLink", origin, projected_offset);
    check_member(projected_link, "Definition", origin, origin);
    check_member(first, "Definition", origin, origin);
    check_member(second, "Definition", unit_x, scaled_definition);
    check_member(definition, "Definition.s", origin, origin);
    check_member(assembly, "OffsetDefinition", origin, object_placement);
    check_member(offset_definition, "Definition.s", origin, shape_offset);
    check_member(offset_link, "OffsetDefinition", origin, origin);
    check_member(placed_offset_link, "OffsetDefinition", origin, object_placement);
    if (find_member(combination(root)->tree, "Definition") ||
	    db_lookup(db, "Hidden", LOOKUP_QUIET) ||
	    db_lookup(db, "Definition.s_2", LOOKUP_QUIET))
	throw std::runtime_error("incorrect FreeCAD visibility or link sharing");
    check_attribute(root, "freecad:Label", "Native assembly");
    check_attribute(definition, "freecad:Label", "Box & definition");
    check_attribute(definition, "freecad:Catalog", "B-102");
    check_attribute(first, "freecad:linked_object", "Definition");
    const auto *color = combination(definition);
    if (!color->rgb_valid || color->rgb[0] != 17 || color->rgb[1] != 34 || color->rgb[2] != 51)
	throw std::runtime_error("wrong FreeCAD shape color");
    const auto *override = combination(first);
    if (!override->rgb_valid || !override->inherit ||
	    override->rgb[0] != 170 || override->rgb[1] != 85 || override->rgb[2] != 0)
	throw std::runtime_error("FreeCAD link material did not override the definition color");
    if (combination(second)->rgb_valid)
	throw std::runtime_error("inactive FreeCAD link material changed the inherited color");
}

static void
check_arrays(const char *path)
{
    std::unique_ptr<struct db_i, decltype(&db_close)> database(
	db_open(path, DB_OPEN_READONLY), db_close);
    struct db_i *db = database.get();
    if (!db || db_dirbuild(db) < 0)
	throw std::runtime_error("could not read converted array fixture");

    const Internal root(db, "arrays", ID_COMBINATION);
    const Internal compact(db, "Compact", ID_COMBINATION);
    const Internal expanded(db, "Expanded", ID_COMBINATION);
    const Internal visible(db, "VisibleElement", ID_COMBINATION);
    const point_t origin = {0, 0, 0};
    const point_t array_placement = {5, 0, 0};
    const point_t visible_placement = {18, 0, 0};
    const point_t unit_x = {1, 0, 0};
    const point_t reflected = {-1, 0, 0};
    check_member(root, "Compact", origin, array_placement);
    check_member(expanded, "VisibleElement", origin, visible_placement);
    check_member(visible, "Definition", unit_x, reflected);
    if (db_lookup(db, "SuppressedElement", LOOKUP_QUIET) ||
	    db_lookup(db, "MaskedElement", LOOKUP_QUIET) ||
	    find_member(combination(root)->tree, "VisibleElement"))
	throw std::runtime_error("incorrect array element visibility");

    std::vector<ON_3dPoint> positions;
    std::vector<const union tree *> pending = {combination(compact)->tree};
    while (!pending.empty()) {
	const auto *node = pending.back();
	pending.pop_back();
	if (node && node->tr_op == OP_UNION) {
	    pending.push_back(node->tr_b.tb_left);
	    pending.push_back(node->tr_b.tb_right);
	    continue;
	}
	if (!node || node->tr_op != OP_DB_LEAF || !node->tr_l.tl_mat ||
		!BU_STR_EQUAL(node->tr_l.tl_name, "Definition"))
	    throw std::runtime_error("array element did not share the definition");
	point_t point;
	MAT4X3PNT(point, node->tr_l.tl_mat, unit_x);
	positions.emplace_back(point);
    }
    const auto has_point = [&](const ON_3dPoint &expected) {
	return std::any_of(positions.begin(), positions.end(), [&](const auto &point) {
	    return point.DistanceTo(expected) < fixture_tolerance;
	});
    };
    if (positions.size() != 2 || !has_point(ON_3dPoint(222, 0, 0)) ||
	    !has_point(ON_3dPoint(0, 323, 0)))
	throw std::runtime_error("incorrect compact array placements");
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc < 2)
	return 1;
    try {
	const std::string command = argv[1];
	if (command == "generate" && argc == 2)
	    generate_fixture();
	else if (command == "check" && argc == 3)
	    check_fixture(argv[2]);
	else if (command == "generate-arrays" && argc == 2)
	    generate_arrays();
	else if (command == "check-arrays" && argc == 3)
	    check_arrays(argv[2]);
	else
	    return 1;
	return 0;
    } catch (const std::exception &error) {
	bu_log("%s\n", error.what());
	return 1;
    }
}

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
