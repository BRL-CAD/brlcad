
/* 
 *			P I P E
 *
 * Generate piping (fuel, hydraulic lines, etc) in MGED
 * format from input (points in space defining the routing).
 * Makes both tubing regions and fluid regions or solid cable.
 * Automatically generates elbow regions (and fluid in the
 * elbows) when the piping changes direction.
 * 
 * Version 1.0
 * Author:	Earl P. Weaver
 * 		Ballistic Research Labratory
 * 		Aberdeen Proving Ground, Md.
 * Date:	Mon March 13 1989
 * Version 2.0
 * Changes by:	John R. Anderson
 * Date:	Wed December 6 1989
 *
 * Version 3.0
 * Changes by:	John R. Anderson
 * Date: 	Tuesday January 18, 1994
 *	combined code into a single file
 *
 * Version 3.1
 * Changes by:	John R. Anderson
 * Date:	Wednesday May 18,1994
 *	included in BRLCAD distribution
 */

#ifndef lint
static char RCSid[] = "$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "db.h"
#include "wdb.h"

#define	VERSION	3
#define	RELEASE	1


#define REGION	1

#define MINR	2.		/* minimum bending radius is MINR*tubingradius	*/
				/* note, the bending radius is measured to the	*/
				/* edge of the tube, NOT the centerline of tube	*/

struct points
{
	point_t p;	/* data point */
	point_t p1;	/* adjusted point torward previous point */
	point_t p2;	/* adjusted point torward next point */
	point_t center;	/* center point for tori elbows */
	vect_t nprev;	/* unit vector towards previous point */
	vect_t nnext;	/* unit vector towards next point */
	fastf_t alpha; /* angle between "nprev" and "nnext" */
	vect_t norm;	/* unit vector normal to "nprev" and "nnext" */
	vect_t nmitre;	/* unit vector along mitre joint */
	vect_t mnorm;	/* unit vector normal to mitre joint and "norm" */
	char tube[NAMESIZE],tubflu[NAMESIZE];			/* solid names */
	char elbow[NAMESIZE],elbflu[NAMESIZE],cut[NAMESIZE];	/* solid names */
	char tube_r[NAMESIZE],tubflu_r[NAMESIZE];		/* region names */
	char elbow_r[NAMESIZE],elbflu_r[NAMESIZE];		/* region names */
	struct points *next,*prev;
};


double radius,wall,pi,k;
struct points *root;
char	name[16];
float	delta=10.0;		/* 10mm excess in ARB sizing for cutting tubes */
mat_t	identity;

int torus=0,sphere=0,mitre=0,nothing=0,cable=0;

/*	formats for solid and region names	*/
char *tor_out="%02d-out.tor",*tor_in="%02d-in.tor";
char *haf="%02d.haf";
char *sph_out="%02d-out.sph",*sph_in="%02d-in.sph";
char *rcc_out="%02d-out.rcc",*rcc_in="%02d-in.rcc";
char *tub_reg="%02d.tube",*tub_flu="%02d.tubflu";
char *elb_reg="%02d.elbow",*elb_flu="%02d.elbflu";
char *arb="%02d.arb";

void Usage();

FILE *fdout;	/* file for libwdb writes */

void
Make_name( ptr , form , base , number )
char *ptr,*form,*base;
int number;
{

	char scrat[NAMESIZE];
	int len;

	strcpy( ptr , base );
	sprintf( scrat , form , number );
	len = strlen( ptr ) + strlen( scrat );
	if( len > (NAMESIZE-1) )
		ptr[ (NAMESIZE-1) - strlen( scrat ) ] = '\0';
	strcat( ptr , scrat );
}

void
Readpoints()
{
	struct points *ptr,*prev;
	fastf_t x,y,z;
	
	ptr = root;
	prev = NULL;


	printf( "X Y Z (^D for end): ");
	while(scanf("%lf%lf%lf",&x, &y, &z) ==  3 )
	{
		if( ptr == NULL )
		{
			ptr = (struct points *)malloc( sizeof( struct points ) );
			root = ptr;
		}
		else
		{
			ptr->next = (struct points *)malloc( sizeof( struct points ) );
			ptr = ptr->next;
		}
		ptr->next = NULL;
		ptr->prev = prev;
		prev = ptr;
		VSET(ptr->p,k*x,k*y,k*z);
		ptr->tube[0] = '\0';
		ptr->tubflu[0] = '\0';
		ptr->elbow[0] = '\0';
		ptr->elbflu[0] = '\0';
		ptr->cut[0] = '\0';
		printf( "X Y Z (^D for end): ");
	}
}

