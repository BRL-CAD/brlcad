/*                     S H A P E F A C T . C
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
/** @file shapefact.c
 *
 *  Program to find shape factors for the engine by firing random rays.
 *  This program fires parallel rays.
 *
 *  Author -
 *	S.Coates - 27 February 1992
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 */

/*	23 May 1991       - Take out lvpt that wasn't necessary.  */
/*		Put in optional dump.  Sum shape factors.  */
/*	30 August 1991    - Put in option to create a generic  */
/*		shape factor file.  */
/*	30 October 1991   - Read region number & name file to get  */
/*		get region number that matches region number from  */
/*		firpass & secpass.  Print out engine area in  */
/*		shapefactor file.  */
/*	 4 November 1991  - Fire reversed ray in order to have  */
/*		reciprocity.  (i.e. sf[ij] * A[i] = sf[ji] * A[j])  */
/*	 8 November 1991  - Fix number of rays to divide by when finding  */
/*		engine area.  */
/*	13 November 1991  - Put check in for rays leaving i & entering j  */
/*		not equal to number of rays leaving j & entering i.  */
/*	15 November 1991  - Change a print statement.  */
/*	22 November 1991  - Change the number of maximum regions to 200.  */
/*	25 November 1991  - Put in print statements for error checking.  */
/*	27 November 1991  - If only air is hit it hit function return 1  */
/*		indicating miss.  */
/*	 3 December 1991  - Add some comments.  */
/*	27 February 1992  - Remove backward firing ray & replace with  */
/*		simpilar solution.  If the ray goes from region i to  */
/*		region j through engine air N(i) & N(i,j) are incremented.  */
/*		Simply increment N(j) & N(j,i) also.  EASY.  Add loop  */
/*		to check reciprocity.  Fix problem with number of rays  */
/*		leaving a region.  To compute engine area the total number  */
/*		of rays leaving a region and entering engine air must be  */
/*		used.  Not the total that leave throught engine air and  */
/*		hit another region.  */
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

/*  Need the following for rt_shootray.  */
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "msr.h"


#if !defined(PI)
#  define PI 3.14159265358979323846262	/*  Pi.  */
#endif
#define ADJTOL 1.e-1	/*  Tolerance for adjacent regions.  */
#define ZEROTOL 1.e-20	/*  Zero tolerance.  */
#define MAXREG 200	/*  Maximum number of regions.  */


struct application ap;	/*  Structure passed between functions.  */

extern int hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp);	/*  User supplied hit function.  */
extern int miss(struct application *ap);	/*  User supplied miss function.  */
extern int overlap(void);	/*  User supplied overlap function.  */

/*  Define structure to hold all information needed.  */
struct table
{
  const char *name;	/*  Region name.  */
  int regnum;		/*  Region number that matches firpass &  */
			/*  secpass.  */
  int numchar;		/*  Number of char each region name has.  */
  double lvrays;	/*  Number of rays that leave the region through  */
			/*  air and hit another region.  */
  double intrays[MAXREG];	/*  Number of rays that leave region  */
  /*  through air and are intercepted.  */
  double allvrays;	/*  All rays that leave a region through engine  */
			/*  air.  */
  double engarea;	/*  Engine area for each region.  */
};
struct table info[MAXREG];
double nummiss;		/*  Number of misses.  */


#ifndef HAVE_DRAND48
/* simulate drand48() --  using 31-bit random() -- assumed to exist */
double drand48() {
  extern long random();
  return (double)random() / 2147483648.0; /* range [0,1) */
}
#endif

