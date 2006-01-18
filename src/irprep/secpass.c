/*                       S E C P A S S . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2006 United States Government as represented by
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
/** @file secpass.c
 *
 *  Author -
 *	S.Coates - 11 February 1993
 *
 *  Source -
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *
 *  Notes -
 *	This version ONLY shoots down x-axis.
 */

/*	To compile:  cc -c secpass.c  */
/*		     cake  */

/*	CHANGES		*/
/*	11 December 1990 - 'Dimension' arrays using malloc.  */
/*	18 December 1990 - Incorperates subroutines rotate and radians.  */
/*	19 February 1991 - No defaults for material properties, thermal  */
/*			   conductivity is zero if one is zero.  */
/*	25 February 1991 - Print triangular conductivity file (still  */
/*			   computes entire matrix).  */
/*	14 March 1991    - Creates a generic file if needed.  */
/*	13 May 1991      - Remove some write statements.  */
/*	24 October 1991  - Put in appropriate include statements so that  */
/*			   I can compile.  */
/*	30 October 1991  - Make region numbering scheme the same through-  */
/*			   out, i.e. region numbers start at 1.  */
/*	15 November 1991 - Make a change to printing of conductivity table.  */
/*	11 February 1992 - Reverse order of regions in writing conductivity  */
/*			   file.  The largest number must come first.  */
/*	13 February 1992 - Change format for writing PRISM 3.0 files.  */
/*	10 March 1992    - Print out PRISM release being used.  */
/*	11 February 1993 - Add option to fire from three orthogonal  */
/*			   directions.  */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"


#define ADJTOL 1.e-1	/*  Tolerance for adjacent regions.  */
#define ZEROTOL 1.e-20	/*  Tolerance for dividing by zero.  */
#define MINCAL 10	/*  Minimum number of calculations for length.  */
#define ALPHA 25.	/*  Rotation about z-axis.  */
#define BETA 50.	/*  Rotation about y-axis.  */
#define GAMMA 35.	/*  Rotation about x-axis.  */


extern int hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp);	/*  User defined hit function.  */
extern int miss(register struct application *ap_p);	/*  User defined miss function.  */
extern int ovrlap(register struct application *ap_p, struct partition *PartHeadp, struct region *reg1, struct region *reg2);	/*  User defined overlap function.  */
extern void rotate(double *p, double *a, double *np);	/*  Subroutine to rotate a point.  */
extern double radians(double a);/*  Subroutine to put an angle into radians.  */

/*  Define structure.  */
/*  Make arrays into pointers such that malloc may be used  */
/*  later.  */
struct table
{
  double centroid[3];	/*  centroid of region  */
  double *shrarea;	/*  shared surface area between adjacent  */
  /*  regions  */
  int mat;		/*  material id  */
  double *avglen;		/*  average length between centroid &  */
				/*  shared surface area  */
  double *rmslen;		/*  root mean squared length between  */
				/*  centroid & shared surface area  */
  double *minlen;		/*  minimum length between centroid &  */
				/*  shared surface area  */
  double *maxlen;		/*  maximum length between centroid &  */
				/*  shared surface area  */
  double *numcal;		/*  number of length calculations  */
  double *rkavg;		/*  rk value using average length  */
  double *rkrms;		/*  rk value using root mean squared length  */
  double *rkmin;		/*  rk value using minimum length  */
  double *rkmax;		/*  rk value using maximum length  */
};

struct table *cond;		/*  name of table structure  */

int iprev;			/*  previous region hit  */
int icur;			/*  current region hit  */
double enterpt[3];		/*  point where ray enters  */
double leavept[3];		/*  point where ray leaves  */

