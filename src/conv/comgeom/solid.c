/*                         S O L I D . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file solid.c
 *
 *  Subroutine to convert solids from
 *  COMGEOM card decks into GED object files.  This conversion routine
 *  is used to translate between COMGEOM solids, and the
 *  more general GED solids.
 *
 *  Authors -
 *	Michael John Muuss
 *	Susanne L. Muuss, J.D.
 *
 *  Original Version -
 *	March, 1980
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

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <ctype.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "rtlist.h"
#include "raytrace.h"
#include "wdb.h"
#include "bu.h"

/* defined in read.c */
extern int get_line(register char *cp, int buflen, char *title);
extern int getint(char *cp, int start, int len);
extern void namecvt(register int n, register char **cp, int c);

/* defined in cvt.c */
extern void col_pr(char *str);

/* defined in solid.c */
extern int read_arbn(char *name);

extern struct rt_wdb	*outfp;
extern int	version;
extern int	verbose;

extern double	getdouble(char *cp, int start, int len);
extern int	sol_total, sol_work;

char	scard[132];			/* Solid card buffer area */

void	trim_trail_spaces(register char *cp);
void	eat(int count);

/*
 *			G E T S O L D A T A
 *
 *  Obtain 'num' data items from input card(s).
 *  The first input card is already in global 'scard'.
 *
 *  Returns -
 *	 0	OK
 *	-1	failure
 */
int
getsoldata(double *dp, int num, int solid_num)
{
	int	cd;
	double	*fp;
	int	i;
	int	j;

	fp = dp;
	for( cd=1; num > 0; cd++ )  {
		if( cd != 1 )  {
			if( get_line( scard, sizeof(scard), "solid continuation card" ) == EOF )  {
				printf("too few cards for solid %d\n",
					solid_num);
				return(-1);
			}
			/* continuation card
			 * solid type should be blank
			 */
			if( (version==5 && scard[5] != ' ' ) ||
			    (version==4 && scard[3] != ' ' ) )  {
				printf("solid %d (continuation) card %d non-blank\n",
					solid_num, cd);
				return(-1);
			}
		}

		if( num < 6 )
			j = num;
		else
			j = 6;

		for( i=0; i<j; i++ )  {
			*fp++ = getdouble( scard, 10+i*10, 10 );
		}
		num -= j;
	}
	return(0);
}

/*
 *			G E T X S O L D A T A
 *
 *  Obtain 'num' data items from input card(s).
 *  All input cards must be freshly read.
 *
 *  Returns -
 *	 0	OK
 *	-1	failure
 */
int
getxsoldata(double *dp, int num, int solid_num)
{
	int	cd;
	double	*fp;
	int	i;
	int	j;

	fp = dp;
	for( cd=1; num > 0; cd++ )  {
		if( get_line( scard, sizeof(scard), "x solid card" ) == EOF )  {
			printf("too few cards for solid %d\n",
				solid_num);
			return(-1);
		}
		if( cd != 1 )  {
			/* continuation card
			 * solid type should be blank
			 */
			if( (version==5 && scard[5] != ' ' ) ||
			    (version==4 && scard[3] != ' ' ) )  {
				printf("solid %d (continuation) card %d non-blank\n",
					solid_num, cd);
				return(-1);
			}
		}

		if( num < 6 )
			j = num;
		else
			j = 6;

		for( i=0; i<j; i++ )  {
			*fp++ = getdouble( scard, 10+i*10, 10 );
		}
		num -= j;
	}
	return(0);
}

/*
 *			T R I M _ T R A I L _ S P A C E S
 */
void
trim_trail_spaces(register char *cp)
{
	register char	*ep;

	ep = cp + strlen(cp) - 1;
	while( ep >= cp )  {
		if( *ep != ' ' )  break;
		*ep-- = '\0';
	}
}

/*
 *			G E T S O L I D
 *
 *  Returns -
 *	-1	error
 *	 0	conversion OK
 *	 1	EOF
 */
