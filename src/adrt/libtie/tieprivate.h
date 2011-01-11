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

struct tie_geom_s {
    struct tie_tri_s **tri_list;
    unsigned int tri_num;
};

#ifdef _WIN32
# undef near
# undef far
#endif

struct tie_stack_s {
    struct tie_kdtree_s *node;
    tfloat near;
    tfloat far;
};

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
