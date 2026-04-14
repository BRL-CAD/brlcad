/* ========================================================================= *
 * BotSmooth.h - Jacobi-Laplace mesh smoother                                *
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
#include <set>
#include <array>
#include <cmath>
#include <algorithm>

namespace gte {

namespace smooth_detail {

using Vec3 = std::array<float,3>;

/* Directed-edge → face map */
typedef std::map<std::pair<int,int>, int> HalfedgeMap;

static HalfedgeMap build_halfedge_map(size_t numF, const int* faces) {
    HalfedgeMap m;
    for (size_t f = 0; f < numF; ++f) {
        int v0=faces[3*f],v1=faces[3*f+1],v2=faces[3*f+2];
        m[{v0,v1}]=(int)f; m[{v1,v2}]=(int)f; m[{v2,v0}]=(int)f;
    }
    return m;
}

/* Per-vertex adjacency (unordered neighbors) */
static std::vector<std::vector<int>> build_adj(size_t numV, size_t numF, const int* faces) {
    std::vector<std::vector<int>> adj(numV);
    std::vector<std::set<int>> seen(numV);
    for (size_t f = 0; f < numF; ++f) {
        for (int j=0;j<3;++j) {
            int a=faces[3*f+j], b=faces[3*f+(j+1)%3];
            if (!seen[(size_t)a].count(b)){ adj[(size_t)a].push_back(b); seen[(size_t)a].insert(b); }
            if (!seen[(size_t)b].count(a)){ adj[(size_t)b].push_back(a); seen[(size_t)b].insert(a); }
        }
    }
    return adj;
}

static std::vector<bool> find_boundary(size_t numV, size_t numF, const int* faces,
                                        const HalfedgeMap& hm) {
    std::vector<bool> bnd(numV, false);
    for (size_t f = 0; f < numF; ++f) {
        for (int j=0;j<3;++j) {
            int v0=faces[3*f+j], v1=faces[3*f+(j+1)%3];
            if (hm.find({v1,v0})==hm.end()) { bnd[(size_t)v0]=true; bnd[(size_t)v1]=true; }
        }
    }
    return bnd;
}

/* Compute area-weighted vertex normals */
static std::vector<Vec3> compute_normals(size_t numV, size_t numF,
                                          const float* V, const int* faces) {
    std::vector<Vec3> N(numV, {0.f,0.f,0.f});
    for (size_t f = 0; f < numF; ++f) {
        int v0=faces[3*f],v1=faces[3*f+1],v2=faces[3*f+2];
        float ax=V[3*v1]-V[3*v0], ay=V[3*v1+1]-V[3*v0+1], az=V[3*v1+2]-V[3*v0+2];
        float bx=V[3*v2]-V[3*v0], by=V[3*v2+1]-V[3*v0+1], bz=V[3*v2+2]-V[3*v0+2];
        float nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
        N[(size_t)v0][0]+=nx; N[(size_t)v0][1]+=ny; N[(size_t)v0][2]+=nz;
        N[(size_t)v1][0]+=nx; N[(size_t)v1][1]+=ny; N[(size_t)v1][2]+=nz;
        N[(size_t)v2][0]+=nx; N[(size_t)v2][1]+=ny; N[(size_t)v2][2]+=nz;
    }
    for (auto& n : N) {
        float len = std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if (len > 1e-12f) { n[0]/=len; n[1]/=len; n[2]/=len; }
    }
    return N;
}

/* Project vector p onto the tangent plane of vertex with normal n */
static Vec3 project_tangent(Vec3 orig, Vec3 newp, Vec3 n) {
    /* translation from orig to newp */
    Vec3 t{newp[0]-orig[0], newp[1]-orig[1], newp[2]-orig[2]};
    /* remove normal component */
    float dot = t[0]*n[0]+t[1]*n[1]+t[2]*n[2];
    /* tangential component only */
    Vec3 tang{t[0]-dot*n[0], t[1]-dot*n[1], t[2]-dot*n[2]};
    return {orig[0]+tang[0], orig[1]+tang[1], orig[2]+tang[2]};
}

