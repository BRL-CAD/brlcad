/*
	SCCS id:	@(#) empty.c	2.1
	Modified: 	12/9/86 at 15:55:14
	Retrieved: 	12/26/86 at 21:54:00
	SCCS archive:	/vld/moss/src/fbed/s.empty.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) empty.c 2.1, modified 12/9/86 at 15:55:14, archive /vld/moss/src/fbed/s.empty.c";
#endif

#ifdef BSD
#include <sys/types.h>
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
#ifdef sgi
#include <bsd/sys/types.h>
#include <bsd/sys/time.h>
#endif
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
#ifdef sgi
	{
	winattach();
	return	! qtest();
	}
#else	
	{	static struct timeval	timeout = { 0L, 600L };
		auto long	readfds = 1 << fd;
		register int	nfound;
	readfds = 1<<fd;
	nfound = select( fd+1, &readfds, (fd_set *)0, (fd_set *)0, &timeout );
	return	nfound == -1 ? 1 : (nfound == 0);
	}
#endif
