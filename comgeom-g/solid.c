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

#include "./ged_types.h"
#include "./3d.h"

#define VADD2SCALE( o, a, b, s )	o[X] = ((a)[X] + (b)[X]) * (s); \
					o[Y] = ((a)[Y] + (b)[Y]) * (s); \
					o[Z] = ((a)[Z] + (b)[Z]) * (s);

extern FILE	*outfp;
extern int	version;

extern int sol_total, sol_work;

#define PI	3.14159265358979323846264	/* Approx */


struct scard {
	char	sc_num_and_type[10];
	char	sc_fields[6][10];
	char	sc_remark[16];
} scard;

int ncards[] =  {
	0,
	1,	/* RPP */
	2,
	2,
	2,
	3,
	3,
	4,
	4,	/* ARB8 */
	2,	/* ELL */
	2,	/* ELL1 */
	1,	/* SPH */
	2,	/* RCC */
	2,	/* REC */
	2,	/* TRC */
	3,	/* TEC */
	2,	/* TOR */
	3,	/* TGC */
	0,	/* GENTGC, 18 */
	2,	/* ELLG, 19 */
};

/*
 * Table to map names of solids into internal numbers.
 *
 * NOTE that the name field is expected to be four characters
 * wide, right blank filled, null terminated.
 */
struct table  {
	int	t_value;
	char	t_name[6];
}  
table[] =  {
	RPP,	"rpp",
	BOX,	"box",
	RAW,	"raw",
	ARB8,	"arb8",
	ARB7,	"arb7",
	ARB6,	"arb6",
	ARB5,	"arb5",
	ARB4,	"arb4",
	GENELL,	"ellg",
	ELL1,	"ell1",
	ELL,	"ell",		/* After longer ell entries */
	SPH,	"sph",
	RCC,	"rcc",
	REC,	"rec",
	TRC,	"trc",
	TEC,	"tec",
	TOR,	"tor",
	TGC,	"tgc",
	GENTGC,	"cone",
	ARS,	"ars",
	0,	""
};

/*
 *			L O O K U P
 */
