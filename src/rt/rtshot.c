/*                        R T S H O T . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2011 United States Government as represented by
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
/** @file rt/rtshot.c
 *
 * Demonstration Ray Tracing main program, using RT library.
 * Fires a single ray, given any two of these three parameters:
 *	start point
 *	at point
 *	direction vector
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"

#include "./rtuif.h"


extern int rt_shootray_bundle (struct application *ap, struct xray *rays, int nrays);
extern int rt_raybundle_maker(struct xray *rp, double radius, const fastf_t *avec, const fastf_t *bvec, int rays_per_ring, int nring);


void
usage(const char *argv0)
{
    bu_log("Usage:  %s [options] model.g objects...\n", argv0);
    bu_log(" -U #		Set reporting of air regions (default=1)\n");
    bu_log(" -u #		Set libbu debug flag\n");
    bu_log(" -x #		Set librt debug flags\n");
    bu_log(" -X #		Set rt program debug flags\n");
    bu_log(" -N #		Set NMG debug flags\n");
    bu_log(" -d # # #	Set direction vector\n");
    bu_log(" -p # # #	Set starting point\n");
    bu_log(" -a # # #	Set shoot-at point\n");
    bu_log(" -t #		Set number of triangles per piece for BOT's (default is 4)\n");
    bu_log(" -b #		Set threshold number of triangles to use pieces (default is 32)\n");
    bu_log(" -O #		Set overlap-claimant handling\n");
    bu_log(" -o #		Set onehit flag\n");
    bu_log(" -r #		Set ray length\n");
    bu_log(" -n #		Set number of rings for ray bundle\n");
    bu_log(" -c #		Set number of rays per ring for ray bundle\n");
    bu_log(" -R #		Set radius for ray bundle in mm\n");
    bu_log(" -g #		Set ray bundle grid size in mm(default 1mm)\n");
    bu_log(" -v \"attribute_name1 attribute_name2 ...\" Show attribute values\n");
}


static FILE *plotfp;		/* For plotting into */

int set_dir = 0;
int set_pt = 0;
int set_at = 0;
int set_onehit = 0;
int set_air = 1;
fastf_t set_ray_length = 0.0;
vect_t at_vect;
int overlap_claimant_handling = 0;
fastf_t grid_size = -1.0; /* gridsize in mm */
int rays_per_ring = 0;
int num_rings = 0;
fastf_t bundle_radius = 0.0;

extern int hit(struct application *ap, struct partition *PartHeadp, struct seg *segp);
extern int miss(register struct application *ap);

int bundle_hit(register struct application_bundle *bundle, struct partition_bundle *PartBundlep);
int bundle_miss(register struct application_bundle *bundle);

/*
 * M A I N
 */
