// Copyright (c) 2026, open2open contributors
// SPDX-License-Identifier: MIT
//
// geom_convert.cpp — NURBS curve and surface translation between
//                    openNURBS and OpenCASCADE (OCCT).

#include "open2open/geom_convert.h"

// ---- openNURBS ----
#include "opennurbs.h"

// ---- OCCT ----
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_Array1OfPnt2d.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array2OfReal.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom2d_BSplineCurve.hxx>

#include <vector>

namespace open2open {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Convert an openNURBS flat knot array to OCCT (unique knots + multiplicities).
///
/// openNURBS stores `order + cv_count - 2` knot values – omitting one
/// "phantom" knot at each end that OCCT needs.  This function builds a
/// temporary extended array of length `num_knots + 2`, prepending
/// @p phantom_lo and appending @p phantom_hi, then deduplicates into
/// the OCCT (knot, multiplicity) format.
///
/// For clamped (non-periodic) curves pass `phantom_lo = flat_knots[0]`
/// and `phantom_hi = flat_knots[num_knots-1]`.
///
/// For uniform (non-clamped, non-periodic) curves pass extrapolated values:
/// `phantom_lo = 2*k[0] - k[1]` and `phantom_hi = 2*k[last] - k[last-1]`.
///
/// For periodic curves pass `periodic = true` and the phantoms are ignored.
static void ExtractKnotsAndMults(const double* flat_knots,
                                 int           num_knots,
                                 TColStd_Array1OfReal&    knots,
                                 TColStd_Array1OfInteger& mults,
                                 double                   phantom_lo,
                                 double                   phantom_hi,
                                 bool                     periodic = false)
{
    if (periodic) {
        // Periodic: pass through without adding phantom knots
        int num_distinct = 0;
        for (int i = 0; i < num_knots; ) {
            ++num_distinct;
            int j = i + 1;
            while (j < num_knots && flat_knots[j] == flat_knots[i]) ++j;
            i = j;
        }
        knots.Resize(1, num_distinct, Standard_False);
        mults.Resize(1, num_distinct, Standard_False);
        int idx = 1, i = 0;
        while (i < num_knots) {
            int j = i + 1;
            while (j < num_knots && flat_knots[j] == flat_knots[i]) ++j;
            knots.SetValue(idx, flat_knots[i]);
            mults.SetValue(idx, j - i);
            ++idx; i = j;
        }
        return;
    }

    // Build extended array: [phantom_lo] + flat_knots[] + [phantom_hi]
    const int ext_len = num_knots + 2;
    std::vector<double> ext(ext_len);
    ext[0] = phantom_lo;
    for (int k = 0; k < num_knots; ++k)
        ext[k + 1] = flat_knots[k];
    ext[ext_len - 1] = phantom_hi;

    // Count distinct values in the extended array
    int num_distinct = 0;
    for (int i = 0; i < ext_len; ) {
        ++num_distinct;
        int j = i + 1;
        while (j < ext_len && ext[j] == ext[i]) ++j;
        i = j;
    }

    knots.Resize(1, num_distinct, Standard_False);
    mults.Resize(1, num_distinct, Standard_False);

    int idx = 1, i = 0;
    while (i < ext_len) {
        int j = i + 1;
        while (j < ext_len && ext[j] == ext[i]) ++j;
        knots.SetValue(idx, ext[i]);
        mults.SetValue(idx, j - i);
        ++idx; i = j;
    }
}

// ---------------------------------------------------------------------------
// ON_NurbsCurve → Geom_BSplineCurve  (3-D)
// ---------------------------------------------------------------------------

Handle(Geom_BSplineCurve) ON_NurbsCurveToOCCT(const ON_NurbsCurve& c)
{
    if (c.m_dim != 3 || c.m_order < 2 || c.m_cv_count < c.m_order ||
        !c.m_cv || !c.m_knot)
        return Handle(Geom_BSplineCurve)();

    const int degree    = c.m_order - 1;
    const int num_poles = c.m_cv_count;
    const bool rational = (c.m_is_rat != 0);
    // ON_IsKnotVectorPeriodic detects uniform knot spacing at both ends.
    // We always build a non-periodic OCCT B-spline (the evaluation is
    // identical) and add the two phantom knots that openNURBS omits.
    // True closed curves are handled as non-periodic with a domain
    // restricted to the interior knot span.

    // --- control points (OCCT is 1-indexed, Euclidean) ---
    TColgp_Array1OfPnt poles(1, num_poles);
    TColStd_Array1OfReal weights(1, num_poles);

    for (int i = 0; i < num_poles; ++i) {
        const double* cv = c.m_cv + static_cast<std::ptrdiff_t>(i) * c.m_cv_stride;
        if (rational) {
            double w = cv[c.m_dim]; // homogeneous weight
            if (w == 0.0) return Handle(Geom_BSplineCurve)();
            poles.SetValue(i + 1, gp_Pnt(cv[0] / w, cv[1] / w, cv[2] / w));
            weights.SetValue(i + 1, w);
        } else {
            poles.SetValue(i + 1, gp_Pnt(cv[0], cv[1], cv[2]));
            weights.SetValue(i + 1, 1.0);
        }
    }

    // --- knot vector ---
    // openNURBS omits one "phantom" knot at each end; OCCT requires them.
    // For clamped ends the phantom = boundary knot value.
    // For non-clamped ends extrapolate by one knot-step.
    const int num_flat_knots = c.m_order + c.m_cv_count - 2;
    const double k0  = c.m_knot[0];
    const double k1  = c.m_knot[1];
    const double kn1 = c.m_knot[num_flat_knots - 2];
    const double kn  = c.m_knot[num_flat_knots - 1];
    const double phantom_lo = (k0 != k1)  ? 2.0 * k0  - k1  : k0;
    const double phantom_hi = (kn != kn1) ? 2.0 * kn  - kn1 : kn;

    TColStd_Array1OfReal    knots(1, 1);
    TColStd_Array1OfInteger mults(1, 1);
    ExtractKnotsAndMults(c.m_knot, num_flat_knots, knots, mults,
                         phantom_lo, phantom_hi, false);

    try {
        if (rational) {
            return new Geom_BSplineCurve(poles, weights, knots, mults,
                                         degree, false);
        } else {
            return new Geom_BSplineCurve(poles, knots, mults,
                                         degree, false);
        }
    } catch (...) {
        return Handle(Geom_BSplineCurve)();
    }
}

// ---------------------------------------------------------------------------
// ON_NurbsCurve → Geom2d_BSplineCurve  (2-D parameter-space)
// ---------------------------------------------------------------------------

Handle(Geom2d_BSplineCurve) ON_NurbsCurve2dToOCCT(const ON_NurbsCurve& c)
{
    if (c.m_dim != 2 || c.m_order < 2 || c.m_cv_count < c.m_order ||
        !c.m_cv || !c.m_knot)
        return Handle(Geom2d_BSplineCurve)();

    const int degree    = c.m_order - 1;
    const int num_poles = c.m_cv_count;
    const bool rational = (c.m_is_rat != 0);

    TColgp_Array1OfPnt2d poles(1, num_poles);
    TColStd_Array1OfReal weights(1, num_poles);

    for (int i = 0; i < num_poles; ++i) {
        const double* cv = c.m_cv + static_cast<std::ptrdiff_t>(i) * c.m_cv_stride;
        if (rational) {
            double w = cv[c.m_dim];
            if (w == 0.0) return Handle(Geom2d_BSplineCurve)();
            poles.SetValue(i + 1, gp_Pnt2d(cv[0] / w, cv[1] / w));
            weights.SetValue(i + 1, w);
        } else {
            poles.SetValue(i + 1, gp_Pnt2d(cv[0], cv[1]));
            weights.SetValue(i + 1, 1.0);
        }
    }

    const int num_flat_knots = c.m_order + c.m_cv_count - 2;
    const double k2_0  = c.m_knot[0];
    const double k2_1  = c.m_knot[1];
    const double k2_n1 = c.m_knot[num_flat_knots - 2];
    const double k2_n  = c.m_knot[num_flat_knots - 1];
    const double plo2  = (k2_0 != k2_1)  ? 2.0 * k2_0 - k2_1  : k2_0;
    const double phi2  = (k2_n != k2_n1) ? 2.0 * k2_n - k2_n1 : k2_n;

    TColStd_Array1OfReal    knots(1, 1);
    TColStd_Array1OfInteger mults(1, 1);
    ExtractKnotsAndMults(c.m_knot, num_flat_knots, knots, mults,
                         plo2, phi2, false);

    try {
        if (rational) {
            return new Geom2d_BSplineCurve(poles, weights, knots, mults,
                                            degree, false);
        } else {
            return new Geom2d_BSplineCurve(poles, knots, mults,
                                            degree, false);
        }
    } catch (...) {
        return Handle(Geom2d_BSplineCurve)();
    }
}

// ---------------------------------------------------------------------------
// ON_NurbsSurface → Geom_BSplineSurface
// ---------------------------------------------------------------------------

Handle(Geom_BSplineSurface) ON_NurbsSurfaceToOCCT(const ON_NurbsSurface& s)
{
    if (s.m_dim != 3 || s.m_order[0] < 2 || s.m_order[1] < 2 ||
        s.m_cv_count[0] < s.m_order[0] || s.m_cv_count[1] < s.m_order[1] ||
        !s.m_cv || !s.m_knot[0] || !s.m_knot[1])
        return Handle(Geom_BSplineSurface)();

    const int udeg    = s.m_order[0] - 1;
    const int vdeg    = s.m_order[1] - 1;
    const int nu      = s.m_cv_count[0];
    const int nv      = s.m_cv_count[1];
    const bool rational = (s.m_is_rat != 0);

    // OCCT Array2: rows = U index (1..nu), cols = V index (1..nv)
    TColgp_Array2OfPnt   poles(1, nu, 1, nv);
    TColStd_Array2OfReal weights(1, nu, 1, nv);

    for (int i = 0; i < nu; ++i) {
        for (int j = 0; j < nv; ++j) {
            const double* cv = s.m_cv
                + static_cast<std::ptrdiff_t>(i) * s.m_cv_stride[0]
                + static_cast<std::ptrdiff_t>(j) * s.m_cv_stride[1];
            if (rational) {
                double w = cv[s.m_dim];
                if (w == 0.0) return Handle(Geom_BSplineSurface)();
                poles.SetValue(i + 1, j + 1,
                               gp_Pnt(cv[0] / w, cv[1] / w, cv[2] / w));
                weights.SetValue(i + 1, j + 1, w);
            } else {
                poles.SetValue(i + 1, j + 1,
                               gp_Pnt(cv[0], cv[1], cv[2]));
                weights.SetValue(i + 1, j + 1, 1.0);
            }
        }
    }

    const int nu_knots = s.m_order[0] + s.m_cv_count[0] - 2;
    const int nv_knots = s.m_order[1] + s.m_cv_count[1] - 2;

    // Compute phantom knot values for each parametric direction
    auto PhantomLo = [](const double* k, int n) -> double {
        return (k[0] != k[1]) ? 2.0 * k[0] - k[1] : k[0];
    };
    auto PhantomHi = [](const double* k, int n) -> double {
        return (k[n-1] != k[n-2]) ? 2.0 * k[n-1] - k[n-2] : k[n-1];
    };

    TColStd_Array1OfReal    uknots(1, 1), vknots(1, 1);
    TColStd_Array1OfInteger umults(1, 1), vmults(1, 1);
    ExtractKnotsAndMults(s.m_knot[0], nu_knots, uknots, umults,
                         PhantomLo(s.m_knot[0], nu_knots),
                         PhantomHi(s.m_knot[0], nu_knots), false);
    ExtractKnotsAndMults(s.m_knot[1], nv_knots, vknots, vmults,
                         PhantomLo(s.m_knot[1], nv_knots),
                         PhantomHi(s.m_knot[1], nv_knots), false);

    try {
        if (rational) {
            return new Geom_BSplineSurface(poles, weights,
                                           uknots, vknots,
                                           umults, vmults,
                                           udeg, vdeg,
                                           false, false);
        } else {
            return new Geom_BSplineSurface(poles,
                                           uknots, vknots,
                                           umults, vmults,
                                           udeg, vdeg,
                                           false, false);
        }
    } catch (...) {
        return Handle(Geom_BSplineSurface)();
    }
}

// ---------------------------------------------------------------------------
// Geom_BSplineCurve → ON_NurbsCurve  (3-D)
// ---------------------------------------------------------------------------

bool OCCTCurveToON(const Handle(Geom_BSplineCurve)& c, ON_NurbsCurve& out)
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
            out.m_knot[ki++] = val;
        }
    }

    return out.IsValid() ? true : false;
}

// ---------------------------------------------------------------------------
// Geom_BSplineSurface → ON_NurbsSurface
// ---------------------------------------------------------------------------

bool OCCTSurfaceToON(const Handle(Geom_BSplineSurface)& s, ON_NurbsSurface& out)
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
            for (int m = 0; m < mult; ++m)
                out.m_knot[0][ki++] = val;
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
            for (int m = 0; m < mult; ++m)
                out.m_knot[1][ki++] = val;
        }
    }

    return out.IsValid() ? true : false;
}

} // namespace open2open
