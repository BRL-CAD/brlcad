/*
 *                      D O N U T S . C
 *
 * This program generages a "donut flake".  Based in-part off
 * sphflake originally written by Jason Owens.
 *
 * Author -
 * Christopher Sean Morrison
 *
 * Source -
 * The U. S. Army Research Laboratory
 * Aberdeen Proving Ground, Maryland 21005-5068 USA
 *
 * Distribution Notice -
 * Re-distribution of this software is restricted, as described in
 * your "Statement of Terms and Conditions for the Release of
 * The BRL-CAD Package" agreement.
 *
 * Copyright Notice -
 * This software is Copyright (C) 1998-2021 by the United States Army
 * in all countries except the USA.  All rights reserved.
 *
 ***********************************************************************/

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"


/* this macro does the standard conversion of an angle to a radian
 * value.  the value of pi defined in M_PI is pulled from math.h
 */
#define RADIAN(x) (((x)*_PI)/180.0)

#define MATXPNT(d, m, v) { \
    register double _i = 1.0/((m)[12]*(v)[0] + (m)[13]*(v)[1] + (m)[14]*(v)[2] + (m)[15]*1); \
    (d)[0] = ((m)[0]*(v)[0] + (m)[1]*(v)[1] + (m)[2]*(v)[2] + (m)[3])*_i; \
    (d)[1] = ((m)[4]*(v)[0] + (m)[5]*(v)[1] + (m)[6]*(v)[2] + (m)[7])*_i; \
    (d)[2] = ((m)[8]*(v)[0] + (m)[9]*(v)[1] + (m)[10]*(v)[2] + (m)[11])*_i; \
  }


#define DEFAULT_DEBUG 0
#define DEFAULT_VERBOSE 0
#define DEFAULT_INTERACTIVE 0

#define DEFAULT_DEBUG_OUTPUT stdout
#define DEFAULT_VERBOSE_OUTPUT stderr

/* this is the name of the default output file name
 */
#define DEFAULT_OUTPUTFILENAME "donuts.g"

/* this is the default measuring units for the database file
 */
#define DEFAULT_UNITS "mm"

/* this is the identifying title or name for the database file
 */
#define DEFAULT_TITLE "Untitled"

/* this is an upper bounds restriction placed on the length of any and
 * all names in the database.
 */
#define DEFAULT_MAXNAMELENGTH MAXPATHLEN

/* this is the upper bounds on input from the user
 */
#define MAX_INPUT_LENGTH MAXPATHLEN

/* for all of the default names that are specified, a `.s' or `.r' suffix
 * will automatically get appended where needed for the solid and
 * region objects that are generated, respectively, and do not need
 * to be added.
 *
 * here are the default program values for the actual donut flake object
 */

#define DEFAULT_ORIGIN_X 0
#define DEFAULT_ORIGIN_Y 0
#define DEFAULT_ORIGIN_Z 0

#define DEFAULT_MAXDEPTH 3
#define DEFAULT_MAXINNERRADIUS 1000
#define DEFAULT_MAXTORUSRADIUS 25
#define DEFAULT_RADIUSDELTA .333333333
#define DEFAULT_POSITION {0.0, 0.0, 0.0}
#define DEFAULT_NORMAL {0.0, 0.0, 1.0}

#define DEFAULT_MATERIAL "mirror"
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


struct depthMaterial {
  char name[DEFAULT_MAXNAMELENGTH];
  char parameters[DEFAULT_MAXNAMELENGTH];
  unsigned char color[3];
};
typedef struct depthMaterial depthMaterial_t;

struct params {
  char fileName[DEFAULT_MAXNAMELENGTH];
  int maxDepth;
  int maxInnerRadius;
  int maxTorusRadius;
  double innerRadiusDelta;
  double torusRadiusDelta;
  point_t position;
  vect_t normal;
  depthMaterial_t *materialArray;
};
typedef struct params params_t;

int count = 0; /* global sphere count */
struct rt_wdb *fp;


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


