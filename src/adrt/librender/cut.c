/*                        C U T . C
 * BRL-CAD / ADRT
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
/** @file cut.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"

#ifndef TIE_PRECISION
# define TIE_PRECISION 0
#endif

#include "adrt.h"
#include "adrt_struct.h"
#include "render.h"

void* render_cut_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr);
void render_cut(tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

typedef struct render_cut_s {
    TIE_3 ray_pos;
    TIE_3 ray_dir;
    tfloat plane[4];
    tie_t tie;
} render_cut_t;

typedef struct render_cut_hit_s {
    tie_id_t id;
    adrt_mesh_t *mesh;
    tfloat plane[4];
    tfloat mod;
} render_cut_hit_t;

void *
render_cut_hit_cutline(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr)
{
    ((adrt_mesh_t *)(tri->ptr))->flags |= ADRT_MESH_HIT;
    *((double *)ptr) = id->dist;
    return ptr;
}

extern tie_t *tie;

void
render_cut_free(render_t *render)
{
    render_cut_t *d;

    d = (render_cut_t *)render->data;
    tie_free(&d->tie);
    bu_free(render->data, "render_cut_free");
}


static void *
render_arrow_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr)
{
    return tri;
}


void *
render_cut_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr)
{
    render_cut_hit_t *hit = (render_cut_hit_t *)ptr;

    hit->id = *id;
    hit->mesh = (adrt_mesh_t *)(tri->ptr);
    return hit ;
}


void
render_cut_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel)
{
    render_cut_t *rd;
    render_cut_hit_t hit;
    TIE_3 color;
    tie_id_t id;
    tfloat t, dot;

    rd = (render_cut_t *)render->data;

    /* Draw Arrow - Blue */
    if (tie_work(&rd->tie, ray, &id, render_arrow_hit, NULL)) {
	VSET(pixel->v, 0.0, 0.0, 1.0);
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
     * t = -(Pn � R0 + D) / (Pn � Rd)
     */
    t = (rd->plane[0]*ray->pos.v[0] + rd->plane[1]*ray->pos.v[1] + rd->plane[2]*ray->pos.v[2] + rd->plane[3]) /
	(rd->plane[0]*ray->dir.v[0] + rd->plane[1]*ray->dir.v[1] + rd->plane[2]*ray->dir.v[2]);

    /* Ray never intersects plane */
    if (t > 0)
	return;

    ray->pos.v[0] += -t * ray->dir.v[0];
    ray->pos.v[1] += -t * ray->dir.v[1];
    ray->pos.v[2] += -t * ray->dir.v[2];
    HMOVE(hit.plane, rd->plane);

    /* Render Geometry */
    if (!tie_work(tie, ray, &id, render_cut_hit, &hit))
	return;

    /*
     * If the point after the splitting plane is an outhit, fill it in as if it were solid.
     * If the point after the splitting plane is an inhit, then just shade as usual.
     */

    /* flipped normal */
    dot = fabs(VDOT( ray->dir.v,  hit.id.norm.v));

    if (hit.mesh->flags & (ADRT_MESH_SELECT|ADRT_MESH_HIT)) {
	VSET(color.v, hit.mesh->flags & ADRT_MESH_HIT ? 0.9 : 0.2, 0.2, hit.mesh->flags & ADRT_MESH_SELECT ? 0.9 : 0.2);
    } else {
	/* Mix actual color with white 4:1, shade 50% darker */
#if 0
	VSET(color.v, 1.0, 1.0, 1.0);
	VSCALE(color.v,  color.v,  3.0);
	VADD2(color.v,  color.v,  hit.mesh->attributes->color.v);
	VSCALE(color.v,  color.v,  0.125);
#else
	VSET(color.v, 0.8, 0.8, 0.7);
#endif
    }

#if 0
    if (dot < 0) {
#endif
	/* Shade using inhit */
	VSCALE((*pixel).v,  color.v,  (dot*0.90));
#if 0
    } else {
	TIE_3 vec;
	fastf_t angle;
	/* shade solid */
	VSUB2(vec.v,  ray->pos.v,  hit.id.pos.v);
	VUNITIZE(vec.v);
	angle = vec.v[0]*hit.mod*-hit.plane[0] + vec.v[1]*-hit.mod*hit.plane[1] + vec.v[2]*-hit.mod*hit.plane[2];
	VSCALE((*pixel).v,  color.v,  (angle*0.90));
    }
#endif

    pixel->v[0] += 0.1;
    pixel->v[1] += 0.1;
    pixel->v[2] += 0.1;
}

