/*
 * HtmlTree.c ---
 *
 *     This file implements the tree structure that can be used to access
 *     elements of an HTML document.
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

static const char rcsid[] = "$Id: htmltree.c,v 1.161 2008/02/14 08:39:14 danielk1977 Exp $";

#include "html.h"
#include "swproc.h"
#include <assert.h>
#include <string.h>


struct HtmlFragmentContext {
  HtmlNode *pRoot;
  HtmlElementNode *pCurrent;
  Tcl_Obj *pNodeList;
};

/*
 * An HTML table is structured as follows:
 *
 *  <TABLE>                            <!-- Level 4 -->
 *    <TBODY|THEAD|TFOOT>              <!-- Level 3 -->
 *      <TR>                           <!-- Level 2 -->
 *        <TH|TD>                      <!-- Level 1 -->
 *
 * Other, non-table, elements have a level of 0. 
 *
 * Currently Tkhtml does not even parse <COL> and <COLGROUP> tags. When
 * this is fixed these two elements may need to be factored into the macro.
 */
#define TAG_TO_TABLELEVEL(eTag) (  \
    (eTag==Html_TABLE) ? 4 :       \
    (eTag==Html_TBODY) ? 3 :       \
    (eTag==Html_THEAD) ? 3 :       \
    (eTag==Html_TFOOT) ? 3 :       \
    (eTag==Html_TR)    ? 2 :       \
    (eTag==Html_TD)    ? 1 :       \
    (eTag==Html_TH)    ? 1 : 0     \
)

static void treeCloseFosterTree(HtmlTree *);

/*
 *---------------------------------------------------------------------------
 *
 * explicitCloseCount --
 *
 *
 * Results:
 *     Sets the value of *pNClose.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
explicitCloseCount(pCurrent, eTag, zTag, pNClose)
    HtmlNode *pCurrent;     /* Node currently being constructed */
    int eTag;               /* Id of closing tag (i.e. "</p>" -> Html_P) */
    const char *zTag;       /* Atom of closing tag */
    int *pNClose;           /* OUT: Number of elements to close */
{
    *pNClose = 0;
    if (eTag != Html_HTML && eTag != Html_BODY && eTag != Html_HEAD) {
        HtmlNode *p;
        int nLevel = 0;

        for (p = pCurrent; p;  p = HtmlNodeParent(p)) {
            nLevel++;

            assert(zTag == p->zTag || stricmp(zTag, p->zTag));
            if (zTag == p->zTag) {
                *pNClose = nLevel;
                break;
            }

            if (
                TAG_TO_TABLELEVEL(p->eTag) &&
                TAG_TO_TABLELEVEL(eTag) <= TAG_TO_TABLELEVEL(p->eTag)
            ) break;
        }
    }
}

static void 
implicitCloseCount(pTree, pCurrent, eTag, pNClose)
    HtmlTree *pTree;
    HtmlNode *pCurrent;
    int eTag;
    int *pNClose;
{
    int nClose = 0;

    if (pCurrent) {
        int nLevel = 0;
        HtmlNode *p;
        int eCloseRes = TAG_PARENT;
        assert(HtmlNodeAsElement(pCurrent));
    
        for (p = pCurrent; p && eCloseRes != TAG_OK; p = HtmlNodeParent(p)) {
            HtmlTokenMap *pMap = HtmlMarkup(HtmlNodeTagType(p));
            nLevel++;
    
            if (pMap && pMap->xClose) {
                eCloseRes = pMap->xClose(pTree, p, eTag);
                assert(
                    eCloseRes == TAG_CLOSE || 
                    eCloseRes == TAG_OK || 
                    eCloseRes == TAG_PARENT
                );
                if (eCloseRes == TAG_CLOSE) {
                    nClose = nLevel;
                }
            }
        }
    }

    *pNClose = nClose;
}

static void
geomRequestProcCb(clientData) 
    ClientData clientData;
{
    HtmlNode *pNode = (HtmlNode *)clientData;
    HtmlTree *pTree = pNode->pNodeCmd->pTree;
    HtmlCallbackLayout(pTree, pNode);
}

static void 
geomRequestProc(clientData, widget)
    ClientData clientData;
    Tk_Window widget;
{
    HtmlNode *pNode = (HtmlNode *)clientData;
    HtmlTree *pTree = pNode->pNodeCmd->pTree;
    if (!pTree->cb.inProgress) {
        HtmlCallbackLayout(pTree, pNode);
    } else {
        Tcl_DoWhenIdle(geomRequestProcCb, (ClientData)pNode);
    }
}

static void
clearReplacement(pTree, pElem)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
{
    HtmlNodeReplacement *p = pElem->pReplacement;
    pElem->pReplacement = 0;
    if (p) {

        /* Cancel any idle callback scheduled by geomRequestProc() */
        Tcl_CancelIdleCall(geomRequestProcCb, (ClientData)pElem);

        /* If there is a delete script, invoke it now. */
        if (p->pDelete) {
            int flags = TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL;
            Tcl_EvalObjEx(pTree->interp, p->pDelete, flags);
        }

        /* Remove any entry from the HtmlTree.pMapped list. */
        if (p == pTree->pMapped) {
            pTree->pMapped = p->pNext;
        } else {
            HtmlNodeReplacement *pCur = pTree->pMapped; 
            while( pCur && pCur->pNext != p ) pCur = pCur->pNext;
            if (pCur) {
                pCur->pNext = p->pNext;
            }
        }

        /* Cancel geometry management */
        if (p->win) {
            if (Tk_IsMapped(p->win)) {
                Tk_UnmapWindow(p->win);
            }
            Tk_ManageGeometry(p->win, 0, 0);
        }

        /* Delete the Tcl_Obj's and the structure itself. */
        if (p->pDelete)       { Tcl_DecrRefCount(p->pDelete); }
        if (p->pReplace)      { Tcl_DecrRefCount(p->pReplace); }
        if (p->pConfigureCmd) { Tcl_DecrRefCount(p->pConfigureCmd); }
        HtmlFree(p);
    }
}

int 
HtmlNodeClearStyle(pTree, pElem)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
{
    if (pElem) {
        HtmlNodeClearGenerated(pTree, pElem);
        HtmlComputedValuesRelease(pTree, pElem->pPropertyValues);
        HtmlComputedValuesRelease(pTree, pElem->pPreviousValues);
        HtmlCssInlineFree(pElem->pStyle);
        HtmlCssFreeDynamics(pElem);
        pElem->pStyle = 0;
        pElem->pPropertyValues = 0;
        pElem->pPreviousValues = 0;
        pElem->pDynamic = 0;
        HtmlDelStackingInfo(pTree, pElem);
    }
    return 0;
}

int 
HtmlNodeDeleteCommand(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pNode->pNodeCmd) {
        Tcl_Obj *pCommand = pNode->pNodeCmd->pCommand;
        Tcl_DeleteCommand(pTree->interp, Tcl_GetString(pCommand));
        Tcl_DecrRefCount(pCommand);
        HtmlFree(pNode->pNodeCmd);
        pNode->pNodeCmd = 0;
    }
    return 0;
}


/*
 *---------------------------------------------------------------------------
 *
 * freeNode --
 *
 *     Free the memory allocated for pNode and all of it's children. If the
 *     node has attached style information, either from stylesheets or an
 *     Html style attribute, this is deleted here too.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     pNode and children are made invalid.
 *
 *---------------------------------------------------------------------------
 */
static void 
freeNode(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if( pNode ){
        int i;

        /* Invalidate the cache of the parent node before deleting any
         * child nodes. This is because invalidating a cache may involve
         * deleting primitives that correspond to descendant nodes. In
         * general, primitives must be deleted before their owner nodes.
         */
        HtmlLayoutInvalidateCache(pTree, pNode);

        if (!HtmlNodeIsText(pNode)) {
            /* Do HtmlElementNode specific destruction */
            HtmlElementNode *pElem = (HtmlElementNode *)pNode;
            HtmlFree(pElem->pAttributes);

            /* Delete the computed values caches. */
            HtmlNodeClearStyle(pTree, pElem);
            HtmlCssFreeDynamics(pElem);

            if (pElem->pOverride) {
                Tcl_DecrRefCount(pElem->pOverride);
                pElem->pOverride = 0;
            }

            /* Delete the descendant nodes. */
            for(i=0; i < pElem->nChild; i++){
                freeNode(pTree, pElem->apChildren[i]);
            }
            HtmlFree(pElem->apChildren);

            clearReplacement(pTree, pElem);

            HtmlDrawCanvasItemRelease(pTree, pElem->pBox);

        } else {
            HtmlTextNode *pTextNode = HtmlNodeAsText(pNode);
            assert(pTextNode);
            HtmlTagCleanupNode(pTextNode);
            HtmlFree(pTextNode->aToken);
        }

        /* Delete the computed values caches. */
        HtmlDelScrollbars(pTree, pNode);

        HtmlNodeDeleteCommand(pTree, pNode);

        HtmlFree(pNode);
    }
}

int
HtmlNodeClearGenerated(pTree, pElem)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
{
    assert(!pElem->pBefore || !HtmlNodeIsText(pElem->pBefore));
    freeNode(pTree, pElem->pBefore);
    freeNode(pTree, pElem->pAfter);
    pElem->pBefore = 0;
    pElem->pAfter = 0;
    return 0;
}

static Tcl_Obj *
nodeGetPreText(pTextNode)
    HtmlTextNode *pTextNode;
{
    HtmlTextIter sIter;
    Tcl_Obj *pRet = Tcl_NewObj();

    for (
        HtmlTextIterFirst(pTextNode, &sIter);
        HtmlTextIterIsValid(&sIter);
        HtmlTextIterNext(&sIter)
    ) {
        char *zWhite = " ";

        int eType = HtmlTextIterType(&sIter);
        int nData = HtmlTextIterLength(&sIter);
        char const * zData = HtmlTextIterData(&sIter);

        switch (eType) {
            case HTML_TEXT_TOKEN_TEXT:
                Tcl_AppendToObj(pRet, zData, nData);
                break;

            case HTML_TEXT_TOKEN_NEWLINE: 
                zWhite = "\n";
            case HTML_TEXT_TOKEN_SPACE: {
                int ii;
                for (ii = 0; ii < nData; ii++) {
                    Tcl_AppendToObj(pRet, zWhite, 1);
                }
                break;
            }
        }
    }

    return pRet;
}


/*
 *---------------------------------------------------------------------------
 *
 * nodeRemoveChild --
 *
 *     If pChild is a child-node of pElem, then remove it from the
 *     HtmlElementNode.apChildren[] array (so that it is no longer a 
 *     child).
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
nodeRemoveChild(pElem, pChild)
    HtmlElementNode *pElem;
    HtmlNode *pChild;
{
    int eSeen = 0;
    int ii;

    for (ii = 0; ii < pElem->nChild; ii++) {
        if (eSeen) {
            pElem->apChildren[ii - 1] = pElem->apChildren[ii];
        }
        if (pElem->apChildren[ii] == pChild) {
            assert(pChild->pParent == (HtmlNode *)pElem);
            pChild->pParent = 0;
            eSeen = 1;
        }
    }
    if (eSeen) {
        pElem->nChild--;
    }
    return eSeen;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlElementNormalize --
 *
 *     This function normalizes the text children of element *pElem.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     May combine two or more text-nodes into a single node.
 *
 *---------------------------------------------------------------------------
 */
