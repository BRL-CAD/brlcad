/*                      S P H F L A K E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file proc-db/sphflake.c
 *
 * Program to create a sphere-flake utilizing libwdb
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"


#define D2R(x) (x*DEG2RAD)
#define MATXPNT(d, m, v) {						\
	double _i = 1.0/((m)[12]*(v)[0] + (m)[13]*(v)[1] + (m)[14]*(v)[2] + (m)[15]*1); \
	(d)[0] = ((m)[0]*(v)[0] + (m)[1]*(v)[1] + (m)[2]*(v)[2] + (m)[3])*_i; \
	(d)[1] = ((m)[4]*(v)[0] + (m)[5]*(v)[1] + (m)[6]*(v)[2] + (m)[7])*_i; \
	(d)[2] = ((m)[8]*(v)[0] + (m)[9]*(v)[1] + (m)[10]*(v)[2] + (m)[11])*_i; \
    }


#define DEFAULT_FILENAME "sflake.g"
#define DEFAULT_MAXRADIUS 1000
#define DEFAULT_MAXDEPTH 3
#define DEFAULT_DELTARADIUS .3
#define DEFAULT_ORIGIN_X 0
#define DEFAULT_ORIGIN_Y 0
#define DEFAULT_ORIGIN_Z 0

#define DEFAULT_MAT "mirror"
#define DEFAULT_MATPARAM "sh=50 sp=.7 di=.3 re=.5"
#define DEFAULT_MATCOLOR "80 255 255"
#define DEFAULT_SCALE 10.3
#define ADDITIONAL_OBJECTS 5
#define SCENE_ID 0
#define LIGHT0_ID 1
#define LIGHT0_MAT "light"
#define LIGHT0_MATPARAM "inten=10000 shadows=1"
#define LIGHT0_MATCOLOR "255 255 255"
#define LIGHT1_ID 2
#define LIGHT1_MAT "light"
#define LIGHT1_MATPARAM "inten=5000 shadows=1 fract=.5"
#define LIGHT1_MATCOLOR "255 255 255"
#define PLANE_ID 3
#define PLANE_MAT "stack"
#define PLANE_MATPARAM "checker; plastic sh=20 sp=.1 di=.9"
#define PLANE_MATCOLOR "255 255 255"
#define ENVIRON_ID 4
#define ENVIRON_MAT "envmap"
#define ENVIRON_MATPARAM "cloud"

#define MAX_INPUT_LENGTH 48

struct depthMat {
    char name[MAX_INPUT_LENGTH];
    char params[MAX_INPUT_LENGTH];
    unsigned char color[3];
};
typedef struct depthMat depthMat_t;

struct params {
    char fileName[MAX_INPUT_LENGTH];
    int maxRadius;
    int maxDepth;
    double deltaRadius;
    point_t pos;
    depthMat_t *matArray;
};
typedef struct params params_t;

int count = 0; /* global sphere count */
struct rt_wdb *fp;
mat_t IDENT;

/* make the wmember structs, in order to produce individual
   combinations so we can have separate materials among differing
   depths */
struct wmember *wmemberArray;

/* the vector directions, in which the flakes will be drawn */
/* theta, phi */
int dir[9][2] = {  {0, -90},
		   {60, -90},
		   {120, -90},
		   {180, -90},
		   {240, -90},
		   {300, -90},
		   {120, -30},
		   {240, -30},
		   {360, -30} };

extern void initializeInfo(params_t *p, int inter, char *name, int depth);
extern void createSphereflake(params_t *p);
extern void createLights(params_t *p);
extern void createPlane(params_t *p);
extern void createScene(params_t *p);
extern void createEnvironMap(params_t *p);
extern void getYRotMat(mat_t *mat, fastf_t theta);
extern void getZRotMat(mat_t *mat, fastf_t phi);
extern void getTrans(mat_t *trans, int i, int j, fastf_t v);
extern void makeFlake(int depth, mat_t *trans, point_t center, fastf_t radius, double delta, int maxDepth);
extern void usage(char *n);

