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
  int linewidth;
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
  int stereo_is_on;
  GLXContext glxc;
  int fontOffset;
  int ovec;		/* Old color map entry number */
  char is_direct;
  struct modifiable_ogl_vars mvars;
};

extern struct dm *Ogl_open();
extern void Ogl_configure_window_shape();
extern void Ogl_establish_zbuffer();
extern void Ogl_establish_lighting();
extern void Ogl_establish_perspective();
extern void Ogl_set_perspective();
extern void Ogl_do_fog();
extern void Ogl_do_linewidth();
#ifdef IR_KNOBS
extern int Ogl_add_tol();
extern int Ogl_irlimit();			/* provides knob dead spot */
#endif
extern int Ogl_irisX2ged();
extern int Ogl_irisY2ged();
extern struct dm dm_ogl;
extern struct ogl_vars head_ogl_vars;

#endif /* SEEN_DM_OGL */