int
getsolid(void)
{
	char	given_solid_num[16];
	char	solid_type[16];
	int	i;
	double	r1, r2;
	vect_t	work;
	double	m1, m2;		/* Magnitude temporaries */
	char	*name=NULL;
	double	dd[4*6];	/* 4 cards of 6 nums each */
	point_t	tmp[8];		/* 8 vectors of 3 nums each */
	int	ret;
#define D(_i)	(&(dd[_i*3]))
#define T(_i)	(&(tmp[_i][0]))

	if( (i = get_line( scard, sizeof(scard), "solid card" )) == EOF )  {
		printf("getsolid: unexpected EOF\n");
		return( 1 );
	}

	switch( version )  {
	case 5:
		strncpy( given_solid_num, scard+0, 5 );
		given_solid_num[5] = '\0';
		strncpy( solid_type, scard+5, 5 );
		solid_type[5] = '\0';
		break;
	case 4:
		strncpy( given_solid_num, scard+0, 3 );
		given_solid_num[3] = '\0';
		strncpy( solid_type, scard+3, 7 );
		solid_type[7] = '\0';
		break;
	case 1:
		/* DoE/MORSE version, believed to be original MAGIC format */
		strncpy( given_solid_num, scard+5, 4 );
		given_solid_num[4] = '\0';
		strncpy( solid_type, scard+2, 3 );
		break;
	default:
		fprintf(stderr,"getsolid() version %d unimplemented\n", version);
		exit(1);
		break;
	}
	/* Trim trailing spaces */
	trim_trail_spaces( given_solid_num );
	trim_trail_spaces( solid_type );

	/* another solid - increment solid counter
	 * rather than using number from the card, which may go into
	 * pseudo-hex format in version 4 models (due to 3 column limit).
	 */
	sol_work++;
	if( version == 5 )  {
		if( (i = getint( scard, 0, 5 )) != sol_work )  {
			printf("expected solid card %d, got %d, abort\n",
				sol_work, i );
			return(1);
		}
	}

	/* Reduce solid type to lower case */
	{
		register char	*cp;
		register char	c;

		cp = solid_type;
		while( (c = *cp) != '\0' )  {
			if( !isascii(c) )  {
				*cp++ = '?';
			} else if( isupper(c) )  {
				*cp++ = tolower(c);
			} else {
				cp++;
			}
		}
	}

	namecvt( sol_work, &name, 's' );
	if(verbose) col_pr( name );

	if( strcmp( solid_type, "end" ) == 0 )  {
		/* DoE/MORSE version 1 format */
		bu_free( name, "name" );
		return(1);		/* END */
	}

	if( strcmp( solid_type, "ars" ) == 0 )  {
		int		ncurves;
		int		pts_per_curve;
		double		**curve;

		ncurves = getint( scard, 10, 10 );
		pts_per_curve = getint( scard, 20, 10 );

		/* Allocate curves pointer array */
		curve = (double **)bu_malloc((ncurves+1)*sizeof(double *), "curve");

		/* Allocate space for a curve, and read it in */
		for( i=0; i<ncurves; i++ )  {
			curve[i] = (double *)bu_malloc((pts_per_curve+1)*3*sizeof(double), "curve[i]" );

			/* Get data for this curve */
			if( getxsoldata( curve[i], pts_per_curve*3, sol_work ) < 0 )  {
				printf("ARS %d: getxsoldata failed, curve %d\n",
					sol_work, i);
				return(-1);
			}
		}
		if( (ret = mk_ars( outfp, name, ncurves, pts_per_curve, curve )) < 0 )  {
			printf("mk_ars(%s) failed\n", name );
			/* Need to free memory; 'ret' is returned below */
		}

		for( i=0; i<ncurves; i++ )  {
			bu_free( (char *)curve[i], "curve[i]" );
		}
		bu_free( (char *)curve, "curve" );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "rpp" ) == 0 )  {
		double	min[3], max[3];

		if( getsoldata( dd, 2*3, sol_work ) < 0 )
			return(-1);
		VSET( min, dd[0], dd[2], dd[4] );
		VSET( max, dd[1], dd[3], dd[5] );
		ret = mk_rpp( outfp, name, min, max );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "box" ) == 0 )  {
		if( getsoldata( dd, 4*3, sol_work ) < 0 )
			return(-1);
		VMOVE( T(0), D(0) );
		VADD2( T(1), D(0), D(2) );
		VADD3( T(2), D(0), D(2), D(1) );
		VADD2( T(3), D(0), D(1) );

		VADD2( T(4), D(0), D(3) );
		VADD3( T(5), D(0), D(3), D(2) );
		VADD4( T(6), D(0), D(3), D(2), D(1) );
		VADD3( T(7), D(0), D(3), D(1) );
		ret = mk_arb8( outfp, name, &tmp[0][X] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "raw" ) == 0 ||
	    strcmp( solid_type, "wed" ) == 0		/* DoE name */
	)  {
		if( getsoldata( dd, 4*3, sol_work ) < 0 )
			return(-1);
		VMOVE( T(0), D(0) );
		VADD2( T(1), D(0), D(2) );
		VMOVE( T(2), T(1) );
		VADD2( T(3), D(0), D(1) );

		VADD2( T(4), D(0), D(3) );
		VADD3( T(5), D(0), D(3), D(2) );
		VMOVE( T(6), T(5) );
		VADD3( T(7), D(0), D(3), D(1) );
		ret = mk_arb8( outfp, name, &tmp[0][X] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "rvw" ) == 0 )  {
		/* Right Vertical Wedge (Origin: DoE/MORSE) */
		double	a2, theta, phi, h2;
		double	a2theta;
		double	angle1, angle2;
		vect_t	a, b, c;

		if( getsoldata( dd, 1*3+4, sol_work ) < 0 )
			return(-1);
		a2 = dd[3];		/* XY side length */
		theta = dd[4];
		phi = dd[5];
		h2 = dd[6];		/* height in +Z */

		angle1 = (phi+theta-90) * rt_degtorad;
		angle2 = (phi+theta) * rt_degtorad;
		a2theta = a2 * tan(theta * rt_degtorad);

		VSET( a, a2theta*cos(angle1), a2theta*sin(angle1), 0 );
		VSET( b, -a2*cos(angle2), -a2*sin(angle2), 0 );
		VSET( c, 0, 0, h2 );

		VSUB2( T(0), D(0), b );
		VMOVE( T(1), D(0) );
		VMOVE( T(2), D(0) );
		VADD2( T(3), T(0), a );

		VADD2( T(4), T(0), c );
		VADD2( T(5), T(1), c );
		VMOVE( T(6), T(5) );
		VADD2( T(7), T(3), c );
		ret = mk_arb8( outfp, name, &tmp[0][X] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "arw" ) == 0) {
		/* ARbitrary Wedge --- ERIM */
		if( getsoldata( dd, 4*3, sol_work ) < 0)
			return(-1);
		VMOVE( T(0), D(0) );
		VADD2( T(1), D(0), D(2) );
		VADD3( T(2), D(0), D(2), D(3) );
		VADD2( T(3), D(0), D(3) );

		VADD2( T(4), D(0), D(1) );
		VMOVE( T(5), T(4) );

		VADD3( T(6), D(0), D(1), D(3) );
		VMOVE( T(7), T(6) );
		ret = mk_arb8( outfp, name, &tmp[0][X]);
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "arb8" ) == 0 )  {
		if( getsoldata( dd, 8*3, sol_work ) < 0 )
			return(-1);
		ret = mk_arb8( outfp, name, dd );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "arb7" ) == 0 )  {
		if( getsoldata( dd, 7*3, sol_work ) < 0 )
			return(-1);
		VMOVE( D(7), D(4) );
		ret = mk_arb8( outfp, name, dd );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "arb6" ) == 0 )  {
		if( getsoldata( dd, 6*3, sol_work ) < 0 )
			return(-1);
		/* Note that the ordering is important, as data is in D(4), D(5) */
		VMOVE( D(7), D(5) );
		VMOVE( D(6), D(5) );
		VMOVE( D(5), D(4) );
		ret = mk_arb8( outfp, name, dd );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "arb5" ) == 0 )  {
		if( getsoldata( dd, 5*3, sol_work ) < 0 )
			return(-1);
		VMOVE( D(5), D(4) );
		VMOVE( D(6), D(4) );
		VMOVE( D(7), D(4) );
		ret = mk_arb8( outfp, name, dd );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "arb4" ) == 0 )  {
		if( getsoldata( dd, 4*3, sol_work ) < 0 )
			return(-1);
		ret = mk_arb4( outfp, name, dd );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "rcc" ) == 0 )  {
		/* V, H, r */
		if( getsoldata( dd, 2*3+1, sol_work ) < 0 )
			return(-1);
		ret = mk_rcc( outfp, name, D(0), D(1), dd[6] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "rec" ) == 0 )  {
		/* V, H, A, B */
		if( getsoldata( dd, 4*3, sol_work ) < 0 )
			return(-1);
		ret = mk_tgc( outfp, name, D(0), D(1),
			      D(2), D(3), D(2), D(3) );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "trc" ) == 0 )  {
		/* V, H, r1, r2 */
		if( getsoldata( dd, 2*3+2, sol_work ) < 0 )
			return(-1);
		ret = mk_trc_h( outfp, name, D(0), D(1), dd[6], dd[7] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "tec" ) == 0 )  {
		/* V, H, A, B, p */
		if( getsoldata( dd, 4*3+1, sol_work ) < 0 )
			return(-1);
		r1 = 1.0/dd[12];	/* P */
		VSCALE( D(4), D(2), r1 );
		VSCALE( D(5), D(3), r1 );
		ret = mk_tgc( outfp, name, D(0), D(1),
			      D(2), D(3), D(4), D(5) );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "tgc" ) == 0 )  {
		/* V, H, A, B, r1, r2 */
		if( getsoldata( dd, 4*3+2, sol_work ) < 0 )
			return(-1);
		r1 = dd[12] / MAGNITUDE( D(2) );	/* A/|A| * C */
		r2 = dd[13] / MAGNITUDE( D(3) );	/* B/|B| * D */
		VSCALE( D(4), D(2), r1 );
		VSCALE( D(5), D(3), r2 );
		ret = mk_tgc( outfp, name, D(0), D(1),
			D(2), D(3), D(4), D(5) );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "sph" ) == 0 )  {
		/* V, radius */
		if( getsoldata( dd, 1*3+1, sol_work ) < 0 )
			return(-1);
		ret = mk_sph( outfp, name, D(0), dd[3] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strncmp( solid_type, "wir", 3 ) == 0 )  {
		int			numpts;		/* points per wire */
		int			num;
		int			i;
		double			dia;
		double			*pts;		/* 3 entries per pt */
		struct	wdb_pipept	*ps;
		struct	bu_list		head;		/* allow a whole struct for head */

		/* This might be getint( solid_type, 3, 2 ); for non-V5 */
		numpts = getint( scard, 8, 2 );
		num = numpts * 3 + 1;			/* 3 entries per pt */

		/* allocate space for the points array */
		pts = ( double *)bu_malloc(num * sizeof( double), "pts" );

		if( getsoldata( pts, num, sol_work ) < 0 )  {
			return(-1);
		}
		dia = pts[num-1] * 2.0;	/* radius X 2.0 == diameter */

		/* allocate nodes on a list and store all information in
		 * the appropriate location.
		 */
		RT_LIST_INIT( &head );
		for( i = 0; i < numpts; i++ )  {
			/* malloc a new structure */
			ps = (struct wdb_pipept *)bu_malloc(sizeof( struct wdb_pipept), "ps");

			ps->l.magic = WDB_PIPESEG_MAGIC;
			VMOVE( ps->pp_coord, &pts[i*3]);	/* 3 pts at a time */
			ps->pp_id = 0;				/* solid */
			ps->pp_od = dia;
			ps->pp_bendradius = dia;
			RT_LIST_INSERT( &head, &ps->l );
		}

		if( mk_pipe( outfp, name, &head ) < 0 )
			return(-1);
		mk_pipe_free( &head );
		bu_free( name, "name" );
		return(0);		/* OK */
	}

	if( strcmp( solid_type, "rpc" ) == 0 )  {
		/* V, H, B, r */
		if( getsoldata( dd, 3*3+1, sol_work ) < 0 )
			return(-1);
		ret = mk_rpc( outfp, name, D(0), D(1),
			D(2), dd[9] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "rhc" ) == 0 )  {
		/* V, H, B, r, c */
		if( getsoldata( dd, 3*3+2, sol_work ) < 0 )
			return(-1);
		ret = mk_rhc( outfp, name, D(0), D(1),
			D(2), dd[9], dd[10] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "epa" ) == 0 )  {
		/* V, H, Au, r1, r2 */
		if( getsoldata( dd, 3*3+2, sol_work ) < 0 )
			return(-1);
		ret = mk_epa( outfp, name, D(0), D(1),
			D(2), dd[9], dd[10] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "ehy" ) == 0 )  {
		/* V, H, Au, r1, r2, c */
		if( getsoldata( dd, 3*3+3, sol_work ) < 0 )
			return(-1);
		ret = mk_ehy( outfp, name, D(0), D(1),
			D(2), dd[9], dd[10], dd[11] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "eto" ) == 0 )  {
		/* V, N, C, r, rd */
		if( getsoldata( dd, 3*3+2, sol_work ) < 0 )
			return(-1);
		ret = mk_eto( outfp, name, D(0), D(1),
			D(2), dd[9], dd[10] );
		bu_free( name, "name" );
		return(ret);
	}


	if( version <= 4 && strcmp( solid_type, "ell" ) == 0 )  {
		/* Foci F1, F2, major axis length L */
		vect_t	v;

		/*
		 * For simplicity, we convert ELL to ELL1, then
		 * fall through to ELL1 code.
		 * Format of ELL is F1, F2, len
		 * ELL1 format is V, A, r
		 */
		if( getsoldata( dd, 2*3+1, sol_work ) < 0 )
			return(-1);
		VADD2SCALE( v, D(0), D(1), 0.5 ); /* V is midpoint */

		VSUB2( work, D(1), D(0) );	/* work holds F2 -  F1 */
		m1 = MAGNITUDE( work );
		r2 = 0.5 * dd[6] / m1;
		VSCALE( D(1), work, r2 );	/* A */

		dd[6] = sqrt( MAGSQ( D(1) ) -
			(m1 * 0.5)*(m1 * 0.5) );	/* r */
		VMOVE( D(0), v );
		goto ell1;
	}

	if( (version == 5 && strcmp( solid_type, "ell" ) == 0)  ||
	    strcmp( solid_type, "ell1" ) == 0 )  {
		/* V, A, r */
		/* GIFT4 name is "ell1", GIFT5 name is "ell" */
		if( getsoldata( dd, 2*3+1, sol_work ) < 0 )
			return(-1);

ell1:
		r1 = dd[6];		/* R */
		VMOVE( work, D(0) );
		work[0] += rt_pi;
		work[1] += rt_pi;
		work[2] += rt_pi;
		VCROSS( D(2), work, D(1) );
		m1 = r1/MAGNITUDE( D(2) );
		VSCALE( D(2), D(2), m1 );

		VCROSS( D(3), D(1), D(2) );
		m2 = r1/MAGNITUDE( D(3) );
		VSCALE( D(3), D(3), m2 );

		/* Now we have V, A, B, C */
		ret = mk_ell( outfp, name, D(0), D(1), D(2), D(3) );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "ellg" ) == 0 )  {
		/* V, A, B, C */
		if( getsoldata( dd, 4*3, sol_work ) < 0 )
			return(-1);
		ret = mk_ell( outfp, name, D(0), D(1), D(2), D(3) );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "tor" ) == 0 )  {
		/* V, N, r1, r2 */
		if( getsoldata( dd, 2*3+2, sol_work ) < 0 )
			return(-1);
		ret = mk_tor( outfp, name, D(0), D(1), dd[6], dd[7] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "haf" ) == 0 )  {
		/* N, d */
		if( getsoldata( dd, 1*3+1, sol_work ) < 0 )
			return(-1);
		ret = mk_half( outfp, name, D(0), -dd[3] );
		bu_free( name, "name" );
		return(ret);
	}

	if( strcmp( solid_type, "arbn" ) == 0 )  {
		ret = read_arbn( name );
		bu_free( name, "name" );
	}

	/*
	 *  The solid type string is defective,
	 *  or that solid is not currently supported.
	 */
	printf("getsolid:  no support for solid type '%s'\n", solid_type );
	return(-1);
}

