/*              T E S T _ S T A T E _ N O D E S . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/tests/test_state_nodes.c
 *
 * Stage 8 typed state-node tests.
 */

#include "common.h"

#include "../action_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bsg/action.h"
#include "bsg/camera.h"
#include "bsg/complexity.h"
#include "bsg/draw_style.h"
#include "bsg/group.h"
#include "bsg/light.h"
#include "bsg/material.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/separator.h"
#include "bsg/state.h"
#include "bsg/util.h"

#define CHECK(_expr, _msg) \
    do { \
	if (!(_expr)) { \
	    printf("FAIL: %s\n", (_msg)); \
	    return 1; \
	} \
    } while (0)

struct state_capture {
    int seen_shape;
    int material_seen;
    int style_seen;
    int complexity_seen;
    int camera_seen;
    int light_seen;
    unsigned char material_color[3];
    double alpha;
    int draw_mode;
    int line_width;
    int line_style;
    double complexity;
    int camera_projection;
    double camera_perspective;
    point_t camera_position;
    int light_type;
    unsigned char light_diffuse[3];
    double light_intensity;
};

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v = NULL;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "state_nodes_test_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "state nodes view");
}

static int
capture_shape_state(bsg_action_ref UNUSED(action),
		    bsg_node_ref UNUSED(node),
		    bsg_state_ref state,
		    void *userdata)
{
    struct state_capture *cap = (struct state_capture *)userdata;
    if (!cap)
	return 1;

    cap->seen_shape = 1;
    cap->material_seen = bsg_state_ref_has_material(state);
    cap->style_seen = bsg_state_ref_has_draw_style(state);
    cap->complexity_seen = bsg_state_ref_has_complexity(state);
    cap->camera_seen = bsg_state_ref_has_camera(state);
    cap->light_seen = bsg_state_ref_has_light(state);
    bsg_state_ref_material_color(state, cap->material_color);
    cap->alpha = bsg_state_ref_material_transparency(state);
    cap->draw_mode = bsg_state_ref_draw_mode(state);
    cap->line_width = bsg_state_ref_line_width(state);
    cap->line_style = bsg_state_ref_line_style(state);
    cap->complexity = bsg_state_ref_complexity(state);
    cap->camera_projection = bsg_state_ref_camera_projection(state);
    cap->camera_perspective = bsg_state_ref_camera_perspective(state);
    bsg_state_ref_camera_position(state, cap->camera_position);
    cap->light_type = bsg_state_ref_light_type(state);
    bsg_state_ref_light_diffuse_color(state, cap->light_diffuse);
    cap->light_intensity = bsg_state_ref_light_intensity(state);
    return 1;
}

