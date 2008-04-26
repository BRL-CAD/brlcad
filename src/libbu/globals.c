/*                       G L O B A L S . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file globals.c
 *
 * Global variables in LIBBU.
 *
 * New global variables are discouraged and refactoring in ways that
 * eliminates existing global variables without reducing functionality
 * is always encouraged.
 *
 */

/** this variable controls the libbu debug level */
int	bu_debug = 0;

/**
 * number of calls to bu_malloc()/bu_calloc()/bu_alloc().
 *
 * not semaphore-protected and is thus only an estimate
 */
long	bu_n_malloc = 0;

/**
 * number of calls to bu_free().
 *
 * not semaphore-protected and is thus only an estimate
 */
long	bu_n_free = 0;

/**
 * number of calls to bu_realloc().
 *
 * not semaphore-protected and is thus only an estimate
 */
long	bu_n_realloc = 0;


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
