/*
 *                      T C L . C
 *
 *  Tcl interface to LIBFB routines.
 *
 *  Authors -
 *	Robert G. Parker
 *
 *	Source -
 *		The U. S. Army Research Laboratory
 *		Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1997 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "fb.h"
#include "bu.h"

#define FB_TCL_CKMAG(_ptr, _magic, _str){ \
	struct bu_vls _fb_vls; \
\
	if(!(_ptr)){ \
		bu_vls_init(&_fb_vls); \
		bu_vls_printf(&_fb_vls, "ERROR: null %s ptr, file %s, line %d\n", \
			_str, __FILE__, __LINE__ ); \
		Tcl_AppendResult(interp, bu_vls_addr(&_fb_vls), (char *)NULL); \
		bu_vls_free(&_fb_vls); \
\
		return TCL_ERROR; \
	}else if(*((long *)(_ptr)) != (_magic)){ \
		bu_vls_init(&_fb_vls); \
		bu_vls_printf(&_fb_vls, "ERROR: bad %s ptr x%x, s/b x%x, was x%x, file %s, line %d\n", \
		_str, _ptr, _magic, *((long *)(_ptr)), __FILE__, __LINE__); \
		Tcl_AppendResult(interp, bu_vls_addr(&_fb_vls), (char *)NULL); \
		bu_vls_free(&_fb_vls); \
\
		return TCL_ERROR; \
	} \
}

#define FB_TCL_CK_FBIO(_p) FB_TCL_CKMAG(_p, FB_MAGIC, "FBIO")

/*XXX At some point these routines should be moved to FBIO */
#ifdef IF_OGL
extern int ogl_refresh();
extern int ogl_open_existing();
extern int ogl_close_existing();
extern FBIO ogl_interface;
#endif

extern int X24_refresh();
extern int X24_open_existing();
extern int X24_close_existing();
extern FBIO X24_interface;

int fb_tcl_open();
int fb_tcl_close();
int fb_tcl_open_existing();
int fb_tcl_close_existing();
int fb_tcl_clear();
int fb_tcl_cursor();
int fb_tcl_getcursor();
int fb_tcl_writerect();
int fb_tcl_refresh();
int fb_refresh();

static struct fbcmd{
  char *cmdName;
  int (*cmdFunc)();
}fb_cmds[] = {
  "fb_open", fb_tcl_open,
  "fb_close", fb_tcl_close,
  "fb_open_existing", fb_tcl_open_existing,
  "fb_close_existing", fb_tcl_close_existing,
  "fb_clear", fb_tcl_clear,
  "fb_cursor", fb_tcl_cursor,
  "fb_getcursor", fb_tcl_getcursor,
  "fb_writerect", fb_tcl_writerect,
  "fb_refresh", fb_tcl_refresh,
  (char *)0, (int (*)())0
};

