/*                        C A M E R A . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bio.h"

#include "bu/parallel.h"
#include "bu/dylib.h"
#include "bu/log.h"
#include "bu/str.h"
#include "rt/primitives/annot.h"

#include "./camera.h"


struct render_shader_s {
    const char *name;
    int (*init)(render_t *, const char *);
    void *dlh;	/* dynamic library handle */
    struct render_shader_s *next;
};


static struct render_shader_s *shaders = NULL;

struct render_camera_annotation_data {
    const struct rt_annot_scene *scene;
    fastf_t model_units_per_tie_unit;
    struct rt_annot_view view;
};


struct render_camera_parallel_data {
    render_camera_thread_data_t thread;
    struct render_camera_annotation_data annotation;
};

static const fastf_t render_camera_default_scene_radius = 1.0;


void render_camera_render_thread(int cpu, void *ptr);	/* for bu_parallel */
static void render_camera_prep_ortho(render_camera_t *camera);
static void render_camera_prep_persp(render_camera_t *camera);
static void render_camera_prep_persp_dof(render_camera_t *camera);
static void render_camera_annotation_view(const render_camera_t *camera,
	fastf_t model_units_per_tie_unit, struct rt_annot_view *view);

static struct render_shader_s *render_shader_register (const char *name, int (*init)(render_t *, const char *));

void
render_camera_init(render_camera_t *camera, size_t threads)
{
    camera->type = RENDER_CAMERA_PERSPECTIVE;

    camera->view_num = 1;
    camera->view_list = (render_camera_view_t *) bu_malloc (sizeof(render_camera_view_t), "render_camera_init");
    camera->dof = 0;
    camera->tilt = 0;

    /* The camera will use a thread for every cpu the machine has. */
    camera->thread_num = threads ? (uint8_t)threads : (uint8_t)bu_avail_cpus();

    /* Initialize camera to rendering surface normals */
    render_normal_init(&camera->render, NULL);
    camera->rm = RENDER_METHOD_PHONG;

    if (shaders == NULL) {
	render_shader_register("component", render_component_init);
	render_shader_register("cut", render_cut_init);
	render_shader_register("depth", render_depth_init);
	render_shader_register("flat", render_flat_init);
	render_shader_register("flos", render_flos_init);
	render_shader_register("grid", render_grid_init);
	render_shader_register("normal", render_normal_init);
	render_shader_register("path", render_path_init);
	render_shader_register("phong", render_phong_init);
	render_shader_register("spall", render_spall_init);
	render_shader_register("surfel", render_surfel_init);
    }
}


void
render_camera_free(render_camera_t *UNUSED(camera))
{
    return;
}


static void
render_camera_prep_ortho(render_camera_t *camera)
{
    vect_t look, up, side, temp;
    TFLOAT angle, s, c;

    /* Generate standard up vector */
    up[0] = 0;
    up[1] = 0;
    up[2] = 1;

    /* Generate unitized lookector */
    VSUB2(look, camera->focus, camera->pos);
    VUNITIZE(look);

    /* Make unitized up vector perpendicular to lookector */
    VMOVE(temp, look);
    angle = VDOT(up, temp);
    VSCALE(temp, temp, angle);
    VSUB2(up, up, temp);
    VUNITIZE(up);

    /* Generate a temporary side vector */
    VCROSS(side, up, look);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    s = sin(-camera->tilt * DEG2RAD);
    c = cos(-camera->tilt * DEG2RAD);
    VSCALE(up, up, c);
    VSCALE(side, side, s);
    VADD2(up, up, side);

    /* Create final side vector */
    VCROSS(side, up, look);

    /* look direction */
    VMOVE(camera->view_list[0].top_l, look);

    /* gridsize is millimeters along the horizontal axis to display */
    /* left (side) */
    VSCALE(temp, side, (camera->aspect * camera->gridsize * 0.5));
    VADD2(camera->view_list[0].pos, camera->pos, temp);
    /* and (up) */
    VSCALE(temp, up, (camera->gridsize * 0.5));
    VADD2(camera->view_list[0].pos, camera->view_list[0].pos, temp);

    /* compute step vectors for camera position */

    /* X */
    VSCALE(camera->view_list[0].step_x, side, (-camera->gridsize * camera->aspect / (TFLOAT)camera->w));

    /* Y */
    VSCALE(camera->view_list[0].step_y, up, (-camera->gridsize / (TFLOAT)camera->h));
}


