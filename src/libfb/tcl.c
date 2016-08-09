/*                           T C L . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2016 United States Government as represented by
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
#include <string.h>
#include <ctype.h>

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "tcl.h"
#include "bu/cmd.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "fb_private.h"
#include "fb.h"

/* private headers */
#include "brlcad_version.h"

/* from libfb/fb_obj.c */
extern int Fbo_Init(Tcl_Interp *interp);

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
