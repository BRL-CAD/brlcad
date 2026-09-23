/*                    T E S T _ U T I L S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#ifndef RT_EDIT_TEST_UTILS_H
#define RT_EDIT_TEST_UTILS_H

#include "common.h"

#include "bu/defines.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

static inline int
edit_test_make_sketch(struct rt_wdb *wdbp, const char *name, fastf_t marker)
{
    struct rt_sketch_internal *skt;
    BU_ALLOC(skt, struct rt_sketch_internal);
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;
    VSET(skt->V, 0, 0, 0);
    VSET(skt->u_vec, 1, 0, 0);
    VSET(skt->v_vec, 0, 1, 0);
    skt->vert_count = 1;
    skt->verts = (point2d_t *)bu_calloc(1, sizeof(point2d_t), "edit test sketch vertex");
    skt->verts[0][X] = marker;
    skt->curve.count = 0;
    skt->curve.reverse = NULL;
    skt->curve.segment = NULL;

    return wdb_export(wdbp, name, (void *)skt, ID_SKETCH, 1.0);
}

static inline int
edit_test_filename_callback(int UNUSED(argc), const char **UNUSED(argv),
                            void *data, void *result)
{
    *(const char **)result = (const char *)data;
    return BRLCAD_OK;
}

#endif /* RT_EDIT_TEST_UTILS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
