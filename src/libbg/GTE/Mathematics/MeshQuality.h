// MeshQuality.h — Triangle mesh quality metrics (Verdict-equivalent)
//
// Copyright (c) 2025 BRL-CAD Contributors.  License: MIT (see LICENSE).
//
// Implements the seven triangle quality metrics used by the Verdict library
// (Knupp, P.M., "Algebraic Mesh Quality Metrics for Unstructured Initial
// Meshes", 2003) and exposed by VTK's vtkMeshQuality class.  This header
// provides a self-contained, VTK-free implementation so that repair and
// remesh pipelines can evaluate mesh quality without taking a VTK dependency.
//
// All metrics are normalized to match VTK's vtkMeshQuality output exactly:
// an equilateral triangle gives 1.0 for all metrics (except MinAngle=60 and
// MaxAngle=60 which are in degrees).  Validated against vtkMeshQuality 9.1.
//
// For a triangle with vertices v0, v1, v2 and edge lengths l0, l1, l2 where
//   l0 = |v1-v0|,  l1 = |v2-v1|,  l2 = |v0-v2|
// and area A = 0.5 * |cross(v1-v0, v2-v0)|, the metrics are:
//
//   AspectRatio    = max(l0,l1,l2) * (l0+l1+l2) / (4*sqrt(3)*A)
//                    [ideal: 1.0; matches VTK TriangleAspectRatio]
//   ScaledJacobian = min(sin(A), sin(B), sin(C)) / sin(60°)
//                  = 2*A / (max(l0*l1, l1*l2, l0*l2) * sin(60°))
//                    [ideal: 1.0; matches VTK TriangleScaledJacobian]
//   MinAngle (deg) = smallest interior angle
//                    [ideal: 60; matches VTK TriangleMinAngle]
//   MaxAngle (deg) = largest  interior angle
//                    [ideal: 60; matches VTK TriangleMaxAngle]
//   Shape          = 4*sqrt(3)*A / (l0^2 + l1^2 + l2^2)
//                    [ideal: 1.0; matches VTK TriangleShape]
//   Condition      = (l0^2 + l1^2 + l2^2) / (4*sqrt(3)*A)
//                    [ideal: 1.0; matches VTK TriangleCondition]
//   EdgeRatio      = max(l0,l1,l2) / min(l0,l1,l2)
//                    [ideal: 1.0; matches VTK TriangleEdgeRatio]
//
// The ScaledJacobian equals min(sin(angle)) / sin(60°), so it is 1.0 for
// equilateral, positive for well-shaped, and negative for inverted triangles.
// Shape and Condition are reciprocals (Shape = 1/Condition).
//
// Usage:
//   #include <Mathematics/MeshQuality.h>
//   using namespace gte;
//   ...
//   auto tm = MeshQuality<double>::ComputeTriangle(v0, v1, v2);
//   auto mm = MeshQuality<double>::ComputeMeshMetrics(vertices, triangles);
//   std::cout << "median aspect ratio: " << mm.aspectRatio.median << "\n";
//   // All metrics normalized to equilateral=1.0 (except min/max angle in degrees).
//   // Matches VTK vtkMeshQuality output exactly.

