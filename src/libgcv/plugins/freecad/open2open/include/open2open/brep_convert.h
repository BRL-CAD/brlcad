// Copyright (c) 2026, open2open contributors
// SPDX-License-Identifier: MIT
//
// brep_convert.h — B-Rep topology conversion helpers.
//
// These functions translate complete B-Rep solid models between openNURBS
// (ON_Brep) and OpenCASCADE (TopoDS_Shape).

#ifndef OPEN2OPEN_BREP_CONVERT_H
#define OPEN2OPEN_BREP_CONVERT_H

#include <TopoDS_Shape.hxx>

// Forward declarations from openNURBS
class ON_Brep;

namespace open2open {

/// Convert an openNURBS B-Rep to an OCCT TopoDS_Shape (Shell or Solid).
///
/// All NURBS curves and surfaces in the brep are translated using the
/// geometry helpers from geom_convert.h.  Non-NURBS primitives are
/// first converted to their NURBS representations via
/// ON_Curve::NurbsCurve() / ON_Surface::NurbsSurface().
///
/// @param brep              Source B-Rep solid.
/// @param linear_tolerance  Positional tolerance propagated to OCCT topology.
/// @return                  A TopoDS_Solid (or TopoDS_Shell for open models).
///                          A null shape is returned on failure.
TopoDS_Shape ON_BrepToOCCT(const ON_Brep& brep,
                            double         linear_tolerance = 1e-6);

/// Convert an OCCT TopoDS_Shape to an openNURBS ON_Brep.
///
/// The function traverses the shape with TopExp_Explorer, translates each
/// geometric entity to NURBS using the geometry helpers, and populates the
/// provided ON_Brep with the resulting topology.
///
/// @param shape             Source OCCT shape (must contain Face topology).
/// @param brep              Destination B-Rep (will be cleared first).
/// @param linear_tolerance  Tolerance used when building the ON_Brep.
/// @return                  true on success; false if the conversion failed.
bool OCCTToON_Brep(const TopoDS_Shape& shape,
                   ON_Brep&            brep,
                   double              linear_tolerance = 1e-6);

} // namespace open2open

#endif // OPEN2OPEN_BREP_CONVERT_H
