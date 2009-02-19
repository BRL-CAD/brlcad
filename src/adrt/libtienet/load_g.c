/*                        L O A D _ G . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2009-2009 United States Government as represented by
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
/** @file load_g.c
 *
 * Attempt to load a single top-level comb from a named .g file. The file must
 * exist on the machine the 'slave' program is running, with the correct path
 * passed to it. Only one combination is used, intended to be the top of the
 * tree of concern. It's assumed that only BOT's are to be loaded, non-bots will
 * be silently ignored for now. No KD-TREE caching is assumed. I like tacos.
 */

#ifndef TIE_PRECISION
# define TIE_PRECISION 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"

#include "tienet.h"

#include "load.h"

int
slave_load_g (tie_t *tie, char *data)
{
    char *filename, *region;
    filename = data;
    region = data + strlen(filename) + 1;
    printf("Want to read %s from %s\n", region, filename);
    return -1;
}

