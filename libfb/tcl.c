/*
 *                      T C L . C
 *
 *  LIBFB's Tcl interface.
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
#include "cmd.h"                  /* includes bu.h */
#include "fb.h"

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

/* from libfb/fb_obj.c */
extern int Fbo_Init();

/*XXX At some point these routines should be moved to FBIO */
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

int fb_tcl_open_existing();
int fb_tcl_close_existing();

static struct bu_cmdtab cmdtab[] = {
	{"fb_open_existing",	 fb_tcl_open_existing},
	{"fb_close_existing",	 fb_tcl_close_existing},
	{(char *)0, (int (*)())0}
};

int
Fb_Init(interp)
     Tcl_Interp *interp;
{
	char *version_number;

	/* register commands */
	bu_register_cmds(interp, cmdtab);

	/* initialize framebuffer object code */
	Fbo_Init(interp);

	Tcl_SetVar(interp, "fb_version", (char *)fb_version+5, TCL_GLOBAL_ONLY);
	Tcl_Eval(interp, "lindex $fb_version 2");
	version_number = Tcl_GetStringResult(interp);
	Tcl_PkgProvide(interp,  "Fb", version_number);

	return TCL_OK;
}

int
fb_tcl_open_existing(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char **argv;
{
#ifdef IF_X
	register FBIO *ifp;
	char *X_name = "/dev/X";
#ifdef IF_OGL
	char *ogl_name = "/dev/ogl";
#endif
	struct bu_vls vls;

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

		if((X24_open_existing(ifp, argc - 1, argv + 1)) <= -1){
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

		if((ogl_open_existing(ifp, argc - 1, argv + 1)) <= -1){
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
#endif /* IF_X */

	return TCL_OK;
}

int
fb_tcl_close_existing(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char **argv;
{
#ifdef IF_X
	FBIO *ifp;
#ifdef IF_OGL
	char *ogl_name = "/dev/ogl";
#endif
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
		bu_vls_printf(&vls, "fb_close_existing: can not close device\nifp: %s    device name: %s\n",
			      argv[1], ifp->if_name);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	if(ifp->if_pbase != PIXEL_NULL)
		free((void *)ifp->if_pbase);
	free((void *)ifp->if_name);
	free((void *)ifp);
#endif /* IF_X */

	return TCL_OK;
}

void
fb_configureWindow(ifp, width, height)
     FBIO *ifp;
     int width, height;
{
#ifdef IF_X
	const char *X_name = "/dev/X";
#ifdef IF_OGL
	const char *ogl_name = "/dev/ogl";
#endif

	if (!strncmp(ifp->if_name, X_name, strlen(X_name)))
		X24_configureWindow(ifp, width, height);
#ifdef IF_OGL
	else if (!strncmp(ifp->if_name, ogl_name, strlen(ogl_name)))
		ogl_configureWindow(ifp, width, height);
#endif
#endif /* IF_X */
}

int
fb_refresh(ifp, x, y, w, h)
     FBIO *ifp;
     int x, y;
     int w, h;
{
#ifdef IF_X
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
#endif /* IF_X */

	return TCL_OK;
}