void
HtmlElementNormalize(pElem)
    HtmlElementNode *pElem;
{
    int ii;
    for (ii = 0; ii < (pElem->nChild - 1); ii++) {
        if (
            HtmlNodeIsText(pElem->apChildren[ii]) &&
            HtmlNodeIsText(pElem->apChildren[ii + 1])
        ) {
            HtmlNode *pRemove = pElem->apChildren[ii + 1];
            nodeRemoveChild(pElem, pRemove);

            /* TODO: Fold text from pRemove into pElem->apChildren[ii] */

            HtmlTextFree(HtmlNodeAsText(pRemove));
            ii--;
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeHandlerCallbacks --
 *
 *     This is called for every tree node by HtmlWalkTree() immediately
 *     after the document tree is constructed. It calls the node handler
 *     script for the node, if one exists.
 *
 *     If the [$widget reset] method is called within the node-handler,
 *     non-zero is returned. Otherwise 0.
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
nodeHandlerCallbacks(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    Tcl_HashEntry *pEntry;
    Tcl_Interp *interp = pTree->interp;
    int eTag = HtmlNodeTagType(pNode);
    int isFragment = (pTree->pFragment?1:0);

    /* If this is called as a result of a [parse] (not [fragment])
     * command, this variable will be set to true if there is a node-handler
     * script and the script calls the [reset] method of this widget.
     */

    assert(isFragment || pTree->eWriteState == HTML_WRITE_NONE);
    assert(isFragment || (eTag != Html_TD && eTag != Html_TH) || (
           HtmlNodeParent(pNode) && 
           HtmlNodeTagType(HtmlNodeParent(pNode)) == Html_TR
       )
    );
    
    if (!HtmlNodeIsText(pNode)) {
        /* HtmlElementNormalize(HtmlNodeAsElement(pNode)); */
    }

    if (!isFragment && TAG_TO_TABLELEVEL(eTag) > 0) {
        treeCloseFosterTree(pTree);
    }

    /* Execute the node-handler script for node pNode, if one exists. */
    pEntry = Tcl_FindHashEntry(&pTree->aNodeHandler, (char *)((size_t) eTag));
    if (pEntry) {
        Tcl_Obj *pEval;
        Tcl_Obj *pScript;
        Tcl_Obj *pNodeCmd;
        int rc;

        pScript = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
        pEval = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pEval);

        if (!isFragment) {
            pTree->eWriteState = HTML_PARSE_NODEHANDLER;
        }

        pNodeCmd = HtmlNodeCommand(pTree, pNode);
        Tcl_ListObjAppendElement(0, pEval, pNodeCmd);
        rc = Tcl_EvalObjEx(interp, pEval, TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL);
        if (rc != TCL_OK) {
            Tcl_BackgroundError(interp);
        }
        Tcl_DecrRefCount(pEval);

        assert(
            isFragment || 
            pTree->eWriteState == HTML_PARSE_NODEHANDLER ||
            pTree->eWriteState == HTML_WRITE_INHANDLERRESET
        );
        if (!isFragment && pTree->eWriteState == HTML_PARSE_NODEHANDLER){
            pTree->eWriteState = HTML_WRITE_NONE;
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlFinishNodeHandlers --
 *
 *     Execute any outstanding node-handler callbacks. This is used when
 *     the end of a document is reached - the EOF implicitly closes all
 *     open nodes. This function executes node-handler scripts for nodes
 *     closed in such a fashion.
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
HtmlFinishNodeHandlers(pTree)
    HtmlTree *pTree;
{
    HtmlNode *p;
    for (p = pTree->state.pCurrent ; p; p = HtmlNodeParent(p)) {
        nodeHandlerCallbacks(pTree, p);
    }
    pTree->state.pCurrent = 0;
}   

int HtmlNodeIsOrphan(pNode)
    HtmlNode *pNode;
{
    while (pNode && pNode->iNode != HTML_NODE_ORPHAN) {
        pNode = HtmlNodeParent(pNode);
    }
    return (pNode ? 1 : 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeOrphanize --
 * nodeDeorphanize --
 *
 *     Mark a node as an orphan, or unmark it. All nodes marked as orphans
 *     are stored in the HtmlTree.aOrphan hash table and have HtmlNode.iNode
 *     set to HTML_NODE_ORPHAN.
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
nodeOrphanize(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    int eNew;
    assert(
        pNode->iNode != HTML_NODE_ORPHAN || 
        pNode == pTree->pFragment->pRoot
    );
    pNode->iNode = HTML_NODE_ORPHAN;
    pNode->pParent = 0;

    Tcl_CreateHashEntry(&pTree->aOrphan, (const char *)pNode, &eNew);
    assert(eNew);
}
static void
nodeDeorphanize(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    Tcl_HashEntry *pEntry;
    assert(pNode->iNode == HTML_NODE_ORPHAN);
    pNode->iNode = 0;
    pEntry = Tcl_FindHashEntry(&pTree->aOrphan, (const char *)pNode);
    assert(pEntry);
    Tcl_DeleteHashEntry(pEntry);
}

int
HtmlNodeIndexOfChild(pParent, pChild)
    HtmlNode *pParent;
    HtmlNode *pChild;
{
    int ii;
    for (ii = 0; ii < HtmlNodeNumChildren(pParent); ii++) {
        if (HtmlNodeChild(pParent, ii) == pChild) return ii;
    }
    return -1;
}

static void
nodeInsertChild(pTree, pElem, pBefore, pAfter, pChild)
    HtmlTree *pTree;
    HtmlElementNode *pElem;
    HtmlNode *pBefore;
    HtmlNode *pAfter;
    HtmlNode *pChild;
{
    int n;                  /* Number of bytes to alloc for pNode->apChildren */
    int ii;
    int iBefore;

    assert(pBefore == 0 || pAfter == 0);
    assert(pChild);

    if (pChild == pBefore || pChild == pAfter) {
        assert(HtmlNodeParent(pChild) == (HtmlNode *)pElem);
        return;
    }

    /* Unlink pChild from it's parent node. */
    if (HtmlNodeParent(pChild)) {
        HtmlNode *pParent = HtmlNodeParent(pChild);

        /* Before removing a child, invalidate the layout of the parent */
        HtmlCallbackLayout(pTree, pChild);

        /* Clear all the style and layout information cached in the
         * sub-tree rooted at the child node. At present anything
         * moved to the orphan tree has all style/layout info cleared.
         */
        HtmlNodeClearRecursive(pTree, pChild);
        nodeRemoveChild(HtmlNodeAsElement(pParent), pChild);
    }

    if (pBefore) {
        iBefore = HtmlNodeIndexOfChild((HtmlNode *)pElem, pBefore);
        assert(iBefore>=0);
    } else if (pAfter) {
        iBefore = HtmlNodeIndexOfChild((HtmlNode *)pElem, pAfter);
        assert(iBefore>=0);
        iBefore++;
    } else {
        iBefore = pElem->nChild;
    }

    /* Extend the size of the HtmlElementNode.apChildren[] array */
    assert(pElem);
    pElem->nChild++;
    n = (pElem->nChild) * sizeof(HtmlNode*);
    pElem->apChildren = (HtmlNode **)HtmlRealloc(
        "HtmlNode.apChildren", (char *)pElem->apChildren, n
    );

    for (ii = (pElem->nChild - 1); ii > iBefore; ii--) {
        pElem->apChildren[ii] = pElem->apChildren[ii - 1];
    }
    pElem->apChildren[iBefore] = pChild;

    /* Link pChild into the new parent node */
    pChild->pParent = (HtmlNode *)pElem;
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeAddChild --
 *
 *     Add a new child node to node pNode. pToken becomes the starting
 *     token for the new node. The value returned is the index of the new
 *     child. So the call:
 *
 *          HtmlNodeChild(pNode, HtmlNodeAddChild(pNode, pToken))
 *
 *     returns the new child node.
 *
 * Results:
 *     Index of the child added to pNode.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlNodeAddChild(pElem, eTag, zTag, pAttributes)
    HtmlElementNode *pElem;
    int eTag;
    const char *zTag;               /* Atom for tag name */
    HtmlAttributes *pAttributes;
{
    int n;                  /* Number of bytes to alloc for pNode->apChildren */
    int r;                  /* Return value */
    HtmlElementNode *pNew;  /* New child node */

    assert(pElem);
    
    r = pElem->nChild++;
    n = (r+1) * sizeof(HtmlNode*);
    pElem->apChildren = (HtmlNode **)HtmlRealloc(
        "HtmlNode.apChildren", (char *)pElem->apChildren, n
    );

    if (!zTag) {
        zTag = HtmlTypeToName(0, eTag);
    }
    assert(zTag);

    pNew = HtmlNew(HtmlElementNode);
    pNew->pAttributes = pAttributes;
    pNew->node.pParent = (HtmlNode *)pElem;
    pNew->node.eTag = eTag;
    pNew->node.zTag = zTag;
    pElem->apChildren[r] = (HtmlNode *)pNew;

    assert(r < pElem->nChild);
    return r;
}

int 
HtmlNodeAddTextChild(pNode, pTextNode)
    HtmlNode *pNode;
    HtmlTextNode *pTextNode;
{
    int n;             /* Number of bytes to alloc for pNode->apChildren */
    int r;             /* Return value */
    HtmlNode *pNew;    /* New child node */

    HtmlElementNode *pElem = HtmlNodeAsElement(pNode);

    assert(pElem);
    assert(pTextNode);
    
    r = pElem->nChild++;
    n = (r+1) * sizeof(HtmlNode*);
    pElem->apChildren = (HtmlNode **)HtmlRealloc(
        "HtmlNode.apChildren", (char *)pElem->apChildren, n
    );

    pNew = (HtmlNode *)pTextNode;
    memset(pNew, 0, sizeof(HtmlNode));
    pNew->pParent = pNode;
    pNew->eTag = Html_Text;
    pElem->apChildren[r] = pNew;

    assert(r < pElem->nChild);
    return r;
}

/*
 *---------------------------------------------------------------------------
 *
 * setNodeAttribute --
 *
 *     Set the value of an attribute on a node. This function is currently
 *     a bit inefficient, due to the way the HtmlToken structure is 
 *     allocated.
 *
 * Results:
 *     None
 *
 * Side effects:
 *     Modifies the HtmlToken structure associated with the specified node.
 *
 *---------------------------------------------------------------------------
 */
static void
setNodeAttribute(pNode, zAttrName, zAttrVal)
    HtmlNode *pNode;
    const char *zAttrName;
    const char *zAttrVal;
{
    #define MAX_NUM_ATTRIBUTES 100
    char const *azPtr[MAX_NUM_ATTRIBUTES * 2];
    int aLen[MAX_NUM_ATTRIBUTES * 2];

    int i;
    int isDone = 0;
    int nArgs;
    HtmlElementNode *pElem;
    HtmlAttributes *pAttr;

    pElem = HtmlNodeAsElement(pNode);
    if (!pElem) return;
    pAttr = pElem->pAttributes;

    for (i = 0; pAttr && i < pAttr->nAttr && i < MAX_NUM_ATTRIBUTES; i++) {
        azPtr[i*2] = pAttr->a[i].zName;
        if (0 != strcmp(pAttr->a[i].zName, zAttrName)) {
            azPtr[i*2+1] = pAttr->a[i].zValue;
        } else {
            azPtr[i*2+1] = zAttrVal;
            isDone = 1;
        }
    }

    if (!isDone && i < MAX_NUM_ATTRIBUTES) {
        azPtr[i*2] = zAttrName;
        azPtr[i*2+1] = zAttrVal;
        i++;
    }

    nArgs = i * 2;
    for (i = 0; i < nArgs; i++) {
        aLen[i] = strlen(azPtr[i]);
    }

    pElem->pAttributes = HtmlAttributesNew(nArgs, azPtr, aLen, 0);
    HtmlFree(pAttr);

    /* If this was a call to set the "style" attribute, discard the
     * compiled version at version HtmlElementNode.pStyle.
     */
    if (strcmp(HTML_INLINE_STYLE_ATTR, zAttrName) == 0) {
        HtmlCssInlineFree(pElem->pStyle);
        pElem->pStyle = 0;
    }
}

static void
mergeAttributes(pNode, pAttr)
    HtmlNode *pNode;
    HtmlAttributes *pAttr;
{
    int ii;
    for (ii = 0; pAttr && ii < pAttr->nAttr; ii++) {
        setNodeAttribute(pNode, pAttr->a[ii].zName, pAttr->a[ii].zValue);
    }
    HtmlFree(pAttr);
}

static int
doAttributeHandler(pTree, pNode, zAttr, zValue) 
    HtmlTree *pTree;
    HtmlNode *pNode;
    const char *zAttr;
    const char *zValue;
{
    int rc = TCL_OK;
    int eType = pNode->eTag;
    Tcl_HashEntry *pEntry;

    pEntry = Tcl_FindHashEntry(&pTree->aAttributeHandler, (char*)((size_t) eType));
    if (pEntry) {
        Tcl_Obj *pScript;
        pScript = (Tcl_Obj *)Tcl_GetHashValue(pEntry);

        pScript = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pScript);
        Tcl_ListObjAppendElement(0, pScript, HtmlNodeCommand(pTree, pNode));
        Tcl_ListObjAppendElement(0, pScript, Tcl_NewStringObj(zAttr, -1));
        Tcl_ListObjAppendElement(0, pScript, Tcl_NewStringObj(zValue, -1));
        rc = Tcl_EvalObjEx(pTree->interp, pScript, TCL_EVAL_GLOBAL);
        Tcl_DecrRefCount(pScript);
    }

    return rc;
}

static int
doParseHandler(pTree, eType, pNode, iOffset)
    HtmlTree *pTree;
    int eType;
    HtmlNode *pNode;
    int iOffset;
{
    int rc = TCL_OK;
    Tcl_HashEntry *pEntry;
    if (iOffset < 0) return TCL_OK;

    if (eType == Html_Space) {
        eType = Html_Text;
    }

    pEntry = Tcl_FindHashEntry(&pTree->aParseHandler, (char *)((size_t) eType));
    if (pEntry) {
        Tcl_Obj *pScript;
        pScript = (Tcl_Obj *)Tcl_GetHashValue(pEntry);

        pScript = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pScript);
        if (pNode) {
            Tcl_ListObjAppendElement(0, pScript, HtmlNodeCommand(pTree, pNode));
        } else {
            Tcl_ListObjAppendElement(0, pScript, Tcl_NewStringObj("", -1));
        }
        Tcl_ListObjAppendElement(
            0, pScript, Tcl_NewIntObj(iOffset + pTree->nParsed)
        );

        rc = Tcl_EvalObjEx(pTree->interp, pScript, TCL_EVAL_GLOBAL);
        Tcl_DecrRefCount(pScript);
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlInitTree --
 *
 *     Create the parts of the tree that are always present. i.e.:
 *
 *       <html>
 *         <head>
 *         <body>
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May modify the tree structure at HtmlTree.pRoot and
 *     HtmlTree.pCurrent.
 *
 *---------------------------------------------------------------------------
 */
void
HtmlInitTree(pTree)
    HtmlTree *pTree;
{
    if (!pTree->pRoot) {
        /* If pTree->pRoot is NULL, then the first token of the document
         * has just been parsed. If the document is well-formed, it should
         * be an <html> tag (Html documents may have a DOCTYPE and other such
         * garbage in them, but the tokenizer should ignore all that).
         *
         * But in these uncertain times you really can't trust anyone, so
         * Tkhtml automatically inserts the following structure at the root
         * of every document:
         *
         *    <html>
         *      <head>
         *      </head>
         *      <body>
         */
        HtmlElementNode *pRoot;

        pRoot = HtmlNew(HtmlElementNode);
        pRoot->node.eTag = Html_HTML;
        pRoot->node.zTag = HtmlTypeToName(pTree, Html_HTML);
        pTree->pRoot = (HtmlNode *)pRoot;


        HtmlNodeAddChild(pRoot, Html_HEAD, HtmlTypeToName(pTree, Html_HEAD), 0);
        HtmlNodeAddChild(pRoot, Html_BODY, HtmlTypeToName(pTree, Html_BODY), 0);
        HtmlCallbackRestyle(pTree, (HtmlNode *)pRoot);
    }

    if (!pTree->state.pCurrent) {
        /* If there is no current node, then the <body> node of the 
         * document is the current node. 
         */
        pTree->state.pCurrent = HtmlNodeChild(pTree->pRoot, 1);
        assert(HtmlNodeTagType(pTree->state.pCurrent) == Html_BODY);
    }
}

static HtmlNode *
findFosterParent(pTree, ppTable)
    HtmlTree *pTree;
    HtmlNode **ppTable;
{
    HtmlNode *pFosterParent;
    HtmlNode *pTable;

    /* Find the parent of the <TABLE> element (the foster-parent) */
    for (
        pTable = pTree->state.pCurrent;
        HtmlNodeTagType(pTable) != Html_TABLE;
        pTable = HtmlNodeParent(pTable)
    );
    pFosterParent = HtmlNodeParent(pTable);
    assert(pFosterParent);
    if (ppTable) {
        *ppTable = pTable;
    }

    return pFosterParent;
}

static void
treeCloseFosterTree(pTree)
    HtmlTree *pTree;
{
    if (pTree->state.pFoster) {
        HtmlNode *pFosterRoot = findFosterParent(pTree, 0);
        HtmlNode *pFoster;

        pFoster = pTree->state.pFoster;
        for ( ;pFoster != pFosterRoot; pFoster = HtmlNodeParent(pFoster)) {
            nodeHandlerCallbacks(pTree, pFoster);
        }

        pTree->state.pFoster = 0;
    }
}

static void
treeAddFosterText(pTree, pTextNode)
    HtmlTree *pTree;
    HtmlTextNode *pTextNode;
{
    if (pTree->state.pFoster) {
        HtmlNodeAddTextChild(pTree->state.pFoster, pTextNode);
    } else {
        HtmlNode *pFosterParent;
        HtmlNode *pBefore = 0;
        pFosterParent = findFosterParent(pTree, &pBefore);
           
        nodeInsertChild(pTree,
            (HtmlElementNode *)pFosterParent, pBefore, 0, (HtmlNode *)pTextNode
        );
    }
}

HtmlNode * 
treeAddFosterElement(pTree, eTag, zTag, pAttr)
    HtmlTree *pTree;
    int eTag;
    const char *zTag;                /* Atom for tag */
    HtmlAttributes *pAttr;
{
    HtmlNode *pFosterParent;

    HtmlNode *pNew;
    HtmlNode *pFoster = pTree->state.pFoster;
    HtmlNode *pBefore = 0;

    /* Find the parent of the <TABLE> element (the foster-parent) */
    pFosterParent = findFosterParent(pTree, &pBefore);

    if (pFoster) {
        int nClose;
        int ii;
        implicitCloseCount(pTree, pTree->state.pFoster, eTag, &nClose);
        for (
            ii = 0; 
            ii < nClose && pFoster != pFosterParent; 
            pFoster = HtmlNodeParent(pFoster)
        ) {
            nodeHandlerCallbacks(pTree, pFoster);
        }
        if (pFoster == pFosterParent) pFoster = 0;
    }

    if (pFoster) {
        int n = HtmlNodeAddChild((HtmlElementNode *)pFoster, eTag, zTag, pAttr);
        pNew = HtmlNodeChild(pFoster, n);
    } else {
        pNew = (HtmlNode *)HtmlNew(HtmlElementNode);
        ((HtmlElementNode *)pNew)->pAttributes = pAttr;
        pNew->eTag = eTag;
        if (!zTag) {
            zTag = HtmlTypeToName(0, eTag);
        }
        pNew->zTag = zTag;
        nodeInsertChild(pTree, (HtmlElementNode *)pFosterParent,pBefore,0,pNew);
    }

    pNew->iNode = pTree->iNextNode++;
    if (HtmlMarkupFlags(eTag) & HTMLTAG_EMPTY) {
        nodeHandlerCallbacks(pTree, pNew);
        pTree->state.pFoster = HtmlNodeParent(pNew);
        if (pTree->state.pFoster == pFosterParent) pTree->state.pFoster = 0;
    } else {
        pTree->state.pFoster = pNew;
    }

    HtmlCallbackRestyle(pTree, pNew);
    return pNew;
}

static void
treeAddFosterClosingTag(pTree, eTag, zTag)
    HtmlTree *pTree;
    int eTag;
    const char *zTag;
{
    HtmlNode *pFosterParent;
    HtmlNode *pFoster;
    int ii;
    int nClose;

    /* Find the parent of the <TABLE> element (the foster-parent) */
    pFosterParent = findFosterParent(pTree, 0);
    assert(pFosterParent);

    explicitCloseCount(pTree->state.pFoster, eTag, zTag, &nClose);
    pFoster = pTree->state.pFoster;
    for (ii = 0; ii < nClose && pFoster != pFosterParent; ii++) {
        nodeHandlerCallbacks(pTree, pFoster);
        pFoster = HtmlNodeParent(pFoster);
    }
    if (pFoster == pFosterParent) {
        pFoster = 0;
    }
    pTree->state.pFoster = pFoster;
}

static HtmlNode *
treeAddTableComponent(pTree, eTag, pAttr)
    HtmlTree *pTree;
    int eTag;
    HtmlAttributes *pAttr;
{
    HtmlNode *pCurrent = pTree->state.pCurrent;
    HtmlNode *pParent;

    int eParentTag;

    /* The newly created document node */
    HtmlNode *pNew;
    int n;

    for (pParent = pCurrent; pParent; pParent = HtmlNodeParent(pParent)) {
        int eCTag = HtmlNodeTagType(pParent);

        if (
            (eCTag == Html_TABLE) || (
                (eCTag==Html_TBODY || eCTag==Html_THEAD || eCTag==Html_TFOOT) &&
                (eTag==Html_TH || eTag==Html_TD || eTag==Html_TR)
            ) ||
            (eCTag == Html_TR && (eTag == Html_TD || eTag == Html_TH))
        ) break;
    }
    if (!pParent) {
        HtmlFree(pAttr);
        return pParent;
    }
    eParentTag = HtmlNodeTagType(pParent);

    /* Invoke node-handler callbacks for implicitly closed nodes */
    for ( ; pCurrent != pParent ; pCurrent = HtmlNodeParent(pCurrent)) {
        nodeHandlerCallbacks(pTree, pCurrent);
    }
    treeCloseFosterTree(pTree);

    assert( 
        eParentTag == Html_TABLE || eParentTag == Html_TBODY || 
        eParentTag == Html_THEAD || eParentTag == Html_TFOOT ||
        eParentTag == Html_TR
    );

    /* See if we need to add an implicit <TBODY> node */
    if (
        eParentTag == Html_TABLE && 
        (eTag == Html_TR || eTag == Html_TD || eTag == Html_TH)
    ) {
        int n2 = HtmlNodeAddChild((HtmlElementNode *)pParent, Html_TBODY, 0, 0);
        pParent = HtmlNodeChild(pParent, n2);
        pParent->iNode = pTree->iNextNode++;
        eParentTag = Html_TBODY;
    }

    /* See if we need to add an implicit <TR> node */
    if (eParentTag != Html_TR && (eTag == Html_TD || eTag == Html_TH)) {
        int n2 = HtmlNodeAddChild((HtmlElementNode *)pParent, Html_TR, 0, 0);
        pParent = HtmlNodeChild(pParent, n2);
        pParent->iNode = pTree->iNextNode++;
        eParentTag = Html_TR;
    }
    
    /* Add the new node to pParent */
    n = HtmlNodeAddChild((HtmlElementNode *)pParent, eTag, 0, pAttr);
    pNew = HtmlNodeChild(pParent, n);
    pNew->iNode = pTree->iNextNode++;
    pTree->state.pCurrent = pNew;

    /* Return a pointer to the node just added */
    return pNew;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeAddElement --
 *
 *     Update the tree structure with an element of type eType, attributes
 *     as specified in *pAttr.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May modify the tree structure at HtmlTree.pRoot and
 *     HtmlTree.pCurrent.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlTreeAddElement(pTree, eType, zType, pAttr, iOffset)
    HtmlTree *pTree;
    int eType;
    const char *zType;
    HtmlAttributes *pAttr;
    int iOffset;
{
    HtmlNode *pCurrent;
    HtmlNode *pHeadNode;
    HtmlNode *pBodyNode;
    HtmlElementNode *pHeadElem;

    /* If token pToken causes a node to be added to the tree, or the
     * attributes of an <html>, <body> or <head> element to be updated,
     * this variable is set to point to the node. At the end of this
     * function, it is used as an argument to any registered 
     * [$widget handler parse] callback script.
     */
    HtmlNode *pParsed = 0; 

    /* Initialise the tree and find the <HEAD> and <BODY> elements */
    HtmlInitTree(pTree);
    pHeadNode = HtmlNodeChild(pTree->pRoot, 0);
    pBodyNode = HtmlNodeChild(pTree->pRoot, 1);
    pHeadElem = HtmlNodeAsElement(pHeadNode);

    pCurrent = pTree->state.pCurrent;

    assert(pCurrent);
    assert(pHeadNode);
    assert(eType != Html_Text && eType != Html_Space);

    /* If the HtmlTreeState.isCdataInHead flag is true, then the previous
     * token parsed was <TITLE> (generally: a token that generates an
     * element in the <HEAD> section with #CDATA content). Close the 
     * element before proceeding.
     */
    if (pTree->state.isCdataInHead) {
        int nChild = HtmlNodeNumChildren(pHeadNode) - 1;
        HtmlNode *pTitle = HtmlNodeChild(pHeadNode, nChild);
        nodeHandlerCallbacks(pTree, pTitle);
    }
    pTree->state.isCdataInHead = 0;

    switch (eType) {
        case Html_HTML:
            pParsed = pTree->pRoot;
            mergeAttributes(pParsed, pAttr);
            HtmlCallbackRestyle(pTree, pParsed);
            break;
        case Html_HEAD:
            pParsed = pHeadNode;
            mergeAttributes(pParsed, pAttr);
            HtmlCallbackRestyle(pTree, pParsed);
            break;
        case Html_BODY:
            pParsed = pBodyNode;
            mergeAttributes(pParsed, pAttr);
            HtmlCallbackRestyle(pTree, pParsed);
            break;

        /* Elements with content #CDATA for the document head. 
         *
         * Todo: Technically, we should be worried about <script> and
         * <style> elements in the document head too, but in practice it
         * makes little difference where these wind up. <script> is
         * a bit tricky as this can appear in either the <head> or <body>
         * section.
         */
        case Html_TITLE: {
            int n = HtmlNodeAddChild(pHeadElem, eType, 0, pAttr);
            HtmlNode *p = HtmlNodeChild(pHeadNode, n);
            pTree->state.isCdataInHead = 1;
            p->iNode = pTree->iNextNode++;
            pParsed = p;
            HtmlCallbackRestyle(pTree, pParsed);
            break;
        }

        /* Self-closing elements to add to the document head */
        case Html_META:
        case Html_LINK:
        case Html_BASE: {
            int n = HtmlNodeAddChild(pHeadElem, eType, 0, pAttr);
            HtmlNode *p = HtmlNodeChild(pHeadNode, n);
            p->iNode = pTree->iNextNode++;
            nodeHandlerCallbacks(pTree, p);
            if (pTree->eWriteState != HTML_WRITE_INHANDLERRESET) {
                pParsed = p;
                HtmlCallbackRestyle(pTree, pParsed);
            }
            break;
        }

        /* Elements that form part of <TABLE> structures. Special 
         * rules apply to these so they are handled seperately.
         */
        case Html_TBODY:
        case Html_TFOOT:
        case Html_THEAD:
        case Html_TR:
        case Html_TD:
        case Html_TH: {
            pParsed = treeAddTableComponent(pTree, eType, pAttr);
            break;
        }

        default: {
            int eCurrentType = HtmlNodeTagType(pCurrent);
            int isTableType = ((
                eCurrentType == Html_TABLE || eCurrentType == Html_TBODY || 
                eCurrentType == Html_TFOOT || eCurrentType == Html_THEAD || 
                eCurrentType == Html_TR
            ) ? 1 : 0);
            if (isTableType && eType != Html_FORM) {
                /* Need to add this node to the foster tree. */
                pParsed = treeAddFosterElement(pTree, eType, zType, pAttr);
            } else {
                /* Add this node to pCurrent. */
                int nClose = 0;
                int i;
                HtmlElementNode *pC;
                int N;

                implicitCloseCount(pTree, pCurrent, eType, &nClose);
                for (i = 0; i < nClose && pCurrent != pBodyNode; i++) {
                    nodeHandlerCallbacks(pTree, pCurrent);
                    pCurrent = HtmlNodeParent(pCurrent);
                }
                pTree->state.pCurrent = pCurrent;

                pC = HtmlNodeAsElement(pCurrent);
                assert(!HtmlNodeIsText(pTree->state.pCurrent));
                N = HtmlNodeAddChild(pC, eType, zType, pAttr);
                pCurrent = HtmlNodeChild(pCurrent, N);
                pCurrent->iNode = pTree->iNextNode++;
                pParsed = pCurrent;

                assert(!isTableType || eType == Html_FORM);
                if ((HtmlMarkupFlags(eType) & HTMLTAG_EMPTY) || isTableType) {
                    nodeHandlerCallbacks(pTree, pCurrent);
                    pCurrent = HtmlNodeParent(pCurrent);
                }
                pTree->state.pCurrent = pCurrent;
            }
        }
    }

    if (pParsed) {
        if (HtmlNodeComputedValues(pParsed)) {
            HtmlCallbackRestyle(pTree, pParsed);
        }
        doParseHandler(pTree, eType, pParsed, iOffset);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeAddText --
 *
 *     Add the text-node pTextNode to the tree.
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
HtmlTreeAddText(pTree, pTextNode, iOffset)
    HtmlTree *pTree;
    HtmlTextNode *pTextNode;
    int iOffset;
{
    HtmlNode *pCurrent;
    int eCurrentType;

    HtmlInitTree(pTree);
    pCurrent = pTree->state.pCurrent;
    eCurrentType = HtmlNodeTagType(pCurrent);

    if (pTree->state.isCdataInHead) {
        HtmlNode *pHeadNode = HtmlNodeChild(pTree->pRoot, 0);
        int nChild = HtmlNodeNumChildren(pHeadNode) - 1;
        HtmlNode *pTitle = HtmlNodeChild(pHeadNode, nChild);

        HtmlNodeAddTextChild(pTitle, pTextNode);
        pTextNode->node.iNode = pTree->iNextNode++;
        pTree->state.isCdataInHead = 0;
        nodeHandlerCallbacks(pTree, pTitle);
    } else if (
        eCurrentType == Html_TABLE || eCurrentType == Html_TBODY || 
        eCurrentType == Html_TFOOT || eCurrentType == Html_THEAD || 
        eCurrentType == Html_TR
    ) {
        treeAddFosterText(pTree, pTextNode);
        pTextNode->node.iNode = pTree->iNextNode++;
        pTextNode->node.eTag = Html_Text;
    } else {
        HtmlNodeAddTextChild(pCurrent, pTextNode);
        pTextNode->node.iNode = pTree->iNextNode++;
    }

    assert(pTextNode->node.eTag == Html_Text);
    doParseHandler(pTree, Html_Text, (HtmlNode *)pTextNode, iOffset);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeAddClosingTag --
 *
 *     Process the closing tag eTag. 
 *
 *     This method is prefixed "HtmlTreeAdd" to match HtmlTreeAddText() 
 *     and HtmlTreeAddElement(), the other two functions used by the 
 *     document parser to build the tree.
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
HtmlTreeAddClosingTag(pTree, eTag, zTag, iOffset)
    HtmlTree *pTree;
    int eTag;
    const char *zTag;
    int iOffset;
{
    int nClose;
    int ii;

    HtmlInitTree(pTree);

    if (pTree->state.pFoster && 0 == TAG_TO_TABLELEVEL(eTag)) {
        assert(TAG_TO_TABLELEVEL(HtmlNodeTagType(pTree->state.pCurrent)) > 0);
        treeAddFosterClosingTag(pTree, eTag, zTag);
    } else {
        HtmlNode *pBody = HtmlNodeChild(pTree->pRoot, 1);
        explicitCloseCount(pTree->state.pCurrent, eTag, zTag, &nClose);
        for (ii = 0; ii < nClose && pTree->state.pCurrent != pBody; ii++) {
            nodeHandlerCallbacks(pTree, pTree->state.pCurrent);
            pTree->state.pCurrent = HtmlNodeParent(pTree->state.pCurrent);
        }
    }

    doParseHandler(pTree, -1 * eTag, 0, iOffset);
}

/*
 *---------------------------------------------------------------------------
 *
 * walkTree --
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
walkTree(pTree, xCallback, pNode, clientData)
    HtmlTree *pTree;
    int (*xCallback)(HtmlTree *, HtmlNode *, ClientData clientData);
    HtmlNode *pNode;
    ClientData clientData;
{
    int i;
    if( pNode ){
        int rc = xCallback(pTree, pNode, clientData);
        switch (rc) {
            case HTML_WALK_ABANDON:
                return 1;
            case HTML_WALK_DESCEND:
                break;
            case HTML_WALK_DO_NOT_DESCEND:
                return 0;
            default:
                    assert(!"Bad return value from HtmlWalkTree() callback");
        }

        for (i = 0; i < HtmlNodeNumChildren(pNode); i++) {
            HtmlNode *pChild = HtmlNodeChild(pNode, i);
            int rc = walkTree(pTree, xCallback, pChild, clientData);
            assert(HtmlNodeParent(pChild) == pNode);
            if (rc) return rc;
        }
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlWalkTree --
 *
 *     Traverse the subset of document tree pTree rooted at pNode. If pNode is
 *     NULL the entire tree is traversed. This function does a pre-order or
 *     prefix traversal (each node is visited before it's children).
 *
 *     When a node is visited the supplied callback function is invoked. The
 *     callback function must return one of the following three hash
 *     defined values:
 *
 *         HTML_WALK_DESCEND
 *         HTML_WALK_DO_NOT_DESCEND
 *         HTML_WALK_ABANDON
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
HtmlWalkTree(pTree, pNode, xCallback, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    int (*xCallback)(HtmlTree *, HtmlNode *, ClientData clientData);
    ClientData clientData;
{
    return walkTree(pTree, xCallback, pNode?pNode:pTree->pRoot, clientData);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeNumChildren --
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
int HtmlNodeNumChildren(pNode)
    HtmlNode *pNode;
{
    if (HtmlNodeIsText(pNode)) return 0;
    return ((HtmlElementNode *)(pNode))->nChild;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeChild --
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
HtmlNode * 
HtmlNodeChild(pNode, n)
    HtmlNode *pNode;
    int n;
{
    HtmlElementNode *pElem = (HtmlElementNode *)pNode;
    if (!pNode || HtmlNodeIsText(pNode) || pElem->nChild <= n) return 0;
    return pElem->apChildren[n];
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeBefore --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode * 
HtmlNodeBefore(pNode)
    HtmlNode *pNode;
{
    if (!HtmlNodeIsText(pNode)) {
        return ((HtmlElementNode *)pNode)->pBefore;
    }
    return 0;
}

#if 0
HtmlComputedValues * 
HtmlNodeComputedValues(pNode)
    HtmlNode *pNode;
{
    if (HtmlNodeIsText(pNode)) {
        pNode = HtmlNodeParent(pNode);
    }
    if (pNode) {
        return ((HtmlElementNode *)pNode)->pPropertyValues;
    }
    return 0;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeAfter --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode * 
HtmlNodeAfter(pNode)
    HtmlNode *pNode;
{
    if (!HtmlNodeIsText(pNode)) {
        return ((HtmlElementNode *)pNode)->pAfter;
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeTagType --
 *
 *     Return the tag-type of the node, i.e. Html_P, Html_Text or
 *     Html_Space.
 *
 * Results:
 *     Integer tag type.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Html_u8 HtmlNodeTagType(pNode)
    HtmlNode *pNode;
{
    assert(pNode);
    return pNode->eTag;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeTagName --
 *
 *     Return the name of the tag-type of the node, i.e. "p", "text" or
 *     "div".
 *
 * Results:
 *     Boolean.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
CONST char * HtmlNodeTagName(pNode)
    HtmlNode *pNode;
{
    assert(pNode->zTag || HtmlNodeIsText(pNode));
    if (!pNode->zTag) return "";
    return pNode->zTag;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeRightSibling --
 * 
 *     Get the right-hand sibling to a node, if it has one.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode *HtmlNodeRightSibling(pNode)
    HtmlNode *pNode;
{
    HtmlElementNode *pParent = (HtmlElementNode *)pNode->pParent;
    if( pParent ){
        int i;
        for (i=0; i < pParent->nChild - 1; i++) {
            if (pNode == pParent->apChildren[i]) {
                return pParent->apChildren[i+1];
            }
        }
        assert(pNode == pParent->apChildren[pParent->nChild - 1]);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeLeftSibling --
 * 
 *     Get the left-hand sibling to a node, if it has one.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode *HtmlNodeLeftSibling(pNode)
    HtmlNode *pNode;
{
    HtmlElementNode *pParent = (HtmlElementNode *)pNode->pParent;
    if( pParent ){
        int i;
        for (i = 1; i < pParent->nChild; i++) {
            if (pNode == pParent->apChildren[i]) {
                return pParent->apChildren[i-1];
            }
        }
        assert(pNode == pParent->apChildren[0]);
    }
    return 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeAttr --
 *
 *     Return a pointer to the value of node attribute zAttr. Attributes
 *     are always represented as NULL-terminated strings.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
char CONST *HtmlNodeAttr(pNode, zAttr)
    HtmlNode *pNode; 
    char CONST *zAttr;
{
    HtmlElementNode *pElem = HtmlNodeAsElement(pNode);
    if (pElem) {
        return HtmlMarkupArg(pElem->pAttributes, zAttr, 0);
    }
    return 0;
}

static int 
markWindowAsClipped(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    if (!HtmlNodeIsText(pNode)) {
        HtmlNodeReplacement *p = ((HtmlElementNode *)pNode)->pReplacement;
        if (p) {
            p->clipped = 1;
        }
    }

    return HTML_WALK_DESCEND;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeViewCmd --
 *
 *     This function implements the Tcl commands:
 *
 *         [nodeHandle yview] 
 *         [nodeHandle xview]
 *
 *     used to scroll boxes generated by tree elements with "overflow:auto"
 *     or "overflow:scroll". At present, the implementation of this is
 *     not very efficient.
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
nodeViewCmd(pNode, isVertical, objv, objc)
    HtmlNode *pNode;
    int isVertical;
    Tcl_Obj *CONST objv[];
    int objc;
{
    HtmlTree *pTree;
    int eType;       /* One of the TK_SCROLL_ symbols */
    double fraction;
    int count;

    int iNew;
    int iMax;
    int iSize;
    int iIncr;

    int x, y, w, h;

    HtmlElementNode *pElem = (HtmlElementNode *)pNode;

    if (HtmlNodeIsText(pNode) || !pElem->pScrollbar) {
        return TCL_ERROR;
    }

    pTree = pNode->pNodeCmd->pTree;
    if (isVertical) {
        iNew = pElem->pScrollbar->iVertical;
        iMax = pElem->pScrollbar->iVerticalMax;
        iSize = pElem->pScrollbar->iHeight;
        iIncr = pTree->options.yscrollincrement;
    } else {
        iNew = pElem->pScrollbar->iHorizontal;
        iMax = pElem->pScrollbar->iHorizontalMax;
        iSize = pElem->pScrollbar->iWidth;
        iIncr = pTree->options.xscrollincrement;
    }

    eType = Tk_GetScrollInfoObj(pTree->interp, objc, objv, &fraction, &count);

    switch (eType) {
        case TK_SCROLL_MOVETO:
            iNew = (int)((double)iMax * fraction);
            break;
        case TK_SCROLL_PAGES: /* TODO */
            iNew += count * (0.9 * iSize);
            break;
        case TK_SCROLL_UNITS: /* TODO */
            iNew += count * iIncr;
            break;
        case TK_SCROLL_ERROR:
            return TCL_ERROR;

        default: assert(!"Not possible");
    }

    iNew = MAX(0, iNew);
    iNew = MIN(iNew, iMax - iSize);
    if (isVertical) {
        pElem->pScrollbar->iVertical = iNew;
    } else {
        pElem->pScrollbar->iHorizontal = iNew;
    }

    /* Invoke the scrollbar callbacks (i.e. [$scrollbar set]) to update
     * the scrollbar widgets with their new positions.
     */
    HtmlNodeScrollbarDoCallback(pNode->pNodeCmd->pTree, pNode);

    HtmlWidgetOverflowBox(pTree, pNode, &x, &y, &w, &h);
    HtmlCallbackDamage(pTree, x - pTree->iScrollX, y - pTree->iScrollY, w, h);
    if (pTree->cb.flags) {
        pTree->cb.flags |= HTML_NODESCROLL;
    }
    HtmlWalkTree(pTree, pNode, markWindowAsClipped, 0);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeRemoveCmd --
 *
 *         $node remove NODE-LIST...
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
nodeRemoveCmd(pNode, objc, objv)
    HtmlNode *pNode;
    int objc;
    Tcl_Obj *CONST objv[];
{
    HtmlTree *pTree = pNode->pNodeCmd->pTree;
    int ii;

    if (objc < 3) {
        Tcl_WrongNumArgs(pTree->interp, 2, objv, "NODE-LIST");
        return TCL_ERROR;
    }

    for (ii = 2; ii < objc; ii++) {
        Tcl_Obj **apNode;
        int nNode;
        int jj;
        int rc;

        rc = Tcl_ListObjGetElements(pTree->interp, objv[ii], &nNode, &apNode);
        if (rc != TCL_OK) {
            return rc;
        }

        for (jj = 0; jj < nNode; jj++) {
            int e;
            Tcl_Obj *pObj = apNode[jj];
            HtmlNode *pChild = HtmlNodeGetPointer(pTree, Tcl_GetString(pObj));
            e = nodeRemoveChild((HtmlElementNode *)pNode, pChild);
            if (e) {
                nodeOrphanize(pTree, pChild);
                HtmlNodeClearRecursive(pTree, pChild);
            }
        }
    }

    HtmlCheckRestylePoint(pTree);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeDestroyCmd --
 *
 *         $node destroy
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
nodeDestroyCmd(pNode, objc, objv)
    HtmlNode *pNode;
    int objc;
    Tcl_Obj *CONST objv[];
{
    HtmlTree *pTree = pNode->pNodeCmd->pTree;

    if (objc != 2) {
        Tcl_WrongNumArgs(pTree->interp, 2, objv, "");
        return TCL_ERROR;
    }

    assert(
        pNode->iNode == HTML_NODE_ORPHAN || 
        pNode == pTree->pRoot || 
        pNode->pParent
    );

    if (pNode->iNode == HTML_NODE_ORPHAN) {
        nodeDeorphanize(pTree, pNode);
    } else if (pNode->pParent) {
        HtmlCallbackRestyle(pTree, pNode->pParent);
        HtmlCallbackLayout(pTree, pNode->pParent);
        nodeRemoveChild(HtmlNodeAsElement(pNode->pParent), pNode);
    } else {
        assert(!"TODO: Delete the root node?");
    }
    
    freeNode(pTree, pNode);

    HtmlCheckRestylePoint(pTree);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeInsertCmd --
 *
 *         $node insert ?-before|-after NODE? NODE-LIST...
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
nodeInsertCmd(pNode, objc, objv)
    HtmlNode *pNode;
    int objc;
    Tcl_Obj *CONST objv[];
{
    HtmlTree *pTree = pNode->pNodeCmd->pTree;
    Tcl_Interp *interp = pTree->interp;
    int ii;
    HtmlNode *pBefore = 0;
    HtmlNode *pAfter = 0;

    /* Process the "-before" or "-after" option, if one is specified. 
     */
    if (objc > 3 && (
            0 == strcmp(Tcl_GetString(objv[2]), "-before") ||
            0 == strcmp(Tcl_GetString(objv[2]), "-after")
    )) {
        int iBefore;
        pBefore = HtmlNodeGetPointer(pTree, Tcl_GetString(objv[3]));
        iBefore = HtmlNodeIndexOfChild(pNode, pBefore);
        if (iBefore < 0) {
            Tcl_ResetResult(pTree->interp);
            Tcl_AppendResult(pTree->interp, Tcl_GetString(objv[3]), 
                " is not a child node of ", Tcl_GetString(objv[0]), 0
            );
            return TCL_ERROR;
        }
        if (0 == strcmp(Tcl_GetString(objv[2]), "-after")) {
            pAfter = pBefore;
            pBefore = 0;
        }
    }

    /* Complain if there are insufficient arguments to this command */
    if (objc < 3 || (pBefore && objc < 5)) {
        Tcl_WrongNumArgs(interp, 2, objv, "?-before|-after NODE? NODE-LIST");
        return TCL_ERROR;
    }

    for (ii = (pBefore ? 4 : 2); ii < objc; ii++) {
        Tcl_Obj **apNode;
        int nNode;
        int jj;
        int rc;

        rc = Tcl_ListObjGetElements(interp, objv[ii], &nNode, &apNode);
        if (rc != TCL_OK) {
            return rc;
        }

        for (jj = 0; jj < nNode; jj++) {
            Tcl_Obj *pObj = apNode[jj];
            HtmlNode *pChild = HtmlNodeGetPointer(pTree, Tcl_GetString(pObj));
            if (pChild) {
                HtmlElementNode *pElem = HtmlNodeAsElement(pNode);
                if (pChild->iNode == HTML_NODE_ORPHAN) {
                    nodeDeorphanize(pTree, pChild);
                }
                nodeInsertChild(pTree, pElem, pBefore, pAfter, pChild);
            }
        }
    }

    pTree->isSequenceOk = 0;
    HtmlCheckRestylePoint(pTree);

    return TCL_OK;
}

static CssPropertySet *
nodeGetStyle(pTree, p)
    HtmlTree *pTree;
    HtmlNode *p;
{
    HtmlElementNode *pElem = HtmlNodeAsElement(p);
    const char *zStyle;
    if (!pElem->pStyle && (zStyle = HtmlNodeAttr(p, "style"))) { 
        HtmlCssInlineParse(pTree, -1, zStyle, &pElem->pStyle);
    }
    return pElem->pStyle;
}

/*
 */
/*
 *---------------------------------------------------------------------------
 *
 * nodeTextCommand --
 *
 *         $node text ?get?
 *         $node text -tokens 
 *         $node text -pre 
 *         $node text set TEXT
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
nodeTextCommand(interp, pNode, objc, objv)
    Tcl_Interp *interp;
    HtmlNode *pNode;
    int objc;
    Tcl_Obj *CONST objv[];
{
    HtmlTree *pTree = pNode->pNodeCmd->pTree;
    Tcl_Obj *pRet = 0;
    int nByte = 0;

    enum NODE_TEXT_enum {
        NODE_TEXT_GET,
        NODE_TEXT_PRE, 
        NODE_TEXT_SET, 
        NODE_TEXT_TOKENS 
    };
    enum NODE_TEXT_enum eChoice;

    static const struct NodeTextSubCommand {
        const char *zCommand;
        enum NODE_TEXT_enum eSymbol;
        int nArg;
    } aSubCommand[] = {
        {"get",       NODE_TEXT_GET, 0},  
        {"-pre",      NODE_TEXT_PRE, 0},      
        {"set",       NODE_TEXT_SET, 1},
        {"-tokens",   NODE_TEXT_TOKENS, 0},      
        {0, 0, 0}
    };

    HtmlTextNode *pTextNode = HtmlNodeAsText(pNode);

    /* This is a no-op for non text nodes */
    if (!pTextNode) return TCL_OK;

    if (objc==2) {
        eChoice = NODE_TEXT_GET;
    } else {
        int iChoice;
        if (Tcl_GetIndexFromObjStruct(interp, objv[2], aSubCommand, 
            sizeof(struct NodeTextSubCommand), "option", 0, &iChoice) 
        ){
            return TCL_ERROR;
        }
        if (objc != 3+aSubCommand[iChoice].nArg) {
            Tcl_WrongNumArgs(interp, 3, objv, ((objc==3)?"TEXT":""));
            return TCL_ERROR;
        }
        eChoice = aSubCommand[iChoice].eSymbol;
    }

    if (eChoice == NODE_TEXT_SET) {
        /* Because the storage for a text nodes data is allocated as
         * part of the HtmlTextNode allocation, the only way to modify
         * the text of a node is to allocate a new HtmlTextNode 
         * structure and then to change all the pointers that refer to
         * it. There are only three possibilities:
         *
         *     * In the Tcl_CmdInfo struct for this node-handle,
         *     * In the parent node, if this is not an orphan.
         *     * In the orphan node table, if this is an orphan.
         */
        const char *zNew;
        int nNew;
        HtmlTextNode *pOrig;

        pOrig = HtmlNodeAsText(pNode);
        assert(pOrig);

        /* Invalidate the layout of this node. */
        HtmlCallbackLayout(pTree, pNode);

        /* Set the node to contain the new text */
        zNew = Tcl_GetStringFromObj(objv[3], &nNew);
        HtmlTextSet(pOrig, nNew, zNew, 0, 0);

    } else if (eChoice == NODE_TEXT_PRE) {
        pRet = nodeGetPreText(HtmlNodeAsText(pNode));
        Tcl_IncrRefCount(pRet);
    } else {
        HtmlTextIter sIter;

        pRet = Tcl_NewObj();
        Tcl_IncrRefCount(pRet);

        for (
            HtmlTextIterFirst((HtmlTextNode *)pNode, &sIter);
            HtmlTextIterIsValid(&sIter);
            HtmlTextIterNext(&sIter)
        ) {
            int eType = HtmlTextIterType(&sIter);
            int nData = HtmlTextIterLength(&sIter);
            char const * zData = HtmlTextIterData(&sIter);
    
            if (eChoice == NODE_TEXT_TOKENS) {
                char *zType = 0;
                Tcl_Obj *p = Tcl_NewObj();
                Tcl_Obj *pObj = 0;
    
                switch (eType) {
                    case HTML_TEXT_TOKEN_TEXT:
                        zType = "text";
                        pObj = Tcl_NewStringObj(zData, nData);
                        break;
                    case HTML_TEXT_TOKEN_SPACE:
                        zType = "space";
                        pObj = Tcl_NewIntObj(nData);
                        break;
                    case HTML_TEXT_TOKEN_NEWLINE:
                        zType = "newline";
                        pObj = Tcl_NewIntObj(nData);
                        break;
                }
                assert(zType);
                Tcl_ListObjAppendElement(
                    0, p, Tcl_NewStringObj(zType, -1)
                );
                Tcl_ListObjAppendElement(0, p, pObj);
                Tcl_ListObjAppendElement(0, pRet, p);
            } else {
                assert(eChoice == NODE_TEXT_GET);
                if (eType == HTML_TEXT_TOKEN_TEXT) {
                    nByte = (HtmlTextIterData(&sIter) - pTextNode->zText);
                    nByte += HtmlTextIterLength(&sIter);
                }
            }
        }
    }

    if (eChoice == NODE_TEXT_GET) {
        Tcl_SetStringObj(pRet, pTextNode->zText, nByte);
    }

    if( pRet ){
        Tcl_SetObjResult(interp, pRet);
        Tcl_DecrRefCount(pRet);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * nodeCommand --
 *
 *         attr                    Read/write node attributes 
 *         children                Return a list of the nodes child nodes 
 *         dynamic                 Set/clear dynamic flags (i.e. :hover) 
 *         override                Read/write CSS property overrides 
 *         parent                  Return the parent node 
 *         prop                    Query CSS property values 
 *         property                Query a single CSS property value 
 *         replace                 Set/clear the node replacement object 
 *         tag                     Read/write the node's tag 
 *         text                    Read/write the node's text content 
 *         xview                   Scroll a scrollable node horizontally 
 *         yview                   Scroll a scrollable node vertically 
 *
 *     This function is the implementation of the Tcl node handle command. The
 *     clientData passed to the command is a pointer to the HtmlNode structure
 *     for the document node. 
 *
 *     When this function is called, ((HtmlNode *)clientData)->pNodeCmd is 
 *     guaranteed to point to a valid HtmlNodeCmd structure.
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
nodeCommand(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[];
{
    HtmlNode *pNode = (HtmlNode *)clientData;
    HtmlTree *pTree = pNode->pNodeCmd->pTree;
    int iChoice;

    enum NODE_enum {
        NODE_ATTRIBUTE, NODE_CHILDREN, NODE_DESTROY, NODE_DYNAMIC, 
        NODE_HTML,
        NODE_INSERT, NODE_OVERRIDE, NODE_PARENT, NODE_PROPERTY, 
        NODE_REMOVE, NODE_REPLACE, NODE_STACKING, NODE_TAG, NODE_TEXT, 
        NODE_XVIEW, NODE_YVIEW
    };

    static const struct NodeSubCommand {
        const char *zCommand;
        enum NODE_enum eSymbol;
        int TODO;
    } aSubCommand[] = {
        {"attribute", NODE_ATTRIBUTE, 0},  
        {"children",  NODE_CHILDREN,  0},
        {"destroy",   NODE_DESTROY,   0},      
        {"dynamic",   NODE_DYNAMIC,   0},      
        {"html",      NODE_HTML,      0},
        {"insert",    NODE_INSERT,    0},
        {"override",  NODE_OVERRIDE,  0},    
        {"parent",    NODE_PARENT,    0},     
        {"property",  NODE_PROPERTY,  0}, 
        {"remove",    NODE_REMOVE,    0},
        {"replace",   NODE_REPLACE,   0}, 
        {"stacking",  NODE_STACKING,  0},    
        {"tag",       NODE_TAG,       0},    
        {"text",      NODE_TEXT,      0},  
        {"xview",     NODE_XVIEW,     0},
        {"yview",     NODE_YVIEW,     0},
        {0, 0, 0}
    };

    if (objc<2) {
        Tcl_WrongNumArgs(interp, 1, objv, "SUBCOMMAND ...");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObjStruct(interp, objv[1], aSubCommand, 
            sizeof(struct NodeSubCommand), "option", 0, &iChoice) 
    ){
        return TCL_ERROR;
    }

    switch (aSubCommand[iChoice].eSymbol) {
        /*
         * nodeHandle attr ??-default DEFAULT-VALUE? ATTR-NAME? ?NEW-VALUE?
         */
        case NODE_ATTRIBUTE: {
            char CONST *zAttr = 0;
            char *zAttrName = 0;
            char *zAttrVal = 0;
            char *zDefault = 0;

            switch (objc) {
                case 2:
                    break;
                case 3:
                    zAttrName = Tcl_GetString(objv[2]);
                    break;
                case 4:
                    zAttrName = Tcl_GetString(objv[2]);
                    zAttrVal = Tcl_GetString(objv[3]);
                    break;
                case 5:
                    if (strcmp(Tcl_GetString(objv[2]), "-default")) {
                        goto node_attr_usage;
                    }
                    zDefault = Tcl_GetString(objv[3]);
                    zAttrName = Tcl_GetString(objv[4]);
                    break;
                default:
                    goto node_attr_usage;
            }

            /* If there are values for both zAttrName and zAttrVal, then
             * set the value of the attribute to the string pointed to by 
             * zAttrVal. After doing this, run the code for an attribute
             * query, so that the new attribute value is returned.
             */
            if (zAttrName && zAttrVal) {
                /* Check if there is an attribute-handler for this type
                 * of node. If so, invoke the script as follows:
                 *
                 *     eval $handler [list $attribute-name] [list $new-value]
                 */
                int rc;
                char *zCopy; 

                assert(!zDefault);

                zCopy = HtmlAlloc("tmp", strlen(zAttrName)+1);
                strcpy(zCopy, zAttrName);
                Tcl_UtfToLower(zCopy);
                rc = doAttributeHandler(pTree, pNode, zCopy, zAttrVal);
                HtmlFree(zCopy);

                if (rc != TCL_OK) {
                    return rc;
                }
                setNodeAttribute(pNode, zAttrName, zAttrVal);
                HtmlCallbackRestyle(pTree, pNode);
            }

            if (zAttrName) {
                zAttr = HtmlNodeAttr(pNode, zAttrName);
                zAttr = (zAttr ? zAttr : zDefault);
                if (zAttr==0) {
                    Tcl_AppendResult(interp, "No such attr: ", zAttrName, 0);
                    return TCL_ERROR;
                }
                Tcl_SetResult(interp, (char *)zAttr, TCL_VOLATILE);
            } else 

            if (!HtmlNodeIsText(pNode)) {
                int i;
                HtmlAttributes *pAttr = ((HtmlElementNode *)pNode)->pAttributes;
                Tcl_Obj *p = Tcl_NewObj();
                for (i = 0; pAttr && i < pAttr->nAttr; i++) {
                    Tcl_Obj *pArg;
                    pArg = Tcl_NewStringObj(pAttr->a[i].zName, -1);
                    Tcl_ListObjAppendElement(interp, p, pArg);
                    pArg = Tcl_NewStringObj(pAttr->a[i].zValue, -1);
                    Tcl_ListObjAppendElement(interp, p, pArg);
                }
                Tcl_SetObjResult(interp, p);
            }
            break;

node_attr_usage:
            Tcl_ResetResult(interp);
            Tcl_AppendResult(interp, "Usage: ",
                Tcl_GetString(objv[0]), " ",
                Tcl_GetString(objv[1]), " ",
                "? ?-default DEFAULT-VALUE? ATTR-NAME ?NEW-VAL??", 0);
            return TCL_ERROR;
        }

        /*
         * nodeHandle children
         *
         *     Return a list of node handles for all children of nodeHandle.
         *     The leftmost child node becomes element 0 of the list, the
         *     second leftmost element 1, and so on.
         */
        case NODE_CHILDREN: {
            if (objc == 2) {
                int i;
                Tcl_Obj *pRes = Tcl_NewObj();
                for (i = 0; i < HtmlNodeNumChildren(pNode); i++) {
                    HtmlNode *pChild = HtmlNodeChild(pNode, i);
                    Tcl_Obj *pCmd = HtmlNodeCommand(pTree, pChild);
                    Tcl_ListObjAppendElement(0, pRes, pCmd);
                }
                Tcl_SetObjResult(interp, pRes);
            } else {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            break;
        }

        /*
         * nodeHandle stacking
         *
         *     Return the node-handle that forms the stacking context
         *     this node is located in. Return "" for the root-element.
         *
         *     Also return "" for any element that is part of an
         *     orphan subtree.
         */
        case NODE_STACKING: {
            HtmlElementNode *pElem = HtmlNodeAsElement(pNode);
            if (!pElem) {
                pElem = HtmlNodeAsElement(HtmlNodeParent(pNode));
            }
            assert(
                pNode==pTree->pRoot || pElem->pStack || HtmlNodeIsOrphan(pNode)
            );
            if ((HtmlNode *)pElem != pTree->pRoot && pElem->pStack) {
                HtmlNode *p = &(pElem->pStack->pElem->node);
                Tcl_SetObjResult(interp, HtmlNodeCommand(pTree, p));
            }
            break;
        }

        case NODE_TAG: {
            char CONST *zTag;
            if (objc!=2) {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            zTag = HtmlNodeTagName(pNode);
            Tcl_SetResult(interp, (char *)zTag, TCL_VOLATILE);
            break;
        }

        case NODE_TEXT: {
            return nodeTextCommand(interp, pNode, objc, objv);
        }

        case NODE_PARENT: {
            HtmlNode *pParent;
            pParent = HtmlNodeParent(pNode);
            if (pParent) {
                Tcl_SetObjResult(interp, HtmlNodeCommand(pTree, pParent));
            } 
            break;
        }

        /*
         * nodeHandle property ?-before|-after|-inline? ?PROPERTY-NAME?
         *
         *     Return the calculated value of a node's CSS property. If the
         *     node is a text node, return the value of the property as
         *     assigned to the parent node.
         */
        case NODE_PROPERTY: {
            int nArg = objc - 2;
            Tcl_Obj * CONST *aArg = &objv[2];

            HtmlComputedValues *pComputed; 
            HtmlNode *p = pNode;

            /* This method is a no-op for text nodes */
            if (HtmlNodeIsText(p)) break;

            if (nArg > 0 && 0 == strcmp(Tcl_GetString(aArg[0]), "-inline")) {
                CssPropertySet *pSet = nodeGetStyle(pTree, p);
                if (nArg == 1) return HtmlCssInlineQuery(interp, pSet, 0);
                if (nArg == 2) return HtmlCssInlineQuery(interp, pSet, aArg[1]);
                /* Otherwise, fall through for the WrongNumArgs() message */
            }

	    /* Orphan nodes may have an inline style specified (required by 
             * DOM implementations to implement HTMLElement.style), but 
             * they do not have a computed style, so the rest of this 
             * function is a no-op for orphan nodes.
             */
            if (HtmlNodeIsOrphan(p)) break;
            HtmlCallbackForce(pTree);

            if (nArg > 0) {
                HtmlElementNode *pElem = HtmlNodeAsElement(pNode);
                char *zArg0 = Tcl_GetString(aArg[0]);
                if (0 == strcmp(zArg0, "-before")) {
                    p = pElem ? pElem->pBefore : 0;
                    aArg = &aArg[1];
                    nArg--;
                }
                else if (0 == strcmp(zArg0, "-after")) {
                    p = pElem ? pElem->pAfter : 0;
                    aArg = &aArg[1];
                    nArg--;
                }
            }

            /* If the -before or -after switch was specified, and the
             * element doesn't have any corresponding generated content,
             * then p is NULL at this point. Return an empty string.
             */
            if (!p) {
                return TCL_OK;
            }

            pComputed = HtmlNodeComputedValues(p);

            switch (nArg) {
                case 0:
                    return HtmlNodeProperties(interp, pComputed);
                case 1:
                    return HtmlNodeGetProperty(interp, objv[2], pComputed);
                default:
                    Tcl_WrongNumArgs(
                        interp, 2, objv,"?-before|-after|-inline? PROPERTY-NAME"
                    );
                    return TCL_ERROR;
            }
            break;
        }

        /*
         * nodeHandle replace ?new-value? ?options?
         *
         *     supported options are:
         *
         *         -configurecmd       <script>
         *         -deletecmd          <script>
         *         -stylecmd           <script>
         */
        case NODE_REPLACE: {

            HtmlElementNode *pElem = HtmlNodeAsElement(pNode);
            if (!pElem) {
                char *zErr = "Text node does not support [replace]";
                Tcl_SetResult(interp, zErr, 0);
                return TCL_ERROR;
            }

            if (objc > 2) {
                Tcl_Obj *aArgs[4];
                HtmlNodeReplacement *pReplace = 0; /* New pNode->pReplacement */
                Tk_Window widget;            /* Replacement widget */
                Tk_Window mainwin = Tk_MainWindow(pTree->interp);

                const char *zWin = 0;        /* Replacement window name */

                SwprocConf aArgConf[] = {
                    {SWPROC_ARG, "new-value", 0, 0},      /* aArgs[0] */
                    {SWPROC_OPT, "configurecmd", 0, 0},   /* aArgs[1] */
                    {SWPROC_OPT, "deletecmd", 0, 0},      /* aArgs[2] */
                    {SWPROC_OPT, "stylecmd", 0, 0},       /* aArgs[3] */
                    {SWPROC_END, 0, 0, 0}
                };
                if (SwprocRt(interp, objc - 2, &objv[2], aArgConf, aArgs)) {
                    return TCL_ERROR;
                }

                zWin = Tcl_GetString(aArgs[0]);

                if (zWin[0]) {
        	    /* If the replacement object is a Tk window,
                     * register Tkhtml as the geometry manager.
                     */
                    widget = Tk_NameToWindow(interp, zWin, mainwin);
                    if (widget) {
                        static Tk_GeomMgr sManage = {
                            "Tkhtml",
                            geomRequestProc,
                            0
                        };
                        Tk_ManageGeometry(widget, &sManage, pNode);
                    }

                    pReplace = HtmlNew(HtmlNodeReplacement);
                    pReplace->pReplace = aArgs[0];
                    pReplace->pConfigureCmd = aArgs[1];
                    pReplace->pDelete = aArgs[2];
                    pReplace->pStyleCmd = aArgs[3];
                    pReplace->win = widget;
                }

        	/* Free any existing replacement object and set
        	 * pNode->pReplacement to point at the new structure. 
                 */
                clearReplacement(pTree, pElem);
                pElem->pReplacement = pReplace;

                /* Run the layout engine. */
                HtmlCallbackLayout(pTree, pNode);
            }

            /* The result of this command is the name of the current
             * replacement object (or an empty string).
             */
            if (pElem->pReplacement) {
                assert(pElem->pReplacement->pReplace);
                Tcl_SetObjResult(interp, pElem->pReplacement->pReplace);
            }
            break;
        }

        /*
         * nodeHandle dynamic set|clear ?flag?
         * nodeHandle dynamic conditions
         *
         *     Note that the [nodeHandle dynamic conditions] command is for
         *     debugging only. It is not documented in the man page.
         */
        case NODE_DYNAMIC: {
            struct DynamicFlag {
                const char *zName;
                Html_u8 flag;
            } flags[] = {
                {"active",  HTML_DYNAMIC_ACTIVE}, 
                {"focus",   HTML_DYNAMIC_FOCUS}, 
                {"hover",   HTML_DYNAMIC_HOVER},
                {"link",    HTML_DYNAMIC_LINK},
                {"visited", HTML_DYNAMIC_VISITED},
                {0, 0}
            };
            const char *zArg1 = (objc>2) ? Tcl_GetString(objv[2]) : 0;
            const char *zArg2 = (objc>3) ? Tcl_GetString(objv[3]) : 0;
            Tcl_Obj *pRet;
            int i;
            Html_u8 mask = 0;

            HtmlElementNode *pElem = (HtmlElementNode *)pNode;
            if (HtmlNodeIsText(pNode)) {
                Tcl_SetResult(interp, 
                    "Cannot call method [dynamic] on a text node", 0
                );
                return TCL_ERROR;
            }

            if (zArg1 && 0 == strcmp(zArg1, "conditions")) {
                HtmlCallbackForce(pTree);
                return HtmlCssTclNodeDynamics(interp, pNode);
            }

            if (zArg2) {
                for (i = 0; flags[i].zName; i++) {
                    if (0 == strcmp(zArg2, flags[i].zName)) {
                        mask = flags[i].flag;
                        break;
                    }
                }
                if (!mask) {
                    Tcl_AppendResult(interp, 
                        "Unsupported dynamic CSS flag: ", zArg2, 0);
                    return TCL_ERROR;
                }
            }

            if ( 
                !zArg1 || 
                (strcmp(zArg1, "set") && strcmp(zArg1, "clear")) ||
                (zArg2 && !mask)
            ) {
                Tcl_WrongNumArgs(interp, 2, objv, "set|clear ?flag?");
                return TCL_ERROR;
            }

            if (*zArg1 == 's') {
                pElem->flags |= mask;
            } else {
                pElem->flags &= ~(mask?mask:0xFF);
            }

            if (zArg2) {
                if (
                    mask == HTML_DYNAMIC_LINK || 
                    mask == HTML_DYNAMIC_VISITED
                ) {
                    HtmlCallbackRestyle(pTree, pNode);
                } else {
                    HtmlCallbackDynamic(pTree, pNode);
                }
            }

            pRet = Tcl_NewObj();
            for (i = 0; flags[i].zName; i++) {
                if (pElem->flags & flags[i].flag) {
                    Tcl_Obj *pNew = Tcl_NewStringObj(flags[i].zName, -1);
                    Tcl_ListObjAppendElement(0, pRet, pNew);
                }
            }
            Tcl_SetObjResult(interp, pRet);
            break;
        }

        /*
         * nodeHandle override ?new-value?
         *
         *     Get/set the override list.
         */
        case NODE_OVERRIDE: {

            HtmlElementNode *pElem = (HtmlElementNode *)pNode;
            if (HtmlNodeIsText(pNode)) {
                Tcl_SetResult(interp, 
                    "Cannot call method [override] on a text node", 0
                );
                return TCL_ERROR;
            }

            if (objc != 2 && objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "?new-value?");
                return TCL_ERROR;
            }

            if (objc == 3) {
                if (pElem->pOverride) {
                    Tcl_DecrRefCount(pElem->pOverride);
                }
                pElem->pOverride = objv[2];
                Tcl_IncrRefCount(pElem->pOverride);
            }

            Tcl_ResetResult(interp);
            if (pElem->pOverride) {
                Tcl_SetObjResult(interp, pElem->pOverride);
            }
            HtmlCallbackRestyle(pTree, pNode);
            return TCL_OK;
        }

        case NODE_XVIEW: {
            return nodeViewCmd(pNode, 0, objv, objc);
        }
        case NODE_YVIEW: {
            return nodeViewCmd(pNode, 1, objv, objc);
        }

        /*
         * nodeHandle html
         */
        case NODE_HTML: {
            if (objc != 2) {
                Tcl_WrongNumArgs(interp, 2, objv, "");
                return TCL_ERROR;
            }
            Tcl_SetResult(interp, 
                (char *)Tcl_GetCommandName(interp, pTree->cmd), TCL_STATIC
            );
            return TCL_OK;
        }

        /*
         * nodeHandle insert ?-before NODE? NODE-LIST
         *
         */
        case NODE_INSERT: {
            HtmlCallbackRestyle(pTree, pNode);
            HtmlCallbackLayout(pTree, pNode);
            return nodeInsertCmd(pNode, objc, objv);
        }

        /*
         * nodeHandle remove NODE-LIST
         */
        case NODE_REMOVE: {
            HtmlCallbackRestyle(pTree, pNode);
            HtmlCallbackLayout(pTree, pNode);
            return nodeRemoveCmd(pNode, objc, objv);
        }

        /*
         * nodeHandle destroy
         */
        case NODE_DESTROY: {
            return nodeDestroyCmd(pNode, objc, objv);
        }

        default:
            assert(!"Impossible!");
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeCommand --
 *
 *     Return a Tcl object containing the name of the Tcl command used to
 *     access pNode. If the command does not already exist it is created.
 *
 *     The Tcl_Obj * returned is always a pointer to pNode->pCommand.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj *
HtmlNodeCommand(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    static int nodeNumber = 0;
    HtmlNodeCmd *pNodeCmd = pNode->pNodeCmd;

    if (pNode->iNode == HTML_NODE_GENERATED) {
        return 0;
    }

    if (!pNodeCmd) {
        char zBuf[100];
        Tcl_Obj *pCmd;
        sprintf(zBuf, "::tkhtml::node%d", nodeNumber++);

        pCmd = Tcl_NewStringObj(zBuf, -1);
        Tcl_IncrRefCount(pCmd);
        Tcl_CreateObjCommand(pTree->interp, zBuf, nodeCommand, pNode, 0);
        pNodeCmd = HtmlNew(HtmlNodeCmd);
        pNodeCmd->pCommand = pCmd;
        pNodeCmd->pTree = pTree;
        pNode->pNodeCmd = pNodeCmd;
    }

    return pNodeCmd->pCommand;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeScrollbarDoCallback --
 *
 *     If node pNode is scrollable, invoke the [$scrollbar set] command
 *     for each of it's scrollbar widgets.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Invokes 0-2 [$scrollbar set] scripts.
 *
 *---------------------------------------------------------------------------
 */
int HtmlNodeScrollbarDoCallback(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlElementNode *pElem = (HtmlElementNode *)pNode;

    if (!HtmlNodeIsText(pNode) && pElem->pScrollbar) {
        HtmlNodeScrollbars *p = pElem->pScrollbar;
        char zTmp[256];
        if (p->vertical.win) {
            snprintf(zTmp, 255, "%s set %f %f", 
                Tcl_GetString(p->vertical.pReplace), 
                (double)p->iVertical / (double)p->iVerticalMax,
                (double)(p->iVertical + p->iHeight) / (double)p->iVerticalMax
            );
            zTmp[255] = '\0';
            Tcl_Eval(pTree->interp, zTmp);
        }
        if (p->horizontal.win) {
            snprintf(zTmp, 255, "%s set %f %f", 
                Tcl_GetString(p->horizontal.pReplace), 
                (double)p->iHorizontal / (double)p->iHorizontalMax,
                (double)(p->iHorizontal + p->iWidth) / (double)p->iHorizontalMax
            );
            zTmp[255] = '\0';
            Tcl_Eval(pTree->interp, zTmp);
        }
    }

    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * HtmlTreeClear --
 *
 *     Completely reset the widgets internal structures - for example when
 *     loading a new document.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Deletes internal document representation.
 *
 *---------------------------------------------------------------------------
 */
int HtmlTreeClear(pTree)
    HtmlTree *pTree;
{
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch search;

    /* Free the canvas representation */
    HtmlDrawCleanup(pTree, &pTree->canvas);
    memset(&pTree->canvas, 0, sizeof(HtmlCanvas));

    /* Free any snapshot */
    HtmlDrawSnapshotFree(pTree, pTree->cb.pSnapshot);
    pTree->cb.pSnapshot = 0;

    /* Free the contents of the search-cache */
    HtmlCssSearchInvalidateCache(pTree);

    /* Free the tree representation - pTree->pRoot */
    freeNode(pTree, pTree->pRoot);
    pTree->pRoot = 0;
    pTree->state.pCurrent = 0;
    pTree->state.pFoster = 0;

    /* Free any orphan nodes associated with this tree: */
    for (
        pEntry = Tcl_FirstHashEntry(&pTree->aOrphan, &search);
        pEntry;
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        HtmlNode *pOrphan = (HtmlNode *)Tcl_GetHashKey(&pTree->aOrphan, pEntry);
        assert(pOrphan->iNode == HTML_NODE_ORPHAN);
        freeNode(pTree, pOrphan);
    }
    Tcl_DeleteHashTable(&pTree->aOrphan);
    Tcl_InitHashTable(&pTree->aOrphan, TCL_ONE_WORD_KEYS);

    /* Free the formatted text, if any (HtmlTree.pText) */
    HtmlTextInvalidate(pTree);

    /* Free the plain text representation */
    if (pTree->pDocument) {
        Tcl_DecrRefCount(pTree->pDocument);
    }
    pTree->nParsed = 0;
    pTree->pDocument = 0;

    /* Free the stylesheets */
    HtmlCssStyleSheetFree(pTree->pStyle);
    pTree->pStyle = 0;

    /* Set the scroll position to top-left and clear the selection */
    pTree->iScrollX = 0;
    pTree->iScrollY = 0;

    /* Deschedule any dynamic, style or layout callback. */
    pTree->cb.pDynamic = 0;
    pTree->cb.pRestyle = 0;
    pTree->cb.flags &= ~(HTML_DYNAMIC|HTML_RESTYLE|HTML_LAYOUT);

    pTree->iNextNode = 0;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeGetPointer --
 *
 *     String argument zCmd is the name of a node command created for
 *     some node of tree pTree. Find the corresponding HtmlNode pointer
 *     and return it. If zCmd is not the name of a node command, leave
 *     an error in pTree->interp and return NULL.
 *
 * Results:
 *     Pointer to node object associated with Tcl command zCmd, or NULL.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlNode *
HtmlNodeGetPointer(pTree, zCmd)
    HtmlTree *pTree;
    char CONST *zCmd;
{
    Tcl_Interp *interp = pTree->interp;
    Tcl_CmdInfo info;
    int rc;

    rc = Tcl_GetCommandInfo(interp, zCmd, &info);
    if (rc == 0 || info.objProc != nodeCommand){ 
        Tcl_AppendResult(interp, "no such node: ", zCmd, 0);
        return 0;
    }
    return (HtmlNode *)info.objClientData;
}

/************************************************************************
 * Start of [fragment] parsing code.
 */

static void
fragmentOrphan(pTree)
    HtmlTree *pTree;
{
    HtmlFragmentContext *pFragment = pTree->pFragment;
    HtmlNode *pOrphan = pFragment->pRoot;

    if (pOrphan) {
        Tcl_Obj *pCmd = HtmlNodeCommand(pTree, pOrphan);
        Tcl_ListObjAppendElement(0, pFragment->pNodeList, pCmd);
        nodeOrphanize(pTree, pOrphan);
        pFragment->pRoot = 0;
        pFragment->pCurrent = 0;
    }

    assert(!pFragment->pRoot && !pFragment->pCurrent);
}

static void 
fragmentAddText(pTree, pTextNode, iOffset)
    HtmlTree *pTree;
    HtmlTextNode *pTextNode; 
    int iOffset;
{
    HtmlFragmentContext *pFragment = pTree->pFragment;

    pTextNode->node.eTag = Html_Text;

    if (pFragment->pRoot) {
        /* If there is a fragment root node, add the new text node
         * as the right-most child of HtmlFragmentContext.pCurrent.
         */
        nodeInsertChild(pTree, pFragment->pCurrent, 0,0, (HtmlNode *)pTextNode);
    } else {
        /* The text node becomes the a sub-tree all on it's own. */
        pFragment->pRoot = (HtmlNode *)pTextNode;
        fragmentOrphan(pTree);
    }
}

static void 
fragmentAddElement(pTree, eType, zType, pAttributes, iOffset)
    HtmlTree *pTree;
    int eType;
    const char *zType;               /* Atom */
    HtmlAttributes *pAttributes; 
    int iOffset;
{
    HtmlElementNode *pElem;
    HtmlFragmentContext *pFragment = pTree->pFragment;
    int nClose;
    int ii;

    switch (eType) {
        /* Ignore <HEAD>, <BODY> and elements that occur as descendants of
         * <HEAD> completely. TODO: This will have to change....
         */
        case Html_HTML:
        case Html_HEAD:
        case Html_BODY:
        case Html_TITLE:
        case Html_META:
        case Html_LINK:
        case Html_BASE:
            return;
    }

    implicitCloseCount(pTree, pFragment->pCurrent, eType, &nClose);
    for (ii = 0; ii < nClose; ii++) {
        HtmlNode *pC = &pFragment->pCurrent->node;
        HtmlNode *pParentC = HtmlNodeParent(pC);
        assert(pC);
        nodeHandlerCallbacks(pTree, pC);
        pFragment->pCurrent = (HtmlElementNode *)pParentC;
    }
    if (!pFragment->pCurrent) {
        fragmentOrphan(pTree);
    }

    pElem = HtmlNew(HtmlElementNode);
    pElem->pAttributes = pAttributes;
    pElem->node.eTag = eType;
    if (!zType) {
        zType = HtmlTypeToName(0, eType);
    }
    pElem->node.zTag = zType;

    if (pFragment->pCurrent) {
        nodeInsertChild(pTree, pFragment->pCurrent, 0, 0, (HtmlNode *)pElem);
    } else {
        assert(!pFragment->pRoot);
        pFragment->pRoot = (HtmlNode *)pElem;
        pElem->node.iNode = HTML_NODE_ORPHAN;
    }
    pFragment->pCurrent = pElem;

    if (HtmlMarkup(eType)->flags & HTMLTAG_EMPTY) {
        nodeHandlerCallbacks(pTree, pFragment->pCurrent);
        pFragment->pCurrent = (HtmlElementNode *)HtmlNodeParent(pElem);
    }
    if (!pFragment->pCurrent) {
        fragmentOrphan(pTree);
    }
}

static void 
fragmentAddClosingTag(pTree, eType, zType, iOffset)
    HtmlTree *pTree;
    int eType;
    const char *zType;
    int iOffset;
{
    int nClose;
    int ii;
    HtmlFragmentContext *p = pTree->pFragment;
    explicitCloseCount(p->pCurrent, eType, zType, &nClose);
    for (ii = 0; ii < nClose; ii++) {
        assert(p->pCurrent);
        nodeHandlerCallbacks(pTree, p->pCurrent);
        p->pCurrent = (HtmlElementNode *)HtmlNodeParent(p->pCurrent);
    }
    if (!p->pCurrent) {
        fragmentOrphan(pTree);
    }
}

void
HtmlParseFragment(pTree, zHtml)
    HtmlTree *pTree;
    const char *zHtml;
{
    HtmlFragmentContext sContext;

    assert(!pTree->pFragment);
    sContext.pRoot = 0;
    sContext.pCurrent = 0;
    sContext.pNodeList = Tcl_NewObj();

    pTree->pFragment = &sContext;
    HtmlTokenize(pTree, zHtml, 1,
        fragmentAddText, fragmentAddElement, fragmentAddClosingTag
    );

    while (sContext.pCurrent) {
        HtmlNode *pParent = HtmlNodeParent(sContext.pCurrent); 
        nodeHandlerCallbacks(pTree, sContext.pCurrent);
        sContext.pCurrent = (HtmlElementNode *)pParent;
    }

    fragmentOrphan(pTree);
    pTree->pFragment = 0;
    Tcl_SetObjResult(pTree->interp, sContext.pNodeList);
}

/*
 *---------------------------------------------------------------------------
 *
 * sequenceCb --
 *
 *     This is an HtmlWalkTree() callback function (the tree iteration
 *     is started from with HtmlSequenceNodes()).
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
sequenceCb(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    pNode->iNode = pTree->iNextNode++;
    return HTML_WALK_DESCEND;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlSequenceNodes --
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void
HtmlSequenceNodes(pTree)
    HtmlTree *pTree;
{
    if (!pTree->isSequenceOk) {
        pTree->iNextNode = 0;
        HtmlWalkTree(pTree, 0, sequenceCb, 0);
        pTree->isSequenceOk = 1;
    }
}