int main(int argc, char **argv)
{
    int i;
    int optc;

    params_t params;

    int inter = 0;
    char fileName[MAX_INPUT_LENGTH];
    int depth = DEFAULT_MAXDEPTH;

    while ((optc = bu_getopt(argc, argv, "iIDd:f:F:h?")) != -1) {
	switch (optc) {
	    case 'I' :
	    case 'i' : /* interactive mode */
		inter = 1;
		break;
	    case 'D':  /* Use ALL default parameters */
		memset(fileName, 0, MAX_INPUT_LENGTH);
		bu_strlcpy(fileName, DEFAULT_FILENAME, sizeof(fileName));
		depth = DEFAULT_MAXDEPTH;
		break;
	    case 'd':  /* Use a user-defined depth */
		depth = atoi(bu_optarg);
		if (depth > 5)
		    printf("\nWARNING: Depths greater than 5 produce extremely large numbers of objects.\n");
		break;
	    case 'F':
	    case 'f':  /* Use a user-defined filename */
		memset(fileName, 0, MAX_INPUT_LENGTH);
		bu_strlcpy(fileName, bu_optarg, sizeof(fileName));
		break;
	    default:
		usage(argv[0]);
		bu_exit(0, NULL);
	}
    }
    if (bu_optind <= 1) {
	usage(argv[0]);
	fprintf(stderr,"Using all default parameters.\n");
	fprintf(stderr,"       Program continues running:\n");
	memset(fileName, 0, MAX_INPUT_LENGTH);
	bu_strlcpy(fileName, DEFAULT_FILENAME, sizeof(fileName));
	inter = 0;
    }

    initializeInfo(&params, inter, fileName, depth);

    /* now open a file for outputting the database */
    fp = wdb_fopen(params.fileName);

    /* create the initial id */
    i = mk_id_units(fp, "SphereFlake", "mm");

    /* initialize the wmember structs...
       this is for creating the regions */
    wmemberArray = (struct wmember *)bu_malloc(sizeof(struct wmember)
					       *(params.maxDepth+1+ADDITIONAL_OBJECTS), "alloc wmemberArray");
    for (i = 0; i <= params.maxDepth+ADDITIONAL_OBJECTS; i++) {
	BU_LIST_INIT(&(wmemberArray[i].l));
    }

    createSphereflake(&params);
    createLights(&params);
    createPlane(&params);
    createEnvironMap(&params);
    createScene(&params);

    /* clean up */
    wdb_close(fp);
    bu_free(wmemberArray, "free wmemberArray");
    bu_free(params.matArray, "free matArray");

    return 0;
}


void usage(char *n)
{
    fprintf(stderr,
	"\nUsage: %s -D -d# -i -f fileName\n\
	  D -- use default parameters\n\
	  d -- set the recursive depth of the procedure\n\
	  i -- use interactive mode\n\
	  f -- specify output file\n\n", n);
}


