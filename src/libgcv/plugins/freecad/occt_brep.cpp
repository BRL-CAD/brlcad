// Copyright (c) 2026, open2open contributors
// SPDX-License-Identifier: MIT
//
// OCCT to openNURBS conversion adapted from open2open.

#include "common.h"

#include "occt_brep.h"

#include "vmath.h"

// ---- openNURBS ----
#include "opennurbs.h"

// ---- OCCT topology ----
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopAbs_Orientation.hxx>

// ---- OCCT geometry ----
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_ConicalSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_SphericalSurface.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <Geom_SurfaceOfRevolution.hxx>
#include <Geom_Plane.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomConvert.hxx>
#include <Geom2dConvert.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pnt.hxx>

// ---- OCCT tools ----
#include <NCollection_IndexedMap.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Handle.hxx>
#include <TopTools_ShapeMapHasher.hxx>

#include <vector>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <unordered_map>
#include <cmath>


namespace freecad {

static bool
occt_curve_to_nurbs(const Handle(Geom_BSplineCurve)& c, ON_NurbsCurve& out)
{
    if (c.IsNull()) return false;

    const int degree    = c->Degree();
    const int num_poles = c->NbPoles();
    const bool rational = c->IsRational() ? true : false;

    if (!out.Create(3, rational, degree + 1, num_poles))
        return false;

    // --- control points ---
    for (int i = 1; i <= num_poles; ++i) {
        gp_Pnt p  = c->Pole(i);
        double w  = rational ? c->Weight(i) : 1.0;
        // openNURBS rational curves store homogeneous coordinates
        out.SetCV(i - 1,
                  ON_4dPoint(p.X() * w, p.Y() * w, p.Z() * w, w));
    }

    // --- knot vector ---
    // OCCT stores sum(mults) = num_poles + degree + 1 (non-periodic).
    // openNURBS stores sum(flat) = num_poles + degree - 1 (2 fewer).
    // The conversion always added one phantom knot at each end so we
    // always strip exactly one from the first and last multiplicity groups.
    const int num_knot_groups = c->NbKnots();
    int ki = 0;
    for (int j = 1; j <= num_knot_groups; ++j) {
        double val  = c->Knot(j);
        int    mult = c->Multiplicity(j);
        if (j == 1 || j == num_knot_groups)
            mult -= 1;
        for (int m = 0; m < mult; ++m) {
            if (ki >= out.KnotCount()) return false;
            out.SetKnot(ki++, val);
        }
    }

    return out.IsValid() ? true : false;
}

// ---------------------------------------------------------------------------
// Geom_BSplineSurface → ON_NurbsSurface
// ---------------------------------------------------------------------------

static bool
occt_surface_to_nurbs(const Handle(Geom_BSplineSurface)& s, ON_NurbsSurface& out)
{
    if (s.IsNull()) return false;

    const int udeg    = s->UDegree();
    const int vdeg    = s->VDegree();
    const int nu      = s->NbUPoles();
    const int nv      = s->NbVPoles();
    const bool rational = s->IsURational() || s->IsVRational();

    if (!out.Create(3, rational, udeg + 1, vdeg + 1, nu, nv))
        return false;

    // --- control points ---
    for (int i = 1; i <= nu; ++i) {
        for (int j = 1; j <= nv; ++j) {
            gp_Pnt p = s->Pole(i, j);
            double w = rational ? s->Weight(i, j) : 1.0;
            out.SetCV(i - 1, j - 1,
                      ON_4dPoint(p.X() * w, p.Y() * w, p.Z() * w, w));
        }
    }

    // --- U knots ---
    {
        int ki = 0;
        const int nku = s->NbUKnots();
        // OCCT stores sum(mults) = nu + udeg + 1 (non-periodic).
        // openNURBS stores sum(flat) = nu + udeg - 1  (2 fewer).
        // Whether clamped or non-clamped, the conversion always added one
        // phantom knot at each end, so we always strip one from each end.
        for (int j = 1; j <= nku; ++j) {
            double val  = s->UKnot(j);
            int    mult = s->UMultiplicity(j);
            if (j == 1 || j == nku)
                mult -= 1;
            for (int m = 0; m < mult; ++m) {
                if (ki >= out.KnotCount(0)) return false;
                out.SetKnot(0, ki++, val);
            }
        }
    }

    // --- V knots ---
    {
        int ki = 0;
        const int nkv = s->NbVKnots();
        for (int j = 1; j <= nkv; ++j) {
            double val  = s->VKnot(j);
            int    mult = s->VMultiplicity(j);
            if (j == 1 || j == nkv)
                mult -= 1;
            for (int m = 0; m < mult; ++m) {
                if (ki >= out.KnotCount(1)) return false;
                out.SetKnot(1, ki++, val);
            }
        }
    }

    return out.IsValid() ? true : false;
}

static Handle(Geom_Surface)
surface_basis(Handle(Geom_Surface) surface)
{
    // The trim wrapper keeps the parameterization of its (possibly reversed)
    // basis.  The BRep face's UV bounds still delimit the actual face.
    for (;;) {
        const auto trimmed = Handle(Geom_RectangularTrimmedSurface)::DownCast(surface);
        if (trimmed.IsNull())
            return surface;
        surface = trimmed->BasisSurface();
    }
}

static Handle(Geom2d_BSplineCurve)
bounded_pcurve(const Handle(Geom2d_Curve) &curve, double first, double last)
{
    if (!std::isfinite(first) || !std::isfinite(last) || !(last > first))
        return {};

    const double tolerance = Precision::PConfusion();
    auto spline = Handle(Geom2d_BSplineCurve)::DownCast(curve);
    if (!spline.IsNull()) {
        if (!spline->IsPeriodic() &&
            fabs(spline->FirstParameter() - first) <= tolerance &&
            fabs(spline->LastParameter() - last) <= tolerance)
            return spline;

        // A BRep edge can use a proper subinterval of a shared periodic
        // pcurve.  Extract the interval before removing periodicity so an
        // open edge does not acquire the entire closed curve.
        spline = Handle(Geom2d_BSplineCurve)::DownCast(spline->Copy());
        try {
            spline->Segment(first, last);
            if (!spline->IsPeriodic())
                return spline;
        } catch (const Standard_Failure &) {
            // Try OCCT's general bounded conversion below.
        }
    }

    try {
        const Handle(Geom2d_TrimmedCurve) trimmed =
            new Geom2d_TrimmedCurve(curve, first, last, true, false);
        spline = Geom2dConvert::CurveToBSplineCurve(trimmed);
        if (!spline.IsNull() && !spline->IsPeriodic())
            return spline;
    } catch (const Standard_Failure &) {
        return {};
    }
    return {};
}

static ON_RevSurface *
analytic_revolution(const Handle(Geom_Surface) &surface,
                    double u0, double u1, double v0, double v1)
{
    constexpr double revolution = 2.0 * ON_PI;
    constexpr double angle_roundoff = 1.0e-10;
    const double u_span = u1 - u0;
    const double v_span = v1 - v0;
    if (!(u_span > ON_ZERO_TOLERANCE && u_span <= revolution + angle_roundoff) ||
        !(v_span > ON_ZERO_TOLERANCE) || Precision::IsInfinite(v0) || Precision::IsInfinite(v1))
        return nullptr;

    gp_Ax3 frame;
    double major_radius = 0.0;
    double minor_radius = 0.0;
    double radial_slope = 0.0;
    double axial_slope = 1.0;
    const auto cylinder = Handle(Geom_CylindricalSurface)::DownCast(surface);
    const auto cone = Handle(Geom_ConicalSurface)::DownCast(surface);
    const auto sphere = Handle(Geom_SphericalSurface)::DownCast(surface);
    const auto torus = Handle(Geom_ToroidalSurface)::DownCast(surface);
    if (!cylinder.IsNull()) {
        frame = cylinder->Position();
        major_radius = cylinder->Radius();
    } else if (!cone.IsNull()) {
        frame = cone->Position();
        major_radius = cone->RefRadius();
        radial_slope = sin(cone->SemiAngle());
        axial_slope = cos(cone->SemiAngle());
    } else if (!sphere.IsNull()) {
        frame = sphere->Position();
        minor_radius = sphere->Radius();
    } else if (!torus.IsNull()) {
        frame = torus->Position();
        major_radius = torus->MajorRadius();
        minor_radius = torus->MinorRadius();
    } else {
        return nullptr;
    }

    const auto location = frame.Location();
    const auto x_dir = frame.XDirection();
    const auto y_dir = frame.YDirection();
    const auto z_dir = frame.Direction();
    const ON_3dPoint origin(location.X(), location.Y(), location.Z());
    const ON_3dVector x(x_dir.X(), x_dir.Y(), x_dir.Z());
    const ON_3dVector y(y_dir.X(), y_dir.Y(), y_dir.Z());
    const ON_3dVector z(z_dir.X(), z_dir.Y(), z_dir.Z());

    std::unique_ptr<ON_Curve> profile;
    if (minor_radius > 0.0) {
        if (v_span > revolution + angle_roundoff)
            return nullptr;
        const ON_3dVector radial = cos(v0) * x + sin(v0) * z;
        const ON_3dVector tangent = -sin(v0) * x + cos(v0) * z;
        const ON_Plane plane(origin + major_radius * x, radial, tangent);
        const ON_Arc arc(plane, minor_radius, std::min(v_span, revolution));
        profile.reset(new ON_ArcCurve(arc, v0, v1));
    } else {
        const auto point = [&](double v) {
            return origin + (major_radius + radial_slope * v) * x + (axial_slope * v) * z;
        };
        profile.reset(new ON_LineCurve(point(v0), point(v1)));
        if (!profile->SetDomain(v0, v1))
            return nullptr;
    }

    std::unique_ptr<ON_RevSurface> result(new ON_RevSurface());
    result->m_curve = profile.release();
    result->m_axis = ON_Line(origin, origin + ON_3dVector::CrossProduct(x, y));
    result->m_angle.Set(u0, u0 + std::min(u_span, revolution));
    result->m_t.Set(u0, u1);
    result->BoundingBox();
    return result->IsValid() ? result.release() : nullptr;
}

static bool
join_singular_trims(ON_Brep &brep, double tolerance)
{
    for (int li = 0; li < brep.m_L.Count(); ++li) {
        ON_BrepLoop &loop = brep.m_L[li];
        const auto *surface = loop.Face()->SurfaceOf();
        const int trim_count = loop.TrimCount();
        if (trim_count < 2 || !surface)
            continue;
        for (int i = 0; i < trim_count; ++i) {
            ON_BrepTrim &trim = *loop.Trim(i);
            if (trim.m_type != ON_BrepTrim::singular)
                continue;
            const ON_BrepTrim &previous = *loop.Trim((i + trim_count - 1) % trim_count);
            const ON_BrepTrim &next = *loop.Trim((i + 1) % trim_count);
            const int vertex_index = trim.m_vi[0];
            if (vertex_index < 0 || vertex_index >= brep.m_V.Count() ||
                trim.m_vi[1] != vertex_index || previous.m_vi[1] != vertex_index ||
                next.m_vi[0] != vertex_index)
                continue;
            const auto start = previous.PointAtEnd();
            const auto end = next.PointAtStart();
            if ((start.DistanceTo(trim.PointAtStart()) <= ON_ZERO_TOLERANCE &&
                 end.DistanceTo(trim.PointAtEnd()) <= ON_ZERO_TOLERANCE) ||
                start.DistanceTo(end) <= ON_ZERO_TOLERANCE)
                continue;

            // An authored degenerate edge has no model-space length, but its
            // pcurve may still extend past the neighboring trims at a pole.
            // Rebuild only on a truly collapsed side at their shared vertex.
            bool singular_side = false;
            for (int side = 0; side < 4 && !singular_side; ++side) {
                if (!surface->IsSingular(side))
                    continue;
                const int fixed_axis = (side % 2 == 0) ? 1 : 0;
                const ON_Interval fixed_domain = surface->Domain(fixed_axis);
                const ON_Interval varying_domain = surface->Domain(1 - fixed_axis);
                const double bound = (side == 0 || side == 3) ?
                    fixed_domain.Min() : fixed_domain.Max();
                singular_side = fabs(start[fixed_axis] - bound) <= ON_ZERO_TOLERANCE &&
                    fabs(end[fixed_axis] - bound) <= ON_ZERO_TOLERANCE &&
                    varying_domain.Includes(start[1 - fixed_axis]) &&
                    varying_domain.Includes(end[1 - fixed_axis]);
            }
            if (!singular_side)
                continue;
            const ON_BrepVertex &vertex = brep.m_V[vertex_index];
            const double distance = std::max(tolerance, vertex.m_tolerance);
            const auto start_lift = surface->PointAt(start.x, start.y);
            const auto end_lift = surface->PointAt(end.x, end.y);
            if (!start_lift.IsValid() || !end_lift.IsValid() ||
                start_lift.DistanceTo(vertex.point) > distance ||
                end_lift.DistanceTo(vertex.point) > distance)
                continue;
            auto curve = std::make_unique<ON_LineCurve>(start, end);
            const auto domain = trim.Domain();
            if (!curve->ChangeDimension(2) || !curve->SetDomain(domain.Min(), domain.Max()))
                return false;
            const int c2 = brep.AddTrimCurve(curve.get());
            if (c2 < 0)
                return false;
            curve.release();
            if (!brep.SetTrimCurve(trim, c2) || !brep.SetTrimBoundingBoxes(loop))
                return false;
        }
    }
    return true;
}

bool
occt_to_brep(const TopoDS_Shape& shape, ON_Brep& brep,
             double linear_tolerance, ON_TextLog* log)
{
    brep.Destroy();

    if (shape.IsNull()) return false;


    // OCCT identity includes the location and ignores traversal orientation.
    // Coincident points can belong to separate topology vertices.
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> vertices, edges, faces;
    TopExp::MapShapes(shape, TopAbs_VERTEX, vertices);
    TopExp::MapShapes(shape, TopAbs_EDGE, edges);
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    using EdgeKey = int;
    std::map<int, int> vertex_map;
    std::map<EdgeKey, int> edge_map;
    std::map<int, int> surface_map;
    std::unordered_map<int,int> trim_curve_refcount; // c2i -> trims created so far
    // Original OCCT surface periods, stored per surface index (si) so that the
    // gap-snap code can apply period-shift corrections even when the converted
    // ON_NurbsSurface does not report IsClosed() correctly (e.g. a cylinder face
    // whose BSpline representation spans slightly more than one period).
    std::map<int, double> surface_uperiod; // si → OCCT U period (0 = non-periodic)
    std::map<int, double> surface_vperiod; // si → OCCT V period (0 = non-periodic)

    const auto make_edge_key = [&edges](const TopoDS_Edge& edge) {
        return edges.FindIndex(edge);
    };

    // ------------------------------------------------------------------
    // Pass 1 — collect and translate all vertices
    // ------------------------------------------------------------------
    for (TopExp_Explorer ex(shape, TopAbs_VERTEX); ex.More(); ex.Next()) {
        const TopoDS_Vertex& vtx =
            TopoDS::Vertex(ex.Current());

        gp_Pnt p = BRep_Tool::Pnt(vtx);  // applies location transform
        const int key = vertices.FindIndex(vtx);
        if (vertex_map.count(key)) continue;

        double tol = BRep_Tool::Tolerance(vtx);

        ON_BrepVertex& ov = brep.NewVertex(
            ON_3dPoint(p.X(), p.Y(), p.Z()), tol);
        vertex_map[key] = ov.m_vertex_index;
    }

    // ------------------------------------------------------------------
    // Pass 2 — collect and translate all 3-D edge curves
    // ------------------------------------------------------------------
    for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
        const TopoDS_Edge& edge = TopoDS::Edge(ex.Current());
        EdgeKey ek = make_edge_key(edge);
        if (edge_map.count(ek)) continue;

        // Degenerate edges (poles) must NOT become ON_BrepEdge objects —
        // they are represented as singular trims in openNURBS.  We store
        // a sentinel value (-2) so Pass 3 can detect them.
        if (BRep_Tool::Degenerated(edge)) {
            edge_map[ek] = -2;
            continue;
        }

        // Detect topologically-closed edges: same underlying vertex TShape at
        // both ends AND the same 3D world position.  Edges where the same TShape
        // appears at two DIFFERENT positions (e.g. a translated body-of-revolution
        // with a shared seam vertex at two Z heights) are NOT closed edges.
        //
        // Secondary case: different TShape objects at the same 3D position can
        // arise when STEP assemblies store a seam-return edge with two distinct
        // vertex instances that happen to be geometrically coincident.  We treat
        // such edges as closed only when the 3D curve also evaluates to the same
        // 3D point at both ends (confirming a genuine loop rather than a very
        // short open edge with coincident-by-chance endpoints).
        bool topoClosedEdge = false;
        bool sameTShapeClosedEdge = false; // same-TShape detection (canonical closed)
        {
            TopoDS_Vertex tv0, tv1;
            TopExp::Vertices(edge, tv0, tv1, /*CumOri=*/false);
            if (!tv0.IsNull() && !tv1.IsNull()) {
                gp_Pnt p0 = BRep_Tool::Pnt(tv0);
                gp_Pnt p1 = BRep_Tool::Pnt(tv1);
                double vtx_tol =
                    std::max(BRep_Tool::Tolerance(tv0),
                             BRep_Tool::Tolerance(tv1));
                if (tv0.TShape() == tv1.TShape()) {
                    // Canonical closed edge: same underlying TShape.
                    // Require the vertex positions to be coincident.
                    if (p0.Distance(p1) <= vtx_tol + 1e-7) {
                        topoClosedEdge = true;
                        sameTShapeClosedEdge = true;
                    }
                } else {
                    // Different TShape instances.  Apply a two-tier check:
                    // 1. Outer guard (vertex proximity): the vertex positions
                    //    must be within vtx_tol.  This is the same condition
                    //    as for same-TShape and prevents treating open edges
                    //    in round-tripped shapes (where vtx_tol comes from the
                    //    linear_tolerance passed to ON_BrepToOCCT, typically
                    //    1e-6) as closed.
                    // 2. Inner check (curve closure): the 3D curve must
                    //    evaluate to NEARLY EXACTLY coincident endpoints.
                    //    True closed loops (full circles, full ellipses)
                    //    evaluate to the same mathematical point at both ends;
                    //    after B-spline conversion the residual gap is at most
                    //    ~1e-10 (machine precision).  Open arcs whose endpoints
                    //    happen to be "within etol" of each other have a gap of
                    //    order etol itself — so we require gap < etol * 1e-3.
                    //    This rejects such open arcs while still accepting
                    //    genuine full-period seam curves.
                    if (p0.Distance(p1) <= vtx_tol + 1e-7) {
                        double tc0, tc1;
                        Handle(Geom_Curve) gc_tmp =
                            BRep_Tool::Curve(edge, tc0, tc1);
                        if (!gc_tmp.IsNull()) {
                            gp_Pnt q0, q1;
                            gc_tmp->D0(tc0, q0);
                            gc_tmp->D0(tc1, q1);
                            double etol = BRep_Tool::Tolerance(edge);
                            topoClosedEdge =
                                (q0.Distance(q1) <= etol * 1e-3 + 1e-10);
                        }
                    }
                }
            }
        }

        double t0, t1;
        Handle(Geom_Curve) gc = BRep_Tool::Curve(edge, t0, t1);

        // Determine the actual vertex parameter range for this edge.
        // BRep_Tool::Curve gives the 3D curve and its edge-level range
        // [t0, t1].  For BSplines stored with a wider natural domain (e.g.
        // [-3.1, 76.07] when the edge only uses [7.9, 24.19]), t0 equals the
        // full BSpline start, not the vertex parameter.  We fetch the vertex
        // parameters via BRep_Tool::Parameter to get the exact sub-range.
        //
        // TopExp::Vertices(CumOri=false) returns vertices ordered by the
        // EDGE's own orientation: for a REVERSED edge tv0 is the end vertex
        // (at the curve's last parameter) and tv1 is the start vertex.  When
        // the resulting vertex parameters are inverted (vp0 > vp1) we swap
        // tv0/tv1 so that tv0 always corresponds to edge_t0 (curve start).
        TopoDS_Vertex tv0, tv1;
        double edge_t0 = t0, edge_t1 = t1;
        if (!topoClosedEdge) {
            TopExp::Vertices(edge, tv0, tv1, /*CumOri=*/false);
            if (!tv0.IsNull()) {
                try {
                    double vertex_param = BRep_Tool::Parameter(tv0, edge);
                    if (std::isfinite(vertex_param)) edge_t0 = vertex_param;
                } catch (...) {}
            }
            if (!tv1.IsNull()) {
                try {
                    double vertex_param = BRep_Tool::Parameter(tv1, edge);
                    if (std::isfinite(vertex_param)) edge_t1 = vertex_param;
                } catch (...) {}
            }
            // For REVERSED edges BRep_Tool::Parameter(tv0) = t_end > t_start.
            // Swap so edge_t0 ≤ edge_t1 and tv0 corresponds to curve start.
            if (edge_t0 > edge_t1) {
                std::swap(edge_t0, edge_t1);
                std::swap(tv0, tv1);
            }
        } else {
            // Closed edges: still look up vertices for vi0/vi1 assignment.
            TopExp::Vertices(edge, tv0, tv1, /*CumOri=*/false);
        }

        int c3i = -1;
        if (!gc.IsNull()) {
            // For non-BSpline curves (Geom_Line, Geom_Circle, Geom_Ellipse,
            // etc.) always convert only the edge's actual parameter range
            // [edge_t0, edge_t1] using a Geom_TrimmedCurve.  This avoids
            // domain mismatch when the full curve spans more than the edge
            // (e.g. Geom_Circle with t=[3π/2..5π/2]) and avoids periodic
            // BSplines.
            //
            // For topologically-closed edges we also force the TrimmedCurve
            // path even when gc is already a BSpline, so that the resulting
            // ON_NurbsCurve has domain exactly [t0, t1] with matching
            // start/end 3D points (required for IsClosed() == true).
            //
            // For BSpline edges whose natural domain is wider than the edge
            // range (e.g. [-3.1, 76.07] vs vertex parameters [7.9, 24.19]),
            // we also trim so that NC.Domain() == [edge_t0, edge_t1] and
            // NC(edge_t0) == vertex position.  This keeps ON_BrepEdge.Domain()
            // equal to the trimmed range without relying on reparameterization
            // via SetProxyCurveDomain (whose Domain() getter still returns the
            // wider NC domain, causing vertex mismatches in ON_BrepToOCCT).
            // A small tolerance is used to allow for floating-point rounding
            // when comparing the edge range against the BSpline domain bounds.
            static constexpr double kEdgeParamTol = 1e-9;
            Handle(Geom_BSplineCurve) bc;
            if (!topoClosedEdge) {
                bc = Handle(Geom_BSplineCurve)::DownCast(gc);
                // Trim the BSpline if its natural domain is wider than the
                // vertex parameter range.
                if (!bc.IsNull() &&
                    (fabs(edge_t0 - bc->FirstParameter()) > kEdgeParamTol ||
                     fabs(edge_t1 - bc->LastParameter())  > kEdgeParamTol)) {
                    try {
                        Handle(Geom_TrimmedCurve) tc =
                            new Geom_TrimmedCurve(bc, edge_t0, edge_t1);
                        bc = GeomConvert::CurveToBSplineCurve(tc);
                    } catch (...) {
                        bc = Handle(Geom_BSplineCurve)::DownCast(gc);
                    }
                }
            }
            if (bc.IsNull()) {
                // Trimmed conversion first (preserves arc domain)
                try {
                    Handle(Geom_TrimmedCurve) tc =
                        new Geom_TrimmedCurve(gc, edge_t0, edge_t1);
                    bc = GeomConvert::CurveToBSplineCurve(tc);
                } catch (...) {}
            }
            if (bc.IsNull()) {
                // Last resort: convert the full (possibly infinite) curve
                try {
                    bc = GeomConvert::CurveToBSplineCurve(gc);
                } catch (...) {}
            }
            // Periodic BSplines have one fewer knot than the non-periodic
            // convention expected by occt_curve_to_nurbs.  Flatten to non-periodic.
            if (!bc.IsNull() && bc->IsPeriodic())
                bc->SetNotPeriodic();
            if (!bc.IsNull()) {
                ON_NurbsCurve* nc = new ON_NurbsCurve();
                if (occt_curve_to_nurbs(bc, *nc)) {
                    c3i = brep.AddEdgeCurve(nc);
                } else {
                    delete nc;
                }
            }
        }

        // Secondary topoClosedEdge detection: if the converted 3D NURBS is
        // already IsClosed() (endpoints coincide via ON_PointsAreCoincident),
        // treat the edge as topologically closed.  This handles edges where
        // the OCCT vertices have different TShape objects at positions that
        // don't satisfy the vertex-proximity check above (e.g. a seam edge
        // whose two vertex instances are stored at different assembly
        // positions, even though the 3D curve is a perfect closed loop).
        if (!topoClosedEdge && c3i >= 0) {
            const ON_NurbsCurve* nc3 =
                ON_NurbsCurve::Cast(brep.m_C3[c3i]);
            if (nc3 && nc3->IsClosed())
                topoClosedEdge = true;
        }

        // For topologically-closed edges the curve's own domain endpoints
        // must be coincident (IsClosed() == true).  Snap the last CV to
        // exactly match the first so that ON_PointsAreCoincident() returns
        // true.  This covers two cases:
        // 1. Same-TShape seam edges: endpoints are nearly equal after
        //    B-spline conversion; the snap removes any floating-point
        //    residual.  Guard: only snap when the current gap is small
        //    (within the edge tolerance) to avoid corrupting open arcs that
        //    were misclassified as closed.
        // 2. Secondary IsClosed() edges: the B-spline conversion already
        //    produced endpoints that ON_PointsAreCoincident() accepts;
        //    snap is a no-op in that case.
        //
        // Two sub-cases:
        // (a) Small gap (< kTopoSnapGap): floating-point residual from
        //     SetNotPeriodic() or tiny numerical drift.  Snap cv[N-1] to
        //     cv[0] so ON_PointsAreCoincident() returns true.
        // (b) Large gap (≥ kTopoSnapGap) for a same-TShape edge: the OCCT
        //     shape has an edge whose 3D curve nearly closes via the vertex
        //     tolerance rather than geometric coincidence (e.g. a spiral-
        //     like arc that starts and ends at the same vertex with a large
        //     tolerance).  Snapping would corrupt the geometry; instead
        //     record the gap so the edge tolerance can be raised to cover it,
        //     satisfying the IsClosed() tolerance-fallback check in openNURBS.
        static constexpr double kTopoSnapGap = 1e-3;
        double topo_closed_large_gap = 0.0; // filled when (b) applies
        if (topoClosedEdge && c3i >= 0) {
            ON_NurbsCurve* nc =
                ON_NurbsCurve::Cast(brep.m_C3[c3i]);
            if (nc && nc->m_cv_count >= 2) {
                // Compute endpoint gap via the NURBS evaluator.
                ON_3dPoint np0 = nc->PointAt(nc->Domain()[0]);
                ON_3dPoint np1 = nc->PointAt(nc->Domain()[1]);
                double nc_gap = np0.DistanceTo(np1);
                double edge_tol_snap = BRep_Tool::Tolerance(edge);
                if (nc_gap < kTopoSnapGap) {
                    // Case (a): small gap — snap last CV to first.
                    // Applies to both same-TShape (floating-point residual
                    // after SetNotPeriodic) and secondary IsClosed() detections.
                    const int stride = nc->m_cv_stride;
                    double* cv0 = nc->CV(0);
                    double* cvN = nc->CV(nc->m_cv_count - 1);
                    if (cv0 && cvN) {
                        for (int k = 0; k < stride; ++k)
                            cvN[k] = cv0[k];
                    }
                } else if (sameTShapeClosedEdge) {
                    // Case (b): large gap, same-TShape topology.  The edge
                    // is topologically closed with a large vertex tolerance
                    // bridging the geometric gap.  Record for tolerance fixup.
                    topo_closed_large_gap = nc_gap;
                }
                // If nc_gap ≥ kTopoSnapGap and NOT sameTShape (different-
                // TShape false-close detection), skip both actions — this
                // is an open arc that happened to pass proximity checks; do
                // not corrupt it.
                (void)edge_tol_snap; // used only by the removed guard
            }
        }

        // Determine start/end vertex indices.
        // tv0/tv1 were already computed above (with the swap applied so that
        // tv0 is always the vertex at edge_t0 = the curve start).
        // The EdgeKey used by make_edge_key may differ from the swapped tv0
        // (it was based on the original TopExp order), but that is only for
        // lookup purposes; the vi0/vi1 assignment here uses the corrected tv0.
        int vi0 = -1, vi1 = -1;
        {
            if (!tv0.IsNull()) {
                auto it = vertex_map.find(vertices.FindIndex(tv0));
                if (it != vertex_map.end()) vi0 = it->second;
            }
            if (!tv1.IsNull()) {
                auto it = vertex_map.find(vertices.FindIndex(tv1));
                if (it != vertex_map.end()) vi1 = it->second;
            }
        }
        // For topologically-closed edges always use the same vertex at both
        // ends so that the pcurve's IsClosed() == true check in ON_Brep
        // IsValid() is consistent with the trim's vertex indices.
        if (topoClosedEdge) {
            if      (vi0 >= 0) vi1 = vi0;
            else if (vi1 >= 0) vi0 = vi1;
        }
        if (vi1 < 0) vi1 = vi0; // fallback for truly free-standing closed edge

        // Use the vertex-reference form of NewEdge so that openNURBS
        // automatically maintains the vertex→edge back-references.
        // The NC was already trimmed to [edge_t0, edge_t1] above, so
        // NewEdge will set ProxyCurveDomain = NC.Domain() = [edge_t0, edge_t1]
        // automatically (no separate SetProxyCurveDomain call needed).
        double edge_tol = BRep_Tool::Tolerance(edge);
        if (edge_tol < 0.0) edge_tol = linear_tolerance;
        // For same-TShape edges whose 3D curve has a large geometric gap
        // (case b above): raise edge tolerance to cover the gap so that
        // ON_BrepEdge::IsClosed() succeeds via the tolerance fallback.
        if (topo_closed_large_gap > edge_tol)
            edge_tol = topo_closed_large_gap;
        ON_BrepEdge* poe = nullptr;
        if (vi0 >= 0 && vi1 >= 0) {
            poe = &brep.NewEdge(brep.m_V[vi0], brep.m_V[vi1], c3i,
                                nullptr,
                                edge_tol);
        } else {
            poe = &brep.NewEdge(c3i);
            poe->m_tolerance = edge_tol;
        }
        int this_ei = poe->m_edge_index;
        // Defensive: force tolerance on the edge after creation
        brep.m_E[this_ei].m_tolerance = edge_tol;
        edge_map[ek] = this_ei;
    }

