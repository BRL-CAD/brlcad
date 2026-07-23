/*                         J T _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "jt_parser.h"

#include <zlib.h>

#include "vmath.h"
#include "nmg.h"
#include "bu/assert.h"
#include "bu/avs.h"
#include "bu/log.h"
#include "bu/mime.h"
#include "bu/units.h"
#include "gcv/api.h"
#include "gcv/util.h"
#include "raytrace.h"
#include "wdb.h"

static int
jt_can_read(const char *path)
{
    return jt::has_jt_signature(path) ? 1 : 0;
}

/* JT segment types that can carry renderable shape/LOD geometry (JT File Format
 * Reference, TOC/Data Segment types): 6 Shape, 7 Shape LOD, 8..16 the various
 * tri-strip/point/polyline/primitive shape and LOD segment kinds. */
static const uint32_t JT_SEG_SHAPE_MIN = 6;
static const uint32_t JT_SEG_SHAPE_MAX = 16;

/* Map a JT measurement-units string (from JT_PROP_MEASUREMENT_UNITS) to a
 * BRL-CAD unit name and the millimetres-per-unit factor used to convert the
 * file's coordinates into BRL-CAD's mm base.  Defaults to millimetres. */
static void
jt_units_to_brlcad(const std::string &jt_units, const char *&unit_name, double &mm_per_unit)
{
    std::string u;
    for (char c : jt_units) u.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    unit_name = "mm";
    mm_per_unit = 1.0;
    if (u.find("inch") != std::string::npos || u == "in") { unit_name = "in"; mm_per_unit = 25.4; }
    else if (u.find("feet") != std::string::npos || u.find("foot") != std::string::npos || u == "ft") { unit_name = "ft"; mm_per_unit = 304.8; }
    else if (u.find("centimet") != std::string::npos || u == "cm") { unit_name = "cm"; mm_per_unit = 10.0; }
    else if (u.find("meter") != std::string::npos || u.find("metre") != std::string::npos || u == "m") { unit_name = "m"; mm_per_unit = 1000.0; }
    else if (u.find("micro") != std::string::npos || u == "um") { unit_name = "um"; mm_per_unit = 0.001; }
}

/* Format a 16-byte JT GUID as the canonical 8-4-4-4-12 lowercase hex string. */
static std::string
jt_guid_to_string(const jt::Guid &guid)
{
    static const char hex[] = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < guid.size(); ++i) {
	if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back('-');
	out.push_back(hex[(guid[i] >> 4) & 0xf]);
	out.push_back(hex[guid[i] & 0xf]);
    }
    return out;
}

/* Attach the JT provenance carried on a decoded mesh (element GUID/object id, LOD
 * rank, and any per-axis quantization / vertex-binding metadata) to the named
 * BRL-CAD object as bu_avs attributes for round-trip fidelity. */
static void
jt_attach_mesh_attributes(struct db_i *dbip, const char *name, const jt::Mesh &mesh)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return;
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    (void)bu_avs_add(&avs, "jt_element_guid", jt_guid_to_string(mesh.element_type_id).c_str());
    if (mesh.element_object_id >= 0)
	(void)bu_avs_add(&avs, "jt_object_id", std::to_string(mesh.element_object_id).c_str());
    (void)bu_avs_add(&avs, "jt_lod_level", std::to_string(static_cast<unsigned>(mesh.lod_level)).c_str());
    /* JT element base type: 4 == legacy tri-strip/polygon Shape LOD, 9 == JT
     * 9/10 topologically compressed rep. */
    (void)bu_avs_add(&avs, "jt_element_base_type", std::to_string(static_cast<unsigned>(mesh.element_base_type)).c_str());
    (void)bu_avs_add(&avs, "jt_binding_mode", std::to_string(mesh.vertex_binding_mode).c_str());
    if (mesh.bbox.valid) {
	static const char *axis[3] = {"x", "y", "z"};
	for (int c = 0; c < 3; ++c) {
	    (void)bu_avs_add(&avs, (std::string("jt_bbox_min_") + axis[c]).c_str(),
		std::to_string(mesh.bbox.min[c]).c_str());
	    (void)bu_avs_add(&avs, (std::string("jt_bbox_max_") + axis[c]).c_str(),
		std::to_string(mesh.bbox.max[c]).c_str());
	}
    }
    if (mesh.has_quantizers) {
	static const char *axis[3] = {"x", "y", "z"};
	for (int c = 0; c < 3; ++c) {
	    (void)bu_avs_add(&avs, (std::string("jt_quantize_") + axis[c] + "_min").c_str(),
		std::to_string(mesh.quantizers[c].minimum).c_str());
	    (void)bu_avs_add(&avs, (std::string("jt_quantize_") + axis[c] + "_max").c_str(),
		std::to_string(mesh.quantizers[c].maximum).c_str());
	    (void)bu_avs_add(&avs, (std::string("jt_quantize_") + axis[c] + "_bits").c_str(),
		std::to_string(static_cast<unsigned>(mesh.quantizers[c].bits)).c_str());
	}
    }
    /* GEO-028: preserve the topological mesh's polygon/vertex grouping.  Each
     * distinct group ID recovered from the source is recorded as jt_group_N ->
     * <group id> so downstream tools can reconstruct the JT part grouping. */
    for (size_t g = 0; g < mesh.polygon_groups.size(); ++g)
	(void)bu_avs_add(&avs, (std::string("jt_group_") + std::to_string(g)).c_str(),
	    std::to_string(mesh.polygon_groups[g]).c_str());
    (void)db5_update_attributes(dp, &avs, dbip);
    bu_avs_free(&avs);
}

/* Attach the JT file header metadata (version, byte order, LSG root segment GUID)
 * to the top-level 'all' combination as bu_avs attributes. */
