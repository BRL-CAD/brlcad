// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2026
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 8.0.2026.03.05
//
// LSCM (Least Squares Conformal Maps) parameterization
//
// Provides two distinct operations:
//
//  1. MapBoundaryToCircle (lightweight, no linear solve)
//     Maps a closed 3D boundary loop to a 2D polygon inscribed on a unit
//     circle using arc-length parameterization.  This is the primary method
//     in MeshHoleFilling (before the 3D ear-clipping last resort):
//
//       LSCM (arc-length circle) → EarClipping3D
//
//     A circle-inscribed polygon is always convex, so EC always succeeds
//     regardless of how non-planar the original 3D loop is.
//
//  2. Parameterize (full conformal parameterization, for meshes with
//     interior vertices as well as a boundary)
//     Pins the boundary to a convex circle polygon (via arc-length), then
//     solves a cotangent-Laplacian system for the interior vertices using
//     conjugate-gradient (GTE's LinearSystem::SolveSymmetricCG).  The
//     cotangent Laplacian minimises the Dirichlet energy, which is a close
//     approximation of the LSCM conformal energy.  The result can be used
//     as UV coordinates for texture mapping or as a 2D domain for mesh
//     operations on complex surface patches.
//
//     For a boundary-only polygon (no interior vertices) Parameterize()
//     reduces to MapBoundaryToCircle() and no linear system is built.
//
// Reference for LSCM:
//   Lévy, Petitjean, Ray, Maillot, "Least Squares Conformal Maps for
//   Automatic Texture Atlas Generation", SIGGRAPH 2002.
// Reference for cotangent Laplacian:
//   Pinkall & Polthier, "Computing Discrete Minimal Surfaces and Their
//   Conjugates", Experimental Mathematics 2(1), 1993.

#pragma once

#include <Mathematics/Vector2.h>
#include <Mathematics/Vector3.h>
#include <Mathematics/LinearSystem.h>
#include <Mathematics/Constants.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

namespace gte
{
    template <typename Real>
    class LSCMParameterization
    {
    public:
        // ── Arc-length boundary mapping ───────────────────────────────────────
        //
        // Map a 3D boundary loop to 2D UV positions on a unit circle using
        // arc-length parameterization.
        //
        // Input:
        //   vertices3D      – 3D positions of all mesh vertices
        //   boundaryIndices – ordered vertex indices forming the closed loop
        //
        // Output:
        //   uv – UV coordinates for each boundary vertex, parallel to
        //        boundaryIndices.  The polygon is inscribed on the unit circle
        //        so it is always simple (non-self-intersecting).
        //
        // Returns true on success.
        static bool MapBoundaryToCircle(
            std::vector<Vector3<Real>> const& vertices3D,
            std::vector<int32_t> const& boundaryIndices,
            std::vector<Vector2<Real>>& uv)
        {
            int32_t n = static_cast<int32_t>(boundaryIndices.size());
            if (n < 3)
            {
                return false;
            }

            // Compute cumulative arc lengths along the boundary.
            std::vector<Real> arcLen(n, static_cast<Real>(0));
            Real totalLen = static_cast<Real>(0);
            for (int32_t i = 0; i < n; ++i)
            {
                int32_t i0 = boundaryIndices[i];
                int32_t i1 = boundaryIndices[(i + 1) % n];
                Real edgeLen = Length(vertices3D[i1] - vertices3D[i0]);
                totalLen += edgeLen;
                arcLen[i] = totalLen;  // cumulative length *after* edge i
            }

            if (totalLen <= static_cast<Real>(0))
            {
                return false;
            }

            // Map each boundary vertex to the unit circle.  Vertex 0 is placed
            // at angle 0; subsequent vertices follow in arc-length order.
            Real const twoPi = static_cast<Real>(2) * static_cast<Real>(GTE_C_PI);
            uv.resize(n);
            uv[0] = Vector2<Real>{ static_cast<Real>(1), static_cast<Real>(0) };
            for (int32_t i = 1; i < n; ++i)
            {
                Real fraction = arcLen[i - 1] / totalLen;
                Real angle = twoPi * fraction;
                uv[i] = Vector2<Real>{ std::cos(angle), std::sin(angle) };
            }
            return true;
        }

