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
  int perspective_mode;
  int dummy_perspective;
  int fastfog;
  double fogdensity;
  int zbuf;
  int rgb;
  int doublebuffer;
  int depth;
  int debug;
};

struct ogl_vars {
  struct bu_list l;
  Display *dpy;
  Window win;
  Tk_Window top;
  Tk_Window xtkwin;
  int omx, omy;
  unsigned int mb_mask;
  int perspective_angle;
  XFontStruct *fontstruct;
  Colormap cmap;
  GLdouble faceplate_mat[16];
  int face_flag;
  int devmotionnotify;
  int devbuttonpress;
  int devbuttonrelease;
  int knobs[8];
  GLXContext glxc;
  int fontOffset;
  int ovec;		/* Old color map entry number */
  char is_direct;
  struct modifiable_ogl_vars mvars;
};

extern void ogl_configure_window_shape();
extern void ogl_establish_zbuffer();
extern void ogl_establish_lighting();
extern void ogl_establish_perspective();
extern void ogl_set_perspective();
extern void ogl_do_fog();
extern struct ogl_vars head_ogl_vars;

#endif /* SEEN_DM_OGL */
