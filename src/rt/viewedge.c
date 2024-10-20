/*                      V I E W E D G E . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2024 United States Government as represented by
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
/** @file rt/viewedge.c
 *
 * Ray Tracing program RTEDGE bottom half.
 *
 * This module utilizes the RT library to interrogate a MGED model and
 * produce a pixfile or framebuffer image of the hidden line 'edges'
 * of the geometry. An edge exists whenever there is a change in
 * region ID, or a significant change in obliquity or line-of-sight
 * distance.
 *
 * TODO: would be nice to add support for detecting changes in
 * specified attributes.
 *
 * TODO: the antialiasing implementation is very poor at the moment
 * (and possibly has a bug) in that it subsamples the current edge
 * cell at an awkward gridding.
 *
 * TODO: horribly inefficient at the moment with the current
 * book-keeping with 2x as much work occurring as necessary on default
 * renders due to double-firing of the "below" row.  antialiasing
 * makes it even worse with entire subgrids being refired (although
 * they should all at least be unique).
 *
 * TODO: it would be nice to capture an edge "intensity" based on
 * normals, region ids, and the other detectable patterns, but then
 * still allow edge processing (e.g., basic canny edge detection) on
 * the resultant set so you can control the level of edges visible.
 *
 * There are a variety of accepted mechanisms for generating an edge
 * diagram.  Some are found more acceptable than others, though, for
 * general purpose use.
 *
 * THOSE GENERALLY ALL AGREE ON ARE:
 *
 * ** Exterior silhouettes: an outer edge contour, changing from one
 * object to the next or to background. [rtedge implements both]
 *
 * ** Occluding contours: depth discontinuities, surface normal is
 * nearly perpendicular to view direction [rtedge implements both]
 *
 * OTHERS THAT COULD BE USEFUL BUT RTEDGE DOES NOT IMPLEMENT ARE:
 *
 * ** Suggestive contours
 *
 * ** "Almost contours"
 *
 * ** A point that becomes a contour in nearby views
 *
 * ** DOT(normal, viewdir) is at a local minimum (it's a form of
 * curvature where radial curvature == 0)
 *
 * OTHERS THAT COULD ALSO BE USEFUL INCLUDE:
 *
 * ** Image valleys, indicated by intensity
 *
 * ** Principal highlights, showing maxima of (n.v) to indicate
 * view-independent ridges and valleys
 *
 * ** Different family of lines in shadowed regions, not just inverted
 * (e.g., cross-hatching)
 *
 * ** Apparent ridges and valleys (view-dependent) where you look for
 * rapid screen-space normal variation
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "dm.h"
#include "bu/parse.h"
#include "bu/parallel.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "icv.h"

#include "./rtuif.h"
#include "./ext.h"


#define COSTOL 0.91    /* normals differ if dot product < COSTOL */
#define OBLTOL 0.1     /* high obliquity if cosine of angle < OBLTOL ! */
#define is_Odd(_a)      ((_a)&01)
#define ARCTAN_87 19.08

#ifndef Abs
#  define Abs(x) ((x) < 0 ? -(x) : (x))                  /* UNSAFE */
#endif


extern struct fb *fbp;	/* Framebuffer handle */
extern fastf_t viewsize;
extern int lightmodel;
extern size_t width, height;
extern int per_processor_chunk;
extern int default_background;

static int pixsize = 0;	/* bytes per pixel in scanline */

struct cell {
    int c_ishit;
    struct region * c_region;
    fastf_t c_dist;	/* distance from emanation plane to in_hit */
    int c_id;		/* region_id of component hit */
    point_t c_hit;	/* 3-space hit point of ray */
    vect_t c_normal;	/* surface normal at the hit point */
    vect_t c_rdir;	/* ray direction, permits perspective */
};
#define CELL_INIT {0, NULL, 0.0, 0, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO}

#define MISS_DIST MAX_FASTF
#define MISS_ID -1

static unsigned char *writeable[MAX_PSW];
static unsigned char *scanline[MAX_PSW];
static unsigned char *blendline[MAX_PSW];
static struct cell *saved[MAX_PSW];
static struct resource occlusion_resources[MAX_PSW];

int nEdges = 0;
int nPixels = 0;

/** min. distance for drawing pits/mountains */
fastf_t max_dist = -1;

/**
 * Value of the cosine of the angle between surface normals that
 * triggers shading
 */
fastf_t maxangle;

typedef int color[3];
fastf_t float_fgcolor[3] = {1.0, 1.0, 1.0};
color fgcolor = {255, 255, 255};
color bgcolor = {0, 0, 0};

/*
 * Flags that set which edges are detected.
 *
 * detect_ids -> detect boundaries region id codes.
 * detect_regions -> detect region boundaries.
 * detect_distance -> detect noticeable differences in hit distance.
 * detect_normals -> detect rapid change in surface normals
 */
int detect_ids = 1;
int detect_regions = 0;
int detect_distance = 1;
int detect_normals = 1;
int detect_attributes = 0; /* unsupported yet */

RGBpixel fb_bg_color;

/*
 * Overlay Mode
 *
 * If set, and the fbio points to a readable framebuffer, only edge
 * pixels are splatted. This allows rtedge to overlay edges directly
 * rather than having to use pixmerge.
 */
#define OVERLAY_MODE_UNSET 0
#define OVERLAY_MODE_DOIT  1
#define OVERLAY_MODE_FORCE 2

static int overlay = OVERLAY_MODE_UNSET;

/*
 * Blend Mode
 *
 * If set, and the fbio points to a readable framebuffer, the edge
 * pixels are blended (using some HSV manipulations) with the original
 * framebuffer pixels. The intent is to produce an effect similar to
 * the "bugs" on TV networks.
 *
 * Doesn't work worth beans!
 */
int blend = 0;

/*
 * Region Colors Mode
 *
 * If set, the color of edge pixels is set to the region colors.  If
 * the edge is determined because of a change from one region to
 * another, the color selected is the one from the region with the
 * lowest hit distance.
 */
int region_colors = 0;

/*
 * Occlusions Mode
 *
 * This is really cool! Occlusion allows the user to specify a second
 * set of objects (from the same .g) that can be used to separate
 * fore- ground from background.
 */
#define OCCLUSION_MODE_NONE 0
#define OCCLUSION_MODE_EDGES 1
#define OCCLUSION_MODE_HITS 2
#define OCCLUSION_MODE_DITHER 3
#define OCCLUSION_MODE_DEFAULT 2

int occlusion_mode = OCCLUSION_MODE_NONE;

