/*                         S P A L L . C
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
/** @file librender/spall.c
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
     * t = -(Pn · R0 + D) / (Pn · Rd)
     *
     */

    t = (rd->plane[0]*ray->pos[0] + rd->plane[1]*ray->pos[1] + rd->plane[2]*ray->pos[2] + rd->plane[3]) /
	(rd->plane[0]*ray->dir[0] + rd->plane[1]*ray->dir[1] + rd->plane[2]*ray->dir[2]);

    /* Ray never intersects plane */
    if (t > 0)
	return;

    ray->pos[0] += -t * ray->dir[0];
    ray->pos[1] += -t * ray->dir[1];
    ray->pos[2] += -t * ray->dir[2];

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

    dot = VDOT( ray->dir,  hit.id.norm);
    /* flip normal */
    dot = fabs(dot);


    if (hit.mesh->flags == 1) {
	VSET(color, 0.9, 0.2, 0.2);
    } else {
	/* Mix actual color with white 4:1, shade 50% darker */
	VSET(color, 1.0, 1.0, 1.0);
	VSCALE(color,  color,  3.0);
	VADD2(color,  color,  hit.mesh->attributes->color.v);
	VSCALE(color,  color,  0.125);
    }

#if 0
    if (dot < 0) {
#endif
	/* Shade using inhit */
	VSCALE(color,  color,  (dot*0.50));
	VADD2(*pixel,  *pixel,  color);
#if 0
    } else {
	/* shade solid */
	VSUB2(vec,  ray->pos,  hit.id.pos);
	VUNITIZE(vec);
	angle = vec[0]*hit.mod*-hit.plane[0] + vec[1]*-hit.mod*hit.plane[1] + vec[2]*-hit.mod*hit.plane[2];
	VSCALE((*pixel),  color,  (angle*0.50));
    }
#endif

    *pixel[0] += (tfloat)0.1;
    *pixel[1] += (tfloat)0.1;
    *pixel[2] += (tfloat)0.1;
}

int
render_spall_init(render_t *render, const char *buf)
{
    struct render_spall_s *d;
    vect_t *tri_list, *vec_list, normal, up, ray_pos, ray_dir;
    fastf_t plane[4], angle;
    int i;

    if(buf == NULL)
	return -1;

    render->work = render_spall_work;
    render->free = render_spall_free;

    sscanf(buf, "(%lg %lg %lg) (%lg %lg %lg) %lg",
		    &ray_pos[0], &ray_pos[1], &ray_pos[2],
		    &ray_dir[0], &ray_dir[1], &ray_dir[2],
		    &angle);

    render->data = (struct render_spall_s *)bu_malloc(sizeof(struct render_spall_s), "render_spall_init");
    if (!render->data) {
	perror("render->data");
	exit(1);
    }
    d = (struct render_spall_s *)render->data;

    VMOVE(d->ray_pos, ray_pos);
    VMOVE(d->ray_dir, ray_dir);

    tie_init(&d->tie, TESSELATION, TIE_KDTREE_FAST);

    /* Calculate the normal to be used for the plane */
    up[0] = 0;
    up[1] = 0;
    up[2] = 1;

    VCROSS(normal,  ray_dir,  up);
    VUNITIZE(normal);

    /* Construct the plane */
    d->plane[0] = normal[0];
    d->plane[1] = normal[1];
    d->plane[2] = normal[2];
    plane[3] = VDOT( normal,  ray_pos); /* up is really new ray_pos */
    d->plane[3] = -plane[3];

    /******************/
    /* The spall Cone */
    /******************/
    vec_list = (vect_t *)bu_malloc(sizeof(vect_t) * TESSELATION, "vec_list");
    tri_list = (vect_t *)bu_malloc(sizeof(vect_t) * TESSELATION * 3, "tri_list");

    render_util_spall_vec(ray_dir, angle, TESSELATION, vec_list);

    /* triangles to approximate */
    for (i = 0; i < TESSELATION; i++) {
	VMOVE(tri_list[3*i+0], ray_pos);

	VSCALE(tri_list[3*i+1],  vec_list[i],  SPALL_LEN);
	VADD2(tri_list[3*i+1],  tri_list[3*i+1],  ray_pos);

	if (i == TESSELATION - 1) {
	    VSCALE(tri_list[3*i+2],  vec_list[0],  SPALL_LEN);
	    VADD2(tri_list[3*i+2],  tri_list[3*i+2],  ray_pos);
	} else {
	    VSCALE(tri_list[3*i+2],  vec_list[i+1],  SPALL_LEN);
	    VADD2(tri_list[3*i+2],  tri_list[3*i+2],  ray_pos);
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
