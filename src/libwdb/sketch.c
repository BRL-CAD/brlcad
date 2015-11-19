/*                        S K E T C H . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2014 United States Government as represented by
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

/** @file libwdb/sketch.c
 *
 * Support for sketches
 *
 */

#include "common.h"

#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


int
mk_sketch(struct rt_wdb *fp, const char *name, const struct rt_sketch_internal *skt)
{
    struct rt_sketch_internal *sketch;

    RT_SKETCH_CK_MAGIC(skt);

    /* copy the caller's struct */
    sketch = rt_copy_sketch(skt);

    return wdb_export(fp, name, (void *)sketch, ID_SKETCH, mk_conv2mm);
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
