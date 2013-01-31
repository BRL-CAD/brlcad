/* Kevin Scott
   CS651 Assignment 1.1
   Sept 30, 1998

   This code is cursed.
*/

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FL/forms.H"

#include "trackball.h"
#include "geom.h"
#include "poly.h"
#include "polyviewctrl.h"

#include "vds.h"
#include "stdvds.h"
#include "vector.h"

/* Defines. */
#define INSPECT 0
#define WALKTHRU 1

#define WIRE_FRAME 0
#define SHADED_LIT 1

typedef float Matrix4[4][4];

/* Callbacks for glut. */
void reshape(int w, int h);
void mouse(int button, int state, int x, int y);
void keyboard(unsigned char key, int x, int y);
void motion(int x, int y);
void display(void);
void idle (void);

/* Main rendering function. */
void drawScene(void);

/* Main initialization function. */
void init (void);

/* Utility routines. */
void printHelp(void);
void processCmdLine(int argc, char *argv[]);
void set4fv(GLfloat *v, GLfloat x, GLfloat y, GLfloat z, GLfloat a);
void setdefault(void);
float calcViewAngle (float size, float distance);
void getObjectSize (object *o, float *xysize, float *zsize,
		    float *x, float *y, float *z);
void objectToScene (object *o, float *sceneSize);

/* matrix inverter, in invmatrix.c */
extern void mat4Inverse( Matrix4 dst, Matrix4 src );

/* Global variables. Yuck. But it's C, waddaya gonna do? */

FD_PolyViewCtrl *fd_poly;        /* Structure for accessing our xforms window. */
static GLfloat  rm[4][4];        /* Scene rotation matrix. */
static float insp_zt;         /* Scene translation in z for INPECT mode. */
static float ix, iz;          /* X and Z view position in WALKTHRU mode. */
static float viewAngle;       /* Viewing angle. */
static float sceneDist;       /* Distance from view point to scene. */
static float sceneSize;       /* Max value of scene size in x, y or z. */ 
static float near, far;       /* Near & far (z) clipping planes at -near,-far. */
static GLint    W,H;             /* Viewport width and height in pixels. */

static int      lstx, lsty;      /* Last x & y mouse coordinates. */
static GLfloat  curquat[4];      /* Current quarternion rotation. */
static GLfloat  lastquat[4];     /* Last quarternion rotation. */
static int      lmouse_down;     /* Status of the mouse. */

static int      viewMode;        /* Current viewing mode: INSPECT or WALKTHRU. */
static int      renderMode;      /* Current render mode: WIRE_FRAME or SHADED_LIT. */

static object   o;               /* Current object being displayed. */

static GLfloat global_ambient[]  = { 0.2, 0.2, 0.2, 1.0 };
static GLfloat light0_ambient[]  = { 0.0, 0.0, 0.0, 1.0 };
static GLfloat light0_diffuse[]  = { 1.0, 1.0, 1.0, 1.0 };
static GLfloat light0_specular[] = { 1.0, 1.0, 1.0, 1.0 };
static GLfloat light0_position[] = { 0.0, 0.0, 1.0, 0.0 };

/* Stuff I added for Dave & Dale. */
static GLfloat def_clear[4]   = { 1.0, 1.0, 1.0, 0.0};
static GLfloat def_diffuse[4] = { 0.5, 0.5, 0.5, 1.0};
static GLfloat def_zoom       = 0.0;
static GLfloat def_quat[4]    = { 0.0, 0.0, 0.0, 1.0};
static int     view_mode      = 0;
static int     def_mode       = WIRE_FRAME;
static int     grab_mode      = 0;
static char    grab_file[255]  = "";
/* VDS-related globals */
int usevds;
float threshold;

