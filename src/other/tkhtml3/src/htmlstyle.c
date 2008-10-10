
/*
 * htmlstyle.c ---
 *
 *     This file applies the cascade algorithm using the stylesheet
 *     code in css.c to the tree built with code in htmltree.c
 *
 *--------------------------------------------------------------------------
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
static const char rcsid[] = "$Id: htmlstyle.c,v 1.61 2007/12/12 04:50:29 danielk1977 Exp $";

#include "html.h"
#include <assert.h>
#include <string.h>

void
HtmlDelScrollbars(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlElementNode *pElem = (HtmlElementNode *)pNode;

    if (!HtmlNodeIsText(pNode) && pElem->pScrollbar) {
        HtmlNodeScrollbars *p = pElem->pScrollbar;
        if (p->vertical.win) {
	    /* Remove any entry from the HtmlTree.pMapped list. */
            if (&p->vertical == pTree->pMapped) {
                pTree->pMapped = p->vertical.pNext;
            } else {
                HtmlNodeReplacement *pCur = pTree->pMapped; 
                while( pCur && pCur->pNext != &p->vertical ) pCur = pCur->pNext;
                if (pCur) {
                    pCur->pNext = p->vertical.pNext;
                }
            }

            Tk_DestroyWindow(p->vertical.win);
            Tcl_DecrRefCount(p->vertical.pReplace);
        }
        if (p->horizontal.win) {
	    /* Remove any entry from the HtmlTree.pMapped list. */
            if (&p->horizontal == pTree->pMapped) {
                pTree->pMapped = p->horizontal.pNext;
            } else {
                HtmlNodeReplacement *pCur = pTree->pMapped; 
                while( pCur && pCur->pNext != &p->horizontal ) {
                    pCur = pCur->pNext;
                }
                if (pCur) {
                    pCur->pNext = p->horizontal.pNext;
                }
            }

            Tk_DestroyWindow(p->horizontal.win);
            Tcl_DecrRefCount(p->horizontal.pReplace);
        }
        HtmlFree(p);
        pElem->pScrollbar = 0;
    }
}

void
HtmlDelStackingInfo(pTree, pElem)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
{
    HtmlNodeStack *pStack = pElem->pStack;
    if (pStack && pStack->pElem == pElem){
        if (pStack->pPrev) {
            pStack->pPrev->pNext = pStack->pNext;
        } 
        if (pStack->pNext) {
            pStack->pNext->pPrev = pStack->pPrev;
        } 
        if (pStack==pTree->pStack) {
          pTree->pStack = pStack->pNext;
        }
        assert(!pTree->pStack || !pTree->pStack->pPrev);

        HtmlFree(pStack);
        pTree->nStack--;
    }
    pElem->pStack = 0;
}

#define STACK_NONE      0
#define STACK_FLOAT     1
#define STACK_AUTO      2
#define STACK_CONTEXT   3
static int 
stackType(p) 
    HtmlNode *p;
{
    HtmlComputedValues *pV = HtmlNodeComputedValues(p);

    /* STACK_CONTEXT is created by the root element and any
     * positioned block with (z-index!='auto).
     */
    if (
        (!HtmlNodeParent(p)) ||
        (pV->ePosition != CSS_CONST_STATIC && pV->iZIndex != PIXELVAL_AUTO)
    ) {
        return STACK_CONTEXT;
    }

    /* Postioned elements with 'auto' z-index are STACK_AUTO. */
    if (pV->ePosition != CSS_CONST_STATIC) {
        return STACK_AUTO;
    }

    /* Floating boxes are STACK_FLOAT. */
    if (pV->eFloat != CSS_CONST_NONE){
        return STACK_FLOAT;
    }

    return STACK_NONE;
}

static void
addStackingInfo(pTree, pElem)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
{
    HtmlNode *pNode = (HtmlNode *)pElem;
    int eStack = stackType(pNode);
    
    /* A node forms a new stacking context if it is positioned or floating.
     * Or if it is the root node. We only need create an HtmlNodeStack if this
     * is the case.
     */
    if (eStack != STACK_NONE) {
        HtmlNodeStack *pStack = HtmlNew(HtmlNodeStack);

        pStack->pElem = pElem;
        pStack->eType = eStack;
        pStack->pNext = pTree->pStack;
        if( pStack->pNext ){
            pStack->pNext->pPrev = pStack;
        }
        pTree->pStack = pStack;
        pElem->pStack = pStack;
        pTree->cb.flags |= HTML_STACK;
        pTree->nStack++;
    } else {
        pElem->pStack = ((HtmlElementNode *)HtmlNodeParent(pNode))->pStack;
    }
    assert(pElem->pStack);
}

