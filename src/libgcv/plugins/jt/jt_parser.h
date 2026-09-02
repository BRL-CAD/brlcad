/*                         J T _ P A R S E R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef LIBGCV_JT_PARSER_H
#define LIBGCV_JT_PARSER_H

#include "common.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace jt {

using Guid = std::array<unsigned char, 16>;
using ByteBuffer = std::vector<unsigned char>;

/* Length in bytes of the ASCII version string at the start of a JT file header. */
constexpr size_t VERSION_LENGTH = 80;

/* EXP-047: partial-file marker.  When a JT export write is truncated, the writer
 * stamps JT_INCOMPLETE_MARKER at byte JT_INCOMPLETE_MARKER_OFFSET of the version
 * signature (past the "Version N.N ..." prefix the version regex needs) so the
 * reader can positively reject a partial file. */
constexpr long JT_INCOMPLETE_MARKER_OFFSET = 40;
constexpr char JT_INCOMPLETE_MARKER[] = "*INCOMPLETE*";

/* GUID of the JT Tri-Strip Set Shape LOD Element, stored little-endian.  Shared
 * by the parser (element classification) and the writer (synthetic files). */
constexpr std::array<unsigned char, 16> jt_tristrip_guid = {
    {0xab, 0x10, 0xdd, 0x10, 0xc8, 0x2a, 0xd1, 0x11,
     0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97}};

/* Centralised JT shape/element GUID constants (JT File Format Reference).  All of
 * the mesh Shape LOD GUIDs share the trailing eight bytes JT_GUID_TAIL and the
 * second/third fields JT_SHAPE_GUID_D2/D3, differing only in the first field:
 * Tri-Strip Set, Polygon Set, and (JT 9 topological) Tri-Indexed Set.  Grouping
 * them here keeps the format knowledge out of the parser body. */
namespace guid {
/* Common trailing eight bytes shared by every "...-2ac8-11d1-9b6b-0080c7bb5997"
 * JT shape/element GUID. */
constexpr std::array<unsigned char, 8> JT_GUID_TAIL = {
    {0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97}};
/* First three GUID fields of the JT mesh Shape LODs. */
constexpr uint32_t JT_TRISTRIP_GUID_D1 = 0x10dd10ab;
constexpr uint32_t JT_POLYGON_GUID_D1 = 0x10dd109f;
/* First GUID field of the JT 8 Null Shape LOD Element (empty geometry). */
constexpr uint32_t JT_NULLSHAPE_GUID_D1 = 0x10dd10ee;
constexpr uint16_t JT_SHAPE_GUID_D2 = 0x2ac8;
constexpr uint16_t JT_SHAPE_GUID_D3 = 0x11d1;
} // namespace guid

/* The kind of mesh a shape-element GUID identifies (JT 8 legacy shapes and the
 * JT 9/10 topologically compressed rep reuse the same GUIDs).  Null is a JT 8
 * Null Shape LOD: a recognised legacy shape that carries no geometry. */
enum class ShapeKind { None, TriStrip, Polygon, Null };

/* Central point for JT GUID pattern matching and shape-type identification.  DRYs
 * the JT_GUID_TAIL / D2 / D3 constants that the individual element checks used to
 * repeat and keeps all GUID knowledge in one testable place. */
class GuidRegistry {
  public:
    /* True if guid equals the little-/big-endian encoding of the GUID whose first
     * three fields are (first, second, third) and whose trailing bytes are tail. */
    static bool matches(const Guid &guid, bool little_endian, uint32_t first, uint16_t second,
	uint16_t third, const std::array<unsigned char, 8> &tail)
    {
	std::array<unsigned char, 16> expected{};
	size_t k = 0;
	for (size_t i = 0; i < 4; ++i) expected[k++] = static_cast<unsigned char>(first >> ((little_endian ? i : 3 - i) * 8));
	for (size_t i = 0; i < 2; ++i) expected[k++] = static_cast<unsigned char>(second >> ((little_endian ? i : 1 - i) * 8));
	for (size_t i = 0; i < 2; ++i) expected[k++] = static_cast<unsigned char>(third >> ((little_endian ? i : 1 - i) * 8));
	for (unsigned char b : tail) expected[k++] = b;
	return std::equal(expected.begin(), expected.end(), guid.begin());
    }

