/*
 *			M E M S E T
 *
 *  Author - Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif


char *
memset(char *s, register char c, register int n)
{
	register char *p=s;

	if (p)
		while (n-- > 0)
			*p++ = c;

	return s;
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
