#ifndef SEEN_DM_X
#define SEEN_DM_X

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define	GED_TO_Xx(dmp, x) ((int)(((x)/4096.0+0.5)*((struct x_vars *)((dmp)->dmr_vars))->width))
#define	GED_TO_Xy(dmp, x) ((int)((0.5-(x)/4096.0)*((struct x_vars *)((dmp)->dmr_vars))->height))
#define Xx_TO_GED(dmp, x) ((int)(((x)/(double)((struct x_vars *)((dmp)->dmr_vars))->width - 0.5) * 4095))
#define Xy_TO_GED(dmp, x) ((int)((0.5 - (x)/(double)((struct x_vars *)((dmp)->dmr_vars))->height) * 4095))

#define TRY_COLOR_CUBE 1
#define NUM_PIXELS  216
#define X_MV_O(_m) offsetof(struct modifiable_x_vars, _m)

struct modifiable_x_vars {
  int perspective_mode;
  int dummy_perspective;
  int debug;
};

struct x_vars {
  struct bu_list l;
  Display *dpy;
  Tk_Window xtkwin;
  Window win;
  Pixmap pix;
  unsigned int mb_mask;
  int width;
  int height;
  int omx, omy;
  int perspective_angle;
  GC gc;
  XFontStruct *fontstruct;
  int is_monochrome;
  unsigned long black,gray,white,yellow,red,blue;
  unsigned long bd, bg, fg;   /* color of border, background, foreground */
#if TRY_COLOR_CUBE
  Colormap cmap;
  unsigned long   pixel[NUM_PIXELS];
#endif
  struct modifiable_x_vars mvars;
};

extern struct x_vars head_x_vars;
extern void X_configure_window_shape();
extern void X_establish_perspective();
extern void X_set_perspective();

#endif /* SEEN_DM_X */
