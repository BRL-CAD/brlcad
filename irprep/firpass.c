
/*  File:  firpass.c  */
/*  S.Coates - 13 March 1991  */
/*  This version ONLY shoots down the x-axis.  */
/*  This version rotates the starting point and directions.  */

/*	CHANGES		*/
/*	10 December 1990 - 'Dimension' arrays using malloc.  */
/*	17 December 1990 - Incorperate subroutine rotate & radians.  */
/*	19 February 1991 - No defaults for material properties.  */
/*	 5 March 1991    - Creates PRISM, generic, or geometric file.  */
/*	13 March 1991    - Corrects problem writing out material.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*  The following are needed when using rt_shootray.  */

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#define BLANK " "	/*  Define a blank.  */
#define ADJTOL 1.e-1	/*  Tolerance for adjacent regions.  */
#define COSTOL 1.e-20	/*  Tolerance for costheta & costheta1,  */
			/*  to avoid dividing by zero.  */
#define VOLVAR 0.1	/*  Variation in volume & surface area  */
			/*  that is allowed, before flag is set.  */
#define ZEROTOL 1.e-20	/*  Tolerance for dividing by zero.  */
#define NORMTOL 1.e-2	/*  Tolerance for finding cummulative normals.  */
#define PI 3.14159265358979323846264	/*  Pi.  */
#define ALPHA 25.	/*  Rotation about z-axis.  */
#define BETA 50.	/*  Rotation about y-axis.  */
#define GAMMA 35.	/*  Rotation about x-axis.  */

struct application ap;	/*  Structure passed between functions.  */
extern int hit();	/*  User supplied hit function.  */
extern int miss();	/*  User supplied miss function.  */
extern int ovrlap();	/*  User supplied overlap function.  */
extern void rotate();	/*  Subroutine to rotate a point.  */
extern double radians();/*  Subroutines to find angle in radians.  */

/*  Define structure.  */
struct table
{
	char *regname;		/*  region name  */
	short mat;		/*  material code  */
	double cumnorm[3];	/*  cummulative normal vector sum  */
				/*  for the exterior free surfaces  */
	double cumvol[3];	/*  cummulative volume sums for each  */
				/*  direction fired  */
	double centroid[3];	/*  centroid calculation  */
	double cumfs[3];	/*  cummulative surface area for */
				/*  each air type, 0-exterior air,  */
				/*  1-crew compartment air, 2-engine  */
				/*  compartment air  */
	int *adjreg;		/*  adjacent region, 0=>no touch,  */
				/*  1=>touch  */
	double *ssurarea[3];	/*  cummulative sum of shared surface  */
				/*  area of adjacent regions  */
	double surarea[3];	/*  surface area of that region, one  */
				/*  for each direction rays fired  */
};
struct table *region;		/*  name of table structure  */
int iprev,icur;			/*  previous & current region hit  */
double area;			/*  area of "ray"  */
double leavept[3];		/*  leave point for previous region hit  */
double lnormal[3];		/*  normal for leaving ray  */
int whichview;			/*  flag for direction rays are fired  */

struct structovr		/*  structure for recording overlaps  */
{
	int *ovrreg;		/*  regions that overlap  */
	double *ovrdep;		/*  maximum depth of overlap  */
};
struct structovr *overlaps;	/*  name of structovr structure  */
double newx,newy,newz;		/*  Point after it has been rotated back.  */

main(argc,argv)

int argc;
char **argv[];