#define STACK_STACKING  1
#define STACK_BLOCK     3
#define STACK_INLINE    5
typedef struct StackCompare StackCompare;
struct StackCompare {
    HtmlNodeStack *pStack;
    int eStack;
};


/*
 *---------------------------------------------------------------------------
 *
 * scoreStack --
 *
 * Results:
 *     1 -> Border and background of stacking context box.
 *     2 -> Descendants with negative z-index values.
 *     3 -> In-flow, non-inline descendants.
 *     4 -> Floats and their contents.
 *     5 -> In-flow, inline descendants.
 *     6 -> Positioned descendants with z-index values of "auto" or "0".
 *     7 -> Descendants with positive z-index values.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
scoreStack(pParentStack, pStack, eStack)
    HtmlNodeStack *pParentStack;
    HtmlNodeStack *pStack;
    int eStack;
{
    int z;
    if (pStack == pParentStack) {
        return eStack;
    }
    assert(pStack->pElem->node.pParent);
    if (pStack->eType == STACK_FLOAT) return 4;
    if (pStack->eType == STACK_AUTO) return 6;
    z = pStack->pElem->pPropertyValues->iZIndex;
    assert(z != PIXELVAL_AUTO);
    if (z == 0) return 6;
    if (z < 0) return 2;
    return 7;
}

#define IS_STACKING_CONTEXT(x) (                                    \
         x == x->pStack->pElem && x->pStack->eType == STACK_CONTEXT \
)

static void setStackingContext(p, ppOut)
    HtmlElementNode *p;
    HtmlNodeStack **ppOut;
{
    if (p == p->pStack->pElem) {
        HtmlNodeStack *pS = p->pStack;
        if (pS->eType == STACK_CONTEXT || (*ppOut)->eType != STACK_CONTEXT) {
            *ppOut = pS;
        }
    }
}


static int
stackCompare(pVoidLeft, pVoidRight)
    const void *pVoidLeft;
    const void *pVoidRight;
{
    StackCompare *pLeft = (StackCompare *)pVoidLeft;
    StackCompare *pRight = (StackCompare *)pVoidRight;

    HtmlNodeStack *pLeftStack = pLeft->pStack;
    HtmlNodeStack *pRightStack = pRight->pStack;
    HtmlNodeStack *pParentStack = 0;

    int nLeftDepth = -1;        /* Tree depth of node pLeftStack->pNode */
    int nRightDepth = -1;       /* Tree depth of node pRightStack->pNode */

    int iLeft;
    int iRight;
    int iRes;
    int iTreeOrder = 0;

    int ii;
    HtmlElementNode *pL;
    HtmlElementNode *pR;

    /* There are three scenarios:
     *
     *     1) pLeft and pRight are associated with the same HtmlNodeStack
     *        structure. In this case "inline" beats "block" and "block"
     *        beats "stacking".
     *
     *     2) pLeft is descended from pRight, or vice versa.
     *
     *     3) Both are descended from a common stacking context.
     */

    /* Calculate pLeftStack, pRightStack and pParentStack */
    for (pL = pLeftStack->pElem; pL; pL = HtmlElemParent(pL)) nLeftDepth++;
    for (pR = pRightStack->pElem; pR; pR = HtmlElemParent(pR)) nRightDepth++;
    pL = pLeftStack->pElem;
    pR = pRightStack->pElem;
    for (ii = 0; ii < MAX(0, nLeftDepth - nRightDepth); ii++) {
        setStackingContext(pL, &pLeftStack);
        pL = HtmlElemParent(pL);
        iTreeOrder = +1;
    }
    for (ii = 0; ii < MAX(0, nRightDepth - nLeftDepth); ii++) {
        setStackingContext(pR, &pRightStack);
        pR = HtmlElemParent(pR);
        iTreeOrder = -1;
    }
    while (pR != pL) {
        HtmlElementNode *pParentL = HtmlElemParent(pL);
        HtmlElementNode *pParentR = HtmlElemParent(pR);
        setStackingContext(pL, &pLeftStack);
        setStackingContext(pR, &pRightStack);
        if (pParentL == pParentR) {
            iTreeOrder = 0;
            for (ii = 0; 0 == iTreeOrder; ii++) {
                HtmlNode *pChild = HtmlNodeChild(&pParentL->node, ii);
                if (pChild == (HtmlNode *)pL) {
                    iTreeOrder = -1;
                }
                if (pChild == (HtmlNode *)pR) {
                    iTreeOrder = +1;
                }
            }
            assert(iTreeOrder != 0);
        }
        pL = pParentL;
        pR = pParentR;
        assert(pL && pR);
    }
    while (pR->pStack->pElem != pR) {
        pR = HtmlElemParent(pR);
        assert(pR);
    }
    pParentStack = pR->pStack;

    iLeft = scoreStack(pParentStack, pLeftStack, pLeft->eStack);
    iRight = scoreStack(pParentStack, pRightStack, pRight->eStack);

    iRes = iLeft - iRight;
    if (iRes == 0 && (iRight == 2 || iRight == 6 || iRight == 7)) {
        int z1 = pLeftStack->pElem->pPropertyValues->iZIndex;
        int z2 = pRightStack->pElem->pPropertyValues->iZIndex;
        if (z1 == PIXELVAL_AUTO) z1 = 0;
        if (z2 == PIXELVAL_AUTO) z2 = 0;
        iRes = z1 - z2;
    }
    /* if (iRes == 0 && (iRight == 4 || pLeftStack == pRightStack)) { */
    if (iRes == 0 && pLeftStack == pRightStack) {
        iRes = (pLeft->eStack - pRight->eStack);
    }
    if (iRes == 0) {
        assert(iTreeOrder != 0);
        iRes = iTreeOrder;
    }
    return iRes;
}