static void
render_camera_prep_persp(render_camera_t *camera)
{
    vect_t look, up, side, temp, topl, topr, botl;
    TFLOAT angle, s, c;


    /* Generate unitized lookector */
    VSUB2(look, camera->focus, camera->pos);
    VUNITIZE(look);

    /* Generate standard up vector */
    up[0] = 0;
    up[1] = 0;
    up[2] = 1;

    /* Make unitized up vector perpendicular to lookector */
    VMOVE(temp, look);
    angle = VDOT(up, temp);
    VSCALE(temp, temp, angle);
    VSUB2(up, up, temp);
    VUNITIZE(up);

    /* Generate a temporary side vector */
    VCROSS(side, up, look);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    s = sin(-camera->tilt * DEG2RAD);
    c = cos(-camera->tilt * DEG2RAD);
    VSCALE(up, up, c);
    VSCALE(side, side, s);
    VADD2(up, up, side);

    /* Create final side vector */
    VCROSS(side, up, look);

    /* Compute sine and cosine terms for field of view */
    s = sin(camera->fov*DEG2RAD);
    c = cos(camera->fov*DEG2RAD);

    /* Up, Look, and Side vectors are complete, generate Top Left reference vector */
    topl[0] = s*up[0] + camera->aspect*s*side[0] + c*look[0];
    topl[1] = s*up[1] + camera->aspect*s*side[1] + c*look[1];
    topl[2] = s*up[2] + camera->aspect*s*side[2] + c*look[2];

    topr[0] = s*up[0] - camera->aspect*s*side[0] + c*look[0];
    topr[1] = s*up[1] - camera->aspect*s*side[1] + c*look[1];
    topr[2] = s*up[2] - camera->aspect*s*side[2] + c*look[2];

    botl[0] = -s*up[0] + camera->aspect*s*side[0] + c*look[0];
    botl[1] = -s*up[1] + camera->aspect*s*side[1] + c*look[1];
    botl[2] = -s*up[2] + camera->aspect*s*side[2] + c*look[2];

    VUNITIZE(topl);
    VUNITIZE(botl);
    VUNITIZE(topr);

    /* Store Camera Position */
    VMOVE(camera->view_list[0].pos, camera->pos);

    /* Store the top left vector */
    VMOVE(camera->view_list[0].top_l, topl);

    /* Generate stepx and stepy vectors for sampling each pixel */
    VSUB2(camera->view_list[0].step_x, topr, topl);
    VSUB2(camera->view_list[0].step_y, botl, topl);

    /* Divide stepx and stepy by the number of pixels */
    VSCALE(camera->view_list[0].step_x, camera->view_list[0].step_x, 1.0 / camera->w);
    VSCALE(camera->view_list[0].step_y, camera->view_list[0].step_y, 1.0 / camera->h);
    return;
}


