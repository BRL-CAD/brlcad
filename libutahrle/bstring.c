/*
 *			B S T R I N G . C
 *
 *  Interface Berkeley bzero() and bcopy() to SystemV routines,
 *  when not on a Berkeley system.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Id$ (BRL)";
#endif

#ifndef BSD

/*
 *			B Z E R O
 */
void
bzero(str,n)
char	*str;
int	n;
{
	memset( str, '\0', n );
}

/*
 *			B C O P Y
 */
void
bcopy(from,to,count)
char	*from;
char	*to;
int	count;
{
	memcpy( to, from, count );
}

#endif

