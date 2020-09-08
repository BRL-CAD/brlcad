/*                    V I E W W E I G H T . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2020 United States Government as represented by
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
 * Ray Tracing program RTWEIGHT bottom half.
 *
 * This module outputs the weights and moments of a target model
 * using density values located in ".density" or "$HOME/.density"
 * Output is given in metric and english units, although input is
 * assumed in lbs/cu.in.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bu/app.h"
#include "bu/parallel.h"
#include "bu/str.h"
#include "bu/sort.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "vmath.h"
#include "raytrace.h"
#include "analyze.h"

#include "rt/db4.h"  /* FIXME: Yes, I know I shouldn't be peeking, put I
		    am only looking to see what units we prefer... */

#include "./rtuif.h"
#include "./ext.h"


extern struct resource resource[];

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};


const char title[] = "RT Weight";

int noverlaps = 0;
FILE *densityfp;
struct bu_vls densityfile_vls = BU_VLS_INIT_ZERO;
#define DENSITY_FILE ".density"

struct analyze_densities *density = NULL;

struct datapoint {
    struct datapoint *next;
    vect_t centroid;
    fastf_t weight;
    fastf_t volume;
};


extern int rpt_overlap;     	/* report region verbosely */
extern fastf_t cell_width;      	/* model space grid cell width */
extern fastf_t cell_height;     	/* model space grid cell height */
extern FILE *outfp;          	/* optional output file */
extern char *outputfile;     	/* name of base of output file */
extern char *densityfile;     	/* name of density file */
extern int output_is_binary;	/* !0 means output is binary */


static int
hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    struct partition *pp;
    register struct xray *rp = &ap->a_ray;
    void *addp;
    int part_count = 0;

    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
	register struct region *reg = pp->pt_regionp;
	register struct hit *ihitp = pp->pt_inhit;
	register struct hit *ohitp = pp->pt_outhit;
	register fastf_t depth;
	register struct datapoint *dp;
	long int material_id = reg->reg_gmater;
	fastf_t density_factor = analyze_densities_density(density, material_id);

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
	reg->reg_udata = (void *)dp;
	dp->next = (struct datapoint *)addp;
	bu_semaphore_release(BU_SEM_SYSCALL);

	/* no density factor means we use a default (zero) */
	{
	    /* keep track of the first few only */
	    #define MAX_MASS_TRACKED 256
	    static int mass_undef[MAX_MASS_TRACKED] = {0};

	    if (density_factor < 0) {
		if (material_id > 0 && material_id < MAX_MASS_TRACKED && !mass_undef[material_id]) {
		    bu_log("WARNING: Material ID %ld has no density file entry.\n"
			   "         Mass is undefined, only reporting volume.\n"
			   "       ( Encountered on region %s )\n\n", material_id, reg->reg_name);
		    mass_undef[material_id] = 1;
		}
		density_factor = analyze_densities_density(density, 0);
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		reg->reg_gmater = 0;
		bu_semaphore_release(BU_SEM_SYSCALL);
	    }
	}

	{
	    /* precompute partition size */
	    const fastf_t partition_volume = depth * cell_height * cell_width / (hypersample + 1);

	    /* convert reg_los percentage to factor */
	    const fastf_t los_factor = (fastf_t)reg->reg_los * 0.01;

	    dp->volume = partition_volume;
	    VBLEND2(dp->centroid, 0.5, ihitp->hit_point, 0.5, ohitp->hit_point);

	    if (density_factor >= 0) {
		/* Compute mass in terms of grams */
		dp->weight = partition_volume * los_factor * density_factor;
	    }
	}
    }
    return 1;	/* report hit to main routine */
}


static int
miss(register struct application *UNUSED(ap))
{
    return 0;
}


static int
overlap(struct application *UNUSED(ap), struct partition *UNUSED(pp), struct region *UNUSED(reg1), struct region *UNUSED(reg2), struct partition *UNUSED(hp))
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    noverlaps++;
    bu_semaphore_release(BU_SEM_SYSCALL);
    return 0;
}


