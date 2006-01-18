/*                       F I R P A S S . C
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
/** @file firpass.c
 *
 *  Author -
 *	S.Coates - 10 February 1993
 *
 *  Source -
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *
 *  Notes -
 *
 *  This version ONLY shoots down the x-axis.
 *  This version rotates the starting point and directions.
 */

/*	CHANGES		*/
/*	10 December 1990 - 'Dimension' arrays using malloc.  */
/*	17 December 1990 - Incorperate subroutine rotate & radians.  */
/*	19 February 1991 - No defaults for material properties.  */
/*	 5 March 1991    - Creates PRISM, generic, or geometric file.  */
/*	13 March 1991    - Corrects problem writing out material.  */
/*	23 October 1991  - Writes out region # & name file.  */
/*	30 October 1991  - Make region numbering sceme the same for all  */
/*			   files, i.e. region numbers start at 1.  */
/*	 5 November 1991 - Print engine air area in radius field.  Give  */
/*			   user a choice of a PRISM 2.0 or 3.0 file.  */
/*	 3 December 1991 - Put in closed compartment surface area, exhaust  */
/*			   surface area, generic air 1 surface area, &  */
/*			   generic air 2 surface area.  */
/*	12 February 1992 - Add an additional line at the end of the facet  */
/*			   file to signify the end.  Correct second line  */
/*			   of facet file for PRISM 3.0 format.  */
/*	18 February 1992 - Write out total surface area when there is no  */
/*			   exterior surface area & if surface area is small  */
/*			   (< .001) set to .001.  This is done only in the  */
/*			   PRISM output file since PRISM will not accept a  */
/*			   a zero for exterior surface area.  It seems that  */
/*			   FRED does the same thing.  (If there are exterior  */
/*			   polygons FRED uses this for the area.  If there  */
/*			   are no exterior polygons FRED sums the area over  */
/*			   the interior polygons.)  */
/*	10 March 1992    - Print out PRISM release being used.  */
/*	10 February 1992 - Fire rays from 3 orthogonal directions, if  */
/*			   desired.  */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"


#define BLANK " "	/*  Define a blank.  */
#define ADJTOL 1.e-1	/*  Tolerance for adjacent regions.  */
#define COSTOL 1.e-20	/*  Tolerance for costheta & costheta1,  */
			/*  to avoid dividing by zero.  */
#define VOLVAR 0.1	/*  Variation in volume & surface area  */
			/*  that is allowed, before flag is set.  */
#define ZEROTOL 1.e-20	/*  Tolerance for dividing by zero.  */
#define NORMTOL 1.e-2	/*  Tolerance for finding cummulative normals.  */
#define ALPHA 25.	/*  Rotation about z-axis.  */
#define BETA 50.	/*  Rotation about y-axis.  */
#define GAMMA 35.	/*  Rotation about x-axis.  */


extern int hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp);	/*  User supplied hit function.  */
extern int miss(register struct application *ap_p);	/*  User supplied miss function.  */
extern int ovrlap(register struct application *ap_p, struct partition *PartHeadp, struct region *reg1, struct region *reg2);	/*  User supplied overlap function.  */
extern void rotate(double *p, double *a, double *np);	/*  Subroutine to rotate a point.  */
extern double radians(double a);/*  Subroutines to find angle in radians.  */

/*  Define structure.  */
struct table
{
  const char *regname;		/*  region name  */
  short mat;		/*  material code  */
  double cumnorm[3];	/*  cummulative normal vector sum  */
  /*  for the exterior free surfaces  */
  double cumvol[3];	/*  cummulative volume sums for each  */
  /*  direction fired  */
  double centroid[3];	/*  centroid calculation  */
  double cumfs[7][3];	/*  cummulative surface area for */
  /*  each air type, 0-exterior air,  */
  /*  1-crew compartment air, 2-engine  */
  /*  compartment air, 3-closed compartment  */
  /*  air, 4-exhaust air, 5-generic air 1,  */
  /*  6-generic air 2.  Air MUST be designated  */
  /*  in the following way in the mged file.  */
  /*  1 => exterior air  */
  /*  2 => crew compartment air  */
  /*  5 => engine compartment air  */
  /*  6 => closed compartment air  */
  /*  7 => exhaust air  */
  /*  8 => generic air 1  */
  /*  9 => generic air 2  */
  int *adjreg;		/*  adjacent region, 0=>no touch,  */
  /*  1=>touch  */
  double *ssurarea[3];	/*  cummulative sum of shared surface  */
  /*  area of adjacent regions  */
  double surarea[3];	/*  surface area of that region, one  */
  /*  for each direction rays fired  */
};
struct table *region;		/*  name of table structure  */
int iprev,icur;			/*  previous & current region hit  */
double area;			/*  area of "ray"  */
double leavept[3];		/*  leave point for previous region hit  */
double lnormal[3];		/*  normal for leaving ray  */
int whichview;			/*  flag for direction rays are fired  */
				/*  (0=>x, 1=>y, 2=>z)  */

struct structovr		/*  structure for recording overlaps  */
{
  int *ovrreg;		/*  regions that overlap  */
  double *ovrdep;		/*  maximum depth of overlap  */
};
struct structovr *overlaps;	/*  name of structovr structure  */

