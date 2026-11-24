/*                        R T S H O T . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2026 United States Government as represented by
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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/debug.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "raytrace.h"
#include "bv/plot3.h"

#include "./rtuif.h"

struct inv_mat_data {
    const char *attr_key;
    struct bu_hash_tbl *tbl;
};

void
inv_mat_clbk(struct rt_i *rtip, struct db_tree_state *tsp, struct region *rp)
{
    if (!rtip || !tsp || !rp)
	return;

    struct inv_mat_data *d = (struct inv_mat_data *)rtip->rti_udata;
    if (!d)
	return;
    struct bu_hash_tbl *tbl = d->tbl;
    if (!d->attr_key || !tbl)
	return;

    if (!bu_avs_get(&tsp->ts_attrs, d->attr_key))
	return;

    uint32_t key = rp->reg_bit;

    matp_t inv_mat = (matp_t)bu_calloc(16, sizeof(fastf_t), "inv_mat");
    bn_mat_inv(inv_mat, tsp->ts_mat);

    bu_semaphore_acquire(RT_SEM_RESULTS);

    (void)bu_hash_set(tbl, (const uint8_t *)&key, sizeof(key), (void *)inv_mat);

    bu_semaphore_release(RT_SEM_RESULTS);
}

static FILE *plotfp;		/* For plotting into */

static int overlap_claimant_handling = 0;

struct rtshot_args {
    int help;
    point_t point;
    vect_t direction;
    point_t at;
    int use_air;
    int onehit;
    fastf_t ray_length;
    const char *overlap;
    const char *attributes;
    const char *inverse_attribute;
    int rings;
    int rays_per_ring;
    fastf_t bundle_radius;
    fastf_t grid_spacing;
    unsigned int bu_debug_mask;
    unsigned int rt_debug_mask;
    unsigned int optical_debug_mask;
    unsigned int nmg_debug_mask;
    const char *deprecated_t;
    const char *deprecated_b;
};

static int
rtshot_help_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    if (arg && !BU_STR_EQUAL(arg, "all")) {
	if (msg)
	    bu_vls_strcat(msg, "--help accepts only the optional value 'all'\n");
	return -1;
    }
    if (storage)
	*((int *)storage) = arg ? 2 : 1;
    return 0;
}

static int
rtshot_attributes_validate(struct bu_vls *msg, const char *arg)
{
    const unsigned char *cp = (const unsigned char *)arg;

    while (cp && *cp && isspace(*cp))
	cp++;
    if (cp && *cp)
	return 0;
    if (msg)
	bu_vls_strcat(msg, "attribute list must contain at least one name\n");
    return -1;
}