int
lookup( cp )
register char	*cp;
{
	register struct table *tp;
	register char	*lp;
	register char	c;
	char	low[32];
	

	/* Reduce input to lower case */
	lp = low;
	while( (c = *cp++) != '\0' )  {
		if( !isascii(c) )  {
			*lp++ = '?';
		} else if( isupper(c) )  {
			*lp++ = tolower(c);
		} else {
			*lp++ = c;
		}
	}
	*lp = '\0';

	tp = &table[0];
	while( tp->t_value != 0 )  {
		if( strcmp( low, tp->t_name ) == 0 )
			return( tp->t_value );
		tp++;
	}

	printf("ERROR:  bad solid type '%s'\n", low );
	return(0);
}

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
	register struct solids *solidp;
	static int i,j,n,jj,m,mx;
	static float *fp;
	int cd,cds,M,N,Nb_strcx,Nb_strsl,nst,structn,rmcx,cdcx;
	static char nbuf[6];
	static float xmax, ymax, zmax;
	static float xmin, ymin, zmin;
	static float ibuf[10];
	static float jbuf[10];
	struct ars_rec *arsap;
	union record *b;
	char	cur_solid_num[16];
	char	solid_type[16];
	int	cur_type;
	union record	rec;

	solidp = &rec.s;

	if( sol_work == sol_total )	/* processed all solids */
		return( 0 );

	if( (i = getline( &scard, sizeof(scard), "solid card" )) == EOF )  {
		printf("getsolid: unexpected EOF\n");
		return( 0 );
	}

	if( version == 5 )  {
		strncpy( cur_solid_num, ((char *)&scard)+0, 5 );
		cur_solid_num[5] = '\0';
		strncpy( solid_type, ((char *)&scard)+5, 5 );
		solid_type[5] = '\0';
	} else {
		strncpy( cur_solid_num, ((char *)&scard)+0, 3 );
		cur_solid_num[3] = '\0';
		strncpy( solid_type, ((char *)&scard)+3, 7 );
		solid_type[7] = '\0';
	}
	/* Trim trailing spaces */
	trim_trail_spaces( cur_solid_num );
	trim_trail_spaces( solid_type );

	/* another solid - increment solid counter
	 * rather than using number from the card, which may go into
	 * pseudo-hex format in version 4 models (due to 3 column limit).
	 */
	sol_work++;

	if( (cur_type = lookup(solid_type)) <= 0 )  {
		printf("getsolid: bad type\n");
		return -1;
	}

	if( cur_type == ARS){
		/*
		 * PROCESS ARS SOLID
		 */
		arsap = (struct ars_rec *) solidp;
		arsap->a_id = ARS_A;
		arsap -> a_type = ARS;
		namecvt( sol_work, arsap -> a_name, 's');
		col_pr( arsap->a_name );

		/*  init  max and min values */
		xmax = ymax = zmax = -10000.0;
		xmin = ymin = zmin = 10000.0;
		/* read   N  M values    ->     convert ASCII to integer  */
		nbuf[10] = '\0';
		for (i=0; i<10; i++)
			nbuf[i]=scard.sc_fields[0][i];

		M = atoi(nbuf);
		arsap -> a_m = M;
		for(i=0; i<10; i++)
			nbuf[i]=scard.sc_fields[1][i];
		N=atoi(nbuf);
		arsap->a_n = N;

		/*
		 *   the number of b type structures needed to store 1
		 * cross section is equal to N/8;  if N modulo 8 is not
		 * equal to 0 then 1 more structure is needed.
		 */
		Nb_strcx = N/8;
		if(( N % 8) !=0)
			Nb_strcx++;
		rmcx = N % 8;

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
		Nb_strsl = Nb_strcx * M;
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

		/* calculate number of ARS data cards that have to be read in */
		/*   as given in GIFT users manual  */
		cd = M*((N+1)/2);

		/*    number of data cards per cross section calculated here */
		cdcx = (N+1)/2;

		/*  2 sets of xyz coordinates for every data card
		 *  set up counters to read in coordinate points and load into 
		 *       b structures created above
		 */
		cds = 0;	/* number of data cards read in (1-> cd) */
		nst = -1;	/* b type structure number currently loaded */

		structn = 0;	/* granule # within a cross secton  */
		for(m=1; m < (M+1); m++)  {
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
					b[nst].b.b_id = ARS_B;
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

			/* calculate  min and max values associated with ARS data points */
			/*   save in A type structure */
			for(j=0; j < Nb_strsl; j++)   {
				for(i=0; i<8; i++){
					if(b[j].b.b_values[i*3] > xmax)
						xmax = b[j].b.b_values[i*3];
					else if(b[j].b.b_values[i*3] < xmin)
						xmin = b[j].b.b_values[i*3];
					else if(b[j].b.b_values[(i*3)+1] > ymax )
						ymax = b[j].b.b_values[(i*3)+1];
					else if(b[j].b.b_values[(i*3)+1] < ymin)
						ymin = b[j].b.b_values[(i*3)+1];
					else if(b[j].b.b_values[(i*3)+2] > zmax)
						zmax = b[j].b.b_values[(i*3)+2];
					else if(b[j].b.b_values[(i*3)+2] < zmin)
						zmin = b[j].b.b_values[(i*3)+2];
				}
			}
			/* save max's and min in atype structure */
			arsap->a_xmax=xmax;
			arsap->a_xmin=xmin;
			arsap->a_ymax=ymax;
			arsap->a_ymin=ymin;
			arsap->a_zmax=zmax;
			arsap->a_zmin=zmin;
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

	}  else   {
		/* solid type other than ARS */
		convert( cur_type, sol_work, solid_type );

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

		fp = &dp[cd*6-1];
		if( num <= 6 )
			i = num-1;
		else
			i = 5;
		for(; i>=0; i--,num--)   {
			scard.sc_fields[i][10] = '\0';	/* null OFF END */
			*fp-- = atof( scard.sc_fields[i] );
		}
	}
	return(0);
}

/*
 *			C O N V E R T
 *
 *  This routine is expected to write the records out itself.
 */