int main(int argc, char **argv)
{
  struct application ap;	/*  Structure passed between functions.  */
  int i,j,k,ii;	/*  variables used in loops  */
  int ia;		/*  variable used to set short to int  */

  int index;	/*  Index for rt_dirbuild and  */
  /*  rt_gettree.  */
  static struct rt_i *rtip;	/*  rtip is a pointer to  */
  /*  a structure of type  */
  /*  rt_i, defined in headers  */
  char idbuf[132];	/*  first ID record info, used  */
  /*  in rt_dirbuild  */
  double gridspace;	/*  variables to determine where ray starts  */
  int flag;		/*  flag for checking variance of volume &  */
  /*  surface area  */
  int num;		/*  number of regions  */
  double total;		/*  used in computing different values  */
  FILE *fp=NULL;		/*  used for writing output to file  */
  char filename[16];	/*  file name for writing output to  */
  int iwrite;		/*  0=>write to standard out, 1=>write  */
  /*  to file  */
  int typeout;		/*  type of file written, 0=>PRISM, 1=>  */
  /*  generic, 2=>geometric, 3=>non-readable  */
  /*  geometric file  */
  int typeouta;		/*  geometric type file  */
  double denom;		/*  used when computing normal  */
  FILE *fp3;		/*  used for error file  */
  char fileerr[16];	/*  used for error file  */
  FILE *fp4;		/*  used for reading material id file  */
  char fileden[16];	/*  used for reading material id file  */
  FILE *fp5;		/*  used for creating generic file  */
  char filegen[16];	/*  used for creating generic file  */
  FILE *fp6;		/*  used for creating gemetric file  */
  char filegeo[16];	/*  used for creating gemetric file  */
  FILE *fp7;		/*  Used for creating region # & name file.  */
  char filernn[16];	/*  Used for creating region # & name file.  */
  int numadjreg;		/*  used for finding the number of  */
				/*  adj regions  */
  double diagonal;	/*  Length of diagonal of bounding rpp.  */
  double xmin,xmax;	/*  Minimum & maximum x.  */
  double ymin,ymax;	/*  Minimum & maximum y.  */
  double zmin,zmax;	/*  Minimum & maximum z.  */

  /*************************************************************************/

  /*  Variables used in facet grouping file.  */
  FILE *fp1;		/*  facet grouping file  */
  char facfile[16];	/*  facet grouping file name  */
  int facnum=0;		/*  facet number  */
  char facname[25];	/*  facet name  */
  int c;			/*  used in finding facet name from  */
				/*  region name  */
  int icnt;		/*  used in finding facet name  */
  int factype;		/*  type of facet  */
  double facarea;		/*  area of facet (sq meters)  */
  double facmass;		/*  mass of facet (kg)  */
  double facspheat;	/*  specific heat of facet (J/kg deg C)  */
  double face1;		/*  emissivity of facet wrt  */
  /*  environment (>0)  */
  double face2;		/*  emissivity of facet wrt other  */
  /*  facets (>0)  */
  double facabs;		/*  absorptivity of facet  */
  double facnorm[3];	/*  surface normal of facet  */
  double faccv;		/*  convection coefficient  */
  int fack;		/*  facet number of facet seen by back  */
  /*  surface of current facet  */
  int facl;		/*  facedt number of facet seen by front  */
  /*  surface of current facet  */
  double facshape;	/*  shape factors for engine & track  */
  /*  (between 0 & 1)  */
  double facradius;	/*  hub radius (m)  */
  double facfric;		/*  bearing friction constant for wheels  */
				/*  (J)  */
  struct table1		/*  Used to read in material property file.  */
  {
    double d;		/*  Density.  */
    double sh;		/*  Specific heat.  */
    double a;		/*  Absorptivity.  */
    double e1;		/*  Emissivity.  */
    double e2;		/*  Emissivity.  */
    double tc;		/*  Thermal conductivity.  */
    char m[25];		/*  Material.  */
  };
  struct table1 matprop[41];
  char line[151];		/*  used for reading files, one line at  */
				/*  a time  */

  /*  Variables used in second pass file.  */
  FILE *fp2;		/*  second pass file  */
  char spfile[16];	/*  second pass file  */
  double spsarea;		/*  average of shared surface area for  */
				/*  second pass  */

  /************************************************************************/
  double angle[3];	/*  Angles of rotation in radians.  */
  /*  angle[0] is rotation about x-axis,  */
  /*  angle[1] is rotation about y-axis,  */
  /*  angle[2] is rotation about z-axis,  */
  double strtpt[3];	/*  Starting point of fired ray.  */
  double strtdir[3];	/*  Starting direction of fired ray.  */
  double r[3],t[3];	/*  Used in computing rotations.  */
  double center[3];	/*  Center of bounding rpp.  */
  int numext;		/*  Number of exterior surfaces.  */
  int numint;		/*  Number of interior surfaces.  */
  int numsol;		/*  Number of solar loaded surfaces.  */
  int prmrel;		/*  PRISM release number 2=>2.0 & 3=>3.0.  */
  int ifire;		/*  Number of sets of rays to be fired,  */
  /*  0=>fire from 3 orthogonal postions,  */
  /*  1=>fire from 1 postion.  */
  double diff;		/*  Difference, used in finding variance.  */

  /*  Check to see if arguments implimented correctly.  */
  if(argv[1]==NULL || argv[2]==NULL)
    {
      (void)fprintf(stderr,"\nusage:  firpass file.g objects\n\n");
    }

  else
    {


      /*  Ask if output goes to standard out or a file.  */
      (void)printf("Write output to standard out (0) or a file (1) or ");
      (void)printf("not at all (2)?  ");
      (void)fflush(stdout);
      (void)scanf("%d",&iwrite);
      if((iwrite != 0) && (iwrite != 1)) iwrite=2;
      if(iwrite == 1)
	{
	  (void)printf("Enter name of file output is to be written ");
	  (void)printf("to (15 char max).  ");
	  (void)fflush(stdout);
	  (void)scanf("%s",filename);
	  fp=fopen(filename,"w");
	}

      /*  Get error file name.  */
      (void)printf("Enter name of error file to be created ");
      (void)printf("(15 char max).  ");
      (void)fflush(stdout);
      (void)scanf("%s",fileerr);

      /*  Get second pass file name.  */
      (void)printf("Enter name of second pass file to be ");
      (void)printf("created (15 char max).  ");
      (void)fflush(stdout);
      (void)scanf("%s",spfile);

      /*  Get region # & name file (for use w/shapefact).  */
      (void)printf("Enter name of region # & name file to be  ");
      (void)printf("created (15 char max).  ");
      (void)fflush(stdout);
      (void)scanf("%s",filernn);

      /*  Get name of material id file.  */
      (void)printf("Enter name of material id file to be read ");
      (void)printf("(15 char max).  ");
      (void)fflush(stdout);
      (void)scanf("%s",fileden);

      /*  What types of files are to be written?  */
      (void)printf("Enter type of file to be written.\n");
      (void)printf("\t0 - PRISM file\n");
      (void)printf("\t1 - Generic file\n");
      (void)printf("\t2 - Geometric properties file\n");
      (void)fflush(stdout);
      (void)scanf("%d",&typeout);

      /*  Get facet file name.  */
      if( typeout == 0 )
	{
	  (void)printf("Enter name of facet file to be created. ");
	  (void)printf("(15 char max)  ");
	  (void)fflush(stdout);
	  (void)scanf("%s",facfile);

	  /*  Find which PRISM release is being used.  The facet number  */
	  /*  in release 3.0 is written with an I6 & in release 2.0 it  */
	  /*  is written with I3.  */
	  prmrel = 2;
	  (void)printf("Which release of PRISM is being used, 2.0 (2) ");
	  (void)printf("or 3.0 (3)?  ");
	  (void)fflush(stdout);
	  (void)scanf("%d",&prmrel);
	  if(prmrel != 3) prmrel = 2;
	}

      /*  Get generic file name.  */
      if( typeout == 1 )
	{
	  (void)printf("Enter name of generic file to be created. ");
	  (void)printf("(15 char max)  ");
	  (void)fflush(stdout);
	  (void)scanf("%s",filegen);
	}

      /*  Get geometric file name.  */
      if( typeout == 2 )
	{
	  (void)printf("Do you want a readable (0) or non-readable (1) ");
	  (void)printf("geometric file?  ");
	  (void)fflush(stdout);
	  (void)scanf("%d",&typeouta);
	  typeout += typeouta;
	  (void)printf("Enter name of geometric properties file to be ");
	  (void)printf("created (15 char max).  ");
	  (void)fflush(stdout);
	  (void)scanf("%s",filegeo);
	}

      /*
       *	(void)printf("typeout = %d\n",typeout);
       *	(void)fflush(stdout);
       */

      /*  Determine whether to fire one set of rays or to fire  */
      /*  three sets of orthogonal rays.  */
      (void)printf("Should there be 3 sets of orthogonal rays fired ");
      (void)printf("(0) or 1 set\n");
      (void)printf("of rays fired (1)?\n\t");
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

      /*  Open & read material id file.  */
      fp4=fopen(fileden,"r");
      /*
       *	(void)printf("Materail id file open for reading.\n");
       *	(void)fflush(stdout);
       */
      /*  Assumption is made that material ids run from 0 to 40  */
      /*  and ALL numbers 0-40 have a density.  */

      for(i=0; i<41; i++)
	{
	  (void)fgets(line,150,fp4);
	  (void)sscanf(line,"%*d%lf%lf%lf%lf%lf%25c",
		       &matprop[i].d,&matprop[i].sh,&matprop[i].a,
		       &matprop[i].e1,&matprop[i].tc,matprop[i].m);
	  matprop[i].e2 = matprop[i].e1;
	  for(j=0; j<25; j++)
	    {
	      if(matprop[i].m[j] == '\n')
		{
		  for(ii=j; ii<25; ii++)
		    {
		      matprop[i].m[ii] = ' ';
		    }
		}
	    }
	  /*  Substitute blanks for tabs in material name.  */
	  for(j=0; j<25; j++)
	    {
	      if(matprop[i].m[j] == '\t')
		matprop[i].m[j] = ' ';
	    }

	  /*
	   *		(void)printf("%d, %f, %f, %f, %f, %f\n",i,
	   *		   matprop[i].d,matprop[i].sh,matprop[i].a,
	   *		   matprop[i].e1,matprop[i].e2);
	   *		(void)fflush(stdout);
	   */

	}
      (void)fclose(fp4);

      /*  Print out the name of the file being used.  */
      (void)printf("File Used:  %s\n",argv[1]);
      (void)fflush(stdout);
      if(iwrite == 1)
	{
	  (void)fprintf(fp,"File Used:  %s,  ",argv[1]);
	  (void)fflush(fp);
	}

      /*  Print out name of material id file being used.  */
      (void)printf("Material ID File:  %s\n",fileden);
      (void)fflush(stdout);
      if(iwrite == 1)
	{
	  (void)fprintf(fp,"Material ID File:  %s\n",fileden);
	  (void)fflush(fp);
	}

      /*  Build the directory.  */
      index = 1;	/*  Setup index for rt_dirbuild  */
      rtip=rt_dirbuild(argv[index],idbuf,sizeof(idbuf));
      (void)printf("Database Title:  %s\n",idbuf);
      (void)fflush(stdout);
      if(iwrite == 1)
	{
	  (void)fprintf(fp,"Database Title:  %s\n",idbuf);
	  (void)fflush(fp);
	}

      /*  Set useair to 1, to show hits of air.  */
      rtip->useair = 1;

      /*  Load desired objects.  */
      index = 2;	/*  Set index for rt_gettree.  */
      while(argv[index] != NULL)
	{
	  rt_gettree(rtip,argv[index]);
	  index=index+1;
	}

      /*  Find total number of regions.  */
      num = (int)rtip->nregions;
      /*
       *	(void)printf("\n Number of Regions? = %d\n",num);
       *	(void)fflush(stdout);
       */

      /*  The number of regions (num) is now known.  It  */
      /*  is time to use the malloc command to 'dimension'  */
      /*  the appropriate arrays and structures.  */

      (void)printf("Mallocing arrays.\n");
      (void)fflush(stdout);

      region = (struct table *)malloc(num * sizeof (*region) );
      overlaps = (struct structovr *)malloc(num * sizeof (*overlaps) );

      for(i=0; i<num; i++)
	{
	  region[i].adjreg = (int *)malloc(num * sizeof (int) );
	  region[i].ssurarea[0] = (double *)malloc(num * sizeof (double) );
	  region[i].ssurarea[1] = (double *)malloc(num * sizeof (double) );
	  region[i].ssurarea[2] = (double *)malloc(num * sizeof (double) );

	  overlaps[i].ovrreg = (int *)malloc(num * sizeof (int) );
	  overlaps[i].ovrdep = (double *)malloc(num * sizeof (double) );
	}

      /*  Now that the arrays are 'dimensioned' zero ALL variables.  */

      (void)printf("Zeroing variables.\n");
      (void)fflush(stdout);

      for(i=0; i<num; i++)
	{
	  region[i].regname="\0";
	  region[i].cumnorm[0]=0.;
	  region[i].cumnorm[1]=0.;
	  region[i].cumnorm[2]=0.;
	  region[i].cumvol[0]=0.;
	  region[i].cumvol[1]=0.;
	  region[i].cumvol[2]=0.;
	  region[i].centroid[0]=0.;
	  region[i].centroid[1]=0.;
	  region[i].centroid[2]=0.;
	  for(k=0; k<7; k++)
	    {
	      region[i].cumfs[k][0] = 0.;
	      region[i].cumfs[k][1] = 0.;
	      region[i].cumfs[k][2] = 0.;
	    }
	  region[i].surarea[0]=0.;
	  region[i].surarea[1]=0.;
	  region[i].surarea[2]=0.;
	  for(j=0; j<num; j++)
	    {
	      region[i].adjreg[j]=0;
	      region[i].ssurarea[0][j]=0.;
	      region[i].ssurarea[1][j]=0.;
	      region[i].ssurarea[2][j]=0.;
	    }
	}

      /*  Zero overlaps.  */
      for(i=0; i<num; i++)
	{
	  for(j=0; j<num; j++)
	    {
	      overlaps[i].ovrreg[j] = 0;
	      overlaps[i].ovrdep[j] = 0.;
	    }
	}

      /*  Get database ready by starting preparation.  */
      rt_prep(rtip);

      /*  Find center of bounding rpp.  */
      center[X] = rtip->mdl_min[X] + (rtip->mdl_max[X]
				      - rtip->mdl_min[X]) / 2.;
      center[Y] = rtip->mdl_min[Y] + (rtip->mdl_max[Y]
				      - rtip->mdl_min[Y]) / 2.;
      center[Z] = rtip->mdl_min[Z] + (rtip->mdl_max[Z]
				      - rtip->mdl_min[Z]) / 2.;

      /*  Find length of diagonal of bounding rpp.  */
      diagonal = (rtip->mdl_max[X] - rtip->mdl_min[X])
	* (rtip->mdl_max[X] - rtip->mdl_min[X])
	+ (rtip->mdl_max[Y] - rtip->mdl_min[Y])
	* (rtip->mdl_max[Y] - rtip->mdl_min[Y])
	+ (rtip->mdl_max[Z] - rtip->mdl_min[Z])
	* (rtip->mdl_max[Z] - rtip->mdl_min[Z]);
      diagonal = sqrt(diagonal) / 2. + 0.5;

      /*  Find minimum and maximums.  */
      xmin = center[X] - diagonal;
      xmax = center[X] + diagonal;
      ymin = center[Y] - diagonal;
      ymax = center[Y] + diagonal;
      zmin = center[Z] - diagonal;
      zmax = center[Z] + diagonal;

      /*  Print center of bounding rpp, diagonal, maximum, &  */
      /*  minimum.  */
      (void)printf("Center of bounding rpp ( %f, %f, %f )\n",
		   center[X],center[Y],center[Z]);
      (void)printf("Length of diagonal of bounding rpp:  %f\n",
		   diagonal);
      (void)printf("Minimums & maximums\n");
      (void)printf("  x:  %f - %f\n",xmin,xmax);
      (void)printf("  y:  %f - %f\n",ymin,ymax);
      (void)printf("  z:  %f - %f\n",zmin,zmax);
      (void)fflush(stdout);

      /*  Write model minimum & maximum.  */
      (void)printf("Model minimum & maximum.\n");
      (void)fflush(stdout);
      (void)printf("\tX:  %f to %f\n\tY:  %f to %f\n\tZ:  %f to %f\n\n",
		   rtip->mdl_min[X],rtip->mdl_max[X],
		   rtip->mdl_min[Y],rtip->mdl_max[Y],
		   rtip->mdl_min[Z],rtip->mdl_max[Z]);
      (void)fflush(stdout);
      if(iwrite == 1)
	{
	  (void)fprintf(fp,"Model minimum & maximum.\n");
	  (void)fprintf(fp,"\tX:  %f to %f\n\tY:  %f to %f\n\tZ:  %f to %f\n",
			rtip->mdl_min[X],rtip->mdl_max[X],
			rtip->mdl_min[Y],rtip->mdl_max[Y],
			rtip->mdl_min[Z],rtip->mdl_max[Z]);
	  (void)fflush(fp);
	}

      /*  User enters grid spacing.  All units are in mm.  */
      (void)printf("Enter grid spacing (mm) for fired rays.\n");
      (void)fflush(stdout);
      (void)scanf("%lf",&gridspace);

      if(iwrite == 0)
	{
	  (void)printf("grid spacing:  %f\n",gridspace);
	  (void)fflush(stdout);
	}
      else if(iwrite == 1)
	{
	  (void)fprintf(fp,"grid spacing:  %f,  ",gridspace);
	  (void)fflush(fp);
	}

      /*  Find area of "ray".  */
      area = gridspace * gridspace;
      if(iwrite == 0)
	{
	  (void)printf("area=%f\n",area);
	  (void)fflush(stdout);
	}
      if(iwrite == 1)
	{
	  (void)fprintf(fp,"area=%f\n",area);
	  (void)fflush(fp);
	}

      /*  Set up other parameters for rt_shootray.  */
      RT_APPLICATION_INIT(&ap);
      ap.a_hit=hit;	/*  User supplied hit function.  */
      ap.a_miss=miss;	/*  User supplied miss function.  */
      ap.a_overlap=ovrlap;	/*  User supplied overlap function.  */

      /*  Set other parameters that need to be  */
      /*  set for rt_shootray.  */
      ap.a_rt_i=rtip;	/*  Pointer from rt_dirbuild.  */
      ap.a_onehit=0;	/*  Hit flag:  return all hits.  */
      ap.a_level=0;	/*  Recursion level for diagnostics  */
      ap.a_resource=0;	/*  address of resource structure:  */
      /*  NULL  */

      /*  Set up for 1st set of fired rays, positive to negative  */
      /*  (shooting down the x-axis).  */

      (void)printf("\nSHOOTING DOWN THE 1st AXIS\n");
      (void)fflush(stdout);

      whichview=0;

      strtpt[X] = xmax;
      strtpt[Y] = ymin + gridspace / 2.;
      strtpt[Z] = zmin + gridspace / 2.;
      strtdir[X] = (-1.);
      strtdir[Y] = 0.;
      strtdir[Z] = 0.;

      /*  Put angle into radians.  */
      angle[X] = radians((double)GAMMA);
      angle[Y] = radians((double)BETA);
      angle[Z] = radians((double)ALPHA);

      /*  Rotate starting point.  (new pt = C + R[P - C])  */
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

      while( strtpt[Z] <= zmax )
	{

	  /*  Set to show no previous shots.  */
	  iprev=(-99);
	  r[X]=xmax - center[X] + 5.;
	  r[Y]=ymax - center[Y] + 5.;
	  r[Z]=zmax - center[Z] + 5.;


	  (void)rotate(r,angle,t);

	  leavept[X] = center[X] + t[X];
	  leavept[Y] = center[Y] + t[Y];
	  leavept[Z] = center[Z] + t[Z];

	  /*  Print starting point & direction.  */
	  /*
	   *		(void)printf("Starting point & direction.\n");
	   *		(void)printf("  %f, %f, %f\n  %f, %f, %f\n",
	   *		   ap.a_ray.r_pt[X],ap.a_ray.r_pt[Y],ap.a_ray.r_pt[Z],
	   *		   ap.a_ray.r_dir[X],ap.a_ray.r_dir[Y],ap.a_ray.r_dir[Z]);
	   *		(void)fflush(stdout);
	   */

	  lnormal[X]=0.;
	  lnormal[Y]=0.;
	  lnormal[Z]=0.;

	  /*  Call rt_shootray.  */
	  (void)rt_shootray( &ap );

	  strtpt[Y] += gridspace;
	  if( strtpt[Y] > ymax )
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
	}

      /*  Set up to fire 2nd & 3rd set of rays if appropriate.  */
      if(ifire == 0)
	{						/*  START # 1000  */
	  /*  Set up & fire 2nd set of arrays.  */
	  (void)printf("\nSHOOTING DOWN THE 2nd AXIS\n");
	  (void)fflush(stdout);

	  whichview = 1;

	  strtpt[X] = xmin + gridspace / 2.;
	  strtpt[Y] = ymax;
	  strtpt[Z] = xmin + gridspace / 2.;
	  strtdir[X] = 0.;
	  strtdir[Y] = (-1.);
	  strtdir[Z] = 0.;

	  /*  Angle already in radians.  */

	  /*  Rotate starting point (new pt = C + R[P - C]).  */
	  t[X] = strtpt[X] - center[X];
	  t[Y] = strtpt[Y] - center[Y];
	  t[Z] = strtpt[Z] - center[Z];

	  (void)rotate(t,angle,r);

	  ap.a_ray.r_pt[X] = center[X] + r[X];
	  ap.a_ray.r_pt[Y] = center[Y] + r[Y];
	  ap.a_ray.r_pt[Z] = center[Z] + r[Z];

	  /*  Rotate firing direction (new dir = R[D]).  */
	  (void)rotate(strtdir,angle,r);
	  ap.a_ray.r_dir[X] = r[X];
	  ap.a_ray.r_dir[Y] = r[Y];
	  ap.a_ray.r_dir[Z] = r[Z];

	  while(strtpt[Z] <= zmax)
	    {
	      /*  Set to show no previous shots.  */
	      iprev = (-99);
	      r[X] = xmax - center[X] + 5.;
	      r[Y] = ymax - center[Y] + 5.;
	      r[Z] = zmax - center[Z] + 5.;

	      (void)rotate(r,angle,t);

	      leavept[X] = center[X] + t[X];
	      leavept[Y] = center[Y] + t[Y];
	      leavept[Z] = center[Z] + t[Z];

	      lnormal[X] = 0.;
	      lnormal[Y] = 0.;
	      lnormal[Z] = 0.;

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
	    }

	  /*  Set up & fire 3rd set of rays.  */
	  (void)printf("\nSHOOTING DOWN THE 3rd AXIS.\n");
	  (void)fflush(stdout);

	  whichview = 2;

	  strtpt[X] = xmin + gridspace / 2.;
	  strtpt[Y] = ymin + gridspace / 2.;
	  strtpt[Z] = zmax;
	  strtdir[X] = 0.;
	  strtdir[Y] = 0.;
	  strtdir[Z] = (-1.);

	  /*  Angle already in radians.  */

	  /*  Rotate starting point (new pt = C + R[P - C]).  */
	  t[X] = strtpt[X] - center[X];
	  t[Y] = strtpt[Y] - center[Y];
	  t[Z] = strtpt[Z] - center[Z];

	  (void)rotate(t,angle,r);

	  ap.a_ray.r_pt[X] = center[X] + r[X];
	  ap.a_ray.r_pt[Y] = center[Y] + r[Y];
	  ap.a_ray.r_pt[Z] = center[Z] + r[Z];

	  /*  Rotate firing direction (new dir = R[D]).  */
	  (void)rotate(strtdir,angle,r);

	  ap.a_ray.r_dir[X] = r[X];
	  ap.a_ray.r_dir[Y] = r[Y];
	  ap.a_ray.r_dir[Z] = r[Z];

	  while(strtpt[Y] <= ymax)
	    {
	      /*  Set to show no previous shots.  */
	      iprev = (-99);
	      r[X] = xmax - center[X] + 5.;
	      r[Y] = xmax - center[Y] + 5.;
	      r[Z] = xmax - center[Z] + 5.;

	      (void)rotate(r,angle,t);

	      leavept[X] = center[X] + t[X];
	      leavept[Y] = center[Y] + t[Y];
	      leavept[Z] = center[Z] + t[Z];

	      lnormal[X] = 0.;
	      lnormal[Y] = 0.;
	      lnormal[Z] = 0.;

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
	    }

	}						/*  END # 1000  */

      if(iwrite == 0)
	{
	  (void)printf("\nNumber of regions:  %d\n\n",num);
	  (void)fflush(stdout);
	}
      if(iwrite == 1)
	{
	  (void)fprintf(fp,"Number of regions:  %d\n",num);
	  (void)fflush(fp);
	}

      /*  Compute the volume & surface area of each region.  */
      i=0;
      while( i < num )
	{
	  /*  Finish computing centroids.  */
	  /*
	   *		(void)printf("\tcentroid before division:  %f, %f, %f\n",
	   *		   region[i].centroid[0],region[i].centroid[1],
	   *		   region[i].centroid[2]);
	   *		(void)printf("\tvolumes:  %f, %f, %f\n",region[i].cumvol[0],
	   *		   region[i].cumvol[1],region[i].cumvol[2]);
	   *		(void)fflush(stdout);
	   */
	  total = region[i].cumvol[0] + region[i].cumvol[1] +
	    region[i].cumvol[2];

	  /*
	   *		(void)printf("\tcummulative volume:  %f",total);
	   *		(void)fflush(stdout);
	   */
	  if( (total < -ZEROTOL) || (ZEROTOL < total) )
	    {
	      region[i].centroid[X] = region[i].centroid[X]/total;
	      region[i].centroid[Y] = region[i].centroid[Y]/total;
	      region[i].centroid[Z] = region[i].centroid[Z]/total;
	    }
	  /*
	   *		(void)printf("volumes:  %f, %f, %f\n",region[i].cumvol[0],
	   *		   region[i].cumvol[1],region[i].cumvol[2]);
	   *		(void)printf("areas:  %f, %f, %f\n",region[i].surarea[0],
	   *		   region[i].surarea[1],region[i].surarea[2]);
	   */

	  /*  Check for variance of volume & find volume.  */
	  flag=0;
	  if(ifire == 0)
	    {					/*  START # 1040  */
	      if( (region[i].cumvol[0] != 0.) &&
		  (region[i].cumvol[1] != 0.) &&
		  (region[i].cumvol[2] != 0.) )
		{					/*  START # 1045  */
		  diff = region[i].cumvol[0] - region[i].cumvol[1];
		  if(diff < 0.) diff = (-diff);
		  if( (diff / region[i].cumvol[0]) > VOLVAR) flag = 1;
		  if( (diff / region[i].cumvol[1]) > VOLVAR) flag = 1;

		  diff = region[i].cumvol[0] - region[i].cumvol[2];
		  if(diff < 0.) diff = (-diff);
		  if( (diff / region[i].cumvol[0]) > VOLVAR) flag = 1;
		  if( (diff / region[i].cumvol[2]) > VOLVAR) flag = 1;

		  diff = region[i].cumvol[1] - region[i].cumvol[2];
		  if(diff < 0.) diff = (-diff);
		  if( (diff / region[i].cumvol[1]) > VOLVAR) flag = 1;
		  if( (diff / region[i].cumvol[2]) > VOLVAR) flag = 1;
		}					/*  END # 1045  */

	      else if( (region[i].cumvol[0] == 0.) ||
		       (region[i].cumvol[1] == 0.) ||
		       (region[i].cumvol[2] == 0.) )
		{
		  flag = 2;
		}

	      /*  Put in check.  */
	      /*
	       *		   if(flag == 1)
	       *		   {
	       *			(void)printf("** Vol Var exceeded for region %d:  ",
	       *				i);
	       *			(void)printf("%f, %f, %f **\n",region[i].cumvol[0],
	       *				region[i].cumvol[1],region[i].cumvol[2]);
	       *			(void)fflush(stdout);
	       *		   }
	       *		   else
	       *		   {
	       *			(void)printf("** Vol Var NOT exceeded for region %d:  ",
	       *				i);
	       *			(void)printf("%f, %f, %f **\n",region[i].cumvol[0],
	       *				region[i].cumvol[1],region[i].cumvol[2]);
	       *			(void)fflush(stdout);
	       *		   }
	       */
	    }					/*  END # 1040  */

	  if(ifire == 0)
	    {
	      region[i].cumvol[0] = region[i].cumvol[0]
		+ region[i].cumvol[1] + region[i].cumvol[2];
	      region[i].cumvol[0] /= 3.;
	    }
	  region[i].cumvol[1]=(double)flag;

	  /*  Check for variance of surface area & find  */
	  /*  surface area.  */
	  flag=0;
	  if(ifire == 0)
	    {					/*  START # 1050  */
	      if( (region[i].surarea[0] != 0.) &&
		  (region[i].surarea[1] != 0.) &&
		  (region[i].surarea[2] != 0.) )
		{					/*  START # 1055  */
		  diff = region[i].surarea[0] - region[i].surarea[1];
		  if(diff < 0.) diff = (-diff);
		  if( (diff / region[i].surarea[0]) > VOLVAR) flag = 1;
		  if( (diff / region[i].surarea[1]) > VOLVAR) flag = 1;

		  diff = region[i].surarea[0] - region[i].surarea[2];
		  if(diff < 0.) diff = (-diff);
		  if( (diff / region[i].surarea[0]) > VOLVAR) flag = 1;
		  if( (diff / region[i].surarea[2]) > VOLVAR) flag = 1;

		  diff = region[i].surarea[1] - region[i].surarea[2];
		  if(diff < 0.) diff = (-diff);
		  if( (diff / region[i].surarea[1]) > VOLVAR) flag = 1;
		  if( (diff / region[i].surarea[2]) > VOLVAR) flag = 1;
		}					/*  END # 1055  */

	      else if( (region[i].surarea[0] == 0.) ||
		       (region[i].surarea[1] == 0.) ||
		       (region[i].surarea[2] == 0.) )
		{
		  flag = 2;
		}

	      /*  Put in check.  */
	      /*
	       *		   if(flag == 1)
	       *		   {
	       *			(void)printf("** Sur Area Var exceeded for ");
	       *			(void)printf("region %d:  ",i);
	       *			(void)printf("%f, %f, %f **\n",region[i].surarea[0],
	       *				region[i].surarea[1],region[i].surarea[2]);
	       *			(void)fflush(stdout);
	       *		   }
	       *		   else
	       *		   {
	       *			(void)printf("** Sur Area Var NOT exceeded for ");
	       *			(void)printf("region %d:  ",i);
	       *			(void)printf("%f, %f, %f **\n",region[i].surarea[0],
	       *				region[i].surarea[1],region[i].surarea[2]);
	       *			(void)fflush(stdout);
	       *		   }
	       */
	    }					/*  END # 1050  */
	  if(ifire == 0)
	    {
	      region[i].surarea[0] = region[i].surarea[0]
		+ region[i].surarea[1] + region[i].surarea[2];
	      region[i].surarea[0] /= 3.;
	    }
	  region[i].surarea[1]=(double)flag;

	  /*  Check for variance of shared surface area &  */
	  /*  find shared surface area.  */
	  for(j=0; j<num; j++)
	    {
	      if(region[i].adjreg[j] == 1)
		{

		  /*
		   *			(void)printf("reg %d, adj reg %d\n",i,j);
		   *			(void)printf("  shared area:  %f, %f, %f\n",
		   *			   region[i].ssurarea[0][j],
		   *			   region[i].ssurarea[1][j],
		   *			   region[i].ssurarea[2][j]);
		   *			(void)fflush(stdout);
		   */
		  flag = 0;
		  if(ifire == 0)
		    {				/*  START # 1060  */
		      if( (region[i].ssurarea[0][j] != 0.) &&
			  (region[i].ssurarea[1][j] != 0.) &&
			  (region[i].ssurarea[2][j] != 0.) )
			{				/*  START # 1065  */
			  diff = region[i].ssurarea[0][j]
			    - region[i].ssurarea[1][j];
			  if(diff < 0.) diff = (-diff);
			  if( (diff / region[i].ssurarea[0][j]) > VOLVAR)
			    flag = 1;
			  if( (diff / region[i].ssurarea[1][j]) > VOLVAR)
			    flag = 1;

			  diff = region[i].ssurarea[0][j]
			    - region[i].ssurarea[2][j];
			  if(diff < 0.) diff = (-diff);
			  if( (diff / region[i].ssurarea[0][j]) > VOLVAR)
			    flag = 1;
			  if( (diff / region[i].ssurarea[2][j]) > VOLVAR)
			    flag = 1;

			  diff = region[i].ssurarea[1][j]
			    - region[i].ssurarea[2][j];
			  if(diff < 0.) diff = (-diff);
			  if( (diff / region[i].ssurarea[1][j]) > VOLVAR)
			    flag = 1;
			  if( (diff / region[i].ssurarea[2][j]) > VOLVAR)
			    flag = 1;
			}				/*  END # 1065  */

		      else if( (region[i].ssurarea[0][j] == 0.) ||
			       (region[i].ssurarea[1][j] == 0.) ||
			       (region[i].ssurarea[2][j] == 0.) )
			{
			  flag = 2;
			}

		      /*  Put in check.  */
		      /*
		       *			   if(flag == 1)
		       *			   {
		       *				(void)printf("** Shar Sur Area Var exceeded ");
		       *				(void)printf("for region %d, %d:  ",i,j);
		       *				(void)printf("%f, %f, %f **\n",
		       *					region[i].ssurarea[0][j],
		       *					region[i].ssurarea[1][j],
		       *					region[i].ssurarea[2][j]);
		       *				(void)fflush(stdout);
		       *			   }
		       *			   else
		       *			   {
		       *				(void)printf("** Shar Sur Area Var NOT exceed");
		       *				(void)printf("ed for region %d,%d:  ",i,j);
		       *				(void)printf("%f, %f, %f **\n",
		       *					region[i].ssurarea[0][j],
		       *					region[i].ssurarea[1][j],
		       *					region[i].ssurarea[2][j]);
		       *				(void)fflush(stdout);
		       *			   }
		       */
		    }				/*  END # 1060  */
		  if(ifire == 0)
		    {
		      region[i].ssurarea[0][j] = region[i].ssurarea[0][j]
			+ region[i].ssurarea[1][j]
			+ region[i].ssurarea[2][j];
		      region[i].ssurarea[0][j] /= 3.;
		    }
		  region[i].ssurarea[1][j] = (double)flag;
		}
	    }

	  /*  Check for variance of all air areas (exterior,  */
	  /*  crew, engine, closed compartment, exhaust, generic  */
	  /*  1 & generic 2) & find area.  */
	  for(k=0; k<7; k++)
	    {					/*  START # 1070  */
	      flag = 0;
	      if(ifire == 0)
		{					/*  START # 1080  */
		  if( (region[i].cumfs[k][0] != 0.) &&
		      (region[i].cumfs[k][1] != 0.) &&
		      (region[i].cumfs[k][2] != 0.) )
		    {				/*  START # 1090  */
		      diff = region[i].cumfs[k][0]
			- region[i].cumfs[k][1];
		      if(diff < 0.) diff = (-diff);
		      if( (diff / region[i].cumfs[k][0]) > VOLVAR)
			flag = 1;
		      if( (diff / region[i].cumfs[k][1]) > VOLVAR)
			flag = 1;

		      diff = region[i].cumfs[k][0]
			- region[i].cumfs[k][2];
		      if(diff < 0.) diff = (-diff);
		      if( (diff / region[i].cumfs[k][0]) > VOLVAR)
			flag = 1;
		      if( (diff / region[i].cumfs[k][2]) > VOLVAR)
			flag = 1;

		      diff = region[i].cumfs[k][1]
			- region[i].cumfs[k][2];
		      if(diff < 0.) diff = (-diff);
		      if( (diff / region[i].cumfs[k][1]) > VOLVAR)
			flag = 1;
		      if( (diff / region[i].cumfs[k][2]) > VOLVAR)
			flag = 1;
		    }				/*  END # 1090  */

		  else if( (region[i].cumfs[k][0] == 0.) ||
			   (region[i].cumfs[k][1] == 0.) ||
			   (region[i].cumfs[k][2] == 0.) )
		    {
		      flag = 2;
		    }
		}					/*  END # 1080  */

	      if(ifire == 0)
		{
		  region[i].cumfs[k][0] = region[i].cumfs[k][0] +
		    region[i].cumfs[k][1] +
		    region[i].cumfs[k][2];
		  region[i].cumfs[k][0] /= 3.;
		}

	      region[i].cumfs[k][1] = (double)flag;
	    }					/*  END # 1070  */

	  /*  Finish finding cummulative normal of exterior  */
	  /*  free surface.  */

	  /*  Print out normal before normalizing.  */
	  /*
	   *		(void)printf("Normal before normalizing\n");
	   *		(void)printf("  %f, %f, %f\n",region[i].cumnorm[X],
	   *			region[i].cumnorm[Y],region[i].cumnorm[Z]);
	   *		(void)fflush(stdout);
	   */

	  if( ( (-NORMTOL < region[i].cumnorm[X]) &&
		(region[i].cumnorm[X] < NORMTOL) ) &&
	      ( (-NORMTOL < region[i].cumnorm[Y]) &&
		(region[i].cumnorm[Y] < NORMTOL) ) &&
	      ( (-NORMTOL < region[i].cumnorm[Z]) &&
		(region[i].cumnorm[Z] < NORMTOL) ) )
	    {
	      region[i].cumnorm[X] = 0.;
	      region[i].cumnorm[Y] = 0.;
	      region[i].cumnorm[Z] = 0.;
	    }

	  else
	    {
	      denom = sqrt( region[i].cumnorm[X] * region[i].cumnorm[X] +
			    region[i].cumnorm[Y] * region[i].cumnorm[Y] +
			    region[i].cumnorm[Z] * region[i].cumnorm[Z]);
	      if(denom > ZEROTOL)
		{
		  region[i].cumnorm[X] /= denom;
		  region[i].cumnorm[Y] /= denom;
		  region[i].cumnorm[Z] /= denom;
		}
	      else
		{
		  region[i].cumnorm[X] = 0.;
		  region[i].cumnorm[Y] = 0.;
		  region[i].cumnorm[Z] = 0.;
		}
	    }

	  i++;
	}

      if(iwrite == 0)
	{ (void)printf("\n\n\nPRINT OUT STRUCTURE\n");
	(void)fflush(stdout);
	i=0;
	while( i < num )
	  {
	    (void)printf("region #:  %d, name:  %s\n",(i+1),
			 region[i].regname);
	    (void)printf("\tmaterial code:  %d\n",region[i].mat);
	    (void)fflush(stdout);

	    if(region[i].cumvol[1] == 1.)
	      {
		(void)printf("\tvolume:  %f - difference is above",
			     region[i].cumvol[0]);
		(void)printf(" %f variance.\n",VOLVAR);
		(void)fflush(stdout);
	      }
	    else
	      {
		(void)printf("\t volume:  %f - within variance.\n",
			     region[i].cumvol[0]);
		(void)fflush(stdout);
	      }

	    if(region[i].surarea[1] == 1.)
	      {
		(void)printf("\tarea:  %f - difference is above",
			     region[i].surarea[0]);
		(void)printf(" %f variance.\n",VOLVAR);
		(void)fflush(stdout);
	      }
	    else
	      {
		(void)printf("\t area:  %f - within variance.\n",
			     region[i].surarea[0]);
		(void)fflush(stdout);
	      }

	    (void)printf("\tcentroid:  %f, %f, %f\n",region[i].centroid[0],
			 region[i].centroid[1],region[i].centroid[2]);
	    (void)fflush(stdout);
	    (void)printf("\tcummulative normal of the exterior ");
	    (void)printf("free surface:\n\t\t%f, %f, %f\n",
			 region[i].cumnorm[X],region[i].cumnorm[Y],
			 region[i].cumnorm[Z]);
	    (void)fflush(stdout);
	    (void)printf("\text sur air:  %f",region[i].cumfs[0][0]);
	    if(region[i].cumfs[0][1] == 0.) (void)printf(" - ok\n");
	    if(region[i].cumfs[0][1] == 1.) (void)printf(" - not ok\n");
	    if(region[i].cumfs[0][1] == 2.) (void)printf(" - none\n");
	    (void)printf("\tcrew comp air:  %f",region[i].cumfs[1][0]);
	    if(region[i].cumfs[1][1] == 0.) (void)printf(" - ok\n");
	    if(region[i].cumfs[1][1] == 1.) (void)printf(" - not ok\n");
	    if(region[i].cumfs[1][1] == 2.) (void)printf(" - none\n");
	    (void)printf("\teng comp air:  %f",region[i].cumfs[2][0]);
	    if(region[i].cumfs[2][1] == 0.) (void)printf(" - ok\n");
	    if(region[i].cumfs[2][1] == 1.) (void)printf(" - not ok\n");
	    if(region[i].cumfs[2][1] == 2.) (void)printf(" - none\n");
	    (void)printf("\tclosed comp air:  %f",region[i].cumfs[3][0]);
	    if(region[i].cumfs[3][1] == 0.) (void)printf(" - ok\n");
	    if(region[i].cumfs[3][1] == 1.) (void)printf(" - not ok\n");
	    if(region[i].cumfs[3][1] == 2.) (void)printf(" - none\n");
	    (void)printf("\texhaust air:  %f",region[i].cumfs[4][0]);
	    if(region[i].cumfs[4][1] == 0.) (void)printf(" - ok\n");
	    if(region[i].cumfs[4][1] == 1.) (void)printf(" - not ok\n");
	    if(region[i].cumfs[4][1] == 2.) (void)printf(" - none\n");
	    (void)printf("\tgen air 1:  %f",region[i].cumfs[5][0]);
	    if(region[i].cumfs[5][1] == 0.) (void)printf(" - ok\n");
	    if(region[i].cumfs[5][1] == 1.) (void)printf(" - not ok\n");
	    if(region[i].cumfs[5][1] == 2.) (void)printf(" - none\n");
	    (void)printf("\tgen air 2:  %f",region[i].cumfs[6][0]);
	    if(region[i].cumfs[6][1] == 0.) (void)printf(" - ok\n");
	    if(region[i].cumfs[6][1] == 1.) (void)printf(" - not ok\n");
	    if(region[i].cumfs[6][1] == 2.) (void)printf(" - none\n");
	    (void)fflush(stdout);
	    for(j=0; j<num; j++)
	      {
		if(region[i].adjreg[j] == 1)
		  {
		    (void)printf("\tadjreg[%d]=%d, ",
				 (j+1),region[i].adjreg[j]);
		    (void)printf("shared surface area:  %f\n",
				 region[i].ssurarea[0][j]);
		    (void)fflush(stdout);
		    if(region[i].ssurarea[1][j] == 1.)
		      {
			(void)printf("\tdifference is above %f variance\n",
				     VOLVAR);
			(void)fflush(stdout);
		      }
		    else
		      {
			(void)printf("\twithin variance\n");
			(void)fflush(stdout);
		      }
		  }
	      }
	    i++;
	  } }
      if(iwrite == 1)
	{ i=0;
	while( i < num )
	  {
	    (void)fprintf(fp,"region #:  %d, name:  %s\n",
			  (i+1),region[i].regname);
	    (void)fprintf(fp,"\tmaterial code:  %d\n",region[i].mat);

	    if(region[i].cumvol[1] == 1.)
	      {
		(void)fprintf(fp,"\tvolume:  %f - difference is above",
			      region[i].cumvol[0]);
		(void)fprintf(fp," %f variance.\n",VOLVAR);
		(void)fflush(fp);
	      }
	    else
	      {
		(void)fprintf(fp,"\t volume:  %f - within variance.\n",
			      region[i].cumvol[0]);
		(void)fflush(fp);
	      }

	    if(region[i].surarea[1] == 1.)
	      {
		(void)fprintf(fp,"\tarea:  %f - difference is above",
			      region[i].surarea[0]);
		(void)fprintf(fp," %f variance.\n",VOLVAR);
		(void)fflush(fp);
	      }
	    else
	      {
		(void)fprintf(fp,"\t area:  %f - within variance.\n",
			      region[i].surarea[0]);
		(void)fflush(fp);
	      }

	    (void)fprintf(fp,"\tcentroid:  %f, %f, %f\n",
			  region[i].centroid[0],
			  region[i].centroid[1],region[i].centroid[2]);
	    (void)fflush(fp);
	    (void)fprintf(fp,"\tcummulative normal of the exterior ");
	    (void)fprintf(fp,"free surface:\n\t\t%f, %f, %f\n",
			  region[i].cumnorm[X],region[i].cumnorm[Y],
			  region[i].cumnorm[Z]);
	    (void)fflush(fp);
	    (void)fprintf(fp,"\text sur air:  %f",region[i].cumfs[0][0]);
	    if(region[i].cumfs[0][1] == 0.) (void)fprintf(fp," - ok\n");
	    if(region[i].cumfs[0][1] == 1.) (void)fprintf(fp," - not ok\n");
	    if(region[i].cumfs[0][1] == 2.) (void)fprintf(fp," - none\n");
	    (void)fprintf(fp,"\tcrew comp air:  %f",region[i].cumfs[1][0]);
	    if(region[i].cumfs[1][1] == 0.) (void)fprintf(fp," - ok\n");
	    if(region[i].cumfs[1][1] == 1.) (void)fprintf(fp," - not ok\n");
	    if(region[i].cumfs[1][1] == 2.) (void)fprintf(fp," - none\n");
	    (void)fprintf(fp,"\teng comp air:  %f",region[i].cumfs[2][0]);
	    if(region[i].cumfs[2][1] == 0.) (void)fprintf(fp," - ok\n");
	    if(region[i].cumfs[2][1] == 1.) (void)fprintf(fp," - not ok\n");
	    if(region[i].cumfs[2][1] == 2.) (void)fprintf(fp," - none\n");
	    (void)fprintf(fp,"\tclsd comp air:  %f",region[i].cumfs[3][0]);
	    if(region[i].cumfs[3][1] == 0.) (void)fprintf(fp," - ok\n");
	    if(region[i].cumfs[3][1] == 1.) (void)fprintf(fp," - not ok\n");
	    if(region[i].cumfs[3][1] == 2.) (void)fprintf(fp," - none\n");
	    (void)fprintf(fp,"\texhaust air:  %f",region[i].cumfs[4][0]);
	    if(region[i].cumfs[4][1] == 0.) (void)fprintf(fp," - ok\n");
	    if(region[i].cumfs[4][1] == 1.) (void)fprintf(fp," - not ok\n");
	    if(region[i].cumfs[4][1] == 2.) (void)fprintf(fp," - none\n");
	    (void)fprintf(fp,"\tgen air 1:  %f",region[i].cumfs[5][0]);
	    if(region[i].cumfs[5][1] == 0.) (void)fprintf(fp," - ok\n");
	    if(region[i].cumfs[5][1] == 1.) (void)fprintf(fp," - not ok\n");
	    if(region[i].cumfs[5][1] == 2.) (void)fprintf(fp," - none\n");
	    (void)fprintf(fp,"\tgen air 2:  %f",region[i].cumfs[6][0]);
	    if(region[i].cumfs[6][1] == 0.) (void)fprintf(fp," - ok\n");
	    if(region[i].cumfs[6][1] == 1.) (void)fprintf(fp," - not ok\n");
	    if(region[i].cumfs[6][1] == 2.) (void)fprintf(fp," - none\n");
	    (void)fflush(fp);
	    for(j=0; j<num; j++)
	      {
		if(region[i].adjreg[j] == 1)
		  {
		    (void)fprintf(fp,"\tadjreg[%d]=%d, ",
				  (j+1),region[i].adjreg[j]);
		    (void)fprintf(fp,"shared surface area:  %f;\n",
				  region[i].ssurarea[0][j]);
		    (void)fflush(fp);
		    if(region[i].ssurarea[1][j] == 1.)
		      {
			(void)fprintf(fp,"\tdifference is above ");
			(void)fprintf(fp,"%f variance\n",
				      VOLVAR);
			(void)fflush(fp);
		      }
		    else
		      {
			(void)fprintf(fp,"\twithin variance\n");
			(void)fflush(fp);
		      }
		  }
	      }
	    i++;
	  }

	/*  Print out names of all files used.  */
	(void)fprintf(fp,"\n\nSUMMARY OF FILES USED & CREATED\n");
	(void)fprintf(fp,"\t.g file used:  %s\n",argv[1]);
	(void)fprintf(fp,"\tregions used:\n");
	(void)fflush(fp);
	i=2;
	while(argv[i] != NULL)
	  {
	    (void)fprintf(fp,"\t\t%s\n",argv[i]);
	    (void)fflush(fp);
	    i++;
	  }
	(void)fprintf(fp,"\tmaterial id file used:  %s\n",fileden);
	if(iwrite == 1)
	  {
	    (void)fprintf(fp,"\toutput file created:  %s\n",filename);
	  }
	(void)fprintf(fp,"\tsecond pass file created:  %s\n",spfile);
	(void)fprintf(fp,"\terror file created:  %s\n",fileerr);
	(void)fprintf(fp,"\tregion # & name file created:  %s\n",
		      filernn);
	if( typeout == 0 )
	  {
	    (void)fprintf(fp,"\tfacet file created:  %s\n",facfile);
	    (void)fprintf(fp,"\t  (format is PRISM %d.0)\n",prmrel);
	  }
	if( typeout == 1 ) (void)fprintf(fp,
					 "\tgeneric file created:  %s\n",filegen);
	if( (typeout == 2) || (typeout == 3) ) (void)fprintf(fp,
							     "\tgeometric file created:  %s\n",filegeo);
	(void)fflush(fp);

	(void)fclose(fp); }

      /****************************************************************************/
      /*  Write region # & name file, for use with shapefact.  */
      fp7 = fopen(filernn,"w");
      for(i=0; i<num; i++)
	{
	  (void)fprintf(fp7,"%d\t%s\n",(i+1),region[i].regname);
	  (void)fflush(fp7);
	}
      (void)fclose(fp7);
      /****************************************************************************/
      if( typeout == 0 ) {			/*  START # 11 */

	/*  Open facet file for writing to .  */
	fp1=fopen(facfile,"w");

	/*  Print type number of file (02) and description.  */
	(void)fprintf(fp1,"02\tFacet file for use with PRISM.\n");
	(void)fflush(fp1);

	/*  Print header information for facet file.  (Note:  header  */
	/*  info is the same for PRISM 2.0 & 3.0.)  */
	(void)fprintf(fp1," FN DESCRIPTION               TY");
	(void)fprintf(fp1,"    AREA    MASS  SPHEAT      E1");
	(void)fprintf(fp1,"      E2   ABSOR\n");
	(void)fflush(fp1);
	(void)fprintf(fp1,"   SN(X)   SN(Y)   SN(Z)    CONV");
	(void)fprintf(fp1,"       K       L   SHAPE\n");
	(void)fflush(fp1);

	/*  Make calculations to get PRISM information & then  */
	/*  write to file for each region (or facet as PRISM  */
	/*  calls them).  */

	for(i=0; i<num; i++)
	  {
	    /*  Find facet number (>=1).  */
	    facnum = i + 1;
	    /*
	     *		(void)printf("\n\tfacet number:  %d\n",facnum);
	     *		(void)fflush(stdout);
	     */

	    /*  Find facet name (25 char max).  */
	    c=region[i].regname[0];
	    icnt = 0;
	    while(c != '\0')
	      {
		icnt += 1;
		c=region[i].regname[icnt];
	      }
	    icnt -= 1;
	    for(j=24; j>=0; j--)
	      {
		if(icnt >= 0)
		  {
		    facname[j] = region[i].regname[icnt];
		    icnt -= 1;
		  }
		else
		  {
		    facname[j] = ' ';
		    icnt -= 1;
		  }
	      }
	    /*
	     *		(void)printf("name:%.25s:\n",facname);
	     *		(void)fflush(stdout);
	     */

	    /*  Find facet type (for now all are 1).  */
	    factype = 1;
	    /*
	     *		(void)printf("facet type:  %d  ",factype);
	     *		(void)fflush(stdout);
	     */

	    /*  Find area of facet or as it has been called  */
	    /*  exterior surface area (sq meters).  */
	    facarea = region[i].cumfs[0][0] * (1.e-6);

	    /*  If there is no exterior surface area, i.e. the  */
	    /*  surface normal is (0,0,0) set the area of the  */
	    /*  facet to the total surface area.  If the exterior  */
	    /*  surface area is small (< .001) set the area of the  */
	    /*  facet to .001.  This is done since PRISM will not  */
	    /*  accept a 0 surface area.  */
	    if( (region[i].cumnorm[0] == 0.) &&
		(region[i].cumnorm[1] == 0.) &&
		(region[i].cumnorm[2] == 0.) )
	      {
		facarea = region[i].surarea[0] * (1.e-6);
		(void)printf("There are no exterior surfaces on region ");
		(void)printf("%d.  Setting exterior surface area\n",(i+1));
		(void)printf("\tto total surface area %f\n",facarea);
		(void)fflush(stdout);
	      }
	    if(facarea < .001)
	      {
		facarea = .001;
		(void)printf("Small surface area for region %d.  ",(i+1));
		(void)printf("Setting exterior surface area");
		(void)printf("\tto %f.\n",facarea);
	      }
	    /*
	     *		(void)printf("area:  %8.3f  ",facarea);
	     *		(void)fflush(stdout);
	     */

	    /*  Set material id number.  */
	    if(region[i].mat <= 40.) ia = (int)region[i].mat;
	    else ia = 0;

	    /*  Find the mass of the facet (kg).  This is  */
	    /*  dependent on the volume and material code.  */
	    facmass = matprop[ia].d * region[i].cumvol[0] * (1.e-9);
	    /*
	     *		(void)printf("mass:  %8.3f\n",facmass);
	     *		(void)fflush(stdout);
	     */

	    /*  Find the specific heat of the facet (J/kg deg C).  */
	    /*  Read from material file.  */
	    facspheat = matprop[ia].sh;

	    /*  Emissivities with respect to the environment &  */
	    /*  other facets.  They are read from material file.  */
	    face1 = matprop[ia].e1;
	    face2 = matprop[ia].e2;

	    /*  Absorptivity of facet.  Read from material file.  */
	    facabs = matprop[ia].a;
	    /*
	     *		(void)printf("specific heat:  %8.3f  emissivities:  %8.3f & %8.3f  absorb:  %8.3f\n",
	     *		   facspheat,face1,face2,facabs);
	     *		(void)fflush(stdout);
	     */

	    /*  Surface normal of facet.  Must be rotated back first.  */
	    facnorm[X] = region[i].cumnorm[0];
	    facnorm[Y] = region[i].cumnorm[1];
	    facnorm[Z] = region[i].cumnorm[2];
	    /*
	     *		(void)printf("normal:  %8.3f %8.3f %8.3f\n",
	     *		   facnorm[0],facnorm[1],facnorm[2]);
	     */

	    /*  Convection coefficient of facet.  Currently  */
	    /*  set to 1.  */
	    faccv = 1.;

	    /*  Facets seen by the back & front of current  */
	    /*  facet.  Will be set to 0 thereby assuming  */
	    /*  there is not radiative exchange taking  */
	    /*  place.  */
	    fack = 0;
	    facl = 0;
	    /*
	     *		(void)printf("faccv:  %8.3f, fack:  %3d, facl:  %3d\n",
	     *		   faccv,fack,facl);
	     *		(void)fflush (stdout);
	     */

	    /*  Shape factors for engine & track facets  */
	    /*  (between 0 & 1).  Currently set to 0.  */
	    facshape = 0.;

	    /*  Hub radius (m).  Currently set to 0 unless engine  */
	    /*  air area exist then print engine air area in square  */
	    /*  meters.  */
	    facradius = 0.;
	    if(region[i].cumfs[2][0] > ZEROTOL)
	      facradius = region[i].cumfs[2][0] * (1.e-6);

	    /*  Bearing friction constant (J) for wheels.  */
	    /*  Currently set to 0.  */
	    facfric = 0.;
	    /*
	     *		(void)printf("facshape:  %8.3f, facradius:  %8.3f, facfric:  %8.3f\n",
	     *		   facshape,facradius,facfric);
	     *		(void)fflush(stdout);
	     */

	    /*  Print information to the facet file.  */
	    if(prmrel == 2)
	      (void)fprintf(fp1,"%3d %.25s%3d%8.3f%8.3f%8.3f",
			    facnum,facname,factype,facarea,facmass,facspheat);
	    if(prmrel == 3)
	      (void)fprintf(fp1,"%6d %.25s%3d%8.3f%8.3f%8.3f",
			    facnum,facname,factype,facarea,facmass,facspheat);
	    (void)fprintf(fp1,"%8.3f%8.3f%8.3f\n",face1,
			  face2,facabs);
	    (void)fflush(fp1);

	    (void)fprintf(fp1,"%8.3f%8.3f%8.3f%8.3f%8d%8d",
			  facnorm[0],facnorm[1],facnorm[2],faccv,fack,facl);
	    (void)fprintf(fp1,"%8.3f%8.3f%8.3f\n",facshape,
			  facradius,facfric);
	    (void)fflush(fp1);

	  }

	/*  Write last line to signify end of facet information.  */
	if(prmrel == 2)(void)fprintf(fp1,"%3d END OF REGIONS           999\n",
				     (facnum+1));
	if(prmrel == 3)(void)fprintf(fp1,"%6d END OF REGIONS           999\n",
				     (facnum+1));
	(void)fflush(fp1);

	/*  Close facet file.  */
	(void)fclose(fp1);

      }						/*  END # 11  */

      /****************************************************************************/

      /*  Open and write to generic file if needed.  */

      /*  Format for generic file.  */
      /*	1  region #  region name  volume  density  ...  */
      /*		thermal conductivity  specific heat  material  */
      /*                                                             */
      /*	2  region #  # of exterior convection surfaces  ...  */
      /*		# of internal convection surfaces  */
      /*	   areas of all external surfaces  */
      /*	   areas of all internal surfaces  */
      /*                                                             */
      /*	3  region #  # of solar loaded surfaces  */
      /*	   surface area  surface normal (X Y Z)  absorptivity  */

      if(typeout == 1)
	{						/*  START # 12  */
	  /*  Open generic file.  */
	  fp5 = fopen(filegen,"w");

	  /*  Write out # 1 information.  */
	  for(i=0; i<num; i++)
	    {
	      /*  Find region name (25 char).  */
	      c = region[i].regname[0];
	      icnt = 0;
	      while(c != '\0')
		{
		  icnt += 1;
		  c = region[i].regname[icnt];
		}
	      icnt -= 1;
	      for(j=24; j>=0; j--)
		{
		  if(icnt >= 0)
		    {
		      facname[j] = region[i].regname[icnt];
		      icnt -= 1;
		    }
		  else
		    {
		      facname[j] = ' ';
		      icnt -= 1;
		    }
		}

	      if(region[i].mat <= 40) ia = (int)region[i].mat;
	      else ia = 0;

	      (void)fprintf(fp5,"1 %6d %.25s %.3e %.3e ",
			    (i+1),facname,(region[i].cumvol[X]*1.e-9),
			    matprop[ia].d);
	      (void)fprintf(fp5,"%.3e %.3e %.25s\n",matprop[ia].tc,
			    matprop[ia].sh,matprop[ia].m);
	      (void)fflush(fp5);
	    }

	  /*  Write out # 2 information.  */
	  for(i=0; i<num; i++)
	    {
	      /*  Any exterior surfaces?  */
	      numext = 0;
	      if(region[i].cumfs[0][0] > ZEROTOL) numext = 1;

	      /*  Any interior surfaces?  */
	      numint = 0;
	      if(region[i].cumfs[1][0] > ZEROTOL) numint += 1;
	      if(region[i].cumfs[2][0] > ZEROTOL) numint += 1;
	      if(region[i].cumfs[3][0] > ZEROTOL) numint += 1;
	      if(region[i].cumfs[4][0] > ZEROTOL) numint += 1;
	      if(region[i].cumfs[5][0] > ZEROTOL) numint += 1;
	      if(region[i].cumfs[6][0] > ZEROTOL) numint += 1;

	      (void)fprintf(fp5,"2 %6d   %3d          %3d\n",
			    (i+1),numext,numint);
	      (void)fflush(fp5);

	      if(numext == 1)
		{
		  (void)fprintf(fp5,"  %.3e\n",(region[i].cumfs[0][0]*1.e-6));
		  (void)fflush(fp5);
		}
	      if(numint > 0)
		{
		  if(region[i].cumfs[1][0] > ZEROTOL)
		    {
		      (void)fprintf(fp5,"  %.3e",
				    (region[i].cumfs[1][0]*1.e-6));
		    }
		  if(region[i].cumfs[2][0] > ZEROTOL)
		    {
		      (void)fprintf(fp5," %.3e",
				    (region[i].cumfs[2][0]*1.e-6));
		    }
		  if(region[i].cumfs[3][0] > ZEROTOL)
		    {
		      (void)fprintf(fp5," %.3e",
				    (region[i].cumfs[3][0]*1.e-6));
		    }
		  if(region[i].cumfs[4][0] > ZEROTOL)
		    {
		      (void)fprintf(fp5," %.3e",
				    (region[i].cumfs[4][0]*1.e-6));
		    }
		  if(region[i].cumfs[5][0] > ZEROTOL)
		    {
		      (void)fprintf(fp5," %.3e",
				    (region[i].cumfs[5][0]*1.e-6));
		    }
		  if(region[i].cumfs[6][0] > ZEROTOL)
		    {
		      (void)fprintf(fp5," %.3e",
				    (region[i].cumfs[6][0]*1.e-6));
		    }
		  (void)fprintf(fp5,"\n");
		  (void)fflush(stdout);
		}
	    }

	  /*  Write out # 3 information.  */
	  for(i=0; i<num; i++)
	    {
	      numsol = 0;
	      if(region[1].cumfs[0][0] > ZEROTOL) numsol = 1;
	      (void)fprintf(fp5,"3 %6d %3d\n",(i+1),numsol);
	      (void)fflush(fp5);
	      if(numsol > 0)
		{
		  if(region[i].mat <= 40) ia = (int)region[i].mat;
		  else ia = 0;

		  (void)fprintf(fp5,"  %.3e %+.3e %+.3e %+.3e %.3e\n",
				(region[i].cumfs[0][0]*1.e-6),region[i].cumnorm[X],
				region[i].cumnorm[Y],region[i].cumnorm[Z],
				matprop[ia].a);
		  (void)fflush(fp5);
		}
	    }

	  /*  Close generic file.  */
	  (void)fclose(fp5);

	}						/*  END # 12  */

      /****************************************************************************/

      /*  Open and write to geometric file if needed.  */

      if( (typeout == 2) || (typeout==3) )
	{						/*  START # 13  */

	  /*  Open geometry file.  */
	  fp6 = fopen(filegeo,"w");

	  /*  Readable geometric file.  */

	  /*  Format for readable geometric file.  */
	  /*	region #  region name  centroid-X  Y  Z  ...  */
	  /*		volume (m**3)  mass (kg)  */
	  /*						      */
	  /*	region #  ext surface area (m**2)  ...  */
	  /*		engine surface area (m**2)  ...  */
	  /*		crew surface area (m**2)  ...  */
	  /*		closed compartment surface area (m**2)  ...  */
	  /*		exhaust surface area (m**2)  ...  */
	  /*		generic 1 surface area (m**2)  ...  */
	  /*		generic 2 surface area (m**2)  */
	  /*						      */
	  /*	region #  material code  density (kg/m3)  ...  */
	  /*		specific heat  absorptivity  emissivity  ...  */
	  /*		thermal conductivity (W/mK)  material  */
	  /*						      */
	  /*	region #  all adjacent regions (up to 20)  */

	  if(typeout == 2) {
	    /*  Write to geometric file.  */
	    (void)fprintf(fp6,"\nGEOMETRIC FILE - from firpass\n");
	    (void)fprintf(fp6,"\n.gfile used:  %s\n",argv[1]);
	    (void)fprintf(fp6,"\tregions used:\n");
	    (void)fflush(fp6);
	    i = 2;
	    while(argv[i] != NULL)
	      {
		(void)fprintf(fp6,"\t\t%s\n",argv[i]);
		(void)fflush(fp6);
		i++;
	      }
	    (void)fprintf(fp6,"\n\n");
	    (void)fflush(fp6);

	    (void)fprintf(fp6,"region   region name                 ");
	    (void)fprintf(fp6,"centroid                           ");
	    (void)fprintf(fp6,"volume       mass\n");
	    (void)fprintf(fp6,"number                               ");
	    (void)fprintf(fp6,"X          Y          Z            ");
	    (void)fprintf(fp6,"(m**3)       (kg)\n");
	    (void)fflush(fp6);

	    for(i=0; i<num; i++)
	      {

		/*  Find region name (25 char).  */
		c = region[i].regname[0];
		icnt = 0;
		while(c != '\0')
		  {
		    icnt += 1;
		    c = region[i].regname[icnt];
		  }
		icnt -= 1;
		for(j=24; j>=0; j--)
		  {
		    if(icnt >= 0)
		      {
			facname[j] = region[i].regname[icnt];
			icnt -= 1;
		      }
		    else
		      {
			facname[j] = ' ';
			icnt -= 1;
		      }
		  }

		if(region[i].mat <=40) ia = (int)region[i].mat;
		else ia = 0;
		facmass = matprop[ia].d * region[i].cumvol[0] * (1.e-9);
		(void)fprintf(fp6,"%6d   %.25s   %+.3e %+.3e %+.3e   ",
			      (i+1),facname,region[i].centroid[X],
			      region[i].centroid[Y],region[i].centroid[Z]);
		(void)fprintf(fp6,"%.3e   %.3e\n",(region[i].cumvol[X]*1.e-9),
			      facmass);
		(void)fflush(fp6);
	      }

	    (void)fprintf(fp6,"\n\n\nregion   exterior sur   engine sur    ");
	    (void)fprintf(fp6,"crew sur      closed compartment");
	    (void)fprintf(fp6,"   exhaust sur   generic 1 sur   ");
	    (void)fprintf(fp6,"generic 2 sur\n");
	    (void)fprintf(fp6,"number   area (m**2)    area (m**2)   ");
	    (void)fprintf(fp6,"area (m**2)   sur area (m**2)");
	    (void)fprintf(fp6,"      area (m**2)   area (m**2)     ");
	    (void)fprintf(fp6,"area (m**2)\n");
	    (void)fflush(fp6);

	    for(i=0; i<num; i++)
	      {
		(void)fprintf(fp6,"%6d   %.3e      %.3e     %.3e    ",
			      (i+1),(region[i].cumfs[0][0]*1.e-6),
			      (region[i].cumfs[2][0]*1.e-6),
			      (region[i].cumfs[1][0]*1.e-6));
		(void)fprintf(fp6," %.3e            %.3e     %.3e       %.3e\n",
			      (region[i].cumfs[3][0]*1.e-6),
			      (region[i].cumfs[4][0]*1.e-6),
			      (region[i].cumfs[5][0]*1.e-6),
			      (region[i].cumfs[6][0]*1.e-6));
		(void)fflush(fp6);
	      }

	    (void)fprintf(fp6,"\n\nregion   material   density      ");
	    (void)fprintf(fp6,"specific     absorptivity   emissivity   ");
	    (void)fprintf(fp6,"thermal               material\n");
	    (void)fprintf(fp6,"number   code       (kg/m3)      ");
	    (void)fprintf(fp6,"heat                                     ");
	    (void)fprintf(fp6,"conductivity (W/mK)\n");

	    for(i=0; i<num; i++)
	      {
		if(region[i].mat <= 40) ia = (int)region[i].mat;
		else ia = 0;

		(void)fprintf(fp6,"%6d   %3d        %.3e    ",
			      (i+1),region[i].mat,matprop[ia].d);
		(void)fprintf(fp6,"%.3e    %.3e      %.3e    %.3e",
			      matprop[ia].sh,matprop[ia].a,matprop[ia].e1,
			      matprop[ia].tc);
		(void)fprintf(fp6,"           %s\n",matprop[ia].m);
		(void)fflush(fp6);
	      }

	    (void)fprintf(fp6,"\n\nregion   adjacent\n");
	    (void)fprintf(fp6,"number   regions");
	    (void)fflush(fp6);

	    for(i=0; i<num; i++)
	      {
		(void)fprintf(fp6,"\n%6d   ",(i+1));

		for(j=0; j<num; j++)
		  {
		    if(region[i].adjreg[j] == 1)
		      {
			(void)fprintf(fp6,"%4d, ",(j+1));
			(void)fflush(fp6);
		      }
		  }
	      } }

	  if(typeout == 3)
	    {

	      /*  Format for non-readable geometric file.  */
	      /*	region #, region name, centroid-X, Y, Z, volume,  ...  */
	      /*		mass, exterior surface area,  ...  */
	      /*		crew surface area, engine surface area,  ...  */
	      /*		closed compartment surface area,  ...  */
	      /*		exhaust surface area, generic 1 surface area,  ...  */
	      /*		generic 2 surface area,  ...  */
	      /*		material code, density, specific heat,  ...  */
	      /*		absorptivity, emissivity, thermal conductivity,  ...  */
	      /*		all adjacent regions  */

	      for(i=0; i<num; i++)
		{

		  /*  Find region name (25 char).  */
		  c = region[i].regname[0];
		  icnt = 0;
		  while(c != '\0')
		    {
		      icnt += 1;
		      c = region[i].regname[icnt];
		    }
		  icnt -= 1;
		  for(j=24; j>=0; j--)
		    {
		      if(icnt >= 0)
			{
			  facname[j] = region[i].regname[icnt];
			  icnt -= 1;
			}
		      else
			{
			  facname[j] = ' ';
			  icnt -= 1;
			}
		    }

		  if(region[i].mat <=40) ia = (int)region[i].mat;
		  else ia = 0;
		  facmass = matprop[ia].d * region[i].cumvol[0] * (1.e-9);

		  (void)fprintf(fp6,"%d,%s,%.3e,%.3e,%.3e,",
				(i+1),facname,region[i].centroid[X],
				region[i].centroid[Y],region[i].centroid[Z]);
		  (void)fflush(fp6);

		  (void)fprintf(fp6,"%.3e,%.3e,%.3e,%.3e,%.3e,",
				(region[i].cumvol[X]*1.e-9),facmass,
				(region[i].cumfs[0][0]*1.e-6),
				(region[i].cumfs[1][0]*1.e-6),
				(region[i].cumfs[2][0]*1.e-6));
		  (void)fprintf(fp6,"%.3e,%.3e,%.3e,%.3e,",
				(region[i].cumfs[3][0]*1.e-6),
				(region[i].cumfs[4][0]*1.e-6),
				(region[i].cumfs[5][0]*1.e-6),
				(region[i].cumfs[6][0]*1.e-6));
		  (void)fflush(fp6);

		  (void)fprintf(fp6,"%d,%.3e,%.3e,%.3e,%.3e,%.3e,",
				region[i].mat,matprop[ia].d,matprop[ia].sh,
				matprop[ia].a,matprop[ia].e1,matprop[ia].tc);
		  (void)fflush(fp6);

		  ii = 0;
		  for(j=0; j<num; j++)
		    {
		      if( (region[i].adjreg[j] == 1) && (ii < 20) )
			{
			  (void)fprintf(fp6,"%d,",(j+1));
			  (void)fflush(fp6);
			  ii++;
			}

		    }
		  if(ii < 20)
		    {
		      for(j=ii; j<20; j++)
			{
			  (void)fprintf(fp6,"%d,",0);
			  (void)fflush(fp6);
			}
		    }
		  (void)fprintf(fp6,"\n");
		  (void)fflush(stdout);
		}
	    }

	  /*  Close geometric file.  */
	  (void)fclose(fp6);

	}						/*  END # 13  */
      /****************************************************************************/

      /*  Open second pass file.  */
      fp2 = fopen(spfile,"w");

      /*  Write info to second pass file.  */
      /*  Write number of regions to file.  */
      (void)fprintf(fp2,"%8d\n",num);
      (void)fflush(fp2);

      for(i=0; i<num; i++)
	{

	  /*  Write region number, centroid & material id  */
	  /*  to 2nd pass file.  */
	  (void)fprintf(fp2,"%8d  %.6e  %.6e  %.6e  %3d\n",
			(i+1),region[i].centroid[0],region[i].centroid[1],
			region[i].centroid[2],region[i].mat);
	  (void)fflush(fp2);

	  /*  Write area of adjacent region.  */
	  for(j=0; j<num; j++)
	    {
	      spsarea = region[i].ssurarea[0][j];
	      (void)fprintf(fp2,"%8d  %.6e\n",
			    (j+1),spsarea);
	      (void)fflush(fp2);
	    }
	}

      /*  Close second pass file.  */
      (void)fclose(fp2);

      /****************************************************************************/

      /*  Open error file.  */
      fp3=fopen(fileerr,"w");

      /*  Write errors to error file.  */
      (void)fprintf(fp3,"\nERRORS from firpass\n\n");
      /*  Write type of file created to error file.  */
      if(typeout == 0)
	{
	  (void)fprintf(fp3,"Facet file, %s, PRISM %d.0 was created.\n\n",
			facfile,prmrel);
	}
      if(typeout == 1) (void)fprintf(fp3,"Generic file, %s, was created.\n\n",
				     filegen);
      if(typeout == 2)
	{
	  (void)fprintf(fp3,"Geometric file, %s, was created.\n\n",filegeo);
	}
      (void)fflush(fp3);
      for(i=0; i<num; i++)
	{
	  if(region[i].cumvol[1] == 1)
	    {
	      (void)fprintf(fp3,"region %d:  ",(i + 1));
	      (void)fprintf(fp3,"large variance on volume:  %f\n",
			    region[i].cumvol[0]);
	      (void)fflush(fp3);
	    }

	  if(region[i].surarea[1] == 1)
	    {
	      (void)fprintf(fp3,"region %d:  large variance ",(i + 1));
	      (void)fprintf(fp3,"on surface area:  %f\n",
			    region[i].surarea[0]);
	      (void)fflush(fp3);
	    }

	  for(j=0; j<num; j++)
	    {
	      if(region[i].ssurarea[1][j] == 1)
		{
		  (void)fprintf(fp3,"region %d:  adjacent region %d:\n",
				(i + 1),(j + 1));
		  (void)fprintf(fp3,"\tlarge variance on shared surface ");
		  (void)fprintf(fp3,"area:  %f\n",region[i].ssurarea[0][j]);
		  (void)fflush(fp3);
		}
	    }

	  for(j=0; j<7; j++)
	    {						/*  START # 2000  */
	      if(region[i].cumfs[j][1] == 1.)
		{					/*  START # 2010  */
		  if(j == 0)
		    {
		      (void)fprintf(fp3,"\treg %d - large variance ",i);
		      (void)fprintf(fp3,"of exterior air\n");
		    }
		  if(j == 1)
		    {
		      (void)fprintf(fp3,"\treg %d - large variance ",i);
		      (void)fprintf(fp3,"of crew comp air\n");
		    }
		  if(j == 2)
		    {
		      (void)fprintf(fp3,"\treg %d - large variance ",i);
		      (void)fprintf(fp3,"of engine comp air\n");
		    }
		  if(j == 3)
		    {
		      (void)fprintf(fp3,"\treg %d - large variance ",i);
		      (void)fprintf(fp3,"of closed comp air\n");
		    }
		  if(j == 4)
		    {
		      (void)fprintf(fp3,"\treg %d - large variance ",i);
		      (void)fprintf(fp3,"of exhaust air\n");
		    }
		  if(j == 5)
		    {
		      (void)fprintf(fp3,"\treg %d - large variance ",i);
		      (void)fprintf(fp3,"of generic air 1\n");
		    }
		  if(j == 6)
		    {
		      (void)fprintf(fp3,"\treg %d - large variance ",i);
		      (void)fprintf(fp3,"of generic air 2\n");
		    }
		  (void)fflush(fp3);
		}					/*  END # 2010  */
	    }						/*  END # 2000  */
	}

      /*  Write overlaps to error file.  */
      (void)fprintf(fp3,"\n\n\tOVERLAPS\n\n");
      (void)fflush(fp3);
      for(i=0; i<num; i++)
	{
	  for(j=0; j<num; j++)
	    {
	      if(overlaps[i].ovrreg[j] == 1)
		{
		  (void)fprintf(fp3,"%s & %s, max depth:  %fmm  ",
				region[i].regname,region[j].regname,
				overlaps[i].ovrdep[j]);
		  (void)fflush(fp3);
		  if(overlaps[i].ovrdep[j] < ADJTOL)
		    {
		      (void)fprintf(fp3,"(within tolerance)\n");
		      (void)fflush(fp3);
		    }
		  else
		    {
		      (void)fprintf(fp3,"\n");
		      (void)fflush(fp3);
		    }
		}
	    }
	}

      /*  Write number of adjacent regions to error file.  */
      (void)fprintf(fp3,"\n\nREGION NUMBER     NUMBER OF ADJACENT REGIONS\n");
      (void)fflush(fp3);
      for(i=0; i<num; i++)
	{
	  numadjreg = 0;
	  for(j=0; j<num; j++)
	    {
	      if(region[i].adjreg[j] == 1) numadjreg++;
	    }
	  (void)fprintf(fp3,"        %5d                          ",(i+1));
	  (void)fprintf(fp3,"%5d\n",numadjreg);
	  (void)fflush(fp3);
	}

      /*  Print out names of all files used.  */
      (void)printf("\n\nSUMMARY OF FILES USED & CREATED\n");
      (void)printf("\t.g file used:  %s\n",argv[1]);
      (void)printf("\tregions used:\n");
      (void)fflush(stdout);
      i=2;
      while(argv[i] != NULL)
	{
	  (void)printf("\t\t%s\n",argv[i]);
	  (void)fflush(stdout);
	  i++;
	}
      (void)printf("\tmaterial id file used:  %s\n",fileden);
      if(iwrite == 1)
	{
	  (void)printf("\toutput file created:  %s\n",filename);
	}
      (void)printf("\tsecond pass file created:  %s\n",spfile);
      (void)printf("\terror file created:  %s\n",fileerr);
      (void)printf("\tregion # & name file created:  %s\n",filernn);
      if( typeout == 0 )
	{
	  (void)printf("\tfacet file created:  %s\n",facfile);
	  (void)printf("\t  (format is PRISM %d.0)\n\n\n",prmrel);
	}
      if( typeout == 1 ) (void)printf(
				      "\tgeneric file created:  %s\n\n\n",filegen);
      if( (typeout == 2) || (typeout == 3) ) (void)printf(
							  "\tgeometric file created:  %s\n\n\n",filegeo);
      (void)fflush(stdout);

      /*  Close error file.  */
      (void)fclose(fp3);

      /*  Everything is complete, free all memory.  */

      (void)printf("Freeing memory.\n");
      (void)fflush(stdout);

      for(i=0; i<num; i++)
	{
	  free(region[i].adjreg);
	  free(region[i].ssurarea[0]);
	  free(region[i].ssurarea[1]);
	  free(region[i].ssurarea[2]);
	  free(overlaps[i].ovrreg);
	  free(overlaps[i].ovrdep);
	}
      free(region);
      free(overlaps);

    }
  return(0);
}

