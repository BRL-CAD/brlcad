/*                        C U T . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2008 United States Government as represented by
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
/** @file cut.c
 *
 */

#include "cut.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"

#include "adrt.h"
#include "adrt_struct.h"
#include "hit.h"

#define THICKNESS 0.02
#define LENGTH 100

void* render_cut_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr);
void render_cut(tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);


typedef struct render_cut_hit_s {
    tie_id_t id;
    adrt_mesh_t *mesh;
    tfloat plane[4];
    tfloat mod;
} render_cut_hit_t;


void render_cut_init(render_t *render, TIE_3 ray_pos, TIE_3 ray_dir) {
    render_cut_t *d;
    TIE_3 list[6], normal, up;
    tfloat plane[4];

    render->work = render_cut_work;
    render->free = render_cut_free;

    render->data = (render_cut_t *)bu_malloc(sizeof(render_cut_t), "render_cut_init");
    if (!render->data) {
	perror("render->data");
	exit(1);
    }
    d = (render_cut_t *)render->data;

    d->ray_pos = ray_pos;
    d->ray_dir = ray_dir;

    tie_init(&d->tie, 2, TIE_KDTREE_FAST);

    /* Calculate the normal to be used for the plane */
    up.v[0] = 0;
    up.v[1] = 0;
    up.v[2] = 1;

    MATH_VEC_CROSS(normal, ray_dir, up);
    MATH_VEC_UNITIZE(normal);

    /* Construct the plane */
    d->plane[0] = normal.v[0];
    d->plane[1] = normal.v[1];
    d->plane[2] = normal.v[2];
    MATH_VEC_DOT(plane[3], normal, ray_pos); /* up is really new ray_pos */
    d->plane[3] = -plane[3];

    /* Triangle 1 */
    list[0].v[0] = ray_pos.v[0];
    list[0].v[1] = ray_pos.v[1];
    list[0].v[2] = ray_pos.v[2] - THICKNESS;

    list[1].v[0] = ray_pos.v[0] + LENGTH*ray_dir.v[0];
    list[1].v[1] = ray_pos.v[1] + LENGTH*ray_dir.v[1];
    list[1].v[2] = ray_pos.v[2] + LENGTH*ray_dir.v[2] - THICKNESS;

    list[2].v[0] = ray_pos.v[0] + LENGTH*ray_dir.v[0];
    list[2].v[1] = ray_pos.v[1] + LENGTH*ray_dir.v[1];
    list[2].v[2] = ray_pos.v[2] + LENGTH*ray_dir.v[2] + THICKNESS;

    /* Triangle 2 */
    list[3].v[0] = ray_pos.v[0];
    list[3].v[1] = ray_pos.v[1];
    list[3].v[2] = ray_pos.v[2] - THICKNESS;

    list[4].v[0] = ray_pos.v[0] + LENGTH*ray_dir.v[0];
    list[4].v[1] = ray_pos.v[1] + LENGTH*ray_dir.v[1];
    list[4].v[2] = ray_pos.v[2] + LENGTH*ray_dir.v[2] + THICKNESS;

    list[5].v[0] = ray_pos.v[0];
    list[5].v[1] = ray_pos.v[1];
    list[5].v[2] = ray_pos.v[2] + THICKNESS;


    tie_push(&d->tie, (TIE_3 **)&list, 2, NULL, 0);
    tie_prep(&d->tie);
}


void render_cut_free(render_t *render) {
    render_cut_t *d;

    d = (render_cut_t *)render->data;
    tie_free(&d->tie);
    bu_free(render->data, "render_cut_free");
}


static void* render_arrow_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
    return tri;
}


void* render_cut_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
    render_cut_hit_t *hit = (render_cut_hit_t *)ptr;

    hit->id = *id;
    hit->mesh = (adrt_mesh_t *)(tri->ptr);
    return hit ;
}


