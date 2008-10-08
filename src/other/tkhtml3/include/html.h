/*
 * html.h --
 *
 *----------------------------------------------------------------------------
 * Copyright (c) 2005 Dan Kennedy.
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
 *     * Neither the name of Eolas Technologies. nor the names of its
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

#ifndef __HTMLTREE_H__
#define __HTMLTREE_H__

/*
 * Without exception the tkhtml code uses the wrapper functions HtmlAlloc(),
 * HtmlFree() and HtmlRealloc() in place of the regular Tcl ckalloc(), ckfree()
 * and ckrealloc() functions.
 */
#ifdef HTML_DEBUG
    #include "restrack.h"

    #define HtmlAlloc(zTopic, n) Rt_Alloc((zTopic), n)
    #define HtmlFree(x) Rt_Free((char *)(x))
    #define HtmlRealloc(zTopic, x, n) Rt_Realloc(zTopic , (char *)(x), (n))
#else
    #define HtmlAlloc(zTopic, n) ckalloc(n)
    #define HtmlFree(x) ckfree((char *)(x))
    #define HtmlRealloc(zTopic, x, n) ckrealloc((char *)(x), n)
#endif

/* HtmlClearAlloc() is a version of HtmlAlloc() that returns zeroed memory */
#define HtmlClearAlloc(zTopic, x) ((char *)memset(HtmlAlloc(zTopic,(x)),0,(x)))

#define HtmlNew(x) ((x *)HtmlClearAlloc(#x, sizeof(x)))

#define USE_COMPOSITELESS_PHOTO_PUT_BLOCK
#include <tk.h>

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "htmltokens.h"
#include "htmlmacros.h"

/*
 * Version information for the package mechanism.
 */
#define HTML_PKGNAME "Tkhtml"
#define HTML_PKGVERSION "3.0"

/*
 * Various data types.  This code is designed to run on a modern
 * cached architecture where the CPU runs a lot faster than the
 * memory bus.  Hence we try to pack as much data into as small a space
 * as possible so that it is more likely to fit in cache.  The
 * extra CPU instruction or two needed to unpack the data is not
 * normally an issue since we expect the speed of the memory bus 
 * to be the limiting factor.
 */
typedef unsigned char  Html_u8;      /* 8-bit unsigned integer */
typedef short          Html_16;      /* 16-bit signed integer */
typedef unsigned short Html_u16;     /* 16-bit unsigned integer */
typedef int            Html_32;      /* 32-bit signed integer */

/*
 * Linux doesn't have a stricmp() function and windows doesn't have
 * strcasecmp(). Tkhtml3 code uses stricmp (and strnicmp) because
 * it is faster to type.
 */
#ifndef __WIN32__
# define stricmp strcasecmp
# define strnicmp strncasecmp
#endif

/*
 * Some versions of windows do not have the vsnprintf() and snprintf()
 * macros. According to msdn, these are for "backwards compatibility" only.
 */
#ifdef __WIN32__
# define vsnprintf _vsnprintf
# define snprintf _snprintf
#endif

typedef struct HtmlNodeStack HtmlNodeStack;
typedef struct HtmlOptions HtmlOptions;
typedef struct HtmlTree HtmlTree;
typedef struct HtmlTreeState HtmlTreeState;
typedef struct HtmlAttributes HtmlAttributes;
typedef struct HtmlTokenMap HtmlTokenMap;
typedef struct HtmlCanvas HtmlCanvas;
typedef struct HtmlCanvasItem HtmlCanvasItem;
typedef struct HtmlFloatList HtmlFloatList;
typedef struct HtmlPropertyCache HtmlPropertyCache;
typedef struct HtmlNodeReplacement HtmlNodeReplacement;
typedef struct HtmlCallback HtmlCallback;
typedef struct HtmlNodeCmd HtmlNodeCmd;
typedef struct HtmlLayoutCache HtmlLayoutCache;
typedef struct HtmlNodeScrollbars HtmlNodeScrollbars;

typedef struct HtmlImageServer HtmlImageServer;
typedef struct HtmlImage2 HtmlImage2;

typedef struct HtmlWidgetTag HtmlWidgetTag;
typedef struct HtmlTaggedRegion HtmlTaggedRegion;
typedef struct HtmlText HtmlText;

typedef struct HtmlNode HtmlNode;
typedef struct HtmlElementNode HtmlElementNode;
typedef struct HtmlTextNode HtmlTextNode;

typedef struct HtmlTextToken HtmlTextToken;
typedef struct HtmlTextIter HtmlTextIter;

typedef struct HtmlDamage HtmlDamage;

typedef struct HtmlFragmentContext HtmlFragmentContext;
typedef struct HtmlSearchCache HtmlSearchCache;

#include "css.h"
#include "htmlprop.h"

typedef int (*HtmlContentTest)(HtmlTree *, HtmlNode *, int);

#define FLOAT_LEFT       CSS_CONST_LEFT
#define FLOAT_RIGHT      CSS_CONST_RIGHT
#define FLOAT_NONE       CSS_CONST_NONE

#define CLEAR_NONE       CSS_CONST_NONE
#define CLEAR_LEFT       CSS_CONST_LEFT
#define CLEAR_RIGHT      CSS_CONST_RIGHT
#define CLEAR_BOTH       CSS_CONST_BOTH

