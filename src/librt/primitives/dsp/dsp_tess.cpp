/*                   D S P _ T E S S . C P P
 * BRL-CAD
 *
 * Copyright (c) 1999-2026 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/dsp/dsp_tess.cpp
 *
 * DSP tessellation logic
 *
 */

#include "common.h"

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cstdint>
#include <cmath>

#include "bu/time.h"
#include "vmath.h"
#include "raytrace.h"
#include "rt/functab.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"
#include "nmg.h"

#include "TerraScape.hpp"
#include "mmesh/meshdecimation.h"

/* private header */
#include "./dsp.h"

/* Thin wrapper: returns elapsed milliseconds since t0.                       */
static double
dsp_elapsed_ms(int64_t t0)
{
    int64_t t1 = bu_gettime();
    return (double)(t1 - t0) / 1000.0;
}

/* ------------------------------------------------------------------ */
/* Named constants for the decimation-based DSP tessellation           */
/* ------------------------------------------------------------------ */

/* GCT decimation cost-model exponents.
 * The original GCT code uses a sixth-power cost; mmesh uses the same.
 * rt_bot_decimate_gct() adjusts for backward compatibility with the
 * fourth-power legacy model: fsize = feature^(2/3) * 2^(4/3).        */
static const double DSP_DECIMATE_STRENGTH_EXPONENT = 2.0 / 3.0;
static const double DSP_DECIMATE_STRENGTH_SCALE    = /* pow(2.0, 4.0/3.0) */ 2.5198420997897;

/* Minimum and maximum feature-size clamps (in grid units). */
static const double DSP_DECIMATE_MIN_FEATURE   = 0.5;
static const double DSP_DECIMATE_MAX_FEATURE_RATIO = 0.25; /* fraction of grid diagonal */

/* DSP height values are 16-bit unsigned integers; any cell where all four
 * corners have the integer value 0 (stored as double 0.0) is a sea-level /
 * void cell that contributes no solid volume.  Using 0.5 as the threshold
 * for the < comparison robustly identifies these integer-0 cells without
 * triggering the -Wfloat-equal compiler warning.                          */
static const double DSP_ZERO_HEIGHT_THRESHOLD  = 0.5;

/* Steiner-point grid: points placed every SPACING_FACTOR * min_dist apart
 * so that at most one Steiner point falls between any two boundary edges.
 * The start/end offsets of 0.5 / 0.25 keep points off the boundary.  */
static const double DSP_STEINER_GRID_SPACING_FACTOR = 2.5;

/* Minimum spacing between Steiner points in cell units.               */
static const double DSP_STEINER_MIN_CELL_SPACING = 3.0;

/**
 * Fast NMG assembler for a manifold, CCW-oriented triangulated BOT.
 *
 * The standard rt_bot_tess() path calls nmg_cmface() per triangle,
 * which internally calls nmg_findeu() to search the vertex's vertexuse
 * list for a matching dangling edge.  For well-formed triangulated
 * meshes the per-call cost is O(local degree) ≈ O(1), but the constant
 * factor is high due to NMG_CK_* validation and heap churn.
 *
 * This assembler avoids nmg_findeu() entirely:
 *   1. Call nmg_cface() for every triangle — creates faces/edgeuses
 *      without joining shared edges.
 *   2. Collect every directed edgeuse (v_start → v_end) from every
 *      OT_SAME loop into a hash map keyed on the (vertex*, vertex*)
 *      pointer pair.
 *   3. For each entry whose key (A,B) has a matching reverse (B,A),
 *      call nmg_je() exactly once to join the pair.
 *   4. Compute face planes (nmg_calc_face_g), mark edges real, and
 *      compute bounding boxes (nmg_region_a).
 *
 * The entire assembly is O(V + F) with small constants.
 *
 * Returns 0 on success, -1 on failure.
 */
static int
dsp_build_nmg_from_bot(struct nmgregion **r_out,
		       struct model *m,
		       const struct rt_bot_internal *bot,
		       const struct bn_tol *tol,
		       struct bu_list *vlfree)
{
    struct nmgregion *r;
    struct shell *s;
    size_t i;

    if (!bot || bot->num_faces == 0 || bot->num_vertices == 0)
	return -1;

    /* Create region and shell (nmg_mrsv also creates one shell-vertex
     * placeholder; it will simply be an orphaned vertexuse).          */
    r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &r->s_hd);

    /* ---- Step 1: create every face with nmg_cface ---- */

    /* Track which NMG vertex belongs to each BOT vertex index.
     * Starts all-NULL; nmg_cface() fills in newly allocated vertices. */
    struct vertex **v_arr = (struct vertex **)bu_calloc(
	    bot->num_vertices, sizeof(struct vertex *), "dsp nmg v_arr");

    /* Storage for the faceuses so we can iterate them in step 3. */
    struct faceuse **fu_arr = (struct faceuse **)bu_calloc(
	    bot->num_faces, sizeof(struct faceuse *), "dsp nmg fu_arr");

    for (i = 0; i < bot->num_faces; i++) {
	int a = bot->faces[3*i + 0];
	int b = bot->faces[3*i + 1];
	int c = bot->faces[3*i + 2];

	struct vertex *corners[3];
	corners[0] = v_arr[a];
	corners[1] = v_arr[b];
	corners[2] = v_arr[c];

	struct faceuse *fu = nmg_cface(s, corners, 3);
	if (!fu) {
	    bu_log("dsp_build_nmg: nmg_cface failed for face %zu\n", i);
	    continue;
	}
	fu_arr[i] = fu;

	/* nmg_cface() fills NULL entries with freshly allocated vertices. */
	v_arr[a] = corners[0];
	v_arr[b] = corners[1];
	v_arr[c] = corners[2];
    }

    /* ---- Step 2: assign geometry to every vertex ---- */
    for (i = 0; i < bot->num_vertices; i++) {
	if (v_arr[i] && !v_arr[i]->vg_p) {
	    point_t pt;
	    VMOVE(pt, &bot->vertices[3*i]);
	    nmg_vertex_gv(v_arr[i], pt);
	}
    }

    /* ---- Step 3: join shared edges via a hash map ---- */

    /* Key type: pair of vertex pointers encoding a directed edge. */
    struct EdgeKey {
	const struct vertex *a;
	const struct vertex *b;
	bool operator==(const EdgeKey& o) const { return a == o.a && b == o.b; }
    };
    struct EdgeKeyHash {
	std::size_t operator()(const EdgeKey& k) const {
	    /* Golden-ratio mixing sized for the platform's pointer width. */
	    std::size_t ha = std::hash<const void *>()(k.a);
	    std::size_t hb = std::hash<const void *>()(k.b);
#if SIZE_MAX > 0xFFFFFFFFU
	    /* 64-bit: Knuth / Fibonacci multiplier for 64-bit size_t */
	    return ha ^ (hb * (std::size_t)11400714819323198485ULL + (ha >> 16));
#else
	    /* 32-bit */
	    return ha ^ (hb * (std::size_t)2654435761U + (ha >> 16));
#endif
	}
    };

    std::unordered_map<EdgeKey, struct edgeuse *, EdgeKeyHash> eu_map;
    eu_map.reserve(bot->num_faces * 3);

    for (i = 0; i < bot->num_faces; i++) {
	struct faceuse *fu = fu_arr[i];
	if (!fu) continue;

	/* Iterate only the OT_SAME loopuse. */
	struct loopuse *lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;

	struct edgeuse *eu;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    const struct vertex *vs = eu->vu_p->v_p;
	    const struct vertex *ve = eu->eumate_p->vu_p->v_p;
	    eu_map[{vs, ve}] = eu;
	}
    }

    /* Walk the map once: for every (A→B) entry, look for (B→A) and
     * join them.  We use the pointer-ordering trick to process each
     * unordered pair exactly once and avoid double-joining.           */
    for (auto& kv : eu_map) {
	const struct vertex *va = kv.first.a;
	const struct vertex *vb = kv.first.b;

	/* Only process each geometric edge once (the lower-addr end first). */
	if (va > vb)
	    continue;

	auto it = eu_map.find({vb, va});
	if (it == eu_map.end())
	    continue; /* boundary / open edge — leave dangling */

	struct edgeuse *eu_fwd = kv.second;
	struct edgeuse *eu_rev = it->second;

	/* Guard: skip if already joined (radial != eumate means joined). */
	if (eu_fwd->radial_p != eu_fwd->eumate_p)
	    continue;

	nmg_je(eu_fwd, eu_rev);
    }

    /* ---- Step 4: compute face planes and bounding boxes ---- */
    for (i = 0; i < bot->num_faces; i++) {
	struct faceuse *fu = fu_arr[i];
	if (!fu) continue;
	if (nmg_calc_face_g(fu, vlfree))
	    nmg_kfu(fu);
    }

    nmg_mark_edges_real(&s->l.magic, vlfree);
    nmg_region_a(r, tol);

    bu_free(v_arr,  "dsp nmg v_arr");
    bu_free(fu_arr, "dsp nmg fu_arr");

    *r_out = r;
    return 0;
}

