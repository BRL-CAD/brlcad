/*                          P A T H . C
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
/** @file path.c
 *
 */

#ifndef TIE_PRECISION
# define TIE_PRECISION 0
#endif

#include "path.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "hit.h"
#include "adrt_struct.h"

#include "bu.h"
#include "bn.h"
#include "vmath.h"


/* _a is reflected ray, _b is incident ray, _c is normal */
#define MATH_VEC_REFLECT(_a, _b, _c) { \
    tfloat _d; \
    _d = VDOT( _b.v,  _c.v); \
    VSCALE(_a.v,  _c.v,  2.0*_d); \
    VSUB2(_a.v,  _b.v,  _a.v); \
    VUNITIZE(_a.v); }

void render_path_init(render_t *render, char *samples) {
    render_path_t *d;

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
}


void render_path_free(render_t *render) {
    bu_free(render->data, "render_path_free");
}


void render_path_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
    tie_ray_t new_ray;
    tie_id_t new_id;
    TIE_3 new_pix, accum, T, ref, bax, bay;
    adrt_mesh_t *new_mesh;
    tfloat sin_theta, cos_theta, sin_phi, cos_phi;
    int i, n, propogate;
    render_path_t *rd;

    VSETALL(new_pix.v, 0);

    rd = (render_path_t *)render->data;

    accum.v[0] = accum.v[1] = accum.v[2] = 0;

    for (i = 0; i < rd->samples; i++) {
	/* Prime variables */
	new_ray = *ray;
	propogate = 1;

	/* Terminate if depth is too great. */
	while (propogate) {
	    if ((new_mesh = (adrt_mesh_t *)tie_work(tie, &new_ray, &new_id, render_hit, NULL)) && new_ray.depth < RENDER_MAX_DEPTH) {
		if (new_mesh->attributes->ior != 1.0) {
		    /* Refractive Caustic */
		    /* Deal with refractive-fu */
		} else if (new_mesh->attributes->emission > 0.0) {
		    /* Emitting Light Source */
		    T = new_mesh->attributes->color;
		    VSCALE(T.v,  T.v,  new_mesh->attributes->emission);
		    propogate = 0;
		} else {
		    /* Diffuse */
		    if (new_mesh->texture) {
			new_mesh->texture->work(new_mesh->texture, new_mesh, &new_ray, &new_id, &T);
		    } else {
			T = new_mesh->attributes->color;
		    }
		}

		if (new_ray.depth) {
		    VELMUL(new_pix.v,  new_pix.v,  T.v);
		} else {
		    new_pix = T;
		}

		new_ray.depth++;

		MATH_VEC_REFLECT(ref, new_ray.dir, new_id.norm);

		new_ray.pos.v[0] = new_id.pos.v[0] + new_id.norm.v[0]*TIE_PREC;
		new_ray.pos.v[1] = new_id.pos.v[1] + new_id.norm.v[1]*TIE_PREC;
		new_ray.pos.v[2] = new_id.pos.v[2] + new_id.norm.v[2]*TIE_PREC;

		T.v[0] = new_id.norm.v[0] - new_mesh->attributes->gloss*ref.v[0];
		T.v[1] = new_id.norm.v[1] - new_mesh->attributes->gloss*ref.v[1];
		T.v[2] = new_id.norm.v[2] - new_mesh->attributes->gloss*ref.v[2];
		VUNITIZE(T.v);

		/* Form Basis X */
		bax.v[0] = T.v[0] || T.v[1] ? -T.v[1] : 1.0;
		bax.v[1] = T.v[0];
		bax.v[2] = 0;
		VUNITIZE(bax.v);

		/* Form Basis Y, Simplified Cross Product of two unit vectors is a unit vector */
		bay.v[0] = -T.v[2]*bax.v[1];
		bay.v[1] = T.v[2]*bax.v[0];
		bay.v[2] = T.v[0]*bax.v[1] - T.v[1]*bax.v[0];

		cos_theta = bn_randmt();
		sin_theta = sqrt(cos_theta);
		cos_theta = 1-cos_theta;

		cos_phi = bn_randmt() * 2 * M_PI;
		sin_phi = sin(cos_phi);
		cos_phi = cos(cos_phi);

		for (n = 0; n < 3; n++) {
		    T.v[n] = sin_theta*cos_phi*bax.v[n] + sin_theta*sin_phi*bay.v[n] + cos_theta*T.v[n];
		    /* Weigh reflected vector back in */
		    new_ray.dir.v[n] = (1.0 - new_mesh->attributes->gloss)*T.v[n] + new_mesh->attributes->gloss * ref.v[n];
		}

		VUNITIZE(new_ray.dir.v);
	    } else {
		new_pix.v[0] = 0;
		new_pix.v[1] = 0;
		new_pix.v[2] = 0;
		propogate = 0;
	    }
	}

	VADD2(accum.v,  accum.v,  new_pix.v);
    }

    VSCALE((*pixel).v,  accum.v,  rd->inv_samples);
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
