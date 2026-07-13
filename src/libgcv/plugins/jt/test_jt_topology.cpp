/*                     T E S T _ J T _ T O P O L O G Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "jt_topology.h"

#include "bu/app.h"

#include <iostream>

int main(int argc, char **argv)
{
    (void)argc;
    bu_setprogname(argv[0]);

    /* A single triangle plus its cover face.  In the dual representation this
     * is two valence-three vertices joined by three degree-two faces. */
    jt::TopologySymbols symbols;
    symbols.vertex_valences = {3, 3};
    symbols.vertex_groups = {7, 7};
    symbols.vertex_flags = {0, 1};
    symbols.face_degrees[0] = {2, 2};
    symbols.face_degrees[1] = {2};
    symbols.face_attribute_masks[0] = {0, 0, 0};

    jt::TopologyMesh mesh;
    std::string error;
    if (!jt::decode_topology(symbols, mesh, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    if (mesh.polygons.size() != 1 || mesh.polygons[0].size() != 3 || mesh.polygon_groups[0] != 7)
        return 2;
    if (mesh.polygons[0][0] != 0 || mesh.polygons[0][1] != 1 || mesh.polygons[0][2] != 2)
        return 3;
    if (mesh.triangles != std::vector<int32_t>({0, 1, 2}))
        return 5;

    symbols.vertex_groups.pop_back();
    if (jt::decode_topology(symbols, mesh, error) || error.empty())
        return 4;
    return 0;
}
