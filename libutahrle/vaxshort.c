/*
 *			V A X S H O R T
 *
 *  Code to manipulate 16-bit integers in VAX order in a
 *  machine independent manner.
 *
 *  (VAX is a trademark of Digital Equipment Corporation)
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

/*
 *			V A X _ G S H O R T
 *
 *  Obtain a 16-bit signed integer from two adjacent characters,
 *  stored in VAX order, regardless of word alignment.
 */
int
vax_gshort(msgp)
char *msgp;
{
	register unsigned char *p = (unsigned char *) msgp;
	register int	i;

	if( (i = (p[1] << 8) | p[0]) & 0x8000 )
		return(i | ~0xFFFF);	/* Sign extend */
	return(i);
}

/*
 *			V A X _ P S H O R T
 */
char *
vax_pshort(msgp, s)
register char *msgp;
register unsigned short s;
{

	msgp[0] = s & 0xFF;
	msgp[1] = s >> 8;
	return(msgp+2);
}