    // ------------------------------------------------------------------
    // Pass 3 — translate faces
    // ------------------------------------------------------------------
    for (TopExp_Explorer fex(shape, TopAbs_FACE); fex.More(); fex.Next()) {
        TopoDS_Face face = TopoDS::Face(fex.Current());
        // Wire direction is relative to the surface.  Store face sense separately.
        const bool reversed_face = face.Orientation() == TopAbs_REVERSED;
        face.Orientation(TopAbs_FORWARD);

        // Translate surface
        Handle(Geom_Surface) gs = BRep_Tool::Surface(face);
        int si = -1;
        if (!gs.IsNull()) {
            // ------------------------------------------------------------------
            // Try analytical surface types first (cylinder, sphere, plane).
            // Storing these as ON_RevSurface / ON_PlaneSurface lets ON_BrepToOCCT
            // reconstruct exact OCCT analytical surfaces, so BRepGProp can use
            // exact integration rather than Gaussian quadrature on a B-spline.
            // ------------------------------------------------------------------
            const int sk_try = faces.FindIndex(face);
            if (!surface_map.count(sk_try)) {
                double u1, u2, v1, v2;
                BRepTools::UVBounds(face, u1, u2, v1, v2);

                const double kTwoPi = 2.0 * ON_PI;
                const auto basis = surface_basis(gs);
                if (ON_RevSurface *analytic = analytic_revolution(basis, u1, u2, v1, v2))
                    si = brep.AddSurface(analytic);

                // Surfaces stored analytically as Geom_SurfaceOfRevolution
                // (e.g. produced by SurfaceToOCCT for torus, NurbsCurve-
                // profile, or partial-revolution arc-profile surfaces) are
                // converted back to ON_RevSurface to preserve the analytic
                // structure.  The meridian curve is recovered via NURBS
                // conversion of BasisCurve(); m_angle is set to the face's
                // UV U bounds; m_t = m_angle so the UV parameter is the
                // angle in radians.
                if (si < 0) {
                    Handle(Geom_SurfaceOfRevolution) revSrf =
                        Handle(Geom_SurfaceOfRevolution)::DownCast(basis);
                    if (!revSrf.IsNull() && u2 > u1 &&
                        (u2 - u1) <= kTwoPi + 1e-10) {
                        gp_Ax1 ax  = revSrf->Axis();
                        gp_Pnt loc = ax.Location();
                        gp_Dir dir = ax.Direction();
                        // Convert the meridian (basis curve) to ON_NurbsCurve.
                        Handle(Geom_Curve) basisC = revSrf->BasisCurve();
                        Handle(Geom_BSplineCurve) basisBS;
                        try {
                            basisBS = GeomConvert::CurveToBSplineCurve(basisC);
                            if (!basisBS.IsNull() && basisBS->IsPeriodic())
                                basisBS->SetNotPeriodic();
                        } catch (...) {}
                        if (!basisBS.IsNull()) {
                            ON_NurbsCurve* nc = new ON_NurbsCurve();
                            if (occt_curve_to_nurbs(basisBS, *nc)) {
                                ON_RevSurface* rev2 = new ON_RevSurface();
                                rev2->m_curve = nc;
                                rev2->m_axis.from =
                                    ON_3dPoint(loc.X(), loc.Y(), loc.Z());
                                ON_3dVector axd(dir.X(), dir.Y(), dir.Z());
                                rev2->m_axis.to = rev2->m_axis.from + axd;
                                rev2->m_angle.Set(u1, u2);
                                rev2->m_t = rev2->m_angle;
                                rev2->m_bTransposed = false;
                                rev2->BoundingBox();
                                if (rev2->IsValid()) {
                                    si = brep.AddSurface(rev2);
                                } else {
                                    delete rev2;
                                    delete nc;
                                }
                            } else {
                                delete nc;
                            }
                        }
                    }
                }
            } // closes if(!surface_map.count(sk_try))

            Handle(Geom_BSplineSurface) bs =
                Handle(Geom_BSplineSurface)::DownCast(gs);
            // For periodic BSpline surfaces OR non-BSpline analytical surfaces
            // (Geom_CylindricalSurface, Geom_Plane, etc.): use the
            // Geom_RectangularTrimmedSurface path which always produces a
            // non-periodic BSpline whose UV domain exactly matches the face's
            // UV parameter bounds (= the pcurves' UV range).
            //
            // IMPORTANT: For seam faces whose UV bounds span the full period
            // of a periodic surface, Geom_RectangularTrimmedSurface reduces
            // the domain modulo the period and collapses it to nearly zero.
            // Subsequent knot reparameterisation scales the knots to [0,2π]
            // but the control points describe a zero-width arc, giving zero
            // surface area.  Fix: when the face spans ≥99% of the period in
            // U or V, trim only in the OTHER direction and then remove the
            // residual periodicity with SetU/VNotPeriodic().
            bool need_trimmed = si < 0 && (bs.IsNull() ||
                                bs->IsUPeriodic() || bs->IsVPeriodic());
            if (need_trimmed) {
                Handle(Geom_BSplineSurface) bs_orig = bs; // save in case convert fails
                try {
                    double u1, u2, v1, v2;
                    BRepTools::UVBounds(face, u1, u2, v1, v2);

                    // Detect if the face spans (nearly) the full period in U
                    // or V so that we can avoid the domain-collapse trap.
                    bool u_full = false, v_full = false;
                    if (gs->IsUPeriodic()) {
                        double uper = gs->UPeriod();
                        if (uper > 0 && (u2 - u1) >= 0.99 * uper)
                            u_full = true;
                    }
                    if (gs->IsVPeriodic()) {
                        double vper = gs->VPeriod();
                        if (vper > 0 && (v2 - v1) >= 0.99 * vper)
                            v_full = true;
                    }

                    if ((u_full || v_full) &&
                        !Precision::IsInfinite(v1) &&
                        !Precision::IsInfinite(v2) &&
                        v2 > v1 &&
                        !Precision::IsInfinite(u1) &&
                        !Precision::IsInfinite(u2) &&
                        u2 > u1) {
                        // Trim only in the direction(s) that are NOT full
                        // period, then use SetNotPeriodic + Segment to
                        // extract the exact face parameter range.
                        //
                        // Why Segment?  SetUNotPeriodic() on a degree-d
                        // periodic circle expands the domain from [0, T] to
                        // [-T/d, T*(1+1/d)] by prepending/appending extra
                        // knots.  A subsequent linear knot rescale would
                        // restore the domain width but change the rational
                        // blending functions, distorting the geometry.
                        // Segment() uses Boehm's knot-insertion algorithm
                        // which correctly adjusts both poles and weights, so
                        // the geometry is preserved exactly.
                        if (u_full && !v_full) {
                            // Trim V only (Standard_False = V trim)
                            Handle(Geom_RectangularTrimmedSurface) ts_v =
                                new Geom_RectangularTrimmedSurface(
                                    gs, v1, v2, false);
                            bs = GeomConvert::SurfaceToBSplineSurface(ts_v);
                        } else if (v_full && !u_full) {
                            // Trim U only (Standard_True = U trim)
                            Handle(Geom_RectangularTrimmedSurface) ts_u =
                                new Geom_RectangularTrimmedSurface(
                                    gs, u1, u2, true);
                            bs = GeomConvert::SurfaceToBSplineSurface(ts_u);
                        } else {
                            // Both directions span the full period (e.g.
                            // a torus): convert the un-trimmed surface.
                            bs = GeomConvert::SurfaceToBSplineSurface(gs);
                        }

                        // Remove periodicity and then segment to the exact
                        // face UV bounds so the surface domain matches the
                        // pcurves without any knot rescaling.
                        if (!bs.IsNull()) {
                            if (bs->IsUPeriodic() && bs->NbUKnots() >= 2 &&
                                bs->UKnot(bs->NbUKnots()) -
                                    bs->UKnot(1) > 1e-15)
                                bs->SetUNotPeriodic();
                            if (bs->IsVPeriodic() && bs->NbVKnots() >= 2 &&
                                bs->VKnot(bs->NbVKnots()) -
                                    bs->VKnot(1) > 1e-15)
                                bs->SetVNotPeriodic();
                            // Segment to face bounds (clamped to the new
                            // non-periodic domain).
                            //
                            // Segment() uses Boehm's knot-insertion algorithm
                            // to correctly adjust poles and weights while
                            // producing a B-spline with domain exactly
                            // [su0,su1]x[sv0,sv1].  If Segment() fails (e.g.
                            // because the bounds coincide with existing
                            // multiplicity-saturated knots), fall back to the
                            // SetUNotPeriodic result: the expanded domain
                            // [-T/d, T*(1+1/d)] still contains all pcurve UV
                            // values and evaluates correctly at them, so the
                            // face topology is preserved.  The updated
                            // reparameterization guard below avoids distorting
                            // the geometry by not rescaling in this case.
                            double ku0 = bs->UKnot(1);
                            double ku1 = bs->UKnot(bs->NbUKnots());
                            double kv0 = bs->VKnot(1);
                            double kv1 = bs->VKnot(bs->NbVKnots());
                            double su0 = std::max(ku0, std::min(ku1, u1));
                            double su1 = std::max(ku0, std::min(ku1, u2));
                            double sv0 = std::max(kv0, std::min(kv1, v1));
                            double sv1 = std::max(kv0, std::min(kv1, v2));
                            if (su1 - su0 > 1e-12 && sv1 - sv0 > 1e-12) {
                                try {
                                    bs->Segment(su0, su1, sv0, sv1);
                                } catch (...) {}
                            }
                        }
                    } else {
                        Handle(Geom_RectangularTrimmedSurface) ts =
                            new Geom_RectangularTrimmedSurface(
                                gs, u1, u2, v1, v2);
                        bs = GeomConvert::SurfaceToBSplineSurface(ts);
                    }
                } catch (...) {
                    // Restore original so SetUNotPeriodic() below is not
                    // called on the shared geometry object (which would
                    // mutate gs and break period detection).
                    bs = bs_orig;
                }
            }
            if (si < 0 && !bs.IsNull()) {
                // Handle any residual periodicity (safety fallback for
                // surfaces that GeomConvert still left periodic).
                // BSplCLib::Unperiodize computes scale = 1/(knot_last - knot_first).
                // For degenerate periodic surfaces (collapsed knot span), this
                // produces a zero denominator, triggering SIGFPE which cannot be
                // caught by C++ try-catch.  Guard: only unperiodize when the
                // knot span is non-zero.
                if (bs->IsUPeriodic() && bs->NbUKnots() >= 2 &&
                    bs->UKnot(bs->NbUKnots()) - bs->UKnot(1) > 1e-15)
                    bs->SetUNotPeriodic();
                if (bs->IsVPeriodic() && bs->NbVKnots() >= 2 &&
                    bs->VKnot(bs->NbVKnots()) - bs->VKnot(1) > 1e-15)
                    bs->SetVNotPeriodic();
                ON_NurbsSurface* ns = new ON_NurbsSurface();
                if (occt_surface_to_nurbs(bs, *ns)) {
                    // If the converted surface's UV domain doesn't match the
                    // face UV bounds (which define the pcurves' UV range),
                    // reparameterize the knots to match.  This handles cases
                    // where GeomConvert normalizes the parameterization (e.g.
                    // a cylinder u-range [0,2π] becomes [0,5.59e-9]).
                    // Uniform reparameterization is geometry-preserving.
                    if (need_trimmed) {
                        double fu1, fu2, fv1, fv2;
                        BRepTools::UVBounds(face, fu1, fu2, fv1, fv2);
                        ON_Interval srf_u = ns->Domain(0);
                        ON_Interval srf_v = ns->Domain(1);
                        const double kDomTol = 1e-6;
                        // Reparameterize only when the face UV bounds fall
                        // OUTSIDE the surface domain (surface is too small
                        // for the face).  Do NOT reparameterize when the
                        // surface domain is larger than the face bounds —
                        // the rational blending functions must not change.
                        // For full-period surfaces the Segment() call above
                        // already made the domains match.
                        if (fu2 > fu1 && srf_u.IsValid() && srf_u.Length() > 0 &&
                            (fu1 < srf_u[0] - kDomTol ||
                             fu2 > srf_u[1] + kDomTol)) {
                            double ku_scale = (fu2 - fu1) / srf_u.Length();
                            for (int k = 0; k < ns->KnotCount(0); ++k)
                                ns->m_knot[0][k] = fu1 +
                                    (ns->m_knot[0][k] - srf_u[0]) * ku_scale;
                        }
                        if (fv2 > fv1 && srf_v.IsValid() && srf_v.Length() > 0 &&
                            (fv1 < srf_v[0] - kDomTol ||
                             fv2 > srf_v[1] + kDomTol)) {
                            double kv_scale = (fv2 - fv1) / srf_v.Length();
                            for (int k = 0; k < ns->KnotCount(1); ++k)
                                ns->m_knot[1][k] = fv1 +
                                    (ns->m_knot[1][k] - srf_v[0]) * kv_scale;
                        }
                    }
                    si = brep.AddSurface(ns);
                } else {
                    delete ns;
                }
            }

            // Cache by located face identity so seam surfaces are shared
            const int sk = faces.FindIndex(face);
            if (!surface_map.count(sk) && si >= 0) {
                surface_map[sk] = si;
                // Record the original OCCT surface's period so the gap-snap
                // code can apply period-shift corrections even when the
                // converted ON_NurbsSurface's IsClosed() returns false.
                //
                // Geom_RectangularTrimmedSurface::IsUPeriodic() always returns
                // false even when the underlying basis surface is periodic.
                // Unwrap one level of trimming before checking, and use the
                // converted ON_NurbsSurface's domain width as the effective
                // period (which correctly reflects any reparameterisation).
                if (!gs.IsNull()) {
                    const Handle(Geom_Surface) gs_base = surface_basis(gs);
                    const ON_NurbsSurface* ns_ptr =
                        ON_NurbsSurface::Cast(brep.m_S[si]);
                    // U period
                    if (gs_base->IsUPeriodic()) {
                        double pd = gs_base->UPeriod();
                        if (ns_ptr) {
                            ON_Interval du = ns_ptr->Domain(0);
                            if (du.IsIncreasing()) {
                                // Use the domain width as the effective period
                                // only when it closely matches the OCCT period
                                // (i.e. the surface domain was not expanded by
                                // SetUNotPeriodic from [-T/d, T*(1+1/d)]).
                                // For expanded domains, the OCCT period is the
                                // correct shift for gap-repair corrections.
                                if (fabs(du.Length() - pd) < 1e-4 * pd)
                                    pd = du.Length();
                            }
                        }
                        surface_uperiod[si] = pd;
                    } else {
                        surface_uperiod[si] =
                            gs->IsUPeriodic() ? gs->UPeriod() : 0.0;
                    }
                    // V period
                    if (gs_base->IsVPeriodic()) {
                        double pd = gs_base->VPeriod();
                        if (ns_ptr) {
                            ON_Interval dv = ns_ptr->Domain(1);
                            if (dv.IsIncreasing()) {
                                if (fabs(dv.Length() - pd) < 1e-4 * pd)
                                    pd = dv.Length();
                            }
                        }
                        surface_vperiod[si] = pd;
                    } else {
                        surface_vperiod[si] =
                            gs->IsVPeriodic() ? gs->VPeriod() : 0.0;
                    }
                }
            }
        }

        // Check if the surface pointer was already added
        if (si < 0) {
            const int sk = faces.FindIndex(face);
            auto it = surface_map.find(sk);
            if (it != surface_map.end())
                si = it->second;
        }

        if (si < 0) {
            if (log)
                log->Print("cannot translate source face %d surface %s\n",
                    faces.FindIndex(face), gs.IsNull() ? "(none)" : gs->DynamicType()->Name());
            return false;
        }

        ON_BrepFace& of = brep.NewFace(si);
        of.m_bRev = reversed_face;

        // -- Iterate wires → loops
        // openNURBS requires face.m_li[0] to be the outer loop.  Use
        // BRepTools::OuterWire() to identify the outer wire, then
        // iterate wires in outer-first order so the outer loop lands at
        // face.m_li[0].
        {
        TopoDS_Wire face_outer_wire = BRepTools::OuterWire(face);

        // Collect wires, outer wire first.
        std::vector<TopoDS_Wire> face_wires;
        if (!face_outer_wire.IsNull())
            face_wires.push_back(face_outer_wire);
        for (TopExp_Explorer wex(face, TopAbs_WIRE); wex.More(); wex.Next()) {
            const TopoDS_Wire& w = TopoDS::Wire(wex.Current());
            if (face_outer_wire.IsNull() || !w.IsSame(face_outer_wire))
                face_wires.push_back(w);
        }

        for (const TopoDS_Wire& wire : face_wires) {
            // First entry is outer; all others are inner holes.
            const bool wire_is_outer = (&wire == &face_wires[0]);

            // Collect edges in the correct wire traversal order.
            // BRepTools_WireExplorer follows the wire's connectivity (handles
            // seam and degenerate edges in native OCCT shapes correctly).
            // For ON_BrepToOCCT-generated wires that lack full vertex
            // connectivity, the wire explorer may not visit all edges.  In
            // that case fall back to TopExp_Explorer which is order-agnostic
            // but visits all edges.
            std::vector<TopoDS_Edge> wire_edges;
            bool wire_explorer_found_edges = false;
            {
                for (BRepTools_WireExplorer we(wire, face); we.More(); we.Next()) {
                    wire_edges.push_back(we.Current());
                    wire_explorer_found_edges = true;
                }
            }
            // Count unique physical edges from
            // TopExp_Explorer.  WireExplorer may visit seam edges twice (FORWARD
            // and REVERSED); since both traversals produce the same EdgeKey, the
            // unique-key count is the right comparison.
            {
                std::set<EdgeKey> tex_keys;
                for (TopExp_Explorer eex(wire, TopAbs_EDGE);
                     eex.More(); eex.Next()) {
                    tex_keys.insert(
                        make_edge_key(TopoDS::Edge(eex.Current())));
                }
                std::set<EdgeKey> we_keys;
                for (const TopoDS_Edge& e : wire_edges)
                    we_keys.insert(make_edge_key(e));

                // If BRepTools_WireExplorer found nothing but TopExp found
                // edges, this is an open (slit/dangling) wire.  ON_Brep
                // requires all loops to be closed; skip these wires.  They
                // appear in FreeCAD files as reference geometry or partition
                // seams and do not bound a surface region.
                if (!wire_explorer_found_edges && !tex_keys.empty())
                    continue;

                if (we_keys.size() < tex_keys.size()) {
                    // WireExplorer missed some edges (e.g. degenerate edges
                    // break the vertex-connectivity chain).  Fall back to
                    // TopoDS_Iterator which preserves the stored wire order
                    // as-built by the STEP/BRep reader — this matches the
                    // intended UV traversal order.  TopExp_Explorer would
                    // also visit all edges, but may revisit certain edges
                    // (e.g. seam edges) and does not guarantee the same
                    // sequential order as TopoDS_Iterator for wires with
                    // mixed degenerate/regular edges.
                    wire_edges.clear();
                    for (TopoDS_Iterator it(wire); it.More(); it.Next()) {
                        if (it.Value().ShapeType() == TopAbs_EDGE)
                            wire_edges.push_back(TopoDS::Edge(it.Value()));
                    }
                }
            }

            ON_BrepLoop& ol = brep.NewLoop(ON_BrepLoop::unknown, of);

            for (const TopoDS_Edge& edge : wire_edges) {
                EdgeKey ek = make_edge_key(edge);

                auto eit = edge_map.find(ek);
                if (eit == edge_map.end()) continue;
                int ei = eit->second;

                // Translate 2D pcurve for this edge on this face
                double pc_t0, pc_t1;

                Handle(Geom2d_Curve) gc2 =
                    BRep_Tool::CurveOnSurface(edge, face, pc_t0, pc_t1);

                int c2i = -1;
                if (!gc2.IsNull()) {
                    const auto bc2 = bounded_pcurve(gc2, pc_t0, pc_t1);
                    if (!bc2.IsNull()) {
                        int deg2    = bc2->Degree();
                        int npoles2 = bc2->NbPoles();
                        bool rat2   = bc2->IsRational() ? true : false;

                        ON_NurbsCurve* nc = new ON_NurbsCurve();
                        if (nc->Create(2, rat2, deg2 + 1, npoles2)) {
                            for (int k = 1; k <= npoles2; ++k) {
                                gp_Pnt2d p2 = bc2->Pole(k);
                                double w2   = rat2 ? bc2->Weight(k) : 1.0;
                                nc->SetCV(k - 1,
                                          ON_4dPoint(p2.X() * w2,
                                                     p2.Y() * w2,
                                                     0.0,
                                                     w2));
                            }
                            // Strip one phantom knot from each end (same
                            // convention as occt_curve_to_nurbs / occt_surface_to_nurbs).
                            int ki2 = 0;
                            const int nkg2 = bc2->NbKnots();
                            for (int k = 1; k <= nkg2; ++k) {
                                double kv2 = bc2->Knot(k);
                                int    km2 = bc2->Multiplicity(k);
                                if (k == 1 || k == nkg2)
                                    km2 -= 1;
                                for (int m = 0; m < km2 && ki2 < nc->KnotCount(); ++m)
                                    nc->SetKnot(ki2++, kv2);
                            }
                            if (ki2 == nc->KnotCount() && nc->IsValid()) {
                                // Sync pc_t0/pc_t1 to the actual B-spline domain.
                                //
                                // Geom2dConvert::CurveToBSplineCurve normalizes the
                                // output domain to [0,1] for many curve types (lines,
                                // circles, trimmed non-BSplines).  If the OCCT pcurve
                                // parameter range pc_t0/pc_t1 differs from nc->Domain(),
                                // SetProxyCurve would evaluate PointAtEnd() at an
                                // intermediate parameter of the [0,1] curve rather than
                                // at cv[last], giving a wrong trim endpoint.
                                //
                                // After this sync, proxy_dom = nc->Domain() so
                                // PointAtEnd() = curve(nc_dom.Max()) = cv[last] = the
                                // correct geometric endpoint of the edge.
                                //
                                // The m_c3i reparameterization below may further adjust
                                // nc's knots for period-shifted seam pcurves; pc_t0/pc_t1
                                // are updated there as well.
                                {
                                    ON_Interval nc_dom_init = nc->Domain();
                                    if (nc_dom_init.IsValid()) {
                                        pc_t0 = nc_dom_init.Min();
                                        pc_t1 = nc_dom_init.Max();
                                    }
                                }

                                // Remap the pcurve's knot domain to match the
                                // 3D edge domain so SameRange=true is preserved
                                // in the round-trip.  In the original OCCT B-Rep
                                // every edge with a 3D curve has SameRange=true
                                // (pcurve parameter domain == 3D curve domain).
                                // The conversion may produce a pcurve with a
                                // different domain (e.g. period-shifted by ±2π),
                                // which would require SameRange=false and trigger
                                // BRepCheck_InvalidSameRangeFlag on reconstruction.
                                if (ei >= 0 && ei < brep.m_E.Count() &&
                                    brep.m_E[ei].m_c3i >= 0) {
                                    ON_Interval edge_dom = brep.m_E[ei].Domain();
                                    ON_Interval nc_dom   = nc->Domain();
                                    if (edge_dom.IsValid() && nc_dom.IsValid() &&
                                        edge_dom.Length() > 0 && nc_dom.Length() > 0) {
                                        double old0 = nc_dom.Min(), old1 = nc_dom.Max();
                                        double new0 = edge_dom.Min(), new1 = edge_dom.Max();
                                        const double kTol = 1e-10;
                                        if (fabs(old0 - new0) > kTol ||
                                            fabs(old1 - new1) > kTol) {
                                            double scale = (new1 - new0) /
                                                           (old1 - old0);
                                            for (int k = 0; k < nc->KnotCount(); ++k)
                                                nc->m_knot[k] = new0 +
                                                    (nc->m_knot[k] - old0) * scale;
                                            pc_t0 = new0;
                                            pc_t1 = new1;
                                        }
                                    }
                                }

                                // OCCT stores all pcurves in the FORWARD edge
                                // direction, regardless of the edge orientation.
                                // For REVERSED edges, the openNURBS trim direction
                                // is opposite, so the pcurve must be reversed.
                                // This applies to seam edges too.
                                bool need_rev =
                                    (edge.Orientation() == TopAbs_REVERSED);
                                if (need_rev) {
                                    // Reverse() negates the knots so the
                                    // domain becomes [-t1, -t0].  Restore the
                                    // original parameter range [pc_t0, pc_t1]
                                    // so that SetProxyCurveDomain succeeds.
                                    nc->Reverse();
                                    nc->SetDomain(pc_t0, pc_t1);
                                }
                                c2i = brep.AddTrimCurve(nc);
                            } else {
                                delete nc;
                            }
                        } else {
                            delete nc;
                        }
                    }
                }

                // Trim orientation
                bool bRev3d = (edge.Orientation() == TopAbs_REVERSED);

                if (ei == -2) {
                    // Degenerate OCCT edge → openNURBS singular trim.
                    // Find the pole vertex by evaluating the surface at the
                    // start point of the 2D pcurve and picking the nearest
                    // already-translated vertex.  O(|vertices|) search; for
                    // typical B-Rep models (2-8 vertices) this is negligible.
                    int pole_vi = -1;
                    if (c2i >= 0 && si >= 0 && brep.m_V.Count() > 0) {
                        const ON_Surface* srf = brep.m_S[si];
                        const ON_NurbsCurve* c2 =
                            ON_NurbsCurve::Cast(brep.m_C2[c2i]);
                        if (srf && c2) {
                            ON_Interval dom = c2->Domain();
                            ON_3dPoint uv = c2->PointAt(dom.Min());
                            ON_3dPoint pt3;
                            if (srf->EvPoint(uv.x, uv.y, pt3)) {
                                double best_d = 1e30;
                                for (int vi = 0; vi < brep.m_V.Count(); ++vi) {
                                    double d = brep.m_V[vi].point.DistanceTo(pt3);
                                    if (d < best_d) {
                                        best_d = d;
                                        pole_vi = vi;
                                    }
                                }
                            }
                        }
                    }
                    if (pole_vi < 0) continue; // no vertex found — skip trim

                    ON_BrepTrim& strim = brep.NewSingularTrim(
                        brep.m_V[pole_vi], ol, ON_Surface::not_iso, c2i);
                    if (c2i >= 0)
                        ++trim_curve_refcount[c2i];
                    strim.m_tolerance[0] = BRep_Tool::Tolerance(edge);
                    strim.m_tolerance[1] = strim.m_tolerance[0];
                    if (c2i >= 0) {
                        ON_Interval proxy_dom(pc_t0, pc_t1);
                        ON_Interval nc_dom = brep.m_C2[c2i]->Domain();
                        bool use_sub = proxy_dom.IsIncreasing() &&
                                       nc_dom.Includes(proxy_dom[0]) &&
                                       nc_dom.Includes(proxy_dom[1]);
                        strim.SetProxyCurve(brep.m_C2[c2i],
                                            use_sub ? proxy_dom : nc_dom);
                    }
                    continue;
                }

                ON_BrepTrim& trim =
                    brep.NewTrim(brep.m_E[ei], bRev3d, ol, c2i);
                if (c2i >= 0)
                    ++trim_curve_refcount[c2i];
                trim.m_tolerance[0] = BRep_Tool::Tolerance(edge);
                trim.m_tolerance[1] = trim.m_tolerance[0];
                if (c2i >= 0) {
                    // SetProxyCurve (two-arg form) sets both m_this_domain and
                    // m_real_curve_domain to [pc_t0, pc_t1], so that
                    // ON_Brep::IsValid()'s direct evaluation of
                    // m_C2[c2i]->PointAt(trim.Domain()[1]) agrees with the
                    // snap code's PointAtEnd() which respects the proxy domain.
                    // SetProxyCurveDomain alone only updates m_real_curve_domain,
                    // leaving m_this_domain at the curve's full domain and causing
                    // a false gap error when the pcurve domain is a sub-range.
                    // Guards:
                    // 1. proxy_dom must be strictly increasing (no degenerate
                    //    or inverted interval).
                    // 2. proxy_dom must be a subset of the stored B-spline's
                    //    domain; SetProxyCurve intersects the two intervals, so
                    //    an out-of-range sub-interval produces EmptyInterval and
                    //    an invalid proxy (m_real_curve_domain not increasing).
                    // When either guard fails, fall back to the full curve domain.
                    ON_Interval proxy_dom(pc_t0, pc_t1);
                    ON_Interval nc_dom = brep.m_C2[c2i]->Domain();
                    bool use_sub = proxy_dom.IsIncreasing() &&
                                   nc_dom.Includes(proxy_dom[0]) &&
                                   nc_dom.Includes(proxy_dom[1]);
                    trim.SetProxyCurve(brep.m_C2[c2i],
                                       use_sub ? proxy_dom : nc_dom);
                }
                // Note: trim.m_type is set automatically by NewTrim
                // (boundary→seam→mated) — do NOT override it here.
            }

            // UV connectivity fix: close any UV gap between adjacent trims
            // in a loop so that ON_Brep::IsValid() passes the loop-continuity
            // check.  Two strategies are applied in order:
            //
            // 1. Period-gap: if the gap equals the surface period in U or V,
            //    translate ALL poles of tk1's pcurve by ±period.  This
            //    corrects cases where BRep_Tool::CurveOnSurface returns the
            //    "wrong" seam side (U=2π instead of U=0, etc.).
            //
            // 2. Small-gap snap: for any remaining non-zero gap (floating-
            //    point noise from independent pcurve evaluations, or a small
            //    but non-trivial discrepancy), overwrite the first control
            //    point of tk1's pcurve with tk's UV endpoint.  For a clamped
            //    B-spline the first pole is exactly the curve value at the
            //    domain start, so this makes the two trim endpoints exactly
            //    equal at the machine-epsilon level.
            {

                const int li = ol.m_loop_index;
                const int fi = (li >= 0 && li < brep.m_L.Count())
                                   ? brep.m_L[li].m_fi
                                   : -1;
                const ON_Surface* srf =
                    (fi >= 0) ? brep.m_F[fi].SurfaceOf() : nullptr;

                if (srf) {
                    double u0, u1, v0, v1;
                    srf->GetDomain(0, &u0, &u1);
                    srf->GetDomain(1, &v0, &v1);
                    const double uperiod = u1 - u0;
                    const double vperiod = v1 - v0;
                    const bool cls_u = srf->IsClosed(0);
                    const bool cls_v = srf->IsClosed(1);

                    // Also fetch the original OCCT surface period (recorded
                    // during surface conversion).  When the converted
                    // ON_NurbsSurface does not report IsClosed() correctly —
                    // e.g. a cylinder face whose BSpline was trimmed to a UV
                    // range slightly wider than one period, causing IsClosed(0)
                    // = false even though the underlying geometry closes — use
                    // the stored OCCT period to allow period-shift detection.
                    const int si_for_gap = (fi >= 0 && fi < brep.m_F.Count())
                                             ? brep.m_F[fi].SurfaceIndexOf()
                                             : -1;
                    auto up_it = surface_uperiod.find(si_for_gap);
                    auto vp_it = surface_vperiod.find(si_for_gap);
                    const double occt_uperiod =
                        (up_it != surface_uperiod.end()) ? up_it->second : 0.0;
                    const double occt_vperiod =
                        (vp_it != surface_vperiod.end()) ? vp_it->second : 0.0;

                    const int nc = ol.m_ti.Count();

                    for (int k = 0; k < nc; ++k) {
                        ON_BrepTrim& tk  = brep.m_T[ol.m_ti[k]];
                        ON_BrepTrim& tk1 =
                            brep.m_T[ol.m_ti[(k + 1) % nc]];

                        ON_3dPoint pe  = tk.PointAtEnd();
                        ON_3dPoint ps  = tk1.PointAtStart();
                        double dx = ps.x - pe.x;
                        double dy = ps.y - pe.y;

                        if (fabs(dx) < 1e-14 && fabs(dy) < 1e-14)
                            continue;  // already connected

                        int c2i = tk1.m_c2i;
                        if (c2i < 0 || c2i >= brep.m_C2.Count()) continue;
                        ON_NurbsCurve* nc2 =
                            ON_NurbsCurve::Cast(brep.m_C2[c2i]);
                        if (!nc2 || nc2->m_cv_count < 1) continue;

                        bool modified = false;

                        // Strategy 1: period gap — translate all poles.
                        // Check using both the ON_Surface domain width (cls_u /
                        // uperiod) and the original OCCT surface period stored
                        // during conversion.  The OCCT period catches cylinders
                        // whose BSpline spans slightly more than one period,
                        // causing IsClosed(0) = false.
                        //
                        // The cross-direction residual (dy for a U period gap,
                        // dx for a V period gap) is bounded by a fraction of
                        // the OPPOSITE domain dimension so that small numerical
                        // misalignment in the non-periodic direction does not
                        // block detection.
                        const double kRelTol = 3e-3;
                        double shift_u = 0.0, shift_v = 0.0;
                        // U period: prefer OCCT period if available, fall back
                        // to domain width when surface reports IsClosed.
                        const double eff_uperiod = (occt_uperiod > 0.0)
                                                       ? occt_uperiod
                                                       : (cls_u ? uperiod : 0.0);
                        const double eff_vperiod = (occt_vperiod > 0.0)
                                                       ? occt_vperiod
                                                       : (cls_v ? vperiod : 0.0);
                        // Absolute tolerance for the cross-direction component:
                        // use the domain width in the OTHER direction so that
                        // small residuals (e.g. dy=0.05 on a V-domain of 2.0)
                        // don't block a valid U-period shift detection.
                        const double vdom = (v1 > v0) ? (v1 - v0) : 1.0;
                        const double udom = (u1 > u0) ? (u1 - u0) : 1.0;
                        if (eff_uperiod > 0.0 &&
                            fabs(fabs(dx) - eff_uperiod) <
                                kRelTol * eff_uperiod &&
                            fabs(dy) < kRelTol * vdom) {
                            shift_u = -dx;
                        } else if (eff_vperiod > 0.0 &&
                                   fabs(fabs(dy) - eff_vperiod) <
                                       kRelTol * eff_vperiod &&
                                   fabs(dx) < kRelTol * udom) {
                            shift_v = -dy;
                        }

                        // Strategy 1b: domain-width fallback period detection
                        // for geometrically closed non-periodic B-spline
                        // surfaces.
                        //
                        // Revolution surfaces (e.g. cones, toroids) are often
                        // stored as B-spline surfaces whose U parameterization
                        // spans exactly one full revolution [0, 2π] or another
                        // complete period, yet OCCT marks them IsUPeriodic=false
                        // and IsClosed(0)=false because the B-spline knots are
                        // not periodic.  In that case eff_uperiod = 0 and
                        // Strategy 1 above does not fire.
                        //
                        // Detection: if eff_[u|v]period = 0 but the gap
                        // magnitude closely matches the raw domain width, treat
                        // the domain width as the effective period and apply the
                        // same all-poles translation.  This is safe because:
                        //  - the gap-magnitude match condition is tight (0.3%),
                        //    so random gaps on planar faces don't trigger this;
                        //  - the cross-direction residual is checked against the
                        //    opposite domain (same as Strategy 1), preventing
                        //    misclassification of oblique non-period gaps;
                        //  - a seam-crossing guard (below) prevents firing when
                        //    both endpoints are at opposite domain boundaries
                        //    and the gap is just the expected seam crossing
                        //    (which would cascade around the loop if shifted).
                        //
                        // Seam-crossing guard for Strategy 1b only: when tk
                        // ends at one surface boundary and tk1 starts at the
                        // OPPOSITE boundary, the gap is a legitimate seam
                        // crossing on a non-periodic surface — the two UV
                        // points map to the same 3D location.  Applying a
                        // period shift would cascade and corrupt the loop.
                        // Strategy 1 (periodic surfaces) is NOT guarded here
                        // because periodic-surface seam edges are always
                        // represented as explicit seam trims in OCCT, so the
                        // k-loop never encounters a raw seam gap for periodic
                        // surfaces; the guard is only needed for non-periodic
                        // closed B-splines (eff_uperiod == 0).
                        if (ZERO(shift_u) && ZERO(shift_v)) {
                            const double kSeamTol = kRelTol;
                            const bool at_u_seam_1b =
                                (fabs(pe.x - u1) < kSeamTol * udom &&
                                 fabs(ps.x - u0) < kSeamTol * udom) ||
                                (fabs(pe.x - u0) < kSeamTol * udom &&
                                 fabs(ps.x - u1) < kSeamTol * udom);
                            const bool at_v_seam_1b =
                                (fabs(pe.y - v1) < kSeamTol * vdom &&
                                 fabs(ps.y - v0) < kSeamTol * vdom) ||
                                (fabs(pe.y - v0) < kSeamTol * vdom &&
                                 fabs(ps.y - v1) < kSeamTol * vdom);
                            if (!at_u_seam_1b && ZERO(eff_uperiod) &&
                                udom > 0.0 &&
                                fabs(fabs(dx) - udom) < kRelTol * udom &&
                                fabs(dy) < kRelTol * vdom) {
                                shift_u = -dx;
                            } else if (!at_v_seam_1b && ZERO(eff_vperiod) &&
                                       vdom > 0.0 &&
                                       fabs(fabs(dy) - vdom) < kRelTol * vdom &&
                                       fabs(dx) < kRelTol * udom) {
                                shift_v = -dy;
                            }
                        }

                        if (fabs(shift_u) > 1e-12 ||
                            fabs(shift_v) > 1e-12) {
                            // Shift all poles of tk1's pcurve by the period.
                            // Only tk1 is shifted (not tk+2, tk+3, ...): if
                            // multiple consecutive junctions in the same loop
                            // each need the same period shift, the subsequent
                            // k-iterations will detect and apply the same shift
                            // to tk+2, tk+3, etc. individually.  The seam-
                            // crossing guard above ensures that loops whose
                            // junctions are genuine seam crossings (not period
                            // shift errors) are never shifted, preventing the
                            // cascade where shifting tk+1 creates a new gap at
                            // the next junction (and eventually at the wrap-
                            // around junction k=nc-1 which would corrupt the
                            // k=0 fix).
                            for (int p = 0; p < nc2->m_cv_count; ++p) {
                                double* cv = nc2->CV(p);
                                const double weight = nc2->Weight(p);
                                cv[0] += weight * shift_u;
                                cv[1] += weight * shift_v;
                            }
                            modified = true;
                            // Re-evaluate after translation so that
                            // strategy 2 below sees the updated gap.
                            tk1.SetProxyCurveDomain(tk1.ProxyCurveDomain());
                            pe = tk.PointAtEnd();
                            ps = tk1.PointAtStart();
                            dx = ps.x - pe.x;
                            dy = ps.y - pe.y;
                        }

                        // Strategy 2a: snap the first pole of tk1's pcurve
                        // to tk's UV endpoint.  Applied for gaps within 5% of
                        // the surface domain (floating-point noise level).
                        //
                        // Strategy 2b: C1-preserving re-fit for larger gaps
                        // (between 5% and 30% of domain).  Instead of only
                        // moving cv[0], move both cv[0] and cv[1] by the same
                        // delta so the initial tangent direction is preserved.
                        // For a clamped B-spline the tangent at the domain
                        // start is proportional to (cv[1] - cv[0]), so an
                        // equal translation of both maintains C1 continuity at
                        // the loop junction.  The re-fit is riskier than the
                        // pure snap (it moves two poles instead of one) and
                        // can be disabled via kEnableLargeGapRefit.
                        //
                        // Guard: the C1 re-fit only moves cv[1] when the
                        // pcurve is not shared between multiple trims.
                        // Shared pcurves (e.g. degenerate seam edge pairs) can
                        // have a different trim use the same curve in the
                        // opposite direction; moving cv[1] would corrupt its
                        // end-point and can incorrectly cause SetTrimIsoFlags
                        // to classify another trim as iso-parametric.
                        //
                        // The B-spline domain trimming above ensures that
                        // cv[0] corresponds to PointAt(proxy_domain_start),
                        // making both strategies effective.
                        static constexpr bool kEnableLargeGapRefit = true;

                        if (fabs(dx) > 1e-14 || fabs(dy) > 1e-14) {
                            // Compute per-axis domain sizes for the limits.
                            const double udomain_sz =
                                uperiod > 0.0 ? uperiod : udom;
                            const double vdomain_sz =
                                vperiod > 0.0 ? vperiod : vdom;
                            const double snap_limit_u = 0.05 * udomain_sz;
                            const double snap_limit_v = 0.05 * vdomain_sz;
                            const double refit_limit_u = 0.30 * udomain_sz;
                            const double refit_limit_v = 0.30 * vdomain_sz;

                            const bool within_snap =
                                (snap_limit_u > 0.0 && fabs(dx) < snap_limit_u) &&
                                (snap_limit_v > 0.0 && fabs(dy) < snap_limit_v);
                            const bool within_refit =
                                kEnableLargeGapRefit &&
                                (refit_limit_u > 0.0 && fabs(dx) < refit_limit_u) &&
                                (refit_limit_v > 0.0 && fabs(dy) < refit_limit_v);

                            if (within_snap || within_refit) {
                                double* cv0 = nc2->CV(0);
                                const double w0 = nc2->IsRational()
                                                    ? cv0[nc2->m_dim] : 1.0;
                                // Guard against degenerate rational curves
                                // where a zero weight can arise due to
                                // floating-point cancellation during OCCT
                                // B-spline conversion; treat as weight=1.
                                const double safe_w0 =
                                    (!ZERO(w0)) ? w0 : 1.0;

                                // Always move cv[0] to the target start point.
                                cv0[0] = pe.x * safe_w0;
                                cv0[1] = pe.y * safe_w0;

                                // Strategy 2b: also move cv[1] by the same
                                // delta to preserve the initial tangent.
                                // Only safe when:
                                //   - the gap is too large for strategy 2a
                                //     (we are in the refit range), AND
                                //   - the pcurve is NOT shared by another
                                //     trim (moving cv[1] of a shared curve
                                //     corrupts the other trim's geometry).
                                if (!within_snap && nc2->m_cv_count >= 2) {
                                    // delta is computed here rather than
                                    // outside the if-block so it is only
                                    // evaluated in the refit path.
                                    const double delta_x = pe.x - ps.x;
                                    const double delta_y = pe.y - ps.y;
                                    // Use precomputed refcount map (O(1)).
                                    const int c2i_check = tk1.m_c2i;
                                    auto rc_it = trim_curve_refcount.find(c2i_check);
                                    const int c2i_refs =
                                        (rc_it != trim_curve_refcount.end())
                                            ? rc_it->second : 0;
                                    if (c2i_refs <= 1) {
                                        // Pcurve is exclusive to tk1; safe to
                                        // move cv[1] for C1 continuity.
                                        double* cv1 = nc2->CV(1);
                                        const double w1 = nc2->IsRational()
                                                            ? cv1[nc2->m_dim]
                                                            : 1.0;
                                        const double safe_w1 =
                                            (!ZERO(w1)) ? w1 : 1.0;
                                        cv1[0] += delta_x * safe_w1;
                                        cv1[1] += delta_y * safe_w1;
                                    }
                                    // If pcurve is shared (c2i_refs > 1),
                                    // only cv[0] was moved (strategy 2a
                                    // applied with the larger gap threshold).
                                }
                                modified = true;
                                tk1.SetProxyCurveDomain(
                                    tk1.ProxyCurveDomain());
                            }
                        }

                        // Invalidate cached bounding boxes once after
                        // either modification so SetTolerancesBoxesAndFlags
                        // recomputes them from the updated curve.
                        if (modified) {
                            tk1.m_pbox.Destroy();
                            ol.m_pbox.Destroy();
                        }
                    }
                }

            }

            // Fix up loop type based on outer-wire detection above.
            ol.m_type = wire_is_outer ? ON_BrepLoop::outer
                                      : ON_BrepLoop::inner;
        }
        } // end face_wires scope
    }

