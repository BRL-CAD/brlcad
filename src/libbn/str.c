/*                           S T R . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2021 United States Government as represented by
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

/** @addtogroup bnstr */
/** @{ */
/** @file libbn/str.c
 *
 * @brief
 * LIBBN string encoding/decoding routines.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/str.h"
#include "bn/str.h"


int
bn_decode_angle(double *ang, const char *str)
{
    char unit[5];
    double val;
    int ret;

    ret = sscanf(str,"%lf%4s",&val,unit);
    if (ret == 1) {
	*ang = val;
    } else if (ret == 2) {
	if (BU_STR_EQUAL(unit,"rad")) {
	    *ang = (val * RAD2DEG);
	} else if (BU_STR_EQUAL(unit,"deg")){
	    *ang = val;
	} else {
	    ret = 0;
	}
    }
    return ret;
}


int
bn_decode_mat(mat_t mat, const char *str)
{
    double m[16];
    int ret;

    if (BU_STR_EQUAL(str, "I")) {
	MAT_IDN(m);
	return 16;
    }
    if (*str == '{') str++;

    ret = sscanf(str,
		 "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
		 &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &m[6], &m[7],
		 &m[8], &m[9], &m[10], &m[11], &m[12], &m[13], &m[14], &m[15]);
    MAT_COPY(mat, m);

    return ret;
}


int
bn_decode_quat(quat_t quat, const char *str)
{
    double q[4];
    int ret;

    if (*str == '{') str++;
    ret = sscanf(str, "%lf %lf %lf %lf", &q[0], &q[1], &q[2], &q[3]);
    HMOVE(quat, q);

    return ret;
}


int
bn_decode_vect(vect_t vec, const char *str)
{
    double v[3];
    int ret;

    if (*str == '{') str++;
    ret = sscanf(str, "%lf %lf %lf", &v[0], &v[1], &v[2]);
    VMOVE(vec, v);

    return ret;
}


int
bn_decode_hvect(hvect_t v, const char *str)
{
    return bn_decode_quat(v, str);
}


void
bn_encode_mat(struct bu_vls *vp, const mat_t m, int clamp)
{
    if (m == NULL) {
	bu_vls_putc(vp, 'I');
	return;
    }

    if (clamp) {
	bu_vls_printf(vp, "%g %g %g %g  %g %g %g %g  %g %g %g %g  %g %g %g %g",
		      INTCLAMP(m[0]), INTCLAMP(m[1]), INTCLAMP(m[2]), INTCLAMP(m[3]),
		      INTCLAMP(m[4]), INTCLAMP(m[5]), INTCLAMP(m[6]), INTCLAMP(m[7]),
		      INTCLAMP(m[8]), INTCLAMP(m[9]), INTCLAMP(m[10]), INTCLAMP(m[11]),
		      INTCLAMP(m[12]), INTCLAMP(m[13]), INTCLAMP(m[14]), INTCLAMP(m[15]));
    } else {
	bu_vls_printf(vp, "%.15g %.15g %.15g %.15g  %.15g %.15g %.15g %.15g  %.15g %.15g %.15g %.15g  %.15g %.15g %.15g %.15g",
		      m[0],m[1],m[2],m[3],
		      m[4],m[5],m[6],m[7],
		      m[8],m[9],m[10],m[11],
		      m[12],m[13],m[14],m[15]);
    }
}


void
bn_encode_quat(struct bu_vls *vp, const quat_t q, int clamp)
{
    if (clamp) {
	bu_vls_printf(vp, "%g %g %g %g", V4INTCLAMPARGS(q));
    } else {
	bu_vls_printf(vp, "%.15g %.15g %.15g %.15g", V4ARGS(q));
    }
}


void
bn_encode_vect(struct bu_vls *vp, const vect_t v, int clamp)
{
    if (clamp) {
	bu_vls_printf(vp, "%g %g %g", V3INTCLAMPARGS(v));
    } else {
	bu_vls_printf(vp, "%.15g %.15g %.15g", V3ARGS(v));
    }
}


void
bn_encode_hvect(struct bu_vls *vp, const hvect_t v, int clamp)
{
    if (clamp) {
	bu_vls_printf(vp, "%g %g %g %g", V4INTCLAMPARGS(v));
    } else {
	bu_vls_printf(vp, "%.15g %.15g %.15g %.15g", V4ARGS(v));
    }
}



/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
