/*
	SCCS id:	@(#) empty.c	2.3
	Modified: 	1/5/87 at 16:52:54
	Retrieved: 	1/5/87 at 16:58:01
	SCCS archive:	/vld/moss/src/fbed/s.empty.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#if ! defined( lint )
static
char sccsTag[] = "@(#) empty.c 2.3, modified 1/5/87 at 16:52:54, archive /vld/moss/src/fbed/s.empty.c";
#endif
#if defined( BSD ) || defined( CRAY ) || defined( sun )
#	include <sys/types.h>
#	include <sys/time.h>
#endif

#if defined(sgi)
#	include <bsd/sys/types.h>
#	if !defined(mips) || defined(SGI4D_Rel2)
		/* 3D systems, and Rel2 4D systems. */
#		include <bsd/sys/time.h>
#	else
		/* Rel3 4D systems got it right */
#		include <sys/time.h>
#	endif
#	include "fb.h"
#endif

#ifdef VLDSYSV
/* BRL SysV under 4.2 */
#define select	_select
struct timeval
	{
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
	};
#else
#endif

#ifndef FD_ZERO
/* 4.2 does not define these */
#ifdef CRAY
#define	FD_SET(n, p)	(*p |= (n) == 0 ? 1 : (1 << (n)))
#define FD_ZERO(p)	*p = 0
typedef	long	fd_set;
#else
#define	FD_SET(n, p)	((p)->fds_bits[0] |= (n) == 0 ? 1 : (1 << (n)))
#define FD_ZERO(p)	(p)->fds_bits[0] = 0
/** typedef	struct fd_set { fd_mask	fds_bits[1]; } fd_set; **/
#endif
#endif /* FD_ZERO */

/*	e m p t y ( )
	Examine file descriptor for input with no time delay.
	Return 1 if input is pending on file descriptor 'fd'.
	Return 0 if no input or error.
 */
int
empty( fd )
int fd;
	{
#if 0
	if( isSGI )
		return sgi_Empty();
	else
#endif
#if defined( sgi )
		{	static struct timeval	timeout = { 0L, 600L };
			fd_set		readfds;
			register int nfound;
		FD_ZERO( &readfds );
		FD_SET( fd, &readfds );
		nfound = select( fd+1, &readfds, (fd_set *)0, (fd_set *)0, &timeout );
		return nfound == -1 ? 1 : (nfound == 0);
		}
#else
	/* On most machines we aren't supporting the mouse, so no need to
		not block on keyboard input. */
	return 0;
#endif
	}
