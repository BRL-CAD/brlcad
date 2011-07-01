/*                      V I E W E D G E . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2011 United States Government as represented by
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
 * book-keeping with 2x as much work occuring as necessary on default
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
#include "fb.h"
#include "bu.h"

#include "./rtuif.h"
#include "./ext.h"


#define COSTOL 0.91    /* normals differ if dot product < COSTOL */
#define OBLTOL 0.1     /* high obliquity if cosine of angle < OBLTOL ! */
#define is_Odd(_a)      ((_a)&01)
#define ARCTAN_87 19.08

#ifndef Abs
#  define Abs(x) ((x) < 0 ? -(x) : (x))                  /* UNSAFE */
#endif


extern FBIO *fbp;	/* Framebuffer handle */
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
color fgcolor = {255, 255, 255};
color bgcolor = {0, 0, 0};

/*
 * Flags that set which edges are detected.
 *
 * detect_ids -> detect boundaries region id codes.
 * detect_regions -> detect region boundaries.
 * detect_distance -> detect noticable differences in hit distance.
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


struct bu_vls occlusion_objects;
struct rt_i *occlusion_rtip = NULL;
struct application **occlusion_apps;


/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"%d", 1, "detect_regions", bu_byteoffset(detect_regions), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "dr", bu_byteoffset(detect_regions), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "detect_distance", bu_byteoffset(detect_distance), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "dd", bu_byteoffset(detect_distance), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "detect_normals", bu_byteoffset(detect_normals), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "dn", bu_byteoffset(detect_normals), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "detect_ids", bu_byteoffset(detect_ids), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "di", bu_byteoffset(detect_ids), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 3, "foreground", bu_byteoffset(fgcolor), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 3, "fg", bu_byteoffset(fgcolor), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 3, "background", bu_byteoffset(bgcolor), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 3, "bg", bu_byteoffset(bgcolor), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "overlay", bu_byteoffset(overlay), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "ov", bu_byteoffset(overlay), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "blend", bu_byteoffset(blend), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "bl", bu_byteoffset(blend), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "region_color", bu_byteoffset(region_colors), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "rc", bu_byteoffset(region_colors), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%V", 1, "occlusion_objects", bu_byteoffset(occlusion_objects), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%V", 1, "oo", bu_byteoffset(occlusion_objects), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "occlusion_mode", bu_byteoffset(occlusion_mode), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "om", bu_byteoffset(occlusion_mode), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 1, "max_dist", bu_byteoffset(max_dist), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%f", 1, "md", bu_byteoffset(max_dist), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "antialias", bu_byteoffset(antialias), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "aa", bu_byteoffset(antialias), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "both_sides", bu_byteoffset(both_sides), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"%d", 1, "bs", bu_byteoffset(both_sides), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


const char title[] = "RT Hidden-Line Renderer";

void
usage(const char *argv0)
{
    bu_log("Usage:  %s [options] model.g objects... >file.pix\n", argv0);
    bu_log("Options:\n");
    bu_log(" -r		Report overlaps (default)\n");
    bu_log(" -R		Do not report overlaps\n");
    bu_log(" -M		Read matrix+commands on stdin\n");
    bu_log(" -o model.pix	Output .pix format file (default is window)\n");
    bu_log(" -s #		Square grid size in pixels (default is 512)\n");
    bu_log(" -w # -n #	Grid size width (w) and height (n) in pixels\n");
    bu_log(" -a # -e #	Azimuth (a) and elevation (e) in degrees\n");
    bu_log(" -V #		View (pixel) aspect ratio (width/height)\n");
    bu_log(" -p #		Perspective angle, degrees side to side\n");
    bu_log(" -P #		Set number of processors\n");
    bu_log(" -T #/#		Tolerance: distance/angular\n");
    bu_log(" -l #		Set lighting model rendering style\n");
    bu_log(" -U #		Use air if # is greater than 0\n");
    bu_log(" -x #		librt debug flags\n");
    bu_log(" -N #		NMG debug flags\n");
    bu_log(" -X #		rt debug flags\n");
    bu_log(" -c		Auxillary commands (see man page)\n");
}


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
    int cpu = ap->a_resource->re_cpu;
    int oc_hit = 0;
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

    if (occlusion_apps[cpu]->a_dist < here->c_dist) {
	/*
	 * The second geometry is close than the edge, therefore it is
	 * 'foreground'. Do not draw the edge.
	 *
	 * This pixel DOES NOT occlude the second geometry.
	 */
	return 0;
    }

    return 1;
}


