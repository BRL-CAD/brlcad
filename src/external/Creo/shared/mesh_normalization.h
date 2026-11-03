/**
 *       M E S H _ N O R M A L I Z A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CREO_SHARED_MESH_NORMALIZATION_H
#define CREO_SHARED_MESH_NORMALIZATION_H

#include <stddef.h>

#include <vector>

#include "shared/conversion_snapshot.h"


extern const double CREO_BRL_MESH_VERTEX_TOLERANCE_MM;


struct creo_brl_mesh_normalization {
    std::vector<struct creo_brl_snapshot_vector> vertices;
    std::vector<struct creo_brl_snapshot_triangle> triangles;
    std::vector<size_t> source_triangle_indices;
    int is_solid;
};


int creo_brl_normalize_mesh(
    const std::vector<struct creo_brl_snapshot_vector>& source_vertices,
    const std::vector<struct creo_brl_snapshot_triangle>& source_triangles,
    struct creo_brl_mesh_normalization *mesh);


#endif /* CREO_SHARED_MESH_NORMALIZATION_H */
