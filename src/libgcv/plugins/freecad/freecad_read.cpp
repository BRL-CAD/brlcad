/*                 F R E E C A D _ R E A D . C P P
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
#include <chrono>
#include <istream>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BinTools.hxx>
#include <NCollection_IndexedMap.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <gp_Trsf.hxx>

#include "opennurbs.h"

#include "bn/mat.h"
#include "bu/path.h"
#include "gcv/api.h"
#include "rt/geom.h"
#include "wdb.h"

#include "fcstd_document.h"
#include "occt_brep.h"

namespace freecad {

static const char *
occt_failure_message(const Standard_Failure &failure)
{
#if OCC_VERSION_MAJOR >= 8
    return failure.what();
#else
    return failure.GetMessageString();
#endif
}

class ShapeStream : public std::streambuf {
public:
    explicit ShapeStream(Archive &archive) : source(archive) {}

    void finish()
    {
	std::ptrdiff_t count;
	do {
	    count = source.read(buffer.data(), buffer.size());
	} while (count > 0);
	if (count < 0)
	    throw std::runtime_error("could not finish reading FCStd shape member");
    }

protected:
    int_type underflow() override
    {
	const auto count = source.read(buffer.data(), buffer.size());
	if (count < 0)
	    throw std::runtime_error("could not inflate FCStd shape member");
	if (!count)
	    return traits_type::eof();
	setg(buffer.data(), buffer.data(), buffer.data() + count);
	return traits_type::to_int_type(*gptr());
    }

private:
    static constexpr size_t buffer_size = 64 * 1024;
    Archive &source;
    std::array<char, buffer_size> buffer;
};

class BinaryShapeBuffer : public std::streambuf {
public:
    explicit BinaryShapeBuffer(Archive &archive)
    {
	constexpr size_t block_size = 64 * 1024;
	std::array<char, block_size> block;
	for (;;) {
	    const auto count = archive.read(block.data(), block.size());
	    if (count < 0)
		throw std::runtime_error("could not inflate binary FCStd shape member");
	    if (!count)
		break;
	    storage.insert(storage.end(), block.data(), block.data() + count);
	}
	if (!storage.empty())
	    setg(storage.data(), storage.data(), storage.data() + storage.size());
    }

protected:
    pos_type seekoff(off_type offset, std::ios_base::seekdir direction,
	    std::ios_base::openmode mode) override
    {
	if (!(mode & std::ios_base::in) || storage.empty())
	    return pos_type(-1);
	const off_type size = egptr() - eback();
	const off_type base = direction == std::ios_base::beg ? 0 :
	    (direction == std::ios_base::cur ? gptr() - eback() : size);
	if (offset < -base || offset > size - base)
	    return pos_type(-1);
	const off_type position = base + offset;
	setg(eback(), eback() + position, egptr());
	return pos_type(position);
    }

    pos_type seekpos(pos_type position, std::ios_base::openmode mode) override
    {
	return seekoff(off_type(position), std::ios_base::beg, mode);
    }

private:
    std::vector<char> storage;
};

static bool
extension_is(const char *path, const char *wanted)
{
    const char *extension = strrchr(path, '.');
    return extension && BU_STR_EQUIV(extension + 1, wanted);
}

static std::string
unique_name(struct db_i *db, std::string name)
{
    for (char &c : name)
	if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		    (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_'))
	    c = '_';
    const std::string base = name.empty() ? "freecad" : name;
    if (!db_lookup(db, base.c_str(), LOOKUP_QUIET))
	return base;
    for (size_t suffix = 2; ; ++suffix) {
	name = base + "_" + std::to_string(suffix);
	if (!db_lookup(db, name.c_str(), LOOKUP_QUIET))
	    return name;
    }
}

struct Member {
    std::string name;
    Matrix transform = MAT_INIT_IDN;
};

static void
write_combination(struct rt_wdb *writer, const std::string &name,
	const std::vector<Member> &members, const Properties &properties,
	const unsigned char *color, bool override_color = false)
{
    struct wmember head;
    BU_LIST_INIT(&head.l);
    for (const auto &member : members) {
	Matrix transform = member.transform;
	if (mk_addmember(member.name.c_str(), &head.l, transform.data(), WMOP_UNION))
	    continue;
	mk_freemembers(&head.l);
	throw std::runtime_error("could not add member to " + name);
    }
    if (mk_comb(writer, name.c_str(), &head.l, 0, nullptr, nullptr, color,
	    0, 0, 0, 0, color && override_color, 0, 0))
	throw std::runtime_error("could not write FreeCAD combination " + name);
    struct directory *entry = db_lookup(writer->dbip, name.c_str(), LOOKUP_QUIET);
    if (!entry)
	throw std::runtime_error("could not find FreeCAD combination " + name);

    struct bu_attribute_value_set attrs;
    bu_avs_init(&attrs, properties.size(), "FreeCAD properties");
    for (const auto &property : properties)
	bu_avs_add(&attrs, ("freecad:" + property.first).c_str(), property.second.c_str());
    if (db5_update_attributes(entry, &attrs, writer->dbip))
	throw std::runtime_error("could not write FreeCAD properties for " + name);
}

static bool
has_link(const Object &object)
{
    return !object.linked_object.empty() || object.type == "App::Link" ||
	object.unsupported_link;
}

static std::vector<std::string>
dependencies(const Document &document, const std::string &id, const Object &object)
{
    if (has_link(object)) {
	if (object.unsupported_link) {
	    bu_log("FreeCAD: %s uses an external, subobject, or array link\n", id.c_str());
	    return {};
	}
	if (document.objects.find(object.linked_object) != document.objects.end())
	    return {object.linked_object};
	bu_log("FreeCAD: %s has a missing link target\n", id.c_str());
	return {};
    }
    // A saved Body shape already represents its modeling history.
    if (!object.shape_file.empty() && object.type == "PartDesign::Body")
	return {};
    std::vector<std::string> children;
    for (const auto &child : object.children) {
	const auto found = document.objects.find(child);
	if (found == document.objects.end()) {
	    bu_log("FreeCAD: %s references missing group member %s\n", id.c_str(), child.c_str());
	    continue;
	}
	if (found->second.visible)
	    children.push_back(child);
    }
    return children;
}

struct Node {
    Member occurrence;
    std::vector<std::string> dependencies;
    bool complete = false;
};

static std::vector<std::string>
order_nodes(const Document &document, const std::vector<std::string> &roots,
	std::map<std::string, Node> &nodes)
{
    std::vector<std::string> order;
    for (const auto &root : roots) {
	auto added = nodes.emplace(root, Node());
	if (!added.second)
	    continue;
	added.first->second.dependencies = dependencies(document, root, document.objects.at(root));
	std::vector<std::pair<std::string, size_t>> stack = {{root, 0}};
	while (!stack.empty()) {
	    auto &frame = stack.back();
	    Node &node = nodes.at(frame.first);
	    if (frame.second == node.dependencies.size()) {
		node.complete = true;
		order.push_back(frame.first);
		stack.pop_back();
		continue;
	    }
	    const auto &child = node.dependencies[frame.second++];
	    auto inserted = nodes.emplace(child, Node());
	    if (inserted.second) {
		inserted.first->second.dependencies = dependencies(document, child,
		    document.objects.at(child));
		stack.emplace_back(child, 0);
	    } else if (!inserted.first->second.complete) {
		throw std::runtime_error("cyclic FreeCAD hierarchy at " + child);
	    }
	}
    }
    return order;
}

struct Geometry {
    Matrix transform = MAT_INIT_IDN;
    std::vector<Member> parts;
    size_t invalid_parts = 0;
    size_t nonmanifold_parts = 0;
};

using ImportClock = std::chrono::steady_clock;

struct ImportTimings {
    ImportClock::duration decode = {};
    ImportClock::duration translate = {};
    ImportClock::duration store = {};
};

static double
seconds(ImportClock::duration elapsed)
{
    return std::chrono::duration<double>(elapsed).count();
}

static bool
preserves_source_nonmanifold_edges(const TopoDS_Shape &shape, const ON_Brep &brep)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    std::vector<int> source_uses(static_cast<size_t>(edges.Extent()));
    for (TopExp_Explorer face(shape, TopAbs_FACE); face.More(); face.Next()) {
	for (TopExp_Explorer edge(face.Current(), TopAbs_EDGE); edge.More(); edge.Next()) {
	    if (BRep_Tool::Degenerated(TopoDS::Edge(edge.Current())))
		continue;
	    const int index = edges.FindIndex(edge.Current());
	    if (!index)
		return false;
	    ++source_uses[static_cast<size_t>(index - 1)];
	}
    }
    // A zero use belongs to an OCCT degenerate edge represented by an
    // edgeless singular trim in OpenNURBS.  Preserve only closed inputs.
    std::sort(source_uses.begin(), source_uses.end());
    source_uses.erase(source_uses.begin(), std::upper_bound(source_uses.begin(), source_uses.end(), 0));
    constexpr int manifold_uses = 2;
    if (source_uses.empty() || source_uses.front() < manifold_uses ||
	    source_uses.back() <= manifold_uses || source_uses.size() != static_cast<size_t>(brep.m_E.Count()))
	return false;
    std::vector<int> target_uses;
    target_uses.reserve(source_uses.size());
    for (int i = 0; i < brep.m_E.Count(); ++i)
	target_uses.push_back(brep.m_E[i].m_ti.Count());
    std::sort(target_uses.begin(), target_uses.end());
    return target_uses == source_uses;
}

static void
write_shape(struct rt_wdb *writer, const std::string &name,
	const TopoDS_Shape &shape, double tolerance, Geometry &geometry, ImportTimings &timings)
{
    if (shape.ShapeType() == TopAbs_COMPOUND || shape.ShapeType() == TopAbs_COMPSOLID) {
	for (TopoDS_Iterator child(shape); child.More(); child.Next())
	    write_shape(writer, name, child.Value(), tolerance, geometry, timings);
	return;
    }
    if (!TopExp_Explorer(shape, TopAbs_FACE).More())
	return;

    auto brep = std::make_unique<ON_Brep>();
    const auto started = ImportClock::now();
    bool nonmanifold = false;
    try {
	ON_wString diagnostic;
	ON_TextLog log(diagnostic);
	const bool valid = occt_to_brep(shape, *brep, tolerance, &log) && brep->m_F.Count() > 0;
	const bool solid = shape.ShapeType() != TopAbs_SOLID || (valid && brep->IsSolid());
	nonmanifold = valid && !solid && preserves_source_nonmanifold_edges(shape, *brep);
	timings.translate += ImportClock::now() - started;
	if (!valid || (!solid && !nonmanifold)) {
	    ++geometry.invalid_parts;
	    if (diagnostic.IsEmpty() && valid) {
		bool oriented = false;
		bool boundary = false;
		const bool manifold = brep->IsManifold(&oriented, &boundary);
		log.Print("source solid became an openNURBS non-solid (manifold=%d, oriented=%d, boundary=%d)",
		    manifold ? 1 : 0, oriented ? 1 : 0, boundary ? 1 : 0);
	    }
	    const ON_String message(diagnostic);
	    std::string detail(message.Array() ? message.Array() : "");
	    const auto last = detail.find_last_not_of("\r\n");
	    if (last != std::string::npos)
		detail.resize(last + 1);
	    for (char &c : detail)
		if (c == '\r' || c == '\n')
		    c = ' ';
	    bu_log("FreeCAD: could not translate %s: %s\n", name.c_str(),
		detail.empty() ? "invalid BRep topology" : detail.c_str());
	    return;
	}
    } catch (const Standard_Failure &failure) {
	timings.translate += ImportClock::now() - started;
	++geometry.invalid_parts;
	const char *message = occt_failure_message(failure);
	bu_log("FreeCAD: %s: %s\n", name.c_str(), message ? message : "BRep conversion failed");
	return;
    }
    const auto primitive = unique_name(writer->dbip, name + ".s");
    const auto store_started = ImportClock::now();
    // wdb_export takes ownership.  The BRep already uses millimeters.
    struct rt_brep_internal *internal;
    BU_ALLOC(internal, struct rt_brep_internal);
    internal->magic = RT_BREP_INTERNAL_MAGIC;
    internal->brep = brep.release();
    if (wdb_export(writer, primitive.c_str(), internal, ID_BREP, 1.0))
	throw std::runtime_error("could not write FreeCAD BRep " + primitive);
    if (nonmanifold) {
	if (db5_update_attribute(primitive.c_str(), "freecad:topology",
		"source_nonmanifold", writer->dbip))
	    throw std::runtime_error("could not mark nonmanifold FreeCAD BRep " + primitive);
	++geometry.nonmanifold_parts;
	bu_log("FreeCAD: preserved %s with its authored nonmanifold edge uses\n", primitive.c_str());
    }
    geometry.parts.push_back({primitive});
    timings.store += ImportClock::now() - store_started;
}

static Geometry
read_shape(Archive &archive, const std::string &member, const std::string &name,
	struct rt_wdb *writer, double tolerance, ImportTimings &timings)
{
    Geometry result;
    TopoDS_Shape shape;
    const auto started = ImportClock::now();
    bool has_data = false;
    if (extension_is(member.c_str(), "bin")) {
	// BinTools seeks to previously read entities by their file offsets.
	// Buffer just this member, since compressed ZIP data cannot seek.
	BinaryShapeBuffer source(archive);
	std::istream in(&source);
	has_data = in.peek() != std::istream::traits_type::eof();
	if (has_data)
	    BinTools::Read(shape, in);
	if (in.bad())
	    throw std::runtime_error("failed to read FreeCAD shape " + member);
    } else {
	ShapeStream source(archive);
	std::istream in(&source);
	in.imbue(std::locale::classic());
	has_data = in.peek() != std::istream::traits_type::eof();
	if (has_data) {
	    BRep_Builder builder;
	    BRepTools::Read(shape, in, builder);
	}
	if (in.bad())
	    throw std::runtime_error("failed to read FreeCAD shape " + member);
	source.finish();
    }
    timings.decode += ImportClock::now() - started;
    if (shape.IsNull()) {
	if (has_data) {
	    ++result.invalid_parts;
	    bu_log("FreeCAD: could not decode shape member %s\n", member.c_str());
	}
	return result;
    }
    // FreeCAD stores the object placement in the shape's top-level location.
    // Keep the definition local so LinkTransform can omit that placement.
    result.transform = placement_matrix(shape.Location().Transformation());
    shape.Location(TopLoc_Location());
    write_shape(writer, name, shape, tolerance, result, timings);
    return result;
}

static Matrix
link_member_matrix(const Object &link, const Member &target)
{
    Matrix scale = MAT_INIT_IDN;
    constexpr int axes = 3;
    for (int axis = 0; axis < axes; ++axis) {
	const double value = link.has_scale_vector ? link.scale_vector[axis] : link.scale;
	if (ZERO(value))
	    throw std::runtime_error("a FreeCAD link has zero scale");
	scale[axis * (ELEMENTS_PER_PLANE + 1)] = value;
    }
    if (!link.link_transform)
	return scale;
    Matrix result;
    bn_mat_mul(result.data(), scale.data(), target.transform.data());
    return result;
}

static bool
import_document(struct rt_wdb *writer, const gcv_opts *options, const char *source_path)
{
    const auto started = ImportClock::now();
    const Document document = read_document(source_path);
    std::set<std::string> group_members;
    for (const auto &entry : document.objects)
	group_members.insert(entry.second.children.begin(), entry.second.children.end());
    std::vector<std::string> roots;
    for (const auto &entry : document.objects)
	if (entry.second.visible && !group_members.count(entry.first))
	    roots.push_back(entry.first);

    std::map<std::string, Node> nodes;
    const auto order = order_nodes(document, roots, nodes);
    std::map<std::string, std::string> wanted;
    std::map<std::string, Geometry> geometry;
    for (const auto &id : order) {
	const auto &object = document.objects.at(id);
	if (!has_link(object) && !object.shape_file.empty())
	    wanted.emplace(object.shape_file, id);
    }
    size_t written = 0;
    size_t invalid_parts = 0;
    size_t nonmanifold_parts = 0;
    ImportTimings timings;
    constexpr size_t progress_threshold = 128;
    constexpr auto report_interval = std::chrono::seconds(15);
    const bool report_progress = wanted.size() >= progress_threshold || options->verbosity_level;
    auto last_report = ImportClock::now();
    if (report_progress)
	bu_log("FreeCAD: reading %zu shape members from %zu document objects (metadata %.1fs)\n",
	    wanted.size(), document.objects.size(), seconds(last_report - started));
    if (!wanted.empty()) {
	Archive archive(source_path);
	const double tolerance = options->calculational_tolerance.dist;
	std::string member;
	while (archive.next(member)) {
	    const auto requested = wanted.find(member);
	    if (requested == wanted.end())
		continue;
	    if (geometry.find(member) != geometry.end())
		throw std::runtime_error("duplicate FCStd shape member " + member);
	    Geometry result;
	    try {
		result = read_shape(archive, member, requested->second, writer, tolerance, timings);
	    } catch (const Standard_Failure &failure) {
		++result.invalid_parts;
		const char *message = occt_failure_message(failure);
		bu_log("FreeCAD: %s: %s\n", member.c_str(), message ? message : "shape read failed");
	    }
	    written += result.parts.size();
	    invalid_parts += result.invalid_parts;
	    nonmanifold_parts += result.nonmanifold_parts;
	    geometry.emplace(member, std::move(result));
	    const auto now = ImportClock::now();
	    if (report_progress && now - last_report >= report_interval) {
		bu_log("FreeCAD: %zu/%zu shape members, %zu BReps, %zu rejected parts (%.1fs elapsed, last %s)\n",
		    geometry.size(), wanted.size(), written, invalid_parts, seconds(now - started),
		    requested->second.c_str());
		last_report = now;
	    }
	}
	for (const auto &request : wanted)
	    if (!geometry.count(request.first)) {
		++invalid_parts;
		bu_log("FreeCAD: missing archive member %s\n", request.first.c_str());
	    }
    }
    if (!written) {
	bu_log("FreeCAD: no saved BRep geometry could be imported\n");
	return false;
    }

    for (const auto &id : order) {
	const auto &object = document.objects.at(id);
	Node &node = nodes.at(id);
	std::vector<Member> members;
	node.occurrence.transform = object.placement;
	const bool linked = has_link(object);
	if (linked) {
	    if (!node.dependencies.empty()) {
		const Member &target = nodes.at(node.dependencies.front()).occurrence;
		if (!target.name.empty())
		    members.push_back({target.name, link_member_matrix(object, target)});
	    }
	    if (object.has_link_placement)
		node.occurrence.transform = object.link_placement;
	} else {
	    const auto shape = geometry.find(object.shape_file);
	    if (shape != geometry.end() && !shape->second.parts.empty()) {
		node.occurrence.transform = shape->second.transform;
		members = shape->second.parts;
	    }
	    for (const auto &child : node.dependencies) {
		const Member &occurrence = nodes.at(child).occurrence;
		if (!occurrence.name.empty())
		    members.push_back(occurrence);
	    }
	}
	if (members.empty())
	    continue;
	node.occurrence.name = unique_name(writer->dbip, id);
	Properties properties = object.properties;
	properties["name"] = id;
	properties["type"] = object.type;
	properties["visible"] = object.visible ? "true" : "false";
	if (linked)
	    properties["linked_object"] = object.linked_object;
	const auto &color = object.material_color && (!linked || object.override_material)
	    ? object.material_color : object.color;
	write_combination(writer, node.occurrence.name, members, properties,
	    color ? color->data() : nullptr, linked);
    }
    std::vector<Member> members;
    for (const auto &id : roots) {
	const Member &occurrence = nodes.at(id).occurrence;
	if (!occurrence.name.empty())
	    members.push_back(occurrence);
    }
    if (members.empty())
	return false;
    struct bu_vls basename = BU_VLS_INIT_ZERO;
    bu_path_component(&basename, source_path, BU_PATH_BASENAME_EXTLESS);
    const char *default_name = options->default_name;
    if (!default_name || BU_STR_EQUAL(default_name, "unnamed"))
	default_name = bu_vls_cstr(&basename);
    const auto root_name = unique_name(writer->dbip, default_name);
    bu_vls_free(&basename);
    write_combination(writer, root_name, members, document.properties, nullptr);
    if (report_progress)
	bu_log("FreeCAD: %zu BReps from %zu shape members, %zu rejected parts, %zu source nonmanifold parts, %zu assembly roots (%.1fs read, %.1fs translate, %.1fs store, %.1fs reader total)\n",
	    written, geometry.size(), invalid_parts, nonmanifold_parts, members.size(), seconds(timings.decode),
	    seconds(timings.translate), seconds(timings.store), seconds(ImportClock::now() - started));
    return true;
}

static int
read(gcv_context *context, const gcv_opts *options, const void *UNUSED(data), const char *path)
{
    if (!context || !context->dbip || !options || !path)
	return 0;
    struct rt_wdb *writer = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!writer)
	return 0;
    try {
	return import_document(writer, options, path);
    } catch (const Standard_Failure &failure) {
	const char *message = occt_failure_message(failure);
	bu_log("FreeCAD: %s: %s\n", path, message ? message : "OCCT import failed");
    } catch (const std::exception &failure) {
	bu_log("FreeCAD: %s: %s\n", path, failure.what());
    }
    return 0;
}

static int
can_read(const char *path)
{
    if (!path)
	return 0;
    if (extension_is(path, "fcstd"))
	return 1;
    try {
	Archive archive(path);
	std::string name;
	while (archive.next(name))
	    if (name == "Document.xml")
		return 1;
    } catch (const std::runtime_error &) {
	return 0;
    }
    return 0;
}

static const gcv_filter reader = {
    "FreeCAD Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_FREECAD, can_read,
    nullptr, nullptr, read
};
static const gcv_filter * const filters[] = {&reader, nullptr};

}

extern "C" COMPILER_DLLEXPORT const gcv_plugin *
gcv_plugin_info()
{
    static const gcv_plugin plugin = {freecad::filters};
    return &plugin;
}

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