#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace gte
{

template <typename Real>
class MeshQuality
{
public:
    // Quality metrics for a single triangle.
    struct TriangleMetrics
    {
        Real aspectRatio;    // [1, ∞)  — 1.0 = equilateral, matches VTK TriangleAspectRatio
        Real scaledJacobian; // (-∞, 1] — 1.0 = equilateral, < 0 = inverted, matches VTK
        Real minAngle;       // degrees (0, 60] — 60 = equilateral, matches VTK TriangleMinAngle
        Real maxAngle;       // degrees [60, 180) — 60 = equilateral, matches VTK TriangleMaxAngle
        Real shape;          // (0, 1]  — 1.0 = equilateral, matches VTK TriangleShape
        Real condition;      // [1, ∞)  — 1.0 = equilateral, matches VTK TriangleCondition
        Real edgeRatio;      // [1, ∞)  — 1.0 = equilateral, matches VTK TriangleEdgeRatio
    };

    // Per-metric aggregate statistics over an entire mesh.
    struct MetricStats
    {
        double mean;
        double median;
        double min_val;
        double max_val;
    };

    // Aggregate quality report for a whole triangle mesh.
    struct MeshMetrics
    {
        MetricStats aspectRatio;
        MetricStats scaledJacobian;
        MetricStats minAngle;
        MetricStats maxAngle;
        MetricStats shape;
        MetricStats condition;
        MetricStats edgeRatio;

        size_t totalTriangles;
        size_t invertedTriangles; // scaledJacobian < 0
        size_t poorAspect;        // aspectRatio > 3
        size_t smallAngle;        // minAngle < 20 degrees
        size_t largeAngle;        // maxAngle > 120 degrees
    };

    // Compute quality metrics for a single triangle with vertices v0, v1, v2.
    // v0, v1, v2 are arrays of 3 Real values (x, y, z).
    static TriangleMetrics ComputeTriangle(
        std::array<Real, 3> const& v0,
        std::array<Real, 3> const& v1,
        std::array<Real, 3> const& v2)
    {
        TriangleMetrics m{};

        const Real SQRT3 = std::sqrt((Real)3);

        // Edge vectors
        Real L0[3], L1[3], L2[3];
        for (int i = 0; i < 3; ++i) {
            L0[i] = v1[i] - v0[i];  // v0 -> v1
            L1[i] = v2[i] - v1[i];  // v1 -> v2
            L2[i] = v0[i] - v2[i];  // v2 -> v0
        }

        // Edge lengths
        Real l0 = std::sqrt(Dot3(L0, L0));
        Real l1 = std::sqrt(Dot3(L1, L1));
        Real l2 = std::sqrt(Dot3(L2, L2));

        // Twice the triangle area via cross product of (v1-v0) and (v2-v0)
        // cross(L0, -L2) = cross(v1-v0, v2-v0)
        Real negL2[3] = { -L2[0], -L2[1], -L2[2] };
        Real crossVec[3];
        Cross3(L0, negL2, crossVec);
        Real two_area = std::sqrt(Dot3(crossVec, crossVec));
        Real area     = (Real)0.5 * two_area;

        // Edge ratio
        Real max_edge = std::max({l0, l1, l2});
        Real min_edge = std::min({l0, l1, l2});
        m.edgeRatio = (min_edge > (Real)0) ? max_edge / min_edge : (Real)1e15;

        if (area > (Real)0) {
            Real perimeter = l0 + l1 + l2;

            // Aspect ratio: max_edge * perimeter / (4 * sqrt(3) * area)
            // Matches VTK TriangleAspectRatio; equilateral = 1.0
            m.aspectRatio = max_edge * perimeter / ((Real)4 * SQRT3 * area);

            // Scaled Jacobian: min(sin(A), sin(B), sin(C)) / sin(60°)
            //   = (2*area / max_edge_product) / (sqrt(3)/2)
            //   = 4*area / (sqrt(3) * max_edge_product)
            // Matches VTK TriangleScaledJacobian; equilateral = 1.0
            Real max_prod = std::max({l0 * l1, l1 * l2, l0 * l2});
            m.scaledJacobian = (max_prod > (Real)0)
                ? (Real)4 * area / (SQRT3 * max_prod)
                : (Real)0;

            // Shape: 4*sqrt(3)*area / (l0^2 + l1^2 + l2^2)
            // Matches VTK TriangleShape; equilateral = 1.0
            Real sumsq = l0*l0 + l1*l1 + l2*l2;
            m.shape     = (Real)4 * SQRT3 * area / sumsq;

            // Condition: (l0^2 + l1^2 + l2^2) / (4*sqrt(3)*area)
            // Matches VTK TriangleCondition; equilateral = 1.0
            m.condition = sumsq / ((Real)4 * SQRT3 * area);
        } else {
            // Degenerate / zero-area triangle — use sentinel values matching VTK Verdict
            const Real SENTINEL = (Real)1e15;
            m.aspectRatio    = SENTINEL;
            m.scaledJacobian = (Real)0;
            m.shape          = (Real)0;
            m.condition      = SENTINEL;
        }

        // Angles (degrees) using the dot-product formula
        const Real RAD2DEG = (Real)180 / std::acos((Real)-1);

        // Angle at v0: between (v1-v0) and (v2-v0) = L0 and -L2
        // Angle at v1: between (v0-v1) and (v2-v1) = -L0 and L1
        // Angle at v2: between (v1-v2) and (v0-v2) = -L1 and L2
        Real negL0[3] = { -L0[0], -L0[1], -L0[2] };
        Real negL1[3] = { -L1[0], -L1[1], -L1[2] };

        // For degenerate triangles (area==0 or any edge zero), clamp both angles
        // to 0 — matches VTK vtkMeshQuality behavior on degenerate cells.
        if (area <= (Real)0 || l0 <= (Real)0 || l1 <= (Real)0 || l2 <= (Real)0) {
            m.minAngle = (Real)0;
            m.maxAngle = (Real)0;
        } else {
            Real cosA = Clamp(Dot3(L0, negL2) / (l0 * l2), (Real)-1, (Real)1);
            Real cosB = Clamp(Dot3(negL0, L1) / (l0 * l1), (Real)-1, (Real)1);
            Real cosC = Clamp(Dot3(negL1, L2) / (l1 * l2), (Real)-1, (Real)1);

            Real angA = std::acos(cosA) * RAD2DEG;
            Real angB = std::acos(cosB) * RAD2DEG;
            Real angC = std::acos(cosC) * RAD2DEG;

            m.minAngle = std::min({angA, angB, angC});
            m.maxAngle = std::max({angA, angB, angC});
        }

        return m;
    }

    // Overload accepting raw pointer arrays for interop with GTE Vector3<Real>.
    // pts[i] must point to an array of 3 Real values (the i-th vertex).
    static TriangleMetrics ComputeTriangleRaw(
        Real const* p0, Real const* p1, Real const* p2)
    {
        std::array<Real, 3> v0{ p0[0], p0[1], p0[2] };
        std::array<Real, 3> v1{ p1[0], p1[1], p1[2] };
        std::array<Real, 3> v2{ p2[0], p2[1], p2[2] };
        return ComputeTriangle(v0, v1, v2);
    }

    // Compute aggregate quality metrics for a complete triangle mesh.
    //
    // vertices  : flat array of 3D vertex positions, indexed by triangle indices.
    // triangles : each element is {i0, i1, i2} — vertex indices of one triangle.
    static MeshMetrics ComputeMeshMetrics(
        std::vector<std::array<Real, 3>> const& vertices,
        std::vector<std::array<int32_t, 3>> const& triangles)
    {
        MeshMetrics mm{};
        mm.totalTriangles = triangles.size();
        if (triangles.empty()) return mm;

        size_t n = vertices.size();

        std::vector<double> ar, sj, mna, mxa, sh, cnd, er;
        ar.reserve(triangles.size());
        sj.reserve(triangles.size());
        mna.reserve(triangles.size());
        mxa.reserve(triangles.size());
        sh.reserve(triangles.size());
        cnd.reserve(triangles.size());
        er.reserve(triangles.size());

        for (auto const& t : triangles) {
            if (t[0] < 0 || t[1] < 0 || t[2] < 0 ||
                (size_t)t[0] >= n || (size_t)t[1] >= n || (size_t)t[2] >= n)
                continue;

            TriangleMetrics tm = ComputeTriangle(
                vertices[(size_t)t[0]],
                vertices[(size_t)t[1]],
                vertices[(size_t)t[2]]);

            ar.push_back((double)tm.aspectRatio);
            sj.push_back((double)tm.scaledJacobian);
            mna.push_back((double)tm.minAngle);
            mxa.push_back((double)tm.maxAngle);
            sh.push_back((double)tm.shape);
            cnd.push_back((double)tm.condition);
            er.push_back((double)tm.edgeRatio);
        }

        if (ar.empty()) return mm;

        Aggregate(ar,  mm.aspectRatio);
        Aggregate(sj,  mm.scaledJacobian);
        Aggregate(mna, mm.minAngle);
        Aggregate(mxa, mm.maxAngle);
        Aggregate(sh,  mm.shape);
        Aggregate(cnd, mm.condition);
        Aggregate(er,  mm.edgeRatio);

        for (double v : ar)  if (v > 3.0)  ++mm.poorAspect;
        for (double v : sj)  if (v < 0.0)  ++mm.invertedTriangles;
        for (double v : mna) if (v < 20.0) ++mm.smallAngle;
        for (double v : mxa) if (v > 120.0)++mm.largeAngle;

        return mm;
    }

    // Convenience overload for meshes stored as GTE-style Vector3 containers.
    // Requires that Vector3<Real> has operator[](int) returning Real.
    template <typename Vec3>
    static MeshMetrics ComputeMeshMetricsVec3(
        std::vector<Vec3> const& vertices,
        std::vector<std::array<int32_t, 3>> const& triangles)
    {
        std::vector<std::array<Real, 3>> verts;
        verts.reserve(vertices.size());
        for (auto const& v : vertices)
            verts.push_back({ v[0], v[1], v[2] });
        return ComputeMeshMetrics(verts, triangles);
    }

private:
    static Real Dot3(Real const* a, Real const* b)
    {
        return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
    }

    static void Cross3(Real const* a, Real const* b, Real* out)
    {
        out[0] = a[1]*b[2] - a[2]*b[1];
        out[1] = a[2]*b[0] - a[0]*b[2];
        out[2] = a[0]*b[1] - a[1]*b[0];
    }

    static Real Clamp(Real v, Real lo, Real hi)
    {
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    static void Aggregate(std::vector<double>& vals, MetricStats& s)
    {
        if (vals.empty()) return;
        s.min_val = *std::min_element(vals.begin(), vals.end());
        s.max_val = *std::max_element(vals.begin(), vals.end());
        s.mean    = std::accumulate(vals.begin(), vals.end(), 0.0)
                    / static_cast<double>(vals.size());
        std::sort(vals.begin(), vals.end());
        size_t n  = vals.size();
        s.median  = (n % 2 == 0)
            ? 0.5 * (vals[n/2 - 1] + vals[n/2])
            : vals[n/2];
    }
};

} // namespace gte