/*
 * whether to perform antialiasing via sub-pixel sampling
 */
static int antialias = 0;
static int both_sides = 0;


/*
 * whether to draw aligned axes at the model's origin
*/
static int draw_axes = 0;


struct bu_vls occlusion_objects = BU_VLS_INIT_ZERO;
struct rt_i *occlusion_rtip = NULL;
struct application **occlusion_apps;


/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"%d", 1, "detect_regions", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "dr", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "detect_distance", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "dd", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "detect_normals", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "dn", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "detect_ids", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "di", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 3, "foreground", 0, color_hook, NULL, NULL},
    {"%f", 3, "fg", 0, color_hook, NULL, NULL},
    {"%f", 3, "background", 0, color_hook, NULL, NULL},
    {"%f", 3, "bg", 0, color_hook, NULL, NULL},
    {"%d", 1, "overlay", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "ov", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "blend", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "bl", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "region_color", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "rc", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%V", 1, "occlusion_objects", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%V", 1, "oo", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "occlusion_mode", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "om", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 1, "max_dist", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 1, "md", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "antialias", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "aa", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "both_sides", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "bs", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "draw_axes", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "da", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


const char title[] = "RT Hidden-Line Renderer";


int handle_main_ray(struct application *ap, register struct partition *PartHeadp, struct seg *segp);
int diffpixel(RGBpixel a, RGBpixel b);

static int occlusion_hit(struct application *ap, struct partition *pt, struct seg *UNUSED(segp))
{
    struct hit *hitp = pt->pt_forw->pt_inhit;

    ap->a_dist = hitp->hit_dist;
    return 1;
}


static int occlusion_miss(struct application *ap)
{
    ap->a_dist = MAX_FASTF;
    return 0;
}


static int occludes(struct application *ap, struct cell *here)
{
    int cpu;
    int oc_hit = 0;

    if (ap->a_resource->re_cpu > 0)
	cpu = ap->a_resource->re_cpu - 1;
    else
	cpu = ap->a_resource->re_cpu;

    /*
     * Test the hit distance on the second geometry.  If the second
     * geometry is closer, do not color pixel
     */
    VMOVE(occlusion_apps[cpu]->a_ray.r_pt, ap->a_ray.r_pt);
    VMOVE(occlusion_apps[cpu]->a_ray.r_dir, ap->a_ray.r_dir);

    oc_hit = rt_shootray(occlusion_apps[cpu]);

    if (!oc_hit) {
	/*
	 * The occlusion ray missed, therefore this pixel occludes the
	 * second geometry.
	 *
	 * Return 2 so that the fact that there is no geometry behind
	 * can be conveyed to the OCCLUSION_MODE_DITHER section.
	 */
	return 2;
    }

    if (NEAR_EQUAL(occlusion_apps[cpu]->a_dist, here->c_dist, BN_TOL_DIST)) {
	/* same hit point.  object is probably in first and second
	 * geometry sets.  it's not occluding itself, but we want an
	 * edge because it's in the first display set.
	 */
	return 1;
    } else if (	occlusion_apps[cpu]->a_dist < here->c_dist) {
	/* second geometry is closer than the first, therefore it is
	 * 'foreground'. Do not draw the edge.
	 *
	 * This pixel DOES NOT occlude the second geometry.
	 */
	return 0;
    }

    return 1;
}


int rayhit(struct application *ap, register struct partition *pt,
	   struct seg *segp)
{
    handle_main_ray(ap, pt, segp);
    return 1;
}


int raymiss(struct application *ap)
{
    handle_main_ray(ap, NULL, NULL);
    return 0;
}


void choose_color(RGBpixel col, double intensity, struct cell *me,
		  struct cell *left, struct cell *below, struct cell *right, struct cell *above)
{
    col[RED] = fgcolor[RED];
    col[GRN] = fgcolor[GRN];
    col[BLU] = fgcolor[BLU];

    if (region_colors && me) {

	struct cell *use_this = me;

	/*
	 * Determine the cell with the smallest hit distance.
	 */

	if (left)
	    use_this = (use_this->c_dist < left->c_dist) ? use_this : left;
	if (below)
	    use_this = (use_this->c_dist < below->c_dist) ? use_this : below;
	if (right)
	    use_this = (use_this->c_dist < right->c_dist) ? use_this : right;
	if (above)
	    use_this = (use_this->c_dist < above->c_dist) ? use_this : above;

	if (use_this == (struct cell *)NULL)
	    bu_exit(EXIT_FAILURE, "Error: use_this is NULL.\n");

	if (!use_this->c_region)
	    bu_exit(EXIT_FAILURE, "Error: use_this->c_region is NULL.\n");

	col[RED] = 255 * use_this->c_region->reg_mater.ma_color[RED];
	col[GRN] = 255 * use_this->c_region->reg_mater.ma_color[GRN];
	col[BLU] = 255 * use_this->c_region->reg_mater.ma_color[BLU];
    }

    col[RED] = (col[RED] * intensity) + (bgcolor[RED] * (1.0-intensity));
    col[GRN] = (col[GRN] * intensity) + (bgcolor[GRN] * (1.0-intensity));
    col[BLU] = (col[BLU] * intensity) + (bgcolor[BLU] * (1.0-intensity));

    return;
}


/**
 * Called at the start of a run.
 *
 * Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init(struct application *ap, char *file, char *UNUSED(obj), int minus_o, int minus_F)
{
    /*
     * Allocate a scanline for each processor.
     */
    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 1;

    /*
     * Does the user want occlusion checking?
     *
     * If so, load and prep.
     */
    if (bu_vls_strlen(&occlusion_objects) != 0) {
	struct db_i *dbip;
	size_t nObjs;
	int split_argc;
	const char **objs;
	size_t i;

	bu_log("rtedge: loading occlusion geometry from %s.\n", file);

	if (bu_argv_from_tcl_list(bu_vls_addr(&occlusion_objects), &split_argc, &objs) == 1) {
	    bu_log("rtedge: occlusion list = %s\n",
		   bu_vls_addr(&occlusion_objects));
	    bu_exit(EXIT_FAILURE, "rtedge: could not parse occlusion objects list.\n");
	}
	nObjs = split_argc;

	for (i=0; i<nObjs; ++i) {
	    bu_log("rtedge: occlusion object %zu = %s\n", i, objs[i]);
	}


	if ((dbip = db_open(file, DB_OPEN_READONLY)) == DBI_NULL)
	    bu_exit(EXIT_FAILURE, "rtedge: could not open geometry database file %s.\n", file);
	RT_CK_DBI(dbip);

#if 0
	/* FIXME: calling this when db_open()'s mapped file doesn't
	 * fail will cause duplicate directory entries.  need to make
	 * sure db_dirbuild rebuilds from scratch or only updates
	 * existing entries when they exist.
	 */
	if (db_dirbuild(dbip) < 0)
	    bu_exit(EXIT_FAILURE, "rtedge: could not read database.\n");
#endif

	occlusion_rtip = rt_new_rti(dbip); /* clones dbip */

	memset(occlusion_resources, 0, sizeof(occlusion_resources));
	for (i=0; i < MAX_PSW; i++) {
	    rt_init_resource(&occlusion_resources[i], i, occlusion_rtip);
	}

	db_close(dbip);			 /* releases original dbip */

	for (i=0; i<nObjs; ++i)
	    if (rt_gettree(occlusion_rtip, objs[i]) < 0)
		bu_log("rtedge: gettree failed for %s\n", objs[i]);
	    else
		bu_log("rtedge: got tree for object %zu = %s\n", i, objs[i]);

	bu_free((char *)objs, "free occlusion objs array");

	bu_log("rtedge: occlusion rt_gettrees done.\n");

	rt_prep(occlusion_rtip);

	bu_log("rtedge: occlusion prep done.\n");

	/*
	 * Create a set of application structures for the occlusion
	 * geometry. Need one per cpu, the upper half does the per-
	 * thread allocation in worker, but that's off limits.
	 */
	occlusion_apps = (struct application **)bu_calloc((size_t)npsw, sizeof(struct application *),
							  "occlusion application structure array");
	for (i=0; i<(size_t)npsw; ++i) {
	    BU_ALLOC(occlusion_apps[i], struct application);
	    RT_APPLICATION_INIT(occlusion_apps[i]);

	    occlusion_apps[i]->a_rt_i = occlusion_rtip;
	    occlusion_apps[i]->a_resource = (struct resource *)BU_PTBL_GET(&occlusion_rtip->rti_resources, i);
	    occlusion_apps[i]->a_onehit = 1;
	    occlusion_apps[i]->a_hit = occlusion_hit;
	    occlusion_apps[i]->a_miss = occlusion_miss;
	    if (rpt_overlap)
		occlusion_apps[i]->a_logoverlap = (void (*)(struct application *, const struct partition *, const struct bu_ptbl *, const struct partition *))NULL;
	    else
		occlusion_apps[i]->a_logoverlap = rt_silent_logoverlap;

	}
	bu_log("rtedge: will perform occlusion testing.\n");

	/*
	 * If an inclusion mode has not been specified, use the default.
	 */
	if (occlusion_mode == OCCLUSION_MODE_NONE) {
	    occlusion_mode = OCCLUSION_MODE_DEFAULT;
	    bu_log("rtedge: occlusion mode = %d\n", occlusion_mode);
	}

	if ((occlusion_mode != OCCLUSION_MODE_NONE) &&
	    (overlay == OVERLAY_MODE_UNSET)) {
	    bu_log("rtedge: automagically activating overlay mode.\n");
	    overlay = OVERLAY_MODE_DOIT;
	}

    }

    if (occlusion_mode != OCCLUSION_MODE_NONE &&
	bu_vls_strlen(&occlusion_objects) == 0) {
	bu_exit(EXIT_FAILURE, "rtedge: occlusion mode set, but no objects were specified.\n");
    }


    // TODO - the setting of colors below broke the -W flag - lines and
    // background were both white.  Looks like rtedge was relying on bgcolor
    // being black to get a black line in that mode, but it gets set to white
    // first via -W.  Work around this by stashing the "original" bgcolor in a
    // temporary variable, but this is just a hacky workaround to restore
    // previous behavior.  If -W is just doing what it is supposed to do by
    // setting a white background, and rtedge wants to set default line color
    // that is far away from the background color automatically, we need a
    // different solution for the general case. There is no guarantee that the
    // initialized contents of bgcolor (currently hard-coded to 0,0,0 up on
    // line 159) will be appropriate for all backgrounds - indeed, it is
    // trivially NOT appropriate for all possible background colors.
    color tmpcolor;
    tmpcolor[RED] = bgcolor[RED];
    tmpcolor[GRN] = bgcolor[GRN];
    tmpcolor[BLU] = bgcolor[BLU];


    // Set bgcolor and fgcolor from the floating point versions
    bgcolor[0] = background[0] * 255.0 + 0.5;
    bgcolor[1] = background[1] * 255.0 + 0.5;
    bgcolor[2] = background[2] * 255.0 + 0.5;
    fgcolor[0] = float_fgcolor[0] * 255.0 + 0.5;
    fgcolor[1] = float_fgcolor[1] * 255.0 + 0.5;
    fgcolor[2] = float_fgcolor[2] * 255.0 + 0.5;

    /* if non-default/inverted background was requested, swap the
     * foreground and background colors.
     */
    if (!default_background) {
	int tmp;
	tmp = fgcolor[RED];
	fgcolor[RED] = tmpcolor[RED];
	bgcolor[RED] = tmp;
	tmp = fgcolor[GRN];
	fgcolor[GRN] = tmpcolor[GRN];
	bgcolor[GRN] = tmp;
	tmp = fgcolor[BLU];
	fgcolor[BLU] = tmpcolor[BLU];
	bgcolor[BLU] = tmp;
    }

    if (minus_o && (overlay || blend)) {
	/*
	 * Output is to a file stream.  Do not allow parallel
	 * processing since we can't seek to the rows.
	 */
	RTG.rtg_parallel = 0;
	bu_log("view_init: deactivating parallelism due to -o option.\n");
	/*
	 * The overlay and blend cannot be used in -o mode.  Note that
	 * the overlay directive takes precedence, they can't be used
	 * together.
	 */
	overlay = 0;
	blend = 0;
	bu_log("view_init: deactivating overlay and blending due to -o option.\n");
    }

    if (overlay)
	bu_log("view_init: will perform simple overlay.\n");
    else if (blend)
	bu_log("view_init: will perform blending.\n");

    return minus_F || (!minus_o && !minus_F); /* we need a framebuffer */
}


