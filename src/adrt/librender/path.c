/*                          P A T H . C
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
/** @file librender/path.c
 *
 */

#ifndef TIE_PRECISION
# define TIE_PRECISION 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "adrt_struct.h"

#include "bu.h"
#include "bn.h"
#include "vmath.h"

typedef struct render_path_s {
    int samples;
    tfloat inv_samples;
} render_path_t;


/* _a is reflected ray, _b is incident ray, _c is normal */
#define MATH_VEC_REFLECT(_a, _b, _c) { \
    fastf_t _d; \
    _d = VDOT( _b,  _c); \
    VSCALE(_a,  _c,  2.0*_d); \
    VSUB2(_a,  _b,  _a); \
    VUNITIZE(_a); }


void
render_path_free(render_t *render)
{
    bu_free(render->data, "render_path_free");
}


void
render_path_work(render_t *render, struct tie_s *tie, struct tie_ray_s *ray, vect_t *pixel)
{
    struct tie_ray_s new_ray;
    struct tie_id_s new_id;
    vect_t new_pix, accum, T, ref, bax, bay;
    adrt_mesh_t *new_mesh;
    tfloat sin_theta, cos_theta, sin_phi, cos_phi;
    int i, n, propogate;
    render_path_t *rd;

    VSETALL(new_pix, 0);

    rd = (render_path_t *)render->data;

    accum[0] = accum[1] = accum[2] = 0;

    for (i = 0; i < rd->samples; i++) {
	/* Prime variables */
	new_ray = *ray;
	propogate = 1;

	/* Terminate if depth is too great. */
	while (propogate) {
	    if ((new_mesh = (adrt_mesh_t *)tie_work(tie, &new_ray, &new_id, render_hit, NULL)) && new_ray.depth < RENDER_MAX_DEPTH) {
		if (!EQUAL(new_mesh->attributes->ior, 1.0)) {
		    /* Refractive Caustic */
		    /* Deal with refractive-fu */
		} else if (new_mesh->attributes->emission > 0.0) {
		    /* Emitting Light Source */
		    VMOVE(T, new_mesh->attributes->color.v);
		    VSCALE(T,  T,  new_mesh->attributes->emission);
		    propogate = 0;
		} else {
		    /* Diffuse */
		    if (new_mesh->texture) {
			new_mesh->texture->work(new_mesh->texture, new_mesh, &new_ray, &new_id, &T);
		    } else {
			VMOVE(T, new_mesh->attributes->color.v);
		    }
		}

		if (new_ray.depth) {
		    VELMUL(new_pix,  new_pix,  T);
		} else {
		    VMOVE(new_pix, T);
		}

		new_ray.depth++;

		MATH_VEC_REFLECT(ref, new_ray.dir, new_id.norm);

		new_ray.pos[0] = new_id.pos[0] + new_id.norm[0]*TIE_PREC;
		new_ray.pos[1] = new_id.pos[1] + new_id.norm[1]*TIE_PREC;
		new_ray.pos[2] = new_id.pos[2] + new_id.norm[2]*TIE_PREC;

		T[0] = new_id.norm[0] - new_mesh->attributes->gloss*ref[0];
		T[1] = new_id.norm[1] - new_mesh->attributes->gloss*ref[1];
		T[2] = new_id.norm[2] - new_mesh->attributes->gloss*ref[2];
		VUNITIZE(T);

		/* Form Basis X */
		bax[0] = ZERO(T[0]) || ZERO(T[1]) ? -T[1] : 1.0;
		bax[1] = T[0];
		bax[2] = 0;
		VUNITIZE(bax);

		/* Form Basis Y, Simplified Cross Product of two unit vectors is a unit vector */
		bay[0] = -T[2]*bax[1];
		bay[1] = T[2]*bax[0];
		bay[2] = T[0]*bax[1] - T[1]*bax[0];

		cos_theta = bn_randmt();
		sin_theta = sqrt(cos_theta);
		cos_theta = 1-cos_theta;

		cos_phi = bn_randmt() * 2 * M_PI;
		sin_phi = sin(cos_phi);
		cos_phi = cos(cos_phi);

		for (n = 0; n < 3; n++) {
		    T[n] = sin_theta*cos_phi*bax[n] + sin_theta*sin_phi*bay[n] + cos_theta*T[n];
		    /* Weigh reflected vector back in */
		    new_ray.dir[n] = (1.0 - new_mesh->attributes->gloss)*T[n] + new_mesh->attributes->gloss * ref[n];
		}

		VUNITIZE(new_ray.dir);
	    } else {
		new_pix[0] = 0;
		new_pix[1] = 0;
		new_pix[2] = 0;
		propogate = 0;
	    }
	}

	VADD2(accum,  accum,  new_pix);
    }

    VSCALE(*pixel,  accum,  rd->inv_samples);
}

int
render_path_init(render_t *render, const char *samples)
{
    render_path_t *d;

    if(samples == NULL)
	return -1;

    render->work = render_path_work;
    render->free = render_path_free;
    render->data = (render_path_t *)bu_malloc(sizeof(render_path_t), "render_path_init");
    if (!render->data) {
	perror("render->data");
	exit(1);
    }
    d = (render_path_t *)render->data;
    d->samples = atoi(samples);	/* TODO: make this more robust */
    d->inv_samples = 1.0 / d->samples;
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
