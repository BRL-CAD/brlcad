/*                           O P T . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2026 United States Government as represented by
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
/** @file rt/opt.c
 *
 * Option handling for Ray Tracing main program.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "bu/debug.h"
#include "bu/file.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/parallel.h"
#include "bu/units.h"
#include "bn/str.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm.h"

#include "./rtuif.h"
#include "./ext.h"


/* Note: struct parsing requires no space after the commas.  take care
 * when formatting this file.  if the compile breaks here, it means
 * that spaces got inserted incorrectly.
 */
#define COMMA ','

int rt_verbosity = -1;        /* blather incessantly by default */

size_t width = 0;                /* # of pixels in X */
size_t height = 0;                /* # of lines in Y */


/***** Variables shared with viewing model *** */
int doubles_out = 0;        /* u_char or double .pix output file */
fastf_t azimuth = 0.0, elevation = 0.0;
int lightmodel = 0;                /* Select lighting model */
int rpt_overlap = 1;        /* report overlapping region names */
int default_background = 1; /* Default is black */
int output_is_binary = 1;        /* !0 means output file is binary */

/**
 * When non-zero, embed scene + camera metadata into the output PNG as
 * tEXt chunks so that icv_diff / imgdiff can later reconstruct the
 * exact nirt shotlines for any differing pixel.
 * Enable with:  rt ... -c 'set embed_icv_metadata=1'
 */
int embed_icv_metadata = 0;

/***** end of sharing with viewing model *****/

