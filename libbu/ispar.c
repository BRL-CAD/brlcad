/*
 *			I S P A R . C
 *
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
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSispar[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"


int	bu_pid_of_initiating_thread = 0;	/* don't declare in h/bu.h */

/*
 *			B U _ I S _ P A R A L L E L
 *
 *  A clean way for bu_bomb() to tell if this is a parallel application.
 *  If bu_parallel() is active, this routine will return non-zero.
 */
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
	if( bu_pid_of_initiating_thread == 0 )  return;
	if( bu_pid_of_initiating_thread == getpid() )  return;
	(void)kill( bu_pid_of_initiating_thread, 9 );
}