/**
 * Generate interior Steiner points for the DSP bottom face triangulation.
 *
 * Produces a grid of interior points inside @p outer_poly (and outside any
 * @p hole_polys) spaced no closer than @p min_distance to each other or to
 * any boundary/hole vertex already in the proximity index.
 *
 * Points are placed on a uniform grid with step = 2.5 * min_distance and
 * rejected when they fall outside the outer polygon, inside a hole polygon,
 * or closer than min_distance to an already-accepted point.
 *
 * Returns a vector of (x, y) pairs in world XY coordinates (z=0 plane).
 */
static std::vector<std::pair<double, double>>
dsp_generate_steiner_pts(
	const std::vector<std::pair<double, double>>& outer_poly,
	const std::vector<std::vector<std::pair<double, double>>>& hole_polys,
	double bb_min_x, double bb_max_x,
	double bb_min_y, double bb_max_y,
	double min_distance)
{
    TerraScape::SteinerIndex idx;

    /* Seed the proximity index with boundary / hole vertices so Steiner
     * points are kept away from the constraint edges.                    */
    for (const auto& p : outer_poly)
	idx.insert(p.first, p.second);
    for (const auto& hp : hole_polys)
	for (const auto& p : hp)
	    idx.insert(p.first, p.second);

    double step = min_distance * DSP_STEINER_GRID_SPACING_FACTOR;

    std::vector<std::pair<double, double>> result;
    for (double sy = bb_min_y + step * 0.5; sy < bb_max_y - step * 0.25; sy += step) {
	for (double sx = bb_min_x + step * 0.5; sx < bb_max_x - step * 0.25; sx += step) {
	    if (!TerraScape::pointInPolygon(sx, sy, outer_poly))
		continue;
	    bool in_hole = false;
	    for (const auto& hp : hole_polys) {
		if (TerraScape::pointInPolygon(sx, sy, hp)) {
		    in_hole = true;
		    break;
		}
	    }
	    if (in_hole)
		continue;
	    if (idx.hasNear(sx, sy, min_distance))
		continue;
	    idx.insert(sx, sy);
	    result.push_back({sx, sy});
	}
    }

    return result;
}


/**
 * Step 1.5 of the decimation pipeline: replace flat coplanar surface regions
 * with sparse CDT triangulations before the mmesh pass.
 *
 * After the naive per-cell tessellation a contiguous group of cells whose
 * four height-corner values are all equal forms a "flat region".  These
 * produce a dense slab of coplanar triangles that mmesh cannot effectively
 * reduce (its quadric-error metric sees zero error on every interior vertex
 * of a perfectly flat patch).  This function finds every such connected
 * region, extracts its 2-D rectilinear boundary polygon, CDT-triangulates it
 * with detria, and replaces the 2·N naive triangles with O(perimeter)
 * triangles — dramatically shrinking the input to the main decimation pass.
 *
 * Boundary winding (CCW in world-XY, i.e. positive shoelace area):
 *   Right boundary (no neighbor gx+1): (gy+1)*W+(gx+1) → gy*W+(gx+1)
 *   Left  boundary (no neighbor gx-1): gy*W+gx           → (gy+1)*W+gx
 *   Top   boundary (no neighbor gy-1): gy*W+(gx+1)       → gy*W+gx
 *   Bottom boundary (no neighbor gy+1): (gy+1)*W+gx      → (gy+1)*W+(gx+1)
 *
 * On CDT failure for any component the original naive triangles are restored
 * so the mesh always remains topologically complete.
 */
