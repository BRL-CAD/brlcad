#ifndef SEEN_DM_GLX
#define SEEN_DM_GLX

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define Glx_MV_O(_m) offsetof(struct modifiable_glx_vars, _m)

struct modifiable_glx_vars {
  int cueing_on;
  int zclipping_on;
  int zbuffer_on;
  int lighting_on;
  int perspective_mode;
  int dummy_perspective;
  int debug;
  int linewidth;
  int zbuf;
  int rgb;
  int doublebuffer;
  int min_scr_z;       /* based on getgdesc(GD_ZMIN) */
  int max_scr_z;       /* based on getgdesc(GD_ZMAX) */
};

struct glx_vars {
  struct bu_list l;
  Display *dpy;
  Window win;
  Tk_Window top;
  Tk_Window xtkwin;
  int omx, omy;
  unsigned int mb_mask;
  Colormap cmap;
  Visual *vis;
  int depth;
  int perspective_angle;
  int devmotionnotify;
  int devbuttonpress;
  int devbuttonrelease;
  int knobs[8];
  int stereo_is_on;
  int is_gt;
  struct modifiable_glx_vars mvars;
};

extern void glx_configure_window_shape();
extern void glx_establish_perspective();
extern void glx_set_perspective();
extern void glx_establish_lighting();
extern void glx_establish_zbuffer();
extern void glx_clear_to_black();
extern int glx_irisX2ged();
extern int glx_irisY2ged();
extern struct glx_vars head_glx_vars;

#endif /* SEEN_DM_GLX */