static void
render_camera_prep_persp_dof(render_camera_t *camera)
{
    vect_t look, up, side, dof_look, dof_up, dof_side, dof_topl, dof_topr, dof_botl, temp, step_x, step_y, topl, topr, botl;
    TFLOAT angle, mag, sfov, cfov, sdof, cdof;
    uint32_t i, n;

    /* Generate unitized lookector */
    VSUB2(dof_look, camera->focus, camera->pos);
    VUNITIZE(dof_look);

    /* Generate standard up vector */
    dof_up[0] = 0;
    dof_up[1] = 0;
    dof_up[2] = 1;

    /* Make unitized up vector perpendicular to lookector */
    VMOVE(temp, dof_look);
    angle = VDOT(dof_up, temp);
    VSCALE(temp, temp, angle);
    VSUB2(dof_up, dof_up, temp);
    VUNITIZE(dof_up);

    /* Generate a temporary side vector */
    VCROSS(dof_side, dof_up, dof_look);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    sdof = sin(-camera->tilt * DEG2RAD);
    cdof = cos(-camera->tilt * DEG2RAD);
    VSCALE(dof_up, dof_up, cdof);
    VSCALE(dof_side, dof_side, sdof);
    VADD2(dof_up, dof_up, dof_side);

    /* Create final side vector */
    VCROSS(dof_side, dof_up, dof_look);

    /*
     * Generate a camera position, top left vector, and step vectors for each DOF sample
     */

    /* Obtain magnitude of reverse lookector */
    VSUB2(dof_look, camera->pos, camera->focus);
    mag = MAGNITUDE(dof_look);
    VUNITIZE(dof_look);

    /* Compute sine and cosine terms for field of view */
    sdof = sin(camera->dof*DEG2RAD);
    cdof = cos(camera->dof*DEG2RAD);


    /* Up, Look, and Side vectors are complete, generate Top Left reference vector */
    dof_topl[0] = sdof*dof_up[0] + sdof*dof_side[0] + cdof*dof_look[0];
    dof_topl[1] = sdof*dof_up[1] + sdof*dof_side[1] + cdof*dof_look[1];
    dof_topl[2] = sdof*dof_up[2] + sdof*dof_side[2] + cdof*dof_look[2];

    dof_topr[0] = sdof*dof_up[0] - sdof*dof_side[0] + cdof*dof_look[0];
    dof_topr[1] = sdof*dof_up[1] - sdof*dof_side[1] + cdof*dof_look[1];
    dof_topr[2] = sdof*dof_up[2] - sdof*dof_side[2] + cdof*dof_look[2];

    dof_botl[0] = -sdof*dof_up[0] + sdof*dof_side[0] + cdof*dof_look[0];
    dof_botl[1] = -sdof*dof_up[1] + sdof*dof_side[1] + cdof*dof_look[1];
    dof_botl[2] = -sdof*dof_up[2] + sdof*dof_side[2] + cdof*dof_look[2];

    VUNITIZE(dof_topl);
    VUNITIZE(dof_botl);
    VUNITIZE(dof_topr);

    VSUB2(step_x, dof_topr, dof_topl);
    VSUB2(step_y, dof_botl, dof_topl);

    for (i = 0; i < RENDER_CAMERA_DOF_SAMPLES; i++) {
	for (n = 0; n < RENDER_CAMERA_DOF_SAMPLES; n++) {
	    /* Generate virtual camera position for this depth of field sample */
	    VSCALE(temp, step_x, ((TFLOAT)i/(TFLOAT)(RENDER_CAMERA_DOF_SAMPLES-1)));
	    VADD2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos, dof_topl, temp);
	    VSCALE(temp, step_y, ((TFLOAT)n/(TFLOAT)(RENDER_CAMERA_DOF_SAMPLES-1)));
	    VADD2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos, camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos, temp);
	    VUNITIZE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos);
	    VSCALE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos, camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos, mag);
	    VADD2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos, camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos, camera->focus);

	    /* Generate unitized lookector */
	    VSUB2(look, camera->focus, camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos);
	    VUNITIZE(look);

	    /* Generate standard up vector */
	    up[0] = 0;
	    up[1] = 0;
	    up[2] = 1;

	    /* Make unitized up vector perpendicular to lookector */
	    VMOVE(temp, look);
	    angle = VDOT(up, temp);
	    VSCALE(temp, temp, angle);
	    VSUB2(up, up, temp);
	    VUNITIZE(up);

	    /* Generate a temporary side vector */
	    VCROSS(side, up, look);

	    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
	    sfov = sin(-camera->tilt * DEG2RAD);
	    cfov = cos(-camera->tilt * DEG2RAD);
	    VSCALE(up, up, cfov);
	    VSCALE(side, side, sfov);
	    VADD2(up, up, side);

	    /* Create final side vector */
	    VCROSS(side, up, look);

	    /* Compute sine and cosine terms for field of view */
	    sfov = sin(camera->fov*DEG2RAD);
	    cfov = cos(camera->fov*DEG2RAD);


	    /* Up, Look, and Side vectors are complete, generate Top Left reference vector */
	    topl[0] = sfov*up[0] + camera->aspect*sfov*side[0] + cfov*look[0];
	    topl[1] = sfov*up[1] + camera->aspect*sfov*side[1] + cfov*look[1];
	    topl[2] = sfov*up[2] + camera->aspect*sfov*side[2] + cfov*look[2];

	    topr[0] = sfov*up[0] - camera->aspect*sfov*side[0] + cfov*look[0];
	    topr[1] = sfov*up[1] - camera->aspect*sfov*side[1] + cfov*look[1];
	    topr[2] = sfov*up[2] - camera->aspect*sfov*side[2] + cfov*look[2];

	    botl[0] = -sfov*up[0] + camera->aspect*sfov*side[0] + cfov*look[0];
	    botl[1] = -sfov*up[1] + camera->aspect*sfov*side[1] + cfov*look[1];
	    botl[2] = -sfov*up[2] + camera->aspect*sfov*side[2] + cfov*look[2];

	    VUNITIZE(topl);
	    VUNITIZE(botl);
	    VUNITIZE(topr);

	    /* Store the top left vector */
	    VMOVE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].top_l, topl);

	    /* Generate stepx and stepy vectors for sampling each pixel */
	    VSUB2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_x, topr, topl);
	    VSUB2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_y, botl, topl);

	    /* Divide stepx and stepy by the number of pixels */
	    VSCALE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_x, camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_x, 1.0 / camera->w);
	    VSCALE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_y, camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_y, 1.0 / camera->h);
	}
    }
}


