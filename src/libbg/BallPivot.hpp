/*
Header-only Ball Pivoting Algorithm implementation using nanoflann (2.x) and robust predicates.

- KD-tree: nanoflann (https://github.com/jlblancoc/nanoflann)
- Robust predicates: robust_orient3d.hpp (adapted from https://github.com/wlenthe/GeometricPredicates)
- MIT License

Features:
- Optional debugging telemetry: set BallPivoting::set_debug(true) to enable verbose diagnostics.

Usage:
  - Your PointCloud must provide:
      - points: std::vector<std::array<fastf_t, 3>>
      - normals: std::vector<std::array<fastf_t, 3>>
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
#include <iostream>
#include <sstream>
#include <array>
#include <nanoflann.hpp>
#include "robust_orient3d.hpp"
extern "C" {
#include "vmath.h"
}

namespace ball_pivoting {

// Forward declarations for pointer types
struct BallPivotingVertex;
struct BallPivotingEdge;
struct BallPivotingTriangle;

// Smart pointers
using BallPivotingVertexPtr = BallPivotingVertex*;
using BallPivotingEdgePtr = std::shared_ptr<BallPivotingEdge>;
using BallPivotingTrianglePtr = std::shared_ptr<BallPivotingTriangle>;

// Point cloud adaptor for nanoflann (std::array<fastf_t,3>)
struct PointCloudAdaptor {
    const std::vector<std::array<fastf_t, 3>>& pts;
    inline size_t kdtree_get_point_count() const { return pts.size(); }
    inline double kdtree_get_pt(const size_t idx, const size_t dim) const {
        return static_cast<double>(pts[idx][dim]);
    }
    template <class BBOX>
    bool kdtree_get_bbox(BBOX&) const { return false; }
};

// Edge definition must come first for correct use in BallPivotingVertex
struct BallPivotingEdge {
    enum Type { Border = 0, Front = 1, Inner = 2 };
    BallPivotingVertexPtr source;
    BallPivotingVertexPtr target;
    BallPivotingTrianglePtr triangle0 = nullptr;
    BallPivotingTrianglePtr triangle1 = nullptr;
    Type type = Front;

    BallPivotingEdge(BallPivotingVertexPtr s, BallPivotingVertexPtr t)
        : source(s), target(t) {}

    void AddAdjacentTriangle(BallPivotingTrianglePtr tri);
    BallPivotingVertexPtr GetOppositeVertex();
};

// Vertex definition
struct BallPivotingVertex {
    enum Type { Orphan = 0, Front = 1, Inner = 2 };
    int idx;
    std::array<fastf_t, 3> point;
    std::array<fastf_t, 3> normal;
    std::unordered_set<BallPivotingEdgePtr> edges;
    Type type = Orphan;

    BallPivotingVertex(int i, const std::array<fastf_t, 3>& p, const std::array<fastf_t, 3>& n)
        : idx(i), point(p), normal(n) {}

    void UpdateType() {
        if (edges.empty()) {
            type = Orphan;
        } else {
            for (const auto& e : edges)
                if (e->type != BallPivotingEdge::Inner) {
                    type = Front; return;
                }
            type = Inner;
        }
    }
};

// Triangle definition
struct BallPivotingTriangle {
    BallPivotingVertexPtr vert0, vert1, vert2;
    std::array<fastf_t, 3> ball_center;
    BallPivotingTriangle(BallPivotingVertexPtr v0, BallPivotingVertexPtr v1, BallPivotingVertexPtr v2, const std::array<fastf_t, 3>& c)
        : vert0(v0), vert1(v1), vert2(v2), ball_center(c) {}
};

// AddAdjacentTriangle and GetOppositeVertex implementations
inline void BallPivotingEdge::AddAdjacentTriangle(BallPivotingTrianglePtr tri) {
    if (triangle0 == nullptr) {
        triangle0 = tri; type = Front;
        auto opp = GetOppositeVertex();
        if (opp) {
            vect_t tr_norm, tmp1, tmp2, pt_norm;
            VSUB2(tmp1, target->point.data(), source->point.data());
            VSUB2(tmp2, opp->point.data(), source->point.data());
            VCROSS(tr_norm, tmp1, tmp2);
            VUNITIZE(tr_norm);

            VADD2(pt_norm, source->normal.data(), target->normal.data());
            VADD2(pt_norm, pt_norm, opp->normal.data());
            VUNITIZE(pt_norm);

            if (VDOT(pt_norm, tr_norm) < 0) std::swap(target, source);
        }
    } else if (triangle1 == nullptr) {
        triangle1 = tri; type = Inner;
    }
}
inline BallPivotingVertexPtr BallPivotingEdge::GetOppositeVertex() {
    if (!triangle0) return nullptr;
    for (auto v : {triangle0->vert0, triangle0->vert1, triangle0->vert2}) {
        if (v != source && v != target) return v;
    }
    return nullptr;
}

// Output mesh structure
struct Mesh {
    std::vector<std::array<fastf_t, 3>> vertices;
    std::vector<std::array<fastf_t, 3>> normals;
    std::vector<std::array<int, 3>> triangles;
    std::vector<std::array<fastf_t, 3>> triangle_normals;
};

class BallPivoting {
public:
    BallPivoting(const std::vector<std::array<fastf_t, 3>>& pts, const std::vector<std::array<fastf_t, 3>>& nrm)
        : pc_adaptor{pts}, kdtree(3, pc_adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(10)), points(pts), normals(nrm) {
        kdtree.buildIndex();
        mesh.vertices = pts;
        mesh.normals = nrm;
        for (size_t i = 0; i < pts.size(); ++i)
            vertices.emplace_back(new BallPivotingVertex((int)i, pts[i], nrm[i]));
    }
    ~BallPivoting() { for (auto v : vertices) delete v; }

    static void set_debug(bool flag) { debug_enabled() = flag; }
    static std::string get_debug_log() { return debug_log().str(); }
    static void clear_debug_log() { debug_log().str(""); debug_log().clear(); }

    Mesh Run(const std::vector<double>& radii) {
        mesh.triangles.clear();
        mesh.triangle_normals.clear();
        edge_front.clear();
        border_edges.clear();
        for (auto v : vertices) {
            v->edges.clear();
            v->type = BallPivotingVertex::Orphan;
        }
        clear_debug_log();
        if (debug_enabled()) {
            log() << "[BallPivoting] Run called with " << points.size() << " points, "
                  << normals.size() << " normals, " << radii.size() << " radii\n";
        }
        for (double radius : radii) {
            if (debug_enabled()) {
                log() << "[BallPivoting] Trying radius: " << radius << "\n";
            }
            UpdateBorderEdges(radius);
            if (edge_front.empty()) {
                FindSeedTriangle(radius);
            } else {
                ExpandTriangulation(radius);
            }
            if (debug_enabled()) {
                log() << "[BallPivoting] After radius " << radius << ", mesh has "
                      << mesh.triangles.size() << " triangles, "
                      << edge_front.size() << " front edges, "
                      << border_edges.size() << " border edges\n";
            }
        }
        if (debug_enabled() && mesh.triangles.empty()) {
            log() << "[BallPivoting] No mesh triangles generated for input.\n";
            if (points.empty()) {
                log() << "  - Reason: Input point cloud is empty.\n";
            } else if (normals.empty()) {
                log() << "  - Reason: Input normals are empty.\n";
            }
        }
        return mesh;
    }

    static double estimate_average_spacing(const std::vector<std::array<fastf_t, 3>>& points, std::vector<double>* dists_out = nullptr) {
        if (points.size() < 2) return 0.0;
        PointCloudAdaptor pc_adaptor{points};
        nanoflann::KDTreeSingleIndexAdaptor<
            nanoflann::L2_Simple_Adaptor<double, PointCloudAdaptor>,
            PointCloudAdaptor, 3> kdtree(3, pc_adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(10));
        kdtree.buildIndex();
        std::vector<double> dists(points.size(), 0.0);
        for (size_t i = 0; i < points.size(); ++i) {
            size_t idx[2];
            double dist2[2];
            nanoflann::KNNResultSet<double> resultSet(2);
            resultSet.init(idx, dist2);
            kdtree.findNeighbors(resultSet, points[i].data(), nanoflann::SearchParameters());
            dists[i] = std::sqrt(dist2[1]);
        }
        double mean = std::accumulate(dists.begin(), dists.end(), 0.0) / dists.size();
        if (dists_out) *dists_out = std::move(dists);
        return mean;
    }

private:
    PointCloudAdaptor pc_adaptor;
    nanoflann::KDTreeSingleIndexAdaptor<
        nanoflann::L2_Simple_Adaptor<double, PointCloudAdaptor>,
        PointCloudAdaptor, 3> kdtree;
    const std::vector<std::array<fastf_t, 3>>& points;
    const std::vector<std::array<fastf_t, 3>>& normals;

    std::vector<BallPivotingVertexPtr> vertices;
    std::list<BallPivotingEdgePtr> edge_front, border_edges;
    Mesh mesh;

    static bool& debug_enabled() { static bool flag = false; return flag; }
    static std::ostringstream& debug_log() { static std::ostringstream ss; return ss; }
    static std::ostream& log() { return debug_log(); }

    static void FaceNormal(const std::array<fastf_t, 3>& v0, const std::array<fastf_t, 3>& v1, const std::array<fastf_t, 3>& v2, fastf_t out_n[3]) {
        vect_t tmp1, tmp2;
        VSUB2(tmp1, v1.data(), v0.data());
        VSUB2(tmp2, v2.data(), v0.data());
        VCROSS(out_n, tmp1, tmp2);
        if (MAGNITUDE(out_n) > 0.0) {
            VUNITIZE(out_n);
        } else {
            VSETALL(out_n, 0.0);
        }
    }

    static bool RobustCompatible(const std::array<fastf_t, 3>& v0, const std::array<fastf_t, 3>& v1, const std::array<fastf_t, 3>& v2,
                                const std::array<fastf_t, 3>& n0, const std::array<fastf_t, 3>& n1, const std::array<fastf_t, 3>& n2) {
        fastf_t normal[3];
        FaceNormal(v0, v1, v2, normal);
        if (VDOT(normal, n0.data()) < 0) VREVERSE(normal, normal);
        fastf_t test_pt[3];
        VADD2(test_pt, v0.data(), n0.data());
        int orient = robust_orient3d(v0.data(), v1.data(), v2.data(), test_pt);
        return orient > 0;
    }

    bool ComputeBallCenter(int i1, int i2, int i3, double R, std::array<fastf_t, 3>& c) {
        const auto& v1 = points[i1];
        const auto& v2 = points[i2];
        const auto& v3 = points[i3];
        vect_t e12, e13, n;
        VSUB2(e12, v2.data(), v1.data());
        VSUB2(e13, v3.data(), v1.data());
        VCROSS(n, e12, e13);
        double n2 = MAGSQ(n);
        double d = 2.0 * n2;
        if (d < 1e-16) {
            if (debug_enabled()) log() << "[ComputeBallCenter] Degenerate triangle (area ~ 0)\n";
            return false;
        }
        vect_t vtmp;
        VSUB2(vtmp, v2.data(), v3.data());
        double a = MAGSQ(vtmp);
        VSUB2(vtmp, v3.data(), v1.data());
        double b = MAGSQ(vtmp);
        VSUB2(vtmp, v1.data(), v2.data());
        double c2 = MAGSQ(vtmp);
        double abg = a*(b + c2 - a) + b*(a + c2 - b) + c2*(a + b - c2);
        if (fabs(abg) < 1e-16) {
            if (debug_enabled()) log() << "[ComputeBallCenter] Degenerate triangle (abg ~ 0)\n";
            return false;
        }
        double alpha = a * (b + c2 - a) / abg;
        double beta = b * (a + c2 - b) / abg;
        double gamma = c2 * (a + b - c2) / abg;
        fastf_t circ_c[3] = {0,0,0};
        VSCALE(circ_c, v1.data(), alpha);
        vect_t tmp2;
        VSCALE(tmp2, v2.data(), beta); VADD2(circ_c, circ_c, tmp2);
        VSCALE(tmp2, v3.data(), gamma); VADD2(circ_c, circ_c, tmp2);

        double r2 = MAGSQ(e12) * MAGSQ(e13) * MAGSQ(vtmp);
        double a1 = sqrt(MAGSQ(e12)), a2 = sqrt(MAGSQ(e13)), a3 = sqrt(MAGSQ(vtmp));
        double denom = (a1+a2+a3)*(a2+a3-a1)*(a3+a1-a2)*(a1+a2-a3);
        if (fabs(denom) < 1e-16) {
            if (debug_enabled()) log() << "[ComputeBallCenter] Degenerate triangle (circumradius denominator ~ 0)\n";
            return false;
        }
        r2 /= denom;
        double h2 = R*R - r2;
        if (h2 < 0.0) {
            if (debug_enabled()) log() << "[ComputeBallCenter] Ball radius too small, no sphere possible\n";
            return false;
        }
        vect_t tr_norm, pt_norm;
        VMOVE(tr_norm, n); VUNITIZE(tr_norm);
        VADD2(pt_norm, normals[i1].data(), normals[i2].data()); VADD2(pt_norm, pt_norm, normals[i3].data()); VUNITIZE(pt_norm);
        if (VDOT(tr_norm, pt_norm) < 0) VREVERSE(tr_norm, tr_norm);
        double h = sqrt(h2);
        VSCALE(tmp2, tr_norm, h);
        VADD2(c.data(), circ_c, tmp2);
        return true;
    }

    void SearchRadius(const std::array<fastf_t, 3>& query, double radius, std::vector<int>& out_indices) {
        out_indices.clear();
        nanoflann::SearchParameters p;
        std::vector<nanoflann::ResultItem<unsigned int, double>> ret_matches;
        kdtree.radiusSearch(query.data(), radius*radius, ret_matches, p);
        for (auto& m : ret_matches)
            out_indices.push_back(static_cast<int>(m.first));
    }

    BallPivotingEdgePtr GetLinkingEdge(BallPivotingVertexPtr v0, BallPivotingVertexPtr v1) {
        for (const auto& e0 : v0->edges)
            for (const auto& e1 : v1->edges)
                if ((e0->source == e1->source && e0->target == e1->target) ||
                    (e0->source == e1->target && e0->target == e1->source))
                    return e0;
        return nullptr;
    }

    void CreateTriangle(BallPivotingVertexPtr v0, BallPivotingVertexPtr v1, BallPivotingVertexPtr v2, const std::array<fastf_t, 3>& center) {
        auto tri = std::make_shared<BallPivotingTriangle>(v0, v1, v2, center);
        BallPivotingEdgePtr e0 = GetOrCreateEdge(v0, v1, tri);
        BallPivotingEdgePtr e1 = GetOrCreateEdge(v1, v2, tri);
        BallPivotingEdgePtr e2 = GetOrCreateEdge(v2, v0, tri);
        v0->UpdateType(); v1->UpdateType(); v2->UpdateType();
        fastf_t f_n[3];
        FaceNormal(v0->point, v1->point, v2->point, f_n);
        if (VDOT(f_n, v0->normal.data()) > -1e-16)
            mesh.triangles.push_back({v0->idx, v1->idx, v2->idx});
        else
            mesh.triangles.push_back({v0->idx, v2->idx, v1->idx});
        mesh.triangle_normals.push_back({f_n[0], f_n[1], f_n[2]});
        if (debug_enabled()) log() << "[CreateTriangle] (" << v0->idx << "," << v1->idx << "," << v2->idx << ")\n";
    }

    BallPivotingEdgePtr GetOrCreateEdge(BallPivotingVertexPtr v0, BallPivotingVertexPtr v1, BallPivotingTrianglePtr tri) {
        BallPivotingEdgePtr e = GetLinkingEdge(v0, v1);
        if (!e) e = std::make_shared<BallPivotingEdge>(v0, v1);
        e->AddAdjacentTriangle(tri);
        v0->edges.insert(e); v1->edges.insert(e);
        return e;
    }

    void UpdateBorderEdges(double radius) {
        for (auto it = border_edges.begin(); it != border_edges.end(); ) {
            BallPivotingEdgePtr e = *it;
            BallPivotingTrianglePtr tri = e->triangle0;
            std::array<fastf_t, 3> c;
            if (ComputeBallCenter(tri->vert0->idx, tri->vert1->idx, tri->vert2->idx, radius, c)) {
                std::vector<int> indices; SearchRadius(c, radius, indices);
                bool empty_ball = true;
                for (auto idx : indices)
                    if (idx != tri->vert0->idx && idx != tri->vert1->idx && idx != tri->vert2->idx) {
                        empty_ball = false; break;
                    }
                if (empty_ball) {
                    e->type = BallPivotingEdge::Front;
                    edge_front.push_back(e);
                    it = border_edges.erase(it);
                    if (debug_enabled()) log() << "[UpdateBorderEdges] Added edge to front\n";
                    continue;
                }
            }
            ++it;
        }
    }

    void ExpandTriangulation(double radius) {
        if (debug_enabled()) log() << "[ExpandTriangulation] radius=" << radius << "\n";
        while (!edge_front.empty()) {
            BallPivotingEdgePtr e = edge_front.front(); edge_front.pop_front();
            if (e->type != BallPivotingEdge::Front) continue;
            std::array<fastf_t, 3> center;
            BallPivotingVertexPtr candidate = FindCandidateVertex(e, radius, center);
            if (!candidate) {
                if (debug_enabled()) log() << "[ExpandTriangulation] No candidate found for edge (" << e->source->idx << "," << e->target->idx << ")\n";
            }
            if (!candidate || candidate->type == BallPivotingVertex::Inner
                || !RobustCompatible(candidate->point, e->source->point, e->target->point, candidate->normal, e->source->normal, e->target->normal)) {
                e->type = BallPivotingEdge::Border;
                border_edges.push_back(e);
                continue;
            }
            BallPivotingEdgePtr e0 = GetLinkingEdge(candidate, e->source);
            BallPivotingEdgePtr e1 = GetLinkingEdge(candidate, e->target);
            if ((e0 && e0->type != BallPivotingEdge::Front) ||
                (e1 && e1->type != BallPivotingEdge::Front)) {
                e->type = BallPivotingEdge::Border;
                border_edges.push_back(e);
                if (debug_enabled()) log() << "[ExpandTriangulation] Candidate edge already not front; skipping\n";
                continue;
            }
            CreateTriangle(e->source, e->target, candidate, center);
            e0 = GetLinkingEdge(candidate, e->source);
            e1 = GetLinkingEdge(candidate, e->target);
            if (e0 && e0->type == BallPivotingEdge::Front) edge_front.push_front(e0);
            if (e1 && e1->type == BallPivotingEdge::Front) edge_front.push_front(e1);
        }
    }

    void FindSeedTriangle(double radius) {
        bool found_seed = false;
        for (size_t i = 0; i < vertices.size(); ++i) {
            if (vertices[i]->type == BallPivotingVertex::Orphan) {
                if (TrySeed(vertices[i], radius)) {
                    found_seed = true;
                    ExpandTriangulation(radius);
                }
            }
        }
        if (debug_enabled() && !found_seed) {
            log() << "[FindSeedTriangle] No valid seed triangle found for radius " << radius << "\n";
        }
    }

    bool TrySeed(BallPivotingVertexPtr v, double radius) {
        std::vector<int> indices; SearchRadius(v->point, 2*radius, indices);
        if (indices.size() < 3) {
            if (debug_enabled()) log() << "[TrySeed] Not enough neighbors for point " << v->idx << "\n";
            return false;
        }
        for (size_t i0 = 0; i0 < indices.size(); ++i0) {
            BallPivotingVertexPtr nb0 = vertices[indices[i0]];
            if (nb0->type != BallPivotingVertex::Orphan || nb0->idx == v->idx) continue;
            int candidate2 = -1;
            std::array<fastf_t, 3> center;
            for (size_t i1 = i0+1; i1 < indices.size(); ++i1) {
                BallPivotingVertexPtr nb1 = vertices[indices[i1]];
                if (nb1->type != BallPivotingVertex::Orphan || nb1->idx == v->idx) continue;
                if (TryTriangleSeed(v, nb0, nb1, indices, radius, center)) {
                    candidate2 = nb1->idx; break;
                }
            }
            if (candidate2 >= 0) {
                BallPivotingVertexPtr nb1 = vertices[candidate2];
                BallPivotingEdgePtr e0 = GetLinkingEdge(v, nb1);
                BallPivotingEdgePtr e1 = GetLinkingEdge(nb0, nb1);
                BallPivotingEdgePtr e2 = GetLinkingEdge(v, nb0);
                if ((e0 && e0->type != BallPivotingEdge::Front) ||
                    (e1 && e1->type != BallPivotingEdge::Front) ||
                    (e2 && e2->type != BallPivotingEdge::Front)) continue;
                CreateTriangle(v, nb0, nb1, center);
                e0 = GetLinkingEdge(v, nb1); e1 = GetLinkingEdge(nb0, nb1); e2 = GetLinkingEdge(v, nb0);
                if (e0 && e0->type == BallPivotingEdge::Front) edge_front.push_front(e0);
                if (e1 && e1->type == BallPivotingEdge::Front) edge_front.push_front(e1);
                if (e2 && e2->type == BallPivotingEdge::Front) edge_front.push_front(e2);
                if (!edge_front.empty()) return true;
            }
        }
        return false;
    }

    bool TryTriangleSeed(BallPivotingVertexPtr v0, BallPivotingVertexPtr v1, BallPivotingVertexPtr v2,
                         const std::vector<int>& nb_indices, double radius, std::array<fastf_t, 3>& center) {
        if (!RobustCompatible(v0->point, v1->point, v2->point, v0->normal, v1->normal, v2->normal)) {
            if (debug_enabled()) log() << "[TryTriangleSeed] Triangle (" << v0->idx << "," << v1->idx << "," << v2->idx << ") orientation not compatible\n";
            return false;
        }
        BallPivotingEdgePtr e0 = GetLinkingEdge(v0, v2), e1 = GetLinkingEdge(v1, v2);
        if ((e0 && e0->type == BallPivotingEdge::Inner) ||
            (e1 && e1->type == BallPivotingEdge::Inner)) {
            if (debug_enabled()) log() << "[TryTriangleSeed] Triangle (" << v0->idx << "," << v1->idx << "," << v2->idx << ") blocked by inner edge\n";
            return false;
        }
        if (!ComputeBallCenter(v0->idx, v1->idx, v2->idx, radius, center)) {
            if (debug_enabled()) log() << "[TryTriangleSeed] Triangle (" << v0->idx << "," << v1->idx << "," << v2->idx << ") cannot compute ball center\n";
            return false;
        }
        for (auto nbidx : nb_indices) {
            BallPivotingVertexPtr v = vertices[nbidx];
            if (v == v0 || v == v1 || v == v2) continue;
            vect_t diff;
            VSUB2(diff, center.data(), v->point.data());
            if (MAGNITUDE(diff) < radius - 1e-16) {
                if (debug_enabled()) log() << "[TryTriangleSeed] Ball not empty for triangle (" << v0->idx << "," << v1->idx << "," << v2->idx << ")\n";
                return false;
            }
        }
        return true;
    }

    BallPivotingVertexPtr FindCandidateVertex(const BallPivotingEdgePtr& edge, double radius, std::array<fastf_t, 3>& candidate_center) {
        BallPivotingVertexPtr src = edge->source, tgt = edge->target, opp = edge->GetOppositeVertex();
        if (!opp) {
            if (debug_enabled()) log() << "[FindCandidateVertex] No opposite vertex\n";
            return nullptr;
        }
        std::array<fastf_t, 3> mp;
        for (int i = 0; i < 3; ++i) mp[i] = fastf_t(0.5) * (src->point[i] + tgt->point[i]);
        BallPivotingTrianglePtr tri = edge->triangle0;
        const auto& center = tri->ball_center;
        vect_t v, a;
        VSUB2(v, tgt->point.data(), src->point.data()); VUNITIZE(v);
        VSUB2(a, center.data(), mp.data()); VUNITIZE(a);
        std::vector<int> indices; SearchRadius(mp, 2*radius, indices);
        BallPivotingVertexPtr min_candidate = nullptr;
        double min_angle = 2 * M_PI;
        for (auto nbidx : indices) {
            BallPivotingVertexPtr candidate = vertices[nbidx];
            if (candidate==src || candidate==tgt || candidate==opp) continue;
            int coplanar = robust_orient3d(src->point.data(), tgt->point.data(), opp->point.data(), candidate->point.data());
            if (coplanar == 0) continue;
            std::array<fastf_t, 3> new_center;
            if (!ComputeBallCenter(src->idx, tgt->idx, candidate->idx, radius, new_center)) continue;
            vect_t b;
            VSUB2(b, new_center.data(), mp.data()); VUNITIZE(b);
            double cos_theta = clamp(VDOT(a, b), -1.0, 1.0);
            double angle = acos(cos_theta);
            vect_t cross;
            VCROSS(cross, a, b);
            if (VDOT(cross, v) < 0) angle = 2*M_PI - angle;
            if (angle >= min_angle) continue;
            bool empty_ball = true;
            for (auto nbidx2 : indices) {
                BallPivotingVertexPtr nb = vertices[nbidx2];
                if (nb==src || nb==tgt || nb==candidate) continue;
                vect_t d2;
                VSUB2(d2, new_center.data(), nb->point.data());
                if (MAGNITUDE(d2) < radius - 1e-16) {
                    empty_ball = false; break;
                }
            }
            if (empty_ball) {
                min_angle = angle;
                min_candidate = candidate;
                candidate_center = new_center;
            }
        }
        if (debug_enabled() && !min_candidate) {
            log() << "[FindCandidateVertex] No candidate found for edge ("
                  << src->idx << "," << tgt->idx << ") with opp " << opp->idx << "\n";
        }
        return min_candidate;
    }

    template <typename T>
    static inline const T& clamp(const T& v, const T& lo, const T& hi) {
        return (v < lo) ? lo : (hi < v) ? hi : v;
    }
};

} // namespace ball_pivoting

/*
Debugging/Telemetry usage:

// Enable telemetry
ball_pivoting::BallPivoting::set_debug(true);

// Run the algorithm as usual
auto mesh = bp.Run({radius});

// If the mesh is empty, get logs:
if(mesh.triangles.empty()) {
    std::cerr << ball_pivoting::BallPivoting::get_debug_log();
}

// You can clear the log any time:
ball_pivoting::BallPivoting::clear_debug_log();
*/