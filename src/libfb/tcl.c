/*                           T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libfb */
/** @{ */
/** @file ./libfb/tcl.c
 *
 *  LIBFB's Tcl interface.
 *
 *  Authors -
 *	Robert G. Parker
 *
 *	Source -
 *		The U. S. Army Research Laboratory
 *		Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "tcl.h"
#include "machine.h"
#include "cmd.h"                  /* includes bu.h */
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
		bu_vls_printf(&_fb_vls, "ERROR: bad %s ptr x%lx, s/b x%x, was x%lx, file %s, line %d\n", \
		_str, (long)_ptr, _magic, (long)*((long *)(_ptr)), __FILE__, __LINE__); \
		Tcl_AppendResult(interp, bu_vls_addr(&_fb_vls), (char *)NULL); \
		bu_vls_free(&_fb_vls); \
\
		return TCL_ERROR; \
	} \
}

#define FB_TCL_CK_FBIO(_p) FB_TCL_CKMAG(_p, FB_MAGIC, "FBIO")

/* from libfb/fb_obj.c */
extern int Fbo_Init(Tcl_Interp *interp);

/* XXX -- At some point these routines should be moved to FBIO */
#ifdef IF_WGL
extern int wgl_open_existing();
extern int wgl_close_existing();
extern FBIO wgl_interface;
extern void wgl_configureWindow();
extern int wgl_refresh();
#endif

#ifdef IF_OGL
extern int ogl_open_existing();
extern int ogl_close_existing();
extern FBIO ogl_interface;
extern void ogl_configureWindow();
extern int ogl_refresh();
#endif

#ifdef IF_X
extern void X24_configureWindow();
extern int X24_refresh();
extern int X24_open_existing();
extern int X24_close_existing();
extern FBIO X24_interface;
#endif

