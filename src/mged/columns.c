/*                       C O L U M N S . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file columns.c
 *
 *  A set of routines for printing columns of data.
 *
 * Functions -
 *	col_item	Called to print an item
 *	col_putchar	Called to annotate an item
 *	col_eol		Called to end a line
 *	cmpdirname	Comparison function for col_pr4v
 *	col_pr4v	Called to sort and print directory entry names
 *			  vertically in four columns (ala "ls -C")
 *
 *  Authors -
 *	Michael John Muuss
 *	Robert Jon Reschly Jr.
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"

#include "./ged.h"

static int	col_count;		/* names listed on current line */
static int	col_len;		/* length of previous name */
#define TERMINAL_WIDTH	80		/* XXX */
#define	COLUMNS	((TERMINAL_WIDTH + NAMESIZE - 1) / NAMESIZE)

/*
 *			V L S _ C O L _ I T E M
 */
void
vls_col_item(
	struct bu_vls		*str,
	register const char	*cp)
{
	/* Output newline if last column printed. */
	if( col_count >= COLUMNS || (col_len+NAMESIZE-1) >= TERMINAL_WIDTH )  {
		/* line now full */
		bu_vls_putc( str, '\n' );
		col_count = 0;
	} else if ( col_count != 0 ) {
		/* Space over before starting new column */
		do {
			bu_vls_putc( str, ' ' );
			col_len++;
		}  while ( (col_len % NAMESIZE) != 0 );
	}
	/* Output string and save length for next tab. */
	col_len = 0;
	while ( *cp != '\0' )  {
		bu_vls_putc( str, *cp );
		++cp;
		++col_len;
	}
	col_count++;
}

/*
 */
void
vls_col_eol( struct bu_vls *str )
{
	if ( col_count != 0 )		/* partial line */
		bu_vls_putc( str, '\n' );
	col_count = 0;
	col_len = 0;
}


/*
 *			C M P D I R N A M E
 *
 * Given two pointers to pointers to directory entries, do a string compare
 * on the respective names and return that value.
 */
int
cmpdirname(const genptr_t a, const genptr_t b)
{
	register struct directory **dp1, **dp2;

	dp1 = (struct directory **)a;
	dp2 = (struct directory **)b;
	return( strcmp( (*dp1)->d_namep, (*dp2)->d_namep));
}

/*
 *				C O L _ P R 4 V
 *
 *  Given a pointer to a list of pointers to names and the number of names
 *  in that list, sort and print that list in column order over four columns.
 */
