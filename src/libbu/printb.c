/*                        P R I N T B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup libbu */
/*@{*/

/** @file printb.c
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
/*@}*/

#ifndef lint
static const char libbu_printb_RCSid[] = "@(#)$Header$ (ARL)";
#endif



#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
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
bu_vls_printb(struct bu_vls *vls, const char *s, register long unsigned int v, register const char *bits)
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
bu_printb(const char *s, register long unsigned int v, register const char *bits)
{
	struct bu_vls	str;

	bu_vls_init(&str);
	bu_vls_printb( &str, s, v, bits );
	bu_log("%s", bu_vls_addr(&str) );
	bu_vls_free(&str);
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
