/*
 *			R E G I O N . C
 */
#include <stdio.h>
#include <ctype.h>

#include "machine.h"
#include "vmath.h"
#include "wdb.h"

extern char	name_it[];

extern struct wmember	*wmp;	/* array indexed by region number */

extern FILE	*infp;
extern FILE	*outfp;

extern int	reg_total;
extern int	version;
extern int	verbose;

struct rcard  {
	char	rc_num[5];
	char	rc_null;
	struct	rcfields  {
		char	rcf_null;
		char	rcf_or;
		char	rcf_solid[5];
	}  
	rc_fields[9];
	char	rc_remark[11+3+100];
} rcard;

/*
 *			G E T R E G I O N
 *
 *  Use wmp[region_number] as head for each region.
 */
getregion()
{
	int i, j;
	int card;
	int	op;
	int reg_reg_flag;
	int	reg_num;
	char	inst_name[32];
	int	inst_num;
	char *cp;

	reg_num = 0;		/* safety */

	/* Pre-load very first region card */
	if( getline( &rcard, sizeof(rcard), "region card" ) == EOF )  {
		printf("getregion: premature EOF\n");
		return( -1 );
	}
	if( getint( &rcard, 0, 5 ) != 1 )  {
		printf("First region card not #1\ncard='%s'\n", &rcard);
		return(-1);
	}

top:
	reg_reg_flag = 0;

	for( card=0; ; card++ )  {
		if( card == 0 )  {
			/* First card is already in input buffer */
			reg_num = getint( &rcard, 0, 5 );

			/* -1 region number terminates table */
			if( reg_num < 0 ) 
				return( 0 );		/* Done */

			if( reg_num > reg_total )  {
				printf("%d regions is more than claimed %d\n",
					reg_num, reg_total );
				return(-1);
			}

			namecvt( reg_num, wmp[reg_num].wm_name, 'r' );
		} else {
			if( getline( &rcard, sizeof(rcard), "region card" ) == EOF )  {
				printf("getregion: premature EOF\n");
				return( -1 );
			}
			if( getint( &rcard, 0, 5 ) != 0 )  {
				/* finished with this region */
				break;
			}
		}

		cp = (char *) rcard.rc_fields;

		/* Scan each of the 9 fields on the card */
		for( i=0; i<9; i++ )  {
			struct wmember	*membp;

			cp[7] = 0;	/* clobber succeeding 'O' pos */

			/* check for "-    5" field which atoi will
			 *	return a zero
			 */
			if(cp[2] == '-' || cp[2] == '+') {
				/* remove any followin blanks */
				for(j=3; j<6; j++) {
					if(cp[j] == ' ') {
						cp[j] = cp[j-1];
						cp[j-1] = ' ';
					}
				}
			}

			inst_num = atoi( cp+2 );

			/* Check for null field -- they are to be skipped */
			if( inst_num == 0 )  {
				cp += 7;
				continue;	/* zeros are allowed as placeholders */
			}

			if( version == 5 )  {
				/* Region references region in Gift5 */
				if(rcard.rc_fields[i].rcf_or == 'g' ||
				   rcard.rc_fields[i].rcf_or == 'G')
					reg_reg_flag = 1;

				if( cp[1] == 'R' || cp[1] == 'r' ) 
					op = WMOP_UNION;
				else {
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
				namecvt(inst_num, inst_name, 'r');
			else
				namecvt( inst_num, inst_name, 's' );
			reg_reg_flag = 0;

			membp = mk_addmember( inst_name, &wmp[reg_num] );
			membp->wm_op = op;

			cp += 7;
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
getid()
{
	register int	i;
	int reg_num;
	int id;
	int air;
	int mat= -1;
	int los= -2;
	char buff[11];
	int	buflen;
	register struct wmember	*wp;
	char	idcard[132];

	while(1)  {
		if( getline( idcard, sizeof(idcard), "region ident card" ) == EOF )  {
			printf("\ngetid:  EOF\n");
			return(0);
		}
		if( idcard[0] == '\n' )
			return( 0 );

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
			return(0);
		}
#if 0
printf("reg_num=%d,id=%d,air=%d,mat=%d,los=%d\n", reg_num,id,air,mat,los);
#endif

		wp = &wmp[reg_num];
		if( wp->wm_forw == wp )  {
			char	paren[32];
			if( !verbose)  continue;
			/* Denote an empty region */
			sprintf( paren, "(%s)", wp->wm_name );
			col_pr( paren );
			continue;
		}

		mk_lrcomb( outfp, wp->wm_name, wp, reg_num,
			"", "", (char *)0, id, air, mat, los, 0 );

		/* Add region to the one group that it belongs to. */
		group_add( id, wp->wm_name );

		if(verbose) col_pr( wp->wm_name );
	}
}

#define NGROUPS	21
struct groups {
	struct wmember	grp_wm;
	int		grp_lo;
	int		grp_hi;
} groups[NGROUPS];
int	ngroups;

group_init()
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

group_register( name, lo, hi )
char	*name;
{
	char	nbuf[32];
	register struct wmember	*wp;

	if( ngroups >= NGROUPS )  {
		printf("Too many groups, ABORTING\n");
		exit(13);
	}
	wp = &groups[ngroups].grp_wm;

	sprintf( nbuf, "%s%s", name, name_it );
	strncpy( wp->wm_name, nbuf, sizeof(wp->wm_name) );

	wp->wm_forw = wp->wm_back = wp;

	groups[ngroups].grp_lo = lo;
	groups[ngroups].grp_hi = hi;
	ngroups++;
}

group_add( val, name )
register int	val;
char		*name;
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
	(void)mk_addmember( name, &groups[i].grp_wm );
}

group_write()
{
	register struct wmember	*wp;
	register int	i;

	for( i=0; i < ngroups; i++ )  {
		wp = &groups[i].grp_wm;
		/* Skip empty groups */
		if( wp->wm_forw == wp )  continue;

		/* Make a non-region combination */
		mk_lfcomb( outfp, wp->wm_name, wp, 0 );

		if(verbose) col_pr( wp->wm_name );
	}
	/* Could make all-encompasing "all.g" group here */
}