/**
 * beginning of a frame
 */
void
view_2init(struct application *UNUSED(ap), char *UNUSED(framename))
{
    size_t i;

    /*
     * Per_processor_chuck specifies the number of pixels rendered per
     * each pass of a worker. By making this value equal to the width
     * of the image, each worker will render one scanline at a time.
     */
    per_processor_chunk = width;

    /*
     * Use three bytes per pixel.
     */
    pixsize = 3;

    /*
     * Set the hit distance difference necessary to trigger an edge.
     * This algorithm was stolen from lgt, I may make it settable
     * later.
     */
    if (max_dist < .00001)
	max_dist = (cell_width*ARCTAN_87)+2;

    /*
     * Determine if the framebuffer is readable.
     */
    if (overlay || blend)
	if (fb_read(fbp, 0, 0, fb_bg_color, 1) < 0)
	    bu_exit(EXIT_FAILURE, "rt_edge: specified framebuffer is not readable, cannot merge.\n");

    /*
     * Create a cell to store current data for next cells left side.
     * Create a edge flag buffer for each processor.  Create a
     * scanline buffer for each processor.
     */
    for (i = 0; i < (size_t)npsw; ++i) {
	if (saved[i] == NULL)
	    BU_ALLOC(saved[i], struct cell);
	if (writeable[i] == NULL)
	    writeable[i] = (unsigned char *) bu_calloc(1, per_processor_chunk, "writeable pixel flag buffer");
	if (scanline[i] == NULL)
	    scanline[i] = (unsigned char *) bu_calloc(per_processor_chunk, pixsize, "scanline buffer");
	/*
	 * If blending is desired, create scanline buffers to hold the
	 * read-in lines from the framebuffer.
	 */
	if (blend && blendline[i] == NULL)
	    blendline[i] = (unsigned char *) bu_calloc(per_processor_chunk, pixsize, "blend buffer");
    }

    /*
     * If operating in overlay mode, we want the rtedge background
     * color to be the shaded images background. This sets the bg
     * color automatically, but assumes that pixel 0, 0 is
     * background. If not, the user can set it manually (so long as it
     * isn't 0 0 1!).
     *
     */
    if (overlay && bgcolor[RED] == 0 && bgcolor[GRN] == 0 && bgcolor[BLU] == 1) {
	bgcolor[RED] = fb_bg_color[RED];
	bgcolor[GRN] = fb_bg_color[GRN];
	bgcolor[BLU] = fb_bg_color[BLU];
    }
    return;
}


