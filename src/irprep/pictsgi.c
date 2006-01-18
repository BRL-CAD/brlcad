/*                       P I C T S G I . C
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
/** @file pictsgi.c
 *
 *  This is a program that will raytrace a BRL-CAD mged
 *  model and/or display it using the PRISM temperature output
 *  file, using auxiliary program "ir-sgi".
 *
 *  File:  pictsgi.c
 *  S.Coates - 22 April 1992
 *  Compile:  cc pictsgi.c -o pictsgi
 */

/*  Include files.  */
#include<stdio.h>
#include<string.h>
#include<math.h>

main(void)
{
   int ichoice;			/*  Choice.  */
   char *irsgi = "ir-sgi";	/*  Calls ir-sgi program.  */
   char showtherm[125];		/*  Calls showtherm program.  */
   char gfile[16];		/*  .g file.  */
   char group[26];		/*  Group names.  */
   int ngrp;			/*  Number of groups.  */
   int i,j,k;			/*  Loop counters.  */

   /*  Find option.  */
   (void)printf("This takes a BRL-CAD mged model with a PRISM\n");
   (void)printf("temperature output file and raytrace and/or\n");
   (void)printf("display it using sgi graphics.  Make your\n");
   (void)printf("selection.\n");
   (void)printf("\t0 - raytrace & store file\n");
   (void)printf("\t1 - raytrace, store, & showtherm file\n");
   (void)printf("\t2 - showtherm file\n");
   (void)fflush(stdout);
   (void)scanf("%d",&ichoice);

   while( (ichoice !=0 ) && (ichoice != 1) &&(ichoice != 2) )
   {
	(void)printf("Your choice was not 0, 1, or 2, enter again!!\n");
	(void)fflush(stdout);
	(void)scanf("%d",&ichoice);
   }

   if( (ichoice == 0) || (ichoice == 1) )
   {
	/*  Start setting showtherm variable.  */
	showtherm[0] = 's';
	showtherm[1] = 'h';
	showtherm[2] = 'o';
	showtherm[3] = 'w';
	showtherm[4] = 't';
	showtherm[5] = 'h';
	showtherm[6] = 'e';
	showtherm[7] = 'r';
	showtherm[8] = 'm';
	showtherm[9] = ' ';
	i = 10;
	/*  Find name of .g file to be used.  */
	(void)printf("Enter .g file to be raytraced (15 char max).\n\t");
	(void)fflush(stdout);
	(void)scanf("%s",gfile);
	/*  Find number of groups to be raytraced.  */
	(void)printf("Enter the number of groups to be raytraced.\n\t");
	(void)fflush(stdout);
	(void)scanf("%d",&ngrp);
	/*  Read each group & put it in the variable showtherm.  */
	j = 0;
	while( (gfile[j] != '\0') && (i < 123) )
	{
	   showtherm[i] = gfile[j];
	   i++;
	   j++;
	}
	for(j=0; j<ngrp; j++)
	{
	   (void)printf("Enter group %d (25 char max).\n\t",j);
	   (void)fflush(stdout);
	   (void)scanf("%s",group);
	   showtherm[i] = ' ';
	   i++;
	   k = 0;
	   while( (group[k] != '\0') && (i < 123) )
	   {
		showtherm[i] = group[k];
		i++;
		k++;
	   }
	}
	showtherm[i] = '\0';
	if(i >= 123)
	{
	   (void)printf("There are too many characters for showtherm,\n");
	   (void)printf("please revise pictx.\n");
	   (void)fflush(stdout);
	}

	/*  Call the program showtherm with the appropriate options.  */
	/*  This will raytrace a .g file & find the appropriate  */
	/*  temperature for each region.  */
	(void)printf("\nThe program showtherm is now being run.\n\n");
	(void)fflush(stdout);
	system(showtherm);
   }

   if( (ichoice == 1) || (ichoice == 2) )
   {
	/*  Call the program ir-sgi so that a file that has been raytraced  */
	/*  may be displayed using X-Windows.  */
	(void)printf("\nThe program ir-sgi in now being run.  If option\n");
	(void)printf("0 or 1 was used when the name of a file is asked\n");
	(void)printf("for enter the name of the file that was just\n");
	(void)printf("stored.\n\n");
	(void)fflush(stdout);
	system(irsgi);
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
