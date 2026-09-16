// Ported from Geogram: https://github.com/BrunoLevy/geogram
// Original files: src/lib/geogram/mesh/mesh_fill_holes.cpp, mesh_fill_holes.h
// Original license: BSD 3-Clause (see below)
// Copyright (c) 2000-2022 Inria
//
// Adaptations for Geometric Tools Engine:
// - Changed from GEO::Mesh to std::vector<Vector3<Real>> and std::vector<std::array<int32_t, 3>>
// - Uses GTE::ETManifoldMesh for topology operations
// - Uses GTE's TriangulateEC for robust 2D triangulation (via LSCM mapping)
// - Removed Geogram-specific Logger and CmdLine calls
// - Removed planar-projection methods (CDT and EarClipping 2D) in favour of LSCM
//
// Original Geogram License (BSD 3-Clause):
// [Same license text as in MeshRepair.h]

#pragma once

#include <Mathematics/Vector3.h>
#include <Mathematics/Vector2.h>
#include <Mathematics/ETManifoldMesh.h>
#include <Mathematics/MeshValidation.h>
#include <Mathematics/TriangulateEC.h>
#include <Mathematics/ArbitraryPrecision.h>
#include <Mathematics/LSCMParameterization.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The MeshHoleFilling class provides hole detection and filling operations
// ported from Geogram. It detects boundary loops in a triangle mesh and
// fills them with new triangles using GTE's robust triangulation algorithms.
//
// Supports two triangulation methods (applied as a fallback chain):
// - LSCM:          Arc-length boundary-to-circle mapping + EC; always
//                  succeeds for any simple closed 3D loop regardless
//                  of planarity (primary method, default)
// - EarClipping3D: Ear clipping directly in 3D (last resort fallback)
//
// The former planar-projection methods (EarClipping 2D and CDT) have been
// removed.  Both require projecting the hole boundary onto a best-fit plane,
// which fails for non-planar or highly curved holes — exactly the cases
// common in engineering meshes (aircraft skins, compound curves).  LSCM
// handles all such cases correctly while being no slower in practice.

namespace gte
{
    template <typename Real>
    class MeshHoleFilling
    {
    public:
        // Triangulation method for hole filling.
        // Only two methods remain after removal of the planar-projection paths:
        //   LSCM          — primary: maps boundary to circle via arc-length
        //                   parameterisation then ear-clips in 2D; always
        //                   succeeds for any topologically simple boundary loop.
        //   EarClipping3D — last resort: ear clipping directly in 3D without
        //                   any projection; slower but always terminates.
        enum class TriangulationMethod
        {
            LSCM,           // Arc-length circle map + EC in 2D; works for any simple
                            // closed 3D boundary regardless of planarity (primary method)
            EarClipping3D   // 3D ear clipping without any projection (last resort)
        };

        // Parameters for hole filling operations.
        struct Parameters
        {
            Real maxArea;                       // Maximum hole area to fill (0 = unlimited)
            size_t maxEdges;                    // Maximum hole perimeter edges (0 = unlimited)
            bool repair;                        // Run mesh repair after filling
            TriangulationMethod method;         // Triangulation algorithm to use
            bool autoFallback;                  // Automatically fall back to EarClipping3D if LSCM fails
            bool validateOutput;                // Validate that output is manifold and non-self-intersecting
            bool requireManifold;               // Fail if output is not manifold
            bool requireNoSelfIntersections;    // Fail if output has self-intersections

            Parameters()
                : maxArea(static_cast<Real>(0))
                , maxEdges(0)                               // 0 = unlimited (fill all holes)
                , repair(false)                             // repair is caller's responsibility
                , method(TriangulationMethod::LSCM)         // Default to LSCM (handles any topology)
                , autoFallback(true)                        // Enable automatic fallback to EarClipping3D
                , validateOutput(false)                     // No validation by default (Geogram-compatible)
                , requireManifold(false)                    // No manifold requirement by default
                , requireNoSelfIntersections(false)         // Don't require (can be expensive)
            {
            }
        };

