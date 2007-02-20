/*                          S E E 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file see2.c
 *
 */

/*  File:  see2.c  */
/*  S.Coates - 27 May 1992  */
/*  Compile:  cc see2.c -o see2 -lgl_s  */

/*  A graphics program using the SGI graphics library to create  */
/*  a picture pixel by pixel.  */
/*  This program reads a file that has as its first line the  */
/*  width and height (integers) of the picture.  MAXPIX indicates  */
/*  the maximum width and height.  Each line after the first  */
/*  contains one floating point number, this is the value of  */
/*  that pixel (such as a temperature).  The pixels are read  */
/*  from left to right, top to bottom.  The program 'showtherm'  */
/*  will create a file with the appropriate format.  */

/*	27 May 1992      - Allow pix file to be written if desired.  */

#if defined(IRIX)

/*  Include files needed.  */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gl.h>

#define MAXFIL 26		/*  Maximum number of char in file name.  */
#define MAXPIX 512		/*  Maximum number of pixels is (512*512).  */
#define MAXCOL 256		/*  Maximum number of colors.  */

main()
{
   FILE *fpr;			/*  Used to read a file.  */
   FILE *fpw;			/*  Used to write a file.  */
   char file[MAXFIL];		/*  File to be read.  */
   char line[151];		/*  Used to read one line of a file.  */
   unsigned int main_w,main_h;	/*  Width & height of main window.  */
   unsigned int wide,high;	/*  Width and height of picture window.  */
   double pixval[MAXPIX][MAXPIX];/*  Pixel value (width,height).  */
   double min,max;		/*  Minimum & maximum pixel values.  */
   int i,j,k;			/*  Loop counters.  */
   double r;			/*  Temporary variable.  */
   int dcol;			/*  Color step.  */
   double pixbin[MAXCOL + 1];	/*  Pixel bins.  */
   double dpix;			/*  Delta between pixel bins.  */
   int iscan;			/*  0=>print scan number, 1=>don't print.  */
   int colscl;			/*  Color scale to use, 0=>gray, 1=>black-  */
				/*  blue-cyan-green-yellow-white, 2=>black-  */
				/*  blue-magenta-red-yellow-green-white.  */
   int icolor[MAXPIX][MAXPIX];	/*  Indicates color.  */
   float rgbcol[MAXCOL][3];	/*  RGB colors.  */
   float check;			/*  Used for finding colors.  */
   long x1,x2,y1,y2;		/*  Corners of window.  */
   float r1,r2,s1,s2;		/*  Used in determining rectangle to fill.  */
   long a,b;			/*  Dummy variable.  */
   char string[11];		/*  Used to write a character string.  */
   int c;			/*  Character used to determine end of  */
				/*  program.  */
   short blackvec[3];		/*  The color black.  */
   short vec[3];		/*  Used for color vector.  */
   int flag;			/*  Flag.  */
   int flag_pix;		/*  0=>don't write pix file, 1=>write pix  */
				/*  file.  */
   char file_pix[MAXFIL];	/*  Pix file.  */
   unsigned char d;		/*  Used to write a pix file.  */

   /*  Set the color black.  */
   blackvec[0] = 0;
   blackvec[1] = 0;
   blackvec[2] = 0;

   /*  Get file name to be read.  */
   (void)printf("Enter name of file to be read (%d char max).\n\t",MAXFIL);
   (void)fflush(stdout);
   (void)scanf("%s",file);

   /*  Determine what color scale should be used.  */
   (void)printf("Indicate color scale to be used.\n");
   (void)printf("\t0 - gray\n");
   (void)printf("\t1 - black-blue-cyan-green-yellow-white.\n");
   (void)printf("\t2 - black-blue-magenta-red-yellow-white.\n");
   (void)fflush(stdout);
   (void)scanf("%d",&colscl);
   if( (colscl != 1) && (colscl != 2) ) colscl = 0;

   /*  Determine if scan lines should be printed.  */
   (void)printf("Print scan line number (0-yes, 1-no)?\n\t");
   (void)fflush(stdout);
   (void)scanf("%d",&iscan);
   if(iscan != 0) iscan=1;

   flag_pix = 0;
   (void)printf("Do you wish to create a pix file (0-no, 1-yes)?\n\t");
   (void)fflush(stdout);
   (void)scanf("%d",&flag_pix);
   if(flag_pix != 1) flag_pix = 0;
   if(flag_pix == 1)
   {
	(void)printf("Enter the name of the pix file to be created ");
	(void)printf("(%d char max).\n\t",MAXFIL);
	(void)fflush(stdout);
	(void)scanf("%s",file_pix);
   }

   (void)printf("Setting color scale - ");
   (void)fflush(stdout);

   /*  Set up color scale to be used.  */
   /*  Gray scale.  */
   if(colscl == 0)
   {
	(void)printf("gray scale ");
	(void)fflush(stdout);
	if(MAXCOL > 256)
	{
	   (void)printf("Maximum number of colors, %d, is ",MAXCOL);
	   (void)printf("greater than 256.\n");
	   (void)printf("This may create problems.\n");
	   (void)fflush(stdout);
	}
	/*  Color step.  */
	dcol = (int)(256. / MAXCOL);
	for(i=0; i<MAXCOL; i++)
	{
	   rgbcol[i][0] = (float)i;
	   rgbcol[i][1] = (float)i;
	   rgbcol[i][2] = (float)i;
	}
   }

   /*  Black-blue-cyan-green-yellow-white scale.  */
   if(colscl == 1)
   {
	(void)printf("black-blue-cyan-green-yellow-white scale ");
	(void)fflush(stdout);
	if(MAXCOL > 1280.)
	{
	   (void)printf("Maximum number of colors, %d, is ",MAXCOL);
	   (void)printf("greater than 1280.\n");
	   (void)printf("This may create problems.\n");
	   (void)fflush(stdout);
	}
	/*  Color step.  */
	dcol = (int)(1280. / MAXCOL);
	i = 0;
	/*  Colors (0,0,0) to (0,0,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = 0.;
	   rgbcol[i][1] = 0.;
	   rgbcol[i][2] = check;
	   check += (float)dcol;
	   i++;
	}
	/*  Colors (0,0,255) to (0,255,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = 0.;
	   rgbcol[i][1] = check;
	   rgbcol[i][2] = 255.;
	   check += (float)dcol;
	   i++;
	}
	/*  Colors (0,255,255) to (0,255,0).  */
	check = 255.;
	while( (check >= 0.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = 0.;
	   rgbcol[i][1] = 255.;
	   rgbcol[i][2] = check;
	   check -= (float)dcol;
	   i++;
	}
	/*  Colors (0,255,0) to (255,255,0).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = check;
	   rgbcol[i][1] = 255.;
	   rgbcol[i][2] = 0.;
	   check += (float)dcol;
	   i++;
	}
	/*  Colors (255,255,0) to (255,255,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = 255.;
	   rgbcol[i][1] = 255.;
	   rgbcol[i][2] = check;
	   check += (float)dcol;
	   i++;
	}
   }

   /*  Black-blue-magenta-red-yellow-white scale.  */
   if(colscl == 2)
   {
	(void)printf("black-blue-magenta-red-yellow-white scale ");
	(void)fflush(stdout);
	if(MAXCOL > 1280.)
	{
	   (void)printf("Maximum number of colors, %d, is ",MAXCOL);
	   (void)printf("greater than 1280.\n");
	   (void)printf("This may create problems.\n");
	   (void)fflush(stdout);
	}
	/*  Color step.  */
	dcol = (int)(1280. / MAXCOL);
	i = 0;
	/*  Colors (0,0,0) to (0,0,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = 0.;
	   rgbcol[i][1] = 0.;
	   rgbcol[i][2] = check;
	   check += (float)dcol;
	   i++;
	}
	/*  Colors (0,0,255) to (255,0,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = check;
	   rgbcol[i][1] = 0.;
	   rgbcol[i][2] = 255.;
	   check += (float)dcol;
	   i++;
	}
	/*  Colors (255,0,255) to (255,0,0).  */
	check = 255.;
	while( (check >= 0.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = 255.;
	   rgbcol[i][1] = 0.;
	   rgbcol[i][2] = check;
	   check -= (float)dcol;
	   i++;
	}
	/*  Colors (255,0,0) to (255,255,0).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = 255.;
	   rgbcol[i][1] = check;
	   rgbcol[i][2] = 0.;
	   check += (float)dcol;
	   i++;
	}
	/*  Colors (255,255,0) to (255,255,255).  */
	check = 0.;
	while( (check <= 255.) && (i < MAXCOL) )
	{
	   rgbcol[i][0] = 255.;
	   rgbcol[i][1] = 255.;
	   rgbcol[i][2] = check;
	   check += (float)dcol;
	   i++;
	}
   }

   (void)printf("- set.\n");
   (void)fflush(stdout);

   (void)printf("Reading file - ");
   (void)fflush(stdout);

   /*  Open file for reading.  */
   fpr = fopen(file,"r");

   /*  Check for existance of file.  */
   while(fpr == NULL)
   {
	(void)printf("\nThe file does not exist, try again.\n");
	(void)fflush(stdout);
	(void)scanf("%s",file);
	fpr = fopen(file,"r");
   }

   /*  Read width and height of window.  */
   (void)bu_fgets(line,150,fpr);
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
	   (void)bu_fgets(line,150,fpr);
	   (void)sscanf(line,"%lf",&pixval[j][i]);
	}
   }

   /*  Close file.  */
   (void)fclose(fpr);

   (void)printf("file read.\n");
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

   (void)printf("Finding pixel bins - ");
   (void)fflush(stdout);

   /*  Find pixel bins.  */
   dpix = (max - min) / MAXCOL;
   pixbin[0] = min;
   for(i=1; i<MAXCOL; i++)
   {
	pixbin[i] = pixbin[i-1] + dpix;
   }
   pixbin[MAXCOL] = max;

   (void)printf("pixel bins found.\n");
   (void)fflush(stdout);

   (void)printf("Setting color for each pixel - ");
   (void)fflush(stdout);

   for(i=0; i<high; i++)
   {
	for(j=0; j< wide; j++)
	{
	   flag = 0;
	   k = 0;
	   icolor[j][i] = 0;
	   while( (flag == 0) && (k < MAXCOL) )
	   {
		if( (pixval[j][i] > pixbin[k]) &&
			(pixval[j][i] <= pixbin[k + 1]) )
		{
		   icolor[j][i] = k;
		   flag = 1;
		}
		k++;
	   }
	}
   }

   (void)printf("colors found.\n");
   (void)fflush(stdout);

   /*  Make a window with the color scale.  */
   /*  Set location of window.  */
   x1 = (long)100;
   y1 = (long)(100 + high + 50);
   x2 = x1 + (long)700;
   y2 = y1 + (long)50;
   prefposition(x1,x2,y1,y2);

   /*  Graphics in foreground.  */
   foreground();

   /*  Set name of window.  */
   winopen("Color Scale");

   /*  Use rgb mode.  */
   RGBmode();
   gconfig();

   /*  Scale window.  */
   ortho2(0.,(float)MAXCOL,0.,50.);

   /*  Set background color.  */
   c3s(blackvec);
   clear();

   /*  Set color & draw.  */
   for(i=0; i<MAXCOL; i++)
   {
	vec[0] = (short)rgbcol[i][0];
	vec[1] = (short)rgbcol[i][1];
	vec[2] = (short)rgbcol[i][2];
	c3s(vec);
	r1 = (float)i;
	r2 = (float)(i + 1);
	s1 = 0.;
	s2 = 25.;
	rectf(r1,s1,r2,s2);
   }

   /*  Write minimum and maximum temperature.  */
   color(YELLOW);
   cmov2(0.1,30.);
   (void)sprintf(string,"%.0f",min);
   charstr(string);
   cmov2((MAXCOL * .24),30.);
   r = min + (max - min) / 4.;
   (void)sprintf(string,"%.0f",r);
   charstr(string);
   cmov2((MAXCOL * .49),30.);
   r = min + (max - min) / 2.;
   (void)sprintf(string,"%.0f",r);
   charstr(string);
   cmov2((MAXCOL * .74),30.);
   r = min + (max - min) * 3. / 4.;
   (void)sprintf(string,"%.0f",r);
   charstr(string);
   cmov2((MAXCOL * .98),30.);
   (void)sprintf(string,"%.0f",max);
   charstr(string);

   /*  Set location of window.  */
   x1 = (long)100;
   y1 = (long)100;
   x2 = x1 + (long)wide;
   y2 = y1 + (long)high;
   prefposition(x1,x2,y1,y2);

   /*  Prevent graphics from being put in background.  */
   foreground();

   /*  Set name of window.  */
   winopen("SEE2");

   /*  Scale window.  */
   ortho2(0.,(float)wide,0.,(float)high);

   /*  Use rgb mode.  */
   RGBmode();
   gconfig();

   /*  Make the background black.  */
   c3s(blackvec);
   clear();

   for(i=0; i<high; i++)
   {
	for(j=0; j<wide; j++)
	{
	   /*  Set color.  */
	   vec[0] = (short)rgbcol[icolor[j][i]][0];
	   vec[1] = (short)rgbcol[icolor[j][i]][1];
	   vec[2] = (short)rgbcol[icolor[j][i]][2];
	   c3s(vec);

	   /*  Draw the pixel & fill in.  */
	   r1 = (float)(j);
	   r2 = (float)(j + 1);
	   s1 = (float)(high - i - 1);
	   s2 = (float)(high - i);
	   rectf(r1,s1,r2,s2);
	}
	/*  Print scan line number if necessary.  */
	if(iscan == 0)
	{
	   (void)printf("scan line:  %d\n",i);
	   (void)fflush(stdout);
	}
   }

/*
 * sleep(10);
 */

   (void)printf("Press 'z' return to end.  ");
   (void)fflush(stdout);
   c = 'a';
   while(c != 'z')
   {
	c = getchar();
   }

   /*  Write pix file if necessary.  */
   if(flag_pix == 1)
   {						/*  START # 20.  */
	/*  Open pix file.  */
	fpw = fopen(file_pix,"w");

	for(i=high; i>0; i--)
	{					/*  START # 21.  */
	   for(j=0; j<wide; j++)
	   {					/*  START # 22.  */
		d = (unsigned char)(rgbcol[icolor[j][i-1]][0]);
		putc(d,fpw);
		d = (unsigned char)(rgbcol[icolor[j][i-1]][1]);
		putc(d,fpw);
		d = (unsigned char)(rgbcol[icolor[j][i-1]][2]);
		putc(d,fpw);
	   }					/*  END # 22.  */
	}					/*  END # 21.  */

	/*  Close file.  */
	(void)fclose(fpw);
	(void)printf("Pix file, %s, has been written.\n",file_pix);
	(void)fflush(stdout);
   }						/*  END # 20.  */

   (void)printf("THE END\n\n");
   (void)fflush(stdout);
}

#else	/* !defined(IRIX) */
#include <stdio.h>
main(void)
{
	fprintf(stderr,"see2: This program only works on an SGI workstation\n");
	exit(1);
}
#endif	/* !defined(IRIX) */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
