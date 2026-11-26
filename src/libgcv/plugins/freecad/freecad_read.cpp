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
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <istream>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
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
#include "bg/trimesh.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/process.h"
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
	const unsigned char *color = nullptr, bool override_color = false,
	std::optional<double> transparency = std::nullopt, bool face_colors = false)
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
    const bool transmit = transparency && (override_color || !ZERO(*transparency));
    char shader[] = "plastic";
    std::string shader_args = transmit ? "tr=" + std::to_string(*transparency) : "";
    if (face_colors)
	shader_args += shader_args.empty() ? "face_colors=1" : " face_colors=1";
    if (mk_comb(writer, name.c_str(), &head.l, 0, shader_args.empty() ? nullptr : shader,
	    shader_args.empty() ? nullptr : shader_args.data(), color,
	    0, 0, 0, 0, override_color && (color || transparency), 0, 0))
	throw std::runtime_error("could not write FreeCAD combination " + name);
    struct directory *entry = db_lookup(writer->dbip, name.c_str(), LOOKUP_QUIET);
    if (!entry)
	throw std::runtime_error("could not find FreeCAD combination " + name);

    struct bu_attribute_value_set attrs;
    bu_avs_init(&attrs, properties.size() + (transparency ? 1 : 0), "FreeCAD properties");
    for (const auto &property : properties)
	bu_avs_add(&attrs, ("freecad:" + property.first).c_str(), property.second.c_str());
    if (transparency)
	bu_avs_add(&attrs, "freecad:transparency", std::to_string(*transparency).c_str());
    if (db5_update_attributes(entry, &attrs, writer->dbip))
	throw std::runtime_error("could not write FreeCAD properties for " + name);
}

struct ObjectKey {
    size_t archive;
    std::string id;
    std::string selection = {};

    bool operator<(const ObjectKey &other) const
    {
	return std::tie(archive, id, selection) < std::tie(other.archive, other.id, other.selection);
    }

    std::string name() const
    {
	return selection.empty() ? id : id + "." + selection;
    }
};

struct SourceDocument {
    std::string path;
    std::string archive;
    Document document;
    std::map<std::string, std::optional<size_t>> resolved = {};
};

using Snapshots = std::map<std::filesystem::path, std::string>;

static std::filesystem::path
normalized_path(const std::filesystem::path &file)
{
    std::error_code error;
    const auto path = std::filesystem::weakly_canonical(file, error);
    return error || !path.is_absolute() ? std::filesystem::absolute(file).lexically_normal() : path;
}

class DocumentSet {
public:
    explicit DocumentSet(const char *path, const Snapshots &updates) : snapshots(updates)
    {
	const auto file = normalized_path(std::filesystem::u8path(path));
	load(file);
	files.emplace(file, 0);
    }

    std::optional<size_t> resolve(size_t owner, const std::string &name)
    {
	if (name.empty())
	    return owner;
	auto &source = sources.at(owner);
	const auto reference = source.resolved.try_emplace(name, std::nullopt);
	if (!reference.second)
	    return reference.first->second;
	const auto file = normalized_path(std::filesystem::u8path(source.path).parent_path() /
		std::filesystem::u8path(name));
	const auto result = files.try_emplace(file, std::nullopt);
	auto &index = result.first->second;
	if (result.second) {
	    try {
		load(file);
		index = sources.size() - 1;
	    } catch (const std::exception &failure) {
		bu_log("FreeCAD: could not read linked document %s: %s\n", name.c_str(), failure.what());
	    }
	}
	reference.first->second = index;
	return index;
    }

    const Object &object(const ObjectKey &key) const
    {
	return sources.at(key.archive).document.objects.at(key.id);
    }

    std::deque<SourceDocument> sources;

private:
    void load(const std::filesystem::path &file)
    {
	const auto origin = file.u8string();
	const auto update = snapshots.find(file);
	if (update == snapshots.end()) {
	    sources.push_back({origin, origin, read_document(origin.c_str())});
	    return;
	}
	sources.push_back({origin, update->second, read_document(update->second.c_str(), origin.c_str())});
    }

    const Snapshots &snapshots;
    std::map<std::filesystem::path, std::optional<size_t>> files;
};

struct Reference {
    ObjectKey key;
    Matrix prefix = MAT_INIT_IDN;
    bool use_placement = true;
};

static std::vector<Reference>
child_dependencies(const Document &document, const ObjectKey &key,
	const Object &object, const std::vector<std::string> &entries, bool per_element,
	size_t &unavailable)
{
    std::vector<Reference> children;
    for (size_t index = 0; index < entries.size(); ++index) {
	const auto &child = entries[index];
	if (child.empty() || (per_element && !object.element_visible(index)))
	    continue;
	const auto found = document.objects.find(child);
	if (found == document.objects.end()) {
	    bu_log("FreeCAD: %s references missing group member %s\n", key.id.c_str(), child.c_str());
	    ++unavailable;
	    continue;
	}
	// Link groups and arrays can show a member hidden elsewhere.
	if (!found->second.suppressed && (per_element || found->second.visible))
	    children.push_back({{key.archive, child}});
    }
    return children;
}

static Matrix
object_placement(const Object &object)
{
    const Matrix identity = MAT_INIT_IDN;
    return object.has_link_placement ? object.link_placement : object.placement.value_or(identity);
}

