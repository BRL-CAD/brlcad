/*                           T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2011 United States Government as represented by
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
 * LIBFB's Tcl interface.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "bio.h"
#include "tcl.h"
#include "cmd.h"                  /* includes bu.h */
#include "fb.h"
#include "bu.h"

/* private headers */
#include "brlcad_version.h"


#define FB_TCL_CKMAG(_ptr, _magic, _str) {				\
	struct bu_vls _fb_vls;						\
									\
	if (!(_ptr)) {							\
	    bu_vls_init(&_fb_vls);					\
	    bu_vls_printf(&_fb_vls, "ERROR: null %s ptr, file %s, line %d\n", \
			  _str, __FILE__, __LINE__);			\
	    Tcl_AppendResult(interp, bu_vls_addr(&_fb_vls), (char *)NULL); \
	    bu_vls_free(&_fb_vls);					\
									\
	    return TCL_ERROR;						\
	} else if (*((uint32_t *)(_ptr)) != (_magic)) {			\
	    bu_vls_init(&_fb_vls);					\
	    bu_vls_printf(&_fb_vls, "ERROR: bad %s ptr %p, s/b x%x, was x%lx, file %s, line %d\n", \
			  _str, (void *)_ptr, _magic, (unsigned long)*((uint32_t *)(_ptr)), __FILE__, __LINE__); \
	    Tcl_AppendResult(interp, bu_vls_addr(&_fb_vls), (char *)NULL); \
	    bu_vls_free(&_fb_vls);					\
									\
	    return TCL_ERROR;						\
	}								\
    }


#define FB_TCL_CK_FBIO(_p) FB_TCL_CKMAG(_p, FB_MAGIC, "FBIO")

/* from libfb/fb_obj.c */
extern int Fbo_Init(Tcl_Interp *interp);

/* XXX -- At some point these routines should be moved to FBIO */
#ifdef IF_WGL
extern FBIO wgl_interface;
static const char *wgl_device_name = "/dev/wgl";
extern void wgl_configureWindow(FBIO *ifp, int width, int height);
extern int wgl_open_existing(FBIO *ifp, int argc, const char **argv);
extern int wgl_refresh(FBIO *ifp, int x, int y, int w, int h);
#endif

#ifdef IF_OGL
extern FBIO ogl_interface;
static const char *ogl_device_name = "/dev/ogl";
extern void ogl_configureWindow(FBIO *ifp, int width, int height);
extern int ogl_open_existing(FBIO *ifp, int argc, const char **argv);
extern int ogl_refresh(FBIO *ifp, int x, int y, int w, int h);
#endif

#ifdef IF_X
extern FBIO X24_interface;
static const char *X_device_name = "/dev/X";
extern void X24_configureWindow(FBIO *ifp, int width, int height);
extern int X24_open_existing(FBIO *ifp, int argc, const char **argv);
extern int X24_refresh(FBIO *ifp, int x, int y, int w, int h);
#endif

#ifdef IF_TK
extern FBIO tk_interface;
static const char *tk_device_name = "/dev/tk";
#if 0
/*XXX TJM implement this interface */
extern void tk_configureWindow(FBIO *ifp, int width, int height);
extern int tk_open_existing(FBIO *ifp, int argc, const char **argv);
extern int tk_refresh(FBIO *ifp, int x, int y, int w, int h);
#endif
#endif


