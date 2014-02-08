/*                    V I E W W E I G H T . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2014 United States Government as represented by
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
/** @file rt/viewweight.c
 *
 *  Ray Tracing program RTWEIGHT bottom half.
 *
 *  This module outputs the weights and moments of a target model
 *  using density values located in ".density" or "$HOME/.density"
 *  Output is given in metric and english units, although input is
 *  assumed in lbs/cu.in.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "vmath.h"
#include "raytrace.h"

#include "db.h"  /* FIXME: Yes, I know I shouldn't be peeking, put I
		    am only looking to see what units we prefer... */

#include "./rtuif.h"
#include "./ext.h"


extern struct resource resource[];

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

const char title[] = "RT Weight";

void
usage(const char *argv0)
{
    bu_log("Usage: %s [options] model.g objects...\n", argv0);
    bu_log("Options:\n");
    bu_log(" -s #		Grid size in pixels, default 512\n");
    bu_log(" -g #		Grid cell width [and height] in mm\n");
    bu_log(" -G #		Grid cell height [and width] in mm\n");
    bu_log(" -a Az		Azimuth in degrees\n");
    bu_log(" -e Elev	Elevation in degrees\n");
    bu_log(" -o file.out	Weights and Moments output file\n");
    bu_log(" -M		Read matrix, cmds on stdin\n");
    bu_log(" -r 		Report verbosely mass of each region\n");
    bu_log(" -x #		Set librt debug flags\n");
    bu_log("Files:\n");
    bu_log(" .density, OR\n");
    bu_log(" $HOME/.density\n");
}


int	noverlaps = 0;
FILE	*densityfp;
char	*densityfile;
#define	DENSITY_FILE	".density"

/* FIXME: use a bu_avs instead of a hard-coded limit so that materials
 * are looked up by the key in the density file, not assuming it's an
 * integer index into a fixed-size array.  gqa does it better by using
 * dynamic memory.
 */
#define MAXMATLS	32768
fastf_t	density[MAXMATLS];
char	*dens_name[MAXMATLS];

struct datapoint {
    struct datapoint *next;
    vect_t	centroid;
    fastf_t	weight;
    fastf_t	volume;
};

extern int     	rpt_overlap;     	/* report region verbosely */
extern fastf_t  cell_width;      	/* model space grid cell width */
extern fastf_t  cell_height;     	/* model space grid cell height */
extern FILE     *outfp;          	/* optional output file */
extern char	*outputfile;     	/* name of base of output file */
extern int	output_is_binary;	/* !0 means output is binary */


int
hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    struct partition *pp;
    register struct xray *rp = &ap->a_ray;
    genptr_t addp;
    int part_count = 0;

    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	register struct region	*reg = pp->pt_regionp;
	register struct hit	*ihitp = pp->pt_inhit;
	register struct hit	*ohitp = pp->pt_outhit;
	register fastf_t	depth;
	register struct datapoint *dp;

	if (reg->reg_aircode)
	    continue;

	/* fill in hit points and hit distances */
	VJOIN1(ihitp->hit_point, rp->r_pt, ihitp->hit_dist, rp->r_dir);
	VJOIN1(ohitp->hit_point, rp->r_pt, ohitp->hit_dist, rp->r_dir);
	depth = ohitp->hit_dist - ihitp->hit_dist;

	part_count++;
	/* add the datapoint structure in and then calculate it
	   in parallel, the region structures are a shared resource */
	BU_ALLOC(dp, struct datapoint);
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	addp = reg->reg_udata;
	reg->reg_udata = (genptr_t)dp;
	dp->next = (struct datapoint *)addp;
	bu_semaphore_release(BU_SEM_SYSCALL);

	if (density[reg->reg_gmater] < 0) {
	    bu_log("Material type %d used, but has no density file entry.\n", reg->reg_gmater);
	    bu_log("  (region %s)\n", reg->reg_name);
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    reg->reg_gmater = 0;
	    bu_semaphore_release(BU_SEM_SYSCALL);
	}
	else if (density[reg->reg_gmater] >= 0) {
	    VBLEND2(dp->centroid, 0.5, ihitp->hit_point, 0.5, ohitp->hit_point);

	    /* Compute mass in terms of grams */
	    dp->weight = depth * density[reg->reg_gmater] *
		(fastf_t)reg->reg_los *
		cell_height * cell_height * 0.00001;
	    dp->volume = depth * cell_height * cell_width;
	}
    }
    return 1;	/* report hit to main routine */
}


