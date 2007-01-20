/*                        A L L _ S F . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
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
/** @file all_sf.c
 *
 *  Program to find shape factors for an entire vehicle by firing
 *  one set of rays & tracking hits along the entire ray.
 *
 *  Author -
 *      S.Coates - 22 February 1993
 *
 *  Source -
 *      SECAD/VLD Computing Consortium, Bldg 394
 *      The U. S. Army Ballistic Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5066
 *
 */

/*	22 February 1993 - Original copy.  */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
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


extern int hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp);       /*  User supplied hit function.  */
extern int miss(struct application *ap);      /*  User supplied miss function.  */
extern int overlap(void);   /*  User supplied overlap function.  */

struct table
{
   const char *name;	/*  Region name.  */
   double lvrays;	/*  Number of rays that leave a region &  */
			/*  hit air or nothing.  */
   double *intrays;	/*  Number of rays that leave a region &  */
			/*  hit air or nothing & then hit another  */
			/*  region.  */
   double regarea;	/*  Surface area of region.  */
   double *sf;		/*  Shape factor of each region.  */
};
struct table *info;	/*  Structure that contains all info.  */

FILE *fpw1;		/*  Allows file to be written to from all func.  */

#ifndef HAVE_DRAND48
/* simulate drand48() --  using 31-bit random() -- assumed to exist */
double drand48() {
  extern long random();
  return (double)random() / 2147483648.0; /* range [0,1) */
}
#endif

