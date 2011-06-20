/*                       G L O B A L S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/globals.c
 *
 * Global variables in LIBGED.
 *
 * New global variables are discouraged and refactoring in ways that
 * eliminates existing global variables without reducing functionality
 * is always encouraged.
 *
 */
/** @} */

#include "common.h"

#include "bio.h"

#include "dg.h"
#include "solid.h"

#include "./ged_private.h"


/**
 * head of drawable geometry object list
 */
struct dg_obj HeadDGObj;

/**
 * head of free solid list
 */
struct solid _FreeSolid;

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
