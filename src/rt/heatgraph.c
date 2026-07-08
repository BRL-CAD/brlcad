/*                      H E A T G R A P H . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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

/** @file rt/heatgraph.c
 *
 * Holds information on the time table used for the heat graph, which
 * is a light mode used in view.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "bu/log.h"
#include "bu/parallel.h"
#include "icv.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm.h"
#include "bv/plot3.h"
#include "scanline.h"

#include "./rtuif.h"
#include "./ext.h"


static fastf_t **heatgraph_table = NULL;
static size_t table_width = 0;
static size_t table_height = 0;


static unsigned char
heat_pixel_value(fastf_t timeval, int positivePixels, fastf_t minPositiveTime, fastf_t maxTime, fastf_t range)
{
    fastf_t norm = 0.0;

    if (timeval < 0.0)
	return 0;

    if (positivePixels == 0) {
	norm = 0.0;
    } else if (range <= SMALL_FASTF) {
	norm = (timeval > 0.0) ? 1.0 : 0.0;
    } else if (timeval > 0.0) {
	/* Timing values tend to cluster near zero.  Normalize the
	 * distance from the fastest measured pixel on a log scale so
	 * microsecond-level variation remains visible. */
	if (minPositiveTime > SMALL_FASTF) {
	    fastf_t denom = log1p((maxTime - minPositiveTime) / minPositiveTime);
	    if (denom > SMALL_FASTF)
		norm = log1p((timeval - minPositiveTime) / minPositiveTime) / denom;
	    else
		norm = (timeval - minPositiveTime) / range;
	}
	else
	    norm = (timeval - minPositiveTime) / range;
    }

    if (norm < 0.0)
	norm = 0.0;
    if (norm > 1.0)
	norm = 1.0;

    return (unsigned char)((norm * 254.0) + ((timeval > 0.0) ? 1 : 0));
}


/**
 * This function creates the table of values that will store the
 * time taken to complete pixels during a raytrace. Returns a
 * pointer to the table.
 */
fastf_t**
timeTable_init(size_t x, size_t y)
{
    /*
     * Time table will be initialized to the size of the current
     * framebuffer by using a malloc.
     * So first we need to get the size of the framebuffer
     * height and width and use that as the starting point.
     */

    if (x && y) {
	size_t i=0;
	size_t w=0;

	/* Semaphore Acquire goes here */
	if (heatgraph_table != NULL && (x != table_width || y != table_height)) {
	    for (i = 0; i < table_width; i++)
		bu_free(heatgraph_table[i], "timeTable[]");
	    bu_free(heatgraph_table, "timeTable");
	    heatgraph_table = NULL;
	    table_width = 0;
	    table_height = 0;
	}
	if (heatgraph_table == NULL) {
	    bu_log("Initializing heatgraph timers (%zu x %zu)\n", x, y);
	    heatgraph_table = (fastf_t **)bu_malloc(x * sizeof(fastf_t *), "timeTable");
	    for (i = 0; i < x; i++) {
		heatgraph_table[i] = (fastf_t *)bu_malloc(y * sizeof(fastf_t), "timeTable[i]");
	    }
	    table_width = x;
	    table_height = y;
	    for (i = 0; i < x; i++) {
		for (w = 0; w < y; w++) {
		    heatgraph_table[i][w] = -1;
		}
	    }

	}
	/* Semaphore release goes here */
    }
    return heatgraph_table;
}


/**
 * Clears the time table for a new frame.
 */
void
timeTable_clear(size_t x, size_t y)
{
    fastf_t **timeTable = timeTable_init(x, y);
    size_t i = 0;
    size_t w = 0;

    if (!timeTable)
	return;

    for (i = 0; i < x; i++) {
	for (w = 0; w < y; w++) {
	    timeTable[i][w] = -1;
	}
    }
}


/**
 * Frees up the time table array.
 */
void
timeTable_free(fastf_t **tt)
{
    size_t i = 0;

    if (!tt)
	return;

    for (i = 0; i < table_width; i++)
	bu_free(tt[i], "timeTable[]");
    bu_free(tt, "timeTable");
    if (tt == heatgraph_table)
	heatgraph_table = NULL;
    table_width = 0;
    table_height = 0;
}


/**
 * This function inputs the time taken to complete a pixel during a
 * raytrace and places it into the timeTable for use in creating a
 * heat graph light model.
 */
void
timeTable_input(int x, int y, fastf_t timeval, fastf_t **timeTable)
{
    /* bu_log("Enter timeTable input\n"); */
    if (x < 0)
	x = 0;
    if (y < 0)
	y = 0;
    if ((size_t)y >= height) {
	bu_log("Error, putting in values greater than height!\n");
	return;
    }
    if ((size_t)x >= width) {
	bu_log("Error, putting in values greater than width!\n");
	return;
    }
    timeTable[x][y] = timeval;
    /* bu_log("Input %lf into timeTable %d %d\n", timeval, x, y); */
}


/**
 * This function processes the time table 1 pixel at a time, as
 * opposed to all at once like timeTable_process. Heat values are
 * bracketed to certain values, instead of normalized.
 */