void
Names()
{
	struct points *ptr;
	char *inform,*outform,*cutform;
	int nummer=0;


	if( torus )
	{
		outform = tor_out;
		inform = tor_in;
		cutform = arb;
	}
	else if( sphere )
	{
		outform = sph_out;
		inform = sph_in;
	}
	else if( mitre )
		cutform = haf;

	ptr = root;
	while( ptr->next != NULL )
	{
		nummer++;

		/* Outer RCC */
		Make_name( ptr->tube , rcc_out , name , nummer );

		if( !cable ) /* Inner RCC */
			Make_name( ptr->tubflu , rcc_in , name , nummer );

		if( sphere || torus && ptr != root )
		{
			/* Outer elbow */
			Make_name( ptr->elbow , outform , name , nummer );

			if( !cable ) /* Inner elbow */
				Make_name( ptr->elbflu , inform , name , nummer );
		}
		if( torus || mitre )	/* Make cutting solid name */
			Make_name( ptr->cut , cutform , name , nummer );

		/* Make region names */
		Make_name( ptr->tube_r , tub_reg , name , nummer ); /* tube region */

		if( torus || sphere && ptr != root )	/* Make elbow region name */
			Make_name( ptr->elbow_r , elb_reg , name , nummer );

		if( !cable )	/* Make fluid region names */
		{
			Make_name( ptr->tubflu_r , tub_flu , name , nummer );

			if( torus || sphere && ptr != root )	/* Make elbow fluid region names */
				Make_name( ptr->elbflu_r , elb_flu , name , nummer );
		}

		ptr = ptr->next;
	}
	ptr->tube[0] = '\0';
	ptr->tubflu[0] = '\0';
	ptr->elbow[0] = '\0';
	ptr->elbflu[0] = '\0';
	ptr->cut[0] = '\0';
	ptr->tube_r[0] = '\0';
	ptr->tubflu_r[0] = '\0';
	ptr->elbow_r[0] = '\0';
	ptr->elbflu_r[0] = '\0';
}

void
Normals()
{
	struct points *ptr;


	if( root == NULL )
		return;

	ptr = root->next;
	if( ptr == NULL )
		return;

	VSUB2( root->nnext , ptr->p , root->p );
	VUNITIZE( root->nnext );

	while( ptr->next != NULL )
	{
		VREVERSE( ptr->nprev , ptr->prev->nnext );
		VSUB2( ptr->nnext , ptr->next->p , ptr->p );
		VUNITIZE( ptr->nnext );
		VCROSS( ptr->norm , ptr->nprev , ptr->nnext );
		VUNITIZE( ptr->norm );
		VADD2( ptr->nmitre , ptr->nprev , ptr->nnext );
		VUNITIZE( ptr->nmitre );
		VCROSS( ptr->mnorm , ptr->norm , ptr->nmitre );
		VUNITIZE( ptr->mnorm );
		if( VDOT( ptr->mnorm , ptr->nnext ) > 0.0 )
			VREVERSE( ptr->mnorm , ptr->mnorm );
		ptr->alpha = acos( VDOT( ptr->nnext , ptr->nprev ) );
		ptr = ptr->next;
	}
}

