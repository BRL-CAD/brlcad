/*                   S T A T E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file state_private.h
 *
 * Source-private backing storage for public traversal-state refs.
 */

#ifndef BSG_STATE_PRIVATE_H
#define BSG_STATE_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/state.h"

struct bsg_appearance_settings;

__BEGIN_DECLS

struct bsg_state {
    struct bsg_view *view;
    bsg_node *root;
    bsg_node *node;
    mat_t model_mat;
    const struct bsg_appearance_settings *inherited_settings;
    struct bsg_appearance_settings inherited_settings_storage;
    int inherited_visible;
    int visible;
    int force_draw;
    int selected;
    int highlighted;
    bsg_lod_source_ref lod_source;
    int lod_level;
    int lod_level_count;
    int render_phase;
    int view_scope_visible;
    uint64_t source_identity;
    unsigned int backend_capabilities;
    unsigned char material_color[3];
    double material_transparency;
    uint32_t material_revision;
    uint32_t appearance_revision;
    int material_set;
    int draw_mode;
    int line_width;
    int line_style;
    int draw_style_set;
    double complexity;
    int complexity_set;
    int camera_set;
    int camera_projection;
    double camera_perspective;
    point_t camera_position;
    mat_t camera_orientation;
    double camera_near_distance;
    double camera_far_distance;
    int light_set;
    int light_type;
    point_t light_position;
    vect_t light_direction;
    unsigned char light_diffuse_color[3];
    unsigned char light_specular_color[3];
    unsigned char light_ambient_color[3];
    double light_intensity;
    int light_active;
};

extern struct bsg_state *bsg_state_ref_state(bsg_state_ref ref);

__END_DECLS

#endif /* BSG_STATE_PRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
