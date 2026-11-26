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
#include <cmath>
#include <cstdint>
#include <cstring>
#include <istream>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
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
#include <gp_Quaternion.hxx>
#include <gp_Trsf.hxx>

#include "opennurbs.h"

#include "bn/mat.h"
#include "bu/opt.h"
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

class BinaryMemberBuffer : public std::streambuf {
public:
    explicit BinaryMemberBuffer(Archive &archive)
    {
	constexpr size_t block_size = 64 * 1024;
	std::array<char, block_size> block;
	for (;;) {
	    const auto count = archive.read(block.data(), block.size());
	    if (count < 0)
		throw std::runtime_error("could not inflate binary FCStd archive member");
	    if (!count)
		break;
	    storage.insert(storage.end(), block.data(), block.data() + count);
	}
	if (!storage.empty())
	    setg(storage.data(), storage.data(), storage.data() + storage.size());
    }

    std::string_view data() const
    {
	return storage.empty() ? std::string_view{} : std::string_view(storage.data(), storage.size());
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
    return !object.linked_object.empty() || object.is_link() || object.unsupported_link;
}

static std::vector<std::string>
child_dependencies(const Document &document, const std::string &id,
	const Object &object, const std::vector<std::string> &entries, bool per_element,
	size_t &unavailable)
{
    std::vector<std::string> children;
    for (size_t index = 0; index < entries.size(); ++index) {
	const auto &child = entries[index];
	if (child.empty() || (per_element && !object.element_visible(index)))
	    continue;
	const auto found = document.objects.find(child);
	if (found == document.objects.end()) {
	    bu_log("FreeCAD: %s references missing group member %s\n", id.c_str(), child.c_str());
	    ++unavailable;
	    continue;
	}
	// Link groups and arrays can show a member hidden elsewhere.
	if (!found->second.suppressed && (per_element || found->second.visible))
	    children.push_back(child);
    }
    return children;
}

static std::vector<std::string>
dependencies(const Document &document, const std::string &id, const Object &object,
	size_t &unavailable)
{
    if (has_link(object)) {
	if (object.unsupported_link) {
	    bu_log("FreeCAD: %s uses an external or subobject link\n", id.c_str());
	    ++unavailable;
	    return {};
	}
	if (object.expanded_array()) {
	    if (object.array_elements.size() != object.element_count) {
		bu_log("FreeCAD: %s has an inconsistent link array element list\n", id.c_str());
		++unavailable;
		return {};
	    }
	    return child_dependencies(document, id, object, object.array_elements, true, unavailable);
	}
	const auto target = document.objects.find(object.linked_object);
	if (target != document.objects.end()) {
	    if (target->second.suppressed)
		return {};
	    return {object.linked_object};
	}
	bu_log("FreeCAD: %s has a missing link target\n", id.c_str());
	++unavailable;
	return {};
    }
    // A saved Body shape already represents its modeling history.
    if (!object.shape_file.empty() && object.type == "PartDesign::Body")
	return {};
    if (object.shape_file.empty() && (object.type == "Mesh::Feature" || object.type == "Mesh::FeaturePython")) {
	bu_log("FreeCAD: %s uses an unsupported mesh object\n", id.c_str());
	++unavailable;
    }
    return child_dependencies(document, id, object, object.children, object.is_link_group(), unavailable);
}

struct Node {
    Member occurrence;
    std::vector<std::string> dependencies;
    bool complete = false;
};

static std::vector<std::string>
order_nodes(const Document &document, const std::vector<std::string> &roots,
	std::map<std::string, Node> &nodes, size_t &unavailable)
{
    std::vector<std::string> order;
    for (const auto &root : roots) {
	auto added = nodes.emplace(root, Node());
	if (!added.second)
	    continue;
	added.first->second.dependencies = dependencies(document, root, document.objects.at(root), unavailable);
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
		    document.objects.at(child), unavailable);
		stack.emplace_back(child, 0);
	    } else if (!inserted.first->second.complete) {
		throw std::runtime_error("cyclic FreeCAD hierarchy at " + child);
	    }
	}
    }
    return order;
}

struct Geometry {
    Matrix location = MAT_INIT_IDN;
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
	BinaryMemberBuffer source(archive);
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
    // Keep the geometry reusable. The saved location can contain a shape
    // offset in addition to the owning object's placement.
    result.location = placement_matrix(shape.Location().Transformation());
    shape.Location(TopLoc_Location());
    write_shape(writer, name, shape, tolerance, result, timings);
    return result;
}

