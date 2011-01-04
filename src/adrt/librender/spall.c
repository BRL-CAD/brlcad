/*                         S P A L L . C
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
/** @file spall.c
 *
 */

#include "render_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"

#include "adrt_struct.h"
#include "render.h"

#define TESSELATION 32
#define SPALL_LEN 20

typedef struct render_spall_s {
    TIE_3 ray_pos;
    TIE_3 ray_dir;
    tfloat plane[4];
    tfloat angle;
    tie_t tie;
} render_spall_t;

void* render_spall_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr);
void render_plane(tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

typedef struct render_spall_hit_s {
    tie_id_t id;
    adrt_mesh_t *mesh;
    tfloat plane[4];
    tfloat mod;
} render_spall_hit_t;



void
render_spall_free(render_t *render)
{
    bu_free(render->data, "render data");
}


static void *
render_arrow_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr)
{
    return tri;
}


void *
render_spall_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr)
{
    render_spall_hit_t *hit = (render_spall_hit_t *)ptr;

    hit->id = *id;
    hit->mesh = (adrt_mesh_t *)(tri->ptr);
    return hit;
}


void
render_spall_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel)
{
    render_spall_t *rd;
    render_spall_hit_t hit;
    TIE_3 color;
    tie_id_t id;
    tfloat t, dot;


    rd = (render_spall_t *)render->data;

    /* Draw spall Cone */
    if (tie_work(&rd->tie, ray, &id, render_arrow_hit, NULL)) {
	pixel->v[0] = (tfloat)0.4;
	pixel->v[1] = (tfloat)0.4;
	pixel->v[2] = (tfloat)0.4;
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
     *
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
    if (!tie_work(tie, ray, &id, render_spall_hit, &hit))
	return;


    /*
     * If the point after the splitting plane is an outhit, fill it in as if it were solid.
     * If the point after the splitting plane is an inhit, then just shade as usual.
     */

    dot = VDOT( ray->dir.v,  hit.id.norm.v);
    /* flip normal */
    dot = fabs(dot);


    if (hit.mesh->flags == 1) {
	VSET(color.v, (tfloat)0.9, (tfloat)0.2, (tfloat)0.2);
    } else {
	/* Mix actual color with white 4:1, shade 50% darker */
	VSET(color.v, (tfloat)1.0, (tfloat)1.0, (tfloat)1.0);
	VSCALE(color.v,  color.v,  (tfloat)3.0);
	VADD2(color.v,  color.v,  hit.mesh->attributes->color.v);
	VSCALE(color.v,  color.v,  (tfloat)0.125);
    }

#if 0
    if (dot < 0) {
#endif
	/* Shade using inhit */
	VSCALE(color.v,  color.v,  (dot*0.50));
	VADD2((*pixel).v,  (*pixel).v,  color.v);
#if 0
    } else {
	/* shade solid */
	VSUB2(vec.v,  ray->pos.v,  hit.id.pos.v);
	VUNITIZE(vec.v);
	angle = vec.v[0]*hit.mod*-hit.plane[0] + vec.v[1]*-hit.mod*hit.plane[1] + vec.v[2]*-hit.mod*hit.plane[2];
	VSCALE((*pixel).v,  color.v,  (angle*0.50));
    }
#endif

    pixel->v[0] += (tfloat)0.1;
    pixel->v[1] += (tfloat)0.1;
    pixel->v[2] += (tfloat)0.1;
}

int
render_spall_init(render_t *render, char *buf)
{
    render_spall_t *d;
    TIE_3 *tri_list, *vec_list, normal, up, ray_pos, ray_dir;
    tfloat plane[4], angle;
    int i;

    if(buf == NULL)
	return -1;

    render->work = render_spall_work;
    render->free = render_spall_free;

    sscanf(buf, "(%g %g %g) (%g %g %g) %g",
		    &ray_pos.v[0], &ray_pos.v[1], &ray_pos.v[2],
		    &ray_dir.v[0], &ray_dir.v[1], &ray_dir.v[2],
		    &angle);

    render->data = (render_spall_t *)bu_malloc(sizeof(render_spall_t), "render_spall_init");
    if (!render->data) {
	perror("render->data");
	exit(1);
    }
    d = (render_spall_t *)render->data;

    d->ray_pos = ray_pos;
    d->ray_dir = ray_dir;

    tie_init(&d->tie, TESSELATION, TIE_KDTREE_FAST);

    /* Calculate the normal to be used for the plane */
    up.v[0] = 0;
    up.v[1] = 0;
    up.v[2] = 1;

    VCROSS(normal.v,  ray_dir.v,  up.v);
    VUNITIZE(normal.v);

    /* Construct the plane */
    d->plane[0] = normal.v[0];
    d->plane[1] = normal.v[1];
    d->plane[2] = normal.v[2];
    plane[3] = VDOT( normal.v,  ray_pos.v); /* up is really new ray_pos */
    d->plane[3] = -plane[3];

    /******************/
    /* The spall Cone */
    /******************/
    vec_list = (TIE_3 *)bu_malloc(sizeof(TIE_3) * TESSELATION, "vec_list");
    tri_list = (TIE_3 *)bu_malloc(sizeof(TIE_3) * TESSELATION * 3, "tri_list");

    render_util_spall_vec(ray_dir, angle, TESSELATION, vec_list);

    /* triangles to approximate */
    for (i = 0; i < TESSELATION; i++) {
	tri_list[3*i+0] = ray_pos;

	VSCALE(tri_list[3*i+1].v,  vec_list[i].v,  SPALL_LEN);
	VADD2(tri_list[3*i+1].v,  tri_list[3*i+1].v,  ray_pos.v);

	if (i == TESSELATION - 1) {
	    VSCALE(tri_list[3*i+2].v,  vec_list[0].v,  SPALL_LEN);
	    VADD2(tri_list[3*i+2].v,  tri_list[3*i+2].v,  ray_pos.v);
	} else {
	    VSCALE(tri_list[3*i+2].v,  vec_list[i+1].v,  SPALL_LEN);
	    VADD2(tri_list[3*i+2].v,  tri_list[3*i+2].v,  ray_pos.v);
	}
    }

/*  tie_push(&d->tie, tri_list, TESSELATION, NULL, 0);   */
    tie_prep(&d->tie);

    bu_free(vec_list, "vec_list");
    bu_free(tri_list, "tri_list");
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