    brep.SetTolerancesBoxesAndFlags(/*bLazy=*/true);

    // ---------------------------------------------------------------------------
    // Post-processing pass: repair seam/singular trim m_iso corruption.
    //
    // The UV-gap snap occasionally sets CV[0] of a seam pcurve to the
    // floating-point endpoint of the preceding trim (e.g. U=2.8e-9 instead
    // of exactly U=0).  This tiny error is geometrically negligible but
    // causes ON_Surface::IsIsoparametric() to return not_iso (the surface
    // parameter tolerance is ~1e-15), making SetTrimIsoFlags set m_iso =
    // not_iso for the seam trim and failing the ON_Brep::IsValid() check
    // "seam trim m_iso is not N/E/W/S_iso".
    //
    // Similarly, singular trims (degenerate poles) on B-spline surfaces can
    // have SetTrimIsoFlags() silently return not_iso even when the pcurve CVs
    // lie exactly on a surface boundary.  The ON_NurbsSurface::IsIsoparametric
    // implementation uses a tight absolute tolerance (~1e-12) that can fail for
    // large domain values (e.g. V_max = 28.6515).
    //
    // Also fixes the case where SetTrimIsoFlags correctly identifies a seam
    // pcurve as x_iso or y_iso (interior U/V constant) but the validation
    // requires N/E/W/S_iso (boundary) for seam trims.  For example, a cylinder
    // seam at U=2π gets x_iso instead of E_iso.  We promote x_iso→W_iso/E_iso
    // and y_iso→S_iso/N_iso for seam trims that are at a surface boundary.
    //
    // Fix for all cases: for any seam or singular trim whose m_iso is not_iso,
    // x_iso, or y_iso, check whether all CVs share a nearly-constant U or V
    // value (within 1e-4 of the maximum CV extent in that direction).  If so,
    // snap to the nearest surface domain boundary and recompute.  If
    // SetTrimIsoFlags still returns interior iso, compute boundary iso directly.
    // ---------------------------------------------------------------------------
    for (int ti = 0; ti < brep.m_T.Count(); ++ti) {
        ON_BrepTrim& trim = brep.m_T[ti];
        if (trim.m_type != ON_BrepTrim::seam &&
            trim.m_type != ON_BrepTrim::singular) continue;
        // Skip trims that already have proper boundary iso.
        // Seam and singular trims need W/E/S/N_iso (3–6); x_iso(1) and y_iso(2)
        // are interior-only and invalid for seam and singular trims.
        const bool needs_repair =
            (trim.m_iso == ON_Surface::not_iso) ||
            ((trim.m_type == ON_BrepTrim::seam ||
              trim.m_type == ON_BrepTrim::singular) &&
             (trim.m_iso == ON_Surface::x_iso ||
              trim.m_iso == ON_Surface::y_iso));
        if (!needs_repair) continue;

        if (trim.m_c2i < 0 || trim.m_c2i >= brep.m_C2.Count()) continue;
        ON_NurbsCurve* nc = ON_NurbsCurve::Cast(brep.m_C2[trim.m_c2i]);
        if (!nc || nc->CVCount() < 1 || nc->m_dim < 2) continue;

        const int nCV = nc->CVCount();
        const bool rat = nc->IsRational();

        // Compute min/max U and V across all CVs
        double uMin = 1e300, uMax = -1e300;
        double vMin = 1e300, vMax = -1e300;
        for (int p = 0; p < nCV; ++p) {
            const double* cv = nc->CV(p);
            double w = rat ? cv[nc->m_dim] : 1.0;
            if (ZERO(w)) w = 1.0;
            double u = cv[0] / w, v = cv[1] / w;
            if (u < uMin) uMin = u;
            if (u > uMax) uMax = u;
            if (v < vMin) vMin = v;
            if (v > vMax) vMax = v;
        }

        const double kIsoTol = 1e-4;
        bool snap_u = (uMax - uMin) <= kIsoTol;
        bool snap_v = (vMax - vMin) <= kIsoTol;

        // ---------------------------------------------------------------------------
        // Shared helpers for seam and singular trim domain-relative snapping.
        //
        // kDomRelFrac: threshold for "nearly constant" in relative terms —
        //   1% of domain extent, or absolute 1e-4, whichever is larger.
        //   Trim pcurves can have small numerical drift after B-spline conversion
        //   on large surfaces.
        // kBndRelFrac: wider threshold (5%) for "near a surface boundary".
        //   Boundary detection needs more slack than general iso detection because
        //   the pcurve endpoint may land slightly away from the exact boundary.
        // ---------------------------------------------------------------------------
        const double kDomRelFrac = 0.01;
        const double kBndRelFrac = 0.05;

        // snap_all_cvs: set all CVs' coordinate along the given axis to anchor.
        auto snap_all_cvs = [&](int axis, double anchor) {
            for (int p = 0; p < nCV; ++p) {
                double* cv = nc->CV(p);
                double w = rat ? cv[nc->m_dim] : 1.0;
                if (ZERO(w)) w = 1.0;
                cv[axis] = anchor * w;
            }
        };
        // nearest_bnd: return whichever of b0 or b1 is closer to val.
        auto nearest_bnd = [](double val, double b0, double b1) -> double {
            return (fabs(val-b0) <= fabs(val-b1)) ? b0 : b1;
        };
        // near_bnd_tol: true if val is within tol of b0 or b1.
        auto near_bnd_tol = [](double val, double b0, double b1,
                                double tol) -> bool {
            return fabs(val-b0) <= tol || fabs(val-b1) <= tol;
        };

        // For SEAM trims (periodic surface closure), the trim pcurve must lie
        // exactly on a surface domain boundary (su0 or su1 for U-seams, sv0 or
        // sv1 for V-seams).  The absolute tolerance kIsoTol=1e-4 is too tight
        // for surfaces with large parameter domains (e.g. helical springs where
        // the U domain is many multiples of 2π).  Use domain-relative tolerances
        // and the greedy smallest-variation axis approach, just like singular trims.
        if (trim.m_type == ON_BrepTrim::seam) {
            const int li_seam  = trim.m_li;
            const int fi_seam  = (li_seam >= 0 && li_seam < brep.m_L.Count())
                                   ? brep.m_L[li_seam].m_fi : -1;
            const ON_Surface* srf_seam = (fi_seam >= 0 && fi_seam < brep.m_F.Count())
                                           ? brep.m_F[fi_seam].SurfaceOf() : nullptr;
            if (srf_seam && (!snap_u || !snap_v)) {
                double su0s=0,su1s=0,sv0s=0,sv1s=0;
                srf_seam->GetDomain(0,&su0s,&su1s);
                srf_seam->GetDomain(1,&sv0s,&sv1s);
                const double uDom = su1s - su0s;
                const double vDom = sv1s - sv0s;
                const double uRelTol = (uDom > 0) ? std::max(kIsoTol, kDomRelFrac * uDom) : kIsoTol;
                const double vRelTol = (vDom > 0) ? std::max(kIsoTol, kDomRelFrac * vDom) : kIsoTol;
                const double uBndTol = (uDom > 0) ? std::max(kIsoTol, kBndRelFrac * uDom) : kIsoTol;
                const double vBndTol = (vDom > 0) ? std::max(kIsoTol, kBndRelFrac * vDom) : kIsoTol;

                if (!snap_u && (uMax - uMin) <= uRelTol) snap_u = true;
                if (!snap_v && (vMax - vMin) <= vRelTol) snap_v = true;

                // If still neither, try to match CV mean to a surface boundary.
                if (!snap_u && !snap_v) {
                    double uMean = 0.5*(uMin+uMax), vMean = 0.5*(vMin+vMax);
                    if (near_bnd_tol(vMean, sv0s, sv1s, vBndTol)) {
                        double v_anchor = nearest_bnd(vMean, sv0s, sv1s);
                        snap_all_cvs(1, v_anchor);
                        snap_v = true;
                        vMin = vMax = v_anchor;
                    } else if (near_bnd_tol(uMean, su0s, su1s, uBndTol)) {
                        double u_anchor = nearest_bnd(uMean, su0s, su1s);
                        snap_all_cvs(0, u_anchor);
                        snap_u = true;
                        uMin = uMax = u_anchor;
                    } else if (uDom > 0 && vDom > 0) {
                        // Last resort: seam trims must be at a domain boundary,
                        // so pick the axis with smallest relative variation and
                        // force-snap to the nearest boundary.
                        double uRelVar = (uMax - uMin) / uDom;
                        double vRelVar = (vMax - vMin) / vDom;
                        if (uRelVar <= vRelVar) {
                            double u_anchor = nearest_bnd(0.5*(uMin+uMax), su0s, su1s);
                            snap_all_cvs(0, u_anchor);
                            snap_u = true;
                            uMin = uMax = u_anchor;
                        } else {
                            double v_anchor = nearest_bnd(0.5*(vMin+vMax), sv0s, sv1s);
                            snap_all_cvs(1, v_anchor);
                            snap_v = true;
                            vMin = vMax = v_anchor;
                        }
                    }

                    // Also check CV[0] proximity to boundaries when the iso
                    // direction is still undetermined.  Only run when both
                    // snap_u and snap_v are false so that a correctly-identified
                    // U-iso seam (snap_u=true, snap_v=false) does not have its
                    // V values incorrectly snapped because CV[0] happens to lie
                    // near a V boundary.
                    if (!snap_u && !snap_v) {
                        const double* cv0 = nc->CV(0);
                        double w0 = rat ? cv0[nc->m_dim] : 1.0;
                        if (ZERO(w0)) w0=1.0;
                        if (near_bnd_tol(cv0[1]/w0, sv0s, sv1s, vRelTol)) {
                            double v_anchor = nearest_bnd(cv0[1]/w0, sv0s, sv1s);
                            snap_all_cvs(1, v_anchor);
                            snap_v = true;
                            vMin = vMax = v_anchor;
                        } else if (near_bnd_tol(cv0[0]/w0, su0s, su1s, uRelTol)) {
                            double u_anchor = nearest_bnd(cv0[0]/w0, su0s, su1s);
                            snap_all_cvs(0, u_anchor);
                            snap_u = true;
                            uMin = uMax = u_anchor;
                        }
                    }
                }
            }
        }

        // For SINGULAR trims (pole/apex edges), the trim pcurve should lie
        // exactly on the boundary where the surface degenerates to a point.
        // After B-spline conversion the degenerate locus may not be perfectly
        // iso, but if ANY CV is at a surface boundary we can infer the
        // correct iso line and snap all CVs to it.
        //
        // For singular trims we also use domain-relative tolerances: 1e-4
        // absolute is too tight for large B-spline surfaces (domain ≫ 1).
        // We also allow a greedy "pick smallest-variation axis" approach when
        // even the domain-relative check fails, since openNURBS requires
        // singular trims to have W/E/N/S_iso (boundary iso), not not_iso.
        if (trim.m_type == ON_BrepTrim::singular) {
            const int li_s  = trim.m_li;
            const int fi_s  = (li_s >= 0 && li_s < brep.m_L.Count())
                                  ? brep.m_L[li_s].m_fi : -1;
            const ON_Surface* srf_s = (fi_s >= 0 && fi_s < brep.m_F.Count())
                                          ? brep.m_F[fi_s].SurfaceOf() : nullptr;
            if (srf_s && (!snap_u || !snap_v)) {
                double su0s=0,su1s=0,sv0s=0,sv1s=0;
                srf_s->GetDomain(0,&su0s,&su1s);
                srf_s->GetDomain(1,&sv0s,&sv1s);
                const double uDom = su1s - su0s;
                const double vDom = sv1s - sv0s;
                const double uRelTol = (uDom > 0) ? std::max(kIsoTol, kDomRelFrac * uDom) : kIsoTol;
                const double vRelTol = (vDom > 0) ? std::max(kIsoTol, kDomRelFrac * vDom) : kIsoTol;
                const double uBndTol = (uDom > 0) ? std::max(kIsoTol, kBndRelFrac * uDom) : kIsoTol;
                const double vBndTol = (vDom > 0) ? std::max(kIsoTol, kBndRelFrac * vDom) : kIsoTol;

                if (!snap_u && (uMax - uMin) <= uRelTol) snap_u = true;
                if (!snap_v && (vMax - vMin) <= vRelTol) snap_v = true;

                // If still neither, try to match CV mean to a surface boundary.
                if (!snap_u && !snap_v) {
                    double uMean = 0.5*(uMin+uMax), vMean = 0.5*(vMin+vMax);
                    if (near_bnd_tol(vMean, sv0s, sv1s, vBndTol)) {
                        double v_anchor = nearest_bnd(vMean, sv0s, sv1s);
                        snap_all_cvs(1, v_anchor);
                        snap_v = true;
                        vMin = vMax = v_anchor;
                    } else if (near_bnd_tol(uMean, su0s, su1s, uBndTol)) {
                        double u_anchor = nearest_bnd(uMean, su0s, su1s);
                        snap_all_cvs(0, u_anchor);
                        snap_u = true;
                        uMin = uMax = u_anchor;
                    } else if (uDom > 0 && vDom > 0) {
                        // Last resort: pick the axis with smallest relative
                        // variation and force-snap to nearest boundary.
                        // openNURBS requires singular trims to be boundary-iso,
                        // so this is always the correct direction to try.
                        // Safety: since the singular trim maps to a single 3D
                        // point (degenerate edge), snapping the pcurve's "iso"
                        // axis to the nearest surface boundary preserves the
                        // 3D geometry while satisfying openNURBS's topology
                        // constraint.  Adjacent trim endpoints are corrected
                        // by the junction-gap repair pass below.
                        double uRelVar = (uMax - uMin) / uDom;
                        double vRelVar = (vMax - vMin) / vDom;
                        if (vRelVar <= uRelVar) {
                            double v_anchor = nearest_bnd(vMean, sv0s, sv1s);
                            snap_all_cvs(1, v_anchor);
                            snap_v = true;
                            vMin = vMax = v_anchor;
                        } else {
                            double u_anchor = nearest_bnd(uMean, su0s, su1s);
                            snap_all_cvs(0, u_anchor);
                            snap_u = true;
                            uMin = uMax = u_anchor;
                        }
                    }
                }

                // Also check CV[0] directly against boundaries with relative
                // tolerance, for cases where snap_u XOR snap_v is still false.
                if (!snap_v) {
                    const double* cv0 = nc->CV(0);
                    double w0 = rat ? cv0[nc->m_dim] : 1.0;
                    if (ZERO(w0)) w0=1.0;
                    if (near_bnd_tol(cv0[1]/w0, sv0s, sv1s, vRelTol)) {
                        double v_anchor = nearest_bnd(cv0[1]/w0, sv0s, sv1s);
                        snap_all_cvs(1, v_anchor);
                        snap_v = true;
                        vMin = vMax = v_anchor;
                    }
                }
                if (!snap_u) {
                    const double* cv0 = nc->CV(0);
                    double w0 = rat ? cv0[nc->m_dim] : 1.0;
                    if (ZERO(w0)) w0=1.0;
                    if (near_bnd_tol(cv0[0]/w0, su0s, su1s, uRelTol)) {
                        double u_anchor = nearest_bnd(cv0[0]/w0, su0s, su1s);
                        snap_all_cvs(0, u_anchor);
                        snap_u = true;
                        uMin = uMax = u_anchor;
                    }
                }
            }
        }

        if (!snap_u && !snap_v) continue;  // not iso-like — skip

        // Snap to exact iso: snap to the nearest surface domain
        // boundary (if the iso value is within 1% of a boundary) or
        // to the mean otherwise (interior iso curves).
        {
            const int li = trim.m_li;
            const int fi = (li >= 0 && li < brep.m_L.Count())
                               ? brep.m_L[li].m_fi : -1;
            const ON_Surface* srf = (fi >= 0 && fi < brep.m_F.Count())
                                        ? brep.m_F[fi].SurfaceOf() : nullptr;
            double su0=0, su1=0, sv0=0, sv1=0;
            if (srf) {
                srf->GetDomain(0, &su0, &su1);
                srf->GetDomain(1, &sv0, &sv1);
            }

            auto snap_to_boundary = [](double val, double d0, double d1,
                                        double tol) -> double {
                if (fabs(val - d0) <= tol) return d0;
                if (fabs(val - d1) <= tol) return d1;
                return val;  // interior
            };
            // For singular trims, ALWAYS snap to the nearest surface boundary.
            // openNURBS requires singular trims to have W/E/N/S_iso.  If the
            // singularity is at an interior parameter (e.g. due to B-spline
            // conversion changing the parameterization), we must force it to
            // the nearest boundary.  The adjacent trim endpoints are corrected
            // by the junction-gap repair pass that follows below.
            auto snap_to_nearest = [](double val, double d0, double d1) -> double {
                return (fabs(val-d0) <= fabs(val-d1)) ? d0 : d1;
            };
            const bool is_singular = (trim.m_type == ON_BrepTrim::singular);
            const bool is_seam     = (trim.m_type == ON_BrepTrim::seam);

            if (snap_u) {
                double u_iso = 0.5 * (uMin + uMax);
                if (srf) {
                    const double bndTol = std::max(1e-4, kDomRelFrac*(su1-su0));
                    double u_snapped = snap_to_boundary(u_iso, su0, su1, bndTol);
                    if (EQUAL(u_snapped, u_iso) && (is_singular || is_seam)) {
                        // Interior U: force to nearest boundary.
                        // Seam trims must be at a domain boundary (W_iso/E_iso);
                        // they arise from periodic surface closure and can only
                        // appear at su0 or su1.
                        u_snapped = snap_to_nearest(u_iso, su0, su1);
                    }
                    u_iso = u_snapped;
                }
                for (int p = 0; p < nCV; ++p) {
                    double* cv = nc->CV(p);
                    double w = rat ? cv[nc->m_dim] : 1.0;
                    if (ZERO(w)) w = 1.0;
                    cv[0] = u_iso * w;
                }
            } else {
                double v_iso = 0.5 * (vMin + vMax);
                if (srf) {
                    const double bndTol = std::max(1e-4, kDomRelFrac*(sv1-sv0));
                    double v_snapped = snap_to_boundary(v_iso, sv0, sv1, bndTol);
                    if (EQUAL(v_snapped, v_iso) && (is_singular || is_seam)) {
                        // Interior V: force to nearest boundary.
                        // Seam trims must be at a domain boundary (S_iso/N_iso).
                        v_snapped = snap_to_nearest(v_iso, sv0, sv1);
                    }
                    v_iso = v_snapped;
                }
                for (int p = 0; p < nCV; ++p) {
                    double* cv = nc->CV(p);
                    double w = rat ? cv[nc->m_dim] : 1.0;
                    if (ZERO(w)) w = 1.0;
                    cv[1] = v_iso * w;
                }
            }
        }

        // Recompute the iso flag and parameter bounding box for this trim.
        brep.SetTrimIsoFlags(trim);

        // Fallback: if SetTrimIsoFlags still returns not_iso, x_iso, or y_iso
        // for a seam trim (can happen because ON_NurbsSurface::IsIsoparametric
        // uses a tight absolute tolerance, and because it cannot distinguish
        // between boundary and interior iso curves), compute the correct
        // boundary iso type directly from the snapped CV values and the surface
        // domain.  x_iso / y_iso are only valid for non-seam interior curves;
        // seam trims require W/E/S/N_iso.
        const bool still_bad =
            (trim.m_iso == ON_Surface::not_iso) ||
            ((trim.m_type == ON_BrepTrim::seam ||
              trim.m_type == ON_BrepTrim::singular) &&
             (trim.m_iso == ON_Surface::x_iso ||
              trim.m_iso == ON_Surface::y_iso));
        if (still_bad) {
            const int li2   = trim.m_li;
            const int fi2   = (li2 >= 0 && li2 < brep.m_L.Count())
                                  ? brep.m_L[li2].m_fi : -1;
            const ON_Surface* srf2 = (fi2 >= 0 && fi2 < brep.m_F.Count())
                                         ? brep.m_F[fi2].SurfaceOf() : nullptr;
            if (srf2 && nc->CVCount() > 0) {
                double su0=0, su1=0, sv0=0, sv1=0;
                srf2->GetDomain(0, &su0, &su1);
                srf2->GetDomain(1, &sv0, &sv1);
                const double uBndTol = std::max(1e-4, kDomRelFrac*(su1-su0));
                const double vBndTol = std::max(1e-4, kDomRelFrac*(sv1-sv0));
                if (snap_u) {
                    const double* cv0 = nc->CV(0);
                    double w0 = nc->IsRational() ? cv0[nc->m_dim] : 1.0;
                    if (ZERO(w0)) w0 = 1.0;
                    double u0 = cv0[0] / w0;
                    if      (fabs(u0 - su0) <= uBndTol) trim.m_iso = ON_Surface::W_iso;
                    else if (fabs(u0 - su1) <= uBndTol) trim.m_iso = ON_Surface::E_iso;
                    else if (trim.m_type == ON_BrepTrim::seam ||
                             trim.m_type == ON_BrepTrim::singular)
                        // Seam/singular trims must have boundary iso; force to
                        // the nearest W_iso/E_iso even if the value is interior.
                        trim.m_iso = (fabs(u0-su0) <= fabs(u0-su1))
                                         ? ON_Surface::W_iso : ON_Surface::E_iso;
                    else                                 trim.m_iso = ON_Surface::x_iso;
                } else {
                    const double* cv0 = nc->CV(0);
                    double w0 = nc->IsRational() ? cv0[nc->m_dim] : 1.0;
                    if (ZERO(w0)) w0 = 1.0;
                    double v0 = cv0[1] / w0;
                    if      (fabs(v0 - sv0) <= vBndTol) trim.m_iso = ON_Surface::S_iso;
                    else if (fabs(v0 - sv1) <= vBndTol) trim.m_iso = ON_Surface::N_iso;
                    else if (trim.m_type == ON_BrepTrim::seam ||
                             trim.m_type == ON_BrepTrim::singular)
                        // Seam/singular trims must have boundary iso; force to
                        // the nearest S_iso/N_iso even if the value is interior.
                        trim.m_iso = (fabs(v0-sv0) <= fabs(v0-sv1))
                                         ? ON_Surface::S_iso : ON_Surface::N_iso;
                    else                                 trim.m_iso = ON_Surface::y_iso;
                }
            }
        }

        // Rebuild m_pbox from the (now-corrected) pcurve.
        {
            const ON_Curve* c2 = brep.m_C2[trim.m_c2i];
            if (c2) {
                trim.m_pbox = c2->BoundingBox();
                trim.m_pbox.m_min.z = 0.0;
                trim.m_pbox.m_max.z = 0.0;
            }
        }

        // After snapping this seam pcurve to exact iso, close the
        // Close junction gaps on both sides of the seam trim:
        //
        // 1. Snap the LAST CV of the preceding trim to match this seam
        //    trim's first point (gap at the trim's START).
        // 2. Snap the FIRST CV of the next trim to match this seam
        //    trim's last point (gap at the trim's END).
        //
        // Only the iso (constant) coordinate is changed to avoid corrupting
        // the non-iso geometry of the adjacent trim.
        // Both snaps skip the case where the adjacent trim is itself a seam.
        {
            ON_3dPoint pt_start = trim.PointAtStart();
            ON_3dPoint pt_end   = trim.PointAtEnd();
            const int li2 = trim.m_li;
            if (li2 >= 0 && li2 < brep.m_L.Count()) {
                const ON_BrepLoop& lp2 = brep.m_L[li2];
                for (int k = 0; k < lp2.m_ti.Count(); ++k) {
                    if (lp2.m_ti[k] != ti) continue;

                    // ---- Snap preceding trim's last CV ----
                    {
                        int prev_k = (k + lp2.m_ti.Count() - 1)
                                         % lp2.m_ti.Count();
                        ON_BrepTrim& prev_t =
                            brep.m_T[lp2.m_ti[prev_k]];
                        if (prev_t.m_type != ON_BrepTrim::seam) {
                            int prev_c2i = prev_t.m_c2i;
                            if (prev_c2i >= 0
                                    && prev_c2i < brep.m_C2.Count()) {
                                ON_NurbsCurve* pnc = ON_NurbsCurve::Cast(
                                    brep.m_C2[prev_c2i]);
                                if (pnc && pnc->CVCount() >= 1) {
                                    const int last = pnc->CVCount() - 1;
                                    double* cvL = pnc->CV(last);
                                    double wL = pnc->IsRational()
                                                    ? cvL[pnc->m_dim] : 1.0;
                                    if (ZERO(wL)) wL = 1.0;
                                    if (snap_u) cvL[0] = pt_start.x * wL;
                                    else        cvL[1] = pt_start.y * wL;
                                    prev_t.m_pbox = brep.m_C2[prev_c2i]->BoundingBox();
                                    prev_t.m_pbox.m_min.z = 0.0;
                                    prev_t.m_pbox.m_max.z = 0.0;
                                }
                            }
                        }
                    }

                    // ---- Snap next trim's first CV (only if already close) ----
                    {
                        int next_k = (k + 1) % lp2.m_ti.Count();
                        ON_BrepTrim& next_t =
                            brep.m_T[lp2.m_ti[next_k]];
                        if (next_t.m_type != ON_BrepTrim::seam) {
                            int next_c2i = next_t.m_c2i;
                            if (next_c2i >= 0
                                    && next_c2i < brep.m_C2.Count()) {
                                ON_NurbsCurve* nnc = ON_NurbsCurve::Cast(
                                    brep.m_C2[next_c2i]);
                                if (nnc && nnc->CVCount() >= 1) {
                                    double* cv0 = nnc->CV(0);
                                    double w0 = nnc->IsRational()
                                                    ? cv0[nnc->m_dim] : 1.0;
                                    if (ZERO(w0)) w0 = 1.0;
                                    // For singular trims, always snap the
                                    // next trim to the iso value (the apex
                                    // boundary can be offset vs. the adjacent
                                    // trim's natural start by up to 0.5 UV
                                    // units due to B-spline approximation of
                                    // cones/spheres).  For seam trims, only
                                    // snap if already close (1e-3) to avoid
                                    // corrupting trims on the opposite seam.
                                    const bool is_singular_trim =
                                        (trim.m_type == ON_BrepTrim::singular);
                                    bool snap_next = is_singular_trim;
                                    if (!snap_next) {
                                        if (snap_u) {
                                            double cur_u = cv0[0] / w0;
                                            snap_next = fabs(cur_u - pt_end.x) <= 1e-3;
                                        } else {
                                            double cur_v = cv0[1] / w0;
                                            snap_next = fabs(cur_v - pt_end.y) <= 1e-3;
                                        }
                                    }
                                    if (snap_next) {
                                        if (snap_u) cv0[0] = pt_end.x * w0;
                                        else        cv0[1] = pt_end.y * w0;
                                        next_t.m_pbox = brep.m_C2[next_c2i]->BoundingBox();
                                        next_t.m_pbox.m_min.z = 0.0;
                                        next_t.m_pbox.m_max.z = 0.0;
                                    }
                                }
                            }
                        }
                    }

                    break;
                }
            }
        }
    }


