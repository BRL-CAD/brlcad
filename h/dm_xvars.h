#ifndef SEEN_DM_XVARS
#define SEEN_DM_XVARS

#define XVARS_MV_O(_m) offsetof(struct dm_xvars, _m)

struct dm_xvars {
  Display *dpy;
  Window win;
  Tk_Window top;
  Tk_Window xtkwin;
  int depth;
  Colormap cmap;
  XVisualInfo *vip;
  XFontStruct *fontstruct;
  int devmotionnotify;
  int devbuttonpress;
  int devbuttonrelease;
};
#endif /* SEEN_DM_XVARS */