int
miss(register struct application *UNUSED(ap))
{
    return 0;
}


int
overlap(struct application *UNUSED(ap), struct partition *UNUSED(pp),
	struct region *UNUSED(reg1), struct region *UNUSED(reg2),
	struct partition *UNUSED(hp))
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    noverlaps++;
    bu_semaphore_release(BU_SEM_SYSCALL);
    return 0;
}


/*
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init(struct application *ap, char *UNUSED(file), char *UNUSED(obj),
	  int minus_o, int UNUSED(minus_F))
{
    register size_t i;
    char buf[BUFSIZ+1];
    char linebuf[BUFSIZ+1];
    static char null = (char)0;
    const char *curdir = getenv("PWD");
    const char *homedir = getenv("HOME");
    int line;

    /* make sure they're not NULL */
    if (!curdir)
	curdir = "."; /* current dir */
    if (!homedir)
	homedir = ""; /* drop to root */

    if (!minus_o) {
	outfp = stdout;
	output_is_binary = 0;
    } else {
	if (outfp == NULL && outputfile != NULL && strlen(outputfile) > 0)
	    outfp = fopen(outputfile, "w");
    }

    for (i = 1; i < MAXMATLS; i++) {
	density[i]   = -1;
	dens_name[i] = &null;
    }

#define maxm(a, b) (a>b?a:b)

    i = maxm(strlen(curdir), strlen(homedir)) + strlen(DENSITY_FILE) + 2;
    /* densityfile is global to this file and will be used later (and then freed) */
    densityfile = (char *)bu_calloc((unsigned int)i, sizeof(char), "densityfile");

    snprintf(densityfile, i, "%s/%s", curdir, DENSITY_FILE);

    if ((densityfp = fopen(densityfile, "r")) == (FILE *)0) {
	snprintf(densityfile, i, "%s/%s", homedir, DENSITY_FILE);
	if ((densityfp = fopen(densityfile, "r")) == (FILE *)0) {
	    bu_log("Unable to load density file \"%s\" for reading\n", densityfile);
	    perror(densityfile);
	    if (minus_o) {
		fclose(outfp);
	    }
	    bu_exit(-1, NULL);
	}
    }

    /* Read in density in terms of grams/cm^3 */

    /* need to use bu_fgets instead of fscanf because fscanf stops
       scanning at first failure or error and we get an infinite loop */
    line = 0;
    while (bu_fgets(linebuf, BUFSIZ+1, densityfp)) {
	const int cmt = '#';
	int idx;
	float dens;
	char* c;

	++line;

	/* delete comments before processing */
	if ((c = strchr(linebuf, cmt)) != NULL) {
	    /* close linebuf with a newline and a null char */
	    *c++ = '\n';
	    *c   = '\0';
	}
	i = sscanf(linebuf, "%d %f %[^\n]", &idx, &dens, buf);
	if (i != 3) {
	    bu_log("error parsing line %d of density file.\n  %zu args recognized instead of 3\n",
		   line, i);
	    bu_log("  line buffer reads : %s\n", linebuf);
	    continue;
	}

	if (idx > 0 && idx < MAXMATLS ) {
	    density[idx] = dens;
	    dens_name[idx] = bu_strdup(buf);
	} else {
	    bu_log("Material index %d in '%s' is out of range.\n",
		   idx, densityfile );
	}
    }

    ap->a_hit = hit;
    ap->a_miss = miss;
    ap->a_overlap = overlap;
    ap->a_onehit = 0;
    if (minus_o) {
	fclose(outfp);
    }

    return 0;		/* no framebuffer needed */
}