static void
jt_attach_header_attributes(struct db_i *dbip, const jt::File &file)
{
    struct directory *dp = db_lookup(dbip, "all", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return;
    const jt::Header &h = file.header();
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    (void)bu_avs_add(&avs, "jt_version",
	(std::to_string(h.major_version) + "." + std::to_string(h.minor_version)).c_str());
    (void)bu_avs_add(&avs, "jt_byte_order", h.little_endian ? "little" : "big");
    if (h.lsg_segment_id)
	(void)bu_avs_add(&avs, "jt_lsg_guid", jt_guid_to_string(*h.lsg_segment_id).c_str());
    (void)db5_update_attributes(dp, &avs, dbip);
    bu_avs_free(&avs);
}

/* Serialize a JT element object-id -> BRL-CAD object-name mapping as a compact
 * JSON object and attach it as the 'jt_object_id_map' attribute on the top-level
 * 'all' assembly, so external tools can resolve JT cross-references to the names
 * this import produced.  A no-op when nothing carried an object id. */
static void
jt_attach_object_id_map(struct db_i *dbip, const std::map<int32_t, std::string> &id_map)
{
    if (id_map.empty())
	return;
    struct directory *dp = db_lookup(dbip, "all", LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return;
    std::string json = "{";
    bool first = true;
    for (const auto &entry : id_map) {
	if (!first) json += ",";
	first = false;
	json += "\"" + std::to_string(entry.first) + "\":\"";
	/* Escape the characters that would otherwise break the JSON string. */
	for (char ch : entry.second) {
	    if (ch == '"' || ch == '\\') json.push_back('\\');
	    json.push_back(ch);
	}
	json += "\"";
    }
    json += "}";
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    (void)bu_avs_add(&avs, "jt_object_id_map", json.c_str());
    (void)db5_update_attributes(dp, &avs, dbip);
    bu_avs_free(&avs);
}

/* GEO-011: drop zero-area (degenerate) triangles from a decoded mesh.  A triangle
 * is degenerate when the magnitude of the cross product of two of its edges is
 * below 'epsilon' (derived from the import scale so the test tracks model units).
 * Faces are stored as flat triples of vertex indices; vertices/normals are left
 * untouched (unreferenced vertices are harmless to mk_bot). */
static void
jt_filter_degenerate_faces(jt::Mesh &mesh, double epsilon)
{
    const size_t vcount = mesh.vertices.size() / 3;
    std::vector<int> kept;
    kept.reserve(mesh.faces.size());
    size_t dropped = 0;
    for (size_t f = 0; f + 2 < mesh.faces.size(); f += 3) {
	const int ia = mesh.faces[f], ib = mesh.faces[f + 1], ic = mesh.faces[f + 2];
	if (ia < 0 || ib < 0 || ic < 0 ||
	    static_cast<size_t>(ia) >= vcount || static_cast<size_t>(ib) >= vcount ||
	    static_cast<size_t>(ic) >= vcount) {
	    ++dropped;
	    continue;
	}
	const double *a = &mesh.vertices[static_cast<size_t>(ia) * 3];
	const double *b = &mesh.vertices[static_cast<size_t>(ib) * 3];
	const double *c = &mesh.vertices[static_cast<size_t>(ic) * 3];
	const double e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
	const double e1[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
	const double cx = e0[1] * e1[2] - e0[2] * e1[1];
	const double cy = e0[2] * e1[0] - e0[0] * e1[2];
	const double cz = e0[0] * e1[1] - e0[1] * e1[0];
	if (std::sqrt(cx * cx + cy * cy + cz * cz) < epsilon) {
	    ++dropped;
	    continue;
	}
	kept.push_back(ia); kept.push_back(ib); kept.push_back(ic);
    }
    if (dropped)
	mesh.faces.swap(kept);
}

/* GEO-008: merge vertices whose positions coincide within 'tolerance' and remap
 * every face index onto the surviving vertex.  A coarse spatial hash keyed on the
 * quantized cell of each position makes the near-duplicate search close to linear;
 * per-vertex normals are carried along with the first vertex kept in each cell. */
static void
jt_weld_vertices(jt::Mesh &mesh, double tolerance)
{
    const size_t vcount = mesh.vertices.size() / 3;
    if (vcount == 0 || tolerance <= 0.0)
	return;
    const bool have_normals = mesh.normals.size() == mesh.vertices.size();
    const double inv_cell = 1.0 / tolerance;
    auto key_of = [&](const double *p) {
	long kx = static_cast<long>(std::floor(p[0] * inv_cell));
	long ky = static_cast<long>(std::floor(p[1] * inv_cell));
	long kz = static_cast<long>(std::floor(p[2] * inv_cell));
	return std::to_string(kx) + "," + std::to_string(ky) + "," + std::to_string(kz);
    };
    std::map<std::string, std::vector<int>> cells;   /* cell -> new indices in it */
    std::vector<int> remap(vcount, -1);
    std::vector<double> new_vertices;
    std::vector<double> new_normals;
    new_vertices.reserve(mesh.vertices.size());
    const double tol_sq = tolerance * tolerance;
    for (size_t v = 0; v < vcount; ++v) {
	const double *p = &mesh.vertices[v * 3];
	int found = -1;
	/* Neighbouring cells must be searched too, since a point near a cell
	 * boundary can have its match in an adjacent cell. */
	for (int dx = -1; dx <= 1 && found < 0; ++dx)
	    for (int dy = -1; dy <= 1 && found < 0; ++dy)
		for (int dz = -1; dz <= 1 && found < 0; ++dz) {
		    const double np[3] = {p[0] + dx * tolerance, p[1] + dy * tolerance, p[2] + dz * tolerance};
		    auto it = cells.find(key_of(np));
		    if (it == cells.end()) continue;
		    for (int cand : it->second) {
			const double *q = &new_vertices[static_cast<size_t>(cand) * 3];
			const double dd = (p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) +
			    (p[2] - q[2]) * (p[2] - q[2]);
			if (dd <= tol_sq) { found = cand; break; }
		    }
		}
	if (found >= 0) {
	    remap[v] = found;
	    continue;
	}
	const int nidx = static_cast<int>(new_vertices.size() / 3);
	new_vertices.push_back(p[0]); new_vertices.push_back(p[1]); new_vertices.push_back(p[2]);
	if (have_normals) {
	    const double *n = &mesh.normals[v * 3];
	    new_normals.push_back(n[0]); new_normals.push_back(n[1]); new_normals.push_back(n[2]);
	}
	cells[key_of(p)].push_back(nidx);
	remap[v] = nidx;
    }
    if (new_vertices.size() == mesh.vertices.size())
	return;   /* nothing merged */
    for (int &idx : mesh.faces)
	if (idx >= 0 && static_cast<size_t>(idx) < remap.size())
	    idx = remap[static_cast<size_t>(idx)];
    mesh.vertices.swap(new_vertices);
    if (have_normals)
	mesh.normals.swap(new_normals);
}

/* GEO-009: detect and correct a mesh whose triangles are predominantly wound so
 * their geometric normals point inward.  Accumulate each face normal's dot with
 * the vector from the mesh centroid to the face centroid; when the majority point
 * inward (negative sum) every triangle's winding is reversed by swapping its last
 * two vertices. */
static void
jt_correct_winding(jt::Mesh &mesh)
{
    const size_t vcount = mesh.vertices.size() / 3;
    if (vcount == 0 || mesh.faces.size() < 3)
	return;
    double center[3] = {0.0, 0.0, 0.0};
    for (size_t v = 0; v < vcount; ++v)
	for (int c = 0; c < 3; ++c) center[c] += mesh.vertices[v * 3 + c];
    for (int c = 0; c < 3; ++c) center[c] /= static_cast<double>(vcount);
    double accumulated = 0.0;
    for (size_t f = 0; f + 2 < mesh.faces.size(); f += 3) {
	const int ia = mesh.faces[f], ib = mesh.faces[f + 1], ic = mesh.faces[f + 2];
	if (ia < 0 || ib < 0 || ic < 0)
	    continue;
	const double *a = &mesh.vertices[static_cast<size_t>(ia) * 3];
	const double *b = &mesh.vertices[static_cast<size_t>(ib) * 3];
	const double *c = &mesh.vertices[static_cast<size_t>(ic) * 3];
	const double e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
	const double e1[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
	const double n[3] = {e0[1] * e1[2] - e0[2] * e1[1], e0[2] * e1[0] - e0[0] * e1[2],
	    e0[0] * e1[1] - e0[1] * e1[0]};
	const double fc[3] = {(a[0] + b[0] + c[0]) / 3.0 - center[0],
	    (a[1] + b[1] + c[1]) / 3.0 - center[1], (a[2] + b[2] + c[2]) / 3.0 - center[2]};
	accumulated += n[0] * fc[0] + n[1] * fc[1] + n[2] * fc[2];
    }
    if (accumulated < 0.0)
	for (size_t f = 0; f + 2 < mesh.faces.size(); f += 3)
	    std::swap(mesh.faces[f + 1], mesh.faces[f + 2]);
}

/* GEO-048: drop any triangle referencing a vertex index outside [0, vertex_count)
 * before the mesh reaches mk_bot, where a corrupt index would otherwise cause a
 * failed or crashing BoT creation.  Counts and (verbosely) reports the number of
 * out-of-bounds faces filtered. */
static void
jt_validate_face_indices(jt::Mesh &mesh, bool verbose)
{
    const size_t vcount = mesh.vertices.size() / 3;
    std::vector<int> kept;
    kept.reserve(mesh.faces.size());
    size_t dropped = 0;
    for (size_t f = 0; f + 2 < mesh.faces.size(); f += 3) {
	const int ia = mesh.faces[f], ib = mesh.faces[f + 1], ic = mesh.faces[f + 2];
	if (ia < 0 || ib < 0 || ic < 0 ||
	    static_cast<size_t>(ia) >= vcount || static_cast<size_t>(ib) >= vcount ||
	    static_cast<size_t>(ic) >= vcount) {
	    ++dropped;
	    continue;
	}
	kept.push_back(ia); kept.push_back(ib); kept.push_back(ic);
    }
    if (dropped) {
	mesh.faces.swap(kept);
	if (verbose)
	    bu_log("JT import: filtered %zu triangle(s) with out-of-bounds vertex indices\n", dropped);
    }
}

/* GEO-045: check whether the mesh is edge-manifold -- every undirected edge should
 * be shared by exactly two faces.  Counts edges with a face count != 2 and, in
 * verbose mode, warns with the non-manifold edge count so meshes needing repair
 * can be flagged.  Read-only: the mesh is not modified. */
static void
jt_report_non_manifold(const jt::Mesh &mesh, bool verbose)
{
    if (!verbose || mesh.faces.size() < 3)
	return;
    std::map<std::pair<int, int>, int> edge_faces;
    for (size_t f = 0; f + 2 < mesh.faces.size(); f += 3) {
	const int v[3] = {mesh.faces[f], mesh.faces[f + 1], mesh.faces[f + 2]};
	for (int e = 0; e < 3; ++e) {
	    int a = v[e], b = v[(e + 1) % 3];
	    if (a > b) std::swap(a, b);
	    ++edge_faces[std::make_pair(a, b)];
	}
    }
    size_t non_manifold = 0;
    for (const auto &entry : edge_faces)
	if (entry.second != 2)
	    ++non_manifold;
    if (non_manifold)
	bu_log("JT import: mesh is non-manifold (%zu edge(s) not shared by exactly two faces)\n",
	    non_manifold);
}

/* GEO-047: when a mesh carries no per-vertex normals, synthesize per-vertex
 * shading normals by area-weighted accumulation of the geometric normal of every
 * incident face (edge cross product), then normalizing.  Populates mesh.normals
 * (parallel to vertices) so downstream shading has a fallback; a no-op when
 * normals are already present or the mesh is empty. */
static void
jt_generate_normals(jt::Mesh &mesh)
{
    const size_t vcount = mesh.vertices.size() / 3;
    if (vcount == 0 || mesh.faces.size() < 3)
	return;
    if (mesh.normals.size() == mesh.vertices.size())
	return;   /* normals already recovered from the source */
    std::vector<double> normals(vcount * 3, 0.0);
    for (size_t f = 0; f + 2 < mesh.faces.size(); f += 3) {
	const int ia = mesh.faces[f], ib = mesh.faces[f + 1], ic = mesh.faces[f + 2];
	if (ia < 0 || ib < 0 || ic < 0 ||
	    static_cast<size_t>(ia) >= vcount || static_cast<size_t>(ib) >= vcount ||
	    static_cast<size_t>(ic) >= vcount)
	    continue;
	const double *a = &mesh.vertices[static_cast<size_t>(ia) * 3];
	const double *b = &mesh.vertices[static_cast<size_t>(ib) * 3];
	const double *c = &mesh.vertices[static_cast<size_t>(ic) * 3];
	const double e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
	const double e1[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
	/* Unnormalized cross product: its magnitude is twice the triangle area,
	 * giving a natural area weight when accumulated at each corner. */
	const double n[3] = {e0[1] * e1[2] - e0[2] * e1[1], e0[2] * e1[0] - e0[0] * e1[2],
	    e0[0] * e1[1] - e0[1] * e1[0]};
	const int idx[3] = {ia, ib, ic};
	for (int k = 0; k < 3; ++k)
	    for (int c2 = 0; c2 < 3; ++c2)
		normals[static_cast<size_t>(idx[k]) * 3 + c2] += n[c2];
    }
    for (size_t v = 0; v < vcount; ++v) {
	double *n = &normals[v * 3];
	const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
	if (len > 0.0) {
	    n[0] /= len; n[1] /= len; n[2] /= len;
	}
    }
    mesh.normals.swap(normals);
}

/* Write a decoded mesh as a BoT, using mk_bot_w_normals when per-vertex normals
 * were recovered (GEO-002/GEO-018) so ray tracers get smooth shading, and plain
 * mk_bot otherwise.  Returns the mk_bot* status (0 on success). */
static int
jt_make_bot(struct rt_wdb *wdb, const char *name, jt::Mesh &mesh)
{
    const size_t nvert = mesh.vertices.size() / 3;
    const size_t nface = mesh.faces.size() / 3;
    if (mesh.normals.size() == mesh.vertices.size() && nvert > 0 && nface > 0) {
	/* Per-vertex normals are parallel to the vertex array, so each face's
	 * normal indices are exactly its vertex indices. */
	return mk_bot_w_normals(wdb, name, RT_BOT_SOLID, RT_BOT_CCW,
	    RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS, nvert, nface,
	    mesh.vertices.data(), mesh.faces.data(), NULL, NULL,
	    nvert, mesh.normals.data(), mesh.faces.data());
    }
    return mk_bot(wdb, name, RT_BOT_SOLID, RT_BOT_CCW, 0, nvert, nface,
	mesh.vertices.data(), mesh.faces.data(), NULL, NULL);
}

/* Apply the geometry-fidelity post-processing passes to a decoded mesh in the
 * order that keeps the arrays consistent: drop degenerate triangles, weld
 * coincident vertices (remapping faces), then correct inverted winding.  The
 * tolerance is derived from the caller's import scale. */
static void
jt_postprocess_mesh(jt::Mesh &mesh, double tolerance, bool verbose)
{
    jt_filter_degenerate_faces(mesh, tolerance * tolerance);
    jt_weld_vertices(mesh, tolerance);
    jt_correct_winding(mesh);
    /* GEO-048: reject any remaining out-of-bounds face indices before mk_bot. */
    jt_validate_face_indices(mesh, verbose);
    /* GEO-045: report non-manifold edges (verbose-only, read-only). */
    jt_report_non_manifold(mesh, verbose);
    /* GEO-047: synthesize per-vertex normals when the source carried none. */
    jt_generate_normals(mesh);
}

static int
jt_read(struct gcv_context *context, const struct gcv_opts *options,
	const void *UNUSED(options_data), const char *source_path)
{
    jt::File file;
    std::string error;
    if (!file.load(source_path, error)) {
	bu_log("JT import failed: %s: %s\n", source_path, error.c_str());
	return 0;
    }

    /* A zero or negative scale factor would collapse all geometry; fall back to
     * an identity scale so a mis-defaulted option cannot silently destroy the model. */
    double scale = options->scale_factor;
    if (!(scale > 0.0))
	scale = 1.0;

    /* Detailed per-segment/per-mesh skip reasons are only emitted when the user
     * asks for verbose output; otherwise a single summary is printed so large
     * assemblies (hundreds of segments) do not flood the log. */
    const bool verbose = options->verbosity_level > 0 || getenv("JT_VERBOSE") != NULL;

    struct rt_wdb *wdb = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);

    /* Parse the scene graph up front so the model units (JT_PROP_MEASUREMENT_UNITS)
     * can drive both the database's display units and the mm conversion factor. */
    std::vector<jt::SceneInstance> lsg_instances;
    std::string lsg_error;
    (void)file.scene_graph(lsg_instances, lsg_error);

    const char *unit_name = "mm";
    double mm_per_unit = 1.0;
    for (const jt::SceneInstance &inst : lsg_instances) {
	bool found = false;
	for (const auto &prop : inst.properties)
	    if (prop.first == "JT_PROP_MEASUREMENT_UNITS") {
		jt_units_to_brlcad(prop.second, unit_name, mm_per_unit);
		found = true;
		break;
	    }
	if (found) break;
    }
    scale *= mm_per_unit;
    /* Fold the unit scale into a composable transform so future rotation/recenter
     * adjustments can be applied in the same vertex pass without re-tessellating. */
    jt::MeshTransform transform;
    transform.scale = scale;
    /* Tolerance (in the post-transform mm space) for the GEO welding / degenerate
     * / winding passes.  Scaled off the import factor so it tracks model size;
     * clamped to a small positive floor so a tiny scale cannot disable it. */
    double weld_tolerance = scale * 1e-5;
    if (!(weld_tolerance > 0.0))
	weld_tolerance = 1e-6;
    /* GEO-033: configurable duplicate-vertex merge tolerance.  The gcv_opts
     * structure carries no dedicated field for this (adding one is a public-API
     * change outside this plugin), so honor the JT_VERTEX_MERGE_TOLERANCE
     * environment variable as the override in BRL-CAD units, defaulting to the
     * scale-derived value above (i.e. 1e-6 mm at unit scale).  A non-positive or
     * unparsable value leaves the default untouched. */
    if (const char *tol_env = getenv("JT_VERTEX_MERGE_TOLERANCE")) {
	char *endp = NULL;
	const double v = strtod(tol_env, &endp);
	if (endp != tol_env && v > 0.0) {
	    weld_tolerance = v;
	    if (verbose)
		bu_log("JT import: vertex merge tolerance set to %g by JT_VERTEX_MERGE_TOLERANCE\n", v);
	}
    }
    mk_id_units(wdb, "Conversion from JT format", unit_name);
    if (verbose && mm_per_unit != 1.0)
	bu_log("JT import: model units are %s (x%g to mm)\n", unit_name, mm_per_unit);

    /* Decode the first renderable mesh from a shape segment (referenced by GUID
     * from the scene graph, or scanned directly). */
    auto decode_segment = [&](const jt::TocEntry &entry, jt::Mesh &out) -> bool {
	jt::Segment segment;
	std::vector<jt::Element> elements;
	std::string e;
	if (!file.segment(entry, segment, e) || !file.elements(segment, elements, e))
	    return false;
	for (const jt::Element &element : elements) {
	    const bool topological = file.is_topological_mesh(element);
	    const bool legacy = file.is_legacy_mesh(element);
	    if (!topological && !legacy) continue;
	    jt::Mesh mesh;
	    if (!(topological ? file.topological_mesh(element, mesh, e) : file.legacy_mesh(element, mesh, e)))
		continue;
	    if (mesh.vertices.size() < 9 || mesh.faces.size() < 3) continue;
	    out = std::move(mesh);
	    return true;
	}
	return false;
    };

    /* ---- Preferred path: Logical Scene Graph (assembly hierarchy) ----
     * Each leaf shape instance becomes a colored region whose single member is
     * the shared shape BoT placed by the instance's accumulated 4x4 transform. */
    if (!lsg_instances.empty()) {
	struct wmember all = WMEMBER_INIT_ZERO;
	BU_LIST_INIT(&all.l);
	std::map<jt::Guid, std::string> shape_names;   /* segment GUID -> BoT name (cached) */
	std::set<std::string> used_names;
	std::map<int32_t, std::string> object_id_map;  /* JT element object id -> BoT name */
	size_t shape_count = 0, created = 0;
	for (const jt::SceneInstance &inst : lsg_instances) {
	    std::string bot;
	    auto cached = shape_names.find(inst.segment_id);
	    if (cached == shape_names.end()) {
		const jt::TocEntry *entry = file.toc_entry_by_id(inst.segment_id);
		jt::Mesh mesh;
		if (entry && decode_segment(*entry, mesh)) {
		    transform.apply(mesh);
		    jt_postprocess_mesh(mesh, weld_tolerance, verbose);
		    bot = "jt_shape_" + std::to_string(++shape_count) + ".bot";
		    if (mesh.vertices.size() < 9 || mesh.faces.size() < 3 ||
			jt_make_bot(wdb, bot.c_str(), mesh) != 0)
			bot.clear();
		    else {
			jt_attach_mesh_attributes(context->dbip, bot.c_str(), mesh);
			if (mesh.element_object_id >= 0)
			    object_id_map[mesh.element_object_id] = bot;
		    }
		}
		shape_names[inst.segment_id] = bot;   /* cache even failures to avoid re-decoding */
	    } else {
		bot = cached->second;
	    }
	    if (bot.empty()) continue;

	    mat_t m;
	    for (int i = 0; i < 16; ++i) m[i] = inst.matrix[i];
	    m[3] *= scale; m[7] *= scale; m[11] *= scale;   /* translation shares the unit scale */
	    struct wmember member = WMEMBER_INIT_ZERO;
	    BU_LIST_INIT(&member.l);
	    mk_addmember(bot.c_str(), &member.l, m, WMOP_UNION);

	    std::string base = inst.name.empty() ? std::string("jt_part") : inst.name;
	    for (char &ch : base)
		if (ch == '/' || ch == '\\' || static_cast<unsigned char>(ch) < 32) ch = '_';
	    std::string rname = base;
	    for (int suffix = 1; used_names.count(rname); ++suffix)
		rname = base + "_" + std::to_string(suffix);
	    used_names.insert(rname);

	    unsigned char rgb[3];
	    const unsigned char *rgbp = NULL;
	    if (inst.has_color) {
		for (int i = 0; i < 3; ++i) {
		    float v = inst.color[i];
		    v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
		    rgb[i] = static_cast<unsigned char>(v * 255.0f + 0.5f);
		}
		rgbp = rgb;
	    }
	    mk_lrcomb(wdb, rname.c_str(), &member, 1, NULL, NULL, rgbp, 0, 0, 0, 0, 0);
	    mk_addmember(rname.c_str(), &all.l, NULL, WMOP_UNION);
	    ++created;

	    /* Preserve the JT part metadata (name, part number, custom keys) plus
	     * per-instance material transparency and node visibility as BRL-CAD
	     * attributes on the region. */
	    {
		struct directory *dp = db_lookup(context->dbip, rname.c_str(), LOOKUP_QUIET);
		if (dp != RT_DIR_NULL) {
		    struct bu_attribute_value_set avs;
		    bu_avs_init_empty(&avs);
		    for (const auto &prop : inst.properties)
			if (!prop.first.empty() && prop.first.size() < 128 && prop.second.size() < 4096)
			    (void)bu_avs_add(&avs, prop.first.c_str(), prop.second.c_str());
		    /* color[3] is the alpha/opacity from LSG material parsing. */
		    if (inst.has_color) {
			float a = inst.color[3];
			a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
			(void)bu_avs_add(&avs, "jt_transparency", std::to_string(1.0f - a).c_str());
		    }
		    (void)bu_avs_add(&avs, "jt_visible", inst.visible ? "1" : "0");
		    /* Node ID of the Part referenced by the enclosing LSG Instance
		     * node, recorded for cross-reference traceability. */
		    if (inst.referenced_part_id >= 0)
			(void)bu_avs_add(&avs, "jt_referenced_part_id",
			    std::to_string(inst.referenced_part_id).c_str());
		    (void)db5_update_attributes(dp, &avs, context->dbip);
		    bu_avs_free(&avs);
		}
	    }
	}
	if (created > 0) {
	    mk_lcomb(wdb, "all", &all, 0, NULL, NULL, NULL, 0);
	    jt_attach_header_attributes(context->dbip, file);
	    jt_attach_object_id_map(context->dbip, object_id_map);
	    if (verbose)
		bu_log("JT import: %zu part instance(s) placed from the scene graph\n", created);
	    return 1;
	}
	/* Nothing usable came out of the LSG; fall through to the flat scan. */
    }

    /* ---- Fallback: flat scan of every shape segment in the TOC ---- */
    std::vector<jt::Mesh> meshes;
    size_t toc_index = 0;
    size_t skipped_segments = 0;
    size_t skipped_meshes = 0;
    for (const jt::TocEntry &entry : file.toc()) {
	++toc_index;
	if (entry.type() < JT_SEG_SHAPE_MIN || entry.type() > JT_SEG_SHAPE_MAX) continue;
	jt::Segment segment;
	std::vector<jt::Element> elements;
	if (!file.segment(entry, segment, error) || !file.elements(segment, elements, error)) {
	    ++skipped_segments;
	    if (verbose)
		bu_log("JT shape segment in TOC entry %zu skipped: %s\n", toc_index - 1, error.c_str());
	    continue;
	}
	for (const jt::Element &element : elements) {
	    const bool topological = file.is_topological_mesh(element);
	    const bool legacy = file.is_legacy_mesh(element);
	    if (!topological && !legacy) {
		/* J8-015: report elements that are silently skipped because their
		 * GUID is not a supported renderable shape, including the element
		 * GUID and base type/version so unsupported segment kinds can be
		 * identified from a verbose import log. */
		++skipped_meshes;
		if (verbose)
		    bu_log("JT element in TOC entry %zu skipped: unsupported shape GUID %s "
			"(base type %u, JT major version %u)\n", toc_index - 1,
			jt_guid_to_string(element.type_id).c_str(),
			static_cast<unsigned>(element.base_type), file.header().major_version);
		continue;
	    }
	    jt::Mesh mesh;
	    const bool imported = topological ?
		file.topological_mesh(element, mesh, error) : file.legacy_mesh(element, mesh, error);
	    if (!imported) {
		++skipped_meshes;
		if (verbose)
		    bu_log("JT mesh in TOC entry %zu skipped: %s\n", toc_index - 1, error.c_str());
		continue;
	    }
	    /* A mesh with no coordinates or no faces is not renderable; drop it
	     * rather than handing mk_bot a degenerate (zero-vertex) BoT. */
	    if (mesh.vertices.size() < 9 || mesh.faces.size() < 3)
		continue;
	    meshes.push_back(std::move(mesh));
	}
    }
    if (!verbose && (skipped_segments || skipped_meshes))
	bu_log("JT import: skipped %zu segment(s) and %zu mesh(es) that could not be decoded "
	    "(use -v for details)\n", skipped_segments, skipped_meshes);

    if (meshes.empty()) {
	bu_log("JT import failed: %s: no supported renderable mesh geometry found\n", source_path);
	return 0;
    }

    struct wmember all = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&all.l);
    std::map<int32_t, std::string> object_id_map;   /* JT element object id -> BoT name */
    size_t created = 0;
    for (size_t i = 0; i < meshes.size(); ++i) {
	std::string name = "jt_mesh_" + std::to_string(i + 1) + ".bot";
	jt::Mesh &mesh = meshes[i];
	transform.apply(mesh);
	jt_postprocess_mesh(mesh, weld_tolerance, verbose);
	/* One bad BoT should not abort a multi-part import: warn and skip it. */
	if (mesh.vertices.size() < 9 || mesh.faces.size() < 3 ||
		jt_make_bot(wdb, name.c_str(), mesh) != 0) {
	    bu_log("JT import: cannot create %s; skipping\n", name.c_str());
	    continue;
	}
	jt_attach_mesh_attributes(context->dbip, name.c_str(), mesh);
	if (mesh.element_object_id >= 0)
	    object_id_map[mesh.element_object_id] = name;
	mk_addmember(name.c_str(), &all.l, NULL, WMOP_UNION);
	++created;
    }
    if (created == 0) {
	bu_log("JT import failed: %s: no mesh could be written\n", source_path);
	return 0;
    }
    mk_lcomb(wdb, "all", &all, 0, NULL, NULL, NULL, 0);
    jt_attach_header_attributes(context->dbip, file);
    jt_attach_object_id_map(context->dbip, object_id_map);
    return 1;
}

/* ------------------------------------------------------------------------
 * JT export.  Geometry is tessellated to triangles through the NMG booleans
 * (as g-stl does) and written as a single JT 8 Tri-Strip Set Shape LOD
 * element with lossless, uncompressed vertex coordinates -- the simplest
 * representation this plugin's reader round-trips faithfully.
 * ---------------------------------------------------------------------- */
namespace {

/* EXP-030: object identity preserved for one exported region: its BRL-CAD path
 * name, whether an explicit region color was found, and that RGB (0..1).  The
 * writer records one of these per region contributing geometry so the names and
 * colors can be threaded into JT metadata / LSG property bindings rather than
 * being discarded in favour of generic jt_mesh_N labels. */
struct jt_region_info {
    std::string name;
    bool has_color = false;
    float color[3] = {0.0f, 0.0f, 0.0f};
    /* EXP-008: material/shader name and density carried from the region's tree
     * state / attributes so the JT Meta Data property map can record the region's
     * appearance and physical material.  Empty name / non-positive density mean
     * "not specified" and are omitted from the property records. */
    std::string material;
    double density = 0.0;
    size_t first_triangle = 0;   /* index of this region's first triangle in coords */
    size_t triangle_count = 0;   /* triangles this region contributed */
    /* EXP-046: the 4x4 placement matrix accumulated down the BRL-CAD comb/region
     * hierarchy to this leaf (db_tree_state::ts_mat, row-major, mm).  Captured so
     * the leaf's spatial relationship (its transform from model space) can be
     * emitted alongside the region in the Meta Data property map, matching the JT
     * LSG PartitionNode transform-binding intent.  has_matrix is false for an
     * identity/unavailable transform, in which case it is omitted. */
    bool has_matrix = false;
    double matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

struct jt_write_state {
    const struct gcv_opts *gcv_options;
    struct db_i *dbip;
    struct model *the_model;
    struct bu_list *vlfree;
    std::vector<double> coords;   /* nine doubles per triangle (three xyz corners) */
    size_t triangles;
    /* EXP-030: per-region object name + color, in the order regions were walked. */
    std::vector<jt_region_info> regions;
};

/* Writer-side options controlling the optional encodings the exporter can emit.
 * There is no GCV options struct registered for the JT writer yet (the filter's
 * create_opts/process_options hooks are NULL), so these carry conservative,
 * interoperable defaults; each flag gates one EXP-* encoding path so the choices
 * stay explicit and testable in one place. */
struct jt_write_options {
    /* EXP-001: zlib-deflate the lossless vertex coordinate block instead of
     * storing raw floats.  The reader inflates via zlib uncompress(), which
     * requires the RFC 1950 wrapper, so compressed output is always wrapped. */
    bool compress_vertices = true;
    /* EXP-034: when compressing, emit the RFC 1950 zlib wrapper (0x78 0x9C
     * header + Adler-32 trailer).  The reader's uncompress() path requires the
     * wrapper, so this stays true; false would produce a raw deflate stream the
     * current reader cannot inflate. */
    bool zlib_wrapper = true;
    /* EXP-040: when a quantized-vertex path chooses codes for an all-non-negative
     * model, use unsigned codes (one extra bit of precision) rather than signed
     * 2's-complement.  Detection is automatic (see jt_coords_all_nonneg); this is
     * the master enable. */
    bool unsigned_quant = true;

    /* EXP-015: write-time options threaded from the caller (parsed from
     * gcv_opts->options_data when supplied).  These carry conservative,
     * interoperable defaults so an unconfigured export matches the previous
     * lossless JT 8 output byte-for-byte. */

    /* Target JT major format version (8, 9, or 10).  Controls the header
     * signature and the wide-offset / object-id element framing (EXP-003).
     * Defaults to the widely interoperable JT 8. */
    unsigned version = 8;
    /* zlib deflate level 0..9 used for the vertex block (EXP-001).  -1 selects
     * zlib's default; any other out-of-range value is clamped on use. */
    int compress_level = Z_DEFAULT_COMPRESSION;
    /* EXP-002: per-axis quantization bit count.  0 keeps the lossless float
     * vertex path (the default); 1..24 selects the quantized-vertex encoding. */
    unsigned quantize_bits = 0;
    /* EXP-010/019: number of LOD records to request.  Only a single LOD is
     * emitted today; retained so the option round-trips through the parser. */
    unsigned lod_count = 1;
    /* EXP-019: soft cap on triangles per shape segment.  0 disables chunking
     * (one segment for all geometry, the previous behaviour). */
    size_t max_triangles_per_segment = 0;
    /* EXP-007: preserve the BRL-CAD comb/region hierarchy by routing each leaf
     * region's triangles to its own shape segment (its own GUID and LSG shape
     * node), rather than flattening every region into one segment.  On by
     * default; a single-region model still yields exactly one shape segment, so
     * the common case is unchanged.  Disabled automatically when triangle
     * chunking (max_triangles_per_segment) is requested, since the two ways of
     * splitting the stream are mutually exclusive. */
    bool per_region_segments = true;
    /* EXP-009: emit per-vertex normals with the Sun/Deering angular codec.  The
     * reader only decodes normals in the quantized-vertex path, so normals are
     * emitted only when quantization is also active (quantize_bits > 0); off by
     * default so unconfigured output stays byte-for-byte compatible.  normal_bits
     * is the angular code width. */
    bool emit_normals = false;
    unsigned normal_bits = 8;
    /* EXP-010: number of LOD records to emit as additional shape segments, and the
     * per-level triangle-decimation ratios.  lod_count == 1 (default) emits only
     * the full-resolution mesh.  When lod_count > 1, coarser meshes at the given
     * fractions of the full triangle count are emitted as extra shape segments. */
    std::vector<double> lod_ratios;
    /* EXP-038: optional coordinate-reference-system identifier (e.g. an EPSG code
     * like "EPSG:4326" or a custom projection string) recorded in the File
     * Description segment for CRS awareness.  Empty by default (BRL-CAD's local
     * engineering frame has no geodetic datum), so unconfigured output is
     * unchanged apart from the always-emitted engineering-CRS provenance lines. */
    std::string crs_code;
};

/* EXP-015: parse the JT-specific write options from a caller-supplied option
 * string (gcv_opts->options_data, when non-NULL).  The JT writer registers no
 * GCV option struct yet, so the payload is treated as a whitespace-separated
 * list of "key=value" tokens; unknown tokens are ignored so future keys stay
 * backward compatible.  Recognised keys: version, compress_level, quantize_bits,
 * lod_count, segment_triangles.  Returns the options with any recognised
 * overrides applied. */
static jt_write_options
jt_parse_write_options(const void *options_data)
{
    jt_write_options opts;
    if (!options_data)
	return opts;
    const char *text = static_cast<const char *>(options_data);
    std::string token;
    auto apply = [&opts](const std::string &tok) {
	const size_t eq = tok.find('=');
	if (eq == std::string::npos)
	    return;
	const std::string key = tok.substr(0, eq);
	const std::string val = tok.substr(eq + 1);
	if (val.empty())
	    return;
	if (key == "version") {
	    const long v = std::strtol(val.c_str(), NULL, 10);
	    if (v == 8 || v == 9 || v == 10) opts.version = static_cast<unsigned>(v);
	} else if (key == "compress_level") {
	    long v = std::strtol(val.c_str(), NULL, 10);
	    if (v < -1) v = -1;
	    if (v > 9) v = 9;
	    opts.compress_level = static_cast<int>(v);
	    opts.compress_vertices = (v != 0);
	} else if (key == "quantize_bits") {
	    long v = std::strtol(val.c_str(), NULL, 10);
	    if (v < 0) v = 0;
	    if (v > 24) v = 24;
	    opts.quantize_bits = static_cast<unsigned>(v);
	} else if (key == "lod_count") {
	    long v = std::strtol(val.c_str(), NULL, 10);
	    if (v >= 1) opts.lod_count = static_cast<unsigned>(v);
	} else if (key == "normals") {
	    /* EXP-009: enable Deering per-vertex normal output (only takes effect in
	     * the quantized path, which the reader can decode normals from). */
	    opts.emit_normals = (val == "1" || val == "true" || val == "yes" || val == "on");
	} else if (key == "normal_bits") {
	    long v = std::strtol(val.c_str(), NULL, 10);
	    if (v < 1) v = 1;
	    if (v > 16) v = 16;
	    opts.normal_bits = static_cast<unsigned>(v);
	} else if (key == "segment_triangles") {
	    long v = std::strtol(val.c_str(), NULL, 10);
	    if (v > 0) opts.max_triangles_per_segment = static_cast<size_t>(v);
	} else if (key == "per_region_segments") {
	    /* EXP-007: opt out of the per-region hierarchy (flatten to one shape
	     * segment) when set to a false-ish value. */
	    opts.per_region_segments = !(val == "0" || val == "false" || val == "no" || val == "off");
	} else if (key == "crs_code") {
	    /* EXP-038: keep the CRS identifier ASCII/printable and free of the
	     * newline/'=' delimiters used in the File Description text block. */
	    std::string clean;
	    for (char ch : val)
		if (static_cast<unsigned char>(ch) >= 0x20 && static_cast<unsigned char>(ch) < 0x7f &&
		    ch != '=' && ch != '\n')
		    clean.push_back(ch);
	    opts.crs_code = clean;
	}
    };
    for (const char *p = text; ; ++p) {
	const char c = *p;
	if (c == '\0' || std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';') {
	    if (!token.empty()) { apply(token); token.clear(); }
	    if (c == '\0') break;
	} else {
	    token.push_back(c);
	}
    }
    return opts;
}

/* EXP-040: report whether every coordinate in an XYZ stream is non-negative, so a
 * quantizer can choose unsigned codes (gaining one bit of precision) instead of
 * signed 2's-complement.  An empty stream is treated as non-negative. */
static bool
jt_coords_all_nonneg(const std::vector<double> &coords)
{
    for (double v : coords)
	if (v < 0.0)
	    return false;
    return true;
}

/* EXP-002/040: minimal uniform quantizer configuration.  Records the axis range,
 * bit count, and (EXP-040) whether codes are unsigned.  The encoding path is not
 * yet emitted (the writer stays lossless), so this documents the parameters the
 * quantizer would use and keeps the signed/unsigned choice in one testable place. */
struct jt_quantizer {
    double minimum = 0.0;
    double maximum = 0.0;
    uint32_t bits = 0;
    bool is_unsigned = false;   /* EXP-040 */
};

/* EXP-042: CRC-32 (IEEE 802.3 / zlib polynomial) over a byte range.  Used to
 * checksum an element payload before writing and to verify it on round-trip read,
 * catching silent corruption.  Self-contained (table built on first use) so it
 * needs no zlib symbol and stays deterministic across platforms. */
static uint32_t
jt_crc32(const unsigned char *data, size_t len)
{
    static uint32_t table[256];
    static bool table_ready = false;
    if (!table_ready) {
	for (uint32_t n = 0; n < 256; ++n) {
	    uint32_t c = n;
	    for (int k = 0; k < 8; ++k)
		c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
	    table[n] = c;
	}
	table_ready = true;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
	crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* EXP-012: JT Int32 CDP Stride1 predicted value, mirroring the reader's
 * PredictorCodec::predicted_value() (jt_parser.cpp Stride1 case): the value at
 * index i is predicted as v[i-1] + (v[i-1] - v[i-2]) using wraparound (uint32)
 * arithmetic.  The writer stores residuals so that the reader recovers the true
 * sequence by adding this prediction back; deriving both from the same formula
 * keeps them in lock-step (no hand-expanded arithmetic that can silently drift).
 * Only defined for i >= 2 (the reader leaves the first four values verbatim). */
inline int32_t
jt_stride1_predict(const std::vector<int32_t> &values, size_t index)
{
    const uint32_t v1 = static_cast<uint32_t>(values[index - 1]);
    const uint32_t v2 = static_cast<uint32_t>(values[index - 2]);
    /* add(v1, subtract(v1, v2)) == 2*v1 - v2 in modular arithmetic. */
    return static_cast<int32_t>(v1 + (v1 - v2));
}

/* EXP-012: encode a monotone/arbitrary Int32 value sequence as Stride1 residuals
 * the reader will unpack (jt_parser.cpp unpack_residuals): the first four values
 * are stored verbatim, and each later value stores value[i] - predict(i) so the
 * reader recovers it via value[i] += predict(i).  The prediction reads earlier
 * *decoded* values, which for a residual-encoded stream are exactly the original
 * values, so passing the original sequence here is correct. */
static std::vector<int32_t>
jt_stride1_encode(const std::vector<int32_t> &values)
{
    std::vector<int32_t> residuals = values;
    for (size_t i = values.size(); i-- > 4; ) {
	const uint32_t predicted = static_cast<uint32_t>(jt_stride1_predict(values, i));
	residuals[i] = static_cast<int32_t>(static_cast<uint32_t>(values[i]) - predicted);
    }
    return residuals;
}

/* EXP-002: Lag1 residual encoding, the inverse of the reader's Lag1 unpack
 * (PredictorCodec, predicted = v[i-1]).  The first four values are stored
 * verbatim; each later value stores value[i] - value[i-1] (modular), which the
 * reader recovers by adding the running previous value back. */
static std::vector<int32_t>
jt_lag1_encode(const std::vector<int32_t> &values)
{
    std::vector<int32_t> residuals = values;
    for (size_t i = values.size(); i-- > 4; )
	residuals[i] = static_cast<int32_t>(static_cast<uint32_t>(values[i]) -
	    static_cast<uint32_t>(values[i - 1]));
    return residuals;
}

/* EXP-002: StripIndex residual encoding, the inverse of the reader's StripIndex
 * predictor: predicted = (|v[i-2]-v[i-4]| < 8) ? 2*v[i-2]-v[i-4] : v[i-2]+2.
 * The prediction reads earlier *decoded* values, so it is computed from the
 * original sequence here (which is what the reader reconstructs). */
static std::vector<int32_t>
jt_stripindex_encode(const std::vector<int32_t> &values)
{
    std::vector<int32_t> residuals = values;
    for (size_t i = values.size(); i-- > 4; ) {
	const uint32_t v2 = static_cast<uint32_t>(values[i - 2]);
	const uint32_t v4 = static_cast<uint32_t>(values[i - 4]);
	const int32_t delta = static_cast<int32_t>(v2 - v4);
	const uint32_t predicted = (delta < 8 && delta > -8) ?
	    (v2 + static_cast<uint32_t>(delta)) : (v2 + 2u);
	residuals[i] = static_cast<int32_t>(static_cast<uint32_t>(values[i]) - predicted);
    }
    return residuals;
}

/* EXP-002/012: append an Int32 CDP Null-CODEC packet (codec byte 0, i32 count,
 * then the residual values) to 'out'.  The residuals must already be predictor-
 * encoded to match the predictor the reader applies to this packet. */
static void
jt_put_null_cdp(jt::ByteBuffer &out, const std::vector<int32_t> &residuals)
{
    out.push_back(0);   /* Int32 CDP: Null CODEC */
    for (int i = 0; i < 4; ++i)
	out.push_back(static_cast<unsigned char>((static_cast<uint32_t>(residuals.size()) >> (i * 8)) & 0xff));
    for (int32_t r : residuals)
	for (int i = 0; i < 4; ++i)
	    out.push_back(static_cast<unsigned char>((static_cast<uint32_t>(r) >> (i * 8)) & 0xff));
}

inline void jt_put_u16(jt::ByteBuffer &b, uint16_t v)
{
    b.push_back(static_cast<unsigned char>(v & 0xff));
    b.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
}
inline void jt_put_u32(jt::ByteBuffer &b, uint32_t v)
{
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<unsigned char>((v >> (i * 8)) & 0xff));
}
inline void jt_put_i32(jt::ByteBuffer &b, int32_t v) { jt_put_u32(b, static_cast<uint32_t>(v)); }
inline void jt_put_u64(jt::ByteBuffer &b, uint64_t v)
{
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<unsigned char>((v >> (i * 8)) & 0xff));
}
inline void jt_put_f32(jt::ByteBuffer &b, float f)
{
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(u));
    jt_put_u32(b, u);
}

/* EXP-036: deterministic RFC 4122 v5-style GUID factory.  A segment's identity is
 * derived from its content bytes so distinct segments get distinct GUIDs while a
 * given segment always maps to the same GUID (reproducible output, testable
 * against JT validators).  A self-contained FNV-1a mix over the content seeds the
 * 16 bytes; the version (5) and variant (RFC 4122) nibbles are then stamped in so
 * the result is a well-formed v5 UUID.  This has no external dependency (MD5/SHA)
 * and stays ASCII/CRLF-clean. */
static jt::Guid
jt_generate_guid(const jt::ByteBuffer &content)
{
    /* Two independent 64-bit FNV-1a accumulators (one running forward, one over a
     * salted stream) fill the 16 bytes with well-mixed, content-dependent data. */
    uint64_t h1 = 0xcbf29ce484222325ULL;
    uint64_t h2 = 0x84222325cbf29ce4ULL;
    for (size_t i = 0; i < content.size(); ++i) {
	h1 = (h1 ^ content[i]) * 0x100000001b3ULL;
	h2 = (h2 ^ static_cast<unsigned char>(content[i] + static_cast<unsigned char>(i))) * 0x100000001b3ULL;
    }
    jt::Guid guid;
    for (int i = 0; i < 8; ++i) {
	guid[i] = static_cast<unsigned char>((h1 >> (i * 8)) & 0xff);
	guid[8 + i] = static_cast<unsigned char>((h2 >> (i * 8)) & 0xff);
    }
    /* Stamp the RFC 4122 version (5) into the high nibble of byte 6 and the
     * variant (10xx) into the two high bits of byte 8. */
    guid[6] = static_cast<unsigned char>((guid[6] & 0x0f) | 0x50);
    guid[8] = static_cast<unsigned char>((guid[8] & 0x3f) | 0x80);
    return guid;
}

/* EXP-039: bounding box of an XYZ vertex stream (nine doubles per triangle, or any
 * flat XYZ triple array).  Fills mn[]/mx[] with the per-axis min/max and returns
 * false for an empty stream (leaving mn/mx untouched).  This is the input the JT 8
 * uniform quantizer needs before the EXP-002 quantized-vertex encoding path can
 * choose a range and bit count. */
static bool
jt_vertex_bounds(const std::vector<double> &coords, double mn[3], double mx[3])
{
    if (coords.size() < 3)
	return false;
    mn[0] = mx[0] = coords[0];
    mn[1] = mx[1] = coords[1];
    mn[2] = mx[2] = coords[2];
    for (size_t i = 0; i + 2 < coords.size(); i += 3)
	for (int c = 0; c < 3; ++c) {
	    const double v = coords[i + c];
	    if (v < mn[c]) mn[c] = v;
	    if (v > mx[c]) mx[c] = v;
	}
    return true;
}

/* EXP-039: infer a uniform-quantizer bit count from a bounding box and a target
 * absolute precision.  Returns the smallest bit count in [1, 24] whose 2^bits
 * levels resolve the largest axis span to within 'precision'; clamped so a
 * degenerate (zero-span) or huge model still yields a usable value.  This is the
 * groundwork for EXP-002's packed-int32 vertex encoding. */
static uint32_t
jt_infer_quant_bits(const double mn[3], const double mx[3], double precision)
{
    double span = mx[0] - mn[0];
    for (int c = 1; c < 3; ++c) {
	const double s = mx[c] - mn[c];
	if (s > span) span = s;
    }
    if (!(span > 0.0) || !(precision > 0.0))
	return 1;
    for (uint32_t bits = 1; bits < 24; ++bits) {
	const double levels = static_cast<double>(1u << bits) - 1.0;
	if (span / levels <= precision)
	    return bits;
    }
    return 24;
}

/* EXP-014: one Table-of-Contents entry describing a written data segment.  The
 * writer builds a vector of these (one per segment) and jt_write_toc() emits the
 * count followed by N entries, matching the reader's TOC parse (GUID, offset,
 * length, and segment type packed in the high byte of the attributes field). */
struct jt_toc_entry {
    jt::Guid guid;
    uint64_t offset;
    uint32_t length;
    uint32_t seg_type;
};

/* EXP-014/003: append the TOC (entry count + N entries) to 'file'.  Generalized
 * from the previous single-hardcoded-entry path so multiple segments (EXP-006)
 * can be described without touching this writer.  For JT 10+ (wide_offsets) the
 * offset field is 64-bit, matching the reader's version-gated TOC parse. */
static void
jt_write_toc(jt::ByteBuffer &file, const std::vector<jt_toc_entry> &entries, bool wide_offsets)
{
    jt_put_u32(file, static_cast<uint32_t>(entries.size()));   /* TOC entry count */
    for (const jt_toc_entry &e : entries) {
	file.insert(file.end(), e.guid.begin(), e.guid.end());
	if (wide_offsets)
	    jt_put_u64(file, e.offset);
	else
	    jt_put_u32(file, static_cast<uint32_t>(e.offset));
	jt_put_u32(file, e.length);
	jt_put_u32(file, e.seg_type << 24);   /* attributes: segment type in high byte */
    }
}

/* Append the triangle described by a triangulated NMG loop as nine doubles
 * (three XYZ corners) to 'coords'.  Returns true when a well-formed, finite
 * triangle was added, false when the loop is not a 3-vertex triangle or has a
 * non-finite vertex (so a bad tessellation cannot write NaN/Inf to the file).
 * Factored out of nmg_to_jt for isolated testability. */
static bool
add_triangle_from_nmg(const struct loopuse *lu, std::vector<double> &coords)
{
    struct edgeuse *eu;
    double corners[9];
    int vert_count = 0;
    NMG_CK_LOOPUSE(lu);
    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	return false;
    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	struct vertex *v = eu->vu_p->v_p;
	NMG_CK_VERTEX(v);
	if (vert_count < 3) {
	    corners[vert_count * 3 + 0] = v->vg_p->coord[X];
	    corners[vert_count * 3 + 1] = v->vg_p->coord[Y];
	    corners[vert_count * 3 + 2] = v->vg_p->coord[Z];
	}
	vert_count++;
    }
    if (vert_count != 3)
	return false;
    for (int i = 0; i < 9; ++i)
	if (!(corners[i] == corners[i]) || corners[i] > 1e300 || corners[i] < -1e300)
	    return false;
    for (int i = 0; i < 9; ++i) coords.push_back(corners[i]);
    return true;
}

void
nmg_to_jt(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *tsp,
    void *client_data)
{
    struct jt_write_state *state = static_cast<struct jt_write_state *>(client_data);
    struct shell *s;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    /* EXP-044: pre-flight topology validation.  Reject a region with no shells or
     * with a shell carrying no faces before tessellation, so malformed geometry is
     * skipped rather than tessellated into a corrupted (empty/degenerate) mesh.
     * Counts total faces across all shells; a region contributing zero faces adds
     * nothing and is quietly dropped. */
    size_t region_faces = 0;
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;
	NMG_CK_SHELL(s);
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation == OT_SAME)
		++region_faces;
	}
    }
    if (region_faces == 0) {
	if (state->gcv_options->verbosity_level) {
	    char *pstr = db_path_to_string(pathp);
	    bu_log("JT export: skipping region '%s' with no faces\n", pstr ? pstr : "");
	    if (pstr) bu_free(pstr, "jt path string");
	}
	return;
    }

    nmg_triangulate_model(r->m_p, state->vlfree, &state->gcv_options->calculational_tolerance);

    /* EXP-030: record this region's object identity before adding its triangles,
     * capturing the leaf path name and any explicit region color (from the tree
     * state's inherited material) so the names/colors survive into JT metadata
     * instead of being flattened away. */
    jt_region_info info;
    char *pstr = db_path_to_string(pathp);
    if (pstr) { info.name = pstr; bu_free(pstr, "jt path string"); }
    if (tsp && tsp->ts_mater.ma_color_valid) {
	info.has_color = true;
	for (int c = 0; c < 3; ++c)
	    info.color[c] = tsp->ts_mater.ma_color[c];
    }
    /* EXP-008: capture the region's material/shader name (from the inherited tree
     * state) and its density (from the leaf region's 'density' attribute, if any)
     * so the JT Meta Data property map can record the physical material. */
    if (tsp && tsp->ts_mater.ma_shader && tsp->ts_mater.ma_shader[0])
	info.material = tsp->ts_mater.ma_shader;
    if (state->dbip && pathp->fp_len > 0) {
	struct directory *leaf = DB_FULL_PATH_CUR_DIR(pathp);
	if (leaf != RT_DIR_NULL) {
	    struct bu_attribute_value_set ravs;
	    bu_avs_init_empty(&ravs);
	    if (db5_get_attributes(state->dbip, &ravs, leaf) == 0) {
		const char *d = bu_avs_get(&ravs, "density");
		if (d && d[0]) {
		    char *endp = NULL;
		    const double dv = strtod(d, &endp);
		    if (endp != d && dv > 0.0)
			info.density = dv;
		}
	    }
	    bu_avs_free(&ravs);
	}
    }
    /* EXP-046: capture the leaf's accumulated 4x4 placement matrix from the tree
     * state so its spatial relationship (transform from model space) travels with
     * the region.  db_walk_tree bakes this transform into the tessellated geometry,
     * so it is recorded for provenance/assembly reconstruction rather than applied
     * again.  An identity (or a non-finite) matrix is treated as "no transform"
     * and omitted from the metadata. */
    if (tsp) {
	bool identity = true;
	bool finite = true;
	for (int i = 0; i < 16 && finite; ++i) {
	    const double m = tsp->ts_mat[i];
	    if (!(m == m) || m > 1e300 || m < -1e300)
		finite = false;
	    const double id = ((i % 5) == 0) ? 1.0 : 0.0;   /* diagonal elements are 1 */
	    if (std::fabs(m - id) > 1e-12)
		identity = false;
	}
	if (finite && !identity) {
	    info.has_matrix = true;
	    for (int i = 0; i < 16; ++i)
		info.matrix[i] = tsp->ts_mat[i];
	}
    }
    info.first_triangle = state->triangles;

    size_t region_triangles = 0;
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;
	NMG_CK_SHELL(s);
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		continue;
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		if (add_triangle_from_nmg(lu, state->coords)) {
		    state->triangles++;
		    ++region_triangles;
		}
	    }
	}
    }
    info.triangle_count = region_triangles;
    if (region_triangles)
	state->regions.push_back(info);
}