/*
 * Called at the start of a run.
 * Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init(struct application *ap, char *UNUSED(file), char *UNUSED(obj), int minus_o, int UNUSED(minus_F))
{
    struct bu_vls pbuff_msgs = BU_VLS_INIT_ZERO;
    struct bu_mapped_file *dfile = NULL;
    char *dbuff = NULL;

    if (!minus_o) {
	outfp = stdout;
	output_is_binary = 0;
    } else {
	if (outfp == NULL && outputfile != NULL && strlen(outputfile) > 0)
	    outfp = fopen(outputfile, "w");
    }

    if (outfp == NULL && outputfile != NULL && strlen(outputfile) > 0) {
	bu_log("Unable to open output file \"%s\" for writing\n", outputfile);
	goto view_init_rtweight_fail;
    }


    /* densityfile is global to this file and will be used later (and then freed) */
    bu_vls_init(&densityfile_vls);
    if (analyze_densities_create(&density)) {
	bu_log("INTERNAL ERROR: Unable to initialize density table\n");
    }

    if (densityfile) {
	bu_vls_sprintf(&densityfile_vls, "%s", densityfile);
	if (!bu_file_exists(bu_vls_cstr(&densityfile_vls), NULL)) {
	    bu_log("Unable to load density file \"%s\" for reading\n", bu_vls_cstr(&densityfile_vls));
	    goto view_init_rtweight_fail;
	}

	dfile = bu_open_mapped_file(bu_vls_cstr(&densityfile_vls), "densities file");
	if (!dfile) {
	    bu_log("Unable to open density file \"%s\" for reading\n", bu_vls_cstr(&densityfile_vls));
	    goto view_init_rtweight_fail;
	}

	dbuff = (char *)(dfile->buf);


	/* Read in density */
	if (analyze_densities_load(density, dbuff, &pbuff_msgs, NULL) ==  0) {
	    bu_log("Unable to parse density file \"%s\":%s\n", bu_vls_cstr(&densityfile_vls), bu_vls_cstr(&pbuff_msgs));
	    bu_close_mapped_file(dfile);
	    goto view_init_rtweight_fail;
	}
	bu_close_mapped_file(dfile);


    } else {

	/* If we don't have a density file, first try the .g itself */
	struct directory *dp;
	dp = db_lookup(ap->a_rt_i->rti_dbip, "_DENSITIES", LOOKUP_QUIET);
	if (dp != (struct directory *)NULL) {
	    int ret, ecnt;
	    char *buf;
	    struct rt_db_internal intern;
	    struct rt_binunif_internal *bip;
	    struct bu_vls msgs = BU_VLS_INIT_ZERO;
	    if (rt_db_get_internal(&intern, dp, ap->a_rt_i->rti_dbip, NULL, &rt_uniresource) < 0) {
		bu_log("Could not import %s\n", dp->d_namep);
		goto view_init_rtweight_fail;
	    }
	    if ((intern.idb_major_type & DB5_MAJORTYPE_BINARY_MASK) == 0)
		goto view_init_rtweight_fail;

	    bip = (struct rt_binunif_internal *)intern.idb_ptr;
	    RT_CHECK_BINUNIF (bip);

	    buf = (char *)bu_calloc(bip->count+1, sizeof(char), "density buffer");
	    memcpy(buf, bip->u.int8, bip->count);
	    rt_db_free_internal(&intern);

	    ret = analyze_densities_load(density, buf, &msgs, &ecnt);

	    bu_free((void *)buf, "density buffer");

	    if (ret <= 0) {
		bu_log("Problem reading densities from .g file:\n%s\n", bu_vls_cstr(&msgs));
		bu_vls_free(&msgs);
		goto view_init_rtweight_fail;
	    }

	    bu_vls_sprintf(&densityfile_vls, "%s", ap->a_rt_i->rti_dbip->dbi_filename);

	} else {

	    /* If we still don't have density information, fall back on the pre-defined defaults */
	    bu_vls_sprintf(&densityfile_vls, "%s%c%s", bu_dir(NULL, 0, BU_DIR_CURR, NULL), BU_DIR_SEPARATOR, DENSITY_FILE);

	    if (!bu_file_exists(bu_vls_cstr(&densityfile_vls), NULL)) {
		bu_vls_sprintf(&densityfile_vls, "%s%c%s", bu_dir(NULL, 0, BU_DIR_HOME, NULL), BU_DIR_SEPARATOR, DENSITY_FILE);
		if (!bu_file_exists(bu_vls_cstr(&densityfile_vls), NULL)) {
		    bu_log("Unable to load density file \"%s\" for reading\n", bu_vls_cstr(&densityfile_vls));
		    goto view_init_rtweight_fail;
		}
	    }
	    dfile = bu_open_mapped_file(bu_vls_cstr(&densityfile_vls), "densities file");
	    if (!dfile) {
		bu_log("Unable to open density file \"%s\" for reading\n", bu_vls_cstr(&densityfile_vls));
		goto view_init_rtweight_fail;
	    }

	    dbuff = (char *)(dfile->buf);


	    /* Read in density */
	    if (analyze_densities_load(density, dbuff, &pbuff_msgs, NULL) ==  0) {
		bu_log("Unable to parse density file \"%s\":%s\n", bu_vls_cstr(&densityfile_vls), bu_vls_cstr(&pbuff_msgs));
		bu_close_mapped_file(dfile);
		goto view_init_rtweight_fail;
	    }
	    bu_close_mapped_file(dfile);
	}
    }

    ap->a_hit = hit;
    ap->a_miss = miss;
    ap->a_overlap = overlap;
    ap->a_onehit = 0;

    bu_vls_free(&pbuff_msgs);
    if (minus_o) {
	fclose(outfp);
    }

    return 0;		/* no framebuffer needed */

view_init_rtweight_fail:
    bu_vls_free(&densityfile_vls);
    bu_vls_free(&pbuff_msgs);
    analyze_densities_destroy(density);
    if (minus_o && outfp) {
	fclose(outfp);
    }

    bu_exit(-1, NULL);
}