static const struct bu_arg_value_keyword rtshot_overlap_values[] = {
    {"resolve", BU_ARG_ALIASES("0"), "Resolve overlaps before reporting"},
    {"rebuild_fastgen", BU_ARG_ALIASES("1"),
	"Rebuild overlaps using FASTGEN plate-mode rules"},
    {"rebuild_all", BU_ARG_ALIASES("2"), "Rebuild and report all overlaps"},
    {"retain", BU_ARG_ALIASES("3"), "Retain overlaps without resolving them"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option rtshot_options[] = {
    BU_ARG_ATTACHED_OPTIONAL("h", "help", "help", struct rtshot_args,
	help, rtshot_help_parse, "all", "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VECTOR3("p", "point", struct rtshot_args, point, "x y z",
	"Set the ray starting point"),
    BU_ARG_VECTOR3("d", "direction", struct rtshot_args, direction, "x y z",
	"Set the ray direction vector (normalized before use)"),
    BU_ARG_VECTOR3("a", "at", struct rtshot_args, at, "x y z",
	"Set the point toward which the ray is fired"),
    BU_ARG_VALUE("U", "use-air", struct rtshot_args, use_air,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "value",
	"Set air-region reporting (default 1)"),
    BU_ARG_VALUE("o", "one-hit", struct rtshot_args, onehit,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "count",
	"Stop after the requested number of hits (0 means unlimited)"),
    BU_ARG_VALUE_VALIDATE("r", "ray-length", struct rtshot_args, ray_length,
	BU_ARG_VALUE_NUMBER, bu_arg_nonnegative_number_validate, "millimeters",
	"Set the maximum ray length in millimeters (0 means unlimited)"),
    BU_ARG_KEYWORD_VALUES("O", "overlap", struct rtshot_args, overlap,
	"mode", "Select overlap-claimant handling", rtshot_overlap_values),
    BU_ARG_VALUE_VALIDATE("v", "attributes", struct rtshot_args, attributes,
	BU_ARG_VALUE_STRING, rtshot_attributes_validate, "names",
	"Report a whitespace-separated list of region attributes"),
    BU_ARG_VALUE("A", "inverse-placement-attribute", struct rtshot_args,
	inverse_attribute, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "name",
	"Transform hits into local coordinates for regions carrying this attribute"),
    BU_ARG_INTEGER_RANGE("n", "rings", struct rtshot_args, rings,
	1, 1, 1, INT_MAX, "count",
	"Set ray-bundle rings in addition to the central ray"),
    BU_ARG_INTEGER_RANGE("c", "rays-per-ring", struct rtshot_args,
	rays_per_ring, 1, 1, 1, INT_MAX, "count",
	"Set the number of rays in each spiral bundle ring"),
    BU_ARG_VALUE_VALIDATE("R", "bundle-radius", struct rtshot_args,
	bundle_radius, BU_ARG_VALUE_NUMBER, bu_arg_nonnegative_number_validate,
	"millimeters", "Set the outer ray-bundle radius in millimeters"),
    BU_ARG_VALUE_VALIDATE("g", "grid-spacing", struct rtshot_args,
	grid_spacing, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"millimeters", "Set circular-grid ray spacing (default 1 mm)"),
    BU_ARG_VALUE("u", "bu-debug", struct rtshot_args, bu_debug_mask,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set libbu hexadecimal debug flags"),
    BU_ARG_VALUE("x", "rt-debug", struct rtshot_args, rt_debug_mask,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set librt hexadecimal debug flags"),
    BU_ARG_VALUE("X", "optical-debug", struct rtshot_args,
	optical_debug_mask, BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set optical hexadecimal debug flags; ray plotting writes rtshot.plot3"),
    BU_ARG_VALUE("N", "nmg-debug", struct rtshot_args, nmg_debug_mask,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set NMG hexadecimal debug flags"),
    BU_ARG_VALUE("t", NULL, struct rtshot_args, deprecated_t,
	BU_ARG_VALUE_RAW, BU_ARG_REQUIRED, "value", "Deprecated and ignored"),
    BU_ARG_VALUE("b", NULL, struct rtshot_args, deprecated_b,
	BU_ARG_VALUE_RAW, BU_ARG_REQUIRED, "value", "Deprecated and ignored"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand rtshot_operands[] = {
    BU_ARG_OPERAND("model.g", BU_ARG_VALUE_FILE, 1, 1,
	"BRL-CAD geometry database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 0,
	BU_ARG_COUNT_UNLIMITED, "Object to ray trace", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const rtshot_ray_options[] = {
    "point", "direction", "at", NULL
};
static const char * const rtshot_rings_require[] = {
    "rings", "rays-per-ring", NULL
};
static const char * const rtshot_rays_require[] = {
    "rays-per-ring", "rings", NULL
};
static const char * const rtshot_grid_requires[] = {
    "grid-spacing", "bundle-radius", NULL
};
static const char * const rtshot_grid_conflicts[] = {
    "grid-spacing", "rings", "rays-per-ring", NULL
};

static const struct bu_arg_constraint rtshot_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS,
	rtshot_ray_options, 2, 2,
	"select exactly two of --point, --direction, and --at"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_REQUIRES,
	BU_ARG_CONDITION_ALWAYS, rtshot_rings_require, 0, 0,
	"--rings requires --rays-per-ring"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_REQUIRES,
	BU_ARG_CONDITION_ALWAYS, rtshot_rays_require, 0, 0,
	"--rays-per-ring requires --rings"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_REQUIRES,
	BU_ARG_CONDITION_ALWAYS, rtshot_grid_requires, 0, 0,
	"--grid-spacing requires --bundle-radius"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_CONFLICTS,
	BU_ARG_CONDITION_ALWAYS, rtshot_grid_conflicts, 0, 0,
	"--grid-spacing selects a circular grid, not a ring bundle"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_help_group rtshot_help_groups[] = {
    BU_ARG_HELP_GROUP("General", NULL, "help", BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Ray definition", "Select exactly two forms", "point",
	BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Ray behavior", NULL, "use-air",
	BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Reported attributes", NULL, "attributes",
	BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Ray bundles", NULL, "rings", BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Diagnostics", NULL, "bu-debug",
	BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Compatibility", NULL, "t", BU_ARG_VISIBILITY_INTERNAL),
    BU_ARG_HELP_GROUP_NULL
};

static const struct bu_arg_option_visibility rtshot_visibility[] = {
    BU_ARG_OPTION_VISIBILITY("t", BU_ARG_VISIBILITY_INTERNAL),
    BU_ARG_OPTION_VISIBILITY("b", BU_ARG_VISIBILITY_INTERNAL),
    BU_ARG_OPTION_VISIBILITY_NULL
};

static const struct bu_arg_presentation rtshot_presentation =
    BU_ARG_PRESENTATION(rtshot_help_groups, rtshot_visibility, NULL);

static const struct bu_arg_schema rtshot_schema = BU_ARG_SCHEMA_PRESENTED(
    "rtshot", "Fire a ray at a BRL-CAD model and report its intersections",
    rtshot_options, rtshot_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, rtshot_constraints, NULL, NULL, NULL),
    &rtshot_presentation);

static const struct bu_arg_grammar rtshot_grammar = {
    &rtshot_schema, NULL, NULL
};

static int
rtshot_attributes_split(const char *input, char ***attributes, char **working)
{
    size_t limit;
    size_t count;

    if (!input || !attributes || !working)
	return -1;
    limit = strlen(input) + 1;
    *working = bu_strdup(input);
    *attributes = (char **)bu_calloc(limit + 1, sizeof(char *),
	"rtshot attributes");
    count = bu_argv_from_string(*attributes, limit, *working);
    if (count > 0)
	return 0;
    bu_free(*attributes, "rtshot attributes");
    bu_free(*working, "rtshot attribute working string");
    *attributes = NULL;
    *working = NULL;
    return -1;
}

extern int r_hit(struct application *ap, struct partition *PartHeadp, struct seg *segp);
extern int r_miss(register struct application *ap);

int bundle_hit(register struct application_bundle *bundle, struct partition_bundle *PartBundlep);
int bundle_miss(register struct application_bundle *bundle);

int
main(int argc, char **argv)
{
    struct application ap;
    static struct rt_i *rtip;
    const char *title_file;
    char idbuf[2048] = {0};		/* First ID record info */
    char **attrs = NULL;
    char *attr_working = NULL;
    int operand_index;
    int introspection;
    int set_at;
    int set_dir;
    const char **tree_argv;
    int tree_argc;
    struct resource res = RT_RESOURCE_INIT_ZERO;
    struct bu_vls attr_key = BU_VLS_INIT_ZERO;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct rtshot_args args = {0};

    args.use_air = 1;
    args.grid_spacing = 1.0;
    args.overlap = "resolve";

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &rtshot_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&rtshot_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help_mode(&rtshot_schema, bu_getprogname(),
	    args.help > 1 ? BU_ARG_HELP_EXTENDED : BU_ARG_HELP_DEFAULT);
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "rtshot help");
	return 0;
    }

    set_dir = bu_arg_schema_option_present(&rtshot_schema, argc - 1,
	(const char **)(argv + 1), "direction");
    set_at = bu_arg_schema_option_present(&rtshot_schema, argc - 1,
	(const char **)(argv + 1), "at");
    if (args.rings && args.rays_per_ring > (INT_MAX - 1) / args.rings) {
	bu_flog(stderr, "ray bundle exceeds the supported ray count\n");
	return 1;
    }

    if (bu_arg_schema_option_present(&rtshot_schema, argc - 1,
	    (const char **)(argv + 1), "bu-debug")) {
	bu_debug = args.bu_debug_mask;
	bu_flog(stderr, "librt bu_debug=x%x\n", bu_debug);
    }
    if (bu_arg_schema_option_present(&rtshot_schema, argc - 1,
	    (const char **)(argv + 1), "rt-debug")) {
	rt_debug = args.rt_debug_mask;
	bu_flog(stderr, "librt rt_debug=x%x\n", rt_debug);
    }
    if (bu_arg_schema_option_present(&rtshot_schema, argc - 1,
	    (const char **)(argv + 1), "optical-debug")) {
	optical_debug = args.optical_debug_mask;
	bu_flog(stderr, "optical_debug=x%x\n", optical_debug);
    }
    if (bu_arg_schema_option_present(&rtshot_schema, argc - 1,
	    (const char **)(argv + 1), "nmg-debug")) {
	nmg_debug = (uint32_t)args.nmg_debug_mask;
	bu_flog(stderr, "librt nmg_debug=x%x\n", nmg_debug);
    }
    if (args.deprecated_t)
	bu_log("DEPRECATION WARNING: remove -t option.\n"
	    "No longer used since 7.42 BoT ray tracing.\n");
    if (args.deprecated_b)
	bu_log("DEPRECATION WARNING: remove -b option.\n"
	    "No longer used since 7.42 BoT ray tracing.\n");

    if (BU_STR_EQUAL(args.overlap, "rebuild_fastgen"))
	overlap_claimant_handling = 1;
    else if (BU_STR_EQUAL(args.overlap, "rebuild_all"))
	overlap_claimant_handling = 2;
    else if (BU_STR_EQUAL(args.overlap, "retain"))
	overlap_claimant_handling = 3;

    if (args.attributes &&
	rtshot_attributes_split(args.attributes, &attrs, &attr_working) < 0) {
	bu_flog(stderr, "unable to parse the attribute list\n");
	return 1;
    }
    if (args.inverse_attribute)
	bu_vls_strcpy(&attr_key, args.inverse_attribute);

    RT_APPLICATION_INIT(&ap);
    VMOVE(ap.a_ray.r_pt, args.point);
    VMOVE(ap.a_ray.r_dir, args.direction);

    title_file = argv[operand_index + 1];
    tree_argv = (const char **)(argv + operand_index + 2);
    tree_argc = argc - operand_index - 2;

    /* Load database */
    if ((rtip=rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL) {
	bu_exit(2, "rtshot:  rt_dirbuild failure\n");
    }

    if (overlap_claimant_handling)
	rtip->rti_save_overlaps = 1;

    ap.a_rt_i = rtip;
    bu_flog(stderr, "db title:  %s\n", idbuf);
    rtip->useair = args.use_air;

    const char *default_objv[2] = {NULL, NULL};
    if (tree_argc <= 0) {
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	struct directory *dp = RT_DIR_NULL;
	int dret = db_default_object(rtip->rti_dbip, &dp, &msg);

	if (dret == 1) {
	    default_objv[0] = dp->d_namep;
	    tree_argv = default_objv;
	    tree_argc = 1;
	} else {
	    if (bu_vls_strlen(&msg))
		bu_log("%s", bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	    bu_vls_free(&attr_key);
	    rt_i_destroy(rtip);
	    bu_exit(1, "rtshot: no objects specified\n");
	}
	bu_vls_free(&msg);
    }

    /* Set up rti_getrees_clbk data */
    struct inv_mat_data *imd;
    BU_GET(imd, struct inv_mat_data);
    imd->attr_key = (bu_vls_strlen(&attr_key)) ? bu_vls_cstr(&attr_key) : NULL;
    imd->tbl = bu_hash_create(64);
    rtip->rti_udata = (void *)imd;
    rtip->rti_gettrees_clbk = &inv_mat_clbk;

    /* Walk trees */
    if (rt_gettrees_and_attrs(rtip, (const char **)attrs, tree_argc, tree_argv, 1)) {
	bu_vls_free(&attr_key);
	bu_hash_destroy(imd->tbl);
	BU_PUT(imd, struct inv_mat_data);
	bu_exit(1, "rt_gettrees FAILED\n");
    }
    ap.attrs = attrs;

    rt_prep(rtip);

    if (OPTICAL_DEBUG&OPTICAL_DEBUG_RAYPLOT) {
	if ((plotfp = fopen("rtshot.plot3", "w")) == NULL) {
	    perror("rtshot.plot3");
	    bu_exit(1, NULL);
	}
	pdv_3space(plotfp, rtip->rti_pmin, rtip->rti_pmax);
    }

    /* set up our resources */
    rt_init_resource(&res, 0, rtip);
    ap.a_resource = &res;

    /* Compute r_dir and r_pt from the inputs */
    if (set_at) {
	if (set_dir) {
	    vect_t diag;
	    fastf_t viewsize;
	    VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
	    viewsize = MAGNITUDE(diag);
	    VJOIN1(ap.a_ray.r_pt, args.at,
		   -viewsize/2.0, ap.a_ray.r_dir);
	} else {
	    /* set_pt */
	    VSUB2(ap.a_ray.r_dir, args.at, ap.a_ray.r_pt);
	}
    }
    VUNITIZE(ap.a_ray.r_dir);

    if (args.rays_per_ring) {
	bu_log("Central Ray:\n");
    }
    VPRINT("Pnt", ap.a_ray.r_pt);
    VPRINT("Dir", ap.a_ray.r_dir);

    ap.a_onehit = args.onehit;
    ap.a_ray_length = args.ray_length;

    /* Shoot Ray */
    ap.a_purpose = "main ray";
    ap.a_hit = r_hit;
    ap.a_miss = r_miss;

    if (args.rays_per_ring) {
	vect_t avec, bvec;
	struct xray *rp;
	int ray_count = args.rays_per_ring * args.rings + 1;

	/* create orthogonal rays for basis of bundle */
	bn_vec_ortho(avec, ap.a_ray.r_dir);
	VCROSS(bvec, ap.a_ray.r_dir, avec);
	VUNITIZE(bvec);

	rp = (struct xray *)bu_calloc(ray_count, sizeof(struct xray),
	    "ray bundle");
	rp[0] = ap.a_ray;	/* struct copy */
	rp[0].magic = RT_RAY_MAGIC;
	rt_raybundle_maker(rp, args.bundle_radius, avec, bvec,
	    args.rays_per_ring, args.rings);
	(void)rt_shootray_bundle(&ap, rp, ray_count);
	bu_free(rp, "ray bundle");
    } else if (args.bundle_radius > 0.0) {
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

	/* create orthogonal rays for basis of bundle */
	bn_vec_ortho(avec, ap.a_ray.r_dir);
	VCROSS(bvec, ap.a_ray.r_dir, avec);
	VUNITIZE(bvec);

	center_ray = ap.a_ray;	/* struct copy */
	(void)rt_gen_circular_grid(&b.b_rays, &center_ray,
	    args.bundle_radius, avec, args.grid_spacing);

	(void) rt_shootrays(&b);

	while (BU_LIST_WHILE(xr, xrays, &(b.b_rays.l))) {
	    BU_LIST_DEQUEUE(&(xr->l));
	    bu_free(xr, "bundled ray");
	}

	rt_clean(rtip);
    } else {
	(void)rt_shootray(&ap);
    }

    bu_vls_free(&attr_key);
    if (attrs)
	bu_free(attrs, "rtshot attributes");
    if (attr_working)
	bu_free(attr_working, "rtshot attribute working string");
    bu_hash_destroy(imd->tbl);
    BU_PUT(imd, struct inv_mat_data);

    return 0;
}


int r_hit(register struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    register struct partition *pp;
    register struct soltab *stp;
    fastf_t out;
    point_t inpt, outpt;
    vect_t inormal, onormal;
    struct curvature cur;
    cur.crv_c1 = 0.0;
    cur.crv_c2 = 0.0;
    VSETALL(cur.crv_pdir, 0.0);

    if ((pp=PartHeadp->pt_forw) == PartHeadp)
	return 0;		/* Nothing hit?? */

    if (overlap_claimant_handling == 1)
	rt_rebuild_overlaps(PartHeadp, ap, 1);
    else if (overlap_claimant_handling == 2)
	rt_rebuild_overlaps(PartHeadp, ap, 0);

    /* First, plot ray start to inhit */
    if (OPTICAL_DEBUG&OPTICAL_DEBUG_RAYPLOT) {
	if (pp->pt_inhit->hit_dist > 0.0001) {
	    VJOIN1(inpt, ap->a_ray.r_pt,
		   pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	    pl_color(plotfp, 0, 0, 255);
	    pdv_3line(plotfp, ap->a_ray.r_pt, inpt);
	}
    }
    for (; pp != PartHeadp; pp = pp->pt_forw) {
	matp_t inv_mat;
	const uint8_t *key = (uint8_t *)&(pp->pt_regionp->reg_bit);

	bu_log("\n--- Hit region %s (in %s, out %s) reg_bit = %d\n",
	       pp->pt_regionp->reg_name,
	       pp->pt_inseg->seg_stp->st_name,
	       pp->pt_outseg->seg_stp->st_name,
	       pp->pt_regionp->reg_bit);

	struct inv_mat_data *imd = (struct inv_mat_data *)ap->a_rt_i->rti_udata;

	inv_mat = (matp_t)bu_hash_get(imd->tbl, key, sizeof(pp->pt_regionp->reg_bit));
	if (inv_mat) {
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
	    bu_log("\ttransformed %s inhit = (%g %g %g)\n", imd->attr_key, V3ARGS(in_trans));
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
	    bu_log("\ttransformed %s outhit = (%g %g %g)\n", imd->attr_key, V3ARGS(out_trans));
	    bu_log("\ttransformed %s ray direction = (%g %g %g)\n", imd->attr_key, V3ARGS(dir_trans));
	}

	/* Plot inhit to outhit */
	if (OPTICAL_DEBUG&OPTICAL_DEBUG_RAYPLOT) {
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


int r_miss(register struct application *ap)
{
    bu_log("missed\n");
    if (OPTICAL_DEBUG&OPTICAL_DEBUG_RAYPLOT) {
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
    struct curvature cur = RT_CURVATURE_INIT_ZERO;
    point_t inpt, outpt;
    vect_t inormal, onormal;
    int raycnt=1;
    struct partition_list *pl;

    bu_log("------------------- bundle hit -------------------\n");

    /* First, plot bundle's application ray */
    if (OPTICAL_DEBUG & OPTICAL_DEBUG_RAYPLOT) {
	vect_t vout;

	VJOIN1(vout, bundle->b_ap.a_ray.r_pt, 10000, bundle->b_ap.a_ray.r_dir); /* to imply direction */
	pl_color(plotfp, 0, 0, 255);
	pdv_3line(plotfp, bundle->b_ap.a_ray.r_pt, vout);
    }

    for (BU_LIST_FOR(pl, partition_list, &(PartBundlep->list->l))) {
	bu_log("------------------- partition %d -------------------\n", raycnt++);
	/* First, plot ray start to inhit */
	pp = pl->PartHeadp.pt_forw;
	if (OPTICAL_DEBUG & OPTICAL_DEBUG_RAYPLOT) {
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
	    const uint8_t *key = (uint8_t *)&(pp->pt_regionp->reg_bit);

	    bu_log("\n--- Hit region %s (in %s, out %s) reg_bit = %d\n",
		   pp->pt_regionp->reg_name, pp->pt_inseg->seg_stp->st_name,
		   pp->pt_outseg->seg_stp->st_name, pp->pt_regionp->reg_bit);

	    struct inv_mat_data *imd = (struct inv_mat_data *)pl->ap->a_rt_i->rti_udata;
	    inv_mat = (matp_t)bu_hash_get(imd->tbl, key, sizeof(pp->pt_regionp->reg_bit));

	    if (inv_mat) {
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
		bu_log("\ttransformed %s inhit = (%g %g %g)\n", imd->attr_key, V3ARGS(in_trans));
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
		bu_log("\ttransformed %s outhit = (%g %g %g)\n", imd->attr_key, V3ARGS(out_trans));
		bu_log("\ttransformed %s ray direction = (%g %g %g)\n", imd->attr_key, V3ARGS(dir_trans));
	    }

	    /* Plot inhit to outhit */
	    if (OPTICAL_DEBUG & OPTICAL_DEBUG_RAYPLOT) {
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
    if (OPTICAL_DEBUG & OPTICAL_DEBUG_RAYPLOT) {
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