void render_cut_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
    render_cut_t *rd;
    render_cut_hit_t hit;
    TIE_3 color;
    tie_id_t id;
    tfloat t, dot;


    rd = (render_cut_t *)render->data;

    /* Draw Ballistic Arrow - Blue */
    if (tie_work(&rd->tie, ray, &id, render_arrow_hit, NULL)) {
	pixel->v[0] = 0.0;
	pixel->v[1] = 0.0;
	pixel->v[2] = 1.0;
	return;
    }

    /*
     * I don't think this needs to be done for every pixel?
     * Flip plane normal to face us.
     */
    t = ray->pos.v[0]*rd->plane[0] + ray->pos.v[1]*rd->plane[1] + ray->pos.v[2]*rd->plane[2] + rd->plane[3];
    hit.mod = t < 0 ? 1 : -1;


    /*
     * Optimization:
     * First intersect this ray with the plane and fire the ray from there
     * Plane: Ax + By + Cz + D = 0
     * Ray = O + td
     * t = -(Pn · R0 + D) / (Pn · Rd)
     */

    t = (rd->plane[0]*ray->pos.v[0] + rd->plane[1]*ray->pos.v[1] + rd->plane[2]*ray->pos.v[2] + rd->plane[3]) /
	(rd->plane[0]*ray->dir.v[0] + rd->plane[1]*ray->dir.v[1] + rd->plane[2]*ray->dir.v[2]);

    /* Ray never intersects plane */
    if (t > 0)
	return;

    ray->pos.v[0] += -t * ray->dir.v[0];
    ray->pos.v[1] += -t * ray->dir.v[1];
    ray->pos.v[2] += -t * ray->dir.v[2];

    hit.plane[0] = rd->plane[0];
    hit.plane[1] = rd->plane[1];
    hit.plane[2] = rd->plane[2];
    hit.plane[3] = rd->plane[3];

    /* Render Geometry */
    if (!tie_work(tie, ray, &id, render_cut_hit, &hit))
	return;

    /*
     * If the point after the splitting plane is an outhit, fill it in as if it were solid.
     * If the point after the splitting plane is an inhit, then just shade as usual.
     */

    MATH_VEC_DOT(dot, ray->dir, hit.id.norm);
    /* flip normal */
    dot = fabs(dot);


    if (hit.mesh->flags & (ADRT_MESH_SELECT|ADRT_MESH_HIT)) {
	color.v[0] = hit.mesh->flags & ADRT_MESH_HIT ? 0.9 : 0.2;
	color.v[1] = 0.2;
	color.v[2] = hit.mesh->flags & ADRT_MESH_SELECT ? 0.9 : 0.2;
    } else {
	/* Mix actual color with white 4:1, shade 50% darker */
#if 0
	MATH_VEC_SET(color, 1.0, 1.0, 1.0);
	MATH_VEC_MUL_SCALAR(color, color, 3.0);
	MATH_VEC_ADD(color, color, hit.mesh->prop->color);
	MATH_VEC_MUL_SCALAR(color, color, 0.125);
#else
	MATH_VEC_SET(color, 0.8, 0.8, 0.7);
#endif
    }

#if 0
    if (dot < 0) {
#endif
	/* Shade using inhit */
	MATH_VEC_MUL_SCALAR((*pixel), color, (dot*0.90));
#if 0
    } else {
	/* shade solid */
	MATH_VEC_SUB(vec, ray->pos, hit.id.pos);
	MATH_VEC_UNITIZE(vec);
	angle = vec.v[0]*hit.mod*-hit.plane[0] + vec.v[1]*-hit.mod*hit.plane[1] + vec.v[2]*-hit.mod*hit.plane[2];
	MATH_VEC_MUL_SCALAR((*pixel), color, (angle*0.90));
    }
#endif

    pixel->v[0] += 0.1;
    pixel->v[1] += 0.1;
    pixel->v[2] += 0.1;
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
