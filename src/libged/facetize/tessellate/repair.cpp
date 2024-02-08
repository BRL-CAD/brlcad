/*                   R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/facetize/tessellate/repair.cpp
 *
 * When we have a volumetric bot that can't form a manifold, we need to see if
 * we can "automatically fix" it to be suitable for these purposes.
 *
 * This is a hard problem, without a truly general solution - we need to try to
 * handle "almost correct" meshes, but can't do much with complete garbage.
 */

#include "common.h"

#include "bu/opt.h"

#include "../../ged_private.h"
#include "./tessellate.h"

// Potential resources to investigate:
//
// https://github.com/wjakob/instant-meshes (also https://github.com/Volumental/instant-meshes)
// https://github.com/BrunoLevy/geogram (hole filling, Co3Ne)
int
bot_repair(struct rt_bot_internal **obot, struct rt_bot_internal *bot, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    if (!obot || !bot || !ttol || !tol)
	return -1;
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