    /* Classify an element type GUID as a Tri-Strip Set, Polygon Set, or neither. */
    static ShapeKind mesh_shape_kind(const Guid &type_id, bool little_endian)
    {
	if (matches(type_id, little_endian, guid::JT_TRISTRIP_GUID_D1, guid::JT_SHAPE_GUID_D2, guid::JT_SHAPE_GUID_D3, guid::JT_GUID_TAIL))
	    return ShapeKind::TriStrip;
	if (matches(type_id, little_endian, guid::JT_POLYGON_GUID_D1, guid::JT_SHAPE_GUID_D2, guid::JT_SHAPE_GUID_D3, guid::JT_GUID_TAIL))
	    return ShapeKind::Polygon;
	if (matches(type_id, little_endian, guid::JT_NULLSHAPE_GUID_D1, guid::JT_SHAPE_GUID_D2, guid::JT_SHAPE_GUID_D3, guid::JT_GUID_TAIL))
	    return ShapeKind::Null;
	return ShapeKind::None;
    }
};

/* Overflow-safe range check for a [offset, offset+length) window against a buffer
 * of a fixed size.  Construct once with the buffer size, then call check() (or
 * the call operator) per candidate range; centralises the wraparound-safe
 * arithmetic the parser used to open-code at every bounds test. */
class RangeValidator {
  public:
    explicit RangeValidator(size_t size) : size_(size) {}

    bool check(uint64_t offset, uint64_t length) const
    {
	/* Compare length against the remaining space via subtraction rather than
	 * forming the potentially-overflowing offset + length: offset <= size_
	 * guarantees size_ - offset does not underflow. */
	if (offset > size_) return false;
	return length <= size_ - static_cast<size_t>(offset);
    }
    bool operator()(uint64_t offset, uint64_t length) const { return check(offset, length); }

  private:
    size_t size_;
};

struct TocEntry {
    Guid id;
    uint64_t offset;
    uint32_t length;
    uint32_t attributes;

    uint8_t type() const { return static_cast<uint8_t>(attributes >> 24); }
};

/* Named JT file-format capability flags derived from the major version, replacing
 * scattered magic comparisons like 'major_version >= 10'.  Build one from a
 * Header (or major version) and query the intent-revealing accessors. */
struct FileFormatVersion {
    unsigned major = 0;

    FileFormatVersion() = default;
    explicit FileFormatVersion(unsigned major_version) : major(major_version) {}

    /* JT 10+ widened the TOC/segment file offsets from 32 to 64 bits. */
    bool has_wide_offsets() const { return major >= 10; }
    /* JT 9+ logical elements carry a 4-byte object id after the element header. */
    bool has_object_id() const { return major >= 9; }
    /* JT 10 introduced the nibble-based ("chopper") Bitlength2 CODEC variant. */
    bool jt10_chopper() const { return major >= 10; }
    /* JT 8 legacy mesh Shape LODs vs the JT 9/10 topologically compressed rep. */
    bool is_legacy() const { return major == 8; }
    bool is_topological() const { return major >= 9; }
    /* Int32 probability-context block uses the JT 8 (<9.0) two-table layout. */
    bool two_table_prob_layout() const { return major < 9; }
};

struct Header {
    std::string version_text;
    unsigned major_version;
    unsigned minor_version;
    bool little_endian;
    uint64_t toc_offset;
    /* Present only when the header's reserved field flags an extended header;
     * holds the reserved GUID that follows.  nullopt when absent. */
    std::optional<Guid> version_extended_header;
    /* The LSG (Logical Scene Graph) root segment GUID.  nullopt when the header
     * carries the all-zero "no scene graph" sentinel. */
    std::optional<Guid> lsg_segment_id;

    FileFormatVersion version() const { return FileFormatVersion(major_version); }
};

struct Segment {
    Guid id;
    uint64_t offset;
    uint32_t length;
    uint32_t type;
};

struct Element {
    Guid type_id;
    uint8_t base_type;
    int32_t object_id;
    uint64_t data_offset;
    uint32_t data_length;
    /* Level-of-detail rank inferred from element ordering within a shape
     * segment: the first decoded element is the finest LOD (0), the next 1, and
     * so on.  Preserved so the importer can record which LOD a mesh represents. */
    uint8_t lod_level = 0;
};

/* Per-axis quantization parameters recovered from a topological/point mesh
 * coordinate record: the [minimum, maximum] range and bit count used to encode
 * the coordinate (bits == 0 meaning a lossless float codec).  Mirrors the parser's
 * internal Quantizer so the importer can preserve the source precision. */
struct MeshQuantizer {
    float minimum = 0.0f;
    float maximum = 0.0f;
    uint8_t bits = 0;
};

/* Axis-aligned bounding box of a decoded mesh, in the mesh's own coordinate
 * space (before any import transform is applied).  valid is false until a mesh
 * has been scanned; when true, min[c] <= max[c] for every axis. */