int main(int argc, char **argv)
{
  extern struct table info[];	/*  Structure is external.  */
  struct rt_i *rtip;
  int index;		/*  Index for rt_dirbuild & rt_gettree.  */
  char idbuf[32];	/*  Contains database name.  */
  struct region *pr;	/*  Used in finding region names.  */
  double rho,phi,theta;/*  Spherical coordinates for starting point.  */
  double areabs=0.0;	/*  Area of bounding sphere (mm**2).  */
  int ians;		/*  Answer of question.  */
  double strtpt[3];	/*  Starting point of ray.  */
  double strtdir[3];	/*  Starting direction.  */
  double loops;	/*  Number of rays fired.  */
  double r;		/*  Variable in loops.  */
  int i,j,k;		/*  Variable in loops.  */
  long seed;		/*  Initial seed for random number generator.  */
  double denom;	/*  Denominator.  */
  double elev;		/*  Elevation, used to find point on yz-plane.  */
  double az;		/*  Azimuth, used to find point on yz-plane.  */
  double rad;		/*  Radius, used to find point on yz-plane.  */
  double s[3],t[3];	/*  Temporary variables used to find points.  */
  double q;		/*  Temporary variable used to find points.  */
  int numreg;		/*  Number of regions.  */
  double center[3];	/*  Center of the bounding rpp.  */
  double sf;		/*  Used to print shape factor.  */
  double dump;		/*  How often a dump is to occur.  */
  int idump;		/*  1=>dump to occur.  */

  FILE *fp;		/*  Used to open files.  */
  char outfile[16];	/*  Output file.  */
  FILE *fp1;		/*  Used to read region # & name file.  */
  char rnnfile[16];	/*  Region # & name file.  */
  FILE *fp2;		/*  Used to write the error file.  */
  char errfile[16];	/*  Error file.  */
  double totalsf;	/*  Sum of shape factors.  */
  double totalnh;	/*  Sum of number of hits.  */
  int itype;		/*  Type of file to be created, 0=>regular,  */
			/*  1=>generic.  */
  char line[500];	/*  Buffer to read a line of data into.  */
  int c;		/*  Reads one character of information.  */
  int icnt;		/*  Counter for shape factor.  */
  char tmpname[150];	/*  Temporary name.  */
  int tmpreg;		/*  Temporary region number.  */
  char rnnname[800][150];  /*  Region name from region # & name file.  */
  int rnnnum;		/*  Number of regions in region # & name file.  */
  int rnnchar[800];	/*  Number of characters in name.  */
  int rnnreg[800];	/*  Region number from region # & name file.  */
  int jcnt;		/*  Counter.  */
  int equal;		/*  0=>equal, 1=>not equal.  */
  double rcpi,rcpj;	/*  Used to check reciprocity.  */
  double rcp_diff;	/*  Difference in reciprocity.  */
  double rcp_pdiff;	/*  Percent difference in reciprocity.  */

  /*  Check to see if arguments are implimented correctly.  */
  if( (argv[1] == NULL) || (argv[2] == NULL) )
    {
      (void)fprintf(stderr,"\nusage:  %s file.g objects\n\n", *argv);
    }
  else
    {						/*  START # 1  */

      /*  Ask what type of file is to be created - regualar  */
      /*  or generic.  */
      (void)printf("Enter type of file to be written (0=>regular or ");
      (void)printf("1=>generic).  ");
      (void)fflush(stdout);
      (void)scanf("%d",&itype);
      if(itype != 1) itype = 0;

      /*  Enter names of files to be used.  */
      (void)fprintf(stderr,"Enter name of output file (15 char max).\n\t");
      (void)fflush(stderr);
      (void)scanf("%s",outfile);

      /*  Read name of the error file to be written.  */
      (void)printf("Enter the name of the error file (15 char max).\n\t");
      (void)fflush(stdout);
      (void)scanf("%s",errfile);

      /*  Enter name of region # & name file to be read.  */
      {
	(void)printf("Enter region # & name file to be read ");
	(void)printf("(15 char max).\n\t");
	(void)fflush(stdout);
	(void)scanf("%s",rnnfile);
      }

      /*  Check if dump is to occur.  */
      idump = 0;
      (void)printf("Do you want to dump intermediate shape factors to ");
      (void)printf("screen (0-no, 1-yes)?  ");
      (void)fflush(stdout);
      (void)scanf("%d",&idump);

      /*  Find number of rays to be fired.  */
      (void)fprintf(stderr,"Enter number of rays to be fired.  ");
      (void)fflush(stderr);
      (void)scanf("%lf",&loops);

      /*  Set seed for random number generator.  */
      seed = 1;
      (void)fprintf(stderr,"Do you wish to enter your own seed (0) or ");
      (void)fprintf(stderr,"use the default of 1 (1)?  ");
      (void)fflush(stderr);
      (void)scanf("%d",&ians);
      if(ians == 0)
	{
	  (void)fprintf(stderr,"Enter unsigned integer seed.  ");
	  (void)fflush(stderr);
	  (void)scanf("%ld",&seed);
	}
#ifdef MSRMAXTBL
      msr = msr_unif_init(seed, 0);
#else
#  ifndef HAVE_SRAND48
      (void) srandom(seed);
#  else
      (void) srand48(seed);
#  endif
#endif
      rt_log("seed initialized\n");

      /*  Read region # & name file.  */
      rnnnum = 0;
      fp1 = fopen(rnnfile,"r");
      /*
       *	 (void)printf("Region # & name file opened.\n");
       *	 (void)fflush(stdout);
       */
      c = getc(fp1);
      while(c != EOF)
	{
	  (void)ungetc(c,fp1);
	  (void)fgets(line,200,fp1);
	  (void)sscanf(line,"%d%s",&tmpreg,tmpname);
	  for(i=0; i<150; i++)
	    {
	      rnnname[rnnnum][i] = tmpname[i];
	    }
	  rnnreg[rnnnum] = tmpreg;
	  rnnnum++;
	  c = getc(fp1);
	}
      (void)fclose(fp1);
      /*
       *	 (void)printf("Region # & name file closed.\n");
       *	 (void)fflush(stdout);
       */

      (void)printf("Number of regions read from region # & name file:  %d\n",		rnnnum);
      (void)fflush(stdout);

      /*  Find number of characters in each region name.  */
      for(i=0; i<rnnnum; i++)
	{
	  jcnt = 0;
	  while(rnnname[i][jcnt] != '\0')
	    {
	      jcnt++;
	    }
	  rnnchar[i] = jcnt;
	  /*
	   *	   (void)printf("number of char, i= %d:  %d\n",i,rnnchar[i]);
	   *	   (void)fflush(stdout);
	   */
	}

      /*  Check to see that region # & name file read correctly.  */
      /*
       *	 for(i=0; i<rnnnum; i++)
       *	 {
       *	   (void)printf("%d\t%d\t-%s-\n",i,rnnchar[i],rnnname[i]);
       *	   (void)fflush(stdout);
       *	 }
       */

      /*  Build directory.  */
      index = 1;	/*  Set index for rt_dirbuild.  */
      rtip = rt_dirbuild(argv[index],idbuf,sizeof(idbuf));
      (void)printf("Database Title:  %s\n",idbuf);
      (void)fflush(stdout);

      /*  Set useair to 1 to show hits of air.  */
      rtip->useair = 1;

      /*  Load desired objects.  */
      index = 2;	/*  Set index.  */
      while(argv[index] != NULL)
	{
	  rt_gettree(rtip,argv[index]);
	  index += 1;
	}

      /*  Find number of regions.  */
      numreg = (int)rtip->nregions;

      (void)fprintf(stderr,"Number of regions:  %d\n",numreg);
      (void)fflush(stderr);

      /*  Zero all arrays.  */
      for(i=0; i<numreg; i++)
	{
	  info[i].name = "\0";
	  info[i].regnum = (-1);
	  info[i].numchar = 0;
	  info[i].lvrays = 0.;
	  info[i].engarea = 0.;
	  for(j=0; j<numreg; j++)
	    {
	      info[i].intrays[j] = 0.;
	    }
	}
      nummiss = 0.;

      /*  Get database ready by starting prep.  */
      rt_prep(rtip);

      /*  Find the center of the bounding rpp.  */
      center[X] = rtip->mdl_min[X] +
	(rtip->mdl_max[X] - rtip->mdl_min[X]) / 2.;
      center[Y] = rtip->mdl_min[Y] +
	(rtip->mdl_max[Y] - rtip->mdl_min[Y]) / 2.;
      center[Z] = rtip->mdl_min[Z] +
	(rtip->mdl_max[Z] - rtip->mdl_min[Z]) / 2.;

      /*  Put region names into structure.  */
      pr = BU_LIST_FIRST(region, &rtip->HeadRegion);
      for(i=0; i<numreg; i++)
	{
	  info[(int)(pr->reg_bit)].name = pr->reg_name;
	  pr = BU_LIST_FORW(region, &(pr->l) );
	}
      /*
       *	for(i=0; i<numreg; i++)
       *	{
       *		(void)printf("%d - %s\n",i,info[i].name);
       *		(void)fflush(stdout);
       *	}
       */

      /*
       *	(void)printf("Region names in structure.\n");
       *	(void)fflush(stdout);
       */

      /*  Set up parameters for rt_shootray.  */
      ap.a_hit = hit;		/*  User supplied hit function.  */
      ap.a_miss = miss;	/*  User supplied miss function.  */
      ap.a_overlap = overlap;	/*  User supplied overlap function.  */
      ap.a_rt_i = rtip;	/*  Pointer from rt_dirbuild.  */
      ap.a_onehit = 0;	/*  Look at all hits.  */
      ap.a_level = 0;		/*  Recursion level for diagnostics.  */
      ap.a_resource = 0;	/*  Address for resource structure.  */

      dump = 1000000.;	/*  Used for dumping info.  */

      for(r=0; r<loops; r++)	/*  Number of rays fired.  */
	{					/*  START # 2  */
	  /*
	   *	   (void)fprintf(stderr,"loop # %f\n",r);
	   */

	  /*  Find length of 'diagonal' (rho).  (In reality rho is  */
	  /*  the radius of bounding sphere).  */
	  rho = (rtip->mdl_max[X] - rtip->mdl_min[X])
	    * (rtip->mdl_max[X] - rtip->mdl_min[X])
	    +(rtip->mdl_max[Y] - rtip->mdl_min[Y])
	    * (rtip->mdl_max[Y] - rtip->mdl_min[Y])
	    +(rtip->mdl_max[Z] - rtip->mdl_min[Z])
	    * (rtip->mdl_max[Z] - rtip->mdl_min[Z]);
	  rho = sqrt(rho) / 2. + .5;

	  /*  find surface area of bounding sphere.  */
	  areabs = 4. * PI * rho * rho;

	  /*  Second way to find starting point and direction.  */
	  /*  This approach finds the starting point and direction  */
	  /*  by using parallel rays.  */

	  /*  Find point on the bounding sphere.  (The negative  */
	  /*  of the unit vector of this point will eventually be  */
	  /*  the firing direction.  */
	  /*
	   *	   (void)printf("\n\nrho:  %f\n",rho);
	   *	   (void)fflush(stdout);
	   */
#ifdef MSRMAXTBL
	  q = MSR_UNIF_DOUBLE(msr) + 0.5;
#else
	  q = drand48();
#endif
	  theta = q * 2. * PI;
	  /*
	   *	   (void)printf("random number:  %f, theta:  %f\n",q,theta);
	   *	   (void)fflush(stdout);
	   */
#ifdef MSRMAXTBL
	  q = MSR_UNIF_DOUBLE(msr) + 0.5;
#else
	  q = drand48();
#endif
	  phi = ( q * 2.) - 1.;
	  phi = acos(phi);
	  /*
	   *	   (void)printf("random number:  %f, phi:  %f\n",q,phi);
	   *	   (void)fflush(stdout);
	   */
	  strtdir[X] = rho * sin(phi) * cos(theta);
	  strtdir[Y] = rho * sin(phi) * sin(theta);
	  strtdir[Z] = rho * cos(phi);
	  /*
	   *	   (void)printf("starting direction:  %f, %f, %f\n",strtdir[X],
	   *		strtdir[Y],strtdir[Z]);
	   *	   (void)fflush(stdout);
	   */

	  /*  Elevation and azimuth for finding a vector in a plane.  */
	  elev = PI / 2. - phi;
	  az = theta;
	  /*
	   *	   (void)printf("elevation:  %f, azimuth:  %f\n",elev,az);
	   *	   (void)fflush(stdout);
	   */

	  /*  Find vector in yz-plane.  */

#ifdef MSRMAXTBL
	  q = MSR_UNIF_DOUBLE(msr) + 0.5;
#else
	  q = drand48();
#endif
	  theta = q * 2. * PI;
	  /*
	   *	   (void)printf("random number:  %f, theta:  %f\n",q,theta);
	   *	   (void)fflush(stdout);
	   */
#ifdef MSRMAXTBL
	  q = MSR_UNIF_DOUBLE(msr) + 0.5;
#else
	  q = drand48();
#endif
	  rad = rho * sqrt( q );
	  /*
	   *	   (void)printf("random number:  %f, rad:  %f\n",q,rad);
	   *	   (void)fflush(stdout);
	   */
	  s[X] = 0.;
	  s[Y] = rad * cos(theta);
	  s[Z] = rad * sin(theta);
	  /*
	   *	   (void)printf("vector in yz-plane:  %f, %f, %f\n",s[X],s[Y],s[Z]);
	   *	   (void)fflush(stdout);
	   */

	  /*  Rotate vector.  */
	  t[X] = s[X] * cos(elev) * cos(az) - s[Z] * sin(elev) * cos(az)
	    - s[Y] * sin(az);
	  t[Y] = s[X] * cos(elev) * sin(az) - s[Z] * sin(elev) * sin(az)
	    + s[Y] * cos(az);
	  t[Z] = s[X] * sin(elev) + s[Z] * cos(elev);
	  /*
	   *	   (void)printf("rotated vector:  %f, %f, %f\n",t[X],t[Y],t[Z]);
	   *	   (void)fflush(stdout);
	   */

	  /*  Translate the point.  This is starting point.  */
	  strtpt[X] = t[X] + strtdir[X];
	  strtpt[Y] = t[Y] + strtdir[Y];
	  strtpt[Z] = t[Z] + strtdir[Z];
	  /*
	   *	   (void)printf("translated point:  %f, %f, %f\n",strtpt[X],
	   *		strtpt[Y],strtpt[Z]);
	   *	   (void)fflush(stdout);
	   */

	  /*  Now transfer starting point so that it is in  */
	  /*  the absolute coordinates not the origin's.  */
	  strtpt[X] += center[X];
	  strtpt[Y] += center[Y];
	  strtpt[Z] += center[Z];

	  /*
	   *	   (void)printf("point in absolute coordinates:  %f, %f, %f\n",
	   *		strtpt[X],strtpt[Y],strtpt[Z]);
	   *	   (void)fflush(stdout);
	   */

	  /*  Normalize starting direction and make negative.  */
	  denom = strtdir[X] * strtdir[X] +
	    strtdir[Y] * strtdir[Y] +
	    strtdir[Z] * strtdir[Z];
	  denom = sqrt(denom);
	  strtdir[X] /= (-denom);
	  strtdir[Y] /= (-denom);
	  strtdir[Z] /= (-denom);

	  /*
	   *	   (void)printf("starting direction (normalized):  %f, %f, %f\n",
	   *		strtdir[X],strtdir[Y],strtdir[Z]);
	   *	   (void)fflush(stdout);
	   */

	  /*  Set up firing point and direction.  */
	  ap.a_ray.r_pt[X] = strtpt[X];
	  ap.a_ray.r_pt[Y] = strtpt[Y];
	  ap.a_ray.r_pt[Z] = strtpt[Z];
	  ap.a_ray.r_dir[X] = strtdir[X];
	  ap.a_ray.r_dir[Y] = strtdir[Y];
	  ap.a_ray.r_dir[Z] = strtdir[Z];

	  /*
	   *	   (void)printf("Calling rt_shootray.\n");
	   *	   (void)fflush(stdout);
	   */

	  /*  Call rt_shootray for "forward ray".  */
	  (void)rt_shootray(&ap);

	  /*
	   *	   (void)printf("Finished rt_shootray.\n");
	   *	   (void)fflush(stdout);
	   */

	  /*
	   *	   (void)printf("r = %f\n",r);
	   *	   (void)fflush(stdout);
	   */

	  if( r == (dump - 1.) )
	    {
	      (void)printf("%f rays have been fired in forward direction.\n",
			   (r+1));
	      (void)fflush(stdout);
	      if(idump == 1)
		{						/*  START # 3  */
		  (void)printf("\n****************************************");
		  (void)printf("****************************************\n");
		  (void)fflush(stdout);
		  /*  Dump info to file.  */
		  for(i=0; i<numreg; i++)
		    {
		      for(j=0; j<numreg; j++)
			{
			  sf = 0.;
			  if( (info[i].lvrays < -ZEROTOL) || (ZEROTOL <
							      info[i].lvrays) )
			    sf = info[i].intrays[j] / info[i].lvrays;

			  (void)printf("%d\t%d\t%f\n",i,j,sf);
			  (void)fflush(stdout);
			}
		    }
		}						/*  START # 3  */

	      dump = dump + 1000000.;
	    }

	}					/*  END # 2  */

      /*  Find area bounded by engine air using Monte Carlo method.  */
      for(i=0; i<numreg; i++)
	{
	  /*  Old way, only incrementing info[i].allvrays for forward  */
	  /*  ray therefore do not divide by 2.  Division by 2 is to  */
	  /*  include the backwards ray also.  */
	  /*
	   *	  info[i].engarea = info[i].allvrays * areabs / loops / 2.;
	   */
	  info[i].engarea = info[i].allvrays * areabs / loops;

	  /*  Put area into square meters.  */
	  info[i].engarea *= 1.e-6;
	}

      /*  Find number of characters in each region name.  */
      for(i=0; i<numreg; i++)
	{
	  jcnt = 0;
	  while(info[i].name[jcnt] != '\0')
	    {
	      jcnt++;
	    }
	  info[i].numchar = jcnt;
	}
      /*  Print out number of char & region name.  */
      /*
       *	   for(i=0; i<numreg; i++)
       *	   {
       *		(void)printf("%d\t-%s-\n",info[i].numchar,info[i].name);
       *		(void)fflush(stdout);
       *	   }
       */

      /*  Find correct region number.  */
      (void)printf("Finding correct region numbers.\n");
      (void)fflush(stdout);
      for(i=0; i<numreg; i++)
	{
	  for(j=0; j<rnnnum; j++)
	    {
	      equal = 0;	/*  1=>not equal.  */
	      jcnt = rnnchar[j];
	      for(k=info[i].numchar; k>=0; k--)
		{
		  /*
		   *			(void)printf("i=%d, j=%d, k=%d, jcnt=%d, -%c-, -%c-\n",
		   *			   i,j,k,jcnt,info[i].name[k],rnnname[j][jcnt]);
		   *			(void)fflush(stdout);
		   */
		  if(jcnt<0) equal = 1;
		  else if(info[i].name[k] != rnnname[j][jcnt])
		    equal = 1;
		  jcnt--;
		}
	      if(equal == 0) info[i].regnum = rnnreg[j];
	    }
	}
      (void)printf("Finished finding correct region numbers.\n");
      (void)fflush(stdout);

      /******************************************************************/

      /*  Check for reciprocity.  */
      /*  Open error file.  */
      fp2 = fopen(errfile,"w");
      (void)fprintf(fp2,"\nError file for shapefact.\n");
      (void)fprintf(fp2,"Shape factor file created:  %s\n\n",outfile);
      (void)fprintf(fp2,"Regions with reciprocity errors greater ");
      (void)fprintf(fp2,"than 10%%.\n\n");
      (void)fflush(fp2);

      for(i=0; i<numreg; i++)
	{
	  for(j=0; j<numreg; j++)
	    {
	      rcpi = 0.;
	      rcpj = 0.;
	      if( (info[i].lvrays < -ZEROTOL) || (ZEROTOL < info[i].lvrays) )
		rcpi = info[i].intrays[j] * info[i].engarea /info[i].lvrays;
	      if( (info[j].lvrays < -ZEROTOL) || (ZEROTOL < info[j].lvrays) )
		rcpj = info[j].intrays[i] * info[j].engarea /info[j].lvrays;
	      rcp_diff = rcpi - rcpj;
	      if(rcp_diff < 0.) rcp_diff = (-rcp_diff);
	      if( (rcpi < -ZEROTOL) || (ZEROTOL < rcpi) )
		rcp_pdiff = rcp_diff / rcpi;
	      else rcp_pdiff = 0.;	/*  Don't divide by 0.  */
	      /*  Print reciprocity errors greater than 10%.  */
	      if(rcp_pdiff > 0.1)
		{
		  (void)fprintf(fp2,"%d   %d   %f   %f   %f   %f\n",
				info[i].regnum,info[j].regnum,rcpi,rcpj,rcp_diff,
				(rcp_pdiff * 100.));
		  (void)fflush(fp2);
		}
	    }
	}
      /*  Close error file.  */
      (void)fclose(fp2);

      /******************************************************************/

      /*  Print out shape factor to regular output file.  */
      if(itype == 0)
	{
	  fp = fopen(outfile,"w");
	  (void)fprintf(fp,"Number of forward rays fired:  %f\n\n",loops);
	  (void)fflush(fp);

	  /*  Print out structure.  */
	  for(i=0; i<numreg; i++)
	    {
	      /*  Print region number, region name, & engine area.  */
	      (void)fprintf(fp,"%d\t%s\t%e\n",
			    info[i].regnum,info[i].name,info[i].engarea);
	      (void)fflush(fp);

	      /*  Zero sums for shape factors & rays hit.  */
	      totalsf = 0.;
	      totalnh = 0.;

	      for(j=0; j<numreg; j++)
		{
		  sf = 0.;
		  if( (info[i].lvrays < -ZEROTOL) || (ZEROTOL <
						      info[i].lvrays) )
		    sf = info[i].intrays[j] / info[i].lvrays;

		  /*  Print region number & shape factor.  */
		  (void)fprintf(fp,"   %d\t%e\n",
				info[j].regnum,sf);
		  (void)fflush(fp);

		  /*  Print if number of rays leaving i & entering j is  */
		  /*  not the same as the number of rays leaving j &   */
		  /*  entering i.  */
		  /*
		   *		if( info[i].intrays[j] != info[j].intrays[i] )
		   *		{
		   *		   stemp = info[i].intrays[j] - info[j].intrays[i];
		   *		   if(stemp < 0) stemp = (-stemp);
		   *		   (void)fprintf(fp,"     %e (%e) - %e (%e) - %e\n",
		   *			info[i].intrays[j],(stemp/info[i].intrays[j]),
		   *			info[j].intrays[i],(stemp/info[j].intrays[i]),stemp);
		   *		   (void)fflush(fp);
		   *		}
		   */

		  /*  Add to sum of shape factors & number of hits.  */
		  totalsf += sf;
		  totalnh += info[i].intrays[j];

		}

	      /*  Print sum of hits & sum of shape factors.  */
	      (void)fprintf(fp,"  sum of hits:  %f\n",totalnh);
	      (void)fprintf(fp,"  sum of shape factors:  %f\n\n",totalsf);
	      (void)fflush(fp);
	    }
	  (void)fclose(fp);
	}

      /******************************************************************/

      /*  Create and write to generic shape factor file.  */
      if(itype == 1)
	{
	  fp = fopen(outfile,"w");
	  for(i=0; i<numreg; i++)
	    {
	      /*  Count the number of shape factors.  */
	      icnt = 0;
	      for(j=0; j<numreg; j++)
		{
		  if(info[i].intrays[j] > ZEROTOL) icnt++;
		}
	      /*  Print the # 5, region number (matches firpass &  */
	      /*  secpass), engine area, & number of shape factors.  */
	      (void)fprintf(fp," 5  %d  %e  %d\n",
			    info[i].regnum,info[i].engarea,icnt);
	      (void)fflush(fp);
	      for(j=0; j<numreg; j++)
		{
		  if(info[i].intrays[j] > ZEROTOL)
		    {
		      sf = info[i].intrays[j] / info[i].lvrays;
		      /*  Print each region # & shape factor.  */
		      (void)fprintf(fp,"    %d  %e\n",info[j].regnum,sf);
		      (void)fflush(fp);
		    }
		}
	    }
	  (void)fclose(fp);
	}

    }						/*  END # 1  */
  return(0);
}