/***** variables shared with worker() ******/
int query_x = 0;
int query_y = 0;
int Query_one_pixel = 0;
int query_optical_debug  = 0;
int query_debug = 0;
int stereo = 0;                         /* stereo viewing */
int hypersample = 0;                    /* number of extra rays to fire */
unsigned int jitter = 0;                /* ray jitter control variable */
fastf_t rt_perspective = (fastf_t)0.0;  /* presp (degrees X) 0 => ortho */
fastf_t aspect = (fastf_t)1.0;          /* view aspect ratio X/Y (needs to be 1.0 for g/G options) */
vect_t dx_model;                        /* view delta-X as model-space vect */
vect_t dy_model;                        /* view delta-Y as model-space vect */
vect_t dx_unit;                         /* view delta-X as unit-len vect */
vect_t dy_unit;                         /* view delta-Y as unit-len vect */
fastf_t cell_width = (fastf_t)0.0;      /* model space grid cell width */
fastf_t cell_height = (fastf_t)0.0;     /* model space grid cell height */
int cell_newsize = 0;                   /* new grid cell size */
point_t eye_model = {(fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0}; /* model-space location of eye */
fastf_t eye_backoff = (fastf_t)M_SQRT2; /* dist from eye to center */
mat_t Viewrotscale = { (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0,
		       (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0,
		       (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0,
		       (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0};
fastf_t viewsize = (fastf_t)0.0;
int incr_mode = 0;                      /* !0 for incremental resolution */
int full_incr_mode = 0;                 /* !0 for fully incremental resolution */
size_t incr_level = 0;                  /* current incremental level */
size_t incr_nlevel = 0;                 /* number of levels */
size_t full_incr_sample = 0;            /* current fully incremental sample */
size_t full_incr_nsamples = 0;          /* number of samples in the fully incremental mode */
ssize_t npsw = 1;                        /* number of worker PSWs to run */
struct resource resource[MAX_PSW] = {0};      /* memory resources */
int top_down = 0;                       /* render image top-down or bottom-up (default) */
int random_mode = 0;                    /* Mode to shoot rays at random directions */
int opencl_mode = 0;                    /* enable/disable OpenCL */
/***** end variables shared with worker() *****/

/***** Photon Mapping Variables *****/
double pmargs[9];
char pmfile[255];
/***** ************************ *****/

/***** variables shared with do.c *****/
int objc = 0;                           /* Number of cmd-line treetops */
char **objv = (char **)NULL;            /* array of treetop strings */
struct bu_ptbl *cmd_objs = NULL;        /* container for command specified objects */
char *string_pix_start = (char *)NULL;  /* string spec of starting pixel */
char *string_pix_end = (char *)NULL;    /* string spec of ending pixel */
int pix_start = -1;                     /* pixel to start at */
int pix_end = 0;                        /* pixel to end at */
int matflag = 0;                        /* read matrix from stdin */
int orientflag = 0;                     /* 1 means orientation has been set */
int desiredframe = 0;                   /* frame to start at */
int finalframe = -1;                    /* frame to halt at */
int curframe = 0;                       /* current frame number,
					 * also shared with view.c */
char *outputfile = (char *)NULL;        /* name of base of output file */
int benchmark = 0;                      /* No random numbers:  benchmark */

int sub_grid_mode = 0;                  /* mode to raytrace a rectangular portion of view */
int sub_xmin = 0;                       /* lower left of sub rectangle */
int sub_ymin = 0;
int sub_xmax = 0;                       /* upper right of sub rectangle */
int sub_ymax = 0;

/**
 * If this variable is set to zero, then "air" solids in the model
 * will be entirely discarded.
 * If this variable is set non-zero, then "air" solids will be
 * retained, and can be expected to show up in the partition lists.
 */
int use_air = 0;                        /* whether librt should handle air */

int save_overlaps = 0;                  /* flag for setting rti_save_overlaps */

int rtg_parallel = 0;

/***** end variables shared with do.c *****/


/***** variables shared with view.c *****/
double airdensity = 0.0;                /* is the scene hazy (we shade the void space) */
double haze[3] = { 0.8, 0.9, 0.99 };    /* color of the haze */
int do_kut_plane = 0;
plane_t kut_plane;

double units = 1.0;
int default_units = 1;
int model_units = 0;

/***** end variables shared with view.c *****/

const char *densityfile = NULL;       /* name of density file */

/* temporary kludge to get rt to use a tighter tolerance for raytracing */
fastf_t rt_dist_tol = (fastf_t)0.0005;  /* Value for rti_tol.dist */

fastf_t rt_perp_tol = (fastf_t)0.0;     /* Value for rti_tol.perp */
char *framebuffer = NULL;       /* desired framebuffer */

/**
 * space partitioning algorithm to use.  previously had experimental
 * grid support, but now only uses a Non-uniform Binary Spatial
 * Partitioning (BSP) tree.
 */
int space_partition = RT_PART_NUBSPT;

#define MAX_WIDTH (32*1024)


extern struct command_tab rt_do_tab[];


/* this helper function is used to increase a bit variable through
 * five levels (8 bits set at a time, 0 through level 4).  this can be
 * used to incrementally increase uint32 bits as typically used for
 * verbose and/or debug printing.
 */
static unsigned int
increase_level(unsigned int bits)
{
    if ((bits & 0x000000ff) != 0x000000ff)
	bits |= 0x000000ff;
    else if ((bits & 0x0000ff00) != 0x0000ff00)
	bits |= 0x0000ff00;
    else if ((bits & 0x00ff0000) != 0x00ff0000)
	bits |= 0x00ff0000;
    else if ((bits & 0xff000000) != 0xff000000)
	bits |= 0xff000000;

    return bits;
}


/* -----------------------------------------------------------------------
 * File-scope state used by option callbacks.  Reset at the start of each
 * get_args() call so that re-entrant callers (cm_opt in do.c) work correctly.
 * ----------------------------------------------------------------------- */

/* Set to 1 if the user requests help (-h / -?) */
static int want_help = 0;


/* =======================================================================
 * bu_opt argument-process callbacks
 *
 * Each callback follows the bu_opt_arg_process_t contract:
 *   returns -1  on error
 *   returns  0  if no argv entry was consumed (flag-style option)
 *   returns  N  (>0) for the number of argv entries consumed
 *
 * IMPORTANT: several callbacks below have cumulative side-effects (they
 * modify global counters).  bu_opt_parse() calls a callback speculatively
 * via opt_is_flag() – with msg==NULL – to decide whether a combined
 * single-dash string (e.g. "-vvv") should be treated as grouped flags or
 * as one flag with an embedded argument.  To prevent double-counting, all
 * non-idempotent callbacks guard their side-effects behind a (msg != NULL)
 * check.  Idempotent callbacks (those that simply assign a value) need no
 * such guard.
 * ======================================================================= */


/* -q / --rand-table-size */
static int
rt_opt_rand_table(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    int i;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "rand-table-size");
    i = atoi(argv[0]);
    if (i <= 0)
	bu_exit(EXIT_FAILURE, "-q %d is < 0\n", i);
    if (i > BN_RANDHALFTABSIZE)
	bu_exit(EXIT_FAILURE, "-q %d is > maximum (%d)\n", i, BN_RANDHALFTABSIZE);
    bn_randhalftabsize = i;
    return 1;
}


/* -m / --haze  density,r,g,b */
static int
rt_opt_haze(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    int n;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "haze");
    n = sscanf(argv[0], "%lg,%lg,%lg,%lg",
	       &airdensity, &haze[RED], &haze[GRN], &haze[BLU]);
    if (n != 4) {
	/* try with spaces after commas */
	n = sscanf(argv[0], "%lg, %lg, %lg, %lg",
		   &airdensity, &haze[RED], &haze[GRN], &haze[BLU]);
    }
    if (n != 4)
	bu_exit(EXIT_FAILURE, "ERROR: bad air density or haze 0.0-to-1.0 RGB values\n");
    return 1;
}


/* -j / --subgrid  xmin,ymin,xmax,ymax */
static int
rt_opt_subgrid(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    char *cp;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "subgrid");
    cp = (char *)argv[0];
    sub_xmin = atoi(cp);
    while (*cp >= '0' && *cp <= '9') cp++;
    while (*cp && (*cp < '0' || *cp > '9')) cp++;
    sub_ymin = atoi(cp);
    while (*cp >= '0' && *cp <= '9') cp++;
    while (*cp && (*cp < '0' || *cp > '9')) cp++;
    sub_xmax = atoi(cp);
    while (*cp >= '0' && *cp <= '9') cp++;
    while (*cp && (*cp < '0' || *cp > '9')) cp++;
    sub_ymax = atoi(cp);
    bu_log("Sub-rectangle: (%d, %d) (%d, %d)\n",
	   sub_xmin, sub_ymin, sub_xmax, sub_ymax);
    if (sub_xmin >= 0 && sub_xmin < sub_xmax &&
	sub_ymin >= 0 && sub_ymin < sub_ymax) {
	sub_grid_mode = 1;
    } else {
	sub_grid_mode = 0;
	bu_log("WARNING: bad sub-rectangle, ignored\n");
    }
    return 1;
}


/* -k / --cut-plane  xd,yd,zd,dist OR x,y,z,nx,ny,nz OR x,y,z */
static int
rt_opt_cut_plane(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    fastf_t f;
    point_t pt;
    vect_t nrml;
    const char *arg = argv[0];
    char *scan_arg;
    double scan[6];
    int n;
    size_t i, j, len;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "cut-plane");
    do_kut_plane = 1;

    len = strlen(arg);
    scan_arg = (char *)bu_malloc(len + 1, "cut-plane parse buffer");
    for (i = 0, j = 0; i < len; i++) {
	if (!isspace((unsigned char)arg[i]))
	    scan_arg[j++] = arg[i];
    }
    scan_arg[j] = '\0';

    n = sscanf(scan_arg, "%lg,%lg,%lg,%lg,%lg,%lg",
	       &scan[0], &scan[1], &scan[2], &scan[3], &scan[4], &scan[5]);
    if (n == 6) {
	VSET(pt, scan[0], scan[1], scan[2]);
	VSET(nrml, scan[3], scan[4], scan[5]);
	f = MAGNITUDE(nrml);
	if (f <= SMALL)
	    bu_exit(EXIT_FAILURE, "ERROR: bad normal for cutting plane, length=%g\n", f);
	VUNITIZE(nrml);
	VMOVE(kut_plane, nrml);
	/* Plane form is N . X = d, so derive d from the supplied point. */
	kut_plane[W] = VDOT(pt, nrml);
	bu_free(scan_arg, "cut-plane parse buffer");
	return 1;
    }

    n = sscanf(scan_arg, "%lg,%lg,%lg,%lg",
	       &scan[0], &scan[1], &scan[2], &scan[3]);
    if (n == 3) {
	VSET(pt, scan[0], scan[1], scan[2]);
	VSET(nrml, scan[0], scan[1], scan[2]);
	f = MAGNITUDE(nrml);
	if (f <= SMALL)
	    bu_exit(EXIT_FAILURE, "ERROR: bad normal for cutting plane, length=%g\n", f);
	VUNITIZE(nrml);
	VMOVE(kut_plane, nrml);
	kut_plane[W] = VDOT(pt, nrml);
	bu_free(scan_arg, "cut-plane parse buffer");
	return 1;
    }
    bu_free(scan_arg, "cut-plane parse buffer");
    if (n != 4)
	bu_exit(EXIT_FAILURE, "ERROR: bad cutting plane, expected xdir,ydir,zdir,dist or x,y,z,nx,ny,nz or x,y,z\n");
    HMOVE(kut_plane, scan); /* double to fastf_t */
    f = MAGNITUDE(kut_plane);
    if (f <= SMALL)
	bu_exit(EXIT_FAILURE, "ERROR: bad normal for cutting plane, length=%g\n", f);
    f = 1.0 / f;
    VSCALE(kut_plane, kut_plane, f);
    kut_plane[W] *= f;
    return 1;
}


/* -, / --space-partition  (temporarily disabled) */
static int
rt_opt_comma_disabled(struct bu_vls *UNUSED(msg), size_t UNUSED(argc), const char **UNUSED(argv), void *UNUSED(set_var))
{
    /* Use bu_exit so that this path is safe even when called from
     * bu_opt_parse's opt_is_flag() probe (msg == NULL) or from
     * the main processing pass.  We never want to silently continue. */
    bu_exit(EXIT_FAILURE,
	    "ERROR: the -,N space-partition option is temporarily disabled.\n"
	    "       Long-option support (--space-partition) will be enabled in a future update.\n");
    return -1; /* unreachable; keeps the compiler happy */
}


/* -c / --command  "script_command" */
static int
rt_opt_command(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "command");
    (void)rt_do_cmd((struct rt_i *)0, argv[0], rt_do_tab);
    return 1;
}


