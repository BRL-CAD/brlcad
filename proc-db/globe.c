/*	W D B _ E X A M P L E . C
 *
 *  Create a BRL-CAD geometry database from C code.
 * 
 *  Note that this is for writing (creating/appending) a database.  
 *  There is currently no API for modifying a database.
 *
 *  Note that since the values in the database are stored in millimeters.
 *  This constrains the arguments to the mk_* routines to also be in 
 *  millimeters.
 */
#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

char *progname ="(noname)";

void usage()
{
	fprintf(stderr, "Usage: %s db_file.g [stepSize [finalSize [initialSize]]]\n", progname);
	exit(-1);
}

int
main(int ac, char *av[])
{
	struct rt_wdb *db_fp;
	point_t p1;
	int is_region;
	unsigned char rgb[3];
	struct wmember wm_hd; /* defined in wdb.h */
	struct wmember bigList;
	double initialSize=900.0;
	double finalSize=1000.0;
	double stepSize=1.0;
	double currentSize=0.0;
	int counter=0;
	char name[256]="";
	char prevName[256]="";
	char shaderparams[256]="";

	progname = *av;

	if (ac < 2) usage();

	if (ac > 2) stepSize=(double)atof(av[2]);
	if (ac > 3) finalSize=(double)atof(av[3]);
	if (ac > 4) initialSize=(double)atof(av[4]);

	if ((db_fp = wdb_fopen(av[1])) == NULL) {
		perror(av[1]);
		exit(-1);
	}

	mk_id(db_fp, "Sphere Database"); /* create the database header record */

	/* make a region that is the union of these two objects
	 * To accomplish this, we need to create a linked list of the
	 * items that make up the combination.  The wm_hd structure serves
	 * as the head of the list of items.
	 */
	BU_LIST_INIT(&wm_hd.l);
	BU_LIST_INIT(&bigList.l);

	/* make a sphere centered at 1.0, 2.0, 3.0 with radius .75 */
	VSET(p1, 0.0, 0.0, 0.0);
	VSET(rgb, 130, 253, 194); /* some green */
	is_region = 1;


	sprintf(name, "land.s");

	mk_sph(db_fp, name, p1, initialSize);
	mk_addmember(name, &wm_hd.l, WMOP_UNION);
	mk_lcomb(db_fp, "land.r", &wm_hd, is_region, "plastic", "di=.8 sp=.2", rgb, 0);
	mk_addmember("land.r", &bigList.l, WMOP_UNION);

	VSET(rgb, 130, 194, 253); /* a light blue */
	sprintf(prevName, "land.s");
	for (counter=0, currentSize=initialSize+stepSize; currentSize < finalSize; counter += 1, currentSize+=stepSize) {
	  BU_LIST_INIT(&wm_hd.l);

	  sprintf(name, "air.%d.s", counter);
	  mk_sph(db_fp, name , p1, currentSize);
	  mk_addmember(name, &wm_hd.l, WMOP_UNION);
	  mk_addmember(prevName, &wm_hd.l, WMOP_SUBTRACT);

	  sprintf(name, "air.%d.r", counter);
/*	  sprintf(shaderparams, "", (float)finalSize/currentSize); */
	  /* can also use d for delta and s for scale */

	  mk_lcomb(db_fp,
		   name,	/* Name of the db element created */
		   &wm_hd,		/* list of elements & boolean operations */
		   is_region,	/* Flag:  This is a region */
		   "plastic",	/* optical shader */
		   shaderparams, /* shader parameters */
		   rgb,		/* item color */
		   0);		/* inherit (override) flag */
	  mk_addmember(name, &bigList.l, WMOP_UNION);
	  sprintf(prevName, "%s", name);
	}


	/* Create the combination
	 * In this case we are going to make it a region (hence the
	 * is_region flag is set, and we provide shader parameter information.
	 *
	 * When making a combination that is NOT a region, the region flag
	 * argument is 0, and the strings for optical shader, and shader
	 * parameters should (in general) be null pointers.
	 */
	mk_lcomb(db_fp,
		 "air.r",	/* Name of the db element created */
		 &bigList,		/* list of elements & boolean operations */
		 is_region,	/* Flag:  This is a region */
		 NULL,	/* optical shader */
		 NULL, /* shader parameters */
		 NULL,		/* item color */
		 0);		/* inherit (override) flag */

	wdb_close(db_fp);
	return 0;
}