struct HtmlTokenMap {
  char *zName;                    /* Name of a markup */
  Html_16 type;                   /* Markup type code */
  Html_u8 flags;                  /* Combination of HTMLTAG values */
  HtmlContentTest xClose;         /* Function to identify close tag */
  HtmlTokenMap *pCollide;         /* Hash table collision chain */
};

#define HTMLTAG_INLINE      0x02  /* Set for an HTML inline tag */
#define HTMLTAG_BLOCK       0x04  /* Set for an HTML block tag */
#define HTMLTAG_EMPTY       0x08  /* Set for an empty tag (i.e. <img>) */

#define HTMLTAG_HEAD        0x10  /* Element must go in <head> */
#define HTMLTAG_BODY        0x20  /* Element must go in <body> */
#define HTMLTAG_FRAMESET    0x40  /* Element is from a frameset document */
#define HTMLTAG_PCDATA      0x80  /* Contents of element is PCDATA */

#define TAG_CLOSE    1
#define TAG_PARENT   2
#define TAG_OK       3

struct HtmlAttributes {
    int nAttr;
    struct HtmlAttribute {
        char *zName;
        char *zValue;
    } a[1];
};

/*
 * For a replaced node, the HtmlNode.pReplacement variable points to an
 * instance of the following structure. The member objects are the name of
 * the replaced object (widget handle), the configure script if any, and the
 * delete script if any. i.e. in Tcl:
 *
 *     $nodeHandle replace $pReplace \
 *             -configurecmd $pConfigure -deletecmd $pDelete
 *
 * The iOffset variable holds the integer returned by any -configurecmd
 * script (default value 0). This is assumed to be the number of pixels
 * between the bottom of the replacement window and the point that should
 * be aligned with the bottom of a line-box. i.e. equivalent to the 
 * "descent" property of a font.
 */
struct HtmlNodeReplacement {
    Tcl_Obj *pReplace;            /* Replacement window name */
    Tk_Window win;                /* Replacement window (if any) */
    Tcl_Obj *pConfigureCmd;       /* Script passed to -configurecmd */
    Tcl_Obj *pStyleCmd;           /* Script passed to -stylecmd */
    Tcl_Obj *pDelete;             /* Script passed to -deletecmd */
    int iOffset;                  /* See above */

    /* Display related variables */
    int clipped;                  /* Boolean. If true, do not display */
    int iCanvasX;                 /* Current X canvas coordinate of window */
    int iCanvasY;                 /* Current Y canvas coordinate of window */
    int iWidth;                   /* Current calculated pixel width of window*/
    int iHeight;                  /* Current calculated pixel height of window*/
    HtmlNodeReplacement *pNext;   /* Next element in HtmlTree.pMapped list */
};

/*
 * When a Tcl command representing a node-handle is created, an instance of the 
 * following structure is allocated.
 */
struct HtmlNodeCmd {
    Tcl_Obj *pCommand;
    HtmlTree *pTree;
};

struct HtmlNodeStack {
    HtmlElementNode *pElem;
    int eType;              /* Usage defined in htmlstyle.c */

    HtmlNodeStack *pNext;
    HtmlNodeStack *pPrev;

    /* These three are set by HtmlRestackNodes() after the style-engine
     * runs, and used by htmldraw.c at during drawing to determine the
     * relative z-axis position of each drawing primitive.
     */
    int iInlineZ;
    int iBlockZ;
    int iStackingZ;
};

/*
 * Scrollbar information for nodes with "overflow:auto" or "overflow:scroll".
 * Nodes for which the overflow property takes one of the other values 
 * ("hidden" or "visible") do not have an associated instance of this
 * structure. 
 *
 * The structure itself is allocated and released seperately for
 * each node by style-engine code. The layout-engine takes care of
 * creating the scrollbars (if required).
 */
struct HtmlNodeScrollbars {
    HtmlNodeReplacement vertical;
    HtmlNodeReplacement horizontal;

    int iVertical;
    int iHorizontal;

    int iHeight;               /* Height of viewport */
    int iWidth;                /* Width of viewport */
    int iVerticalMax;          /* Height of scrollable area */
    int iHorizontalMax;        /* Width of scrollable area */
};

/*
 * Widget tag properties. Each widget tag is stored in the 
 * HtmlTree.aTag hash table.
 */
struct HtmlWidgetTag {
    XColor *foreground;        /* Foreground color to use for tagged regions */
    XColor *background;        /* Background color to use for tagged regions */
};

/*
 * Each text node has a list of "tagged regions" attached to it (the 
 * list may be empty). See the HtmlTextNode.pTagged variable.
 */
struct HtmlTaggedRegion {
    int iFrom;                 /* Index the region starts at */
    int iTo;                   /* Index the region ends at */
    HtmlWidgetTag *pTag;       /* Tag properties */
    HtmlTaggedRegion *pNext;   /* Next tagged region of this text node */
};

/* 
 * Each node of the document tree is represented as an HtmlNode structure.
 * This structure carries no information to do with the node itself, it is
 * simply used to build the tree structure. All the information for the
 * node is carried by the derived classes HtmlTextNode and HtmlElementNode.
 */