static void
dsp_replace_coplanar_regions(
    const int W, const int H,
    const TerraScape::TerrainData& terrain,
    const std::vector<double>& surf_verts,
    std::vector<int>& surf_faces,
    size_t& n_surf_faces)
{
    if (W < 2 || H < 2 || n_surf_faces == 0) return;

    /* ---- 1. Build flat-cell map ---- */
    /* A cell (gx, gy) is flat when all four corner heights are equal within
     * 0.5 height-unit (exact for integer DSP values) and all are non-zero. */
    const size_t ncells = (size_t)(H-1) * (W-1);
    std::vector<bool> flat_cell(ncells, false);
    for (int gy = 0; gy < H-1; ++gy) {
	for (int gx = 0; gx < W-1; ++gx) {
	    double h00 = terrain.getHeight(gx,   gy);
	    if (h00 < DSP_ZERO_HEIGHT_THRESHOLD) continue;
	    double h10 = terrain.getHeight(gx+1, gy);
	    double h01 = terrain.getHeight(gx,   gy+1);
	    double h11 = terrain.getHeight(gx+1, gy+1);
	    if (fabs(h00-h10) < 0.5 && fabs(h00-h01) < 0.5 && fabs(h00-h11) < 0.5)
		flat_cell[(size_t)gy * (W-1) + gx] = true;
	}
    }

    /* ---- 2. 4-connected BFS to find same-height flat components ---- */
    std::vector<int> comp(ncells, -1);
    std::vector<std::vector<std::pair<int,int>>> components;

    for (int gy = 0; gy < H-1; ++gy) {
	for (int gx = 0; gx < W-1; ++gx) {
	    size_t idx = (size_t)gy * (W-1) + gx;
	    if (!flat_cell[idx] || comp[idx] >= 0) continue;

	    double ch = terrain.getHeight(gx, gy);
	    int cid = (int)components.size();
	    components.push_back({});

	    std::queue<std::pair<int,int>> q;
	    q.push({gx, gy});
	    comp[idx] = cid;
	    while (!q.empty()) {
		auto [cx, cy] = q.front(); q.pop();
		components[cid].push_back({cx, cy});
		static const int dx4[] = {1, -1, 0,  0};
		static const int dy4[] = {0,  0, 1, -1};
		for (int i = 0; i < 4; ++i) {
		    int nx = cx + dx4[i], ny = cy + dy4[i];
		    if (nx < 0 || nx >= W-1 || ny < 0 || ny >= H-1) continue;
		    size_t nidx = (size_t)ny * (W-1) + nx;
		    if (!flat_cell[nidx] || comp[nidx] >= 0) continue;
		    if (fabs(terrain.getHeight(nx, ny) - ch) >= 0.5) continue;
		    comp[nidx] = cid;
		    q.push({nx, ny});
		}
	    }
	}
    }

    if (components.empty()) return;

    /* ---- 3. Rebuild face list: copy non-flat faces, apply CDT for flat ---- */
    /* We walk the naive face array in the same scan order as step 1 of the
     * caller so face indices map 1:1 to cell (gx, gy).                       */
    std::vector<int> new_faces;
    new_faces.reserve(surf_faces.size()); /* usually shrinks */

    /* Build per-component cell sets for O(1) boundary lookup. */
    std::vector<std::unordered_set<int>> cell_set(components.size());
    for (size_t ci = 0; ci < components.size(); ++ci)
	for (auto& [gx, gy] : components[ci])
	    cell_set[ci].insert(gy * (W-1) + gx);

    /* Which components have already been processed (CDT emitted or fallback)? */
    std::vector<bool> done(components.size(), false);

    size_t fi = 0; /* naive face index (advances by 2 per non-skipped cell) */
    for (int gy = 0; gy < H-1; ++gy) {
	for (int gx = 0; gx < W-1; ++gx) {
	    /* Mirror the all-zero skip used by the naive mesh builder. */
	    bool all_zero =
		terrain.getHeight(gx,   gy)   < DSP_ZERO_HEIGHT_THRESHOLD &&
		terrain.getHeight(gx+1, gy)   < DSP_ZERO_HEIGHT_THRESHOLD &&
		terrain.getHeight(gx,   gy+1) < DSP_ZERO_HEIGHT_THRESHOLD &&
		terrain.getHeight(gx+1, gy+1) < DSP_ZERO_HEIGHT_THRESHOLD;
	    if (all_zero) continue;

	    int cid = comp[(size_t)gy * (W-1) + gx];

	    if (cid < 0) {
		/* Non-flat cell: copy its two naive faces straight through. */
		new_faces.push_back(surf_faces[3*fi + 0]);
		new_faces.push_back(surf_faces[3*fi + 1]);
		new_faces.push_back(surf_faces[3*fi + 2]);
		new_faces.push_back(surf_faces[3*(fi+1) + 0]);
		new_faces.push_back(surf_faces[3*(fi+1) + 1]);
		new_faces.push_back(surf_faces[3*(fi+1) + 2]);
		fi += 2;
		continue;
	    }

	    /* Flat component: advance the face counter but defer CDT until we
	     * see the component for the first time.                              */
	    fi += 2;
	    if (done[(size_t)cid]) continue;
	    done[(size_t)cid] = true;

	    const auto& cells = components[(size_t)cid];

	    /* ---- 3a. Extract directed boundary half-edges (CCW in world) ---- */
	    /* The sign convention (see function header) ensures positive shoelace
	     * area for outer loops and negative for inner holes.                 */
	    std::unordered_map<int,int> he_next;
	    he_next.reserve(cells.size() * 4);

	    for (auto& [cx, cy] : cells) {
		/* Right: no neighbor at (cx+1, cy) */
		if (cx+1 >= W-1 || !cell_set[(size_t)cid].count(cy*(W-1)+(cx+1)))
		    he_next[(cy+1)*W + (cx+1)] = cy*W + (cx+1);
		/* Left: no neighbor at (cx-1, cy) */
		if (cx-1 < 0    || !cell_set[(size_t)cid].count(cy*(W-1)+(cx-1)))
		    he_next[cy*W + cx] = (cy+1)*W + cx;
		/* Top (smaller gy = higher world-y): no neighbor at (cx, cy-1) */
		if (cy-1 < 0    || !cell_set[(size_t)cid].count((cy-1)*(W-1)+cx))
		    he_next[cy*W + (cx+1)] = cy*W + cx;
		/* Bottom (larger gy = lower world-y): no neighbor at (cx, cy+1) */
		if (cy+1 >= H-1 || !cell_set[(size_t)cid].count((cy+1)*(W-1)+cx))
		    he_next[(cy+1)*W + cx] = (cy+1)*W + (cx+1);
	    }

	    /* ---- 3b. Chain half-edges into closed polygon loops ---- */
	    std::vector<std::vector<int>> loops;
	    {
		std::unordered_set<int> visited;
		for (auto& kv : he_next) {
		    int start = kv.first;
		    if (visited.count(start)) continue;
		    std::vector<int> loop;
		    int cur = start;
		    for (;;) {
			if (visited.count(cur)) break;
			visited.insert(cur);
			loop.push_back(cur);
			auto it = he_next.find(cur);
			if (it == he_next.end()) break;
			cur = it->second;
		    }
		    if (loop.size() >= 3)
			loops.push_back(std::move(loop));
		}
	    }

	    if (loops.empty()) goto flat_fallback;

	    /* ---- 3c. Classify loops: positive shoelace = outer, negative = hole ---- */
	    {
		int outer_li = -1;
		double outer_area = 0.0;
		std::vector<int> hole_lis;

		for (int li = 0; li < (int)loops.size(); ++li) {
		    const auto& lp = loops[(size_t)li];
		    double sa = 0.0;
		    size_t n = lp.size();
		    for (size_t i = 0; i < n; ++i) {
			int va = lp[i], vb = lp[(i+1) % n];
			double xa = surf_verts[3*va + 0], ya = surf_verts[3*va + 1];
			double xb = surf_verts[3*vb + 0], yb = surf_verts[3*vb + 1];
			sa += xa * yb - xb * ya;
		    }
		    sa *= 0.5;
		    if (sa > 0.0) {
			if (outer_li < 0 || sa > outer_area) {
			    if (outer_li >= 0) hole_lis.push_back(outer_li);
			    outer_li = li;
			    outer_area = sa;
			} else {
			    hole_lis.push_back(li);
			}
		    } else {
			hole_lis.push_back(li);
		    }
		}

		if (outer_li < 0) goto flat_fallback;

		/* ---- 3d. Set up detria CDT for this flat region ---- */
		{
		    std::vector<detria::PointD> dtri_pts;
		    std::vector<int>            dtri_to_sv;
		    dtri_pts.reserve(loops[(size_t)outer_li].size() + 8);
		    dtri_to_sv.reserve(loops[(size_t)outer_li].size() + 8);

		    /* Outer loop. */
		    std::vector<uint32_t> oidx(loops[(size_t)outer_li].size());
		    for (size_t i = 0; i < loops[(size_t)outer_li].size(); ++i) {
			oidx[i] = (uint32_t)dtri_pts.size();
			int sv = loops[(size_t)outer_li][i];
			dtri_pts.push_back({surf_verts[3*sv + 0], surf_verts[3*sv + 1]});
			dtri_to_sv.push_back(sv);
		    }

		    /* Hole loops. */
		    std::vector<std::vector<uint32_t>> hidx_storage;
		    for (int hli : hole_lis) {
			hidx_storage.emplace_back(loops[(size_t)hli].size());
			for (size_t i = 0; i < loops[(size_t)hli].size(); ++i) {
			    hidx_storage.back()[i] = (uint32_t)dtri_pts.size();
			    int sv = loops[(size_t)hli][i];
			    dtri_pts.push_back({surf_verts[3*sv + 0], surf_verts[3*sv + 1]});
			    dtri_to_sv.push_back(sv);
			}
		    }

		    detria::Triangulation dtri;
		    dtri.setPoints(dtri_pts);
		    dtri.addOutline(oidx);
		    for (const auto& hi : hidx_storage) dtri.addHole(hi);

		    bool ok = false;
		    try { ok = dtri.triangulate(true); }
		    catch (const std::exception& e) {
			bu_log("DSP coplanar: detria threw: %s\n", e.what());
		    }

		    if (!ok) {
			bu_log("DSP coplanar: detria failed: %s\n",
			       dtri.getErrorMessage().c_str());
			goto flat_fallback;
		    }

		    /* Emit CDT triangles — CCW winding = same as naive surface. */
		    dtri.forEachTriangle([&](detria::Triangle<uint32_t> t) {
			new_faces.push_back(dtri_to_sv[t.x]);
			new_faces.push_back(dtri_to_sv[t.y]);
			new_faces.push_back(dtri_to_sv[t.z]);
		    }, false); /* false → CCW */
		}
		continue; /* next cell */
	    }

	    flat_fallback:
	    /* Restore original naive triangles for this component. */
	    for (auto& [rx, ry] : cells) {
		int v00 = ry*W + rx,    v10 = ry*W + (rx+1);
		int v01 = (ry+1)*W + rx, v11 = (ry+1)*W + (rx+1);
		new_faces.push_back(v00); new_faces.push_back(v01); new_faces.push_back(v10);
		new_faces.push_back(v10); new_faces.push_back(v01); new_faces.push_back(v11);
	    }
	} /* gx */
    } /* gy */

    size_t old_count = n_surf_faces;
    surf_faces   = std::move(new_faces);
    n_surf_faces = surf_faces.size() / 3;
    bu_log("DSP coplanar: %zu component(s); %zu → %zu faces\n",
	   components.size(), old_count, n_surf_faces);
}


