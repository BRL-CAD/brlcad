/*                       G L O B A L S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
#include "db.h"

struct resource rt_uniresource;

void (*nmg_plot_anim_upcall)(void);

void (*nmg_vlblock_anim_upcall)(void);

void (*nmg_mged_debug_display_hack)(void);

double nmg_eue_dist = 0.05;

fastf_t rt_cline_radius = (fastf_t)-1.0;

uint32_t nmg_debug = 0;

struct bu_list	rtg_vlfree = BU_LIST_INIT_ZERO;

/**
 * minimum number of bot pieces
 */
size_t rt_bot_minpieces = RT_DEFAULT_MINPIECES;

/**
 * minimum number of bot for TIE
 */
size_t rt_bot_mintie = RT_DEFAULT_MINTIE;

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
