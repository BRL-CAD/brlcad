/*                            D S P . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file libwdb/dsp.c
 *
 */

#include "common.h"

#include "bio.h"

#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"
#include "rt/db4.h"

int
mk_dsp(struct rt_wdb *fp, const char *name, const char *file, size_t xdim, size_t ydim, const matp_t mat)
    /* name of file containing elevation data */
    /* X dimension of file (w cells) */
    /* Y dimension of file (n cells) */
    /* convert solid coords to model space */
{
    struct rt_dsp_internal *dsp;

    BU_ALLOC(dsp, struct rt_dsp_internal);
    dsp->magic = RT_DSP_INTERNAL_MAGIC;

    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcat(&dsp->dsp_name, file);

    dsp->dsp_xcnt = xdim;
    dsp->dsp_ycnt = ydim;
    MAT_COPY(dsp->dsp_stom, mat);

    return wdb_export(fp, name, (void *)dsp, ID_DSP, mk_conv2mm);
}


int
mk_dsp_obj(struct rt_wdb *fp, const char *name, const char *binunif, size_t xcnt, size_t ycnt, const matp_t stom)
    /* name of database BINUNIF object containing elevation data */
    /* X dimension of data (w cells) */
    /* Y dimension of data (n cells) */
    /* convert solid coords to model space */
{
    struct rt_dsp_internal *dsp;

    BU_ALLOC(dsp, struct rt_dsp_internal);
    dsp->magic = RT_DSP_INTERNAL_MAGIC;

    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcat(&dsp->dsp_name, binunif);

    dsp->dsp_xcnt = xcnt;
    dsp->dsp_ycnt = ycnt;
    dsp->dsp_datasrc = RT_DSP_SRC_OBJ;
    MAT_COPY(dsp->dsp_stom, stom);

    return wdb_export(fp, name, (void *)dsp, ID_DSP, mk_conv2mm);
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
