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


struct scard {
	char	sc_num_and_type[10];
	char	sc_fields[6][10];
	char	sc_remark[16];
} scard;

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
 *	0	done, or EOF
 *	1	non-ARS done
 *	2	ARS done
 */
getsolid()
{
	char	given_solid_num[16];
	char	solid_type[16];
	int	i;

	if( sol_work == sol_total )	/* processed all solids */
		return( 0 );

	if( (i = getline( &scard, sizeof(scard), "solid card" )) == EOF )  {
		printf("getsolid: unexpected EOF\n");
		return( 0 );
	}

	if( version == 5 )  {
		strncpy( given_solid_num, ((char *)&scard)+0, 5 );
		given_solid_num[5] = '\0';
		strncpy( solid_type, ((char *)&scard)+5, 5 );
		solid_type[5] = '\0';
	} else {
		strncpy( given_solid_num, ((char *)&scard)+0, 3 );
		given_solid_num[3] = '\0';
		strncpy( solid_type, ((char *)&scard)+3, 7 );
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

	if( strcmp( solid_type, "ars" ) == 0 )  {
		static int j,n,jj,m,mx;
		static float *fp;
		int cd,cds;
		int	ncurves;
		int	pts_per_curve;
		int	Nb_strcx,Nb_strsl,nst,structn,rmcx,cdcx;
		static char nbuf[6];
		static float ibuf[10];
		static float jbuf[10];
		struct ars_rec *arsap;
		union record *b;
		union record	rec;
		double		**curve;


		/*
		 * PROCESS ARS SOLID
		 */
		arsap = &rec.a;
		arsap->a_id = ID_ARS_A;
		arsap->a_type = ARS;
		namecvt( sol_work, arsap->a_name, 's');
		col_pr( arsap->a_name );

		/* ncurves is # of curves, pts_per_curve is # of points per curve */
		ncurves = getint( &scard, 10, 10 );
		pts_per_curve = getint( &scard, 20, 10 );
		arsap->a_m = ncurves;
		arsap->a_n = pts_per_curve;

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
		if( mk_ars( outfp, arsap->a_name, ncurves, pts_per_curve, curve ) < 0 )  {
			printf("mk_ars(%s) failed\n", arsap->a_name );
			return(-1);
		}

		/* Free memory */
		for( i=0; i<ncurves; i++ )  {
			free( (char *)curve[i] );
		}
		free( (char *)curve);
		/* DONE */
#if 0
		/*
		 *   the number of b type structures needed to store 1
		 * cross section is equal to pts_per_curve/8;  if pts_per_curve modulo 8 is not
		 * equal to 0 then 1 more structure is needed.
		 */
		Nb_strcx = pts_per_curve/8;
		if(( pts_per_curve % 8) !=0)
			Nb_strcx++;
		rmcx = pts_per_curve % 8;

		/*
		 * (8 - rmcx) required # of unused storage locations for the
		 * last structure associated with a cross section.
		 */
		arsap->a_curlen=Nb_strcx;

		/*
		 * number of b type structures calculated above multiplied
		 * by the total number of cross sections equals the total
		 * amount of storage required.
		 */
		Nb_strsl = Nb_strcx * ncurves;
		arsap->a_totlen = Nb_strsl;

		/*
		 * the number of b type structures is used here
		 * to allocate proper storage dynamically
		 */
		if( (b =
		    (union record *)
		    malloc( (unsigned) Nb_strsl*sizeof(union record) )
		    ) == (union record *) NULL
			) {
			(void) fprintf( stderr, "Getsolid(): Malloc() failed!\n" );
			exit( 1 );
		}

		/*    number of data cards per cross section calculated here */
		cdcx = (pts_per_curve+1)/2;

		/* calculate number of ARS data cards that have to be read in */
		/*   as given in GIFT users manual  */
		cd = ncurves*(cdcx);

		/*  2 sets of xyz coordinates for every data card
		 *  set up counters to read in coordinate points and load into 
		 *       b structures created above
		 */
		cds = 0;	/* number of data cards read in (1-> cd) */
		nst = -1;	/* b type structure number currently loaded */

		structn = 0;	/* granule # within a cross secton  */
		for(m=1; m < (ncurves+1); m++)  {
			structn=0;
			for(n=1; n <(cdcx+1); n++)  {
				if( getline(&scard, sizeof(scard), "ars solid card") == EOF)
					{
					printf("read of ARS granule failed\n");
					return	0;
					}
				cds++;
				if((j=(n % 4)) == 1)  {
					structn++;   /* increment granule number   */
					nst++;    /*increment structure number */
					b[nst].b.b_id = ID_ARS_B;
					b[nst].b.b_type = ARSCONT;
					b[nst].b.b_n = m;  /*  save cross section number  */
					b[nst].b.b_ngranule = structn;  /* save granule number  */
				}
				for(i=5; i>=0; i--) {
					scard.sc_fields[i][10] = '\0';
					ibuf[i] = atof(scard.sc_fields[i]);
				}
				if(cds == 1){ /* save 1st point coordinate */
					for(i=0; i<3; i++)
						jbuf[i] = ibuf[i];
				}
				switch( j ) {
				case 1:
					b[nst].b.b_values[0]=ibuf[0];
					b[nst].b.b_values[1]=ibuf[1];
					b[nst].b.b_values[2]=ibuf[2];
					b[nst].b.b_values[3]=ibuf[3];
					b[nst].b.b_values[4]=ibuf[4];
					b[nst].b.b_values[5]=ibuf[5];
					break;
				case 2:
					b[nst].b.b_values[6]=ibuf[0];
					b[nst].b.b_values[7]=ibuf[1];
					b[nst].b.b_values[8]=ibuf[2];
					b[nst].b.b_values[9]=ibuf[3];
					b[nst].b.b_values[10]=ibuf[4];
					b[nst].b.b_values[11]=ibuf[5];
					break;
				case 3:
					b[nst].b.b_values[12]=ibuf[0];
					b[nst].b.b_values[13]=ibuf[1];
					b[nst].b.b_values[14]=ibuf[2];
					b[nst].b.b_values[15]=ibuf[3];
					b[nst].b.b_values[16]=ibuf[4];
					b[nst].b.b_values[17]=ibuf[5];
					break;
				case 0:
					b[nst].b.b_values[18]=ibuf[0];
					b[nst].b.b_values[19]=ibuf[1];
					b[nst].b.b_values[20]=ibuf[2];
					b[nst].b.b_values[21]=ibuf[3];
					b[nst].b.b_values[22]=ibuf[4];
					b[nst].b.b_values[23]=ibuf[5];

				}
			}
		}

		/*    subtract base vector from  each vector in description */
		for(j=0; j<Nb_strsl; j++)  {
			for(i=0; i<8; i++){
				b[j].b.b_values[(i*3)] -= jbuf[0];
				b[j].b.b_values[(i*3)+1] -= jbuf[1];
				b[j].b.b_values[(i*3)+2] -= jbuf[2];
			}
		}

		/* restore base vector to original value */
		b[0].b.b_values[0] = jbuf[0];
		b[0].b.b_values[1] = jbuf[1];
		b[0].b.b_values[2] = jbuf[2];

		/*  write out A and B type records  */
		fwrite( arsap, sizeof(union record), 1, outfp );
		fwrite( &b[0], sizeof(union record), Nb_strsl, outfp );

		/* free dynamic storage alloated for ARS */
		free (b);
		return(2);
#endif
	}  else   {
		/* solid type other than ARS */
		convert( sol_work, solid_type );

		return(1);		/* input is valid */
	}
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
			if( getline( &scard, sizeof(scard), "solid continuation card" ) == EOF )  {
				printf("too few cards for solid %d\n",
					solid_num);
				return(-1);
			}
			/* continuation card
			 * solid type should be blank 
			 */
			if( (version==5 && ((char *)&scard)[5] != ' ' ) ||
			    (version==4 && ((char *)&scard)[3] != ' ' ) )  {
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
			*fp++ = getdouble( &scard, 10+i*10, 10 );
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
		if( getline( &scard, sizeof(scard), "x solid card" ) == EOF )  {
			printf("too few cards for solid %d\n",
				solid_num);
			return(-1);
		}
		if( cd != 1 )  {
			/* continuation card
			 * solid type should be blank 
			 */
			if( (version==5 && ((char *)&scard)[5] != ' ' ) ||
			    (version==4 && ((char *)&scard)[3] != ' ' ) )  {
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
			*fp++ = getdouble( &scard, 10+i*10, 10 );
		}
		num -= j;
	}
	return(0);
}

/*
 *			C O N V E R T
 *
 *  This routine is expected to write the records out itself.
 *  The first card has already been read into 'scard'.
 */
convert( sol_num, solid_type )
int	sol_num;
char	*solid_type;
{
	double	r1, r2;
	vect_t	work;
	double	m1, m2;		/* Magnitude temporaries */
	static int i;
	char	name[16+2];
	double	dd[4*6];	/* 4 cards of 6 nums each */
	double	tmp[3*8];	/* 8 vectors of 3 nums each */

	namecvt( sol_num, name, 's' );
	col_pr( name );

#define D(_i)	(&(dd[_i*3]))
#define T(_i)	(&(tmp[_i*3]))

	if( strcmp( solid_type, "rpp" ) == 0 )  {
		double	min[3], max[3];

		if( getsoldata( dd, 2*3, sol_num ) < 0 )
			return(-1);
		VSET( min, dd[0], dd[2], dd[4] );
		VSET( max, dd[1], dd[3], dd[5] );
		return( mk_rpp( outfp, name, min, max ) );
	}

	if( strcmp( solid_type, "box" ) == 0 )  {
		if( getsoldata( dd, 4*3, sol_num ) < 0 )
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
		if( getsoldata( dd, 4*3, sol_num ) < 0 )
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
		if( getsoldata( dd, 8*3, sol_num ) < 0 )
			return(-1);
		return( mk_arb8( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "arb7" ) == 0 )  {
		if( getsoldata( dd, 7*3, sol_num ) < 0 )
			return(-1);
		VMOVE( D(7), D(4) );
		return( mk_arb8( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "arb6" ) == 0 )  {
		if( getsoldata( dd, 6*3, sol_num ) < 0 )
			return(-1);
		/* Note that the ordering is important, as data is in D(4), D(5) */
		VMOVE( D(7), D(5) );
		VMOVE( D(6), D(5) );
		VMOVE( D(5), D(4) );
		return( mk_arb8( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "arb5" ) == 0 )  {
		if( getsoldata( dd, 5*3, sol_num ) < 0 )
			return(-1);
		VMOVE( D(5), D(4) );
		VMOVE( D(6), D(4) );
		VMOVE( D(7), D(4) );
		return( mk_arb8( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "arb4" ) == 0 )  {
		if( getsoldata( dd, 4*3, sol_num ) < 0 )
			return(-1);
		return( mk_arb4( outfp, name, dd ) );
	}

	if( strcmp( solid_type, "rcc" ) == 0 )  {
		/* V, H, r */
		if( getsoldata( dd, 2*3+1, sol_num ) < 0 )
			return(-1);
		return( mk_rcc( outfp, name, D(0), D(1), dd[6] ) );
	}

	if( strcmp( solid_type, "rec" ) == 0 )  {
		/* V, H, A, B */
		if( getsoldata( dd, 4*3, sol_num ) < 0 )
			return(-1);
		return( mk_tgc( outfp, name, D(0), D(1),
			D(2), D(3), D(2), D(3) ) );
	}

	if( strcmp( solid_type, "trc" ) == 0 )  {
		/* V, H, r1, r2 */
		if( getsoldata( dd, 2*3+2, sol_num ) < 0 )
			return(-1);
		return( mk_trc( outfp, name, D(0), D(1), dd[6], dd[7] ) );
	}

	if( strcmp( solid_type, "tec" ) == 0 )  {
		/* V, H, A, B, p */
		if( getsoldata( dd, 4*3+1, sol_num ) < 0 )
			return(-1);
		r1 = 1.0/dd[12];	/* P */
		VSCALE( D(4), D(2), r1 );
		VSCALE( D(5), D(3), r1 );
		return( mk_tgc( outfp, name, D(0), D(1),
			D(2), D(3), D(4), D(5) ) );
	}

	if( strcmp( solid_type, "tgc" ) == 0 )  {
		/* V, H, A, B, r1, r2 */
		if( getsoldata( dd, 4*3+2, sol_num ) < 0 )
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
		if( getsoldata( dd, 1*3+2, sol_num ) < 0 )
			return(-1);
		return( mk_sph( outfp, name, D(0), dd[3] ) );
	}

	if( strcmp( solid_type, "ell" ) == 0 )  {
		if( version == 4 )  {
			vect_t	v;

			/*
			 * For simplicity, we convert ELL to ELL1, then
			 * fall through to ELL1 code.
			 * Format of ELL is F1, F2, len
			 * ELL1 format is V, A, r
			 */
			if( getsoldata( dd, 2*3+1, sol_num ) < 0 )
				return(-1);
			VADD2SCALE( v, D(0), D(1), 0.5 ); /* V is midpoint */

			VSUB2( work, D(1), D(0) );	/* work holds F2 -  F1 */
			m1 = MAGNITUDE( work );
			r2 = 0.5 * dd[6] / m1;
			VSCALE( D(1), work, r2 );	/* A */

			dd[6] = sqrt( MAGSQ( D(1) ) -
				(m1 * 0.5)*(m1 * 0.5) );	/* r */
			VMOVE( D(0), v );
			goto ellcom;
		}
		/* NO ELL's in gift5  -  handle as ELL1? */
		printf("No ELL in version 5??\n");
		return(-1);
	}

	if( strcmp( solid_type, "ell1" ) == 0 ||
	    strcmp( solid_type, "ellg" ) == 0 )  {
		/* V, A, r */
	    	/* GIFT4 name is "ell1", GIFT5 name is "ellg" */
		if( getsoldata( dd, 2*3+1, sol_num ) < 0 )
			return(-1);

ellcom:
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

	if( strcmp( solid_type, "tor" ) == 0 )  {
		/* V, N, r1, r2 */
		if( getsoldata( dd, 2*3+2, sol_num ) < 0 )
			return(-1);
		return( mk_tor( outfp, name, D(0), D(1), dd[6], dd[7] ) );
	}

	/*
	 *  The solid type string is defective,
	 *  or that solid is not currently supported.
	 */
	printf("convert:  no support for solid type '%s'\n", solid_type );
	return(-1);
}

/*
 *			M K _ A R S
 *
 *  The input is an array of pointers to an array of fastf_t values.
 *  There is one pointer for each curve.
 *  It is anticipated that there will be pts_per_curve+1 elements per
 *  curve, the first point being repeated as the final point, although
 *  this is not checked here.
 *
 *  Returns -
 *	 0	OK
 *	-1	Fail
 */
int
mk_ars( filep, name, ncurves, pts_per_curve, curves )
FILE	*filep;
char	*name;
int	ncurves;
int	pts_per_curve;
fastf_t	**curves;
{
	union record	rec;
	vect_t		base_pt;
	int		per_curve_grans;
	int		cur;

	bzero( (char *)&rec, sizeof(rec) );

	rec.a.a_id = ID_ARS_A;
	rec.a.a_type = ARS;			/* obsolete? */
	NAMEMOVE( name, rec.a.a_name );
	rec.a.a_m = ncurves;
	rec.a.a_n = pts_per_curve;

	per_curve_grans = (pts_per_curve+7)/8;
	rec.a.a_curlen = per_curve_grans;
	rec.a.a_totlen = per_curve_grans * ncurves;
	if( fwrite( (char *)&rec, sizeof(rec), 1, filep ) != 1 )
		return(-1);

	VMOVE( base_pt, &curves[0][0] );
	/* The later subtraction will "undo" this, leaving just base_pt */
	VADD2( &curves[0][0], &curves[0][0], base_pt);

	for( cur=0; cur<ncurves; cur++ )  {
		register fastf_t	*fp;
		int			npts;
		int			left;

		fp = curves[cur];
		left = pts_per_curve;
		for( npts=0; npts < pts_per_curve; npts+=8, left -= 8 )  {
			register int	el;
			register int	lim;

			bzero( (char *)&rec, sizeof(rec) );
			rec.b.b_id = ID_ARS_B;
			rec.b.b_type = ARSCONT;	/* obsolete? */
			rec.b.b_n = cur+1;	/* obsolete? */
			rec.b.b_ngranule = (npts/8)+1; /* obsolete? */

			lim = (left > 8 ) ? 8 : left;
			for( el=0; el < lim; el++ )  {
				vect_t	diff;
				VSUB2( diff, fp, base_pt );
				/* XXX converts to dbfloat_t */
				VMOVE( &(rec.b.b_values[el*3]), diff );
				fp += ELEMENTS_PER_VECT;
			}
			if( fwrite( (char *)&rec, sizeof(rec), 1, filep ) != 1 )
				return(-1);
		}
	}
	return(0);
}
