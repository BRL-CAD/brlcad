/*  File:  pictx.c  */
/*  S.Coates - 30 September 1994  */
/*  Compile:  cc pictx.c -L/usr/X11/lib -lX11 -o pictx  */

/*  This is an X-windows program that will raytrace a BRL-CAD mged  */
/*  model and/or diplay it using the PRISM temperature output file.  */

/*  Include files.  */
#include "conf.h"

#include<stdio.h>
#ifdef USE_STRING_H
#include<string.h>
#else
#include <strings.h>
#endif
#include<math.h>
#include<X11/Xlib.h>
#include<X11/Xutil.h>

main()
{
   int ichoice;			/*  Choice.  */
   char *irX = "ir-X";		/*  Calls ir-X program.  */
   char display[125];		/*  Calls display program.  */
   char gfile[16];		/*  .g file.  */
   char group[26];		/*  Group names.  */
   int ngrp;			/*  Number of groups.  */
   int i,j,k;			/*  Loop counters.  */

   /*  Find option.  */
   (void)printf("This takes a BRL-CAD mged model with a PRISM\n");
   (void)printf("temperature output file and raytrace and/or\n");
   (void)printf("display it using X-Windows graphics.  Make\n");
   (void)printf("your selection.\n");
   (void)printf("\t0 - raytrace & store file\n");
   (void)printf("\t1 - raytrace, store, & display file\n");
   (void)printf("\t2 - display file\n");
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
	/*  Start setting display variable.  */
	display[0] = 'd';
	display[1] = 'i';
	display[2] = 's';
	display[3] = 'p';
	display[4] = 'l';
	display[5] = 'a';
	display[6] = 'y';
	display[7] = ' ';
	i = 8;
	/*  Find name of .g file to be used.  */
	(void)printf("Enter .g file to be raytraced (15 char max).\n\t");
	(void)fflush(stdout);
	(void)scanf("%s",gfile);
	/*  Find number of groups to be raytraced.  */
	(void)printf("Enter the number of groups to be raytraced.\n\t");
	(void)fflush(stdout);
	(void)scanf("%d",&ngrp);
	/*  Read each group & put it in the variable display.  */
	j = 0;
	while( (gfile[j] != '\0') && (i < 123) )
	{
	   display[i] = gfile[j];
	   i++;
	   j++;
	}
	for(j=0; j<ngrp; j++)
	{
	   (void)printf("Enter group %d (25 char max).\n\t",j);
	   (void)fflush(stdout);
	   (void)scanf("%s",group);
	   display[i] = ' ';
	   i++;
	   k = 0;
	   while( (group[k] != '\0') && (i < 123) )
	   {
		display[i] = group[k];
		i++;
		k++;
	   }
	}
	display[i] = '\0';
	if(i >= 123)
	{
	   (void)printf("There are too many characters for display,\n");
	   (void)printf("please revise pictx.\n");
	   (void)fflush(stdout);
	}

	/*  Call the program display with the appropriate options.  */
	/*  This will raytrace a .g file & find the appropriate  */
	/*  temperature for each region.  */
	(void)printf("\nThe program display is now being run.\n\n");
	(void)fflush(stdout);
	system(display);
   }

   if( (ichoice == 1) || (ichoice == 2) )
   {
	/*  Call the program ir-X so that a file that has been raytraced  */
	/*  may be displayed using X-Windows.  */
	(void)printf("\nThe program ir-X in now being run.  If option\n");
	(void)printf("0 or 1 was used when the name of a file is asked\n");
	(void)printf("for enter the name of the file that was just\n");
	(void)printf("stored.\n\n");
	(void)fflush(stdout);
	system(irX);
   }

}