/**
 * end of each pixel
 */
void view_pixel(struct application *UNUSED(ap))
{
}


/**
 * action performed at the end of each scanline
 */
void
view_eol(struct application *ap)
{
    int cpu;
    int i;

    if (ap->a_resource->re_cpu > 0)
	cpu = ap->a_resource->re_cpu - 1;
    else
	cpu = ap->a_resource->re_cpu;

    if (overlay) {
	/*
	 * Overlay mode. Check if the pixel is an edge.  If so, write
	 * it to the framebuffer.
	 */
	for (i = 0; i < per_processor_chunk; ++i) {
	    if (writeable[cpu][i]) {
		/*
		 * Write this pixel
		 */
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		fb_write(fbp, i, ap->a_y, &scanline[cpu][i*3], 1);
		bu_semaphore_release(BU_SEM_SYSCALL);
	    }
	}
    } else if (blend) {
	/*
	 * Blend mode.
	 *
	 * Read a line from the existing framebuffer, convert to HSV,
	 * manipulate, and put the results in the scanline as RGB.
	 */
	int replace_down = 0; /* flag that specifies if the pixel in the
			       * scanline below must be replaced.
			       */
	RGBpixel rgb;
	fastf_t hsv[3];

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	if (fb_read(fbp, 0, ap->a_y, blendline[cpu], per_processor_chunk) < 0)
	    bu_exit(EXIT_FAILURE, "rtedge: error reading from framebuffer.\n");
	bu_semaphore_release(BU_SEM_SYSCALL);

	for (i = 0; i < per_processor_chunk; ++i) {
	    /*
	     * Is this pixel an edge?
	     */
	    if (writeable[cpu][i]) {

		/*
		 * The pixel is an edge, retrieve the appropriate
		 * pixel from the line buffer and convert it to HSV.
		 */
		rgb[RED] = blendline[cpu][i*3+RED];
		rgb[GRN] = blendline[cpu][i*3+GRN];
		rgb[BLU] = blendline[cpu][i*3+BLU];

		/*
		 * Is the pixel in the blendline array the background
		 * color? If so, look left and down to determine which
		 * pixel is the "source" of the edge. Unless, of
		 * course, we are on the bottom scanline or the
		 * leftmost column (x=y=0)
		 */
		if (i != 0 && ap->a_y != 0 && !diffpixel(rgb, fb_bg_color)) {
		    RGBpixel left;
		    RGBpixel down;

		    left[RED] = blendline[cpu][(i-1)*3+RED];
		    left[GRN] = blendline[cpu][(i-1)*3+GRN];
		    left[BLU] = blendline[cpu][(i-1)*3+BLU];

		    bu_semaphore_acquire(BU_SEM_SYSCALL);
		    fb_read(fbp, i, ap->a_y - 1, down, 1);
		    bu_semaphore_release(BU_SEM_SYSCALL);

		    if (diffpixel(left, fb_bg_color)) {
			/*
			 * Use this one.
			 */
			rgb[RED] = left[RED];
			rgb[GRN] = left[GRN];
			rgb[BLU] = left[BLU];
		    } else if (diffpixel(down, fb_bg_color)) {
			/*
			 * Use the pixel from the scanline below
			 */
			replace_down = 1;

			rgb[RED] = down[RED];
			rgb[GRN] = down[GRN];
			rgb[BLU] = down[BLU];
		    }
		}
		/*
		 * Convert to HSV
		 */
		bu_rgb_to_hsv(rgb, hsv);

		/*
		 * Now perform the manipulations.
		 */
		hsv[VAL] *= 3.0;
		hsv[SAT] /= 3.0;

		if (hsv[VAL] > 1.0) {
		    fastf_t d = hsv[VAL] - 1.0;

		    hsv[VAL] = 1.0;
		    hsv[SAT] -= d;
		    hsv[SAT] = hsv[SAT] >= 0.0 ? hsv[SAT] : 0.0;
		}

		/*
		 * Convert back to RGB.
		 */
		bu_hsv_to_rgb(hsv, rgb);

		if (replace_down) {
		    /*
		     * Write this pixel immediately, do not put it
		     * into the blendline since it corresponds to the
		     * wrong scanline.
		     */
		    bu_semaphore_acquire(BU_SEM_SYSCALL);
		    fb_write(fbp, i, ap->a_y, rgb, 1);
		    bu_semaphore_release(BU_SEM_SYSCALL);

		    replace_down = 0;
		} else {
		    /*
		     * Put this pixel back into the blendline array.
		     * We'll push it to the buffer when the entire
		     * scanline has been processed.
		     */
		    blendline[cpu][i*3+RED] = rgb[RED];
		    blendline[cpu][i*3+GRN] = rgb[GRN];
		    blendline[cpu][i*3+BLU] = rgb[BLU];
		}
	    } /* end "if this pixel is an edge" */
	} /* end pixel loop */

	/*
	 * Write the blendline to the framebuffer.
	 */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fb_write(fbp, 0, ap->a_y, blendline[cpu], per_processor_chunk);
	bu_semaphore_release(BU_SEM_SYSCALL);
    } /* end blend */

    else if (fbp != FB_NULL) {
	/*
	 * Simple whole scanline write to a framebuffer.
	 */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fb_write(fbp, 0, ap->a_y, scanline[cpu], per_processor_chunk);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    else if (bif != NULL) {
	/*
	 * Write to an icv_image_t.
	 */
	/* TODO : Add double type data to maintain resolution */
	icv_writeline(bif, ap->a_y, scanline[cpu],  ICV_DATA_UCHAR);
    }

    else
	bu_log("rtedge: strange, no end of line actions taken.\n");

    return;

}


void view_setup(struct rt_i *UNUSED(rtip))
{
}


/**
 * end of a frame, called after rt_clean()
 */
void
view_cleanup(struct rt_i *UNUSED(rtip))
{
}

/**
 * draws a pixel depending on whether we're writing to a file or a window
 */
void draw_pixel(const double x, const double y, const RGBpixel pixel)
{
    if (fbp != FB_NULL) {
        (void)fb_write(fbp, x, y, pixel, 1);
    }
    else if (bif) {
	RGBpixel ptmp;
	for (int i = 0; i < 3; i++)
	    ptmp[i] = pixel[i];
	double *pdata = icv_uchar2double(ptmp, 3);
        (void)icv_writepixel(bif, x, y, pdata);
	bu_free(pdata, "pdata");
    }
}

/**
 * draws a 'X' with bottom left pixel location denoted by (X, Y).
 */
void draw_x_label(const double x, const double y, const unsigned int lineLength, const RGBpixel pixel)
{
    if (!draw_axes) {
        return;
    }
    for (unsigned int i = 0; i <= lineLength; i++) {
        draw_pixel(x + i, y + i, pixel);
        draw_pixel(x + lineLength - i, y + i, pixel);
    }
}

/**
 * draws a 'Y' with bottom left pixel location denoted by (X, Y).
 */
void draw_y_label(const double x, const double y, const unsigned int lineLength, const RGBpixel pixel)
{
    if (!draw_axes) {
        return;
    }
    for (unsigned int i = 0; i <= lineLength; i++) {
        draw_pixel(x, y + i, pixel);
        draw_pixel(x - i, y + lineLength + i, pixel);
        draw_pixel(x + i, y + lineLength + i, pixel);
    }
}

/**
 * Draws a 'Z' with bottom left pixel location denoted by(X, Y).
 */
void draw_z_label(const double x, const double y, const unsigned int lineLength, const RGBpixel pixel)
{
    if (!draw_axes) {
        return;
    }
    for (unsigned int i = 0; i <= lineLength; i++) {
        draw_pixel(x + i, y, pixel);
        draw_pixel(x + i, y + i, pixel);
        draw_pixel(x + i, y + lineLength, pixel);
    }
}

/**
 * end of each frame, draws axis aligned axes and origin if "draw_axes" is enabled
 */
void
view_end(struct application* UNUSED(ap))
{
    if (!draw_axes || (fbp == NULL && bif == NULL)) {
        return;
    }

    const RGBpixel pixel = { 31, 73, 133 };

    // model center in pixel coordinates
    const double modelCenter[2] = { (width + 0.5) / 2.0 + (model2view[MDX] / cell_width), (height + 0.5) / 2.0 + (model2view[MDY] / cell_height) };

    if (modelCenter[0] < 0 || modelCenter[0] >= width || modelCenter[1] < 0 || modelCenter[1] >= height) {
        return;
    }

    const double EPSILON = 1e-9;
    const unsigned int HALF_AXES_LENGTH = 10;
    const unsigned int POSITIVE_AXES_EXTRA_LENGTH = 5;
    const unsigned int AXES_END = 15;
    // offset to draw text labels after the axes
    const unsigned int OFFSET = 5;

    // front view
    if (fabs(azimuth) <= EPSILON && fabs(elevation) <= EPSILON)
    {
        // drawing positive axis labels (+X, +Y, +Z).
        // The added magic numbers are used to offset the space taken up by the label.
        draw_z_label(modelCenter[0] - 5, modelCenter[1] + AXES_END + OFFSET, 10, pixel);
        draw_y_label(modelCenter[0] + AXES_END + OFFSET + 3, modelCenter[1] - 5, 6, pixel);
        for (unsigned int i = 1; i <= POSITIVE_AXES_EXTRA_LENGTH; i++) {
            // making positive axes longer
            draw_pixel(modelCenter[0], modelCenter[1] + HALF_AXES_LENGTH + i, pixel);
            draw_pixel(modelCenter[0] + HALF_AXES_LENGTH + i, modelCenter[1], pixel);
        }
    }
    // top view
    else if (fabs(azimuth) <= EPSILON && fabs(elevation - 90.0) <= EPSILON)
    {
        draw_x_label(modelCenter[0] - AXES_END - OFFSET - 7, modelCenter[1] - 5, 10, pixel);
        draw_y_label(modelCenter[0], modelCenter[1] - AXES_END - OFFSET - 10, 6, pixel);
        for (unsigned int i = 1; i <= POSITIVE_AXES_EXTRA_LENGTH; i++) {
            draw_pixel(modelCenter[0], modelCenter[1] - HALF_AXES_LENGTH - i, pixel);
            draw_pixel(modelCenter[0] - HALF_AXES_LENGTH - i, modelCenter[1], pixel);
        }
    }
    // left view
    else if (fabs(azimuth - 270.0) <= EPSILON && fabs(elevation) <= EPSILON) {
        draw_z_label(modelCenter[0] - 5, modelCenter[1] + AXES_END + OFFSET, 10, pixel);
        draw_x_label(modelCenter[0] - AXES_END - OFFSET - 7, modelCenter[1] - 5, 10, pixel);
        for (unsigned int i = 1; i <= POSITIVE_AXES_EXTRA_LENGTH; i++) {
            draw_pixel(modelCenter[0], modelCenter[1] + HALF_AXES_LENGTH + i, pixel);
            draw_pixel(modelCenter[0] - HALF_AXES_LENGTH - i, modelCenter[1], pixel);
        }
    }
    else {
        return;
    }

    for (unsigned int i = 0; i < HALF_AXES_LENGTH; i++) {
        draw_pixel(modelCenter[0] + i, modelCenter[1]    , pixel);
        draw_pixel(modelCenter[0]    , modelCenter[1] + i, pixel);
        draw_pixel(modelCenter[0] - i, modelCenter[1]    , pixel);
        draw_pixel(modelCenter[0]    , modelCenter[1] - i, pixel);
    }
}


int rayhit2(struct application *ap, register struct partition *pt, struct seg *UNUSED(segp))
{
    struct partition *pp = pt->pt_forw;
    struct hit *hitp = pt->pt_forw->pt_inhit;
    struct cell *c = (struct cell *)ap->a_uptr;

    c->c_ishit 		= 1;
    c->c_region 	= pp->pt_regionp;
    c->c_id 		= pp->pt_regionp->reg_regionid;
    VMOVE(c->c_rdir, ap->a_ray.r_dir);
    VJOIN1(c->c_hit, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
    RT_HIT_NORMAL(c->c_normal, hitp,
		  pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);
    c->c_dist = hitp->hit_dist;

    return 1;
}


static int
raymiss2(register struct application *ap)
{
    struct cell *c = (struct cell *)ap->a_uptr;

    c->c_ishit    	= 0;
    c->c_region   	= 0;
    c->c_dist     	= MISS_DIST;
    c->c_id	    	= MISS_ID;
    VSETALL(c->c_hit, MISS_DIST);
    VSETALL(c->c_normal, 0);
    VMOVE(c->c_rdir, ap->a_ray.r_dir);

    return 0;
}


/* get_intensity() calls is_edge() unfortunately.  cycle. */
static int
is_edge(double *intensity, struct application *ap, const struct cell *here,
	const struct cell *left, const struct cell *below, const struct cell *right, const struct cell *above);

/**
 * absurdly inefficient way to perform sub-pixel anti-aliasing.  we
 * shoot additional rays to estimate the intensity contribution of
 * this edge cell.
 */
static void
get_intensity(double *intensity, struct application *ap, const struct cell *UNUSED(here), const struct cell *left, const struct cell *below, const struct cell *right, const struct cell *above)
{
    vect_t dy, dx, dy3, dx3;
    struct application aaap;
    struct cell grid[4][4];

    /* Grid layout:
     *
     *    left      right
     * _____________________
     * |0 0 | AL | AR | 0 3|  above
     * |____|____|____|____|
     * | TL | UL | UR | TR |  top/upper
     * |____|____|____|____|
     * | BL | LL | LR | BR |  bottom/lower
     * |____|____|____|____|
     * |3 0 | DL | DR | 3 3|  debajo
     * |____|____|____|____|
     */

    struct cell *AL = &grid[0][1];
    struct cell *AR = &grid[0][2];
    struct cell *TL = &grid[1][0];
    struct cell *UL = &grid[1][1];
    struct cell *UR = &grid[1][2];
    struct cell *TR = &grid[1][3];
    struct cell *BL = &grid[2][0];
    struct cell *LL = &grid[2][1];
    struct cell *LR = &grid[2][2];
    struct cell *BR = &grid[2][3];
    struct cell *DL = &grid[3][1];
    struct cell *DR = &grid[3][2];

    RT_APPLICATION_INIT(&aaap);
    memset(&grid, 0, sizeof(struct cell) * 16);

    /* 4x4 sub-pixel grid, dx gets to inner cells */
    VSCALE(dy, dy_model, 0.125);
    VSCALE(dx, dx_model, 0.125);

    /* 4x4 sub-pixel grid, dx*3 gets to outer cells */
    VSCALE(dy3, dy_model, 0.375);
    VSCALE(dx3, dx_model, 0.375);

    /* setup */
    aaap.a_hit = rayhit2;
    aaap.a_miss = raymiss2;
    aaap.a_onehit = 1;
    aaap.a_rt_i = ap->a_rt_i;
    aaap.a_resource = ap->a_resource;
    aaap.a_logoverlap = ap->a_logoverlap;

    /* Above Left */
    aaap.a_uptr = (void *)AL;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy3);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("AL", aaap.a_ray.r_pt);
#endif

    /* Above Right */
    aaap.a_uptr = (void *)AR;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy3);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("AR", aaap.a_ray.r_pt);
#endif

    /* Top Left */
    aaap.a_uptr = (void *)TL;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx3);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("TL", aaap.a_ray.r_pt);
