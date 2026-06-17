/*                     C A M E R A . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libbsg/camera.c
 *
 * Camera snapshot — capture and restore bsg_view camera state.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "vmath.h"

#include "bsg/defines.h"
#include "bsg/camera.h"
#include "field_private.h"
#include "object_private.h"

static bsg_node *
_camera_node(bsg_camera_ref camera)
{
    if (!bsg_type_is_a(bsg_object_type(camera.node.object), bsg_camera_type()))
	return NULL;
    return bsg_object_ref_node(camera.node.object);
}


struct bsg_camera *
bsg_camera_create(void)
{
    struct bsg_camera *cam;
    BU_ALLOC(cam, struct bsg_camera);
    memset(cam, 0, sizeof(struct bsg_camera));
    cam->scale       = 1.0;
    cam->perspective = 0.0;
    VSETALL(cam->aet, 0.0);
    VSETALL(cam->eye_pos, 0.0);
    MAT_IDN(cam->rotation);
    MAT_IDN(cam->center);
    MAT_IDN(cam->model2view);
    MAT_IDN(cam->view2model);
    return cam;
}


void
bsg_camera_destroy(struct bsg_camera *cam)
{
    if (!cam)
	return;
    bu_free(cam, "bsg_camera");
}


struct bsg_camera *
bsg_camera_snapshot(const struct bsg_view *v)
{
    if (!v)
	return NULL;

    struct bsg_camera *cam = bsg_camera_create();
    if (!cam)
	return NULL;

    cam->scale       = v->gv_scale;
    cam->perspective = v->gv_perspective;
    VMOVE(cam->aet,     v->gv_aet);
    VMOVE(cam->eye_pos, v->gv_eye_pos);
    MAT_COPY(cam->rotation,   v->gv_rotation);
    MAT_COPY(cam->center,     v->gv_center);
    MAT_COPY(cam->model2view, v->gv_model2view);
    MAT_COPY(cam->view2model, v->gv_view2model);

    return cam;
}


void
bsg_camera_apply(struct bsg_camera *cam, struct bsg_view *v)
{
    if (!cam || !v)
	return;

    v->gv_scale       = cam->scale;
    v->gv_perspective = cam->perspective;
    VMOVE(v->gv_aet,     cam->aet);
    VMOVE(v->gv_eye_pos, cam->eye_pos);
    MAT_COPY(v->gv_rotation,   cam->rotation);
    MAT_COPY(v->gv_center,     cam->center);
    MAT_COPY(v->gv_model2view, cam->model2view);
    MAT_COPY(v->gv_view2model, cam->view2model);
}


/* -----------------------------------------------------------------------
 * Camera-node binding.
 * ----------------------------------------------------------------------- */

void
bsg_view_set_camera_node(struct bsg_view *v, struct bsg_camera *cam)
{
    if (!v)
	return;
    v->gv_camera_node = cam;
}


struct bsg_camera *
bsg_view_get_camera_node(const struct bsg_view *v)
{
    if (!v)
	return NULL;
    return v->gv_camera_node;
}


void
bsg_view_apply_camera_node(struct bsg_view *v)
{
    if (!v || !v->gv_camera_node)
	return;
    bsg_camera_apply(v->gv_camera_node, v);
}

bsg_field_ref
bsg_camera_ref_projection_field(bsg_camera_ref camera)
{
    return bsg_node_field_ref(_camera_node(camera), BSG_FIELD_CAMERA_PROJECTION);
}

bsg_field_ref
bsg_camera_ref_perspective_field(bsg_camera_ref camera)
{
    return bsg_node_field_ref(_camera_node(camera), BSG_FIELD_CAMERA_PERSPECTIVE);
}

bsg_field_ref
bsg_camera_ref_position_field(bsg_camera_ref camera)
{
    return bsg_node_field_ref(_camera_node(camera), BSG_FIELD_CAMERA_POSITION);
}

bsg_field_ref
bsg_camera_ref_orientation_field(bsg_camera_ref camera)
{
    return bsg_node_field_ref(_camera_node(camera), BSG_FIELD_CAMERA_ORIENTATION);
}

bsg_field_ref
bsg_camera_ref_near_distance_field(bsg_camera_ref camera)
{
    return bsg_node_field_ref(_camera_node(camera), BSG_FIELD_CAMERA_NEAR_DISTANCE);
}

bsg_field_ref
bsg_camera_ref_far_distance_field(bsg_camera_ref camera)
{
    return bsg_node_field_ref(_camera_node(camera), BSG_FIELD_CAMERA_FAR_DISTANCE);
}

int
bsg_camera_ref_set_projection(bsg_camera_ref camera, int projection)
{
    return bsg_field_set_enum(bsg_camera_ref_projection_field(camera), projection);
}

int
bsg_camera_ref_projection(bsg_camera_ref camera)
{
    int value = BSG_CAMERA_ORTHOGRAPHIC;
    (void)bsg_field_get_enum(bsg_camera_ref_projection_field(camera), &value);
    return value;
}

int
bsg_camera_ref_set_perspective(bsg_camera_ref camera, double perspective)
{
    return bsg_field_set_double(bsg_camera_ref_perspective_field(camera), perspective);
}

double
bsg_camera_ref_perspective(bsg_camera_ref camera)
{
    double value = 0.0;
    (void)bsg_field_get_double(bsg_camera_ref_perspective_field(camera), &value);
    return value;
}

int
bsg_camera_ref_set_position(bsg_camera_ref camera, const point_t position)
{
    return bsg_field_set_vec3(bsg_camera_ref_position_field(camera), position);
}

int
bsg_camera_ref_position(bsg_camera_ref camera, point_t position)
{
    return bsg_field_get_vec3(bsg_camera_ref_position_field(camera), position);
}

int
bsg_camera_ref_set_orientation(bsg_camera_ref camera, const mat_t orientation)
{
    return bsg_field_set_matrix(bsg_camera_ref_orientation_field(camera), orientation);
}

int
bsg_camera_ref_orientation(bsg_camera_ref camera, mat_t orientation)
{
    return bsg_field_get_matrix(bsg_camera_ref_orientation_field(camera), orientation);
}

int
bsg_camera_ref_set_near_distance(bsg_camera_ref camera, double distance)
{
    return bsg_field_set_double(bsg_camera_ref_near_distance_field(camera), distance);
}

double
bsg_camera_ref_near_distance(bsg_camera_ref camera)
{
    double value = 0.0;
    (void)bsg_field_get_double(bsg_camera_ref_near_distance_field(camera), &value);
    return value;
}

int
bsg_camera_ref_set_far_distance(bsg_camera_ref camera, double distance)
{
    return bsg_field_set_double(bsg_camera_ref_far_distance_field(camera), distance);
}

double
bsg_camera_ref_far_distance(bsg_camera_ref camera)
{
    double value = 0.0;
    (void)bsg_field_get_double(bsg_camera_ref_far_distance_field(camera), &value);
    return value;
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
