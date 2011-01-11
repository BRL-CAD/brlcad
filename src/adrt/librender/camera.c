/*                        C A M E R A . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2010 United States Government as represented by
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
/** @file camera.c
 *
 * Brief description
 *
 */

#include "common.h"

#ifdef HAVE_DLFCN_H
# include <dlfcn.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bio.h"
#include "bu.h"
#include "raytrace.h" /* for last RT_SEM_LAST */

#include "camera.h"

#define TIE_SEM_WORKER (RT_SEM_LAST)
#define TIE_SEM_LAST (TIE_SEM_WORKER+1)

struct render_shader_s {
	const char *name;
	void (*init)(render_t *, const char *);
	void *dlh;	/* dynamic library handle */
	struct render_shader_s *next;
};

static struct render_shader_s *shaders = NULL;

void render_camera_render_thread(int cpu, genptr_t ptr);	/* for bu_parallel */
static void render_camera_prep_ortho(render_camera_t *camera);
static void render_camera_prep_persp(render_camera_t *camera);
static void render_camera_prep_persp_dof(render_camera_t *camera);

static struct render_shader_s *render_shader_register (const char *name, void (*init)(render_t *, const char *));

void
render_camera_init(render_camera_t *camera, int threads)
{
    camera->type = RENDER_CAMERA_PERSPECTIVE;

    camera->view_num = 1;
    camera->view_list = (render_camera_view_t *) bu_malloc (sizeof(render_camera_view_t), "render_camera_init");
    camera->dof = 0;
    camera->tilt = 0;

    /* The camera will use a thread for every cpu the machine has. */
    camera->thread_num = threads ? threads : bu_avail_cpus();

    bu_semaphore_init(TIE_SEM_LAST);

    /* Initialize camera to rendering surface normals */
    render_normal_init(&camera->render, NULL);
    camera->rm = RENDER_METHOD_PHONG;

    if(shaders == NULL) {
#define REGISTER(x) render_shader_register((const char *)#x, render_##x##_init);
	REGISTER(component);
	REGISTER(cut);
	REGISTER(depth);
	REGISTER(flat);
	REGISTER(flos);
	REGISTER(grid);
	REGISTER(normal);
	REGISTER(path);
	REGISTER(phong);
	REGISTER(spall);
	REGISTER(surfel);
#undef REGISTER
    }
}


void
render_camera_free(render_camera_t *camera)
{
}


static void
render_camera_prep_ortho(render_camera_t *camera)
{
    TIE_3 look, up, side, temp;
    tfloat angle, s, c;

    /* Generate standard up vector */
    up.v[0] = 0;
    up.v[1] = 0;
    up.v[2] = 1;

    /* Generate unitized look vector */
    VSUB2(look.v,  camera->focus.v,  camera->pos.v);
    VUNITIZE(look.v);

    /* Make unitized up vector perpendicular to look vector */
    temp = look;
    angle = VDOT( up.v,  temp.v);
    VSCALE(temp.v,  temp.v,  angle);
    VSUB2(up.v,  up.v,  temp.v);
    VUNITIZE(up.v);

    /* Generate a temporary side vector */
    VCROSS(side.v,  up.v,  look.v);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    s = sin(-camera->tilt * DEG2RAD);
    c = cos(-camera->tilt * DEG2RAD);
    VSCALE(up.v,  up.v,  c);
    VSCALE(side.v,  side.v,  s);
    VADD2(up.v,  up.v,  side.v);

    /* Create final side vector */
    VCROSS(side.v,  up.v,  look.v);

    /* look direction */
    camera->view_list[0].top_l = look;

    /* gridsize is milimeters along the horizontal axis to display */
    /* left (side) */
    VSCALE(temp.v,  side.v,  (camera->aspect * camera->gridsize * 0.5));
    VADD2(camera->view_list[0].pos.v,  camera->pos.v,  temp.v);
    /* and (up) */
    VSCALE(temp.v,  up.v,  (camera->gridsize * 0.5));
    VADD2(camera->view_list[0].pos.v,  camera->view_list[0].pos.v,  temp.v);

    /* compute step vectors for camera position */

    /* X */
    VSCALE(camera->view_list[0].step_x.v,  side.v,  (-camera->gridsize * camera->aspect / (tfloat)camera->w));

    /* Y */
    VSCALE(camera->view_list[0].step_y.v,  up.v,  (-camera->gridsize / (tfloat)camera->h));
}

