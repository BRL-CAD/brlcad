/**
 * Ball Pivoting Algorithm for surface reconstruction from point clouds.
 *
 * Derived and adapted from the Ball Pivoting Algorithm implementation in
 * Open3D: https://github.com/isl-org/Open3D
 *
 * Copyright (c) 2018-2024 www.open3d.org
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Uses nanoflann (https://github.com/jlblancoc/nanoflann) for KD-tree search
 * Uses robust predicates from https://github.com/wlenthe/GeometricPredicates
 */

#pragma once
#include <vector>
#include <list>
#include <unordered_set>
#include <memory>
#include <numeric>
#include <limits>
#include <cmath>
#include <cassert>
#include <array>
#include <algorithm>
#include <unordered_map>

extern "C" {
#include "vmath.h"
#include "bu/log.h"
}

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push /* start new diagnostic pragma */
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#elif defined(__clang__)
#  pragma clang diagnostic push /* start new diagnostic pragma */
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif

#include "nanoflann.hpp"
#include "predicates.h"

namespace bpiv
{

// --- Forward Declarations ---
struct Vertex;
struct Edge;
struct Face;

// --- Pointer Types ---
using VPtr = Vertex*;
using EPtr = std::shared_ptr<Edge>;
using FPtr = std::shared_ptr<Face>;

// --- PointCloud Adaptor for nanoflann ---
struct PCAdaptor {
    const std::vector<std::array<fastf_t, 3>>& pts;
    inline size_t kdtree_get_point_count() const { return pts.size(); }
    inline double kdtree_get_pt(size_t i, size_t d) const { return (double)pts[i][d]; }
    template <class BBOX> bool kdtree_get_bbox(BBOX&) const { return false; }
};

// --- Edge ---
struct Edge {
    enum Type { Border = 0, Front = 1, Inner = 2 };
    VPtr a, b;
    FPtr f0 = nullptr, f1 = nullptr;
    Type type = Front;
    // Orientation sign to control pivot angle orientation without swapping endpoints.
    // +1 means keep default orientation; -1 means invert the angle wrap direction.
    int orient_sign = +1;

    Edge(VPtr s, VPtr t) : a(s), b(t) {}
    void add_face(FPtr f);
    VPtr opp_vert();
};

// --- Vertex ---
struct Vertex {
    enum Type { Orphan = 0, Front = 1, Inner = 2 };
    int id;
    std::array<fastf_t, 3> pos, nrm;
    std::unordered_set<EPtr> edges;
    Type type = Orphan;
    Vertex(int i, const std::array<fastf_t, 3>& p, const std::array<fastf_t, 3>& n)
	: id(i), pos(p), nrm(n) {}
    void update_type()
    {
	if (edges.empty()) type = Orphan;
	else {
	    for (const auto& e : edges) {
		if (e->type != Edge::Inner) {
		    type = Front;
		    return;
		}
	    }
	    type = Inner;
	}
    }
};

// --- Face (Triangle) ---
struct Face {
    VPtr v0, v1, v2;
    std::array<fastf_t, 3> center;
    Face(VPtr a, VPtr b, VPtr c, const std::array<fastf_t, 3>& ctr)
	: v0(a), v1(b), v2(c), center(ctr) {}
};

// --- Edge implementation ---
inline VPtr Edge::opp_vert()
{
    if (!f0) return nullptr;
    if (f0->v0 != a && f0->v0 != b) return f0->v0;
    if (f0->v1 != a && f0->v1 != b) return f0->v1;
    return f0->v2;
}

inline void Edge::add_face(FPtr f)
{
    if (!f0) {
	f0 = f;
	type = Front;
	// Set an orientation flag without swapping endpoints.
	auto o = opp_vert(); // third vertex of face f0
	if (o) {
	    vect_t tn, tmp1, tmp2, nsum;
	    VSUB2(tmp1, b->pos.data(), a->pos.data());
	    VSUB2(tmp2, o->pos.data(), a->pos.data());
	    VCROSS(tn, tmp1, tmp2);
	    if (MAGNITUDE(tn) > 0.0) VUNITIZE(tn); else VSETALL(tn, 0.0);
	    VADD2(nsum, a->nrm.data(), b->nrm.data());
	    VADD2(nsum, nsum, o->nrm.data());
	    if (MAGNITUDE(nsum) > 0.0) VUNITIZE(nsum); else VSETALL(nsum, 0.0);
	    orient_sign = (VDOT(nsum, tn) >= 0.0) ? +1 : -1;
	}
    } else if (!f1) {
	f1 = f;
	type = Inner;
    }
}

// --- Output Mesh ---
struct Mesh {
    std::vector<std::array<int, 3>> tris;
    std::vector<std::array<fastf_t, 3>> tri_nrms;
};

// --- Main Ball Pivoting Class ---
class BallPivot
{
    public:
	BallPivot(const std::vector<std::array<fastf_t, 3>>& pts,
		const std::vector<std::array<fastf_t, 3>>& nrms)
	    : pc_adapt{pts}, kdtree(3, pc_adapt, nanoflann::KDTreeSingleIndexAdaptorParams(10)), points(pts), normals(nrms)
	    {
		kdtree.buildIndex();
		verts.reserve(pts.size());
		for (size_t i = 0; i < pts.size(); ++i)
		    verts.emplace_back(new Vertex((int)i, pts[i], nrms[i]));
	    }

	~BallPivot()
	{
	    for (auto v : verts) delete v;
	}

	static void debug(bool flag) { dbg_flag() = flag; }
	static bool is_debug() { return dbg_flag(); }

