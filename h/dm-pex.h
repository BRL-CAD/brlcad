/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define	GED_TO_Xx(x)	((int)(((x)/4096.0+0.5)*((struct pex_vars *)dmp->dmr_vars)->width))
#define	GED_TO_Xy(x)	((int)((0.5-(x)/4096.0)*((struct pex_vars *)dmp->dmr_vars)->height))
#define Xx_TO_GED(x)    ((int)(((x)/(double)((struct pex_vars *)dmp->dmr_vars)->width - 0.5) * 4095))
#define Xy_TO_GED(x)    ((int)((0.5 - (x)/(double)((struct pex_vars *)dmp->dmr_vars)->height) * 4095))

#define Pex_MV_O(_m) offsetof(struct modifiable_pex_vars, _m)
#define TRY_DEPTHCUE 0

struct modifiable_pex_vars {
#if TRY_DEPTHCUE
  int cue;
#endif
  int perspective_mode;
  int dummy_perspective;
};

struct pex_vars {
  struct bu_list l;
  Display *dpy;
  Tk_Window xtkwin;
  Window win;
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  Pixmap pix;
  int pix_width, pix_height;
#endif
  PEXRenderer renderer;
  PEXRendererAttributes rattrs;
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
  void (*color_func)();
  struct modifiable_pex_vars mvars;
  genptr_t app_vars;   /* application specific variables */
};

extern struct pex_vars head_pex_vars;
extern void Pex_configure_window_shape();
extern void Pex_establish_perspective();
extern void Pex_set_perspective();
