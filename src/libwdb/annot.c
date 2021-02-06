/*                        A N N O T . C
 * BRL-CAD
 *
 * Copyright (c) 2017-2021 United States Government as represented by
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

/** @file libwdb/annot.c
 *
 * Support for annotations
 *
 */

#include "common.h"

#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


int
mk_annot(struct rt_wdb *fp, const char *name, const struct rt_annot_internal *ann)
{
    struct rt_annot_internal *annot_ip;

    RT_ANNOT_CK_MAGIC(ann);

    /* copy the caller's struct */
    annot_ip = rt_copy_annot(ann);

    return wdb_export(fp, name, (void *)annot_ip, ID_ANNOT, mk_conv2mm);
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
