/*
 *			P A T C H - G . C
 *
 *	Converts FASTGEN format target descriptions to MGED format.
 *	  This version assumes the following FASTGEN primitives:
 *		0 - triangle (< 0.99" thick for plate mode)
 *		1 - triangle (< 1.99" thick but > 0.99")
 *		2 - triangle (< 2.99" thick but > 1.99")
 *		3 - triangle (< 3.99" thick but > 2.99")
 *		4 - donut / torus (changed from type 3 by rpatch)
 *		5 - wedge
 *		6 - sphere
 *		7 - box
 *		8 - cylinder (24pt type-4's converted by rpatch)
 *		9 - rod
 *	  The target is also assumed to be pointing forward along the
 *	  positive X-axis and positive Y on the left side of the target.
 *
 *  Author -
 *	Bill Mermagen Jr.
 *      Dan Dender
 *  Revisions -
 *	Robert L. Strausser, The SURVICE Engineering Co.; Jul. '93
 *
 *	John R. Anderson (Jan '94)
 *		Converted proc_tri and proc_plate to produce NMG's
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"
#include "./patch-g.h"
#include "../rt/mathtab.h"

RT_EXTERN( struct shell *nmg_dup_shell , ( struct shell *s , long ***trans_tbl, struct bn_tol *tol ) );
RT_EXTERN( fastf_t mat_determinant, ( CONST mat_t m ) );

#define ABS( _x )	(( _x > 0.0 )? _x : (-_x))

void	proc_plate();
void	proc_label();
void	proc_triangle();
void	proc_wedge();
void	proc_sphere();
void	proc_box();
void	proc_cylin();
void	proc_rod();
void	set_color();
void	mk_cyladdmember();
void	proc_donut();

static struct bn_tol	tol;

static char usage[] = "\
Usage: patch-g [options] > model.g\n\
	-f fastgen.rp	specify pre-processed fastgen file (default stdin)\n\
	-a		process phantom armor?\n\
	-n		process volume mode as plate mode?\n\
	-u #		number of union operations per region (default 5)\n\
	-c \"x y z\"	center of object in inches (for some surface normal calculations)\n\
	-t title	optional title (default \"Untitled MGED database\")\n\
	-o object_name	optional top-level name (no spaces)(default \"all\")\n\
	-p		write volume and plate mode components as polysolids\n\
	-i group.file	specify group labels source file\n\
	-m mat.file	specify materials information source file\n\
	-r		reverse normals for plate mode triangles\n\
	-d #		debug level\n\
	-x #		librt debug flag\n\
	-X #		librt NMG debug flags\n\
	-T #		distance tolerance (inches) (two points within this distance are the same point )\n\
	-A #		parallel tolerance (if A dot B (unit vectors) is less than this value, they are perpendicular )\n\
Note: fastgen.rp is the pre-processed (through rpatch) fastgen file\n";

main(argc,argv)
int	argc;
char	*argv[];
{

	fastf_t  x1, y1, z1;
	int numf = 3;
	int fd,fl,nread;
	FILE	*gfp=NULL;
	FILE    *mfp=NULL;
	char	buf[99],s[132+2];
	int	c;
	int j = 1;
	int i;
	int done;
	int stop,num;
	char *next, *ptr;
	char *matname = "plastic";
	char *matparm = "sh=100.0,sp=.9,di=.1";
	char hold = 0;
	char	name[17];

	RT_LIST_INIT( &head.l);
	RT_LIST_INIT( &heada.l);
	RT_LIST_INIT( &headb.l);
	RT_LIST_INIT( &headc.l);
	RT_LIST_INIT( &headd.l);
	RT_LIST_INIT( &heade.l);
	RT_LIST_INIT( &headf.l);

	bzero( (char *)list,sizeof(list));

        /* initialize tolerance structure */
        tol.magic = RT_TOL_MAGIC;
        tol.dist = 0.01;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 0.001;
        tol.para = 1 - tol.perp;

	conv_mm2in = 1.0/25.4; /* convert mm to inches */

	if ( isatty(fileno(stdout)) ){
		(void)fputs("attempting to send binary output to tty, aborting!\n",stderr);
		(void)fputs(usage, stderr);
		exit(1);
	}

	/*     This section checks usage options given at run command time.   */

	/* Get command line arguments. */
	while ((c = getopt(argc, argv, "A:T:x:X:pf:i:m:anu:t:o:rc:d:")) != EOF)
	{
		switch (c)
		{
			case 'T':  /* tolerance distance */

				tol.dist = atof( optarg );
				if( tol.dist < 0.0 )
				{
					bu_log( "Illegal tolerance distance (%g inches) (must be non-negative).\n", tol.dist );
					bu_log(usage);
					exit( 1 );
				}
				tol.dist *= mmtin;
				tol.dist_sq = tol.dist * tol.dist;
				break;

			case 'A':  /* angular tolerance */

				tol.perp = atof( optarg );
				if( tol.perp < 0.0 || tol.perp > 1.0 )
				{
					bu_log( "Illegal angular tolerance (%g) (must be non-negative between 0 and 1).\n", tol.perp );
					bu_log(usage);
					exit( 1 );
				}
				tol.para = 1 - tol.perp;
				break;

			case 'x':  /* librt debug flags */

				sscanf( optarg , "%x" , &rt_g.debug );
				break;

			case 'X':  /* librt NMG debug flags */

				sscanf( optarg , "%x" , &rt_g.NMG_debug );
				break;

			case 'p':  /* polysolid output */

				polysolid = 1;
				break;

			case 'f':  /* fastgen source file data */

				patchfile = optarg;
				break;

			case 'i':  /* group labels source file */

				labelfile = optarg;
				break;

			case 'm':  /* materials information file */

				matfile = optarg;
				break;

			case 'a':  /* process phantom armor ? */

				aflg++;
				break;

			case 'n':  /* process volume mode as plate mode ? */

				nflg = 0;
				break;

			case 'u':  /* specify number of union operations
				    * to put in a region 
							    */

				if( (num_unions = atoi( optarg )) <= 0 ) {
					rt_log( "%d: bad number of unions to put in a region\n", num_unions );
					exit( 1 );
				}
				break;

			case 't':  /* optional title for the database */

				title = optarg;
				break;

			case 'o':  /* optional top-level object name */

				top_level = optarg;
				break;

			case 'r':  /* reverse normals for plate mode triangles */

				rev_norms++;
				break;

			case 'c':  /* center of object (used for some plate mode
				    * triangle surface normal calculations
							    */
				sscanf( optarg,"%lf %lf %lf", 
				    &Centroid[0],&Centroid[1],&Centroid[2]);

				rt_log( "Centroid = ( %f %f %f )\n" , V3ARGS( Centroid ) );
				VSCALE( Centroid, Centroid, mmtin );
				break;

			case 'd':  /* debug flag checking */

				debug = atoi(optarg); /* Debug level */
				break;

			default:
				(void)fputs(usage, stderr);
				exit(1);
			}
	}

	if( debug )
		rt_log( "debug level = %d\n" , debug );

	if( rt_g.debug )
	{
		rt_printb( "librt rt_g.debug", rt_g.debug, DEBUG_FORMAT );
		rt_log( "\n" );
	}

	if( rt_g.NMG_debug )
	{
		rt_log( "librt rt_g.NMG_debug = x%x\n" , rt_g.NMG_debug );
	}

	/*     This section opens input files - the data file defaults to standard
	     input,  both files provide error checking for failure to open.      */

	if( patchfile != (char *)0 )  {
		if((fd = open( patchfile, 0664)) < 0) {
			perror(patchfile);
			exit(1);
		}
	} else {
		fd = 0;		/* stdin */
		patchfile = "stdin";
	}

	if( labelfile != (char *)0 )  {
		if(( gfp = fopen( labelfile, "r" )) == NULL ) {
			perror(labelfile);
			exit(1);
		}
	}

	if( matfile != (char *)0 ) {
		if(( mfp = fopen( matfile, "r" )) == NULL ) {
			perror(matfile);
			exit(1);
		}
	}

	/*     This is the primary processing section to input fastgen data and
	     manufacture related mged elements.  Previous editions of PATCH failed
	     to process the final element after hitting EOF so I moved the read
	     statement into the for loop and made a check flag "done" to verify
	     that all elements are processed prior to falling out of the "for".      */

	mat_idn(m);

	/* FASTGEN targets are always in inches */
	mk_id_units(stdout, title, "in");

 /*
  *      This section loads the label file into an array
  *       needed to label processed solids, regions, and groups.
  *       The file should be limited to a 4 digit component number and two
  *       group labels (<15 characters) separated by "white space".
  */
	done = 1;
	if( labelfile != NULL ){

		while (done != 0){

			if( (stop=fscanf( gfp, "%4d", &num )) == 1 ){
				fscanf( gfp, "%s %s", nm[num].ug, nm[num].lg );
				while( (hold=fgetc( gfp )) != '\n' )
					;
			}
			else {
				if( stop == EOF ){
					done = 0;
				}
				else {
					while( (hold=fgetc( gfp )) != '\n' )
						;
				}
			}
		}
		done = 1;
	}

	/* Read the material codes file, which is a component code list
	   with equivalent los % and material code at the end of the line.
	   Non-conforming and blank lines should already have been stripped, 
	   since minimal error checking is done.
	   Line format is "%6d%66c%3d%5d".
	   Example:
		8215  COMPONENT WIDGET                                 95    5
	 */
	if( mfp ) {
		int eqlos, matcode;

		while( fgets(s,132+2,mfp) != NULL ) {

			if( sscanf(s,"%6d%*66c%3d%5d",
			    &i,&eqlos,&matcode) != 3 ) {

				rt_log( "Incomplete line in materials file for component '%.4d'\n",i);
				exit(1);
			}
			nm[i].matcode = matcode;
			nm[i].eqlos = eqlos;
		}
	}

	for( i = done = 0; !done ; i++ )
	{
		nread = read(fd,buf,sizeof(buf));     /* read one line of file into a buffer  */

		if(nread != 0){         /*  For valid reads, assign values to the input array  */

			sscanf(buf,"%lf %lf %lf %c %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			    &in[i].x,&in[i].y,&in[i].z,&in[i].surf_mode,&in[i].surf_type,
			    &in[i].surf_thick,&in[i].spacecode, &in[i].cc,
			    &in[i].ept[0],&in[i].ept[1],&in[i].ept[2],
			    &in[i].ept[3],&in[i].ept[4],&in[i].ept[5],
			    &in[i].ept[6],&in[i].ept[7],&in[i].mirror,&in[i].vc);

			/*  Perform english to metric conversions.  */
			in[i].x = mmtin*in[i].x;
			in[i].y = mmtin*in[i].y;
			in[i].z = mmtin*in[i].z;
			/* Normal thickness is in hundreths of an inch */
			if( in[i].surf_type <= 3 ){
				in[i].rsurf_thick = (mmtin/100) *
				    (abs(in[i].surf_thick)) +
				    (abs(in[i].surf_type))*mmtin;
			} else {
				in[i].rsurf_thick = (mmtin/100)*
				    (abs(in[i].surf_thick));
			}


			in[i].cc = abs(in[i].cc);
			in[i].surf_type = abs(in[i].surf_type);

			/*  Regurgitate data just loaded for debugging   */
			if (debug > 0){
				rt_log( "%lf %lf %lf %c %lf %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
				    in[i].x,in[i].y,in[i].z,in[i].surf_mode,in[i].surf_type,
				    in[i].surf_thick,in[i].spacecode, in[i].cc,
				    in[i].ept[0],in[i].ept[1],in[i].ept[2],
				    in[i].ept[3],in[i].ept[4],in[i].ept[5],
				    in[i].ept[6],in[i].ept[7],in[i].mirror,in[i].vc);
			}
			if (in[i].cc == 0) {
				done = 1;
				in[i].cc = -1;
			}
		}
		else{     	/*  Read hit EOF, set flag and process one last time.    */
			done = 1;
			in[i].cc = -1;
		}

		/* Process a component code number series when the structure
		 type changes or when a new component code number is found. */

		if( i == 0 )
			continue;
                if ( done || (in[i].cc != in[i-1].cc) ||
                    ((in[i].surf_type > 3 || in[i-1].surf_type > 3) &&
                    (in[i].surf_type != in[i-1].surf_type)) ) {

               		rt_log( "component code #%d\n" , in[i-1].cc );

			if( debug > 2 ) {
				for( j=0; j<i; j++ )
					rt_log("IN: %f %f %f\n",in[j].x,in[j].y,in[j].z);
			}

                    	if( rt_g.debug&DEBUG_MEM_FULL )
                    		rt_prmem( "At start of component" );

			switch(in[i-1].surf_type){    /* Key on surface types. */

			case 0:  	/* triangle approximation */
			case 1:  	/* triangle approximation (thickness + 1") */
			case 2:  	/* triangle approximation (thickness + 2") */
			case 3:  	/* triangle approximation (thickness + 3") */

				if ((nflg > 0)&&(in[i-1].surf_mode== '+'))
				{
					proc_triangle(i);
				}
				else if( (in[i-1].surf_mode == '-') && (in[i-1].surf_thick == 0) )
				{
					proc_triangle(i);
				}
				else
				{
					proc_plate(i);
				}
				break;

			case 4: 	/* new "donut/torus" */
				proc_donut(i);
				break;

			case 5:		/* wedge */

				proc_wedge(i);
				break;

			case 6:		/* sphere */

				proc_sphere(i);
				break;

			case 7:		/* box */

				proc_box(i);
				break;

			case 8:		/* cylinder */

				proc_cylin(i);
				break;

			case 9:		/* rod */

				proc_rod(i);
				break;

			default:
				rt_log( "component %.4d: unknown solid type %d\n",
				    in[i-1].cc,in[i-1].surf_type);
				break;

			}       /* end switch */

			/* If the component code number has changed, call
			   the subroutine for making groups from regions.   */

			if( (in[i].cc != in[i-1].cc) && (in[i].cc != 0) ) {
				proc_label( labelfile );
			}

			if( done ) {
				sprintf(name,"%dxxx_series",in[0].cc/1000);
				mk_lcomb(stdout,name,&headd,0,"","",rgb,0);
				(void) mk_addmember(name,&heade,WMOP_UNION);
			}

			/* We have already read the first record of the
			   next element, let's put it in the first position. */

			in[0] = in[i];
			i = 0;

                    	if( rt_g.debug&DEBUG_MEM_FULL )
                    		rt_prmem( "At end of component" );

		}       /* end "processing" if */
	}

	sprintf(name,"%s",top_level);
	mk_lcomb(stdout,name,&heade,0,"","",0,0);

	if( RT_LIST_NON_EMPTY( &headf.l )) {
		sprintf(name,"check.group");
		mk_lcomb(stdout,name,&headf,0,"","",0,0);
	}

	/* check for non-empty lists */
	if( RT_LIST_NON_EMPTY( &head.l ) )
	{
		struct wmember *wp;

		rt_log( "list head not empty: \n" );
		for( RT_LIST_FOR( wp , wmember , &head.l ) )
		{
			rt_log( "\t%c %s\n" , wp->wm_op , wp->wm_name );
		}
	}

	/* check for non-empty lists */
	if( RT_LIST_NON_EMPTY( &heada.l ) )
	{
		struct wmember *wp;

		rt_log( "list heada not empty: \n" );
		for( RT_LIST_FOR( wp , wmember , &heada.l ) )
		{
			rt_log( "\t%c %s\n" , wp->wm_op , wp->wm_name );
		}
	}

	/* check for non-empty lists */
	if( RT_LIST_NON_EMPTY( &headb.l ) )
	{
		struct wmember *wp;

		rt_log( "list headb not empty: \n" );
		for( RT_LIST_FOR( wp , wmember , &headb.l ) )
		{
			rt_log( "\t%c %s\n" , wp->wm_op , wp->wm_name );
		}
	}

	/* check for non-empty lists */
	if( RT_LIST_NON_EMPTY( &headc.l ) )
	{
		struct wmember *wp;

		rt_log( "list headc not empty: \n" );
		for( RT_LIST_FOR( wp , wmember , &headc.l ) )
		{
			rt_log( "\t%c %s\n" , wp->wm_op , wp->wm_name );
		}
	}

	/* check for non-empty lists */
	if( RT_LIST_NON_EMPTY( &headd.l ) )
	{
		struct wmember *wp;

		rt_log( "list headd not empty: \n" );
		for( RT_LIST_FOR( wp , wmember , &headd.l ) )
		{
			rt_log( "\t%c %s\n" , wp->wm_op , wp->wm_name );
		}
	}

	/* check for non-empty lists */
	if( RT_LIST_NON_EMPTY( &heade.l ) )
	{
		struct wmember *wp;

		rt_log( "list heade not empty: \n" );
		for( RT_LIST_FOR( wp , wmember , &heade.l ) )
		{
			rt_log( "\t%c %s\n" , wp->wm_op , wp->wm_name );
		}
	}

	/* check for non-empty lists */
	if( RT_LIST_NON_EMPTY( &headf.l ) )
	{
		struct wmember *wp;

		rt_log( "list headf not empty: \n" );
		for( RT_LIST_FOR( wp , wmember , &headf.l ) )
		{
			rt_log( "\t%c %s\n" , wp->wm_op , wp->wm_name );
		}
	}

}	/* END MAIN PROGRAM  */


