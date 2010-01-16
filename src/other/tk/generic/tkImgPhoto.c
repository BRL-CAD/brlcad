/*
 * tkImgPhoto.c --
 *
 *	Implements images of type "photo" for Tk. Photo images are stored in
 *	full color (32 bits per pixel including alpha channel) and displayed
 *	using dithering if necessary.
 *
 * Copyright (c) 1994 The Australian National University.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2002-2003 Donal K. Fellows
 * Copyright (c) 2003 ActiveState Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Author: Paul Mackerras (paulus@cs.anu.edu.au),
 *	   Department of Computer Science,
 *	   Australian National University.
 *
 * RCS: @(#) $Id$
 */

#include "tkImgPhoto.h"
#include <ctype.h>

/*
 * The following data structure is used to return information from
 * ParseSubcommandOptions:
 */

struct SubcommandOptions {
    int options;		/* Individual bits indicate which options were
				 * specified - see below. */
    Tcl_Obj *name;		/* Name specified without an option. */
    int fromX, fromY;		/* Values specified for -from option. */
    int fromX2, fromY2;		/* Second coordinate pair for -from option. */
    int toX, toY;		/* Values specified for -to option. */
    int toX2, toY2;		/* Second coordinate pair for -to option. */
    int zoomX, zoomY;		/* Values specified for -zoom option. */
    int subsampleX, subsampleY;	/* Values specified for -subsample option. */
    Tcl_Obj *format;		/* Value specified for -format option. */
    XColor *background;		/* Value specified for -background option. */
    int compositingRule;	/* Value specified for -compositingrule
				 * option. */
};

/*
 * Bit definitions for use with ParseSubcommandOptions: each bit is set in the
 * allowedOptions parameter on a call to ParseSubcommandOptions if that option
 * is allowed for the current photo image subcommand. On return, the bit is
 * set in the options field of the SubcommandOptions structure if that option
 * was specified.
 *
 * OPT_BACKGROUND:		Set if -format option allowed/specified.
 * OPT_COMPOSITE:		Set if -compositingrule option allowed/spec'd.
 * OPT_FORMAT:			Set if -format option allowed/specified.
 * OPT_FROM:			Set if -from option allowed/specified.
 * OPT_GRAYSCALE:		Set if -grayscale option allowed/specified.
 * OPT_SHRINK:			Set if -shrink option allowed/specified.
 * OPT_SUBSAMPLE:		Set if -subsample option allowed/spec'd.
 * OPT_TO:			Set if -to option allowed/specified.
 * OPT_ZOOM:			Set if -zoom option allowed/specified.
 */

#define OPT_BACKGROUND	1
#define OPT_COMPOSITE	2
#define OPT_FORMAT	4
#define OPT_FROM	8
#define OPT_GRAYSCALE	0x10
#define OPT_SHRINK	0x20
#define OPT_SUBSAMPLE	0x40
#define OPT_TO		0x80
#define OPT_ZOOM	0x100

/*
 * List of option names. The order here must match the order of declarations
 * of the OPT_* constants above.
 */

static const char *const optionNames[] = {
    "-background",
    "-compositingrule",
    "-format",
    "-from",
    "-grayscale",
    "-shrink",
    "-subsample",
    "-to",
    "-zoom",
    NULL
};

/*
 * Message to generate when an attempt to resize an image fails due to memory
 * problems.
 */

#define TK_PHOTO_ALLOC_FAILURE_MESSAGE \
	"not enough free memory for image buffer"

/*
 * Functions used in the type record for photo images.
 */

static int		ImgPhotoCreate(Tcl_Interp *interp, const char *name,
			    int objc, Tcl_Obj *const objv[],
			    const Tk_ImageType *typePtr, Tk_ImageMaster master,
			    ClientData *clientDataPtr);
static void		ImgPhotoDelete(ClientData clientData);
static int		ImgPhotoPostscript(ClientData clientData,
			    Tcl_Interp *interp, Tk_Window tkwin,
			    Tk_PostscriptInfo psInfo, int x, int y, int width,
			    int height, int prepass);

/*
 * The type record itself for photo images:
 */

Tk_ImageType tkPhotoImageType = {
    "photo",			/* name */
    ImgPhotoCreate,		/* createProc */
    TkImgPhotoGet,		/* getProc */
    TkImgPhotoDisplay,		/* displayProc */
    TkImgPhotoFree,		/* freeProc */
    ImgPhotoDelete,		/* deleteProc */
    ImgPhotoPostscript,		/* postscriptProc */
    NULL			/* nextPtr */
};

