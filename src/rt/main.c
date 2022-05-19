/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2022 United States Government as represented by
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
/** @file rt/main.c
 *
 * Ray Tracing User Interface (RTUIF) main program, using LIBRT
 * library.
 *
 * Invoked by MGED for quick pictures.
 * Is linked with each of several "back ends":
 *	view.c, viewpp.c, viewray.c, viewcheck.c, etc.
 * to produce different executable programs:
 *	rt, rtpp, rtray, rtcheck, etc.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <math.h>

#ifdef MPI_ENABLED
#  include <mpi.h>
#endif

#include "bio.h"

#include "bu/app.h"
#include "bu/endian.h"
#include "bu/getopt.h"
#include "bu/bitv.h"
#include "bu/debug.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/parallel.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bu/version.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm.h"
#include "pkg.h"

/* private */
#include "./rtuif.h"
#include "./ext.h"
#include "brlcad_ident.h"


extern void application_init(void);

extern const char title[];


/***** Variables shared with viewing model *** */
struct fb	*fbp = FB_NULL;	/* Framebuffer handle */
FILE		*outfp = NULL;		/* optional pixel output file */
struct icv_image *bif = NULL;
mat_t		view2model;
mat_t		model2view;
/***** end of sharing with viewing model *****/


/***** variables shared with worker() ******/
struct application APP;
int		report_progress;	/* !0 = user wants progress report */
extern int	incr_mode;		/* !0 for incremental resolution */
extern size_t	incr_nlevel;		/* number of levels */
extern vect_t   left_eye_delta;
/***** end variables shared with worker() *****/


/***** variables shared with do.c *****/
extern int	pix_start;		/* pixel to start at */
extern int	pix_end;		/* pixel to end at */
size_t		n_malloc;		/* Totals at last check */
size_t		n_free;
size_t		n_realloc;
extern int	matflag;		/* read matrix from stdin */
extern int	orientflag;		/* 1 means orientation has been set */
extern int	desiredframe;		/* frame to start at */
extern int	curframe;		/* current frame number,
					 * also shared with view.c */
extern char	*outputfile;		/* name of base of output file */
/***** end variables shared with do.c *****/


extern fastf_t	rt_dist_tol;		/* Value for rti_tol.dist */
extern fastf_t	rt_perp_tol;		/* Value for rti_tol.perp */
extern char	*framebuffer;		/* desired framebuffer */

extern struct command_tab rt_do_tab[];


void
siginfo_handler(int UNUSED(arg))
{
    report_progress = 1;
#ifdef SIGUSR1
    (void)signal(SIGUSR1, siginfo_handler);
#endif
#ifdef SIGINFO
    (void)signal(SIGINFO, siginfo_handler);
#endif
}


void
memory_summary(void)
{
    if (rt_verbosity & VERBOSE_STATS) {
	size_t mdelta = bu_n_malloc - n_malloc;
	size_t fdelta = bu_n_free - n_free;
	bu_log("Additional #malloc=%zu, #free=%zu, #realloc=%zu (%zu retained)\n",
	       mdelta,
	       fdelta,
	       bu_n_realloc - n_realloc,
	       mdelta - fdelta);
    }
    n_malloc = bu_n_malloc;
    n_free = bu_n_free;
    n_realloc = bu_n_realloc;
}


