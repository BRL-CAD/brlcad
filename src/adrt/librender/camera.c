/*                        C A M E R A . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
	int (*init)(render_t *, const char *);
	void *dlh;	/* dynamic library handle */
	struct render_shader_s *next;
};

static struct render_shader_s *shaders = NULL;

void render_camera_render_thread(int cpu, genptr_t ptr);	/* for bu_parallel */
static void render_camera_prep_ortho(render_camera_t *camera);
static void render_camera_prep_persp(render_camera_t *camera);
static void render_camera_prep_persp_dof(render_camera_t *camera);

static struct render_shader_s *render_shader_register (const char *name, int (*init)(render_t *, const char *));

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
render_camera_free(render_camera_t *UNUSED(camera))
{
}


static void
render_camera_prep_ortho(render_camera_t *camera)
{
    vect_t look, up, side, temp;
    tfloat angle, s, c;

    /* Generate standard up vector */
    up[0] = 0;
    up[1] = 0;
    up[2] = 1;

    /* Generate unitized lookector */
    VSUB2(look,  camera->focus,  camera->pos);
    VUNITIZE(look);

    /* Make unitized up vector perpendicular to lookector */
    VMOVE(temp, look);
    angle = VDOT( up,  temp);
    VSCALE(temp,  temp,  angle);
    VSUB2(up,  up,  temp);
    VUNITIZE(up);

    /* Generate a temporary side vector */
    VCROSS(side,  up,  look);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    s = sin(-camera->tilt * DEG2RAD);
    c = cos(-camera->tilt * DEG2RAD);
    VSCALE(up,  up,  c);
    VSCALE(side,  side,  s);
    VADD2(up,  up,  side);

    /* Create final side vector */
    VCROSS(side,  up,  look);

    /* look direction */
    VMOVE(camera->view_list[0].top_l, look);

    /* gridsize is milimeters along the horizontal axis to display */
    /* left (side) */
    VSCALE(temp,  side,  (camera->aspect * camera->gridsize * 0.5));
    VADD2(camera->view_list[0].pos,  camera->pos,  temp);
    /* and (up) */
    VSCALE(temp,  up,  (camera->gridsize * 0.5));
    VADD2(camera->view_list[0].pos,  camera->view_list[0].pos,  temp);

    /* compute step vectors for camera position */

    /* X */
    VSCALE(camera->view_list[0].step_x,  side,  (-camera->gridsize * camera->aspect / (tfloat)camera->w));

    /* Y */
    VSCALE(camera->view_list[0].step_y,  up,  (-camera->gridsize / (tfloat)camera->h));
}

static void
render_camera_prep_persp(render_camera_t *camera)
{
    vect_t look, up, side, temp, topl, topr, botl;
    tfloat angle, s, c;


    /* Generate unitized lookector */
    VSUB2(look,  camera->focus,  camera->pos);
    VUNITIZE(look);

    /* Generate standard up vector */
    up[0] = 0;
    up[1] = 0;
    up[2] = 1;

    /* Make unitized up vector perpendicular to lookector */
    VMOVE(temp, look);
    angle = VDOT(up,  temp);
    VSCALE(temp,  temp,  angle);
    VSUB2(up,  up,  temp);
    VUNITIZE(up);

    /* Generate a temporary side vector */
    VCROSS(side,  up,  look);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    s = sin(-camera->tilt * DEG2RAD);
    c = cos(-camera->tilt * DEG2RAD);
    VSCALE(up,  up,  c);
    VSCALE(side,  side,  s);
    VADD2(up,  up,  side);

    /* Create final side vector */
    VCROSS(side,  up,  look);

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
    VSUB2(camera->view_list[0].step_x,  topr,  topl);
    VSUB2(camera->view_list[0].step_y,  botl,  topl);

    /* Divide stepx and stepy by the number of pixels */
    VSCALE(camera->view_list[0].step_x,  camera->view_list[0].step_x,  1.0 / camera->w);
    VSCALE(camera->view_list[0].step_y,  camera->view_list[0].step_y,  1.0 / camera->h);
    return;
}

