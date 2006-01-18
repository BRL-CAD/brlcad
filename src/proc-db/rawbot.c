/*                        R A W B O T . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2006 United States Government as represented by
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
/** @file rawbot.c
 *
 *    Program to generate a BoT from raw triangle data in a file
 *
 *    File input is assumed to be interleaved XYZ vertex data
 *    where three vertices comprise a single unoriented triangle.
 *
 *    e.g. The following describes three triangles:
 *
 *      X1 Y1 Z1 X2 Y2 Z2 X3 Y3 Z3 X4 Y4 Z4 X5 Y5 Z5
 *      X6 Y6 Z6 X7 Y7 Z7 X8 Y8 Z8 X9 Y9 Z9
 *
 *    Shared edges or points are not accounted for, implying data
 *    duplication for triangulated surface data where shared
 *    vertices/edges are common.
 *
 *  Author -
 *	Christopher Sean Morrison
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"

struct rt_wdb *outfp;

void usage(const char *progname)
{
	fprintf(stderr, "Usage: %s raw_vertex_file\n", progname);
	exit(-1);
}

int main( int argc, char *argv[] )
{
	char inputString[512];
	float inputX, inputY, inputZ;
	fastf_t *vertices;
	int *faces;
	fastf_t *thickness;
	FILE *inputFile;
	short int triangleAvailable;
	long int triangleCount;
	long int maxTriangleCapacity;
	long int j;
	char *outputObjectName;

	if (argc != 2) {
		usage(argv[0]);
	}

	outfp = wdb_fopen( "rawbot.g" );
	if (outfp == NULL) {
		fprintf(stderr, "Unable to open the output file rawbot.g\n");
		return 1;
	}
	/* units would be nice... */
	mk_id( outfp, "RAW BOT" );

	inputFile = fopen(argv[1], "r");
	if (inputFile == NULL) {
		perror("unable to open file");
		fprintf(stderr, "The input file [%s] was not readable\n", argv[1]);
		return 1;
	}

	vertices = calloc(128 * 3, sizeof(fastf_t));
	if (vertices == NULL) {
		perror("unable to calloc memory");
		fprintf(stderr, "Unable to allocate space for the initial vertices\n");
		return 2;
	}
	maxTriangleCapacity = 128;

	triangleCount=0;
	triangleAvailable = 1;
	while ( triangleAvailable == 1 ) {
		/* read a set of input values -- input data should be a 3-tuple
		 * of floating points.
		 */
	  if (fscanf(inputFile, "%s", inputString) != 1) {
			triangleAvailable = 0;
			continue;
		}
		inputX = atof(inputString);
		if (fscanf(inputFile, "%s", inputString) != 1) {
			triangleAvailable = 0;
			continue;
		}
		inputY = atof(inputString);
		if (fscanf(inputFile, "%s", inputString) != 1) {
			triangleAvailable = 0;
			continue;
		}
		inputZ = atof(inputString);

		if (triangleCount >= maxTriangleCapacity) {
			vertices = realloc(vertices, ((maxTriangleCapacity + 128) * 3) * sizeof(fastf_t));
			if (vertices == NULL) {
				perror("unable to calloc memory");
				fprintf(stderr, "Unable to reallocate space for more triangles\n");
				return 2;
			}
			maxTriangleCapacity += 128;
		}

		/*		VSET( &vertices[triangleCount*3], inputX, inputY, inputZ ); */
		vertices[(triangleCount*3)] = inputX;
		vertices[(triangleCount*3)+1] = inputY;
		vertices[(triangleCount*3)+2] = inputZ;
		/*		printf("%f %f %f\n", vertices[(triangleCount*3)], vertices[(triangleCount*3)+1], vertices[(triangleCount*3)+2]); */
		triangleCount++;
	}

	/* done with the input file */
	fclose(inputFile);

	/* make sure we found some vertices */
	if (triangleCount <= 0) {
	  fprintf(stderr, "There were no triangles found in the input file\n");
		free(vertices);
	  return 0;
	} else {
		printf("Found %ld triangles\n", triangleCount);
	}

	/* allocate memory for faces and thickness arrays */
	/* XXX unfortunately we are limited to sizeof(int) since mk_bot takes
	 * an int array */
	faces = (int *)calloc(triangleCount * 3, sizeof(int));
	if (faces == NULL) {
		perror("unable to allocate memory");
		fprintf(stderr, "Unable to allocate memory for the faces array\n");
		free(vertices);
		return 3;
	}
	thickness = (fastf_t *)calloc(triangleCount * 3, sizeof(int));
	if (thickness == NULL) {
		perror("unable to allocate memory");
		fprintf(stderr, "Unable to allocate memory for the thickness array\n");
		free(vertices);
		free(faces);
		return 3;
	}
	for (j=0; j<triangleCount; j++) {
		faces[(j*3)] = (j*3);
		faces[(j*3)+1] = (j*3) + 1;
		faces[(j*3)+2] = (j*3) + 2;
		printf("%ld %ld %ld == (%f %f %f)\n", (j*3), (j*3)+1, (j*3)+2, vertices[(j*3)], vertices[(j*3)+1], vertices[(j*3)+2]);
		thickness[(j*3)] = thickness[(j*3)+1] = thickness[(j*3)+2] = 1.0;
	}

	/*
	for (j=0; j < triangleCount * 3; j++) {
		printf("%f\n", vertices[j]);
	}
	*/

	outputObjectName = (char *)calloc(512, sizeof(char));
	if (outputObjectName == NULL) {
	  perror("unable to allocate memory");
	  fprintf(stderr, "Unable to allocate memory for the output object name\n");
		free(vertices);
		free(faces);
		free(thickness);
	  return 4;
	}

	snprintf(outputObjectName, 512, "%s.surface.s", argv[1]);
	mk_bot( outfp, outputObjectName, RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, triangleCount*3, triangleCount, vertices,  faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );

	snprintf(outputObjectName, 512, "%s.solid.s", argv[1]);
	mk_bot( outfp, outputObjectName, RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, triangleCount*3, triangleCount, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );

	/*	snprintf(outputObjectName, 512, "%s.plate.s", argv[1]);*/
	/*	mk_bot( outfp, "bot_u_plate", RT_BOT_PLATE, RT_BOT_UNORIENTED, 0, triangleCount, triangleCount, vertices, faces, thickness, NULL ); */

	free(vertices);
	free(faces);
	free(thickness);

	wdb_close(outfp);

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
