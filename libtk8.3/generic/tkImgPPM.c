/*
 * tkImgPPM.c --
 *
 *	A photo image file handler for PPM (Portable PixMap) files.
 *
 * Copyright (c) 1994 The Australian National University.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Author: Paul Mackerras (paulus@cs.anu.edu.au),
 *	   Department of Computer Science,
 *	   Australian National University.
 *
 * RCS: @(#) $Id$
 */

#define USE_OLD_IMAGE

#include "tkInt.h"
#include "tkPort.h"

/*
 * The maximum amount of memory to allocate for data read from the
 * file.  If we need more than this, we do it in pieces.
 */

#define MAX_MEMORY	10000		/* don't allocate > 10KB */

/*
 * Define PGM and PPM, i.e. gray images and color images.
 */

#define PGM 1
#define PPM 2

/*
 * The format record for the PPM file format:
 */

static int		FileMatchPPM _ANSI_ARGS_((Tcl_Channel chan,
			    char *fileName, char *formatString,
			    int *widthPtr, int *heightPtr));
static int		FileReadPPM  _ANSI_ARGS_((Tcl_Interp *interp,
			    Tcl_Channel chan, char *fileName,
			    char *formatString, Tk_PhotoHandle imageHandle,
			    int destX, int destY, int width, int height,
			    int srcX, int srcY));
static int		FileWritePPM _ANSI_ARGS_((Tcl_Interp *interp,
			    char *fileName, char *formatString,
			    Tk_PhotoImageBlock *blockPtr));

Tk_PhotoImageFormat tkImgFmtPPM = {
    "PPM",			/* name */
    FileMatchPPM,		/* fileMatchProc */
    NULL,			/* stringMatchProc */
    FileReadPPM,		/* fileReadProc */
    NULL,			/* stringReadProc */
    FileWritePPM,		/* fileWriteProc */
    NULL,			/* stringWriteProc */
};

/*
 * Prototypes for local procedures defined in this file:
 */

static int		ReadPPMFileHeader _ANSI_ARGS_((Tcl_Channel chan,
			    int *widthPtr, int *heightPtr,
			    int *maxIntensityPtr));

/*
 *----------------------------------------------------------------------
 *
 * FileMatchPPM --
 *
 *	This procedure is invoked by the photo image type to see if
 *	a file contains image data in PPM format.
 *
 * Results:
 *	The return value is >0 if the first characters in file "f" look
 *	like PPM data, and 0 otherwise.
 *
 * Side effects:
 *	The access position in f may change.
 *
 *----------------------------------------------------------------------
 */

static int
FileMatchPPM(chan, fileName, formatString, widthPtr, heightPtr)
    Tcl_Channel chan;		/* The image file, open for reading. */
    char *fileName;		/* The name of the image file. */
    char *formatString;		/* User-specified format string, or NULL. */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here if the file is a valid
				 * raw PPM file. */
{
    int dummy;

    return ReadPPMFileHeader(chan, widthPtr, heightPtr, &dummy);
}