	Mesh run(const std::vector<double>& radii)
	{
	    mesh.tris.clear();
	    mesh.tri_nrms.clear();
	    e_front.clear();
	    e_border.clear();
	    for (auto v : verts) {
		v->edges.clear();
		v->type = Vertex::Orphan;
	    }
	    if (is_debug()) bu_log("[BallPivot] Run: %zu points, %zu normals, %zu radii\n",
		    points.size(), normals.size(), radii.size());

	    // Compute a global average spacing once for loop classification heuristics
	    if (avg_spacing_global <= 0.0) {
		avg_spacing_global = avg_spacing(points);
		if (is_debug()) bu_log("[BallPivot] avg spacing: %.17g\n", avg_spacing_global);
	    }

	    for (double r : radii) {
		if (is_debug()) bu_log("[BallPivot] Radius: %.17g\n", r);
		if (!(r > 0.0)) continue;

		// Cheap first pass: try to re-activate border edges that become valid at this radius
		update_borders(r);

		// Loop-aware escalation: classify border loops and selectively reopen edges using a trimmed probe
		escalate_borders_by_loops(r);

		// Do the reconstruction
		if (e_front.empty()) find_seed(r);
		else expand(r);

		if (is_debug()) bu_log("[BallPivot] After r=%.17g, tris: %zu, fronts: %zu, borders: %zu\n",
			r, mesh.tris.size(), e_front.size(), e_border.size());
	    }

	    // Final orientation BFS pass to ensure per-component consistent winding
	    finalize_orientation_bfs();

	    if (is_debug() && mesh.tris.empty()) {
		bu_log("[BallPivot] No tris generated.\n");
		if (points.empty()) bu_log("  - Empty input cloud.\n");
		else if (normals.empty()) bu_log("  - Empty input normals.\n");
	    }
	    return mesh;
	}

	// Estimate nearest neighbor average spacing
	static double avg_spacing(const std::vector<std::array<fastf_t, 3>>& pts, std::vector<double>* out_dists = nullptr)
	{
	    if (pts.size() < 2) return 0.0;
	    PCAdaptor pc{pts};
	    nanoflann::KDTreeSingleIndexAdaptor<
		nanoflann::L2_Simple_Adaptor<double, PCAdaptor>, PCAdaptor, 3> kd(3, pc, nanoflann::KDTreeSingleIndexAdaptorParams(10));
	    kd.buildIndex();
	    std::vector<double> dists(pts.size(), 0.0);
	    for (size_t i = 0; i < pts.size(); ++i) {
		size_t idx[2];
		double d2[2];
		nanoflann::KNNResultSet<double> rs(2);
		rs.init(idx, d2);
		kd.findNeighbors(rs, pts[i].data(), nanoflann::SearchParameters());
		dists[i] = std::sqrt(d2[1]);
	    }
	    double mean = std::accumulate(dists.begin(), dists.end(), 0.0) / dists.size();
	    if (out_dists) *out_dists = std::move(dists);
	    return mean;
	}

	// Multi-scale radii heuristic
	static std::vector<double> choose_auto_radii(const std::vector<std::array<fastf_t, 3>>& pts)
	{
	    std::vector<double> nn;
	    double avg = avg_spacing(pts, &nn);
	    std::vector<double> radii;
	    if (avg <= 0.0 || nn.size() < 3) {
		if (avg > 0.0) radii.push_back(1.5 * avg);
		return radii;
	    }
	    std::nth_element(nn.begin(), nn.begin() + nn.size()/2, nn.end());
	    double med = nn[nn.size()/2];
	    size_t p90_i = (size_t)std::floor(0.9 * (nn.size() - 1));
	    std::nth_element(nn.begin(), nn.begin() + p90_i, nn.end());
	    double p90 = nn[p90_i];
	    double r0 = 0.9 * med;
	    double r1 = 1.2 * avg;
	    double r2 = std::min(2.5 * avg, 1.1 * p90);
	    if (!(r1 > r0)) r1 = r0 * 1.1;
	    if (!(r2 > r1)) r2 = r1 * 1.1;
	    if (r0 <= 0) r0 = avg * 0.8;
	    if (r1 <= 0) r1 = avg * 1.2;
	    if (r2 <= 0) r2 = avg * 2.0;
	    radii = {r0, r1, r2};
	    return radii;
	}

    private:
	PCAdaptor pc_adapt;
	nanoflann::KDTreeSingleIndexAdaptor<
	    nanoflann::L2_Simple_Adaptor<double, PCAdaptor>, PCAdaptor, 3> kdtree;
	const std::vector<std::array<fastf_t, 3>>& points;
	const std::vector<std::array<fastf_t, 3>>& normals;

	std::vector<VPtr> verts;
	std::list<EPtr> e_front, e_border;
	Mesh mesh;

	// Tunable parameters
	struct Params {
	    double alpha_probe = 1.5;                 // neighbor radius factor for trimmed probe
	    int    max_knn_probe = 256;               // cap candidate count for probe
	    double one_sided_majority = 0.90;         // loop one-sided test (majority fraction)
	    double one_sided_minority = 0.05;         // loop one-sided test (minority fraction)
	    double perim_to_spacing_max = 100.0;      // very large loop perimeter => boundary
	    bool   allow_close_true_boundaries = false;

	    // Normal agreement threshold with nsum (cosine); default cos(60°) = 0.5
	    double nsum_min_dot = 0.5;
	} params;

	// Cached global average spacing (computed in run())
	double avg_spacing_global = 0.0;

	static bool& dbg_flag()
	{
	    static bool f = false;
	    return f;
	}

	static void face_nrm(const std::array<fastf_t, 3>& p0, const std::array<fastf_t, 3>& p1,
		const std::array<fastf_t, 3>& p2, fastf_t n[3])
	{
	    vect_t v1, v2;
	    VSUB2(v1, p1.data(), p0.data());
	    VSUB2(v2, p2.data(), p0.data());
	    VCROSS(n, v1, v2);
	    if (MAGNITUDE(n) > 0.0) VUNITIZE(n);
	    else VSETALL(n, 0.0);
	}

	// Compatibility check: require |dot(n_face, normalize(n0+n1+n2))| >= cos_thresh
	static bool normals_compatible_nsum(VPtr v0, VPtr v1, VPtr v2, double cos_thresh)
	{
	    vect_t nsum;
	    VADD2(nsum, v0->nrm.data(), v1->nrm.data());
	    VADD2(nsum, nsum, v2->nrm.data());
	    double nsum_mag = MAGNITUDE(nsum);
	    if (nsum_mag <= 1e-20) return true; // if degenerate normals, don't block
	    VSCALE(nsum, nsum, 1.0/nsum_mag);

	    fastf_t n[3];
	    face_nrm(v0->pos, v1->pos, v2->pos, n);
	    double d = fabs(VDOT(n, nsum));
	    return d >= cos_thresh;
	}

