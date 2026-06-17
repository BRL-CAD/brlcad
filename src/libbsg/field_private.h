/*                    F I E L D _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file field_private.h
 *
 * Private legacy field flag and sensor notification hooks.
 */

#ifndef BSG_FIELD_PRIVATE_H
#define BSG_FIELD_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "bsg/field.h"

__BEGIN_DECLS

#ifndef BSG_LEGACY_FIELD_ID_DEFINED
#define BSG_LEGACY_FIELD_ID_DEFINED
typedef enum {
    BSG_FIELD_UNKNOWN                  = 0,
    BSG_FIELD_FLAG                     = 1,
    BSG_FIELD_COLOR                    = 2,
    BSG_FIELD_VISIBILITY               = 3,
    BSG_FIELD_TRANSFORM                = 4,
    BSG_FIELD_CHILDREN                 = 5,
    BSG_FIELD_NAME                     = 6,
    BSG_FIELD_BOUNDS                   = 7,
    BSG_FIELD_SWITCH_WHICH             = 8,
    BSG_FIELD_MATERIAL_DIFFUSE_COLOR   = 9,
    BSG_FIELD_MATERIAL_SPECULAR_COLOR  = 10,
    BSG_FIELD_MATERIAL_EMISSIVE_COLOR  = 11,
    BSG_FIELD_MATERIAL_ALPHA           = 12,
    BSG_FIELD_MATERIAL_SHININESS       = 13,
    BSG_FIELD_MATERIAL_LINE_COLOR      = 14,
    BSG_FIELD_DRAW_LINE_WIDTH          = 15,
    BSG_FIELD_DRAW_LINE_STYLE          = 16,
    BSG_FIELD_DRAW_FILL_MODE           = 17,
    BSG_FIELD_DRAW_POINT_SIZE          = 18,
    BSG_FIELD_DRAW_TRANSPARENCY_POLICY = 19,
    BSG_FIELD_COMPLEXITY_VALUE         = 20,
    BSG_FIELD_COMPLEXITY_CURVE_SCALE   = 21,
    BSG_FIELD_COMPLEXITY_POINT_SCALE   = 22,
    BSG_FIELD_COMPLEXITY_LOD_POLICY    = 23,
    BSG_FIELD_COMPLEXITY_LOD_LEVEL     = 24,
    BSG_FIELD_CAMERA_PROJECTION        = 25,
    BSG_FIELD_CAMERA_PERSPECTIVE       = 26,
    BSG_FIELD_CAMERA_POSITION          = 27,
    BSG_FIELD_CAMERA_ORIENTATION       = 28,
    BSG_FIELD_CAMERA_NEAR_DISTANCE     = 29,
    BSG_FIELD_CAMERA_FAR_DISTANCE      = 30,
    BSG_FIELD_LIGHT_TYPE               = 31,
    BSG_FIELD_LIGHT_POSITION           = 32,
    BSG_FIELD_LIGHT_DIRECTION          = 33,
    BSG_FIELD_LIGHT_DIFFUSE_COLOR      = 34,
    BSG_FIELD_LIGHT_SPECULAR_COLOR     = 35,
    BSG_FIELD_LIGHT_AMBIENT_COLOR      = 36,
    BSG_FIELD_LIGHT_INTENSITY          = 37,
    BSG_FIELD_LIGHT_ACTIVE             = 38,
    BSG_FIELD_GEOMETRY_KIND            = 39,
    BSG_FIELD_GEOMETRY_COORDINATES     = 40,
    BSG_FIELD_GEOMETRY_NORMALS         = 41,
    BSG_FIELD_GEOMETRY_COLORS          = 42,
    BSG_FIELD_GEOMETRY_INDICES         = 43,
    BSG_FIELD_GEOMETRY_PRIMITIVE_SETS  = 44,
    BSG_FIELD_GEOMETRY_TEXT            = 45,
    BSG_FIELD_GEOMETRY_IMAGE_WIDTH     = 46,
    BSG_FIELD_GEOMETRY_IMAGE_HEIGHT    = 47,
    BSG_FIELD_GEOMETRY_IMAGE_CHANNELS  = 48,
    BSG_FIELD_GEOMETRY_SOURCE_REVISION = 49,
    BSG_FIELD_LOD_SELECTION            = 50,
    BSG_FIELD_LOD_ACTIVE_LEVEL         = 51,
    BSG_FIELD_LOD_RANGES               = 52,
    BSG_FIELD_DATABASE_SOURCE_PATH     = 53,
    BSG_FIELD_DATABASE_SOURCE_DRAW_MODE = 54,
    BSG_FIELD_DATABASE_SOURCE_MATERIAL_POLICY = 55,
    BSG_FIELD_DATABASE_SOURCE_REVISION = 56,
    BSG_FIELD_DATABASE_SOURCE_INPUTS_REVISION = 57,
    BSG_FIELD_DATABASE_SOURCE_REALIZED_REVISION = 58,
    BSG_FIELD_DATABASE_SOURCE_REALIZED_INPUTS_REVISION = 59,
    BSG_FIELD_DATABASE_SOURCE_STALE_REASON = 60,
    BSG_FIELD_DATABASE_SOURCE_REALIZATION_IDENTITY = 61,
    BSG_FIELD_DATABASE_SOURCE_REALIZATION_STATUS = 62,
    BSG_FIELD_DATABASE_SOURCE_REALIZATION_ROLE_FLAGS = 63,
    BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_DEPENDENT = 64,
    BSG_FIELD_DATABASE_SOURCE_REALIZATION_VIEW_SCALE = 65,
    BSG_FIELD_DATABASE_SOURCE_REALIZATION_BOT_THRESHOLD = 66,
    BSG_FIELD_DATABASE_SOURCE_REALIZATION_CURVE_SCALE = 67,
    BSG_FIELD_DATABASE_SOURCE_REALIZATION_POINT_SCALE = 68,
    BSG_FIELD_DRAW_ARROW_TIP_LENGTH = 69,
    BSG_FIELD_DRAW_ARROW_TIP_WIDTH = 70
} bsg_field_id_t;
#endif

