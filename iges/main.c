/*
 *			I G E S - G / M A I N . C
 *
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <errno.h>

#include "machine.h"
#include "externs.h"		/* For getopt */
#include "vmath.h"
#include "./iges_struct.h"
#include "./iges_types.h"

char eor,eof,card[256];
fastf_t scale,inv_scale,conv_factor;
int units,counter,pstart,dstart,totentities,dirarraylen;
FILE *fd,*fdout;
char brlcad_file[256];
int reclen,currec,ntypes;
int brlcad_att_de=0;
struct iges_directory **dir;
struct reglist *regroot;
struct iges_edge_list *edge_root;
struct iges_vertex_list *vertex_root;
struct rt_tol tol;

char operator[]={
	' ',
	'u',
	'+',
	'-' };

mat_t *identity;

extern char	version[];

static int do_splines=0;
static int do_drawings=0;
static int trimmed_surf=0;
int do_polysolids=0;

static char *iges_file;

static char *msg1=
"\nThis IGES file contains solid model entities, but your options do not permit\n\
converting them to BRL-CAD. You may want to try 'iges-g -o file.g %s' to\n\
convert the solid model elements\n";

static char *msg2=
"\nThis IGES file contains drawing entities, but no solid model entities. You may\n\
convert the drawing to BRL-CAD by 'iges-g -d -o file.g %s'. Note that the resulting\n\
BRL-CAD object will be a 2D drawing, not a solid object\n";

static char *msg3=
"\nThis IGES file contains spline surfaces, but no solid model entities. All the spline\n\
surfaces in the IGES file may be combined into a single BRL-CAD spline solid by\n\
'iges-g -n -o file.g %s'\n";

static char *msg4=
"\nThis IGES file contains trimmed surfaces, but no solid model entities.\n\
A '-t' option for 'iges-g' is under construction that will eventually allow you\n\
to convert all the trimmed surfaces in an IGES file into one BRL-CAD solid.\n\
Sorry this isn't complete yet.\n";

void
Suggestions()
{
	int i;
	int csg=0;
	int brep=0;
	int splines=0;
	int tsurfs=0;
	int drawing=0;

	/* categorize the elements in the IGES file as to whether they are
	 * CSG, BREP, Trimmed surfaces, Spline surfaces, or drawing elements
	 */
	for( i=0 ; i<NTYPES ; i++ )
	{
		if( typecount[i].type >= 150 && typecount[i].type <= 184 ||
		    typecount[i].type == 430 )
			csg += typecount[i].count;
		else if( typecount[i].type == 186 ||
			 typecount[i].type >= 502 && typecount[i].type <=514 )
			brep += typecount[i].count;
		else if( typecount[i].type == 128 )
			splines += typecount[i].count;
		else if( typecount[i].type == 144 )
			tsurfs += typecount[i].count;
		else if( typecount[i].type >= 100 && typecount[i].type <= 112 ||
			 typecount[i].type == 126 ||
			 typecount[i].type >= 202 && typecount[i].type <= 230 ||
			 typecount[i].type == 404 || typecount[i].type == 410 )
			drawing += typecount[i].count;
	}

	if( (csg || brep) && (do_splines || do_drawings || trimmed_surf ) )
		printf( msg1 , iges_file );

	if( drawing && csg == 0 && brep == 0 && !do_drawings )
		printf( msg2 , iges_file );

	if( splines && csg == 0 && brep == 0 && !do_splines )
		printf( msg3 , iges_file );

	if( tsurfs && csg == 0 && brep == 0 && !trimmed_surf )
		printf( msg4 , iges_file );
}

main( argc , argv )
int argc;
char *argv[];
{
	int i;
	int c;
	char *output_file=(char *)NULL;

	while( (c=getopt( argc , argv , "dntpo:" )) != EOF )
	{
		switch( c )
		{
			case 'd':
				do_drawings = 1;
				break;
			case 'n':
				do_splines = 1;
				break;
			case 'o':
				output_file = optarg;
				break;
			case 't':
				trimmed_surf = 1;
				break;
			case 'p':
				do_polysolids = 1;
				break;
		}
	}

	if (optind >= argc || output_file == (char *)NULL || do_drawings+do_splines+trimmed_surf > 1) {
		usage();
		exit(1);
	}

	printf( "%s", version+5);
	printf( "Please direct bug reports to <jra@brl.mil>\n\n" );

	/* Initialize some variables */
	ntypes = NTYPES;
	regroot = NULL;
	edge_root = NULL;
	vertex_root = NULL;
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	argc -= optind;
	argv += optind;
	iges_file = argv[0];
	fd = fopen( argv[0] , "r" );	/* open IGES file */
	if( fd == NULL )
	{
		fprintf( stderr , "Cannot open %s\n" , argv[0] );
		perror( "iges-g" );
		usage();
		exit( 1 );
	}

	if( (fdout = fopen( output_file , "w" )) == NULL )
	{
		fprintf( stderr , "Cannot open %s\n" , output_file );
		perror( "iges-g" );
		usage();
		exit( 1 );
	}
	strcpy( brlcad_file ,  output_file );

	reclen = Recsize() * sizeof( char ); /* Check length of records */
	if( reclen == 0 )
	{
		fprintf( stderr , "File not in IGES ASCII format\n" );
		exit(1);
	}

	identity = (mat_t *)malloc( sizeof( mat_t ) );
	for( i=0 ; i<16 ; i++ )
	{
		if( !(i%5) )
			(*identity)[i] = 1.0;
		else
			(*identity)[i] = 0.0;
	}

	Initstack();	/* Initialize node stack */

	Readstart();	/* Read start section */

	Readglobal();	/* Read global section */

	pstart = Findp();	/* Find start of parameter section */

	Makedir();	/* Read directory section and build a linked list of entries */

	Summary();	/* Print a summary of what is in the IGES file */

	Docolor();	/* Get color info from color definition entities */

	Get_att();	/* Look for a BRLCAD attribute definition */

	Evalxform();	/* Accumulate the transformation matrices */

	Check_names();	/* Look for name entities */

	if( do_drawings )
		Conv_drawings();	/* convert drawings to wire edges */
	else if( trimmed_surf )
		Convtrimsurfs();	/* try to convert trimmed surfaces to a single solid */
	else if( do_splines )
		Convsurfs();		/* Convert NURBS to a single solid */
	else
	{
		Convinst();	/* Handle Instances */

		Convsolids();	/* Convert solid entities */

		Convtree();	/* Convert Boolean Trees */

		Convassem();	/* Convert solid assemblies */
	}

	Suggestions();
}