/**
 * Proposed decimation-based DSP tessellation.
 *
 * Implements the six-step pipeline:
 *   1.   Build a naive two-triangle-per-cell surface mesh.
 *   1.5. Replace flat coplanar regions with sparse CDT (dsp_replace_coplanar_regions).
 *   2.   Decimate with mmesh (error-bounded, O(N log N)).
 *   3.   Extract outer/hole boundary loops from the decimated half-edge set.
 *   4.   Triangulate the bottom face with detria (Delaunay + Steiner points).
 *   5.   Assemble surface + walls + bottom into a closed NMG.
 *
 * Each step is timed and logged so long steps can be identified quickly.
 *
 * The boundary half-edge extraction guarantees that wall winding is always
 * consistent with the surface: for a directed half-edge ta→tb, the triangles
 * (ta, bot_a, bot_b) and (ta, bot_b, tb) produce an outward-pointing normal
 * regardless of whether the loop belongs to the outer boundary or a hole.
 * This identity holds for both CCW (positive signed area) outer loops and
 * CW (negative signed area) hole loops as verified analytically.
 *
 * Returns 0 on success; -1 if decimation or triangulation fails.
 * On success, *r_out is the newly created NMG region.
 */
static int
dsp_tess_with_decimation(
	struct nmgregion **r_out,
	struct model *m,
	const struct rt_dsp_internal *dsp_ip,
	const TerraScape::TerrainData& terrain,
	double effective_err,
	const struct bn_tol *tol,
	struct bu_list *vlfree)
{
    const int W = terrain.width;
    const int H = terrain.height;
    if (W < 2 || H < 2)
	return -1;

    int64_t t_total = bu_gettime(); /* wall-clock start for whole pipeline */
    int64_t t_step  = t_total;

    /* ------------------------------------------------------------------ */
    /* Pre-compute grid scale and stride before building the naive mesh.   */
    /*                                                                     */
    /* When the error budget allows spanning many grid cells (large         */
    /* raw_feature), building a full-resolution naive mesh and then asking  */
    /* mmesh to remove most of its vertices is wasteful.  Instead we        */
    /* pre-subsample the height grid at stride intervals so that mmesh sees  */
    /* a mesh that is already near the target density.                       */
    /* ------------------------------------------------------------------ */
    double grid_scale = 1.0;
    {
	point_t o_grid = {0.0, 0.0, 0.0};
	point_t x_grid = {1.0, 0.0, 0.0};
	point_t o_world, x_world;
	MAT4X3PNT(o_world, dsp_ip->dsp_stom, o_grid);
	MAT4X3PNT(x_world, dsp_ip->dsp_stom, x_grid);
	double ds = sqrt(
	    (x_world[0]-o_world[0])*(x_world[0]-o_world[0]) +
	    (x_world[1]-o_world[1])*(x_world[1]-o_world[1]) +
	    (x_world[2]-o_world[2])*(x_world[2]-o_world[2]));
	if (ds > 1e-10)
	    grid_scale = ds;
    }

    double raw_feature = effective_err / grid_scale;
    double grid_diag   = sqrt((double)(W-1)*(W-1) + (double)(H-1)*(H-1));
    if (raw_feature < DSP_DECIMATE_MIN_FEATURE)
	raw_feature = DSP_DECIMATE_MIN_FEATURE;
    if (raw_feature > grid_diag * DSP_DECIMATE_MAX_FEATURE_RATIO)
	raw_feature = grid_diag * DSP_DECIMATE_MAX_FEATURE_RATIO;

    /* Stride: sample every (raw_feature/2) cells, minimum 1, maximum 16.
     * This keeps the naive mesh well below the mmesh input budget while
     * still giving mmesh enough resolution to refine high-curvature areas.
     * The boundary columns/rows are always included at full resolution. */
    int stride = (int)(raw_feature * 0.5);
    if (stride < 1)  stride = 1;
    if (stride > 16) stride = 16;

    /* ------------------------------------------------------------------ */
    /* Step 1: build compact subsampled surface mesh                       */
    /*                                                                     */
    /* Sampled grid positions along X: 0, stride, 2*stride, ..., W-1      */
    /* Sampled grid positions along Y: 0, stride, 2*stride, ..., H-1      */
    /* Boundary (first/last row/col) is always included so wall quads have  */
    /* correct corner vertices.                                             */
    /* ------------------------------------------------------------------ */
    std::vector<int> gx_list, gy_list;
    for (int gx = 0; gx < W; gx += stride) gx_list.push_back(gx);
    if (gx_list.back() != W-1) gx_list.push_back(W-1);
    for (int gy = 0; gy < H; gy += stride) gy_list.push_back(gy);
    if (gy_list.back() != H-1) gy_list.push_back(H-1);

    int nx = (int)gx_list.size(); /* sampled columns */
    int ny = (int)gy_list.size(); /* sampled rows    */

    /* Compact vertex array: index = sy*nx + sx */
    std::vector<double> surf_verts((size_t)nx * ny * 3);
    for (int sy = 0; sy < ny; ++sy) {
	for (int sx = 0; sx < nx; ++sx) {
	    int gx = gx_list[(size_t)sx];
	    int gy = gy_list[(size_t)sy];
	    size_t i = (size_t)sy * nx + sx;
	    surf_verts[3*i + 0] = terrain.origin.x + gx * terrain.cell_size;
	    surf_verts[3*i + 1] = terrain.origin.y - gy * terrain.cell_size;
	    surf_verts[3*i + 2] = terrain.getHeight(gx, gy);
	}
    }

    std::vector<int> surf_faces;
    surf_faces.reserve((size_t)(nx-1) * (ny-1) * 6);

    for (int sy = 0; sy < ny-1; ++sy) {
	for (int sx = 0; sx < nx-1; ++sx) {
	    int gx0 = gx_list[(size_t)sx],   gx1 = gx_list[(size_t)sx+1];
	    int gy0 = gy_list[(size_t)sy],   gy1 = gy_list[(size_t)sy+1];
	    /* Skip cells where every corner has height 0. */
	    if (terrain.getHeight(gx0, gy0) < DSP_ZERO_HEIGHT_THRESHOLD &&
		terrain.getHeight(gx1, gy0) < DSP_ZERO_HEIGHT_THRESHOLD &&
		terrain.getHeight(gx0, gy1) < DSP_ZERO_HEIGHT_THRESHOLD &&
		terrain.getHeight(gx1, gy1) < DSP_ZERO_HEIGHT_THRESHOLD)
		continue;
	    int v00 = sy * nx + sx;
	    int v10 = sy * nx + (sx + 1);
	    int v01 = (sy + 1) * nx + sx;
	    int v11 = (sy + 1) * nx + (sx + 1);
	    /* CCW from above (+Z normal). */
	    surf_faces.push_back(v00); surf_faces.push_back(v01); surf_faces.push_back(v10);
	    surf_faces.push_back(v10); surf_faces.push_back(v01); surf_faces.push_back(v11);
	}
    }

    size_t n_surf_verts = (size_t)nx * ny;
    size_t n_surf_faces  = surf_faces.size() / 3;

    bu_log("DSP decimate: naive surface %zu verts %zu faces stride=%d (%.1f ms)\n",
	   n_surf_verts, n_surf_faces, stride, dsp_elapsed_ms(t_step));
    t_step = bu_gettime();

