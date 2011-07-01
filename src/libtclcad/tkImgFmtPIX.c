/*                   T K I M G F M T P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file libtclcad/tkImgFmtPIX.c
 *
 *      A photo image file handler for BRL-CAD(tm) ".pix" format files.
 *
 * Based on:
 *	tkImgFmtPPM.c --
 *
 *	A photo image file handler for PPM (Portable PixMap) files.
 *
 * Questionable whether they actually hold copyright or are simply
 * being given credit:
 *
 * Copyright (c) 1994 The Australian National University.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * Author: Paul Mackerras (paulus@cs.anu.edu.au),
 *	   Department of Computer Science,
 *	   Australian National University.
 */

#include "common.h"

/* make sure we have tk if we're going to compile this up */
#ifdef HAVE_TK

#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"
#include "tk.h"


/* we need tk version numbers for compatibility compilation */
#if !defined(TK_MAJOR_VERSION) || (TK_MAJOR_VERSION != 8)
#  error "Expecting Tk 8"
#endif


/*
 * The format record for the PIX file format:
 */

static int		FileMatchPIX _ANSI_ARGS_((Tcl_Channel chan,
						  const char *fileName,
						  Tcl_Obj *format, int *widthPtr,
						  int *heightPtr, Tcl_Interp *interp));
static int		FileReadPIX  _ANSI_ARGS_((Tcl_Interp *interp,
						  Tcl_Channel chan,
						  const char *fileName, Tcl_Obj *formatString,
						  Tk_PhotoHandle imageHandle, int destX, int destY,
						  int width, int height, int srcX, int srcY));
static int		FileWritePIX _ANSI_ARGS_((Tcl_Interp *interp,
						  const char *fileName, Tcl_Obj *formatString,
						  Tk_PhotoImageBlock *blockPtr));

Tk_PhotoImageFormat tkImgFmtPIX = {
    "PIX",			/* name */
    FileMatchPIX,		/* fileMatchProc */
    NULL,			/* stringMatchProc */
    FileReadPIX,		/* fileReadProc */
    NULL,			/* stringReadProc */
    FileWritePIX,		/* fileWriteProc */
    NULL,			/* stringWriteProc */
    NULL			/* nextPtr/tk-private */
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
FileMatchPIX(Tcl_Channel UNUSED(chan), const char *fileName, Tcl_Obj *format, int *widthPtr, int *heightPtr, Tcl_Interp *interp)

    /* The name of the image file. */
    /* User-specified format string, or NULL. */
    /* The dimensions of the image are
     * returned here if the file is a valid
     * raw PIX file. */

{
    /* The format string must be nonnull and it must contain the word "pix". */
    /* If the user also specified the dimensions in the format string,
       use those.  Otherwise, guess from the file size. */
    char *formatString;
    int len;
    size_t width, height;

    if (format == NULL || interp == NULL)
	return 0;

    formatString = Tcl_GetStringFromObj(format, &len);
    if (formatString == NULL) return 0;

    if (strstr(formatString, "pix") == NULL &&
	strstr(formatString, "PIX") == NULL)
	return 0;

    if (fb_common_name_size(&width, &height, formatString) <= 0) {
	if (fb_common_file_size(&width, &height, fileName, 3) <= 0) {
	    return 0;
	}
    }

    *widthPtr = (int)width;
    *heightPtr = (int)height;

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
 *	then an error message is left in interp.
 *
 * Side effects:
 *	The access position in file f is changed, and new data is
 *	added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int
FileReadPIX(Tcl_Interp *interp, Tcl_Channel chan, const char *fileName, Tcl_Obj *format, Tk_PhotoHandle imageHandle, int destX, int destY, int width, int height, int srcX, int srcY)
    /* Interpreter to use for reporting errors. */

    /* The name of the image file. */
    /* User-specified format string, or NULL. */
    /* The photo image to write into. */
    /* Coordinates of top-left pixel in
     * photo image to be written to. */
    /* Dimensions of block of photo image to
     * be written to. */
    /* Coordinates of top-left pixel to be used
     * in image being read. */
{
    size_t fileWidth, fileHeight;
    int nBytes, h, count;
    unsigned char *pixelPtr;
    Tk_PhotoImageBlock block;
    char *formatString;
    int len;

    /* Determine dimensions of file. */

    formatString = Tcl_GetStringFromObj(format, &len);

    if (fb_common_name_size(&fileWidth, &fileHeight, formatString) <= 0)
	if (fb_common_file_size(&fileWidth, &fileHeight, fileName, 3) <= 0) {
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

    if ((size_t)(srcX + width) > fileWidth) {
	width = fileWidth - srcX;
    }
    if ((size_t)(srcY + height) > fileHeight) {
	height = fileHeight - srcY;
    }
    if ((width <= 0) || (height <= 0)
	|| ((size_t)srcX >= fileWidth) || ((size_t)srcY >= fileHeight)) {
	return TCL_OK;
    }

    block.pixelSize = 3;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.width = width;
    block.pitch = block.pixelSize * fileWidth;

#if TK_MINOR_VERSION < 5
    Tk_PhotoExpand(imageHandle, destX + width, destY + height);
#else
    Tk_PhotoExpand(interp, imageHandle, destX + width, destY + height);
#endif

    if ((size_t)(srcY + height) < fileHeight) {
	Tcl_Seek( chan, (long) ((fileHeight - srcY - height) * block.pitch),
		  SEEK_CUR );

    }

    nBytes = block.pitch;
    pixelPtr = (unsigned char *) bu_malloc((unsigned) nBytes,
					   "PIX image buffer");
    block.pixelPtr = pixelPtr + srcX * block.pixelSize;

    for (h = height; h > 0; h--) {
	count = Tcl_Read( chan, (char *)pixelPtr, nBytes );
	if (count != nBytes) {
	    Tcl_AppendResult(interp, "error reading PIX image file \"",
			     fileName, "\": ",
			     Tcl_Eof(chan) ? "not enough data" : Tcl_PosixError(interp),
			     (char *) NULL);
	    bu_free((char *) pixelPtr, "PIX image");
	    return TCL_ERROR;
	}
	block.height = 1;
#if TK_MINOR_VERSION < 5
	Tk_PhotoPutBlock(imageHandle, &block, destX, destY+h-1, width, height, 1);
#else
	Tk_PhotoPutBlock(interp, imageHandle, &block, destX, destY+h-1, width, height, 1);
#endif
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
 *	then an error message is left in interp.
 *
 * Side effects:
 *	Data is written to the file given by "fileName".
 *
 *----------------------------------------------------------------------
 */

static int
FileWritePIX(Tcl_Interp *interp, const char *fileName, Tcl_Obj *format, Tk_PhotoImageBlock *blockPtr)
{
    FILE *f;
    int w, h;
    int greenOffset, blueOffset;
    unsigned char *pixelPtr, *pixLinePtr;

    if ((f = fopen(fileName, "wb")) == NULL) {
	Tcl_AppendResult(interp, fileName, ": ", Tcl_PosixError(interp), (char *)NULL);
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
		|| (putc(pixelPtr[blueOffset], f) == EOF))
	    {
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
    Tcl_AppendResult(interp, "error writing \"", fileName, "\" as format [", format, "]: ", Tcl_PosixError(interp), (char *) NULL);
    if (f != NULL) {
	fclose(f);
    }
    return TCL_ERROR;
}

#endif /* HAVE_TK */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