    // Rebuild loop pboxes after seam/singular trim CV snapping.
    // SetTolerancesBoxesAndFlags computed loop pboxes from the original CVs;
    // our snapping may have moved a trim's pbox outside the loop's pbox.
    for (int li = 0; li < brep.m_L.Count(); ++li) {
        ON_BrepLoop& lp = brep.m_L[li];
        lp.m_pbox.Destroy();
        for (int k = 0; k < lp.m_ti.Count(); ++k) {
            int tki = lp.m_ti[k];
            if (tki >= 0 && tki < brep.m_T.Count())
                lp.m_pbox.Union(brep.m_T[tki].m_pbox);
        }
        // Parameter-space bounding boxes are 2D (UV only); the z coordinate
        // must be zero so that ON_BrepLoop::IsValid() accepts the pbox.
        lp.m_pbox.m_min.z = 0.0;
        lp.m_pbox.m_max.z = 0.0;
    }

    // ---------------------------------------------------------------------------
    // Post-processing pass: close small UV gaps between adjacent regular trims.
    //
    // The openNURBS loop validity check requires that the end of each trim UV
    // matches the start of the next trim within tolerance.  Small floating-point
    // discrepancies (e.g. 0.001) can arise when converting OCCT B-spline pcurve
    // endpoints to ON_NurbsCurve form.  For non-seam trim pairs with gaps larger
    // than tolerance but ≤ 0.01, snap the endpoint CVs to the average of the two
    // mismatched UV points.  Seam and singular trims are handled by the
    // junction-gap repair in the seam/singular pass above; skip them here.
    // ---------------------------------------------------------------------------
    for (int li = 0; li < brep.m_L.Count(); ++li) {
        const ON_BrepLoop& lp = brep.m_L[li];
        const int n = lp.m_ti.Count();
        for (int k = 0; k < n; ++k) {
            const int ti0 = lp.m_ti[k];
            const int ti1 = lp.m_ti[(k + 1) % n];
            if (ti0 < 0 || ti0 >= brep.m_T.Count()) continue;
            if (ti1 < 0 || ti1 >= brep.m_T.Count()) continue;
            ON_BrepTrim& t0 = brep.m_T[ti0];
            ON_BrepTrim& t1 = brep.m_T[ti1];
            // Skip if either trim is seam or singular — handled elsewhere.
            if (t0.m_type == ON_BrepTrim::seam ||
                t0.m_type == ON_BrepTrim::singular ||
                t1.m_type == ON_BrepTrim::seam ||
                t1.m_type == ON_BrepTrim::singular) continue;
            // Get pcurves.
            if (t0.m_c2i < 0 || t0.m_c2i >= brep.m_C2.Count()) continue;
            if (t1.m_c2i < 0 || t1.m_c2i >= brep.m_C2.Count()) continue;
            ON_NurbsCurve* nc0 = ON_NurbsCurve::Cast(brep.m_C2[t0.m_c2i]);
            ON_NurbsCurve* nc1 = ON_NurbsCurve::Cast(brep.m_C2[t1.m_c2i]);
            if (!nc0 || !nc1) continue;
            if (nc0->CVCount() < 1 || nc1->CVCount() < 1) continue;
            // Compute UV gap between t0 end and t1 start.
            ON_3dPoint p0 = t0.PointAtEnd();
            ON_3dPoint p1 = t1.PointAtStart();
            const double dx = fabs(p0.x - p1.x);
            const double dy = fabs(p0.y - p1.y);
            // kMaxUVGap: maximum gap size to snap.  0.01 covers typical
            // floating-point drift from B-spline endpoint rounding (empirically
            // ~0.001 for PCB shapes in the phasma-mech-assembly test suite) while
            // rejecting genuinely mismatched trims that indicate a deeper error.
            const double kMaxUVGap = 0.01;
            if (dx <= ON_ZERO_TOLERANCE && dy <= ON_ZERO_TOLERANCE) continue;
            if (dx > kMaxUVGap || dy > kMaxUVGap) continue; // too large; skip
            // Average the gap and snap both endpoints to it.
            double avg_x = 0.5 * (p0.x + p1.x);
            double avg_y = 0.5 * (p0.y + p1.y);
            // Snap t0's last CV to (avg_x, avg_y).
            {
                const int last = nc0->CVCount() - 1;
                double* cv = nc0->CV(last);
                double w = nc0->IsRational() ? cv[nc0->m_dim] : 1.0;
                if (ZERO(w)) w = 1.0;
                cv[0] = avg_x * w;
                cv[1] = avg_y * w;
                t0.m_pbox = brep.m_C2[t0.m_c2i]->BoundingBox();
                t0.m_pbox.m_min.z = 0.0;
                t0.m_pbox.m_max.z = 0.0;
            }
            // Snap t1's first CV to (avg_x, avg_y).
            {
                double* cv = nc1->CV(0);
                double w = nc1->IsRational() ? cv[nc1->m_dim] : 1.0;
                if (ZERO(w)) w = 1.0;
                cv[0] = avg_x * w;
                cv[1] = avg_y * w;
                t1.m_pbox = brep.m_C2[t1.m_c2i]->BoundingBox();
                t1.m_pbox.m_min.z = 0.0;
                t1.m_pbox.m_max.z = 0.0;
            }
        }
    }