/*  Throughout these procedures, the following naming conventions are used.
 *   For unique id# solids:	surface type code . component code number . "s"count
 *   For "neg" mirror solids:	side identifier & surface type code . component code number . "s"count
 *   For internal solids:	external name format, replacing "s" with "c" (for: cut out)
 *   For regions:		solid name with "s cnt" replaced by "r cnt"
 *   For groups:		text nomenclature identification from labelfile
 */

int
make_inside_trc( base, top, rbase, rtop, new_base, new_top, new_rbase, new_rtop, do_base, do_top, do_sides, thick )
point_t base, top, new_base, new_top;
fastf_t rbase, rtop;
fastf_t *new_rbase, *new_rtop;
int do_base, do_top, do_sides;
fastf_t thick;
{
	fastf_t delta_r;
	fastf_t sin_ang;
	vect_t h;
	fastf_t magh, inv_magh;
	fastf_t l;
	vect_t unit_h;
	vect_t new_h;

	VMOVE( new_base, base )
	VMOVE( new_top, top )
	*new_rtop = rtop;
	*new_rbase = rbase;

	if( (!do_base && !do_top && !do_sides) || thick == 0.0 )
		return( 0 );

	VSUB2( h, new_top, new_base )
	magh = MAGNITUDE( h );
	inv_magh = 1.0/magh;
	VSCALE( unit_h, h, inv_magh )

	VMOVE( new_h, h )
	l = hypot( (double)magh, (rbase - rtop ) );
	sin_ang = fabs( magh / l );
	delta_r = thick / sin_ang;

	if( do_base )
	{
		/* move base up by thickness */
		VJOIN1( new_base, new_base, thick, unit_h )

		/* adjust radius at new base */
		*new_rbase = *new_rbase + (thick/magh) * (*new_rtop - *new_rbase);

		/* recalculate height */
		VSUB2( new_h, new_top, new_base )
		magh = MAGNITUDE( new_h );
	}

	if( do_top )
	{
		/* move top down by thickness */
		VJOIN1( new_top, top, -thick, unit_h )

		/* adjust radius at new top */
		*new_rtop = *new_rtop + (thick/magh) * (*new_rbase - *new_rtop);

		/* recalculate height */
		VSUB2( new_h, new_top, new_base )
		magh = MAGNITUDE( new_h );
	}

	/* if height has reversed direction, we can't make an inside solid */
	if( VDOT( new_h, h ) <= 0.0 )
		return( 1 );

	if(  do_sides )
	{
		*new_rbase = (*new_rbase) - delta_r;
		*new_rtop = (*new_rtop) - delta_r;

		/* if radii are greater than zero, we're fine */
		if( *new_rtop > 0.0 && *new_rbase > 0.0 )
			return( 0 );

		/* if both radii are less then zero, we're toast */
		if( *new_rtop <= 0.0 && *new_rbase <= 0.0 )
			return( 1 );

		if( *new_rtop <= 0.0 )
		{
			/* adjust height (move top towards base) */
			magh = magh * (*new_rbase) / (*new_rbase - *new_rtop);
			VJOIN1( new_top, new_base, magh, unit_h )
			VSUB2( new_h, new_top , new_base )

			/* set new top radius to approximate zero */
			*new_rtop = 0.00001;
		}
		else if( *new_rbase <= 0.0 )
		{
			/* adjust base (move towards top) */
			magh = magh * (*new_rtop) / (*new_rtop - *new_rbase);
			VJOIN1( new_base, new_top, -magh, unit_h )
			VSUB2( new_h, new_top, new_base )

			/* set new bottom radius to approximate zero */
			*new_rbase = 0.00001;
		}
	}
	return( 0 );
}

/*
 *     This subroutine generates solid names with annotations for sidedness as
 *      required.
 */
char *
proc_sname(shflg,mrflg,cnt,ctflg)
char shflg,mrflg,ctflg;
int  cnt;
{
	char side;
	static char new_name[17];

	/* shflg == identifies shape process which called this function
	 * mrflg == indicates called by "as-modeled" pass or mirrored pass
	 * cnt   == suffix indentifier for the name
	 * ctflg == isolates internal cutting solids from regular solids
	 * new_name == solid name
	 * side  == left or right sidedness
	 */

	if (((mrflg == 'n') && (in[0].y >= 0)) ||
	    ((mrflg == 'y') && (in[0].y < 0))) {
	  side = 'l';
	}
	else {
	  side = 'r';
	}

	if (in[0].mirror >= 0) {
	  if ((mrflg == 'n') && (ctflg == 'n')) {
	    sprintf(new_name,"%c.%.4d.s%.2d",shflg,in[0].cc,cnt);
	  }
	  else if ((mrflg == 'n') && (ctflg == 'y')) {
	    sprintf(new_name,"%c.%.4d.c%.2d",shflg,in[0].cc,cnt);
	  }
	  else if ((mrflg == 'y') && (ctflg == 'n')) {
	    sprintf(new_name,"%c.%.4d.s%.2d",shflg,(in[0].cc+in[0].mirror),cnt);
	  }
	  else {
	    sprintf(new_name,"%c.%.4d.c%.2d",shflg,(in[0].cc+in[0].mirror),cnt);
	  }
	}
	else if (ctflg == 'n') {
	  sprintf(new_name,"%c%c.%.4d.s%.2d",side,shflg,in[0].cc,cnt);
	}
	else {
	  sprintf(new_name,"%c%c.%.4d.c%.2d",side,shflg,in[0].cc,cnt);
	}

	return( new_name );
}


/*
 *			N M G _ P A T C H _ C O P L A N A R _ F A C E _ M E R G E
 *
 *  A geometric routine to
 *  find all pairs of faces in a shell that have the same plane equation
 *  (to within the given tolerance), and combine them into a single face.
 *
 *  Note that this may result in some of the verticies being very slightly
 *  off the plane equation, but the geometry routines need to be prepared
 *  for this in any case.
 *  If the "simplify" flag is set, pairs of loops in the face that touch
 *  will be combined into a single loop where possible.
 *
 *  XXX Perhaps should be recast as "nmg_shell_shared_face_merge()", leaving
 *  XXX all the geometric calculations to the code in nmg_fuse.c ?
 */

static void
nmg_patch_coplanar_face_merge( s , face_count , p_faces , tol , simplify )
struct shell		*s;
int			*face_count;
struct patch_faces	*p_faces;
struct rt_tol		*tol;
int			simplify;
{
	struct model	*m;
	int		len;
	int		*flags1;
	int		*flags2;
	int		face1_no;
	int		face2_no;
	struct faceuse	*fu1;
	struct faceuse	*fu2;
	struct face	*f1;
	struct face	*f2;
	struct face_g_plane	*fg1;
	struct face_g_plane	*fg2;

	NMG_CK_SHELL(s);
	RT_CK_TOL( tol );

	m = nmg_find_model( &s->l.magic );
	len = sizeof(int) * m->maxindex;
	flags1 = (int *)rt_calloc( sizeof(int), m->maxindex,
		"nmg_shell_coplanar_face_merge flags1[]" );
	flags2 = (int *)rt_calloc( sizeof(int), m->maxindex,
		"nmg_shell_coplanar_face_merge flags2[]" );

	/* Visit each face in the shell */
	for( RT_LIST_FOR( fu1, faceuse, &s->fu_hd ) )  {
		plane_t		n1;

		if( RT_LIST_NEXT_IS_HEAD(fu1, &s->fu_hd) )  break;
		f1 = fu1->f_p;
		NMG_CK_FACE(f1);
		if( NMG_INDEX_TEST(flags1, f1) )  continue;
		NMG_INDEX_SET(flags1, f1);

		fg1 = f1->g.plane_p;
		NMG_CK_FACE_G_PLANE(fg1);
		NMG_GET_FU_PLANE( n1, fu1 );

		/* find the entry in p_faces for this faceuse */
		face1_no = (-1);
		while( p_faces[++face1_no].fu != fu1
			&& p_faces[face1_no].fu != fu1->fumate_p
			&& face1_no < *face_count );
		if( p_faces[face1_no].fu != fu1 &&
			p_faces[face1_no].fu != fu1 )
		{
			rt_log( "nmg_patch_coplanar_face_merge: Can't find entry for faceuse x%x in p_faces\n" , fu1 );
			continue;
		}

		/* For this face, visit all remaining faces in the shell. */
		/* Don't revisit any faces already considered. */
		bcopy( flags1, flags2, len );
		for( fu2 = RT_LIST_NEXT(faceuse, &fu1->l);
		     RT_LIST_NOT_HEAD(fu2, &s->fu_hd);
		     fu2 = RT_LIST_NEXT(faceuse,&fu2->l)
		)  {
			register fastf_t	dist;
			plane_t			n2;

			f2 = fu2->f_p;
			NMG_CK_FACE(f2);
			if( NMG_INDEX_TEST(flags2, f2) )  continue;
			NMG_INDEX_SET(flags2, f2);

			fg2 = f2->g.plane_p;
			NMG_CK_FACE_G_PLANE(fg2);

			/* See if face geometry is shared & same direction */
			if( fg1 != fg2 || f1->flip != f2->flip )  {
				/* If plane equations are different, done */
				NMG_GET_FU_PLANE( n2, fu2 );

				/* Compare distances from origin */
				dist = n1[3] - n2[3];
				if( !NEAR_ZERO(dist, tol->dist) )  continue;

				/*
				 *  Compare angle between normals.
				 *  Can't just use RT_VECT_ARE_PARALLEL here,
				 *  because they must point in the same direction.
				 */
				dist = VDOT( n1, n2 );
				if( !(dist >= tol->para) )  continue;
			}

			/* Find the entry for fu2 in p_faces */
			face2_no = (-1);
			while( p_faces[++face2_no].fu != fu2
				&& p_faces[face2_no].fu != fu2->fumate_p
				&& face2_no < *face_count );
			if( p_faces[face2_no].fu != fu2 &&
				p_faces[face2_no].fu != fu2 )
			{
				rt_log( "nmg_patch_coplanar_face_merge: Couldn`t find entry for faceuse x%x in p_faces\n" , fu2 );
				continue;
			}

			/* If the two faces don't get extruded the same distance, can't merge them */
			if( p_faces[face1_no].thick != p_faces[face2_no].thick )
				continue;

			/*
			 * Plane equations are the same, within tolerance,
			 * or by shared fg topology.
			 * Move everything into fu1, and
			 * kill now empty faceuse, fumate, and face
			 */
			{
				struct faceuse	*prev_fu;
				int		face_no;

				prev_fu = RT_LIST_PREV(faceuse, &fu2->l);
				/* The prev_fu can never be the head */
				if( RT_LIST_IS_HEAD(prev_fu, &s->fu_hd) )
					rt_bomb("prev is head?\n");

				nmg_jf( fu1, fu2 );

				fu2 = prev_fu;

				/* fix p_faces array */
				(*face_count)--;
				for( face_no=face2_no ; face_no<(*face_count) ; face_no++ )
				{
					p_faces[face_no].fu = p_faces[face_no+1].fu;
					p_faces[face_no].thick = p_faces[face_no+1].thick;
				}
			}

			/* There is now the option of simplifying the face,
			 * by removing unnecessary edges.
			 */
			if( simplify )  {
				struct loopuse *lu;

				for (RT_LIST_FOR(lu, loopuse, &fu1->lu_hd))
					nmg_simplify_loop(lu);
			}
		}
	}
	rt_free( (char *)flags1, "nmg_shell_coplanar_face_merge flags1[]" );
	rt_free( (char *)flags2, "nmg_shell_coplanar_face_merge flags2[]" );

	if (rt_g.NMG_debug & DEBUG_BASIC)  {
		rt_log("nmg_shell_coplanar_face_merge(s=x%x, tol=x%x, simplify=%d)\n",
			s, tol, simplify);
	}
}

int
Build_solid( l, name, mirror_name, plate_mode, centroid, thickness, pl1, tol )
int l;
char *name,*mirror_name;
int plate_mode;
point_t centroid;
fastf_t thickness;
plane_t pl1;
struct rt_tol *tol;
{
	struct model *m;
	struct nmgregion *r;
	struct shell *s,*is,*os;
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct nmg_ptbl faces;
	struct nmg_ptbl verts_to_move;
	struct patch_faces *p_faces;
	struct patch_verts *verts;
	struct rt_tol tol_tmp;
	vect_t out;
	int face_count;
	int missed_faces;
	int i,k;
	int vert1,vert2;
	fastf_t outdot;
	fastf_t min_dot=MAX_FASTF;
	vect_t norm;
	int planar=0;
	long *flags;
	long **copy_tbl;

	/* A local tolerance structure for times when I don't want tolerancing */
	tol_tmp.magic = RT_TOL_MAGIC;
	tol_tmp.dist = SQRT_SMALL_FASTF;
	tol_tmp.dist_sq = SMALL_FASTF;
	tol_tmp.perp = 0.0;
	tol_tmp.para = 1.0;

	if( debug )
		rt_log( "%s\n" , name );

	/* Make an array of patch_verts to hold the vertices and coordinates */
	verts = (struct patch_verts *)rt_calloc( l , sizeof( struct patch_verts ) , "patch-g: verts array" );
	for( k=1 ; k<l ; k++ )
	{
		verts[k].vp = (struct vertex *)NULL;
		VSET( verts[k].coord , x[k] , y[k] , z[k] );
	}


	/* make a model, region and shell to hold the component */
	m = nmg_mm();
	r = nmg_mrsv( m );
	s = RT_LIST_FIRST( shell , &r->s_hd );
	os = s;

	/* initialize the list of faces, for later use by nmg_gluefaces */
	nmg_tbl( &faces , TBL_INIT , NULL );

	/* make an array of patch_faces to hold faceuses and the desired thickness of each face */
	p_faces = (struct patch_faces *)rt_calloc( 2*l , sizeof( struct patch_faces ) , "build_solid: patch_faces" );