static void
place_shape(const Object &object, const Geometry &geometry, Member &occurrence,
	std::vector<Member> &members)
{
    members = geometry.parts;
    occurrence.transform = object.placement.value_or(geometry.location);
    if (!object.placement)
	return;

    // LinkTransform can omit the object's placement. Keep the remaining
    // shape location on the primitive members so it follows every link.
    Matrix inverse_placement;
    if (!bn_mat_inverse(inverse_placement.data(), object.placement->data()))
	throw std::runtime_error("could not invert FreeCAD shape placement");
    Matrix shape_offset;
    bn_mat_mul(shape_offset.data(), inverse_placement.data(), geometry.location.data());
    for (auto &member : members)
	bn_mat_mul2(shape_offset.data(), member.transform.data());
}

static Matrix
scale_matrix(const Scale &values)
{
    Matrix scale = MAT_INIT_IDN;
    for (size_t axis = 0; axis < values.size(); ++axis) {
	const double value = values[axis];
	if (ZERO(value))
	    throw std::runtime_error("a FreeCAD link has zero scale");
	scale[axis * (ELEMENTS_PER_PLANE + 1)] = value;
    }
    return scale;
}

template <typename UInt>
static UInt
read_little_endian(const char *data)
{
    UInt result = 0;
    for (size_t byte = 0; byte < sizeof(UInt); ++byte)
	result |= static_cast<UInt>(static_cast<unsigned char>(data[byte])) << (byte * 8);
    return result;
}

template <typename Real, typename UInt>
static Real
read_list_real(const char *data)
{
    static_assert(sizeof(Real) == sizeof(UInt), "unsupported floating-point size");
    const auto bits = read_little_endian<UInt>(data);
    Real result;
    std::memcpy(&result, &bits, sizeof(result));
    if (!std::isfinite(result))
	throw std::runtime_error("non-finite value in FCStd link array");
    return result;
}

static constexpr size_t placement_components = 7;
static constexpr size_t scale_components = 3;
using ArrayLists = std::map<std::string, std::vector<double>>;

static std::vector<double>
read_array_list(Archive &archive, size_t components)
{
    const BinaryMemberBuffer buffer(archive);
    auto data = buffer.data();
    if (data.size() < sizeof(uint32_t))
	throw std::runtime_error("truncated FCStd link array header");
    const auto count = read_little_endian<uint32_t>(data.data());
    data.remove_prefix(sizeof(uint32_t));
    if (!count && data.empty())
	return {};
    if (!count || count > data.size() / components)
	throw std::runtime_error("invalid FCStd link array size");

    // FreeCAD writes a count followed by either float32 or float64 tuples.
    const size_t values = count * components;
    const size_t bytes = data.size() / values;
    if (data.size() % values || (bytes != sizeof(uint32_t) && bytes != sizeof(uint64_t)))
	throw std::runtime_error("invalid FCStd link array payload");
    std::vector<double> result;
    result.reserve(values);
    for (size_t index = 0; index < values; ++index) {
	const char *value = data.data() + index * bytes;
	if (bytes == sizeof(uint32_t))
	    result.push_back(read_list_real<float, uint32_t>(value));
	else
	    result.push_back(read_list_real<double, uint64_t>(value));
    }
    return result;
}

static bool
find_array_list(const ArrayLists &lists, const std::string &file, const std::vector<double> *&result)
{
    if (file.empty()) {
	result = nullptr;
	return true;
    }
    const auto found = lists.find(file);
    if (found == lists.end())
	return false;
    result = &found->second;
    return true;
}

static void
write_link_members(const Object &object, const Node &node,
	const std::map<std::string, Node> &nodes, const ArrayLists &lists,
	std::vector<Member> &members, const std::string &id, size_t &unavailable)
{
    if (node.dependencies.empty())
	return;
    const Matrix scale = scale_matrix(object.has_scale_vector ? object.scale_vector :
	Scale{object.scale, object.scale, object.scale});
    if (object.expanded_array()) {
	for (const auto &element : node.dependencies) {
	    Member target = nodes.at(element).occurrence;
	    if (!target.name.empty()) {
		bn_mat_mul2(scale.data(), target.transform.data());
		members.push_back(std::move(target));
	    }
	}
	return;
    }

    const auto &target = nodes.at(node.dependencies.front()).occurrence;
    if (target.name.empty())
	return;
    const Matrix identity = MAT_INIT_IDN;
    const Matrix &target_placement = object.link_transform ? target.transform : identity;
    if (!object.element_count) {
	Member member{target.name, target_placement};
	bn_mat_mul2(scale.data(), member.transform.data());
	members.push_back(std::move(member));
	return;
    }

    const std::vector<double> *placements = nullptr;
    const std::vector<double> *scales = nullptr;
    if (!find_array_list(lists, object.array_placements, placements) ||
	    !find_array_list(lists, object.array_scales, scales)) {
	bu_log("FreeCAD: %s is missing an array placement or scale member\n", id.c_str());
	++unavailable;
	return;
    }
    for (size_t index = 0; index < object.element_count; ++index) {
	if (!object.element_visible(index))
	    continue;
	Matrix element_placement = MAT_INIT_IDN;
	if (placements && index < placements->size() / placement_components) {
	    const double *p = placements->data() + index * placement_components;
	    element_placement = placement_matrix(gp_Quaternion(p[3], p[4], p[5], p[6]), {p[0], p[1], p[2]});
	}
	Matrix element_scale = MAT_INIT_IDN;
	if (scales && index < scales->size() / scale_components) {
	    const double *s = scales->data() + index * scale_components;
	    element_scale = scale_matrix({s[0], s[1], s[2]});
	}
	Member member{target.name};
	bn_mat_mul4(member.transform.data(), scale.data(), element_placement.data(),
	    element_scale.data(), target_placement.data());
	members.push_back(std::move(member));
    }
}