struct HtmlNode {
    ClientData clientData;
    HtmlNode *pParent;             /* Parent of this node */
    int iNode;                     /* Node index */

    Html_u8 eTag;                  /* Tag type (or 0) */
    const char *zTag;              /* Atom string for tag type */

    int iSnapshot;                 /* Last changed snapshot */
    HtmlNodeCmd *pNodeCmd;         /* Tcl command for this node */

    /* Cache used for [$widget bbox] */
    int iBboxX; int iBboxY;
    int iBboxX2; int iBboxY2;
};

/* Value of HtmlNode.iNode for orphan and generated nodes. */
#define HTML_NODE_ORPHAN -23
#define HTML_NODE_GENERATED -1

/*
 * Structure to store a text node.
 */
struct HtmlTextNode {
    HtmlNode node;                 /* Base class. MUST BE FIRST. */
    HtmlTaggedRegion *pTagged;     /* List of applied Widget tags */

    /* These variables are manipulated by code in htmltext.c. They are
     * populated when this structure is allocated (function HtmlTextNew()),
     * and accessed using the HtmlTextIterXXX() API. See comments in 
     * htmltext.c for a description.
     */
    HtmlTextToken *aToken;
    char *zText;
};

/*
 * The name of the attribute parsed to generate inline-style properties
 * for an element (HtmlElementNode.pStyle).
 */
#define HTML_INLINE_STYLE_ATTR "style"

/*
 * Structure to store an element (non-text) node.
 */
struct HtmlElementNode {
    HtmlNode node;          /* Base class. MUST BE FIRST. */

    HtmlAttributes *pAttributes;      /* Html attributes associated with node */

    /* Children of this element node */
    int nChild;                    /* Number of child nodes */
    HtmlNode **apChildren;         /* Array of pointers to children nodes */

    CssPropertySet *pStyle;                /* Parsed inline style */

    /* Information generated by the style engine */
    HtmlComputedValues *pPropertyValues;   /* Current CSS property values */
    HtmlComputedValues *pPreviousValues;   /* Previous CSS property values */
    CssDynamic *pDynamic;                  /* CSS dynamic conditions */
    Tcl_Obj *pOverride;                    /* List of property overrides */
    HtmlNodeStack *pStack;                 /* Stacking context */
    HtmlNode *pBefore;                     /* Generated :before content */
    HtmlNode *pAfter;                      /* Generated :after content */

    /* Manipulated by the [nodeHandle dynamic] command */
    Html_u8 flags;                         /* HTML_DYNAMIC_XXX flags */

    HtmlNodeReplacement *pReplacement;     /* Replaced object, if any */
    HtmlLayoutCache *pLayoutCache;         /* Cached layout, if any */
    HtmlNodeScrollbars *pScrollbar;        /* Internal scrollbars, if any */

    HtmlCanvasItem *pBox;
};

/* Alias for HtmlNodeXXX() methods */
#define HtmlElemParent(p) ((HtmlElementNode *)HtmlNodeParent(&(p)->node))

/* Values for HtmlNode.flags. These may be set and cleared via the Tcl
 * interface on the node command: [$node dynamic set|clear ...]
 */
#define HTML_DYNAMIC_HOVER    0x01
#define HTML_DYNAMIC_FOCUS    0x02
#define HTML_DYNAMIC_ACTIVE   0x04
#define HTML_DYNAMIC_LINK     0x08
#define HTML_DYNAMIC_VISITED  0x10
#define HTML_DYNAMIC_USERFLAG 0x20

struct HtmlCanvas {
    int left;
    int right;
    int top;
    int bottom;
    HtmlCanvasItem *pFirst;
    HtmlCanvasItem *pLast;
};

/*
 * All widget options are stored in this structure, which is a part of
 * the HtmlTree structure (HtmlTree.options).
 *
 * All of the options in this structure should be documented in the 
 * Tkhtml3 man-page. If they are not, please report a bug.
 */
struct HtmlOptions {

    /* Tkhtml3 supports the following standard Tk options */
    int      width;
    int      height;
    int      xscrollincrement;
    int      yscrollincrement;
    Tcl_Obj *yscrollcommand;
    Tcl_Obj *xscrollcommand;

    Tcl_Obj *defaultstyle;
    double   fontscale;
    Tcl_Obj *fonttable;
    int      forcefontmetrics;
    int      forcewidth;
    Tcl_Obj *imagecmd;
    int      imagecache;
    int      imagepixmapify;
    int      mode;                      /* One of the HTML_MODE_XXX values */
    int      shrink;                    /* Boolean */
    double   zoom;                      /* Universal scaling factor. */

    int      parsemode;                 /* One of the HTML_PARSEMODE values */

    /* Debugging options. Not part of the official interface. */
    int      enablelayout;
    int      layoutcache;
    Tcl_Obj *logcmd;
    Tcl_Obj *timercmd;
};

#define HTML_MODE_QUIRKS    0
#define HTML_MODE_ALMOST    1
#define HTML_MODE_STANDARDS 2

#define HTML_PARSEMODE_HTML    0
#define HTML_PARSEMODE_XHTML   1
#define HTML_PARSEMODE_XML     2

