/*
 *			P A T C H - G . C
 *
 *	Converts FASTGEN format target descriptions to MGED format.
 *	  This version assumes the following FASTGEN primitives:
 *		0 - triangle (< 0.99" thick for plate mode)
 *		1 - triangle (< 1.99" thick but > 0.99")
 *		2 - triangle (< 2.99" thick but > 1.99")
 *		3 - triangle (< 3.99" thick but > 2.99")
 *		4 - donut / torus (not supported - new type 3)
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

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "db.h"
#include "vmath.h"
#include "wdb.h"
#include "./patch-g.h"
#include "../rt/mathtab.h"

#if defined(sgi) && !defined(mips)
/* Horrible bug in 3.3.1 and 3.4 and 3.5 -- hypot ruins stack! */
long float
hypot(a,b)
double a,b;
{
	return(sqrt(a*a+b*b));
}
#else
#include <malloc.h>		/* not needed on SGI 3030s */
#endif /* sgi */

void	proc_label();
void	mk_cyladdmember();

static char usage[] = "\
Usage: patch-g [options] > model.g\n\
	-f fastgen.rp	specify pre-processed fastgen file (default stdin)\n\
	-a		process phantom armor?\n\
	-n		process volume mode as plate mode?\n\
	-u #		number of union operations per region (default 5)\n\
	-c \"x y z\"	center of object (for some surface normal calculations)\n\
	-t title	optional title (default \"Untitled MGED database\")\n\
	-o object_name	optional top-level name (no spaces)(default \"all\")\n\
	-i group.file	specify group labels source file\n\
	-m mat.file	specify materials information source file\n\
	-r		reverse normals for plate mode triangles\n\
	-d #		debug level\n\
Note: fastgen.rp is the pre-processed (through rpatch) fastgen file\n";