struct BoundingBox {
    bool valid = false;
    double min[3] = {0.0, 0.0, 0.0};
    double max[3] = {0.0, 0.0, 0.0};

    /* Expand the box to include one XYZ point, initialising it on first use. */
    void include(double x, double y, double z)
    {
	const double p[3] = {x, y, z};
	if (!valid) {
	    for (int c = 0; c < 3; ++c) min[c] = max[c] = p[c];
	    valid = true;
	    return;
	}
	for (int c = 0; c < 3; ++c) {
	    if (p[c] < min[c]) min[c] = p[c];
	    if (p[c] > max[c]) max[c] = p[c];
	}
    }

    /* Scan a flat XYZ-triple coordinate array (as stored in Mesh::vertices) and
     * return the enclosing box (invalid when the array holds no whole triple). */
    static BoundingBox from_vertices(const std::vector<double> &vertices)
    {
	BoundingBox box;
	for (size_t i = 0; i + 2 < vertices.size(); i += 3)
	    box.include(vertices[i], vertices[i + 1], vertices[i + 2]);
	return box;
    }
};

struct Mesh {
    std::vector<double> vertices;
    std::vector<int> faces;
    /* Optional per-vertex unit normals, parallel to vertices (three doubles per
     * vertex).  Empty when the source carried no normals or they were not
     * decoded; when populated, normals.size() == vertices.size(). */
    std::vector<double> normals;

    /* Provenance carried over from the source JT logical element so the importer
     * can attach it as BRL-CAD attributes: the element type GUID, its object id
     * (-1 when the JT version has no header object id), and the LOD rank. */
    Guid element_type_id{};
    int32_t element_object_id = -1;
    uint8_t lod_level = 0;
    /* The source JT logical element base type (4 == legacy tri-strip/polygon
     * Shape LOD, 9 == JT 9/10 topologically compressed rep), preserved so the
     * importer can record which representation a mesh came from. */
    uint8_t element_base_type = 0;

    /* Axis-aligned bounding box computed from the decoded vertex array (invalid
     * until a mesh with vertices has been decoded). */
    BoundingBox bbox;

    /* GEO-028: the sorted set of distinct vertex/polygon group IDs recovered from
     * a topological mesh (TopologyMesh::polygon_groups).  Empty for legacy meshes
     * and for topological meshes that carried no grouping.  The importer records
     * these as jt_group_N attributes so part-grouping information survives import. */
    std::vector<int32_t> polygon_groups;

    /* Quantization metadata recovered from a topological mesh coordinate record.
     * has_quantizers is false for legacy meshes (which do not carry per-axis
     * quantizers); vertex_binding_mode records the raw vertex binding word. */
    bool has_quantizers = false;
    MeshQuantizer quantizers[3];
    uint64_t vertex_binding_mode = 0;

    /* Bounds-checked access to the flat face-index array.  In debug builds an
     * out-of-range index trips an assertion; in release builds this compiles to
     * the same unchecked vector access as faces[index]. */
    int &operator[](size_t index)
    {
	assert(index < faces.size());
	return faces[index];
    }
    int operator[](size_t index) const
    {
	assert(index < faces.size());
	return faces[index];
    }

    /* Check the structural invariants of a decoded mesh: the coordinate array
     * must be whole XYZ triples, the index array whole triangles, and every
     * face index must reference an existing vertex.  Returns true when the mesh
     * is well-formed (an empty mesh is trivially valid). */
    bool validate() const
    {
	if (vertices.size() % 3 != 0 || faces.size() % 3 != 0)
	    return false;
	const size_t vertex_count = vertices.size() / 3;
	for (int index : faces)
	    if (index < 0 || static_cast<size_t>(index) >= vertex_count)
		return false;
	return true;
    }
};

/* A composable rigid-plus-scale transform applied to a decoded Mesh in place.
 * Rotation is stored as a row-major 3x3 matrix (identity by default) so several
 * import-time adjustments (unit scale, axis rotation, recentering) can be folded
 * into one pass over the vertices instead of re-tessellating.  The order applied
 * per vertex is: scale, then rotate, then translate. */