void
render_camera_prep(render_camera_t *camera)
{
    /* Generate an aspect ratio coefficient */
    camera->aspect = (TFLOAT)camera->w / (TFLOAT)camera->h;

    if (camera->type == RENDER_CAMERA_ORTHOGRAPHIC)
	render_camera_prep_ortho(camera);

    if (camera->type == RENDER_CAMERA_PERSPECTIVE) {
	if (camera->dof <= 0.0) {
	    render_camera_prep_persp(camera);
	} else {
	    /* Generate camera positions for depth of field - Handle this better */
	    camera->view_num = RENDER_CAMERA_DOF_SAMPLES*RENDER_CAMERA_DOF_SAMPLES;
	    camera->view_list = (render_camera_view_t *)bu_malloc(sizeof(render_camera_view_t) * camera->view_num, "camera view");

	    render_camera_prep_persp_dof(camera);
	}
    }
}


fastf_t
render_camera_fit_scene(render_camera_t *camera, const struct tie_s *tie,
	const struct rt_annot_scene *annotations,
	fastf_t model_units_per_tie_unit)
{
    point_t annotation_min, annotation_max;
    point_t scene_min, scene_max;
    vect_t offset;
    fastf_t scene_radius;

    if (!camera || !tie)
	return 0.0;

    if (!annotations || !rt_annot_scene_bounds(annotations,
	    annotation_min, annotation_max)) {
	VSETALL(camera->pos, tie->radius);
	VMOVE(camera->focus, tie->mid);
	return tie->radius;
    }

    if (!isfinite(model_units_per_tie_unit) ||
	    model_units_per_tie_unit <= 0.0)
	model_units_per_tie_unit = 1.0;
    VSCALE(annotation_min, annotation_min,
	1.0 / model_units_per_tie_unit);
    VSCALE(annotation_max, annotation_max,
	1.0 / model_units_per_tie_unit);
    VMOVE(scene_min, annotation_min);
    VMOVE(scene_max, annotation_max);
    if (tie->tri_num) {
	VMINMAX(scene_min, scene_max, tie->amin);
	VMINMAX(scene_min, scene_max, tie->amax);
    }

    VADD2SCALE(camera->focus, scene_min, scene_max, 0.5);
    VSUB2(offset, scene_max, scene_min);
    scene_radius = 0.5 * MAGNITUDE(offset);
    if (!isfinite(scene_radius) || scene_radius <= SMALL_FASTF)
	scene_radius = render_camera_default_scene_radius;

    /* Keep the established isometric initial direction while positioning it
     * relative to the combined scene center. */
    VSETALL(offset, scene_radius);
    VADD2(camera->pos, camera->focus, offset);
    return scene_radius;
}


