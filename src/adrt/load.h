/*                         L O A D . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2025 United States Government as represented by
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
/** @file load.h
 *
 */

#ifndef ADRT_LOAD_H
#define ADRT_LOAD_H

#include "adrt.h"
#include "adrt_struct.h"

RENDER_EXPORT extern int slave_load(struct tie_s *tie, void *);

RENDER_EXPORT extern uint32_t slave_load_mesh_num;
RENDER_EXPORT extern adrt_mesh_t *slave_load_mesh_list;

RENDER_EXPORT extern int slave_load_g (struct tie_s *tie, char *data);

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