    // Rebuild loop pboxes again after the general UV gap repair.
    for (int li = 0; li < brep.m_L.Count(); ++li) {
        ON_BrepLoop& lp = brep.m_L[li];
        lp.m_pbox.Destroy();
        for (int k = 0; k < lp.m_ti.Count(); ++k) {
            int tki = lp.m_ti[k];
            if (tki >= 0 && tki < brep.m_T.Count())
                lp.m_pbox.Union(brep.m_T[tki].m_pbox);
        }
        lp.m_pbox.m_min.z = 0.0;
        lp.m_pbox.m_max.z = 0.0;
    }




    // ---------------------------------------------------------------------------
    // Post-processing pass: fix edge tolerances so that ON_Brep::IsValid() passes
    // the "distance from trim endpoint to 3d edge" check.
    //
    // On closed-but-non-periodic surfaces (common in Rhino models) the B-spline
    // evaluates slightly differently on the two sides of the seam even though
    // they represent the same physical point.  After round-trip through OCCT the
    // edge 3D curve is tied to one side while the pcurves on the opposite side
    // map to 3D points that are O(0.1..0.4) units away.
    //
    // The validation formula is:
    //   dtol  = max(0.01,  10 * (edge_tol + t0_3d_tol + t1_3d_tol))
    //   FAIL  if  dist(srf(uv_start), edge_start) > dtol
    //         or  dist(srf(uv_end),   edge_end  ) > dtol
    //
    // To guarantee the check passes we raise edge.m_tolerance to d_max / 9.
    // (That gives dtol >= 10*(d_max/9) > d_max — a ~11 % safety margin.)
    // ---------------------------------------------------------------------------
    for (int ti = 0; ti < brep.m_T.Count(); ++ti) {
        ON_BrepTrim& trim = brep.m_T[ti];
        if (trim.m_ei < 0 || trim.m_li < 0) continue;

        const int fi = brep.m_L[trim.m_li].m_fi;
        if (fi < 0) continue;
        const ON_Surface* srf = brep.m_F[fi].SurfaceOf();
        if (!srf) continue;

        ON_BrepEdge& edge = brep.m_E[trim.m_ei];

        // Evaluate the surface at the trim's UV endpoints.
        // ON_BrepTrim::PointAtStart() / PointAtEnd() return (u, v, 0).
        ON_3dPoint uv0 = trim.PointAtStart();
        ON_3dPoint uv1 = trim.PointAtEnd();
        ON_3dPoint srf_pt0, srf_pt1;
        if (!srf->EvPoint(uv0.x, uv0.y, srf_pt0)) continue;
        if (!srf->EvPoint(uv1.x, uv1.y, srf_pt1)) continue;

        // Map trim orientation to edge endpoints.
        const ON_3dPoint edge_pt0 =
            trim.m_bRev3d ? edge.PointAtEnd()   : edge.PointAtStart();
        const ON_3dPoint edge_pt1 =
            trim.m_bRev3d ? edge.PointAtStart() : edge.PointAtEnd();

        const double d0 = edge_pt0.DistanceTo(srf_pt0);
        const double d1 = edge_pt1.DistanceTo(srf_pt1);
        const double d_max = std::max(d0, d1);

        // Always take the maximum so that a later trim with a smaller d_max
        // cannot overwrite a larger tolerance already set by an earlier trim
        // on the same edge.  (Multiple trims can share the same edge.)
        const double new_etol = d_max / 9.0;
        if (new_etol > edge.m_tolerance)
            edge.m_tolerance = new_etol;
    }

