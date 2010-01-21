/*                      H E A T G R A P H . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2010 United States Government as represented by
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

/** @file heatgraph.c
 *
 * Holds information on the time table used for the heat graph, which
 * is a light mode used in view.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "fb.h"
#include "rtprivate.h"
#include "./ext.h"
#include "plot3.h"
#include "photonmap.h"
#include "scanline.h"

extern FBIO *fbp;

/**
 * T I M E T A B L E _ I N I T
 * 
 * This function creates the table of values that will store the 
 * time taken to complete pixels during a raytrace. Returns a
 * pointer to the table.
 */
fastf_t *timeTable_init(FBIO *fbp, int a)
{
    /*
     * Time table will be initialized to the size of the current
     * framebuffer by using a malloc.
     * So first we need to get the size of the framebuffer
     * height and width and use that as the starting point.
     */

    static fastf_t **timeTable = NULL;
    if (a == NULL && fbp != NULL) {
	int x = fb_getwidth(fbp);
	int y = fb_getheight(fbp);
	/* bu_log("X is %d, Y is %d\n", x, y); */
	int i=0;
	int w=0;
	
	/* Semaphore Acquire goes here */
	
	if (timeTable == NULL) {
	    bu_log("Making time Table\n");
	    timeTable = bu_malloc(x * sizeof(fastf_t *), "timeTable");
	    for (i = 0; i < x; i++) {
		timeTable[i] = bu_malloc(y * sizeof(fastf_t *), "timeTable[i]");
	}    
	    for (i = 0; i < x; i++) {
		for (w = 0; w < y; w++) {
		    timeTable[i][w] = -1;
		    /* bu_log("Initializing table %d %d %lf\n", i, w, timeTable[i][w]); */
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
void timeTable_free(fastf_t **timeTable)
{
    /* Temporarily assigned variables, until real ones are found */
    int x = width;
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
void timeTable_input(int x, int y, fastf_t time, fastf_t **timeTable)
{
    /* bu_log("Enter timeTable input\n"); */
    timeTable[x][y] = time;
    /* bu_log("Input %lf into timeTable %d %d\n", time, x, y); */
}


/**
 * T I M E T A B L E _ S I N G L E P R O C E S S
 * 
 * This function processes the time table 1 pixel at a time, as
 * opposed to all at once like timeTable_process. Heat values are
 * bracketed to certain values, instead of normalized.
 */
fastf_t *timeTable_singleProcess(struct application *ap, fastf_t **timeTable, fastf_t *timeColor)
{
    /* Process will take current X Y and time from timeTable, and apply
     * color to that pixel inside the framebuffer.
     */
    fastf_t time = timeTable[ap->a_x][ap->a_y];
    fastf_t Rcolor = 0;	/* 1-255 value of color */
    fastf_t Gcolor = 0;	/* 1-255 value of color */
    fastf_t Bcolor = 0;	/* 1-255 value of color */

    /* bu_log("Time is %lf :", time); */

    /* Eventually the time taken will also span the entire color spectrum (0-255!)
     * For now, the darkest color (1,1,1) will be set to any time slower or equal
     * to 0.00001 sec, and (255,255,255) will be set to any time longer than 0.01 sec,
     * making a gradient of black-white in between.
     */

    if (time <= 0.00001) {
	Rcolor = 1;
	Gcolor = 1;
	Bcolor = 1;
    } else if (time > 0.00001 && time < 0.01) {
	Rcolor = Gcolor = Bcolor = (time*1000)*255;
	if (Rcolor >= 255)
	    Rcolor = Gcolor = Bcolor = 254;
	if (Rcolor <= 1)
	    Rcolor = Gcolor = Bcolor = 2;
    } else if (time > 0.01) {
	Rcolor = Gcolor = Bcolor = 255;
    } else {     /* Error occured with time, color pixel Green */
	Rcolor = Bcolor = 0;
	Gcolor = 255;
    }

    timeColor[0] = Rcolor;
    timeColor[1] = Gcolor;
    timeColor[2] = Bcolor;
    /* bu_log("Color=%d %d %d, %lf\n", Rcolor, Gcolor, Bcolor, time); */
    return timeColor;
}


/**
 * T I M E T A B L E _ P R O C E S S
 * 
 * This function takes the contents of the time table, and produces the 
 * heat graph based on time taken for each pixel.
 */
void timeTable_process(fastf_t **timeTable, struct application *ap)
{
    fastf_t maxTime = -MAX_FASTF;		/* The 255 value */
    fastf_t minTime = MAX_FASTF; 		/* The 1 value */
    fastf_t totalTime = 0;
    fastf_t meanTime = 0;			/* the 128 value */
    fastf_t range = 0;				/* All times should fall in this range */
    int pixels = 0;				/* Used for calculating averages */
    RGBpixel p;					/* Pixel colors for particular pixel */
    int maxX = width, maxY = height; 		/* Maximum render size, uses evil globals */

    /* The following loop will work as follows, it will loop through
     * timeTable and search for pixels which have a non-negative value.
     * Once a value is found, it will assign a color value from 1-255,
     * depending on time taken. ((time / maxTime) * 255), or similar
     * function.
     */
    int x, y;
    for (x = 0; x < maxX; x++) {
	for (y = 0; y < maxY; y++) {
	    if (timeTable[x][y] != -1) {
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
    bu_log("Max = %lf Min = %lf Mean = %lf Range = %lf\n", maxTime, minTime, meanTime, range);

    int Rcolor = 0;
    int Gcolor = 0;
    int Bcolor = 0;
    int npix = 0;

    /* Now fill out the framebuffer with the Heat Graph information */

    for (x = 0; x < maxX; x++) {
	for (y = 0; y < maxY; y++) {
	    if (timeTable[x][y] < minTime) {
		/* Empty Pixels, i.e. background */
		Rcolor = 0;
		Gcolor = 0;
		Bcolor = 12;
	    } else {
		/* Calculations for determining color of heat graph go here */
		if (timeTable[x][y] >= minTime && timeTable[x][y] < meanTime) {
		    Rcolor = Gcolor = Bcolor = (timeTable[x][y] / maxTime) * 255;
		}
		if (timeTable[x][y] >= meanTime && timeTable[x][y] < maxTime) {
		    Rcolor = Gcolor = Bcolor = (timeTable[x][y] / maxTime) * 255;
		}
		if (timeTable[x][y] >=  maxTime)
		    Rcolor = Gcolor = Bcolor = 255;
	    }
	    
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
	    p[0]=p[1]=p[2]=0;
 	}
    }
    (void)fb_view(fbp, width/2, height/2, 1, 1);
}