{
	int i,j,ii,jj;	/*  variables used in loops  */
	int ia;		/*  variable used to set short to int  */

	int index;	/*  Index for rt_dirbuild and  */
			/*  rt_gettree.  */
	static struct rt_i *rtip;	/*  rtip is a pointer to  */
					/*  a structure of type  */
					/*  rt_i, defined in headers  */
	char idbuf[132];	/*  first ID record info, used  */
				/*  in rt_dirbuild  */
	double gridspace;	/*  variables to determine where ray starts  */
	int flag;		/*  flag for checking variance of volume &  */
				/*  surface area  */
	double smallest;	/*  used in checking variance, finds  */
				/*  smallest occurance  */
	double checkvar;	/*  variance allowed  */
	double checkdiff;	/*  actual difference  */
	int num;		/*  number of regions  */
	double total;		/*  used in computing different values  */
	FILE *fp,*fopen();	/*  used for writing output to file  */
	char filename[16];	/*  file name for writing output to  */
	int iwrite;		/*  0=>write to standard out, 1=>write  */
				/*  to file  */
	int typeout;		/*  type of file written, 0=>PRISM, 1=>  */
				/*  generic, 2=>geometric, 3=>non-readable  */
				/*  geometric file  */
	int typeouta;		/*  geometric type file  */
	double denom;		/*  used when computing normal  */
	FILE *fp3,*fopen();	/*  used for error file  */
	char fileerr[16];	/*  used for error file  */
	FILE *fp4,*fopen();	/*  used for reading material id file  */
	char fileden[16];	/*  used for reading material id file  */
	FILE *fp5,*fopen();	/*  used for creating generic file  */
	char filegen[16];	/*  used for creating generic file  */
	FILE *fp6,*fopen();	/*  used for creating gemetric file  */
	char filegeo[16];	/*  used for creating gemetric file  */
	int numadjreg;		/*  used for finding the number of  */
				/*  adj regions  */
	double diagonal;	/*  Length of diagonal of bounding rpp.  */
	double xmin,xmax;	/*  Minimum & maximum x.  */
	double ymin,ymax;	/*  Minimum & maximum y.  */
	double zmin,zmax;	/*  Minimum & maximum z.  */

/*************************************************************************/

	/*  Variables used in facet grouping file.  */
	FILE *fp1,*fopen();	/*  facet grouping file  */
	char facfile[16];	/*  facet grouping file name  */
	int facnum;		/*  facet number  */
	char facname[25];	/*  facet name  */
	int c;			/*  used in finding facet name from  */
				/*  region name  */
	int icnt;		/*  used in finding facet name  */
	int factype;		/*  type of facet  */
	double facarea;		/*  area of facet (sq meters)  */
	double facmass;		/*  mass of facet (kg)  */
	double facspheat;	/*  specific heat of facet (J/kg deg C)  */
	double face1;		/*  emissivity of facet wrt  */
				/*  environment (>0)  */
	double face2;		/*  emissivity of facet wrt other  */
				/*  facets (>0)  */
	double facabs;		/*  absorptivity of facet  */
	double facnorm[3];	/*  surface normal of facet  */
	double faccv;		/*  convection coefficient  */
	int fack;		/*  facet number of facet seen by back  */
				/*  surface of current facet  */
	int facl;		/*  facedt number of facet seen by front  */
				/*  surface of current facet  */
	double facshape;	/*  shape factors for engine & track  */
				/*  (between 0 & 1)  */
	double facradius;	/*  hub radius (m)  */
	double facfric;		/*  bearing friction constant for wheels  */
				/*  (J)  */
	struct table1		/*  Used to read in material property file.  */
	{
	  double d;		/*  Density.  */
	  double sh;		/*  Specific heat.  */
	  double a;		/*  Absorptivity.  */
	  double e1;		/*  Emissivity.  */
	  double e2;		/*  Emissivity.  */
	  double tc;		/*  Thermal conductivity.  */
	  char m[25];		/*  Material.  */
	};
	struct table1 matprop[41];
	char line[151];		/*  used for reading files, one line at  */
				/*  a time  */

	/*  Variables used in second pass file.  */
	FILE *fp2,*fopen();	/*  second pass file  */
	char spfile[16];	/*  second pass file  */
	double spsarea;		/*  average of shared surface area for  */
				/*  second pass  */

/************************************************************************/
	double angle[3];	/*  Angles of rotation in radians.  */
				/*  angle[0] is rotation about x-axis,  */
				/*  angle[1] is rotation about y-axis,  */
				/*  angle[2] is rotation about z-axis,  */
	double strtpt[3];	/*  Starting point of fired ray.  */
	double strtdir[3];	/*  Starting direction of fired ray.  */
	double r[3],t[3];	/*  Used in computing rotations.  */
	double center[3];	/*  Center of bounding rpp.  */
	int numext;		/*  Number of exterior surfaces.  */
	int numint;		/*  Number of interior surfaces.  */
	int numsol;		/*  Number of solar loaded surfaces.  */

   /*  Check to see if arguments implimented correctly.  */
   if(argv[1]==NULL || argv[2]==NULL)
   {
	(void)fprintf(stderr,"\nusage:  firpass file.g objects\n\n");
   }
	
   else
   {


	/*  Ask if output goes to standard out or a file.  */
	(void)printf("Write output to standard out (0) or a file (1) or ");
	(void)printf("not at all (2)?  ");
	fflush(stdout);
	scanf("%d",&iwrite);
	if((iwrite != 0) && (iwrite != 1)) iwrite=2;
	if(iwrite == 1)
	{
	  (void)printf("Enter name of file output is to be written ");
	  (void)printf("to (15 char max).  ");
	  fflush(stdout);
	  scanf("%s",filename);
	  fp=fopen(filename,"w");
	}

	/*  Get error file name.  */
	(void)printf("Enter name of error file to be created ");
	(void)printf("(15 char max).  ");
	fflush(stdout);
	scanf("%s",fileerr);

	/*  Get second pass file name.  */
	(void)printf("Enter name of second pass file to be ");
	(void)printf("created (15 char max).  ");
	fflush(stdout);
	scanf("%s",spfile);

	/*  Get name of material id file.  */
	(void)printf("Enter name of material id file to be read ");
	(void)printf("(15 char max).  ");
	fflush(stdout);
	scanf("%s",fileden);

	/*  What types of files are to be written?  */
	(void)printf("Enter type of file to be written.\n");
	(void)printf("\t0 - PRISM file\n");
	(void)printf("\t1 - Generic file\n");
	(void)printf("\t2 - Geometric properties file\n");
	(void)fflush(stdout);
	(void)scanf("%d",&typeout);

	/*  Get facet file name.  */
	if( typeout == 0 )
	{
	   (void)printf("Enter name of facet file to be created. ");
	   (void)printf("(15 char max)  ");
	   fflush(stdout);
	   scanf("%s",facfile);
	}

	/*  Get generic file name.  */
	if( typeout == 1 )
	{
	   (void)printf("Enter name of generic file to be created. ");
	   (void)printf("(15 char max)  ");
	   fflush(stdout);
	   scanf("%s",filegen);
	}
	
	/*  Get geometric file name.  */
	if( typeout == 2 )
	{
	   (void)printf("Do you want a readable (0) or non-readable (1) ");
	   (void)printf("geometric file?  ");
	   (void)fflush(stdout);
	   (void)scanf("%d",&typeouta);
	   typeout += typeouta;
	   (void)printf("Enter name of geometric properties file to be ");
	   (void)printf("created (15 char max).  ");
	   fflush(stdout);
	   scanf("%s",filegeo);
	}

/*
 *	(void)printf("typeout = %d\n",typeout);
 *	(void)fflush(stdout);
 */

	/*  Open & read material id file.  */
	fp4=fopen(fileden,"r");
/*
 *	(void)printf("Materail id file open for reading.\n");
 *	fflush(stdout);
 */
	/*  Assumption is made that material ids run from 0 to 40  */
	/*  and ALL numbers 0-40 have a density.  */

	for(i=0; i<41; i++)
	{
		(void)fgets(line,150,fp4);
		(void)sscanf(line,"%*d%lf%lf%lf%lf%lf%25c",
		   &matprop[i].d,&matprop[i].sh,&matprop[i].a,
		   &matprop[i].e1,&matprop[i].tc,matprop[i].m);
		matprop[i].e2 = matprop[i].e1;
		for(j=0; j<25; j++)
		{
		   if(matprop[i].m[j] == '\n')
		   {
			for(ii=j; ii<25; ii++)
			{
			   matprop[i].m[ii] = ' ';
			}
		   }
		}
		/*  Substitute blanks for tabs in material name.  */
		for(j=0; j<25; j++)
		{
			if(matprop[i].m[j] == '\t')
			   matprop[i].m[j] = ' ';
		}

/*
 *		(void)printf("%d, %f, %f, %f, %f, %f\n",i,
 *		   matprop[i].d,matprop[i].sh,matprop[i].a,
 *		   matprop[i].e1,matprop[i].e2);
 *		fflush(stdout);
 */
 
	}
	fclose(fp4);

	/*  Print out the name of the file being used.  */
	(void)printf("File Used:  %s\n",argv[1]);
	fflush(stdout);
	if(iwrite == 1)
	{
	  (void)fprintf(fp,"File Used:  %s,  ",argv[1]);
	  fflush(fp);
	}

	/*  Print out name of material id file being used.  */
	(void)printf("Material ID File:  %s\n",fileden);
	fflush(stdout);
	if(iwrite == 1)
	{
		(void)fprintf(fp,"Material ID File:  %s\n",fileden);
		fflush(fp);
	}

	/*  Build the directory.  */
	index = 1;	/*  Setup index for rt_dirbuild  */
	rtip=rt_dirbuild(argv[index],idbuf,sizeof(idbuf));
	(void)printf("Database Title:  %s\n",idbuf);
	fflush(stdout);
	if(iwrite == 1)
	{
	  (void)fprintf(fp,"Database Title:  %s\n",idbuf);
	  fflush(fp);
	}

	/*  Set useair to 1, to show hits of air.  */
	rtip->useair = 1;

	/*  Load desired objects.  */
	index = 2;	/*  Set index for rt_gettree.  */
	while(argv[index] != NULL)
	{
		rt_gettree(rtip,argv[index]);
		index=index+1;
	}

	/*  Find total number of regions.  */
	num = (int)rtip->nregions;
/*
 *	(void)printf("\n Number of Regions? = %d\n",num);
 *	fflush(stdout);
 */

	/*  The number of regions (num) is now known.  It  */
	/*  is time to use the malloc command to 'dimension'  */
	/*  the appropriate arrays and structures.  */

	(void)printf("Mallocing arrays.\n");
	(void)fflush(stdout);

	region = malloc(num * sizeof (*region) );
	overlaps = malloc(num * sizeof (*overlaps) );

	for(i=0; i<num; i++)
	{
		region[i].adjreg = malloc(num * sizeof (int) );
		region[i].ssurarea[0] = malloc(num * sizeof (double) );
		region[i].ssurarea[1] = malloc(num * sizeof (double) );
		region[i].ssurarea[2] = malloc(num * sizeof (double) );

		overlaps[i].ovrreg = malloc(num * sizeof (int) );
		overlaps[i].ovrdep = malloc(num * sizeof (double) );
	}

	/*  Now that the arrays are 'dimensioned' zero ALL variables.  */

	(void)printf("Zeroing variables.\n");
	(void)fflush(stdout);

	for(i=0; i<num; i++)
	{
		region[i].regname="\0";
		region[i].cumnorm[0]=0.;
		region[i].cumnorm[1]=0.;
		region[i].cumnorm[2]=0.;
		region[i].cumvol[0]=0.;
		region[i].cumvol[1]=0.;
		region[i].cumvol[2]=0.;
		region[i].centroid[0]=0.;
		region[i].centroid[1]=0.;
		region[i].centroid[2]=0.;
		region[i].cumfs[0]=0.;
		region[i].cumfs[1]=0.;
		region[i].cumfs[2]=0.;
		region[i].surarea[0]=0.;
		region[i].surarea[1]=0.;
		region[i].surarea[2]=0.;
		for(j=0; j<num; j++)
		{
			region[i].adjreg[j]=0;
			region[i].ssurarea[0][j]=0.;
			region[i].ssurarea[1][j]=0.;
			region[i].ssurarea[2][j]=0.;
		}
	}

	/*  Zero overlaps.  */
	for(i=0; i<num; i++)
	{
	   for(j=0; j<num; j++)
	   {
		overlaps[i].ovrreg[j] = 0;
		overlaps[i].ovrdep[j] = 0.;
	   }
	}

	/*  Get database ready by starting preparation.  */
	rt_prep(rtip);

	/*  Find center of bounding rpp.  */
	center[X] = rtip->mdl_min[X] + (rtip->mdl_max[X]
		- rtip->mdl_min[X]) / 2.;
	center[Y] = rtip->mdl_min[Y] + (rtip->mdl_max[Y]
		- rtip->mdl_min[Y]) / 2.;
	center[Z] = rtip->mdl_min[Z] + (rtip->mdl_max[Z]
		- rtip->mdl_min[Z]) / 2.;

	/*  Find length of diagonal of bounding rpp.  */
	diagonal = (rtip->mdl_max[X] - rtip->mdl_min[X])
			* (rtip->mdl_max[X] - rtip->mdl_min[X])
	         + (rtip->mdl_max[Y] - rtip->mdl_min[Y])
			* (rtip->mdl_max[Y] - rtip->mdl_min[Y])
	         + (rtip->mdl_max[Z] - rtip->mdl_min[Z])
			* (rtip->mdl_max[Z] - rtip->mdl_min[Z]);
	diagonal = sqrt(diagonal) / 2. + 0.5;

	/*  Find minimum and maximums.  */
	xmin = center[X] - diagonal;
	xmax = center[X] + diagonal;
	ymin = center[Y] - diagonal;
	ymax = center[Y] + diagonal;
	zmin = center[Z] - diagonal;
	zmax = center[Z] + diagonal;

	/*  Print center of bounding rpp, diagonal, maximum, &  */
	/*  minimum.  */
	(void)printf("Center of bounding rpp ( %f, %f, %f )\n",
		center[X],center[Y],center[Z]);
	(void)printf("Length of diagonal of bounding rpp:  %f\n",
		diagonal);
	(void)printf("Minimums & maximums\n");
	(void)printf("  x:  %f - %f\n",xmin,xmax);
	(void)printf("  y:  %f - %f\n",ymin,ymax);
	(void)printf("  z:  %f - %f\n",zmin,zmax);
	(void)fflush(stdout);

	/*  Write model minimum & maximum.  */
	(void)printf("Model minimum & maximum.\n");
	fflush(stdout);
	(void)printf("\tX:  %f to %f\n\tY:  %f to %f\n\tZ:  %f to %f\n\n",
		rtip->mdl_min[X],rtip->mdl_max[X],
		rtip->mdl_min[Y],rtip->mdl_max[Y],
		rtip->mdl_min[Z],rtip->mdl_max[Z]);
	fflush(stdout);
	if(iwrite == 1)
	{
	  (void)fprintf(fp,"Model minimum & maximum.\n");
	  (void)fprintf(fp,"\tX:  %f to %f\n\tY:  %f to %f\n\tZ:  %f to %f\n",
		rtip->mdl_min[X],rtip->mdl_max[X],
		rtip->mdl_min[Y],rtip->mdl_max[Y],
		rtip->mdl_min[Z],rtip->mdl_max[Z]);
	  fflush(fp);
	}

	/*  User enters grid spacing.  All units are in mm.  */
	(void)printf("Enter grid spacing (mm) for fired rays.\n");
	fflush(stdout);
	scanf("%lf",&gridspace);

	if(iwrite == 0)
	{
	  (void)printf("grid spacing:  %lf\n",gridspace);
	  fflush(stdout);
	}
	else if(iwrite == 1)
	{
	  (void)fprintf(fp,"grid spacing:  %lf,  ",gridspace);
	  fflush(fp);
	}

	/*  Find area of "ray".  */
	area = gridspace * gridspace;
	if(iwrite == 0)
	{
	  (void)printf("area=%f\n",area);
	  fflush(stdout);
	}
	if(iwrite == 1)
	{
	  (void)fprintf(fp,"area=%f\n",area);
	  fflush(fp);
	}

	/*  Set up other parameters for rt_shootray.  */
	ap.a_hit=hit;	/*  User supplied hit function.  */
	ap.a_miss=miss;	/*  User supplied miss function.  */
	ap.a_overlap=ovrlap;	/*  User supplied overlap function.  */

	/*  Set other parameters that need to be  */
	/*  set for rt_shootray.  */
	ap.a_rt_i=rtip;	/*  Pointer from rt_dirbuild.  */
	ap.a_onehit=0;	/*  Hit flag:  return all hits.  */
	ap.a_level=0;	/*  Recursion level for diagnostics  */
	ap.a_resource=0;	/*  address of resource structure:  */
				/*  NULL  */

	/*  Set up for shooting rays down the x axis (positive to  */
	/*  negative).  */

	(void)printf("\nSHOOTING DOWN X-AXIS\n");
	fflush(stdout);

	whichview=0;

 	strtpt[X] = xmax;
	strtpt[Y] = ymin + gridspace / 2.;
	strtpt[Z] = zmin + gridspace / 2.;
	strtdir[X] = (-1.);
	strtdir[Y] = 0.;
	strtdir[Z] = 0.;

	/*  Put angle into radians.  */
	angle[X] = radians((double)GAMMA);
	angle[Y] = radians((double)BETA);
	angle[Z] = radians((double)ALPHA);

	/*  Rotate starting point.  (new pt = C + R[P - C])  */
	t[X] = strtpt[X] - center[X];
	t[Y] = strtpt[Y] - center[Y];
	t[Z] = strtpt[Z] - center[Z];

	rotate(t,angle,r);

	ap.a_ray.r_pt[X] = center[X] + r[X];
	ap.a_ray.r_pt[Y] = center[Y] + r[Y];
	ap.a_ray.r_pt[Z] = center[Z] + r[Z];

	/*  Rotate firing direction.  (new dir = R[D])  */

	rotate(strtdir,angle,r);

	ap.a_ray.r_dir[X] = r[X];
	ap.a_ray.r_dir[Y] = r[Y];
	ap.a_ray.r_dir[Z] = r[Z];

	while( strtpt[Z] <= zmax )
	{

		/*  Set to show no previous shots.  */
		iprev=(-99);
		r[X]=xmax - center[X] + 5.;
		r[Y]=ymax - center[X] + 5.;
		r[Z]=zmax - center[X] + 5.;


		rotate(r,angle,t);

		leavept[X] = center[X] + t[X];
		leavept[Y] = center[Y] + t[Y];
		leavept[Z] = center[Z] + t[Z];

		/*  Print starting point & direction.  */
/*
 *		(void)printf("Starting point & direction.\n");
 *		(void)printf("  %f, %f, %f\n  %f, %f, %f\n",
 *		   ap.a_ray.r_pt[X],ap.a_ray.r_pt[Y],ap.a_ray.r_pt[Z],
 *		   ap.a_ray.r_dir[X],ap.a_ray.r_dir[Y],ap.a_ray.r_dir[Z]);
 *		(void)fflush(stdout);
 */

		lnormal[X]=0.;
		lnormal[Y]=0.;
		lnormal[Z]=0.;

		/*  Call rt_shootray.  */
		(void)rt_shootray( &ap );

		strtpt[Y] += gridspace;
		if( strtpt[Y] > ymax )
		{
			strtpt[Y] = ymin + gridspace / 2.;
			strtpt[Z] += gridspace;

		}

		t[X] = strtpt[X] - center[X];
		t[Y] = strtpt[Y] - center[Y];
		t[Z] = strtpt[Z] - center[Z];


		rotate(t,angle,r);

		ap.a_ray.r_pt[X] = center[X] + r[X];
		ap.a_ray.r_pt[Y] = center[Y] + r[Y];
		ap.a_ray.r_pt[Z] = center[Z] + r[Z];
	}

	if(iwrite == 0)
	{
	  (void)printf("\nNumber of regions:  %d\n\n",num);
	  fflush(stdout);
	}
	if(iwrite == 1)
	{
	  (void)fprintf(fp,"Number of regions:  %d\n",num);
	  fflush(fp);
	}

	/*  Compute the volume & surface area of each region.  */
	i=0;
	while( i < num )
	{
		/*  Finish computing centroids.  */
/*
 *		(void)printf("\tcentroid before division:  %f, %f, %f\n",
 *		   region[i].centroid[0],region[i].centroid[1],
 *		   region[i].centroid[2]);
 *		(void)printf("\tvolumes:  %f, %f, %f\n",region[i].cumvol[0],
 *		   region[i].cumvol[1],region[i].cumvol[2]);
 *		fflush(stdout);
 */
		total = region[i].cumvol[0] + region[i].cumvol[1] +
		   region[i].cumvol[2];
/*
 *		(void)printf("\tcummulative volume:  %f",total);
 *		fflush(stdout);
 */
		if( (total < -ZEROTOL) || (ZEROTOL < total) )
		{
		   region[i].centroid[X] = region[i].centroid[X]/total;
		   region[i].centroid[Y] = region[i].centroid[Y]/total;
		   region[i].centroid[Z] = region[i].centroid[Z]/total;
		}
/*
 *		(void)printf("volumes:  %f, %f, %f\n",region[i].cumvol[0],
 *		   region[i].cumvol[1],region[i].cumvol[2]);
 *		(void)printf("areas:  %f, %f, %f\n",region[i].surarea[0],
 *		   region[i].surarea[1],region[i].surarea[2]);
 */

		/*  Check for variance of volume.  */
		flag=0;
		region[i].cumvol[1]=flag;

		/*  Check for variance of surface area.  */
		flag=0;
		region[i].surarea[1]=flag;

		/*  Check for variance of shared surface area.  */
		for(j=0; j<num; j++)
		{
		   if(region[i].adjreg[j] == 1)
		   {

/*
 *			(void)printf("reg %d, adj reg %d\n",i,j);
 *			(void)printf("  shared area:  %f, %f, %f\n",
 *			   region[i].ssurarea[0][j],
 *			   region[i].ssurarea[1][j],
 *			   region[i].ssurarea[2][j]);
 *			fflush(stdout);
 */
			flag = 0;
			region[i].ssurarea[1][j] = flag;
		   }
		}

		/*  Finish finding cummulative normal of exterior  */
		/*  free surface.  */

		/*  Print out normal before normalizing.  */
		(void)printf("Normal before normalizing\n");
		(void)printf("  %f, %f, %f\n",region[i].cumnorm[X],
			region[i].cumnorm[Y],region[i].cumnorm[Z]);
		(void)fflush(stdout);

		if( ( (-NORMTOL < region[i].cumnorm[X]) &&
			(region[i].cumnorm[X] < NORMTOL) ) &&
		    ( (-NORMTOL < region[i].cumnorm[Y]) &&
			(region[i].cumnorm[Y] < NORMTOL) ) &&
		    ( (-NORMTOL < region[i].cumnorm[Z]) &&
			(region[i].cumnorm[Z] < NORMTOL) ) )
		{
			region[i].cumnorm[X] = 0.;
			region[i].cumnorm[Y] = 0.;
			region[i].cumnorm[Z] = 0.;
		}

		else
		{
		   denom = sqrt( region[i].cumnorm[X] * region[i].cumnorm[X] +
		              region[i].cumnorm[Y] * region[i].cumnorm[Y] +
		              region[i].cumnorm[Z] * region[i].cumnorm[Z]);
		   if(denom > ZEROTOL)
		   {
			region[i].cumnorm[X] /= denom;
			region[i].cumnorm[Y] /= denom;
			region[i].cumnorm[Z] /= denom;
		   }
		   else
		   {
			region[i].cumnorm[X] = 0.;
			region[i].cumnorm[Y] = 0.;
			region[i].cumnorm[Z] = 0.;
		   }
		}

		i++;
	}

	if(iwrite == 0)
	{ (void)printf("\n\n\nPRINT OUT STRUCTURE\n");
	fflush(stdout);
	i=0;
	while( i < num )
	{
		(void)printf("region #:  %d, name:  %s\n",i,region[i].regname);
		(void)printf("\tmaterial code:  %d\n",region[i].mat);
		fflush(stdout);

		if(region[i].cumvol[1] == 1)
		{
		   (void)printf("\tvolume:  %f - difference is above",
			region[i].cumvol[0]);
		   (void)printf(" %f variance.\n",VOLVAR);
		   fflush(stdout);
		}
		else
		{
		   (void)printf("\t volume:  %f - within variance.\n",
			region[i].cumvol[0]);
		   fflush(stdout);
		}
			
		if(region[i].surarea[1] == 1)
		{
		   (void)printf("\tarea:  %f - difference is above",
			region[i].surarea[0]);
		   (void)printf(" %f variance.\n",VOLVAR);
		   fflush(stdout);
		}
		else
		{
		   (void)printf("\t area:  %f - within variance.\n",
			region[i].surarea[0]);
		   fflush(stdout);
		}
			
		(void)printf("\tcentroid:  %f, %f, %f\n",region[i].centroid[0],
			region[i].centroid[1],region[i].centroid[2]);
		fflush(stdout);
		(void)printf("\tcummulative normal of the exterior ");
		(void)printf("free surface:\n\t\t%f, %f, %f\n",
			region[i].cumnorm[X],region[i].cumnorm[Y],
			region[i].cumnorm[Z]);
		fflush(stdout);
		(void)printf("\texterior surface air:  %f\n",
			region[i].cumfs[0]);
		(void)printf("\tcrew compartment air:  %f\n",
			region[i].cumfs[1]);
		(void)printf("\tengine compartment air:  %f\n",
			region[i].cumfs[2]);
		fflush(stdout);
		for(j=0; j<num; j++)
		{
		   if(region[i].adjreg[j] == 1)
		   {
			(void)printf("\tadjreg[%d]=%d, ",j,region[i].adjreg[j]);
			(void)printf("shared surface area:  %f\n",
			   region[i].ssurarea[0][j]);
			fflush(stdout);
			if(region[i].ssurarea[1][j] == 1)
			{
			   (void)printf("\tdifference is above %f variance\n",
				VOLVAR);
			   fflush(stdout);
			}
			else
			{
			   (void)printf("\twithin variance\n");
			   fflush(stdout);
			}
		   }
		}
		i++;
	} }
	if(iwrite == 1)
	{ i=0;
	while( i < num )
	{
		(void)fprintf(fp,"region #:  %d, name:  %s\n",
		   i,region[i].regname);
		(void)fprintf(fp,"\tmaterial code:  %d\n",region[i].mat);

		if(region[i].cumvol[1] == 1)
		{
		   (void)fprintf(fp,"\tvolume:  %f - difference is above",
			region[i].cumvol[0]);
		   (void)fprintf(fp," %f variance.\n",VOLVAR);
		   fflush(fp);
		}
		else
		{
		   (void)fprintf(fp,"\t volume:  %f - within variance.\n",
			region[i].cumvol[0]);
		   fflush(fp);
		}
			
		if(region[i].surarea[1] == 1)
		{
		   (void)fprintf(fp,"\tarea:  %f - difference is above",
			region[i].surarea[0]);
		   (void)fprintf(fp," %f variance.\n",VOLVAR);
		   fflush(fp);
		}
		else
		{
		   (void)fprintf(fp,"\t area:  %f - within variance.\n",
			region[i].surarea[0]);
		   fflush(fp);
		}
			
		(void)fprintf(fp,"\tcentroid:  %f, %f, %f\n",
			region[i].centroid[0],
			region[i].centroid[1],region[i].centroid[2]);
		fflush(fp);
		(void)fprintf(fp,"\tcummulative normal of the exterior ");
		(void)fprintf(fp,"free surface:\n\t\t%f, %f, %f\n",
			region[i].cumnorm[X],region[i].cumnorm[Y],
			region[i].cumnorm[Z]);
		fflush(fp);
		(void)fprintf(fp,"\texterior surface air:  %f\n",
			region[i].cumfs[0]);
		(void)fprintf(fp,"\tcrew compartment air:  %f\n",
			region[i].cumfs[1]);
		(void)fprintf(fp,"\tengine compartment air:  %f\n",
			region[i].cumfs[2]);
		fflush(fp);
		for(j=0; j<num; j++)
		{
		   if(region[i].adjreg[j] == 1)
		   {
			(void)fprintf(fp,"\tadjreg[%d]=%d, ",
			   j,region[i].adjreg[j]);
			(void)fprintf(fp,"shared surface area:  %f;\n",
			   region[i].ssurarea[0][j]);
			fflush(fp);
			if(region[i].ssurarea[1][j] == 1)
			{
			   (void)fprintf(fp,"\tdifference is above ");
			   (void)fprintf(fp,"%f variance\n",
				VOLVAR);
			   fflush(fp);
			}
			else
			{
			   (void)fprintf(fp,"\twithin variance\n");
			   fflush(fp);
			}
		   }
		}
		i++;
	}

	/*  Print out names of all files used.  */
	(void)fprintf(fp,"\n\nSUMMARY OF FILES USED & CREATED\n");
	(void)fprintf(fp,"\t.g file used:  %s\n",argv[1]);
	(void)fprintf(fp,"\tregions used:\n");
	fflush(fp);
	i=2;
	while(argv[i] != NULL)
	{
	   (void)fprintf(fp,"\t\t%s\n",argv[i]);
	   fflush(fp);
	   i++;
	}
	(void)fprintf(fp,"\tmaterial id file used:  %s\n",fileden);
	if(iwrite == 1)
	{
	   (void)fprintf(fp,"\toutput file created:  %s\n",filename);
	}
	(void)fprintf(fp,"\tsecond pass file created:  %s\n",spfile);
	(void)fprintf(fp,"\terror file created:  %s\n",fileerr);
	if( typeout == 0 ) (void)fprintf(fp,
	   "\tfacet file created:  %s\n",facfile);
	if( typeout == 1 ) (void)fprintf(fp,
	   "\tgeneric file created:  %s\n",filegen);
	if( typeout == 2 ) (void)fprintf(fp,
	   "\tgeometric file created:  %s\n",filegeo);
	fflush(fp);

	fclose(fp); }

/****************************************************************************/
	if( typeout == 0 ) {			/*  START # 11 */

	/*  Open facet file for writing to .  */
	fp1=fopen(facfile,"w");

	/*  Print type number of file (02) and description.  */
	(void)fprintf(fp1,"02\tFacet file for use with PRISM.\n");
	fflush(fp1);

	/*  Print header information for facedt file.  */
	(void)fprintf(fp1," FN DESCRIPTION               TY");
	(void)fprintf(fp1,"    AREA    MASS  SPHEAT      E1");
	(void)fprintf(fp1,"      E2   ABSOR\n");
	fflush(fp1);
	(void)fprintf(fp1,"   SN(X)   SN(Y)   SN(Z)    CONV");
	(void)fprintf(fp1,"       K       L   SHAPE\n");
	fflush(fp1);

	/*  Make calculations to get PRISM information & then  */
	/*  write to file for each region (or facet as PRISM  */
	/*  calls them).  */

	for(i=0; i<num; i++)
	{
		/*  Find facet number (>=1).  */
		facnum = i + 1;
/*
 *		(void)printf("\n\tfacet number:  %d\n",facnum);
 *		fflush(stdout);
 */

		/*  Find facet name (25 char max).  */
		c=region[i].regname[0];
		icnt = 0;
		while(c != '\0')
		{
			icnt += 1;
			c=region[i].regname[icnt];
		}
		icnt -= 1;
		for(j=24; j>=0; j--)
		{
			if(icnt >= 0)
			{
			   facname[j] = region[i].regname[icnt];
			   icnt -= 1;
			}
			else
			{
			   facname[j] = ' ';
			   icnt -= 1;
			}
		}
/*
 *		(void)printf("name:%.25s:\n",facname);
 *		fflush(stdout);
 */

		/*  Find facet type (for now all are 1).  */
		factype = 1;
/*
 *		(void)printf("facet type:  %d  ",factype);
 *		fflush(stdout);
 */

		/*  Find area of facet or as it has been called  */
		/*  exterior surface area (sq meters).  If exterior  */
		/*  surface area is 0 print engine compartment area.  */
		facarea = region[i].cumfs[0] * (1.e-6);
		if( (-ZEROTOL < facarea) && (facarea < ZEROTOL) )
		   facarea = region[i].cumfs[2] * (1.e-6);
/*
 *		(void)printf("area:  %8.3f  ",facarea);
 *		fflush(stdout);
 */

		/*  Set material id number.  */
		if(region[i].mat <= 40.) ia = (int)region[i].mat;
		else ia = 0;

		/*  Find the mass of the facet (kg).  This is  */
		/*  dependent on the volume and material code.  */
		facmass = matprop[ia].d * region[i].cumvol[0] * (1.e-9);
/*
 *		(void)printf("mass:  %8.3f\n",facmass);
 *		fflush(stdout);
 */

		/*  Find the specific heat of the facet (J/kg deg C).  */
		/*  Read from material file.  */
		facspheat = matprop[ia].sh;

		/*  Emissivities with respect to the environment &  */
		/*  other facets.  They are read from material file.  */
		face1 = matprop[ia].e1;
		face2 = matprop[ia].e2;

		/*  Absorptivity of facet.  Read from material file.  */
		facabs = matprop[ia].a;
/*
 *		(void)printf("specific heat:  %8.3f  emissivities:  %8.3f & %8.3f  absorb:  %8.3f\n",
 *		   facspheat,face1,face2,facabs);
 *		fflush(stdout);
 */

		/*  Surface normal of facet.  Must be rotated back first.  */
		facnorm[X] = region[i].cumnorm[0];
		facnorm[Y] = region[i].cumnorm[1];
		facnorm[Z] = region[i].cumnorm[2];
/*
 *		(void)printf("normal:  %8.3f %8.3f %8.3f\n",
 *		   facnorm[0],facnorm[1],facnorm[2]);
 */

		/*  Convection coefficient of facet.  Currently  */
		/*  set to 1.  */
		faccv = 1.;

		/*  Facets seen by the back & front of current  */
		/*  facet.  Will be set to 0 thereby assuming  */
		/*  there is not radiative exchange taking  */
		/*  place.  */
		fack = 0;
		facl = 0;
/*
 *		(void)printf("faccv:  %8.3f, fack:  %3d, facl:  %3d\n",
 *		   faccv,fack,facl);
 *		fflush (stdout);
 */

		/*  Shape factors for engine & track facets  */
		/*  (between 0 & 1).  Currently set to 0.  */
		facshape = 0.;

		/*  Hub radius (m).  Currently set to 0.  */
		facradius = 0.;

		/*  Bearing friction constant (J) for wheels.  */
		/*  Currently set to 0.  */
		facfric = 0.;
/*
 *		(void)printf("facshape:  %8.3f, facradius:  %8.3f, facfric:  %8.3f\n",
 *		   facshape,facradius,facfric);
 *		fflush(stdout);
 */

		/*  Print information to the facet file.  */
		(void)fprintf(fp1,"%3d %.25s%3d%8.3f%8.3f%8.3f",
		    facnum,facname,factype,facarea,facmass,facspheat);
		(void)fprintf(fp1,"%8.3f%8.3f%8.3f\n",face1,
		    face2,facabs);
		fflush(fp1);

		(void)fprintf(fp1,"%8.3f%8.3f%8.3f%8.3f%8d%8d",
		    facnorm[0],facnorm[1],facnorm[2],faccv,fack,facl);
		(void)fprintf(fp1,"%8.3f%8.3f%8.3f\n",facshape,
		    facradius,facfric);
		fflush(fp1);

	}

	/*  Close facet file.  */
	fclose(fp1);

	}						/*  END # 11  */

/****************************************************************************/

	/*  Open and write to generic file if needed.  */

	/*  Format for generic file.  */
	/*	1  region #  region name  volume  density  ...  */
	/*		thermal conductivity  specific heat  material  */
	/*                                                             */
	/*	2  region #  # of exterior convection surfaces  ...  */
	/*		# of internal convection surfaces  */
	/*	   areas of all external surfaces  */
	/*	   areas of all internal surfaces  */
	/*                                                             */
	/*	3  region #  # of solar loaded surfaces  */
	/*	   surface area  surface normal (X Y Z)  absorptivity  */

	if(typeout == 1)
	{						/*  START # 12  */
	   /*  Open generic file.  */
	   fp5 = fopen(filegen,"w");

	   /*  Write out # 1 information.  */
	   for(i=0; i<num; i++)
	   {
		/*  Find region name (25 char).  */
		c = region[i].regname[0];
		icnt = 0;
		while(c != '\0')
		{
		   icnt += 1;
		   c = region[i].regname[icnt];
		}
		icnt -= 1;
		for(j=24; j>=0; j--)
		{
		   if(icnt >= 0)
		   {
			facname[j] = region[i].regname[icnt];
			icnt -= 1;
		   }
		   else
		   {
			facname[j] = ' ';
			icnt -= 1;
		   }
		}

		if(region[i].mat <= 40) ia = (int)region[i].mat;
		else ia = 0;

		(void)fprintf(fp5,"1 %6d %.25s %.3e %.3e ",
			(i+1),facname,(region[i].cumvol[X]*1.e-9),
			matprop[ia].d);
		(void)fprintf(fp5,"%.3e %.3e %.25s\n",matprop[ia].tc,
			matprop[ia].sh,matprop[ia].m);
		(void)fflush(fp5);
	   }

	   /*  Write out # 2 information.  */
	   for(i=0; i<num; i++)
	   {
		/*  Any exterior surfaces?  */
		numext = 0;
		if(region[i].cumfs[0] > ZEROTOL) numext = 1;

		/*  Any interior surfaces?  */
		numint = 0;
		if(region[i].cumfs[1] > ZEROTOL) numint += 1;
		if(region[i].cumfs[2] > ZEROTOL) numint += 1;

		(void)fprintf(fp5,"2 %6d   %3d          %3d\n",
			(i+1),numext,numint);
		(void)fflush(fp5);

		if(numext == 1)
		{
		   (void)fprintf(fp5,"  %.3e\n",(region[i].cumfs[0]*1.e-6));
		   (void)fflush(fp5);
		}
		if(numint > 0)
		{
		   if(region[i].cumfs[1] > ZEROTOL)
		   {
			(void)fprintf(fp5,"  %.3e",(region[i].cumfs[1]*1.e-6));
		   }
		   if(region[i].cumfs[2] > ZEROTOL)
		   {
			(void)fprintf(fp5," %.3e",(region[i].cumfs[2]*1.e-6));
		   }
		   (void)fprintf(fp5,"\n");
		   (void)fflush(stdout);
		}
	   }

	   /*  Write out # 3 information.  */
	   for(i=0; i<num; i++)
	   {
		numsol = 0;
		if(region[1].cumfs[0] > ZEROTOL) numsol = 1;
		(void)fprintf(fp5,"3 %6d %3d\n",(i+1),numsol);
		(void)fflush(fp5);
		if(numsol > 0)
		{
		   if(region[i].mat <= 40) ia = (int)region[i].mat;
		   else ia = 0;

		   (void)fprintf(fp5,"  %.3e %+.3e %+.3e %+.3e %.3e\n",
			(region[i].cumfs[0]*1.e-6),region[i].cumnorm[X],
			region[i].cumnorm[Y],region[i].cumnorm[Z],
			matprop[ia].a);
		   (void)fflush(fp5);
		}
	   }

	   /*  Close generic file.  */
	   fclose(fp5);

	}						/*  END # 12  */

/****************************************************************************/

	/*  Open and write to geometric file if needed.  */

	if( (typeout == 2) || (typeout==3) )
	{						/*  START # 13  */

	   /*  Open geometry file.  */
	   fp6 = fopen(filegeo,"w");

	   /*  Readable geometric file.  */

	   /*  Format for readable geometric file.  */
	   /*	region #  region name  centroid-X  Y  Z  ...  */
	   /*		volume (m**3)  mass (kg)  */
	   /*						      */
	   /*	region #  ext surface area (m**2)  ...  */
	   /*		engine surface area (m**2)  ...  */
	   /*		crew surface area (m**2)  ...  */
	   /*		closed compartment surface area (m**2)  */
	   /*						      */
	   /*	region #  material code  density (kg/m3)  ...  */
	   /*		specific heat  absorptivity  emissivity  ...  */
	   /*		thermal conductivity (W/mK)  material  */
	   /*						      */
	   /*	region #  all adjacent regions (up to 20)  */

	   if(typeout == 2) {
	   /*  Write to geometric file.  */
	   (void)fprintf(fp6,"\nGEOMETRIC FILE - from firpass\n");
	   (void)fprintf(fp6,"\n.gfile used:  %s\n",argv[1]);
	   (void)fprintf(fp6,"\tregions used:\n");
	   (void)fflush(fp6);
	   i = 2;
	   while(argv[i] != NULL)
	   {
		(void)fprintf(fp6,"\t\t%s\n",argv[i]);
		(void)fflush(fp6);
		i++;
	   }
	   (void)fprintf(fp6,"\n\n");
	   (void)fflush(fp6);

	   (void)fprintf(fp6,"region   region name                 ");
	   (void)fprintf(fp6,"centroid                           ");
	   (void)fprintf(fp6,"volume       mass\n");
	   (void)fprintf(fp6,"number                               ");
	   (void)fprintf(fp6,"X          Y          Z            ");
	   (void)fprintf(fp6,"(m**3)       (kg)\n");
	   (void)fflush(fp6);

	   for(i=0; i<num; i++)
	   {

		/*  Find region name (25 char).  */
		c = region[i].regname[0];
		icnt = 0;
		while(c != '\0')
		{
		   icnt += 1;
		   c = region[i].regname[icnt];
		}
		icnt -= 1;
		for(j=24; j>=0; j--)
		{
		   if(icnt >= 0)
		   {
			facname[j] = region[i].regname[icnt];
			icnt -= 1;
		   }
		   else
		   {
			facname[j] = ' ';
			icnt -= 1;
		   }
		}

		if(region[i].mat <=40) ia = (int)region[i].mat;
		else ia = 0;
		facmass = matprop[ia].d * region[i].cumvol[0] * (1.e-9);
		(void)fprintf(fp6,"%6d   %.25s   %+.3e %+.3e %+.3e   ",
			(i+1),facname,region[i].centroid[X],
			region[i].centroid[Y],region[i].centroid[Z]);
		(void)fprintf(fp6,"%.3e   %.3e\n",(region[i].cumvol[X]*1.e-9),
			facmass);
		(void)fflush(fp6);
	   }

	   (void)fprintf(fp6,"\n\n\nregion   exterior sur   engine sur    ");
	   (void)fprintf(fp6,"crew sur      closed compartment\n");
	   (void)fprintf(fp6,"number   area (m**2)    area (m**2)   ");
	   (void)fprintf(fp6,"area (m**2)   sur area (m**2)\n");
	   (void)fflush(fp6);

	   for(i=0; i<num; i++)
	   {
		(void)fprintf(fp6,"%6d   %.3e      %.3e     %.3e    \n",
			(i+1),(region[i].cumfs[0]*1.e-6),
			(region[i].cumfs[2]*1.e-6),
			(region[i].cumfs[1]*1.e-6));
		(void)fflush(fp6);
	   }

	   (void)fprintf(fp6,"\n\nregion   material   density      ");
	   (void)fprintf(fp6,"specific     absorptivity   emissivity   ");
	   (void)fprintf(fp6,"thermal               material\n");
	   (void)fprintf(fp6,"number   code       (kg/m3)      ");
	   (void)fprintf(fp6,"heat                                     ");
	   (void)fprintf(fp6,"conductivity (W/mK)\n");

	   for(i=0; i<num; i++)
	   {
		if(region[i].mat <= 40) ia = (int)region[i].mat;
		else ia = 0;

		(void)fprintf(fp6,"%6d   %3d        %.3e    ",
			(i+1),region[i].mat,matprop[ia].d);
		(void)fprintf(fp6,"%.3e    %.3e      %.3e    %.3e",
			matprop[ia].sh,matprop[ia].a,matprop[ia].e1,
			matprop[ia].tc);
		(void)fprintf(fp6,"    %s\n",matprop[ia].m);
		(void)fflush(fp6);
	   }

	   (void)fprintf(fp6,"\n\nregion   adjacent\n");
	   (void)fprintf(fp6,"number   regions");
	   (void)fflush(fp6);

	   for(i=0; i<num; i++)
	   {
		(void)fprintf(fp6,"\n%6d   ",(i+1));

		for(j=0; j<num; j++)
		{
		   if(region[i].adjreg[j] == 1)
		   {
			(void)fprintf(fp6,"%4d, ",(j+1));
			(void)fflush(fp6);
		   }
		}
	   } }

	   if(typeout == 3)
	   {

	   /*  Format for non-readable geometric file.  */
	   /*	region #, region name, centroid-X, Y, Z, volume,  ...  */
	   /*		mass, exterior surface area,  ...  */
	   /*		crew surface area, engine surface area,  ...  */
	   /*		material code, density, specific heat,  ...  */
	   /*		absorptivity, emissivity, thermal conductivity,  ...  */
	   /*		all adjacent regions  */

	     for(i=0; i<num; i++)
	     {

		/*  Find region name (25 char).  */
		c = region[i].regname[0];
		icnt = 0;
		while(c != '\0')
		{
		   icnt += 1;
		   c = region[i].regname[icnt];
		}
		icnt -= 1;
		for(j=24; j>=0; j--)
		{
		   if(icnt >= 0)
		   {
			facname[j] = region[i].regname[icnt];
			icnt -= 1;
		   }
		   else
		   {
			facname[j] = ' ';
			icnt -= 1;
		   }
		}

		if(region[i].mat <=40) ia = (int)region[i].mat;
		else ia = 0;
		facmass = matprop[ia].d * region[i].cumvol[0] * (1.e-9);

		(void)fprintf(fp6,"%d,%s,%.3e,%.3e,%.3e,",
			(i+1),facname,region[i].centroid[X],
			region[i].centroid[Y],region[i].centroid[Z]);
		(void)fflush(fp6);

		(void)fprintf(fp6,"%.3e,%.3e,%.3e,%.3e,%.3e,",
			(region[i].cumvol[X]*1.e-9),facmass,
			(region[i].cumfs[0]*1.e-6),(region[i].cumfs[1]*1.e-6),
			(region[i].cumfs[2]*1.e-6));
		(void)fflush(fp6);

		(void)fprintf(fp6,"%d,%.3e,%.3e,%.3e,%.3e,%.3e,",
			region[i].mat,matprop[ia].d,matprop[ia].sh,
			matprop[ia].a,matprop[ia].e1,matprop[ia].tc);
		(void)fflush(fp6);

		ii = 0;
		for(j=0; j<num; j++)
		{
		   if( (region[i].adjreg[j] == 1) && (ii < 20) )
		   {
			(void)fprintf(fp6,"%d,",(j+1));
			(void)fflush(fp6);
			ii++;
		   }

		}
		if(ii < 20)
		{
		   for(j=ii; j<20; j++)
		   {
			(void)fprintf(fp6,"%d,",0);
			(void)fflush(fp6);
		   }
		}
		(void)fprintf(fp6,"\n");
		(void)fflush(stdout);
	     }
	   }

	   /*  Close geometric file.  */
	   fclose(fp6);

	}						/*  END # 13  */
/****************************************************************************/

	/*  Open second pass file.  */
	fp2 = fopen(spfile,"w");

	/*  Write info to second pass file.  */
	/*  Write number of regions to file.  */
	(void)fprintf(fp2,"%8d\n",num);
	fflush(fp2);

	for(i=0; i<num; i++)
	{

		/*  Write region number, centroid & material id  */
		/*  to 2nd pass file.  */
		(void)fprintf(fp2,"%8d  %.6e  %.6e  %.6e  %3d\n",
		   i,region[i].centroid[0],region[i].centroid[1],
		   region[i].centroid[2],region[i].mat);
		fflush(fp2);

		/*  Write area of adjacent region.  */
		for(j=0; j<num; j++)
		{
		   spsarea = region[i].ssurarea[0][j];
		   (void)fprintf(fp2,"%8d  %.6e\n",
		      j,spsarea);
		   fflush(fp2);
		}
	}

	/*  Close second pass file.  */
	fclose(fp2);

/****************************************************************************/

	/*  Open error file.  */
	fp3=fopen(fileerr,"w");

	/*  Write errors to error file.  */
	(void)fprintf(fp3,"\nERRORS from firpass\n\n");
	fflush(fp3);
	for(i=0; i<num; i++)
	{
	   if(region[i].cumvol[1] == 1)
	   {
		(void)fprintf(fp3,"region %d:  ",i);
		(void)fprintf(fp3,"large variance on volume:  %f\n",
		   region[i].cumvol[0]);
		fflush(fp3);
	   }

	   if(region[i].surarea[1] == 1)
	   {
		(void)fprintf(fp3,"region %d:  large variance ",i);
		(void)fprintf(fp3,"on surface area:  %f\n",
		   region[i].surarea[0]);
		fflush(fp3);
	   }

	   for(j=0; j<num; j++)
	   {
		if(region[i].ssurarea[1][j] == 1)
		{
		   (void)fprintf(fp3,"region %d:  adjacent region %d:\n",
			i,j);
		   (void)fprintf(fp3,"\tlarge variance on shared surface ");
		   (void)fprintf(fp3,"area:  %f\n",region[i].ssurarea[0][j]);
		   fflush(fp3);
		}
	   }
	}

	/*  Write overlaps to error file.  */
	(void)fprintf(fp3,"\n\n\tOVERLAPS\n\n");
	fflush(fp3);
	for(i=0; i<num; i++)
	{
	   for(j=0; j<num; j++)
	   {
		if(overlaps[i].ovrreg[j] == 1)
		{
		   (void)fprintf(fp3,"%s & %s, max depth:  %fmm  ",
			region[i].regname,region[j].regname,
			overlaps[i].ovrdep[j]);
		   fflush(fp3);
		   if(overlaps[i].ovrdep[j] < ADJTOL)
		   {
			(void)fprintf(fp3,"(within tolerance)\n");
			fflush(fp3);
		   }
		   else
		   {
			(void)fprintf(fp3,"\n");
			fflush(fp3);
		   }
		}
	   }
	}

	/*  Write number of adjacent regions to error file.  */
	(void)fprintf(fp3,"\n\nREGION NUMBER     NUMBER OF ADJACENT REGIONS\n");
	fflush(fp3);
	for(i=0; i<num; i++)
	{
	   numadjreg = 0;
	   for(j=0; j<num; j++)
	   {
		if(region[i].adjreg[j] == 1) numadjreg++;
	   }
	   (void)fprintf(fp3,"        %5d                          ",i);
	   (void)fprintf(fp3,"%5d\n",numadjreg);
	   fflush(fp3);
	}

	/*  Print out names of all files used.  */
	(void)printf("\n\nSUMMARY OF FILES USED & CREATED\n");
	(void)printf("\t.g file used:  %s\n",argv[1]);
	(void)printf("\tregions used:\n");
	fflush(stdout);
	i=2;
	while(argv[i] != NULL)
	{
	   (void)printf("\t\t%s\n",argv[i]);
	   fflush(stdout);
	   i++;
	}
	(void)printf("\tmaterial id file used:  %s\n",fileden);
	if(iwrite == 1)
	{
	   (void)printf("\toutput file created:  %s\n",filename);
	}
	(void)printf("\tsecond pass file created:  %s\n",spfile);
	(void)printf("\terror file created:  %s\n",fileerr);
	if( typeout == 0 ) (void)printf(
	   "\tfacet file created:  %s\n\n\n",facfile);
	if( typeout == 1 ) (void)printf(
	   "\tgeneric file created:  %s\n\n\n",filegen);
	if( typeout == 2 ) (void)printf(
	   "\tgeometric file created:  %s\n\n\n",filegeo);
	fflush(stdout);

	/*  Close error file.  */
	fclose(fp3);

	/*  Everything is complete, free all memory.  */

	(void)printf("Freeing memory.\n");
	(void)fflush(stdout);

	for(i=0; i<num; i++)
	{
		free(region[i].adjreg);
		free(region[i].ssurarea[0]);
		free(region[i].ssurarea[1]);
		free(region[i].ssurarea[2]);
		free(overlaps[i].ovrreg);
		free(overlaps[i].ovrdep);
	}
	free(region);
	free(overlaps);

   }
}


