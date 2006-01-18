/*                          I R - X . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file ir-X.c
 *
 */

/*  File:  ir-X.c  */
/*  S.Coates - 30 September 1994  */
/*  Compile:  cc ir-X.c -L/usr/X11/lib -lX11 -o ir-X  */

/*  An x-windows program to create a pictureby sending groups  */
/*  of pixels to the display.  */
/*  This program reads a file that has as its first line the  */
/*  width and height (integers) of the picture.  MAXPIX indicates  */
/*  the maximum width and height.  Each line after the first  */
/*  contains one floating point number, this is the value of  */
/*  that pixel (such as a temperature).  The pixels are read  */
/*  from left to right, top to bottom.  The program 'display'  */
/*  will create a file with the appropriate format.  */

/*  This program currently has a choice of four colors:  shades  */
/*  of gray; shades of red; black-blue-cyan-green-yellow-white;  */
/*  and black-blue-magenta-red-yellow-white.  */

/*	27 May 1992       - Add statements to allow a pix file  */
/*			    to be written.  */
/*	30 September 1994 - Change name from see.c to ir-X.c	*/

/*  Include files needed.  */
#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define MAXFIL 26		/*  Maximum number of char in file name.  */
#define MAXPIX 512		/*  Maximum number of pixels is (512*512).  */
#define MAXARR 120000		/*  Maximum number of pixels that are the  */
				/*  same color.  */
#define MAXCOL 128		/*  Maximum number of colors.  */
#define EXTRA 1			/*  For colors where there are a lot of  */
				/*  pixels.  */

/*  Define the structure for each color.  */
struct colstr
{
	short x1[MAXARR];	/*  X vertice of square.  */
	short y1[MAXARR];	/*  Y vertice of square.  */
	int cnt;		/*  Counter.  */
	int more;		/*  0=>no more, else this is array extra  */
				/*  pixels are in.  */
};