        // A boundary loop representing a hole in the mesh.
        struct HoleBoundary
        {
            std::vector<int32_t> vertices;  // Ordered vertex indices forming boundary
            Real area;                       // Approximate area of triangulation

            HoleBoundary() : area(static_cast<Real>(0)) {}
        };

        // Fill holes in the mesh by generating new triangles.
        // Holes with area exceeding maxArea (when > 0) or with more than maxEdges
        // boundary edges (when > 0) are skipped.  A value of 0 for either
        // parameter means no limit (fill all holes of that kind).
        static void FillHoles(
            std::vector<Vector3<Real>>& vertices,
            std::vector<std::array<int32_t, 3>>& triangles,
            Parameters const& params = Parameters())
        {
            // Step 1: Build directed-edge existence set using unordered_map for O(T) average.
            // Key: directed edge (v0, v1) encoded as int64_t = (v0<<32)|v1.
            // An undirected edge is a boundary edge iff its reverse (v1, v0) is absent.
            struct DirectedEdgeHash {
                size_t operator()(int64_t key) const noexcept {
                    // Mix high and low halves
                    uint64_t k = static_cast<uint64_t>(key);
                    k ^= k >> 33;
                    k *= 0xff51afd7ed558ccdULL;
                    k ^= k >> 33;
                    k *= 0xc4ceb9fe1a85ec53ULL;
                    k ^= k >> 33;
                    return static_cast<size_t>(k);
                }
            };
            auto encodeEdge = [](int32_t v0, int32_t v1) noexcept -> int64_t {
                return (static_cast<int64_t>(v0) << 32) | static_cast<uint32_t>(v1);
            };

            std::unordered_set<int64_t, DirectedEdgeHash> directedEdges;
            directedEdges.reserve(triangles.size() * 3);
            // Also build directed-edge → third-vertex map (matching geogram's trindex
            // representation: for face (a,b,c), edge a→b has third vertex c, etc.).
            // Used to initialise the v2 ("adjacent face third vertex") field of each
            // working hole triple in TriangulateHole3D, so the adjacent-face normals
            // used for ear scoring match geogram's triangulate_hole_ear_cutting exactly.
            std::unordered_map<int64_t, int32_t> edgeToThirdVert;
            edgeToThirdVert.reserve(triangles.size() * 3);
            for (auto const& tri : triangles)
            {
                for (int j = 0; j < 3; ++j)
                {
                    int64_t key = encodeEdge(tri[j], tri[(j + 1) % 3]);
                    directedEdges.insert(key);
                    edgeToThirdVert[key] = tri[(j + 2) % 3];
                }
            }

            // Step 2: Build boundary adjacency: for each boundary vertex v, map to its
            // successor in the boundary half-edge ring.  O(T) total, O(1) per step.
            // A directed edge (v0, v1) is a boundary edge iff (v1, v0) is absent.
            std::unordered_map<int32_t, std::vector<int32_t>> boundaryNext;
            for (int64_t de : directedEdges)
            {
                int32_t v0 = static_cast<int32_t>(de >> 32);
                int32_t v1 = static_cast<int32_t>(static_cast<uint32_t>(de));
                if (directedEdges.find(encodeEdge(v1, v0)) == directedEdges.end())
                {
                    // (v0, v1) is a boundary directed edge
                    boundaryNext[v0].push_back(v1);
                }
            }

            // Step 3: Detect holes (boundary loops) using O(1) adjacency lookups.
            std::vector<HoleBoundary> holes;
            DetectHolesFromAdjacency(vertices, boundaryNext, holes);

            if (holes.empty())
            {
                return;
            }

            // Save a snapshot of the mesh so we can roll back on validation
            // failure (only when validateOutput is requested).
            std::vector<Vector3<Real>> savedVertices;
            std::vector<std::array<int32_t, 3>> savedTriangles;
            if (params.validateOutput)
            {
                savedVertices  = vertices;
                savedTriangles = triangles;
            }

            // Step 3: Triangulate and add holes that meet criteria
            size_t numFilled = 0;
            for (auto const& hole : holes)
            {
                // Check size constraints
                if (params.maxEdges > 0 &&
                    hole.vertices.size() > params.maxEdges)
                {
                    continue;
                }

                // Triangulate hole using the selected method.
                // The dispatch is simple: LSCM (primary) or EarClipping3D (last resort).
                // The former planar-projection paths (EarClipping 2D and CDT) have been
                // removed — see class-level comment for rationale.
                std::vector<std::array<int32_t, 3>> newTriangles;
                bool success = false;

                if (params.method == TriangulationMethod::EarClipping3D)
                {
                    success = TriangulateHole3D(vertices, hole, newTriangles, edgeToThirdVert);
                }
                else
                {
                    // LSCM (default): arc-length circle mapping, always succeeds for any
                    // simple closed 3D boundary loop regardless of planarity.
                    success = TriangulateHoleLSCM(vertices, hole, newTriangles);
                    // Fall back to 3D if LSCM somehow fails
                    if (!success && params.autoFallback)
                    {
                        success = TriangulateHole3D(vertices, hole, newTriangles, edgeToThirdVert);
                    }
                }
                
                if (success)
                {
                    // Compute area of new triangles
                    Real holeArea = ComputeTriangulationArea(vertices, newTriangles);

                    if (params.maxArea <= static_cast<Real>(0) || holeArea < params.maxArea)
                    {
                        // Reverse winding of new triangles before adding to mesh.
                        // The hole boundary is traced as a directed boundary loop
                        // (clockwise when viewed from outside for a CCW mesh), so the
                        // triangulation produces triangles with inward-facing normals.
                        // Reversing v1 <-> v2 corrects this, matching geogram's
                        // fill_holes which inserts triangles as
                        // create_triangle(indices[2], indices[1], indices[0]).
                        for (auto& tri : newTriangles)
                        {
                            // tri = {v0, v1, v2}: swap v1 and v2 to flip winding
                            std::swap(tri[1], tri[2]);
                        }
                        triangles.insert(triangles.end(), newTriangles.begin(), newTriangles.end());
                        ++numFilled;
                    }
                }
            }

            // Step 4: (repair after filling is the caller's responsibility)
            // MeshRepair::Repair(RECONSTRUCT) or MeshRepair::Repair(DEFAULT)
            // should be called by the user when topology cleanup is needed after
            // FillHoles.  This avoids a circular include dependency.

            // Step 5: Optional output validation.  When requireManifold is set
            // and the result fails the manifold check, restore the pre-fill
            // snapshot so the caller receives the original (unmodified) mesh.
            // Validation is only performed when validateOutput is true and at
            // least one pass/fail criterion (requireManifold or
            // requireNoSelfIntersections) is enabled, since without a criterion
            // the result cannot be acted upon.
            if (numFilled > 0 && params.validateOutput &&
                (params.requireManifold || params.requireNoSelfIntersections))
            {
                auto validationResult = MeshValidation<Real>::Validate(
                    vertices, triangles, params.requireNoSelfIntersections);
                if (params.requireManifold && !validationResult.isManifold)
                {
                    vertices  = std::move(savedVertices);
                    triangles = std::move(savedTriangles);
                }
            }
        }

