/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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
 */
/** @file main.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "./iges_struct.h"
#include "./iges_types.h"
#include "../librt/debug.h"

int do_projection=1;
char eof,eor,card[256];
fastf_t scale,inv_scale,conv_factor;
int units,counter,pstart,dstart,totentities,dirarraylen;
FILE *fd;
struct rt_wdb *fdout;
char brlcad_file[256];
int reclen,currec,ntypes;
int brlcad_att_de=0;
struct iges_directory **dir;
struct reglist *regroot;
struct iges_edge_list *edge_root;
struct iges_vertex_list *vertex_root;
struct bn_tol tol;
char *solid_name=(char *)NULL;
struct file_list iges_list;
struct file_list *curr_file;
struct name_list *name_root;

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
int do_bots=1;

static char *iges_file;

static char *msg1=
"\nThis IGES file contains solid model entities, but your options do not permit\n\
converting them to BRL-CAD. You may want to try 'iges-g -o file.g %s' to\n\
convert the solid model elements\n";

static char *msg2=
"\nThis IGES file contains drawing entities, but no solid model entities. You may\n\
convert the drawing to BRL-CAD by 'iges-g -d -o file.g %s'. Note that the resulting\n\
BRL-CAD object will be a 2D drawing, not a solid object. You might also try the\n\
'-3' option to get 3D drawings\n";

static char *msg3=
"\nThis IGES file contains spline surfaces, but no solid model entities. All the spline\n\
surfaces in the IGES file may be combined into a single BRL-CAD spline solid by\n\
'iges-g -n -o file.g %s'\n";

static char *msg4=
"\nThis IGES file contains trimmed surfaces, but no solid model entities.\n\
Try the '-t' option to convert all the trimmed surfaces into one BRL-CAD solid.\n\
'iges-g -t -o file.g %s'\n";

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
		if( (typecount[i].type >= 150 && typecount[i].type <= 184) ||
		    typecount[i].type == 430 )
			csg += typecount[i].count;
		else if( typecount[i].type == 186 ||
			 (typecount[i].type >= 502 && typecount[i].type <=514) )
			brep += typecount[i].count;
		else if( typecount[i].type == 128 )
			splines += typecount[i].count;
		else if( typecount[i].type == 144 )
			tsurfs += typecount[i].count;
		else if( (typecount[i].type >= 100 && typecount[i].type <= 112) ||
			 typecount[i].type == 126 ||
			 (typecount[i].type >= 202 && typecount[i].type <= 230) ||
			 typecount[i].type == 404 || typecount[i].type == 410 )
			drawing += typecount[i].count;
	}

	if( (csg || brep) && (do_splines || do_drawings || trimmed_surf ) )
		bu_log( msg1 , iges_file );

	if( drawing && csg == 0 && brep == 0 && !do_drawings )
		bu_log( msg2 , iges_file );

	if( splines && csg == 0 && brep == 0 && !do_splines )
		bu_log( msg3 , iges_file );

	if( tsurfs && csg == 0 && brep == 0 && !trimmed_surf )
		bu_log( msg4 , iges_file );
}

int
main( argc , argv )
int argc;
char *argv[];
{
	int i;
	int c;
	int file_count=0;
	char *output_file=(char *)NULL;


	while( (c=bu_getopt( argc , argv , "3dntpo:x:X:N:" )) != EOF )
	{
		switch( c )
		{
			case '3':
				do_drawings = 1;
				do_projection = 0;
				break;
			case 'd':
				do_drawings = 1;
				break;
			case 'n':
				do_splines = 1;
				break;
			case 'o':
				output_file = bu_optarg;
				break;
			case 't':
				trimmed_surf = 1;
				break;
			case 'p':
				do_bots = 0;
				break;
			case 'N':
				solid_name = bu_optarg;
				break;
			case 'x':
				sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.debug );
				if( RT_G_DEBUG & DEBUG_MEM )
					bu_debug |= BU_DEBUG_MEM_LOG;
				if( RT_G_DEBUG & DEBUG_MEM_FULL )
					bu_debug |= BU_DEBUG_MEM_CHECK;
				break;
			case 'X':
				sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug );
				break;
			default:
				usage();
				exit(1);
				break;
		}
	}

	if (bu_optind >= argc || output_file == (char *)NULL || do_drawings+do_splines+trimmed_surf > 1) {
		usage();
		exit(1);
	}

	if( bu_debug & BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Memory checking enabled\n" );
		bu_mem_barriercheck();
	}

	bu_log( "%s", version+5);
	bu_log( "Please direct bug reports to <bugs@brlcad.org>\n\n" );

	/* Initialize some variables */
	ntypes = NTYPES;
	regroot = NULL;
	edge_root = NULL;
	vertex_root = NULL;
	name_root = NULL;
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	Initstack();	/* Initialize node stack */

	identity = (mat_t *)bu_malloc( sizeof( mat_t ), "main: identity" );
	for( i=0 ; i<16 ; i++ )
	{
		if( !(i%5) )
			(*identity)[i] = 1.0;
		else
			(*identity)[i] = 0.0;
	}

	if( (fdout = wdb_fopen( output_file )) == NULL )
	{
		bu_log( "Cannot open %s\n" , output_file );
		perror( "iges-g" );
		usage();
		exit( 1 );
	}
	strcpy( brlcad_file ,  output_file );

	argc -= bu_optind;
	argv += bu_optind;

	BU_LIST_INIT( &iges_list.l );
	curr_file = (struct file_list *)bu_malloc( sizeof( struct file_list ), "iges-g: curr_file" );
	if( solid_name )
		strcpy( curr_file->obj_name, Make_unique_brl_name( solid_name ) );
	else
		strcpy( curr_file->obj_name, Make_unique_brl_name( "all" ) );

	curr_file->file_name = (char *)bu_malloc( strlen( argv[0] ) + 1, "iges-g: curr_file->file_name" );
	strcpy( curr_file->file_name, argv[0] );
	BU_LIST_APPEND( &iges_list.l, &curr_file->l );

	while( BU_LIST_NON_EMPTY( &iges_list.l ) )
	{
		if( RT_G_DEBUG & DEBUG_MEM_FULL )
			bu_mem_barriercheck();

		curr_file = BU_LIST_FIRST( file_list, &iges_list.l );
		iges_file = curr_file->file_name;

#ifdef _WIN32
		fd = fopen( iges_file , "rb" );	/* open IGES file */
#else
		fd = fopen( iges_file , "r" );	/* open IGES file */
#endif
		if( fd == NULL )
		{
			bu_log( "Cannot open %s\n" , iges_file );
			perror( "iges-g" );
			usage();
			exit( 1 );
		}

		bu_log( "\n\n\nIGES FILE: %s\n", iges_file );

		reclen = Recsize() * sizeof( char ); /* Check length of records */
		if( reclen == 0 )
		{
			bu_log( "File (%s) not in IGES ASCII format\n", iges_file );
			exit(1);
		}

		Freestack();	/* Set node stack to empty */

		Zero_counts();	/* Set summary information to all zeros */

		Readstart();	/* Read start section */

		Readglobal( file_count);	/* Read global section */

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
		{
			Do_subfigs();		/* Look for Singular Subfigure Instances */

			Convtrimsurfs();	/* try to convert trimmed surfaces to a single solid */
		}
		else if( do_splines )
			Convsurfs();		/* Convert NURBS to a single solid */
		else
		{
			Convinst();	/* Handle Instances */

			Convsolids();	/* Convert solid entities */

			Convtree();	/* Convert Boolean Trees */

			Convassem();	/* Convert solid assemblies */
		}

		if( RT_G_DEBUG & DEBUG_MEM_FULL )
			bu_mem_barriercheck();

		Free_dir();

		if( RT_G_DEBUG & DEBUG_MEM_FULL )
			bu_mem_barriercheck();

		BU_LIST_DEQUEUE( &curr_file->l );
		bu_free( (char *)curr_file->file_name, "iges-g: curr_file->file_name" );
		bu_free( (char *)curr_file, "iges-g: curr_file" );
		file_count++;

		if( RT_G_DEBUG & DEBUG_MEM_FULL )
			bu_mem_barriercheck();

	}

	iges_file = argv[0];
	Suggestions();
	return 0;
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
