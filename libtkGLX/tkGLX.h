
#ifndef __TKGLX_H_INCLUDED__
#define __TKGLX_H_INCLUDED__

extern int            TkGLXwin_RefExists(char *ref);

extern int            TkGLXwin_RefIsLinked(char *ref);

extern int            TkGLXwin_RefWindowExists(char *ref, int buf);

extern int            TkGLXwin_RefWinset(char *ref, int buf);
extern int            TkGLXwin_GetRefs(char ***refs, int *count);

extern Display       *TkGLXwin_RefGetDisplay(char *ref);
extern Window         TkGLXwin_RefGetWindow(char *ref, int buf);
extern Colormap       TkGLXwin_RefGetColormap(char *ref, int buf);
extern XVisualInfo   *TkGLXwin_RefGetVisualInfo(char *ref, int buf);
extern unsigned long  TkGLXwin_RefGetParam(char *ref, int buf, int param);

extern GLXconfig     *TkGLXwin_RefGetConfig(char *ref);

extern void          *TkGLXwin_RefGetClientData(char *ref);
extern void          *TkGLXwin_RefSetClientData(char *ref, void *clientData);

extern int            TkGLXwin_RefGetDisplayWindow(char *ref, 
						   Display **d, Window *w);

#ifdef TK_MAJOR_VERSION
/* include this only if tk.h has been included earlier (to avoid the
   need to include tk.h just to get the Tk and Tcl types) */

extern Tk_Window    TkGLXwin_RefGetTkwin  (char *ref);
extern Tcl_Interp  *TkGLXwin_RefGetInterp (char *ref);

#endif

#endif /* __TKGLX_H_INCLUDED__ */