main(argc,argv)
int	argc;
char	*argv[];
{

	fastf_t  x1, y1, z1;

	int numf = 3;
	int fd,fl,nread;
	FILE	*gfp,*mfp;
	char	buf[99],s[132+2];
	int j = 1;
	int i;
	int done;
	int stop,num;

	char *next, *ptr;
	char *matname = "plastic";
	char *matparm = "sh=100.0,sp=.9,di=.1";
	char hold = 0;

	RT_LIST_INIT( &head.l);
	RT_LIST_INIT( &heada.l);
	RT_LIST_INIT( &headb.l);
	RT_LIST_INIT( &headc.l);
	RT_LIST_INIT( &headd.l);
	RT_LIST_INIT( &heade.l);
	RT_LIST_INIT( &headf.l);

	bzero(list,sizeof(list));

	argc--,argv++;

	if ( isatty(fileno(stdout)) ){
		(void)fputs("attempting to send binary output to tty, aborting!\n",stderr);
		(void)fputs(usage, stderr);
		exit(1);
	}

	/*     This section checks usage options given at run command time.   */

	while (argc > 0 && argv[0][0] == '-') {
		switch (argv[0][1]) {

		case 'f':  /* fastgen source file data */

			argc--,argv++;
			patchfile = *argv;
			break;

		case 'i':  /* group labels source file */

			argc--,argv++;
			labelfile = *argv;
			break;

		case 'm':  /* materials information file */

			argc--,argv++;
			matfile = *argv;
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

			argc--,argv++;
			if( (num_unions = atoi( *argv )) <= 0 ) {
				fprintf(stderr,"%d: bad number of unions to put in a region\n", num_unions );
				exit( 1 );
			}
			break;

		case 't':  /* optional title for the database */

			argc--,argv++;
			title = *argv;
			break;

		case 'o':  /* optional top-level object name */

			argc--,argv++;
			top_level = *argv;
			break;

		case 'r':  /* reverse normals for plate mode triangles */

			rev_norms++;
			break;
		case 'c':  /* center of object (used for some plate mode
			    * triangle surface normal calculations
						    */
			argc--,argv++;
#if defined( sgi ) && ! defined( mips )			
			sscanf( *argv,"%f %f %f", 
			    &Centroid[0],&Centroid[1],&Centroid[2]);
#else
			sscanf( *argv,"%lf %lf %lf", 
			    &Centroid[0],&Centroid[1],&Centroid[2]);
#endif
			VSCALE( Centroid, Centroid, mmtin );
			break;

		case 'd':  /* debug flag checking */

			argc--,argv++;
			debug = atoi(*argv); /* Debug level */
			fprintf(stderr,"Debug = %d\n",debug);
			break;

		default:
			(void)fputs(usage, stderr);
			exit(1);
		}
		argc--, argv++;

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

	mk_id(stdout,title);

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

				fprintf(stderr,"Incomplete line in materials file for component '%.4d'\n",i);
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

#if defined( sgi ) && ! defined( mips )
			sscanf(buf,"%f %f %f %c %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			    &in[i].x,&in[i].y,&in[i].z,&in[i].surf_mode,&in[i].surf_type,
			    &in[i].surf_thick,&in[i].spacecode, &in[i].cc,
			    &in[i].ept[0],&in[i].ept[1],&in[i].ept[2],
			    &in[i].ept[3],&in[i].ept[4],&in[i].ept[5],
			    &in[i].ept[6],&in[i].ept[7],&in[i].mirror,&in[i].vc);
#else
			sscanf(buf,"%lf %lf %lf %c %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
			    &in[i].x,&in[i].y,&in[i].z,&in[i].surf_mode,&in[i].surf_type,
			    &in[i].surf_thick,&in[i].spacecode, &in[i].cc,
			    &in[i].ept[0],&in[i].ept[1],&in[i].ept[2],
			    &in[i].ept[3],&in[i].ept[4],&in[i].ept[5],
			    &in[i].ept[6],&in[i].ept[7],&in[i].mirror,&in[i].vc);
#endif

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
				fprintf(stderr,"%lf %lf %lf %c %lf %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
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

			if( debug > 2 ) {
				for( j=0; j<i; j++ )
					fprintf(stderr,"IN: %f %f %f\n",in[j].x,in[j].y,in[j].z);
			}

			switch(in[i-1].surf_type){    /* Key on surface types. */

			case 0:  	/* triangle approximation */
			case 1:  	/* triangle approximation (thickness + 1") */
			case 2:  	/* triangle approximation (thickness + 2") */
			case 3:  	/* triangle approximation (thickness + 3") */

				if ((nflg > 0)&&(in[i-1].surf_mode== '+')){
					proc_triangle(i);
				}
				else{
					proc_plate(i);
				}
				break;

			case 4: 	/* new "donut/torus" (not processed) */
				fprintf(stderr,"component %.4d: donut / torus not implemented\n",in[i-1].cc);
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
				fprintf(stderr,"component %.4d: unknown solid type %d\n",
				    in[i-1].cc,in[i-1].surf_type);
				break;

			}       /* end switch */

			/* If the component code number has changed, call
			   the subroutine for making groups from regions.   */

			if( (in[i].cc != in[i-1].cc) && (in[i].cc != 0) ) {
				proc_label(name,labelfile);
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

		}       /* end "processing" if */
	}

	sprintf(name,"%s",top_level);
	mk_lcomb(stdout,name,&heade,0,"","",0,0);

	/*	if( headf.forw != &headf.l ) {
		sprintf(name,"check.group");
		mk_lcomb(stdout,name,&headf,0,"","",0,0);
	}
*/
	if( RT_LIST_NEXT_NOT_HEAD(&headf, &headf.l )) {
		sprintf(name,"check.group");
		mk_lcomb(stdout,name,&headf,0,"","",0,0);
	}


}	/* END MAIN PROGRAM  */


/*  Throughout these procedures, the following naming conventions are used.
 *   For unique id# solids:	surface type code . component code number . "s"count
 *   For "neg" mirror solids:	side identifier & surface type code . component code number . "s"count
 *   For internal solids:	external name format, replacing "s" with "c" (for: cut out)
 *   For regions:		solid name with "s cnt" replaced by "r cnt"
 *   For groups:		text nomenclature identification from labelfile
 */


/*
 *	 Process Volume Mode triangular facetted solids  
 */
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

	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count=0;
	}

	/* assign solid's name */

	shflg = 'f';
	mrflg = 'n';
	ctflg = 'n';
	proc_sname (shflg,mrflg,count+1,ctflg);

	/* make solid */

	mk_polysolid(stdout,name);
	count++;

	(void) mk_addmember(name,&head,WMOP_UNION);


	for(k=0 ; k < (cnt) ; k++){
		for(l=0; l<= 7; l++){

			if(in[k].ept[l] > 0){

				index = in[k].ept[l];
				list[index].x = in[k].x;
				list[index].y = in[k].y;
				list[index].z = in[k].z;
				list[index].flag = 1;

				if (debug > 3)
					fprintf(stderr,"%d %f %f %f\n",list[index].flag,in[k].x,in[k].y,in[k].z);
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
			fprintf(stderr,"k=%d l=%d %f %f %f flag=%d\n",k,l,list[k].x,list[k].y,list[k].z,list[k].flag);
	}

	if (debug > 2){
		for (k=1;(k<=l);k++)
			fprintf(stderr,"%d %f %f %f\n",k,x[k],y[k],z[k]);
	}

	for(k=2; k<=4; k++){
		VSET(pt[k-2],x[k],y[k],z[k]);
	}

	VMOVE(vertice[0],pt[0]);
	VMOVE(vertice[1],pt[1]);
	VMOVE(vertice[2],pt[2]);

	for ( k=0; k<3; k++ )
		centroid[k] = 0.0;
	for ( cpts=0, k=1; k<l; k++ ) {
		point_t last, tmp;

		VSET( tmp, x[k], y[k], z[k] );
		if( VEQUAL( tmp, last ) )
			continue;
		VADD2( centroid, centroid, tmp );
		VMOVE( last, tmp );
		cpts++;
	}
	VSCALE( centroid, centroid, 1.0/cpts );
	if( debug > 2 ) {
		fprintf(stderr,"%d: cpts=%d centroid %f %f %f\n",
		    in[0].cc, cpts, 
		    centroid[0], centroid[1], centroid[2] );
	}


	for (k=5;(k<l); k++){
		if(   VEQUAL(vertice[0],vertice[1])
		    ||VEQUAL(vertice[0],vertice[2])
		    ||VEQUAL(vertice[1],vertice[2])){

			if ((k % 2)== 0){

				VMOVE(vertice[1],vertice[2]);
				VSET(pt[0],x[k],y[k],z[k]);
				VMOVE(vertice[2],pt[0]);
			}
			else{

				VMOVE(vertice[0],vertice[2]);
				VSET(pt[0],x[k],y[k],z[k]);
				VMOVE(vertice[2],pt[0]);

			}

			/* fprintf(stderr,"Repeated Vertice, PUNTING\n"); */
		}
		else{

			/* VADD3(centroid,vertice[0],vertice[1],vertice[2]);
			 * VSCALE(centroid,centroid, third);
			 * 
			 * pnorms(normal,vertice,centroid,3,k);
			 */

			VSUB2(ab,vertice[1],vertice[0]);
			VSUB2(bc,vertice[2],vertice[1]);
			VCROSS(norm,ab,bc);
			VUNITIZE(norm);
			VSUB2( out, vertice[0], centroid );
			if( VDOT( out, norm ) < 0.0 ) {
				VREVERSE( norm, norm );
			}

/* Use same normal for all vertices (flat shading) */  
			VMOVE(normal[0],norm);
			VMOVE(normal[1],norm);
			VMOVE(normal[2],norm);
			mk_poly(stdout,3,vertice,normal);
			

			if ((k % 2)== 0){

				VMOVE(vertice[1],vertice[2]);
				VSET(pt[0],x[k],y[k],z[k]);
				VMOVE(vertice[2],pt[0]);
			}
			else{

				VMOVE(vertice[0],vertice[2]);
				VSET(pt[0],x[k],y[k],z[k]);
				VMOVE(vertice[2],pt[0]);

			}
		}
	}

	/* Write out as region */

	proc_region(name);

	/* Process the mirrored surface ! (duplicates previous code) */

	if(in[0].mirror != 0){

	  mrflg = 'y';
	  ctflg = 'n';
	  proc_sname (shflg,mrflg,mir_count+1,ctflg);

		mk_polysolid(stdout,name);
		mir_count++;

		(void) mk_addmember(name,&head,WMOP_UNION);


		for(k=2; k<=4; k++){

			VSET(pt[k-2],x[k],-y[k],z[k]);
		}

		VMOVE(vertice[0],pt[0]);
		VMOVE(vertice[1],pt[1]);
		VMOVE(vertice[2],pt[2]);

		for ( k=0; k<3; k++ )
			centroid[k] = 0.0;
		for ( cpts=0, k=1; k<l; k++ ) {
			point_t last, tmp;

			VSET( tmp, x[k], -y[k], z[k] );
			if( VEQUAL( tmp, last ) )
				continue;
			VADD2( centroid, centroid, tmp );
			VMOVE( last, tmp );
			cpts++;
		}
		VSCALE( centroid, centroid, 1.0/cpts );
		if( debug > 2 ) {
			fprintf(stderr,"%d: cpts=%d centroid %f %f %f\n",
			    in[0].cc+in[0].mirror, cpts, 
			    centroid[0], centroid[1], centroid[2] );
		}

		for (k=5; k<(l); k++){

			if(VEQUAL(vertice[0],vertice[1])
			    ||VEQUAL(vertice[0],vertice[2])
			    ||VEQUAL(vertice[1],vertice[2])){

				if ((k % 2)== 0){

					VMOVE(vertice[1],vertice[2]);
					VSET(pt[0],x[k],-y[k],z[k]);
					VMOVE(vertice[2],pt[0]);


				}
				else{

					VMOVE(vertice[0],vertice[2]);
					VSET(pt[0],x[k],-y[k],z[k]);
					VMOVE(vertice[2],pt[0]);


				}

				/* fprintf(stderr,"Repeated Vertice, PUNTING\n");*/
			}
			else{

				/* VADD3(centroid,vertice[0],vertice[1],vertice[2]);
				 * VSCALE(centroid,centroid, third);
				 *
				 * pnorms(normal,vertice,centroid,3,k);
				 */

				VSUB2(ab,vertice[1],vertice[0]);
				VSUB2(bc,vertice[2],vertice[1]);
				VCROSS(norm,ab,bc);
				VUNITIZE(norm);
				VSUB2( out, vertice[0], centroid );
				if( VDOT( out, norm ) < 0.0 ) {
					VREVERSE( norm, norm );
				}

/* Use same normal for all vertices (flat shading) */  
				VMOVE(normal[0],norm);
				VMOVE(normal[1],norm);
				VMOVE(normal[2],norm);
				mk_poly(stdout,3,vertice,normal);


				if ((k % 2)== 0){

					VMOVE(vertice[1],vertice[2]);
					VSET(pt[0],x[k],-y[k],z[k]);
					VMOVE(vertice[2],pt[0]);

				}
				else{
					VMOVE(vertice[0],vertice[2]);
					VSET(pt[0],x[k],-y[k],z[k]);
					VMOVE(vertice[2],pt[0]);


				}
			}

		}
		proc_region(name);

	}/* if */
	last_cc = in[cnt-1].cc;
}

/*
 *	 Process Plate Mode triangular surfaces 
 */
proc_plate(cnt)
int cnt;
{
	point_t arb6pt[8];
	vect_t	out;
	vect_t	norm;
	vect_t	ab,bc,ca,ac;
	fastf_t ndot,outdot;
	int k,l;
	int index;
	static int count=0;
	static int mir_count=0;
	static int last_cc=0;
	int cpts;
	char	shflg,mrflg,ctflg;

	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count = 0;
	}

	/* include the check for phantom armor */
	if ((in[0].rsurf_thick > 0)||(aflg > 0)) {

		for(k=0 ; k < (cnt) ; k++){
			for(l=0; l<= 7; l++){
				if(in[k].ept[l] > 0){
					index = in[k].ept[l];

					/*				fprintf(stderr,"index = %d\n",index); */
					list[index].x = in[k].x;
					list[index].y = in[k].y;
					list[index].z = in[k].z;
					list[index].thick = in[k].rsurf_thick;
					list[index].flag = 1;
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

				l= l+1;
			}
		}

		for(k=1; k<=3; k++){
			VSET(pt[k-1],x[k],y[k],z[k]);
		}

		VMOVE(vertice[0],pt[0]);
		VMOVE(vertice[1],pt[1]);
		VMOVE(vertice[2],pt[2]);

		if( debug > 2 ) {
			for ( k=1;k<l; k++ )
				fprintf(stderr,"Compressed: %f %f %f\n",x[k],y[k],z[k]);
		}


		for ( k=0; k<3; k++ )
			centroid[k] = 0.0;
		for ( cpts=0, k=1; k<l; k++ ) {
			point_t last, tmp;

			VSET( tmp, x[k], y[k], z[k] );
			if( VEQUAL( tmp, last ) )
				continue;
			VADD2( centroid, centroid, tmp );
			VMOVE( last, tmp );
			cpts++;
		}
		VSCALE( centroid, centroid, 1.0/cpts );
		if( debug > 2 ) {
			fprintf(stderr,"%d: cpts=%d centroid %f %f %f\n",
			    in[0].cc, cpts, 
			    centroid[0], centroid[1], centroid[2] );
		}


		for (k=4;(k<l); k++){

			VSUB2(ab,vertice[1],vertice[0]);
			VSUB2(bc,vertice[2],vertice[1]);
			ndot = VDOT( ab, bc )/(MAGNITUDE(ab) * MAGNITUDE(bc));

			if(VEQUAL(vertice[0],vertice[1])
			    ||VEQUAL(vertice[0],vertice[2])
			    ||VEQUAL(vertice[1],vertice[2])){

				;	/* do nothing */
				/* fprintf(stderr,"Repeated Vertice, PUNT\n"); */
			}
			else if( ndot >= 0.999999 || ndot <= -0.999999 ) {

				;	/* do nothing */
				/* fprintf(stderr,"%s: collinear points, not made.\n", name); */

			}
			else {

				VSUB2(ab,vertice[1],vertice[0]);
				VSUB2(bc,vertice[2],vertice[1]);
				VSUB2(ca,vertice[0],vertice[2]);
				VSUB2(ac,vertice[0],vertice[2]);

				/* Plate Mode */

				VMOVE(arb6pt[0],vertice[0]);
				VMOVE(arb6pt[1],vertice[1]);
				VMOVE(arb6pt[2],vertice[2]);
				VMOVE(arb6pt[3],vertice[2]);

				VCROSS(norm,ab,ac);
				VUNITIZE(norm);

				VSUB2( out, vertice[0], centroid );
				VUNITIZE( out );
				outdot = VDOT( out, norm );
				if( debug > 2 )
					fprintf(stderr,
					    "%d: solid %d, unitized outward normal dot product is %f\n",
					    in[0].cc, count+1, outdot );

				/* flat plate, use center of description */
				if( outdot <= 0.001 &&  outdot >= -0.001 ) {
					if( debug > 0 )
						fprintf(stderr,
						    "%d: solid %d, using optional Centroid\n", 
						    in[0].cc, count+1 );
					VSUB2( out, vertice[0], Centroid );
					VUNITIZE( out );
					outdot = VDOT( out, norm );
				}

				if( outdot > 0.0 ) {
					VREVERSE( norm, norm );
				}

				if( rev_norms ){
					VREVERSE( norm, norm );
				}

				if (aflg > 0) {
					if (in[0].rsurf_thick == 0) {
						in[0].rsurf_thick = 1;
					}
				}

				VSCALE(norm,norm,thk[k-1]);

				VADD2(arb6pt[4],norm,arb6pt[0]);
				VADD2(arb6pt[5],norm,arb6pt[1]);
				VADD2(arb6pt[6],norm,arb6pt[2]);
				VMOVE(arb6pt[7],arb6pt[6]);

				/* name solids */

				shflg = 't';
				mrflg = 'n';
				ctflg = 'n';
				proc_sname (shflg,mrflg,count+1,ctflg);

				/* make solids */

				mk_arb8(stdout,name,arb6pt);
				count++;

				(void) mk_addmember(name,&head,WMOP_UNION);

				/* For every num_unions triangles, make a separate region */

				if ((count % num_unions) == 0)
					proc_region(name);


			}

			VMOVE(vertice[0],vertice[1]);
			VMOVE(vertice[1],vertice[2]);
			VSET(pt[0],x[k],y[k],z[k]);
			VMOVE(vertice[2],pt[0]);

		}
		/* Make a region for leftover triangles (<num_unions) */

		if ((count % num_unions) != 0)
			proc_region(name);


		if(in[0].mirror != 0){			/* Mirror Processing! */

			for(k=1; k<=3; k++){
				VSET(pt[k-1],x[k],-y[k],z[k]);
			}

			VMOVE(vertice[0],pt[0]);
			VMOVE(vertice[1],pt[1]);
			VMOVE(vertice[2],pt[2]);

			for ( k=0; k<3; k++ )
				centroid[k] = 0.0;
			for ( cpts=0, k=1; k<l; k++ ) {
				point_t last, tmp;

				VSET( tmp, x[k], -y[k], z[k] );
				if( VEQUAL( tmp, last ) )
					continue;
				VADD2( centroid, centroid, tmp );
				VMOVE( last,tmp );
				cpts++;
			}
			VSCALE( centroid, centroid, 1.0/cpts );
			if( debug > 2 ) {
				fprintf(stderr,"%d: cpts=%d centroid %f %f %f\n",
				    in[0].cc + in[0].mirror, cpts, 
				    centroid[0], centroid[1], centroid[2] );
			}


			for (k=4; k<(l); k++){

				VSUB2(ab,vertice[1],vertice[0]);
				VSUB2(bc,vertice[2],vertice[1]);
				ndot = VDOT( ab, bc )/(MAGNITUDE(ab) * MAGNITUDE(bc));

				if(VEQUAL(vertice[0],vertice[1])
				    ||VEQUAL(vertice[0],vertice[2])
				    ||VEQUAL(vertice[1],vertice[2])){

					;	/* do nothing */
					/* fprintf(stderr,"Repeated Vertice, PUNT\n");*/
				}
				else if( ndot >= 0.999999 || ndot <= -0.999999 ) {

					;	/* do nothing */
					/* fprintf(stderr,"%s: collinear points, not made.\n", name); */

				}
				else {

					VSUB2(ab,vertice[1],vertice[0]);
					VSUB2(bc,vertice[2],vertice[1]);
					VSUB2(ca,vertice[0],vertice[2]);
					VSUB2(ac,vertice[0],vertice[2]);

					VMOVE(arb6pt[0],vertice[0]);
					VMOVE(arb6pt[1],vertice[1]);
					VMOVE(arb6pt[2],vertice[2]);
					VMOVE(arb6pt[3],vertice[2]);

					VCROSS(norm,ac,ab);    /* Reversed normals */
					/* in mirror case */
					VUNITIZE(norm);

					VSUB2( out, vertice[0], centroid );
					VUNITIZE( out );
					outdot = VDOT( out, norm );
					if( debug > 2 )
						fprintf(stderr,
						    "%d: solid %d, unitized outward normal dot product is %f\n",
						    in[0].cc+in[0].mirror, mir_count+1, outdot );

					/* flat plate, use center of description */
					if( outdot <= 0.001 &&  outdot >= -0.001 ) {
						if( debug > 0 )
							fprintf(stderr,
							    "%d: solid %d, using optional Centroid\n", 
							    in[0].cc+in[0].mirror, mir_count+1 );
						VSUB2( out, vertice[0], Centroid );
						VUNITIZE( out );
						outdot = VDOT( out, norm );
					}

					if( outdot > 0.0 ) {
						VREVERSE( norm, norm );
					}

					if( rev_norms ){
						VREVERSE( norm, norm );
					}

					VSCALE(norm,norm,thk[k-1]);

					VADD2(arb6pt[4],norm,arb6pt[0]);
					VADD2(arb6pt[5],norm,arb6pt[1]);
					VADD2(arb6pt[6],norm,arb6pt[2]);
					VMOVE(arb6pt[7],arb6pt[6]);


					mrflg = 'y';
					ctflg = 'n';
					proc_sname (shflg,mrflg,mir_count+1,ctflg);

					mk_arb8(stdout,name,arb6pt);
					mir_count++;

					(void) mk_addmember(name,&head,WMOP_UNION);



					if ((mir_count % num_unions) == 0)
						proc_region(name);


				}

				VMOVE(vertice[0],vertice[1]);
				VMOVE(vertice[1],vertice[2]);
				VSET(pt[0],x[k],-y[k],z[k]);
				VMOVE(vertice[2],pt[0]);

			}
			if ((mir_count % num_unions) != 0)
				proc_region(name);

		}/* if mirror */
	} /* phantom armor check */

	last_cc = in[cnt-1].cc;
}

/* 
 *	Process fastgen wedge shape - this does not process hollows
 */
proc_wedge(cnt)
int cnt;
{
	point_t	pts[1];
	point_t	pt8[8];
	int k,l;
	vect_t	ab, ac , ad;
	vect_t	v1,v2,v3,v4;
	static int count=0;
	static int mir_count=0;
	static int last_cc=0;
	char	shflg,mrflg,ctflg;

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

		VADD3(pt8[5],ab,ad,pt8[0]);
		VADD3(pt8[7],ab,ac,pt8[0]);

		VMOVE(pt8[6],pt8[5]);
		VMOVE(pt8[2],pt8[1]);

		/* name solids */

		shflg = 'w';
		mrflg = 'n';
		ctflg = 'n';
		proc_sname (shflg,mrflg,count+1,ctflg);

		/* make solids */

		mk_arb8( stdout, name, pt8 );
		count++;

		(void) mk_addmember(name,&head,WMOP_UNION);

		/* make regions for every num_unions solids */

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


		VADD3(pt8[5],ab,ad,pt8[0]);
		VADD3(pt8[7],ab,ac,pt8[0]);

		VMOVE(pt8[6],pt8[5]);
		VMOVE(pt8[2],pt8[1]);

		mrflg = 'y';
		ctflg = 'n';
		proc_sname (shflg,mrflg,mir_count+1,ctflg);

		mk_arb8( stdout, name, pt8 );
		mir_count++;

		(void) mk_addmember(name,&head,WMOP_UNION);

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
		proc_sname (shflg,mrflg,count+1,ctflg);
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
				proc_sname (shflg,mrflg,count,ctflg);

				/* make inside solid */

				if( (rad = in[i+1].x - in[i].rsurf_thick) > 0.0 ) {
					mk_sph(stdout,name,center,rad);
					mk_addmember(name, &head,WMOP_SUBTRACT);
				}
				else {
					/* add to check group */
					mk_addmember(name, &headf,WMOP_UNION);
				}
			}

			if( (count % num_unions) == 0 )
				proc_region(name);
		}
		else {
			fprintf(stderr,"Bad component %s\n",name);
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
		proc_sname (shflg,mrflg,mir_count+1,ctflg);

		VSET(center,in[i].x,-in[i].y,in[i].z);

		if( in[i+1].x > 0.0 ) {
			mk_sph(stdout,name,center,in[i+1].x);
			mir_count++;

			(void) mk_addmember(name,&head,WMOP_UNION);

			/* Check if mirrored surface is hollow (i.e. plate mode) subract
				sphere with radius R1 - thickness */

			if (in[i].surf_mode== '-'){

				ctflg = 'y';
				proc_sname (shflg,mrflg,mir_count,ctflg);

				if( (rad = in[i+1].x - in[i].rsurf_thick) > 0.0 ) {
					mk_sph(stdout,name,center,rad);
					mk_addmember(name, &head,WMOP_SUBTRACT);
				}
				else {
					/* add to check group */
					mk_addmember(name, &headf,WMOP_UNION);
				}
			}

			if( (mir_count % num_unions) == 0 )
				proc_region(name);
		}
		else {
			fprintf(stderr,"Bad component %s\n",name);
		}

	}

	if( (count % num_unions) != 0 )
		proc_region(name);

	last_cc = in[cnt-1].cc;
}

