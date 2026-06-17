/*             T E S T _ G E O M E T R Y _ N O D E S . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/tests/test_geometry_nodes.c
 *
 * Stage 9 field-backed geometry node tests.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "vmath.h"

#include "bsg/field.h"
#include "bsg/geometry.h"
#include "bsg/group.h"
#include "../payload_typed_private.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/separator.h"
#include "bsg/util.h"
#include "../geometry_private.h"
#include "../node_private.h"
#include "../node_storage_private.h"
#include "../object_private.h"

#define CHECK(_expr, _msg) \
    do { \
	if (!(_expr)) { \
	    printf("FAIL: %s\n", (_msg)); \
	    return 1; \
	} \
    } while (0)

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v = NULL;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "geometry_nodes_test_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "geometry nodes view");
}

static int
test_line_set_fields_and_render(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_line_set_ref lines = bsg_line_set_ref_create(v, "lines");
    point_t pts[2];
    point_t got;
    int cmds[2] = { BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW };
    int got_cmd = -1;
    uint64_t rev0 = 0;
    uint64_t rev1 = 0;
    point_t bmin, bmax;
    struct bsg_render_request *req = NULL;
    struct bsg_render_batch *batch = NULL;
    const struct bsg_render_item *item = NULL;
    bsg_node *line_node = NULL;

    VSET(pts[0], 1.0, 2.0, 3.0);
    VSET(pts[1], 4.0, 5.0, 6.0);
    CHECK(!bsg_node_ref_is_null(bsg_line_set_ref_as_node(lines)), "create line-set node");
    CHECK(bsg_node_is_a(bsg_line_set_ref_as_node(lines), bsg_line_set_type()), "line-set runtime type");
    CHECK(bsg_geometry_ref_kind(bsg_line_set_ref_as_geometry(lines)) == BSG_GEOMETRY_NODE_LINE_SET,
	    "line-set geometry kind");

    rev0 = bsg_geometry_ref_revision(bsg_line_set_ref_as_geometry(lines));
    CHECK(bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 2), "set line-set points");
    line_node = bsg_object_ref_node(bsg_node_ref_object(bsg_line_set_ref_as_node(lines)));
    CHECK(bsg_node_get_payload(line_node) == NULL, "line-set setter stores field geometry without payload mirror");
    CHECK(bsg_node_has_geometry_role(line_node, BSG_GEOMETRY_LINE_SET),
	    "line-set setter publishes line geometry role metadata");
    CHECK(bsg_node_has_geometry_role(line_node, BSG_GEOMETRY_LINE_SET),
	    "line-set geometry role is metadata-backed");
    rev1 = bsg_geometry_ref_revision(bsg_line_set_ref_as_geometry(lines));
    CHECK(rev1 > rev0, "geometry revision increments");
    CHECK(bsg_line_set_ref_point_count(lines) == 2, "line-set field point count");
    CHECK(bsg_line_set_ref_point_at(lines, 1, got), "line-set point at");
    CHECK(VNEAR_EQUAL(got, pts[1], SMALL_FASTF), "line-set point stored in field");
    CHECK(bsg_line_set_ref_command_at(lines, 1, &got_cmd) && got_cmd == BSG_GEOMETRY_LINE_DRAW,
	    "line-set command stored in primitive-set field");
    CHECK(bsg_node_ref_bounds(bsg_line_set_ref_as_node(lines), bmin, bmax),
	    "line-set bounds field valid");
    CHECK(VNEAR_EQUAL(bmin, pts[0], SMALL_FASTF) && VNEAR_EQUAL(bmax, pts[1], SMALL_FASTF),
	    "line-set bounds from coordinate field");

    bsg_separator_ref_append_child(root, bsg_line_set_ref_as_node(lines));
    req = bsg_render_request_create(v, NULL);
    batch = bsg_render_batch_create();
    CHECK(req && batch, "create render request");
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);
    CHECK(bsg_render_request_collect(req, batch) == 1, "collect line-set render item");
    CHECK(bsg_render_batch_count(batch) == 1, "line-set batch count");
    item = bsg_render_batch_get(batch, 0);
    CHECK(item && item->geometry.kind == BSG_RENDER_GEOMETRY_LINE_SET, "render kind from geometry node");
    CHECK(item->geometry.element_count == 2, "render element count from coordinate field");
    CHECK(item->geometry.arrays.point_count == 2 && item->geometry.arrays.points &&
	    VNEAR_EQUAL(item->geometry.arrays.points[1], pts[1], SMALL_FASTF),
	    "render line-set points snapshot from coordinate field");
    CHECK(item->geometry.arrays.command_count == 2 && item->geometry.arrays.commands &&
	    item->geometry.arrays.commands[1] == BSG_GEOMETRY_LINE_DRAW,
	    "render line-set commands snapshot from primitive-set field");
    CHECK(item->geometry.revision == rev1, "render revision from geometry field");

    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);
    free_view(v);
    return 0;
}

static int
test_geometry_ref_conversions(void)
{
    struct bsg_view *v = make_view();
    bsg_line_set_ref lines = bsg_line_set_ref_create(v, "lines");
    bsg_indexed_line_set_ref ilines = bsg_indexed_line_set_ref_create(v, "indexed-lines");
    bsg_point_set_ref points = bsg_point_set_ref_create(v, "points");
    bsg_indexed_face_set_ref faces = bsg_indexed_face_set_ref_create(v, "faces");
    bsg_mesh_ref mesh = bsg_mesh_ref_create(v, "mesh");
    bsg_text_ref text = bsg_text_ref_create(v, "text");
    bsg_image_ref image = bsg_image_ref_create(v, "image");
    bsg_framebuffer_ref framebuffer = bsg_framebuffer_ref_create(v, "framebuffer");
    bsg_overlay_geometry_ref overlay = bsg_overlay_geometry_ref_create(v, "overlay");
    bsg_hud_geometry_ref hud = bsg_hud_geometry_ref_create(v, "hud");
    bsg_annotation_ref annotation = bsg_annotation_ref_create(v, "annotation");
    bsg_csg_proxy_ref csg = bsg_csg_proxy_ref_create(v, "csg");
    bsg_brep_proxy_ref brep = bsg_brep_proxy_ref_create(v, "brep");
    bsg_edit_preview_ref preview = bsg_edit_preview_ref_create(v, "preview");

    CHECK(bsg_node_ref_equal(bsg_line_set_ref_as_node(lines),
		bsg_geometry_ref_as_node(bsg_line_set_ref_as_geometry(lines))), "line-set geometry cast");
    CHECK(bsg_node_ref_equal(bsg_indexed_line_set_ref_as_node(ilines),
		bsg_geometry_ref_as_node(bsg_indexed_line_set_ref_as_geometry(ilines))), "indexed-line geometry cast");
    CHECK(bsg_node_ref_equal(bsg_point_set_ref_as_node(points),
		bsg_geometry_ref_as_node(bsg_point_set_ref_as_geometry(points))), "point-set geometry cast");
    CHECK(bsg_node_ref_equal(bsg_indexed_face_set_ref_as_node(faces),
		bsg_geometry_ref_as_node(bsg_indexed_face_set_ref_as_geometry(faces))), "indexed-face geometry cast");
    CHECK(bsg_node_ref_equal(bsg_mesh_ref_as_node(mesh),
		bsg_geometry_ref_as_node(bsg_mesh_ref_as_geometry(mesh))), "mesh geometry cast");
    CHECK(bsg_node_ref_equal(bsg_text_ref_as_node(text),
		bsg_geometry_ref_as_node(bsg_text_ref_as_geometry(text))), "text geometry cast");
    CHECK(bsg_node_ref_equal(bsg_image_ref_as_node(image),
		bsg_geometry_ref_as_node(bsg_image_ref_as_geometry(image))), "image geometry cast");
    CHECK(bsg_node_ref_equal(bsg_framebuffer_ref_as_node(framebuffer),
		bsg_geometry_ref_as_node(bsg_framebuffer_ref_as_geometry(framebuffer))), "framebuffer geometry cast");
    CHECK(bsg_node_ref_equal(bsg_overlay_geometry_ref_as_node(overlay),
		bsg_geometry_ref_as_node(bsg_overlay_geometry_ref_as_geometry(overlay))), "overlay geometry cast");
    CHECK(bsg_node_ref_equal(bsg_hud_geometry_ref_as_node(hud),
		bsg_geometry_ref_as_node(bsg_hud_geometry_ref_as_geometry(hud))), "hud geometry cast");
    CHECK(bsg_node_ref_equal(bsg_annotation_ref_as_node(annotation),
		bsg_geometry_ref_as_node(bsg_annotation_ref_as_geometry(annotation))), "annotation geometry cast");
    CHECK(bsg_node_ref_equal(bsg_csg_proxy_ref_as_node(csg),
		bsg_geometry_ref_as_node(bsg_csg_proxy_ref_as_geometry(csg))), "csg geometry cast");
    CHECK(bsg_node_ref_equal(bsg_brep_proxy_ref_as_node(brep),
		bsg_geometry_ref_as_node(bsg_brep_proxy_ref_as_geometry(brep))), "brep geometry cast");
    CHECK(bsg_node_ref_equal(bsg_edit_preview_ref_as_node(preview),
		bsg_geometry_ref_as_node(bsg_edit_preview_ref_as_geometry(preview))), "edit-preview geometry cast");

    CHECK(!bsg_node_ref_is_null(bsg_line_set_ref_as_node(bsg_node_ref_as_line_set(bsg_line_set_ref_as_node(lines)))),
	    "line-set node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_indexed_line_set_ref_as_node(bsg_node_ref_as_indexed_line_set(bsg_indexed_line_set_ref_as_node(ilines)))),
	    "indexed-line node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_point_set_ref_as_node(bsg_node_ref_as_point_set(bsg_point_set_ref_as_node(points)))),
	    "point-set node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_indexed_face_set_ref_as_node(bsg_node_ref_as_indexed_face_set(bsg_indexed_face_set_ref_as_node(faces)))),
	    "indexed-face node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_mesh_ref_as_node(bsg_node_ref_as_mesh(bsg_mesh_ref_as_node(mesh)))),
	    "mesh node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_text_ref_as_node(bsg_node_ref_as_text(bsg_text_ref_as_node(text)))),
	    "text node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_image_ref_as_node(bsg_node_ref_as_image(bsg_image_ref_as_node(image)))),
	    "image node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_framebuffer_ref_as_node(bsg_node_ref_as_framebuffer(bsg_framebuffer_ref_as_node(framebuffer)))),
	    "framebuffer node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_overlay_geometry_ref_as_node(bsg_node_ref_as_overlay_geometry(bsg_overlay_geometry_ref_as_node(overlay)))),
	    "overlay node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_hud_geometry_ref_as_node(bsg_node_ref_as_hud_geometry(bsg_hud_geometry_ref_as_node(hud)))),
	    "hud node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_annotation_ref_as_node(bsg_node_ref_as_annotation(bsg_annotation_ref_as_node(annotation)))),
	    "annotation node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_csg_proxy_ref_as_node(bsg_node_ref_as_csg_proxy(bsg_csg_proxy_ref_as_node(csg)))),
	    "csg node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_brep_proxy_ref_as_node(bsg_node_ref_as_brep_proxy(bsg_brep_proxy_ref_as_node(brep)))),
	    "brep node downcast");
    CHECK(!bsg_node_ref_is_null(bsg_edit_preview_ref_as_node(bsg_node_ref_as_edit_preview(bsg_edit_preview_ref_as_node(preview)))),
	    "edit-preview node downcast");
    CHECK(bsg_node_ref_is_null(bsg_mesh_ref_as_node(bsg_node_ref_as_mesh(bsg_line_set_ref_as_node(lines)))),
	    "unrelated geometry subtype downcast is null");

    bsg_node_ref_destroy(bsg_edit_preview_ref_as_node(preview));
    bsg_node_ref_destroy(bsg_brep_proxy_ref_as_node(brep));
    bsg_node_ref_destroy(bsg_csg_proxy_ref_as_node(csg));
    bsg_node_ref_destroy(bsg_annotation_ref_as_node(annotation));
    bsg_node_ref_destroy(bsg_hud_geometry_ref_as_node(hud));
    bsg_node_ref_destroy(bsg_overlay_geometry_ref_as_node(overlay));
    bsg_node_ref_destroy(bsg_framebuffer_ref_as_node(framebuffer));
    bsg_node_ref_destroy(bsg_image_ref_as_node(image));
    bsg_node_ref_destroy(bsg_text_ref_as_node(text));
    bsg_node_ref_destroy(bsg_mesh_ref_as_node(mesh));
    bsg_node_ref_destroy(bsg_indexed_face_set_ref_as_node(faces));
    bsg_node_ref_destroy(bsg_point_set_ref_as_node(points));
    bsg_node_ref_destroy(bsg_indexed_line_set_ref_as_node(ilines));
    bsg_node_ref_destroy(bsg_line_set_ref_as_node(lines));
    free_view(v);
    return 0;
}

static int
test_private_realizations_update_geometry_kind(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_line_set_ref lines = bsg_line_set_ref_create(v, "ged-geometry");
    bsg_node *node = bsg_object_ref_node(bsg_node_ref_object(bsg_line_set_ref_as_node(lines)));
    struct bsg_payload *mesh = bsg_payload_mesh_create(NULL);
    struct bsg_render_request *req = NULL;
    struct bsg_render_batch *batch = NULL;
    const struct bsg_render_item *item = NULL;

    CHECK(node != NULL, "private mesh test node");
    CHECK(mesh != NULL, "create mesh payload");
    CHECK(bsg_geometry_ref_kind(bsg_line_set_ref_as_geometry(lines)) == BSG_GEOMETRY_NODE_LINE_SET,
	    "test node starts as line set");
    CHECK(bsg_geometry_node_set_private_realization(node, mesh),
	    "install private mesh realization");
    CHECK(bsg_geometry_node_kind_get(node) == BSG_GEOMETRY_NODE_MESH,
	    "private mesh realization updates geometry kind");
    CHECK(bsg_node_is_a(bsg_line_set_ref_as_node(lines), bsg_mesh_type()),
	    "private mesh realization updates runtime type");

    bsg_separator_ref_append_child(root, bsg_line_set_ref_as_node(lines));
    req = bsg_render_request_create(v, NULL);
    batch = bsg_render_batch_create();
    CHECK(req && batch, "create mesh render request");
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);
    CHECK(bsg_render_request_collect(req, batch) == 1, "collect mesh render item");
    CHECK(bsg_render_batch_count(batch) == 1, "mesh batch count");
    item = bsg_render_batch_get(batch, 0);
    CHECK(item && item->geometry.kind == BSG_RENDER_GEOMETRY_MESH,
	    "render kind from private mesh realization");

    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);
    free_view(v);
    return 0;
}

static int
test_point_and_indexed_geometry_fields(void)
{
    struct bsg_view *v = make_view();
    bsg_point_set_ref points = bsg_point_set_ref_create(v, "points");
    bsg_indexed_line_set_ref ilines = bsg_indexed_line_set_ref_create(v, "indexed-lines");
    bsg_indexed_face_set_ref faces = bsg_indexed_face_set_ref_create(v, "faces");
    bsg_line_set_ref generic_faces = bsg_line_set_ref_create(v, "generic-faces");
    point_t pts[3];
    vect_t normals[3];
    vect_t separator_normals[4];
    int indices[5] = {0, 1, -1, 1, 2};
    int bad_line_separator_indices[3] = {0, -2, 1};
    int bad_line_short_segment_indices[4] = {0, -1, 1, 2};
    int out_of_range_line_indices[3] = {0, 3, -1};
    int face_indices[4] = {0, 1, 2, -1};
    int duplicate_face_indices[4] = {0, 1, 1, -1};
    int out_of_range_face_indices[4] = {0, 1, 3, -1};
    int line_cmds[3] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW,
	BSG_GEOMETRY_LINE_DRAW};
    int got = -2;
    point_t got_normal;
    point_t bmin, bmax;
    point_t expect_min, expect_max;
    struct bsg_node *iline_node =
	bsg_object_ref_node(bsg_node_ref_object(bsg_indexed_line_set_ref_as_node(ilines)));
    struct bsg_node *face_node =
	bsg_object_ref_node(bsg_node_ref_object(bsg_indexed_face_set_ref_as_node(faces)));

    VSET(pts[0], 0.0, 0.0, 0.0);
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 1.0, 1.0, 0.0);
    VSET(normals[0], 0.0, 0.0, 1.0);
    VSET(normals[1], 0.0, 0.0, 1.0);
    VSET(normals[2], 0.0, 0.0, 1.0);
    for (size_t i = 0; i < 3; i++)
	VMOVE(separator_normals[i], normals[i]);
    VSET(separator_normals[3], 0.0, 0.0, 0.0);
    VSET(expect_min, 0.0, 0.0, 0.0);
    VSET(expect_max, 1.0, 1.0, 0.0);

    CHECK(bsg_point_set_ref_set_points(points, (const point_t *)pts, 3), "set point-set points");
    CHECK(bsg_node_get_payload(bsg_object_ref_node(bsg_node_ref_object(bsg_point_set_ref_as_node(points)))) == NULL,
	    "point-set setter stores field geometry without payload mirror");
    CHECK(bsg_field_multi_count(bsg_geometry_ref_coordinates_field(points.geometry)) == 3,
	    "point-set coordinates field count");
    CHECK(bsg_field_multi_int_at(bsg_geometry_ref_primitive_sets_field(points.geometry), 0, &got) &&
	    got == BSG_GEOMETRY_POINT_DRAW, "point-set primitive command");
    CHECK(bsg_node_ref_bounds(bsg_point_set_ref_as_node(points), bmin, bmax) &&
	    VNEAR_EQUAL(bmin, expect_min, SMALL_FASTF) &&
	    VNEAR_EQUAL(bmax, expect_max, SMALL_FASTF), "point-set bounds from coordinate field");

    bsg_node_set_payload(iline_node,
	    bsg_payload_line_set_create((point_t *)pts, line_cmds, 3));
    CHECK(bsg_node_get_payload(iline_node) != NULL,
	    "indexed-line stale payload setup");
    CHECK(bsg_indexed_line_set_ref_set_geometry(ilines, (const point_t *)pts, 3, indices, 5),
	    "set indexed-line geometry");
    CHECK(bsg_node_get_payload(iline_node) == NULL,
	    "indexed-line setter clears stale private payload");
    CHECK(bsg_field_multi_count(bsg_geometry_ref_indices_field(ilines.geometry)) == 5,
	    "indexed-line index field count");
    CHECK(bsg_field_multi_int_at(bsg_geometry_ref_indices_field(ilines.geometry), 2, &got) && got == -1,
	    "indexed-line separator index");
    CHECK(bsg_node_ref_bounds(bsg_indexed_line_set_ref_as_node(ilines), bmin, bmax) &&
	    VNEAR_EQUAL(bmin, expect_min, SMALL_FASTF) &&
	    VNEAR_EQUAL(bmax, expect_max, SMALL_FASTF), "indexed-line bounds from coordinate field");
    CHECK(!bsg_indexed_line_set_ref_set_geometry(ilines, (const point_t *)pts, 3,
		bad_line_separator_indices, 3),
	    "indexed-line rejects non-separator negative index");
    CHECK(!bsg_indexed_line_set_ref_set_geometry(ilines, (const point_t *)pts, 3,
		bad_line_short_segment_indices, 4),
	    "indexed-line rejects one-vertex segment");
    CHECK(!bsg_indexed_line_set_ref_set_geometry(ilines, (const point_t *)pts, 3,
		out_of_range_line_indices, 3),
	    "indexed-line rejects out-of-range vertex index");
    CHECK(bsg_field_multi_count(bsg_geometry_ref_indices_field(ilines.geometry)) == 5,
	    "indexed-line rejected update preserves previous index count");
    CHECK(bsg_field_multi_int_at(bsg_geometry_ref_indices_field(ilines.geometry), 2, &got) && got == -1,
	    "indexed-line rejected update preserves previous separator index");

    bsg_node_set_payload(face_node,
	    bsg_payload_line_set_create((point_t *)pts, line_cmds, 3));
    CHECK(bsg_node_get_payload(face_node) != NULL,
	    "indexed-face stale payload setup");
    CHECK(bsg_indexed_face_set_ref_set_geometry(faces, (const point_t *)pts, 3, face_indices, 4),
	    "set indexed-face geometry");
    CHECK(bsg_node_get_payload(face_node) == NULL,
	    "indexed-face setter clears stale private payload");
    CHECK(bsg_geometry_ref_kind(faces.geometry) == BSG_GEOMETRY_NODE_INDEXED_FACE_SET,
	    "indexed-face geometry kind");
    CHECK(bsg_node_ref_bounds(bsg_indexed_face_set_ref_as_node(faces), bmin, bmax) &&
	    VNEAR_EQUAL(bmin, expect_min, SMALL_FASTF) &&
	    VNEAR_EQUAL(bmax, expect_max, SMALL_FASTF), "indexed-face bounds from coordinate field");

    CHECK(bsg_geometry_ref_set_indexed_face_set(faces.geometry,
		(const point_t *)pts, 3, (const vect_t *)normals, 3, face_indices, 4),
	    "set indexed-face normals before no-normal replacement");
    CHECK(bsg_field_multi_count(bsg_geometry_ref_normals_field(faces.geometry)) == 3,
	    "indexed-face normal setup count");
    bsg_node_set_payload(face_node,
	    bsg_payload_line_set_create((point_t *)pts, line_cmds, 3));
    CHECK(bsg_indexed_face_set_ref_set_geometry(faces, (const point_t *)pts, 3,
		face_indices, 4),
	    "replace indexed-face geometry without normals");
    CHECK(bsg_node_get_payload(face_node) == NULL,
	    "indexed-face no-normal replacement clears stale private payload");
    CHECK(bsg_field_multi_count(bsg_geometry_ref_normals_field(faces.geometry)) == 0,
	    "indexed-face no-normal replacement clears stale normals");

    CHECK(bsg_geometry_ref_set_indexed_face_set(generic_faces.geometry,
		(const point_t *)pts, 3, (const vect_t *)normals, 3, face_indices, 4),
	    "set indexed-face geometry through generic geometry ref");
    CHECK(bsg_geometry_ref_kind(generic_faces.geometry) == BSG_GEOMETRY_NODE_INDEXED_FACE_SET,
	    "generic geometry ref updates kind");
    CHECK(bsg_node_is_a(bsg_line_set_ref_as_node(generic_faces), bsg_indexed_face_set_type()),
	    "generic geometry ref updates runtime type");
    CHECK(bsg_field_multi_count(bsg_geometry_ref_normals_field(generic_faces.geometry)) == 3,
	    "generic indexed-face normals field count");
    CHECK(bsg_field_multi_point_at(bsg_geometry_ref_normals_field(generic_faces.geometry), 1, got_normal) &&
	    VNEAR_EQUAL(got_normal, normals[1], SMALL_FASTF),
	    "generic indexed-face normal stored in field");
    CHECK(!bsg_geometry_ref_set_indexed_face_set(generic_faces.geometry,
		(const point_t *)pts, 3, (const vect_t *)normals, 2,
		face_indices, 4),
	    "indexed-face rejects unsupported normal count");
    CHECK(!bsg_geometry_ref_set_indexed_face_set(generic_faces.geometry,
		(const point_t *)pts, 3, (const vect_t *)separator_normals, 4,
		face_indices, 4),
	    "indexed-face rejects separator-slot normals");
    CHECK(!bsg_geometry_ref_set_indexed_face_set(generic_faces.geometry,
		(const point_t *)pts, 3, (const vect_t *)normals, 3,
		duplicate_face_indices, 4),
	    "indexed-face rejects duplicate vertex indices in one face");
    CHECK(!bsg_geometry_ref_set_indexed_face_set(generic_faces.geometry,
		(const point_t *)pts, 3, (const vect_t *)normals, 3,
		out_of_range_face_indices, 4),
	    "indexed-face rejects out-of-range vertex indices");
    CHECK(!bsg_indexed_face_set_ref_set_geometry(faces, (const point_t *)pts, 3, indices, 5),
	    "indexed-face rejects degenerate face stream");
    CHECK(bsg_field_multi_int_at(bsg_geometry_ref_indices_field(generic_faces.geometry), 3, &got) && got == -1,
	    "generic indexed-face separator index");
    CHECK(bsg_node_ref_bounds(bsg_line_set_ref_as_node(generic_faces), bmin, bmax) &&
	    VNEAR_EQUAL(bmin, expect_min, SMALL_FASTF) &&
	    VNEAR_EQUAL(bmax, expect_max, SMALL_FASTF), "generic indexed-face bounds from coordinate field");

    bsg_node_ref_destroy(bsg_point_set_ref_as_node(points));
    bsg_node_ref_destroy(bsg_indexed_line_set_ref_as_node(ilines));
    bsg_node_ref_destroy(bsg_indexed_face_set_ref_as_node(faces));
    bsg_node_ref_destroy(bsg_line_set_ref_as_node(generic_faces));
    free_view(v);
    return 0;
}

static int
test_text_and_image_geometry_fields(void)
{
    struct bsg_view *v = make_view();
    bsg_text_ref text = bsg_text_ref_create(v, "label");
    bsg_image_ref image = bsg_image_ref_create(v, "image");
    point_t pos;
    const char *stored = NULL;
    uint64_t width = 0;
    uint64_t height = 0;
    uint64_t channels = 0;

    VSET(pos, 7.0, 8.0, 9.0);
    CHECK(bsg_text_ref_set_text(text, "abc", pos, 12), "set text geometry");
    CHECK(bsg_field_get_string(bsg_geometry_ref_text_field(text.geometry), &stored) &&
	    stored && stored[0] == 'a', "text field stores string");
    CHECK(bsg_field_multi_count(bsg_geometry_ref_coordinates_field(text.geometry)) == 1,
	    "text field stores anchor point");

    CHECK(bsg_image_ref_set_metadata(image, 64, 32, 4), "set image metadata");
    CHECK(bsg_field_get_uint64(bsg_geometry_ref_image_width_field(image.geometry), &width) &&
	    bsg_field_get_uint64(bsg_geometry_ref_image_height_field(image.geometry), &height) &&
	    bsg_field_get_uint64(bsg_geometry_ref_image_channels_field(image.geometry), &channels),
	    "image metadata fields readable");
    CHECK(width == 64 && height == 32 && channels == 4, "image metadata values");

    bsg_node_ref_destroy(bsg_text_ref_as_node(text));
    bsg_node_ref_destroy(bsg_image_ref_as_node(image));
    free_view(v);
    return 0;
}

static int
test_annotation_geometry_fields_and_render(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_annotation_ref annotation = bsg_annotation_ref_create(v, "annotation");
    bsg_node *node = bsg_object_ref_node(bsg_node_ref_object(bsg_annotation_ref_as_node(annotation)));
    point_t anchor;
    point_t pts[2];
    mat_t model_mat, display_mat;
    struct bsg_annotation_segment seg;
    struct bsg_render_request *req = NULL;
    struct bsg_render_batch *batch = NULL;
    const struct bsg_render_item *item = NULL;
    const char *stored = NULL;

    VSET(anchor, 1.0, 2.0, 3.0);
    VSET(pts[0], 1.0, 2.0, 3.0);
    VSET(pts[1], 1.5, 2.5, 3.0);
    MAT_IDN(model_mat);
    MAT_IDN(display_mat);
    memset(&seg, 0, sizeof(seg));
    seg.kind = BSG_ANNOTATION_SEGMENT_TEXT;
    seg.data.text.ref_pt = 1;
    seg.data.text.relative_position = 1;
    seg.data.text.text = (char *)"typed annotation";
    seg.data.text.size = 11.0;

    CHECK(!bsg_node_ref_is_null(bsg_annotation_ref_as_node(annotation)), "create annotation node");
    CHECK(bsg_node_is_a(bsg_annotation_ref_as_node(annotation), bsg_annotation_type()),
	    "annotation runtime type");
    CHECK(bsg_annotation_ref_set_record(annotation, "typed annotation",
		BSG_ANNOTATION_SPACE_DISPLAY, anchor, model_mat, display_mat,
		(const point_t *)pts, 2, &seg, 1), "set annotation record");
    CHECK(bsg_geometry_ref_kind(annotation.geometry) == BSG_GEOMETRY_NODE_ANNOTATION,
	    "annotation geometry kind");
    CHECK(bsg_node_has_geometry_role(node, BSG_GEOMETRY_TEXT_LABELS),
	    "annotation publishes text-label role");
    CHECK(bsg_field_get_string(bsg_geometry_ref_text_field(annotation.geometry), &stored) &&
	    stored && BU_STR_EQUAL(stored, "typed annotation"),
	    "annotation summary stored in text field");
    CHECK(bsg_field_multi_count(bsg_geometry_ref_coordinates_field(annotation.geometry)) == 2,
	    "annotation control points stored in coordinate field");

    bsg_separator_ref_append_child(root, bsg_annotation_ref_as_node(annotation));
    req = bsg_render_request_create(v, NULL);
    batch = bsg_render_batch_create();
    CHECK(req && batch, "create annotation render request");
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);
    CHECK(bsg_render_request_collect(req, batch) == 1, "collect annotation render item");
    CHECK(bsg_render_batch_count(batch) == 1, "annotation batch count");
    item = bsg_render_batch_get(batch, 0);
    CHECK(item && item->geometry.kind == BSG_RENDER_GEOMETRY_ANNOTATION,
	    "render kind from annotation node");
    CHECK(item->geometry.annotation.space == BSG_ANNOTATION_SPACE_DISPLAY,
	    "annotation render space");
    CHECK(VNEAR_EQUAL(item->geometry.annotation.anchor, anchor, SMALL_FASTF),
	    "annotation render anchor");
    CHECK(item->geometry.annotation.point_count == 2 &&
	    item->geometry.annotation.points &&
	    VNEAR_EQUAL(item->geometry.annotation.points[1], pts[1], SMALL_FASTF),
	    "annotation render points snapshot");
    CHECK(item->geometry.annotation.segment_count == 1 &&
	    item->geometry.annotation.segments &&
	    item->geometry.annotation.segments[0].kind == BSG_ANNOTATION_SEGMENT_TEXT &&
	    BU_STR_EQUAL(item->geometry.annotation.segments[0].data.text.text, "typed annotation") &&
	    item->geometry.annotation.segments[0].data.text.ref_pt == 1,
	    "annotation render segment snapshot");

    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);
    bsg_node_ref_destroy(bsg_annotation_ref_as_node(annotation));
    free_view(v);
    return 0;
}

int
main(int UNUSED(argc), const char **argv)
{
    int failures = 0;
    bu_setprogname(argv[0]);
    failures += test_line_set_fields_and_render();
    failures += test_geometry_ref_conversions();
    failures += test_private_realizations_update_geometry_kind();
    failures += test_point_and_indexed_geometry_fields();
    failures += test_text_and_image_geometry_fields();
    failures += test_annotation_geometry_fields_and_render();
    if (failures == 0)
	printf("geometry node tests passed\n");
    return failures ? 1 : 0;
}
