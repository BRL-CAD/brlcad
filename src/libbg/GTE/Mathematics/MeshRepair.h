// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2026
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 8.0.2026.02.26
//
// Ported from Geogram: https://github.com/BrunoLevy/geogram
// Original files: src/lib/geogram/mesh/mesh_repair.cpp, mesh_repair.h
// Original license: BSD 3-Clause (see below)
// Copyright (c) 2000-2022 Inria
//
// Adaptations for Geometric Tools Engine:
// - Changed from GEO::Mesh to std::vector<Vector3<Real>> and std::vector<std::array<int32_t, 3>>
// - Uses GTE::ETManifoldMesh for topology operations
// - Removed Geogram-specific Logger and CmdLine calls
// - Added GTE-style template parameter handling
//
// Original Geogram License (BSD 3-Clause):
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the ALICE Project-Team nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <Mathematics/Vector3.h>
#include <Mathematics/ETManifoldMesh.h>
#include <Mathematics/MeshHoleFilling.h>
#include <Mathematics/MeshPreprocessing.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

// The MeshRepair class provides mesh topology repair operations ported from
// Geogram. It operates on triangle meshes represented as vertex and triangle
// arrays, repairing common defects such as:
// - Colocated (duplicate) vertices
// - Degenerate triangles (triangles with repeated vertices)
// - Duplicate triangles
// - Isolated vertices (not referenced by any triangle)
// - Non-manifold topology
//
// The implementation uses GTE's ETManifoldMesh for topology operations and
// is header-only, following GTE conventions.

namespace gte
{
    template <typename Real>
    class MeshRepair
    {
    public:
        // Repair mode flags. These can be combined using bitwise OR (|).
        enum class RepairMode : uint32_t
        {
            TOPOLOGY = 0,       // Dissociate non-manifold vertices (always done)
            COLOCATE = 1,       // Merge vertices at same location
            DUP_F = 2,          // Remove duplicate facets
            TRIANGULATE = 4,    // Ensure output is triangulated
            RECONSTRUCT = 8,    // Post-process (remove small components, fill holes)
            QUIET = 16,         // Suppress messages (unused in GTE port)
            DEFAULT = COLOCATE | DUP_F | TRIANGULATE
        };