int
hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp)
{
  register struct partition *pp;
  register struct hit *hitp;
  register struct soltab *stp;
  struct curvature cur;

  double enterpt[3];	/*  Point where ray enters.  */
  double distance;	/*  Distance between where point enters  */
  /*  and leaves.  */
  double disx,disy,disz;	/*  Used for finding distance.  */
  double costheta;	/*  Used in computing areas.  */
  double costheta1;	/*  Used in computing surface area.  */
  double costheta2;	/*  Used in computing exterior surface air.  */
  short prevregid;	/*  Previous region id.  */
  short prevair;		/*  Previous air code.  */

  /*  Initialize the following.  */
  prevregid = (-1);
  prevair = (-1);

  pp=PartHeadp->pt_forw;
  for( ; pp != PartHeadp; pp = pp->pt_forw)
    {
      /*
       *		(void)printf("Hit region %s (in %s, out %s)\n",
       *			pp->pt_regionp->reg_name,
       *			pp->pt_inseg->seg_stp->st_name,
       *			pp->pt_outseg->seg_stp->st_name);
       *		(void)fflush(stdout);
       *		(void)printf("index to region:  %d\n",
       *			pp->pt_regionp->reg_bit);
       *		(void)fflush(stdout);
       */

      /*  Print out region id and aircode.  */
      /*
       *		(void)printf("region id:  %d, aircode:  %d\n",
       *		    pp->pt_regionp->reg_regionid,pp->pt_regionp->reg_aircode);
       *		(void)fflush(stdout);
       */

      /*  Put region name into region[].regname.  */
      /*
       *		(void)printf("Putting region name into struct.\n");
       *		(void)fflush(stdout);
       */
      icur=pp->pt_regionp->reg_bit;	/*  region # hit  */
      /*
       *		(void)printf("region %d:  *%s*\n",icur,region[icur].regname);
       *		(void)fflush(stdout);
       */

      region[icur].regname = (char *)pp->pt_regionp->reg_name;

      /*
       *		(void)printf("region #:  %d, region name:  %s\n",
       *			icur,region[icur].regname);
       *		(void)fflush(stdout);
       */

      /*  Put material code into region[].mat.  */
      region[icur].mat = pp->pt_regionp->reg_gmater;

      /*  Find normal and hit point of entering ray.  */
      /*
       *		(void)printf("Entering Ray\n");
       *		(void)fflush(stdout);
       */
      hitp=pp->pt_inhit;
      stp=pp->pt_inseg->seg_stp;
      RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
      /*  Flip normal if needed.  */
      if(pp->pt_inflip)
	{
	  VREVERSE(hitp->hit_normal,hitp->hit_normal);
	  pp->pt_inflip=0;
	}

      /*  Find front surface area of region.  */
      costheta1 = hitp->hit_normal[X] * ap_p->a_ray.r_dir[X]
	+ hitp->hit_normal[Y] * ap_p->a_ray.r_dir[Y]
	+ hitp->hit_normal[Z] * ap_p->a_ray.r_dir[Z];
      if(costheta1 < 0) costheta1 = (-costheta1);
      if( costheta1 > COSTOL)
	region[icur].surarea[whichview] += (area/costheta1);

      if(
	 (-ADJTOL < (hitp->hit_point[X] - leavept[X])) &&
	 ((hitp->hit_point[X] - leavept[X]) < ADJTOL) &&
	 (-ADJTOL < (hitp->hit_point[Y] - leavept[Y])) &&
	 ((hitp->hit_point[Y] - leavept[Y]) < ADJTOL) &&
	 (-ADJTOL < (hitp->hit_point[Z] - leavept[Z])) &&
	 ((hitp->hit_point[Z] - leavept[Z]) < ADJTOL)
	 )

	{
	  /*  Find adjacent regions.  Occasionally a  */
	  /*  a region will seem to be adjacent to self,  */
	  /*  disreguard this.  */
	  if( icur != iprev)
	    {
	      region[icur].adjreg[iprev]=1;
	      region[iprev].adjreg[icur]=1;
	    }

	  /*  Find surface area. */
	  costheta = lnormal[X]*ap_p->a_ray.r_dir[X]
	    + lnormal[Y]*ap_p->a_ray.r_dir[Y]
	    + lnormal[Z]*ap_p->a_ray.r_dir[Z];
	  if( costheta < 0 ) costheta = (-costheta);
	  if( costheta > COSTOL )
	    {
	      region[icur].ssurarea[whichview][iprev] +=
		(area/costheta);
	      region[iprev].ssurarea[whichview][icur] +=
		(area/costheta);
	    }
	}

      /*  Save point where ray enters region.  */
      enterpt[X]=hitp->hit_point[X];
      enterpt[Y]=hitp->hit_point[Y];
      enterpt[Z]=hitp->hit_point[Z];

      /*  Print out hit point & normal.  */
      /*
       *		(void)printf("Hit Point (entering):  (%f, %f, %f)\n",
       *			hitp->hit_point[X],
       *			hitp->hit_point[Y],
       *			hitp->hit_point[Z]);
       *		(void)fflush(stdout);
       *		(void)printf("Normal Point:  (%f, %f, %f)\n",
       *			hitp->hit_normal[X],
       *			hitp->hit_normal[Y],
       *			hitp->hit_normal[Z]);
       *		(void)fflush(stdout);
       */

      /*  Compute cummulative free surface normal,  */
      /*  free surface area, crew compartment area,  */
      /*  engine compartment area, & other air areas  */
      /*   when ray enters from another region.  */
      if(
	 (-ADJTOL < (hitp->hit_point[X] - leavept[X])) &&
	 ((hitp->hit_point[X] - leavept[X]) < ADJTOL) &&
	 (-ADJTOL < (hitp->hit_point[Y] - leavept[Y])) &&
	 ((hitp->hit_point[Y] - leavept[Y]) < ADJTOL) &&
	 (-ADJTOL < (hitp->hit_point[Z] - leavept[Z])) &&
	 ((hitp->hit_point[Z] - leavept[Z]) < ADJTOL)
	 )
	{
	  if((prevregid == 0) && (pp->pt_regionp->reg_regionid != 0))
	    {
	      costheta2 = hitp->hit_normal[X] * ap_p->a_ray.r_dir[X]
		+ hitp->hit_normal[Y] * ap_p->a_ray.r_dir[Y]
		+ hitp->hit_normal[Z] * ap_p->a_ray.r_dir[Z];
	      if(costheta2 < 0.) costheta2=(-costheta2);
	      if(prevair == 1)
		{
		  if(costheta2 > COSTOL)
		    {
		      region[icur].cumnorm[X] += hitp->hit_normal[X]
			/ costheta2;
		      region[icur].cumnorm[Y] += hitp->hit_normal[Y]
			/ costheta2;
		      region[icur].cumnorm[Z] += hitp->hit_normal[Z]
			/ costheta2;
		      region[icur].cumfs[0][whichview]
			+= (area/costheta2);
		    }
		}
	      else if(prevair == 2)
		{
		  if(costheta2 > COSTOL)
		    region[icur].cumfs[1][whichview]
		      += (area/costheta2);
		}
	      else if(prevair == 5)
		{
		  if(costheta2 > COSTOL)
		    region[icur].cumfs[2][whichview]
		      += (area/costheta2);
		}
	      else if(prevair == 6)	/*  Closed compartment.  */
		{
		  if(costheta2 > COSTOL)
		    region[icur].cumfs[3][whichview]
		      += (area/costheta2);
		}
	      else if(prevair == 7)	/*  Exhaust air.  */
		{
		  if(costheta2 > COSTOL)
		    region[icur].cumfs[4][whichview]
		      += (area/costheta2);
		}
	      else if(prevair == 8)	/*  Generic air 1.  */
		{
		  if(costheta2 > COSTOL)
		    region[icur].cumfs[5][whichview]
		      += (area/costheta2);
		}
	      else if(prevair == 9)	/*  Generic air 2.  */
		{
		  if(costheta2 > COSTOL)
		    region[icur].cumfs[6][whichview]
		      += (area/costheta2);
		}
	    }
	}
      /*  A ray entering from nothing.  */
      else if(pp->pt_regionp->reg_regionid != 0)
	{
	  /*  If the ray has previously hit another  */
	  /*  region this area must be taken into  */
	  /*  account.  */
	  if(iprev != -99)
	    {
	      costheta2 = lnormal[X] * ap_p->a_ray.r_dir[X]
		+ lnormal[Y] * ap_p->a_ray.r_dir[Y]
		+ lnormal[Z] * ap_p->a_ray.r_dir[Z];
	      if(costheta2 < 0.) costheta2 = (-costheta2);
	      if(costheta2 > COSTOL)
		{
		  region[iprev].cumnorm[X] += lnormal[X] / costheta2;
		  region[iprev].cumnorm[Y] += lnormal[Y] / costheta2;
		  region[iprev].cumnorm[Z] += lnormal[Z] / costheta2;
		  region[iprev].cumfs[0][whichview]
		    += (area/costheta2);
		}
	    }

	  costheta2 = hitp->hit_normal[X] * ap_p->a_ray.r_dir[X]
	    + hitp->hit_normal[Y] * ap_p->a_ray.r_dir[Y]
	    + hitp->hit_normal[Z] * ap_p->a_ray.r_dir[Z];
	  if(costheta2 < 0.) costheta2=(-costheta2);
	  if(costheta2 > COSTOL)
	    {
	      region[icur].cumnorm[X] += hitp-> hit_normal[X]
		/ costheta2;
	      region[icur].cumnorm[Y] += hitp-> hit_normal[Y]
		/ costheta2;
	      region[icur].cumnorm[Z] += hitp-> hit_normal[Z]
		/ costheta2;
	      region[icur].cumfs[0][whichview] += (area/costheta2);
	    }
	}
      /*  A ray entering  a region from air.  */
      if((pp->pt_regionp->reg_regionid == 0) && (prevregid != 0))
	{
	  costheta2 = lnormal[X] * ap_p->a_ray.r_dir[X]
	    + lnormal[Y] * ap_p->a_ray.r_dir[Y]
	    + lnormal[Z] * ap_p->a_ray.r_dir[Z];
	  if(costheta2 < 0.) costheta2=(-costheta2);
	  if(pp->pt_regionp->reg_aircode == 1)
	    {
	      if(costheta2 > COSTOL)
		{
		  region[iprev].cumnorm[X] += lnormal[X] / costheta2;
		  region[iprev].cumnorm[Y] += lnormal[Y] / costheta2;
		  region[iprev].cumnorm[Z] += lnormal[Z] / costheta2;
		  region[iprev].cumfs[0][whichview]
		    += (area/costheta2);
		}
	    }
	  else if(pp->pt_regionp->reg_aircode == 2)
	    {
	      if(costheta2 > COSTOL)
		region[iprev].cumfs[1][whichview]
		  += (area/costheta2);
	    }
	  else if(pp->pt_regionp->reg_aircode == 5)
	    {
	      if(costheta2 > COSTOL)
		region[iprev].cumfs[2][whichview]
		  += (area/costheta2);
	    }
	  else if(pp->pt_regionp->reg_aircode == 6)
	    {
	      if(costheta2 > COSTOL)
		region[iprev].cumfs[3][whichview]
		  += (area/costheta2);
	    }
	  else if(pp->pt_regionp->reg_aircode == 7)
	    {
	      if(costheta2 > COSTOL)
		region[iprev].cumfs[4][whichview]
		  += (area/costheta2);
	    }
	  else if(pp->pt_regionp->reg_aircode == 8)
	    {
	      if(costheta2 > COSTOL)
		region[iprev].cumfs[5][whichview]
		  += (area/costheta2);
	    }
	  else if(pp->pt_regionp->reg_aircode == 9)
	    {
	      if(costheta2 > COSTOL)
		region[iprev].cumfs[6][whichview]
		  += (area/costheta2);
	    }
	}

      /*  Curvature.  */
      RT_CURVATURE(&cur,hitp,pp->pt_inflip,stp);
      /*  Print out curvature information.  */
      /*
       *		(void)printf("Principle direction of curvature:  ");
       *		(void)printf("(%f, %f, %f)\n",
       *			cur.crv_pdir[X],
       *			cur.crv_pdir[Y],
       *			cur.crv_pdir[Z]);
       *		(void)fflush(stdout);
       *		(void)printf("Inverse radii of curvature:  c1=%f, c2=%f\n",
       *			cur.crv_c1,
       *			cur.crv_c2);
       *		(void)fflush(stdout);
       */

      /*  Find normal and hit point of leaving ray.  */
      /*
       *		(void)printf("Leaving Ray\n");
       *		(void)fflush(stdout);
       */
      hitp=pp->pt_outhit;
      stp=pp->pt_outseg->seg_stp;
      RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
      /*  Flip normal if needed.  */
      if(pp->pt_outflip)
	{
	  VREVERSE(hitp->hit_normal,hitp->hit_normal);
	  pp->pt_inflip=0;
	}

      /*  Find back surface area of regions. */
      costheta1 = hitp->hit_normal[X] * ap_p->a_ray.r_dir[X]
	+ hitp->hit_normal[Y] * ap_p->a_ray.r_dir[Y]
	+ hitp->hit_normal[Z] * ap_p->a_ray.r_dir[Z];
      if( costheta1 < 0 ) costheta1 = (-costheta1);
      if( costheta1 > COSTOL )
	region[icur].surarea[whichview] += (area/costheta1);

      /*  Save normal of leaving ray.  */
      /*
       *		lnormal[X]=hitp->hit_normal[X];
       *		lnormal[Y]=hitp->hit_normal[Y];
       *		lnormal[Z]=hitp->hit_normal[Z];
       */

      /*  Print out hit point & normal.  */
      /*
       *		(void)printf("Hit Point (leaving):  (%f, %f, %f)\n",
       *			hitp->hit_point[X],
       *			hitp->hit_point[Y],
       *			hitp->hit_point[Z]);
       *		(void)fflush(stdout);
       *		(void)printf("Normal Point:  (%f, %f, %f)\n",
       *			hitp->hit_normal[X],
       *			hitp->hit_normal[Y],
       *			hitp->hit_normal[Z]);
       *		(void)fflush(stdout);
       */

      /*  Continue finding cummulative volume.  */
      disx=(enterpt[X] - hitp->hit_point[X]) *
	(enterpt[X] - hitp->hit_point[X]);
      disy=(enterpt[Y] - hitp->hit_point[Y]) *
	(enterpt[Y] - hitp->hit_point[Y]);
      disz=(enterpt[Z] - hitp->hit_point[Z]) *
	(enterpt[Z] - hitp->hit_point[Z]);
      distance=sqrt(disx + disy + disz);
      region[icur].cumvol[whichview] =
	region[icur].cumvol[whichview] + (distance * area);

      /*  Find centroid.  */
      region[icur].centroid[X] += ( distance * area * (enterpt[X] +
						       hitp->hit_point[X]) / 2.);
      region[icur].centroid[Y] += ( distance * area * (enterpt[Y] +
						       hitp->hit_point[Y]) / 2.);
      region[icur].centroid[Z] += ( distance * area * (enterpt[Z] +
						       hitp->hit_point[Z]) / 2.);

      /*  Find the cummulative normal & free surface area  */
      /*  (exterior air)  when a ray is leaving the bounding  */
      /*  rpp.  */
      if((pp->pt_forw == PartHeadp) &&
	 (pp->pt_regionp->reg_regionid != 0))
	{
	  costheta2 = hitp->hit_normal[X] * ap_p->a_ray.r_dir[X]
	    + hitp->hit_normal[Y] * ap_p->a_ray.r_dir[Y]
	    + hitp->hit_normal[Z] * ap_p->a_ray.r_dir[Z];
	  if(costheta2 < 0.) costheta2=(-costheta2);
	  if(costheta2 > COSTOL)
	    {
	      region[icur].cumnorm[X] += hitp->hit_normal[X]
		/ costheta2;
	      region[icur].cumnorm[Y] += hitp->hit_normal[Y]
		/ costheta2;
	      region[icur].cumnorm[Z] += hitp->hit_normal[Z]
		/ costheta2;
	      region[icur].cumfs[0][whichview] += (area/costheta2);
	    }
	}

      iprev=icur;
      leavept[X]=hitp->hit_point[X];
      leavept[Y]=hitp->hit_point[Y];
      leavept[Z]=hitp->hit_point[Z];

      /*  Save normal of leaving ray.  */
      lnormal[X]=hitp->hit_normal[X];
      lnormal[Y]=hitp->hit_normal[Y];
      lnormal[Z]=hitp->hit_normal[Z];

      /*  Save the region id and air code.  */
      prevregid = pp->pt_regionp->reg_regionid;
      prevair = pp->pt_regionp->reg_aircode;
    }

  return (1);
}