void HtmlLog(HtmlTree *, CONST char *, CONST char *, ...);
void HtmlTimer(HtmlTree *, CONST char *, CONST char *, ...);

typedef struct HtmlCanvasSnapshot HtmlCanvasSnapshot;

struct HtmlDamage {
  int x;
  int y;
  int w;
  int h;
  int windowsrepair;
  HtmlDamage *pNext;
};

/*
 * Widget state information information is stored in an instance of this
 * structure, which is a part of the HtmlTree. The variables within control 
 * the behaviour of the idle callback scheduled to update the display.
 */
struct HtmlCallback {
    int flags;                  /* Comb. of HTML_XXX bitmasks defined below */
    int inProgress;             /* Prevent recursive invocation */
    int isForce;                /* Not an idle callback */

    /* Snapshot of layout before the latest round of changes. This is
     * used to reduce the area repainted during "animation" changes.
     * (drag and drop, menus etc. in javascript).
     */
    HtmlCanvasSnapshot *pSnapshot;

    /* HTML_DYNAMIC */
    HtmlNode *pDynamic;         /* Recalculate dynamic CSS for this node */

    /* HTML_DAMAGE */
    HtmlDamage *pDamage;

    /* HTML_RESTYLE */
    HtmlNode *pRestyle;         /* Restyle this node */

    /* HTML_SCROLL */
    int iScrollX;               /* New HtmlTree.iScrollX value */
    int iScrollY;               /* New HtmlTree.iScrollY value */
};

/* Values for HtmlCallback.flags */
#define HTML_DYNAMIC    0x01
#define HTML_DAMAGE     0x02
#define HTML_RESTYLE    0x04
#define HTML_LAYOUT     0x08
#define HTML_SCROLL     0x10
#define HTML_STACK      0x20
#define HTML_NODESCROLL 0x40

/* 
 * Functions used to schedule callbacks and set the HtmlCallback state. 
 */
void HtmlCallbackForce(HtmlTree *);
void HtmlCallbackDynamic(HtmlTree *, HtmlNode *);
void HtmlCallbackDamage(HtmlTree *, int, int, int, int);
void HtmlCallbackLayout(HtmlTree *, HtmlNode *);
void HtmlCallbackRestyle(HtmlTree *, HtmlNode *);

void HtmlCallbackScrollX(HtmlTree *, int);
void HtmlCallbackScrollY(HtmlTree *, int);

void HtmlCallbackDamageNode(HtmlTree *, HtmlNode *);

/*
 * An instance of the following structure stores state for the tree
 * construction phase. See the following functions:
 *
 *     HtmlTreeAddElement()
 *     HtmlTreeAddText()
 *     HtmlTreeAddClosingTag()
 */
struct HtmlTreeState {
    HtmlNode *pCurrent;     /* By default, add new elements as children here */
    HtmlNode *pFoster;      /* The current node in the foster tree (if any) */
    int isCdataInHead;      /* True if previous token was <title> */
};

struct HtmlTree {

    /*
     * The interpreter hosting this widget instance.
     */
    Tcl_Interp *interp;             /* Tcl interpreter */

    /*
     * The widget window.
     */
    Tk_Window tkwin;           /* Widget window */
    int iScrollX;              /* Number of pixels offscreen to the left */
    int iScrollY;              /* Number of pixels offscreen to the top */

    Tk_Window docwin;          /* Document window */

    /*
     * The widget command.
     */
    Tcl_Command cmd;           /* Widget command */
    int isDeleted;             /* True once the widget-delete has begun */

    /*
     * The image server object.
     */
    HtmlImageServer *pImageServer;

    /*
     * The search cache object.
     */
    HtmlSearchCache *pSearchCache;

    /* The following variables are used to stored the text of the current
     * document (i.e. the *.html file) as it is being parsed.
     *
     * nParsed and nCharParsed are kept in sync. If the document consists
     * entirely of 7-bit ASCII, then they are equal. The nCharParsed variable
     * is required so that the offsets passed to parse-handler callbacks
     * are in characters, not bytes. TODO! See ticket #126.
     */
    Tcl_Obj *pDocument;             /* Text of the html document */
    int nParsed;                    /* Bytes of pDocument tokenized */
    int nCharParsed;                /* TODO: Characters parsed */

    int iWriteInsert;               /* Byte offset in pDocument for [write] */
    int eWriteState;                /* One of the HTML_WRITE_XXX values */

    int isIgnoreNewline;            /* True after an opening tag */
    int isParseFinished;            /* True if the html parse is finished */

    HtmlNode *pRoot;                /* The root-node of the document. */

    Tcl_HashTable aAtom;            /* String atoms for this widget */

    HtmlTreeState state;

    /* Sub-trees that are not currently linked into the tree rooted at 
     * pRoot are stored in the following hash-table. The HTML_NODE_ORPHAN
     * flag is set in the HtmlNode.flags member of the root of each tree.
     *
     * The key for each entry is the pointer to the HtmlNode structure
     * that is the root of the orphaned tree. Hash entry data is not used.
     */
    Tcl_HashTable aOrphan;          /* Orphan nodes (see [$html fragment]) */