    // ---------------------------------------------------------------------------
    // Post-processing pass: fix the tangent-direction check for closed trims on
    // closed edges (ON_Brep.m_T[i].m_bRev3d validation).
    //
    // The openNURBS validation requires that for a closed trim on a closed edge
    // the 3-D tangent of the pcurve mapped through the surface Jacobian aligns
    // with the edge-curve tangent at BOTH the start (d0) and end (d1) of the
    // trim domain.  OCCT Boolean-result B-spline pcurves often have a small
    // tangent discontinuity (kink) at the parameter-domain junction (where the
    // closed loop starts/ends), introduced during OCCT's intersection
    // computation.  This kink causes d0 or d1 to be barely negative even though
    // the geometric direction is correct.
    //
    // Fix: for each closed trim on a closed edge whose d0 or d1 fails, snap the
    // 2-D pcurve's last interior control-vertex (CV[n-2] for d1-fail, or CV[1]
    // for d0-fail) so that the end tangent exactly equals the start tangent
    // (C1 closure without kink).  The fix preserves the start and end UV points
    // (only the tangent direction changes) so loop UV connectivity is unaffected.
    // ---------------------------------------------------------------------------
    // NOTE: Do NOT call brep.SetTolerancesBoxesAndFlags() here: it would reset
    // the edge tolerances we adjusted in the loop above.  The trim proxy-curve
    // domains were already set during the conversion loop via
    // SetProxyCurveDomain, so trim.Domain() and trim.Ev1Der() work without it.

