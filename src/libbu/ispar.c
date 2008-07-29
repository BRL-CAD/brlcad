/*                         I S P A R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
/** @addtogroup thread */
/** @{ */
/** @file ispar.c
 *
 * @brief
 * subroutine to determine if we are multi-threaded
 *
 * This subroutine is separated off from parallel.c so that bu_bomb()
 * and others can call it, without causing either parallel.c or
 * semaphore.c to get referenced and thus causing the loader to drag
 * in all the parallel processing stuff from the vendor library.
 *
 */

#include "common.h"

#include <math.h>
#include <signal.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bio.h"

#include "bu.h"

/* non-published global */
extern int bu_pid_of_initiating_thread;


/**
 *			B U _ I S _ P A R A L L E L
 *
 *  A clean way for bu_bomb() to tell if this is a parallel application.
 *  If bu_parallel() is active, this routine will return non-zero.
 */
int
bu_is_parallel(void)
{
    if (bu_pid_of_initiating_thread != 0)
	return 1;
    return 0;
}

/**
 *			B U _ K I L L _ P A R A L L E L
 *
 *  Used by bu_bomb() to help terminate parallel threads,
 *  without dragging in the whole parallel library if it isn't being used.
 */
void
bu_kill_parallel(void)
{
    if (bu_pid_of_initiating_thread == 0)
	return;

    if (bu_pid_of_initiating_thread == bu_process_id())
	return;

    bu_terminate(bu_pid_of_initiating_thread);

    return;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