/* -C / --background  #/#/#  (parse color, then invoke rt_do_cmd) */
static int
rt_opt_bgcolor(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    char buf[128] = {0};
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "background");
    if (!bu_color_from_str(&color, argv[0]))
	bu_exit(EXIT_FAILURE, "ERROR: invalid color string: '%s'\n", argv[0]);
    snprintf(buf, sizeof(buf), "set background=%f/%f/%f",
	    color.buc_rgb[RED], color.buc_rgb[GRN], color.buc_rgb[BLU]);
    (void)rt_do_cmd((struct rt_i *)0, buf, rt_do_tab);
    return 1;
}


/* -T / --tolerance  dist[/perp] or dist[,perp] */
static int
rt_opt_tolerance(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    double f = 0;
    const char *cp;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "tolerance");
    if (sscanf(argv[0], "%lf", &f) == 1) {
	if (f > 0)
	    rt_dist_tol = f;
    }
    f = 0;
    if ((cp = strchr(argv[0], '/')) ||
	(cp = strchr(argv[0], COMMA))) {
	if (sscanf(cp+1, "%lf", &f) == 1) {
	    if (f > 0 && f < 1)
		rt_perp_tol = f;
	}
    }
    bu_log("Using tolerance dist=%lg perp=%lg\n", rt_dist_tol, rt_perp_tol);
    return 1;
}


/* -I / --objects-file  file  (load object list from a text file) */
static int
rt_opt_objects_file(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    FILE *objsfile = NULL;
    struct bu_vls oline = BU_VLS_INIT_ZERO;
    int oid;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "objects-file");
    if (!bu_file_exists(argv[0], NULL))
	bu_exit(1, "Object list file %s not found, aborting\n", argv[0]);
    if ((objsfile = fopen(argv[0], "r")) == NULL)
	bu_exit(1, "Unable to open object list file %s, aborting\n", argv[0]);
    objc = 0;
    while (bu_vls_gets(&oline, objsfile) >= 0) {
	objc++;
	bu_vls_trunc(&oline, 0);
    }
    fclose(objsfile);
    if (objc) {
	objv = (char **)bu_calloc(objc+1, sizeof(char *), "obj array");
	objsfile = fopen(argv[0], "r");
	oid = 0;
	bu_vls_trunc(&oline, 0);
	while (bu_vls_gets(&oline, objsfile) >= 0) {
	    objv[oid] = bu_strdup(bu_vls_addr(&oline));
	    bu_vls_trunc(&oline, 0);
	    oid++;
	}
	fclose(objsfile);
    }
    bu_vls_free(&oline);
    return 1;
}


