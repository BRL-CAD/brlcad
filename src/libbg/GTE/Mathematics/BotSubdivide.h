/* ========================================================================= *
 * BotSubdivide.h - GTE-style mesh subdivision algorithms                    *
 *                                                                           *
 * A translation of the OpenMesh logic into GeometricTools style.            *
 *                                                                           *
 * Copyright (c) 2001-2025, RWTH-Aachen University                           *
 * Department of Computer Graphics and Multimedia                            *
 * All rights reserved.                                                      *
 * www.openmesh.org                                                          *
 *                                                                           *
 * Redistribution and use in source and binary forms, with or without        *
 * modification, are permitted provided that the following conditions        *
 * are met:                                                                  *
 * 1. Redistributions of source code must retain the above copyright notice, *
 *    this list of conditions and the following disclaimer.                  *
 * 2. Redistributions in binary form must reproduce the above copyright      *
 *    notice, this list of conditions and the following disclaimer in the    *
 *    documentation and/or other materials provided with the distribution.   *
 * 3. Neither the name of the copyright holder nor the names of its          *
 *    contributors may be used to endorse or promote products derived from   *
 *    this software without specific prior written permission.               *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED *
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A           *
 * PARTICULAR PURPOSE ARE DISCLAIMED.                                        *
 * ========================================================================= */

#pragma once

#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <array>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <functional>

