/*                            D O . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2024 United States Government as represented by
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
/** @file rt/do.c
 *
 * Routines that process the various commands, and manage the overall
 * process of running the raytracing process.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "bio.h"

#include "bu/app.h"
#include "bu/debug.h"
#include "bu/getopt.h"
#include "bu/mime.h"
#include "bu/vls.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm.h"
#include "icv.h"

#include "./rtuif.h"
#include "./ext.h"

#if defined(HAVE_FDOPEN) && !defined(HAVE_DECL_FDOPEN)
extern FILE *fdopen(int fd, const char *mode);
#endif

/***** Variables shared with viewing model *** */
extern FILE *outfp;			/* optional pixel output file */
extern mat_t view2model;
extern mat_t model2view;
/***** end of sharing with viewing model *****/

/***** variables shared with opt.c *****/
extern int	orientflag;		/* 1 means orientation has been set */
/***** end variables shared with opt.c *****/

/***** variables shared with rt.c *****/
extern char *string_pix_start;	/* string spec of starting pixel */
extern char *string_pix_end;	/* string spec of ending pixel */
extern int finalframe;		/* frame to halt at */
/***** end variables shared with rt.c *****/

void def_tree(register struct rt_i *rtip);
void do_ae(double azim, double elev);
void res_pr(void);
void memory_summary(void);
extern void worker(int cpu, void *arg);

extern struct icv_image *bif;
unsigned char *pixmap = NULL; /**< Pixel Map for rerendering of black pixels */


/**
 * Acquire particulars about a frame, in the old format.  Returns -1
 * if unable to acquire info, 0 if successful.
 */
int
old_frame(FILE *fp)
{
    register int i;
#define NUMBER_LEN 128
    char number[NUMBER_LEN+1];

    /* Visible part is from -1 to +1 in view space */
    if (fscanf(fp, CPP_SCAN(NUMBER_LEN), number) != 1)
	return -1;
    viewsize = atof(number);
    if (fscanf(fp, CPP_SCAN(NUMBER_LEN), number) != 1)
	return -1;
    eye_model[X] = atof(number);
    if (fscanf(fp, CPP_SCAN(NUMBER_LEN), number) != 1)
	return -1;
    eye_model[Y] = atof(number);
    if (fscanf(fp, CPP_SCAN(NUMBER_LEN), number) != 1)
	return -1;
    eye_model[Z] = atof(number);
    for (i = 0; i < 16; i++) {
	if (fscanf(fp, CPP_SCAN(NUMBER_LEN), number) != 1)
	    return -1;
	Viewrotscale[i] = atof(number);
    }
    return 0;		/* OK */
}


/**
 * Determine if input file is old or new format, and if old format,
 * handle process.  Returns 0 if new way, 1 if old way (and all done).
 * Note that the rewind() will fail on ttys, pipes, and sockets
 * (sigh).
 */
int
old_way(FILE *fp)
{
    int c;

    viewsize = -42.0;

    /* Sneak a peek at the first character, and then put it back */
    if ((c = fgetc(fp)) == EOF) {
	/* Claim old way, all (i.e., nothing) done */
	return 1;
    }
    if (ungetc(c, fp) != c)
	bu_exit(EXIT_FAILURE, "do.c:old_way() ungetc failure\n");

    /*
     * Old format files start immediately with a %.9e format, so the
     * very first character should be a digit or '-'.
     */
    if ((c < '0' || c > '9') && c != '-') {
	return 0;		/* Not old way */
    }

    if (old_frame(fp) < 0 || viewsize <= 0.0) {
	rewind(fp);
	return 0;		/* Not old way */
    }
    bu_log("Interpreting command stream in old format\n");

    /* Committing to old way - better have objv ready */
    if (!objv) {
	return -1;
    }

    def_tree(APP.a_rt_i);	/* Load the default trees */

    curframe = 0;
    do {
	if (finalframe >= 0 && curframe > finalframe)
	    return 1;
	if (curframe >= desiredframe)
	    do_frame(curframe);
	curframe++;
    }  while (old_frame(fp) >= 0 && viewsize > 0.0);
    return 1;			/* old way, all done */
}


/**
 * Process "start" command in new format input stream
 */
int cm_start(const int argc, const char **argv)
{
    char *buf = (char *)NULL;
    int frame;

    if (argc < 2)
	return 1;

    frame = atoi(argv[1]);
    if (finalframe >= 0 && frame > finalframe)
	return -1;	/* Indicate EOF -- user declared a halt */
    if (frame >= desiredframe) {
	curframe = frame;
	return 0;
    }

    /* Skip over unwanted frames -- find next frame start */
    while ((buf = rt_read_cmd(stdin)) != (char *)0) {
	register char *cp;

	cp = buf;
	while (*cp && isspace((int)*cp)) cp++;	/* skip spaces */
	if (bu_strncmp(cp, "start", 5) != 0) {
	    bu_free(buf, "cm_start buf");
	    continue;
	}
	while (*cp && !isspace((int)*cp)) cp++;	/* skip keyword */
	while (*cp && isspace((int)*cp)) cp++;	/* skip spaces */
	frame = atoi(cp);
	bu_free(buf, "rt_read_cmd command buffer (skipping frames)");
	buf = (char *)0;
	if (finalframe >= 0 && frame > finalframe) {
	    return -1;			/* "EOF" */
	}
	if (frame >= desiredframe) {
	    curframe = frame;
	    return 0;
	}
    }
    return -1;		/* EOF */
}


