/*                       S U B M O D E L . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2016 United States Government as represented by
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

#include "common.h"

#include "bio.h"

#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"
#include "rt/db4.h"


int
mk_submodel(struct rt_wdb *fp, const char *name, const char *file, const char *treetop, int meth)
{
    struct rt_submodel_internal *in;

    BU_ALLOC(in, struct rt_submodel_internal);
    in->magic = RT_SUBMODEL_INTERNAL_MAGIC;
    bu_vls_init(&in->file);
    if (file) bu_vls_strcpy(&in->file, file);
    bu_vls_init(&in->treetop);
    bu_vls_strcpy(&in->treetop, treetop);
    in->meth = meth;

    return wdb_export(fp, name, (void *)in, ID_SUBMODEL, mk_conv2mm);
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
