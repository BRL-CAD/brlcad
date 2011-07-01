/*                        H M G L O B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file burst/HmGlob.c
 *
 */

#include "common.h"

#include <stdio.h>

FILE *HmTtyFp = NULL;   /* read keyboard, not stdin */
int HmLftMenu = 1;	/* default top-level menu position */
int HmTopMenu = 1;
int HmMaxVis = 10;	/* default maximum menu items displayed */
int HmLastMaxVis = 10;	/* track changes in above parameter */
int HmTtyFd;		/* read keyboard, not stdin */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