/*
 *	Process fastgen box code
 */
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
		proc_sname (shflg,mrflg,count+1,ctflg);

		/* make solid */

		mk_arb8( stdout, name, pt8 );
		count++;

		(void) mk_addmember(name,&head,WMOP_UNION);

		if( in[k].surf_mode == '-' ){

			ctflg = 'y';
			proc_sname (shflg,mrflg,count,ctflg);

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

				mk_arb8( stdout, name, pt8 );
				mk_addmember(name,&head,WMOP_SUBTRACT);
			}
			else {
				/* add to check group */
				mk_addmember(name,&headf,WMOP_UNION);
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
		proc_sname (shflg,mrflg,mir_count+1,ctflg);

		mk_arb8( stdout, name, pt8 );
		mir_count++;

		(void) mk_addmember(name,&head,WMOP_UNION);

		if( in[k].surf_mode == '-' ){

			ctflg = 'y';
			proc_sname (shflg,mrflg,mir_count+1,ctflg);

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

				mk_arb8( stdout, name, pt8 );
				mk_addmember(name,&head,WMOP_SUBTRACT);
			}
			else {
				/* add to check group */
				mk_addmember(name,&headf,WMOP_UNION);
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

	if( in[cnt-1].cc != last_cc )
	{
		count = 0;
		mir_count=0;
	}


	slist = get_subtract(cnt);
	if( debug>2 ){
		struct subtract_list *sp;

		for( sp=slist; sp; sp=sp->next )
			fprintf(stderr,"%d %d %d\n", 
			    sp->outsolid,sp->insolid,sp->inmirror );
	}


	for(k=0 ; k < (cnt-1) ; k+=3){	 /* For all sub-cylinders in this cc */

		/* name solids */
		shflg = 'c';
		mrflg = 'n';
		ctflg = 'n';
		proc_sname (shflg,mrflg,count+1,ctflg);

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
				ctflg = 'y';
				proc_sname (shflg,mrflg,count,ctflg);

				switch(j){

				case 0: /* Both ends open */

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,top,base);
					ht = MAGNITUDE( ab );
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = rad1 - thick;
					srad2 = rad2 - thick;

					if( srad1 > 0.0 && srad2 > 0.0 ) {
						mk_trc_top(stdout,name,base,top,srad1,srad2);
						mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else {
						mk_addmember(name,&headf,WMOP_UNION);
					}
					break;

				case 1: /* Base closed, top open */

					VSUB2(ab,top,base);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
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

					if( srad1 > 0.0 && srad2 > 0.0 && !VEQUAL(sbase,top) ) {
						mk_trc_top(stdout,name,sbase,top,srad1,srad2);
						mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else {
						mk_addmember(name,&headf,WMOP_UNION);
					}
					break;
				case 2: /* Base open, top closed */

					VSUB2(ab,base,top);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
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

					if( srad1 > 0.0 && srad2 > 0.0 && !VEQUAL(base,stop) ) {
						mk_trc_top(stdout,name,base,stop,srad1,srad2);
						mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else {
						mk_addmember(name,&headf,WMOP_UNION);
					}
					break;

				case 3: /* Both closed */

					VSUB2(ab,top,base);
					VSUB2(bc,base,top);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
					VUNITIZE(bc);
					VSCALE(ab,ab,in[k].rsurf_thick);
					VSCALE(bc,bc,in[k].rsurf_thick);
					VADD2(sbase,base,ab);
					VADD2(stop,top,bc);

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,stop,base);
					sht = MAGNITUDE( ab );
					srad1 = rad2 - sht / ht * (rad2 - rad1);
					srad2 = rad1 - sht / ht * (rad1 - rad2);
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = srad1 - thick;
					srad2 = srad2 - thick;

					if( srad1 > 0.0 && srad2 > 0.0 && !VEQUAL(sbase,stop) ) {
						mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
						mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else {
						mk_addmember(name,&headf,WMOP_UNION);
					}
					break;

				default:
					fprintf(stderr,"Unknown cylinder mode\n");
					break;

				}         /* end switch */
			}     		  /* end - plate mode modifications */
		}         			  /* Degenerate length check */
		else {
			fprintf(stderr,"Bad Cylinder Length for %s\n",name);
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
		proc_sname (shflg,mrflg,mir_count+1,ctflg);
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
				ctflg = 'y';
				proc_sname (shflg,mrflg,mir_count,ctflg);

				switch(j){

				case 0: /* Both ends open */

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,top,base);
					ht = MAGNITUDE( ab );
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = rad1 - thick;
					srad2 = rad2 - thick;

					if( srad1 > 0.0 && srad2 > 0.0 ) {
						mk_trc_top(stdout,name,base,top,srad1,srad2);
						mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else {
						mk_addmember(name,&headf,WMOP_UNION);
					}
					break;

				case 1: /* Base closed, top open */

					VSUB2(ab,top,base);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
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

					if( srad1 > 0.0 && srad2 > 0.0 && !VEQUAL(sbase,top) ) {
						mk_trc_top(stdout,name,sbase,top,srad1,srad2);
						mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else {
						mk_addmember(name,&headf,WMOP_UNION);
					}

					break;

				case 2: /* Base open, top closed */

					VSUB2(ab,base,top);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
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

					if( srad1 > 0.0 && srad2 > 0.0 && !VEQUAL(base,stop) ) {
						mk_trc_top(stdout,name,base,stop,srad1,srad2);
						mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else {
						mk_addmember(name,&headf,WMOP_UNION);
					}
					break;

				case 3: /* Both closed */

					VSUB2(ab,top,base);
					VSUB2(bc,base,top);
					ht = MAGNITUDE( ab );
					VUNITIZE(ab);
					VUNITIZE(bc);
					VSCALE(ab,ab,in[k].rsurf_thick);
					VSCALE(bc,bc,in[k].rsurf_thick);
					VADD2(sbase,base,ab);
					VADD2(stop,top,bc);

					rad1 = in[k+2].x;
					rad2 = in[k+2].y;
					VSUB2(ab,stop,base);
					sht = MAGNITUDE( ab );
					srad1 = rad2 - sht / ht * (rad2 - rad1);
					srad2 = rad1 - sht / ht * (rad1 - rad2);
					thick = in[k+2].rsurf_thick / ht * 
					    hypot( ht, rad2-rad1 );
					srad1 = srad1 - thick;
					srad2 = srad2 - thick;

					if( srad1 > 0.0 && srad2 > 0.0 && !VEQUAL(sbase,stop) ) {
						mk_trc_top(stdout,name,sbase,stop,srad1,srad2);
						mk_addmember(name,&head,WMOP_SUBTRACT);
					}
					else {
						mk_addmember(name,&headf,WMOP_UNION);
					}
					break;

				default:
					fprintf(stderr,"Unknown cylinder mode\n");
					break;
				}/* switch */
			}/* plate mode */
		}
		else {
			fprintf(stderr,"Bad Cylinder Length for %s\n",name);
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
					fprintf(stderr,"%d %f %f %f %f %d\n",list[index].flag,in[k].x,in[k].y,in[k].z,in[k].rsurf_thick,
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
				fprintf(stderr,"k=%d l=%d %f %f %f %f %d flag=%d\n",
				    k,l,list[k].x,list[k].y,list[k].z,
				    list[k].flag,list[k].radius,list[k].mirror);
		}
	}

	if (debug > 2){
		for (k=1;(k<=l);k++)
			fprintf(stderr,"compressed: %d %f %f %f %f %d\n",
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
		proc_sname (shflg,mrflg,count+1,ctflg);

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
			fprintf(stderr,"Bad Rod Radius for %s\n",name);
		}

		if( count > 1 && (count % num_unions) == 0 ){
			mk_addmember(name,&head,WMOP_SUBTRACT);
			proc_region( name );
			mk_addmember(name,&head,WMOP_UNION);
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
		proc_sname (shflg,mrflg,mir_count+1,ctflg);

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
			fprintf(stderr,"Bad Rod Radius for %s\n",name);
		}

		if( mir_count > 1 && (mir_count % num_unions) == 0 ) {
			mk_addmember(name,&head,WMOP_SUBTRACT);
			proc_region( name );
			mk_addmember(name,&head,WMOP_UNION);
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
 *     This subroutine generates solid names with annotations for sidedness as
 *      required.
 */
proc_sname(shflg,mrflg,cnt,ctflg)
char shflg,mrflg,ctflg;
int  cnt;
{
	char side;

	/* shflg == identifies shape process which called this function
	 * mrflg == indicates called by "as-modeled" pass or mirrored pass
	 * cnt   == suffix indentifier for the name
	 * ctflg == isolates internal cutting solids from regular solids
	 * name == solid name
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
	    sprintf(name,"%c.%.4d.s%.2d",shflg,in[0].cc,cnt);
	  }
	  else if ((mrflg == 'n') && (ctflg == 'y')) {
	    sprintf(name,"%c.%.4d.c%.2d",shflg,in[0].cc,cnt);
	  }
	  else if ((mrflg == 'y') && (ctflg == 'n')) {
	    sprintf(name,"%c.%.4d.s%.2d",shflg,(in[0].cc+in[0].mirror),cnt);
	  }
	  else {
	    sprintf(name,"%c.%.4d.c%.2d",shflg,(in[0].cc+in[0].mirror),cnt);
	  }
	}
	else if (ctflg == 'n') {
	  sprintf(name,"%c%c.%.4d.s%.2d",side,shflg,in[0].cc,cnt);
	}
	else {
	  sprintf(name,"%c%c.%.4d.c%.2d",side,shflg,in[0].cc,cnt);
	}
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
 *     This subroutine reads a "group label file" and assembles regions and
 *	groups from that file.
 *
 *	heada == linked list of components on one side or not mirrored
 *	headb == linked list of mirrored components
 *	headd == linked list of this thousand series
 *	heade == linked list of over-all group
 */
void
proc_label(name1,labelfile)
char *name1;
char *labelfile;
{
	char gname[16+1], mgname[16+1];	/* group, mirrored group names */
	char *cc;
	static int cur_series = -1;

	if( cur_series == -1 ) {		/* first time */
		cur_series = in[0].cc / 1000;
		set_color( cur_series );
		proc_label(name1,labelfile);
		return;
	}

	if( cur_series == (in[0].cc / 1000)){

		while( *name1++ != '.' )
			;
		cc = name1;

		if ((atoi(cc)) == in[0].cc){

			/* no mirror components */

			if( labelfile != NULL )
				sprintf(gname,"%s", nm[in[0].cc].ug );
			else
				sprintf(gname,"#%.4d",in[0].cc);
			mk_lcomb(stdout,gname,&heada,0,"","",0,0);
			(void) mk_addmember(gname,&headd,WMOP_UNION);
		}
		else{
			/* mirrored components */

			if( labelfile != NULL ) {
				sprintf(gname,"%s", nm[in[0].cc].ug );
				sprintf(mgname,"%s", nm[in[0].cc + in[0].mirror].ug );
			}
			else {
				sprintf(gname,"#%.4d",in[0].cc);
				sprintf(mgname,"#%.4d",(in[0].cc + in[0].mirror));
			}
			mk_lcomb(stdout,gname,&heada,0,"","",0,0);
			mk_lcomb(stdout,mgname,&headb,0,"","",0,0);
			(void) mk_addmember(gname,&headd,WMOP_UNION);
			(void) mk_addmember(mgname,&headd,WMOP_UNION);
		}
	}
	else {
		sprintf(gname,"%dxxx_series",cur_series);
		mk_lcomb(stdout,gname,&headd,0,"","",rgb,0);
		(void) mk_addmember(gname,&heade,WMOP_UNION);

		cur_series = in[0].cc/1000 ;
		set_color( cur_series );
		proc_label(name1,labelfile);
	}

}

/*			S E T _ C O L O R
 *
 * Given a color_map entry (for the thousand series) for the combination being
 * made, set the rgb color array for the upcoming call to make combinations.
 */
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
	if( dist < 0.0  || dist > mag_bt )
		return( 0 );

	radius = ((rad2 - rad1)*dist)/mag_bt + rad2;

	pt_radsq = MAGSQ(ba) - (dist*dist);
	if( debug>2 && pt_radsq < (radius*radius)  ){
		fprintf(stderr,"pt_inside: point (%.4f,%.4f,%.4f) inside cylinder endpoints (%.4f,%.4f,%.4f) and (%.4f,%.4f,%.4f)\n",
		    a[0]/mmtin,a[1]/mmtin,a[2]/mmtin,
		    base[0]/mmtin,base[1]/mmtin,base[2]/mmtin,
		    top[0]/mmtin,top[1]/mmtin,top[2]/mmtin);
		fprintf(stderr,"pt_inside: radius at that point is %f\n",radius/mmtin);
		fprintf(stderr,"pt_inside: radial distance to point is %f\n",sqrt(pt_radsq)/mmtin );
		fprintf(stderr,"pt_inside: square of radial distance is %f\n",pt_radsq/(mmtin*mmtin));
		fprintf(stderr,"pt_inside: dist to base to point is %f\n",MAGSQ(ba)/mmtin );
		fprintf(stderr,"pt_inside: dist to normal between axis and point is %f\n",dist/mmtin);
	}
	if( pt_radsq < (radius*radius) )
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
		mk_addmember( name1, head, WMOP_UNION );
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

	mk_addmember( name1, head, WMOP_UNION );

	for( slist = hold; slist; slist = slist->next ) {
		if( slist->outsolid == solnum ){
			sprintf(tmpname,"c.%.4d.s%.2d",cc,slist->insolid );
			mk_addmember( tmpname, head, WMOP_SUBTRACT );
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
		free( (char *)slist );
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
		slist = (struct subtract_list *)malloc(sizeof(struct subtract_list));
		slist->outsolid = outsolid;
		slist->insolid = insolid;
		slist->inmirror = inmirror;
		slist->next = (struct subtract_list *)NULL;
	}
	else
		slist->next = add_to_list(slist->next,outsolid,insolid,inmirror);

	return( slist );
}