int cm_vsize(const int argc, const char **argv)
{
    if (argc < 2)
	return 1;

    viewsize = atof(argv[1]);
    return 0;
}


int cm_eyept(const int argc, const char **argv)
{
    register int i;

    if (argc < 2)
	return 1;

    for (i = 0; i < 3; i++)
	eye_model[i] = atof(argv[i+1]);
    return 0;
}


int cm_lookat_pt(const int argc, const char **argv)
{
    point_t pt;
    vect_t dir;
    int yflip = 0;
    quat_t quat;

    if (argc < 4)
	return -1;
    pt[X] = atof(argv[1]);
    pt[Y] = atof(argv[2]);
    pt[Z] = atof(argv[3]);
    if (argc > 4)
	yflip = atoi(argv[4]);

    /*
     * eye_pt should have been specified before here and different
     * from the lookat point or the lookat point will be from the
     * "front"
     */
    if (VNEAR_EQUAL(pt, eye_model, VDIVIDE_TOL)) {
	VSETALLN(quat, 0.5, 4);
	quat_quat2mat(Viewrotscale, quat); /* front */
    } else {
	VSUB2(dir, pt, eye_model);
	VUNITIZE(dir);
	bn_mat_lookat(Viewrotscale, dir, yflip);
    }

    return 0;
}


int cm_vrot(const int argc, const char **argv)
{
    register int i;

    if (argc < 16)
	return 1;

    for (i = 0; i < 16; i++)
	Viewrotscale[i] = atof(argv[i+1]);
    return 0;
}


int cm_orientation(const int argc, const char **argv)
{
    register int i;
    quat_t quat;

    if (argc < 4)
	return 1;

    for (i = 0; i < 4; i++)
	quat[i] = atof(argv[i+1]);
    quat_quat2mat(Viewrotscale, quat);
    orientflag = 1;
    return 0;
}


int cm_end(const int UNUSED(argc), const char **UNUSED(argv))
{
    struct rt_i *rtip = APP.a_rt_i;

    if (rtip && BU_LIST_IS_EMPTY(&rtip->HeadRegion)) {
	def_tree(rtip);		/* Load the default trees */
    }

    /* If no matrix or az/el specified yet, use params from cmd line */
    if (Viewrotscale[15] <= 0.0)
	do_ae(azimuth, elevation);

    if (do_frame(curframe) < 0)
	return -1;
    return 0;
}




int cm_draw(const int argc, const char **argv)
{
    int i = 0;
    size_t j = 0;
    if (!cmd_objs || argc < 2) {
	return 0;
    }
    for (i = 1; i < argc; i++) {
	int is_drawn = 0;
	for (j = 0; j < BU_PTBL_LEN(cmd_objs); j++) {
	    const char *dobj = (const char *)BU_PTBL_GET(cmd_objs, j);
	    if (BU_STR_EQUAL(dobj, argv[1])) {
		is_drawn = 1;
		break;
	    }
	}
	if (!is_drawn) {
	    const char *ndraw = bu_strdup(argv[1]);
	    bu_ptbl_ins(cmd_objs, (long *)ndraw);
	}
    }
    return 0;
}

int cm_erase(const int argc, const char **argv)
{
    int i = 0;
    size_t j = 0;
    if (!cmd_objs) {
	return 0;
    }
    for (i = 1; i < argc; i++) {
	for (j = 0; j < BU_PTBL_LEN(cmd_objs); j++) {
	    char *dobj = (char *)BU_PTBL_GET(cmd_objs, j);
	    if (BU_STR_EQUAL(dobj, argv[1])) {
		bu_ptbl_rm(cmd_objs, (long *)dobj);
		bu_free(dobj, "free name");
	    }
	}
    }
    return 0;
}


int cm_prep(const int UNUSED(argc), const char **UNUSED(argv))
{
    register struct rt_i *rtip = APP.a_rt_i;
    struct bu_vls times = BU_VLS_INIT_ZERO;
    int objcnt = 0;
    const char **objargv = NULL;
    if (!cmd_objs) {
	return 0;
    }
    objcnt = (int)BU_PTBL_LEN(cmd_objs);
    if (objcnt <= 0) {
	return 0;
    }
    objargv = (const char **)cmd_objs->buffer;

    rt_prep_timer();
    if (rt_gettrees(rtip, objcnt, objargv, (size_t)npsw) < 0)
	bu_log("rt_gettrees() FAILED\n");
    (void)rt_get_timer(&times, NULL);

    if (rt_verbosity & VERBOSE_STATS)
	bu_log("GETTREE: %s\n", bu_vls_addr(&times));
    bu_vls_free(&times);
    return 0;
}

int cm_tree(const int argc, const char **argv)
{
    int i = 0;
    size_t j = 0;

    if (argc <= 1) {
	return 0;
    }

    for (j = 0; j < BU_PTBL_LEN(cmd_objs); i++) {
	char *dobj = (char *)BU_PTBL_GET(cmd_objs, i);
	bu_free(dobj, "free object name");
    }
    bu_ptbl_reset(cmd_objs);
    for (i = 1; i < argc; i++) {
	const char *ndraw = bu_strdup(argv[i]);
	bu_ptbl_ins(cmd_objs, (long *)ndraw);
    }

    return cm_prep(0,NULL);
}

