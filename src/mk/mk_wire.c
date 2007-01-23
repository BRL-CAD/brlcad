/*                       M K _ W I R E . C
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
/** @file mk_wire.c
 *
 */

/*  File:  mk_wire.c  */
/*  S.Coates - 15 September 1992  */
/*  To compile for use separately:  */
/*  cc mk_wire.c /usr/brlcad/lib/libwdb.a /usr/brlcad/lib/librt.a  */
/*	-lmpc -lm -o mk_wire  */

/*  This is a program to create wiring or fuel lines.  The user  */
/*  Enters only the coordinates of the endpoints and the radius  */
/*  of the lines.  */

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
#include "db.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

#define MAXWIRESEG 10		/*  Maximum number of segments.  The  */
				/*  maximum may be no greater then 100  */
				/*  because of the way the name of the  */
				/*  segments is found.  */

int
main(int argc, char **argv)
{							/*  START # 1  */
   struct rt_wdb *fpw;		/*  File to be created.  */
   char filemged[26];		/*  Mged file name.  */
   double numseg;		/*  Number of segments.  */
   double strtpt[MAXWIRESEG][3];	/*  Start point of segment.  */
   double endpt[MAXWIRESEG][3];	/*  End point of segment.  */
   double strtrad[MAXWIRESEG];	/*  Radius at starting point of segment.  */
   double endrad[MAXWIRESEG];	/*  Radius at ending point of segment.  */

   char solcyl[8],regcyl[8];	/*  Solid & region name for cylinder (cone).  */
   char solsph[8],regsph[8];	/*  Solid & region name for sphere.  */
   char solsub1[8],solsub2[8];	/*  Solids that are subtracted.  */
   char group[6];		/*  Group name.  */

   point_t bs;			/*  Base of cone.  */
   vect_t dir;			/*  Direction of cone.  */
   fastf_t ht;			/*  Height of cone.  */
   fastf_t rdc1;		/*  Radius 1 of cone.  */
   fastf_t rdc2;		/*  Radius 2 of cone.  */
   point_t cent;		/*  Center of sphere.  */
   fastf_t rds;			/*  Radius of sphere.  */
   struct wmember comb;		/*  Used to make regions.  */
   struct wmember comb1;	/*  Used to make groups.  */

   int i;			/*  Loop counters.  */
   double r;			/*  Temporary variables.  */
   char temp[10];		/*  Temporary char string.  */
   char temp1[10];		/*  Temporary char string.  */

   /*  Set up solid, region, & group names.  */
   solcyl[0] = 's';
   solcyl[1] = '.';
   solcyl[2] = 'w';
   solcyl[3] = 'r';
   solcyl[4] = 'c';
   solcyl[5] = '#';
   solcyl[6] = '#';
   solcyl[7] = '\0';

   regcyl[0] = 'r';
   regcyl[1] = '.';
   regcyl[2] = 'w';
   regcyl[3] = 'r';
   regcyl[4] = 'c';
   regcyl[5] = '#';
   regcyl[6] = '#';
   regcyl[7] = '\0';

   solsph[0] = 's';
   solsph[1] = '.';
   solsph[2] = 'w';
   solsph[3] = 'r';
   solsph[4] = 's';
   solsph[5] = '#';
   solsph[6] = '#';
   solsph[7] = '\0';

   regsph[0] = 'r';
   regsph[1] = '.';
   regsph[2] = 'w';
   regsph[3] = 'r';
   regsph[4] = 's';
   regsph[5] = '#';
   regsph[6] = '#';
   regsph[7] = '\0';

   group[0] = 'w';
   group[1] = 'i';
   group[2] = 'r';
   group[3] = 'e';
   group[4] = 's';
   group[5] = '\0';

   solsub1[0] = 's';
   solsub1[1] = '.';
   solsub1[2] = 'w';
   solsub1[3] = 'r';
   solsub1[4] = 'c';
   solsub1[5] = '#';
   solsub1[6] = '#';
   solsub1[7] = '\0';

   solsub2[0] = 's';
   solsub2[1] = '.';
   solsub2[2] = 'w';
   solsub2[3] = 'r';
   solsub2[4] = 'c';
   solsub2[5] = '#';
   solsub2[6] = '#';
   solsub2[7] = '\0';

   /*  Find name of mged file to be created.  */
   (void)printf("Enter mged file name (25 char max).\n\t");
   (void)fflush(stdout);
   (void)scanf("%s",filemged);

   /*  Find the number of segments.  */
   (void)printf("Enter the number of segments (maximum of %d).\n\t",
	MAXWIRESEG);
   (void)fflush(stdout);
   (void)scanf("%lf",&numseg);

   /*  Check that the number of segments is less than or equal to the  */
   /*  maximum.  */
   while(numseg > MAXWIRESEG)
   {
	(void)printf("The maximum number of segments is %d.  Enter the\n",
		MAXWIRESEG);
	(void)printf("number of segments.\n\t");
	(void)fflush(stdout);
	(void)scanf("%lf",&numseg);
   }

   /*  Enter starting & ending points of segments & radi.  */
   for(i=0; i<numseg; i++)
   {							/*  START # 2  */
	if(i == 0)
	{						/*  START # 3  */
	   (void)printf("Enter starting point of segment # %d.\n\t",(i+1));
	   (void)fflush(stdout);
	   (void)scanf("%lf %lf %lf",&strtpt[i][0],&strtpt[i][1],
		&strtpt[i][2]);
	   (void)printf("Enter radius at the starting point of ");
	   (void)printf("segment # %d.\n\t",(i+1));
	   (void)fflush(stdout);
	   (void)scanf("%lf",&strtrad[i]);
	   (void)printf("Enter ending point of segment # %d.\n\t",(i+1));
	   (void)fflush(stdout);
	   (void)scanf("%lf %lf %lf",&endpt[i][0],&endpt[i][1],&endpt[i][2]);
	   (void)printf("Enter radius at the ending point of ");
	   (void)printf("segment # %d.\n\t",(i+1));
	   (void)fflush(stdout);
	   (void)scanf("%lf",&endrad[i]);
	}						/*  END # 3  */

	else
	{						/*  START # 4  */
	   strtpt[i][0] = endpt[i - 1][0];
	   strtpt[i][1] = endpt[i - 1][1];
	   strtpt[i][2] = endpt[i - 1][2];
	   strtrad[i] = endrad[i - 1];
	   (void)printf("Enter ending point of segment # %d.\n\t",(i+1));
	   (void)fflush(stdout);
	   (void)scanf("%lf %lf %lf",&endpt[i][0],&endpt[i][1],&endpt[i][2]);
	   (void)printf("Enter radius at the ending point of ");
	   (void)printf("segment # %d.\n\t",(i+1));
	   (void)fflush(stdout);
	   (void)scanf("%lf",&endrad[i]);
	}						/*  END # 4  */
   }							/*  END # 2  */

   /*  Print out all info.  */
   (void)printf("\n\nmged file created:  %s\n",filemged);
   (void)fflush(stdout);

   /*  Print out coordinates of segments.  */
   for(i=0; i<numseg; i++)
   {							/*  START # 5  */
	(void)printf("Segment # %d:  ",(i+1));
	(void)printf("(%f,%f,%f)", strtpt[i][0],strtpt[i][1],strtpt[i][2]);
        (void)printf(" %f\n", strtrad[i]);
	(void)printf("              ");
	(void)printf("(%f,%f,%f)", endpt[i][0],endpt[i][1],endpt[i][2]);
	(void)printf("%f\n",endrad[i]);
	(void)fflush(stdout);
   }							/*  END # 5  */

   /*  Open mged file.  */
   fpw = wdb_fopen(filemged);

   /*  Write ident record.  */
   mk_id(fpw,"Wiring");

   /*  Create solids.  */

   /*  Create solid for each segment.  */
   for(i=0; i<numseg; i++)
   {							/*  START # 10  */
	/*  Base of cone.  */
	bs[0] = (fastf_t)strtpt[i][0];
	bs[1] = (fastf_t)strtpt[i][1];
	bs[2] = (fastf_t)strtpt[i][2];

	/*  Direction of cone (unit vector).  */
	r = (endpt[i][0] - strtpt[i][0]) * (endpt[i][0] - strtpt[i][0])
	  + (endpt[i][1] - strtpt[i][1]) * (endpt[i][1] - strtpt[i][1])
	  + (endpt[i][2] - strtpt[i][2]) * (endpt[i][2] - strtpt[i][2]);
	r = sqrt(r);
	dir[0] = (fastf_t)( (endpt[i][0] - strtpt[i][0]) / r);
	dir[1] = (fastf_t)( (endpt[i][1] - strtpt[i][1]) / r);
	dir[2] = (fastf_t)( (endpt[i][2] - strtpt[i][2]) / r);

	/*  Height of cone.  */
	ht = (fastf_t)r;

	/*  Radius 1 of cone.  */
	rdc1 = (fastf_t)strtrad[i];

	/*  Radius 2 of cone.  */
	rdc2 = (fastf_t)endrad[i];

	/*  Fill in correct number in solid name.  */
	(void)sprintf(temp,"%d",i);
	if(i < 10)
	{						/*  START # 11  */
	   solcyl[5] = '0';
	   solcyl[6] = temp[0];
	}						/*  END # 11  */
	else if( (10 <= i) || (i < 100) )
	{						/*  START # 12  */
	   solcyl[5] = temp[0];
	   solcyl[6] = temp[1];
	}						/*  END # 12  */
	else
	{						/*  START # 13  */
	   (void)printf("** ERROR ** i = %d\n",i);
	   (void)fflush(stdout);
	}						/*  END # 13  */

	/*  Make cylinder.  */
	mk_cone(fpw,solcyl,bs,dir,ht,rdc1,rdc2);

   }							/*  END # 10  */

   /*  Create solid for each sphere.  */
   for(i=1; i<numseg; i++)

   /*  Sphere 0 does not exist since there is one less sphere  */
   /*  than segment.  */

   {							/*  START # 20  */
	/*  Center of sphere.  */
	cent[0] = (fastf_t)strtpt[i][0];
	cent[1] = (fastf_t)strtpt[i][1];
	cent[2] = (fastf_t)strtpt[i][2];

	/*  Radius of sphere.  */
	rds = (fastf_t)strtrad[i];

	/*  Fill in correct number in solid name.  */
	(void)sprintf(temp,"%d",i);
	if(i < 10)
	{						/*  START # 21  */
	   solsph[5] = '0';
	   solsph[6] = temp[0];
	}						/*  END # 21  */
	else if( (10 <= i) || (i < 100) )
	{						/*  START # 22  */
	   solsph[5] = temp[0];
	   solsph[6] = temp[1];
	}						/*  END # 22  */
	else
	{						/*  START # 23  */
	   (void)printf("** ERROR ** i = %d\n",i);
	   (void)fflush(stdout);
	}						/*  END # 23  */

	/*  Make sphere.  */
	mk_sph(fpw,solsph,cent,rds);
   }							/*  END # 20  */

   /*  Create regions.  */

   /*  Initialize list.  */
   BU_LIST_INIT(&comb.l);

   /*  Create region for each segment.  */

   for(i=0; i<numseg; i++)
   {							/*  START # 30  */
	/*  Fill in correct number in region & solid names.  */
	(void)sprintf(temp,"%d",i);
	(void)sprintf(temp1,"%d",(i+1));

	if(i < 10)
	{						/*  START # 31  */
	   solcyl[5] = '0';
	   solcyl[6] = temp[0];
	   regcyl[5] = '0';
	   regcyl[6] = temp[0];

	   if(i < (numseg - 1) )
	   {						/*  START # 32  */
		if( (i + 1) < 10)
		{					/*  START # 33  */
		   solsub1[5] = '0';
		   solsub1[6] = temp1[0];
		}					/*  END # 33  */
		else
		{					/*  START # 34  */
		   solsub1[5] = temp1[0];
		   solsub1[6] = temp1[1];
		}					/*  END # 34  */
	   }						/*  END # 32  */
	}						/*  END # 31  */

	else if( (10 <= i) || (i < 100) )
	{						/*  START # 35  */
	   solcyl[5] = temp[0];
	   solcyl[6] = temp[1];
	   regcyl[5] = temp[0];
	   regcyl[6] = temp[1];

	   if(i < (numseg - 1) )
	   {						/*  START # 36  */
		solsub1[5] = temp1[0];
		solsub1[6] = temp1[1];
	   }						/*  END # 36  */
	}						/*  END # 35  */

	else
	{						/*  START # 37  */
	   (void)printf("** ERROR ** i = %d\n",i);
	   (void)fflush(stdout);
	}						/*  END # 37  */

	(void)mk_addmember(solcyl,&comb.l, NULL, WMOP_INTERSECT);

	if(i < (numseg - 1) )
	{						/*  START # 38  */
	   (void)mk_addmember(solsub1,&comb.l, NULL, WMOP_SUBTRACT);
	}						/*  END # 38  */

	mk_lfcomb(fpw,regcyl,&comb,1);
   }							/*  END # 30  */

   /*  Create region for each sphere.  */
   for(i=1; i<numseg; i++)
   {							/*  START # 40  */
	/*  Fill in correct region & solid names.  */
	(void)sprintf(temp,"%d",i);
	(void)sprintf(temp1,"%d",(i - 1));

	if(i < 10)
	{						/*  START # 41  */
	   solsph[5] = '0';
	   solsph[6] = temp[0];
	   regsph[5] = '0';
	   regsph[6] = temp[0];
	   solsub1[5] = '0';
	   solsub1[6] = temp1[0];
	   solsub2[5] = '0';
	   solsub2[6] = temp[0];
	}						/*  END # 41  */

	else if( (10 <= i) || (i < 100) )
	{						/*  START # 42  */
	   solsph[5] = temp[0];
	   solsph[6] = temp[1];
	   regsph[5] = temp[0];
	   regsph[6] = temp[1];
	   solsub1[5] = temp1[0];
	   solsub1[6] = temp1[1];
	   solsub2[5] = temp[0];
	   solsub2[6] = temp[1];
	}						/*  END # 42  */

	else
	{						/*  START # 43  */
	   (void)printf("** ERROR ** i = %d\n",i);
	   (void)fflush(stdout);
	}						/*  END # 43  */

	(void)mk_addmember(solsph,&comb.l, NULL, WMOP_INTERSECT);
	(void)mk_addmember(solsub1,&comb.l, NULL, WMOP_SUBTRACT);
	(void)mk_addmember(solsub2,&comb.l, NULL, WMOP_SUBTRACT);

	mk_lfcomb(fpw,regsph,&comb,1);
   }							/*  END # 40  */

   /*  Create group.  */

   /*  Initialize list.  */
   BU_LIST_INIT(&comb1.l);

   for(i=0; i<numseg; i++)
   {							/*  START # 50  */
	(void)sprintf(temp,"%d",i);

	if(i < 10)
	{						/*  START # 51  */
	   regcyl[5] = '0';
	   regcyl[6] = temp[0];
	   if(i != 0)
	   {						/*  START # 52  */
		regsph[5] = '0';
		regsph[6] = temp[0];
	   }						/*  END # 52  */
	}						/*  END # 51  */

	else if( (10 <= i) || (i < 100) )
	{						/*  START # 53  */
	   regcyl[5] = temp[0];
	   regcyl[6] = temp[1];
	   regsph[5] = temp[0];
	   regsph[6] = temp[1];
	}						/*  END # 53  */

	else
	{						/*  START # 54  */
	   (void)printf("** ERROR ** i = %d\n",i);
	   (void)fflush(stdout);
	}						/*  END # 54  */

	(void)mk_addmember(regcyl,&comb1.l, NULL, WMOP_UNION);
	if(i != 0)(void)mk_addmember(regsph,&comb1.l, NULL, WMOP_UNION);
   }							/*  END # 50  */

   mk_lfcomb(fpw,group,&comb1,0);
   wdb_close(fpw);
   return 0;
}							/*  END # 1  */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
