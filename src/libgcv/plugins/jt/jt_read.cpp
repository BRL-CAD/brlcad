/*                         J T _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "jt_parser.h"

#include "vmath.h"
#include "nmg.h"
#include "bu/log.h"
#include "bu/mime.h"
#include "gcv/api.h"
#include "gcv/util.h"
#include "raytrace.h"
#include "wdb.h"

static int
jt_can_read(const char *path)
{
    return jt::has_jt_signature(path) ? 1 : 0;
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

    std::vector<jt::Mesh> meshes;
    size_t toc_index = 0;
    for (const jt::TocEntry &entry : file.toc()) {
	++toc_index;
	if (entry.type() < 6 || entry.type() > 16) continue;
	jt::Segment segment;
	std::vector<jt::Element> elements;
	if (!file.segment(entry, segment, error) || !file.elements(segment, elements, error)) {
	    bu_log("JT shape segment in TOC entry %zu skipped: %s\n", toc_index - 1, error.c_str());
	    continue;
	}
	for (const jt::Element &element : elements) {
	    if (!file.is_legacy_mesh(element) && !file.is_topological_mesh(element)) continue;
	    jt::Mesh mesh;
	    const bool imported = file.is_topological_mesh(element) ?
		file.topological_mesh(element, mesh, error) : file.legacy_mesh(element, mesh, error);
	    if (!imported) {
		bu_log("JT mesh in TOC entry %zu skipped: %s\n", toc_index - 1, error.c_str());
		continue;
	    }
	    meshes.push_back(std::move(mesh));
	}
    }

    struct rt_wdb *wdb = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    mk_id_units(wdb, "Conversion from JT format", "mm");
    if (meshes.empty()) {
	bu_log("JT import failed: %s: no supported renderable mesh geometry found\n", source_path);
	return 0;
    }

    struct wmember all = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&all.l);
    for (size_t i = 0; i < meshes.size(); ++i) {
	std::string name = "jt_mesh_" + std::to_string(i + 1) + ".bot";
	jt::Mesh &mesh = meshes[i];
	for (double &coordinate : mesh.vertices) coordinate *= options->scale_factor;
	if (mk_bot(wdb, name.c_str(), RT_BOT_SOLID, RT_BOT_CCW, 0,
		mesh.vertices.size() / 3, mesh.faces.size() / 3, mesh.vertices.data(), mesh.faces.data(), NULL, NULL) != 0) {
	    bu_log("JT import failed: cannot create %s\n", name.c_str());
	    return 0;
	}
	mk_addmember(name.c_str(), &all.l, NULL, WMOP_UNION);
    }
    mk_lcomb(wdb, "all", &all, 0, NULL, NULL, NULL, 0);
    return 1;
}

/* ------------------------------------------------------------------------
 * JT export.  Geometry is tessellated to triangles through the NMG booleans
 * (as g-stl does) and written as a single JT 8 Tri-Strip Set Shape LOD
 * element with lossless, uncompressed vertex coordinates -- the simplest
 * representation this plugin's reader round-trips faithfully.
 * ---------------------------------------------------------------------- */
namespace {

struct jt_write_state {
    const struct gcv_opts *gcv_options;
    struct db_i *dbip;
    struct model *the_model;
    struct bu_list *vlfree;
    std::vector<double> coords;   /* nine doubles per triangle (three xyz corners) */
    size_t triangles;
};

inline void jt_put_u16(std::vector<unsigned char> &b, uint16_t v)
{
    b.push_back(static_cast<unsigned char>(v & 0xff));
    b.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
}
inline void jt_put_u32(std::vector<unsigned char> &b, uint32_t v)
{
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<unsigned char>((v >> (i * 8)) & 0xff));
}
inline void jt_put_i32(std::vector<unsigned char> &b, int32_t v) { jt_put_u32(b, static_cast<uint32_t>(v)); }
inline void jt_put_f32(std::vector<unsigned char> &b, float f)
{
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(u));
    jt_put_u32(b, u);
}

/* GUID of the JT Tri-Strip Set Shape LOD Element, stored little-endian. */
const unsigned char jt_tristrip_guid[16] = {
    0xab, 0x10, 0xdd, 0x10, 0xc8, 0x2a, 0xd1, 0x11,
    0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97
};

void
nmg_to_jt(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state *UNUSED(tsp),
    void *client_data)
{
    struct jt_write_state *state = static_cast<struct jt_write_state *>(client_data);
    struct shell *s;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    nmg_triangulate_model(r->m_p, state->vlfree, &state->gcv_options->calculational_tolerance);

    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;
	NMG_CK_SHELL(s);
	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;
	    NMG_CK_FACEUSE(fu);
	    if (fu->orientation != OT_SAME)
		continue;
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;
		double corners[9];
		int vert_count = 0;
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;
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
		    continue;
		for (int i = 0; i < 9; ++i) state->coords.push_back(corners[i]);
		state->triangles++;
	    }
	}
    }
}

