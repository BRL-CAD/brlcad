/*
 * tkImgPNG.c --
 *
 *		A Tk photo image file handler for PNG files.  Requires zlib.
 *
 * Copyright (c) 2006 Muonics, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include <stdlib.h>
#include <memory.h>
#include <limits.h>

#include <zlib.h>
#include <math.h>
#include "tcl.h"
#include "tk.h"

#define	PNG_INT32(a,b,c,d)	\
	(((long)(a) << 24) | ((long)(b) << 16) | ((long)(c) << 8) | (long)(d))
#define	PNG_BLOCK_SZ		1024		/* Process up to 1k at a time */
#define PNG_MIN(a, b) (((a) < (b)) ? (a) : (b))

/* Every PNG image starts with the following 8-byte signature */

#define PNG_SIG_SZ	8
static const Byte	gspPNGSignature[]	=
	{ 137, 80, 78, 71, 13, 10, 26, 10 };
static const int	gspStartLine[8]		=
	{ 0, 0, 0, 4, 0, 2, 0, 1 };

/* Chunk type flags */

#define PNG_CF_ANCILLARY	0x10000000L	/* Non-critical chunk (can ignore) */
#define PNG_CF_PRIVATE		0x00100000L	/* Application-specific chunk */
#define PNG_CF_RESERVED		0x00001000L	/* Not used */
#define PNG_CF_COPYSAFE		0x00000010L	/* Opaque data safe for copying */

/* Chunk types */

#define CHUNK_IDAT	PNG_INT32(  73,  68,  65,  84 )	/* Pixel data */
#define CHUNK_IEND	PNG_INT32(  73,  69,  78,  68 )	/* End of Image */
#define CHUNK_IHDR	PNG_INT32(  73,  72,  68,  82 )	/* Header */
#define CHUNK_PLTE	PNG_INT32(  80,  76,  84,  69 )	/* Palette */
#define CHUNK_bKGD	PNG_INT32(  98,  75,  71,  68 )	/* Background Color */
#define CHUNK_cHRM	PNG_INT32(  99,  72,  82,  77 )
#define CHUNK_gAMA	PNG_INT32( 103,  65,  77,  65 )	/* Gamma */
#define CHUNK_hIST	PNG_INT32( 104,  73,  83,  84 )	/* Histogram */
#define CHUNK_iCCP	PNG_INT32( 105,  67,  67,  80 )
#define CHUNK_iTXt	PNG_INT32( 105,  84,  88, 116 )	/* Text (comments etc.) */
#define CHUNK_oFFs	PNG_INT32( 111,  70,  70, 115 )
#define CHUNK_pCAL	PNG_INT32( 112,  67,  65,  76 )
#define CHUNK_pHYs	PNG_INT32( 112,  72,  89, 115 )
#define CHUNK_sBIT	PNG_INT32( 115,  66,  73,  84 )
#define CHUNK_sCAL	PNG_INT32( 115,  67,  65,  76 )
#define CHUNK_sPLT	PNG_INT32( 115,  80,  76,  84 )
#define CHUNK_sRGB	PNG_INT32( 115,  82,  71,  66 )
#define CHUNK_tEXt	PNG_INT32( 116,  69,  88, 116 )	/* More text */
#define CHUNK_tIME	PNG_INT32( 116,  73,  77,  69 )	/* Time stamp */
#define CHUNK_tRNS	PNG_INT32( 116,  82,  78,  83 )	/* Transparency */
#define CHUNK_zTXt	PNG_INT32( 122,  84,  88, 116 )	/* More text */

/* Color flags */

#define PNG_COLOR_INDEXED		1
#define PNG_COLOR_USED			2
#define PNG_COLOR_ALPHA			4

/* Actual color types */

#define PNG_COLOR_GRAY			0
#define PNG_COLOR_RGB			(PNG_COLOR_USED)
#define PNG_COLOR_PLTE			(PNG_COLOR_USED | PNG_COLOR_INDEXED)
#define PNG_COLOR_GRAYALPHA		(PNG_COLOR_GRAY | PNG_COLOR_ALPHA)
#define PNG_COLOR_RGBA			(PNG_COLOR_USED | PNG_COLOR_ALPHA)

/* Compression Methods */

#define PNG_COMPRESS_DEFLATE	0

/* Filter Methods */

#define PNG_FILTMETH_STANDARD	0

/* Interlacing Methods */

#define	PNG_INTERLACE_NONE		0
#define PNG_INTERLACE_ADAM7		1

#define	PNG_ENCODE	0
#define PNG_DECODE	1

typedef struct
{
	Byte	mRed;
	Byte	mGrn;
	Byte	mBlu;
	Byte	mAlpha;
} PNG_RGBA;

/* State information */

typedef struct
{
	/* PNG Data Source/Destination channel/object/byte array */

	Tcl_Channel		mChannel;		/* Channel for from-file reads */
	Tcl_Obj*		mpObjData;
	Byte*			mpStrData;		/* Raw source data for from-string reads */
	int				mStrDataSz;		/* Length of source data */
	Byte*			mpBase64Data;	/* base64 encoded string data */
	Byte			mBase64Bits;	/* Remaining bits from last base64 read */
	Byte			mBase64State;	/* Current state of base64 decoder */
	double			mAlpha;			/* Alpha from -format option */

	/* State information for zlib compression/decompression */

	z_stream		mZStream;		/* Zlib inflate/deflate stream state */
	int				mZStreamInit;	/* Stream has been initialized */
	int				mZStreamDir;	/* PNG_ENCODE/PNG_DECODE */

	/* Image Header Information */

	Byte			mBitDepth;		/* Number of bits per pixel */
	Byte			mColorType;		/* Grayscale, TrueColor, etc. */
	Byte			mCompression;	/* Compression Mode (always zlib) */
	Byte			mFilter;		/* Filter mode (0 - 3) */
	Byte			mInterlace;		/* Type of interlacing (if any) */

	Byte			mChannels;		/* Number of channels per pixel */
	Byte			mBPP;			/* Bytes per pixel in scan line */
	int				mBitScale;		/* Scale factor for RGB/Gray depths < 8 */

	int				mCurrLine;		/* Current line being unfiltered */
	Byte			mPhase;			/* Interlacing phase (0..6) */

	Tk_PhotoImageBlock	mBlock;
	int				mBlockSz;		/* Number of bytes in Tk image pixels */

	/* PLTE Palette and tRNS Transparency Entries */

	int				mPalEntries;	/* Number of PLTE entries (1..256) */
	int				mUseTRNS;
	PNG_RGBA		mpPalette[256];	/* Palette RGB/Transparency table */
	Byte			mpTrans[6];		/* Fully-transparent RGB/Gray Value */

	/* PNG and Tk Photo pixel data */

	Byte*			mpLastLine;		/* Last line of pixels, for unfiltering */
	Byte*			mpThisLine;		/* Current line of pixels to process */
	int				mLineSz;		/* Number of bytes in a PNG line */
	int				mPhaseSz;		/* Number of bytes/line in current phase */
} PNGImage;

/*
 * The format record for the PNG file format:
 */

static int  FileMatchPNG _ANSI_ARGS_((Tcl_Channel chan, CONST char *fileName,
            Tcl_Obj *pObjFmt, int *widthPtr, int *heightPtr,
            Tcl_Interp *interp));
static int  FileReadPNG  _ANSI_ARGS_((Tcl_Interp *interp,
            Tcl_Channel chan, CONST char *fileName, Tcl_Obj *pObjFmt,
            Tk_PhotoHandle imageHandle, int destX, int destY,
            int width, int height, int srcX, int srcY));
static int  StringMatchPNG _ANSI_ARGS_(( Tcl_Obj *pObjData,
            Tcl_Obj *pObjFmt, int *widthPtr, int *heightPtr,
            Tcl_Interp *interp));
static int  StringReadPNG _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *pObjData,
            Tcl_Obj *pObjFmt, Tk_PhotoHandle imageHandle,
            int destX, int destY, int width, int height,
            int srcX, int srcY));
static int  FileWritePNG _ANSI_ARGS_((Tcl_Interp *interp,
            CONST char *filename, Tcl_Obj *pObjFmt,
            Tk_PhotoImageBlock *blockPtr));
static int	StringWritePNG _ANSI_ARGS_((Tcl_Interp* interp,
			Tcl_Obj* pObjFmt, Tk_PhotoImageBlock *blockPtr));

Tk_PhotoImageFormat tkImgFmtPNG = {
	"png",				/* name */
	FileMatchPNG,		/* fileMatchProc */
	StringMatchPNG,		/* stringMatchProc */
	FileReadPNG,		/* fileReadProc */
	StringReadPNG,		/* stringReadProc */
	FileWritePNG,		/* fileWriteProc */
	StringWritePNG		/* stringWriteProc */
};


/*
 *----------------------------------------------------------------------
 *
 * PNGZAlloc --
 *
 *		This function is invoked by zlib to allocate memory it needs.
 *
 * Results:
 *		A pointer to the allocated buffer or NULL if allocation fails.
 *
 * Side effects:
 *		Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

static
voidpf PNGZAlloc(voidpf opaque, uInt items, uInt itemSz)
{
	/* Check for required buffer size within attemptckalloc limits */

	if (items > INT_MAX / itemSz)
		return Z_NULL;

	return (voidpf)attemptckalloc(items * itemSz);
}


/*
 *----------------------------------------------------------------------
 *
 * PNGZFree --
 *
 *		This function is invoked by zlib to free memory it previously
 *		allocated using PNGZAlloc.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		Memory is freed.
 *
 *----------------------------------------------------------------------
 */

static
void PNGZFree(voidpf opaque, voidpf ptr)
{
	if (ptr) ckfree((char *)ptr);
}


/*
 *----------------------------------------------------------------------
 *
 * PNGInit --
 *
 *		This function is invoked by each of the Tk image handler
 *		procs (MatchStringProc, etc.) to initialize state information
 *		used during the course of encoding or decoding a PNG image.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if initialization failed.
 *
 * Side effects:
 *		The reference count of the -data Tcl_Obj*, if any, is
 *		incremented.
 *
 *----------------------------------------------------------------------
 */

