/*
 * htmllayout.c --
 *
 *     This file contains code to layout a document for display using
 *     the CSS box model.
 *
 *     This is a rewrite of the layout engine from Tkhtml 2.0. Many 
 *     ideas from the original are reused. The main differences are
 *     that:
 *     
 *     * This version renders in terms of CSS properties, which are
 *       assigned to document nodes using stylesheets based on HTML
 *       attributes and tag types.
 *     * This version is written to the CSS specification with the
 *       goal of being "pixel-perfect". The HTML spec did not contain
 *       sufficient detail to do this when the original module was
 *       created.
 *
 *--------------------------------------------------------------------------
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
 *     * Neither the name of Eolas Technologies Inc. nor the names of its
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
static const char rcsid[] = "$Id: htmllayout.c,v 1.270 2008/01/07 04:48:02 danielk1977 Exp $";

#include "htmllayout.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define LOG(X) if ( \
(X)->iNode >= 0 && pLayout->pTree->options.logcmd && 0 == pLayout->minmaxTest)

/* Used for debugging the layout cache. Some output appears on stdout. */
/* #define LAYOUT_CACHE_DEBUG */

/*
 * The code to lay out a "normal-flow" is located in this file:
 *
 * A new normal flow is established by:
 *     - the root node of the document,
 *     - a floating box,
 *     - a table cell.
 */

typedef struct NormalFlowCallback NormalFlowCallback;
typedef struct NormalFlow NormalFlow;
typedef struct LayoutCache LayoutCache;

/*
 * This structure is (fairly obviously) used to link node structures into a
 * linked list. This is used as part of the process to layout a node with the
 * 'position' property set to "absolute".
 */
struct NodeList {
    HtmlNode *pNode;
    NodeList *pNext;
    HtmlCanvasItem *pMarker;       /* Static position marker */
};

/*
 * The iMaxMargin, iMinMargin and isValid variables are used to manage
 * collapsing vertical margins. Each margin that collapses at a given
 * point in the vertical flow is added to the structure using
 * normalFlowMarginAdd(). iMaxMargin stores the largest positive value seen 
 * (or zero) and iMinMargin stores the largest negative value seen (or zero).
 */
struct NormalFlow {
    int iMaxMargin;          /* Most positive margin value in pixels */
    int iMinMargin;          /* Most negative margin value in pixels */
    int isValid;             /* True if iMaxMargin and iMinMargin are valid */
    int nonegative;          /* Do not return negative from Collapse() */
    NormalFlowCallback *pCallbackList;
    HtmlFloatList *pFloat;   /* Floating margins */
};

struct NormalFlowCallback {
    void (*xCallback)(NormalFlow *, NormalFlowCallback *, int);
    ClientData clientData;
    NormalFlowCallback *pNext;
};

struct LayoutCache {
    /* Cached input values for normalFlowLayout() */
    NormalFlow normalFlowIn;
    int iContaining;
    int iFloatLeft;
    int iFloatRight;

    /* Cached output values for normalFlowLayout() */
    NormalFlow normalFlowOut;
    int iWidth;
    int iHeight;
    HtmlCanvas canvas;
  
    /* If not PIXELVAL_AUTO, value for normal-flow callbacks */
    int iMarginCollapse;
};

struct HtmlLayoutCache {
    unsigned char flags;     /* Mask indicating validity of aCache[] entries */
    LayoutCache aCache[3];
    int iMinWidth;
    int iMaxWidth;
};
#define CACHED_MINWIDTH_OK ((int)1<<3)
#define CACHED_MAXWIDTH_OK ((int)1<<4)


/*
 * Public functions:
 *
 *     HtmlLayoutInvalidateCache
 *     HtmlLayout
 *
 * Functions declared in htmllayout.h:
 *
 *     HtmlLayoutNodeContent
 *
 *     blockMinMaxWidth
 *     nodeGetMargins
 *     nodeGetBoxProperties
 */

/*
 * These are prototypes for all the static functions in this file. We
 * don't need most of them, but the help with error checking that normally
 * wouldn't happen because of the old-style function declarations. Also
 * they function as a table of contents for this file.
 */

static int inlineLayoutDrawLines
    (LayoutContext*,BoxContext*,InlineContext*,int,int*, NormalFlow*);

/* 
 * nodeIsReplaced() returns a boolean value, true if the node should be
 * layed out as if it were a replaced object (i.e. if there is a value for
 * '-tkhtml-replacement-image' or a replaced window).
 */
static int nodeIsReplaced(HtmlNode *);


typedef int (FlowLayoutFunc) (
    LayoutContext *, 
    BoxContext *, 
    HtmlNode *, 
    int *, 
    InlineContext *, 
    NormalFlow *
);

static void normalFlowLayout(LayoutContext*,BoxContext*,HtmlNode*,NormalFlow*);

static FlowLayoutFunc normalFlowLayoutNode;
static FlowLayoutFunc normalFlowLayoutFloat;
static FlowLayoutFunc normalFlowLayoutBlock;
static FlowLayoutFunc normalFlowLayoutReplaced;
static FlowLayoutFunc normalFlowLayoutTable;
static FlowLayoutFunc normalFlowLayoutText;
static FlowLayoutFunc normalFlowLayoutInline;
static FlowLayoutFunc normalFlowLayoutInlineReplaced;
static FlowLayoutFunc normalFlowLayoutAbsolute;
static FlowLayoutFunc normalFlowLayoutOverflow;

/* Manage collapsing vertical margins in a normal-flow */
static void normalFlowMarginCollapse(LayoutContext*,HtmlNode*,NormalFlow*,int*);
static void normalFlowMarginAdd(LayoutContext *, HtmlNode *, NormalFlow *, int);
static int  normalFlowMarginQuery(NormalFlow *);

/* Hooks to attach the list-marker drawing callback to a NormalFlow */
static void normalFlowCbAdd(NormalFlow *, NormalFlowCallback *);
static void normalFlowCbDelete(NormalFlow *, NormalFlowCallback *);

/* Utility function to format an integer as a roman number (I, II, etc.). */
static void getRomanIndex(char *, int, int);

/* Function to invoke the -configurecmd of a replaced object. */
static void doConfigureCmd(HtmlTree *, HtmlElementNode *, int);

static void drawReplacement(LayoutContext*, BoxContext*, HtmlNode*);
static void drawReplacementContent(LayoutContext*, BoxContext*, HtmlNode*);

/* Add a border to a rendered content area */
static void wrapContent(LayoutContext*, BoxContext*, BoxContext*, HtmlNode*);

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetBoxProperties --
 *    
 *     Calculate and return the border and padding widths of a node as exact
 *     pixel values based on the width of the containing block (parameter
 *     iContaining) and the computed values of the following properties:
 *
 *         'padding'
 *         'border-width'
 *         'border-style'
 *
 *     Four non-negative integer pixel values are returned, the sum of the
 *     padding and border pixel widths for the top, right, bottom and left
 *     sides of the box.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     The four calculated values are written into *pBoxProperties before
 *     returning.
 *
 *---------------------------------------------------------------------------
 */