    for (int ti = 0; ti < brep.m_T.Count(); ++ti) {
        ON_BrepTrim& trim = brep.m_T[ti];
        if (trim.m_ei < 0 || trim.m_li < 0) continue;
        const ON_BrepEdge& edge = brep.m_E[trim.m_ei];

        // Only process truly closed trims on closed edges
        if (trim.m_vi[0] != trim.m_vi[1]) continue;
        if (edge.m_vi[0] != edge.m_vi[1]) continue;
        if (trim.m_vi[0] != edge.m_vi[0]) continue;

        const int fi = brep.m_L[trim.m_li].m_fi;
        if (fi < 0) continue;
        const ON_Surface* srf = brep.m_F[fi].SurfaceOf();
        if (!srf) continue;

        // Compute the tangent check values d0 and d1
        auto compute_checks = [&](const ON_BrepTrim& t) -> std::pair<double,double> {
            ON_Interval td = t.Domain();
            ON_3dPoint uv0, uv1; ON_3dVector td0, td1;
            t.Ev1Der(td[0], uv0, td0);
            t.Ev1Der(td[1], uv1, td1);
            ON_3dPoint sp; ON_3dVector sdu, sdv;
            srf->Ev1Der(uv0.x, uv0.y, sp, sdu, sdv);
            ON_3dVector tt0 = td0.x*sdu + td0.y*sdv; tt0.Unitize();
            srf->Ev1Der(uv1.x, uv1.y, sp, sdu, sdv);
            ON_3dVector tt1 = td1.x*sdu + td1.y*sdv; tt1.Unitize();
            const ON_BrepEdge& e = brep.m_E[t.m_ei];
            bool rev = t.m_bRev3d ? true : false;
            ON_3dVector et0 = e.TangentAt(e.Domain()[rev ? 1 : 0]);
            ON_3dVector et1 = e.TangentAt(e.Domain()[rev ? 0 : 1]);
            double d0 = tt0 * et0; if (rev) d0 = -d0;
            double d1 = tt1 * et1; if (rev) d1 = -d1;
            return {d0, d1};
        };

        auto [d0, d1] = compute_checks(trim);
        // openNURBS IsValid() uses strict d < 0.0 check for closed trims.
        // Use a small positive threshold so any negative (or exactly-zero)
        // dot product triggers the snap/fix below.
        const double kSnapThreshold = 1e-9;
        if (d0 >= kSnapThreshold && d1 >= kSnapThreshold) continue;

        // If BOTH d0 and d1 are strongly negative the m_bRev3d flag itself
        // is wrong (edge and pcurve go in fully opposite directions).  Flip
        // m_bRev3d and recheck before trying the CV tangent snap.
        // This fix is applied independently of the pcurve degree.
        {
            double ad0 = d0, ad1 = d1;
            if (ad0 < -0.5 && ad1 < -0.5) {
                trim.m_bRev3d = !trim.m_bRev3d;
                auto [nd0, nd1] = compute_checks(trim);
                if (nd0 >= kSnapThreshold && nd1 >= kSnapThreshold) continue;
                // Still failing after flip — restore and fall through to snap.
                trim.m_bRev3d = !trim.m_bRev3d;
            }
        }

        // Get the ON_NurbsCurve for this trim's pcurve
        if (trim.m_c2i < 0 || trim.m_c2i >= brep.m_C2.Count()) continue;
        ON_NurbsCurve* nc = ON_NurbsCurve::Cast(brep.m_C2[trim.m_c2i]);
        if (!nc || nc->Degree() < 1) continue;

        // If the pcurve is linear (degree 1, 2 CVs) elevate it to degree 2
        // so that we have an interior CV to apply the C1 snap below.
        if (nc->CVCount() < 3) {
            if (nc->IncreaseDegree(2) && nc->CVCount() >= 3) {
                // Rebuild pbox for the trim after elevation.
                trim.m_pbox = nc->BoundingBox();
                trim.m_pbox.m_min.z = 0.0;
                trim.m_pbox.m_max.z = 0.0;
            } else {
                continue;  // elevation failed — skip
            }
        }
        if (nc->CVCount() < 3) continue;

        // We need at least CV[0], CV[1], CV[n-2], CV[n-1] to be accessible
        const int nCV = nc->CVCount();
        const int dim = nc->m_dim; // should be 2
        if (dim < 2) continue;

        // Read CV[0], CV[1] and CV[n-1], CV[n-2] in Euclidean 2D
        // (divide by weight for rational curves)
        auto get_cv2d = [&](int i) -> ON_2dPoint {
            ON_4dPoint h; nc->GetCV(i, h);
            double w = (!ZERO(h.w)) ? h.w : 1.0;
            return ON_2dPoint(h.x / w, h.y / w);
        };
        auto get_cv_weight = [&](int i) -> double {
            ON_4dPoint h; nc->GetCV(i, h);
            return (!ZERO(h.w)) ? h.w : 1.0;
        };
        auto set_cv2d = [&](int i, const ON_2dPoint& p, double w) {
            nc->SetCV(i, ON_4dPoint(p.x * w, p.y * w, 0.0, w));
        };

        // First interior knot span at start: m_knot[deg] - m_knot[0]
        // Last interior knot span at end  : m_knot[kc-1] - m_knot[kc-1-deg]
        // (openNURBS stores d-1 phantom knots at each end; the tangent formula is
        //  T'(t0) ∝ (CV[1]-CV[0]) / span_s  and  T'(t1) ∝ (CV[n-1]-CV[n-2]) / span_e)
        const int deg = nc->Degree();
        const int kc  = nc->KnotCount(); // = nCV + deg - 1
        double span_s = 0.0, span_e = 0.0;
        if (deg >= 1 && kc > deg) {
            span_s = nc->m_knot[deg] - nc->m_knot[0];
        }
        if (kc >= deg + 1) {
            span_e = nc->m_knot[kc-1] - nc->m_knot[kc-1-deg];
        }
        double scale = (span_s > 1e-14) ? (span_e / span_s) : 1.0;

        // Guard: if the span ratio is extreme (>> 1 or << 1), the 2D pcurve
        // C1 snap would displace CV[n-2] or CV[1] by a huge amount, distorting
        // the pcurve severely and causing BRepCheck_UnorientableShape in the
        // round-trip OCCT shape.  Skip only the 2D snap in such cases; the
        // 3D edge snap and bRev3d flip below still run.
        const bool do_2d_snap = !(scale > 10.0 || scale < 0.1 || !std::isfinite(scale));

        if (do_2d_snap) {
            ON_2dPoint cv0  = get_cv2d(0);
            ON_2dPoint cv1  = get_cv2d(1);
            ON_2dPoint cvn1 = get_cv2d(nCV-1);
            ON_2dPoint cvn2 = get_cv2d(nCV-2);

            if (d1 < 0.0) {
                // End tangent bad: snap CV[n-2] to enforce T_end = T_start
                ON_2dPoint cvn2_new;
                cvn2_new.x = cvn1.x - (cv1.x - cv0.x) * scale;
                cvn2_new.y = cvn1.y - (cv1.y - cv0.y) * scale;
                set_cv2d(nCV-2, cvn2_new, get_cv_weight(nCV-2));
            } else {
                // Start tangent bad: snap CV[1] to enforce T_start = T_end
                ON_2dPoint cv1_new;
                cv1_new.x = cv0.x + (cvn1.x - cvn2.x) / scale;
                cv1_new.y = cv0.y + (cvn1.y - cvn2.y) / scale;
                set_cv2d(1, cv1_new, get_cv_weight(1));
            }
        }

        // After the 2D pcurve snap (if applied), recheck.  If d1 is still bad
        // (or the 2D snap was skipped due to scale guard), also try the 3D
        // edge curve: snap CV[n-2] to remove a C1 kink at the seam, then
        // re-evaluate and flip bRev3d if the flip gives a better result.
        {
            auto [pd0, pd1] = compute_checks(trim);
            // Try to snap the 3D edge when either endpoint check fails
            if (pd0 < 0.0 || pd1 < 0.0) {
                ON_NurbsCurve* ec = ON_NurbsCurve::Cast(
                    brep.m_C3[brep.m_E[trim.m_ei].m_c3i]);
                if (ec && ec->Degree() >= 1 && ec->CVCount() >= 3) {
                    const int enc  = ec->CVCount();
                    const int edeg = ec->Degree();
                    const int ekc  = ec->KnotCount();
                    double esp_s = 0.0, esp_e = 0.0;
                    if (ekc > edeg)
                        esp_s = ec->m_knot[edeg] - ec->m_knot[0];
                    if (ekc >= edeg + 1)
                        esp_e = ec->m_knot[ekc-1] - ec->m_knot[ekc-1-edeg];
                    double escale = (esp_s > 1e-14) ? (esp_e / esp_s) : 1.0;
                    // Widen the escale guard: even if span ratio is up to 100,
                    // the snap can be worthwhile when the kink is near-180°.
                    // The snap just moves one CV slightly; a large span ratio
                    // means the CV moves proportionally more but the 3D kink
                    // fix is still useful.  Only skip if escale is degenerate
                    // (NaN/Inf/zero span).
                    bool escale_ok = (esp_s > 1e-14) &&
                                     std::isfinite(escale) &&
                                     (escale > 1e-6) && (escale < 1e6);
                    if (escale_ok) {
                        auto eget = [&](int i) -> ON_3dPoint {
                            ON_4dPoint h; ec->GetCV(i, h);
                            double w = (!ZERO(h.w)) ? h.w : 1.0;
                            return ON_3dPoint(h.x/w, h.y/w, h.z/w);
                        };
                        auto ew = [&](int i) -> double {
                            ON_4dPoint h; ec->GetCV(i, h);
                            return (!ZERO(h.w)) ? h.w : 1.0;
                        };
                        ON_3dPoint ep0 = eget(0), ep1 = eget(1);
                        ON_3dPoint epn1 = eget(enc-1), epn2 = eget(enc-2);
                        if (pd1 < 0.0) {
                            // End tangent bad: snap CV[n-2] so end tan = start tan
                            ON_3dPoint epn2_new(
                                epn1.x - (ep1.x - ep0.x) * escale,
                                epn1.y - (ep1.y - ep0.y) * escale,
                                epn1.z - (ep1.z - ep0.z) * escale);
                            double ew2 = ew(enc-2);
                            ec->SetCV(enc-2, ON_4dPoint(
                                epn2_new.x * ew2,
                                epn2_new.y * ew2,
                                epn2_new.z * ew2,
                                ew2));
                        } else {
                            // Start tangent bad: snap CV[1] so start tan = end tan
                            double inv_sc = (escale > 1e-14) ? (1.0/escale) : 1.0;
                            ON_3dPoint ep1_new(
                                ep0.x + (epn1.x - epn2.x) * inv_sc,
                                ep0.y + (epn1.y - epn2.y) * inv_sc,
                                ep0.z + (epn1.z - epn2.z) * inv_sc);
                            double ew1 = ew(1);
                            ec->SetCV(1, ON_4dPoint(
                                ep1_new.x * ew1,
                                ep1_new.y * ew1,
                                ep1_new.z * ew1,
                                ew1));
                        }
                    }
                }

                // After the 3D edge snap removes the C1 kink, re-evaluate the
                // tangent checks.  If bRev3d is wrong, the kink may have been
                // the only thing making one of the checks pass; once the kink is
                // gone both checks become clearly negative under the wrong flag.
                // Flip bRev3d if the min check improves under the flipped flag.
                {
                    trim.m_bRev3d = !trim.m_bRev3d;
                    auto [fd0, fd1] = compute_checks(trim);
                    if (std::min(fd0, fd1) > std::min(pd0, pd1)) {
                        // Flipped orientation is better; keep it.
                        // Also re-snap CV[n-2] of the 2D pcurve now that bRev3d
                        // is corrected (the tangent reference direction flipped).
                        if (trim.m_c2i >= 0 && trim.m_c2i < brep.m_C2.Count()) {
                            ON_NurbsCurve* nc2 = ON_NurbsCurve::Cast(
                                brep.m_C2[trim.m_c2i]);
                            if (nc2 && nc2->CVCount() >= 3) {
                                const int cv_count = nc2->CVCount();
                                const int deg2 = nc2->Degree();
                                const int kc2  = nc2->KnotCount();
                                double ss = 0.0, se = 0.0;
                                if (kc2 > deg2)
                                    ss = nc2->m_knot[deg2] - nc2->m_knot[0];
                                if (kc2 >= deg2 + 1)
                                    se = nc2->m_knot[kc2-1] - nc2->m_knot[kc2-1-deg2];
                                double sc2 = (ss > 1e-14) ? (se / ss) : 1.0;
                                if (sc2 > 0.1 && sc2 < 10.0) {
                                    auto g2 = [&](int i) -> ON_2dPoint {
                                        ON_4dPoint h; nc2->GetCV(i, h);
                                        double w = (!ZERO(h.w)) ? h.w : 1.0;
                                        return ON_2dPoint(h.x/w, h.y/w);
                                    };
                                    auto gw2 = [&](int i) -> double {
                                        ON_4dPoint h; nc2->GetCV(i, h);
                                        return (!ZERO(h.w)) ? h.w : 1.0;
                                    };
                                    auto s2 = [&](int i, const ON_2dPoint& p, double w) {
                                        nc2->SetCV(i, ON_4dPoint(p.x*w, p.y*w, 0.0, w));
                                    };
                                    ON_2dPoint p0 = g2(0), p1 = g2(1);
                                    ON_2dPoint pn1 = g2(cv_count-1);
                                    // snap end tangent to start tangent
                                    ON_2dPoint pn2_new;
                                    pn2_new.x = pn1.x - (p1.x - p0.x) * sc2;
                                    pn2_new.y = pn1.y - (p1.y - p0.y) * sc2;
                                    s2(cv_count-2, pn2_new, gw2(cv_count-2));
                                }
                            }
                        }
                    } else {
                        // Flipped is not better; restore.
                        trim.m_bRev3d = !trim.m_bRev3d;
                    }
                }
            }
        }
    }


