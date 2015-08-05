/*                      S O L I D I T Y . H
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file libgcv/bot_solidity.h
 *
 * Functions for determining whether a BoT is solid.
 *
 */


#ifndef BOT_SOLIDITY_H
#define BOT_SOLIDITY_H


#include "common.h"

#include "gcv.h"
#include "rt/geom.h"


__BEGIN_DECLS


/*
 * Determines whether a BoT is solid.
 * Equivalent to bot_is_closed_fan() && bot_is_orientable()
 */
GCV_EXPORT extern int gcv_bot_is_solid(const struct rt_bot_internal *bot);

GCV_EXPORT extern int gcv_bot_is_closed_fan(const struct rt_bot_internal *bot);

GCV_EXPORT extern int gcv_bot_is_orientable(const struct rt_bot_internal *bot);


__END_DECLS


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
