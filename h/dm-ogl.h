#ifndef SEEN_DM_OGL
#define SEEN_DM_OGL

#include "dm_color.h"
#define CMAP_BASE 40

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define Ogl_MV_O(_m) offsetof(struct modifiable_ogl_vars, _m)

struct modifiable_ogl_vars {
  int cueing_on;
  int zclipping_on;
  int zbuffer_on;
  int lighting_on;
  int fastfog;
  double fogdensity;
  int zbuf;
  int rgb;
  int doublebuffer;
  int depth;
  int debug;
  double bound;
  int boundFlag;
};

#ifndef WIN32
struct ogl_vars {
  GLXContext glxc;
  GLdouble faceplate_mat[16];
  int face_flag;
  int *perspective_mode;
  int fontOffset;
  int ovec;		/* Old color map entry number */
  char is_direct;
  GLclampf r, g, b;
  struct modifiable_ogl_vars mvars;
};
#else
struct ogl_vars {
  HGLRC glxc;
  GLdouble faceplate_mat[16];
  int face_flag;
  int *perspective_mode;
  int fontOffset;
  int ovec;		/* Old color map entry number */
  char is_direct;
  GLclampf r, g, b;
  struct modifiable_ogl_vars mvars;
};
#endif

extern void ogl_fogHint();

#endif /* SEEN_DM_OGL */
