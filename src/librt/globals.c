/*                       G L O B A L S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file librt/globals.c
 *
 * Global variables in LIBRT.
 *
 * New global variables are discouraged and refactoring in ways that
 * eliminates existing global variables without reducing functionality
 * is always encouraged.
 *
 */
/** @} */

#include "common.h"

#include "raytrace.h"
#include "rt/db4.h"
#include "rt/debug.h"

/* container holding reusable vlists */
struct bu_list rt_vlfree = BU_LIST_INIT_ZERO;

/* uniprocessor pre-prepared resource */
struct resource rt_uniresource = RT_RESOURCE_INIT_ZERO;


/* Debug flags */
unsigned int rt_debug = 0;

/* see table.c for primitive object function table definition */
extern const struct rt_functab OBJ[];

/* This one is REALLY nasty.  If I'm interpreting its use in cline correctly,
 * it allows an application to change the geometric interpretation of the
 * primitive itself at *run time* (either prep or shot??).  I'm not aware
 * of any other feature like this, and it's not clear to me how to implement
 * something like this in a proper general sense... for a general mechanism
 * of some sort we'd need a way for primitives to communicate back to apps
 * a list of per type, per-primitive (and maybe even per instance) parameters
 * and a way for them to set them on the actual prepped internal objects. */
fastf_t rt_cline_radius = (fastf_t)-1.0;


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