static void
render_camera_prep_persp_dof(render_camera_t *camera)
{
    vect_t look, up, side, dof_look, dof_up, dof_side, dof_topl, dof_topr, dof_botl, temp, step_x, step_y, topl, topr, botl;
    tfloat angle, mag, sfov, cfov, sdof, cdof;
    uint32_t i, n;


    /* Generate unitized lookector */
    VSUB2(dof_look,  camera->focus,  camera->pos);
    VUNITIZE(dof_look);

    /* Generate standard up vector */
    dof_up[0] = 0;
    dof_up[1] = 0;
    dof_up[2] = 1;

    /* Make unitized up vector perpendicular to lookector */
    VMOVE(temp, dof_look);
    angle = VDOT( dof_up,  temp);
    VSCALE(temp,  temp,  angle);
    VSUB2(dof_up,  dof_up,  temp);
    VUNITIZE(dof_up);

    /* Generate a temporary side vector */
    VCROSS(dof_side,  dof_up,  dof_look);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    sdof = sin(-camera->tilt * DEG2RAD);
    cdof = cos(-camera->tilt * DEG2RAD);
    VSCALE(dof_up,  dof_up,  cdof);
    VSCALE(dof_side,  dof_side,  sdof);
    VADD2(dof_up,  dof_up,  dof_side);

    /* Create final side vector */
    VCROSS(dof_side,  dof_up,  dof_look);

    /*
     * Generage a camera position, top left vector, and step vectors for each DOF sample
     */

    /* Obtain magnitude of reverse lookector */
    VSUB2(dof_look,  camera->pos,  camera->focus);
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

    VSUB2(step_x,  dof_topr,  dof_topl);
    VSUB2(step_y,  dof_botl,  dof_topl);

    for (i = 0; i < RENDER_CAMERA_DOF_SAMPLES; i++)
    {
	for (n = 0; n < RENDER_CAMERA_DOF_SAMPLES; n++)
	{
	    /* Generate virtual camera position for this depth of field sample */
	    VSCALE(temp,  step_x,  ((tfloat)i/(tfloat)(RENDER_CAMERA_DOF_SAMPLES-1)));
	    VADD2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos,  dof_topl,  temp);
	    VSCALE(temp,  step_y,  ((tfloat)n/(tfloat)(RENDER_CAMERA_DOF_SAMPLES-1)));
	    VADD2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos,  temp);
	    VUNITIZE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos);
	    VSCALE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos,  mag);
	    VADD2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos,  camera->focus);

	    /* Generate unitized lookector */
	    VSUB2(look,  camera->focus,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].pos);
	    VUNITIZE(look);

	    /* Generate standard up vector */
	    up[0] = 0;
	    up[1] = 0;
	    up[2] = 1;

	    /* Make unitized up vector perpendicular to lookector */
	    VMOVE(temp, look);
	    angle = VDOT( up,  temp);
	    VSCALE(temp,  temp,  angle);
	    VSUB2(up,  up,  temp);
	    VUNITIZE(up);

	    /* Generate a temporary side vector */
	    VCROSS(side,  up,  look);

	    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
	    sfov = sin(-camera->tilt * DEG2RAD);
	    cfov = cos(-camera->tilt * DEG2RAD);
	    VSCALE(up,  up,  cfov);
	    VSCALE(side,  side,  sfov);
	    VADD2(up,  up,  side);

	    /* Create final side vector */
	    VCROSS(side,  up,  look);

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
	    VSUB2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_x,  topr,  topl);
	    VSUB2(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_y,  botl,  topl);

	    /* Divide stepx and stepy by the number of pixels */
	    VSCALE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_x,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_x,  1.0 / camera->w);
	    VSCALE(camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_y,  camera->view_list[i*RENDER_CAMERA_DOF_SAMPLES+n].step_y,  1.0 / camera->h);
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
render_camera_render_thread(int UNUSED(cpu), genptr_t ptr)
{
    render_camera_thread_data_t *td;
    int d, n, res_ind, scanline, v_scanline;
    vect_t pixel, accum, v1, v2;
    struct tie_ray_s ray;
    fastf_t view_inv;

    VSETALL(v1, 0);

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
	    VSCALE(v1,  td->camera->view_list[0].step_y,  v_scanline);
	    VADD2(v1,  v1,  td->camera->view_list[0].top_l);
	}


	/* scanline, horizontal, each pixel */
	for (n = td->tile->orig_x; n < td->tile->orig_x + td->tile->size_x; n++)
	{
	    /* depth of view samples */
	    if (td->camera->view_num > 1)
	    {
		VSET(accum, 0, 0, 0);

		for (d = 0; d < td->camera->view_num; d++)
		{
		    VSCALE(ray.dir,  td->camera->view_list[d].step_y,  v_scanline);
		    VADD2(ray.dir,  ray.dir,  td->camera->view_list[d].top_l);
		    VSCALE(v1,  td->camera->view_list[d].step_x,  n);
		    VADD2(ray.dir,  ray.dir,  v1);

		    VSET(pixel, (tfloat)RENDER_CAMERA_BGR, (tfloat)RENDER_CAMERA_BGG, (tfloat)RENDER_CAMERA_BGB);

		    VMOVE(ray.pos, td->camera->view_list[d].pos);
		    ray.depth = 0;
		    VUNITIZE(ray.dir);

		    /* Compute pixel value using this ray */
		    td->camera->render.work(&td->camera->render, td->tie, &ray, &pixel);

		    VADD2(accum,  accum,  pixel);
		}

		/* Find Mean value of all views */
		VSCALE(pixel,  accum,  view_inv);
	    }
	    else
	    {
		if (td->camera->type == RENDER_CAMERA_PERSPECTIVE)
		{
		    VSCALE(v2,  td->camera->view_list[0].step_x,  n);
		    VADD2(ray.dir,  v1,  v2);

		    VSET(pixel, (tfloat)RENDER_CAMERA_BGR, (tfloat)RENDER_CAMERA_BGG, (tfloat)RENDER_CAMERA_BGB);

		    VMOVE(ray.pos, td->camera->view_list[0].pos);
		    ray.depth = 0;
		    VUNITIZE(ray.dir);

		    /* Compute pixel value using this ray */
		    td->camera->render.work(&td->camera->render, td->tie, &ray, &pixel);
		} else {
		    VMOVE(ray.pos, td->camera->view_list[0].pos);
		    VMOVE(ray.dir, td->camera->view_list[0].top_l);

		    VSCALE(v1,  td->camera->view_list[0].step_x,  n);
		    VSCALE(v2,  td->camera->view_list[0].step_y,  v_scanline);
		    VADD2(ray.pos,  ray.pos,  v1);
		    VADD2(ray.pos,  ray.pos,  v2);

		    VSET(pixel, (tfloat)RENDER_CAMERA_BGR, (tfloat)RENDER_CAMERA_BGG, (tfloat)RENDER_CAMERA_BGB);
		    ray.depth = 0;

		    /* Compute pixel value using this ray */
		    td->camera->render.work(&td->camera->render, td->tie, &ray, &pixel);
		}
	    }


	    if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_24)
	    {
		if (pixel[0] > 1) pixel[0] = 1;
		if (pixel[1] > 1) pixel[1] = 1;
		if (pixel[2] > 1) pixel[2] = 1;
		((char *)(td->res_buf))[res_ind+0] = (unsigned char)(255 * pixel[0]);
		((char *)(td->res_buf))[res_ind+1] = (unsigned char)(255 * pixel[1]);
		((char *)(td->res_buf))[res_ind+2] = (unsigned char)(255 * pixel[2]);
		res_ind += 3;
	    }
	    else if (td->tile->format == RENDER_CAMERA_BIT_DEPTH_128)
	    {
		tfloat alpha;

		alpha = 1.0;

		((tfloat *)(td->res_buf))[res_ind + 0] = pixel[0];
		((tfloat *)(td->res_buf))[res_ind + 1] = pixel[1];
		((tfloat *)(td->res_buf))[res_ind + 2] = pixel[2];
		((tfloat *)(td->res_buf))[res_ind + 3] = alpha;

		res_ind += 4;
	    }