/* EXP-005: build the little-endian encoding of a standard JT LSG element type
 * GUID (dddddddd-2ac8-11d1 + LSG_TAIL_STD), matching the reader's lsg_type()
 * classification.  Used to stamp the PartitionNode element type below. */
static jt::Guid
jt_lsg_std_guid(uint32_t d1)
{
    static const unsigned char tail[8] = {0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97};
    jt::Guid g{};
    for (int i = 0; i < 4; ++i) g[i] = static_cast<unsigned char>((d1 >> (i * 8)) & 0xff);
    g[4] = 0xc8; g[5] = 0x2a;          /* 0x2ac8 little-endian */
    g[6] = 0xd1; g[7] = 0x11;          /* 0x11d1 little-endian */
    for (int i = 0; i < 8; ++i) g[8 + i] = tail[i];
    return g;
}

/* EXP-005/007: emit a JT Logical Scene Graph (LSG) segment (segment type 1) into
 * 'segment', returning its content GUID in 'guid_out'.  The segment carries a root
 * PartitionNode whose children are one TriStripShape node per shape segment; each
 * shape node is bound through the Property Table to a Late Loaded Atom holding its
 * geometry segment GUID (the reader resolves a shape node's geometry exactly this
 * way in File::scene_graph).
 *
 * EXP-007: this is a real (single-level) instance graph -- the Partition (hierarchy
 * depth 0, object id 1) contains N Shape nodes (depth 1, object ids 2..N+1), so the
 * reader's scene-graph traversal recovers one placed shape instance per segment
 * (with the region name) rather than falling back to the flat TOC scan.  The whole
 * element/atom/property-table buffer is zlib-wrapped exactly as the reader's LSG
 * inflate path expects (compress flag 2, algorithm 2).
 *
 * 'shape_names', when the same length as 'shape_guids', supplies a per-shape node
 * name (falling back to the partition name); a shorter/empty vector names every
 * shape after the partition. */