/****** Function Prototypes ******/
#ifdef __cplusplus
extern "C" {
#endif
  extern void initializeInfo(params_t *p, char *name, int depth);
  extern void createDonuts(params_t *p);
  extern void createLights(params_t *p);
  extern void createPlane(params_t *p);
  extern void createScene(params_t *p);
  extern void createEnvironMap(params_t *p);
  extern void getYRotMat(mat_t *mat, fastf_t theta);
  extern void getZRotMat(mat_t *mat, fastf_t phi);
  extern void getTrans(mat_t *trans, int i, int j, fastf_t v);
  extern void makeFlake(int depth, mat_t *trans, point_t center, fastf_t radius, double delta, int maxDepth);
  extern void usage(char *n);
  extern void argumentHelp(FILE *fp, const char *progname, char *message);
  extern void argumentExamples(FILE *fp, char *progname);
  extern void defaultSettings(FILE *fp);
  extern int parseArguments(int argc, char *argv[]);
  extern void printMatrix(FILE *fp, char *n, mat_t m);
  extern char *getName(char *base, int id, char *suffix);
  extern char *getPrePostName(char *prefix, char *base, char *suffix);
#ifdef __cplusplus
}
#endif


/* command-line options are described in the parseArguments function
 */
const char *options="IiDdVvO:o:N:n:U:u:";
extern char *optarg;
extern int optind, opterr, getopt();

/* these variables control the "behavior" of this program's output
 *   if debug is set, debug information (extra "useful" output is given) is
 *     displayed on stdout.
 *   if verbose is set, then all prompting and output that may be shown
 *     (such as results and errors) will be sent to stderr.  this should
 *     probably be switched with debug (maybe) but convention was taken
 *     from another db program.
 *   if interactive is set, then the user will be prompted for values from
 *     stdin.
 *
 *   the default output file pointers for debug and verbose can be set
 *     in the header file
 */
int debug=DEFAULT_DEBUG;
int verbose=DEFAULT_VERBOSE;
int interactive=DEFAULT_INTERACTIVE;

/* start of variable definitions for the donut flake
 *
 * see the donuts.h header file for a description of each of the default values
 * that may be set.  all default values should be set in the donuts.h header
 * file, not here.
 */
char outputFilename[DEFAULT_MAXNAMELENGTH]=DEFAULT_OUTPUTFILENAME;
char title[DEFAULT_MAXNAMELENGTH]=DEFAULT_TITLE;
char units[DEFAULT_MAXNAMELENGTH]=DEFAULT_UNITS;

/* primary parameter variable for the donut flake
 */
params_t parameters;


/*
 * getName() returns a name back based on a base string, a numerical id and a
 * parameter string for merging those two parameters.  Basically it adds the
 * id number to the end of the base so that we can set unique ids for our
 * objects and groups.  (i.e. base="rcc", id="5"==> returns "rcc005.s" or
 * something like that)
 ***********************************/
char *getName(base, id, paramstring)
     char *base;
     int id;
     char *paramstring;
{
  static char name[DEFAULT_MAXNAMELENGTH];

  memset(name, 0, DEFAULT_MAXNAMELENGTH);

  if (id>=0) sprintf(name, paramstring, base, id);
  else sprintf(name, paramstring, base);

  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "getName(): base[%s], id[%d]\n", base, id);
  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Using name[%s]\n", name);

  return name;
}


/*
 * getPrePostName() returns a name for a region.  the name is formed by
 * simply appending three given strings passed.  the three strings comprise
 * the base, prefix, and suffix of the name.  any three are optional by
 * sending a NULL parameter
 *****************************************/
char *getPrePostName(prefix, base, suffix)
     char *prefix;
     char *base;
     char *suffix;
{
  static char newname[DEFAULT_MAXNAMELENGTH];

  memset(newname, 0, DEFAULT_MAXNAMELENGTH);

  if (prefix) sprintf(newname, "%s", prefix);
  if (base) sprintf(newname, "%s%s", newname, base);
  if (suffix) sprintf(newname, "%s%s", newname, suffix);

  return newname;
}


