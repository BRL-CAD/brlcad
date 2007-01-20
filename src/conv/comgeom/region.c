/*                        R E G I O N . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
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
 *
 */
/** @file region.c
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <ctype.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

/* defined in read.c */
extern int get_line(register char *cp, int buflen, char *title);
extern int getint(char *cp, int start, int len);
extern void namecvt(register int n, register char **cp, int c);

/* defined in cvt.c */
extern void col_pr(char *str);

extern char	name_it[];

extern struct wmember	*wmp;	/* array indexed by region number */

extern FILE		*infp;
extern struct rt_wdb	*outfp;

extern int	reg_total;
extern int	version;
extern int	verbose;

char	rcard[128];

void	region_register(int reg_num, int id, int air, int mat, int los);
void	group_init(void);
void	group_register(char *name, int lo, int hi);
void	group_add(register int val, char *name);
void	group_write(void);

/*
 *			G E T R E G I O N
 *
 *  Use wmp[region_number] as head for each region.
 *
 *  Returns -
 *	-1	error
 *	 0	done
 */
int
getregion(void)
{
	int i, j;
	int card;
	int	op;
	int	reg_reg_flag;
	int	reg_num;
	char	*inst_name=NULL;
	int	inst_num;
	char *cp;

	reg_num = 0;		/* safety */

	/* Pre-load very first region card */
	if( get_line( rcard, sizeof(rcard), "region card" ) == EOF )  {
		printf("getregion: premature EOF\n");
		return( -1 );
	}
	if( getint( rcard, 0, 5 ) != 1 )  {
		printf("First region card not #1\ncard='%s'\n", rcard);
		return(-1);
	}

top:
	reg_reg_flag = 0;

	for( card=0; ; card++ )  {
		if( card == 0 )  {
			/* First card is already in input buffer */
			reg_num = getint( rcard, 0, 5 );

			/* -1 region number terminates table */
			if( reg_num < 0 )
				return( 0 );		/* Done */

			if( reg_num > reg_total )  {
				printf("%d regions is more than claimed %d\n",
					reg_num, reg_total );
				return(-1);
			}

			namecvt( reg_num, &(wmp[reg_num].wm_name), 'r' );
		} else {
			if( get_line( rcard, sizeof(rcard), "region card" ) == EOF )  {
				printf("getregion: premature EOF\n");
				return( -1 );
			}
			if( strcmp( rcard, "  end" ) == 0 ||
			    strcmp( rcard, "  END" ) == 0 )  {
			    	/* Version 1, DoE/MORSE */
			    	reg_total = reg_num;
			    	return(0);	/* done */
			}
			if( getint( rcard, 0, 5 ) != 0 )  {
				/* finished with this region */
				break;
			}
		}

		if( version == 1 )  {
			cp = rcard + 10;
		} else {
			cp = rcard + 6;
		}

		op = WMOP_UNION; /* default */

		/* Scan each of the 9 fields on the card */
		for( i=0; i<9; i++, cp += 7 )  {
			char	nbuf[32];
			char	*np;

			/* Remove all spaces from the number */
			np = nbuf;
			for( j=2; j<7; j++ )  {
				if( !isascii( cp[j] ) ) *np++ = '?';
				else if( isspace( cp[j] ) )  continue;
				*np++ = cp[j];
			}
			*np = '\0';

			/* Check for null field -- they are to be skipped */
			if( (inst_num = atoi(nbuf)) == 0 )  {
				/* zeros are allowed as placeholders */
				continue;
			}

			if( version == 5 )  {
				/* Region references region in Gift5 */
				if( cp[1] == 'g' || cp[1] == 'G' ) {
					reg_reg_flag = 1;
				} else if( cp[1] == 'R' || cp[1] == 'r' )  {
					/* 'OR' */
					op = WMOP_UNION;
				} else {
					if( inst_num < 0 )  {
						op = WMOP_SUBTRACT;
						inst_num = -inst_num;
					}  else  {
						op = WMOP_INTERSECT;
					}
				}
			} else {
				/* XXX this may actually be an old piece of code,
				 * rather than the V4 way of doing it. */
				if( cp[1] != ' ' )  {
					op = WMOP_UNION;
				}  else  {
					if( inst_num < 0 )  {
						op = WMOP_SUBTRACT;
						inst_num = -inst_num;
					}  else  {
						op = WMOP_INTERSECT;
					}
				}
			}

			/* In Gift5, regions can reference regions */
			if( reg_reg_flag )
				namecvt(inst_num, &inst_name, 'r');
			else
				namecvt( inst_num, &inst_name, 's' );
			reg_reg_flag = 0;

			(void)mk_addmember( inst_name, &wmp[reg_num].l, NULL, op );
			bu_free( inst_name, "inst_name" );
		}
	}

	if(verbose) col_pr( wmp[reg_num].wm_name );

	/* The region will be output later in getid(), below */

	goto top;
}

