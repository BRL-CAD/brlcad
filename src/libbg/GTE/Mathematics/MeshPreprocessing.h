// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2026
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
// https://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 8.0.2026.02.10
//
// Ported from Geogram: https://github.com/BrunoLevy/geogram
// Original files: src/lib/geogram/mesh/mesh_preprocessing.cpp, mesh_preprocessing.h
// Original license: BSD 3-Clause (see below)
// Copyright (c) 2000-2022 Inria
//
// Adaptations for Geometric Tools Engine:
// - Changed from GEO::Mesh to std::vector<Vector3<Real>> and std::vector<std::array<int32_t, 3>>
// - Uses GTE::ETManifoldMesh for topology operations
// - Removed Geogram-specific Logger calls
//
// Original Geogram License (BSD 3-Clause):
// [Same license text as in MeshRepair.h]

#pragma once

#include <Mathematics/Vector3.h>
#include <Mathematics/ETManifoldMesh.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <vector>

// The MeshPreprocessing class provides mesh preprocessing operations ported
// from Geogram, including:
// - Removal of small facets
// - Removal of small connected components
// - Normal orientation

namespace gte
{
    template <typename Real>
    class MeshPreprocessing
    {
    public:
        // Remove facets with area smaller than threshold.
        static void RemoveSmallFacets(
            std::vector<Vector3<Real>>& vertices,
            std::vector<std::array<int32_t, 3>>& triangles,
            Real minFacetArea)
        {
            if (minFacetArea <= static_cast<Real>(0))
            {
                return;
            }

            std::vector<std::array<int32_t, 3>> newTriangles;
            newTriangles.reserve(triangles.size());

            for (auto const& tri : triangles)
            {
                Real area = ComputeTriangleArea(vertices, tri);
                if (area >= minFacetArea)
                {
                    newTriangles.push_back(tri);
                }
            }

            triangles = std::move(newTriangles);
        }

        // Remove connected components with area or facet count below thresholds.
        static void RemoveSmallComponents(
            std::vector<Vector3<Real>>& vertices,
            std::vector<std::array<int32_t, 3>>& triangles,
            Real minComponentArea,
            size_t minComponentFacets = 0)
        {
            if (triangles.empty())
            {
                return;
            }

            // Step 1: Find connected components
            std::vector<int32_t> componentIds;
            int32_t numComponents = GetConnectedComponents(triangles, componentIds);

            if (numComponents <= 0)
            {
                return;
            }

            // Step 2: Compute area and facet count for each component
            std::vector<Real> componentArea(numComponents, static_cast<Real>(0));
            std::vector<size_t> componentFacets(numComponents, 0);

            for (size_t i = 0; i < triangles.size(); ++i)
            {
                int32_t compId = componentIds[i];
                componentArea[compId] += ComputeTriangleArea(vertices, triangles[i]);
                ++componentFacets[compId];
            }

            // Step 3: Mark components for removal
            std::vector<bool> removeComponent(numComponents, false);
            for (int32_t c = 0; c < numComponents; ++c)
            {
                if (componentArea[c] < minComponentArea || 
                    componentFacets[c] < minComponentFacets)
                {
                    removeComponent[c] = true;
                }
            }

            // Step 4: Filter triangles
            std::vector<std::array<int32_t, 3>> newTriangles;
            newTriangles.reserve(triangles.size());

            for (size_t i = 0; i < triangles.size(); ++i)
            {
                if (!removeComponent[componentIds[i]])
                {
                    newTriangles.push_back(triangles[i]);
                }
            }

            triangles = std::move(newTriangles);

            // Step 5: Remove isolated vertices (vertices no longer referenced by
            // any triangle).  This matches the behaviour of Geogram's
            // remove_small_connected_components, which also compacts the vertex
            // array after removing facets.
            int32_t numVertices = static_cast<int32_t>(vertices.size());
            std::vector<bool> referenced(numVertices, false);
            for (auto const& tri : triangles)
            {
                referenced[tri[0]] = true;
                referenced[tri[1]] = true;
                referenced[tri[2]] = true;
            }

            std::vector<int32_t> oldToNew(numVertices, -1);
            std::vector<Vector3<Real>> newVertices;
            newVertices.reserve(numVertices);

            for (int32_t i = 0; i < numVertices; ++i)
            {
                if (referenced[i])
                {
                    oldToNew[i] = static_cast<int32_t>(newVertices.size());
                    newVertices.push_back(vertices[i]);
                }
            }

            for (auto& tri : triangles)
            {
                tri[0] = oldToNew[tri[0]];
                tri[1] = oldToNew[tri[1]];
                tri[2] = oldToNew[tri[2]];
            }

            vertices = std::move(newVertices);
        }