	/* loop through all the vertices, making faces as we go */
	face_count = 0;
	vert1 = 1;
	for (k=1 ; k<l-3 ; k++)
	{
		if( plate_mode && thk[k+2] != thickness )
			continue;

		if( !rt_3pts_distinct ( verts[k].coord , verts[k+1].coord , verts[k+2].coord , tol ) )
		{

			;	/* do nothing */
			/* rt_log( "Repeated Vertice, no face made\n"); */
		}
		else if( rt_3pts_collinear( verts[k].coord , verts[k+1].coord , verts[k+2].coord , tol ) )
		{

			;	/* do nothing */
			/* rt_log( "%s: collinear points, face not made.\n", name); */

		}
		else
		{
			struct vertex **vert_p[3];
			int found_verts;

			/* Check if this face was already made */
			for( i=0 ; i<NMG_TBL_END( &faces ) ; i++ )
			{
				int j;

				found_verts = 0;
				fu = (struct faceuse *)NMG_TBL_GET( &faces , i );
				NMG_CK_FACEUSE( fu );
				lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
				NMG_CK_LOOPUSE( lu );
				for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				{
					NMG_CK_EDGEUSE( eu );
					for( j=0 ; j<3 ; j++ )
						if( rt_pt3_pt3_equal( eu->vu_p->v_p->vg_p->coord,
								      verts[k+j].coord, tol ) )
							found_verts++;
				}
				if( found_verts == 3 ) /* this face already exists */
					break;
			}

			if( found_verts == 3 )
			{
				point_t tmp_pt;

				rt_log( "Component #%d:\n\tduplicate faces:\n" , in[0].cc );
				VSCALE( tmp_pt, verts[k].coord, conv_mm2in );
				rt_log( "\t\t (%g , %g , %g)", V3ARGS( tmp_pt ) );
				VSCALE( tmp_pt, verts[k+1].coord, conv_mm2in );
				rt_log( " (%g , %g , %g)", V3ARGS( tmp_pt ) );
				VSCALE( tmp_pt, verts[k+2].coord, conv_mm2in );
				rt_log( " (%g , %g , %g)\n", V3ARGS( tmp_pt ) );

				continue;
			}

			if( debug > 2 )
			{
				point_t tmp_pt;

				rt_log( "Make face: " );
				VSCALE( tmp_pt, verts[k].coord, conv_mm2in );
				rt_log( "\t\t (%g , %g , %g)", V3ARGS( tmp_pt ) );
				VSCALE( tmp_pt, verts[k+1].coord, conv_mm2in );
				rt_log( " (%g , %g , %g)", V3ARGS( tmp_pt ) );
				VSCALE( tmp_pt, verts[k+2].coord, conv_mm2in );
				rt_log( " (%g , %g , %g)\n", V3ARGS( tmp_pt ) );
			}

			/* Assign the three vertices for this face */
			for( i=0 ; i<3 ; i++ )
				vert_p[i] = &verts[k+i].vp;

			/* Insure that same coordinates correspond to one vertex pointer */
			for( ; vert1 < k+3 ; vert1++ )
			{
				if( verts[vert1].vp )
					continue;
				for( vert2=1 ; vert2 < vert1 ; vert2++ )
				{
					if( !verts[vert2].vp )
						continue;
					if( VEQUAL( verts[vert1].coord , verts[vert2].coord ) )
						verts[vert1].vp = verts[vert2].vp;
				}
			}

			/* make a face */
			fu = nmg_cmface( os , vert_p , 3 );
			NMG_CK_FACEUSE( fu );

			if( debug > 2 )
				rt_log( "\tMade faceuse x%x\n" , fu );

			/* add it to the list */
			nmg_tbl( &faces , TBL_INS , (long *)fu );

			/* remember this face and its thickness */
			p_faces[face_count].fu = fu;
			p_faces[face_count].thick = thk[k+2];

			/* Make sure we don't overrun our p_faces array */
			face_count++;
			if( face_count >= 2*l )
			{
				rt_log( "Face count = %d, only allowed for %d\n" , face_count , 2*l );
				rt_bomb( "Build_solid\n" );
			}

			/* Assign geometry */
			for( i=0 ; i<3 ; i++ )
			{
				if( verts[k+i].vp != NULL && verts[k+i].vp->vg_p == NULL )
					nmg_vertex_gv( verts[k+i].vp , verts[k+i].coord );
			}

			/* phantom armor */
			if (aflg > 0) {
				if (in[0].rsurf_thick == 0) {
					in[0].rsurf_thick = 1;
				}
			}
		}
	}

	/* calculate plane equations for all the faces */
        for (RT_LIST_FOR(s, shell, &r->s_hd))
        {
            NMG_CK_SHELL( s );
            for (RT_LIST_FOR(fu, faceuse, &s->fu_hd))
            {
                NMG_CK_FACEUSE( fu );
                if( fu->orientation == OT_SAME )
                {
                        if( nmg_fu_planeeqn( fu , tol ) )
                	{
                                rt_log( "Build_solid: Failed to calculate plane eqn for outside fu x%x:\n" , fu );
                		nmg_pr_fu_briefly( fu , (char *)NULL );
                	}
                }
            }
        }

	/* don't need the verts array any more */
	rt_free( (char *)verts , "build_solid: verts" );

	/* fuse vertices */
	(void)nmg_model_vertex_fuse( m, tol );

	/* FASTGEN targets may have vertices that should be part of
	 * an adjoining edge. Use nmg_break_long_edges to fix this
	 */
	i = nmg_break_edges( &m->magic , tol );
	if( debug > 2 )
		rt_log( "nmg_break_edges broke %d edges\n" , i );

	/* glue all the faces together */
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ), tol );

	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
		nmg_make_faces_within_tol( s, tol );

	if( !plate_mode )
	{
		/* make sure the normals are correct */
		for( RT_LIST_FOR( s , shell , &r->s_hd ) )
			nmg_fix_normals( s , tol );

		/* make sure we are dealing with closed shells */
		for( RT_LIST_FOR( s , shell , &r->s_hd ) )
			nmg_close_shell( s, tol );

		/* free the memory for the face list */
		nmg_tbl( &faces , TBL_FREE , NULL );

		/* don't need the p_faces array any more */
		rt_free( (char *)p_faces , "build_solid: p_faces" );

		/* Calculate bounding boxes */
		nmg_region_a( r , tol );

		/* Write solid to BRLCAD DB */
		s = RT_LIST_FIRST( shell , &r->s_hd );

		if( polysolid )
			write_shell_as_polysolid( stdout , name , s );
		else
		{
			nmg_shell_coplanar_face_merge( s , tol , 0 );
			if( !nmg_simplify_shell( s ) )
				mk_nmg( stdout , name , m );
		}

		/* if this solid is mirrored, don't go through the entire process again */
		if( mirror_name[0] )
		{
			nmg_mirror_model( m );
		
			if( polysolid )
				write_shell_as_polysolid( stdout , mirror_name , s );
			else
				mk_nmg( stdout , mirror_name , m );
		}

		/* Kill the model */
		nmg_km( m );

		return( 0 );
	}

	/* Next section is to fix the normals of the shell
	 * Can't use nmg_fix_normals, because shell may not be closed */

	/* get the shell and the first face from our list */
	s = RT_LIST_FIRST( shell , &r->s_hd );
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		struct faceuse *fu1;

		NMG_CK_FACEUSE( fu );
		if( fu->orientation != OT_SAME )
			continue;

		NMG_GET_FU_NORMAL( norm , fu );
		for( RT_LIST_FOR( fu1 , faceuse , &s->fu_hd ) )
		{
			vect_t norm1;
			fastf_t dot;

			if( fu1->orientation != OT_SAME )
				continue;

			if( fu == fu1 )
				continue;

			NMG_GET_FU_NORMAL( norm1 , fu1 );
			dot = VDOT( norm, norm1 );
			if( dot < 0.0 )
				dot = (-dot );
			if( dot < min_dot )
				min_dot = dot;
		}
	}

	if( min_dot > 0.8 )
		planar = 1;

	fu = (struct faceuse *)NMG_TBL_GET( &faces , 0 );
	NMG_CK_FACEUSE( fu );

	/* free the memory for the face list */
	nmg_tbl( &faces , TBL_RST , NULL );

	/* Create a flags array for the model to make sure each face gets its orientation set */
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "patch-g: flags" );

	/* loop to catch all faces */
	missed_faces = 1;
	while( missed_faces )
	{
		struct faceuse *fu1;
		vect_t normal;

		/* get the normal direction for the first face */
		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;
		if( fu->orientation != OT_SAME )
			rt_bomb( "Neither faceuse nor mate have an OT_SAME side\n" );
		NMG_GET_FU_NORMAL( normal , fu );

		if( !planar )
		{
			/* calculate "out" direction, from centroid to face */
			lu = RT_LIST_FIRST( loopuse , &fu->lu_hd );
			eu = RT_LIST_FIRST( edgeuse , &lu->down_hd );
			VSUB2( out , eu->vu_p->v_p->vg_p->coord , centroid );
			VUNITIZE( out );

			/* if "normal" and "out" disagree, reverse normal */
			outdot = VDOT( out , normal );
			if( outdot <= 0.001 &&  outdot >= -0.001 )
			{
				/* try model centroid */
				VSUB2( out , eu->vu_p->v_p->vg_p->coord , Centroid );
				VUNITIZE( out );
				outdot = VDOT( out , normal );
			}
		}
		else
		{
			outdot = VDOT( pl1, normal );
		}

		if( outdot < 0.0 )
			nmg_reverse_face_and_radials( fu , tol );

		if( fu->orientation != OT_SAME )
			fu = fu->fumate_p;

		/* propagate this normal direction throughout the shell */
		nmg_propagate_normals( fu , flags , tol );

		/* check if all the faces have been processed */
		missed_faces = 0;
		for( RT_LIST_FOR( fu1 , faceuse , &s->fu_hd ) )
		{
			NMG_CK_FACEUSE( fu1 );
			if( fu1->orientation == OT_SAME )
			{
				if( !NMG_INDEX_TEST( flags , fu1->f_p ) )
				{
					fu = fu1;
					missed_faces++;
					break;
				}
			}
		}
	}

	nmg_patch_coplanar_face_merge( s , &face_count , p_faces , tol , 0 );

	if( nmg_simplify_shell( s ) )
		return( 1 );

	/* Calculate bounding boxes */
	nmg_region_a( r , tol );

	if( debug > 4 )
	{
		char tmp_name[17];

		sprintf( tmp_name , "out.%s" , name );
		mk_nmg( stdout , tmp_name , m );
		fflush( stdout );
	}

	/* Duplicate shell */
	is = nmg_dup_shell( s , &copy_tbl, tol );

	/* make a new flags array */
	rt_free( (char *)flags , "build_solid: flags" );
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "patch-g: flags" );

	/* Move faces planes */
	for( i=0 ; i<face_count ; i++ )
	{
		struct face_g_plane *fg_p;
		plane_t plane;

		fu = NMG_INDEX_GETP( faceuse , copy_tbl , p_faces[i].fu );
		if( !fu )
		{
			rt_log( "No fu in duplicate shell corresponding to fu #%d (x%x) in original\n" , i , p_faces[i].fu );
			rt_bomb( "patch-g\n" );
		}

		NMG_CK_FACEUSE( fu );
		if( fu->orientation != OT_SAME )
		{
			fu = fu->fumate_p;
			NMG_CK_FACEUSE( fu );
		}
		if( fu->orientation != OT_SAME )
			rt_bomb( "patch-g: neither faceuse nor mate has orientation of OT_SAME\n" );

		fg_p = fu->f_p->g.plane_p;
		NMG_CK_FACE_G_PLANE( fg_p );

		/* move the faces by the distance "thick" */
		if( NMG_INDEX_TEST_AND_SET( flags , fg_p ) )
		{
			if( fu->f_p->flip )
				fg_p->N[3] += p_faces[i].thick;
			else
				fg_p->N[3] -= p_faces[i].thick;
		}
	}

	/* don't need the p_faces array any more */
	rt_free( (char *)p_faces , "build_solid: p_faces" );

	/* Reverse the normals of all the new faces */
	for( RT_LIST_FOR( fu , faceuse , &is->fu_hd ) )
	{
		if( NMG_INDEX_TEST_AND_SET( flags , fu->f_p ) )
			nmg_reverse_face( fu );
	}

	/* glue all the faces of the new shell together */
	for( RT_LIST_FOR( fu , faceuse , &is->fu_hd ) )
	{
		if( fu->orientation == OT_SAME )
			nmg_tbl( &faces , TBL_INS , (long *)fu );
	}
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ), tol );
	nmg_tbl( &faces , TBL_RST , NULL );

	nmg_shell_coplanar_face_merge( is , tol , 0 );
	nmg_shell_a( is , tol );

	/* make yet another version of the flags array */
	rt_free( (char *)flags , "build_solid: flags" );		
	flags = (long *)rt_calloc( m->maxindex , sizeof( long ) , "patch-g: flags" );

	/* make a list of the vertices to be moved */
	nmg_tbl( &verts_to_move , TBL_INIT , (long *)NULL );
	for( RT_LIST_FOR( fu , faceuse , &is->fu_hd ) )
	{
		if( fu->orientation != OT_SAME )
			continue;

		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC( &lu->down_hd ) == NMG_VERTEXUSE_MAGIC )
			{
				/* the vertex in a loop of one vertex
				 * must show up in an edgeuse somewhere,
				 * so don't mess with it here */
				continue;
			}

			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				struct vertexuse *vu;

				NMG_CK_EDGEUSE( eu );
				vu = eu->vu_p;
				NMG_CK_VERTEXUSE( vu );
				NMG_CK_VERTEX( vu->v_p )
				if( NMG_INDEX_TEST_AND_SET( flags , vu->v_p ) )
				{
					/* move this vertex */
					nmg_tbl( &verts_to_move , TBL_INS , (long *)vu->v_p );
				}
			}
		}
	}

	/* now start the actual moving of the vertex coordinates */
	for( i=0 ; i<NMG_TBL_END( &verts_to_move ) ; i++ )
	{
		struct vertex *new_v;

		/* get the vertexuse from the table */
		new_v = (struct vertex *)NMG_TBL_GET( &verts_to_move , i );
		NMG_CK_VERTEX( new_v );

		/* Adjust the vertices of new_v */
		if( debug > 2 )
			rt_log( "Moving ( %f %f %f )" , V3ARGS( new_v->vg_p->coord ) );
		if( nmg_in_vert( new_v , 1 , tol ) )
		{
			/* FAILURE, kill the model and shell and return a failure notification */
			rt_log( "nmg_in_vert failed on %s!\n" , name );
			if( !nmg_ks( is ) )
			{
				/* debugging: write an NMG of the outer shell named "name.BAD" */
				char *bad;

				bad = (char *)rt_malloc( strlen( name ) + 5 , "build_solid: Name for Bad solid" );
				strcpy( bad , name );
				strcat( bad , ".BAD" );
				mk_nmg( stdout , bad , m );
				rt_log( "BAD shell written as %s\n" , bad );
				rt_free( (char *)bad , "build_solid: Name for Bad solid" );
			}
			nmg_km( m );
			rt_free( (char *)flags , "build_solid: flags" );
			rt_free( (char *)copy_tbl , "build_solid: copy_tbl" );
			nmg_tbl( &verts_to_move , TBL_FREE , (long *)NULL );
			return( 1 );
		}
		if( debug > 2 )
			rt_log( " to ( %f %f %f )\n" , V3ARGS( new_v->vg_p->coord ) );
	}

	/* done moving, get rid of table */
	nmg_tbl( &verts_to_move , TBL_FREE , (long *)NULL );

	if( debug > 4 )
	{
		char tmp_name[32];

		sprintf( tmp_name , "open.%s" , name );
		mk_nmg( stdout , tmp_name , m );
		fflush( stdout );
	}

	nmg_make_faces_within_tol( is, tol );

	/* Close shell */
	if( debug )
		rt_log( "Close shell\n" );
	if( nmg_open_shells_connect( s , is , copy_tbl , tol ) )
	{
		/* debugging: write an NMG of the outer shell named "name.BAD" */
		char *bad;

		bad = (char *)rt_malloc( strlen( name ) + 5 , "build_solid: Name for Bad solid" );
		strcpy( bad , name );
		strcat( bad , ".BAD" );
		mk_nmg( stdout , bad , m );
		rt_log( "BAD shell written as %s\n" , bad );
		rt_free( (char *)bad , "build_solid: Name for Bad solid" );
		nmg_km( m );
		rt_free( (char *)flags , "Build_solid: flags" );
		rt_free( (char *)copy_tbl , "Build_solid: copy_tbl" );
		return( 1 );
	}

	rt_free( (char *)flags , "Build_solid: flags" );
	rt_free( (char *)copy_tbl , "Build_solid: copy_tbl" );