/* beginning of a frame */
void
view_2init(struct application *ap, char *UNUSED(framename))
{
    register struct region *rp;
    register struct rt_i *rtip = ap->a_rt_i;

    for (BU_LIST_FOR(rp, region, &(rtip->HeadRegion))) {
	rp->reg_udata = (genptr_t)NULL;
    }
}

/* end of each pixel */
void
view_pixel(struct application *UNUSED(ap))
{
}

/* end of each line */
void
view_eol(struct application *UNUSED(ap))
{
}

/* a region ID sort comparison for use with bu_sort on a region array */
int region_ID_cmp(const void *p1,
		  const void *p2,
		  void *UNUSED(arg))
{
    /* cast into correct type--note the incoming pointer type is a
       pointer to a pointer which must be dereferenced! */
    struct region *r1 = *(struct region **)p1;
    struct region *r2 = *(struct region **)p2;

    if (r1->reg_regionid < r2->reg_regionid)
      return -1;
    else if (r1->reg_regionid > r2->reg_regionid)
      return +1;
    else
      return bu_strcmp(r1->reg_name, r2->reg_name);
}

/* end of a frame */
void
view_end(struct application *ap)
{
    register struct datapoint *dp;
    register struct region *rp;
    register fastf_t total_weight = 0;
    fastf_t sum_x = 0, sum_y = 0, sum_z = 0;
    struct rt_i *rtip = ap->a_rt_i;
    struct db_i *dbp = ap->a_rt_i->rti_dbip;
    fastf_t conversion = 1.0;	/* Conversion factor for mass */
    fastf_t volume = 0;
    char units[128] = {0};
    char unit2[128] = {0};
    int max_item = 0;
    time_t clockval;
    struct tm *locltime;
    char *timeptr;

    /* a sortable array is needed to have a consistently sorted region
       list for regression tests; add variables for such a use */
    struct region **rp_array = (struct region **)NULL;
    register int id = 0; /* new variable name to clarify purpose */
    int nregions = 0;
    int ridx; /* for region array */

    /* default units */
    bu_strlcpy(units, "grams", sizeof(units));
    bu_strlcpy(unit2, bu_units_string(dbp->dbi_local2base), sizeof(unit2));

    (void)time(&clockval);
    locltime = localtime(&clockval);
    timeptr = asctime(locltime);

    /* This section is necessary because libbu doesn't yet
     * support (nor do BRL-CAD databases store) defaults
     * for mass. Once it does, this section should be
     * reorganized.*/

    if (ZERO(dbp->dbi_local2base - 304.8)) {
	/* Feet */
	bu_strlcpy(units, "grams", sizeof(units));
    } else if (ZERO(dbp->dbi_local2base - 25.4)) {
	/* inches */
	conversion = 0.002204623;  /* lbs./gram */
	bu_strlcpy(units, "lbs.", sizeof(units));
    } else if (ZERO(dbp->dbi_local2base - 1.0)) {
	/* mm */
	conversion = 0.001;  /* kg/gram */
	bu_strlcpy(units, "kg", sizeof(units));
    } else if (ZERO(dbp->dbi_local2base - 1000.0)) {
	/* km */
	conversion = 0.001;  /* kg/gram */
	bu_strlcpy(units, "kg", sizeof(units));
    } else if (ZERO(dbp->dbi_local2base - 0.1)) {
	/* cm */
	bu_strlcpy(units, "grams", sizeof(units));
    } else {
	bu_log("Warning: base2mm=%g, using default of %s--%s\n",
	       dbp->dbi_base2local, units, unit2);
    }

    if (noverlaps)
	bu_log("%d overlap%c detected.\n\n", noverlaps,
		noverlaps==1 ? '\0' : 's');

    fprintf(outfp, "RT Weight Program Output:\n");
    fprintf(outfp, "\nDatabase Title: \"%s\"\n", dbp->dbi_title);
    fprintf(outfp, "Time Stamp: %s\n\nDensity Table Used:%s\n\n", timeptr, densityfile);
    fprintf(outfp, "Material  Density(g/cm^3)  Name\n");
    {
	register int i;
	for (i = 1; i < MAXMATLS; i++) {
	    if (density[i] >= 0)
		fprintf(outfp, "%5d     %10.4f       %s\n",
			i, density[i], dens_name[i]);
      }
    }

    /* is this test really necessary? or can we return if !rpt_overlap? */
    if (rpt_overlap) {
	/* move block scope variables here */
	fastf_t *item_wt;
	int start_ridx;

	/* grind through the region list WITHOUT printing but track region
	   IDs used and count regions for later use; also do bookkeeping
	   chores */
	for (BU_LIST_FOR(rp, region, &(rtip->HeadRegion))) {
	    register fastf_t weight = 0;
	    register fastf_t *ptr;

	    ++nregions; /* this is needed to create the region array */

	    /* keep track of the highest region ID */
	    if (max_item < rp->reg_regionid)
		max_item = rp->reg_regionid;

	    for (dp = (struct datapoint *)rp->reg_udata;
		 dp != (struct datapoint *)NULL; dp = dp->next) {
		sum_x  += dp->weight * dp->centroid[X];
		sum_y  += dp->weight * dp->centroid[Y];
		sum_z  += dp->weight * dp->centroid[Z];
		weight += dp->weight;
		volume += dp->volume;
	    }

	    weight *= conversion;
	    total_weight += weight;

	    BU_ALLOC(ptr, fastf_t);
	    *ptr = weight;
	    /* FIXME: shouldn't the existing reg_udata be bu_free'd first (see previous loop) */
	    /* FIXME: isn't the region list a "shared resource"? if so, can we use reg_udata so cavalierly? */
	    rp->reg_udata = (genptr_t)ptr;
	}

	/* make room for a zero ID number and an "end ID " so we can
	   use ID indexing and our usual loop idiom */
	max_item += 2;

	item_wt = (fastf_t *)bu_malloc(sizeof(fastf_t) * (max_item + 1), "item_wt");
	for (id = 1; id < max_item; id++)
	    item_wt[id] = -1.0;

	/* create and fill an array for handling region names and region IDs */
	rp_array = (struct region **)bu_malloc(sizeof(struct region *)
					       * nregions, "rp_array");

	ridx = 0; /* separate index for region array */
	for (BU_LIST_FOR(rp, region, &(rtip->HeadRegion))) {
	    rp_array[ridx++] = rp;

	    id = rp->reg_regionid;

	    /* FIXME: shouldn't we bu_free reg_udata after using here? */
	    if (item_wt[id] < 0)
		item_wt[id] = *(fastf_t *)rp->reg_udata;
	    else
		item_wt[id] += *(fastf_t *)rp->reg_udata;
	}

	/* sort the region array by ID, then by name */
	bu_sort(rp_array, nregions, sizeof(struct region *), region_ID_cmp, NULL);

	/* WEIGHT BY REGION NAME =============== */
	/* ^L is char code for FormFeed/NewPage */
	fprintf(outfp, "Weight by region name (in %s, density given in g/cm^3):\n\n", units);
	fprintf(outfp, " Weight   Matl  LOS  Material Name  Density Name\n");
	fprintf(outfp, "-------- ------ --- --------------- ------- -------------\n");

	start_ridx = 0; /* region array is indexed from 0 */
	for (id = 1; id < max_item; ++id) {
	    const int flen = 37; /* desired size of name field */

	    if (item_wt[id] < 0)
		continue;

	    /* since we're sorted by ID, we only need to start and end with the current ID */
	    for (ridx = start_ridx; ridx < nregions; ++ridx) {
		struct region *r = rp_array[ridx];
		if (r->reg_regionid == id) {
		    fastf_t weight = *(fastf_t *)r->reg_udata;
		    register size_t len = strlen(r->reg_name);
		    len = len > (size_t)flen ? len - (size_t)flen : 0;
		    fprintf(outfp, "%8.3f %5d  %3d %-15.15s %7.4f %-*.*s\n",
			    weight,
			    r->reg_gmater, r->reg_los,
			    dens_name[r->reg_gmater],
			    density[r->reg_gmater],
			    flen, flen, &r->reg_name[len]);
		}
		else if (r->reg_regionid > id) {
		    /* FIXME: an "else" alone should be good enough
		       because the test should not be necessary if we
		       trust the sorted array */
		    /* end loop and save this region index value for the next id iteration */
		    start_ridx = ridx;
		    break;
		}
	    }
	}

	/* WEIGHT BY REGION ID =============== */
	fprintf(outfp, "Weight by region ID (in %s):\n\n", units);
	fprintf(outfp, "  ID   Weight  Region Names\n");
	fprintf(outfp, "----- -------- --------------------\n");

	start_ridx = 0; /* region array is indexed from 0 */
	for (id = 1; id < max_item; ++id) {
	    const int flen = 65; /* desired size of name field */
	    int CR = 0;
	    const int ns = 15;

	    if (item_wt[id] < 0)
	       continue;

	    /* the following format string has 15 spaces before the region name: */
	    fprintf(outfp, "%5d %8.3f ", id, item_wt[id]);

	    /* since we're sorted by ID, we only need to start and end with the current ID */
	    for (ridx = start_ridx; ridx < nregions; ++ridx) {
		struct region *r = rp_array[ridx];
		if (r->reg_regionid == id) {
		    register size_t len = strlen(r->reg_name);
		    len = len > (size_t)flen ? len - (size_t)flen : 0;
		    if (CR) {
			/* need leading spaces */
			fprintf(outfp, "%*.*s", ns, ns, " ");
		    }
		    fprintf(outfp, "%-*.*s\n", flen, flen, &r->reg_name[len]);
		    CR = 1;
		}
		else if (r->reg_regionid > id) {
		    /* FIXME: an "else" alone should be good enough
		       because the test should not be necessary if we
		       trust the sorted array */
		    /* end loop and save this region index value for the next id iteration */
		    start_ridx = ridx;
		    break;
		}
	    }
	}

	/* now finished with heap variables */
	bu_free(item_wt, "item_wt");
	/* FIXME: shouldn't we bu_free reg_udata before freeing the region array? */
	bu_free(rp_array, "rp_array");
    }

    volume *= (dbp->dbi_base2local*dbp->dbi_base2local*dbp->dbi_base2local);
    sum_x  *= (conversion / total_weight) * dbp->dbi_base2local;
    sum_y  *= (conversion / total_weight) * dbp->dbi_base2local;
    sum_z  *= (conversion / total_weight) * dbp->dbi_base2local;

    fprintf(outfp, "RT Weight Program Output:\n");
    fprintf(outfp, "\nDatabase Title: \"%s\"\n", dbp->dbi_title);
    fprintf(outfp, "Time Stamp: %s\n\n", timeptr);
    fprintf(outfp, "Total volume = %g %s^3\n\n", volume, unit2);
    fprintf(outfp, "Centroid: X = %g %s\n", sum_x, unit2);
    fprintf(outfp, "          Y = %g %s\n", sum_y, unit2);
    fprintf(outfp, "          Z = %g %s\n", sum_z, unit2);
    fprintf(outfp, "\nTotal mass = %g %s\n\n", total_weight, units);

    /* now finished with density file name*/
    bu_free(densityfile, "density file name");

}

void view_setup(struct rt_i *UNUSED(rtip))
{
}

/* Associated with "clean" command, before new tree is loaded  */
void view_cleanup(struct rt_i *UNUSED(rtip))
{
}

void application_init (void)
{
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