/* beginning of a frame */
void
view_2init(struct application *ap, char *UNUSED(framename))
{
    register struct region *rp;
    register struct rt_i *rtip = ap->a_rt_i;

    for (BU_LIST_FOR(rp, region, &(rtip->HeadRegion))) {
	rp->reg_udata = (void *)NULL;
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
static int
region_ID_cmp(const void *p1, const void *p2, void *UNUSED(arg))
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
	bu_log("WARNING: base2mm=%g, using default of %s--%s\n",
	       dbp->dbi_base2local, units, unit2);
    }

    if (noverlaps)
	bu_log("%d overlap%c detected.\n\n", noverlaps,
	       noverlaps==1 ? '\0' : 's');

    fprintf(outfp, "RT Weight Program Output:\n");
    fprintf(outfp, "\nDatabase Title: \"%s\"\n", dbp->dbi_title);
    fprintf(outfp, "Time Stamp: %s\n\nDensity Table Used:%s\n\n", timeptr, bu_vls_cstr(&densityfile_vls));
    fprintf(outfp,
	    "Material  Density(g/cm^3)  Name\n"
	    "--------  ---------------  -------------\n");
    {
	long int curr_id = -1;
	while ((curr_id = analyze_densities_next(density, curr_id)) != -1) {
	    char *cname = analyze_densities_name(density, curr_id);
	    fastf_t cdensity = analyze_densities_density(density, curr_id);
	    fprintf(outfp, "%5ld     %10.4f       %s\n", curr_id, cdensity*1000, cname);
	    bu_free(cname, "free name copy");
	}
    }

    /* NOTE: rtweight repurposes the -r report overlap flag, used here
     * as a report region information verbosely flag by default.  Thus
     * -R will output just the summary.
     */
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
		sum_x += dp->weight * dp->centroid[X];
		sum_y += dp->weight * dp->centroid[Y];
		sum_z += dp->weight * dp->centroid[Z];
		weight += dp->weight;
		volume += dp->volume;
	    }

	    weight *= conversion;
	    total_weight += weight;

	    BU_ALLOC(ptr, fastf_t);
	    *ptr = weight;
	    /* FIXME: shouldn't the existing reg_udata be bu_free'd first (see previous loop) */
	    /* FIXME: isn't the region list a "shared resource"? if so, can we use reg_udata so cavalierly? */
	    rp->reg_udata = (void *)ptr;
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

	    if (item_wt[id] < 0)
		continue;

	    /* since we're sorted by ID, we only need to start and end with the current ID */
	    for (ridx = start_ridx; ridx < nregions; ++ridx) {
		struct region *r = rp_array[ridx];
		if (r->reg_regionid == id) {
		    char *cname = analyze_densities_name(density, r->reg_gmater);
		    fastf_t cdensity = analyze_densities_density(density, r->reg_gmater);
		    fastf_t weight = *(fastf_t *)r->reg_udata;
		    fprintf(outfp, "%8.3f %5d  %3d %-15.15s %7.4f %s\n",
			    weight,
			    r->reg_gmater, r->reg_los,
			    cname,
			    cdensity*1000,
			    r->reg_name);
		    if (cname) {
			bu_free(cname, "free name copy");
		    }
		} else if (r->reg_regionid > id) {
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
		    if (CR) {
			/* need leading spaces */
			fprintf(outfp, "%*.*s", ns, ns, " ");
		    }
		    fprintf(outfp, "%s\n", r->reg_name);
		    CR = 1;
		} else if (r->reg_regionid > id) {
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
    sum_x *= (conversion / total_weight) * dbp->dbi_base2local;
    sum_y *= (conversion / total_weight) * dbp->dbi_base2local;
    sum_z *= (conversion / total_weight) * dbp->dbi_base2local;

    fprintf(outfp, "RT Weight Program Output:\n");
    fprintf(outfp, "\nDatabase Title: \"%s\"\n", dbp->dbi_title);
    fprintf(outfp, "Time Stamp: %s\n\n", timeptr);
    fprintf(outfp, "Total volume = %g %s^3\n\n", volume, unit2);
    fprintf(outfp, "Centroid: X = %g %s\n", sum_x, unit2);
    fprintf(outfp, "          Y = %g %s\n", sum_y, unit2);
    fprintf(outfp, "          Z = %g %s\n", sum_z, unit2);
    fprintf(outfp, "\nTotal mass = %g %s\n\n", total_weight, units);

    /* now finished with density file name*/
    bu_vls_free(&densityfile_vls);

    analyze_densities_destroy(density);
}


void
view_setup(struct rt_i *UNUSED(rtip))
{
}


/* Associated with "clean" command, before new tree is loaded */
void
view_cleanup(struct rt_i *UNUSED(rtip))
{
}


void
application_init(void)
{
    option("", "-o file.out", "Weights and Moments output file", 0);
    option("", "-d density.txt", "Density definitions input file", 0);
    option("", "-r", "Report verbosely mass of each region", 0);
    /* this reassignment hack ensures help is last in the first list */
    option("dummy", "-? or -h", "Display help", 1);
    option("", "-? or -h", "Display help", 1);
    option(NULL, "-C", "Disabled, not implemented", -2);
    option(NULL, "-W", "Disabled, non implemented", -2);
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
