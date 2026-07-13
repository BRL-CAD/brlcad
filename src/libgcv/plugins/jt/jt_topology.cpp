/*                       J T _ T O P O L O G Y . C P P
 * BRL-CAD
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "jt_topology.h"

#include <algorithm>
#include <limits>
#include <sstream>

namespace jt {
namespace {

constexpr int32_t kSplitSymbol = 0;
constexpr size_t kActiveSearchWidth = 16;
constexpr size_t kMaxEntities = 100000000;

struct Vertex {
    int32_t group = -1;
    uint16_t flags = 0;
    std::vector<int32_t> faces;
};

struct Face {
    std::vector<int32_t> vertices;
    std::vector<int32_t> attributes;
    std::vector<bool> attribute_mask;
    size_t empty = 0;
};

class Decoder {
  public:
    Decoder(const TopologySymbols &symbols, std::string &error) : s(symbols), err(error) {}

    bool run(TopologyMesh &result)
    {
        if (s.vertex_valences.size() != s.vertex_groups.size() ||
            s.vertex_valences.size() != s.vertex_flags.size())
            return fail("JT topology vertex symbol arrays have inconsistent lengths");
        if (s.vertex_valences.size() > kMaxEntities)
            return fail("JT topology exceeds the vertex safety limit");

        while (vertex_pos < s.vertex_valences.size()) {
            int32_t seed = new_vertex();
            if (seed < 0)
                return false;
            for (size_t slot = 0; slot < vertices[seed].faces.size(); ++slot) {
                if (activate_face(seed, slot) < -1)
                    return false;
            }
            for (;;) {
                int32_t face = next_active_face();
                if (face < 0)
                    break;
                while (faces[face].empty) {
                    auto it = std::find(faces[face].vertices.begin(), faces[face].vertices.end(), -1);
                    if (it == faces[face].vertices.end())
                        return fail("JT topology face empty-slot count is inconsistent");
                    size_t slot = static_cast<size_t>(it - faces[face].vertices.begin());
                    int32_t vertex = activate_vertex(face, slot);
                    if (vertex < 0 || !complete_vertex(vertex, slot))
                        return false;
                }
                removed[face] = true;
            }
        }
        if (!all_consumed())
            return fail("JT topology contains unused symbols");
        return export_mesh(result);
    }

  private:
    bool fail(const char *message) { err = message; return false; }
    static size_t inc(size_t n, size_t modulus) { return (n + 1) % modulus; }
    static size_t dec(size_t n, size_t modulus) { return n ? n - 1 : modulus - 1; }

    int32_t new_vertex()
    {
        if (vertex_pos >= s.vertex_valences.size())
            return -1;
        int32_t valence = s.vertex_valences[vertex_pos];
        if (valence < 2 || static_cast<size_t>(valence) > kMaxEntities) {
            fail("JT topology contains an invalid vertex valence");
            return -1;
        }
        Vertex vertex;
        vertex.group = s.vertex_groups[vertex_pos];
        int32_t flag = s.vertex_flags[vertex_pos++];
        if (flag < 0 || flag > std::numeric_limits<uint16_t>::max()) {
            fail("JT topology contains invalid vertex flags");
            return -1;
        }
        vertex.flags = static_cast<uint16_t>(flag);
        vertex.faces.assign(static_cast<size_t>(valence), -1);
        vertices.push_back(std::move(vertex));
        return static_cast<int32_t>(vertices.size() - 1);
    }

    int context(int32_t vertex) const
    {
        int valence = static_cast<int>(vertices[vertex].faces.size());
        int known = 0, total = 0;
        for (int32_t face : vertices[vertex].faces) {
            if (face >= 0) { ++known; total += static_cast<int>(faces[face].vertices.size()); }
        }
        if (valence == 3) return total < known * 6 ? 0 : (total == known * 6 ? 1 : 2);
        if (valence == 4) return total < known * 4 ? 3 : (total == known * 4 ? 4 : 5);
        return valence == 5 ? 6 : 7;
    }

    int32_t activate_face(int32_t vertex, size_t vertex_slot)
    {
        int ctx = context(vertex);
        if (degree_pos[ctx] >= s.face_degrees[ctx].size()) {
            std::ostringstream message;
            message << "JT topology face-degree stream ended early in context " << ctx;
            err = message.str();
            return -2;
        }
        int32_t degree = s.face_degrees[ctx][degree_pos[ctx]++];
        if (degree == kSplitSymbol)
            return connect_split(vertex, vertex_slot);
        if (degree < 2 || static_cast<size_t>(degree) > kMaxEntities) {
            fail("JT topology contains an invalid face degree");
            return -2;
        }

        int mask_ctx = std::min(7, std::max(0, degree - 2));
        Face face;
        face.vertices.assign(static_cast<size_t>(degree), -1);
        face.attribute_mask.assign(static_cast<size_t>(degree), false);
        face.empty = static_cast<size_t>(degree);
        if (degree <= 64) {
            if (mask_pos[mask_ctx] >= s.face_attribute_masks[mask_ctx].size()) {
                fail("JT topology attribute-mask stream ended early");
                return -2;
            }
            uint64_t mask = s.face_attribute_masks[mask_ctx][mask_pos[mask_ctx]++];
            if (degree < 64 && (mask >> degree)) {
                fail("JT topology attribute mask exceeds its face degree");
                return -2;
            }
            for (int i = 0; i < degree; ++i) face.attribute_mask[i] = (mask >> i) & 1;
        } else {
            size_t words = (static_cast<size_t>(degree) + 31) / 32;
            if (high_mask_pos > s.high_degree_attribute_masks.size() ||
                words > s.high_degree_attribute_masks.size() - high_mask_pos) {
                fail("JT topology high-degree attribute-mask stream ended early");
                return -2;
            }
            for (int i = 0; i < degree; ++i)
                face.attribute_mask[i] = (s.high_degree_attribute_masks[high_mask_pos + i / 32] >> (i % 32)) & 1;
            high_mask_pos += words;
        }
        int32_t index = static_cast<int32_t>(faces.size());
        for (bool present : face.attribute_mask)
            if (present) face.attributes.push_back(attribute_counter++);
        faces.push_back(std::move(face));
        removed.push_back(false);
        vertices[vertex].faces[vertex_slot] = index;
        set_face_vertex(index, 0, vertex);
        active.push_back(index);
        return index;
    }

    int32_t connect_split(int32_t vertex, size_t vertex_slot)
    {
        if (split_face_pos >= s.split_face_offsets.size() || split_slot_pos >= s.split_face_positions.size()) {
            fail("JT topology split stream ended early");
            return -2;
        }
        int32_t offset = s.split_face_offsets[split_face_pos++];
        int32_t slot = s.split_face_positions[split_slot_pos++];
        if (offset <= 0 || static_cast<size_t>(offset) > active.size()) {
            fail("JT topology split face offset is out of range");
            return -2;
        }
        int32_t face = active[active.size() - static_cast<size_t>(offset)];
        if (face < 0 || removed[face] || slot < 0 || static_cast<size_t>(slot) >= faces[face].vertices.size()) {
            std::ostringstream message;
            message << "JT topology split face position " << slot << " is out of range for face " << face
                    << " degree " << (face >= 0 ? faces[face].vertices.size() : 0)
                    << " (offset " << offset << ", active " << active.size() << ", split "
                    << split_slot_pos << '/' << s.split_face_positions.size() << ')';
            err = message.str();
            return -2;
        }
        vertices[vertex].faces[vertex_slot] = face;
        add_vertex_to_face(vertex, vertex_slot, face, static_cast<size_t>(slot));
        return face;
    }

    int32_t activate_vertex(int32_t face, size_t face_slot)
    {
        int32_t vertex = new_vertex();
        if (vertex < 0) { fail("JT topology vertex-valence stream ended early"); return -1; }
        vertices[vertex].faces[0] = face;
        add_vertex_to_face(vertex, 0, face, face_slot);
        return vertex;
    }

    int find_vertex_slot(int32_t face, int32_t vertex) const
    {
        auto it = std::find(faces[face].vertices.begin(), faces[face].vertices.end(), vertex);
        return it == faces[face].vertices.end() ? -1 : static_cast<int>(it - faces[face].vertices.begin());
    }
    int find_face_slot(int32_t vertex, int32_t face) const
    {
        auto it = std::find(vertices[vertex].faces.begin(), vertices[vertex].faces.end(), face);
        return it == vertices[vertex].faces.end() ? -1 : static_cast<int>(it - vertices[vertex].faces.begin());
    }
    void set_face_vertex(int32_t face, size_t slot, int32_t vertex)
    {
        if (faces[face].vertices[slot] == -1) --faces[face].empty;
        faces[face].vertices[slot] = vertex;
    }

    void add_vertex_to_face(int32_t vertex, size_t vertex_face_slot, int32_t face, size_t face_vertex_slot)
    {
        size_t cw = dec(face_vertex_slot, faces[face].vertices.size());
        size_t ccw = inc(face_vertex_slot, faces[face].vertices.size());
        set_face_vertex(face, face_vertex_slot, vertex);
        int32_t previous = faces[face].vertices[cw];
        if (previous >= 0) {
            int pslot = find_face_slot(previous, face);
            size_t next_slot = inc(vertex_face_slot, vertices[vertex].faces.size());
            if (pslot >= 0 && vertices[vertex].faces[next_slot] == -1)
                vertices[vertex].faces[next_slot] = vertices[previous].faces[dec(static_cast<size_t>(pslot), vertices[previous].faces.size())];
        }
        int32_t next = faces[face].vertices[ccw];
        if (next >= 0) {
            int nslot = find_face_slot(next, face);
            size_t previous_slot = dec(vertex_face_slot, vertices[vertex].faces.size());
            if (nslot >= 0 && vertices[vertex].faces[previous_slot] == -1)
                vertices[vertex].faces[previous_slot] = vertices[next].faces[inc(static_cast<size_t>(nslot), vertices[next].faces.size())];
        }
    }

    bool complete_vertex(int32_t vertex, size_t slot_on_face0)
    {
        size_t valence = vertices[vertex].faces.size();
        int32_t previous_face = vertices[vertex].faces[0];
        size_t previous_slot = slot_on_face0;
        size_t i = 1;
        while (i < valence && vertices[vertex].faces[i] != -1) {
            int32_t next_face = vertices[vertex].faces[i];
            previous_slot = dec(previous_slot, faces[previous_face].vertices.size());
            int32_t adjacent = faces[previous_face].vertices[previous_slot];
            if (adjacent < 0) break;
            int slot = find_vertex_slot(next_face, adjacent);
            if (slot < 0) return fail("JT topology has inconsistent adjacent face rings");
            previous_slot = dec(static_cast<size_t>(slot), faces[next_face].vertices.size());
            add_vertex_to_face(vertex, i, next_face, previous_slot);
            previous_face = next_face;
            if (++i == valence) return true;
        }
        size_t first_unknown = i;
        previous_face = vertices[vertex].faces[0];
        previous_slot = slot_on_face0;
        i = valence - 1;
        while (i >= first_unknown && vertices[vertex].faces[i] != -1) {
            int32_t next_face = vertices[vertex].faces[i];
            previous_slot = inc(previous_slot, faces[previous_face].vertices.size());
            int32_t adjacent = faces[previous_face].vertices[previous_slot];
            if (adjacent < 0) break;
            int slot = find_vertex_slot(next_face, adjacent);
            if (slot < 0) return fail("JT topology has inconsistent adjacent face rings");
            previous_slot = inc(static_cast<size_t>(slot), faces[next_face].vertices.size());
            add_vertex_to_face(vertex, i, next_face, previous_slot);
            previous_face = next_face;
            if (i-- == first_unknown) return true;
        }
        for (size_t unresolved = first_unknown; unresolved <= i; ++unresolved)
            if (activate_face(vertex, unresolved) < -1) return false;
        return true;
    }

    int32_t next_active_face()
    {
        while (!active.empty() && removed[active.back()]) active.pop_back();
        int32_t selected = -1;
        size_t lowest = std::numeric_limits<size_t>::max();
        for (size_t i = active.size(); i > 0;) {
	    --i;
	    if (active.size() - i > kActiveSearchWidth) break;
	    int32_t face = active[i];
	    if (removed[face]) {
		active.erase(active.begin() + static_cast<std::ptrdiff_t>(i));
		continue;
	    }
	    if (faces[face].empty < lowest) {
		lowest = faces[face].empty;
		selected = face;
	    }
        }
        return selected;
    }

    bool all_consumed() const
    {
        for (size_t i = 0; i < 8; ++i)
            if (degree_pos[i] != s.face_degrees[i].size() || mask_pos[i] != s.face_attribute_masks[i].size()) return false;
        return vertex_pos == s.vertex_valences.size() && high_mask_pos == s.high_degree_attribute_masks.size() &&
            split_face_pos == s.split_face_offsets.size() && split_slot_pos == s.split_face_positions.size();
    }

    bool export_mesh(TopologyMesh &result)
    {
        result = {};
        for (const Vertex &vertex : vertices) {
            if (vertex.flags & 1) continue; // Cover face added solely to close the dual mesh.
            std::vector<int32_t> polygon;
            std::vector<int32_t> attributes;
            polygon.reserve(vertex.faces.size());
            attributes.reserve(vertex.faces.size());
            for (int32_t face_index : vertex.faces) {
                if (face_index < 0) return fail("JT topology output contains an incomplete polygon");
                polygon.push_back(face_index); // Dual faces are primal coordinate records.
                const Face &face = faces[face_index];
                int slot = find_vertex_slot(face_index, static_cast<int32_t>(&vertex - vertices.data()));
                int attr = -1, cursor = 0;
                if (slot >= 0) {
                    for (int i = 0; i <= slot; ++i) {
                        if (!face.attribute_mask[i]) continue;
                        if (i == slot) attr = face.attributes[cursor];
                        ++cursor;
                    }
                }
                attributes.push_back(attr);
            }
            result.polygons.push_back(std::move(polygon));
            result.polygon_groups.push_back(vertex.group);
            result.attribute_indices.push_back(std::move(attributes));
            const std::vector<int32_t> &stored = result.polygons.back();
            for (size_t i = 1; i + 1 < stored.size(); ++i) {
                if (stored[0] == stored[i] || stored[0] == stored[i + 1] || stored[i] == stored[i + 1])
                    continue;
                result.triangles.push_back(stored[0]);
                result.triangles.push_back(stored[i]);
                result.triangles.push_back(stored[i + 1]);
            }
        }
        return true;
    }

    const TopologySymbols &s;
    std::string &err;
    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    std::vector<int32_t> active;
    std::vector<bool> removed;
    std::array<size_t, 8> degree_pos{};
    std::array<size_t, 8> mask_pos{};
    size_t vertex_pos = 0, high_mask_pos = 0, split_face_pos = 0, split_slot_pos = 0;
    int32_t attribute_counter = 0;
};

} // namespace

bool decode_topology(const TopologySymbols &symbols, TopologyMesh &mesh, std::string &error)
{
    error.clear();
    return Decoder(symbols, error).run(mesh);
}

} // namespace jt
