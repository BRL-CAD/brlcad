#ifndef SEEN_DM_X
#define SEEN_DM_X

#include "dm_color.h"
#define CMAP_BASE 40
#define CUBE_DIMENSION 6
#define NUM_PIXELS 216    /* CUBE_DIMENSION * CUBE_DIMENSION * CUBE_DIMENSION */
#define ColormapNull (Colormap *)NULL

#define X_MV_O(_m) offsetof(struct modifiable_x_vars, _m)

struct modifiable_x_vars {
  int zclip;
  int debug;
};

struct x_vars {
  GC gc;
  Pixmap pix;
  int is_trueColor;
  unsigned long bd, bg, fg;   /* color of border, background, foreground */
  unsigned long pixels[NUM_PIXELS];
  vect_t clipmin;
  vect_t clipmax;
  fastf_t r, g, b;
  struct modifiable_x_vars mvars;
};
#endif /* SEEN_DM_X */