	// Correct computation of ball center with continuity preference
	// If mp and prefer_center are provided, choose the center that continues rotation
	// (i.e., vector (c-mp) is most aligned with (prefer_center - mp)).
	bool ball_ctr_prefer(int i0, int i1, int i2, double R,
		const std::array<fastf_t, 3>* mp,
		const std::array<fastf_t, 3>* prefer_center,
		std::array<fastf_t, 3>& c)
	{
	    const auto& p0 = points[i0];
	    const auto& p1 = points[i1];
	    const auto& p2 = points[i2];

	    // Triangle normal
	    vect_t e01, e02, ntri;
	    VSUB2(e01, p1.data(), p0.data());
	    VSUB2(e02, p2.data(), p0.data());
	    VCROSS(ntri, e01, e02);
	    double n2 = MAGSQ(ntri);
	    if (n2 < 1e-16) {
		if (is_debug()) bu_log("[ball_ctr] degenerate triangle\n");
		return false;
	    }

	    // Squared side lengths
	    vect_t t;
	    VSUB2(t, p1.data(), p2.data());
	    double a = MAGSQ(t);
	    VSUB2(t, p2.data(), p0.data());
	    double b = MAGSQ(t);
	    VSUB2(t, p0.data(), p1.data());
	    double c2 = MAGSQ(t);

	    // Barycentric weights for circumcenter
	    double abg = a*(b + c2 - a) + b*(a + c2 - b) + c2*(a + b - c2);
	    if (fabs(abg) < 1e-16) {
		if (is_debug()) bu_log("[ball_ctr] degenerate abg\n");
		return false;
	    }
	    double alpha = a * (b + c2 - a) / abg;
	    double beta  = b * (a + c2 - b) / abg;
	    double gamma = c2 * (a + b - c2) / abg;

	    // Circumcenter
	    fastf_t cc[3] = {0,0,0};
	    vect_t tmp;
	    VSCALE(cc, p0.data(), alpha);
	    VSCALE(tmp, p1.data(), beta);
	    VADD2(cc, cc, tmp);
	    VSCALE(tmp, p2.data(), gamma);
	    VADD2(cc, cc, tmp);

	    // Circumradius^2
	    double r2 = a * b * c2;
	    double la = std::sqrt(a), lb = std::sqrt(b), lc = std::sqrt(c2);
	    double denom = (la+lb+lc) * (lb+lc-la) * (lc+la-lb) * (la+lb-lc);
	    if (fabs(denom) < 1e-16) {
		if (is_debug()) bu_log("[ball_ctr] degenerate denom\n");
		return false;
	    }
	    r2 /= denom;

	    double h2 = R*R - r2;
	    if (h2 < 0.0) {
		if (is_debug()) bu_log("[ball_ctr] ball too small\n");
		return false;
	    }

	    // Unit triangle normal
	    vect_t tr_n;
	    VMOVE(tr_n, ntri);
	    VUNITIZE(tr_n);

	    // Two possible centers
	    double h = std::sqrt(h2);
	    fastf_t cpos[3], cneg[3];
	    VSCALE(tmp, tr_n, h);
	    VADD2(cpos, cc, tmp);
	    VSUB2(cneg, cc, tmp);

	    if (mp && prefer_center) {
		// Choose center whose direction from mp aligns best with prefer_center direction
		vect_t v_pref, v_pos, v_neg;
		VSUB2(v_pref, prefer_center->data(), mp->data());
		VSUB2(v_pos, cpos, mp->data());
		VSUB2(v_neg, cneg, mp->data());
		double m_pref = MAGNITUDE(v_pref);
		double m_pos  = MAGNITUDE(v_pos);
		double m_neg  = MAGNITUDE(v_neg);
		if (m_pref > 0) VUNITIZE(v_pref);
		if (m_pos > 0) VUNITIZE(v_pos);
		if (m_neg > 0) VUNITIZE(v_neg);
		double dpos = VDOT(v_pref, v_pos);
		double dneg = VDOT(v_pref, v_neg);
		if (dpos >= dneg) { c = {cpos[0], cpos[1], cpos[2]}; }
		else { c = {cneg[0], cneg[1], cneg[2]}; }
		return true;
	    } else {
		// Fall back to summed normals side
		vect_t nsum;
		VADD2(nsum, normals[i0].data(), normals[i1].data());
		VADD2(nsum, nsum, normals[i2].data());
		if (MAGNITUDE(nsum) > 0.0) VUNITIZE(nsum);
		// Prefer center whose offset direction (±tr_n) aligns with nsum
		if (VDOT(tr_n, nsum) >= 0.0) { c = {cpos[0], cpos[1], cpos[2]}; }
		else { c = {cneg[0], cneg[1], cneg[2]}; }
		return true;
	    }
	}

	// Backward-compatible wrapper (no continuity preference)
	bool ball_ctr(int i0, int i1, int i2, double R, std::array<fastf_t, 3>& c)
	{
	    return ball_ctr_prefer(i0, i1, i2, R, nullptr, nullptr, c);
	}

	void radius_search(const std::array<fastf_t, 3>& p, double r, std::vector<int>& idxs)
	{
	    idxs.clear();
	    nanoflann::SearchParameters nparams;
	    std::vector<nanoflann::ResultItem<unsigned int, double>> matches;
	    kdtree.radiusSearch(p.data(), r*r, matches, nparams);
	    for (auto& m : matches) idxs.push_back((int)m.first);
	}

	EPtr edge_between(VPtr v0, VPtr v1)
	{
	    for (const auto& e0 : v0->edges) {
		for (const auto& e1 : v1->edges) {
		    if ((e0->a == e1->a && e0->b == e1->b) || (e0->a == e1->b && e0->b == e1->a))
			return e0;
		}
	    }
	    return nullptr;
	}