int fb_cmd_open_existing(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int fb_cmd_close_existing(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);
int fb_cmd_common_file_size(ClientData clientData, Tcl_Interp *interp, int argc, char **argv);

static struct bu_cmdtab cmdtab[] = {
	{"fb_open_existing",	 fb_cmd_open_existing},
	{"fb_close_existing",	 fb_cmd_close_existing},
	{"fb_common_file_size",	 fb_cmd_common_file_size},
	{(char *)0, (int (*)())0}
};

/* XXX this device list shouldn't be in here */
static const char *X_device_name = "/dev/X";
static const char *ogl_device_name = "/dev/ogl";
static const char *wgl_device_name = "/dev/wgl";


int
fb_cmd_open_existing(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    register FBIO *ifp;
    struct bu_vls vls;
    int found = 0;

    if(argc < 2){
	Tcl_AppendResult(interp, "XXXfb_open_existing: wrong number of args\n", (char *)NULL);
	return TCL_ERROR;
    }

    if((ifp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL){
	Tcl_AppendResult(interp, "fb_open_existing: failed to allocate ifp memory\n", (char *)NULL);
	return TCL_ERROR;
    }

#ifdef IF_X
    if(strcasecmp(argv[1], X_device_name) == 0) {
	found=1;
	*ifp = X24_interface; /* struct copy */

	ifp->if_name = malloc((unsigned)strlen(X_device_name) + 1);
	(void)strcpy(ifp->if_name, X_device_name);

	/* Mark OK by filling in magic number */
	ifp->if_magic = FB_MAGIC;

	if((X24_open_existing(ifp, argc - 1, argv + 1)) <= -1){
	    ifp->if_magic = 0; /* sanity */
	    free((void *) ifp->if_name);
	    free((void *) ifp);
	    Tcl_AppendResult(interp, "fb_open_existing: failed to open X framebuffer\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }
#endif  /* IF_X */

#ifdef IF_WGL
    if(strcasecmp(argv[1], wgl_device_name) == 0) {
	found=1;
	*ifp = wgl_interface; /* struct copy */

	ifp->if_name = malloc((unsigned)strlen(wgl_device_name) + 1);
	(void)strcpy(ifp->if_name, wgl_device_name);

	/* Mark OK by filling in magic number */
	ifp->if_magic = FB_MAGIC;

	if((wgl_open_existing(ifp, argc - 1, argv + 1)) <= -1){
	    ifp->if_magic = 0; /* sanity */
	    free((void *) ifp->if_name);
	    free((void *) ifp);
	    Tcl_AppendResult(interp, "fb_open_existing: failed to open wgl framebuffer\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }
#endif  /* IF_WGL */

#ifdef IF_OGL
    if(strcasecmp(argv[1], ogl_device_name) == 0) {
	found=1;
	*ifp = ogl_interface; /* struct copy */

	ifp->if_name = malloc((unsigned)strlen(ogl_device_name) + 1);
	(void)strcpy(ifp->if_name, ogl_device_name);

	/* Mark OK by filling in magic number */
	ifp->if_magic = FB_MAGIC;

	if((ogl_open_existing(ifp, argc - 1, argv + 1)) <= -1){
	    ifp->if_magic = 0; /* sanity */
	    free((void *) ifp->if_name);
	    free((void *) ifp);
	    Tcl_AppendResult(interp, "fb_open_existing: failed to open ogl framebuffer\n", (char *)NULL);
	    return TCL_ERROR;
	}
    }
#endif  /* IF_OGL */

    if (found) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%lu", (unsigned long)ifp);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    ifp->if_magic = 0; /* sanity */
    free((void *) ifp->if_name);
    free((void *) ifp);

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "fb_open_existing: supports only the following device types\n");
#ifdef IF_X
    bu_vls_strcat(&vls, X_device_name);
    bu_vls_strcat( &vls, "\n" );
#endif  /* IF_X */
#ifdef IF_WGL
    bu_vls_strcat(&vls, wgl_device_name);
    bu_vls_strcat( &vls, "\n" );
#endif  /* IF_WGL */
#ifdef IF_OGL
    bu_vls_strcat(&vls, ogl_device_name);
    bu_vls_strcat( &vls, "\n" );
#endif  /* IF_OGL */
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_ERROR;
}

int
fb_cmd_close_existing(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FBIO *ifp;
    struct bu_vls vls;
    int status;

    if(argc != 2){
	/* XXX need help message */
	return TCL_ERROR;
    }

    if(sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1){
	Tcl_AppendResult(interp, "fb_close_existing: failed to provide ifp\n", (char *)NULL);
	return TCL_ERROR;
    }

    FB_TCL_CK_FBIO(ifp);
    _fb_pgflush(ifp);
#ifdef IF_X
    if(strcasecmp(ifp->if_name, X_device_name) == 0) {
	if((status = X24_close_existing(ifp)) <= -1){
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "fb_close_existing: can not close device \"%s\", ret=%d.\n",
			  ifp->if_name, status);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    return TCL_ERROR;
	}
	if(ifp->if_pbase != PIXEL_NULL) {
	    free((void *)ifp->if_pbase);
	}
	free((void *)ifp->if_name);
	free((void *)ifp);
	return TCL_OK;
    }
#endif  /* IF_X */

#ifdef IF_WGL
    if(strcasecmp(ifp->if_name, wgl_device_name) == 0) {
	if((status = wgl_close_existing(ifp)) <= -1){
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "fb_close_existing: can not close device \"%s\", ret=%d.\n",
			  ifp->if_name, status);
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
#endif  /* IF_WGL */

#ifdef IF_OGL
    if(strcasecmp(ifp->if_name, ogl_device_name) == 0) {
	if((status = ogl_close_existing(ifp)) <= -1){
	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "fb_close_existing: can not close device \"%s\", ret=%d.\n",
			  ifp->if_name, status);
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
#endif  /* IF_OGL */

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "fb_close_existing: can not close device\nifp: %s    device name: %s\n",
		  argv[1], ifp->if_name);
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_ERROR;
}

void
fb_configureWindow(FBIO *ifp, int width, int height)
{
#ifdef IF_X
    if (!strncmp(ifp->if_name, X_device_name, strlen(X_device_name))) {
	X24_configureWindow(ifp, width, height);
    }
#endif /* IF_X */
#ifdef IF_WGL
    if (!strncmp(ifp->if_name, wgl_device_name, strlen(wgl_device_name))) {
	wgl_configureWindow(ifp, width, height);
    }
#endif  /* IF_WGL */
#ifdef IF_OGL
    if (!strncmp(ifp->if_name, ogl_device_name, strlen(ogl_device_name))) {
	ogl_configureWindow(ifp, width, height);
    }
#endif  /* IF_OGL */
}

int
fb_refresh(FBIO *ifp, int x, int y, int w, int h)
{
    int status=0;

    if (w <= 0 || h <= 0) {
	/* nothing to refresh */
	return TCL_OK;
    }

    if (!ifp || !ifp->if_name) {
	/* unset/unknown framebuffer */
	return TCL_OK;
    }

#ifdef IF_X
    status = -1;
    if(!strncmp(ifp->if_name, X_device_name, strlen( X_device_name))) {
	status = X24_refresh(ifp, x, y, w, h);
    }
#endif /* IF_X */
#ifdef IF_WGL
    status = -1;
    if(!strncmp(ifp->if_name, wgl_device_name, strlen( wgl_device_name))) {
	status = wgl_refresh(ifp, x, y, w, h);
    }
#endif  /* IF_WGL */
#ifdef IF_OGL
    status = -1;
    if(!strncmp(ifp->if_name, ogl_device_name, strlen( ogl_device_name))) {
	status = ogl_refresh(ifp, x, y, w, h);
    }
#endif  /* IF_OGL */

    if(status < 0) {
	return TCL_ERROR;
    }

    return TCL_OK;
}


/** fb_cmd_common_file_size
 *
 * Hook function wrapper to the fb_common_file_size Tcl command
 */
int
fb_cmd_common_file_size(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    unsigned long int width, height;
    int pixel_size = 3;

    if (argc != 2 && argc != 3) {
	Tcl_AppendResult(interp, "wrong #args: should be \"", argv[0], " fileName [#bytes/pixel]\"", NULL);
	return TCL_ERROR;
    }

    if (argc >= 3) {
	pixel_size = atoi(argv[2]);
    }

    if (fb_common_file_size(&width, &height, argv[1], pixel_size) > 0) {
	sprintf(interp->result, "%lu %lu", width, height);
	return TCL_OK;
    }

    /* Signal error */
    Tcl_SetResult(interp, "0 0", TCL_STATIC);
    return TCL_OK;
}


/*
 *			F B _ I N I T
 *
 *  Allows LIBFB to be dynamically loade to a vanilla tclsh/wish with
 *  "load /usr/brlcad/lib/libfb.so"
 *
 *  The name of this function is specified by TCL.
 */
int
#ifdef BRLCAD_DEBUG
Fb_d_Init(Tcl_Interp *interp)
#else
Fb_Init(Tcl_Interp *interp)
#endif
{
	/* register commands */
	bu_register_cmds(interp, cmdtab);

	/* initialize framebuffer object code */
	Fbo_Init(interp);

	Tcl_PkgProvide(interp,  "Fb", BRLCAD_VERSION);

	return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
