/*
  TITLE: sphflake1.c 

  AUTHOR: Jason Owens

  DESCRIPTION: Program to create a sphere-flake utilizing libwdb
  
  */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "wdb.h" 

#define D2R(x) (((x)/180)*3.14159265358979)
#define MATXPNT(d, m, v) { \
  register double _i = 1.0/((m)[12]*(v)[0] + (m)[13]*(v)[1] + (m)[14]*(v)[2] + (m)[15]*1); \
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
#define PLANE_MAT "stack checker; plastic"
#define PLANE_MATPARAM "sh=20 sp=.1 di=.9"
#define PLANE_MATCOLOR "255 255 255"
#define ENVIRON_ID 4
#define ENVIRON_MAT "envmap cloud"

struct depthMat { 
  char name[20];
  char params[50];
  unsigned char color[3];
};
typedef struct depthMat depthMat_t;

struct params {
  char fileName[20];
  int  maxRadius;
  int  maxDepth;
  float deltaRadius;
  point_t pos;
  depthMat_t *matArray;
};
typedef struct params params_t;

int count = 0; /* global sphere count */
FILE *fp;
mat_t IDENT;

/* make the wmember structs, in order to produce individual     
   combinations so we can have separate materials among differing
   depths */
struct wmember *wmemberArray;

/* the vector directions, in which the flakes will be drawn */
/* theta,phi */
int dir[9][2] = {  {0,-90},
		   {60,-90},
		   {120,-90},
		   {180,-90},
		   {240,-90},
		   {300,-90},
		   {120,-30},
		   {240,-30},
		   {360,-30} };

/****** Function Prototypes ******/
BU_EXTERN(void initializeInfo, (params_t *p, int inter, int def, char *name, int depth));
BU_EXTERN(void createSphereflake, (params_t *p));
BU_EXTERN(void createLights, (params_t *p));
BU_EXTERN(void createPlane, (params_t *p));
BU_EXTERN(void createScene, (params_t *p));
BU_EXTERN(void createEnvironMap, (params_t *p));
BU_EXTERN(void getYRotMat, (mat_t *, fastf_t theta));
BU_EXTERN(void getZRotMat, (mat_t *, fastf_t phi));
BU_EXTERN(void getTrans, (mat_t *, int, int, fastf_t)); 
BU_EXTERN(void makeFlake, (int depth,mat_t *trans,point_t center, fastf_t radius, float delta,int maxDepth));
BU_EXTERN(void usage, (char *n));


void main(argc, argv)
int argc;
char **argv;
{
  int i, j;
  extern char *optarg;
  int optc;
  params_t params;
  int inter = 0;
  int def = 0;
  int depth = 0;
  char fileName[20];
  int opts = 0;

  while ( (optc = getopt( argc, argv, "hiDd:f:" )) != -1 ) {
    switch (optc) {
    case 'i' : /* interactive mode */
      inter = 1;
      opts = 1;
      break;
    case 'D':  /* Use ALL default parameters */
      def = 1;
      depth = 3;
      strcpy(fileName, DEFAULT_FILENAME);
      opts = 1;
      break;
    case 'd':  /* Use a user-defined depth */
      depth = atoi(optarg);
      if (depth > 5) {
	printf("\nWARNING: Depths greater than 5 produce extremely large numbers of objects.\n");
      }
      if (def) {
	depth = 3;
	break;
      }
      strcpy(fileName, DEFAULT_FILENAME);
      opts = 1;
      break;
    case 'f':  /* Use a user-defined filename */
      strcpy(fileName, optarg);
      opts = 1;
      break;
    case 'h':
    case '?':
      usage(argv[0]);
      exit(0);
    default:
      break;
    }
  }
  if (!opts) {
    printf("Using all defaults. Try %s -h\n", argv[0]);
    inter = 0;
    def = 1;
    depth = 3;
    strcpy(fileName, DEFAULT_FILENAME);
  }
  
  
  initializeInfo(&params, inter, def, fileName, depth);
	   
  /* now open a file for outputting the database */
  fp = fopen(params.fileName, "w");

  /* create the initial id */
  i = mk_id_units(fp, "SphereFlake", "mm");

  /* initialize the wmember structs... 
     this is for creating the regions */
  wmemberArray = (struct wmember *)malloc(sizeof(struct wmember)
					  *(params.maxDepth+1+ADDITIONAL_OBJECTS));
  for (i = 0; i <= params.maxDepth+ADDITIONAL_OBJECTS; i++) {
    RT_LIST_INIT(&(wmemberArray[i].l));
  }

  /****** Create the SphereFlake ******/

  createSphereflake(&params);
  
  /* 
     now that the entire sphereflake has been created, we can create the 
     additional objects needed to complete the scene.
  */
  /****** Create the Lights ******/

  createLights(&params);
  
  /****** Create the Plane ******/

  createPlane(&params);

  /****** Create the Environment map ******/
  
  createEnvironMap(&params);

  /****** Create the entire Scene combination ******/
  
  createScene(&params);

  fclose(fp);
  
  return;
}