static bool
write_lsg_segment(const jt_write_options &opts, const std::vector<jt::Guid> &shape_guids,
    const std::string &partition_name, const std::vector<std::string> &shape_names,
    jt::ByteBuffer &segment, jt::Guid &guid_out, std::string &error)
{
    /* Object id namespace within the LSG (the first I32 of every element body).
     * EXP-007: the partition is depth 0 (id 1) and each shape node is depth 1
     * (ids 2..N+1), so the object id ordering tags the hierarchy level. */
    int32_t next_id = 1;
    const int32_t partition_id = next_id++;
    std::vector<int32_t> shape_ids;
    for (size_t i = 0; i < shape_guids.size(); ++i)
	shape_ids.push_back(next_id++);
    const int32_t name_key_atom = next_id++;
    const int32_t part_name_atom = next_id++;
    std::vector<int32_t> shape_name_atoms;
    for (size_t i = 0; i < shape_guids.size(); ++i)
	shape_name_atoms.push_back(next_id++);
    std::vector<int32_t> late_atom_ids;
    for (size_t i = 0; i < shape_guids.size(); ++i)
	late_atom_ids.push_back(next_id++);

    /* Helper: append a logical element (u32 length, 16-byte type GUID, u8
     * base_type, then body) to a buffer.  element_length counts GUID + base_type
     * + body (i.e. everything after the length field). */
    auto put_element = [](jt::ByteBuffer &buf, const jt::Guid &type_id, const jt::ByteBuffer &body) {
	const uint32_t element_length = static_cast<uint32_t>(body.size() + 16 + 1);
	jt_put_u32(buf, element_length);
	buf.insert(buf.end(), type_id.begin(), type_id.end());
	buf.push_back(0);   /* base type */
	buf.insert(buf.end(), body.begin(), body.end());
    };
    auto put_eoe = [](jt::ByteBuffer &buf) {
	jt_put_u32(buf, 16);
	buf.insert(buf.end(), 16, 0xff);
    };

    jt::ByteBuffer lsg;   /* the inflated LSG data */

    /* --- Graph element section: root PartitionNode + one Shape node per segment. --- */
    {
	jt::ByteBuffer body;
	jt_put_i32(body, partition_id);                        /* object id */
	jt_put_u32(body, 0);                                   /* node flags */
	jt_put_i32(body, 0);                                   /* attribute count */
	jt_put_i32(body, static_cast<int32_t>(shape_ids.size()));   /* child count (EXP-007) */
	for (int32_t child : shape_ids)                        /* child ids: the shape nodes */
	    jt_put_i32(body, child);
	jt_put_i32(body, 0);                                   /* partition flags */
	jt_put_i32(body, 0);                                   /* file name mbstring: length 0 */
	put_element(lsg, jt_lsg_std_guid(0x10dd103e), body);   /* Partition GUID */
    }
    /* EXP-007: each shape node is a TriStripShape base node (object id, flags,
     * empty attribute list); its geometry is bound via the property table below. */
    for (int32_t shape_id : shape_ids) {
	jt::ByteBuffer body;
	jt_put_i32(body, shape_id);   /* object id */
	jt_put_u32(body, 0);          /* node flags */
	jt_put_i32(body, 0);          /* attribute count */
	put_element(lsg, jt_lsg_std_guid(0x10dd1077), body);   /* TriStripShape GUID */
    }
    put_eoe(lsg);   /* end of graph elements */

    /* --- Atom pool section: name key/value string atoms + late-loaded atoms. --- */
    auto put_string_atom = [&](int32_t id, const std::string &text) {
	jt::ByteBuffer body;
	jt_put_i32(body, id);             /* object id */
	jt_put_u32(body, 0);             /* state */
	jt_put_i32(body, static_cast<int32_t>(text.size()));   /* mbstring length */
	for (char ch : text) jt_put_u16(body, static_cast<uint16_t>(static_cast<unsigned char>(ch)));
	put_element(lsg, jt_lsg_std_guid(0x10dd106e), body);   /* StringAtom GUID */
    };
    put_string_atom(name_key_atom, "JT_PROP_NAME");
    const std::string part_name = partition_name.empty() ? std::string("brlcad") : partition_name;
    put_string_atom(part_name_atom, part_name);
    for (size_t i = 0; i < shape_guids.size(); ++i) {
	std::string nm = (i < shape_names.size() && !shape_names[i].empty()) ? shape_names[i] : part_name;
	/* Keep the atom ASCII/single-token so the reader's mbstring parse is clean. */
	std::string clean;
	for (char ch : nm)
	    if (static_cast<unsigned char>(ch) >= 0x20 && static_cast<unsigned char>(ch) < 0x7f)
		clean.push_back(ch);
	if (clean.empty()) clean = "jt_shape";
	put_string_atom(shape_name_atoms[i], clean);
    }
    {
	static const unsigned char late_tail[8] = {0xa3, 0xa7, 0x00, 0xaa, 0x00, 0xd1, 0x09, 0x54};
	jt::Guid late_guid{};
	const uint32_t d1 = 0xe0b05be5;
	for (int i = 0; i < 4; ++i) late_guid[i] = static_cast<unsigned char>((d1 >> (i * 8)) & 0xff);
	late_guid[4] = 0xbd; late_guid[5] = 0xfb;   /* 0xfbbd */
	late_guid[6] = 0xd1; late_guid[7] = 0x11;   /* 0x11d1 */
	for (int i = 0; i < 8; ++i) late_guid[8 + i] = late_tail[i];
	for (size_t i = 0; i < shape_guids.size(); ++i) {
	    jt::ByteBuffer body;
	    jt_put_i32(body, late_atom_ids[i]);   /* object id */
	    jt_put_u32(body, 0);                 /* state */
	    body.insert(body.end(), shape_guids[i].begin(), shape_guids[i].end());   /* referenced segment GUID */
	    jt_put_i32(body, 7);                 /* referenced segment type: Shape LOD */
	    put_element(lsg, late_guid, body);   /* LateLoadedAtom GUID */
	}
    }
    put_eoe(lsg);   /* end of atom pool */

    /* --- Property Table: partition name + per-shape {name, geometry reference}. ---
     * The reader resolves a shape node's geometry by finding a property whose value
     * atom is a Late Loaded Atom (node_segment), and its name via the JT_PROP_NAME
     * key (node_name); one property-table node per graph node. */
    jt_put_u16(lsg, 1);   /* property table version */
    jt_put_i32(lsg, static_cast<int32_t>(1 + shape_ids.size()));   /* nodes with properties */
    /* Partition: JT_PROP_NAME -> partition name. */
    jt_put_i32(lsg, partition_id);
    jt_put_i32(lsg, name_key_atom);
    jt_put_i32(lsg, part_name_atom);
    jt_put_i32(lsg, 0);   /* terminate this node's property list */
    /* Each shape node: JT_PROP_NAME -> shape name, and JT_PROP_NAME -> late atom
     * (the reader keys geometry off the value atom being a late atom, not the key). */
    for (size_t i = 0; i < shape_ids.size(); ++i) {
	jt_put_i32(lsg, shape_ids[i]);
	jt_put_i32(lsg, name_key_atom);
	jt_put_i32(lsg, shape_name_atoms[i]);
	jt_put_i32(lsg, name_key_atom);
	jt_put_i32(lsg, late_atom_ids[i]);
	jt_put_i32(lsg, 0);   /* terminate this node's property list */
    }

    /* --- Wrap: segment header (GUID, type 1, length) + zlib framing. --- */
    if (lsg.size() >= static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
	error = "LSG segment exceeds the 32-bit length limit";
	return false;
    }
    guid_out = jt_generate_guid(lsg);

    uLongf bound = compressBound(static_cast<uLong>(lsg.size()));
    jt::ByteBuffer packed(static_cast<size_t>(bound));
    uLongf produced = bound;
    int level = opts.compress_level;
    if (level < -1) level = -1;
    if (level > 9) level = 9;
    if (compress2(packed.data(), &produced, lsg.data(), static_cast<uLong>(lsg.size()), level) != Z_OK ||
	produced == 0 || produced >= static_cast<uLongf>(std::numeric_limits<int32_t>::max())) {
	error = "cannot deflate LSG segment";
	return false;
    }

    segment.clear();
    segment.insert(segment.end(), guid_out.begin(), guid_out.end());   /* segment GUID */
    jt_put_u32(segment, 1);                                            /* segment type: LSG */
    const size_t seg_length_pos = segment.size();
    jt_put_u32(segment, 0);                                            /* placeholder length */
    jt_put_i32(segment, 2);                                            /* compression flag: zlib */
    jt_put_i32(segment, static_cast<int32_t>(produced) + 1);          /* compressed length (+algorithm byte) */
    segment.push_back(2);                                              /* algorithm: zlib */
    segment.insert(segment.end(), packed.begin(), packed.begin() + static_cast<size_t>(produced));

    if (segment.size() >= 0xFFFFFFFFu) {
	error = "LSG segment exceeds the 32-bit length limit";
	return false;
    }
    const uint32_t segment_length = static_cast<uint32_t>(segment.size());
    segment[seg_length_pos + 0] = static_cast<unsigned char>(segment_length & 0xff);
    segment[seg_length_pos + 1] = static_cast<unsigned char>((segment_length >> 8) & 0xff);
    segment[seg_length_pos + 2] = static_cast<unsigned char>((segment_length >> 16) & 0xff);
    segment[seg_length_pos + 3] = static_cast<unsigned char>((segment_length >> 24) & 0xff);
    return true;
}

