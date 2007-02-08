/*                       P T C L O U D . C
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
/** @file ptcloud.c
 *
 * Author -
 *  Justin Shumaker
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>


typedef	struct TIE_3_t {
  float	v[3];
} TIE_3;


#define UNITIZE(_a) { \
	float _b = sqrt(_a.v[0]*_a.v[0] + _a.v[1]*_a.v[1] + _a.v[2]*_a.v[2]); \
	_a.v[0] /= _b; _a.v[1] /= _b; _a.v[2] /= _b; }


int main(int argc, char *args[]) {
  float		sin_theta, cos_theta, sin_phi, cos_phi, foo;
  int		i, j, k;
  TIE_3		T, n, bax, bay;


  n.v[0] = 0;
  n.v[1] = 0;
  n.v[2] = 1.0;

  bax.v[0] = n.v[0] || n.v[1] ? -n.v[1] : 1.0;
  bax.v[1] = n.v[0];
  bax.v[2] = 0;

  bay.v[0] = -n.v[2]*bax.v[1];
  bay.v[1] = n.v[2]*bax.v[0];
  bay.v[2] = n.v[0]*bax.v[1] - n.v[1]*bax.v[0];


  if(argc < 3) {
    printf("[Options] type unitize\n");
    exit(1);
  }


  for(j = 1; j <= atoi(args[1]); j++) {
    for(k = 1; k <= atoi(args[1]); k++) {
    switch(atoi(args[2])) {
      case 1:
	foo = drand48();
	sin_theta = sqrt(1-foo*foo);
	cos_theta = 1-sin_theta;
	break;

      case 2:
	foo = drand48();
	sin_theta = sqrt(foo);
	cos_theta = sin_theta;
	break;

      case 3:
	foo = drand48();
	sin_theta = sqrt(foo);
	cos_theta = 1-sin_theta;
	break;

      case 4:
	cos_theta = (((float)j / atof(args[1])) + (2.0 * drand48() - 1.0) / (2.0*atof(args[1]))) * 1.57;
	sin_theta = sin(cos_theta);
	cos_theta = cos(cos_theta);
	break;

      default:
	exit(0);
	break;
    }

    cos_phi = (((float)k / atof(args[1])) + (2.0 * drand48() - 1.0) / (2.0*atof(args[1]))) * 2.0*M_PI;
    sin_phi = sin(cos_phi);
    cos_phi = cos(cos_phi);

    for(i = 0; i < 3; i++)
      T.v[i] = sin_theta*cos_phi*bax.v[i] + sin_theta*sin_phi*bay.v[i] + cos_theta*n.v[i];
    if(atoi(args[3]))
      UNITIZE(T);
    printf("%f %f %f\n", T.v[0], T.v[1], T.v[2]);
  }
  }

  return(0);
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