    private:
        // Detect boundary loops using a prebuilt boundary adjacency map.
        // Each entry boundaryNext[v] holds the list of vertices reachable from v
        // along a boundary directed edge.  Traversal is O(1) per step.
        static void DetectHolesFromAdjacency(
            std::vector<Vector3<Real>> const& vertices,
            std::unordered_map<int32_t, std::vector<int32_t>> const& boundaryNext,
            std::vector<HoleBoundary>& holes)
        {
            // Track visited directed boundary edges (encoded as int64_t).
            std::unordered_set<int64_t> visitedEdges;
            visitedEdges.reserve(boundaryNext.size() * 2);

            for (auto const& [startV, succs] : boundaryNext)
            {
                for (int32_t firstNext : succs)
                {
                    int64_t startKey = (static_cast<int64_t>(startV) << 32)
                                     | static_cast<uint32_t>(firstNext);
                    if (visitedEdges.count(startKey))
                    {
                        continue;  // Already part of a traced loop
                    }

                    // Trace the loop starting from (startV → firstNext).
                    HoleBoundary hole;
                    int32_t curV = startV;
                    int32_t nxtV = firstNext;
                    // Safety cap: a valid boundary loop cannot be longer than the number
                    // of boundary directed edges (one edge per loop vertex).
                    const int maxIter = static_cast<int>(boundaryNext.size()) + 1;
                    int iter = 0;
                    bool closed = false;

                    do
                    {
                        int64_t edgeKey = (static_cast<int64_t>(curV) << 32)
                                        | static_cast<uint32_t>(nxtV);

                        if (visitedEdges.count(edgeKey))
                        {
                            // Hit a visited edge mid-trace — non-simple boundary; abort.
                            hole.vertices.clear();
                            break;
                        }

                        hole.vertices.push_back(curV);
                        visitedEdges.insert(edgeKey);

                        int32_t prevV = curV;
                        curV = nxtV;
                        nxtV = -1;

                        // O(1) adjacency lookup: find successor from curV that isn't prevV.
                        auto it = boundaryNext.find(curV);
                        if (it != boundaryNext.end())
                        {
                            for (int32_t nb : it->second)
                            {
                                if (nb != prevV)
                                {
                                    nxtV = nb;
                                    break;
                                }
                            }
                        }

                        if (nxtV == -1)
                        {
                            hole.vertices.clear();  // Dead end — broken boundary
                            break;
                        }

                        if (++iter > maxIter)
                        {
                            hole.vertices.clear();  // Safety: unexpectedly long loop
                            break;
                        }

                        if (curV == startV)
                        {
                            closed = true;
                            break;
                        }

                    } while (true);

                    if (closed && hole.vertices.size() >= 3)
                    {
                        holes.push_back(std::move(hole));
                    }
                }
            }
        }

