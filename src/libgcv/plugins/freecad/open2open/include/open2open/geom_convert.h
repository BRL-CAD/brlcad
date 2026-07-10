// Copyright (c) 2026, open2open contributors
// SPDX-License-Identifier: MIT
//
// geom_convert.h — Low-level NURBS geometry conversion helpers.
//
// These functions translate individual curve and surface objects between
// openNURBS and OpenCASCADE (OCCT) representations.  They are the building
// blocks for the higher-level B-Rep topology conversion in brep_convert.h.

#ifndef OPEN2OPEN_GEOM_CONVERT_H
#define OPEN2OPEN_GEOM_CONVERT_H

#include <Standard_Handle.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom2d_BSplineCurve.hxx>

// Forward declarations from openNURBS
class ON_NurbsCurve;
class ON_NurbsSurface;

namespace open2open {

//--------------------------------------------------------------------
// openNURBS → OCCT
//--------------------------------------------------------------------

/// Convert a 3-D openNURBS NURBS curve to an OCCT Geom_BSplineCurve.
/// @param c   Source curve (m_dim must be 3).
/// @return    Null handle if the conversion fails (e.g. wrong dimension).
Handle(Geom_BSplineCurve) ON_NurbsCurveToOCCT(const ON_NurbsCurve& c);

/// Convert a 2-D openNURBS NURBS curve (parameter-space trim) to a
/// Geom2d_BSplineCurve.
/// @param c   Source curve (m_dim must be 2).
/// @return    Null handle on failure.
Handle(Geom2d_BSplineCurve) ON_NurbsCurve2dToOCCT(const ON_NurbsCurve& c);

/// Convert an openNURBS NURBS surface to an OCCT Geom_BSplineSurface.
/// @param s   Source surface.
/// @return    Null handle on failure.
Handle(Geom_BSplineSurface) ON_NurbsSurfaceToOCCT(const ON_NurbsSurface& s);

//--------------------------------------------------------------------
// OCCT → openNURBS
//--------------------------------------------------------------------

/// Convert an OCCT Geom_BSplineCurve to an openNURBS ON_NurbsCurve.
/// @param c    Source curve handle.
/// @param out  Destination object (will be overwritten).
/// @return     true on success.
bool OCCTCurveToON(const Handle(Geom_BSplineCurve)& c, ON_NurbsCurve& out);

/// Convert an OCCT Geom_BSplineSurface to an openNURBS ON_NurbsSurface.
/// @param s    Source surface handle.
/// @param out  Destination object (will be overwritten).
/// @return     true on success.
bool OCCTSurfaceToON(const Handle(Geom_BSplineSurface)& s, ON_NurbsSurface& out);

} // namespace open2open

#endif // OPEN2OPEN_GEOM_CONVERT_H