static int
test_typed_state_node_fields_and_traversal(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_material_ref material = bsg_material_ref_create(v, "mat");
    bsg_draw_style_ref style = bsg_draw_style_ref_create(v, "style");
    bsg_complexity_ref complexity = bsg_complexity_ref_create(v, "complexity");
    bsg_camera_ref camera = bsg_camera_ref_create(v, "camera");
    bsg_light_ref light = bsg_light_ref_create(v, "light");
    bsg_shape_ref shape = bsg_shape_ref_create(v, "shape");
    unsigned char red[3] = {200, 40, 20};
    unsigned char blue[3] = {10, 20, 220};
    point_t camera_pos;
    point_t light_pos;
    vect_t light_dir;
    struct state_capture cap;

    VSET(camera_pos, 1.0, 2.0, 3.0);
    VSET(light_pos, 4.0, 5.0, 6.0);
    VSET(light_dir, 0.0, 1.0, 0.0);
    memset(&cap, 0, sizeof(cap));

    CHECK(bsg_material_ref_set_diffuse_color(material, red), "set material diffuse");
    CHECK(bsg_material_ref_set_alpha(material, 0.25), "set material alpha");
    CHECK(bsg_draw_style_ref_set_fill_mode(style, 2), "set draw fill mode");
    CHECK(bsg_draw_style_ref_set_line_width(style, 7), "set line width");
    CHECK(bsg_draw_style_ref_set_line_style(style, 3), "set line style");
    CHECK(bsg_complexity_ref_set_value(complexity, 0.75), "set complexity");
    CHECK(bsg_camera_ref_set_projection(camera, BSG_CAMERA_PERSPECTIVE), "set camera projection");
    CHECK(bsg_camera_ref_set_perspective(camera, 35.0), "set camera perspective");
    CHECK(bsg_camera_ref_set_position(camera, camera_pos), "set camera position");
    CHECK(bsg_light_ref_set_type(light, BSG_LIGHT_POINT), "set light type");
    CHECK(bsg_light_ref_set_position(light, light_pos), "set light position");
    CHECK(bsg_light_ref_set_direction(light, light_dir), "set light direction");
    CHECK(bsg_light_ref_set_diffuse_color(light, blue), "set light diffuse");
    CHECK(bsg_light_ref_set_intensity(light, 2.5), "set light intensity");

    bsg_separator_ref_append_child(root, bsg_material_ref_as_node(material));
    bsg_separator_ref_append_child(root, bsg_draw_style_ref_as_node(style));
    bsg_separator_ref_append_child(root, bsg_complexity_ref_as_node(complexity));
    bsg_separator_ref_append_child(root, bsg_camera_ref_as_node(camera));
    bsg_separator_ref_append_child(root, bsg_light_ref_as_node(light));
    bsg_separator_ref_append_child(root, bsg_shape_ref_as_node(shape));

    bsg_type_id action_type = bsg_type_register("BSGStage8StateNodeTestAction", bsg_action_type());
    CHECK(bsg_action_method_register(action_type, bsg_shape_type(), capture_shape_state, &cap),
	    "register shape state method");
    bsg_action_ref action = bsg_action_ref_create(action_type);
    CHECK(!bsg_action_ref_is_null(action), "create custom action");
    CHECK(bsg_action_apply(action, bsg_separator_ref_as_node(root)), "apply custom action");
    bsg_action_ref_destroy(action);

    CHECK(cap.seen_shape, "shape visited");
    CHECK(cap.material_seen, "material state inherited by sibling shape");
    CHECK(cap.material_color[0] == red[0] && cap.material_color[1] == red[1] &&
	    cap.material_color[2] == red[2], "material color inherited");
    CHECK(NEAR_ZERO(cap.alpha - 0.25, SMALL_FASTF), "material alpha inherited");
    CHECK(cap.style_seen && cap.draw_mode == 2 && cap.line_width == 7 &&
	    cap.line_style == 3, "draw style inherited");
    CHECK(cap.complexity_seen && NEAR_ZERO(cap.complexity - 0.75, SMALL_FASTF),
	    "complexity inherited");
    CHECK(cap.camera_seen && cap.camera_projection == BSG_CAMERA_PERSPECTIVE &&
	    NEAR_ZERO(cap.camera_perspective - 35.0, SMALL_FASTF) &&
	    VNEAR_EQUAL(cap.camera_position, camera_pos, SMALL_FASTF),
	    "camera state inherited");
    CHECK(cap.light_seen && cap.light_type == BSG_LIGHT_POINT &&
	    cap.light_diffuse[0] == blue[0] && cap.light_diffuse[1] == blue[1] &&
	    cap.light_diffuse[2] == blue[2] &&
	    NEAR_ZERO(cap.light_intensity - 2.5, SMALL_FASTF),
	    "light state inherited");

    free_view(v);
    return 0;
}

static int
test_render_consumes_state_nodes(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_material_ref material = bsg_material_ref_create(v, "mat");
    bsg_draw_style_ref style = bsg_draw_style_ref_create(v, "style");
    bsg_shape_ref shape = bsg_shape_ref_create(v, "shape");
    unsigned char color[3] = {12, 34, 56};
    struct bsg_render_request *req = NULL;
    struct bsg_render_batch *batch = NULL;

    CHECK(bsg_material_ref_set_diffuse_color(material, color), "set render material");
    CHECK(bsg_material_ref_set_alpha(material, 0.5), "set render material alpha");
    CHECK(bsg_draw_style_ref_set_line_width(style, 4), "set render line width");
    CHECK(bsg_draw_style_ref_set_line_style(style, 9), "set render line style");
    CHECK(bsg_draw_style_ref_set_fill_mode(style, 1), "set render fill mode");

    bsg_separator_ref_append_child(root, bsg_material_ref_as_node(material));
    bsg_separator_ref_append_child(root, bsg_draw_style_ref_as_node(style));
    bsg_separator_ref_append_child(root, bsg_shape_ref_as_node(shape));

    req = bsg_render_request_create(v, NULL);
    batch = bsg_render_batch_create();
    CHECK(req && batch, "create render request and batch");
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);
    CHECK(bsg_render_request_collect(req, batch) == 1, "collect one render item");
    CHECK(bsg_render_batch_count(batch) == 1, "batch has one item");
    const struct bsg_render_item *item = bsg_render_batch_get(batch, 0);
    CHECK(item != NULL, "get render item");
    CHECK(item->appearance.color[0] == color[0] &&
	    item->appearance.color[1] == color[1] &&
	    item->appearance.color[2] == color[2], "render item material color from state node");
    CHECK(NEAR_ZERO(item->appearance.transparency - 0.5, SMALL_FASTF),
	    "render item alpha from state node");
    CHECK(item->appearance.line_width == 4 && item->appearance.line_style == 9 &&
	    item->appearance.draw_mode == 1, "render item draw style from state node");

    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);
    free_view(v);
    return 0;
}

int
main(int UNUSED(argc), const char **argv)
{
    int failures = 0;
    bu_setprogname(argv[0]);
    failures += test_typed_state_node_fields_and_traversal();
    failures += test_render_consumes_state_nodes();
    if (failures == 0)
	printf("RESULT: all Stage 8 state-node tests PASSED\n");
    else
	printf("RESULT: %d Stage 8 state-node test(s) FAILED\n", failures);
    return failures;
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