int fb_setup() {
    /* Framebuffer is desired */
    size_t xx, yy;
    int zoom;

    /* make sure width/height are set via -g/-G */
    grid_sync_dimensions();

    /* Ask for a fb big enough to hold the image, at least 512. */
    /* This is so MGED-invoked "postage stamps" get zoomed up big
     * enough to see.
     */
    xx = yy = 512;
    if (xx < width || yy < height) {
	xx = width;
	yy = height;
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fbp = fb_open(framebuffer, xx, yy);
    bu_semaphore_release(BU_SEM_SYSCALL);
    if (fbp == FB_NULL) {
	fprintf(stderr, "rt:  can't open frame buffer\n");
	return 12;
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    /* If fb came out smaller than requested, do less work */
    size_t fbwidth = (size_t)fb_getwidth(fbp);
    size_t fbheight = (size_t)fb_getheight(fbp);
    if (width > fbwidth)
	width = fbwidth;
    if (height > fbheight)
	height = fbheight;

    /* If fb is lots bigger (>= 2X), zoom up & center */
    if (width > 0 && height > 0) {
	zoom = fbwidth/width;
	if (fbheight/height < (size_t)zoom) {
	    zoom = fb_getheight(fbp)/height;
	}
	(void)fb_view(fbp, width/2, height/2, zoom, zoom);
    } else {
	zoom = 1;
    }
    bu_semaphore_release(BU_SEM_SYSCALL);

#ifdef USE_OPENCL
    clt_connect_fb(fbp);
#endif
    return 0;
}


int main(int argc, char *argv[])
{
    int ret = 0;
    int need_fb = 0;
    struct rt_i *rtip = NULL;
    const char *title_file = NULL, *title_obj = NULL;	/* name of file and first object */
    char idbuf[2048] = {0};			/* First ID record info */
    struct bu_vls times = BU_VLS_INIT_ZERO;
    int i;
    int objs_free_argv = 0;

#ifdef MPI_ENABLED
    int size;
    int rank;
#endif

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    bu_setlinebuf(stdout);
    bu_setlinebuf(stderr);

    azimuth = 35.0;			/* GIFT defaults */
    elevation = 25.0;

    AmbientIntensity = 0.4;
    background[0] = background[1] = 0.0;
    background[2] = 1.0/255.0; /* slightly non-black */

    /* Before option processing, get default number of processors */
    npsw = bu_avail_cpus();		/* Use all that are present */
    if (npsw > MAX_PSW) npsw = MAX_PSW;

    /* Before option processing, do application-specific initialization */
    RT_APPLICATION_INIT(&APP);
    application_init();

#ifdef MPI_ENABLED
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    fprintf(stderr, "MPI Rank: %d of %d\n", rank+1, size);
#endif

    /* Process command line options */
    i = get_args(argc, (const char **)argv);
    if (i < 0) {
	usage(argv[0], 0);
#ifdef MPI_ENABLED
	MPI_Finalize();
#endif
	return 1;
    } else if (i == 0) {
	/* asking for help is ok */
	usage(argv[0], 100);
#ifdef MPI_ENABLED
	MPI_Finalize();
#endif
	return 0;
    }

    /* Identify the versions of the libraries we are using. */
    if (rt_verbosity & VERBOSE_LIBVERSIONS) {
	fprintf(stderr, "%s%s%s%s\n",
		brlcad_ident(title),
		rt_version(),
		bn_version(),
		bu_version()
	    );
    }
#if defined(DEBUG)
    fprintf(stderr, "Compile-time debug symbols are available\n");
#endif
#if defined(NO_BOMBING_MACROS) || defined(NO_MAGIC_CHECKING) || defined(NO_DEBUG_CHECKING)
    fprintf(stderr, "WARNING: Run-time debugging is disabled and may enhance performance\n");
#endif

    /* Identify what host we're running on */
    if (rt_verbosity & VERBOSE_LIBVERSIONS) {
	char hostname[512] = {0};
	if (bu_gethostname(hostname, sizeof(hostname)) >= 0)
	    fprintf(stderr, "Running on %s\n", hostname);
    }

    if (bu_optind >= argc) {
	fprintf(stderr, "%s:  BRL-CAD geometry database not specified\n", argv[0]);
	usage(argv[0], 0);
#ifdef MPI_ENABLED
	MPI_Finalize();
#endif
	return 1;
    }

    if (rpt_overlap)
	APP.a_logoverlap = ((void (*)(struct application *, const struct partition *, const struct bu_ptbl *, const struct partition *))0);
    else
	APP.a_logoverlap = rt_silent_logoverlap;

    /* If user gave no sizing info at all, use 512 as default */
    if (width <= 0 && cell_width <= 0)
	width = 512;
    if (height <= 0 && cell_height <= 0)
	height = 512;

    /* If user didn't provide an aspect ratio, use the image
     * dimensions ratio as a default.
     */
    if (aspect <= 0.0) {
	aspect = (fastf_t)width / (fastf_t)height;
    }

    if (sub_grid_mode) {
	/* check that we have a legal subgrid */
	if ((size_t)sub_xmax >= width || (size_t)sub_ymax >= height) {
	    fprintf(stderr, "rt: illegal values for subgrid %d, %d, %d, %d\n",
		    sub_xmin, sub_ymin, sub_xmax, sub_ymax);
	    fprintf(stderr, "\tFor a %lu X %lu image, the subgrid must be within 0, 0, %lu, %lu\n",
		    (unsigned long)width, (unsigned long)height, (unsigned long)width-1, (unsigned long)height-1);
#ifdef MPI_ENABLED
	    MPI_Finalize();
#endif
	    return 1;
	}
    }

    if (incr_mode) {
	size_t x = height;
	if (x < width) x = width;
	incr_nlevel = 1;
	while ((size_t)(1ULL << incr_nlevel) < x)
	    incr_nlevel++;
	height = width = 1ULL << incr_nlevel;
	if (rt_verbosity & VERBOSE_INCREMENTAL)
	    fprintf(stderr,
		    "incremental resolution, nlevels = %lu, width=%lu\n",
		    (unsigned long)incr_nlevel, (unsigned long)width);
    }

    /*
     * Handle parallel initialization, if applicable.
     */
#ifndef PARALLEL
    npsw = 1;			/* force serial */
#endif

    /* parsing the user value needs separation from the set value,
     * probably handled better during global elimination. --CSM */
/*     if (npsw < 0) { */
/* 	/\* Negative number means "all but" npsw *\/ */
/* 	npsw = bu_avail_cpus() + npsw; */
/*     } */


    /* allow debug builds to go higher than the max */
    if (!(bu_debug & BU_DEBUG_PARALLEL)) {
	if (npsw > MAX_PSW) {
	    npsw = MAX_PSW;
	}
    }

    if (npsw > 1) {
	RTG.rtg_parallel = 1;
	if (rt_verbosity & VERBOSE_MULTICPU)
	    fprintf(stderr, "Planning to run with %lu processors\n", (unsigned long)npsw);
    } else {
	RTG.rtg_parallel = 0;
    }

    /*
     * Do not use bu_log() or bu_malloc() before this point!
     */

    if (bu_debug) {
	bu_printb("libbu bu_debug", bu_debug, BU_DEBUG_FORMAT);
	bu_log("\n");
    }

    if (RT_G_DEBUG) {
	bu_printb("librt rt_debug", rt_debug, RT_DEBUG_FORMAT);
	bu_log("\n");
    }
    if (optical_debug) {
	bu_printb("rt optical_debug", optical_debug, OPTICAL_DEBUG_FORMAT);
	bu_log("\n");
    }

    title_file = argv[bu_optind];
    title_obj = argv[bu_optind+1];
    if (!objv) {
	objc = argc - bu_optind - 1;
	if (objc) {
	    objv = (char **)&(argv[bu_optind+1]);
	} else {
	    /* No objects in either input file or argv - try getting
	     * objs from command processing.  Initialize the table.
	     */
	    BU_GET(cmd_objs, struct bu_ptbl);
	    bu_ptbl_init(cmd_objs, 8, "initialize cmdobjs table");
	}
    } else {
	objs_free_argv = 1;
    }

#ifdef USE_OPENCL
    if (opencl_mode) {
	struct bu_vls str = BU_VLS_INIT_ZERO;

	bu_vls_strcat(&str, "\ncompiling OpenCL programs... ");
	bu_log("%s\n", bu_vls_addr(&str));
	bu_vls_free(&str);

	rt_prep_timer();

	clt_init();

	(void)rt_get_timer(&times, NULL);
	if (rt_verbosity & VERBOSE_STATS)
	    bu_log("OCLINIT: %s\n", bu_vls_addr(&times));
	bu_vls_free(&times);
    }
#endif

    /* Echo back the command line arguments as given, in 3 Tcl
     * commands.
     */
    if (rt_verbosity & VERBOSE_MODELTITLE) {
	struct bu_vls str = BU_VLS_INIT_ZERO;

	bu_vls_from_argv(&str, bu_optind, (const char **)argv);
	bu_vls_strcat(&str, "\n");
	bu_vls_printf(&str, "opendb %s;\n", title_file);

	if (objc) {
	    bu_vls_strcat(&str, "tree ");

	    /* arbitrarily limit number of command-line objects being
	     * echo'd back for log printing, followed by ellipses.
	     */
	    bu_vls_from_argv(&str,
			     objc <= 16 ? objc : 16,
			     (const char **)argv+bu_optind+1);
	    if (objc > 16)
		bu_vls_strcat(&str, " ...");
	    else
		bu_vls_putc(&str, ';');
	}

	bu_log("%s\n", bu_vls_addr(&str));
	bu_vls_free(&str);
    }

    /* Build directory of GED database */
    rt_prep_timer();
    if ((rtip = rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL) {
	bu_log("rt:  rt_dirbuild(%s) failure\n", title_file);
#ifdef MPI_ENABLED
	MPI_Finalize();
#endif
	return 2;
    }
    APP.a_rt_i = rtip;
    (void)rt_get_timer(&times, NULL);
    if (rt_verbosity & VERBOSE_MODELTITLE)
	bu_log("db title:  %s\n", idbuf);
    if (rt_verbosity & VERBOSE_STATS)
	bu_log("DIRBUILD: %s\n", bu_vls_addr(&times));
    bu_vls_free(&times);
    memory_summary();

    /* Copy values from command line options into rtip */
    APP.a_rt_i->rti_space_partition = space_partition;
    APP.a_rt_i->useair = use_air;
    APP.a_rt_i->rti_save_overlaps = save_overlaps;
    if (rt_dist_tol > 0) {
	APP.a_rt_i->rti_tol.dist = rt_dist_tol;
	APP.a_rt_i->rti_tol.dist_sq = rt_dist_tol * rt_dist_tol;
    }
    if (rt_perp_tol > 0) {
	APP.a_rt_i->rti_tol.perp = rt_perp_tol;
	APP.a_rt_i->rti_tol.para = 1 - rt_perp_tol;
    }
    if (rt_verbosity & VERBOSE_TOLERANCE)
	rt_pr_tol(&APP.a_rt_i->rti_tol);

    /* before view_init */
    if (outputfile && BU_STR_EQUAL(outputfile, "-"))
	outputfile = (char *)0;

    /*
     * Initialize all the per-CPU memory resources.
     * The number of processors can change at runtime, init them all.
     */
    memset(resource, 0, sizeof(resource));
    for (i = 0; i < MAX_PSW; i++) {
	rt_init_resource(&resource[i], i, APP.a_rt_i);
    }
    memory_summary();

#ifdef SIGUSR1
    (void)signal(SIGUSR1, siginfo_handler);
#endif
#ifdef SIGINFO
    (void)signal(SIGINFO, siginfo_handler);
#endif

    /* First, see if we're handling old style processing of the -M flag. */
    if (matflag && !isatty(fileno(stdin))) {
	int oret = old_way(stdin);
	if (oret < 0) {
	    bu_log("%s: no objects specified -- raytrace aborted\n", argv[0]);
	    usage(argv[0], 0);
	    ret = 1;
	}
	/* If oret, old way either worked or failed.  Either way, we're done. */
	if (oret) {
	    goto rt_cleanup;
	}
    }

    if (objv && !matflag) {
	int frame_retval;

	def_tree(APP.a_rt_i);		/* Load the default trees */

	/*
	 * Initialize application.
	 * Note that width & height may not have been set yet,
	 * since they may change from frame to frame.
	 */
	need_fb = view_init(&APP, (char *)title_file, (char *)title_obj, outputfile != (char *)0, framebuffer != (char *)0);
	if ((outputfile == (char *)0) && !need_fb) {
	    /* If not going to framebuffer, or to a file, then use stdout */
	    if (outfp == NULL) outfp = stdout;
	    /* output_is_binary is changed by view_init, as appropriate */
	    if (output_is_binary && isatty(fileno(outfp))) {
		fprintf(stderr, "rt:  attempting to send binary output to terminal, aborting\n");
#ifdef MPI_ENABLED
		MPI_Finalize();
#endif
		return 14;
	    }
	}

	/* orientation command has not been used */
	if (!orientflag)
	    do_ae(azimuth, elevation);

	if (need_fb != 0 && !fbp) {
	    int fb_status = fb_setup();
	    if (fb_status) {
#ifdef MPI_ENABLED
		MPI_Finalize();
#endif
		return fb_status;
	    }
	}

	frame_retval = do_frame(curframe);
	if (frame_retval != 0) {
	    /* Release the framebuffer, if any */
	    if (fbp != FB_NULL) {
		fb_close(fbp);
	    }
	    ret = 1;
	    goto rt_cleanup;
	}
    } else {
	register char *buf;
	register int nret;

	/*
	 * Initialize application.
	 * Note that width & height may not have been set yet,
	 * since they may change from frame to frame.
	 */
	need_fb = view_init(&APP, (char *)title_file, (char *)title_obj, outputfile != (char *)0, framebuffer != (char *)0);
	if ((outputfile == (char *)0) && !need_fb) {
	    /* If not going to framebuffer, or to a file, then use stdout */
	    if (outfp == NULL) outfp = stdout;
	    /* output_is_binary is changed by view_init, as appropriate */
	    if (output_is_binary && isatty(fileno(outfp))) {
		fprintf(stderr, "rt:  attempting to send binary output to terminal, aborting\n");
#ifdef MPI_ENABLED
		MPI_Finalize();
#endif
		return 14;
	    }
	}

	/*
	 * New way - command driven.
	 * Process sequence of input commands.
	 * All the work happens in the functions
	 * called by rt_do_cmd().
	 */

	if (!matflag && isatty(fileno(stdin))) {
	    fprintf(stderr, "Additional commands needed - cannot complete raytrace\n");
	    ret = 1;
	    goto rt_cleanup;
	}

	while ((buf = rt_read_cmd(stdin)) != (char *)0) {
	    if (OPTICAL_DEBUG&OPTICAL_DEBUG_PARSE) {
		fprintf(stderr, "cmd: %s\n", buf);
	    }

	    /* Right now, the framebuffer setup blocks processing of
	     * stdin.  Consequently when rt is being run as a
	     * subprocess by libged the stdin pipe can get full on
	     * some platforms, which in turn results in everything
	     * hanging while the calling program waits for rt to
	     * process stdin before continuing to write down the pipe
	     * and rt waits for fbserv info along the libpkg channel
	     * before getting to the logic for processing stdin.
	     * Postpone fb setup until we're ready to render something
	     * to avoid backing up stdin's pipe.
	     */
	    if (!bu_strncmp(buf, "end", sizeof("end")) || !bu_strncmp(buf, "multiview", sizeof("multiview"))) {
		if (need_fb != 0 && !fbp) {
		    int fb_status = fb_setup();
		    if (fb_status) {
#ifdef MPI_ENABLED
			MPI_Finalize();
#endif
			return fb_status;
		    }
		}
	    }

	    nret = rt_do_cmd(APP.a_rt_i, buf, rt_do_tab);
	    bu_free(buf, "rt_read_cmd command buffer");
	    if (nret < 0)
		break;
	}
	if (curframe < desiredframe) {
	    fprintf(stderr,
		    "rt:  Desired frame %d not reached, last was %d\n",
		    desiredframe, curframe);
	}
    }

    /* Release the framebuffer, if any */
    if (fbp != FB_NULL) {
	fb_close(fbp);
    }

rt_cleanup:
    /* Clean up objv memory, if necessary */
    if (objs_free_argv) {
	for (i = 0; i < objc; i++) {
	    bu_free(objv[i], "objv entry");
	}
	bu_free(objv, "objv array");
    }

    if (cmd_objs) {
	for (i = 0; i < (int)BU_PTBL_LEN(cmd_objs); i++) {
	    char *ostr = (char *)BU_PTBL_GET(cmd_objs, i);
	    bu_free(ostr, "object string");
	}
	bu_ptbl_free(cmd_objs);
	BU_PUT(cmd_objs, struct bu_ptbl);
    }

    /* Release the ray-tracer instance */
    rt_free_rti(APP.a_rt_i);
    APP.a_rt_i = NULL;

#ifdef MPI_ENABLED
    MPI_Finalize();
#endif

    return ret;
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
