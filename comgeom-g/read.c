/*			R E A D . C
 * This module contains all of the routines necessary to read in
 * a COMGEOM input file, and put the information into internal form.
 * Ballistic Research Laboratory
 * U. S. Army
 * March 17, 1980
 */
extern double atof();
extern char *malloc();

#include <stdio.h>
#include <ctype.h>
#include "./ged_types.h"
#include "./3d.h"

#define DOWN 0
#define UP   1
#define NCARDS	30	/* Number of cards in 1 region */

extern union record record;
extern int sol_total, sol_work;
extern int	version;

struct scard {
	char	sc_num_and_type[10];
	char	sc_fields[6][10];
	char	sc_remark[16];
} 
scard;

struct rcard  {
	char	rc_num[5];
	char	rc_null;
	struct	rcfields  {
		char	rcf_null;
		char	rcf_or;
		char	rcf_solid[5];
	}  
	rc_fields[9];
	char	rc_remark[11+3];
} 
rcard;

struct idcard  {
	char	id_foo[80];
#if 0
#ifdef GIFT5
	char	id_region[5];
	char	id_rid[5];
	char	id_air[5];
	char	id_mat[5];
	char	id_los[5];
	char	id_waste[55];
#else
	char	id_region[10];
	char	id_rid[10];
	char	id_air[10];
	char	id_waste[44];
	char	id_mat[3];	/* use any existing material code */
	char	id_los[3];	/* use any existing los percentage */
#endif
#endif
} 
idcard;

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

int outfd;		/* Output only fd */
int updfd;		/* Update fd */

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
 */
