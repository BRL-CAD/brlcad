/*                      H E A T G R A P H . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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

#include <sys/types.h>
#include <sys/resource.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "plot3.h"
#include "scanline.h"

#include "./rtuif.h"
#include "./ext.h"


static struct timeval timeStart;	/* These are pulled directly from timer42 */
static struct rusage ruA;		/*  Resource utilization at the start */
static struct rusage ruAc;

static void tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0);

/* Time functions are copied almost verbatium from timer42.c */

/**
 * P R E P _ P I X E L _ T I M E R
 * 
 * Prep the timer for each pixel 
 */
void prep_pixel_timer(void)
{
    gettimeofday(&timeStart, (struct timezone *)0);
    getrusage(RUSAGE_SELF, &ruA);
    getrusage(RUSAGE_CHILDREN, &ruAc);
}

/**
 * T V S U B 
 */

static void
tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{
    tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
    tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
    if (tdiff->tv_usec < 0) {
	tdiff->tv_sec--, tdiff->tv_usec += 1000000;
    }
}

/**
 * G E T _ P I X E L _ T I M E R
 * 
 * Get the length of time the pixel
 * was being made.
 */
double
get_pixel_timer(double *total)
{
    struct timeval timeEnd;
    /* Pulled directly from timer42.c */
    struct rusage ru1;
    struct rusage ru1c;
    struct timeval td;
    double totalTime = 0;
    double elapsed_secs = 0;
    
    getrusage(RUSAGE_SELF, &ru1);
    getrusage(RUSAGE_CHILDREN, &ru1c);
    gettimeofday(&timeEnd, (struct timezone *)0);
    
    elapsed_secs = (timeEnd.tv_sec - timeStart.tv_sec) + 
	(timeEnd.tv_usec - timeStart.tv_usec)/1000000.0;
    
    tvsub(&td, &ru1.ru_utime, &ruA.ru_utime);
    totalTime = td.tv_sec + ((double)td.tv_usec)/1000000.0;
    
    tvsub(&td, &ru1c.ru_utime, &ruAc.ru_utime);
    totalTime += td.tv_sec + ((double)td.tv_usec) / 1000000.0;
    
    if (totalTime < 0.00001) totalTime = 0.00001;
    if (elapsed_secs < 0.00001) elapsed_secs = totalTime;

    if (total) *total = totalTime;
    
    return totalTime;
}


/**
 * T I M E T A B L E _ I N I T
 * 
 * This function creates the table of values that will store the 
 * time taken to complete pixels during a raytrace. Returns a
 * pointer to the table.
 */
fastf_t**
timeTable_init(int x, int y)
{
    /*
     * Time table will be initialized to the size of the current
     * framebuffer by using a malloc.
     * So first we need to get the size of the framebuffer
     * height and width and use that as the starting point.
     */

    static fastf_t **timeTable = NULL;
    if (x && y) {
	int i=0;
	int w=0;

	/* Semaphore Acquire goes here */

	if (timeTable == NULL) {
	    bu_log("X is %d, Y is %d\n", x, y);
	    bu_log("Making time Table\n");
	    timeTable = bu_malloc(x * sizeof(double *), "timeTable");
	    for (i = 0; i < x; i++) {
		timeTable[i] = bu_malloc(y * sizeof(double), "timeTable[i]");
	    }
	    for (i = 0; i < x; i++) {
		for (w = 0; w < y; w++) {
		    timeTable[i][w] = -1;
		}
	    }
	    bu_log("Initialized timetable\n");
	}
	/* Semaphore release goes here */
    }
    return timeTable;
}


/**
 * T I M E T A B L E _ F R E E
 *
 * Frees up the time table array.
 */
void 
timeTable_free(fastf_t **timeTable)
{
    /* Temporarily assigned variables, until real ones are found */
    int y = height;
    int i = 0;
    
    for (i = 0; i < y; i++)
	bu_free(timeTable[i], "timeTable[]");
    bu_free(timeTable, "timeTable");
}


/**
 * T I M E T A B L E _ I N P U T 
 *
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
    if ((size_t)y > height)
	bu_log("Error, putting in values greater than height!\n");
    if ((size_t)x > width)
	bu_log("Error, putting in values greater than width!\n");
    timeTable[x][y] = timeval;
    /* bu_log("Input %lf into timeTable %d %d\n", timeval, x, y); */
}