int main(int argc, char **argv)
{							/*  START # 99  */
   struct application ap;  /*  Structure passed between functions.  */
   struct rt_i *rtip;	/*  Used to build directory.  */
   int index;		/*  Index for rt_dirbuild & rt_gettree.  */
   char idbuf[32];	/*  Contains data base info.  */
   struct region *pr;   /*  Used in finding region names.  */

   int numreg;		/*  Number of regions.  */
   double centall[3];	/*  Center of entire model.  */
   double minall[3];	/*  Minimum of entire model.  */
   double maxall[3];	/*  Maximum of entire model.  */
   double areaall;	/*  Surface area of bounding sphere.  */
   double radall;	/*  Radius of bounding sphere.  */
   long seed;		/*  Seed for random number generator.  */
   double rayfir;	/*  Number of rays to be fired.  */
   double rho,phi,theta;/*  Spherical coordinates for starting point.  */
   double elev,az,rds;	/*  Elevation, azimuth, & radius for finding vector  */
			/*  in yz-plane.  */
   double strtpt[3];	/*  Starting point.  */
   double strtdir[3];	/*  Starting direction.  */
   double denom;	/*  Denominator.  */

   int ians;		/*  Answer to question.  */
   int i,j,k,m;		/*  Loop counters.  */
   double r;		/*  Loop counter.  */
   double q;		/*  Temporary variable.  */
   double s[3],t[3];	/*  Temporary variables.  */

   FILE *fpw;		/*  Allows a file to be written.  */
   char outfile[26];	/*  Output file name.  */
   char errfile[26];	/*  Error file name.  */
   FILE *fpw2;		/*  Allows a file to be written.  */
   char lwxfile[26];	/*  Longwave radiation exchange file for PRISM  */
			/*  (not quite PRISM ready).  */

   /*  Check to see if arguments are implimented correctly.  */
   if( (argv[1] == NULL) || (argv[2] == NULL) )
   {
        (void)fprintf(stderr,"\nusage:  %s file.g objects\n\n", *argv);
   }

   else
   {							/*  START # 100  */

	/*  Find name of output file.  */
	(void)printf("Enter name of output file (25 char max).\n\t");
	(void)fflush(stdout);
	(void)scanf("%s",outfile);

	/*  Find name of longwave radiation exchange (lwx) file  */
	/*  for use with PRISM (not quite PRISM ready).  */
	(void)printf("Enter name of longwave radiation exchange");
	(void)printf(" file (25 char max).\n\t");
	(void)fflush(stdout);
	(void)scanf("%s",lwxfile);

	/*  Find name of error file.  */
	(void)printf("Enter name of error file (25 char max).\n\t");
	(void)fflush(stdout);
	(void)scanf("%s",errfile);

	/*  Open files.  */
	fpw = fopen(outfile,"w");
	fpw1 = fopen(errfile,"w");
	fpw2 = fopen(lwxfile,"w");

	/*  Write info to output & error file.  */
	(void)fprintf(fpw,"\n.g file used:  %s\n",argv[1]);
	(void)fprintf(fpw,"regions used:\n");
	(void)fprintf(fpw1,"\n.g file used:  %s\n",argv[1]);
	(void)fprintf(fpw1,"regions used:\n");
	i = 2;
	while(argv[i] != NULL)
	{
	   (void)fprintf(fpw,"\t%s\n",argv[i]);
	   (void)fprintf(fpw1,"\t%s\n",argv[i]);
	   i++;
	}
	(void)fprintf(fpw,"output file created:  %s\n",outfile);
	(void)fprintf(fpw,"error file created:  %s\n",errfile);
	(void)fprintf(fpw,"lwx file created:  %s\n",errfile);
	(void)fflush(fpw);
	(void)fprintf(fpw1,"output file created:  %s\n",outfile);
	(void)fprintf(fpw1,"error file created:  %s\n",errfile);
	(void)fprintf(fpw1,"lwx file created:  %s\n",errfile);
	(void)fflush(fpw1);

        /*  Build directory.  */
        index = 1;      /*  Set index for rt_dirbuild.  */
        rtip = rt_dirbuild(argv[index],idbuf,sizeof(idbuf));
        (void)printf("Database Title:  %s\n",idbuf);
        (void)fflush(stdout);

        /*  Set useair to 0 to show no hits of air.  */
        rtip->useair = 0;

        /*  Load desired objects.  */
        index = 2;      /*  Set index.  */
        while(argv[index] != NULL)
        {
                rt_gettree(rtip,argv[index]);
                index += 1;
        }

        /*  Find number of regions.  */
        numreg = (int)rtip->nregions;

        (void)fprintf(stderr,"Number of regions:  %d\n",numreg);
        (void)fflush(stderr);

	/*  Malloc everything now that the number of regions is known.  */
	info = (struct table *)bu_malloc(numreg * sizeof(*info), "info");
	for(i=0; i<numreg; i++)
	{
	   info[i].intrays = (double *)bu_malloc(numreg * sizeof(double), "info[i].intrays");
	   info[i].sf = (double *)bu_malloc(numreg * sizeof(double), "info[i].sf");
	}

	/*  Zero all arrays.  */
	for(i=0; i<numreg; i++)
	{						/*  START # 110  */
	   info[i].name = "\0";
	   info[i].lvrays = 0.;
	   for(j=0; j<numreg; j++)
	   {
		info[i].intrays[j] = 0.;
		info[i].sf[j] = 0.;
	   }
	   info[i].regarea = 0.;
	}						/*  END # 110  */

        /*  Get database ready by starting prep.  */
        rt_prep(rtip);

        /*  Find the center of the bounding rpp of entire model.  */
        centall[X] = rtip->mdl_min[X] +
                (rtip->mdl_max[X] - rtip->mdl_min[X]) / 2.;
        centall[Y] = rtip->mdl_min[Y] +
                (rtip->mdl_max[Y] - rtip->mdl_min[Y]) / 2.;
        centall[Z] = rtip->mdl_min[Z] +
                (rtip->mdl_max[Z] - rtip->mdl_min[Z]) / 2.;

	/*  Find minimum and maximum of entire model.  */
	minall[X] = rtip->mdl_min[X];
	minall[Y] = rtip->mdl_min[Y];
	minall[Z] = rtip->mdl_min[Z];
	maxall[X] = rtip->mdl_max[X];
	maxall[Y] = rtip->mdl_max[Y];
	maxall[Z] = rtip->mdl_max[Z];

	/*  Find radius of bounding sphere.  */
	radall = (maxall[X] - minall[X]) * (maxall[X] - minall[X])
	       + (maxall[Y] - minall[Y]) * (maxall[Y] - minall[Y])
	       + (maxall[Z] - minall[Z]) * (maxall[Z] - minall[Z]);
	/*  Add .5 to make sure completely outside the rpp.  */
	radall = sqrt(radall) / 2. + .5;

	/*  Find surface area of bounding sphere.  */
	areaall = 4 * M_PI * radall * radall;

	/*  Print info on min, max, center, radius, & surface area  */
	/*  of entire model.  */
	(void)printf("Min & max for entire model.\n");
	(void)printf("\tX:  %f - %f\n",minall[X],maxall[X]);
	(void)printf("\tY:  %f - %f\n",minall[Y],maxall[Y]);
	(void)printf("\tZ:  %f - %f\n",minall[Z],maxall[Z]);
	(void)printf("Center:  %f, %f, %f\n\n",centall[X],centall[Y],
		centall[Z]);
	(void)printf("Radius:  %f\n",radall);
	(void)printf("Surface Area:  %f\n\n",areaall);
	(void)fflush(stdout);

	/*  Find number of rays to fire.  */
	(void)printf("Enter the number of rays to be fired.\n\t");
	(void)fflush(stdout);
	(void)scanf("%lf",&rayfir);

	/*  Write info to files.  */
	(void)fprintf(fpw,"Min & max for entire region:\n");
	(void)fprintf(fpw,"\tX:  %f - %f\n",minall[X],maxall[X]);
	(void)fprintf(fpw,"\tY:  %f - %f\n",minall[Y],maxall[Y]);
	(void)fprintf(fpw,"\tZ:  %f - %f\n",minall[Z],maxall[Z]);
	(void)fprintf(fpw,"Center:  %f, %f, %f\n",centall[X],
		centall[Y],centall[Z]);
	(void)fprintf(fpw,"Radius:  %f\n",radall);
	(void)fprintf(fpw,"Surface area:  %f\n",areaall);
	(void)fprintf(fpw,"Number of rays fired:  %f\n\n",rayfir);
	(void)fflush(fpw);

	(void)fprintf(fpw1,"Min & max for entire region:\n");
	(void)fprintf(fpw1,"\tX:  %f - %f\n",minall[X],maxall[X]);
	(void)fprintf(fpw1,"\tY:  %f - %f\n",minall[Y],maxall[Y]);
	(void)fprintf(fpw1,"\tZ:  %f - %f\n",minall[Z],maxall[Z]);
	(void)fprintf(fpw1,"Center:  %f, %f, %f\n",centall[X],
		centall[Y],centall[Z]);
	(void)fprintf(fpw1,"Radius:  %f\n",radall);
	(void)fprintf(fpw1,"Surface area:  %f\n",areaall);
	(void)fprintf(fpw1,"Number of rays fired:  %f\n\n",rayfir);
	(void)fflush(fpw1);

        /*  Put region names into structure.  */
        pr = BU_LIST_FIRST(region, &rtip->HeadRegion);
        for(i=0; i<numreg; i++)
        {
           info[(int)(pr->reg_bit)].name = pr->reg_name;
           pr = BU_LIST_FORW(region, &(pr->l) );
        }

	(void)printf("Region names in structure.\n");
	(void)fflush(stdout);

	/*  Write region names to error file.  */
	for(i=0; i<numreg; i++)
	{
	   (void)fprintf(fpw1,"region %d:  %s\n",(i + 1),info[i].name);
	   (void)fflush(fpw1);
	}
	(void)fprintf(fpw1,"\n");
	(void)fflush(fpw1);

	/*  Set seed for random number generator.  */
	seed = 1;
	(void)printf("Do you wish to enter your own seed (0) or ");
	(void)printf("use the default of 1 (1)?\n\t");
	(void)fflush(stdout);
	(void)scanf("%d",&ians);
	if(ians == 0)
	{
	   (void)printf("Enter unsigned integer seed.\n\t");
	   (void)fflush(stdout);
	   (void)scanf("%ld",&seed);
	}
#ifdef MSRMAXTBL
	   msr = msr_unif_init(seed,0);
#else
#  ifndef HAVE_SRAND48
	   (void)srandom(seed);
#  else
	   (void)srand48(seed);
#  endif
#endif
	(void)printf("Seed initialized\n");
	(void)fflush(stdout);

	/*  Set up parameters for rt_shootray.  */
	RT_APPLICATION_INIT(&ap);
	ap.a_hit = hit;			/*  User supplied hit func.  */
	ap.a_miss = miss;		/*  User supplied miss func.  */
	ap.a_overlap = overlap;		/*  User supplied overlap func.  */
	ap.a_rt_i = rtip;		/*  Pointer from rt_dirbuild.  */
	ap.a_onehit = 0;		/*  Look at all hits.  */
	ap.a_level = 0;			/*  Recursion level for diagnostics.  */
	ap.a_resource = 0;		/*  Address for resource struct.  */
/*
 *	(void)printf("Parameters for rt_shootray set.\n");
 *	(void)fflush(stdout);
 */

	   /*  Loop through for each ray fired.  */
	   for(r=0; r<rayfir; r++)
	   {						/*  START # 150  */
/*
 *		(void)printf("In loop - %f\n",r);
 *		(void)fflush(stdout);
 */

		/*  Find point on the bounding sphere.  The negative  */
		/*  of the unit vector of this point will be the  */
		/*  firing direction.  */
#		ifdef MSRMAXTBL
		   q = MSR_UNIF_DOUBLE(msr) + 0.5;
#		else
		   q = drand48();
#		endif
		theta = q * 2. * M_PI;

#		ifdef MSRMAXTBL
		   q = MSR_UNIF_DOUBLE(msr) + 0.5;
#		else
		   q = drand48();
#		endif
		phi = (q * 2.) - 1.;
		phi = acos(phi);

		rho = radall;

		strtdir[X] = rho * sin(phi) * cos(theta);
		strtdir[Y] = rho * sin(phi) * sin(theta);
		strtdir[Z] = rho * cos(phi);

		/*  Elevation & azimuth for finding a vector in a plane.  */
		elev = M_PI / 2. - phi;
		az = theta;

		/*  Find vector in yz-plane.  */
#		ifdef MSRMAXTBL
		   q = MSR_UNIF_DOUBLE(msr) + 0.5;
#		else
		   q = drand48();
#		endif
		theta = q * 2. * M_PI;

#		ifdef MSRMAXTBL
		   q = MSR_UNIF_DOUBLE(msr) + 0.5;
#		else
		   q = drand48();
#		endif
		rds = rho * sqrt(q);
		s[X] = 0.;
		s[Y] = rds * cos(theta);
		s[Z] = rds * sin(theta);

		/*  Rotate vector.  */
		t[X] = s[X] * cos(elev) * cos(az) - s[Z] * sin(elev) * cos(az)
				- s[Y] * sin(az);
		t[Y] = s[X] * cos(elev) * sin(az) - s[Z] * sin(elev) * sin(az)
				+ s[Y] * cos(az);
		t[Z] = s[X] * sin(elev) + s[Z] * cos(elev);

		/*  Translate the point.  This is the starting point.  */
		strtpt[X] = t[X] + strtdir[X];
		strtpt[Y] = t[Y] + strtdir[Y];
		strtpt[Z] = t[Z] + strtdir[Z];

		/*  Now transfer the starting point so that it is in the  */
		/*  absolute coordinates not the origin's.  */
		strtpt[X] += centall[X];
		strtpt[Y] += centall[Y];
		strtpt[Z] += centall[Z];

		/*  Normalize starting direction & make negative.  */
		denom = strtdir[X] *strtdir[X] +
		        strtdir[Y] *strtdir[Y] +
		        strtdir[Z] *strtdir[Z];
		denom = sqrt(denom);
		strtdir[X] /= (-denom);
		strtdir[Y] /= (-denom);
		strtdir[Z] /= (-denom);

		/*  Set up firing point & direction.  */
		ap.a_ray.r_pt[X] = strtpt[X];
		ap.a_ray.r_pt[Y] = strtpt[Y];
		ap.a_ray.r_pt[Z] = strtpt[Z];
		ap.a_ray.r_dir[X] = strtdir[X];
		ap.a_ray.r_dir[Y] = strtdir[Y];
		ap.a_ray.r_dir[Z] = strtdir[Z];

/*
 *		(void)printf("Calling rt_shootray.\n");
 *		(void)fflush(stdout);
 */

		/*  Call rt_shootray.  */
		(void)rt_shootray(&ap);

/*
 *		(void)printf("Rt_shootray finished.\n");
 *		(void)fflush(stdout);
 */

	   }						/*  END # 150  */

/*
 *	   (void)printf("Finished loop.\n");
 *	   (void)fflush(stdout);
 */

	   for(i=0; i<numreg; i++)
	   {						/*  START # 160  */
	   /*  Write region names to output file.  */
	   (void)fprintf(fpw,"Region %d:  %s\n",(i+1),info[i].name);
	   (void)fflush(fpw);

	   /*  Find shape factors & print.  */
	   if(info[i].lvrays == 0)
	   {						/*  START # 1060  */
		(void)fprintf(fpw1,"**  ERROR - # or rays hitting region ");
		(void)fprintf(fpw1,"%d is 0.  **\n",i);
		(void)fflush(fpw1);
	   }						/*  END # 1060  */

	   else
	   {						/*  START # 1070  */
		/*  Must divide by 2. since looking forwards & backwards.  */
		info[i].regarea = info[i].lvrays / rayfir * areaall / 2.;
		for(j=0; j<numreg; j++)
		{					/*  START # 1080  */
			info[i].sf[j] = info[i].intrays[j] / info[i].lvrays;
			(void)fprintf(fpw,"\t%d   %d   %f\n",
				(i + 1),(j + 1),info[i].sf[j]);
			(void)fflush(fpw);

			(void)fprintf(fpw1,"reg %d - reg %d - rays leave ",
				(i + 1),(j + 1));
			(void)fprintf(fpw1,"& int %f - rays leave %f ",
				info[i].intrays[j],info[i].lvrays);
			(void)fprintf(fpw1,"- sf %f - area %f\n",
				info[i].sf[j],info[i].regarea);
			(void)fflush(fpw1);
		}					/*  END # 1080  */
	   }						/*  END # 1070  */
	   }						/*  END # 160  */

	   /*  Write lwx file.  */
	   (void)fprintf(fpw2,"Longwave Radiation Exchange Factors ");
	   (void)fprintf(fpw2,"for %s\n",argv[1]);
	   (void)fprintf(fpw2,"Number of Regions = %4d\n",numreg);
	   (void)fprintf(fpw2,"TEMIS\n\n");

	   for(i=0; i<numreg; i++)
	   {						/*  START # 1090  */
		(void)fprintf(fpw2,"Region\tArea\tEmissivity\n");
		/*  Area is put into square meters.  */
		(void)fprintf(fpw2,"%d\t%f\n",(i + 1),
			(info[i].regarea / 1000. / 1000.));

		/*  Count the number of shape factors.  */
		k = 0;
		for(j=0; j<numreg; j++)
		{
		   if(info[i].sf[j] != 0.) k++;
		}
		(void)fprintf(fpw2,"Bij\t%d\n",k);

		/*  Print shape factors.  */
		m = 0;
		for(j=0; j<numreg; j++)
		{					/*  START # 1100  */
		   if(info[i].sf[j] != 0.)
		   {					/*  START # 1110  */
			(void)fprintf(fpw2,"%4d   %.4f   ",(j + 1),
				info[i].sf[j]);
			m++;
			if(m == 5)
			{				/*  START # 1120  */
			   m = 0;
			   (void)fprintf(fpw2,"\n");
			}				/*  END # 1120  */
		   }					/*  END # 1110  */
		}					/*  END # 1100  */
		if(m != 0) (void)fprintf(fpw2,"\n");
		(void)fprintf(fpw2," Gnd         Sky\n\n");
		(void)fflush(fpw2);
	   }						/*  END # 1090  */


	/* free memory */
	for(i=0; i<numreg; i++)
	{
	   bu_free(info[i].intrays, "info[i].intrays");
	   bu_free(info[i].sf, "info[i].sf");
	}
	bu_free(info, "info");

	/*  Close files.  */
	(void)fclose(fpw);
	(void)fclose(fpw1);
	(void)fclose(fpw2);
   }							/*  END # 100  */
   return 0;
}							/*  END # 99  */