static void
render_camera_prep_persp(render_camera_t *camera)
{
    TIE_3 look, up, side, temp, topl, topr, botl;
    tfloat angle, s, c;


    /* Generate unitized look vector */
    VSUB2(look.v,  camera->focus.v,  camera->pos.v);
    VUNITIZE(look.v);

    /* Generate standard up vector */
    up.v[0] = 0;
    up.v[1] = 0;
    up.v[2] = 1;

    /* Make unitized up vector perpendicular to look vector */
    temp = look;
    angle = VDOT( up.v,  temp.v);
    VSCALE(temp.v,  temp.v,  angle);
    VSUB2(up.v,  up.v,  temp.v);
    VUNITIZE(up.v);

    /* Generate a temporary side vector */
    VCROSS(side.v,  up.v,  look.v);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    s = sin(-camera->tilt * DEG2RAD);
    c = cos(-camera->tilt * DEG2RAD);
    VSCALE(up.v,  up.v,  c);
    VSCALE(side.v,  side.v,  s);
    VADD2(up.v,  up.v,  side.v);

    /* Create final side vector */
    VCROSS(side.v,  up.v,  look.v);

    /* Compute sine and cosine terms for field of view */
    s = sin(camera->fov*DEG2RAD);
    c = cos(camera->fov*DEG2RAD);

    /* Up, Look, and Side vectors are complete, generate Top Left reference vector */
    topl.v[0] = s*up.v[0] + camera->aspect*s*side.v[0] + c*look.v[0];
    topl.v[1] = s*up.v[1] + camera->aspect*s*side.v[1] + c*look.v[1];
    topl.v[2] = s*up.v[2] + camera->aspect*s*side.v[2] + c*look.v[2];

    topr.v[0] = s*up.v[0] - camera->aspect*s*side.v[0] + c*look.v[0];
    topr.v[1] = s*up.v[1] - camera->aspect*s*side.v[1] + c*look.v[1];
    topr.v[2] = s*up.v[2] - camera->aspect*s*side.v[2] + c*look.v[2];

    botl.v[0] = -s*up.v[0] + camera->aspect*s*side.v[0] + c*look.v[0];
    botl.v[1] = -s*up.v[1] + camera->aspect*s*side.v[1] + c*look.v[1];
    botl.v[2] = -s*up.v[2] + camera->aspect*s*side.v[2] + c*look.v[2];

    VUNITIZE(topl.v);
    VUNITIZE(botl.v);
    VUNITIZE(topr.v);

    /* Store Camera Position */
    camera->view_list[0].pos = camera->pos;

    /* Store the top left vector */
    camera->view_list[0].top_l = topl;

    /* Generate stepx and stepy vectors for sampling each pixel */
    VSUB2(camera->view_list[0].step_x.v,  topr.v,  topl.v);
    VSUB2(camera->view_list[0].step_y.v,  botl.v,  topl.v);

    /* Divide stepx and stepy by the number of pixels */
    VSCALE(camera->view_list[0].step_x.v,  camera->view_list[0].step_x.v,  1.0 / camera->w);
    VSCALE(camera->view_list[0].step_y.v,  camera->view_list[0].step_y.v,  1.0 / camera->h);
    return;
}

