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

	if( (i = getline( &scard )) == EOF )  {
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
				if( getline(&scard) == EOF)
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

		solidp->s_id = SOLID;
		solidp->s_type = cur_type;
		solidp->s_num = sol_work;

		namecvt( solidp->s_num, solidp->s_name, 's' );

		for(i=0; i<24; i++)
			solidp->s_values[i] = 0.0;

		for( cd=1; cd <= ncards[solidp->s_type]; cd++ )  {
			if( cd != 1 )  {
				if( getline( &scard ) == EOF )  {
					printf("too few cards for solid %d\n",
						solidp->s_num);
					return	0;
				}
				/* continuation card
				 * solid type should be blank 
				 */
				if( (version==5 && ((char *)&scard)[5] != ' ' ) ||
				    (version==4 && ((char *)&scard)[3] != ' ' ) )  {
					printf("solid %d continuation card solid type non-blank, ='%s'\n",
						solidp->s_num, solid_type);
					return 0;
				}
			}

			fp = &solidp->s_values[cd*6-1];
			for(i=5; i>=0; i--)   {
				scard.sc_fields[i][10] = '\0';	/* null OFF END */
				*fp-- = atof( scard.sc_fields[i] );
			}
		}
		convert( solidp, solidp->s_name );
		return(1);		/* input is valid */
	}
}


/*
 * Input Vector Fields
 */
#define Fi	iv+(i-1)*3
#define F1	iv+(1-1)*3
#define F2	iv+(2-1)*3
#define F3	iv+(3-1)*3
#define F4	iv+(4-1)*3
#define F5	iv+(5-1)*3
#define F6	iv+(6-1)*3
#define F7	iv+(7-1)*3
#define F8	iv+(8-1)*3
/*
 * Output vector fields
 */
#define Oi	ov+(i-1)*3
#define O1	ov+(1-1)*3
#define O2	ov+(2-1)*3
#define O3	ov+(3-1)*3
#define O4	ov+(4-1)*3
#define O5	ov+(5-1)*3
#define O6	ov+(6-1)*3
#define O7	ov+(7-1)*3
#define O8	ov+(8-1)*3
#define O9	ov+(9-1)*3
#define O10	ov+(10-1)*3
#define O11	ov+(11-1)*3
#define O12	ov+(12-1)*3
#define O13	ov+(13-1)*3
#define O14	ov+(14-1)*3
#define O15	ov+(15-1)*3
#define O16	ov+(16-1)*3

/*
 *			C O N V E R T
 *
 *  This routine is expected to write the records out itself.
 */