hit(ap,PartHeadp)
register struct application *ap;
struct partition *PartHeadp;
{

	register struct partition *pp;
	register struct hit *hitp;
	register struct soltab *stp;
	struct curvature cur;

	int index;
	int i;
	double enterpt[3];	/*  Point where ray enters.  */
	double distance;	/*  Distance between where point enters  */
				/*  and leaves.  */
	double disx,disy,disz;	/*  Used for finding distance.  */
	double costheta;	/*  Used in computing areas.  */
	double costheta1;	/*  Used in computing surface area.  */
	double costheta2;	/*  Used in computing exterior surface air.  */
	short prevregid;	/*  Previous region id.  */
	short prevair;		/*  Previous air code.  */

	pp=PartHeadp->pt_forw;
	for( ; pp != PartHeadp; pp = pp->pt_forw)
	{
/*
 *		(void)printf("Hit region %s (in %s, out %s)\n",
 *			pp->pt_regionp->reg_name,
 *			pp->pt_inseg->seg_stp->st_name,
 *			pp->pt_outseg->seg_stp->st_name);
 *		fflush(stdout);
 *		(void)printf("index to region:  %d\n",
 *			pp->pt_regionp->reg_bit);
 *		fflush(stdout);
 */

		/*  Print out region id and aircode.  */
/*
 *		(void)printf("region id:  %d, aircode:  %d\n",
 *		    pp->pt_regionp->reg_regionid,pp->pt_regionp->reg_aircode);
 *		fflush(stdout);
 */

		/*  Put region name into region[].regname.  */
/*
 *		(void)printf("Putting region name into struct.\n");
 *		fflush(stdout);
 */
		icur=pp->pt_regionp->reg_bit;	/*  region # hit  */
/*
 *		(void)printf("region %d:  *%s*\n",icur,region[icur].regname);
 *		fflush(stdout);
 */

		region[icur].regname = pp->pt_regionp->reg_name;

/*
 *		(void)printf("region #:  %d, region name:  %s\n",
 *			icur,region[icur].regname);
 *		fflush(stdout);
 */

		/*  Put material code into region[].mat.  */
		region[icur].mat = pp->pt_regionp->reg_gmater;

		/*  Find normal and hit point of entering ray.  */
/*
 *		(void)printf("Entering Ray\n");
 *		fflush(stdout);
 */
		hitp=pp->pt_inhit;
		stp=pp->pt_inseg->seg_stp;
		RT_HIT_NORM(hitp,stp,&(ap->a_ray));
		/*  Flip normal if needed.  */
		if(pp->pt_inflip)
		{
			VREVERSE(hitp->hit_normal,hitp->hit_normal);
			pp->pt_inflip=0;
		}

		/*  Find front surface area of region.  */
		costheta1 = hitp->hit_normal[X] * ap->a_ray.r_dir[X]
		          + hitp->hit_normal[Y] * ap->a_ray.r_dir[Y]
		          + hitp->hit_normal[Z] * ap->a_ray.r_dir[Z];
		if(costheta1 < 0) costheta1 = (-costheta1);
		if( costheta1 > COSTOL)
		   region[icur].surarea[whichview] += (area/costheta1);

		if(
		   (-ADJTOL < (hitp->hit_point[X] - leavept[X])) &&
		   ((hitp->hit_point[X] - leavept[X]) < ADJTOL) &&
		   (-ADJTOL < (hitp->hit_point[Y] - leavept[Y])) &&
		   ((hitp->hit_point[Y] - leavept[Y]) < ADJTOL) &&
		   (-ADJTOL < (hitp->hit_point[Z] - leavept[Z])) &&
		   ((hitp->hit_point[Z] - leavept[Z]) < ADJTOL)
		  )

		{
			/*  Find adjacent regions.  Occasionally a  */
			/*  a region will semm to be adjacent to self,  */
			/*  disreguard this.  */
			if( icur != iprev)
			{
			  region[icur].adjreg[iprev]=1;
			  region[iprev].adjreg[icur]=1;
			}

			/*  Find surface area. */
			costheta = lnormal[X]*ap->a_ray.r_dir[X]
			          + lnormal[Y]*ap->a_ray.r_dir[Y]
			          + lnormal[Z]*ap->a_ray.r_dir[Z];
			if( costheta < 0 ) costheta = (-costheta);
			if( costheta > COSTOL )
			{
			  region[icur].ssurarea[whichview][iprev] +=
				(area/costheta);
			  region[iprev].ssurarea[whichview][icur] +=
				(area/costheta);
			}
		}

		/*  Save point where ray enters region.  */
		enterpt[X]=hitp->hit_point[X];
		enterpt[Y]=hitp->hit_point[Y];
		enterpt[Z]=hitp->hit_point[Z];

		/*  Print out hit point & normal.  */
/*
 *		(void)printf("Hit Point (entering):  (%f, %f, %f)\n",
 *			hitp->hit_point[X],
 *			hitp->hit_point[Y],
 *			hitp->hit_point[Z]);
 *		fflush(stdout);
 *		(void)printf("Normal Point:  (%f, %f, %f)\n",
 *			hitp->hit_normal[X],
 *			hitp->hit_normal[Y],
 *			hitp->hit_normal[Z]);
 *		fflush(stdout);
 */

		/*  Compute cummulative free surface normal,  */
		/*  free surface area, crew compartment area,  */
		/*  & engine compartment area when ray enters  */
		/*  from another region.  */
		if(
		  (-ADJTOL < (hitp->hit_point[X] - leavept[X])) &&
			((hitp->hit_point[X] - leavept[X]) < ADJTOL) &&
		  (-ADJTOL < (hitp->hit_point[Y] - leavept[Y])) &&
			((hitp->hit_point[Y] - leavept[Y]) < ADJTOL) &&
		  (-ADJTOL < (hitp->hit_point[Z] - leavept[Z])) &&
			((hitp->hit_point[Z] - leavept[Z]) < ADJTOL)
		  )
		{
		   if((prevregid == 0) && (pp->pt_regionp->reg_regionid != 0))
		   {
			costheta2 = hitp->hit_normal[X] * ap->a_ray.r_dir[X]
			          + hitp->hit_normal[Y] * ap->a_ray.r_dir[Y]
			          + hitp->hit_normal[Z] * ap->a_ray.r_dir[Z];
			if(costheta2 < 0.) costheta2=(-costheta2);
			if(prevair == 1)
			{
			   if(costheta2 > COSTOL)
			   {
			   	region[icur].cumnorm[X] += hitp->hit_normal[X]
					/ costheta2;
			   	region[icur].cumnorm[Y] += hitp->hit_normal[Y]
					/ costheta2;
			   	region[icur].cumnorm[Z] += hitp->hit_normal[Z]
					/ costheta2;
				region[icur].cumfs[0] += (area/costheta2);
			   }
			}
			else if(prevair == 2)
			{
			   if(costheta2 > COSTOL)
				region[icur].cumfs[1] += (area/costheta2);
			}
			else if(prevair == 5)
			{
			   if(costheta2 > COSTOL)
				region[icur].cumfs[2] += (area/costheta2);
			}
		   }
		}
		/*  A ray entering from nothing.  */
		else if(pp->pt_regionp->reg_regionid != 0)
		{
		   /*  If the ray has previously hit another  */
		   /*  region this area must be taken into  */
		   /*  account.  */
		   if(iprev != -99)
		   {
			costheta2 = lnormal[X] * ap->a_ray.r_dir[X]
			          + lnormal[Y] * ap->a_ray.r_dir[Y]
			          + lnormal[Z] * ap->a_ray.r_dir[Z];
			if(costheta2 < 0.) costheta2 = (-costheta2);
			if(costheta2 > COSTOL)
			{
			   region[iprev].cumnorm[X] += lnormal[X] / costheta2;
			   region[iprev].cumnorm[Y] += lnormal[Y] / costheta2;
			   region[iprev].cumnorm[Z] += lnormal[Z] / costheta2;
			   region[iprev].cumfs[0] += (area/costheta2);
			}
		   }

		   costheta2 = hitp->hit_normal[X] * ap->a_ray.r_dir[X]
		             + hitp->hit_normal[Y] * ap->a_ray.r_dir[Y]
		             + hitp->hit_normal[Z] * ap->a_ray.r_dir[Z];
		   if(costheta2 < 0.) costheta2=(-costheta2);
		   if(costheta2 > COSTOL)
		   {
			region[icur].cumnorm[X] += hitp-> hit_normal[X]
				/ costheta2;
		   	region[icur].cumnorm[Y] += hitp-> hit_normal[Y]
				/ costheta2;
		   	region[icur].cumnorm[Z] += hitp-> hit_normal[Z]
				/ costheta2;
			region[icur].cumfs[0] += (area/costheta2);
		   }
		}
		/*  A ray entering  a region from air.  */
		if((pp->pt_regionp->reg_regionid == 0) && (prevregid != 0))
		{
		   costheta2 = lnormal[X] * ap->a_ray.r_dir[X]
		             + lnormal[Y] * ap->a_ray.r_dir[Y]
		             + lnormal[Z] * ap->a_ray.r_dir[Z];
		   if(costheta2 < 0.) costheta2=(-costheta2);
		   if(pp->pt_regionp->reg_aircode == 1)
		   {
			if(costheta2 > COSTOL)
			{
			   region[iprev].cumnorm[X] += lnormal[X] / costheta2;
			   region[iprev].cumnorm[Y] += lnormal[Y] / costheta2;
			   region[iprev].cumnorm[Z] += lnormal[Z] / costheta2;
			   region[iprev].cumfs[0] += (area/costheta2);
			}
		   }
		   else if(pp->pt_regionp->reg_aircode == 2)
		   {
			if(costheta2 > COSTOL)
			   region[iprev].cumfs[1] += (area/costheta2);
		   }
		   else if(pp->pt_regionp->reg_aircode == 5)
		   {
			if(costheta2 > COSTOL)
			   region[iprev].cumfs[2] += (area/costheta2);
		   }
		}

		/*  Curvature.  */
		RT_CURVE(&cur,hitp,stp);
		/*  Print out curvature information.  */
/*
 *		(void)printf("Principle direction of curvature:  ");
 *		(void)printf("(%f, %f, %f)\n",
 *			cur.crv_pdir[X],
 *			cur.crv_pdir[Y],
 *			cur.crv_pdir[Z]);
 *		fflush(stdout);
 *		(void)printf("Inverse radii of curvature:  c1=%f, c2=%f\n",
 *			cur.crv_c1,
 *			cur.crv_c2);
 *		fflush(stdout);
 */

		/*  Find normal and hit point of leaving ray.  */
/*
 *		(void)printf("Leaving Ray\n");
 *		fflush(stdout);
 */
		hitp=pp->pt_outhit;
		stp=pp->pt_outseg->seg_stp;
		RT_HIT_NORM(hitp,stp,&(ap->a_ray));
		/*  Flip normal if needed.  */
		if(pp->pt_outflip)
		{
			VREVERSE(hitp->hit_normal,hitp->hit_normal);
			pp->pt_inflip=0;
		}

		/*  Find back surface area of regions. */
		costheta1 = hitp->hit_normal[X] * ap->a_ray.r_dir[X]
		          + hitp->hit_normal[Y] * ap->a_ray.r_dir[Y]
		          + hitp->hit_normal[Z] * ap->a_ray.r_dir[Z];
		if( costheta1 < 0 ) costheta1 = (-costheta1);
		if( costheta1 > COSTOL )
		   region[icur].surarea[whichview] += (area/costheta1);

		/*  Save normal of leaving ray.  */
/*
 *		lnormal[X]=hitp->hit_normal[X];
 *		lnormal[Y]=hitp->hit_normal[Y];
 *		lnormal[Z]=hitp->hit_normal[Z];
 */

		/*  Print out hit point & normal.  */
/*
 *		(void)printf("Hit Point (leaving):  (%f, %f, %f)\n",
 *			hitp->hit_point[X],
 *			hitp->hit_point[Y],
 *			hitp->hit_point[Z]);
 *		fflush(stdout);
 *		(void)printf("Normal Point:  (%f, %f, %f)\n",
 *			hitp->hit_normal[X],
 *			hitp->hit_normal[Y],
 *			hitp->hit_normal[Z]);
 *		fflush(stdout);
 */

		/*  Continue finding cummulative volume.  */
		disx=(enterpt[X] - hitp->hit_point[X]) * 
		   (enterpt[X] - hitp->hit_point[X]);
		disy=(enterpt[Y] - hitp->hit_point[Y]) * 
		   (enterpt[Y] - hitp->hit_point[Y]);
		disz=(enterpt[Z] - hitp->hit_point[Z]) * 
		   (enterpt[Z] - hitp->hit_point[Z]);
		distance=sqrt(disx + disy + disz);
		region[icur].cumvol[whichview] =
		   region[icur].cumvol[whichview] + (distance * area);

		/*  Find centroid.  */
		region[icur].centroid[X] += ( distance * area * (enterpt[X] +
		   hitp->hit_point[X]) / 2.);
		region[icur].centroid[Y] += ( distance * area * (enterpt[Y] +
		   hitp->hit_point[Y]) / 2.);
		region[icur].centroid[Z] += ( distance * area * (enterpt[Z] +
		   hitp->hit_point[Z]) / 2.);

		/*  Find the cummulative normal & free surface area  */
		/*  (exterior air)  when a ray is leaving the bounding  */
		/*  rpp.  */
		if((pp->pt_forw == PartHeadp) &&
		   (pp->pt_regionp->reg_regionid != 0))
		{
		   costheta2 = hitp->hit_normal[X] * ap->a_ray.r_dir[X]
		             + hitp->hit_normal[Y] * ap->a_ray.r_dir[Y]
		             + hitp->hit_normal[Z] * ap->a_ray.r_dir[Z];
		   if(costheta2 < 0.) costheta2=(-costheta2);
		   if(costheta2 > COSTOL)
		   {
		   	region[icur].cumnorm[X] += hitp->hit_normal[X]
				/ costheta2;
		   	region[icur].cumnorm[Y] += hitp->hit_normal[Y]
				/ costheta2;
		   	region[icur].cumnorm[Z] += hitp->hit_normal[Z]
				/ costheta2;
			region[icur].cumfs[0] += (area/costheta2);
		   }
		}

		iprev=icur;
		leavept[X]=hitp->hit_point[X];
		leavept[Y]=hitp->hit_point[Y];
		leavept[Z]=hitp->hit_point[Z];

		/*  Save normal of leaving ray.  */
		lnormal[X]=hitp->hit_normal[X];
		lnormal[Y]=hitp->hit_normal[Y];
		lnormal[Z]=hitp->hit_normal[Z];

		/*  Save the region id and air code.  */
		prevregid = pp->pt_regionp->reg_regionid;
		prevair = pp->pt_regionp->reg_aircode;
	}

