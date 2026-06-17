/*                         S T A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/state.c
 *
 * Public traversal-state refs.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "bn/mat.h"
#include "bu/malloc.h"
#include "bsg/object.h"
#include "state_private.h"
#include "object_private.h"

static bsg_state_ref
_state_ref(struct bsg_state *state)
{
    bsg_state_ref ref = BSG_STATE_REF_NULL_INIT;
    ref.opaque = state;
    return ref;
}

struct bsg_state *
bsg_state_ref_state(bsg_state_ref ref)
{
    return (struct bsg_state *)ref.opaque;
}

static void
_state_init(struct bsg_state *state)
{
    memset(state, 0, sizeof(*state));
    MAT_IDN(state->model_mat);
    state->inherited_visible = 1;
    state->visible = 1;
    state->lod_level = -1;
    state->render_phase = -1;
    state->view_scope_visible = 1;
    state->material_color[0] = 255;
    state->material_color[1] = 255;
    state->material_color[2] = 255;
    state->material_transparency = 1.0;
    state->line_width = 1;
    state->complexity = 0.5;
    state->camera_projection = BSG_CAMERA_ORTHOGRAPHIC;
    MAT_IDN(state->camera_orientation);
    state->camera_far_distance = INFINITY;
    state->light_type = BSG_LIGHT_DIRECTIONAL;
    VSET(state->light_direction, 0.0, 0.0, -1.0);
    state->light_diffuse_color[0] = 255;
    state->light_diffuse_color[1] = 255;
    state->light_diffuse_color[2] = 255;
    state->light_specular_color[0] = 255;
    state->light_specular_color[1] = 255;
    state->light_specular_color[2] = 255;
    state->light_ambient_color[0] = 51;
    state->light_ambient_color[1] = 51;
    state->light_ambient_color[2] = 51;
    state->light_intensity = 1.0;
    state->light_active = 1;
}

bsg_state_ref
bsg_state_ref_null(void)
{
    bsg_state_ref ref = BSG_STATE_REF_NULL_INIT;
    return ref;
}

int
bsg_state_ref_is_null(bsg_state_ref ref)
{
    return ref.opaque ? 0 : 1;
}

bsg_state_ref
bsg_state_ref_create(void)
{
    struct bsg_state *state;
    BU_ALLOC(state, struct bsg_state);
    _state_init(state);
    return _state_ref(state);
}

bsg_state_ref
bsg_state_ref_copy(bsg_state_ref ref)
{
    struct bsg_state *src = bsg_state_ref_state(ref);
    if (!src)
	return bsg_state_ref_create();

    struct bsg_state *state;
    BU_ALLOC(state, struct bsg_state);
    *state = *src;
    if (src->inherited_settings == &src->inherited_settings_storage)
	state->inherited_settings = &state->inherited_settings_storage;
    return _state_ref(state);
}

bsg_state_ref
bsg_state_ref_push(bsg_state_ref parent)
{
    return bsg_state_ref_copy(parent);
}

void
bsg_state_ref_pop(bsg_state_ref ref)
{
    bsg_state_ref_destroy(ref);
}

void
bsg_state_ref_destroy(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (state)
	bu_free(state, "bsg_state");
}

bsg_node_ref
bsg_state_ref_root(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? bsg_node_ref_from_object(bsg_object_ref_from_node(state->root)) : bsg_node_ref_null();
}

bsg_node_ref
bsg_state_ref_node(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? bsg_node_ref_from_object(bsg_object_ref_from_node(state->node)) : bsg_node_ref_null();
}

void
bsg_state_ref_transform(bsg_state_ref ref, mat_t mat)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (state)
	MAT_COPY(mat, state->model_mat);
    else
	MAT_IDN(mat);
}

void
bsg_state_ref_set_transform(bsg_state_ref ref, const mat_t mat)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (state && mat)
	MAT_COPY(state->model_mat, mat);
}

void
bsg_state_ref_multiply_transform(bsg_state_ref ref, const mat_t mat)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!state || !mat)
	return;
    mat_t product;
    bn_mat_mul(product, state->model_mat, mat);
    MAT_COPY(state->model_mat, product);
}

int
bsg_state_ref_inherited_visible(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->inherited_visible : 0;
}

int
bsg_state_ref_visible(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->visible : 0;
}

int
bsg_state_ref_force_draw(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->force_draw : 0;
}

void
bsg_state_ref_set_visibility(bsg_state_ref ref, int inherited_visible, int visible, int force_draw)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!state)
	return;
    state->inherited_visible = inherited_visible ? 1 : 0;
    state->visible = visible ? 1 : 0;
    state->force_draw = force_draw ? 1 : 0;
}

int
bsg_state_ref_selected(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->selected : 0;
}

int
bsg_state_ref_highlighted(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->highlighted : 0;
}

void
bsg_state_ref_set_selection(bsg_state_ref ref, int selected, int highlighted)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!state)
	return;
    state->selected = selected ? 1 : 0;
    state->highlighted = highlighted ? 1 : 0;
}

int
bsg_state_ref_lod_level(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->lod_level : -1;
}

int
bsg_state_ref_lod_level_count(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->lod_level_count : 0;
}

bsg_lod_source_ref
bsg_state_ref_lod_source(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    bsg_lod_source_ref null_ref = BSG_LOD_SOURCE_REF_NULL_INIT;
    return state ? state->lod_source : null_ref;
}

void
bsg_state_ref_set_lod(bsg_state_ref ref, bsg_lod_source_ref source, int level, int level_count)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!state)
	return;
    state->lod_source = source;
    state->lod_level = level;
    state->lod_level_count = level_count;
}

int
bsg_state_ref_render_phase(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->render_phase : -1;
}

void
bsg_state_ref_set_render_phase(bsg_state_ref ref, int phase)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (state)
	state->render_phase = phase;
}

int
bsg_state_ref_view_scope_visible(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->view_scope_visible : 0;
}

void
bsg_state_ref_set_view_scope_visible(bsg_state_ref ref, int visible)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (state)
	state->view_scope_visible = visible ? 1 : 0;
}

uint64_t
bsg_state_ref_source_identity(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->source_identity : 0;
}

void
bsg_state_ref_set_source_identity(bsg_state_ref ref, uint64_t source_identity)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (state)
	state->source_identity = source_identity;
}

unsigned int
bsg_state_ref_backend_capabilities(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->backend_capabilities : 0;
}

void
bsg_state_ref_set_backend_capabilities(bsg_state_ref ref, unsigned int capabilities)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (state)
	state->backend_capabilities = capabilities;
}

void
bsg_state_ref_material_color(bsg_state_ref ref, unsigned char color[3])
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!color)
	return;
    if (state) {
	color[0] = state->material_color[0];
	color[1] = state->material_color[1];
	color[2] = state->material_color[2];
    } else {
	color[0] = color[1] = color[2] = 0;
    }
}

void
bsg_state_ref_set_material_color(bsg_state_ref ref, const unsigned char color[3])
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!state || !color)
	return;
    state->material_color[0] = color[0];
    state->material_color[1] = color[1];
    state->material_color[2] = color[2];
    state->material_set = 1;
}

double
bsg_state_ref_material_transparency(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->material_transparency : 1.0;
}

void
bsg_state_ref_set_material_transparency(bsg_state_ref ref, double transparency)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (state) {
	state->material_transparency = transparency;
	state->material_set = 1;
    }
}

int
bsg_state_ref_has_material(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->material_set : 0;
}

uint32_t
bsg_state_ref_material_revision(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->material_revision : 0;
}

uint32_t
bsg_state_ref_appearance_revision(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->appearance_revision : 0;
}

void
bsg_state_ref_set_appearance_revisions(bsg_state_ref ref, uint32_t material_revision, uint32_t appearance_revision)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!state)
	return;
    state->material_revision = material_revision;
    state->appearance_revision = appearance_revision;
    state->material_set = 1;
}

int
bsg_state_ref_draw_mode(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->draw_mode : 0;
}

int
bsg_state_ref_line_width(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->line_width : 0;
}

int
bsg_state_ref_line_style(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->line_style : 0;
}

int
bsg_state_ref_has_draw_style(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->draw_style_set : 0;
}

void
bsg_state_ref_set_draw_style(bsg_state_ref ref, int draw_mode, int line_width, int line_style)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!state)
	return;
    state->draw_mode = draw_mode;
    state->line_width = line_width;
    state->line_style = line_style;
    state->draw_style_set = 1;
}

double
bsg_state_ref_complexity(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->complexity : 0.0;
}

int
bsg_state_ref_has_complexity(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->complexity_set : 0;
}

void
bsg_state_ref_set_complexity(bsg_state_ref ref, double complexity)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (state) {
	state->complexity = complexity;
	state->complexity_set = 1;
    }
}

int
bsg_state_ref_has_camera(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->camera_set : 0;
}

int
bsg_state_ref_camera_projection(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->camera_projection : BSG_CAMERA_ORTHOGRAPHIC;
}

double
bsg_state_ref_camera_perspective(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->camera_perspective : 0.0;
}

void
bsg_state_ref_camera_position(bsg_state_ref ref, point_t position)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!position)
	return;
    if (state)
	VMOVE(position, state->camera_position);
    else
	VSETALL(position, 0.0);
}

void
bsg_state_ref_camera_orientation(bsg_state_ref ref, mat_t orientation)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!orientation)
	return;
    if (state)
	MAT_COPY(orientation, state->camera_orientation);
    else
	MAT_IDN(orientation);
}

double
bsg_state_ref_camera_near_distance(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->camera_near_distance : 0.0;
}

double
bsg_state_ref_camera_far_distance(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->camera_far_distance : 0.0;
}

void
bsg_state_ref_set_camera(bsg_state_ref ref,
			 int projection,
			 double perspective,
			 const point_t position,
			 const mat_t orientation,
			 double near_distance,
			 double far_distance)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!state)
	return;
    state->camera_projection = projection;
    state->camera_perspective = perspective;
    if (position)
	VMOVE(state->camera_position, position);
    if (orientation)
	MAT_COPY(state->camera_orientation, orientation);
    state->camera_near_distance = near_distance;
    state->camera_far_distance = far_distance;
    state->camera_set = 1;
}

int
bsg_state_ref_has_light(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->light_set : 0;
}

int
bsg_state_ref_light_type(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->light_type : BSG_LIGHT_DIRECTIONAL;
}

void
bsg_state_ref_light_position(bsg_state_ref ref, point_t position)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!position)
	return;
    if (state)
	VMOVE(position, state->light_position);
    else
	VSETALL(position, 0.0);
}

void
bsg_state_ref_light_direction(bsg_state_ref ref, vect_t direction)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!direction)
	return;
    if (state)
	VMOVE(direction, state->light_direction);
    else
	VSETALL(direction, 0.0);
}

void
bsg_state_ref_light_diffuse_color(bsg_state_ref ref, unsigned char color[3])
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!color)
	return;
    if (state) {
	color[0] = state->light_diffuse_color[0];
	color[1] = state->light_diffuse_color[1];
	color[2] = state->light_diffuse_color[2];
    } else {
	color[0] = color[1] = color[2] = 0;
    }
}

void
bsg_state_ref_light_specular_color(bsg_state_ref ref, unsigned char color[3])
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!color)
	return;
    if (state) {
	color[0] = state->light_specular_color[0];
	color[1] = state->light_specular_color[1];
	color[2] = state->light_specular_color[2];
    } else {
	color[0] = color[1] = color[2] = 0;
    }
}

void
bsg_state_ref_light_ambient_color(bsg_state_ref ref, unsigned char color[3])
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!color)
	return;
    if (state) {
	color[0] = state->light_ambient_color[0];
	color[1] = state->light_ambient_color[1];
	color[2] = state->light_ambient_color[2];
    } else {
	color[0] = color[1] = color[2] = 0;
    }
}

double
bsg_state_ref_light_intensity(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->light_intensity : 0.0;
}

int
bsg_state_ref_light_active(bsg_state_ref ref)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    return state ? state->light_active : 0;
}

void
bsg_state_ref_set_light(bsg_state_ref ref,
			int type,
			const point_t position,
			const vect_t direction,
			const unsigned char diffuse_color[3],
			const unsigned char specular_color[3],
			const unsigned char ambient_color[3],
			double intensity,
			int active)
{
    struct bsg_state *state = bsg_state_ref_state(ref);
    if (!state)
	return;
    state->light_type = type;
    if (position)
	VMOVE(state->light_position, position);
    if (direction)
	VMOVE(state->light_direction, direction);
    if (diffuse_color) {
	state->light_diffuse_color[0] = diffuse_color[0];
	state->light_diffuse_color[1] = diffuse_color[1];
	state->light_diffuse_color[2] = diffuse_color[2];
    }
    if (specular_color) {
	state->light_specular_color[0] = specular_color[0];
	state->light_specular_color[1] = specular_color[1];
	state->light_specular_color[2] = specular_color[2];
    }
    if (ambient_color) {
	state->light_ambient_color[0] = ambient_color[0];
	state->light_ambient_color[1] = ambient_color[1];
	state->light_ambient_color[2] = ambient_color[2];
    }
    state->light_intensity = intensity;
    state->light_active = active ? 1 : 0;
    state->light_set = 1;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