/* -J / --jitter  #  (unsigned hex bit-vector) */
static int
rt_opt_jitter(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "jitter");
    sscanf(argv[0], "%x", &jitter);
    return 1;
}


/* -H / --hypersample  #  (also enables jitter) */
static int
rt_opt_hypersample(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "hypersample");
    hypersample = atoi(argv[0]);
    if (hypersample > 0)
	jitter = 1;
    return 1;
}


/* -F / --framebuffer  fb  (framebuffer is char*, so can't use bu_opt_str directly) */
static int
rt_opt_framebuffer(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "framebuffer");
    framebuffer = (char *)argv[0];
    return 1;
}


/* -N / --nmg-debug  #  (hex) */
static int
rt_opt_nmg_debug(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "nmg-debug");
    sscanf(argv[0], "%x", (unsigned int *)&nmg_debug);
    bu_log("NMG_debug=0x%x\n", nmg_debug);
    return 1;
}


/* -x / --rt-debug  #  (hex) */
static int
rt_opt_rt_debug(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "rt-debug");
    sscanf(argv[0], "%x", (unsigned int *)&rt_debug);
    return 1;
}


/* -X / --optical-debug  #  (hex) */
static int
rt_opt_optical_debug(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "optical-debug");
    sscanf(argv[0], "%x", (unsigned int *)&optical_debug);
    return 1;
}


/* -! / --bu-debug  #  (hex) */
static int
rt_opt_bu_debug_cb(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu-debug");
    sscanf(argv[0], "%x", (unsigned int *)&bu_debug);
    return 1;
}


/* -s / --size  #  (square width == height) */
static int
rt_opt_size(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    int i;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "size");
    i = atoi(argv[0]);
    if (i < 1 || i > MAX_WIDTH)
	fprintf(stderr, "squaresize=%d out of range\n", i);
    else
	width = height = i;
    return 1;
}


/* -n / --height  # */
static int
rt_opt_height(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    int i;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "height");
    i = atoi(argv[0]);
    if (i < 1 || i > MAX_WIDTH)
	fprintf(stderr, "height=%d out of range\n", i);
    else
	height = i;
    return 1;
}


/* -W / --white-background  (flag; calls rt_do_cmd) */
static int
rt_opt_white_bg(struct bu_vls *UNUSED(msg), size_t UNUSED(argc), const char **UNUSED(argv), void *UNUSED(set_var))
{
    (void)rt_do_cmd((struct rt_i *)0, "set background=1.0/1.0/1.0", rt_do_tab);
    default_background = 0;
    return 0; /* no argument consumed */
}


/* -A / --ambient  #
 * Wrapper callback needed because AmbientIntensity is a DLL-imported symbol
 * on Windows, so &AmbientIntensity cannot appear in a static initializer.  */
static int
rt_opt_ambient(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    fastf_t tmp = (fastf_t)0.0;
    int ret;
    ret = bu_opt_fastf_t(msg, argc, argv, &tmp);
    if (ret > 0)
	AmbientIntensity = tmp;
    return ret;
}


/* -w / --width  # */
static int
rt_opt_width(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    int i;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "width");
    i = atoi(argv[0]);
    if (i < 1 || i > MAX_WIDTH)
	fprintf(stderr, "width=%d out of range\n", i);
    else
	width = i;
    return 1;
}


/* -g / --cell-width  # (mm; also sets cell_newsize) */
static int
rt_opt_cell_width(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "cell-width");
    cell_width = atof(argv[0]);
    cell_newsize = 1;
    return 1;
}


/* -G / --cell-height  # (mm; also sets cell_newsize) */
static int
rt_opt_cell_height(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "cell-height");
    cell_height = atof(argv[0]);
    cell_newsize = 1;
    return 1;
}


/* -a / --azimuth  # (also clears matflag) */
static int
rt_opt_azimuth(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "azimuth");
    if (bn_decode_angle(&azimuth, argv[0]) == 0)
	fprintf(stderr, "WARNING: Unexpected units for azimuth angle, using default value\n");
    matflag = 0;
    return 1;
}


/* -e / --elevation  # (also clears matflag) */
static int
rt_opt_elevation(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "elevation");
    if (bn_decode_angle(&elevation, argv[0]) == 0)
	fprintf(stderr, "WARNING: Unexpected units for elevation angle, using default value\n");
    matflag = 0;
    return 1;
}


/* -l / --light-model  #[,pmargs...] */
static int
rt_opt_light_model(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "light-model");
    /* Get the lighting model number first; only strdup for photon-mapping args */
    lightmodel = atoi(argv[0]);
    if (lightmodel == 7) {
	char *buf = bu_strdup(argv[0]); /* strtok modifies its input */
	char *item;
	item = strtok(buf, ", "); /* consume the model number itself */
	/* Photon mapping arguments */
	item = strtok(NULL, ", ");
	pmargs[0] = item ? atoi(item) : 16384;  /* Number of Global Photons */
	item = strtok(NULL, ", ");
	pmargs[1] = item ? atof(item) : 50;     /* Percent for Caustic Photons */
	item = strtok(NULL, ", ");
	pmargs[2] = item ? atoi(item) : 10;     /* Irradiance Sample Rays */
	item = strtok(NULL, ", ");
	pmargs[3] = item ? atof(item) : 60.0;   /* Angular Tolerance */
	item = strtok(NULL, ", ");
	pmargs[4] = item ? atoi(item) : 0;      /* Random Seed */
	item = strtok(NULL, ", ");
	pmargs[5] = item ? atoi(item) : 0;      /* Importance Mapping */
	item = strtok(NULL, ", ");
	pmargs[6] = item ? atoi(item) : 0;      /* Irradiance Hypersampling */
	item = strtok(NULL, ", ");
	pmargs[7] = item ? atoi(item) : 0;      /* Visualize Irradiance */
	item = strtok(NULL, ", ");
	pmargs[8] = item ? atof(item) : 1.0;    /* Scale Lumens */
	item = strtok(NULL, ", ");
	if (item)
	    bu_strlcpy(pmfile, item, sizeof(pmfile));
	else
	    pmfile[0] = 0;
	bu_free(buf, "light model arg buf");
    }
    return 1;
}


