/*
 *			B S D S E L E C T
 *
 *  Ordinarily, a stub for select() could have been implemented here,
 *  but the IRIS defines a graphics library routine select().
 *  On non-BSD systems, select() is a name likely to have been reused,
 *  so this special interface has been created.  This also has the slight
 *  advantage of centralizing the struct timeval stuff.
 */

#if defined(BSD) || defined(CRAY)
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

bsdselect( readfds, sec, us )
long readfds;
{
#if defined(BSD) || defined(sgi) || defined(stellar) || defined(CRAY)
	struct timeval tv;

	tv.tv_sec = sec;
	tv.tv_usec = us;
	if( select( 32, &readfds, 0L, 0L, &tv ) <= 0 )
		return(0);
	return( readfds );
#else
	return(32-1);	/* SYSV always has lots of input */
#endif
}