#endif

    /* Upper Left */
    aaap.a_uptr = (void *)UL;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("UL", aaap.a_ray.r_pt);
#endif

    /* Upper Right */
    aaap.a_uptr = (void *)UR;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("UR", aaap.a_ray.r_pt);
#endif

    /* Top Right */
    aaap.a_uptr = (void *)TR;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx3);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("TR", aaap.a_ray.r_pt);
#endif

    /* Bottom Left */
    aaap.a_uptr = (void *)BL;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx3);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("BL", aaap.a_ray.r_pt);
#endif

    /* Lower Left */
    aaap.a_uptr = (void *)LL;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("LL", aaap.a_ray.r_pt);
#endif

    /* Lower Right */
    aaap.a_uptr = (void *)LR;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("LR", aaap.a_ray.r_pt);
#endif

    /* Bottom Right */
    aaap.a_uptr = (void *)BR;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx3);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("BR", aaap.a_ray.r_pt);
#endif

    /* Debajo Left */
    aaap.a_uptr = (void *)DL;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy3);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("DL", aaap.a_ray.r_pt);
#endif

    /* Debajo Right */
    aaap.a_uptr = (void *)DR;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy3);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("DR", aaap.a_ray.r_pt);
#endif

#define TWOBYTWO 1
#ifdef TWOBYTWO
    if (is_edge(NULL, NULL, UL, left, LL, UR, above)) {
	*intensity += 0.25;
    }
    if (is_edge(NULL, NULL, UR, UL, LR, right, above)) {
	*intensity += 0.25;
    }
    if (is_edge(NULL, NULL, LL, left, below, LR, UL)) {
	*intensity += 0.25;
    }
    if (is_edge(NULL, NULL, LR, LL, below, right, UR)) {
	*intensity += 0.25;
    }
