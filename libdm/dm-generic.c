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
extern void X_configureWindowShape();
#endif
#ifdef DM_OGL
extern struct dm *ogl_open();
extern void ogl_configureWindowShape();
extern void ogl_zbuffer();
extern void ogl_lighting();
#endif
#ifdef DM_GLX
extern struct dm *glx_open();
extern void glx_configureWindowShape();
extern void glx_lighting();
extern void glx_zbuffer();
#endif
#ifdef DM_PEX
extern struct dm *pex_open();
#endif

struct dm *
dm_open(type, argc, argv)
int type;
int argc;
char *argv[];
{
  switch(type){
  case DM_TYPE_NULL:
    return Nu_open(argc, argv);
  case DM_TYPE_PLOT:
    return plot_open(argc, argv);
  case DM_TYPE_PS:
    return ps_open(argc, argv);
#ifdef DM_X
  case DM_TYPE_X:
    return X_open(argc, argv);
#endif
#ifdef DM_OGL
  case DM_TYPE_OGL:
    return ogl_open(argc, argv);
#endif
#ifdef DM_GLX
  case DM_TYPE_GLX:
    return glx_open(argc, argv);;
#endif
#ifdef DM_PEX
  case DM_TYPE_PEX:
#endif
  default:
    break;
  }
  return( (struct dm *)NULL );
}

/*
 * Provides a way to (un)share display lists. If dmp2 is
 * NULL, then dmp1 will no longer share its display lists.
 */
int
dm_share_dlist(dmp1, dmp2)
struct dm *dmp1;
struct dm *dmp2;
{
  if(dmp1 == (struct dm *)NULL)
    return TCL_ERROR;

  /*
   * Only display managers of the same type and using the 
   * same OGL server are allowed to share display lists.
   *
   * XXX - need a better way to check if using the same OGL server.
   */
  if(dmp2 != (struct dm *)NULL)
    if(dmp1->dm_type != dmp2->dm_type ||
       strcmp(bu_vls_addr(&dmp1->dm_dName), bu_vls_addr(&dmp2->dm_dName)))
      return TCL_ERROR;

  switch(dmp1->dm_type){
#ifdef DM_OGL
  case DM_TYPE_OGL:
    return ogl_share_dlist(dmp1, dmp2);
#endif
  default:
    return TCL_ERROR;
  }
}

fastf_t
dm_Xx2Normal(dmp, x)
struct dm *dmp;
register int x;
{
  return ((x / (fastf_t)dmp->dm_width - 0.5) * 2.0);
}

int
dm_Normal2Xx(dmp, f)
struct dm *dmp;
register fastf_t f;
{
  return (f * 0.5 + 0.5) * dmp->dm_width;
}

fastf_t
dm_Xy2Normal(dmp, y, use_aspect)
struct dm *dmp;
register int y;
int use_aspect;
{
  if(use_aspect)
    return ((0.5 - y / (fastf_t)dmp->dm_height) / dmp->dm_aspect * 2.0);
  else
    return ((0.5 - y / (fastf_t)dmp->dm_height) * 2.0);
}

int
dm_Normal2Xy(dmp, f, use_aspect)
struct dm *dmp;
register fastf_t f;
int use_aspect;
{
  if(use_aspect)
    return (0.5 - f * 0.5 * dmp->dm_aspect) * dmp->dm_height;
  else
    return (0.5 - f * 0.5) * dmp->dm_height;
}

void
dm_configureWindowShape(dmp)
struct dm *dmp;
{
  switch(dmp->dm_type){
#ifdef DM_X
  case DM_TYPE_X:
    X_configureWindowShape(dmp);
    return;
#endif
#ifdef DM_OGL
  case DM_TYPE_OGL:
    ogl_configureWindowShape(dmp);
    return;
#endif
#ifdef DM_GLX
  case DM_TYPE_GLX:
    glx_configureWindowShape(dmp);
    return;
#endif
#ifdef DM_PEX
  case DM_TYPE_PEX:
    Pex_configureWindowShape(dmp);
    return;
#endif
  case DM_TYPE_PLOT:
  case DM_TYPE_PS:
  case DM_TYPE_NULL:
  default:
    return;
  }
}

void
dm_zbuffer(dmp)
struct dm *dmp;
{
  switch(dmp->dm_type){
#ifdef DM_OGL
  case DM_TYPE_OGL:
    ogl_zbuffer(dmp);
    return;
#endif
#ifdef DM_GLX
  case DM_TYPE_GLX:
   glx_zbuffer(dmp);
    return;
#endif
#ifdef DM_X
  case DM_TYPE_X:
#endif
#ifdef DM_PEX
  case DM_TYPE_PEX:
#endif
  case DM_TYPE_PLOT:
  case DM_TYPE_PS:
  case DM_TYPE_NULL:
  default:
    return;
  }
}

void
dm_lighting(dmp)
struct dm *dmp;
{
  switch(dmp->dm_type){
#ifdef DM_OGL
  case DM_TYPE_OGL:
    ogl_lighting(dmp);
    return;
#endif
#ifdef DM_GLX
  case DM_TYPE_GLX:
    glx_lighting(dmp);
    return;
#endif
#ifdef DM_X
  case DM_TYPE_X:
#endif
#ifdef DM_PEX
  case DM_TYPE_PEX:
#endif
  case DM_TYPE_PLOT:
  case DM_TYPE_PS:
  case DM_TYPE_NULL:
  default:
    return;
  }
}
