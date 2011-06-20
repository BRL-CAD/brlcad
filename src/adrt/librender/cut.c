/*                        C U T . C
 * BRL-CAD / ADRT
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
/** @file librender/cut.c
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

extern struct tie_s *tie;

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
     * t = -(Pn · R0 + D) / (Pn · Rd)
     */
    t = (rd->plane[0]*ray->pos[0] + rd->plane[1]*ray->pos[1] + rd->plane[2]*ray->pos[2] + rd->plane[3]) /
	(rd->plane[0]*ray->dir[0] + rd->plane[1]*ray->dir[1] + rd->plane[2]*ray->dir[2]);

    /* Ray never intersects plane */
    if (t > 0)
	return;

    ray->pos[0] += -t * ray->dir[0];
    ray->pos[1] += -t * ray->dir[1];
    ray->pos[2] += -t * ray->dir[2];
    HMOVE(hit.plane, rd->plane);

    /* Render Geometry */
    if (!tie_work(tiep, ray, &id, render_cut_hit, &hit))
	return;

    /*
     * If the point after the splitting plane is an outhit, fill it in as if it were solid.
     * If the point after the splitting plane is an inhit, then just shade as usual.
     */

    /* flipped normal */
    dot = fabs(VDOT( ray->dir,  hit.id.norm));

    if (hit.mesh->flags & (ADRT_MESH_SELECT|ADRT_MESH_HIT)) {
	VSET(color, hit.mesh->flags & ADRT_MESH_HIT ? (tfloat)0.9 : (tfloat)0.2, (tfloat)0.2, hit.mesh->flags & ADRT_MESH_SELECT ? (tfloat)0.9 : (tfloat)0.2);
    } else {
	/* Mix actual color with white 4:1, shade 50% darker */
#if 0
	VSET(color, 1.0, 1.0, 1.0);
	VSCALE(color,  color,  3.0);
	VADD2(color,  color,  hit.mesh->attributes->color);
	VSCALE(color,  color,  0.125);
#else
	VSET(color, (tfloat)0.8, (tfloat)0.8, (tfloat)0.7);
#endif
    }

#if 0
    if (dot < 0) {
#endif
	/* Shade using inhit */
	VSCALE((*pixel),  color,  (dot*0.90));
#if 0
    } else {
	TIE_3 vec;
	fastf_t angle;
	/* shade solid */
	VSUB2(vec,  ray->pos,  hit.id.pos);
	VUNITIZE(vec);
	angle = vec[0]*hit.mod*-hit.plane[0] + vec[1]*-hit.mod*hit.plane[1] + vec[2]*-hit.mod*hit.plane[2];
	VSCALE((*pixel),  color,  (angle*0.90));
    }
#endif

    *pixel[0] += (tfloat)0.1;
    *pixel[1] += (tfloat)0.1;
    *pixel[2] += (tfloat)0.1;
}

int
render_cut_init(render_t *render, const char *buf)
{
    int i;
    render_cut_t *d;
    static TIE_3 list[6];
	TIE_3 **tlist;
    vect_t up, ray_pos, ray_dir;
    fastf_t shot_len = 100, shot_width = .02;
    struct tie_id_s id;
    struct tie_ray_s ray;
    double step, f[6];

    if(buf == NULL)
	    return -1;

    sscanf(buf, "#(%lf %lf %lf) #(%lf %lf %lf)",
	    f, f+1, f+2,
	    f+3, f+3+1, f+3+2);
	VMOVE(ray_pos, f);
	VMOVE(ray_dir, f);
    VUNITIZE(ray_dir);

    shot_width = 0.01 * render->tie->radius;
    {
	vect_t v;

	VSUB2(v, ray_pos, render->tie->mid);
	shot_len = 2.0 * render->tie->radius + MAGNITUDE(v) - render->tie->radius;;
    }

    /*
     * fire through the entire geometry, marking each intersected mesh with
     * ADRT_MESH_HIT
     */
    VMOVE(ray.pos, ray_pos);
    VMOVE(ray.dir, ray_dir);
    ray.depth = 0;
    tie_work(render->tie, &ray, &id, render_cut_hit_cutline, &step);

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

    VMOVE(d->ray_pos, ray_pos);
    VMOVE(d->ray_dir, ray_dir);

    /* Calculate the normal to be used for the plane */
    VSET(up, 0, 0, 1);
    VCROSS(d->plane,  ray_dir,  up);
    VUNITIZE(d->plane);

    /* Construct the plane */
    d->plane[3] = -VDOT( d->plane,  ray_pos); /* up is really new ray_pos */

    /* generate the shtuff for the blue line */
    tie_init(&d->tie, 2, TIE_KDTREE_FAST);

    /* Triangle 1 */
    VSET(list[0].v, ray_pos[0], ray_pos[1], ray_pos[2] - shot_width);
    VSET(list[1].v, ray_pos[0] + shot_len*ray_dir[0], ray_pos[1] + shot_len*ray_dir[1], ray_pos[2] + shot_len*ray_dir[2] - shot_width);
    VSET(list[2].v, ray_pos[0] + shot_len*ray_dir[0], ray_pos[1] + shot_len*ray_dir[1], ray_pos[2] + shot_len*ray_dir[2] + shot_width);

    /* Triangle 2 */
    VMOVE(list[3].v, ray_pos);
    list[3].v[2] -= shot_width;

    VSET(list[4].v, ray_pos[0] + shot_len*ray_dir[0], ray_pos[1] + shot_len*ray_dir[1], ray_pos[2] + shot_len*ray_dir[2] + shot_width);

    VMOVE(list[5].v, ray_pos);
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
