/*
 *			P R I N T B . C
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static const char libbu_printb_RCSid[] = "@(#)$Header$ (ARL)";
#endif



#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"


/*
 *			B U _ V L S _ P R I N T B
 *
 *  Format a value a la the %b format of the kernel's printf
 *
 *    vls	variable length string to put output in
 *    s		title string
 *    v		the integer with the bits in it
 *    bits	a string which starts with the desired base (8 or 16)
 *		as \010 or \020, followed by
 *		words preceeded with embedded low-value bytes indicating
 *		bit number plus one,
 *		in little-endian order, eg:
 *		"\010\2Bit_one\1BIT_zero"
 */
void
bu_vls_printb( vls, s, v, bits)
struct bu_vls		*vls;
const char		*s;
register unsigned long	v;
register const char	*bits;
{
	register int i, any = 0;
	register char c;

	if (*bits++ == 8)
		bu_vls_printf( vls, "%s=0%o <", s, v);
	else
		bu_vls_printf( vls, "%s=x%x <", s, v);
	while ((i = *bits++)) {
		if (v & (1L << (i-1))) {
			if (any)
				bu_vls_strcat( vls, ",");
			any = 1;
			for (; (c = *bits) > 32; bits++)
				bu_vls_printf( vls, "%c", c);
		} else
			for (; *bits > 32; bits++)
				;
	}
	bu_vls_strcat( vls, ">");
}

/*
 *			B U _ P R I N T B
 *
 *  Format and print, like bu_vls_printb().
 */
void
bu_printb(s, v, bits)
const char		*s;
register unsigned long	v;
register const char	*bits;
{
	struct bu_vls	str;

	bu_vls_init(&str);
	bu_vls_printb( &str, s, v, bits );
	bu_log("%s", bu_vls_addr(&str) );
	bu_vls_free(&str);
}
