/*
 * BallPivot.h (original, non-adaptive, with debugging output)
 *
 * Stand-alone Ball Pivoting Algorithm implementation for triangle mesh reconstruction,
 * using BRL-CAD's vmath.h and bg/trimesh.h and nanoflann for KD-tree acceleration.
 *
 * Portions of this code are adapted from Open3D's SurfaceReconstructionBallPivoting.cpp.
 *
 * -----------------------------------------------------------------------------
 * Open3D License (MIT):
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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * -----------------------------------------------------------------------------
 */

#pragma once

#include <vector>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <cassert>
#include <limits>
#include <algorithm>
#include <tuple>
#include <cstring>
#include <cstdio>

#include "vmath.h"
#include "bg/trimesh.h"
#include "nanoflann.hpp"

#define BG_3D_BALLPIVOT_DEFAULT_RADIUS -1

struct PointCloudAdaptor {
    const point_t* pts;
    size_t N;

    PointCloudAdaptor(const point_t* points, size_t n) : pts(points), N(n) {}

    inline size_t kdtree_get_point_count() const { return N; }
    inline fastf_t kdtree_get_pt(const size_t idx, const size_t dim) const { return pts[idx][dim]; }
    template <class BBOX> bool kdtree_get_bbox(BBOX&) const { return false; }
};

using KDTree = nanoflann::KDTreeSingleIndexAdaptor<
    nanoflann::L2_Simple_Adaptor<fastf_t, PointCloudAdaptor>,
    PointCloudAdaptor,
    3
>;

struct EdgeKey {
    int v0, v1;
    EdgeKey(int a, int b) : v0(std::min(a, b)), v1(std::max(a, b)) {}
    bool operator==(const EdgeKey& other) const { return v0 == other.v0 && v1 == other.v1; }
};
namespace std {
    template<> struct hash<EdgeKey> {
        size_t operator()(const EdgeKey& e) const {
            return std::hash<int>()(e.v0) ^ std::hash<int>()(e.v1 << 16);
        }
    };
}

class BallPivoting {
public:
    BallPivoting(const point_t* points, size_t num_points, double ball_radius)
        : pts(points), N(num_points), user_radius(ball_radius),
          pc_adaptor(points, num_points),
          kdtree(3, pc_adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(10))
    {
        kdtree.buildIndex();
        used.resize(N, false);

        if (user_radius < 0.0 || NEAR_ZERO(user_radius, SMALL_FASTF)) {
            double cx = 0, cy = 0, cz = 0;
            for (size_t i = 0; i < N; ++i) {
                cx += pts[i][X]; cy += pts[i][Y]; cz += pts[i][Z];
            }
            cx /= N; cy /= N; cz /= N;
            double maxr2 = 0;
            for (size_t i = 0; i < N; ++i) {
                double dx = pts[i][X] - cx, dy = pts[i][Y] - cy, dz = pts[i][Z] - cz;
                double r2 = dx*dx + dy*dy + dz*dz;
                if (r2 > maxr2) maxr2 = r2;
            }
            double bounding_radius = sqrt(maxr2);
            radius = bounding_radius * 0.05; // 5% of bounding sphere
            if (radius < bounding_radius * 1e-3)
                radius = bounding_radius * 1e-3;
        } else {
            radius = user_radius;
        }
    }

