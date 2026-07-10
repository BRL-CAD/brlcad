// Copyright (c) 2026, open2open contributors
// SPDX-License-Identifier: MIT
//
// mesh_convert.h — Polygonal mesh conversion helpers.
//
// These functions translate polygonal mesh geometry between openNURBS
// (ON_Mesh) and OpenCASCADE (Poly_Triangulation).

#ifndef OPEN2OPEN_MESH_CONVERT_H
#define OPEN2OPEN_MESH_CONVERT_H

#include <Standard_Handle.hxx>
#include <Poly_Triangulation.hxx>

// Forward declaration from openNURBS
class ON_Mesh;

namespace open2open {

/// Convert an openNURBS polygon mesh to an OCCT Poly_Triangulation.
///
/// Quad faces in the openNURBS mesh are triangulated by splitting along
/// the diagonal between vertex 0 and vertex 2 of each quad.  Vertex
/// positions, per-vertex normals, and per-vertex texture coordinates
/// are all preserved when present.  Vertex colors are not carried over
/// because Poly_Triangulation does not support per-vertex colour data.
///
/// Vertex positions are taken from the double-precision array (m_dV) when
/// it is populated; otherwise from the single-precision array (m_V).
///
/// @param mesh   Source openNURBS mesh.
/// @return       A new Poly_Triangulation handle, or a null handle if the
///               mesh is empty or the conversion fails.
Handle(Poly_Triangulation) ON_MeshToOCCT(const ON_Mesh& mesh);

/// Convert an OCCT Poly_Triangulation to an openNURBS ON_Mesh.
///
/// Each triangle becomes an ON_MeshFace using the degenerate-quad encoding
/// that openNURBS uses for triangles: vi[2] == vi[3].  Per-vertex normals
/// and UV texture coordinates are copied when the triangulation provides them.
///
/// @param tri    Source OCCT triangulation (must not be null).
/// @param mesh   Destination mesh (cleared before writing).
/// @return       true on success; false if @p tri is null or empty.
bool OCCTToON_Mesh(const Handle(Poly_Triangulation)& tri, ON_Mesh& mesh);

} // namespace open2open

#endif // OPEN2OPEN_MESH_CONVERT_H
