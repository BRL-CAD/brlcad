/*                          B O M B . C
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
/** @addtogroup bu_log */
/** @{ */
/** @file ./libbu/bomb.c
 *
 *  This routine is called on a fatal
 *  error, where no recovery is possible.
 *
 *  @par Functions -
 *	bu_bomb		Called upon fatal error.
 *
 *  @author	Michael John Muuss
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory			@n
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */

#ifndef lint
static const char RCSbomb[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_UNIX_IO
#  include <fcntl.h>
#endif

#include "machine.h"
#include "bu.h"


struct bu_hook_list bu_bomb_hook_list = {
    {	BU_LIST_HEAD_MAGIC,
	&bu_bomb_hook_list.l,
	&bu_bomb_hook_list.l
    },
    BUHOOK_NULL,
    GENPTR_NULL
};


/*
 * These variables are global because BU_SETJUMP() *must* be a macro.
 * If you replace this version of bu_bomb() with one of your own,
 * you must also provide these variables, even if you don't use them.
 */
int		bu_setjmp_valid = 0;	/**< @brief !0 = bu_jmpbuf is valid */
jmp_buf		bu_jmpbuf;		/**< @brief for BU_SETJMP() */


/** failsafe storage to help ensure graceful shutdown */
static char *_bu_bomb_failsafe = NULL;

/* release memory on application exit */
static void
_free_bu_bomb_failsafe()
{
    if (_bu_bomb_failsafe) {
	free(_bu_bomb_failsafe);
	_bu_bomb_failsafe = NULL;
    }
}


int
bu_bomb_failsafe_init()
{
    if (_bu_bomb_failsafe) {
	return 1;
    }
    /* cannot use bu_*alloc here */
    _bu_bomb_failsafe = malloc(65536);
    atexit(_free_bu_bomb_failsafe);
    return 1;
}


/**
 *			B U _ B O M B
 *@brief
 *  Abort the program with a message.
 *
 *  Only produce a core-dump when that debugging bit is set.  Note
 *  that this function is meant to be a last resort graceful abort.
 *  It should not attempt to allocate anything on the stack or heap.
 */
void
bu_bomb(const char *str)
{

    /* First thing, always always always try to print the string.
     * Avoid passing additional format arguments so as to avoid
     * buffer allocations inside fprintf().
     */
    fprintf(stderr, "\n");
    fprintf(stderr, str);
    fprintf(stderr, "\n");
    fflush(stderr);

    /* release the failsafe allocation to help get through to the end */
    _free_bu_bomb_failsafe();

    /* MGED would like to be able to additional logging, do callbacks. */
    if (BU_LIST_NON_EMPTY(&bu_bomb_hook_list.l)) {
	bu_call_hook(&bu_bomb_hook_list, (genptr_t)str);
    }

    if( bu_setjmp_valid )  {
	/* Application is catching fatal errors */
	if( bu_is_parallel() )  {
	    fprintf(stderr,"bu_bomb(): in parallel mode, could not longjmp up to application handler\n");
	} else {
	    /* Application is non-parallel, so this is safe */
	    fprintf(stderr,"bu_bomb(): taking longjmp up to application handler\n");
	    longjmp( (void *)(bu_jmpbuf), 1 );
	    /* NOTREACHED */
	}
    }

#if defined(HAVE_UNIX_IO)
    /*
     * No application level error handling,
     * go to extra pains to ensure that user gets to see this message.
     * For example, mged hijacks output sent to stderr.
     */
    {
	int fd = open("/dev/tty", 1);
	if (fd) {
	    write(fd, str, strlen(str));
	    write(fd, "\n", 1);
	    close(fd);
	}
    }
#endif

#if defined(DEBUG)
    /* save a backtrace, should hopefully have debug symbols */
    {
	FILE *fp = NULL;
	char tracefile[512] = {0};
	snprintf(tracefile, 512, "%s-%d-bomb.log", bu_getprogname(), bu_process_id());
	fprintf(stderr, "Saving stack trace to ");
	fprintf(stderr, tracefile);
	fprintf(stderr, "\n");
	fflush(stderr);
	bu_crashreport(tracefile);
    }
#endif

    /* If in parallel mode, try to signal the leader to die. */
    bu_kill_parallel();

    /* try to save a core dump */
    if( bu_debug & BU_DEBUG_COREDUMP )  {
	fprintf(stdout,"Causing intentional core dump due to debug flag\n");
	fprintf(stderr,"Causing intentional core dump due to debug flag\n");
	fflush(stdout);
	fflush(stderr);
	abort();	/* should dump if ulimit is non-zero */
    }

    exit(12);
}
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
