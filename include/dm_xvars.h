#ifndef SEEN_DM_XVARS
#define SEEN_DM_XVARS

#define XVARS_MV_O(_m) offsetof(struct dm_xvars, _m)

#ifdef WIN32
struct dm_xvars {
  HDC  hdc;      // device context of device that OpenGL calls are to be drawn on
  Display *dpy;
  Window win;
  Tk_Window top;
  Tk_Window xtkwin;
  int depth;
  Colormap cmap;
  PIXELFORMATDESCRIPTOR *vip;
  HFONT fontstruct;
  int devmotionnotify;
  int devbuttonpress;
  int devbuttonrelease;
};
#else
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
#endif
#endif /* SEEN_DM_XVARS */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