int main(int argc, char **argv)
{	/*  START # 1  */
  struct application ap;	/*  Structure passed between functions.  */

  static struct rt_i *rtip;	/*  *rtip pointer to structure of  */
				/*  type rt_i  */
  char idbuf[132];	/*  first ID record info, used in  */
			/*  rt_dirbuild  */
  int index;		/*  index for rt_dirbuild & rt_gettree  */

  FILE *fp;		/*  used in opening file for second pass  */
  char spfile[16];	/*  second pass file name  */
  FILE *fp1=NULL;		/*  conductivity file  */
  char confile[16];	/*  conductivity file  */
  FILE *fp2;		/*  conductivity table file  */
  char tblfile[16];	/*  conductivity table file  */

  int i,j;		/*  integers used in loops  */
  int numreg;		/*  number of regions  */
  int nmged;		/*  number of regions in mged file  */
  double gridspace;	/*  spacing between fired rays  */
  int iwrite;		/*  0 => write to standard out, 1 => write  */
			/*  to file  */
  int typeout;		/*  Type of file to be written, 0 => PRISM file,  */
			/*  1 => generic file.  */
  FILE *fp6;		/*  Used in writing generic file.  */
  char genfile[16];	/*  Generic file name.  */
  FILE *fp3=NULL;		/*  used for writing output to file  */
  char filename[16];	/*  output file name  */

  FILE *fp5;		/*  material file  */
  char filemat[16];	/*  material file  */
  char line[150];	/*  used for reading a line from a file  */
  double k[41];	/*  thermal conductivity  */
  int itype;		/*  type of length measurement to use for  */
			/*  rk calculations  */
  double rki,rkj;	/*  used in finding rk  */
  double ki,kj;	/*  thermal conductivity of region  */
  double leni,lenj;	/*  lengths used in finding rk  */
  double areai;	/*  areas used in finding rk  */
  double a1;		/*  area used in writing conductivity table  */
  double l1,l2,l3,l4;	/*  lengths used in writing conductivity table  */
  FILE *fp4;		/*  error file  */
  char fileerr[16];	/*  error file  */

  double angle[3];	/*  Angles of rotation.  angle[0]-rotation about  */
			/*  x-axis, angle[1]-rotation about y-axis, &  */
			/*  angle[2]-rotation about z-axis.  */
  double strtpt[3];	/*  Starting point of fired ray.  */
  double strtdir[3];	/*  Starting direction of fired ray.  */
  double r[3],t[3];	/*  Used in computing rotations.  */
  double center[3];	/*  Center point of bounding rpp.  */
  double diagonal;	/*  Length of diagonal of bounding rpp.  */
  double xmin,xmax;	/*  Maximum & minimum x of grid.  */
  double ymin,ymax;	/*  Maximum & minimum y of grid.  */
  double zmin,zmax;	/*  Maximum & minimum z of grid.  */
  int nadjreg;		/*  Number of adjacent regions.  */
  int prmrel;		/*  PRISM release number, 2=>2.0, 3=>3.0.  */
  int ifire;		/*  Number of sets of rays to be fired, 0=>  */
			/*  fire from 3 orthogonal postions, 1=>fire  */
			/*  from 1 position.  */

  /*  Check to see if arguments implimented correctly.  */
  if(argv[1] == NULL || argv[2] == NULL)
    {
      (void)fprintf(stderr,"\nusage:  secpass file.g objects\n\n");
    }

  else
    {	/*  START # 2  */

	/*  Ask if output goes to standard out or to a file.  */
      (void)fprintf(stdout,"Write output to standard out (0) or a file(1) ");
      (void)fprintf(stdout,"not at all (2)?  ");
      (void)fflush(stdout);
      (void)scanf("%d",&iwrite);
      if((iwrite != 0) && (iwrite != 1)) iwrite=2;
      if(iwrite == 1)
	{
	  (void)fprintf(stdout,"Enter name of output file (15 char max).  ");
	  (void)fflush(stdout);
	  (void)scanf("%s",filename);
	  fp3 = fopen(filename,"w");
	}

      /*  Which file that has second pass information in it?  */
      (void)fprintf(stdout,"Enter name of file that has second pass ");
      (void)fprintf(stdout,"information\nin it (15 char max).  ");
      (void)fflush(stdout);
      (void)scanf("%s",spfile);

      /*  Ask for type of output file to be generated.  */
      (void)printf("Enter type of output file to be generated.\n");
      (void)printf("\t 0 - PRISM File\n");
      (void)printf("\t 1 - Generic File\n");
      (void)fflush(stdout);
      (void)scanf("%d",&typeout);

      /*  Read name of file to write conductivity information  */
      /*  to for use in PRISM.  */
      if(typeout == 0)
	{
	  (void)fprintf(stdout,"Enter name of file to be created for PRISM ");
	  (void)fprintf(stdout,"conductivity\ninformation (15 char max).  ");
	  (void)fflush(stdout);
	  (void)scanf("%s",confile);

	  /*  Find which release of PRISM is being used.  The format  */
	  /*  for writing region numbers is I3 for PRISM 2.0 & I6 for  */
	  /*  PRISM 3.0.  */
	  prmrel = 2;
	  (void)printf("Which release of PRISM is being used, 2.0 (2) ");
	  (void)printf("or 3.0 (3)?  ");
	  (void)fflush(stdout);
	  (void)scanf("%d",&prmrel);
	  if(prmrel != 3) prmrel = 2;
	}

      /*  Read generic file name if necessary.  */
      if(typeout == 1)
	{
	  (void)printf("Enter name of generic file to be created (15 char ");
	  (void)printf("max).  ");
	  (void)fflush(stdout);
	  (void)scanf("%s",genfile);
	}

      /*  Which calculated length should be used when writing to  */
      /*  this file:  1 -> average length, 2 -> rms length, 3 ->  */
      /*  minimum length, and 4 -> maximum length.  */
      (void)fprintf(stdout,"Which length calculation should be used when\n");
      (void)fprintf(stdout,"computing conduction\nbetween regions?\n");
      (void)fprintf(stdout,"\t1 - average length\n");
      (void)fprintf(stdout,"\t2 - rms length\n");
      (void)fprintf(stdout,"\t3 - minimum length\n");
      (void)fprintf(stdout,"\t4 - maximum length\n");
      (void)fflush(stdout);
      (void)scanf("%d",&itype);

      /*  Read name of file to write conductivity information to  */
      /*  in table format.  */
      (void)fprintf(stdout,"Enter name of file to be created for ");
      (void)fprintf(stdout,"conductivity\ntable (15 char max).  ");
      (void)fflush(stdout);
      (void)scanf("%s",tblfile);

      /*  Read name of material file that contains thermal  */
      /*  conductivity information.  */
      (void)fprintf(stdout,"Enter name of material file (15 char max).  ");
      (void)fflush(stdout);
      (void)scanf("%s",filemat);

      /*  Read name of error file.  */
      (void)fprintf(stdout,"Enter name of error file to be created ");
      (void)fprintf(stdout,"(15 char max).  ");
      (void)fflush(stdout);
      (void)scanf("%s",fileerr);

      /*  Choose whether 3 orthogonal sets of rays are to be fired  */
      /*  or 1 set of rays is to be fired.  */
      (void)printf("Should there be 3 sets of orhogonal rays fired ");
      (void)printf("(0) or 1 set (1)?\n\t");
      (void)fflush(stdout);
      (void)scanf("%d",&ifire);
      if(ifire != 0) ifire = 1;
      if(ifire == 0)
	{
	  (void)printf("3 sets of orthogonal rays will be fired.\n");
	}
      if(ifire == 1)
	{
	  (void)printf("1 set of rays will be fired.\n");
	}
      (void)fflush(stdout);

      /*  Write out file information.  */
      if(iwrite ==1)
	{
	  (void)fprintf(fp3,"\nsecond pass file:  %s\n",spfile);
	  (void)fprintf(fp3,"material file:  %s\n",filemat);
	  if(typeout == 0)
	    {
	      (void)fprintf(fp3,"conductivity file for use ");
	      (void)fprintf(fp3,"with PRISM:  %s\n",confile);
	      (void)fprintf(fp3,"  (format is PRISM %d.0)\n",prmrel);
	    }
	  if(typeout == 1) (void)fprintf(fp3,"generic file:  %s\n",genfile);
	  (void)fflush(fp3);
	  if(itype == 1) (void)fprintf(fp3,"\taverage length being used\n");
	  if(itype == 2) (void)fprintf(fp3,"\trms length being used\n");
	  if(itype == 3) (void)fprintf(fp3,"\tminimum length being used\n");
	  if(itype == 4) (void)fprintf(fp3,"\tmaximum length being used\n");
	  (void)fprintf(fp3,"conductivity table file:  %s\n",tblfile);
	  (void)fprintf(fp3,"error file:  %s\n\n",fileerr);
	  (void)fflush(fp3);
	}

      /*  Read thermal conductivity file.  */
      fp5 = fopen(filemat,"r");
      for(i=0; i<41; i++)
	{
	  (void)fgets(line,151,fp5);
	  (void)sscanf(line,"%*d%*f%*f%*f%*f%lf",&k[i]);
	}

      /*  Print out the thermal conductivity.  */
      /*
       *	for(i=0; i<41; i++)
       *	{
       *	   (void)fprintf(stdout,"  thermal conductivity %d = %f\n",i,k[i]);
       *	   (void)fflush(stdout);
       *	}
       */

      /*  Build the directory.  */
      index = 1;	/*  Set index for rt_dirbuild.  */
      rtip=rt_dirbuild(argv[index],idbuf,sizeof(idbuf));
      (void)fprintf(stdout,"Database title:  %s\n",idbuf);
      (void)fflush(stdout);

      /*  Set useair to 1, to show hits of air.  */
      rtip->useair = 1;

      /*  Load desired objects.  */
      index = 2;	/*  Set index for rt_gettree.  */
      while(argv[index] != NULL)
	{
	  rt_gettree(rtip,argv[index]);
	  index += 1;
	}

      /*  Find total number of regions in mged file.  */
      nmged = (int)rtip->nregions;

      (void)fprintf(stdout,"Number of regions in mged file:  %d\n",nmged);
      (void)fflush(stdout);

      if(iwrite == 1)
	{
	  (void)fprintf(fp3,"Number of regions in mged file:  %d\n",nmged);
	  (void)fflush(fp3);
	}

      /*  Number of regions known, everything can be malloced.  */
      (void)fprintf(stdout,"Mallocing arrays.\n");
      (void)fflush(stdout);

      cond = (struct table *)malloc( nmged * sizeof(struct table) );
      (void)fprintf(stdout,"cond malloced\n");
      (void)fflush(stdout);
      for(i=0; i<nmged; i++)
	{
	  cond[i].shrarea = (double *)malloc( nmged * sizeof(double) );
	  cond[i].avglen = (double *)malloc( nmged * sizeof(double) );
	  cond[i].rmslen = (double *)malloc( nmged * sizeof(double) );
	  cond[i].minlen = (double *)malloc( nmged * sizeof(double) );
	  cond[i].maxlen = (double *)malloc( nmged * sizeof(double) );
	  cond[i].numcal = (double *)malloc( nmged * sizeof(double) );
	  cond[i].rkavg = (double *)malloc( nmged * sizeof(double) );
	  cond[i].rkrms = (double *)malloc( nmged * sizeof(double) );
	  cond[i].rkmin = (double *)malloc( nmged * sizeof(double) );
	  cond[i].rkmax = (double *)malloc( nmged * sizeof(double) );
	}
      (void)fprintf(stdout,"loop malloced\n");
      (void)fflush(stdout);

      /*  All variables 'dimensioned', now zero all variables.  */
      for(i=0; i<nmged; i++)
	{
	  cond[i].centroid[0] = (double)0.;
	  cond[i].centroid[1] = (double)0.;
	  cond[i].centroid[2] = (double)0.;
	  cond[i].mat = (int)0;
	  for(j=0; j<nmged; j++)
	    {
	      cond[i].shrarea[j] = (double)0.;
	      cond[i].avglen[j] = (double)0.;
	      cond[i].rmslen[j] = (double)0.;
	      cond[i].minlen[j] = (double)0.;
	      cond[i].maxlen[j] = (double)0.;
	      cond[i].numcal[j] = (double)0.;
	      cond[i].rkavg[j] = (double)0.;
	      cond[i].rkrms[j] = (double)0.;
	      cond[i].rkmin[j] = (double)0.;
	      cond[i].rkmax[j] = (double)0.;
	    }
	}
      (void)fprintf(stdout,"All variables zeroed.\n");
      (void)fflush(stdout);

      /*  Now open file with second pass information in it.  */
      fp = fopen(spfile,"r");
      (void)fprintf(stdout,"second pass file opened\n");
      (void)fflush(stdout);

      /*  Read number of regions in file.  */
      (void)fscanf(fp,"%d\n",&numreg);
      (void)fprintf(stdout,"The number of regions read was %d\n",numreg);
      (void)fflush(stdout);

      if(iwrite == 1)
	{
	  (void)fprintf(fp3,"number of regions read from second ");
	  (void)fprintf(fp3,"pass file:  %d\n",numreg);
	  (void)fflush(fp3);
	}

      /*  Read all information in file.  */
      for(i=0; i<numreg; i++)
	{
	  (void)fscanf(fp,"%*d%le%le%le%d\n",&cond[i].centroid[0],
		       &cond[i].centroid[1],
		       &cond[i].centroid[2],&cond[i].mat);
	  /*
	   *	   (void)fprintf(stdout,"reg=%8d, centroid:  %10.0f, %10.0f, %10.0f\n",
	   *		i,cond[i].centroid[0],cond[i].centroid[1],cond[i].centroid[2]);
	   *	   (void)fflush(stdout);
	   */

	  for(j=0; j<numreg; j++)
	    {
	      (void)fscanf(fp,"%*d%le\n",&cond[i].shrarea[j]);
	      /*
	       *		(void)fprintf(stdout,"\treg=%8d, area=%10.0f\n",
	       *		   j,cond[i].shrarea[j]);
	       *		(void)fflush(stdout);
	       */
	    }
	}

      (void)fclose(fp);

      /*  Check that the number of regions in the mged file  */
      /*  and the second pass file are equal.  */
      if(nmged != numreg)
	{
	  (void)fprintf(stdout,"ERROR -- The number of regions in the mged\n");
	  (void)fprintf(stdout,"file (%d) does not equal the number of\n",
			nmged);
	  (void)fprintf(stdout,"regions in the second pass file (%d).\n",
			numreg);
	  (void)fprintf(stdout,"Watch for unexplained errors.\n");
	  (void)fflush(stdout);

	  if(iwrite == 1)
	    {
	      (void)fprintf(fp3,"ERROR -- The number of regions in the mged\n");
	      (void)fprintf(fp3,"file (%d) does not equal the number of\n",
			    nmged);
	      (void)fprintf(fp3,"regions in the second pass file (%d).\n",
			    numreg);
	      (void)fprintf(fp3,"Watch for unexplained errors.\n");
	      (void)fflush(fp3);
	    }
	}

      /*  Get database ready by starting preparation.  */
      rt_prep(rtip);

      /*  Find center of bounding rpp.  */
      center[X] = rtip->mdl_min[X] + (rtip->mdl_max[X] -
				      rtip->mdl_min[X]) / 2.;
      center[Y] = rtip->mdl_min[Y] + (rtip->mdl_max[Y] -
				      rtip->mdl_min[Y]) / 2.;
      center[Z] = rtip->mdl_min[Z] + (rtip->mdl_max[Z] -
				      rtip->mdl_min[Z]) / 2.;

      /*  Find length of diagonal.  */
      diagonal = (rtip->mdl_max[X] - rtip->mdl_min[X])
	* (rtip->mdl_max[X] - rtip->mdl_min[X])
	+ (rtip->mdl_max[Y] - rtip->mdl_min[Y])
	* (rtip->mdl_max[Y] - rtip->mdl_min[Y])
	+ (rtip->mdl_max[Z] - rtip->mdl_min[Z])
	* (rtip->mdl_max[Z] - rtip->mdl_min[Z]);
      diagonal = sqrt(diagonal) / 2. + .5;

      /*  Find minimum & maximum of grid.  */
      xmin = center[X] - diagonal;
      xmax = center[X] + diagonal;
      ymin = center[Y] - diagonal;
      ymax = center[Y] + diagonal;
      zmin = center[Z] - diagonal;
      zmax = center[Z] + diagonal;

      /*  Print center of bounding rpp, diagonal, & maximum  */
      /*  & minimum of grid.  */
      (void)fprintf(stdout,"Center of bounding rpp ( %f, %f, %f )\n",
		    center[X],center[Y],center[Z]);
      (void)fprintf(stdout,"Length of diagonal of bounding rpp:  %f\n",
		    diagonal);
      (void)fprintf(stdout,"Minimums & maximums of grid:\n");
      (void)fprintf(stdout,"  %f - %f\n",xmin,xmax);
      (void)fprintf(stdout,"  %f - %f\n",ymin,ymax);
      (void)fprintf(stdout,"  %f - %f\n\n",zmin,zmax);
      (void)fflush(stdout);

      /*  Write model minimum & maximum.  */
      (void)fprintf(stdout,"Model minimum & maximum.\n");
      (void)fprintf(stdout,"\tX:  %f to %f\n\tY:  %f to %f\n\tZ:  %f to %f\n\n",
		    rtip->mdl_min[X],rtip->mdl_max[X],
		    rtip->mdl_min[Y],rtip->mdl_max[Y],
		    rtip->mdl_min[Z],rtip->mdl_max[Z]);
      (void)fflush(stdout);

      if(iwrite == 1)
	{
	  (void)fprintf(fp3,"Model minimum & maximum.\n");
	  (void)fprintf(fp3,"\tX:  %f to %f\n\tY:  %f kto %f\n",
			rtip->mdl_min[X],rtip->mdl_max[X],
			rtip->mdl_min[Y],rtip->mdl_max[Y]);
	  (void)fprintf(fp3,"\tZ:  %f to %f\n\n",
			rtip->mdl_min[Z],rtip->mdl_max[Z]);
	  (void)fflush(fp3);
	}

      /*  User enters grid spacing.  All units are in mm.  */
      (void)fprintf(stdout,"Enter spacing (mm) between fired rays.  ");
      (void)fflush(stdout);
      (void)scanf("%lf",&gridspace);

      (void)fprintf(stdout,"\ngrid spacing:  %f\n",gridspace);
      (void)fflush(stdout);

      if(iwrite == 1)
	{
	  (void)fprintf(fp3,"gridspacing:  %f\n\n",gridspace);
	  (void)fflush(fp3);
	}

      /*  Set up parameters for rt_shootray.  */
      RT_APPLICATION_INIT(&ap);
      ap.a_hit = hit;		/*  User supplied hit function.  */
      ap.a_miss = miss;	/*  User supplied miss function.  */
      ap.a_overlap = ovrlap;	/*  user supplied overlap function.  */
      ap.a_rt_i = rtip;	/*  Pointer from rt_dirbuild.  */
      ap.a_onehit = 0;	/*  Hit flag (returns all hits).  */
      ap.a_level = 0;		/*  Recursion level for diagnostics.  */
      ap.a_resource = 0;	/*  Address of resource structure (NULL).  */

      /*  Put angles for rotation into radians.  */
      angle[X] = radians((double)GAMMA);
      angle[Y] = radians((double)BETA);
      angle[Z] = radians((double)ALPHA);

      /*  Set up and shoot down the 1st axis, positive to negative  */
      /*  (x-axis).  */

      (void)fprintf(stdout,"\nShooting down 1st axis.\n");
      (void)fflush(stdout);

      strtpt[X] = xmax;
      strtpt[Y] = ymin + gridspace / 2.;
      strtpt[Z] = zmin + gridspace / 2.;
      strtdir[X] = (-1);
      strtdir[Y] = 0;
      strtdir[Z] = 0;

      /*  Rotate starting point.  (new pt = C + R[P -C])  */
      t[X] = strtpt[X] - center[X];
      t[Y] = strtpt[Y] - center[Y];
      t[Z] = strtpt[Z] - center[Z];

      (void)rotate(t,angle,r);

      ap.a_ray.r_pt[X] = center[X] + r[X];
      ap.a_ray.r_pt[Y] = center[Y] + r[Y];
      ap.a_ray.r_pt[Z] = center[Z] + r[Z];

      /*  Rotate firing direction.  (new dir = R[D])  */
      (void)rotate(strtdir,angle,r);
      ap.a_ray.r_dir[X] = r[X];
      ap.a_ray.r_dir[Y] = r[Y];
      ap.a_ray.r_dir[Z] = r[Z];

      while(strtpt[Z] <= zmax)
	{	/*  START # 3  */

	  iprev = (-1);	/*  No previous shots.  */

	  /*  Call rt_shootray.  */
	  (void)rt_shootray ( &ap );

	  strtpt[Y] += gridspace;
	  if(strtpt[Y] > ymax)
	    {
	      strtpt[Y] = ymin + gridspace / 2.;
	      strtpt[Z] += gridspace;
	    }

	  t[X] = strtpt[X] - center[X];
	  t[Y] = strtpt[Y] - center[Y];
	  t[Z] = strtpt[Z] - center[Z];

	  (void)rotate(t,angle,r);

	  ap.a_ray.r_pt[X] = center[X] + r[X];
	  ap.a_ray.r_pt[Y] = center[Y] + r[Y];
	  ap.a_ray.r_pt[Z] = center[Z] + r[Z];

	}	/*  END # 3  */

      /*  Shoot down 2nd & 3rd axes if necessary.  */
      if(ifire == 0)
	{						/*  START # 1000  */
	  /*  Set up & shoot down the 2nd axis (y-axis).  */
	  (void)printf("\nShooting down the 2nd axis.\n");
	  (void)fflush(stdout);

	  strtpt[X] = xmin + gridspace / 2.;
	  strtpt[Y] = ymax;
	  strtpt[Z] = zmin + gridspace / 2.;
	  strtdir[X] = 0.;
	  strtdir[Y] = (-1.);
	  strtdir[X] = 0.;

	  /*  Rotate starting point (new pt = C + R[P - C]).  */
	  t[X] = strtpt[X] - center [X];
	  t[Y] = strtpt[Y] - center [Y];
	  t[Z] = strtpt[Z] - center [Z];

	  (void)rotate(t,angle,r);

	  ap.a_ray.r_pt[X] = center[X] + r[X];
	  ap.a_ray.r_pt[Y] = center[Y] + r[Y];
	  ap.a_ray.r_pt[Z] = center[Z] + r[Z];

	  /*  Rotate firing direction (new dir = R[D])  */
	  (void)rotate(strtdir,angle,r);

	  ap.a_ray.r_dir[X] = r[X];
	  ap.a_ray.r_dir[Y] = r[Y];
	  ap.a_ray.r_dir[Z] = r[Z];

	  while(strtpt[Z] <= zmax)
	    {						/*  START # 1010  */
	      iprev = (-1);		/*  No previous shots.  */

	      /*  Call rt_shootray.  */
	      (void)rt_shootray(&ap);

	      strtpt[X] += gridspace;
	      if(strtpt[X] > xmax)
		{
		  strtpt[X] = xmin + gridspace / 2.;
		  strtpt[Z] += gridspace;
		}

	      t[X] = strtpt[X] - center[X];
	      t[Y] = strtpt[Y] - center[Y];
	      t[Z] = strtpt[Z] - center[Z];

	      (void)rotate(t,angle,r);

	      ap.a_ray.r_pt[X] = center[X] + r[X];
	      ap.a_ray.r_pt[Y] = center[Y] + r[Y];
	      ap.a_ray.r_pt[Z] = center[Z] + r[Z];
	    }						/*  END # 1010  */

	  /*  Set up & shoot down the 3rd axis (z-axis).  */
	  (void)printf("\nShooting down the 3rd axis.\n");
	  (void)fflush(stdout);

	  strtpt[X] = xmin + gridspace / 2.;
	  strtpt[Y] = ymin + gridspace / 2.;
	  strtpt[Z] = zmax;
	  strtdir[X] = 0.;
	  strtdir[Y] = 0.;
	  strtdir[Z] = (-1.);

	  /*  Rotate starting points (new pt = C + R[P - C]).  */
	  t[X] = strtpt[X] - center[X];
	  t[Y] = strtpt[Y] - center[Y];
	  t[Z] = strtpt[Z] - center[Z];

	  (void)rotate(t,angle,r);

	  ap.a_ray.r_pt[X] = r[X];
	  ap.a_ray.r_pt[Y] = r[Y];
	  ap.a_ray.r_pt[Z] = r[Z];

	  while(strtpt[Y] <= ymax)
	    {						/*  START # 1020  */
	      iprev = (-1);		/*  No previous shots.  */

	      /*  Call rt_shootray.  */
	      (void)rt_shootray(&ap);

	      strtpt[X] += gridspace;
	      if(strtpt[X] > xmax)
		{
		  strtpt[X] = xmin + gridspace / 2.;
		  strtpt[Y] += gridspace;
		}

	      t[X] = strtpt[X] - center[X];
	      t[Y] = strtpt[Y] - center[Y];
	      t[Z] = strtpt[Z] - center[Z];

	      (void)rotate(t,angle,r);

	      ap.a_ray.r_pt[X] = center[X] + r[X];
	      ap.a_ray.r_pt[Y] = center[Y] + r[Y];
	      ap.a_ray.r_pt[Z] = center[Z] + r[Z];
	    }						/*  END # 1020  */
	}						/*  END # 1000  */

      /*  Calculate final length between centroid & shared surface area.  */
      if(iwrite == 0)
	{
	  (void)fprintf(stdout,"\n\nFinal numbers.\n");
	  (void)fflush(stdout);
	}

      for(i=0; i<numreg; i++)
	{

	  if(iwrite == 0)
	    {
	      (void)fprintf(stdout,"reg#=%d, matid=%d\n",(i+1),cond[i].mat);
	      (void)fflush(stdout);
	    }

	  if(iwrite == 1)
	    {
	      (void)fprintf(fp3,"reg#=%d, matid=%d\n",(i+1),cond[i].mat);
	      (void)fflush(fp3);
	    }

	  for(j=0; j<numreg; j++)
	    {
	      if(cond[i].numcal[j] > ZEROTOL)
		{
		  cond[i].avglen[j] /= cond[i].numcal[j];
		  cond[i].rmslen[j] /= cond[i].numcal[j];
		  cond[i].rmslen[j] = sqrt(cond[i].rmslen[j]);

		  if(iwrite == 0)
		    {
		      (void)fprintf(stdout,"\tadjreg=%d, numcal=%f, shrarea=%f, ",
				    (j+1),cond[i].numcal[j],cond[i].shrarea[j]);
		      (void)fprintf(stdout,"avglen=%f\n",cond[i].avglen[j]);
		      (void)fprintf(stdout,"\t\trmslen=%f, ",cond[i].rmslen[j]);
		      (void)fprintf(stdout,"minlen=%f, maxlen=%f\n",
				    cond[i].minlen[j],cond[i].maxlen[j]);
		      (void)fflush(stdout);
		    }

		  if(iwrite == 1)
		    {
		      (void)fprintf(fp3,"\tadjreg=%d, numcal=%f, shrarea=%f, ",
				    (j+1),cond[i].numcal[j],cond[i].shrarea[j]);
		      (void)fprintf(fp3,"avglen=%f\n",cond[i].avglen[j]);
		      (void)fprintf(fp3,"\t\trmslen=%f, ",cond[i].rmslen[j]);
		      (void)fprintf(fp3,"minlen=%f, maxlen=%f\n",
				    cond[i].minlen[j],cond[i].maxlen[j]);
		      (void)fflush(fp3);
		    }

		}
	      else
		{
		  cond[i].avglen[j] = 0.;
		  cond[i].rmslen[j] = 0.;
		}
	    }
	}

      if(iwrite == 1)
	{
	  /*  Print summary of all files used.  */
	  (void)fprintf(fp3,"\n\nSUMMARY OF FILES USED & CREATED\n");
	  (void)fprintf(fp3,"\t.g file used:  %s\n",argv[1]);
	  (void)fprintf(fp3,"\tregions used:\n");
	  (void)fflush(fp3);
	  i=2;
	  while(argv[i] != NULL)
	    {
	      (void)fprintf(fp3,"\t\t%s\n",argv[i]);
	      (void)fflush(fp3);
	      i++;
	    }
	  (void)fprintf(fp3,"\tfile containing second pass information:  %s\n",
			spfile);
	  (void)fprintf(fp3,"\tmaterial file used:  %s\n",filemat);
	  (void)fprintf(fp3,"\toutput file created:  %s\n",filename);
	  if(typeout == 0)
	    {
	      (void)fprintf(fp3,"\tconductivity file created:  %s\n",confile);
	      (void)fprintf(fp3,"\t  (format is PRISM %d.0)\n",prmrel);
	    }
	  if(typeout == 1) (void)fprintf(fp3,"\tgeneric file created:  %s\n"
					 ,genfile);
	  (void)fprintf(fp3,"\tconductivity table file created:  %s\n",
			tblfile);
	  (void)fprintf(fp3,"\terror file created:  %s\n\n\n",fileerr);
	  (void)fflush(fp3);

	  (void)fclose(fp3);
	}

      /*------------------------------------------------------------------*/

      /*  Open conductivity file to be used with PRISM if needed.  */
      if(typeout == 0)
	{
	  fp1=fopen(confile,"w");
	  (void)fprintf(fp1,"Conductivity file for use with PRISM.\n");
	  (void)fflush(fp1);
	}

      /*  Make calculations & write to conductivity file.  */
      for(i=0; i<numreg; i++)
	{	/*  START # 6  */

	  /*  Make conductivity file triangular.  This program still  */
	  /*  computes the ENTIRE matrix.  */

	  for(j=(i+1); j<numreg; j++)
	    {	/*  START # 7  */

	      if( (cond[i].avglen[j] != 0) )
		{	/*  START # 8  */
		  /*  Find correct thermal conductivity.  */
		  /*  If ki or kj = 0 => rk = 0.  */
		  ki = k[cond[i].mat];
		  kj = k[cond[j].mat];

		  /*  All calculations will be in meters &  */
		  /*  square meters.  */

		  areai = cond[i].shrarea[j] * 1.e-6;

		  /*  average length  */
		  leni = cond[i].avglen[j] * 1.e-3;
		  lenj = cond[j].avglen[i] * 1.e-3;
		  if(((-ZEROTOL < ki) && (ki < ZEROTOL)) ||
		     ((-ZEROTOL < kj) && (kj < ZEROTOL)))
		    cond[i].rkavg[j] = 0.;
		  else
		    {
		      rki = leni / (ki * areai);
		      rkj = lenj / (kj * areai);
		      cond[i].rkavg[j] = 1. / (rki + rkj);
		    }

		  /*  rms length  */
		  leni = cond[i].rmslen[j] * 1.e-3;
		  lenj = cond[j].rmslen[i] * 1.e-3;
		  if(((-ZEROTOL < ki) && (ki < ZEROTOL)) ||
		     ((-ZEROTOL < kj) && (kj < ZEROTOL)))
		    cond[i].rkrms[j] = 0.;
		  else
		    {
		      rki = leni / (ki * areai);
		      rkj = lenj / (kj * areai);
		      cond[i].rkrms[j] = 1. / (rki + rkj);
		    }

		  /*  minimum length  */
		  leni = cond[i].minlen[j] * 1.e-3;
		  lenj = cond[j].minlen[i] * 1.e-3;
		  if(((-ZEROTOL < ki) && (ki < ZEROTOL)) ||
		     ((-ZEROTOL < kj) && (kj < ZEROTOL)))
		    cond[i].rkmin[j] = 0.;
		  else
		    {
		      rki = leni / (ki * areai);
		      rkj = lenj / (kj * areai);
		      cond[i].rkmin[j] = 1. / (rki + rkj);
		    }

		  /*  maximum length  */
		  leni = cond[i].maxlen[j] * 1.e-3;
		  lenj = cond[j].maxlen[i] * 1.e-3;
		  if(((-ZEROTOL < ki) && (ki < ZEROTOL)) ||
		     ((-ZEROTOL < kj) && (kj < ZEROTOL)))
		    cond[i].rkmax[j] = 0.;
		  else
		    {
		      rki = leni / (ki * areai);
		      rkj = lenj / (kj * areai);
		      cond[i].rkmax[j] = 1. / (rki + rkj);
		    }

		  /*  Print if had adjacent regions, conductivity  */
		  /*  may be zero.  */

		  /*  Print only if PRISM file is to be created.  */
		  if(typeout == 0) {		/*  START # 8A  */
		    if( (itype == 1) && (cond[i].shrarea[j] > ZEROTOL) )
		      {
			if(prmrel == 2)
			  (void)fprintf(fp1,"%3d %3d %7.3f %.3e\n",
					(j+1),(i+1),cond[i].rkavg[j],
					(cond[i].shrarea[j] * 1.0e-6));
			if(prmrel == 3)
			  (void)fprintf(fp1,"%6d %6d %7.3f %.3e\n",
					(j+1),(i+1),cond[i].rkavg[j],
					(cond[i].shrarea[j] * 1.0e-6));
		      }

		    if( (itype == 2) && (cond[i].shrarea[j] > ZEROTOL) )
		      {
			if(prmrel == 2)
			  (void)fprintf(fp1,"%3d %3d %7.3f %.3e\n",
					(j+1),(i+1),cond[i].rkrms[j],
					(cond[i].shrarea[j] * 1.0e-6));
			if(prmrel == 3)
			  (void)fprintf(fp1,"%6d %6d %7.3f %.3e\n",
					(j+1),(i+1),cond[i].rkrms[j],
					(cond[i].shrarea[j] * 1.0e-6));
		      }

		    if( (itype == 3) && (cond[i].shrarea[j] > ZEROTOL) )
		      {
			if(prmrel == 2)
			  (void)fprintf(fp1,"%3d %3d %7.3f %.3e\n",
					(j+1),(i+1),cond[i].rkmin[j],
					(cond[i].shrarea[j] * 1.0e-6));
			if(prmrel == 3)
			  (void)fprintf(fp1,"%6d %6d %7.3f %.3e\n",
					(j+1),(i+1),cond[i].rkmin[j],
					(cond[i].shrarea[j] * 1.0e-6));
		      }

		    if( (itype == 4) && (cond[i].shrarea[j] > ZEROTOL) )
		      {
			if(prmrel == 2)
			  (void)fprintf(fp1,"%3d %3d %7.3f %.3e\n",
					(j+1),(i+1),cond[i].rkmax[j],
					(cond[i].shrarea[j] * 1.0e-6));
			if(prmrel == 3)
			  (void)fprintf(fp1,"%6d %6d %7.3f %.3e\n",
					(j+1),(i+1),cond[i].rkmax[j],
					(cond[i].shrarea[j] * 1.0e-6));
		      }

		    (void)fflush(fp1);
		  }				/*  END of # 8A  */
		}	/*  END # 8  */
	    }	/*  END # 7  */
	}	/*  END # 6  */

      if(typeout == 0) (void)fclose(fp1);

      /*------------------------------------------------------------------*/

      /*  Open and write to generic file if necessary. */
      /*  The format follows.  */
      /*  4  region number  number of adjacent regions  */
      /*     adjacent region  shared area  conduction distance  */

      if(typeout == 1)
	{
	  /*  Open file.  */
	  fp6 = fopen(genfile,"w");
	  (void)printf("Opened generic file.\n");
	  (void)fflush(stdout);

	  for(i=0; i<numreg; i++)
	    {
	      /*  Find number of adjacent regions.  */
	      /*
	       *		(void)printf("Ready to find number of adjacent areas.\n");
	       *		(void)fflush(stdout);
	       */
	      nadjreg = 0;
	      /*
	       *		(void)printf("nadjreg = %d\n",nadjreg);
	       *		(void)fflush(stdout);
	       */
	      for(j=0; j<numreg; j++)
		{
		  /*
		   *		   (void)printf("%d, %d, %f\n",i,j,cond[i].shrarea[j]);
		   *		   (void)fflush(stdout);
		   */
		  if(cond[i].shrarea[j] > ZEROTOL) nadjreg += 1;
		}
	      /*
	       *		(void)printf("Found number of adjacent areas.\n");
	       *		(void)fflush(stdout);
	       */

	      (void)fprintf(fp6,"4  %5d  %5d\n",(i+1),nadjreg);
	      (void)fflush(fp6);

	      for(j=0; j<numreg; j++)
		{
		  if(cond[i].shrarea[j] > ZEROTOL)
		    {
		      (void)fprintf(fp6,"   %5d  %.3e  ",(j+1),
				    (cond[i].shrarea[j] * 1.e-6));
		      if(itype == 1) (void)fprintf(fp6,"%.3e\n",
						   (cond[i].avglen[j] * 1.e-3));
		      if(itype == 2) (void)fprintf(fp6,"%.3e\n",
						   (cond[i].rmslen[j] * 1.e-3));
		      if(itype == 3) (void)fprintf(fp6,"%.3e\n",
						   (cond[i].minlen[j] * 1.e-3));
		      if(itype == 4) (void)fprintf(fp6,"%.3e\n",
						   (cond[i].maxlen[j] * 1.e-3));
		    }
		  (void)fflush(fp6);
		}
	    }
	  (void)fclose(fp6);
	}

      /*------------------------------------------------------------------*/

      /*  Open conductivity table file and write information to  */
      /*  it.  All units will be in meters or square meters.  */
      fp2=fopen(tblfile,"w");
      (void)fprintf(fp2,"Conductivity table.  Units are in meters or ");
      (void)fprintf(fp2,"square meters.\n");

      (void)fprintf(fp2," reg, mat, adj,   shrarea,");
      (void)fprintf(fp2,"    avglen,     rkavg,");
      (void)fprintf(fp2,"    rmslen,     rkrms,");
      (void)fprintf(fp2,"    minlen,     rkmin,");
      (void)fprintf(fp2,"    maxlen,     rkmax\n");

      (void)fflush(fp2);

      for(i=0; i<numreg; i++)
	{	/*  START # 9  */
	  for(j=0; j<numreg; j++)
	    {	/*  START # 10  */
	      if(cond[i].shrarea[j] != 0)
		{	/*  START # 11  */
		  a1 = cond[i].shrarea[j] * 1.e-6;
		  l1 = cond[i].avglen[j] * 1.e-3;
		  l2 = cond[i].rmslen[j] * 1.e-3;
		  l3 = cond[i].minlen[j] * 1.e-3;
		  l4 = cond[i].maxlen[j] * 1.e-3;

		  (void)fprintf(fp2,"%4d,%4d,%4d, %.3e,",
				(i+1),cond[i].mat,(j+1),a1);
		  if(j > i)
		    {
		      (void)fprintf(fp2," %.3e, %.3e,",l1,cond[i].rkavg[j]);
		      (void)fprintf(fp2," %.3e, %.3e,",l2,cond[i].rkrms[j]);
		      (void)fprintf(fp2," %.3e, %.3e,",l3,cond[i].rkmin[j]);
		      (void)fprintf(fp2," %.3e, %.3e\n",l4,cond[i].rkmax[j]);
		    }
		  else
		    {
		      (void)fprintf(fp2," %.3e, %.3e,",l1,cond[j].rkavg[i]);
		      (void)fprintf(fp2," %.3e, %.3e,",l2,cond[j].rkrms[i]);
		      (void)fprintf(fp2," %.3e, %.3e,",l3,cond[j].rkmin[i]);
		      (void)fprintf(fp2," %.3e, %.3e\n",l4,cond[j].rkmax[i]);
		    }

		  (void)fflush(fp2);
		}	/*  END # 11  */
	    }	/*  END # 10  */
	}	/*  END # 9  */

      (void)fclose(fp2);

      /*------------------------------------------------------------------*/

      /*  Print summary of all files used.  */
      (void)fprintf(stdout,"\n\nSUMMARY OF FILES USED & CREATED\n");
      (void)fprintf(stdout,"\t.g file used:  %s\n",argv[1]);
      (void)fprintf(stdout,"\tregions used:\n");
      (void)fflush(stdout);
      i=2;
      while(argv[i] != NULL)
	{
	  (void)fprintf(stdout,"\t\t%s\n",argv[i]);
	  (void)fflush(stdout);
	  i++;
	}
      (void)fprintf(stdout,"\tfile containing second pass information:  %s\n",
		    spfile);
      (void)fprintf(stdout,"\tmaterial file used:  %s\n",filemat);
      if(iwrite == 1)
	{
	  (void)fprintf(stdout,"\toutput file created:  %s\n",filename);
	}
      if(typeout == 0)
	{
	  (void)fprintf(stdout,"\tconductivity file created:  %s\n",confile);
	  (void)fprintf(stdout,"\t  (format is PRISM %d.0)\n",prmrel);
	}
      if(typeout == 1) (void)printf("\tgeneric file created:  %s\n",genfile);
      (void)fprintf(stdout,"\tconductivity table file created:  %s\n",tblfile);
      (void)fprintf(stdout,"\terror file created:  %s\n\n\n",fileerr);
      (void)fflush(stdout);

      /*------------------------------------------------------------------*/

      /*  Open error file.  */
      fp4 = fopen(fileerr,"w");

      /*  Write errors to error file.  */
      (void)fprintf(fp4,"\nERRORS from secpass\n\n");
      /*  Write type of file created to error file.  */
      if(typeout == 0)
	{
	  (void)fprintf(fp4,"PRISM %d.0 conductivity file, %s, created.\n\n",
			prmrel,confile);
	}
      if(typeout == 1) (void)fprintf(fp4,"Generic file, %s, created.\n\n",
				     genfile);
      (void)fflush(fp4);
      for(i=0; i<numreg; i++)
	{
	  for(j=0; j<numreg;  j++)
	    {
	      if( (cond[i].numcal[j] > ZEROTOL) &&
		  ( cond[i].numcal[j] < MINCAL ) )
		{
		  (void)fprintf(fp4,"region %d, adjacent region %d:\n",
				(i+1),(j+1));
		  (void)fprintf(fp4,"\tnumber of length calculations ");
		  (void)fprintf(fp4,"below minimum of %d\n",MINCAL);
		  (void)fflush(fp4);
		}
	    }
	}
      (void)fclose(fp4);

      /*------------------------------------------------------------------*/

      /*  Everything completed, free memory.  */
      (void)fprintf(stdout,"Freeing memory.\n");
      (void)fflush(stdout);
      for(i=0; i<nmged; i++)
	{
	  free(cond[i].shrarea);
	  free(cond[i].avglen);
	  free(cond[i].rmslen);
	  free(cond[i].minlen);
	  free(cond[i].maxlen);
	  free(cond[i].numcal);
	  free(cond[i].rkavg);
	  free(cond[i].rkrms);
	  free(cond[i].rkmin);
	  free(cond[i].rkmax);
	}
      free(cond);

    }	/*  END # 2  */
  return(0);
}	/*  END # 1  */


