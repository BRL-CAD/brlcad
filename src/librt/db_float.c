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


/**
 * R T _ F A S T F _ F L O A T
 *
 * Convert TO fastf_t FROM 3xfloats (for database)
 */
void
rt_fastf_float(fastf_t *ff, const dbfloat_t *fp, int n)
{
    while (n--) {
	*ff++ = *fp++;
	*ff++ = *fp++;
	*ff++ = *fp++;
	ff += ELEMENTS_PER_VECT-3;
    }
}


/**
 * R T _ M A T _ D B M A T
 *
 * Convert TO fastf_t matrix FROM dbfloats (for database)
 */
void
rt_mat_dbmat(fastf_t *ff, const dbfloat_t *dbp)
{

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