static void
render_camera_annotation_view(const render_camera_t *camera,
	fastf_t model_units_per_tie_unit, struct rt_annot_view *view)
{
    vect_t look, up, side, temp, right;
    point_t model_position;
    fastf_t angle, sine, cosine, xy_scale = 1.0;
    mat_t *matrix = &view->model2view;

    VSUB2(look, camera->focus, camera->pos);
    VUNITIZE(look);
    VSET(up, 0.0, 0.0, 1.0);
    VMOVE(temp, look);
    angle = VDOT(up, temp);
    VSCALE(temp, temp, angle);
    VSUB2(up, up, temp);
    VUNITIZE(up);
    VCROSS(side, up, look);
    sine = sin(-camera->tilt * DEG2RAD);
    cosine = cos(-camera->tilt * DEG2RAD);
    VSCALE(up, up, cosine);
    VSCALE(side, side, sine);
    VADD2(up, up, side);
    VCROSS(side, up, look);
    VREVERSE(right, side);

    MAT_IDN(*matrix);
    if (camera->type == RENDER_CAMERA_ORTHOGRAPHIC &&
	    camera->aspect > 0.0 && camera->gridsize > 0.0) {
	xy_scale = 2.0 / (camera->aspect * camera->gridsize *
		model_units_per_tie_unit);
    } else if (camera->aspect > 0.0) {
	xy_scale = 1.0 / camera->aspect;
    }
    for (size_t axis = 0; axis < 3; ++axis) {
	(*matrix)[axis] = right[axis] * xy_scale;
	(*matrix)[4 + axis] = up[axis] * xy_scale;
	(*matrix)[8 + axis] = -look[axis];
    }
    VSCALE(model_position, camera->pos, model_units_per_tie_unit);
    (*matrix)[3] = -VDOT(&(*matrix)[0], model_position);
    (*matrix)[7] = -VDOT(&(*matrix)[4], model_position);
    (*matrix)[11] = -VDOT(&(*matrix)[8], model_position);
    view->width = camera->w;
    view->height = camera->h;
    view->perspective =
	camera->type == RENDER_CAMERA_PERSPECTIVE ? 2.0 * camera->fov : 0.0;
}


static void
render_camera_composite_annotation(render_camera_thread_data_t *td,
	const struct render_camera_annotation_data *annotation,
	const struct tie_ray_s *tie_ray, fastf_t sample_x, fastf_t sample_y,
	vect_t *pixel)
{
    struct rt_annot_hit back_layer = {0};
    struct xray ray;
    fastf_t scene_distance = INFINITY;

    if (!annotation || !annotation->scene)
	return;
    VSCALE(ray.r_pt, tie_ray->pos, annotation->model_units_per_tie_unit);
    VMOVE(ray.r_dir, tie_ray->dir);
    if (!rt_annot_scene_query_layers(annotation->scene, &annotation->view,
	    &ray, sample_x, sample_y, INFINITY, &back_layer, 1))
	return;

    if (!back_layer.screen_space) {
	struct tie_id_s id;
	struct tie_ray_s depth_ray = *tie_ray;
	if (TIE_WORK(td->tie, &depth_ray, &id, render_hit, NULL))
	    scene_distance = id.dist * annotation->model_units_per_tie_unit;
    }
    (void)rt_annot_scene_composite(annotation->scene, &annotation->view,
	&ray, sample_x, sample_y, scene_distance, *pixel, NULL);
}


