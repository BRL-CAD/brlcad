/*
 *			B S D S E L E C T
 *
 *  Ordinarily, a stub for select() could have been implemented here,
 *  but the IRIS defines a graphics library routine select().
 *  On non-BSD systems, select() is a name likely to have been reused,
 *  so this special interface has been created.  This also has the slight
 *  advantage of centralizing the struct timeval stuff.
 */
#if __STDC__
# include <unistd.h>
#endif

#if defined(i386) && !defined(BSD)
#	define BSD
#endif
#if defined(BSD)
#	include <sys/types.h>	/* for fd_set macros */
#endif
#if defined(BSD) || defined(CRAY) || defined(aux)
#	include <sys/time.h>	/* for struct timeval.  Includes <time.h> */
#else
#	include <time.h>
#endif
#if defined(sgi)
#	if !defined(mips) || defined(SGI4D_Rel2)
		/* 3D systems, and Rel2 4D systems. */
#		include <bsd/sys/types.h>
#		include <bsd/sys/time.h>
#	else
		/* Rel3 4D systems got it right */
#		include <sys/types.h>
#		include <sys/time.h>
#	endif
#endif
#ifdef stellar
#	include <sys/timeval.h>
#endif

#if defined(__sgi)
#	define _BSD_TYPES		/* Needed for IRIX 5.0.1 */
#		include <sys/types.h>
#		include <sys/time.h>
#endif

#ifdef FD_SET
/* The 4.3 BSD version */
bsdselect( readfds, sec, us )
long readfds;
{
	fd_set	fdset;
	int	width;
	struct	timeval tv;
	int	ret;

	tv.tv_sec = sec;
	tv.tv_usec = us;

	if( (width = getdtablesize()) <= 0 )
		width = 32;
	FD_ZERO( &fdset );
	fdset.fds_bits[0] = readfds;	/* peek inside! */

	if( (ret = select( width, &fdset, (fd_set *)0, (fd_set *)0, &tv )) <= 0 )  {
		if( ret < 0 )  perror("bsdselect/select");
		return(0);		/* no bits ready */
	}

	readfds = fdset.fds_bits[0];
	return( readfds );
}
#else

/* The old version */
bsdselect( readfds, sec, us )
long readfds;
{
#if defined(BSD) || defined(sgi) || defined(stellar) || defined(CRAY) || defined(__sgi) || defined(aux)
	struct timeval tv;
	int	ret;
	long	mask;

	tv.tv_sec = sec;
	tv.tv_usec = us;
	mask = readfds;
	if( (ret = select( 32, &mask, 0L, 0L, &tv )) <= 0 )  {
		if( ret < 0 )  perror("bsdselect/select");
		return(0);	/* No bits ready */
	}
	return( mask );
#else
	return(32-1);	/* SYSV always has lots of input */
#endif
}
#endif /* FD_SET */
