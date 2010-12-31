
/*----------------------------------------------------------------------------
 * Copyright (c) 2005 Eolas Technologies Inc.
 * All rights reserved.
 *
 * This Open Source project was made possible through the financial support
 * of Eolas Technologies Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/
static const char rcsid[] = "$Id: htmldraw.c,v 1.208 2008/02/14 08:43:49 danielk1977 Exp $";

#include "html.h"
#include <assert.h>
#include <X11/Xutil.h>


/*-------------------------------------------------------------------------
 * OVERVIEW:
 *
 *     This file contains code and data structures for the runtime 
 *     document.
 *
 *     A runtime document consists of a set of primitives and a background
 *     color. Rendering the page is achieved by iterating through all
 *     primitives (in an order defined by the layout engine) and painting or
 *     positioning them relative to the widget window. A primitive may be one
 *     of the following:
 *
 *         * Box        - CSS style border and background.
 *         * Line       - Horizontal line used for 'text-decoration'.
 *         * Text       - Single line of text
 *         * Image      - An image. Used for replacement + list-marker images.
 *         * Window     - A Tk widget window (set via [node replace])
 *
 *     All web documents are reduced by the layout engine to zero or more 
 *     of these primitives. 
 *
 * USE CASES
 *
 *     The data structures in this module are designed to account for the
 *     the following four use cases:
 *
 *         1. Paint region. 
 *         2. Selection modification
 *         3. Find window coordinates (bbox) given node.
 *         4. Find node given window coordinates (a single point).
 *
 *     There are a few different variations on case (3).
 *
 */

/*
 * This module runs much faster if it can use the Xlib API to set
 * clipping regions. However this doesn't work on windows, or with
 * Xft fonts on Linux. Doubtful that it works on mac either.
 *
 * If this symbol is set to 0, then we use pixmaps for clipping.
 * This can be *really* expensive, so this module tries hard to 
 * detect cases where the CSS design has specified a clipping 
 * window ("overflow:hidden") but no visible items are clipped by it.
 */
#define USE_XLIB_CLIPPING 0
/* #define USE_XLIB_CLIPPING 1 */


/*
 * EXPORTED FUNCTIONS:
 *
 * Functions for keeping the display up to date:
 *
 *     HtmlWidgetRepair
 *     HtmlWidgetSetViewport
 *
 *         Repair() is used when a rectangular region of the viewport must
 *         be repainted (i.e.  to repair window damage). SetViewport() is
 *         used to scroll the window. It recalculates the positions of
 *         mapped windows.
 *
 * Snapshot feature (for figuring out the difference between two layouts):
 *
 *     HtmlDrawSnapshot
 *     HtmlDrawSnapshotDamage
 *     HtmlDrawSnapshotFree
 *
 *         The DrawSnapshot() function is used to capture a snapshot of
 *         the current layout. It can then be passed to DrawSnapshotDamage()
 *         to determine the region of the window that must be repainted
 *         due to differences between the current and snapshot layouts.
 *         DrawSnapshotFree() cleans up the return value of DrawSnapshot().
 *
 * Functions to query a canvas layout:
 *
 *     HtmlWidgetDamageText
 *     HtmlWidgetBboxText
 *     HtmlWidgetNodeBox
 *     HtmlWidgetNodeTop
 *
 *         The NodeBox() function returns the canvas coordinates of a
 *         bounding-box for a supplied node. The NodeTop() function returns a
 *         single coordinate - the offset from the top of the canvas for a
 *         nominated node. 
 *       
 *         DamageText() is used to query for the bounding box of a region of
 *         text. However instead of returning coordinates, it invokes
 *         HtmlCallbackDamage() on the identified region.
 *
 * Tcl command functions:
 *
 *     HtmlLayoutNode
 *     HtmlLayoutPrimitives
 *     HtmlLayoutImage
 *
 *         Implementations of the [widget node] [widget image] and [widget
 *         primitives] commands. Note that the latter two are intended for
 *         debugging only and so are not really part of the public interface.
 *
 * Canvas management:
 *     HtmlDrawCanvas
 *     HtmlDrawCleanup
 *     HtmlDrawCopyCanvas
 *     HtmlDrawIsEmpty
 *
 * Functions for drawing primitives to a canvas:
 *     HtmlDrawOrigin
 *     HtmlDrawImage
 *     HtmlDrawWindow
 *     HtmlDrawText
 *     HtmlDrawBox
 *     HtmlDrawLine
 *     HtmlDrawOverflow
 *
 * Adding and querying for markers:
 *     HtmlDrawAddMarker
 *     HtmlDrawGetMarker
 *
 * HtmlDrawCanvasItemRelease
 * HtmlDrawCanvasItemReference
 *
 * 
 */

#define CANVAS_TEXT     1
#define CANVAS_LINE     2
#define CANVAS_BOX      3
#define CANVAS_IMAGE    4
#define CANVAS_WINDOW   5

#define CANVAS_ORIGIN   6
#define CANVAS_MARKER   7
#define CANVAS_OVERFLOW 8

typedef struct CanvasText CanvasText;
typedef struct CanvasImage CanvasImage;
typedef struct CanvasBox CanvasBox;
typedef struct CanvasWindow CanvasWindow;
typedef struct CanvasOrigin CanvasOrigin;
typedef struct CanvasLine   CanvasLine;
typedef struct CanvasMarker CanvasMarker;
typedef struct CanvasOverflow CanvasOverflow;

typedef struct CanvasItemSorter CanvasItemSorter;
typedef struct CanvasItemSorterLevel CanvasItemSorterLevel;
typedef struct CanvasItemSorterSlot CanvasItemSorterSlot;
typedef struct Overflow Overflow;

/* A single line of text. The relative coordinates (x, y) are as required
 * by Tk_DrawChars() - the far left-edge of the text baseline. The color
 * and font of the text are determined by the properties of CanvasText.pNode.
 */
struct CanvasText {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */
    HtmlNode *pNode;         /* Text node */

    int w;                   /* Width of the text */
    HtmlFont *fFont;         /* Font used by this text item */

    /* If pNode is a non-generated text-node (not the product of a :before
     * or :after rule), then iIndex is the byte offset of CanvasText.zText
     * in HtmlTextNode.zText.
     */
    int iIndex;              /* Index in pNode text of this item (or -1) */

    const char *zText;
    int nText;
};

/* A square box, with borders, background color and image as determined
 * by the properties of pNode. Top-left hand corner is at (x, y). The
 * width and height of the box, as measured from the outer edge of the
 * borders, are w and h pixels, respectively. 
 */
struct CanvasBox {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */
    HtmlNode *pNode;         /* Use computed properties from this node */
    int w;                   /* Width of box area */
    int h;                   /* Height of box area */
    int flags;               /* Combination of CANVAS_BOX flags */
    HtmlComputedValues *pComputed;
};

/* An image. Nothing to see here. */
struct CanvasImage {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate to render at */
    HtmlNode *pNode;         /* Associate document node */
    int w;                   /* Width of image region */
    int h;                   /* Height of image region */
    HtmlImage2 *pImage;      /* Image pointer */
};

/* This primitive is used to implement the 'text-decoration' property.
 * It draws zero or more horizontal lines of width CanvasLine.w starting 
 * at relative x-coordinate CanvasLine.x.
 *
 * The lines draw depend on the 'text-decoration' property of 
 * CanvasLine.pNode. If the property is set to 'overline', then the
 * relative y-coordinate of the line is CanvasLine.y_overline. Similarly
 * if the text-decoration property of the node is 'line-through' or 
 * 'underline', then CanvasLine.y_linethrough or CanvasLine.y_underline
 * is used as the relative y-coordinate of the rendered line.
 *
 * The color of the line is determined by the 'color' property of 
 * CanvasLine.pNode.
 *
 * Todo: Above, it says "zero or more" horizontal lines. At the moment, the
 * truth is that zero or one line is drawn. This is a limitation of
 * the HtmlComputedValues structure. Once that code is fixed, this primitive
 * may draw (for example) both an overline and a linethrough decoration.
 */
struct CanvasLine {
    int x;                   /* Relative x coordinate to render at */
    int y;                   /* Relative y coordinate for overline */
    HtmlNode *pNode;         /* Node pointer */
    int w;                   /* Width of line */
    int y_underline;         /* y coordinate for underline relative to "y" */
    int y_linethrough;       /* y coordinate for linethrough relative to "y" */
};

struct CanvasWindow {
    int x;                   /* Relative x coordinate */
    int y;                   /* Relative y coordinate */
    HtmlElementNode *pElem;  /* Node replaced by this window */
    int iWidth;              /* CSS determined width of widget */
    int iHeight;             /* CSS determined height of widget */
};

/*
 * CanvasOrigin primitives are used for two purposes:
 *
 *     1. To facilitate layout caching, and
 *     2. To speed up searches of the primitives list.
 *
 * CanvasOrigin primitives are added to the display list in pairs. One
 * primitive is added to the start of the display list, the other to the end
 * (ususually, primitives are only added to the end of the list).
 */
struct CanvasOrigin {
    int x;
    int y;
    int horizontal;
    int vertical;
    int nRef;
    HtmlCanvasItem *pSkip;
};

/*
 * A CanvasOverflow primitive is used to deal with blocks that have the
 * 'overflow' property set to something other than "visible".
 */
struct CanvasOverflow {
    int x;                    /* x-coord of top-left of region */
    int y;                    /* y-coord of top-left of region */
    HtmlNode *pNode;          /* Node associated with the 'overflow' property */
    int w;                    /* Width of region */
    int h;                    /* Height of region */
    HtmlCanvasItem *pEnd;     /* Region ends *after* this item */
};

/*
 * Markers are used for two unrelated purposes:
 *
 *     * They are inserted into the display list to record the static position
 *       of fixed or absolutely positioned elements. This sort of marker is
 *       always removed from the display list during the layout and has a
 *       CanvasMarker.flags value of 0. 
 *
 *     * To mark the baseline of lineboxes.
 *
 *     * To show where the "position:fixed" section of the display list begins.
 *       The x and y variables are ignored and the flags variable is set to
 *       MARKER_FIXED.
 */
#define MARKER_FIXED 0x00000001
#define MARKER_LINEBOX 0x00000002
struct CanvasMarker {
    int x;
    int y;
    int flags;
};

struct HtmlCanvasItem {
    int type;
    int iSnapshot;            /* id of last snapshot this was added to */
    int nRef;                 /* Number of pointers to this item */
    union {
        struct GenericItem {
            int x;
            int y; 
            HtmlNode *pNode;
        } generic;
        CanvasText   t;
        CanvasWindow w;
        CanvasOrigin o;
        CanvasImage  i2;
        CanvasBox    box;
        CanvasLine   line;
        CanvasMarker marker;
        CanvasOverflow overflow;
    } x;
    HtmlCanvasItem *pNext;
};

struct Overflow {
    CanvasOverflow *pItem;
    int x;                   /* Top left of region relative to origin */
    int y;                   /* Top left of region relative to origin */
    int w;                   /* Width of region */
    int h;                   /* Height of region */

    int xscroll;
    int yscroll;

    /* Used by pixmapQueryCb() */
    Overflow *pNext;
    Pixmap pixmap;
    int pmx;                 /* Top left of pixmap relative to origin */
    int pmy;
    int pmw;
    int pmh;
};

static int pixmapQueryCb(HtmlCanvasItem *, int, int, Overflow *, ClientData);
static int sorterCb(HtmlCanvasItem *, int, int, Overflow *, ClientData);
static int layoutNodeIndexCb(HtmlCanvasItem *, int, int, Overflow *, ClientData);
static int paintNodesSearchCb(HtmlCanvasItem *, int, int, Overflow *, ClientData);
static int scrollToNodeCb(HtmlCanvasItem *, int, int, Overflow *, ClientData);
static int layoutBboxCb(HtmlCanvasItem *, int, int, Overflow *, ClientData);
static int layoutNodeCb(HtmlCanvasItem *, int, int, Overflow *, ClientData);

/*
 * This is like a big expensive assert() statement that checks the
 * internal state of the HtmlCanvas structure passed as an argument
 * is consistent. It's turned off for debugging builds because it 
 * runs a lot and is very expensive.
 */
#if 0 && !defined(NDEBUG)
static void
CHECK_CANVAS(pCanvas) 
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *p; 
    HtmlCanvasItem *pPrev = 0; 

    int bSeenFixed = 0;
    int nOriginStart = 0;
    int nOriginFin = 0;
    int ox = 0;
    int oy = 0;

    for (p = pCanvas->pFirst; p; p = ((p == pCanvas->pLast)?0:p->pNext)) {
        assert(!pPrev       || pPrev->pNext == p);
        assert(pPrev        || pCanvas->pFirst == p);
        assert(p->pNext || p == pCanvas->pLast);
        pPrev = p;
        switch (p->type) {
            case CANVAS_MARKER:
                if (p->x.marker.flags == MARKER_FIXED){
                    assert(bSeenFixed == 0);
                    bSeenFixed = 1;
                }
                break;
            case CANVAS_ORIGIN:
                assert(
                    (p->x.o.nRef >= 1 && p->x.o.pSkip) ||
                    p->x.o.pSkip == 0
                );
                assert(p->x.o.nRef <= 2);
                assert(p->nRef == 1);
                ox += p->x.o.x;
                oy += p->x.o.y;
                if (p->x.o.pSkip) {
                    nOriginStart++; 
                } else {
                    nOriginFin++;
                }
                break;
        }
    }

    assert(nOriginStart == nOriginFin);
    assert(ox == 0);
    assert(oy == 0);
}
#else
  #define CHECK_CANVAS(x)
#endif

/*
 * Every item in the canvas has an associated z-coord (not to be confused 
 * with the CSS property 'z-index'). A z-coord is a positive integer 
 * close to zero. A larger z-coord indicates the item is closer to 
 * the viewer. See htmlstyle.c for how this is calculated.
 */
struct CanvasItemSorter {
    int iSnapshot;                      /* Non-zero for a snapshot */
    int nLevel;                         /* Number of allocated levels */
    CanvasItemSorterLevel *aLevel;      /* Array of levels */  

    struct OverflowAndPixmap {
        Overflow overflow;
        Pixmap pixmap;
    } *aOverflowAndPixmap;
    int nOverflowAndPixmap;             /* Allocated size of aOver... */
};
struct CanvasItemSorterLevel {
    int iSlot;                       /* Index of next free entry in aSlot */
    int nSlot;                       /* Allocated size of aSlot */
    CanvasItemSorterSlot *aSlot;     /* Array of slots to store items */
};
struct CanvasItemSorterSlot {
    int x;                           /* item x-coord is relative to this */
    int y;                           /* item y-coord is relative to this */
    HtmlCanvasItem *pItem;           /* The item itself */

    /* Clipping region stuff */
    Overflow *pOverflow;
};

static void
sorterInsert(pSorter, pItem, x, y, pOverflow)
    CanvasItemSorter *pSorter;
    HtmlCanvasItem *pItem;
    int x;
    int y;
    Overflow *pOverflow;
{
    int z = 0;
    HtmlNode *pNode = 0;
    HtmlElementNode *pElem = 0;

    CanvasItemSorterLevel *pLevel;
    CanvasItemSorterSlot *pSlot;
    switch( pItem->type) {
        case CANVAS_TEXT:
            pNode = pItem->x.t.pNode;
            break;
        case CANVAS_IMAGE:
            pNode = pItem->x.i2.pNode;
            break;
        case CANVAS_BOX:
            pNode = pItem->x.box.pNode;
            break;
        case CANVAS_LINE:
            pNode = pItem->x.line.pNode;
            break;

        case CANVAS_WINDOW:
        case CANVAS_OVERFLOW:
            break;
        default:
            assert(!"bad type value");
    }

    if (pNode) {
        pElem = HtmlNodeAsElement(pNode);
        if (!pElem) {
            pElem = HtmlNodeAsElement(HtmlNodeParent(pNode));
        }
        assert(pElem);
    }

    if (pElem) {
        assert(pElem->pStack);
        assert(pElem->pPropertyValues);
        if( 
            pItem->type==CANVAS_TEXT || 
            pElem->pPropertyValues->eDisplay == CSS_CONST_INLINE
        ) {
            z = pElem->pStack->iInlineZ;
        } else if (pElem->pStack->pElem == pElem) {
            z = pElem->pStack->iStackingZ;
        } else {
            z = pElem->pStack->iBlockZ;
        }
    }

    assert(z >= 0 && z <= 1000000);
    while (z >= pSorter->nLevel) {
        int n = pSorter->nLevel + 128;
        pSorter->aLevel = (CanvasItemSorterLevel *)HtmlRealloc(0, 
            pSorter->aLevel, n * sizeof(CanvasItemSorterLevel)
        );
        memset(&pSorter->aLevel[pSorter->nLevel], 0, 
            sizeof(CanvasItemSorterLevel) * 128
        );
        pSorter->nLevel = n;
    }
    pLevel = &pSorter->aLevel[z];

    assert(pLevel->nSlot >= pLevel->iSlot);
    while (pLevel->nSlot == pLevel->iSlot) {
        int n = pLevel->nSlot + 128;
        pLevel->aSlot = (CanvasItemSorterSlot *)HtmlRealloc(0,
            pLevel->aSlot, n * sizeof(CanvasItemSorterSlot)
        );
        memset(&pLevel->aSlot[pLevel->nSlot], 0, 
            sizeof(CanvasItemSorterSlot) * 128
        );
        pLevel->nSlot = n;
    }
    pSlot = &pLevel->aSlot[pLevel->iSlot];
    pLevel->iSlot++;
    
    pSlot->x = x;
    pSlot->y = y;
    pSlot->pItem = pItem;
    pSlot->pOverflow = pOverflow;
}
static void
sorterIterate(pSorter, xFunc, clientData)
    CanvasItemSorter *pSorter;
    int (*xFunc)(HtmlCanvasItem *, int, int, Overflow *, ClientData);
    ClientData clientData;
{
    int ii;
    for (ii = 0; ii < pSorter->nLevel; ii++) {
        CanvasItemSorterLevel *pLevel = &pSorter->aLevel[ii];
        int jj;
        for (jj = 0; jj < pLevel->iSlot; jj++) {
            CanvasItemSorterSlot *p = &pLevel->aSlot[jj];
            xFunc(p->pItem, p->x, p->y, p->pOverflow, clientData);
        }
    }
}
static void
sorterReset(pSorter)
    CanvasItemSorter *pSorter;
{
    int ii;
    for (ii = 0; ii < pSorter->nLevel; ii++) {
        HtmlFree(pSorter->aLevel[ii].aSlot);
    }
    HtmlFree(pSorter->aLevel);
}