void usage(n)
char *n;
{
  printf(
"\nUSAGE: %s -D -d# -i -f fileName\n\
       D -- use default parameters\n\
       d -- set the recursive depth of the procedure\n\
       i -- use interactive mode\n\
       f -- specify output file\n\n", n);
}

void initializeInfo(p, inter, def, name, depth)
params_t *p;
int inter;
int def;
char *name;
int depth;
{
  char matName[20];
  int i = 0;
  char c[3];
  int r, g, b;


  if (name == NULL) {
    strcpy(p->fileName, DEFAULT_FILENAME);
  }
  else {
    strcpy(p->fileName, name);
  }
  p->maxRadius = DEFAULT_MAXRADIUS;
  p->maxDepth =  (depth != 0) ? (depth) : (DEFAULT_MAXDEPTH);
  p->deltaRadius = DEFAULT_DELTARADIUS;
  p->pos[X] = DEFAULT_ORIGIN_X;
  p->pos[Y] = DEFAULT_ORIGIN_Y;
  p->pos[Z] = DEFAULT_ORIGIN_Z;
  if (inter) {
    /* prompt the user for some data */
    /* no error checking here.... */
    if (name != NULL) {
      printf("\nPlease enter a filename: ");
      scanf("%s", p->fileName);
    } else
      strcpy(p->fileName, name);

    printf("maxRadius: ");
    scanf("%d", &(p->maxRadius));
    printf("maxDepth: ");
    scanf("%d", &(p->maxDepth));
    printf("deltaRadius: ");
    scanf("%f", &(p->deltaRadius));
    printf("init. position X Y Z: ");
    scanf("%lg %lg %lg", &(p->pos[X]), &(p->pos[Y]), &(p->pos[Z]));
    getchar();
    
    p->matArray = (depthMat_t *)malloc(sizeof(depthMat_t) * (p->maxDepth+1));

    for (i = 0; i <= p->maxDepth; i++) {
      printf("Material for depth %d: [%s] ", i, DEFAULT_MAT);
      gets(p->matArray[i].name);
      if (strcmp(p->matArray[i].name, "") == 0) {
	strcpy(p->matArray[i].name, DEFAULT_MAT);
      }
      printf("Mat. params for depth %d: [%s] ", i, DEFAULT_MATPARAM);
      gets(p->matArray[i].params);
      if (strcmp(p->matArray[i].params, "") == 0) {
	strcpy(p->matArray[i].params, DEFAULT_MATPARAM);
      }
      printf("Mat. color for depth %d: [%s] ", i, DEFAULT_MATCOLOR);
      gets(matName);
      if (strcmp(matName, "") == 0) {
	sscanf(DEFAULT_MATCOLOR, "%d %d %d", &r, &g, &b);
	p->matArray[i].color[0] = (char)r;
	p->matArray[i].color[1] = (char)g;
	p->matArray[i].color[2] = (char)b;
      }
      else {
	sscanf(matName, "%d %d %d", &r, &g, &b);
	p->matArray[i].color[0] = (char)r;
	p->matArray[i].color[1] = (char)g;
	p->matArray[i].color[2] = (char)b;
      }
    }
  }
  MAT_IDN(IDENT);
}

void createSphereflake(p)
params_t *p;
{
  mat_t trans;
  char name[20];
  int i = 0;

  /* now begin the creation of the sphereflake... */
  MAT_IDN(trans); /* get the identity matrix */
  makeFlake(0, trans, p->pos, (fastf_t)p->maxRadius * DEFAULT_SCALE, p->deltaRadius, p->maxDepth);
  /* 
     Now create the depth combinations/regions
     This is done to facilitate application of different
     materials to the different depths */
  for (i = 0; i <= p->maxDepth; i++) {
    sprintf(name, "depth%d.r", i);
    mk_lcomb(fp, name, &(wmemberArray[i+ADDITIONAL_OBJECTS]), 1, 
	     p->matArray[i].name, p->matArray[i].params, p->matArray[i].color,0);
  }
  printf("\nSphereFlake created");

}