/* -O / --output-double  file (double-precision .dpix output) */
static int
rt_opt_output_double(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "output-double");
    outputfile = (char *)argv[0];
    doubles_out = 1;
    return 1;
}


/* -o / --output  file (standard pix / png output) */
static int
rt_opt_output(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "output");
    outputfile = (char *)argv[0];
    doubles_out = 0;
    return 1;
}


/* -p / --perspective  # (degrees; 0 = orthographic) */
static int
rt_opt_perspective(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "perspective");
    rt_perspective = atof(argv[0]);
    if (rt_perspective < 0 || rt_perspective > 179) {
	fprintf(stderr, "persp=%g out of range\n", rt_perspective);
	rt_perspective = 0;
    }
    return 1;
}


/* -u / --units  units-string ("model" is special) */
static int
rt_opt_units(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "units");
    if (BU_STR_EQUAL(argv[0], "model")) {
	model_units = 1;
	default_units = 0;
    } else {
	units = bu_units_conversion(argv[0]);
	if (units <= 0.0) {
	    units = 1.0;
	    default_units = 1;
	    bu_log("WARNING: bad units, using default (%s)\n", bu_units_string(units));
	} else {
	    default_units = 0;
	}
    }
    return 1;
}


/* -v / --verbose  [#]
 *
 * Each bare -v increments the verbosity level by one "band" (8 bits).
 * Repeated letters such as -vvv or a hex value such as -v 0x3ff can
 * also be used.  Both forms of combined single-dash options are handled:
 *   -vvvv       -> embedded string "vvv" after the first 'v' char
 *   -v 0xfff    -> separate next argument "0xfff"
 *
 * SIDE-EFFECT GUARD: bu_opt_parse() probes callbacks speculatively via
 * opt_is_flag() with msg==NULL.  Non-idempotent operations are guarded by
 * (msg != NULL) to prevent double-counting on those probe calls.
 */
static int
rt_opt_verbose(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    if (argc > 0 && argv && argv[0] && *argv[0] != '\0') {
	const char *cp = argv[0];
	size_t num_v = 1;

	while (*cp == 'v') { num_v++; cp++; }

	if (*cp == '\0') {
	    /* pure string of 'v' characters, e.g. "vvv" from "-vvvv" */
	    if (msg != NULL) {
		size_t j;
		for (j = 0; j < num_v; j++)
		    rt_verbosity = increase_level(rt_verbosity);
		bu_printb("Verbosity:", rt_verbosity, VERBOSE_FORMAT);
		bu_log("\n");
	    }
	    return 1; /* consumed argument */
	}

	if (isxdigit((int)*cp)) {
	    /* hex value, possibly preceded by extra 'v' characters */
	    if (msg != NULL) {
		unsigned int scanned = 0;
		size_t j;
		for (j = 0; j < num_v; j++)
		    rt_verbosity = increase_level(rt_verbosity);
		if (sscanf(cp, "%x", &scanned) == 1)
		    rt_verbosity = (int)scanned;
		bu_printb("Verbosity:", rt_verbosity, VERBOSE_FORMAT);
		bu_log("\n");
	    }
	    return 1; /* consumed argument */
	}
	/* does not look like a verbosity argument; fall through */
    }

    /* no argument consumed: just increment verbosity by one level */
    if (msg != NULL) {
	rt_verbosity = increase_level(rt_verbosity);
	bu_printb("Verbosity:", rt_verbosity, VERBOSE_FORMAT);
	bu_log("\n");
    }
    return 0;
}


/* -P / --cpus  # (negative means "all but N") */
static int
rt_opt_cpus(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "cpus");
    npsw = (ssize_t)atoi(argv[0]);
    return 1;
}


/* -Q / --query-pixel  x,y  (toggles Query_one_pixel) */
static int
rt_opt_query_pixel(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "query-pixel");
    Query_one_pixel = !Query_one_pixel;
    sscanf(argv[0], "%d,%d", &query_x, &query_y);
    return 1;
}


/* -B / --benchmark  (flag; also calls bn_mathtab_constant) */
static int
rt_opt_benchmark(struct bu_vls *UNUSED(msg), size_t UNUSED(argc), const char **UNUSED(argv), void *UNUSED(set_var))
{
    benchmark = 1;
    bn_mathtab_constant();
    return 0; /* no argument consumed */
}


/* -b / --single-pixel  "x y"  (also forces npsw=1) */
static int
rt_opt_single_pixel(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "single-pixel");
    string_pix_start = (char *)argv[0];
    npsw = 1; /* cancel running in parallel */
    return 1;
}


