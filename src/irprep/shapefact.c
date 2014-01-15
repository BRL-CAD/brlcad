/*                     S H A P E F A C T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file shapefact.c
 *
 * Program to find shape factors for the engine by firing random rays.
 * This program fires parallel rays.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Need the following for rt_shootray.  */
#include "vmath.h"
#include "raytrace.h"


#define ADJTOL 1.e-1	/* Tolerance for adjacent regions.  */
#define ZEROTOL 1.e-20	/* Zero tolerance.  */
#define MAXREG 200	/* Maximum number of regions.  */


extern int hit(struct application *ap_p, struct partition *PartHeadp, struct seg *segp);	/* User supplied hit function.  */
extern int miss(struct application *ap);	/* User supplied miss function.  */
extern int overlap(struct application *ap, struct partition *pp, struct region *r1, struct region *r2, struct partition *hp);	/* User supplied overlap function.  */

/* Define structure to hold all information needed.  */
struct table
{
    const char *name;	/* Region name.  */
    int regnum;		/* Region number that matches firpass &  */
			/* secpass.  */
    int numchar;		/* Number of char each region name has.  */
    double lvrays;	/* Number of rays that leave the region through */
			/* air and hit another region.  */
    double intrays[MAXREG];	/* Number of rays that leave region */
    /* through air and are intercepted.  */
    double allvrays;	/* All rays that leave a region through engine */
			/* air.  */
    double engarea;	/* Engine area for each region.  */
};
struct table info[MAXREG];
double nummiss;		/* Number of misses.  */


#ifndef HAVE_DRAND48
/* simulate drand48() -- using 31-bit random() -- assumed to exist */
double drand48() {
    extern long random();
    return (double)random() / 2147483648.0; /* range [0, 1) */
}
#endif


