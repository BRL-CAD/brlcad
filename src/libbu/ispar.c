/*                         I S P A R . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbu */
/*@{*/
/** @file ispar.c
 *  This subroutine is separated off from parallel.c so that
 *  bu_bomb() and others can call it, without causing either
 *  parallel.c or semaphore.c to get referenced and thus causing
 *  the loader to drag in all the parallel processing stuff from
 *  the vendor library.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
/*@}*/

#ifndef lint
static const char RCSispar[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include <signal.h>
#include "machine.h"
#include "bu.h"


int	bu_pid_of_initiating_thread = 0;	/* don't declare in h/bu.h */

/*
 *			B U _ I S _ P A R A L L E L
 *
 *  A clean way for bu_bomb() to tell if this is a parallel application.
 *  If bu_parallel() is active, this routine will return non-zero.
 */
#ifndef _WIN32
int
bu_is_parallel(void)
{
	if( bu_pid_of_initiating_thread != 0 )  return 1;
	return 0;
}

/*
 *			B U _ K I L L _ P A R A L L E L
 *
 *  Used by bu_bomb() to help terminate parallel threads,
 *  without dragging in the whole parallel library if it isn't being used.
 */
void
bu_kill_parallel(void)
{
	if( bu_pid_of_initiating_thread == 0 )  return;
	if( bu_pid_of_initiating_thread == getpid() )  return;
	(void)kill( bu_pid_of_initiating_thread, 9 );
}
#else
int
bu_is_parallel()
{
	if( bu_pid_of_initiating_thread != 0 )  return 1;
	return 0;
}

/*
 *			B U _ K I L L _ P A R A L L E L
 *
 *  Used by bu_bomb() to help terminate parallel threads,
 *  without dragging in the whole parallel library if it isn't being used.
 */
void
bu_kill_parallel()
{
	return;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