/* EXP-008/018: append a length-prefixed ASCII text body as a stand-alone JT data
 * segment of the given type and frame it with the standard segment header (16-byte
 * content GUID + u32 type + u32 total length).  The body is stored verbatim as a
 * u32 byte count followed by the text bytes; the reader's flat shape scan only
 * looks at segment types 6..16 and its scene-graph walk only reads type 1, so a
 * type-2 (File Description) or type-4 (Meta Data) segment is registered in the TOC
 * yet safely ignored on import.  Returns false / sets error only on the 32-bit
 * length overflow guard. */
static bool
write_text_segment(uint32_t seg_type, const std::string &text, jt::ByteBuffer &segment,
    jt::Guid &guid_out, std::string &error)
{
    jt::ByteBuffer payload;
    if (text.size() >= static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
	error = "JT metadata text segment exceeds the 32-bit length limit";
	return false;
    }
    jt_put_u32(payload, static_cast<uint32_t>(text.size()));
    for (char ch : text)
	payload.push_back(static_cast<unsigned char>(ch));

    guid_out = jt_generate_guid(payload);

    segment.clear();
    segment.insert(segment.end(), guid_out.begin(), guid_out.end());   /* segment GUID */
    jt_put_u32(segment, seg_type);                                     /* segment type */
    const size_t seg_length_pos = segment.size();
    jt_put_u32(segment, 0);                                            /* placeholder length */
    segment.insert(segment.end(), payload.begin(), payload.end());

    if (segment.size() >= 0xFFFFFFFFu) {
	error = "JT metadata text segment exceeds the 32-bit length limit";
	return false;
    }
    const uint32_t segment_length = static_cast<uint32_t>(segment.size());
    segment[seg_length_pos + 0] = static_cast<unsigned char>(segment_length & 0xff);
    segment[seg_length_pos + 1] = static_cast<unsigned char>((segment_length >> 8) & 0xff);
    segment[seg_length_pos + 2] = static_cast<unsigned char>((segment_length >> 16) & 0xff);
    segment[seg_length_pos + 3] = static_cast<unsigned char>((segment_length >> 24) & 0xff);
    return true;
}

/* EXP-018: emit a JT File Description segment (segment type 2) carrying the export
 * provenance: product name, manufacturer, format version, unit string, an export
 * timestamp, and a BRL-CAD writer tag.  The content is a compact ASCII key=value
 * block (one pair per line) so external tools can read the traceability data
 * without a JT toolkit; the reader ignores type-2 segments.  Registered as its own
 * TOC entry by the caller. */
static bool
write_file_description_segment(const jt_write_options &opts, const char *unit_name,
    jt::ByteBuffer &segment, jt::Guid &guid_out, std::string &error)
{
    std::string ver = "8.0";
    if (opts.version == 9) ver = "9.5";
    else if (opts.version == 10) ver = "10.3";

    /* EXP-018: an ISO-8601-ish UTC export timestamp for traceability.  time()/gmtime
     * are ASCII-only and portable; a failure leaves the field empty rather than
     * emitting garbage. */
    char stamp[32];
    stamp[0] = '\0';
    time_t now = time(NULL);
    if (now != static_cast<time_t>(-1)) {
	struct tm tmv;
#if defined(_WIN32)
	if (gmtime_s(&tmv, &now) == 0)
	    strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &tmv);
#else
	if (gmtime_r(&now, &tmv))
	    strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &tmv);
#endif
    }

    std::string text;
    text += "product=BRL-CAD Model\n";
    text += "manufacturer=BRL-CAD\n";
    text += "version=" + ver + "\n";
    text += std::string("units=") + (unit_name ? unit_name : "mm") + "\n";
    if (stamp[0])
	text += std::string("timestamp=") + stamp + "\n";
    text += "generator=BRL-CAD JT Writer\n";

    /* EXP-038: coordinate-reference-system / spatial-units awareness.  BRL-CAD
     * models are stored in a right-handed Cartesian model space with no geodetic
     * datum, so the CRS is an engineering (local) coordinate system whose linear
     * unit is the .g file's own unit (unit_name above).  Record it as a small,
     * self-describing block so a downstream tool can reason about the spatial
     * frame; a custom projection or an EPSG code (e.g. "EPSG:0" for an
     * engineering CRS) can be threaded through opts here in the future.  The
     * reader ignores type-2 segment text, so this is pure provenance. */
    text += "crs_type=engineering\n";
    text += std::string("crs_linear_unit=") + (unit_name ? unit_name : "mm") + "\n";
    text += "crs_axes=X-right,Y-up,Z-out (right-handed)\n";
    if (!opts.crs_code.empty())
	text += "crs_code=" + opts.crs_code + "\n";

    return write_text_segment(2, text, segment, guid_out, error);
}

/* EXP-008: property-record value type tags used in the JT 9+ Meta Data property
 * map (below).  Each per-region record is a list of {key string, typed value}
 * pairs so a reader can recover the region's name, RGB color, material name, and
 * density without positional parsing. */
enum jt_meta_value_type {
    JT_META_VAL_STRING = 1,   /* u32 length + UTF-8/ASCII bytes */
    JT_META_VAL_RGB    = 2,   /* three f32 (r, g, b), each 0..1 */
    JT_META_VAL_FLOAT  = 3,   /* one f32 */
    JT_META_VAL_MATRIX = 4    /* EXP-046: sixteen f32 (row-major 4x4 transform) */
};

/* EXP-008: append a JT-style length-prefixed ASCII string (u32 byte count then
 * the bytes) to a property-record buffer, stripping control/non-ASCII bytes so
 * the record stays clean. */
static void
jt_put_meta_string(jt::ByteBuffer &buf, const std::string &s)
{
    std::string clean;
    for (char ch : s)
	if (static_cast<unsigned char>(ch) >= 0x20 && static_cast<unsigned char>(ch) < 0x7f)
	    clean.push_back(ch);
    jt_put_u32(buf, static_cast<uint32_t>(clean.size()));
    for (char ch : clean)
	buf.push_back(static_cast<unsigned char>(ch));
}

/* EXP-008: append one typed {key, value} property pair to a per-region record. */
static void
jt_put_meta_string_prop(jt::ByteBuffer &buf, const char *key, const std::string &value)
{
    jt_put_meta_string(buf, key);
    buf.push_back(static_cast<unsigned char>(JT_META_VAL_STRING));
    jt_put_meta_string(buf, value);
}

/* EXP-008: emit a JT Meta Data segment (segment type 4) recording per-region
 * appearance and material as a JT 9+ property map: for each region a record with
 * typed properties -- name (string), color (RGB, 0..1), material (string), and
 * density (float) -- present only when the value was captured.  The whole
 * property-record block is zlib-deflated (RFC 1950 wrapper) and framed exactly
 * like the LSG segment's compressed payload (compression flag 2, algorithm byte
 * 2), following the JT compressed-segment convention.  The reader ignores type-4
 * segments, so the binding travels with the file for downstream tools without
 * affecting geometry import.  Registered as its own TOC entry by the caller.
 * Returns false with an empty 'segment' (and no error) when there is nothing to
 * record, so the caller can skip emitting an empty metadata segment. */
