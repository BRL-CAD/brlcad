/*
 * BallPivot.hpp
 *
 * Ball Pivoting Algorithm for surface reconstruction from point clouds.
 *
 * Origin:
 *   - Derived and adapted from: https://github.com/isl-org/Open3D
 *     (Ball Pivoting Algorithm implementation in Open3D)
 *   - Adapted by Clifford Yapp, BRL-CAD.
 *   - Header-only, C++17+, lightweight, simplified for integration with BRL-CAD and similar C/C++ codebases.
 *   - Uses nanoflann (https://github.com/jlblancoc/nanoflann) for KD-tree search.
 *   - Uses robust predicates from https://github.com/wlenthe/GeometricPredicates (MIT)
 *
 * License:
 *   Portions of this file are derived from Open3D under the MIT License.
 *   Open3D: www.open3d.org
 *   Copyright (c) 2018-2023 www.open3d.org
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *   THE SOFTWARE.
 *
 *   SPDX-License-Identifier: MIT
 *
 *   Additional modifications and original code: MIT License
 *   (c) 2024 Clifford Yapp, BRL-CAD
 *
 *   See included nanoflann and robust_orient3d.hpp for their respective licenses.
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
#include "nanoflann.hpp"
#include "robust_orient3d.hpp"
extern "C" {
#include "vmath.h"
#include "bu/log.h"
}

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
    inline size_t kdtree_get_point_count() const
    {
	return pts.size();
    }
    inline double kdtree_get_pt(size_t i, size_t d) const
    {
	return (double)pts[i][d];
    }
    template <class BBOX> bool kdtree_get_bbox(BBOX&) const
    {
	return false;
    }
};

// --- Edge ---
struct Edge {
    enum Type { Border = 0, Front = 1, Inner = 2 };
    VPtr a, b;
    FPtr f0 = nullptr, f1 = nullptr;
    Type type = Front;
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
	    for (const auto& e : edges)
		if (e->type != Edge::Inner) {
		    type = Front;
		    return;
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
inline void Edge::add_face(FPtr f)
{
    if (!f0) {
	f0 = f;
	type = Front;
	auto o = opp_vert();
	if (o) {
	    vect_t tn, tmp1, tmp2, nsum;
	    VSUB2(tmp1, b->pos.data(), a->pos.data());
	    VSUB2(tmp2, o->pos.data(), a->pos.data());
	    VCROSS(tn, tmp1, tmp2);
	    VUNITIZE(tn);
	    VADD2(nsum, a->nrm.data(), b->nrm.data());
	    VADD2(nsum, nsum, o->nrm.data());
	    VUNITIZE(nsum);
	    if (VDOT(nsum, tn) < 0) std::swap(b, a);
	}
    } else if (!f1) {
	f1 = f;
	type = Inner;
    }
}
inline VPtr Edge::opp_vert()
{
    if (!f0) return nullptr;
    for (auto v : {
	     f0->v0, f0->v1, f0->v2
	 })
	if (v != a && v != b) return v;
    return nullptr;
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
	for (size_t i = 0; i < pts.size(); ++i)
	    verts.emplace_back(new Vertex((int)i, pts[i], nrms[i]));
    }
    ~BallPivot()
    {
	for (auto v : verts) delete v;
    }

    static void debug(bool flag)
    {
	dbg_flag() = flag;
    }
    static bool is_debug()
    {
	return dbg_flag();
    }

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
	for (double r : radii) {
	    if (is_debug()) bu_log("[BallPivot] Radius: %g\n", r);
	    update_borders(r);
	    if (e_front.empty()) find_seed(r);
	    else expand(r);
	    if (is_debug()) bu_log("[BallPivot] After r=%g, tris: %zu, fronts: %zu, borders: %zu\n",
				       r, mesh.tris.size(), e_front.size(), e_border.size());
	}
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
	nanoflann::L2_Simple_Adaptor<double, PCAdaptor>,
		  PCAdaptor, 3> kd(3, pc, nanoflann::KDTreeSingleIndexAdaptorParams(10));
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
    nanoflann::L2_Simple_Adaptor<double, PCAdaptor>,
	      PCAdaptor, 3> kdtree;
    const std::vector<std::array<fastf_t, 3>>& points;
    const std::vector<std::array<fastf_t, 3>>& normals;

    std::vector<VPtr> verts;
    std::list<EPtr> e_front, e_border;
    Mesh mesh;

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

    static bool robust_ok(const std::array<fastf_t, 3>& a, const std::array<fastf_t, 3>& b,
			  const std::array<fastf_t, 3>& c, const std::array<fastf_t, 3>& na)
    {
	fastf_t n[3];
	face_nrm(a, b, c, n);
	if (VDOT(n, na.data()) < 0) VREVERSE(n, n);
	fastf_t tp[3];
	VADD2(tp, a.data(), na.data());
	int o = robust_orient3d(a.data(), b.data(), c.data(), tp);
	return o > 0;
    }

    bool ball_ctr(int i0, int i1, int i2, double R, std::array<fastf_t, 3>& c)
    {
	const auto& p0 = points[i0];
	const auto& p1 = points[i1];
	const auto& p2 = points[i2];
	vect_t e1, e2, n;
	VSUB2(e1, p1.data(), p0.data());
	VSUB2(e2, p2.data(), p0.data());
	VCROSS(n, e1, e2);
	double n2 = MAGSQ(n);
	double d = 2.0 * n2;
	if (d < 1e-16) {
	    if (is_debug()) bu_log("[ball_ctr] degenerate triangle\n");
	    return false;
	}
	vect_t t;
	VSUB2(t, p1.data(), p2.data());
	double a = MAGSQ(t);
	VSUB2(t, p2.data(), p0.data());
	double b = MAGSQ(t);
	VSUB2(t, p0.data(), p1.data());
	double c2 = MAGSQ(t);
	double abg = a*(b + c2 - a) + b*(a + c2 - b) + c2*(a + b - c2);
	if (fabs(abg) < 1e-16) {
	    if (is_debug()) bu_log("[ball_ctr] degenerate abg\n");
	    return false;
	}
	double alpha = a * (b + c2 - a) / abg;
	double beta  = b * (a + c2 - b) / abg;
	double gamma = c2 * (a + b - c2) / abg;
	fastf_t cc[3] = {0,0,0};
	VSCALE(cc, p0.data(), alpha);
	vect_t t2;
	VSCALE(t2, p1.data(), beta);
	VADD2(cc, cc, t2);
	VSCALE(t2, p2.data(), gamma);
	VADD2(cc, cc, t2);

	double r2 = MAGSQ(e1) * MAGSQ(e2) * MAGSQ(t);
	double a1 = sqrt(MAGSQ(e1)), a2 = sqrt(MAGSQ(e2)), a3 = sqrt(MAGSQ(t));
	double denom = (a1+a2+a3)*(a2+a3-a1)*(a3+a1-a2)*(a1+a2-a3);
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
	vect_t tr_n, nsum;
	VMOVE(tr_n, n);
	VUNITIZE(tr_n);
	VADD2(nsum, normals[i0].data(), normals[i1].data());
	VADD2(nsum, nsum, normals[i2].data());
	VUNITIZE(nsum);
	if (VDOT(tr_n, nsum) < 0) VREVERSE(tr_n, tr_n);
	double h = sqrt(h2);
	VSCALE(t2, tr_n, h);
	VADD2(c.data(), cc, t2);
	return true;
    }

    void radius_search(const std::array<fastf_t, 3>& p, double r, std::vector<int>& idxs)
    {
	idxs.clear();
	nanoflann::SearchParameters params;
	std::vector<nanoflann::ResultItem<unsigned int, double>> matches;
	kdtree.radiusSearch(p.data(), r*r, matches, params);
	for (auto& m : matches) idxs.push_back((int)m.first);
    }

    EPtr edge_between(VPtr v0, VPtr v1)
    {
	for (const auto& e0 : v0->edges)
	    for (const auto& e1 : v1->edges)
		if ((e0->a == e1->a && e0->b == e1->b) || (e0->a == e1->b && e0->b == e1->a))
		    return e0;
	return nullptr;
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
	fastf_t n[3];
	face_nrm(v0->pos, v1->pos, v2->pos, n);
	if (VDOT(n, v0->nrm.data()) > -1e-16)
	    mesh.tris.push_back({v0->id, v1->id, v2->id});
	else
	    mesh.tris.push_back({v0->id, v2->id, v1->id});
	mesh.tri_nrms.push_back({n[0], n[1], n[2]});
	if (is_debug()) bu_log("[make_face] (%d,%d,%d)\n", v0->id, v1->id, v2->id);
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

    void update_borders(double r)
    {
	for (auto it = e_border.begin(); it != e_border.end();) {
	    EPtr e = *it;
	    FPtr f = e->f0;
	    std::array<fastf_t, 3> c;
	    if (ball_ctr(f->v0->id, f->v1->id, f->v2->id, r, c)) {
		std::vector<int> idxs;
		radius_search(c, r, idxs);
		bool empty = true;
		for (auto i : idxs)
		    if (i != f->v0->id && i != f->v1->id && i != f->v2->id) {
			empty = false;
			break;
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

    void expand(double r)
    {
	if (is_debug()) bu_log("[expand] r=%g\n", r);
	while (!e_front.empty()) {
	    EPtr e = e_front.front();
	    e_front.pop_front();
	    if (e->type != Edge::Front) continue;
	    std::array<fastf_t, 3> ctr;
	    VPtr cand = find_next_vert(e, r, ctr);
	    if (!cand) {
		if (is_debug()) bu_log("[expand] No candidate for (%d,%d)\n", e->a->id, e->b->id);
	    }
	    if (!cand || cand->type == Vertex::Inner || !robust_ok(cand->pos, e->a->pos, e->b->pos, cand->nrm)) {
		e->type = Edge::Border;
		e_border.push_back(e);
		continue;
	    }
	    EPtr ea = edge_between(cand, e->a), eb = edge_between(cand, e->b);
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
	    bu_log("[find_seed] No seed for r=%g\n", r);
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
	if (!robust_ok(v0->pos, v1->pos, v2->pos, v0->nrm)) {
	    if (is_debug()) bu_log("[try_tri_seed] orient (%d,%d,%d)\n", v0->id, v1->id, v2->id);
	    return false;
	}
	EPtr e0 = edge_between(v0, v2), e1 = edge_between(v1, v2);
	if ((e0 && e0->type == Edge::Inner) || (e1 && e1->type == Edge::Inner)) {
	    if (is_debug()) bu_log("[try_tri_seed] blocked (%d,%d,%d)\n", v0->id, v1->id, v2->id);
	    return false;
	}
	if (!ball_ctr(v0->id, v1->id, v2->id, r, ctr)) {
	    if (is_debug()) bu_log("[try_tri_seed] no ctr (%d,%d,%d)\n", v0->id, v1->id, v2->id);
	    return false;
	}
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
	vect_t v, va;
	VSUB2(v, b->pos.data(), a->pos.data());
	VUNITIZE(v);
	VSUB2(va, c0.data(), mp.data());
	VUNITIZE(va);
	std::vector<int> idxs;
	radius_search(mp, 2*r, idxs);

	VPtr min_cand = nullptr;
	double min_ang = 2 * M_PI;
	for (auto ni : idxs) {
	    VPtr cand = verts[ni];
	    if (cand==a || cand==b || cand==o) continue;
	    int copl = robust_orient3d(a->pos.data(), b->pos.data(), o->pos.data(), cand->pos.data());
	    if (copl == 0) continue;
	    std::array<fastf_t, 3> nc;
	    if (!ball_ctr(a->id, b->id, cand->id, r, nc)) continue;
	    vect_t vb;
	    VSUB2(vb, nc.data(), mp.data());
	    VUNITIZE(vb);
	    double cosa = clamp(VDOT(va, vb), -1.0, 1.0);
	    double ang = acos(cosa);
	    vect_t cross;
	    VCROSS(cross, va, vb);
	    if (VDOT(cross, v) < 0) ang = 2*M_PI - ang;
	    if (ang >= min_ang) continue;
	    bool empty = true;
	    for (auto ni2 : idxs) {
		VPtr nb = verts[ni2];
		if (nb==a || nb==b || nb==cand) continue;
		vect_t d2;
		VSUB2(d2, nc.data(), nb->pos.data());
		if (MAGNITUDE(d2) < r - 1e-16) {
		    empty = false;
		    break;
		}
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

/*
Usage:

// Automatic multi-scale fallback if none provided:
std::vector<int> faces;
int tri_cnt = ball_pivoting_run(faces, pts, nrms, n_pts, nullptr, 0);

// Explicit radii:
double radii[] = {r_small, r_medium, r_large};
int tri_cnt2 = ball_pivoting_run(faces, pts, nrms, n_pts, radii, 3);

*/

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