static void
render_camera_prep_persp_dof(render_camera_t *camera)
{
    TIE_3 look, up, side, dof_look, dof_up, dof_side, dof_topl, dof_topr, dof_botl, temp, step_x, step_y, topl, topr, botl;
    tfloat angle, mag, sfov, cfov, sdof, cdof;
    uint32_t i, n;


    /* Generate unitized look vector */
    VSUB2(dof_look.v,  camera->focus.v,  camera->pos.v);
    VUNITIZE(dof_look.v);

    /* Generate standard up vector */
    dof_up.v[0] = 0;
    dof_up.v[1] = 0;
    dof_up.v[2] = 1;

    /* Make unitized up vector perpendicular to look vector */
    temp = dof_look;
    angle = VDOT( dof_up.v,  temp.v);
    VSCALE(temp.v,  temp.v,  angle);
    VSUB2(dof_up.v,  dof_up.v,  temp.v);
    VUNITIZE(dof_up.v);

    /* Generate a temporary side vector */
    VCROSS(dof_side.v,  dof_up.v,  dof_look.v);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    sdof = sin(-camera->tilt * DEG2RAD);
    cdof = cos(-camera->tilt * DEG2RAD);
    VSCALE(dof_up.v,  dof_up.v,  cdof);
    VSCALE(dof_side.v,  dof_side.v,  sdof);
    VADD2(dof_up.v,  dof_up.v,  dof_side.v);

    /* Create final side vector */
    VCROSS(dof_side.v,  dof_up.v,  dof_look.v);

    /*
     * Generage a camera position, top left vector, and step vectors for each DOF sample
     */

    /* Obtain magnitude of reverse look vector */
    VSUB2(dof_look.v,  camera->pos.v,  camera->focus.v);
    mag = MAGNITUDE(dof_look.v);
    VUNITIZE(dof_look.v);


    /* Compute sine and cosine terms for field of view */
    sdof = sin(camera->dof*DEG2RAD);
    cdof = cos(camera->dof*DEG2RAD);


    /* Up, Look, and Side vectors are complete, generate Top Left reference vector */
    dof_topl.v[0] = sdof*dof_up.v[0] + sdof*dof_side.v[0] + cdof*dof_look.v[0];
    dof_topl.v[1] = sdof*dof_up.v[1] + sdof*dof_side.v[1] + cdof*dof_look.v[1];
    dof_topl.v[2] = sdof*dof_up.v[2] + sdof*dof_side.v[2] + cdof*dof_look.v[2];

    dof_topr.v[0] = sdof*dof_up.v[0] - sdof*dof_side.v[0] + cdof*dof_look.v[0];
    dof_topr.v[1] = sdof*dof_up.v[1] - sdof*dof_side.v[1] + cdof*dof_look.v[1];
    dof_topr.v[2] = sdof*dof_up.v[2] - sdof*dof_side.v[2] + cdof*dof_look.v[2];

    dof_botl.v[0] = -sdof*dof_up.v[0] + sdof*dof_side.v[0] + cdof*dof_look.v[0];
    dof_botl.v[1] = -sdof*dof_up.v[1] + sdof*dof_side.v[1] + cdof*dof_look.v[1];
    dof_botl.v[2] = -sdof*dof_up.v[2] + sdof*dof_side.v[2] + cdof*dof_look.v[2];

    VUNITIZE(dof_topl.v);
    VUNITIZE(dof_botl.v);
    VUNITIZE(dof_topr.v);

    VSUB2(step_x.v,  dof_topr.v,  dof_topl.v);
    VSUB2(step_y.v,  dof_botl.v,  dof_topl.v);

    for (i = 0; i < RENDER_CAMERA_DOF_SAMPLES; i++)
    {
	for (n = 0; n < RENDER_CAMERA_DOF_SAMPLES; n++)
	{
	    /* Generate virtual camera position for this depth of field sample */
	    VSCALE(temp.v,  step_x.v,  ((tfloat)i/(tfloat)(RENDER_CAMERA_DOF_SAMPLES-1)));
	    VADD2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos.v,  dof_topl.v,  temp.v);
	    VSCALE(temp.v,  step_y.v,  ((tfloat)n/(tfloat)(RENDER_CAMERA_DOF_SAMPLES-1)));
	    VADD2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos.v,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos.v,  temp.v);
	    VUNITIZE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos.v);
	    VSCALE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos.v,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos.v,  mag);
	    VADD2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos.v,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos.v,  camera->focus.v);

	    /* Generate unitized look vector */
	    VSUB2(look.v,  camera->focus.v,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos.v);
	    VUNITIZE(look.v);

	    /* Generate standard up vector */
	    up.v[0] = 0;
	    up.v[1] = 0;
	    up.v[2] = 1;

	    /* Make unitized up vector perpendicular to look vector */
	    temp = look;
	    angle = VDOT( up.v,  temp.v);
	    VSCALE(temp.v,  temp.v,  angle);
	    VSUB2(up.v,  up.v,  temp.v);
	    VUNITIZE(up.v);

	    /* Generate a temporary side vector */
	    VCROSS(side.v,  up.v,  look.v);

	    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
	    sfov = sin(-camera->tilt * DEG2RAD);
	    cfov = cos(-camera->tilt * DEG2RAD);
	    VSCALE(up.v,  up.v,  cfov);
	    VSCALE(side.v,  side.v,  sfov);
	    VADD2(up.v,  up.v,  side.v);

	    /* Create final side vector */
	    VCROSS(side.v,  up.v,  look.v);

	    /* Compute sine and cosine terms for field of view */
	    sfov = sin(camera->fov*DEG2RAD);
	    cfov = cos(camera->fov*DEG2RAD);


	    /* Up, Look, and Side vectors are complete, generate Top Left reference vector */
	    topl.v[0] = sfov*up.v[0] + camera->aspect*sfov*side.v[0] + cfov*look.v[0];
	    topl.v[1] = sfov*up.v[1] + camera->aspect*sfov*side.v[1] + cfov*look.v[1];
	    topl.v[2] = sfov*up.v[2] + camera->aspect*sfov*side.v[2] + cfov*look.v[2];

	    topr.v[0] = sfov*up.v[0] - camera->aspect*sfov*side.v[0] + cfov*look.v[0];
	    topr.v[1] = sfov*up.v[1] - camera->aspect*sfov*side.v[1] + cfov*look.v[1];
	    topr.v[2] = sfov*up.v[2] - camera->aspect*sfov*side.v[2] + cfov*look.v[2];

	    botl.v[0] = -sfov*up.v[0] + camera->aspect*sfov*side.v[0] + cfov*look.v[0];
	    botl.v[1] = -sfov*up.v[1] + camera->aspect*sfov*side.v[1] + cfov*look.v[1];
	    botl.v[2] = -sfov*up.v[2] + camera->aspect*sfov*side.v[2] + cfov*look.v[2];

	    VUNITIZE(topl.v);
	    VUNITIZE(botl.v);
	    VUNITIZE(topr.v);

	    /* Store the top left vector */
	    camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].top_l = topl;

	    /* Generate stepx and stepy vectors for sampling each pixel */
	    VSUB2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_x.v,  topr.v,  topl.v);
	    VSUB2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_y.v,  botl.v,  topl.v);

	    /* Divide stepx and stepy by the number of pixels */
	    VSCALE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_x.v,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_x.v,  1.0 / camera->w);
	    VSCALE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_y.v,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_y.v,  1.0 / camera->h);
	}
    }
}