/* -V / --view-aspect  #[:#]  (fraction or colon-separated ratio) */
static int
rt_opt_view_aspect(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    fastf_t xx, yy;
    char *cp;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "view-aspect");
    cp = (char *)argv[0];
    xx = atof(cp);
    while ((*cp >= '0' && *cp <= '9') || *cp == '.') cp++;
    while (*cp && (*cp < '0' || *cp > '9')) cp++;
    yy = atof(cp);
    if (ZERO(yy))
	aspect = xx;
    else
	aspect = xx / yy;
    if (aspect <= 0.0) {
	fprintf(stderr, "Bogus aspect %g, using 1.0\n", aspect);
	aspect = 1.0;
    }
    return 1;
}


/* -R / --no-overlap-report  (flag; sets rpt_overlap=0) */
static int
rt_opt_no_overlap(struct bu_vls *UNUSED(msg), size_t UNUSED(argc), const char **UNUSED(argv), void *UNUSED(set_var))
{
    rpt_overlap = 0;
    return 0; /* no argument consumed */
}


/* -+ / --text-output  t  (sub-option: 't' means text/non-binary output) */
static int
rt_opt_plus(struct bu_vls *msg, size_t argc, const char **argv, void *UNUSED(set_var))
{
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "+");
    switch (argv[0][0]) {
	case 't':
	    output_is_binary = 0;
	    break;
	default:
	    bu_exit(EXIT_FAILURE, "ERROR: unknown option %c\n", argv[0][0]);
    }
    return 1;
}


/* =======================================================================
 * Option descriptor table
 *
 * Each row is:  { shortopt, longopt, arg_helpstr, callback, set_var, help }
 * ======================================================================= */

static struct bu_opt_desc opt_defs[] = {
    /* --- Output -------------------------------------------------------- */
    {"o",  "output",          "file",    rt_opt_output,        NULL,
     "Output image file (e.g. image.pix or image.png)"},
    {"O",  "output-double",   "file",    rt_opt_output_double, NULL,
     "Output double-precision .dpix file"},
    {"F",  "framebuffer",     "fb",      rt_opt_framebuffer,   NULL,
     "Render to named framebuffer (see FB_FILE)"},

    /* --- Image dimensions ---------------------------------------------- */
    {"s",  "size",            "#",       rt_opt_size,          NULL,
     "Square image size in pixels (default 512)"},
    {"w",  "width",           "#",       rt_opt_width,         NULL,
     "Image width in pixels"},
    {"n",  "height",          "#",       rt_opt_height,        NULL,
     "Image height in scanlines"},

    /* --- Background / overlaps ----------------------------------------- */
    {"C",  "background",      "#/#/#",   rt_opt_bgcolor,       NULL,
     "Background color R/G/B (0-255 or 0.0-1.0)"},
    {"W",  "white-background","",        rt_opt_white_bg,      NULL,
     "White background (255/255/254)"},
    {"r",  "report-overlaps", "",        NULL,       &rpt_overlap,
     "Report overlapping region names (on by default)"},
    {"R",  "no-overlap-report","",       rt_opt_no_overlap,    NULL,
     "Suppress overlap region name reports"},

    /* --- View / camera ------------------------------------------------- */
    {"a",  "azimuth",         "deg",     rt_opt_azimuth,       NULL,
     "Auto-size: view azimuth in degrees (conflicts with -M)"},
    {"e",  "elevation",       "deg",     rt_opt_elevation,     NULL,
     "Auto-size: view elevation in degrees (conflicts with -M)"},
    {"p",  "perspective",     "deg",     rt_opt_perspective,   NULL,
     "Perspective angle in degrees (0 <= deg < 180; 0 = ortho)"},
    {"E",  "eye-backoff",     "#",       bu_opt_fastf_t,       &eye_backoff,
     "Distance from eye to model center (default sqrt(2))"},
    {"V",  "view-aspect",     "#[:#]",   rt_opt_view_aspect,   NULL,
     "View aspect ratio width/height (e.g. 1.33 or 4:3)"},
    {"M",  "read-matrix",     "",        NULL,       &matflag,
     "Read model2view matrix (+ animation script) from stdin"},

    /* --- Grid / cells -------------------------------------------------- */
    {"g",  "cell-width",      "mm",      rt_opt_cell_width,    NULL,
     "Grid cell width in mm"},
    {"G",  "cell-height",     "mm",      rt_opt_cell_height,   NULL,
     "Grid cell height in mm"},
    {"j",  "subgrid",         "xmin,ymin,xmax,ymax", rt_opt_subgrid, NULL,
     "Raytrace only a sub-rectangle of the view"},
    {"k",  "cut-plane",       "xdir,ydir,zdir,dist | x,y,z,nx,ny,nz | x,y,z", rt_opt_cut_plane, NULL,
     "Apply a cutting plane (equivalent to subtracting a halfspace)"},

    /* --- Rendering parameters ------------------------------------------ */
    {"A",  "ambient",         "#",       rt_opt_ambient,   NULL,
     "Ambient light intensity as a fraction 0.0-1.0"},
    {"l",  "light-model",     "#[,args]",rt_opt_light_model,   NULL,
     "Select lighting model (default 0; use -l7,... for photon mapping)"},
    {"m",  "haze",            "d,r,g,b", rt_opt_haze,          NULL,
     "Atmospheric haze: density and RGB color (e.g. 2.5e-8,.75,.95,.99)"},
    {"i",  "incremental",     "",        NULL,       &incr_mode,
     "Incremental (progressive) rendering"},
    {"t",  "top-down",        "",        NULL,       &top_down,
     "Render top-to-bottom (default is bottom-up)"},
    {"S",  "stereo",          "",        NULL,       &stereo,
     "Stereo viewing (left eye = red, right eye = blue)"},
    {"H",  "hypersample",     "#",       rt_opt_hypersample,   NULL,
     "Number of extra rays per pixel (also enables -J 1)"},
    {"J",  "jitter",          "#",       rt_opt_jitter,        NULL,
     "Ray jitter bit vector (1=cell jitter, 2=frame shift, 3=both)"},
    {"u",  "units",           "units",   rt_opt_units,         NULL,
     "Display units (\"model\" uses model-space units)"},
    {"U",  "use-air",         "#",       bu_opt_int, &use_air,
     "Whether librt retains air-coded regions (0=off default, 1=on)"},
    {"T",  "tolerance",       "#[/#]",   rt_opt_tolerance,     NULL,
     "Geometry tolerances: dist[/perp] (default 0.005/1e-6)"},
    {"z",  "opencl",          "#",       bu_opt_int, &opencl_mode,
     "Enable OpenCL-accelerated raytracing (must be compiled in)"},