static HtmlCanvasItem *
allocateCanvasItem()
{
    return HtmlNew(HtmlCanvasItem);
}
static void
freeCanvasItem(pTree, p)
    HtmlTree *pTree;
    HtmlCanvasItem *p;
{
    p->nRef--;
    assert(p->nRef >= 0);
    if (p->nRef == 0) {
        switch (p->type) {
            case CANVAS_TEXT:
                HtmlFontRelease(pTree, p->x.t.fFont);
                p->x.t.fFont = 0;
                break;
            case CANVAS_IMAGE:
                HtmlImageFree(p->x.i2.pImage);
                break;
            case CANVAS_BOX:
                HtmlComputedValuesRelease(pTree, p->x.box.pComputed);
                break;
        }
        HtmlFree(p);
    }
}


#if 0
static int 
byteToCharOffset(z, iByte)
    const unsigned char *z;
    int iByte;
{
    int iChar = 0;
    int i;
    for (i = 0; i < iByte; i++) {
        if (!(z[i] & 0x80)) iChar++;
    }
    return iChar;
}
static int 
charToByteOffset(z, iChar)
    const unsigned char *z;
    int iChar;
{
    int i = iChar;
    int iByte;
    for (iByte = 0; i > 0; iByte++) {
        if (!(z[iByte] & 0x80)) i--;
    }
    return iByte;
}
#endif

void
HtmlDrawCanvasItemRelease(pTree, pItem)
    HtmlTree *pTree;
    HtmlCanvasItem *pItem;
{
    if (pItem) {
        freeCanvasItem(pTree, pItem);
    }
}

void
HtmlDrawCanvasItemReference(pItem)
    HtmlCanvasItem *pItem;
{
    if (pItem) {
        pItem->nRef++;
    }
}

static void 
windowsRepair(pTree, pCanvas)
    HtmlTree *pTree;
    HtmlCanvas *pCanvas;
{
    HtmlNodeReplacement *p = pTree->pMapped;
    HtmlNodeReplacement *pPrev = 0;

    /* Loop through the HtmlCanvas.pMapped list. For each mapped window
     * that is clipped by the viewport, unmap the window (if mapped) and
     * remove it from the list. For each mapped window that is not clipped
     * by the viewport, reposition and map it (if unmapped).
     */
    while (p) {
        HtmlNodeReplacement *pNext = p->pNext;
        Tk_Window control = p->win;
        int iViewY;
        int iWidth;
        int iHeight;
        int iViewX;

        if (pTree) {
            iViewX = p->iCanvasX - pTree->iScrollX; 
            iViewY = p->iCanvasY - pTree->iScrollY; 
            if (Tk_Parent(control) == pTree->docwin) {
                iViewX -= Tk_X(pTree->docwin);
                iViewY -= Tk_Y(pTree->docwin);
            }
            iHeight = p->iHeight;
            iWidth = p->iWidth;
        }

        /* If the sub-window is not part of the viewable region, or the 
         * widget is being destroyed (pTree==0) unmap the window and remove it
         * from the HtmlTree.pMapped linked-list. 
         */
	if (!pTree || p->clipped || iWidth <= 0 || iHeight <= 0) {
            if (Tk_IsMapped(control)) {
                Tk_UnmapWindow(control);
            }
            if (pPrev) {
                assert(pPrev->pNext == p);
                pPrev->pNext = pNext;
            } else {
                assert(pTree->pMapped == p);
                pTree->pMapped = pNext;
            }
            p->pNext = 0;
        } else {
            if (!Tk_IsMapped(control)) {
                Tk_MoveResizeWindow(control, iViewX, iViewY, iWidth, iHeight);
                Tk_MapWindow(control);
            } else if(
                iViewX != Tk_X(control) || Tk_Y(control) != iViewY ||
                iWidth != Tk_Width(control) || Tk_Height(control) != iHeight
            ) {
                Tk_MoveResizeWindow(control, iViewX, iViewY, iWidth, iHeight);
            }
            pPrev = p;
        }

        p = pNext;
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawCleanup --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlDrawCleanup(pTree, pCanvas)
    HtmlTree *pTree;
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *pItem;
    HtmlCanvasItem *pPrev = 0;
CHECK_CANVAS(pCanvas);

    assert(pTree || !pCanvas->pFirst);

    pItem = pCanvas->pFirst;
    while (pItem) {
        Tcl_Obj *pObj = 0;
        int save = 0;
        switch (pItem->type) {
            case CANVAS_ORIGIN:
                assert(pItem->x.o.nRef >= 1 || !pItem->x.o.pSkip);
                if (pItem->x.o.pSkip) {
                    pItem->x.o.nRef--;
                    assert(pItem->x.o.pSkip->type == CANVAS_ORIGIN);
                    if (pItem->x.o.nRef > 0) {
                        assert(pItem->x.o.nRef == 1);
                        pItem = pItem->x.o.pSkip;
                        save = 1;
                    }
                }
                break;
            case CANVAS_MARKER:
                assert(pItem->x.marker.flags);
                break;
            case CANVAS_BOX:
            case CANVAS_TEXT:
            case CANVAS_IMAGE:
            case CANVAS_WINDOW:
            case CANVAS_LINE:
            case CANVAS_OVERFLOW:
                break;
            default:
                assert(!"Canvas corruption");
        }
        if (pObj) {
            Tcl_DecrRefCount(pObj);
        }
        if (pPrev) {
            pPrev->pNext = 0;
            freeCanvasItem(pTree, pPrev);
        }
        pPrev = pItem;
        pItem = (pItem == pCanvas->pLast ? 0 : pItem->pNext);

        if (save) {
            assert(pPrev->type == CANVAS_ORIGIN && !pPrev->x.o.pSkip);
            if (pItem) pPrev->pNext = 0;
            pPrev = 0;
        }
    }

    if (pPrev) {
        freeCanvasItem(pTree, pPrev);
    }
    memset(pCanvas, 0, sizeof(HtmlCanvas));
}

/*
 *---------------------------------------------------------------------------
 *
 * linkItem --
 *
 *     Link the item pItem into the end of the pCanvas link-list.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void linkItem(pCanvas, pItem)
    HtmlCanvas *pCanvas;
    HtmlCanvasItem *pItem;
{
    assert(pItem->pNext == 0);
    if (pCanvas->pFirst) {
        pCanvas->pLast->pNext = pItem;
    } else {
        pCanvas->pFirst = pItem;
    }
    pCanvas->pLast = pItem;

    assert(pItem->nRef >= 0);
    pItem->nRef++;
}

static HtmlFont *
fontFromNode(pNode)
    HtmlNode *pNode;
{
    HtmlComputedValues *p = HtmlNodeComputedValues(pNode);
    assert(p);
    return p->fFont;
}

static HtmlColor *
colorFromNode(pNode)
    HtmlNode *pNode;
{
    HtmlComputedValues *p = HtmlNodeComputedValues(pNode);
    assert(p);
    return p->cColor;
}

/*
 *---------------------------------------------------------------------------
 *
 * countPrimitives --
 *
 *     Count the number of primitives drawn directly into pCanvas.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#if 0
static int
countPrimitives(pCanvas)
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *p;
    int n = 0;
    for (p = pCanvas->pFirst; p; p = p->pNext) {
        n++;
        if (p->type == CANVAS_ORIGIN) {
            p = p->x.o.pSkip;
        }
    }
    return n;
}
#endif

static void 
movePrimitives(pCanvas, x, y)
    HtmlCanvas *pCanvas;
    int x;
    int y;
{
    HtmlCanvasItem *p;

    /* Optimization. Do nothing for a +0+0 translation. */
    if (x == 0 && y == 0) {
        return;
    }

    for (p = pCanvas->pFirst; p; p = p->pNext) {
        p->x.generic.x += x;
        p->x.generic.y += y;
        if (p->type == CANVAS_ORIGIN) {
            p = p->x.o.pSkip;
            p->x.generic.x -= x;
            p->x.generic.y -= y;
        }
    }
}

void HtmlDrawOrigin(pCanvas)
    HtmlCanvas *pCanvas;
{
    HtmlCanvasItem *pItem;
    HtmlCanvasItem *pItem2;
    if (!pCanvas->pFirst) return;
    assert(pCanvas->pLast);

    /* Allocate the first CANVAS_ORIGIN item */
    pItem = allocateCanvasItem();
    memset(pItem, 0, sizeof(HtmlCanvasItem));
    pItem->x.o.horizontal = pCanvas->left;
    pItem->x.o.vertical = pCanvas->top;
    pItem->x.o.nRef = 1;
    pItem->type = CANVAS_ORIGIN;
    pItem->nRef = 1;

    /* Add the first CANVAS_ORIGIN item to the start of the list */
    pItem->pNext = pCanvas->pFirst;
    pCanvas->pFirst = pItem;

    /* Allocate the second CANVAS_ORIGIN item */
    pItem2 = allocateCanvasItem();
    memset(pItem2, 0, sizeof(HtmlCanvasItem));
    pItem->x.o.pSkip = pItem2;
    pItem2->type = CANVAS_ORIGIN;
    pItem2->x.o.horizontal = pCanvas->right;
    pItem2->x.o.vertical = pCanvas->bottom;
    pItem2->nRef = 1;

    /* Add the first CANVAS_ORIGIN item to the end of the list */
    pCanvas->pLast->pNext = pItem2;
    pCanvas->pLast = pItem2;
}

void HtmlDrawOverflow(pCanvas, pNode, w, h)
    HtmlCanvas *pCanvas;
    HtmlNode *pNode;
    int w;
    int h;
{
    HtmlCanvasItem *pLast = pCanvas->pLast;
    HtmlCanvasItem *pItem;
CHECK_CANVAS(pCanvas);

    while (pLast && pLast->type==CANVAS_MARKER) {
        HtmlCanvasItem *pEnd = pLast;
        pLast = pCanvas->pFirst;
        if (pLast == pEnd) {
            pLast = 0;
        } else {
            while (pLast->pNext != pEnd) pLast = pLast->pNext;
        }
    }

    if (!pLast) return;
    assert(pCanvas->pFirst);

    pItem = (HtmlCanvasItem *)HtmlClearAlloc("HtmlCanvasItem",
        sizeof(HtmlCanvasItem) + sizeof(Overflow)
    );
    pItem->type = CANVAS_OVERFLOW;
    pItem->x.overflow.pNode = pNode;
    pItem->x.overflow.w = w;
    pItem->x.overflow.h = h;
    pItem->x.overflow.pEnd = pLast;
    pItem->nRef = 1;

    pItem->pNext = pCanvas->pFirst;
    pCanvas->pFirst = pItem;

    pCanvas->left = 0;
    pCanvas->top = 0;
    pCanvas->bottom = h;
    pCanvas->right = w;
CHECK_CANVAS(pCanvas);
}