#if 0
	if( debug )
		rt_log( "Recalculate plane equations\n" );
	/* recalculate plane equations, since some of the vertices we calculated
	 * may not be exactly on the plane */
	for( RT_LIST_FOR( s , shell , &r->s_hd ) )
	{
		for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
		{
			if( fu->orientation == OT_SAME )
			{
plane_t pl1,pl2;

NMG_GET_FU_PLANE( pl1 , fu );
	                        if( nmg_fu_planeeqn( fu , tol ) )
	                	{
NMG_GET_FU_PLANE( pl2 , fu );
	                                rt_log( "Build_solid: Failed to re-calculate plane eqn for fu x%x:\n" , fu );
	                		nmg_pr_fu_briefly( fu , (char *)NULL );
	                		rt_g.NMG_debug |= DEBUG_MESH;
	                		nmg_ck_fu_verts( fu , fu->f_p , tol );
rt_log( "Old plane was ( %f %f %f %f )\n" , V4ARGS( pl1 ) );
rt_log( "New plane is ( %f %f %f %f )\n" , V4ARGS( pl2 ) );
nmg_face_g( fu , pl1 );
	                		nmg_ck_fu_verts( fu , fu->f_p , tol );
	                		rt_g.NMG_debug &= ~DEBUG_MESH;
	                	}
			}
		}
	}
#endif

	nmg_tbl( &faces , TBL_RST , NULL );

	/* glue all the faces of the new shell together */
	s = RT_LIST_FIRST( shell , &r->s_hd );
	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		if( !fu )
			continue;
		NMG_CK_FACEUSE( fu );
		if( fu->orientation == OT_SAME )
			nmg_tbl( &faces , TBL_INS , (long *)fu );
	}
	if( debug )
		rt_log( "Re-glue faces\n" );
	nmg_gluefaces( (struct faceuse **)NMG_TBL_BASEADDR( &faces) , NMG_TBL_END( &faces ), tol );
	nmg_tbl( &faces , TBL_FREE , NULL );

	/* Calculate bounding boxes */
	if( debug )
		rt_log( "nmg_region_a( r = x%x )\n" , r );
	nmg_region_a( r , tol );

	if( debug )
		rt_log( "writing %s to BRLCAD DB\n" , name );

	/* Write solid to BRLCAD DB */
	s = RT_LIST_FIRST( shell , &r->s_hd );

	if( polysolid )
		write_shell_as_polysolid( stdout , name , s );
	else
	{
		nmg_shell_coplanar_face_merge( s , tol , 0 );
		if( !nmg_simplify_shell( s ) )
			mk_nmg( stdout , name , m );
	}

	/* if this solid is mirrored, don't go through the entire process again */
	if( mirror_name[0] )
	{
		if( debug )
			rt_log( "Mirror model\n" );
		nmg_mirror_model( m );

		if( debug )
			rt_log( "writing  %s (mirrored) to BRLCAD DB\n" , mirror_name );
		
		if( polysolid )
			write_shell_as_polysolid( stdout , mirror_name , s );
		else
			mk_nmg( stdout , mirror_name , m );
	}

	/* Kill the model */
	nmg_km( m );

	return( 0 );
}

/*
 *     This subroutine takes previously generated solid names and combines them
 *	into a common region identity.  Format of the make_region command call
 *	requires in order: output file name, input region name, link file of solids,
 *	region/group flag, material name, material parameters, RGB color assignment, region id #,
 *	aircode, material code, LOS, and inheritance flag.  The region is then 
 *	added to a hold file for combination into groups in another process.        
 */
void
proc_region(name1)
char *name1;

{
	char tmpname[24];
	int chkroot;
	int i;
	int cc;
	static int reg_count=0;
	static int mir_count=0;
	static int last_cc=0;


	if( RT_LIST_IS_EMPTY( &head.l ) )
		return;

	strcpy( tmpname , name1 );

	chkroot = 0;
	while( tmpname[chkroot++] != '.' );

	cc = atoi( &tmpname[chkroot] );

	i = strlen( tmpname );
	while( tmpname[--i] != '.' );
	tmpname[i] = '\0';

	if( in[0].cc != last_cc )
	{
		reg_count = 0;
		mir_count = 0;
	}

	if( cc != in[0].cc )
	{
		mir_count++;
		sprintf(cname,"%s.r%.2d",tmpname,mir_count);
	}
	else
	{
		reg_count++;
		sprintf(cname,"%s.r%.2d",tmpname,reg_count);
	}


	if( nm[cc].matcode != 0 ) {
		mk_lrcomb(stdout, cname, &head, 1, 0, 0, 0, cc, 0, nm[cc].matcode, nm[cc].eqlos, 0);
	}
	else {
		mk_lrcomb(stdout, cname, &head, 1, 0, 0, 0, cc, 0, 2, 100, 0);
	}

	if ( cc == in[0].cc){
		(void) mk_addmember(cname,&heada,WMOP_UNION);
	}
	else{
		(void) mk_addmember(cname,&headb,WMOP_UNION);
	}

	last_cc = in[0].cc;
}

/*
 *	 Process Volume Mode triangular facetted solids  
 */
void
proc_triangle(cnt)
int cnt;
{
	vect_t	ab,bc,ca;
	int	k,l;
	int	index;
	int	cpts;
	vect_t  norm,out;
	char	shflg,mrflg,ctflg;
	static int count=0;
	static int mir_count=0;
	static int last_cc=0;
	char 	name[17],mirror_name[17];
	plane_t pl;
	point_t last;

	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count=0;
	}

	for(k=0 ; k < (cnt) ; k++){
		for(l=0; l<= 7; l++){

			if(in[k].ept[l] > 0){

				index = in[k].ept[l];
				list[index].x = in[k].x;
				list[index].y = in[k].y;
				list[index].z = in[k].z;
				list[index].flag = 1;

				if (debug > 3)
					rt_log( "%d %f %f %f\n",list[index].flag,in[k].x,in[k].y,in[k].z);
			}

		}
	}

	/* Everything is sequenced, but separated, compress into single array here */
	l = 1;

	for (k=1; k<10000; k++){
		if(list[k].flag == 1){
			list[k].flag = 0;
			x[l] = list[k].x;
			y[l] = list[k].y;
			z[l] = list[k].z;

			l= l+1;
		}

		if (debug > 3)
			rt_log( "k=%d l=%d %f %f %f flag=%d\n",k,l,list[k].x,list[k].y,list[k].z,list[k].flag);
	}

	if (debug > 2){
		for (k=1;(k<=l);k++)
			rt_log( "%d %f %f %f\n",k,x[k],y[k],z[k]);
	}

	VSET( last, MAX_FASTF, MAX_FASTF, MAX_FASTF );
	VSET( centroid , 0.0 , 0.0 , 0.0 );
	for ( cpts=0, k=1; k<l; k++ ) {
		point_t tmp;

		VSET( tmp, x[k], y[k], z[k] );
		if( VEQUAL( tmp, last ) )
			continue;
		VADD2( centroid, centroid, tmp );
		VMOVE( last, tmp );
		cpts++;
	}
	VSCALE( centroid, centroid, 1.0/cpts );
	if( debug > 2 ) {
		rt_log( "%d: cpts=%d centroid %f %f %f\n",
		    in[0].cc, cpts, 
		    centroid[0], centroid[1], centroid[2] );
	}

	/* assign solid's name */

	shflg = 'f';
	mrflg = 'n';
	ctflg = 'n';
	strcpy( name , proc_sname (shflg,mrflg,count+1,ctflg) );
	count++;

	if(in[0].mirror != 0)
	{

		mrflg = 'y';
		strcpy( mirror_name , proc_sname (shflg,mrflg,mir_count+1,ctflg) );
		mir_count++;
	}
	else
		mirror_name[0] = '\0';

	if( (RT_SETJUMP) || Build_solid( l, name, mirror_name, 0, centroid, 0.0, pl, &tol ) )
	{
		if( mirror_name[0] )
			rt_log( "Failed to build solids %s and %s\n", name, mirror_name );
		else
			rt_log( "Failed to build solid %s\n", name );
	}
	else
	{
		count++;
		(void) mk_addmember(name,&head,WMOP_UNION);
		proc_region( name );
		if( mirror_name[0] )
		{
			(void) mk_addmember(mirror_name,&head,WMOP_UNION);
			mir_count++;
			proc_region( mirror_name );
		}
	}
	RT_UNSETJUMP;

	if( debug )
	{
		rt_log( "\tFinished %s\n" , name );
		if( mirror_name[0] )
			rt_log( "\tand %s\n" , mirror_name );
	}

	last_cc = in[cnt-1].cc;
}

void
Get_ave_plane( pl, num_pts, x, y, z )
plane_t pl;
int num_pts;
fastf_t *x,*y,*z;
{
	mat_t matrix;
	mat_t inverse;
	double one_over_vertex_count;
	vect_t vsum;
	int i;
	fastf_t det;
	int failed=0;

	/* build matrix */
	mat_zero( matrix );
	VSET( vsum , 0.0 , 0.0 , 0.0 );

	one_over_vertex_count = 1.0/(double)(num_pts);

	for( i=0 ; i<num_pts ; i++ )
	{

		matrix[0] += x[i] * x[i];
		matrix[1] += x[i] * y[i];
		matrix[2] += x[i] * z[i];
		matrix[5] += y[i] * y[i];
		matrix[6] += y[i] * z[i];
		matrix[10] += z[i] * z[i];

		vsum[X] += x[i];
		vsum[Y] += y[i];
		vsum[Z] += z[i];
	}
	matrix[4] = matrix[1];
	matrix[8] = matrix[2];
	matrix[9] = matrix[6];
	matrix[15] = 1.0;


	/* Check that we don't have a singular matrix */
	det = mat_determinant( matrix );

	if( !NEAR_ZERO( det , SMALL_FASTF ) )
	{
		fastf_t inv_len_pl;

		/* invert matrix */
		mat_inv( inverse , matrix );

		/* get normal vector */
		MAT4X3PNT( pl , inverse , vsum );

		/* unitize direction vector */
		inv_len_pl = 1.0/(MAGNITUDE( pl ));
		HSCALE( pl , pl , inv_len_pl );

		/* get average vertex coordinates */
		VSCALE( vsum, vsum, one_over_vertex_count );

		/* get distance from plane to orgin */
		pl[H] = VDOT( pl , vsum );

		if( ABS( pl[0] ) > ABS( pl[1] ) )
		{
			if( ABS( pl[0] ) > ABS( pl[2] ) )
			{
				if( pl[0] < 0.0 )
					HREVERSE( pl, pl )
			}
			else
			{
				if( pl[2] < 0.0 )
					HREVERSE( pl, pl )
			}
		}
		else
		{
			if( ABS( pl[1] ) > ABS( pl[2] ) )
			{
				if( pl[1] < 0.0 )
					HREVERSE( pl, pl )
			}
			else
			{
				if( pl[2] < 0.0 )
					HREVERSE( pl, pl )
			}
		}
	}
	else
	{
		fastf_t x0,y0,z0;
		int x_same=1;
		int y_same=1;
		int z_same=1;

		/* singular matrix, may occur if all vertices have the same zero
		 * component.
		 */
		x0 = x[0];
		y0 = y[0];
		z0 = z[0];
		for( i=1 ; i<num_pts ; i++ )
		{
			if( x[i] != x0 )
				x_same = 0;
			if( y[i] != y0 )
				y_same = 0;
			if( z[i] != z0 )
				z_same = 0;

			if( !x_same && !y_same && !z_same )
				break;
		}

		if( x_same )
		{
			VSET( pl , 1.0 , 0.0 , 0.0 );
		}
		else if( y_same )
		{
			VSET( pl , 0.0 , 1.0 , 0.0 );
		}
		else if( z_same )
		{
			VSET( pl , 0.0 , 0.0 , 1.0 );
		}

		if( x_same || y_same || z_same )
		{
			/* get average vertex coordinates */
			VSCALE( vsum, vsum, one_over_vertex_count );

			/* get distance from plane to orgin */
			pl[H] = VDOT( pl , vsum );

		}
		else
		{
			rt_log( "Get_plane: Cannot calculate plane\n" );
			failed = 1;
		}
	}
}

/*
 *	 Process Plate Mode triangular surfaces 
 */
void
proc_plate(cnt)
int cnt;
{
	point_t arb6pt[8];
	vect_t	out;
	vect_t	norm;
	vect_t	ab,bc,ca,ac;
	int	thick_no;
	fastf_t ndot,outdot;
	int k,l;
	int index;
	int thick_warn=0;
	static int count=0;
	static int mir_count=0;
	static int last_cc=0;
	int cpts;
	char	shflg,mrflg,ctflg;
	char	name[17],mirror_name[17];
	plane_t pl;
	point_t last;

	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count = 0;
	}

	/* include the check for phantom armor */
	if ((in[0].rsurf_thick > 0)||(aflg > 0))
	{

		for(k=0 ; k < (cnt) ; k++){
			for(l=0; l<= 7; l++){
				if(in[k].ept[l] > 0){
					index = in[k].ept[l];

					list[index].x = in[k].x;
					list[index].y = in[k].y;
					list[index].z = in[k].z;
					if( in[k].rsurf_thick < tol.dist )
					{
						rt_log( "Warning: thickness of component #%d at sequence #%d is %g inches\n" , in[0].cc , index , in[k].rsurf_thick*conv_mm2in );
						rt_log( "\tsetting thickness to %g inches\n", 3.0*tol.dist*conv_mm2in );
						list[index].thick = 3.0*tol.dist;
					}
					else
						list[index].thick = in[k].rsurf_thick;
					list[index].flag = 1;
				}
			}
		}

		/* make list of thicknesses */
		nthicks = 0;
		for (k=1; k<10000; k++)
		{
			int found_thick;

			if(list[k].flag == 1)
			{
				if( !nthicks )
				{
					thicks[0] = list[k].thick;
					nthicks = 1;
					continue;
				}

				found_thick = 0;
				for( thick_no=0 ; thick_no < nthicks ; thick_no++ )
				{
					if( NEAR_ZERO( list[k].thick - thicks[thick_no], tol.dist ) )
					{
						list[k].thick = thicks[thick_no];
						found_thick = 1;
						break;
					}
				}
				if( !found_thick )
				{
					if( nthicks >= MAX_THICKNESSES )
					{
						rt_log( "Component # has too many different thicknesses\n",
							in[0].cc );
						rt_log( "\t skipping component\n" );
						return;
					}
					else
					{
						thicks[nthicks] = list[k].thick;
						nthicks++;
					}
				}
			}
		}

		/* Everything is sequenced, but separated, compress into single array here */
		l = 1;

		for (k=1; k<10000; k++){
			if(list[k].flag == 1){
				list[k].flag = 0;
				x[l] = list[k].x;
				y[l] = list[k].y;
				z[l] = list[k].z;
				thk[l] = list[k].thick;
				if( thk[l] < tol.dist )
				{
					rt_log( "Proc_plate: Found a bad thickness, should have been fixed by now!!!\n" );
					rt_bomb( "Proc_plate: thickness less tahn tolerance.\n" );
				}
				l= l+1;
			}
		}


		if( debug > 2 ) {
			for ( k=1;k<l; k++ )
				rt_log( "Compressed: %f %f %f\n",x[k],y[k],z[k]);
			rt_log( "%d unique plate thicknesses:\n", nthicks );
			for( thick_no=0 ; thick_no < nthicks ; thick_no++ )
				rt_log( "\t%g inches\n" , thicks[thick_no]*conv_mm2in );
		}

		Get_ave_plane( pl, l, x,y,z );

		VSET( last, MAX_FASTF, MAX_FASTF, MAX_FASTF );
		VSET( centroid , 0.0 , 0.0 , 0.0 );
		for ( cpts=0, k=1; k<l; k++ ) {
			point_t tmp;

			VSET( tmp, x[k], y[k], z[k] );
			if( VEQUAL( tmp, last ) )
				continue;
			VADD2( centroid, centroid, tmp );
			VMOVE( last, tmp );
			cpts++;
		}
		VSCALE( centroid, centroid, 1.0/cpts );
		if( debug > 2 ) {
			rt_log( "%d: cpts=%d centroid %f %f %f\n",
			    in[0].cc, cpts, 
			    centroid[0], centroid[1], centroid[2] );
		}

		for( thick_no=0 ; thick_no < nthicks ; thick_no++ )
		{

			/* name solids */

			shflg = 't';
			mrflg = 'n';
			ctflg = 'n';
			strcpy( name , proc_sname (shflg,mrflg,count+1,ctflg) );

			if( in[0].mirror != 0 )
			{
				mrflg = 'y';
				strcpy( mirror_name , proc_sname (shflg,mrflg,mir_count+1,ctflg) );
			}
			else
				mirror_name[0] = '\0';

			if( (RT_SETJUMP) || Build_solid( l, name, mirror_name, 1, centroid, thicks[thick_no], pl, &tol ) )
			{
				if( mirror_name[0] )
					rt_log( "Failed to build solids %s and %s\n", name, mirror_name );
				else
					rt_log( "Failed to build solid %s\n", name );
			}
			else
			{
				count++;
				(void) mk_addmember(name,&head,WMOP_UNION);
				proc_region( name );
				if( mirror_name[0] )
				{
					(void) mk_addmember(mirror_name,&head,WMOP_UNION);
					mir_count++;
					proc_region( mirror_name );
				}
			}
			RT_UNSETJUMP;

			if( debug )
			{
				rt_log( "\tFinished %s\n" , name );
					if( mirror_name[0] )
					rt_log( "\tand %s\n" , mirror_name );
			}
		}
		proc_region( name );
		if( mirror_name[0] )
			proc_region( mirror_name );
	} /* phantom armor check */

	last_cc = in[cnt-1].cc;
}

