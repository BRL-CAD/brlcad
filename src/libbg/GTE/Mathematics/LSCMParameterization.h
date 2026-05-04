/*          L S C M P A R A M E T E R I Z A T I O N . H
 * BRL-CAD
 *
 * Published in 2026 by the United States Government.
 * This work is in the public domain.
 */

// Mesh parameterization: arc-length boundary mapping + mean-value Laplacian.
//
// Provides two distinct operations:
//
//  1. MapBoundaryToCircle (lightweight, no linear solve)
//     Maps a closed 3D boundary loop to a 2D polygon inscribed on a unit
//     circle using arc-length parameterization.  This is the primary method
//     in MeshHoleFilling (before the 3D ear-clipping last resort):
//
//       arc-length circle → EarClipping3D
//
//     A circle-inscribed polygon is always convex, so EC always succeeds
//     regardless of how non-planar the original 3D loop is.
//
//  2. Parameterize (full planar parameterization, for meshes with interior
//     vertices as well as a boundary)
//     Pins the boundary to a convex circle polygon (via arc-length), then
//     solves a mean-value Laplacian system for the interior vertices using
//     conjugate-gradient (GTE's LinearSystem::SolveSymmetricCG).
//
//     The mean-value weight for edge (v_i, v_j) from a triangle is:
//       w_ij = (tan(α_i/2) + tan(α_j/2)) / (2 * |v_j - v_i|)
//     where α_i and α_j are the interior angles at v_i and v_j respectively.
//     These weights are ALWAYS POSITIVE for non-degenerate triangles, so the
//     system matrix is symmetric positive-definite and Tutte's theorem
//     guarantees an injective (fold-over-free) map for any convex boundary.
//     This is the key advantage over cotangent weights: cotangents go
//     negative for obtuse angles, which can cause fold-overs in the 2D map.
//
//     For a boundary-only polygon (no interior vertices) Parameterize()
//     reduces to MapBoundaryToCircle() and no linear system is built.
//
// Reference for arc-length boundary mapping:
//   Lévy, Petitjean, Ray, Maillot, "Least Squares Conformal Maps for
//   Automatic Texture Atlas Generation", SIGGRAPH 2002.
// Reference for mean-value coordinates:
//   Floater, "Mean Value Coordinates", CAGD 20(1), 2003.
// Reference for Tutte's embedding theorem:
//   Tutte, "How to draw a graph", Proc. London Math. Soc. 13, 1963.

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

        // ── Full planar parameterization (mean-value Laplacian) ───────────────
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
        //        vertices are solved for by the mean-value Laplacian (Floater 2003).
        //        The mean-value weights are always positive, so by Tutte's theorem
        //        the resulting map is injective (no fold-overs) for a convex boundary.
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

            // Step 2: build mean-value Laplacian system for interior vertices.
            //
            // For each interior vertex v, the discrete equation is:
            //   sum_{u in N(v)} w_vu * (uv[u] - uv[v]) = 0
            //
            // We use the symmetric mean-value weight for edge (v_i, v_j):
            //   w_ij = (tan(α_i/2) + tan(α_j/2)) / (2 * |v_j - v_i|)
            // where α_i and α_j are the interior angles at v_i and v_j in the
            // triangle.  Using the identity tan(θ/2) = sin(θ)/(1+cos(θ)) the
            // weight is computed without trigonometric functions:
            //   tan(α_k/2) = |cross(e1,e2)| / (|e1|*|e2| + dot(e1,e2))
            // These weights are ALWAYS POSITIVE for non-degenerate triangles
            // (angles in (0,π)), making the system symmetric positive-definite.
            // Combined with the convex (unit-circle) boundary, Tutte's theorem
            // then guarantees an injective (fold-over-free) parameterization.
            //
            // Rearranged for the linear system (only interior vertices are unknowns):
            //   (sum_u w_vu) * uv[v] - sum_{u interior} w_vu * uv[u]
            //       = sum_{u boundary} w_vu * uv[u]
            // => A * x = b, where A is symmetric positive-definite.
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

            // Accumulate mean-value weights from each triangle.
            for (auto const& tri : triangles)
            {
                // Pre-compute edge lengths and tan(half-angle) at each vertex.
                // edge_len[k] = length of edge from tri[k] to tri[(k+1)%3].
                static constexpr Real kMVThreshold = static_cast<Real>(1e-10);
                std::array<Real, 3> edge_len{};
                for (int k = 0; k < 3; ++k)
                {
                    edge_len[k] = Length(
                        vertices3D[static_cast<size_t>(tri[(k + 1) % 3])] -
                        vertices3D[static_cast<size_t>(tri[k])]);
                }

                // tan(α_k/2) at vertex tri[k]:
                //   e1 = edge from tri[k] to tri[(k+1)%3]  (len = edge_len[k])
                //   e2 = edge from tri[k] to tri[(k+2)%3]  (len = edge_len[(k+2)%3])
                //   tan(α_k/2) = |cross(e1,e2)| / (len(e1)*len(e2) + dot(e1,e2))
                std::array<Real, 3> tan_half{};
                for (int k = 0; k < 3; ++k)
                {
                    Vector3<Real> e1 =
                        vertices3D[static_cast<size_t>(tri[(k + 1) % 3])] -
                        vertices3D[static_cast<size_t>(tri[k])];
                    Vector3<Real> e2 =
                        vertices3D[static_cast<size_t>(tri[(k + 2) % 3])] -
                        vertices3D[static_cast<size_t>(tri[k])];
                    Real cross_mag = Length(Cross(e1, e2));
                    Real dot_val   = Dot(e1, e2);
                    Real len_prod  = edge_len[k] * edge_len[(k + 2) % 3];
                    Real denom     = len_prod + dot_val;
                    if (denom <= kMVThreshold)
                    {
                        // Angle at tri[k] approaching π: cap to a large value.
                        tan_half[k] = static_cast<Real>(1) / kMVThreshold;
                    }
                    else if (cross_mag <= kMVThreshold)
                    {
                        // Degenerate (zero-area) triangle: skip this vertex.
                        tan_half[k] = static_cast<Real>(0);
                    }
                    else
                    {
                        tan_half[k] = cross_mag / denom;
                    }
                }

                // For each edge k (from tri[k] to tri[(k+1)%3]), accumulate the
                // symmetric mean-value weight:
                //   mvWeight = (tan_half[k] + tan_half[(k+1)%3]) / (2 * edge_len[k])
                // This is always ≥ 0 and is symmetric: mvWeight(vi→vj) = mvWeight(vj→vi).
                for (int k = 0; k < 3; ++k)
                {
                    int32_t vi = tri[k];
                    int32_t vj = tri[(k + 1) % 3];

                    if (edge_len[k] <= kMVThreshold) { continue; }
                    Real mvWeight = (tan_half[k] + tan_half[(k + 1) % 3]) /
                                    (static_cast<Real>(2) * edge_len[k]);
                    if (mvWeight <= static_cast<Real>(0)) { continue; }

                    // Accumulate into the system.
                    // Both vi and vj may be interior or boundary vertices.
                    auto iti = vertToUnknown.find(vi);
                    auto itj = vertToUnknown.find(vj);

                    bool viInt = (iti != vertToUnknown.end());
                    bool vjInt = (itj != vertToUnknown.end());

                    if (viInt)
                    {
                        int32_t ri = iti->second;
                        // Diagonal: A[ri,ri] += mvWeight
                        A[{ ri, ri }] += mvWeight;
                        if (vjInt)
                        {
                            int32_t rj = itj->second;
                            // Off-diagonal: A[min,max] -= mvWeight  (symmetric)
                            int32_t lo = std::min(ri, rj);
                            int32_t hi = std::max(ri, rj);
                            A[{ lo, hi }] -= mvWeight;
                        }
                        else
                        {
                            // vj is boundary → contributes to RHS
                            Vector2<Real> const& uvj = uv[static_cast<size_t>(vj)];
                            bu[static_cast<size_t>(ri)] += mvWeight * uvj[0];
                            bv[static_cast<size_t>(ri)] += mvWeight * uvj[1];
                        }
                    }

                    if (vjInt)
                    {
                        int32_t rj = itj->second;
                        A[{ rj, rj }] += mvWeight;
                        if (!viInt)
                        {
                            // vi is boundary → contributes to RHS
                            Vector2<Real> const& uvi = uv[static_cast<size_t>(vi)];
                            bu[static_cast<size_t>(rj)] += mvWeight * uvi[0];
                            bv[static_cast<size_t>(rj)] += mvWeight * uvi[1];
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
            // maxIter = 20 * numInterior: the mean-value Laplacian system is
            // symmetric positive-definite so CG converges in at most numInterior
            // steps in exact arithmetic.  A factor of 20 provides headroom for
            // finite precision while keeping the worst case manageable.
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