static void
render_camera_render_thread_impl(int UNUSED(cpu), render_camera_thread_data_t *td,
	const struct render_camera_annotation_data *annotation)
{
    int d, n, res_ind, scanline, v_scanline;
    vect_t pixel, accum;
    vect_t v1 = VINIT_ZERO;
    vect_t v2 = VINIT_ZERO;
    struct tie_ray_s ray;
    struct tie_ray_s annotation_ray;
    fastf_t view_inv;

    VSETALL(v1, 0);

    view_inv = 1.0 / td->camera->view_num;

    td->camera->render.tie = td->tie;

    res_ind = 0;

    while (1) {
	/* Determine if this scanline should be computed by this thread */
	bu_semaphore_acquire(td->sem_tie_worker);
	if (*td->scanline == td->tile->size_y) {
	    bu_semaphore_release(td->sem_tie_worker);
	    return;
	} else {
	    scanline = *td->scanline;
	    (*td->scanline)++;
	}
	bu_semaphore_release(td->sem_tie_worker);

	v_scanline = scanline + td->tile->orig_y;
	if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_24) {
	    res_ind = 3*scanline*td->tile->size_x;
	} else if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_128) {
	    res_ind = 4*scanline*td->tile->size_x;
	}


	/* optimization if there is no depth of field being applied */
	if (td->camera->view_num == 1) {
	    VSCALE(v1, td->camera->view_list[0].step_y, v_scanline);
	    VADD2(v1, v1, td->camera->view_list[0].top_l);
	}


	/* scanline, horizontal, each pixel */
	for (n = td->tile->orig_x; n < td->tile->orig_x + td->tile->size_x; n++) {
	    /* depth of view samples */
	    if (td->camera->view_num > 1) {
		VSET(accum, 0, 0, 0);

		for (d = 0; d < td->camera->view_num; d++) {
		    VSCALE(ray.dir, td->camera->view_list[d].step_y, v_scanline);
		    VADD2(ray.dir, ray.dir, td->camera->view_list[d].top_l);
		    VSCALE(v1, td->camera->view_list[d].step_x, n);
		    VADD2(ray.dir, ray.dir, v1);

		    VSET(pixel, (TFLOAT)RENDER_CAMERA_BGR, (TFLOAT)RENDER_CAMERA_BGG, (TFLOAT)RENDER_CAMERA_BGB);

		    VMOVE(ray.pos, td->camera->view_list[d].pos);
		    ray.depth = 0;
		    VUNITIZE(ray.dir);
		    annotation_ray = ray;

		    /* Compute pixel value using this ray */
		    td->camera->render.work(&td->camera->render, td->tie, &ray, &pixel);
		    render_camera_composite_annotation(td, annotation,
			&annotation_ray,
			(fastf_t)n + 0.5,
			(fastf_t)td->camera->h - (fastf_t)v_scanline - 0.5,
			&pixel);

		    VADD2(accum, accum, pixel);
		}

		/* Find Mean value of all views */
		VSCALE(pixel, accum, view_inv);
	    } else {
		if (td->camera->type == RENDER_CAMERA_PERSPECTIVE) {
		    VSCALE(v2, td->camera->view_list[0].step_x, n);
		    VADD2(ray.dir, v1, v2);

		    VSET(pixel, (TFLOAT)RENDER_CAMERA_BGR, (TFLOAT)RENDER_CAMERA_BGG, (TFLOAT)RENDER_CAMERA_BGB);

		    VMOVE(ray.pos, td->camera->view_list[0].pos);
		    ray.depth = 0;
		    VUNITIZE(ray.dir);
		    annotation_ray = ray;

		    /* Compute pixel value using this ray */
		    td->camera->render.work(&td->camera->render, td->tie, &ray, &pixel);
		} else {
		    VMOVE(ray.pos, td->camera->view_list[0].pos);
		    VMOVE(ray.dir, td->camera->view_list[0].top_l);

		    VSCALE(v1, td->camera->view_list[0].step_x, n);
		    VSCALE(v2, td->camera->view_list[0].step_y, v_scanline);
		    VADD2(ray.pos, ray.pos, v1);
		    VADD2(ray.pos, ray.pos, v2);

		    VSET(pixel, (TFLOAT)RENDER_CAMERA_BGR, (TFLOAT)RENDER_CAMERA_BGG, (TFLOAT)RENDER_CAMERA_BGB);
		    ray.depth = 0;
		    annotation_ray = ray;

		    /* Compute pixel value using this ray */
		    td->camera->render.work(&td->camera->render, td->tie, &ray, &pixel);
		}
	    }

	    if (td->camera->view_num == 1)
		render_camera_composite_annotation(td, annotation,
		    &annotation_ray, (fastf_t)n + 0.5,
		    (fastf_t)td->camera->h - (fastf_t)v_scanline - 0.5,
		    &pixel);


	    if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_24) {
		V_MIN(pixel[0], 1);
		V_MIN(pixel[1], 1);
		V_MIN(pixel[2], 1);
		((char *)(td->res_buf))[res_ind+0] = (unsigned char)(255 * pixel[0]);
		((char *)(td->res_buf))[res_ind+1] = (unsigned char)(255 * pixel[1]);
		((char *)(td->res_buf))[res_ind+2] = (unsigned char)(255 * pixel[2]);
		res_ind += 3;
	    } else if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_128) {
		TFLOAT alpha;

		alpha = 1.0;

		((TFLOAT *)(td->res_buf))[res_ind + 0] = pixel[0];
		((TFLOAT *)(td->res_buf))[res_ind + 1] = pixel[1];
		((TFLOAT *)(td->res_buf))[res_ind + 2] = pixel[2];
		((TFLOAT *)(td->res_buf))[res_ind + 3] = alpha;

		res_ind += 4;
	    }
	}
    }
}