void HtmlDrawCopyCanvas(pTo, pFrom)
    HtmlCanvas *pTo;
    HtmlCanvas *pFrom;
{
    assert(!pFrom->pFirst || pFrom->pFirst->type == CANVAS_ORIGIN);
    assert(!pFrom->pFirst || pFrom->pFirst->x.o.nRef == 1);
    assert(!pFrom->pLast || 
       (pFrom->pLast->type == CANVAS_ORIGIN && pFrom->pLast->pNext == 0));

CHECK_CANVAS(pTo);
CHECK_CANVAS(pFrom);

    /* The pTo canvas must be empty for this to work */
    assert(pTo->pFirst == 0);
    assert(pTo->pLast == 0);

    memcpy(pTo, pFrom, sizeof(HtmlCanvas));

    if (pTo->pFirst) {
        assert(pTo->pFirst->x.o.nRef == 1);
        pTo->pFirst->x.o.nRef++;
        pTo->pFirst->x.o.x = 0;
        pTo->pFirst->x.o.y = 0;
        pTo->pLast->x.o.x = 0;
        pTo->pLast->x.o.y = 0;
    }

    assert(pTo->pLast == 0 || pTo->pLast->pNext == 0);
CHECK_CANVAS(pTo);
CHECK_CANVAS(pFrom);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawCanvas --
 *
 *     Transfer the contents one canvas (pCanvas2) to another (pCanvas) at
 *     location (x,y). i.e. a geometric primitive at location (a,b) in
 *     pCanvas2 is transfered to location (x+a,y+b) in pCanvas.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlDrawCanvas(pCanvas, pCanvas2, x, y, pNode)
    HtmlCanvas *pCanvas;
    HtmlCanvas *pCanvas2;
    int x;
    int y;
    HtmlNode *pNode;
{
CHECK_CANVAS(pCanvas);
CHECK_CANVAS(pCanvas2);
    if (pCanvas2->pFirst) {
        movePrimitives(pCanvas2, x, y);

        if (pCanvas->pLast) {
            pCanvas->pLast->pNext = pCanvas2->pFirst;
            if (pCanvas2->pLast) {
                pCanvas->pLast = pCanvas2->pLast;
            }
        } else {
            assert(!pCanvas->pFirst);
            pCanvas->pFirst = pCanvas2->pFirst;
            pCanvas->pLast = pCanvas2->pLast;
        }
    }

    pCanvas->left = MIN(pCanvas->left, x+pCanvas2->left);
    pCanvas->top = MIN(pCanvas->top, y+pCanvas2->top);
    pCanvas->bottom = MAX(pCanvas->bottom, y+pCanvas2->bottom);
    pCanvas->right = MAX(pCanvas->right, x+pCanvas2->right);
CHECK_CANVAS(pCanvas);
}

#if 0
static HtmlNode *
itemToNode(pItem)
    HtmlCanvasItem *pItem;
{
    HtmlNode *pNode = 0;
    switch (pItem->type) {
        case CANVAS_BOX:
            pNode = pItem->x.box.pNode;
            break;
        case CANVAS_TEXT:
            pNode = pItem->x.t.pNode;
            break;
        case CANVAS_IMAGE:
            pNode = pItem->x.i2.pNode;
            break;
        case CANVAS_LINE:
            pNode = pItem->x.line.pNode;
            break;
        case CANVAS_WINDOW:
            pNode = pItem->x.w.pNode;
            break;
    }
    return pNode;
}
#endif
#define itemToNode(pItem) (pItem->x.generic.pNode)

static HtmlNode *
itemToBox(pItem, origin_x, origin_y, pX, pY, pW, pH)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    int *pX;
    int *pY;
    int *pW;
    int *pH;
{
    switch (pItem->type) {
        case CANVAS_BOX: {
            int ow = 0;
/*
            HtmlComputedValues *pV = pItem->x.box.pNode->pPropertyValues;
            if (pV->eOutlineStyle != CSS_CONST_NONE) {
                ow = MAX(0, pV->iOutlineWidth);
            }
*/
            *pX = pItem->x.box.x + origin_x - ow;
            *pY = pItem->x.box.y + origin_y - ow;
            *pW = pItem->x.box.w + ow + ow;
            *pH = pItem->x.box.h + ow + ow;
            return pItem->x.box.pNode;
        }
        case CANVAS_TEXT: {
            HtmlFont *pFont = pItem->x.t.fFont;
            *pX = pItem->x.t.x + origin_x;
            *pY = pItem->x.t.y + origin_y - pFont->metrics.ascent;
            *pW = pItem->x.t.w;
            *pH = pFont->metrics.ascent + pFont->metrics.descent;
            return pItem->x.t.pNode;
        }
        case CANVAS_IMAGE:
            *pX = pItem->x.i2.x + origin_x;
            *pY = pItem->x.i2.y + origin_y;
            *pW = pItem->x.i2.w;
            *pH = pItem->x.i2.h;
            return pItem->x.i2.pNode;
        case CANVAS_LINE:
            *pX = pItem->x.line.x + origin_x;
            *pY = pItem->x.line.y + origin_y;
            *pW = pItem->x.line.w;
            *pH = pItem->x.line.y_underline + 1;
            return pItem->x.line.pNode;
        case CANVAS_WINDOW: {
            HtmlNodeReplacement *pR = pItem->x.w.pElem->pReplacement;
            if (pR && pR->win) {
                Tk_Window control = pR->win;
                *pW = Tk_ReqWidth(control);
                *pH = Tk_ReqHeight(control);
            } else {
                *pW = 1;
                *pH = 1;
            }
            *pX = pItem->x.w.x + origin_x;
            *pY = pItem->x.w.y + origin_y;
            return 0;
        }
        default:
            assert(
                pItem->type==CANVAS_ORIGIN || 
                pItem->type==CANVAS_MARKER ||
                pItem->type==CANVAS_OVERFLOW 
            );
            return 0;
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawBox --
 *
 *     Draw a "box" based on the computed properties of node pNode. A "box"
 *     consists of zero or more of the following optional components:
 *
 *         - A border,
 *         - A solid background color,
 *         - A background image.
 *
 *     The (x,y) coordinate specifies the top-left hand corner of the box (the
 *     outer border pixel, if a border is defined). The specified width and
 *     height include any borders and padding, but do NOT include any space 
 *     for the outline (if any).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Adds an item to the canvas pCanvas.
 *
 *---------------------------------------------------------------------------
 */
HtmlCanvasItem *
HtmlDrawBox(pCanvas, x, y, w, h, pNode, flags, size_only, pCandidate)
    HtmlCanvas *pCanvas;
    int x;
    int y;
    int w;
    int h;
    HtmlNode *pNode;
    int flags;
    int size_only;
    HtmlCanvasItem *pCandidate;
{
    if (!size_only) {
        int x1, y1, w1, h1;
        HtmlCanvasItem *pItem; 
        HtmlComputedValues *pComputed = HtmlNodeComputedValues(pNode);
        
        if (
            pCandidate && 
            pCandidate->x.box.w == w && pCandidate->x.box.h == h &&
            pCandidate->x.box.flags == flags &&
            pCandidate->x.box.pComputed == pComputed
        ) {
            pItem = pCandidate;
            assert(pCandidate->type == CANVAS_BOX);
            assert(pCandidate->x.box.pNode == pNode);
        } else {
            pItem = allocateCanvasItem();
            pItem->type = CANVAS_BOX;
            pItem->x.box.w = w;
            pItem->x.box.h = h;
            pItem->x.box.pNode = pNode;
            pItem->x.box.flags = flags;
            pItem->x.box.pComputed = pComputed;
            HtmlComputedValuesReference(pComputed);
        }

        pItem->x.box.x = x;
        pItem->x.box.y = y;

        linkItem(pCanvas, pItem);
        itemToBox(pItem, 0, 0, &x1, &y1, &w1, &h1);
        pCanvas->left = MIN(pCanvas->left, x1);
        pCanvas->right = MAX(pCanvas->right, x1 + w1);
        pCanvas->bottom = MAX(pCanvas->bottom, y1 + h1);
        pCanvas->top = MIN(pCanvas->top, y1);

        return pItem;
    } else {
        pCanvas->left = MIN(pCanvas->left, x);
        pCanvas->right = MAX(pCanvas->right, x + w);
        pCanvas->bottom = MAX(pCanvas->bottom, y + h);
        pCanvas->top = MIN(pCanvas->top, y);
    }

    return 0;
}

void 
HtmlDrawLine(pCanvas, x, w, y_over, y_through, y_under, pNode, size_only)
    HtmlCanvas *pCanvas;
    int x;
    int w;
    int y_over;
    int y_through;
    int y_under;
    HtmlNode *pNode;
    int size_only;
{
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = allocateCanvasItem();
        pItem->type = CANVAS_LINE;
        pItem->x.line.x = x;
        pItem->x.line.w = w;
        pItem->x.line.y = y_over;
        pItem->x.line.y_underline = (y_under - y_over);
        pItem->x.line.y_linethrough = (y_through - y_over);
        pItem->x.line.pNode = pNode;
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x + w);
    pCanvas->bottom = MAX(pCanvas->bottom, y_under);
    pCanvas->top = MIN(pCanvas->top, y_over);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawText --
 *
 *     Add a single line of text drawn in a single font to a canvas.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlDrawText(pCanvas, zText, nText, x, y, w, size_only, pNode, iIndex)
    HtmlCanvas *pCanvas; 
    const char *zText;
    int nText;
    int x;
    int y;
    int w;
    int size_only;
    HtmlNode *pNode;
    int iIndex;
{
    HtmlFont *pFont = fontFromNode(pNode);

    if (!size_only) {
        HtmlCanvasItem *pItem; 

        if (iIndex >= 0) {
            pItem = allocateCanvasItem();
            pItem->x.t.zText = zText;
        } else {
            int nBytes = nText + sizeof(HtmlCanvasItem);
            pItem = (HtmlCanvasItem *)HtmlClearAlloc("HtmlCanvasItem", nBytes);
            pItem->x.t.zText = (char *)&pItem[1];
            memcpy((char *)pItem->x.t.zText, zText, nText);
        }

        pItem->type = CANVAS_TEXT;
        pItem->x.t.nText = nText;
        pItem->x.t.x = x;
        pItem->x.t.y = y;
        pItem->x.t.w = w;
        pItem->x.t.pNode = pNode;
        pItem->x.t.iIndex = iIndex;
        pItem->x.t.fFont = pFont;
        HtmlFontReference(pFont);
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x + w);
    pCanvas->bottom = MAX(pCanvas->bottom, y + pFont->metrics.descent);
    pCanvas->top = MIN(pCanvas->top, y - pFont->metrics.ascent);
}

void
HtmlDrawTextExtend(pCanvas, nChar, nPixel) 
    HtmlCanvas *pCanvas;
    int nChar; 
    int nPixel; 
{
    assert(pCanvas && pCanvas->pLast && pCanvas->pLast->type == CANVAS_TEXT);
    pCanvas->pLast->x.t.nText += nChar;
    pCanvas->pLast->x.t.w += nPixel;
}

int
HtmlDrawTextLength(pCanvas)
    HtmlCanvas *pCanvas;
{
    assert(pCanvas && pCanvas->pLast && pCanvas->pLast->type == CANVAS_TEXT);
    return pCanvas->pLast->x.t.nText;
}

void 
HtmlDrawImage(
        pCanvas, pImage, 
        x, y, w, h, 
        pNode,
        size_only
)
    HtmlCanvas *pCanvas;
    HtmlImage2 *pImage;               /* Image name or NULL */
    HtmlNode *pNode;
    int x; 
    int y;
    int w;                      /* Width of image */
    int h;                      /* Height of image */
    int size_only;
{
    HtmlImageCheck(pImage);
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        pItem = allocateCanvasItem();
        pItem->type = CANVAS_IMAGE;
        pItem->x.i2.pImage = pImage;
        HtmlImageRef(pImage);
        pItem->x.i2.x = x;
        pItem->x.i2.y = y;
        pItem->x.i2.w = w;
        pItem->x.i2.h = h;
        pItem->x.i2.pNode = pNode;
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x+w);
    pCanvas->bottom = MAX(pCanvas->bottom, y+h);
    pCanvas->top = MIN(pCanvas->top, y);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawWindow --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlDrawWindow(pCanvas, pNode, x, y, w, h, size_only)
    HtmlCanvas *pCanvas;
    HtmlNode *pNode;
    int x; 
    int y;
    int w;       /* Width of window */
    int h;       /* Height of window */
    int size_only;
{
    if (!size_only) {
        HtmlCanvasItem *pItem; 
        assert(!HtmlNodeIsText(pNode));
        pItem = allocateCanvasItem();
        memset(pItem, 0, sizeof(HtmlCanvasItem));
        pItem->type = CANVAS_WINDOW;
        pItem->x.w.pElem = (HtmlElementNode *)pNode;
        pItem->x.w.x = x;
        pItem->x.w.y = y;
        pItem->x.w.iWidth = w;
        pItem->x.w.iHeight = h;
        linkItem(pCanvas, pItem);
    }

    pCanvas->left = MIN(pCanvas->left, x);
    pCanvas->right = MAX(pCanvas->right, x+w);
    pCanvas->bottom = MAX(pCanvas->bottom, y+h);
    pCanvas->top = MIN(pCanvas->top, y);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutPrimitives --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlLayoutPrimitives(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlCanvasItem *pItem;
    Tcl_Obj *aObj[13];
    int nObj = 0;
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlCanvas *pCanvas = &pTree->canvas;
    Tcl_Obj *pPrimitives;

    pPrimitives = Tcl_NewObj();
    Tcl_IncrRefCount(pPrimitives);

    for (pItem=pCanvas->pFirst; pItem; pItem=pItem->pNext) {
        Tcl_Obj *pList = 0;
        nObj = 0;
        switch (pItem->type) {
            case CANVAS_OVERFLOW:
                nObj = 3;
                aObj[0] = Tcl_NewStringObj("draw_overflow", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.overflow.w);
                aObj[2] = Tcl_NewIntObj(pItem->x.overflow.h);
                break;
            case CANVAS_ORIGIN:
                if (pItem->x.o.pSkip) {
                    nObj = 5;
                    aObj[0] = Tcl_NewStringObj("draw_origin_start", -1);
                    aObj[1] = Tcl_NewIntObj(pItem->x.o.x);
                    aObj[2] = Tcl_NewIntObj(pItem->x.o.y);
                    aObj[3] = Tcl_NewIntObj(pItem->x.o.horizontal);
                    aObj[4] = Tcl_NewIntObj(pItem->x.o.vertical);
                } else {
                    nObj = 5;
                    aObj[0] = Tcl_NewStringObj("draw_origin_end", -1);
                    aObj[1] = Tcl_NewIntObj(pItem->x.o.x);
                    aObj[2] = Tcl_NewIntObj(pItem->x.o.y);
                    aObj[3] = Tcl_NewIntObj(pItem->x.o.horizontal);
                    aObj[4] = Tcl_NewIntObj(pItem->x.o.vertical);
                }
                break;
            case CANVAS_TEXT: {
                nObj = 7;
                aObj[0] = Tcl_NewStringObj("draw_text", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.t.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.t.y);
                aObj[3] = Tcl_NewIntObj(pItem->x.t.w);
                aObj[4] = HtmlNodeCommand(pTree, pItem->x.t.pNode);
                if( !aObj[4] ){
                    aObj[4] = Tcl_NewStringObj("(null)", 0);
                }
                aObj[5] = Tcl_NewIntObj(pItem->x.t.iIndex);
                aObj[6] = Tcl_NewStringObj(
                    pItem->x.t.zText, pItem->x.t.nText 
                );
                break;
            }
            case CANVAS_IMAGE:
                if (pItem->x.i2.pImage) {
                    nObj = 7;
                    aObj[0] = Tcl_NewStringObj("draw_image", -1);
                    aObj[1] = Tcl_NewIntObj(pItem->x.i2.x);
                    aObj[2] = Tcl_NewIntObj(pItem->x.i2.y);
                    aObj[3] = Tcl_NewIntObj(pItem->x.i2.w);
                    aObj[4] = Tcl_NewIntObj(pItem->x.i2.h);
                    aObj[5] = HtmlNodeCommand(pTree, pItem->x.i2.pNode);
                    aObj[6] = HtmlImageUnscaledName(pItem->x.i2.pImage);
                }
                break;
            case CANVAS_WINDOW:
                nObj = 4;
                aObj[0] = Tcl_NewStringObj("draw_window", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.w.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.w.y);
                aObj[3] = pItem->x.w.pElem->pReplacement->pReplace;
                break;
            case CANVAS_BOX:
                nObj = 6;
                aObj[0] = Tcl_NewStringObj("draw_box", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.box.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.box.y);
                aObj[3] = Tcl_NewIntObj(pItem->x.box.w);
                aObj[4] = Tcl_NewIntObj(pItem->x.box.h);
                aObj[5] = HtmlNodeCommand(pTree, pItem->x.box.pNode);
                break;
            case CANVAS_LINE:
                nObj = 7;
                aObj[0] = Tcl_NewStringObj("draw_line", -1);
                aObj[1] = Tcl_NewIntObj(pItem->x.line.x);
                aObj[2] = Tcl_NewIntObj(pItem->x.line.y);
                aObj[3] = Tcl_NewIntObj(pItem->x.line.w);
                aObj[4] = Tcl_NewIntObj(pItem->x.line.y_linethrough);
                aObj[5] = Tcl_NewIntObj(pItem->x.line.y_underline);
                aObj[6] = HtmlNodeCommand(pTree, pItem->x.line.pNode);
                break;
        }
        if (nObj>0) {
            int ii;
            pList = Tcl_NewObj();
            for (ii = 0; ii < nObj; ii++) {
                if (!aObj[ii]) aObj[ii] = Tcl_NewStringObj("", -1);
            }
            Tcl_SetListObj(pList, nObj, aObj);
        }
        if (pList) {
            Tcl_ListObjAppendElement(interp, pPrimitives, pList);
        }
    }
    Tcl_SetObjResult(interp, pPrimitives);
    Tcl_DecrRefCount(pPrimitives);
    return TCL_OK;
}

typedef struct Outline Outline;
struct Outline {
    int x;
    int y;
    int w;
    int h;
    HtmlNode *pNode;
    Outline *pNext;
};

typedef struct GetPixmapQuery GetPixmapQuery;
struct GetPixmapQuery {
    HtmlTree *pTree;
    HtmlNode *pBgRoot;
    int x;
    int y;
    int w;
    int h;
    int getwin;
    Outline *pOutline;
    Pixmap pmap;

    Overflow *pCurrentOverflow;
    Overflow *pOverflowList;
};

static void
setClippingDrawable(pQuery, pItem, pDrawable, pX, pY)
    GetPixmapQuery *pQuery;
    HtmlCanvasItem *pItem;
    Drawable *pDrawable;
    int *pX;
    int *pY;
{
#if !USE_XLIB_CLIPPING
    Overflow *p = pQuery->pCurrentOverflow;
    if (p && *pDrawable != p->pixmap) {
        int x, y, w, h;
        int ii;

        if (
            p->pmw <= 0 || p->pmh <= 0 ||
            (p->pmx == pQuery->x && p->pmy == pQuery->y &&
            p->pmw == pQuery->w && p->pmh == pQuery->h)
        ) {
            return;
        }

        itemToBox(pItem, *pX + pQuery->x, *pY + pQuery->y, &x, &y, &w, &h);
        if (pItem->type == CANVAS_TEXT) {
            int nSpace = 0;
            CanvasText *pText = &pItem->x.t;
            for (ii = pText->nText - 1; ii >= 0; ii--) {
              if (pText->zText[ii] == '\xA0' && pText->zText[ii-1] == '\xC2') {
                  ii--;
              } else if (pText->zText[ii] != ' ') {
                  break;
              }
              nSpace++;
            } 
            w -= (nSpace * pText->fFont->space_pixels);
        }

        if (
            w > 0 && (
                x < p->x || y < p->y || 
                (x + w) > (p->x + p->w) || (y + h) > (p->y + p->h)
            )
        ) {
            Tk_Window win = pQuery->pTree->tkwin;
            GC gc;
            XGCValues gc_values;

#if 0
printf("Create overflow pixmap 2\n");
printf("TEXT: %dx%d +%d+%d \"%.*s\"(%d)", w, h, x, y, pText->nText, pText->zText, pText->nText);
printf("NSPA: %d\n", nSpace);
printf("%s\n", Tcl_GetString(HtmlNodeCommand(pQuery->pTree, pText->pNode)));
printf("OVFL: %dx%d +%d+%d ", p->w, p->h, p->x, p->y);
printf("%s\n", Tcl_GetString(HtmlNodeCommand(pQuery->pTree, p->pItem->pNode)));
#endif

            if (!p->pixmap) {
#if 0
                printf(
                    "TODO: Using %dx%d pixmap for clipping. (performance hit)\n"
                    , p->pmw, p->pmh
                );
#endif
                p->pixmap = Tk_GetPixmap(
    		    Tk_Display(win), Tk_WindowId(win), 
                    p->pmw, p->pmh,
                    Tk_Depth(win)
                );
                assert(p->pixmap);

                /* Since we have allocated a pixmap, link this structure
                 * into the GetPixmapQuery.pOverflowList linked list. This
                 * is used later to free all allocated pixmaps.
                 */
                p->pNext = pQuery->pOverflowList;
                pQuery->pOverflowList = p;
            }
            memset(&gc_values, 0, sizeof(XGCValues));
            gc = Tk_GetGC(pQuery->pTree->tkwin, 0, &gc_values);

            assert(p->pmx >= pQuery->x);
            assert(p->pmy >= pQuery->y);
            XCopyArea(Tk_Display(win), pQuery->pmap, p->pixmap, gc, 
                p->pmx - pQuery->x, p->pmy - pQuery->y, 
                p->pmw, p->pmh, 
                0, 0
            );
            Tk_FreeGC(Tk_Display(win), gc);

            *pDrawable = p->pixmap;
            *pX += (pQuery->x - p->pmx);
            *pY += (pQuery->y - p->pmy);
        }
    }
#endif /* if !USE_XLIB_CLIPPING */
}

static void
setClippingRegion(pQuery, pDisplay, gc)
    GetPixmapQuery *pQuery;
    Display *pDisplay;
    GC gc;
{
#if USE_XLIB_CLIPPING
    Overflow *p = pQuery->pCurrentOverflow;
    if (p) {
        XRectangle rectangles[1];
        assert(p->pmw>0 && p->pmh>0);

        assert(p->pmx >= pQuery->x);
        assert(p->pmy >= pQuery->y);
        rectangles[0].x = (p->pmx - pQuery->x);
        rectangles[0].y = (p->pmy - pQuery->y);
        rectangles[0].width = p->pmw;
        rectangles[0].height = p->pmh;

        XSetClipRectangles(pDisplay, gc, 0, 0, rectangles, 1, Unsorted);

#if 0
        printf("Set clipping region to: %dx%d, +%d+%d\n",
            rectangles[0].x, rectangles[0].y,
            rectangles[0].width, rectangles[0].height
        );
#endif
    }

#endif /* if USE_XLIB_CLIPPING */
}

static void
clearClippingRegion(pDisplay, gc)
    Display *pDisplay;
    GC gc;
{
#if USE_XLIB_CLIPPING
    XSetClipMask(pDisplay, gc, None);
#endif
}

static int
fill_quad(pQuery, win, d, xcolor, x1, y1, x2, y2, x3, y3, x4, y4)
    GetPixmapQuery *pQuery;
    Tk_Window win;
    Drawable d;
    XColor *xcolor;
    int x1; int y1;
    int x2; int y2;
    int x3; int y3;
    int x4; int y4;
{
    XPoint points[4];
    Display *display = Tk_Display(win);
    GC gc;
    XGCValues gc_values;
    int rc = 0;

    gc_values.foreground = xcolor->pixel;
    gc = Tk_GetGC(win, GCForeground, &gc_values);
    if (pQuery) {
        setClippingRegion(pQuery, display, gc);
    }

    /* The coordinates provided to this function are suitable for
     * passing to XFillPolygon() with the "mode" argument set to 
     * CoordModePrevious. However not all Tk platforms (notably MacOSX,
     * but probably others too) support this mode. So manipulate the
     * supplied coordinates here so that they can be passed with
     * the mode set to CoordModeOrigin.
     */
    points[0].x = x1; 
    points[0].y = y1;
    points[1].x = points[0].x + x2; 
    points[1].y = points[0].y + y2;
    points[2].x = points[1].x + x3; 
    points[2].y = points[1].y + y3;
    points[3].x = points[2].x + x4; 
    points[3].y = points[2].y + y4;

    XFillPolygon(display, d, gc, points, 4, Convex, CoordModeOrigin);

    clearClippingRegion(display, gc);
    Tk_FreeGC(display, gc);
    return rc;
}

static int
fill_rectangle(win, d, xcolor, x, y, w, h)
    Tk_Window win;
    Drawable d;
    XColor *xcolor;
    int x; int y;
    int w; int h;
{
    if (w > 0 && h > 0){
        Display *display = Tk_Display(win);
        GC gc;
        XGCValues gc_values;
    
        gc_values.foreground = xcolor->pixel;
        gc = Tk_GetGC(win, GCForeground, &gc_values);
        XFillRectangle(display, d, gc, x, y, w, h);
        Tk_FreeGC(display, gc);
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutPrimitives --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
tileimage(
pQuery, drawable, d_w, d_h, pImage, bg_x, bg_y, bg_w, bg_h, iPosX, iPosY)
    GetPixmapQuery *pQuery;   /* Clipping region */
    Drawable drawable;        /* Where to draw */
    int d_w; int d_h;         /* Total width and height of drawable */
    HtmlImage2 *pImage;
    int bg_x; int bg_y;       /* Drawable coords for drawn block */
    int bg_w; int bg_h;       /* Width and height of drawn block */
    int iPosX; int iPosY;     /* Origin of image in drawable */
{
    int x1, y1;

    int clip_x1 = MAX(0, bg_x);
    int clip_y1 = MAX(0, bg_y);
    int clip_x2 = MIN(d_w, bg_x + bg_w);
    int clip_y2 = MIN(d_h, bg_y + bg_h);

    Tk_Image img = 0;
    Pixmap pix = 0;
    int i_w;
    int i_h;

    /* Clipping for pQuery */
#if 0
    if (
        pQuery->pCurrentOverflow && 
        drawable != pQuery->pCurrentOverflow->pixmap
    ) {
        Overflow *p = pQuery->pCurrentOverflow;
        int ocx = p->pmx - pQuery->x;
        int ocy = p->pmx - pQuery->x;
        clip_x1 = MAX(clip_x1, ocx);
        clip_y1 = MAX(clip_y1, ocy);
        clip_x2 = MIN(clip_x2, ocx + p->pmw);
        clip_y2 = MIN(clip_y2, ocy + p->pmh);
    }
#endif

    HtmlImageSize(pImage, &i_w, &i_h);
    if (bg_h > (i_h * 2) && bg_w > (i_w * 2)) {
        pix = HtmlImageTilePixmap(pImage, &i_w, &i_h);
        if (!pix) {
            img = HtmlImageTile(pImage, &i_w, &i_h);
	}
    } else {
        pix = HtmlImagePixmap(pImage);
        if (!pix) {
            img = HtmlImageImage(pImage);
        }
    }
    if (i_w <= 0 || i_h <= 0) return;

    x1 = iPosX;
    if (iPosX != bg_x) {
        x1 -= (1 + (iPosX - bg_x) / i_w) * i_w;
    }

    for (; x1 < bg_x + bg_w; x1 += i_w) {
        y1 = iPosY;
        if (iPosY != bg_y) {
            y1 -= (1 + (iPosY - bg_y) / i_h) * i_h;
        }
        for (; y1 < bg_y + bg_h; y1 += i_h) {

            int w = i_w;
            int h = i_h;
            int im_x = 0;
            int im_y = 0;
            int x = x1;
            int y = y1;

            if (x + w > clip_x2) {
                w = (clip_x2 - x);
            }
            if (y + h > clip_y2) {
                h = (clip_y2 - y);
            }

            if (x < clip_x1) {
                im_x = clip_x1 - x;
                w -= (clip_x1 - x);
                x = clip_x1;
            }
            if (y < clip_y1) {
                im_y = clip_y1 - y;
                h -= im_y;
                y = clip_y1;
            }

            if (w > 0 && h > 0) {
                if (pix) {
                    Tk_Window win = pQuery->pTree->tkwin;
                    XGCValues gc_values;
                    GC gc;
                    memset(&gc_values, 0, sizeof(XGCValues));
                    gc = Tk_GetGC(win, 0, &gc_values);
                    XCopyArea(Tk_Display(win), 
                        pix, drawable, gc, im_x, im_y, w, h, x, y
                    );
                    Tk_FreeGC(Tk_Display(win), gc);
                } else {
                    Tk_RedrawImage(img, im_x, im_y, w, h, drawable, x, y);
                }
            }
        }
    }
}

static void
drawScrollbars(pTree, pItem, origin_x, origin_y)
    HtmlTree *pTree;
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
{
    HtmlElementNode *pElem = (HtmlElementNode *)pItem->x.generic.pNode;
    assert(!HtmlNodeIsText(pItem->x.generic.pNode));

    if (pElem->pScrollbar) {
        HtmlNodeReplacement *pRep = &pElem->pScrollbar->vertical;
        HtmlNodeReplacement *p;
        HtmlComputedValues *pV = HtmlNodeComputedValues(pItem->x.box.pNode);

        /* Vertical */
        if (pRep->win) {
            pRep->iCanvasX  = origin_x + pItem->x.box.x + pItem->x.box.w;
            pRep->iCanvasX -= pRep->iWidth;
            if (pV->eBorderRightStyle != CSS_CONST_NONE) {
                pRep->iCanvasX -= pV->border.iRight;
            }
            pRep->iCanvasY = origin_y + pItem->x.box.y;
            if (pV->eBorderTopStyle != CSS_CONST_NONE) {
                pRep->iCanvasY += pV->border.iTop;
            }
            for (p = pTree->pMapped; p && p != pRep; p = p->pNext);
            if (!p) {
                pRep->pNext = pTree->pMapped;
                pTree->pMapped = pRep;
            }
        }

        /* Horizontal */
        pRep = &pElem->pScrollbar->horizontal;
        if (pRep->win) {
            pRep->iCanvasY  = origin_y + pItem->x.box.y + pItem->x.box.h;
            pRep->iCanvasY -= pRep->iHeight;
            if (pV->eBorderBottomStyle != CSS_CONST_NONE) {
                pRep->iCanvasY -= pV->border.iBottom;
            }
            pRep->iCanvasX = origin_x + pItem->x.box.x;
            if (pV->eBorderLeftStyle != CSS_CONST_NONE) {
                pRep->iCanvasX += pV->border.iLeft;
            }
            for (p = pTree->pMapped; p && p != pRep; p = p->pNext);
            if (!p) {
                pRep->pNext = pTree->pMapped;
                pTree->pMapped = pRep;
            }
        }
    }
}

#define DRAWBOX_NOBORDER     0x00000001
#define DRAWBOX_NOBACKGROUND 0x00000002

/*
 *---------------------------------------------------------------------------
 *
 * drawBox --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Outline* 
drawBox(pQuery, pItem, pBox, drawable, x, y, w, h, xview, yview, flags)
    GetPixmapQuery *pQuery;
    HtmlCanvasItem *pItem;
    CanvasBox *pBox;
    Drawable drawable;
    int x;                 /* X-coord in *pDrawable */
    int y;                 /* Y-coord in *pDrawable */
    int w;                 /* Total width of *pDrawable */
    int h;                 /* Total height of *pDrawable */
    int xview;             /* X-coord of drawable in viewport */
    int yview;             /* Y-coord of drawable in viewport */
    int flags;             /* Combination of DRAWBOX_XXX flags */
{
    HtmlTree *pTree = pQuery->pTree;
    HtmlComputedValues *pV = HtmlNodeComputedValues(pBox->pNode);

    /* Figure out the widths of the top, bottom, right and left borders */
    int tw = ((pV->eBorderTopStyle != CSS_CONST_NONE) ? pV->border.iTop : 0);
    int bw = ((pV->eBorderBottomStyle != CSS_CONST_NONE)?pV->border.iBottom:0);
    int rw = ((pV->eBorderRightStyle != CSS_CONST_NONE) ? pV->border.iRight :0);
    int lw = ((pV->eBorderLeftStyle != CSS_CONST_NONE) ? pV->border.iLeft : 0);
    int ow = ((pV->eOutlineStyle != CSS_CONST_NONE) ? pV->iOutlineWidth : 0);

    int bg_x = x + pBox->x + lw;      /* Drawable x coord for background */
    int bg_y = y + pBox->y + tw;      /* Drawable y coord for background */
    int bg_w = pBox->w - lw - rw;     /* Width of background rectangle */
    int bg_h = pBox->h - tw - bw;     /* Height of background rectangle */

    /* Figure out the colors of the top, bottom, right and left borders */
    XColor *tc = pV->cBorderTopColor->xcolor;
    XColor *rc = pV->cBorderRightColor->xcolor;
    XColor *bc = pV->cBorderBottomColor->xcolor;
    XColor *lc = pV->cBorderLeftColor->xcolor;
    XColor *oc = pV->cOutlineColor->xcolor;

    /* int isInline = (pV->eDisplay == CSS_CONST_INLINE); */
    if (pItem) {
        setClippingDrawable(pQuery, pItem, &drawable, &x, &y);
    }
    assert(pBox);

    if (pBox->flags & CANVAS_BOX_OPEN_LEFT) {
        lw = 0;
    }
    if (pBox->flags & CANVAS_BOX_OPEN_RIGHT) {
        rw = 0;
    }

    /* Solid background, if required */
    if (0 == (flags & DRAWBOX_NOBACKGROUND) && pV->cBackgroundColor->xcolor) {
        int boxw = pBox->w + MIN((x + pBox->x), 0);
        int boxh = pBox->h + MIN((y + pBox->y), 0);
        fill_rectangle(pTree->tkwin, 
            drawable, pV->cBackgroundColor->xcolor,
            MAX(0, x + pBox->x), MAX(0, y + pBox->y),
            MIN(boxw, w), MIN(boxh, h)
        );
    }

    if (0 == (flags & DRAWBOX_NOBORDER)) {
        /* Top border */
        if (tw > 0 && tc) {
            fill_quad(pQuery, pTree->tkwin, drawable, tc,
                x + pBox->x, y + pBox->y,
                lw, tw,
                pBox->w - lw - rw, 0,
                rw, -1 * tw
            );
        }
    
        /* Left border, if required */
        if (lw > 0 && lc) {
            fill_quad(pQuery, pTree->tkwin, drawable, lc,
                x + pBox->x, y + pBox->y,
                lw, tw,
                0, pBox->h - tw - bw,
                -1 * lw, bw
            );
        }
    
        /* Bottom border, if required */
        if (bw > 0 && bc) {
            fill_quad(pQuery, pTree->tkwin, drawable, bc,
                x + pBox->x, y + pBox->y + pBox->h,
                lw, - 1 * bw,
                pBox->w - lw - rw, 0,
                rw, bw
            );
        }
    
        /* Right border, if required */
        if (rw > 0 && rc) {
            fill_quad(pQuery, pTree->tkwin, drawable, rc,
                x + pBox->x + pBox->w, y + pBox->y,
                -1 * rw, tw,
                0, pBox->h - tw - bw,
                rw, bw
            );
        }
    }

    /* Image background, if required. */
    if (0 == (flags & DRAWBOX_NOBACKGROUND) && pV->imZoomedBackgroundImage) {
#if 0
        Pixmap ipix;
        GC gc;
        XGCValues gc_values;
#endif
        int iWidth;
        int iHeight;
#if 0
        Tk_Window win = pTree->tkwin;
        Display *display = Tk_Display(win);
        int dep = Tk_Depth(win);
#endif
        int eR = pV->eBackgroundRepeat;

 
        HtmlImageSize(pV->imZoomedBackgroundImage, &iWidth, &iHeight);

        if (iWidth > 0 && iHeight > 0) {
            int iPosX;
            int iPosY;
            /* HtmlNode *pBgNode = pBox->pNode; */

#ifdef WIN32
            /*
	     * Todo: On windows, using XFillRectangle() to draw the image
	     * doesn't seem to work. This is probably a shortcoming of the Tk
	     * porting layer, but this hasn't been checked properly yet. For
             * now, disable the XFillRectangle() optimization. 
             */
            int isAlpha = 1;
#else
	    /* Update: Maybe the XFillRectangle() optimization is less helpful
             * than it seems.
             */
#if 0
            int isAlpha = 
                HtmlImageAlphaChannel(pTree, pV->imZoomedBackgroundImage);
#endif
            int isAlpha = 1;
#endif
    
            iPosX = pV->iBackgroundPositionX;
            iPosY = pV->iBackgroundPositionY;
            if (pV->eBackgroundAttachment == CSS_CONST_SCROLL) {
                if ( pV->mask & PROP_MASK_BACKGROUND_POSITION_X ){
                    iPosX = (double)iPosX * (double)(bg_w - iWidth) / 10000.0;
                }
                if ( pV->mask & PROP_MASK_BACKGROUND_POSITION_Y ){
                    iPosY = (double)iPosY * (double)(bg_h - iHeight) / 10000.0;
                }
                iPosX += bg_x;
                iPosY += bg_y;
            } else {
                /* 'background-attachment' is "fixed" */
                int rw = Tk_Width(pTree->tkwin);
                int rh = Tk_Height(pTree->tkwin);
                if ( pV->mask & PROP_MASK_BACKGROUND_POSITION_X ){
                    iPosX = (double)iPosX * (double)(rw - iWidth) / 10000.0;
                }
                if ( pV->mask & PROP_MASK_BACKGROUND_POSITION_Y ){
                    iPosY = (double)iPosY * (double)(rh - iHeight) / 10000.0;
                }
                iPosX -= xview;
                iPosY -= yview;
            }

            if (eR != CSS_CONST_REPEAT && eR != CSS_CONST_REPEAT_X) {
                int draw_x1 = MAX(bg_x, iPosX);
                int draw_x2 = MIN(bg_x + bg_w, iPosX + iWidth);
                bg_x = draw_x1;
                bg_w = draw_x2 - draw_x1;
            } 
            if (eR != CSS_CONST_REPEAT && eR != CSS_CONST_REPEAT_Y) {
                int draw_y1 = MAX(bg_y, iPosY);
                int draw_y2 = MIN(bg_y + bg_h, iPosY + iHeight);
                bg_y = draw_y1;
                bg_h = draw_y2 - draw_y1;
            }

            if (isAlpha) {
                tileimage(
                    pQuery, drawable, w, h, 
                    pV->imZoomedBackgroundImage,
                    bg_x, bg_y, bg_w, bg_h, 
                    iPosX, iPosY
                );
            } else {
#if 0
                /* Create a pixmap of the image */
                ipix = Tk_GetPixmap(
                    display, Tk_WindowId(win), iWidth, iHeight, dep
                );
                for ( ; pBgNode; pBgNode = HtmlNodeParent(pBgNode)) {
                    HtmlComputedValues *pV2 = HtmlNodeComputedValues(pBgNode);
                    if (pV2->cBackgroundColor->xcolor) {
                        fill_quad(0, pTree->tkwin, ipix, 
                            pV2->cBackgroundColor->xcolor,
                            0, 0, iWidth, 0, 0, iHeight, -1 * iWidth, 0
                        );
                        break;
                    }
                }
                Tk_RedrawImage(img, 0, 0, iWidth, iHeight, ipix, 0, 0);
        
		/* Draw a rectangle to the drawable with origin (bg_x, bg_y).
		 * The size of the rectangle is (bg_w *  bg_h). The background
		 * image is tiled across the region with a relative origin
		 * point as defined by (gc_values.ts_x_origin,
		 * gc_values.ts_y_origin).
                 */
                gc_values.ts_x_origin = iPosX;
                gc_values.ts_y_origin = iPosY;
                gc_values.tile = ipix;
                gc_values.fill_style = FillTiled;
                gc = Tk_GetGC(pTree->tkwin, 
                    GCTile|GCTileStipXOrigin|GCTileStipYOrigin|GCFillStyle, 
                    &gc_values
                );
                setClippingRegion(pQuery, display, gc);
                if (bg_h > 0 && bg_w > 0) {
                    XFillRectangle(display,drawable,gc,bg_x,bg_y,bg_w,bg_h);
                }
                Tk_FreePixmap(display, ipix);
                clearClippingRegion(display, gc);
                Tk_FreeGC(display, gc);
#endif
            }
        }
    }

    /* Outline, if required */
    if (ow > 0 && oc) {
        Outline *pOutline = HtmlNew(Outline);
        pOutline->x = x + pBox->x;
        pOutline->y = y + pBox->y;
        pOutline->w = pBox->w;
        pOutline->h = pBox->h;
        pOutline->pNode = pBox->pNode;
        return pOutline;
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * drawImage --
 *
 *     This function is used to draw a CANVAS_IMAGE primitive to the 
 *     drawable *pDrawable.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
drawImage(pQuery, pI2, drawable, x, y, w, h)
    GetPixmapQuery *pQuery;
    CanvasImage *pI2;
    Drawable drawable;
    int x;                 /* X-coord in *pDrawable */
    int y;                 /* Y-coord in *pDrawable */
    int w;                 /* Total width of *pDrawable */
    int h;                 /* Total height of *pDrawable */
{
    if (pI2->pImage) {
        int imW;                   /* Image width */
        int imH;                   /* Image height */

        HtmlImageSize(pI2->pImage, &imW, &imH);

        tileimage(
            pQuery, drawable, w, h, 
            pI2->pImage,
            x + pI2->x, y + pI2->y,
            imW, imH,
            x + pI2->x, y + pI2->y
        );
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * drawLine --
 *
 *     This function is used to draw a CANVAS_LINE primitive to the 
 *     drawable.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
drawLine(pQuery, pItem, drawable, x, y, w, h)
    GetPixmapQuery *pQuery;
    HtmlCanvasItem *pItem;
    Drawable drawable;
    int x;                 /* X-coord in *pDrawable */
    int y;                 /* Y-coord in *pDrawable */
    int w;                 /* Total width of *pDrawable */
    int h;                 /* Total height of *pDrawable */
{
    HtmlTree *pTree = pQuery->pTree;
    CanvasLine *pLine = &pItem->x.line;
    XColor *xcolor;
    int yrel;

    switch (HtmlNodeComputedValues(pLine->pNode)->eTextDecoration) {
        case CSS_CONST_LINE_THROUGH:
            yrel = pLine->y + pLine->y_linethrough; 
            break;
        case CSS_CONST_UNDERLINE:
            yrel = pLine->y + pLine->y_underline; 
            break;
        case CSS_CONST_OVERLINE:
            yrel = pLine->y; 
            break;
        default:
            return;
    }
    xcolor = HtmlNodeComputedValues(pLine->pNode)->cColor->xcolor;
    setClippingDrawable(pQuery, pItem, &drawable, &x, &y);
    fill_rectangle(
        pTree->tkwin, drawable, xcolor, x + pLine->x, y + yrel, pLine->w, 1
    );
}


#define SWAPINT(x,y) {int tmp = x; x = y; y = tmp;}

/*
 *---------------------------------------------------------------------------
 *
 * drawText --
 *
 *     This function draws a CANVAS_TEXT primitive on the supplied drawable.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
drawText(pQuery, pItem, drawable, x, y)
    GetPixmapQuery *pQuery;        /* Pointer to pixmap-query */
    HtmlCanvasItem *pItem;         /* Text item to draw */
    Drawable drawable;             /* Drawable to draw on */
    int x;                         /* X-coord for drawing origin */
    int y;                         /* Y-coord for drawing origin */
{
    HtmlTree *pTree = pQuery->pTree;
    Display *disp = Tk_Display(pTree->tkwin);
    CanvasText *pT = &pItem->x.t;

    GC gc = 0;
    XGCValues gc_values;
    int mask;

    CONST char *z;          /* String to render */
    int n;                  /* Length of string z in (Todo: bytes? chars?) */

    HtmlFont *pFont = pT->fFont;
    HtmlColor *pColor = colorFromNode(pT->pNode);
    Tk_Font font = pFont->tkfont;

    HtmlTaggedRegion *pTagged;
    HtmlTextNode *pText;

    z = pT->zText;
    n = pT->nText;

    /* Draw the text in the regular way (according to the stylesheet config). 
     *
     * Todo: There seems to be a bug in Tk_DrawChars triggered by
     * attempting to draw a string that lies wholly outside the drawable
     * region. So avoid this...
     *
     * Note: Have to test that pColor->xcolor is not NULL here in case
     * the 'color' property has been explicitly set to "transparant" 
     * (no kidding - http://www.economist.com).
     */ 
    if (pColor->xcolor) {
        mask = GCForeground | GCFont;
        gc_values.foreground = pColor->xcolor->pixel;
        gc_values.font = Tk_FontId(font);
        gc = Tk_GetGC(pTree->tkwin, mask, &gc_values);
        setClippingRegion(pQuery, disp, gc);
        setClippingDrawable(pQuery, pItem, &drawable, &x, &y);
        Tk_DrawChars(disp, drawable, gc, font, z, n, pT->x + x, pT->y + y);
        clearClippingRegion(disp, gc);
        Tk_FreeGC(disp, gc);
    }

    /* Now, if the associated node is a text node with one or more tags
     * applied to it, draw any tagged regions of text over the top of the
     * stylesheet text.
     *
     * TODO: Because tagged regions are drawn over the top of the unadorned
     * text (and possibly each other) Tkhtml does not support tags with
     * transparent background regions. To fix this, this function needs
     * to be rewritten to ensure each character is only drawn once. This
     * would be more efficient too, although it's not really an important
     * case.
     */
    pText = HtmlNodeAsText(pT->pNode);
    for (
        pTagged = (pText ? pText->pTagged : 0); 
        pTagged; 
        pTagged = pTagged->pNext
    ) {
        /* The tagged region of this primitive */
        int iSelFrom = MAX(0, pTagged->iFrom - pT->iIndex);
        int iSelTo = MIN(n, pTagged->iTo - pT->iIndex);
        int eContinue = (iSelTo < (pTagged->iTo - pT->iIndex));
    
        if (iSelTo > 0 && iSelFrom <= n && iSelTo >= iSelFrom) {
            CONST char *zSel = &z[iSelFrom];
            int nSel;
            int w;                  /* Pixels of tagged text */
            int xs = x;             /* Pixel offset of tagged text */
            int h;                  /* Height of text line */
            int ybg;                /* Y coord for bg rectangle */
            HtmlWidgetTag *pTag = pTagged->pTag;
    
            nSel = iSelTo - iSelFrom;
            if (iSelFrom > 0) {
                xs += Tk_TextWidth(font, z, iSelFrom);
            }
            if (eContinue) {
                w = pT->w + x - xs;
            } else {
                w = Tk_TextWidth(font, zSel, nSel);
            }
    
            h = pFont->metrics.ascent + pFont->metrics.descent;
            ybg = pT->y + y - pFont->metrics.ascent;
    
            mask = GCForeground;
            gc_values.foreground = pTag->background->pixel;
            gc = Tk_GetGC(pTree->tkwin, mask, &gc_values);
            setClippingRegion(pQuery, disp, gc);
            XFillRectangle(disp, drawable, gc, pT->x + xs, ybg, w, h);
            clearClippingRegion(disp, gc);
            Tk_FreeGC(disp, gc);
    
            mask = GCForeground | GCFont;
            gc_values.foreground = pTag->foreground->pixel;
            gc_values.font = Tk_FontId(font);
            gc = Tk_GetGC(pTree->tkwin, mask, &gc_values);
            setClippingRegion(pQuery, disp, gc);
            Tk_DrawChars(disp, drawable, gc, font, zSel, nSel,pT->x+xs,pT->y+y);
            clearClippingRegion(disp, gc);
            Tk_FreeGC(disp, gc);
        }
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * searchCanvas --
 *
 *     Iterate through a subset of the drawing primitives in the
 *     canvas associated with widget pTree. For each primitive, invoke
 *     the callback function provided as argument xFunc.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int    
searchCanvas(pTree, ymin, ymax, xFunc, clientData, requireOverflow)
    HtmlTree *pTree;
    int ymin;                    /* Minimum y coordinate, or INT_MIN */
    int ymax;                    /* Maximum y coordinate, or INT_MAX */
    int (*xFunc)(HtmlCanvasItem *, int, int, Overflow *, ClientData);
    ClientData clientData;
    int requireOverflow;         /* Boolean. True to pass Overflow* arg */
{
    HtmlCanvasItem *pItem;
    HtmlCanvasItem *pSkip = 0;
    HtmlCanvas *pCanvas = &pTree->canvas;
    int origin_x = 0;
    int origin_y = 0;
    int rc = 0;
    int nTest = 0;
    int nCallback = 0;

    /* The overflow stack. Grown using HtmlRealloc(). */
    Overflow **apOverflow = 0;
    int nOverflow = 0;
    int iOverflow = -1;

    /* Debugging variables to support assert() statements */
    int nOrigin = 0;
    int bSeenFixedMarker = 0;
     
    for (pItem = pCanvas->pFirst; pItem; pItem = (pSkip?pSkip:pItem->pNext)) {

        pSkip = 0;
        switch (pItem->type) {
            case CANVAS_ORIGIN: {
                CanvasOrigin *pOrigin1 = &pItem->x.o;
                CanvasOrigin *pOrigin2 = 0;
                int ymin2 = ymin;
                int ymax2 = ymax;
                if (pOrigin1->pSkip) pOrigin2 = &pItem->x.o.pSkip->x.o;

                nOrigin += (pOrigin2?1:-1);

                if (iOverflow >= 0) {
                    ymin2 += apOverflow[iOverflow]->yscroll;
                    ymax2 += apOverflow[iOverflow]->yscroll;
                }
    
                origin_x += pOrigin1->x;
                origin_y += pOrigin1->y;
                if (pOrigin2 && (
                    (ymax >= 0 && (origin_y + pOrigin1->vertical) > ymax2) ||
                    (ymin >= 0 && (origin_y + pOrigin2->vertical) < ymin2))
                ) {
                   pSkip = pOrigin1->pSkip;
                }
                break;
            }

            case CANVAS_MARKER: {
                if(pItem->x.marker.flags == MARKER_FIXED){
                    assert(bSeenFixedMarker == 0);
                    assert(nOrigin == 0);
                    assert(origin_x == 0);
                    assert(origin_y == 0);
                    origin_x = pTree->iScrollX;
                    origin_y = pTree->iScrollY;
                    bSeenFixedMarker = 1;
                }
                break;
            }

            case CANVAS_OVERFLOW: {
                if (requireOverflow) {
                    Overflow *pOverflow = (Overflow *)&pItem[1];
                    HtmlNode *pNode = pItem->x.overflow.pNode;
    
                    HtmlElementNode *pElem = (HtmlElementNode *)pNode;
                    assert(!HtmlNodeIsText(pNode));
    
                    iOverflow++;
                    assert(iOverflow <= nOverflow);
                    if (iOverflow == nOverflow) {
                        int n = sizeof(Overflow *) * (nOverflow + 1);
                        apOverflow = (Overflow**)HtmlRealloc(0, apOverflow, n);
                        nOverflow++;
                    }
                    apOverflow[iOverflow] = pOverflow;
                    pOverflow->pItem = &pItem->x.overflow;
                    pOverflow->x = pItem->x.overflow.x + origin_x;
                    pOverflow->y = pItem->x.overflow.y + origin_y;
                    pOverflow->w = pItem->x.overflow.w;
                    pOverflow->h = pItem->x.overflow.h;
                    pOverflow->pixmap = 0;
                    pOverflow->pNext = 0;
    
                    /* Adjust the x and y coords for scrollable blocks: */
                    pOverflow->xscroll = 0;
                    pOverflow->yscroll = 0;
                    if (pElem->pScrollbar) {
                        pOverflow->xscroll = pElem->pScrollbar->iHorizontal;
                        pOverflow->yscroll = pElem->pScrollbar->iVertical;
                    }
                }
           
                break;
            }
           
            default: {
                Overflow *pOver = 0;
                nTest++;

                if (ymax >= 0 || ymin >= 0) {
                    int x, y, w, h;
                    int ymin2 = ymin;
                    int ymax2 = ymax;
                    itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);
                    if (iOverflow >= 0) {
                        ymin2 += apOverflow[iOverflow]->yscroll;
                        ymax2 += apOverflow[iOverflow]->yscroll;
                    }
                    if ((ymax >= 0 && y >= ymax2) || 
                        (ymin >= 0 && (y+h) <= ymin2)
                    ) {
                       break;
                    }
                }

                if (iOverflow >= 0) {
                    pOver = apOverflow[iOverflow];
                }
                rc = xFunc(pItem, origin_x, origin_y, pOver, clientData);
                if (0 != rc) {
                    goto search_out;
                }
                nCallback++;
                break;
            }
        }

        /* Check if we are supposed to pop the overflow stack */
        while (iOverflow >= 0 && pItem == apOverflow[iOverflow]->pItem->pEnd) {
            iOverflow--;
        }
    }

#if 0
printf("Search(%d, %d) -> %d tests %d callbacks\n",ymin,ymax,nTest,nCallback);
#endif
 
search_out:
    HtmlFree(apOverflow);
    return rc;
}

static int
sorterCb(pItem, x, y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int x;
    int y;
    Overflow *pOverflow;
    ClientData clientData;
{
    CanvasItemSorter *pSorter = (CanvasItemSorter *)clientData;

    /* Only visible items are added to the sorter. */
    if (pItem->type == CANVAS_BOX) {
        HtmlComputedValues *p = HtmlNodeComputedValues(pItem->x.box.pNode);
        if (
            (p->eBorderTopStyle == CSS_CONST_NONE || !p->border.iTop) && 
            (p->eBorderBottomStyle == CSS_CONST_NONE || !p->border.iBottom) && 
            (p->eBorderRightStyle == CSS_CONST_NONE || !p->border.iRight) && 
            (p->eBorderLeftStyle == CSS_CONST_NONE || !p->border.iLeft) &&
            (p->eOutlineStyle == CSS_CONST_NONE || !p->iOutlineWidth) &&
            (!p->imBackgroundImage) && 
            (!p->cBackgroundColor || !p->cBackgroundColor->xcolor)
        ) {
            return 0;
        }
    }
    if (pItem->type == CANVAS_LINE) {
        HtmlComputedValues *p = HtmlNodeComputedValues(pItem->x.box.pNode);
        if (p->eTextDecoration == CSS_CONST_NONE) {
            return 0;
        }
    }

    if (pSorter->iSnapshot) {
        /* If CanvasItemSorter.iSnapshot is not zero, then we are creating
         * a snap-shot for HtmlDrawSnapshot().
         */
        pItem->iSnapshot = pSorter->iSnapshot;
        if (pItem->type == CANVAS_BOX) {
            x += pItem->x.box.x;
            y += pItem->x.box.y;
        }
        pItem->nRef++;
        assert(pItem->nRef >= 2);
    } 

    sorterInsert(pSorter, pItem, x, y, pOverflow);
    return 0;
}
static void    
searchSortedCanvas(pTree, ymin, ymax, pNode, xFunc, clientData)
    HtmlTree *pTree;
    int ymin;                    /* Minimum y coordinate, or INT_MIN */
    int ymax;                    /* Maximum y coordinate, or INT_MAX */
    HtmlNode *pNode;             /* Node to search subtree of, or NULL */
    int (*xFunc)(HtmlCanvasItem *, int, int, Overflow *, ClientData);
    ClientData clientData;
{
    CanvasItemSorter sSorter;
    memset(&sSorter, 0, sizeof(CanvasItemSorter));

    searchCanvas(pTree, ymin, ymax, sorterCb, (ClientData)&sSorter, 1);
    sorterIterate(&sSorter, xFunc, clientData);
    sorterReset(&sSorter);
}


static int
snapshotReleaseItemsCb(pItem, x, y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int x;
    int y;
    Overflow *pOverflow;
    ClientData clientData;
{
    freeCanvasItem((HtmlTree *)clientData, pItem);
    return 0;
}

HtmlCanvasSnapshot *
HtmlDrawSnapshot(pTree, isDrawable)
    HtmlTree *pTree;
    int isDrawable;
{
    int ymin = pTree->iScrollY;
    int ymax = ymin + Tk_Height(pTree->tkwin);
    CanvasItemSorter *p;

    p = HtmlNew(CanvasItemSorter);
    p->iSnapshot = (++pTree->iLastSnapshotId);
    searchCanvas(pTree, ymin, ymax, sorterCb, (ClientData)p, 1);

    return (HtmlCanvasSnapshot *)p;
}

static CanvasItemSorterSlot *nextItem(pSorter, piLevel, piItem)
    CanvasItemSorter *pSorter;
    int *piLevel;
    int *piItem;
{
    CanvasItemSorterSlot *pRet = 0;
    int iLevel = *piLevel;
    int iItem = *piItem;
    while (iLevel < pSorter->nLevel && !pRet) {
        if (iItem >= pSorter->aLevel[iLevel].iSlot) {
            iItem = 0;
            iLevel++;
        } else {
            pRet = &pSorter->aLevel[iLevel].aSlot[iItem];
            iItem++;
        }
    }
    *piLevel = iLevel;
    *piItem = iItem;
    return pRet;
}

static void damageSlot(pTree, pSlot, pX1, pY1, pX2, pY2, isOld)
    HtmlTree *pTree;
    CanvasItemSorterSlot *pSlot;
    int *pX1;
    int *pY1;
    int *pX2;
    int *pY2;
    int isOld;
{
    int x;
    int y;
    int h;
    int w;
    itemToBox(pSlot->pItem, pSlot->x, pSlot->y, &x, &y, &w, &h);
    if (isOld && pSlot->pItem->type == CANVAS_BOX) {
        x -= pSlot->pItem->x.box.x;
        y -= pSlot->pItem->x.box.y;
    }
/* printf("%dx%d +%d+%d (%d)\n", w, h, x, y, pSlot->pItem->type); */
    if (pSlot->pItem->type == CANVAS_WINDOW) {
        pSlot->pItem->x.w.pElem->pReplacement->iCanvasX = -10000;
        pSlot->pItem->x.w.pElem->pReplacement->iCanvasY = -10000;
    }
    *pX1 = MIN(*pX1, x);
    *pY1 = MIN(*pY1, y);
    *pX2 = MAX(*pX2, x+w);
    *pY2 = MAX(*pY2, y+h);
}

static int itemsAreEqual(p1, p2)
    HtmlCanvasItem *p1;
    HtmlCanvasItem *p2;
{
    if (p1 == p2) return 1;
    if (p1->type == p2->type) {
        switch (p1->type) {
            case CANVAS_TEXT: {
                return (
                    p1->x.t.x == p2->x.t.x &&
                    p1->x.t.y == p2->x.t.y &&
                    p1->x.t.w == p2->x.t.w &&
                    p1->x.t.fFont == p2->x.t.fFont &&
                    p1->x.t.zText == p2->x.t.zText &&
                    p1->x.t.nText == p2->x.t.nText
                );
            }
            case CANVAS_LINE: {
                return (
                    p1->x.line.x == p2->x.line.x &&
                    p1->x.line.y == p2->x.line.y &&
                    p1->x.line.w == p2->x.line.w &&
                    p1->x.line.y_underline == p2->x.line.y_underline &&
                    p1->x.line.y_linethrough == p2->x.line.y_linethrough
                );
            }
            case CANVAS_IMAGE: {
                return (
                    p1->x.i2.x == p2->x.i2.x &&
                    p1->x.i2.y == p2->x.i2.y &&
                    p1->x.i2.w == p2->x.i2.w &&
                    p1->x.i2.h == p2->x.i2.h &&
                    p1->x.i2.pImage == p2->x.i2.pImage
                );
            }
        }
    }
    return 0;
}

HtmlCanvasSnapshot *
HtmlDrawSnapshotZero(pTree)
    HtmlTree *pTree;
{
    return (HtmlCanvasSnapshot *)HtmlNew(CanvasItemSorter);
}

void
HtmlDrawSnapshotDamage(pTree, pSnapshot, ppCurrent)
    HtmlTree *pTree;
    HtmlCanvasSnapshot *pSnapshot;
    HtmlCanvasSnapshot **ppCurrent;
{
    CanvasItemSorter *pOld = (CanvasItemSorter *)pSnapshot;
    CanvasItemSorter *pNew;

    int ymin = pTree->iScrollY;
    int ymax = ymin + Tk_Height(pTree->tkwin);

    /* Two CanvasItemSorter iterator states (each pair of variables 
     * is the state for a single iterator).
     */
    int iNewLevel = 0; int iNewItem = 0;
    int iOldLevel = 0; int iOldItem = 0;

    int iCreated = 0;
    int iDeleted = 0;
    int iDirty = 0;
    int iMoved = 0;
    int iStuck = 0;

    int x1 = 100000; 
    int y1 = 100000; 
    int x2 = 0;
    int y2 = 0;

    CanvasItemSorterSlot *pNewSlot;
    CanvasItemSorterSlot *pOldSlot;

    /* Create a new current snapshot. */
    pNew = HtmlNew(CanvasItemSorter);
    searchCanvas(pTree, ymin, ymax, sorterCb, (ClientData)pNew, 1);

    pNewSlot = nextItem(pNew, &iNewLevel, &iNewItem);
    pOldSlot = nextItem(pOld, &iOldLevel, &iOldItem);

    while (pNewSlot && pOldSlot) {
        if (itemsAreEqual(pNewSlot->pItem, pOldSlot->pItem)) {
            int newx = pNewSlot->x;
            int newy = pNewSlot->y;
            if (pNewSlot->pItem->type == CANVAS_BOX) {
                newx += pNewSlot->pItem->x.box.x;
                newy += pNewSlot->pItem->x.box.y;
            }
            if (newx != pOldSlot->x || newy != pOldSlot->y) {
                damageSlot(pTree, pOldSlot, &x1, &y1, &x2, &y2, 1);
                damageSlot(pTree, pNewSlot, &x1, &y1, &x2, &y2, 0);
                iMoved++;
            } else {
                HtmlNode *pNode = itemToNode(pNewSlot->pItem);
                if (pNode && pNode->iSnapshot == pOld->iSnapshot) {
                    damageSlot(pTree, pNewSlot, &x1, &y1, &x2, &y2, 0);
                    iDirty++;
                } else {
                    iStuck++;
                }
            }
            pOldSlot = nextItem(pOld, &iOldLevel, &iOldItem);
            pNewSlot = nextItem(pNew, &iNewLevel, &iNewItem);
        } else if (pNewSlot->pItem->iSnapshot == pOld->iSnapshot) {
            damageSlot(pTree, pOldSlot, &x1, &y1, &x2, &y2, 1);
            iDeleted++;
            pOldSlot = nextItem(pOld, &iOldLevel, &iOldItem);
        } else {
            damageSlot(pTree, pNewSlot, &x1, &y1, &x2, &y2, 0);
            iCreated++;
            pNewSlot = nextItem(pNew, &iNewLevel, &iNewItem);
        }
    }

    while (pNewSlot) {
        damageSlot(pTree, pNewSlot, &x1, &y1, &x2, &y2, 0);
        iCreated++;
        pNewSlot = nextItem(pNew, &iNewLevel, &iNewItem);
    }
    while (pOldSlot) {
        damageSlot(pTree, pOldSlot, &x1, &y1, &x2, &y2, 1);
        iDeleted++;
        pOldSlot = nextItem(pOld, &iOldLevel, &iOldItem);
    }

    if (x1<x2 && y1<y2) {
        int x = x1 - pTree->iScrollX - 1;
        int y = y1 - pTree->iScrollY - 1;
        HtmlCallbackDamage(pTree, x, y, 1+x2-x1, 1+y2-y1);
    }

    if (ppCurrent) {
        *ppCurrent = (HtmlCanvasSnapshot *)pNew;
    } else {
        sorterReset(pNew);
        HtmlFree(pNew);
    }
}

void
HtmlDrawSnapshotFree(pTree, pSnapshot)
    HtmlTree *pTree;
    HtmlCanvasSnapshot *pSnapshot;
{
    if (pSnapshot) {
        CanvasItemSorter *p = (CanvasItemSorter *)pSnapshot;
        if (p->iSnapshot) {
            sorterIterate(p, snapshotReleaseItemsCb, (ClientData)pTree);
        }
        sorterReset(p);
        HtmlFree(pSnapshot);
    }
}

static void
clipRectangle(pX, pY, pW, pH, x2, y2, w2, h2)
    int *pX;
    int *pY;
    int *pW;
    int *pH;
    int x2;
    int y2;
    int w2;
    int h2;
{
    int x1 = *pX;
    int y1 = *pY;
    int w1 = *pW;
    int h1 = *pH;

    *pX = MAX(x1, x2);
    *pY = MAX(y1, y2);
    *pW = MIN((x1+w1) - *pX, (x2+w2) - *pX);
    *pH = MIN((y1+h1) - *pY, (y2+h2) - *pY);
}

static void
pixmapQuerySwitchOverflow(pQuery, pOverflow)
    GetPixmapQuery *pQuery;
    Overflow *pOverflow;
{
    if (pOverflow != pQuery->pCurrentOverflow) {
        Overflow *pCurrentOverflow = pQuery->pCurrentOverflow;

#if 0
        if (pQuery->pCurrentOverflow) {
            printf("Clipping region was: %dx%d +%d+%d\n", 
                pQuery->pCurrentOverflow->pItem->w, 
                pQuery->pCurrentOverflow->pItem->h, 
                pQuery->pCurrentOverflow->x, 
                pQuery->pCurrentOverflow->y
            );
        }else{
            printf("Clipping region was: (null)\n");
        }
        if (pOverflow) {
            printf("Clipping region is: %dx%d +%d+%d\n", 
                pOverflow->pItem->w, pOverflow->pItem->h, 
                pOverflow->x, pOverflow->y
            );
        }else{
            printf("Clipping region is: (null)\n");
        }
#endif

        /* If there is a pixmap associated with the current Overflow object,
         * copy it to the output pixmap now (GetPixmapQuery.pmap)
         */
        if (pCurrentOverflow && pCurrentOverflow->pixmap) {
            int src_x = 0;
            int src_y = 0;
            int dest_x = pCurrentOverflow->pmx - pQuery->x;
            int dest_y = pCurrentOverflow->pmy - pQuery->y;
            int copy_w = pCurrentOverflow->pmw;
            int copy_h = pCurrentOverflow->pmh;
            if (copy_w > 0 && copy_h > 0) {
                Tk_Window win = pQuery->pTree->tkwin;
                Pixmap o = pCurrentOverflow->pixmap;
                GC gc;
                XGCValues gc_values;
                memset(&gc_values, 0, sizeof(XGCValues));
                gc = Tk_GetGC(pQuery->pTree->tkwin, 0, &gc_values);
                assert(src_x >= 0 && src_y >= 0);
                assert(dest_x >= 0 && dest_y >= 0);
                XCopyArea(Tk_Display(win), o, pQuery->pmap, gc, 
                    src_x, src_y, copy_w, copy_h, dest_x, dest_y
                );
                Tk_FreeGC(Tk_Display(win), gc);
            }
        }

        pQuery->pCurrentOverflow = 0;

        if (pOverflow && pOverflow->w > 0 && pOverflow->h > 0) {
            pOverflow->pmx = pOverflow->x;
            pOverflow->pmy = pOverflow->y;
            pOverflow->pmw = pOverflow->w;
            pOverflow->pmh = pOverflow->h;
            clipRectangle(
                &pOverflow->pmx, &pOverflow->pmy, 
                &pOverflow->pmw, &pOverflow->pmh, 
                pQuery->x, pQuery->y, pQuery->w, pQuery->h
            );
        }

        pQuery->pCurrentOverflow = pOverflow;
    }
}

static int
pixmapQueryCb(pItem, origin_x, origin_y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    Overflow *pOverflow;
    ClientData clientData;
{
    GetPixmapQuery *pQuery = (GetPixmapQuery *)clientData;

    /* Set (x, y) to the coordinates of the top-left of the queried
     * canvas region, in the coordinate system used by the primitive 
     * pItem. Variables w and h describe the width and height of the
     * queried region.
     *
     * These values are modified below if required to account for clipping 
     * or scrolling by the overflow primitive pOverflow.
     */
    int x = origin_x - pQuery->x;
    int y = origin_y - pQuery->y;
    int w = pQuery->w;
    int h = pQuery->h;
    Drawable drawable = pQuery->pmap;

    /* If the node's item has the 'visibility' property set to "hidden" or
     * "collapse", do not draw any content.
     */
    HtmlComputedValues *pComputed = HtmlNodeComputedValues(itemToNode(pItem));
    assert(pItem->type != CANVAS_ORIGIN && pItem->type != CANVAS_MARKER);
    if (pComputed->eVisibility != CSS_CONST_VISIBLE) {
        return 0;
    }

    pixmapQuerySwitchOverflow(pQuery, pOverflow);
    assert(!pQuery->pCurrentOverflow || pOverflow == pQuery->pCurrentOverflow);
    if (pQuery->pCurrentOverflow) {
        Overflow *p = pQuery->pCurrentOverflow;
        if (p->pmw <= 0 || p->pmh <= 0) {
            return 0;
        }
        if (p->pixmap) {
            drawable = p->pixmap;
            x = origin_x - p->pmx;
            y = origin_y - p->pmy;
            w = p->pmw;
            h = p->pmh;
        }
        x -= p->xscroll;
        y -= p->yscroll;
    }

    switch (pItem->type) {
        case CANVAS_TEXT: {
            drawText(pQuery, pItem, drawable, x, y);
            break;
        }

        case CANVAS_IMAGE: {
            setClippingDrawable(pQuery, pItem, &drawable, &x, &y);
            drawImage(pQuery, &pItem->x.i2, drawable, x, y, w, h);
            break;
        }

        case CANVAS_BOX: {
            Outline *p;
            int xv = pQuery->x - pQuery->pTree->iScrollX;
            int yv = pQuery->y - pQuery->pTree->iScrollY;
            int f = 0;
            if (pQuery->pBgRoot == pItem->x.box.pNode) f = DRAWBOX_NOBACKGROUND;
            p = drawBox(pQuery, pItem, &pItem->x.box,drawable,x,y,w,h,xv,yv,f);
            if (p) {
                p->pNext = pQuery->pOutline;
                pQuery->pOutline = p;
            }
            if (pQuery->getwin) {
                drawScrollbars(pQuery->pTree, pItem, origin_x, origin_y);
            }
            break;
        }

        case CANVAS_LINE: {
            drawLine(pQuery, pItem, drawable, x, y, w, h);
            break;
        }
        case CANVAS_WINDOW: {
            if (pQuery->getwin) {
                HtmlTree *pTree = pQuery->pTree;
                HtmlNodeReplacement *pRep = pItem->x.w.pElem->pReplacement;
                HtmlNodeReplacement *p;

                pRep->iCanvasX = origin_x + pItem->x.w.x;
                pRep->iCanvasY = origin_y + pItem->x.w.y;
                pRep->iWidth   = pItem->x.w.iWidth;
                pRep->iHeight  = pItem->x.w.iHeight;
                pRep->clipped = 0;

                if (pQuery->pCurrentOverflow) {
                    Overflow *pOver = pQuery->pCurrentOverflow;

                    /* Adjust for the current scroll position */
                    pRep->iCanvasX -= pOver->xscroll;
                    pRep->iCanvasY -= pOver->yscroll;

                    /* Vertical clipping */
                    if (pRep->iCanvasY < pOver->y) {
                        pRep->iHeight -= (pOver->y - pRep->iCanvasY);
                        pRep->iCanvasY = pOver->y;
                    }
                    if (pRep->iCanvasY + pRep->iHeight > pOver->y + pOver->h) {
                        pRep->iHeight = pOver->y + pOver->h - pRep->iCanvasY;
                    }

                    /* Horizontal clipping */
                    if (pRep->iCanvasX < pOver->x) {
                        pRep->iWidth -= (pOver->x - pRep->iCanvasX);
                        pRep->iCanvasX = pOver->x;
                    }
                    if (pRep->iCanvasX + pRep->iWidth > pOver->x + pOver->w) {
                        pRep->iWidth = pOver->x + pOver->w - pRep->iCanvasX;
                    }
                }

                for (p = pTree->pMapped; p && p != pRep; p = p->pNext);
                if (!p) {
                    pRep->pNext = pTree->pMapped;
                    pTree->pMapped = pRep;
                }
            }
            break;
        }
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * getPixmap --
 *
 *    Return a Pixmap containing the rendered document. The caller is
 *    responsible for calling Tk_FreePixmap() on the returned value.
 *
 *    This is the function that actually does the drawing using X11 
 *    drawing primitives.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Pixmap 
getPixmap(pTree, xcanvas, ycanvas, w, h, getwin)
    HtmlTree *pTree;        /* Pointer to html widget */
    int xcanvas;            /* top-left canvas x-coord of requested pixmap */
    int ycanvas;            /* top-left canvas y-coord of requested pixmap */
    int w;                  /* Required width of pixmap */
    int h;                  /* Required height of pixmap */
    int getwin;             /* Boolean. True to add windows to pTree->pMapped */
{
    Pixmap pmap;
    Display *pDisplay;
    Tk_Window win = pTree->tkwin;
    XColor *bg_color = 0;
    GetPixmapQuery sQuery;
    Outline *pOutline;
    Overflow *pOverflow;
    ClientData clientData;
    HtmlNode *pBgRoot;

    Tk_MakeWindowExist(win);
    pDisplay = Tk_Display(win);
    pmap = Tk_GetPixmap(pDisplay, Tk_WindowId(win), w, h, Tk_Depth(win));

    /* Determine which tree node (if any) determines the background
     * color and image of the entire canvas.
     */
    pBgRoot = pTree->pRoot;
    if (pBgRoot) {
        HtmlComputedValues *pV = HtmlNodeComputedValues(pBgRoot);
        if (!pV->cBackgroundColor->xcolor && !pV->imZoomedBackgroundImage) {
            pBgRoot = HtmlNodeChild(pBgRoot, 1);
        }
        pV = HtmlNodeComputedValues(pBgRoot);
        if (!pV->cBackgroundColor->xcolor && !pV->imZoomedBackgroundImage) {
            pBgRoot = 0;
        }
    }

    if (
        !pBgRoot || 
        !HtmlNodeComputedValues(pBgRoot)->cBackgroundColor->xcolor
    ) {
        Tcl_HashEntry *pEntry;
        pEntry = Tcl_FindHashEntry(&pTree->aColor, "white");
        assert(pEntry);
        bg_color = ((HtmlColor *)Tcl_GetHashValue(pEntry))->xcolor;
        fill_rectangle(win, pmap, bg_color, 0, 0, w, h);
    }

    sQuery.pTree = pTree;
    sQuery.pBgRoot = pBgRoot;
    sQuery.pmap = pmap;
    sQuery.x = xcanvas;
    sQuery.y = ycanvas;
    sQuery.w = w;
    sQuery.h = h;
    sQuery.pOutline = 0;
    sQuery.getwin = getwin;
    sQuery.pCurrentOverflow = 0;
    sQuery.pOverflowList = 0;

    if (pBgRoot) {
        CanvasBox sBox;
        int xv = xcanvas - pTree->iScrollX;
        int yv = ycanvas - pTree->iScrollY;
        memset(&sBox, 0, sizeof(CanvasBox));
        sBox.pNode = pBgRoot;
        sBox.w = MAX(Tk_Width(pTree->tkwin), pTree->canvas.right);
        sBox.h = MAX(Tk_Height(pTree->tkwin), pTree->canvas.bottom);
        drawBox(
            &sQuery, 0, &sBox, pmap, -1*xcanvas, -1*ycanvas, 
            w, h, xv, yv, DRAWBOX_NOBORDER
        );
    } 

    clientData = (ClientData)&sQuery;
#if 0
    searchCanvas(pTree, ycanvas, ycanvas+h, 0, pixmapQueryCb, clientData);
#else
    if (pTree->cb.pSnapshot) {
        CanvasItemSorter *pSorter = (CanvasItemSorter *)pTree->cb.pSnapshot;
        sorterIterate(pSorter, pixmapQueryCb, clientData);
    }else{
        searchSortedCanvas(pTree, ycanvas, ycanvas+h, 0, pixmapQueryCb, clientData);
    }
#endif
    pixmapQuerySwitchOverflow(&sQuery, 0);
    for (
        pOverflow = sQuery.pOverflowList;  
        pOverflow; 
        pOverflow = pOverflow->pNext
    ) {
        Tk_FreePixmap(Tk_Display(win), pOverflow->pixmap);
        pOverflow->pixmap = 0;
    }

    pOutline = sQuery.pOutline;
    while (pOutline) {
        HtmlComputedValues *pComputed = HtmlNodeComputedValues(pOutline->pNode);
        int ow = pComputed->iOutlineWidth;
        XColor *oc = pComputed->cOutlineColor->xcolor;
        int x1 = pOutline->x;
        int y1 = pOutline->y;
        int w1 = pOutline->w;
        int h1 = pOutline->h;
        Outline *pPrev = pOutline;
        fill_quad(0, pTree->tkwin, pmap, oc, x1,y1, w1,0, 0,ow, -w1,0);
        fill_quad(0, pTree->tkwin, pmap, oc, x1,y1+h1, w1,0, 0,-ow, -w1,0);
        fill_quad(0, pTree->tkwin, pmap, oc, x1,y1, 0,h1, ow,0, 0,-h1);
        fill_quad(0, pTree->tkwin, pmap, oc, x1+w1,y1, 0,h1, -ow,0, 0,-h1);
        pOutline = pOutline->pNext;
        HtmlFree(pPrev);
    }
  
    return pmap;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutImage --
 *
 *     <widget> image
 * 
 *     Render the document to a Tk image and return the name of the image
 *     as the Tcl result. The calling script is responsible for deleting
 *     the image. The image has blank space where controls would be mapped
 *     in a live display.
 *
 * Results:
 *     Standard Tcl return code.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlLayoutImage(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Display *pDisplay = Tk_Display(pTree->tkwin);

    int x = 0;
    int y = 0;
    int w;
    int h;

    /* Force any pending style and/or layout operations to run. */
    HtmlCallbackForce(pTree);

    w = pTree->canvas.right;
    h = pTree->canvas.bottom;
    Tk_MakeWindowExist(pTree->tkwin);
    w = Tk_Width(pTree->tkwin);
    h = Tk_Height(pTree->tkwin);
    assert(w >= 0 && h >= 0);
    if (w>0 && h>0) {
        Pixmap pixmap;
        Tcl_Obj *pImage;
        XImage *pXImage;
        pixmap = getPixmap(pTree, pTree->iScrollX, pTree->iScrollY, w, h, 0);
        pXImage = XGetImage(pDisplay, pixmap, x, y, w, h, AllPlanes, ZPixmap);
        pImage = HtmlXImageToImage(pTree, pXImage, w, h);
        XDestroyImage(pXImage);
        Tcl_SetObjResult(interp, pImage);
        Tcl_DecrRefCount(pImage);
        Tk_FreePixmap(Tk_Display(pTree->tkwin), pixmap);
    } else {
        /* If the width or height is zero, then the image is empty. So just
	 * run the following simple script to set the interpreter result to
	 * an empty image.
         */
        Tcl_Eval(interp, "image create photo");
    }
  
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlDrawIsEmpty --
 *
 *     Return true if the canvas object pCanvas is empty - i.e. contains no
 *     canvas items.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlDrawIsEmpty(pCanvas)
    HtmlCanvas *pCanvas;
{
    return (pCanvas->left==pCanvas->right && pCanvas->top==pCanvas->bottom);
}

static HtmlNode *
findFlowNode(pNode)
    HtmlNode *pNode;
{
    HtmlNode *p;
    for (p = pNode; p; p = HtmlNodeParent(p)) {
        HtmlComputedValues *pV = HtmlNodeComputedValues(p);
        if (pV && (
                pV->eDisplay == CSS_CONST_TABLE_CELL ||
                pV->eFloat != CSS_CONST_NONE ||
                pV->ePosition != CSS_CONST_STATIC
            )
        ) {
            break;
        }
    }
    return p;
}

typedef struct NodeIndexQuery NodeIndexQuery;
struct NodeIndexQuery {
    int x;
    int y;
    CanvasText *pClosest;
    int closest_x;
    HtmlNode *pFlow;
};

/*
 *---------------------------------------------------------------------------
 *
 * layoutNodeIndexCb --
 *
 *     The searchCanvas() callback used by the implementation of the 
 *     [$html node -index X Y] command.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
layoutNodeIndexCb(pItem, origin_x, origin_y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    Overflow *pOverflow;
    ClientData clientData;
{
    CanvasText *pT = &pItem->x.t;
    if (pItem->type == CANVAS_TEXT && pT->iIndex >= 0 && pT->pNode->iNode >= 0){
        NodeIndexQuery *pQuery = (NodeIndexQuery *)clientData;

        /* Calculate the bounding-box of the item. Store the coordinates 
         * of the top-left corner in variables x and y, and the size of
         * the box in variables w and h.
         */
        int x, y, w, h;
        itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);

        if (pOverflow) {
            x -= pOverflow->xscroll;
            y -= pOverflow->yscroll;
            /* TODO: Clipping */
        }

        /* If our point is actually inside the bounding box of this
         * text item, then this item is returned as the "closest text".
         */
        if (
            pQuery->x >= x && pQuery->x <= (x + w) &&
            pQuery->y >= y && pQuery->y <= (y + h)
        ) {
            pQuery->pClosest = pT;
            pQuery->closest_x = x;
            return 1;
        }
        
        /* If a text item for which the bounding box encapsulates the 
         * search point cannot be found, then we are looking for the
         * closest text item that is "above" the search point.
         */
        else {
            if (
                y <= pQuery->y && (
                    (x <= pQuery->x && pQuery->x <= (x + w)) ||  
                    (x <= pQuery->x && pQuery->y < (y + h) &&
                         x > pQuery->closest_x
                    ) ||
                    (pQuery->pFlow == findFlowNode(pT->pNode))
                )
            ) {
                pQuery->pFlow = findFlowNode(pT->pNode);
                pQuery->pClosest = pT;
                pQuery->closest_x = x;
            }
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * layoutNodeIndexCmd --
 *
 *     This function is called to process a command of the form:
 *
 *         <widget> node -index X Y
 *
 * Results:
 *     No results returned.
 *
 * Side effects:
 *     The tcl interpreter HtmlTree.interp is loaded with the result of
 *     the [<widget> node -index X Y] command.
 *
 *---------------------------------------------------------------------------
 */
static void
layoutNodeIndexCmd(pTree, x, y)
    HtmlTree *pTree;        /* Widget tree */
    int x;                  /* Document (not viewport) X coordinate */
    int y;                  /* Document (not viewport) Y coordinate */
{
    NodeIndexQuery sQuery;
    ClientData cd = (ClientData)&sQuery;
    int rc;

    memset(&sQuery, 0, sizeof(NodeIndexQuery));
    sQuery.x = x;
    sQuery.y = y;

    rc = searchCanvas(pTree, y-100, y, layoutNodeIndexCb, cd, 0);
    if (!sQuery.pClosest) {
        int ymin = y - pTree->iScrollY;
        rc = searchCanvas(pTree, ymin, y, layoutNodeIndexCb, cd, 0);
    }
    if (!sQuery.pClosest) {
        rc = searchCanvas(pTree, -1, y, layoutNodeIndexCb, cd, 0);
    }

    if (sQuery.pClosest) {
        HtmlNode *pNode = sQuery.pClosest->pNode;     /* Node to return */
        int iIndex = 0;                               /* Index to return */
        const char *z;
        int n;
        Tcl_Obj *pCmd;

        z = sQuery.pClosest->zText;
        n = sQuery.pClosest->nText;

        iIndex = n;
        if (rc) {
            /* Calculate the index to return */
            int dum;
            Tk_Font font = fontFromNode(sQuery.pClosest->pNode)->tkfont;
            iIndex = Tk_MeasureChars(font, z, n, x - sQuery.closest_x, 0, &dum);
        }
        iIndex += sQuery.pClosest->iIndex;

        /* Load the result into the Tcl interpreter */
        pCmd = Tcl_DuplicateObj(HtmlNodeCommand(pTree, pNode));
        Tcl_ListObjAppendElement(0, pCmd, Tcl_NewIntObj(iIndex));
        Tcl_SetObjResult(pTree->interp, pCmd);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * returnDescNode --
 *
 *     Arguments pNode1 and pNode2 are two document nodes. If one of the 
 *     nodes is a descendant of another (is part of the subtree rooted 
 *     at the other node), then return the pointer to the descendant node.
 *     Otherwise return NULL.
 *
 *    
 * Results:
 *     See above.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static HtmlNode *
returnDescNode(pNode1, pNode2)
    HtmlNode *pNode1;
    HtmlNode *pNode2;
{
    HtmlNode *pN;
    for (pN = pNode1; pN && pN != pNode2; pN = HtmlNodeParent(pN));
    if (pN) {
        return pNode1;
    }
    for (pN = pNode2; pN && pN != pNode1; pN = HtmlNodeParent(pN));
    if (pN) {
        return pNode2;
    }
    return 0;
}

typedef struct NodeQuery NodeQuery;
struct NodeQuery {
    /* Query parameters */
    int x;
    int y;

    /* Variables for building up the result set in */
    HtmlNode **apNode;
    int nNodeAlloc;
    int nNode;
};

static int
layoutNodeCb(pItem, origin_x, origin_y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    Overflow *pOverflow;
    ClientData clientData;
{
    int x, y, w, h;
    NodeQuery *pQuery = (NodeQuery *)clientData;
    HtmlNode *pNode;

    pNode = itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);
    if (pOverflow) {
        x -= pOverflow->xscroll;
        y -= pOverflow->yscroll;
    }

    /* If the query point is clipped by the overflow region, do
     * not include this node in the set returned by [pathName node].
     */
    if (pOverflow && (
        pQuery->x < pOverflow->x || pQuery->x > (pOverflow->x + pOverflow->w) ||
        pQuery->y < pOverflow->y || pQuery->y > (pOverflow->y + pOverflow->h)
    )) {
        return 0;
    }

    if (
        pNode && pNode->iNode >= 0 && 
        x <= pQuery->x && (x + w) >= pQuery->x &&
        y <= pQuery->y && (y + h) >= pQuery->y &&
        !HtmlNodeIsOrphan(pNode)
    ) {
        int i;

        /* If the applicable visibility property is set to "hidden", do
         * not include this node in the set returned by [pathName node].
         */
        HtmlComputedValues *pComputed = HtmlNodeComputedValues(pNode);
        if (pComputed==0 || pComputed->eVisibility != CSS_CONST_VISIBLE) {
            return 0;
        }

        for (i = 0; i < pQuery->nNode; i++) {
            HtmlNode *pDesc = returnDescNode(pNode, pQuery->apNode[i]);
            if (pDesc) {
                pQuery->apNode[i] = pDesc;
                return 0;
            }
        }

        pQuery->nNode++;
        if (pQuery->nNode > pQuery->nNodeAlloc) {
            int nByte;
            pQuery->nNodeAlloc += 16;
            nByte = pQuery->nNodeAlloc * sizeof(HtmlNode *);
            pQuery->apNode = (HtmlNode**)HtmlRealloc(0, pQuery->apNode, nByte);
        }
        assert(i == pQuery->nNode - 1);
        pQuery->apNode[i] = pNode;
    }
    return 0;
}

static int
layoutNodeCompare(pVoidLeft, pVoidRight)
    const void *pVoidLeft;
    const void *pVoidRight;
{
    HtmlNode *pLeft = *(HtmlNode **)pVoidLeft;
    HtmlNode *pRight = *(HtmlNode **)pVoidRight;
    int iLeft = 0;
    int iRight = 0;

    if (HtmlNodeIsText(pLeft)) pLeft = HtmlNodeParent(pLeft);
    if (HtmlNodeIsText(pRight)) pRight = HtmlNodeParent(pRight);

    iLeft = ((HtmlElementNode *)pLeft)->pStack->iBlockZ;
    iRight = ((HtmlElementNode *)pRight)->pStack->iBlockZ;

    return iLeft - iRight;
}

/*
 *---------------------------------------------------------------------------
 *
 * layoutNodeCmd --
 *
 *     This function is called to process a command of the form:
 *
 *         <widget> node X Y
 *    
 * Results:
 *     No results returned.
 *
 * Side effects:
 *     The tcl interpreter HtmlTree.interp is loaded with the result of
 *     the [<widget> node X Y] command.
 *
 *---------------------------------------------------------------------------
 */
static void
layoutNodeCmd(pTree, x, y)
    HtmlTree *pTree;
    int x;
    int y;
{
    NodeQuery sQuery;
    memset(&sQuery, 0, sizeof(NodeQuery));

    sQuery.x = x;
    sQuery.y = y;

    searchCanvas(pTree, y-1, y+1, layoutNodeCb, (ClientData)&sQuery, 1);

    if (sQuery.nNode == 1) {
        Tcl_SetObjResult(pTree->interp, HtmlNodeCommand(pTree, *sQuery.apNode));
    } else if (sQuery.nNode > 0) {
        int i;
        Tcl_Obj *pRet = Tcl_NewObj();
        qsort(sQuery.apNode, sQuery.nNode, sizeof(HtmlNode*),layoutNodeCompare);
        for (i = 0; i < sQuery.nNode; i++) {
            Tcl_Obj *pCmd = HtmlNodeCommand(pTree, sQuery.apNode[i]);
            Tcl_ListObjAppendElement(0, pRet, pCmd);
        }
        Tcl_SetObjResult(pTree->interp, pRet);
    }
    HtmlFree(sQuery.apNode);
}
  

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutNode --
 *
 *     <widget> node
 *     <widget> node X Y
 *     <widget> node -index X Y
 *
 *     Return the Tcl handle for a document node. The first form returns
 *     a handle for the document root node.
 *
 *     The second form returns a list of zero or more nodes.
 *
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlLayoutNode(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int x;
    int y;

    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc == 2){
        if (pTree->pRoot) {
            Tcl_Obj *pCmd = HtmlNodeCommand(pTree, pTree->pRoot);
            Tcl_SetObjResult(interp, pCmd);
        }
    } else if (objc == 4 || objc == 5) {
        if (TCL_OK != Tcl_GetIntFromObj(interp, objv[objc - 2], &x) ||
            TCL_OK != Tcl_GetIntFromObj(interp, objv[objc - 1], &y) 
        ) {
            return TCL_ERROR;
        }

        /*
        ** Really, we do not want to force a callback here. Particularly
        ** since it causes a performance hit. But are the structures
        ** intact enough to do layoutNodeCmd() if we do not?
        */
        /* HtmlCallbackForce(pTree); */

        /* Transform x and y from viewport to document coordinates */
        x += pTree->iScrollX;
        y += pTree->iScrollY;

        if (objc == 4){
            layoutNodeCmd(pTree, x, y);
        } else {
            layoutNodeIndexCmd(pTree, x, y);
        }
    } else {
        Tcl_WrongNumArgs(interp, 2, objv, "?-index ?X Y??");
        return TCL_ERROR;
    }

    return TCL_OK;
}

struct BboxContext {
    HtmlNode *pPrevNode;
};
typedef struct BboxContext BboxContext;

static int
bboxCb(pItem, origin_x, origin_y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    Overflow *pOverflow;
    ClientData clientData;
{
    BboxContext *p = (BboxContext *)clientData;
    if (pItem->x.generic.pNode && 
        (pItem->type == CANVAS_BOX || pItem->type == CANVAS_TEXT)
    ) {
        HtmlNode *pNode = pItem->x.generic.pNode;
        int x, y, w, h;
        itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);
        if (pItem->x.generic.pNode == p->pPrevNode) {
            pNode->iBboxX = MIN(pNode->iBboxX, x);
            pNode->iBboxY = MIN(pNode->iBboxY, y);
            pNode->iBboxX2 = MAX(pNode->iBboxX2, x + w);
            pNode->iBboxY2 = MAX(pNode->iBboxY2, y + h);
        } else {
            pNode->iBboxX = x;
            pNode->iBboxY = y;
            pNode->iBboxX2 = x + w;
            pNode->iBboxY2 = y + h;
        }
    }
    return 0;
}

int 
HtmlWidgetBboxCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int x = 0, y = 0, x2 = -1, y2 = -1;
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tcl_Obj *pRet = Tcl_NewObj();

    if (objc != 2 && objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?NODE-HANDLE?");
        return TCL_ERROR;
    }

    HtmlCallbackForce(pTree);

    if (objc == 3) {
        HtmlNode *pNode = HtmlNodeGetPointer(pTree, Tcl_GetString(objv[2]));
        if (!pNode) {
            return TCL_ERROR;
        }
        if (!HtmlNodeIsOrphan(pNode)) {
            if (!pTree->isBboxOk) {
                BboxContext sContext;
                sContext.pPrevNode = 0;
                searchCanvas(pTree, -1, -1, bboxCb, (ClientData)&sContext, 1);
                pTree->isBboxOk = 1;
            }
    
            x = pNode->iBboxX;
            y = pNode->iBboxY;
            x2 = pNode->iBboxX2;
            y2 = pNode->iBboxY2;
        }
    } else {
        x = 0;
        y = 0;
        x2 = pTree->canvas.right;
        y2 = pTree->canvas.bottom;
    }

    if (x2 >= x) {
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(x));
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(y));
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(x2));
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(y2));
    }

    Tcl_SetObjResult(interp, pRet);
    return TCL_OK;
}

/*
 * A pointer to an instance of the following structure is passed by 
 * HtmlWidgetDamageText() to paintNodesSearchCb() as the client-data
 * parameter. 
 */
typedef struct PaintNodesQuery PaintNodesQuery;
struct PaintNodesQuery {
    int iNodeStart;
    int iIndexStart;
    int iNodeFin;
    int iIndexFin;
    int left;
    int right;
    int top;
    int bottom;
};

/*
 *---------------------------------------------------------------------------
 *
 * paintNodesSearchCb --
 *
 *     The callback for the canvas search performed by HtmlWidgetDamageText().
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
paintNodesSearchCb(pItem, origin_x, origin_y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    Overflow *pOverflow;
    ClientData clientData;
{
    PaintNodesQuery *p = (PaintNodesQuery *)clientData;

    if (pItem->type == CANVAS_TEXT) {
        CanvasText *pT = &(pItem->x.t);
        HtmlFont *pFont = fontFromNode(pT->pNode);
        if (pT->iIndex >= 0) {
            int iNode = pT->pNode->iNode;
            if (iNode >= p->iNodeStart && iNode <= p->iNodeFin) {
                int n;
                const char *z;
                int iIndex = pT->iIndex;
                int iIndex2;

                z = pT->zText;
                n = pT->nText;
                iIndex2 = iIndex + n;

                if ( 
                    (iNode != p->iNodeStart || iIndex2 >= p->iIndexStart) &&
                    (iNode != p->iNodeFin || iIndex <= p->iIndexFin)
                ) {
                    int top    = origin_y + pT->y - pFont->metrics.ascent;
                    int bottom = origin_y + pT->y + pFont->metrics.descent;
                    int left   = origin_x + pT->x;
                    int right;
                    int nFin = n;

                    if (iNode == p->iNodeFin && p->iIndexFin >= 0) {
                        nFin = MIN(n, 1 + p->iIndexFin - pT->iIndex);
                        right = Tk_TextWidth(pFont->tkfont, z, nFin) + left;
                    } else {
                        right = pT->w + left;
                    }
                    if (iNode == p->iNodeStart && p->iIndexStart > 0) {
                        int nStart = MAX(0, p->iIndexStart - pT->iIndex);
                        if (nStart > 0) {
                            assert(nStart <= n);
                            left += Tk_TextWidth(pFont->tkfont, z, nStart);
                        }
                    }

                    if (pOverflow) {
                        top -= pOverflow->yscroll;
                        bottom -= pOverflow->yscroll;
                        left -= pOverflow->xscroll;
                        right -= pOverflow->xscroll;
                        /* TODO: Clip to overflow region (optimization only) */
                    }
    
                    p->left   = MIN(left, p->left);
                    p->right  = MAX(right, p->right);
                    p->top    = MIN(top, p->top);
                    p->bottom = MAX(bottom, p->bottom);
                }
            }
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetDamageText --
 *
 *     This function is used to repaint the area covered by the text
 *     associated with a series of sequential nodes. It is used to update 
 *     the display when the selection changes. This function does not do any
 *     drawing itself, it schedules a callback using HtmlCallbackDamage()
 *     to do the actual work.
 *
 *     At most, a single rectangular area is redrawn. This is the minimum
 *     rectangle that is visible in the viewport that includes all text
 *     between node iStartNode, index iStartIndex and node iNodeFin, 
 *     iIndexFin.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May schedule a HTML_CALLBACK_DAMAGE callback, as described above.
 *
 *---------------------------------------------------------------------------
 */
void
HtmlWidgetDamageText(pTree, pNodeStart, iIndexStart, pNodeFin, iIndexFin)
    HtmlTree *pTree;         /* Widget tree */
    HtmlNode *pNodeStart;    /* First node to repaint */
    int iIndexStart;         /* First node to repaint */
    HtmlNode *pNodeFin;      /* Last node to repaint */
    int iIndexFin;           /* Last node to repaint */
{
    PaintNodesQuery sQuery;
    int ymin, ymax;
    int x, y;
    int w, h;
    int iNodeStart;
    int iNodeFin;

    HtmlSequenceNodes(pTree);
    iNodeStart = pNodeStart->iNode;
    iNodeFin = pNodeFin->iNode;

    if (iNodeStart > iNodeFin || 
        (iNodeStart == iNodeFin && iIndexStart > iIndexFin)
    ) {
        SWAPINT(iNodeStart, iNodeFin);
        SWAPINT(iIndexStart, iIndexFin);
    }

    sQuery.iNodeStart = iNodeStart;
    sQuery.iNodeFin = iNodeFin;
    sQuery.iIndexStart = iIndexStart;
    sQuery.iIndexFin = iIndexFin;
    sQuery.left = pTree->canvas.right;
    sQuery.right = pTree->canvas.left;
    sQuery.top = pTree->canvas.bottom;
    sQuery.bottom = pTree->canvas.top;

    ymin = pTree->iScrollY;
    ymax = pTree->iScrollY + Tk_Height(pTree->tkwin);

    searchCanvas(pTree,ymin,ymax,paintNodesSearchCb,(ClientData)&sQuery,1);

    x = sQuery.left - pTree->iScrollX;
    w = (sQuery.right - pTree->iScrollX) - x;
    y = sQuery.top - pTree->iScrollY;
    h = (sQuery.bottom - pTree->iScrollY) - y;
    HtmlCallbackDamage(pTree, x, y, w, h);
}

void
HtmlWidgetBboxText(
pTree, pNodeStart, iIndexStart, pNodeFin, iIndexFin, piT, piL, piB, piR
)
    HtmlTree *pTree;         /* Widget tree */
    HtmlNode *pNodeStart;    /* First node to repaint */
    int iIndexStart;         /* First node to repaint */
    HtmlNode *pNodeFin;      /* First node to repaint */
    int iIndexFin;           /* Last node to repaint */
    int *piT;
    int *piL;
    int *piB;
    int *piR;
{
    PaintNodesQuery sQuery;
    int iNodeStart;
    int iNodeFin;

    HtmlSequenceNodes(pTree);
    iNodeStart = pNodeStart->iNode;
    iNodeFin = pNodeFin->iNode;
  
    assert(iNodeStart <= iNodeFin);
    assert(iNodeFin > iNodeStart || iIndexFin >= iIndexStart);

    sQuery.iNodeStart = iNodeStart;
    sQuery.iNodeFin = iNodeFin;
    sQuery.iIndexStart = iIndexStart;
    sQuery.iIndexFin = iIndexFin;
    sQuery.left = pTree->canvas.right;
    sQuery.right = pTree->canvas.left;
    sQuery.top = pTree->canvas.bottom;
    sQuery.bottom = pTree->canvas.top;

    searchCanvas(pTree, -1, -1, paintNodesSearchCb, (ClientData)&sQuery, 1);

    *piL = sQuery.left;
    *piR = sQuery.right;
    *piT = sQuery.top;
    *piB = sQuery.bottom;
}

/*
 * The client-data for the search-callback used by HtmlWidgetNodeTop()
 */
typedef struct ScrollToQuery ScrollToQuery;
struct ScrollToQuery {
    HtmlTree *pTree;
    int iMinNode;
    int iMaxNode;
    int iReturn;
};

/*
 *---------------------------------------------------------------------------
 *
 * scrollToNodeCb --
 *     
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
scrollToNodeCb(pItem, origin_x, origin_y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    Overflow *pOverflow;
    ClientData clientData;
{
    int x, y, w, h;
    ScrollToQuery *pQuery = (ScrollToQuery *)clientData;
    HtmlNode *pNode;
    int iMaxNode = pQuery->iMaxNode;

    pNode = itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);

    /* If we have found a CANVAS_BOX for the node sought, then 
     * unconditionally return the pixel offset of the top-border edge
     * of the box. This is defined in CSS2.1.
     */
    if (pNode && pItem->type == CANVAS_BOX && pNode->iNode == iMaxNode){
        pQuery->iReturn = y;
        return 1;
    }
 
    if (
        pNode && 
        pNode->iNode <= pQuery->iMaxNode && 
        pNode->iNode >= pQuery->iMinNode
    ) {
        pQuery->iReturn = y;
        pQuery->iMinNode = pNode->iNode;
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetNodeTop --
 *
 *     The second argument, iNode, must be the node-number for some node 
 *     in the document tree pTree. This function returns the canvas 
 *     y-coordinate, in pixels of the top of the content generated by
 *     the node.
 *
 *     This is used in the implementation of the [widget yview nodeHandle]
 *     command. 
 *
 * Results:
 *     Pixels from the top of the canvas to the top of the content generated 
 *     by node iNode. Or, if node iNode does not generate content, then
 *     the content generated by node (iNode - 1). And so on. If no node
 *     with a node number less than iNode generated content, 0 is returned.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int
HtmlWidgetNodeTop(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    ScrollToQuery sQuery;

    HtmlSequenceNodes(pTree);
    HtmlCallbackForce(pTree);

    sQuery.iMaxNode = pNode->iNode;
    sQuery.iMinNode = 0;
    sQuery.iReturn = 0;
    sQuery.pTree = pTree;
    searchCanvas(pTree, -1, -1, scrollToNodeCb, (ClientData)&sQuery, 1);
    return sQuery.iReturn;
}

typedef struct LayoutBboxQuery LayoutBboxQuery;
struct LayoutBboxQuery {
    HtmlNode *pNode;
    int left;
    int right;
    int top;
    int bottom;
};

static int
layoutBboxCb(pItem, origin_x, origin_y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    Overflow *pOverflow;
    ClientData clientData;
{
    int x, y, w, h;
    LayoutBboxQuery *pQuery = (LayoutBboxQuery *)clientData;
    HtmlNode *pNode;

    pNode = itemToBox(pItem, origin_x, origin_y, &x, &y, &w, &h);
    if (pOverflow) {
        x -= pOverflow->xscroll;
        y -= pOverflow->yscroll;
    }

/* assert(x > -2000); */

    for (; pNode; pNode = HtmlNodeParent(pNode)) {
        if (pNode == pQuery->pNode) {
            pQuery->left = MIN(pQuery->left, x);
            pQuery->top = MIN(pQuery->top, y);
            pQuery->right = MAX(pQuery->right, x + w);
            pQuery->bottom = MAX(pQuery->bottom, y + h);
        }
    }
    return 0;
}

typedef struct OverflowBox OverflowBox;
struct OverflowBox {
    HtmlNode *pNode;
    int *pX;
    int *pY;
    int *pW;
    int *pH;
    int isFound;
};

static int
overflowBoxCb(pItem, origin_x, origin_y, pOverflow, clientData)
    HtmlCanvasItem *pItem;
    int origin_x;
    int origin_y;
    Overflow *pOverflow;
    ClientData clientData;
{
    OverflowBox *p = (OverflowBox *)clientData;
    if (p->isFound) return 0;

    if (pOverflow && p->pNode == pOverflow->pItem->pNode) {
        *p->pX = pOverflow->x;
        *p->pY = pOverflow->y;
        *p->pW = pOverflow->w;
        *p->pH = pOverflow->h;
        p->isFound = 1;
    }
    return 0;
}

void 
HtmlWidgetOverflowBox(pTree, pNode, pX, pY, pW, pH)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int *pX;
    int *pY;
    int *pW;
    int *pH;
{
    OverflowBox sOverflowBox;
    memset(&sOverflowBox, 0, sizeof(OverflowBox));
    sOverflowBox.pX = pX;
    sOverflowBox.pY = pY;
    sOverflowBox.pW = pW;
    sOverflowBox.pH = pH;
    sOverflowBox.pNode = pNode;
    searchCanvas(pTree, -1, -1, overflowBoxCb, &sOverflowBox, 1);
    return;
}

void 
HtmlWidgetNodeBox(pTree, pNode, pX, pY, pW, pH)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int *pX;
    int *pY;
    int *pW;
    int *pH;
{
    HtmlCanvas *pCanvas = &pTree->canvas;
    LayoutBboxQuery sQuery;
    HtmlCanvasItem *pItem;
    HtmlCanvasItem *pSkip = 0;

    int origin_x = 0;
    int origin_y = 0;

    HtmlCallbackForce(pTree);

    sQuery.left = pCanvas->right;
    sQuery.right = pCanvas->left;
    sQuery.top = pCanvas->bottom;
    sQuery.bottom = pCanvas->top;
    sQuery.pNode = pNode;

    for (pItem = pCanvas->pFirst; pItem; pItem = (pSkip?pSkip:pItem->pNext)) {
        pSkip = 0;
        if (pItem->type == CANVAS_OVERFLOW) {
            CanvasOverflow *pO = &pItem->x.overflow;
            HtmlNode *p = pO->pNode;
            while (p && p != pNode) p = HtmlNodeParent(p);
            if (p) {
                CanvasOverflow *pO = &pItem->x.overflow;
                sQuery.left = MIN(sQuery.left, pO->x + origin_x);
                sQuery.top = MIN(sQuery.top, pO->y + origin_y);
                sQuery.right = MAX(sQuery.right, pO->x + origin_x + pO->w);
                sQuery.bottom = MAX(sQuery.bottom, pO->y + origin_y + pO->h);
            }
            pSkip = pO->pEnd;
        } else if (pItem->type == CANVAS_ORIGIN) {
            origin_x += pItem->x.o.x;
            origin_y += pItem->x.o.y;
        } else {
            layoutBboxCb(pItem, origin_x, origin_y, 0, (ClientData)&sQuery);
        }
    }

    if (sQuery.left < sQuery.right && sQuery.top < sQuery.bottom) {
        *pX = sQuery.left;
        *pY = sQuery.top;
        *pW = sQuery.right - *pX;
        *pH = sQuery.bottom - *pY;
    } else {
        *pX = 0;
        *pY = 0;
        *pW = 0;
        *pH = 0;
    }
}

static void 
widgetRepair(pTree, x, y, w, h, g)
    HtmlTree *pTree;
    int x;
    int y;
    int w;
    int h;
    int g;
{
    Pixmap pixmap;
    GC gc;
    XGCValues gc_values;
    Tk_Window win = pTree->tkwin;
    Display *pDisp = Tk_Display(win); 

    if (w <= 0 || h <= 0) {
        return;
    }

    pixmap = getPixmap(pTree, pTree->iScrollX+x, pTree->iScrollY+y, w, h, g);
    memset(&gc_values, 0, sizeof(XGCValues));
    gc = Tk_GetGC(pTree->tkwin, 0, &gc_values);
    assert(Tk_WindowId(win));

    XCopyArea(
        pDisp, pixmap, Tk_WindowId(pTree->docwin), gc, 0, 0, w, h, 
        x - Tk_X(pTree->docwin), y - Tk_Y(pTree->docwin)
    );

    Tk_FreePixmap(pDisp, pixmap);
    Tk_FreeGC(pDisp, gc);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetRepair --
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlWidgetRepair(pTree, x, y, w, h, windowsrepair)
    HtmlTree *pTree;
    int x;
    int y;
    int w;
    int h;
    int windowsrepair;
{
    /* if( !pTree->options.enablelayout ) return; */
    if (!Tk_IsMapped(pTree->tkwin)) return;

    /* Make sure the widget main window exists before painting anything */
    Tk_MakeWindowExist(pTree->tkwin);
    Tk_MakeWindowExist(pTree->docwin);

    widgetRepair(pTree, x, y, w, h, windowsrepair);
    if (windowsrepair) {
        windowsRepair(pTree, &pTree->canvas);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWidgetSetViewport --
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlWidgetSetViewport(pTree, scroll_x, scroll_y, force_redraw)
    HtmlTree *pTree;            /* Tree structure */
    int scroll_x;               /* New value for pTree->iScrollX */
    int scroll_y;               /* New value for pTree->iScrollY */
    int force_redraw;           /* Redraw the entire viewport regardless */
{
    pTree->iScrollY = scroll_y;
    pTree->iScrollX = scroll_x;

    if (pTree->isFixed) {
        /* Variable HtmlTree.isFixed is true if the document contains
         * fixed background images or boxes. If this is not zero, then we need
         * to redraw the entire viewport each time the user scrolls the window.
         * In other words, we need to do something generate an expose event
         * that covers the whole viewport.
         *
         * Moving the docwin between coords (0,0) and (-10000,0) each time
         * the window is scrolled seems to achieve this.
         */
        int iNewY = Tk_Y(pTree->docwin);
        if (iNewY <= -5000) {
            iNewY = 0;
        }else{
            iNewY = -10000;
        }
        Tk_MoveWindow(pTree->docwin, 0, iNewY);
    } else {
        int iShiftY;
        int iShiftX;

        scroll_x = scroll_x%25000;
        scroll_y = scroll_y%25000;
        iShiftY = Tk_Y(pTree->docwin) - scroll_y;
        iShiftX = Tk_X(pTree->docwin) - scroll_x;

        if ( iShiftY > 20000 || iShiftY < -20000 || 
             iShiftX > 20000 || iShiftX < -20000
        ) {
            /* If moving the window more than 20000 pixels in either the
	     * horizontal or vertical direction, make sure the entire viewport
             * is redrawn.
             */
            HtmlCallbackDamage(pTree, 0, 0, 100000, 100000);
        }
        Tk_MoveWindow(pTree->docwin, -1*scroll_x, -1*scroll_y);
    }
}

HtmlCanvasItem *
HtmlDrawAddMarker(pCanvas, x, y, fixed)
    HtmlCanvas *pCanvas;
    int x;
    int y;
    int fixed;
{
    HtmlCanvasItem *pItem; 
CHECK_CANVAS(pCanvas);
    pItem = allocateCanvasItem();
    pItem->type = CANVAS_MARKER;
    pItem->x.marker.x = x;
    pItem->x.marker.y = y;
    pItem->x.marker.flags = (fixed ? MARKER_FIXED : 0);
    linkItem(pCanvas, pItem);
CHECK_CANVAS(pCanvas);
    return pItem;
}

void
HtmlDrawAddLinebox(pCanvas, x, y) 
    HtmlCanvas *pCanvas;
    int x;
    int y;
{
    HtmlCanvasItem *pItem; 
CHECK_CANVAS(pCanvas);
    pItem = allocateCanvasItem();
    pItem->type = CANVAS_MARKER;
    pItem->x.marker.x = x;
    pItem->x.marker.y = y;
    pItem->x.marker.flags = MARKER_LINEBOX;
    linkItem(pCanvas, pItem);
CHECK_CANVAS(pCanvas);
    return;
}

int
HtmlDrawFindLinebox(pCanvas, pX, pY)
    HtmlCanvas *pCanvas;
    int *pX;
    int *pY;
{
    int origin_x = 0;
    int origin_y = 0;
    HtmlCanvasItem *pItem; 
    CHECK_CANVAS(pCanvas);
    for (pItem = pCanvas->pFirst; pItem; pItem = pItem->pNext) {
        if (pItem->type == CANVAS_ORIGIN) {
            CanvasOrigin *pOrigin = &pItem->x.o;
            origin_x += pOrigin->x;
            origin_y += pOrigin->y;
        } else if (
            pItem->type == CANVAS_MARKER && 
            pItem->x.marker.flags == MARKER_LINEBOX
        ){
            *pX = origin_x + pItem->x.marker.x;
            *pY = origin_y + pItem->x.marker.y;
            return 1;
        }
    }
    return 0;
}

int
HtmlDrawGetMarker(pCanvas, pMarker, pX, pY)
    HtmlCanvas *pCanvas;
    HtmlCanvasItem *pMarker;
    int *pX;
    int *pY;
{
    int origin_x = 0;
    int origin_y = 0;
    HtmlCanvasItem *pItem; 
    HtmlCanvasItem *pPrev = 0; 
    CHECK_CANVAS(pCanvas);
    for (pItem = pCanvas->pFirst; pItem && pMarker; pItem = pItem->pNext) {
        if (pItem->type == CANVAS_ORIGIN) {
            CanvasOrigin *pOrigin = &pItem->x.o;
            origin_x += pOrigin->x;
            origin_y += pOrigin->y;
        } else if (pItem == pMarker) {
            *pX = origin_x + pItem->x.marker.x;
            *pY = origin_y + pItem->x.marker.y;
            if (pPrev) {
                assert(pPrev->pNext == pMarker);
                pPrev->pNext = pMarker->pNext;
            } else {
                assert(pCanvas->pFirst == pMarker);
                pCanvas->pFirst = pMarker->pNext;
            }
            if (pCanvas->pLast == pMarker) {
                pCanvas->pLast = pPrev ? pPrev : pCanvas->pFirst;
            }
            freeCanvasItem(0, pMarker);
            CHECK_CANVAS(pCanvas);
            return 0;
        }
        pPrev = pItem;
    }
    return 1;
}