/* Clamp displacement to error tolerance */
static Vec3 clamp_error(Vec3 orig, Vec3 newp, Vec3 n, float tol) {
    Vec3 t{newp[0]-orig[0], newp[1]-orig[1], newp[2]-orig[2]};
    float s = std::fabs(t[0]*n[0]+t[1]*n[1]+t[2]*n[2]);
    if (s > tol) {
        float scale = tol / s;
        t[0]*=scale; t[1]*=scale; t[2]*=scale;
    }
    return {orig[0]+t[0], orig[1]+t[1], orig[2]+t[2]};
}

} /* namespace smooth_detail */

/**
 * Smooth a triangle mesh using Jacobi-Laplace smoothing.
 *
 * @param numV       Number of input vertices
 * @param verts      Input vertices (xyz)
 * @param numF       Number of input faces
 * @param faces      Input faces (vertex indices)
 * @param component  0=Tangential, 1=Normal, 2=Both
 * @param continuity 0=C0, 1=C1, 2=C2 (C2 is a no-op matching OpenMesh JacobiLaplaceSmootherT,
 *                   whose SmootherT base class compute_new_positions() does nothing for C2)
 * @param max_lerr   Max relative local error (<=0: disabled)
 * @param max_aerr   Max absolute local error (<=0: disabled)
 * @param iterations Number of smoothing iterations
 * @param outV       Output vertices
 * @param outF       Output face indices (unchanged)
 */
