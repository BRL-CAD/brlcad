/*
 *			B O M B . C
 *
 *  This routine is called on a fatal
 *  error, where no recovery is possible.
 *
 *  Functions -
 *	bu_bomb		Called upon fatal error.
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
static const char RCSbomb[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "machine.h"
#include "externs.h"
#include "bu.h"

#ifdef HAVE_UNIX_IO
#include <fcntl.h>
#endif

#if 1
struct bu_hook_list bu_bomb_hook_list = {
	{	BU_LIST_HEAD_MAGIC, 
		&bu_bomb_hook_list.l, 
		&bu_bomb_hook_list.l
	}, 
	BUHOOK_NULL,
	GENPTR_NULL
};
#else
struct bu_hook_list bu_bomb_hook_list;
#endif

/*
 * These variables are global because BU_SETJUMP() *must* be a macro.
 * If you replace this version of bu_bomb() with one of your own,
 * you must also provide these variables, even if you don't use them.
 */
int		bu_setjmp_valid = 0;	/* !0 = bu_jmpbuf is valid */
jmp_buf		bu_jmpbuf;		/* for BU_SETJMP() */

/*
 *			B U _ B O M B
 *  
 *  Abort the program with a message.
 *  Only produce a core-dump when that debugging bit is set.
 */
void
bu_bomb(const char *str)
{

	/* First thing, always always always try to print the string */
	fprintf(stderr,"\n%s\n", str);
	fflush(stderr);

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
#if __STDC__
			longjmp( (void *)(bu_jmpbuf), 1 );
#else
			longjmp( (int *)(bu_jmpbuf), 1 );
#endif
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
		int	fd = open("/dev/tty", 1);
		if( fd >= 0 )  {
			write( fd, str, strlen(str) );
			close(fd);
		}
	}
#endif

	/* If in parallel mode, try to signal the leader to die. */
	bu_kill_parallel();

	if( bu_debug & BU_DEBUG_COREDUMP )  {
		fprintf(stderr,"bu_bomb causing intentional core dump due to debug flag\n");
		abort();	/* should dump */
	}

	exit(12);
}
