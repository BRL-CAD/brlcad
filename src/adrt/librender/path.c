/*                          P A T H . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007 United States Government as represented by
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
 *  Author -
 *      Justin L. Shumaker
 *
 */

#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include "umath.h"
#include "hit.h"
#include "adrt_common.h"


void render_path_init(render_t *render, int samples) {
  render_path_t *d;

  render->work = render_path_work;
  render->free = render_path_free;
  render->data = (render_path_t *)malloc(sizeof(render_path_t));
  if (!render->data) {
      perror("render->data");
      exit(1);
  }
  d = (render_path_t *)render->data;
  d->samples = samples;
  d->inv_samples = 1.0 / samples;
}


void render_path_free(render_t *render) {
  free(render->data);
}


void render_path_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_ray_t	new_ray;
  tie_id_t	new_id;
  TIE_3		new_pix, accum, T, ref, bax, bay;
  common_mesh_t	*new_mesh;
  TFLOAT	sin_theta, cos_theta, sin_phi, cos_phi;
  int		i, n, propogate;
  render_path_t *rd;


  rd = (render_path_t *)render->data;

  accum.v[0] = accum.v[1] = accum.v[2] = 0;

  for(i = 0; i < rd->samples; i++) {
    /* Prime variables */
    new_ray = *ray;
    propogate = 1;

    /* Terminate if depth is too great. */
    while(propogate) {
      if((new_mesh = (common_mesh_t *)tie_work(tie, &new_ray, &new_id, render_hit, NULL)) && new_ray.depth < RENDER_MAX_DEPTH) {
	if(new_mesh->prop->ior != 1.0) {	/* Refractive Caustic */
	  /* Deal with refractive-fu */
	} else if(new_mesh->prop->emission > 0.0) {	/* Emitting Light Source */
	  T = new_mesh->prop->color;
	  MATH_VEC_MUL_SCALAR(T, T, new_mesh->prop->emission);
	  propogate = 0;
	} else {	/* Diffuse */
	  if(new_mesh->texture) {
	    new_mesh->texture->work(new_mesh->texture, new_mesh, &new_ray, &new_id, &T);
	  } else {
	    T = new_mesh->prop->color;
	  }
	}

	if(new_ray.depth) {
	  MATH_VEC_MUL(new_pix, new_pix, T);
	} else {
	  new_pix = T;
	}

	new_ray.depth++;

	MATH_VEC_REFLECT(ref, new_ray.dir, new_id.norm);

	new_ray.pos.v[0] = new_id.pos.v[0] + new_id.norm.v[0]*TIE_PREC;
	new_ray.pos.v[1] = new_id.pos.v[1] + new_id.norm.v[1]*TIE_PREC;
	new_ray.pos.v[2] = new_id.pos.v[2] + new_id.norm.v[2]*TIE_PREC;

	T.v[0] = new_id.norm.v[0] - new_mesh->prop->gloss*ref.v[0];
	T.v[1] = new_id.norm.v[1] - new_mesh->prop->gloss*ref.v[1];
	T.v[2] = new_id.norm.v[2] - new_mesh->prop->gloss*ref.v[2];
	MATH_VEC_UNITIZE(T);

	/* Form Basis X */
	bax.v[0] = T.v[0] || T.v[1] ? -T.v[1] : 1.0;
	bax.v[1] = T.v[0];
	bax.v[2] = 0;
	MATH_VEC_UNITIZE(bax);

	/* Form Basis Y, Simplified Cross Product of two unit vectors is a unit vector */
	bay.v[0] = -T.v[2]*bax.v[1];
	bay.v[1] = T.v[2]*bax.v[0];
	bay.v[2] = T.v[0]*bax.v[1] - T.v[1]*bax.v[0];

	cos_theta = math_rand();
	sin_theta = sqrt(cos_theta);
	cos_theta = 1-cos_theta;

	cos_phi = math_rand()*MATH_2_PI;
	sin_phi = sin(cos_phi);
	cos_phi = cos(cos_phi);

	for(n = 0; n < 3; n++) {
	  T.v[n] = sin_theta*cos_phi*bax.v[n] + sin_theta*sin_phi*bay.v[n] + cos_theta*T.v[n];
	  /* Weigh reflected vector back in */
	  new_ray.dir.v[n] = (1.0 - new_mesh->prop->gloss)*T.v[n] + new_mesh->prop->gloss * ref.v[n];
	}

	MATH_VEC_UNITIZE(new_ray.dir);
      } else {
	new_pix.v[0] = 0;
	new_pix.v[1] = 0;
	new_pix.v[2] = 0;
	propogate = 0;
      }
    }

    MATH_VEC_ADD(accum, accum, new_pix);
  }

  MATH_VEC_MUL_SCALAR((*pixel), accum, rd->inv_samples);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