void initializeInfo(p, name, depth)
     params_t *p;
     char *name;
     int depth;
{
  char input[MAX_INPUT_LENGTH] = {0};
  int i = 0;
  int len = 0;
  unsigned int c[3];

  if (name == NULL) {
    bu_strlcpy(p->fileName, DEFAULT_OUTPUTFILENAME, DEFAULT_MAXNAMELENGTH);
  } else {
    bu_strlcpy(p->fileName, name, DEFAULT_MAXNAMELENGTH);
  }
  p->maxInnerRadius = DEFAULT_MAXINNERRADIUS;
  p->maxTorusRadius = DEFAULT_MAXTORUSRADIUS;
  p->maxDepth =  (depth > 0) ? (depth) : (DEFAULT_MAXDEPTH);
  p->innerRadiusDelta = p->torusRadiusDelta = DEFAULT_RADIUSDELTA;
  p->position[X] = DEFAULT_ORIGIN_X;
  p->position[Y] = DEFAULT_ORIGIN_Y;
  p->position[Z] = DEFAULT_ORIGIN_Z;

  p->materialArray = (depthMaterial_t *)bu_malloc(sizeof(depthMaterial_t) * (p->maxDepth+1), "alloc materialArray");

  for (i = 0; i <= p->maxDepth; i++) {
    strncpy(p->materialArray[i].name, DEFAULT_MATERIAL, MAX_INPUT_LENGTH);
    strncpy(p->materialArray[i].parameters, DEFAULT_MATPARAM, MAX_INPUT_LENGTH);
    sscanf(DEFAULT_MATCOLOR, "%u %u %u", &(c[0]), &(c[1]), &(c[2]));
    p->materialArray[i].color[0] = c[0];
    p->materialArray[i].color[1] = c[1];
    p->materialArray[i].color[2] = c[2];
  }

  if (interactive) {
    /* prompt the user for some data */
    /* no error checking here.... */
    printf("\nPlease enter a filename for donuts output: [%s] ", p->fileName);
    if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
      fprintf(stderr, "donuts: initializeInfo: fgets filename read error.\n");
      fprintf(stderr, "Continuing with default value.\n");
    } else {
      len = strlen(input);
      if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
      if (strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
        sscanf(input, "%s", p->fileName);
      }
    }
    fflush(stdin);

    printf("Initial position X Y Z: [%.2f %.2f %.2f] ", p->position[X], p->position[Y], p->position[Z]);
    if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
      fprintf(stderr, "donuts: initializeInfo: fgets position read error.\n");
      fprintf(stderr, "Continuing with default values.\n");
    } else {
      len = strlen(input);
      if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
      if (strncmp(input, "", MAX_INPUT_LENGTH) == 0) {
        sscanf(input, "%lg %lg %lg", &(p->position[X]), &(p->position[Y]), &(p->position[Z]));
      }
    }
    fflush(stdin);

    printf("maxInnerRadius: [%d] ", p->maxInnerRadius);
    if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
      fprintf(stderr, "donuts: initializeInfo: fgets maxinnerradius read error.\n");
      fprintf(stderr, "Continuing with default value.\n");
    } else {
      len = strlen(input);
      if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
      if (strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
        sscanf(input, "%d", &(p->maxInnerRadius));
      }
    }
    fflush(stdin);

    printf("maxTorusRadius: [%d] ", p->maxTorusRadius);
    if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
      fprintf(stderr, "donuts: initializeInfo: fgets maxouterradius read error.\n");
      fprintf(stderr, "Continuing with default value.\n");
    } else {
      len = strlen(input);
      if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
      if (strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
        sscanf(input, "%d", &(p->maxTorusRadius));
      }
    }
    fflush(stdin);

    printf("innerRadiusDelta: [%.2f] ", p->innerRadiusDelta);
    if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
      fprintf(stderr, "donuts: initializeInfo: fgets innerradiusdelta read error.\n");
      fprintf(stderr, "Continuing with default value.\n");
    } else {
      len = strlen(input);
      if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
      if (strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
        sscanf(input, "%lg", &(p->innerRadiusDelta));
      }
    }
    fflush(stdin);

    printf("torusRadiusDelta: [%.2f] ", p->torusRadiusDelta);
    if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
      fprintf(stderr, "donuts: initializeInfo: fgets torusradiusdelta read error.\n");
      fprintf(stderr, "Continuing with default value.\n");
    } else {
      len = strlen(input);
      if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
      if (strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
        sscanf(input, "%lg", &(p->torusRadiusDelta));
      }
    }
    fflush(stdin);

    printf("maxDepth: [%d] ", p->maxDepth);
    if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
      fprintf(stderr, "donuts: initializeInfo: fgets maxdepth read error.\n");
      fprintf(stderr, "Continuing with default value.\n");
    } else {
      len = strlen(input);
      if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
      if (strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
        sscanf(input, "%d", &(p->maxDepth));
      }
    }
    fflush(stdin);


    for (i = 0; i <= p->maxDepth; i++) {
      printf("Material for depth %d: [%s] ", i, p->materialArray[i].name);
      if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
        fprintf(stderr, "donuts: initializeInfo: fgets material read error.\n");
        fprintf(stderr, "Continuing with default value.\n");
      } else {
        len = strlen(input);
        if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
        if (strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
          sscanf(input, "%s", p->materialArray[i].name);
        }
      }
      fflush(stdin);

      printf("Mat. params for depth %d: [%s] ", i, p->materialArray[i].parameters);
      if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
        fprintf(stderr, "donuts: initializeInfo: fgets params read error.\n");
        fprintf(stderr, "Continuing with default value.\n");
      } else {
        len = strlen(input);
        if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
        if (strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
          sscanf(input, "%s", p->materialArray[i].parameters);
        }
      }
      fflush(stdin);

      printf("Mat. color for depth %d: [%d %d %d] ", i, p->materialArray[i].color[0], p->materialArray[i].color[1], p->materialArray[i].color[2]);
      if (! fgets(input, MAX_INPUT_LENGTH, stdin)) {
        fprintf(stderr, "donuts: initializeInfo: fgets color read error.\n");
        fprintf(stderr, "Continuing with default values.\n");
      } else {
        len = strlen(input);
        if ((len > 0) && (input[len-1] == '\n')) input[len-1] = 0;
        if (strncmp(input, "", MAX_INPUT_LENGTH) != 0) {
          sscanf(input, "%d %d %d", &(c[0]), &(c[1]), &(c[2]));
          p->materialArray[i].color[0] = c[0];
          p->materialArray[i].color[1] = c[1];
          p->materialArray[i].color[2] = c[2];
        }
      }
      fflush(stdin);
    }
  }
}


