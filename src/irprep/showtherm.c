/*                     S H O W T H E R M . C
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
/** @file showtherm.c
 *
 *  Graphical display of output from PRISM or another infrared model.
 *  This program will read as input two different types of output
 *  files, a PRISM output file or a generic output file.  A PRISM file
 *  has a time stamp then the next line contains the ellasped time,
 *  background temperature, and six region temperatures.  Each line
 *  afterwards contains eight temperatures.  Then this sequence
 *  repeats (except for the time stamp) for each time step.  The
 *  number of regions must be known.  The first line of the generic
 *  file will contain the number of regions, the second line will
 *  contain the ellasped time, and on each of the succeeding lines
 *  will be the temperature of a region (with the background being
 *  first and the rest of the regions in order).  This pattern will
 *  repeat (except for the number of regions).  This program will
 *  write a file that can be read by Glen Durfee's 'Analyzer2' program
 *  or by my 'ir-X' program.
 *
 *  S.Coates - 30 September 1994
 *
 *  NOTE:
 *  The .g file must contain ONLY the regions contained in the
 *  PRISM (or other infrared model) output file.  For example
 *  if 'file.g' with objects 'all' and 'air' were use with firpass
 *  and secpass.  The air objects had to be removed in order for
 *  PRISM to run.  Then when running showtherm use only the object
 *  'all.'  Make sure the air was at the end so that the numbering
 *  sceme is not altered.
 *
 *	CHANGES
 *	25 November 1991 - Start of original program.
 *	12 March 1992	 - Add some comments.
 *	30 September 1994- Changed references to see to ir-X.
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"


#define MAXFIL 26	/*  Maximum number of char in file name.  */
#define TOL 1e-10	/*  Tolerance, if two numbers are w/in this  */
			/*  tolerance they are equal.  */
#if !defined(PI)
#  define PI 3.14159265358979323846264	/*  Pi.  */
#endif


struct table		/*  Table for region name & temperature.  */
{
  char regname[150];	/*  Region name.  */
  double temp;		/*  Temperature in degress C.  */
};
struct table *info;	/*  Dimension later with malloc.  */

/*  Variables needed for rt_shootray.  */
struct application ap;	/*  Application struct, passed between functions.  */
extern int hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp);	/*  User supplied hit function.  */
extern int miss(register struct application *ap_p);	/*  User supplied miss function.  */
extern int overlap(register struct application *ap_p, register struct partition *PartHeadp);	/*  User supplied overlap function.  */


