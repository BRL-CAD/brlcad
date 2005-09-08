/*
 * bltTile.c --
 *
 *	This module manages images for tiled backgrounds for the BLT toolkit.
 *
 * Copyright 1995-1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 */

#include "bltInt.h"
#include "bltChain.h"
#include "bltHash.h"
#include "bltImage.h"
#include <X11/Xutil.h>

#include "bltTile.h"

#define TILE_THREAD_KEY	"BLT Tile Data"
#define TILE_MAGIC ((unsigned int) 0x46170277)

typedef struct {
    Blt_HashTable tileTable;	/* Hash table of tile structures keyed by 
				 * the name of the image. */
    Tcl_Interp *interp;
} TileInterpData;

typedef struct {
    char *name;			/* Name of image used to generate the pixmap.*/
    Display *display;		/* Display where pixmap was created. */
    int flags;			/* See definitions below. */
    Tcl_Interp *interp;
    Blt_HashEntry *hashPtr;	/* Pointer to hash table location */
    Blt_HashTable *tablePtr;

    Pixmap pixmap;		/* Pixmap generated from image. */
    Pixmap mask;		/* Monochrome pixmap used as
				 * transparency mask. */
    GC gc;			/* GC */
    Tk_Image tkImage;		/* Tk image token. */
    Blt_Chain *clients;		/* Chain of clients sharing this tile. */
    int width, height;		
} Tile;