	EPtr get_edge(VPtr v0, VPtr v1, FPtr f)
	{
	    EPtr e = edge_between(v0, v1);
	    if (!e) e = std::make_shared<Edge>(v0, v1);
	    e->add_face(f);
	    v0->edges.insert(e);
	    v1->edges.insert(e);
	    return e;
	}

	void make_face(VPtr v0, VPtr v1, VPtr v2, const std::array<fastf_t, 3>& ctr)
	{
	    auto f = std::make_shared<Face>(v0, v1, v2, ctr);
	    EPtr e0 = get_edge(v0, v1, f);
	    EPtr e1 = get_edge(v1, v2, f);
	    EPtr e2 = get_edge(v2, v0, f);

	    v0->update_type();
	    v1->update_type();
	    v2->update_type();

	    // Decide winding using the average of the three vertex normals nsum
	    vect_t nsum;
	    VADD2(nsum, v0->nrm.data(), v1->nrm.data());
	    VADD2(nsum, nsum, v2->nrm.data());
	    double nsum_mag = MAGNITUDE(nsum);
	    if (nsum_mag > 0.0) VUNITIZE(nsum);

	    fastf_t n[3];
	    face_nrm(v0->pos, v1->pos, v2->pos, n);
	    // If face normal disagrees with nsum, flip winding
	    if (nsum_mag > 0.0 && VDOT(n, nsum) < 0.0) {
		// store as v0, v2, v1
		mesh.tris.push_back({v0->id, v2->id, v1->id});
		// recompute normal after final ordering
		face_nrm(v0->pos, v2->pos, v1->pos, n);
	    } else {
		mesh.tris.push_back({v0->id, v1->id, v2->id});
		// n already computed for this order
	    }
	    mesh.tri_nrms.push_back({n[0], n[1], n[2]});

	    if (is_debug()) bu_log("[make_face] (%d,%d,%d)\n", v0->id, v1->id, v2->id);
	}

	void update_borders(double r)
	{
	    for (auto it = e_border.begin(); it != e_border.end();) {
		EPtr e = *it;
		FPtr f = e->f0;
		if (!f) { ++it; continue; }
		std::array<fastf_t, 3> c;
		if (ball_ctr(f->v0->id, f->v1->id, f->v2->id, r, c)) {
		    std::vector<int> idxs;
		    radius_search(c, r, idxs);
		    bool empty = true;
		    for (auto i : idxs) {
			if (i != f->v0->id && i != f->v1->id && i != f->v2->id) {
			    empty = false;
			    break;
			}
		    }
		    if (empty) {
			e->type = Edge::Front;
			e_front.push_back(e);
			it = e_border.erase(it);
			if (is_debug()) bu_log("[update_borders] border->front\n");
			continue;
		    }
		}
		++it;
	    }
	}

	// --- Loop-aware escalation helpers ---
	struct BorderLoop {
	    std::vector<EPtr> edges; // ordered
	    std::vector<VPtr> verts; // ordered, verts[i] is an endpoint of edges[i], verts[i+1] connects to the other endpoint
	    bool is_closed = false;
	};

	enum class LoopClass { Closeable, TrueBoundary, Unknown };

	void collect_border_loops(std::vector<BorderLoop>& loops)
	{
	    loops.clear();
	    if (e_border.empty()) return;

	    std::unordered_map<VPtr, std::vector<EPtr>> adj;
	    adj.reserve(e_border.size() * 2);
	    for (auto &e : e_border) {
		adj[e->a].push_back(e);
		adj[e->b].push_back(e);
	    }

	    std::unordered_set<EPtr> visited;
	    visited.reserve(e_border.size());

	    // Helper to pick any unvisited edge incident to v
	    auto next_edge = [&](VPtr v)->EPtr {
		auto it = adj.find(v);
		if (it == adj.end()) return nullptr;
		for (auto &e : it->second) if (!visited.count(e)) return e;
		return nullptr;
	    };

	    // First, trace open chains (endpoints with degree 1)
	    for (auto &kv : adj) {
		VPtr start = kv.first;
		if (kv.second.size() != 1) continue; // endpoint only
		// Skip if its sole edge is visited
		bool any_unvisited = false;
		for (auto &ee : kv.second) if (!visited.count(ee)) { any_unvisited = true; break; }
		if (!any_unvisited) continue;

		BorderLoop L;
		VPtr cur_v = start;
		EPtr cur_e = next_edge(cur_v);
		while (cur_e) {
		    visited.insert(cur_e);
		    // Determine the next vertex without mutating edge endpoints
		    VPtr v0 = cur_e->a, v1 = cur_e->b;
		    VPtr other = (v0 == cur_v) ? v1 : v0;
		    L.edges.push_back(cur_e);
		    L.verts.push_back(cur_v);
		    cur_v = other;
		    cur_e = next_edge(cur_v);
		}
		L.verts.push_back(cur_v);
		L.is_closed = false;
		loops.push_back(std::move(L));
	    }

	    // Then, trace closed loops (all vertices degree >= 2)
	    for (auto &e : e_border) {
		if (visited.count(e)) continue;
		BorderLoop L;
		// Start from e->a
		VPtr start_v = e->a;
		VPtr cur_v = start_v;
		EPtr cur_e = e;
		while (cur_e && !visited.count(cur_e)) {
		    visited.insert(cur_e);
		    VPtr v0 = cur_e->a, v1 = cur_e->b;
		    VPtr other = (v0 == cur_v) ? v1 : v0;
		    L.edges.push_back(cur_e);
		    L.verts.push_back(cur_v);
		    cur_v = other;
		    // pick next unvisited edge at cur_v
		    cur_e = next_edge(cur_v);
		}
		// If we returned to start, it's closed
		L.is_closed = (cur_v == start_v);
		if (L.is_closed) L.verts.push_back(cur_v);
		else if (!L.verts.empty()) L.verts.push_back(cur_v);
		if (!L.edges.empty()) loops.push_back(std::move(L));
	    }
	}

