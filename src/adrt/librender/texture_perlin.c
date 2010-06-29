/*                     T E X T U R E _ P E R L I N . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2010 United States Government as represented by
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
/** @file texture_perlin.c
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
#define	AT3(rx, ry, rz) (rx*q.v[0] + ry*q.v[1] + rz*q.v[2]);
#define	LERP(t, a, b) (a+t*(b-a))


void	texture_perlin_init(texture_perlin_t *P);
void	texture_perlin_free(texture_perlin_t *P);
tfloat	texture_perlin_noise3(texture_perlin_t *P, TIE_3 V, tfloat Size, int Depth);
tfloat	texture_perlin_omega(texture_perlin_t *P, TIE_3 V);


void texture_perlin_init(texture_perlin_t *P) {
    int i, j, k;

    P->PV = (int *)bu_malloc(sizeof(int)*(2*B+2), "PV");
    P->RV = (TIE_3 *)bu_malloc(sizeof(TIE_3)*(2*B+2), "RV");

    /* Generate Random Vectors */
    for (i = 0; i < B; i++) {
	P->RV[i].v[0] = (tfloat)((PRAND % (2*B)) - B) / B;
	P->RV[i].v[1] = (tfloat)((PRAND % (2*B)) - B) / B;
	P->RV[i].v[2] = (tfloat)((PRAND % (2*B)) - B) / B;
	VUNITIZE(P->RV[i].v);
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
	P->RV[B+i] = P->RV[i];
    }
}


void texture_perlin_free(texture_perlin_t *P) {
    bu_free(P->PV, "PV");
    bu_free(P->RV, "RV");
}


tfloat texture_perlin_noise3(texture_perlin_t *P, TIE_3 V, tfloat Size, int Depth) {
    int i;
    tfloat sum;

    sum = 0;
    for (i = 0; i < Depth; i++) {
	sum += texture_perlin_omega(P, V);
	VSCALE(V.v,  V.v,  Size);
    }

    return sum;
}


tfloat texture_perlin_omega(texture_perlin_t *P, TIE_3 V) {
    TIE_3		q;
    tfloat	r0[3], r1[3], sy, sz, a, b, c, d, t, u, v;
    int		b0[3], b1[3], b00, b10, b01, b11;
    int		i, j;


    for (i = 0; i < 3; i++) {
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

    t = S_CURVE(r0[0]);
    sy = S_CURVE(r0[1]);
    sz = S_CURVE(r0[2]);

    q = P->RV[b00 + b0[2]];
    u = AT3(r0[0], r0[1], r0[2]);
    q = P->RV[b10 + b0[2]];
    v = AT3(r1[0], r0[1], r0[2]);
    a = LERP(t, u, v);

    q = P->RV[b01 + b0[2]];
    u = AT3(r0[0], r1[1], r0[2]);
    q = P->RV[b11 + b0[2]];
    v = AT3(r1[0], r1[1], r0[2]);
    b = LERP(t, u, v);

    c = LERP(sy, a, b);

    q = P->RV[b00 + b1[2]];
    u = AT3(r0[0], r0[1], r1[2]);
    q = P->RV[b10 + b1[2]];
    v = AT3(r1[0], r0[1], r1[2]);
    a = LERP(t, u, v);

    q = P->RV[b01 + b1[2]];
    u = AT3(r0[0], r1[1], r1[2]);
    q = P->RV[b11 + b1[2]];
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
