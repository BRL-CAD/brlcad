/*                       T I E P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file tieprivate.h
 *
 * private stuff for libtie/librender
 *
 */

#ifndef _TIEPRIVATE_H
#define _TIEPRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tie_geom_s {
    tie_tri_t **tri_list;
    unsigned int tri_num;
} tie_geom_t;

typedef struct tie_stack_s {
    tie_kdtree_t *node;
    tfloat near;
    tfloat far;
} tie_stack_t;

#ifdef __cplusplus
}
#endif

#endif /* _TIEPRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
