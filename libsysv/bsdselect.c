/*
 *			B S D S E L E C T
 *
 *  Ordinarily, a stub for select() could have been implemented here,
 *  but the IRIS defines a graphics library routine select().
 *  On non-BSD systems, select() is a name likely to have been reused,
 *  so this special interface has been created.  This also has the slight
 *  advantage of centralizing the struct timeval stuff.
 */

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
