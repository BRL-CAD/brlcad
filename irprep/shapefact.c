/*
 *			S H A P E F A C T . C
 *
 *  Program to find shape factors for the engine by firing random rays.
 *  This program fires parallel rays.
 *
 *  Author -
 *	S.Coates - 8 July 1991
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

/*	23 May 1991 - Take out lvpt that wasn't necessary.  */
/*		Put in optional dump.  Sum shape factors.  */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>

/*  Need the following for rt_shootray.  */
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "raytrace.h"

#define PI 3.14159265358979323846262	/*  Pi.  */
#define ADJTOL 1.e-1	/*  Tolerance for adjacent regions.  */
#define ZEROTOL 1.e-20	/*  Zero tolerance.  */
#define MAXREG 70	/*  Maximum number of regions.  */

struct application ap;	/*  Structure passed between functions.  */

extern int hit();	/*  User supplied hit function.  */
extern int miss();	/*  User supplied miss function.  */
extern int overlap();	/*  User supplied overlap function.  */

/*  Define structure to hold all information needed.  */
struct table
{
   CONST char *name;		/*  Region name.  */
   double lvrays;	/*  Number of rays that leave the region through  */
			/*  air.  */
   double intrays[MAXREG];	/*  Number of rays that leave region  */
			/*  through air and are intercepted.  */
};
struct table info[MAXREG];
double nummiss;		/*  Number of misses.  */

int main(argc,argv)

int argc;
char **argv;

{
   extern struct table info[];	/*  Structure is external.  */
   struct rt_i *rtip;
   int index;		/*  Index for rt_dirbuild & rt_gettree.  */
   char idbuf[32];	/*  Contains database name.  */
   struct region *pr;	/*  Used in finding region names.  */
   double rho,phi,theta;/*  Spherical coordinates for starting point.  */
   int ians;		/*  Answer of question.  */
   double strtpt[3];	/*  Starting point of ray.  */
   double strtdir[3];	/*  Starting direction.  */
   double loops;	/*  Number of rays fired.  */
   double r;		/*  Variable in loops.  */
   int i,j;		/*  Variable in loops.  */
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
   double totalsf;	/*  Sum of shape factors.  */
   double totalnh;	/*  Sum of number of hits.  */

   /*  Check to see if arguments are implimented correctly.  */
   if( (argv[1] == NULL) || (argv[2] == NULL) )
   {
	(void)fprintf(stderr,"\nusage:  rand.ray file.g objects\n\n");
   }
   else
   {						/*  START # 1  */

	/*  Enter names of files to be used.  */
	(void)fprintf(stderr,"Enter name of output file (15 char max).  ");
	(void)fflush(stderr);
	(void)scanf("%s",outfile);

	/*  Check if dump is to occur.  */
	idump = 0;
	(void)printf("Do you want to dump interm shape factors to ");
	(void)printf("screen (0-no, 1-yes)?  ");
	(void)fflush(stdout);
	(void)scanf("%d",&idump);

	/*  Find number of rays to be fired.  */
	(void)fprintf(stderr,"Enter number of rays to be fired.  ");
	(void)fflush(stderr);
	(void)scanf("%lf",&loops);

	/*  Set seed for random number generator.  */
	seed = 1;
	(void)fprintf(stderr,"Do you wish to enter your own seed (0) or\n");
	(void)fprintf(stderr,"use the default of 1 (1)?  ");
	(void)fflush(stderr);
	(void)scanf("%d",&ians);
	if(ians == 0)
	{
		(void)fprintf(stderr,"Enter unsigned integer seed.  ");
		(void)fflush(stderr);
		(void)scanf("%ld",&seed);
	}
	(void) srand48(seed);

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
		info[i].lvrays = 0.;
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
	pr = rtip->HeadRegion;
	for(i=0; i<numreg; i++)
	{
	   info[(int)(pr->reg_bit)].name = pr->reg_name;
	   pr = pr->reg_forw;
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
	   /*  Find length of 'diagonal' (rho).  (In reality rho is  */
	   /*  the radius of bounding sphere).  */
	   rho = (rtip->mdl_max[X] - rtip->mdl_min[X])
	   	   * (rtip->mdl_max[X] - rtip->mdl_min[X])
	   	+(rtip->mdl_max[Y] - rtip->mdl_min[Y])
	   	   * (rtip->mdl_max[Y] - rtip->mdl_min[Y])
	   	+(rtip->mdl_max[Z] - rtip->mdl_min[Z])
	   	   * (rtip->mdl_max[Z] - rtip->mdl_min[Z]);
	   rho = sqrt(rho) / 2. + .5;

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
	   q = drand48();
	   theta = q * 2. * PI;
/*
 *	   (void)printf("random number:  %f, theta:  %f\n",q,theta);
 *	   (void)fflush(stdout);
 */
	   q = drand48();
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
	   q = drand48();
	   theta = q * 2. * PI;
/*
 *	   (void)printf("random number:  %f, theta:  %f\n",q,theta);
 *	   (void)fflush(stdout);
 */
	   q = drand48();
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

	   /*  Call rt_shootray.  */
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
	     (void)printf("%f rays have been fired.\n",(r+1));
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

	/*  Print out shape factor to output file.  */
	fp = fopen(outfile,"w");
	(void)fprintf(fp,"Number of rays fired:  %f\n",loops);
	(void)fprintf(fp,"Number of misses:  %f\n\n",nummiss);
	(void)fflush(fp);

	/*  Print out structure.  */
	for(i=0; i<numreg; i++)
	{
	   (void)fprintf(fp,"\n%d\t%s\n",i,info[i].name);
	   (void)fprintf(fp,"  total number of rays:  %f\n",
		info[i].lvrays);
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

		(void)fprintf(fp,"    region %d, number of hits:  %f",
			j,info[i].intrays[j]);
		(void)fprintf(fp,", shape factor:  %f\n",sf);
		(void)fflush(fp);

		/*  Add to sum of shape factors & number of hits.  */
		totalsf += sf;
		totalnh += info[i].intrays[j];

	   }

	   (void)fprintf(fp,"  sum of hits:  %f\n",totalnh);
	   (void)fprintf(fp,"  sum of shape factors:  %f\n",totalsf);
	   (void)fflush(fp);
	}
	(void)fclose(fp);

   }						/*  END # 1  */
   return(0);
}


