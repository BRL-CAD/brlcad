/*                          I R - X . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/* An X-windows program to create a picture by sending groups
 * of pixels to the display.
 * This program reads a file that has as its first line the
 * width and height (integers) of the picture.  MAXPIX indicates
 * the maximum width and height.  Each line after the first
 * contains one floating point number, which is the value of
 * that pixel (such as a temperature).  The pixels are read
 * from left to right, top to bottom.  The program 'display'
 * will create a file with the appropriate format.
 */

/* This program currently has a choice of four colors:  shades
 * of gray; shades of red; black-blue-cyan-green-yellow-white;
 * and black-blue-magenta-red-yellow-white.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "bu.h"


#define MAXFIL 26		/* Maximum number of char in file name.  */
#define MAXPIX 512		/* Maximum number of pixels is (512*512).  */
#define MAXARR 65536		/* Maximum number of pixels that are the
				 * same color.  */
#define MAXCOL 128		/* Maximum number of colors.  */
#define EXTRA 1			/* For colors where there are a lot of
				 * pixels.  */

/* Define the structure for each color.  */
struct colstr
{
    short x1[MAXARR];	/* X vertex of square.  */
    short y1[MAXARR];	/* Y vertex of square.  */
    int cnt;		/* Counter.  */
    int more;		/* 0=>no more, else this is array extra
                         * pixels are in.  */
};