void
vls_col_pr4v(struct bu_vls *vls, struct directory **list_of_names, int num_in_list)
{
  int lines, i, j, namelen, this_one;
  int k,
      maxnamelen,      /* longest name in list */
      cwidth,          /* column width */
      numcol;         /* number of columns */

  qsort( (genptr_t)list_of_names,
	 (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	 (int (*)())cmpdirname);

  /*
   * Traverse the list of names, find the longest name and set the
   * the column width and number of columns accordingly.
   * If the longest name is greater than 80 characters, the number of columns
   * will be one.
   */
  maxnamelen = 0;
  for( k=0; k < num_in_list; k++) {
    namelen = strlen(list_of_names[k]->d_namep);
    if(namelen > maxnamelen)
      maxnamelen = namelen;
  }
  if(maxnamelen <= 16)
    maxnamelen = 16;
  cwidth = maxnamelen + 4;
  if(cwidth > 80)
    cwidth = 80;
  numcol = TERMINAL_WIDTH / cwidth;

  /*
   * For the number of (full and partial) lines that will be needed,
   * print in vertical format.
   */
  lines = (num_in_list + (numcol - 1)) / numcol;
  for( i=0; i < lines; i++) {
    for(j=0; j < numcol; j++) {
      this_one = j * lines + i;
      bu_vls_printf(vls, "%s", list_of_names[this_one]->d_namep);
      namelen = strlen( list_of_names[this_one]->d_namep);
      /*
       * Region and ident checks here....  Since the code
       * has been modified to push and sort on pointers,
       * the printing of the region and ident flags must
       * be delayed until now.  There is no way to make the
       * decision on where to place them before now.
       */
      if(list_of_names[this_one]->d_flags & DIR_COMB) {
	bu_vls_putc(vls, '/');
	namelen++;
      }
      if(list_of_names[this_one]->d_flags & DIR_REGION) {
	bu_vls_putc(vls, 'R');
	namelen++;
      }
      /*
       * Size check (partial lines), and line termination.
       * Note that this will catch the end of the lines
       * that are full too.
       */
      if( this_one + lines >= num_in_list) {
	bu_vls_putc(vls, '\n');
	break;
      } else {
	/*
	 * Pad to next boundary as there will be
	 * another entry to the right of this one.
	 */
        while( namelen++ < cwidth)
	  bu_vls_putc(vls, ' ');
      }
    }
  }
}

void
vls_long_dpp(
	struct bu_vls *vls,
	struct directory **list_of_names,
	int num_in_list,
	int aflag,	/* print all objects */
	int cflag,	/* print combinations */
	int rflag,	/* print regions */
	int sflag)	/* print solids */
{
  int i;
  int isComb, isRegion;
  int isSolid;
  const char *type;
  int max_nam_len = 0;
  int max_type_len = 0;
  struct directory *dp;

  qsort( (genptr_t)list_of_names,
	 (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	 (int (*)())cmpdirname);

  for( i=0 ; i<num_in_list ; i++ ) {
	  int len;

	  dp = list_of_names[i];
	  len = strlen( dp->d_namep );
	  if( len > max_nam_len )
		  max_nam_len = len;

	  if( dp->d_flags & DIR_REGION )
		  len = 6;
	  else if( dp->d_flags & DIR_COMB )
		  len = 4;
	  else if( dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY )
		  len = 6;
	  else
		  len = strlen( rt_functab[dp->d_minor_type].ft_label );

	  if( len > max_type_len )
		  max_type_len = len;
  }
  /*
   * i - tracks the list item
   */
  for (i=0; i < num_in_list; ++i) {
    if (list_of_names[i]->d_flags & DIR_COMB) {
      isComb = 1;
      isSolid = 0;
      type = "comb";

      if (list_of_names[i]->d_flags & DIR_REGION) {
	isRegion = 1;
        type = "region";
      }
      else
	isRegion = 0;
    } else {
      isComb = isRegion = 0;
      isSolid = 1;
      type = rt_functab[list_of_names[i]->d_minor_type].ft_label;
    }

    if( list_of_names[i]->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY ) {
	    isSolid = 0;
	    type = "global";
    }

    /* print list item i */
    dp = list_of_names[i];
    if (aflag ||
	(!cflag && !rflag && !sflag) ||
	(cflag && isComb) ||
	(rflag && isRegion) ||
	(sflag && isSolid)) {
	    bu_vls_printf(vls, "%s", dp->d_namep );
	    bu_vls_spaces(vls, max_nam_len - strlen( dp->d_namep ) );
	    bu_vls_printf(vls, " %s", type );
	    bu_vls_spaces(vls, max_type_len - strlen( type ) );
	    bu_vls_printf(vls,  " %2d %2d %d\n",
		    dp->d_major_type, dp->d_minor_type, dp->d_len);
    }
  }
}
/*
 *				V L S _ L I N E _ D P P
 *
 *  Given a pointer to a list of pointers to names and the number of names
 *  in that list, sort and print that list on the same line.
 */
void
vls_line_dpp(
	struct bu_vls *vls,
	struct directory **list_of_names,
	int num_in_list,
	int aflag,	/* print all objects */
	int cflag,	/* print combinations */
	int rflag,	/* print regions */
	int sflag)	/* print solids */
{
  int i;
  int isComb, isRegion;
  int isSolid;

  qsort( (genptr_t)list_of_names,
	 (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	 (int (*)())cmpdirname);

  /*
   * i - tracks the list item
   */
  for (i=0; i < num_in_list; ++i) {
    if (list_of_names[i]->d_flags & DIR_COMB) {
      isComb = 1;
      isSolid = 0;

      if (list_of_names[i]->d_flags & DIR_REGION)
	isRegion = 1;
      else
	isRegion = 0;
    } else {
      isComb = isRegion = 0;
      isSolid = 1;
    }

    /* print list item i */
    if (aflag ||
	(!cflag && !rflag && !sflag) ||
	(cflag && isComb) ||
	(rflag && isRegion) ||
	(sflag && isSolid)) {
      bu_vls_printf(vls,  "%s ", list_of_names[i]->d_namep);
    }
  }
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