/*****************************************************************************/
/*		Hit, miss, and overlap functions.                            */
/*****************************************************************************/
/*  User supplied hit function.  */
int
hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp)
{						/*  START # 0H  */
  extern struct table info[];	/*  Structure is external.  */
  register struct partition *pp;
  register struct hit *hitp;
  register struct soltab *stp;
  int icur=0;			/*  Current region hit.  */
  int iprev;			/*  Previous region hit.  */
  int iair;			/*  Type of air or region came from,  */
				/*  0=>region, 1=>exterior air, 2=>crew  */
				/*  air, 5=>engine air, 6=>closed  */
				/*  compartment air, 7=>exhaust air,  */
				/*  8=>generic air 1, 9=>generic air 2.  */

  /*
   * (void)printf("In hit function.\n");
   * (void)fflush(stdout);
   */

  /*  Set beginning parameters.  */
  iprev = -1;
  iair = 1;		/*  Comes from exterior air.  */

  /*
   * (void)printf("Beginning loop again.\n");
   * (void)fflush(stdout);
   */

  pp = PartHeadp->pt_forw;
  for( ; pp != PartHeadp;  pp = pp->pt_forw)
    {						/*  START # 1H  */
      /*
       *	(void)printf("Region %d - %s - %d - %d - %d - \n",
       *		pp->pt_regionp->reg_bit,
       *		info[(int)pp->pt_regionp->reg_bit].name,
       *		pp->pt_regionp->reg_gmater,
       *		pp->pt_regionp->reg_aircode,
       *		pp->pt_regionp->reg_regionid);
       *	(void)fflush(stdout);
       */

      if(iair == 1)	/*  Ray comes from nothing (exterior air).  */
	{					/*  START # 2H  */
	  /*
	   *	   (void)printf("Ray comes from exterior air (1).\n");
	   *	   (void)fflush(stdout);
	   */

	  if(pp->pt_regionp->reg_regionid > (short)0)	/*  Hit region.  */
	    {					/*  START # 3H  */
	      /*  Region number hit.  */
	      icur = (int)(pp->pt_regionp->reg_bit);

	      /*  Find leaving point.  */
	      hitp = pp->pt_outhit;
	      stp = pp->pt_outseg->seg_stp;
	      RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
	      /*  Flip normal if needed.  */
	      if(pp->pt_outflip)
		{
		  VREVERSE(hitp->hit_normal,hitp->hit_normal);
		  pp->pt_outflip = 0;
		}
	      iprev = icur;
	      iair = 0;	/*  A region was just hit.  */
	    }					/*  END # 3H  */

	  else	/*  Hit air.  */
	    {					/*  START # 4H  */
	      iair = pp->pt_regionp->reg_aircode;
	    }					/*  END # 4H  */
	}					/*  END # 2H  */

      else if(iair == 5)	/*  Ray comes from engine air.  */
	{					/*  START # 5H  */
	  /*
	   *	   (void)printf("Ray comes from engine air (5).\n");
	   *	   (void)fflush(stdout);
	   */

	  if(pp->pt_regionp->reg_regionid > (short)0)	/*  Hit region.  */
	    {					/*  START # 6H  */
	      /*  Region number hit.  */
	      icur = (int)(pp->pt_regionp->reg_bit);

	      /*  Only execute the following two statements if iprev >= 0.  */
	      if(iprev < (-1))
		{
		  (void)fprintf(stderr,"ERROR -- iprev = %d\n",iprev);
		  (void)fflush(stderr);
		}
	      if(iprev == (-1))
		{
		  (void)fprintf(stderr,"iprev = %d - entered ",iprev);
		  (void)fprintf(stderr,"through engine air\n");
		  (void)fflush(stderr);
		}
	      if(iprev > (-1))
		{
		  /*  Add one to number of rays leaving previous region.  */
		  info[iprev].lvrays++;

		  /*  Add one to the number of rays leaving current region  */
		  /*  for the backward ray.  */
		  info[icur].lvrays++;

		  /*  Add one to number of rays leaving previous region and  */
		  /*  intercepted by current region.  */
		  info[iprev].intrays[icur]++;

		  /*  Add one to the number of rays leaving the current  */
		  /*  region and intersecting the previous region for the  */
		  /*  backward ray.  */
		  info[icur].intrays[iprev]++;

		}

	      /*  Find leave point.  */
	      hitp = pp->pt_outhit;
	      stp = pp->pt_outseg->seg_stp;
	      RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
	      /*  Flip normal if needed.  */
	      if(pp->pt_outflip)
		{
		  VREVERSE(hitp->hit_normal,hitp->hit_normal);
		  pp->pt_outflip = 0;
		}
	      iprev = icur;
	      iair = 0;	/*  Hit a region.  */
	    }					/*  END # 6H  */

	  else	/*  Hit air.  */
	    {
	      iair = 5;	/*  Since coming through engine air  */
	      /*  assume should still be engine air.  */
	    }
	}					/*  END # 5H  */

      else if(iair == 0)	/*  Ray comes from a region.  */
	{					/*  START # 7H  */
	  /*
	   *	   (void)printf("Ray comes from region (0).\n");
	   *	   (void)fflush(stdout);
	   */

	  if(pp->pt_regionp->reg_regionid > (short)0)	/*  Hit a region.  */
	    {					/*  START # 8H  */
	      /*  Region number hit.  */
	      icur = (int)(pp->pt_regionp->reg_bit);

	      /*  Find leaving point.  */
	      hitp = pp->pt_outhit;
	      stp = pp->pt_outseg->seg_stp;
	      RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
	      /*  Flip normal if needed.  */
	      if(pp->pt_outflip)
		{
		  VREVERSE(hitp->hit_normal,hitp->hit_normal);
		  pp->pt_outflip = 0;
		}
	      iprev = icur;
	      iair = 0;	/*  Hit a region.  */
	    }					/*  END # 8H  */

	  else	/*  Hit air.  */
	    {					/*  START # 9H  */
	      /*  Increment allvrays if the ray is leaving through  */
	      /*  engine air.  Make sure this is only done once.  */
	      if( (iair != 5) && (pp->pt_regionp->reg_aircode == 5) )
		info[icur].allvrays++;

	      if(iair != 5) iair = pp->pt_regionp->reg_aircode;
	    }					/*  END # 9H  */
	}					/*  END # 7H  */

      else	/*  Ray comes from any interior air.  */
	{					/*  START # 10H  */
	  /*
	   *	   (void)printf("Ray comes from interior air (not 0, 1, 5).\n");
	   *	   (void)fflush(stdout);
	   */

	  if(pp->pt_regionp->reg_regionid > (short)0)	/*  Hits region.  */
	    {					/*  START # 11H  */
	      /*  Region number hit.  */
	      icur = (int)(pp->pt_regionp->reg_bit);

	      /*  Find leaving point.  */
	      hitp = pp->pt_outhit;
	      stp = pp->pt_outseg->seg_stp;
	      RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
	      /*  Flip normal if needed.  */
	      if(pp->pt_outflip)
		{
		  VREVERSE(hitp->hit_normal,hitp->hit_normal);
		  pp->pt_outflip = 0;
		}
	      iprev = icur;
	      iair = 0;	/*  Hit region.  */
	    }					/*  END # 11H  */

	  else	/*  Hits air.  */
	    {
	      iair = pp->pt_regionp->reg_aircode;
	    }
	}					/*  END # 10H  */
    }						/*  END # 1H  */

  if(iprev == (-1) )		/*  Went through air only.  */
    {
      return(1);		/*  Indicates miss.  */
    }

  else
    {
      return(0);
    }
}						/*  END # 0H  */

/*  User supplied miss function.  */
int
miss(struct application *ap)
{
  /*
   * (void)printf("In miss function.\n");
   * (void)fflush(stdout);
   */

  nummiss = nummiss + 1.;

  return(1);
}

/*  User supplied overlap function.  */
int
overlap(void)
{
  /*
   * (void)printf("In overlap function.\n");
   * (void)fflush(stdout);
   */

  return(2);
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