void 
nodeGetBoxProperties(pLayout, pNode, iContaining, pBoxProperties)
    LayoutContext *pLayout;            /* Unused */
    HtmlNode *pNode;                   /* Node to calculate values for */
    int iContaining;                   /* Width of pNode's containing block */
    BoxProperties *pBoxProperties;     /* OUT: Write pixel values here */
{
    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);

    /* Under some circumstance, a negative value may be passed for iContaining.
     * If this happens, use 0 as the containing width when calculating padding
     * widths with computed percentage values. Otherwise we will return a
     * negative padding width, which is illegal.
     *
     * Also, if we are running a min-max text, percentage widths are zero.
     */
    int c = iContaining;
    if (pLayout->minmaxTest || c < 0) {
        c = 0;
    }

    assert(pV);
    pBoxProperties->iTop    = PIXELVAL(pV, PADDING_TOP, c);
    pBoxProperties->iRight  = PIXELVAL(pV, PADDING_RIGHT, c);
    pBoxProperties->iBottom = PIXELVAL(pV, PADDING_BOTTOM, c);
    pBoxProperties->iLeft   = PIXELVAL(pV, PADDING_LEFT, c);

    /* For each border width, use the computed value if border-style is
     * something other than 'none', otherwise use 0. The PIXELVAL macro is not
     * used because 'border-width' properties may not be set to % values.
     */
    pBoxProperties->iTop += (
        (pV->eBorderTopStyle != CSS_CONST_NONE) ? pV->border.iTop : 0);
    pBoxProperties->iRight += (
        (pV->eBorderRightStyle != CSS_CONST_NONE) ? pV->border.iRight : 0);
    pBoxProperties->iBottom += (
        (pV->eBorderBottomStyle != CSS_CONST_NONE) ? pV->border.iBottom : 0);
    pBoxProperties->iLeft += (
        (pV->eBorderLeftStyle != CSS_CONST_NONE) ?  pV->border.iLeft : 0);

    assert(
        pBoxProperties->iTop >= 0 &&
        pBoxProperties->iRight >= 0 &&
        pBoxProperties->iBottom >= 0 &&
        pBoxProperties->iLeft >= 0 
    );
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeGetMargins --
 *
 *     Get the margin properties for a node.
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
nodeGetMargins(pLayout, pNode, iContaining, pMargins)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int iContaining;
    MarginProperties *pMargins;
{
    int iMarginTop;
    int iMarginRight;
    int iMarginBottom;
    int iMarginLeft;

    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);
    assert(pV);

    /* Margin properties do not apply to table cells or rows. So return zero
     * for all. We have to do this sort of thing here in the layout engine, not
     * in the styler code, because a table-cell can still have a computed value
     * of a margin property (which may be inherited from), just not an actual
     * one.
     */
    if (
        pV->eDisplay == CSS_CONST_TABLE_CELL ||
        pV->eDisplay == CSS_CONST_TABLE_ROW
    ) {
       memset(pMargins, 0, sizeof(MarginProperties));
       return;
    }

    /* If we are running a min-max text, percentage widths are zero. */
    if (pLayout->minmaxTest) {
        iContaining = 0;
    }

    iMarginTop =    PIXELVAL(pV, MARGIN_TOP, iContaining);
    iMarginRight =  PIXELVAL(pV, MARGIN_RIGHT, iContaining);
    iMarginBottom = PIXELVAL(pV, MARGIN_BOTTOM, iContaining);
    iMarginLeft =   PIXELVAL(pV, MARGIN_LEFT, iContaining);

    pMargins->margin_top = ((iMarginTop > MAX_PIXELVAL)?iMarginTop:0);
    pMargins->margin_bottom = ((iMarginBottom > MAX_PIXELVAL)?iMarginBottom:0);
    pMargins->margin_left = ((iMarginLeft > MAX_PIXELVAL)?iMarginLeft:0);
    pMargins->margin_right = ((iMarginRight > MAX_PIXELVAL)?iMarginRight:0);

    pMargins->leftAuto = ((iMarginLeft == PIXELVAL_AUTO) ? 1 : 0);
    pMargins->rightAuto = ((iMarginRight == PIXELVAL_AUTO) ? 1 : 0);
    pMargins->topAuto = ((iMarginTop == PIXELVAL_AUTO) ? 1 : 0);
    pMargins->bottomAuto = ((iMarginBottom == PIXELVAL_AUTO) ? 1 : 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowMarginCollapse --
 * normalFlowMarginQuery --
 * normalFlowMarginAdd --
 *
 *     The following three functions are used to manage collapsing vertical
 *     margins within a normal flow.
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
normalFlowMarginQuery(pNormal) 
    NormalFlow *pNormal;
{
    int iMargin = pNormal->iMinMargin + pNormal->iMaxMargin;
    if (pNormal->nonegative) {
        iMargin = MAX(0, iMargin);
    }
    return iMargin;
}
static void 
normalFlowMarginCollapse(pLayout, pNode, pNormal, pY) 
    LayoutContext *pLayout;
    HtmlNode *pNode;
    NormalFlow *pNormal;
    int *pY;
{
    NormalFlowCallback *pCallback = pNormal->pCallbackList;
    int iMargin = pNormal->iMinMargin + pNormal->iMaxMargin;
    if (pNormal->nonegative) {
        iMargin = MAX(0, iMargin);
    }
    while (pCallback) {
        pCallback->xCallback(pNormal, pCallback, iMargin);
        pCallback = pCallback->pNext;
    }
    *pY += iMargin;

    assert(pNormal->isValid || (!pNormal->iMaxMargin && !pNormal->iMaxMargin));
    pNormal->isValid = 1;
    pNormal->iMaxMargin = 0;
    pNormal->iMinMargin = 0;
    pNormal->nonegative = 0;

    LOG(pNode) {
        HtmlTree *pTree = pLayout->pTree;
        char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlLog(pTree, "LAYOUTENGINE", "%s normalFlowMarginCollapse()"
            "<p>Margins collapse to: %dpx", zNode, iMargin
		, NULL);
    }
}
static void 
normalFlowMarginAdd(pLayout, pNode, pNormal, y) 
    LayoutContext *pLayout;
    HtmlNode *pNode;
    NormalFlow *pNormal;
    int y;
{
    if (pNormal->isValid && (!pNormal->nonegative || y >= 0)) {
        assert(pNormal->iMaxMargin >= 0);
        assert(pNormal->iMinMargin <= 0);
        pNormal->iMaxMargin = MAX(pNormal->iMaxMargin, y);
        pNormal->iMinMargin = MIN(pNormal->iMinMargin, y);
    }
    LOG(pNode) {
        HtmlTree *pTree = pLayout->pTree;
        char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlLog(pTree, "LAYOUTENGINE", "%s normalFlowMarginAdd()"
            "<p>Add margin of %dpx" 
            "<ul><li>positive-margin = %dpx"
            "    <li>negative-margin = %dpx"
            "    <li>is-valid = %s"
            "    <li>no-negative = %s",
            zNode, y, pNormal->iMaxMargin, pNormal->iMinMargin,
            pNormal->isValid ? "true" : "false",
            pNormal->nonegative ? "true" : "false"
		, NULL);
    }
}

static void 
normalFlowCbAdd(pNormal, pCallback)
    NormalFlow *pNormal;
    NormalFlowCallback *pCallback;
{
    pCallback->pNext = pNormal->pCallbackList;
    pNormal->pCallbackList = pCallback;
}
static void 
normalFlowCbDelete(pNormal, pCallback)
    NormalFlow *pNormal;
    NormalFlowCallback *pCallback;
{
    NormalFlowCallback *pCallbackList = pNormal->pCallbackList;
    if (pCallback == pCallbackList) {
        pNormal->pCallbackList = pCallback->pNext;
    } else {
        NormalFlowCallback *p;
        for (p = pCallbackList; p && p->pNext != pCallback; p = p->pNext);
        if (p) {
            assert(p->pNext && p->pNext == pCallback);
            p->pNext = p->pNext->pNext;
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeIsReplaced --
 *
 *     Return true if the node should be handled as a replaced node, 
 *     false otherwise. A node is handled as a replaced node if a 
 *     replacement widget has been supplied via [node replace], or
 *     if the custom -tkhtml-replacement-image property is defined.
 *
 * Results:
 *     1 or 0.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
nodeIsReplaced(pNode)
    HtmlNode *pNode;
{
    HtmlElementNode *pElem = HtmlNodeAsElement(pNode);
    assert(!pElem || pElem->pPropertyValues);
    return ((
        pElem && (
            (pElem->pReplacement && pElem->pReplacement->win) ||
            (pElem->pPropertyValues->imReplacementImage != 0)
        )
    ) ? 1 : 0);
}

static void
considerMinMaxHeight(pNode, iContaining, piHeight)
    HtmlNode *pNode;
    int iContaining;
    int *piHeight;
{
    int iHeight = *piHeight;
    if (iHeight != PIXELVAL_AUTO) {
        HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);
        int iMinHeight;
        int iMaxHeight;
 
        iMinHeight = PIXELVAL(pV, MIN_HEIGHT, iContaining);
        iMaxHeight = PIXELVAL(pV, MAX_HEIGHT, iContaining);

        if (iMinHeight < MAX_PIXELVAL) iMinHeight = 0;
        if (iMaxHeight < MAX_PIXELVAL) iMaxHeight = PIXELVAL_NONE;
 
        if (iMaxHeight != PIXELVAL_NONE) {
            iHeight = MIN(iHeight, iMaxHeight);
        }
        iHeight = MAX(iHeight, iMinHeight);

        *piHeight = iHeight;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * getHeight --
 *
 *     The second parameter is the content-height in pixels of node pNode. 
 *     This function returns the actual pixel height of the node content
 *     area considering both the supplied value and the computed value
 *     of the height property:
 *
 *         IF ('height' == "auto") 
 *             return iHeight
 *         ELSE
 *             return 'height'
 *
 * Results:
 *     See above.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int
getHeight(pNode, iHeight, iContainingHeight)
    HtmlNode *pNode;             /* Node to determine height of */
    int iHeight;                 /* Natural Content height */
    int iContainingHeight;       /* Containing height, or PIXELVAL_AUTO */
{
    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);

    int height = PIXELVAL(pV, HEIGHT, iContainingHeight);
    if (height == PIXELVAL_AUTO) {
        height = iHeight;
    }

    considerMinMaxHeight(pNode, iContainingHeight, &height);
    return height;
}

static int
getWidth(iWidthCalculated, iWidthContent) 
    int iWidthCalculated;
    int iWidthContent;
{
    if (iWidthCalculated < 0) {
        return iWidthContent;
    }
    return iWidthCalculated;
}

static int
getWidthProperty(pLayout, pComputed, iContaining) 
    LayoutContext *pLayout;
    HtmlComputedValues *pComputed;
    int iContaining;
{
    return PIXELVAL(
        pComputed, WIDTH, pLayout->minmaxTest ? PIXELVAL_AUTO : iContaining
    );
}

/*
 *---------------------------------------------------------------------------
 *
 * considerMinMaxWidth --
 *
 * Results:
 *     See above.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
considerMinMaxWidth(pNode, iContaining, piWidth)
    HtmlNode *pNode;
    int iContaining;
    int *piWidth;
{
    int iWidth = *piWidth;
    if (iWidth != PIXELVAL_AUTO) {
        HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);
        int iMinWidth;
        int iMaxWidth;
 
        iMinWidth = PIXELVAL(pV, MIN_WIDTH, iContaining);
        iMaxWidth = PIXELVAL(pV, MAX_WIDTH, iContaining);

        assert(iMaxWidth == PIXELVAL_NONE || iMaxWidth >= MAX_PIXELVAL);
        assert(iMinWidth >= MAX_PIXELVAL);
 
        if (iMaxWidth != PIXELVAL_NONE) {
            iWidth = MIN(iWidth, iMaxWidth);
        }
        iWidth = MAX(iWidth, iMinWidth);

        *piWidth = iWidth;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * doHorizontalBlockAlign --
 *
 *     This function is used to horizontally align a block box in the
 *     normal flow. i.e. a table or regular block (including blocks with
 *     the 'overflow' property set).
 * 
 *     The first three argument are the layout-context, node object and
 *     the MarginProperties structure associated with the node (the
 *     margin-properties can be derived from the node, but it's slightly more
 *     efficient to pass them in, as anyone calling this function requires
 *     them anyway). The final argument is the number of "spare" pixels
 *     that will be placed to the left or the right of the box generated
 *     by pNode. For example, if pNode generates a box 500px wide (including
 *     horizontal padding, borders and margins) and is being layed out
 *     in a normal-flow context 800px wide, iSpareWidth should be 300.
 *
 * Results:
 *     The value returned is the number of pixels that should be added
 *     to the left of the box generated by pNode when it is layed out
 *     into it's context. In the example above (500px and 800px), if the
 *     box should be centered, 150 is returned. If it should be left-aligned,
 *     0. If it should be right-aligned, 300.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
doHorizontalBlockAlign(pLayout, pNode, pMargin, iSpareWidth)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    MarginProperties *pMargin;
    int iSpareWidth;
{
    int iRet = 0;

    /* Do not do horizontal-alignment of blocks when figuring out minimum
     * or maximum widths. It just confuses everybody.
     */
    if (pLayout->minmaxTest) return 0;
    if (iSpareWidth <= 0) return 0;

    if (pMargin->leftAuto && pMargin->rightAuto) {
        /* Both horizontal margins are "auto". Center the block. */
        iRet = iSpareWidth / 2;
    } else if (pMargin->leftAuto) {
        /* The 'margin-left' property is "auto" and 'margin-right' is not.
         * The block is therefore right-aligned.
         */
        iRet = iSpareWidth;
    } else if (!pMargin->rightAuto) {
        /* If neither margin is set to "auto", then consider the following
         * special values of the 'text-align' attribute on the parent 
         * block:
         * 
         *     "-tkhtml-right"
         *     "-tkhtml-center"
         *
         * These may be set by the default stylesheet (html.css file in the 
         * src directory) in response to a <div align="center|right">
         * construction (or a <center> element).  This is non-standard stuff
         * inherited from Netscape 4. Gecko and webkit use "-moz-center" and
         * "-webkit-center" respectively.
         */
        HtmlNode *pParent = HtmlNodeParent(pNode);
        if (pParent) {
            switch (HtmlNodeComputedValues(pParent)->eTextAlign) {
                case CSS_CONST__TKHTML_CENTER:
                    iRet = iSpareWidth / 2;
                    break;
                case CSS_CONST__TKHTML_RIGHT:
                    iRet = iSpareWidth;
                   break;
            }
        }
    }

    return iRet;
}

#define SCROLLBAR_WIDTH 15

/*
 *---------------------------------------------------------------------------
 *
 * createScrollbars --
 *
 *     This function creates scrollbar widgets to scroll the content
 *     generated by pNode. If argument "v" is non-zero, a vertical scrollbar
 *     is created. If "h" is non-zero, a horizontal scrollbar is also
 *     created.
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
createScrollbars(pTree, pNode, iWidth, iHeight, iHorizontal, iVertical)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int iWidth;
    int iHeight;
    int iHorizontal;
    int iVertical;
{
    HtmlElementNode *pElem = (HtmlElementNode *)pNode;
    HtmlNodeScrollbars *p;
    if (HtmlNodeIsText(pNode) || !pElem->pScrollbar) return;
    p = pElem->pScrollbar;

    p->iWidth = iWidth;
    p->iHeight = iHeight;
    p->iVerticalMax = iVertical;
    p->iHorizontalMax = iHorizontal;

    if (iVertical > 0) {
        if (!p->vertical.win) {
            char zTmp[256];
            Tcl_Obj *pName;
            Tk_Window win;
            snprintf(zTmp, 255, "::tkhtml::vscrollbar %s %s",
                Tk_PathName(pTree->docwin), 
                Tcl_GetString(HtmlNodeCommand(pTree, pNode))
            );
            zTmp[255] = '\0';
            Tcl_Eval(pTree->interp, zTmp);
            pName = Tcl_GetObjResult(pTree->interp);
            win = Tk_NameToWindow(
                pTree->interp, Tcl_GetString(pName), pTree->tkwin
            );
            assert(win);
            Tcl_IncrRefCount(pName);
            p->vertical.pReplace = pName;
            p->vertical.win = win;
        }
        p->vertical.iWidth = SCROLLBAR_WIDTH;
        p->vertical.iHeight = iHeight;
    }
    if (iHorizontal > 0) {
        if (!p->horizontal.win) {
            char zTmp[256];
            Tcl_Obj *pName;
            Tk_Window win;
            snprintf(zTmp, 255, "::tkhtml::hscrollbar %s %s",
                Tk_PathName(pTree->docwin), 
                Tcl_GetString(HtmlNodeCommand(pTree, pNode))
            );
            zTmp[255] = '\0';
            Tcl_Eval(pTree->interp, zTmp);
            pName = Tcl_GetObjResult(pTree->interp);
            win = Tk_NameToWindow(
                pTree->interp, Tcl_GetString(pName), pTree->tkwin
            );
            assert(win);
            Tcl_IncrRefCount(pName);
            p->horizontal.pReplace = pName;
            p->horizontal.win = win;
        }
        p->horizontal.iWidth = iWidth;
        p->horizontal.iWidth -= p->vertical.iWidth;
        p->horizontal.iHeight = SCROLLBAR_WIDTH;
    }
    p->iHorizontalMax += p->vertical.iWidth;
    p->iVerticalMax += p->horizontal.iHeight;

    HtmlNodeScrollbarDoCallback(pTree, pNode);
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutOverflow --
 *
 *     This FlowLayoutFunc is called to handle an in-flow block with the
 *     'overflow' property set to something other than "visible" - i.e.
 *     "scroll", "hidden" or "auto".
 *
 *     At least some of the code in this function should be combined with
 *     that in normalFlowLayoutBlock() (and possibly normalFlowLayoutTable()
 *     for that matter).
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
normalFlowLayoutOverflow(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);
    int eOverflow = pV->eOverflow;

    MarginProperties margin;
    BoxProperties box;
    BoxContext sBox;
    BoxContext sContent;
    HtmlFloatList *pFloat = pNormal->pFloat;
    int y;
    int iMPB;              /* Horizontal margins, padding, borders */
    int iLeft;             /* Left floating margin where box is drawn */
    int iRight;            /* Right floating margin where box is drawn */
    int iMinContentWidth;  /* Minimum content width */
    int iMin;              /* Minimum width as used for vertical positioning */
    int iSpareWidth;

    int iComputedWidth;    /* Return value of getWidthProperty() */
    int iWidth;            /* Content width of box */
    int iHeight;           /* Content height of box */

    int useVertical = 0;   /* True to use a vertical scrollbar */
    int useHorizontal = 0; /* True to use a horizontal scrollbar */

    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);
    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    iComputedWidth = getWidthProperty(pLayout, pV, pBox->iContaining);
    iWidth = iComputedWidth;

    iMPB = margin.margin_left + margin.margin_right + box.iLeft + box.iRight;

    /* Collapse the vertical margins above this box. */
    normalFlowMarginAdd(pLayout, pNode, pNormal, margin.margin_top);
    normalFlowMarginCollapse(pLayout, pNode, pNormal, pY);

    /* Do vertical positioning. (i.e. move the block downwards to accomadate
     * the minimum or specified width if there are floating margins). 
     * After this block has executed, variable iWidth contains the number
     * of pixels between the horizontal padding edges of the box. If a 
     * vertical scrollbar is used, it has to fit into this space along with
     * the content.
     */
    iMin = iWidth;
    blockMinMaxWidth(pLayout, pNode, &iMinContentWidth, 0);
    if (iWidth == PIXELVAL_AUTO) {
        iMin = iMinContentWidth;
    }
    iMin += iMPB;
    y = HtmlFloatListPlace(pFloat, pBox->iContaining, iMin, 1000, *pY);
    iLeft = 0;
    iRight = pBox->iContaining;
    HtmlFloatListMargins(pFloat, y, y+1000, &iLeft, &iRight);
    if (iWidth == PIXELVAL_AUTO) {
        iWidth = iRight - iLeft - iMPB;
    }
    considerMinMaxWidth(pNode, pBox->iContaining, &iWidth);

    /* Set variable iHeight to the specified height of the block. If
     * there is no specified height "height:auto", set iHeight to 
     * PIXELVAL_AUTO.
     */
    iHeight = PIXELVAL(pV, HEIGHT, pBox->iContainingHeight);
   
    /* Figure out whether or not this block uses a vertical scrollbar. */
    if (eOverflow == CSS_CONST_SCROLL) {
        useVertical = 1;
    } else if (eOverflow == CSS_CONST_AUTO && iHeight != PIXELVAL_AUTO) {
        memset(&sContent, 0, sizeof(BoxContext));
        sContent.iContaining = iWidth;
        sContent.iContainingHeight = iHeight;
        HtmlLayoutNodeContent(pLayout, &sContent, pNode);
        if ((sContent.height + SCROLLBAR_WIDTH) > iHeight) {
            useVertical = 1;
        }
        HtmlDrawCleanup(pLayout->pTree, &sContent.vc);
    }

    /* Figure out whether or not this block uses a horizontal scrollbar. */
    if (eOverflow == CSS_CONST_SCROLL || (
            eOverflow == CSS_CONST_AUTO && 
            iMinContentWidth > (iWidth - (useVertical ? SCROLLBAR_WIDTH : 0))
        )
    ) {
        useHorizontal = 1;
    }
   
    memset(&sBox, 0, sizeof(BoxContext));
    memset(&sContent, 0, sizeof(BoxContext));
    sContent.iContaining = iWidth - (useVertical?SCROLLBAR_WIDTH:0);
    sContent.iContainingHeight = iHeight;
    if (iHeight != PIXELVAL_AUTO) {
        sContent.iContainingHeight -= (useHorizontal ? SCROLLBAR_WIDTH : 0);
    }
    HtmlLayoutNodeContent(pLayout, &sContent, pNode);
    sContent.height = getHeight(pNode, sContent.height, iHeight);
    if (!pLayout->minmaxTest) {
        sContent.width = iWidth;
    } else if (iComputedWidth >= 0) {
        sContent.width = iComputedWidth;
    }
   
    LOG(pNode) {
        HtmlTree *pTree = pLayout->pTree;
        char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlLog(pTree, "LAYOUTENGINE", "%s normalFlowLayoutOverflow()"
            "containing width for content = %dpx",
            zNode, sContent.iContaining
		, NULL);
    }

    if (
        pLayout->minmaxTest == 0 && (
            pV->eOverflow == CSS_CONST_SCROLL || 
            (pV->eOverflow == CSS_CONST_AUTO && (useHorizontal || useVertical)
    ))) {
        HtmlElementNode *pElem = (HtmlElementNode *)pNode;
        if (pElem->pScrollbar == 0) {
            pElem->pScrollbar = HtmlNew(HtmlNodeScrollbars);
        }
        createScrollbars(pLayout->pTree, pNode, 
            sContent.width, sContent.height,
            useHorizontal ? sContent.vc.right : -1, 
            useVertical ? sContent.vc.bottom : -1
        );
    }

    /* Wrap an overflow primitive around the content of this box. This
     * has to be done after the createScrollbars() call above, because
     * it modifies the sContent.vc.right and sContent.vc.bottom variables.
     */
    if (
        sContent.width < sContent.vc.right ||
        sContent.height < sContent.vc.bottom
    ) {
        HtmlDrawOverflow(&sContent.vc, pNode, sContent.width, sContent.height);
    }

    sBox.iContaining = pBox->iContaining;
    wrapContent(pLayout, &sBox, &sContent, pNode);

    iSpareWidth = iRight - iLeft - sBox.width;
    iLeft += doHorizontalBlockAlign(pLayout, pNode, &margin, iSpareWidth);

    DRAW_CANVAS(&pBox->vc, &sBox.vc, iLeft, y, pNode);

    *pY = y + sBox.height;
    pBox->width = MAX(pBox->width, sBox.width);
    pBox->height = MAX(pBox->height, *pY);

    normalFlowMarginAdd(pLayout, pNode, pNormal, margin.margin_bottom);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutFloat --
 *
 *     This FlowLayoutFunc is called to add a floating box to a normal 
 *     flow. pNode is the node with the 'float' property set.
 *
 *     Parameter pY is a pointer to the current Y coordinate in pBox.
 *     The top-edge of the floating box is drawn at or below *pY. Unlike
 *     other normalFlowLayoutXXX() functions, *pY is never modified by
 *     this function. Floating boxes do not increase the current Y 
 *     coordinate.
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
normalFlowLayoutFloat(pLayout, pBox, pNode, pY, pDoNotUse, pNormal)
    LayoutContext *pLayout;   /* Layout context */
    BoxContext *pBox;         /* Containing box context */
    HtmlNode *pNode;          /* Node that generates floating box */
    int *pY;                  /* IN/OUT: y-coord to draw float at */
    InlineContext *pDoNotUse; /* Unused by this function */
    NormalFlow *pNormal;
{
    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);
    int eFloat = pV->eFloat;
    int iContaining = pBox->iContaining;
    HtmlFloatList *pFloat = pNormal->pFloat;

    int iTotalHeight;        /* Height of floating box (incl. margins) */
    int iTotalWidth;         /* Width of floating box (incl. margins) */

    int x, y;                /* Coords for content to be drawn */
    int iLeft;               /* Left floating margin where box is drawn */
    int iRight;              /* Right floating margin where box is drawn */
    int iTop;                /* Top of top margin of box */

    MarginProperties margin; /* Margin properties of pNode */
    BoxContext sBox;         /* Box context for content to be drawn into */

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = iContaining;

    if (pLayout->minmaxTest) {
        eFloat = CSS_CONST_LEFT;
    }

    /* The following two lines set variable 'y' to the y-coordinate for the top
     * outer-edge of the floating box. This might be increased (but not
     * decreased) if the existing floating-margins at y are too narrow for this
     * floating box.
     *
     * If the 'clear' property is set to other than "none", this is handled
     * by adding the following constraint (from CSS 2.1, section 9.5.2):
     *
     *     The top outer edge of the float must be below the bottom outer edge
     *     of all earlier left-floating boxes (in the case of 'clear: left'),
     *     or all earlier right-floating boxes (in the case of 'clear: right'),
     *     or both ('clear: both').
     *
     * Note: "outer-edge" means including the the top and bottom margins.
     */
    y = (*pY);
    y += normalFlowMarginQuery(pNormal);
    pBox->height = MAX(pBox->height, *pY);
    y = HtmlFloatListClear(pNormal->pFloat, pV->eClear, y);
    y = HtmlFloatListClearTop(pNormal->pFloat, y);

    nodeGetMargins(pLayout, pNode, iContaining, &margin);

    /* The code that calculates computed values (htmlprop.c) should have
     * ensured that all floating boxes have a 'display' value of "block",
     * "table" or "list-item".
     */
    assert(
      DISPLAY(pV) == CSS_CONST_BLOCK || 
      DISPLAY(pV) == CSS_CONST_TABLE ||
      DISPLAY(pV) == CSS_CONST_LIST_ITEM
    );
    assert(eFloat == CSS_CONST_LEFT || eFloat == CSS_CONST_RIGHT);

    /* Draw the floating element to sBox. The procedure for determining the
     * width to use for the element is described in sections 10.3.5
     * (non-replaced) and 10.3.6 (replaced) of the CSS 2.1 spec.
     */
    if (nodeIsReplaced(pNode)) {
        /* For a replaced element, the drawReplacement() function takes care of
         * calculating the actual width and height, and of drawing borders
         * etc. As usual horizontal margins are included, but vertical are not.
         */
        CHECK_INTEGER_PLAUSIBILITY(sBox.vc.bottom);
        drawReplacement(pLayout, &sBox, pNode);
        CHECK_INTEGER_PLAUSIBILITY(sBox.vc.bottom);
    } else {
        /* A non-replaced element. */
        BoxProperties box;   /* Box properties of pNode */
        BoxContext sContent;
        int c = pLayout->minmaxTest ? PIXELVAL_AUTO : iContaining;
        int iWidth = PIXELVAL(pV, WIDTH, c);
        int iHeight = PIXELVAL(pV, HEIGHT, pBox->iContainingHeight);
        int isAuto = 0;

        nodeGetBoxProperties(pLayout, pNode, iContaining, &box);

        /* If the computed value if iWidth is "auto", calculate the
         * shrink-to-fit content width and use that instead.  */
        if (iWidth == PIXELVAL_AUTO) {
            int iMax;            /* Preferred maximum width */
            int iMin;            /* Preferred minimum width */
            int iAvailable;      /* Available width */
    
            iAvailable = sBox.iContaining;
            iAvailable -= (margin.margin_left + margin.margin_right);
            iAvailable -= (box.iLeft + box.iRight);
            blockMinMaxWidth(pLayout, pNode, &iMin, &iMax);
            iWidth = MIN(MAX(iMin, iAvailable), iMax);
            isAuto = 1;
        }
        considerMinMaxWidth(pNode, iContaining, &iWidth);

        /* Layout the node content into sContent. Then add the border and
         * transfer the result to sBox. 
         */
        memset(&sContent, 0, sizeof(BoxContext));
        sContent.iContaining = iWidth;
        sContent.iContainingHeight = iHeight;
        HtmlLayoutNodeContent(pLayout, &sContent, pNode);

        iHeight = getHeight(
            pNode, sContent.height, pBox->iContainingHeight
        );
        if (pV->eDisplay == CSS_CONST_TABLE) {
            sContent.height = MAX(iHeight, sContent.height);
        } else {
            sContent.height = iHeight;
        }

        if (!isAuto && DISPLAY(pV) != CSS_CONST_TABLE) {
            sContent.width = iWidth;
        } else {
            sContent.width = MAX(iWidth, sContent.width);
        }
        considerMinMaxWidth(pNode, iContaining, &sContent.width);

        wrapContent(pLayout, &sBox, &sContent, pNode);
    }

    iTotalWidth = sBox.width;
    iTotalHeight = sBox.height + margin.margin_top + margin.margin_bottom;
    iTotalHeight = MAX(iTotalHeight, 0);

    iLeft = 0;
    iRight = iContaining;

    iTop = y;
    iTop = HtmlFloatListPlace(
            pFloat, iContaining, iTotalWidth, iTotalHeight, iTop);
    HtmlFloatListMargins(pFloat, iTop, iTop+iTotalHeight, &iLeft, &iRight);

    y = iTop + margin.margin_top;
    if (eFloat == CSS_CONST_LEFT) {
        x = iLeft;
    } else {
        x = iRight - iTotalWidth;
    }
    DRAW_CANVAS(&pBox->vc, &sBox.vc, x, y, pNode);

    /* If the right-edge of this floating box exceeds the current actual
     * width of the box it is drawn in, set the actual width to the 
     * right edge. Floating boxes do not affect the height of the parent
     * box.
     */
    pBox->width = MAX(x + iTotalWidth, pBox->width);

    LOG(pNode) {
        HtmlTree *pTree = pLayout->pTree;
        char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        char const *zCaption = "normalFlowLayoutFloat() Float list before:";
        HtmlFloatListLog(pTree, zCaption, zNode, pNormal->pFloat);
    }

    /* Fix the float list in the parent block so that nothing overlaps
     * this floating box. Only add an entry if the height of the box including
     * margins is greater than zero pixels. Partly because it's not required,
     * and partly I'm worried the float list code will get confused. ;)
     *
     * A floating box may have a total height of less than zero if the
     * top or bottom margins are negative.
     */
    if (iTotalHeight > 0) {
        HtmlFloatListAdd(pNormal->pFloat, eFloat, 
            ((eFloat == CSS_CONST_LEFT) ? x + iTotalWidth : x),
            iTop, iTop + iTotalHeight);
    }

    LOG(pNode) {
        HtmlTree *pTree = pLayout->pTree;
        char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        char const *zCaption = "normalFlowLayoutFloat() Float list after:";
        HtmlLog(pTree, "LAYOUTENGINE", "%s (Float) %dx%d (%d,%d)", 
		zNode, iTotalWidth, iTotalHeight, x, iTop, NULL);
        HtmlFloatListLog(pTree, zCaption, zNode, pNormal->pFloat);
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * getRomanIndex --
 *
 *     Print an ordered list index into the given buffer.  Use roman
 *     numerals.  For indices greater than a few thousand, revert to
 *     decimal.
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
getRomanIndex(zBuf, iList, isUpper)
    char *zBuf;
    int iList;
    int isUpper;
{
    int i = 0;
    int j;
    static struct {
        int value;
        char *name;
    } values[] = {
        { 1000, "m" },
        { 999, "im" },
        { 990, "xm" },
        { 900, "cm" },
        { 500, "d"  },
        { 499, "id" },
        { 490, "xd" },
        { 400, "cd" },
        { 100, "c"  },
        { 99, "ic"  },
        { 90, "xc"  },
        { 50, "l"   },
        { 49, "il"  },
        { 40, "xl"  },
        { 10, "x"   },
        { 9, "ix"   },
        { 5, "v"    },
        { 4, "iv"   },
        { 1, "i"    },
    };
    int index = iList;

    if (index<1 || index>=5000) {
        sprintf(zBuf, "%d", index);
        return;
    }
    for (j = 0; index > 0 && j < sizeof(values)/sizeof(values[0]); j++) {
        while (index >= values[j].value) {
            int k;
            for (k = 0; values[j].name[k]; k++) {
                zBuf[i++] = values[j].name[k];
            }
            index -= values[j].value;
        }
    }
    zBuf[i] = 0;
    if (isUpper) {
        for(i=0; zBuf[i]; i++){
            zBuf[i] += 'A' - 'a';
        }
    }
}

void
HtmlLayoutMarkerBox(eStyle, iList, isList, zBuf)
    int eStyle;
    int iList;
    int isList;
    char *zBuf;
{
    zBuf[0] = '\0';

    if (eStyle == CSS_CONST_LOWER_LATIN) eStyle = CSS_CONST_LOWER_ALPHA;
    if (eStyle == CSS_CONST_UPPER_LATIN) eStyle = CSS_CONST_UPPER_ALPHA;

    /* If the document has requested an alpha marker, switch to decimal
     * after item 26. (i.e. item markers will be "x", "y", "z", "27".
     */
    if (eStyle == CSS_CONST_LOWER_ALPHA || eStyle == CSS_CONST_UPPER_ALPHA)
        if (iList > 26 || iList < 0) {
            eStyle = CSS_CONST_DECIMAL;
        }

    switch (eStyle) {
        case CSS_CONST_SQUARE:
             strcpy(zBuf, "\xe2\x96\xa1");      /* Unicode 0x25A1 */ 
             break;
        case CSS_CONST_CIRCLE:
             strcpy(zBuf, "\xe2\x97\x8b");      /* Unicode 0x25CB */ 
             break;
        case CSS_CONST_DISC:
             strcpy(zBuf ,"\xe2\x80\xa2");      /* Unicode 0x25CF */ 
             break;

        case CSS_CONST_LOWER_ALPHA:
             sprintf(zBuf, "%c%s", iList + 96, isList?".":"");
             break;
        case CSS_CONST_UPPER_ALPHA:
             sprintf(zBuf, "%c%s", iList + 64, isList?".":"");
             break;

        case CSS_CONST_LOWER_ROMAN:
             getRomanIndex(zBuf, iList, 0);
             if (isList) strcat(zBuf, ".");
             break;
        case CSS_CONST_UPPER_ROMAN:
             getRomanIndex(zBuf, iList, 1);
             if (isList) strcat(zBuf, ".");
             break;
        case CSS_CONST_DECIMAL:
             sprintf(zBuf, "%d%s", iList, isList?".":"");
             break;
        case CSS_CONST_DECIMAL_LEADING_ZERO:
             sprintf(zBuf, "%.2d%s", iList, isList?".":"");
             break;
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * markerBoxLayout --
 *
 *     This is called to generate the marker-box for an element with
 *     "display:list-item". The contents of the marker box are drawn
 *     to pBox.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Draws to pBox. Sets *pVerticalOffset.
 *
 *---------------------------------------------------------------------------
 */
static int
markerBoxLayout(pLayout, pBox, pNode, pVerticalOffset)
    LayoutContext *pLayout;       /* IN: Layout context */
    HtmlNode *pNode;              /* IN: Node with "display:list-item" */
    BoxContext *pBox;             /* OUT: Generated box */
    int *pVerticalOffset;         /* OUT: Ascent of generated box */
{
    HtmlComputedValues *pComputed = HtmlNodeComputedValues(pNode);
    int mmt = pLayout->minmaxTest;
    int voffset = 0;

    if (
        0 == pComputed->imListStyleImage && 
        pComputed->eListStyleType == CSS_CONST_NONE
    ) {
        return 0;
    }

    if (pComputed->imListStyleImage) {
        HtmlImage2 *pImg = pComputed->imListStyleImage;
        int iWidth = PIXELVAL_AUTO;
        int iHeight = PIXELVAL_AUTO;
        pImg = HtmlImageScale(pComputed->imListStyleImage, &iWidth, &iHeight,1);
        /* voffset = iHeight * -1; */
        HtmlDrawImage(
            &pBox->vc, pImg, 0, -1 * iHeight, iWidth, iHeight, pNode, mmt
        );
        HtmlImageFree(pImg);      /* Canvas has it's own reference */
        pBox->width = iWidth;
        pBox->height = iHeight;
    } else {
        HtmlCanvas *pCanvas = &pBox->vc;
        int eStyle;             /* Copy of pComputed->eListStyleType */
        Tk_Font font;           /* Font to draw list marker in */
        char zBuf[128];         /* Buffer for string to use as list marker */
        int iList = 1;

        HtmlNode *pParent = HtmlNodeParent(pNode);
        eStyle = pComputed->eListStyleType;

        /* Figure out the numeric index of this list element in it's parent.
         * i.e. the number to draw in the marker box if the list-style-type is
         * "decimal". Store the value in local variable iList.
         */
        if (pParent) {
            int ii;
            int iStart = HtmlNodeComputedValues(pParent)->iOrderedListStart;
            if (iStart != PIXELVAL_AUTO) {
                iList = iStart;
            }
            for (ii = 0; ii < HtmlNodeNumChildren(pParent); ii++) {
                HtmlNode *pSibling = HtmlNodeChild(pParent, ii);
                HtmlComputedValues *pSibProp = HtmlNodeComputedValues(pSibling);
                if (pSibling == pNode) {
                    break;
                }
                if (DISPLAY(pSibProp) == CSS_CONST_LIST_ITEM) {
                    iList++;
                    if (pSibProp->iOrderedListValue != PIXELVAL_AUTO) {
                        iList = pSibProp->iOrderedListValue;
                    }
                }
            }
        }
        if (pComputed->iOrderedListValue != PIXELVAL_AUTO) {
            iList = pComputed->iOrderedListValue;
        }

        HtmlLayoutMarkerBox(eStyle, iList, 1, zBuf);

        font = pComputed->fFont->tkfont;
        /* voffset = pComputed->fFont->metrics.ascent; */
        pBox->height = voffset + pComputed->fFont->metrics.descent;
        pBox->width = Tk_TextWidth(font, zBuf, strlen(zBuf));

        HtmlDrawText(
            pCanvas, zBuf, strlen(zBuf), 0, voffset, pBox->width, mmt, pNode, -1
        );
    }

    pBox->width += pComputed->fFont->ex_pixels;
    *pVerticalOffset = voffset;
    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * inlineLayoutDrawLines --
 *
 *     This function extracts zero or more line-boxes from an InlineContext
 *     object and draws them to a BoxContext. Variable *pY holds the
 *     Y-coordinate to draw line-boxes at in pBox. It is incremented by the
 *     height of each line box drawn before this function returns.
 *
 *     If parameter 'forceflag' is true, then all inline-boxes currently
 *     held by pContext are layed out, even if this means laying out an
 *     incomplete line. This is used (for example) at the end of a
 *     paragraph.
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
inlineLayoutDrawLines(pLayout, pBox, pContext, forceflag, pY, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    InlineContext *pContext;
    int forceflag;               /* True to lay out final, incomplete line. */
    int *pY;                     /* IN/OUT: Y-coordinate in sBox.vc. */
    NormalFlow *pNormal;
{
    int have;
    do {
        HtmlCanvas lc;             /* Line-Canvas */
        int w;
        int forcebox;              /* Force at least one inline-box per line */
        int closeborders = 0;
        int f;
        int y = *pY;               /* Y coord for line-box baseline. */
        int leftFloat = 0;
        int rightFloat = pBox->iContaining;
        int nV = 0;                /* Vertical height of line. */
        int nA = 0;                /* Ascent of line box. */

        /* If the inline-context is not completely empty, we collapse any
         * vertical margin here. Even though a line box may not be drawn by
         * this routine, at least one will be drawn by this InlineContext
         * eventually. Therefore it is safe to collapse the vertical margin.
         */
        if (!HtmlInlineContextIsEmpty(pContext)) {
            HtmlNode *pNode = HtmlInlineContextCreator(pContext);
            normalFlowMarginCollapse(pLayout, pNode, pNormal, &y);
        }

        /* Todo: We need a real line-height here, not a hard-coded '10' */
        HtmlFloatListMargins(pNormal->pFloat, y, y+10, &leftFloat, &rightFloat);
        forcebox = (rightFloat==pBox->iContaining && leftFloat==0);

        memset(&lc, 0, sizeof(HtmlCanvas));
        w = rightFloat - leftFloat;
        f = (forcebox ? LINEBOX_FORCEBOX : 0) | 
            (forceflag ? LINEBOX_FORCELINE : 0) |
            (closeborders ? LINEBOX_CLOSEBORDERS : 0);
        have = HtmlInlineContextGetLineBox(pLayout,pContext,f,&w,&lc,&nV,&nA);

        if (have) {
            DRAW_CANVAS(&pBox->vc, &lc, leftFloat, y, 0);
            if (pLayout->minmaxTest == 0) {
                HtmlDrawAddLinebox(&pBox->vc, leftFloat, y + nA);
            }
            y += nV;
            pBox->width = MAX(pBox->width, lc.right + leftFloat);
            pBox->height = MAX(pBox->height, y);
        } else if( w ) {
            /* If have==0 but w has been set to some non-zero value, then
             * there are inline-boxes in the inline-context, but there is
             * not enough space for the first inline-box in the width
             * provided. Increase the Y-coordinate and try the loop again.
             *
             * TODO: Pass the minimum height of the line-box to
             * HtmlFloatListPlace().
             */
            assert(!(f & LINEBOX_FORCEBOX));
            y = HtmlFloatListPlace(pNormal->pFloat, pBox->iContaining,w,10,y);
            have = 1;
        } 

        /* floatListClear(pBox->pFloats, y); */
        *pY = y;
    } while (have);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutNodeContent --
 *
 *     Draw the content area of node pNode into the box context pBox. If pNode
 *     has the 'display' property set to "table", it generates a table context.
 *     Otherwise, pNode is assumed to generate a normal flow context. According
 *     to CSS2.1 section 9.4.1, the following elements should use this
 *     function (as well as the root element):
 *
 *         Floats
 *         Absolutely positioned elements
 *         Table cells
 *         Elements with 'display' set to "inline-block" (not supported yet)
 *         Elements with 'overflow' other than "visible".
 *
 *     It is illegal to pass a replaced element (according to nodeIsReplaced())
 *     to this function.
 *
 *     When this function is called, pBox->iContaining should contain the pixel
 *     width available for the content.  The top-left hand corner of the
 *     content is placed at the (0,0) coordinate of canvas pBox->vc.
 *     pBox->width and height are set to the intrinsic width and height of the
 *     content when rendered with containing block width pBox->iContaining.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     If pNode is a replaced element, a Tcl script may be invoked to
 *     configured the element.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlLayoutNodeContent(pLayout, pBox, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
{
    int eDisplay = DISPLAY(HtmlNodeComputedValues(pNode));
    assert(!nodeIsReplaced(pNode));

    if (eDisplay == CSS_CONST_NONE) {
        /* Do nothing */
    } else if (eDisplay == CSS_CONST_TABLE) {
        /* All the work for tables is done in htmltable.c */
        HtmlTableLayout(pLayout, pBox, pNode);
    } else {
        /* Set up a new NormalFlow for this flow */
        HtmlFloatList *pFloat;
        NormalFlow sNormal;
    
        /* Set up the new normal-flow context */
        memset(&sNormal, 0, sizeof(NormalFlow));
        pFloat = HtmlFloatListNew();
        sNormal.pFloat = pFloat;

        /* TODO: Not sure about this. sNormal.isValid used to be initialized to
         * zero. If this line stays in, then the isValid variable can be
         * eliminated entirely (a very good thing).
         */
        sNormal.isValid = 1;
    
        /* Layout the contents of the node */
        normalFlowLayout(pLayout, pBox, pNode, &sNormal);

        normalFlowMarginCollapse(pLayout, pNode, &sNormal, &pBox->height);
    
        /* Increase the height of the box to cover any floating boxes that
         * extend down past the end of the content. 
         */
        pBox->height = HtmlFloatListClear(pFloat, CSS_CONST_BOTH, pBox->height);
    
        /* Clean up the float list */
        HtmlFloatListDelete(pFloat);
    }

    assert(!pLayout->minmaxTest || !pBox->vc.pFirst);
    assert(pBox->width < 100000);
    return 0;
}


/*
 *---------------------------------------------------------------------------
 *
 * drawReplacementContent --
 *
 *     pNode must be a replaced element according to nodeIsReplaced() (either
 *     it has a value set for -tkhtml-replacement-image or the Tcl [nodeHandle
 *     replace] has been used to replace the element with a Tk window).
 *
 *     The replaced content is drawn into pBox, with it's top left corner at
 *     (0, 0). Only content is drawn, no borders or backgrounds. The
 *     pBox->width and pBox->height variables are set to the size of the
 *     content.
 *
 *     If the 'width' property of pNode is set to a percentage it is calculated
 *     with respect to pBox->iContaining.
 *
 *     See drawReplacement() for a wrapper around this function that also 
 *     draws borders and backgrounds.
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
drawReplacementContent(pLayout, pBox, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
{
    int iWidth;
    int height;

    HtmlComputedValues *pV= HtmlNodeComputedValues(pNode);
    HtmlElementNode *pElem = HtmlNodeAsElement(pNode);

    assert(pNode && pElem);
    assert(nodeIsReplaced(pNode));

    /* Read the values of the 'width' and 'height' properties of the node.
     * PIXELVAL either returns a value in pixels (0 or greater) or the constant
     * PIXELVAL_AUTO. A value of less than 1 pixel that is not PIXELVAL_AUTO
     * is treated as exactly 1 pixel.
     */
    iWidth = PIXELVAL(
        pV, WIDTH, pLayout->minmaxTest ? PIXELVAL_AUTO : pBox->iContaining
    );
    height = PIXELVAL(pV, HEIGHT, pBox->iContainingHeight);
    if (height != PIXELVAL_AUTO) height = MAX(height, 1);
    if (iWidth != PIXELVAL_AUTO) iWidth = MAX(iWidth, 1);
    assert(iWidth != 0);

    if (pElem->pReplacement && pElem->pReplacement->win) {
        CONST char *zReplace = Tcl_GetString(pElem->pReplacement->pReplace);
        Tk_Window win = pElem->pReplacement->win;
        if (win) {
            Tcl_Obj *pWin = 0;
            int iOffset;
            int mmt = pLayout->minmaxTest;

            /* At this point local variable iWidth may be either a pixel 
             * width or PIXELVAL_AUTO. If it is PIXELVAL_AUTO we need to
             * figure out the actual width to use. This block does that
             * and sets iWidth to a pixel value.
             */
            assert(iWidth != 0);
            if (iWidth == PIXELVAL_AUTO) {
                switch (mmt) {
                    case MINMAX_TEST_MIN: {
                        int isPercent = ((PIXELVAL(pV, WIDTH, 0) == 0) ? 1 : 0);
                        if (!isPercent && pV->eDisplay == CSS_CONST_INLINE) {
                            iWidth = Tk_ReqWidth(win);
                        }
                        break;
                    }
                    default: 
                        iWidth = MIN(pBox->iContaining, Tk_ReqWidth(win));
                }
            }
            iWidth = MAX(iWidth, Tk_MinReqWidth(win));

            if (height == PIXELVAL_AUTO) {
                switch (mmt) {
                    case MINMAX_TEST_MIN: height = Tk_MinReqHeight(win); break;
                    default: height = Tk_ReqHeight(win);
                }
            }
            height = MAX(height, Tk_MinReqHeight(win));

            if (!pLayout->minmaxTest) {
                doConfigureCmd(pLayout->pTree, pElem, pBox->iContaining);
                pWin = Tcl_NewStringObj(zReplace, -1);
            }

            iOffset = pElem->pReplacement->iOffset;
            DRAW_WINDOW(&pBox->vc, pNode, 0, 0, iWidth, height);
        }
    } else {
        int t = pLayout->minmaxTest;
        int dummy_height = height;
        HtmlImage2 *pImg = pV->imReplacementImage;

        /* Take the 'max-width'/'min-width' properties into account */
        if (iWidth == PIXELVAL_AUTO) {
            HtmlImageScale(pImg, &iWidth, &dummy_height, 0);
        }
        considerMinMaxWidth(pNode, pBox->iContaining, &iWidth);

        pImg = HtmlImageScale(pImg, &iWidth, &height, (t ? 0 : 1));
        HtmlDrawImage(&pBox->vc, pImg, 0, 0, iWidth, height, pNode, t);
        HtmlImageFree(pImg);
    }

    if ( pNode->iNode >= 0 && pLayout->pTree->options.logcmd ){
        HtmlTree *pTree = pLayout->pTree;
        HtmlLog(pTree, "LAYOUTENGINE", 
            "%s drawReplacementContent() (%s) %dx%d descent=%d",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)),
            (pLayout->minmaxTest == MINMAX_TEST_MIN ? "mintest" : 
             pLayout->minmaxTest == MINMAX_TEST_MAX ? "maxtest" : "regular"),
             iWidth, height, 
             (pElem->pReplacement ? pElem->pReplacement->iOffset : 0)
		, NULL);
    }

    /* Note that width and height may still be PIXELVAL_AUTO here (if we failed
     * to find the named replacement image or window). This makes no difference
     * because PIXELVAL_AUTO is a large negative number.
     */
    assert(PIXELVAL_AUTO < 0);
    pBox->width = MAX(pBox->width, iWidth);
    pBox->height = MAX(pBox->height, height);
}


/*
 *---------------------------------------------------------------------------
 *
 * drawReplacement --
 *
 *     pNode must be a replaced element. 
 *
 *     The node content, background and borders are drawn into argument pBox.
 *     The pBox coordinates (0, 0) correspond to the outside edge of the left
 *     margin and the upper edge of the top border. That is, horizontal margins
 *     are included but vertical are not.
 *
 *     Any percentage lengths are calculated with respect to pBox->iContaining.
 *
 *     When this function returns, pBox->width is set to the width between
 *     outer edges of the right and left margins. pBox->height is set to the
 *     distance between the outside edges of the top and bottom borders. Again,
 *     horizontal margins are included but vertical are not.
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
drawReplacement(pLayout, pBox, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
{
    BoxProperties box;                /* Box properties of pNode */
    MarginProperties margin;          /* Margin properties of pNode */
    BoxContext sBox;

    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = pBox->iContaining;
    sBox.iContainingHeight = PIXELVAL_AUTO;
    drawReplacementContent(pLayout, &sBox, pNode);
    wrapContent(pLayout, pBox, &sBox, pNode);
}


static void
drawAbsolute(pLayout, pBox, pStaticCanvas, x, y)
    LayoutContext *pLayout;       /* Layout context */
    BoxContext *pBox;             /* Padding edge box to draw to */
    HtmlCanvas *pStaticCanvas;    /* Canvas containing static positions */
    int x;                        /* Offset of padding edge in pStaticCanvas */
    int y;                        /* Offset of padding edge in pStaticCanvas */
{
    NodeList *pList;
    NodeList *pNext;
    int iBoxHeight = pBox->height;
    for (pList = pLayout->pAbsolute; pList; pList = pNext) {
        int s_x;               /* Static X offset */
        int s_y;               /* Static Y offset */

        MarginProperties margin;
        BoxProperties box;

        BoxContext sBox;
        BoxContext sContent;

        HtmlNode *pNode = pList->pNode;
        HtmlComputedValues *pV= HtmlNodeComputedValues(pNode);
        int isReplaced = nodeIsReplaced(pList->pNode);

        int iLeft   = PIXELVAL(pV, LEFT, pBox->iContaining);
        int iRight  = PIXELVAL(pV, RIGHT, pBox->iContaining);
        int iWidth  = PIXELVAL(pV, WIDTH, pBox->iContaining);
        int iTop    = PIXELVAL(pV, TOP, iBoxHeight);
        int iBottom = PIXELVAL(pV, BOTTOM, iBoxHeight);
        int iHeight = PIXELVAL(pV, HEIGHT, iBoxHeight);
        int iSpace;

        pNext = pList->pNext;

        considerMinMaxWidth(pNode, pBox->iContaining, &iWidth);
        considerMinMaxHeight(pNode, iBoxHeight, &iHeight);

        if (HtmlDrawGetMarker(pStaticCanvas, pList->pMarker, &s_x, &s_y)) {
            /* If GetMarker() returns non-zero, then pList->pMarker is not
             * a part of pStaticCanvas. In this case do not draw the box
             * or remove the entry from the list either.
             */
            continue;
        }
        pList->pMarker = 0;

        nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);
        nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);

        /* Correct the static position for the padding edge offset. After the
         * correction the point (s_x, s_y) is the static position in pBox.
         */
        s_x -= x;
        s_y -= y;

        LOG(pNode) {
            HtmlTree *pTree = pLayout->pTree;
            char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
            HtmlLog(pTree, "LAYOUTENGINE", "%s drawAbsolute() -> "
                "containing block is %dx%d", zNode, pBox->width, iBoxHeight
		    , NULL);
            HtmlLog(pTree, "LAYOUTENGINE", "%s "
                "static position is (%d,%d) (includes correction of (%d,%d))", 
                zNode, s_x, s_y, x, y
		    , NULL);
        }

        memset(&sContent, 0, sizeof(BoxContext));
        if (isReplaced) {
            sContent.iContaining = pBox->iContaining;
            sContent.iContainingHeight = pBox->height;
            drawReplacementContent(pLayout, &sContent, pNode);
            iWidth = sContent.width;
        }

        /* Determine a content-width for pNode, according to the following:
         *
         * The sum of the following quantities is equal to the width of the
         * containing block.
         *
         *     + 'left' (may be "auto")
         *     + 'width' (may be "auto")
         *     + 'right' (may be "auto")
         *     + horizontal margins (one or both may be "auto")
         *     + horizontal padding and borders.
         */
        iSpace = pBox->iContaining - box.iLeft - box.iRight;
        if (
            iLeft != PIXELVAL_AUTO && 
            iRight != PIXELVAL_AUTO &&  
            iWidth != PIXELVAL_AUTO
        ) {
            iSpace -= (iLeft + iRight + iWidth);
            if (margin.leftAuto && margin.rightAuto) {
                if (iSpace < 0) {
                    margin.margin_right = iSpace;
                } else {
                    margin.margin_left = iSpace / 2;
                    margin.margin_right = iSpace - margin.margin_left;
                }
            } else if (margin.leftAuto) {
                margin.margin_left = iSpace;
            } else if (margin.rightAuto) {
                margin.margin_right = iSpace;
            } else {
                /* Box is overconstrained. Set 'right' to auto */
                iRight = PIXELVAL_AUTO;
            }
        }
        if (iLeft == PIXELVAL_AUTO && iRight == PIXELVAL_AUTO) {
            /* Box is underconstrained. Set 'left' to the static position */
            iLeft = s_x;
        }

        iSpace -= (margin.margin_left + margin.margin_right);
        iSpace -= (iLeft == PIXELVAL_AUTO ? 0: iLeft);
        iSpace -= (iRight == PIXELVAL_AUTO ? 0: iRight);
        iSpace -= (iWidth == PIXELVAL_AUTO ? 0: iWidth);

        if (
            iWidth == PIXELVAL_AUTO &&
            (iLeft == PIXELVAL_AUTO || iRight == PIXELVAL_AUTO)
        ) {
            int min;
            int max;
            assert(iRight != PIXELVAL_AUTO || iLeft != PIXELVAL_AUTO);
            blockMinMaxWidth(pLayout, pNode, &min, &max);
            iWidth = MIN(MAX(min, iSpace), max);
            LOG(pNode) {
                HtmlTree *pTree = pLayout->pTree;
                char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree,pNode));
                HtmlLog(pTree, "LAYOUTENGINE", "%s drawAbsolute() -> "
                    "Box is under-constrained and width is auto. "
                    "Using shrink-to-fit width: %dpx "
                    "(min=%dpx max=%dpx available=%dpx)", zNode, 
                    iWidth, min, max, iSpace
			, NULL);
            }
            iSpace -= iWidth;
        }

        /* At this point at most 1 of iWidth, iLeft and iRight can be "auto" */
        if (iWidth == PIXELVAL_AUTO) {
            assert(iLeft != PIXELVAL_AUTO && iRight != PIXELVAL_AUTO);
            iWidth = iSpace;
        } else if (iLeft == PIXELVAL_AUTO) {
            assert(iWidth != PIXELVAL_AUTO && iRight != PIXELVAL_AUTO);
            iLeft = iSpace;
        } else if (iRight == PIXELVAL_AUTO) {
            assert(iWidth != PIXELVAL_AUTO && iLeft != PIXELVAL_AUTO);
            iRight = iSpace;
        }

        /* Layout the content into sContent */
        if (!isReplaced) {
            considerMinMaxWidth(pNode, pBox->iContaining, &iWidth);
            sContent.iContaining = iWidth;
            HtmlLayoutNodeContent(pLayout, &sContent, pNode);
        }

        /* Determine a content-height for pNode, according to the following:
         *
         * The sum of the following quantities is equal to the width of the
         * containing block.
         *
         *     + 'top' (may be "auto")
         *     + 'height' (may be "auto")
         *     + 'bottom' (may be "auto")
         *     + vertical margins (one or both may be "auto")
         *     + vertical padding and borders.
         */
        iSpace = iBoxHeight - box.iTop - box.iBottom;
        if (
            iTop != PIXELVAL_AUTO && 
            iBottom != PIXELVAL_AUTO &&  
            iHeight != PIXELVAL_AUTO
        ) {
            iSpace -= (iTop + iBottom + iHeight);
            if (margin.topAuto && margin.bottomAuto) {
                if (iSpace < 0) {
                    margin.margin_bottom = iSpace;
                } else {
                    margin.margin_top = iSpace / 2;
                    margin.margin_bottom = iSpace - margin.margin_top;
                }
            } else if (margin.topAuto) {
                margin.margin_top = iSpace;
            } else if (margin.bottomAuto) {
                margin.margin_bottom = iSpace;
            } else {
                iBottom = PIXELVAL_AUTO;
            }
        }
        if (iTop == PIXELVAL_AUTO && iBottom == PIXELVAL_AUTO) {
            /* Box is underconstrained. Set 'top' to the static position */
            iTop = s_y;
        }

        iSpace -= (margin.margin_top + margin.margin_bottom);
        iSpace -= (iTop == PIXELVAL_AUTO ? 0: iTop);
        iSpace -= (iBottom == PIXELVAL_AUTO ? 0: iBottom);
        iSpace -= (iHeight == PIXELVAL_AUTO ? 0: iHeight);

        if (
            iHeight == PIXELVAL_AUTO &&
            (iTop == PIXELVAL_AUTO || iBottom == PIXELVAL_AUTO)
        ) {
            assert(iTop != PIXELVAL_AUTO || iBottom != PIXELVAL_AUTO);
            iHeight = sContent.height;
            iSpace -= iHeight;
        }

        /* At this point at most 1 of iWidth, iLeft and iRight can be "auto" */
        if (iHeight == PIXELVAL_AUTO) {
            assert(iTop != PIXELVAL_AUTO && iBottom != PIXELVAL_AUTO);
            iHeight = iSpace;
        } else if (iTop == PIXELVAL_AUTO) {
            assert(iHeight != PIXELVAL_AUTO && iBottom != PIXELVAL_AUTO);
            iTop = iSpace;
        } else if (iBottom == PIXELVAL_AUTO) {
            assert(iHeight != PIXELVAL_AUTO && iTop != PIXELVAL_AUTO);
            iBottom = iSpace;
        }

        considerMinMaxHeight(pNode, iBoxHeight, &iHeight);
        sContent.height = iHeight;
        sContent.width = iWidth;

        memset(&sBox, 0, sizeof(BoxContext));
        sBox.iContaining = pBox->iContaining;
        /* Wrap an overflow primitive around the content of this box. At
         * the moment this just clips the displayed content. But eventually
         * the HtmlCanvas module will automatically insert scrollbars if 
         * required.
         */
        if (pV->eOverflow == CSS_CONST_HIDDEN) {
            HtmlDrawOverflow(&sContent.vc,pNode,sContent.width,sContent.height);
        }
        wrapContent(pLayout, &sBox, &sContent, pNode);

        LOG(pNode) {
            HtmlTree *pTree = pLayout->pTree;
            char const *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
            HtmlLog(pTree, "LAYOUTENGINE", "%s Calculated values: "
                "left=%d right=%d top=%d bottom=%d width=%d height=%d", 
                zNode, iLeft, iRight, iTop, iBottom, iWidth, iHeight
		    , NULL);
        }

        DRAW_CANVAS(&pBox->vc, &sBox.vc, iLeft, iTop+margin.margin_top, pNode);
        pBox->height = MAX(pBox->height, 
            iTop + margin.margin_top + margin.margin_bottom + sBox.height
        );

        if (pLayout->pAbsolute == pList) {
            pLayout->pAbsolute = pList->pNext;
        } else {
            NodeList *pTmp = pLayout->pAbsolute;
            for (; pTmp->pNext != pList; pTmp = pTmp->pNext);
            pTmp->pNext = pList->pNext;
        }
        HtmlFree(pList);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * wrapContent --
 *
 *     BORDERS + BACKGROUND
 *
 *         The box context pointed to by pContent contains a content block
 *         with it's top-left at coordinates (0, 0). The width and height
 *         of the content are given by pContent->width and pContent->height,
 *         respectively. This function adds the background/borders box to
 *         the content and moves the result into box context pBox.
 *
 *         After this function returns, the point (0, 0) in pBox->vc
 *         corresponds to the top-left of the box including padding,
 *         borders and horizontal (but not vertical) margins. The values
 *         stored in pBox->width and pBox->height apply to the same box
 *         (including horizontal, but not vertical, margins).
 *
 *         Any percentage padding or margin values are calculated with
 *         respect to the value in pBox->iContaining. A value of "auto" for
 *         the left or right margin is treated as 0.
 *
 *     RELATIVE POSITIONING
 *
 *         If the value of the 'position' property for pNode is "relative",
 *         then the required offset (if any) is taken into account when
 *         drawing to pBox->vc. The static position of the element is still
 *         described by the (0, 0) point, pBox->width and pBox->height.
 *         Percentage values of properties 'left' and 'right', are
 *         calculated with respect to pBox->iContaining. 
 *
 *         Percentage values for 'top' and 'bottom' are treated as zero.
 *         TODO: This is a bug.
 *
 *     ABSOLUTE POSITIONING
 *
 *         If the value of the 'position' property is anything other than
 *         "static", then any absolutely positioned nodes accumulated in
 *         pLayout->pAbsolute are laid out into pBox with respect to the
 *         padding edge of pNode.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Many and varied.
 *---------------------------------------------------------------------------
 */
static void
wrapContent(pLayout, pBox, pContent, pNode)
    LayoutContext *pLayout;
    BoxContext *pBox;
    BoxContext *pContent;
    HtmlNode *pNode;
{
    HtmlComputedValues *pV= HtmlNodeComputedValues(pNode);
    MarginProperties margin;      /* Margin properties of pNode */
    BoxProperties box;            /* Box properties of pNode */
    int iRelLeft = 0;
    int iRelTop = 0;
    int x;
    int y;
    int w;
    int h;

    /* We do not want to generate a box for an implicit table-node
     * created to wrap around a stray "display:table-cell" or
     * "display:table-row" box. In this case, just copy the content
     * directly into the output box (without a border).
     */
    if (!pNode->pParent && pNode != pLayout->pTree->pRoot) {
        int iContaining = pBox->iContaining;
        memcpy(pBox, pContent, sizeof(BoxContext));
        pBox->iContaining = iContaining;
        memset(pContent, 0x55, sizeof(BoxContext));
        return;
    }

    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);
    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);

    x = margin.margin_left;
    y = 0;

    if (pV->ePosition == CSS_CONST_RELATIVE) {
        assert(pV->position.iLeft != PIXELVAL_AUTO);
        assert(pV->position.iTop != PIXELVAL_AUTO);
        assert(pV->position.iLeft == -1 * pV->position.iRight);
        assert(pV->position.iTop == -1 * pV->position.iBottom);
        iRelLeft = PIXELVAL(pV, LEFT, pBox->iContaining);
        iRelTop = PIXELVAL(pV, TOP, 0);
        x += iRelLeft;
        y += iRelTop;
    }

    w = box.iLeft + pContent->width + box.iRight;
    h = box.iTop + pContent->height + box.iBottom;

    HtmlLayoutDrawBox(
        pLayout->pTree, &pBox->vc, x, y, w, h, pNode, 0, pLayout->minmaxTest
    );

    x += box.iLeft;
    y += box.iTop;
    DRAW_CANVAS(&pBox->vc, &pContent->vc, x, y, pNode);

    pBox->width = MAX(pBox->width, 
        margin.margin_left + box.iLeft + 
        pContent->width + 
        box.iRight + margin.margin_right
    );
    pBox->height = MAX(pBox->height, 
        box.iTop + pContent->height + box.iBottom
    );

    LOG(pNode) {
        char zNotes[] = "<ol>"
            "<li>The content-block is the size of the content, as "
            "    determined by the 'width' and 'height' properties, or by"
            "    the intrinsic size of the block contents."
            "<li>The wrapped-block includes all space for padding and"
            "    borders, and horizontal (but not vertical) margins."
        "</ol>";

        char zDim[128];
        HtmlTree *pTree = pLayout->pTree;
        Tcl_Obj *pLog = Tcl_NewObj();
        Tcl_IncrRefCount(pLog);

        Tcl_AppendToObj(pLog, zNotes, -1);
        sprintf(zDim, 
            "<p>Size of content block: <b>%dx%d</b></p>", 
            pContent->width, pContent->height
        );
        Tcl_AppendToObj(pLog, zDim, -1);

        sprintf(zDim, 
            "<p>Size of wrapped block: <b>%dx%d</b></p>", 
            pBox->width, pBox->height
        );
        Tcl_AppendToObj(pLog, zDim, -1);

        HtmlLog(pTree, "LAYOUTENGINE", "%s wrapContent() %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)),
            Tcl_GetString(pLog)
		, NULL);
        Tcl_DecrRefCount(pLog);
    }

    if (
        (pV->ePosition != CSS_CONST_STATIC || pNode == pLayout->pTree->pRoot) &&
        pLayout->pAbsolute
    ) {
        BoxContext sAbsolute;
        int iLeftBorder = 0;
        int iTopBorder = 0;

        memset(&sAbsolute, 0, sizeof(BoxContext));
        sAbsolute.height = pContent->height;
        sAbsolute.height += box.iTop;
        sAbsolute.height += box.iBottom;
        if (pV->eBorderTopStyle != CSS_CONST_NONE) {
            iTopBorder = pV->border.iTop;
            sAbsolute.height -= iTopBorder;
        }
        if (pV->eBorderBottomStyle != CSS_CONST_NONE) {
            sAbsolute.height -= pV->border.iBottom;
        }
        sAbsolute.width = pContent->width;
        sAbsolute.width += box.iLeft;
        sAbsolute.width += box.iRight;
        if (pV->eBorderLeftStyle != CSS_CONST_NONE) {
            iLeftBorder = pV->border.iLeft;
            sAbsolute.width -= iLeftBorder;
        }
        if (pV->eBorderRightStyle != CSS_CONST_NONE) {
            sAbsolute.width -= pV->border.iRight;
        }
        sAbsolute.iContaining = sAbsolute.width;
        drawAbsolute(pLayout, &sAbsolute, &pBox->vc,
            iLeftBorder + margin.margin_left, iTopBorder
        );
        DRAW_CANVAS(&pBox->vc, &sAbsolute.vc, 
            iRelLeft + margin.margin_left + iLeftBorder, 
            iRelTop + iTopBorder, pNode
        );
    }
}

void 
HtmlLayoutDrawBox(pTree, pCanvas, x, y, w, h, pNode, flags, size_only)
    HtmlTree *pTree;
    HtmlCanvas *pCanvas;
    int x;
    int y;
    int w;
    int h;
    HtmlNode *pNode;
    int flags;
    int size_only;
{
    if (size_only) {
        HtmlDrawBox(pCanvas, x, y, w, h, pNode, flags, size_only, 0);
    } else {
        HtmlElementNode *pElem = HtmlNodeAsElement(pNode); 
        HtmlCanvasItem *pNew;
        HtmlCanvasItem *pItem = pElem->pBox;
        pNew = HtmlDrawBox(pCanvas, x, y, w, h, pNode, flags, size_only, pItem);
        HtmlDrawCanvasItemRelease(pTree, pItem);
        HtmlDrawCanvasItemReference(pNew);
        pElem->pBox = pNew;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutTable --
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
normalFlowLayoutTable(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    int iMinWidth;                     /* Minimum from blockMinMaxWidth */
    int iMaxWidth;                     /* Maximum from blockMinMaxWidth */
    int iContaining = pBox->iContaining;
    HtmlFloatList *pFloat = pNormal->pFloat;

    int iLeftFloat = 0;
    int iRightFloat = pBox->iContaining;

    int iWidth;                        /* Specified content width */
    int iCalcWidth;                    /* Calculated content width */

    int x, y;                     /* Coords for content to be drawn */
    BoxContext sContent;          /* Box context for content to be drawn into */
    BoxContext sBox;              /* Box context for sContent + borders */
    MarginProperties margin;      /* Margin properties of pNode */
    BoxProperties box;            /* Box properties of pNode */
    int iMPB;                     /* Sum of margins, padding and borders */

    nodeGetMargins(pLayout, pNode, iContaining, &margin);
    nodeGetBoxProperties(pLayout, pNode, iContaining, &box);

    iMPB = box.iLeft + box.iRight + margin.margin_left + margin.margin_right;

    /* Account for the 'margin-top' property of this node. The margin always
     * collapses for a table element.
     */
    normalFlowMarginAdd(pLayout, pNode, pNormal, margin.margin_top);
    normalFlowMarginCollapse(pLayout, pNode, pNormal, pY);

    iWidth = PIXELVAL(
        HtmlNodeComputedValues(pNode), WIDTH,
        pLayout->minmaxTest ? PIXELVAL_AUTO : pBox->iContaining
    );
    if (iWidth == PIXELVAL_AUTO) {
        /* Move down if the minimum rendered width of this  table cannot fit at
         * the current Y coordinate due to floating boxes. Then we can
         * figure out exactly how much room is available where the table
         * will actually be drawn. Of course, this only matters if the table
         * has 'width:auto'. Otherwise we'll go with the specified width
         * regardless.
         *
         * Note: Passing 10000 as the required height means in some (fairly
         * unlikely) circumstances the table will be placed lower in the flow
         * than would have been necessary. But it's not that big of a deal.
         */
        blockMinMaxWidth(pLayout, pNode, &iMinWidth, &iMaxWidth);
        *pY = HtmlFloatListPlace(pFloat,iContaining,iMPB+iMinWidth,10000,*pY);
        HtmlFloatListMargins(pFloat, *pY, *pY+10000, &iLeftFloat, &iRightFloat);
        iCalcWidth = MIN(iMaxWidth, iRightFloat - iLeftFloat - iMPB);
    } else {
        /* Astonishingly, the 'width' property when applied to an element
         * with "display:table" includes the horizontal borders (but not the
         * margins). So subtract the border widths from iWidth here.
         * 
         * See section 17 of CSS 2.1. It's something to do with the table
         * element generating an anonymous block box wrapped around itself and
         * it's captions (we don't implement captions yet).
         *
         * Note that for a "display:table" element, all padding values are
         * automatically zero so we don't have to worry about that when using
         * box.iLeft and box.iRight.
         */
        iCalcWidth = iWidth - iMPB;
    }

    memset(&sContent, 0, sizeof(BoxContext));
    memset(&sBox, 0, sizeof(BoxContext));
    sContent.iContaining = iCalcWidth;
    HtmlLayoutNodeContent(pLayout, &sContent, pNode);

    sContent.height = MAX(sContent.height, 
        getHeight(pNode, sContent.height, PIXELVAL_AUTO)
    );
    if (iWidth != PIXELVAL_AUTO) {
        sContent.width = MAX(sContent.width, iWidth - iMPB);
    }

    sBox.iContaining = iContaining;
    wrapContent(pLayout, &sBox, &sContent, pNode);

    y = HtmlFloatListPlace(
        pFloat, pBox->iContaining, sBox.width, sBox.height, *pY
    );
    *pY = y + sBox.height;
    HtmlFloatListMargins(pFloat, y, *pY, &iLeftFloat, &iRightFloat);
 
    x = iLeftFloat + doHorizontalBlockAlign(
        pLayout, pNode, &margin, iRightFloat - iLeftFloat - sBox.width
    );
    x = MAX(0, x);

    DRAW_CANVAS(&pBox->vc, &sBox.vc, x, y, pNode);
    pBox->height = MAX(pBox->height, *pY);
    pBox->width = MAX(pBox->width, x + sBox.width);

    LOG(pNode) {
        HtmlTree *pTree = pLayout->pTree;
        Tcl_Obj *pLog = Tcl_NewObj();
        Tcl_IncrRefCount(pLog);

        /* Log the table blocks final position in it's parent */
        Tcl_AppendToObj(pLog, "<p> Wrapped box coords in parent: (", -1);
        Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(x));
        Tcl_AppendToObj(pLog, ", ", -1);
        Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(y));
        Tcl_AppendToObj(pLog, ")", -1);

        HtmlLog(pTree, "LAYOUTENGINE", "%s normalFlowLayoutTable() %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)),
            Tcl_GetString(pLog),
            x, y
		, NULL);
        Tcl_DecrRefCount(pLog);
    }

    /* Account for the 'margin-bottom' property of this node. */
    normalFlowMarginAdd(pLayout, pNode, pNormal, margin.margin_bottom);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutTableComponent --
 *
 *     This function is called when a table-row or table-cell is encountered
 *     in the normal-flow (i.e not inside a table block the way they should
 *     be.
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
normalFlowLayoutTableComponent(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    int nChild;
    int ii;
    int idx;
    HtmlNode *pParent = HtmlNodeParent(pNode);
    HtmlElementNode sTable;            /* The fabricated "display:table" */

    for (ii = 0; ii < HtmlNodeNumChildren(pParent); ii++) {
        if (pNode == HtmlNodeChild(pParent, ii)) break;
    }
    idx = ii;

    for (; ii < HtmlNodeNumChildren(pParent); ii++) {
        HtmlNode *pChild = HtmlNodeChild(pParent, ii);
        int eDisp = DISPLAY(HtmlNodeComputedValues(pChild));
        if (
            0 == HtmlNodeIsWhitespace(pChild) &&
            eDisp != CSS_CONST_TABLE_CELL && eDisp != CSS_CONST_TABLE_ROW
        ) {
            break;
        }
        LOG(pNode) {
            HtmlTree *pTree = pLayout->pTree;
            const char *zFmt = 
                    "%s normalFlowLayoutTableComponent() -> "
                    "Child %d of implicit display:table";
            const char *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pChild));
            HtmlLog(pTree, "LAYOUTENGINE", zFmt, zNode, ii - idx, NULL);
        }
    }
    nChild = ii - idx;
    assert(nChild > 0);

    memset(&sTable, 0, sizeof(HtmlElementNode));
    sTable.apChildren = &((HtmlElementNode *)pParent)->apChildren[idx];
    sTable.nChild = nChild;
    sTable.node.iNode = -1;

    if (!pLayout->pImplicitTableProperties) {
        HtmlComputedValuesCreator sCreator;
        CssProperty sProp;
        sProp.eType = CSS_CONST_TABLE;
        sProp.v.zVal = "table";
        HtmlComputedValuesInit(pLayout->pTree, &sTable.node, 0, &sCreator);
        HtmlComputedValuesSet(&sCreator, CSS_PROPERTY_DISPLAY, &sProp);
        pLayout->pImplicitTableProperties = HtmlComputedValuesFinish(&sCreator);
    }
    sTable.pPropertyValues = pLayout->pImplicitTableProperties;

    normalFlowLayoutTable(pLayout, pBox, &sTable.node, pY, pContext, pNormal);

    /* Make sure the pretend node has not accumulated a layout-cache or
     * node-command (which can happen in a LOG block).  */
    HtmlLayoutInvalidateCache(pLayout->pTree, (HtmlNode *)&sTable);
    HtmlNodeDeleteCommand(pLayout->pTree, (HtmlNode *)&sTable);

    return nChild - 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutReplaced --
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
normalFlowLayoutReplaced(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    MarginProperties margin;          /* Margin properties of pNode */

    int x;             /* X-coord for content to be drawn */
    BoxContext sBox;   /* Box context for replacement to be drawn into */

    int iLeftFloat = 0;
    int iRightFloat = pBox->iContaining;

    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    /* First lay out the content of the element into sBox. Then figure out
     * where to put it in the parent box. 
     */
    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = pBox->iContaining;
    drawReplacement(pLayout, &sBox, pNode);

    /* Account for the 'margin-top' property of this node. The margin always
     * collapses for a replaced block node.
     */
    normalFlowMarginAdd(pLayout, pNode, pNormal, margin.margin_top);
    normalFlowMarginCollapse(pLayout, pNode, pNormal, pY);

    *pY = HtmlFloatListPlace(
        pNormal->pFloat, pBox->iContaining, sBox.width, sBox.height, *pY);
    HtmlFloatListMargins(
        pNormal->pFloat, *pY, *pY + sBox.height, &iLeftFloat, &iRightFloat);

    if (margin.leftAuto && margin.rightAuto) {
        x = (iRightFloat - iLeftFloat - sBox.width) / 2;
    } else if (margin.leftAuto) {
        x = (iRightFloat - sBox.width);
    } else {
        x = iLeftFloat;
    }

    DRAW_CANVAS(&pBox->vc, &sBox.vc, x, *pY, pNode);
    *pY += sBox.height;
    pBox->height = MAX(pBox->height, *pY);
    pBox->width = MAX(pBox->width, sBox.width);

    /* Account for the 'margin-bottom' property of this node. */
    normalFlowMarginAdd(pLayout, pNode, pNormal, margin.margin_bottom);

    return 0;
}

static void
setValueCallback(pNormal, pCallback, y)
    NormalFlow *pNormal;
    NormalFlowCallback *pCallback;
    int y;
{
    *(int *)(pCallback->clientData) = y;
    normalFlowCbDelete(pNormal, pCallback);
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutBlock --
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
normalFlowLayoutBlock(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);

    BoxProperties box;                /* Box properties of pNode */
    MarginProperties margin;          /* Margin properties of pNode */
    int iMPB;                         /* Sum of margins, padding borders */
    int iWidth;                       /* Content width of pNode in pixels */
    int iUsedWidth;
    int iWrappedX = 0;                /* X-offset of wrapped content */
    int iContHeight;                  /* Containing height for % 'height' val */
    int iSpareWidth;

    int yBorderOffset;     /* Y offset for top of block border */
    int x, y;              /* Coords for content to be drawn in pBox */
    BoxContext sContent;   /* Box context for content to be drawn into */
    BoxContext sBox;       /* sContent + borders */
    BoxContext sTmp;       /* Used to offset content */

    NormalFlowCallback sNormalFlowCallback;

    memset(&sContent, 0, sizeof(BoxContext));
    memset(&sBox, 0, sizeof(BoxContext));
    memset(&sTmp, 0, sizeof(BoxContext));

    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    iContHeight = pBox->iContainingHeight;

    /* Calculate iWidth and xBorderLeft. Both are interpreted as pixel values.
     * For a non-replaced block element, the width is always as calculated
     * here, even if the content is not as wide.
     */
    iWidth = PIXELVAL(
        pV, WIDTH, pLayout->minmaxTest ? PIXELVAL_AUTO : pBox->iContaining
    );

    iMPB = box.iLeft + box.iRight + margin.margin_left + margin.margin_right;
    if (iWidth == PIXELVAL_AUTO) {
        /* If 'width' is set to "auto", then treat an "auto" value for
         * 'margin-left' or 'margin-right' as 0. Then calculate the width
         * available for the content by subtracting the margins, padding and
         * borders from the width of the containing block.
         */
        iUsedWidth = pBox->iContaining - iMPB;
    } else {
        iUsedWidth = iWidth;
    }

    considerMinMaxWidth(pNode, pBox->iContaining, &iUsedWidth);
    sContent.iContaining = iUsedWidth;

    iSpareWidth = pBox->iContaining - iUsedWidth - iMPB;
    iWrappedX = doHorizontalBlockAlign(pLayout, pNode, &margin, iSpareWidth);

    if (!pLayout->minmaxTest) {
        /* Unless this is part of a min-max width test, then the content is at
         * least as wide as it's containing block. The call to
         * normalFlowLayout() below may increase sContent.width, but not
         * decrease it.
         */
        sContent.width = sContent.iContaining;
    }

    /* Account for the 'margin-top' property of this node. */
    normalFlowMarginAdd(pLayout, pNode, pNormal, margin.margin_top);

    /* If this box has either top-padding or a top border, then collapse the
     * vertical margin between this block and the one above now. In this
     * case, the top-left of the wrapContent() box will be at coordinates
     * (0, 0) of sContent.
     *
     * Otherwise, we have to wait for the vertical margins at the current
     * point to collapse before we know where the top of the box is drawn.
     * Do this by setting up a callback on the normal-flow object.
     */
    yBorderOffset = 0;
    if (box.iTop > 0 || pLayout->pTree->pRoot == pNode) {
        normalFlowMarginCollapse(pLayout, pNode, pNormal, pY); 
    } else {
        sNormalFlowCallback.xCallback = setValueCallback;
        sNormalFlowCallback.clientData = (ClientData)(&yBorderOffset);
        sNormalFlowCallback.pNext = 0;
        normalFlowCbAdd(pNormal, &sNormalFlowCallback);
    }

    /* Calculate x and y as pixel values. */
    *pY += box.iTop;
    y = *pY;
    x = iWrappedX + margin.margin_left + box.iLeft;

    /* Set up the box-context used to draw the content. */
    HtmlFloatListNormalize(pNormal->pFloat, -1 * x, -1 * y);

    /* Layout the content of this non-replaced block box. For this kind
     * of box, we treat any computed 'height' value apart from "auto" as a
     * minimum height.
     */
    sContent.iContainingHeight = PIXELVAL(pV, HEIGHT, iContHeight);
    normalFlowLayout(pLayout, &sContent, pNode, pNormal);

    /* Remove any margin-collapse callback added to the normal flow context. */
    normalFlowCbDelete(pNormal, &sNormalFlowCallback);

    /* Special case. If the intrinsic height of the box is 0 (i.e. 
     * it is empty) but the 'height' or 'min-height' properties cause
     * the height to be non-zero, then we need to collapse the vertical 
     * margins above this box.
     */
    if (sContent.height == 0 && getHeight(pNode, 0, iContHeight) > 0) {
        int iMargin = 0;
        normalFlowMarginCollapse(pLayout, pNode, pNormal, &iMargin); 
        *pY += iMargin;
        HtmlFloatListNormalize(pNormal->pFloat, 0, -1 * iMargin);
        y += iMargin;
    }

    /* Adjust for 'height', 'min-height' and 'max-height' properties */
    sContent.height = yBorderOffset + 
            getHeight(pNode, sContent.height - yBorderOffset, iContHeight);
    sContent.width = getWidth(iWidth, sContent.width);
    considerMinMaxWidth(pNode, pBox->iContaining, &sContent.width);

    LOG(pNode) {
        HtmlTree *pTree = pLayout->pTree;
        const char *zFmt = "%s normalFlowLayoutBlock() -> "
                "content size: %dx%d (y-border-offset: %d)";
        const char *zNode = Tcl_GetString(HtmlNodeCommand(pTree, pNode));
        HtmlLog(pTree, "LAYOUTENGINE", zFmt, zNode, sContent.width, 
		sContent.height - yBorderOffset, yBorderOffset, NULL);
    }

    /* Re-normalize the float-list. */
    HtmlFloatListNormalize(pNormal->pFloat, x, y);

    if (box.iBottom > 0) {
        pNormal->nonegative = 1;
        normalFlowMarginCollapse(pLayout, pNode, pNormal, &sContent.height);
    } 
    *pY += sContent.height;
    *pY += box.iBottom;

    sBox.iContaining = pBox->iContaining;
    DRAW_CANVAS(&sTmp.vc, &sContent.vc, 0, -1 * yBorderOffset, pNode);
    sTmp.width = sContent.width;

    sTmp.height = sContent.height - yBorderOffset;

    wrapContent(pLayout, &sBox, &sTmp, pNode);
    DRAW_CANVAS(&pBox->vc, &sBox.vc,iWrappedX, y-box.iTop+yBorderOffset, pNode);
    pBox->width = MAX(pBox->width, sBox.width);
    pBox->height = MAX(pBox->height, *pY);

    /* Account for the 'margin-bottom' property of this node. */
    normalFlowMarginAdd(pLayout, pNode, pNormal, margin.margin_bottom);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowClearFloat --
 *
 *     This is called when a node with the 'clear' property set is 
 *     encountered in the normal flow.
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
normalFlowClearFloat(pBox, pNode, pNormal, y)
    BoxContext *pBox;
    HtmlNode *pNode;
    NormalFlow *pNormal;
    int y;
{
    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);
    int eClear = pV->eClear;
    int ynew = y;
    if (eClear != CSS_CONST_NONE) {
        int ydiff;
        ynew = HtmlFloatListClear(pNormal->pFloat, eClear, ynew);
        ydiff = ynew - y;
        assert(ydiff >= 0);
        pNormal->iMaxMargin = MAX(pNormal->iMaxMargin - ydiff, 0);
        /* if (!pNormal->nonegative) pNormal->iMinMargin = 0; */
        pNormal->iMinMargin -= ydiff;
        pNormal->nonegative = 1;
        pBox->height = MAX(ynew, pBox->height);
    }
    return ynew;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutText --
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
normalFlowLayoutText(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    HtmlInlineContextAddText(pContext, pNode);
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutInlineReplaced --
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
normalFlowLayoutInlineReplaced(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    BoxContext sBox;
    HtmlCanvas canvas;
    int w, h;
    int iOffset = 0;

    MarginProperties margin;
    BoxProperties box;
    HtmlNodeReplacement *pReplace = HtmlNodeAsElement(pNode)->pReplacement;

    memset(&sBox, 0, sizeof(BoxContext));
    sBox.iContaining = pBox->iContaining;
    drawReplacement(pLayout, &sBox, pNode);

    /* Include the top and bottom margins in the box passed to the 
     * inline context code. 
     */
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);
    nodeGetBoxProperties(pLayout, pNode, pBox->iContaining, &box);
    h = sBox.height + margin.margin_top + margin.margin_bottom;
    w = sBox.width;

    /* If the box does not have a baseline (i.e. if the replaced content
     * is an image, not a widget), then the bottom margin edge of the box 
     * is it's baseline. See the description of "baseline" in CSS2.1 section
     * 10.8 ('vertical-align' property).
     */
    if (pReplace) {
        iOffset = box.iBottom + pReplace->iOffset;
    } 
    memset(&canvas, 0, sizeof(HtmlCanvas));
    DRAW_CANVAS(&canvas, &sBox.vc, 0, margin.margin_top, pNode);
    HtmlInlineContextAddBox(pContext, pNode, &canvas, w, h, iOffset);

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * layoutChildren --
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
layoutChildren(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    int ii;

    HtmlNode *pBefore = HtmlNodeBefore(pNode);
    HtmlNode *pAfter = HtmlNodeAfter(pNode);

    /* Layout the :before pseudo-element */
    normalFlowLayoutNode(pLayout, pBox, pBefore, pY, pContext, pNormal);

    /* Layout each of the child nodes. */
    for(ii = 0; ii < HtmlNodeNumChildren(pNode) ; ii++) {
        HtmlNode *p = HtmlNodeChild(pNode, ii);
        int r;
        r = normalFlowLayoutNode(pLayout, pBox, p, pY, pContext, pNormal);
        assert(r >= 0);
        ii += r;
    }

    /* Layout the :after pseudo-element */
    normalFlowLayoutNode(pLayout, pBox, pAfter, pY, pContext, pNormal);
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutInline --
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
normalFlowLayoutInline(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    InlineBorder *pBorder;
    pBorder = HtmlGetInlineBorder(pLayout, pContext, pNode);
    HtmlInlineContextPushBorder(pContext, pBorder);
    layoutChildren(pLayout, pBox, pNode, pY, pContext, pNormal);
    HtmlInlineContextPopBorder(pContext, pBorder);
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutInlineBlock --
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
normalFlowLayoutInlineBlock(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    BoxContext sBox;           /* Content */
    BoxContext sBox2;          /* After wrapContent() */
    BoxContext sBox3;          /* Adjusted for vertical margins */

    int iWidth;                /* Calculated value of 'width' */
    int iContaining;
    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);

    int w;                     /* Width of wrapped inline-block */
    int h;                     /* Height of wrapped inline-block */
    int dummy;
    int iLineBox;

    HtmlCanvas canvas;

    MarginProperties margin;
    nodeGetMargins(pLayout, pNode, pBox->iContaining, &margin);

    memset(&sBox, 0, sizeof(BoxContext));
    memset(&sBox2, 0, sizeof(BoxContext));
    memset(&sBox3, 0, sizeof(BoxContext));

    if (pV->eDisplay == CSS_CONST__TKHTML_INLINE_BUTTON) {
        iWidth = PIXELVAL_AUTO;
    } else {
        iWidth = PIXELVAL(pV, WIDTH, pBox->iContaining);
    }
    iContaining = iWidth;
    if (iContaining == PIXELVAL_AUTO) {
        blockMinMaxWidth(pLayout, pNode, &iContaining, 0);
    }

    sBox.iContaining = iContaining;
    HtmlLayoutNodeContent(pLayout, &sBox, pNode);
    if (iWidth != PIXELVAL_AUTO) {
        sBox.width = iWidth;
    }
    wrapContent(pLayout, &sBox2, &sBox, pNode);

    /* Include the vertical margins in the box. */
    memset(&canvas, 0, sizeof(HtmlCanvas));
    DRAW_CANVAS(&canvas, &sBox2.vc, 0, margin.margin_top, pNode);
    w = sBox2.width;
    h = sBox2.height + margin.margin_top + margin.margin_bottom;
    iLineBox = h;
    HtmlDrawFindLinebox(&canvas, &dummy, &iLineBox);

    HtmlInlineContextAddBox(pContext, pNode, &canvas, w, h, h - iLineBox);
    return 0;
}

static int 
normalFlowLayoutAbsolute(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    /* If this call is part of a min-max test, then ignore the absolutely
     * positioned block completely. It does not contribute to the width or
     * height of the parent anyway.
     */
    if (pLayout->minmaxTest == 0) {
        int iLeft = 0;
        int iDummy = 0;

        int y = *pY + normalFlowMarginQuery(pNormal);

        NodeList *pNew = (NodeList *)HtmlClearAlloc(0, sizeof(NodeList));
        pNew->pNode = pNode;
        pNew->pNext = pLayout->pAbsolute;

        /* Place a marker in the canvas that will be used to determine
         * the "static position" of the element. Two values are currently
         * stored:
         *
         *     * The static 'left' position
         *     * The static 'top' position
         *
         * See sections 10.3.7 and 10.6.4 of CSS2.1 for further details.
         * Technically the static 'right' position should be stored also
         * (this would only matter if right-to-left text was supported).
         */
        HtmlFloatListMargins(pNormal->pFloat, y, y, &iLeft, &iDummy);
        pNew->pMarker = HtmlDrawAddMarker(&pBox->vc, iLeft, y, 0);

        pLayout->pAbsolute = pNew;
    }
    return 0;
}

static int 
normalFlowLayoutFixed(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    if (pLayout->minmaxTest == 0) {
        int y = *pY + normalFlowMarginQuery(pNormal);
        NodeList *pNew = (NodeList *)HtmlClearAlloc(0, sizeof(NodeList));
        pNew->pNode = pNode;
        pNew->pNext = pLayout->pFixed;
        pNew->pMarker = HtmlDrawAddMarker(&pBox->vc, 0, y, 0);
        pLayout->pFixed = pNew;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * appendVerticalMarginsToObj --
 *
 *     This function is used with LOG {...} blocks only. It appends
 *     a description of the current vertical margins stored in pNormal
 *     to Tcl object pObj.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Appends to pObj.
 *
 *---------------------------------------------------------------------------
 */
static void
appendVerticalMarginsToObj(pObj, pNormal)
    Tcl_Obj *pObj;
    NormalFlow *pNormal;
{
    char zBuf[1024];
    sprintf(zBuf, "min=%d max=%d isValid=%d nonegative=%d", 
        pNormal->iMinMargin,
        pNormal->iMaxMargin,
        pNormal->isValid,
        pNormal->nonegative
    );
    Tcl_AppendToObj(pObj, zBuf, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutNode --
 *
 * Results:
 *     If pNode only is layed out, 0 is returned. If right-siblings of
 *     pNode are also layed out (this can happen when an implicit <table>
 *     is inserted) then the number of extra siblings layed out is returned.
 * 
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
normalFlowLayoutNode(pLayout, pBox, pNode, pY, pContext, pNormal)
    LayoutContext *pLayout;
    BoxContext *pBox;
    HtmlNode *pNode;
    int *pY;
    InlineContext *pContext;
    NormalFlow *pNormal;
{
    typedef struct FlowType FlowType;
    struct FlowType {
        char *z;
        int doDrawLines;          /* True to call inlineLayoutDrawLines() */
        int doClearFloat;         /* True to call normalFlowClearFloat() */
        FlowLayoutFunc *xLayout;  /* Layout function to invoke */
    };

    /* Look-up table used by this function. */
    #define F(z, d, c, x) static FlowType FT_ ## z = {#z, d, c, x}
    F( NONE,            0, 0, 0);
    F( BLOCK,           1, 1, normalFlowLayoutBlock);
    F( FLOAT,           0, 0, normalFlowLayoutFloat);
    F( TABLE,           1, 1, normalFlowLayoutTable);
    F( BLOCK_REPLACED,  1, 1, normalFlowLayoutReplaced);
    F( TEXT,            0, 0, normalFlowLayoutText);
    F( INLINE,          0, 0, normalFlowLayoutInline);
    F( INLINE_BLOCK,    0, 0, normalFlowLayoutInlineBlock);
    F( INLINE_REPLACED, 0, 0, normalFlowLayoutInlineReplaced);
    F( ABSOLUTE,        0, 0, normalFlowLayoutAbsolute);
    F( FIXED,           0, 0, normalFlowLayoutFixed);
    F( OVERFLOW,        1, 1, normalFlowLayoutOverflow);
    F( TABLE_COMPONENT, 0, 0, normalFlowLayoutTableComponent);
    #undef F

    /* 
     * Note about FT_NONE : The CSS 2.1 spec says, in section 9.2.4,
     * that an element with display 'none' has no effect on layout at all.
     * But rendering of http://slashdot.org depends on honouring the
     * 'clear' property on an element with display 'none'. And Mozilla,
     * KHTML and Opera do so.  Find out about this and if there are any
     * other properties that need handling here.
     *
     * Another question: Is this a quirks mode thing?
     */

    HtmlComputedValues *pV;               /* Property values of pNode */
    int eDisplay;                         /* Value of 'display' property */
    FlowType *pFlow = &FT_NONE;
    int ret = 0;                          /* Return value */

    CHECK_INTEGER_PLAUSIBILITY(pBox->vc.bottom);
    CHECK_INTEGER_PLAUSIBILITY(pBox->vc.right);

    /* If there is no node, do nothing */
    if (!pNode) return 0;
    pV = HtmlNodeComputedValues(pNode);
    eDisplay = DISPLAY(pV);

    if (HtmlNodeIsText(pNode)) {
        pFlow = &FT_TEXT;
    } else if (eDisplay == CSS_CONST_NONE) {
        /* Do nothing */
    } else if (eDisplay == CSS_CONST_INLINE) {
        pFlow = &FT_INLINE;
        if (nodeIsReplaced(pNode)) {
            pFlow = &FT_INLINE_REPLACED;
        } 
    } else if (
        eDisplay == CSS_CONST_INLINE_BLOCK ||
        eDisplay == CSS_CONST__TKHTML_INLINE_BUTTON
    ) {
        pFlow = &FT_INLINE_BLOCK;
        if (nodeIsReplaced(pNode)) {
            pFlow = &FT_INLINE_REPLACED;
        } 
    } else if (pV->ePosition == CSS_CONST_ABSOLUTE) {
        pFlow = &FT_ABSOLUTE;
    } else if (pV->ePosition == CSS_CONST_FIXED) {
        pFlow = &FT_FIXED;
    } else if (pV->eFloat != CSS_CONST_NONE) {
        pFlow = &FT_FLOAT;
    } else if (nodeIsReplaced(pNode)) {
        pFlow = &FT_BLOCK_REPLACED;
    } else if (eDisplay == CSS_CONST_BLOCK || eDisplay == CSS_CONST_LIST_ITEM) {
        pFlow = &FT_BLOCK;
        if (pV->eOverflow != CSS_CONST_VISIBLE) {
            pFlow = &FT_OVERFLOW;
        }
    } else if (eDisplay == CSS_CONST_TABLE) {
        /* Todo: 'inline-table' is currently handled as 'table' */
        pFlow = &FT_TABLE;
    } else if (
        eDisplay == CSS_CONST_TABLE_CELL || 
        eDisplay == CSS_CONST_TABLE_ROW
    ) {
        /* Special case - we need to wrap an implicit table block 
         * around this and any other table-cell or table-row components
         * that are right-siblings of pNode. This is not dealt with
         * using a FlowType instruction because it may consume more than
         * single node.
         */
        pFlow = &FT_TABLE_COMPONENT;
    }

    /* Log the state of the normal-flow context before this node */
    LOG(pNode) {
        HtmlTree *pTree = pLayout->pTree;
        Tcl_Obj *pLog = Tcl_NewObj();
        Tcl_IncrRefCount(pLog);

        Tcl_AppendToObj(pLog, "<ul style=\"list-item-style:none\">", -1);
        Tcl_AppendToObj(pLog, "<li>Layout as type: ", -1);
        Tcl_AppendToObj(pLog, pFlow->z, -1);
        Tcl_AppendToObj(pLog, "<li>Current y-coordinate: ", -1);
        Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(*pY));
        Tcl_AppendToObj(pLog, "<li>Containing width: ", -1);
        Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(pBox->iContaining));
        Tcl_AppendToObj(pLog, "<li>Vertical margins: ", -1);
        appendVerticalMarginsToObj(pLog, pNormal);
        Tcl_AppendToObj(pLog, "</ul>", -1);

        HtmlLog(pTree, "LAYOUTENGINE", "%s normalFlowLayoutNode() Before: %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)),
            Tcl_GetString(pLog)
		, NULL);

        HtmlFloatListLog(pTree, "Float list Before:", 
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)),
            pNormal->pFloat
        );

        Tcl_DecrRefCount(pLog);
    }

    if (pFlow->doDrawLines) {
        inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
    }
    if (pFlow->doClearFloat) {
        *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
    }
    if (pFlow->xLayout) {
        ret = pFlow->xLayout(pLayout, pBox, pNode, pY, pContext, pNormal);
    }

    /* See if there are any complete line-boxes to copy to the main canvas. */
    inlineLayoutDrawLines(pLayout, pBox, pContext, 0, pY, pNormal);

    /* More backwards compatible insanity. As of CSS 2, the 'clear' property
     * should be ignored on all inline elements. And the BR element, so
     * says the spec, should be implemented as:
     *
     *     BR:before { content: "\A" ; white-space: pre-line; }
     *
     * But it's common to apply the 'clear' property to BR elements. The
     * following block seems to mimic what other browsers are doing - add
     * the newline, then clear any floats if required.
     */
    if (
        HtmlNodeTagType(pNode) == Html_BR &&
        pV->eClear != CSS_CONST_NONE && 
        pV->eDisplay == CSS_CONST_INLINE
    ) {
        inlineLayoutDrawLines(pLayout, pBox, pContext, 1, pY, pNormal);
        *pY = normalFlowClearFloat(pBox, pNode, pNormal, *pY);
    }

    /* Log the state of the normal-flow context after this node */
    LOG(pNode) {
        HtmlTree *pTree = pLayout->pTree;
        Tcl_Obj *pLog = Tcl_NewObj();
        Tcl_IncrRefCount(pLog);

        Tcl_AppendToObj(pLog, "<ul style=\"list-item-style:none\">", -1);
        Tcl_AppendToObj(pLog, "<li>Current y-coordinate: ", -1);
        Tcl_AppendObjToObj(pLog, Tcl_NewIntObj(*pY));
        Tcl_AppendToObj(pLog, "<li>Vertical margins: ", -1);
        appendVerticalMarginsToObj(pLog, pNormal);
        Tcl_AppendToObj(pLog, "</ul>", -1);

        HtmlLog(pTree, "LAYOUTENGINE", "%s normalFlowLayoutNode() After: %s",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)),
            Tcl_GetString(pLog)
		, NULL);

        Tcl_DecrRefCount(pLog);
    }

    CHECK_INTEGER_PLAUSIBILITY(pBox->vc.bottom);
    CHECK_INTEGER_PLAUSIBILITY(pBox->vc.right);

    return ret;
}


#define LAYOUT_CACHE_N_USE_COND 6
#ifdef LAYOUT_CACHE_DEBUG
#define LAYOUT_CACHE_N_STORE_COND 9
static int aDebugUseCacheCond[LAYOUT_CACHE_N_USE_COND + 1];
static int aDebugStoreCacheCond[LAYOUT_CACHE_N_STORE_COND + 1];
#endif

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayoutFromCache --
 *
 *     This routine is strictly a helper function for normalFlowLayout().
 *     It is never called from anywhere else. The arguments are the
 *     same as those passed to normalFlowLayout().
 *
 *     This function tries to do the job of normalFlowLayout() (see comments
 *     above that function) using data stored in the layout-cache associated
 *     with pNode (the structure *pNode->pLayoutCache). If successful, it
 *     returns non-zero. In this case normalFlowLayout() will return
 *     immediately, it's job having been performed using cached data.
 *     If the cache is not present or cannot be used, this function returns
 *     zero. In this case normalFlowLayout() should proceed.
 * 
 *     See also normalFlowLayoutToCache(), the function that creates
 *     the layout-cache used by this routine.
 *
 *         1. The widget -layoutcache option is set to true.
 *         2. A valid layout cache exists for pNode.
 *         3. The width allocated for node content is the same as when the
 *            the cache was generated.
 *         4. The vertical margins that will collapse with the top margin of 
 *            the first block in this flow are the same as they were when the
 *            cache was generated.
 *         5. The current floating margins are the same as they were when 
 *            the cache was generated and there are no new floating margins
 *            in the float list that affect the area where the cached 
 *            layout is to be placed.
 *
 * Results:
 *     Non-zero if the cache associated with pNode contained a usable
 *     layout, else zero.
 *
 * Side effects:
 *     May render content of pNode into pBox. See above.
 *
 *---------------------------------------------------------------------------
 */
static int 
normalFlowLayoutFromCache(pLayout, pBox, pElem, pNormal, iLeft, iRight)
    LayoutContext *pLayout;       /* Layout context */
    BoxContext *pBox;             /* Box context to draw to */
    HtmlElementNode *pElem;       /* Node to start drawing at */
    NormalFlow *pNormal;
    int iLeft;
    int iRight;
{
    int cache_mask = (1 << pLayout->minmaxTest);

    HtmlFloatList   *pFloat = pNormal->pFloat;
    HtmlLayoutCache *pLayoutCache = pElem->pLayoutCache;
    LayoutCache     *pCache = &pLayoutCache->aCache[pLayout->minmaxTest];

    assert(pNormal->isValid == 0 || pNormal->isValid == 1);

#ifdef LAYOUT_CACHE_DEBUG
  #define COND(x, y) ( (y) || ((aDebugUseCacheCond[x]++) < 0) )
#else
  #define COND(x, y) (y)
#endif

    if ( 0 == (
        COND(1, pLayout->pTree->options.layoutcache) &&
        COND(2, pLayoutCache && (pLayoutCache->flags & cache_mask)) &&
        COND(3, pBox->iContaining == pCache->iContaining) &&
        COND(4,
            pNormal->isValid    == pCache->normalFlowIn.isValid &&
            pNormal->iMinMargin == pCache->normalFlowIn.iMinMargin &&   
            pNormal->iMaxMargin == pCache->normalFlowIn.iMaxMargin &&
            pNormal->nonegative == pCache->normalFlowIn.nonegative
        ) &&
        COND(5, iLeft == pCache->iFloatLeft && iRight == pCache->iFloatRight) &&
        COND(6, HtmlFloatListIsConstant(pFloat, 0, pCache->iHeight))
    )) {
        if (pLayoutCache) {
            HtmlDrawCleanup(pLayout->pTree, &pCache->canvas);
        }
        return 0;
    }

#ifdef LAYOUT_CACHE_DEBUG
    aDebugUseCacheCond[0]++;
#endif

    /* Hooray! A cached layout can be used. */
    assert(!pBox->vc.pFirst);
    if (pCache->iMarginCollapse != PIXELVAL_AUTO) {
        NormalFlowCallback *pCallback = pNormal->pCallbackList;
        int iMargin = pCache->iMarginCollapse;
        while (pCallback) {
            pCallback->xCallback(pNormal, pCallback, iMargin);
            pCallback = pCallback->pNext;
        }
    }
    HtmlDrawCopyCanvas(&pBox->vc, &pCache->canvas);
    pBox->width = pCache->iWidth;
    assert(pCache->iHeight >= pBox->height);
    pBox->height = pCache->iHeight;
    pNormal->iMaxMargin = pCache->normalFlowOut.iMaxMargin;
    pNormal->iMinMargin = pCache->normalFlowOut.iMinMargin;
    pNormal->isValid = pCache->normalFlowOut.isValid;
    pNormal->nonegative = pCache->normalFlowOut.nonegative;

    return 1;
}

/*
 *---------------------------------------------------------------------------
 *
 * normalFlowLayout --
 *
 *     This function is called in two circumstances:
 *
 *         1. When pNode creates a new normal flow context and
 *         2. When pNode is a non-replaced, non-floating block box in a normal
 *            flow (recursively from normalFlowLayoutBlock()).
 *
 *     In either case, the content of pNode is drawn to pBox->vc with the
 *     top-left hand corner at canvas coordinates (0, 0). It is the callers
 *     responsibility to deal with margins, border padding and background. 
 *
 *     When this function is called, pBox->iContaining should contain the width
 *     available for the content of pNode (*not* the width of pNode's
 *     containing block as for FlowLayoutFunc functions).
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
normalFlowLayout(pLayout, pBox, pNode, pNormal)
    LayoutContext *pLayout;       /* Layout context */
    BoxContext *pBox;             /* Box context to draw to */
    HtmlNode *pNode;              /* Node to start drawing at */
    NormalFlow *pNormal;
{
    InlineContext *pContext;
    int y = 0;
    int rc = 0;                       /* Return Code */
    InlineBorder *pBorder;
    HtmlFloatList *pFloat = pNormal->pFloat;
    NodeList *pAbsolute = pLayout->pAbsolute;
    NodeList *pFixed = pLayout->pFixed;

    int left = 0; 
    int right = pBox->iContaining;
    int overhang;

    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);
    int isSizeOnly = pLayout->minmaxTest;
    int iTextIndent = PIXELVAL(pV, TEXT_INDENT, pBox->iContaining);

    HtmlLayoutCache *pLayoutCache = 0;
    LayoutCache *pCache = 0;
    int cache_mask = (1 << pLayout->minmaxTest);

    NormalFlowCallback sCallback;

    HtmlElementNode *pElem = (HtmlElementNode *)pNode;
    assert(!HtmlNodeIsText(pNode));

    CHECK_INTEGER_PLAUSIBILITY(pBox->vc.bottom);
    CHECK_INTEGER_PLAUSIBILITY(pBox->vc.right);

    /* TODO: Should the fourth case ("display:inline") really be here? */
    assert( 
        DISPLAY(pV) == CSS_CONST_BLOCK ||
        DISPLAY(pV) == CSS_CONST_INLINE_BLOCK ||
        DISPLAY(pV) == CSS_CONST_TABLE_CELL ||
        DISPLAY(pV) == CSS_CONST_LIST_ITEM ||
        DISPLAY(pV) == CSS_CONST_INLINE ||
        DISPLAY(pV) == CSS_CONST__TKHTML_INLINE_BUTTON
    );
    assert(!nodeIsReplaced(pNode));

    /* Attempt to use a layout cache */
    HtmlFloatListMargins(pFloat, 0, 1, &left, &right);
    if (normalFlowLayoutFromCache(pLayout, pBox, pElem, pNormal, left, right)) {
        return;
    }

    /* If the structure for cached layout has not yet been allocated,
     * allocate it now. The corresponding call to HtmlFree() is in
     * the HtmlLayoutInvalidateCache() function.
     * 
     * Set pCache to point at the LayoutCache object used to cache
     * this call. Boolean variable isCacheValid indicates whether or
     * not the contents of pCache are currently valid.
     */
    if (!pElem->pLayoutCache) {
        pElem->pLayoutCache = HtmlNew(HtmlLayoutCache);
    }
    pLayoutCache = pElem->pLayoutCache;
    pCache = &pLayoutCache->aCache[pLayout->minmaxTest];

    HtmlDrawCleanup(pLayout->pTree, &pCache->canvas);
    pLayoutCache->flags &= ~(cache_mask);
    pCache->normalFlowIn.iMaxMargin = pNormal->iMaxMargin;
    pCache->normalFlowIn.iMinMargin = pNormal->iMinMargin;
    pCache->normalFlowIn.isValid = pNormal->isValid;
    pCache->normalFlowIn.nonegative = pNormal->nonegative;
    pCache->iContaining = pBox->iContaining;
    pCache->iFloatLeft = left;
    pCache->iFloatRight = right;
    pCache->iMarginCollapse = PIXELVAL_AUTO;

    sCallback.xCallback = setValueCallback;
    sCallback.clientData = (ClientData) &pCache->iMarginCollapse;
    sCallback.pNext = 0;
    normalFlowCbAdd(pNormal, &sCallback);

    /* Create the InlineContext object for this containing box */
    pContext = HtmlInlineContextNew(
            pLayout->pTree, pNode, isSizeOnly, iTextIndent
    );

    /* Add any inline-border created by the node that generated this
     * normal-flow to the InlineContext. Actual border attributes do not apply
     * in this case, but the 'text-decoration' attribute may.
     */
    pBorder = HtmlGetInlineBorder(pLayout, pContext, pNode);
    HtmlInlineContextPushBorder(pContext, pBorder);

    /* If this element is a list-item with "list-style-position:inside", 
     * then add the list-marker as the first box in the new inline-context.
     */
    if (
        DISPLAY(pV) == CSS_CONST_LIST_ITEM &&
        pV->eListStylePosition == CSS_CONST_INSIDE
    ) {
        BoxContext sMarker;
        int iAscent;
        memset(&sMarker, 0, sizeof(BoxContext));
        if (markerBoxLayout(pLayout, &sMarker, pNode, &iAscent)) {
            HtmlInlineContextAddBox(pContext, pNode, 
                &sMarker.vc, sMarker.width, sMarker.height, 
                sMarker.height - iAscent
            );
        }
    }

    layoutChildren(pLayout, pBox, pNode, &y, pContext, pNormal);
    
    /* Finish the inline-border started by the parent, if any. */
    HtmlInlineContextPopBorder(pContext, pBorder);

    rc = inlineLayoutDrawLines(pLayout, pBox, pContext, 1, &y, pNormal);
    HtmlInlineContextCleanup(pContext);

    /* If this element is a list-item with "list-style-position:outside", 
     * and at least one line box was drawn, line up the list marker
     * with the baseline of the first line box.
     */
    if (
        DISPLAY(pV) == CSS_CONST_LIST_ITEM &&
        pV->eListStylePosition == CSS_CONST_OUTSIDE
    ) {
        BoxContext sMarker;
        int iAscent;
        int xline, yline;
        memset(&sMarker, 0, sizeof(BoxContext));
        if( 
            HtmlDrawFindLinebox(&pBox->vc, &xline, &yline) &&
            markerBoxLayout(pLayout, &sMarker, pNode, &iAscent)
        ) {
            int xlist = xline - sMarker.width;
            int ylist = yline - iAscent;
            assert(iAscent == 0);
            DRAW_CANVAS(&pBox->vc, &sMarker.vc, xlist, ylist, pNode);
        }
    }

    left = 0;
    right = pBox->iContaining;
    HtmlFloatListMargins(pFloat, pBox->height-1, pBox->height, &left, &right);

    /* TODO: Danger! elements with "position:relative" might break this? */
    overhang = MAX(0, pBox->vc.bottom - pBox->height);

    normalFlowCbDelete(pNormal, &sCallback);

#undef COND
#ifdef LAYOUT_CACHE_DEBUG
  #define COND(x, y) ( (y) || ((aDebugStoreCacheCond[x]++) < 0) )
#else
  #define COND(x, y) (y)
#endif

    if (
        COND(1, pLayout->pTree->options.layoutcache) && 
        COND(2, pCache->iFloatLeft == left) &&
        COND(3, pCache->iFloatRight == right) &&
        COND(4, HtmlFloatListIsConstant(pFloat, pBox->height, overhang)) &&
        COND(5, pLayout->pAbsolute == pAbsolute) &&
        COND(6, pLayout->pFixed == pFixed) &&
        COND(7, !HtmlNodeBefore(pNode) && !HtmlNodeAfter(pNode)) && 
        COND(8, pNode->pParent) &&
        COND(9, pNode->iNode >= 0)
    ) {
        HtmlDrawOrigin(&pBox->vc);
        HtmlDrawCopyCanvas(&pCache->canvas, &pBox->vc);
        pCache->iWidth = pBox->width;
        pCache->iHeight = pBox->height;
        pCache->normalFlowOut.iMaxMargin = pNormal->iMaxMargin;
        pCache->normalFlowOut.iMinMargin = pNormal->iMinMargin;
        pCache->normalFlowOut.isValid = pNormal->isValid;
        pCache->normalFlowOut.nonegative = pNormal->nonegative;
        pLayoutCache->flags |= cache_mask;

        LOG(pNode) {
            HtmlTree *pTree = pLayout->pTree;
            HtmlLog(pTree, "LAYOUTENGINE", "%s normalFlowLayout() "
                "Cached layout for node:"
                "<ul><li>width = %d"
                "    <li>height = %d"
                "</ul>",
                Tcl_GetString(HtmlNodeCommand(pTree, pNode)),
                pCache->iWidth, pCache->iHeight
		    , NULL);
        }

#ifdef LAYOUT_CACHE_DEBUG
        aDebugStoreCacheCond[0]++;
#endif
    }

    CHECK_INTEGER_PLAUSIBILITY(pBox->vc.bottom);
    CHECK_INTEGER_PLAUSIBILITY(pBox->vc.right);
    return;
}

/*
 *---------------------------------------------------------------------------
 *
 * blockMinMaxWidth --
 *
 *     Figure out the minimum and maximum widths that the content generated 
 *     by pNode may use. This is used during table and floating box layout.
 *
 *     The returned widths do not include the borders, padding or margins of
 *     the node. Just the content.
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
blockMinMaxWidth(pLayout, pNode, pMin, pMax)
    LayoutContext *pLayout;
    HtmlNode *pNode;
    int *pMin;
    int *pMax;
{
    BoxContext sBox;
    HtmlLayoutCache *pCache;
    int minmaxTestOrig = pLayout->minmaxTest;

    HtmlElementNode *pElem = (HtmlElementNode *)pNode;
    assert(!HtmlNodeIsText(pNode));

    /* If there is no layout-cache allocated, allocate one now */
    if (!pElem->pLayoutCache) {
        pElem->pLayoutCache = (HtmlLayoutCache *)HtmlClearAlloc(
            "HtmlLayoutCache", sizeof(HtmlLayoutCache)
        );
    }
    pCache = pElem->pLayoutCache;

    /* Figure out the minimum width of the box by
     * pretending to lay it out with a parent-width of 0.
     */
    if (pMin) {
        if (!(pCache->flags & CACHED_MINWIDTH_OK)) {
            pLayout->minmaxTest = MINMAX_TEST_MIN;
            memset(&sBox, 0, sizeof(BoxContext));
            HtmlLayoutNodeContent(pLayout, &sBox, pNode);
            HtmlDrawCleanup(0, &sBox.vc);
            pCache->iMinWidth = sBox.width;
            pCache->flags |= CACHED_MINWIDTH_OK;
        }
        *pMin = pCache->iMinWidth;
    }

    /* Figure out the maximum width of the box by pretending to lay it
     * out with a very large parent width. It is not expected to
     * be a problem that tables may be layed out incorrectly on
     * displays wider than 10000 pixels.
     */
    if (pMax) {
        if (!(pCache->flags & CACHED_MAXWIDTH_OK)) {
            pLayout->minmaxTest = MINMAX_TEST_MAX;
            memset(&sBox, 0, sizeof(BoxContext));
            sBox.iContaining = 10000;
            HtmlLayoutNodeContent(pLayout, &sBox, pNode);
            HtmlDrawCleanup(0, &sBox.vc);
            pCache->iMaxWidth = sBox.width;
            pCache->flags |= CACHED_MAXWIDTH_OK;
        }
        *pMax = pCache->iMaxWidth;
    }

    pLayout->minmaxTest = minmaxTestOrig;

    /* It is, surprisingly, possible for the minimum width to be greater
     * than the maximum width at this point. For example, consider the
     * following:
     *
     *   div { font: fixed ; text-indent: -2.5ex }
     *   <div>x x</div>
     *
     * The "max-width" layout puts the two 'x' characters on the same line,
     * for a width of (0.5ex). The "min-width" puts the second 'x' on a
     * second line-box, for a width of (1.0ex).
     *
     * First encountered in october 2006 at:
     *     http://root.cern.ch/root/htmldoc/TF1.html
     */
    if (
        pCache->flags & CACHED_MAXWIDTH_OK &&
        pCache->flags & CACHED_MINWIDTH_OK &&
        pCache->iMaxWidth < pCache->iMinWidth
    ) {
        pCache->iMaxWidth = MAX(pCache->iMaxWidth, pCache->iMinWidth);
        if (pMax) {
            *pMax = pCache->iMaxWidth;
        }
    }

    LOG(pNode) {
        char zMin[24];
        char zMax[24];
        HtmlTree *pTree = pLayout->pTree;

        if (pMax) sprintf(zMax, "%d", *pMax);
        else      sprintf(zMax, "N/A");
        if (pMin) sprintf(zMin, "%d", *pMin);
        else      sprintf(zMin, "N/A");

        HtmlLog(pTree, "LAYOUTENGINE", "%s blockMinMaxWidth() -> "
            "min=%s max=%s",
            Tcl_GetString(HtmlNodeCommand(pTree, pNode)), 
            zMin, zMax
		, NULL);
    }

    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * doConfigureCmd --
 *
 *     Argument pNode must be a pointer to a node that has been replaced (i.e.
 *     using the [$node replace] interface) with a Tk window.  This function
 *     executes the "-configurecmd" script to configure the window based on the
 *     actual CSS property values for the node.
 *
 *     Currently, the configuration array contains the following:
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     If the -configurecmd script returns an error, Tcl_BackgroundError() is
 *     called.
 *---------------------------------------------------------------------------
 */
static void 
doConfigureCmd(pTree, pElem, iContaining)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
    int iContaining;
{
    Tcl_Obj *pConfigure;                           /* -configurecmd script */

    assert(pElem && pElem->pReplacement);
    pConfigure = pElem->pReplacement->pConfigureCmd;
    pElem->pReplacement->iOffset = 0;

    if (pConfigure) {
        Tcl_Interp *interp = pTree->interp;
        HtmlComputedValues *pV = pElem->pPropertyValues;
        HtmlComputedValues *pTmpComputed;
        HtmlNode *pTmp;
        Tcl_Obj *pArray;
        Tcl_Obj *pScript;
        Tcl_Obj *pRes;
        int rc;
        int iWidth;
        int iHeight;

        pArray = Tcl_NewObj();
        Tcl_ListObjAppendElement(interp, pArray, Tcl_NewStringObj("color",-1));
        Tcl_ListObjAppendElement(interp, pArray, 
                Tcl_NewStringObj(Tk_NameOfColor(pV->cColor->xcolor), -1)
        );

        pTmp = (HtmlNode *)pElem;
        pTmpComputed = pV;
        while (pTmp && pTmpComputed->cBackgroundColor->xcolor == 0) {
            pTmp = HtmlNodeParent(pTmp);
            if (pTmp) {
                pTmpComputed = HtmlNodeComputedValues(pTmp);
            }
        }
        if (pTmp) {
            XColor *xcolor = pTmpComputed->cBackgroundColor->xcolor;
            Tcl_ListObjAppendElement(interp, pArray, 
                    Tcl_NewStringObj("background-color", -1)
            );
            Tcl_ListObjAppendElement(interp, pArray, 
                    Tcl_NewStringObj(Tk_NameOfColor(xcolor), -1)
            );
        }

        Tcl_ListObjAppendElement(interp, pArray, Tcl_NewStringObj("font",-1));
        Tcl_ListObjAppendElement(interp, pArray, 
                Tcl_NewStringObj(pV->fFont->zFont, -1)
        );

        /* If the 'width' attribute is not PIXELVAL_AUTO, pass it to the
         * replacement window.  */
        if (PIXELVAL_AUTO != (iWidth = PIXELVAL(pV, WIDTH, iContaining))) {
            Tcl_Obj *pWidth = Tcl_NewStringObj("width",-1);
            iWidth = MAX(iWidth, 1);
            Tcl_ListObjAppendElement(interp, pArray, pWidth);
            Tcl_ListObjAppendElement(interp, pArray, Tcl_NewIntObj(iWidth));
        }

        /* If the 'height' attribute is not PIXELVAL_AUTO, pass it to the
         * replacement window.  */
        if (PIXELVAL_AUTO != (iHeight = PIXELVAL(pV, HEIGHT, PIXELVAL_AUTO))) {
            Tcl_Obj *pHeight = Tcl_NewStringObj("height",-1);
            iHeight = MAX(iHeight, 1);
            Tcl_ListObjAppendElement(interp, pArray, pHeight);
            Tcl_ListObjAppendElement(interp, pArray, Tcl_NewIntObj(iHeight));
        }

        pScript = Tcl_DuplicateObj(pConfigure);
        Tcl_IncrRefCount(pScript);
        Tcl_ListObjAppendElement(interp, pScript, pArray);
        rc = Tcl_EvalObjEx(interp, pScript, TCL_EVAL_GLOBAL|TCL_EVAL_DIRECT);
        if (rc != TCL_OK) {
            Tcl_BackgroundError(interp);
        }
        Tcl_DecrRefCount(pScript);

        pRes = Tcl_GetObjResult(interp);
        pElem->pReplacement->iOffset = 0;
        Tcl_GetIntFromObj(0, pRes, &pElem->pReplacement->iOffset);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayout --
 * 
 *     Build the internal representation of the formatted document (document
 *     layout). The document layout is stored in the HtmlTree.canvas and
 *     HtmlTree.iCanvasWidth variables.
 *
 * Results:
 *
 * Side effects:
 *     Destroys the existing document layout, if one exists.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlLayout(pTree)
    HtmlTree *pTree;
{
    HtmlNode *pBody = 0;
    int rc = TCL_OK;
    int nWidth;
    int nHeight;
    LayoutContext sLayout;

    /* TODO: At this point we are assuming that the computed style 
     * information in HtmlElementNode.pPropertyValues is complete
     * and up to date. It would be good to run the style engine
     * on the document tree here and assert() none of the pPropertyValues
     * structures change.
     *
     * Of course, such an assert() would be pretty expensive. It would
     * normally be disabled... 
     */

    /* Set variable nWidth to the pixel width of the viewport to render 
     * to. This code should use the actual width of the window if the
     * widget is displayed, or the configured width if it is not (i.e. if 
     * the widget is never packed).
     *
     * It would be better to use the Tk_IsMapped() function here, but for
     * some reason I can't make it work. So instead depend on the observed 
     * behaviour that Tk_Width() returns 1 if the window is not mapped.
     */
    nWidth = Tk_Width(pTree->tkwin);
    if (nWidth < 5 || pTree->options.forcewidth) {
        nWidth = pTree->options.width;
    }
    nHeight = Tk_Height(pTree->tkwin);
    if (nHeight < 5) {
        nHeight = PIXELVAL_AUTO;
    }

    /* Delete any existing document layout. */
    HtmlDrawCleanup(pTree, &pTree->canvas);
    memset(&pTree->canvas, 0, sizeof(HtmlCanvas));

    /* Set up the layout context object. */
    memset(&sLayout, 0, sizeof(LayoutContext));
    sLayout.pTree = pTree;
    sLayout.interp = pTree->interp;

#ifdef LAYOUT_CACHE_DEBUG
    memset(aDebugUseCacheCond, 0, sizeof(int) * (LAYOUT_CACHE_N_USE_COND + 1));
    memset(aDebugStoreCacheCond, 0, sizeof(int)*(LAYOUT_CACHE_N_STORE_COND+1));
#endif

    HtmlLog(pTree, "LAYOUTENGINE", "START", NULL);

    /* Call HtmlLayoutNodeContent() to layout the top level box, generated 
     * by the root node.  
     */
    pBody = pTree->pRoot;
    if (pBody) {
        int y = 0;
        MarginProperties margin;
        BoxProperties box;

        BoxContext sBox;
        NormalFlow sNormal;

        if (pTree->options.shrink) {
            int iMaxWidth = 0;
            blockMinMaxWidth(&sLayout, pBody, 0, &iMaxWidth);
            nWidth = MIN(iMaxWidth, nWidth);
        }

        nodeGetMargins(&sLayout, pBody, nWidth, &margin);
        nodeGetBoxProperties(&sLayout, pBody, nWidth, &box);

        memset(&sBox, 0, sizeof(BoxContext));
        memset(&sNormal, 0, sizeof(NormalFlow));
        sNormal.pFloat = HtmlFloatListNew();
        sNormal.isValid = 1;

        /* Layout content */
        sBox.iContaining =  nWidth;
        sBox.iContainingHeight = nHeight;
        normalFlowLayoutBlock(&sLayout, &sBox, pBody, &y, 0, &sNormal);
        normalFlowMarginCollapse(&sLayout, pBody, &sNormal, &sBox.height);

        /* Copy the content into the tree-canvas (the thing htmldraw.c 
         * actually uses to draw the pretty pictures that were the point
         * of all the shenanigans in this file).
         */
        HtmlDrawCanvas(&pTree->canvas, &sBox.vc, 0, 0, pBody);

        /* This loop takes care of nested "position:fixed" elements. */
        HtmlDrawAddMarker(&pTree->canvas, 0, 0, 1);
        while (sLayout.pFixed) {
            BoxContext sFixed;
            memset(&sFixed, 0, sizeof(BoxContext));
            sFixed.height = Tk_Height(pTree->tkwin);
            if (sFixed.height < 5) sFixed.height = pTree->options.height;
            sFixed.width = Tk_Width(pTree->tkwin);
            sFixed.iContaining = sFixed.width;

            assert(sLayout.pAbsolute == 0);
            sLayout.pAbsolute = sLayout.pFixed;
            sLayout.pFixed = 0;

            drawAbsolute(&sLayout, &sFixed, &pTree->canvas, 0, 0);
            HtmlDrawCanvas(&pTree->canvas, &sFixed.vc, 0, 0, pBody);
        }

        /* Note: Changed to using the actual size of the <body> element 
         * (including margins) for scrolling purposes to avoid a problem
         * with code like:
         *
         *   .class { padding-bottom: 30000px; margin-bottom: -30000px }
         *
         * Example (november 2006): http://www.readwriteweb.com/
         */
        pTree->canvas.right = MAX(pTree->canvas.right, sBox.width);
        pTree->canvas.bottom = MAX(pTree->canvas.bottom, sBox.height);

        HtmlFloatListDelete(sNormal.pFloat);
    }

#ifdef LAYOUT_CACHE_DEBUG
    {
        int ii;
        printf("LayoutCacheDebug: USE: ");
        for (ii = 0; ii <= LAYOUT_CACHE_N_USE_COND; ii++) {
            printf("%d ", aDebugUseCacheCond[ii]);
        }
        printf("\n");
        printf("LayoutCacheDebug: STORE: ");
        for (ii = 0; ii <= LAYOUT_CACHE_N_STORE_COND; ii++) {
            printf("%d ", aDebugStoreCacheCond[ii]);
        }
        printf("\n");
    }
#endif

    HtmlComputedValuesRelease(pTree, sLayout.pImplicitTableProperties);

    if (rc == TCL_OK) {
        pTree->iCanvasWidth = Tk_Width(pTree->tkwin);
        pTree->iCanvasHeight = Tk_Height(pTree->tkwin);
        if (pTree->options.shrink) {
            Tk_GeometryRequest(
                pTree->tkwin, pTree->canvas.right, pTree->canvas.bottom
            );
            Tk_SetMinimumRequestSize(
                pTree->tkwin, pTree->canvas.right, pTree->canvas.bottom
            );
        }
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlLayoutInvalidateCache --
 * 
 *     Invalidate the layout-cache for the specified node. 
 *
 *     Note that the layout-caches of the parent and ancestor nodes are NOT
 *     invalidated. If the caller wants the layout to be recomputed next time
 *     HtmlLayout() is called, the layout-caches of the parent and ancestor
 *     nodes must also be invalidated (by calling this function).
 * 
 * Results:
 *     None.
 *
 * Side effects:
 *     Deletes the layout cache for node pNode.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlLayoutInvalidateCache(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (!HtmlNodeIsText(pNode)) {
        HtmlElementNode *pElem = (HtmlElementNode *)pNode;
        if (pElem->pLayoutCache) {
            HtmlDrawCleanup(pTree, &pElem->pLayoutCache->aCache[0].canvas);
            HtmlDrawCleanup(pTree, &pElem->pLayoutCache->aCache[1].canvas);
            HtmlDrawCleanup(pTree, &pElem->pLayoutCache->aCache[2].canvas);
            HtmlFree(pElem->pLayoutCache);
            pElem->pLayoutCache = 0;
        }
    }
}