void createLights(p)
params_t *p;
{
  char name[20];
  point_t lPos;
  int r, g, b;
  char c[3];
  

  /* first create the light spheres */
  VSET(lPos, p->pos[X]+(5 * p->maxRadius), p->pos[Y]+(-5 * p->maxRadius), p->pos[Z]+(150 * p->maxRadius));
  sprintf(name, "light0");
  mk_sph(fp, name, lPos, p->maxRadius*5);
  
  /* now make the light region... */
  mk_addmember(name, &(wmemberArray[LIGHT0_ID]), WMOP_UNION);
  strcat(name, ".r");
  sscanf(LIGHT0_MATCOLOR, "%d %d %d", &r, &g, &b);
  c[0] = (char)r;
  c[1] = (char)g;
  c[2] = (char)b;
  mk_lcomb(fp, name, &(wmemberArray[LIGHT0_ID]), 1, LIGHT0_MAT, LIGHT0_MATPARAM,
	(CONST unsigned char *) c, 0);
  
  VSET(lPos, p->pos[X]+(13 * p->maxRadius), p->pos[Y]+(-13 * p->maxRadius), p->pos[Z]+(152 * p->maxRadius));
  sprintf(name, "light1");
  mk_sph(fp, name, lPos, p->maxRadius*5);

  /* now make the light region... */
  mk_addmember(name, &(wmemberArray[LIGHT1_ID]), WMOP_UNION);
  strcat(name, ".r");
  sscanf(LIGHT1_MATCOLOR, "%d %d %d", &r, &g, &b);
  c[0] = (char)r;
  c[1] = (char)g;
  c[2] = (char)b;
  mk_lcomb(fp, name, &(wmemberArray[LIGHT1_ID]), 1, LIGHT1_MAT, LIGHT1_MATPARAM,
	(CONST unsigned char *) c, 0);

  printf("\nLights created");
}

void createPlane(p)
params_t *p;
{
  char name[20];
  point_t lPos;

  VSET(lPos, 0, 0, 1); /* set the normal */
  sprintf(name, "plane");
  mk_half(fp, name, lPos, -p->maxRadius * 2 * DEFAULT_SCALE);

  /* now make the plane region... */
  mk_addmember(name, &(wmemberArray[PLANE_ID]), WMOP_UNION);
  strcat(name, ".r");
  mk_lcomb(fp, name, &(wmemberArray[PLANE_ID]), 1, PLANE_MAT, PLANE_MATPARAM, PLANE_MATCOLOR, 0);

  printf("\nPlane created");
}

void createEnvironMap(p)
params_t *p;
{
  char name[20];
  
  sprintf(name, "light0");
  mk_addmember(name, &(wmemberArray[ENVIRON_ID]), WMOP_UNION);
  sprintf(name, "environ.r");
  mk_lcomb(fp, name, &(wmemberArray[ENVIRON_ID]), 1, ENVIRON_MAT, "", "0 0 0", 0);

  printf("\nEnvironment map created");

}

void createScene(p)
params_t *p;
{
  int i;
  char name[20];

  for (i = 0; i < p->maxDepth+1; i++) {
    sprintf(name, "depth%d.r", i);
    mk_addmember(name, &(wmemberArray[SCENE_ID]), WMOP_UNION);
  }
  mk_addmember("light0.r", &(wmemberArray[SCENE_ID]), WMOP_UNION);
  mk_addmember("light1.r", &(wmemberArray[SCENE_ID]), WMOP_UNION);
  mk_addmember("plane.r", &(wmemberArray[SCENE_ID]), WMOP_UNION);
  mk_addmember("environ.r", &(wmemberArray[SCENE_ID]), WMOP_UNION);
  sprintf(name, "scene.r");
  mk_lfcomb(fp, name, &(wmemberArray[SCENE_ID]), 0);

  printf("\nScene created (FILE: %s)\n", p->fileName);
}

void printMatrix(n, m)
char *n;
mat_t m;
{
  int i = 0;
  printf("\n-----%s------\n", n);
  for (i = 0; i < 16; i++) {
    if (i%4 == 0 && i != 0) printf("\n");
    printf("%6.2f ", m[i]);
  }
  printf("\n-----------\n");
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

  getZRotMat(z, theta);
  getYRotMat(y, phi);

  mat_mul2(toRelative, newPos); /* translate to new position */
  mat_mul2(y, newPos);          /* rotate z */
  mat_mul2(z, newPos);          /* rotate y */
  MAT_DELTAS(*t, 0,0,0);
  mat_mul2(*t, newPos);
  
  memcpy(*t, newPos, sizeof(newPos));
}

void getYRotMat(t, theta)
mat_t *t;
fastf_t theta;
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

void getZRotMat(t, phi)
mat_t *t;
fastf_t phi;
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

void makeFlake(depth, trans, center, radius, delta, maxDepth)
int depth;
mat_t *trans;
point_t center;
fastf_t radius;
float delta;
int maxDepth;
{
  char name[20];
  int i = 0;
  point_t pcent;
  fastf_t newRadius;
  mat_t temp;
  mat_t invTrans;
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
  mk_addmember(name, &(wmemberArray[depth+ADDITIONAL_OBJECTS]), WMOP_UNION);

  for (i = 0; i < 9; i++) {
    memcpy(temp, trans, sizeof(temp));
    getTrans(temp, dir[i][0], dir[i][1], radius+newRadius);
    MATXPNT(pcentTemp, temp, origin);
    VADD2(pcent, pcentTemp, center);
    makeFlake(depth+1, temp, pcent, newRadius, delta, maxDepth);
  }
}