getsolid( solidp )
register struct solids *solidp;
{
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


	if( sol_work == sol_total )	/* processed all solids */
		return( 0 );

	if( (i = getline( &scard )) == EOF )
		return( 0 );	/* end of file */

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
printf("cur_solid_num='%s', solid_type='%s'\n", cur_solid_num, solid_type );

	/* another solid - increment solid counter
	 * rather than using number from the card, which may go into
	 * pseudo-hex format in version 4 models (due to 3 column limit).
	 */
	sol_work++;

	/*
	 * PROCESS ARS SOLID
	 */
	if( lookup(solid_type) == ARS){
		arsap = (struct ars_rec *) solidp;
		arsap->a_id = ARS_A;
		arsap -> a_type = ARS;
		namecvt( sol_work, arsap -> a_name, 's');
		printf("%s \t",arsap->a_name);

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
		    malloc( (unsigned) Nb_strsl*sizeof record )
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
		write( outfd, arsap, sizeof record);
		write(outfd, &b[0], (Nb_strsl * sizeof (record)));

		/* free dynamic storage alloated for ARS */
		free (b);
		return(2);

	}  else   {
		/* solid type other than ARS */


		solidp->s_id = SOLID;
		solidp->s_type = lookup( solid_type );
		solidp->s_num = sol_work;

		namecvt( solidp->s_num, solidp->s_name, 's' );

		for( cd=1; cd <= ncards[solidp->s_type]; cd++ )  {
			if( cd != 1 )  {
				if( getline( &scard ) == EOF )
					{
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
		return(1);		/* input is valid */
	}
}

int noreadflag = DOWN;

/*
 *			G E T R E G I O N
 */
getregion( rp )
register union record *rp;
{
	static union record mem[9*NCARDS];	/* Holds WHOLE Combination */
	register struct members *mp;		/* Pointer to current member */
	int i, j;
	int card;
	int count;
	int n;
	int reg_reg_flag = 0;
	char *cp;

	rp->c.c_id = COMB;
	count = 0;		/* count of # of solids in region */

	if( noreadflag == DOWN )  {
		if( getline( &rcard ) == EOF ) 
			return( 0 );		/* EOF */
	}
	noreadflag = DOWN;

	rcard.rc_null = 0;
	rp->c.c_num = atoi( rcard.rc_num );

	/* -1 region number terminates table */
	if( rp->c.c_num < 0 ) 
		return( 0 );

	/* Build name from region number, set region flag */
	namecvt( rp->c.c_num, rp->c.c_name, 'r' );
	rp->c.c_flags = 'R';		/* Region flag */
	rp->c.c_material = 0;
	rp->c.c_los = 100;

/*
	rcard.rc_fields[0].rcf_or = 'R';
*/

	for( card=0; card<NCARDS; card++ )  {
		if( card != 0 )  {
			if( getline( &rcard ) == EOF )
				{
				printf("read of card failed\n");
				return(0);
				}
			rcard.rc_null = 0;
			if( atoi( rcard.rc_num ) != 0 )  {
				/* finished with this region */
				noreadflag = UP;
				break;
			}
		}

		cp = (char *) rcard.rc_fields;

		/* Scan each of the 9 fields on the card */
		for( i=0; i<9; i++ )  {
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

			n = atoi( cp+2 );

			/* Check for null field -- they are to be skipped */
			if( n == 0 )  {
				cp += 7;
				continue;	/* zeros are allowed as placeholders */
			}

			mp = &(mem[count++].M);
			if( version == 5 )  {
				/* Region references region in Gift5 */
				if(rcard.rc_fields[i].rcf_or == 'g' ||
				   rcard.rc_fields[i].rcf_or == 'G')
					reg_reg_flag = 1;

				if( cp[1] == 'R' || cp[1] == 'r' ) 
					mp->m_relation = UNION;
				else {
					if( n < 0 )  {
						mp->m_relation = SUBTRACT;
						n = -n;
					}  else  {
						mp->m_relation = INTERSECT;
					}
				}
			} else {
				/* XXX this may actually be an old piece of code,
				 * rather than the V4 way of doing it. */
				if( cp[1] != ' ' )  {
					mp->m_relation = UNION;
				}  else  {
					if( n < 0 )  {
						mp->m_relation = SUBTRACT;
						n = -n;
					}  else  {
						mp->m_relation = INTERSECT;
					}
				}
			}

			mp->m_id = MEMB;
			mp->m_num = n;
			mat_idn( mp->m_mat );

			/* In Gift5, regions can reference regions */
			if( reg_reg_flag )
				namecvt(n, mp->m_instname, 'r');
			else
				namecvt( n, mp->m_instname, 's' );
			reg_reg_flag = 0;

			cp += 7;
		}
	}

	if(card == NCARDS) {
		/* check if NCARDS is large enough */
		if(getline(&rcard) == EOF )
			return(0);
		rcard.rc_null = 0;
		if(atoi(rcard.rc_num) == 0) {
			printf("STOP: too many lines for region %d\n",rp->c.c_num);
			printf("must change code (read.c)  NCARDS = %d\n",NCARDS);
			exit(10);
		}
		noreadflag = UP;
	}

	rp->c.c_length = count;

	write( outfd, rp, sizeof record );
	write( outfd, mem, count * sizeof record );

	return( 1 );		/* success */
}

/*
 *			G E T I D
 *
 * Load the region ID information into the structures
 */
getid( rp )
register union record *rp;
{
	register int	i;
	int reg;
	int id;
	int air;
	int mat;
	int los;
	char buff[11];
	int	buflen;

	if( getline( (char *) &idcard ) == EOF ||
	    ((char *) &idcard)[0] == '\n' )
		return( 0 );

	if( version == 5 )  {
		sscanf( idcard, "%5d%5d%5d%5d%5d",
			&reg, &id, &air, &mat, &los );
	} else {
		sscanf( idcard, "%10d%10d%10d%*44s%3d%3d",
			&reg, &id, &air, &mat, &los );
	}
printf("reg=%d,id=%d,air=%d,mat=%d,los=%d\n", reg,id,air,mat,los);

#if 0
	if( version == 5 )
		buflen = 5;
	else
		buflen = 10;
	buff[buflen] = '\0';

	for(i=0; i<buflen; i++)
		buff[i] = idcard.id_region[i];
	reg = atoi( buff );

	for(i=0; i<buflen; i++)
		buff[i] = idcard.id_rid[i];
	id = atoi( buff );

	for(i=0; i<buflen; i++)
		buff[i] = idcard.id_air[i];
	air = atoi( buff );

	if( version == 5 )  {
		for(i=0; i<5; i++)
			buff[i] = idcard.id_mat[i];
		mat = atoi( buff );

		for(i=0; i<5; i++)
			buff[i] = idcard.id_los[i];
		los = atoi( buff );
	} else {
		idcard.id_mat[2] = '\0';
		mat = atoi( idcard.id_mat );

		for( i=0; i<3; i++ )
			buff[i] = idcard.id_los[i];
		buff[3] = '\0';
		los = atoi( buff );
	}
#endif

	if( (i = read( updfd, (char *) rp, sizeof(union record) ))
		!= sizeof(union record)
		)
		{
		if( i == -1 )
			perror( "read" );
		printf("read error...STOP\n");
		exit(10);
		}
	/* Back up */
	if( lseek( updfd, -(long)(sizeof(union record)), 1 ) == -1 )
		{
		perror( "lseek" );
		exit(10);
		}
	if( rp->c.c_id != COMB )  {
		printf("Record ID was not COMB...STOP\n");
		exit( 10 );
	}

	if( rp->c.c_num > reg )  {
		/* Must have been a null region */
		return(1);	/* try again with next input card */
	}

	if( rp->c.c_num != reg )  {
		printf("MISMATCH: region table region is %d  ident table region is %d\n",rp->c.c_num,reg);
		printf("STOP\n");
		exit( 10 );
	}

	/* Write out the enhanced Comb (header) record */
	rp->c.c_regionid = id;
	rp->c.c_aircode = air;
	rp->c.c_material = mat;
	rp->c.c_los = los;

	write( updfd, rp, sizeof record );

	/* skip over member records */
	lseek( updfd, rp->c.c_length * ((long)sizeof record), 1 );
	return( 1 );
}

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

/*
 *			G E T L I N E
 */
int
getline( cp )
register char *cp;
{
	register int	c;
	register int	count = 80;

	while( (c = getchar()) == '\n' ) /* Skip blank lines.		*/
		;
	while( c != EOF && c != '\n' )  {
		*cp++ = c;
		count--;
		if( count <= 0 )  {
			printf("input buffer (80) overflow\n");
			break;
		}
		c = getchar();
	}
	if( c == EOF )
		return	EOF;
	while( count-- > 0 ) 
		*cp++ = 0;
	return	c;
}


/*		N A M E C V T	 */
namecvt( n, cp, c )
register char *cp;
register int n;
{ static char str[16], s[16];
  extern char name_it[16];		/* argv[3] */
#if 0
	str[0] = c;			/* record type letter.*/
	str[1] = '\0';			/* terminate string.*/
	f2a( (float)n, s, 12, 0 );	/* get string for 'n'.*/
	strcat( str, s );		/* append 'n'string.*/
	strcat( str, name_it );	/* append argv[3].*/
#else
	sprintf( str, "%c%d%s", c, n, name_it );
#endif
	strncpy( cp, str, 16 );		/* truncate str to 16 chars.*/
}



/*
 *			G E T _ T I T L E
 */
get_title()
{
	char ctitle[80];
	int i;

	if( (i = getline( ctitle )) == EOF ) {
		printf("Empty input file\n");
		exit(10);
	}
	/* First 2 chars are units */
	printf("Units  Title: %s\n",ctitle);
	return;
}



/*
 *			G E T _ C O N T R O L
 */
get_control()
{
	int i;
	char ch[80];
	int	reg_total=0;

	if( (i = getline( ch )) == EOF ) {
		printf("No control card .... STOP\n");
		exit(10);
	}

	if( version == 5 )  {
		sscanf( ch, "%5d%5d", &sol_total, &reg_total );
	} else {
		ch[20] = 0;
		sol_total = atoi( ch+10 );
	}

	printf("Expecting %d solids, %d regions\n", sol_total, reg_total);
}
