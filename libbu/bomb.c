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
static char RCSbomb[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"

/* These are global because BU_SETJUMP() *must* be a macro */
int		bu_setjmp_valid = 0;	/* !0 = bu_jmpbuf is valid */
jmp_buf		bu_jmpbuf;		/* for BU_SETJMP() */

/*
 *			R T _ B O M B
 *  
 *  Abort the program with a message.
 *  Only produce a core-dump when that debugging bit is set.
 */
void
bu_bomb(str)
CONST char *str;
{
	fprintf(stderr,"\n%s\n", str);
	fflush(stderr);

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

	if( bu_debug & BU_DEBUG_COREDUMP )  {
		fprintf(stderr,"bu_bomb causing intentional core dump due to debug flag\n");
		abort();	/* should dump */
	}

	/* If in parallel mode, try to signal the leader to die. */
	bu_kill_parallel();

	exit(12);
}
