/*                     T E X T U R E _ P E R L I N . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2011 United States Government as represented by
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
/** @file librender/texture_perlin.c
 *
 *  Comments -
 *      Texture Library - Perlin Utility
 *
 */

#include "texture.h"
#include <stdlib.h>

#include "bu.h"
#include "bn.h"


#define	B	0x100
#define	BM	0xff
#define	N	0x1000


#define	PRAND (int)(bn_randmt()*16384)
#define	S_CURVE(t) (t * t * (3.0 - 2.0 * t))
#define	AT3(rx, ry, rz) (rx*q[0] + ry*q[1] + rz*q[2]);
#define	LERP(t, a, b) (a+t*(b-a))


void	texture_perlin_init(struct texture_perlin_s *P);
void	texture_perlin_free(struct texture_perlin_s *P);
fastf_t	texture_perlin_noise3(struct texture_perlin_s *P, vect_t V, fastf_t Size, int Depth);
fastf_t	texture_perlin_omega(struct texture_perlin_s *P, vect_t V);

void
texture_perlin_init(struct texture_perlin_s *P) {
    int i, j, k;

    P->PV = (int *)bu_malloc(sizeof(int)*(2*B+2), "PV");
    P->RV = (vect_t *)bu_malloc(sizeof(vect_t)*(2*B+2), "RV");

    /* Generate Random Vectors */
    for (i = 0; i < B; i++) {
	P->RV[i][0] = (fastf_t)((PRAND % (2*B)) - B) / B;
	P->RV[i][1] = (fastf_t)((PRAND % (2*B)) - B) / B;
	P->RV[i][2] = (fastf_t)((PRAND % (2*B)) - B) / B;
	VUNITIZE(P->RV[i]);
	P->PV[i] = i;
    }

    /* Permute Indices into Vector List G */
    for (i = 0; i < B; i++) {
	k = P->PV[i];
	P->PV[i] = P->PV[j = PRAND % B];
	P->PV[j] = k;
    }

    for (i = 0; i < B + 2; i++) {
	P->PV[B+i] = P->PV[i];
	VMOVE(P->RV[B+i], P->RV[i]);
    }
}

void
texture_perlin_free(struct texture_perlin_s *P) {
    bu_free(P->PV, "PV");
    bu_free(P->RV, "RV");
}

fastf_t
texture_perlin_noise3(struct texture_perlin_s *P, vect_t V, fastf_t Size, int Depth) {
    int i;
    fastf_t sum;

    sum = 0;
    for (i = 0; i < Depth; i++) {
	sum += texture_perlin_omega(P, V);
	VSCALE(V,  V,  Size);
    }

    return sum;
}

fastf_t
texture_perlin_omega(struct texture_perlin_s *P, vect_t V) {
    vect_t		q;
    fastf_t	r0[3], r1[3], sy, sz, a, b, c, d, t, u, v;
    int		b0[3], b1[3], b00, b10, b01, b11;
    int		i, j;


    for (i = 0; i < 3; i++) {
	t = V[i] + N;
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

    t = S_CURVE(r0[0]);
    sy = S_CURVE(r0[1]);
    sz = S_CURVE(r0[2]);

    VMOVE(q, P->RV[b00 + b0[2]]);
    u = AT3(r0[0], r0[1], r0[2]);
    VMOVE(q, P->RV[b10 + b0[2]]);
    v = AT3(r1[0], r0[1], r0[2]);
    a = LERP(t, u, v);

    VMOVE(q, P->RV[b01 + b0[2]]);
    u = AT3(r0[0], r1[1], r0[2]);
    VMOVE(q, P->RV[b11 + b0[2]]);
    v = AT3(r1[0], r1[1], r0[2]);
    b = LERP(t, u, v);

    c = LERP(sy, a, b);

    VMOVE(q, P->RV[b00 + b1[2]]);
    u = AT3(r0[0], r0[1], r1[2]);
    VMOVE(q, P->RV[b10 + b1[2]]);
    v = AT3(r1[0], r0[1], r1[2]);
    a = LERP(t, u, v);

    VMOVE(q, P->RV[b01 + b1[2]]);
    u = AT3(r0[0], r1[1], r1[2]);
    VMOVE(q, P->RV[b11 + b1[2]]);
    v = AT3(r1[0], r1[1], r1[2]);
    b = LERP(t, u, v);

    d = LERP(sy, a, b);

    return LERP(sz, c, d);
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