#else
    if (is_edge(NULL, NULL, UL, TL, LL, UR, AL)) {
	*intensity += 0.25;
    }
    if (is_edge(NULL, NULL, UR, UL, LR, TR, AR)) {
	*intensity += 0.25;
    }
    if (is_edge(NULL, NULL, LL, BL, DL, LR, UL)) {
	*intensity += 0.25;
    }
    if (is_edge(NULL, NULL, LR, LL, DR, BR, UR)) {
	*intensity += 0.25;
    }
#endif
}


static int
is_edge(double *intensity, struct application *ap, const struct cell *here,
	const struct cell *left, const struct cell *below, const struct cell *right, const struct cell *above)
{
    int found_edge = 0;

    if (!here) {
	return 0;
    }

    if (here && here->c_ishit) {
	if (detect_ids) {
	    if (left && below) {
		if (here->c_id != left->c_id || here->c_id != below->c_id) {
		    found_edge = 1;
		}
	    }
	    if (both_sides && right && above) {
		if (here->c_id != right->c_id || here->c_id != above->c_id) {
		    found_edge = 1;
		}
	    }
	}

	if (detect_regions) {
	    if (left && below) {
		if (here->c_region != left->c_region ||here->c_region != below->c_region) {
		    found_edge = 1;
		}
	    }
	    if (both_sides && right && above) {
		if (here->c_region != right->c_region || here->c_region != above->c_region) {
		    found_edge = 1;
		}
	    }
	}

	if (detect_distance) {
	    if (left && below) {
		if (Abs(here->c_dist - left->c_dist) > max_dist || Abs(here->c_dist - below->c_dist) > max_dist) {
		    found_edge = 1;
		}
	    }
	    if (both_sides && right && above) {
		if (Abs(here->c_dist - right->c_dist) > max_dist || Abs(here->c_dist - above->c_dist) > max_dist) {
		    found_edge = 1;
		}
	    }
	}

	if (detect_normals) {
	    if (left && below) {
		if ((VDOT(here->c_normal, left->c_normal) < COSTOL) || (VDOT(here->c_normal, below->c_normal)< COSTOL)) {
		    found_edge = 1;
		}
	    }
	    if (both_sides && right && above) {
		if ((VDOT(here->c_normal, right->c_normal)< COSTOL) || (VDOT(here->c_normal, above->c_normal)< COSTOL)) {
		    found_edge = 1;
		}
	    }
	}
    } else {
	if (left && below) {
	    if (left->c_ishit || below->c_ishit) {
		found_edge = 1;
	    }
	}
	if (both_sides && right && above) {
	    if (right->c_ishit || above->c_ishit) {
		found_edge = 1;
	    }
	}
    }

    if (found_edge) {
	if (intensity && ap && antialias)
	    get_intensity(intensity, ap, here, left, below, right, above);
	return 1;
    }
    return 0;
}