    /* This pointer is used to store context during the exeuction of 
     * the [$html fragment] command. See htmltree.c for details.
     */
    HtmlFragmentContext *pFragment;

    int isFixed;                    /* True if any "fixed" graphics */

    /*
     * Handler callbacks configured by the [$widget handler] command.
     *
     * The aScriptHandler hash table contains entries representing
     * script-handler callbacks. Each entry maps a tag-type (i.e. Html_P)
     * to a Tcl_Obj* that contains the Tcl script to call when the tag type is
     * encountered. A single argument is appended to the script - all the text
     * between the start and end tag. The ref-count of the Tcl_Obj* should be
     * decremented if it is removed from the hash table.
     *
     * The node-handler and parse-handler tables are similar.
     */
    Tcl_HashTable aScriptHandler;     /* Script handler callbacks. */
    Tcl_HashTable aNodeHandler;       /* Node handler callbacks. */
    Tcl_HashTable aParseHandler;      /* Parse handler callbacks. */
    Tcl_HashTable aAttributeHandler;  /* Attribute handler callbacks. */

    CssStyleSheet *pStyle;          /* Style sheet configuration */

    /* Used by code in HtmlStyleApply() */
    void *pStyleApply;

    HtmlOptions options;            /* Configurable options */
    Tk_OptionTable optionTable;     /* Option table */

    /* Linked list of stacking contexts */
    HtmlNodeStack *pStack;
    int nStack;                   /* Number of elements in linked list */

    /*
     * Internal representation of a completely layed-out document.
     */
    HtmlCanvas canvas;              /* Canvas to render into */
    int iCanvasWidth;               /* Width of window for canvas */
    int iCanvasHeight;              /* Height of window for canvas */

    /* Linked list of currently mapped replacement objects */
    HtmlNodeReplacement *pMapped;

    /* 
     * Tables managed by code in htmlprop.c. Initialised in function
     * HtmlComputedValuesSetupTables(), except for aFontSizeTable[], which is
     * set via the -fonttable option. 
     */
    Tcl_HashTable aColor;
    HtmlFontCache fontcache;
    Tcl_HashTable aValues;
    Tcl_HashTable aFontFamilies;
    Tcl_HashTable aCounterLists;
    HtmlComputedValuesCreator *pPrototypeCreator;

    int aFontSizeTable[7];

    /*
     * Hash table for all html widget tags (similar to text widget tags -
     * nothing to do with markup tags).
     */
    Tcl_HashTable aTag;
    Tk_OptionTable tagOptionTable;     /* Option table for tags*/

    /* The isSequenceOk variable is true if the HtmlNode.iNode values for all
     * nodes in the tree are currently in tree order.
     */
    int isSequenceOk;    
    int iNextNode;       /* Next node index to allocate */

    /* True if the HtmlElementNode.iBboxX and HtmlElementNode.iBboxY values
     * for all elements in the tree are valid.
     */
    int isBboxOk;

    HtmlCallback cb;                /* See structure definition comments */
    int iLastSnapshotId;            /* Last snapshot id allocated */
    Tcl_TimerToken delayToken;

    /* 
     * Data structure used by the [widget text] commands. See the
     * HtmlTextXXX() API below. 
     */
    HtmlText *pText;

#ifdef TKHTML_ENABLE_PROFILE
    /*
     * Client data from instrument command ([::tkhtml::instrument]).
     */
    ClientData pInstrumentData;
#endif
};

#define HTML_WRITE_NONE           0
#define HTML_WRITE_INHANDLER      1
#define HTML_WRITE_INHANDLERWAIT  2
#define HTML_WRITE_INHANDLERRESET 3
#define HTML_WRITE_WAIT           4
#define HTML_PARSE_NODEHANDLER    5
int HtmlWriteWait(HtmlTree *);
int HtmlWriteText(HtmlTree *, Tcl_Obj *);
int HtmlWriteContinue(HtmlTree *);


#define MAX(x,y)   ((x)>(y)?(x):(y))
#define MIN(x,y)   ((x)<(y)?(x):(y))
#define INTEGER(x) ((int)((x) + (((x) > 0.0) ? 0.49 : -0.49)))

void HtmlFinishNodeHandlers(HtmlTree *);

Tcl_ObjCmdProc HtmlTreeCollapseWhitespace;
Tcl_ObjCmdProc HtmlStyleSyntaxErrs;
Tcl_ObjCmdProc HtmlLayoutSize;
Tcl_ObjCmdProc HtmlLayoutNode;
Tcl_ObjCmdProc HtmlLayoutImage;
Tcl_ObjCmdProc HtmlLayoutPrimitives;
Tcl_ObjCmdProc HtmlCssStyleConfigDump;
Tcl_ObjCmdProc Rt_AllocCommand;
Tcl_ObjCmdProc HtmlWidgetBboxCmd;
Tcl_ObjCmdProc HtmlImageServerReport;

Tcl_ObjCmdProc HtmlDebug;
Tcl_ObjCmdProc HtmlDecode;
Tcl_ObjCmdProc HtmlEncode;
Tcl_ObjCmdProc HtmlEscapeUriComponent;
Tcl_ObjCmdProc HtmlResolveUri;
Tcl_ObjCmdProc HtmlCreateUri;

