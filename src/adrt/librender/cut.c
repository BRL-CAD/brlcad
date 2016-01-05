/*                        C U T . C
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
/** @file librender/cut.c
 *
 */

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "bu/malloc.h"
#include "bu/log.h"
#include "vmath.h"

#ifndef TIE_PRECISION
#  define TIE_PRECISION 0
#endif

#include "adrt.h"
#include "adrt_struct.h"
#include "render.h"


extern struct tie_s *tie;

void* render_cut_hit(struct tie_ray_s *ray, struct tie_id_s *id, struct tie_tri_s *tri, void *ptr);
void render_cut(struct tie_s *tie, struct tie_ray_s *ray, TIE_3 *pixel);


typedef struct render_cut_s {
    point_t ray_pos;
    vect_t ray_dir;
    tfloat plane[4];
    struct tie_s tie;
} render_cut_t;

typedef struct render_cut_hit_s {
    struct tie_id_s id;
    adrt_mesh_t *mesh;
    tfloat plane[4];
    tfloat mod;
} render_cut_hit_t;


void *
render_cut_hit_cutline(struct tie_ray_s *UNUSED(ray), struct tie_id_s *UNUSED(id), struct tie_tri_s *tri, void *UNUSED(ptr))
{
    ((adrt_mesh_t *)(tri->ptr))->flags |= ADRT_MESH_HIT;
    return NULL;
}


void
render_cut_free(render_t *render)
{
    render_cut_t *d;

    d = (render_cut_t *)render->data;
    tie_free(&d->tie);
    bu_free(render->data, "render_cut_free");
}


static void *
render_arrow_hit(struct tie_ray_s *UNUSED(ray), struct tie_id_s *UNUSED(id), struct tie_tri_s *tri, void *UNUSED(ptr))
{
    return tri;
}


void *
render_cut_hit(struct tie_ray_s *UNUSED(ray), struct tie_id_s *id, struct tie_tri_s *tri, void *ptr)
{
    render_cut_hit_t *hit = (render_cut_hit_t *)ptr;

    hit->id = *id;
    hit->mesh = (adrt_mesh_t *)(tri->ptr);
    return hit ;
}


void
render_cut_work(render_t *render, struct tie_s *tiep, struct tie_ray_s *ray, vect_t *pixel)
{
    render_cut_t *rd;
    render_cut_hit_t hit;
    vect_t color;
    struct tie_id_s id;
    tfloat t, dot;

    rd = (render_cut_t *)render->data;

    /* Draw Arrow - Blue */
    if (tie_work(&rd->tie, ray, &id, render_arrow_hit, NULL)) {
	VSET(*pixel, 0.0, 0.0, 1.0);
	return;
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