static int
PNGInit(Tcl_Interp* interp, PNGImage* pPNG,
	Tcl_Channel chan, Tcl_Obj* pObj, int dir)
{
	int zresult;

	memset(pPNG, 0, sizeof(PNGImage));

	pPNG -> mChannel = chan;
	pPNG -> mAlpha = 1.0;

	/*
	 * If decoding from a -data string object, increment its reference
	 * count for the duration of the decode and get its length and
	 * byte array for reading with PNGRead().
	 */

	if (pObj)
	{
		Tcl_IncrRefCount(pObj);
		pPNG -> mpObjData = pObj;
		pPNG -> mpStrData = Tcl_GetByteArrayFromObj(pObj, &pPNG -> mStrDataSz);
	}

	/* Initialize the palette transparency table to fully opaque */

	memset(pPNG -> mpPalette, 255, sizeof(pPNG -> mpPalette));

	/* Initialize Zlib inflate/deflate stream */

	pPNG -> mZStream.zalloc	= PNGZAlloc;	/* Memory allocation */
	pPNG -> mZStream.zfree	= PNGZFree;		/* Memory deallocation */

	if (PNG_DECODE == dir) {
		zresult = inflateInit(&pPNG -> mZStream);
	} else {
		zresult = deflateInit(&pPNG -> mZStream, Z_DEFAULT_COMPRESSION);
	}

	/* Make sure that Zlib stream initialization was successful */

	if (Z_OK != zresult)
	{
		if (pPNG -> mZStream.msg)
			Tcl_SetResult(interp, pPNG -> mZStream.msg, TCL_VOLATILE);
		else
			Tcl_SetResult(interp, "zlib initialization failed", TCL_STATIC);

		return TCL_ERROR;
	}

	/* Flag the image as having an initialized Zlib stream */

	pPNG -> mZStreamInit = 1;

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * PNGCleanup --
 *
 *		This function is invoked by each of the Tk image handler
 *		procs (MatchStringProc, etc.) prior to returning to Tcl
 *		in order to clean up any allocated memory and call other
 *		cleanup handlers such as zlib's inflateEnd/deflateEnd.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		The reference count of the -data Tcl_Obj*, if any, is
 *		decremented.  Buffers are freed, zstreams are closed.
 *      The PNGImage should not be used for any purpose without being
 *      reinitialized post-cleanup.
 *
 *----------------------------------------------------------------------
 */

static void
PNGCleanup(PNGImage* pPNG)
{
	/* Don't need the object containing the -data value anymore. */

	if (pPNG -> mpObjData)
	{
		Tcl_DecrRefCount(pPNG -> mpObjData);
	}

	/* Discard pixel buffer */

	if (pPNG -> mZStreamInit)
	{
		if (PNG_ENCODE == pPNG -> mZStreamDir)
			deflateEnd(&pPNG -> mZStream);
		else
			inflateEnd(&pPNG -> mZStream);
	}

	if (pPNG -> mBlock.pixelPtr)
		ckfree((char *)pPNG -> mBlock.pixelPtr);
	if (pPNG -> mpThisLine)
		ckfree((char *)pPNG -> mpThisLine);
	if (pPNG -> mpLastLine)
		ckfree((char *)pPNG -> mpLastLine);
}


/*
 *----------------------------------------------------------------------
 *
 * PNGReadBase64 --
 *
 *		This function is invoked to read the specified number of bytes
 *		from base-64 encoded image data.
 *
 *		Note: It would be better if the Tk_PhotoImage stuff handled
 *		this by creating a channel from the -data value, which would
 *		take care of base64 decoding and made the data readable as if
 *		it were coming from a file.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error occurs.
 *
 * Side effects:
 *		The file position will change.  The running CRC is updated
 *		if a pointer to it is provided.
 *
 *----------------------------------------------------------------------
 */

#define PNG64_SPECIAL     0x80
#define PNG64_SPACE       0x80
#define PNG64_PAD         0x81
#define PNG64_DONE        0x82
#define PNG64_BAD         0x83

static Byte gspFrom64[] =
{
	0x82, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x80, 0x80, 0x83,
	0x80, 0x80, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x80, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x3e, 0x83, 0x83, 0x83, 0x3f,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x83, 0x83,
	0x83, 0x81, 0x83, 0x83, 0x83, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
	0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24,
	0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83, 0x83,
	0x83, 0x83, 0x83
};

static int
PNGReadBase64(Tcl_Interp* interp, PNGImage* pPNG,
	Byte* pDest, int destSz, uLong* pCRC)
{
	while (destSz && pPNG -> mStrDataSz)
	{
		Byte	c	= 0;
		Byte	c64 = gspFrom64[*pPNG -> mpStrData++];

		pPNG -> mStrDataSz--;

		if (PNG64_SPACE == c64)
			continue;

		if (c64 & PNG64_SPECIAL)
		{
			c = pPNG -> mBase64Bits;
		}
		else
		{
			if (0 == pPNG -> mBase64State)
			{
				pPNG -> mBase64Bits = c64 << 2;
				pPNG -> mBase64State++;
				continue;
			}

			switch (pPNG -> mBase64State++)
			{
			case 1:
				c = (Byte)(pPNG -> mBase64Bits | (c64 >> 4));
				pPNG -> mBase64Bits = (c64 & 0xF) << 4;
				break;
			case 2:
				c = (Byte)(pPNG -> mBase64Bits | (c64 >> 2));
				pPNG -> mBase64Bits = (c64 & 0x3) << 6;
				break;
			case 3:
				c = (Byte)(pPNG -> mBase64Bits | c64);
				pPNG -> mBase64State = 0;
				pPNG -> mBase64Bits = 0;
				break;
			}
		}

		if (pCRC)
			*pCRC = crc32(*pCRC, &c, 1);

		if (pDest)
			*pDest++ = c;

		destSz--;

		if (c64 & PNG64_SPECIAL)
			break;
	}

	if (destSz)
	{
		Tcl_SetResult(interp, "Unexpected end of image data", TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * PNGReadByteArray --
 *
 *		This function is invoked to read the specified number of bytes
 *		from a non-base64-encoded byte array provided via the -data
 *		option.
 *
 *		Note: It would be better if the Tk_PhotoImage stuff handled
 *		this by creating a channel from the -data value and made the
 *		data readable as if it were coming from a file.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error occurs.
 *
 * Side effects:
 *		The file position will change.  The running CRC is updated
 *		if a pointer to it is provided.
 *
 *----------------------------------------------------------------------
 */

static int
PNGReadByteArray(Tcl_Interp* interp, PNGImage* pPNG,
	Byte* pDest, int destSz, uLong* pCRC)
{
	/* Check to make sure the number of requested bytes are available */

	if (pPNG -> mStrDataSz < destSz)
	{
		Tcl_SetResult(interp, "Unexpected end of image data", TCL_STATIC);
		return TCL_ERROR;
	}

	while (destSz)
	{
		int blockSz = PNG_MIN(destSz, PNG_BLOCK_SZ);

		memcpy(pDest, pPNG -> mpStrData, blockSz);

		pPNG -> mpStrData	+= blockSz;
		pPNG -> mStrDataSz	-= blockSz;

		if (pCRC)
			*pCRC = crc32(*pCRC, pDest, blockSz);

		pDest	+= blockSz;
		destSz	-= blockSz;
	}

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * PNGRead --
 *
 *		This function is invoked to read the specified number of bytes
 *		from the image file or data.  It is a wrapper around the
 *		choice of byte array Tcl_Obj or Tcl_Channel which depends on
 *		whether the image data is coming from a file or -data.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error occurs.
 *
 * Side effects:
 *		The file position will change.  The running CRC is updated
 *		if a pointer to it is provided.
 *
 *----------------------------------------------------------------------
 */

static int
PNGRead(Tcl_Interp* interp, PNGImage* pPNG,
	Byte* pDest, int destSz, uLong* pCRC)
{
	if (pPNG -> mpBase64Data)
	{
		return PNGReadBase64(interp, pPNG, pDest, destSz, pCRC);
	}

	if (pPNG -> mpStrData)
	{
		return PNGReadByteArray(interp, pPNG, pDest, destSz, pCRC);
	}

	while (destSz)
	{
		int blockSz = PNG_MIN(destSz, PNG_BLOCK_SZ);

		blockSz = Tcl_Read(pPNG -> mChannel, (char *)pDest, blockSz);

		/* Check for read failure */

		if (blockSz < 0)
		{
			/* TODO: failure info... */
			Tcl_SetResult(interp, "Channel read failed", TCL_STATIC);
			return TCL_ERROR;
		}

		/* Update CRC, pointer, and remaining count if anything was read */

		if (blockSz)
		{
			if (pCRC)
				*pCRC = crc32(*pCRC, pDest, blockSz);

			pDest	+= blockSz;
			destSz	-= blockSz;
		}

		/* Check for EOF before all desired data was read */

		if (destSz && Tcl_Eof(pPNG -> mChannel))
		{
			Tcl_SetResult(interp, "Unexpected end of file ", TCL_STATIC);
			return TCL_ERROR;
		}
	}

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * PNGReadInt32 --
 *
 *		This function is invoked to read a 32-bit integer in network
 *		byte order from the image data and return the value in host
 *		byte order.  This is used, for example, to read the 32-bit CRC
 *		value for a chunk stored in the image file for comparison with
 *		the calculated CRC value.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error occurs.
 *
 * Side effects:
 *		The file position will change.  The running CRC is updated
 *		if a pointer to it is provided.
 *
 *----------------------------------------------------------------------
 */

static int
PNGReadInt32(Tcl_Interp* interp, PNGImage* pPNG, uLong* pResult, uLong* pCRC)
{
	Byte p[4];

	if (PNGRead(interp, pPNG, p, 4, pCRC) == TCL_ERROR)
		return TCL_ERROR;

	*pResult = PNG_INT32(p[0],p[1],p[2],p[3]);

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * CheckCRC --
 *
 *		This function is reads the final 4-byte integer CRC from a
 *		chunk and compares it to the running CRC calculated over the
 *		chunk type and data fields.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error or CRC mismatch occurs.
 *
 * Side effects:
 *		The file position will change.
 *
 *----------------------------------------------------------------------
 */

static
int CheckCRC(Tcl_Interp* interp, PNGImage* pPNG, uLong calculated)
{
	uLong	chunked;

	/* Read the CRC field at the end of the chunk */

	if (PNGReadInt32(interp, pPNG, &chunked, NULL) == TCL_ERROR)
		return TCL_ERROR;

	/* Compare the read CRC to what we calculate to make sure they match. */

	if (calculated != chunked)
	{
		Tcl_SetResult(interp, "CRC check failed", TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * SkipChunk --
 *
 *		This function is used to skip a PNG chunk that is not used
 *		by this implementation.  Given the input stream has had the
 *		chunk length and chunk type fields already read, this function
 *		will read the number of bytes indicated by the chunk length,
 *		plus four for the CRC, and will verify that CRC is
 *		correct for the skipped data.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error or CRC mismatch occurs.
 *
 * Side effects:
 *		The file position will change.
 *
 *----------------------------------------------------------------------
 */

static int
SkipChunk(Tcl_Interp* interp, PNGImage* pPNG, int chunkSz, uLong crc)
{
	Byte	pBuffer[PNG_BLOCK_SZ];

	/*
	 * Skip data in blocks until none is left.  Read up to PNG_BLOCK_SZ
	 * bytes at a time, rather than trusting the claimed chunk size,
	 * which may not be trustworthy.
	 */

	while (chunkSz)
	{
		int blockSz = PNG_MIN(chunkSz, PNG_BLOCK_SZ);

		if (PNGRead(interp, pPNG, pBuffer, blockSz, &crc) == TCL_ERROR)
			return TCL_ERROR;

		chunkSz -= blockSz;
	}

	if (CheckCRC(interp, pPNG, crc) == TCL_ERROR)
		return TCL_ERROR;

	return TCL_OK;
}


/*
4.3. Summary of standard chunks

This table summarizes some properties of the standard chunk types. 

	Critical chunks (must appear in this order, except PLTE
					is optional):

		   Name  Multiple  Ordering constraints
				   OK?

		   IHDR	No	  Must be first
		   PLTE	No	  Before IDAT
		   IDAT	Yes	 Multiple IDATs must be consecutive
		   IEND	No	  Must be last

	Ancillary chunks (need not appear in this order):

		   Name  Multiple  Ordering constraints
				   OK?

		   cHRM	No	  Before PLTE and IDAT
		   gAMA	No	  Before PLTE and IDAT
		   iCCP	No	  Before PLTE and IDAT
		   sBIT	No	  Before PLTE and IDAT
		   sRGB	No	  Before PLTE and IDAT
		   bKGD	No	  After PLTE; before IDAT
		   hIST	No	  After PLTE; before IDAT
		   tRNS	No	  After PLTE; before IDAT
		   pHYs	No	  Before IDAT
		   sPLT	Yes	 Before IDAT
		   tIME	No	  None
		   iTXt	Yes	 None
		   tEXt	Yes	 None
		   zTXt	Yes	 None

	[From the PNG specification.]
*/


/*
 *----------------------------------------------------------------------
 *
 * ReadChunkHeader --
 *
 *		This function is used at the start of each chunk to extract
 *		the four-byte chunk length and four-byte chunk type fields.
 *		It will continue reading until it finds a chunk type that is
 *		handled by this implementation, checking the CRC of any chunks
 *		it skips.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error occurs or an unknown
 *		critical chunk type is encountered.
 *
 * Side effects:
 *		The file position will change.  The running CRC is updated.
 *
 *----------------------------------------------------------------------
 */

static int
ReadChunkHeader(Tcl_Interp* interp, PNGImage* pPNG,
	int* pSize, uLong* pType, uLong* pCRC)
{
	uLong	chunkType	= 0;
	int		chunkSz		= 0;
	uLong	crc			= 0;

	/* Continue until finding a chunk type that is handled. */

	while (!chunkType)
	{
		uLong	temp;
		Byte	pc[4];
		int		i;

		/*
		 * Read the 4-byte length field for the chunk.  The length field
		 * is not included in the CRC calculation, so the running CRC must
		 * be reset afterward.  Limit chunk lengths to INT_MAX, to align
		 * with the maximum size for Tcl_Read, Tcl_GetByteArrayFromObj, etc.
		 */

		if (PNGRead(interp, pPNG, pc, 4, NULL) == TCL_ERROR)
			return TCL_ERROR;

		temp = PNG_INT32(pc[0],pc[1],pc[2],pc[3]);

		if (temp > INT_MAX)
		{
			Tcl_SetResult(interp,
				"Chunk size is out of supported range on this architecture",
				TCL_STATIC);
			return TCL_ERROR;
		}

		chunkSz	= (int)temp;
		crc		= crc32(0, NULL, 0);

		/* Read the 4-byte chunk type */

		if (PNGRead(interp, pPNG, pc, 4, &crc) == TCL_ERROR)
			return TCL_ERROR;

		/* Convert it to a host-order integer for simple comparison */

		chunkType = PNG_INT32(pc[0], pc[1], pc[2], pc[3]);

		/*
		 * Check to see if this is a known/supported chunk type.  Note
		 * that the PNG specs require non-critical (i.e., ancillary)
		 * chunk types that are not recognized to be ignored, rather
		 * than be treated as an error.  It does, however, recommend
		 * that an unknown critical chunk type be treated as a failure.
		 *
		 * This switch/loop acts as a filter of sorts for undesired
		 * chunk types.  The chunk type should still be checked
		 * elsewhere for determining it is in the correct order.
		 */

		switch (chunkType)
		{
		/* These chunk types are required and/or supported */

		case CHUNK_IDAT:
		case CHUNK_IEND:
		case CHUNK_IHDR:
		case CHUNK_PLTE:
		case CHUNK_tRNS:
			break;

		/*
		 * These chunk types are part of the standard, but are not used by
		 * this implementation (at least not yet).  Note that these are
		 * all ancillary chunks (lowercase first letter).
		 */

		case CHUNK_bKGD:
		case CHUNK_cHRM:
		case CHUNK_gAMA:
		case CHUNK_hIST:
		case CHUNK_iCCP:
		case CHUNK_iTXt:
		case CHUNK_oFFs:
		case CHUNK_pCAL:
		case CHUNK_pHYs:
		case CHUNK_sBIT:
		case CHUNK_sCAL:
		case CHUNK_sPLT:
		case CHUNK_sRGB:
		case CHUNK_tEXt:
		case CHUNK_tIME:
		case CHUNK_zTXt:
			/* TODO: might want to check order here. */
			if (SkipChunk(interp, pPNG, chunkSz, crc) == TCL_ERROR)
				return TCL_ERROR;

			chunkType = 0;
			break;

		default:
			/* Unknown chunk type. If it's critical, we can't continue. */

			if (!(chunkType & PNG_CF_ANCILLARY))
			{
				Tcl_SetResult(interp,
					"Encountered an unsupported criticial chunk type",
					TCL_STATIC);
				return TCL_ERROR;
			}

			/* Check to see if the chunk type has legal bytes */

			for (i = 0 ; i < 4 ; i++)
			{
				if ((pc[i] < 65) || (pc[i] > 122) ||
					((pc[i] > 90) && (pc[i] < 97)))
				{
					Tcl_SetResult(interp, "Invalid chunk type", TCL_STATIC);
					return TCL_ERROR;
				}
			}

			/*
			 * It seems to be an otherwise legally labelled ancillary chunk
			 * that we don't want, so skip it after at least checking its CRC.
			 */

			if (SkipChunk(interp, pPNG, chunkSz, crc) == TCL_ERROR)
				return TCL_ERROR;

			chunkType = 0;
		}
	}

	/*
	 * Found a known chunk type that's handled, albiet possibly not in
	 * the right order.  Send back the chunk type (for further checking
	 * or handling), the chunk size and the current CRC for the rest
	 * of the calculation.
	 */

	*pType	= chunkType;
	*pSize	= chunkSz;
	*pCRC	= crc;

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * PNGCheckColor --
 *
 *		Do validation on color type, depth, and related information,
 *		and calculates storage requirements and offsets based on image
 *		dimensions and color.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if color information is invalid or some
 *		other failure occurs.
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

static int
PNGCheckColor(Tcl_Interp* interp, PNGImage* pPNG)
{
	int result = TCL_OK;
	int	offset;

	/* Verify the color type is valid and the bit depth is allowed */

	switch (pPNG -> mColorType)
	{
	case PNG_COLOR_GRAY:
		pPNG -> mChannels = 1;
		if ((1 != pPNG->mBitDepth) && (2 != pPNG->mBitDepth) &&
			(4 != pPNG->mBitDepth) && (8 != pPNG->mBitDepth) &&
			(16 != pPNG -> mBitDepth))
			result = TCL_ERROR;
		break;

	case PNG_COLOR_RGB:
		pPNG -> mChannels = 3;
		if ((8 != pPNG->mBitDepth) && (16 != pPNG->mBitDepth))
			result = TCL_ERROR;
		break;

	case PNG_COLOR_PLTE:
		pPNG -> mChannels = 1;
		if ((1 != pPNG->mBitDepth) && (2 != pPNG->mBitDepth) &&
			(4 != pPNG->mBitDepth) && (8 != pPNG->mBitDepth))
				result = TCL_ERROR;
		break;

	case PNG_COLOR_GRAYALPHA:
		pPNG -> mChannels = 2;
		if ((8 != pPNG->mBitDepth) && (16 != pPNG->mBitDepth))
			result = TCL_ERROR;
		break;

	case PNG_COLOR_RGBA:
		pPNG -> mChannels = 4;
		if ((8 != pPNG->mBitDepth) && (16 != pPNG->mBitDepth))
			result = TCL_ERROR;
		break;

	default:
		Tcl_SetResult(interp, "Unknown Color Type field", TCL_STATIC);
		return TCL_ERROR;
	}

	if (TCL_ERROR == result)
	{
		Tcl_SetResult(interp, "Bit depth is not allowed for given color type",
			TCL_STATIC);
		return TCL_ERROR;
	}

	/*
	 * Set up the Tk photo block's pixel size and channel offsets.
	 * offset array elements should already be 0 from the memset
	 * during PNGInit().
	 */

	offset = (pPNG -> mBitDepth > 8) ? 2 : 1;

	if (pPNG -> mColorType & PNG_COLOR_USED)
	{
		pPNG -> mBlock.pixelSize	= offset * 4;
		pPNG -> mBlock.offset[1]	= offset;
		pPNG -> mBlock.offset[2]	= offset * 2;
		pPNG -> mBlock.offset[3]	= offset * 3;
	}
	else
	{
		pPNG -> mBlock.pixelSize	= offset * 2;
		pPNG -> mBlock.offset[3]	= offset;
	}

	/*
	 * Calculate the block pitch, which is the number of bytes per line in
	 * the image, given image width and depth of color.  Make sure that it
	 * it isn't larger than Tk can handle.
	 */

	if (pPNG -> mBlock.width > INT_MAX / pPNG -> mBlock.pixelSize)
	{
		Tcl_SetResult(interp,
			"Image pitch is out of supported range on this architecture",
			TCL_STATIC);
		return TCL_ERROR;
	}

	pPNG -> mBlock.pitch = pPNG -> mBlock.pixelSize * pPNG -> mBlock.width;

	/*
	 * Calculate the total size of the image as represented to Tk given
	 * pitch and image height.  Make sure that it isn't larger than Tk can
	 * handle.
	 */

	if (pPNG -> mBlock.height > INT_MAX / pPNG -> mBlock.pitch)
	{
		Tcl_SetResult(interp,
			"Image total size is out of supported range on this architecture",
			TCL_STATIC);
		return TCL_ERROR;
	}

	pPNG -> mBlockSz = pPNG -> mBlock.height * pPNG -> mBlock.pitch;

	/* Determine number of bytes per pixel in the source for later use */

	switch (pPNG -> mColorType)
	{
	case PNG_COLOR_GRAY:
		pPNG -> mBPP = (pPNG -> mBitDepth > 8) ? 2 : 1;
		break;
	case PNG_COLOR_RGB:
		pPNG -> mBPP = (pPNG -> mBitDepth > 8) ? 6 : 3;
		break;
	case PNG_COLOR_PLTE:
		pPNG -> mBPP = 1;
		break;
	case PNG_COLOR_GRAYALPHA:
		pPNG -> mBPP = (pPNG -> mBitDepth > 8) ? 4 : 2;
		break;
	case PNG_COLOR_RGBA:
		pPNG -> mBPP = (pPNG -> mBitDepth > 8) ? 8 : 4;
		break;
	default:
		Tcl_SetResult(interp, "internal error - unknown color type",
			TCL_STATIC);
		return TCL_ERROR;
	}

	/*
	 * Calculate scale factor for bit depths less than 8, in order to
	 * adjust them to a minimum of 8 bits per pixel in the Tk image.
	 */

	if (pPNG -> mBitDepth < 8)
		pPNG -> mBitScale = (Byte)(255/(pow(2, pPNG -> mBitDepth)-1));
	else
		pPNG -> mBitScale = 1;

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ReadIHDR --
 *
 *		This function reads the PNG header from the beginning of a
 *		PNG file and returns the dimensions of the image.
 *
 * Results:
 *		The return value is 1 if file "f" appears to start with
 *		a valid PNG header, 0 otherwise.  If the header is valid,
 *		then *widthPtr and *heightPtr are modified to hold the
 *		dimensions of the image.
 *
 * Side effects:
 *		The access position in f advances.
 *
 *----------------------------------------------------------------------
 */

static int
ReadIHDR(Tcl_Interp* interp, PNGImage* pPNG)
{
	Byte	pSig[PNG_SIG_SZ];
	uLong	chunkType;
	int		chunkSz;
	uLong	crc;
	uLong	width, height;
	int		mismatch;

	/* Read the appropriate number of bytes for the PNG signature */

	if (PNGRead(interp, pPNG, pSig, PNG_SIG_SZ, NULL) == TCL_ERROR)
		return TCL_ERROR;

	/* Compare the read bytes to the expected signature. */

	mismatch = memcmp(pSig, gspPNGSignature, PNG_SIG_SZ);

	/* If reading from string, reset position and try base64 decode */

	if (mismatch && pPNG -> mpStrData)
	{
		pPNG -> mpStrData = Tcl_GetByteArrayFromObj(pPNG -> mpObjData,
			&pPNG -> mStrDataSz);
		pPNG -> mpBase64Data = pPNG -> mpStrData;

		if (PNGRead(interp, pPNG, pSig, PNG_SIG_SZ, NULL) == TCL_ERROR)
			return TCL_ERROR;

		mismatch = memcmp(pSig, gspPNGSignature, PNG_SIG_SZ);
	}

	if (mismatch)
	{
		Tcl_SetResult(interp, "Data stream does not have a PNG signature",
			TCL_STATIC);
		return TCL_ERROR;
	}

	if (ReadChunkHeader(interp, pPNG, &chunkSz, &chunkType,
			&crc) == TCL_ERROR)
		return TCL_ERROR;

	/* Read in the IHDR (header) chunk for width, height, etc. */
	/* The first chunk in the file must be the IHDR (headr) chunk */

	if (chunkType != CHUNK_IHDR)
	{
		Tcl_SetResult(interp, "Expected IHDR chunk type", TCL_STATIC);
		return TCL_ERROR;
	}

	if (chunkSz != 13)
	{
		Tcl_SetResult(interp, "Invalid IHDR chunk size", TCL_STATIC);
		return TCL_ERROR;
	}

	/*
	 * Read and verify the image width and height to be sure Tk can handle
	 * its dimensions.  The PNG specification does not permit zero-width
	 * or zero-height images.
	 */

	if (PNGReadInt32(interp, pPNG, &width, &crc) == TCL_ERROR)
		return TCL_ERROR;

	if (PNGReadInt32(interp, pPNG, &height, &crc) == TCL_ERROR)
		return TCL_ERROR;

	if (!width || !height || (width > INT_MAX) || (height > INT_MAX))
	{
		Tcl_SetResult(interp,
			"Image dimensions are invalid or beyond architecture limits",
			TCL_STATIC);
		return TCL_ERROR;
	}

	/* Set height and width for the Tk photo block */

	pPNG -> mBlock.width	= (int)width;
	pPNG -> mBlock.height	= (int)height;

	/* Read and the Bit Depth and Color Type */

	if (PNGRead(interp, pPNG, &pPNG->mBitDepth, 1, &crc) == TCL_ERROR)
		return TCL_ERROR;

	if (PNGRead(interp, pPNG, &pPNG->mColorType, 1, &crc) == TCL_ERROR)
		return TCL_ERROR;

	/*
	 * Verify that the color type is valid, the bit depth is allowed
	 * for the color type, and calculate the number of channels and
	 * pixel depth (bits per pixel * channels).  Also set up offsets
	 * and sizes in the Tk photo block for the pixel data.
	 */

	if (PNGCheckColor(interp, pPNG) == TCL_ERROR)
		return TCL_ERROR;

	/* Only one compression method is currently defined by the standard */

	if (PNGRead(interp, pPNG, &pPNG->mCompression, 1, &crc) == TCL_ERROR)
		return TCL_ERROR;

	if (PNG_COMPRESS_DEFLATE != pPNG -> mCompression)
	{
		Tcl_SetResult(interp, "Unknown compression method", TCL_STATIC);
		return TCL_ERROR;
	}

	/*
	 * Only one filter method is currently defined by the standard; the
	 * method has five actual filter types associated with it.
	 */

	if (PNGRead(interp, pPNG, &pPNG->mFilter, 1, &crc) == TCL_ERROR)
		return TCL_ERROR;

	if (PNG_FILTMETH_STANDARD != pPNG -> mFilter)
	{
		Tcl_SetResult(interp, "Unknown filter method", TCL_STATIC);
		return TCL_ERROR;
	}

	if (PNGRead(interp, pPNG, &pPNG->mInterlace, 1, &crc) == TCL_ERROR)
		return TCL_ERROR;

	switch (pPNG -> mInterlace)
	{
	case PNG_INTERLACE_NONE:
	case PNG_INTERLACE_ADAM7:
		break;

	default:
		Tcl_SetResult(interp, "Unknown interlace method", TCL_STATIC);
		return TCL_ERROR;
	}

	return CheckCRC(interp, pPNG, crc);
}


/*
 *----------------------------------------------------------------------
 *
 * ReadPLTE --
 *
 *		This function reads the PLTE (indexed color palette) chunk
 *		data from the PNG file and populates the palette table in the
 *		PNGImage structure.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error occurs or the PLTE
 *		chunk is invalid.
 *
 * Side effects:
 *		The access position in f advances.
 *
 *----------------------------------------------------------------------
 */

#define	PNG_PLTE_MAXSZ	768		/* 3 bytes/RGB entry, 256 entries max */

static int
ReadPLTE(Tcl_Interp* interp, PNGImage* pPNG, int chunkSz, uLong crc)
{
	Byte	pBuffer[PNG_PLTE_MAXSZ];
	int		i, c;

	/* This chunk is mandatory for color type 3 and forbidden for 2 and 6 */

	switch (pPNG -> mColorType)
	{
	case PNG_COLOR_GRAY:
	case PNG_COLOR_GRAYALPHA:
		Tcl_SetResult(interp, "PLTE chunk type forbidden for grayscale",
			TCL_STATIC);
		return TCL_ERROR;

	default:
		break;
	}

	/*
	 * The palette chunk contains from 1 to 256 palette entries.
	 * Each entry consists of a 3-byte RGB value.  It must therefore
	 * contain a non-zero multiple of 3 bytes, up to 768.
	 */

	if (!chunkSz || (chunkSz > PNG_PLTE_MAXSZ) || (chunkSz % 3))
	{
		Tcl_SetResult(interp, "Invalid palette chunk size", TCL_STATIC);
		return TCL_ERROR;
	}

	/* Read the palette contents and stash them for later, possibly */

	if (PNGRead(interp, pPNG, pBuffer, chunkSz, &crc) == TCL_ERROR)
		return TCL_ERROR;

	if (CheckCRC(interp, pPNG, crc) == TCL_ERROR)
		return TCL_ERROR;

	/*
	 * Stash away the palette entries and entry count for later mapping
	 * each pixel's palette index to its color.
	 */

	for (i = 0, c = 0 ; c < chunkSz ; i++)
	{
		pPNG -> mpPalette[i].mRed = pBuffer[c++];
		pPNG -> mpPalette[i].mGrn = pBuffer[c++];
		pPNG -> mpPalette[i].mBlu = pBuffer[c++];
	}

	pPNG -> mPalEntries = i;
	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ReadtRNS --
 *
 *		This function reads the tRNS (transparency) chunk data from the
 *		PNG file and populates the alpha field of the palette table in
 *		the PNGImage structure or the single color transparency, as
 *		appropriate for the color type.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error occurs or the tRNS
 *		chunk is invalid.
 *
 * Side effects:
 *		The access position in f advances.
 *
 *----------------------------------------------------------------------
 */

#define	PNG_TRNS_MAXSZ	256		/* 1-byte alpha, 256 entries max */

static int
ReadtRNS(Tcl_Interp* interp, PNGImage* pPNG, int chunkSz, uLong crc)
{
	Byte	pBuffer[PNG_TRNS_MAXSZ];
	int		i;

	if (pPNG -> mColorType & PNG_COLOR_ALPHA)
	{
		Tcl_SetResult(interp,
			"tRNS chunk not allowed color types with a full alpha channel",
			TCL_STATIC);
		return TCL_ERROR;
	}

	/*
	 * For indexed color, there is up to one single-byte transparency value
	 * per palette entry (thus a max of 256).
	 */

	if (chunkSz > PNG_TRNS_MAXSZ)
	{
		Tcl_SetResult(interp, "Invalid tRNS chunk size", TCL_STATIC);
		return TCL_ERROR;
	}

	/* Read in the raw transparency information */

	if (PNGRead(interp, pPNG, pBuffer, chunkSz, &crc) == TCL_ERROR)
		return TCL_ERROR;

	if (CheckCRC(interp, pPNG, crc) == TCL_ERROR)
		return TCL_ERROR;

	switch (pPNG -> mColorType)
	{
	case PNG_COLOR_GRAYALPHA:
	case PNG_COLOR_RGBA:
		break;

	case PNG_COLOR_PLTE:
		/*
		 * The number of tRNS entries must be less than or equal to
		 * the number of PLTE entries, and consists of a single-byte
		 * alpha level for the corresponding PLTE entry.
		 */

		if (chunkSz > pPNG -> mPalEntries)
		{
			Tcl_SetResult(interp,
				"Size of tRNS chunk is too large for the palette",
				TCL_STATIC);
			return TCL_ERROR;
		}

		for (i = 0 ; i < chunkSz ; i++)
		{
			pPNG -> mpPalette[i].mAlpha = pBuffer[i];
		}
		break;

	case PNG_COLOR_GRAY:
		/*
		 * Grayscale uses a single 2-byte gray level, which we'll
		 * store in palette index 0, since we're not using the palette.
		 */

		if (chunkSz != 2)
		{
			Tcl_SetResult(interp,
				"Invalid tRNS chunk size - must 2 bytes for grayscale",
				TCL_STATIC);
			return TCL_ERROR;
		}

		/*
		 * According to the PNG specs, if the bit depth is less than 16,
		 * then only the lower byte is used.
		 */

		if (16 == pPNG -> mBitDepth)
		{
			pPNG -> mpTrans[0] = pBuffer[0];
			pPNG -> mpTrans[1] = pBuffer[1];
		}
		else
		{
			pPNG -> mpTrans[0] = pBuffer[1];
		}
		pPNG -> mUseTRNS = 1;
		break;

	case PNG_COLOR_RGB:
		/* TrueColor uses a single RRGGBB triplet. */

		if (chunkSz != 6)
		{
			Tcl_SetResult(interp,
				"Invalid tRNS chunk size - must 6 bytes for RGB",
				TCL_STATIC);
			return TCL_ERROR;
		}

		/*
		 * According to the PNG specs, if the bit depth is less than 16,
		 * then only the lower byte is used.  But the tRNS chunk still
		 * contains two bytes per channel.
		 */

		if (16 == pPNG -> mBitDepth)
		{
			memcpy(pPNG -> mpTrans, pBuffer, 6);
		}
		else
		{
			pPNG -> mpTrans[0] = pBuffer[1];
			pPNG -> mpTrans[1] = pBuffer[3];
			pPNG -> mpTrans[2] = pBuffer[5];
		}
		pPNG -> mUseTRNS = 1;
		break;
	}

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Paeth --
 *
 *		Utility function for applying the Paeth filter to a pixel.  The
 *		Paeth filter is a linear function of the pixel to be filtered
 *		and the pixels to the left, above, and above-left of the pixel
 *		to be unfiltered.
 *
 * Results:
 *		Result of the Paeth function for the left, above, and above-left
 *		pixels.
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

static Byte
Paeth(int a, int b, int c)
{
	int		pa	= abs(b - c);
	int		pb	= abs(a - c);
	int		pc	= abs(a + b - c - c);

	if ((pa <= pb) && (pa <= pc))
		return (Byte)a;

	if (pb <= pc)
		return (Byte)b;

	return (Byte)c;
}

/*
 *----------------------------------------------------------------------
 *
 * UnfilterLine --
 *
 *		Applies the filter algorithm specified in first byte of a line
 *		to the line of pixels being read from a PNG image.
 *
 *		PNG specifies four filter algorithms (Sub, Up, Average, and
 *		Paeth) that combine a pixel's value with those of other pixels
 *		in the same and/or previous lines. Filtering is intended to
 *		make an image more compressible.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if the filter type is not recognized.
 *
 * Side effects:
 *		Pixel data in mpThisLine are modified.
 *
 *----------------------------------------------------------------------
 */

#define	PNG_FILTER_NONE		0
#define	PNG_FILTER_SUB		1
#define	PNG_FILTER_UP		2
#define	PNG_FILTER_AVG		3
#define	PNG_FILTER_PAETH	4

static int
UnfilterLine(Tcl_Interp* interp, PNGImage* pPNG)
{
	switch (*pPNG->mpThisLine)
	{
	case PNG_FILTER_NONE:	/* Nothing to do */
		break;
	case PNG_FILTER_SUB:	/* Sub(x) = Raw(x) - Raw(x-bpp) */
		{
			Byte*	pRaw_bpp= pPNG -> mpThisLine + 1;
			Byte*	pRaw	= pRaw_bpp + pPNG -> mBPP;
			Byte*	pEnd	= pPNG->mpThisLine + pPNG->mPhaseSz;

			while (pRaw < pEnd)
			{
				*pRaw++ += *pRaw_bpp++;
			}
		}
		break;
	case PNG_FILTER_UP:		/* Up(x) = Raw(x) - Prior(x) */
		if (pPNG -> mCurrLine > gspStartLine[pPNG -> mPhase])
		{
			Byte*	pPrior	= pPNG -> mpLastLine + 1;
			Byte*	pRaw	= pPNG -> mpThisLine + 1;
			Byte*	pEnd	= pPNG->mpThisLine + pPNG->mPhaseSz;

			while (pRaw < pEnd)
			{
				*pRaw++ += *pPrior++;
			}
		}
		break;
	case PNG_FILTER_AVG:
		/* Avg(x) = Raw(x) - floor((Raw(x-bpp)+Prior(x))/2) */
		if (pPNG -> mCurrLine > gspStartLine[pPNG -> mPhase])
		{
			Byte*	pPrior	= pPNG -> mpLastLine + 1;
			Byte*	pRaw_bpp= pPNG -> mpThisLine + 1;
			Byte*	pRaw	= pRaw_bpp;
			Byte*	pEnd	= pPNG->mpThisLine + pPNG->mPhaseSz;
			Byte*	pEnd2	= pRaw + pPNG -> mBPP;

			while ((pRaw < pEnd2) && (pRaw < pEnd))
			{
				*pRaw++ += (*pPrior++/2);
			}

			while (pRaw < pEnd)
			{
				*pRaw++ += (Byte)(((int)*pRaw_bpp++ + (int)*pPrior++)/2);
			}
		}
		else
		{
			Byte*	pRaw_bpp= pPNG -> mpThisLine + 1;
			Byte*	pRaw	= pRaw_bpp + pPNG -> mBPP;
			Byte*	pEnd	= pPNG->mpThisLine + pPNG->mPhaseSz;

			while (pRaw < pEnd)
			{
				*pRaw++ += (*pRaw_bpp++/2);
			}
		}
		break;
	case PNG_FILTER_PAETH:
		/* Paeth(x) = Raw(x) - PaethPredictor(Raw(x-bpp), Prior(x),
				Prior(x-bpp)) */
		if (pPNG -> mCurrLine > gspStartLine[pPNG -> mPhase])
		{
			Byte*	pPrior_bpp	= pPNG -> mpLastLine + 1;
			Byte*	pPrior		= pPrior_bpp;
			Byte*	pRaw_bpp	= pPNG -> mpThisLine + 1;
			Byte*	pRaw		= pRaw_bpp;
			Byte*	pEnd		= pPNG->mpThisLine + pPNG->mPhaseSz;
			Byte*	pEnd2		= pRaw_bpp + pPNG -> mBPP;

			while ((pRaw < pEnd) && (pRaw < pEnd2))
			{
				*pRaw++ += *pPrior++;
			}

			while (pRaw < pEnd)
			{
				*pRaw++ += Paeth(*pRaw_bpp++, *pPrior++, *pPrior_bpp++);
			}
		}
		else
		{
			Byte*	pRaw_bpp= pPNG -> mpThisLine + 1;
			Byte*	pRaw	= pRaw_bpp + pPNG -> mBPP;
			Byte*	pEnd	= pPNG->mpThisLine + pPNG->mPhaseSz;

			while (pRaw < pEnd)
			{
				*pRaw++ += *pRaw_bpp++;
			}
		}
		break;
	default:
		Tcl_SetResult(interp, "Invalid filter type", TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DecodeLine --
 *
 *		Unfilters a line of pixels from the PNG source data and decodes
 *		the data into the Tk_PhotoImageBlock for later copying into the
 *		Tk image.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if the filter type is not recognized.
 *
 * Side effects:
 *		Pixel data in mpThisLine and mBlock are modified and state
 *		information updated.
 *
 *----------------------------------------------------------------------
 */

static int
DecodeLine(Tcl_Interp* interp, PNGImage* pPNG)
{
	Byte*	pixelPtr	= pPNG -> mBlock.pixelPtr;
	int		colNum		= 0;	/* Current pixel column */
	Byte	chan		= 0;	/* Current channel (0..3) = (R, G, B, A) */
	Byte	readByte	= 0;	/* Current scan line byte */
	int		haveBits	= 0;	/* Number of bits remaining in current byte */
	Byte	pixBits		= 0;	/* Extracted bits for current channel */
	int		shifts		= 0;	/* Number of channels extracted from byte */
	int		offset		= 0;	/* Current offset into pixelPtr */
	int		colStep		= 1;	/* Column increment each pass */
	int		pixStep		= 0;	/* extra pixelPtr increment each pass */
	Byte	pLastPixel[6];
	Byte*	p			= pPNG -> mpThisLine + 1;

	if (UnfilterLine(interp, pPNG) == TCL_ERROR)
		return TCL_ERROR;

	if (pPNG -> mInterlace)
	{
		switch (pPNG -> mPhase)
		{
		case 1:				/* Phase 1: */
			colStep	= 8;	/* 1 pixel per block of 8 per line */
			break;			/* Start at column 0 */
		case 2:				/* Phase 2: */
			colStep	= 8;	/* 1 pixels per block of 8 per line */
			colNum	= 4;	/* Start at column 4 */
			break;
		case 3:				/* Phase 3: */
			colStep	= 4;	/* 2 pixels per block of 8 per line */
			break;			/* Start at column 0 */
		case 4:				/* Phase 4: */
			colStep	= 4;	/* 2 pixels per block of 8 per line */
			colNum	= 2;	/* Start at column 2 */
			break;
		case 5:				/* Phase 5: */
			colStep	= 2;	/* 4 pixels per block of 8 per line */
			break;			/* Start at column 0 */
		case 6:				/* Phase 6: */
			colStep	= 2;	/* 4 pixels per block of 8 per line */
			colNum	= 1;	/* Start at column 1 */
			break;
							/* Phase 7: */
							/* 8 pixels per block of 8 per line */
							/* Start at column 0 */
		}
	}

	/* Calculate offset into pixelPtr for the first pixel of the line */

	offset	= pPNG -> mCurrLine * pPNG -> mBlock.pitch;

	/* Adjust up for the starting pixel of the line */

	offset += colNum * pPNG -> mBlock.pixelSize;

	/* Calculate the extra number of bytes to skip between columns */

	pixStep = (colStep - 1) * pPNG -> mBlock.pixelSize;

	for ( ; colNum < pPNG -> mBlock.width ; colNum += colStep)
	{
		if (haveBits < (pPNG -> mBitDepth * pPNG -> mChannels))
			haveBits = 0;

		for (chan = 0 ; chan < pPNG -> mChannels ; chan++)
		{
			if (!haveBits)
			{
				shifts = 0;

				readByte = *p++;

				haveBits += 8;
			}

			if (16 == pPNG -> mBitDepth)
			{
				pPNG->mBlock.pixelPtr[offset++] = readByte;

				if (pPNG -> mUseTRNS)
					pLastPixel[chan * 2] = readByte;

				readByte = *p++;

				if (pPNG -> mUseTRNS)
					pLastPixel[(chan * 2) + 1] = readByte;

				pPNG->mBlock.pixelPtr[offset++] = readByte;

				haveBits = 0;
				continue;
			}

			switch (pPNG -> mBitDepth)
			{
			case 1:
				pixBits = (Byte)((readByte >> (7 - shifts)) & 0x01);
				break;
			case 2:
				pixBits = (Byte)((readByte >> (6 - shifts*2)) & 0x03);
				break;
			case 4:
				pixBits = (Byte)((readByte >> (4 - shifts*4)) & 0x0f);
				break;
			case 8:
				pixBits = readByte;
				break;
			}

			if (PNG_COLOR_PLTE == pPNG -> mColorType)
			{
				pixelPtr[offset++] = pPNG -> mpPalette[pixBits].mRed;
				pixelPtr[offset++] = pPNG -> mpPalette[pixBits].mGrn;
				pixelPtr[offset++] = pPNG -> mpPalette[pixBits].mBlu;
				pixelPtr[offset++] = pPNG -> mpPalette[pixBits].mAlpha;
				chan += 2;
			}
			else
			{
				pixelPtr[offset++] = (Byte)(pixBits * pPNG -> mBitScale);

				if (pPNG -> mUseTRNS)
					pLastPixel[chan] = pixBits;
			}

			haveBits -= pPNG -> mBitDepth;
			shifts++;
		}

		/*
		 * Apply boolean transparency via tRNS data if necessary
		 * (where necessary means a tRNS chunk was provided and
		 * we're not using an alpha channel or indexed alpha).
		 */

		if ((PNG_COLOR_PLTE != pPNG -> mColorType) &&
			((pPNG -> mColorType & PNG_COLOR_ALPHA) == 0))
		{
			Byte alpha;

			if (pPNG -> mUseTRNS)
			{
				if (memcmp(pLastPixel, pPNG -> mpTrans, pPNG -> mBPP) == 0)
					alpha = 0x00;
				else
					alpha = 0xff;
			}
			else
			{
				alpha = 0xff;
			}

			pixelPtr[offset++] = alpha;

			if (16 == pPNG -> mBitDepth)
				pixelPtr[offset++] = alpha;
		}

		offset += pixStep;
	}

	if (pPNG -> mInterlace)
	{
		/* Skip lines */

		switch (pPNG -> mPhase)
		{
		case 1: case 2: case 3:
			pPNG -> mCurrLine += 8;
			break;
		case 4: case 5:
			pPNG -> mCurrLine += 4;
			break;
		case 6: case 7:
			pPNG -> mCurrLine += 2;
			break;
		}

		/* Start the next phase if there are no more lines to do */

		if (pPNG -> mCurrLine >= pPNG -> mBlock.height)
		{
			uLong pixels	= 0;

			while ((!pixels || (pPNG -> mCurrLine >= pPNG -> mBlock.height)) &&
				(pPNG->mPhase<7))
			{
				pPNG -> mPhase++;

				switch (pPNG -> mPhase)
				{
				case 2:
					pixels = (pPNG -> mBlock.width + 3) >> 3;
					pPNG -> mCurrLine	= 0;
					break;
				case 3:
					pixels = (pPNG -> mBlock.width + 3) >> 2;
					pPNG -> mCurrLine	= 4;
					break;
				case 4:
					pixels = (pPNG -> mBlock.width + 1) >> 2;
					pPNG -> mCurrLine	= 0;
					break;
				case 5:
					pixels = (pPNG -> mBlock.width + 1) >> 1;
					pPNG -> mCurrLine	= 2;
					break;
				case 6:
					pixels = (pPNG -> mBlock.width) >> 1;
					pPNG -> mCurrLine	= 0;
					break;
				case 7:
					pPNG -> mCurrLine	= 1;
					pixels				= pPNG -> mBlock.width;
					break;
				}
			}

			if (16 == pPNG -> mBitDepth)
			{
				pPNG -> mPhaseSz = 1 + (pPNG -> mChannels * pixels * 2);
			}
			else
			{
				pPNG -> mPhaseSz = 1 + (((pPNG -> mChannels * pixels *
					pPNG -> mBitDepth) + 7) >> 3);
			}
		}
	}
	else
	{
		pPNG -> mCurrLine++;
	}

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ReadIDAT --
 *
 *		This function reads the IDAT (pixel data) chunk from the
 *		PNG file to build the image.  It will continue reading until
 *		all IDAT chunks have been processed or an error occurs.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error occurs or an IDAT
 *		chunk is invalid.
 *
 * Side effects:
 *		The access position in f advances.  Memory may be allocated
 *		by zlib through PNGZAlloc.
 *
 *----------------------------------------------------------------------
 */

static int
ReadIDAT(Tcl_Interp* interp, PNGImage* pPNG, int chunkSz, uLong crc)
{
	Byte	pInput[PNG_BLOCK_SZ];

	/* Process IDAT contents until there is no more in this chunk */

	while (chunkSz)
	{
		int blockSz = PNG_MIN(chunkSz, PNG_BLOCK_SZ);
		int zresult;

		/* Read the next bit of IDAT chunk data, up to read buffer size */

		if (PNGRead(interp, pPNG, pInput, blockSz, &crc) == TCL_ERROR)
			return TCL_ERROR;

		chunkSz -= blockSz;

		/* Run inflate() until output buffer is not full. */

		pPNG -> mZStream.avail_in = blockSz;
		pPNG -> mZStream.next_in = pInput;

		do {
			zresult = inflate(&pPNG -> mZStream, Z_NO_FLUSH);

			switch (zresult)
			{
			case Z_STREAM_ERROR:
				break;
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				Tcl_SetResult(interp, "zlib inflation failed", TCL_STATIC);
				return TCL_ERROR;
			}

			/* Process pixels when a full scan line has been obtained */

			if (!pPNG -> mZStream.avail_out)
			{
				Byte*	temp;

				if (pPNG -> mPhase > 7)
				{
					Tcl_SetResult(interp,
						"Extra data after final scan line of final phase",
						TCL_STATIC);
					return TCL_ERROR;
				}

				if (DecodeLine(interp, pPNG) == TCL_ERROR)
					return TCL_ERROR;

				/*
				 * Swap the current/last lines so that we always have the last
				 * line processed available, which is necessary for filtering.
				 */

				temp = pPNG -> mpLastLine;
				pPNG -> mpLastLine = pPNG -> mpThisLine;
				pPNG -> mpThisLine = temp;

				/* Next pass through, inflate into the new current line */

				pPNG -> mZStream.avail_out	= (uInt)pPNG -> mPhaseSz;
				pPNG -> mZStream.next_out	= pPNG -> mpThisLine;
			}
		} while (pPNG -> mZStream.avail_in);

		/* Check for end of zlib stream */

		if (Z_STREAM_END == zresult)
		{
			if (chunkSz)
			{
				Tcl_SetResult(interp, "Extra data after end of zlib stream",
					TCL_STATIC);
				return TCL_ERROR;
			}

			break;
		}
	}

	if (CheckCRC(interp, pPNG, crc) == TCL_ERROR)
		return TCL_ERROR;

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ApplyAlpha --
 *
 *		Applies an overall alpha value to a complete image that has
 *		been read.  This alpha value is specified using the -format
 *		option to [image create photo].
 *
 * Results:
 *
 *		N/A
 *
 * Side effects:
 *		The access position in f may change.
 *
 *----------------------------------------------------------------------
 */
static
void ApplyAlpha(PNGImage* pPNG)
{
	if (pPNG->mAlpha != 1.0)
	{
		unsigned char*	p		= pPNG -> mBlock.pixelPtr;
		unsigned char*	pEnd	= p + pPNG -> mBlockSz;
		int				offset	= pPNG -> mBlock.offset[3];

		p += offset;

		if (16 == pPNG -> mBitDepth)
		{
			int channel;

			while (p < pEnd)
			{
				channel = (Byte)(((p[0] << 8) | p[1]) * pPNG->mAlpha);

				*p++ = (Byte)(channel >> 8);
				*p++ = (Byte)(channel & 0xff);

				p += offset;
			}
		}
		else
		{
			while (p < pEnd)
			{
				p[0] = (Byte)(pPNG->mAlpha * p[0]);
				p += 1 + offset;
			}
		}
	}
}


/*
 *----------------------------------------------------------------------
 *
 * ParseFormat --
 *
 *		This function parses the -format string that can be specified
 *		to the [image create photo] command to extract options for
 *		postprocessing of loaded images.  Currently, this just allows
 *		specifying and applying an overall alpha value to the loaded
 *		image (for example, to make it entirely 50% as transparent
 *		as the actual image file).
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if the format specification is invalid.
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

static int
ParseFormat(Tcl_Interp* interp, Tcl_Obj* pObjFmt, PNGImage* pPNG)
{
	Tcl_Obj**	objv		= NULL;
	int			objc		= 0;

	static const char* fmtOptions[] = {
		"png", "-alpha", (char *)NULL
	};

	enum fmtOptions {
		OPT_PNG, OPT_ALPHA
	};

	/* Extract elements of format specification as a list */

	if (pObjFmt && (Tcl_ListObjGetElements(interp, pObjFmt,
			&objc, &objv) == TCL_ERROR))
		return TCL_ERROR;

	while (objc)
	{
    	int optIndex;

        if (Tcl_GetIndexFromObj(interp, objv[0], fmtOptions, "option", 0,
				&optIndex) == TCL_ERROR)
            return TCL_ERROR;

		/* Ignore the "png" part of the format specification */

		if (OPT_PNG == optIndex)
		{
			objc--; objv++;
			continue;
		}

    	if (objc < 2)
    	{
        	Tcl_WrongNumArgs(interp, 1, objv, "value");
        	return TCL_ERROR;
    	}

		objc--; objv++;

		switch ((enum fmtOptions) optIndex)
		{
		case OPT_PNG:
			break;

		case OPT_ALPHA:
			if (Tcl_GetDoubleFromObj(interp, objv[0], &pPNG->mAlpha) == TCL_ERROR)
				return TCL_ERROR;

			if ((pPNG->mAlpha < 0.0) || (pPNG->mAlpha > 1.0))
			{
				Tcl_SetResult(interp,
					"-alpha value must be between 0.0 and 1.0",
					TCL_STATIC);
				return TCL_ERROR;
			}
			break;
		}

		objc--; objv++;
	}

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * PNGDecode --
 *
 *		This function handles the entirety of reading a PNG file (or
 *		data) from the first byte to the last.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O error occurs or any problems
 *		are detected in the PNG file.
 *
 * Side effects:
 *		The access position in f advances.  Memory may be allocated
 *		and image dimensions and contents may change.
 *
 *----------------------------------------------------------------------
 */

static int
PNGDecode(Tcl_Interp* interp, PNGImage* pPNG, Tcl_Obj* pObjFmt,
	Tk_PhotoHandle imageHandle, int destX, int destY)
{
	uLong		chunkType;
	int			chunkSz;
	uLong		crc;

	/* Parse the PNG signature and IHDR (header) chunk */

	if (ReadIHDR(interp, pPNG) == TCL_ERROR)
		return TCL_ERROR;

	/* Extract alpha value from -format object, if specified */

	if (ParseFormat(interp, pObjFmt, pPNG) == TCL_ERROR)
		return TCL_ERROR;

	/*
	 * The next chunk may either be a PLTE (Palette) chunk or the first
	 * of at least one IDAT (data) chunks.  It could also be one of
	 * a number of ancillary chunks, but those are skipped for us by
	 * the switch in ReadChunkHeader().
	 *
	 * PLTE is mandatory for color type 3 and forbidden for 2 and 6
	 */

	if (ReadChunkHeader(interp, pPNG, &chunkSz, &chunkType,
			&crc) == TCL_ERROR)
		return TCL_ERROR;

	if (CHUNK_PLTE == chunkType)
	{
		/* Finish parsing the PLTE chunk */

		if (ReadPLTE(interp, pPNG, chunkSz, crc) == TCL_ERROR)
			return TCL_ERROR;

		/* Begin the next chunk */

		if (ReadChunkHeader(interp, pPNG, &chunkSz, &chunkType,
				&crc) == TCL_ERROR)
			return TCL_ERROR;
	}
	else
	if (PNG_COLOR_PLTE == pPNG -> mColorType)
	{
		Tcl_SetResult(interp, "PLTE chunk required for indexed color",
			TCL_STATIC);
		return TCL_ERROR;
	}

	/*
	 * The next chunk may be a tRNS (palette transparency) chunk,
	 * depending on the color type. It must come after the PLTE
	 * chunk and before the IDAT chunk, but can be present if there
	 * is no PLTE chunk because it can be used for Grayscale and
	 * TrueColor in lieu of an alpha channel.
	 */

	if (CHUNK_tRNS == chunkType)
	{
		/* Finish parsing the tRNS chunk */

		if (ReadtRNS(interp, pPNG, chunkSz, crc) == TCL_ERROR)
			return TCL_ERROR;

		/* Begin the next chunk */

		if (ReadChunkHeader(interp, pPNG, &chunkSz, &chunkType,
				&crc) == TCL_ERROR)
			return TCL_ERROR;
	}

	/*
	 * Other ancillary chunk types could appear here, but for now we're
	 * only interested in IDAT.  The others should have been skipped.
	 */

	if (CHUNK_IDAT != chunkType)
	{
		Tcl_SetResult(interp, "At least one IDAT chunk is required",
			TCL_STATIC);
		return TCL_ERROR;
	}

	/*
	 * Expand the photo size (if not set by the user) to provide enough
	 * space for the image being parsed.  It does not matter if width
	 * or height wrap to negative here: Tk will not shrink the image.
	 */

#ifndef USE_PANIC_ON_PHOTO_ALLOC_FAILURE
#if ((TCL_MAJOR_VERSION > 8) || \
	((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 5)))
	if (Tk_PhotoExpand(interp, imageHandle, destX + pPNG -> mBlock.width,
			destY + pPNG -> mBlock.height) == TCL_ERROR)
		return TCL_ERROR;
#else
	Tk_PhotoExpand(imageHandle, destX + pPNG -> mBlock.width,
		destY + pPNG -> mBlock.height);
#endif
#endif /* !USE_PANIC_ON_PHOTO_ALLOC_FAILURE */

	/*
	 * A scan line consists of one byte for a filter type, plus
	 * the number of bits per color sample times the number of
	 * color samples per pixel.
	 */

	if (pPNG -> mBlock.width > ((INT_MAX - 1) / (pPNG -> mChannels * 2)))
	{
		Tcl_SetResult(interp,
			"Line size is out of supported range on this architecture",
			TCL_STATIC);
		return TCL_ERROR;
	}

	if (16 == pPNG -> mBitDepth)
	{
		pPNG -> mLineSz = 1 + (pPNG -> mChannels * pPNG -> mBlock.width * 2);
	}
	else
	{
		pPNG -> mLineSz = 1 + ((pPNG -> mChannels * pPNG -> mBlock.width) /
			(8 / pPNG -> mBitDepth));
		if (pPNG -> mBlock.width % (8 / pPNG -> mBitDepth))
			pPNG -> mLineSz++;
	}

	/* Allocate space for decoding the scan lines */

	pPNG -> mpLastLine	= (Byte*)attemptckalloc(pPNG -> mLineSz);
	pPNG -> mpThisLine	= (Byte*)attemptckalloc(pPNG -> mLineSz);
	pPNG -> mBlock.pixelPtr	= (Byte*)attemptckalloc(pPNG -> mBlockSz);

	if (!pPNG -> mpLastLine || !pPNG -> mpThisLine || !pPNG -> mBlock.pixelPtr)
	{
		Tcl_SetResult(interp, "Memory allocation failed", TCL_STATIC);
		return TCL_ERROR;
	}

	/*
	 * Determine size of the first phase if interlaced.  Phase size should
	 * always be <= line size, so probably not necessary to check for
	 * arithmetic overflow here: should be covered by line size check.
	 */

	if (pPNG -> mInterlace)
	{
		/* Only one pixel per block of 8 per line in the first phase */

		uInt	pixels = (pPNG -> mBlock.width + 7) >> 3;
		pPNG -> mPhase = 1;

		if (16 == pPNG -> mBitDepth)
		{
			pPNG -> mPhaseSz = 1 + (pPNG -> mChannels * pixels * 2);
		}
		else
		{
			pPNG -> mPhaseSz = 1 +
				(((pPNG -> mChannels * pixels * pPNG -> mBitDepth) + 7) >> 3);
		}
	}
	else
	{
		pPNG -> mPhaseSz = pPNG -> mLineSz;
	}
	pPNG -> mZStream.avail_out	= pPNG -> mPhaseSz;
	pPNG -> mZStream.next_out	= pPNG -> mpThisLine;

	/* All of the IDAT (data) chunks must be consecutive */

	while (CHUNK_IDAT == chunkType)
	{
		if (ReadIDAT(interp, pPNG, chunkSz, crc) == TCL_ERROR)
			return TCL_ERROR;

		if (ReadChunkHeader(interp, pPNG, &chunkSz, &chunkType,
				&crc) == TCL_ERROR)
			return TCL_ERROR;
	}

	/* Now skip the remaining chunks which we're also not interested in. */

	while (CHUNK_IEND != chunkType)
	{
		if (SkipChunk(interp, pPNG, chunkSz, crc) == TCL_ERROR)
			return TCL_ERROR;

		if (ReadChunkHeader(interp, pPNG, &chunkSz, &chunkType,
				&crc) == TCL_ERROR)
			return TCL_ERROR;
	}

	/* Got the IEND (end of image) chunk.  Do some final checks... */

	if (chunkSz)
	{
		Tcl_SetResult(interp, "IEND chunk contents must be empty", TCL_STATIC);
		return TCL_ERROR;
	}

	/* Check the CRC on the IEND chunk */

	if (CheckCRC(interp, pPNG, crc) == TCL_ERROR)
		return TCL_ERROR;

	/*
	 * TODO: verify that nothing else comes after the IEND chunk, or do
	 * we really care?
	 */

#if 0
	if (PNGRead(interp, pPNG, &c, 1, NULL) != TCL_ERROR)
	{
		Tcl_SetResult(interp, "Extra data following IEND chunk", TCL_STATIC);
		return TCL_ERROR;
	}
#endif

	/* Apply overall image alpha if specified */

	ApplyAlpha(pPNG);

	/* Copy the decoded image block into the Tk photo image */

#ifndef USE_PANIC_ON_PHOTO_ALLOC_FAILURE
#if ((TCL_MAJOR_VERSION > 8) || \
	((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 5)))
	if (Tk_PhotoPutBlock(interp, imageHandle, &pPNG -> mBlock, destX, destY,
			pPNG -> mBlock.width, pPNG -> mBlock.height,
			TK_PHOTO_COMPOSITE_SET) == TCL_ERROR)
		return TCL_ERROR;
#else
	Tk_PhotoPutBlock(imageHandle, &pPNG -> mBlock, destX, destY,
		pPNG -> mBlock.width, pPNG -> mBlock.height, TK_PHOTO_COMPOSITE_SET);
#endif
#endif /* !USE_PANIC_ON_PHOTO_ALLOC_FAILURE */

	return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * FileMatchPNG --
 *
 *		This function is invoked by the photo image type to see if
 *		a file contains image data in PNG format.
 *
 * Results:
 *		The return value is 1 if the first characters in file f look
 *		like PNG data, and 0 otherwise.
 *
 * Side effects:
 *		The access position in f may change.
 *
 *----------------------------------------------------------------------
 */

static int
FileMatchPNG(Tcl_Channel chan, CONST char *fileName, Tcl_Obj *pObjFmt,
	int *widthPtr, int *heightPtr, Tcl_Interp *interp)
{
	PNGImage		png;
	int				match = 0;
	Tcl_SavedResult	sya;

	Tcl_SaveResult(interp, &sya);

	PNGInit(interp, &png, chan, NULL, PNG_DECODE);

	if (ReadIHDR(interp, &png) == TCL_OK)
	{
		*widthPtr	= png.mBlock.width;
		*heightPtr	= png.mBlock.height;
		match		= 1;
	}

	PNGCleanup(&png);
	Tcl_RestoreResult(interp, &sya);

	return match;
}


/*
 *----------------------------------------------------------------------
 *
 * FileReadPNG --
 *
 *		This function is called by the photo image type to read
 *		PNG format data from a file and write it into a given
 *		photo image.
 *
 * Results:
 *		A standard TCL completion code.  If TCL_ERROR is returned
 *		then an error message is left in the interp's result.
 *
 * Side effects:
 *		The access position in file f is changed, and new data is
 *		added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int
FileReadPNG(Tcl_Interp *interp, Tcl_Channel chan, CONST char *fileName,
	Tcl_Obj *pObjFmt, Tk_PhotoHandle imageHandle, int destX, int destY,
	int width, int height, int srcX, int srcY)
{
	PNGImage	png;
	int			result		= TCL_ERROR;

	result = PNGInit(interp, &png, chan, NULL, PNG_DECODE);

	if (TCL_OK == result)
		result = PNGDecode(interp, &png, pObjFmt, imageHandle, destX, destY);

	PNGCleanup(&png);
	return result;
}


/*
 *----------------------------------------------------------------------
 *
 * StringMatchPNG --
 *
 *  This function is invoked by the photo image type to see if
 *  an object contains image data in PNG format.
 *
 * Results:
 *  The return value is 1 if the first characters in the data are
 *  like PNG data, and 0 otherwise.
 *
 * Side effects:
 *  the size of the image is placed in widthPre and heightPtr.
 *
 *----------------------------------------------------------------------
 */

static int
StringMatchPNG(Tcl_Obj *pObjData, Tcl_Obj *pObjFmt, int *widthPtr,
	int *heightPtr, Tcl_Interp *interp)
{
	PNGImage		png;
	int				match		= 0;
	Tcl_SavedResult	sya;

	Tcl_SaveResult(interp, &sya);
	PNGInit(interp, &png, (Tcl_Channel)NULL, pObjData, PNG_DECODE);

	png.mpStrData = Tcl_GetByteArrayFromObj(pObjData, &png.mStrDataSz);

	if (ReadIHDR(interp, &png) == TCL_OK)
	{
		*widthPtr	= png.mBlock.width;
		*heightPtr	= png.mBlock.height;
		match		= 1;
	}

	PNGCleanup(&png);
	Tcl_RestoreResult(interp, &sya);

	return match;
}


/*
 *----------------------------------------------------------------------
 *
 * StringReadPNG --
 *
 *		This function is called by the photo image type to read
 *		PNG format data from an object and give it to the photo image.
 *
 * Results:
 *		A standard TCL completion code.  If TCL_ERROR is returned
 *		then an error message is left in the interp's result.
 *
 * Side effects:
 *		new data is added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

static int
StringReadPNG(Tcl_Interp *interp, Tcl_Obj *pObjData, Tcl_Obj *pObjFmt,
	Tk_PhotoHandle imageHandle, int destX, int destY, int width, int height,
	int srcX, int srcY)
{
	PNGImage	png;
	int			result		= TCL_ERROR;

	result = PNGInit(interp, &png, (Tcl_Channel)NULL, pObjData, PNG_DECODE);

	if (TCL_OK == result)
		result = PNGDecode(interp, &png, pObjFmt, imageHandle, destX, destY);

	PNGCleanup(&png);
	return result;
}


/*
 *----------------------------------------------------------------------
 *
 * PNGWrite --
 *
 *		This function writes a bytes from a buffer out to the PNG image.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if the write fails.
 *
 * Side effects:
 *		File or buffer will be modified.
 *
 *----------------------------------------------------------------------
 */

static int
PNGWrite(Tcl_Interp* interp, PNGImage* pPNG,
	const Byte* pSrc, int srcSz, uLong* pCRC)
{
	if (!pSrc || !srcSz)
		return TCL_OK;

	if (pCRC)
		*pCRC = crc32(*pCRC, pSrc, srcSz);

	/*
	 * TODO: is Tcl_AppendObjToObj faster here? i.e., does Tcl join
	 * the objects immediately or store them in a multi-object rep?
	 */

	if (pPNG -> mpObjData)
	{
		int		objSz;
		Byte*	pDest;

		Tcl_GetByteArrayFromObj(pPNG->mpObjData, &objSz);

		if (objSz > INT_MAX - srcSz)
		{
			Tcl_SetResult(interp,
				"Image too large to store completely in byte array",
				TCL_STATIC);
			return TCL_ERROR;
		}

		pDest = Tcl_SetByteArrayLength(pPNG->mpObjData, objSz + srcSz);

		if (!pDest)
		{
			Tcl_SetResult(interp, "Memory allocation failed", TCL_STATIC);
			return TCL_ERROR;
		}

		memcpy(pDest+objSz, pSrc, srcSz);
	}
	else
	if (Tcl_Write(pPNG -> mChannel, (CONST char*)pSrc, srcSz) < 0)
	{
		/* TODO: reason */

		Tcl_SetResult(interp, "Write to channel failed", TCL_STATIC);
		return TCL_ERROR;
	}

	return TCL_OK;
}

static int
PNGWriteByte(Tcl_Interp* interp, PNGImage* pPNG, Byte c, uLong* pCRC)
{
	return PNGWrite(interp, pPNG, &c, 1, pCRC);
}


/*
 *----------------------------------------------------------------------
 *
 * PNGWriteInt32 --
 *
 *		This function writes a 32-bit integer value out to the PNG
 *		image as four bytes in network byte order.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if the write fails.
 *
 * Side effects:
 *		File or buffer will be modified.
 *
 *----------------------------------------------------------------------
 */

static int
PNGWriteInt32(Tcl_Interp* interp, PNGImage* pPNG, uLong L, uLong* pCRC)
{
	Byte	pc[4];

	pc[0] = (Byte)((L & 0xff000000) >> 24);
	pc[1] = (Byte)((L & 0x00ff0000) >> 16);
	pc[2] = (Byte)((L & 0x0000ff00) >> 8);
	pc[3] = (Byte)((L & 0x000000ff) >> 0);

	return PNGWrite(interp, pPNG, pc, 4, pCRC);
}


/*
 *----------------------------------------------------------------------
 *
 * PNGWriteChunk --
 *
 *		Writes a complete chunk to the PNG image, including chunk type,
 *		length, contents, and CRC.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if the write fails.
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

static int
PNGWriteChunk(Tcl_Interp* interp, PNGImage* pPNG,
	uLong chunkType, const Byte* pData, int dataSz)
{
	uLong	crc		= crc32(0, NULL, 0);
	int		result	= TCL_OK;

	/* Write the length field for the chunk */

	result = PNGWriteInt32(interp, pPNG, dataSz, NULL);

	/* Write the Chunk Type */

	if (TCL_OK == result)
		result = PNGWriteInt32(interp, pPNG, chunkType, &crc);

	/* Write the contents (if any) */

	if (TCL_OK == result)
		result = PNGWrite(interp, pPNG, pData, dataSz, &crc);

	/* Write out the CRC at the end of the chunk */

	if (TCL_OK == result)
		result = PNGWriteInt32(interp, pPNG, crc, NULL);

	return result;
}


/*
 *----------------------------------------------------------------------
 *
 * WriteIHDR --
 *
 *		This function writes the PNG header at the beginning of a
 *		PNG file, which includes information such as dimensions
 *		and color type.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if the write fails.
 *
 * Side effects:
 *		File or buffer will be modified.
 *
 *----------------------------------------------------------------------
 */

static int
WriteIHDR(Tcl_Interp* interp, PNGImage* pPNG, Tk_PhotoImageBlock *blockPtr)
{
	uLong	crc		= crc32(0, NULL, 0);
	int		result	= TCL_OK;

	/* The IHDR (header) chunk has a fixed size of 13 bytes */

	result = PNGWriteInt32(interp, pPNG, 13, NULL);

	/* Write the IHDR Chunk Type */

	if (TCL_OK == result)
		result = PNGWriteInt32(interp, pPNG, CHUNK_IHDR, &crc);

	/* Write the image width, height */

	if (TCL_OK == result)
		result = PNGWriteInt32(interp, pPNG, (uLong)blockPtr->width, &crc);

	if (TCL_OK == result)
		result = PNGWriteInt32(interp, pPNG, (uLong)blockPtr->height, &crc);

	/*
	 * Write bit depth.  Although the PNG format supports 16 bits
	 * per channel, Tk supports only 8 in the internal representation,
	 * which blockPtr points to.
	 */

	if (TCL_OK == result)
		result = PNGWriteByte(interp, pPNG, 8, &crc);

	/* Write out the color type, previously determined */

	if (TCL_OK == result)
		result = PNGWriteByte(interp, pPNG, pPNG->mColorType, &crc);

	/* Write compression method (only one method is defined) */

	if (TCL_OK == result)
		result = PNGWriteByte(interp, pPNG, PNG_COMPRESS_DEFLATE, &crc);

	/* Write filter method (only one method is defined) */

	if (TCL_OK == result)
		result = PNGWriteByte(interp, pPNG, PNG_FILTMETH_STANDARD, &crc);

	/* Write interlace method as not interlaced */
	/* TODO: support interlace through -format? */

	if (TCL_OK == result)
		result = PNGWriteByte(interp, pPNG, PNG_INTERLACE_NONE, &crc);

	/* Write out the CRC at the end of the chunk */

	if (TCL_OK == result)
		result = PNGWriteInt32(interp, pPNG, crc, NULL);

	return result;
}


/*
 *----------------------------------------------------------------------
 *
 * WriteIDAT --
 *
 *		Writes the IDAT (data) chunk to the PNG image, containing the
 *		pixel channel data.  Currently, image lines are not filtered
 *		and writing interlaced pixels is not supported.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if the write fails.
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

static int
WriteIDAT(Tcl_Interp* interp, PNGImage* pPNG, Tk_PhotoImageBlock *blockPtr)
{
	int		rowNum;

	/* Filter and compress each row one at a time */

	for (rowNum = 0 ; rowNum < blockPtr->height ; rowNum++)
	{
		int		colNum;
		Byte*	pSrc;
		Byte*	pDest;
		Byte*	temp;

		pSrc	= blockPtr->pixelPtr + (rowNum * blockPtr->pitch);
		pDest	= pPNG -> mpThisLine;

		/* TODO: use Paeth filtering */

		*pDest++ = PNG_FILTER_NONE;

		/*
		 * Copy each pixel into the destination buffer after the
		 * filter type before filtering.
		 */

		for (colNum = 0 ; colNum < blockPtr->width ; colNum++)
		{
			/* Copy red or gray channel */

			*pDest++ = pSrc[blockPtr->offset[0]];

			/* If not grayscale, copy the green and blue channels */

			if (pPNG->mColorType & PNG_COLOR_USED)
			{
				*pDest++ = pSrc[blockPtr->offset[1]];
				*pDest++ = pSrc[blockPtr->offset[2]];
			}

			/* Copy the alpha channel, if used */

			if (pPNG->mColorType & PNG_COLOR_ALPHA)
			{
				*pDest++ = pSrc[blockPtr->offset[3]];
			}

			/* Point to the start of the next pixel */

			pSrc += blockPtr->pixelSize;
		}

		/*
		 * Compress the line of pixels into the destination, continuing
		 * until the entire input is consumed by zlib.
		 */

		pPNG->mZStream.avail_in	= pPNG -> mLineSz;
		pPNG->mZStream.next_in	= pPNG -> mpThisLine;

		do {
			if (deflate(&pPNG->mZStream, 0) != Z_OK)
			{
				Tcl_SetResult(interp, "deflate() returned error", TCL_STATIC);
				return TCL_ERROR;
			}
		} while (pPNG->mZStream.avail_in);

		/* Swap line buffers to keep the last around for filtering next */

		temp = pPNG -> mpLastLine;
		pPNG -> mpLastLine = pPNG -> mpThisLine;
		pPNG -> mpThisLine = temp;
	}

	/* Finish compressing any remaining input that hasn't been output yet */

	if (deflate(&pPNG->mZStream, Z_SYNC_FLUSH) != Z_OK)
	{
		Tcl_SetResult(interp, "deflate() returned error on finish", TCL_STATIC);
		return TCL_ERROR;
	}

	/*
	 * Trim remaining buffer space from the IDAT content to be written.
	 * This is the difference between the uncompressed and compressed
	 * sizes of the IDAT contents.
	 */

	pPNG->mBlockSz -= pPNG->mZStream.avail_out;

	/* Write out the compressed data as one big IDAT chunk */

	return PNGWriteChunk(interp, pPNG, CHUNK_IDAT, pPNG->mBlock.pixelPtr,
		(uLong)pPNG->mBlockSz);
}

/*
 *----------------------------------------------------------------------
 *
 * PNGEncode --
 *
 *		This function handles the entirety of writing a PNG file (or
 *		data) from the first byte to the last.  No effort is made to
 *		optimize the image data for best compression.
 *
 * Results:
 *		TCL_OK, or TCL_ERROR if an I/O or memory error occurs.
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

static int
PNGEncode(Tcl_Interp* interp, Tk_PhotoImageBlock *blockPtr, PNGImage* pPNG)
{
	int		greenOffset;
	int		blueOffset;
	int		alphaOffset;

	/*
	 * Determine appropriate color type based on color usage (e.g.,
	 * only red and maybe alpha channel = grayscale).
	 */

	greenOffset = blockPtr->offset[1] - blockPtr->offset[0];
	blueOffset = blockPtr->offset[2] - blockPtr->offset[0];
	alphaOffset = blockPtr->offset[3];
	if ((alphaOffset >= blockPtr->pixelSize) || (alphaOffset < 0)) {
		alphaOffset = 0;
	} else {
		alphaOffset -= blockPtr->offset[0];
	}

	if ((greenOffset != 0) || (blueOffset != 0)) {
		if (alphaOffset) {
			pPNG -> mColorType	= PNG_COLOR_RGBA;
			pPNG -> mBPP		= 4;
		} else {
			pPNG -> mColorType	= PNG_COLOR_RGBA;
			pPNG -> mBPP		= 3;
		}
	} else {
		if (alphaOffset) {
			pPNG -> mColorType	= PNG_COLOR_GRAYALPHA;
			pPNG -> mBPP		= 2;
		} else {
			pPNG -> mColorType	= PNG_COLOR_GRAY;
			pPNG -> mBPP		= 1;
		}
	}

	/* Allocate buffers for lines for filtering and compressed data */

	pPNG -> mLineSz = 1 + (pPNG->mBPP * blockPtr->width);
	pPNG -> mBlockSz = pPNG -> mLineSz * blockPtr->height;

	if ((blockPtr->width > (INT_MAX - 1) / (pPNG->mBPP)) ||
		(blockPtr->height > INT_MAX / pPNG->mLineSz))
	{
		Tcl_SetResult(interp,
			"Image is too large to encode pixel data",
			TCL_STATIC);
		return TCL_ERROR;
	}

	pPNG -> mpLastLine = (Byte*)attemptckalloc(pPNG -> mLineSz);
	pPNG -> mpThisLine = (Byte*)attemptckalloc(pPNG -> mLineSz);
	pPNG -> mBlock.pixelPtr	= (Byte*)attemptckalloc(pPNG -> mBlockSz);

	if (!pPNG -> mpLastLine || !pPNG -> mpThisLine || !pPNG -> mBlock.pixelPtr)
	{
		Tcl_SetResult(interp, "Memory allocation failed", TCL_STATIC);
		return TCL_ERROR;
	}

	/* Set up zlib stream for compressed output */

	pPNG -> mZStream.next_out	= pPNG -> mBlock.pixelPtr;
	pPNG -> mZStream.avail_out	= pPNG -> mBlockSz;

	/* Write out the PNG Signature that all PNGs begin with */

	if (PNGWrite(interp, pPNG, gspPNGSignature,
			PNG_SIG_SZ, NULL) == TCL_ERROR)
		return TCL_ERROR;

	/*
	 * Write out the IHDR (header) chunk containing image dimensions,
	 * color type, etc.
	 */

	if (WriteIHDR(interp, pPNG, blockPtr) == TCL_ERROR)
		return TCL_ERROR;

	/* Write out the image pixels in the IDAT (data) chunk */

	if (WriteIDAT(interp, pPNG, blockPtr) == TCL_ERROR)
		return TCL_ERROR;

	/* Write out the IEND chunk that all PNGs end with */

	return PNGWriteChunk(interp, pPNG, CHUNK_IEND, NULL, 0);
}


/*
 *----------------------------------------------------------------------
 *
 * FileWritePNG --
 *
 *		This function is called by the photo image type to write
 *		PNG format data to a file.
 *
 * Results:
 *		A standard TCL completion code.  If TCL_ERROR is returned
 *		then an error message is left in the interp's result.
 *
 * Side effects:
 *		The specified file is overwritten.
 *
 *----------------------------------------------------------------------
 */

static int
FileWritePNG(Tcl_Interp* interp, CONST char* filename, Tcl_Obj* pObjFmt,
	Tk_PhotoImageBlock *blockPtr)
{
	Tcl_Channel	chan;
	PNGImage	png;
	int			result		= TCL_ERROR;

	/*
	 * Open a Tcl file channel where the image data will be stored.
	 * Tk ought to take care of this, and just provide a channel,
	 * but it doesn't.
	 */

	chan = Tcl_OpenFileChannel(interp, filename, "w", 0644);

	if (!chan)
		return TCL_ERROR;

	/* Initalize PNGImage instance for encoding */

	if (PNGInit(interp, &png, chan, NULL, PNG_ENCODE) == TCL_ERROR)
		goto cleanup;

	/*
	 * Set the translation mode to binary so that CR and LF are not
	 * to the platform's EOL sequence.
	 */

	if (Tcl_SetChannelOption(interp, chan, "-translation", "binary") != TCL_OK)
		goto cleanup;

	if (Tcl_SetChannelOption(interp, chan, "-encoding", "binary") != TCL_OK)
		goto cleanup;

	/* Write the raw PNG data out to the file */

	result = PNGEncode(interp, blockPtr, &png);

cleanup:
	Tcl_Close(interp, chan);
	PNGCleanup(&png);
	return result;
}


/*
 *----------------------------------------------------------------------
 *
 * StringWritePNG --
 *
 *		This function is called by the photo image type to write
 *		PNG format data to a Tcl object and return it in the result.
 *
 * Results:
 *		A standard TCL completion code.  If TCL_ERROR is returned
 *		then an error message is left in the interp's result.
 *
 * Side effects:
 *		None
 *
 *----------------------------------------------------------------------
 */

static int
StringWritePNG(Tcl_Interp* interp, Tcl_Obj* pObjFmt,
	Tk_PhotoImageBlock *blockPtr)
{
	Tcl_Obj*	pObjResult	= Tcl_NewObj();
	PNGImage	png;
	int			result		= TCL_ERROR;

	/* Initalize PNGImage instance for encoding */

	if (PNGInit(interp, &png, NULL, pObjResult, PNG_ENCODE) == TCL_ERROR)
		goto cleanup;

	/*
	 * Write the raw PNG data into the prepared Tcl_Obj buffer
	 * Set the result back to the interpreter if successful.
	 */

	result = PNGEncode(interp, blockPtr, &png);

	if (TCL_OK == result)
		Tcl_SetObjResult(interp, png.mpObjData);

cleanup:
	PNGCleanup(&png);
	return result;
}