static Matrix scale_matrix(const Scale &values);

static Matrix
link_scale(const Object &object)
{
    return scale_matrix(object.has_scale_vector ? object.scale_vector :
	Scale{object.scale, object.scale, object.scale});
}

static std::optional<ObjectKey>
linked_root(DocumentSet &documents, size_t archive, const Object &object)
{
    const auto source = documents.resolve(archive, object.linked_file);
    if (!source || !documents.sources[*source].document.objects.count(object.linked_object))
	return std::nullopt;
    return ObjectKey{*source, object.linked_object};
}

static std::optional<Reference>
resolve_subobject(DocumentSet &documents, const ObjectKey &root, std::string path,
	bool link_transform)
{
    Reference target{root};
    bool use_placement = link_transform;
    std::set<ObjectKey> traversed;
    const auto append_transform = [&](const Matrix &transform) {
	Matrix prefix;
	bn_mat_mul(prefix.data(), target.prefix.data(), transform.data());
	target.prefix = prefix;
    };
    while (!path.empty()) {
	const auto &parent = documents.object(target.key);
	if (!traversed.insert(target.key).second)
	    return std::nullopt;
	const auto dot = path.find('.');
	if (!parent.has_link() && dot == std::string::npos)
	    break;
	if (use_placement) {
	    append_transform(object_placement(parent));
	}
	if (parent.has_link() && !parent.expanded_array()) {
	    if (parent.unsupported_link || parent.element_count || parent.linked_subobjects.size() > 1)
		return std::nullopt;
	    const auto next = linked_root(documents, target.key.archive, parent);
	    if (!next || documents.object(*next).suppressed)
		return std::nullopt;
	    if (!parent.linked_subobjects.empty() && !parent.linked_subobjects.front().empty()) {
		const auto &selected = parent.linked_subobjects.front();
		if (selected.back() != '.')
		    return std::nullopt;
		path.insert(0, selected);
	    }
	    append_transform(link_scale(parent));
	    target.key = *next;
	    use_placement = parent.link_transform;
	    continue;
	}
	if (dot == std::string::npos)
	    return std::nullopt;
	const auto &document = documents.sources[target.key.archive].document;
	const auto child_name = std::string_view(path).substr(0, dot);
	const auto match = [&](const std::string &id) {
	    const auto child = document.objects.find(id);
	    if (child == document.objects.end() || child->second.suppressed)
		return false;
	    if (child_name == id)
		return true;
	    const auto label = child->second.properties.find("Label");
	    return !child_name.empty() && child_name.front() == '$' &&
		label != child->second.properties.end() && label->second == child_name.substr(1);
	};
	const auto &children = parent.expanded_array() ? parent.array_elements : parent.children;
	const auto child = std::find_if(children.begin(), children.end(), match);
	if (child == children.end())
	    return std::nullopt;
	if (parent.expanded_array())
	    append_transform(link_scale(parent));
	target.key.id = *child;
	use_placement = true;
	path.erase(0, dot + 1);
    }
    target.key.selection = path;
    target.use_placement = use_placement;
    return target;
}

static std::vector<Reference>
dependencies(DocumentSet &documents, const ObjectKey &key, size_t &unavailable)
{
    const auto &document = documents.sources[key.archive].document;
    const auto &object = documents.object(key);
    if (!key.selection.empty())
	return {};
    if (object.has_link()) {
	if (object.unsupported_link) {
	    bu_log("FreeCAD: %s has an unsupported link property\n", key.id.c_str());
	    ++unavailable;
	    return {};
	}
	if (object.linked_subobject_count && *object.linked_subobject_count != object.linked_subobjects.size()) {
	    bu_log("FreeCAD: %s has an inconsistent subobject list\n", key.id.c_str());
	    ++unavailable;
	    return {};
	}
	if (object.expanded_array()) {
	    if (object.array_elements.size() != object.element_count) {
		bu_log("FreeCAD: %s has an inconsistent link array element list\n", key.id.c_str());
		++unavailable;
		return {};
	    }
	    return child_dependencies(document, key, object, object.array_elements, true, unavailable);
	}
	const auto root = linked_root(documents, key.archive, object);
	if (root) {
	    if (documents.object(*root).suppressed)
		return {};
	    if (object.linked_subobjects.empty())
		return {{*root, MAT_INIT_IDN, object.link_transform}};
	    std::vector<Reference> targets;
	    for (const auto &path : object.linked_subobjects) {
		auto target = resolve_subobject(documents, *root, path, object.link_transform);
		if (target) {
		    targets.push_back(std::move(*target));
		} else {
		    bu_log("FreeCAD: %s has an unavailable subobject %s.%s\n", key.id.c_str(),
			root->id.c_str(), path.c_str());
		    ++unavailable;
		}
	    }
	    return targets;
	}
	bu_log("FreeCAD: %s has a missing link target %s\n", key.id.c_str(), object.linked_object.c_str());
	++unavailable;
	return {};
    }
    // A saved Body shape already represents its modeling history.
    if (!object.shape_file.empty() && object.type == "PartDesign::Body")
	return {};
    if (object.mesh_file.empty() && (object.type == "Mesh::Feature" || object.type == "Mesh::FeaturePython")) {
	bu_log("FreeCAD: %s has no saved mesh\n", key.id.c_str());
	++unavailable;
    }
    return child_dependencies(document, key, object, object.children, object.is_link_group(), unavailable);
}