void createDonuts(p)
     params_t *p;
{
  mat_t trans;
  char name[MAX_INPUT_LENGTH];
  int i = 0;

  /* now begin the creation of the donuts... */
  MAT_IDN(trans); /* get the identity matrix */
  makeFlake(0, &trans, p->position, (fastf_t)p->maxTorusRadius * DEFAULT_SCALE, p->torusRadiusDelta, p->maxDepth);
  /*
    Now create the depth combinations/regions This is done to
    facilitate application of different materials to the different
    depths */

  for (i = 0; i <= p->maxDepth; i++) {
    memset(name, 0, MAX_INPUT_LENGTH);
    sprintf(name, "depth%d.r", i);
    mk_lcomb(fp, name, &(wmemberArray[i+ADDITIONAL_OBJECTS]), 1, p->materialArray[i].name, p->materialArray[i].parameters, p->materialArray[i].color, 0);
  }
  printf("\nDonuts created");

}


void createLights(p)
     params_t *p;
{
  char name[MAX_INPUT_LENGTH];
  point_t lPos;
  int r, g, b;
  unsigned char c[3];


  /* first create the light spheres */
  VSET(lPos, p->position[X]+(5 * p->maxTorusRadius), p->position[Y]+(-5 * p->maxTorusRadius), p->position[Z]+(150 * p->maxTorusRadius));
  memset(name, 0, MAX_INPUT_LENGTH);
  sprintf(name, "light0");
  mk_sph(fp, name, lPos, p->maxTorusRadius*5);

  /* now make the light region... */
  mk_addmember(name, &(wmemberArray[LIGHT0_ID].l), NULL, WMOP_UNION);
  strcat(name, ".r");
  sscanf(LIGHT0_MATCOLOR, "%d %d %d", &r, &g, &b);
  c[0] = (char)r;
  c[1] = (char)g;
  c[2] = (char)b;
  mk_lcomb(fp, name, &(wmemberArray[LIGHT0_ID]), 1, LIGHT0_MAT, LIGHT0_MATPARAM,
           (const unsigned char *) c, 0);

  VSET(lPos, p->position[X]+(13 * p->maxTorusRadius), p->position[Y]+(-13 * p->maxTorusRadius), p->position[Z]+(152 * p->maxTorusRadius));
  sprintf(name, "light1");
  mk_sph(fp, name, lPos, p->maxTorusRadius*5);

  /* now make the light region... */
  mk_addmember(name, &(wmemberArray[LIGHT1_ID].l), NULL, WMOP_UNION);
  strcat(name, ".r");
  sscanf(LIGHT1_MATCOLOR, "%d %d %d", &r, &g, &b);
  c[0] = (char)r;
  c[1] = (char)g;
  c[2] = (char)b;
  mk_lcomb(fp, name, &(wmemberArray[LIGHT1_ID]), 1, LIGHT1_MAT, LIGHT1_MATPARAM,
           (const unsigned char *) c, 0);

  printf("\nLights created");
}


