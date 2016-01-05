/*                         S P A L L . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2016 United States Government as represented by
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
/** @file librender/spall.c
 *
 */

#include "render_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#include "bu/malloc.h"
#include "bu/log.h"
#include "adrt_struct.h"
#include "render.h"

#define TESSELLATION 32
#define SPALL_LEN 20

struct render_spall_s {
    point_t ray_pos;
    vect_t ray_dir;
    fastf_t plane[4];
    fastf_t angle;
    struct tie_s tie;
};


void* render_spall_hit(struct tie_ray_s *ray, struct tie_id_s *id, struct tie_tri_s *tri, void *ptr);
void render_plane(struct tie_s *tie, struct tie_ray_s *ray, TIE_3 *pixel);

struct render_spall_hit_s {
    struct tie_id_s id;
    adrt_mesh_t *mesh;
    fastf_t plane[4];
    fastf_t mod;
};


void
render_spall_free(render_t *render)
{
    bu_free(render->data, "render data");
}


static void *
render_arrow_hit(struct tie_ray_s *UNUSED(ray), struct tie_id_s *UNUSED(id), struct tie_tri_s *tri, void *UNUSED(ptr))
{
    return tri;
}


void *
render_spall_hit(struct tie_ray_s *UNUSED(ray), struct tie_id_s *id, struct tie_tri_s *tri, void *ptr)
{
    struct render_spall_hit_s *hit = (struct render_spall_hit_s *)ptr;

    hit->id = *id;
    hit->mesh = (adrt_mesh_t *)(tri->ptr);
    return hit;
}


void
render_spall_work(render_t *render, struct tie_s *tie, struct tie_ray_s *ray, vect_t *pixel)
{
    struct render_spall_s *rd;
    struct render_spall_hit_s hit;
    vect_t color;
    struct tie_id_s id;
    tfloat t, dot;


    rd = (struct render_spall_s *)render->data;

    /* Draw spall Cone */
    if (tie_work(&rd->tie, ray, &id, render_arrow_hit, NULL)) {
	*pixel[0] = (tfloat)0.4;
	*pixel[1] = (tfloat)0.4;
	*pixel[2] = (tfloat)0.4;
    }

    /*
     * I don't think this needs to be done for every pixel?
     * Flip plane normal to face us.
     */
    t = ray->pos[0]*rd->plane[0] + ray->pos[1]*rd->plane[1] + ray->pos[2]*rd->plane[2] + rd->plane[3];
    hit.mod = t < 0 ? 1 : -1;


    /*
     * Optimization:
     * First intersect this ray with the plane and fire the ray from there
     * Plane: Ax + By + Cz + D = 0
     * Ray = O + td