typedef struct ThreadSpecificData {
    Tk_PhotoImageFormat *formatList;
				/* Pointer to the first in the list of known
				 * photo image formats.*/
    Tk_PhotoImageFormat *oldFormatList;
				/* Pointer to the first in the list of known
				 * photo image formats.*/
    int initialized;		/* Set to 1 if we've initialized the
				 * structure. */
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

/*
 * Default configuration
 */

#define DEF_PHOTO_GAMMA		"1"
#define DEF_PHOTO_HEIGHT	"0"
#define DEF_PHOTO_PALETTE	""
#define DEF_PHOTO_WIDTH		"0"

/*
 * Information used for parsing configuration specifications:
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_STRING, "-file", NULL, NULL,
	 NULL, Tk_Offset(PhotoMaster, fileString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_DOUBLE, "-gamma", NULL, NULL,
	 DEF_PHOTO_GAMMA, Tk_Offset(PhotoMaster, gamma), 0},
    {TK_CONFIG_INT, "-height", NULL, NULL,
	 DEF_PHOTO_HEIGHT, Tk_Offset(PhotoMaster, userHeight), 0},
    {TK_CONFIG_UID, "-palette", NULL, NULL,
	 DEF_PHOTO_PALETTE, Tk_Offset(PhotoMaster, palette), 0},
    {TK_CONFIG_INT, "-width", NULL, NULL,
	 DEF_PHOTO_WIDTH, Tk_Offset(PhotoMaster, userWidth), 0},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/*
 * Forward declarations
 */

static void		PhotoFormatThreadExitProc(ClientData clientData);
static int		ImgPhotoCmd(ClientData clientData, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static int		ParseSubcommandOptions(
			    struct SubcommandOptions *optPtr,
			    Tcl_Interp *interp, int allowedOptions,
			    int *indexPtr, int objc, Tcl_Obj *const objv[]);
static void		ImgPhotoCmdDeletedProc(ClientData clientData);
static int		ImgPhotoConfigureMaster(Tcl_Interp *interp,
			    PhotoMaster *masterPtr, int objc,
			    Tcl_Obj *const objv[], int flags);
static int		ToggleComplexAlphaIfNeeded(PhotoMaster *mPtr);
static int		ImgPhotoSetSize(PhotoMaster *masterPtr, int width,
			    int height);
static int		ImgStringWrite(Tcl_Interp *interp,
			    Tcl_Obj *formatString,
			    Tk_PhotoImageBlock *blockPtr);
static char *		ImgGetPhoto(PhotoMaster *masterPtr,
			    Tk_PhotoImageBlock *blockPtr,
			    struct SubcommandOptions *optPtr);
static int		MatchFileFormat(Tcl_Interp *interp, Tcl_Channel chan,
			    const char *fileName, Tcl_Obj *formatString,
			    Tk_PhotoImageFormat **imageFormatPtr,
			    int *widthPtr, int *heightPtr, int *oldformat);
static int		MatchStringFormat(Tcl_Interp *interp, Tcl_Obj *data,
			    Tcl_Obj *formatString,
			    Tk_PhotoImageFormat **imageFormatPtr,
			    int *widthPtr, int *heightPtr, int *oldformat);
static Tcl_ObjCmdProc *	PhotoOptionFind(Tcl_Interp *interp, Tcl_Obj *obj);
static void		PhotoOptionCleanupProc(ClientData clientData,
			    Tcl_Interp *interp);

/*
 *----------------------------------------------------------------------
 *
 * PhotoFormatThreadExitProc --
 *
 *	Clean up the registered list of photo formats.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The thread's linked lists of photo image formats is deleted.
 *
 *----------------------------------------------------------------------
 */

static void
PhotoFormatThreadExitProc(
    ClientData clientData)	/* not used */
{
    Tk_PhotoImageFormat *freePtr;
    ThreadSpecificData *tsdPtr =
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    while (tsdPtr->oldFormatList != NULL) {
	freePtr = tsdPtr->oldFormatList;
	tsdPtr->oldFormatList = tsdPtr->oldFormatList->nextPtr;
	ckfree((char *) freePtr);
    }
    while (tsdPtr->formatList != NULL) {
	freePtr = tsdPtr->formatList;
	tsdPtr->formatList = tsdPtr->formatList->nextPtr;
	ckfree((char *) freePtr->name);
	ckfree((char *) freePtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CreateOldPhotoImageFormat, Tk_CreatePhotoImageFormat --
 *
 *	This function is invoked by an image file handler to register a new
 *	photo image format and the functions that handle the new format. The
 *	function is typically invoked during Tcl_AppInit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The new image file format is entered into a table used in the photo
 *	image "read" and "write" subcommands.
 *
 *----------------------------------------------------------------------
 */

void
Tk_CreateOldPhotoImageFormat(
    const Tk_PhotoImageFormat *formatPtr)
				/* Structure describing the format. All of the
				 * fields except "nextPtr" must be filled in
				 * by caller. */
{
    Tk_PhotoImageFormat *copyPtr;
    ThreadSpecificData *tsdPtr =
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (!tsdPtr->initialized) {
	tsdPtr->initialized = 1;
	Tcl_CreateThreadExitHandler(PhotoFormatThreadExitProc, NULL);
    }
    copyPtr = (Tk_PhotoImageFormat *) ckalloc(sizeof(Tk_PhotoImageFormat));
    *copyPtr = *formatPtr;
    copyPtr->nextPtr = tsdPtr->oldFormatList;
    tsdPtr->oldFormatList = copyPtr;
}

void
Tk_CreatePhotoImageFormat(
    const Tk_PhotoImageFormat *formatPtr)
				/* Structure describing the format. All of the
				 * fields except "nextPtr" must be filled in
				 * by caller. */
{
    Tk_PhotoImageFormat *copyPtr;
    ThreadSpecificData *tsdPtr =
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (!tsdPtr->initialized) {
	tsdPtr->initialized = 1;
	Tcl_CreateThreadExitHandler(PhotoFormatThreadExitProc, NULL);
    }
    copyPtr = (Tk_PhotoImageFormat *) ckalloc(sizeof(Tk_PhotoImageFormat));
    *copyPtr = *formatPtr;
    if (isupper((unsigned char) *formatPtr->name)) {
	copyPtr->nextPtr = tsdPtr->oldFormatList;
	tsdPtr->oldFormatList = copyPtr;
    } else {
	/* for compatibility with aMSN: make a copy of formatPtr->name */
	char *name = ckalloc(strlen(formatPtr->name) + 1);
	strcpy(name, formatPtr->name);
	copyPtr->name = name;
	copyPtr->nextPtr = tsdPtr->formatList;
	tsdPtr->formatList = copyPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoCreate --
 *
 *	This function is called by the Tk image code to create a new photo
 *	image.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The data structure for a new photo image is allocated and initialized.
 *
 *----------------------------------------------------------------------
 */

static int
ImgPhotoCreate(
    Tcl_Interp *interp,		/* Interpreter for application containing
				 * image. */
    const char *name,		/* Name to use for image. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[],	/* Argument objects for options (doesn't
				 * include image name or type). */
    const Tk_ImageType *typePtr,/* Pointer to our type record (not used). */
    Tk_ImageMaster master,	/* Token for image, to be used by us in later
				 * callbacks. */
    ClientData *clientDataPtr)	/* Store manager's token for image here; it
				 * will be returned in later callbacks. */
{
    PhotoMaster *masterPtr;

    /*
     * Allocate and initialize the photo image master record.
     */

    masterPtr = (PhotoMaster *) ckalloc(sizeof(PhotoMaster));
    memset(masterPtr, 0, sizeof(PhotoMaster));
    masterPtr->tkMaster = master;
    masterPtr->interp = interp;
    masterPtr->imageCmd = Tcl_CreateObjCommand(interp, name, ImgPhotoCmd,
	    masterPtr, ImgPhotoCmdDeletedProc);
    masterPtr->palette = NULL;
    masterPtr->pix32 = NULL;
    masterPtr->instancePtr = NULL;
    masterPtr->validRegion = TkCreateRegion();

    /*
     * Process configuration options given in the image create command.
     */

    if (ImgPhotoConfigureMaster(interp, masterPtr, objc, objv, 0) != TCL_OK) {
	ImgPhotoDelete(masterPtr);
	return TCL_ERROR;
    }

    *clientDataPtr = masterPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoCmd --
 *
 *	This function is invoked to process the Tcl command that corresponds
 *	to a photo image. See the user documentation for details on what it
 *	does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ImgPhotoCmd(
    ClientData clientData,	/* Information about photo master. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    static const char *const photoOptions[] = {
	"blank", "cget", "configure", "copy", "data", "get", "put",
	"read", "redither", "transparency", "write", NULL
    };
    enum PhotoOptions {
	PHOTO_BLANK, PHOTO_CGET, PHOTO_CONFIGURE, PHOTO_COPY, PHOTO_DATA,
	PHOTO_GET, PHOTO_PUT, PHOTO_READ, PHOTO_REDITHER, PHOTO_TRANS,
	PHOTO_WRITE
    };

    PhotoMaster *masterPtr = clientData;
    int result, index, x, y, width, height, dataWidth, dataHeight, listObjc;
    struct SubcommandOptions options;
    Tcl_Obj **listObjv, **srcObjv;
    unsigned char *pixelPtr;
    Tk_PhotoImageBlock block;
    Tk_Window tkwin;
    Tk_PhotoImageFormat *imageFormat;
    int imageWidth, imageHeight, matched, length, oldformat = 0;
    Tcl_Channel chan;
    Tk_PhotoHandle srcHandle;
    ThreadSpecificData *tsdPtr =
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], photoOptions, "option", 0,
	    &index) != TCL_OK) {
	Tcl_ObjCmdProc *proc = PhotoOptionFind(interp, objv[1]);

	if (proc == NULL) {
	    return TCL_ERROR;
	}
	return proc(clientData, interp, objc, objv);
    }

    switch ((enum PhotoOptions) index) {
    case PHOTO_BLANK:
	/*
	 * photo blank command - just call Tk_PhotoBlank.
	 */

	if (objc == 2) {
	    Tk_PhotoBlank(masterPtr);
	    return TCL_OK;
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    return TCL_ERROR;
	}

    case PHOTO_CGET: {
	const char *arg;

	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "option");
	    return TCL_ERROR;
	}
	arg = Tcl_GetStringFromObj(objv[2], &length);
	if (strncmp(arg,"-data", (unsigned) length) == 0) {
	    if (masterPtr->dataString) {
		Tcl_SetObjResult(interp, masterPtr->dataString);
	    }
	} else if (strncmp(arg,"-format", (unsigned) length) == 0) {
	    if (masterPtr->format) {
		Tcl_SetObjResult(interp, masterPtr->format);
	    }
	} else {
	    Tk_ConfigureValue(interp, Tk_MainWindow(interp), configSpecs,
		    (char *) masterPtr, Tcl_GetString(objv[2]), 0);
	}
	return TCL_OK;
    }

    case PHOTO_CONFIGURE:
	/*
	 * photo configure command - handle this in the standard way.
	 */

	if (objc == 2) {
	    Tcl_Obj *obj, *subobj;

	    result = Tk_ConfigureInfo(interp, Tk_MainWindow(interp),
		    configSpecs, (char *) masterPtr, NULL, 0);
	    if (result != TCL_OK) {
		return result;
	    }
	    obj = Tcl_NewObj();
	    subobj = Tcl_NewStringObj("-data {} {} {}", 14);
	    if (masterPtr->dataString) {
		Tcl_ListObjAppendElement(NULL, subobj, masterPtr->dataString);
	    } else {
		Tcl_AppendStringsToObj(subobj, " {}", NULL);
	    }
	    Tcl_ListObjAppendElement(interp, obj, subobj);
	    subobj = Tcl_NewStringObj("-format {} {} {}", 16);
	    if (masterPtr->format) {
		Tcl_ListObjAppendElement(NULL, subobj, masterPtr->format);
	    } else {
		Tcl_AppendStringsToObj(subobj, " {}", NULL);
	    }
	    Tcl_ListObjAppendElement(interp, obj, subobj);
	    Tcl_ListObjAppendList(interp, obj, Tcl_GetObjResult(interp));
	    Tcl_SetObjResult(interp, obj);
	    return TCL_OK;

	} else if (objc == 3) {
	    const char *arg = Tcl_GetStringFromObj(objv[2], &length);

	    if (length > 1 && !strncmp(arg, "-data", (unsigned) length)) {
		Tcl_AppendResult(interp, "-data {} {} {}", NULL);
		if (masterPtr->dataString) {
		    /*
		     * TODO: Modifying result is bad!
		     */

		    Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp),
			    masterPtr->dataString);
		} else {
		    Tcl_AppendResult(interp, " {}", NULL);
		}
		return TCL_OK;
	    } else if (length > 1 &&
		    !strncmp(arg, "-format", (unsigned) length)) {
		Tcl_AppendResult(interp, "-format {} {} {}", NULL);
		if (masterPtr->format) {
		    /*
		     * TODO: Modifying result is bad!
		     */

		    Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp),
			    masterPtr->format);
		} else {
		    Tcl_AppendResult(interp, " {}", NULL);
		}
		return TCL_OK;
	    } else {
		return Tk_ConfigureInfo(interp, Tk_MainWindow(interp),
			configSpecs, (char *) masterPtr, arg, 0);
	    }
	} else {
	    return ImgPhotoConfigureMaster(interp, masterPtr, objc-2, objv+2,
		    TK_CONFIG_ARGV_ONLY);
	}

    case PHOTO_COPY:
	/*
	 * photo copy command - first parse options.
	 */

	index = 2;
	memset(&options, 0, sizeof(options));
	options.zoomX = options.zoomY = 1;
	options.subsampleX = options.subsampleY = 1;
	options.name = NULL;
	options.compositingRule = TK_PHOTO_COMPOSITE_OVERLAY;
	if (ParseSubcommandOptions(&options, interp,
		OPT_FROM | OPT_TO | OPT_ZOOM | OPT_SUBSAMPLE | OPT_SHRINK |
		OPT_COMPOSITE, &index, objc, objv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (options.name == NULL || index < objc) {
	    Tcl_WrongNumArgs(interp, 2, objv,
		    "source-image ?-compositingrule rule? ?-from x1 y1 x2 y2? ?-to x1 y1 x2 y2? ?-zoom x y? ?-subsample x y?");
	    return TCL_ERROR;
	}

	/*
	 * Look for the source image and get a pointer to its image data.
	 * Check the values given for the -from option.
	 */

	srcHandle = Tk_FindPhoto(interp, Tcl_GetString(options.name));
	if (srcHandle == NULL) {
	    Tcl_AppendResult(interp, "image \"",
		    Tcl_GetString(options.name), "\" doesn't",
		    " exist or is not a photo image", NULL);
	    return TCL_ERROR;
	}
	Tk_PhotoGetImage(srcHandle, &block);
	if ((options.fromX2 > block.width) || (options.fromY2 > block.height)
		|| (options.fromX2 > block.width)
		|| (options.fromY2 > block.height)) {
	    Tcl_AppendResult(interp, "coordinates for -from option extend ",
		    "outside source image", NULL);
	    return TCL_ERROR;
	}

	/*
	 * Hack to pass through the message that the place we're coming from
	 * has a simple alpha channel.
	 */

	if (!(((PhotoMaster *) srcHandle)->flags & COMPLEX_ALPHA)) {
	    options.compositingRule |= SOURCE_IS_SIMPLE_ALPHA_PHOTO;
	}

	/*
	 * Fill in default values for unspecified parameters.
	 */

	if (!(options.options & OPT_FROM) || (options.fromX2 < 0)) {
	    options.fromX2 = block.width;
	    options.fromY2 = block.height;
	}
	if (!(options.options & OPT_TO) || (options.toX2 < 0)) {
	    width = options.fromX2 - options.fromX;
	    if (options.subsampleX > 0) {
		width = (width + options.subsampleX - 1) / options.subsampleX;
	    } else if (options.subsampleX == 0) {
		width = 0;
	    } else {
		width = (width - options.subsampleX - 1) / -options.subsampleX;
	    }
	    options.toX2 = options.toX + width * options.zoomX;

	    height = options.fromY2 - options.fromY;
	    if (options.subsampleY > 0) {
		height = (height + options.subsampleY - 1)
			/ options.subsampleY;
	    } else if (options.subsampleY == 0) {
		height = 0;
	    } else {
		height = (height - options.subsampleY - 1)
			/ -options.subsampleY;
	    }
	    options.toY2 = options.toY + height * options.zoomY;
	}

	/*
	 * Set the destination image size if the -shrink option was specified.
	 */

	if (options.options & OPT_SHRINK) {
	    if (ImgPhotoSetSize(masterPtr, options.toX2,
		    options.toY2) != TCL_OK) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, TK_PHOTO_ALLOC_FAILURE_MESSAGE, NULL);
		return TCL_ERROR;
	    }
	}

	/*
	 * Copy the image data over using Tk_PhotoPutZoomedBlock.
	 */

	block.pixelPtr += options.fromX * block.pixelSize
		+ options.fromY * block.pitch;
	block.width = options.fromX2 - options.fromX;
	block.height = options.fromY2 - options.fromY;
	return Tk_PhotoPutZoomedBlock(interp, (Tk_PhotoHandle) masterPtr,
		&block, options.toX, options.toY, options.toX2 - options.toX,
		options.toY2 - options.toY, options.zoomX, options.zoomY,
		options.subsampleX, options.subsampleY,
		options.compositingRule);

    case PHOTO_DATA: {
	char *data;

	/*
	 * photo data command - first parse and check any options given.
	 */

	Tk_ImageStringWriteProc *stringWriteProc = NULL;

	index = 2;
	memset(&options, 0, sizeof(options));
	options.name = NULL;
	options.format = NULL;
	options.fromX = 0;
	options.fromY = 0;
	if (ParseSubcommandOptions(&options, interp,
		OPT_FORMAT | OPT_FROM | OPT_GRAYSCALE | OPT_BACKGROUND,
		&index, objc, objv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((options.name != NULL) || (index < objc)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?-option value ...?");
	    return TCL_ERROR;
	}
	if ((options.fromX > masterPtr->width)
		|| (options.fromY > masterPtr->height)
		|| (options.fromX2 > masterPtr->width)
		|| (options.fromY2 > masterPtr->height)) {
	    Tcl_AppendResult(interp, "coordinates for -from option extend ",
		    "outside image", NULL);
	    return TCL_ERROR;
	}

	/*
	 * Fill in default values for unspecified parameters.
	 */

	if (((options.options & OPT_FROM) == 0) || (options.fromX2 < 0)) {
	    options.fromX2 = masterPtr->width;
	    options.fromY2 = masterPtr->height;
	}

	/*
	 * Search for an appropriate image string format handler.
	 */

	if (options.options & OPT_FORMAT) {
	    matched = 0;
	    for (imageFormat = tsdPtr->formatList; imageFormat != NULL;
	 	imageFormat = imageFormat->nextPtr) {
		if ((strncasecmp(Tcl_GetString(options.format),
			imageFormat->name, strlen(imageFormat->name)) == 0)) {
		    matched = 1;
		    if (imageFormat->stringWriteProc != NULL) {
			stringWriteProc = imageFormat->stringWriteProc;
			break;
		    }
		}
	    }
	    if (stringWriteProc == NULL) {
		oldformat = 1;
		for (imageFormat = tsdPtr->oldFormatList; imageFormat != NULL;
			imageFormat = imageFormat->nextPtr) {
		    if ((strncasecmp(Tcl_GetString(options.format),
			    imageFormat->name,
			    strlen(imageFormat->name)) == 0)) {
			matched = 1;
			if (imageFormat->stringWriteProc != NULL) {
			    stringWriteProc = imageFormat->stringWriteProc;
			    break;
			}
		    }
		}
	    }
	    if (stringWriteProc == NULL) {
		Tcl_AppendResult(interp, "image string format \"",
			Tcl_GetString(options.format), "\" is ",
			(matched ? "not supported" : "unknown"), NULL);
		return TCL_ERROR;
	    }
	} else {
	    stringWriteProc = ImgStringWrite;
	}

	/*
	 * Call the handler's string write function to write out the image.
	 */

	data = ImgGetPhoto(masterPtr, &block, &options);

	if (oldformat) {
	    Tcl_DString buffer;
	    typedef int (*OldStringWriteProc)(Tcl_Interp *interp,
		    Tcl_DString *dataPtr, const char *formatString,
		    Tk_PhotoImageBlock *blockPtr);

	    Tcl_DStringInit(&buffer);
	    result = ((OldStringWriteProc) stringWriteProc)(interp, &buffer,
		    Tcl_GetString(options.format), &block);
	    if (result == TCL_OK) {
		Tcl_DStringResult(interp, &buffer);
	    } else {
		Tcl_DStringFree(&buffer);
	    }
	} else {
	    typedef int (*NewStringWriteProc)(Tcl_Interp *interp,
		    Tcl_Obj *formatString, Tk_PhotoImageBlock *blockPtr,
		    void *dummy);

	    result = ((NewStringWriteProc) stringWriteProc)(interp,
		    options.format, &block, NULL);
	}
	if (options.background) {
	    Tk_FreeColor(options.background);
	}
	if (data) {
	    ckfree(data);
	}
	return result;
    }

    case PHOTO_GET: {
	/*
	 * photo get command - first parse and check parameters.
	 */

	char string[TCL_INTEGER_SPACE * 3];

	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "x y");
	    return TCL_ERROR;
	}
	if ((Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK)
		|| (Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK)) {
	    return TCL_ERROR;
	}
	if ((x < 0) || (x >= masterPtr->width)
		|| (y < 0) || (y >= masterPtr->height)) {
	    Tcl_AppendResult(interp, Tcl_GetString(objv[0]), " get: ",
		    "coordinates out of range", NULL);
	    return TCL_ERROR;
	}

	/*
	 * Extract the value of the desired pixel and format it as a string.
	 */

	pixelPtr = masterPtr->pix32 + (y * masterPtr->width + x) * 4;
	sprintf(string, "%d %d %d", pixelPtr[0], pixelPtr[1],
		pixelPtr[2]);
	Tcl_AppendResult(interp, string, NULL);
	return TCL_OK;
    }

    case PHOTO_PUT:
	/*
	 * photo put command - first parse the options and colors specified.
	 */

	index = 2;
	memset(&options, 0, sizeof(options));
	options.name = NULL;
	if (ParseSubcommandOptions(&options, interp, OPT_TO|OPT_FORMAT,
		&index, objc, objv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((options.name == NULL) || (index < objc)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "data ?-option value ...?");
	    return TCL_ERROR;
	}

	if (MatchStringFormat(interp, options.name ? objv[2]:NULL,
		options.format, &imageFormat, &imageWidth,
		&imageHeight, &oldformat) == TCL_OK) {
	    Tcl_Obj *format, *data;

	    if (((options.options & OPT_TO) == 0) || (options.toX2 < 0)) {
		options.toX2 = options.toX + imageWidth;
		options.toY2 = options.toY + imageHeight;
	    }
	    if (imageWidth > options.toX2 - options.toX) {
		imageWidth = options.toX2 - options.toX;
	    }
	    if (imageHeight > options.toY2 - options.toY) {
		imageHeight = options.toY2 - options.toY;
	    }
	    format = options.format;
	    data = objv[2];
	    if (oldformat) {
		if (format) {
		    format = (Tcl_Obj *) Tcl_GetString(format);
		}
		data = (Tcl_Obj *) Tcl_GetString(data);
	    }
	    if (imageFormat->stringReadProc(interp, data, format,
		    (Tk_PhotoHandle) masterPtr, options.toX, options.toY,
		    imageWidth, imageHeight, 0, 0) != TCL_OK) {
		return TCL_ERROR;
	    }
	    masterPtr->flags |= IMAGE_CHANGED;
	    return TCL_OK;
	}
	if (options.options & OPT_FORMAT) {
	    return TCL_ERROR;
	}
	Tcl_ResetResult(interp);
	if (Tcl_ListObjGetElements(interp, options.name,
		&dataHeight, &srcObjv) != TCL_OK) {
	    return TCL_ERROR;
	}
	tkwin = Tk_MainWindow(interp);
	block.pixelPtr = NULL;
	dataWidth = 0;
	pixelPtr = NULL;
	for (y = 0; y < dataHeight; ++y) {
	    if (Tcl_ListObjGetElements(interp, srcObjv[y],
		    &listObjc, &listObjv) != TCL_OK) {
		break;
	    }

	    if (y == 0) {
		if (listObjc == 0) {
		    /*
		     * Lines must be non-empty...
		     */

		    break;
		}
		dataWidth = listObjc;
		pixelPtr = (unsigned char *)
			ckalloc((unsigned) dataWidth * dataHeight * 3);
		block.pixelPtr = pixelPtr;
	    } else if (listObjc != dataWidth) {
		Tcl_AppendResult(interp, "all elements of color list must",
			" have the same number of elements", NULL);
		break;
	    }

	    for (x = 0; x < dataWidth; ++x) {
		const char *colorString = Tcl_GetString(listObjv[x]);
		XColor color;
		int tmpr, tmpg, tmpb;

		/*
		 * We do not use Tk_GetColorFromObj() because we absolutely do
		 * not want to invoke the fallback code.
		 */

		if (colorString[0] == '#') {
		    if (isxdigit(UCHAR(colorString[1])) &&
			    isxdigit(UCHAR(colorString[2])) &&
			    isxdigit(UCHAR(colorString[3]))) {
			if (colorString[4] == '\0') {
			    /* Got #rgb */
			    sscanf(colorString+1, "%1x%1x%1x",
				    &tmpr, &tmpg, &tmpb);
			    *pixelPtr++ = tmpr * 0x11;
			    *pixelPtr++ = tmpg * 0x11;
			    *pixelPtr++ = tmpb * 0x11;
			    continue;
			} else if (isxdigit(UCHAR(colorString[4])) &&
				isxdigit(UCHAR(colorString[5])) &&
				isxdigit(UCHAR(colorString[6])) &&
				colorString[7] == '\0') {
			    /* Got #rrggbb */
			    sscanf(colorString+1, "%2x%2x%2x",
				    &tmpr, &tmpg, &tmpb);
			    *pixelPtr++ = tmpr;
			    *pixelPtr++ = tmpg;
			    *pixelPtr++ = tmpb;
			    continue;
			}
		    }
		}

		if (!XParseColor(Tk_Display(tkwin), Tk_Colormap(tkwin),
			colorString, &color)) {
		    Tcl_AppendResult(interp, "can't parse color \"",
			    colorString, "\"", NULL);
		    break;
		}
		*pixelPtr++ = color.red >> 8;
		*pixelPtr++ = color.green >> 8;
		*pixelPtr++ = color.blue >> 8;
	    }
	    if (x < dataWidth) {
		break;
	    }
	}
	if (y < dataHeight || dataHeight == 0 || dataWidth == 0) {
	    if (block.pixelPtr != NULL) {
		ckfree((char *) block.pixelPtr);
	    }
	    if (y < dataHeight) {
		return TCL_ERROR;
	    }
	    return TCL_OK;
	}

	/*
	 * Fill in default values for the -to option, then copy the block in
	 * using Tk_PhotoPutBlock.
	 */

	if (!(options.options & OPT_TO) || (options.toX2 < 0)) {
	    options.toX2 = options.toX + dataWidth;
	    options.toY2 = options.toY + dataHeight;
	}
	block.width = dataWidth;
	block.height = dataHeight;
	block.pitch = dataWidth * 3;
	block.pixelSize = 3;
	block.offset[0] = 0;
	block.offset[1] = 1;
	block.offset[2] = 2;
	block.offset[3] = 0;
	result = Tk_PhotoPutBlock(interp, masterPtr, &block,
		options.toX, options.toY, options.toX2 - options.toX,
		options.toY2 - options.toY,
		TK_PHOTO_COMPOSITE_SET);
	ckfree((char *) block.pixelPtr);
	return result;

    case PHOTO_READ: {
	Tcl_Obj *format;

	/*
	 * photo read command - first parse the options specified.
	 */

	index = 2;
	memset(&options, 0, sizeof(options));
	options.name = NULL;
	options.format = NULL;
	if (ParseSubcommandOptions(&options, interp,
		OPT_FORMAT | OPT_FROM | OPT_TO | OPT_SHRINK,
		&index, objc, objv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((options.name == NULL) || (index < objc)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "fileName ?-option value ...?");
	    return TCL_ERROR;
	}

	/*
	 * Prevent file system access in safe interpreters.
	 */

	if (Tcl_IsSafe(interp)) {
	    Tcl_AppendResult(interp, "can't get image from a file in a",
		    " safe interpreter", NULL);
	    return TCL_ERROR;
	}

	/*
	 * Open the image file and look for a handler for it.
	 */

	chan = Tcl_OpenFileChannel(interp,
		Tcl_GetString(options.name), "r", 0);
	if (chan == NULL) {
	    return TCL_ERROR;
	}
	if (Tcl_SetChannelOption(interp, chan, "-translation", "binary")
		!= TCL_OK) {
	    Tcl_Close(NULL, chan);
	    return TCL_ERROR;
	}
	if (Tcl_SetChannelOption(interp, chan, "-encoding", "binary")
		!= TCL_OK) {
	    Tcl_Close(NULL, chan);
	    return TCL_ERROR;
	}

	if (MatchFileFormat(interp, chan,
		Tcl_GetString(options.name), options.format, &imageFormat,
		&imageWidth, &imageHeight, &oldformat) != TCL_OK) {
	    Tcl_Close(NULL, chan);
	    return TCL_ERROR;
	}

	/*
	 * Check the values given for the -from option.
	 */

	if ((options.fromX > imageWidth) || (options.fromY > imageHeight)
		|| (options.fromX2 > imageWidth)
		|| (options.fromY2 > imageHeight)) {
	    Tcl_AppendResult(interp, "coordinates for -from option extend ",
		    "outside source image", NULL);
	    Tcl_Close(NULL, chan);
	    return TCL_ERROR;
	}
	if (((options.options & OPT_FROM) == 0) || (options.fromX2 < 0)) {
	    width = imageWidth - options.fromX;
	    height = imageHeight - options.fromY;
	} else {
	    width = options.fromX2 - options.fromX;
	    height = options.fromY2 - options.fromY;
	}

	/*
	 * If the -shrink option was specified, set the size of the image.
	 */

	if (options.options & OPT_SHRINK) {
	    if (ImgPhotoSetSize(masterPtr, options.toX + width,
		    options.toY + height) != TCL_OK) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, TK_PHOTO_ALLOC_FAILURE_MESSAGE, NULL);
		return TCL_ERROR;
	    }
	}

	/*
	 * Call the handler's file read function to read the data into the
	 * image.
	 */

	format = options.format;
	if (oldformat && format) {
	    format = (Tcl_Obj *) Tcl_GetString(format);
	}
	result = imageFormat->fileReadProc(interp, chan,
		Tcl_GetString(options.name),
		format, (Tk_PhotoHandle) masterPtr, options.toX,
		options.toY, width, height, options.fromX, options.fromY);
	if (chan != NULL) {
	    Tcl_Close(NULL, chan);
	}
	return result;
    }

    case PHOTO_REDITHER:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    return TCL_ERROR;
	}

	/*
	 * Call Dither if any part of the image is not correctly dithered at
	 * present.
	 */

	x = masterPtr->ditherX;
	y = masterPtr->ditherY;
	if (masterPtr->ditherX != 0) {
	    Tk_DitherPhoto((Tk_PhotoHandle) masterPtr, x, y,
		    masterPtr->width - x, 1);
	}
	if (masterPtr->ditherY < masterPtr->height) {
	    x = 0;
	    Tk_DitherPhoto((Tk_PhotoHandle)masterPtr, 0,
		    masterPtr->ditherY, masterPtr->width,
		    masterPtr->height - masterPtr->ditherY);
	}

	if (y < masterPtr->height) {
	    /*
	     * Tell the core image code that part of the image has changed.
	     */

	    Tk_ImageChanged(masterPtr->tkMaster, x, y,
		    (masterPtr->width - x), (masterPtr->height - y),
		    masterPtr->width, masterPtr->height);
	}
	return TCL_OK;

    case PHOTO_TRANS: {
	static const char *const photoTransOptions[] = {
	    "get", "set", NULL
	};
	enum transOptions {
	    PHOTO_TRANS_GET, PHOTO_TRANS_SET
	};

	if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "option ?arg ...?");
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(interp, objv[2], photoTransOptions, "option",
		0, &index) != TCL_OK) {
	    return TCL_ERROR;
	}

	switch ((enum transOptions) index) {
	case PHOTO_TRANS_GET: {
	    XRectangle testBox;
	    TkRegion testRegion;

	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "x y");
		return TCL_ERROR;
	    }
	    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
		    || (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    if ((x < 0) || (x >= masterPtr->width)
		    || (y < 0) || (y >= masterPtr->height)) {
		Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
			" transparency get: coordinates out of range", NULL);
		return TCL_ERROR;
	    }

	    testBox.x = x;
	    testBox.y = y;
	    testBox.width = 1;
	    testBox.height = 1;
	    /* What a way to do a test! */
	    testRegion = TkCreateRegion();
	    TkUnionRectWithRegion(&testBox, testRegion, testRegion);
	    TkIntersectRegion(testRegion, masterPtr->validRegion, testRegion);
	    TkClipBox(testRegion, &testBox);
	    TkDestroyRegion(testRegion);

	    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(
		    testBox.width==0 && testBox.height==0));
	    return TCL_OK;
	}

	case PHOTO_TRANS_SET: {
	    int transFlag;
	    XRectangle setBox;

	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 3, objv, "x y boolean");
		return TCL_ERROR;
	    }
	    if ((Tcl_GetIntFromObj(interp, objv[3], &x) != TCL_OK)
		    || (Tcl_GetIntFromObj(interp, objv[4], &y) != TCL_OK)
		    || (Tcl_GetBooleanFromObj(interp, objv[5],
		    &transFlag) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    if ((x < 0) || (x >= masterPtr->width)
		|| (y < 0) || (y >= masterPtr->height)) {
		Tcl_AppendResult(interp, Tcl_GetString(objv[0]),
			" transparency set: coordinates out of range", NULL);
		return TCL_ERROR;
	    }

	    setBox.x = x;
	    setBox.y = y;
	    setBox.width = 1;
	    setBox.height = 1;
	    pixelPtr = masterPtr->pix32 + (y * masterPtr->width + x) * 4;

	    if (transFlag) {
		/*
		 * Make pixel transparent.
		 */

		TkRegion clearRegion = TkCreateRegion();

		TkUnionRectWithRegion(&setBox, clearRegion, clearRegion);
		TkSubtractRegion(masterPtr->validRegion, clearRegion,
			masterPtr->validRegion);
		TkDestroyRegion(clearRegion);

		/*
		 * Set the alpha value correctly.
		 */

		pixelPtr[3] = 0;
	    } else {
		/*
		 * Make pixel opaque.
		 */

		TkUnionRectWithRegion(&setBox, masterPtr->validRegion,
			masterPtr->validRegion);
		pixelPtr[3] = 255;
	    }

	    /*
	     * Inform the generic image code that the image
	     * has (potentially) changed.
	     */

	    Tk_ImageChanged(masterPtr->tkMaster, x, y, 1, 1,
		    masterPtr->width, masterPtr->height);
	    masterPtr->flags &= ~IMAGE_CHANGED;
	    return TCL_OK;
	}

	}
	Tcl_Panic("unexpected fallthrough");
    }

    case PHOTO_WRITE: {
	char *data;
	Tcl_Obj *format;

	/*
	 * Prevent file system access in safe interpreters.
	 */

	if (Tcl_IsSafe(interp)) {
	    Tcl_AppendResult(interp, "can't write image to a file in a",
		    " safe interpreter", NULL);
	    return TCL_ERROR;
	}

	/*
	 * photo write command - first parse and check any options given.
	 */

	index = 2;
	memset(&options, 0, sizeof(options));
	options.name = NULL;
	options.format = NULL;
	if (ParseSubcommandOptions(&options, interp,
		OPT_FORMAT | OPT_FROM | OPT_GRAYSCALE | OPT_BACKGROUND,
		&index, objc, objv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((options.name == NULL) || (index < objc)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "fileName ?-option value ...?");
	    return TCL_ERROR;
	}
	if ((options.fromX > masterPtr->width)
		|| (options.fromY > masterPtr->height)
		|| (options.fromX2 > masterPtr->width)
		|| (options.fromY2 > masterPtr->height)) {
	    Tcl_AppendResult(interp, "coordinates for -from option extend ",
		    "outside image", NULL);
	    return TCL_ERROR;
	}

	/*
	 * Fill in default values for unspecified parameters.
	 */

	if (!(options.options & OPT_FROM) || (options.fromX2 < 0)) {
	    options.fromX2 = masterPtr->width;
	    options.fromY2 = masterPtr->height;
	}

	/*
	 * Search for an appropriate image file format handler, and give an
	 * error if none is found.
	 */

	matched = 0;
	for (imageFormat = tsdPtr->formatList; imageFormat != NULL;
		imageFormat = imageFormat->nextPtr) {
	    if ((options.format == NULL)
		    || (strncasecmp(Tcl_GetString(options.format),
		    imageFormat->name, strlen(imageFormat->name)) == 0)) {
		matched = 1;
		if (imageFormat->fileWriteProc != NULL) {
		    break;
		}
	    }
	}
	if (imageFormat == NULL) {
	    oldformat = 1;
	    for (imageFormat = tsdPtr->oldFormatList; imageFormat != NULL;
		    imageFormat = imageFormat->nextPtr) {
		if ((options.format == NULL)
			|| (strncasecmp(Tcl_GetString(options.format),
			imageFormat->name, strlen(imageFormat->name)) == 0)) {
		    matched = 1;
		    if (imageFormat->fileWriteProc != NULL) {
			break;
		    }
		}
	    }
	}
	if (imageFormat == NULL) {
	    if (options.format == NULL) {
		Tcl_AppendResult(interp, "no available image file format ",
			"has file writing capability", NULL);
	    } else if (!matched) {
		Tcl_AppendResult(interp, "image file format \"",
			Tcl_GetString(options.format),
			"\" is unknown", NULL);
	    } else {
		Tcl_AppendResult(interp, "image file format \"",
			Tcl_GetString(options.format),
			"\" has no file writing capability", NULL);
	    }
	    return TCL_ERROR;
	}

	/*
	 * Call the handler's file write function to write out the image.
	 */

	data = ImgGetPhoto(masterPtr, &block, &options);
	format = options.format;
	if (oldformat && format) {
	    format = (Tcl_Obj *) Tcl_GetString(options.format);
	}
	result = imageFormat->fileWriteProc(interp,
		Tcl_GetString(options.name), format, &block);
	if (options.background) {
	    Tk_FreeColor(options.background);
	}
	if (data) {
	    ckfree(data);
	}
	return result;
    }

    }
    Tcl_Panic("unexpected fallthrough");
    return TCL_ERROR; /* NOT REACHED */
}