void createPlane(p)
     params_t *p;
{
  char name[MAX_INPUT_LENGTH];
  point_t lPos;
  const unsigned char *matcolor = (unsigned char *)PLANE_MATCOLOR;

  VSET(lPos, 0, 0, 1); /* set the normal */
  memset(name, 0, MAX_INPUT_LENGTH);
  sprintf(name, "plane");
  mk_half(fp, name, lPos, -p->maxTorusRadius * 2 * DEFAULT_SCALE);

  /* now make the plane region... */
  mk_addmember(name, &(wmemberArray[PLANE_ID].l), NULL, WMOP_UNION);
  strcat(name, ".r");
  mk_lcomb(fp, name, &(wmemberArray[PLANE_ID]), 1, PLANE_MAT, PLANE_MATPARAM, matcolor, 0);

  printf("\nPlane created");
}


void createEnvironMap(p)
     params_t *p;
{
  char name[MAX_INPUT_LENGTH];
  const unsigned char *color = (unsigned char *)"0 0 0";

  if (!p) return;

  memset(name, 0, MAX_INPUT_LENGTH);
  sprintf(name, "light0");
  mk_addmember(name, &(wmemberArray[ENVIRON_ID].l), NULL, WMOP_UNION);
  memset(name, 0, MAX_INPUT_LENGTH);
  sprintf(name, "environ.r");
  mk_lcomb(fp, name, &(wmemberArray[ENVIRON_ID]), 1, ENVIRON_MAT, ENVIRON_MATPARAM, color, 0);

  printf("\nEnvironment map created");

}


void createScene(p)
     params_t *p;
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


/*
 * printMatrix() does just that, it prints out a matrix and a label passed to
 * some fp (usually DEFAULT_DEBUG_OUTPUT or DEFAULT_VERBOSE_OUTPUT).
 *************************/
void printMatrix(outfp, n, m)
  FILE *outfp;
  char *n;
  mat_t m;
{
  int i = 0;
  fprintf(outfp, "\n-----%s------\n", n);
  for (i = 0; i < 16; i++) {
    if (i%4 == 0 && i != 0) fprintf(outfp, "\n");
    fprintf(outfp, "%6.2f ", m[i]);
  }
  fprintf(outfp, "\n-----------\n");
}


