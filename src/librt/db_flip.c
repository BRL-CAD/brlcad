/*                   D B _ F L I P . C
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
/** @file librt/db_flip.c
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

typedef union {
    short s;
    unsigned char c[2];
} sval;


short
flip_short(short s)
{
    sval v, v2;
    v2.s = s;
    v.c[0] = v2.c[1];
    v.c[1] = v2.c[0];
    return v.s;
}


fastf_t
flip_dbfloat(dbfloat_t d)
{
    fastf_t f;
    val v, v2;
    v2.f = d;
    v.c[0] = v2.c[3];
    v.c[1] = v2.c[2];
    v.c[2] = v2.c[1];
    v.c[3] = v2.c[0];
    f = v.f;
    return f;
}


/* private */
void
flip_fastf_float(fastf_t *ff, const dbfloat_t *fp, int n, int flip)
{
    if (!flip) {
	while (n--) {
	    *ff++ = *fp++;
	    *ff++ = *fp++;
	    *ff++ = *fp++;
	    ff += ELEMENTS_PER_VECT-3;
	}
    } else {
	while (n--) {
	    *ff++ = flip_dbfloat(fp[0]);
	    *ff++ = flip_dbfloat(fp[1]);
	    *ff++ = flip_dbfloat(fp[2]);
	    fp += 3;
	}
    }
}


/**
 * R T _ F A S T F _ F L O A T
 *
 * Convert TO fastf_t FROM 3xfloats (for database)
 *
 * DEPRECATED.
 */
void
rt_fastf_float(fastf_t *ff, const dbfloat_t *fp, int n, int flip)
{
    flip_fastf_float(ff, fp, n, flip);
}


void
flip_mat_dbmat(fastf_t *ff, const dbfloat_t *dbp, int flip)
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
	*ff++ = flip_dbfloat(dbp[0]);
	*ff++ = flip_dbfloat(dbp[1]);
	*ff++ = flip_dbfloat(dbp[2]);
	*ff++ = flip_dbfloat(dbp[3]);

	*ff++ = flip_dbfloat(dbp[4]);
	*ff++ = flip_dbfloat(dbp[5]);
	*ff++ = flip_dbfloat(dbp[6]);
	*ff++ = flip_dbfloat(dbp[7]);

	*ff++ = flip_dbfloat(dbp[8]);
	*ff++ = flip_dbfloat(dbp[9]);
	*ff++ = flip_dbfloat(dbp[10]);
	*ff++ = flip_dbfloat(dbp[11]);

	*ff++ = flip_dbfloat(dbp[12]);
	*ff++ = flip_dbfloat(dbp[13]);
	*ff++ = flip_dbfloat(dbp[14]);
	*ff++ = flip_dbfloat(dbp[15]);
    }
}


/**
 * R T _ M A T _ D B M A T
 *
 * Convert TO fastf_t matrix FROM dbfloats (for database)
 *
 * DEPRECATED.
 */
void
rt_mat_dbmat(fastf_t *ff, const dbfloat_t *dbp, int flip)
{
    flip_mat_dbmat(ff, dbp, flip);
}


void
flip_dbmat_mat(dbfloat_t *dbp, const fastf_t *ff)
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


/**
 * R T _ D B M A T _ M A T
 *
 * Convert FROM fastf_t matrix TO dbfloats (for updating database)
 *
 * DEPRECATED.
 */
void
rt_dbmat_mat(dbfloat_t *dbp, const fastf_t *ff)
{
    flip_dbmat_mat(dbp, ff);
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