int
render_cut_init(render_t *render, char *buf)
{
    int i;
    render_cut_t *d;
    static TIE_3 list[6];
    TIE_3 **tlist, up, ray_pos, ray_dir;
    fastf_t shot_len = 100, shot_width = .02;
    tie_id_t id;
    tie_ray_t ray;
    double step;

    if(buf == NULL)
	    return -1;

    sscanf(buf, "#(%f %f %f) #(%f %f %f)",
	    ray_pos.v, ray_pos.v+1, ray_pos.v+2,
	    ray_dir.v, ray_dir.v+1, ray_dir.v+2);

#if 0
    /*
     * fire through the entire geometry, marking each intersected mesh with
     * ADRT_MESH_HIT
     */
    VMOVE(ray.pos.v, ray_pos.v);
    VMOVE(ray.dir.v, ray_dir.v);
    ray.depth = 0;
    while(tie_work(render->tie, &ray, &id, render_cut_hit_cutline, &step))
	VJOIN1( ray.pos.v, ray.pos.v, step + SMALL_FASTF, ray.dir.v );
#endif

    /* prepare cut stuff */
    tlist = (TIE_3 **)bu_malloc(sizeof(TIE_3 *) * 6, "cutting plane triangles");

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

    /* Calculate the normal to be used for the plane */
    VSET(up.v, 0, 0, 1);
    VCROSS(d->plane,  ray_dir.v,  up.v);
    VUNITIZE(d->plane);

    /* Construct the plane */
    d->plane[3] = -VDOT( d->plane,  ray_pos.v); /* up is really new ray_pos */

    /* generate the shtuff for the blue line */
    tie_init(&d->tie, 2, TIE_KDTREE_FAST);

    /* Triangle 1 */
    VMOVE(list[0].v, ray_pos.v);
    list[0].v[2] -= shot_width;

    list[0].v[0] = ray_pos.v[0];
    list[0].v[1] = ray_pos.v[1];
    list[0].v[2] = ray_pos.v[2] - shot_width;

    list[1].v[0] = ray_pos.v[0] + shot_len*ray_dir.v[0];
    list[1].v[1] = ray_pos.v[1] + shot_len*ray_dir.v[1];
    list[1].v[2] = ray_pos.v[2] + shot_len*ray_dir.v[2] - shot_width;

    list[2].v[0] = ray_pos.v[0] + shot_len*ray_dir.v[0];
    list[2].v[1] = ray_pos.v[1] + shot_len*ray_dir.v[1];
    list[2].v[2] = ray_pos.v[2] + shot_len*ray_dir.v[2] + shot_width;

    /* Triangle 2 */
    VMOVE(list[3].v, ray_pos.v);
    list[3].v[2] -= shot_width;

    list[4].v[0] = ray_pos.v[0] + shot_len*ray_dir.v[0];
    list[4].v[1] = ray_pos.v[1] + shot_len*ray_dir.v[1];
    list[4].v[2] = ray_pos.v[2] + shot_len*ray_dir.v[2] + shot_width;

    VMOVE(list[5].v, ray_pos.v);
    list[5].v[2] += shot_width;

    for(i=0;i<6;i++)
	tlist[i] = &list[i];

    tie_push(&d->tie, tlist, 2, NULL, 0);

    tie_prep(&d->tie);
    bu_free(tlist, "cutting plane triangles");
    return 0;
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