int
fb_cmd_open_existing(void *clientData, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    register FBIO *ifp;
    struct bu_vls vls;
    int found = 0;

    if (argc < 2) {
	bu_log("fb_open_existing: wrong number of args\n");
	return TCL_ERROR;
    }

    if ((ifp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL) {
	bu_log("fb_open_existing: failed to allocate ifp memory\n");
	return TCL_ERROR;
    }

#ifdef IF_X
    if (strcasecmp(argv[1], X_device_name) == 0) {
	found=1;
	*ifp = X24_interface; /* struct copy */

	ifp->if_name = malloc((unsigned)strlen(X_device_name) + 1);
	bu_strlcpy(ifp->if_name, X_device_name, strlen(X_device_name)+1);

	/* Mark OK by filling in magic number */
	ifp->if_magic = FB_MAGIC;

	if ((X24_open_existing(ifp, argc - 1, argv + 1)) <= -1) {
	    ifp->if_magic = 0; /* sanity */
	    free((void *) ifp->if_name);
	    free((void *) ifp);
	    bu_log("fb_open_existing: failed to open X framebuffer\n");
	    return TCL_ERROR;
	}
    }
#endif  /* IF_X */

#ifdef IF_TK
#if 0
/* XXX TJM implment tk_open_existing */
    if (strcasecmp(argv[1], tk_device_name) == 0) {
	found=1;
	*ifp = tk_interface; /* struct copy */

	ifp->if_name = malloc((unsigned)strlen(tk_device_name) + 1);
	bu_strlcpy(ifp->if_name, tk_device_name, strlen(tk_device_name)+1);

	/* Mark OK by filling in magic number */
	ifp->if_magic = FB_MAGIC;

	if ((tk_open_existing(ifp, argc - 1, argv + 1)) <= -1) {
	    ifp->if_magic = 0; /* sanity */
	    free((void *) ifp->if_name);
	    free((void *) ifp);
	    bu_log("fb_open_existing: failed to open tk framebuffer\n");
	    return TCL_ERROR;
	}
    }
#endif
#endif  /* IF_TK */

#ifdef IF_WGL
    if (strcasecmp(argv[1], wgl_device_name) == 0) {
	found=1;
	*ifp = wgl_interface; /* struct copy */

	ifp->if_name = malloc((unsigned)strlen(wgl_device_name) + 1);
	bu_strlcpy(ifp->if_name, wgl_device_name, strlen(wgl_device_name)+1);

	/* Mark OK by filling in magic number */
	ifp->if_magic = FB_MAGIC;

	if ((wgl_open_existing(ifp, argc - 1, argv + 1)) <= -1) {
	    ifp->if_magic = 0; /* sanity */
	    free((void *) ifp->if_name);
	    free((void *) ifp);
	    bu_log("fb_open_existing: failed to open wgl framebuffer\n");
	    return TCL_ERROR;
	}
    }
#endif  /* IF_WGL */

#ifdef IF_OGL
    if (strcasecmp(argv[1], ogl_device_name) == 0) {
	found=1;
	*ifp = ogl_interface; /* struct copy */

	ifp->if_name = malloc((unsigned)strlen(ogl_device_name) + 1);
	bu_strlcpy(ifp->if_name, ogl_device_name, strlen(ogl_device_name)+1);

	/* Mark OK by filling in magic number */
	ifp->if_magic = FB_MAGIC;

	if ((ogl_open_existing(ifp, argc - 1, argv + 1)) <= -1) {
	    ifp->if_magic = 0; /* sanity */
	    free((void *) ifp->if_name);
	    free((void *) ifp);
	    bu_log("fb_open_existing: failed to open ogl framebuffer\n");
	    return TCL_ERROR;
	}
    }
#endif  /* IF_OGL */

    /* FIXME: a printed pointer address string is a blatent security
     * and integrity violation worst practice.  do not use, do not
     * pass go, find a better data-based approach.
     */
    if (found) {
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%p", (void *)ifp);
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
    bu_vls_strcat(&vls, "\n");
#endif  /* IF_X */
#ifdef IF_WGL
    bu_vls_strcat(&vls, wgl_device_name);
    bu_vls_strcat(&vls, "\n");
#endif  /* IF_WGL */
#ifdef IF_OGL
    bu_vls_strcat(&vls, ogl_device_name);
    bu_vls_strcat(&vls, "\n");
#endif  /* IF_OGL */
#ifdef IF_TK
    bu_vls_strcat(&vls, tk_device_name);
    bu_vls_strcat(&vls, "\n");
#endif  /* IF_TK */
    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return TCL_ERROR;
}


int
fb_cmd_close_existing(ClientData UNUSED(clientData), int argc, const char **argv)
{
    FBIO *ifp;

    if (argc != 2) {
	/* XXX need help message */
	return TCL_ERROR;
    }

    if (sscanf(argv[1], "%lu", (unsigned long *)&ifp) != 1) {
	bu_log("fb_close_existing: failed to provide ifp\n");
	return TCL_ERROR;
    }

    /* FB_TCL_CK_FBIO(ifp); */
    return fb_close_existing(ifp);
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
#ifdef IF_TK
#if 0
/* XXX TJM implement tk_configureWindow */
    if (!strncmp(ifp->if_name, tk_device_name, strlen(tk_device_name))) {
	tk_configureWindow(ifp, width, height);
    }
#endif
#endif  /* IF_TK */
}


int
fb_refresh(FBIO *ifp, int x, int y, int w, int h)
{
    int status=0;

#if 1
    if (w == 0 || h == 0)
	return TCL_OK;
#else
    if (w <= 0 || h <= 0) {
	/* nothing to refresh */
	return TCL_OK;
    }
#endif

    if (!ifp || !ifp->if_name) {
	/* unset/unknown framebuffer */
	return TCL_OK;
    }

#ifdef IF_X
    status = -1;
    if (!strncmp(ifp->if_name, X_device_name, strlen(X_device_name))) {
	status = X24_refresh(ifp, x, y, w, h);
    }
#endif /* IF_X */
#ifdef IF_WGL
    status = -1;
    if (!strncmp(ifp->if_name, wgl_device_name, strlen(wgl_device_name))) {
	status = wgl_refresh(ifp, x, y, w, h);
    }
#endif  /* IF_WGL */
#ifdef IF_OGL
    status = -1;
    if (!strncmp(ifp->if_name, ogl_device_name, strlen(ogl_device_name))) {
	status = ogl_refresh(ifp, x, y, w, h);
    }
#endif  /* IF_OGL */
#ifdef IF_TK
#if 0
/* XXX TJM implement tk_refresh */
    status = -1;
    if (!strncmp(ifp->if_name, tk_device_name, strlen(tk_device_name))) {
	status = tk_refresh(ifp, x, y, w, h);
    }
#endif
#endif  /* IF_TK */

    if (status < 0) {
	return TCL_ERROR;
    }

    return TCL_OK;
}


/**
 * Hook function wrapper to the fb_common_file_size Tcl command
 */
int
fb_cmd_common_file_size(ClientData clientData, int argc, const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    size_t width, height;
    int pixel_size = 3;

    if (argc != 2 && argc != 3) {
	bu_log("wrong #args: should be \"", argv[0], " fileName [#bytes/pixel]\"");
	return TCL_ERROR;
    }

    if (argc >= 3) {
	pixel_size = atoi(argv[2]);
    }

    if (fb_common_file_size(&width, &height, argv[1], pixel_size) > 0) {
	struct bu_vls vls;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%lu %lu", (unsigned long)width, (unsigned long)height);
	Tcl_SetObjResult(interp,
			 Tcl_NewStringObj(bu_vls_addr(&vls), bu_vls_strlen(&vls)));
	bu_vls_free(&vls);
	return TCL_OK;
    }

    /* Signal error */
    Tcl_SetResult(interp, "0 0", TCL_STATIC);
    return TCL_OK;
}


static int
wrapper_func(ClientData data, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_cmdtab *ctp = (struct bu_cmdtab *)data;;

    return ctp->ct_func(interp, argc, argv);
}


static void
register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds)
{
    struct bu_cmdtab *ctp = NULL;

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	(void)Tcl_CreateCommand(interp, ctp->ct_name, wrapper_func, (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }
}


/*
 * F B _ I N I T
 *
 * Allows LIBFB to be dynamically loade to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libfb.so"
 *
 * The name of this function is specified by TCL.
 */
int
Fb_Init(Tcl_Interp *interp)
{
    static struct bu_cmdtab cmdtab[] = {
	{"fb_open_existing",	 fb_cmd_open_existing},
	{"fb_close_existing",	 fb_cmd_close_existing},
	{"fb_common_file_size",	 fb_cmd_common_file_size},
	{(char *)0, (int (*)())0}
    };

    /* register commands */
    register_cmds(interp, cmdtab);

    /* initialize framebuffer object code */
    Fbo_Init(interp);

    Tcl_PkgProvide(interp,  "Fb", brlcad_version());

    return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