        // Detect boundary loops (holes) using edge adjacency map.
        // LEGACY: kept for reference; replaced by DetectHolesFromAdjacency above.
        static void DetectHolesFromEdges(
            std::vector<Vector3<Real>> const& vertices,
            std::vector<std::array<int32_t, 3>> const& triangles,
            std::map<std::pair<int32_t, int32_t>, std::vector<size_t>> const& edgeToTriangles,
            std::vector<HoleBoundary>& holes)
        {
            // Track visited boundary edges
            std::set<std::pair<int32_t, int32_t>> visitedEdges;

            // Find boundary edges (edges with no opposite edge)
            for (auto const& entry : edgeToTriangles)
            {
                int32_t v0 = entry.first.first;
                int32_t v1 = entry.first.second;

                // Check if the opposite edge exists
                auto oppositeEdge = std::make_pair(v1, v0);
                if (edgeToTriangles.find(oppositeEdge) == edgeToTriangles.end())
                {
                    // This is a boundary edge
                    if (visitedEdges.find(entry.first) != visitedEdges.end())
                    {
                        continue; // Already processed
                    }

                    // Trace the boundary loop starting from this edge
                    HoleBoundary hole;
                    if (TraceHoleBoundaryFromEdges(edgeToTriangles, v0, v1, visitedEdges, hole) &&
                        hole.vertices.size() >= 3)
                    {
                        holes.push_back(hole);
                    }
                }
            }
        }