/*
 *---------------------------------------------------------------------------
 *
 * checkStackSort --
 *
 *     This function is equivalent to an assert() statement. It is not
 *     part of the functionality of Tkhtml3, but is used to check the
 *     integrity of internal data structures.
 *
 *     The first argument, aStack, should be an array of nStack (the
 *     second arg) StackCompare structure. An assert fails inside
 *     this function if the array is not sorted in ascending order.
 *
 *     The primary purpose of this test is to ensure that the stackCompare()
 *     comparision function is stable. It is quite an expensive check,
 *     so is normally disabled at compile time. Change the "#if 0"
 *     below to reenable the checking.
 *
 *     NOTE: If you got this file from tkhtml.tcl.tk and there is an 
 *           "#if 1" in the code below, I have checked it in by mistake.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#ifdef NDEBUG
  #define checkStackSort(a,b,c)
#else
static void 
checkStackSort(pTree, aStack, nStack)
    HtmlTree *pTree;
    StackCompare *aStack;
    int nStack;
{
#if 0
    int ii;
    int jj;
    for (ii = 0; ii < nStack; ii++) {
      for (jj = ii + 1; jj < nStack; jj++) {
        int r1 = stackCompare(&aStack[ii], &aStack[jj]);
        int r2 = stackCompare(&aStack[jj], &aStack[ii]);
        assert(r1 && r2);
        assert((r1 * r2) < 0);
        assert(r1 < 0);
      }
    }
#endif
}
#endif


/*
 *---------------------------------------------------------------------------
 *
 * HtmlRestackNodes --
 *
 *     This function is called from with the callbackHandler() routine
 *     after updating the computed properties of the tree.
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
HtmlRestackNodes(pTree)
    HtmlTree *pTree;
{
    HtmlNodeStack *pStack;
    StackCompare *apTmp;
    int iTmp = 0;
    if (0 == (pTree->cb.flags & HTML_STACK)) return;

    apTmp = (StackCompare *)HtmlAlloc(0, sizeof(StackCompare)*pTree->nStack*3);

    for (pStack = pTree->pStack; pStack; pStack = pStack->pNext) {
        apTmp[iTmp].pStack = pStack;
        apTmp[iTmp].eStack = STACK_BLOCK;
        apTmp[iTmp+1].pStack = pStack;
        apTmp[iTmp+1].eStack = STACK_INLINE;
        apTmp[iTmp+2].pStack = pStack;
        apTmp[iTmp+2].eStack = STACK_STACKING;
        iTmp += 3;
    }
    assert(iTmp == pTree->nStack * 3);

    qsort(apTmp, pTree->nStack * 3, sizeof(StackCompare), stackCompare);

    for (iTmp = 0; iTmp < pTree->nStack * 3; iTmp++) {
#if 0
printf("Stack %d: %s %s\n", iTmp, 
    Tcl_GetString(HtmlNodeCommand(pTree, &apTmp[iTmp].pStack->pElem->node)),
    (apTmp[iTmp].eStack == STACK_INLINE ? "inline" : 
     apTmp[iTmp].eStack == STACK_BLOCK ? "block" : "stacking")
);
#endif
        switch (apTmp[iTmp].eStack) {
            case STACK_INLINE:
                apTmp[iTmp].pStack->iInlineZ = iTmp;
                break;
            case STACK_BLOCK:
                apTmp[iTmp].pStack->iBlockZ = iTmp;
                break;
            case STACK_STACKING:
                apTmp[iTmp].pStack->iStackingZ = iTmp;
                break;
        }
    }
    checkStackSort(pTree, apTmp, pTree->nStack * 3);

    pTree->cb.flags &= (~HTML_STACK);
    HtmlFree(apTmp);
}

/*
 *---------------------------------------------------------------------------
 *
 * styleNode --
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
styleNode(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    CONST char *zStyle;      /* Value of "style" attribute for node */
    int trashDynamics = (int)((size_t) clientData);

    HtmlElementNode *pElem = (HtmlElementNode *)pNode;
    HtmlComputedValues *pV = pElem->pPropertyValues;
    pElem->pPropertyValues = 0;
    HtmlDelStackingInfo(pTree, pElem);

    /* If the clientData was set to a non-zero value, then the 
     * stylesheet configuration has changed. In this case we need to
     * recalculate the nodes list of dynamic conditions.
     */
    if (trashDynamics) {
        HtmlCssFreeDynamics(pElem);
    }

    /* If there is a "style" attribute on this node, parse the attribute
     * value and put the resulting mini-stylesheet in pNode->pStyle. 
     *
     * We assume that if the pStyle attribute is not NULL, then this node
     * has been styled before. The stylesheet configuration may have
     * changed since then, so we have to recalculate pNode->pProperties,
     * but the "style" attribute is constant so pStyle is never invalid.
     *
     * Actually, the style attribute can be modified by the user, using 
     * the [$node attribute style "new-value"] command. In this case
     * the style attribute is treated as a special case and the 
     * pElem->pStyle structure is invalidated/recalculated as required.
     */
    if (!pElem->pStyle) {
        zStyle = HtmlNodeAttr(pNode, "style");
        if (zStyle) {
            HtmlCssInlineParse(pTree, -1, zStyle, &pElem->pStyle);
        }
    }

    /* Recalculate the properties for this node */
    HtmlCssStyleSheetApply(pTree, pNode);
    HtmlComputedValuesRelease(pTree, pElem->pPreviousValues);
    pElem->pPreviousValues = pV;

    addStackingInfo(pTree, pElem);

    /* Compare the new computed property set with the old. If
     * ComputedValuesCompare() returns 0, then the properties have
     * not changed (in any way that affects rendering). If it
     * returns 1, then some aspect has changed that does not
     * require a relayout (i.e. 'color', or 'text-decoration'). 
     * If it returns 2, then something has changed that does require
     * relayout (i.e. 'display', 'font-size').
     */
    return HtmlComputedValuesCompare(pElem->pPropertyValues, pV);
}