char *HtmlPropertyToString(CssProperty *, char **);

int HtmlStyleApply(HtmlTree *, HtmlNode *);
int HtmlStyleCounter(HtmlTree *, const char *);
int HtmlStyleCounters(HtmlTree *, const char *, int *, int);
void HtmlStyleHandleCounters(HtmlTree *, HtmlComputedValues *);

int HtmlLayout(HtmlTree *);
void HtmlLayoutMarkerBox(int, int, int, char *);

int HtmlStyleParse(HtmlTree*, Tcl_Obj*, Tcl_Obj*, Tcl_Obj*, Tcl_Obj*, Tcl_Obj*);
void HtmlTokenizerAppend(HtmlTree *, const char *, int, int);
int HtmlNameToType(void *, char *);
Html_u8 HtmlMarkupFlags(int);


#define HTML_WALK_ABANDON            4
#define HTML_WALK_DESCEND            5
#define HTML_WALK_DO_NOT_DESCEND     6
typedef int (*html_walk_tree_cb)(HtmlTree*,HtmlNode*,ClientData);
int HtmlWalkTree(HtmlTree*, HtmlNode *, html_walk_tree_cb, ClientData);

int HtmlTreeClear(HtmlTree *);
int         HtmlNodeNumChildren(HtmlNode *);
HtmlNode *  HtmlNodeBefore(HtmlNode *);
HtmlNode *  HtmlNodeAfter(HtmlNode *);
HtmlNode *  HtmlNodeRightSibling(HtmlNode *);
HtmlNode *  HtmlNodeLeftSibling(HtmlNode *);
char CONST *HtmlNodeTagName(HtmlNode *);
char CONST *HtmlNodeAttr(HtmlNode *, char CONST *);
char *      HtmlNodeToString(HtmlNode *);
HtmlNode *  HtmlNodeGetPointer(HtmlTree *, char CONST *);
int         HtmlNodeIsOrphan(HtmlNode *);

int HtmlNodeAddChild(HtmlElementNode *, int, const char *, HtmlAttributes *);
int HtmlNodeAddTextChild(HtmlNode *, HtmlTextNode *);

Html_u8     HtmlNodeTagType(HtmlNode *);

Tcl_Obj *HtmlNodeCommand(HtmlTree *, HtmlNode *pNode);
int HtmlNodeDeleteCommand(HtmlTree *, HtmlNode *pNode);

void HtmlDrawCleanup(HtmlTree *, HtmlCanvas *);
void HtmlDrawDeleteControls(HtmlTree *, HtmlCanvas *);

void HtmlDrawCanvas(HtmlCanvas*,HtmlCanvas*,int,int,HtmlNode*);
void HtmlDrawText(HtmlCanvas*,const char*,int,int,int,int,int,HtmlNode*,int);
void HtmlDrawTextExtend(HtmlCanvas*, int, int);
int HtmlDrawTextLength(HtmlCanvas*);

#define CANVAS_BOX_OPEN_LEFT    0x01      /* Open left-border */
#define CANVAS_BOX_OPEN_RIGHT   0x02      /* Open right-border */
HtmlCanvasItem *HtmlDrawBox(
HtmlCanvas *, int, int, int, int, HtmlNode *, int, int, HtmlCanvasItem *);
void HtmlDrawLine(HtmlCanvas *, int, int, int, int, int, HtmlNode *, int);

void HtmlDrawWindow(HtmlCanvas *, HtmlNode *, int, int, int, int, int);
void HtmlDrawBackground(HtmlCanvas *, XColor *, int);
void HtmlDrawQuad(HtmlCanvas*,int,int,int,int,int,int,int,int,XColor*,int);
int  HtmlDrawIsEmpty(HtmlCanvas *);

void HtmlDrawImage(HtmlCanvas*, HtmlImage2*, int, int, int, int, HtmlNode*, int);
void HtmlDrawOrigin(HtmlCanvas*);
void HtmlDrawCopyCanvas(HtmlCanvas*, HtmlCanvas*);

void HtmlDrawOverflow(HtmlCanvas*, HtmlNode*, int, int);

HtmlCanvasItem *HtmlDrawAddMarker(HtmlCanvas*, int, int, int);
int HtmlDrawGetMarker(HtmlCanvas*, HtmlCanvasItem *, int*, int*);

void HtmlDrawAddLinebox(HtmlCanvas*, int, int);
int HtmlDrawFindLinebox(HtmlCanvas*, int*, int*);

HtmlCanvasSnapshot *HtmlDrawSnapshotZero(HtmlTree *);
HtmlCanvasSnapshot *HtmlDrawSnapshot(HtmlTree *, int);
void HtmlDrawSnapshotDamage(HtmlTree*,HtmlCanvasSnapshot*,HtmlCanvasSnapshot**);
void HtmlDrawSnapshotFree(HtmlTree *, HtmlCanvasSnapshot *);

void HtmlDrawCanvasItemRelease(HtmlTree *, HtmlCanvasItem *);
void HtmlDrawCanvasItemReference(HtmlCanvasItem *);