/**
 * R A Y H I T
 */
int rayhit(struct application *ap, register struct partition *pt,
	   struct seg *segp)
{
    handle_main_ray(ap, pt, segp);
    return 1;
}


/**
 * R A Y M I S S
 */
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
	int nObjs;
	const char **objs;
	int i;

	bu_log("rtedge: loading occlusion geometry from %s.\n", file);

	if (Tcl_SplitList(NULL, bu_vls_addr(&occlusion_objects), &nObjs,
			  &objs) == TCL_ERROR) {
	    bu_log("rtedge: occlusion list = %s\n",
		   bu_vls_addr(&occlusion_objects));
	    bu_exit(EXIT_FAILURE, "rtedge: could not parse occlusion objects list.\n");
	}

	for (i=0; i<nObjs; ++i) {
	    bu_log("rtedge: occlusion object %d = %s\n", i, objs[i]);
	}


	if ((dbip = db_open(file, "r")) == DBI_NULL)
	    bu_exit(EXIT_FAILURE, "rtedge: could not open database.\n");
	RT_CK_DBI(dbip);

	occlusion_rtip = rt_new_rti(dbip); /* clones dbip */

	for (i=0; i < MAX_PSW; i++) {
	    rt_init_resource(&occlusion_resources[i], i, occlusion_rtip);
	    bn_rand_init(occlusion_resources[i].re_randptr, i);
	}

	db_close(dbip);			 /* releases original dbip */

	for (i=0; i<nObjs; ++i)
	    if (rt_gettree(occlusion_rtip, objs[i]) < 0)
		bu_log("rtedge: gettree failed for %s\n", objs[i]);
	    else
		bu_log("rtedge: got tree for object %d = %s\n", i, objs[i]);

	bu_log("rtedge: occlusion rt_gettrees done.\n");

	rt_prep(occlusion_rtip);

	bu_log("rtedge: occlustion prep done.\n");

	/*
	 * Create a set of application structures for the occlusion
	 * geometry. Need one per cpu, the upper half does the per-
	 * thread allocation in worker, but that's off limits.
	 */
	occlusion_apps = bu_calloc(npsw, sizeof(struct application *),
				   "occlusion application structure array");
	for (i=0; i<npsw; ++i) {
	    occlusion_apps[i] = bu_malloc(sizeof(struct application), "occlusion_application structure");

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

    /* if non-default/inverted background was requested, swap the
     * foreground and background colors.
     */
    if (!default_background) {
	color tmp;
	tmp[RED] = fgcolor[RED];
	tmp[GRN] = fgcolor[GRN];
	tmp[BLU] = fgcolor[BLU];
	fgcolor[RED] = bgcolor[RED];
	fgcolor[GRN] = bgcolor[GRN];
	fgcolor[BLU] = bgcolor[BLU];
	bgcolor[RED] = tmp[RED];
	bgcolor[GRN] = tmp[GRN];
	bgcolor[BLU] = tmp[BLU];
    }

    if (minus_o && (overlay || blend)) {
	/*
	 * Output is to a file stream.  Do not allow parallel
	 * processing since we can't seek to the rows.
	 */
	rt_g.rtg_parallel = 0;
	bu_log("view_init: deactivating parallelism due to -o option.\n");
	/*
	 * The overlay and blend cannot be used in -o mode.  Note that
	 * the overlay directive takes precendence, they can't be used
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
    int i;

    if (outputfile)
	bif = bu_image_save_open(outputfile, BU_IMAGE_AUTO, width, height, 3);

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
    for (i = 0; i < npsw; ++i) {
	if (saved[i] == NULL)
	    saved[i] = (struct cell *) bu_calloc(1, sizeof(struct cell), "saved cell info");
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
    int cpu = ap->a_resource->re_cpu;
    int i;

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
	return;
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

	for (i=0; i<per_processor_chunk; ++i) {
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
	return;
    } /* end blend */

    if (fbp != FBIO_NULL) {
	/*
	 * Simple whole scanline write to a framebuffer.
	 */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fb_write(fbp, 0, ap->a_y, scanline[cpu], per_processor_chunk);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (outputfile != NULL) {
	/*
	 * Write to a file.
	 */
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	bu_image_save_writeline(bif, ap->a_y, scanline[cpu]);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
    if (fbp == FBIO_NULL && outputfile == NULL)
	bu_log("rtedge: strange, no end of line actions taken.\n");

    return;

}


void view_setup(struct rt_i *UNUSED(rtip))
{
}


/**
 * end of a frame, called after rt_clean()
 */
void view_cleanup(struct rt_i *UNUSED(rtip))
{
}


/**
 * end of each frame
 */
void view_end(struct application *UNUSED(ap)) { 
    if (bif)
	bu_image_save_close(bif); 
    bif = NULL;
}


/**
 * R A Y H I T 2
 */
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


/**
 * R A Y M I S S 2
 */
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
     * |0, 0 | AL | AR | 0, 3|  above
     * |____|____|____|____|
     * | TL | UL | UR | TR |  top/upper
     * |____|____|____|____|
     * | BL | LL | LR | BR |  bottom/lower
     * |____|____|____|____|
     * |3, 0 | DL | DR | 3, 3|  debajo
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
    aaap.a_uptr = (genptr_t)AL;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy3);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("AL", aaap.a_ray.r_pt);
#endif

    /* Above Right */
    aaap.a_uptr = (genptr_t)AR;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy3);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("AR", aaap.a_ray.r_pt);
#endif

    /* Top Left */
    aaap.a_uptr = (genptr_t)TL;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx3);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("TL", aaap.a_ray.r_pt);