int
handle_main_ray(struct application *ap, register struct partition *PartHeadp,
		struct seg *segp)
{
    register struct partition *pp;
    register struct hit *hitp; /* which hit */

    struct application a2;
    struct cell me = CELL_INIT;
    struct cell below = CELL_INIT;
    struct cell left = CELL_INIT;

    struct cell above = CELL_INIT;
    struct cell right = CELL_INIT;

    double intensity = 1.0;

    int edge = 0;
    int cpu;
    int oc = 1;

    RGBpixel col;

    RT_APPLICATION_INIT(&a2);
    memset(&me, 0, sizeof(struct cell));
    memset(&below, 0, sizeof(struct cell));
    memset(&left, 0, sizeof(struct cell));

    if (ap->a_resource->re_cpu > 0)
	cpu = ap->a_resource->re_cpu - 1;
    else
	cpu = ap->a_resource->re_cpu;

    if (PartHeadp == NULL || segp == NULL) {
	/* The main shotline missed.  pack the application struct
	 */
	me.c_ishit    = 0;
	me.c_dist   = MISS_DIST;
	me.c_id	    = MISS_ID;
	me.c_region = 0;
	VSETALL(me.c_hit, MISS_DIST);
	VSETALL(me.c_normal, 0);
	VMOVE(me.c_rdir, ap->a_ray.r_dir);
    } else {
	pp = PartHeadp->pt_forw;
	hitp = pp->pt_inhit;
	/*
	 * Stuff the information for this cell.
	 */
	me.c_ishit    = 1;
	me.c_id = pp->pt_regionp->reg_regionid;
	me.c_dist = hitp->hit_dist;
	me.c_region = pp->pt_regionp;
	VMOVE(me.c_rdir, ap->a_ray.r_dir);
	VJOIN1(me.c_hit, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
	RT_HIT_NORMAL(me.c_normal, hitp,
		      pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);
    }

    /*
     * Now, fire a ray for both the cell below and if necessary, the
     * cell to the left.
     */
    a2.a_hit = rayhit2;
    a2.a_miss = raymiss2;
    a2.a_onehit = 1;
    a2.a_rt_i = ap->a_rt_i;
    a2.a_resource = ap->a_resource;
    a2.a_logoverlap = ap->a_logoverlap;

    VSUB2(a2.a_ray.r_pt, ap->a_ray.r_pt, dy_model); /* below */
    VMOVE(a2.a_ray.r_dir, ap->a_ray.r_dir);
    a2.a_uptr = (void *)&below;
    rt_shootray(&a2);

    if (ap->a_x == 0) {
	/*
	 * For the first pixel in a scanline, we have to shoot to the
	 * left.  For each pixel afterword, we save the current cell
	 * info to be used as the left side cell info for the
	 * following pixel
	 */
	VSUB2(a2.a_ray.r_pt, ap->a_ray.r_pt, dx_model); /* left */
	VMOVE(a2.a_ray.r_dir, ap->a_ray.r_dir);
	a2.a_uptr = (void *)&left;
	rt_shootray(&a2);
    } else {
	left.c_ishit = saved[cpu]->c_ishit;
	left.c_id = saved[cpu]->c_id;
	left.c_dist = saved[cpu]->c_dist;
	left.c_region = saved[cpu]->c_region;
	VMOVE(left.c_rdir, saved[cpu]->c_rdir);
	VMOVE(left.c_hit, saved[cpu]->c_hit);
	VMOVE(left.c_normal, saved[cpu]->c_normal);
    }

    if (both_sides) {
	VADD2(a2.a_ray.r_pt, ap->a_ray.r_pt, dy_model); /* above */
	VMOVE(a2.a_ray.r_dir, ap->a_ray.r_dir);
	a2.a_uptr = (void *)&above;
	rt_shootray(&a2);

	VADD2(a2.a_ray.r_pt, ap->a_ray.r_pt, dx_model); /* right */
	VMOVE(a2.a_ray.r_dir, ap->a_ray.r_dir);
	a2.a_uptr = (void *)&right;
	rt_shootray(&a2);
    }


    /*
     * Is this pixel an edge?
     */
    if (both_sides) {
	edge = is_edge(&intensity, ap, &me, &left, &below, &right, &above);
    } else {
	edge = is_edge(&intensity, ap, &me, &left, &below, NULL, NULL);
    }

    /*
     * Does this pixel occlude the second geometry?  Note that we must
     * check on edges as well since right side and top edges are
     * actually misses.
     */
    if (occlusion_mode != OCCLUSION_MODE_NONE) {
	if (me.c_ishit || edge) {
	    oc = occludes(ap, &me);
	}
    }

    /*
     * Perverse Pixel Painting Paradigm(tm) If a pixel should be
     * written to the fb, writeable is set.
     */
    if (occlusion_mode == OCCLUSION_MODE_EDGES)
	writeable[cpu][ap->a_x] = (edge && oc);
    else if (occlusion_mode == OCCLUSION_MODE_HITS)
	writeable[cpu][ap->a_x] = ((me.c_ishit || edge) && oc);
    else if (occlusion_mode == OCCLUSION_MODE_DITHER) {
	if (edge && oc)
	    writeable[cpu][ap->a_x] = 1;
	else if (me.c_ishit && oc) {
	    /*
	     * Dither mode.
	     *
	     * For occluding non-edges, only write every other pixel.
	     */
	    if (oc == 1 && ((ap->a_x + ap->a_y) % 2) == 0)
		writeable[cpu][ap->a_x] = 1;
	    else if (oc == 2)
		writeable[cpu][ap->a_x] = 1;
	    else
		writeable[cpu][ap->a_x] = 0;
	} else {
	    writeable[cpu][ap->a_x] = 0;
	}
    } else {
	if (edge)
	    writeable[cpu][ap->a_x] = 1;
	else
	    writeable[cpu][ap->a_x] = 0;
    }

    if (edge) {
	if (both_sides) {
	    choose_color(col, intensity, &me, &left, &below, &right, &above);
	} else {
	    choose_color(col, intensity, &me, &left, &below, NULL, NULL);
	}

	scanline[cpu][ap->a_x*3+RED] = col[RED];
	scanline[cpu][ap->a_x*3+GRN] = col[GRN];
	scanline[cpu][ap->a_x*3+BLU] = col[BLU];
    } else {
	scanline[cpu][ap->a_x*3+RED] = bgcolor[RED];
	scanline[cpu][ap->a_x*3+GRN] = bgcolor[GRN];
	scanline[cpu][ap->a_x*3+BLU] = bgcolor[BLU];
    }

    /*
     * Save the cell info for the next pixel.
     */
    saved[cpu]->c_ishit = me.c_ishit;
    saved[cpu]->c_id = me.c_id;
    saved[cpu]->c_dist = me.c_dist;
    saved[cpu]->c_region = me.c_region;
    VMOVE(saved[cpu]->c_rdir, me.c_rdir);
    VMOVE(saved[cpu]->c_hit, me.c_hit);
    VMOVE(saved[cpu]->c_normal, me.c_normal);

    return edge;
}


void application_init(void) {
    bu_vls_trunc(&occlusion_objects, 0);

    /* Set the byte offsets at run time */
    view_parse[ 0].sp_offset = bu_byteoffset(detect_regions);
    view_parse[ 1].sp_offset = bu_byteoffset(detect_regions);
    view_parse[ 2].sp_offset = bu_byteoffset(detect_distance);
    view_parse[ 3].sp_offset = bu_byteoffset(detect_distance);
    view_parse[ 4].sp_offset = bu_byteoffset(detect_normals);
    view_parse[ 5].sp_offset = bu_byteoffset(detect_normals);
    view_parse[ 6].sp_offset = bu_byteoffset(detect_ids);
    view_parse[ 7].sp_offset = bu_byteoffset(detect_ids);
    view_parse[ 8].sp_offset = bu_byteoffset(float_fgcolor[0]);
    view_parse[ 9].sp_offset = bu_byteoffset(float_fgcolor[0]);
    view_parse[10].sp_offset = bu_byteoffset(background[0]);
    view_parse[11].sp_offset = bu_byteoffset(background[0]);
    view_parse[12].sp_offset = bu_byteoffset(overlay);
    view_parse[13].sp_offset = bu_byteoffset(overlay);
    view_parse[14].sp_offset = bu_byteoffset(blend);
    view_parse[15].sp_offset = bu_byteoffset(blend);
    view_parse[16].sp_offset = bu_byteoffset(region_colors);
    view_parse[17].sp_offset = bu_byteoffset(region_colors);
    view_parse[18].sp_offset = bu_byteoffset(occlusion_objects);
    view_parse[19].sp_offset = bu_byteoffset(occlusion_objects);
    view_parse[20].sp_offset = bu_byteoffset(occlusion_mode);
    view_parse[21].sp_offset = bu_byteoffset(occlusion_mode);
    view_parse[22].sp_offset = bu_byteoffset(max_dist);
    view_parse[23].sp_offset = bu_byteoffset(max_dist);
    view_parse[24].sp_offset = bu_byteoffset(antialias);
    view_parse[25].sp_offset = bu_byteoffset(antialias);
    view_parse[26].sp_offset = bu_byteoffset(both_sides);
    view_parse[27].sp_offset = bu_byteoffset(both_sides);
    view_parse[28].sp_offset = bu_byteoffset(draw_axes);
    view_parse[29].sp_offset = bu_byteoffset(draw_axes);

    option("", "-c \"command\"", "Customize behavior (see rtedge manual)", 1);
    option("Raytrace", "-i", "Enable incremental (progressive-style) rendering", 1);
    option("Raytrace", "-t", "Render from top to bottom (default: from bottom up)", 1);

    /* this reassignment hack ensures help is last in the first list */
    option("dummy", "-? or -h", "Display help", 1);
    option("", "-? or -h", "Display help", 1);
}


int diffpixel(RGBpixel a, RGBpixel b)
{
    if (a[RED] != b[RED]) return 1;
    if (a[GRN] != b[GRN]) return 1;
    if (a[BLU] != b[BLU]) return 1;
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
