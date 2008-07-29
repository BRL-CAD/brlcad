/*                       M L T _ D E F . H
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file mlt_def.h
 *
 * This file will contain definitions and declarations that will be
 * used in the Metropolis Light Transport algorithm
 *
 */

#ifndef MLT_DEF_H
#define MLT_DEF_H

#include "raytrace.h"

struct point_list {
    struct bu_list l;
    point_t pt_cell;
};

struct mlt_path {
    struct bu_list l;
    struct point_list * points;
};

struct mlt_app {
    struct mlt_path * path_list;
    point_t eye;
    point_t camera;
    /* ... */
};

/* struct light_source ?? */

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