int
main (int argc, char **argv)
{
    /* Variables used for XWindow.  */
    Display *my_display;		/* Display unit.  */
    Window root_id;		/* Root window.  */
    Window my_window;		/* First window opened.  */
    Window wind_pic;		/* Window with picture.  */
    Window wind_exit;		/* Exit window.  */
    Window wind_scale;		/* Color scale window.  */
    GC my_gc;			/* Gc for my_window.  */
    XSizeHints window_hints;	/* Hints for 1st window.  */
    XEvent event_received;	/* Events.  */
    long input_event_mask;	/* Input event mask that are to
				 * be responded to.  */
    unsigned long black;		/* Black pixel value.  */
    unsigned long white;		/* White pixel value.  */
    int screen=0;		/* Used for getting colors.  */
    XColor colval[MAXCOL];	/* Color values.  */
    XRectangle rect[MAXARR];	/* Array for drawing rectangles.  */
    char **a=(char **)NULL;	/* Place holder for XSetStandard Properties.  */
    const char winttl[] = "SEE";	/* Window title.  */
    const char quit[] = "QUIT";		/* Exit label.  */
    Font font;			/* Structure for setting font.  */

    /* Other variables.  */
    FILE *fpr;			/* Used to read a file.  */
    FILE *fpw;			/* Used to write a file.  */
    char file[MAXFIL];		/* File to be read.  */
    char line[151];		/* Used to read one line of a file.  */
    size_t main_w, main_h;	/* Width & height of main window.  */
    size_t wide, high;	/* Width and height of picture window.  */
    double pixval[MAXPIX][MAXPIX];/* Pixel value (width, height).  */
    double min=0.0, max=0.0;	/* Minimum & maximum pixel values.  */
    int i, j, k, m;			/* Loop counters.  */
    double r;			/* Temporary variable.  */
    double dcol;			/* Color step.  */
    double pixbin[MAXCOL + 1];	/* Pixel bins.  */
    double dpix;			/* Delta between pixel bins.  */
    int color[MAXPIX][MAXPIX];	/* Indicates color.  */
    int icol;			/* Indicates color shading.  */
    double check;		/* Used for finding colors.  */
    char string[11];		/* Used to write a label.  */
    int flag=0;			/* Color flag.  */
    int lstarr;			/* Last color array.  */
    int flag_pix;		/* 0=>no pix file written, 1=>pix file written.  */
    char file_pix[MAXFIL];	/* Pix file name.  */
    unsigned char c;		/* Used to write pix file.  */
    int ret;
    int ch;

    struct colstr *array = NULL;	/* Array for color information.  */

    bu_opterr = 0;
    while ((ch = bu_getopt(argc, argv, "h?")) != -1)
    {	if (bu_optopt == '?') ch = 'h';
	switch (ch) {
	    case 'h':
		fprintf(stderr,"ir-X is interactive; sample session in 'man' page\n");
		bu_exit(1,NULL);
	    default:
		break;
	}
    }

    fprintf(stderr,"   Program continues running:\n");

    array = (struct colstr *)bu_calloc(MAXCOL + EXTRA, sizeof(struct colstr), "allocate colstr array");

    /* Get file name to be read.  */
    printf("Enter name of file to be read (%d char max).\n\t", MAXFIL);
    (void)fflush(stdout);
    ret = scanf("%25s", file);
    if (ret == 0)
	perror("scanf");

    /* Find what color shading to use.  */
    printf("Indicate type of color shading to use.\n");
    printf("\t0 - gray\n");
    printf("\t1 - red\n");
    printf("\t2 - black-blue-cyan-green-yellow-white\n");
    printf("\t3 - black-blue-magenta-red-yellow-white\n");
    (void)fflush(stdout);
    ret = scanf("%d", &icol);
    if (ret == 0)
	perror("scanf");
    if ((icol != 1) && (icol != 2) && (icol != 3)) icol = 0;

    /* Determine if a pix file is to be written.  */
    flag_pix = 0;
    printf("Do you wish to create a pix file (0-no, 1-yes)?\n\t");
    (void)fflush(stdout);
    ret = scanf("%d", &flag_pix);
    if (ret == 0)
	perror("scanf");
    if (flag_pix != 1)
	 flag_pix = 0;
    else {
	printf("Enter name of the pix file to be created ");
	printf("(%d char max).\n\t", MAXFIL);
	(void)fflush(stdout);
	ret = scanf("%25s", file_pix);
	if (ret == 0)
	    perror("scanf");
    }

    printf("Zeroing color info array ");
    (void)fflush(stdout);

    /* Zero color info array.  */
    for (i=0; i<(MAXCOL + EXTRA); i++) {
	array[i].cnt = 0;
	array[i].more = 0;
    }
    lstarr = MAXCOL;

    printf("- finished zeroing\n");
    (void)fflush(stdout);

    printf("Setting up color scale ");
    (void)fflush(stdout);

    /* Set up color scale.  */
    dcol = (65535.0 + 1.0) / MAXCOL;
    if (icol == 0) {
	/* Shades of gray.  */
	printf("-shades of gray ");
	(void)fflush(stdout);
	for (i=0; i<MAXCOL; i++) {
	    colval[i].red = (unsigned short)((double)i * dcol);
	    colval[i].green = (unsigned short)((double)i * dcol);
	    colval[i].blue = (unsigned short)((double)i * dcol);
	}
    }

    else if (icol == 1) {
	/* Shades of red.  */
	printf("- shades of red ");
	(void)fflush(stdout);
	for (i=0; i<MAXCOL; i++) {
	    colval[i].red = (unsigned short)((double)i * dcol);
	    colval[i].green = (unsigned short)0;
	    colval[i].blue = (unsigned short)0;
	}
    }

    else if (icol == 2) {
	/* Black-blue-cyan-green-yellow-white.  */
	printf("- black-blue-cyan-green-yellow-white ");
	(void)fflush(stdout);
	if (MAXCOL > 1280.0) {
	    printf("Maximum number of colors, %d, is ", MAXCOL);
	    printf("greater than 1280.\n");
	    printf("This may create problems.\n");
	}
	/* Color step.  */
	dcol = 1280. / MAXCOL;
	i = 0;
	/* Colors (0, 0, 0) to (0, 0, 255).  */
	check = 0.0;
	while ((check <= 255.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)0;
	    colval[i].green = (unsigned short)0;
	    colval[i].blue = (unsigned short)(check * 256.0);
	    check += dcol;
	    i++;
	}
	/* Colors (0, 0, 255) to (0, 255, 255).  */
	check = 0.0;
	while ((check <= 255.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)0;
	    colval[i].green = (unsigned short)(check * 256.0);
	    colval[i].blue = (unsigned short)(255 * 256);
	    check += dcol;
	    i++;
	}
	/* Colors (0, 255, 255) to (0, 255, 0).  */
	check = 255.0;
	while ((check >= 0.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)0;
	    colval[i].green = (unsigned short)(255 * 256);
	    colval[i].blue = (unsigned short)(check * 256.0);
	    check -= dcol;
	    i++;
	}
	/* Colors (0, 255, 0) to (255, 255, 0).  */
	check = 0.0;
	while ((check <= 255.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)(check * 256.0);
	    colval[i].green = (unsigned short)(255 * 256);
	    colval[i].blue = (unsigned short)0;
	    check += dcol;
	    i++;
	}
	/* Colors (255, 255, 0) to (255, 255, 255).  */
	check = 0.0;
	while ((check <= 255.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)(255 * 256);
	    colval[i].green = (unsigned short)(255 * 256);
	    colval[i].blue = (unsigned short)(check * 256.0);
	    check += dcol;
	    i++;
	}
    }

    else if (icol == 3) {
	/* Black-blue-magenta-red-yellow-white.  */
	printf("- black-blue-magenta-red-yellow-white ");
	(void)fflush(stdout);
	if (MAXCOL > 1280.0) {
	    printf("Maximum number of colors, %d, is ", MAXCOL);
	    printf("greater than 1280.\n");
	    printf("This may create problems.\n");
	}
	/* Color step.  */
	dcol = 1280. / MAXCOL;
	i = 0;
	/* Colors (0, 0, 0) to (0, 0, 255).  */
	check = 0.0;
	while ((check <= 255.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)0;
	    colval[i].green = (unsigned short)0;
	    colval[i].blue = (unsigned short)(check * 256.0);
	    check += dcol;
	    i++;
	}
	/* Colors (0, 0, 255) to (255, 0, 255).  */
	check = 0.0;
	while ((check <= 255.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)(check * 256.0);
	    colval[i].green = (unsigned short)0;
	    colval[i].blue = (unsigned short)(255 * 256);
	    check += dcol;
	    i++;
	}
	/* Colors (255, 0, 255) to (255, 0, 0).  */
	check = 255.0;
	while ((check >= 0.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)(255 * 256);
	    colval[i].green = (unsigned short)0;
	    colval[i].blue = (unsigned short)(check * 256.0);
	    check -= dcol;
	    i++;
	}
	/* Colors (255, 0, 0) to (255, 255, 0).  */
	check = 0.0;
	while ((check <= 255.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)(255 * 256);
	    colval[i].green = (unsigned short)(check * 256.0);
	    colval[i].blue = (unsigned short)0;
	    check += dcol;
	    i++;
	}
	/* Colors (255, 255, 0) to (255, 255, 255).  */
	check = 0.0;
	while ((check <= 255.0) && (i < MAXCOL)) {
	    colval[i].red = (unsigned short)(255 * 256);
	    colval[i].green = (unsigned short)(255 * 256);
	    colval[i].blue = (unsigned short)(check * 256.0);
	    check += dcol;
	    i++;
	}
    }

    printf("- finished.\n");
    (void)fflush(stdout);

    printf("Reading file ");
    (void)fflush(stdout);

    /* Open file for reading.  */
    fpr = fopen(file, "rb");

    /* Check for non-existent file.  */
    while (fpr == NULL) {
	printf("\nThis file does not exist, please try again.\n");
	(void)fflush(stdout);
	ret = scanf("%25s", file);
	if (ret == 0) {
	    bu_exit(EXIT_FAILURE, "ir-X - scanf failure - no file to work with!!\n");
	}
	fpr = fopen(file, "rb");
    }

    /* Read width and height of window.  */
    (void)bu_fgets(line, 150, fpr);
    {
	unsigned long w, h;
	sscanf(line, "%lu %lu", &w, &h);
	wide = w;
	high = h;
    }

    /* Check that width and height are not too big.  */
    if (wide > (size_t)MAXPIX) {
	printf("The width of the window, %lu, is greater\n", (unsigned long)wide);
	printf("than the maximum for width, %lu.  Press\n", (unsigned long)MAXPIX);
	printf("delete to end program.\n");
	(void)fflush(stdout);
    }
    if (high > MAXPIX) {
	printf("The height of the window, %lu, is greater\n", (unsigned long)wide);
	printf("than the maximum for height, %lu.  Press\n", (unsigned long)MAXPIX);
	printf("delete to end program.\n");
	(void)fflush(stdout);
    }

    /* Read pixel values.  */
    for (i=0; i<(int)high; i++) {
	for (j=0; j<(int)wide; j++) {
	    (void)bu_fgets(line, 150, fpr);
	    sscanf(line, "%lf", &pixval[j][i]);
	}
    }

    /* Close file.  */
    (void)fclose(fpr);

    printf("- file read.\n");
    (void)fflush(stdout);

    /* Print out width and height of window.  */
    printf("Width:  %lu\n", (unsigned long)wide);
    printf("Height:  %lu\n", (unsigned long)high);
    (void)fflush(stdout);

    printf("Finding min & max.\n");
    (void)fflush(stdout);

    /* Find minimum and maximum of pixel values.  */
    for (i=0; (size_t)i<high; i++) {
	for (j=0; (size_t)j<wide; j++) {
	    if ((j==0) && (i==0)) {
		min = pixval[j][i];
		max = pixval[j][i];
	    }
	    if (min > pixval[j][i]) min = pixval[j][i];
	    if (max < pixval[j][i]) max = pixval[j][i];
	}
    }

    /* Write minimum and maximum pixel values.  */
    printf("Minimum:  %f\n", min);
    printf("Maximum:  %f\n", max);
    (void)fflush(stdout);

    printf("Finding pixel bins ");
    (void)fflush(stdout);

    /* Find pixel bins.  */
    dpix = (max - min) / MAXCOL;
    pixbin[0] = min;
    for (i=1; i<MAXCOL; i++) {
	pixbin[i] = pixbin[i-1] + dpix;
    }
    pixbin[MAXCOL] = max;

    /* Find the color for each pixel.  */
    for (i=0; (size_t)i<high; i++) {
	for (j=0; (size_t)j<wide; j++) {
	    /* Determine the color associated with pixval[j][i].  */
	    m = 0;
	    k = 0;
	    color[j][i] = 0;
	    while ((m == 0) && (k < MAXCOL)) {
		if ((pixval[j][i] > pixbin[k]) &&
		    (pixval[j][i] <= pixbin[k + 1]))
		{
		    color[j][i] = k;
		    m = 1;
		}
		k++;
	    }
	}
    }

    printf("- found pixel bins.\n");
    (void)fflush(stdout);

    printf("Putting color info in arrays ");
    (void)fflush(stdout);

    /* Put color info in arrays.  */
    for (i=0; (size_t)i<high; i++) {
	for (j=0; (size_t)j<wide; j++) {
	    if (array[color[j][i]].cnt < MAXARR) {
		array[color[j][i]].x1[array[color[j][i]].cnt] = j;
		array[color[j][i]].y1[array[color[j][i]].cnt] = i;
		array[color[j][i]].cnt++;
	    } else {
		if (array[color[j][i]].more == 0) {
		    if (lstarr < (MAXCOL + EXTRA)) {
			array[color[j][i]].more = lstarr;
			k = lstarr;
			lstarr++;
			array[k].x1[array[k].cnt] = j;
			array[k].y1[array[k].cnt] = i;
			array[k].cnt++;
		    } else {
			flag = 1;
		    }
		} else {
		    k = array[color[j][i]].more;
		    if (array[k].cnt < MAXARR) {
			array[k].x1[array[k].cnt] = j;
			array[k].y1[array[k].cnt] = i;
			array[k].cnt++;
		    } else {
			flag = 1;
		    }
		}
	    }
	}
    }

    printf("- color info in arrays.\n");
    (void)fflush(stdout);

    if (flag != 0) {
	printf("There too many pixels of one color.  Note that\n");
	printf("this means there is something wrong with the\n");
	printf("picture!\n");
	(void)fflush(stdout);
    }

    /* Set up window to be the size that was read.  */
    /* Open display.  */
    my_display = XOpenDisplay(NULL);

    /* Get id of parent (root is parent).  */
    root_id = DefaultRootWindow(my_display);

    /* Find value for black & white.  */
    black = BlackPixel(my_display, DefaultScreen(my_display));
    white = WhitePixel(my_display, DefaultScreen(my_display));

    /* Create a window & find its id.  */
    main_w = wide + (unsigned int)150;
    main_h = high + (unsigned int)150;
    my_window = XCreateSimpleWindow(my_display, root_id, 0, 0,
				    main_w, main_h, 50, white, black);

    /* Set appropriate fields in window hits structure.  */
    /* This is only done for the first window created.  */
    window_hints.width = (int)main_w;
    window_hints.height = (int)main_h;
    window_hints.flags = PSize;

    /* Inform window manager of hints.  */
    /* Set XSetStandarProperties instead of XSetNormalHints.  */
    XSetStandardProperties(my_display, my_window, winttl, winttl, None, a, 0,
			   &window_hints);

    /* Create picture window & find its id.  */
    wind_pic = XCreateSimpleWindow(my_display, my_window, 20, 20, wide, high,
				   5, white, black);

    /* Create exit window & find its id.  */
    wind_exit = XCreateSimpleWindow(my_display, my_window, (wide + 30),
				    (high + 65), 80, 30, 5, white, black);

    /* Create color scale window & find its id.  */
    wind_scale = XCreateSimpleWindow(my_display, my_window, 10, (high + 50),
				     (2 * MAXCOL), 60, 5, white, black);

    /* Select input event masks that are to be responded to (exposure).  */
    input_event_mask = ExposureMask | ButtonPressMask;

    /* Notify server about input event masks.  */
    XSelectInput(my_display, my_window, ExposureMask);
    XSelectInput(my_display, wind_pic, ExposureMask);
    XSelectInput(my_display, wind_exit, input_event_mask);
    XSelectInput(my_display, wind_scale, input_event_mask);

    /* Map window to display so that it will show up.  */
    XMapWindow(my_display, my_window);

    /* Map picture window, exit window, & color scale window to display */
    /* so that they will show up.  */
    XMapWindow(my_display, wind_pic);
    XMapWindow(my_display, wind_exit);
    XMapWindow(my_display, wind_scale);

    /* Create graphics context (gc) for my window.  */
    my_gc = XCreateGC(my_display, my_window, None, NULL);

    /* Create a loop so that events will occur.  */
    for (;;) {
	XNextEvent(my_display, &event_received);
	switch (event_received.type) {
	    /* Do the following when an expose occurs.  */
	    case Expose:		/* Code for expose event.  */
		/* Draw the picture if the picture window is exposed.  */
		if (event_received.xexpose.window == wind_pic) {
		    /* START # 1 */
		    /* Send groups of color to screen.  */
		    for (k=0; k<MAXCOL; k++) {
			if (array[k].cnt > 0) {
			    for (i=0; i<array[k].cnt; i++) {
				rect[i].x = array[k].x1[i];
				rect[i].y = array[k].y1[i];
				rect[i].width = 1;
				rect[i].height = 1;
			    }
			    m = array[k].cnt;
			    XAllocColor(my_display, DefaultColormap(my_display, screen),
					&colval[k]);
			    XSetForeground(my_display, my_gc, colval[k].pixel);
			    XFillRectangles(my_display, wind_pic, my_gc, rect, m);

			    /* Send extra array if there is a lot of that color.  */
			    if (array[k].more > 0) {
				j = array[k].more;
				for (i=0; i<array[j].cnt; i++) {
				    rect[i].x = array[j].x1[i];
				    rect[i].y = array[j].y1[i];
				    rect[i].width = 1;
				    rect[i].height = 1;
				}
				m = array[j].cnt;
				XAllocColor(my_display, DefaultColormap(my_display,
									screen), &colval[k]);
				XSetForeground(my_display, my_gc, colval[k].pixel);
				XFillRectangles(my_display, wind_pic, my_gc, rect, m);
			    }
			}
		    }
		}						/* END # 1 */

		/* If exit window is exposed write label.  */
		else if (event_received.xexpose.window == wind_exit) {
		    /* START # 2 */
		    XSetForeground(my_display, my_gc, white);
		    font = XLoadFont(my_display, "8x13bold");
		    XSetFont(my_display, my_gc, font);
		    XDrawString(my_display, wind_exit, my_gc, 25, 20, quit,
				strlen(quit));
		}						/* END # 2 */

		/* If color scale window is exposed put up color scale.  */
		else if (event_received.xexpose.window == wind_scale) {
		    for (i=0; i<MAXCOL; i++) {
			XAllocColor(my_display, DefaultColormap(my_display,
								screen), &colval[i]);
			XSetForeground(my_display, my_gc, colval[i].pixel);
			XFillRectangle(my_display, wind_scale, my_gc,
				       (i * 2), 0, 2, 30);
		    }
		    /* Write label.  */
		    XSetForeground(my_display, my_gc, white);
		    font = XLoadFont(my_display, "8x13bold");
		    XSetFont(my_display, my_gc, font);
		    (void)sprintf(string, "%.0f", min);
		    XDrawString(my_display, wind_scale, my_gc,
				2, 45, string, strlen(string));
		    r = min + (max - min) / 4.0;
		    (void)sprintf(string, "%.0f", r);
		    XDrawString(my_display, wind_scale, my_gc,
				(MAXCOL * 2 / 4 - 8), 45, string, strlen(string));
		    r = min + (max - min) / 2.0;
		    (void)sprintf(string, "%.0f", r);
		    XDrawString(my_display, wind_scale, my_gc,
				(MAXCOL * 2 / 2 - 8), 45, string, strlen(string));
		    r = min + (max - min) * 3. / 4.0;
		    (void)sprintf(string, "%.0f", r);
		    XDrawString(my_display, wind_scale, my_gc,
				(MAXCOL * 2 * 3 / 4 - 8), 45, string, strlen(string));
		    (void)sprintf(string, "%.0f", max);
		    XDrawString(my_display, wind_scale, my_gc,
				(MAXCOL * 2 - 16), 45, string, strlen(string));
		}
		break;

		/* Do the following if a mouse press occurs.  */
	    case ButtonPress:
		if (event_received.xbutton.window == wind_exit) {
		    XCloseDisplay(my_display);

		    /* Create pix file if necessary.  */
		    if (flag_pix == 1) {
			/* START # 1.  */
			/* Open pix file to be written to.  */
			fpw = fopen(file_pix, "wb");

			/* Write colors to file.  */
			for (i=high; i>0; i--) {
			    /* START # 2.  */
			    for (j=0; (size_t)j<wide; j++) {
				/* START # 3.  */
				c = (unsigned char)((int)(colval[color[j][i-1]].red
							  / 256));
				putc(c, fpw);
				c = (unsigned char)((int)
						    (colval[color[j][i-1]].green / 256));
				putc(c, fpw);
				c = (unsigned char)((int)(colval[color[j][i-1]].blue
							  / 256));
				putc(c, fpw);
			    }				/* END # 3.  */
			}					/* END # 2.  */

			/* Close file.  */
			(void)fclose(fpw);

			printf("Pix file, %s, has been written.\n", file_pix);
			(void)fflush(stdout);
		    }					/* END # 1.  */

		    bu_free(array, "free colstr array");
		    return 0;
		}
		break;
	}
    }

    bu_free(array, "free colstr array");
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