	LoopClass classify_loop(const BorderLoop& L, double r_next)
	{
	    // Heuristic 1: One-sided support test using adjacent face normals
	    size_t pos = 0, neg = 0, total = 0;
	    const double band = std::max(3.0 * avg_spacing_global, r_next * 0.25);
	    for (size_t i = 0; i + 1 < L.verts.size() && i < L.edges.size(); ++i) {
		VPtr a = L.verts[i];
		VPtr b = L.verts[i+1];
		EPtr e = L.edges[i];
		if (!e || !e->f0) continue;
		// Face normal from existing triangle
		fastf_t n[3];
		face_nrm(e->f0->v0->pos, e->f0->v1->pos, e->f0->v2->pos, n);
		// Edge midpoint
		std::array<fastf_t,3> mp = { fastf_t(0.5)*(a->pos[0] + b->pos[0]),
		    fastf_t(0.5)*(a->pos[1] + b->pos[1]),
		    fastf_t(0.5)*(a->pos[2] + b->pos[2]) };
		std::vector<int> idxs;
		radius_search(mp, band, idxs);
		for (auto id : idxs) {
		    vect_t d;
		    VSUB2(d, points[id].data(), mp.data());
		    double s = VDOT(d, n);
		    if (s > 1e-12) ++pos;
		    else if (s < -1e-12) ++neg;
		    ++total;
		}
	    }
	    if (total > 0) {
		double frac_pos = (double)pos / (double)total;
		double frac_neg = (double)neg / (double)total;
		double maj = std::max(frac_pos, frac_neg);
		double mino = std::min(frac_pos, frac_neg);
		if (maj > params.one_sided_majority && mino < params.one_sided_minority) {
		    if (is_debug()) bu_log("[classify_loop] one-sided -> TrueBoundary (maj=%.3f, min=%.3f)\n", maj, mino);
		    return LoopClass::TrueBoundary;
		}
	    }

	    // Heuristic 2: Very large perimeter compared to sampling => likely dataset boundary
	    double perim = 0.0;
	    for (size_t i = 0; i + 1 < L.verts.size(); ++i) {
		vect_t d; VSUB2(d, L.verts[i+1]->pos.data(), L.verts[i]->pos.data());
		perim += std::sqrt(VDOT(d,d));
	    }
	    if (avg_spacing_global > 0.0 && perim > params.perim_to_spacing_max * avg_spacing_global) {
		if (is_debug()) bu_log("[classify_loop] large perimeter (%.3f) -> TrueBoundary\n", perim);
		return LoopClass::TrueBoundary;
	    }

	    // Otherwise, attempt feasibility via a trimmed probe on a subset of edges
	    const size_t stride = std::max<size_t>(1, L.edges.size() / 8);
	    for (size_t i = 0; i < L.edges.size(); i += stride) {
		std::array<fastf_t,3> ctr;
		if (probe_candidate_trimmed(L.edges[i], r_next, ctr, params.alpha_probe, params.max_knn_probe)) {
		    if (is_debug()) bu_log("[classify_loop] feasibility -> Closeable\n");
		    return LoopClass::Closeable;
		}
	    }
	    return LoopClass::Unknown;
	}

	VPtr probe_candidate_trimmed(const EPtr& e, double r, std::array<fastf_t,3>& ctr, double alpha, int knn_cap)
	{
	    if (!e || !e->f0) return nullptr;
	    VPtr a = e->a, b = e->b, o = e->opp_vert();
	    if (!o) return nullptr;

	    // Edge midpoint and initial direction (as in find_next_vert)
	    std::array<fastf_t, 3> mp = { fastf_t(0.5) * (a->pos[0] + b->pos[0]),
		fastf_t(0.5) * (a->pos[1] + b->pos[1]),
		fastf_t(0.5) * (a->pos[2] + b->pos[2]) };
	    const auto& c0 = e->f0->center;
	    vect_t v_ab, v_a;
	    VSUB2(v_ab, b->pos.data(), a->pos.data());
	    if (MAGNITUDE(v_ab) > 0.0) VUNITIZE(v_ab); else VSETALL(v_ab, 0.0);
	    VSUB2(v_a, c0.data(), mp.data());
	    if (MAGNITUDE(v_a) > 0.0) VUNITIZE(v_a); else VSETALL(v_a, 0.0);

	    // Neighbor search with reduced radius and capped K
	    std::vector<int> idxs_raw;
	    radius_search(mp, alpha * r, idxs_raw);
	    if (idxs_raw.empty()) return nullptr;
	    std::vector<std::pair<double,int>> idxs;
	    idxs.reserve(idxs_raw.size());
	    for (int id : idxs_raw) {
		vect_t d; VSUB2(d, points[id].data(), mp.data());
		idxs.emplace_back(VDOT(d,d), id);
	    }
	    std::sort(idxs.begin(), idxs.end(), [](const auto& A, const auto& B){ return A.first < B.first; });
	    if ((int)idxs.size() > knn_cap) idxs.resize(knn_cap);

	    // Try candidates in order of proximity; stop at the first valid
	    for (const auto& pr : idxs) {
		VPtr cand = verts[pr.second];
		if (cand == a || cand == b || cand == o) continue;
		// Robust coplanarity/segment guard
		fastf_t val = predicates::adaptive::orient3d<fastf_t>(a->pos.data(), b->pos.data(), o->pos.data(), cand->pos.data());
		if (val == 0) {
		    double d1 = segseg_dist2(mp, cand->pos, a->pos, o->pos);
		    double d2 = segseg_dist2(mp, cand->pos, b->pos, o->pos);
		    if (d1 < 1e-24 || d2 < 1e-24) continue;
		}
		// Normal compatibility w.r.t. nsum
		if (cand->type == Vertex::Inner || !normals_compatible_nsum(cand, a, b, params.nsum_min_dot)) continue;

		// Ball center with continuity preference
		std::array<fastf_t,3> nc;
		if (!ball_ctr_prefer(a->id, b->id, cand->id, r, &mp, &c0, nc)) continue;

		// Empty ball check (trimmed to idxs set)
		bool empty = true;
		for (const auto& pr2 : idxs) {
		    VPtr nb = verts[pr2.second];
		    if (nb == a || nb == b || nb == cand) continue;
		    vect_t d2v; VSUB2(d2v, nc.data(), nb->pos.data());
		    if (MAGNITUDE(d2v) < r - 1e-16) { empty = false; break; }
		}
		if (!empty) continue;
		// All checks passed
		ctr = nc;
		return cand;
	    }
	    return nullptr;
	}