void initializeInfo(params_t *p, int inter, char *name, int depth)
{
    char input[MAX_INPUT_LENGTH];
    int i;
    size_t len = 0;
    unsigned int c[3];

    if (name == NULL)
	bu_strlcpy(p->fileName, DEFAULT_FILENAME, sizeof(p->fileName));
    else
	bu_strlcpy(p->fileName, name, sizeof(p->fileName));

    p->maxRadius = DEFAULT_MAXRADIUS;
    p->maxDepth =  (depth > 0) ? (depth) : (DEFAULT_MAXDEPTH);
    p->deltaRadius = DEFAULT_DELTARADIUS;

    p->pos[X] = DEFAULT_ORIGIN_X;
    p->pos[Y] = DEFAULT_ORIGIN_Y;
    p->pos[Z] = DEFAULT_ORIGIN_Z;

    p->matArray = (depthMat_t *)bu_malloc(sizeof(depthMat_t) * (p->maxDepth+1), "alloc matArray");

    for (i = 0; i <= p->maxDepth; i++) {
	bu_strlcpy(p->matArray[i].name, DEFAULT_MAT, sizeof(p->matArray[i].name));
	bu_strlcpy(p->matArray[i].params, DEFAULT_MATPARAM, sizeof(p->matArray[i].params));

	sscanf(DEFAULT_MATCOLOR, "%u %u %u", &(c[0]), &(c[1]), &(c[2]));

	p->matArray[i].color[0] = c[0];
	p->matArray[i].color[1] = c[1];
	p->matArray[i].color[2] = c[2];
    }

    if (inter) {
	/* prompt the user for some data */
	/* no error checking here.... */
	printf("\nPlease enter a filename for sphereflake output: [%s] ", p->fileName);
	if (! bu_fgets(input, MAX_INPUT_LENGTH, stdin)) {
	    fprintf(stderr, "sphereflake: initializeInfo: fgets filename read error.\n");
	    fprintf(stderr, "Continuing with default value.\n");
	} else {
	    len = strlen(input);
	    if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
	    if (bu_strncmp(input, "", MAX_INPUT_LENGTH) != 0)
		sscanf(input, "%48s", p->fileName); /* MAX_INPUT_LENGTH */
	}
	fflush(stdin);

	printf("Initial position X Y Z: [%.2f %.2f %.2f] ", p->pos[X], p->pos[Y], p->pos[Z]);
	if (! bu_fgets(input, MAX_INPUT_LENGTH, stdin)) {
	    fprintf(stderr, "sphereflake: initializeInfo: fgets position read error.\n");
	    fprintf(stderr, "Continuing with default values.\n");
	} else {
	    len = strlen(input);
	    if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
	    if (bu_strncmp(input, "", MAX_INPUT_LENGTH) == 0) {
		double scan[3];
		sscanf(input, "%lg %lg %lg", &scan[X], &scan[Y], &scan[Z]);
		VMOVE(p->pos, scan);
	    }
	}
	fflush(stdin);

	printf("maxRadius: [%d] ", p->maxRadius);
	if (! bu_fgets(input, MAX_INPUT_LENGTH, stdin)) {
	    fprintf(stderr, "sphereflake: initializeInfo: fgets maxradius read error.\n");
	    fprintf(stderr, "Continuing with default value.\n");
	} else {
	    len = strlen(input);
	    if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
	    if (bu_strncmp(input, "", MAX_INPUT_LENGTH) != 0)
		sscanf(input, "%d", &(p->maxRadius));
	}
	fflush(stdin);

	printf("deltaRadius: [%.2f] ", p->deltaRadius);
	if (! bu_fgets(input, MAX_INPUT_LENGTH, stdin)) {
	    fprintf(stderr, "sphereflake: initializeInfo: fgets deltaradius read error.\n");
	    fprintf(stderr, "Continuing with default value.\n");
	} else {
	    len = strlen(input);
	    if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
	    if (bu_strncmp(input, "", MAX_INPUT_LENGTH) != 0)
		sscanf(input, "%lg", &(p->deltaRadius));
	}
	fflush(stdin);

	printf("maxDepth: [%d] ", p->maxDepth);
	if (! bu_fgets(input, MAX_INPUT_LENGTH, stdin)) {
	    fprintf(stderr, "sphereflake: initializeInfo: fgets maxdepth read error.\n");
	    fprintf(stderr, "Continuing with default value.\n");
	} else {
	    len = strlen(input);
	    if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
	    if (bu_strncmp(input, "", MAX_INPUT_LENGTH) != 0)
		sscanf(input, "%d", &(p->maxDepth));
	}
	fflush(stdin);


	for (i = 0; i <= p->maxDepth; i++) {
	    printf("Material for depth %d: [%s] ", i, p->matArray[i].name);
	    if (! bu_fgets(input, MAX_INPUT_LENGTH, stdin)) {
		fprintf(stderr, "sphereflake: initializeInfo: fgets material read error.\n");
		fprintf(stderr, "Continuing with default value.\n");
	    } else {
		len = strlen(input);
		if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
		if (bu_strncmp(input, "", MAX_INPUT_LENGTH) != 0)
		    sscanf(input, "%48s", p->matArray[i].name); /* MAX_INPUT_LENGTH */
	    }
	    fflush(stdin);

	    printf("Mat. params for depth %d: [%s] ", i, p->matArray[i].params);
	    if (! bu_fgets(input, MAX_INPUT_LENGTH, stdin)) {
		fprintf(stderr, "sphereflake: initializeInfo: fgets params read error.\n");
		fprintf(stderr, "Continuing with default value.\n");
	    } else {
		len = strlen(input);
		if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
		if (bu_strncmp(input, "", MAX_INPUT_LENGTH) != 0)
		    sscanf(input, "%48s", p->matArray[i].params); /* MAX_INPUT_LENGTH */
	    }
	    fflush(stdin);

	    printf("Mat. color for depth %d: [%d %d %d] ", i, p->matArray[i].color[0], p->matArray[i].color[1], p->matArray[i].color[2]);
	    if (! bu_fgets(input, MAX_INPUT_LENGTH, stdin)) {
		fprintf(stderr, "sphereflake: initializeInfo: fgets color read error.\n");
		fprintf(stderr, "Continuing with default values.\n");
	    } else {
		len = strlen(input);
		if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
		if (bu_strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
		    sscanf(input, "%d %d %d", (int *)&(c[0]), (int *)&(c[1]), (int *)&(c[2]));
		    p->matArray[i].color[0] = c[0];
		    p->matArray[i].color[1] = c[1];
		    p->matArray[i].color[2] = c[2];
		}
	    }
	    fflush(stdin);
	}
    }
    MAT_IDN(IDENT);
}


