/*                       G L O B A L S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @file libbu/globals.c
 *
 * Global variables in LIBBU.
 *
 * New global variables are discouraged and refactoring in ways that
 * eliminates existing global variables without reducing functionality
 * is always encouraged.
 *
 */

#include "bu/debug.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/vls.h"

/**
 * number of calls to bu_malloc()/bu_calloc()/bu_alloc().
 *
 * used by rt.
 * not semaphore-protected and is thus only an estimate.
 */
long bu_n_malloc = 0;

/**
 * number of calls to bu_realloc().
 *
 * used by rt.
 * not semaphore-protected and is thus only an estimate.
 */
long bu_n_realloc = 0;

/**
 * used by malloc and vls as the bu_malloc/bu_free debug string.
 *
 * NOT published in a public header.
 */
EXTERNVARINIT const char bu_vls_message[] = "bu_vls_str";

/**
 * used by malloc and vls as the bu_strdup debug string.
 *
 * NOT published in a public header.
 */
EXTERNVARINIT const char bu_strdup_message[] = "bu_strdup string";

/**
 * Marker for knowing if an exception handler is set.  bu_setjmp_valid
 * is global array because BU_SETJUMP() *must* be a macro (jump calls
 * must be embedded into the frame they return to).
 *
 * If you replace bu_bomb() with one of your own, you must also
 * provide these variables, even if you don't use them.
 */
int bu_setjmp_valid[MAX_PSW] = {0};

/**
 * Exception handling contexts used by BU_SETJUMP().  bu_jmpbuf is a
 * global array because BU_SETJUMP() *must* be a macro (jump calls
 * must be embedded into the frame they return to).
 *
 * If you replace bu_bomb() with one of your own, you must also
 * provide these variables, even if you don't use them.
 */
jmp_buf bu_jmpbuf[MAX_PSW];

/* externed in bu/ headers */
int bu_debug = 0;
int bu_opterr = 1;
int bu_optind = 1;
int bu_optopt = 0;
char *bu_optarg = NULL;


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