#endif

    /* Upper Left */
    aaap.a_uptr = (genptr_t)UL;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("UL", aaap.a_ray.r_pt);
#endif

    /* Upper Right */
    aaap.a_uptr = (genptr_t)UR;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("UR", aaap.a_ray.r_pt);
#endif

    /* Top Right */
    aaap.a_uptr = (genptr_t)TR;
    VADD2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx3);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("TR", aaap.a_ray.r_pt);
#endif

    /* Bottom Left */
    aaap.a_uptr = (genptr_t)BL;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx3);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("BL", aaap.a_ray.r_pt);
#endif

    /* Lower Left */
    aaap.a_uptr = (genptr_t)LL;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("LL", aaap.a_ray.r_pt);
#endif

    /* Lower Right */
    aaap.a_uptr = (genptr_t)LR;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("LR", aaap.a_ray.r_pt);
#endif

    /* Bottom Right */
    aaap.a_uptr = (genptr_t)BR;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy);
    VADD2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx3);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("BR", aaap.a_ray.r_pt);
#endif

    /* Debajo Left */
    aaap.a_uptr = (genptr_t)DL;
    VSUB2(aaap.a_ray.r_pt, ap->a_ray.r_pt, dy3);
    VSUB2(aaap.a_ray.r_pt, aaap.a_ray.r_pt, dx);
    VMOVE(aaap.a_ray.r_dir, ap->a_ray.r_dir);
    rt_shootray(&aaap);

#ifdef VEDEBUG
    VPRINT("DL", aaap.a_ray.r_pt);
#endif

    /* Debajo Right */
    aaap.a_uptr = (genptr_t)DR;
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


/**
 * H A N D L E _ M A I N _ R A Y
 *
 */
int
handle_main_ray(struct application *ap, register struct partition *PartHeadp,
		struct seg *segp)
{
    register struct partition *pp;
    register struct hit *hitp; /* which hit */

    struct application a2;
    struct cell me;
    struct cell below;
    struct cell left;

    struct cell above;
    struct cell right;

    double intensity = 1.0;

    int edge = 0;
    int cpu;
    int oc = 1;

    RGBpixel col;

    RT_APPLICATION_INIT(&a2);
    memset(&me, 0, sizeof(struct cell));
    memset(&below, 0, sizeof(struct cell));
    memset(&left, 0, sizeof(struct cell));

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
    a2.a_uptr = (genptr_t)&below;
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
	a2.a_uptr = (genptr_t)&left;
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
	a2.a_uptr = (genptr_t)&above;
	rt_shootray(&a2);
 
	VADD2(a2.a_ray.r_pt, ap->a_ray.r_pt, dx_model); /* right */
	VMOVE(a2.a_ray.r_dir, ap->a_ray.r_dir);
	a2.a_uptr = (genptr_t)&right;
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
    if (occlusion_mode != OCCLUSION_MODE_NONE) 
	if (me.c_ishit || edge) 
	    oc = occludes(ap, &me);

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
    bu_vls_init(&occlusion_objects);
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