void HtmlWidgetDamageText(HtmlTree *, HtmlNode *, int, HtmlNode *, int);
int HtmlWidgetNodeTop(HtmlTree *, HtmlNode *);
void HtmlWidgetOverflowBox(HtmlTree *, HtmlNode *, int *, int *, int *, int *);

HtmlTokenMap *HtmlMarkup(int);
CONST char * HtmlMarkupName(int);
char * HtmlMarkupArg(HtmlAttributes *, CONST char *, char *);

void HtmlFloatListAdd(HtmlFloatList*, int, int, int, int);
HtmlFloatList *HtmlFloatListNew();
void HtmlFloatListDelete();
int HtmlFloatListPlace(HtmlFloatList*, int, int, int, int);
int HtmlFloatListClear(HtmlFloatList*, int, int);
int HtmlFloatListClearTop(HtmlFloatList*, int);
void HtmlFloatListNormalize(HtmlFloatList*, int, int);
void HtmlFloatListMargins(HtmlFloatList*, int, int, int *, int *);
void HtmlFloatListLog(HtmlTree *, CONST char *, CONST char *, HtmlFloatList *);
int HtmlFloatListIsConstant(HtmlFloatList*, int, int);

HtmlPropertyCache * HtmlNewPropertyCache();
void HtmlSetPropertyCache(HtmlPropertyCache *, int, CssProperty *);
void HtmlAttributesToPropertyCache(HtmlNode *pNode);

Tcl_HashKeyType * HtmlCaseInsenstiveHashType();
Tcl_HashKeyType * HtmlFontKeyHashType();
Tcl_HashKeyType * HtmlComputedValuesHashType();

CONST char *HtmlDefaultTcl();
CONST char *HtmlDefaultCss();

/* Functions from htmlimage.c */
void HtmlImageServerInit(HtmlTree *);
void HtmlImageServerShutdown(HtmlTree *);
HtmlImage2 *HtmlImageServerGet(HtmlImageServer *, const char *);
HtmlImage2 *HtmlImageScale(HtmlImage2 *, int *, int *, int);
void HtmlImageSize(HtmlImage2 *, int *, int *);
Tcl_Obj *HtmlImageUnscaledName(HtmlImage2 *);
Tk_Image HtmlImageImage(HtmlImage2 *);
Tk_Image HtmlImageTile(HtmlImage2 *, int*, int *);
Pixmap HtmlImageTilePixmap(HtmlImage2 *, int*, int *);
Pixmap HtmlImagePixmap(HtmlImage2 *);
void HtmlImageFree(HtmlImage2 *);
void HtmlImageRef(HtmlImage2 *);
const char *HtmlImageUrl(HtmlImage2 *);
void HtmlImageCheck(HtmlImage2 *);
Tcl_Obj *HtmlXImageToImage(HtmlTree *, XImage *, int, int);
int HtmlImageAlphaChannel(HtmlImage2 *);

void HtmlImageServerSuspendGC(HtmlTree *);
void HtmlImageServerDoGC(HtmlTree *);
int HtmlImageServerCount(HtmlTree *);

void HtmlLayoutPaintNode(HtmlTree *, HtmlNode *);
void HtmlLayoutInvalidateCache(HtmlTree *, HtmlNode *);
void HtmlWidgetNodeBox(HtmlTree *, HtmlNode *, int *, int *, int *, int *);

void HtmlWidgetSetViewport(HtmlTree *, int, int, int);
void HtmlWidgetRepair(HtmlTree *, int, int, int, int, int);

int HtmlNodeClearStyle(HtmlTree *, HtmlElementNode *);
int HtmlNodeClearGenerated(HtmlTree *, HtmlElementNode *);
void HtmlNodeClearRecursive(HtmlTree *, HtmlNode *);

void HtmlTranslateEscapes(char *);
void HtmlRestackNodes(HtmlTree *pTree);
void HtmlDelStackingInfo(HtmlTree *, HtmlElementNode *);

#define HTML_TAG_ADD 10
#define HTML_TAG_REMOVE 11
#define HTML_TAG_SET 12
int HtmlTagAddRemoveCmd(ClientData, Tcl_Interp *, int, Tcl_Obj *CONST[], int);
Tcl_ObjCmdProc HtmlTagDeleteCmd;
Tcl_ObjCmdProc HtmlTagConfigureCmd;
void HtmlTagCleanupNode(HtmlTextNode *);
void HtmlTagCleanupTree(HtmlTree *);

Tcl_ObjCmdProc HtmlTextTextCmd;
Tcl_ObjCmdProc HtmlTextIndexCmd;
Tcl_ObjCmdProc HtmlTextBboxCmd;
Tcl_ObjCmdProc HtmlTextOffsetCmd;
void HtmlTextInvalidate(HtmlTree *);

void HtmlWidgetBboxText(
HtmlTree *, HtmlNode *, int, HtmlNode *, int, int *, int *, int *, int*);
int HtmlNodeScrollbarDoCallback(HtmlTree *, HtmlNode *);

void HtmlDelScrollbars(HtmlTree *, HtmlNode *);

HtmlAttributes * HtmlAttributesNew(int, char const **, int *, int);

void HtmlParseFragment(HtmlTree *, const char *);
void HtmlSequenceNodes(HtmlTree *);