        friend RepairMode operator|(RepairMode a, RepairMode b)
        {
            return static_cast<RepairMode>(
                static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
        }

        // Parameters for mesh repair operations.
        struct Parameters
        {
            Real epsilon;               // Tolerance for vertex merging
            RepairMode mode;            // Combination of repair mode flags
            bool verbose;               // Enable verbose output (unused in header-only impl)

            Parameters()
                : epsilon(static_cast<Real>(0))
                , mode(RepairMode::DEFAULT)
                , verbose(false)
            {
            }
        };

        // Main repair function. Modifies the input mesh in-place.
        // vertices: vertex positions (3 coordinates per vertex)
        // triangles: triangle indices (3 vertex indices per triangle)
        // params: repair parameters
        //
        // ── Comparison with Geogram mesh_repair() ────────────────────────────
        //
        // Geogram's mesh_repair() sequence (mesh_repair.cpp, commit f5abd69):
        //
        //  G1.  mesh_colocate_vertices_no_check   if COLOCATE
        //  G2.  M.facets.triangulate()             if TRIANGULATE
        //  G3.  mesh_remove_bad_facets_no_check    always (degenerate + dup_f)
        //  G4.  repair_connect_facets(M)           ALWAYS — build face adjacency
        //  G5.  repair_reorient_facets_anti_moebius(M) ALWAYS — consistent winding
        //  G6.  repair_split_non_manifold_vertices(M)  ALWAYS — topological split
        //  G7.  if RECONSTRUCT:
        //         remove_small_connected_components  (< min_comp_area / min_comp_facets)
        //         fill_holes                          (< max_hole_area / max_hole_edges)
        //         remove_small_connected_components   (again)
        //         repair_connect_facets               (again)
        //         repair_reorient_facets_anti_moebius (again)
        //         repair_split_non_manifold_vertices  (again)
        //  G8.  orient_normals(M)                  if dimension >= 3
        //
        // GTE Repair() sequence and mapping:
        //
        //  Step 1 = G1: ColocateVertices               if COLOCATE ✓
        //  Step 2 = G2: (no-op, GTE is always triangulated) ✓
        //  Step 3 = G3: RemoveBadFacets                always ✓
        //  Step 4 = G4–G6: ** intentionally omitted ** (see note below)
        //  Step 5a= G7 (first remove_small):           if RECONSTRUCT ✓
        //  Step 5b= G7 (fill_holes):                   if RECONSTRUCT ✓
        //  Step 5c= G7 (second remove_small):          if RECONSTRUCT ✓
        //  Step 5d= G7 (second connect+reorient+split): ** omitted ** (see note)
        //  Step 7 = G8: OrientNormals                  always ✓
        //
        //  Isolated vertex removal (RemoveIsolatedVertices, Step 4) is a GTE
        //  addition not present in Geogram's mesh_repair(); Geogram removes them
        //  indirectly through mesh element deletion routines.
        //
        // ** Note on G4–G6 (connect + reorient + split non-manifold) **
        //
        //  GTE deliberately does NOT call ConnectFacets / ReorientFacetsAntiMoebius
        //  / SplitNonManifoldVertices unconditionally.  These functions are
        //  implemented as public static methods (see below in this file) for
        //  callers that need them explicitly.
        //
        //  Reason: for GTE's primary use case (repairing BRL-CAD solid geometry
        //  meshes with intentional multi-body topology), calling
        //  SplitNonManifoldVertices unconditionally causes boundary edges shared
        //  between two bodies to be duplicated into extra open loops.  The
        //  component-removal + hole-filling sequence (G7) already produces the
        //  correct manifold outcome without disrupting legitimate shared topology.
        //
        //  Callers that need Geogram-equivalent unconditional topology repair
        //  can call these directly in sequence:
        //    std::vector<int32_t> adj;
        //    MeshRepair<Real>::ConnectFacets(triangles, adj);
        //    MeshRepair<Real>::ReorientFacetsAntiMoebius(vertices, triangles, adj);
        //    MeshRepair<Real>::SplitNonManifoldVertices(vertices, triangles, adj);
        // ─────────────────────────────────────────────────────────────────────
        static void Repair(
            std::vector<Vector3<Real>>& vertices,
            std::vector<std::array<int32_t, 3>>& triangles,
            Parameters const& params)
        {
            uint32_t mode = static_cast<uint32_t>(params.mode);

            // Step 1: Colocate vertices if requested (Geogram G1).
            if (mode & static_cast<uint32_t>(RepairMode::COLOCATE))
            {
                ColocateVertices(vertices, triangles, params.epsilon);
            }

            // Step 2: Triangulate if needed — no-op; GTE always works with triangles.

            // Step 3: Remove degenerate and duplicate facets (Geogram G3).
            bool checkDuplicates = (mode & static_cast<uint32_t>(RepairMode::DUP_F)) != 0;
            RemoveBadFacets(vertices, triangles, checkDuplicates);

            // Step 4: Remove isolated vertices (GTE addition; not in Geogram Repair()).
            RemoveIsolatedVertices(vertices, triangles);

            // Step 5: RECONSTRUCT post-processing (Geogram G7).
            // Matches Geogram's sequence:
            //   remove_small_connected_components (< 0.01% area, < 10 facets)
            //   fill_holes                        (< 5% area, < 500 edges)
            //   remove_small_connected_components (again)
            // The second connect+reorient+split within Geogram's RECONSTRUCT
            // block is omitted for the same reason as G4-G6 (see note above).
            if ((mode & static_cast<uint32_t>(RepairMode::RECONSTRUCT)) != 0
                && !triangles.empty())
            {
                auto computeTotalArea = [&]() -> Real
                {
                    Real total = static_cast<Real>(0);
                    for (auto const& tri : triangles)
                    {
                        Vector3<Real> const& a = vertices[tri[0]];
                        Vector3<Real> const& b = vertices[tri[1]];
                        Vector3<Real> const& c = vertices[tri[2]];
                        Vector3<Real> ab = b - a, ac = c - a;
                        Real area = static_cast<Real>(0.5) *
                            std::sqrt(Dot(Cross(ab, ac), Cross(ab, ac)));
                        total += area;
                    }
                    return total;
                };

                // 5a: remove small components (Geogram: first remove_small_connected_components)
                {
                    Real Marea = computeTotalArea();
                    MeshPreprocessing<Real>::RemoveSmallComponents(
                        vertices, triangles,
                        Marea * static_cast<Real>(0.0001),  // 0.01% of total area
                        10);                                 // < 10 facets
                }

                // 5b: fill small holes (Geogram: fill_holes)
                {
                    Real Marea = computeTotalArea();
                    typename MeshHoleFilling<Real>::Parameters hp;
                    hp.maxArea  = Marea * static_cast<Real>(0.05);  // 5% of total area
                    hp.maxEdges = 500;
                    MeshHoleFilling<Real>::FillHoles(vertices, triangles, hp);
                }

                // 5c: remove small components again (Geogram: second remove_small_connected_components)
                {
                    Real Marea = computeTotalArea();
                    MeshPreprocessing<Real>::RemoveSmallComponents(
                        vertices, triangles,
                        Marea * static_cast<Real>(0.0001),
                        10);
                }
            }

            // Step 7: Orient normals consistently per connected component (Geogram G8).
            if (!triangles.empty())
            {
                MeshPreprocessing<Real>::OrientNormals(vertices, triangles);
            }
        }

        // Detect colocated vertices and return mapping to canonical vertex indices.
        // For each vertex i, colocated[i] contains either i (if unique) or
        // the index of the canonical vertex that i should be merged with.
        static void DetectColocatedVertices(
            std::vector<Vector3<Real>> const& vertices,
            std::vector<int32_t>& colocated,
            Real epsilon)
        {
            int32_t numVertices = static_cast<int32_t>(vertices.size());
            colocated.resize(numVertices);

            // Initially, each vertex is its own canonical vertex
            for (int32_t i = 0; i < numVertices; ++i)
            {
                colocated[i] = i;
            }

            if (epsilon <= static_cast<Real>(0))
            {
                // Exact comparison for zero epsilon
                std::map<Vector3<Real>, int32_t> uniqueVertices;
                for (int32_t i = 0; i < numVertices; ++i)
                {
                    auto iter = uniqueVertices.find(vertices[i]);
                    if (iter != uniqueVertices.end())
                    {
                        // Found duplicate
                        colocated[i] = iter->second;
                    }
                    else
                    {
                        // New unique vertex
                        uniqueVertices[vertices[i]] = i;
                    }
                }
            }
            else
            {
                // Epsilon-based colocate using sort-by-x + union-find.
                //
                // ── Algorithm ────────────────────────────────────────────────────
                //
                //  1. Sort vertex indices by x-coordinate.                O(N log N)
                //  2. For each vertex i (sorted order), scan forward while
                //     dx ≤ epsilon.  For each candidate j, check the full
                //     3D Euclidean distance; if ≤ epsilon, union i and j.  O(N·W)
                //     W = max number of vertices in any epsilon-wide x-slab.
                //  3. Each union-find root becomes the canonical vertex.   O(N·α(N))
                //
                // ── Correctness ───────────────────────────────────────────────────
                //
                // Any two vertices within distance epsilon must have
                // |x_i - x_j| ≤ epsilon, so they always appear within epsilon of
                // each other in the sorted order and are never skipped.
                //
                // Union-find guarantees FULL TRANSITIVITY: if A–B and B–C are
                // merged they all share one canonical vertex even when
                // |x_A - x_C| > epsilon.  This is stronger than Geogram's
                // epsilon colocate (colocate.cpp), which uses min-index assignment
                // + chain-following.  Geogram's chain-following can fail when the
                // kd-tree assigns a vertex to a local minimum that is not the
                // global minimum of its transitive closure, producing two distinct
                // clusters for what should be one group.
                //
                // ── Comparison with Geogram's kd-tree (colocate.cpp) ──────────
                //
                // Geogram's colocate() (epsilon > 0 path) builds an ANN kd-tree
                // and queries each vertex for its K nearest neighbors, doubling K
                // until all within-tolerance neighbors are found.  It is O(N log N)
                // for well-separated point sets, but degrades to O(N² log N) when
                // many points fall within the tolerance ball (repeated doublings
                // drive K → N).
                //
                // The sort+scan approach degrades to O(N²) when all vertices lie
                // in an epsilon-wide x-slab (e.g. a flat panel exactly in the YZ
                // plane) — but in that case the inner distance check fails fast on
                // dy or dz, keeping the practical constant small.
                //
                // Both approaches share the same O(N²) worst-case order of
                // magnitude for maximally degenerate point sets.  The kd-tree
                // approach carries additional overhead (tree construction, heap
                // allocations per query, NearestNeighborSearch virtual dispatch)
                // that is not justified for the repair use case.
                //
                // ── Why worst-case does not arise for repair inputs ──────────────
                //
                // The caller (repair.cpp bot_to_gte_init) sets
                //   epsilon = 1e-8 × bbox_diag.
                // For a 10 000 mm bounding box this is epsilon ≈ 0.0001 mm.
                // Fitting more than a handful of distinct mesh vertices into a
                // 0.0001 mm x-slab requires vertex density > 10 000/mm — far
                // beyond any CAD or scan mesh.  In practice W ≪ N for all real
                // inputs, the inner scan terminates in O(1) steps per vertex, and
                // the total cost is O(N log N) dominated by the initial sort.
                //
                // ── Summary ──────────────────────────────────────────────────────
                //
                //  · Correctness: stronger than Geogram (full union-find transitivity)
                //  · Worst-case:  O(N²) — same order as Geogram's kd-tree approach
                //  · Practical:   O(N log N) for all real-world repair inputs
                //  · Dependencies: none (std::sort + in-line union-find only)

                // --- Union-find with path-halving ---
                std::vector<int32_t> uf(static_cast<size_t>(numVertices));
                std::iota(uf.begin(), uf.end(), static_cast<int32_t>(0));

                // Path-halving find (O(α(n)) amortised).
                auto uf_find = [&](int32_t v) -> int32_t
                {
                    while (uf[static_cast<size_t>(v)] != v)
                    {
                        // Path halving: point to grandparent.
                        uf[static_cast<size_t>(v)] =
                            uf[static_cast<size_t>(uf[static_cast<size_t>(v)])];
                        v = uf[static_cast<size_t>(v)];
                    }
                    return v;
                };

                // Union by smaller root index for deterministic canonical vertex
                // (always the lowest original index in the group).
                auto uf_union = [&](int32_t a, int32_t b)
                {
                    a = uf_find(a);
                    b = uf_find(b);
                    if (a == b) { return; }
                    if (a > b) { std::swap(a, b); }
                    uf[static_cast<size_t>(b)] = a;  // b → a (a is smaller)
                };

                // --- Sort vertex indices by x-coordinate ---
                std::vector<int32_t> sortedIdx(static_cast<size_t>(numVertices));
                std::iota(sortedIdx.begin(), sortedIdx.end(), static_cast<int32_t>(0));
                std::sort(sortedIdx.begin(), sortedIdx.end(),
                    [&](int32_t a, int32_t b)
                    {
                        return vertices[static_cast<size_t>(a)][0]
                             < vertices[static_cast<size_t>(b)][0];
                    });

                // --- Scan forward and unite all pairs within epsilon ---
                Real epsSq = epsilon * epsilon;
                for (int32_t si = 0; si < numVertices; ++si)
                {
                    int32_t i = sortedIdx[static_cast<size_t>(si)];
                    for (int32_t sj = si + 1; sj < numVertices; ++sj)
                    {
                        int32_t j = sortedIdx[static_cast<size_t>(sj)];
                        Real dx = vertices[static_cast<size_t>(j)][0]
                                - vertices[static_cast<size_t>(i)][0];
                        if (dx > epsilon) { break; }  // all further j are strictly farther in x
                        Real dy = vertices[static_cast<size_t>(i)][1]
                                - vertices[static_cast<size_t>(j)][1];
                        Real dz = vertices[static_cast<size_t>(i)][2]
                                - vertices[static_cast<size_t>(j)][2];
                        if (dx * dx + dy * dy + dz * dz <= epsSq)
                        {
                            uf_union(i, j);
                        }
                    }
                }

                // Build colocated mapping from union-find roots.
                for (int32_t i = 0; i < numVertices; ++i)
                {
                    colocated[static_cast<size_t>(i)] = uf_find(i);
                }
            }
        }

        // Detect isolated vertices (not referenced by any triangle).
        static void DetectIsolatedVertices(
            std::vector<Vector3<Real>> const& vertices,
            std::vector<std::array<int32_t, 3>> const& triangles,
            std::vector<bool>& isolated)
        {
            int32_t numVertices = static_cast<int32_t>(vertices.size());
            isolated.resize(numVertices);
            std::fill(isolated.begin(), isolated.end(), true);

            for (auto const& tri : triangles)
            {
                isolated[tri[0]] = false;
                isolated[tri[1]] = false;
                isolated[tri[2]] = false;
            }
        }

        // Detect degenerate facets (triangles with repeated vertices).
        static void DetectDegenerateFacets(
            std::vector<std::array<int32_t, 3>> const& triangles,
            std::vector<bool>& degenerate)
        {
            degenerate.resize(triangles.size());
            for (size_t i = 0; i < triangles.size(); ++i)
            {
                auto const& tri = triangles[i];
                degenerate[i] = (tri[0] == tri[1] || tri[1] == tri[2] || tri[2] == tri[0]);
            }
        }

    private:
        // Merge colocated vertices and update triangle indices.
        static void ColocateVertices(
            std::vector<Vector3<Real>>& vertices,
            std::vector<std::array<int32_t, 3>>& triangles,
            Real epsilon)
        {
            std::vector<int32_t> colocated;
            DetectColocatedVertices(vertices, colocated, epsilon);

            // Remap triangle indices to canonical vertices
            for (auto& tri : triangles)
            {
                tri[0] = colocated[tri[0]];
                tri[1] = colocated[tri[1]];
                tri[2] = colocated[tri[2]];
            }

            // Compact vertex array: build mapping from old to new indices
            int32_t numVertices = static_cast<int32_t>(vertices.size());
            std::vector<int32_t> oldToNew(numVertices, -1);
            std::vector<Vector3<Real>> newVertices;
            newVertices.reserve(numVertices);

            for (int32_t i = 0; i < numVertices; ++i)
            {
                if (colocated[i] == i)
                {
                    // This is a canonical vertex, keep it
                    oldToNew[i] = static_cast<int32_t>(newVertices.size());
                    newVertices.push_back(vertices[i]);
                }
            }

            // Update non-canonical vertices to point to new indices
            for (int32_t i = 0; i < numVertices; ++i)
            {
                if (colocated[i] != i)
                {
                    oldToNew[i] = oldToNew[colocated[i]];
                }
            }

            // Update triangle indices
            for (auto& tri : triangles)
            {
                tri[0] = oldToNew[tri[0]];
                tri[1] = oldToNew[tri[1]];
                tri[2] = oldToNew[tri[2]];
            }

            vertices = std::move(newVertices);
        }

    public:
        // ── Manifold-topology repair helpers ────────────────────────────────
        // Direct translation of Geogram's mesh_repair.cpp functions:
        //   repair_connect_facets, repair_reorient_facets_anti_moebius,
        //   repair_split_non_manifold_vertices.
        //
        // These are NOT called by Repair() by default (see the detailed
        // comment in Repair() above), but are exposed here for callers that
        // need the full Geogram-equivalent topology-repair sequence.  Call
        // them in order:
        //
        //   std::vector<int32_t> adj;
        //   MeshRepair<Real>::ConnectFacets(triangles, adj);
        //   MeshRepair<Real>::ReorientFacetsAntiMoebius(vertices, triangles, adj);
        //   MeshRepair<Real>::SplitNonManifoldVertices(vertices, triangles, adj);
        // ─────────────────────────────────────────────────────────────────────

        // Build per-facet adjacency adj[f*3+e] for the directed half-edge
        // (tri[e] → tri[(e+1)%3]) of facet f:
        //   ≥ 0  : adjacent facet (manifold edge, one pair with opposite direction)
        //   −1   : boundary (unmatched edge)
        //   −2   : non-manifold sentinel (3+ triangles sharing the edge; treated as
        //          boundary to match Geogram's repair_connect_facets behaviour)
        //
        // Geogram's repair_connect_facets matches BOTH same-direction and
        // opposite-direction edge pairs (the reorient step later fixes orientation).
        // GTE uses undirected edge keys to achieve the same effect.
        static void ConnectFacets(
            std::vector<std::array<int32_t, 3>> const& triangles,
            std::vector<int32_t>& adj)
        {
            static constexpr int32_t NM = -2;  // non-manifold sentinel
            size_t n = triangles.size();
            adj.assign(n * 3, -1);

            // Map undirected edge {min(a,b), max(a,b)} → first/second incident face.
            // A third face flags the entry as non-manifold.
            using EKey = std::pair<int32_t, int32_t>;
            std::map<EKey, std::pair<int32_t, int32_t>> edgeMap;

            for (size_t f = 0; f < n; ++f)
            {
                for (int e = 0; e < 3; ++e)
                {
                    int32_t va = triangles[f][e];
                    int32_t vb = triangles[f][(e + 1) % 3];
                    EKey key = {std::min(va, vb), std::max(va, vb)};

                    auto [it, inserted] = edgeMap.emplace(key, std::make_pair(int32_t(-1), int32_t(-1)));
                    if (it->second.first == NM)
                    {
                        // Already non-manifold — leave flagged
                    }
                    else if (it->second.first < 0)
                    {
                        it->second.first = static_cast<int32_t>(f);
                    }
                    else if (it->second.second < 0)
                    {
                        it->second.second = static_cast<int32_t>(f);
                    }
                    else
                    {
                        it->second = {NM, NM};  // third triangle → non-manifold
                    }
                }
            }

            for (size_t f = 0; f < n; ++f)
            {
                for (int e = 0; e < 3; ++e)
                {
                    int32_t va = triangles[f][e];
                    int32_t vb = triangles[f][(e + 1) % 3];
                    EKey key = {std::min(va, vb), std::max(va, vb)};

                    auto it = edgeMap.find(key);
                    if (it == edgeMap.end()) { continue; }
                    int32_t f0 = it->second.first;
                    int32_t f1 = it->second.second;
                    if (f0 == NM || f1 < 0) { continue; }  // NM or boundary
                    adj[f * 3 + e] = (f0 == static_cast<int32_t>(f)) ? f1 : f0;
                }
            }
        }

        // Returns +1 if f1 and f2 are consistently oriented (share an edge in
        // opposite directions, as expected for a 2-manifold), −1 if inconsistently
        // oriented (same directed edge shared), 0 if not adjacent.
        // Translation of Geogram's repair_relative_orientation().
        static int32_t RelativeOrientation(
            std::vector<std::array<int32_t, 3>> const& tris,
            int32_t f1, int32_t f2)
        {
            for (int e1 = 0; e1 < 3; ++e1)
            {
                int32_t v11 = tris[f1][e1];
                int32_t v12 = tris[f1][(e1 + 1) % 3];
                for (int e2 = 0; e2 < 3; ++e2)
                {
                    int32_t v21 = tris[f2][e2];
                    int32_t v22 = tris[f2][(e2 + 1) % 3];
                    if (v11 == v21 && v12 == v22) { return -1; }  // same dir → bad
                    if (v11 == v22 && v12 == v21) { return  1; }  // opp  dir → good
                }
            }
            return 0;
        }

        // Remove the adjacency link between facets f1 and f2 in adj.
        // Translation of Geogram's repair_dissociate().
        static void Dissociate(
            std::vector<int32_t>& adj,
            int32_t f1, int32_t f2)
        {
            for (int e = 0; e < 3; ++e)
            {
                if (adj[f1 * 3 + e] == f2) { adj[f1 * 3 + e] = -1; }
                if (adj[f2 * 3 + e] == f1) { adj[f2 * 3 + e] = -1; }
            }
        }

        // Propagate orientation from already-visited neighbours to triangle f.
        // If a Möbius conflict is detected, the minority edges are dissociated.
        // After this call, f may have been flipped and adj updated.
        // Translation of Geogram's repair_propagate_orientation().
        static void PropagateOrientation(
            std::vector<std::array<int32_t, 3>>& tris,
            std::vector<int32_t>& adj,
            int32_t f,
            std::vector<bool> const& visited)
        {
            int32_t nbPlus = 0, nbMinus = 0;
            for (int e = 0; e < 3; ++e)
            {
                int32_t g = adj[f * 3 + e];
                if (g < 0 || !visited[static_cast<size_t>(g)]) { continue; }
                int32_t ori = RelativeOrientation(tris, f, g);
                if (ori ==  1) { ++nbPlus;  }
                if (ori == -1) { ++nbMinus; }
            }

            if (nbPlus != 0 && nbMinus != 0)
            {
                // Möbius conflict: disconnect the minority.
                if (nbPlus > nbMinus)
                {
                    nbMinus = 0;
                    for (int e = 0; e < 3; ++e)
                    {
                        int32_t g = adj[f * 3 + e];
                        if (g < 0 || !visited[static_cast<size_t>(g)]) { continue; }
                        if (RelativeOrientation(tris, f, g) < 0)
                        {
                            Dissociate(adj, f, g);
                        }
                    }
                }
                else
                {
                    nbPlus = 0;
                    for (int e = 0; e < 3; ++e)
                    {
                        int32_t g = adj[f * 3 + e];
                        if (g < 0 || !visited[static_cast<size_t>(g)]) { continue; }
                        if (RelativeOrientation(tris, f, g) > 0)
                        {
                            Dissociate(adj, f, g);
                        }
                    }
                }
            }

            // After Möbius handling, if nbMinus > 0, flip f.
            (void)nbPlus;
            if (nbMinus != 0)
            {
                std::swap(tris[f][1], tris[f][2]);
                // Flip changes local-edge layout: old edge 0 ↔ new edge 2, edge 1 stays.
                std::swap(adj[f * 3 + 0], adj[f * 3 + 2]);
            }
        }

        // Reorient all triangles so that adjacent triangles have consistent
        // (anti-Möbius) orientation, using a BFS from border triangles outward.
        // Translation of Geogram's repair_reorient_facets_anti_moebius().
        static void ReorientFacetsAntiMoebius(
            std::vector<Vector3<Real>>& /*vertices*/,
            std::vector<std::array<int32_t, 3>>& tris,
            std::vector<int32_t>& adj)
        {
            size_t n = tris.size();
            if (n == 0) { return; }

            // Compute border distance for each triangle (clamped to MAX_ITER=5).
            // Border triangles (any edge adjacent to -1) get distance 0.
            // Translation of Geogram's compute_border_distance().
            static constexpr int32_t MAX_ITER = 5;
            std::vector<int8_t> dist(n, static_cast<int8_t>(MAX_ITER));
            for (size_t f = 0; f < n; ++f)
            {
                for (int e = 0; e < 3; ++e)
                {
                    if (adj[f * 3 + e] < 0)
                    {
                        dist[f] = 0;
                        break;
                    }
                }
            }
            for (int32_t iter = 1; iter < MAX_ITER; ++iter)
            {
                for (size_t f = 0; f < n; ++f)
                {
                    if (dist[f] == static_cast<int8_t>(MAX_ITER))
                    {
                        for (int e = 0; e < 3; ++e)
                        {
                            int32_t g = adj[f * 3 + e];
                            if (g >= 0 && dist[static_cast<size_t>(g)] == static_cast<int8_t>(iter - 1))
                            {
                                dist[f] = static_cast<int8_t>(iter);
                                break;
                            }
                        }
                    }
                }
            }

            // BFS from border outward, propagating consistent orientation.
            // Uses a priority queue (highest border distance first) matching
            // Geogram's SimplePriorityQueue: interior facets are processed before
            // boundary ones, so all inner neighbours are already oriented before
            // border-adjacent facets are handled.  This reduces Möbius conflicts.
            std::vector<bool> visited(n, false);

            using PQEntry = std::pair<int8_t, int32_t>;  // {dist, facet}
            std::priority_queue<PQEntry> pq;

            for (int32_t d = MAX_ITER; d >= 0; --d)
            {
                for (size_t f = 0; f < n; ++f)
                {
                    if (!visited[f] && dist[f] == static_cast<int8_t>(d))
                    {
                        visited[f] = true;
                        pq.push({dist[f], static_cast<int32_t>(f)});
                        while (!pq.empty())
                        {
                            int32_t f1 = pq.top().second;
                            pq.pop();
                            for (int e = 0; e < 3; ++e)
                            {
                                int32_t f2 = adj[f1 * 3 + e];
                                if (f2 < 0 || visited[static_cast<size_t>(f2)]) { continue; }
                                visited[static_cast<size_t>(f2)] = true;
                                PropagateOrientation(tris, adj, f2, visited);
                                pq.push({dist[static_cast<size_t>(f2)], f2});
                            }
                        }
                    }
                }
            }
        }

        // Split vertices that are shared by multiple disconnected triangle fans.
        // Each fan component of a non-manifold vertex gets its own vertex copy.
        // Translation of Geogram's repair_split_non_manifold_vertices().
        static void SplitNonManifoldVertices(
            std::vector<Vector3<Real>>& vertices,
            std::vector<std::array<int32_t, 3>>& triangles,
            std::vector<int32_t> const& adj)
        {
            size_t nFacets = triangles.size();
            size_t nVerts  = vertices.size();

            // c_is_visited: one entry per (facet, local-corner) = triangle corner.
            // A "corner" (f, lv) corresponds to vertex triangles[f][lv].
            std::vector<bool> cVisited(nFacets * 3, false);
            std::vector<bool> vUsed(nVerts, false);

            std::vector<Vector3<Real>> newVerts;  // new vertex positions

            for (size_t f = 0; f < nFacets; ++f)
            {
                for (int lv = 0; lv < 3; ++lv)
                {
                    size_t c = f * 3 + static_cast<size_t>(lv);
                    if (cVisited[c]) { continue; }

                    int32_t oldV = triangles[f][lv];
                    int32_t newV = oldV;

                    if (vUsed[static_cast<size_t>(oldV)])
                    {
                        // Another fan for this vertex → create a new vertex
                        newV = static_cast<int32_t>(nVerts + newVerts.size());
                        newVerts.push_back(vertices[static_cast<size_t>(oldV)]);
                    }
                    else
                    {
                        vUsed[static_cast<size_t>(oldV)] = true;
                    }

                    // Walk forward around the fan of triangles sharing oldV,
                    // following next_corner_around_vertex via adjacency.
                    // For triangle (v0,v1,v2) at corner lv (vertex = v_lv):
                    //   forward step:  adj through edge (v_lv → v_{(lv+1)%3})
                    //                  then find the corner at new facet that has oldV
                    //   Translation of Geogram's repair_split_non_manifold_vertices.
                    size_t curF = f;
                    int    curLv = lv;
                    for (;;)
                    {
                        cVisited[curF * 3 + static_cast<size_t>(curLv)] = true;
                        triangles[curF][curLv] = newV;

                        int32_t nextFacet = adj[curF * 3 + static_cast<size_t>(curLv)];
                        if (nextFacet < 0 || static_cast<size_t>(nextFacet) == f) { break; }

                        // Find the corner in nextFacet that has oldV
                        int found = -1;
                        for (int k = 0; k < 3; ++k)
                        {
                            if (triangles[static_cast<size_t>(nextFacet)][k] == oldV)
                            {
                                found = k;
                                break;
                            }
                        }
                        if (found < 0) { break; }  // oldV not in nextFacet (shouldn't happen)
                        curF  = static_cast<size_t>(nextFacet);
                        curLv = found;
                    }

                    // If we hit a boundary (forward walk ended at adj==-1), also walk
                    // backward to cover the other side of the fan.
                    // Translation of Geogram's backward walk in repair_split_non_manifold_vertices.
                    if (adj[f * 3 + static_cast<size_t>(lv)] != static_cast<int32_t>(f))
                    {
                        // only do backward if the forward walk didn't close the loop
                        int32_t adjAtStart = adj[f * 3 + static_cast<size_t>(lv)];
                        if (adjAtStart < 0)
                        {
                            // Boundary at start of forward walk: walk backward
                            // prev corner of f for vertex lv: prev local edge = (lv+2)%3
                            curF  = f;
                            curLv = lv;
                            for (;;)
                            {
                                int prevE = (curLv + 2) % 3;
                                int32_t prevFacet = adj[curF * 3 + static_cast<size_t>(prevE)];
                                if (prevFacet < 0) { break; }

                                int found = -1;
                                for (int k = 0; k < 3; ++k)
                                {
                                    if (triangles[static_cast<size_t>(prevFacet)][k] == oldV)
                                    {
                                        found = k;
                                        break;
                                    }
                                }
                                if (found < 0) { break; }
                                curF  = static_cast<size_t>(prevFacet);
                                curLv = found;
                                cVisited[curF * 3 + static_cast<size_t>(curLv)] = true;
                                triangles[curF][curLv] = newV;
                            }
                        }
                    }
                }
            }

            if (!newVerts.empty())
            {
                vertices.insert(vertices.end(), newVerts.begin(), newVerts.end());
            }
        }

    private:
        // Remove degenerate and optionally duplicate facets.
        static void RemoveBadFacets(
            std::vector<Vector3<Real>>& vertices,
            std::vector<std::array<int32_t, 3>>& triangles,
            bool checkDuplicates)
        {
            std::vector<bool> degenerate;
            DetectDegenerateFacets(triangles, degenerate);

            std::set<std::array<int32_t, 3>> uniqueFacets;
            std::vector<std::array<int32_t, 3>> newTriangles;
            newTriangles.reserve(triangles.size());

            for (size_t i = 0; i < triangles.size(); ++i)
            {
                if (degenerate[i])
                {
                    continue; // Skip degenerate triangles
                }

                if (checkDuplicates)
                {
                    // Normalize triangle to canonical form for duplicate detection
                    std::array<int32_t, 3> normalized = triangles[i];
                    std::sort(normalized.begin(), normalized.end());

                    if (uniqueFacets.find(normalized) != uniqueFacets.end())
                    {
                        continue; // Skip duplicate
                    }
                    uniqueFacets.insert(normalized);
                }

                newTriangles.push_back(triangles[i]);
            }

            triangles = std::move(newTriangles);
        }

        // Remove vertices not referenced by any triangle.
        static void RemoveIsolatedVertices(
            std::vector<Vector3<Real>>& vertices,
            std::vector<std::array<int32_t, 3>>& triangles)
        {
            std::vector<bool> isolated;
            DetectIsolatedVertices(vertices, triangles, isolated);

            // Build mapping from old to new vertex indices
            int32_t numVertices = static_cast<int32_t>(vertices.size());
            std::vector<int32_t> oldToNew(numVertices, -1);
            std::vector<Vector3<Real>> newVertices;
            newVertices.reserve(numVertices);

            for (int32_t i = 0; i < numVertices; ++i)
            {
                if (!isolated[i])
                {
                    oldToNew[i] = static_cast<int32_t>(newVertices.size());
                    newVertices.push_back(vertices[i]);
                }
            }

            // Update triangle indices
            for (auto& tri : triangles)
            {
                tri[0] = oldToNew[tri[0]];
                tri[1] = oldToNew[tri[1]];
                tri[2] = oldToNew[tri[2]];
            }

            vertices = std::move(newVertices);
        }
    };
}

// Hash functions for GTE types to support unordered containers.
// These need to be in the std namespace and defined before use.
namespace std
{
    // Hash function for gte::Vector3
    template <typename Real>
    struct hash<gte::Vector<3, Real>>
    {
        size_t operator()(gte::Vector<3, Real> const& v) const
        {
            size_t h1 = std::hash<Real>{}(v[0]);
            size_t h2 = std::hash<Real>{}(v[1]);
            size_t h3 = std::hash<Real>{}(v[2]);
            // Combine hashes using a simple formula
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}
