/*                           T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2014 United States Government as represented by
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
#include <string.h>
#include <ctype.h>

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "bio.h"
#include "tcl.h"
#include "bu/cmd.h"
#include "fb.h"
#include "bu.h"

/* private headers */
#include "brlcad_version.h"


#define FB_TCL_CKMAG(_ptr, _magic, _str) {				\
	struct bu_vls _fb_vls = BU_VLS_INIT_ZERO;			\
									\
	if (!(_ptr)) {							\
	    bu_vls_printf(&_fb_vls, "ERROR: null %s ptr, file %s, line %d\n", \
			  _str, __FILE__, __LINE__);			\
	    Tcl_AppendResult(interp, bu_vls_addr(&_fb_vls), (char *)NULL); \
	    bu_vls_free(&_fb_vls);					\
									\
	    return TCL_ERROR;						\
	} else if (*((uint32_t *)(_ptr)) != (_magic)) {			\
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

#ifdef IF_OSG
extern FBIO osg_interface;
static const char *osg_device_name = "/dev/osg";
extern void osg_configureWindow(FBIO *ifp, int width, int height);
extern int osg_open_existing(FBIO *ifp, int argc, const char **argv);
extern int osg_refresh(FBIO *ifp, int x, int y, int w, int h);
#endif

int
#if !defined(IF_X) && !defined(IF_WGL) && !defined(IF_OGL) && !defined(IF_OSG) && !defined(IF_TK)
fb_cmd_open_existing(void *clientData, int argc, const char **UNUSED(argv))
#else
fb_cmd_open_existing(void *clientData, int argc, const char **argv)
#endif
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    register FBIO *ifp;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int found = 0;

    if (argc < 2) {
	bu_log("fb_open_existing: wrong number of args\n");
	return TCL_ERROR;
    }

    if ((ifp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL) {
	bu_log("fb_open_existing: failed to allocate ifp memory\n");
	return TCL_ERROR;
    }
#ifdef IF_OSG
    if (BU_STR_EQUIV(argv[1], osg_device_name)) {
	found=1;
	*ifp = osg_interface; /* struct copy */

	ifp->if_name = (char *)malloc((unsigned)strlen(osg_device_name) + 1);
	bu_strlcpy(ifp->if_name, osg_device_name, strlen(osg_device_name)+1);

	/* Mark OK by filling in magic number */
	ifp->if_magic = FB_MAGIC;

	if ((osg_open_existing(ifp, argc - 1, argv + 1)) <= -1) {
	    ifp->if_magic = 0; /* sanity */
	    free((void *) ifp->if_name);
	    free((void *) ifp);
	    bu_log("fb_open_existing: failed to open osg framebuffer\n");
	    return TCL_ERROR;
	}
    }
#endif  /* IF_OSG */


    /* FIXME: a printed pointer address string is a blatant security
     * and integrity violation worst practice.  do not use, do not
     * pass go, find a better data-based approach.
     */
    if (found) {
	bu_vls_printf(&vls, "%p", (void *)ifp);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	ifp->if_magic = 0; /* sanity */
	free((void *) ifp->if_name);
	free((void *) ifp);
	return TCL_OK;
    }

    ifp->if_magic = 0; /* sanity */
    free((void *) ifp->if_name);
    free((void *) ifp);

    bu_vls_printf(&vls, "fb_open_existing: supports only the following device types\n");
#ifdef IF_OSG
    bu_vls_strcat(&vls, osg_device_name);
    bu_vls_strcat(&vls, "\n");
#endif  /* IF_OSG */
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
    /* unknown/unset framebuffer */
    if (!ifp || !ifp->if_name || width < 0 || height < 0) {
	return;
    }
#ifdef IF_OSG
    if (!bu_strncmp(ifp->if_name, osg_device_name, strlen(osg_device_name))) {
	osg_configureWindow(ifp, width, height);
    }
#endif  /* IF_OSG */
}


int
fb_refresh(FBIO *ifp, int x, int y, int w, int h)
{
    int status=0;

    /* what does negative mean? */
    if (x < 0)
	x = 0;
    if (y < 0)
	y = 0;
    if (w < 0)
	w = 0;
    if (h < 0)
	h = 0;

    if (w == 0 || h == 0) {
	/* nothing to refresh */
	return TCL_OK;
    }

    if (!ifp || !ifp->if_name) {
	/* unset/unknown framebuffer */
	return TCL_OK;
    }

#ifdef IF_OSG
    status = -1;
    if (!bu_strncmp(ifp->if_name, osg_device_name, strlen(osg_device_name))) {
	status = osg_refresh(ifp, x, y, w, h);
    }
#endif  /* IF_OSG */

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
	bu_log("wrong #args: should be \" fileName [#bytes/pixel]\"");
	return TCL_ERROR;
    }

    if (argc >= 3) {
	pixel_size = atoi(argv[2]);
    }

    if (fb_common_file_size(&width, &height, argv[1], pixel_size) > 0) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
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
    struct bu_cmdtab *ctp = (struct bu_cmdtab *)data;

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
 * Allows LIBFB to be dynamically loaded to a vanilla tclsh/wish with
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
	{(const char *)NULL, BU_CMD_NULL}
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