    /* --- Animation / scripting ----------------------------------------- */
    {"c",  "command",         "cmd",     rt_opt_command,       NULL,
     "Execute an rt script command before raytracing"},
    {"d",  "density-file",    "file",    bu_opt_str, &densityfile,
     "Density definitions file"},
    {"D",  "start-frame",     "#",       bu_opt_int, &desiredframe,
     "Starting frame number for animation"},
    {"K",  "end-frame",       "#",       bu_opt_int, &finalframe,
     "Ending (kill-after) frame number for animation"},

    /* --- Single-pixel / sub-image ------------------------------------- */
    {"b",  "single-pixel",    "\"x y\"", rt_opt_single_pixel,  NULL,
     "Shoot one debug ray at pixel (x, y); forces serial execution"},
    {"Q",  "query-pixel",     "x,y",     rt_opt_query_pixel,   NULL,
     "Compute full image but enable debug for pixel (x,y)"},

    /* --- Object input -------------------------------------------------- */
    {"I",  "objects-file",    "file",    rt_opt_objects_file,  NULL,
     "Load object names from a text file (one per line)"},

    /* --- Output format ------------------------------------------------- */
    {"+",  "text-output",     "t",       rt_opt_plus,          NULL,
     "Output mode modifier: 't' selects non-binary (text) output"},

    /* --- Random-table size (DEPRECATED SHORT FORM: -q commonly means --quiet) */
    {"q",  "rand-table-size", "#",       rt_opt_rand_table,    NULL,
     "Size of random-number half-table (default BN_RANDHALFTABSIZE)"},

    /* --- Developer / debugging ----------------------------------------- */
    {"v",  "verbose",         "[#]",     rt_opt_verbose,       NULL,
     "Verbosity level; repeat (-vvvv) or give hex value (-v 0xff010030)"},
    {"x",  "rt-debug",        "#",       rt_opt_rt_debug,      NULL,
     "librt debug flags (hexadecimal bit vector, see raytrace.h)"},
    {"X",  "optical-debug",   "#",       rt_opt_optical_debug, NULL,
     "rt optical/program debug flags (hexadecimal)"},
    {"N",  "nmg-debug",       "#",       rt_opt_nmg_debug,     NULL,
     "libnmg debug flags (hexadecimal, see nmg.h)"},
    {"!",  "bu-debug",        "#",       rt_opt_bu_debug_cb,   NULL,
     "libbu debug flags (hexadecimal, see bu.h); -!1 causes bu_bomb core dump"},
    {"P",  "cpus",            "#",       rt_opt_cpus,          NULL,
     "Max processor cores to use (negative = all but N)"},
    {"B",  "benchmark",       "",        rt_opt_benchmark,     NULL,
     "Benchmark mode: disable all intentional randomness (dither, etc.)"},

    /* --- Space partition (temporarily disabled) ------------------------ */
    {",",  "",                "",        rt_opt_comma_disabled,NULL,
     "Space partition algorithm selector (temporarily disabled)"},

    /* --- Help ---------------------------------------------------------- */
    {"h",  "help",            "",        NULL, &want_help,
     "Print help and exit"},
    {"?",  "",                "",        NULL, &want_help,
     "Print help and exit"},

    BU_OPT_DESC_NULL
};