int main(int argc, char **argv)
{
    struct application ap;	/* Structure passed between functions.  */
    struct rt_i *rtip;
    int idx;		/* Index for rt_dirbuild & rt_gettree.  */
    char idbuf[32];	/* Contains database name.  */
    struct region *pr;	/* Used in finding region names.  */
    double rho, phi, theta;/* Spherical coordinates for starting point.  */
    double areabs=0.0;	/* Area of bounding sphere (mm**2).  */
    int ians;		/* Answer of question.  */
    double strtpt[3];	/* Starting point of ray.  */
    double strtdir[3];	/* Starting direction.  */
    size_t loops;	/* Number of rays fired.  */
    size_t r;		/* Variable in loops.  */
    int i, j, k;		/* Variable in loops.  */
    long seed;		/* Initial seed for random number generator.  */
    double denom;	/* Denominator.  */
    double elev;		/* Elevation, used to find point on yz-plane.  */
    double az;		/* Azimuth, used to find point on yz-plane.  */
    double rad;		/* Radius, used to find point on yz-plane.  */
    double s[3], t[3];	/* Temporary variables used to find points.  */
    double q;		/* Temporary variable used to find points.  */
    int numreg;		/* Number of regions.  */
    double center[3];	/* Center of the bounding rpp.  */
    double sf;		/* Used to print shape factor.  */
    double dump;		/* How often a dump is to occur.  */
    int idump;		/* 1=>dump to occur.  */

    FILE *fp;		/* Used to open files.  */
    char outfile[16];	/* Output file.  */
    FILE *fp1;		/* Used to read region # & name file.  */
    char rnnfile[16];	/* Region # & name file.  */
    FILE *fp2;		/* Used to write the error file.  */
    char errfile[16];	/* Error file.  */
    double totalsf;	/* Sum of shape factors.  */
    double totalnh;	/* Sum of number of hits.  */
    int itype;		/* Type of file to be created, 0=>regular,  */
			/* 1=>generic.  */
    char line[500];	/* Buffer to read a line of data into.  */
    int c;		/* Reads one character of information.  */
    int icnt;		/* Counter for shape factor.  */
    char tmpname[150];	/* Temporary name.  */
    int tmpreg;		/* Temporary region number.  */
    char rnnname[800][150];  /* Region name from region # & name file.  */
    int rnnnum;		/* Number of regions in region # & name file.  */
    int rnnchar[800];	/* Number of characters in name.  */
    int rnnreg[800];	/* Region number from region # & name file.  */
    int jcnt;		/* Counter.  */
    int equal;		/* 0=>equal, 1=>not equal.  */
    double rcpi, rcpj;	/* Used to check reciprocity.  */
    double rcp_diff;	/* Difference in reciprocity.  */
    double rcp_pdiff;	/* Percent difference in reciprocity.  */
    int ret;

    struct bn_unif *msr = NULL;

    /* Check to see if arguments are implemented correctly.  */
    if ((argc < 3 || argv[1] == NULL) || (argv[2] == NULL)) {
	fprintf(stderr, "\nUsage:  %s file.g objects\n\n", *argv);
    } else {
	/* START # 1 */

	/* Ask what type of file is to be created - regular */
	/* or generic.  */
	printf("Enter type of file to be written (0=>regular or ");
	printf("1=>generic).  ");
	(void)fflush(stdout);
	ret = scanf("%1d", &itype);
	if (ret == 0)
	    bu_exit(-1, "scanf failure when reading file type");
	if (itype != 1) itype = 0;

	/* Enter names of files to be used.  */
	fprintf(stderr, "Enter name of output file (15 char max).\n\t");
	(void)fflush(stderr);
	ret = scanf("%15s", outfile);
	if (ret == 0)
	    bu_exit(-1, "scanf failure when reading output file name");

	/* Read name of the error file to be written.  */
	printf("Enter the name of the error file (15 char max).\n\t");
	(void)fflush(stdout);
	ret = scanf("%15s", errfile);
	if (ret == 0)
	    bu_exit(-1, "scanf failure when reading error file name");

	{
	    /* Enter name of region # & name file to be read.  */
	    printf("Enter region # & name file to be read ");
	    printf("(15 char max).\n\t");
	    (void)fflush(stdout);
	    ret = scanf("%15s", rnnfile);
	    if (ret == 0)
		bu_exit(-1, "scanf failure when reading region # + name file");
	}

	/* Check if dump is to occur.  */
	idump = 0;
	printf("Do you want to dump intermediate shape factors to ");
	printf("screen (0-no, 1-yes)?  ");
	(void)fflush(stdout);
	ret = scanf("%1d", &idump);
	if (ret == 0)
	    bu_exit(-1, "scanf failure - intermediate shape factors setting");

	/* Find number of rays to be fired.  */
	fprintf(stderr, "Enter number of rays to be fired.  ");
	(void)fflush(stderr);
	ret = scanf("%llu", (unsigned long long *)&loops);
	if (ret == 0)
	    bu_exit(-1, "scanf failure - number of rays to be fired");

	/* clamp loops */
	if (loops > UINT32_MAX)
	    loops = UINT32_MAX;

	/* Set seed for random number generator.  */
	seed = 1;
	fprintf(stderr, "Do you wish to enter your own seed (0) or ");
	fprintf(stderr, "use the default of 1 (1)?  ");
	(void)fflush(stderr);
	ret = scanf("%1d", &ians);
	if (ret == 0)
	    bu_exit(-1, "scanf failure - seed use setting");
	if (ians == 0) {
	    fprintf(stderr, "Enter unsigned integer seed.  ");
	    (void)fflush(stderr);
	    ret = scanf("%ld", &seed);
	    if (ret == 0)
		bu_exit(-1, "scanf failure - seed");
	}
	msr = bn_unif_init(seed, 0);
	bu_log("seed initialized\n");

	/* Read region # & name file.  */
	rnnnum = 0;
	fp1 = fopen(rnnfile, "rb");
	c = getc(fp1);
	while (c != EOF) {
	    (void)ungetc(c, fp1);
	    (void)bu_fgets(line, 200, fp1);
	    sscanf(line, "%d%149s", &tmpreg, tmpname);
	    for (i=0; i<150; i++) {
		rnnname[rnnnum][i] = tmpname[i];
	    }
	    rnnreg[rnnnum] = tmpreg;
	    rnnnum++;
	    c = getc(fp1);
	}
	(void)fclose(fp1);

	printf("Number of regions read from region # & name file:  %d\n",		rnnnum);
	(void)fflush(stdout);

	/* Find number of characters in each region name.  */
	for (i=0; i<rnnnum; i++) {
	    jcnt = 0;
	    while (rnnname[i][jcnt] != '\0') {
		jcnt++;
	    }
	    rnnchar[i] = jcnt;
	}

	/* Build directory.  */
	idx = 1;	/* Set index for rt_dirbuild.  */
	rtip = rt_dirbuild(argv[idx], idbuf, sizeof(idbuf));
	printf("Database Title:  %s\n", idbuf);
	(void)fflush(stdout);

	/* Set useair to 1 to show hits of air.  */
	rtip->useair = 1;

	/* Load desired objects.  */
	idx = 2;	/* Set index.  */
	while (argv[idx] != NULL) {
	    rt_gettree(rtip, argv[idx]);
	    idx += 1;
	}

	/* Find number of regions.  */
	numreg = (int)rtip->nregions;

	fprintf(stderr, "Number of regions:  %d\n", numreg);
	(void)fflush(stderr);

	/* Zero all arrays.  */
	for (i=0; i<numreg; i++) {
	    info[i].name = "\0";
	    info[i].regnum = (-1);
	    info[i].numchar = 0;
	    info[i].lvrays = 0.0;
	    info[i].engarea = 0.0;
	    for (j=0; j<numreg; j++) {
		info[i].intrays[j] = 0.0;
	    }
	}
	nummiss = 0.0;

	/* Get database ready by starting prep.  */
	rt_prep(rtip);

	/* Find the center of the bounding rpp.  */
	center[X] = rtip->mdl_min[X] +
	    (rtip->mdl_max[X] - rtip->mdl_min[X]) / 2.0;
	center[Y] = rtip->mdl_min[Y] +
	    (rtip->mdl_max[Y] - rtip->mdl_min[Y]) / 2.0;
	center[Z] = rtip->mdl_min[Z] +
	    (rtip->mdl_max[Z] - rtip->mdl_min[Z]) / 2.0;

	/* Put region names into structure.  */
	pr = BU_LIST_FIRST(region, &rtip->HeadRegion);
	for (i=0; i<numreg; i++) {
	    info[(int)(pr->reg_bit)].name = pr->reg_name;
	    pr = BU_LIST_FORW(region, &(pr->l));
	}

	/* Set up parameters for rt_shootray.  */
	RT_APPLICATION_INIT(&ap);
	ap.a_hit = hit;		/* User supplied hit function.  */
	ap.a_miss = miss;	/* User supplied miss function.  */
	ap.a_overlap = overlap;	/* User supplied overlap function.  */
	ap.a_rt_i = rtip;	/* Pointer from rt_dirbuild.  */
	ap.a_onehit = 0;	/* Look at all hits.  */
	ap.a_level = 0;		/* Recursion level for diagnostics.  */
	ap.a_resource = 0;	/* Address for resource structure.  */

	dump = 1000000.0;	/* Used for dumping info.  */

	for (r=0; r<loops; r++) {
	    /* Number of rays fired.  */
	    /* START # 2 */

	    /* Find length of 'diagonal' (rho).  (In reality rho is */
	    /* the radius of bounding sphere).  */
	    rho = (rtip->mdl_max[X] - rtip->mdl_min[X])
		* (rtip->mdl_max[X] - rtip->mdl_min[X])
		+(rtip->mdl_max[Y] - rtip->mdl_min[Y])
		* (rtip->mdl_max[Y] - rtip->mdl_min[Y])
		+(rtip->mdl_max[Z] - rtip->mdl_min[Z])
		* (rtip->mdl_max[Z] - rtip->mdl_min[Z]);
	    rho = sqrt(rho) / 2.0 + .5;

	    /* find surface area of bounding sphere.  */
	    areabs = 4. * M_PI * rho * rho;

	    /* Second way to find starting point and direction.  */
	    /* This approach finds the starting point and direction */
	    /* by using parallel rays.  */

	    /* Find point on the bounding sphere.  (The negative */
	    /* of the unit vector of this point will eventually be */
	    /* the firing direction.  */
	    q = BN_UNIF_DOUBLE(msr) + 0.5;
	    theta = q * 2. * M_PI;
	    q = BN_UNIF_DOUBLE(msr) + 0.5;
	    phi = (q * 2.0) - 1.0;
	    phi = acos(phi);
	    strtdir[X] = rho * sin(phi) * cos(theta);
	    strtdir[Y] = rho * sin(phi) * sin(theta);
	    strtdir[Z] = rho * cos(phi);

	    /* Elevation and azimuth for finding a vector in a plane.  */
	    elev = M_PI_2 - phi;
	    az = theta;

	    /* Find vector in yz-plane.  */

	    q = BN_UNIF_DOUBLE(msr) + 0.5;
	    theta = q * 2. * M_PI;
	    q = BN_UNIF_DOUBLE(msr) + 0.5;
	    rad = rho * sqrt(q);
	    s[X] = 0.0;
	    s[Y] = rad * cos(theta);
	    s[Z] = rad * sin(theta);

	    /* Rotate vector.  */
	    t[X] = s[X] * cos(elev) * cos(az) - s[Z] * sin(elev) * cos(az)
		- s[Y] * sin(az);
	    t[Y] = s[X] * cos(elev) * sin(az) - s[Z] * sin(elev) * sin(az)
		+ s[Y] * cos(az);
	    t[Z] = s[X] * sin(elev) + s[Z] * cos(elev);

	    /* Translate the point.  This is starting point.  */
	    strtpt[X] = t[X] + strtdir[X];
	    strtpt[Y] = t[Y] + strtdir[Y];
	    strtpt[Z] = t[Z] + strtdir[Z];

	    /* Now transfer starting point so that it is in */
	    /* the absolute coordinates not the origin's.  */
	    strtpt[X] += center[X];
	    strtpt[Y] += center[Y];
	    strtpt[Z] += center[Z];

	    /* Normalize starting direction and make negative.  */
	    denom = strtdir[X] * strtdir[X] +
		strtdir[Y] * strtdir[Y] +
		strtdir[Z] * strtdir[Z];
	    denom = sqrt(denom);
	    strtdir[X] /= (-denom);
	    strtdir[Y] /= (-denom);
	    strtdir[Z] /= (-denom);

	    /* Set up firing point and direction.  */
	    ap.a_ray.r_pt[X] = strtpt[X];
	    ap.a_ray.r_pt[Y] = strtpt[Y];
	    ap.a_ray.r_pt[Z] = strtpt[Z];
	    ap.a_ray.r_dir[X] = strtdir[X];
	    ap.a_ray.r_dir[Y] = strtdir[Y];
	    ap.a_ray.r_dir[Z] = strtdir[Z];

	    /* Call rt_shootray for "forward ray".  */
	    (void)rt_shootray(&ap);

	    if (EQUAL(r, (dump - 1.0))) {
		printf("%llu rays have been fired in forward direction.\n",
		       (unsigned long long)(r+1));
		(void)fflush(stdout);
		if (idump == 1) {
		    /* START # 3 */
		    printf("\n****************************************");
		    printf("****************************************\n");
		    (void)fflush(stdout);
		    /* Dump info to file.  */
		    for (i=0; i<numreg; i++) {
			for (j=0; j<numreg; j++) {
			    sf = 0.0;
			    if ((info[i].lvrays < -ZEROTOL) || (ZEROTOL <
								info[i].lvrays))
				sf = info[i].intrays[j] / info[i].lvrays;

			    printf("%d\t%d\t%f\n", i, j, sf);
			    (void)fflush(stdout);
			}
		    }
		}						/* START # 3 */

		dump = dump + 1000000.0;
	    }

	}					/* END # 2 */

	/* Find area bounded by engine air using Monte Carlo method.  */
	for (i=0; i<numreg; i++) {
	    /* Old way, only incrementing info[i].allvrays for forward */
	    /* ray therefore do not divide by 2.  Division by 2 is to */
	    /* include the backwards ray also.  */
	    /*
	     * info[i].engarea = info[i].allvrays * areabs / loops / 2.0;
	     */
	    info[i].engarea = info[i].allvrays * areabs / (double)loops;

	    /* Put area into square meters.  */
	    info[i].engarea *= 1.e-6;
	}

	/* Find number of characters in each region name.  */
	for (i=0; i<numreg; i++) {
	    jcnt = 0;
	    while (info[i].name[jcnt] != '\0') {
		jcnt++;
	    }
	    info[i].numchar = jcnt;
	}

	/* Find correct region number.  */
	printf("Finding correct region numbers.\n");
	(void)fflush(stdout);
	for (i=0; i<numreg; i++) {
	    for (j=0; j<rnnnum; j++) {
		equal = 0;	/* 1=>not equal.  */
		jcnt = rnnchar[j];
		for (k=info[i].numchar; k>=0; k--) {
		    if (jcnt<0) equal = 1;
		    else if (info[i].name[k] != rnnname[j][jcnt])
			equal = 1;
		    jcnt--;
		}
		if (equal == 0) info[i].regnum = rnnreg[j];
	    }
	}
	printf("Finished finding correct region numbers.\n");
	(void)fflush(stdout);

	/******************************************************************/

	/* Check for reciprocity.  */
	/* Open error file.  */
	fp2 = fopen(errfile, "wb");
	fprintf(fp2, "\nError file for shapefact.\n");
	fprintf(fp2, "Shape factor file created:  %s\n\n", outfile);
	fprintf(fp2, "Regions with reciprocity errors greater ");
	fprintf(fp2, "than 10%%.\n\n");
	(void)fflush(fp2);

	for (i=0; i<numreg; i++) {
	    for (j=0; j<numreg; j++) {
		rcpi = 0.0;
		rcpj = 0.0;
		if ((info[i].lvrays < -ZEROTOL) || (ZEROTOL < info[i].lvrays))
		    rcpi = info[i].intrays[j] * info[i].engarea /info[i].lvrays;
		if ((info[j].lvrays < -ZEROTOL) || (ZEROTOL < info[j].lvrays))
		    rcpj = info[j].intrays[i] * info[j].engarea /info[j].lvrays;
		rcp_diff = rcpi - rcpj;
		if (rcp_diff < 0.0) rcp_diff = (-rcp_diff);
		if ((rcpi < -ZEROTOL) || (ZEROTOL < rcpi))
		    rcp_pdiff = rcp_diff / rcpi;
		else rcp_pdiff = 0.0;	/* Don't divide by 0.  */
		/* Print reciprocity errors greater than 10%.  */
		if (rcp_pdiff > 0.1) {
		    fprintf(fp2, "%d   %d   %f   %f   %f   %f\n",
			    info[i].regnum, info[j].regnum, rcpi, rcpj, rcp_diff,
			    (rcp_pdiff * 100.0));
		    (void)fflush(fp2);
		}
	    }
	}
	/* Close error file.  */
	(void)fclose(fp2);

	/******************************************************************/

	/* Print out shape factor to regular output file.  */
	if (itype == 0) {
	    fp = fopen(outfile, "wb");
	    fprintf(fp, "Number of forward rays fired:  %llu\n\n", (unsigned long long)loops);
	    (void)fflush(fp);

	    /* Print out structure.  */
	    for (i=0; i<numreg; i++) {
		/* Print region number, region name, & engine area.  */
		fprintf(fp, "%d\t%s\t%e\n",
			info[i].regnum, info[i].name, info[i].engarea);
		(void)fflush(fp);

		/* Zero sums for shape factors & rays hit.  */
		totalsf = 0.0;
		totalnh = 0.0;

		for (j=0; j<numreg; j++) {
		    sf = 0.0;
		    if ((info[i].lvrays < -ZEROTOL) || (ZEROTOL <
							info[i].lvrays))
			sf = info[i].intrays[j] / info[i].lvrays;

		    /* Print region number & shape factor.  */
		    fprintf(fp, "   %d\t%e\n",
			    info[j].regnum, sf);
		    (void)fflush(fp);

		    /* Add to sum of shape factors & number of hits.  */
		    totalsf += sf;
		    totalnh += info[i].intrays[j];

		}

		/* Print sum of hits & sum of shape factors.  */
		fprintf(fp, " sum of hits:  %f\n", totalnh);
		fprintf(fp, " sum of shape factors:  %f\n\n", totalsf);
		(void)fflush(fp);
	    }
	    (void)fclose(fp);
	}

	/******************************************************************/

	/* Create and write to generic shape factor file.  */
	if (itype == 1) {
	    fp = fopen(outfile, "wb");
	    for (i=0; i<numreg; i++) {
		/* Count the number of shape factors.  */
		icnt = 0;
		for (j=0; j<numreg; j++) {
		    if (info[i].intrays[j] > ZEROTOL) icnt++;
		}
		/* Print the # 5, region number (matches firpass &  */
		/* secpass), engine area, & number of shape factors.  */
		fprintf(fp, " 5  %d  %e  %d\n",
			info[i].regnum, info[i].engarea, icnt);
		(void)fflush(fp);
		for (j=0; j<numreg; j++) {
		    if (info[i].intrays[j] > ZEROTOL) {
			sf = info[i].intrays[j] / info[i].lvrays;
			/* Print each region # & shape factor.  */
			fprintf(fp, "    %d  %e\n", info[j].regnum, sf);
			(void)fflush(fp);
		    }
		}
	    }
	    (void)fclose(fp);
	}

    }						/* END # 1 */
    return 0;
}


