/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define NSLOTS		4080	/* The mostest possible - may be fewer */
#define NOISE 32		/* Size of dead spot on knob */
#define Glx_MV_O(_m) offsetof(struct modifiable_glx_vars, _m)

struct modifiable_glx_vars {
  int cueing_on;
  int zclipping_on;
  int zbuffer_on;
  int lighting_on;
  int perspective_mode;
  int dummy_perspective;
  int zbuf;
  int rgb;
  int doublebuffer;
  int min_scr_z;       /* based on getgdesc(GD_ZMIN) */
  int max_scr_z;       /* based on getgdesc(GD_ZMAX) */
  int debug;
  int linewidth;
};

struct glx_vars {
  struct bu_list l;
  struct dm_list *dm_list;
  Display *dpy;
  Window win;
  Tk_Window xtkwin;
  Visual *vis;
  Colormap cmap;
  int depth;
  unsigned int mb_mask;
#if 0
  long win_l, win_b, win_r, win_t;
#endif
  long width, height;
  int omx, omy;
  int perspective_angle;
  int devmotionnotify;
  int devbuttonpress;
  int devbuttonrelease;
  int knobs[8];
  int stereo_is_on;
  int is_gt;
  fastf_t aspect;
  void (*color_func)();
  struct modifiable_glx_vars mvars;
  genptr_t app_vars;   /* application specific variables */
/*
 * SGI Color Map table
 */
  int nslots;          /* how many we have, <= NSLOTS */
  int uslots;          /* how many actually used */
  struct rgbtab {
    unsigned char   r;
    unsigned char   g;
    unsigned char   b;
  }rgbtab[NSLOTS];
};

extern void Glx_viewchange();
extern void Glx_configure_window_shape();
extern void Glx_establish_perspective();
extern void Glx_set_perspective();
extern void Glx_establish_lighting();
extern void Glx_establish_zbuffer();
extern int Glx_irlimit();
extern int Glx_add_tol();
extern int Glx_irisX2ged();
extern int Glx_irisY2ged();
extern struct glx_vars head_glx_vars;
