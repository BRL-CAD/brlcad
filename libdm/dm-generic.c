#include "conf.h"
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "tk.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "dm.h"

extern struct dm *Nu_open();
extern struct dm *plot_open();
extern struct dm *ps_open();
#ifdef DM_X
extern struct dm *X_open();
#endif
#ifdef DM_OGL
extern struct dm *ogl_open();
#endif
#ifdef DM_GLX
extern struct dm *glx_open();
#endif
#ifdef DM_PEX
extern struct dm *pex_open();
#endif

struct dm *
dm_open(type, eventHandler, argc, argv)
int type;
int (*eventHandler)();
int argc;
char *argv[];
{
  switch(type){
  case DM_TYPE_NULL:
    return Nu_open(eventHandler, argc, argv);
  case DM_TYPE_PLOT:
    return plot_open(eventHandler, argc, argv);
  case DM_TYPE_PS:
    return ps_open(eventHandler, argc, argv);
#ifdef DM_X
  case DM_TYPE_X:
    return X_open(eventHandler, argc, argv);
#endif
#ifdef DM_OGL
  case DM_TYPE_OGL:
    return ogl_open(eventHandler, argc, argv);
#endif
#ifdef DM_GLX
  case DM_TYPE_GLX:
    return glx_open(eventHandler, argc, argv);;
#endif
#ifdef DM_PEX
  case DM_TYPE_PEX:
#endif
  default:
    break;
  }
  return( (struct dm *)NULL );
}

fastf_t
dm_X2Normal(dmp, x, use_aspect)
struct dm *dmp;
register int x;
int use_aspect;
{
  if(use_aspect)
    return ((x / (fastf_t)dmp->dm_width - 0.5) / dmp->dm_aspect * 2.0);
  else
    return ((x / (fastf_t)dmp->dm_width - 0.5) * 2.0);
}

fastf_t
dm_Y2Normal(dmp, y)
struct dm *dmp;
register int y;
{
  return ((0.5 - y / (fastf_t)dmp->dm_height) * 2.0);
}