	void escalate_borders_by_loops(double r)
	{
	    if (e_border.empty()) return;
	    std::vector<BorderLoop> loops;
	    collect_border_loops(loops);
	    if (loops.empty()) return;

	    // Decide which edges to reopen
	    std::unordered_set<EPtr> reopen;
	    reopen.reserve(e_border.size());

	    for (const auto& L : loops) {
		LoopClass cls = classify_loop(L, r);
		if (cls == LoopClass::TrueBoundary && !params.allow_close_true_boundaries) continue;
		// For acceptable loops, try a trimmed probe per edge to find at least one candidate
		for (auto& e : L.edges) {
		    std::array<fastf_t,3> ctr;
		    if (probe_candidate_trimmed(e, r, ctr, params.alpha_probe, params.max_knn_probe)) {
			reopen.insert(e);
		    }
		}
	    }

	    // Apply reopen decisions to e_border -> e_front
	    if (reopen.empty()) return;
	    for (auto it = e_border.begin(); it != e_border.end(); ) {
		EPtr e = *it;
		if (reopen.count(e)) {
		    e->type = Edge::Front;
		    e_front.push_back(e);
		    it = e_border.erase(it);
		    if (is_debug()) bu_log("[escalate_borders_by_loops] border->front via loop probe\n");
		} else {
		    ++it;
		}
	    }
	}

	void expand(double r)
	{
	    if (is_debug()) bu_log("[expand] r=%.17g\n", r);
	    while (!e_front.empty()) {
		EPtr e = e_front.front();
		e_front.pop_front();
		if (e->type != Edge::Front) continue;

		std::array<fastf_t, 3> ctr;
		VPtr cand = find_next_vert(e, r, ctr);
		if (!cand) {
		    if (is_debug()) bu_log("[expand] No candidate for (%d,%d)\n", e->a->id, e->b->id);
		    e->type = Edge::Border;
		    e_border.push_back(e);
		    continue;
		}

		// Normal consistency check against nsum
		if (cand->type == Vertex::Inner || !normals_compatible_nsum(cand, e->a, e->b, params.nsum_min_dot)) {
		    e->type = Edge::Border;
		    e_border.push_back(e);
		    continue;
		}

		// Check candidate's edges are not already non-front
		EPtr ea = edge_between(cand, e->a);
		EPtr eb = edge_between(cand, e->b);
		if ((ea && ea->type != Edge::Front) || (eb && eb->type != Edge::Front)) {
		    e->type = Edge::Border;
		    e_border.push_back(e);
		    if (is_debug()) bu_log("[expand] candidate edge not front\n");
		    continue;
		}

		make_face(e->a, e->b, cand, ctr);

		ea = edge_between(cand, e->a);
		eb = edge_between(cand, e->b);
		if (ea && ea->type == Edge::Front) e_front.push_front(ea);
		if (eb && eb->type == Edge::Front) e_front.push_front(eb);
	    }
	}

	void find_seed(double r)
	{
	    bool found = false;
	    for (size_t i = 0; i < verts.size(); ++i) {
		if (verts[i]->type == Vertex::Orphan) {
		    if (try_seed(verts[i], r)) {
			found = true;
			expand(r);
		    }
		}
	    }
	    if (is_debug() && !found)
		bu_log("[find_seed] No seed for r=%.17g\n", r);
	}

	bool try_seed(VPtr v, double r)
	{
	    std::vector<int> idxs;
	    radius_search(v->pos, 2*r, idxs);
	    if (idxs.size() < 3) {
		if (is_debug()) bu_log("[try_seed] few nbrs %d\n", v->id);
		return false;
	    }
	    for (size_t i0 = 0; i0 < idxs.size(); ++i0) {
		VPtr n0 = verts[idxs[i0]];
		if (n0->type != Vertex::Orphan || n0->id == v->id) continue;

		int c2 = -1;
		std::array<fastf_t, 3> ctr;
		for (size_t i1 = i0+1; i1 < idxs.size(); ++i1) {
		    VPtr n1 = verts[idxs[i1]];
		    if (n1->type != Vertex::Orphan || n1->id == v->id) continue;
		    if (try_tri_seed(v, n0, n1, idxs, r, ctr)) {
			c2 = n1->id;
			break;
		    }
		}

		if (c2 >= 0) {
		    VPtr n1 = verts[c2];
		    EPtr e0 = edge_between(v, n1), e1 = edge_between(n0, n1), e2 = edge_between(v, n0);
		    if ((e0 && e0->type != Edge::Front) || (e1 && e1->type != Edge::Front) || (e2 && e2->type != Edge::Front)) continue;

		    make_face(v, n0, n1, ctr);

		    e0 = edge_between(v, n1);
		    e1 = edge_between(n0, n1);
		    e2 = edge_between(v, n0);
		    if (e0 && e0->type == Edge::Front) e_front.push_front(e0);
		    if (e1 && e1->type == Edge::Front) e_front.push_front(e1);
		    if (e2 && e2->type == Edge::Front) e_front.push_front(e2);
		    if (!e_front.empty()) return true;
		}
	    }
	    return false;
	}

