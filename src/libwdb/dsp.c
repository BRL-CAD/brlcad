/*                            D S P . C
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
/** @file libwdb/dsp.c
 *
 */

#include "common.h"

#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "db.h"

int
mk_dsp(struct rt_wdb *fp, const char *name, const char *file, size_t xdim, size_t ydim, const matp_t mat)
    /* name of file containing elevation data */
    /* X dimension of file (w cells) */
    /* Y dimension of file (n cells) */
    /* convert solid coords to model space */
{
    struct rt_dsp_internal *dsp;

    BU_GET(dsp, struct rt_dsp_internal);
    dsp->magic = RT_DSP_INTERNAL_MAGIC;

    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcat(&dsp->dsp_name, file);

    dsp->dsp_xcnt = xdim;
    dsp->dsp_ycnt = ydim;
    MAT_COPY(dsp->dsp_stom, mat);

    return wdb_export(fp, name, (genptr_t)dsp, ID_DSP, mk_conv2mm);
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