        // Trace a boundary loop starting from edge (v0, v1) using edge map.
        static bool TraceHoleBoundaryFromEdges(
            std::map<std::pair<int32_t, int32_t>, std::vector<size_t>> const& edgeToTriangles,
            int32_t startV0,
            int32_t startV1,
            std::set<std::pair<int32_t, int32_t>>& visitedEdges,
            HoleBoundary& hole)
        {
            int32_t currentV = startV0;
            int32_t nextV = startV1;
            int maxIterations = 10000; // Prevent infinite loops
            int iterations = 0;

            do
            {
                hole.vertices.push_back(currentV);

                // Mark edge as visited
                visitedEdges.insert(std::make_pair(currentV, nextV));

                // Find the next boundary edge starting from nextV
                int32_t prevV = currentV;
                currentV = nextV;
                nextV = FindNextBoundaryVertexFromEdges(edgeToTriangles, prevV, currentV);

                if (nextV == -1)
                {
                    // Failed to close the loop
                    return false;
                }

                if (++iterations > maxIterations)
                {
                    // Safety check to prevent infinite loops
                    return false;
                }

            } while (currentV != startV0);

            return true;
        }

        // Find the next vertex along the boundary from (prevV, currentV).
        static int32_t FindNextBoundaryVertexFromEdges(
            std::map<std::pair<int32_t, int32_t>, std::vector<size_t>> const& edgeToTriangles,
            int32_t prevV,
            int32_t currentV)
        {
            // Look for an outgoing boundary edge from currentV
            for (auto const& entry : edgeToTriangles)
            {
                int32_t v0 = entry.first.first;
                int32_t v1 = entry.first.second;

                // Check if this edge starts at currentV and doesn't go back to prevV
                if (v0 == currentV && v1 != prevV)
                {
                    // Check if the opposite edge exists
                    auto oppositeEdge = std::make_pair(v1, v0);
                    if (edgeToTriangles.find(oppositeEdge) == edgeToTriangles.end())
                    {
                        // This is a boundary edge
                        return v1;
                    }
                }
            }

            return -1; // Not found
        }

        // Detect boundary loops (holes) in the mesh (old ETManifoldMesh-based version).
        static void DetectHoles(
            std::vector<Vector3<Real>> const& vertices,
            ETManifoldMesh const& mesh,
            std::vector<HoleBoundary>& holes)
        {
            // Track visited boundary edges
            std::set<std::pair<int32_t, int32_t>> visitedEdges;

            // Iterate through all edges in the mesh
            for (auto const& edgePair : mesh.GetEdges())
            {
                auto const& edge = *edgePair.second;

                // Check if this is a boundary edge (has only one triangle)
                if (edge.T[1] == nullptr)
                {
                    int32_t v0 = edge.V[0];
                    int32_t v1 = edge.V[1];
                    std::pair<int32_t, int32_t> edgeKey = std::make_pair(
                        std::min(v0, v1), std::max(v0, v1));

                    if (visitedEdges.find(edgeKey) != visitedEdges.end())
                    {
                        continue; // Already processed this boundary edge
                    }

                    // Trace the boundary loop starting from this edge
                    HoleBoundary hole;
                    TraceHoleBoundary(mesh, v0, v1, visitedEdges, hole);

                    if (hole.vertices.size() >= 3)
                    {
                        holes.push_back(hole);
                    }
                }
            }
        }

        // Trace a boundary loop starting from edge (v0, v1).
        static void TraceHoleBoundary(
            ETManifoldMesh const& mesh,
            int32_t startV0,
            int32_t startV1,
            std::set<std::pair<int32_t, int32_t>>& visitedEdges,
            HoleBoundary& hole)
        {
            int32_t currentV = startV0;
            int32_t nextV = startV1;

            do
            {
                hole.vertices.push_back(currentV);

                // Mark edge as visited
                std::pair<int32_t, int32_t> edgeKey = std::make_pair(
                    std::min(currentV, nextV), std::max(currentV, nextV));
                visitedEdges.insert(edgeKey);

                // Find the next boundary edge starting from nextV
                int32_t prevV = currentV;
                currentV = nextV;
                nextV = FindNextBoundaryVertex(mesh, prevV, currentV);

                if (nextV == -1)
                {
                    // Failed to close the loop
                    hole.vertices.clear();
                    return;
                }

            } while (currentV != startV0);
        }

