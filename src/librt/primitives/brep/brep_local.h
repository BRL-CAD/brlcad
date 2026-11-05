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

#include <cstdint>
#include <vector>


struct brep_directional_object_shared_cache;


struct brep_edge_span {
    ON_BezierCurve curve;
    ON_BoundingBox bbox;
    ON_Interval edge_domain;
    int edge_index = -1;
};


struct brep_trim_span {
    ON_BezierCurve curve;
    ON_Interval trim_domain;
};


struct brep_edge_trim_cell {
    ON_Interval edge_domain;
    ON_Interval trim_domain;
    size_t edge_span = 0;
    size_t trim_span_begin = 0;
    size_t trim_span_count = 0;
    int trim_index = -1;
};


struct brep_edge_record {
    ON_BoundingBox bbox;
    double bbox_coordinate_scale = 0.0;
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
    size_t trim_cell_begin[2] = {0, 0};
    size_t trim_cell_count[2] = {0, 0};
    int edge_index = -1;
    int face_index[2] = {-1, -1};
    bool supported = false;
    bool bbox_coordinate_scale_valid = false;
    bool discrepancy_measured = false;
    bool correspondence_screened = false;
    bool correspondence_supported = false;
    bool correspondence_exhausted = false;
    bool discrepancy_bounded = false;
    bool discrepancy_bound_exhausted = false;
    bool discrepancy_endpoints_certified = false;
    bool frame_interval_supported = false;
    bool discrepancy_sample_authorized = false;
    int discrepancy_proof_class = 0;
    bool discrepancy_authorized = false;
    bool tolerance_inferred = false;
};


struct brep_surface_control_layout {
    const double *packed_controls = NULL;
    size_t packed_begin = 0;
    size_t packed_count = 0;
    int stride[2] = {0, 0};
    int order[2] = {0, 0};
    bool packed = false;
    bool direct = false;
    bool rational = false;
};


struct brep_surface_span {
    ON_BezierSurface surface;
    brep_surface_control_layout control_layout;
    ON_BoundingBox bbox;
    double bbox_coordinate_scale = 0.0;
    /* Conservative, unnormalized Su x Sv component hull over the complete
     * Bezier span.  It is geometry-only prep data: a ray direction turns it
     * into a cheap transversality proof without rebuilding derivatives. */
    double normal_hull_minimum[3] = {0.0, 0.0, 0.0};
    double normal_hull_maximum[3] = {0.0, 0.0, 0.0};
    double normal_hull_magnitude_upper = 0.0;
    ON_Interval surface_domain[2];
    int face_index = -1;
    int span_index = -1;
    unsigned int singular_side_mask = 0;
    unsigned int routing_capability_flags = 0;
    unsigned int routing_risk_flags = 0;
    bool control_points_valid = false;
    bool bbox_coordinate_scale_valid = false;
    bool normal_hull_valid = false;
    bool routing_correction_cell_overlap = false;
};


struct brep_routing_feature_cell {
    ON_BoundingBox bbox;
    ON_Interval surface_domain[2];
    uint32_t span_index_begin = 0;
    uint32_t span_index_count = 0;
    uint32_t directional_span_begin = 0;
    uint32_t directional_span_count = 0;
    unsigned int capability_flags = 0;
    unsigned int risk_flags = 0;
    uint32_t leaf_ordinal = UINT32_MAX;
    int face_index = -1;
};


struct brep_routing_control_interval {
    double minimum[4] = {0.0, 0.0, 0.0, 0.0};
    double maximum[4] = {0.0, 0.0, 0.0, 0.0};
};


struct brep_routing_subpatch_bbox {
    ON_BoundingBox bbox;
    double normal_hull_minimum[3] = {0.0, 0.0, 0.0};
    double normal_hull_maximum[3] = {0.0, 0.0, 0.0};
    double normal_hull_magnitude_upper = 0.0;
    bool normal_hull_valid = false;
};


/* Immutable association between one untrimmed SurfaceTree feature cell and
 * one prepared Bezier span.  The normalized box and its geometry-only normal
 * hull are rebuilt after either fresh or cached SurfaceTree prep. */
struct brep_directional_cell_span {
    double normalized_minimum[2] = {0.0, 0.0};
    double normalized_maximum[2] = {1.0, 1.0};
    double normal_hull_minimum[3] = {0.0, 0.0, 0.0};
    double normal_hull_maximum[3] = {0.0, 0.0, 0.0};
    double normal_hull_magnitude_upper = 0.0;
    uint32_t span_index = UINT32_MAX;
    uint32_t subpatch_begin = 0;
    uint16_t subpatch_count = 0;
    bool normal_hull_valid = false;
    bool cell_domain_mapped = false;
};


struct brep_directional_subpatch {
    ON_BoundingBox bbox;
    double bbox_coordinate_scale = 0.0;
    double normalized_minimum[2] = {0.0, 0.0};
    double normalized_maximum[2] = {1.0, 1.0};
    double normal_hull_minimum[3] = {0.0, 0.0, 0.0};
    double normal_hull_maximum[3] = {0.0, 0.0, 0.0};
    double normal_hull_magnitude_upper = 0.0;
    bool bbox_coordinate_scale_valid = false;
    bool normal_hull_valid = false;
};