void createSphereflake(params_t *p)
{
    mat_t trans;
    char name[MAX_INPUT_LENGTH];
    int i = 0;

    /* now begin the creation of the sphereflake... */
    MAT_IDN(trans); /* get the identity matrix */
    makeFlake(0, &trans, p->pos, (fastf_t)p->maxRadius * DEFAULT_SCALE, p->deltaRadius, p->maxDepth);
    /*
      Now create the depth combinations/regions
      This is done to facilitate application of different
      materials to the different depths */

    for (i = 0; i <= p->maxDepth; i++) {
	memset(name, 0, MAX_INPUT_LENGTH);
	sprintf(name, "depth%d.r", i);
	mk_lcomb(fp, name, &(wmemberArray[i+ADDITIONAL_OBJECTS]), 1, p->matArray[i].name, p->matArray[i].params, p->matArray[i].color, 0);
    }
    printf("\nSphereFlake created");

}


void createLights(params_t *p)
{
    char name[MAX_INPUT_LENGTH];
    point_t lPos;
    int r, g, b;
    unsigned char c[3];


    /* first create the light spheres */
    VSET(lPos, p->pos[X]+(5 * p->maxRadius), p->pos[Y]+(-5 * p->maxRadius), p->pos[Z]+(150 * p->maxRadius));
    memset(name, 0, MAX_INPUT_LENGTH);
    sprintf(name, "light0");
    mk_sph(fp, name, lPos, p->maxRadius*5);

    /* now make the light region... */
    mk_addmember(name, &(wmemberArray[LIGHT0_ID].l), NULL, WMOP_UNION);
    bu_strlcat(name, ".r", sizeof(name));
    sscanf(LIGHT0_MATCOLOR, "%d %d %d", &r, &g, &b);
    c[0] = (char)r;
    c[1] = (char)g;
    c[2] = (char)b;
    mk_lcomb(fp, name, &(wmemberArray[LIGHT0_ID]), 1, LIGHT0_MAT, LIGHT0_MATPARAM,
	     (const unsigned char *) c, 0);

    VSET(lPos, p->pos[X]+(13 * p->maxRadius), p->pos[Y]+(-13 * p->maxRadius), p->pos[Z]+(152 * p->maxRadius));
    sprintf(name, "light1");
    mk_sph(fp, name, lPos, p->maxRadius*5);

    /* now make the light region... */
    mk_addmember(name, &(wmemberArray[LIGHT1_ID].l), NULL, WMOP_UNION);
    bu_strlcat(name, ".r", sizeof(name));
    sscanf(LIGHT1_MATCOLOR, "%d %d %d", &r, &g, &b);
    c[0] = (char)r;
    c[1] = (char)g;
    c[2] = (char)b;
    mk_lcomb(fp, name, &(wmemberArray[LIGHT1_ID]), 1, LIGHT1_MAT, LIGHT1_MATPARAM,
	     (const unsigned char *) c, 0);

    printf("\nLights created");
}


void createPlane(params_t *p)
{
    char name[MAX_INPUT_LENGTH];
    point_t lPos;

    VSET(lPos, 0, 0, 1); /* set the normal */
    memset(name, 0, MAX_INPUT_LENGTH);
    sprintf(name, "plane");
    mk_half(fp, name, lPos, -p->maxRadius * 2 * DEFAULT_SCALE);

    /* now make the plane region... */
    mk_addmember(name, &(wmemberArray[PLANE_ID].l), NULL, WMOP_UNION);
    bu_strlcat(name, ".r", sizeof(name));
    mk_lcomb(fp, name, &(wmemberArray[PLANE_ID]), 1, PLANE_MAT, PLANE_MATPARAM, (unsigned char *)PLANE_MATCOLOR, 0);

    printf("\nPlane created");
}


void createEnvironMap(params_t *UNUSED(p))
{
    char name[MAX_INPUT_LENGTH];

    memset(name, 0, MAX_INPUT_LENGTH);
    sprintf(name, "light0");
    mk_addmember(name, &(wmemberArray[ENVIRON_ID].l), NULL, WMOP_UNION);
    memset(name, 0, MAX_INPUT_LENGTH);
    sprintf(name, "environ.r");
    mk_lcomb(fp, name, &(wmemberArray[ENVIRON_ID]), 1, ENVIRON_MAT, ENVIRON_MATPARAM, (unsigned char *)"0 0 0", 0);

    printf("\nEnvironment map created");

}


