#ifndef SEEN_DM_X
#define SEEN_DM_X

#include "dm_color.h"
#define CMAP_BASE 40
#define CUBE_DIMENSION 6
#define NUM_PIXELS 216    /* CUBE_DIMENSION * CUBE_DIMENSION * CUBE_DIMENSION */

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define	GED_TO_Xx(_dmp, x) ((int)(((x)/4095.0+0.5)*_dmp->dm_width))
#define	GED_TO_Xy(_dmp, x) ((int)((0.5-(x)/4095.0)*_dmp->dm_height))
#define Xx_TO_GED(_dmp, x) ((int)(((x)/(double)_dmp->dm_width - 0.5) * 4095))
#define Xy_TO_GED(_dmp, x) ((int)((0.5 - (x)/(double)_dmp->dm_height) * 4095))

#define X_MV_O(_m) offsetof(struct modifiable_x_vars, _m)

struct modifiable_x_vars {
  int linewidth;
  int linestyle;
  int perspective_mode;
  int dummy_perspective;
  int debug;
};

struct x_vars {
  struct bu_list l;
  Display *dpy;
  Window win;
  Tk_Window top;
  Tk_Window xtkwin;
  int depth;
  int omx, omy;
  unsigned int mb_mask;
  int perspective_angle;
  XFontStruct *fontstruct;
  GC gc;
  Pixmap pix;
  int is_monochrome;
  unsigned long black,gray,white,yellow,red,blue;
  unsigned long bd, bg, fg;   /* color of border, background, foreground */
  Colormap cmap;
  unsigned long pixels[NUM_PIXELS];
  struct modifiable_x_vars mvars;
};

extern void X_configure_window_shape();
extern void X_establish_perspective();
extern void X_set_perspective();
extern int X_drawString2D();
extern struct x_vars head_x_vars;

#endif /* SEEN_DM_X */