        // Orient normals consistently within each connected component.
        static void OrientNormals(
            std::vector<Vector3<Real>> const& vertices,
            std::vector<std::array<int32_t, 3>>& triangles)
        {
            if (triangles.empty())
            {
                return;
            }

            // Build adjacency information
            std::map<std::pair<int32_t, int32_t>, std::vector<size_t>> edgeToTriangles;

            for (size_t i = 0; i < triangles.size(); ++i)
            {
                auto const& tri = triangles[i];
                for (int j = 0; j < 3; ++j)
                {
                    int32_t v0 = tri[j];
                    int32_t v1 = tri[(j + 1) % 3];
                    auto edge = std::make_pair(std::min(v0, v1), std::max(v0, v1));
                    edgeToTriangles[edge].push_back(i);
                }
            }

            // Process each connected component
            std::vector<int32_t> componentIds;
            int32_t numComponents = GetConnectedComponents(triangles, componentIds);

            for (int32_t compId = 0; compId < numComponents; ++compId)
            {
                // Determine if component should be flipped based on signed volume
                Real signedVolume = static_cast<Real>(0);
                for (size_t i = 0; i < triangles.size(); ++i)
                {
                    if (componentIds[i] == compId)
                    {
                        signedVolume += ComputeSignedVolume(vertices, triangles[i]);
                    }
                }

                // If signed volume is negative, flip all triangles in component
                if (signedVolume < static_cast<Real>(0))
                {
                    for (size_t i = 0; i < triangles.size(); ++i)
                    {
                        if (componentIds[i] == compId)
                        {
                            std::swap(triangles[i][1], triangles[i][2]);
                        }
                    }
                }
            }
        }

        // Invert all normals in the mesh.
        static void InvertNormals(
            std::vector<std::array<int32_t, 3>>& triangles)
        {
            for (auto& tri : triangles)
            {
                std::swap(tri[1], tri[2]);
            }
        }

    private:
        // Compute area of a triangle.
        static Real ComputeTriangleArea(
            std::vector<Vector3<Real>> const& vertices,
            std::array<int32_t, 3> const& tri)
        {
            Vector3<Real> const& p0 = vertices[tri[0]];
            Vector3<Real> const& p1 = vertices[tri[1]];
            Vector3<Real> const& p2 = vertices[tri[2]];

            Vector3<Real> edge1 = p1 - p0;
            Vector3<Real> edge2 = p2 - p0;
            Vector3<Real> normal = Cross(edge1, edge2);

            return Length(normal) * static_cast<Real>(0.5);
        }

        // Compute signed volume of pyramid from origin to triangle.
        // Ported from Geogram's signed_volume function.
        static Real ComputeSignedVolume(
            std::vector<Vector3<Real>> const& vertices,
            std::array<int32_t, 3> const& tri)
        {
            Vector3<Real> const& p0 = vertices[tri[0]];
            Vector3<Real> const& p1 = vertices[tri[1]];
            Vector3<Real> const& p2 = vertices[tri[2]];

            // Signed volume = dot(p0, cross(p1, p2)) / 6
            return Dot(p0, Cross(p1, p2)) / static_cast<Real>(6);
        }

        // Find connected components using triangle adjacency.
        // Returns number of components and fills componentIds vector.
        static int32_t GetConnectedComponents(
            std::vector<std::array<int32_t, 3>> const& triangles,
            std::vector<int32_t>& componentIds)
        {
            size_t numTriangles = triangles.size();
            componentIds.resize(numTriangles);
            std::fill(componentIds.begin(), componentIds.end(), -1);

            // Build edge-to-triangle adjacency
            std::map<std::pair<int32_t, int32_t>, std::vector<size_t>> edgeToTriangles;

            for (size_t i = 0; i < numTriangles; ++i)
            {
                auto const& tri = triangles[i];
                for (int j = 0; j < 3; ++j)
                {
                    int32_t v0 = tri[j];
                    int32_t v1 = tri[(j + 1) % 3];
                    auto edge = std::make_pair(std::min(v0, v1), std::max(v0, v1));
                    edgeToTriangles[edge].push_back(i);
                }
            }

            // Flood fill to assign component IDs
            int32_t currentComponent = 0;
            for (size_t seed = 0; seed < numTriangles; ++seed)
            {
                if (componentIds[seed] >= 0)
                {
                    continue; // Already assigned
                }

                // BFS to mark all connected triangles
                std::queue<size_t> queue;
                queue.push(seed);
                componentIds[seed] = currentComponent;

                while (!queue.empty())
                {
                    size_t current = queue.front();
                    queue.pop();

                    auto const& tri = triangles[current];

                    // Check all edges of current triangle
                    for (int j = 0; j < 3; ++j)
                    {
                        int32_t v0 = tri[j];
                        int32_t v1 = tri[(j + 1) % 3];
                        auto edge = std::make_pair(std::min(v0, v1), std::max(v0, v1));

                        // Find adjacent triangles through this edge
                        auto iter = edgeToTriangles.find(edge);
                        if (iter != edgeToTriangles.end())
                        {
                            for (size_t neighbor : iter->second)
                            {
                                if (neighbor != current && componentIds[neighbor] < 0)
                                {
                                    componentIds[neighbor] = currentComponent;
                                    queue.push(neighbor);
                                }
                            }
                        }
                    }
                }

                ++currentComponent;
            }

            return currentComponent;
        }
    };
}