void createScene(params_t *p)
{
    int i;
    char name[MAX_INPUT_LENGTH];

    for (i = 0; i < p->maxDepth+1; i++) {
	memset(name, 0, MAX_INPUT_LENGTH);
	sprintf(name, "depth%d.r", i);
	mk_addmember(name, &(wmemberArray[SCENE_ID].l), NULL, WMOP_UNION);
    }
    mk_addmember("light0.r", &(wmemberArray[SCENE_ID].l), NULL, WMOP_UNION);
    mk_addmember("light1.r", &(wmemberArray[SCENE_ID].l), NULL, WMOP_UNION);
    mk_addmember("plane.r", &(wmemberArray[SCENE_ID].l), NULL, WMOP_UNION);
    mk_addmember("environ.r", &(wmemberArray[SCENE_ID].l), NULL, WMOP_UNION);
    memset(name, 0, MAX_INPUT_LENGTH);
    sprintf(name, "scene.r");
    mk_lfcomb(fp, name, &(wmemberArray[SCENE_ID]), 0);

    printf("\nScene created (FILE: %s)\n", p->fileName);
}


void printMatrix(char *n, fastf_t *m)
{
    int i = 0;
    printf("\n-----%s------\n", n);
    for (i = 0; i < 16; i++) {
	if (i%4 == 0 && i != 0) printf("\n");
	printf("%6.2f ", m[i]);
    }
    printf("\n-----------\n");
}


void getTrans(mat_t (*t), int theta, int phi, fastf_t radius)
{
    mat_t z;
    mat_t y;
    mat_t toRelative;
    mat_t newPos;
    MAT_IDN(z);
    MAT_IDN(y);
    MAT_IDN(newPos);
    MAT_IDN(toRelative);

    MAT_DELTAS(toRelative, 0, 0, radius);

    getZRotMat(&z, theta);
    getYRotMat(&y, phi);

    bn_mat_mul2(toRelative, newPos); /* translate to new position */
    bn_mat_mul2(y, newPos);          /* rotate z */
    bn_mat_mul2(z, newPos);          /* rotate y */
    MAT_DELTAS(*t, 0, 0, 0);
    bn_mat_mul2(*t, newPos);

    memcpy(*t, newPos, sizeof(newPos));
}


void getYRotMat(mat_t (*t), fastf_t theta)
{
    fastf_t sin_ = sin(D2R(theta));
    fastf_t cos_ = cos(D2R(theta));
    mat_t r;
    MAT_ZERO(r);
    r[0] = cos_;
    r[2] = sin_;
    r[5] = 1;
    r[8] = -sin_;
    r[10] = cos_;
    r[15] = 1;
    memcpy(*t, r, sizeof(*t));
}


void getZRotMat(mat_t (*t), fastf_t phi)
{
    fastf_t sin_ = sin(D2R(phi));
    fastf_t cos_ = cos(D2R(phi));
    mat_t r;
    MAT_ZERO(r);
    r[0] = cos_;
    r[1] = -sin_;
    r[4] = sin_;
    r[5] = cos_;
    r[10] = 1;
    r[15] = 1;
    memcpy(*t, r, sizeof(*t));
}

void makeFlake(int depth, mat_t (*trans), fastf_t *center, fastf_t radius, double delta, int maxDepth)
{
    char name[MAX_INPUT_LENGTH];
    int i = 0;
    point_t pcent;
    fastf_t newRadius;
    mat_t temp;
    point_t origin;
    point_t pcentTemp;

    VSET(origin, 0, 0, 0);

    /* just return if depth == maxDepth */
    if (depth > maxDepth) return;


    /* create self, then recurse for each different angle */
    count++;
    sprintf(name, "sph%d", count);
    mk_sph(fp, name, center, radius);
    newRadius = radius*delta;

    /* add the sphere to the correct combination */
    mk_addmember(name, &(wmemberArray[depth+ADDITIONAL_OBJECTS].l), NULL, WMOP_UNION);

    for (i = 0; i < 9; i++) {
	memcpy(temp, trans, sizeof(temp));
	getTrans(&temp, dir[i][0], dir[i][1], radius+newRadius);
	MATXPNT(pcentTemp, temp, origin);
	VADD2(pcent, pcentTemp, center);
	makeFlake(depth+1, &temp, pcent, newRadius, delta, maxDepth);
    }
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