/* Main doesn't do much more than initialize scene params and GLUT. */
int main(int argc, char *argv[]) {

  /* First process the command line. */
  processCmdLine(argc,argv);

  /* Then initialize xforms so that it sees the -display flag before GLUT. */
  fl_initialize(&argc,argv,"PolyView",0,0);
  fd_poly = create_form_PolyViewCtrl();
  fl_show_form(fd_poly->PolyViewCtrl,
	       FL_PLACE_POSITION | FL_PLACE_SIZE,FL_FULLBORDER,
	       "PolyView 0.1");
  
  /* Then do the obligatory GLUT stuff. */
  glutInit(&argc,argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutInitWindowSize(500,500);
  glutInitWindowPosition(20,20);
  glutCreateWindow("Viewer");

  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutMouseFunc(mouse);
  glutKeyboardFunc(keyboard);
  glutMotionFunc(motion);
  glutIdleFunc(idle);

  /* Initialize all of my stuff. */
  init();

  /* Finally, enter the GLUT event loop. */
  glutMainLoop();
  return 0;
}

/* All of this goo is for the callbacks for xforms. */

/* Call back for the enable VDS button */
void cb_vds(FL_OBJECT *obj, long argument) {
    usevds = fl_get_button(obj);
    glutPostRedisplay();
}

/* Call back for the VDS threshold slider */
void cb_threshold(FL_OBJECT *obj, long argument) {
    threshold = fl_get_slider_value(obj);
    glutPostRedisplay();
}

/* Call back for the Load button */
void cb_load(FL_OBJECT *obj, long argument) {
    const char *fname;
    FILE *loadfile;

    fname = fl_show_file_selector("Load vertex tree from file...","","*.vds","");
    if (fname == NULL) { return; }
    loadfile = fopen(fname, "r");
    if (loadfile == NULL)
    {
	fprintf(stderr, "Error: couldn't open file %s for reading!\n", fname);
	return;
    }
    vdsFreeTree(o.vertexTree); 
    o.vertexTree = vdsReadTree(loadfile, NULL);
    fclose(loadfile);
}

/* Call back for the Save button */
void cb_save(FL_OBJECT *obj, long argument) {
    const char *fname;
    FILE *savefile;

    fname = fl_show_file_selector("Save vertex tree to file...","","*.vds","");
    if (fname == NULL) { return; }
    savefile = fopen(fname, "w");
    if (savefile == NULL)
    {
	fprintf(stderr, "Error: couldn't open file %s for writing!\n", fname);
	return;
    }
    vdsWriteTree(savefile, o.vertexTree, NULL);
    fclose(savefile);
}

/* Call back for the quit button. */
void cb_quit(FL_OBJECT *obj, long argument) {
    vdsFreeTree(o.vertexTree);
    exit(0);
}

/* Call back for the wireframe button. */
void cb_wireframe(FL_OBJECT *obj, long argment) {
  renderMode = WIRE_FRAME;
  glutPostRedisplay();
}

/* Call back for the shaded/lit button. */
void cb_shadedlit(FL_OBJECT *obj, long argument) {
  renderMode = SHADED_LIT;
  glutPostRedisplay();
}

/* Call back for the inspect button. */
void cb_inspect(FL_OBJECT *obj, long argument) {
  viewMode = INSPECT;
  glutPostRedisplay();
}

/* Call back for the walkthru button. */
void cb_walkthru(FL_OBJECT *obj, long argument) {
  viewMode = WALKTHRU;
  glutPostRedisplay();
}

/* Call back for material ambient sliders. */
void cb_mat_amb(FL_OBJECT *obj, long i) {
  float val;

  val = (float)fl_get_slider_value(obj);
  if (i >= 0 && i <= 2) {
    o.mat_ambient[i] = val;
    glutPostRedisplay();
  }
}

/* Call back for material diffuse sliders. */
void cb_mat_dif(FL_OBJECT *obj, long i) {
  float val;

  val = (float)fl_get_slider_value(obj);
  if (i >= 0 && i <= 2) {
    o.mat_diffuse[i] = val;
    glutPostRedisplay();
  }
}

/* Call back for material specular sliders. */
void cb_mat_spec(FL_OBJECT *obj, long i) {
  float val;

  val = (float)fl_get_slider_value(obj);
  if (i >= 0 && i <= 2) {
    o.mat_specular[i] = val;
    glutPostRedisplay();
  }
}

/* Call back for material emission sliders. */
void cb_mat_emis(FL_OBJECT *obj, long i) {
  float val;

  val = (float)fl_get_slider_value(obj);
  if (i >= 0 && i <= 2) {
    o.mat_emission[i] = val;
    glutPostRedisplay();
  }
}

/* Call back for global ambient sliders. */
void cb_glb_amb(FL_OBJECT *obj, long i) {
  float val;

  val = (float)fl_get_slider_value(obj);
  if (i >= 0 && i <= 2) {
    global_ambient[i] = val;
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,global_ambient);
    glutPostRedisplay();
  }
}