int main(int argc, char **argv)
{							/*  START # 1  */
  int index;		/*  Index for rt_dirbuild & rt_gettree.  */
  static struct rt_i *rtip;/*  Used for building directory, ect.  */
  char idbuf[132];	/*  First id record in .g file.  */

  int i,j,k;		/*  Loop variables.  */
  double vec[3];		/*  Temporary vector.  */
  double r[8];		/*  Temporary variable.  */
  int c;			/*  Variable to read a character.  */
  char tmpstrng[150];	/*  Temporary string variable.  */
  FILE *fpr=NULL;	/*  Used to read a file.  */
  FILE *fpw;		/*  Used to write a file.  */
  char filetmp[MAXFIL];	/*  Temperature file name.  */
  char filernn[MAXFIL];	/*  Region # & name file.  */
  char fileout[MAXFIL];	/*  Output file name.  */
  char line[151];	/*  Used to read one line of a file.  */
  int mon,day,yr;	/*  Month, day, and year read from PRISM file.  */
  float hr,min;		/*  Hour & minute read from PRISM file.  */
  int itype;		/*  Type of temperature file, 0=>PRISM, 1=>other.  */
  int numreg;		/*  Number of regions (includes background).  */
			/*  User enters when using a PRISM file or read  */
			/*  from generic file.  */
  int numreg_read;	/*  Number of regions read from region # & name  */
			/*  file (includes background).  */
  int numreg_g;		/*  Number of regions read from .g file plus one  */
			/*  for the background.  */
  double eltim;		/*  Elasped time.  */
  double eltim_read;	/*  Elasped time read from temperature file.  */
  int frst_line;		/*  Number of regions to be read in first line  */
  /*  of PRISM file.  */
  int last_line;		/*  Number of regions to be read in last line  */
  /*  of PRISM file.  */
  int full_line;		/*  Number of full lines of PRISM file to read.  */

  double center[3];	/*  Center of bounding sphere or rpp.  */
  double radius;		/*  Radius of bounding sphere.  */
  double rppmax[3];	/*  Maximum of bounding rpp.  */
  double rppmin[3];	/*  Minimum of bounding rpp.  */
  double multi;		/*  Multiplication factor for radius.  */

  int region_hit;	/*  Region number hit by ray.  */
  int wide,high;		/*  Width & height of picture.  */
  double deltaw,deltah;	/*  Spacing between rays.  */

  double denom;		/*  Denominator for use in equations.  */
  double az,el;		/*  Viewing azimuth & elevation.  */
  double alpha,beta;	/*  Angles for rotation (rad).  */
  double calpha,salpha;	/*  Cosine & sine of alpha.  */
  double cbeta,sbeta;	/*  Cosine & sine of beta.  */

  /*  Check to see if arguments implemented correctly.  */
  if(argv[1]==NULL || argv[2]==NULL)
    {
      (void)fprintf(stderr,"\nusage:  showtherm file.g objects\n\n");
    }

  else
    {							/*  START # 4  */
      /*  Get beginning info such as name of temperature file,  */
      /*  name of region # & name file, type of temperature file  */
      /*  using.  */

      /*  Ask type of temperature file to be used.  */
      (void)fprintf(stderr,"Type of output file to be read 0=>PRISM, ");
      (void)fprintf(stderr,"1=>generic.\n\t");
      (void)scanf("%d",&itype);
      if(itype != 1) itype = 0;

      if(itype == 0)	/*  Read info about (name & # regions) PRISM file.  */
	{
	  (void)fprintf(stderr,"Enter name of the PRISM output ");
	  (void)fprintf(stderr,"file to be read (%d char max).\n\t",MAXFIL);
	  (void)scanf("%s",filetmp);

	  /*  Ask for number of regions.  */
	  (void)fprintf(stderr,"Enter the number of regions in the PRISM ");
	  (void)fprintf(stderr,"file, must be more\n");
	  (void)fprintf(stderr,"than eight (not including the background).\n\t");
	  (void)scanf("%d",&numreg);
	}
      else			/*  Read info about (name) generic file.  */
	{
	  (void)fprintf(stderr,"Enter name of the generic output file to be ");
	  (void)fprintf(stderr,"read (%d char max).\n\t",MAXFIL);
	  (void)scanf("%s",filetmp);
	}

      /*  Find name of region # & name file.  */
      (void)fprintf(stderr,"Enter name of region # & name file to be read ");
      (void)fprintf(stderr,"(%d char max).\n\t",MAXFIL);
      (void)scanf("%s",filernn);

      /*  Find name of output file.  */
      (void)fprintf(stderr,"Enter name of output file (%d char max).\n\t",MAXFIL);
      (void)scanf("%s",fileout);

      /*  Find elasped time to create graphical representation of.  */
      (void)fprintf(stderr,"Enter the elapsed time to create graphical ");
      (void)fprintf(stderr,"representation of.\n\t");
      (void)scanf("%lf",&eltim);

      /*  Open generic file and read number of regions if necessary.  */
      if(itype == 1)
	{
	  fpr = fopen(filetmp,"r");
	  (void)fgets(line,150,fpr);
	  (void)sscanf(line,"%d",&numreg);
	}

      /*  Add one to number of regions to include background.  */
      numreg ++;
      (void)printf("Number of regions (including ");
      (void)fflush(stdout);
      (void)printf("the background):  %d\n",numreg);
      (void)fflush(stdout);

      /*  Malloc arrays.  */
      info = (struct table *)malloc( numreg * sizeof (struct table) );

      /*  Zero all arrays.  */
      for(i=0; i<numreg; i++)
	{
	  info[i].temp = 0.;
	  for(j=0; j<150; j++)
	    {
	      info[i].regname[j] = ' ';
	    }
	}

      /*  Now read the temperature file.  */
      if(itype == 0)	/*  PRISM file.  */
	{							/*  START # 2  */
	  fpr = fopen(filetmp,"r");

	  /*  Read date and print out.  */
	  (void)fgets(line,150,fpr);
	  (void)sscanf(line,"%d %d %d %f %f",&mon,&day,&yr,&hr,&min);
	  (void)printf("%d/%d/%d ",mon,day,yr);
	  (void)printf(" %f:%f\n",hr,min);
	  (void)fflush(stdout);

	  /*  Find number of lines to read.  */
	  frst_line = 7;
	  full_line = (numreg - frst_line) / (frst_line + 1);
	  last_line = numreg - frst_line -(full_line * (frst_line + 1) );

	  /*
	   *	(void)printf("1st line contains %d regions.\n",frst_line);
	   *	(void)printf("There are %d full lines of data.\n",full_line);
	   *	(void)printf("The last line contains %d regions.\n",last_line);
	   *	(void)fflush(stdout);
	   */

	  /*  Read first line & check if correct ellasped time.  */
	  (void)fgets(line,150,fpr);
	  (void)sscanf(line,"%lf %lf %lf %lf %lf %lf %lf %lf",&eltim_read,
		       &r[0],&r[1],&r[2],&r[3],&r[4],&r[5],&r[6]);

	  /*
	   *	while ( eltim_read != eltim)
	   */
	  while ( (eltim_read < (eltim - TOL)) || ((eltim + TOL) < eltim_read) )
	    /*  Page through to end of data.  */
	    {
	      for(i=0; i<(full_line + 1); i++)
		{
		  (void)fgets(line,150,fpr);
		}
	      /*  Read next elapsed time.  */
	      (void)fgets(line,150,fpr);
	      (void)sscanf(line,"%lf %lf %lf %lf %lf %lf %lf %lf",&eltim_read,
			   &r[0],&r[1],&r[2],&r[3],&r[4],&r[5],&r[6]);
	    }

	  /*  When correct ellasped time is found, read data.  */
	  /*  Read first line of data.  */
	  for(i=0; i<frst_line; i++)
	    {
	      info[i].temp = r[i];
	    }
	  k = frst_line;	/*  Next region number of temperature to be read.  */
	  /*  Read full lines of data.  */
	  for(i=0; i<full_line; i++)
	    {
	      (void)fgets(line,150,fpr);
	      (void)sscanf(line,"%lf %lf %lf %lf %lf %lf %lf %lf",
			   &r[0],&r[1],&r[2],&r[3],&r[4],&r[5],&r[6],&r[7]);
	      for(j=0; j<(frst_line + 1); j++)
		{
		  info[k].temp = r[j];
		  k++;
		}
	    }
	  /*  Read last line of data.  */
	  (void)fgets(line,150,fpr);
	  if(last_line == 1) (void)sscanf(line,"%lf",&r[0]);
	  if(last_line == 2) (void)sscanf(line,"%lf %lf",&r[0],&r[1]);
	  if(last_line == 3) (void)sscanf(line,"%lf %lf %lf",&r[0],&r[1],&r[2]);
	  if(last_line == 4) (void)sscanf(line,"%lf %lf %lf %lf",&r[0],&r[1],&r[2],
					  &r[3]);
	  if(last_line == 5) (void)sscanf(line,"%lf %lf %lf %lf %lf",
					  &r[0],&r[1],&r[2],&r[3],&r[4]);
	  if(last_line == 6) (void)sscanf(line,"%lf %lf %lf %lf %lf %lf",
					  &r[0],&r[1],&r[2],&r[3],&r[4],&r[5]);
	  if(last_line == 7) (void)sscanf(line,"%lf %lf %lf %lf %lf %lf %lf",
					  &r[0],&r[1],&r[2],&r[3],&r[4],&r[5],&r[6]);
	  if(last_line == 8) (void)sscanf(line,"%lf %lf %lf %lf %lf %lf %lf %lf",
					  &r[0],&r[1],&r[2],&r[3],&r[4],&r[5],&r[6],&r[7]);
	  if(last_line != 0)
	    {
	      for(j=0; j<last_line; j++)
		{
		  info[k].temp = r[j];
		  k++;
		}
	    }
	  (void)printf("Prism out file read.\n");
	  (void)fflush(stdout);

	}							/*  END # 2  */

      else		/*  Read generic file.  */
	{							/*  START # 3  */
	  /*  File is alread open.  */
	  /*  Read elapsed time.  */
	  (void)fgets(line,150,fpr);
	  (void)sscanf(line,"%lf",&eltim_read);

	  while(eltim_read != eltim)	/*  Page through to end of data.  */
	    {
	      for(i=0; i<numreg; i++)
		{
		  (void)fgets(line,150,fpr);
		}
	      (void)fgets(line,150,fpr);
	      (void)sscanf(line,"%lf",&eltim_read);
	    }

	  /*  When correct ellasped time is found, read data.  */
	  for(i=0; i<numreg; i++)
	    {
	      (void)fgets(line,150,fpr);
	      (void)sscanf(line,"%lf",&r[0]);
	      info[i].temp = r[0];
	    }
	}							/*  END # 3  */

      /*  Close file.  */
      (void)fclose(fpr);

      /*  Read the region # & name file.  */
      fpr = fopen(filernn,"r");
      (void)printf("Region # & name file opened.\n");
      (void)fflush(stdout);
      numreg_read = 1;
      c = getc(fpr);
      while( (c != EOF) && (numreg_read < numreg) )
	{
	  (void)ungetc(c,fpr);
	  (void)fgets(line,150,fpr);
	  (void)sscanf(line,"%*d%s",tmpstrng);
	  for(i=0; i<150; i++)
	    {
	      info[numreg_read].regname[i] = tmpstrng[i];
	    }
	  numreg_read++;
	  c = getc(fpr);
	}
      /*  Close file.  */
      (void)fclose(fpr);

      /*  Check if the number of regions read from the output file is  */
      /*  the same as the number of regions read from the region # &  */
      /*  name file.  */
      if(numreg_read == numreg)
	{
	  (void)printf("The number of regions read from the output file ");
	  (void)printf("and the region # & name\n");
	  (void)printf("file was the same, %d (does not ",(numreg-1));
	  (void)printf("include background in number).\n");
	  (void)fflush(stdout);
	}
      if(numreg_read != numreg)
	{
	  (void)printf("The number of regions read from the output file ");
	  (void)printf("and the region # & name\n");
	  (void)printf("file was not the same, %d vs %d.\n",(numreg-1),
		       (numreg_read-1));
	  (void)printf("This is an ERROR.\n\n");
	  (void)fflush(stdout);
	}

      /*  Print out data for check.  */
      /*
       * for(i=0; i<numreg; i++)
       * {
       *	if(i==0)(void)printf("reg = %d\tname = background\ttemp = %f\n",
       *		i,info[i].temp);
       *	else (void)printf("reg = %d\tname = %s\ttemp = %f\n",
       *		i,info[i].regname,info[i].temp);
       *	(void)fflush(stdout);
       * }
       */

      /*  Build the directory.  */
      (void)printf("Building directory.\n");
      (void)fflush(stdout);
      index = 1;		/*  Set index for rt_dirbuild.  */
      rtip = rt_dirbuild(argv[index],idbuf,sizeof(idbuf));
      (void)printf("File:  %s\n",argv[index]);
      (void)fflush(stdout);
      (void)printf("Database Title:  %s\n",idbuf);
      (void)fflush(stdout);

      /*  Set useair to 1, to show hits of air.  Must show hits of air  */
      /*  since other irprep programs do.  */
      rtip->useair = 1;

      /*  Load desired objects.  */
      index = 2;		/*  Set index for rt_gettree.  */
      while(argv[index] != NULL)
	{
	  rt_gettree(rtip,argv[index]);
	  (void)printf("\t%s loaded.\n",argv[index]);
	  (void)fflush(stdout);
	  index++;
	}

      /*  Find the total number of regions in the .g file & add one  */
      /*  for background.  */
      numreg_g = (int)rtip->nregions + 1;

      if( (numreg == numreg_read) && (numreg_read == numreg_g) )
	{
	  (void)printf("The number of regions read from the output\n");
	  (void)printf("file, the region # & name file, and the .g\n");
	  (void)printf("file are all equal.  The number of regions\n");
	  (void)printf("read, including the background is %d\n",numreg_g);
	  (void)fflush(stdout);
	}
      else
	{
	  (void)printf("The number of regions read from the output\n");
	  (void)printf("file, the region # & name file, and the .g\n");
	  (void)printf("file are not all equal.\n");
	  (void)printf("\toutput file:  %d\n",numreg);
	  (void)printf("\tregion # & name file:  %d\n",numreg_read);
	  (void)printf("\t.g file:  %d\n",numreg_g);
	  (void)fflush(stdout);
	}

      /*  Start preparation.  */
      (void)printf("Preparation started.\n");
      (void)fflush(stdout);
      rt_prep(rtip);

      /*  Maximums & minimums of bounding rpp.  */
      rppmin[X] = rtip->mdl_min[X];
      rppmin[Y] = rtip->mdl_min[Y];
      rppmin[Z] = rtip->mdl_min[Z];
      rppmax[X] = rtip->mdl_max[X];
      rppmax[Y] = rtip->mdl_max[Y];
      rppmax[Z] = rtip->mdl_max[Z];

      /*  Find the center of the bounding sphere or rpp.  */
      center[X] = rppmin[X] + (rppmax[X] - rppmin[X]) / 2.;
      center[Y] = rppmin[Y] + (rppmax[Y] - rppmin[Y]) / 2.;
      center[Z] = rppmin[Z] + (rppmax[Z] - rppmin[Z]) / 2.;

      /*  Find the length of the radius of the bounding sphere.  */
      radius = (rppmax[X] - rppmin[X]) * (rppmax[X] - rppmin[X]) +
	(rppmax[Y] - rppmin[Y]) * (rppmax[Y] - rppmin[Y]) +
	(rppmax[Z] - rppmin[Z]) * (rppmax[Z] - rppmin[Z]);
      radius = sqrt(radius) / 2. + 1.;	/*  Make radius a bit longer.  */

      (void)printf("\nMinimum & maximum X:  %f - %f\n",rppmin[X],rppmax[X]);
      (void)printf("Minimum & maximum Y:  %f - %f\n",rppmin[Y],rppmax[Y]);
      (void)printf("Minimum & maximum Z:  %f - %f\n",rppmin[Z],rppmax[Z]);
      (void)printf("Center of bounding sphere:  %f, %f, %f\n",
		   center[X],center[Y],center[Z]);
      (void)printf("Radius of bounding sphere:  %f\n",radius);
      (void)printf("Enter multiplication factor for radius.\n\t");
      (void)fflush(stdout);
      (void)scanf("%lf",&multi);
      /*  Multiply radius by multiplication factor.  */
      radius = radius * multi;

      /*  Set up parameters for rt_shootray.  */
      ap.a_hit = hit;		/*  User supplied hit function.  */
      ap.a_miss = miss;		/*  User supplied miss function.  */
      ap.a_overlap = overlap;	/*  User supplied overlap function.  */
      ap.a_rt_i = rtip;		/*  Pointer from rt_dirbuild.  */
      ap.a_onehit = 1;		/*  Hit flag, stop after first hit.  */
      ap.a_level = 0;		/*  Recursion level for diagnostics.  */
      ap.a_resource = 0;		/*  Address of resource struct.  */

      /*  Open output file.  */
      fpw = fopen(fileout,"w");

      /*  User enters grid size.  */
      (void)fprintf(stderr,"Enter grid size.\n\t");
      (void)scanf("%d",&wide);
      high = wide;

      /*  User enters azimuth & elevation for viewing.  */
      (void)fprintf(stderr,"Enter azimuth & elevation.\n\t");
      (void)scanf("%lf %lf",&az,&el);
      alpha = az * PI / 180.;
      beta = (-el) * PI / 180.;
      calpha = cos(alpha);
      salpha = sin(alpha);
      cbeta = cos(beta);
      sbeta = sin(beta);

      /*  Find spacing between rays.  */
      deltaw = 2. * radius / (float)wide;
      deltah = 2. * radius / (float)high;

      /*  Print grid size, azimuth, and elevation.  */
      (void)printf("gridsize:  %d x %d\n",wide,high);
      (void)printf("azimuth:  %f degrees\n",az);
      (void)printf("elevation:  %f degrees\n",el);
      (void)fflush(stdout);

      /*  Write size of grid to output file.  */
      (void)fprintf(fpw,"%d\t%d\n",wide,high);
      (void)fflush(stdout);

      /*  Set firing direction.  Rotate (-1,0,0) to proper position.  */
      vec[X] = (-1.) * cbeta * calpha;
      vec[Y] = (-1.) * cbeta * salpha;
      vec[Z] = (-1.) * (-1.) * sbeta;
      /*  Normalize.  */
      denom = vec[X] * vec[X] + vec[Y] * vec[Y] + vec[Z] * vec[Z];
      denom = sqrt(denom);
      vec[X] /= denom;
      vec[Y] /= denom;
      vec[Z] /= denom;
      ap.a_ray.r_dir[X] = vec[X];
      ap.a_ray.r_dir[Y] = vec[Y];
      ap.a_ray.r_dir[Z] = vec[Z];

      /*
       * (void)printf("firing direction:  %f, %f, %f\n\n",ap.a_ray.r_dir[X],
       * 	ap.a_ray.r_dir[Y],ap.a_ray.r_dir[Z]);
       * (void)fflush(stdout);
       */

      /*  Set starting point.  */
      vec[X] = center[X] + radius;

      for(i=0; i<high; i++)
	{
	  vec[Z] = center[Z] + radius - (float)i * deltah;

	  for(j=0; j<wide; j++)
	    {
	      vec[Y] = center[Y] - radius + (float)j * deltaw;
	      /*  Rotate starting point.  */
	      ap.a_ray.r_pt[X] = vec[X] * cbeta * calpha +
	   	vec[Z] * sbeta * calpha - vec[Y] * salpha;
	      ap.a_ray.r_pt[Y] = vec[X] * cbeta * salpha +
	   	vec[Z] * sbeta * salpha + vec[Y] * calpha;
	      ap.a_ray.r_pt[Z] = (-vec[X]) * sbeta + vec[Z] * cbeta;

	      /*
	       *	   (void)printf("old:  %f, %f, %f - new:  %f, %f, %f\n",
	       *		vec[X],vec[Y],vec[Z],ap.a_ray.r_pt[X],ap.a_ray.r_pt[Y],
	       *		ap.a_ray.r_pt[Z]);
	       *	   (void)fflush(stdout);
	       */

	      /*  Call rt_shootray.  */
	      region_hit = rt_shootray(&ap);

	      /*  Write temperature of region to output file.  */
	      (void)fprintf(fpw,"%f\n",info[region_hit].temp);
	      (void)fflush(fpw);
	    }
	}

    }							/*  END # 4  */
  return(0);
}							/*  END # 1  */


/****************************************************************************/
/*  User supplied hit function.  */
int
hit(register struct application *ap_p, struct partition *PartHeadp, struct seg *segp)
{
  register struct partition *pp;

  /*
   * (void)printf("It is a hit.\n");
   * (void)fflush(stdout);
   */

  pp = PartHeadp->pt_forw;

  /*
   * (void)printf("Region id:  %d, name:  %s\n",
   *	(pp->pt_regionp->reg_bit + 1),pp->pt_regionp->reg_name);
   * (void)fflush(stdout);
   */

  return(pp->pt_regionp->reg_bit + 1);
}

/****************************************************************************/
/*  User supplied miss function.  */
int
miss(register struct application *ap_p)
{
  /*
   * (void)printf("It is a miss.\n");
   * (void)fflush(stdout);
   */

  return(0);
}

/****************************************************************************/
/*  User supplied overlap function.  */
int
overlap(register struct application *ap_p, register struct partition *PartHeadp)
{
  (void)printf("It is an overlap.\n");
  (void)fflush(stdout);

  return(-1);
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