void
render_camera_prep(render_camera_t *camera)
{
    /* Generate an aspect ratio coefficient */
    camera->aspect = (tfloat)camera->w / (tfloat)camera->h;

    if (camera->type == RENDER_CAMERA_ORTHOGRAPHIC)
	render_camera_prep_ortho(camera);

    if (camera->type == RENDER_CAMERA_PERSPECTIVE)
    {
	if (camera->dof <= 0.0)
	{
	    render_camera_prep_persp(camera);
	}
	else
	{
	    /* Generate camera positions for depth of field - Handle this better */
	    camera->view_num = RENDER_CAMERA_DOF_SAMPLES*RENDER_CAMERA_DOF_SAMPLES;
	    camera->view_list = (render_camera_view_t *)bu_malloc(sizeof(render_camera_view_t) * camera->view_num, "camera view");

	    render_camera_prep_persp_dof(camera);
	}
    }
}


void
render_camera_render_thread(int cpu, genptr_t ptr)
{
    render_camera_thread_data_t *td;
    int d, n, res_ind, scanline, v_scanline;
    TIE_3 pixel, accum, v1, v2;
    tie_ray_t ray;
    tfloat view_inv;

    VSETALL(v1.v, 0);

    td = (render_camera_thread_data_t *)ptr;
    view_inv = 1.0 / td->camera->view_num;

    td->camera->render.tie = td->tie;

    res_ind = 0;
/* row, vertical */
/*
  for (i = td->tile->orig_y; i < td->tile->orig_y + td->tile->size_y; i++)
  {
*/
    while (1)
    {
	/* Determine if this scanline should be computed by this thread */
	bu_semaphore_acquire(TIE_SEM_WORKER);
	if (*td->scanline == td->tile->size_y)
	{
	    bu_semaphore_release(TIE_SEM_WORKER);
	    return;
	}
	else
	{
	    scanline = *td->scanline;
	    (*td->scanline)++;
	}
	bu_semaphore_release(TIE_SEM_WORKER);

	v_scanline = scanline + td->tile->orig_y;
	if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_24)
	{
	    res_ind = 3*scanline*td->tile->size_x;
	}
	else if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_128)
	{
	    res_ind = 4*scanline*td->tile->size_x;
	}


	/* optimization if there is no depth of field being applied */
	if (td->camera->view_num == 1)
	{
	    VSCALE(v1.v,  td->camera->view_list[0].step_y.v,  v_scanline);
	    VADD2(v1.v,  v1.v,  td->camera->view_list[0].top_l.v);
	}


	/* scanline, horizontal, each pixel */
	for (n = td->tile->orig_x; n < td->tile->orig_x + td->tile->size_x; n++)
	{
	    /* depth of view samples */
	    if (td->camera->view_num > 1)
	    {
		VSET(accum.v, 0, 0, 0);

		for (d = 0; d < td->camera->view_num; d++)
		{
		    VSCALE(ray.dir.v,  td->camera->view_list[d].step_y.v,  v_scanline);
		    VADD2(ray.dir.v,  ray.dir.v,  td->camera->view_list[d].top_l.v);
		    VSCALE(v1.v,  td->camera->view_list[d].step_x.v,  n);
		    VADD2(ray.dir.v,  ray.dir.v,  v1.v);

		    VSET(pixel.v, (tfloat)RENDER_CAMERA_BGR, (tfloat)RENDER_CAMERA_BGG, (tfloat)RENDER_CAMERA_BGB);

		    ray.pos = td->camera->view_list[d].pos;
		    ray.depth = 0;
		    VUNITIZE(ray.dir.v);

		    /* Compute pixel value using this ray */
		    td->camera->render.work(&td->camera->render, td->tie, &ray, &pixel);

		    VADD2(accum.v,  accum.v,  pixel.v);
		}

		/* Find Mean value of all views */
		VSCALE(pixel.v,  accum.v,  view_inv);
	    }
	    else
	    {
		if (td->camera->type == RENDER_CAMERA_PERSPECTIVE)
		{
		    VSCALE(v2.v,  td->camera->view_list[0].step_x.v,  n);
		    VADD2(ray.dir.v,  v1.v,  v2.v);

		    VSET(pixel.v, (tfloat)RENDER_CAMERA_BGR, (tfloat)RENDER_CAMERA_BGG, (tfloat)RENDER_CAMERA_BGB);

		    ray.pos = td->camera->view_list[0].pos;
		    ray.depth = 0;
		    VUNITIZE(ray.dir.v);

		    /* Compute pixel value using this ray */
		    td->camera->render.work(&td->camera->render, td->tie, &ray, &pixel);
		} else {
		    ray.pos = td->camera->view_list[0].pos;
		    ray.dir = td->camera->view_list[0].top_l;

		    VSCALE(v1.v,  td->camera->view_list[0].step_x.v,  n);
		    VSCALE(v2.v,  td->camera->view_list[0].step_y.v,  v_scanline);
		    VADD2(ray.pos.v,  ray.pos.v,  v1.v);
		    VADD2(ray.pos.v,  ray.pos.v,  v2.v);

		    VSET(pixel.v, (tfloat)RENDER_CAMERA_BGR, (tfloat)RENDER_CAMERA_BGG, (tfloat)RENDER_CAMERA_BGB);
		    ray.depth = 0;

		    /* Compute pixel value using this ray */
		    td->camera->render.work(&td->camera->render, td->tie, &ray, &pixel);
		}
	    }


	    if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_24)
	    {
		if (pixel.v[0] > 1) pixel.v[0] = 1;
		if (pixel.v[1] > 1) pixel.v[1] = 1;
		if (pixel.v[2] > 1) pixel.v[2] = 1;
		((char *)(td->res_buf))[res_ind+0] = (unsigned char)(255 * pixel.v[0]);
		((char *)(td->res_buf))[res_ind+1] = (unsigned char)(255 * pixel.v[1]);
		((char *)(td->res_buf))[res_ind+2] = (unsigned char)(255 * pixel.v[2]);
		res_ind += 3;
	    }
	    else if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_128)
	    {
		tfloat alpha;

		alpha = 1.0;

		((tfloat *)(td->res_buf))[res_ind + 0] = pixel.v[0];
		((tfloat *)(td->res_buf))[res_ind + 1] = pixel.v[1];
		((tfloat *)(td->res_buf))[res_ind + 2] = pixel.v[2];
		((tfloat *)(td->res_buf))[res_ind + 3] = alpha;

		res_ind += 4;
	    }
