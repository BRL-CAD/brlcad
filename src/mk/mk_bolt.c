/*                       M K _ B O L T . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file mk_bolt.c
 *
 */

/*  File:  mk_bolt.c  */
/*  S.Coates - 2 September 1992  */
/*  To compile for use separately:  */
/*  cc mk_bolt.c /usr/brlcad/lib/libwdb.a /usr/brlcad/lib/librt.a  */
/*	-lmpc -lm -o mk_bolt  */

/*  Program to make a bolt using libwdb.  The objects will be  */
/*  in millimeters.  */

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "wdb.h"

#ifdef M_PI
#define PI M_PI
#else
#define PI 3.141592653589793
#endif

int
main(int argc, char **argv)
{
   struct rt_wdb *fpw;		/*  File to be written to.  */
   char filemged[26];		/*  Mged file create.  */
   double hd,hh;		/*  Diameter & height of bolt head.  */
   double wd,wh;		/*  Diameter & height of washer.  */
   double sd,sh;		/*  Diameter & height of bolt stem.  */
   double leg,hyp;		/*  Length of leg & hypotenus of triangle.  */
   point_t pts[8];		/*  Eight points of arb8.  */
   point_t bs;			/*  Base of rcc.  */
   vect_t ht;			/*  Height of rcc.  */
   fastf_t rad;			/*  Radius of rcc.  */
   int iopt;			/*  Type of bolt to be built: 1=>bolt head,  */
				/*  2=>head & washer; 3=>head, washer, &  */
				/*  stem; 4=>head & stem.  */
   int i,j,k;			/*  Loop counters.  */
   char *temp;			/*  Temporary character string.  */
   char temp1[16];		/*  Temporary character string.  */

   char solnam[9];		/*  Solid name.  */
   char regnam[9];		/*  Region name.  */
   char grpnam[9];		/*  Group name.  */
   int numblt;			/*  Number of bolts to be created (<=26).  */

   struct wmember comb;		/*  Used to make regions.  */
   struct wmember comb1;	/*  Used to make groups.  */

   /*  Zero all dimnsions of bolt.  */
   iopt = 0;
   hd = 0;
   hh = 0;
   wd = 0;
   wh = 0;
   sd = 0;
   sh = 0;

   /*   Set up solid, region, and group names.  */
   solnam[0] = 's';
   solnam[1] = '.';
   solnam[2] = 'b';
   solnam[3] = 'o';
   solnam[4] = 'l';
   solnam[5] = 't';
   solnam[6] = ' ';
   solnam[7] = '#';
   solnam[8] = '\0';
   regnam[0] = 'r';
   regnam[1] = '.';
   regnam[2] = 'b';
   regnam[3] = 'o';
   regnam[4] = 'l';
   regnam[5] = 't';
   regnam[6] = ' ';
   regnam[7] = '#';
   regnam[8] = '\0';
   grpnam[0] = 'b';
   grpnam[1] = 'o';
   grpnam[2] = 'l';
   grpnam[3] = 't';
   grpnam[4] = ' ';
   grpnam[5] = '\0';

   /*  If there are no arguments ask questions.  */
   if(argc == 1)
   {							/*  START # 1  */

   /*  Find type of bolt to build.  */
   (void)printf("Enter option:\n");
   (void)printf("\t1 - bolt head\n");
   (void)printf("\t2 - bolt head & washer\n");
   (void)printf("\t3 - bolt head, washer, & stem\n");
   (void)printf("\t4 - bolt head & stem\n");
   (void)fflush(stdout);
   (void)scanf("%d",&iopt);

   /*  Get file name of mged file to be created.  */
   (void)printf("Enter name of mged file to be created (25 char max).\n\t");
   (void)fflush(stdout);
   (void)scanf("%s",filemged);

   /*  Find the number of bolts to be created (<=26).  */
   (void)printf("Enter the number of bolts to be created (26 max).\n\t");
   (void)fflush(stdout);
   (void)scanf("%d",&numblt);
   if(numblt > 26) numblt = 26;

   /*  Find dimensions of the bolt.  */
   /*  Find dimensions of head first.  */
   (void)printf("Enter diameter (flat edge to flat edge) & height of ");
   (void)printf("bolt head.\n\t");
   (void)fflush(stdout);
   (void)scanf("%lf %lf",&hd,&hh);
   /*  Find dimensions of washer if necessary.  */
   if( (iopt == 2) || (iopt == 3) )
   {
	(void)printf("Enter diameter & height of washer.\n\t");
	(void)fflush(stdout);
	(void)scanf("%lf %lf",&wd,&wh);
   }
   /*  Find dimensions of bolt stem if necessary.  */
   if( (iopt == 3) || (iopt == 4) )
   {
	(void)printf("Enter diameter & height of bolt stem.\n\t");
	(void)fflush(stdout);
	(void)scanf("%lf %lf",&sd,&sh);
   }

   }							/*  END # 1  */

   /*  If there are arguments do not ask any questions.  Get the  */
   /*  answers from the arguments.  */
   else
   {							/*  START # 2  */

	/*  List of options.  */
	/*	-o# - # = 1 => bolt head  */
	/*	          2 => head & washer  */
	/*	          3 => head, washer, & stem  */
	/*	          4 => head & stem  */
	/*	-fname - name = name of .g file  */
	/*  	-n# - # = number of bolts to create (<=26)  */
	/*	-hd# - # = head diameter  */
	/*	-hh# - # = head height  */
	/*	-wd# - # = washer diameter  */
	/*	-wh# - # = washer height  */
	/*	-sd# - # = stem diameter  */
	/*	-sh# - # = stem height  */

	for(i=1; i<argc; i++)
	{						/*  START # 3  */
	   /*  Put argument into temporary character string.  */
	   temp = argv[i];

	   /*  -o - set type of bolt to make.  */
	   if(temp[1] == 'o')
	   {						/*  START # 4  */
		if(temp[2] == '1') iopt = 1;
		if(temp[2] == '2') iopt = 2;
		if(temp[2] == '3') iopt = 3;
		if(temp[2] == '4') iopt = 4;

	   }						/*  END # 4  */

	   /*  -f - mged file name.  */
	   else if(temp[1] == 'f')
	   {						/*  START # 5  */
		j = 2;
		k = 0;
		while( (temp[j] != '\0') && (k < 25) )
		{					/*  START # 6  */
		   filemged[k] = temp[j];
		   j++;
		   k++;
		}					/*  END # 6  */
		filemged[k] = '\0';
	   }						/*  END # 5  */

	   /*  -n - number of bolts to be created.  */
	   else if(temp[1] == 'n')
	   {						/*  START # 6.05  */
		/*  Set up temporary character string, temp1.  */
		j = 2;
		k = 0;
		while( (temp[j] != '\0') && (k < 15) )
		{
		   temp1[k] = temp[j];
		   j++;
		   k++;
		}
		temp1[k] = '\0';
		(void)sscanf(temp1,"%d",&numblt);
		if(numblt > 26) numblt = 26;
	   }						/*  END # 6.05  */

	   /*  Take care of all other arguments.  */
	   else
	   {						/*  START # 6.1  */
		/*  Set temporary character string, temp1.  */
		j = 3;
		k = 0;
		while( (temp[j] != '\0') && (k < 15) )
		{
		   temp1[k] = temp[j];
		   j++;
		   k++;
		}
		temp1[k] = '\0';

		/*  -hd & -hh - head diameter & height.  */
		if(temp[1] == 'h')
		{					/*  START # 7  */
		   if(temp[2] == 'd')	/*  Head diameter.  */
		   {
			(void)sscanf(temp1,"%lf",&hd);
		   }
		   else if(temp[2] =='h')	/*  Head height.  */
		   {
			(void)sscanf(temp1,"%lf",&hh);
		   }
		}					/*  END # 7  */

		/*  -wd & -wh - washer diameter & height.  */
		else if(temp[1] == 'w')
		{					/*  START # 8  */
		   if(temp[2] == 'd')	/*  Washer diameter.  */
		   {
			(void)sscanf(temp1,"%lf",&wd);
		   }
		   else if(temp[2] == 'h')	/*  Washer height.  */
		   {
			(void)sscanf(temp1,"%lf",&wh);
		   }
		}					/*  END # 8  */

		/*  -sd & -sh - stem washer diameter & height.  */
		else if(temp[1] == 's')
		{					/*  START # 9  */
		   if(temp[2] == 'd')	/*  Stem diameter.  */
		   {
			(void)sscanf(temp1,"%lf",&sd);
		   }
		   else if(temp[2] == 'h')	/*  Stem height.  */
		   {
			(void)sscanf(temp1,"%lf",&sh);
		   }
		}					/*  END # 9  */
	   }						/*  END # 6.1  */

	}						/*  END # 3  */
   }							/*  END # 2  */

   /*  Print out bolt dimensions.  */
   (void)printf("\noption:  %d - ",iopt);
   if(iopt == 1) (void)printf("bolt head\n");
   if(iopt == 2) (void)printf("head & washer\n");
   if(iopt == 3) (void)printf("head, washer, & stem\n");
   if(iopt == 4) (void)printf("head & stem\n");
   (void)printf(".g file:  %s\n",filemged);
   (void)printf("head diameter:  %f, & height:  %f\n",hd,hh);
   (void)printf("washer diameter:  %f, & height:  %f\n",wd,wh);
   (void)printf("stem diameter:  %f, & height:  %f\n",sd,sh);
   (void)printf("number of bolts:  %d\n\n",numblt);
   (void)fflush(stdout);

   /*  Open mged file for writing to.  */
   fpw = wdb_fopen(filemged);

   /*  Write ident record.  */
   mk_id(fpw,"bolts");

   for(i=0; i<numblt; i++)	/*  Loop for each bolt created.  */
   {							/*  START # 20  */

   /*  Create all solids needed.  */
   /*  Create solids of bolt head.  */
   leg = tan(PI / 6.) * hd / 2.;
   hyp = leg * leg + (hd / 2.) * (hd / 2.);
   hyp = sqrt(hyp);
   /*  Bolt head is two solids, create first solid.  */
   pts[0][0] = (fastf_t) ((-hd) / 2.);
   pts[0][1] = (fastf_t)leg;
   pts[0][2] = (fastf_t)hh;
   pts[1][0] = (fastf_t)0.;
   pts[1][1] = (fastf_t)hyp;
   pts[1][2] = (fastf_t)hh;
   pts[2][0] = (fastf_t)0.;
   pts[2][1] = (fastf_t)(-hyp);
   pts[2][2] = (fastf_t)hh;
   pts[3][0] = (fastf_t) ((-hd) / 2.);
   pts[3][1] = (fastf_t)(-leg);
   pts[3][2] = (fastf_t)hh;
   pts[4][0] = (fastf_t) ((-hd) / 2.);
   pts[4][1] = (fastf_t)leg;
   pts[4][2] = (fastf_t)0.;
   pts[5][0] = (fastf_t)0.;
   pts[5][1] = (fastf_t)hyp;
   pts[5][2] = (fastf_t)0.;
   pts[6][0] = (fastf_t)0.;
   pts[6][1] = (fastf_t)(-hyp);
   pts[6][2] = (fastf_t)0.;
   pts[7][0] = (fastf_t) ((-hd) / 2.);
   pts[7][1] = (fastf_t)(-leg);
   pts[7][2] = (fastf_t)0.;
   solnam[6] = 97 + i;
   solnam[7] = '1';
   mk_arb8(fpw,solnam,&pts[0][X]);

   /*  Create second solid.  */
   pts[0][0] = (fastf_t) (hd / 2.);
   pts[0][1] = (fastf_t)leg;
   pts[0][2] = (fastf_t)hh;
   pts[1][0] = (fastf_t)0.;
   pts[1][1] = (fastf_t)hyp;
   pts[1][2] = (fastf_t)hh;
   pts[2][0] = (fastf_t)0.;
   pts[2][1] = (fastf_t)(-hyp);
   pts[2][2] = (fastf_t)hh;
   pts[3][0] = (fastf_t) (hd / 2.);
   pts[3][1] = (fastf_t)(-leg);
   pts[3][2] = (fastf_t)hh;
   pts[4][0] = (fastf_t) (hd / 2.);
   pts[4][1] = (fastf_t)leg;
   pts[4][2] = (fastf_t)0.;
   pts[5][0] = (fastf_t)0.;
   pts[5][1] = (fastf_t)hyp;
   pts[5][2] = (fastf_t)0.;
   pts[6][0] = (fastf_t)0.;
   pts[6][1] = (fastf_t)(-hyp);
   pts[6][2] = (fastf_t)0.;
   pts[7][0] = (fastf_t) (hd / 2.);
   pts[7][1] = (fastf_t)(-leg);
   pts[7][2] = (fastf_t)0.;
   solnam[7] = '2';
   mk_arb8(fpw,solnam,&pts[0][X]);

   /*  Create washer if necessary.  */
   if( (iopt == 2) || (iopt == 3) )
   {
	bs[0] = (fastf_t)0.;
	bs[1] = (fastf_t)0.;
	bs[2] = (fastf_t)0.;
	ht[0] = (fastf_t)0.;
	ht[1] = (fastf_t)0.;
	ht[2] = (fastf_t)(-wh);
	rad = (fastf_t) (wd / 2.);
	solnam[7] = '3';
	mk_rcc(fpw,solnam,bs,ht,rad);
   }

   /*  Create bolt stem if necessary.  */
   if( (iopt == 3) || (iopt == 4) )
   {
	bs[0] = (fastf_t)0.;
	bs[1] = (fastf_t)0.;
	bs[2] = (fastf_t)0.;
	ht[0] = (fastf_t)0.;
	ht[1] = (fastf_t)0.;
	ht[2] = (fastf_t)(-sh);
	rad = (fastf_t) (sd / 2.);
	solnam[7] = '4';
	mk_rcc(fpw,solnam,bs,ht,rad);
   }

   /*  Create all regions.  */
   /*  First must initialize list.  */
   BU_LIST_INIT(&comb.l);

   /*  Create region for first half of bolt head.  */
   solnam[7] = '1';
   (void)mk_addmember(solnam,&comb.l,NULL, WMOP_INTERSECT);
   solnam[7] = '2';
   (void)mk_addmember(solnam,&comb.l,NULL, WMOP_SUBTRACT);
   /*  Subtract washer if it exists.  */
   if( (iopt == 2) || (iopt == 3) )
   {
	solnam[7] = '3';
	(void)mk_addmember(solnam,&comb.l,NULL, WMOP_SUBTRACT);
   }
   /*  Subtract stem if it exists.  */
   if( (iopt == 3) || (iopt == 4) )
   {
	solnam[7] = '3';
	(void)mk_addmember(solnam,&comb.l,NULL, WMOP_SUBTRACT);
   }
   regnam[6] = 97 + i;
   regnam[7] = '1';
   mk_lfcomb(fpw,regnam,&comb,1);

   /*  Create region for second half of bolt head.  */
   solnam[7] = '2';
   (void)mk_addmember(solnam,&comb.l,NULL, WMOP_INTERSECT);
   /*  Subtract washer if it exists.  */
   if( (iopt == 2) || (iopt == 3) )
   {
	solnam[7] = '3';
	(void)mk_addmember(solnam,&comb.l,NULL, WMOP_SUBTRACT);
   }
   /*  Subtract stem if it exists.  */
   if( (iopt == 3) || (iopt == 4) )
   {
	solnam[7] = '4';
	(void)mk_addmember(solnam,&comb.l,NULL, WMOP_SUBTRACT);
   }
   regnam[7] = '2';
   mk_lfcomb(fpw,regnam,&comb,1);

   /*  Create region for washer if it exists.  */
   if( (iopt == 2) || (iopt == 3) )
   {
	solnam[7] = '3';
	(void)mk_addmember(solnam,&comb.l,NULL, WMOP_INTERSECT);
	/*  Subtract bolt stem if it exists.  */
	if(iopt == 3)
	{
	   solnam[7] = '4';
	   (void)mk_addmember(solnam,&comb.l,NULL, WMOP_SUBTRACT);
	}
	regnam[7] = '3';
	mk_lfcomb(fpw,regnam,&comb,1);
   }

   /*  Create region for bolt stem if it exists.  */
   if( (iopt == 3) || (iopt == 4) )
   {
	solnam[7] = '4';
	(void)mk_addmember(solnam,&comb.l,NULL, WMOP_INTERSECT);
	regnam[7] = '4';
	mk_lfcomb(fpw,regnam,&comb,1);
   }

   /*  Create a group containing all bolt regions.  */
   /*  First initialize the list.  */
   BU_LIST_INIT(&comb1.l);
   /*  Add both bolt head regions to the list.  */
   regnam[7] = '1';
   (void)mk_addmember(regnam,&comb1.l,NULL, WMOP_UNION);
   regnam[7] = '2';
   (void)mk_addmember(regnam,&comb1.l,NULL, WMOP_UNION);
   /*  Add washer region if necessary.  */
   if( (iopt == 2) || (iopt == 3) )
   {
	regnam[7] = '3';
	(void)mk_addmember(regnam,&comb1.l,NULL, WMOP_UNION);
   }
   /*  Add bolt stem region if necessary.  */
   if( (iopt == 3) || (iopt == 4) )
   {
	regnam[7] = '4';
	(void)mk_addmember(regnam,&comb1.l,NULL, WMOP_UNION);
   }
   /*  Actually create the group.  */
   grpnam[4] = 97 + i;
   mk_lfcomb(fpw,grpnam,&comb1,0);

   }							/*  END # 20  */

   /*  Close mged file.  */
   wdb_close(fpw);
   return 0;
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