namespace gte {

/* ======================================================================== *
 *  Internal mesh helper for subdivision algorithms                          *
 * ======================================================================== */
namespace subd_detail {

using Vec3 = std::array<float,3>;

inline Vec3 vadd(Vec3 a, Vec3 b){ return {a[0]+b[0], a[1]+b[1], a[2]+b[2]}; }
inline Vec3 vsub(Vec3 a, Vec3 b){ return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
inline Vec3 vscl(float s, Vec3 a){ return {s*a[0], s*a[1], s*a[2]}; }

/* Hash for std::pair<int,int> used by unordered containers */
struct PairHasher {
    std::size_t operator()(const std::pair<int,int>& p) const noexcept {
        return std::hash<long long>()(((long long)(unsigned int)p.first << 32)
                                      | (unsigned int)p.second);
    }
};

/* directed-edge → face index; -1 = boundary */
typedef std::unordered_map<std::pair<int,int>, int, PairHasher> HalfedgeMap;

/* edge (min,max) → midpoint vertex index */
typedef std::unordered_map<std::pair<int,int>, int, PairHasher> EdgeMidMap;

static HalfedgeMap build_halfedge_map(size_t numF, const int* faces) {
    HalfedgeMap m;
    for (size_t f = 0; f < numF; ++f) {
        int v0=faces[3*f], v1=faces[3*f+1], v2=faces[3*f+2];
        m[{v0,v1}] = (int)f;
        m[{v1,v2}] = (int)f;
        m[{v2,v0}] = (int)f;
    }
    return m;
}

/* Return the "third" vertex of the face that contains directed edge v0→v1 */
static int opp_vertex(const HalfedgeMap& hm, const int* faces, int v0, int v1) {
    auto it = hm.find({v0,v1});
    if (it == hm.end()) return -1;
    int f = it->second;
    int a=faces[3*f], b=faces[3*f+1], c=faces[3*f+2];
    if (a==v0&&b==v1) return c;
    if (b==v0&&c==v1) return a;
    if (c==v0&&a==v1) return b;
    return -1;
}

/* Get ordered one-ring of v starting from neighbor w (CCW fan).
   Works for interior vertices (fan closes back to start). */
static std::vector<int> ordered_one_ring(const HalfedgeMap& hm,
                                          const int* faces,
                                          int v, int start_w,
                                          bool& is_bnd) {
    std::vector<int> ring;
    ring.reserve(8);
    int cur = start_w;
    is_bnd = false;
    int iter = 0;
    do {
        ring.push_back(cur);
        /* Rotate CCW: the face containing v→cur gives next_neighbor as opp_vertex */
        int next = opp_vertex(hm, faces, v, cur);
        if (next < 0) { is_bnd = true; break; }
        cur = next;
        ++iter;
        if (iter > 1000) break;
    } while (cur != start_w);
    return ring;
}

/* Detect boundary vertices */
static std::vector<bool> find_boundary_verts(size_t numV, size_t numF, const int* faces,
                                              const HalfedgeMap& hm) {
    std::vector<bool> bnd(numV, false);
    for (size_t f = 0; f < numF; ++f) {
        for (int j = 0; j < 3; ++j) {
            int v0 = faces[3*f+j], v1 = faces[3*f+(j+1)%3];
            /* edge v0-v1 is boundary if directed v1→v0 is NOT in the map */
            if (hm.find({v1,v0}) == hm.end()) {
                bnd[(size_t)v0] = true;
                bnd[(size_t)v1] = true;
            }
        }
    }
    return bnd;
}

/* For boundary vertex v, find the two boundary neighbors in order (prev, next) */
static std::pair<int,int> boundary_neighbors(size_t numV, size_t numF, const int* faces,
                                               const HalfedgeMap& hm, int v) {
    (void)numV; (void)numF; (void)faces;
    int prev_v = -1, next_v = -1;
    for (auto& kv : hm) {
        int from = kv.first.first, to = kv.first.second;
        /* boundary edge v→to: to→v is NOT in map */
        if (from == v && hm.find({to,v}) == hm.end())
            next_v = to;
        /* boundary edge from→v: v→from is NOT in map */
        if (to == v && hm.find({v,from}) == hm.end())
            prev_v = from;
    }
    return {prev_v, next_v};
}

/* === Loop subdivision weights === */
static std::pair<float,float> loop_weight(int n) {
    /* alpha(n) = (40 - (3 + 2cos(2pi/n))^2) / 64 */
    if (n <= 0) return {0.f, 0.f};
    double inv_n = 1.0 / n;
    double t     = 3.0 + 2.0*std::cos(2.0*M_PI*inv_n);
    double alpha = (40.0 - t*t) / 64.0;
    return {(float)(1.0-alpha), (float)(inv_n*alpha)};
}

/* Loop midpoint: (3*(a+b) + c + d) / 8 for interior edge; (a+b)/2 for boundary */
static Vec3 loop_midpoint(Vec3 a, Vec3 b, Vec3 c, Vec3 d, bool bnd) {
    if (bnd) {
        return {(a[0]+b[0])*0.5f, (a[1]+b[1])*0.5f, (a[2]+b[2])*0.5f};
    }
    /* 3/8*(a+b) + 1/8*(c+d) */
    Vec3 r;
    for (int i=0;i<3;++i) r[i] = (3.f*(a[i]+b[i]) + c[i] + d[i]) * (1.f/8.f);
    return r;
}

/* Sqrt3 weights: alpha_n = (4 - 2cos(2pi/n)) / 9 */
static std::pair<float,float> sqrt3_weight(int n) {
    if (n <= 0) return {1.f, 0.f};
    double alpha = (4.0 - 2.0*std::cos(2.0*M_PI/n)) / 9.0;
    return {(float)(1.0-alpha), (float)(alpha/n)};
}

/* Precomputed MB weights for K=3 and K=4 */
static std::vector<float> mb_weights_k3() { return {5.f/12, -1.f/12, -1.f/12, 3.f/4}; }
static std::vector<float> mb_weights_k4() { return {3.f/8, 0.f, -1.f/8, 0.f, 3.f/4}; }

/* ======================================================================== *
 *  1. Loop Subdivision                                                       *
 * ======================================================================== */
static bool loop_one_iteration(std::vector<float>& V, std::vector<int>& F,
                                 bool update_points) {
    int numV = (int)V.size()/3;
    int numF = (int)F.size()/3;
    if (numV == 0 || numF == 0) return false;

    auto hm  = build_halfedge_map((size_t)numF, F.data());
    auto bnd  = find_boundary_verts((size_t)numV, (size_t)numF, F.data(), hm);

    /* Edge → midpoint index */
    EdgeMidMap edge_mid;
    std::vector<float> newV = V;  /* starts as copy of original */

    /* 1. New positions for existing vertices */
    if (update_points) {
        /* Build adjacency per vertex from halfedge map */
        std::vector<std::vector<int>> nbrs(numV);
        std::vector<std::set<int>> seen(numV);
        for (auto& kv : hm) {
            int from = kv.first.first, to = kv.first.second;
            if (!seen[from].count(to)) { nbrs[from].push_back(to); seen[from].insert(to); }
        }
        for (int v = 0; v < numV; ++v) {
            if (bnd[v]) {
                auto [pv, nv] = boundary_neighbors((size_t)numV, (size_t)numF, F.data(), hm, v);
                if (pv >= 0 && nv >= 0) {
                    /* (v_prev + 6*v + v_next) / 8 */
                    for (int i=0;i<3;++i)
                        newV[3*v+i] = (V[3*pv+i] + 6.f*V[3*v+i] + V[3*nv+i]) * (1.f/8.f);
                }
            } else {
                int n = (int)nbrs[v].size();
                auto [w0, w1] = loop_weight(n);
                float sx=0,sy=0,sz=0;
                for (int nb : nbrs[v]) { sx+=V[3*nb]; sy+=V[3*nb+1]; sz+=V[3*nb+2]; }
                newV[3*v+0] = w0*V[3*v+0] + w1*sx;
                newV[3*v+1] = w0*V[3*v+1] + w1*sy;
                newV[3*v+2] = w0*V[3*v+2] + w1*sz;
            }
        }
    }

    /* 2. Compute midpoints for each edge */
    for (int f = 0; f < numF; ++f) {
        for (int j = 0; j < 3; ++j) {
            int v0 = F[3*f+j], v1 = F[3*f+(j+1)%3];
            auto key = std::make_pair(std::min(v0,v1), std::max(v0,v1));
            if (edge_mid.count(key)) continue;
            int mid_idx = (int)newV.size()/3;
            edge_mid[key] = mid_idx;
            /* find opposite vertices */
            int c = opp_vertex(hm, F.data(), v0, v1);  /* third vertex of face v0→v1→c */
            int d = opp_vertex(hm, F.data(), v1, v0);  /* third vertex of face v1→v0→d */
            bool is_bnd = (c < 0 || d < 0);
            Vec3 a{V[3*v0],V[3*v0+1],V[3*v0+2]};
            Vec3 b{V[3*v1],V[3*v1+1],V[3*v1+2]};
            Vec3 cv{}, dv{};
            if (c >= 0) cv = {V[3*c],V[3*c+1],V[3*c+2]};
            if (d >= 0) dv = {V[3*d],V[3*d+1],V[3*d+2]};
            Vec3 mp = loop_midpoint(a, b, cv, dv, is_bnd);
            newV.push_back(mp[0]); newV.push_back(mp[1]); newV.push_back(mp[2]);
        }
    }

    /* 3. Build new faces (1 → 4 split) */
    std::vector<int> newF;
    newF.reserve(numF * 4 * 3);
    for (int f = 0; f < numF; ++f) {
        int v0=F[3*f], v1=F[3*f+1], v2=F[3*f+2];
        auto m01 = edge_mid[{std::min(v0,v1), std::max(v0,v1)}];
        auto m12 = edge_mid[{std::min(v1,v2), std::max(v1,v2)}];
        auto m20 = edge_mid[{std::min(v2,v0), std::max(v2,v0)}];
        /* 4 triangles */
        int tri[4][3] = { {v0,m01,m20}, {v1,m12,m01}, {v2,m20,m12}, {m01,m12,m20} };
        for (auto& t : tri) { newF.push_back(t[0]); newF.push_back(t[1]); newF.push_back(t[2]); }
    }

    V = newV; F = newF;
    return true;
}

/* ======================================================================== *
 *  2. Sqrt3 Subdivision                                                      *
 * ======================================================================== */
static bool sqrt3_one_iteration(std::vector<float>& V, std::vector<int>& F, int gen) {
    int numV = (int)V.size()/3;
    int numF = (int)F.size()/3;
    if (numV == 0 || numF == 0) return false;

    auto hm   = build_halfedge_map((size_t)numF, F.data());
    auto bnd  = find_boundary_verts((size_t)numV, (size_t)numF, F.data(), hm);

    /* 1. Compute new positions for existing vertices (store, don't apply yet) */
    std::vector<Vec3> new_pos(numV);
    std::vector<std::vector<int>> nbrs(numV);
    {
        std::vector<std::set<int>> seen(numV);
        for (auto& kv : hm) {
            int from = kv.first.first, to = kv.first.second;
            if (!seen[from].count(to)) { nbrs[from].push_back(to); seen[from].insert(to); }
        }
    }
    for (int v = 0; v < numV; ++v) {
        Vec3 pv{V[3*v],V[3*v+1],V[3*v+2]};
        if (bnd[v]) {
            if (gen % 2 == 1) {
                /* odd generation: (4*left + 19*v + 4*right) / 27 */
                auto [pv2, nv2] = boundary_neighbors((size_t)numV, (size_t)numF, F.data(), hm, v);
                if (pv2 >= 0 && nv2 >= 0) {
                    Vec3 lv{V[3*pv2],V[3*pv2+1],V[3*pv2+2]};
                    Vec3 rv{V[3*nv2],V[3*nv2+1],V[3*nv2+2]};
                    for (int i=0;i<3;++i)
                        new_pos[v][i] = (4.f*lv[i] + 19.f*pv[i] + 4.f*rv[i]) * (1.f/27.f);
                } else {
                    new_pos[v] = pv;
                }
            } else {
                /* even generation: keep boundary vertex */
                new_pos[v] = pv;
            }
        } else {
            int n = (int)nbrs[v].size();
            auto [w0, w1] = sqrt3_weight(n);
            Vec3 sum{};
            for (int nb : nbrs[v]) {
                sum[0]+=V[3*nb]; sum[1]+=V[3*nb+1]; sum[2]+=V[3*nb+2];
            }
            for (int i=0;i<3;++i) new_pos[v][i] = w0*pv[i] + w1*sum[i];
        }
    }

    /* 2. Compute centroids using ORIGINAL positions */
    std::vector<Vec3> centroids(numF);
    for (int f = 0; f < numF; ++f) {
        int v0=F[3*f],v1=F[3*f+1],v2=F[3*f+2];
        for (int i=0;i<3;++i)
            centroids[f][i] = (V[3*v0+i]+V[3*v1+i]+V[3*v2+i]) * (1.f/3.f);
    }

    /* 3. Build new topology: split each face into 3, then flip original edges
     *    Result for interior edge (v0,v1) with faces f_ab=(v0,v1,v2) and f_ba=(v1,v0,v3):
     *      - Flip: replace (v0,v1,C_ab) and (v1,v0,C_ba) with (v0,C_ab,C_ba) and (v1,C_ba,C_ab)
     *      - Keep: (v1,v2,C_ab), (v2,v0,C_ab), (v0,v3,C_ba), (v3,v1,C_ba)
     *    For boundary edge (v0,v1) in face f_ab=(v0,v1,v2):
     *      - Keep sub-triangles: (v0,v1,C_ab), (v1,v2,C_ab), (v2,v0,C_ab)
     */
    std::vector<float> newV;
    newV.reserve((numV + numF) * 3);
    /* original vertices with new positions */
    for (int v = 0; v < numV; ++v) {
        newV.push_back(new_pos[v][0]);
        newV.push_back(new_pos[v][1]);
        newV.push_back(new_pos[v][2]);
    }
    /* centroid vertices */
    for (int f = 0; f < numF; ++f) {
        newV.push_back(centroids[f][0]);
        newV.push_back(centroids[f][1]);
        newV.push_back(centroids[f][2]);
    }
    /* centroid index for face f: numV + f */

    std::vector<int> newF;
    newF.reserve(numF * 3 * 3);

    /* Track which interior edges we've already processed */
    std::unordered_set<std::pair<int,int>, PairHasher> processed_edges;

    for (int f = 0; f < numF; ++f) {
        int v[3] = {F[3*f], F[3*f+1], F[3*f+2]};
        int C = numV + f;  /* centroid index */
        for (int j = 0; j < 3; ++j) {
            int va = v[j], vb = v[(j+1)%3];
            auto edge_key = std::make_pair(std::min(va,vb), std::max(va,vb));
            if (processed_edges.count(edge_key)) continue;
            processed_edges.insert(edge_key);

            /* Find face on the other side: face containing vb→va */
            int f2 = -1;
            auto it = hm.find({vb,va});
            if (it != hm.end()) f2 = it->second;

            if (f2 >= 0) {
                /* Interior edge: produce flip triangles */
                int C2 = numV + f2;
                /* (va, C, C2) and (vb, C2, C) */
                newF.push_back(va); newF.push_back(C);  newF.push_back(C2);
                newF.push_back(vb); newF.push_back(C2); newF.push_back(C);
            } else {
                /* Boundary edge: keep the sub-triangle (va, vb, C) */
                newF.push_back(va); newF.push_back(vb); newF.push_back(C);
            }
        }
        /* Also add the two non-edge sub-triangles from this face
         * (the ones NOT on an original edge, i.e., involving the centroid
         * but NOT the original edge that gets flipped) */
        /* For face (v0,v1,v2) with centroid C:
         * After splitting, non-edge sub-triangles are:
         * (v1,v2,C) if edge v0→v1 is interior (C side) ...
         * Actually, per face, the 3 sub-triangles are:
         * (v0,v1,C), (v1,v2,C), (v2,v0,C)
         * Of these, edges v0-v1, v1-v2, v2-v0 might be flipped.
         * The "non-flipped" sub-triangles come from the OPPOSITE face
         * and are: (vb,vc,C_of_opp_face) for each edge (va,vb) of opp face.
         * Since we process each edge once, we handle:
         * - interior edge: the flip produces 2 triangles containing centroids from both sides
         * - boundary edge: produces (va,vb,C) unchanged
         * But the sub-triangles (v1,v2,C), (v2,v0,C) for face f are the
         * "far" sub-triangles for edges v1-v2 and v2-v0. These appear as the
         * "non-C side" triangles of the adjacent faces.
         * Wait - we need to also emit (v1,v2,C) and (v2,v0,C) somehow.
         * These appear when processing edge v1-v2 or v2-v0.
         * For edge v1-v2: face f contributes (v1,v2,C)... but that gets replaced if interior.
         * Hmm. Let me reconsider.
         */
    }

    /* The above produces flip triangles but NOT the "non-edge" sub-triangles.
     * For each face, 3 sub-triangles; for each interior edge, both sub-triangles
     * from the two faces get replaced by the 2 flip triangles.
     * But for edges shared by the same face (all 3 edges), we need to:
     * - For each INTERIOR edge (va,vb): contribute (va,C,C2) and (vb,C2,C)
     * - For each BOUNDARY edge (va,vb): contribute (va,vb,C) unchanged
     * Wait - that's exactly 2 triangles per interior edge and 1 per boundary edge.
     * For a closed manifold with F faces, E edges, V vertices:
     * E = 3F/2 (each edge shared by 2 faces, each face has 3 edges)
     * Triangles from interior edges: 2 * E_int = 2 * E = 3F
     * That's correct (3F new triangles from F original faces, sqrt3 factor).
     * For a mesh with boundary:
     * E = E_int + E_bnd
     * Triangles = 2*E_int + E_bnd = 2*(3F/2 - E_bnd/2 + E_bnd/2) = ...
     * Actually for a triangle mesh: 3F = 2*E_int + E_bnd
     * So: 2*E_int + E_bnd = 3F triangles total ✓
     * Good, the above gives exactly 3F triangles.
     */

    V = newV; F = newF;
    return true;
}

/* ======================================================================== *
 *  3. Sqrt3 Interpolating (Labsik-Greiner)                                  *
 * ======================================================================== */

/* Weights for valence K >= 5 */
static std::vector<float> sqrt3interp_weights(int K) {
    std::vector<float> w(K+1);
    if (K == 3) {
        return {4.f/27, -5.f/27, 4.f/27, 8.f/9};
    }
    if (K == 4) {
        return {2.f/9, -1.f/9, -1.f/9, 2.f/9, 7.f/9};
    }
    if (K == 6) { /* special case: use regular rule for K==6 */ return {}; }
    double aH = 2.0*std::cos(M_PI/(double)K)/3.0;
    w[K] = (float)(1.0 - aH*aH);
    for (int i = 0; i < K; ++i) {
        double val = (aH*aH
                      + 2.0*aH*std::cos(2.0*(double)i*M_PI/(double)K + M_PI/(double)K)
                      + 2.0*aH*aH*std::cos(4.0*(double)i*M_PI/(double)K + 2.0*M_PI/(double)K))
                     / (double)K;
        w[i] = (float)val;
    }
    return w;
}

/* Compute centroid for regular interior face (all vertices valence 6):
   For each halfedge h of face, walk outward to get 8-point stencil. */
static Vec3 sqrt3interp_regular_centroid(const std::vector<float>& V,
                                           const HalfedgeMap& hm,
                                           const int* faces, int f) {
    /* Face (v0, v1, v2) */
    int v0=faces[3*f], v1=faces[3*f+1], v2=faces[3*f+2];
    int fv[3] = {v0, v1, v2};
    Vec3 pos{};
    /* For each halfedge h = fv[j]→fv[(j+1)%3]: */
    for (int j = 0; j < 3; ++j) {
        int from = fv[j], to = fv[(j+1)%3];
        /* one-ring vertex: weight 32/81 */
        for (int i=0;i<3;++i) pos[i] += (32.f/81.f) * V[3*to+i];
        /* tip vertex: opp_vertex of directed edge to→from */
        int tip = opp_vertex(hm, faces, to, from);
        if (tip >= 0)
            for (int i=0;i<3;++i) pos[i] -= (1.f/81.f) * V[3*tip+i];
        /* outer vertex 1: third vertex of face containing to→tip */
        if (tip >= 0) {
            int ov1 = opp_vertex(hm, faces, to, tip);
            if (ov1 >= 0)
                for (int i=0;i<3;++i) pos[i] -= (2.f/81.f) * V[3*ov1+i];
            /* outer vertex 2: third vertex of face containing to→ov1 ... actually
               continue the fan: opp(prev(h2)) where h2 is the opp(h), i.e. rotate
               around 'to' vertex */
            if (ov1 >= 0) {
                int ov2 = opp_vertex(hm, faces, to, ov1);
                if (ov2 >= 0)
                    for (int i=0;i<3;++i) pos[i] -= (2.f/81.f) * V[3*ov2+i];
            }
        }
    }
    return pos;
}

/* Compute centroid for irregular interior face */
static Vec3 sqrt3interp_irregular_centroid(const std::vector<float>& V,
                                             const HalfedgeMap& hm,
                                             const int* faces, int f,
                                             const std::vector<bool>& bnd,
                                             const std::vector<int>& valence) {
    int fv[3] = {faces[3*f], faces[3*f+1], faces[3*f+2]};

    int nOrdinary = 0;
    for (int j = 0; j < 3; ++j) {
        int val = valence[fv[j]];
        if (val == 6 || bnd[fv[j]]) ++nOrdinary;
    }

    Vec3 pos{};
    if (nOrdinary == 3) {
        pos = sqrt3interp_regular_centroid(V, hm, faces, f);
    } else {
        /* Use irregular vertex weights */
        int cnt_irregular = 0;
        for (int j = 0; j < 3; ++j) {
            int v = fv[j];
            int K = valence[v];
            if (K == 6 || bnd[v]) continue;
            /* Irregular vertex: use its weights */
            auto w = sqrt3interp_weights(K);
            if (w.empty()) continue;
            Vec3 contrib{};
            /* Get ordered one-ring of v starting from the halfedge leaving v in face f:
               h = v → fv[(j+1)%3] */
            int start_nbr = fv[(j+1)%3];
            /* Walk the fan around v starting from start_nbr */
            int cur = start_nbr;
            bool is_bv = false;
            auto ring = ordered_one_ring(hm, faces, v, cur, is_bv);
            (void)is_bv;
            /* Apply weights */
            if ((int)ring.size() >= K) {
                if ((int)w.size() > K)
                    for (int i=0;i<3;++i) contrib[i] += w[K]*V[3*v+i];
                for (int ri = 0; ri < K && ri < (int)ring.size(); ++ri)
                    for (int i=0;i<3;++i) contrib[i] += w[ri]*V[3*ring[ri]+i];
                for (int i=0;i<3;++i) pos[i] += contrib[i];
                ++cnt_irregular;
            } else {
                /* Fall back to centroid */
                for (int jj=0;jj<3;++jj) for (int i=0;i<3;++i) pos[i]+=V[3*fv[jj]+i]*(1.f/3.f);
                return pos;
            }
        }
        if (cnt_irregular > 0) {
            float inv = 1.f / (float)cnt_irregular;
            for (int i=0;i<3;++i) pos[i] *= inv;
        }
    }
    return pos;
}

static bool sqrt3interp_one_iteration(std::vector<float>& V, std::vector<int>& F, int gen) {
    int numV = (int)V.size()/3;
    int numF = (int)F.size()/3;
    if (numV == 0 || numF == 0) return false;

    auto hm  = build_halfedge_map((size_t)numF, F.data());
    auto bnd = find_boundary_verts((size_t)numV, (size_t)numF, F.data(), hm);

    /* Precompute vertex valences (outgoing halfedge count per vertex) */
    std::vector<int> valence(numV, 0);
    for (auto& kv : hm) ++valence[kv.first.first];

    /* Compute centroid positions (using original vertex positions) */
    std::vector<Vec3> centroids(numF);
    for (int f = 0; f < numF; ++f) {
        /* Check if boundary face (even gen: compute centroid; odd gen: skip) */
        bool is_bnd_face = false;
        for (int j = 0; j < 3; ++j) {
            int va=F[3*f+j], vb=F[3*f+(j+1)%3];
            if (hm.find({vb,va}) == hm.end()) { is_bnd_face=true; break; }
        }
        if (is_bnd_face && gen % 2 == 1) {
            /* odd gen boundary face: use simple centroid as fallback */
            int v0=F[3*f],v1=F[3*f+1],v2=F[3*f+2];
            for (int i=0;i<3;++i)
                centroids[f][i] = (V[3*v0+i]+V[3*v1+i]+V[3*v2+i])*(1.f/3.f);
            continue;
        }
        /* Regular case: compute using Sqrt3Interp formula */
        centroids[f] = sqrt3interp_irregular_centroid(V, hm, F.data(), f, bnd, valence);
    }

    /* Build new mesh: same topology as Sqrt3 but old vertices don't move */
    std::vector<float> newV;
    newV.reserve((numV + numF) * 3);
    /* original vertices unchanged */
    for (int v = 0; v < numV; ++v) {
        newV.push_back(V[3*v]); newV.push_back(V[3*v+1]); newV.push_back(V[3*v+2]);
    }
    /* centroid vertices */
    for (int f = 0; f < numF; ++f) {
        newV.push_back(centroids[f][0]);
        newV.push_back(centroids[f][1]);
        newV.push_back(centroids[f][2]);
    }

    std::vector<int> newF;
    newF.reserve(numF * 3 * 3);
    std::unordered_set<std::pair<int,int>, PairHasher> processed_edges;

    for (int f = 0; f < numF; ++f) {
        int v[3] = {F[3*f], F[3*f+1], F[3*f+2]};
        int C = numV + f;
        for (int j = 0; j < 3; ++j) {
            int va = v[j], vb = v[(j+1)%3];
            auto ekey = std::make_pair(std::min(va,vb), std::max(va,vb));
            if (processed_edges.count(ekey)) continue;
            processed_edges.insert(ekey);

            auto it2 = hm.find({vb,va});
            int f2 = (it2 != hm.end()) ? it2->second : -1;
            if (f2 >= 0) {
                int C2 = numV + f2;
                newF.push_back(va); newF.push_back(C);  newF.push_back(C2);
                newF.push_back(vb); newF.push_back(C2); newF.push_back(C);
            } else {
                newF.push_back(va); newF.push_back(vb); newF.push_back(C);
            }
        }
    }

    V = newV; F = newF;
    return true;
}

/* ======================================================================== *
 *  4. Modified Butterfly Subdivision                                         *
 * ======================================================================== */

/* Get ordered one-ring of vertex v starting from neighbor start_nb,
   traversing the fan using directed edge lookups.
   Returns the ring in CCW order (may be partial if boundary). */
static std::vector<int> mb_fan_ring(const HalfedgeMap& hm,
                                     const int* faces, int v, int start_nb) {
    std::vector<int> ring;
    ring.reserve(8);
    int cur = start_nb;
    int iter = 0;
    do {
        ring.push_back(cur);
        int next = opp_vertex(hm, faces, v, cur);
        if (next < 0) break;
        cur = next;
        ++iter;
    } while (cur != start_nb && iter < 1000);
    return ring;
}

static Vec3 mb_midpoint(const std::vector<float>& V,
                          const HalfedgeMap& hm,
                          const int* faces,
                          const std::vector<bool>& bnd,
                          const std::vector<int>& valence,
                          int v0, int v1) {
    /* Edge (v0, v1) */
    /* Find opposite vertices */
    int c = opp_vertex(hm, faces, v0, v1);  /* third vertex in face v0→v1 */
    int d = opp_vertex(hm, faces, v1, v0);  /* third vertex in face v1→v0 */
    bool is_bnd = (c < 0 || d < 0);

    Vec3 pa{V[3*v0],V[3*v0+1],V[3*v0+2]};
    Vec3 pb{V[3*v1],V[3*v1+1],V[3*v1+2]};

    if (is_bnd) {
        /* Boundary edge: 4-point cubic B-spline interpolation.
         * Stencil: (9*v0 + 9*v1 - v_prev - v_next) / 16
         * where v_prev is the other boundary neighbor of v0 (not v1)
         * and v_next is the other boundary neighbor of v1 (not v0).
         * Matches OpenMesh ModifiedButterflyT::compute_midpoint for boundary edges. */
        size_t numV_ = V.size() / 3;
        auto [pv0a, pv0b] = boundary_neighbors(numV_, 0, faces, hm, v0);
        auto [pv1a, pv1b] = boundary_neighbors(numV_, 0, faces, hm, v1);
        /* pick the neighbor that is NOT the other endpoint */
        int v_prev = (pv0a == v1) ? pv0b : pv0a;
        int v_next = (pv1a == v0) ? pv1b : pv1a;
        Vec3 r{};
        for (int i=0;i<3;++i) {
            r[i] = (9.f*(pa[i]+pb[i])) * (1.f/16.f);
            if (v_prev >= 0) r[i] -= V[3*v_prev+i] * (1.f/16.f);
            else             r[i] -= pa[i]           * (1.f/16.f); /* ghost: extrapolate */
            if (v_next >= 0) r[i] -= V[3*v_next+i] * (1.f/16.f);
            else             r[i] -= pb[i]           * (1.f/16.f); /* ghost: extrapolate */
        }
        return r;
    }

    Vec3 pc{V[3*c],V[3*c+1],V[3*c+2]};
    Vec3 pd{V[3*d],V[3*d+1],V[3*d+2]};

    /* Determine valence from precomputed array */
    int val_v0 = valence[v0];
    int val_v1 = valence[v1];
    bool v0_reg = (val_v0==6 || bnd[v0]);
    bool v1_reg = (val_v1==6 || bnd[v1]);

    if (v0_reg && v1_reg) {
        /* 8-point regular stencil (Zorin et al.):
         *   1/2*(v0+v1) + 1/8*(c+d) - 1/16*(b0+b1+b2+b3)
         * where c = opp vertex of face v0→v1, d = opp vertex of face v1→v0,
         * b0..b3 = next-ring vertices traversed via halfedge fan.
         * Matches OpenMesh ModifiedButterflyT compute_midpoint regular case. */
        int b0 = opp_vertex(hm, faces, v1, c);
        int b1 = opp_vertex(hm, faces, v0, d);
        /* c vertices (one more step out) */
        int c0 = (b0 >= 0) ? opp_vertex(hm, faces, v1, b0) : -1;
        int c1 = (b1 >= 0) ? opp_vertex(hm, faces, v0, b1) : -1;
        int c2 = (b1 >= 0) ? opp_vertex(hm, faces, v1, b1) : -1;
        int c3 = (b0 >= 0) ? opp_vertex(hm, faces, v0, b0) : -1;

        Vec3 r;
        for (int i=0;i<3;++i) r[i] = 0.5f*(pa[i]+pb[i]) + 0.125f*(pc[i]+pd[i]);
        /* subtract outer vertices (use ghost symmetry if missing) */
        Vec3 pc0 = (c0>=0) ? Vec3{V[3*c0],V[3*c0+1],V[3*c0+2]} : vadd(pb, vsub(pc, pa));
        Vec3 pc1 = (c1>=0) ? Vec3{V[3*c1],V[3*c1+1],V[3*c1+2]} : vadd(pb, vsub(pd, pa));
        Vec3 pc2 = (c2>=0) ? Vec3{V[3*c2],V[3*c2+1],V[3*c2+2]} : vadd(pa, vsub(pd, pb));
        Vec3 pc3 = (c3>=0) ? Vec3{V[3*c3],V[3*c3+1],V[3*c3+2]} : vadd(pa, vsub(pc, pb));
        for (int i=0;i<3;++i) r[i] -= 0.0625f*(pc0[i]+pc1[i]+pc2[i]+pc3[i]);
        return r;
    }

    /* At least one irregular endpoint */
    Vec3 r{};
    float norm_factor = 0.f;

    if (!v0_reg && !bnd[v0]) {
        /* Irregular v0: use MB weights */
        int K = val_v0;
        std::vector<float> wts;
        if (K == 3) wts = mb_weights_k3();
        else if (K == 4) wts = mb_weights_k4();
        else {
            wts.resize(K+1);
            double invK = 1.0/K;
            float sum_w = 0;
            for (int j=0;j<K;++j) {
                wts[j]=(float)((0.25+std::cos(2.0*M_PI*j*invK)+0.5*std::cos(4.0*M_PI*j*invK))*invK);
                sum_w += wts[j];
            }
            wts[K] = 1.f - sum_w;
        }
        /* Fan ring around v0 starting from v1 */
        auto ring = mb_fan_ring(hm, faces, v0, v1);
        Vec3 contrib{};
        for (int j=0;j<K && j<(int)ring.size() && j<(int)wts.size();++j)
            for (int i=0;i<3;++i) contrib[i] += wts[j]*V[3*ring[j]+i];
        if ((int)wts.size() > K)
            for (int i=0;i<3;++i) contrib[i] += wts[K]*pa[i];
        for (int i=0;i<3;++i) r[i] += contrib[i];
        norm_factor += 1.f;
    }

    if (!v1_reg && !bnd[v1]) {
        int K = val_v1;
        std::vector<float> wts;
        if (K == 3) wts = mb_weights_k3();
        else if (K == 4) wts = mb_weights_k4();
        else {
            wts.resize(K+1);
            double invK = 1.0/K;
            float sum_w = 0;
            for (int j=0;j<K;++j) {
                wts[j]=(float)((0.25+std::cos(2.0*M_PI*j*invK)+0.5*std::cos(4.0*M_PI*j*invK))*invK);
                sum_w += wts[j];
            }
            wts[K] = 1.f - sum_w;
        }
        auto ring = mb_fan_ring(hm, faces, v1, v0);
        Vec3 contrib{};
        for (int j=0;j<K && j<(int)ring.size() && j<(int)wts.size();++j)
            for (int i=0;i<3;++i) contrib[i] += wts[j]*V[3*ring[j]+i];
        if ((int)wts.size() > K)
            for (int i=0;i<3;++i) contrib[i] += wts[K]*pb[i];
        for (int i=0;i<3;++i) r[i] += contrib[i];
        norm_factor += 1.f;
    }

    if (norm_factor < 1e-6f) {
        /* Both regular: fall back to regular case */
        for (int i=0;i<3;++i) r[i] = 0.5f*(pa[i]+pb[i]);
        return r;
    }
    for (int i=0;i<3;++i) r[i] /= norm_factor;
    return r;
}

static bool mb_one_iteration(std::vector<float>& V, std::vector<int>& F) {
    int numV = (int)V.size()/3;
    int numF = (int)F.size()/3;
    if (numV == 0 || numF == 0) return false;

    auto hm  = build_halfedge_map((size_t)numF, F.data());
    auto bnd = find_boundary_verts((size_t)numV, (size_t)numF, F.data(), hm);

    /* Precompute vertex valences (outgoing halfedge count per vertex) */
    std::vector<int> valence(numV, 0);
    for (auto& kv : hm) ++valence[kv.first.first];

    /* Compute midpoints */
    EdgeMidMap edge_mid;
    std::vector<float> newV = V;  /* old vertices stay */

    for (int f = 0; f < numF; ++f) {
        for (int j = 0; j < 3; ++j) {
            int v0=F[3*f+j], v1=F[3*f+(j+1)%3];
            auto key = std::make_pair(std::min(v0,v1), std::max(v0,v1));
            if (edge_mid.count(key)) continue;
            int mid_idx = (int)newV.size()/3;
            edge_mid[key] = mid_idx;
            Vec3 mp = mb_midpoint(V, hm, F.data(), bnd, valence, v0, v1);
            newV.push_back(mp[0]); newV.push_back(mp[1]); newV.push_back(mp[2]);
        }
    }

    std::vector<int> newF;
    newF.reserve(numF * 4 * 3);
    for (int f = 0; f < numF; ++f) {
        int v0=F[3*f], v1=F[3*f+1], v2=F[3*f+2];
        auto m01 = edge_mid[{std::min(v0,v1), std::max(v0,v1)}];
        auto m12 = edge_mid[{std::min(v1,v2), std::max(v1,v2)}];
        auto m20 = edge_mid[{std::min(v2,v0), std::max(v2,v0)}];
        int tri[4][3] = { {v0,m01,m20}, {v1,m12,m01}, {v2,m20,m12}, {m01,m12,m20} };
        for (auto& t : tri) { newF.push_back(t[0]); newF.push_back(t[1]); newF.push_back(t[2]); }
    }

    V = newV; F = newF;
    return true;
}

/* ======================================================================== *
 *  5. Midpoint Subdivision                                                   *
 * ======================================================================== */
static bool midpoint_one_iteration(std::vector<float>& V, std::vector<int>& F) {
    int numV = (int)V.size()/3;
    int numF = (int)F.size()/3;
    if (numV == 0 || numF == 0) return false;

    auto hm  = build_halfedge_map((size_t)numF, F.data());
    auto bnd = find_boundary_verts((size_t)numV, (size_t)numF, F.data(), hm);

    /* Mark edge midpoints */
    EdgeMidMap edge_mid;
    std::vector<float> newV;

    for (int f = 0; f < numF; ++f) {
        for (int j = 0; j < 3; ++j) {
            int v0=F[3*f+j], v1=F[3*f+(j+1)%3];
            auto key = std::make_pair(std::min(v0,v1), std::max(v0,v1));
            if (edge_mid.count(key)) continue;
            int mid_idx = (int)newV.size()/3;
            edge_mid[key] = mid_idx;
            newV.push_back((V[3*v0]+V[3*v1])*0.5f);
            newV.push_back((V[3*v0+1]+V[3*v1+1])*0.5f);
            newV.push_back((V[3*v0+2]+V[3*v1+2])*0.5f);
        }
    }

    std::vector<int> newF;

    /* 1. For each original face: triangle from edge midpoints */
    for (int f = 0; f < numF; ++f) {
        int v0=F[3*f], v1=F[3*f+1], v2=F[3*f+2];
        int m01 = edge_mid[{std::min(v0,v1),std::max(v0,v1)}];
        int m12 = edge_mid[{std::min(v1,v2),std::max(v1,v2)}];
        int m20 = edge_mid[{std::min(v2,v0),std::max(v2,v0)}];
        newF.push_back(m01); newF.push_back(m12); newF.push_back(m20);
    }

    /* 2. For each interior original vertex: fan of edge-midpoint triangles */
    /* Build ordered fan of edges around each vertex using halfedge traversal */
    for (int v = 0; v < numV; ++v) {
        if (bnd[v]) continue;  /* skip boundary vertices */

        /* Get CCW-ordered one-ring neighbors */
        /* Find starting neighbor */
        int start_nb = -1;
        for (auto& kv : hm) {
            if (kv.first.first == v) { start_nb = kv.first.second; break; }
        }
        if (start_nb < 0) continue;

        bool is_bv = false;
        auto ring = ordered_one_ring(hm, F.data(), v, start_nb, is_bv);
        if (is_bv || ring.size() < 3) continue;

        /* Edge midpoints for edges v→ring[i] */
        std::vector<int> mids;
        mids.reserve(ring.size());
        for (int nb : ring) {
            auto key = std::make_pair(std::min(v,nb), std::max(v,nb));
            auto it  = edge_mid.find(key);
            if (it == edge_mid.end()) goto skip_vertex;
            mids.push_back(it->second);
        }
        /* ordered_one_ring returns a CCW ring; fan-triangulate directly
         * (no reversal needed — reversing would invert triangle winding
         * and create non-manifold shared edges with the face triangles) */
        /* Fan-triangulate the polygon from mids[0] */
        for (size_t i = 1; i+1 < mids.size(); ++i) {
            newF.push_back(mids[0]);
            newF.push_back(mids[i]);
            newF.push_back(mids[i+1]);
        }
        skip_vertex:;
    }

    V = newV; F = newF;
    return true;
}

} /* namespace subd_detail */

/* ======================================================================== *
 *  Public API                                                                *
 * ======================================================================== */

/**
 * Subdivide a triangle mesh.
 *
 * @param numV   Number of input vertices
 * @param verts  Input vertices (numV * 3 floats, xyz)
 * @param numF   Number of input faces
 * @param faces  Input faces   (numF * 3 ints, vertex indices)
 * @param alg    Algorithm: 1=Loop(default), 2=Sqrt3, 3=Sqrt3Interp,
 *                          4=ModifiedButterfly, 5=Midpoint
 * @param level  Number of subdivision iterations
 * @param outV   Output vertices
 * @param outF   Output face indices
 */
inline bool gte_bot_subdivide(
        size_t numV, const float* verts,
        size_t numF, const int*   faces,
        int alg, int level,
        std::vector<float>& outV,
        std::vector<int>&   outF)
{
    using namespace subd_detail;

    if (!verts || !faces || numV == 0 || numF == 0 || level <= 0)
        return false;

    /* Copy inputs */
    outV.assign(verts, verts + numV*3);
    outF.assign(faces, faces + numF*3);

    int gen = 0;
    for (int i = 0; i < level; ++i) {
        bool ok = false;
        switch (alg) {
            case 2: ok = sqrt3_one_iteration(outV, outF, gen); ++gen; break;
            case 3: ok = sqrt3interp_one_iteration(outV, outF, gen); ++gen; break;
            case 4: ok = mb_one_iteration(outV, outF); break;
            case 5: ok = midpoint_one_iteration(outV, outF); break;
            default: /* 1 and default: Loop */
                ok = loop_one_iteration(outV, outF, true); break;
        }
        if (!ok) return false;
    }
    return true;
}

} /* namespace gte */