void
fb_tclInit(interp)
Tcl_Interp *interp;
{
  struct fbcmd *fbp;

  for(fbp = fb_cmds; fbp->cmdName != (char *)0; ++fbp){
    (void)Tcl_CreateCommand(interp, fbp->cmdName, fbp->cmdFunc,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
  }
}

int
fb_tcl_open(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int width, height;
  struct bu_vls vls;

  if(argc != 4){
    /*XXX put error message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[2], "%d", &width) != 1)
    return TCL_ERROR;

  if(sscanf(argv[3], "%d", &height) != 1)
    return TCL_ERROR;
  
  ifp = fb_open(argv[1], width, height);
  bu_vls_init(&vls);
  bu_vls_printf(&vls, "%lu", (unsigned long)ifp);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}

int
fb_tcl_clear(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  int status;
  FBIO *ifp;
  unsigned char pp[4];
  unsigned char *ms;
  int r, g, b;

  if(argc < 2){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);

  if(argc == 5){
    if(sscanf(argv[2], "%d", &r) == 1 &&
       sscanf(argv[3], "%d", &g) == 1 &&
	 sscanf(argv[4], "%d", &b) == 1){
      ms = pp;
      pp[0] = (unsigned char)r;
      pp[1] = (unsigned char)g;
      pp[2] = (unsigned char)b;
      pp[3] = (unsigned char)0;
    }else
      ms = RGBPIXEL_NULL;
  }else
    ms = RGBPIXEL_NULL;

  status = fb_clear(ifp, ms);

  if(status < 0)
    return TCL_ERROR;

  return TCL_OK;
}

int
fb_tcl_cursor(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int mode;
  int x, y;
  int status;

  if(argc != 5){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }
  FB_TCL_CK_FBIO(ifp);

  if(sscanf(argv[2], "%d", &mode) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[3], "%d", &x) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[4], "%d", &y) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  status = fb_cursor(ifp, mode, x, y);
  if(status == 0)
    return TCL_OK;

  return TCL_ERROR;
}

int
fb_tcl_getcursor(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int status;
  int mode;
  int x, y;

  if(argc != 2){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);

  status = fb_getcursor(ifp, &mode, &x, &y);
  if(status == 0){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d %d %d", mode, x, y);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_OK;
  }

  return TCL_ERROR;
}

#if 0
int
fb_tcl_(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int status;

  if(argc != ){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);
}
int
fb_tcl_(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int status;

  if(argc != ){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);
}
int
fb_tcl_(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int status;

  if(argc != ){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);
}
int
fb_tcl_(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int status;

  if(argc != ){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);
}
int
fb_tcl_(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int status;

  if(argc != ){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);
}
#endif

int
fb_tcl_close(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  struct fbcmd *fbp;
  FBIO *ifp;
  int status;

  if(argc != 2){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);
  status = fb_close(ifp);

  if(status < 0)
    return TCL_ERROR;

#if 0
  for(fbp = fb_cmds; fbp->cmdName != (char *)0; ++fbp){
    Tcl_DeleteCommand(interp, fbp->cmdName);
  }
#endif

  return TCL_OK;
}

/*XXX Experimenting */
int
fb_tcl_open_existing(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  register FBIO *ifp;
  char *X_name = "/dev/X";
#ifdef IF_OGL
  char *ogl_name = "/dev/ogl";
#endif
  struct bu_vls vls;
  int status;

  if(argc < 2){
    Tcl_AppendResult(interp, "XXXfb_open_existing: wrong number of args\n", (char *)NULL);
    return TCL_ERROR;
  }

  if((ifp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL){
    Tcl_AppendResult(interp, "fb_open_existing: failed to allocate ifp memory\n",
		     (char *)NULL);
    return TCL_ERROR;
  }

  if(strcmp(argv[1], X_name) == 0){
    *ifp = X24_interface; /* struct copy */

    ifp->if_name = malloc((unsigned)strlen(X_name) + 1);
    (void)strcpy(ifp->if_name, X_name);

    /* Mark OK by filling in magic number */
    ifp->if_magic = FB_MAGIC;

    if((status = X24_open_existing(ifp, argc - 1, argv + 1)) <= -1){
      ifp->if_magic = 0; /* sanity */
      free((void *) ifp->if_name);
      free((void *) ifp);
      Tcl_AppendResult(interp, "fb_open_existing: failed to open X framebuffer\n",
		       (char *)NULL);
      return TCL_ERROR;
    }
#ifdef IF_OGL
  }else if(strcmp(argv[1], ogl_name) == 0){
    *ifp = ogl_interface; /* struct copy */

    ifp->if_name = malloc((unsigned)strlen(ogl_name) + 1);
    (void)strcpy(ifp->if_name, ogl_name);

    /* Mark OK by filling in magic number */
    ifp->if_magic = FB_MAGIC;

    if((status = ogl_open_existing(ifp, argc - 1, argv + 1)) <= -1){
      ifp->if_magic = 0; /* sanity */
      free((void *) ifp->if_name);
      free((void *) ifp);
      Tcl_AppendResult(interp, "fb_open_existing: failed to open ogl framebuffer\n",
		       (char *)NULL);
      return TCL_ERROR;
    }
#endif
  }else{
    ifp->if_magic = 0; /* sanity */
    free((void *) ifp->if_name);
    free((void *) ifp);

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "fb_open_existing: supports only the following device types\n");
    bu_vls_printf(&vls, "%s", X_name);
#ifdef IF_OGL
    bu_vls_printf(&vls, ", %s", ogl_name);
#endif
    bu_vls_printf(&vls, "\n");
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  bu_vls_init(&vls);
  bu_vls_printf(&vls, "%lu", (unsigned long)ifp);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}

int
fb_tcl_close_existing(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  char *ogl_name = "/dev/ogl";
  char *X_name = "/dev/X";
  struct bu_vls vls;
  int status;

  if(argc != 2){
    /*XXX need help message */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    Tcl_AppendResult(interp, "fb_close_existing: failed to provide ifp\n", (char *)NULL);
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);
  _fb_pgflush(ifp);

  if(strcmp(ifp->if_name, X_name) == 0){
    if((status = X24_close_existing(ifp)) <= -1){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "fb_close_existing: can not close device \"%s\", ret=%d.\n",
		    ifp->if_name, status);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }
#ifdef IF_OGL
  }else if(strcmp(ifp->if_name, ogl_name) == 0){
    if((status = ogl_close_existing(ifp)) <= -1){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "fb_close_existing: can not close device \"%s\", ret=%d.\n",
		    ifp->if_name, status);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }
#endif
  }else{
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "fb_close_existing: can not close device\n   ifp: %s    device name: %s\n",
		  argv[1], ifp->if_name);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_ERROR;
  }

  if(ifp->if_pbase != PIXEL_NULL)
    free((void *)ifp->if_pbase);
  free((void *)ifp->if_name);
  free((void *)ifp);

  return TCL_OK;
}

int
fb_tcl_writerect(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int x, y, w, h;
  unsigned char *pp;
  int status;

  if(argc != 7){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);

  if(sscanf(argv[2], "%d", &x) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[3], "%d", &y) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[4], "%d", &w) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[5], "%d", &h) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[6], "%lu", (unsigned long *)&pp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  status = fb_writerect(ifp, x, y, w, h, pp);

  if(status < 0)
    return TCL_ERROR;

  return TCL_OK;
}