/* 
 *	Process fastgen wedge shape - also process hollow wedges.
 */
void
proc_wedge(cnt)
int cnt;
{
	point_t	pts[1];
	point_t	pt8[8];
	point_t inpt8[8];
	int i,k,l;
	vect_t	ab, ac , ad;
	fastf_t len;
	plane_t planes[5];
	static int count=0;
	static int mir_count=0;
	static int last_cc=0;
	char	shflg,mrflg,ctflg;
	char	name[17];
	int ret = 0;
	static struct rt_tol *tols = &tol;


	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count=0;
	}

	for(k=0 ; k <= (cnt-1) ; k+=4){
		VSET( pt8[0], in[k].x,in[k].y,in[k].z );
		VSET( pt8[1], in[k+1].x,in[k+1].y,in[k+1].z );
		VSET( pt8[3], in[k+2].x,in[k+2].y,in[k+2].z );
		VSET( pt8[4], in[k+3].x,in[k+3].y,in[k+3].z );

		VSUB2(ab,pt8[4],pt8[0]);
		VSUB2(ac,pt8[3],pt8[0]);
		VSUB2(ad,pt8[1],pt8[0]);

		VADD3(pt8[7],ab,ac,pt8[0]);

		VADD3(pt8[2],ac,ad,pt8[0]);

		VMOVE(pt8[6],pt8[7]);
		VMOVE(pt8[5],pt8[4]);

		/* name solids */

		shflg = 'w';
		mrflg = 'n';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,count+1,ctflg) );

		/* make solids */

		mk_arb8( stdout, name, &pt8[0][X] );
		count++;

		(void) mk_addmember(name,&head,WMOP_UNION);

		if( in[k].surf_mode == '-' ){

			ctflg = 'y';
			strcpy( name , proc_sname (shflg,mrflg,count,ctflg) );
			
		/* Create planes for arb6. Planes will be formed with
		   normal pointing inward for creation of inner arb6 */

			ret = rt_mk_plane_3pts(planes[0],pt8[0],pt8[3],pt8[2],tols);
			ret = ret | rt_mk_plane_3pts(planes[1],pt8[2],pt8[3],pt8[6],tols);
			ret = ret | rt_mk_plane_3pts(planes[2],pt8[6],pt8[3],pt8[0],tols);
			ret = ret | rt_mk_plane_3pts(planes[3],pt8[4],pt8[0],pt8[1],tols);
			ret = ret | rt_mk_plane_3pts(planes[4],pt8[1],pt8[2],pt8[6],tols);


		/* Moves planes inward by normal thickness */

			for (i=0; i < 5; i++) {
				
				planes[i][3] += in[k].rsurf_thick;
			}

		/* Find new vertices of interior arb6 using
		   intersection of 3 planes subroutine */

			ret = ret | rt_mkpoint_3planes(inpt8[0],planes[0],planes[3],planes[2]);
			ret = ret | rt_mkpoint_3planes(inpt8[1],planes[0],planes[3],planes[4]);
			ret = ret | rt_mkpoint_3planes(inpt8[2],planes[0],planes[1],planes[4]);
			ret = ret | rt_mkpoint_3planes(inpt8[3],planes[0],planes[1],planes[2]);
			ret = ret | rt_mkpoint_3planes(inpt8[4],planes[2],planes[3],planes[4]);
			ret = ret | rt_mkpoint_3planes(inpt8[6],planes[1],planes[2],planes[4]);

			VMOVE(inpt8[7],inpt8[6]);
			VMOVE(inpt8[5],inpt8[4]);

			if( ret == 0 ) { /* valid record */
				
				mk_arb8( stdout, name, &inpt8[0][X] );
				(void)mk_addmember(name,&head,WMOP_SUBTRACT);
			}
			else {
				/* add to check group */
				(void)mk_addmember(name,&headf,WMOP_UNION);
			}
		}

		/* make regions for every num_unions solids */

		if ((count % num_unions) == 0)
			proc_region(name);

	}
	/* catch leftover solids */

	if ((count % num_unions) != 0)
		proc_region(name);

	/*   Mirror Processing - duplicates above code!   */
	ret = 0;
	for(k=0 ; k <= (cnt-1) && in[k].mirror != 0 ; k+=4){

		VSET( pt8[0], in[k].x,-in[k].y,in[k].z );
		VSET( pt8[1], in[k+1].x,-in[k+1].y,in[k+1].z );
		VSET( pt8[3], in[k+2].x,-in[k+2].y,in[k+2].z );
		VSET( pt8[4], in[k+3].x,-in[k+3].y,in[k+3].z );

		VSUB2(ab,pt8[4],pt8[0]);
		VSUB2(ac,pt8[3],pt8[0]);
		VSUB2(ad,pt8[1],pt8[0]);

		VADD3(pt8[7],ab,ac,pt8[0]);

		VADD3(pt8[2],ac,ad,pt8[0]);

		VMOVE(pt8[6],pt8[7]);
		VMOVE(pt8[5],pt8[4]);

		mrflg = 'y';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,mir_count+1,ctflg) );

		mk_arb8( stdout, name, &pt8[0][X] );
		mir_count++;

		(void) mk_addmember(name,&head,WMOP_UNION);
		if( in[k].surf_mode == '-' ){

			ctflg = 'y';
			strcpy( name , proc_sname (shflg,mrflg,count,ctflg) );

		/* Create planes for arb6. Planes will be formed with
		   normal pointing inward for creation of inner arb6 */

			ret = rt_mk_plane_3pts(planes[0],pt8[0],pt8[3],pt8[2],tols);
			ret = ret | rt_mk_plane_3pts(planes[1],pt8[2],pt8[3],pt8[6],tols);
			ret = ret | rt_mk_plane_3pts(planes[2],pt8[6],pt8[3],pt8[0],tols);
			ret = ret | rt_mk_plane_3pts(planes[3],pt8[4],pt8[0],pt8[1],tols);
			ret = ret | rt_mk_plane_3pts(planes[4],pt8[1],pt8[2],pt8[6],tols);


		/* Moves planes inward by normal thickness */

			for (i=0; i < 5; i++) {
				
				planes[i][3] += in[k].rsurf_thick;
			}

		/* Find new vertices of interior arb6 using
		   intersection of 3 planes subroutine */

			ret = ret | rt_mkpoint_3planes(inpt8[0],planes[0],planes[3],planes[2]);
			ret = ret | rt_mkpoint_3planes(inpt8[1],planes[0],planes[3],planes[4]);
			ret = ret | rt_mkpoint_3planes(inpt8[2],planes[0],planes[1],planes[4]);
			ret = ret | rt_mkpoint_3planes(inpt8[3],planes[0],planes[1],planes[2]);
			ret = ret | rt_mkpoint_3planes(inpt8[4],planes[2],planes[3],planes[4]);
			ret = ret | rt_mkpoint_3planes(inpt8[6],planes[1],planes[2],planes[4]);

			VMOVE(inpt8[7],inpt8[6]);
			VMOVE(inpt8[5],inpt8[4]);

			if( ret == 0 ) { /* valid record */

				mk_arb8( stdout, name, &pt8[0][X] );
				(void)mk_addmember(name,&head,WMOP_SUBTRACT);
			}
			else {
				/* add to check group */
				(void)mk_addmember(name,&headf,WMOP_UNION);
			}
		}

		if ((mir_count % num_unions) == 0)
			proc_region(name);
	}
	if ((count % num_unions) != 0)
		proc_region(name);

	last_cc = in[cnt-1].cc;
}

/*
 *
 *	 Process fastgen spheres - can handle hollowness 
 */
void
proc_sphere(cnt)
int cnt;
{
	fastf_t rad;
	point_t center;
	int i;
	char    shflg,mrflg,ctflg;
	static int count=0;
	static int mir_count=0;
	static int last_cc=0;
	char 	name[17];

	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count=0;
	}


	for( i=0 ; i < cnt ; i+=2 ){

		/* name solids */
		shflg = 's';
		mrflg = 'n';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,count+1,ctflg) );
		count++;

		VSET(center,in[i].x,in[i].y,in[i].z);

		/* Make sphere if it has a "Good Radius" */
		if( in[i+1].x > 0.0 ) {

			mk_sph(stdout,name,center,in[i+1].x);

			(void) mk_addmember(name,&head,WMOP_UNION);


			/* Check if hollow (i.e. plate mode) subract sphere with
				   radius R1 - thickness */

			if (in[i].surf_mode== '-'){

				/* name inside solid */

				ctflg = 'y';
				strcpy( name , proc_sname (shflg,mrflg,count,ctflg) );

				/* make inside solid */

				if( (rad = in[i+1].x - in[i].rsurf_thick) > 0.0 ) {
					mk_sph(stdout,name,center,rad);
					(void)mk_addmember(name, &head,WMOP_SUBTRACT);
				}
				else {
					/* add to check group */
					(void)mk_addmember(name, &headf,WMOP_UNION);
				}
			}

			if( (count % num_unions) == 0 )
				proc_region(name);
		}
		else {
			rt_log( "Bad component %s\n",name);
		}

	}

	/* clean up any loose solids into a region */
	if( (count % num_unions) != 0 )
		proc_region(name);

	for( i=0; i < cnt ; i+= 2 ) {

		if( in[i].mirror == 0 )
			continue;

		mrflg = 'y';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,mir_count+1,ctflg) );

		VSET(center,in[i].x,-in[i].y,in[i].z);

		if( in[i+1].x > 0.0 ) {
			mk_sph(stdout,name,center,in[i+1].x);
			mir_count++;

			(void) mk_addmember(name,&head,WMOP_UNION);

			/* Check if mirrored surface is hollow (i.e. plate mode) subract
				sphere with radius R1 - thickness */

			if (in[i].surf_mode== '-'){

				ctflg = 'y';
				strcpy( name , proc_sname (shflg,mrflg,mir_count,ctflg) );

				if( (rad = in[i+1].x - in[i].rsurf_thick) > 0.0 ) {
					mk_sph(stdout,name,center,rad);
					(void)mk_addmember(name, &head,WMOP_SUBTRACT);
				}
				else {
					/* add to check group */
					(void)mk_addmember(name, &headf,WMOP_UNION);
				}
			}

			if( (mir_count % num_unions) == 0 )
				proc_region(name);
		}
		else {
			rt_log( "Bad component %s\n",name);
		}

	}

	if( (count % num_unions) != 0 )
		proc_region(name);

	last_cc = in[cnt-1].cc;
}

/*
 *	Process fastgen box code
 */
void
proc_box(cnt)
int cnt;
{
	point_t	pts[1];
	point_t	pt8[8];
	int k,l;
	vect_t	ab, ac , ad, abi, aci, adi;
	vect_t	v1,v2,v3,v4;
	fastf_t len,leni;			/* box edge lengths */
	int valid;				/* valid inside box? */
	char    shflg,mrflg,ctflg;
	static int count=0;
	static int mir_count=0;
	static int last_cc=0;
	char	name[17];

	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count=0;
	}


	for(k=0 ; k <= (cnt-1) ; k+=4){
		VSET( pt8[0], in[k].x,in[k].y,in[k].z );
		VSET( pt8[1], in[k+1].x,in[k+1].y,in[k+1].z );
		VSET( pt8[4], in[k+2].x,in[k+2].y,in[k+2].z );
		VSET( pt8[3], in[k+3].x,in[k+3].y,in[k+3].z );

		VSUB2(ab,pt8[4],pt8[0]);
		VSUB2(ac,pt8[3],pt8[0]);
		VSUB2(ad,pt8[1],pt8[0]);

		VADD3(pt8[7],ab,ac,pt8[0]);
		VADD3(pt8[5],ab,ad,pt8[0]);
		VADD3(pt8[2],ac,ad,pt8[0]);
		VADD4(pt8[6],ab,ac,ad,pt8[0]);

		/* name solids */

		shflg = 'b';
		mrflg = 'n';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,count+1,ctflg) );

		/* make solid */

		mk_arb8( stdout, name, &pt8[0][X] );
		count++;

		(void) mk_addmember(name,&head,WMOP_UNION);

		if( in[k].surf_mode == '-' ){

			ctflg = 'y';
			strcpy( name , proc_sname (shflg,mrflg,count,ctflg) );

			valid = 1;
			len = MAGNITUDE( ab );
			leni = (len - (2.0 * in[k].rsurf_thick)) / len;
			if( leni > 0.0 ){
				VSCALE( abi, ab, leni );
				VSCALE( ab, ab, in[k].rsurf_thick / len );
			}
			else
				valid = 0;

			len = MAGNITUDE( ac );
			leni = (len - (2.0 * in[k].rsurf_thick)) / len;
			if( valid && leni > 0.0 ){
				VSCALE( aci, ac, leni );
				VSCALE( ac, ac, in[k].rsurf_thick / len );
			}
			else
				valid = 0;

			len = MAGNITUDE( ad );
			leni = (len - (2.0 * in[k].rsurf_thick)) / len;
			if( valid && leni > 0.0 ){
				VSCALE( adi, ad, leni );
				VSCALE( ad, ad, in[k].rsurf_thick / len );
			}
			else
				valid = 0;

			if( valid ) {
				VADD4( pt8[0], pt8[0], ab, ac, ad );
				VADD2(pt8[4],abi,pt8[0]);
				VADD2(pt8[3],aci,pt8[0]);
				VADD2(pt8[1],adi,pt8[0]);

				VADD3(pt8[7],abi,aci,pt8[0]);
				VADD3(pt8[5],abi,adi,pt8[0]);
				VADD3(pt8[2],aci,adi,pt8[0]);
				VADD4(pt8[6],abi,aci,adi,pt8[0]);

				mk_arb8( stdout, name, &pt8[0][X] );
				(void)mk_addmember(name,&head,WMOP_SUBTRACT);
			}
			else {
				/* add to check group */
				(void)mk_addmember(name,&headf,WMOP_UNION);
			}
		}

		/* make region for every num_unions solids */

		if ((count % num_unions) == 0)
			proc_region(name);

	}
	/* catch leftover solids */

	if ((count % num_unions) != 0)
		proc_region(name);


	/*   Mirror Processing - duplicates above code!   */

	for(k=0 ; k <= (cnt-1) && in[k].mirror != 0 ; k+=4){
		VSET( pt8[0], in[k].x,-in[k].y,in[k].z );
		VSET( pt8[1], in[k+1].x,-in[k+1].y,in[k+1].z );
		VSET( pt8[4], in[k+2].x,-in[k+2].y,in[k+2].z );
		VSET( pt8[3], in[k+3].x,-in[k+3].y,in[k+3].z );

		VSUB2(ab,pt8[4],pt8[0]);
		VSUB2(ac,pt8[3],pt8[0]);
		VSUB2(ad,pt8[1],pt8[0]);

		VADD3(pt8[7],ab,ac,pt8[0]);
		VADD3(pt8[5],ab,ad,pt8[0]);
		VADD3(pt8[2],ac,ad,pt8[0]);
		VADD4(pt8[6],ab,ac,ad,pt8[0]);

		mrflg = 'y';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,mir_count+1,ctflg) );

		mk_arb8( stdout, name, &pt8[0][X] );
		mir_count++;

		(void) mk_addmember(name,&head,WMOP_UNION);

		if( in[k].surf_mode == '-' ){

			ctflg = 'y';
			strcpy( name , proc_sname (shflg,mrflg,mir_count+1,ctflg) );

			valid = 1;
			len = MAGNITUDE( ab );
			leni = (len - (2.0 * in[k].rsurf_thick)) / len;
			if( leni > 0.0 ){
				VSCALE( abi, ab, leni );
				VSCALE( ab, ab, in[k].rsurf_thick / len );
			}
			else
				valid = 0;

			len = MAGNITUDE( ac );
			leni = (len - (2.0 * in[k].rsurf_thick)) / len;
			if( valid && leni > 0.0 ){
				VSCALE( aci, ac, leni );
				VSCALE( ac, ac, in[k].rsurf_thick / len );
			}
			else
				valid = 0;

			len = MAGNITUDE( ad );
			leni = (len - (2.0 * in[k].rsurf_thick)) / len;
			if( valid && leni > 0.0 ){
				VSCALE( adi, ad, leni );
				VSCALE( ad, ad, in[k].rsurf_thick / len );
			}
			else
				valid = 0;

			if( valid ) {
				VADD4( pt8[0], pt8[0], ab, ac, ad );
				VADD2(pt8[4],abi,pt8[0]);
				VADD2(pt8[3],aci,pt8[0]);
				VADD2(pt8[1],adi,pt8[0]);

				VADD3(pt8[7],abi,aci,pt8[0]);
				VADD3(pt8[5],abi,adi,pt8[0]);
				VADD3(pt8[2],aci,adi,pt8[0]);
				VADD4(pt8[6],abi,aci,adi,pt8[0]);

				mk_arb8( stdout, name, &pt8[0][X] );
				(void)mk_addmember(name,&head,WMOP_SUBTRACT);
			}
			else {
				/* add to check group */
				(void)mk_addmember(name,&headf,WMOP_UNION);
			}
		}

		if ((mir_count % num_unions) == 0)
			proc_region(name);
	}
	if ((count % num_unions) != 0)
		proc_region(name);
	last_cc = in[cnt-1].cc;
}

