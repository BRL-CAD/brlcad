/*                         P I X 2 G . C
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
/** @file pix2g.c
 * Generates geometry from an pixmap file
 *
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"


/* workers acquire semaphore number 0 on smp machines */
#define P2G_WORKER RT_SEM_LAST
#define P2G_INIT_COUNT P2G_WORKER+1
int done=0;
int ncpu=1;

char *progname ="pix2g";

/* procedure variable start */
struct rt_wdb *db_fp;
int is_region=1;
/*	struct wmember allPixelList; */ /* defined in wdb.h */
struct wmember allScanlineList; /* defined in wdb.h */
int width=512;
int height=512;
double cellSize=1.0;
double objectSize=1.0;
const char *prototype="pixel";

int nextAvailableRow=0;

/* image file */
struct bu_mapped_file *image;

/* procedure variable end */


void usage(void)
{
	fprintf(stderr, "Usage: %s image_file.pix db_file.g [pixelWidth [pixelHeight [cellSize [objectSize]]]\n", progname);
	exit(-1);
}

void computeScanline( int pid, genptr_t arg ) {
	int i=0;
	/* working pixel component value */
	unsigned char *value = image->buf;

	/*	struct wmember *allScanlineList = (struct wmember *)arg;*/
	fflush(stdout);
	db_sync(db_fp->dbip);

	while (i < height) {
		int j;
		char scratch[256]="";
		struct wmember scanlineList;
		BU_LIST_INIT(&scanlineList.l);

		bu_semaphore_acquire(P2G_WORKER);
		i=nextAvailableRow;
		nextAvailableRow++;
		bu_semaphore_release(P2G_WORKER);

		if (i >= height) {
			break;
		}

		/* cheap pretty printing */
		if (i % 64 == 0) {
			if (i != 0) {
				bu_log("[%d%%]\n[%.4d]", (int)((float)i/(float)height*100.0), i);
			} else {
				bu_log("\n[%.4d]", i);
			}
		} else {
			bu_log(".");
		}

		for (j=0; j<width; j++) {
			/*			char solidName[256]="";*/
			unsigned int r,g,b;
			unsigned char rgb[3];
			point_t p1;
			mat_t matrix;
			struct wmember wm_hd; /* defined in wdb.h */
			BU_LIST_INIT(&wm_hd.l);

			/*			bu_log("[%f:%f]", (float)i*cellSize,(float)j*cellSize);*/

			VSET(p1,(float)i*cellSize, (float)j*cellSize, 0.0);

			/* make the primitive */
			/* we do not need to make a bazillion objects if they are not going to
			 * be modified.  as such, we created one prototypical object, and all
			 * of the regions will use it.
			 ***
			   sprintf(solidName, "%dx%d.s", i+1,j+1);
				 bu_semaphore_acquire(P2G_WORKER);
				 mk_sph(db_fp, solidName, p1, objectSize/2.0);
				 bu_semaphore_release(P2G_WORKER);
			*/

			/* make the region */
			mk_addmember(prototype, &wm_hd.l, NULL, WMOP_UNION);
			/* mk_addmember(solidName, &wm_hd.l, NULL, WMOP_UNION); */

			/* get the rgb color values */
			r = *(value+(i*width*3)+(j*3));
			g = *(value+(i*width*3)+(j*3)+1);
			b = *(value+(i*width*3)+(j*3)+2);
			VSET(rgb, r, g, b);
			/* VSET(rgb, 200 , 200, 200); */

			sprintf(scratch, "%dx%d.r", i+1,j+1);
			MAT_IDN(matrix);
			MAT_DELTAS(matrix, p1[0], p1[1], 0.0);

			bu_semaphore_acquire(P2G_WORKER);
			mk_lcomb(db_fp, scratch, &wm_hd, is_region, NULL, NULL, rgb, 0);
			bu_semaphore_release(P2G_WORKER);

			mk_addmember(scratch, &scanlineList.l, matrix, WMOP_UNION);
		}

		/* write out a combination for each scanline */
		sprintf(scratch, "%d.c", i+1);
		bu_semaphore_acquire(P2G_WORKER);
		mk_lcomb(db_fp, scratch, &scanlineList, 0, NULL, NULL, NULL, 0);
		bu_semaphore_release(P2G_WORKER);

		/* all threads keep track of the scan line (in case they get to the end first */
		sprintf(scratch, "%d.c", i+1);
		mk_addmember(scratch, &allScanlineList.l, NULL, WMOP_UNION);
	}

}