/*****************************************************************************/
/*		Hit, miss, and overlap functions.                            */
/*****************************************************************************/
/* User supplied hit function.  */
int
hit(struct application *UNUSED(ap_p), struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    /* START # 0H */
    struct partition *pp;
    struct hit *hitp;
    struct soltab *stp;
    int icur=0;			/* Current region hit.  */
    int iprev;			/* Previous region hit.  */
    int iair;			/* Type of air or region came from,  */
				/* 0=>region, 1=>exterior air, 2=>crew */
				/* air, 5=>engine air, 6=>closed */
				/* compartment air, 7=>exhaust air,  */
				/* 8=>generic air 1, 9=>generic air 2.  */

    /*
     * printf("In hit function.\n");
     * (void)fflush(stdout);
     */

    /* Set beginning parameters.  */
    iprev = -1;
    iair = 1;		/* Comes from exterior air.  */

    /*
     * printf("Beginning loop again.\n");
     * (void)fflush(stdout);
     */

    pp = PartHeadp->pt_forw;
    for (; pp != PartHeadp;  pp = pp->pt_forw) {
	/* START # 1H */

	if (iair == 1) {
	    /* Ray comes from nothing (exterior air).  */
	    /* START # 2H */

	    if (pp->pt_regionp->reg_regionid > (short)0) {
		/* Hit region.  */
		/* START # 3H */
		/* Region number hit.  */
		icur = (int)(pp->pt_regionp->reg_bit);

		/* Find leaving point.  */
		hitp = pp->pt_outhit;
		stp = pp->pt_outseg->seg_stp;
		RT_HIT_NORMAL(hitp->hit_normal, hitp, stp, &(ap_p->a_ray), pp->pt_outflip);

		iprev = icur;
		iair = 0;	/* A region was just hit.  */
		/* END # 3H */
	    } else {
		/* Hit air.  */
		/* START # 4H */
		iair = pp->pt_regionp->reg_aircode;
	    }					/* END # 4H */
	    /* END # 2H */
	} else if (iair == 5) {
	    /* Ray comes from engine air.  */
	    /* START # 5H */

	    if (pp->pt_regionp->reg_regionid > (short)0) {
		/* Hit region.  */
		/* START # 6H */
		/* Region number hit.  */
		icur = (int)(pp->pt_regionp->reg_bit);

		/* Only execute the following two statements if iprev >= 0.  */
		if (iprev < (-1)) {
		    fprintf(stderr, "ERROR -- iprev = %d\n", iprev);
		    (void)fflush(stderr);
		}
		if (iprev == (-1)) {
		    fprintf(stderr, "iprev = %d - entered ", iprev);
		    fprintf(stderr, "through engine air\n");
		    (void)fflush(stderr);
		}
		if (iprev > (-1)) {
		    /* Add one to number of rays leaving previous region.  */
		    info[iprev].lvrays++;

		    /* Add one to the number of rays leaving current region */
		    /* for the backward ray.  */
		    info[icur].lvrays++;

		    /* Add one to number of rays leaving previous region and */
		    /* intercepted by current region.  */
		    info[iprev].intrays[icur]++;

		    /* Add one to the number of rays leaving the current */
		    /* region and intersecting the previous region for the */
		    /* backward ray.  */
		    info[icur].intrays[iprev]++;

		}

		/* Find leave point.  */
		hitp = pp->pt_outhit;
		stp = pp->pt_outseg->seg_stp;
		RT_HIT_NORMAL(hitp->hit_normal, hitp, stp, &(ap_p->a_ray), pp->pt_outflip);

		iprev = icur;
		iair = 0;	/* Hit a region.  */
		/* END # 6H */
	    } else {
		/* Hit air.  */
		iair = 5;	/* Since coming through engine air */
		/* assume should still be engine air.  */
	    }
	    /* END # 5H */
	} else if (iair == 0) {
	    /* Ray comes from a region.  */
	    /* START # 7H */

	    if (pp->pt_regionp->reg_regionid > (short)0) {
		/* Hit a region.  */
		/* START # 8H */
		/* Region number hit.  */
		icur = (int)(pp->pt_regionp->reg_bit);

		/* Find leaving point.  */
		hitp = pp->pt_outhit;
		stp = pp->pt_outseg->seg_stp;
		RT_HIT_NORMAL(hitp->hit_normal, hitp, stp, &(ap_p->a_ray), pp->pt_outflip);

		iprev = icur;
		iair = 0;	/* Hit a region.  */
		/* END # 8H */
	    } else {
		/* Hit air.  */
		/* START # 9H */
		/* Increment allvrays if the ray is leaving through */
		/* engine air.  Make sure this is only done once.  */
		if ((iair != 5) && (pp->pt_regionp->reg_aircode == 5))
		    info[icur].allvrays++;

		if (iair != 5) iair = pp->pt_regionp->reg_aircode;
	    }					/* END # 9H */
	    /* END # 7H */
	} else {
	    /* Ray comes from any interior air.  */
	    /* START # 10H */

	    if (pp->pt_regionp->reg_regionid > (short)0) {
		/* Hits region.  */
		/* START # 11H */
		/* Region number hit.  */
		icur = (int)(pp->pt_regionp->reg_bit);

		/* Find leaving point.  */
		hitp = pp->pt_outhit;
		stp = pp->pt_outseg->seg_stp;
		RT_HIT_NORMAL(hitp->hit_normal, hitp, stp, &(ap_p->a_ray), pp->pt_outflip);

		iprev = icur;
		iair = 0;	/* Hit region.  */
		/* END # 11H */
	    } else {
		/* Hits air.  */
		iair = pp->pt_regionp->reg_aircode;
	    }
	}					/* END # 10H */
    }						/* END # 1H */

    if (iprev == (-1)) {
	/* Went through air only.  */
	return 1;		/* Indicates miss.  */
    } else {
	return 0;
    }
}						/* END # 0H */

/* User supplied miss function.  */
int
miss(struct application *UNUSED(ap))
{
    nummiss = nummiss + 1.0;

    return 1;
}


/* User supplied overlap function.  */
int
overlap(struct application *UNUSED(ap), struct partition *UNUSED(pp), struct region *UNUSED(r1), struct region *UNUSED(r2), struct partition *UNUSED(hp))
{
    return 2;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