int
get_args(int argc, const char *argv[])
{
    char *env_str;
    int n_unknown;
    int i;
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    const char **opt_argv = NULL;

    /* Reset per-call state (supports re-entrant calls from cm_opt in do.c) */
    want_help = 0;
    bu_optind = 1; /* restore compat: callers read bu_optind after return */

    if (argc < 2) {
	/* Nothing to parse; leave bu_optind at 1 (== argc for argc==1) */
	bu_optind = argc;
	return 1;
    }

    /* Make a working copy of argv[1..argc-1] to pass to bu_opt_parse.
     * bu_opt_parse reorders its argv array to place unknowns at the front;
     * we must not shuffle the caller's original argv because main.c
     * accesses it by index (using bu_optind) after we return.
     *
     * Pre-processing: split any "-XVALUE" token (where X is a single-char
     * short option that takes an argument) into two separate tokens "-X"
     * and "VALUE".  This is necessary for the common shell idiom
     * -c"set foo=1 bar=2" which the shell delivers as a single argv token
     * "-cset foo=1 bar=2".  Without splitting, bu_opt_parse's can_be_opt()
     * would reject the string (it contains spaces) and the option would
     * never be recognized.  It also avoids a double-execution of the
     * arg_process callback that would otherwise occur via opt_is_flag(). */
    {
	int n_extra = 0;
	int di;
	for (i = 0; i < argc - 1; i++) {
	    const char *tok = argv[i + 1];
	    if (!tok || tok[0] != '-' || tok[1] == '-' || tok[1] == '\0' || tok[2] == '\0')
		continue;
	    /* tok is "-XREST" with len > 2: check if X is a short opt with arg */
	    for (di = 0; opt_defs[di].shortopt != NULL || opt_defs[di].longopt != NULL; di++) {
		if (opt_defs[di].shortopt && opt_defs[di].shortopt[0] == tok[1] &&
		    opt_defs[di].arg_process) {
		    n_extra++;
		    break;
		}
	    }
	}
	opt_argv = (const char **)bu_malloc(
	    (size_t)(argc - 1 + n_extra) * sizeof(const char *), "rt opt_argv");
	{
	    int out = 0;
	    char **split_flags = NULL;
	    int n_split_used = 0;
	    if (n_extra > 0)
		split_flags = (char **)bu_malloc((size_t)n_extra * sizeof(char *), "rt split flags");
	    for (i = 0; i < argc - 1; i++) {
		const char *tok = argv[i + 1];
		int split = 0;
		if (tok && tok[0] == '-' && tok[1] != '-' && tok[1] != '\0' && tok[2] != '\0') {
		    for (di = 0; opt_defs[di].shortopt != NULL || opt_defs[di].longopt != NULL; di++) {
			if (opt_defs[di].shortopt && opt_defs[di].shortopt[0] == tok[1] &&
			    opt_defs[di].arg_process) {
			    /* Emit "-X" as a fresh string and VALUE as a pointer into tok */
			    char *flag = (char *)bu_malloc(3, "rt short opt split");
			    flag[0] = '-'; flag[1] = tok[1]; flag[2] = '\0';
			    split_flags[n_split_used++] = flag;
			    opt_argv[out++] = flag;
			    opt_argv[out++] = tok + 2;
			    split = 1;
			    break;
			}
		    }
		}
		if (!split)
		    opt_argv[out++] = tok;
	    }
	    n_unknown = bu_opt_parse(&msgs, (size_t)out, opt_argv, opt_defs);

	    /* Emit any diagnostic messages produced by the parser */
	    if (bu_vls_strlen(&msgs) > 0)
		bu_log("%s", bu_vls_cstr(&msgs));
	    bu_vls_free(&msgs);

	    /* Free the split flag strings ("-X") that we allocated.
	     * Their pointers appear in opt_argv[known_args] which we
	     * never access again; the unknown (positional) entries at
	     * opt_argv[0..n_unknown-1] are all original argv pointers. */
	    for (i = 0; i < n_split_used; i++)
		bu_free(split_flags[i], "rt short opt split");
	    if (split_flags)
		bu_free(split_flags, "rt split flags");
	}
    }

    if (n_unknown < 0) {
	/* Fatal parse error */
	bu_free(opt_argv, "rt opt_argv");
	return -1;
    }

    /* Reject any remaining token that starts with '-' (unrecognised option) */
    {
	int bad = 0;
	for (i = 0; i < n_unknown; i++) {
	    if (opt_argv[i] && opt_argv[i][0] == '-') {
		fprintf(stderr, "ERROR: unrecognized option: %s\n", opt_argv[i]);
		bad++;
	    }
	}
	if (bad) {
	    bu_free(opt_argv, "rt opt_argv");
	    return -1;
	}
    }

    /* Help requested */
    if (want_help) {
	bu_free(opt_argv, "rt opt_argv");
	return 0;
    }

    /* Restore bu_optind to the position of model.g in the original argv.
     *
     * bu_opt_parse does not copy strings; the pointers in opt_argv[] are
     * the same pointers as in the original argv[].  We find the first
     * positional (unknown) arg in the original argv by pointer identity. */
    if (n_unknown > 0) {
	for (bu_optind = 1; bu_optind < argc; bu_optind++) {
	    if (argv[bu_optind] == opt_argv[0])
		break;
	}
    } else {
	bu_optind = argc; /* all tokens were options */
    }

    bu_free(opt_argv, "rt opt_argv");

    /* Sanity checks */
    if (aspect <= 0.0)
	aspect = 1.0;

    /* Propagate debug flags for backward compatibility */
    if (RT_G_DEBUG || OPTICAL_DEBUG || nmg_debug)
	bu_debug |= BU_DEBUG_COREDUMP;
    if (RT_G_DEBUG & RT_DEBUG_PARALLEL)
	bu_debug |= BU_DEBUG_PARALLEL;
    if (RT_G_DEBUG & RT_DEBUG_MATH)
	bu_debug |= BU_DEBUG_MATH;

    /* TODO: add options instead of reading from ENV */
    env_str = getenv("LIBRT_RAND_MODE");
    if (env_str != NULL && atoi(env_str) == 1) {
	random_mode = 1;
	bu_log("random mode\n");
    }
    /* TODO: Read from command line */
    /* Read from ENV if we're going to use the experimental mode */
    env_str = getenv("LIBRT_EXP_MODE");
    if (env_str != NULL && atoi(env_str) == 1) {
	full_incr_mode = 1;
	full_incr_nsamples = 10;
	bu_log("multi-sample mode\n");
    }

    return 1; /* OK */
}


void
color_hook(const struct bu_structparse *sp, const char *name, void *base, const char *value, void *UNUSED(data))
{
    struct bu_color color = BU_COLOR_INIT_ZERO;

    BU_CK_STRUCTPARSE(sp);

    if (!sp || !name || !value || sp->sp_count != 3 || bu_strcmp("%f", sp->sp_fmt))
	bu_bomb("color_hook(): invalid arguments");

    if (!bu_color_from_str(&color, value)) {
	bu_log("ERROR: invalid color string: '%s'\n", value);
	VSETALL(color.buc_rgb, 0.0);
    }

    VMOVE((fastf_t *)((char *)base + sp->sp_offset), color.buc_rgb);
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