#define NOTIFY_PENDING	1	/* If set, indicates that the image
				 * associated with the tile has been
				 * updated or deleted.  The tile pixmap
				 * will be changed and the clients of the
				 * tile will be notified (if they supplied
				 * a TileChangedProc routine. */

typedef struct Blt_TileClientStruct {
    unsigned int magic;
    Tk_Window tkwin;		/* Client window. */
    int xOrigin, yOrigin;	/* Tiling origin in relation to the
				 * client window. */
    Blt_TileChangedProc *notifyProc; /* If non-NULL, routine to
				 * call to when tile image changes. */
    ClientData clientData;	/* Data to pass to when calling the above
				 * routine. */
    Tile *tilePtr;		/* Pointer to actual tile information */
    Blt_ChainLink *linkPtr;	/* Pointer to client entry in the server's
				 * client list.  Used to delete the client */
} TileClient;

typedef struct {
    Display *display;
    Tk_Uid nameId;
    int depth;
} TileKey;
    
static TileInterpData *GetTileInterpData _ANSI_ARGS_((Tcl_Interp *interp));

static Tcl_IdleProc UpdateTile;
static Tk_ImageChangedProc ImageChangedProc;
static Tcl_InterpDeleteProc TileInterpDeleteProc;

static void DestroyClient _ANSI_ARGS_((TileClient *clientPtr));
static void DestroyTile _ANSI_ARGS_((Tile *tilePtr));


/*
 *----------------------------------------------------------------------
 *
 * RedrawTile --
 *
 *	Generates a pixmap and draws the tile image into it.  Also
 *	a tranparency mask is possibly generated from the image.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
RedrawTile(tkwin, tilePtr)
    Tk_Window tkwin;
    Tile *tilePtr;
{
    GC newGC;
    Tk_PhotoHandle photo;
    XGCValues gcValues;
    int width, height;
    unsigned int gcMask;
    
    Tk_SizeOfImage(tilePtr->tkImage, &width, &height);

    Tk_MakeWindowExist(tkwin);
    if ((width != tilePtr->width) || (height != tilePtr->height)) {
	Pixmap pixmap;

	/*
	 * Create the new pixmap *before* destroying the old one.  I don't
	 * why this happens, but if you delete the old pixmap first, the
	 * old pixmap sometimes gets used in the client's GCs.  I suspect
	 * it has something to do with the way Tk reallocates X resource
	 * identifiers.  
	 */
	pixmap = Tk_GetPixmap(Tk_Display(tkwin), Tk_WindowId(tkwin), width, 
			      height, Tk_Depth(tkwin));
	if (tilePtr->pixmap != None) {
	    Tk_FreePixmap(Tk_Display(tkwin), tilePtr->pixmap);
	}
	tilePtr->pixmap = pixmap;
    }
    Tk_RedrawImage(tilePtr->tkImage, 0, 0, width, height, tilePtr->pixmap, 
	0, 0);
    
    gcMask = (GCTile | GCFillStyle);
    gcValues.fill_style = FillTiled;
    gcValues.tile = tilePtr->pixmap;
    newGC = Tk_GetGC(tkwin, gcMask, &gcValues);
    if (tilePtr->gc != NULL) {
	Tk_FreeGC(Tk_Display(tkwin), tilePtr->gc);
    }
    tilePtr->gc = newGC;
    tilePtr->width = width;
    tilePtr->height = height;

    if (tilePtr->mask != None) {
#ifdef WIN32
	Tk_FreePixmap(Tk_Display(tkwin), tilePtr->mask);
#else 
	XFreePixmap(Tk_Display(tkwin), tilePtr->mask);
#endif /* WIN32 */
	tilePtr->mask = None;
    }
    photo = Blt_FindPhoto(tilePtr->interp, 
			  Blt_NameOfImage(tilePtr->tkImage));
    if (photo != NULL) {
	Tk_PhotoImageBlock src;

	Tk_PhotoGetImage(photo, &src);
	if ((src.offset[3] < src.pixelSize) && (src.offset[3] >= 0)) {
	    tilePtr->mask = Blt_PhotoImageMask(tkwin, src);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateTile --
 *
 *	It would be better if Tk checked for NULL proc pointers.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
UpdateTile(clientData)
    ClientData clientData;
{
    Tile *tilePtr = (Tile *)clientData;
    TileClient *clientPtr;
    Blt_ChainLink *linkPtr;

    tilePtr->flags &= ~NOTIFY_PENDING;
    if (Tk_ImageIsDeleted(tilePtr->tkImage)) {
	if (tilePtr->pixmap != None) {
	    Tk_FreePixmap(tilePtr->display, tilePtr->pixmap);
	}
	tilePtr->pixmap = None;
    } else {
	/* Pick any client window to generate the new pixmap. */
	linkPtr = Blt_ChainFirstLink(tilePtr->clients);
	clientPtr = Blt_ChainGetValue(linkPtr);
	RedrawTile(clientPtr->tkwin, tilePtr);
    }

    /* Notify each of the tile's clients that the pixmap has changed. */

    for (linkPtr = Blt_ChainFirstLink(tilePtr->clients); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	clientPtr = Blt_ChainGetValue(linkPtr);
	if (clientPtr->notifyProc != NULL) {
	    (*clientPtr->notifyProc) (clientPtr->clientData, clientPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ImageChangedProc
 *
 *	The Tk image has changed or been deleted, redraw the pixmap
 *	tile.
 *
 *	Note:	As of Tk 4.2 (rechecked in 8.3), if you redraw Tk 
 *		images from a Tk_ImageChangedProc you'll get a 
 *		coredump.  As a workaround, we have to simulate 
 *		how the Tk widgets use images and redraw within 
 *		an idle event.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static void
ImageChangedProc(clientData, x, y, width, height, imageWidth, imageHeight)
    ClientData clientData;
    int x, y, width, height;	/* Not used. */
    int imageWidth, imageHeight; /* Not used. */
{
    Tile *tilePtr = (Tile *) clientData;

    if (!(tilePtr->flags & NOTIFY_PENDING)) {
	Tcl_DoWhenIdle(UpdateTile, tilePtr);
	tilePtr->flags |= NOTIFY_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyTile --
 *
 *	Deletes the core tile structure, including the pixmap
 *	representing the tile.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyTile(Tile *tilePtr)
{
    Blt_ChainLink *linkPtr;
    TileClient *clientPtr;

    if (tilePtr->flags & NOTIFY_PENDING) {
	Tcl_CancelIdleCall(UpdateTile, tilePtr);
    }
    for (linkPtr = Blt_ChainFirstLink(tilePtr->clients); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	clientPtr = Blt_ChainGetValue(linkPtr);
	Blt_Free(clientPtr);
    }
    Blt_ChainDestroy(tilePtr->clients);

    if (tilePtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(tilePtr->tablePtr, tilePtr->hashPtr);
    }
    if (tilePtr->pixmap != None) {
	Tk_FreePixmap(tilePtr->display, tilePtr->pixmap);
    }
    Tk_FreeImage(tilePtr->tkImage);

    if (tilePtr->gc != NULL) {
	Tk_FreeGC(tilePtr->display, tilePtr->gc);
    }
    if (tilePtr->name != NULL) {
	Blt_Free(tilePtr->name);
    }
    Blt_Free(tilePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateTile --
 *
 *	Creates a tile server.  A tile server manages a single image,
 *	possibly shared by several clients.  Clients will be updated
 *	(if requested) by the server if the image changes, so they
 *	know to redraw themselves.  For X11 the image is drawn into a
 *	pixmap that is used in a new GC as its tile.  For Windows we
 *	have to do the tiling ourselves by redrawing the image across
 *	the drawing area (see Blt_TileRectangle and Blt_TilePolygon).
 *
 * Results:
 *	Returns a pointer to the new tile server.  If the image name
 *	does not represent a Tk image, NULL is returned.
 *
 *---------------------------------------------------------------------- 
 */
static Tile *
CreateTile(
    Tcl_Interp *interp,
    Tk_Window tkwin,
    char *imageName)
{
    Tile *tilePtr;
    Tk_Image tkImage;

    tilePtr = Blt_Calloc(1, sizeof(Tile));
    assert(tilePtr);
    /*
     * Get the image. Funnel all change notifications to a single routine.
     */
    tkImage = Tk_GetImage(interp, tkwin, imageName, ImageChangedProc,
	tilePtr);
    if (tkImage == NULL) {
	Blt_Free(tilePtr);
	return NULL;
    }

    /*
     * Initialize the tile server.
     */
    tilePtr->display = Tk_Display(tkwin);
    tilePtr->interp = interp;
    tilePtr->name = Blt_Strdup(imageName);
    tilePtr->clients = Blt_ChainCreate();
    tilePtr->tkImage = tkImage;
    RedrawTile(tkwin, tilePtr);
    return tilePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyClient --
 *
 *	Removes the client from the servers's list of clients and
 *	memory used by the client token is released.  When the last
 *	client is deleted, the server is also removed.
 *
 * Results:
 *	None.
 *
 *---------------------------------------------------------------------- 
 */
static void
DestroyClient(TileClient *clientPtr)
{
    Tile *tilePtr;
    tilePtr = clientPtr->tilePtr;

    /* Remove the client from the server's list */
    if (clientPtr->linkPtr != NULL) {
	Blt_ChainDeleteLink(tilePtr->clients, clientPtr->linkPtr);
    }
    if (Blt_ChainGetLength(tilePtr->clients) == 0) {
	/*
	 * If there are no more clients of the tile, then remove the
	 * pixmap, image, and the server record.
	 */
	DestroyTile(tilePtr);
    }
    Blt_Free(clientPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CreateClient --
 *
 *	Returns a token to a tile (possibly shared by many clients).
 *	A client uses the token to query or display the tile.  Clients
 *	request tiles by their image names.  Each tile is known by its
 *	display, screen depth, and image name.  The tile server tracks
 *	what clients are using the tile and notifies them (via a
 *	callback) whenever the tile changes. If no server exists
 *	already, one is created on-the-fly.
 *
 * Results:
 *	A pointer to the newly created client (i.e. tile).
 *
 *---------------------------------------------------------------------- 
 */
static TileClient *
CreateClient(
    Tcl_Interp *interp,
    Tk_Window tkwin,
    char *name)
{
    TileClient *clientPtr;
    Tile *tilePtr;
    TileInterpData *dataPtr;
    Blt_HashEntry *hPtr;
    int isNew;
    TileKey key;

    dataPtr = GetTileInterpData(interp);

    key.nameId = Tk_GetUid(name);
    key.display = Tk_Display(tkwin);
    key.depth = Tk_Depth(tkwin);
    hPtr = Blt_CreateHashEntry(&dataPtr->tileTable, (char *)&key, &isNew);
    if (isNew) {
	tilePtr = CreateTile(interp, tkwin, name);
	if (tilePtr == NULL) {
	    Blt_DeleteHashEntry(&(dataPtr->tileTable), hPtr);
	    return NULL;
	}
	tilePtr->hashPtr = hPtr;
	tilePtr->tablePtr = &(dataPtr->tileTable);
	Blt_SetHashValue(hPtr, tilePtr);
    } else {
	tilePtr = Blt_GetHashValue(hPtr);
    }
    clientPtr = Blt_Calloc(1, sizeof(TileClient));
    assert(clientPtr);

    /* Initialize client information. */
    clientPtr->magic = TILE_MAGIC;
    clientPtr->tkwin = tkwin;
    clientPtr->linkPtr = Blt_ChainAppend(tilePtr->clients, clientPtr);
    clientPtr->tilePtr = tilePtr;
    return clientPtr;
}

/*
 * -----------------------------------------------------------------------
 *
 * TileInterpDeleteProc --
 *
 *	This is called when the interpreter is deleted. All the tiles
 *	are specific to that interpreter are destroyed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys the tile table.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
TileInterpDeleteProc(
    ClientData clientData,	/* Thread-specific data. */
    Tcl_Interp *interp)
{
    TileInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Tile *tilePtr;
    
    for (hPtr = Blt_FirstHashEntry(&(dataPtr->tileTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	tilePtr = Blt_GetHashValue(hPtr);
	tilePtr->hashPtr = NULL;
	DestroyTile(tilePtr);
    }
    Blt_DeleteHashTable(&(dataPtr->tileTable));
    Tcl_DeleteAssocData(interp, TILE_THREAD_KEY);
    Blt_Free(dataPtr);
}

static TileInterpData *
GetTileInterpData(interp)
     Tcl_Interp *interp;
{
    TileInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (TileInterpData *)
	Tcl_GetAssocData(interp, TILE_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_Malloc(sizeof(TileInterpData));
	assert(dataPtr);
	dataPtr->interp = interp;
	Tcl_SetAssocData(interp, TILE_THREAD_KEY, TileInterpDeleteProc, 
		dataPtr);
	Blt_InitHashTable(&(dataPtr->tileTable), sizeof(TileKey)/sizeof(int));
    }
    return dataPtr;
}


/* Public API for tiles. */

/*
 *----------------------------------------------------------------------
 *
 * Blt_GetTile
 *
 *	Convert the named image into a tile.
 *
 * Results:
 *	If the image is valid, a new tile is returned.  If the name
 *	does not represent a proper image, an error message is left in
 *	interp->result.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
int
Blt_GetTile(
    Tcl_Interp *interp,		/* Interpreter to report results back to */
    Tk_Window tkwin,		/* Window on the same display as tile */
    char *imageName,		/* Name of image */
    Blt_Tile *tokenPtr)		/* (out) Returns the allocated tile token. */
{
    TileClient *clientPtr;

    clientPtr = CreateClient(interp, tkwin, imageName);
    if (clientPtr == NULL) {
	return TCL_ERROR;
    } 
    *tokenPtr = clientPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_FreeTile
 *
 *	Release the resources associated with the tile.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Memory and X resources are freed.  Bookkeeping information
 *	about the tile (i.e. width, height, and name) is discarded.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_FreeTile(TileClient *clientPtr) /* Tile to be deleted */
{
    if ((clientPtr == NULL) || (clientPtr->magic != TILE_MAGIC)) {
	return;			/* No tile */
    }
    DestroyClient(clientPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_NameOfTile
 *
 *	Returns the name of the image from which the tile was
 *	generated.
 *
 * Results:
 *	The name of the image is returned.  The name is not unique.
 *	Many tiles may use the same image.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
char *
Blt_NameOfTile(TileClient *clientPtr) /* Tile to query */
{
    if (clientPtr == NULL) {
	return "";
    }
    if (clientPtr->magic != TILE_MAGIC) {
	return "not a tile";
    }
    return clientPtr->tilePtr->name;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_PixmapOfTile
 *
 *	Returns the pixmap of the tile.
 *
 * Results:
 *	The X pixmap used as the tile is returned.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
Pixmap
Blt_PixmapOfTile(TileClient *clientPtr) /* Tile to query */
{
    if ((clientPtr == NULL) || (clientPtr->magic != TILE_MAGIC)) {
	return None;
    }
    return clientPtr->tilePtr->pixmap;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_SizeOfTile
 *
 *	Returns the width and height of the tile.
 *
 * Results:
 *	The width and height of the tile are returned.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_SizeOfTile(
    TileClient *clientPtr,	/* Tile to query */
    int *widthPtr, 
    int *heightPtr)		/* Returned dimensions of the tile (out) */
{
    if ((clientPtr == NULL) || (clientPtr->magic != TILE_MAGIC)) {
	*widthPtr = *heightPtr = 0;
	return;			/* No tile given. */
    }
    *widthPtr = clientPtr->tilePtr->width;
    *heightPtr = clientPtr->tilePtr->height;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_SetTileChangedProc
 *
 *	Sets the routine to called when an image changes.  
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The designated routine will be called the next time the
 *	image associated with the tile changes.
 *
 *----------------------------------------------------------------------
 */
/*LINTLIBRARY*/
void
Blt_SetTileChangedProc(
    TileClient *clientPtr,		/* Tile to query */
    Blt_TileChangedProc *notifyProc,
    ClientData clientData)
{
    if ((clientPtr != NULL) && (clientPtr->magic == TILE_MAGIC)) {
	clientPtr->notifyProc = notifyProc;
	clientPtr->clientData = clientData;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_SetTileOrigin --
 *
 *	Set the pattern origin of the tile to a common point (i.e. the
 *	origin (0,0) of the top level window) so that tiles from two
 *	different widgets will match up.  This done by setting the
 *	GCTileStipOrigin field is set to the translated origin of the
 *	toplevel window in the hierarchy.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The GCTileStipOrigin is reset in the GC.  This will cause the
 *	tile origin to change when the GC is used for drawing.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
void
Blt_SetTileOrigin(
    Tk_Window tkwin,
    TileClient *clientPtr,
    int x, int y)
{
    while (!Tk_IsTopLevel(tkwin)) {
	x += Tk_X(tkwin) + Tk_Changes(tkwin)->border_width;
	y += Tk_Y(tkwin) + Tk_Changes(tkwin)->border_width;
	tkwin = Tk_Parent(tkwin);
    }
    XSetTSOrigin(Tk_Display(tkwin), clientPtr->tilePtr->gc, -x, -y);
    clientPtr->xOrigin = -x;
    clientPtr->yOrigin = -y;
}

void
Blt_SetTSOrigin(
    Tk_Window tkwin,
    TileClient *clientPtr,
    int x, int y)
{
    XSetTSOrigin(Tk_Display(tkwin), clientPtr->tilePtr->gc, x, y);
    clientPtr->xOrigin = x;
    clientPtr->yOrigin = y;
}

#ifdef WIN32
static int tkpWinRopModes[] =
{
    R2_BLACK,			/* GXclear */
    R2_MASKPEN,			/* GXand */
    R2_MASKPENNOT,		/* GXandReverse */
    R2_COPYPEN,			/* GXcopy */
    R2_MASKNOTPEN,		/* GXandInverted */
    R2_NOT,			/* GXnoop */
    R2_XORPEN,			/* GXxor */
    R2_MERGEPEN,		/* GXor */
    R2_NOTMERGEPEN,		/* GXnor */
    R2_NOTXORPEN,		/* GXequiv */
    R2_NOT,			/* GXinvert */
    R2_MERGEPENNOT,		/* GXorReverse */
    R2_NOTCOPYPEN,		/* GXcopyInverted */
    R2_MERGENOTPEN,		/* GXorInverted */
    R2_NOTMASKPEN,		/* GXnand */
    R2_WHITE			/* GXset */
};
#define MASKPAT		0x00E20746 /* dest = (src & pat) | (!src & dst) */
#define COPYFG		0x00CA0749 /* dest = (pat & src) | (!pat & dst) */
#define COPYBG		0x00AC0744 /* dest = (!pat & src) | (pat & dst) */

static void
TileRegion(
    HDC srcDC,			/* Source device context. */
    HDC destDC,			/* Destination device context. */
    HDC maskDC,			/* If non-NULL, device context of the
				 * mask tile mask. */
    TileClient *clientPtr,
    int x, int y, 
    int width, int height)
{
    Tile *tilePtr = clientPtr->tilePtr;
    int destX, destY;
    int destWidth, destHeight;
    int srcX, srcY;
    int startX, startY;		/* Starting upper left corner of region. */
    int delta;
    int left, top, right, bottom;

    startX = x;
    if (x < clientPtr->xOrigin) {
	delta = (clientPtr->xOrigin - x) % tilePtr->width;
	if (delta > 0) {
	    startX -= (tilePtr->width - delta);
	}
    } else if (x > clientPtr->xOrigin) {
	delta = (x - clientPtr->xOrigin) % tilePtr->width;
	if (delta > 0) {
	    startX -= delta;
	}
    }
    startY = y;
    if (y < clientPtr->yOrigin) {
	delta = (clientPtr->yOrigin - y) % tilePtr->height;
	if (delta > 0) {
	    startY -= (tilePtr->height - delta);
	}
    } else if (y >= clientPtr->yOrigin) {
	delta = (y - clientPtr->yOrigin) % tilePtr->height;
	if (delta > 0) {
	    startY -= delta;
	}
    }
#ifdef notdef
    PurifyPrintf("tile is (%d,%d,%d,%d)\n", 
		 clientPtr->xOrigin, clientPtr->yOrigin, 
		 tilePtr->width, tilePtr->height);
    PurifyPrintf("region is (%d,%d,%d,%d)\n", x, y, width, height);
    PurifyPrintf("starting at %d,%d\n", startX, startY);
#endif
    left = x;
    right = x + width;
    top = y;
    bottom = y + height;
    for (y = startY; y < bottom; y += tilePtr->height) {
	srcY = 0;
	destY = y;
	destHeight = tilePtr->height;
	if (y < top) {
	    srcY = (top - y);
	    destHeight = tilePtr->height - srcY;
	    destY = top;
	} 
	if ((destY + destHeight) > bottom) {
	    destHeight = (bottom - destY);
	}
	for (x = startX; x < right; x += tilePtr->width) {
	    srcX = 0;
	    destX = x;
	    destWidth = tilePtr->width;
	    if (x < left) {
		srcX = (left - x);
		destWidth = tilePtr->width - srcX;
		destX = left;
	    } 
	    if ((destX + destWidth) > right) {
		destWidth = (right - destX);
	    }
#ifdef notdef
	    PurifyPrintf("drawing pattern (%d,%d,%d,%d) at %d,%d\n",
		 srcX , srcY, destWidth, destHeight, destX, destY);
#endif
	    if (tilePtr->mask != None) { /* With transparency. */
#ifdef notdef
		HDC maskDC;
		TkWinDCState maskState;

		maskDC = TkWinGetDrawableDC(tilePtr->display, 
			tilePtr->mask, &maskState);
		SetBkColor(destDC, RGB(255, 255, 255));      
		SetTextColor(destDC, RGB(0, 0, 0));
#endif
		BitBlt(destDC, destX, destY, destWidth, destHeight, maskDC, 
		       0, 0, SRCAND);
		BitBlt(destDC, destX, destY, destWidth, destHeight, srcDC, 
		       srcX, srcY, SRCPAINT);
#ifdef notdef
		TkWinReleaseDrawableDC(tilePtr->mask, maskDC, &maskState);
#endif
	    } else {		/* Opaque tile. */
	        BitBlt(destDC, destX, destY, destWidth, destHeight, 
		       srcDC, srcX, srcY, SRCCOPY);
	    }
	}
    }
}

void
Blt_TilePolygon(
    Tk_Window tkwin,
    Drawable drawable,
    TileClient *clientPtr,
    XPoint pointArr[],
    int nPoints)
{
    HBITMAP oldBitmap;
    HDC hDC, memDC;
    HRGN hRgn;
    POINT *p, *winPts;
    Region2D bbox;
    Tile *tilePtr;
    TkWinDCState state;
    TkWinDrawable *twdPtr;
    XPoint *endPtr, *pointPtr;
    int fillMode;    
    int width, height;
    
    if (drawable == None) {
	return;
    }
    tilePtr = clientPtr->tilePtr;

    /* Determine the bounding box of the polygon. */
    bbox.left = bbox.right = pointArr[0].x;
    bbox.top = bbox.bottom = pointArr[0].y;
    
    endPtr = pointArr + nPoints;
    for (pointPtr = pointArr; pointPtr < endPtr; pointPtr++) {
	if (pointPtr->x < bbox.left) {
	    bbox.left = pointPtr->x;
	} 
	if (pointPtr->x > bbox.right) {
	    bbox.right = pointPtr->x;
	}
	if (pointPtr->y < bbox.top) {
	    bbox.top = pointPtr->y;
	} 
	if (pointPtr->y > bbox.bottom) {
	    bbox.bottom = pointPtr->y;
	}
    }
    width = bbox.right - bbox.left + 1;
    height = bbox.bottom - bbox.top + 1;


    /* Allocate and fill an array of POINTS to create the polygon path. */
    p = winPts = Blt_Malloc(sizeof(POINT) * nPoints);
    for (pointPtr = pointArr; pointPtr < endPtr; pointPtr++) {
	p->x = pointPtr->x - bbox.left;
	p->y = pointPtr->y - bbox.top;
	p++;
    }

    hDC = TkWinGetDrawableDC(Tk_Display(tkwin), drawable, &state);
    SetROP2(hDC, tkpWinRopModes[tilePtr->gc->function]);
    fillMode = (tilePtr->gc->fill_rule == EvenOddRule) ? ALTERNATE : WINDING;
    /* Use the polygon as a clip path. */
    LPtoDP(hDC, winPts, nPoints);
    hRgn = CreatePolygonRgn(winPts, nPoints, fillMode);
    SelectClipRgn(hDC, hRgn);
    OffsetClipRgn(hDC, bbox.left, bbox.top);
    Blt_Free(winPts);

    twdPtr = (TkWinDrawable *)tilePtr->pixmap;
    memDC = CreateCompatibleDC(hDC);
    oldBitmap = SelectBitmap(memDC, twdPtr->bitmap.handle);
    
    /* Tile the bounding box. */
    if (tilePtr->mask != None) {
	TkWinDCState maskState;
	HDC maskDC;

	maskDC = TkWinGetDrawableDC(tilePtr->display, tilePtr->mask, 
	    &maskState);
	SetBkColor(hDC, RGB(255, 255, 255));      
	SetTextColor(hDC, RGB(0, 0, 0));
	TileRegion(memDC, hDC, maskDC, clientPtr, bbox.left, bbox.top, width, 
		   height);
	TkWinReleaseDrawableDC(tilePtr->mask, maskDC, &maskState);
    } else {
	TileRegion(memDC, hDC, NULL, clientPtr, bbox.left, bbox.top, width, 
	   height);
    }
    SelectBitmap(memDC, oldBitmap);
    DeleteDC(memDC);
    SelectClipRgn(hDC, NULL);
    DeleteRgn(hRgn);
    TkWinReleaseDrawableDC(drawable, hDC, &state);
}

void
Blt_TileRectangle(
    Tk_Window tkwin,
    Drawable drawable,
    TileClient *clientPtr,
    int x, int y,
    unsigned int width, 
    unsigned int height)
{
    HBITMAP oldBitmap;
    HDC hDC, memDC;
    Tile *tilePtr;
    TkWinDCState state;
    TkWinDrawable *twdPtr;

    if (drawable == None) {
	return;
    }
    tilePtr = clientPtr->tilePtr;
    hDC = TkWinGetDrawableDC(Tk_Display(tkwin), drawable, &state);
    SetROP2(hDC, tkpWinRopModes[tilePtr->gc->function]);

    twdPtr = (TkWinDrawable *)tilePtr->pixmap;
    memDC = CreateCompatibleDC(hDC);
    oldBitmap = SelectBitmap(memDC, twdPtr->bitmap.handle);

    /* Tile the bounding box. */
    if (tilePtr->mask != None) {
	TkWinDCState maskState;
	HDC maskDC;

	maskDC = TkWinGetDrawableDC(tilePtr->display, tilePtr->mask, 
	    &maskState);
	SetBkColor(hDC, RGB(255, 255, 255));      
	SetTextColor(hDC, RGB(0, 0, 0));
	TileRegion(memDC, hDC, maskDC, clientPtr, x, y, width, height);
	TkWinReleaseDrawableDC(tilePtr->mask, maskDC, &maskState);
    } else {
	TileRegion(memDC, hDC, NULL, clientPtr, x, y, width, height);
    }
    SelectBitmap(memDC, oldBitmap);
    DeleteDC(memDC);
    TkWinReleaseDrawableDC(drawable, hDC, &state);
}

void
Blt_TileRectangles(
    Tk_Window tkwin,
    Drawable drawable,
    TileClient *clientPtr,
    XRectangle rectArr[],
    int nRectangles)
{
    HBITMAP oldBitmap;
    HDC hDC, memDC;
    Tile *tilePtr;
    TkWinDCState state;
    TkWinDrawable *twdPtr;
    XRectangle *rectPtr, *endPtr;

    if (drawable == None) {
	return;
    }
    tilePtr = clientPtr->tilePtr;
    hDC = TkWinGetDrawableDC(Tk_Display(tkwin), drawable, &state);
    SetROP2(hDC, tkpWinRopModes[tilePtr->gc->function]);

    twdPtr = (TkWinDrawable *)tilePtr->pixmap;
    memDC = CreateCompatibleDC(hDC);
    oldBitmap = SelectBitmap(memDC, twdPtr->bitmap.handle);

    endPtr = rectArr + nRectangles;
    /* Tile the bounding box. */
    if (tilePtr->mask != None) {
	TkWinDCState maskState;
	HDC maskDC;

	maskDC = TkWinGetDrawableDC(tilePtr->display, tilePtr->mask, 
	    &maskState);
	SetBkColor(hDC, RGB(255, 255, 255));      
	SetTextColor(hDC, RGB(0, 0, 0));
	for (rectPtr = rectArr; rectPtr < endPtr; rectPtr++) {
	    TileRegion(memDC, hDC, maskDC, clientPtr, (int)rectPtr->x, 
		(int)rectPtr->y, (int)rectPtr->width, (int)rectPtr->height);
	}
	TkWinReleaseDrawableDC(tilePtr->mask, maskDC, &maskState);
    } else {
	for (rectPtr = rectArr; rectPtr < endPtr; rectPtr++) {
	    TileRegion(memDC, hDC, NULL, clientPtr, (int)rectPtr->x, 
		(int)rectPtr->y, (int)rectPtr->width, (int)rectPtr->height);
	}
    }
    SelectBitmap(memDC, oldBitmap);
    DeleteDC(memDC);
    TkWinReleaseDrawableDC(drawable, hDC, &state);
}

#else 

/*
 *----------------------------------------------------------------------
 *
 * RectangleMask --
 *
 *	Creates a rectangular mask also stippled by the mask of the 
 *	tile.  This is used to draw the tiled polygon images with 
 *	transparent areas.
 *
 * Results:
 *	A bitmap mask is returned.
 *
 *----------------------------------------------------------------------
 */
static Pixmap
RectangleMask(display, drawable, x, y, width, height, mask, xOrigin, yOrigin)
    Display *display;
    Drawable drawable;
    int x, y;
    unsigned int width, height;
    Pixmap mask;
    int xOrigin, yOrigin;
{
    GC gc;
    Pixmap bitmap;
    XGCValues gcValues;
    unsigned long gcMask;

    bitmap = Tk_GetPixmap(display, drawable, width, height, 1);
    gcMask = (GCForeground | GCBackground | GCFillStyle | 
	      GCTileStipXOrigin | GCTileStipYOrigin | GCStipple);
    gcValues.foreground = 0x1;
    gcValues.background = 0x0;
    gcValues.fill_style = FillOpaqueStippled;
    gcValues.ts_x_origin = xOrigin - x;
    gcValues.ts_y_origin = yOrigin - y;
    gcValues.stipple = mask;
    gc = XCreateGC(display, bitmap, gcMask, &gcValues);
    XFillRectangle(display, bitmap, gc, 0, 0, width, height);
    Blt_FreePrivateGC(display, gc);
    return bitmap;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TileRectangle --
 *
 *	Draws a rectangle filled by a tiled image.  This differs from 
 *	the normal XFillRectangle call in that we also try to handle 
 *	a transparency mask. 
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Draws the rectangle.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TileRectangle(
    Tk_Window tkwin,
    Drawable drawable,
    TileClient *clientPtr,
    int x, int y,
    unsigned int width, 
    unsigned int height)
{
    Tile *tilePtr;
    Display *display;

    display = Tk_Display(tkwin);
    tilePtr = clientPtr->tilePtr;
    if (clientPtr->tilePtr->mask != None) {
	Pixmap mask;

	mask = RectangleMask(display, drawable, x, y, width, height,
		tilePtr->mask, clientPtr->xOrigin, clientPtr->yOrigin);
	XSetClipMask(display, tilePtr->gc, mask);
	XSetClipOrigin(display, tilePtr->gc, x, y);
	XFillRectangle(display, drawable, tilePtr->gc, x, y, width, height);
	XSetClipMask(display, tilePtr->gc, None);
	XSetClipOrigin(display, tilePtr->gc, 0, 0);
	Tk_FreePixmap(display, mask);
    } else {
	XFillRectangle(display, drawable, tilePtr->gc, x, y, width, height);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TileRectangles --
 *
 *	Draws rectangles filled by a tiled image.  This differs from 
 *	the normal XFillRectangles call in that we also try to handle 
 *	a transparency mask. 
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Draws the given rectangles.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TileRectangles(
    Tk_Window tkwin,
    Drawable drawable,
    TileClient *clientPtr,
    XRectangle rectArr[],
    int nRectangles)
{
    Tile *tilePtr;

    tilePtr = clientPtr->tilePtr;
    if (tilePtr->mask != None) {
	XRectangle *rectPtr, *endPtr;

	endPtr = rectArr + nRectangles;
	for (rectPtr = rectArr; rectPtr < endPtr; rectPtr++) {
	    Blt_TileRectangle(tkwin, drawable, clientPtr, rectPtr->x, 
		rectPtr->y, rectPtr->width, rectPtr->height);
	}
    } else {
	XFillRectangles(Tk_Display(tkwin), drawable, tilePtr->gc, rectArr, 
		nRectangles);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PolygonMask --
 *
 *	Creates a polygon shaped mask also stippled by the mask
 *	of the tile.  This is used to draw the tiled polygon images
 *	with transparent areas.
 *
 * Results:
 *	A bitmap mask is returned.
 *
 *----------------------------------------------------------------------
 */
static Pixmap
PolygonMask(display, pointArr, nPoints, regionPtr, mask, xOrigin, yOrigin)
    Display *display;
    XPoint *pointArr;
    int nPoints;
    Region2D *regionPtr;
    Pixmap mask;
    int xOrigin, yOrigin;
{
    unsigned int width, height;
    Pixmap bitmap;
    GC gc;
    XPoint *destArr;
    register XPoint *srcPtr, *destPtr, *endPtr;

    width = regionPtr->right - regionPtr->left + 1;
    height = regionPtr->bottom - regionPtr->top + 1;
    bitmap = 
	Tk_GetPixmap(display, DefaultRootWindow(display), width, height, 1);

    destArr = Blt_Malloc(sizeof(XPoint) * nPoints);
    endPtr = destArr + nPoints;
    srcPtr = pointArr;
    for (destPtr = destArr; destPtr < endPtr; destPtr++) {
	destPtr->x = srcPtr->x - regionPtr->left;
	destPtr->y = srcPtr->y - regionPtr->top;
	srcPtr++;
    }
    gc = XCreateGC(display, bitmap, 0, NULL);
    XFillRectangle(display, bitmap, gc, 0, 0, width, height);
    XSetForeground(display, gc, 0x01);
    XSetFillStyle(display, gc, FillStippled);
    XSetTSOrigin(display, gc, xOrigin - regionPtr->left, 
		 yOrigin - regionPtr->top);
    XSetStipple(display, gc, mask);
    XFillPolygon(display, bitmap, gc, destArr, nPoints, Complex, 
		 CoordModeOrigin);
    XFreeGC(display, gc);
    Blt_Free(destArr);
    return bitmap;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_TilePolygon --
 *
 *	Draws a polygon filled by a tiled image.  This differs from 
 *	the normal XFillPolygon call in that we also try to handle 
 *	a transparency mask. 
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Draws the polygon.
 *
 *----------------------------------------------------------------------
 */
void
Blt_TilePolygon(
    Tk_Window tkwin,
    Drawable drawable,
    TileClient *clientPtr,
    XPoint pointArr[],
    int nPoints)
{
    Tile *tilePtr;
    Display *display;
    
    display = Tk_Display(tkwin);
    tilePtr = clientPtr->tilePtr;
    if (tilePtr->mask != None) {
	XPoint *pointPtr, *endPtr;
	Region2D region;
	Pixmap mask;

	/* Determine the bounding box of the polygon. */
	pointPtr = pointArr;
	region.left = region.right = pointPtr->x;
	region.top = region.bottom = pointPtr->y;
	
	endPtr = pointArr + nPoints;
	for (pointPtr = pointArr; pointPtr < endPtr; pointPtr++) {
	    if (region.left > pointPtr->x) {
		region.left = pointPtr->x;
	    } else if (region.right < pointPtr->x) {
		region.right = pointPtr->x;
	    }
	    if (region.top > pointPtr->y) {
		region.top = pointPtr->y;
	    } else if (region.bottom < pointPtr->y) {
		region.bottom = pointPtr->y;
	    }
	}
	mask = PolygonMask(display, pointArr, nPoints, &region, 
		   tilePtr->mask, clientPtr->xOrigin, clientPtr->yOrigin);
	XSetClipMask(display, tilePtr->gc, mask);
	XSetClipOrigin(display, tilePtr->gc, region.left, region.top);
	XFillPolygon(display, drawable, tilePtr->gc, pointArr, 
		     nPoints, Complex, CoordModeOrigin);
	XSetClipMask(display, tilePtr->gc, None);
	XSetClipOrigin(display, tilePtr->gc, 0, 0);
	Tk_FreePixmap(display, mask);
    } else {
	XFillPolygon(display, drawable, tilePtr->gc, pointArr, 
		     nPoints, Complex, CoordModeOrigin);
    }
}
#endif