/*  User supplied miss function.  */
int
miss(register struct application *ap_p)
{
  /*
   *	(void)printf("It was a miss.\n");
   *	(void)fflush(stdout);
   */
  return (0);
}

/*  User supplied overlap function.  */
int
ovrlap(register struct application *ap_p, struct partition *PartHeadp, struct region *reg1, struct region *reg2)
{
  int a,b;
  double depth;

  /*
   *	(void)printf("In ovrlap.  ");
   *	(void)fflush(stdout);
   *	(void)printf("%s & %s\n",reg1->reg_name,reg2->reg_name);
   *	(void)fflush(stdout);
   */

  a=(int)reg1->reg_bit;
  b=(int)reg2->reg_bit;

  /*
   *	(void)printf("a=%d, b=%d.  ",a,b);
   *	(void)fflush(stdout);
   */

  /*  Enter region names incase they are never entered  */
  /*  anywhere else.  */
  region[a].regname = (char *)reg1->reg_name;
  region[b].regname = reg2->reg_name;

  overlaps[a].ovrreg[b] = 1;

  /*
   *	(void)printf("ovrreg set to 1.  ");
   *	(void)fflush(stdout);
   */

  depth = PartHeadp->pt_outhit->hit_dist - PartHeadp->pt_inhit->hit_dist;

  /*
   *	(void)printf("depth set to %f.  ",depth);
   *	(void)fflush(stdout);
   */

  if(depth > overlaps[a].ovrdep[b]) overlaps[a].ovrdep[b] = depth;

  /*
   *	(void)printf("ovrdep set.\n");
   *	(void)fflush(stdout);
   */

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
