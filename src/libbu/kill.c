/*                          K I L L . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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

/* system headers */
#include <signal.h>
#include "bio.h"

/* common headers */
#include "bu.h"


/* c99 doesn't declare these */
#ifdef HAVE_KILL
extern int kill(pid_t, int);
#endif


int
bu_terminate(int process)
{
    int successful = 0;

#ifdef HAVE_KILL
    /* kill() returns 0 for success */
    successful = kill(process, SIGKILL);
    successful = !successful;
#else
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, (DWORD)process);
    if (hProcess != NULL) {
	successful = TerminateProcess(hProcess, 0);
    }
#endif

    return successful;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
