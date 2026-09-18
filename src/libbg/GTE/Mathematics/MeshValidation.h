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
#include <Mathematics/IntrTriangle3Triangle3.h>
#include <Mathematics/DistTriangle3Triangle3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
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
	    return FindSelfIntersections(vertices, triangles, true) > 0;
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

	static constexpr size_t invalidNode = std::numeric_limits<size_t>::max();
	// Small leaves limit pair tests without creating a node per triangle.
	static constexpr size_t leafTriangleCount = 4;
	// Thin facets can amplify FIQuery's shared-vertex drift by over 100 ulps.
	static constexpr Real contactToleranceFactor = static_cast<Real>(256);
	// Distance-query contacts need only a much smaller roundoff allowance.
	static constexpr Real unconnectedContactToleranceFactor = static_cast<Real>(8);

	struct TriangleBounds
	{
	    Vector3<Real> min;
	    Vector3<Real> max;
	    Vector3<Real> center;
	};

	struct TreeNode
	{
	    Vector3<Real> min;
	    Vector3<Real> max;
	    size_t begin = 0;
	    size_t end = 0;
	    size_t left = invalidNode;
	    size_t right = invalidNode;
	};

	static bool BoxesOverlap(Vector3<Real> const& min0, Vector3<Real> const& max0,
		Vector3<Real> const& min1, Vector3<Real> const& max1)
	{
	    for (int axis = 0; axis < 3; ++axis)
	    {
		if (max0[axis] < min1[axis] || max1[axis] < min0[axis])
		{
		    return false;
		}
	    }
	    return true;
	}

	static void ExpandBounds(Vector3<Real>& min, Vector3<Real>& max,
		Vector3<Real> const& otherMin, Vector3<Real> const& otherMax)
	{
	    for (int axis = 0; axis < 3; ++axis)
	    {
		min[axis] = std::min(min[axis], otherMin[axis]);
		max[axis] = std::max(max[axis], otherMax[axis]);
	    }
	}

	static size_t BuildTree(std::vector<TriangleBounds> const& bounds,
		std::vector<size_t>& order, std::vector<TreeNode>& nodes,
		size_t begin, size_t end)
	{
	    size_t nodeIndex = nodes.size();
	    nodes.emplace_back();
	    nodes[nodeIndex].begin = begin;
	    nodes[nodeIndex].end = end;

	    if (end - begin <= leafTriangleCount)
	    {
		auto const& first = bounds[order[begin]];
		nodes[nodeIndex].min = first.min;
		nodes[nodeIndex].max = first.max;
		for (size_t i = begin + 1; i < end; ++i)
		{
		    auto const& box = bounds[order[i]];
		    ExpandBounds(nodes[nodeIndex].min, nodes[nodeIndex].max,
			box.min, box.max);
		}
		return nodeIndex;
	    }

	    Vector3<Real> centerMin = bounds[order[begin]].center;
	    Vector3<Real> centerMax = centerMin;
	    for (size_t i = begin + 1; i < end; ++i)
	    {
		auto const& center = bounds[order[i]].center;
		ExpandBounds(centerMin, centerMax, center, center);
	    }
	    int axis = 0;
	    for (int i = 1; i < 3; ++i)
	    {
		if (centerMax[i] - centerMin[i] > centerMax[axis] - centerMin[axis])
		{
		    axis = i;
		}
	    }

	    size_t middle = begin + (end - begin) / 2;
	    std::nth_element(order.begin() + begin, order.begin() + middle,
		order.begin() + end, [&bounds, axis](size_t a, size_t b) {
		    return bounds[a].center[axis] < bounds[b].center[axis];
		});

	    size_t left = BuildTree(bounds, order, nodes, begin, middle);
	    size_t right = BuildTree(bounds, order, nodes, middle, end);
	    nodes[nodeIndex].left = left;
	    nodes[nodeIndex].right = right;
	    nodes[nodeIndex].min = nodes[left].min;
	    nodes[nodeIndex].max = nodes[left].max;
	    ExpandBounds(nodes[nodeIndex].min, nodes[nodeIndex].max,
		nodes[right].min, nodes[right].max);
	    return nodeIndex;
	}

	static bool IntersectBeyondSharedFeature(
		std::vector<Vector3<Real>> const& vertices,
		std::vector<std::array<int32_t, 3>> const& triangles,
		size_t i, size_t j)
	{
	    auto const& indices0 = triangles[i];
	    auto const& indices1 = triangles[j];
	    std::array<int32_t, 3> shared{};
	    size_t sharedCount = 0;
	    for (int32_t index0 : indices0)
	    {
		for (int32_t index1 : indices1)
		{
		    if (index0 == index1)
		    {
			shared[sharedCount++] = index0;
			break;
		    }
		}
	    }
	    if (sharedCount == 3)
	    {
		return true; // Coincident faces overlap, regardless of winding.
	    }
	    if (sharedCount == 2)
	    {
		// Two planes through the same edge meet only on that edge unless
		// they are coplanar.  In the coplanar case, the third vertices
		// must be on opposite sides of the edge to avoid overlap.
		auto const& a = vertices[shared[0]];
		Vector3<Real> edge = vertices[shared[1]] - a;
		int32_t third0 = -1;
		int32_t third1 = -1;
		for (int32_t index : indices0)
		{
		    if (index != shared[0] && index != shared[1]) third0 = index;
		}
		for (int32_t index : indices1)
		{
		    if (index != shared[0] && index != shared[1]) third1 = index;
		}
		if (third0 < 0 || third1 < 0)
		{
		    return true;
		}
		Vector3<Real> side0 = Cross(edge, vertices[third0] - a);
		Vector3<Real> side1 = Cross(edge, vertices[third1] - a);
		Vector3<Real> planeDifference = Cross(side0, side1);
		return Dot(planeDifference, planeDifference) <= static_cast<Real>(0) &&
		    Dot(side0, side1) > static_cast<Real>(0);
	    }

	    Triangle3<Real> triangle0(vertices[indices0[0]], vertices[indices0[1]],
		vertices[indices0[2]]);
	    Triangle3<Real> triangle1(vertices[indices1[0]], vertices[indices1[1]],
		vertices[indices1[2]]);
	    if (sharedCount == 0)
	    {
		// The separating-axis test can report contact for near-coplanar
		// disjoint faces.  Confirm positives with the intersection query.
		TIQuery<Real, Triangle3<Real>, Triangle3<Real>> test;
		if (!test(triangle0, triangle1).intersect)
		{
		    return false;
		}
		FIQuery<Real, Triangle3<Real>, Triangle3<Real>> find;
		if (find(triangle0, triangle1).intersect)
		{
		    return true;
		}
		// FIQuery can omit isolated boundary contact.  The distance query
		// distinguishes that from the separating-axis false positives.
		DCPQuery<Real, Triangle3<Real>, Triangle3<Real>> distance;
		Real scale = static_cast<Real>(0);
		for (auto const& triangle : {triangle0, triangle1})
		{
		    for (auto const& point : triangle.v)
		    {
			for (int axis = 0; axis < 3; ++axis)
			{
			    scale = std::max(scale, std::abs(point[axis]));
			}
		    }
		}
		Real tolerance = unconnectedContactToleranceFactor *
		    std::numeric_limits<Real>::epsilon() * scale;
		return distance(triangle0, triangle1).sqrDistance <= tolerance * tolerance;
	    }

	    FIQuery<Real, Triangle3<Real>, Triangle3<Real>> query;
	    auto result = query(triangle0, triangle1);
	    if (!result.intersect)
	    {
		return false;
	    }
	    if (result.intersection.empty())
	    {
		return true;
	    }
	    auto const& vertex = vertices[shared[0]];
	    Real scale = static_cast<Real>(0);
	    for (auto const& indices : {indices0, indices1})
	    {
		for (int32_t index : indices)
		{
		    for (int axis = 0; axis < 3; ++axis)
		    {
			scale = std::max(scale, std::abs(vertices[index][axis]));
			scale = std::max(scale, std::abs(vertices[index][axis] - vertex[axis]));
		    }
		}
	    }
	    // GTE's computed contact can drift from a shared vertex on thin
	    // triangles or when their extent greatly exceeds its coordinates.
	    Real tolerance = contactToleranceFactor *
		std::numeric_limits<Real>::epsilon() * scale;
	    for (auto const& point : result.intersection)
	    {
		Vector3<Real> offset = point - vertex;
		if (Dot(offset, offset) > tolerance * tolerance)
		{
		    return true;
		}
	    }
	    return false;
	}

	static size_t CountNodePairs(
		std::vector<Vector3<Real>> const& vertices,
		std::vector<std::array<int32_t, 3>> const& triangles,
		std::vector<TriangleBounds> const& bounds,
		std::vector<size_t> const& order,
		std::vector<TreeNode> const& nodes,
		size_t a, size_t b, bool stopAtFirst)
	{
	    auto const& nodeA = nodes[a];
	    auto const& nodeB = nodes[b];
	    if (!BoxesOverlap(nodeA.min, nodeA.max, nodeB.min, nodeB.max))
	    {
		return 0;
	    }

	    bool leafA = nodeA.left == invalidNode;
	    bool leafB = nodeB.left == invalidNode;
	    if (leafA && leafB)
	    {
		size_t count = 0;
		for (size_t ia = nodeA.begin; ia < nodeA.end; ++ia)
		{
		    size_t i = order[ia];
		    for (size_t ib = (a == b ? ia + 1 : nodeB.begin);
			ib < nodeB.end; ++ib)
		    {
			size_t j = order[ib];
			if (BoxesOverlap(bounds[i].min, bounds[i].max,
				bounds[j].min, bounds[j].max) &&
			    IntersectBeyondSharedFeature(vertices, triangles, i, j))
			{
			    ++count;
			    if (stopAtFirst)
			    {
				return count;
			    }
			}
		    }
		}
		return count;
	    }

	    if (a == b)
	    {
		size_t count = CountNodePairs(vertices, triangles, bounds, order,
		    nodes, nodeA.left, nodeA.left, stopAtFirst);
		if (stopAtFirst && count)
		{
		    return count;
		}
		count += CountNodePairs(vertices, triangles, bounds, order,
		    nodes, nodeA.left, nodeA.right, stopAtFirst);
		if (stopAtFirst && count)
		{
		    return count;
		}
		return count + CountNodePairs(vertices, triangles, bounds, order,
		    nodes, nodeA.right, nodeA.right, stopAtFirst);
	    }

	    if (!leafA && (leafB || nodeA.end - nodeA.begin >= nodeB.end - nodeB.begin))
	    {
		size_t count = CountNodePairs(vertices, triangles, bounds, order,
		    nodes, nodeA.left, b, stopAtFirst);
		if (stopAtFirst && count)
		{
		    return count;
		}
		return count + CountNodePairs(vertices, triangles, bounds, order,
		    nodes, nodeA.right, b, stopAtFirst);
	    }

	    size_t count = CountNodePairs(vertices, triangles, bounds, order,
		nodes, a, nodeB.left, stopAtFirst);
	    if (stopAtFirst && count)
	    {
		return count;
	    }
	    return count + CountNodePairs(vertices, triangles, bounds, order,
		nodes, a, nodeB.right, stopAtFirst);
	}

	static size_t FindSelfIntersections(
		std::vector<Vector3<Real>> const& vertices,
		std::vector<std::array<int32_t, 3>> const& triangles,
		bool stopAtFirst)
	{
	    if (triangles.size() < 2)
	    {
		return 0;
	    }

	    std::vector<TriangleBounds> bounds(triangles.size());
	    for (size_t i = 0; i < triangles.size(); ++i)
	    {
		auto const& tri = triangles[i];
		TriangleBounds& box = bounds[i];
		box.min = vertices[tri[0]];
		box.max = box.min;
		for (size_t j = 1; j < 3; ++j)
		{
		    ExpandBounds(box.min, box.max, vertices[tri[j]], vertices[tri[j]]);
		}
		box.center = box.min / static_cast<Real>(2) +
		    box.max / static_cast<Real>(2);
	    }

	    std::vector<size_t> order(triangles.size());
	    std::iota(order.begin(), order.end(), 0);
	    std::vector<TreeNode> nodes;
	    nodes.reserve(triangles.size());
	    BuildTree(bounds, order, nodes, 0, triangles.size());
	    return CountNodePairs(vertices, triangles, bounds, order, nodes,
		0, 0, stopAtFirst);
	}

	static void CheckSelfIntersections(
		std::vector<Vector3<Real>> const& vertices,
		std::vector<std::array<int32_t, 3>> const& triangles,
		ValidationResult& result)
	{
	    result.intersectingTrianglePairs = FindSelfIntersections(vertices, triangles, false);
	    result.hasSelfIntersections = result.intersectingTrianglePairs > 0;
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
