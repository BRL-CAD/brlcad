// Copyright (c) 2026, open2open contributors
// SPDX-License-Identifier: MIT
//
// mesh_convert.cpp — Polygonal mesh translation between openNURBS and OCCT.

#include "open2open/mesh_convert.h"

// ---- openNURBS ----
#include "opennurbs.h"

// ---- OCCT ----
#include <Poly_Triangulation.hxx>
#include <Poly_Triangle.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Vec3f.hxx>

namespace open2open {

// ---------------------------------------------------------------------------
// ON_Mesh → Poly_Triangulation
// ---------------------------------------------------------------------------

Handle(Poly_Triangulation) ON_MeshToOCCT(const ON_Mesh& mesh)
{
    const int nv = mesh.m_V.Count();
    const int nf = mesh.m_F.Count();
    if (nv <= 0 || nf <= 0)
        return Handle(Poly_Triangulation)();

    // Count output triangles: quads become 2 triangles, tris stay as 1.
    int ntri = 0;
    for (int fi = 0; fi < nf; ++fi) {
        const ON_MeshFace& f = mesh.m_F[fi];
        ntri += f.IsTriangle() ? 1 : 2;
    }
    if (ntri == 0)
        return Handle(Poly_Triangulation)();

    const bool has_uv      = mesh.HasTextureCoordinates() &&
                             mesh.m_T.Count() == nv;
    const bool has_normals = mesh.HasVertexNormals() &&
                             mesh.m_N.Count() == nv;

    Handle(Poly_Triangulation) tri =
        new Poly_Triangulation(nv, ntri, has_uv, has_normals);

    // Copy vertices — prefer double-precision array when available.
    const bool use_double = (mesh.m_dV.Count() == nv);
    for (int vi = 0; vi < nv; ++vi) {
        gp_Pnt pt;
        if (use_double) {
            const ON_3dPoint& p = mesh.m_dV[vi];
            pt = gp_Pnt(p.x, p.y, p.z);
        } else {
            const ON_3fPoint& p = mesh.m_V[vi];
            pt = gp_Pnt(static_cast<double>(p.x),
                        static_cast<double>(p.y),
                        static_cast<double>(p.z));
        }
        tri->SetNode(vi + 1, pt);   // Poly_Triangulation is 1-indexed
    }

    // Copy per-vertex normals.
    if (has_normals) {
        for (int ni = 0; ni < nv; ++ni) {
            const ON_3fVector& n = mesh.m_N[ni];
            tri->SetNormal(ni + 1,
                           gp_Vec3f(static_cast<float>(n.x),
                                    static_cast<float>(n.y),
                                    static_cast<float>(n.z)));
        }
    }

    // Copy per-vertex texture coordinates.
    if (has_uv) {
        for (int ui = 0; ui < nv; ++ui) {
            const ON_2fPoint& uv = mesh.m_T[ui];
            tri->SetUVNode(ui + 1,
                           gp_Pnt2d(static_cast<double>(uv.x),
                                    static_cast<double>(uv.y)));
        }
    }

    // Copy faces — split quads into two triangles.
    // ON_MeshFace vi[] is 0-indexed; Poly_Triangle is 1-indexed.
    int tidx = 1;
    for (int fi = 0; fi < nf; ++fi) {
        const ON_MeshFace& f = mesh.m_F[fi];
        tri->SetTriangle(tidx++,
                         Poly_Triangle(f.vi[0] + 1,
                                       f.vi[1] + 1,
                                       f.vi[2] + 1));
        if (!f.IsTriangle()) {
            // Second triangle covers the quad's second half:
            // vertices 0, 2, 3 of the original quad.
            tri->SetTriangle(tidx++,
                             Poly_Triangle(f.vi[0] + 1,
                                           f.vi[2] + 1,
                                           f.vi[3] + 1));
        }
    }

    return tri;
}

// ---------------------------------------------------------------------------
// Poly_Triangulation → ON_Mesh
// ---------------------------------------------------------------------------

bool OCCTToON_Mesh(const Handle(Poly_Triangulation)& tri, ON_Mesh& mesh)
{
    if (tri.IsNull())
        return false;

    const int nv = tri->NbNodes();
    const int nt = tri->NbTriangles();
    if (nv <= 0 || nt <= 0)
        return false;

    mesh.Destroy();

    // Allocate and fill vertex array.
    mesh.m_V.SetCapacity(nv);
    mesh.m_V.SetCount(nv);
    for (int vi = 1; vi <= nv; ++vi) {
        const gp_Pnt& p = tri->Node(vi);
        mesh.m_V[vi - 1] = ON_3fPoint(static_cast<float>(p.X()),
                                       static_cast<float>(p.Y()),
                                       static_cast<float>(p.Z()));
    }

    // Allocate and fill per-vertex normal array.
    if (tri->HasNormals()) {
        mesh.m_N.SetCapacity(nv);
        mesh.m_N.SetCount(nv);
        for (int ni = 1; ni <= nv; ++ni) {
            gp_Vec3f n;
            tri->Normal(ni, n);
            mesh.m_N[ni - 1] = ON_3fVector(n.x(), n.y(), n.z());
        }
    }

    // Allocate and fill per-vertex UV array.
    if (tri->HasUVNodes()) {
        mesh.m_T.SetCapacity(nv);
        mesh.m_T.SetCount(nv);
        for (int ui = 1; ui <= nv; ++ui) {
            const gp_Pnt2d& uv = tri->UVNode(ui);
            mesh.m_T[ui - 1] = ON_2fPoint(static_cast<float>(uv.X()),
                                            static_cast<float>(uv.Y()));
        }
    }

    // Allocate and fill face array.
    // openNURBS encodes triangles as degenerate quads: vi[2] == vi[3].
    mesh.m_F.SetCapacity(nt);
    mesh.m_F.SetCount(nt);
    for (int ti = 1; ti <= nt; ++ti) {
        const Poly_Triangle& t = tri->Triangle(ti);
        ON_MeshFace& f = mesh.m_F[ti - 1];
        f.vi[0] = t.Value(1) - 1;   // convert 1-indexed → 0-indexed
        f.vi[1] = t.Value(2) - 1;
        f.vi[2] = t.Value(3) - 1;
        f.vi[3] = f.vi[2];           // degenerate-quad triangle encoding
    }

    return true;
}

} // namespace open2open
