/*                        N M G _ M K . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2023 United States Government as represented by
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
/** @file edit.cpp
 *
 * Implementation of edit support for brep.
 *
 */


#include "common.h"

#include <stddef.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bu/log.h"
#include "brep/defines.h"
#include "brep/edit.h"

#include "debug_plot.h"
#include "brep_except.h"
#include "brep_defines.h"

void *create_empty_brep()
{
    ON_Brep *brep = new ON_Brep();
    return (void *)brep;
}