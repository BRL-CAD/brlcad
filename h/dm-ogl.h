#ifndef SEEN_DM_OGL
#define SEEN_DM_OGL

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define	GED_TO_Xx(x)	(((x)/4096.0+0.5)*((struct ogl_vars *)dm_vars)->width)
#define	GED_TO_Xy(x)	((0.5-(x)/4096.0)*((struct ogl_vars *)dm_vars)->height)

/* Map +/-2048 GED space into -1.0..+1.0 :: x/2048*/
#define GED2IRIS(x)	(((float)(x))*0.00048828125)

#define NOISE 32		/* Size of dead spot on knob */
#define NSLOTS		4080	/* The mostest possible - may be fewer */
#define Ogl_MV_O(_m) offsetof(struct modifiable_ogl_vars, _m)

struct modifiable_ogl_vars {
  int cueing_on;
  int zclipping_on;
  int zbuffer_on;
  int lighting_on;
  int perspective_mode;
  int dummy_perspective;
  int zbuf;
  int rgb;
  int doublebuffer;
  int depth;
  int debug;
  int linewidth;
  int fastfog;
  double fogdensity;
};

struct ogl_vars {
  struct bu_list l;
  Display *dpy;
  Window win;
  Tk_Window xtkwin;
  Colormap cmap;
  GLdouble faceplate_mat[16];
  unsigned int mb_mask;
  int face_flag;
  int width;
  int height;
  int omx, omy;
  int perspective_angle;
  int devmotionnotify;
  int devbuttonpress;
  int devbuttonrelease;
  int knobs[8];
  int stereo_is_on;
  fastf_t aspect;
  GLXContext glxc;
  XFontStruct *fontstruct;
  int fontOffset;
  int ovec;		/* Old color map entry number */
  char is_direct;
  int index_size;
  fastf_t *viewscale;
  void (*color_func)();
  struct modifiable_ogl_vars mvars;
  genptr_t app_vars;   /* application specific variables */
/*
 * SGI Color Map table
 */
  int nslots;		/* how many we have, <= NSLOTS */
  int uslots;		/* how many actually used */
  struct rgbtab {
	unsigned char	r;
	unsigned char	g;
	unsigned char	b;
  }rgbtab[NSLOTS];
};

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
extern struct ogl_vars head_ogl_vars;

#endif /* SEEN_DM_OGL */