convert( cur_type, sol_num, solid_type )
int	cur_type;
int	sol_num;
char	*solid_type;
{
	double	r1, r2;
	vect_t	work;
	double	m1, m2;	/* Magnitude temporaries */
	static int i;
	char	name[16+2];
	double	dd[4*6];	/* 4 cards of 6 nums each */
	double	tmp[3*8];	/* 8 vectors of 3 nums each */

	namecvt( sol_num, name, 's' );

#define D(_i)	(&(dd[_i*3]))
#define T(_i)	(&(tmp[_i*3]))
	if( getsoldata( dd, ncards[cur_type]*6, sol_num ) < 0 )
		return(-1);

	col_pr( name );

	switch( cur_type )  {

	case RPP:
		{
			double	min[3], max[3];

			VSET( min, dd[0], dd[2], dd[4] );
			VSET( max, dd[1], dd[3], dd[5] );
			return( mk_rpp( outfp, name, min, max ) );
		}

	case BOX:
		VMOVE( T(0), D(0) );
		VADD2( T(1), D(0), D(2) );
		VADD3( T(2), D(0), D(2), D(1) );
		VADD2( T(3), D(0), D(1) );

		VADD2( T(4), D(0), D(3) );
		VADD3( T(5), D(0), D(3), D(2) );
		VADD4( T(6), D(0), D(3), D(2), D(1) );
		VADD3( T(7), D(0), D(3), D(1) );
		return( mk_arb8( outfp, name, tmp ) );

	case RAW:
		VMOVE( T(0), D(0) );
		VADD2( T(1), D(0), D(2) );
		VMOVE( T(2), T(1) );
		VADD2( T(3), D(0), D(1) );

		VADD2( T(4), D(0), D(3) );
		VADD3( T(5), D(0), D(3), D(2) );
		VMOVE( T(6), T(5) );
		VADD3( T(7), D(0), D(3), D(1) );
		return( mk_arb8( outfp, name, tmp ) );

	case ARB8:
		return( mk_arb8( outfp, name, dd ) );

	case ARB7:
		VMOVE( D(7), D(4) );
		return( mk_arb8( outfp, name, dd ) );

	case ARB6:
		/* Note that the ordering is important, as data is in D(4), D(5) */
		VMOVE( D(7), D(5) );
		VMOVE( D(6), D(5) );
		VMOVE( D(5), D(4) );
		return( mk_arb8( outfp, name, dd ) );

	case ARB5:
		VMOVE( D(5), D(4) );
		VMOVE( D(6), D(4) );
		VMOVE( D(7), D(4) );
		return( mk_arb8( outfp, name, dd ) );

	case ARB4:
		return( mk_arb4( outfp, name, dd ) );

	case RCC:
		return( mk_rcc( outfp, name, D(0), D(1), dd[6] ) );

	case REC:
		/* base height, a, b, c, d */
		return( mk_tgc( outfp, name, D(0), D(1),
			D(2), D(3), D(2), D(3) ) );

	case TRC:
		return( mk_trc( outfp, name, D(0), D(1), dd[6], dd[7] ) );

	case TEC:
		/* V, H, A, B, p */
		r1 = 1.0/dd[12];	/* P */
		VSCALE( D(4), D(2), r1 );
		VSCALE( D(5), D(3), r1 );
		return( mk_tgc( outfp, name, D(0), D(1),
			D(2), D(3), D(4), D(5) ) );

	case TGC:
		/* V, H, A, B, r1, r2 */
		r1 = dd[12] / MAGNITUDE( D(2) );	/* A/|A| * C */
		r2 = dd[13] / MAGNITUDE( D(3) );	/* B/|B| * D */
		VSCALE( D(4), D(2), r1 );
		VSCALE( D(5), D(3), r2 );
		return( mk_tgc( outfp, name, D(0), D(1),
			D(2), D(3), D(4), D(5) ) );

	case SPH:
		/* V, radius */
		return( mk_sph( outfp, name, D(0), dd[3] ) );

	case ELL:
		if( version == 4 )  {
			vect_t	v;
			/*
			 * For simplicity, we convert ELL to ELL1, then
			 * fall through to ELL1 code.
			 * Format of ELL is F1, F2, len
			 * ELL1 format is V, A, r
			 */
			VADD2SCALE( v, D(0), D(1), 0.5 ); /* V is midpoint */

			VSUB2( work, D(1), D(0) );	/* work holds F2 -  F1 */
			m1 = MAGNITUDE( work );
			r2 = 0.5 * dd[6] / m1;
			VSCALE( D(1), work, r2 );	/* A */

			dd[6] = sqrt( MAGSQ( D(1) ) -
				(m1 * 0.5)*(m1 * 0.5) );	/* r */
			VMOVE( D(0), v );
		} else {
			/* NO ELL's in gift5  -  fall through to ELL1 */
		}
		/* fall through */

	case ELL1:		/* GIFT4 name */
	case GENELL:		/* GIFT5 name */
		/* V, A, r */

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

	case TOR:
		/* V, N, r1, r2 */
		return( mk_tor( outfp, name, D(0), D(1), dd[6], dd[7] ) );
	}

	/*
	 * We should never reach here
	 */
	printf("convert:  no support for solid %d\n", cur_type );
	return(-1);
}
