#
/*
 *			S O L I D . C
 *
 *	Conversion subroutine for CVT program, used to convert
 * COMGEOM card decks into GED object files.  This conversion routine
 * is used to translate between COMGEOM object types, and the
 * more general GED object types.
 *
 * Michael Muuss
 * Ballistic Research Laboratory
 * March, 1980
 *
 *		R E V I S I O N   H I S T O R Y
 *
 *	Feb, 1981 Chuck Stanley	Added code to process TORUS (TOR)
 *
 *	02/26/81  MJM	Fixed bug in TORUS code...(PI,PI,PI) was being
 *			added to F1, and put in the file.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "db.h"

extern FILE	*outfp;
extern int	version;

extern double	getdouble();
extern int	sol_total, sol_work;

#define PI	3.14159265358979323846264	/* Approx */

char	scard[132];

trim_trail_spaces( cp )
register char	*cp;
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
getsolid()
{
	char	given_solid_num[16];
	char	solid_type[16];
	int	i;
	double	r1, r2;
	vect_t	work;
	double	m1, m2;		/* Magnitude temporaries */
	char	name[16+2];
	double	dd[4*6];	/* 4 cards of 6 nums each */
	double	tmp[3*8];	/* 8 vectors of 3 nums each */
#define D(_i)	(&(dd[_i*3]))
#define T(_i)	(&(tmp[_i*3]))

	if( (i = getline( scard, sizeof(scard), "solid card" )) == EOF )  {
		printf("getsolid: unexpected EOF\n");
		return( 1 );
	}

	if( version == 5 )  {
		strncpy( given_solid_num, scard+0, 5 );
		given_solid_num[5] = '\0';
		strncpy( solid_type, scard+5, 5 );
		solid_type[5] = '\0';
	} else {
		strncpy( given_solid_num, scard+0, 3 );
		given_solid_num[3] = '\0';
		strncpy( solid_type, scard+3, 7 );
		solid_type[7] = '\0';
	}
	/* Trim trailing spaces */
	trim_trail_spaces( given_solid_num );
	trim_trail_spaces( solid_type );

	/* another solid - increment solid counter
	 * rather than using number from the card, which may go into
	 * pseudo-hex format in version 4 models (due to 3 column limit).
	 */
	sol_work++;

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
				*cp++;
			}
		}
	}

	namecvt( sol_work, name, 's' );
	col_pr( name );

	if( strcmp( solid_type, "ars" ) == 0 )  {
		int		ncurves;
		int		pts_per_curve;
		double		**curve;
		int		ret;

		ncurves = getint( scard, 10, 10 );
		pts_per_curve = getint( scard, 20, 10 );

		/* Allocate curves pointer array */
		if( (curve = (double **)malloc((ncurves+1)*sizeof(double *))) == ((double **)0) )  {
			printf("malloc failure for ARS %d\n", sol_work);
			return(-1);
		}
		/* Allocate space for a curve, and read it in */
		for( i=0; i<ncurves; i++ )  {
			if( (curve[i] = (double *)malloc(
			    (pts_per_curve+1)*3*sizeof(double) )) ==
			    ((double *)0) )  {
				printf("malloc failure for ARS %d curve %d\n",
					sol_work, i);
				return(-1);
			}
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
			free( (char *)curve[i] );
		}
		free( (char *)curve);
		return(ret);
	}

	if( strcmp( solid_type, "rpp" ) == 0 )  {
		double	min[3], max[3];

		if( getsoldata( dd, 2*3, sol_work ) < 0 )
			return(-1);
		VSET( min, dd[0], dd[2], dd[4] );
		VSET( max, dd[1], dd[3], dd[5] );
		return( mk_rpp( outfp, name, min, max ) );
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
		return( mk_arb8( outfp, name, tmp ) );
	}

	if( strcmp( solid_type, "raw" ) == 0 )  {
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
		return( mk_arb8( outfp, name, tmp ) );
	}

	if( strcmp( solid_type, "arb8" ) == 0 )  {
		if( getsoldata( dd, 8*3, sol_work ) < 0 )
			return(-1);
		return( mk_arb8( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "arb7" ) == 0 )  {
		if( getsoldata( dd, 7*3, sol_work ) < 0 )
			return(-1);
		VMOVE( D(7), D(4) );
		return( mk_arb8( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "arb6" ) == 0 )  {
		if( getsoldata( dd, 6*3, sol_work ) < 0 )
			return(-1);
		/* Note that the ordering is important, as data is in D(4), D(5) */
		VMOVE( D(7), D(5) );
		VMOVE( D(6), D(5) );
		VMOVE( D(5), D(4) );
		return( mk_arb8( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "arb5" ) == 0 )  {
		if( getsoldata( dd, 5*3, sol_work ) < 0 )
			return(-1);
		VMOVE( D(5), D(4) );
		VMOVE( D(6), D(4) );
		VMOVE( D(7), D(4) );
		return( mk_arb8( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "arb4" ) == 0 )  {
		if( getsoldata( dd, 4*3, sol_work ) < 0 )
			return(-1);
		return( mk_arb4( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "rcc" ) == 0 )  {
		/* V, H, r */
		if( getsoldata( dd, 2*3+1, sol_work ) < 0 )
			return(-1);
		return( mk_rcc( outfp, name, D(0), D(1), dd[6] ) );
	}

	if( strcmp( solid_type, "rec" ) == 0 )  {
		/* V, H, A, B */
		if( getsoldata( dd, 4*3, sol_work ) < 0 )
			return(-1);
		return( mk_tgc( outfp, name, D(0), D(1),
			D(2), D(3), D(2), D(3) ) );
	}

	if( strcmp( solid_type, "trc" ) == 0 )  {
		/* V, H, r1, r2 */
		if( getsoldata( dd, 2*3+2, sol_work ) < 0 )
			return(-1);
		return( mk_trc( outfp, name, D(0), D(1), dd[6], dd[7] ) );
	}

	if( strcmp( solid_type, "tec" ) == 0 )  {
		/* V, H, A, B, p */
		if( getsoldata( dd, 4*3+1, sol_work ) < 0 )
			return(-1);
		r1 = 1.0/dd[12];	/* P */
		VSCALE( D(4), D(2), r1 );
		VSCALE( D(5), D(3), r1 );
		return( mk_tgc( outfp, name, D(0), D(1),
			D(2), D(3), D(4), D(5) ) );
	}

	if( strcmp( solid_type, "tgc" ) == 0 )  {
		/* V, H, A, B, r1, r2 */
		if( getsoldata( dd, 4*3+2, sol_work ) < 0 )
			return(-1);
		r1 = dd[12] / MAGNITUDE( D(2) );	/* A/|A| * C */
		r2 = dd[13] / MAGNITUDE( D(3) );	/* B/|B| * D */
		VSCALE( D(4), D(2), r1 );
		VSCALE( D(5), D(3), r2 );
		return( mk_tgc( outfp, name, D(0), D(1),
			D(2), D(3), D(4), D(5) ) );
	}

	if( strcmp( solid_type, "sph" ) == 0 )  {
		/* V, radius */
		if( getsoldata( dd, 1*3+2, sol_work ) < 0 )
			return(-1);
		return( mk_sph( outfp, name, D(0), dd[3] ) );
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
		work[0] += PI;
		work[1] += PI;
		work[2] += PI;
		VCROSS( D(2), work, D(1) );
		m1 = r1/MAGNITUDE( D(2) );
		VSCALE( D(2), D(2), m1 );

		VCROSS( D(3), D(1), D(2) );
		m2 = r1/MAGNITUDE( D(3) );
		VSCALE( D(3), D(3), m2 );

		/* Now we have V, A, B, C */
		return( mk_ell( outfp, name, D(0), D(1), D(2), D(3) ) );
	}

	if( version == 5 && strcmp( solid_type, "ellg" ) == 0 )  {
		/* V, A, B, C */
		if( getsoldata( dd, 4*3, sol_work ) < 0 )
			return(-1);
		return( mk_ell( outfp, name, D(0), D(1), D(2), D(3) ) );
	}

	if( strcmp( solid_type, "tor" ) == 0 )  {
		/* V, N, r1, r2 */
		if( getsoldata( dd, 2*3+2, sol_work ) < 0 )
			return(-1);
		return( mk_tor( outfp, name, D(0), D(1), dd[6], dd[7] ) );
	}

	/*
	 *  The solid type string is defective,
	 *  or that solid is not currently supported.
	 */
	printf("getsolid:  no support for solid type '%s'\n", solid_type );
	return(-1);
}

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
getsoldata( dp, num, solid_num )
double	*dp;
int	num;
int	solid_num;
{
	int	cd;
	double	*fp;
	int	i;
	int	j;

	fp = dp;
	for( cd=1; num > 0; cd++ )  {
		if( cd != 1 )  {
			if( getline( scard, sizeof(scard), "solid continuation card" ) == EOF )  {
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
getxsoldata( dp, num, solid_num )
double	*dp;
int	num;
int	solid_num;
{
	int	cd;
	double	*fp;
	int	i;
	int	j;

	fp = dp;
	for( cd=1; num > 0; cd++ )  {
		if( getline( scard, sizeof(scard), "x solid card" ) == EOF )  {
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