/***************************************************************************/
/*		Hit, miss, & overlap functions.                            */
/***************************************************************************/
/*  User supplied hit function.  */
int
hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp)
{							/*  START # 1000  */
   register struct partition *pp;
   register struct hit *hitp;
   register struct soltab *stp;

   int rh1;		/*  1st region hit (leaving region).  */
   int rh2;		/*  2nd region hit (entering region).  */
   double lvpt[3];	/*  Leaving point of 1st region.  */
   double entpt[3];	/*  Entering point of 2nd region.  */
   double tol[3];	/*  Used to check tolerance of equal numbers.  */

/*
 * (void)fprintf(fpw1,"In function hit.\n");
 * (void)fflush(fpw1);
 */

   /*  Set regions hit to -1 to show no regions hit yet.  */
   rh1 = (-1);
   rh2 = (-1);

   /*  Set leave & enter points to show no hits yet.  */
   lvpt[X] = (-999.);
   lvpt[Y] = (-999.);
   lvpt[Z] = (-999.);
   entpt[X] = (-999.);
   entpt[Y] = (-999.);
   entpt[Z] = (-999.);

   /*  Set for first region.  */
   pp = PartHeadp->pt_forw;

   for(; pp != PartHeadp; pp=pp->pt_forw)
   {							/*  START # 1010  */
	if( (rh1 == (-1)) && (rh2 == (-1)) )
	{						/*  START # 1020  */
	   /*  First time through; therefore, find the region hit for  */
	   /*  the first region.  */

	   rh1 = (int)pp->pt_regionp->reg_bit;

	   /*  Find the leave point of the first region.  */
	   hitp = pp->pt_outhit;
	   stp = pp->pt_outseg->seg_stp;
	   RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
	   /*  Flip normal if needed.  */
	   if(pp->pt_outflip)
	   {
		VREVERSE(hitp->hit_normal,hitp->hit_normal);
		pp->pt_outflip = 0;
	   }
	   lvpt[X] = hitp->hit_point[X];
	   lvpt[Y] = hitp->hit_point[Y];
	   lvpt[Z] = hitp->hit_point[Z];

	   /*  Increment backwards for hitting ray.  */
	   info[rh1].lvrays += 1.;

	}						/*  END # 1020  */

	else
	{						/*  START # 1030  */
	   /*  Any time but the first time through.  Find the region hit  */
	   /*  for the second region.  */
	   rh2 = (int)pp->pt_regionp->reg_bit;

	   /*  Find the enter point for the second region.  */
	   hitp = pp->pt_inhit;
	   stp = pp->pt_inseg->seg_stp;
	   RT_HIT_NORM(hitp,stp,&(ap_p->a_ray));
	   /*  Flip normal if needed.  */
	   if(pp->pt_inflip)
	   {
		VREVERSE(hitp->hit_normal,hitp->hit_normal);
		pp->pt_inflip = 0;
	   }
	   entpt[X] = hitp->hit_point[X];
	   entpt[Y] = hitp->hit_point[Y];
	   entpt[Z] = hitp->hit_point[Z];

	   /*  Check if the leave & enter point are the same.  */
	   tol[X] = lvpt[X] - entpt[X];
	   tol[Y] = lvpt[Y] - entpt[Y];
	   tol[Z] = lvpt[Z] - entpt[Z];
	   if(tol[X] < 0.) tol[X] = (-tol[X]);
	   if(tol[Y] < 0.) tol[Y] = (-tol[Y]);
	   if(tol[Z] < 0.) tol[Z] = (-tol[Z]);

	   if( (tol[X] < VDIVIDE_TOL) && (tol[Y] < VDIVIDE_TOL) && (tol[Z] < VDIVIDE_TOL) )
	   {
		/*  Nothing happens since the points are the same.  */
	   }

	   else
	   {						/*  START # 1040  */
		/*  The points are not the same; therefore, increment  */
		/*  appropriately.  */
		info[rh1].lvrays += 1.;
		info[rh1].intrays[rh2] += 1.;

		/*  Increment backwards also.  */
		info[rh2].lvrays += 1.;
		info[rh2].intrays[rh1] += 1.;
	   }						/*  END # 1040  */

	   /*  Find info for 1st region.  */
	   rh1 = rh2;
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
	   lvpt[X] = hitp->hit_point[X];
	   lvpt[Y] = hitp->hit_point[Y];
	   lvpt[Z] = hitp->hit_point[Z];

	}						/*  END # 1030  */
   }							/*  END # 1010  */

   /*  Increment the leaving ray since it leaves & doesn't hit anything.  */
   info[rh1].lvrays += 1.;

   return(0);
}							/*  END # 1000  */

/***************************************************************************/
/*  User supplied hit function.  */
int
miss(struct application *ap)
{							/*  START # 2000  */
   return(1);
}							/*  END # 2000  */

/***************************************************************************/
/*  User supplied overlap function.  */
int
overlap(void)
{							/*  START # 3000  */
   return(2);
}							/*  END # 3000  */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
