/*
 *			G E T P U T . C
 *
 * Routines to insert/extract short/long's. Must account for byte
 * order and non-alignment problems.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

u_short
getshort(msgp)
	char *msgp;
{
	register u_char *p = (u_char *) msgp;
#ifdef vax
	/*
	 * vax compiler doesn't put shorts in registers
	 */
	register u_long u;
#else
	register u_short u;
#endif

	u = *p++ << 8;
	return ((u_short)(u | *p));
}

u_long
getlong(msgp)
	char *msgp;
{
	register u_char *p = (u_char *) msgp;
	register u_long u;

	u = *p++; u <<= 8;
	u |= *p++; u <<= 8;
	u |= *p++; u <<= 8;
	return (u | *p);
}

char *
putshort(s, msgp)
	register u_short s;
	register char *msgp;
{

	msgp[1] = s;
	msgp[0] = s >> 8;
	return(msgp+2);
}

char *
putlong(l, msgp)
	register u_long l;
	register char *msgp;
{

	msgp[3] = l;
	msgp[2] = (l >>= 8);
	msgp[1] = (l >>= 8);
	msgp[0] = l >> 8;
	return(msgp+4);
}