	return (1);
}

miss(ap)

/*  User supplied miss function.  */

register struct application *ap;
{
/*
 *	(void)printf("It was a miss.\n");
 *	fflush(stdout);
 */
	return (0);
}

ovrlap(ap,PartHeadp,reg1,reg2)

/*  User supplied overlap function.  */

register struct application *ap;
struct partition *PartHeadp;
struct region *reg1,*reg2;
{
	register struct partition *pp;
	int a,b;
	double depth;

/*
 *	(void)printf("In ovrlap.  ");
 *	fflush(stdout);
 *	(void)printf("%s & %s\n",reg1->reg_name,reg2->reg_name);
 *	fflush(stdout);
 */

	a=(int)reg1->reg_bit;
	b=(int)reg2->reg_bit;

/*
 *	(void)printf("a=%d, b=%d.  ",a,b);
 *	fflush(stdout);
 */

	/*  Enter region names incase they are never entered  */
	/*  anywhere else.  */
	region[a].regname = reg1->reg_name;
	region[b].regname = reg2->reg_name;

	overlaps[a].ovrreg[b] = 1;

/*
 *	(void)printf("ovrreg set to 1.  ");
 *	fflush(stdout);
 */

	depth = PartHeadp->pt_outhit->hit_dist - PartHeadp->pt_inhit->hit_dist;

/*
 *	(void)printf("depth set to %f.  ",depth);
 *	fflush(stdout);
 */

	if(depth > overlaps[a].ovrdep[b]) overlaps[a].ovrdep[b] = depth;

/*
 *	(void)printf("ovrdep set.\n");
 *	fflush(stdout);
 */

	return(1);
}