struct Node {
    Member occurrence;
    std::vector<Reference> dependencies;
    bool complete = false;
};

using Nodes = std::map<ObjectKey, Node>;

static std::vector<ObjectKey>
order_nodes(DocumentSet &documents, const std::vector<ObjectKey> &roots,
	Nodes &nodes, size_t &unavailable)
{
    std::vector<ObjectKey> order;
    for (const auto &root : roots) {
	auto added = nodes.emplace(root, Node());
	if (!added.second)
	    continue;
	added.first->second.dependencies = dependencies(documents, root, unavailable);
	std::vector<std::pair<ObjectKey, size_t>> stack = {{root, 0}};
	while (!stack.empty()) {
	    auto &frame = stack.back();
	    Node &node = nodes.at(frame.first);
	    if (frame.second == node.dependencies.size()) {
		node.complete = true;
		order.push_back(frame.first);
		stack.pop_back();
		continue;
	    }
	    const size_t child_index = frame.second++;
	    const auto &child = node.dependencies[child_index].key;
	    auto inserted = nodes.emplace(child, Node());
	    if (inserted.second) {
		inserted.first->second.dependencies = dependencies(documents, child, unavailable);
		stack.emplace_back(child, 0);
	    } else if (!inserted.first->second.complete) {
		bu_log("FreeCAD: cyclic reference from %s to %s\n", frame.first.id.c_str(), child.id.c_str());
		++unavailable;
		node.dependencies.erase(node.dependencies.begin() + child_index);
		--frame.second;
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
    bool face_colors = false;
};

struct Appearance {
    Rgb color;
    double transparency;
};

using Shapes = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;
using Appearances = std::vector<Appearance>;

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
write_shape(struct rt_wdb *writer, const ObjectKey &key,
	const TopoDS_Shape &shape, double tolerance, Geometry &geometry, ImportTimings &timings,
	const Shapes &source_faces, const Appearances &appearance)
{
    if (shape.ShapeType() == TopAbs_COMPOUND || shape.ShapeType() == TopAbs_COMPSOLID) {
	for (TopoDS_Iterator child(shape); child.More(); child.Next())
	    write_shape(writer, key, child.Value(), tolerance, geometry, timings, source_faces, appearance);
	return;
    }
    if (key.selection.empty() && !TopExp_Explorer(shape, TopAbs_FACE).More())
	return;
    const auto name = key.name();
    if (shape.ShapeType() == TopAbs_VERTEX) {
	const auto point = BRep_Tool::Pnt(TopoDS::Vertex(shape));
	const point_t position = {point.X(), point.Y(), point.Z()};
	const auto primitive = unique_name(writer->dbip, name + ".s");
	if (mk_pnts(writer, primitive.c_str(), RT_PNT_TYPE_PNT, 0.0, 1, position, nullptr, nullptr, nullptr))
	    throw std::runtime_error("could not write FreeCAD point " + primitive);
	geometry.parts.push_back({primitive});
	return;
    }
    if (!TopExp_Explorer(shape, TopAbs_VERTEX).More())
	return;

    auto brep = std::make_unique<ON_Brep>();
    const auto started = ImportClock::now();
    bool nonmanifold = false;
    try {
	ON_wString diagnostic;
	ON_TextLog log(diagnostic);
	const bool valid = occt_to_brep(shape, *brep, tolerance, &log);
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
    if (!source_faces.IsEmpty()) {
	Shapes faces;
	TopExp::MapShapes(shape, TopAbs_FACE, faces);
	for (int face = 0; face < brep->m_F.Count(); ++face) {
	    const auto source = source_faces.FindIndex(faces.FindKey(face + 1));
	    if (!source)
		continue;
	    const auto &material = appearance[source - 1];
	    constexpr auto channel_max = std::numeric_limits<unsigned char>::max();
	    const int transparency = static_cast<int>(std::lround(material.transparency * channel_max));
	    ON_Color color(material.color[0], material.color[1], material.color[2], transparency);
	    // openNURBS reserves transparent white for faces without an override.
	    if (color == ON_Color::UnsetColor)
		color.SetRGBA(color.Red(), color.Green(), color.Blue() - 1, transparency);
	    brep->m_F[face].SetPerFaceColor(color);
	}
	geometry.face_colors |= brep->HasPerFaceColors();
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

static TopoDS_Shape
read_shape(Archive &archive, const std::string &member, ImportTimings &timings)
{
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
    if (shape.IsNull() && has_data)
	throw std::runtime_error("could not decode shape member " + member);
    return shape;
}

static TopoDS_Shape
select_shape(const TopoDS_Shape &shape, const std::string &selection)
{
    static constexpr std::pair<std::string_view, TopAbs_ShapeEnum> types[] = {
	{"Compound", TopAbs_COMPOUND}, {"CompSolid", TopAbs_COMPSOLID},
	{"Solid", TopAbs_SOLID}, {"Shell", TopAbs_SHELL}, {"Face", TopAbs_FACE},
	{"Wire", TopAbs_WIRE}, {"Edge", TopAbs_EDGE}, {"Vertex", TopAbs_VERTEX}
    };
    for (const auto &type : types) {
	if (selection.compare(0, type.first.size(), type.first))
	    continue;
	int index = 0;
	const char *end = selection.data() + selection.size();
	const auto converted = std::from_chars(selection.data() + type.first.size(), end, index);
	if (converted.ec != std::errc() || converted.ptr != end || index <= 0)
	    return {};
	NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> elements;
	TopExp::MapShapes(shape, type.second, elements);
	return static_cast<size_t>(index) <= static_cast<size_t>(elements.Size())
	    ? elements.FindKey(index) : TopoDS_Shape();
    }
    return {};
}

static Geometry
translate_shape(struct rt_wdb *writer, const ObjectKey &key, TopoDS_Shape shape,
	double tolerance, ImportTimings &timings, const Appearances &appearance)
{
    Geometry result;
    if (shape.IsNull())
	return result;
    // Keep the geometry reusable. The saved location can contain a shape
    // offset in addition to the owning object's placement.
    result.location = placement_matrix(shape.Location().Transformation());
    shape.Location(TopLoc_Location());
    Shapes source_faces;
    if (appearance.size() > 1) {
	TopExp::MapShapes(shape, TopAbs_FACE, source_faces);
	const auto face_count = static_cast<size_t>(source_faces.Size());
	if (face_count != appearance.size()) {
	    bu_log("FreeCAD: %s has %zu appearance entries for %zu saved faces\n",
		key.id.c_str(), appearance.size(), face_count);
	    source_faces.Clear();
	}
    }
    if (!key.selection.empty()) {
	shape = select_shape(shape, key.selection);
	if (shape.IsNull()) {
	    bu_log("FreeCAD: %s has no saved %s\n", key.id.c_str(), key.selection.c_str());
	    ++result.invalid_parts;
	    return result;
	}
    }
    write_shape(writer, key, shape, tolerance, result, timings, source_faces, appearance);
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
read_binary_real(const char *data)
{
    static_assert(sizeof(Real) == sizeof(UInt), "unsupported floating-point size");
    const auto bits = read_little_endian<UInt>(data);
    Real result;
    std::memcpy(&result, &bits, sizeof(result));
    if (!std::isfinite(result))
	throw std::runtime_error("non-finite value in FCStd binary member");
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
	    result.push_back(read_binary_real<float, uint32_t>(value));
	else
	    result.push_back(read_binary_real<double, uint64_t>(value));
    }
    return result;
}

static Appearances
read_appearance_list(Archive &archive, bool materials, bool legacy_alpha)
{
    const BinaryMemberBuffer buffer(archive);
    auto data = buffer.data();
    if (data.size() < sizeof(uint32_t))
	throw std::runtime_error("truncated FCStd appearance header");
    auto count = read_little_endian<uint32_t>(data.data());
    data.remove_prefix(sizeof(uint32_t));
    // Early MaterialList writers stored a negative version marker before the count.
    if (materials && count > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) &&
	data.size() >= sizeof(uint32_t)) {
	count = read_little_endian<uint32_t>(data.data());
	data.remove_prefix(sizeof(uint32_t));
    }
    // Material lists store ambient, diffuse, specular, emission, shine and transparency.
    constexpr size_t material_size = 4 * sizeof(uint32_t) + 2 * sizeof(float);
    constexpr size_t diffuse_offset = sizeof(uint32_t);
    constexpr size_t transparency_offset = material_size - sizeof(float);
    const size_t stride = materials ? material_size : sizeof(uint32_t);
    if (count > data.size() / stride || (!materials && count * stride != data.size()))
	throw std::runtime_error("invalid FCStd appearance list size");
    Appearances result;
    result.reserve(count);
    for (size_t index = 0; index < count; ++index) {
	const char *entry = data.data() + index * stride;
	const auto rgba = read_little_endian<uint32_t>(entry + (materials ? diffuse_offset : 0));
	constexpr auto channel_max = std::numeric_limits<unsigned char>::max();
	const double alpha = static_cast<unsigned char>(rgba) / static_cast<double>(channel_max);
	const double transparency = materials ? read_binary_real<float, uint32_t>(entry + transparency_offset)
	    : (legacy_alpha ? alpha : 1.0 - alpha);
	result.push_back({unpack_rgb(rgba), std::clamp(transparency, 0.0, 1.0)});
    }
    return result;
}

using AppearanceLists = std::map<std::string, Appearances>;

static std::string
appearance_member(const Object &object)
{
    return object.appearance_file.empty() ? object.diffuse_file : object.appearance_file;
}

static AppearanceLists
read_appearances(const SourceDocument &source, const std::map<std::string, bool> &wanted)
{
    AppearanceLists result;
    if (wanted.empty())
	return result;
    Archive archive(source.path.c_str());
    std::string member;
    while (archive.next(member)) {
	const auto entry = wanted.find(member);
	if (entry != wanted.end() &&
	    !result.emplace(member, read_appearance_list(archive, entry->second,
		source.document.legacy_alpha)).second)
	    throw std::runtime_error("duplicate FCStd appearance member " + member);
    }
    for (const auto &entry : wanted)
	if (!result.count(entry.first))
	    bu_log("FreeCAD: missing appearance member %s\n", entry.first.c_str());
    return result;
}

struct TriangleMesh {
    std::vector<fastf_t> vertices;
    std::vector<int> faces;
};

static TriangleMesh
read_mesh(Archive &archive)
{
    const BinaryMemberBuffer source(archive);
    const auto data = source.data();
    // MeshKernel::Write stores a magic number, version, and 256-byte description.
    constexpr uint32_t mesh_magic = 0xa0b0c0d0;
    constexpr uint32_t mesh_version = 0x010000;
    constexpr size_t count_offset = 2 * sizeof(uint32_t) + 256;
    constexpr size_t header_size = count_offset + 2 * sizeof(uint32_t);
    constexpr size_t point_size = ELEMENTS_PER_VECT * sizeof(float);
    constexpr size_t facet_size = 2 * ELEMENTS_PER_VECT * sizeof(uint32_t);
    constexpr size_t bounds_size = 2 * point_size;
    if (data.size() < header_size + bounds_size ||
	read_little_endian<uint32_t>(data.data()) != mesh_magic ||
	read_little_endian<uint32_t>(data.data() + sizeof(uint32_t)) != mesh_version)
	throw std::runtime_error("unsupported or truncated FCStd mesh header");
    const auto point_count = read_little_endian<uint32_t>(data.data() + count_offset);
    const auto face_count = read_little_endian<uint32_t>(data.data() + count_offset + sizeof(uint32_t));
    const size_t payload = data.size() - header_size - bounds_size;
    constexpr auto max_elements = std::numeric_limits<int>::max() / ELEMENTS_PER_VECT;
    if (point_count > max_elements || face_count > max_elements ||
	point_count > payload / point_size || face_count > (payload - point_count * point_size) / facet_size ||
	point_count * point_size + face_count * facet_size != payload)
	throw std::runtime_error("invalid FCStd mesh member size");
    if (!point_count || !face_count)
	return {};

    const char *point = data.data() + header_size;
    TriangleMesh mesh;
    mesh.vertices.reserve(static_cast<size_t>(point_count) * ELEMENTS_PER_VECT);
    for (size_t index = 0; index < static_cast<size_t>(point_count) * ELEMENTS_PER_VECT; ++index) {
	mesh.vertices.push_back(read_binary_real<float, uint32_t>(point));
	point += sizeof(float);
    }
    mesh.faces.reserve(static_cast<size_t>(face_count) * ELEMENTS_PER_VECT);
    for (size_t face = 0; face < face_count; ++face, point += facet_size) {
	for (size_t corner = 0; corner < ELEMENTS_PER_VECT; ++corner) {
	    const auto index = read_little_endian<uint32_t>(point + corner * sizeof(uint32_t));
	    if (index >= point_count)
		throw std::runtime_error("FCStd mesh facet references a missing vertex");
	    mesh.faces.push_back(static_cast<int>(index));
	}
    }
    return mesh;
}

static Geometry
write_mesh(struct rt_wdb *writer, const std::string &name, TriangleMesh &mesh, ImportTimings &timings)
{
    if (mesh.faces.empty())
	return {};
    const auto point_count = mesh.vertices.size() / ELEMENTS_PER_VECT;
    const auto face_count = mesh.faces.size() / ELEMENTS_PER_VECT;
    const auto store_started = ImportClock::now();
    const bool closed = !bg_trimesh_solid(static_cast<int>(point_count), static_cast<int>(face_count),
	mesh.vertices.data(), mesh.faces.data(), nullptr);
    const auto primitive = unique_name(writer->dbip, name + ".s");
    if (mk_bot(writer, primitive.c_str(), closed ? RT_BOT_SOLID : RT_BOT_SURFACE,
	    closed ? RT_BOT_CCW : RT_BOT_UNORIENTED, 0, point_count, face_count,
	    mesh.vertices.data(), mesh.faces.data(), nullptr, nullptr))
	throw std::runtime_error("could not write FreeCAD mesh " + primitive);
    Geometry result;
    result.parts.push_back({primitive});
    timings.store += ImportClock::now() - store_started;
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
	const Nodes &nodes, const ArrayLists &lists,
	std::vector<Member> &members, const std::string &id, size_t &unavailable)
{
    if (node.dependencies.empty())
	return;
    const Matrix scale = link_scale(object);
    if (object.expanded_array()) {
	for (const auto &element : node.dependencies) {
	    Member target = nodes.at(element.key).occurrence;
	    if (!target.name.empty()) {
		bn_mat_mul2(scale.data(), target.transform.data());
		members.push_back(std::move(target));
	    }
	}
	return;
    }

    const std::vector<double> *placements = nullptr;
    const std::vector<double> *scales = nullptr;
    if (object.element_count && (!find_array_list(lists, object.array_placements, placements) ||
	    !find_array_list(lists, object.array_scales, scales))) {
	bu_log("FreeCAD: %s is missing an array placement or scale member\n", id.c_str());
	++unavailable;
	return;
    }
    const size_t count = object.element_count ? object.element_count : 1;
    for (size_t index = 0; index < count; ++index) {
	if (object.element_count && !object.element_visible(index))
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
	for (const auto &reference : node.dependencies) {
	    const auto &target = nodes.at(reference.key).occurrence;
	    if (target.name.empty())
		continue;
	    Matrix target_transform = reference.prefix;
	    if (reference.use_placement)
		bn_mat_mul(target_transform.data(), reference.prefix.data(), target.transform.data());
	    Member member{target.name};
	    bn_mat_mul4(member.transform.data(), scale.data(), element_placement.data(),
		element_scale.data(), target_transform.data());
	    members.push_back(std::move(member));
	}
    }
}

struct ArchiveRequests {
    std::map<std::string, std::vector<ObjectKey>> geometry;
    std::map<std::string, size_t> array_fields;
    ArrayLists array_lists;
    std::map<std::string, bool> appearance_fields;
    AppearanceLists appearances;

    const Appearances &appearance(const Object &object) const
    {
	static const Appearances empty;
	const auto found = appearances.find(appearance_member(object));
	return found == appearances.end() ? empty : found->second;
    }

    void request_list(const std::string &file, size_t components)
    {
	if (file.empty())
	    return;
	const auto result = array_fields.emplace(file, components);
	if (result.first->second != components)
	    throw std::runtime_error("conflicting FCStd link array member " + file);
    }
};

static bool
import_document(struct rt_wdb *writer, const gcv_opts *options, const char *source_path,
	bool strict, const Snapshots &snapshots)
{
    const auto started = ImportClock::now();
    DocumentSet documents(source_path, snapshots);
    const auto &document = documents.sources.front().document;
    std::set<std::string> group_members;
    for (const auto &entry : document.objects) {
	const auto &object = entry.second;
	group_members.insert(object.children.begin(), object.children.end());
	if (object.expanded_array())
	    group_members.insert(object.array_elements.begin(), object.array_elements.end());
    }
    std::vector<ObjectKey> roots;
    for (const auto &entry : document.objects)
	if (entry.second.visible && !entry.second.suppressed && !group_members.count(entry.first))
	    roots.push_back({0, entry.first});

    Nodes nodes;
    size_t unavailable = 0;
    const auto order = order_nodes(documents, roots, nodes, unavailable);
    if (strict && unavailable) {
	bu_log("FreeCAD: strict import rejected %zu unavailable references\n", unavailable);
	return false;
    }
    std::vector<ArchiveRequests> archives(documents.sources.size());
    for (const auto &key : order) {
	const auto &object = documents.object(key);
	auto &requests = archives[key.archive];
	const auto appearance = appearance_member(object);
	if (!appearance.empty() && (!object.has_link() || object.override_material))
	    requests.appearance_fields.emplace(appearance, !object.appearance_file.empty());
	if (!object.has_link()) {
	    const auto &asset = object.shape_file.empty() ? object.mesh_file : object.shape_file;
	    if (!asset.empty() && (key.selection.empty() || !object.shape_file.empty()))
		requests.geometry[asset].push_back(key);
	    else if (!key.selection.empty()) {
		bu_log("FreeCAD: %s has no saved shape for %s\n", key.id.c_str(), key.selection.c_str());
		++unavailable;
	    }
	}
	if (object.element_count && !object.expanded_array() && !nodes.at(key).dependencies.empty()) {
	    requests.request_list(object.array_placements, placement_components);
	    requests.request_list(object.array_scales, scale_components);
	}
    }
    size_t wanted = 0;
    for (const auto &requests : archives)
	wanted += requests.geometry.size();
    size_t processed = 0;
    size_t written = 0;
    size_t meshes = 0;
    size_t invalid_parts = 0;
    size_t nonmanifold_parts = 0;
    std::map<ObjectKey, Geometry> geometry;
    ImportTimings timings;
    constexpr size_t progress_threshold = 128;
    constexpr auto report_interval = std::chrono::seconds(15);
    const bool report_progress = wanted >= progress_threshold || options->verbosity_level;
    auto last_report = ImportClock::now();
    if (report_progress)
	bu_log("FreeCAD: reading %zu geometry members from %zu documents (metadata %.1fs)\n",
	    wanted, archives.size(), seconds(last_report - started));
    for (size_t source = 0; source < archives.size(); ++source) {
	auto &requests = archives[source];
	requests.appearances = read_appearances(documents.sources[source], requests.appearance_fields);
	if (requests.geometry.empty() && requests.array_fields.empty())
	    continue;
	Archive archive(documents.sources[source].archive.c_str());
	const double tolerance = options->calculational_tolerance.dist;
	std::set<std::string> seen;
	std::string member;
	while (archive.next(member)) {
	    const auto requested = requests.geometry.find(member);
	    const auto array = requests.array_fields.find(member);
	    if (array != requests.array_fields.end()) {
		if (requested != requests.geometry.end() || requests.array_lists.count(member))
		    throw std::runtime_error("duplicate or conflicting FCStd array member " + member);
		requests.array_lists.emplace(member, read_array_list(archive, array->second));
		continue;
	    }
	    if (requested == requests.geometry.end())
		continue;
	    if (!seen.insert(member).second)
		throw std::runtime_error("duplicate FCStd geometry member " + member);
	    const auto &first = requested->second.front();
	    const bool mesh = documents.object(first).shape_file.empty();
	    TopoDS_Shape shape;
	    TriangleMesh triangles;
	    try {
		if (mesh) {
		    const auto decode_started = ImportClock::now();
		    triangles = read_mesh(archive);
		    timings.decode += ImportClock::now() - decode_started;
		} else {
		    shape = read_shape(archive, member, timings);
		}
	    } catch (const Standard_Failure &failure) {
		const char *message = occt_failure_message(failure);
		bu_log("FreeCAD: %s: %s\n", member.c_str(), message ? message : "shape read failed");
		++invalid_parts;
		continue;
	    } catch (const std::runtime_error &failure) {
		bu_log("FreeCAD: %s: %s\n", member.c_str(), failure.what());
		++invalid_parts;
		continue;
	    }

	    std::map<std::pair<std::string, std::string>, const Geometry *> translated;
	    for (const auto &key : requested->second) {
		const auto &object = documents.object(key);
		const auto &appearance = requests.appearance(object);
		const auto variant = std::make_pair(key.selection,
		    !mesh && appearance.size() > 1 ? appearance_member(object) : std::string());
		const auto reused = translated.find(variant);
		if (reused != translated.end()) {
		    geometry.emplace(key, *reused->second);
		    continue;
		}
		Geometry result = mesh ? write_mesh(writer, first.name(), triangles, timings) :
		    translate_shape(writer, key, shape, tolerance, timings, appearance);
		if (result.parts.empty() && !result.invalid_parts) {
		    bu_log("FreeCAD: %s contains no saved geometry for %s\n", member.c_str(), key.name().c_str());
		    ++result.invalid_parts;
		}
		written += result.parts.size();
		if (mesh)
		    meshes += result.parts.size();
		invalid_parts += result.invalid_parts;
		nonmanifold_parts += result.nonmanifold_parts;
		const auto stored = geometry.emplace(key, std::move(result));
		translated.emplace(variant, &stored.first->second);
	    }
	    ++processed;
	    const auto now = ImportClock::now();
	    if (report_progress && now - last_report >= report_interval) {
		bu_log("FreeCAD: %zu/%zu geometry members, %zu BReps, %zu meshes, %zu rejected parts (%.1fs elapsed, last %s)\n",
		    processed, wanted, written - meshes, meshes, invalid_parts, seconds(now - started),
		    first.name().c_str());
		last_report = now;
	    }
	}
	for (const auto &request : requests.geometry)
	    if (!seen.count(request.first)) {
		++invalid_parts;
		bu_log("FreeCAD: missing archive member %s\n", request.first.c_str());
	    }
    }
    if (!written) {
	bu_log("FreeCAD: no saved geometry could be imported\n");
	return false;
    }

    for (const auto &key : order) {
	const auto &object = documents.object(key);
	Node &node = nodes.at(key);
	std::vector<Member> members;
	bool face_colors = false;
	node.occurrence.transform = object_placement(object);
	const bool linked = object.has_link();
	if (linked) {
	    const auto &lists = archives[key.archive].array_lists;
	    write_link_members(object, node, nodes, lists, members, key.id, unavailable);
	} else {
	    const auto shape = geometry.find(key);
	    if (shape != geometry.end() && !shape->second.parts.empty()) {
		face_colors = shape->second.face_colors;
		if (object.shape_file.empty())
		    members = shape->second.parts;
		else
		    place_shape(object, shape->second, node.occurrence, members);
	    }
	    for (const auto &child : node.dependencies) {
		const Member &occurrence = nodes.at(child.key).occurrence;
		if (!occurrence.name.empty())
		    members.push_back(occurrence);
	    }
	}
	if (members.empty())
	    continue;
	node.occurrence.name = unique_name(writer->dbip, key.name());
	Properties properties = object.properties;
	properties["name"] = key.id;
	properties["type"] = object.type;
	properties["visible"] = object.visible ? "true" : "false";
	if (!key.selection.empty())
	    properties["subobject"] = key.selection;
	if (key.archive)
	    properties["document"] = std::filesystem::u8path(documents.sources[key.archive].path).filename().u8string();
	if (linked) {
	    properties["linked_object"] = object.linked_object;
	    if (!object.linked_file.empty())
		properties["linked_file"] = object.linked_file;
	}
	auto color = object.material_color && (!linked || object.override_material)
	    ? object.material_color : object.color;
	auto transparency = linked && !object.override_material ? std::nullopt
	    : (object.material_transparency ? object.material_transparency : object.transparency);
	const auto &appearance = archives[key.archive].appearance(object);
	if (!appearance.empty() && (!linked || object.override_material)) {
	    color = appearance.front().color;
	    if (!object.appearance_file.empty() || !transparency || appearance.size() > 1)
		transparency = appearance.front().transparency;
	}
	if (linked && object.override_material && color && !transparency)
	    transparency = 0.0;
	write_combination(writer, node.occurrence.name, members, properties,
	    color ? color->data() : nullptr, linked, transparency, face_colors);
    }
    if (unavailable || invalid_parts) {
	bu_log("FreeCAD: %s (%zu unavailable references, %zu rejected parts)\n",
	    strict ? "strict import rejected" : "partial import", unavailable, invalid_parts);
	if (strict)
	    return false;
    }
    std::vector<Member> members;
    for (const auto &key : roots) {
	const Member &occurrence = nodes.at(key).occurrence;
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
    write_combination(writer, root_name, members, document.properties);
    if (report_progress)
	bu_log("FreeCAD: %zu BReps, %zu meshes from %zu archive members, %zu rejected parts, %zu source nonmanifold parts, %zu assembly roots (%.1fs read, %.1fs translate, %.1fs store, %.1fs reader total)\n",
	    written - meshes, meshes, processed, invalid_parts, nonmanifold_parts, members.size(), seconds(timings.decode),
	    seconds(timings.translate), seconds(timings.store), seconds(ImportClock::now() - started));
    return true;
}

class RecomputedDocuments {
public:
    ~RecomputedDocuments()
    {
	if (!folder.empty()) {
	    std::error_code error;
	    std::filesystem::remove_all(folder, error);
	    if (error)
		bu_log("FreeCAD: could not remove recompute workspace: %s\n", error.message().c_str());
	}
    }

    Snapshots run(const char *python, const char *source_path)
    {
	char temporary[MAXPATHLEN];
	FILE *reservation = bu_temp_file(temporary, sizeof(temporary));
	if (!reservation)
	    throw std::runtime_error("could not reserve a FreeCAD recompute workspace");
	std::fclose(reservation);
	auto directory = std::filesystem::u8path(temporary);
	directory += ".d";
	std::error_code error;
	const bool created = std::filesystem::create_directory(directory, error);
	bu_file_delete(temporary);
	if (!created)
	    throw std::runtime_error("could not create FreeCAD recompute workspace");
	folder = std::move(directory);
#ifndef _WIN32
	std::filesystem::permissions(folder, std::filesystem::perms::owner_all);
#endif
	const auto path = normalized_path(std::filesystem::u8path(source_path));
	const auto input = path.u8string();
	const auto output = folder.u8string();

	// Force stale as well as touched features. FreeCAD owns feature
	// evaluation and dependency order, including those in linked documents.
	static constexpr char program[] = R"PY(
import sys
from pathlib import Path
import FreeCAD

root = FreeCAD.openDocument(sys.argv[1])
out = Path(sys.argv[2])
with (out / 'sources').open('wb') as manifest:
    for index, doc in enumerate(root.getDependentDocuments()):
        print('FreeCAD: recomputing ' + doc.Name, flush=True)
        for obj in doc.Objects:
            obj.enforceRecompute()
        count = doc.recompute(None, True)
        failed = [obj.Name for obj in doc.Objects if 'Invalid' in obj.State]
        if failed:
            raise RuntimeError(doc.Name + ': invalid features: ' + ', '.join(failed))
        print('FreeCAD: recomputed ' + str(count) + ' features in ' + doc.Name, flush=True)
        doc.saveCopy(str(out / (str(index) + '.FCStd')))
        manifest.write(doc.FileName.encode('utf-8') + b'\0')
)PY";
	const char *args[] = {python, "-c", program, input.c_str(), output.c_str(), nullptr};
	struct bu_process *process = nullptr;
	bu_process_create(&process, args, BU_PROCESS_OUT_EQ_ERR | BU_PROCESS_HIDE_WINDOW);
	if (!process)
	    throw std::runtime_error("could not launch FreeCAD's Python runtime");
	bu_process_file_close(process, BU_PROCESS_STDIN);
	if (FILE *diagnostic = bu_process_file_open(process, BU_PROCESS_STDOUT)) {
	    std::array<char, 4096> line;
	    while (bu_fgets(line.data(), line.size(), diagnostic))
		bu_log("%s", line.data());
	}
	if (bu_process_wait_n(&process, 0))
	    throw std::runtime_error("FreeCAD feature recomputation failed");

	std::ifstream manifest(folder / "sources", std::ios::binary);
	Snapshots snapshots;
	size_t index = 0;
	std::string origin;
	while (std::getline(manifest, origin, '\0')) {
	    if (origin.empty())
		throw std::runtime_error("invalid FreeCAD recompute snapshot source");
	    const auto archive = folder / (std::to_string(index++) + ".FCStd");
	    if (!std::filesystem::is_regular_file(archive))
		throw std::runtime_error("missing FreeCAD recompute snapshot");
	    snapshots.emplace(normalized_path(std::filesystem::u8path(origin)), archive.u8string());
	}
	if (!snapshots.count(path))
	    throw std::runtime_error("FreeCAD recompute did not save the requested document");
	return snapshots;
    }

private:
    std::filesystem::path folder;
};

struct ReaderOptions {
    int strict = 0;
    const char *python = nullptr;
};

static void
create_options(struct bu_opt_desc **descriptions, void **data)
{
    auto reader_options = std::make_unique<ReaderOptions>();
    const struct bu_opt_desc options[] = {
	{"", "strict", "", nullptr, &reader_options->strict, "reject partial geometry imports"},
	{"", "recompute", "PYTHON", bu_opt_str, &reader_options->python,
	    "recompute trusted documents using a Python executable that can import FreeCAD"},
	BU_OPT_DESC_NULL
    };
    *descriptions = static_cast<struct bu_opt_desc *>(bu_malloc(sizeof(options), "FreeCAD reader options"));
    std::copy_n(options, sizeof(options) / sizeof(options[0]), *descriptions);
    *data = reader_options.release();
}

static void
free_options(void *data)
{
    delete static_cast<ReaderOptions *>(data);
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
	const auto reader_options = data ? *static_cast<const ReaderOptions *>(data) : ReaderOptions{};
	RecomputedDocuments recomputed;
	const auto snapshots = reader_options.python ? recomputed.run(reader_options.python, path) : Snapshots{};
	return import_document(writer, options, path, reader_options.strict, snapshots);
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