struct bsg_field {
    struct bsg_node *owner;
    const char *name;
    bsg_field_value_type value_type;
    bsg_field_id_t id;
    uint64_t revision;
    struct bsg_field *connection;
    union {
	int bool_value;
	int int_value;
	uint64_t uint64_value;
	float float_value;
	double double_value;
	int enum_value;
	char *string_value;
	vect_t vec3_value;
	unsigned char color3_value[3];
	mat_t matrix_value;
	struct {
	    point_t min;
	    point_t max;
	    int valid;
	} bbox_value;
	bsg_object_ref object_value;
	bsg_node_ref node_value;
	struct {
	    double *values;
	    size_t count;
	} multi_double_value;
	struct {
	    point_t *values;
	    size_t count;
	} multi_point_value;
	struct {
	    int *values;
	    size_t count;
	} multi_int_value;
	struct {
	    unsigned char (*values)[3];
	    size_t count;
	} multi_color3_value;
    } value;
};

struct bsg_node_fields {
    struct bsg_field name;
    struct bsg_field visibility;
    struct bsg_field color;
    struct bsg_field transform;
    struct bsg_field bounds;
    struct bsg_field switch_which_child;
    struct bsg_field children;
    struct bsg_field material_diffuse_color;
    struct bsg_field material_specular_color;
    struct bsg_field material_emissive_color;
    struct bsg_field material_alpha;
    struct bsg_field material_shininess;
    struct bsg_field material_line_color;
    struct bsg_field draw_line_width;
    struct bsg_field draw_line_style;
    struct bsg_field draw_fill_mode;
    struct bsg_field draw_point_size;
    struct bsg_field draw_transparency_policy;
    struct bsg_field draw_arrow_tip_length;
    struct bsg_field draw_arrow_tip_width;
    struct bsg_field complexity_value;
    struct bsg_field complexity_curve_scale;
    struct bsg_field complexity_point_scale;
    struct bsg_field complexity_lod_policy;
    struct bsg_field complexity_lod_level;
    struct bsg_field camera_projection;
    struct bsg_field camera_perspective;
    struct bsg_field camera_position;
    struct bsg_field camera_orientation;
    struct bsg_field camera_near_distance;
    struct bsg_field camera_far_distance;
    struct bsg_field light_type;
    struct bsg_field light_position;
    struct bsg_field light_direction;
    struct bsg_field light_diffuse_color;
    struct bsg_field light_specular_color;
    struct bsg_field light_ambient_color;
    struct bsg_field light_intensity;
    struct bsg_field light_active;
    struct bsg_field geometry_kind;
    struct bsg_field geometry_coordinates;
    struct bsg_field geometry_normals;
    struct bsg_field geometry_colors;
    struct bsg_field geometry_indices;
    struct bsg_field geometry_primitive_sets;
    struct bsg_field geometry_text;
    struct bsg_field geometry_image_width;
    struct bsg_field geometry_image_height;
    struct bsg_field geometry_image_channels;
    struct bsg_field geometry_source_revision;
    struct bsg_field lod_selection;
    struct bsg_field lod_active_level;
    struct bsg_field lod_ranges;
    struct bsg_field database_source_path;
    struct bsg_field database_source_draw_mode;
    struct bsg_field database_source_material_policy;
    struct bsg_field database_source_revision;
    struct bsg_field database_source_inputs_revision;
    struct bsg_field database_source_realized_revision;
    struct bsg_field database_source_realized_inputs_revision;
    struct bsg_field database_source_stale_reason;
    struct bsg_field database_source_realization_identity;
    struct bsg_field database_source_realization_status;
    struct bsg_field database_source_realization_role_flags;
    struct bsg_field database_source_realization_view_dependent;
    struct bsg_field database_source_realization_view_scale;
    struct bsg_field database_source_realization_bot_threshold;
    struct bsg_field database_source_realization_curve_scale;
    struct bsg_field database_source_realization_point_scale;
};

BSG_EXPORT extern void bsg_node_fields_reset(bsg_node *n);
extern void bsg_node_fields_free(bsg_node *n);
extern bsg_field_ref bsg_node_field_ref(bsg_node *n, bsg_field_id_t fid);
extern void bsg_node_field_touch(bsg_node *n, bsg_field_id_t fid);
extern void bsg_node_set_flag(bsg_node *n, int flag);
extern int bsg_node_get_flag(const bsg_node *n);

BSG_EXPORT extern void bsg_node_set_color(bsg_node *n,
					   unsigned char r,
					   unsigned char g,
					   unsigned char b);
BSG_EXPORT extern void bsg_node_get_color(const bsg_node *n,
					   unsigned char *r,
					   unsigned char *g,
					   unsigned char *b);
BSG_EXPORT extern void bsg_node_set_visible(bsg_node *n, int on);
BSG_EXPORT extern int bsg_node_get_visible(const bsg_node *n);

__END_DECLS

#endif /* BSG_FIELD_PRIVATE_H */
