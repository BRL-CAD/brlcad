/*
 * tkImgFmtPIX.c --
 *
 *      A photo image file handler for PIX (BRL-CAD) files.
 *
 * Army copyright foo
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * Based on:
 * tkImgFmtPPM.c --
 *
 *	A photo image file handler for PPM (Portable PixMap) files.
 *
 * Copyright (c) 1994 The Australian National University.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Author: Paul Mackerras (paulus@cs.anu.edu.au),
 *	   Department of Computer Science,
 *	   Australian National University.
 */

static char rcsid[] = "@(#) tkImgFmtPIX.c 1.7 95/06/14 22:49:55";

#include "./tkInt.h"
#include "./tkPort.h"

/*
 * The maximum amount of memory to allocate for data read from the
 * file.  If we need more than this, we do it in pieces.
 */

#define MAX_MEMORY	10000		/* don't allocate > 10KB */

/*
 * The format record for the PIX file format:
 */

static int		FileMatchPIX _ANSI_ARGS_((FILE *f, char *fileName,
			    char *formatString, int *widthPtr,
			    int *heightPtr));
static int		FileReadPIX  _ANSI_ARGS_((Tcl_Interp *interp,
			    FILE *f, char *fileName, char *formatString,
			    Tk_PhotoHandle imageHandle, int destX, int destY,
			    int width, int height, int srcX, int srcY));
static int		FileWritePIX _ANSI_ARGS_((Tcl_Interp *interp,
			    char *fileName, char *formatString,
			    Tk_PhotoImageBlock *blockPtr));

Tk_PhotoImageFormat tkImgFmtPIX = {
    "PIX",			/* name */
    FileMatchPIX,		/* fileMatchProc */
    NULL,			/* stringMatchProc */
    FileReadPIX,		/* fileReadProc */
    NULL,			/* stringReadProc */
    FileWritePIX,		/* fileWriteProc */
    NULL,			/* stringWriteProc */
};

/*
 * Prototypes for local procedures defined in this file:
 */


/*
 *----------------------------------------------------------------------
 *
 * FileMatchPIX --
 *
 *	This procedure is invoked by the photo image type to see if
 *	a file contains image data in PIX format.
 *
 * Results:
 *	The return value is >0 if the format option seems to be requesting
 *	the PIX image type.
 *
 * Side effects:
 *	The access position in f may change.
 *
 *----------------------------------------------------------------------
 */