    /* ------------------------------------------------------------------ */
    /* Step 1.5: replace flat coplanar regions with CDT pre-triangulation  */
    /* Only useful at stride=1 (full resolution); skip for coarser grids.  */
    /* ------------------------------------------------------------------ */
    if (stride == 1) {
	dsp_replace_coplanar_regions(nx, ny, terrain, surf_verts, surf_faces, n_surf_faces);
	bu_log("DSP coplanar: done in %.1f ms\n", dsp_elapsed_ms(t_step));
	t_step = bu_gettime();
    }

    /* ------------------------------------------------------------------ */
    /* Step 2: decimate surface with mmesh                                 */
    /* ------------------------------------------------------------------ */

    /* Feature size in grid units (already computed above as raw_feature). */
    double fsize = pow(raw_feature, DSP_DECIMATE_STRENGTH_EXPONENT)
		 * DSP_DECIMATE_STRENGTH_SCALE;

    {
	mdOperation mdop;
	mdOperationInit(&mdop);
	mdOperationData(&mdop,
			n_surf_verts, surf_verts.data(),
			MD_FORMAT_DOUBLE, 3 * sizeof(double),
			n_surf_faces,  surf_faces.data(),
			MD_FORMAT_INT,    3 * sizeof(int));
	mdOperationStrength(&mdop, fsize);

	/* MD_FLAGS_PLANAR_MODE: prevent face normals from flipping, which
	 * is essential for height-field surfaces.
	 * MD_FLAGS_TRIANGLE_WINDING_CCW: preserve the CCW orientation.   */
	int flags = MD_FLAGS_PLANAR_MODE | MD_FLAGS_TRIANGLE_WINDING_CCW;
	int n_threads = (int)bu_avail_cpus();
	if (n_threads < 1) n_threads = 1;

	mdMeshDecimation(&mdop, n_threads, flags);

	n_surf_verts = (size_t)mdop.vertexcount;
	n_surf_faces  = (size_t)mdop.tricount;
	surf_verts.resize(n_surf_verts * 3);
	surf_faces.resize(n_surf_faces  * 3);

	bu_log("DSP decimate: → %zu verts %zu faces (raw_feature=%g fsize=%g, %.1f ms)\n",
	       n_surf_verts, n_surf_faces, raw_feature, fsize,
	       dsp_elapsed_ms(t_step));
	t_step = bu_gettime();
    }

    if (n_surf_faces == 0) {
	bu_log("DSP decimate: all faces removed by decimation\n");
	return -1;
    }

    /* ------------------------------------------------------------------ */
    /* Step 3: extract boundary loops via directed half-edges              */
    /*                                                                     */
    /* A half-edge (va → vb) is a boundary edge when its reverse           */
    /* (vb → va) has no corresponding triangle.  We encode each directed  */
    /* edge as a uint64_t key = (uint32_t(va) << 32) | uint32_t(vb).      */
    /* ------------------------------------------------------------------ */
    std::unordered_map<uint64_t, bool> fwd_edges;
    fwd_edges.reserve(n_surf_faces * 3 * 2);

    for (size_t f = 0; f < n_surf_faces; ++f) {
	uint32_t va = (uint32_t)surf_faces[3*f + 0];
	uint32_t vb = (uint32_t)surf_faces[3*f + 1];
	uint32_t vc = (uint32_t)surf_faces[3*f + 2];
	fwd_edges[((uint64_t)va << 32) | vb] = true;
	fwd_edges[((uint64_t)vb << 32) | vc] = true;
	fwd_edges[((uint64_t)vc << 32) | va] = true;
    }

    /* bdry_next[va] = vb means the directed boundary half-edge va→vb exists. */
    std::unordered_map<int, int> bdry_next;
    for (const auto& kv : fwd_edges) {
	int va = (int)(kv.first >> 32);
	int vb = (int)(kv.first & 0xFFFFFFFFu);
	uint64_t rev = ((uint64_t)(uint32_t)vb << 32) | (uint32_t)va;
	if (!fwd_edges.count(rev))
	    bdry_next[va] = vb;
    }

    if (bdry_next.empty()) {
	bu_log("DSP decimate: no boundary edges found\n");
	return -1;
    }

    /* Chain boundary half-edges into closed loops. */
    std::vector<std::vector<int>> boundary_loops;
    {
	std::unordered_set<int> visited;
	for (const auto& kv : bdry_next) {
	    int start = kv.first;
	    if (visited.count(start))
		continue;
	    std::vector<int> loop;
	    int cur = start;
	    for (;;) {
		if (visited.count(cur))
		    break;
		visited.insert(cur);
		loop.push_back(cur);
		auto it = bdry_next.find(cur);
		if (it == bdry_next.end())
		    break;
		cur = it->second;
	    }
	    if (loop.size() >= 3)
		boundary_loops.push_back(std::move(loop));
	}
    }

    bu_log("DSP decimate: %zu boundary loop(s) (%.1f ms)\n",
	   boundary_loops.size(), dsp_elapsed_ms(t_step));
    if (boundary_loops.empty()) {
	bu_log("DSP decimate: no valid boundary loops found\n");
	return -1;
    }

    /* Compute signed area of each loop (shoelace formula) and build 2D polygon.
     * Positive area (CCW in world XY) = outer boundary of a non-zero island.
     * Negative area (CW) = interior hole (void, lake, crevice) inside an island.
     *
     * With all-zero cells excluded from the naive mesh, each disconnected
     * non-zero terrain region produces its own CCW outer loop.  Multiple
     * outer loops mean the DSP has multiple separate non-zero islands; they
     * are assembled as separate manifold shells → multi-manifold BoT.       */
    std::vector<double> loop_area(boundary_loops.size(), 0.0);
    std::vector<std::vector<std::pair<double, double>>> loop_poly_2d(boundary_loops.size());

    for (size_t li = 0; li < boundary_loops.size(); ++li) {
	const auto& loop = boundary_loops[li];
	double sa = 0.0;
	size_t n = loop.size();
	for (size_t i = 0; i < n; ++i) {
	    int va = loop[i], vb = loop[(i + 1) % n];
	    double xa = surf_verts[3*va], ya = surf_verts[3*va + 1];
	    double xb = surf_verts[3*vb], yb = surf_verts[3*vb + 1];
	    sa += xa * yb - xb * ya;
	    loop_poly_2d[li].push_back({xa, ya});
	}
	loop_area[li] = 0.5 * sa;
    }

    /* Partition loops: positive area → outer boundary, negative → hole.
     * A loop with exactly zero area is degenerate and treated as a hole
     * to avoid creating a vacuous outer boundary.                          */
    std::vector<int> outer_loop_idxs, hole_loop_idxs;
    for (size_t li = 0; li < boundary_loops.size(); ++li) {
	if (loop_area[li] > 0.0)
	    outer_loop_idxs.push_back((int)li);
	else
	    hole_loop_idxs.push_back((int)li);
    }

    /* Safety fallback: if all loops are CW (unusual, e.g. single-layer mesh),
     * pick the one with the largest |area| as the outer boundary.           */
    if (outer_loop_idxs.empty()) {
	int best = 0;
	for (int li = 1; li < (int)boundary_loops.size(); ++li)
	    if (std::abs(loop_area[li]) > std::abs(loop_area[best]))
		best = li;
	outer_loop_idxs.push_back(best);
	bu_log("DSP decimate: all loops CW — using largest as outer\n");
    }

    bu_log("DSP decimate: %zu outer loop(s), %zu hole(s)\n",
	   outer_loop_idxs.size(), hole_loop_idxs.size());
    t_step = bu_gettime(); /* reset step timer for wall/bottom construction */

    /* Assign each hole to the outer loop that contains it.
     * Test any one point of the hole polygon against each outer polygon.   */
    std::vector<std::vector<int>> outer_holes(outer_loop_idxs.size());
    for (int hidx : hole_loop_idxs) {
	bool assigned = false;
	int sv_sample = boundary_loops[hidx][0];
	double hx = surf_verts[3*sv_sample];
	double hy = surf_verts[3*sv_sample + 1];
	for (size_t oi = 0; oi < outer_loop_idxs.size(); ++oi) {
	    if (TerraScape::pointInPolygon(hx, hy, loop_poly_2d[outer_loop_idxs[oi]])) {
		outer_holes[oi].push_back(hidx);
		assigned = true;
		break;
	    }
	}
	if (!assigned) {
	    bu_log("DSP decimate: orphan hole loop %d not inside any outer loop\n", hidx);
	    return -1;
	}
    }