/*
 *----------------------------------------------------------------------
 *
 * ParseSubcommandOptions --
 *
 *	This function is invoked to process one of the options which may be
 *	specified for the photo image subcommands, namely, -from, -to, -zoom,
 *	-subsample, -format, -shrink, and -compositingrule.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Fields in *optPtr get filled in.
 *
 *----------------------------------------------------------------------
 */

static int
ParseSubcommandOptions(
    struct SubcommandOptions *optPtr,
				/* Information about the options specified and
				 * the values given is returned here. */
    Tcl_Interp *interp,		/* Interpreter to use for reporting errors. */
    int allowedOptions,		/* Indicates which options are valid for the
				 * current command. */
    int *optIndexPtr,		/* Points to a variable containing the current
				 * index in objv; this variable is updated by
				 * this function. */
    int objc,			/* Number of arguments in objv[]. */
    Tcl_Obj *const objv[])	/* Arguments to be parsed. */
{
    int index, c, bit, currentBit, length;
    int values[4], numValues, maxValues, argIndex;
    const char *option;
    const char *const *listPtr;

    for (index = *optIndexPtr; index < objc; *optIndexPtr = ++index) {
	/*
	 * We can have one value specified without an option; it goes into
	 * optPtr->name.
	 */

	option = Tcl_GetStringFromObj(objv[index], &length);
	if (option[0] != '-') {
	    if (optPtr->name == NULL) {
		optPtr->name = objv[index];
		continue;
	    }
	    break;
	}

	/*
	 * Work out which option this is.
	 */

	c = option[0];
	bit = 0;
	currentBit = 1;
	for (listPtr = optionNames; *listPtr != NULL; ++listPtr) {
	    if ((c == *listPtr[0])
		    && (strncmp(option, *listPtr, (size_t) length) == 0)) {
		if (bit != 0) {
		    bit = 0;	/* An ambiguous option. */
		    break;
		}
		bit = currentBit;
	    }
	    currentBit <<= 1;
	}

	/*
	 * If this option is not recognized and allowed, put an error message
	 * in the interpreter and return.
	 */

	if ((allowedOptions & bit) == 0) {
	    Tcl_AppendResult(interp, "unrecognized option \"",
	    	    Tcl_GetString(objv[index]),
		    "\": must be ", NULL);
	    bit = 1;
	    for (listPtr = optionNames; *listPtr != NULL; ++listPtr) {
		if ((allowedOptions & bit) != 0) {
		    if ((allowedOptions & (bit - 1)) != 0) {
			Tcl_AppendResult(interp, ", ", NULL);
			if ((allowedOptions & ~((bit << 1) - 1)) == 0) {
			    Tcl_AppendResult(interp, "or ", NULL);
			}
		    }
		    Tcl_AppendResult(interp, *listPtr, NULL);
		}
		bit <<= 1;
	    }
	    return TCL_ERROR;
	}

	/*
	 * For the -from, -to, -zoom and -subsample options, parse the values
	 * given. Report an error if too few or too many values are given.
	 */

	if (bit == OPT_BACKGROUND) {
	    /*
	     * The -background option takes a single XColor value.
	     */

	    if (index + 1 < objc) {
		*optIndexPtr = ++index;
		optPtr->background = Tk_GetColor(interp, Tk_MainWindow(interp),
			Tk_GetUid(Tcl_GetString(objv[index])));
		if (!optPtr->background) {
		    return TCL_ERROR;
		}
	    } else {
		Tcl_AppendResult(interp, "the \"-background\" option ",
			"requires a value", NULL);
		return TCL_ERROR;
	    }
	} else if (bit == OPT_FORMAT) {
	    /*
	     * The -format option takes a single string value. Note that
	     * parsing this is outside the scope of this function.
	     */

	    if (index + 1 < objc) {
		*optIndexPtr = ++index;
		optPtr->format = objv[index];
	    } else {
		Tcl_AppendResult(interp, "the \"-format\" option ",
			"requires a value", NULL);
		return TCL_ERROR;
	    }
	} else if (bit == OPT_COMPOSITE) {
	    /*
	     * The -compositingrule option takes a single value from a
	     * well-known set.
	     */

	    if (index + 1 < objc) {
		/*
		 * Note that these must match the TK_PHOTO_COMPOSITE_*
		 * constants.
		 */

		static const char *const compositingRules[] = {
		    "overlay", "set", NULL
		};

		index++;
		if (Tcl_GetIndexFromObj(interp, objv[index], compositingRules,
			"compositing rule", 0, &optPtr->compositingRule)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
		*optIndexPtr = index;
	    } else {
		Tcl_AppendResult(interp, "the \"-compositingrule\" option ",
			"requires a value", NULL);
		return TCL_ERROR;
	    }
	} else if ((bit != OPT_SHRINK) && (bit != OPT_GRAYSCALE)) {
	    const char *val;
	    maxValues = ((bit == OPT_FROM) || (bit == OPT_TO))? 4: 2;
	    argIndex = index + 1;
	    for (numValues = 0; numValues < maxValues; ++numValues) {
		if (argIndex >= objc) {
		    break;
		}
		val = Tcl_GetString(objv[argIndex]);
		if ((argIndex < objc) && (isdigit(UCHAR(val[0]))
			|| ((val[0] == '-') && isdigit(UCHAR(val[1]))))) {
		    if (Tcl_GetInt(interp, val, &values[numValues])
			    != TCL_OK) {
			return TCL_ERROR;
		    }
		} else {
		    break;
		}
		++argIndex;
	    }

	    if (numValues == 0) {
		Tcl_AppendResult(interp, "the \"", option, "\" option ",
			 "requires one ", maxValues == 2? "or two": "to four",
			 " integer values", NULL);
		return TCL_ERROR;
	    }
	    *optIndexPtr = (index += numValues);

	    /*
	     * Y values default to the corresponding X value if not specified.
	     */

	    if (numValues == 1) {
		values[1] = values[0];
	    }
	    if (numValues == 3) {
		values[3] = values[2];
	    }

	    /*
	     * Check the values given and put them in the appropriate field of
	     * the SubcommandOptions structure.
	     */

	    switch (bit) {
	    case OPT_FROM:
		if ((values[0] < 0) || (values[1] < 0) || ((numValues > 2)
			&& ((values[2] < 0) || (values[3] < 0)))) {
		    Tcl_AppendResult(interp, "value(s) for the -from",
			    " option must be non-negative", NULL);
		    return TCL_ERROR;
		}
		if (numValues <= 2) {
		    optPtr->fromX = values[0];
		    optPtr->fromY = values[1];
		    optPtr->fromX2 = -1;
		    optPtr->fromY2 = -1;
		} else {
		    optPtr->fromX = MIN(values[0], values[2]);
		    optPtr->fromY = MIN(values[1], values[3]);
		    optPtr->fromX2 = MAX(values[0], values[2]);
		    optPtr->fromY2 = MAX(values[1], values[3]);
		}
		break;
	    case OPT_SUBSAMPLE:
		optPtr->subsampleX = values[0];
		optPtr->subsampleY = values[1];
		break;
	    case OPT_TO:
		if ((values[0] < 0) || (values[1] < 0) || ((numValues > 2)
			&& ((values[2] < 0) || (values[3] < 0)))) {
		    Tcl_AppendResult(interp, "value(s) for the -to",
			    " option must be non-negative", NULL);
		    return TCL_ERROR;
		}
		if (numValues <= 2) {
		    optPtr->toX = values[0];
		    optPtr->toY = values[1];
		    optPtr->toX2 = -1;
		    optPtr->toY2 = -1;
		} else {
		    optPtr->toX = MIN(values[0], values[2]);
		    optPtr->toY = MIN(values[1], values[3]);
		    optPtr->toX2 = MAX(values[0], values[2]);
		    optPtr->toY2 = MAX(values[1], values[3]);
		}
		break;
	    case OPT_ZOOM:
		if ((values[0] <= 0) || (values[1] <= 0)) {
		    Tcl_AppendResult(interp, "value(s) for the -zoom",
			    " option must be positive", NULL);
		    return TCL_ERROR;
		}
		optPtr->zoomX = values[0];
		optPtr->zoomY = values[1];
		break;
	    }
	}

	/*
	 * Remember that we saw this option.
	 */

	optPtr->options |= bit;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoConfigureMaster --
 *
 *	This function is called when a photo image is created or reconfigured.
 *	It processes configuration options and resets any instances of the
 *	image.
 *
 * Results:
 *	A standard Tcl return value. If TCL_ERROR is returned then an error
 *	message is left in the masterPtr->interp's result.
 *
 * Side effects:
 *	Existing instances of the image will be redisplayed to match the new
 *	configuration options.
 *
 *----------------------------------------------------------------------
 */

static int
ImgPhotoConfigureMaster(
    Tcl_Interp *interp,		/* Interpreter to use for reporting errors. */
    PhotoMaster *masterPtr,	/* Pointer to data structure describing
				 * overall photo image to (re)configure. */
    int objc,			/* Number of entries in objv. */
    Tcl_Obj *const objv[],	/* Pairs of configuration options for image. */
    int flags)			/* Flags to pass to Tk_ConfigureWidget, such
				 * as TK_CONFIG_ARGV_ONLY. */
{
    PhotoInstance *instancePtr;
    const char *oldFileString, *oldPaletteString;
    Tcl_Obj *oldData, *data = NULL, *oldFormat, *format = NULL;
    Tcl_Obj *tempdata, *tempformat;
    int length, i, j, result, imageWidth, imageHeight, oldformat;
    double oldGamma;
    Tcl_Channel chan;
    Tk_PhotoImageFormat *imageFormat;
    const char **args;

    args = (const char **) ckalloc((objc + 1) * sizeof(char *));
    for (i = 0, j = 0; i < objc; i++,j++) {
	args[j] = Tcl_GetStringFromObj(objv[i], &length);
	if ((length > 1) && (args[j][0] == '-')) {
	    if ((args[j][1] == 'd') &&
		    !strncmp(args[j], "-data", (size_t) length)) {
		if (++i < objc) {
		    data = objv[i];
		    j--;
		} else {
		    ckfree((char *) args);
		    Tcl_AppendResult(interp,
			    "value for \"-data\" missing", NULL);
		    return TCL_ERROR;
		}
	    } else if ((args[j][1] == 'f') &&
		    !strncmp(args[j], "-format", (size_t) length)) {
		if (++i < objc) {
		    format = objv[i];
		    j--;
		} else {
		    ckfree((char *) args);
		    Tcl_AppendResult(interp,
			    "value for \"-format\" missing", NULL);
		    return TCL_ERROR;
		}
	    }
	}
    }

    /*
     * Save the current values for fileString and dataString, so we can tell
     * if the user specifies them anew. IMPORTANT: if the format changes we
     * have to interpret "-file" and "-data" again as well! It might be that
     * the format string influences how "-data" or "-file" is interpreted.
     */

    oldFileString = masterPtr->fileString;
    if (oldFileString == NULL) {
	oldData = masterPtr->dataString;
	if (oldData != NULL) {
	    Tcl_IncrRefCount(oldData);
	}
    } else {
	oldData = NULL;
    }
    oldFormat = masterPtr->format;
    if (oldFormat != NULL) {
	Tcl_IncrRefCount(oldFormat);
    }
    oldPaletteString = masterPtr->palette;
    oldGamma = masterPtr->gamma;

    /*
     * Process the configuration options specified.
     */

    if (Tk_ConfigureWidget(interp, Tk_MainWindow(interp), configSpecs,
	    j, args, (char *) masterPtr, flags) != TCL_OK) {
	ckfree((char *) args);
	goto errorExit;
    }
    ckfree((char *) args);

    /*
     * Regard the empty string for -file, -data or -format as the null value.
     */

    if ((masterPtr->fileString != NULL) && (masterPtr->fileString[0] == 0)) {
	ckfree(masterPtr->fileString);
	masterPtr->fileString = NULL;
    }
    if (data) {
	/*
	 * Force into ByteArray format, which most (all) image handlers will
	 * use anyway. Empty length means ignore the -data option.
	 */

	(void) Tcl_GetByteArrayFromObj(data, &length);
	if (length) {
	    Tcl_IncrRefCount(data);
	} else {
	    data = NULL;
	}
	if (masterPtr->dataString) {
	    Tcl_DecrRefCount(masterPtr->dataString);
	}
	masterPtr->dataString = data;
    }
    if (format) {
	/*
	 * Stringify to ignore -format "". It may come in as a list or other
	 * object.
	 */

	(void) Tcl_GetStringFromObj(format, &length);
	if (length) {
	    Tcl_IncrRefCount(format);
	} else {
	    format = NULL;
	}
	if (masterPtr->format) {
	    Tcl_DecrRefCount(masterPtr->format);
	}
	masterPtr->format = format;
    }
    /*
     * Set the image to the user-requested size, if any, and make sure storage
     * is correctly allocated for this image.
     */

    if (ImgPhotoSetSize(masterPtr, masterPtr->width,
	    masterPtr->height) != TCL_OK) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, TK_PHOTO_ALLOC_FAILURE_MESSAGE, NULL);
	goto errorExit;
    }

    /*
     * Read in the image from the file or string if the user has specified the
     * -file or -data option.
     */

    if ((masterPtr->fileString != NULL)
	    && ((masterPtr->fileString != oldFileString)
	    || (masterPtr->format != oldFormat))) {
	/*
	 * Prevent file system access in a safe interpreter.
	 */

	if (Tcl_IsSafe(interp)) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp,
		    "can't get image from a file in a safe interpreter", NULL);
	    goto errorExit;
	}

	chan = Tcl_OpenFileChannel(interp, masterPtr->fileString, "r", 0);
	if (chan == NULL) {
	    goto errorExit;
	}

	/*
	 * -translation binary also sets -encoding binary
	 */

	if ((Tcl_SetChannelOption(interp, chan,
		"-translation", "binary") != TCL_OK) ||
		(MatchFileFormat(interp, chan, masterPtr->fileString,
			masterPtr->format, &imageFormat, &imageWidth,
			&imageHeight, &oldformat) != TCL_OK)) {
	    Tcl_Close(NULL, chan);
	    goto errorExit;
	}
	result = ImgPhotoSetSize(masterPtr, imageWidth, imageHeight);
	if (result != TCL_OK) {
	    Tcl_Close(NULL, chan);
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, TK_PHOTO_ALLOC_FAILURE_MESSAGE, NULL);
	    goto errorExit;
	}
	tempformat = masterPtr->format;
	if (oldformat && tempformat) {
	    tempformat = (Tcl_Obj *) Tcl_GetString(tempformat);
	}
	result = imageFormat->fileReadProc(interp, chan,
		masterPtr->fileString, tempformat, (Tk_PhotoHandle) masterPtr,
		0, 0, imageWidth, imageHeight, 0, 0);
	Tcl_Close(NULL, chan);
	if (result != TCL_OK) {
	    goto errorExit;
	}

	Tcl_ResetResult(interp);
	masterPtr->flags |= IMAGE_CHANGED;
    }

    if ((masterPtr->fileString == NULL) && (masterPtr->dataString != NULL)
	    && ((masterPtr->dataString != oldData)
		    || (masterPtr->format != oldFormat))) {

	if (MatchStringFormat(interp, masterPtr->dataString,
		masterPtr->format, &imageFormat, &imageWidth,
		&imageHeight, &oldformat) != TCL_OK) {
	    goto errorExit;
	}
	if (ImgPhotoSetSize(masterPtr, imageWidth, imageHeight) != TCL_OK) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, TK_PHOTO_ALLOC_FAILURE_MESSAGE, NULL);
	    goto errorExit;
	}
	tempformat = masterPtr->format;
	tempdata = masterPtr->dataString;
	if (oldformat) {
	    if (tempformat) {
		tempformat = (Tcl_Obj *) Tcl_GetString(tempformat);
	    }
	    tempdata = (Tcl_Obj *) Tcl_GetString(tempdata);
	}
	if (imageFormat->stringReadProc(interp, tempdata, tempformat,
		(Tk_PhotoHandle) masterPtr, 0, 0, imageWidth, imageHeight,
		0, 0) != TCL_OK) {
	    goto errorExit;
	}

	Tcl_ResetResult(interp);
	masterPtr->flags |= IMAGE_CHANGED;
    }

    /*
     * Enforce a reasonable value for gamma.
     */

    if (masterPtr->gamma <= 0) {
	masterPtr->gamma = 1.0;
    }

    if ((masterPtr->gamma != oldGamma)
	    || (masterPtr->palette != oldPaletteString)) {
	masterPtr->flags |= IMAGE_CHANGED;
    }

    /*
     * Cycle through all of the instances of this image, regenerating the
     * information for each instance. Then force the image to be redisplayed
     * everywhere that it is used.
     */

    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	TkImgPhotoConfigureInstance(instancePtr);
    }

    /*
     * Inform the generic image code that the image has (potentially) changed.
     */

    Tk_ImageChanged(masterPtr->tkMaster, 0, 0, masterPtr->width,
	    masterPtr->height, masterPtr->width, masterPtr->height);
    masterPtr->flags &= ~IMAGE_CHANGED;

    if (oldData != NULL) {
	Tcl_DecrRefCount(oldData);
    }
    if (oldFormat != NULL) {
	Tcl_DecrRefCount(oldFormat);
    }

    ToggleComplexAlphaIfNeeded(masterPtr);

    return TCL_OK;

  errorExit:
    if (oldData != NULL) {
	Tcl_DecrRefCount(oldData);
    }
    if (oldFormat != NULL) {
	Tcl_DecrRefCount(oldFormat);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ToggleComplexAlphaIfNeeded --
 *
 *	This function is called when an image is modified to check if any
 *	partially transparent pixels exist, which requires blending instead of
 *	straight copy.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	(Re)sets COMPLEX_ALPHA flag of master.
 *
 *----------------------------------------------------------------------
 */

static int
ToggleComplexAlphaIfNeeded(
    PhotoMaster *mPtr)
{
    size_t len = MAX(mPtr->userWidth, mPtr->width) *
	    MAX(mPtr->userHeight, mPtr->height) * 4;
    unsigned char *c = mPtr->pix32;
    unsigned char *end = c + len;

    /*
     * Set the COMPLEX_ALPHA flag if we have an image with partially
     * transparent bits.
     */

    mPtr->flags &= ~COMPLEX_ALPHA;
    c += 3;			/* Start at first alpha byte. */
    for (; c < end; c += 4) {
	if (*c && *c != 255) {
     	    mPtr->flags |= COMPLEX_ALPHA;
	    break;
	}
    }
    return (mPtr->flags & COMPLEX_ALPHA);
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoDelete --
 *
 *	This function is called by the image code to delete the master
 *	structure for an image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources associated with the image get freed.
 *
 *----------------------------------------------------------------------
 */

static void
ImgPhotoDelete(
    ClientData masterData)	/* Pointer to PhotoMaster structure for image.
				 * Must not have any more instances. */
{
    PhotoMaster *masterPtr = masterData;
    PhotoInstance *instancePtr;

    while ((instancePtr = masterPtr->instancePtr) != NULL) {
	if (instancePtr->refCount > 0) {
	    Tcl_Panic("tried to delete photo image when instances still exist");
	}
	Tcl_CancelIdleCall(TkImgDisposeInstance, instancePtr);
	TkImgDisposeInstance(instancePtr);
    }
    masterPtr->tkMaster = NULL;
    if (masterPtr->imageCmd != NULL) {
	Tcl_DeleteCommandFromToken(masterPtr->interp, masterPtr->imageCmd);
    }
    if (masterPtr->pix32 != NULL) {
	ckfree((char *) masterPtr->pix32);
    }
    if (masterPtr->validRegion != NULL) {
	TkDestroyRegion(masterPtr->validRegion);
    }
    if (masterPtr->dataString != NULL) {
	Tcl_DecrRefCount(masterPtr->dataString);
    }
    if (masterPtr->format != NULL) {
	Tcl_DecrRefCount(masterPtr->format);
    }
    Tk_FreeOptions(configSpecs, (char *) masterPtr, NULL, 0);
    ckfree((char *) masterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoCmdDeletedProc --
 *
 *	This function is invoked when the image command for an image is
 *	deleted. It deletes the image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image is deleted.
 *
 *----------------------------------------------------------------------
 */

static void
ImgPhotoCmdDeletedProc(
    ClientData clientData)	/* Pointer to PhotoMaster structure for
				 * image. */
{
    PhotoMaster *masterPtr = clientData;

    masterPtr->imageCmd = NULL;
    if (masterPtr->tkMaster != NULL) {
	Tk_DeleteImage(masterPtr->interp, Tk_NameOfImage(masterPtr->tkMaster));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ImgPhotoSetSize --
 *
 *	This function reallocates the image storage and instance pixmaps for a
 *	photo image, as necessary, to change the image's size to `width' x
 *	`height' pixels.
 *
 * Results:
 *	TCL_OK if successful, TCL_ERROR if failure occurred (currently just
 *	with memory allocation.)
 *
 * Side effects:
 *	Storage gets reallocated, for the master and all its instances.
 *
 *----------------------------------------------------------------------
 */

static int
ImgPhotoSetSize(
    PhotoMaster *masterPtr,
    int width, int height)
{
    unsigned char *newPix32 = NULL;
    int h, offset, pitch;
    unsigned char *srcPtr, *destPtr;
    XRectangle validBox, clipBox;
    TkRegion clipRegion;
    PhotoInstance *instancePtr;

    if (masterPtr->userWidth > 0) {
	width = masterPtr->userWidth;
    }
    if (masterPtr->userHeight > 0) {
	height = masterPtr->userHeight;
    }

    pitch = width * 4;

    /*
     * Test if we're going to (re)allocate the main buffer now, so that any
     * failures will leave the photo unchanged.
     */

    if ((width != masterPtr->width) || (height != masterPtr->height)
	    || (masterPtr->pix32 == NULL)) {
	/*
	 * Not a u-long, but should be one.
	 */

	unsigned /*long*/ newPixSize = (unsigned /*long*/) (height * pitch);

	/*
	 * Some mallocs() really hate allocating zero bytes. [Bug 619544]
	 */

	if (newPixSize == 0) {
	    newPix32 = NULL;
	} else {
	    newPix32 = (unsigned char *) attemptckalloc(newPixSize);
	    if (newPix32 == NULL) {
		return TCL_ERROR;
	    }
	}
    }

    /*
     * We have to trim the valid region if it is currently larger than the new
     * image size.
     */

    TkClipBox(masterPtr->validRegion, &validBox);
    if ((validBox.x + validBox.width > width)
	    || (validBox.y + validBox.height > height)) {
	clipBox.x = 0;
	clipBox.y = 0;
	clipBox.width = width;
	clipBox.height = height;
	clipRegion = TkCreateRegion();
	TkUnionRectWithRegion(&clipBox, clipRegion, clipRegion);
	TkIntersectRegion(masterPtr->validRegion, clipRegion,
		masterPtr->validRegion);
	TkDestroyRegion(clipRegion);
	TkClipBox(masterPtr->validRegion, &validBox);
    }

    /*
     * Use the reallocated storage (allocation above) for the 32-bit image and
     * copy over valid regions. Note that this test is true precisely when the
     * allocation has already been done.
     */

    if (newPix32 != NULL) {
	/*
	 * Zero the new array. The dithering code shouldn't read the areas
	 * outside validBox, but they might be copied to another photo image
	 * or written to a file.
	 */

	if ((masterPtr->pix32 != NULL)
	    && ((width == masterPtr->width) || (width == validBox.width))) {
	    if (validBox.y > 0) {
		memset(newPix32, 0, (size_t) (validBox.y * pitch));
	    }
	    h = validBox.y + validBox.height;
	    if (h < height) {
		memset(newPix32 + h*pitch, 0, (size_t) ((height - h) * pitch));
	    }
	} else {
	    memset(newPix32, 0, (size_t) (height * pitch));
	}

	if (masterPtr->pix32 != NULL) {
	    /*
	     * Copy the common area over to the new array array and free the
	     * old array.
	     */

	    if (width == masterPtr->width) {

		/*
		 * The region to be copied is contiguous.
		 */

		offset = validBox.y * pitch;
		memcpy(newPix32 + offset, masterPtr->pix32 + offset,
			(size_t) (validBox.height * pitch));

	    } else if ((validBox.width > 0) && (validBox.height > 0)) {
		/*
		 * Area to be copied is not contiguous - copy line by line.
		 */

		destPtr = newPix32 + (validBox.y * width + validBox.x) * 4;
		srcPtr = masterPtr->pix32 + (validBox.y * masterPtr->width
			+ validBox.x) * 4;
		for (h = validBox.height; h > 0; h--) {
		    memcpy(destPtr, srcPtr, (size_t) (validBox.width * 4));
		    destPtr += width * 4;
		    srcPtr += masterPtr->width * 4;
		}
	    }

	    ckfree((char *) masterPtr->pix32);
	}

	masterPtr->pix32 = newPix32;
	masterPtr->width = width;
	masterPtr->height = height;

	/*
	 * Dithering will be correct up to the end of the last pre-existing
	 * complete scanline.
	 */

	if ((validBox.x > 0) || (validBox.y > 0)) {
	    masterPtr->ditherX = 0;
	    masterPtr->ditherY = 0;
	} else if (validBox.width == width) {
	    if ((int) validBox.height < masterPtr->ditherY) {
		masterPtr->ditherX = 0;
		masterPtr->ditherY = validBox.height;
	    }
	} else if ((masterPtr->ditherY > 0)
		|| ((int) validBox.width < masterPtr->ditherX)) {
	    masterPtr->ditherX = validBox.width;
	    masterPtr->ditherY = 0;
	}
    }

    ToggleComplexAlphaIfNeeded(masterPtr);

    /*
     * Now adjust the sizes of the pixmaps for all of the instances.
     */

    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	TkImgPhotoInstanceSetSize(instancePtr);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MatchFileFormat --
 *
 *	This function is called to find a photo image file format handler
 *	which can parse the image data in the given file. If a user-specified
 *	format string is provided, only handlers whose names match a prefix of
 *	the format string are tried.
 *
 * Results:
 *	A standard TCL return value. If the return value is TCL_OK, a pointer
 *	to the image format record is returned in *imageFormatPtr, and the
 *	width and height of the image are returned in *widthPtr and
 *	*heightPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
MatchFileFormat(
    Tcl_Interp *interp,		/* Interpreter to use for reporting errors. */
    Tcl_Channel chan,		/* The image file, open for reading. */
    const char *fileName,	/* The name of the image file. */
    Tcl_Obj *formatObj,		/* User-specified format string, or NULL. */
    Tk_PhotoImageFormat **imageFormatPtr,
				/* A pointer to the photo image format record
				 * is returned here. */
    int *widthPtr, int *heightPtr,
				/* The dimensions of the image are returned
				 * here. */
    int *oldformat)		/* Returns 1 if the old image API is used. */
{
    int matched = 0, useoldformat = 0;
    Tk_PhotoImageFormat *formatPtr;
    ThreadSpecificData *tsdPtr =
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    const char *formatString = NULL;

    if (formatObj) {
	formatString = Tcl_GetString(formatObj);
    }

    /*
     * Scan through the table of file format handlers to find one which can
     * handle the image.
     */

    for (formatPtr = tsdPtr->formatList; formatPtr != NULL;
	    formatPtr = formatPtr->nextPtr) {
	if (formatObj != NULL) {
	    if (strncasecmp(formatString,
		    formatPtr->name, strlen(formatPtr->name)) != 0) {
		continue;
	    }
	    matched = 1;
	    if (formatPtr->fileMatchProc == NULL) {
		Tcl_AppendResult(interp, "-file option isn't supported for ",
			formatString, " images", NULL);
		return TCL_ERROR;
	    }
	}
	if (formatPtr->fileMatchProc != NULL) {
	    (void) Tcl_Seek(chan, Tcl_LongAsWide(0L), SEEK_SET);

	    if (formatPtr->fileMatchProc(chan, fileName, formatObj,
		    widthPtr, heightPtr, interp)) {
		if (*widthPtr < 1) {
		    *widthPtr = 1;
		}
		if (*heightPtr < 1) {
		    *heightPtr = 1;
		}
		break;
	    }
	}
    }
    if (formatPtr == NULL) {
	useoldformat = 1;
	for (formatPtr = tsdPtr->oldFormatList; formatPtr != NULL;
		formatPtr = formatPtr->nextPtr) {
	    if (formatString != NULL) {
		if (strncasecmp(formatString,
			formatPtr->name, strlen(formatPtr->name)) != 0) {
		    continue;
		}
		matched = 1;
		if (formatPtr->fileMatchProc == NULL) {
		    Tcl_AppendResult(interp, "-file option isn't supported",
			    " for ", formatString, " images", NULL);
		    return TCL_ERROR;
		}
	    }
	    if (formatPtr->fileMatchProc != NULL) {
		(void) Tcl_Seek(chan, Tcl_LongAsWide(0L), SEEK_SET);
		if (formatPtr->fileMatchProc(chan, fileName, (Tcl_Obj *)
			formatString, widthPtr, heightPtr, interp)) {
		    if (*widthPtr < 1) {
			*widthPtr = 1;
		    }
		    if (*heightPtr < 1) {
			*heightPtr = 1;
		    }
		    break;
		}
	    }
	}
    }

    if (formatPtr == NULL) {
	if ((formatObj != NULL) && !matched) {
	    Tcl_AppendResult(interp, "image file format \"", formatString,
		    "\" is not supported", NULL);
	} else {
	    Tcl_AppendResult(interp,
		    "couldn't recognize data in image file \"", fileName, "\"",
		    NULL);
	}
	return TCL_ERROR;
    }

    *imageFormatPtr = formatPtr;
    *oldformat = useoldformat;
    (void) Tcl_Seek(chan, Tcl_LongAsWide(0L), SEEK_SET);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MatchStringFormat --
 *
 *	This function is called to find a photo image file format handler
 *	which can parse the image data in the given string. If a
 *	user-specified format string is provided, only handlers whose names
 *	match a prefix of the format string are tried.
 *
 * Results:
 *	A standard TCL return value. If the return value is TCL_OK, a pointer
 *	to the image format record is returned in *imageFormatPtr, and the
 *	width and height of the image are returned in *widthPtr and
 *	*heightPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
MatchStringFormat(
    Tcl_Interp *interp,		/* Interpreter to use for reporting errors. */
    Tcl_Obj *data,		/* Object containing the image data. */
    Tcl_Obj *formatObj,		/* User-specified format string, or NULL. */
    Tk_PhotoImageFormat **imageFormatPtr,
				/* A pointer to the photo image format record
				 * is returned here. */
    int *widthPtr, int *heightPtr,
				/* The dimensions of the image are returned
				 * here. */
    int *oldformat)		/* Returns 1 if the old image API is used. */
{
    int matched = 0, useoldformat = 0;
    Tk_PhotoImageFormat *formatPtr;
    ThreadSpecificData *tsdPtr =
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    const char *formatString = NULL;

    if (formatObj) {
	formatString = Tcl_GetString(formatObj);
    }

    /*
     * Scan through the table of file format handlers to find one which can
     * handle the image.
     */

    for (formatPtr = tsdPtr->formatList; formatPtr != NULL;
	    formatPtr = formatPtr->nextPtr) {
	if (formatObj != NULL) {
	    if (strncasecmp(formatString,
		    formatPtr->name, strlen(formatPtr->name)) != 0) {
		continue;
	    }
	    matched = 1;
	    if (formatPtr->stringMatchProc == NULL) {
		Tcl_AppendResult(interp, "-data option isn't supported for ",
			formatString, " images", NULL);
		return TCL_ERROR;
	    }
	}
	if ((formatPtr->stringMatchProc != NULL)
		&& (formatPtr->stringReadProc != NULL)
		&& formatPtr->stringMatchProc(data, formatObj,
			widthPtr, heightPtr, interp)) {
	    break;
	}
    }

    if (formatPtr == NULL) {
	useoldformat = 1;
	for (formatPtr = tsdPtr->oldFormatList; formatPtr != NULL;
		formatPtr = formatPtr->nextPtr) {
	    if (formatObj != NULL) {
		if (strncasecmp(formatString,
			formatPtr->name, strlen(formatPtr->name)) != 0) {
		    continue;
		}
		matched = 1;
		if (formatPtr->stringMatchProc == NULL) {
		    Tcl_AppendResult(interp, "-data option isn't supported",
			    " for ", formatString, " images", NULL);
		    return TCL_ERROR;
		}
	    }
	    if ((formatPtr->stringMatchProc != NULL)
		    && (formatPtr->stringReadProc != NULL)
		    && formatPtr->stringMatchProc(
			    (Tcl_Obj *) Tcl_GetString(data),
			    (Tcl_Obj *) formatString,
			    widthPtr, heightPtr, interp)) {
		break;
	    }
	}
    }
    if (formatPtr == NULL) {
	if ((formatObj != NULL) && !matched) {
	    Tcl_AppendResult(interp, "image format \"", formatString,
		    "\" is not supported", NULL);
	} else {
	    Tcl_AppendResult(interp, "couldn't recognize image data", NULL);
	}
	return TCL_ERROR;
    }

    *imageFormatPtr = formatPtr;
    *oldformat = useoldformat;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FindPhoto --
 *
 *	This function is called to get an opaque handle (actually a
 *	PhotoMaster *) for a given image, which can be used in subsequent
 *	calls to Tk_PhotoPutBlock, etc. The `name' parameter is the name of
 *	the image.
 *
 * Results:
 *	The handle for the photo image, or NULL if there is no photo image
 *	with the name given.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_PhotoHandle
Tk_FindPhoto(
    Tcl_Interp *interp,		/* Interpreter (application) in which image
				 * exists. */
    const char *imageName)	/* Name of the desired photo image. */
{
    const Tk_ImageType *typePtr;
    ClientData clientData =
	    Tk_GetImageMasterData(interp, imageName, &typePtr);

    if ((typePtr == NULL) || (typePtr->name != tkPhotoImageType.name)) {
	return NULL;
    }
    return clientData;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoPutBlock --
 *
 *	This function is called to put image data into a photo image.
 *
 * Results:
 *	A standard Tcl result code.
 *
 * Side effects:
 *	The image data is stored. The image may be expanded. The Tk image code
 *	is informed that the image has changed. If the result code is
 *	TCL_ERROR, an error message will be placed in the interpreter (if
 *	non-NULL).
 *
 *----------------------------------------------------------------------
 */

int
Tk_PhotoPutBlock(
    Tcl_Interp *interp,		/* Interpreter for passing back error
				 * messages, or NULL. */
    Tk_PhotoHandle handle,	/* Opaque handle for the photo image to be
				 * updated. */
    register Tk_PhotoImageBlock *blockPtr,
				/* Pointer to a structure describing the pixel
				 * data to be copied into the image. */
    int x, int y,		/* Coordinates of the top-left pixel to be
				 * updated in the image. */
    int width, int height,	/* Dimensions of the area of the image to be
				 * updated. */
    int compRule)		/* Compositing rule to use when processing
				 * transparent pixels. */
{
    register PhotoMaster *masterPtr;
    int xEnd, yEnd, greenOffset, blueOffset, alphaOffset;
    int wLeft, hLeft, wCopy, hCopy, pitch;
    unsigned char *srcPtr, *srcLinePtr, *destPtr, *destLinePtr;
    int sourceIsSimplePhoto = compRule & SOURCE_IS_SIMPLE_ALPHA_PHOTO;
    XRectangle rect;

    masterPtr = (PhotoMaster *) handle;
    compRule &= ~SOURCE_IS_SIMPLE_ALPHA_PHOTO;

    if ((masterPtr->userWidth != 0) && ((x + width) > masterPtr->userWidth)) {
	width = masterPtr->userWidth - x;
    }
    if ((masterPtr->userHeight != 0)
	    && ((y + height) > masterPtr->userHeight)) {
	height = masterPtr->userHeight - y;
    }
    if ((width <= 0) || (height <= 0)) {
	return TCL_OK;
    }

    xEnd = x + width;
    yEnd = y + height;
    if ((xEnd > masterPtr->width) || (yEnd > masterPtr->height)) {
	int sameSrc = (blockPtr->pixelPtr == masterPtr->pix32);

	if (ImgPhotoSetSize(masterPtr, MAX(xEnd, masterPtr->width),
		MAX(yEnd, masterPtr->height)) == TCL_ERROR) {
	    if (interp != NULL) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, TK_PHOTO_ALLOC_FAILURE_MESSAGE, NULL);
	    }
	    return TCL_ERROR;
	}
	if (sameSrc) {
	    blockPtr->pixelPtr = masterPtr->pix32;
	    blockPtr->pitch = masterPtr->width * 4;
	}
    }

    if ((y < masterPtr->ditherY) || ((y == masterPtr->ditherY)
	    && (x < masterPtr->ditherX))) {
	/*
	 * The dithering isn't correct past the start of this block.
	 */

	masterPtr->ditherX = x;
	masterPtr->ditherY = y;
    }

    /*
     * If this image block could have different red, green and blue
     * components, mark it as a color image.
     */

    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
    alphaOffset = blockPtr->offset[3];
    if ((alphaOffset >= blockPtr->pixelSize) || (alphaOffset < 0)) {
	alphaOffset = 0;
	sourceIsSimplePhoto = 1;
    } else {
	alphaOffset -= blockPtr->offset[0];
    }
    if ((greenOffset != 0) || (blueOffset != 0)) {
	masterPtr->flags |= COLOR_IMAGE;
    }

    /*
     * Copy the data into our local 32-bit/pixel array. If we can do it with a
     * single memcpy, we do.
     */

    destLinePtr = masterPtr->pix32 + (y * masterPtr->width + x) * 4;
    pitch = masterPtr->width * 4;

    /*
     * Test to see if we can do the whole write in a single copy. This test is
     * probably too restrictive. We should also be able to do a memcpy if
     * pixelSize == 3 and alphaOffset == 0. Maybe other cases too.
     */

    if ((blockPtr->pixelSize == 4)
	    && (greenOffset == 1) && (blueOffset == 2) && (alphaOffset == 3)
	    && (width <= blockPtr->width) && (height <= blockPtr->height)
	    && ((height == 1) || ((x == 0) && (width == masterPtr->width)
		&& (blockPtr->pitch == pitch)))
	    && (compRule == TK_PHOTO_COMPOSITE_SET)) {
	memcpy(destLinePtr, blockPtr->pixelPtr + blockPtr->offset[0],
		(size_t) (height * width * 4));

	/*
	 * We know there's an alpha offset and we're setting the data, so skip
	 * directly to the point when we recompute the photo validity region.
	 */

	goto recalculateValidRegion;
    }

    /*
     * Copy and merge pixels according to the compositing rule.
     */

    for (hLeft = height; hLeft > 0;) {
	int pixelSize = blockPtr->pixelSize;
	int compRuleSet = (compRule == TK_PHOTO_COMPOSITE_SET);

	srcLinePtr = blockPtr->pixelPtr + blockPtr->offset[0];
	hCopy = MIN(hLeft, blockPtr->height);
	hLeft -= hCopy;
	for (; hCopy > 0; --hCopy) {
	    /*
	     * If the layout of the source line matches our memory layout and
	     * we're setting, we can just copy the bytes directly, which is
	     * much faster.
	     */

	    if ((pixelSize == 4) && (greenOffset == 1)
		    && (blueOffset == 2) && (alphaOffset == 3)
		    && (width <= blockPtr->width)
		    && compRuleSet) {
		memcpy(destLinePtr, srcLinePtr, (size_t) (width * 4));
		srcLinePtr += blockPtr->pitch;
		destLinePtr += pitch;
		continue;
	    }

	    /*
	     * Have to copy the slow way.
	     */

	    destPtr = destLinePtr;
	    for (wLeft = width; wLeft > 0;) {
		wCopy = MIN(wLeft, blockPtr->width);
		wLeft -= wCopy;
		srcPtr = srcLinePtr;

		/*
		 * But we might be lucky and be able to use fairly fast loops.
		 * It's worth checking...
		 */

		if (alphaOffset == 0) {
		    /*
		     * This is the non-alpha case, so can still be fairly
		     * fast. Note that in the non-alpha-source case, the
		     * compositing rule doesn't apply.
		     */

		    for (; wCopy>0 ; --wCopy, srcPtr+=pixelSize) {
			*destPtr++ = srcPtr[0];
			*destPtr++ = srcPtr[greenOffset];
			*destPtr++ = srcPtr[blueOffset];
			*destPtr++ = 255;
		    }
		    continue;
		} else if (compRuleSet) {
		    /*
		     * This is the SET compositing rule, which just replaces
		     * what was there before with the new data. This is
		     * another fairly fast case. No point in doing a memcpy();
		     * the order of channels is probably wrong.
		     */

		    for (; wCopy>0 ; --wCopy, srcPtr+=pixelSize) {
			*destPtr++ = srcPtr[0];
			*destPtr++ = srcPtr[greenOffset];
			*destPtr++ = srcPtr[blueOffset];
			*destPtr++ = srcPtr[alphaOffset];
		    }
		    continue;
		}

		/*
		 * Bother; need to consider the alpha value of each pixel to
		 * know what to do.
		 */

		for (; wCopy>0 ; --wCopy, srcPtr+=pixelSize) {
		    int alpha = srcPtr[alphaOffset];

		    if (alpha == 255 || !destPtr[3]) {
			/*
			 * Either the source is 100% opaque, or the
			 * destination is entirely blank. In all cases, we
			 * just set the destination to the source.
			 */

			*destPtr++ = srcPtr[0];
			*destPtr++ = srcPtr[greenOffset];
			*destPtr++ = srcPtr[blueOffset];
			*destPtr++ = alpha;
			continue;
		    }

		    /*
		     * Can still skip doing work if the source is 100%
		     * transparent at this point.
		     */

		    if (alpha) {
			int Alpha = destPtr[3];

			/*
			 * OK, there's real work to be done. Luckily, there's
			 * a substantial literature on what to do in this
			 * case. In particular, Porter and Duff have done a
			 * taxonomy of compositing rules, and the right one is
			 * the "Source Over" rule. This code implements that.
			 */

			destPtr[0] = PD_SRC_OVER(srcPtr[0], alpha, destPtr[0],
				Alpha);
			destPtr[1] = PD_SRC_OVER(srcPtr[greenOffset], alpha,
				destPtr[1], Alpha);
			destPtr[2] = PD_SRC_OVER(srcPtr[blueOffset], alpha,
				destPtr[2], Alpha);
			destPtr[3] = PD_SRC_OVER_ALPHA(alpha, Alpha);
		    }

		    destPtr += 4;
		}
	    }
	    srcLinePtr += blockPtr->pitch;
	    destLinePtr += pitch;
	}
    }

    /*
     * Add this new block to the region which specifies which data is valid.
     */

    if (alphaOffset) {
	/*
	 * This block is grossly inefficient. For each row in the image, it
	 * finds each continguous string of nontransparent pixels, then marks
	 * those areas as valid in the validRegion mask. This makes drawing
	 * very efficient, because of the way we use X: we just say, here's
	 * your mask, and here's your data. We need not worry about the
	 * current background color, etc. But this costs us a lot on the image
	 * setup. Still, image setup only happens once, whereas the drawing
	 * happens many times, so this might be the best way to go.
	 *
	 * An alternative might be to not set up this mask, and instead, at
	 * drawing time, for each transparent pixel, set its color to the
	 * color of the background behind that pixel. This is what I suspect
	 * most of programs do. However, they don't have to deal with the
	 * canvas, which could have many different background colors.
	 * Determining the correct bg color for a given pixel might be
	 * expensive.
	 */

	if (compRule != TK_PHOTO_COMPOSITE_OVERLAY) {
	    TkRegion workRgn;

	    /*
	     * Don't need this when using the OVERLAY compositing rule, which
	     * always strictly increases the valid region.
	     */

	recalculateValidRegion:
	    workRgn = TkCreateRegion();
	    rect.x = x;
	    rect.y = y;
	    rect.width = width;
	    rect.height = height;
	    TkUnionRectWithRegion(&rect, workRgn, workRgn);
	    TkSubtractRegion(masterPtr->validRegion, workRgn,
		    masterPtr->validRegion);
	    TkDestroyRegion(workRgn);
	}

	/*
	 * Factorize out the main part of the building of the region data to
	 * allow for more efficient per-platform implementations. [Bug 919066]
	 */

	TkpBuildRegionFromAlphaData(masterPtr->validRegion, (unsigned) x,
		(unsigned) y, (unsigned) width, (unsigned) height,
		masterPtr->pix32 + (y * masterPtr->width + x) * 4 + 3,
		4, (unsigned) masterPtr->width * 4);
    } else {
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;
	TkUnionRectWithRegion(&rect, masterPtr->validRegion,
		masterPtr->validRegion);
    }

    /*
     * Check if display code needs alpha blending...
     */

    if (!sourceIsSimplePhoto && (height == 1)) {
	/*
	 * Optimize the single span case if we can. This speeds up code that
	 * builds up large simple-alpha images by scan-lines or individual
	 * pixels. We don't negate COMPLEX_ALPHA in this case. [Bug 1409140]
	 * [Patch 1539990]
	 */

	if (!(masterPtr->flags & COMPLEX_ALPHA)) {
	    register int x1;

	    for (x1=x ; x1<x+width ; x1++) {
		register unsigned char newAlpha;

		destLinePtr = masterPtr->pix32 + (y*masterPtr->width + x1)*4;
		newAlpha = destLinePtr[3];
		if (newAlpha && newAlpha != 255) {
		    masterPtr->flags |= COMPLEX_ALPHA;
		    break;
		}
	    }
	}
    } else if ((alphaOffset != 0) || (masterPtr->flags & COMPLEX_ALPHA)) {
	/*
	 * Check for partial transparency if alpha pixels are specified, or
	 * rescan if we already knew such pixels existed. To restrict this
	 * Toggle to only checking the changed pixels requires knowing where
	 * the alpha pixels are.
	 */

	ToggleComplexAlphaIfNeeded(masterPtr);
    }

    /*
     * Update each instance.
     */

    Tk_DitherPhoto((Tk_PhotoHandle)masterPtr, x, y, width, height);

    /*
     * Tell the core image code that this image has changed.
     */

    Tk_ImageChanged(masterPtr->tkMaster, x, y, width, height,
	    masterPtr->width, masterPtr->height);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoPutZoomedBlock --
 *
 *	This function is called to put image data into a photo image, with
 *	possible subsampling and/or zooming of the pixels.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image data is stored. The image may be expanded. The Tk image code
 *	is informed that the image has changed.
 *
 *----------------------------------------------------------------------
 */

int
Tk_PhotoPutZoomedBlock(
    Tcl_Interp *interp,		/* Interpreter for passing back error
				 * messages, or NULL. */
    Tk_PhotoHandle handle,	/* Opaque handle for the photo image to be
				 * updated. */
    register Tk_PhotoImageBlock *blockPtr,
				/* Pointer to a structure describing the pixel
				 * data to be copied into the image. */
    int x, int y,		/* Coordinates of the top-left pixel to be
				 * updated in the image. */
    int width, int height,	/* Dimensions of the area of the image to be
				 * updated. */
    int zoomX, int zoomY,	/* Zoom factors for the X and Y axes. */
    int subsampleX, int subsampleY,
				/* Subsampling factors for the X and Y
				 * axes. */
    int compRule)		/* Compositing rule to use when processing
				 * transparent pixels. */
{
    register PhotoMaster *masterPtr = (PhotoMaster *) handle;
    int xEnd, yEnd, greenOffset, blueOffset, alphaOffset;
    int wLeft, hLeft, wCopy, hCopy, blockWid, blockHt;
    unsigned char *srcPtr, *srcLinePtr, *srcOrigPtr, *destPtr, *destLinePtr;
    int pitch, xRepeat, yRepeat, blockXSkip, blockYSkip, sourceIsSimplePhoto;
    XRectangle rect;

    if (zoomX==1 && zoomY==1 && subsampleX==1 && subsampleY==1) {
	return Tk_PhotoPutBlock(interp, handle, blockPtr, x, y, width, height,
		compRule);
    }

    sourceIsSimplePhoto = compRule & SOURCE_IS_SIMPLE_ALPHA_PHOTO;
    compRule &= ~SOURCE_IS_SIMPLE_ALPHA_PHOTO;

    if (zoomX <= 0 || zoomY <= 0) {
	return TCL_OK;
    }
    if ((masterPtr->userWidth != 0) && ((x + width) > masterPtr->userWidth)) {
	width = masterPtr->userWidth - x;
    }
    if ((masterPtr->userHeight != 0)
	    && ((y + height) > masterPtr->userHeight)) {
	height = masterPtr->userHeight - y;
    }
    if (width <= 0 || height <= 0) {
	return TCL_OK;
    }

    xEnd = x + width;
    yEnd = y + height;
    if ((xEnd > masterPtr->width) || (yEnd > masterPtr->height)) {
	int sameSrc = (blockPtr->pixelPtr == masterPtr->pix32);
	if (ImgPhotoSetSize(masterPtr, MAX(xEnd, masterPtr->width),
		MAX(yEnd, masterPtr->height)) == TCL_ERROR) {
	    if (interp != NULL) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, TK_PHOTO_ALLOC_FAILURE_MESSAGE, NULL);
	    }
	    return TCL_ERROR;
	}
	if (sameSrc) {
	    blockPtr->pixelPtr = masterPtr->pix32;
	    blockPtr->pitch = masterPtr->width * 4;
	}
    }

    if ((y < masterPtr->ditherY) || ((y == masterPtr->ditherY)
	    && (x < masterPtr->ditherX))) {
	/*
	 * The dithering isn't correct past the start of this block.
	 */

	masterPtr->ditherX = x;
	masterPtr->ditherY = y;
    }

    /*
     * If this image block could have different red, green and blue
     * components, mark it as a color image.
     */

    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
    alphaOffset = blockPtr->offset[3];
    if ((alphaOffset >= blockPtr->pixelSize) || (alphaOffset < 0)) {
	alphaOffset = 0;
	sourceIsSimplePhoto = 1;
    } else {
	alphaOffset -= blockPtr->offset[0];
    }
    if ((greenOffset != 0) || (blueOffset != 0)) {
	masterPtr->flags |= COLOR_IMAGE;
    }

    /*
     * Work out what area the pixel data in the block expands to after
     * subsampling and zooming.
     */

    blockXSkip = subsampleX * blockPtr->pixelSize;
    blockYSkip = subsampleY * blockPtr->pitch;
    if (subsampleX > 0) {
	blockWid = ((blockPtr->width + subsampleX - 1) / subsampleX) * zoomX;
    } else if (subsampleX == 0) {
	blockWid = width;
    } else {
	blockWid = ((blockPtr->width - subsampleX - 1) / -subsampleX) * zoomX;
    }
    if (subsampleY > 0) {
	blockHt = ((blockPtr->height + subsampleY - 1) / subsampleY) * zoomY;
    } else if (subsampleY == 0) {
	blockHt = height;
    } else {
	blockHt = ((blockPtr->height - subsampleY - 1) / -subsampleY) * zoomY;
    }

    /*
     * Copy the data into our local 32-bit/pixel array.
     */

    destLinePtr = masterPtr->pix32 + (y * masterPtr->width + x) * 4;
    srcOrigPtr = blockPtr->pixelPtr + blockPtr->offset[0];
    if (subsampleX < 0) {
	srcOrigPtr += (blockPtr->width - 1) * blockPtr->pixelSize;
    }
    if (subsampleY < 0) {
	srcOrigPtr += (blockPtr->height - 1) * blockPtr->pitch;
    }

    pitch = masterPtr->width * 4;
    for (hLeft = height; hLeft > 0; ) {
	hCopy = MIN(hLeft, blockHt);
	hLeft -= hCopy;
	yRepeat = zoomY;
	srcLinePtr = srcOrigPtr;
	for (; hCopy > 0; --hCopy) {
	    destPtr = destLinePtr;
	    for (wLeft = width; wLeft > 0;) {
		wCopy = MIN(wLeft, blockWid);
		wLeft -= wCopy;
		srcPtr = srcLinePtr;
		for (; wCopy > 0; wCopy -= zoomX) {
		    for (xRepeat = MIN(wCopy, zoomX); xRepeat > 0; xRepeat--) {
			int alpha = srcPtr[alphaOffset];/* Source alpha. */

			/*
			 * Common case (solid pixels) first
			 */

			if (!alphaOffset || (alpha == 255)) {
			    *destPtr++ = srcPtr[0];
			    *destPtr++ = srcPtr[greenOffset];
			    *destPtr++ = srcPtr[blueOffset];
			    *destPtr++ = 255;
			    continue;
 			}

			if (compRule==TK_PHOTO_COMPOSITE_SET || !destPtr[3]) {
			    /*
			     * Either this is the SET rule (we overwrite
			     * whatever is there) or the destination is
			     * entirely blank. In both cases, we just set the
			     * destination to the source.
			     */

			    *destPtr++ = srcPtr[0];
			    *destPtr++ = srcPtr[greenOffset];
			    *destPtr++ = srcPtr[blueOffset];
			    *destPtr++ = alpha;
			} else if (alpha) {
			    int Alpha = destPtr[3];	/* Destination
							 * alpha. */

			    destPtr[0] = PD_SRC_OVER(srcPtr[0], alpha,
				    destPtr[0], Alpha);
			    destPtr[1] = PD_SRC_OVER(srcPtr[greenOffset],alpha,
				    destPtr[1], Alpha);
			    destPtr[2] = PD_SRC_OVER(srcPtr[blueOffset], alpha,
				    destPtr[2], Alpha);
			    destPtr[3] = PD_SRC_OVER_ALPHA(alpha, Alpha);

			    destPtr += 4;
			} else {
			    destPtr += 4;
			}
		    }
		    srcPtr += blockXSkip;
		}
	    }
	    destLinePtr += pitch;
	    yRepeat--;
	    if (yRepeat <= 0) {
		srcLinePtr += blockYSkip;
		yRepeat = zoomY;
	    }
	}
    }

    /*
     * Recompute the region of data for which we have valid pixels to plot.
     */

    if (alphaOffset) {
	if (compRule != TK_PHOTO_COMPOSITE_OVERLAY) {
	    /*
	     * Don't need this when using the OVERLAY compositing rule, which
	     * always strictly increases the valid region.
	     */

	    TkRegion workRgn = TkCreateRegion();

	    rect.x = x;
	    rect.y = y;
	    rect.width = width;
	    rect.height = 1;
	    TkUnionRectWithRegion(&rect, workRgn, workRgn);
	    TkSubtractRegion(masterPtr->validRegion, workRgn,
		    masterPtr->validRegion);
	    TkDestroyRegion(workRgn);
	}

	TkpBuildRegionFromAlphaData(masterPtr->validRegion,
		(unsigned)x, (unsigned)y, (unsigned)width, (unsigned)height,
		&masterPtr->pix32[(y * masterPtr->width + x) * 4 + 3], 4,
		(unsigned) masterPtr->width * 4);
    } else {
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;
	TkUnionRectWithRegion(&rect, masterPtr->validRegion,
		masterPtr->validRegion);
    }

    /*
     * Check if display code needs alpha blending...
     */

    if (!sourceIsSimplePhoto && (width == 1) && (height == 1)) {
	/*
	 * Optimize the single pixel case if we can. This speeds up code that
	 * builds up large simple-alpha images by single pixels. We don't
	 * negate COMPLEX_ALPHA in this case. [Bug 1409140]
	 */
	if (!(masterPtr->flags & COMPLEX_ALPHA)) {
	    unsigned char newAlpha;

	    destLinePtr = masterPtr->pix32 + (y * masterPtr->width + x) * 4;
	    newAlpha = destLinePtr[3];

	    if (newAlpha && newAlpha != 255) {
		masterPtr->flags |= COMPLEX_ALPHA;
	    }
	}
    } else if ((alphaOffset != 0) || (masterPtr->flags & COMPLEX_ALPHA)) {
	/*
	 * Check for partial transparency if alpha pixels are specified, or
	 * rescan if we already knew such pixels existed. To restrict this
	 * Toggle to only checking the changed pixels requires knowing where
	 * the alpha pixels are.
	 */
	ToggleComplexAlphaIfNeeded(masterPtr);
    }

    /*
     * Update each instance.
     */

    Tk_DitherPhoto((Tk_PhotoHandle) masterPtr, x, y, width, height);

    /*
     * Tell the core image code that this image has changed.
     */

    Tk_ImageChanged(masterPtr->tkMaster, x, y, width, height, masterPtr->width,
	    masterPtr->height);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_DitherPhoto --
 *
 *	This function is called to update an area of each instance's pixmap by
 *	dithering the corresponding area of the image master.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The pixmap of each instance of this image gets updated. The fields in
 *	*masterPtr indicating which area of the image is correctly dithered
 *	get updated.
 *
 *----------------------------------------------------------------------
 */

void
Tk_DitherPhoto(
    Tk_PhotoHandle photo,	/* Image master whose instances are to be
				 * updated. */
    int x, int y,		/* Coordinates of the top-left pixel in the
				 * area to be dithered. */
    int width, int height)	/* Dimensions of the area to be dithered. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) photo;
    PhotoInstance *instancePtr;

    if ((width <= 0) || (height <= 0)) {
	return;
    }

    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	TkImgDitherInstance(instancePtr, x, y, width, height);
    }

    /*
     * Work out whether this block will be correctly dithered and whether it
     * will extend the correctly dithered region.
     */

    if (((y < masterPtr->ditherY)
	    || ((y == masterPtr->ditherY) && (x <= masterPtr->ditherX)))
	    && ((y + height) > (masterPtr->ditherY))) {
	/*
	 * This block starts inside (or immediately after) the correctly
	 * dithered region, so the first scan line at least will be right.
	 * Furthermore this block extends into scanline masterPtr->ditherY.
	 */

	if ((x == 0) && (width == masterPtr->width)) {
	    /*
	     * We are doing the full width, therefore the dithering will be
	     * correct to the end.
	     */

	    masterPtr->ditherX = 0;
	    masterPtr->ditherY = y + height;
	} else {
	    /*
	     * We are doing partial scanlines, therefore the
	     * correctly-dithered region will be extended by at most one scan
	     * line.
	     */

	    if (x <= masterPtr->ditherX) {
		masterPtr->ditherX = x + width;
		if (masterPtr->ditherX >= masterPtr->width) {
		    masterPtr->ditherX = 0;
		    masterPtr->ditherY++;
		}
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoBlank --
 *
 *	This function is called to clear an entire photo image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The valid region for the image is set to the null region. The generic
 *	image code is notified that the image has changed.
 *
 *----------------------------------------------------------------------
 */

void
Tk_PhotoBlank(
    Tk_PhotoHandle handle)	/* Handle for the image to be blanked. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) handle;
    PhotoInstance *instancePtr;

    masterPtr->ditherX = masterPtr->ditherY = 0;
    masterPtr->flags = 0;

    /*
     * The image has valid data nowhere.
     */

    if (masterPtr->validRegion != NULL) {
	TkDestroyRegion(masterPtr->validRegion);
    }
    masterPtr->validRegion = TkCreateRegion();

    /*
     * Clear out the 32-bit pixel storage array. Clear out the dithering error
     * arrays for each instance.
     */

    memset(masterPtr->pix32, 0,
	    (size_t) (masterPtr->width * masterPtr->height * 4));
    for (instancePtr = masterPtr->instancePtr; instancePtr != NULL;
	    instancePtr = instancePtr->nextPtr) {
	TkImgResetDither(instancePtr);
    }

    /*
     * Tell the core image code that this image has changed.
     */

    Tk_ImageChanged(masterPtr->tkMaster, 0, 0, masterPtr->width,
	    masterPtr->height, masterPtr->width, masterPtr->height);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoExpand --
 *
 *	This function is called to request that a photo image be expanded if
 *	necessary to be at least `width' pixels wide and `height' pixels high.
 *	If the user has declared a definite image size (using the -width and
 *	-height configuration options) then this call has no effect.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size of the photo image may change; if so the generic image code
 *	is informed.
 *
 *----------------------------------------------------------------------
 */

int
Tk_PhotoExpand(
    Tcl_Interp *interp,		/* Interpreter for passing back error
				 * messages, or NULL. */
    Tk_PhotoHandle handle,	/* Handle for the image to be expanded. */
    int width, int height)	/* Desired minimum dimensions of the image. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) handle;

    if (width <= masterPtr->width) {
	width = masterPtr->width;
    }
    if (height <= masterPtr->height) {
	height = masterPtr->height;
    }
    if ((width != masterPtr->width) || (height != masterPtr->height)) {
	if (ImgPhotoSetSize(masterPtr, MAX(width, masterPtr->width),
		MAX(height, masterPtr->height)) == TCL_ERROR) {
	    if (interp != NULL) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, TK_PHOTO_ALLOC_FAILURE_MESSAGE, NULL);
	    }
	    return TCL_ERROR;
	}
	Tk_ImageChanged(masterPtr->tkMaster, 0, 0, 0, 0, masterPtr->width,
		masterPtr->height);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoGetSize --
 *
 *	This function is called to obtain the current size of a photo image.
 *
 * Results:
 *	The image's width and height are returned in *widthp and *heightp.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tk_PhotoGetSize(
    Tk_PhotoHandle handle,	/* Handle for the image whose dimensions are
				 * requested. */
    int *widthPtr, int *heightPtr)
				/* The dimensions of the image are returned
				 * here. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) handle;

    *widthPtr = masterPtr->width;
    *heightPtr = masterPtr->height;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoSetSize --
 *
 *	This function is called to set size of a photo image. This call is
 *	equivalent to using the -width and -height configuration options.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size of the image may change; if so the generic image code is
 *	informed.
 *
 *----------------------------------------------------------------------
 */

int
Tk_PhotoSetSize(
    Tcl_Interp *interp,		/* Interpreter for passing back error
				 * messages, or NULL. */
    Tk_PhotoHandle handle,	/* Handle for the image whose size is to be
				 * set. */
    int width, int height)	/* New dimensions for the image. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) handle;

    masterPtr->userWidth = width;
    masterPtr->userHeight = height;
    if (ImgPhotoSetSize(masterPtr, ((width > 0) ? width: masterPtr->width),
	    ((height > 0) ? height: masterPtr->height)) == TCL_ERROR) {
	if (interp != NULL) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, TK_PHOTO_ALLOC_FAILURE_MESSAGE, NULL);
	}
	return TCL_ERROR;
    }
    Tk_ImageChanged(masterPtr->tkMaster, 0, 0, 0, 0,
	    masterPtr->width, masterPtr->height);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetPhotoValidRegion --
 *
 *	This function is called to get the part of the photo where there is
 *	valid data. Or, conversely, the part of the photo which is
 *	transparent.
 *
 * Results:
 *	A TkRegion value that indicates the current area of the photo that is
 *	valid. This value should not be used after any modification to the
 *	photo image.
 *
 * Side Effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkRegion
TkPhotoGetValidRegion(
    Tk_PhotoHandle handle)	/* Handle for the image whose valid region is
				 * to obtained. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) handle;

    return masterPtr->validRegion;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgGetPhoto --
 *
 *	This function is called to obtain image data from a photo image. This
 *	function fills in the Tk_PhotoImageBlock structure pointed to by
 *	`blockPtr' with details of the address and layout of the image data in
 *	memory.
 *
 * Results:
 *	A pointer to the allocated data which should be freed later. NULL if
 *	there is no need to free data because blockPtr->pixelPtr points
 *	directly to the image data.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
ImgGetPhoto(
    PhotoMaster *masterPtr,	/* Handle for the photo image from which image
				 * data is desired. */
    Tk_PhotoImageBlock *blockPtr,
				/* Information about the address and layout of
				 * the image data is returned here. */
    struct SubcommandOptions *optPtr)
{
    unsigned char *pixelPtr;
    int x, y, greenOffset, blueOffset, alphaOffset;

    Tk_PhotoGetImage((Tk_PhotoHandle) masterPtr, blockPtr);
    blockPtr->pixelPtr += optPtr->fromY * blockPtr->pitch
	    + optPtr->fromX * blockPtr->pixelSize;
    blockPtr->width = optPtr->fromX2 - optPtr->fromX;
    blockPtr->height = optPtr->fromY2 - optPtr->fromY;

    if (!(masterPtr->flags & COLOR_IMAGE) &&
	    (!(optPtr->options & OPT_BACKGROUND)
	    || ((optPtr->background->red == optPtr->background->green)
	    && (optPtr->background->red == optPtr->background->blue)))) {
	blockPtr->offset[0] = blockPtr->offset[1] = blockPtr->offset[2];
    }
    alphaOffset = 0;
    for (y = 0; y < blockPtr->height; y++) {
	pixelPtr = blockPtr->pixelPtr + (y * blockPtr->pitch)
		+ blockPtr->pixelSize - 1;
	for (x = 0; x < blockPtr->width; x++) {
	    if (*pixelPtr != 255) {
		alphaOffset = 3;
		break;
	    }
	    pixelPtr += blockPtr->pixelSize;
	}
	if (alphaOffset) {
	    break;
	}
    }
    if (!alphaOffset) {
	blockPtr->pixelPtr--;
	blockPtr->offset[0]++;
	blockPtr->offset[1]++;
	blockPtr->offset[2]++;
    }
    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
    if (((optPtr->options & OPT_BACKGROUND) && alphaOffset) ||
	    ((optPtr->options & OPT_GRAYSCALE) && (greenOffset||blueOffset))) {
	int newPixelSize,x,y;
	unsigned char *srcPtr, *destPtr;
	char *data;

	newPixelSize = (!(optPtr->options & OPT_BACKGROUND) && alphaOffset)
		? 2 : 1;
	if ((greenOffset||blueOffset) && !(optPtr->options & OPT_GRAYSCALE)) {
	    newPixelSize += 2;
	}
	data = attemptckalloc((unsigned int) (newPixelSize *
		blockPtr->width * blockPtr->height));
	if (data == NULL) {
	    return NULL;
	}
	srcPtr = blockPtr->pixelPtr + blockPtr->offset[0];
	destPtr = (unsigned char *) data;
	if (!greenOffset && !blueOffset) {
	    for (y = blockPtr->height; y > 0; y--) {
		for (x = blockPtr->width; x > 0; x--) {
		    *destPtr = *srcPtr;
		    srcPtr += blockPtr->pixelSize;
		    destPtr += newPixelSize;
		}
		srcPtr += blockPtr->pitch -
			blockPtr->width * blockPtr->pixelSize;
	    }
	} else if (optPtr->options & OPT_GRAYSCALE) {
	    for (y = blockPtr->height; y > 0; y--) {
		for (x = blockPtr->width; x > 0; x--) {
		    *destPtr = (unsigned char) ((srcPtr[0]*11 + srcPtr[1]*16
			    + srcPtr[2]*5 + 16) >> 5);
		    srcPtr += blockPtr->pixelSize;
		    destPtr += newPixelSize;
		}
		srcPtr += blockPtr->pitch -
			blockPtr->width * blockPtr->pixelSize;
	    }
	} else {
	    for (y = blockPtr->height; y > 0; y--) {
		for (x = blockPtr->width; x > 0; x--) {
		    destPtr[0] = srcPtr[0];
		    destPtr[1] = srcPtr[1];
		    destPtr[2] = srcPtr[2];
		    srcPtr += blockPtr->pixelSize;
		    destPtr += newPixelSize;
		}
		srcPtr += blockPtr->pitch -
			blockPtr->width * blockPtr->pixelSize;
	    }
	}
	srcPtr = blockPtr->pixelPtr + alphaOffset;
	destPtr = (unsigned char *) data;
	if (!alphaOffset) {
	    /*
	     * Nothing to be done.
	     */
	} else if (optPtr->options & OPT_BACKGROUND) {
	    if (newPixelSize > 2) {
		int red = optPtr->background->red>>8;
		int green = optPtr->background->green>>8;
		int blue = optPtr->background->blue>>8;

		for (y = blockPtr->height; y > 0; y--) {
		    for (x = blockPtr->width; x > 0; x--) {
			destPtr[0] += (unsigned char) (((255 - *srcPtr) *
				(red-destPtr[0])) / 255);
			destPtr[1] += (unsigned char) (((255 - *srcPtr) *
				(green-destPtr[1])) / 255);
			destPtr[2] += (unsigned char) (((255 - *srcPtr) *
				(blue-destPtr[2])) / 255);
			srcPtr += blockPtr->pixelSize;
			destPtr += newPixelSize;
		    }
		    srcPtr += blockPtr->pitch -
			    blockPtr->width * blockPtr->pixelSize;
		}
	    } else {
	 	int gray = (unsigned char) (((optPtr->background->red>>8) * 11
			+ (optPtr->background->green>>8) * 16
			+ (optPtr->background->blue>>8) * 5 + 16) >> 5);

		for (y = blockPtr->height; y > 0; y--) {
		    for (x = blockPtr->width; x > 0; x--) {
			destPtr[0] += ((255 - *srcPtr) *
				(gray-destPtr[0])) / 255;
			srcPtr += blockPtr->pixelSize;
			destPtr += newPixelSize;
		    }
		    srcPtr += blockPtr->pitch -
			    blockPtr->width * blockPtr->pixelSize;
		}
	    }
	} else {
	    destPtr += newPixelSize-1;
	    for (y = blockPtr->height; y > 0; y--) {
		for (x = blockPtr->width; x > 0; x--) {
		    *destPtr = *srcPtr;
		    srcPtr += blockPtr->pixelSize;
		    destPtr += newPixelSize;
		}
		srcPtr += blockPtr->pitch -
			blockPtr->width * blockPtr->pixelSize;
	    }
	}
	blockPtr->pixelPtr = (unsigned char *) data;
	blockPtr->pixelSize = newPixelSize;
	blockPtr->pitch = newPixelSize * blockPtr->width;
	blockPtr->offset[0] = 0;
	if (newPixelSize>2) {
	    blockPtr->offset[1]= 1;
	    blockPtr->offset[2]= 2;
	} else {
	    blockPtr->offset[1]= 0;
	    blockPtr->offset[2]= 0;
	}
	return data;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ImgStringWrite --
 *
 *	Default string write function. The data is formatted in the default
 *	format as accepted by the "<img> put" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
ImgStringWrite(
    Tcl_Interp *interp,
    Tcl_Obj *formatString,
    Tk_PhotoImageBlock *blockPtr)
{
    int greenOffset, blueOffset;
    Tcl_DString data;

    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];

    Tcl_DStringInit(&data);
    if ((blockPtr->width > 0) && (blockPtr->height > 0)) {
	char *line = ckalloc((unsigned) ((8 * blockPtr->width) + 2));
	int row, col;

	for (row=0; row<blockPtr->height; row++) {
	    unsigned char *pixelPtr = blockPtr->pixelPtr + blockPtr->offset[0]
		    + row * blockPtr->pitch;
	    char *linePtr = line;

	    for (col=0; col<blockPtr->width; col++) {
		sprintf(linePtr, " #%02x%02x%02x", *pixelPtr,
			pixelPtr[greenOffset], pixelPtr[blueOffset]);
		pixelPtr += blockPtr->pixelSize;
		linePtr += 8;
	    }
	    Tcl_DStringAppendElement(&data, line+1);
	}
	ckfree(line);
    }
    Tcl_DStringResult(interp, &data);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoGetImage --
 *
 *	This function is called to obtain image data from a photo image. This
 *	function fills in the Tk_PhotoImageBlock structure pointed to by
 *	`blockPtr' with details of the address and layout of the image data in
 *	memory.
 *
 * Results:
 *	TRUE (1) indicating that image data is available, for backwards
 *	compatibility with the old photo widget.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tk_PhotoGetImage(
    Tk_PhotoHandle handle,	/* Handle for the photo image from which image
				 * data is desired. */
    Tk_PhotoImageBlock *blockPtr)
				/* Information about the address and layout of
				 * the image data is returned here. */
{
    PhotoMaster *masterPtr = (PhotoMaster *) handle;

    blockPtr->pixelPtr = masterPtr->pix32;
    blockPtr->width = masterPtr->width;
    blockPtr->height = masterPtr->height;
    blockPtr->pitch = masterPtr->width * 4;
    blockPtr->pixelSize = 4;
    blockPtr->offset[0] = 0;
    blockPtr->offset[1] = 1;
    blockPtr->offset[2] = 2;
    blockPtr->offset[3] = 3;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * PhotoOptionFind --
 *
 *	Finds a specific Photo option.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	After commands are removed.
 *
 *----------------------------------------------------------------------
 */

typedef struct OptionAssocData {
    struct OptionAssocData *nextPtr;
				/* Pointer to next OptionAssocData. */
    Tcl_ObjCmdProc *command;	/* Command associated with this option. */
    char name[1];		/* Name of option (remaining chars) */
} OptionAssocData;

static Tcl_ObjCmdProc *
PhotoOptionFind(
    Tcl_Interp *interp,		/* Interpreter that is being deleted. */
    Tcl_Obj *obj)		/* Name of option to be found. */
{
    int length;
    const char *name = Tcl_GetStringFromObj(obj, &length);
    char *prevname = NULL;
    Tcl_ObjCmdProc *proc = NULL;
    OptionAssocData *list = Tcl_GetAssocData(interp, "photoOption", NULL);

    while (list != NULL) {
	if (strncmp(name, list->name, (unsigned) length) == 0) {
	    if (proc != NULL) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "ambiguous option \"", name,
			"\": must be ", prevname, NULL);
		while (list->nextPtr != NULL) {
		    Tcl_AppendResult(interp, prevname, ", ",NULL);
		    list = list->nextPtr;
		    prevname = list->name;
		}
		Tcl_AppendResult(interp, ", or", prevname, NULL);
		return NULL;
	    }
	    proc = list->command;
	    prevname = list->name;
	}
	list = list->nextPtr;
    }
    if (proc != NULL) {
	Tcl_ResetResult(interp);
    }
    return proc;
}

/*
 *----------------------------------------------------------------------
 *
 * PhotoOptionCleanupProc --
 *
 *	This function is invoked whenever an interpreter is deleted to cleanup
 *	the AssocData for "photoVisitor".
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Photo Visitor options are removed.
 *
 *----------------------------------------------------------------------
 */

static void
PhotoOptionCleanupProc(
    ClientData clientData,	/* Points to "photoVisitor" AssocData for the
				 * interpreter. */
    Tcl_Interp *interp)		/* Interpreter that is being deleted. */
{
    OptionAssocData *list = clientData;

    while (list != NULL) {
	register OptionAssocData *ptr = list;

	list = list->nextPtr;
	ckfree((char *) ptr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CreatePhotoOption --
 *
 *	This function may be invoked to add a new kind of photo option to the
 *	core photo command supported by Tk.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on, the new option will be useable by the photo command.
 *
 *--------------------------------------------------------------
 */

MODULE_SCOPE void
Tk_CreatePhotoOption(
    Tcl_Interp *interp,		/* Interpreter. */
    const char *name,		/* Option name. */
    Tcl_ObjCmdProc *proc)	/* Function to execute command. */
{
    OptionAssocData *typePtr2, *prevPtr, *ptr;
    OptionAssocData *list = Tcl_GetAssocData(interp, "photoOption", NULL);

    /*
     * If there's already a photo option with the given name, remove it.
     */

    for (typePtr2 = list, prevPtr = NULL; typePtr2 != NULL;
	    prevPtr = typePtr2, typePtr2 = typePtr2->nextPtr) {
	if (strcmp(typePtr2->name, name) == 0) {
	    if (prevPtr == NULL) {
		list = typePtr2->nextPtr;
	    } else {
		prevPtr->nextPtr = typePtr2->nextPtr;
	    }
	    ckfree((char *) typePtr2);
	    break;
	}
    }
    ptr = (OptionAssocData *) ckalloc(sizeof(OptionAssocData) + strlen(name));
    strcpy(&(ptr->name[0]), name);
    ptr->command = proc;
    ptr->nextPtr = list;
    Tcl_SetAssocData(interp, "photoOption", PhotoOptionCleanupProc, ptr);
}

/*
 *--------------------------------------------------------------
 *
 * TkPostscriptPhoto --
 *
 *	This function is called to output the contents of a photo image in
 *	Postscript by calling the Tk_PostscriptPhoto function.
 *
 * Results:
 *	Returns a standard Tcl return value.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ImgPhotoPostscript(
    ClientData clientData,	/* Handle for the photo image. */
    Tcl_Interp *interp,		/* Interpreter. */
    Tk_Window tkwin,		/* (unused) */
    Tk_PostscriptInfo psInfo,	/* Postscript info. */
    int x, int y,		/* First pixel to output. */
    int width, int height,	/* Width and height of area. */
    int prepass)		/* (unused) */
{
    Tk_PhotoImageBlock block;

    Tk_PhotoGetImage(clientData, &block);
    block.pixelPtr += y * block.pitch + x * block.pixelSize;

    return Tk_PostscriptPhoto(interp, &block, psInfo, width, height);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoPutBlock_NoComposite, Tk_PhotoPutZoomedBlock_NoComposite --
 *
 * These backward-compatability functions just exist to fill slots in stubs
 * table. For the behaviour of *_NoComposite, refer to the corresponding
 * function without the extra suffix, except that the compositing rule is
 * always "overlay" and the function always panics on memory-allocation
 * failure.
 *
 *----------------------------------------------------------------------
 */

void
Tk_PhotoPutBlock_NoComposite(
    Tk_PhotoHandle handle,
    Tk_PhotoImageBlock *blockPtr,
    int x, int y, int width, int height)
{
    if (Tk_PhotoPutBlock(NULL, handle, blockPtr, x, y, width, height,
	    TK_PHOTO_COMPOSITE_OVERLAY) != TCL_OK) {
	Tcl_Panic(TK_PHOTO_ALLOC_FAILURE_MESSAGE);
    }
}

void
Tk_PhotoPutZoomedBlock_NoComposite(
    Tk_PhotoHandle handle,
    Tk_PhotoImageBlock *blockPtr,
    int x, int y, int width, int height,
    int zoomX, int zoomY, int subsampleX, int subsampleY)
{
    if (Tk_PhotoPutZoomedBlock(NULL, handle, blockPtr, x, y, width, height,
	    zoomX, zoomY, subsampleX, subsampleY,
	    TK_PHOTO_COMPOSITE_OVERLAY) != TCL_OK) {
	Tcl_Panic(TK_PHOTO_ALLOC_FAILURE_MESSAGE);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_PhotoExpand_Panic, Tk_PhotoPutBlock_Panic,
 * Tk_PhotoPutZoomedBlock_Panic, Tk_PhotoSetSize_Panic
 *
 * Backward compatability functions for preserving the old behaviour (i.e.
 * panic on memory allocation failure) so that extensions do not need to be
 * significantly updated to take account of TIP #116. These call the new
 * interface (i.e. the interface without the extra suffix), but panic if an
 * error condition is returned.
 *
 *----------------------------------------------------------------------
 */

void
Tk_PhotoExpand_Panic(
    Tk_PhotoHandle handle,
    int width, int height)
{
    if (Tk_PhotoExpand(NULL, handle, width, height) != TCL_OK) {
	Tcl_Panic(TK_PHOTO_ALLOC_FAILURE_MESSAGE);
    }
}

void
Tk_PhotoPutBlock_Panic(
    Tk_PhotoHandle handle,
    Tk_PhotoImageBlock *blockPtr,
    int x, int y, int width, int height, int compRule)
{
    if (Tk_PhotoPutBlock(NULL, handle, blockPtr, x, y, width, height,
	    compRule) != TCL_OK) {
	Tcl_Panic(TK_PHOTO_ALLOC_FAILURE_MESSAGE);
    }
}

void
Tk_PhotoPutZoomedBlock_Panic(
    Tk_PhotoHandle handle, Tk_PhotoImageBlock *blockPtr,
    int x, int y, int width, int height,
    int zoomX, int zoomY, int subsampleX, int subsampleY,
    int compRule)
{
    if (Tk_PhotoPutZoomedBlock(NULL, handle, blockPtr, x, y, width, height,
	    zoomX, zoomY, subsampleX, subsampleY, compRule) != TCL_OK) {
	Tcl_Panic(TK_PHOTO_ALLOC_FAILURE_MESSAGE);
    }
}

void
Tk_PhotoSetSize_Panic(
    Tk_PhotoHandle handle,
    int width, int height)
{
    if (Tk_PhotoSetSize(NULL, handle, width, height) != TCL_OK) {
	Tcl_Panic(TK_PHOTO_ALLOC_FAILURE_MESSAGE);
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