	bool try_tri_seed(VPtr v0, VPtr v1, VPtr v2, const std::vector<int>& nb_idx, double r, std::array<fastf_t, 3>& ctr)
	{
	    // Reject if face normal disagrees with nsum beyond threshold (orientation-insensitive)
	    if (!normals_compatible_nsum(v0, v1, v2, params.nsum_min_dot)) {
		if (is_debug()) bu_log("[try_tri_seed] nsum reject (%d,%d,%d)\n", v0->id, v1->id, v2->id);
		return false;
	    }

	    // If adding v2 would make either of the edges inner, reject
	    EPtr e0 = edge_between(v0, v2), e1 = edge_between(v1, v2);
	    if ((e0 && e0->type == Edge::Inner) || (e1 && e1->type == Edge::Inner)) {
		if (is_debug()) bu_log("[try_tri_seed] blocked (%d,%d,%d)\n", v0->id, v1->id, v2->id);
		return false;
	    }

	    // No continuity preference for initial seeds
	    if (!ball_ctr(v0->id, v1->id, v2->id, r, ctr)) {
		if (is_debug()) bu_log("[try_tri_seed] no ctr (%d,%d,%d)\n", v0->id, v1->id, v2->id);
		return false;
	    }

	    // Empty ball test
	    for (auto ni : nb_idx) {
		VPtr v = verts[ni];
		if (v == v0 || v == v1 || v == v2) continue;
		vect_t d;
		VSUB2(d, ctr.data(), v->pos.data());
		if (MAGNITUDE(d) < r - 1e-16) {
		    if (is_debug()) bu_log("[try_tri_seed] not empty (%d,%d,%d)\n", v0->id, v1->id, v2->id);
		    return false;
		}
	    }
	    return true;
	}

	// Squared distance between two 3D segments (for coplanar intersection check)
	static inline double segseg_dist2(const std::array<fastf_t, 3>& P0,
		const std::array<fastf_t, 3>& P1,
		const std::array<fastf_t, 3>& Q0,
		const std::array<fastf_t, 3>& Q1)
	{
	    vect_t u, v, w0;
	    VSUB2(u, P1.data(), P0.data());
	    VSUB2(v, Q1.data(), Q0.data());
	    VSUB2(w0, P0.data(), Q0.data());
	    double a = VDOT(u, u);
	    double b = VDOT(u, v);
	    double c = VDOT(v, v);
	    double d = VDOT(u, w0);
	    double e = VDOT(v, w0);
	    double D = a*c - b*b;
	    double sc, sN, sD = D;
	    double tc, tN, tD = D;
	    const double EPS = 1e-16;

	    if (D < EPS) {
		// Parallel
		sN = 0.0; sD = 1.0;
		tN = e;   tD = c;
	    } else {
		sN = (b*e - c*d);
		tN = (a*e - b*d);
		if (sN < 0.0) {
		    sN = 0.0;
		    tN = e;
		    tD = c;
		} else if (sN > sD) {
		    sN = sD;
		    tN = e + b;
		    tD = c;
		}
	    }

	    if (tN < 0.0) {
		tN = 0.0;
		if (-d < 0.0) {
		    sN = 0.0;
		} else if (-d > a) {
		    sN = sD;
		} else {
		    sN = -d;
		    sD = a;
		}
	    } else if (tN > tD) {
		tN = tD;
		if ((-d + b) < 0.0) {
		    sN = 0.0;
		} else if ((-d + b) > a) {
		    sN = sD;
		} else {
		    sN = (-d + b);
		    sD = a;
		}
	    }

	    sc = (fabs(sN) < EPS ? 0.0 : sN / sD);
	    tc = (fabs(tN) < EPS ? 0.0 : tN / tD);

	    vect_t sP, tP, dP;
	    VSCALE(sP, u, sc); VADD2(sP, sP, P0.data());
	    VSCALE(tP, v, tc); VADD2(tP, tP, Q0.data());
	    VSUB2(dP, sP, tP);
	    return VDOT(dP, dP);
	}

	VPtr find_next_vert(const EPtr& e, double r, std::array<fastf_t, 3>& ctr)
	{
	    VPtr a = e->a, b = e->b, o = e->opp_vert();
	    if (!o) {
		if (is_debug()) bu_log("[find_next_vert] No opp\n");
		return nullptr;
	    }

	    std::array<fastf_t, 3> mp;
	    for (int i = 0; i < 3; ++i) mp[i] = fastf_t(0.5) * (a->pos[i] + b->pos[i]);

	    FPtr f = e->f0;
	    const auto& c0 = f->center;

	    vect_t v_ab, v_a;
	    VSUB2(v_ab, b->pos.data(), a->pos.data());
	    if (MAGNITUDE(v_ab) > 0.0) VUNITIZE(v_ab);
	    else VSETALL(v_ab, 0.0);
	    VSUB2(v_a, c0.data(), mp.data());
	    if (MAGNITUDE(v_a) > 0.0) VUNITIZE(v_a);
	    else VSETALL(v_a, 0.0);

	    std::vector<int> idxs;
	    radius_search(mp, 2*r, idxs);

	    VPtr min_cand = nullptr;
	    double min_ang = 2 * M_PI;

	    for (auto ni : idxs) {
		VPtr cand = verts[ni];
		if (cand == a || cand == b || cand == o) continue;

		// Coplanarity test with robust predicate
		fastf_t val = predicates::adaptive::orient3d<fastf_t>(a->pos.data(), b->pos.data(), o->pos.data(), cand->pos.data());
		bool coplanar = (val == 0);
		if (coplanar) {
		    double d1 = segseg_dist2(mp, cand->pos, a->pos, o->pos);
		    double d2 = segseg_dist2(mp, cand->pos, b->pos, o->pos);
		    if (d1 < 1e-24 || d2 < 1e-24) {
			if (is_debug()) bu_log("[find_next_vert] coplanar intersection for cand %d\n", cand->id);
			continue;
		    }
		}

		// Early normal agreement w.r.t. nsum
		if (!normals_compatible_nsum(cand, a, b, params.nsum_min_dot)) continue;

		// Ball center with continuity preference (towards previous center c0)
		std::array<fastf_t, 3> nc;
		if (!ball_ctr_prefer(a->id, b->id, cand->id, r, &mp, &c0, nc)) continue;

		vect_t v_b;
		VSUB2(v_b, nc.data(), mp.data());
		if (MAGNITUDE(v_b) > 0.0) VUNITIZE(v_b);
		else VSETALL(v_b, 0.0);

		double cosa = clamp(VDOT(v_a, v_b), -1.0, 1.0);
		double ang = std::acos(cosa);

		vect_t cross;
		VCROSS(cross, v_a, v_b);
		double sgn = VDOT(cross, v_ab);
		if (e->orient_sign < 0) sgn = -sgn;
		if (sgn < 0) ang = 2*M_PI - ang;

		if (ang >= min_ang) continue;

		// Empty ball check
		bool empty = true;
		for (auto ni2 : idxs) {
		    VPtr nb = verts[ni2];
		    if (nb == a || nb == b || nb == cand) continue;
		    vect_t d2v;
		    VSUB2(d2v, nc.data(), nb->pos.data());
		    if (MAGNITUDE(d2v) < r - 1e-16) { empty = false; break; }
		}
		if (empty) {
		    min_ang = ang;
		    min_cand = cand;
		    ctr = nc;
		}
	    }

	    if (is_debug() && !min_cand) bu_log("[find_next_vert] No cand for (%d,%d) opp %d\n", a->id, b->id, o->id);
	    return min_cand;
	}