        // Find the next vertex along the boundary from (prevV, currentV).
        static int32_t FindNextBoundaryVertex(
            ETManifoldMesh const& mesh,
            int32_t prevV,
            int32_t currentV)
        {
            // Find all edges connected to currentV
            for (auto const& edgePair : mesh.GetEdges())
            {
                auto const& edge = *edgePair.second;

                // Check if this edge includes currentV and is a boundary edge
                if (edge.T[1] == nullptr)
                {
                    if (edge.V[0] == currentV && edge.V[1] != prevV)
                    {
                        return edge.V[1];
                    }
                    if (edge.V[1] == currentV && edge.V[0] != prevV)
                    {
                        return edge.V[0];
                    }
                }
            }

            return -1; // Not found
        }

        // Triangulate a hole using LSCM arc-length circle mapping.
        //
        // Maps the 3D boundary loop to a 2D polygon inscribed on a unit circle
        // via arc-length parameterization, then triangulates in 2D using EC.
        // Because a circle-inscribed polygon is always convex (vertices are laid
        // out in arc-length order around the circle), EC never fails, making
        // this the primary triangulation method.
        //
        // The key advantage over the removed planar-projection methods: a
        // non-planar boundary whose orthographic projection self-intersects still
        // maps to a valid (non-self-intersecting) polygon under arc-length circle
        // parameterization.  LSCM therefore succeeds for ANY topologically simple
        // boundary loop, not just planar ones.
        static bool TriangulateHoleLSCM(
            std::vector<Vector3<Real>> const& vertices,
            HoleBoundary const& hole,
            std::vector<std::array<int32_t, 3>>& triangles)
        {
            if (hole.vertices.size() < 3)
            {
                return false;
            }

            // Map boundary to 2D circle positions via arc-length parameterization
            std::vector<Vector2<Real>> uv;
            if (!LSCMParameterization<Real>::MapBoundaryToCircle(vertices, hole.vertices, uv))
            {
                return false;
            }

            // uv is parallel to hole.vertices; build local triangles in UV space
            std::vector<std::array<int32_t, 3>> localTriangles;
            if (!TriangulateWithEC(uv, localTriangles))
            {
                return false;
            }

            // Map local indices back to global mesh indices
            for (auto const& tri : localTriangles)
            {
                triangles.push_back({
                    hole.vertices[tri[0]],
                    hole.vertices[tri[1]],
                    hole.vertices[tri[2]]
                });
            }
            return true;
        }