int cm_multiview(const int UNUSED(argc), const char **UNUSED(argv))
{
    register struct rt_i *rtip = APP.a_rt_i;
    size_t i;
    static int a[] = {
	35,   0,
	0,  90, 135, 180, 225, 270, 315,
	0,  90, 135, 180, 225, 270, 315
    };
    static int e[] = {
	25, 90,
	30, 30, 30, 30, 30, 30, 30,
	60, 60, 60, 60, 60, 60, 60
    };

    if (rtip && BU_LIST_IS_EMPTY(&rtip->HeadRegion)) {
	def_tree(rtip);		/* Load the default trees */
    }
    for (i = 0; i < (sizeof(a)/sizeof(a[0])); i++) {
	do_ae((double)a[i], (double)e[i]);
	(void)do_frame(curframe++);
    }
    return -1;	/* end RT by returning an error */
}


/**
 * Experimental animation code
 *
 * Usage: anim path type args
 */
int cm_anim(const int argc, const char **argv)
{

    if (db_parse_anim(APP.a_rt_i->rti_dbip, argc, argv) < 0) {
	bu_log("cm_anim:  %s %s failed\n", argv[1], argv[2]);
	return -1;		/* BAD */
    }
    return 0;
}


/**
 * Clean out results of last rt_prep(), and start anew.
 */
int cm_clean(const int UNUSED(argc), const char **UNUSED(argv))
{
    /* Allow lighting model clean up (e.g. lights, materials, etc.) */
    view_cleanup(APP.a_rt_i);

    rt_clean(APP.a_rt_i);

    if (OPTICAL_DEBUG&OPTICAL_DEBUG_RTMEM_END)
	bu_prmem("After cm_clean");
    return 0;
}


/**
 * To be invoked after a "clean" command, to close out the ".g"
 * database.  Intended for memory debugging, to help chase down memory
 * "leaks".  This terminates the program, as there is no longer a
 * database.
 */
int cm_closedb(const int UNUSED(argc), const char **UNUSED(argv))
{
    db_close(APP.a_rt_i->rti_dbip);
    APP.a_rt_i->rti_dbip = DBI_NULL;

    bu_free((void *)APP.a_rt_i, "struct rt_i");
    APP.a_rt_i = RTI_NULL;

    bu_prmem("After _closedb");
    bu_exit(0, NULL);

    return 1;	/* for compiler */
}


/* viewing module specific variables */
extern struct bu_structparse view_parse[];

