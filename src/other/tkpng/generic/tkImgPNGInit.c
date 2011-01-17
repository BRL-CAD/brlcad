/*
 * tkImgPNGInit.c --
 *
 *		This file initializes a package implementing a PNG photo image
 *      type for Tcl/Tk.  See the file tkImgPNG.c for the actual
 *      implementation.
 *
 * Copyright (c) 2006 Muonics, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include <tcl.h>
#include <tk.h>

#ifndef	PACKAGE_VERSION
#define	PACKAGE_VERSION "0.9"
#endif

extern Tk_PhotoImageFormat tkImgFmtPNG;

#ifndef USE_PANIC_ON_PHOTO_ALLOC_FAILURE
#if ((TCL_MAJOR_VERSION > 8) || \
	((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 5)))
#define	TKPNG_REQUIRE "8.5"
#else
#define	TKPNG_REQUIRE "8.3"
#endif
#endif /* !USE_PANIC_ON_PHOTO_ALLOC_FAILURE */

/*
 *----------------------------------------------------------------------
 *
 * Tkpng_Init --
 *
 *		Initialize the Tcl PNG package.
 *
 * Results:
 *		A standard Tcl result
 *
 * Side effects:
 *		PNG support added to the "photo" image type.
 *
 *----------------------------------------------------------------------
 */

#ifdef __cplusplus
extern "C"
{
#endif
DLLEXPORT
int
Tkpng_Init(Tcl_Interp *interp)
{
	if (Tcl_InitStubs(interp, TKPNG_REQUIRE, 0) == NULL) {
		return TCL_ERROR;
	}
	if (Tcl_PkgRequire(interp, "Tcl", TKPNG_REQUIRE, 0) == NULL) {
		return TCL_ERROR;
	}
	if (Tk_InitStubs(interp, TKPNG_REQUIRE, 0) == NULL) {
		return TCL_ERROR;
	}
	if (Tcl_PkgRequire(interp, "Tk", TKPNG_REQUIRE, 0) == NULL) {
		return TCL_ERROR;
	}

	Tk_CreatePhotoImageFormat(&tkImgFmtPNG);

	if (Tcl_PkgProvide(interp, "tkpng", PACKAGE_VERSION) != TCL_OK) {
		return TCL_ERROR;
	}

	return TCL_OK;
}
#ifdef __cplusplus
extern "C"
}
#endif