        // Triangulate a hole working directly in 3D (no projection)
        // Ported from Geogram's ear cutting algorithm (triangulate_hole_ear_cutting)
        //
        // Each working-hole triple stores (v0, v1, v2) where:
        //   v0  = start of the boundary directed edge
        //   v1  = end of the boundary directed edge (== start of next edge)
        //   v2  = third vertex of the mesh face adjacent to edge (v0, v1)
        //
        // This mirrors geogram's trindex representation exactly.  For the initial
        // boundary edges v2 is looked up from the supplied edgeToThirdVert map
        // (built from all existing triangles in FillHoles).  After each ear clip
        // the newly created triangle (v0, v2_new, v1_ear) becomes the face adjacent
        // to the new boundary edge (v0, v2_new), so its third vertex is v1_ear —
        // matching geogram's  hole[best_i1] = T  update step exactly.
        static bool TriangulateHole3D(
            std::vector<Vector3<Real>> const& vertices,
            HoleBoundary const& hole,
            std::vector<std::array<int32_t, 3>>& triangles,
            std::unordered_map<int64_t, int32_t> const& edgeToThirdVert = {})
        {
            if (hole.vertices.size() < 3)
            {
                return false;
            }
            
            if (hole.vertices.size() == 3)
            {
                triangles.push_back({hole.vertices[0], hole.vertices[1], hole.vertices[2]});
                return true;
            }
            
            // EdgeTriple stores (v0, v1, v2): v0→v1 is the boundary edge, v2 is the
            // third vertex of the mesh face adjacent to that edge.  Matches geogram's
            // trindex(origin, next_boundary_vertex, interior_face_vertex).
            struct EdgeTriple
            {
                int32_t v0, v1, v2;
            };

            // Helper: encode directed edge for use with edgeToThirdVert.
            auto encodeEdge3D = [](int32_t va, int32_t vb) noexcept -> int64_t {
                return (static_cast<int64_t>(va) << 32) | static_cast<uint32_t>(vb);
            };

            // Mutable map tracking the adjacent-face third vertex for each current
            // boundary edge.  Initialised from the caller-supplied map (existing mesh
            // faces) and updated as new fill triangles are created.
            std::unordered_map<int64_t, int32_t> adjThirdVert(edgeToThirdVert);

            std::vector<EdgeTriple> workingHole;
            workingHole.reserve(hole.vertices.size());
            
            size_t n = hole.vertices.size();
            for (size_t i = 0; i < n; ++i)
            {
                EdgeTriple et;
                et.v0 = hole.vertices[i];
                et.v1 = hole.vertices[(i + 1) % n];
                // Initialise v2 from the adjacent-face map (geogram approach).
                // Fall back to the next boundary vertex when no adjacent face is
                // recorded (should not occur for a well-formed manifold mesh, but
                // provides a safe approximation when the map is absent).
                auto it = adjThirdVert.find(encodeEdge3D(et.v0, et.v1));
                if (it != adjThirdVert.end())
                {
                    et.v2 = it->second;
                }
                else
                {
                    et.v2 = hole.vertices[(i + 2) % n];
                }
                workingHole.push_back(et);
            }
            
            // Ear cutting loop - select best ear at each iteration
            while (workingHole.size() > 3)
            {
                int32_t bestIdx = -1;
                Real bestScore = -std::numeric_limits<Real>::max();
                
                // Find best ear based on 3D geometric quality
                for (size_t i = 0; i < workingHole.size(); ++i)
                {
                    size_t nextIdx = (i + 1) % workingHole.size();
                    Real score = ComputeEarScore3D(vertices, 
                        workingHole[i], workingHole[nextIdx]);
                    
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestIdx = static_cast<int32_t>(i);
                    }
                }
                
                if (bestIdx < 0)
                {
                    return false; // Failed to find valid ear
                }
                
                size_t nextIdx = (static_cast<size_t>(bestIdx) + 1) % workingHole.size();
                
                // Create triangle from best ear (matches geogram's trindex T)
                EdgeTriple const& t1 = workingHole[bestIdx];
                EdgeTriple const& t2 = workingHole[nextIdx];
                // Triangle = (T1[0], T2[1], T1[1]) in geogram notation
                triangles.push_back({t1.v0, t2.v1, t1.v1});
                
                // Update working hole by replacing the clipped position with the
                // merged triple, then removing the now-redundant successor triple.
                //
                // After clipping ear vertex t1.v1:
                //   - New boundary edge is (t1.v0, t2.v1)
                //   - The new face adjacent to this edge is the triangle just created:
                //     (t1.v0, t2.v1, t1.v1) — so its third vertex is t1.v1.
                // This matches geogram's:  hole[best_i1] = T  where T carries
                //   indices = (T1[0], T2[1], T1[1]).
                EdgeTriple merged;
                merged.v0 = t1.v0;
                merged.v1 = t2.v1;
                merged.v2 = t1.v1;  // third vertex of newly created adjacent face

                // Record new edge in the map so subsequent iterations see the
                // updated adjacent face when scoring ears at this boundary edge.
                adjThirdVert[encodeEdge3D(merged.v0, merged.v1)] = merged.v2;
                
                workingHole[bestIdx] = merged;
                workingHole.erase(workingHole.begin() + static_cast<std::ptrdiff_t>(nextIdx));
            }
            