int
main(void)
{
   /*  Variables used for XWindow.  */
   Display *my_display;		/*  Display unit.  */
   Window root_id;		/*  Root window.  */
   Window my_window;		/*  First window opened.  */
   Window wind_pic;		/*  Window with picture.  */
   Window wind_exit;		/*  Exit window.  */
   Window wind_scale;		/*  Color scale window.  */
   GC my_gc;			/*  Gc for my_window.  */
   XSizeHints window_hints;	/*  Hints for 1st window.  */
   XEvent event_received;	/*  Events.  */
   long input_event_mask;	/*  Input event mask that are to  */
				/*  be responed to.  */
   unsigned long black;		/*  Black pixel value.  */
   unsigned long white;		/*  White pixel value.  */
   int screen=0;		/*  Used for getting colors.  */
   XColor colval[MAXCOL];	/*  Color values.  */
   XRectangle rect[MAXARR];	/*  Array for drawing rectangles.  */
   char **a=(char **)NULL;	/*  Place holder for XSetStandard  */
				/*  Properties.  */
   char *winttl = "SEE";	/*  Window title.  */
   char *exit = "QUIT";		/*  Exit label.  */
   Font font;			/*  Structure for setting font.  */

   /*  Other variables.  */
   FILE *fpr;			/*  Used to read a file.  */
   FILE *fpw;			/*  Used to write a file.  */
   char file[MAXFIL];		/*  File to be read.  */
   char line[151];		/*  Used to read one line of a file.  */
   unsigned int main_w,main_h;	/*  Width & height of main window.  */
   unsigned int wide,high;	/*  Width and height of picture window.  */
   double pixval[MAXPIX][MAXPIX];/*  Pixel value (width,height).  */
   double min=0.0,max=0.0;	/*  Minimum & maximum pixel values.  */
   int i,j,k,m;			/*  Loop counters.  */
   double r;			/*  Temporary variable.  */
   double dcol;			/*  Color step.  */
   double pixbin[MAXCOL + 1];	/*  Pixel bins.  */
   double dpix;			/*  Delta between pixel bins.  */
   int color[MAXPIX][MAXPIX];	/*  Indicates color.  */
   int icol;			/*  Indicates color shading.  */
   double check;		/*  Used for finding colors.  */
   char string[11];		/*  Used to write a label.  */
   int flag=0;			/*  Color flag.  */
   int lstarr;			/*  Last color array.  */
   int flag_pix;		/*  0=>no pix file written, 1=>pix file  */
				/*  written.  */
   char file_pix[MAXFIL];	/*  Pix file name.  */
   unsigned char c;		/*  Used to write pix file.  */

   struct colstr array[MAXCOL + EXTRA];	/*  Array for color information.  */

   /*  Get file name to be read.  */
   (void)printf("Enter name of file to be read (%d char max).\n\t",MAXFIL);
   (void)fflush(stdout);
   (void)scanf("%s",file);

   /*  Find what color shading to use.  */
   (void)printf("Indicate type of color shading to use.\n");
   (void)printf("\t0 - gray\n");
   (void)printf("\t1 - red\n");
   (void)printf("\t2 - black-blue-cyan-green-yellow-white\n");
   (void)printf("\t3 - black-blue-magenta-red-yellow-white\n");
   (void)fflush(stdout);
   (void)scanf("%d",&icol);
   if( (icol != 1) && (icol != 2) && (icol != 3) ) icol = 0;

   /*  Determine if a pix file is to be written.  */
   flag_pix = 0;
   (void)printf("Do you wish to create a pix file (0-no, 1-yes)?\n\t");
   (void)fflush(stdout);
   (void)scanf("%d",&flag_pix);
   if(flag_pix != 1) flag_pix = 0;
   if(flag_pix == 1)
   {
	(void)printf("Enter name of the pix file to be created ");
	(void)printf("(%d char max).\n\t",MAXFIL);
	(void)fflush(stdout);
	(void)scanf("%s",file_pix);
   }

   (void)printf("Zeroing color info array ");
   (void)fflush(stdout);

   /*  Zero color info array.  */
   for(i=0; i<(MAXCOL + EXTRA); i++)
   {
	/*  This part not necessarily needed.  cnt & more must be zeroed.  */
/*	for(j=0; j<MAXARR; j++)
 *	{
 *	   array[i].x1[j] = (-1);
 *	   array[i].y1[j] = (-1);
 *	}
 */
	array[i].cnt = 0;
	array[i].more = 0;
   }
   lstarr = MAXCOL;

   (void)printf("- finished zeroing\n");
   (void)fflush(stdout);

   (void)printf("Setting up color scale ");
   (void)fflush(stdout);

   /*  Set up color scale.  */
   dcol = (65535. + 1.) / MAXCOL;
   if(icol == 0)		/*  Shades of gray.  */
   {
	(void)printf("-shades of gray ");
	(void)fflush(stdout);
	for(i=0; i<MAXCOL; i++)
	{
	   colval[i].red = (unsigned short)( (double)i * dcol );
	   colval[i].green = (unsigned short)( (double)i * dcol );
	   colval[i].blue = (unsigned short)( (double)i * dcol );
	}
   }

   if(icol == 1)		/*  Shades of red.  */
   {
	(void)printf("- shades of red ");
	(void)fflush(stdout);
	for(i=0; i<MAXCOL; i++)
	{
	   colval[i].red = (unsigned short)( (double)i * dcol );
	   colval[i].green = (unsigned short)0;
	   colval[i].blue = (unsigned short)0;
	}
   }

   if(icol == 2)		/*  Black-blue-cyan-green-yellow-white.  */
   {
	(void)printf("- black-blue-cyan-green-yellow-white ");
	(void)fflush(stdout);
	if(MAXCOL > 1280.)
	{
	   (void)printf("Maximum number of colors, %d, is ",MAXCOL);
	   (void)printf("greater than 1280.\n");
	   (void)printf("This may create problems.\n");
	}
	/*  Color step.  */
	dcol = 1280. / MAXCOL;
	i = 0;
	/*  Colors (0,0,0) to (0,0,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)0;
	   colval[i].green = (unsigned short)0;
	   colval[i].blue = (unsigned short)(check * 256.);
	   check += dcol;
	   i++;
	}
	/*  Colors (0,0,255) to (0,255,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)0;
	   colval[i].green = (unsigned short)(check * 256.);
	   colval[i].blue = (unsigned short)(255 * 256);
	   check += dcol;
	   i++;
	}
	/*  Colors (0,255,255) to (0,255,0).  */
	check = 255.;
	while( (check >= 0.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)0;
	   colval[i].green = (unsigned short)(255 * 256);
	   colval[i].blue = (unsigned short)(check * 256.);
	   check -= dcol;
	   i++;
	}
	/*  Colors (0,255,0) to (255,255,0).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)(check * 256.);
	   colval[i].green = (unsigned short)(255 * 256);
	   colval[i].blue = (unsigned short)0;
	   check += dcol;
	   i++;
	}
	/*  Colors (255,255,0) to (255,255,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)(255 * 256);
	   colval[i].green = (unsigned short)(255 * 256);
	   colval[i].blue = (unsigned short)(check * 256.);
	   check += dcol;
	   i++;
	}
   }

   if(icol == 3)		/*  Black-blue-magenta-red-yellow-white.  */
   {
	(void)printf("- black-blue-magenta-red-yellow-white ");
	(void)fflush(stdout);
	if(MAXCOL > 1280.)
	{
	   (void)printf("Maximum number of colors, %d, is ",MAXCOL);
	   (void)printf("greater than 1280.\n");
	   (void)printf("This may create problems.\n");
	}
	/*  Color step.  */
	dcol = 1280. / MAXCOL;
	i = 0;
	/*  Colors (0,0,0) to (0,0,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)0;
	   colval[i].green = (unsigned short)0;
	   colval[i].blue = (unsigned short)(check * 256.);
	   check += dcol;
	   i++;
	}
	/*  Colors (0,0,255) to (255,0,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)(check * 256.);
	   colval[i].green = (unsigned short)0;
	   colval[i].blue = (unsigned short)(255 * 256);
	   check += dcol;
	   i++;
	}
	/*  Colors (255,0,255) to (255,0,0).  */
	check = 255.;
	while( (check >= 0.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)(255 * 256);
	   colval[i].green = (unsigned short)0;
	   colval[i].blue = (unsigned short)(check * 256.);
	   check -= dcol;
	   i++;
	}
	/*  Colors (255,0,0) to (255,255,0).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)(255 * 256);
	   colval[i].green = (unsigned short)(check * 256.);
	   colval[i].blue = (unsigned short)0;
	   check += dcol;
	   i++;
	}
	/*  Colors (255,255,0) to (255,255,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   colval[i].red = (unsigned short)(255 * 256);
	   colval[i].green = (unsigned short)(255 * 256);
	   colval[i].blue = (unsigned short)(check * 256.);
	   check += dcol;
	   i++;
	}
   }

   (void)printf("- finished.\n");
   (void)fflush(stdout);

   /*  Print out colors.  */
/*
 * for(i=0; i<MAXCOL; i++)
 * {
 *	(void)printf("Color %d:  %d, %d, %d\n",i,colval[i].red,
 *		colval[i].green,colval[i].blue);
 *	(void)fflush(stdout);
 * }
 */

   (void)printf("Reading file ");
   (void)fflush(stdout);

   /*  Open file for reading.  */
   fpr = fopen(file,"r");

   /*  Check for non-existant file.  */
   while(fpr == NULL)
   {
	(void)printf("\nThis file does not exist, please try again.\n");
	(void)fflush(stdout);
	(void)scanf("%s",file);
	fpr = fopen(file,"r");
   }

   /*  Read width and height of window.  */
   (void)fgets(line,150,fpr);
   (void)sscanf(line,"%u %u",&wide,&high);

   /*  Check that width and height are not too big.  */
   if(wide > MAXPIX)
   {
	(void)printf("The width of the window, %d, is greater\n",wide);
	(void)printf("than the maximum for width, %d.  Press\n",MAXPIX);
	(void)printf("delete to end program.\n");
	(void)fflush(stdout);
   }
   if(high > MAXPIX)
   {
	(void)printf("The height of the window, %d, is greater\n",wide);
	(void)printf("than the maximum for height, %d.  Press\n",MAXPIX);
	(void)printf("delete to end program.\n");
	(void)fflush(stdout);
   }

   /*  Read pixel values.  */
   for(i=0; i<(int)high; i++)
   {
	for(j=0; j<(int)wide; j++)
	{
	   (void)fgets(line,150,fpr);
	   (void)sscanf(line,"%lf",&pixval[j][i]);
	}
   }

   /*  Close file.  */
   (void)fclose(fpr);

   (void)printf("- file read.\n");
   (void)fflush(stdout);

   /*  Print out width and height of window.  */
   (void)printf("Width:  %d\n",wide);
   (void)printf("Height:  %d\n",high);
   (void)fflush(stdout);

   /*  Print out the first ten values as check.  */
/*
 * for(i=0; i<10; i++)
 * {
 *	(void)printf("value %d:  %f\n",(i+1),pixval[0][i]);
 *	(void)fflush(stdout);
 * }
 */

   /*  Print out the last ten values (only last 10 values when it is  */
   /*  a 256 x 256 file) as a check.  */
/*
 * for(i=((int)wide - 11); i<(int)wide; i++)
 * {
 *	(void)printf("value %d:  %f\n",(i+1),pixval[(int)high - 1][i]);
 *	(void)fflush(stdout);
 * }
 */

   (void)printf("Finding min & max.\n");
   (void)fflush(stdout);

   /*  Find minimum and maximum of pixel values.  */
   for(i=0; i<high; i++)
   {
	for(j=0; j<wide; j++)
	{
	   if( (j==0) && (i==0) )
	   {
		min = pixval[j][i];
		max = pixval[j][i];
	   }
	   if( min > pixval[j][i] ) min = pixval[j][i];
	   if( max < pixval[j][i] ) max = pixval[j][i];
	}
   }

   /*  Write minimum and maximum pixel values.  */
   (void)printf("Minimum:  %f\n",min);
   (void)printf("Maximum:  %f\n",max);
   (void)fflush(stdout);

   (void)printf("Finding pixel bins ");
   (void)fflush(stdout);

   /*  Find pixel bins.  */
   dpix = (max - min) / MAXCOL;
   pixbin[0] = min;
   for(i=1; i<MAXCOL; i++)
   {
	pixbin[i] = pixbin[i-1] + dpix;
   }
   pixbin[MAXCOL] = max;

   /*  Find the color for each pixel.  */
   for(i=0; i<high; i++)
   {
	for(j=0; j<wide; j++)
	{
	   /*  Determine the color associated with pixval[j][i].  */
	   m = 0;
	   k = 0;
	   color[j][i] = 0;
	   while( (m == 0) && (k < MAXCOL) )
	   {
		if( (pixval[j][i] > pixbin[k]) &&
			(pixval[j][i] <= pixbin[k + 1]) )
		{
		   color[j][i] = k;
		   m = 1;
		}
		k++;
	   }
	}
   }

   (void)printf("- found pixel bins.\n");
   (void)fflush(stdout);

   (void)printf("Putting color info in arrays ");
   (void)fflush(stdout);

   /*  Put color info in arrays.  */
   for(i=0; i<high; i++)
   {
	for(j=0; j<wide; j++)
	{
	   if(array[color[j][i]].cnt < MAXARR)
	   {
		array[color[j][i]].x1[array[color[j][i]].cnt] = j;
		array[color[j][i]].y1[array[color[j][i]].cnt] = i;
		array[color[j][i]].cnt++;
	   }
	   else
	   {
		if(array[color[j][i]].more == 0)
		{
		   if(lstarr < (MAXARR + EXTRA) )
		   {
			array[color[j][i]].more = lstarr;
			k = lstarr;
			lstarr++;
			array[k].x1[array[k].cnt] = j;
			array[k].y1[array[k].cnt] = i;
			array[k].cnt++;
		   }
		   else
		   {
			flag = 1;
		   }
		}
		else
		{
		   k = array[color[j][i]].more;
		   if(array[k].cnt < MAXARR)
		   {
			array[k].x1[array[k].cnt] = j;
			array[k].y1[array[k].cnt] = i;
			array[k].cnt++;
		   }
		   else
		   {
			flag = 1;
		   }
		}
	   }
	}
   }

   (void)printf("- color info in arrays.\n");
   (void)fflush(stdout);

   if(flag != 0)
   {
	(void)printf("There too many pixels of one color.  Note that\n");
	(void)printf("this means there is something wrong with the\n");
	(void)printf("picture!\n");
	(void)fflush(stdout);
   }

   /*  Write out count for each array.  */
/* for(i=0; i<lstarr; i++)
 * {
 *	(void)printf("Color %d - count %d ",i,array[i].cnt);
 *	if(array[i].more > 0)
 *	{
 *	   (void)printf("- more %d\n",array[i].more);
 *	}
 *	else
 *	{
 *	   (void)printf("\n");
 *	}
 *	(void)fflush(stdout);
 * }
 */

   /*  Set up window to be the size that was read.  */
   /*  Open display.  */
   my_display = XOpenDisplay(NULL);

   /*  Get id of parent (root is parent).  */
   root_id = DefaultRootWindow(my_display);

   /*  Find value for black & white.  */
   black = BlackPixel(my_display,DefaultScreen(my_display));
   white = WhitePixel(my_display,DefaultScreen(my_display));

   /*  Create a window & find its id.  */
   main_w = wide + (unsigned int)150;
   main_h = high + (unsigned int)150;
   my_window = XCreateSimpleWindow(my_display,root_id,0,0,
	main_w,main_h,50,white,black);

   /*  Set appropriate fields in window hits structure.  */
   /*  This is only done for the first window created.  */
   window_hints.width = (int)main_w;
   window_hints.height = (int)main_h;
   window_hints.flags = PSize;

   /*  Inform window manager of hints.  */
   /*  Set XSetStandarProperties instead of XSetNormalHints.  */
   XSetStandardProperties(my_display,my_window,winttl,winttl,None,a,0,
	&window_hints);

   /*  Create picture window & find its id.  */
   wind_pic = XCreateSimpleWindow(my_display,my_window,20,20,wide,high,
	5,white,black);

   /*  Create exit window & find its id.  */
   wind_exit = XCreateSimpleWindow(my_display,my_window,(wide + 30),
	(high + 65),80,30,5,white,black);

   /*  Create color scale window & find its id.  */
   wind_scale = XCreateSimpleWindow(my_display,my_window,10,(high + 50),
	(2 * MAXCOL),60,5,white,black);

   /*  Select input event masks that are to be responed to (exposure).  */
   input_event_mask = ExposureMask | ButtonPressMask;

   /*  Notify server about input event masks.  */
   XSelectInput(my_display,my_window,ExposureMask);
   XSelectInput(my_display,wind_pic,ExposureMask);
   XSelectInput(my_display,wind_exit,input_event_mask);
   XSelectInput(my_display,wind_scale,input_event_mask);

   /*  Map window to display so that it will show up.  */
   XMapWindow(my_display,my_window);

   /*  Map picture window, exit window, & color scale window to display  */
   /*  so that they will show up.  */
   XMapWindow(my_display,wind_pic);
   XMapWindow(my_display,wind_exit);
   XMapWindow(my_display,wind_scale);

   /*  Create graphics context (gc) for my window.  */
   my_gc = XCreateGC(my_display,my_window,None,NULL);

   /*  Create a loop so that events will occur.  */
   for(;;)
   {
	XNextEvent(my_display,&event_received);
	switch(event_received.type)
	{
	   /*  Do the following when an expose occurs.  */
	   case Expose:		/*  Code for expose event.  */
	     /*  Draw the picture if the picture window is exposed.  */
	     if(event_received.xexpose.window == wind_pic)
	     {						/*  START # 1  */
		/*  Send groups of color to screen.  */
		for(k=0; k<MAXCOL; k++)
		{
		  if(array[k].cnt > 0)
		  {
		   for(i=0; i<array[k].cnt; i++)
		   {
			rect[i].x = array[k].x1[i];
			rect[i].y = array[k].y1[i];
			rect[i].width = 1;
			rect[i].height = 1;
		   }
		   m = array[k].cnt;
		   XAllocColor(my_display,DefaultColormap(my_display,screen),
			&colval[k]);
		   XSetForeground(my_display,my_gc,colval[k].pixel);
		   XFillRectangles(my_display,wind_pic,my_gc,rect,m);

		   /*  Send extra array if there is a lot of that color.  */
		   if(array[k].more > 0)
		   {
			j = array[k].more;
			for(i=0; i<array[j].cnt; i++)
			{
			   rect[i].x = array[j].x1[i];
			   rect[i].y = array[j].y1[i];
			   rect[i].width = 1;
			   rect[i].height = 1;
			}
			m = array[j].cnt;
			XAllocColor(my_display,DefaultColormap(my_display,
				screen),&colval[k]);
			XSetForeground(my_display,my_gc,colval[k].pixel);
			XFillRectangles(my_display,wind_pic,my_gc,rect,m);
		   }
		  }
		}
	     }						/*  END # 1  */

	     /*  If exit window is exposed write label.  */
	     else if(event_received.xexpose.window == wind_exit)
	     {						/*  START # 2  */
		XSetForeground(my_display,my_gc,white);
		font = XLoadFont(my_display,"8x13bold");
		XSetFont(my_display,my_gc,font);
		XDrawString(my_display,wind_exit,my_gc,25,20,exit,
			strlen(exit));
	     }						/*  END # 2  */

	     /*  If color scale window is exposed put up color scale.  */
	     else if(event_received.xexpose.window == wind_scale)
	     {
		for(i=0; i<MAXCOL; i++)
		{
		   XAllocColor(my_display,DefaultColormap(my_display,
			screen),&colval[i]);
		   XSetForeground(my_display,my_gc,colval[i].pixel);
		   XFillRectangle(my_display,wind_scale,my_gc,
			(i * 2),0,2,30);
		}
		/*  Write label.  */
		XSetForeground(my_display,my_gc,white);
		font = XLoadFont(my_display,"8x13bold");
		XSetFont(my_display,my_gc,font);
		(void)sprintf(string,"%.0f",min);
		XDrawString(my_display,wind_scale,my_gc,
			2,45,string,strlen(string));
		r = min + (max - min) / 4.;
		(void)sprintf(string,"%.0f",r);
		XDrawString(my_display,wind_scale,my_gc,
			(MAXCOL * 2 / 4 - 8),45,string,strlen(string));
		r = min + (max - min) / 2.;
		(void)sprintf(string,"%.0f",r);
		XDrawString(my_display,wind_scale,my_gc,
			(MAXCOL * 2 / 2 - 8),45,string,strlen(string));
		r = min + (max - min) * 3. / 4.;
		(void)sprintf(string,"%.0f",r);
		XDrawString(my_display,wind_scale,my_gc,
			(MAXCOL * 2 * 3 / 4 - 8),45,string,strlen(string));
		(void)sprintf(string,"%.0f",max);
		XDrawString(my_display,wind_scale,my_gc,
			(MAXCOL * 2 - 16),45,string,strlen(string));
	     }
	   break;

	   /*  Do the following if a mouse press occurs.  */
	   case ButtonPress:
	     if(event_received.xbutton.window == wind_exit)
	     {
		XCloseDisplay(my_display);

		/*  Create pix file if necessary.  */
		if(flag_pix == 1)
		{					/*  START # 1.  */
		   /*  Open pix file to be written to.  */
		   fpw = fopen(file_pix,"w");

		   /*  Write colors to file.  */
		   for(i=high; i>0; i--)
		   {					/*  START # 2.  */
			for(j=0; j<wide; j++)
			{				/*  START # 3.  */
			   c = (unsigned char)( (int)(colval[color[j][i-1]].red
				/ 256) );
			   putc(c,fpw);
			   c = (unsigned char)( (int)
				(colval[color[j][i-1]].green / 256) );
			   putc(c,fpw);
			   c = (unsigned char)( (int)(colval[color[j][i-1]].blue
				/ 256) );
			   putc(c,fpw);
			}				/*  END # 3.  */
		   }					/*  END # 2.  */

		   /*  Close file.  */
		   (void)fclose(fpw);

		   (void)printf("Pix file, %s, has been written.\n",file_pix);
		   (void)fflush(stdout);
		}					/*  END # 1.  */

		return(0);
	     }
	   break;
	}
   }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
