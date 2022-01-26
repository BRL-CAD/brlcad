/*                   N M G _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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
/** @file nmg_private.h
 *
 * Functions that are used internally by multiple NMG files but are
 * not intended to be public API.
 *
 * For the moment we will leave the export annotations, in case unit
 * testing requires us to work with these functions specifically in
 * testing executables.
 *
 */

#include "common.h"
#include "nmg/defines.h"

/**
 * @brief Internal routine to kill an edge geometry structure (of either
 * type), if all the edgeuses on its list have vanished.  Regardless,
 * the edgeuse's geometry pointer is cleared.
 *
 * This routine does only a single edgeuse.  If the edgeuse mate has
 * geometry to be killed, make a second call.  Sometimes only one of
 * the two needs to release the geometry.
 *
 * @retval 0 If the old edge geometry (eu->g.magic_p) has other uses.
 * @retval 1 If the old edge geometry has been destroyed. Caller beware!
 *
 * NOT INTENDED FOR GENERAL USE!
 * However, it is not unique to mk.c - mod.c needs it for nmg_eusplit().
 */
NMG_EXPORT extern int nmg_keg(struct edgeuse *eu);


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