typedef struct StyleCounter StyleCounter;
struct StyleCounter {
  char *zName;
  int iValue;
};

struct StyleApply {
  /* Node to begin recalculating style at */
  HtmlNode *pRestyle;

  /* True if currently traversing pRestyle, or a descendent, right-sibling
   * or descendent of a right-sibling of pRestyle.
   */
  int doStyle;

  int doContent;

  /* True if the whole tree is being restyled. */
  int isRoot;

  StyleCounter **apCounter;
  int nCounter;
  int nCounterAlloc;
  int nCounterStartScope;

  /* True if we have seen one or more "fixed" items */
  int isFixed;
};
typedef struct StyleApply StyleApply;

static void 
styleApply(pTree, pNode, p)
    HtmlTree *pTree;
    HtmlNode *pNode;
    StyleApply *p;
{
    int i;
    int doStyle;
    int nCounterStartScope;
    int redrawmode = 0;
    HtmlElementNode *pElem = HtmlNodeAsElement(pNode);

    /* Text nodes do not have an associated style. */
    if (!pElem) return;

    if (p->pRestyle == pNode) {
        p->doStyle = 1;
    }

    if (p->doStyle) {
        redrawmode = styleNode(pTree, pNode, (ClientData) ((size_t) p->isRoot));

        /* If there has been a style-callback configured (-stylecmd option to
         * the [nodeHandle replace] command) for this node, invoke it now.
         */
        if (pElem->pReplacement && pElem->pReplacement->pStyleCmd) {
            Tcl_Obj *pCmd = pElem->pReplacement->pStyleCmd;
            int rc = Tcl_EvalObjEx(pTree->interp, pCmd, TCL_EVAL_GLOBAL);
            if (rc != TCL_OK) {
                Tcl_BackgroundError(pTree->interp);
            }
        }
    }

    HtmlStyleHandleCounters(pTree, HtmlNodeComputedValues(pNode));
    nCounterStartScope = p->nCounterStartScope;
    p->nCounterStartScope = p->nCounter;

    if (p->doStyle || p->doContent) {
        /* Destroy current generated content */
        if (pElem->pBefore || pElem->pAfter) {
            HtmlNodeClearGenerated(pTree, pElem);
            redrawmode = MAX(redrawmode, 2);
        }

        /* Generate :before content */
        HtmlCssStyleGenerateContent(pTree, pElem, 1);
        if (pElem->pBefore) {
            ((HtmlElementNode *)(pElem->pBefore))->pStack = pElem->pStack;
            pElem->pBefore->pParent = pNode;
            pElem->pBefore->iNode = -1;
        }
    } else if (pElem->pBefore) {
        HtmlStyleHandleCounters(pTree, HtmlNodeComputedValues(pElem->pBefore));
    }

    doStyle = p->doStyle;
    for (i = 0; i < HtmlNodeNumChildren(pNode); i++) {
        styleApply(pTree, HtmlNodeChild(pNode, i), p);
    }
    p->doStyle = doStyle;

    if (p->doStyle || p->doContent) {
        /* Generate :after content */
        HtmlCssStyleGenerateContent(pTree, pElem, 0);
        if (pElem->pAfter) {
            ((HtmlElementNode *)(pElem->pAfter))->pStack = pElem->pStack;
            pElem->pAfter->pParent = pNode;
            pElem->pAfter->iNode = -1;
        }

        if (pElem->pBefore || pElem->pAfter) {
            redrawmode = MAX(redrawmode, 2);
        }
    } else if(pElem->pAfter) {
        HtmlStyleHandleCounters(pTree, HtmlNodeComputedValues(pElem->pAfter));
    }

    for (i = p->nCounterStartScope; i < p->nCounter; i++) {
        HtmlFree(p->apCounter[i]);
    }
    p->nCounter = p->nCounterStartScope;
    p->nCounterStartScope = nCounterStartScope;

    if (redrawmode == 3) {
        HtmlCallbackLayout(pTree, pNode);
        HtmlCallbackDamageNode(pTree, pNode);
        p->doContent = 1;
    } else if (redrawmode == 2) {
        HtmlCallbackLayout(pTree, pNode);
        HtmlCallbackDamageNode(pTree, pNode);
    } else if (redrawmode == 1) {
        /* HtmlCallbackLayout(pTree, pNode); */
        HtmlCallbackDamageNode(pTree, pNode);
    }

    /* If this element was either the <body> or <html> nodes,
     * go ahead and repaint the entire display. The worst that
     * can happen is that we have to paint a little extra
     * area if the document background is set by the <HTML>
     * element.
     */
    if (redrawmode && (
            (HtmlNode *)pElem == pTree->pRoot || 
            (HtmlNode *)pElem == HtmlNodeChild(pTree->pRoot, 1)
        )
    ) {
        HtmlCallbackDamage(pTree, 0, 0, 1000000, 1000000);
    }

    if (pElem->pPropertyValues->eDisplay != CSS_CONST_NONE && (
        pElem->pPropertyValues->ePosition == CSS_CONST_FIXED ||
        pElem->pPropertyValues->eBackgroundAttachment == CSS_CONST_FIXED
    )) {
        p->isFixed = 1;
    }
}

