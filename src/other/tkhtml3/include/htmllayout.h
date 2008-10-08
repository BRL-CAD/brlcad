
/*
 * htmllayout.h --
 *
 *     This header file is included by the files involved in layout only:
 *
 *         htmllayout.c
 *         htmllayoutinline.c
 *         htmllayouttable.c
 *
 *----------------------------------------------------------------------------
 */
#ifndef __HTML_LAYOUT_H
#define __HTML_LAYOUT_H

#include "html.h"

typedef struct LayoutContext LayoutContext;
typedef struct NodeList NodeList;

/*
 * A single Layout context object is allocated for use throughout
 * the entire layout process. It contains global resources required
 * by the drawing routines.
 *
 * TODO: It's tempting to combine this with HtmlTree. They are always 1-to-1.
 */
struct LayoutContext {
    HtmlTree *pTree;         /* The Html widget. */
    Tcl_Interp *interp;      /* The interpreter */

    HtmlComputedValues *pImplicitTableProperties;

    int minmaxTest;          /* Currently figuring out min/max widths */

    NodeList *pAbsolute;     /* List of nodes with "absolute" 'position' */
    NodeList *pFixed;        /* List of nodes with "fixed" 'position' */
};

/* Values for LayoutContext.minmaxTest */
#define MINMAX_TEST_MIN 1
#define MINMAX_TEST_MAX 2

typedef struct BoxProperties BoxProperties;
typedef struct MarginProperties MarginProperties;

struct BoxProperties {
    int iTop;         /* Pixels of border + pixels of padding at top */
    int iRight;       /* Pixels of border + pixels of padding at right */
    int iBottom;      /* Pixels of border + pixels of padding at bottom */
    int iLeft;        /* Pixels of border + pixels of padding at left */
};

struct MarginProperties {
    int margin_top;
    int margin_left;
    int margin_bottom;
    int margin_right;
    int leftAuto;        /* True if ('margin-left' == "auto") */
    int rightAuto;       /* True if ('margin-right' == "auto") */
    int topAuto;  
    int bottomAuto;
};


/*--------------------------------------------------------------------------*
 * htmllayoutinline.c --
 *
 *     htmllayoutinline.c contains the implementation of the InlineContext
 *     object.
 */
typedef struct InlineContext InlineContext;
typedef struct InlineBorder InlineBorder;

/* Allocate and deallocate InlineContext structures */
InlineContext *HtmlInlineContextNew(HtmlTree *,HtmlNode *, int, int);
void HtmlInlineContextCleanup(InlineContext *);

/* Add a text node to the inline context */
void HtmlInlineContextAddText(InlineContext*, HtmlNode *);

/* Add box (i.e. replaced) inline elements to the inline context */
void HtmlInlineContextAddBox(InlineContext*,HtmlNode*,HtmlCanvas*,int,int,int);

/* Test to see if the inline-context contains any more boxes */
int HtmlInlineContextIsEmpty(InlineContext *);

/* Retrieve the next line-box from an inline context */
int HtmlInlineContextGetLineBox(
LayoutContext *, InlineContext*,int,int*,HtmlCanvas*,int*, int*);

/* Manage inline borders and text-decoration */
InlineBorder *HtmlGetInlineBorder(LayoutContext*,InlineContext*,HtmlNode*);
int HtmlInlineContextPushBorder(InlineContext *, InlineBorder *);
void HtmlInlineContextPopBorder(InlineContext *, InlineBorder *);

HtmlNode *HtmlInlineContextCreator(InlineContext *);

/* End of htmllayoutinline.c interface
 *-------------------------------------------------------------------------*/

#define DRAW_CANVAS(a, b, c, d, e) \
HtmlDrawCanvas(a, b, c, d, e)
#define DRAW_WINDOW(a, b, c, d, e, f) \
HtmlDrawWindow(a, b, c, d, e, f, pLayout->minmaxTest)
#define DRAW_BACKGROUND(a, b) \
HtmlDrawBackground(a, b, pLayout->minmaxTest)
#define DRAW_QUAD(a, b, c, d, e, f, g, h, i, j) \
HtmlDrawQuad(a, b, c, d, e, f, g, h, i, j, pLayout->minmaxTest)

/* The following flags may be passed as the 4th argument to
 * HtmlInlineContextGetLineBox().
 */
#define LINEBOX_FORCELINE          0x01
#define LINEBOX_FORCEBOX           0x02
#define LINEBOX_CLOSEBORDERS       0x04


/*
 * A seperate BoxContext struct is used for each block box layed out.
 *
 *     HtmlTableLayout()
 */
typedef struct BoxContext BoxContext;
struct BoxContext {
    int iContaining;       /* DOWN: Width of containing block. */
    int iContainingHeight; /* DOWN: Height of containing block (may be AUTO). */
    int height;            /* UP: Generated box height. */
    int width;             /* UP: Generated box width. */
    HtmlCanvas vc;         /* UP: Canvas to draw the block on. */
};

void nodeGetBoxProperties(LayoutContext *, HtmlNode *, int, BoxProperties *);
void nodeGetMargins(LayoutContext *, HtmlNode *, int, MarginProperties *);

int  blockMinMaxWidth(LayoutContext *, HtmlNode *, int *, int *);

int getHeight(HtmlNode *, int, int);

/*--------------------------------------------------------------------------*
 * htmltable.c --
 *
 *     htmlTableLayout.c contains code to layout HTML/CSS tables.
 */
int HtmlTableLayout(LayoutContext*, BoxContext*, HtmlNode*);

/* End of htmlTableLayout.c interface
 *-------------------------------------------------------------------------*/

int HtmlLayoutNodeContent(LayoutContext *, BoxContext *, HtmlNode *);

void HtmlLayoutDrawBox(HtmlTree*,HtmlCanvas*,int,int,int,int,HtmlNode*,int,int);

/*
 *---------------------------------------------------------------------------
 *
 * unsigned char DISPLAY(HtmlComputedValues *); --
 *
 *     Return the value of the computed 'display' property from the
 *     HtmlComputedValues structure passed as an argument. Or, if NULL is
 *     passed, return CSS_CONST_INLINE. This is so that the 'display' property
 *     of a text node can be requested, even though a text node does not have
 *     computed properties.
 *
 * Results:
 *     Value of 'display' property.
 *
 * Side effects:
 *     None.
 *---------------------------------------------------------------------------
 */
#define DISPLAY(pV) ((pV) ? (pV)->eDisplay : CSS_CONST_INLINE)

#ifndef NDEBUG
  static void CHECK_INTEGER_PLAUSIBILITY(x) 
      int x; 
  {
      const static int limit = 10000000;
      assert(x < limit);
      assert(x > (limit * -1));
  }
#else
  #define CHECK_INTEGER_PLAUSIBILITY(x)
#endif

#endif
