#include "conf.h"
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#if 0
#include "tk.h"
#else
#include "tcl.h"
#include <X11/Xlib.h>
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"

int dm_validXType(char *dpy_string, char *name);
char *dm_bestXType(char *dpy_string);

int
dm_validXType(char	*dpy_string,
	      char	*name)
{
	int val = 0;
#if !defined(USE_MESA_GL) && defined(DM_OGL)
	int return_val;
#endif

#ifdef USE_MESA_GL

#ifdef DM_OGL
	if (!strcmp(name, "ogl"))
		return 1;
#endif
#ifdef DM_X
	if (!strcmp(name, "X"))
		return 1;
#endif

#else /* Here we assume the X server supports OpenGL */
	Display *dpy;

	if ((dpy = XOpenDisplay(dpy_string)) == NULL) {
		bu_log("dm_validXType: failed to open display - %s\n", dpy_string);
		return val;
	}

#ifdef DM_OGL
	if (!strcmp(name, "ogl") &&
	    XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val))
		val = 1;
	else
#endif  
#ifdef DM_X
		if (!strcmp(name, "X"))
			val = 1;
#endif

	XCloseDisplay(dpy);

#endif

  return val;
}

char *
dm_bestXType(char *dpy_string)
{
	char *name = (char *)NULL;
#if !defined(USE_MESA_GL) && defined(DM_OGL)
	int return_val;
#endif

#ifdef USE_MESA_GL

#ifdef DM_OGL
	return "ogl";
#endif
#ifdef DM_X
	return "X";
#endif

#else /* Here we assume the X server supports OpenGL */
	Display *dpy;

	if ((dpy = XOpenDisplay(dpy_string)) == NULL) {
		bu_log("dm_bestXType: failed to open display - %s\n", dpy_string);
		return name;
	}

#ifdef DM_OGL
	if (XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val))
		name = "ogl";
	else
#endif  
#ifdef DM_X
		name = "X";
#endif

	XCloseDisplay(dpy);

#endif

	return name;
}