static void addCounterEntry(p, zName, iValue)
    StyleApply *p;
    const char *zName;
    int iValue;
{
    StyleCounter *pCounter;

    int n;
    if (p->nCounterAlloc < (p->nCounter + 1)) {
        int nByte;
        p->nCounterAlloc += 10;
        nByte = p->nCounterAlloc * sizeof(HtmlCounterList *);
        p->apCounter = (StyleCounter **)HtmlRealloc(
            "apCounter", p->apCounter, nByte
        );
    }

    n = sizeof(StyleCounter) + strlen(zName) + 1;
    pCounter = (StyleCounter *)HtmlAlloc("StyleCounter", n);
    pCounter->zName = (char *)&pCounter[1];
    strcpy(pCounter->zName, zName);
    pCounter->iValue = iValue;
    p->apCounter[p->nCounter] = pCounter;
    p->nCounter++;
}

void
HtmlStyleHandleCounters(pTree, pComputed)
    HtmlTree *pTree;
    HtmlComputedValues *pComputed;
{
    StyleApply *p = (StyleApply *)pTree->pStyleApply;

    HtmlCounterList *pReset = pComputed->clCounterReset;
    HtmlCounterList *pIncr = pComputed->clCounterIncrement;


    /* Section 12.4.3 of CSS 2.1: Elements with "display:none" neither
     * increment or reset counters.
     */
    if (pComputed->eDisplay == CSS_CONST_NONE) {
        return;
    }

    if (pReset) {
        int ii;

        for (ii = 0; ii < pReset->nCounter; ii++) {
            StyleCounter *pCounter = 0;
            int jj;
            for (jj = p->nCounterStartScope; jj < p->nCounter; jj++) {
                if (!strcmp(pReset->azCounter[ii], p->apCounter[jj]->zName)) {
                    pCounter = p->apCounter[jj];
                    pCounter->iValue = pReset->anValue[ii];
                    break;
                }
            }
            if (pCounter == 0) {
                addCounterEntry(p, pReset->azCounter[ii], pReset->anValue[ii]);
            }
        }
    }

    if (pIncr) {
        int ii;
        for (ii = 0; ii < pIncr->nCounter; ii++) {
            int jj;
            for (jj = p->nCounter - 1; jj >= 0; jj--) {
                if (0 == strcmp(pIncr->azCounter[ii],p->apCounter[jj]->zName)) {
                    p->apCounter[jj]->iValue += pIncr->anValue[ii];
                    break;
                }
            }

            if (jj < 0) {
                /* No counter with the specified name is found. Act as if 
                 * there is a 'counter-reset: zName iValue' directive.
                 */
                addCounterEntry(p, pIncr->azCounter[ii], pIncr->anValue[ii]);
            }
        }
    }
}