/* Call back for reset button. */
void cb_reset(FL_OBJECT *obj, long arg) {
  setdefault();
  glutPostRedisplay();
}

/* Call back for zoom slider. */
void cb_zoom(FL_OBJECT *obj, long arg) {
  float val;

  val = (float)fl_get_slider_value(obj);
  insp_zt = 5.0 * val * sceneSize;
  glutPostRedisplay();
}

/* Call back for shininess slider. */
void cb_mat_shiny(FL_OBJECT *obj, long i) {
  float val;

  val = (float)fl_get_slider_value(obj);
  o.mat_shininess = val;
  glutPostRedisplay();
}

/* Call back for light positioner. */
void cb_light_pos(FL_OBJECT *obj, long arg) {
  float x,y;
  
  x = (float)fl_get_positioner_xvalue(obj);
  y = (float)fl_get_positioner_yvalue(obj);
  light0_position[0] = x * sceneSize;
  light0_position[1] = y * sceneSize;
  glutPostRedisplay();
}

/* Handle a reshape event. */
void reshape(int w, int h) {
  /* Set the viewport. */
  glViewport(0,0,(GLsizei)w,(GLsizei)h);

  /* Reset the projection transform. */
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(viewAngle,1.0,near,far);
  glMatrixMode(GL_MODELVIEW);

  /* Store new width and height globally. */
  W = w; H = h;
}

/* Handle a mouse (up/down) event. */
void mouse(int button, int state, int x, int y) {
  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
    lstx = x;
    lsty = y;
    lmouse_down = 1;
  } else {
    lmouse_down = 0;
  }
}

/* Handle a keyboard event. */
void keyboard(unsigned char key, int x, int y) {
  if (key == 'v') {
    printf("-q %8.4f %8.4f %8.4f %8.4f -z %8.4f\n",
      curquat[0],curquat[1],curquat[2],curquat[3],insp_zt);
  }
}
           

/* GLUT idle function. */
void idle (void) {
  fl_check_forms();
}

/* Handle mouse motion while a button is down. */
void motion(int x, int y) {
  if (lmouse_down) {
    if (viewMode == INSPECT) {
      trackball(lastquat,(2.0*lstx-W)/W,(H-2.0*lsty)/H,(2.0*x-W)/W,(H-2.0*y)/H);
      add_quats(lastquat,curquat,curquat);
    } else {
      iz += sceneSize * (  2.0*((float)(y - lsty) / H) );
      ix += sceneSize * (  2.0*((float)(x - lstx) / W) );
    }
    lstx = x; lsty = y;
    glutPostRedisplay();
  }
}