static bool
import_document(struct rt_wdb *writer, const gcv_opts *options, const char *source_path,
	bool strict)
{
    const auto started = ImportClock::now();
    const Document document = read_document(source_path);
    std::set<std::string> group_members;
    for (const auto &entry : document.objects) {
	const auto &object = entry.second;
	group_members.insert(object.children.begin(), object.children.end());
	if (object.expanded_array())
	    group_members.insert(object.array_elements.begin(), object.array_elements.end());
    }
    std::vector<std::string> roots;
    for (const auto &entry : document.objects)
	if (entry.second.visible && !entry.second.suppressed && !group_members.count(entry.first))
	    roots.push_back(entry.first);

    std::map<std::string, Node> nodes;
    size_t unavailable = 0;
    const auto order = order_nodes(document, roots, nodes, unavailable);
    if (strict && unavailable) {
	bu_log("FreeCAD: strict import rejected %zu unavailable references\n", unavailable);
	return false;
    }
    std::map<std::string, std::string> wanted;
    std::map<std::string, Geometry> geometry;
    std::map<std::string, size_t> array_fields;
    ArrayLists array_lists;
    const auto request_list = [&](const std::string &file, size_t components) {
	if (file.empty())
	    return;
	const auto result = array_fields.emplace(file, components);
	if (result.first->second != components)
	    throw std::runtime_error("conflicting FCStd link array member " + file);
    };
    for (const auto &id : order) {
	const auto &object = document.objects.at(id);
	if (!has_link(object) && !object.shape_file.empty())
	    wanted.emplace(object.shape_file, id);
	if (object.element_count && !object.expanded_array() && !nodes.at(id).dependencies.empty()) {
	    request_list(object.array_placements, placement_components);
	    request_list(object.array_scales, scale_components);
	}
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
	    const auto array = array_fields.find(member);
	    if (array != array_fields.end()) {
		if (requested != wanted.end() || array_lists.count(member))
		    throw std::runtime_error("duplicate or conflicting FCStd array member " + member);
		array_lists.emplace(member, read_array_list(archive, array->second));
		continue;
	    }
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
	    if (result.parts.empty() && !result.invalid_parts) {
		bu_log("FreeCAD: %s contains no saved BRep surfaces\n", member.c_str());
		++result.invalid_parts;
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
	if (object.placement)
	    node.occurrence.transform = *object.placement;
	const bool linked = has_link(object);
	if (linked) {
	    write_link_members(object, node, nodes, array_lists, members, id, unavailable);
	    if (object.has_link_placement)
		node.occurrence.transform = object.link_placement;
	} else {
	    const auto shape = geometry.find(object.shape_file);
	    if (shape != geometry.end() && !shape->second.parts.empty())
		place_shape(object, shape->second, node.occurrence, members);
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
    if (unavailable || invalid_parts) {
	bu_log("FreeCAD: %s (%zu unavailable references, %zu rejected parts)\n",
	    strict ? "strict import rejected" : "partial import", unavailable, invalid_parts);
	if (strict)
	    return false;
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

static void
create_options(struct bu_opt_desc **descriptions, void **data)
{
    auto strict = std::make_unique<int>(0);
    const struct bu_opt_desc options[] = {
	{"", "strict", "", nullptr, strict.get(), "reject partial geometry imports"},
	BU_OPT_DESC_NULL
    };
    *descriptions = static_cast<struct bu_opt_desc *>(bu_malloc(sizeof(options), "FreeCAD reader options"));
    std::copy_n(options, sizeof(options) / sizeof(options[0]), *descriptions);
    *data = strict.release();
}

static void
free_options(void *data)
{
    delete static_cast<int *>(data);
}

static int
read(gcv_context *context, const gcv_opts *options, const void *data, const char *path)
{
    if (!context || !context->dbip || !options || !path)
	return 0;
    struct rt_wdb *writer = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!writer)
	return 0;
    try {
	return import_document(writer, options, path, data && *static_cast<const int *>(data));
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
    create_options, free_options, read
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