int
fb_tcl_refresh(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
  FBIO *ifp;
  int x, y, w, h; /* rectangle to be refreshed */

  if(argc != 2 && argc != 6){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
    /*XXX put usage message here */
    return TCL_ERROR;
  }

  FB_TCL_CK_FBIO(ifp);

  if(argc == 2){  /* refresh the whole display */
    x = y = 0;
    w = ifp->if_width;
    h = ifp->if_height;
  }else{  /* refresh only the given rectangle of the display */
    if(sscanf(argv[2], "%d", &x) != 1){
      /*XXX put usage message here */
      return TCL_ERROR;
    }

    if(sscanf(argv[3], "%d", &y) != 1){
      /*XXX put usage message here */
      return TCL_ERROR;
    }

    if(sscanf(argv[4], "%d", &w) != 1){
      /*XXX put usage message here */
      return TCL_ERROR;
    }

    if(sscanf(argv[5], "%d", &h) != 1){
      /*XXX put usage message here */
      return TCL_ERROR;
    }
  }

  return fb_refresh(ifp, x, y, w, h);
}

int
fb_refresh(ifp, x, y, w, h)
FBIO *ifp;
int x, y;
int w, h;
{
  char *X_name = "/dev/X";
#ifdef IF_OGL
  char *ogl_name = "/dev/ogl";
#endif
  int status;

  if(!strcmp(ifp->if_name, X_name)){
    status = X24_refresh(ifp, x, y, w, h);
  }
#ifdef IF_OGL
  else if(!strcmp(ifp->if_name, ogl_name)){
    status = ogl_refresh(ifp, x, y, w, h);
  }
#endif
  else{
    return TCL_ERROR;
  }

  if(status < 0)
    return TCL_ERROR;

  return TCL_OK;
}

void
fb_configureWindow(ifp, width, height)
FBIO *ifp;
int width, height;
{
  char *X_name = "/dev/X";
#ifdef IF_OGL
  char *ogl_name = "/dev/ogl";
#endif

  if(!strcmp(ifp->if_name, X_name))
    X24_configureWindow(ifp, width, height);
#ifdef IF_OGL
  else if(!strcmp(ifp->if_name, ogl_name))
    ogl_configureWindow(ifp, width, height);
#endif
}