/*          printf("Pixel: [%d, %d, %d]\n", rgb[0], rgb[1], rgb[2]); */

	}
    }
}


void
render_camera_render(render_camera_t *camera, tie_t *tie, camera_tile_t *tile, tienet_buffer_t *result)
{
    render_camera_thread_data_t td;
    unsigned int scanline, ind;

    ind = result->ind;

    /* Allocate storage for results */
    if (tile->format == RENDER_CAMERA_BIT_DEPTH_24)
    {
	ind += 3 * tile->size_x * tile->size_y + sizeof(camera_tile_t);
    }
    else if (tile->format == RENDER_CAMERA_BIT_DEPTH_128)
    {
	ind += 4 * sizeof(tfloat) * tile->size_x * tile->size_y + sizeof(camera_tile_t);
    }

    TIENET_BUFFER_SIZE((*result), ind);

    TCOPY(camera_tile_t, tile, 0, result->data, result->ind);
    result->ind += sizeof(camera_tile_t);

    td.tie = tie;
    td.camera = camera;
    td.tile = tile;
    td.res_buf = &((char *)result->data)[result->ind];
    scanline = 0;
    td.scanline = &scanline;

    bu_parallel(render_camera_render_thread, camera->thread_num, &td);

    result->ind = ind;

    return;
}