void
Adjust()
{
	fastf_t beta,d,len;
	struct points *ptr;

	if( root == NULL )
		return;

	VMOVE( root->p1 , root->p );
	VMOVE( root->p2 , root->p );
	
	ptr = root->next;
	if( ptr == NULL )
		return;

	if( ptr->next == NULL )
	{
		VMOVE( ptr->p1 , ptr->p );
		VMOVE( ptr->p2 , ptr->p );
		return;
	}


	while( ptr->next != NULL )
	{	
		if( !torus && !mitre )
		{
			VMOVE( ptr->p1 , ptr->p );
			VMOVE( ptr->p2 , ptr->p );
		}
		else if( torus )
		{
/*			beta=.5*( pi-ptr->alpha );	
			d=( MINR+1.0 )*radius*tan( beta ); */ /* dist from new endpts to p2 */

			beta = 0.5 * ptr->alpha;
			d = (MINR + 1.0) * radius / tan( beta );
			VJOIN1( ptr->p1 , ptr->p , d , ptr->nprev );
			VJOIN1( ptr->p2 , ptr->p , d , ptr->nnext );
			d = sqrt( d*d + (MINR+1.0)*(MINR+1.)*radius*radius );
			VJOIN1( ptr->center , ptr->p , d , ptr->nmitre );
		}
		else if( mitre )
		{
			len = radius/tan(ptr->alpha/2.0);
			VJOIN1( ptr->p1 , ptr->p , -len , ptr->nprev );
			VJOIN1( ptr->p2 , ptr->p , -len , ptr->nnext );
		}
		ptr = ptr->next;
	}
	VMOVE( ptr->p1 , ptr->p );
	VMOVE( ptr->p2 , ptr->p );
}

void
Pipes()
{
	vect_t ht;
	struct points *ptr;
	fastf_t len;
	int comblen;

	ptr = root;
	if( ptr == NULL )
		return;

	while( ptr->next != NULL )
	{

		/* Make the basic pipe solids */

		VSUB2( ht , ptr->next->p1 , ptr->p2);

		mk_rcc( fdout , ptr->tube , ptr->p2 , ht , radius );	/* make a solid record	     */

		if( !cable ) /* make inside solid */
			mk_rcc( fdout , ptr->tubflu , ptr->p2 , ht , radius-wall );

		if( torus )
		{
			/* Make tubing region */
			mk_fcomb( fdout , ptr->tube_r , 2-cable , REGION );	/* make REGION  comb record	   */
			mk_memb( fdout , ptr->tube , identity , UNION );	/* make 'u' member record	   */
			if( !cable )
			{
				/* Subtract inside solid */
				mk_memb( fdout , ptr->tubflu , identity , SUBTRACT );	/* make '-' member record	   */

				/* Make fluid region */
				mk_fcomb( fdout , ptr->tubflu_r , 1 , REGION );		/* make REGION comb record	*/
				mk_memb( fdout , ptr->tubflu , identity , UNION );	/* make 'u' member record	*/
			}
		}
		else if( mitre )
		{
			if( ptr->prev != NULL )
			{
				len = VDOT( ptr->p , ptr->mnorm );
				mk_half( fdout , ptr->cut , ptr->mnorm , len );
			}

			comblen = 4 - cable;
			if( ptr->next->next == NULL )
				comblen--;
			if( ptr->prev == NULL )
				comblen--;

			mk_fcomb( fdout , ptr->tube_r , comblen , REGION );		/* make REGION  comb record	   */
			mk_memb( fdout , ptr->tube , identity , UNION );	/* make 'u' member record	   */

			if( !cable )
				mk_memb( fdout , ptr->tubflu , identity , SUBTRACT );	/* make '-' member record	   */

			if( ptr->next->next != NULL )
				mk_memb( fdout , ptr->next->cut , identity , SUBTRACT );	/* make '+' member record	   */

			if( ptr->prev != NULL )
				mk_memb( fdout , ptr->cut , identity , INTERSECT );	/* subtract HAF */

			if( !cable )
			{
				/* Make fluid region */

				comblen = 3;
				if( ptr->next->next == NULL )
					comblen--;
				if( ptr->prev == NULL )
					comblen--;

				mk_fcomb( fdout , ptr->tubflu_r , comblen , REGION );	/* make REGION comb record	*/

				mk_memb( fdout , ptr->tubflu , identity , UNION );	/* make 'u' member record	*/

				if( ptr->next->next != NULL )
					mk_memb( fdout , ptr->next->cut , identity , SUBTRACT );	/* make '+' member record	   */

				if( ptr->prev != NULL )
					mk_memb( fdout , ptr->cut , identity , INTERSECT );	/* subtract */
			}
		}
		else if( sphere )
		{

			/* make REGION  comb record for tube */
			comblen = 2;
			if( cable )
				comblen--;
			if( ptr->next->tube[0] != '\0' )
				comblen++;
			if( !cable && ptr->prev != NULL )
				comblen++;
			
			mk_fcomb( fdout , ptr->tube_r , comblen , REGION );

			/* make 'u' member record	   */

			mk_memb( fdout , ptr->tube , identity , UNION );

			/* make '-' member record	   */
			if( !cable )
				mk_memb( fdout , ptr->tubflu , identity , SUBTRACT );

			if( ptr->next->tube[0] != '\0' )	/* subtract outside of next tube */
				mk_memb( fdout , ptr->next->tube , identity , SUBTRACT );
			if( !cable && ptr->prev != NULL ) /* subtract inside of previous tube */
				mk_memb( fdout , ptr->prev->tubflu , identity , SUBTRACT );

			if( !cable )
			{
				/* make REGION for fluid */
			
				if( ptr->next->tubflu[0] != '\0' )	/* there will be 2 members */
					mk_fcomb( fdout , ptr->tubflu_r , 2 , REGION );
				else		/* only 1 member */
					mk_fcomb( fdout , ptr->tubflu_r , 1 , REGION );

				/* make 'u' member record	*/

				mk_memb( fdout , ptr->tubflu , identity , UNION );

				if( ptr->next->tubflu[0] != '\0'  )	/* subtract inside of next tube */
					mk_memb( fdout , ptr->next->tubflu , identity , SUBTRACT );
			}
		}
		else if( nothing )
		{

			/* make REGION  comb record for tube */
			comblen = 2;
			if( cable )
				comblen--;
			if( ptr->next->tube[0] != '\0' )
				comblen++;
			if( !cable && ptr->prev != NULL )
				comblen++;
			
			mk_fcomb( fdout , ptr->tube_r , comblen , REGION );

			/* make 'u' member record	   */

			mk_memb( fdout , ptr->tube , identity , UNION );

			/* make '-' member record	   */
			if( !cable )
				mk_memb( fdout , ptr->tubflu , identity , SUBTRACT );

			if( ptr->next->tube[0] != '\0' )	/* subtract outside of next tube */
				mk_memb( fdout , ptr->next->tube , identity , SUBTRACT );
			if( !cable && ptr->prev != NULL ) /* subtract inside of previous tube */
				mk_memb( fdout , ptr->prev->tubflu , identity , SUBTRACT );

			if( !cable )
			{
				/* make REGION for fluid */
			
				if( ptr->next->tubflu[0] != '\0' )	/* there will be 2 members */
					mk_fcomb( fdout , ptr->tubflu_r , 2 , REGION );
				else		/* only 1 member */
					mk_fcomb( fdout , ptr->tubflu_r , 1 , REGION );

				/* make 'u' member record	*/

				mk_memb( fdout , ptr->tubflu , identity , UNION );

				if( ptr->next->tubflu[0] != '\0'  )	/* subtract inside of next tube */
					mk_memb( fdout , ptr->next->tubflu , identity , SUBTRACT );
			}
		}
		ptr = ptr->next;
	}
}