/* Handle a display request. */
void display(void) {

  /* Clear the display and depth buffer. */
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* The modelling transform. */
  glLoadIdentity();

  /* Position the light. */
  glLightfv(GL_LIGHT0,GL_POSITION,light0_position);

  /* Now setup the modelling transform. */

  if (viewMode == WALKTHRU) {
    /* Set the viewing position. */
    gluLookAt(0.0,0.0,iz,ix,0.0,0.0,0.0,1.0,0.0);
  } else if (viewMode == INSPECT) {
    gluLookAt(0.0,0.0,sceneDist,0.0,0.0,0.0,0.0,1.0,0.0);
    /* Rotate using trackball quarternion for INSPECT mode. */
    glTranslated(0.0,0.0,insp_zt);
    build_rotmatrix(rm,curquat);
    glMultMatrixf(&rm[0][0]);
  }

  /* now, draw the scene. */
  drawScene();
  glutSwapBuffers();
  if (grab_mode) {
    GLubyte image[3*500*500];
    int x,y;
    FILE *imagefile;

    glReadBuffer(GL_FRONT);
    glReadPixels (0,0,500, 500, GL_RGB, GL_UNSIGNED_BYTE, image);
  
    if( (imagefile = fopen(grab_file,"wb")) == NULL )
    {
      fprintf(stderr,"Error opening output image file.\n");
      exit(1);
    }
  
    fprintf(imagefile, "P6\n%d %d 255\n", 500, 500);
  
    if( ferror(imagefile) )
    {
      fprintf(stderr,"Error writing output image file header.\n");
      exit(1);
    }
  
    for(x=499;x>=0;x--)
      for(y=0;y<500;y++)
      {
        fputc(image[(x*500+y)*3  ],imagefile); /* R */
        fputc(image[(x*500+y)*3+1],imagefile); /* G */
        fputc(image[(x*500+y)*3+2],imagefile); /* B */
      }
  
    fflush(imagefile);
  
    if( fclose(imagefile) == EOF )
    {
      fprintf(stderr,"Error closing output image file.\n");
      exit(1);
    }
    exit(0);
  }
}

/* Main rendering function. */
void drawScene(void) {
  int i, j;
  float x, y, z, nx, ny, nz;

  if (renderMode == WIRE_FRAME) {
    /* Set polygon mode to lines and disable lighting. */
    glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
    glDisable(GL_LIGHTING);
    glDisable(GL_LIGHT0);
    glColor3fv(o.mat_diffuse);
  } else if (renderMode == SHADED_LIT) {
    /* Set polygon mode to filled and enable lighting. */
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    /* Set material properties. */
    glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,o.mat_ambient);
    glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,o.mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,o.mat_specular);
    glMaterialfv(GL_FRONT_AND_BACK,GL_EMISSION,o.mat_emission);
    glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,o.mat_shininess);
  }

  /* Translate object to the origin, where it shall live forevermore. */
  glTranslated(-o.cx,-o.cy,-o.cz);

  /* Find and set the current viewpoint, look vector, and FOV */
  if (usevds)
  {
      Matrix4 viewmat, invviewmat;
      vdsVec3 eyept, viewvector;

      glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *)viewmat);
      mat4Inverse(invviewmat, viewmat);
      eyept[0] = invviewmat[3][0];
      eyept[1] = invviewmat[3][1];
      eyept[2] = invviewmat[3][2];
      viewvector[0] = invviewmat[2][0];
      viewvector[1] = invviewmat[2][1];
      viewvector[2] = invviewmat[2][2];
      VEC3_NORMALIZE(viewvector);
      vdsSetViewpoint(eyept);
      vdsSetLookVec(viewvector);
      vdsSetFOV(viewAngle * M_PI / 180);
      vdsSetThreshold(threshold);
      vdsAdjustTreeBoundary(o.vertexTree, vdsThresholdTest);
      if (renderMode == WIRE_FRAME) {
	  vdsRenderTree(o.vertexTree, vdsRenderWireframe, vdsSimpleVisibility);
      }
      else if (renderMode == SHADED_LIT) {
	  vdsRenderTree(o.vertexTree, vdsRenderLit, vdsSimpleVisibility);
      }
  }
  else	/* usevds == 0 */
  {
      /* Draw triangles. */
      for(i=0;i<o.nt;i++) {
	  glNormal3fv(o.t[i].normal);
	  glBegin(GL_TRIANGLES);
	  for(j=0;j<3;j++) {
	      glVertex3fv(o.v[o.t[i].verts[j] - 1].coord);
	  }
	  glEnd();
      }
  }
}

/* Set a 4 vector of GLfloats. */
void set4fv(GLfloat *v, GLfloat x, GLfloat y, GLfloat z, GLfloat a) {
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = a;
}

/* Force modeling, lighting and material properties to their default values. 
 * This routine is a terrible hack.  I would have preferred better data
 * structures for tying GL properties to GUI elements, but alas the GUI
 * was an afterthought and I am short on time. */