int
read_arbn(char *name)
{
	int	npt;			/* # vertex pts to be read in */
	int	npe;			/* # planes from 3 vertex points */
	int	neq;			/* # planes from equation */
	int	nae;			/* # planes from az,el & vertex index */
	int	nface;			/* total number of faces */
	double	*input_points = (double *)0;
	double	*vertex = (double *)0;	/* vertex list of final solid */
	int	last_vertex;		/* index of first unused vertex */
	int	max_vertex;		/* size of vertex array */
	int	*used = (int *)0;	/* plane eqn use count */
	plane_t	*eqn = (plane_t *)0;	/* plane equations */
	int	cur_eq = 0;		/* current (free) equation number */
	int	symm = 0;		/* symmetry about Y used */
	register int	i;
	int	j;
	int	k;
	register int	m;
	point_t	cent;			/* centroid of arbn */
	struct rt_tol	tol;

	/* XXX The tolerance here is sheer guesswork */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	npt = getint( scard, 10+0*10, 10 );
	npe = getint( scard, 10+1*10, 10 );
	neq = getint( scard, 10+2*10, 10 );
	nae = getint( scard, 10+3*10, 10 );

	nface = npe + neq + nae;
	if( npt < 1 )  {
		/* Having one point is necessary to compute centroid */
		printf("arbn defined without at least one point\n");
bad:
		if(npt>0) eat( (npt+1)/2 );	/* vertex input_points */
		if(npe>0) eat( (npe+5)/6 );	/* vertex pt index numbers */
		if(neq>0) eat( neq );		/* plane eqns? */
		if(nae>0) eat( (nae+1)/2 );	/* az el & vertex index? */
		return(-1);
	}

	/* Allocate storage for plane equations */
	eqn = (plane_t *)bu_malloc(nface*sizeof(plane_t), "eqn");

	/* Allocate storage for per-plane use count */
	used = (int *)bu_malloc(nface*sizeof(int), "used");

	if( npt >= 1 )  {
		/* Obtain vertex input_points */
		input_points = (double *)bu_malloc(npt*3*sizeof(double), "input_points");

		if( getxsoldata( input_points, npt*3, sol_work ) < 0 )
			goto bad;
	}

	/* Get planes defined by three points, 6 per card */
	for( i=0; i<npe; i += 6 )  {
		if( get_line( scard, sizeof(scard), "arbn vertex point indices" ) == EOF )  {
			printf("too few cards for arbn %d\n",
				sol_work);
			return(-1);
		}
		for( j=0; j<6; j++ )  {
			int	q,r,s;
			point_t	a,b,c;

			q = getint( scard, 10+j*10+0, 4 );
			r = getint( scard, 10+j*10+4, 3 );
			s = getint( scard, 10+j*10+7, 3 );

			if( q == 0 || r == 0 || s == 0 ) continue;

			if( q < 0 )  {
				VMOVE( a, &input_points[((-q)-1)*3] );
				a[Y] = -a[Y];
				symm = 1;
			} else {
				VMOVE( a, &input_points[((q)-1)*3] );
			}

			if( r < 0 )  {
				VMOVE( b, &input_points[((-r)-1)*3] );
				b[Y] = -b[Y];
				symm = 1;
			} else {
				VMOVE( b, &input_points[((r)-1)*3] );
			}

			if( s < 0 )  {
				VMOVE( c, &input_points[((-s)-1)*3] );
				c[Y] = -c[Y];
				symm = 1;
			} else {
				VMOVE( c, &input_points[((s)-1)*3] );
			}
			if( rt_mk_plane_3pts( eqn[cur_eq], a,b,c, &tol ) < 0 )  {
				printf("arbn degenerate plane\n");
				VPRINT("a", a);
				VPRINT("b", b);
				VPRINT("c", c);
				continue;
			}
			cur_eq++;
		}
	}

	/* Get planes defined by their equation */
	for( i=0; i < neq; i++ )  {
		register double	scale;
		if( get_line( scard, sizeof(scard), "arbn plane equation card" ) == EOF )  {
			printf("too few cards for arbn %d\n",
				sol_work);
			return(-1);
		}
		eqn[cur_eq][0] = getdouble( scard, 10+0*10, 10 );
		eqn[cur_eq][1] = getdouble( scard, 10+1*10, 10 );
		eqn[cur_eq][2] = getdouble( scard, 10+2*10, 10 );
		eqn[cur_eq][3] = getdouble( scard, 10+3*10, 10 );
		scale = MAGNITUDE(eqn[cur_eq]);
		if( scale < SMALL )  {
			printf("arbn plane normal too small\n");
			continue;
		}
		scale = 1/scale;
		VSCALE( eqn[cur_eq], eqn[cur_eq], scale );
		eqn[cur_eq][3] *= scale;
		cur_eq++;
	}

	/* Get planes defined by azimuth, elevation, and pt, 2 per card */
	for( i=0; i < nae;  i += 2 )  {
		if( get_line( scard, sizeof(scard), "arbn az/el card" ) == EOF )  {
			printf("too few cards for arbn %d\n",
				sol_work);
			return(-1);
		}
		for( j=0; j<2; j++ )  {
			double	az, el;
			int	vert_no;
			double	cos_el;
			point_t	pt;

			az = getdouble( scard, 10+j*30+0*10, 10 ) * rt_degtorad;
			el = getdouble( scard, 10+j*30+1*10, 10 ) * rt_degtorad;
			vert_no = getint( scard, 10+j*30+2*10, 10 );
			if( vert_no == 0 )  break;
			cos_el = cos(el);
			eqn[cur_eq][X] = cos(az)*cos_el;
			eqn[cur_eq][Y] = sin(az)*cos_el;
			eqn[cur_eq][Z] = sin(el);

			if( vert_no < 0 )  {
				VMOVE( pt, &input_points[((-vert_no)-1)*3] );
				pt[Y] = -pt[Y];
			} else {
				VMOVE( pt, &input_points[((vert_no)-1)*3] );
			}
			eqn[cur_eq][3] = VDOT(pt, eqn[cur_eq]);
			cur_eq++;
		}
	}
	if( nface != cur_eq )  {
		printf("arbn expected %d faces, got %d\n", nface, cur_eq);
		return(-1);
	}

	/* Average all given points together to find centroid */
	/* This is why there must be at least one (two?) point given */
	VSETALL(cent, 0);
	for( i=0; i<npt; i++ )  {
		VADD2( cent, cent, &input_points[i*3] );
	}
	VSCALE( cent, cent, 1.0/npt );
	if( symm )  cent[Y] = 0;

	/* Point normals away from centroid */
	for( i=0; i<nface; i++ )  {
		double	dist;

		dist = VDOT( eqn[i], cent ) - eqn[i][3];
		/* If dist is negative, 'cent' is inside halfspace */
#define DIST_TOL	(1.0e-8)
#define DIST_TOL_SQ	(1.0e-10)
		if( dist < -DIST_TOL )  continue;
		if( dist > DIST_TOL )  {
			/* Flip halfspace over */
			VREVERSE( eqn[i], eqn[i] );
			eqn[i][3] = -eqn[i][3];
		} else {
			/* Centroid lies on this face */
			printf("arbn centroid lies on face\n");
			return(-1);
		}

	}

	/* Release storage for input points */
	bu_free( (char *)input_points, "input_points" );
	input_points = (double *)0;


	/*
	 *  ARBN must be convex.  Test for concavity.
	 *  Byproduct is an enumeration of all the verticies.
	 */
	last_vertex = max_vertex = 0;

	/* Zero face use counts */
	for( i=0; i<nface; i++ )  {
		used[i] = 0;
	}
	for( i=0; i<nface-2; i++ )  {
		for( j=i+1; j<nface-1; j++ )  {
			double	dot;
			int	point_count;	/* # points on this line */

			/* If normals are parallel, no intersection */
			dot = VDOT( eqn[i], eqn[j] );
			if( !NEAR_ZERO( dot, 0.999999 ) )  continue;

			point_count = 0;
			for( k=j+1; k<nface; k++ )  {
				point_t	pt;

				if( rt_mkpoint_3planes( pt, eqn[i], eqn[j], eqn[k] ) < 0 )  continue;

				/* See if point is outside arb */
				for( m=0; m<nface; m++ )  {
					if( i==m || j==m || k==m )  continue;
					if( VDOT(pt, eqn[m])-eqn[m][3] > DIST_TOL )
						goto next_k;
				}
				/* See if vertex already was found */
				for( m=0; m<last_vertex; m++ )  {
					vect_t	dist;
					VSUB2( dist, pt, &vertex[m*3] );
					if( MAGSQ(dist) < DIST_TOL_SQ )
						goto next_k;
				}

				/*
				 *  Add point to vertex array.
				 *  If more room needed, realloc.
				 */
				if( last_vertex >= max_vertex )  {
					if( max_vertex == 0 )   {
						max_vertex = 3;
						vertex = (double *)bu_malloc( max_vertex*3*sizeof(double), "vertex" );
					} else {
						max_vertex *= 10;
						vertex = (double *)bu_realloc( (char *)vertex, max_vertex*3*sizeof(double), "vertex" );
					}
				}

				VMOVE( &vertex[last_vertex*3], pt );
				last_vertex++;
				point_count++;

				/* Increment "face used" counts */
				used[i]++;
				used[j]++;
				used[k]++;
next_k:				;
			}
			if( point_count > 2 )  {
				printf("arbn: warning, point_count on line=%d\n", point_count);
			}
		}
	}

	/* If any planes were not used, then arbn is not convex */
	for( i=0; i<nface; i++ )  {
		if( used[i] != 0 )  continue;	/* face was used */
		printf("arbn face %d unused, solid is not convex\n", i);
		return(-1);
	}

	/* Write out the solid ! */
	i = mk_arbn( outfp, name, nface, eqn );

	if( input_points )  bu_free( (char *)input_points, "input_points" );
	if( vertex )  bu_free( (char *)vertex, "vertex" );
	if( eqn )  bu_free( (char *)eqn, "eqn" );
	if( used )  bu_free( (char *)used, "used" );

	return(i);
}

/*
 *			E A T
 *
 *  Eat the indicated number of input lines
 */
void
eat(int count)
{
	char	lbuf[132];
	int	i;

	for( i=0; i<count; i++ )  {
		(void)get_line( lbuf, sizeof(lbuf), "eaten card" );
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
