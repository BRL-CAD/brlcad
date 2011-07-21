/*                          B O M B . C
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

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "bio.h"

#include "bu.h"


/* failsafe storage to help ensure graceful shutdown */
static char *_bu_bomb_failsafe = NULL;

/* used for tty printing */
static int fd = -1;

#if defined(DEBUG)
/* used for crash reporting */
static char tracefile[512] = {0};
#endif

/* release memory on application exit */
static void
_free_bu_bomb_failsafe(void)
{
    if (_bu_bomb_failsafe) {
	free(_bu_bomb_failsafe);
	_bu_bomb_failsafe = NULL;
    }
}


int
bu_bomb_failsafe_init(void)
{
    if (_bu_bomb_failsafe) {
	return 1;
    }
    /* cannot use bu_*alloc here */
    _bu_bomb_failsafe = (char *)malloc(65536);
    atexit(_free_bu_bomb_failsafe);
    return 1;
}


void
bu_bomb(const char *str)
{

    /* First thing, always always always try to print the string.
     * Avoid passing additional format arguments so as to avoid
     * buffer allocations inside fprintf().
     */
    if (str && (strlen(str) > 0)) {
	fputc('\n', stderr);
	fputs(str, stderr);
	fputc('\n', stderr);
	fflush(stderr);
    }

    /* release the failsafe allocation to help get through to the end */
    _free_bu_bomb_failsafe();

    /* MGED would like to be able to additional logging, do callbacks. */
    if (BU_LIST_NON_EMPTY(&bu_bomb_hook_list.l)) {
	bu_call_hook(&bu_bomb_hook_list, (genptr_t)str);
    }

    if (bu_setjmp_valid) {
	/* Application is catching fatal errors */
	if (bu_is_parallel()) {
	    fprintf(stderr, "bu_bomb(): in parallel mode, could not longjmp up to application handler\n");
	} else {
	    /* Application is non-parallel, so this is safe */
	    longjmp(bu_jmpbuf, 1);
	    /* NOTREACHED */
	}
    }

#ifdef HAVE_UNISTD_H
    /*
     * No application level error handling,
     * go to extra pains to ensure that user gets to see this message.
     * For example, mged hijacks output sent to stderr.
     */
    {
	fd = open("/dev/tty", 1);
	if (LIKELY(fd > 0)) {
	    if (str && (strlen(str) > 0)) {
		ssize_t len, ret;

		len = strlen(str);
		ret = write(fd, str, len);
		if (ret != len)
		    perror("write failed");

		ret = write(fd, "\n", 1);
		if (ret != 1)
		    perror("write failed");
	    }
	    close(fd);
	}
    }
#endif

#if defined(DEBUG)
    /* save a backtrace, should hopefully have debug symbols */
    {
	/* if the file already exists, there's probably another thread
	 * writing out a report for the current process. acquire a
	 * mapped file semaphore so we only have one thread writing to
	 * the file at a time (can't just use BU_SEM_SYSCALL).
	 */
	bu_semaphore_acquire(BU_SEM_MAPPEDFILE);
	snprintf(tracefile, 512, "%s-%d-bomb.log", bu_getprogname(), bu_process_id());
	if (LIKELY(!bu_file_exists(tracefile))) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    fputs("Saving stack trace to ", stderr);
	    fputs(tracefile, stderr);
	    fputc('\n', stderr);
	    fflush(stderr);
	    bu_semaphore_release(BU_SEM_SYSCALL);

	    bu_crashreport(tracefile);
	}
	bu_semaphore_release(BU_SEM_MAPPEDFILE);
    }
#endif

    /* If in parallel mode, try to signal the leader to die. */
    bu_kill_parallel();

    /* try to save a core dump */
    if (UNLIKELY(bu_debug & BU_DEBUG_COREDUMP)) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fputs("Causing intentional core dump due to debug flag\n", stdout);
	fputs("Causing intentional core dump due to debug flag\n", stderr);
	fflush(stdout);
	fflush(stderr);
	bu_semaphore_release(BU_SEM_SYSCALL);

	fd = open("/dev/tty", 1);
	if (LIKELY(fd > 0)) {
	    ssize_t ret;
	    ret = write(fd, "Causing intentional core dump due to debug flag\n", 48);
	    if (ret != 48)
		perror("write failed");
	    close(fd);
	}
	abort();	/* should dump if ulimit is non-zero */
    }

    exit(12);
}


void
bu_exit(int status, const char *fmt, ...)
{
    if (LIKELY(fmt && strlen(fmt) > 0)) {
	va_list ap;
	struct bu_vls message;

	va_start(ap, fmt);

	bu_vls_init(&message);

	bu_vls_vprintf(&message, fmt, ap);

	if (!BU_SETJUMP) {
	    bu_bomb(bu_vls_addr(&message));
	}
	BU_UNSETJUMP;

	bu_vls_free(&message);
    }

    exit(status);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