    void run(std::vector<int>& faces_out)
    {
        printf("[BallPivot] Starting mesh for %zu points, radius = %.8f\n", N, radius);
        faces.clear();
        std::fill(used.begin(), used.end(), false);

        std::tuple<int, int, int> seed;
        if (!find_seed_triangle(seed)) {
            printf("[BallPivot] ERROR: No seed triangle found.\n");
            faces_out.clear();
            return;
        }
        int i0, i1, i2;
        std::tie(i0, i1, i2) = seed;
        printf("[BallPivot] Seed triangle: %d, %d, %d\n", i0, i1, i2);

        used[i0] = used[i1] = used[i2] = true;
        faces.push_back({i0, i1, i2});

        std::vector<bg_trimesh_halfedge> boundary = {
            {i0, i1, 0},
            {i1, i2, 0},
            {i2, i0, 0}
        };
        std::unordered_map<EdgeKey, int> boundary_map;
        for (const auto& e : boundary) boundary_map[EdgeKey(e.va, e.vb)] = 1;

        int pivots = 0, failed_pivots = 0;
        while (!boundary.empty()) {
            bg_trimesh_halfedge edge = boundary.back();
            boundary.pop_back();
            EdgeKey ek_opposite(edge.vb, edge.va);
            if (boundary_map.count(ek_opposite)) {
                boundary_map.erase(ek_opposite);
                continue;
            }
            int new_vertex = find_pivot_vertex(edge.va, edge.vb);
            if (new_vertex < 0) {
                failed_pivots++;
                continue;
            }
            pivots++;
            if (used[new_vertex]) {
                printf("[BallPivot] WARNING: new_vertex %d already used!\n", new_vertex);
                continue;
            }
            faces.push_back({edge.va, edge.vb, new_vertex});
            used[new_vertex] = true;
            std::vector<std::pair<int, int>> new_edges = {
                {edge.vb, new_vertex},
                {new_vertex, edge.va}
            };
            for (auto [a, b] : new_edges) {
                EdgeKey opp(b, a);
                if (boundary_map.count(opp)) {
                    boundary_map.erase(opp);
                    auto it = std::find_if(boundary.begin(), boundary.end(),
                        [&](const bg_trimesh_halfedge& e) { return e.va == b && e.vb == a; });
                    if (it != boundary.end()) boundary.erase(it);
                } else {
                    boundary.push_back({a, b, 0});
                    boundary_map[EdgeKey(a, b)] = 1;
                }
            }
        }
        printf("[BallPivot] Done. Faces: %zu, pivots: %d, failed_pivots: %d\n", faces.size(), pivots, failed_pivots);
        faces_out.clear();
        for (const auto& face : faces)
            faces_out.insert(faces_out.end(), face.begin(), face.end());
    }

private:
    const point_t* pts;
    size_t N;
    double user_radius;
    double radius;
    PointCloudAdaptor pc_adaptor;
    KDTree kdtree;
    std::vector<bool> used;
    std::vector<std::array<int,3>> faces;

    bool find_seed_triangle(std::tuple<int,int,int>& out)
    {
        static constexpr size_t K = 20;
        int triangle_candidates = 0, circ_fail = 0, inball_fail = 0, degenerate = 0;
        for (size_t i = 0; i < N; ++i) {
            std::vector<size_t> idx(K+1);
            std::vector<fastf_t> distsq(K+1);
            nanoflann::KNNResultSet<fastf_t> resultSet(K+1);
            resultSet.init(idx.data(), distsq.data());
            kdtree.findNeighbors(resultSet, pts[i], nanoflann::SearchParameters());
            for (size_t j = 1; j < resultSet.size(); ++j) {
                for (size_t k = j+1; k < resultSet.size(); ++k) {
                    triangle_candidates++;
                    int i0 = (int)i, i1 = (int)idx[j], i2 = (int)idx[k];
                    point_t center;
                    if (!compute_circumcenter(pts[i0], pts[i1], pts[i2], center)) {
                        degenerate++;
                        continue;
                    }
                    double circ = DIST_PNT_PNT(center, pts[i0]);
                    if (fabs(circ - radius) > 1e-6) {
                        circ_fail++;
                        continue;
                    }
                    if (ball_has_other_points(center, radius, {i0, i1, i2})) {
                        inball_fail++;
                        continue;
                    }
                    printf("[BallPivot] Found seed triangle after %d candidates (degenerate=%d, circ_fail=%d, inball_fail=%d)\n",
                        triangle_candidates, degenerate, circ_fail, inball_fail);
                    out = {i0, i1, i2};
                    return true;
                }
            }
        }
        printf("[BallPivot] Failed to find seed triangle after %d candidates (degenerate=%d, circ_fail=%d, inball_fail=%d)\n",
            triangle_candidates, degenerate, circ_fail, inball_fail);
        return false;
    }