/*  User supplied hit function.  */
int
hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp)
{	/*  START # 1H  */

  register struct partition *pp;
  register struct hit *hitp;
  register struct soltab *stp;

  double d[3];		/*  used for checking tolerance of  */
			/*  adjacent regions  */
  double dist;		/*  used for finding lenght between  */
			/*  centroid & adjacent surface area  */

  /*
   * (void)fprintf(stdout,"In hit function.\n");
   * (void)fflush(stdout);
   */

  pp = PartHeadp->pt_forw;
  for( ; pp != PartHeadp; pp = pp->pt_forw)
    {	/*  START # 2H  */
      icur = pp->pt_regionp->reg_bit;	/*  Number of region hit.  */

      /*  Find hit point of entering ray.  */
      hitp = pp->pt_inhit;
      stp = pp->pt_inseg->seg_stp;
      RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
      enterpt[X] = hitp->hit_point[X];
      enterpt[Y] = hitp->hit_point[Y];
      enterpt[Z] = hitp->hit_point[Z];

      /*  Find lengths between centroids and adjacent surface areas.  */

      if(iprev >= 0)
	{	/*  START # 3H  */
	  d[X] = enterpt[X] - leavept[X];
	  if(d[X] < 0) d[X] = (-d[X]);
	  d[Y] = enterpt[Y] - leavept[Y];
	  if(d[Y] < 0) d[Y] = (-d[Y]);
	  d[Z] = enterpt[Z] - leavept[Z];
	  if(d[Z] < 0) d[Z] = (-d[Z]);

	  if( (d[X] < ADJTOL) && (d[Y] < ADJTOL) && (d[Z] < ADJTOL) )
	    {	/*  START # 4H  */
		/*  Find length for previous region. */
	      dist = ( (cond[iprev].centroid[X] - enterpt[X])
		       * (cond[iprev].centroid[X] - enterpt[X]) ) +
		( (cond[iprev].centroid[Y] - enterpt[Y])
		  * (cond[iprev].centroid[Y] - enterpt[Y]) ) +
		( (cond[iprev].centroid[Z] - enterpt[Z])
		  * (cond[iprev].centroid[Z] - enterpt[Z]) );
	      dist = sqrt(dist);
	      cond[iprev].avglen[icur] += dist;
	      cond[iprev].rmslen[icur] += (dist * dist);
	      cond[iprev].numcal[icur] += 1.;

	      if( (-ZEROTOL < cond[iprev].minlen[icur]) &&
		  (cond[iprev].minlen[icur] < ZEROTOL) )
		{
		  cond[iprev].minlen[icur] = dist;
		}
	      else if( dist < cond[iprev].minlen[icur] )
		{
		  cond[iprev].minlen[icur] = dist;
		}

	      if( (-ZEROTOL < cond[iprev].maxlen[icur]) &&
		  (cond[iprev].maxlen[icur] < ZEROTOL) )
		{
		  cond[iprev].maxlen[icur] = dist;
		}
	      else if( cond[iprev].maxlen[icur] < dist )
		{
		  cond[iprev].maxlen[icur] = dist;
		}

	      /*
	       *		(void)fprintf(stdout,"\treg#:  %d, length:  %f\n",iprev,dist);
	       *		(void)fflush(stdout);
	       */

	      /*  Find lenght for current region.  */
	      dist = ( (cond[icur].centroid[X] - enterpt[X])
		       * (cond[icur].centroid[X] - enterpt[X]) ) +
		( (cond[icur].centroid[Y] - enterpt[Y])
		  * (cond[icur].centroid[Y] - enterpt[Y]) ) +
		( (cond[icur].centroid[Z] - enterpt[Z])
		  * (cond[icur].centroid[Z] - enterpt[Z]) );
	      dist = sqrt(dist);
	      cond[icur].avglen[iprev] += dist;
	      cond[icur].rmslen[iprev] += (dist * dist);
	      cond[icur].numcal[iprev] += 1.;

	      if( (-ZEROTOL < cond[icur].minlen[iprev]) &&
		  (cond[icur].minlen[iprev] < ZEROTOL) )
		{
		  cond[icur].minlen[iprev] = dist;
		}
	      else if( dist < cond[icur].minlen[iprev] )
		{
		  cond[icur].minlen[iprev] = dist;
		}

	      if( (-ZEROTOL < cond[icur].maxlen[iprev]) &&
		  (cond[icur].maxlen[iprev] < ZEROTOL) )
		{
		  cond[icur].maxlen[iprev] = dist;
		}
	      else if( cond[icur].maxlen[iprev] < dist )
		{
		  cond[icur].maxlen[iprev] = dist;
		}

	      /*
	       *		(void)fprintf(stdout,"\treg#:  %d, length:  %f\n",icur,dist);
	       *		(void)fflush(stdout);
	       */

	    }	/*  END # 4H  */
	}	/*  END # 3H  */

      /*  Find hit point of leaving ray.  */
      hitp = pp->pt_outhit;
      stp = pp->pt_outseg->seg_stp;
      RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
      leavept[X] = hitp->hit_point[X];
      leavept[Y] = hitp->hit_point[Y];
      leavept[Z] = hitp->hit_point[Z];

      /*
       *	(void)fprintf(stdout,"current region:  %d, previous region:  %d\n",icur,iprev);
       *	(void)fprintf(stdout,"   entering pt:  %f, %f, %f\n",enterpt[X],enterpt[Y],
       *		enterpt[Z]);
       *	(void)fprintf(stdout,"   leaving pt:   %f, %f, %f\n",leavept[X],leavept[Y],
       *		leavept[Z]);
       */

      /*  Set previous region to current region.  */
      iprev = icur;
    }	/*  END # 2H  */

  return(1);

}	/*  END # 1H  */


/*  User supplied miss function.  */
int
miss(register struct application *ap_p)
{	/*  START # 1M  */

  /*
   * (void)fprintf(stdout,"In miss function.\n");
   * (void)fflush(stdout);
   */

  return(0);
}	/*  END # 1M */

/*  User supplied overlap function that does nothing.  */
int
ovrlap(register struct application *ap_p, struct partition *PartHeadp, struct region *reg1, struct region *reg2)
{
  return(1);
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