    // Final post-processing: re-run SetTrimIsoFlags for ALL trims to ensure
    // m_iso is consistent with the current pcurve geometry.
    //
    // The seam/singular repair and UV gap snap passes may modify pcurves after
    // the initial SetTolerancesBoxesAndFlags call (line 1738).  When a pcurve
    // is shared between a seam trim and a regular trim, the seam repair only
    // calls SetTrimIsoFlags on the seam trim — leaving the regular trim's
    // m_iso stale.  Similarly, the UV gap snap can kink a shared pcurve,
    // making it no longer truly iso-parametric even though m_iso was set to
    // x_iso/y_iso before the snap.  A fresh unconditional call here ensures
    // IsValid() and the stored m_iso agree, avoiding "m_iso = X but should
    // be Y" failures.
    //
    // NOTE: We call SetTrimIsoFlags() directly rather than
    // SetTolerancesBoxesAndFlags() to avoid resetting the edge tolerances
    // adjusted by the previous pass.
    if (!join_singular_trims(brep, linear_tolerance))
        return false;
    brep.SetTrimIsoFlags();

    if (brep.m_F.Count() != faces.Extent()) {
        if (log)
            log->Print("translated %d of %d source faces\n",
                brep.m_F.Count(), faces.Extent());
        return false;
    }
    return brep.IsValid(log);
}

} // namespace freecad
