/*
 *  			C O L U M N S
 *  
 *  A set of routines for printing columns of data.
 *
 * Functions -
 *	col_item	Called to print an item
 *	col_putchar	Called to annotate an item
 *	col_eol		Called to end a line
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "../h/db.h"		/* for NAMESIZE */

static int	col_count;		/* names listed on current line */
static int	col_len;		/* length of previous name */
#define	COLUMNS	((80 + NAMESIZE - 1) / NAMESIZE)

void
col_item(cp)
register char *cp;
{
	/* Output newline if last column printed. */
	if ( col_count >= COLUMNS )  {	/* line now full */
		(void)putchar( '\n' );
		col_count = 0;
	} else if ( col_count != 0 ) {
		/* Tab to start column. */
		do
			(void)putchar( '\t' );
		while ( (col_len += 8) < NAMESIZE );
	}
	/* Output string and save length for next tab. */
	col_len = 0;
	while ( *cp != '\0' )  {
		(void)putchar( *cp++ );
		++col_len;
	}
	col_count++;
#undef	COLUMNS
}

void
col_putchar(c)
char c;
{
	(void)putchar(c);
	col_len++;
}

void
col_eol()
{
	if ( col_count != 0 )		/* partial line */
		(void)putchar( '\n' );
	col_count = 0;
	col_len = 0;
}
