/*                            E B M . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @file libwdb/ebm.c
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "db.h"


/*
 * M K _ E B M
 */
int
mk_ebm(struct rt_wdb *fp, const char *name, const char *file, size_t xdim, size_t ydim, fastf_t tallness, const matp_t mat)
    /* name of file containing bitmap */
    /* X dimansion of file (w cells) */
    /* Y dimension of file (n cells) */
    /* Z extrusion height (mm) */
    /* convert local coords to model space */
{
    struct rt_ebm_internal *ebm;

    BU_GET(ebm, struct rt_ebm_internal);
    ebm->magic = RT_EBM_INTERNAL_MAGIC;
    bu_strlcpy(ebm->file, file, RT_EBM_NAME_LEN);
    ebm->xdim = xdim;
    ebm->ydim = ydim;
    ebm->tallness = tallness;
    MAT_COPY(ebm->mat, mat);

    return wdb_export(fp, name, (genptr_t)ebm, ID_EBM, mk_conv2mm);
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
