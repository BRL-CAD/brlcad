/*
	SCCS id:	@(#) empty.c	1.3
	Last edit: 	8/5/86 at 10:59:26
	Retrieved: 	8/6/86 at 13:35:58
	SCCS archive:	/vld/moss/src/fbed/s.empty.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) empty.c	1.3	last edit 8/5/86 at 10:59:26";
#endif

#ifdef BSD
#include <sys/time.h>
#endif

#ifdef SYSV
#ifdef VLDSYSV
/* BRL SysV under 4.2 */
#define select	_select
struct timeval
	{
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
	};
#else
/* Regular SysV */
xxx?;
#endif
#endif SYSV

/*	e m p t y ( )
	Examine file descriptor for input with no time delay.
	Return 1 if input is pending on file descriptor 'fd'.
	Return 0 if no input or error.
 */
int
empty( fd )
int	fd;
	{	static struct timeval	timeout = { 0L, 600L };
		int		readfds = 1 << fd;
		register int	nfound =
			select( fd+1, &readfds, (int *)0, (int *)0, &timeout );
	return	nfound == -1 ? 1 : (nfound == 0);
	}