static bool
write_metadata_segment(const jt_write_options &opts, const std::vector<jt_region_info> &regions,
    jt::ByteBuffer &segment, jt::Guid &guid_out, std::string &error)
{
    if (regions.empty()) {
	segment.clear();
	return false;
    }

    /* --- Build the uncompressed property-record block. --- */
    jt::ByteBuffer meta;
    jt_put_u16(meta, 1);                                       /* property-map version */
    jt_put_u32(meta, static_cast<uint32_t>(regions.size()));  /* record count */
    for (size_t idx = 0; idx < regions.size(); ++idx) {
	const jt_region_info &ri = regions[idx];
	jt_put_i32(meta, static_cast<int32_t>(idx));          /* object id / record index */
	/* Count the typed properties this region actually carries, then emit them. */
	uint32_t prop_count = 1;   /* name is always present */
	if (ri.has_color) ++prop_count;
	if (!ri.material.empty()) ++prop_count;
	if (ri.density > 0.0) ++prop_count;
	if (ri.has_matrix) ++prop_count;   /* EXP-046 */
	jt_put_u32(meta, prop_count);
	jt_put_meta_string_prop(meta, "JT_PROP_NAME", ri.name);
	if (ri.has_color) {
	    jt_put_meta_string(meta, "JT_PROP_COLOR");
	    meta.push_back(static_cast<unsigned char>(JT_META_VAL_RGB));
	    for (int c = 0; c < 3; ++c)
		jt_put_f32(meta, ri.color[c]);
	}
	if (!ri.material.empty())
	    jt_put_meta_string_prop(meta, "JT_PROP_MATERIAL", ri.material);
	if (ri.density > 0.0) {
	    jt_put_meta_string(meta, "JT_PROP_DENSITY");
	    meta.push_back(static_cast<unsigned char>(JT_META_VAL_FLOAT));
	    jt_put_f32(meta, static_cast<float>(ri.density));
	}
	if (ri.has_matrix) {
	    /* EXP-046: the region's 4x4 placement matrix (row-major, 16 f32).  The JT
	     * LSG PartitionNode transform binding is the natural home for this; the
	     * reader ignores the type-4 metadata segment, so it is recorded here as a
	     * portable property so the spatial relationship survives export. */
	    jt_put_meta_string(meta, "JT_PROP_TRANSFORM");
	    meta.push_back(static_cast<unsigned char>(JT_META_VAL_MATRIX));
	    for (int m = 0; m < 16; ++m)
		jt_put_f32(meta, static_cast<float>(ri.matrix[m]));
	}
    }

    if (meta.size() >= static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
	error = "JT Meta Data segment exceeds the 32-bit length limit";
	return false;
    }
    guid_out = jt_generate_guid(meta);

    /* --- zlib-deflate the property block (RFC 1950 wrapper). --- */
    uLongf bound = compressBound(static_cast<uLong>(meta.size()));
    jt::ByteBuffer packed(static_cast<size_t>(bound));
    uLongf produced = bound;
    int level = opts.compress_level;
    if (level < -1) level = -1;
    if (level > 9) level = 9;
    if (compress2(packed.data(), &produced, meta.data(), static_cast<uLong>(meta.size()), level) != Z_OK ||
	produced == 0 || produced >= static_cast<uLongf>(std::numeric_limits<int32_t>::max())) {
	error = "cannot deflate JT Meta Data segment";
	return false;
    }

    /* --- Frame: segment header (GUID, type 4, length) + zlib framing. --- */
    segment.clear();
    segment.insert(segment.end(), guid_out.begin(), guid_out.end());   /* segment GUID */
    jt_put_u32(segment, 4);                                            /* segment type: Meta Data */
    const size_t seg_length_pos = segment.size();
    jt_put_u32(segment, 0);                                            /* placeholder length */
    jt_put_i32(segment, 2);                                            /* compression flag: zlib */
    jt_put_i32(segment, static_cast<int32_t>(produced) + 1);          /* compressed length (+algorithm byte) */
    segment.push_back(2);                                              /* algorithm: zlib */
    segment.insert(segment.end(), packed.begin(), packed.begin() + static_cast<size_t>(produced));

    if (segment.size() >= 0xFFFFFFFFu) {
	error = "JT Meta Data segment exceeds the 32-bit length limit";
	return false;
    }
    const uint32_t segment_length = static_cast<uint32_t>(segment.size());
    segment[seg_length_pos + 0] = static_cast<unsigned char>(segment_length & 0xff);
    segment[seg_length_pos + 1] = static_cast<unsigned char>((segment_length >> 8) & 0xff);
    segment[seg_length_pos + 2] = static_cast<unsigned char>((segment_length >> 16) & 0xff);
    segment[seg_length_pos + 3] = static_cast<unsigned char>((segment_length >> 24) & 0xff);
    return true;
}

/* EXP-029: per-corner shading normals for a flat triangle coordinate stream
 * (nine doubles per triangle).  The tri-strip writer emits one unique vertex per
 * corner, so nothing is topologically shared; to recover smooth vertex normals we
 * accumulate each triangle's (area-weighted, un-normalized) face normal at every
 * corner that shares a vertex *position*, then normalize.  Corners are matched by
 * position through a quantized spatial hash so numerically-identical vertices are
 * merged even without shared NMG topology (the Deering codec the caller feeds
 * these into re-normalizes, so an exact merge is not required).  A vertex with no
 * finite contribution falls back to +Z.  The result is parallel to 'coords'
 * (three doubles per corner, 3*triangles corners).
 *
 * This averaging is the EXP-029 refinement of the earlier per-face assignment:
 * accumulate adjacent face normals at each vertex, normalize, then hand off to the
 * sextant/octant/theta/psi Deering quantizer in build_lod_element_data. */