class MeshTransform {
  public:
    double scale = 1.0;
    /* row-major 3x3 rotation */
    std::array<double, 9> rotation{{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    std::array<double, 3> translation{{0, 0, 0}};

    void apply(Mesh &mesh) const
    {
	for (size_t i = 0; i + 2 < mesh.vertices.size(); i += 3) {
	    const double x = mesh.vertices[i] * scale;
	    const double y = mesh.vertices[i + 1] * scale;
	    const double z = mesh.vertices[i + 2] * scale;
	    mesh.vertices[i] = rotation[0] * x + rotation[1] * y + rotation[2] * z + translation[0];
	    mesh.vertices[i + 1] = rotation[3] * x + rotation[4] * y + rotation[5] * z + translation[1];
	    mesh.vertices[i + 2] = rotation[6] * x + rotation[7] * y + rotation[8] * z + translation[2];
	}
	/* Normals are direction vectors: rotate them but never scale or translate,
	 * so a non-identity rotation keeps the per-vertex shading normals aligned. */
	for (size_t i = 0; i + 2 < mesh.normals.size(); i += 3) {
	    const double x = mesh.normals[i];
	    const double y = mesh.normals[i + 1];
	    const double z = mesh.normals[i + 2];
	    mesh.normals[i] = rotation[0] * x + rotation[1] * y + rotation[2] * z;
	    mesh.normals[i + 1] = rotation[3] * x + rotation[4] * y + rotation[5] * z;
	    mesh.normals[i + 2] = rotation[6] * x + rotation[7] * y + rotation[8] * z;
	}
    }
};

/* One leaf shape reachable through the Logical Scene Graph, with its accumulated
 * placement, resolved geometry segment, and (optional) material colour. */
struct SceneInstance {
    std::string name;
    double matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; /* row-major (BRL-CAD) */
    Guid segment_id{};   /* Shape LOD segment GUID to decode for this instance */
    bool has_color = false;
    float color[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    /* LSG node visibility: true unless an ancestor/this node carried the
     * "invisible" node flag bit.  Preserved as a 'jt_visible' attribute. */
    bool visible = true;
    /* JT string properties inherited down to this part (name, part number, etc.),
     * suitable for attaching as BRL-CAD attributes. */
    std::vector<std::pair<std::string, std::string>> properties;
    /* Node ID referenced by the nearest ancestor Instance node (the Part/child
     * this instance points at), or -1 if this shape was not reached through an
     * Instance.  Preserved as a 'jt_referenced_part_id' attribute for
     * traceability of LSG part references. */
    int32_t referenced_part_id = -1;
};

enum class Predictor {
    Lag1, Lag2, Stride1, Stride2, StripIndex, Ramp, Xor1, Xor2, None
};

class File {
  public:
    [[nodiscard]] bool load(const char *path, std::string &error);
    [[nodiscard]] bool load(const ByteBuffer &bytes, std::string &error);
    [[nodiscard]] bool segment(const TocEntry &entry, Segment &result, std::string &error) const;
    [[nodiscard]] bool elements(const Segment &segment, std::vector<Element> &result, std::string &error) const;
    [[nodiscard]] bool int32_packet(uint64_t offset, Predictor predictor, std::vector<int32_t> &values,
	size_t &bytes_read, std::string &error) const;
    [[nodiscard]] bool int32_packet2(uint64_t offset, Predictor predictor, std::vector<int32_t> &values,
	size_t &bytes_read, std::string &error) const;
    [[nodiscard]] bool is_legacy_mesh(const Element &element) const;
    [[nodiscard]] bool legacy_mesh(const Element &element, Mesh &mesh, std::string &error) const;
    [[nodiscard]] bool is_topological_mesh(const Element &element) const;
    [[nodiscard]] bool topological_mesh(const Element &element, Mesh &mesh, std::string &error) const;

    /* Parse the Logical Scene Graph (segment type 1) and return the flat set of
     * leaf shape instances with accumulated transforms, names, and colours.
     * Returns false (with error set) if there is no usable LSG; callers then
     * fall back to a flat scan of the shape segments. */
    bool scene_graph(std::vector<SceneInstance> &instances, std::string &error) const;
    /* Look up a TOC entry by its segment GUID (used to resolve LSG shape nodes). */
    const TocEntry *toc_entry_by_id(const Guid &id) const;

    const Header &header() const { return header_; }
    const std::vector<TocEntry> &toc() const { return toc_; }

    /* J8-040: strict element-stream termination.  By default elements() accepts a
     * shape segment whose element list ends by simply running out of bytes (some
     * writers omit the explicit all-0xFF end-of-elements marker required by the
     * JT File Format Reference, Section 9.3.3).  Enabling strict mode requires the
     * explicit marker and rejects a segment that ends on padding alone. */
    void set_strict_end_of_segment(bool strict) { strict_end_of_segment_ = strict; }
    bool strict_end_of_segment() const { return strict_end_of_segment_; }

  private:
    Header header_{};
    std::vector<TocEntry> toc_;
    ByteBuffer bytes_;
    bool strict_end_of_segment_ = false;
};

bool has_jt_signature(const char *path);

} // namespace jt

#endif