    /* ------------------------------------------------------------------ */
    /* Step 4: build walls + prepare bottom face data                      */
    /*                                                                     */
    /* For each directed boundary half-edge ta→tb (regardless of loop     */
    /* orientation), the triangles:                                        */
    /*   (ta, bot_a, bot_b)  and  (ta, bot_b, tb)                         */
    /* always produce an outward-pointing normal.  This is verified        */
    /* analytically: for CCW outer loops the normal points away from the   */
    /* terrain body; for CW hole loops it points toward the hole interior  */
    /* (away from the terrain body).                                       */
    /* ------------------------------------------------------------------ */
    std::vector<double> all_verts(
	surf_verts.begin(), surf_verts.begin() + n_surf_verts * 3);
    std::vector<int>    all_faces(
	surf_faces.begin(), surf_faces.begin() + n_surf_faces * 3);

    /* Map surface vertex index → bottom vertex index.
     *
     * The bottom face is placed at z = -1 (one grid unit below the minimum
     * terrain height of 0).  This guarantees every boundary wall has non-zero
     * height, preventing degenerate wall triangles when mixed cells (some
     * corners at z=0, others above) lie on the non-zero terrain boundary.  */
    std::unordered_map<int, int> surf_to_bot;

    auto add_bottom_vert = [&](int sv) -> int {
	auto it = surf_to_bot.find(sv);
	if (it != surf_to_bot.end()) return it->second;
	int bv = (int)(all_verts.size() / 3);
	all_verts.push_back(surf_verts[3*sv + 0]);
	all_verts.push_back(surf_verts[3*sv + 1]);
	all_verts.push_back(-1.0); /* one unit below terrain floor */
	surf_to_bot[sv] = bv;
	return bv;
    };

    /* Pre-allocate bottom vertices for all boundary loops.
     * loop_bot_indices[li][i] is the all_verts index of the bottom vertex
     * that corresponds to boundary_loops[li][i] on the surface.            */
    std::vector<std::vector<size_t>> loop_bot_indices(boundary_loops.size());
    for (size_t li = 0; li < boundary_loops.size(); ++li) {
	for (int sv : boundary_loops[li])
	    loop_bot_indices[li].push_back((size_t)add_bottom_vert(sv));
    }

    /* Emit wall triangles for every boundary loop.
     *
     * For each directed boundary half-edge (va → vb), the wall quad
     * decomposes into two triangles:
     *   T1: (va, bot_a, bot_b)   — top-left + lower-left + lower-right
     *   T2: (va, bot_b, vb)      — top-left + lower-right + top-right
     *
     * Since the bottom face is at z=-1, surface vertices always have
     * different indices from their bottom counterparts (bot_x ≠ vx in the
     * vertex array).  The degenerate-skip guards protect against a corner
     * case where two boundary vertices happen to share the same bottom
     * vertex (i.e., they have the same XY and would produce bot_a==bot_b),
     * which would yield a zero-area face that nmg_kfu would remove and
     * leave open edges behind.                                              */
    for (size_t li = 0; li < boundary_loops.size(); ++li) {
	const auto& loop = boundary_loops[li];
	size_t n = loop.size();
	for (size_t i = 0; i < n; ++i) {
	    int va    = loop[i];
	    int vb    = loop[(i + 1) % n];
	    int bot_a = (int)loop_bot_indices[li][i];
	    int bot_b = (int)loop_bot_indices[li][(i + 1) % n];
	    /* T1: (va, bot_a, bot_b) — skip if degenerate */
	    if (va != bot_a && va != bot_b && bot_a != bot_b) {
		all_faces.push_back(va);    all_faces.push_back(bot_a); all_faces.push_back(bot_b);
	    }
	    /* T2: (va, bot_b, vb) — skip if degenerate */
	    if (va != bot_b && va != vb && bot_b != vb) {
		all_faces.push_back(va);    all_faces.push_back(bot_b); all_faces.push_back(vb);
	    }
	}
    }

    bu_log("DSP decimate: walls built (%.1f ms)\n", dsp_elapsed_ms(t_step));
    t_step = bu_gettime();

    /* ------------------------------------------------------------------ */
    /* Step 5: triangulate bottom face(s) with detria + Steiner points    */
    /*                                                                     */
    /* Each outer loop (island) gets its own detria run using its outer   */
    /* boundary and any holes that fall inside it.  This ensures that each */
    /* non-zero island gets a proper closed bottom, making the final BoT  */
    /* a multi-manifold with one closed shell per island.                  */
    /* ------------------------------------------------------------------ */

    /* Bounding box of the entire surface (for Steiner point grid).       */
    double bb_min_x = surf_verts[0], bb_max_x = surf_verts[0];
    double bb_min_y = surf_verts[1], bb_max_y = surf_verts[1];
    for (size_t i = 0; i < n_surf_verts; ++i) {
	double x = surf_verts[3*i], y = surf_verts[3*i + 1];
	if (x < bb_min_x) bb_min_x = x;
	if (x > bb_max_x) bb_max_x = x;
	if (y < bb_min_y) bb_min_y = y;
	if (y > bb_max_y) bb_max_y = y;
    }

    double min_dist = terrain.cell_size * DSP_STEINER_MIN_CELL_SPACING;