void
Elbows()	/* make a tubing elbow and fluid elbow */
{
	vect_t RN1,RN2;
	point_t pts[8];
	fastf_t len;
	struct points *ptr;
	int comblen;

	if( nothing || mitre )
		return;

	if( root == NULL )
		return;

	ptr = root->next;
	while( ptr->next != NULL )
	{
		
		/* Make outside elbow solid */
		if( torus )
			mk_tor( fdout , ptr->elbow , ptr->center , ptr->norm , ( MINR+1 )*radius , radius );
		else if( sphere )
			mk_sph( fdout , ptr->elbow , ptr->p , radius );

		if( !cable ) /* Make inside elbow solid */
		{
			if( torus )
				mk_tor( fdout , ptr->elbflu , ptr->center , ptr->norm , ( MINR+1 )*radius , radius-wall );
			else if( sphere )
				mk_sph( fdout , ptr->elbflu , ptr->p , radius-wall );
		}

		if( torus )	/* Make ARB8 solid */
		{
			len = ((MINR+2)*radius + delta) / cos( (pi-ptr->alpha)/4.0 );
				/* vector from center of torus to rcc end */
			VSUB2( RN1 , ptr->p1 , ptr->center );
			VUNITIZE( RN1 );		/* unit vector */
				/* beginning of next rcc */
			VSUB2( RN2 , ptr->p2 , ptr->center );
			VUNITIZE( RN2 );		/* and unitize again */

			/* build the eight points for the ARB8 */

			VJOIN1( pts[0] , ptr->center , radius+delta , ptr->norm );
			VJOIN1( pts[1] , pts[0] , len , RN1 );
			VJOIN1( pts[2] , pts[0] , -len , ptr->nmitre );
			VJOIN1( pts[3] , pts[0] , len , RN2 );
			VJOIN1( pts[4] , ptr->center , -radius-delta , ptr->norm );
			VJOIN1( pts[5] , pts[4] , len , RN1 );
			VJOIN1( pts[6] , pts[4] , -len , ptr->nmitre );
			VJOIN1( pts[7] , pts[4] , len , RN2 );
		
			mk_arb8( fdout , ptr->cut , &pts[0][X] );
		}

		if( torus )
		{
			mk_fcomb( fdout , ptr->elbow_r , 3-cable , REGION );	/* make REGION  comb record	   */
			mk_memb( fdout , ptr->elbow , identity , UNION );	/* make 'u' member record	   */
			if( !cable )
				mk_memb( fdout , ptr->elbflu , identity , SUBTRACT );	/* make '-' member record	   */
			mk_memb( fdout , ptr->cut , identity , INTERSECT );

			if( !cable )
			{
				mk_fcomb( fdout , ptr->elbflu_r , 2 , REGION );		/* make REGION comb record	*/
				mk_memb( fdout , ptr->elbflu , identity , UNION );	/* make 'u' member record	*/
				mk_memb( fdout , ptr->cut , identity , INTERSECT );
			}
		}
		else if( sphere )
		{
			comblen = 4 - cable;
			mk_fcomb( fdout , ptr->elbow_r , comblen , REGION );	/* make REGION  comb record	   */
			mk_memb( fdout , ptr->elbow , identity , UNION );	/* make 'u' member record	   */
			if( !cable )
				mk_memb( fdout , ptr->elbflu , identity , SUBTRACT );	/* make '-' member record	   */
			mk_memb( fdout , ptr->tube , identity , SUBTRACT );
			mk_memb( fdout , ptr->prev->tube , identity , SUBTRACT );	

			if( !cable )
			{
				comblen = 3;
				mk_fcomb( fdout , ptr->elbflu_r , 3 , REGION );		/* make REGION comb record	*/
				mk_memb( fdout , ptr->elbflu , identity , UNION );	/* make 'u' member record	*/
				mk_memb( fdout , ptr->tube , identity , SUBTRACT );
				mk_memb( fdout , ptr->prev->tube , identity , SUBTRACT );	
			}
		}
		ptr = ptr->next;
	}
}

