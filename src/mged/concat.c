/*                        C O N C A T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2010 United States Government as represented by
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
 */
/** @file concat.c
 *
 *  Functions -
 *	f_dup()		checks for dup names before cat'ing of two files
 *	f_concat()	routine to cat another GED file onto end of current file
 *
 *  Authors -
 *	Michael John Muuss
 *	Keith A. Applin
 *
 */

#include "common.h"

#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#include <string.h>

#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif

#include "bio.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"

#include "./mged.h"
#include "./sedit.h"

char	new_name[NAMESIZE+1];
char	prestr[NAMESIZE+1];
int	ncharadd;


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