void
render_camera_render_thread(int cpu, void *ptr)
{
    render_camera_render_thread_impl(cpu, (render_camera_thread_data_t *)ptr,
	NULL);
}


static void
render_camera_render_annotations_thread(int cpu, void *ptr)
{
    struct render_camera_parallel_data *data =
	(struct render_camera_parallel_data *)ptr;

    render_camera_render_thread_impl(cpu, &data->thread, &data->annotation);
}


static void
render_camera_render_internal(render_camera_t *camera, struct tie_s *tie,
	camera_tile_t *tile, tienet_buffer_t *result,
	const struct rt_annot_scene *annotations,
	fastf_t model_units_per_tie_unit)
{
    struct render_camera_parallel_data data = {0};
    unsigned int scanline;
    uint32_t ind;

    ind = result->ind;

    /* Allocate storage for results */
    if (tile->format == RENDER_CAMERA_BIT_DEPTH_24) {
	ind += 3 * (unsigned int)tile->size_x * (unsigned int)tile->size_y + sizeof(camera_tile_t);
    } else if (tile->format == RENDER_CAMERA_BIT_DEPTH_128) {
	ind += 4 * sizeof(TFLOAT) * (unsigned int)tile->size_x * (unsigned int)tile->size_y + sizeof(camera_tile_t);
    }

    TIENET_BUFFER_SIZE((*result), ind);

    TCOPY(camera_tile_t, tile, 0, result->data, result->ind);
    result->ind += sizeof(camera_tile_t);

    data.thread.tie = tie;
    data.thread.camera = camera;
    data.thread.tile = tile;
    data.thread.res_buf = &((char *)result->data)[result->ind];
    scanline = 0;
    data.thread.scanline = &scanline;
    data.thread.sem_tie_worker = bu_semaphore_register("sem_tie_worker");
    data.annotation.scene = annotations;
    data.annotation.model_units_per_tie_unit =
	model_units_per_tie_unit > 0.0 ? model_units_per_tie_unit : 1.0;
    if (annotations)
	render_camera_annotation_view(camera,
	    data.annotation.model_units_per_tie_unit, &data.annotation.view);

    if (annotations)
	bu_parallel(render_camera_render_annotations_thread,
	    camera->thread_num, &data);
    else
	bu_parallel(render_camera_render_thread, camera->thread_num,
	    &data.thread);

    result->ind = ind;
}