/*****************************************************************************/
/*		Hit, miss, and overlap functions.                            */
/*****************************************************************************/

hit(ap,PartHeadp)
/*  User supplied hit function.  */
register struct application *ap;
struct partition *PartHeadp;

{						/*  START # 0H  */
   extern struct table info[];	/*  Structure is external.  */
   register struct partition *pp;
   register struct hit *hitp;
   register struct soltab *stp;
   int icur;			/*  Current region hit.  */
   int iprev;			/*  Previous region hit.  */
   int iair;			/*  Type of air or region came from,  */
				/*  0=>region, 1=>exterior air, 2=>crew  */
				/*  air, 5=>engine air.  */

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
		RT_HIT_NORM(hitp,stp,&(ap->a_ray));
		/*  Flip normal if needed.  */
		if(pp->pt_outflip)
		{
			VREVERSE(hitp->hit_normal,hitp->hit_normal)
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

		/*  Add one to number of rays leaving previous region and  */
		/*  intercepted by current region.  */
		info[iprev].intrays[icur]++;
		}

		/*  Find leave point.  */
		hitp = pp->pt_outhit;
		stp = pp->pt_outseg->seg_stp;
		RT_HIT_NORM(hitp,stp,&(ap->a_ray));
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
		RT_HIT_NORM(hitp,stp,&(ap->a_ray));
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
		RT_HIT_NORM(hitp,stp,&(ap->a_ray));
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

   return(0);
}						/*  END # 0H  */


miss()
/*  User supplied miss function.  */
{

/*
 * (void)printf("In miss function.\n");
 * (void)fflush(stdout);
 */

   nummiss = nummiss + 1.;

   return(1);
}


overlap()
/*  User supplied overlap function.  */
{

/*
 * (void)printf("In overlap function.\n");
 * (void)fflush(stdout);
 */

   return(1);
}