/**
 * T I M E T A B L E _ S I N G L E P R O C E S S
 * 
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
    } else {     /* Error occured with time, color pixel Green */
	Rcolor = Bcolor = 0;
	Gcolor = 255;
    }
    
    timeColor[0] = Rcolor;
    timeColor[1] = Gcolor;
    timeColor[2] = Bcolor;
    return timeColor;
}


/**
 * T I M E T A B L E _ P R O C E S S
 * 
 * This function takes the contents of the time table, and produces the 
 * heat graph based on time taken for each pixel.
 */
void 
timeTable_process(fastf_t **timeTable, struct application *UNUSED(app), FBIO *fbp)
{
    fastf_t maxTime = -MAX_FASTF;		/* The 255 value */
    fastf_t minTime = MAX_FASTF; 		/* The 1 value */
    fastf_t totalTime = 0;
    fastf_t meanTime = 0;			/* the 128 value */
    fastf_t range = 0;				/* All times should fall in this range */
    int pixels = 0;				/* Used for calculating averages */
    RGBpixel p;					/* Pixel colors for particular pixel */
    int maxX = width;
    int maxY = height; 	/* Maximum render size, uses evil globals */
    int Rcolor = 0;
    int Gcolor = 0;
    int Bcolor = 0;
    int npix = 0;
    int zoomH = 0;
    int zoomW = 0;

    /* The following loop will work as follows, it will loop through
     * timeTable and search for pixels which have a non-negative value.
     * Once a value is found, it will assign a color value from 1-255,
     * depending on time taken. ((time / maxTime) * 255), or similar
     * function.
     */
    int x = 0, y = 0;
    bu_log("MaxX =%d MaxY =%d\n", maxX, maxY);
    for (x = 0; x < maxX; x++) {
	for (y = 0; y < maxY; y++) {
	    if (!(timeTable[x][y] < 0.0)) {
		/* Semaphore acquire goes here */
		if (timeTable[x][y] > maxTime)
		    maxTime = timeTable[x][y];
		if (timeTable[x][y] < minTime)
		    minTime = timeTable[x][y];
		/* Semaphore release goes here */
		pixels++;
		totalTime += timeTable[x][y];
	    }
	}
    }
    meanTime = totalTime / pixels;
    range = maxTime - minTime;
    bu_log("Time:%lf Max = %lf Min = %lf Mean = %lf Range = %lf\n", totalTime, maxTime, minTime, meanTime, range);

    /* Now fill out the framebuffer with the Heat Graph information */

    for (x = 0; x < maxX; x++) {
	for (y = 0; y < maxY; y++) {
	    if (timeTable[x][y] < minTime) {
		/* error pixels, time is less than minTime */
		Rcolor = 0;
		Gcolor = 255;
		Bcolor = 0;
	    }
	    if (timeTable[x][y] < minTime || EQUAL(timeTable[x][y], minTime)) {
		Rcolor = Gcolor = Bcolor = 1;
	    }
	    if (timeTable[x][y] > minTime
		&& (timeTable[x][y] < maxTime || EQUAL(timeTable[x][y], maxTime))) {
		Rcolor = Gcolor = Bcolor = (255/range)*timeTable[x][y];
	    }
	    if (timeTable[x][y] > maxTime)
		Rcolor = 255;
	    
	    p[0]=Rcolor;
	    p[1]=Gcolor;
	    p[2]=Bcolor;
	    
	    if (fbp != FBIO_NULL) {
		bu_semaphore_acquire(BU_SEM_SYSCALL);
		npix = fb_write(fbp, x, y, (unsigned char *)p, 1);
		bu_semaphore_release(BU_SEM_SYSCALL);
		if (npix < 1)
		    bu_exit(EXIT_FAILURE, "pixel fb_write error");
	    }
 	}
    }
    zoomH = fb_getheight(fbp) / height;
    zoomW = fb_getwidth(fbp) / width;
    (void)fb_view(fbp, width/2, height/2, zoomH, zoomW);
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