void
Groups()
{
	struct points *ptr;
	char tag[NAMESIZE];
	char *pipe_group=".pipe";
	char *fluid_group=".fluid";
	int comblen=0;

	ptr = root;
	if( ptr == NULL )
		return;

	while( ptr->next != NULL )
	{
		if( !nothing && !mitre && comblen)	/* count elbow sections (except first point) */
			comblen++;
		comblen++;	/* count pipe sections */

		ptr = ptr->next;
	}

	if( comblen )
	{

		/* Make name for pipe group = "name".pipe */
		Make_name( tag , pipe_group , name , 0 );

		/* Make group */
		mk_fcomb( fdout , tag , comblen , 0 );
		ptr = root;
		while( ptr->next != NULL )
		{
			mk_memb( fdout , ptr->tube_r , identity , UNION );	/* tube regions */

			if( !nothing && !mitre && ptr != root )
				mk_memb( fdout , ptr->elbow_r , identity , UNION );	/* elbows */

			ptr = ptr->next;
		}

		if( !cable )
		{
			/* Make name for fluid group = "name".fluid */
			Make_name( tag , fluid_group , name , 0 );

			/* Make group */
			mk_fcomb( fdout , tag , comblen , 0 );
			ptr = root;
			while( ptr->next != NULL )
			{
				mk_memb( fdout , ptr->tubflu_r , identity , UNION );	/* fluid in tubes */
				
				if( !nothing && !mitre && ptr != root )
					mk_memb( fdout , ptr->elbflu_r , identity , UNION ); /* fluid in elbows */
				
					ptr = ptr->next;
			}
		}
	}
}