        // ── Full conformal parameterization ──────────────────────────────────
        //
        // Parameterize a 3D mesh patch to 2D UV space.
        //
        // Input:
        //   vertices3D      – 3D positions of all mesh vertices
        //   boundaryIndices – ordered vertex indices forming the outer boundary
        //   interiorIndices – vertex indices of interior (free) vertices; may be
        //                     empty for boundary-only polygons
        //   triangles       – triangle connectivity for the entire patch;
        //                     required when interiorIndices is non-empty
        //
        // Output:
        //   uv – UV coordinates indexed by vertex index (same size as vertices3D);
        //        boundary vertices are pinned to the unit circle, interior
        //        vertices are solved for by minimising the Dirichlet energy
        //        (cotangent Laplacian, a close approximation of LSCM conformal
        //        energy)
        //
        // Returns true on success.
        static bool Parameterize(
            std::vector<Vector3<Real>> const& vertices3D,
            std::vector<int32_t> const& boundaryIndices,
            std::vector<int32_t> const& interiorIndices,
            std::vector<std::array<int32_t, 3>> const& triangles,
            std::vector<Vector2<Real>>& uv)
        {
            int32_t numVertices = static_cast<int32_t>(vertices3D.size());
            if (numVertices < 3 || static_cast<int32_t>(boundaryIndices.size()) < 3)
            {
                return false;
            }

            // Step 1: initialise all UV to zero, then pin boundary.
            uv.assign(static_cast<size_t>(numVertices),
                      Vector2<Real>{ static_cast<Real>(0), static_cast<Real>(0) });

            // Build a temporary boundary-UV array (MapBoundaryToCircle output is
            // indexed by position in boundaryIndices, not by vertex index).
            std::vector<Vector2<Real>> boundaryUV;
            if (!MapBoundaryToCircle(vertices3D, boundaryIndices, boundaryUV))
            {
                return false;
            }
            for (int32_t i = 0; i < static_cast<int32_t>(boundaryIndices.size()); ++i)
            {
                uv[static_cast<size_t>(boundaryIndices[i])] = boundaryUV[static_cast<size_t>(i)];
            }

            // If no interior vertices, the boundary pinning is the complete result.
            if (interiorIndices.empty() || triangles.empty())
            {
                return true;
            }

            // Step 2: build cotangent-Laplacian system for interior vertices.
            //
            // For each interior vertex v, the equation is:
            //   sum_{u in N(v)} w_vu * (uv[u] - uv[v]) = 0
            // where w_vu = (cot(alpha_vu) + cot(beta_vu)) / 2, with alpha_vu and
            // beta_vu the angles of triangles opposite to edge (v,u).
            //
            // Rearranged for the linear system (only interior vertices are unknowns):
            //   (sum_u w_vu) * uv[v] - sum_{u interior} w_vu * uv[u]
            //       = sum_{u boundary} w_vu * uv[u]
            // => A * x = b, where A is symmetric positive (semi-)definite.
            int32_t numInterior = static_cast<int32_t>(interiorIndices.size());

            // Map from global vertex index to interior-unknown index.
            std::map<int32_t, int32_t> vertToUnknown;
            for (int32_t i = 0; i < numInterior; ++i)
            {
                vertToUnknown[interiorIndices[i]] = i;
            }

            // Mark boundary vertices for quick lookup.
            std::map<int32_t, int32_t> vertToBoundary;
            for (int32_t i = 0; i < static_cast<int32_t>(boundaryIndices.size()); ++i)
            {
                vertToBoundary[boundaryIndices[i]] = i;
            }

            // We solve u and v coordinates independently with the same Laplacian.
            typename LinearSystem<Real>::SparseMatrix A;  // symmetric, upper triangle
            std::vector<Real> bu(static_cast<size_t>(numInterior), static_cast<Real>(0));
            std::vector<Real> bv(static_cast<size_t>(numInterior), static_cast<Real>(0));

            // Accumulate cotangent weights from each triangle.
            for (auto const& tri : triangles)
            {
                // For each of the 3 edges (i,j) of the triangle, compute the
                // cotangent of the angle at the opposite corner k.
                for (int corner = 0; corner < 3; ++corner)
                {
                    int32_t vi = tri[corner];
                    int32_t vj = tri[(corner + 1) % 3];
                    int32_t vk = tri[(corner + 2) % 3];  // opposite to edge (vi,vj)

                    Vector3<Real> ei = vertices3D[static_cast<size_t>(vi)] -
                                       vertices3D[static_cast<size_t>(vk)];
                    Vector3<Real> ej = vertices3D[static_cast<size_t>(vj)] -
                                       vertices3D[static_cast<size_t>(vk)];

                    Real cosA = Dot(ei, ej);
                    Real sinA = Length(Cross(ei, ej));
                    // Skip degenerate triangles (cross product magnitude near zero).
                    // The threshold is relative to the magnitude of sinA; for Real=float
                    // a slightly larger value may be needed, but 1e-10 is safe for double.
                    static constexpr Real kDegenerateThreshold = static_cast<Real>(1e-10);
                    if (sinA <= kDegenerateThreshold) { continue; }
                    Real cotA = cosA / sinA;  // cot(angle at vk)

                    // Each triangle contributes cotA for the opposite edge (vi,vj).
                    // The symmetric cotangent Laplacian weight for edge (vi,vj) is
                    //   w_ij = cotA_left + cotA_right
                    // where "left" and "right" are the two triangles sharing (vi,vj).
                    // We accumulate the full cotA from each triangle as it is visited,
                    // so no halving is needed here; the total across both visits gives
                    // w_ij = cotA_left + cotA_right as required.
                    Real cotWeight = cotA;

                    // Accumulate into the system.
                    // Both vi and vj may be interior or boundary vertices.
                    auto iti = vertToUnknown.find(vi);
                    auto itj = vertToUnknown.find(vj);

                    bool viInt = (iti != vertToUnknown.end());
                    bool vjInt = (itj != vertToUnknown.end());

                    if (viInt)
                    {
                        int32_t ri = iti->second;
                        // Diagonal: A[ri,ri] += cotWeight
                        A[{ ri, ri }] += cotWeight;
                        if (vjInt)
                        {
                            int32_t rj = itj->second;
                            // Off-diagonal: A[min,max] -= cotWeight  (symmetric)
                            int32_t lo = std::min(ri, rj);
                            int32_t hi = std::max(ri, rj);
                            A[{ lo, hi }] -= cotWeight;
                        }
                        else
                        {
                            // vj is boundary → contributes to RHS
                            Vector2<Real> const& uvj = uv[static_cast<size_t>(vj)];
                            bu[static_cast<size_t>(ri)] += cotWeight * uvj[0];
                            bv[static_cast<size_t>(ri)] += cotWeight * uvj[1];
                        }
                    }

                    if (vjInt)
                    {
                        int32_t rj = itj->second;
                        A[{ rj, rj }] += cotWeight;
                        if (!viInt)
                        {
                            // vi is boundary → contributes to RHS
                            Vector2<Real> const& uvi = uv[static_cast<size_t>(vi)];
                            bu[static_cast<size_t>(rj)] += cotWeight * uvi[0];
                            bv[static_cast<size_t>(rj)] += cotWeight * uvi[1];
                        }
                        // The off-diagonal case (vi interior) was already handled above.
                    }
                }
            }

            // Step 3: solve A * x_u = b_u  and  A * x_v = b_v.
            // Tolerance: 1e-8 for double; for float-precision Real, 1e-6 would be
            // more appropriate.  Using 1e-8 as a conservative default suitable for
            // both since float accuracy is limited to ~7 decimal digits.
            //
            // maxIter = 20 * numInterior: the cotangent-Laplacian system is
            // positive-definite so CG converges in at most numInterior steps in
            // exact arithmetic.  A factor of 20 provides headroom for finite
            // precision while keeping the worst case manageable.  In practice
            // the mesh-repair call site uses boundary-only holes (numInterior == 0),
            // so this solver path is only exercised by the full Parameterize() API.
            static constexpr uint32_t kCGIterMultiplier = 20u;
            uint32_t maxIter = static_cast<uint32_t>(numInterior) * kCGIterMultiplier;
            Real const cgTolerance = static_cast<Real>(1e-8);

            std::vector<Real> xu(static_cast<size_t>(numInterior), static_cast<Real>(0));
            std::vector<Real> xv(static_cast<size_t>(numInterior), static_cast<Real>(0));

            LinearSystem<Real>::SolveSymmetricCG(
                numInterior, A, bu.data(), xu.data(), maxIter, cgTolerance);
            LinearSystem<Real>::SolveSymmetricCG(
                numInterior, A, bv.data(), xv.data(), maxIter, cgTolerance);

            // Step 4: store results for interior vertices.
            for (int32_t i = 0; i < numInterior; ++i)
            {
                uv[static_cast<size_t>(interiorIndices[i])] =
                    Vector2<Real>{ xu[static_cast<size_t>(i)],
                                   xv[static_cast<size_t>(i)] };
            }

            return true;
        }
    };
}