    int find_pivot_vertex(int va, int vb)
    {
        std::vector<nanoflann::ResultItem<size_t, fastf_t>> ret_matches;
        const fastf_t search_radius = radius * 2.1;
        nanoflann::RadiusResultSet<fastf_t, size_t> resultSet(search_radius*search_radius, ret_matches);
        kdtree.findNeighbors(resultSet, pts[va], nanoflann::SearchParameters());

        int best = -1;
        fastf_t best_angle = std::numeric_limits<fastf_t>::max();
        int circ_fail = 0, inball_fail = 0, degenerate = 0;
        for (const auto& item : ret_matches) {
            size_t idx = item.first;
            if ((int)idx == va || (int)idx == vb || used[idx]) continue;
            point_t center;
            if (!compute_circumcenter(pts[va], pts[vb], pts[idx], center)) {
                degenerate++;
                continue;
            }
            double circ = DIST_PNT_PNT(center, pts[va]);
            if (fabs(circ - radius) > 1e-6) {
                circ_fail++;
                continue;
            }
            if (ball_has_other_points(center, radius, {va, vb, (int)idx})) {
                inball_fail++;
                continue;
            }
            vect_t ab, ac;
            VSUB2(ab, pts[vb], pts[va]);
            VSUB2(ac, pts[idx], pts[va]);
            fastf_t angle = acos(VDOT(ab, ac) / (MAGNITUDE(ab)*MAGNITUDE(ac) + 1e-12));
            if (angle < best_angle) {
                best_angle = angle;
                best = (int)idx;
            }
        }
        if (best < 0) {
            printf("[BallPivot] find_pivot_vertex(%d,%d): failed (degenerate=%d, circ_fail=%d, inball_fail=%d)\n",
                va, vb, degenerate, circ_fail, inball_fail);
        }
        return best;
    }

    // Robust circumcenter computation for triangle a,b,c in 3D
    static bool compute_circumcenter(const point_t& a, const point_t& b, const point_t& c, point_t& center)
    {
        vect_t ab, ac, ab_cross_ac;
        VSUB2(ab, b, a);
        VSUB2(ac, c, a);
        VCROSS(ab_cross_ac, ab, ac);
        double norm2 = MAGSQ(ab_cross_ac);
        if (NEAR_ZERO(norm2, 1e-14)) return false; // Degenerate triangle

        double ab2 = MAGSQ(ab);
        double ac2 = MAGSQ(ac);

        vect_t cross1, cross2, offset;
        VCROSS(cross1, ab_cross_ac, ab);
        VSCALE(cross1, cross1, ac2);
        VCROSS(cross2, ac, ab_cross_ac);
        VSCALE(cross2, cross2, ab2);
        VADD2(offset, cross1, cross2);
        VSCALE(offset, offset, 1.0 / (2.0 * norm2));
        VADD2(center, a, offset);

        return true;
    }

    bool ball_has_other_points(const point_t& center, fastf_t r, std::initializer_list<int> ignore) const
    {
        std::vector<nanoflann::ResultItem<size_t, fastf_t>> matches;
        nanoflann::RadiusResultSet<fastf_t, size_t> resultSet(r*r, matches);
        kdtree.findNeighbors(resultSet, center, nanoflann::SearchParameters());
        for (const auto& item : matches) {
            size_t idx = item.first;
            if (std::find(ignore.begin(), ignore.end(), (int)idx) != ignore.end()) continue;
            return true;
        }
        return false;
    }
};

inline int ball_pivoting(const point_t* points, int num_points, double ball_radius, std::vector<int>& faces_out)
{
    faces_out.clear();
    BallPivoting bpa(points, num_points, ball_radius);
    bpa.run(faces_out);
    return (int)(faces_out.size() / 3);
}