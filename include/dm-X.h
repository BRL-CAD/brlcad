#ifndef SEEN_DM_X
#define SEEN_DM_X

#include "dm_color.h"
#define CMAP_BASE 40
#define CUBE_DIMENSION 6
#define NUM_PIXELS 216    /* CUBE_DIMENSION * CUBE_DIMENSION * CUBE_DIMENSION */
#define ColormapNull (Colormap *)NULL

struct x_vars {
  GC gc;
  Pixmap pix;
  mat_t xmat;
  int is_trueColor;
  unsigned long bd, bg, fg;   /* color of border, background, foreground */
  unsigned long pixels[NUM_PIXELS];
};
#endif /* SEEN_DM_X */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
