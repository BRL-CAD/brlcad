/*                    B R E P _ L O C A L . H
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file brep_local.h
 *
 * Local data structures for brep primitive
 */

#ifndef LIBRT_PRIMITIVES_BREP_BREP_LOCAL_H
#define LIBRT_PRIMITIVES_BREP_BREP_LOCAL_H

#include <vector>


struct brep_edge_span {
    ON_BezierCurve curve;
    ON_BoundingBox bbox;
    ON_Interval edge_domain;
    int edge_index = -1;
};


struct brep_edge_record {
    size_t span_begin = 0;
    size_t span_count = 0;
    double tolerance = ON_UNSET_VALUE;
    double model_tolerance = 0.0;
    double declared_tolerance = ON_UNSET_VALUE;
    double measured_discrepancy = ON_UNSET_VALUE;
    double discrepancy_lower_bound = ON_UNSET_VALUE;
    double discrepancy_upper_bound = ON_UNSET_VALUE;
    double discrepancy_bound_tolerance = ON_UNSET_VALUE;
    size_t correspondence_cells = 0;
    size_t correspondence_depth = 0;
    size_t discrepancy_bound_cells = 0;
    size_t discrepancy_bound_depth = 0;
    int edge_index = -1;
    int face_index[2] = {-1, -1};
    bool supported = false;
    bool discrepancy_measured = false;
    bool correspondence_screened = false;
    bool correspondence_supported = false;
    bool correspondence_exhausted = false;
    bool discrepancy_bounded = false;
    bool discrepancy_bound_exhausted = false;
    bool discrepancy_sample_authorized = false;
    int discrepancy_proof_class = 0;
    bool discrepancy_authorized = false;
    bool tolerance_inferred = false;
};


struct brep_surface_span {
    ON_BezierSurface surface;
    ON_BoundingBox bbox;
    ON_Interval surface_domain[2];
    int face_index = -1;
    int span_index = -1;
};


struct brep_face_record {
    size_t span_begin = 0;
    size_t span_count = 0;
    int face_index = -1;
    bool supported = false;
};


/* One oriented arc of the link obtained by intersecting a sufficiently small
 * sphere about a manifold vertex with the incident faces.  outgoing is the
 * tangent of edge_index directed away from the vertex.  face_index carries
 * this arc from outgoing to the next arc's outgoing tangent, with solid
 * directions on its left. */
struct brep_vertex_arc {
    ON_3dVector outgoing;
    ON_3dVector outward_normal;
    double clockwise_sweep = 0.0;
    int edge_index = -1;
    int face_index = -1;
};


struct brep_vertex_record {
    ON_3dPoint point;
    std::vector<brep_vertex_arc> arcs;
    int vertex_index = -1;
    bool planar = false;
    bool supported = false;
};


/**
 * The b-rep specific data structure for caching the prepared
 * acceleration data structure.
 */
struct brep_specific {
    ON_Brep *brep = NULL;
    BrepBoundingVolume *bvh = NULL;
    std::vector<const brlcad::CurveTree *> ctrees;
    std::vector<brep_edge_record> edge_records;
    std::vector<brep_edge_span> edge_spans;
    std::vector<brep_face_record> face_records;
    std::vector<brep_surface_span> surface_spans;
    std::vector<brep_vertex_record> vertex_records;
    int is_solid = 0;
    int plate_mode = 0;
    int plate_mode_nocos = 0;
    double plate_mode_thickness = 0.0;
};

#endif /* LIBRT_PRIMITIVES_BREP_BREP_LOCAL_H */
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