fastf_t *
timeTable_singleProcess(struct application *app, fastf_t **timeTable, fastf_t *timeColor)
{
    /* Process will take current X Y and time from timeTable, and apply
     * color to that pixel inside the framebuffer.
     */
    fastf_t timeval = timeTable[app->a_x][app->a_y];
    fastf_t Rcolor = 0;	/* 1-255 value of color */
    fastf_t Gcolor = 0;	/* 1-255 value of color */
    fastf_t Bcolor = 0;	/* 1-255 value of color */

    /* bu_log("Time is %lf :", timeval); */

    /* Eventually the time taken will also span the entire color spectrum (0-255!)
     * For now, the darkest color (1,1,1) will be set to any time slower or equal
     * to 0.00001 sec, and (255,255,255) will be set to any time longer than 0.01 sec,
     * making a gradient of black-white in between.
     */

    if (timeval < 0.00001 || EQUAL(timeval, 0.00001)) {
	Rcolor = 1;
	Gcolor = 1;
	Bcolor = 1;
    } else if (timeval > 0.00001 && timeval < 0.01) {
	Rcolor = Gcolor = Bcolor = (timeval*1000)*255;
	if (Rcolor >= 255)
	    Rcolor = Gcolor = Bcolor = 254;
	if (Rcolor <= 1)
	    Rcolor = Gcolor = Bcolor = 2;
    } else if (timeval > 0.01) {
	Rcolor = Gcolor = Bcolor = 255;
    } else {     /* Error occurred with time, color pixel Green */
	Rcolor = Bcolor = 0;
	Gcolor = 255;
    }

    timeColor[0] = Rcolor;
    timeColor[1] = Gcolor;
    timeColor[2] = Bcolor;
    return timeColor;
}


/**
 * This function takes the contents of the time table, and produces the
 * heat graph based on time taken for each pixel.
 */
void
timeTable_process(fastf_t **timeTable, struct application *UNUSED(app), struct fb *out_fbp)
{
    fastf_t maxTime = -MAX_FASTF;		/* The 255 value */
    fastf_t minTime = MAX_FASTF; 		/* The 1 value */
    fastf_t minPositiveTime = MAX_FASTF;
    fastf_t totalTime = 0;
    fastf_t meanTime = 0;			/* the 128 value */
    fastf_t range = 0;				/* All times should fall in this range */
    int pixels = 0;				/* Used for calculating averages */
    int positivePixels = 0;
    size_t maxX = width;
    size_t maxY = height; 	/* Maximum render size, uses evil globals */
    ssize_t npix = 0;
    int zoomH = 0;
    int zoomW = 0;
    int outfp_error = 0;
    RGBpixel *line = NULL;

    /* The following loop will work as follows, it will loop through
     * timeTable and search for pixels which have a non-negative value.
     * Once a value is found, it will assign a color value from 1-255,
     * depending on time taken. ((time / maxTime) * 255), or similar
     * function.
     */
    size_t x = 0, y = 0;
    if (timeTable == NULL) {
	bu_log("ERROR: heatgraph timer table is not initialized\n");
	return;
    }
    bu_log("MaxX =%zu MaxY =%zu\n", maxX, maxY);
    for (x = 0; x < maxX; x++) {
	for (y = 0; y < maxY; y++) {
	    if (!(timeTable[x][y] < 0.0)) {
		/* Semaphore acquire goes here */
		if (timeTable[x][y] > maxTime)
		    maxTime = timeTable[x][y];
		if (timeTable[x][y] < minTime)
		    minTime = timeTable[x][y];
		if (timeTable[x][y] > 0.0 && timeTable[x][y] < minPositiveTime)
		    minPositiveTime = timeTable[x][y];
		/* Semaphore release goes here */
		pixels++;
		if (timeTable[x][y] > 0.0)
		    positivePixels++;
		totalTime += timeTable[x][y];
	    }
	}
    }
    if (pixels == 0) {
	bu_log("ERROR: heatgraph timer table has no populated pixels\n");
	return;
    }
    meanTime = totalTime / pixels;
    if (positivePixels > 0 && minPositiveTime < MAX_FASTF)
	range = maxTime - minPositiveTime;
    else
	range = maxTime - minTime;
    bu_log("Time: %lf\n  Max = %lf\n  Min = %lf\n  Mean = %lf\n  Range = %lf\n", totalTime, maxTime, minTime, meanTime, range);

    /* Now fill out the framebuffer with the Heat Graph information */
    line = (RGBpixel *)bu_malloc(maxX * sizeof(RGBpixel), "heatgraph scanline");

    for (y = 0; y < maxY; y++) {
	for (x = 0; x < maxX; x++) {
	    fastf_t timeval = timeTable[x][y];
	    unsigned char value = heat_pixel_value(timeval, positivePixels, minPositiveTime, maxTime, range);

	    line[x][RED] = value;
	    line[x][GRN] = value;
	    line[x][BLU] = value;
	}

	if (bif != NULL && icv_writeline(bif, y, (unsigned char *)line, ICV_DATA_UCHAR) != 0)
	    bu_log("WARNING: heatgraph icv_writeline error at row %zu\n", y);

	if (outfp != NULL && !outfp_error) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    if (bu_fseek(outfp, y * maxX * sizeof(RGBpixel), 0) != 0 ||
		fwrite(line, sizeof(RGBpixel), maxX, outfp) != maxX) {
		outfp_error = 1;
		bu_log("WARNING: heatgraph file write error\n");
	    }
	    bu_semaphore_release(BU_SEM_SYSCALL);
	}

	if (out_fbp != FB_NULL) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    npix = fb_write(out_fbp, 0, (int)y, (unsigned char *)line, maxX);
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    if (npix != (ssize_t)maxX)
		bu_exit(EXIT_FAILURE, "heatgraph fb_write error");
	}
    }
    bu_free(line, "heatgraph scanline");

    if (out_fbp != FB_NULL) {
      zoomH = fb_getheight(out_fbp) / height;
      zoomW = fb_getwidth(out_fbp) / width;
      (void)fb_view(out_fbp, width/2, height/2, zoomH, zoomW);
    }
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
