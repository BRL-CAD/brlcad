/*                       G L O B A L S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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

#include "bu.h"


/**
 * number of calls to bu_malloc()/bu_calloc()/bu_alloc().
 *
 * used by rt.
 * not semaphore-protected and is thus only an estimate.
 */
long bu_n_malloc = 0;

/**
 * number of calls to bu_free().
 *
 * used by rt.
 * not semaphore-protected and is thus only an estimate.
 */
long bu_n_free = 0;

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
const char bu_vls_message[] = "bu_vls_str";

/**
 * used by malloc and vls as the bu_strdup debug string.
 *
 * NOT published in a public header.
 */
const char bu_strdup_message[] = "bu_strdup string";

/**
 * process id of the initiating thread. used to shutdown bu_parallel
 * threads/procs.
 *
 * NOT published in a public header.
 */
int bu_pid_of_initiating_thread = 0;

/**
 * list of callbacks to call during bu_bomb.
 */
struct bu_hook_list bu_bomb_hook_list = {
    {
	BU_LIST_HEAD_MAGIC,
	&bu_bomb_hook_list.l,
	&bu_bomb_hook_list.l
    },
    NULL,
    GENPTR_NULL
};

/**
 * list of callbacks to call during bu_log.
 *
 * NOT published in a public header.
 */
struct bu_hook_list bu_log_hook_list = {
    {
	BU_LIST_HEAD_MAGIC,
	&bu_log_hook_list.l,
	&bu_log_hook_list.l
    },
    NULL,
    GENPTR_NULL
};

/**
 * bu_setjmp_valid is global because BU_SETJUMP() *must* be a macro.
 * If you replace bu_bomb() with one of your own, you must also
 * provide these variables, even if you don't use them.
 */
int bu_setjmp_valid = 0;

/**
 * for BU_SETJMP().  bu_jmpbuf is global because BU_SETJUMP() *must*
 * be a macro.  If you replace bu_bomb() with one of your own, you
 * must also provide these variables, even if you don't use them.
 */
jmp_buf bu_jmpbuf;

/* externed in bu.h */
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