void setdefault(void) {

    /* Default VDSlib settings */
    threshold = 0.01;
    vdsSetThreshold(threshold);
    fl_set_slider_bounds(fd_poly->sld_threshold, 0.0, 0.25);
    fl_set_slider_value(fd_poly->sld_threshold, threshold);
    usevds = 1;
    fl_set_button(fd_poly->btn_vds, usevds);
    vdsSetFOV(viewAngle * M_PI / 180.0);

  /* Default material ambient. */
  set4fv(o.mat_ambient,0.0,0.0,0.0,1.0);
  fl_set_slider_value(fd_poly->sld_mat_amb_r,o.mat_ambient[0]);
  fl_set_slider_value(fd_poly->sld_mat_amb_g,o.mat_ambient[1]);
  fl_set_slider_value(fd_poly->sld_mat_amb_b,o.mat_ambient[2]);

  /* Default material diffuse. */
  /* set4fv(o.mat_diffuse,0.5,0.5,0.5,1.0); */
  memcpy(o.mat_diffuse,def_diffuse,sizeof(o.mat_diffuse));
  fl_set_slider_value(fd_poly->sld_mat_dif_r,o.mat_diffuse[0]);
  fl_set_slider_value(fd_poly->sld_mat_dif_g,o.mat_diffuse[1]);
  fl_set_slider_value(fd_poly->sld_mat_dif_b,o.mat_diffuse[2]);

  /* Default material specular. */
  set4fv(o.mat_specular,0.0,0.0,0.0,1.0);
  fl_set_slider_value(fd_poly->sld_mat_spec_r,o.mat_specular[0]);
  fl_set_slider_value(fd_poly->sld_mat_spec_g,o.mat_specular[1]);
  fl_set_slider_value(fd_poly->sld_mat_spec_b,o.mat_specular[2]);

  /* Default material emission. */
  set4fv(o.mat_emission,0.0,0.0,0.0,1.0);
  fl_set_slider_value(fd_poly->sld_mat_emis_r,o.mat_emission[0]);
  fl_set_slider_value(fd_poly->sld_mat_emis_g,o.mat_emission[1]);
  fl_set_slider_value(fd_poly->sld_mat_emis_b,o.mat_emission[2]);

  /* Default material shininess. */
  o.mat_shininess = 2.0;
  fl_set_slider_value(fd_poly->sld_mat_shiny,o.mat_shininess);

  /* Default global ambient lighting. */
  set4fv(global_ambient,0.2,0.2,0.2,1.0);
  fl_set_slider_value(fd_poly->sld_glb_amb_r,global_ambient[0]);
  fl_set_slider_value(fd_poly->sld_glb_amb_g,global_ambient[1]);
  fl_set_slider_value(fd_poly->sld_glb_amb_b,global_ambient[2]);

  /* Default light0 position. */
  set4fv(light0_position,0.0,0.0,2.0*sceneSize,0.0);
  fl_set_positioner_xvalue(fd_poly->light_pos,0.0);
  fl_set_positioner_yvalue(fd_poly->light_pos,0.0);

  /* Default viewing mode. */
  viewMode = INSPECT;
  fl_set_button(fd_poly->btn_inspect,1);
  
  /* Default rendering mode. */
  renderMode = def_mode; 
  if (renderMode == WIRE_FRAME) {
    fl_set_button(fd_poly->btn_wireframe,1);
  } else {
    fl_set_button(fd_poly->btn_shaded,1);
  }

  /* Default WALKTHRU mode settings. */
  iz = sceneDist;
  ix = 0.0;

  /* Default INSPECT mode settings. */
  if (view_mode)
    memcpy(curquat,def_quat,sizeof(curquat));
  else
    trackball(curquat,0.0,0.0,0.0,0.0);

  /* Default ZOOM setting for inspect mode. */
  insp_zt = def_zoom;
  fl_set_slider_value(fd_poly->sld_zoom,insp_zt);
}