void getZRotMat(t, phi)
     mat_t *t;
     fastf_t phi;
{
  fastf_t sin_ = sin(DEG2RAD*phi);
  fastf_t cos_ = cos(DEG2RAD*phi);
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


void getYRotMat(t, theta)
     mat_t *t;
     fastf_t theta;
{
  fastf_t sin_ = sin(DEG2RAD*theta);
  fastf_t cos_ = cos(DEG2RAD*theta);
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


void getTrans(t, theta, phi, radius)
     mat_t *t;
     int theta;
     int phi;
     fastf_t radius;
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


/*
  void makeFlake(int depth, mat_t *trans, point_t center, fastf_t radius, float delta, int maxDepth)
*/
void makeFlake(depth, trans, center, radius, delta, maxDepth)
     int depth;
     mat_t *trans;
     point_t center;
     fastf_t radius;
     double delta;
     int maxDepth;
{
  char name[MAX_INPUT_LENGTH];
  int i = 0;
  point_t pcent;
  fastf_t newRadius;
  mat_t temp;
  point_t origin;
  point_t pcentTemp;
  vect_t normal;

  VSET(origin, 0, 0, 0);
  VSET(normal, 1.0, 0, 0);

  /* just return if depth == maxDepth */
  if (depth > maxDepth) return;


  /* create self, then recurse for each different angle */
  count++;
  sprintf(name, "sph%d", count);
  mk_tor(fp, name, center, normal, radius, radius/4.0);
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
 * defaultSettings() outputs the default values of the program to a
 * given file pointer.
 *
 ***********************/
void defaultSettings(outfp)
     FILE *outfp;
{
  fprintf(outfp, "Default values:\n\n");
  fprintf(outfp, "\tVerbose mode is %s\n", verbose ? "on" : "off");
  fprintf(outfp, "\tDebug mode is %s\n", debug ? "on" : "off");
  fprintf(outfp, "\tInteractive mode is %s\n", interactive ? "on" : "off");
  fprintf(outfp, "\n\tOutput file name is %s\n", outputFilename);
  fprintf(outfp, "\t\tid='%s' units='%s'\n", title, units);
  fprintf(outfp, "No action performed.\n");
}


/*
 * argumentExamples() outputs some examples of using the command line
 * arguments in a useful manner
 **********************************/
void argumentExamples(outfp, progname)
     FILE *outfp;
     char *progname;
{
  if (progname)
    fprintf(outfp, "Usage Examples: \n\n");

  return;
}


/*
 * parseArguments() is called by main to take care of all of the command-line
 * arguments that the user specifies.  The args that are read set variables
 * which, in turn, are used as fence parameters.
 *****************************/
int parseArguments(argc, argv)
     int argc;
     char **argv;
{
  int c = 0;

  const char *progname = argv[0];

  opterr = 0;

  while ((c=getopt(argc, argv, options)) != -1) {
    switch (c) {
      case 'I' :
        interactive=(DEFAULT_INTERACTIVE) ? 0 : 1;
        break;

      case 'i' :
        interactive=1;
        break;

      case 'D' :
        debug=(DEFAULT_DEBUG) ? 0 : 1;
        break;

      case 'd' :
        debug=1;
        break;

      case 'V' :
        verbose=(DEFAULT_VERBOSE) ? 0 : 1;
        break;

      case 'v' :
        verbose=1;
        break;

      case 'o' :
        memset(outputFilename, 0, DEFAULT_MAXNAMELENGTH);
        strncpy(outputFilename, optarg, DEFAULT_MAXNAMELENGTH);
        break;
      case 'O' :
        memset(outputFilename, 0, DEFAULT_MAXNAMELENGTH);
        strncpy(outputFilename, optarg, DEFAULT_MAXNAMELENGTH);
        break;

      case 'n' :
        memset(title, 0, DEFAULT_MAXNAMELENGTH);
        strncpy(title, optarg, DEFAULT_MAXNAMELENGTH);
        break;
      case 'N' :
        memset(title, 0, DEFAULT_MAXNAMELENGTH);
        strncpy(title, optarg, DEFAULT_MAXNAMELENGTH);
        break;

      case 'u' :
        memset(units, 0, DEFAULT_MAXNAMELENGTH);
        strncpy(units, optarg, DEFAULT_MAXNAMELENGTH);
        break;
      case 'U' :
        memset(units, 0, DEFAULT_MAXNAMELENGTH);
        strncpy(units, optarg, DEFAULT_MAXNAMELENGTH);
        break;

      case 'l' :
        // maxDepth = atoi(c);
        break;

      case '?' :
        (void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Command-line argument assistance");
        exit(1);
        break;

      default  : /*shouldn't be reached since getopt throws a ? for args not found*/
        (void)argumentHelp(DEFAULT_VERBOSE_OUTPUT, progname, "Illegal command-line argument");
        exit(1);
        break;
    }
  }
  return(optind);
}


/*
 * argumentHelp() is the all encompassing help message that is displayed when
 * an invalid command line argument is entered or if the user explicitly
 * requests assistance.
 ***************************************/
void argumentHelp(outfp, progname, message)
     FILE *outfp;
     const char *progname;
     char *message;
{
  if (message) {
    fprintf(outfp, "%s\n", message);
  }

  fprintf(outfp, "Usage Format: \n%s %s\n\n", progname, \
          "-[ivdonu]" \
          );

  fprintf(outfp, "\t-[zZ]\n\t\tdisplays the default settings\n");
  putc((int)'\n', outfp);

  return;
}


/* main() is basically just a simple interface to the program.  The interface changes the
 * defaults according to what the user has specified and offers the opportunity to enter
 * data in an interactive mode
 *******************/
int main(argc, argv)
     int argc;
     char **argv;
{
  int i;

  bu_strlcpy(parameters.fileName, DEFAULT_OUTPUTFILENAME, DEFAULT_MAXNAMELENGTH);
  parameters.maxDepth=DEFAULT_MAXDEPTH;
  parameters.maxInnerRadius=DEFAULT_MAXINNERRADIUS;
  parameters.maxTorusRadius=DEFAULT_MAXTORUSRADIUS;
  parameters.innerRadiusDelta=DEFAULT_RADIUSDELTA;
  parameters.torusRadiusDelta=DEFAULT_RADIUSDELTA;
  vect_t normal = DEFAULT_NORMAL;
  point_t position = DEFAULT_POSITION;
  VMOVE(parameters.normal, normal);
  VMOVE(parameters.position, position);
  //  parameters.materialArray;

  (void) parseArguments(argc, argv);

  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "\nUsing [%s] for output file\n\n", outputFilename);

  if (verbose) fprintf(DEFAULT_VERBOSE_OUTPUT, "Verbose mode is on\n");
  if (debug) fprintf(DEFAULT_DEBUG_OUTPUT, "Debug mode is on\n");
  if (verbose && interactive) fprintf(DEFAULT_VERBOSE_OUTPUT, "Interactive mode is on\n");

  if (verbose) {
    fprintf(DEFAULT_VERBOSE_OUTPUT, "\nDonut Flake Properties:\n");

    putc((int)'\n', DEFAULT_VERBOSE_OUTPUT);
  }

  initializeInfo(&parameters, parameters.fileName, parameters.maxDepth);

  /* now open a file for outputting the database */
  bu_strlcpy(parameters.fileName, outputFilename, DEFAULT_MAXNAMELENGTH);
  fp = wdb_fopen(parameters.fileName);
  if (fp==NULL) {
    perror(outputFilename);
    exit(2);
  }

  /* create the initial id */
  mk_id_units(fp, "DonutFlake", "mm");

  /* initialize the wmember structs...
     this is for creating the regions */
  wmemberArray = (struct wmember *)bu_malloc(sizeof(struct wmember)  *(parameters.maxDepth+1+ADDITIONAL_OBJECTS), "alloc wmemberArray");
  for (i = 0; i <= parameters.maxDepth+ADDITIONAL_OBJECTS; i++) {
    BU_LIST_INIT(&(wmemberArray[i].l));
  }

  /****** Create the Donuts ******/

  createDonuts(&parameters);

  /*
    now that the entire donuts has been created, we can create the
    additional objects needed to complete the scene.
  */
  /****** Create the Lights ******/

  createLights(&parameters);

  /****** Create the Plane ******/

  createPlane(&parameters);

  /****** Create the Environment map ******/

  createEnvironMap(&parameters);

  /****** Create the entire Scene combination ******/

  createScene(&parameters);

  wdb_close(fp);
  bu_free(wmemberArray, "free wmemberArray");


  return 0;
}
