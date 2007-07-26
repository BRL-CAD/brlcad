/*                        G L O B A L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file global.c
 *
 *  All global state for the BRL-CAD Package ray-tracing library "librt".
 *
 *  Author -
 *	Michael John Muuss
 *
 */
/** @} */

#ifndef lint
static const char RCSglobal[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

struct rt_g	rt_g;			/* All global state */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