int
jt_write(struct gcv_context *context, const struct gcv_opts *gcv_options,
    const void *UNUSED(options_data), const char *dest_path)
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

    const uint32_t triangles = static_cast<uint32_t>(state.triangles);
    const uint32_t vertex_count = triangles * 3;

    /* Element data: JT 8 Tri-Strip Set, lossless uncompressed vertices. */
    std::vector<unsigned char> data;
    jt_put_u16(data, 1);          /* vertex LOD version */
    jt_put_u32(data, 0);          /* vertex bindings */
    for (int i = 0; i < 4; ++i) data.push_back(0);   /* quantization params */
    jt_put_u16(data, 1);          /* tri-strip version */
    jt_put_u16(data, 1);          /* representation version */
    data.push_back(0);            /* normal binding */
    data.push_back(0);            /* texture binding */
    data.push_back(0);            /* color binding */
    for (int i = 0; i < 4; ++i) data.push_back(0);   /* representation quantization (lossless) */

    /* Primitive List Indices: one 3-vertex strip per triangle -> vertex-index
     * boundaries [0, 3, ..., 3*triangles].  The reader unpacks these with the
     * Stride1 predictor, so store the Stride1 residuals under the Null CODEC. */
    const uint32_t boundary_count = triangles + 1;
    data.push_back(0);            /* Int32 CDP: Null CODEC (single-byte codec id) */
    jt_put_i32(data, static_cast<int32_t>(boundary_count));
    for (uint32_t i = 0; i < boundary_count; ++i) {
	const uint32_t s0 = i * 3;
	uint32_t residual;
	if (i < 4)
	    residual = s0;
	else
	    residual = s0 - (2u * (i - 1) * 3u - (i - 2) * 3u);
	jt_put_u32(data, residual);
    }

    /* Lossless raw vertex data: uncompressed (negative compressed size). */
    const uint32_t raw_bytes = vertex_count * 3u * 4u;
    jt_put_i32(data, static_cast<int32_t>(raw_bytes));
    jt_put_i32(data, -static_cast<int32_t>(raw_bytes));
    for (size_t i = 0; i < state.coords.size(); ++i)
	jt_put_f32(data, static_cast<float>(state.coords[i]));

    /* Wrap the element data in a logical element and a shape segment. */
    std::vector<unsigned char> segment;
    segment.insert(segment.end(), jt_tristrip_guid, jt_tristrip_guid + 16);   /* segment GUID */
    jt_put_u32(segment, 7);                                                    /* segment type: Shape LOD */
    const size_t seg_length_pos = segment.size();
    jt_put_u32(segment, 0);                                                    /* placeholder length */

    const uint32_t element_length = static_cast<uint32_t>(16 + 1 + data.size());
    jt_put_u32(segment, element_length);
    segment.insert(segment.end(), jt_tristrip_guid, jt_tristrip_guid + 16);   /* element type GUID */
    segment.push_back(4);                                                      /* base type */
    segment.insert(segment.end(), data.begin(), data.end());

    /* End-of-elements marker: length == 16 with an all-0xFF GUID. */
    jt_put_u32(segment, 16);
    for (int i = 0; i < 16; ++i) segment.push_back(0xff);

    const uint32_t segment_length = static_cast<uint32_t>(segment.size());
    segment[seg_length_pos + 0] = static_cast<unsigned char>(segment_length & 0xff);
    segment[seg_length_pos + 1] = static_cast<unsigned char>((segment_length >> 8) & 0xff);
    segment[seg_length_pos + 2] = static_cast<unsigned char>((segment_length >> 16) & 0xff);
    segment[seg_length_pos + 3] = static_cast<unsigned char>((segment_length >> 24) & 0xff);

    /* File header (JT 8, little endian): 105 bytes, then the segment, then the TOC. */
    const uint32_t header_length = 80 + 1 + 4 + 4 + 16;
    const uint32_t segment_offset = header_length;
    const uint32_t toc_offset = segment_offset + segment_length;

    std::vector<unsigned char> file;
    char version[80];
    std::memset(version, ' ', sizeof(version));
    const char *sig = "Version 8.0 BRL-CAD JT Writer";
    std::memcpy(version, sig, strlen(sig));
    file.insert(file.end(), version, version + 80);
    file.push_back(0);                          /* byte order: little endian */
    jt_put_u32(file, 0);                        /* reserved / empty state field */
    jt_put_u32(file, toc_offset);
    for (int i = 0; i < 16; ++i) file.push_back(0);   /* LSG segment id (unused by the reader) */

    file.insert(file.end(), segment.begin(), segment.end());

    jt_put_u32(file, 1);                        /* TOC entry count */
    file.insert(file.end(), jt_tristrip_guid, jt_tristrip_guid + 16);
    jt_put_u32(file, segment_offset);
    jt_put_u32(file, segment_length);
    jt_put_u32(file, static_cast<uint32_t>(7) << 24);   /* attributes: segment type in high byte */

    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
	perror("libgcv");
	bu_log("cannot open JT output file (%s) for writing\n", dest_path);
	return 0;
    }
    const size_t written = fwrite(file.data(), 1, file.size(), fp);
    fclose(fp);
    if (written != file.size()) {
	bu_log("JT export failed: short write to '%s'\n", dest_path);
	return 0;
    }

    if (gcv_options->verbosity_level)
	bu_log("Wrote %u triangles (%u vertices) to JT file %s\n", triangles, vertex_count, dest_path);
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