/*
 *			G E T I D
 *
 * Load the region ID information into the structures
 */
void
getid(void)
{
	int reg_num;
	int id;
	int air;
	int mat= -1;
	int los= -2;
	char	idcard[132];

	while(1)  {
		if( get_line( idcard, sizeof(idcard), "region ident card" ) == EOF )  {
			printf("\ngetid:  EOF\n");
			return;
		}
		if( idcard[0] == '\n' )
			return;

		if( version == 5 )  {
			reg_num = getint( idcard, 0, 5 );
			id =	getint( idcard, 5, 5 );
			air =	getint( idcard, 10, 5 );
			mat =	getint( idcard, 15, 5 );
			los =	getint( idcard, 20, 5 );
		} else {
			reg_num = getint( idcard, 0, 10 );
			id =	getint( idcard, 10, 10 );
			air =	getint( idcard, 20, 10 );
			mat =	getint( idcard, 74, 3 );
			los =	getint( idcard, 77, 3 );
		}

		if( reg_num <= 0 )  {
			printf("\ngetid:  region_id %d encountered, stoping\n", reg_num);
			return;
		}

		region_register( reg_num, id, air, mat, los );
	}
}

/*
 *			R E G I O N _ R E G I S T E R
 */
void
region_register(int reg_num, int id, int air, int mat, int los)
{
	register struct wmember	*wp;

	wp = &wmp[reg_num];
	if( RT_LIST_IS_EMPTY( &wp->l ) )  {
		if( verbose )  {
			char	paren[32];

			/* Denote an empty region */
			sprintf( paren, "(%s)", wp->wm_name );
			col_pr( paren );
		}
		return;
	}
	mk_lrcomb( outfp, wp->wm_name, wp, 1,
		"", "", (unsigned char *)0, id, air, mat, los, 0 );
		/* Add region to the one group that it belongs to. */
	group_add( id, wp->wm_name );

	if(verbose) col_pr( wp->wm_name );
}

#define NGROUPS	21
struct groups {
	struct wmember	grp_wm;
	int		grp_lo;
	int		grp_hi;
} groups[NGROUPS];
int	ngroups;

void
group_init(void)
{
	group_register( "g00", 0, 0 );
	group_register( "g0", 1, 99 );
	group_register( "g1", 100, 199);
	group_register( "g2", 200, 299 );
	group_register( "g3", 300, 399 );
	group_register( "g4", 400, 499 );
	group_register( "g5", 500, 599 );
	group_register( "g6", 600, 699 );
	group_register( "g7", 700, 799 );
	group_register( "g8", 800, 899 );
	group_register( "g9", 900, 999 );
	group_register( "g10", 1000, 1999 );
	group_register( "g11", 2000, 2999 );
	group_register( "g12", 3000, 3999 );
	group_register( "g13", 4000, 4999 );
	group_register( "g14", 5000, 5999 );
	group_register( "g15", 6000, 6999 );
	group_register( "g16", 7000, 7999 );
	group_register( "g17", 8000, 8999 );
	group_register( "g18", 9000, 9999 );
	group_register( "g19", 10000, 32767 );

}

void
group_register(char *name, int lo, int hi)
{
	char	nbuf[32];
	register struct wmember	*wp;

	if( ngroups >= NGROUPS )  {
		printf("Too many groups, ABORTING\n");
		exit(13);
	}
	wp = &groups[ngroups].grp_wm;

	sprintf( nbuf, "%s%s", name, name_it );
	wp->wm_name = bu_strdup( nbuf );

	RT_LIST_INIT( &wp->l );

	groups[ngroups].grp_lo = lo;
	groups[ngroups].grp_hi = hi;
	ngroups++;
}

void
group_add(register int val, char *name)
{
	register int	i;

	for( i=ngroups-1; i>=0; i-- )  {
		if( val < groups[i].grp_lo )  continue;
		if( val > groups[i].grp_hi )  continue;
		goto add;
	}
	printf("Unable to find group for value %d\n", val);
	i = 0;

add:
	(void)mk_addmember( name, &groups[i].grp_wm.l, NULL, WMOP_UNION );
}

void
group_write(void)
{
	register struct wmember	*wp;
	struct wmember		allhead;
	register int	i;

	RT_LIST_INIT( &allhead.l );

	for( i=0; i < ngroups; i++ )  {
		wp = &groups[i].grp_wm;
		/* Skip empty groups */
		if( RT_LIST_IS_EMPTY( &wp->l ) )  continue;

		/* Make a non-region combination */
		mk_lfcomb( outfp, wp->wm_name, wp, 0 );

		/* Add it to "all.g" */
		(void)mk_addmember( wp->wm_name, &allhead.l, NULL, WMOP_UNION );

		if(verbose) col_pr( wp->wm_name );
	}
	/* Make all-encompasing "all.g" group here */
	mk_lfcomb( outfp, "all.g", &allhead, 0 );
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
