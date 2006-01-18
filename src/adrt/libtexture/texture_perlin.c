/*                     T E X T U R E _ P E R L I N . C
 *
 * @file texture_perlin.c
 *
 * BRL-CAD
 *
 * Copyright (c) 2002-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 *  Comments -
 *      Texture Library - Perlin Utility
 *
 *  Author -
 *      Kenneth Perlin
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#include "texture_perlin.h"
#include <stdlib.h>
#include "umath.h"


#define	B	0x100
#define	BM	0xff
#define	N	0x1000


#define	prand (int)(math_rand()*16384)
#define	s_curve(t) (t * t * (3.0 - 2.0 * t))
#define	at3(rx,ry,rz) (rx*q.v[0] + ry*q.v[1] + rz*q.v[2]);
#define	lerp(t, a, b) (a+t*(b-a))


void	texture_perlin_init(texture_perlin_t *P);
void	texture_perlin_free(texture_perlin_t *P);
tfloat	texture_perlin_noise3(texture_perlin_t *P, TIE_3 V, tfloat Size, int Depth);
tfloat	texture_perlin_omega(texture_perlin_t *P, TIE_3 V);


void texture_perlin_init(texture_perlin_t *P) {
  int i, j, k;

  P->PV = (int *)malloc(sizeof(int)*(2*B+2));
  P->RV = (TIE_3 *)malloc(sizeof(TIE_3)*(2*B+2));

  /* Generate Random Vectors */
  for (i = 0; i < B; i++) {
    P->RV[i].v[0] = (tfloat)((prand % (2*B)) - B) / B;
    P->RV[i].v[1] = (tfloat)((prand % (2*B)) - B) / B;
    P->RV[i].v[2] = (tfloat)((prand % (2*B)) - B) / B;
    math_vec_unitize(P->RV[i]);
    P->PV[i] = i;
  }

  /* Permute Indices into Vector List G */
  for(i = 0; i < B; i++) {
    k = P->PV[i];
    P->PV[i] = P->PV[j = prand % B];
    P->PV[j] = k;
  }

  for(i = 0; i < B + 2; i++) {
    P->PV[B+i] = P->PV[i];
    P->RV[B+i] = P->RV[i];
  }
}


void texture_perlin_free(texture_perlin_t *P) {
  free(P->PV);
  free(P->RV);
}


tfloat texture_perlin_noise3(texture_perlin_t *P, TIE_3 V, tfloat Size, int Depth) {
  int i;
  tfloat sum;

  sum = 0;
  for(i = 0; i < Depth; i++) {
    sum += texture_perlin_omega(P, V);
    math_vec_mul_scalar(V, V, Size);
  }

  return(sum);
}


tfloat texture_perlin_omega(texture_perlin_t *P, TIE_3 V) {
  TIE_3		q;
  tfloat	r0[3], r1[3], sy, sz, a, b, c, d, t, u, v;
  int		b0[3], b1[3], b00, b10, b01, b11;
  int		i, j;


  for(i = 0; i < 3; i++) {
    t = V.v[i] + N;
    b0[i] = ((int)t) & BM;
    b1[i] = (b0[i]+1) & BM;
    r0[i] = t - (int)t;
    r1[i] = r0[i] - 1.0;
  }

  i = P->PV[b0[0]];
  j = P->PV[b1[0]];

  b00 = P->PV[i + b0[1]];
  b10 = P->PV[j + b0[1]];
  b01 = P->PV[i + b1[1]];
  b11 = P->PV[j + b1[1]];

  t = s_curve(r0[0]);
  sy = s_curve(r0[1]);
  sz = s_curve(r0[2]);

  q = P->RV[b00 + b0[2]];
  u = at3(r0[0], r0[1], r0[2]);
  q = P->RV[b10 + b0[2]];
  v = at3(r1[0], r0[1], r0[2]);
  a = lerp(t, u, v);

  q = P->RV[b01 + b0[2]];
  u = at3(r0[0], r1[1], r0[2]);
  q = P->RV[b11 + b0[2]];
  v = at3(r1[0], r1[1], r0[2]);
  b = lerp(t, u, v);

  c = lerp(sy, a, b);

  q = P->RV[b00 + b1[2]];
  u = at3(r0[0], r0[1], r1[2]);
  q = P->RV[b10 + b1[2]];
  v = at3(r1[0], r0[1], r1[2]);
  a = lerp(t, u, v);

  q = P->RV[b01 + b1[2]];
  u = at3(r0[0], r1[1], r1[2]);
  q = P->RV[b11 + b1[2]];
  v = at3(r1[0], r1[1], r1[2]);
  b = lerp(t, u, v);

  d = lerp(sy, a, b);

  return( lerp(sz, c, d) );
}