main(argc, argv)
int argc;
char *argv[];
{
	int done;
	char units[16],fname[80];
	extern int optind;
	int optc;

	while( (optc = getopt( argc,argv,"tsmnc" )) != -1)
	{
		switch( optc )	/* Set joint type and cable option */
		{
			case 't':
				torus = 1;
				break;
			case 's':
				sphere = 1;
				break;
			case 'm':
				mitre = 1;
				break;
			case 'n':
				nothing = 1;
				break;
			case 'c':
				cable = 1;
				break;
			case '?':
				fprintf( stderr , "Illegal option %c\n" , optc );
				Usage();
				exit( 1 );
				break;
		
		}
	}

	if( (torus + sphere + mitre + nothing) > 1 ) /* Too many joint options */
	{
		Usage();
		fprintf( stderr , "Options t,s,m,n are mutually exclusive\n" );
		exit( 1 );
	}
	else if( (torus + sphere + mitre + nothing) == 0 ) {
		torus = 1;		/* default */
	}

	if( (argc - optind) != 2 ) {
		Usage();
		exit( 1 );
	}

	strcpy( name , argv[optind++] ); /* Base name for objects */

	fdout = fopen( argv[optind] , "w" );
	if( fdout == NULL )
	{
		fprintf( stderr , "Cannot open %s\n" , argv[optind] );
		perror( "Pipe" );
		Usage();
		exit( 1 );
	}

	mat_idn(identity);	/* Identity matrix for all objects */
	pi = atan2( 0.0 , -1.0 );	/* PI */

	printf( "FLUID & PIPING V%d.%d 10 Mar 89\n\n" , VERSION , RELEASE );
	printf( "append %s to your target description using 'concat' in mged\n" , argv[optind] );

	k = 0.0;
	while( k == 0.0 )
	{
		printf( "UNITS? (ft,in,m,cm, default is millimeters) ");
		fgets(units, sizeof(units), stdin);	
		switch (units[0])
		{

			case '\0':
				k = 1.0;
				break;

			case 'f':
				k = 12*25.4;
				break;

			case 'i':
				k=25.4;
				break;

			case 'm':
				if ( units[1] == '\0') k=1000.0;
				else k=1.0;
				break;

			case 'c':
				k=10.0;
				break;

			default:
				k=0.0;
				printf( "\n\t%s is not a legal choice for units\n" , units );
				printf( "\tTry again\n" );
				break;
		}
	}

	done = 0;
	while( !done )
	{
		if( !cable ) {
			printf( "radius and wall thickness: ");	
			if (scanf("%lf %lf", &radius,&wall) == EOF )
				exit( 1 );
			if (radius > wall)
				done = 1;
			else
			{
				printf( " *** bad input!\n\n");
				printf( "\tradius must be larger than wall thickness\n" );
				printf( "\tTry again\n" );
			}
		}
		else {
			printf( "radius: ");
			if( scanf("%lf", &radius ) == EOF )
				exit( 1 );
			done=1;
		}
	}
	radius=k*radius;
	wall=k*wall;
	
	Readpoints();	/* Read data points */

	Names();	/* Construct names for all solids */

	Normals();	/* Calculate normals and other vectors */

        Adjust();       /* Adjust points to allow for elbows */

/*	Generate Title */

	strcpy(fname,name);
	if( !cable )
		strcat(fname," pipe and fluid");
	else
		strcat(fname," cable");

/*	Create ident record	*/

	mk_id(fdout,fname);

	Pipes();	/* Construct the piping */

	Elbows();	/* Construct the elbow sections */

	Groups();	/* Make some groups */

	exit( 0 );

}

void
Usage()
{
	fprintf( stderr , "Usage: pipe [-tsmnc] tag filename\n");
	fprintf( stderr , "   where 'tag' is the name of the piping run\n");
	fprintf( stderr , "   and 'filename' is the .g file (e.g., fuel.g)\n");
	fprintf( stderr , "   -t -> use tori at the bends (default)\n" );
	fprintf( stderr , "   -s -> use spheres at the corners\n" );
	fprintf( stderr , "   -m -> mitre the corners\n" );
	fprintf( stderr , "   -n -> nothing at the corners\n" );
	fprintf( stderr , "   -c -> cable (no fluid)\n" );
}