convert( in, name )
struct solids *in;
char	*name;
{
	static struct solids out;
	register float *iv;
	register float *ov;
	static float r1, r2;
	static float work[3];
	static float m1, m2, m3, m4;	/* Magnitude temporaries */
	static float m5, m6;	/* TOR temporaries */
	static float r3,r4; /* magnitude temporaries */
	static int i;

	col_pr( name );

	iv = &in->s_values[0];
	ov = &out.s_values[0];

	switch( in->s_type )  {

	case RPP:
		{
			double	min[3], max[3];

			VSET( min, iv[0], iv[2], iv[4] );
			VSET( max, iv[1], iv[3], iv[5] );
			mk_rpp( outfp, name, min, max );
			return;
		}

	case BOX:
		VMOVE( O1, F1 );
		VADD2( O2, F1, F3 );
		VADD3( O3, F1, F3, F2 );
		VADD2( O4, F1, F2 );
		VADD2( O5, F1, F4 );
		VADD3( O6, F1, F4, F3 );
		VADD4( O7, F1, F4, F3, F2 );
		VADD3( O8, F1, F4, F2 );
		goto ccommon;

	case RAW:
		VMOVE( O1, F1 );
		VADD2( O2, F1, F3 );
		VMOVE( O3, O2 );
		VADD2( O4, F1, F2 );
		VADD2( O5, F1, F4 );
		VADD3( O6, F1, F4, F3 );
		VMOVE( O7, O6 );
		VADD3( O8, F1, F4, F2 );
	ccommon:
		VMOVE( F1, O1 );
		for( i=2; i<=8; i++ )  {
			VSUB2( Fi, Oi, O1 );
		}
		in->s_type = GENARB8;
		break;

	case ARB8:
	arb8common:
		for(i=2; i<=8; i++)  {
			VSUB2( Fi, Fi, F1 );
		}
		in->s_type = GENARB8;
		break;

	case ARB7:
		VMOVE( F8, F5 );
		in->s_type = ARB8;
		goto arb8common;

	case ARB6:
		/* Note that the ordering is important, as data is in F5, F6 */
		VMOVE( F8, F6 );
		VMOVE( F7, F6 );
		VMOVE( F6, F5 );
		in->s_type = ARB8;
		goto arb8common;

	case ARB5:
		VMOVE( F6, F5 );
		VMOVE( F7, F5 );
		VMOVE( F8, F5 );
		in->s_type = ARB8;
		goto arb8common;

	case ARB4:
		/* Order is important, data is in F4 */
		VMOVE( F8, F4 );
		VMOVE( F7, F4 );
		VMOVE( F6, F4 );
		VMOVE( F5, F4 );
		VMOVE( F4, F1 );
		in->s_type = ARB8;
		goto arb8common;

	case RCC:
		r1 = iv[6];	/* R */
		r2 = iv[6];
		goto trccommon;		/* sorry */

	case REC:
		VMOVE( F5, F3 );
		VMOVE( F6, F4 );
		in->s_type = GENTGC;
		break;

		/*
		 * For the TRC, if the V vector (F1) is of zero length,
		 * a divide by zero will occur when scaling by the magnitude.
		 * We add the vector [PI, PI, PI] to V to produce a unique
		 * (and most likely non-zero) resultant vector.  This will
		 * do nicely for purposes of cross-product.
		 */

	case TRC:
		r1 = iv[6];	/* R1 */
		r2 = iv[7];	/* R2 */
	trccommon:
		VMOVE( work, F1 );
		work[0] += PI;
		work[1] += PI;
		work[2] += PI;
		VCROSS( F3, work, F2 );
		m1 = MAGNITUDE( F3 );
		VSCALE( F3, F3, r1/m1 );

		VCROSS( F4, F2, F3 );
		m2 = MAGNITUDE( F4 );
		VSCALE( F4, F4, r1/m2 );

		VSCALE( F5, F3, r2/r1 );
		VSCALE( F6, F4, r2/r1 );
		in->s_type = GENTGC;
		break;

	case TEC:
		r1 = iv[12];	/* P */
		VSCALE( F5, F3, (1.0/r1) );
		VSCALE( F6, F4, (1.0/r1) );
		in->s_type = GENTGC;
		break;

	case TGC:
		r1 = iv[12] / MAGNITUDE( F3 );	/* A/|A| * C */
		r2 = iv[13] / MAGNITUDE( F4 );	/* B/|B| * D */
		VSCALE( F5, F3, r1 );
		VSCALE( F6, F4, r2 );
		in->s_type = GENTGC;
		break;

	case SPH:
		{
			double	center[3], radius;
			VMOVE( center, iv );
			radius = iv[3];		/* R */
			mk_sph( outfp, name, center, radius );
			return;
		}

	case ELL:
		if( version == 4 )  {
			/*
			 * For simplicity, we convert ELL to ELL1, then
			 * fall through to ELL1 code.  Format of ELL is
			 * F1, F2, l
			 * ELL1 format is V, A, r
			 */
			r1 = iv[6];		/* length */
			VADD2( O1, F1, F2 );
			VSCALE( O1, O1, 0.5 );	/* O1 holds V */

			VSUB2( O3, F2, F1 );	/* O3 holds F2 -  F1 */
			m1 = MAGNITUDE( O3 );
			r2 = 0.5 * r1 / m1;
			VSCALE( O2, O3, r2 );	/* O2 holds A */

			iv[6] = sqrt( MAGSQ( O2 ) - (m1 * 0.5)*(m1 * 0.5) );	/* r */
			VMOVE( F1, O1 );	/* Move V */
			VMOVE( F2, O2 );	/* Move A */
		} else {
			/* NO ELL's in gift5  -  fall through to ELL1 */
		}
		/* fall through */

	case ELL1:		/* GIFT4 name */
	case GENELL:		/* GIFT5 name */
		/* GENELL is V, A, B, C */
		r1 = iv[6];		/* R */
		VMOVE( work, F1 );
		work[0] += PI;
		work[1] += PI;
		work[2] += PI;
		VCROSS( F3, work, F2 );		/* See the TRC comments */
		m1 = MAGNITUDE( F3 );
		VSCALE( F3, F3, r1/m1 );

		VCROSS( F4, F2, F3 );
		m2 = MAGNITUDE( F4 );
		VSCALE( F4, F4, r1/m2 );
		in->s_type = GENELL;
		break;

	case TOR:
		{
			double	center[3], norm[3], r1, r2;

			VMOVE( center, &iv[0] );
			VMOVE( norm, &iv[3] );
			r1=iv[6];	/* Dist from end of V to center of (solid portion) of TORUS */
			r2=iv[7];	/* Radius of solid portion of TORUS */

			mk_tor( outfp, name, center, norm, r1, r2 );
			return;
		}

	default:
		/*
		 * We should never reach here
		 */
		printf("convert:  no support for type %d\n", in->s_type );
		return;
	}

	/* Common code to write out the record */
	/* XXX error checking? */
	fwrite( (char *)in, sizeof(union record), 1, outfp );
}