static std::vector<double>
jt_face_corner_normals(const std::vector<double> &coords)
{
    const size_t corner_count = coords.size() / 3;
    std::vector<double> normals(coords.size(), 0.0);
    if (corner_count == 0)
	return normals;

    /* Spatial-hash vertex positions so corners at the same location share an
     * accumulator index.  A relative epsilon derived from the coordinate spread
     * keeps distinct-but-close vertices apart while merging exact duplicates. */
    double mn[3], mx[3];
    const bool have_bounds = jt_vertex_bounds(coords, mn, mx);
    double span = 0.0;
    if (have_bounds)
	for (int c = 0; c < 3; ++c) {
	    const double s = mx[c] - mn[c];
	    if (s > span) span = s;
	}
    const double quant = (span > 0.0) ? (span / 1.0e6) : 1.0e-9;

    auto key_of = [&](const double *p) {
	long long k[3];
	for (int c = 0; c < 3; ++c)
	    k[c] = static_cast<long long>(std::llround(p[c] / quant));
	/* Combine the three lattice coordinates into one hash key string; a string
	 * map keeps this dependency-free and exact (no float compare). */
	return std::to_string(k[0]) + "," + std::to_string(k[1]) + "," + std::to_string(k[2]);
    };

    std::map<std::string, size_t> vertex_index;
    std::vector<size_t> corner_vertex(corner_count, 0);
    std::vector<double> accum;   /* three doubles per unique vertex */
    for (size_t i = 0; i < corner_count; ++i) {
	const std::string key = key_of(&coords[i * 3]);
	auto it = vertex_index.find(key);
	size_t vi;
	if (it == vertex_index.end()) {
	    vi = accum.size() / 3;
	    vertex_index.emplace(key, vi);
	    accum.insert(accum.end(), 3, 0.0);
	} else {
	    vi = it->second;
	}
	corner_vertex[i] = vi;
    }

    /* Accumulate each triangle's raw (area-weighted) face normal at its three
     * corner vertices. */
    for (size_t t = 0; t + 8 < coords.size(); t += 9) {
	const double *a = &coords[t];
	const double *b = &coords[t + 3];
	const double *c = &coords[t + 6];
	const double e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
	const double e1[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
	const double n[3] = {e0[1] * e1[2] - e0[2] * e1[1], e0[2] * e1[0] - e0[0] * e1[2],
	    e0[0] * e1[1] - e0[1] * e1[0]};   /* length == 2*area, so this area-weights */
	if (!(n[0] == n[0]) || !(n[1] == n[1]) || !(n[2] == n[2]))
	    continue;
	for (int corner = 0; corner < 3; ++corner) {
	    const size_t vi = corner_vertex[t / 3 + corner];
	    for (int c2 = 0; c2 < 3; ++c2)
		accum[vi * 3 + c2] += n[c2];
	}
    }

    /* Normalize each accumulated vertex normal, then scatter back to every corner. */
    for (size_t vi = 0; vi < accum.size() / 3; ++vi) {
	double *v = &accum[vi * 3];
	const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (len > 1e-12) { v[0] /= len; v[1] /= len; v[2] /= len; }
	else { v[0] = 0.0; v[1] = 0.0; v[2] = 1.0; }
    }
    for (size_t i = 0; i < corner_count; ++i) {
	const size_t vi = corner_vertex[i];
	for (int c2 = 0; c2 < 3; ++c2)
	    normals[i * 3 + c2] = accum[vi * 3 + c2];
    }
    return normals;
}

/* EXP-009: quantize one unit normal into the Sun/Deering angular codec, the inverse
 * of the reader's deering_decode_normal() (jt_parser.cpp): pick the octant from the
 * component signs, then search the six sextant axis-permutations for the one that
 * folds the absolute vector into the first-octant patch (theta in [0, pi/4], psi in
 * [0, psi_max]) and quantize (theta, psi) to 'bits'-wide codes.  A best-effort
 * reconstruction is acceptable (the reader re-normalizes), so on no exact fit the
 * closest patch is used.  Fills sextant/octant/theta_code/psi_code. */
static void
jt_deering_encode_normal(const double n_in[3], unsigned bits, int32_t &sextant, int32_t &octant,
    int32_t &theta_code, int32_t &psi_code)
{
    const int levels = bits > 0 ? (1 << bits) : 1;
    const double quarter_pi = 0.785398163397448;   /* pi/4 */
    const double psi_max = 0.615479709;
    /* Normalize defensively. */
    double n[3] = {n_in[0], n_in[1], n_in[2]};
    const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (len > 1e-12) { n[0] /= len; n[1] /= len; n[2] /= len; }
    else { n[0] = 0.0; n[1] = 0.0; n[2] = 1.0; }

    /* Octant from the signs (matches the decoder's sign flips). */
    octant = 0;
    if (n[0] < 0.0) octant |= 0x4;
    if (n[1] < 0.0) octant |= 0x2;
    if (n[2] < 0.0) octant |= 0x1;
    const double a[3] = {std::fabs(n[0]), std::fabs(n[1]), std::fabs(n[2])};

    /* The decoder maps a folded first-octant vector xyz (xyz[0]=sin psi,
     * xyz[1]=cos psi sin theta, xyz[2]=cos psi cos theta) through one of six axis
     * permutations to get n.  Invert each permutation to recover the candidate xyz
     * from the absolute vector, then keep the sextant whose xyz lies in the valid
     * folded patch (smallest out-of-range penalty). */
    static const int perm[6][3] = {
	{0, 1, 2}, {2, 0, 1}, {1, 2, 0}, {0, 2, 1}, {2, 1, 0}, {1, 0, 2}
    };
    double best_penalty = 1e30;
    sextant = 0; theta_code = 0; psi_code = 0;
    for (int s = 0; s < 6; ++s) {
	/* n[k] = a[perm[s][k]] in the decoder; so xyz[perm[s][k]] = a[k] gives the
	 * pre-permutation folded vector for this sextant. */
	double xyz[3];
	for (int k = 0; k < 3; ++k)
	    xyz[perm[s][k]] = a[k];
	double px = xyz[0], py = xyz[1], pz = xyz[2];
	if (px > 1.0) px = 1.0;
	double psi = std::asin(px);
	double theta = std::atan2(py, pz);
	/* Penalize a candidate that leaves the folded ranges; the correct sextant
	 * keeps both angles inside their patch. */
	double penalty = 0.0;
	if (theta < 0.0) penalty += -theta;
	if (theta > quarter_pi) penalty += theta - quarter_pi;
	if (psi < 0.0) penalty += -psi;
	if (psi > psi_max) penalty += psi - psi_max;
	if (penalty < best_penalty) {
	    best_penalty = penalty;
	    sextant = s;
	    if (theta < 0.0) theta = 0.0;
	    if (theta > quarter_pi) theta = quarter_pi;
	    if (psi < 0.0) psi = 0.0;
	    if (psi > psi_max) psi = psi_max;
	    if (levels > 1) {
		const double top = static_cast<double>(levels - 1);
		double tc = theta / quarter_pi * top;
		double pc = psi / psi_max * top;
		if (tc < 0.0) tc = 0.0;
		if (tc > top) tc = top;
		if (pc < 0.0) pc = 0.0;
		if (pc > top) pc = top;
		theta_code = static_cast<int32_t>(std::llround(tc));
		psi_code = static_cast<int32_t>(std::llround(pc));
	    } else {
		theta_code = 0;
		psi_code = 0;
	    }
	}
    }
}

/* EXP-010: coarse level-of-detail generation by vertex clustering.  Snap every
 * triangle corner to a uniform spatial grid (cell size derived from the bounding
 * box and 'ratio': a smaller ratio -> a coarser grid -> fewer distinct corners ->
 * fewer surviving triangles), then drop any triangle that collapses to a shared
 * cell.  Returns a flat nine-doubles-per-triangle coordinate stream for the
 * decimated mesh.  Robust and dependency-free (no NMG); the result is a genuine
 * subset-resolution mesh suitable for a JT LOD record.  'ratio' in (0,1); a ratio
 * >= 1 (or an empty/degenerate input) returns the input unchanged. */
static std::vector<double>
jt_decimate_coords(const std::vector<double> &coords, double ratio)
{
    if (!(ratio > 0.0) || ratio >= 1.0 || coords.size() < 9)
	return coords;
    double mn[3], mx[3];
    if (!jt_vertex_bounds(coords, mn, mx))
	return coords;
    double span = mx[0] - mn[0];
    for (int c = 1; c < 3; ++c) {
	const double s = mx[c] - mn[c];
	if (s > span) span = s;
    }
    if (!(span > 0.0))
	return coords;
    /* Grid resolution scales with sqrt(ratio) so the surviving triangle count
     * tracks the requested fraction: fewer cells per axis -> fewer triangles. */
    const size_t triangles = coords.size() / 9;
    double cells = std::sqrt(static_cast<double>(triangles) * ratio);
    if (cells < 2.0) cells = 2.0;
    const double cell = span / cells;
    if (!(cell > 0.0))
	return coords;
    const double inv = 1.0 / cell;
    auto snap = [&](const double *p, double *out) {
	for (int c = 0; c < 3; ++c)
	    out[c] = mn[c] + (std::floor((p[c] - mn[c]) * inv) + 0.5) * cell;
    };
    std::vector<double> out;
    out.reserve(coords.size());
    for (size_t t = 0; t + 8 < coords.size(); t += 9) {
	double a[3], b[3], cc[3];
	snap(&coords[t], a); snap(&coords[t + 3], b); snap(&coords[t + 6], cc);
	/* Drop a triangle whose snapped corners collapsed (zero area). */
	const double e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
	const double e1[3] = {cc[0] - a[0], cc[1] - a[1], cc[2] - a[2]};
	const double cx = e0[1] * e1[2] - e0[2] * e1[1];
	const double cy = e0[2] * e1[0] - e0[0] * e1[2];
	const double cz = e0[0] * e1[1] - e0[1] * e1[0];
	if (std::sqrt(cx * cx + cy * cy + cz * cz) < cell * cell * 1e-6)
	    continue;
	for (int k = 0; k < 3; ++k) out.push_back(a[k]);
	for (int k = 0; k < 3; ++k) out.push_back(b[k]);
	for (int k = 0; k < 3; ++k) out.push_back(cc[k]);
    }
    /* If clustering removed everything, keep the original so the LOD still has
     * renderable geometry rather than an empty element. */
    return out.size() >= 9 ? out : coords;
}

/* EXP-009/010: build one JT 8 Tri-Strip Set Shape LOD logical-element payload
 * (everything after the element's base-type/object-id framing) from a flat
 * nine-doubles-per-triangle coordinate stream.  Factored out of build_shape_segment
 * so the segment can carry several LOD elements (EXP-010): the finest LOD first,
 * then progressively coarser meshes.  Returns the payload bytes in 'data'. */
static void
build_lod_element_data(const std::vector<double> &coords, const jt_write_options &opts,
    jt::ByteBuffer &data)
{
    const size_t triangles = coords.size() / 9;
    const uint32_t vertex_count = static_cast<uint32_t>(triangles * 3);

    jt_quantizer quant[3];
    double vmin[3], vmax[3];
    const bool have_bounds = jt_vertex_bounds(coords, vmin, vmax);
    const bool do_quantize = opts.quantize_bits > 0 && have_bounds && vertex_count > 0;
    uint32_t quant_bits = 0u;
    if (do_quantize) {
	quant_bits = opts.quantize_bits > 24 ? 24u : opts.quantize_bits;
	double span = vmax[0] - vmin[0];
	for (int c = 1; c < 3; ++c) {
	    const double s = vmax[c] - vmin[c];
	    if (s > span) span = s;
	}
	const double precision = (span > 0.0) ? span / 65535.0 : 0.0;
	const uint32_t inferred = jt_infer_quant_bits(vmin, vmax, precision);
	if (inferred > 0 && inferred < quant_bits)
	    quant_bits = inferred;
    }
    if (have_bounds) {
	for (int c = 0; c < 3; ++c) {
	    quant[c].minimum = vmin[c];
	    quant[c].maximum = vmax[c];
	    quant[c].bits = quant_bits;
	    quant[c].is_unsigned = opts.unsigned_quant && jt_coords_all_nonneg(coords);
	}
    }

    const bool emit_normals = do_quantize && opts.emit_normals && vertex_count > 0;
    const unsigned normal_bits = (opts.normal_bits < 1) ? 1u : (opts.normal_bits > 16 ? 16u : opts.normal_bits);
    const uint32_t vertex_bindings = emit_normals ? 0x1u : 0u;
    const unsigned char quant_flag = do_quantize ? 1 : 0;

    data.clear();
    jt_put_u16(data, 1);          /* vertex LOD version */
    jt_put_u32(data, vertex_bindings);   /* vertex bindings (EXP-020) */
    data.push_back(quant_flag);   /* quantization params (EXP-002) */
    data.insert(data.end(), 3, 0);
    jt_put_u16(data, 1);          /* tri-strip version */
    jt_put_u16(data, 1);          /* representation version */
    data.push_back(emit_normals ? 1 : 0);   /* normal binding (EXP-009) */
    data.push_back(0);            /* texture binding */
    data.push_back(0);            /* color binding */
    data.push_back(quant_flag);   /* representation quantization (EXP-002) */
    data.insert(data.end(), 3, 0);

    const uint32_t boundary_count = static_cast<uint32_t>(triangles + 1);
    std::vector<int32_t> boundaries(boundary_count);
    for (uint32_t i = 0; i < boundary_count; ++i)
	boundaries[i] = static_cast<int32_t>(i * 3);
    const std::vector<int32_t> boundary_residuals = jt_stride1_encode(boundaries);
    data.push_back(0);            /* Int32 CDP: Null CODEC */
    jt_put_i32(data, static_cast<int32_t>(boundary_count));
    for (int32_t residual : boundary_residuals)
	jt_put_i32(data, residual);

    if (do_quantize) {
	for (int c = 0; c < 3; ++c) {
	    jt_put_f32(data, static_cast<float>(quant[c].minimum));
	    jt_put_f32(data, static_cast<float>(quant[c].maximum));
	    data.push_back(static_cast<unsigned char>(quant_bits));
	}
	const uint64_t maximum_code = quant_bits >= 32 ?
	    0xFFFFFFFFULL : ((1ULL << quant_bits) - 1ULL);
	jt_put_i32(data, static_cast<int32_t>(vertex_count));   /* unique vertex count */
	std::vector<int32_t> codes[3];
	for (int c = 0; c < 3; ++c)
	    codes[c].resize(vertex_count);
	for (uint32_t v = 0; v < vertex_count; ++v) {
	    for (int c = 0; c < 3; ++c) {
		const double range = quant[c].maximum - quant[c].minimum;
		double code = 0.0;
		if (range > 0.0) {
		    code = (coords[static_cast<size_t>(v) * 3 + c] - quant[c].minimum) /
			range * static_cast<double>(maximum_code);
		    if (code < 0.0) code = 0.0;
		    if (code > static_cast<double>(maximum_code)) code = static_cast<double>(maximum_code);
		}
		codes[c][v] = static_cast<int32_t>(std::llround(code));
	    }
	}
	for (int c = 0; c < 3; ++c)
	    jt_put_null_cdp(data, jt_lag1_encode(codes[c]));
	if (emit_normals) {
	    const std::vector<double> corner_normals = jt_face_corner_normals(coords);
	    std::vector<int32_t> nstreams[4];
	    for (int stream = 0; stream < 4; ++stream)
		nstreams[stream].resize(vertex_count);
	    for (uint32_t v = 0; v < vertex_count; ++v) {
		double nv[3] = {0.0, 0.0, 1.0};
		if (static_cast<size_t>(v) * 3 + 2 < corner_normals.size()) {
		    nv[0] = corner_normals[static_cast<size_t>(v) * 3 + 0];
		    nv[1] = corner_normals[static_cast<size_t>(v) * 3 + 1];
		    nv[2] = corner_normals[static_cast<size_t>(v) * 3 + 2];
		}
		int32_t sextant = 0, octant = 0, theta_code = 0, psi_code = 0;
		jt_deering_encode_normal(nv, normal_bits, sextant, octant, theta_code, psi_code);
		nstreams[0][v] = sextant;
		nstreams[1][v] = octant;
		nstreams[2][v] = theta_code;
		nstreams[3][v] = psi_code;
	    }
	    data.push_back(static_cast<unsigned char>(normal_bits));
	    jt_put_i32(data, static_cast<int32_t>(vertex_count));
	    for (int stream = 0; stream < 4; ++stream)
		jt_put_null_cdp(data, jt_lag1_encode(nstreams[stream]));
	}
	std::vector<int32_t> indices(vertex_count);
	for (uint32_t v = 0; v < vertex_count; ++v)
	    indices[v] = static_cast<int32_t>(v);
	jt_put_null_cdp(data, jt_stripindex_encode(indices));
    } else {
	const uint32_t raw_bytes = vertex_count * 3u * 4u;
	jt::ByteBuffer raw;
	raw.reserve(raw_bytes);
	for (size_t i = 0; i < coords.size(); ++i)
	    jt_put_f32(raw, static_cast<float>(coords[i]));

	jt_put_i32(data, static_cast<int32_t>(raw_bytes));
	bool wrote_compressed = false;
	if (opts.compress_vertices && !raw.empty()) {
	    uLongf bound = compressBound(static_cast<uLong>(raw.size()));
	    jt::ByteBuffer packed(static_cast<size_t>(bound));
	    uLongf produced = bound;
	    int level = opts.compress_level;
	    if (level < -1) level = -1;
	    if (level > 9) level = 9;
	    const int zr = compress2(packed.data(), &produced, raw.data(), static_cast<uLong>(raw.size()), level);
	    if (zr == Z_OK && produced > 0 && produced < raw.size() &&
		produced <= static_cast<uLongf>(std::numeric_limits<int32_t>::max()) &&
		opts.zlib_wrapper) {
		jt_put_i32(data, static_cast<int32_t>(produced));
		data.insert(data.end(), packed.begin(), packed.begin() + static_cast<size_t>(produced));
		wrote_compressed = true;
	    }
	}
	if (!wrote_compressed) {
	    jt_put_i32(data, -static_cast<int32_t>(raw_bytes));
	    data.insert(data.end(), raw.begin(), raw.end());
	}
    }
}

/* Build one JT 8 Tri-Strip Set Shape LOD segment from a group of triangle
 * coordinates (nine doubles per triangle).  Returns the finished segment bytes in
 * 'segment', its content-derived GUID in 'guid_out', and the CRC-32 of the element
 * payload in 'crc_out' (EXP-042).  Returns false and sets 'error' on overflow.
 *
 * EXP-010: when opts.lod_ratios is non-empty, additional coarser LOD elements are
 * appended to the same segment after the finest (full-resolution) element, in the
 * order the reader assigns increasing lod_level.  The default (empty ratios)
 * reproduces the previous single-element output byte-for-byte.
 *
 * Extracted from jt_write so the writer can emit N segments (EXP-006) by calling
 * this per coordinate group; the single-group case reproduces the previous output
 * byte-for-byte when compression is disabled. */
static bool
build_shape_segment(const std::vector<double> &coords, const jt_write_options &opts,
    int32_t object_id, jt::ByteBuffer &segment, jt::Guid &guid_out, uint32_t &crc_out,
    std::string &error)
{
    /* EXP-003: JT 9+ logical elements carry a 4-byte object id after the base-type
     * byte; JT 8 elements do not.  The extra width is folded into every element
     * length below so the reader's element walk stays aligned. */
    const bool has_object_id = opts.version >= 9;
    const uint32_t object_id_bytes = has_object_id ? 4u : 0u;

    /* EXP-010: assemble the LOD coordinate streams: the finest (full-resolution)
     * mesh first, then a decimated mesh per requested ratio.  With no ratios (the
     * default) this is just the single full-resolution element, byte-for-byte
     * compatible with the previous output. */
    std::vector<std::vector<double>> lod_coords;
    lod_coords.push_back(coords);
    for (double ratio : opts.lod_ratios) {
	std::vector<double> decimated = jt_decimate_coords(coords, ratio);
	if (decimated.size() >= 9)
	    lod_coords.push_back(std::move(decimated));
    }

    /* Build each LOD element's payload up front so the finest element's payload can
     * seed the segment GUID (EXP-036) and CRC footer (EXP-042). */
    std::vector<jt::ByteBuffer> lod_data(lod_coords.size());
    for (size_t i = 0; i < lod_coords.size(); ++i)
	build_lod_element_data(lod_coords[i], opts, lod_data[i]);
    const jt::ByteBuffer &data = lod_data.front();

    /* EXP-042: CRC-32 of the finest element payload, so a round-trip reader can
     * detect corruption of the written bytes.  Computed over 'data' before it is
     * wrapped in the element/segment framing. */
    crc_out = jt_crc32(data.data(), data.size());

    /* EXP-036: content-derived, unique, reproducible segment GUID. */
    guid_out = jt_generate_guid(data);

    /* Wrap the element data in a logical element and a shape segment. */
    segment.clear();
    segment.insert(segment.end(), guid_out.begin(), guid_out.end());   /* segment GUID (EXP-036) */
    jt_put_u32(segment, 7);                                            /* segment type: Shape LOD */
    const size_t seg_length_pos = segment.size();
    jt_put_u32(segment, 0);                                            /* placeholder length */

    /* EXP-010: emit one logical element per LOD, finest first (the reader assigns
     * increasing lod_level in element order). */
    for (const jt::ByteBuffer &lod : lod_data) {
	const uint32_t element_length = static_cast<uint32_t>(16 + 1 + object_id_bytes + lod.size());
	jt_put_u32(segment, element_length);
	segment.insert(segment.end(), jt::jt_tristrip_guid.begin(), jt::jt_tristrip_guid.end());   /* element type GUID */
	segment.push_back(4);                                          /* base type */
	if (has_object_id)
	    jt_put_i32(segment, object_id);                           /* EXP-003/030: JT 9+ object id */
	segment.insert(segment.end(), lod.begin(), lod.end());
    }

    /* EXP-042: store the payload CRC-32 in a footer after the element data but
     * before the end-of-elements marker.  It is framed as a well-formed logical
     * element (length = 16-byte GUID + 1 base-type byte + [JT9+ 4-byte object id] +
     * 4-byte body) with a distinct all-0xEE "checksum" GUID, so a reader that does
     * not recognise the GUID still skips it correctly (elements are self-describing
     * by length) while a checking reader can re-derive the payload CRC and compare it. */
    jt_put_u32(segment, 16 + 1 + object_id_bytes + 4);                 /* footer element length */
    segment.insert(segment.end(), 16, 0xee);                          /* checksum element type GUID */
    segment.push_back(4);                                              /* base type */
    if (has_object_id)
	jt_put_i32(segment, object_id);                               /* EXP-003: JT 9+ object id */
    jt_put_u32(segment, crc_out);                                     /* CRC-32 body */

    /* End-of-elements marker: length == 16 with an all-0xFF GUID (EXP-004). */
    const size_t eoe_marker_pos = segment.size();
    jt_put_u32(segment, 16);
    segment.insert(segment.end(), 16, 0xff);
    BU_ASSERT(segment.size() == eoe_marker_pos + 4 + 16);
    for (size_t i = 0; i < 16; ++i)
	BU_ASSERT(segment[eoe_marker_pos + 4 + i] == 0xff);

    /* EXP-033: the segment length is a uint32_t field; reject rather than truncate. */
    if (segment.size() >= 0xFFFFFFFFu) {
	error = "segment exceeds the 32-bit length limit";
	return false;
    }
    const uint32_t segment_length = static_cast<uint32_t>(segment.size());
    segment[seg_length_pos + 0] = static_cast<unsigned char>(segment_length & 0xff);
    segment[seg_length_pos + 1] = static_cast<unsigned char>((segment_length >> 8) & 0xff);
    segment[seg_length_pos + 2] = static_cast<unsigned char>((segment_length >> 16) & 0xff);
    segment[seg_length_pos + 3] = static_cast<unsigned char>((segment_length >> 24) & 0xff);
    return true;
}

/* EXP-043: JT writer capabilities and current limitations.  Keep this list in
 * sync as the EXP-* items land -- external callers rely on it to know what a file
 * produced here will (and will not) contain.
 *
 * Current limitations:
 *   - JT 8.0 default; 9.5 / 10.3 selectable via options (EXP-003/015)
 *   - single LOD by default; coarse LODs via lod_count / lod_ratios (EXP-010)
 *   - Tri-Strip Set Shape LOD only (no topological mesh -- EXP-021)
 *   - lossless floats by default; uniform quantization selectable (EXP-002/015)
 *   - per-vertex Deering normals selectable in the quantized path (EXP-009);
 *     no colors or texture coordinates yet (EXP-028/037)
 *   - LSG instance graph: root partition with one shape node per shape segment
 *     (EXP-005/006/007), so an LSG-aware reader recovers placed shapes
 *   - dedicated File Description (type 2, EXP-018) and Meta Data (type 4, EXP-008)
 *     segments carry export provenance and per-region names/colors; object
 *     names/colors + units are also mirrored in the version-text tail (EXP-017/030)
 */
int
jt_write(struct gcv_context *context, const struct gcv_opts *gcv_options,
    const void *options_data, const char *dest_path)
{
    struct jt_write_state state;
    struct db_tree_state tree_state;
    struct gcv_region_end_data gcvwriter;

    state.gcv_options = gcv_options;
    state.dbip = context->dbip;
    state.the_model = nmg_mm();
    state.vlfree = &rt_vlfree;
    state.triangles = 0;

    gcvwriter.write_region = nmg_to_jt;
    gcvwriter.client_data = &state;

    RT_DBTS_INIT(&tree_state);
    tree_state.ts_tol = &gcv_options->calculational_tolerance;
    tree_state.ts_ttol = &gcv_options->tessellation_tolerance;
    tree_state.ts_m = &state.the_model;

    (void)db_walk_tree(state.dbip, gcv_options->num_objects, (const char **)gcv_options->object_names,
	1, &tree_state, 0, gcv_region_end, rt_booltree_leaf_tess, (void *)&gcvwriter);

    nmg_km(state.the_model);
    rt_vlist_cleanup();

    if (state.triangles == 0) {
	bu_log("JT export failed: no triangles were produced from '%s'\n", dest_path);
	return 0;
    }

    /* The JT 8 counts below are 32-bit fields.  Reject meshes whose triangle
     * or vertex counts would overflow a uint32_t rather than writing a file
     * with silently truncated counts. */
    if (state.triangles > std::numeric_limits<uint32_t>::max() / 3) {
	bu_log("JT export failed: mesh from '%s' exceeds the 32-bit triangle/vertex limit\n", dest_path);
	return 0;
    }
    const uint32_t triangles = static_cast<uint32_t>(state.triangles);
    const uint32_t vertex_count = triangles * 3;

    /* EXP-015: parse write-time options (version/compression/quantization/LOD/
     * segment size) from the caller's options_data, falling back to conservative
     * interoperable defaults (EXP-001/034/040). */
    jt_write_options opts = jt_parse_write_options(options_data);

    /* EXP-010: when more than one LOD is requested, populate the decimation ratios
     * for the coarse levels.  The finest (ratio 1.0) is always emitted implicitly
     * as the first element; each extra level halves-and-then-some toward 10% so
     * lod_count == 4 yields the canonical 50% / 25% / 10% coarse set.  Kept empty
     * for the default lod_count == 1 so output is unchanged. */
    if (opts.lod_ratios.empty() && opts.lod_count > 1) {
	static const double canonical[3] = {0.5, 0.25, 0.10};
	const unsigned extra = opts.lod_count - 1;
	for (unsigned i = 0; i < extra; ++i) {
	    if (i < 3)
		opts.lod_ratios.push_back(canonical[i]);
	    else
		/* Beyond the canonical three, keep halving the last ratio. */
		opts.lod_ratios.push_back(opts.lod_ratios.back() * 0.5);
	}
    }

    /* EXP-017: convert the mm-based geometry into the .g file's own unit before
     * writing, the inverse of the reader's mm_per_unit import scaling.  The .g
     * stores everything in mm (base); dbi_base2local is the (unit-per-mm) factor
     * and bu_units_string(dbi_local2base) names the unit for the metadata below.
     * A unit of mm leaves the coordinates untouched (factor 1.0). */
    double base2local = 1.0;
    const char *unit_name = "mm";
    if (state.dbip) {
	base2local = state.dbip->dbi_base2local;
	const char *nm = bu_units_string(state.dbip->dbi_local2base);
	if (nm) unit_name = nm;
    }
    if (!(base2local > 0.0))
	base2local = 1.0;
    if (base2local != 1.0)
	for (double &coord : state.coords)
	    coord *= base2local;

    /* EXP-006/019: collect the geometry into a vector of coordinate groups, one
     * per shape segment.  With chunking disabled (the default) all triangles form
     * a single group/segment; when opts.max_triangles_per_segment is set, the flat
     * triangle stream is split into groups of at most that many triangles so a very
     * large mesh becomes several segments (each its own TOC entry and GUID). */
    std::vector<std::vector<double>> groups;
    if (opts.max_triangles_per_segment > 0 && state.triangles > opts.max_triangles_per_segment) {
	const size_t chunk = opts.max_triangles_per_segment;
	for (size_t start = 0; start < state.triangles; start += chunk) {
	    const size_t count = std::min(chunk, state.triangles - start);
	    std::vector<double> group(state.coords.begin() + start * 9,
		state.coords.begin() + (start + count) * 9);
	    groups.push_back(std::move(group));
	}
    } else if (opts.per_region_segments && state.regions.size() > 1) {
	/* EXP-007: route each leaf region's triangles to its own shape segment so
	 * the comb/region hierarchy is preserved as separate LSG shape nodes (each
	 * with its own content GUID and region name).  Regions were recorded in
	 * walk order with contiguous [first_triangle, first_triangle+triangle_count)
	 * ranges into state.coords, so a per-region slice is a straight copy.  A
	 * region contributing no triangles is skipped (it produced no geometry). */
	for (const jt_region_info &ri : state.regions) {
	    if (ri.triangle_count == 0)
		continue;
	    const size_t begin = ri.first_triangle * 9;
	    const size_t end = (ri.first_triangle + ri.triangle_count) * 9;
	    if (end > state.coords.size())
		continue;
	    groups.push_back(std::vector<double>(state.coords.begin() + begin,
		state.coords.begin() + end));
	}
	if (groups.empty())
	    groups.push_back(state.coords);
    } else {
	groups.push_back(state.coords);
    }

    /* File header: version_text(80) + byte_order(1) + reserved(4) + toc_offset
     * (EXP-003: 8 bytes for JT 10 wide offsets, else 4) + LSG segment id(16),
     * then the segments, then the TOC. */
    const bool wide_offsets = opts.version >= 10;
    const uint32_t header_length = static_cast<uint32_t>(jt::VERSION_LENGTH) + 1 + 4 +
	(wide_offsets ? 8u : 4u) + 16u;

    /* EXP-041: streaming write.  Rather than accumulating every segment (and the
     * TOC) into one in-memory "file" vector and issuing a single fwrite at the
     * end, open the destination now, write the header with a placeholder TOC
     * offset, then stream each finished segment straight to disk as it is built --
     * freeing the segment buffer immediately -- while tracking only the running
     * byte offset (no full-file buffer).  The TOC is written last, then the header
     * is patched via fseek to record the real TOC offset.  Peak memory is now one
     * segment plus the (small) TOC list instead of the whole file.
     *
     * EXP-018/030: build the version-text tail (units + preserved per-region names
     * and colors) up front so the header can be written before any segment. */
    std::string meta = std::string("units=") + unit_name;
    for (const jt_region_info &ri : state.regions) {
	meta += ";name=";
	for (char ch : ri.name)
	    if (ch != ';' && ch != '=' && ch != '*' && static_cast<unsigned char>(ch) >= 0x20 &&
		static_cast<unsigned char>(ch) < 0x7f)
		meta.push_back(ch);
	if (ri.has_color) {
	    char rgb[32];
	    snprintf(rgb, sizeof(rgb), ",color=%.3f,%.3f,%.3f",
		static_cast<double>(ri.color[0]), static_cast<double>(ri.color[1]),
		static_cast<double>(ri.color[2]));
	    meta += rgb;
	}
    }

    char version[jt::VERSION_LENGTH];
    std::fill(version, version + sizeof(version), ' ');
    const char *sig = "Version 8.0 BRL-CAD JT Writer";
    if (opts.version == 9) sig = "Version 9.5 BRL-CAD JT Writer";
    else if (opts.version == 10) sig = "Version 10.3 BRL-CAD JT Writer";
    std::memcpy(version, sig, strlen(sig));
    size_t meta_pos = strlen(sig) + 1;
    for (size_t i = 0; i < meta.size() && meta_pos < jt::VERSION_LENGTH; ++i, ++meta_pos)
	version[meta_pos] = meta[i];

    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
	perror("libgcv");
	bu_log("cannot open JT output file (%s) for writing\n", dest_path);
	return 0;
    }

    /* Stream-write helper: append bytes to the file, tracking the running offset
     * and latching the first short write so the caller can abort cleanly. */
    uint64_t stream_offset = 0;
    bool write_ok = true;
    auto stream_write = [&](const unsigned char *data, size_t n) {
	if (!write_ok)
	    return;
	if (n == 0)
	    return;
	if (fwrite(data, 1, n, fp) != n) {
	    write_ok = false;
	    return;
	}
	stream_offset += n;
    };

    /* Header: version text, byte order, reserved, placeholder TOC offset, LSG id. */
    stream_write(reinterpret_cast<const unsigned char *>(version), jt::VERSION_LENGTH);
    {
	jt::ByteBuffer hdr;
	hdr.push_back(0);          /* byte order: little endian */
	jt_put_u32(hdr, 0);        /* reserved / empty state field */
	if (wide_offsets)
	    jt_put_u64(hdr, 0);    /* placeholder TOC offset (patched below) */
	else
	    jt_put_u32(hdr, 0);
	hdr.insert(hdr.end(), 16, 0);   /* LSG segment id (unused by the reader) */
	stream_write(hdr.data(), hdr.size());
    }
    /* Byte offset of the placeholder TOC-offset field within the header, for the
     * fseek patch after the TOC is written. */
    const long toc_offset_field_pos = static_cast<long>(jt::VERSION_LENGTH) + 1 + 4;

    std::vector<jt_toc_entry> toc;
    std::set<jt::Guid> seen_guids;   /* EXP-031: enforce GUID uniqueness across segments */
    std::vector<jt::Guid> shape_guids;   /* EXP-005: per-shape GUIDs the LSG references */

    int32_t next_object_id = 0;
    for (std::vector<double> &group : groups) {
	jt::ByteBuffer segment;
	jt::Guid segment_guid;
	uint32_t payload_crc = 0;
	std::string seg_error;
	/* EXP-003/030: JT 9+ tags each element with an object id; use a per-segment
	 * running id so multi-segment output cross-references stay distinct. */
	const int32_t object_id = (opts.version >= 9) ? next_object_id++ : -1;
	if (!build_shape_segment(group, opts, object_id, segment, segment_guid, payload_crc, seg_error)) {
	    bu_log("JT export failed: %s for '%s'\n", seg_error.c_str(), dest_path);
	    fclose(fp);
	    return 0;
	}

	/* EXP-031: every segment GUID must be unique (spec requirement).  The GUID
	 * is content-derived (EXP-036), so a collision means two segments carry
	 * identical bytes; perturb the copy with the EXP-036 factory over a salted
	 * buffer until it is unique rather than emitting a duplicate. */
	if (seen_guids.count(segment_guid)) {
	    bu_log("JT export: duplicate segment GUID detected, regenerating\n");
	    jt::ByteBuffer salt = segment;
	    for (uint32_t attempt = 0; seen_guids.count(segment_guid) && attempt < 1000; ++attempt) {
		salt.push_back(static_cast<unsigned char>(attempt & 0xff));
		segment_guid = jt_generate_guid(salt);
	    }
	    if (seen_guids.count(segment_guid)) {
		bu_log("JT export failed: could not generate a unique segment GUID for '%s'\n", dest_path);
		fclose(fp);
		return 0;
	    }
	    /* Rewrite the segment's leading GUID with the de-duplicated value. */
	    std::copy(segment_guid.begin(), segment_guid.end(), segment.begin());
	}
	seen_guids.insert(segment_guid);

	const uint64_t segment_offset = stream_offset;   /* EXP-041: current file offset */
	const uint32_t segment_length = static_cast<uint32_t>(segment.size());
	stream_write(segment.data(), segment.size());
	toc.push_back(jt_toc_entry{segment_guid, segment_offset, segment_length, 7});
	shape_guids.push_back(segment_guid);
    }

    /* EXP-005: append an LSG (Logical Scene Graph) PartitionNode segment that
     * references the shape segments above.  It provides the assembly-container
     * scaffold; EXP-007 makes it a real instance graph (root partition with one
     * shape node per shape segment), so a reader that traverses the LSG recovers
     * the placed shapes with their region names. */
    {
	jt::ByteBuffer lsg_segment;
	jt::Guid lsg_guid;
	std::string lsg_error;
	std::string part_name = state.regions.empty() ? std::string("brlcad") : state.regions.front().name;
	/* EXP-007: per-shape node names.  When the shape segment count matches the
	 * region count (the common single-LOD, unchunked case) each shape carries
	 * its region name; otherwise generic names keep the graph well-formed. */
	std::vector<std::string> shape_names;
	if (shape_guids.size() == state.regions.size())
	    for (const jt_region_info &ri : state.regions)
		shape_names.push_back(ri.name);
	else
	    for (size_t i = 0; i < shape_guids.size(); ++i)
		shape_names.push_back("jt_shape_" + std::to_string(i + 1));
	if (write_lsg_segment(opts, shape_guids, part_name, shape_names, lsg_segment, lsg_guid, lsg_error)) {
	    if (!seen_guids.count(lsg_guid)) {
		const uint64_t segment_offset = stream_offset;   /* EXP-041 */
		const uint32_t segment_length = static_cast<uint32_t>(lsg_segment.size());
		stream_write(lsg_segment.data(), lsg_segment.size());
		toc.push_back(jt_toc_entry{lsg_guid, segment_offset, segment_length, 1});
		seen_guids.insert(lsg_guid);
	    }
	} else if (gcv_options->verbosity_level) {
	    bu_log("JT export: LSG segment omitted (%s)\n", lsg_error.c_str());
	}
    }

    /* EXP-018: append a File Description metadata segment (type 2) with the export
     * provenance (product/manufacturer/version/units/timestamp/BRL-CAD tag).  The
     * reader ignores type-2 segments, so it is registered in the TOC purely for
     * traceability and does not affect geometry import. */
    {
	jt::ByteBuffer fd_segment;
	jt::Guid fd_guid;
	std::string fd_error;
	if (write_file_description_segment(opts, unit_name, fd_segment, fd_guid, fd_error) &&
	    !seen_guids.count(fd_guid)) {
	    const uint64_t segment_offset = stream_offset;   /* EXP-041 */
	    const uint32_t segment_length = static_cast<uint32_t>(fd_segment.size());
	    stream_write(fd_segment.data(), fd_segment.size());
	    toc.push_back(jt_toc_entry{fd_guid, segment_offset, segment_length, 2});
	    seen_guids.insert(fd_guid);
	} else if (!fd_error.empty() && gcv_options->verbosity_level) {
	    bu_log("JT export: File Description segment omitted (%s)\n", fd_error.c_str());
	}
    }

    /* EXP-008: append a Meta Data segment (type 4) with per-region names and RGB
     * colors when any region carried an explicit color/name.  Also reader-ignored,
     * so it records the appearance binding for downstream tools without affecting
     * the geometry that imports through the shape segments above. */
    {
	jt::ByteBuffer md_segment;
	jt::Guid md_guid;
	std::string md_error;
	if (write_metadata_segment(opts, state.regions, md_segment, md_guid, md_error) &&
	    !md_segment.empty() && !seen_guids.count(md_guid)) {
	    const uint64_t segment_offset = stream_offset;   /* EXP-041 */
	    const uint32_t segment_length = static_cast<uint32_t>(md_segment.size());
	    stream_write(md_segment.data(), md_segment.size());
	    toc.push_back(jt_toc_entry{md_guid, segment_offset, segment_length, 4});
	    seen_guids.insert(md_guid);
	} else if (!md_error.empty() && gcv_options->verbosity_level) {
	    bu_log("JT export: Meta Data segment omitted (%s)\n", md_error.c_str());
	}
    }

    /* EXP-041: the TOC begins at the current streamed offset.  Serialize it into a
     * small buffer and stream it, then patch the header's TOC-offset field. */
    const uint64_t toc_offset = stream_offset;

    /* EXP-014/003: emit one TOC entry per written segment (wide offsets for JT 10). */
    jt::ByteBuffer toc_bytes;
    jt_write_toc(toc_bytes, toc, wide_offsets);
    stream_write(toc_bytes.data(), toc_bytes.size());

    /* EXP-041: patch the placeholder TOC offset now that its real value is known,
     * then confirm every streamed write succeeded before declaring success. */
    bool patch_ok = write_ok;
    if (patch_ok) {
	jt::ByteBuffer off;
	if (wide_offsets)
	    jt_put_u64(off, toc_offset);            /* EXP-003: JT 10 wide TOC offset */
	else
	    jt_put_u32(off, static_cast<uint32_t>(toc_offset));
	if (fseek(fp, toc_offset_field_pos, SEEK_SET) != 0 ||
	    fwrite(off.data(), 1, off.size(), fp) != off.size())
	    patch_ok = false;
    }
    /* header_length is the offset of the first segment; the streamed header must
     * have advanced by exactly that many bytes (a sanity check on the layout). */
    (void)header_length;

    const bool ok = write_ok && patch_ok;
    fclose(fp);
    if (!ok) {
	/* EXP-047: a write failed partway through, so the on-disk file is incomplete
	 * (its TOC or the patched offset is missing/partial).  Stamp an explicit
	 * "*INCOMPLETE*" marker into the version-signature area (well past the
	 * "Version 8.0 ..." prefix the reader's version regex needs) so a reader can
	 * positively reject the partial file.  Best-effort: even if the marker write
	 * fails, the missing/short TOC still makes the file unreadable. */
	FILE *mark = fopen(dest_path, "r+b");
	if (mark) {
	    const char *incomplete = "*INCOMPLETE*";
	    if (fseek(mark, jt::JT_INCOMPLETE_MARKER_OFFSET, SEEK_SET) == 0)
		(void)fwrite(incomplete, 1, strlen(incomplete), mark);
	    fclose(mark);
	}
	bu_log("JT export failed: short write to '%s' (marked incomplete)\n", dest_path);
	return 0;
    }

    if (gcv_options->verbosity_level)
	bu_log("Wrote %u triangles (%u vertices) in %zu segment(s) to JT file %s\n",
	    triangles, vertex_count, toc.size(), dest_path);
    return 1;
}

} // namespace

extern "C" {
static const struct gcv_filter gcv_conv_jt_read = {
    "JT Reader", GCV_FILTER_READ, BU_MIME_MODEL_JT, jt_can_read, NULL, NULL, jt_read
};

static const struct gcv_filter gcv_conv_jt_write = {
    "JT Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_JT, NULL, NULL, NULL, jt_write
};

static const struct gcv_filter * const filters[] = {&gcv_conv_jt_read, &gcv_conv_jt_write, NULL};
const struct gcv_plugin gcv_plugin_info_s = {filters};

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(void)
{
    return &gcv_plugin_info_s;
}
}
