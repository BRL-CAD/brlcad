/*                M E S H V A L I D A T I O N . H
 * BRL-CAD
 *
 * Published in 2026 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file MeshValidation.h
 *
 * Mesh validation utilities for verifying manifold property and detecting self-intersections
 */

#pragma once

#include <Mathematics/Vector3.h>
#include <Mathematics/ETManifoldMesh.h>
#include <Mathematics/ETNonmanifoldMesh.h>
#include <Mathematics/IntrTriangle3Triangle3.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace gte
{
template <typename Real>
    class MeshValidation
{
    public:
	struct ValidationResult
	{
	    bool isValid;
	    bool isManifold;
	    bool hasSelfIntersections;
	    bool isOriented;
	    bool isClosed;

	    size_t nonManifoldEdges;
	    size_t boundaryEdges;
	    size_t intersectingTrianglePairs;

	    std::string errorMessage;

	    ValidationResult()
		: isValid(false)
		  , isManifold(false)
		  , hasSelfIntersections(false)
		  , isOriented(false)
		  , isClosed(false)
		  , nonManifoldEdges(0)
		  , boundaryEdges(0)
		  , intersectingTrianglePairs(0)
	    {
	    }
	};

	// Validate mesh for manifoldness and self-intersections
	static ValidationResult Validate(
		std::vector<Vector3<Real>> const& vertices,
		std::vector<std::array<int32_t, 3>> const& triangles,
		bool checkSelfIntersections = true)
	{
	    ValidationResult result;

	    // Check basic validity
	    if (vertices.empty() || triangles.empty())
	    {
		result.errorMessage = "Empty mesh";
		return result;
	    }

	    // Check vertex indices
	    for (auto const& tri : triangles)
	    {
		for (int i = 0; i < 3; ++i)
		{
		    if (tri[i] < 0 || tri[i] >= static_cast<int32_t>(vertices.size()))
		    {
			result.errorMessage = "Invalid vertex index in triangle";
			return result;
		    }
		}
	    }

	    // Check manifoldness using ETNonmanifoldMesh
	    CheckManifold(triangles, result);

	    // Check orientation if manifold
	    if (result.isManifold)
	    {
		CheckOrientation(triangles, result);
	    }

	    // Check for self-intersections if requested
	    if (checkSelfIntersections)
	    {
		CheckSelfIntersections(vertices, triangles, result);
	    }

	    // Overall validity
	    result.isValid = result.isManifold && !result.hasSelfIntersections;

	    if (result.isValid)
	    {
		result.errorMessage = "Mesh is valid";
	    }
	    else if (!result.isManifold)
	    {
		result.errorMessage = "Mesh is non-manifold";
	    }
	    else if (result.hasSelfIntersections)
	    {
		result.errorMessage = "Mesh has self-intersections";
	    }

	    return result;
	}

	// Quick check: is the mesh manifold?
	static bool IsManifold(std::vector<std::array<int32_t, 3>> const& triangles)
	{
	    ValidationResult result;
	    CheckManifold(triangles, result);
	    return result.isManifold;
	}

	// Quick check: does the mesh have self-intersections?
	static bool HasSelfIntersections(
		std::vector<Vector3<Real>> const& vertices,
		std::vector<std::array<int32_t, 3>> const& triangles)
	{
	    ValidationResult result;
	    CheckSelfIntersections(vertices, triangles, result);
	    return result.hasSelfIntersections;
	}

    private:
	// Check manifold property using edge analysis
	static void CheckManifold(
		std::vector<std::array<int32_t, 3>> const& triangles,
		ValidationResult& result)
	{
	    // Build edge map
	    std::map<std::pair<int32_t, int32_t>, size_t> edgeCount;

	    for (auto const& tri : triangles)
	    {
		for (int i = 0; i < 3; ++i)
		{
		    int32_t v0 = tri[i];
		    int32_t v1 = tri[(i + 1) % 3];

		    // Canonical edge (smaller index first)
		    auto edge = std::make_pair(std::min(v0, v1), std::max(v0, v1));
		    edgeCount[edge]++;
		}
	    }

	    // Check edge counts
	    result.nonManifoldEdges = 0;
	    result.boundaryEdges = 0;

	    for (auto const& edgePair : edgeCount)
	    {
		size_t count = edgePair.second;

		if (count == 1)
		{
		    result.boundaryEdges++;
		}
		else if (count > 2)
		{
		    result.nonManifoldEdges++;
		}
	    }

	    result.isManifold = (result.nonManifoldEdges == 0);
	    result.isClosed = (result.boundaryEdges == 0);
	}

	// Check orientation consistency
	static void CheckOrientation(
		std::vector<std::array<int32_t, 3>> const& triangles,
		ValidationResult& result)
	{
	    // Build directed edge map
	    std::map<std::pair<int32_t, int32_t>, int> directedEdgeCount;

	    for (auto const& tri : triangles)
	    {
		for (int i = 0; i < 3; ++i)
		{
		    int32_t v0 = tri[i];
		    int32_t v1 = tri[(i + 1) % 3];

		    auto edge = std::make_pair(v0, v1);
		    directedEdgeCount[edge]++;
		}
	    }

	    // For oriented manifold, each directed edge should appear at most once
	    bool oriented = true;
	    for (auto const& edgePair : directedEdgeCount)
	    {
		if (edgePair.second > 1)
		{
		    oriented = false;
		    break;
		}
	    }

	    result.isOriented = oriented;
	}

	// Check for self-intersecting triangles
	static void CheckSelfIntersections(
		std::vector<Vector3<Real>> const& vertices,
		std::vector<std::array<int32_t, 3>> const& triangles,
		ValidationResult& result)
	{
	    result.intersectingTrianglePairs = 0;

	    size_t n = triangles.size();

	    // Brute force check all pairs
	    // TODO: Could optimize with spatial data structure (octree, BVH) for large meshes
	    for (size_t i = 0; i < n; ++i)
	    {
		auto const& tri1 = triangles[i];
		Triangle3<Real> triangle1(
			vertices[tri1[0]],
			vertices[tri1[1]],
			vertices[tri1[2]]
			);

		for (size_t j = i + 1; j < n; ++j)
		{
		    auto const& tri2 = triangles[j];

		    // Skip if triangles share a vertex (adjacent triangles)
		    bool shareVertex = false;
		    for (int a = 0; a < 3; ++a)
		    {
			for (int b = 0; b < 3; ++b)
			{
			    if (tri1[a] == tri2[b])
			    {
				shareVertex = true;
				break;
			    }
			}
			if (shareVertex) break;
		    }

		    if (shareVertex)
		    {
			continue;  // Adjacent triangles can touch at edges/vertices
		    }

		    // Check for intersection
		    Triangle3<Real> triangle2(
			    vertices[tri2[0]],
			    vertices[tri2[1]],
			    vertices[tri2[2]]
			    );

		    FIQuery<Real, Triangle3<Real>, Triangle3<Real>> query;
		    auto queryResult = query(triangle1, triangle2);

		    if (queryResult.intersect)
		    {
			result.intersectingTrianglePairs++;
		    }
		}
	    }

	    result.hasSelfIntersections = (result.intersectingTrianglePairs > 0);
	}
};
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