/* Main initialization function. */
void init (void) {
  float zsize;

  /* Determine the size and center of the object. */
  objectToScene(&o,&sceneSize);
  sceneDist = 3.0 * sceneSize;

  /* Compute stuff for the projection transformation. */
  viewAngle = calcViewAngle(sceneSize,2.0*sceneSize);
  near = sceneSize/10.0;
  far = 10.0 * sceneSize;

  /* Set user adjustable params to their defaults. */
  setdefault();

  /* Other Open GL initialization things... */
  glClearColor(def_clear[0],def_clear[1],def_clear[2],def_clear[3]);
  glShadeModel(GL_SMOOTH);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT,global_ambient);
  glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
  glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,GL_TRUE);

  glLightfv(GL_LIGHT0,GL_AMBIENT,light0_ambient);
  glLightfv(GL_LIGHT0,GL_DIFFUSE,light0_diffuse);
  glLightfv(GL_LIGHT0,GL_SPECULAR,light0_specular);
  
  glEnable(GL_DEPTH_TEST);

  lmouse_down = 0;
}

/* Compute a viewing angle (degrees) given a scene size and distance. */
float calcViewAngle (float size, float distance) {
  float thetaRad, thetaDeg;
  
  thetaRad = 2.0 * atan2(size/2.0, distance);
  thetaDeg = (180.0 * thetaRad) / M_PI;
  return thetaDeg;
}

/* Given an object, determines scaling and translation factors to make the
   object visible at the origin in the current global coordinate system. */
void objectToScene (object *o, float *sceneSize) {
  int i;
  float minx, maxx, miny, maxy, minz, maxz;
  float osize;
  float len;

  if (o->nf == 0 || o->nv == 0) {
    /* Object is empty. */
    o->scale = 1.0;
    o->cx = 0.0;
    o->cy = 0.0;
    o->cz = 0.0;
    o->xdim = o->ydim = o->zdim = 0.0;
  } else {
    /* Initialize min and max parms. */
    minx = miny = minz = HUGE_VAL;
    maxx = maxy = maxz = -HUGE_VAL;

    /* Find the bounding box for object o and normalize normals. */
    for(i=0;i<o->nv;i++) {
      if (o->v[i].coord[0] < minx) minx = o->v[i].coord[0];
      if (o->v[i].coord[0] > maxx) maxx = o->v[i].coord[0];
      if (o->v[i].coord[1] < miny) miny = o->v[i].coord[1];
      if (o->v[i].coord[1] > maxy) maxy = o->v[i].coord[1];
      if (o->v[i].coord[2] < minz) minz = o->v[i].coord[2];
      if (o->v[i].coord[2] > maxz) maxz = o->v[i].coord[2];
      /*
      len = sqrt(o->v[i].normal[0]*o->v[i].normal[0] +
		 o->v[i].normal[1]*o->v[i].normal[1] +
		 o->v[i].normal[2]*o->v[i].normal[2]);
      if (len != 0.0) {
	o->v[i].normal[0] /= len;
	o->v[i].normal[1] /= len;
	o->v[i].normal[2] /= len;
      }
      */
    }

    o->xdim = maxx - minx;
    o->ydim = maxy - miny;
    o->zdim = maxz - minz;

    /* Find maximum x-y object size. */
    if (o->xdim > o->ydim) {
      osize = o->xdim;
    } else {
      osize = o->ydim;
    }

    /* Now, check z. */
    if (o->zdim > osize) {
      osize = o->zdim;
    }
    *sceneSize = osize;

    /* Compute center of this bounding box. */
    o->cx = minx + o->xdim / 2.0;
    o->cy = miny + o->ydim / 2.0;
    o->cz = minz + o->zdim / 2.0;

    /* Compute scaling factor. */
    o->scale = 1.0; 
  }
}

void readTuple (char *argv[], int beg, int n, GLfloat v[4]) {
  int i;

  for(i=0;i<n;i++) {
    v[i] = atof(argv[beg+i]);
  }
}

/* Parse/process the command line. 
 * Options are:
 *    -pf filename    == open .poly file filename. 
 *    -h              == display help message.
 */