	void finalize_orientation_bfs()
	{
	    size_t F = mesh.tris.size();
	    if (F == 0) return;

	    // Build edge -> faces adjacency (undirected)
	    struct EdgeKey { int u, v; };
	    struct EKHash { size_t operator()(const EdgeKey& k) const { return (size_t)k.u * 1315423911u ^ (size_t)k.v; } };
	    struct EKEq { bool operator()(const EdgeKey& a, const EdgeKey& b) const { return a.u == b.u && a.v == b.v; } };

	    std::unordered_map<EdgeKey, std::vector<int>, EKHash, EKEq> adj;
	    adj.reserve(F * 3);

	    auto add_edge = [&](int fidx, int a, int b) {
		int u = std::min(a, b), v = std::max(a, b);
		EdgeKey k{u, v};
		adj[k].push_back(fidx);
	    };

	    for (int i = 0; i < (int)F; ++i) {
		auto t = mesh.tris[i];
		add_edge(i, t[0], t[1]);
		add_edge(i, t[1], t[2]);
		add_edge(i, t[2], t[0]);
	    }

	    std::vector<char> vis(F, 0);
	    std::vector<int> q; q.reserve(F);

	    for (int s = 0; s < (int)F; ++s) {
		if (vis[s]) continue;
		vis[s] = 1;
		q.clear();
		q.push_back(s);
		size_t qi = 0;
		while (qi < q.size()) {
		    int fi = q[qi++];
		    auto ti = mesh.tris[fi];
		    std::array<std::pair<int,int>,3> fe = {{
			{ti[0], ti[1]}, {ti[1], ti[2]}, {ti[2], ti[0]}
		    }};
		    for (auto e : fe) {
			int u = std::min(e.first, e.second), v = std::max(e.first, e.second);
			EdgeKey k{u, v};
			auto it = adj.find(k);
			if (it == adj.end()) continue;
			for (int fj : it->second) {
			    if (fj == fi || vis[fj]) continue;
			    // If normals disagree, flip neighbor face
			    double d = mesh.tri_nrms[fi][0]*mesh.tri_nrms[fj][0]
				+ mesh.tri_nrms[fi][1]*mesh.tri_nrms[fj][1]
				+ mesh.tri_nrms[fi][2]*mesh.tri_nrms[fj][2];
			    if (d < 0.0) {
				// flip fj
				std::swap(mesh.tris[fj][1], mesh.tris[fj][2]);
				mesh.tri_nrms[fj][0] = -mesh.tri_nrms[fj][0];
				mesh.tri_nrms[fj][1] = -mesh.tri_nrms[fj][1];
				mesh.tri_nrms[fj][2] = -mesh.tri_nrms[fj][2];
			    }
			    vis[fj] = 1;
			    q.push_back(fj);
			}
		    }
		}
	    }
	}

	template <typename T>
	    static inline const T& clamp(const T& v, const T& lo, const T& hi)
	    {
		return (v < lo) ? lo : (hi < v) ? hi : v;
	    }
};

} // namespace bpiv

#ifdef __cplusplus
extern "C" {
#endif

    // ---  C linkage adapter ---
    int ball_pivoting_run(
	    std::vector<int>& faces,
	    const point_t* pts3,
	    const vect_t* nrms3,
	    int n_pts,
	    const double* radii,
	    int n_radii
	    )
    {
	using fastf = fastf_t;
	if (!pts3 || !nrms3 || n_pts <= 0)
	    return 0;

	std::vector<std::array<fastf, 3>> pts(n_pts);
	std::vector<std::array<fastf, 3>> nrms(n_pts);
	for (int i = 0; i < n_pts; ++i) {
	    pts[i][0] = pts3[i][0];
	    pts[i][1] = pts3[i][1];
	    pts[i][2] = pts3[i][2];
	    nrms[i][0] = nrms3[i][0];
	    nrms[i][1] = nrms3[i][1];
	    nrms[i][2] = nrms3[i][2];
	}

	std::vector<double> rvec;
	if (radii && n_radii > 0) {
	    rvec.reserve(n_radii);
	    for (int i = 0; i < n_radii; ++i)
		if (radii[i] > 0.0) rvec.push_back(radii[i]);
	    std::sort(rvec.begin(), rvec.end());
	    rvec.erase(std::unique(rvec.begin(), rvec.end()), rvec.end());
	}

	// Fallback to automatic radii if none supplied/valid
	if (rvec.empty()) {
	    rvec = bpiv::BallPivot::choose_auto_radii(pts);
	    if (rvec.empty()) {
		double avg = bpiv::BallPivot::avg_spacing(pts);
		if (avg <= 0.0) return 0;
		rvec = {1.5 * avg};
	    }
	}

	bpiv::BallPivot bp(pts, nrms);
	auto mesh = bp.run(rvec);
	if (mesh.tris.empty()) return 0;

	faces.clear();
	faces.reserve(mesh.tris.size() * 3);
	for (const auto& t : mesh.tris) {
	    faces.push_back(t[0]);
	    faces.push_back(t[1]);
	    faces.push_back(t[2]);
	}
	return (int)mesh.tris.size();
    }

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop /* end ignoring warnings */
#elif defined(__clang__)
#  pragma clang diagnostic pop /* end ignoring warnings */
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
