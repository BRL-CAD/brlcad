/*
 *			G E T P U T . C
 *
 * Routines to insert/extract short/long's. Must account for byte
 * order and non-alignment problems.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

unsigned short
getshort(msgp)
	char *msgp;
{
	register unsigned char *p = (unsigned char *) msgp;
#ifdef vax
	/*
	 * vax compiler doesn't put shorts in registers
	 */
	register unsigned long u;
#else
	register unsigned short u;
#endif

	u = *p++ << 8;
	return ((unsigned short)(u | *p));
}

unsigned long
getlong(msgp)
	char *msgp;
{
	register unsigned char *p = (unsigned char *) msgp;
	register unsigned long u;

	u = *p++; u <<= 8;
	u |= *p++; u <<= 8;
	u |= *p++; u <<= 8;
	return (u | *p);
}

char *
putshort(s, msgp)
	register unsigned short s;
	register char *msgp;
{

	msgp[1] = s;
	msgp[0] = s >> 8;
	return(msgp+2);
}

char *
putlong(l, msgp)
	register unsigned long l;
	register char *msgp;
{

	msgp[3] = l;
	msgp[2] = (l >>= 8);
	msgp[1] = (l >>= 8);
	msgp[0] = l >> 8;
	return(msgp+4);
}
