/*
 * bltTile.h --
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

#ifndef BLT_TILE_H
#define BLT_TILE_H

#define TILE_THREAD_KEY	"BLT Tile Data"
#define TILE_MAGIC ((unsigned int) 0x46170277)

typedef struct Blt_TileClientStruct *Blt_Tile; /* Opaque type for tiles */

typedef void (Blt_TileChangedProc) _ANSI_ARGS_((ClientData clientData,
	Blt_Tile tile));

extern int Blt_GetTile _ANSI_ARGS_((Tcl_Interp *interp, Tk_Window tkwin,
	char *imageName, Blt_Tile *tilePtr));

extern void Blt_FreeTile _ANSI_ARGS_((Blt_Tile tile));

extern char *Blt_NameOfTile _ANSI_ARGS_((Blt_Tile tile));

extern void Blt_SetTileChangedProc _ANSI_ARGS_((Blt_Tile tile,
	Blt_TileChangedProc *changeProc, ClientData clientData));

extern void Blt_TileRectangle _ANSI_ARGS_((Tk_Window tkwin, Drawable drawable,
	Blt_Tile tile, int x, int y, unsigned int width, unsigned int height));
extern void Blt_TileRectangles _ANSI_ARGS_((Tk_Window tkwin, Drawable drawable,
	Blt_Tile tile, XRectangle *rectArr, int nRects));
extern void Blt_TilePolygon _ANSI_ARGS_((Tk_Window tkwin, Drawable drawable, 
	Blt_Tile tile, XPoint *pointArr, int nPoints));
extern Pixmap Blt_PixmapOfTile _ANSI_ARGS_((Blt_Tile tile));

extern void Blt_SizeOfTile _ANSI_ARGS_((Blt_Tile tile, int *widthPtr,
	int *heightPtr));

extern void Blt_SetTileOrigin _ANSI_ARGS_((Tk_Window tkwin, Blt_Tile tile, 
	int x, int y));

extern void Blt_SetTSOrigin _ANSI_ARGS_((Tk_Window tkwin, Blt_Tile tile, 
	int x, int y));

#endif /* BLT_TILE_H */