int HtmlStyleCounters(pTree, zName, aValue, nValue)
    HtmlTree *pTree;
    const char *zName;
    int *aValue;
    int nValue;
{
    int ii;
    StyleApply *p = (StyleApply *)(pTree->pStyleApply);

    int n = 0;

    for (ii = 0; ii < p->nCounter && n < nValue; ii++) {
        if (0 == strcmp(zName, p->apCounter[ii]->zName)) {
            aValue[n] = p->apCounter[ii]->iValue;
            n++;
        }
    }

    if (n == 0) {
        n = 1;
        aValue[0] = 0;
    }

    return n;
}

int HtmlStyleCounter(pTree, zName)
    HtmlTree *pTree;
    const char *zName;
{
    int ii;
    StyleApply *p = (StyleApply *)(pTree->pStyleApply);

    for (ii = p->nCounter - 1; ii >= 0; ii--) {
        if (0 == strcmp(zName, p->apCounter[ii]->zName)) {
            return p->apCounter[ii]->iValue;
        }
    }

    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlStyleApply --
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
HtmlStyleApply(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    StyleApply sApply;
    int isRoot = ((pNode == pTree->pRoot) ? 1 : 0);
    HtmlLog(pTree, "STYLEENGINE", "START");

    memset(&sApply, 0, sizeof(StyleApply));
    sApply.pRestyle = pNode;
    sApply.isRoot = isRoot;

    assert(pTree->pStyleApply == 0);
    pTree->pStyleApply = (void *)&sApply;
    styleApply(pTree, pTree->pRoot, &sApply);
    pTree->pStyleApply = 0;
    pTree->isFixed = sApply.isFixed;
    HtmlFree(sApply.apCounter);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlStyleSyntaxErrs --
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
HtmlStyleSyntaxErrs(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    int nSyntaxErrs = 0;
    if( pTree->pStyle ){
        nSyntaxErrs = HtmlCssStyleSheetSyntaxErrs(pTree->pStyle);
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(nSyntaxErrs));
    return TCL_OK;
}