static int
FileMatchPIX(f, fileName, formatString, widthPtr, heightPtr)
    FILE *f;			/* The image file, open for reading. */
    char *fileName;		/* The name of the image file. */
    char *formatString;		/* User-specified format string, or NULL. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here if the file is a valid
				 * raw PIX file. */
{
    /* The format string must be nonnull and it must contain the word "pix". */
    /* If the user also specified the dimensions in the format string,
       use those.  Otherwise, guess from the file size. */

    if (formatString == NULL)
	return 0;

    if (strstr(formatString, "pix") == NULL &&
	strstr(formatString, "PIX") == NULL)
	return 0;

    if (bn_common_name_size(widthPtr, heightPtr, formatString) <= 0)
	if (bn_common_file_size(widthPtr, heightPtr, fileName, 3) <= 0)
	    return 0;

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * FileReadPIX --
 *
 *	This procedure is called by the photo image type to read
 *	PIX format data from a file and write it into a given
 *	photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	The access position in file f is changed, and new data is
 *	added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int
FileReadPIX(interp, f, fileName, formatString, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    FILE *f;			/* The image file, open for reading. */
    char *fileName;		/* The name of the image file. */
    char *formatString;		/* User-specified format string, or NULL. */
    Tk_PhotoHandle imageHandle;	/* The photo image to write into. */
    int destX, destY;		/* Coordinates of top-left pixel in
				 * photo image to be written to. */
    int width, height;		/* Dimensions of block of photo image to
				 * be written to. */
    int srcX, srcY;		/* Coordinates of top-left pixel to be used
				 * in image being read. */
{
    int fileWidth, fileHeight;
    int nBytes, h, type, count;
    unsigned char *pixelPtr;
    Tk_PhotoImageBlock block;

    /* Determine dimensions of file. */

    if (bn_common_name_size(&fileWidth, &fileHeight, formatString) <= 0)
	if (bn_common_file_size(&fileWidth, &fileHeight, fileName, 3) <= 0) {
	    Tcl_AppendResult(interp, "cannot determine dimensions of \"",
			     fileName, "\": please use -format pix-w#-n#",
			     NULL);
	    return TCL_ERROR;
	}

    if ((fileWidth <= 0) || (fileHeight <= 0)) {
	Tcl_AppendResult(interp, "PIX image file \"", fileName,
			 "\" has dimension(s) <= 0", (char *) NULL);
	return TCL_ERROR;
    }

    if ((srcX + width) > fileWidth) {
	width = fileWidth - srcX;
    }
    if ((srcY + height) > fileHeight) {
	height = fileHeight - srcY;
    }
    if ((width <= 0) || (height <= 0)
	|| (srcX >= fileWidth) || (srcY >= fileHeight)) {
	return TCL_OK;
    }

    block.pixelSize = 3;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.width = width;
    block.pitch = block.pixelSize * fileWidth;

    Tk_PhotoExpand(imageHandle, destX + width, destY + height);

    if ((srcY + height) < fileHeight) {
	fseek(f, (long) ((fileHeight - srcY - height) * block.pitch),
	      SEEK_CUR);
    }

    nBytes = block.pitch;
    pixelPtr = (unsigned char *) bu_malloc((unsigned) nBytes,
					   "PIX image buffer");
    block.pixelPtr = pixelPtr + srcX * block.pixelSize;

    for (h = height; h > 0; h--) {
	count = fread(pixelPtr, 1, (unsigned) nBytes, f);
	if (count != nBytes) {
	    Tcl_AppendResult(interp, "error reading PIX image file \"",
		    fileName, "\": ",
		    feof(f) ? "not enough data" : Tcl_PosixError(interp),
		    (char *) NULL);
	    bu_free((char *) pixelPtr, "PIX image");
	    return TCL_ERROR;
	}
	block.height = 1;
	Tk_PhotoPutBlock(imageHandle, &block, destX, destY+h-1, width, 1);
    }

    bu_free((char *) pixelPtr, "PIX image buffer");
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FileWritePIX --
 *
 *	This procedure is invoked to write image data to a file in PIX
 *	format.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	Data is written to the file given by "fileName".
 *
 *----------------------------------------------------------------------
 */

static int
FileWritePIX(interp, fileName, formatString, blockPtr)
    Tcl_Interp *interp;
    char *fileName;
    char *formatString;
    Tk_PhotoImageBlock *blockPtr;
{
    FILE *f;
    int w, h;
    int greenOffset, blueOffset, nBytes;
    unsigned char *pixelPtr, *pixLinePtr;

    if ((f = fopen(fileName, "wb")) == NULL) {
	Tcl_AppendResult(interp, fileName, ": ", Tcl_PosixError(interp),
		(char *)NULL);
	return TCL_ERROR;
    }

    pixLinePtr = blockPtr->pixelPtr + blockPtr->offset[0] +
	         (blockPtr->height-1)*blockPtr->pitch;
    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];

    for (h = blockPtr->height; h > 0; h--) {
	pixelPtr = pixLinePtr;
	for (w = blockPtr->width; w > 0; w--) {
	    if ((putc(pixelPtr[0], f) == EOF)
		|| (putc(pixelPtr[greenOffset], f) == EOF)
		|| (putc(pixelPtr[blueOffset], f) == EOF)) {
		goto writeerror;
	    }
	    pixelPtr += blockPtr->pixelSize;
	}
	pixLinePtr -= blockPtr->pitch;
    }

    if (fclose(f) == 0) {
	return TCL_OK;
    }
    f = NULL;

 writeerror:
    Tcl_AppendResult(interp, "error writing \"", fileName, "\": ",
	    Tcl_PosixError(interp), (char *) NULL);
    if (f != NULL) {
	fclose(f);
    }
    return TCL_ERROR;
}