/*
 *	Donuts
 *
 *	These are specific combinations of two sort of "cones".
 *	Handled by creating two to eight TGC's and combining appropriately.
 *	In order to use "donuts", rpatch must have been invoked with the "-D" option.
 */
void
proc_donut(cnt)
int cnt;
{
	int k;
	point_t base1, top1, base2, top2;
	point_t base1_in, top1_in, base2_in, top2_in;
	fastf_t rbase1, rtop1, rbase2, rtop2;
	fastf_t rbase1_in, rtop1_in, rbase2_in, rtop2_in;
	vect_t h1, h2, h3, h4;
	fastf_t magh3, magh4;
	int end_code;
	struct wmember donut_head;
	char name[17];
	char shflg,mrflg,ctflg;
	int count=0;
	int make_basic_solids;
	char scratch_name1[17];
	char scratch_name2[17];
	char scratch_name3[17];
	char scratch_name4[17];

	for( k=0 ; k<cnt-1 ; k += 6 )	/* for each donut */
	{
		if( (in[k].x==in[k+1].x)&&(in[k].y==in[k+1].y)&&(in[k].z==in[k+1].z) )
		{
			bu_log( "Bad Donut Length for component #%d",in[k].cc);
			continue;
		}
		if( (in[k+3].x==in[k+4].x)&&(in[k+3].y==in[k+4].y)&&(in[k+3].z==in[k+4].z) )
		{
			bu_log( "Bad Donut Length for component #%d\n",in[k].cc);
			continue;
		}

		/* get base and top location of each of the two TRC's */
		VSET( base1, in[k].x, in[k].y, in[k].z );
		VSET( top1, in[k+1].x, in[k+1].y, in[k+1].z );
		VSET( base2, in[k+3].x, in[k+3].y, in[k+3].z );
		VSET( top2, in[k+4].x, in[k+4].y, in[k+4].z );

		/* get radii and bottom and top of each TRC
		 * BRL-CAD insists on non-zero radii
		 */
		rbase1 = in[k+2].x;
		if( rbase1 < tol.dist )
			rbase1 = tol.dist;
		rtop1 = in[k+2].y;
		if( rtop1 < tol.dist )
			rtop1 = tol.dist;

		rbase2 = in[k+5].x;
		if( rbase2 < tol.dist )
			rbase2 = tol.dist;
		rtop2 = in[k+5].y;
		if( rtop2 < tol.dist )
			rtop2 = tol.dist;

		if( rbase2 > rbase1 )
		{
			bu_log( "Bad Donut: inner base radius bigger than outer for component #%d\n", in[k].cc );
			continue;
		}

		if( rtop2 > rtop1 )
		{
			bu_log( "Bad Donut: inner top radius bigger than outer for component #%d\n", in[k].cc );
			continue;
		}

		/* calculate height vectors for the two basic TRC's */
		VSUB2( h2, top2, base2 );
		VSUB2( h1, top1, base1 );

		/* calculate height vectors for the 'end' TRC's
		 * (when inner and outer TRC ends are not the same)
		 */
		VSUB2( h3, base1, base2 );
		magh3 = MAGNITUDE( h3 );
		VSUB2( h4, top1, top2 );
		magh4 = MAGNITUDE( h4 );

		/* If inner and outer TRC ends are nearly the same, make them the same */
		if( magh3 < tol.dist )
		{
			magh3 = 0.0;
			VMOVE( base1, base2 )
		}

		if( magh4 < tol.dist )
		{
			magh4 = 0.0;
			VMOVE( top1, top2 )
		}

		/* get end code */
		end_code = (int)(in[k+5].z/mmtin);

		/* make name of this component */
		shflg = 'd';
		mrflg = 'n';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,count+1,ctflg) );
		count++;

		BU_LIST_INIT( &donut_head.l );

		/* in some cases we won't even need the two basic TRC's */
		make_basic_solids = 1;
		if( in[k].surf_mode == '-' ) /* plate mode */
		{
			if( magh3 > 0.0 && end_code == 4 )
				make_basic_solids = 0;
			else if( magh4 > 0.0 && end_code == 5 )
				make_basic_solids = 0;
			else if( magh3 > 0.0 && magh4 > 0.0 && end_code == 6 )
				make_basic_solids = 0;
		}

		/* make the two basic TRC's */
		if( make_basic_solids )
		{
			sprintf( scratch_name1, "tmp.%d", scratch_num );
			scratch_num++;
			mk_trc_top( stdout, scratch_name1, base1, top1, rbase1, rtop1 );

			sprintf( scratch_name2, "tmp.%d", scratch_num );
			scratch_num++;
			mk_trc_top( stdout, scratch_name2, base2, top2, rbase2, rtop2 );
		}

		/* make the end TRC's if needed */
		if( magh3 > 0.0 )
		{
			if( in[k].surf_mode != '-' ||
			    end_code == 2 || end_code == 3 || end_code == 4 || end_code == 6 )
			{
				sprintf( scratch_name3, "tmp.%d", scratch_num );
				scratch_num++;
				mk_trc_top( stdout, scratch_name3, base2, base1, rbase2, rbase1 );
			}
		}

		if( magh4 > 0.0 )
		{
			if( in[k].surf_mode != '-' ||
			    end_code == 1 || end_code == 3 || end_code == 5 || end_code == 6 )
			{
				sprintf( scratch_name4, "tmp.%d", scratch_num );
				scratch_num++;
				mk_trc_top( stdout, scratch_name4, top2, top1, rtop2, rtop1 );
			}
		}

		if( in[k].surf_mode != '-' )	/* volume mode */
		{
			fastf_t dot3, dot4;

			dot3 = VDOT( h3, h1 );
			dot4 = VDOT( h4, h1 );

			if( magh3 > 0.0 && magh4 > 0.0 )
			{
				/* both ends have TRC's built */
				if( dot3 > 0.0 && dot4 < 0.0 )
				{
					/* union in both ends */
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name3, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name4, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
				}
				else if( dot3 < 0.0 && dot4 > 0.0 )
				{
					/* subtract both ends */
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name3, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name4, &donut_head, WMOP_SUBTRACT );
				}
				else if( dot3 > 0.0 && dot4 > 0.0 )
				{
					/* union #3 subtract #4 */
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name4, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name3, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
				}
				else
				{
					/* union #4 subtract #3 */
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name3, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name4, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
				}
			}
			else if( magh3 > 0.0 )
			{
				/* only first end has a TRC */
				if( dot3 > 0.0 )
				{
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name3, &donut_head, WMOP_UNION );
				}
				else
				{
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name3, &donut_head, WMOP_SUBTRACT );
				}
			}
			else if( magh4 > 0.0 )
			{
				/* only second end has a TRC */
				if( dot4 < 0.0 )
				{
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name4, &donut_head, WMOP_UNION );
				}
				else
				{
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
					(void)mk_addmember( scratch_name4, &donut_head, WMOP_SUBTRACT );
				}
			}
			else
			{
				/* no end TRC's */
				(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
				(void)mk_addmember( scratch_name2, &donut_head, WMOP_SUBTRACT );
			}

			if( BU_LIST_NON_EMPTY( &donut_head.l ) )
			{
				mk_lfcomb( stdout, name, &donut_head, 0 );
				(void)mk_addmember( name, &head, WMOP_UNION );
			}

			continue;
		}
		else	/* plate mode */
		{
			char scratch_name1_in[17];
			char scratch_name2_in[17];
			char scratch_name3_in[17];
			char scratch_name4_in[17];

			if( end_code == 0 || end_code == 1 || end_code == 2 || end_code ==3 )
			{
				/* Need sides */
				if( make_inside_trc( base1, top1, rbase1, rtop1, base1_in, top1_in, &rbase1_in, &rtop1_in, 0, 0, 1, in[k+4].rsurf_thick ) )
				{
					bu_log( "ERROR: Unable to create plate thickness of %g inches for component #%d\n",
						in[k+4].rsurf_thick*conv_mm2in, in[k].cc );
					continue;
				}

				sprintf( scratch_name1_in, "tmp.%d", scratch_num );
				scratch_num++;
				mk_trc_top(stdout,scratch_name1_in,base1_in,top1_in,rbase1_in,rtop1_in);
				(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
				(void)mk_addmember(scratch_name1_in,&donut_head,WMOP_SUBTRACT);

				if( make_inside_trc( base2, top2, rbase2, rtop2, base2_in, top2_in, &rbase2_in, &rtop2_in, 0, 0, 1, in[k+4].rsurf_thick ) )
				{
					bu_log( "ERROR: Unable to create plate thickness of %g inches for component #%d\n",
						in[k+4].rsurf_thick*conv_mm2in, in[k].cc );
					continue;
				}

				sprintf( scratch_name2_in, "tmp.%d", scratch_num );
				scratch_num++;
				mk_trc_top(stdout,scratch_name2_in,base2_in,top2_in,rbase2_in,rtop2_in);
				(void)mk_addmember( scratch_name2, &donut_head, WMOP_UNION );
				(void)mk_addmember(scratch_name2_in,&donut_head,WMOP_SUBTRACT);
			}

			if( end_code == 1 || end_code == 3 || end_code == 5 || end_code == 6 )
			{
				vect_t base_in, top_in;
				fastf_t rbase_in, rtop_in;

				/* close end B (top) */
				if( magh4 > 0.0 )
				{
					if( make_inside_trc( top2, top1, rtop2, rtop1, base_in, top_in, &rbase_in, &rtop_in, 0, 0, 1, in[k+4].rsurf_thick ) )
					{
						bu_log( "ERROR: Unable to create plate thickness of %g inches for component #%d\n",
							in[k+4].rsurf_thick*conv_mm2in, in[k].cc );
						continue;
					}
					sprintf( scratch_name4_in, "tmp.%d", scratch_num );
					scratch_num++;
					mk_trc_top(stdout,scratch_name4_in,base_in,top_in,rbase_in,rtop_in);
					(void)mk_addmember( scratch_name4, &donut_head, WMOP_UNION );
					(void)mk_addmember(scratch_name4_in,&donut_head,WMOP_SUBTRACT);
				}
				else
				{
					if( make_inside_trc( base1, top1, rbase1, rtop1, base_in, top_in, &rbase_in, &rtop_in, 0, 1, 0, in[k+4].rsurf_thick ) )
					{
						bu_log( "ERROR: Unable to create plate thickness of %g inches for component #%d\n",
							in[k+4].rsurf_thick*conv_mm2in, in[k].cc );
						continue;
					}
					sprintf( scratch_name4_in, "tmp.%d", scratch_num );
					scratch_num++;
					mk_trc_top(stdout,scratch_name4_in,base_in,top_in,rbase_in,rtop_in);
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember(scratch_name4_in,&donut_head,WMOP_SUBTRACT);
					(void)mk_addmember(scratch_name2,&donut_head,WMOP_SUBTRACT);
				}
			}
			if( end_code == 2 || end_code == 3 || end_code == 4 || end_code == 6 )
			{
				vect_t base_in, top_in;
				fastf_t rbase_in, rtop_in;

				/* close end A (base) */
				if( magh3 > 0.0 )
				{
					if( make_inside_trc( base2, base1, rbase2, rbase1, base_in, top_in, &rbase_in, &rtop_in, 0, 0, 1, in[k+4].rsurf_thick ) )
					{
						bu_log( "ERROR: Unable to create plate thickness of %g inches for component #%d\n",
							in[k+4].rsurf_thick*conv_mm2in, in[k].cc );
						continue;
					}
					sprintf( scratch_name3_in, "tmp.%d", scratch_num );
					scratch_num++;
					mk_trc_top(stdout,scratch_name3_in,base_in,top_in,rbase_in,rtop_in);
					(void)mk_addmember( scratch_name3, &donut_head, WMOP_UNION );
					(void)mk_addmember(scratch_name3_in,&donut_head,WMOP_SUBTRACT);
				}
				else
				{
					if( make_inside_trc( base1, top1, rbase1, rtop1, base_in, top_in, &rbase_in, &rtop_in, 1, 0, 0, in[k+4].rsurf_thick ) )
					{
						bu_log( "ERROR: Unable to create plate thickness of %g inches for component #%d\n",
							in[k+4].rsurf_thick*conv_mm2in, in[k].cc );
						continue;
					}
					sprintf( scratch_name3_in, "tmp.%d", scratch_num );
					scratch_num++;
					mk_trc_top(stdout,scratch_name3_in,base_in,top_in,rbase_in,rtop_in);
					(void)mk_addmember( scratch_name1, &donut_head, WMOP_UNION );
					(void)mk_addmember(scratch_name3_in,&donut_head,WMOP_SUBTRACT);
					(void)mk_addmember(scratch_name2,&donut_head,WMOP_SUBTRACT);
				}
			}

			if( BU_LIST_NON_EMPTY( &donut_head.l ) )
			{
				mk_lfcomb( stdout, name, &donut_head, 0 );
				(void)mk_addmember( name, &head, WMOP_UNION );
			}

			continue;
		}
	}

	if( BU_LIST_NON_EMPTY( &head.l ) )
		proc_region( name );
}

/* 
 *	Cylinder Fastgen Support:
 *	Cylinders have the added complexity of being plate or volume mode,
 *	and closed vs. open ends. This makes things a bit ugly. 
 *
 *	NOTE:	This handles plate mode subtractions. It also handles a
 *		subset of the allowable volume mode subtractions, in that
 *		it will correctly hollow cylinders in a pairwise manner.
 *		If cylinder1 *completely* encloses cylinder2, then cylinder2
 *		will be subtracted from cylinder1.
 *
 */