int
main(int ac, char *av[])
{
	char imageFileName[256]="";
	char databaseFileName[256]="";
	char scratch[256]="";
	point_t origin;

	progname = *av;

	if (ac < 3) usage();

	sprintf(imageFileName, "%s", av[1]);
	sprintf(databaseFileName, "%s", av[2]);

	if (ac > 3) width=(int)atoi(av[3]);
	if (ac > 4) height=(int)atoi(av[4]);
	if (ac > 5) cellSize=(double)atof(av[5]);
	if (ac > 6) objectSize=(double)atof(av[6]);

	/*	bu_log("{%s} {%s} {%s} {%s} {%s} {%s} {%s}\n", av[0], av[1], av[2], av[3], av[4], av[5], av[6]); */

	if ((db_fp = wdb_fopen(databaseFileName)) == NULL) {
	  bu_log("unable to open database [%s]\n", databaseFileName);
		perror("Unable to open database file");
		exit(-1);
	}

	if ((image = bu_open_mapped_file(imageFileName, NULL)) == NULL) {
	  bu_log("unable to open image [%s]\n", imageFileName);
		perror("Unable to open file");
		exit(-2);
	}

	bu_log("Loading image %s from file...", imageFileName);
	if (image->buflen < width * height * 3) {
		bu_log("\nWARNING: %s needs %d bytes, file only contains %d bytes\n", width*height*3, image->buflen);
	} else if (image->buflen > width* height * 3) {
		bu_log("\nWarning: Image file size is larger than specified texture size\n");
	}
	bu_log("...done loading image\n");

	bu_log("Image size is %dx%d (%d bytes)\n", width, height, image->buflen);
	bu_log("Objects are %f with %f spacing\n", objectSize, cellSize);


	sprintf(scratch, "%s Geometry Image", imageFileName);
	mk_id(db_fp, scratch); /* create the database header record */

	/* make a region that is the union of these two objects
	 * To accomplish this, we need to create a linked list of the
	 * items that make up the combination.  The wm_hd structure serves
	 * as the head of the list of items.
	 */
	/*	BU_LIST_INIT(&allPixelList.l); */
	BU_LIST_INIT(&allScanlineList.l);

	/*
	 * write out the image primitives
	 ***************/
	is_region = 1;

	ncpu=bu_avail_cpus();

	if (ncpu > 1) {
		bu_log("Found %d cpu\'s!  Sweet.\n", ncpu);
	}

	/* the first critical section semaphore is for coordinating work, the
	 * second for writing out the final record and cleaning up.
	 */
	/* XXX must use RT_SEM_LAST if we plan on calling bu_parallel since the
	 * semaphore count is held in a global
	 */
	bu_semaphore_init(P2G_INIT_COUNT);

	bu_log("Writing database...\n");

	/* write out the prototypical pixel object */
	VSET(origin, 0.0, 0.0, 0.0);
	mk_sph(db_fp, prototype, origin, objectSize/2.0);

	/* XXX I do not like the idea of having to pass everything around in global
	 * space. but forking on our own is just as bad (need IPC)
	 */
	bu_parallel(computeScanline, ncpu, &allScanlineList);

	/* XXX We cannot write out one BIG combination of all the pixels due to
	 * library stack limitations and tree build implementation
	 */
	/*	mk_lcomb(db_fp, "image.c", &allPixelList, 0, NULL, NULL, NULL, 0); */
	/* write out the main image combination */

	mk_lcomb(db_fp, "image.c", &allScanlineList, 0, NULL, NULL, NULL, 0);

	bu_log("\n...done! (see %s)\n", databaseFileName);

	bu_close_mapped_file(image);

	wdb_close(db_fp);

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