    /* Process each island independently. */
    for (size_t oi = 0; oi < outer_loop_idxs.size(); ++oi) {
	int outer_loop_idx = outer_loop_idxs[oi];

	const auto& outer_loop = boundary_loops[outer_loop_idx];
	std::vector<std::pair<double, double>> outer_poly_2d;
	std::vector<size_t> outer_bot_idx;
	for (size_t i = 0; i < outer_loop.size(); ++i) {
	    int sv = outer_loop[i];
	    outer_poly_2d.push_back({surf_verts[3*sv], surf_verts[3*sv + 1]});
	    outer_bot_idx.push_back(loop_bot_indices[outer_loop_idx][i]);
	}

	std::vector<std::vector<std::pair<double, double>>> hole_polys_2d;
	std::vector<std::vector<size_t>> hole_bot_idx;
	for (int hidx : outer_holes[oi]) {
	    const auto& hloop = boundary_loops[hidx];
	    std::vector<std::pair<double, double>> hpoly;
	    for (int sv : hloop)
		hpoly.push_back({surf_verts[3*sv], surf_verts[3*sv + 1]});
	    hole_polys_2d.push_back(std::move(hpoly));
	    hole_bot_idx.push_back(loop_bot_indices[hidx]);
	}

	auto steiner_pts = dsp_generate_steiner_pts(
	    outer_poly_2d, hole_polys_2d,
	    bb_min_x, bb_max_x, bb_min_y, bb_max_y,
	    min_dist);

	bu_log("DSP decimate: island %zu — %zu Steiner pts, %zu hole(s) (%.1f ms Steiner)\n",
	       oi, steiner_pts.size(), hole_polys_2d.size(),
	       dsp_elapsed_ms(t_step));
	t_step = bu_gettime();

    /* Assemble the detria point list and index mapping
	 * (detria index → index into all_verts).                              */
	std::vector<detria::PointD> dtri_pts;
	std::vector<size_t>         dtri_to_mesh;

	/* Outer boundary. */
	size_t outer_dtri_count = outer_poly_2d.size();
	for (size_t i = 0; i < outer_dtri_count; ++i) {
	    dtri_pts.push_back({outer_poly_2d[i].first, outer_poly_2d[i].second});
	    dtri_to_mesh.push_back(outer_bot_idx[i]);
	}

	/* Hole boundaries. */
	std::vector<std::pair<size_t, size_t>> hole_ranges; /* [start, count) */
	for (size_t hi = 0; hi < hole_polys_2d.size(); ++hi) {
	    size_t hstart = dtri_pts.size();
	    for (size_t i = 0; i < hole_polys_2d[hi].size(); ++i) {
		dtri_pts.push_back({hole_polys_2d[hi][i].first,
				    hole_polys_2d[hi][i].second});
		dtri_to_mesh.push_back(hole_bot_idx[hi][i]);
	    }
	    hole_ranges.push_back({hstart, dtri_pts.size() - hstart});
	}

	/* Steiner points become extra bottom-face vertices at z=-1. */
	for (const auto& sp : steiner_pts) {
	    int mesh_idx = (int)(all_verts.size() / 3);
	    all_verts.push_back(sp.first);
	    all_verts.push_back(sp.second);
	    all_verts.push_back(-1.0); /* match bottom face z=-1 */
	    dtri_pts.push_back({sp.first, sp.second});
	    dtri_to_mesh.push_back((size_t)mesh_idx);
	}

	/* Run Delaunay triangulation.
	 * IMPORTANT: ReadonlySpan stores a raw pointer into the vector data.
	 * All index vectors must remain alive until after triangulate() returns. */
	detria::Triangulation dtri;
	dtri.setPoints(dtri_pts);

	std::vector<uint32_t> oidx(outer_dtri_count);
	for (uint32_t i = 0; i < (uint32_t)outer_dtri_count; ++i)
	    oidx[i] = i;
	dtri.addOutline(oidx);

	/* Keep hole index vectors alive alongside the Triangulation object. */
	std::vector<std::vector<uint32_t>> hidx_storage;
	for (const auto& hr : hole_ranges) {
	    hidx_storage.emplace_back(hr.second);
	    for (uint32_t i = 0; i < (uint32_t)hr.second; ++i)
		hidx_storage.back()[i] = (uint32_t)(hr.first + i);
	    dtri.addHole(hidx_storage.back());
	}

	bool dtri_ok = false;
	try {
	    dtri_ok = dtri.triangulate(true); /* true = Delaunay */
	} catch (const std::exception& e) {
	    bu_log("DSP decimate: detria threw exception: %s\n", e.what());
	}

	if (!dtri_ok) {
	    bu_log("DSP decimate: detria failed: %s\n",
		   dtri.getErrorMessage().c_str());
	    return -1;
	}

	{
	    /* Add bottom triangles with reversed winding (normal points -Z). */
	    dtri.forEachTriangle([&](detria::Triangle<uint32_t> t) {
		size_t v0 = dtri_to_mesh[t.x];
		size_t v1 = dtri_to_mesh[t.y];
		size_t v2 = dtri_to_mesh[t.z];
		/* Reversed: addTriangle(v0, v2, v1) pattern from TerraScape. */
		all_faces.push_back((int)v0);
		all_faces.push_back((int)v2);
		all_faces.push_back((int)v1);
	    }, false); /* false = CCW triangles from detria */
	}

    } /* end per-island detria loop */

    bu_log("DSP decimate: bottom CDT done (%.1f ms)\n", dsp_elapsed_ms(t_step));
    t_step = bu_gettime();

    /* ------------------------------------------------------------------ */
    /* Step 6: apply dsp_stom transform and build NMG                     */
    /* ------------------------------------------------------------------ */
    size_t n_all_verts = all_verts.size() / 3;
    size_t n_all_faces = all_faces.size() / 3;

    bu_log("DSP decimate: final mesh %zu verts %zu faces\n",
	   n_all_verts, n_all_faces);

    if (n_all_faces == 0)
	return -1;
    struct rt_bot_internal bot_ip;
    memset(&bot_ip, 0, sizeof(bot_ip));
    bot_ip.magic        = RT_BOT_INTERNAL_MAGIC;
    bot_ip.num_vertices = (int)n_all_verts;
    bot_ip.num_faces    = (int)n_all_faces;
    bot_ip.mode         = RT_BOT_SOLID;
    bot_ip.orientation  = RT_BOT_CCW;

    bot_ip.vertices = (fastf_t *)bu_malloc(
	3 * n_all_verts * sizeof(fastf_t), "dsp decimate bot verts");
    bot_ip.faces    = (int *)    bu_malloc(
	3 * n_all_faces * sizeof(int),     "dsp decimate bot faces");

    for (size_t i = 0; i < n_all_verts; ++i) {
	point_t in_pt  = { all_verts[3*i], all_verts[3*i+1], all_verts[3*i+2] };
	point_t out_pt;
	MAT4X3PNT(out_pt, dsp_ip->dsp_stom, in_pt);
	VMOVE(&bot_ip.vertices[3*i], out_pt);
    }
    for (size_t f = 0; f < n_all_faces; ++f) {
	bot_ip.faces[3*f + 0] = all_faces[3*f + 0];
	bot_ip.faces[3*f + 1] = all_faces[3*f + 1];
	bot_ip.faces[3*f + 2] = all_faces[3*f + 2];
    }

    int ret = dsp_build_nmg_from_bot(r_out, m, &bot_ip, tol, vlfree);

    bu_free(bot_ip.vertices, "dsp decimate bot verts");
    bu_free(bot_ip.faces,    "dsp decimate bot faces");

    bu_log("DSP decimate: NMG build %.1f ms  |  total %.1f ms\n",
	   dsp_elapsed_ms(t_step), dsp_elapsed_ms(t_total));

    return ret;
}