void
render_camera_render(render_camera_t *camera, struct tie_s *tie,
	camera_tile_t *tile, tienet_buffer_t *result)
{
    render_camera_render_internal(camera, tie, tile, result, NULL, 1.0);
}


void
render_camera_render_annotations(render_camera_t *camera, struct tie_s *tie,
	camera_tile_t *tile, tienet_buffer_t *result,
	const struct rt_annot_scene *annotations,
	fastf_t model_units_per_tie_unit)
{
    render_camera_render_internal(camera, tie, tile, result, annotations,
	model_units_per_tie_unit);
}


struct render_shader_s *
render_shader_register(const char *name, int (*init)(render_t *, const char *))
{
    struct render_shader_s *shader;
    BU_ALLOC(shader, struct render_shader_s);

    /* should probably search shader list for dups */
    shader->name = name;
    shader->init = init;
    shader->next = shaders;
    shader->dlh = NULL;
    shaders = shader;
    return shader;
}


const char *
render_shader_load_plugin(const char *filename)
{
#ifdef HAVE_DLFCN_H
    void *lh;	/* library handle */
    void *init_val;
    int (*init)(render_t *, const char *);
    char *name;
    struct render_shader_s *s;

    lh = bu_dlopen(filename, RTLD_LOCAL|RTLD_LAZY);

    if (lh == NULL) {
	bu_log("Faulty plugin %s: %s\n", filename, bu_dlerror());
	return NULL;
    }
    name = (char *)bu_dlsym(lh, "name");
    if (name == NULL) {
	bu_log("Faulty plugin %s: No name\n", filename);
	bu_dlclose(lh);
	return NULL;
    }
    /* assumes function pointers can be stored as a number, which ISO C does not guarantee */
    init_val = bu_dlsym(lh, "init");
    init = (int (*) (render_t *, const char *))(intptr_t)init_val;
    if (init == NULL) {
	bu_log("Faulty plugin %s: No init\n", filename);
	bu_dlclose(lh);
	return NULL;
    }
    s = render_shader_register(name, init);
    s->dlh = lh;
    return s->name;
#else
    bu_log("No plugin support.\n");
    return NULL;
#endif
}


int
render_shader_unload_plugin(render_t *r, const char *name)
{
#ifdef HAVE_DLFCN_H
    struct render_shader_s *t, *s = shaders, *meh;
    if (!bu_strncmp(s->name, name, 8)) {
	t = s->next;
	if (r && r->shader && !bu_strncmp(r->shader, name, 8)) {
	    meh = s->next;
	    while (meh) {
		if (render_shader_init(r, meh->name, NULL) != -1)
		    goto LOADED;
		meh = meh->next;
	    }
	    bu_exit(-1, "Unable to find suitable shader\n");
	}
LOADED:

	if (s->dlh)
	    bu_dlclose(s->dlh);
	bu_free(s, "unload first shader");
	shaders = t;
	return 0;
    }

    while (s->next) {
	if (!bu_strncmp(s->next->name, name, 8)) {
	    if (r)
		render_shader_init(r, s->name, NULL);
	    if (s->next->dlh)
		bu_dlclose(s->next->dlh);
	    t = s->next;
	    s->next = s->next->next;
	    bu_free(t, "unload shader");
	    return 0;
	}
    }

    bu_log("Could not find shader \"%s\"\n", name);
#else
    bu_log("No plugin support.\n");
#endif
    return -1;
}


int
render_shader_init(render_t *r, const char *name, const char *buf)
{
    struct render_shader_s *s = shaders;
    while (s) {
	if (!bu_strncmp(s->name, name, 8)) {
	    s->init(r, buf);
	    r->shader = s->name;
	    return 0;
	}
	s = s->next;
    }
    bu_log("Shader \"%s\" not found\n", name);
    return -1;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