enum brep_status2_profile_map_kind {
    BREP_STATUS2_PROFILE_MAP_IDENTITY = 0,
    BREP_STATUS2_PROFILE_MAP_ARC = 1
};


/* One immutable piece of a status-2 revolution profile parameter map.
 * profile_domain is the piece's interval in the complete profile (a
 * PolyCurve segment slice mapped through an outer proxy when applicable).
 * segment_domain is the selected interval in the leaf segment's public
 * domain.  real_parameter records the oriented affine image of the top-level
 * piece endpoints in real_domain and therefore composes both leaf and outer
 * proxy orientation. */
struct brep_status2_profile_map_piece {
    ON_Interval profile_domain = ON_Interval::EmptyInterval;
    ON_Interval segment_domain = ON_Interval::EmptyInterval;
    ON_Interval real_domain = ON_Interval::EmptyInterval;
    double real_parameter[2] = {ON_UNSET_VALUE, ON_UNSET_VALUE};
    double arc_angle = 0.0;
    uint8_t kind = BREP_STATUS2_PROFILE_MAP_IDENTITY;
};


/* Derived, depth-specific data for one routing-cell/Bezier-span association.
 * The controls enclose the span restricted to the cell in homogeneous
 * coordinates.  They are rebuilt after cache load rather than serialized. */
struct brep_routing_cell_span_pack {
    ON_BoundingBox bbox;
    double bbox_coordinate_scale = 0.0;
    uint32_t span_index = UINT32_MAX;
    uint32_t control_begin = 0;
    uint32_t control_count = 0;
    uint32_t subpatch_begin = 0;
    uint32_t subpatch_count = 0;
    uint8_t order[2] = {0, 0};
    bool bbox_coordinate_scale_valid = false;
    bool controls_complete = false;
};


struct brep_face_record {
    ON_BoundingBox bbox;
    double bbox_coordinate_scale = 0.0;
    /* Conservative union of every prepared span's unnormalized normal hull.
     * It is a cheap direction-dependent rejection before a ray walks the
     * cell/span bindings of this face. */
    double normal_hull_minimum[3] = {0.0, 0.0, 0.0};
    double normal_hull_maximum[3] = {0.0, 0.0, 0.0};
    double normal_hull_magnitude_upper = 0.0;
    ON_Interval nurbs_domain[2];
    size_t span_begin = 0;
    size_t span_count = 0;
    size_t span_tree_root = 0;
    size_t routing_cell_begin = 0;
    size_t routing_cell_count = 0;
    size_t status2_profile_piece_begin = 0;
    size_t status2_profile_piece_count = 0;
    int face_index = -1;
    int nurb_form_status = 0;
    unsigned int routing_capability_flags = 0;
    unsigned int routing_risk_flags = 0;
    bool status2_revolution_singular_map = false;
    bool status2_revolution_interval_map = false;
    bool supported = false;
    bool bbox_coordinate_scale_valid = false;
    bool normal_hull_valid = false;
    bool span_tree_available = false;
    bool routing_has_correction_cell = false;
    bool routing_correction_cells_complete = false;
    /* True only when prep found an adjacent edge that can satisfy every
     * ray-independent premise of the legacy one-hit seam repair.  This is a
     * conservative work predicate, not publication authority. */
    bool legacy_repair_candidate = false;
};


/* A balanced hierarchy over one face's contiguous Bezier spans.  Traversal
 * retains span order so bounded root collection stays deterministic. */
struct brep_prepared_span_node {
    ON_BoundingBox bbox;
    /* Union of the Bezier parameter rectangles below this node.  This lets
     * prep associate a SurfaceTree cell with spans through the already-built
     * balanced hierarchy instead of rescanning every span of the face. */
    ON_Interval surface_domain[2];
    double bbox_coordinate_scale = 0.0;
    size_t span_count = 0;
    bool cullable = false;
    bool bbox_coordinate_scale_valid = false;
    bool surface_domain_valid = false;
};


/* A balanced hierarchy over contiguous face records.  Keeping the original
 * order avoids perturbing the bounded prepared-root collection. */
struct brep_prepared_face_node {
    ON_BoundingBox bbox;
    double bbox_coordinate_scale = 0.0;
    size_t face_begin = 0;
    size_t span_count = 0;
    size_t supported_face_count = 0;
    size_t reparameterized_face_count = 0;
    size_t unsupported_face_count = 0;
    size_t left_child = 0;
    size_t right_child = 0;
    bool cullable = false;
    bool leaf = false;
    bool bbox_coordinate_scale_valid = false;
};


/* A compact immutable hierarchy over BREP face bounds.  Leaves retain their
 * source index, allowing callers to restore the original face order after
 * spatial selection. */