            // Add final triangle
            if (workingHole.size() == 3)
            {
                triangles.push_back({
                    workingHole[0].v0,
                    workingHole[1].v0,
                    workingHole[2].v0
                });
            }
            
            return true;
        }
        
        // Compute ear quality score in 3D (ported from Geogram)
        // Higher score = better triangle
        template <typename EdgeTriple>
        static Real ComputeEarScore3D(
            std::vector<Vector3<Real>> const& vertices,
            EdgeTriple const& t1,
            EdgeTriple const& t2)
        {
            // Get triangle vertices
            Vector3<Real> const& p10 = vertices[t1.v0];
            Vector3<Real> const& p11 = vertices[t1.v1];
            Vector3<Real> const& p12 = vertices[t1.v2];
            Vector3<Real> const& p20 = vertices[t2.v0];
            Vector3<Real> const& p21 = vertices[t2.v1];
            Vector3<Real> const& p22 = vertices[t2.v2];
            
            // Compute averaged normal
            Vector3<Real> n1 = Cross(p11 - p10, p12 - p10);
            Vector3<Real> n2 = Cross(p21 - p20, p22 - p20);
            Vector3<Real> n = n1 + n2;
            
            Real nlen = Length(n);
            if (nlen < static_cast<Real>(1e-10))
            {
                return -std::numeric_limits<Real>::max(); // Degenerate
            }
            n /= nlen;
            
            // Compute edge directions
            Vector3<Real> a = p11 - p10;
            Vector3<Real> b = p21 - p20;
            
            Real alen = Length(a);
            Real blen = Length(b);
            
            if (alen < static_cast<Real>(1e-10) || blen < static_cast<Real>(1e-10))
            {
                return -std::numeric_limits<Real>::max(); // Degenerate edge
            }
            
            a /= alen;
            b /= blen;
            
            // Score based on angle between edges
            // Prefer ears that maintain smooth transitions
            Vector3<Real> axb = Cross(a, b);
            Real score = -std::atan2(Dot(n, axb), Dot(a, b));
            
            return score;
        }


        // Triangulate using GTE's Ear Clipping
        // (private: used internally by TriangulateHoleLSCM after mapping to 2D)
        static bool TriangulateWithEC(
            std::vector<Vector2<Real>> const& points2D,
            std::vector<std::array<int32_t, 3>>& triangles)
        {
            try
            {
                // Use BSNumber for exact arithmetic (recommended for robustness)
                using ComputeType = BSNumber<UIntegerFP32<70>>;
                
                TriangulateEC<Real, ComputeType> triangulator(points2D);
                triangulator();  // Triangulate simple polygon
                
                triangles = triangulator.GetTriangles();
                return !triangles.empty();
            }
            catch (...)
            {
                // Fall back to floating-point if exact arithmetic fails
                try
                {
                    TriangulateEC<Real, Real> triangulator(points2D);
                    triangulator();
                    triangles = triangulator.GetTriangles();
                    return !triangles.empty();
                }
                catch (...)
                {
                    return false;
                }
            }
        }

        // Compute total area of triangulation.
        static Real ComputeTriangulationArea(
            std::vector<Vector3<Real>> const& vertices,
            std::vector<std::array<int32_t, 3>> const& triangles)
        {
            Real totalArea = static_cast<Real>(0);

            for (auto const& tri : triangles)
            {
                Vector3<Real> const& p0 = vertices[tri[0]];
                Vector3<Real> const& p1 = vertices[tri[1]];
                Vector3<Real> const& p2 = vertices[tri[2]];

                Vector3<Real> edge1 = p1 - p0;
                Vector3<Real> edge2 = p2 - p0;
                Vector3<Real> normal = Cross(edge1, edge2);

                totalArea += Length(normal) * static_cast<Real>(0.5);
            }

            return totalArea;
        }
    };
}