struct render_shader_s *
render_shader_register(const char *name, void (*init)(render_t *, const char *))
{
	struct render_shader_s *shader = (struct render_shader_s *)bu_malloc(sizeof(struct render_shader_s), "shader");
	if(shader == NULL)
		return NULL;
	/* should probably search shader list for dups */
	shader->name = name;
	shader->init = init;
	shader->next = shaders;
	shader->dlh = NULL;
	shaders = shader;
	return shader;
}

const char *
render_shader_load_plugin(const char *filename) {
#ifdef HAVE_DLFCN_H
    void *lh;	/* library handle */
    void (*init)(render_t *, char *);
    char *name;
    struct render_shader_s *s;

    lh = dlopen(filename, RTLD_LOCAL|RTLD_LAZY);

    if(lh == NULL) { bu_log("Faulty plugin %s: %s\n", filename, dlerror()); return NULL; }
    name = dlsym(lh, "name");
    if(name == NULL) { bu_log("Faulty plugin %s: No name\n", filename); return NULL; }
    init = dlsym(lh, "init");
    if(init == NULL) { bu_log("Faulty plugin %s: No init\n", filename); return NULL; }
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
    if(!strncmp(s->name, name, 8)) {
	t = s->next;
	if(r && r->shader && !strncmp(r->shader, name, 8)) {
	    meh = s->next;
	    while( meh ) {
		if(render_shader_init(r, meh->name, NULL) != -1)
		    goto LOADED;
		meh = meh->next;
	    }
	    bu_exit(-1, "Unable to find suitable shader\n");
	}
LOADED:

	if(s->dlh)
	    dlclose(s->dlh);
	bu_free(s, "unload first shader");
	shaders = t; 
	return 0;
    }

    while(s->next) {
	if(!strncmp(s->next->name, name, 8)) {
	    if(r)
		render_shader_init(r, s->name, NULL);
	    if(s->next->dlh)
		dlclose(s->next->dlh);
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
    while(s) {
	if(!strncmp(s->name, name, 8)) {
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
