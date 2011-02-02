/*                  D B _ F L O A T . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2011 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file db_float.c
 *
 * v4 geometry database serialization routines.
 *
 */
/** @} */

#include "common.h"

#include "vmath.h"
#include "db.h"


typedef union {
    float f;
    unsigned char c[4];
} val;


/**
 * R T _ F A S T F _ F L O A T
 *
 * Convert TO fastf_t FROM 3xfloats (for database)
 */
void
rt_fastf_float(fastf_t *ff, const dbfloat_t *fp, int n, int flip)
{
    if (!flip) {
	while (n--) {
	    *ff++ = *fp++;
	    *ff++ = *fp++;
	    *ff++ = *fp++;
	    ff += ELEMENTS_PER_VECT-3;
	}

    } else {
	val v, v2;

	while (n--) {
	    v2.f = fp[0];
	    v.c[0] = v2.c[3];
	    v.c[1] = v2.c[2];
	    v.c[2] = v2.c[1];
	    v.c[3] = v2.c[0];
	    *ff++ = v.f;

	    v2.f = fp[1];
	    v.c[0] = v2.c[3];
	    v.c[1] = v2.c[2];
	    v.c[2] = v2.c[1];
	    v.c[3] = v2.c[0];
	    *ff++ = v.f;

	    v2.f = fp[2];
	    v.c[0] = v2.c[3];
	    v.c[1] = v2.c[2];
	    v.c[2] = v2.c[1];
	    v.c[3] = v2.c[0];
	    *ff++ = v.f;

	    fp += 3;
	}
    }
}


/**
 * R T _ M A T _ D B M A T
 *
 * Convert TO fastf_t matrix FROM dbfloats (for database)
 */
void
rt_mat_dbmat(fastf_t *ff, const dbfloat_t *dbp, int flip)
{
    if (!flip) {
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;

	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
	*ff++ = *dbp++;
    } else {
	val v, v2;

	v2.f = dbp[0];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[1];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[2];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[3];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[4];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[5];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[6];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[7];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[8];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[9];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[10];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[11];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[12];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[13];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[14];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;

	v2.f = dbp[15];
	v.c[0] = v2.c[3];
	v.c[1] = v2.c[2];
	v.c[2] = v2.c[1];
	v.c[3] = v2.c[0];
	*ff++ = v.f;
    }
}


/**
 * R T _ D B M A T _ M A T
 *
 * Convert FROM fastf_t matrix TO dbfloats (for updating database)
 */
void
rt_dbmat_mat(dbfloat_t *dbp, const fastf_t *ff)
{

    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;

    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;

    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;

    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;
    *dbp++ = (dbfloat_t) *ff++;
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