struct bu_structparse set_parse[] = {
    {"%d",	1, "width",			bu_byteoffset(width),			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "height",			bu_byteoffset(height),			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "save_overlaps",		bu_byteoffset(save_overlaps),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1, "perspective",		bu_byteoffset(rt_perspective),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1, "angle",			bu_byteoffset(rt_perspective),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
#if !defined(_WIN32) || defined(__CYGWIN__)
    /* FIXME: these cannot be listed in here because they are LIBRT
     * globals.  due to the way symbols are not imported until a DLL
     * is loaded on Windows, the byteoffset address of the global is
     * not known at compile-time.  they would needed to be added to
     * set_parse() during runtime initialization.
     */
    {"%d",	1, "rt_bot_minpieces",		bu_byteoffset(rt_bot_minpieces),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "rt_bot_tri_per_piece",	bu_byteoffset(rt_bot_tri_per_piece),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",	1, "rt_cline_radius",		bu_byteoffset(rt_cline_radius),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
#endif
    /* daisy-chain to additional app-specific parameters */
    {"%p",	1, "Application-Specific Parameters", bu_byteoffset(view_parse[0]),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0,		0,						BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/**
 * Allow variable values to be set or examined.
 */
int cm_set(const int argc, const char **argv)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (argc <= 1) {
	bu_struct_print("Generic and Application-Specific Parameter Values",
			set_parse, (char *)0);
	return 0;
    }

    bu_vls_from_argv(&str, argc-1, (const char **)argv+1);
    if (bu_struct_parse(&str, set_parse, (char *)0, NULL) < 0) {
	bu_vls_free(&str);
	return -1;
    }
    bu_vls_free(&str);
    return 0;
}


int cm_ae(const int argc, const char **argv)
{
    if (argc < 3)
	return 1;

    azimuth = atof(argv[1]);	/* set elevation and azimuth */
    elevation = atof(argv[2]);
    do_ae(azimuth, elevation);

    return 0;
}


int cm_opt(const int argc, const char **argv)
{
    int old_bu_optind=bu_optind;	/* need to restore this value after calling get_args() */

    if (get_args(argc, argv) <= 0) {
	bu_optind = old_bu_optind;
	return -1;
    }
    bu_optind = old_bu_optind;
    return 0;
}


/**
 * Load default tree list, from command line.
 */
void
def_tree(register struct rt_i *rtip)
{
    struct bu_vls times = BU_VLS_INIT_ZERO;

    RT_CK_RTI(rtip);

    rt_prep_timer();
    if (rt_gettrees(rtip, objc, (const char **)objv, (size_t)npsw) < 0) {
	bu_log("rt_gettrees(%s) FAILED\n", (objv && objv[0]) ? objv[0] : "ERROR");
    }
    (void)rt_get_timer(&times, NULL);

    if (rt_verbosity & VERBOSE_STATS)
	bu_log("GETTREE: %s\n", bu_vls_addr(&times));
    bu_vls_free(&times);
    memory_summary();
}



/*********************************************************************************/
#ifdef USE_OPENCL
/* from opt.c */
extern double haze[3];
extern double airdensity;


static unsigned int clt_mode;           /* Active render buffers */
static uint8_t clt_o[2];		/* Sub buffer offsets in bytes: {CLT_COLOR, MAX} */

static struct fb *clt_fbp = FB_NULL;


void
clt_connect_fb(struct fb *fbp)
{
    clt_fbp = fbp;
}

void
clt_view_init(unsigned int mode)
{
    uint8_t o[2];
    int i;

    clt_mode = mode;

    o[0] = (mode & CLT_COLOR) ? 3 : 0;	/* uchar rgb[3] */
    o[1] = 0;

    clt_o[0] = 0;
    for (i=1; i<2; i++) {
	clt_o[i] = o[i-1] + clt_o[i-1];
    }
}

void
clt_run(int cur_pixel, int last_pixel)
{
    int ibackground[3];      /* integer 0..255 version */
    int inonbackground[3];   /* integer non-background */
    const double gamma_corr = 0.0;

    int npix, a_x, a_y, i;
    uint8_t *pixels, *pixelp;
    size_t cpu = 0;
    struct application a;

    ssize_t size;
    ssize_t count;

    npix = last_pixel-cur_pixel+1;
    size = npix * clt_o[1];

    a_y = (int)(cur_pixel/width);
    a_x = (int)(cur_pixel - (a_y * width));

    /* Obtain fresh copy of global application struct */
    a = APP;
    a.a_resource = &resource[cpu];
    a.a_level = 0;


    if (lightmodel == 2)
        VSETALL(background, 0);

    /* Create integer version of background color */
    inonbackground[0] = ibackground[0] = background[0] * 255.0 + 0.5;
    inonbackground[1] = ibackground[1] = background[1] * 255.0 + 0.5;
    inonbackground[2] = ibackground[2] = background[2] * 255.0 + 0.5;

    /*
     * If a non-background pixel comes out the same color as the
     * background, modify it slightly, to permit compositing.  Perturb
     * the background color channel with the largest intensity.
     */
    if (inonbackground[0] > inonbackground[1]) {
        if (inonbackground[0] > inonbackground[2]) i = 0;
        else i = 2;
    } else {
        if (inonbackground[1] > inonbackground[2]) i = 1;
        else i = 2;
    }
    if (inonbackground[i] < 127) inonbackground[i]++;
    else inonbackground[i]--;


    pixels = (uint8_t*)bu_calloc(size, sizeof(uint8_t), "image buffer");

    clt_frame(pixels, clt_o, cur_pixel, last_pixel, width,
              ibackground, inonbackground,
	      airdensity, haze, gamma_corr, view2model, cell_width,
              cell_height, aspect, lightmodel, APP.a_no_booleans);

    pixelp = pixels + cur_pixel*clt_o[1];

    if (clt_fbp != FB_NULL) {
        bu_semaphore_acquire(BU_SEM_SYSCALL);
        count = fb_write(clt_fbp, a_x, a_y, pixelp, npix);
        bu_semaphore_release(BU_SEM_SYSCALL);
        if (count < npix)
            bu_exit(EXIT_FAILURE, "pixel fb_write error");
    }
    if (outfp) {
        bu_semaphore_acquire(BU_SEM_SYSCALL);
        if (bu_fseek(outfp, cur_pixel*clt_o[1], 0) != 0)
            fprintf(stderr, "fseek error\n");
        if (fwrite(pixelp, size, 1, outfp) != 1)
            bu_exit(EXIT_FAILURE, "pixel fwrite error");
        bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (bif) {
        int span = width*clt_o[1];

        BU_ASSERT(a_x == 0);
        while (pixelp < pixels+size) {
            icv_writeline(bif, a_y++, pixelp, ICV_DATA_UCHAR);
            pixelp += span;
        }
    }
    bu_free(pixels, "image buffer");

    bu_log("sub_grid_mode: %d, fullfloat_mode: %d, hypersample: %d, jitter: %d\n"
	   "prism: %d, perspective: %e, stereo: %d\n",
	   sub_grid_mode, fullfloat_mode, hypersample, jitter & JITTER_CELL,
	   a.a_rt_i->rti_prismtrace, rt_perspective, stereo);

    /* Tally up the statistics */
    for (cpu = 0; cpu < MAX_PSW; cpu++) {
	if (resource[cpu].re_magic != RESOURCE_MAGIC) {
	    bu_log("ERROR: CPU %d resources corrupted, statistics bad\n", cpu);
	    continue;
	}
	rt_add_res_stats(APP.a_rt_i, &resource[cpu]);
    }
    bu_log("SHOT: opencl\n");
}
#endif


/**
 * This is a separate function primarily as a service to REMRT.
 */
void
do_prep(struct rt_i *rtip)
{
    struct bu_vls times = BU_VLS_INIT_ZERO;

    RT_CHECK_RTI(rtip);
    if (rtip->needprep) {
	/* Allow lighting model to set up (e.g. lights, materials, etc.) */
	view_setup(rtip);

	/* Allow RT library to prepare itself */
	rt_prep_timer();
	rt_prep_parallel(rtip, (size_t)npsw);

	(void)rt_get_timer(&times, NULL);
	if (rt_verbosity & VERBOSE_STATS)
	    bu_log("PREP: %s\n", bu_vls_addr(&times));
	bu_vls_free(&times);

#ifdef USE_OPENCL
	if (opencl_mode) {
	    rt_prep_timer();
	    clt_prep(rtip);

	    (void)rt_get_timer(&times, NULL);
	    if (rt_verbosity & VERBOSE_STATS)
		bu_log("OCLPREP: %s\n", bu_vls_addr(&times));
	    bu_vls_free(&times);
	}
#endif
    }
    memory_summary();
    if (rt_verbosity & VERBOSE_STATS) {
	bu_log("%s: %zu cut, %zu box (%zu empty)\n",
	       rtip->rti_space_partition == RT_PART_NUBSPT ?
	       "NUBSP" : "unknown",
	       rtip->rti_ncut_by_type[CUT_CUTNODE],
	       rtip->rti_ncut_by_type[CUT_BOXNODE],
	       rtip->nempty_cells);
    }
}


static int
validate_raytrace(struct rt_i *rtip)
{
    if (!rtip) {
	bu_log("ERROR: No raytracing instance.\n");
	return 1;
    }
    if (rtip->nsolids <= 0) {
	bu_log("ERROR: No primitives remaining.\n");
	return 2;
    }
    if (rtip->nregions <= 0) {
	bu_log("ERROR: No regions remaining.\n");
	return 3;
    }

    return 0;
}


/**
 * Do all the actual work to run a frame.
 *
 * Returns -1 on error, 0 if OK.
 */
int
do_frame(int framenumber)
{
    struct bu_vls times = BU_VLS_INIT_ZERO;
    char framename[128] = {0};		/* File name to hold current frame */
    struct rt_i *rtip = APP.a_rt_i;
    double utime = 0.0;			/* CPU time used */
    double nutime = 0.0;		/* CPU time used, normalized by ncpu */
    double wallclock = 0.0;		/* # seconds of wall clock time */
    vect_t work, temp;
    quat_t quat;

    if (rt_verbosity & VERBOSE_FRAMENUMBER)
	bu_log("\n...................Frame %5d...................\n",
	       framenumber);

    /* Compute model RPP, etc. */
    do_prep(rtip);

    if (rt_verbosity & VERBOSE_VIEWDETAIL)
	bu_log("Tree: %zu solids in %zu regions\n", rtip->nsolids, rtip->nregions);

    if (Query_one_pixel) {
	query_optical_debug = OPTICAL_DEBUG;
	query_debug = RT_G_DEBUG;
	rt_debug = optical_debug = 0;
    }

    if (validate_raytrace(rtip) > 0)
	return -1;

    if (rt_verbosity & VERBOSE_VIEWDETAIL)
	bu_log("Model: X(%g, %g), Y(%g, %g), Z(%g, %g)\n",
	       rtip->mdl_min[X], rtip->mdl_max[X],
	       rtip->mdl_min[Y], rtip->mdl_max[Y],
	       rtip->mdl_min[Z], rtip->mdl_max[Z]);

    /*
     * Perform Grid setup.
     * This may alter cell size or width/height.
     */
    {
	int setup;
	struct bu_vls msg = BU_VLS_INIT_ZERO;

	setup = grid_setup(&msg);
	if (setup ) {
	    bu_exit(BRLCAD_ERROR, "%s\n", bu_vls_cstr(&msg));
	}

	bu_vls_free(&msg);
    }

    /* az/el 0, 0 is when screen +Z is model +X */
    VSET(work, 0, 0, 1);
    MAT3X3VEC(temp, view2model, work);
    bn_ae_vec(&azimuth, &elevation, temp);

    if (rt_verbosity & VERBOSE_VIEWDETAIL)
	bu_log("View: %g azimuth, %g elevation off of front view\n",
	       azimuth, elevation);
    quat_mat2quat(quat, model2view);

    if (rt_verbosity & VERBOSE_VIEWDETAIL) {
	bu_log("Orientation: %g, %g, %g, %g\n", V4ARGS(quat));
	bu_log("Eye_pos: %g, %g, %g\n", V3ARGS(eye_model));
	bu_log("Size: %gmm\n", viewsize);

	/**
	 * This code shows how the model2view matrix can be
	 * reconstructed using the information from the Orientation,
	 * Eye_pos, and Size messages in the rt log output.
	 @code
	 {
	 mat_t rotscale, xlate, newmat;
	 quat_t newquat;
	 bn_mat_print("model2view", model2view);
	 quat_quat2mat(rotscale, quat);
	 rotscale[15] = 0.5 * viewsize;
	 MAT_IDN(xlate);
	 MAT_DELTAS_VEC_NEG(xlate, eye_model);
	 bn_mat_mul(newmat, rotscale, xlate);
	 bn_mat_print("reconstructed m2v", newmat);
	 quat_mat2quat(newquat, newmat);
	 HPRINT("reconstructed orientation:", newquat);
	 @endcode
	 *
	 */

	bu_log("Grid: (%g, %g) mm, (%zu, %zu) pixels\n",
	       cell_width, cell_height,
	       width, height);
	bu_log("Beam: radius=%g mm, divergence=%g mm/1mm\n",
	       APP.a_rbeam, APP.a_diverge);
    }

    /* Process -b and ??? options now, for this frame */
    if (pix_start == -1) {
	pix_start = 0;
	pix_end = (int)(height * width - 1);
    }
    if (string_pix_start) {
	int xx, yy;
	register char *cp = string_pix_start;

	xx = atoi(cp);
	while (*cp >= '0' && *cp <= '9') cp++;
	while (*cp && (*cp < '0' || *cp > '9')) cp++;
	yy = atoi(cp);
	bu_log("only pixel %d %d\n", xx, yy);
	if (xx * yy >= 0) {
	    pix_start = (int)(yy * width + xx);
	    pix_end = pix_start;
	}
    }
    if (string_pix_end) {
	int xx, yy;
	register char *cp = string_pix_end;

	xx = atoi(cp);
	while (*cp >= '0' && *cp <= '9') cp++;
	while (*cp && (*cp < '0' || *cp > '9')) cp++;
	yy = atoi(cp);
	bu_log("ending pixel %d %d\n", xx, yy);
	if (xx * yy >= 0) {
	    pix_end = (int)(yy * width + xx);
	}
    }

    /* Allocate data for pixel map for rerendering of black pixels */
    if (pixmap == NULL) {
	pixmap = (unsigned char*)bu_calloc(sizeof(RGBpixel), width*height, "pixmap allocate");
    }

    /*
     * Determine output file name
     * On UNIX only, check to see if this is a "restart".
     */
    if (outputfile != (char *)0) {
	if (framenumber <= 0) {
	    snprintf(framename, 128, "%s", outputfile);
	} else {
	    snprintf(framename, 128, "%s.%d", outputfile, framenumber);
	}

#ifdef HAVE_SYS_STAT_H
	/*
	 * This code allows the computation of a particular frame to a
	 * disk file to be resumed automatically.  This is worthwhile
	 * crash protection.  This use of stat() and bu_fseek() is
	 * UNIX-specific.
	 *
	 * It is not appropriate for the RT "top part" to assume
	 * anything about the data that the view module will be
	 * storing.  Therefore, it is the responsibility of
	 * view_2init() to also detect that some existing data is in
	 * the file, and take appropriate measures (like reading it
	 * in).
	 *
	 * view_2init() can depend on the file being open for both
	 * reading and writing, but must do its own positioning.
	 */
	{
	    int fd;
	    int ret;
	    struct stat sb;

	    if (bu_file_exists(framename, NULL)) {
		/* File exists, maybe with partial results */
		outfp = NULL;
		fd = open(framename, O_RDWR);
		if (fd < 0) {
		    perror("open");
		} else {
		    outfp = fdopen(fd, "r+");
		    if (!outfp)
			perror("fdopen");
		}

		if (fd < 0 || !outfp) {
		    bu_log("ERROR: Unable to open \"%s\" for reading and writing (check file permissions)\n", framename);

		    if (matflag)
			return 0; /* OK: some undocumented reason */

		    return -1; /* BAD: oops, disappeared */
		}

		/* check if partial result */
		ret = fstat(fd, &sb);
		if (ret >= 0 && sb.st_size > 0 && (size_t)sb.st_size < width*height*sizeof(RGBpixel)) {

		    /* Read existing pix data into the frame buffer */
		    if (sb.st_size > 0) {
			size_t bytes_read = fread(pixmap, 1, (size_t)sb.st_size, outfp);
			if (rt_verbosity & VERBOSE_OUTPUTFILE)
			    bu_log("Reading existing pix data from \"%s\".\n", framename);
			if (bytes_read < (size_t)sb.st_size)
			    return -1;
		    }

		}
	    }
	}
#endif

	/* Ordinary case for creating output file */
	if (outfp == NULL) {
#ifndef RT_TXT_OUTPUT
	    /* FIXME: in the case of rtxray, this is wrong.  it writes
	     * out a bw image so depth should be just 1, not 3.
	     */
	    bif = icv_create(width, height, ICV_COLOR_SPACE_RGB);

	    if (bif == NULL && (outfp = fopen(framename, "w+b")) == NULL) {
		perror(framename);
		if (matflag)
		    return 0;	/* OK */
		return -1;			/* Bad */
	    }
#else
	    outfp = fopen(framename, "w");
	    if (outfp == NULL) {
		perror(framename);
		if (matflag)
		    return 0;	/* OK */
		return -1;			/* Bad */
	    }
#endif
	}

	if (rt_verbosity & VERBOSE_OUTPUTFILE)
	    bu_log("Output file is '%s' %zux%zu pixels\n", framename, width, height);
    }

    /* initialize lighting, may update pix_start */
    view_2init(&APP, framename);

#ifdef USE_OPENCL
    if (opencl_mode) {
        unsigned int mode = 0;

	mode |= CLT_COLOR;
        if (full_incr_mode)
	    mode |= CLT_ACCUM;

        clt_view_init(mode);
    }
#endif

    rtip->nshots = 0;
    rtip->nmiss_model = 0;
    rtip->nmiss_tree = 0;
    rtip->nmiss_solid = 0;
    rtip->nmiss = 0;
    rtip->nhits = 0;
    rtip->rti_nrays = 0;

    if (rt_verbosity & (VERBOSE_LIGHTINFO|VERBOSE_STATS))
	bu_log("\n");
    fflush(stdout);
    fflush(stderr);

    /*
     * Compute the image
     * It may prove desirable to do this in chunks
     */
    rt_prep_timer();

#ifdef USE_OPENCL
    if (opencl_mode) {
	clt_run(pix_start, pix_end);

	/* Reset values to full size, for next frame (if any) */
	pix_start = 0;
	pix_end = (int)(height*width - 1);
    }
    else
#endif
    if (incr_mode) {
	for (incr_level = 1; incr_level <= incr_nlevel; incr_level++) {
	    if (incr_level > 1)
		view_2init(&APP, framename);

	    do_run(0, (1<<incr_level)*(1<<incr_level)-1);
	}
    }
    else if (full_incr_mode) {
	/* Multiple frame buffer mode */
	for (full_incr_sample = 1; full_incr_sample <= full_incr_nsamples;
	    full_incr_sample++) {
	    if (full_incr_sample > 1) /* first sample was already initialized */
		view_2init(&APP, framename);
	    do_run(pix_start, pix_end);
	}
    }
    else {
	do_run(pix_start, pix_end);

	/* Reset values to full size, for next frame (if any) */
	pix_start = 0;
	pix_end = (int)(height*width - 1);
    }
    utime = rt_get_timer(&times, &wallclock);

    /*
     * End of application.  Done outside of timing section.
     * Typically, writes any remaining results out.
     */
    view_end(&APP);

    /* These results need to be normalized.  Otherwise, all we would
     * know is that a given workload takes about the same amount of
     * CPU time, regardless of the number of CPUs.
     */
    if ((size_t)npsw > 1) {
	size_t avail_cpus;
	size_t ncpus;

	avail_cpus = bu_avail_cpus();
	if ((size_t)npsw > avail_cpus) {
	    ncpus = avail_cpus;
	} else {
	    ncpus = npsw;
	}
	nutime = utime / ncpus;			/* compensate */
    } else {
	nutime = utime;
    }

    /* prevent a bogus near-zero time to prevent infinite and
     * near-infinite results without relying on IEEE floating point
     * zero comparison.
     */
    if (NEAR_ZERO(nutime, VDIVIDE_TOL)) {
	bu_log("WARNING:  Raytrace timings are likely to be meaningless\n");
	nutime = VDIVIDE_TOL;
    }

    /*
     * All done.  Display run statistics.
     */
    if (rt_verbosity & VERBOSE_STATS)
	bu_log("SHOT: %s\n", bu_vls_addr(&times));
    bu_vls_free(&times);
    memory_summary();
    if (rt_verbosity & VERBOSE_STATS) {
	bu_log("%zu solid/ray intersections: %zu hits + %zu miss\n",
	       rtip->nshots, rtip->nhits, rtip->nmiss);
	bu_log("pruned %.1f%%:  %zu model RPP, %zu dups skipped, %zu solid RPP\n",
	       rtip->nshots > 0 ? ((double)rtip->nhits*100.0)/rtip->nshots : 100.0,
	       rtip->nmiss_model, rtip->ndup, rtip->nmiss_solid);
	bu_log("Frame %2d: %10zu pixels in %9.2f sec = %12.2f pixels/sec\n",
	       framenumber,
	       width*height, nutime, ((double)(width*height))/nutime);
	bu_log("Frame %2d: %10zu rays   in %9.2f sec = %12.2f rays/sec (RTFM)\n",
	       framenumber,
	       rtip->rti_nrays, nutime, ((double)(rtip->rti_nrays))/nutime);
	bu_log("Frame %2d: %10zu rays   in %9.2f sec = %12.2f rays/CPU_sec\n",
	       framenumber,
	       rtip->rti_nrays, utime, ((double)(rtip->rti_nrays))/utime);
	bu_log("Frame %2d: %10zu rays   in %9.2f sec = %12.2f rays/sec (wallclock)\n",
	       framenumber,
	       rtip->rti_nrays,
	       wallclock, ((double)(rtip->rti_nrays))/wallclock);
    }
    if (bif != NULL) {
	icv_write(bif, framename, BU_MIME_IMAGE_AUTO);
	icv_destroy(bif);
	bif = NULL;
    }

    if (outfp != NULL) {
	(void)fclose(outfp);
	outfp = NULL;
    }

    if (OPTICAL_DEBUG&OPTICAL_DEBUG_STATS) {
	/* Print additional statistics */
	res_pr();
    }

    bu_log("\n");
    bu_free(pixmap, "pixmap allocate");
    pixmap = (unsigned char *)NULL;
    return 0;		/* OK */
}


/* given the bounds and aspect ratio for a view, calculate the
 * viewsize so that its diagonal will fit in view (thus always
 * visible).
 */
static double
autoviewsize(point_t viewmin, point_t viewmax, double aspectratio)
{
    double size = viewsize; /* global */
    vect_t diag;

    /* Fit a sphere to the model RPP, diameter is viewsize, unless
     * viewsize command used to override.
     */
    if (size < 0.0 || ZERO(size)) {
	VSUB2(diag, viewmax, viewmin);
	size = MAGNITUDE(diag);
	if (aspectratio > 1.0) {
	    /* don't clip any of the image when autoscaling */
	    size *= aspectratio;
	}
    }

    /* sanity check: make sure viewsize still isn't zero in case
     * bounding box is empty, otherwise bn_mat_int() will bomb.
     */
    if (size < 0.0 || ZERO(size)) {
	size = 2.0; /* arbitrary so Viewrotscale is normal */
    }

    return size;
}


/**
 * Compute the rotation specified by the azimuth and elevation
 * parameters.  First, note that these are specified relative to the
 * GIFT "front view", i.e., model (X, Y, Z) is view (Z, X, Y): looking
 * down X axis, Y right, Z up.
 *
 * A positive azimuth represents rotating the *eye* around the
 * Y axis, or, rotating the *model* in -Y.
 * A positive elevation represents rotating the *eye* around the
 * X axis, or, rotating the *model* in -X.
 */
void
do_ae(double azim, double elev)
{
    vect_t temp;
    mat_t toEye;
    struct rt_i *rtip = APP.a_rt_i;

    if (rtip == NULL)
	return;

    if (rtip->mdl_max[X] >= INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit minimum\n");
	VSETALL(rtip->mdl_min, -1);
    }
    if (rtip->mdl_max[X] <= -INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit maximum\n");
	VSETALL(rtip->mdl_max, 1);
    }

    /*
     * Enlarge the model RPP just slightly, to avoid nasty effects
     * with a solid's face being exactly on the edge NOTE: This code
     * is duplicated out of librt/tree.c/rt_prep(), and has to appear
     * here to enable the viewsize calculation to match the final RPP.
     */
    rtip->mdl_min[X] = floor(rtip->mdl_min[X]);
    rtip->mdl_min[Y] = floor(rtip->mdl_min[Y]);
    rtip->mdl_min[Z] = floor(rtip->mdl_min[Z]);
    rtip->mdl_max[X] = ceil(rtip->mdl_max[X]);
    rtip->mdl_max[Y] = ceil(rtip->mdl_max[Y]);
    rtip->mdl_max[Z] = ceil(rtip->mdl_max[Z]);

    MAT_IDN(Viewrotscale);
    bn_mat_angles(Viewrotscale, 270.0+elev, 0.0, 270.0-azim);

    /* Look at the center of the model */
    MAT_IDN(toEye);
    toEye[MDX] = -((rtip->mdl_max[X]+rtip->mdl_min[X])/2.0);
    toEye[MDY] = -((rtip->mdl_max[Y]+rtip->mdl_min[Y])/2.0);
    toEye[MDZ] = -((rtip->mdl_max[Z]+rtip->mdl_min[Z])/2.0);

    /* determine global viewsize based on model size */
    viewsize = autoviewsize(rtip->mdl_min, rtip->mdl_max, aspect);

    Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);
    VSET(temp, 0, 0, eye_backoff);
    MAT4X3PNT(eye_model, view2model, temp);
}


void
res_pr(void)
{
    register struct resource *res;
    register size_t i;

    bu_log("\nResource use summary, by processor:\n");
    res = &resource[0];
    for (i = 0; i < (size_t)npsw; i++, res++) {
	bu_log("---CPU %zu:\n", i);
	if (res->re_magic != RESOURCE_MAGIC) {
	    bu_log("Bad magic number!\n");
	    continue;
	}
	bu_log("seg       len=%10ld get=%10ld free=%10ld\n", res->re_seglen, res->re_segget, res->re_segfree);
	bu_log("partition len=%10ld get=%10ld free=%10ld\n", res->re_partlen, res->re_partget, res->re_partfree);
	bu_log("boolstack len=%10ld\n", res->re_boolslen);
    }
}


/**
 * Command table for RT control script language
 */
struct command_tab rt_do_tab[] = {
    {"start", "frame number", "start a new frame",
     cm_start,	2, 2},
    {"viewsize", "size in mm", "set view size",
     cm_vsize,	2, 2},
    {"eye_pt", "xyz of eye", "set eye point",
     cm_eyept,	4, 4},
    {"lookat_pt", "x y z [yflip]", "set eye look direction, in X-Y plane",
     cm_lookat_pt,	4, 5},
    {"viewrot", "4x4 matrix", "set view direction from matrix",
     cm_vrot,	17, 17},
    {"orientation", "quaternion", "set view direction from quaternion",
     cm_orientation,	5, 5},
    {"end", 	"", "end of frame setup, begin raytrace",
     cm_end,		1, 1},
    {"multiview", "", "produce stock set of views",
     cm_multiview,	1, 1},
    {"anim", 	"path type args", "specify articulation animation",
     cm_anim,	4, 999},
    {"tree", 	"treetop(s)", "specify alternate list of tree tops",
     cm_tree,	1, 999},
    {"draw", 	"obj", "add an object to the active list",
     cm_draw,	2, 999},
    {"erase", 	"obj", "remove an object from the active list",
     cm_erase,	2, 999},
    {"prep", 	"", "(re)prep for raytrace with the current obj list",
     cm_prep,	1, 1},
    {"clean", "", "clean articulation from previous frame",
     cm_clean,	1, 1},
    {"_closedb", "", "Close .g database, (for memory debugging)",
     cm_closedb,	1, 1},
    {"set", 	"", "show or set parameters",
     cm_set,		1, 999},
    {"ae", "azim elev", "specify view as azim and elev, in degrees",
     cm_ae,		3, 3},
    {"opt", "-flags", "set flags, like on command line",
     cm_opt,		2, 999},
    {(char *)0, (char *)0, (char *)0,
     0,		0, 0	/* END */}
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