/*
 *----------------------------------------------------------------------
 *
 * FileReadPPM --
 *
 *	This procedure is called by the photo image type to read
 *	PPM format data from a file and write it into a given
 *	photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in the interp's result.
 *
 * Side effects:
 *	The access position in file f is changed, and new data is
 *	added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int
FileReadPPM(interp, chan, fileName, formatString, imageHandle, destX, destY,
	width, height, srcX, srcY)
    Tcl_Interp *interp;		/* Interpreter to use for reporting errors. */
    Tcl_Channel chan;		/* The image file, open for reading. */
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
    int fileWidth, fileHeight, maxIntensity;
    int nLines, nBytes, h, type, count;
    unsigned char *pixelPtr;
    Tk_PhotoImageBlock block;

    type = ReadPPMFileHeader(chan, &fileWidth, &fileHeight, &maxIntensity);
    if (type == 0) {
	Tcl_AppendResult(interp, "couldn't read raw PPM header from file \"",
		fileName, "\"", NULL);
	return TCL_ERROR;
    }
    if ((fileWidth <= 0) || (fileHeight <= 0)) {
	Tcl_AppendResult(interp, "PPM image file \"", fileName,
		"\" has dimension(s) <= 0", (char *) NULL);
	return TCL_ERROR;
    }
    if ((maxIntensity <= 0) || (maxIntensity >= 256)) {
	char buffer[TCL_INTEGER_SPACE];

	sprintf(buffer, "%d", maxIntensity);
	Tcl_AppendResult(interp, "PPM image file \"", fileName,
		"\" has bad maximum intensity value ", buffer,
		(char *) NULL);
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

    if (type == PGM) {
        block.pixelSize = 1;
        block.offset[0] = 0;
	block.offset[1] = 0;
	block.offset[2] = 0;
    }
    else {
        block.pixelSize = 3;
        block.offset[0] = 0;
	block.offset[1] = 1;
	block.offset[2] = 2;
    }
    block.offset[3] = 0;
    block.width = width;
    block.pitch = block.pixelSize * fileWidth;

    Tk_PhotoExpand(imageHandle, destX + width, destY + height);

    if (srcY > 0) {
	Tcl_Seek(chan, (srcY * block.pitch), SEEK_CUR);
    }

    nLines = (MAX_MEMORY + block.pitch - 1) / block.pitch;
    if (nLines > height) {
	nLines = height;
    }
    if (nLines <= 0) {
	nLines = 1;
    }
    nBytes = nLines * block.pitch;
    pixelPtr = (unsigned char *) ckalloc((unsigned) nBytes);
    block.pixelPtr = pixelPtr + srcX * block.pixelSize;

    for (h = height; h > 0; h -= nLines) {
	if (nLines > h) {
	    nLines = h;
	    nBytes = nLines * block.pitch;
	}
	count = Tcl_Read(chan, (char *) pixelPtr, nBytes);
	if (count != nBytes) {
	    Tcl_AppendResult(interp, "error reading PPM image file \"",
		    fileName, "\": ",
		    Tcl_Eof(chan) ? "not enough data" : Tcl_PosixError(interp),
		    (char *) NULL);
	    ckfree((char *) pixelPtr);
	    return TCL_ERROR;
	}
	if (maxIntensity != 255) {
	    unsigned char *p;

	    for (p = pixelPtr; count > 0; count--, p++) {
		*p = (((int) *p) * 255)/maxIntensity;
	    }
	}
	block.height = nLines;
	Tk_PhotoPutBlock(imageHandle, &block, destX, destY, width, nLines);
	destY += nLines;
    }

    ckfree((char *) pixelPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FileWritePPM --
 *
 *	This procedure is invoked to write image data to a file in PPM
 *	format.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in the interp's result.
 *
 * Side effects:
 *	Data is written to the file given by "fileName".
 *
 *----------------------------------------------------------------------
 */

static int
FileWritePPM(interp, fileName, formatString, blockPtr)
    Tcl_Interp *interp;
    char *fileName;
    char *formatString;
    Tk_PhotoImageBlock *blockPtr;
{
    Tcl_Channel chan;
    int w, h;
    int greenOffset, blueOffset, nBytes;
    unsigned char *pixelPtr, *pixLinePtr;
    char header[16 + TCL_INTEGER_SPACE * 2];

    chan = Tcl_OpenFileChannel(interp, fileName, "w", 0666);
    if (chan == NULL) {
	return TCL_ERROR;
    }

    if (Tcl_SetChannelOption(interp, chan, "-translation", "binary")
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tcl_SetChannelOption(interp, chan, "-encoding", "binary")
	    != TCL_OK) {
	return TCL_ERROR;
    }
    
    sprintf(header, "P6\n%d %d\n255\n", blockPtr->width, blockPtr->height);
    Tcl_Write(chan, header, -1);
	
    pixLinePtr = blockPtr->pixelPtr + blockPtr->offset[0];
    greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
    blueOffset = blockPtr->offset[2] - blockPtr->offset[0];

    if ((greenOffset == 1) && (blueOffset == 2) && (blockPtr->pixelSize == 3)
	    && (blockPtr->pitch == (blockPtr->width * 3))) {
	nBytes = blockPtr->height * blockPtr->pitch;
	if (Tcl_Write(chan, (char *) pixLinePtr, nBytes) != nBytes) {
	    goto writeerror;
	}
    } else {
	for (h = blockPtr->height; h > 0; h--) {
	    pixelPtr = pixLinePtr;
	    for (w = blockPtr->width; w > 0; w--) {
		if ((Tcl_Write(chan, (char *) &pixelPtr[0], 1) == -1)
			|| (Tcl_Write(chan, (char *) &pixelPtr[greenOffset], 1) == -1)
			|| (Tcl_Write(chan, (char *) &pixelPtr[blueOffset], 1) == -1)) {
		    goto writeerror;
		}
		pixelPtr += blockPtr->pixelSize;
	    }
	    pixLinePtr += blockPtr->pitch;
	}
    }

    if (Tcl_Close(NULL, chan) == 0) {
	return TCL_OK;
    }
    chan = NULL;

 writeerror:
    Tcl_AppendResult(interp, "error writing \"", fileName, "\": ",
	    Tcl_PosixError(interp), (char *) NULL);
    if (chan != NULL) {
	Tcl_Close(NULL, chan);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ReadPPMFileHeader --
 *
 *	This procedure reads the PPM header from the beginning of a
 *	PPM file and returns information from the header.
 *
 * Results:
 *	The return value is PGM if file "f" appears to start with
 *	a valid PGM header, PPM if "f" appears to start with a valid
 *      PPM header, and 0 otherwise.  If the header is valid,
 *	then *widthPtr and *heightPtr are modified to hold the
 *	dimensions of the image and *maxIntensityPtr is modified to
 *	hold the value of a "fully on" intensity value.
 *
 * Side effects:
 *	The access position in f advances.
 *
 *----------------------------------------------------------------------
 */

static int
ReadPPMFileHeader(chan, widthPtr, heightPtr, maxIntensityPtr)
    Tcl_Channel chan;		/* Image file to read the header from */
    int *widthPtr, *heightPtr;	/* The dimensions of the image are
				 * returned here. */
    int *maxIntensityPtr;	/* The maximum intensity value for
				 * the image is stored here. */
{
#define BUFFER_SIZE 1000
    char buffer[BUFFER_SIZE];
    int i, numFields;
    int type = 0;
    char c;

    /*
     * Read 4 space-separated fields from the file, ignoring
     * comments (any line that starts with "#").
     */

    if (Tcl_Read(chan, &c, 1) != 1) {
	return 0;
    }
    i = 0;
    for (numFields = 0; numFields < 4; numFields++) {
	/*
	 * Skip comments and white space.
	 */

	while (1) {
	    while (isspace(UCHAR(c))) {
		if (Tcl_Read(chan, &c, 1) != 1) {
		    return 0;
		}
	    }
	    if (c != '#') {
		break;
	    }
	    do {
		if (Tcl_Read(chan, &c, 1) != 1) {
		    return 0;
		}
	    } while (c != '\n');
	}

	/*
	 * Read a field (everything up to the next white space).
	 */

	while (!isspace(UCHAR(c))) {
	    if (i < (BUFFER_SIZE-2)) {
		buffer[i] = c;
		i++;
	    }
	    if (Tcl_Read(chan, &c, 1) != 1) {
		goto done;
	    }
	}
	if (i < (BUFFER_SIZE-1)) {
	    buffer[i] = ' ';
	    i++;
	}
    }
    done:
    buffer[i] = 0;

    /*
     * Parse the fields, which are: id, width, height, maxIntensity.
     */

    if (strncmp(buffer, "P6 ", 3) == 0) {
	type = PPM;
    } else if (strncmp(buffer, "P5 ", 3) == 0) {
	type = PGM;
    } else {
	return 0;
    }
    if (sscanf(buffer+3, "%d %d %d", widthPtr, heightPtr, maxIntensityPtr)
	    != 3) {
	return 0;
    }
    return type;
}