void
proc_cylin(cnt)
int cnt;
{
	point_t base;
	point_t	top;
	point_t sbase;			/* For subtraction case */
	point_t	stop; 			/* For subtraction case */
	vect_t	ab,bc;
	fastf_t	rad1,rad2;
	fastf_t srad1,srad2;		/* for subtraction case */
	int k,j;
	struct subtract_list *slist,*get_subtract();
	double	thick,ht,sht;
	char    shflg,mrflg,ctflg;
	static int count=0;
	static int mir_count=0;
	static int last_cc=0;
	char	name[17];

	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count=0;
	}


	slist = get_subtract(cnt);
	if( debug>2 ){
		struct subtract_list *sp;

		for( sp=slist; sp; sp=sp->next )
			rt_log( "%d %d %d\n", 
			    sp->outsolid,sp->insolid,sp->inmirror );
	}


	for(k=0 ; k < (cnt-1) ; k+=3){	 /* For all sub-cylinders in this cc */

		/* name solids */
		shflg = 'c';
		mrflg = 'n';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,count+1,ctflg) );

		count++;

		/* Test for a cylinder with no length, all conditions must be true to fail. */

		if(!((in[k].x==in[k+1].x)&&(in[k].y==in[k+1].y)&&(in[k].z==in[k+1].z))){

			VSET(base,in[k].x,in[k].y,in[k].z);
			VSET(top,in[k+1].x,in[k+1].y,in[k+1].z);

			/* change valid 0 radius cone pts to very small radii,
			 * also treat negative value radii as positive.
			 */
			if(in[k+2].x == 0) 
			  in[k+2].x = .00001;
			if(in[k+2].x < 0)
			  in[k+2].x = -in[k+2].x;
			if(in[k+2].y == 0) 
			  in[k+2].y = .00001;
			if(in[k+2].y < 0)
			  in[k+2].y = -in[k+2].y;

			/* make solid */

			mk_trc_top(stdout,name,base,top,in[k+2].x,in[k+2].y);
			mk_cyladdmember(name,&head,slist,0);

			/* mk_trc_top destroys the values of base,top */
			VSET(base,in[k].x,in[k].y,in[k].z);
			VSET(top,in[k+1].x,in[k+1].y,in[k+1].z);

			j = (int)(in[k+2].z/mmtin);

			if (in[k].surf_mode== '-'){     /* Plate mode */
				vect_t unit_h;
				fastf_t sin_ang;
				fastf_t rad1_tmp,rad2_tmp;

				ctflg = 'y';
				strcpy( name , proc_sname (shflg,mrflg,count,ctflg) );

				rad1 = in[k+2].x;
				rad2 = in[k+2].y;
				VSUB2(ab,top,base);
				ht = MAGNITUDE( ab );
				sin_ang = ht/hypot( ht, rad2-rad1 );
				switch(j){

				case 0: /* Both ends open */

					thick = in[k+2].rsurf_thick / sin_ang;
					srad1 = rad1 - thick;
					srad2 = rad2 - thick;

					if( srad1 <= 0.0 && srad2 <= 0.0 )
						(void)mk_addmember(name,&headf,WMOP_UNION);
					else if( srad1 <= 0.0 )
					{
						fastf_t new_h_factor;
						fastf_t invers_height;

						invers_height = 1.0/ht;
						VSCALE( unit_h, ab, invers_height )
						new_h_factor = (thick - rad1)*(ht/(rad2-rad1));
						VJOIN1( sbase, base, new_h_factor, unit_h )

						/* base radius should really be zero, get close */
						srad1 = .00001;
						mk_trc_top(stdout,name,sbase,top,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else if( srad2 <= 0.0 )
					{
						fastf_t new_h_factor;
						fastf_t invers_height;

						invers_height = 1.0/ht;
						VSCALE( unit_h, ab, invers_height )
						new_h_factor = ht + srad2 * (ht/(rad1-rad2));
						VJOIN1( stop, base, new_h_factor, unit_h )

						/* top radius should really be zero, get close */
						srad2 = .00001;
						mk_trc_top(stdout,name,base,stop,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else
					{
						mk_trc_top(stdout,name,base,top,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					break;

				case 1: /* Base closed, top open */

					VSUB2(ab,top,base);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
					VMOVE( unit_h, ab );
					VSCALE(ab,ab,in[k].rsurf_thick);
					VADD2(sbase,base,ab);

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,top,sbase);
					sht = MAGNITUDE( ab );
					srad1 = rad2 - sht / ht * (rad2 - rad1);
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = srad1 - thick;
					srad2 = rad2 - thick;

					if( srad1 <= 0.0 && srad2 <= 0.0 )
						(void)mk_addmember(name,&headf,WMOP_UNION);
					else if( srad1 <= 0.0 )
					{
						fastf_t new_ht;

						new_ht = ht*(thick - rad1)/(rad2-rad1);
						if( new_ht >= ht )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( sbase, base, new_ht, unit_h );
							srad1 = 0.00001;
							mk_trc_top(stdout,name,sbase,top,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else if( srad2 <= 0.0 )
					{
						fastf_t new_ht;

						new_ht = sht + ht*srad2/(rad1-rad2);
						if( new_ht <= 0.0 )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( stop, sbase, new_ht, unit_h );
							srad2 = 0.00001;
							mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else
					{
						mk_trc_top(stdout,name,sbase,top,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					break;
				case 2: /* Base open, top closed */

					VSUB2(ab,base,top);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
					VMOVE( unit_h, ab );
					VSCALE(ab,ab,in[k].rsurf_thick);
					VADD2(stop,top,ab);

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,stop,base);
					sht = MAGNITUDE( ab );
					srad2 = rad1 - sht / ht * (rad1 - rad2);
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = rad1 - thick;
					srad2 = srad2 - thick;

					if( srad1 <= 0.0 && srad2 <= 0.0 )
						(void)mk_addmember(name,&headf,WMOP_UNION);
					else if( srad1 <= 0.0 )
					{
						fastf_t new_ht;

						new_ht = sht + ht*srad1/(rad2-rad1);
						if( new_ht <= 0.0 )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( sbase, stop, new_ht, unit_h )
							srad1 = 0.00001;
							mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else if( srad2 <= 0.0 )
					{
						fastf_t new_ht;

						new_ht = ht*(thick - rad2)/(rad1-rad2);
						if( new_ht >= ht )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( stop, top, new_ht, unit_h )
							srad2 = 0.00001;
							mk_trc_top(stdout,name,base,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else
					{
						mk_trc_top(stdout,name,base,stop,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					break;

				case 3: /* Both closed */

					VSUB2(ab,top,base);
					VSUB2(bc,base,top);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
					VMOVE( unit_h, ab );
					VUNITIZE(bc);
					VSCALE(ab,ab,in[k].rsurf_thick);
					VSCALE(bc,bc,in[k].rsurf_thick);
					VADD2(sbase,base,ab);
					VADD2(stop,top,bc);

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,stop,base);
					sht = MAGNITUDE( ab );
					rad1_tmp = rad2 - sht / ht * (rad2 - rad1);
					rad2_tmp = rad1 - sht / ht * (rad1 - rad2);
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = rad1_tmp - thick;
					srad2 = rad2_tmp - thick;

					if( (srad1 <= 0.0 && srad2 <= 0.0) || sht <= 0.0)
						(void)mk_addmember(name,&headf,WMOP_UNION);
					else if( srad1 < 0.0 )
					{
						fastf_t new_ht;

						new_ht = ht*(thick-rad1_tmp)/(rad2_tmp-rad1_tmp);
						if( new_ht >= sht )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( sbase, sbase, new_ht, unit_h )
							srad1 = 0.00001;
							mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else if( srad2 <= 0.0 )
					{
						fastf_t new_ht;
						vect_t rev_h;


						new_ht = (-ht*srad2/(rad1-rad2));
						if( new_ht <= 0.0 )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VREVERSE( rev_h, unit_h )
							VJOIN1( stop, stop, new_ht, rev_h )
							srad2 = 0.00001;
							mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else
					{
						mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					break;

				default:
					rt_log( "Unknown cylinder mode\n");
					break;

				}         /* end switch */
			}     		  /* end - plate mode modifications */
		}         			  /* Degenerate length check */
		else {
			rt_log( "Bad Cylinder Length for %s\n",name);
		}
		/* make regions */

		/* due to solid subtractions, this might be a null region */

		if ((count % num_unions) == 0 && (RT_LIST_NEXT_NOT_HEAD(&head, &head.l)))
			proc_region(name);

	} 	   /* end - for */
	/* catch missed solids into final region */

	/* due to solid subtractions, this might be a null region */
	if ((count % num_unions) != 0 && (RT_LIST_NEXT_NOT_HEAD(&head, &head.l)))
		proc_region(name);

	/*    Mirror Processing - duplicates above code!   */

	for(k=0 ; k < (cnt-1) ; k+=3){

		if( in[k].mirror == 0 )
			continue;

		mrflg = 'y';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,mir_count+1,ctflg) );
		mir_count++;

		if(!((in[k].x==in[k+1].x)&&(in[k].y==in[k+1].y)&&(in[k].z==in[k+1].z))){

			VSET(base,in[k].x,-in[k].y,in[k].z);
			VSET(top,in[k+1].x,-in[k+1].y,in[k+1].z);

			if(in[k+2].x == 0) 
				in[k+2].x = .00001;
			if(in[k+2].x < 0)
				in[k+2].x = -in[k+2].x;
			if(in[k+2].y == 0) 
				in[k+2].y = .00001;
			if(in[k+2].y < 0)
				in[k+2].y = -in[k+2].y;

			mk_trc_top(stdout,name,base,top,in[k+2].x,in[k+2].y);
			mk_cyladdmember(name,&head,slist,1);

			/* mk_trc_top destroys the values of base,top */
			VSET(base,in[k].x,-in[k].y,in[k].z);
			VSET(top,in[k+1].x,-in[k+1].y,in[k+1].z);

			j = (int)(in[k+2].z/mmtin);

			if (in[k].surf_mode== '-'){ 	/* Plate mode */
				vect_t unit_h;
				fastf_t sin_ang;
				fastf_t rad1_tmp,rad2_tmp;

				ctflg = 'y';
				strcpy( name , proc_sname (shflg,mrflg,mir_count,ctflg) );

				rad1 = in[k+2].x;
				rad2 = in[k+2].y;
				VSUB2(ab,top,base);
				ht = MAGNITUDE( ab );
				sin_ang = ht/hypot( ht, rad2-rad1 );

				switch(j){

				case 0: /* Both ends open */

					thick = in[k+2].rsurf_thick / sin_ang;
					srad1 = rad1 - thick;
					srad2 = rad2 - thick;

					if( srad1 <= 0.0 && srad2 <= 0.0 )
						(void)mk_addmember(name,&headf,WMOP_UNION);
					else if( srad1 <= 0.0 )
					{
						fastf_t new_h_factor;
						fastf_t invers_height;

						invers_height = 1.0/ht;
						VSCALE( unit_h, ab, invers_height )
						new_h_factor = (thick - rad1)*(ht/(rad2-rad1));
						VJOIN1( sbase, base, new_h_factor, unit_h )

						/* base radius should really be zero, get close */
						srad1 = .00001;
						mk_trc_top(stdout,name,sbase,top,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else if( srad2 <= 0.0 )
					{
						fastf_t new_h_factor;
						fastf_t invers_height;

						invers_height = 1.0/ht;
						VSCALE( unit_h, ab, invers_height )
						new_h_factor = ht + srad2 * (ht/(rad1-rad2));
						VJOIN1( stop, base, new_h_factor, unit_h )

						/* top radius should really be zero, get close */
						srad2 = .00001;
						mk_trc_top(stdout,name,base,stop,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else
					{
						mk_trc_top(stdout,name,base,top,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					break;

				case 1: /* Base closed, top open */

					VSUB2(ab,top,base);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
					VMOVE( unit_h, ab );
					VSCALE(ab,ab,in[k].rsurf_thick);
					VADD2(sbase,base,ab);

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,top,sbase);
					sht = MAGNITUDE( ab );
					srad1 = rad2 - sht / ht * (rad2 - rad1);
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = srad1 - thick;
					srad2 = rad2 - thick;

					if( srad1 <= 0.0 && srad2 <= 0.0 )
						(void)mk_addmember(name,&headf,WMOP_UNION);
					else if( srad1 <= 0.0 )
					{
						fastf_t new_ht;

						new_ht = ht*(thick - rad1)/(rad2-rad1);
						if( new_ht >= ht )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( sbase, base, new_ht, unit_h );
							srad1 = 0.00001;
							mk_trc_top(stdout,name,sbase,top,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else if( srad2 <= 0.0 )
					{
						fastf_t new_ht;

						new_ht = sht + ht*srad2/(rad1-rad2);
						if( new_ht <= 0.0 )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( stop, sbase, new_ht, unit_h );
							srad2 = 0.00001;
							mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else
					{
						mk_trc_top(stdout,name,sbase,top,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					break;
				case 2: /* Base open, top closed */

					VSUB2(ab,base,top);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
					VMOVE( unit_h, ab );
					VSCALE(ab,ab,in[k].rsurf_thick);
					VADD2(stop,top,ab);

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,stop,base);
					sht = MAGNITUDE( ab );
					srad2 = rad1 - sht / ht * (rad1 - rad2);
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = rad1 - thick;
					srad2 = srad2 - thick;

					if( srad1 <= 0.0 && srad2 <= 0.0 )
						(void)mk_addmember(name,&headf,WMOP_UNION);
					else if( srad1 <= 0.0 )
					{
						fastf_t new_ht;

						new_ht = sht + ht*srad1/(rad2-rad1);
						if( new_ht <= 0.0 )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( sbase, stop, new_ht, unit_h )
							srad1 = 0.00001;
							mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else if( srad2 <= 0.0 )
					{
						fastf_t new_ht;

						new_ht = ht*(thick - rad2)/(rad1-rad2);
						if( new_ht >= ht )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( stop, top, new_ht, unit_h )
							srad2 = 0.00001;
							mk_trc_top(stdout,name,base,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else
					{
						mk_trc_top(stdout,name,base,stop,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					break;

				case 3: /* Both closed */

					VSUB2(ab,top,base);
					VSUB2(bc,base,top);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
					VMOVE( unit_h, ab );
					VUNITIZE(bc);
					VSCALE(ab,ab,in[k].rsurf_thick);
					VSCALE(bc,bc,in[k].rsurf_thick);
					VADD2(sbase,base,ab);
					VADD2(stop,top,bc);

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,stop,base);
					sht = MAGNITUDE( ab );
					rad1_tmp = rad2 - sht / ht * (rad2 - rad1);
					rad2_tmp = rad1 - sht / ht * (rad1 - rad2);
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = rad1_tmp - thick;
					srad2 = rad2_tmp - thick;

					if( (srad1 <= 0.0 && srad2 <= 0.0) || sht <= 0.0)
						(void)mk_addmember(name,&headf,WMOP_UNION);
					else if( srad1 < 0.0 )
					{
						fastf_t new_ht;

						new_ht = ht*(thick-rad1_tmp)/(rad2_tmp-rad1_tmp);
						if( new_ht >= sht )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VJOIN1( sbase, sbase, new_ht, unit_h )
							srad1 = 0.00001;
							mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else if( srad2 <= 0.0 )
					{
						fastf_t new_ht;
						vect_t rev_h;


						new_ht = (-ht*srad2/(rad1-rad2));
						if( new_ht <= 0.0 )
							(void)mk_addmember(name,&headf,WMOP_UNION);
						else
						{
							VREVERSE( rev_h, unit_h )
							VJOIN1( stop, stop, new_ht, rev_h )
							srad2 = 0.00001;
							mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
							(void)mk_addmember(name,&head,WMOP_SUBTRACT);
						}
					}
					else
					{
						mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
						(void)mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					break;

				default:
					rt_log( "Unknown cylinder mode\n");
					break;

				}         /* end switch */
			}/* plate mode */
		}
		else {
			rt_log( "Bad Cylinder Length for %s\n",name);
		}
		/* due to solid subtractions, this might be a null region */
		if ((mir_count % num_unions) == 0 && (RT_LIST_NEXT_NOT_HEAD(&head, &head.l)))
			proc_region(name);
	}	/* end for k loop */
	/* due to solid subtractions, this might be a null region */

	if ((count % num_unions) != 0 && (RT_LIST_NEXT_NOT_HEAD(&head, &head.l)))
		proc_region(name);

	last_cc = in[cnt-1].cc;
}

/*
 *	Process fastgen rod mode
 */
void
proc_rod(cnt)
int cnt;
{

	int k,l,index;
	point_t base;
	point_t	top;
	fastf_t tmp;
	fastf_t tmp1;
	char    shflg,mrflg,ctflg;
	static int count=0;
	static int mir_count=0;
	static int last_cc=0;
	char	name[17];

	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count=0;
	}


	for(k=0 ; k < cnt ; k++){
		for(l=0; l<= 7; l++){

			if(in[k].ept[l] > 0){

				index = in[k].ept[l];
				list[index].x = in[k].x;
				list[index].y = in[k].y;
				list[index].z = in[k].z;
				list[index].radius = in[k].rsurf_thick;
				list[index].mirror = in[k].mirror;
				list[index].flag = 1;

				if (debug > 3)
					rt_log( "%d %f %f %f %f %d\n",list[index].flag,in[k].x,in[k].y,in[k].z,in[k].rsurf_thick,
					    in[k].mirror);
			}

		}
	}

	/* Everything is sequenced, but separated, compress into single array here */
	/* list[0] will not hold anything, so don't look */

	l = 0;
	for (k=1; k<10000; k++){
		if(list[k].flag == 1){
			list[k].flag = 0;
			x[l] = list[k].x;
			y[l] = list[k].y;
			z[l] = list[k].z;
			radius[l] = list[k].radius;
			mirror[l] = list[k].mirror;

			l= l+1;
			if (debug > 3)
				rt_log( "k=%d l=%d %f %f %f %f %d flag=%d\n",
				    k,l,list[k].x,list[k].y,list[k].z,
				    list[k].flag,list[k].radius,list[k].mirror);
		}
	}

	if (debug > 2){
		for (k=1;(k<=l);k++)
			rt_log( "compressed: %d %f %f %f %f %d\n",
			    k,x[k],y[k],z[k],radius[k],mirror[k]);
	}

	for(k=1 ; k < (l-1) ; k++){

		if( (x[k]==x[k+1]) && (y[k]==y[k+1]) && (z[k]==z[k+1]) ) {
			k += 2;
			continue;
		}

		/* name solids */
		shflg = 'r';
		mrflg = 'n';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,count+1,ctflg) );

		/* make solids */
		count++;

		VSET(base,x[k],y[k],z[k]);
		VSET(top,x[k+1],y[k+1],z[k+1]);

		tmp = radius[k];
		tmp1 = radius[k+1];

		if((tmp > 0)&&(tmp1 > 0)){
			mk_trc_top(stdout,name,base,top,tmp,tmp1);
		}
		else {
			rt_log( "Bad Rod Radius for %s\n",name);
		}

		if( count > 1 && (count % num_unions) == 0 ){
			(void)mk_addmember(name,&head,WMOP_SUBTRACT);
			proc_region( name );
			(void)mk_addmember(name,&head,WMOP_UNION);
		} else {
			(void) mk_addmember(name,&head,WMOP_UNION);
		}
	}
	/* catch leftover solids */
	if( count > 0  ) {
		proc_region( name );
	}

	/*    Mirror Processing - duplicates above code!    */

	for( k=1 ; k < (l-1) ; k++){

		if( mirror[k] == 0 )
			continue;

		if( (x[k]==x[k+1]) && (y[k]==y[k+1]) && (z[k]==z[k+1]) ) {
			k += 2;
			continue;
		}


		mrflg = 'y';
		ctflg = 'n';
		strcpy( name , proc_sname (shflg,mrflg,mir_count+1,ctflg) );

		/* make solids */
		mir_count++;

		VSET(base,x[k],-y[k],z[k]);
		VSET(top,x[k+1],-y[k+1],z[k+1]);

		tmp = radius[k];
		tmp1 = radius[k+1];

		if((tmp > 0)&&(tmp1 > 0)){
			mk_trc_top(stdout,name,base,top,tmp,tmp1);
		}
		else {
			rt_log( "Bad Rod Radius for %s\n",name);
		}

		if( mir_count > 1 && (mir_count % num_unions) == 0 ) {
			(void)mk_addmember(name,&head,WMOP_SUBTRACT);
			proc_region( name );
			(void)mk_addmember(name,&head,WMOP_UNION);
		}
		else {
			(void) mk_addmember(name,&head,WMOP_UNION);
		}

	} /* for */
	if( count > 0 ) {
		proc_region( name );
	}

	last_cc = in[cnt-1].cc;
}

/*
 *  Find the single outward pointing normal for a polygon.
 *  Assumes all points are coplanar (they better be!).
 */
void
pnorms( norms, verts, centroid, npts, inv )
fastf_t	norms[5][3];
fastf_t	verts[5][3];
point_t	centroid;
int	npts;
{
	register int i;
	vect_t	ab, ac;
	vect_t	n,ncent;
	vect_t	out,tmp;

	VSUB2( ab, verts[1], verts[0] );
	VSUB2( ac, verts[2], verts[0] );
	VCROSS( n, ab, ac );
	VUNITIZE( n );

	/*	VSUB2( out, verts[0], centroid );
	if( VDOT( n, out ) < 0 )  {
		VREVERSE( n, n );
	}
*/

	/*	VSUB2( out, centroid,verts[0]);
	VADD3(tmp,verts[0],out,n);
*/

	if ((inv % 2)!= 0)  {
		VREVERSE( n, n );
	}

	/*	for( i=0; i<npts; i++ )  {
		VMOVE( norms[i], n );
	}
*/

	for( i=0; i<npts; i++ )  {
		VMOVE( norms[i], n );
	}
}

/*
 *     This subroutine reads a "group label file" and assembles regions and
 *	groups from that file.
 *
 *	heada == linked list of components on one side or not mirrored
 *	headb == linked list of mirrored components
 *	headd == linked list of this thousand series
 *	heade == linked list of over-all group
 */
void
proc_label(labelfile)
char *labelfile;
{
	char gname[16+1], mgname[16+1];	/* group, mirrored group names */
	char *cc;
	static int cur_series = -1;

	if( cur_series == -1 ) {		/* first time */
		cur_series = in[0].cc / 1000;
		set_color( cur_series );
		proc_label(labelfile);
		return;
	}

	if( cur_series == (in[0].cc / 1000))
	{
		if( labelfile != NULL ) {
			sprintf(gname,"%s", nm[in[0].cc].ug );
			sprintf(mgname,"%s", nm[in[0].cc + in[0].mirror].ug );
		}
		else {
			sprintf(gname,"#%.4d",in[0].cc);
			sprintf(mgname,"#%.4d",(in[0].cc + in[0].mirror));
		}
		if( RT_LIST_NON_EMPTY( &heada.l ) )
		{
			mk_lcomb(stdout,gname,&heada,0,"","",0,0);
			(void) mk_addmember(gname,&headd,WMOP_UNION);
		}
		if( RT_LIST_NON_EMPTY( &headb.l ) )
		{
			mk_lcomb(stdout,mgname,&headb,0,"","",0,0);
			(void) mk_addmember(mgname,&headd,WMOP_UNION);
		}
	}
	else {
		sprintf(gname,"%dxxx_series",cur_series);
		mk_lcomb(stdout,gname,&headd,0,"","",rgb,0);
		(void) mk_addmember(gname,&heade,WMOP_UNION);

		cur_series = in[0].cc/1000 ;
		set_color( cur_series );
		proc_label(labelfile);
	}

}

/*			S E T _ C O L O R
 *
 * Given a color_map entry (for the thousand series) for the combination being
 * made, set the rgb color array for the upcoming call to make combinations.
 */
void
set_color( color )
int color;
{

	switch ( color ){

	case 0:      /* 0XXX_series */

		rgb[0] = 191;
		rgb[1] = 216;
		rgb[2] = 216;

		break;

	case 1:      /* 1XXX_series */

		rgb[0] = 255;
		rgb[1] = 127;
		rgb[2] =   0;

		break;

	case 2:      /* 2XXX_series */

		rgb[0] = 159;
		rgb[1] = 159;
		rgb[2] =  95;

		break;

	case 3:      /* 3XXX_series */

		rgb[0] = 159;
		rgb[1] =  95;
		rgb[2] = 159;

		break;

	case 4:      /* 4XXX_series */

		rgb[0] = 245;
		rgb[1] = 245;
		rgb[2] =   0;

		break;

	case 5:      /* 5XXX_series */

		rgb[0] = 204;
		rgb[1] = 110;
		rgb[2] =  50;

		break;

	case 6:      /* 6XXX_series */

		rgb[0] = 200;
		rgb[1] = 100;
		rgb[2] = 100;

		break;

	case 7:      /* 7XXX_series */

		rgb[0] =  95;
		rgb[1] = 159;
		rgb[2] = 159;

		break;

	case 8:      /* 8XXX_series */

		rgb[0] = 100;
		rgb[1] = 200;
		rgb[2] = 100;

		break;

	case 9:     /* 9XXX_series */

		rgb[0] = 150;
		rgb[1] = 150;
		rgb[2] = 150;

		break;

	default:
		break;
	}
}

/*			I N S I D E _ C Y L
 *
 * Returns 1 if the cylinder starting at in[j] is inside ( for solid
 * subtraction) the cylinder described at in[i], 0 otherwise.
 *
 * This is not a foolproof determination. We only check to see whether the
 * endpoints of the supposed inside cylinder lie within the first cylinder
 * and that the radii of the second cylinder are <= those of the first 
 * cylinder. We don't actually see whether the entire second cylinder lies
 * within the first.
 */
inside_cyl(i,j)
int i,j;
{
	point_t	outbase,outtop,inbase,intop;
	fastf_t	r1,r2;

	r1 = in[i+2].x;
	r2 = in[i+2].y;

	if( (r1 < in[j+2].x) || (r2 < in[j+2].y) )
		return( 0 );

	VSET( outbase, in[i].x,in[i].y,in[i].z );
	VSET( outtop, in[i+1].x,in[i+1].y,in[i+1].z );

	VSET( inbase, in[j].x,in[j].y,in[j].z );
	VSET( intop, in[j+1].x,in[j+1].y,in[j+1].z );

	if( !pt_inside( inbase, outbase, outtop, r1, r2 ) )
		return( 0 );
	else if( !pt_inside( intop, outbase, outtop, r1, r2 ) )
		return( 0 );
	else
		return( 1 );
}

/*			P T _ I N S I D E
 *
 * Returns 1 if point a is inside the cylinder defined by base,top,rad1,rad2.
 * Returns 0 if not.
 */
pt_inside( a,base,top,rad1,rad2 )
point_t a,base,top;
fastf_t rad1,rad2;
{
	vect_t bt,ba;		/* bt: base to top, ba: base to a */
	fastf_t mag_bt,
	    dist,		/* distance to the normal between the axis
				 * and the point
				 */
	radius,		/* radius of cylinder at above distance */
	pt_radsq;	/* sqare of radial distance from the axis 
				 * to point
					 */

	VSUB2( bt, top, base );
	VSUB2( ba, a, base );
	mag_bt = MAGNITUDE( bt );
	VUNITIZE( bt );

	dist = VDOT( bt,ba );
	if( dist < -tol.dist  || dist - mag_bt > tol.dist )
		return( 0 );

	radius = ((rad2 - rad1)*dist)/mag_bt + rad1;

	pt_radsq = MAGSQ(ba) - (dist*dist);
	if( debug>2 && pt_radsq - (radius*radius) < tol.dist_sq  ){
		rt_log( "pt_inside: point (%.4f,%.4f,%.4f) inside cylinder endpoints (%.4f,%.4f,%.4f) and (%.4f,%.4f,%.4f)\n",
		    a[0]/mmtin,a[1]/mmtin,a[2]/mmtin,
		    base[0]/mmtin,base[1]/mmtin,base[2]/mmtin,
		    top[0]/mmtin,top[1]/mmtin,top[2]/mmtin);
		rt_log( "pt_inside: radius at that point is %f\n",radius/mmtin);
		rt_log( "pt_inside: radial distance to point is %f\n",sqrt(pt_radsq)/mmtin );
		rt_log( "pt_inside: square of radial distance is %f\n",pt_radsq/(mmtin*mmtin));
		rt_log( "pt_inside: dist to base to point is %f\n",MAGSQ(ba)/mmtin );
		rt_log( "pt_inside: dist to normal between axis and point is %f\n",dist/mmtin);
	}
	if( pt_radsq - (radius*radius) < tol.dist_sq )
		return( 1 );
	else
		return( 0 );
}



/* 			M K _ C Y L A D D M E M B E R
 *
 * For the cylinder given by 'name1', determine whether it has any
 * volume mode subtractions from it by looking at the subtraction list
 * for this component number. If we find that this cylinder is one
 * of the subtracting cylinders inside, don't do anything. Otherwise,
 * add this cylinder onto the region list along with the subtractions
 * of cylinders determined from the subtraction list. Assume that the
 * subtracted solids will be eventually be made.
 */
void
mk_cyladdmember(name1,head,slist,mirflag)
char *name1;
struct wmember *head;
struct subtract_list *slist;
int mirflag;
{

	char			tmpname[16];
	int			cc,solnum;
	struct subtract_list	*hold;

	if( !slist ) {
		(void)mk_addmember( name1, head, WMOP_UNION );
		return;
	}

	sscanf( name1,"%*[^0-9]%d%*[^0-9]%d", &cc, &solnum );

	/* check to see whether this solid shows up in the subtract 
	 * list as a volume mode solid being subtracted
	 */
	hold = slist;
	while( slist->insolid != solnum && slist->next )
		slist = slist->next;
	if( slist->insolid == solnum )
		return;

	(void)mk_addmember( name1, head, WMOP_UNION );

	for( slist = hold; slist; slist = slist->next ) {
		if( slist->outsolid == solnum ){
			sprintf(tmpname,"c.%.4d.s%.2d",cc,slist->insolid );
			(void)mk_addmember( tmpname, head, WMOP_SUBTRACT );
		}
	}
}




/*			G E T _ S U B T R A C T
 *
 * Make up the list of subtracted volume mode solids for this group of
 * cylinders. Go through the cylinder list and, for each solid, see whether 
 * any of the other solid records following qualify as volume mode subtracted
 * solids. Record the number of the outside cylinder and the number of
 * the inside cylinder in the subtraction list, along with the mirror
 * flag value of the inside solid (for naming convention reasons).
 *
 * Plate mode for a cylinder disqualifies it for any role as a volume mode
 * subtracting cylinder.
 */
struct subtract_list *
get_subtract( cnt )
int cnt;
{
	static struct subtract_list	*slist = NULL;
	struct subtract_list		*next,*add_to_list();
	int i,j;

	/* free up memory for slist, if any */
	for( next=slist ; next; ){
		slist = next;
		next = slist->next;
		rt_free( (char *)slist , "get_subtract: slist" );
	}

	slist = (struct subtract_list *)NULL;
	for( i = 0; i < cnt; i += 3 ) {
		for( j = i + 3; j < cnt ; j += 3 ) {
			if( in[j].surf_mode == '-' )
				continue;
			if( inside_cyl(i,j) )
				slist = add_to_list(slist,(i+3)/3,(j+3)/3,in[j].mirror);
		}
	}
	return( slist );
}


/*			A D D _ T O _ L I S T
 *
 * Add the inside,outside cylinder numbers to the subtraction list slist.
 */
struct subtract_list *
add_to_list( slist,outsolid,insolid,inmirror )
struct subtract_list *slist;
int outsolid,insolid,inmirror;
{

	if( slist == NULL ){
		slist = (struct subtract_list *)rt_malloc(sizeof(struct subtract_list) , "add_to_list: slist" );
		slist->outsolid = outsolid;
		slist->insolid = insolid;
		slist->inmirror = inmirror;
		slist->next = (struct subtract_list *)NULL;
	}
	else
		slist->next = add_to_list(slist->next,outsolid,insolid,inmirror);

	return( slist );
}
