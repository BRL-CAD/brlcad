#include "conf.h"
#include "tk.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "dm.h"

int dm_validXType();
char *dm_bestXType();

int
dm_validXType(dpy_string, name)
char *dpy_string;
char *name;
{
  Display *dpy;
  int return_val;
  int val = 0;

  if((dpy = XOpenDisplay(dpy_string)) == NULL){
    bu_log("dm_validXType: failed to open display - %s\n", dpy_string);
    return val;
  }

#ifdef DM_OGL
  if(!strcmp(name, "ogl") &&
     XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val))
      val = 1;
  else
#endif  
#ifdef DM_X
    if(!strcmp(name, "X"))
      val = 1;
#endif

  XCloseDisplay(dpy);

  return val;
}

char *
dm_bestXType(dpy_string)
char *dpy_string;
{
  Display *dpy;
  int return_val;
  char *name = (char *)NULL;

  if((dpy = XOpenDisplay(dpy_string)) == NULL){
    bu_log("dm_bestXType: failed to open display - %s\n", dpy_string);
    return name;
  }

#ifdef DM_OGL
  if(XQueryExtension(dpy, "GLX", &return_val, &return_val, &return_val))
    name = "ogl";
  else
#endif  
#ifdef DM_X
    name = "X";
#endif

  XCloseDisplay(dpy);

  return name;
}