struct brep_face_bvh_node {
    ON_BoundingBox bbox;
    double bbox_coordinate_scale = 0.0;
    size_t left_child = 0;
    size_t right_child = 0;
    size_t index = 0;
    bool leaf = false;
    bool bbox_coordinate_scale_valid = false;
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


enum brep_prepared_support_flag {
    BREP_PREPARED_SUPPORT_SURFACE_DATA = 1u << 0,
    BREP_PREPARED_SUPPORT_SPAN_HIERARCHIES = 1u << 1,
    BREP_PREPARED_SUPPORT_FACE_HIERARCHY = 1u << 2,
    BREP_PREPARED_SUPPORT_FACE_BVH = 1u << 3,
    BREP_PREPARED_SUPPORT_EDGE_RECORDS = 1u << 4,
    BREP_PREPARED_SUPPORT_EDGE_CORRESPONDENCE = 1u << 5,
    BREP_PREPARED_SUPPORT_VERTEX_RECORDS = 1u << 6,
    BREP_PREPARED_SUPPORT_PACKED_SURFACE_CONTROLS = 1u << 7,
    BREP_PREPARED_SUPPORT_ROUTING_METADATA = 1u << 8,
    BREP_PREPARED_SUPPORT_ROUTING_FEATURE_CELLS = 1u << 9
};


/**
 * The b-rep specific data structure for caching the prepared
 * acceleration data structure.
 */
struct brep_specific {
    ON_Brep *brep = NULL;
    BrepBoundingVolume *bvh = NULL;
    std::vector<brep_face_bvh_node> legacy_face_nodes;
    size_t legacy_face_root = (size_t)-1;
    std::vector<const brlcad::CurveTree *> ctrees;
    std::vector<brep_edge_record> edge_records;
    std::vector<brep_edge_span> edge_spans;
    std::vector<brep_edge_trim_cell> edge_trim_cells;
    std::vector<brep_trim_span> edge_trim_spans;
    std::vector<brep_face_record> face_records;
    std::vector<brep_surface_span> surface_spans;
    std::vector<brep_status2_profile_map_piece> status2_profile_map_pieces;
    std::vector<brep_routing_feature_cell> routing_feature_cells;
    std::vector<brep_routing_cell_span_pack> routing_cell_span_packs;
    std::vector<brep_routing_control_interval> routing_cell_span_controls;
    std::vector<brep_routing_subpatch_bbox> routing_cell_span_subpatches;
    std::vector<brep_directional_cell_span> directional_cell_spans;
    std::vector<brep_directional_subpatch> directional_subpatches;
    std::vector<brep_face_bvh_node> routing_correction_bvh_nodes;
    size_t routing_correction_bvh_root = (size_t)-1;
    /* Four homogeneous doubles per control, ordered span/U/V.  Spans retain
     * stable offsets and are bound to this immutable array after prep. */
    std::vector<double> packed_surface_controls;
    std::vector<brep_prepared_span_node> prepared_span_nodes;
    std::vector<brep_prepared_face_node> prepared_face_nodes;
    std::vector<brep_face_bvh_node> prepared_face_bvh_nodes;
    size_t prepared_face_bvh_root = (size_t)-1;
    std::vector<brep_vertex_record> vertex_records;
    unsigned int prepared_support_flags = 0;
    unsigned int routing_capability_flags = 0;
    unsigned int routing_risk_flags = 0;
    size_t routing_seeded_faces = 0;
    size_t routing_seeded_spans = 0;
    size_t routing_unqualified_faces = 0;
    size_t routing_unsupported_faces = 0;
    size_t routing_reparameterized_faces = 0;
    size_t routing_singular_spans = 0;
    size_t routing_periodic_faces = 0;
    size_t routing_inner_trim_faces = 0;
    size_t routing_boundary_faces = 0;
    size_t routing_nonmanifold_faces = 0;
    size_t routing_seam_faces = 0;
    int directional_cache_depth = 0;
    uint64_t directional_cache_generation = 0;
    /* Fixed storage allocated during prep.  Shot-time cache misses only
     * populate or replace these slots; they never allocate storage. */
    brep_directional_object_shared_cache *directional_object_cache = NULL;
    size_t directional_object_cache_bytes = 0;
    bool routing_metadata_complete = false;
    size_t routing_seeded_cells = 0;
    size_t routing_unsupported_cells = 0;
    size_t routing_trim_boundary_cells = 0;
    size_t routing_domain_boundary_cells = 0;
    size_t routing_correction_cells = 0;
    size_t routing_pack_cache_budget_bytes = 0;
    bool routing_feature_cells_complete = false;
    bool routing_hot_ranges_complete = false;
    bool routing_correction_bvh_complete = false;
    size_t prepared_supported_faces = 0;
    size_t prepared_reparameterized_faces = 0;
    size_t prepared_supported_edges = 0;
    size_t prepared_correspondence_edges = 0;
    size_t prepared_supported_vertices = 0;
    int surface_tree_depth_limit = 0;
    int surface_tree_maximum_depth = 0;
    size_t surface_tree_nodes = 0;
    size_t surface_tree_leaves = 0;
    size_t curve_tree_leaves = 0;
    size_t surface_tree_build_microseconds = 0;
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