int
main(int argc, char **argv)
{
    struct application ap;
    static struct rt_i *rtip;
    char *title_file;
    char idbuf[2048] = {0};		/* First ID record info */
    char *ptr;
    int attr_count=0, i;
    char **attrs = (char **)NULL;
    const char *argv0 = argv[0];

    if (argc < 3) {
	usage(argv0);
	return 1;
    }

    RT_APPLICATION_INIT(&ap);

    argc--;
    argv++;
    while (argv[0][0] == '-') switch (argv[0][1]) {
	case 'R':
	    bundle_radius = atof(argv[1]);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'n':
	    num_rings = atoi(argv[1]);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'g':
	    grid_size = atof(argv[1]);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'c':
	    rays_per_ring = atoi(argv[1]);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'v':
	    /* count the number of attribute names provided */
	    ptr = argv[1];
	    while (*ptr) {
		while (*ptr && isspace(*ptr))
		    ptr++;
		if (*ptr)
		    attr_count++;
		while (*ptr && !isspace(*ptr))
		    ptr++;
	    }

	    if (attr_count == 0) {
		bu_log("missing list of attribute names!\n");
		usage(argv0);
		return 1;
	    }

	    /* allocate enough for a null terminated list */
	    attrs = (char **)bu_calloc(attr_count + 1, sizeof(char *), "attrs");

	    /* use strtok to actually grab the names */
	    i = 0;
	    ptr = strtok(argv[1], "\t ");
	    while (ptr && i < attr_count) {
		attrs[i] = bu_strdup(ptr);
		ptr = strtok((char *)NULL, "\t ");
		i++;
	    }
	    argc -= 2;
	    argv += 2;
	    break;
	case 't':
	    rt_bot_tri_per_piece = atoi(argv[1]);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'b':
	    rt_bot_minpieces = atoi(argv[1]);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'o':
	    sscanf(argv[1], "%d", &set_onehit);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'r':
	    {
		float ray_len;

		sscanf(argv[1], "%f", &ray_len);
		set_ray_length = ray_len;
	    }
	    argc -= 2;
	    argv += 2;
	    break;
	case 'U':
	    sscanf(argv[1], "%d", &set_air);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'u':
	    sscanf(argv[1], "%x", (unsigned int *)&bu_debug);
	    fprintf(stderr, "librt bu_debug=x%x\n", bu_debug);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'x':
	    sscanf(argv[1], "%x", (unsigned int *)&rt_g.debug);
	    fprintf(stderr, "librt rt_g.debug=x%x\n", rt_g.debug);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'X':
	    sscanf(argv[1], "%x", (unsigned int *)&rdebug);
	    fprintf(stderr, "rdebug=x%x\n", rdebug);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'N':
	    sscanf(argv[1], "%x", (unsigned int *)&rt_g.NMG_debug);
	    fprintf(stderr, "librt rt_g.NMG_debug=x%x\n", rt_g.NMG_debug);
	    argc -= 2;
	    argv += 2;
	    break;
	case 'd':
	    if (argc < 4) goto err;
	    ap.a_ray.r_dir[X] = atof(argv[1]);
	    ap.a_ray.r_dir[Y] = atof(argv[2]);
	    ap.a_ray.r_dir[Z] = atof(argv[3]);
	    set_dir = 1;
	    argc -= 4;
	    argv += 4;
	    continue;

	case 'p':
	    if (argc < 4) goto err;
	    ap.a_ray.r_pt[X] = atof(argv[1]);
	    ap.a_ray.r_pt[Y] = atof(argv[2]);
	    ap.a_ray.r_pt[Z] = atof(argv[3]);
	    set_pt = 1;
	    argc -= 4;
	    argv += 4;
	    continue;

	case 'a':
	    if (argc < 4) goto err;
	    at_vect[X] = atof(argv[1]);
	    at_vect[Y] = atof(argv[2]);
	    at_vect[Z] = atof(argv[3]);
	    set_at = 1;
	    argc -= 4;
	    argv += 4;
	    continue;

	case 'O':
	    {
		if (BU_STR_EQUAL(argv[1], "resolve") || BU_STR_EQUAL(argv[1], "0"))
		    overlap_claimant_handling = 0;
		else if (BU_STR_EQUAL(argv[1], "rebuild_fastgen") || BU_STR_EQUAL(argv[1], "1"))
		    overlap_claimant_handling = 1;
		else if (BU_STR_EQUAL(argv[1], "rebuild_all") || BU_STR_EQUAL(argv[1], "2"))
		    overlap_claimant_handling = 2;
		else if (BU_STR_EQUAL(argv[1], "retain") || BU_STR_EQUAL(argv[1], "3"))
		    overlap_claimant_handling = 3;
		else {
		    bu_log("Illegal argument (%s) to '-O' option.  Must be:\n", argv[1]);
		    bu_log("\t'resolve' or '0'\n");
		    bu_log("\t'rebuild_fastgen' or '1'\n");
		    bu_log("\t'rebuild_all' or '2'\n");
		    bu_log("\t'retain' or '3'\n");
		    bu_exit(1, NULL);
		}
		argc -= 2;
		argv += 2;
	    }
	    continue;

	default:
    err:
	    usage(argv0);
	    return 1;
    }
    if (argc < 2) {
	usage(argv0);
	bu_exit(1, "%s: BRL-CAD geometry database not specified\n", argv0);
    }

    if (set_dir + set_pt + set_at != 2) goto err;

    if (num_rings != 0 || rays_per_ring != 0) {
	if (num_rings <= 0 || rays_per_ring <= 0 || bundle_radius <= 0.0) {
	    fprintf(stderr, "Must have all of \"-R\", \"-n\", and \"-c\" set\n");
	    goto err;
	}
    }

    /* Load database */
    title_file = argv[0];
    argv++;
    argc--;
    if ((rtip=rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL) {
	bu_exit(2, "rtshot:  rt_dirbuild failure\n");
    }

    if (overlap_claimant_handling)
	rtip->rti_save_overlaps = 1;

    ap.a_rt_i = rtip;
    fprintf(stderr, "db title:  %s\n", idbuf);
    rtip->useair = set_air;

    /* Walk trees */
    if (rt_gettrees_and_attrs(rtip, (const char **)attrs, argc, (const char **)argv, 1)) {
	bu_exit(1, "rt_gettrees FAILED\n");
    }
    ap.attrs = attrs;

    rt_prep(rtip);

    if (R_DEBUG&RDEBUG_RAYPLOT) {
	if ((plotfp = fopen("rtshot.plot", "w")) == NULL) {
	    perror("rtshot.plot");
	    bu_exit(1, NULL);
	}
	pdv_3space(plotfp, rtip->rti_pmin, rtip->rti_pmax);
    }

    /* Compute r_dir and r_pt from the inputs */
    if (set_at) {
	if (set_dir) {
	    vect_t diag;
	    fastf_t viewsize;
	    VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
	    viewsize = MAGNITUDE(diag);
	    VJOIN1(ap.a_ray.r_pt, at_vect,
		   -viewsize/2.0, ap.a_ray.r_dir);
	} else {
	    /* set_pt */
	    VSUB2(ap.a_ray.r_dir, at_vect, ap.a_ray.r_pt);
	}
    }
    VUNITIZE(ap.a_ray.r_dir);

    if (rays_per_ring) {
	bu_log("Central Ray:\n");
    }
    VPRINT("Pnt", ap.a_ray.r_pt);
    VPRINT("Dir", ap.a_ray.r_dir);

    if (set_onehit)
	ap.a_onehit = set_onehit;
    else
	ap.a_onehit = 0;

    if (set_ray_length > 0.0)
	ap.a_ray_length = set_ray_length;
    else
	ap.a_ray_length = 0.0;

    /* Shoot Ray */
    ap.a_purpose = "main ray";
    ap.a_hit = hit;
    ap.a_miss = miss;

    if (rays_per_ring) {
	vect_t avec, bvec;
	struct xray *rp;

	/* create orthogonal rays for basis of bundle */
	bn_vec_ortho(avec, ap.a_ray.r_dir);
	VCROSS(bvec, ap.a_ray.r_dir, avec);
	VUNITIZE(bvec);

	rp = (struct xray *)bu_calloc(sizeof(struct xray),
				      (rays_per_ring * num_rings) + 1,
				      "ray bundle");
	rp[0] = ap.a_ray;	/* struct copy */
	rp[0].magic = RT_RAY_MAGIC;
	rt_raybundle_maker(rp, bundle_radius, avec, bvec, rays_per_ring, num_rings);
	(void)rt_shootray_bundle(&ap, rp, (rays_per_ring * num_rings) + 1);
    } else if (bundle_radius > 0.0) {
	vect_t avec, bvec;
	struct xray center_ray;
	struct application_bundle b;
	struct xrays *xr;

	b.b_ap = ap;

	/* to use default bundling routines set b.b_ap.a_hit
	 * and b.b_ap.a_miss to NULL */
	b.b_ap.a_hit = NULL;
	b.b_ap.a_miss = NULL;

	BU_LIST_INIT(&b.b_rays.l);
	b.b_hit = bundle_hit;
	b.b_miss = bundle_miss;

	if (grid_size <= 0.0)
	    grid_size = 1.0;

	/* create orthogonal rays for basis of bundle */
	bn_vec_ortho(avec, ap.a_ray.r_dir);
	VCROSS(bvec, ap.a_ray.r_dir, avec);
	VUNITIZE(bvec);

	center_ray = ap.a_ray;	/* struct copy */
	(void)rt_gen_circular_grid(&b.b_rays, &center_ray, bundle_radius, avec, grid_size);

	(void) rt_shootrays(&b);

	while (BU_LIST_WHILE(xr, xrays, &(b.b_rays.l))) {
	    BU_LIST_DEQUEUE(&(xr->l));
	    bu_free(xr, "bundled ray");
	}

	rt_clean(rtip);
    } else {
	(void)rt_shootray(&ap);
    }

    return 0;
}


int hit(register struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    register struct partition *pp;
    register struct soltab *stp;
    struct curvature cur;
    fastf_t out;
    point_t inpt, outpt;
    vect_t inormal, onormal;

    if ((pp=PartHeadp->pt_forw) == PartHeadp)
	return 0;		/* Nothing hit?? */

    if (overlap_claimant_handling == 1)
	rt_rebuild_overlaps(PartHeadp, ap, 1);
    else if (overlap_claimant_handling == 2)
	rt_rebuild_overlaps(PartHeadp, ap, 0);

    /* First, plot ray start to inhit */
    if (R_DEBUG&RDEBUG_RAYPLOT) {
	if (pp->pt_inhit->hit_dist > 0.0001) {
	    VJOIN1(inpt, ap->a_ray.r_pt,
		   pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	    pl_color(plotfp, 0, 0, 255);
	    pdv_3line(plotfp, ap->a_ray.r_pt, inpt);
	}
    }
    for (; pp != PartHeadp; pp = pp->pt_forw) {
	matp_t inv_mat;
	Tcl_HashEntry *entry;

	bu_log("\n--- Hit region %s (in %s, out %s) reg_bit = %d\n",
	       pp->pt_regionp->reg_name,
	       pp->pt_inseg->seg_stp->st_name,
	       pp->pt_outseg->seg_stp->st_name,
	       pp->pt_regionp->reg_bit);

	entry = Tcl_FindHashEntry((Tcl_HashTable *)ap->a_rt_i->Orca_hash_tbl,
				  (const char *)(size_t)pp->pt_regionp->reg_bit);
	if (!entry) {
	    inv_mat = (matp_t)NULL;
	} else {
	    inv_mat = (matp_t)Tcl_GetHashValue(entry);
	    bn_mat_print("inv_mat", inv_mat);
	}

	if (pp->pt_overlap_reg) {
	    struct region *pp_reg;
	    int j=-1;

	    bu_log("    Claiming regions:\n");
	    while ((pp_reg=pp->pt_overlap_reg[++j]))
		bu_log("        %s\n", pp_reg->reg_name);
	}

	/* inhit info */
	stp = pp->pt_inseg->seg_stp;
	VJOIN1(inpt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	RT_HIT_NORMAL(inormal, pp->pt_inhit, stp, &(ap->a_ray), pp->pt_inflip);
	RT_CURVATURE(&cur, pp->pt_inhit, pp->pt_inflip, stp);

	rt_pr_hit("  In", pp->pt_inhit);
	VPRINT("  Ipoint", inpt);
	VPRINT("  Inormal", inormal);
	bu_log("   PDir (%g, %g, %g) c1=%g, c2=%g\n",
	       V3ARGS(cur.crv_pdir), cur.crv_c1, cur.crv_c2);

	if (inv_mat) {
	    point_t in_trans;

	    MAT4X3PNT(in_trans, inv_mat, inpt);
	    bu_log("\ttransformed ORCA inhit = (%g %g %g)\n", V3ARGS(in_trans));
	}

	/* outhit info */
	stp = pp->pt_outseg->seg_stp;
	VJOIN1(outpt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
	RT_HIT_NORMAL(onormal, pp->pt_outhit, stp, &(ap->a_ray), pp->pt_outflip);
	RT_CURVATURE(&cur, pp->pt_outhit, pp->pt_outflip, stp);

	rt_pr_hit("  Out", pp->pt_outhit);
	VPRINT("  Opoint", outpt);
	VPRINT("  Onormal", onormal);
	bu_log("   PDir (%g, %g, %g) c1=%g, c2=%g\n",
	       V3ARGS(cur.crv_pdir), cur.crv_c1, cur.crv_c2);

	if (inv_mat) {
	    point_t out_trans;
	    vect_t dir_trans;

	    MAT4X3PNT(out_trans, inv_mat, outpt);
	    MAT4X3VEC(dir_trans, inv_mat, ap->a_ray.r_dir);
	    VUNITIZE(dir_trans);
	    bu_log("\ttranformed ORCA outhit = (%g %g %g)\n", V3ARGS(out_trans));
	    bu_log("\ttransformed ORCA ray direction = (%g %g %g)\n", V3ARGS(dir_trans));
	}

	/* Plot inhit to outhit */
	if (R_DEBUG&RDEBUG_RAYPLOT) {
	    if ((out = pp->pt_outhit->hit_dist) >= INFINITY)
		out = 10000;	/* to imply the direction */

	    VJOIN1(outpt,
		   ap->a_ray.r_pt, out,
		   ap->a_ray.r_dir);
	    pl_color(plotfp, 0, 255, 255);
	    pdv_3line(plotfp, inpt, outpt);
	}

	{
	    struct region *regp = pp->pt_regionp;
	    int i;

	    if (ap->attrs) {
		bu_log("\tattribute values:\n");
		i = 0;
		while (ap->attrs[i] && regp->attr_values.count) {
		    bu_log("\t\t%s:\n", ap->attrs[i]);
		    bu_log("\t\t\tstring rep = %s\n",
			   bu_avs_get(&(regp->attr_values), ap->attrs[i]));
		  /*  bu_log("\t\t\tlong rep = %d\n",
			   BU_MRO_GETLONG(regp->attr_values[i]));
		    bu_log("\t\t\tdouble rep = %f\n",
			   BU_MRO_GETDOUBLE(regp->attr_values[i]));*/
		    i++;
		}
	    }
	}
    }
    return 1;
}


int miss(register struct application *ap)
{
    bu_log("missed\n");
    if (R_DEBUG&RDEBUG_RAYPLOT) {
	vect_t out;

	VJOIN1(out, ap->a_ray.r_pt,
	       10000, ap->a_ray.r_dir);	/* to imply direction */
	pl_color(plotfp, 190, 0, 0);
	pdv_3line(plotfp, ap->a_ray.r_pt, out);
    }
    return 0;
}


int bundle_hit(register struct application_bundle *bundle, struct partition_bundle *PartBundlep)
{
    register struct partition *pp;
    register struct soltab *stp;
    struct curvature cur;
    point_t inpt, outpt;
    vect_t inormal, onormal;
    int raycnt=1;
    struct partition_list *pl;

    bu_log("------------------- bundle hit -------------------\n");

    /* First, plot bundle's application ray */
    if (R_DEBUG & RDEBUG_RAYPLOT) {
	vect_t vout;

	VJOIN1(vout, bundle->b_ap.a_ray.r_pt, 10000, bundle->b_ap.a_ray.r_dir); /* to imply direction */
	pl_color(plotfp, 0, 0, 255);
	pdv_3line(plotfp, bundle->b_ap.a_ray.r_pt, vout);
    }

    for (BU_LIST_FOR(pl, partition_list, &(PartBundlep->list->l))) {
	bu_log("------------------- partition %d -------------------\n", raycnt++);
	/* First, plot ray start to inhit */
	pp = pl->PartHeadp.pt_forw;
	if (R_DEBUG & RDEBUG_RAYPLOT) {
	    if (pp->pt_inhit->hit_dist > 0.0001) {
		VJOIN1(inpt, pl->ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
		       pl->ap->a_ray.r_dir);
		pl_color(plotfp, 0, 0, 255);
		pdv_3line(plotfp, pl->ap->a_ray.r_pt, inpt);
	    }
	}
	for (; pp != &pl->PartHeadp; pp = pp->pt_forw) {
	    fastf_t out;
	    matp_t inv_mat;
	    Tcl_HashEntry *entry;

	    bu_log("\n--- Hit region %s (in %s, out %s) reg_bit = %d\n",
		   pp->pt_regionp->reg_name, pp->pt_inseg->seg_stp->st_name,
		   pp->pt_outseg->seg_stp->st_name, pp->pt_regionp->reg_bit);

	    entry = Tcl_FindHashEntry(
		(Tcl_HashTable *) pl->ap->a_rt_i->Orca_hash_tbl,
		(const char *) (size_t) pp->pt_regionp->reg_bit);
	    if (!entry) {
		inv_mat = (matp_t) NULL;
	    } else {
		inv_mat = (matp_t) Tcl_GetHashValue(entry);
		bn_mat_print("inv_mat", inv_mat);
	    }

	    if (pp->pt_overlap_reg) {
		struct region *pp_reg;
		int j = -1;

		bu_log("    Claiming regions:\n");
		while ((pp_reg = pp->pt_overlap_reg[++j]))
		    bu_log("        %s\n", pp_reg->reg_name);
	    }

	    /* inhit info */
	    stp = pp->pt_inseg->seg_stp;
	    VJOIN1(inpt, pl->ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
		   pl->ap->a_ray.r_dir);
	    RT_HIT_NORMAL(inormal, pp->pt_inhit, stp, &(pl->ap->a_ray),
			  pp->pt_inflip);
	    RT_CURVATURE(&cur, pp->pt_inhit, pp->pt_inflip, stp);

	    rt_pr_hit("  In", pp->pt_inhit);
	    VPRINT("  Ipoint", inpt);
	    VPRINT("  Inormal", inormal);
	    bu_log("   PDir (%g, %g, %g) c1=%g, c2=%g\n", V3ARGS(cur.crv_pdir),
		   cur.crv_c1, cur.crv_c2);

	    if (inv_mat) {
		point_t in_trans;

		MAT4X3PNT(in_trans, inv_mat, inpt);
		bu_log("\ttransformed ORCA inhit = (%g %g %g)\n", V3ARGS(
			   in_trans));
	    }

	    /* outhit info */
	    stp = pp->pt_outseg->seg_stp;
	    VJOIN1(outpt, pl->ap->a_ray.r_pt, pp->pt_outhit->hit_dist,
		   pl->ap->a_ray.r_dir);
	    RT_HIT_NORMAL(onormal, pp->pt_outhit, stp, &(pl->ap->a_ray),
			  pp->pt_outflip);
	    RT_CURVATURE(&cur, pp->pt_outhit, pp->pt_outflip, stp);

	    rt_pr_hit("  Out", pp->pt_outhit);
	    VPRINT("  Opoint", outpt);
	    VPRINT("  Onormal", onormal);
	    bu_log("   PDir (%g, %g, %g) c1=%g, c2=%g\n", V3ARGS(cur.crv_pdir),
		   cur.crv_c1, cur.crv_c2);

	    if (inv_mat) {
		point_t out_trans;
		vect_t dir_trans;

		MAT4X3PNT(out_trans, inv_mat, outpt);
		MAT4X3VEC(dir_trans, inv_mat, pl->ap->a_ray.r_dir);
		VUNITIZE(dir_trans);
		bu_log("\ttranformed ORCA outhit = (%g %g %g)\n", V3ARGS(
			   out_trans));
		bu_log("\ttransformed ORCA ray direction = (%g %g %g)\n",
		       V3ARGS(dir_trans));
	    }

	    /* Plot inhit to outhit */
	    if (R_DEBUG & RDEBUG_RAYPLOT) {
		if ((out = pp->pt_outhit->hit_dist) >= INFINITY)
		    out = 10000; /* to imply the direction */

		VJOIN1(outpt, pl->ap->a_ray.r_pt, out, pl->ap->a_ray.r_dir);
		pl_color(plotfp, 0, 255, 255);
		pdv_3line(plotfp, inpt, outpt);
	    }

	    {
		struct region *regp = pp->pt_regionp;
		int i;

		if (pl->ap->attrs) {
			bu_log("\tattribute values:\n");
			i = 0;
			while (pl->ap->attrs[i] && regp->attr_values.count) {
				bu_log("\t\t%s:\n", pl->ap->attrs[i]);
				bu_log("\t\t\tstring rep = %s\n",
						bu_avs_get(&(regp->attr_values), pl->ap->attrs[i]));
				/*  bu_log("\t\t\tlong rep = %d\n",
				    BU_MRO_GETLONG(regp->attr_values[i]));
				    bu_log("\t\t\tdouble rep = %f\n",
				    BU_MRO_GETDOUBLE(regp->attr_values[i]));*/
				i++;
			}
		}
	    }
	}
    }
    return 1;
}
int bundle_miss(register struct application_bundle *bundle) {
    bu_log("bundle missed\n");
    if (R_DEBUG & RDEBUG_RAYPLOT) {
	vect_t out;

	VJOIN1(out, bundle->b_ap.a_ray.r_pt, 10000, bundle->b_ap.a_ray.r_dir); /* to imply direction */
	pl_color(plotfp, 190, 0, 0);
	pdv_3line(plotfp, bundle->b_ap.a_ray.r_pt, out);
    }
    return 0;
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