/**
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
extern "C" int
rt_dsp_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    struct rt_dsp_internal *dsp_ip;

    if (RT_G_DEBUG & RT_DEBUG_HF)
	bu_log("rt_dsp_tess()\n");

    RT_CK_DB_INTERNAL(ip);
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	    if (!dsp_ip->dsp_mp) {
		bu_log("WARNING: Cannot find data file for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data file [%s] not found or empty\n", bu_vls_cstr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data file not found or not specified\n");
		}
		return -1;
	    }
	    break;
	case RT_DSP_SRC_OBJ:
	    if (!dsp_ip->dsp_bip) {
		bu_log("WARNING: Cannot find data object for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data object [%s] not found or empty\n", bu_vls_cstr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data object not found or not specified\n");
		}
		return -1;
	    }
	    RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
	    RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);
	    break;
    }

    // Step 1: Create TerraScape DSPData from rt_dsp_internal
    TerraScape::DSPData dsp;
    dsp.dsp_buf = dsp_ip->dsp_buf;           // Point to existing buffer (owned by BRL-CAD)
    dsp.dsp_xcnt = dsp_ip->dsp_xcnt;         // Copy dimensions
    dsp.dsp_ycnt = dsp_ip->dsp_ycnt;
    dsp.cell_size = 1.0;                     // Will be scaled by transformation matrix
    dsp.origin = TerraScape::Point3D(0, 0, 0);
    dsp.owns_buffer = false;                 // Don't delete BRL-CAD's buffer

    // Step 2. Convert to TerrainData
    TerraScape::TerrainData terrain;
    if (!dsp.toTerrain(terrain)) {
	bu_log("Failed to convert DSP buffer to TerrainData\n");
	return -1;
    }

    // Step 3.  Decide triangulation strategy based on tolerances -
    // we need to translate BRL-CAD's tolerances into those used by
    // the terrain algorithms.
    TerraScape::TerrainMesh mesh;

    point_t dsp_bb_min, dsp_bb_max;
    if (rt_dsp_bbox(ip, &dsp_bb_min, &dsp_bb_max, tol)) {
	/* Fallback if bbox computation fails */
	VSETALL(dsp_bb_min, 0.0);
	VSETALL(dsp_bb_max, 0.0);
    }
    double dx = dsp_bb_max[0] - dsp_bb_min[0];
    double dy = dsp_bb_max[1] - dsp_bb_min[1];
    double dz = dsp_bb_max[2] - dsp_bb_min[2];
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;
    if (dz < 0) dz = 0;
    double diag = sqrt(dx*dx + dy*dy + dz*dz);
    double height_range = dz;

    /* Extract tessellation tolerances */
    double abs_tol = (ttol && ttol->abs > 0.0) ? ttol->abs : INFINITY;
    double rel_tol = (ttol && ttol->rel > 0.0) ? (ttol->rel * (diag > 0.0 ? diag : 1.0)) : INFINITY;

    double effective_err = INFINITY;
    if (abs_tol < INFINITY && rel_tol < INFINITY)
	effective_err = (abs_tol < rel_tol) ? abs_tol : rel_tol;
    else if (abs_tol < INFINITY)
	effective_err = abs_tol;
    else if (rel_tol < INFINITY)
	effective_err = rel_tol;

    /* Provide a fallback if neither tolerance is set */
    double base_cell = 1.0; /* original grid spacing pre-transform */
    if (!isfinite(effective_err))
	effective_err = base_cell * 0.25;

    /* Respect modeling tolerance floor */
    if (tol && tol->dist > 0.0 && effective_err < tol->dist * 0.5)
	effective_err = tol->dist * 0.5;

    /* Normal tolerance to slope threshold */
    double slope_threshold = 0.2; /* default fallback */
    if (ttol && ttol->norm > 0.0) {
	double angle_rad = 0.0;
	if (ttol->norm < 1.0) {
	    /* treat as cosine of angle */
	    if (ttol->norm > 0.0) {
		double c = ttol->norm;
		if (c > 1.0) c = 1.0;
		if (c < -1.0) c = -1.0;
		angle_rad = acos(c);
	    }
	} else {
	    /* treat as degrees */
	    angle_rad = ttol->norm * (M_PI / 180.0);
	}
	if (angle_rad > 0.0) {
	    double t = tan(angle_rad);
	    if (t < 0.0) t = -t;
	    /* clamp to avoid runaway */
	    if (t > 10.0) t = 10.0;
	    slope_threshold = t;
	}
    }

    /* Derive a heuristic reduction target */
    int min_reduction = 0;
    if (height_range > 1e-9) {
	double hscale = effective_err / height_range;
	if (hscale < 0.0) hscale = 0.0;
	if (hscale > 1.0) hscale = 1.0;
	min_reduction = (int)(hscale * 80.0); /* up to 80% if very loose */
    }
    if (min_reduction < 0) min_reduction = 0;
    if (min_reduction > 90) min_reduction = 90;

    TerraScape::SimplificationParams simp;
    simp.setErrorTol(effective_err);
    simp.setSlopeTol(slope_threshold);
    simp.setMinReduction(min_reduction);
    simp.setPreserveBounds(true);

    int use_simplified = 0;
    /* Decide whether to simplify:
       - If effective error significantly larger than base cell
       - Or slope tolerance generous
       */
    if (effective_err > base_cell * 0.6 || slope_threshold > 0.6) {
	use_simplified = 1;
    }

    if (RT_G_DEBUG & RT_DEBUG_HF) {
	bu_log("DSP tess tol mapping:\n");
	bu_log("  bbox: min=(%g %g %g) max=(%g %g %g)\n",
		V3ARGS(dsp_bb_min), V3ARGS(dsp_bb_max));
	bu_log("  abs_tol=%g rel_tol=%g -> eff=%g\n", abs_tol, rel_tol, effective_err);
	bu_log("  slope_threshold=%g min_triangle_reduction=%d (height_range=%g diag=%g)\n",
		slope_threshold, min_reduction, height_range, diag);
	bu_log("  using %s path\n", use_simplified ? "simplified" : "full");
    }

    bu_log("DSP tess: bbox diag=%g eff_err=%g use_simplified=%d min_reduction=%d step=%d terrain=%dx%d\n",
	    diag, effective_err, use_simplified, min_reduction,
	    std::max(1, (int)std::sqrt(100.0 / (100.0 - std::max(0, std::min(90, min_reduction))))),
	    terrain.width, terrain.height);

    // Step 4.  Make the tessellation.
    //
    // For the simplified path we use the proposed decimation-based approach:
    //   1. Naive two-triangle-per-cell surface mesh
    //   2. mmesh decimation (error-bounded, O(N log N))
    //   3. Boundary loop extraction from the half-edge set
    //   4. detria Delaunay bottom face with Steiner points
    //   5. Assemble into NMG
    //
    // The full path retains the original TerraScape triangulateVolume.
    if (use_simplified) {
	struct bu_list vlfree;
	BU_LIST_INIT(&vlfree);
	int ret = dsp_tess_with_decimation(r, m, dsp_ip, terrain,
					   effective_err, tol, &vlfree);
	bu_list_free(&vlfree);
	if (ret == 0)
	    return 0;
	/* Fall through to the full TerraScape path on failure. */
	bu_log("DSP decimate path failed; falling back to triangulateVolume\n");
	mesh.triangulateVolume(terrain);
    } else {
	mesh.triangulateVolume(terrain);
    }
    bu_log("DSP tess: mesh has %zu vertices, %zu triangles\n",
	    mesh.vertices.size(), mesh.triangles.size());
    if (mesh.vertices.empty() || mesh.triangles.empty()) {
	bu_log("TerraScape produced empty mesh\n");
	return -1;
    }

    // Step 5.  Translate to BoT
    /* Allocate BOT internal */
    struct rt_bot_internal *bot_ip = (struct rt_bot_internal *)bu_calloc(1, sizeof(struct rt_bot_internal), "dsp bot_ip");
    bot_ip->magic = RT_BOT_INTERNAL_MAGIC;
    bot_ip->num_vertices = (int)mesh.vertices.size();
    bot_ip->num_faces = (int)mesh.triangles.size();
    bot_ip->vertices = (fastf_t *)bu_calloc(3 * bot_ip->num_vertices, sizeof(fastf_t), "bot verts");
    bot_ip->faces = (int *)bu_calloc(3 * bot_ip->num_faces, sizeof(int), "bot faces");
    bot_ip->thickness = NULL;
    bot_ip->face_mode = NULL;
    bot_ip->mode = RT_BOT_SOLID;
    bot_ip->orientation = RT_BOT_CCW;
    bot_ip->bot_flags = 0;
    bot_ip->face_normals = NULL;
    bot_ip->num_normals = 0;
    bot_ip->normals = NULL;
    bot_ip->num_face_normals = 0;
    bot_ip->face_normals = NULL;

    /* Populate vertices (apply dsp_stom) */
    for (size_t i = 0; i < bot_ip->num_vertices; ++i) {
	const TerraScape::Point3D &p = mesh.vertices[(size_t)i];
	point_t in = { p.x, p.y, p.z };
	point_t out;
	MAT4X3PNT(out, dsp_ip->dsp_stom, in);
	bot_ip->vertices[3*i+0] = out[0];
	bot_ip->vertices[3*i+1] = out[1];
	bot_ip->vertices[3*i+2] = out[2];
    }

    /* Populate faces */
    for (size_t f = 0; f < bot_ip->num_faces; ++f) {
	const TerraScape::Triangle &tri = mesh.triangles[(size_t)f];
	bot_ip->faces[3*f+0] = (int)tri.vertices[0];
	bot_ip->faces[3*f+1] = (int)tri.vertices[1];
	bot_ip->faces[3*f+2] = (int)tri.vertices[2];
    }

    /* Wrap in rt_db_internal and invoke fast manifold NMG assembler.
     * The assembler bypasses rt_bot_tess / nmg_cmface / nmg_findeu
     * and builds the NMG in O(V+F) using a hash-map edge-pairing step. */
    struct bu_list vlfree;
    BU_LIST_INIT(&vlfree);

    int ret = dsp_build_nmg_from_bot(r, m, bot_ip, tol, &vlfree);

    bu_list_free(&vlfree);

    /* Free our temporary BOT arrays */
    bu_free(bot_ip->vertices, "dsp bot verts");
    bu_free(bot_ip->faces,    "dsp bot faces");
    bu_free(bot_ip,           "dsp bot_ip");

    return ret;
}

/** @} */


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
