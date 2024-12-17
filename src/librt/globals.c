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

unsigned int rt_debug = 0;

struct rt_g RTG = RT_G_INIT_ZERO;

struct resource rt_uniresource = RT_RESOURCE_INIT_ZERO;

void (*nmg_plot_anim_upcall)(void);


/**
 * this array depends on the values of the definitions of the
 * DB5_MINORTYPE_BINU_* in db5.h
 */
const char *binu_types[] = {
    NULL,
    NULL,
    "binary(float)",
    "binary(double)",
    "binary(u_8bit_int)",
    "binary(u_16bit_int)",
    "binary(u_32bit_int)",
    "binary(u_64bit_int)",
    NULL,
    NULL,
    NULL,
    NULL,
    "binary(8bit_int)",
    "binary(16bit_int)",
    "binary(32bit_int)",
    "binary(64bit_int)"
};

/* see table.c for primitive object function table definition */
extern const struct rt_functab OBJ[];


fastf_t rt_cline_radius = (fastf_t)-1.0;

/**
 * minimum number of bot pieces
 */
size_t rt_bot_minpieces = RT_DEFAULT_MINPIECES;

/**
 * minimum triangles per piece
 */
size_t rt_bot_tri_per_piece = RT_DEFAULT_TRIS_PER_PIECE;

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