void processCmdLine(int argc, char *argv[]) {
  int j, i = 1, got_polys = 0;

  /* Setup default properties for the global object o. */
  o.nf = o.nv = o.nt = 0;
 
  /* Parse the command line. */
  while(i < argc) {
    if (strcmp(argv[i],"-pf") == 0) {
      if (i+1 < argc ) {
	/* Process file. */
	read_poly(argv[i+1],&o);
	got_polys = 1;
	i += 2;
      } else {
	fprintf(stderr,"Must give a filename with -pf flag.\n");
	exit(1);
      }
    } else if (strcmp(argv[i],"-h") == 0) {
      printHelp();
      exit(0);
    } else if (strcmp(argv[i],"-bg") == 0) {
      readTuple(argv,i+1,3,def_clear);
      i += 4;
    } else if (strcmp(argv[i],"-oc") == 0) {
      readTuple(argv,i+1,3,def_diffuse);
      i += 4;
    } else if (strcmp(argv[i],"-z") == 0) {
      def_zoom = atof(argv[i+1]);
      i += 2;
    } else if (strcmp(argv[i],"-q") == 0) {
      view_mode = 1;
      readTuple(argv,i+1,4,def_quat);
      i += 5;
    } else if (strcmp(argv[i],"-w") == 0) {
      def_mode = WIRE_FRAME;
      i++;
    } else if (strcmp(argv[i],"-s") == 0) {
      def_mode = SHADED_LIT;
      i++;
    } else if (strcmp(argv[i],"-o") == 0) {
      grab_mode = 1;
      strncpy(grab_file, argv[i+1], 255);
      i += 2;
    } else if (strcmp(argv[i], "-display") == 0) {
	/* just ignore and pass this and the following argument to xforms */
	i+= 2;
    }  else {
      fprintf( stderr, "Unknown command line option: %s\n", argv[i] );
      i++;
    }
  }

  /* Display help text if no poly file specified */
  if (! got_polys) {
    printHelp();
    exit(0);
  }
}

/* Print the help banner. */
void printHelp(void) {
  printf("PolyView v.0.1, Kevin Scott\n");
  printf("Usage: polyview [options]\n\n");
  printf("Options are:\n");
  printf("   -pf filename    ==>  open .poly file filename.\n"); 
  printf("   -w              ==>  display in wireframe mode (default).\n");
  printf("   -s              ==>  display in shaded mode.\n");
  printf("   -z              ==>  set the initial zoom factor [0].\n");
  printf("   -bg r g b       ==>  set background color [1 1 1].\n");
  printf("   -oc r g b       ==>  set the diffuse color [.5 .5 .5].\n");
  printf("   -q x y z r      ==>  set quaternion rotation [0 0 0 1].\n");
  printf("   -o foo.ppm      ==>  save window as PPM file then exit.\n");
  printf("   -h              ==>  display this message.\n");
}

/***************************************************************************\

  Copyright 1999 The University of Virginia.
  All Rights Reserved.

  Permission to use, copy, modify and distribute this software and its
  documentation without fee, and without a written agreement, is
  hereby granted, provided that the above copyright notice and the
  complete text of this comment appear in all copies, and provided that
  the University of Virginia and the original authors are credited in
  any publications arising from the use of this software.

  IN NO EVENT SHALL THE UNIVERSITY OF VIRGINIA
  OR THE AUTHOR OF THIS SOFTWARE BE LIABLE TO ANY PARTY FOR DIRECT,
  INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
  LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
  DOCUMENTATION, EVEN IF THE UNIVERSITY OF VIRGINIA AND/OR THE
  AUTHOR OF THIS SOFTWARE HAVE BEEN ADVISED OF THE POSSIBILITY OF 
  SUCH DAMAGES.

  The author of the vdslib software library may be contacted at:

  US Mail:             Dr. David Patrick Luebke
                       Department of Computer Science
                       Thornton Hall, University of Virginia
		       Charlottesville, VA 22903

  Phone:               (804)924-1021

  EMail:               luebke@cs.virginia.edu

\*****************************************************************************/
