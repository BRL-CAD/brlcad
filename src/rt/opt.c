/*                           O P T . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2022 United States Government as represented by
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
#include "bu/getopt.h"
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
size_t npsw = 1;                        /* number of worker PSWs to run */
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


int
get_args(int argc, const char *argv[])
{
    register int c;
    register int i;
    char *env_str;
    FILE *objsfile = NULL;
    struct bu_vls oline = BU_VLS_INIT_ZERO;
    int oid = 0;
    bu_optind = 1;                /* restart */
    npsw = bu_avail_cpus();

#define GETOPT_STR	\
    ".:, :@:a:b:c:d:e:f:g:m:ij:k:l:n:o:p:q:rs:tu:v::w:x:z:A:BC:D:E:F:G:H:I:J:K:MN:O:P:Q:RST:U:V:WX:!:+:h?"

    while ((c=bu_getopt(argc, (char * const *)argv, GETOPT_STR)) != -1) {
	if (bu_optopt == '?')
	    /* specifically asking for help? */
	    c = 'h';
	switch (c) {
	    case 'q':
		i = atoi(bu_optarg);
		if (i <= 0) {
		    bu_exit(EXIT_FAILURE, "-q %d is < 0\n", i);
		}
		if (i > BN_RANDHALFTABSIZE) {
		    bu_exit(EXIT_FAILURE, "-q %d is > maximum (%d)\n", i, BN_RANDHALFTABSIZE);
		}
		bn_randhalftabsize = i;
		break;
	    case 'm':
		i = sscanf(bu_optarg, "%lg, %lg, %lg, %lg", &airdensity, &haze[RED], &haze[GRN], &haze[BLU]);
		if (i != 4) {
		    bu_exit(EXIT_FAILURE, "ERROR: bad air density or haze 0.0-to-1.0 RGB values\n");
		}
		break;
	    case 't':
		top_down = 1;
		break;
	    case 'j':
		{
		    register char *cp = bu_optarg;

		    sub_xmin = atoi(cp);
		    while ((*cp >= '0' && *cp <= '9')) cp++;
		    while (*cp && (*cp < '0' || *cp > '9')) cp++;
		    sub_ymin = atoi(cp);
		    while ((*cp >= '0' && *cp <= '9')) cp++;
		    while (*cp && (*cp < '0' || *cp > '9')) cp++;
		    sub_xmax = atoi(cp);
		    while ((*cp >= '0' && *cp <= '9')) cp++;
		    while (*cp && (*cp < '0' || *cp > '9')) cp++;
		    sub_ymax = atoi(cp);

		    bu_log("Sub-rectangle: (%d, %d) (%d, %d)\n",
			   sub_xmin, sub_ymin,
			   sub_xmax, sub_ymax);
		    if (sub_xmin >= 0 && sub_xmin < sub_xmax &&
			sub_ymin >= 0 && sub_ymin < sub_ymax) {
			sub_grid_mode = 1;
		    } else {
			sub_grid_mode = 0;
			bu_log("WARNING: bad sub-rectangle, ignored\n");
		    }
		}
		break;
	    case 'k': {
		/* define cutting plane */
		fastf_t f;
		double scan[4];

		do_kut_plane = 1;
		i = sscanf(bu_optarg, "%lg, %lg, %lg, %lg", &scan[0], &scan[1], &scan[2], &scan[3]);
		if (i != 4) {
		    bu_exit(EXIT_FAILURE, "ERROR: bad cutting plane\n");
		}
		HMOVE(kut_plane, scan); /* double to fastf_t */

		/* verify that normal has unit length */
		f = MAGNITUDE(kut_plane);
		if (f <= SMALL) {
		    bu_exit(EXIT_FAILURE, "Bad normal for cutting plane, length=%g\n", f);
		}
		f = 1.0 /f;
		VSCALE(kut_plane, kut_plane, f);
		kut_plane[W] *= f;
		break;
	    }
	    case COMMA:
		space_partition = atoi(bu_optarg);
		break;
	    case 'c':
		(void)rt_do_cmd((struct rt_i *)0, bu_optarg, rt_do_tab);
		break;
	    case 'd':
		densityfile = bu_optarg;
		break;
	    case 'C':
		{
		    struct bu_color color = BU_COLOR_INIT_ZERO;

		    if (!bu_color_from_str(&color, bu_optarg)) {
			bu_exit(EXIT_FAILURE, "ERROR: invalid color string: '%s'\n", bu_optarg);
		    }
		    {
			char buf[128] = {0};
			sprintf(buf, "set background=%f/%f/%f", color.buc_rgb[RED], color.buc_rgb[GRN], color.buc_rgb[BLU]);
			(void)rt_do_cmd((struct rt_i *)0, buf, rt_do_tab);
		    }
		}
		break;
	    case 'T':
		{
		    double f;
		    char *cp;
		    f = 0;
		    if (sscanf(bu_optarg, "%lf", &f) == 1) {
			if (f > 0)
			    rt_dist_tol = f;
		    }
		    f = 0;
		    if ((cp = strchr(bu_optarg, '/')) ||
			(cp = strchr(bu_optarg, COMMA))) {
			if (sscanf(cp+1, "%lf", &f) == 1) {
			    if (f > 0 && f < 1)
				rt_perp_tol = f;
			}
		    }
		    bu_log("Using tolerance %lg", f);
		    break;
		}
	    case 'U':
		use_air = atoi(bu_optarg);
		break;
	    case 'i':
		incr_mode = 1;
		break;
	    case 'S':
		stereo = 1;
		break;
	    case 'I':
		if (!bu_file_exists(bu_optarg, NULL)) {
		    bu_exit(1, "Object list file %s not found, aborting\n", bu_optarg);
		}
		if ((objsfile = fopen(bu_optarg, "r")) == NULL) {
		    bu_exit(1, "Unable to open object list file %s, aborting\n", bu_optarg);
		}
		objc = 0;
		while (bu_vls_gets(&oline, objsfile) >= 0) {
		    objc++;
		    bu_vls_trunc(&oline, 0);
		}
		fclose(objsfile);
		if (objc) {
		    objv = (char **)bu_calloc(objc+1, sizeof(char *), "obj array");
		    objsfile = fopen(bu_optarg, "r");
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
		break;
	    case 'J':
		sscanf(bu_optarg, "%x", &jitter);
		break;
	    case 'H':
		hypersample = atoi(bu_optarg);
		if (hypersample > 0)
		    jitter = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 'D':
		desiredframe = atoi(bu_optarg);
		break;
	    case 'K':
		finalframe = atoi(bu_optarg);
		break;
	    case 'N':
		sscanf(bu_optarg, "%x", (unsigned int *)&nmg_debug);
		bu_log("NMG_debug=0x%x\n", nmg_debug);
		break;
	    case 'M':
		matflag = 1;
		break;
	    case 'A':
		AmbientIntensity = atof(bu_optarg);
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_debug);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&optical_debug);
		break;
	    case '!':
		sscanf(bu_optarg, "%x", (unsigned int *)&bu_debug);
		break;
	    case 's':
		/* Square size */
		i = atoi(bu_optarg);
		if (i < 1 || i > MAX_WIDTH)
		    fprintf(stderr, "squaresize=%d out of range\n", i);
		else
		    width = height = i;
		break;
	    case 'n':
		i = atoi(bu_optarg);
		if (i < 1 || i > MAX_WIDTH)
		    fprintf(stderr, "height=%d out of range\n", i);
		else
		    height = i;
		break;
	    case 'W':
		(void)rt_do_cmd((struct rt_i *)0, "set background=1.0/1.0/1.0", rt_do_tab);
		default_background = 0;
		break;
	    case 'w':
		i = atoi(bu_optarg);
		if (i < 1 || i > MAX_WIDTH)
		    fprintf(stderr, "width=%d out of range\n", i);
		else
		    width = i;
		break;
	    case 'g':
		cell_width = atof(bu_optarg);
		cell_newsize = 1;
		break;
	    case 'G':
		cell_height = atof(bu_optarg);
		cell_newsize = 1;
		break;

	    case 'a':
		/* Set azimuth */
		if (bn_decode_angle(&azimuth, bu_optarg) == 0) {
		    fprintf(stderr, "WARNING: Unexpected units for azimuth angle, using default value\n");
		}
		matflag = 0;
		break;
	    case 'e':
		/* Set elevation */
		if (bn_decode_angle(&elevation, bu_optarg) == 0) {
		    fprintf(stderr, "WARNING: Unexpected units for elevation angle, using default value\n");
		}
		matflag = 0;
		break;
	    case 'l':
		{
		    char *item;

		    /* Select lighting model # */
		    lightmodel= 1;	/* Initialize with Full Lighting Model */
		    item= strtok(bu_optarg, ", ");
		    lightmodel= atoi(item);

		    if (lightmodel == 7) {
			/* Process the photon mapping arguments */
			item= strtok(NULL, ", ");
			pmargs[0]= item ? atoi(item) : 16384;		/* Number of Global Photons */
			item= strtok(NULL, ", ");
			pmargs[1]= item ? atof(item) : 50;		/* Percent of Global Photons that should be used for Caustic Photons */
			item= strtok(NULL, ", ");
			pmargs[2]= item ? atoi(item) : 10;		/* Number of Irradiance Sample Rays, Total Rays is this number squared */
			item= strtok(NULL, ", ");
			pmargs[3]= item ? atof(item) : 60.0;		/* Angular Tolerance */
			item= strtok(NULL, ", ");
			pmargs[4]= item ? atoi(item) : 0;		/* Random Seed */
			item= strtok(NULL, ", ");
			pmargs[5]= item ? atoi(item) : 0;		/* Importance Mapping */
			item= strtok(NULL, ", ");
			pmargs[6]= item ? atoi(item) : 0;		/* Irradiance Hypersampling */
			item= strtok(NULL, ", ");
			pmargs[7]= item ? atoi(item) : 0;		/* Visualize Irradiance */
			item= strtok(NULL, ", ");
			pmargs[8]= item ? atof(item) : 1.0;		/* Scale Lumens */
			item= strtok(NULL, ", ");
			if (item) {
			    bu_strlcpy(pmfile, item, sizeof(pmfile));
			} else {
			    pmfile[0]= 0;
			}
		    }
		}
		break;
	    case 'O':
		/* Output pixel file name, double precision format */
		outputfile = bu_optarg;
		doubles_out = 1;
		break;
	    case 'o':
		/* Output pixel file name, unsigned char format */
		outputfile = bu_optarg;
		doubles_out = 0;
		break;
	    case 'p':
		rt_perspective = atof(bu_optarg);
		if (rt_perspective < 0 || rt_perspective > 179) {
		    fprintf(stderr, "persp=%g out of range\n", rt_perspective);
		    rt_perspective = 0;
		}
		break;
	    case 'u':
		if (BU_STR_EQUAL(bu_optarg, "model")) {
		    model_units = 1;
		    default_units = 0;
		} else {
		    units = bu_units_conversion(bu_optarg);
		    if (units <= 0.0) {
			units = 1.0;
			default_units = 1;
			bu_log("WARNING: bad units, using default (%s)\n", bu_units_string(units));
		    } else {
			default_units = 0;
		    }
		}
		break;
	    case 'v': {
		/* change our "verbosity" level, increasing the amount
		 * of (typically non-debug) information printed during
		 * ray tracing for each -v specified or to the
		 * specific setting if an optional hexadecimal value
		 * is provided as an argument.
		 */

		int ret;
		unsigned int scanned_verbosity = 0;
		size_t num_v = 1;
		const char *cp = bu_optarg;

		if (!bu_optarg || *bu_optarg == '\0') {
		    /* turn on next 2 bits */
		    rt_verbosity = increase_level(rt_verbosity);
		} else {

		    while (*cp == 'v') {
			num_v++;
			cp++;
		    }

		    while (num_v--)
			rt_verbosity = increase_level(rt_verbosity);

		    /* we have a hex, set it specifically */
		    if (*cp != '\0' && isxdigit((int)*cp)) {
			ret = sscanf(cp, "%x", (unsigned int *)&scanned_verbosity);
			if (ret == 1) {
			    rt_verbosity = scanned_verbosity;
			}
		    }
		}

		bu_printb("Verbosity:", rt_verbosity, VERBOSE_FORMAT);
		bu_log("\n");
		break;
	    }
	    case 'E':
		eye_backoff = atof(bu_optarg);
		break;

	    case 'P':
		{
		    /* Number of parallel workers */
		    size_t avail_cpus;

		    avail_cpus = bu_avail_cpus();

		    npsw = atoi(bu_optarg);

		    if (npsw > avail_cpus) {
			fprintf(stderr, "Requesting %lu cpus, only %lu available.",
				(unsigned long)npsw, (unsigned long)avail_cpus);

			if ((bu_debug & BU_DEBUG_PARALLEL) ||
			    (RT_G_DEBUG & RT_DEBUG_PARALLEL)) {
			    fprintf(stderr, "\nAllowing surplus cpus due to debug flag.\n");
			} else {
			    fprintf(stderr, "  Will use %lu.\n", (unsigned long)avail_cpus);
			    npsw = avail_cpus;
			}
		    }
		    if (npsw < 1 || npsw > MAX_PSW) {
			fprintf(stderr, "Numer of requested cpus (%lu) is out of range 1..%d", (unsigned long)npsw, MAX_PSW);

			if ((bu_debug & BU_DEBUG_PARALLEL) ||
			    (RT_G_DEBUG & RT_DEBUG_PARALLEL)) {
			    fprintf(stderr, ", but allowing due to debug flag\n");
			} else {
			    fprintf(stderr, ", using -P1\n");
			    npsw = 1;
			}
		    }
		}
		break;
	    case 'Q':
		Query_one_pixel = ! Query_one_pixel;
		sscanf(bu_optarg, "%d, %d\n", &query_x, &query_y);
		break;
	    case 'B':
		/* Remove all intentional random effects
		 * (dither, etc.) for benchmarking purposes.
		 */
		benchmark = 1;
		bn_mathtab_constant();
		break;
	    case 'b':
		/* Specify a single pixel to be done; X and Y pixel coordinates need enclosing quotes. */
		/* Actually processed in do_frame() */
		string_pix_start = bu_optarg;
		npsw = 1;	/* Cancel running in parallel */
		break;
	    case 'V':
		{
		    /* View aspect */

		    fastf_t xx, yy;
		    register char *cp = bu_optarg;

		    xx = atof(cp);
		    while ((*cp >= '0' && *cp <= '9')
			   || *cp == '.') cp++;
		    while (*cp && (*cp < '0' || *cp > '9')) cp++;
		    yy = atof(cp);
		    if (ZERO(yy))
			aspect = xx;
		    else
			aspect = xx/yy;
		    if (aspect <= 0.0) {
			fprintf(stderr, "Bogus aspect %g, using 1.0\n", aspect);
			aspect = 1.0;
		    }
		}
		break;
	    case 'r':
		/* report overlapping region names */
		rpt_overlap = 1;
		break;
	    case 'R':
		/* DON'T report overlapping region names */
		rpt_overlap = 0;
		break;
	    case 'z':
		opencl_mode = atoi(bu_optarg);
		break;
	    case '+':
		{
		    register char *cp = bu_optarg;
		    switch (*cp) {
			case 't':
			    output_is_binary = 0;
			    break;
			default:
			    bu_exit(EXIT_FAILURE, "ERROR: unknown option %c\n", *cp);
		    }
		}
		break;
	    case 'h':
		/* asking for help */
		return 0;
	    default:
		/* '?' except not -? */
		fprintf(stderr, "ERROR: argument missing or bad option specified\n");
		return -1;
	}
    }

    /* sanity checks for sane values */
    if (aspect <= 0.0) {
	aspect = 1.0;
    }

    /* Compat */
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
    /* Read from ENV with we're going to use the experimental mode */
    env_str = getenv("LIBRT_EXP_MODE");
    if (env_str != NULL && atoi(env_str) == 1) {
	full_incr_mode = 1;
	full_incr_nsamples = 10;
	bu_log("multi-sample mode\n");
    }

    return 1;			/* OK */
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