/*          printf("Pixel: [%d, %d, %d]\n", rgb[0], rgb[1], rgb[2]); */

	}
    }
}


void
render_camera_render(render_camera_t *camera, struct tie_s *tie, camera_tile_t *tile, tienet_buffer_t *result)
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
render_shader_register(const char *name, int (*init)(render_t *, const char *))
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
    int (*init)(render_t *, const char *);
    char *name;
    struct render_shader_s *s;

    lh = bu_dlopen(filename, RTLD_LOCAL|RTLD_LAZY);

    if(lh == NULL) { bu_log("Faulty plugin %s: %s\n", filename, bu_dlerror()); return NULL; }
    name = bu_dlsym(lh, "name");
    if(name == NULL) { bu_log("Faulty plugin %s: No name\n", filename); return NULL; }
    /* assumes function pointers can be stored as a number, which ISO C does not guarantee */
    init = (int (*) (render_t *, const char *))(intptr_t)bu_dlsym(lh, "init");
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
	    bu_dlclose(s->dlh);
	bu_free(s, "unload first shader");
	shaders = t;
	return 0;
    }

    while(s->next) {
	if(!strncmp(s->next->name, name, 8)) {
	    if(r)
		render_shader_init(r, s->name, NULL);
	    if(s->next->dlh)
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