void HtmlFontReference(HtmlFont *);
void HtmlFontRelease(HtmlTree *, HtmlFont *);

/* HTML Tokenizer function. */
int HtmlTokenize(HtmlTree *, char const *, int,
    void (*)(HtmlTree *, HtmlTextNode *, int),
    void (*)(HtmlTree *, int, const char *, HtmlAttributes *, int),
    void (*)(HtmlTree *, int, const char *, int)
);

/* The following three HtmlTreeAddXXX() functions - defined in htmltree.c - 
 * are invoked by the tokenizer (function HtmlTokenize()) when it is invoked 
 * to parse the main document (not a fragment).
 *
 * The functions invoked by the tokenizer when it is parsing a document
 * fragment are:
 *
 *     fragmentAddElement()
 *     fragmentAddText()
 *     fragmentAddClosingTag()
 *
 * These are also in htmltree.c but are not defined with external linkage.
 */
void HtmlTreeAddElement(HtmlTree *, int, const char *, HtmlAttributes *, int);
void HtmlTreeAddText(HtmlTree *, HtmlTextNode *, int);
void HtmlTreeAddClosingTag(HtmlTree *, int, const char *, int);

void HtmlInitTree(HtmlTree *);

void HtmlHashInit(void *, int);
HtmlTokenMap * HtmlHashLookup(void *, const char *zType);

/*******************************************************************
 * Interface to code in htmltext.c
 *******************************************************************/

/*
 * Creation, modification and deletion of HtmlTextNode objects.
 */
HtmlTextNode * HtmlTextNew(int, const char *, int, int);
void           HtmlTextSet(HtmlTextNode *, int, const char *, int, int);
void           HtmlTextFree(HtmlTextNode *);

/* The details of this structure should be considered private to
 * htmltext.c. They are here because other code needs to know the
 * size of the structure so that it may be allocated on the stack.
 */
struct HtmlTextIter {
    HtmlTextNode *pTextNode;
    int iText;
    int iToken;
};

/*
 * Functions for iterating through the tokens of a text node. Used as 
 * follows:
 *
 * HtmlTextNode *pText = ...
 * HtmlTextIter iter;
 *
 * for (
 *     HtmlTextIterFirst(&iter);
 *     HtmlTextIterIsValid(&iter);
 *     HtmlTextIterNext(&iter)
 * ) {
 *    ...
 * } 
 */
void HtmlTextIterFirst(HtmlTextNode *, HtmlTextIter *);
void HtmlTextIterNext(HtmlTextIter *);
int HtmlTextIterIsValid(HtmlTextIter *);
int HtmlTextIterIsLast(HtmlTextIter *);

/*
 * Query functions to discover the token type, length and data.
 *
 * If the token type is HTML_TEXT_TOKEN_TEXT, then the length 
 * is the number of bytes of text. The text is returned as the data.
 *
 * If the token type is SPACE or NEWLINE, then it is an error to
 * call HtmlTextTokenData(). TokenLength() returns the number of
 * consecutive space or newline characters that occured in the
 * document.
 *
 * If the token type is HARDNEWLINE, this must have come from CSS
 * generated content like this: "\A". In this case a newline will
 * be rendered no matter the value of the 'white-space' property 
 * and so on.
 */
int         HtmlTextIterType(HtmlTextIter *);
int         HtmlTextIterLength(HtmlTextIter *);
const char *HtmlTextIterData(HtmlTextIter *);

#ifdef NDEBUG
  #define HtmlCheckRestylePoint(x)
#else
  void HtmlCheckRestylePoint(HtmlTree *pTree);
#endif

/* Values returned by HtmlTextTokenType */
#define HTML_TEXT_TOKEN_TEXT          1
#define HTML_TEXT_TOKEN_SPACE         2
#define HTML_TEXT_TOKEN_NEWLINE       3
#define HTML_TEXT_TOKEN_HARDNEWLINE   4

/* These values are used internally by the htmltext.c module. They
 * should never be returned by HtmlTextTokenType(). But define them
 * here to make it easier to keep them distinct from the other
 * HTML_TEXT_* values defined above.
 */
#define HTML_TEXT_TOKEN_END       0
#define HTML_TEXT_TOKEN_LONGTEXT  5

/*
 * The following symbols are used for the built-in instrumentation 
 * function. (profile data).
 */
#define HTML_INSTRUMENT_SCRIPT_CALLBACK      0
#define HTML_INSTRUMENT_CALLBACK             1
#define HTML_INSTRUMENT_DYNAMIC_STYLE_ENGINE 2
#define HTML_INSTRUMENT_STYLE_ENGINE         3
#define HTML_INSTRUMENT_LAYOUT_ENGINE        4
#define HTML_INSTRUMENT_ALLOCATE_FONT        5

#define HTML_INSTRUMENT_NUM_SYMS             6
void HtmlInstrumentInit(Tcl_Interp *);
void HtmlInstrumentCall(ClientData, int, void(*)(ClientData), ClientData);
void *HtmlInstrumentCall2(ClientData, int, void*(*)(ClientData), ClientData);

/* htmltagdb.c */
const char *HtmlTypeToName(void *, int);

#endif

