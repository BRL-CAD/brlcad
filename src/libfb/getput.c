/*
 *			G E T P U T . C
 *
 * Routines to insert/extract short/long's. Must account for byte
 * order and non-alignment problems.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

unsigned short
fbgetshort(char *msgp)
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
fbgetlong(char *msgp)
{
	register unsigned char *p = (unsigned char *) msgp;
	register unsigned long u;

	u = *p++; u <<= 8;
	u |= *p++; u <<= 8;
	u |= *p++; u <<= 8;
	return (u | *p);
}

char *
fbputshort(register short unsigned int s, register char *msgp)
{

	msgp[1] = s;
	msgp[0] = s >> 8;
	return(msgp+2);
}

char *
fbputlong(register long unsigned int l, register char *msgp)
{

	msgp[3] = l;
	msgp[2] = (l >>= 8);
	msgp[1] = (l >>= 8);
	msgp[0] = l >> 8;
	return(msgp+4);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