inline bool gte_bot_smooth(
        size_t numV, const float* verts,
        size_t numF, const int*   faces,
        int component, int continuity,
        double max_lerr, double max_aerr,
        int iterations,
        std::vector<float>& outV,
        std::vector<int>&   outF)
{
    using namespace smooth_detail;

    if (!verts || !faces || numV == 0 || numF == 0 || iterations <= 0)
        return false;

    outV.assign(verts, verts + numV*3);
    outF.assign(faces, faces + numF*3);

    auto hm  = build_halfedge_map(numF, faces);
    auto adj = build_adj(numV, numF, faces);
    auto bnd = find_boundary(numV, numF, faces, hm);

    /* Compute initial normals from original positions */
    auto normals = compute_normals(numV, numF, verts, faces);

    /* Absolute error tolerance */
    float tol = -1.f;
    if (max_aerr > 0)
        tol = (float)max_aerr;
    else if (max_lerr > 0) {
        /* relative to bounding box diagonal */
        float bb_min[3]={1e30f,1e30f,1e30f}, bb_max[3]={-1e30f,-1e30f,-1e30f};
        for (size_t i=0;i<numV;++i) {
            for (int k=0;k<3;++k) {
                if (verts[3*i+k]<bb_min[k]) bb_min[k]=verts[3*i+k];
                if (verts[3*i+k]>bb_max[k]) bb_max[k]=verts[3*i+k];
            }
        }
        float dx=bb_max[0]-bb_min[0], dy=bb_max[1]-bb_min[1], dz=bb_max[2]-bb_min[2];
        float diag = std::sqrt(dx*dx+dy*dy+dz*dz);
        tol = (float)(max_lerr * diag);
    }

    /* Uniform weights: edge weight=1, vertex weight=1/valence */
    std::vector<float> vweight(numV, 0.f);
    for (size_t v=0; v<numV; ++v) {
        int n = (int)adj[v].size();
        vweight[v] = (n > 0) ? 1.f/(float)n : 0.f;
    }
    /* edge weight = 1 (uniform) */

    /* Smooth iterations */
    for (int it = 0; it < iterations; ++it) {
        std::vector<Vec3> new_pos(numV);

        if (continuity == 0) {
            /* C0: Jacobi uniform Laplacian, damping 0.5
             * u[v] = (sum_nbr(pos[nbr]) * w_edge) * w_vert - pos[v]
             * new_pos = pos[v] + 0.5 * u
             */
            for (size_t v=0; v<numV; ++v) {
                Vec3 pv{outV[3*v],outV[3*v+1],outV[3*v+2]};
                if (bnd[v]) { new_pos[v]=pv; continue; }
                Vec3 u{};
                for (int nb : adj[v]) {
                    u[0] += outV[3*nb];
                    u[1] += outV[3*nb+1];
                    u[2] += outV[3*nb+2];
                }
                /* u = sum * vweight - p */
                for (int i=0;i<3;++i) u[i] = u[i]*vweight[v] - pv[i];
                /* damping 0.5 */
                for (int i=0;i<3;++i) new_pos[v][i] = pv[i] + 0.5f*u[i];
            }
        } else if (continuity == 1) {
            /* C1: bi-Laplacian Jacobi with 0.25 damping.
             * Matches OpenMesh JacobiLaplaceSmootherT::compute_new_positions_C1().
             * Pass 1: compute umbrellas u[v] = p[v] - sum_nbr(w_e * p[nbr]) * w_v
             * Pass 2: uu[v] = u[v] - sum_nbr(w_e * u[nbr]) * w_v / diag
             * new_pos = p[v] - 0.25 * uu[v]
             */
            std::vector<Vec3> umbrella(numV);
            for (size_t v=0; v<numV; ++v) {
                Vec3 pv{outV[3*v],outV[3*v+1],outV[3*v+2]};
                Vec3 u{};
                for (int nb : adj[v]) {
                    u[0] -= outV[3*nb];
                    u[1] -= outV[3*nb+1];
                    u[2] -= outV[3*nb+2];
                }
                for (int i=0;i<3;++i) u[i] = u[i]*vweight[v] + pv[i];
                umbrella[v] = u;
            }
            for (size_t v=0; v<numV; ++v) {
                Vec3 pv{outV[3*v],outV[3*v+1],outV[3*v+2]};
                if (bnd[v]) { new_pos[v]=pv; continue; }
                Vec3 uu{};
                float diag = 0.f;
                for (int nb : adj[v]) {
                    for (int i=0;i<3;++i) uu[i] -= umbrella[(size_t)nb][i];
                    /* diag += (w_e * w_nbr + 1) * w_e */
                    diag += (vweight[(size_t)nb] + 1.f);
                }
                for (int i=0;i<3;++i) uu[i] = uu[i]*vweight[v];
                diag *= vweight[v];
                for (int i=0;i<3;++i) uu[i] += umbrella[v][i];
                if (std::fabs(diag) > 1e-12f)
                    for (int i=0;i<3;++i) uu[i] /= diag;
                /* damping 0.25 */
                for (int i=0;i<3;++i) new_pos[v][i] = pv[i] - 0.25f*uu[i];
            }
        } else {
            /* C2: no-op — matches OpenMesh JacobiLaplaceSmootherT behaviour where the
             * SmootherT base-class compute_new_positions() does nothing for C2. */
            for (size_t v=0; v<numV; ++v) {
                Vec3 pv{outV[3*v],outV[3*v+1],outV[3*v+2]};
                new_pos[v] = pv;
            }
        }

        /* Apply component filter */
        for (size_t v=0; v<numV; ++v) {
            if (bnd[v]) continue;
            Vec3 pv{outV[3*v],outV[3*v+1],outV[3*v+2]};
            Vec3 np = new_pos[v];
            Vec3 n  = normals[v];

            if (component == 0) {
                /* Tangential only: project onto tangent plane */
                np = project_tangent(pv, np, n);
            } else if (component == 1) {
                /* Normal only: keep only normal component of displacement */
                Vec3 t{np[0]-pv[0], np[1]-pv[1], np[2]-pv[2]};
                float dot = t[0]*n[0]+t[1]*n[1]+t[2]*n[2];
                np = {pv[0]+dot*n[0], pv[1]+dot*n[1], pv[2]+dot*n[2]};
            }
            /* component==2: Tangential_and_Normal = keep full displacement */

            /* Error clamping */
            if (tol > 0.f) {
                np = clamp_error(pv, np, n, tol);
            }

            outV[3*v]=np[0]; outV[3*v+1]=np[1]; outV[3*v+2]=np[2];
        }

        /* Recompute normals for next iteration */
        normals = compute_normals(numV, numF, outV.data(), faces);
    }

    return true;
}

} /* namespace gte */
