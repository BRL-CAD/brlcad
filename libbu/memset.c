/*
 *			M E M S E T
 *
 *  Author - Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static const char libbu_memset_RCSid[] = "@(#)$Header$ (ARL)";
#endif

#if defined(BSD) && (BSD < 43)

char *
memset( s, c, n )
char	*s;
register char	c;
register int	n;
{
	register char *p=s;

	if (p)
		while (n-- > 0)
			*p++ = c;

	return s;
}

#endif
