/*                        G L O B A L S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup nmg_global
 *
 */
/** @{ */
/** @file nmg/globals.h */

#ifndef NMG_GLOBALS_H
#define NMG_GLOBALS_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"

__BEGIN_DECLS

/**
 * @brief  debug bits for NMG's
 */
NMG_EXPORT extern uint32_t nmg_debug;

/**
 * @brief
 * global nmg animation vblock callback
 */
NMG_EXPORT extern void (*nmg_vlblock_anim_upcall)(void);

/**
 * @brief
 * global nmg mged display debug callback (ew)
 */
NMG_EXPORT extern void (*nmg_mged_debug_display_hack)(void);

/**
 * @brief
 * edge use distance tolerance
 */
NMG_EXPORT extern double nmg_eue_dist;


__END_DECLS

#endif  /* NMG_GLOBALS_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
