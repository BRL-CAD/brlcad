/*                    B T G . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2024 United States Government as represented by
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
/** @file primitives/bot/btg.c
 *
 * For compiling double version of tie library for use in adrt_slave and isst.
 *
 */

#ifdef TIE_PRECISION
#  undef TIE_PRECISION
#endif
#define TIE_PRECISION 1

#include "raytrace.h"

#include "tie.c"
#include "tie_kdtree.c"

int tie_check_degenerate = 0;
fastf_t TIE_PREC = 0.1;

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
