/*
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
static char const rcsid[] = "@(#) $Id: htmltcl.c,v 1.207 2008/01/16 06:29:27 danielk1977 Exp $";

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "html.h"
#include "restrack.h"
#include "swproc.h"

#include <time.h>

#include "htmldefaultstyle.c"

#define LOG if (pTree->options.logcmd)

#define SafeCheck(interp,str) if (Tcl_IsSafe(interp)) { \
    Tcl_AppendResult(interp, str, " invalid in safe interp", 0); \
    return TCL_ERROR; \
}

/* We need to get at the TkBindEventProc() function, which is in the
 * internal stubs table for Tk. So create this structure and hope that
 * it is compatible enough.
 */
struct MyTkIntStubs {
  int magic;
  void *hooks;
  void (*zero)(void);
  void (*one)(void);
  void (*two)(void);
  void (*three)(void);
  void (*tkBindEventProc)(Tk_Window winPtr, XEvent *eventPtr);  /* 4 */
};
extern struct MyTkIntStubs *tkIntStubsPtr;

#ifndef NDEBUG
static int 
allocCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return Rt_AllocCommand(0, interp, objc, objv);
}
static int 
heapdebugCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlHeapDebug(0, interp, objc, objv);
}
static int 
hashstatsCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tcl_HashEntry *p;
    Tcl_HashSearch search;
    int nObj = 0;
    int nRef = 0;
    char zRes[128];

    for (
        p = Tcl_FirstHashEntry(&pTree->aValues, &search); 
        p; 
        p = Tcl_NextHashEntry(&search)
    ) {
        HtmlComputedValues *pV = 
            (HtmlComputedValues *)Tcl_GetHashKey(&pTree->aValues, p);
        nObj++;
        nRef += pV->nRef;
    }

    sprintf(zRes, "%d %d", nObj, nRef);
    Tcl_SetResult(interp, zRes, TCL_VOLATILE);
    return TCL_OK;
}
#endif

struct SubCmd {
    const char *zName;
    Tcl_ObjCmdProc *xFunc;
};
typedef struct SubCmd SubCmd;


/*
 *---------------------------------------------------------------------------
 *
 * HtmlLog --
 * HtmlTimer --
 *
 *     This function is used by various parts of the widget to output internal
 *     information that may be useful in debugging. 
 *
 *     The first argument is the HtmlTree structure. The second is a string
 *     identifying the sub-system logging the message. The third argument is a
 *     printf() style format string and subsequent arguments are substituted
 *     into it to form the logged message.
 *
 *     If -logcmd is set to an empty string, this function is a no-op.
 *     Otherwise, the name of the subsystem and the formatted error message are
 *     appended to the value of the -logcmd option and the result executed as a
 *     Tcl script.
 *
 *     This function is replaced with an empty macro if NDEBUG is defined at
 *     compilation time. If the "-logcmd" option is set to an empty string it
 *     returns very quickly.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Invokes the -logcmd script, if it is not "".
 *
 *---------------------------------------------------------------------------
 */
void 
logCommon(
    HtmlTree *pTree, 
    Tcl_Obj *pLogCmd, 
    CONST char *zSubject, 
    CONST char *zFormat, 
    va_list ap
)
{
    if (pLogCmd) {
        char *zDyn = 0;
        char zStack[200];
        char *zBuf = zStack;
        int nBuf;
        Tcl_Obj *pCmd;

        nBuf = vsnprintf(zBuf, 200, zFormat, ap);
        if (nBuf >= 200) {
            zDyn = HtmlAlloc(0, nBuf + 10);
            zBuf = zDyn;
            nBuf = vsnprintf(zBuf, nBuf + 1, zFormat, ap);
        }

        pCmd = Tcl_DuplicateObj(pLogCmd);
        Tcl_IncrRefCount(pCmd);
        Tcl_ListObjAppendElement(0, pCmd, Tcl_NewStringObj(zSubject, -1));
        Tcl_ListObjAppendElement(0, pCmd, Tcl_NewStringObj(zBuf, nBuf));

        if (Tcl_EvalObjEx(pTree->interp, pCmd, TCL_GLOBAL_ONLY)) {
            Tcl_BackgroundError(pTree->interp);
        }

        Tcl_DecrRefCount(pCmd);
        HtmlFree(zDyn);
    }
}

void 
HtmlTimer(HtmlTree *pTree, CONST char *zSubject, CONST char *zFormat, ...) {
    va_list ap;
    va_start(ap, zFormat);
    logCommon(pTree, pTree->options.timercmd, zSubject, zFormat, ap);
}
void 
HtmlLog(HtmlTree *pTree, CONST char *zSubject, CONST char *zFormat, ...) {
    va_list ap;
    va_start(ap, zFormat);
    logCommon(pTree, pTree->options.logcmd, zSubject, zFormat, ap);
}

/*
 *---------------------------------------------------------------------------
 *
 * doLoadDefaultStyle --
 *
 *     Load the default-style sheet into the current stylesheet configuration.
 *     The text of the default stylesheet is stored in the -defaultstyle
 *     option.
 *
 *     This function is called once when the widget is created and each time
 *     [.html reset] is called thereafter.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Loads the default style.
 *
 *---------------------------------------------------------------------------
 */
static void
doLoadDefaultStyle(pTree)
    HtmlTree *pTree;
{
    Tcl_Obj *pObj = pTree->options.defaultstyle;
    Tcl_Obj *pId = Tcl_NewStringObj("agent", 5);
    assert(pObj);
    Tcl_IncrRefCount(pId);
    HtmlStyleParse(pTree, pObj, pId, 0, 0, 0);
    Tcl_DecrRefCount(pId);
}

/*
 *---------------------------------------------------------------------------
 *
 * doSingleScrollCallback --
 *
 *     Helper function for doScrollCallback().
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May invoke the script in *pScript.
 *
 *---------------------------------------------------------------------------
 */
static void
doSingleScrollCallback(interp, pScript, iOffScreen, iTotal, iPage)
    Tcl_Interp *interp;
    Tcl_Obj *pScript;
    int iOffScreen;
    int iTotal;
    int iPage;
{
    if (pScript) {
        double fArg1;
        double fArg2;
        int rc;
        Tcl_Obj *pEval; 

        if (iTotal == 0){ 
            fArg1 = 0.0;
            fArg2 = 1.0;
        } else {
            fArg1 = (double)iOffScreen / (double)iTotal;
            fArg2 = (double)(iOffScreen + iPage) / (double)iTotal;
            fArg2 = MIN(1.0, fArg2);
        }

        pEval = Tcl_DuplicateObj(pScript);
        Tcl_IncrRefCount(pEval);
        Tcl_ListObjAppendElement(interp, pEval, Tcl_NewDoubleObj(fArg1));
        Tcl_ListObjAppendElement(interp, pEval, Tcl_NewDoubleObj(fArg2));
        rc = Tcl_EvalObjEx(interp, pEval, TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL);
        if (TCL_OK != rc) {
            Tcl_BackgroundError(interp);
        }
        Tcl_DecrRefCount(pEval);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * doScrollCallback --
 *
 *     Invoke both the -xscrollcommand and -yscrollcommand scripts (unless they
 *     are set to empty strings). The arguments passed to the two scripts are
 *     calculated based on the current values of HtmlTree.iScrollY,
 *     HtmlTree.iScrollX and HtmlTree.canvas.
 *
 * Results:
 *     None.
 *
 * Side effects: 
 *     May invoke either or both of the -xscrollcommand and -yscrollcommand
 *     scripts.
 *
 *---------------------------------------------------------------------------
 */
static void 
doScrollCallback(pTree)
    HtmlTree *pTree;
{
    Tcl_Interp *interp = pTree->interp;
    Tk_Window win = pTree->tkwin;

    Tcl_Obj *pScrollCommand;
    int iOffScreen;
    int iTotal;
    int iPage;

    if (!Tk_IsMapped(win)) return;

    pScrollCommand = pTree->options.yscrollcommand;
    iOffScreen = pTree->iScrollY;
    iTotal = pTree->canvas.bottom;
    iPage = Tk_Height(win);
    doSingleScrollCallback(interp, pScrollCommand, iOffScreen, iTotal, iPage);

    pScrollCommand = pTree->options.xscrollcommand;
    iOffScreen = pTree->iScrollX;
    iTotal = pTree->canvas.right;
    iPage = Tk_Width(win);
    doSingleScrollCallback(interp, pScrollCommand, iOffScreen, iTotal, iPage);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCheckRestylePoint --
 *
 *     Check that for each node in the tree one of the following is true:
 *
 *       1. The node is a text node, or
 *       2. The node has a computed style (HtmlElementNode.pComputed!=0), or
 *       3. The node is HtmlTree.cb.pRestyle, or
 *       4. The node is a descendant of a pRestyle or a descendent of
 *          a right-sibling of pRestyle.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
#ifndef NDEBUG
static int
checkRestylePointCb(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    HtmlNode *pParent;
    HtmlNode *p;

    /* Condition 1 */
    if (HtmlNodeIsText(pNode)) goto ok_out;

    /* Condition 2 */
    if (HtmlNodeComputedValues(pNode)) goto ok_out;

    /* Condition 3 */
    if (pNode==pTree->cb.pRestyle) goto ok_out;

    /* Condition 4 */
    assert(pTree->cb.pRestyle);
    pParent = HtmlNodeParent(pTree->cb.pRestyle);
    for (p = pNode; p && HtmlNodeParent(p) != pParent; p = HtmlNodeParent(p));
    assert(p);
    for ( ; p && p != pTree->cb.pRestyle; p = HtmlNodeLeftSibling(p));
    assert(p);

ok_out:
    return HTML_WALK_DESCEND;
}
void
HtmlCheckRestylePoint(pTree)
    HtmlTree *pTree;
{
    HtmlWalkTree(pTree, 0, checkRestylePointCb, 0);
}
#endif /* #ifndef NDEBUG */

static void callbackHandler(ClientData clientData);
static void runDynamicStyleEngine(ClientData clientData);
static void runStyleEngine(ClientData clientData);
static void runLayoutEngine(ClientData clientData);

#if defined(TKHTML_ENABLE_PROFILE)
  #define INSTRUMENTED(name, id)                                             \
    static void real_ ## name (ClientData);                                  \
    static void name (clientData)                                            \
      ClientData clientData;                                                 \
    {                                                                        \
      HtmlTree *p = (HtmlTree *)clientData;                                  \
      HtmlInstrumentCall(p->pInstrumentData, id, real_ ## name, clientData); \
    }                                                                        \
    static void real_ ## name (clientData)                                   \
      ClientData clientData;                                                 
#else
  #define INSTRUMENTED(name, id)                                             \
    static void name (clientData)                                            \
      ClientData clientData;                                                 
#endif

INSTRUMENTED(runDynamicStyleEngine, HTML_INSTRUMENT_DYNAMIC_STYLE_ENGINE)
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    assert(pTree->cb.pDynamic);
    HtmlCssCheckDynamic(pTree);
}

INSTRUMENTED(runStyleEngine, HTML_INSTRUMENT_STYLE_ENGINE)
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlNode *pRestyle = pTree->cb.pRestyle;

    pTree->cb.pRestyle = 0;
    assert(pTree->cb.pSnapshot);
    assert(pRestyle);

    HtmlStyleApply(pTree, pRestyle);
    HtmlRestackNodes(pTree);
    HtmlCheckRestylePoint(pTree);

    if (!pTree->options.imagecache) {
        HtmlImageServerDoGC(pTree);
    }
}

INSTRUMENTED(runLayoutEngine, HTML_INSTRUMENT_LAYOUT_ENGINE)
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlDamage *pD;

    assert(pTree->cb.pSnapshot);

    if (!pTree->options.enablelayout) return;

    pD = pTree->cb.pDamage;
    HtmlLayout(pTree);
    if (0 && pTree->cb.isForce) {
        pTree->cb.flags |= HTML_SCROLL;
    }
    if (!pTree->cb.pSnapshot) {
        pTree->cb.flags |= HTML_NODESCROLL;
    }

    doScrollCallback(pTree);
}

/*
 *---------------------------------------------------------------------------
 *
 * callbackHandler --
 *
 *     This is called, usually from an idle-callback handler, to update
 *     the widget display. This may involve all manner of stuff, depending
 *     on the bits set in the HtmlTree.cb.flags mask. Basically, it
 *     is a 5 step process:
 *
 *         1. Dynamic
 *         2. Style engine, 
 *         3. Layout engine, 
 *         4. Repair,
 *         5. Scroll
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
INSTRUMENTED(callbackHandler, HTML_INSTRUMENT_CALLBACK)
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlCallback *p = &pTree->cb;

    int offscreen;
    int force_redraw = 0;

    assert(
        !pTree->pRoot ||
        HtmlNodeComputedValues(pTree->pRoot) ||
        pTree->cb.pRestyle==pTree->pRoot
    );
    HtmlCheckRestylePoint(pTree);

    HtmlLog(pTree, "CALLBACK", 
        "flags=( %s%s%s%s%s) pDynamic=%s pRestyle=%s scroll=(+%d+%d) ",
        (p->flags & HTML_DYNAMIC ? "Dynamic " : ""),
        (p->flags & HTML_RESTYLE ? "Style " : ""),
        (p->flags & HTML_LAYOUT ? "Layout " : ""),
        (p->flags & HTML_DAMAGE ? "Damage " : ""),
        (p->flags & HTML_SCROLL ? "Scroll " : ""),
        (p->pDynamic?Tcl_GetString(HtmlNodeCommand(pTree,p->pDynamic)):"N/A"),
        (p->pRestyle?Tcl_GetString(HtmlNodeCommand(pTree,p->pRestyle)):"N/A"),
         p->iScrollX, p->iScrollY
    );

    assert(!pTree->cb.inProgress);
    pTree->cb.inProgress = 1;

    /* If the HTML_DYNAMIC flag is set, then call HtmlCssCheckDynamic()
     * to recalculate all the dynamic CSS rules that may apply to 
     * the sub-tree rooted at HtmlCallback.pDynamic. CssCheckDynamic() 
     * calls HtmlCallbackRestyle() if any computed style values are 
     * modified (setting the HTML_RESTYLE flag). 
     */
    if (pTree->cb.flags & HTML_DYNAMIC) {
        runDynamicStyleEngine(clientData);
    }
    HtmlCheckRestylePoint(pTree);
    pTree->cb.flags &= ~HTML_DYNAMIC;

    /* If the HtmlCallback.pRestyle variable is set, then recalculate 
     * style information for the sub-tree rooted at HtmlCallback.pRestyle,
     * and the sub-trees rooted at all right-siblings of pRestyle. 
     * Note that restyling a node may invoke the -imagecmd callback.
     *
     * Todo: This seems dangerous.  What happens if the -imagecmd calls
     * [.html parse] or something?
     */
    if (pTree->cb.flags & HTML_RESTYLE) {
        runStyleEngine(clientData);
    }
    pTree->cb.flags &= ~HTML_RESTYLE;

    /* If the HTML_LAYOUT flag is set, run the layout engine. If the layout
     * engine is run, then also set the HTML_SCROLL bit in the
     * HtmlCallback.flags bitmask. This ensures that the entire display is
     * redrawn and that the Tk windows for any replaced nodes are correctly
     * mapped, unmapped or moved.
     */
    assert(pTree->cb.pDamage == 0 || pTree->cb.flags & HTML_DAMAGE);
    if (pTree->cb.flags & HTML_LAYOUT) {
        runLayoutEngine(clientData);
    }
    pTree->cb.flags &= ~HTML_LAYOUT;

    if (pTree->cb.pSnapshot) {
        HtmlCanvasSnapshot *pSnapshot = 0;
        HtmlDrawSnapshotDamage(pTree, pTree->cb.pSnapshot, &pSnapshot);
        HtmlDrawSnapshotFree(pTree, pTree->cb.pSnapshot);
        HtmlDrawSnapshotFree(pTree, pSnapshot);
        pTree->cb.pSnapshot = 0;
    }

    if (pTree->cb.isForce) {
        assert(pTree->cb.inProgress);
        pTree->cb.inProgress = 0;
        return;
    }

    /* If the HTML_DAMAGE flag is set, repaint one or more window regions. */
    assert(pTree->cb.pDamage == 0 || pTree->cb.flags & HTML_DAMAGE);
    if (pTree->cb.flags & HTML_DAMAGE) {
        HtmlDamage *pD = pTree->cb.pDamage;
        if (pD && (
            (pTree->cb.flags & HTML_SCROLL)==0 ||
            pD->x != 0 || pD->y != 0 || 
            pD->w < Tk_Width(pTree->tkwin) ||
            pD->h < Tk_Height(pTree->tkwin)
        )) {
            pTree->cb.pDamage = 0;
            while (pD) {
                HtmlDamage *pNext = pD->pNext;
                HtmlLog(pTree, 
                    "ACTION", "Repair: %dx%d +%d+%d", 
                    pD->w, pD->h, pD->x, pD->y
                );
                HtmlWidgetRepair(pTree, pD->x, pD->y, pD->w, pD->h, 1);
                HtmlFree(pD);
                pD = pNext;
            }
        }
    }

    /* If the HTML_SCROLL flag is set, scroll the viewport. */
    if (pTree->cb.flags & HTML_SCROLL) {
        clock_t scrollClock = 0;              
        HtmlLog(pTree, "ACTION", "SetViewport: x=%d y=%d force=%d isFixed=%d", 
            p->iScrollX, p->iScrollY, force_redraw, pTree->isFixed
        );
        scrollClock = clock();
        HtmlWidgetSetViewport(pTree, p->iScrollX, p->iScrollY, 0);
        scrollClock = clock() - scrollClock;
        HtmlLog(pTree, "TIMING", "SetViewport: clicks=%d", scrollClock);
    }

    if (pTree->cb.flags & (HTML_SCROLL)) {
        doScrollCallback(pTree);
    }

    pTree->cb.flags = 0;
    assert(pTree->cb.inProgress);
    pTree->cb.inProgress = 0;

    if (pTree->cb.pDamage) {
        pTree->cb.flags = HTML_DAMAGE;
        Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
    }

    offscreen = MAX(0, 
        MIN(pTree->canvas.bottom - Tk_Height(pTree->tkwin), pTree->iScrollY)
    );
    if (offscreen != pTree->iScrollY) {
        HtmlCallbackScrollY(pTree, offscreen);
    }
    offscreen = MAX(0, 
        MIN(pTree->canvas.right - Tk_Width(pTree->tkwin), pTree->iScrollX)
    );
    if (offscreen != pTree->iScrollX) {
        HtmlCallbackScrollX(pTree, offscreen);
    }
}

static void
delayCallbackHandler(clientData)
    ClientData clientData;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    pTree->delayToken = 0;
    if (pTree->cb.flags) {
        callbackHandler(clientData);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackForce --
 *
 *     If there is a callback scheduled, execute it now instead of waiting 
 *     for the idle loop.
 *
 *     An exception: If the only reason for the callback is widget damage
 *     (a repaint-area callback), don't run it.
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
HtmlCallbackForce(pTree)
    HtmlTree *pTree;
{
    if (
        (pTree->cb.flags & ~(HTML_DAMAGE|HTML_SCROLL|HTML_NODESCROLL)) && 
        (!pTree->cb.inProgress) 
    ) {
        ClientData clientData = (ClientData)pTree;
        assert(!pTree->cb.isForce);
        pTree->cb.isForce++;
        callbackHandler(clientData);
        pTree->cb.isForce--;
        assert(pTree->cb.isForce >= 0);
        if (pTree->cb.flags == 0) {
            Tcl_CancelIdleCall(callbackHandler, clientData);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * upgradeRestylePoint --
 *
 * Results:
 *     True if argument pNode is located in the document tree, or false
 *     if it is located in some orphan tree.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int
upgradeRestylePoint(ppRestyle, pNode)
    HtmlNode **ppRestyle;
    HtmlNode *pNode;
{
    HtmlNode *pA;
    HtmlNode *pB;
    assert(pNode && ppRestyle);

    /* Do nothing if pNode is part of an orphan tree */
    for (pA = pNode; pA; pA = HtmlNodeParent(pA)) {
        if (pA->iNode == HTML_NODE_ORPHAN) return 0;
    }

    for (pA = *ppRestyle; pA; pA = HtmlNodeParent(pA)) {
        HtmlNode *pParentA = HtmlNodeParent(pA);
        for (pB = pNode; pB; pB = HtmlNodeParent(pB)) {
            if (pB == pA) {
                *ppRestyle = pB;
                return 1;
            }  
            if (HtmlNodeParent(pB) == pParentA) {
                int i;
                for (i = 0; i < HtmlNodeNumChildren(pParentA); i++) {
                    HtmlNode *pChild = HtmlNodeChild(pParentA, i);
                    if (pChild == pB || pChild == pA) {
                        *ppRestyle = pChild;
                        return 1;
                    }
                }
                assert(!"Cannot happen");
            }
        }
    }

    assert(!*ppRestyle);
    *ppRestyle = pNode;
    return 1;
}

static void
snapshotLayout(pTree)
    HtmlTree *pTree;
{
    if (pTree->cb.pSnapshot == 0) {
        pTree->cb.pSnapshot = HtmlDrawSnapshot(pTree, 0);
    }
}
static void
snapshotZero(pTree)
    HtmlTree *pTree;
{
    HtmlNodeReplacement *p;
    HtmlDrawSnapshotFree(pTree, pTree->cb.pSnapshot);
    pTree->cb.pSnapshot = HtmlDrawSnapshotZero(pTree);
    for (p = pTree->pMapped; p ; p = p->pNext) {
        p->iCanvasX = -10000;
        p->iCanvasY = -10000;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackRestyle --
 *
 *     Next widget idle-callback, recalculate style information for the
 *     sub-tree rooted at pNode. This function is a no-op if (pNode==0).
 *     If pNode is the root of the document, then the list of dynamic
 *     conditions (HtmlNode.pDynamic) that apply to each node is also
 *     recalculated.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May modify HtmlTree.cb and/or register for an idle callback with
 *     the Tcl event loop.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlCallbackRestyle(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pNode) {
        snapshotLayout(pTree);
        if (upgradeRestylePoint(&pTree->cb.pRestyle, pNode)) {
            if (!pTree->cb.flags) {
                Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
            }
            pTree->cb.flags |= HTML_RESTYLE;
            assert(pTree->cb.pSnapshot);
        }
    }

    /* This is also where the text-representation of the document is
     * invalidated. If the style of a node is to change, or a new node
     * that has no style is added, then the current text-representation
     * is clearly suspect.
     */
    HtmlTextInvalidate(pTree);
    HtmlCssSearchInvalidateCache(pTree);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackDynamic --
 *
 *     Next widget idle-callback, check if any dynamic CSS conditions
 *     attached to nodes that are part of the sub-tree rooted at pNode
 *     have changed. If so, restyle the affected nodes.  This function
 *     is a no-op if (pNode==0).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May modify HtmlTree.cb and/or register for an idle callback with
 *     the Tcl event loop.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlCallbackDynamic(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pNode) {
        if (upgradeRestylePoint(&pTree->cb.pDynamic, pNode)) {
            if (!pTree->cb.flags) {
                Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
            }
            pTree->cb.flags |= HTML_DYNAMIC;
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackLayout --
 *
 *     Ensure the layout of node pNode is recalculated next idle
 *     callback. This is a no-op if (pNode==0).
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     May modify HtmlTree.cb and/or register for an idle callback with
 *     the Tcl event loop. May expire layout-caches belonging to pNode
 *     and it's ancestor nodes.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlCallbackLayout(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pNode) {
        HtmlNode *p;
        snapshotLayout(pTree);
        if (!pTree->cb.flags) {
            Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
        }
        pTree->cb.flags |= HTML_LAYOUT;
        assert(pTree->cb.pSnapshot);
        for (p = pNode; p; p = HtmlNodeParent(p)) {
            HtmlLayoutInvalidateCache(pTree, p);
        }

        pTree->isBboxOk = 0;
    }
}

static int setSnapshotId(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    pNode->iSnapshot = pTree->iLastSnapshotId;
    return HTML_WALK_DESCEND;
}

void 
HtmlCallbackDamageNode(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    if (pTree->cb.pSnapshot) {
        if (pNode->iSnapshot != pTree->iLastSnapshotId){
            HtmlWalkTree(pTree, pNode, setSnapshotId, 0);
        }
    } else {
        int x, y, w, h;
        HtmlWidgetNodeBox(pTree, pNode, &x, &y, &w, &h);
        HtmlCallbackDamage(pTree, x-pTree->iScrollX, y-pTree->iScrollY, w, h);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCallbackDamage --
 *
 *     Schedule a region to be repainted during the next callback.
 *     The x and y arguments are relative to the viewport, not the
 *     document origin.
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
HtmlCallbackDamage(pTree, x, y, w, h)
    HtmlTree *pTree;
    int x; 
    int y;
    int w; 
    int h;
{
    HtmlDamage *pNew;
    HtmlDamage *p;

    /* Clip the values to the viewport */
    if (x < 0) {w += x; x = 0;}
    if (y < 0) {h += y; y = 0;}
    w = MIN(w, Tk_Width(pTree->tkwin) - x);
    h = MIN(h, Tk_Height(pTree->tkwin) - y);
    
    /* If the damaged region is not currently visible, do nothing */
    if (w <= 0 || h <= 0) {
        return;
    }

    /* Loop through the current list of damaged rectangles. If possible
     * clip the new damaged region so that the same part of the display
     * is not painted more than once.
     */
    for (p = pTree->cb.pDamage; p; p = p->pNext) {
        /* Check if region p completely encapsulates the new region. If so,
         * we need do nothing. 
         */
        assert(pTree->cb.flags & HTML_DAMAGE);
        if (
            p->x <= x && p->y <= y && 
            (p->x + p->w) >= (x + w) && (p->y + p->h) >= (y + h)
        ) {
            return;
        }
    }

#if 0
    if (pTree->cb.flags & HTML_DAMAGE) {
        int x2 = MAX(x + w, pTree->cb.x + pTree->cb.w);
        int y2 = MAX(y + h, pTree->cb.y + pTree->cb.h);
        pTree->cb.x = MIN(pTree->cb.x, x);
        pTree->cb.y = MIN(pTree->cb.y, y);
        pTree->cb.w = x2 - pTree->cb.x;
        pTree->cb.h = y2 - pTree->cb.y;
    } else {
        pTree->cb.x = x;
        pTree->cb.y = y;
        pTree->cb.w = w;
        pTree->cb.h = h;
    }

    assert(pTree->cb.x >= 0);
    assert(pTree->cb.y >= 0);
    assert(pTree->cb.w > 0);
    assert(pTree->cb.h > 0);
#endif
 
    pNew = HtmlNew(HtmlDamage);
    pNew->x = x;
    pNew->y = y;
    pNew->w = w;
    pNew->h = h;
    pNew->pNext = pTree->cb.pDamage;
    pTree->cb.pDamage = pNew;

    if (!pTree->cb.flags) {
        Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
    }
    pTree->cb.flags |= HTML_DAMAGE;
}

void 
HtmlCallbackScrollY(pTree, y)
    HtmlTree *pTree;
    int y; 
{
    if (!pTree->cb.flags) {
        Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
    }
    pTree->cb.flags |= HTML_SCROLL;
    pTree->cb.iScrollY = y;
}

void 
HtmlCallbackScrollX(pTree, x)
    HtmlTree *pTree;
    int x; 
{
    if (!pTree->cb.flags) {
        Tcl_DoWhenIdle(callbackHandler, (ClientData)pTree);
    }
    pTree->cb.flags |= HTML_SCROLL;
    pTree->cb.iScrollX = x;
}

/*
 *---------------------------------------------------------------------------
 *
 * cleanupHandlerTable --
 *
 *      This function is called to delete the contents of one of the
 *      HtmlTree.aScriptHandler, aNodeHandler or aParseHandler tables.
 *      It is called as the tree is being deleted.
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
cleanupHandlerTable(pHash)
    Tcl_HashTable *pHash;
{
    Tcl_HashEntry *pEntry;
    Tcl_HashSearch search;

    for (
        pEntry = Tcl_FirstHashEntry(pHash, &search); 
        pEntry; 
        pEntry = Tcl_NextHashEntry(&search)
    ) {
        Tcl_DecrRefCount((Tcl_Obj *)Tcl_GetHashValue(pEntry));
    }
    Tcl_DeleteHashTable(pHash);
}

/*
 *---------------------------------------------------------------------------
 *
 * deleteWidget --
 *
 *     destroy $html
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
deleteWidget(clientData)
    ClientData clientData;
{
    HtmlDamage *pDamage;
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlTreeClear(pTree);

    /* Delete the contents of the three "handler" hash tables */
    cleanupHandlerTable(&pTree->aNodeHandler);
    cleanupHandlerTable(&pTree->aAttributeHandler);
    cleanupHandlerTable(&pTree->aParseHandler);
    cleanupHandlerTable(&pTree->aScriptHandler);

    /* Clear any widget tags */
    HtmlTagCleanupTree(pTree);

    /* Clear the remaining colors etc. from the styler code hash tables */
    HtmlComputedValuesCleanupTables(pTree);

    /* Delete the image-server */
    HtmlImageServerDoGC(pTree);
    HtmlImageServerShutdown(pTree);

    /* Delete the search cache. */
    HtmlCssSearchShutdown(pTree);

    /* Cancel any pending idle callback */
    Tcl_CancelIdleCall(callbackHandler, (ClientData)pTree);
    if (pTree->delayToken) {
        Tcl_DeleteTimerHandler(pTree->delayToken);
    }
    pTree->delayToken = 0;
    while ((pDamage = pTree->cb.pDamage)) {
        pTree->cb.pDamage = pDamage->pNext;
        HtmlFree(pDamage);
    }

    /* Atoms table */
    Tcl_DeleteHashTable(&pTree->aAtom);

    /* Delete the structure itself */
    HtmlFree(pTree);
}

/*
 *---------------------------------------------------------------------------
 *
 * widgetCmdDel --
 *
 *     This command is invoked by Tcl when a widget object command is
 *     deleted. This can happen under two circumstances:
 *
 *         * A script deleted the command. In this case we should delete
 *           the widget window (and everything else via the window event
 *           callback) as well.
 *         * A script deleted the widget window. In this case we need
 *           do nothing.
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
widgetCmdDel(clientData)
    ClientData clientData;
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    if( !pTree->isDeleted ){
      pTree->cmd = 0;
      Tk_DestroyWindow(pTree->tkwin);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * eventHandler --
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
eventHandler(clientData, pEvent)
    ClientData clientData;
    XEvent *pEvent;
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    switch (pEvent->type) {
        case ConfigureNotify: {
            /* XConfigureEvent *p = (XConfigureEvent*)pEvent; */
            int iWidth = Tk_Width(pTree->tkwin);
            int iHeight = Tk_Height(pTree->tkwin);
            HtmlLog(pTree, "EVENT", "ConfigureNotify: width=%dpx", iWidth);
            if (
                iWidth != pTree->iCanvasWidth || 
                iHeight != pTree->iCanvasHeight
            ) {
                HtmlCallbackLayout(pTree, pTree->pRoot);
                snapshotZero(pTree);
                HtmlCallbackDamage(pTree, 0, 0, iWidth, iHeight);
            }
            break;
        }

        case UnmapNotify: {
            break;
        }

        case DestroyNotify: {
            pTree->isDeleted = 1;
            Tcl_DeleteCommandFromToken(pTree->interp, pTree->cmd);
            deleteWidget(pTree);
            break;
        }

    }
}

static void 
docwinEventHandler(clientData, pEvent)
    ClientData clientData;
    XEvent *pEvent;
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    switch (pEvent->type) {
        case Expose: {
            XExposeEvent *p = (XExposeEvent *)pEvent;
    
            HtmlLog(pTree, "EVENT", 
                "Docwin Expose: x=%d y=%d width=%d height=%d",
                p->x, p->y, p->width, p->height
            );
    
            HtmlCallbackDamage(pTree, 
                p->x + Tk_X(pTree->docwin), p->y + Tk_Y(pTree->docwin),
                p->width, p->height
            );
            break;
        }

        case ButtonPress:
        case ButtonRelease:
        case MotionNotify:
        case LeaveNotify:
        case EnterNotify:
            /* I am not a bad person. But sometimes people make me do
             * bad things. This here is a bad thing. The idea is to
             * have mouse related events delivered to the html widget
             * window, not this document window. Then, before returning,
             * mess up the internals of the event so that the processing
             * in Tk_BindEvent() ignores it.
             */
            pEvent->xmotion.window = Tk_WindowId(pTree->tkwin);
            pEvent->xmotion.x += Tk_X(pTree->docwin);
            pEvent->xmotion.y += Tk_Y(pTree->docwin);
            tkIntStubsPtr->tkBindEventProc(pTree->tkwin, pEvent);
            pEvent->type = EnterNotify;
            pEvent->xcrossing.detail = NotifyInferior;
            break;
    }
}

static int 
relayoutCb(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    HtmlCallbackLayout(pTree, pNode);
    return HTML_WALK_DESCEND;
}

static int 
worldChangedCb(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    if (!HtmlNodeIsText(pNode)) {
        HtmlElementNode *pElem = (HtmlElementNode *)pNode;
        HtmlLayoutInvalidateCache(pTree, pNode);
        HtmlNodeClearStyle(pTree, pElem);
        HtmlDrawCanvasItemRelease(pTree, pElem->pBox);
        pElem->pBox = 0;
        pTree->isBboxOk = 0;
    }
    return HTML_WALK_DESCEND;
}

void HtmlNodeClearRecursive(pTree, pNode)
    HtmlTree *pTree;
    HtmlNode *pNode;
{
    HtmlWalkTree(pTree, pNode, worldChangedCb, 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * configureCmd --
 *
 *     Implementation of the standard Tk "configure" command.
 *
 *         <widget> configure -OPTION VALUE ?-OPTION VALUE? ...
 *
 *     TODO: Handle configure of the forms:
 *         <widget> configure -OPTION 
 *         <widget> configure
 *
 * Results:
 *     Standard tcl result.
 *
 * Side effects:
 *     May set values of HtmlTree.options struct. May call
 *     Tk_GeometryRequest().
 *
 *---------------------------------------------------------------------------
 */
static int 
configureCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    static const char *azModes[] = {"quirks","almost standards","standards",0};
    static const char *azParseModes[] = {"html","xhtml","xml",0};

    /*
     * Mask bits for options declared in htmlOptionSpec.
     */
    #define GEOMETRY_MASK  0x00000001
    #define FT_MASK        0x00000002    
    #define DS_MASK        0x00000004    
    #define S_MASK         0x00000008    
    #define F_MASK         0x00000010   
    #define L_MASK         0x00000020   

    /*
     * Macros to generate static Tk_OptionSpec structures for the
     * htmlOptionSpec() array.
     */
    #define PIXELS(v, s1, s2, s3) \
        {TK_OPTION_PIXELS, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, 0}
    #define GEOMETRY(v, s1, s2, s3) \
        {TK_OPTION_PIXELS, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, GEOMETRY_MASK}
    #define STRING(v, s1, s2, s3) \
        {TK_OPTION_STRING, "-" #v, s1, s2, s3, \
         Tk_Offset(HtmlOptions, v), -1, TK_OPTION_NULL_OK, 0, 0}
    #define STRINGT(v, s1, s2, s3, t) \
        {TK_OPTION_STRING_TABLE, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, (ClientData)t, 0}
    #define BOOLEAN(v, s1, s2, s3, flags) \
        {TK_OPTION_BOOLEAN, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, flags}
    #define OBJ(v, s1, s2, s3, f) \
        {TK_OPTION_STRING, "-" #v, s1, s2, s3, \
         Tk_Offset(HtmlOptions, v), -1, 0, 0, f}
    #define DOUBLE(v, s1, s2, s3, f) \
        {TK_OPTION_DOUBLE, "-" #v, s1, s2, s3, -1, \
         Tk_Offset(HtmlOptions, v), 0, 0, f}
    
    /* Option table definition for the html widget. */
    static Tk_OptionSpec htmlOptionSpec[] = {

/* Standard geometry and scrolling interface - same as canvas, text */
GEOMETRY(height, "height", "Height", "600"),
GEOMETRY(width, "width", "Width", "800"),
PIXELS  (yscrollincrement, "yScrollIncrement", "ScrollIncrement", "20"),
PIXELS  (xscrollincrement, "xScrollIncrement", "ScrollIncrement", "20"),
STRING  (xscrollcommand, "xScrollCommand", "ScrollCommand", ""),
STRING  (yscrollcommand, "yScrollCommand", "ScrollCommand", ""),

/* Non-debugging, non-standard options in alphabetical order. */
OBJ     (defaultstyle, "defaultStyle", "DefaultStyle", HTML_DEFAULT_CSS, 0),
DOUBLE  (fontscale, "fontScale", "FontScale", "1.0", F_MASK),
OBJ     (fonttable, "fontTable", "FontTable", "8 9 10 11 13 15 17", FT_MASK),
BOOLEAN (forcefontmetrics, "forceFontMetrics", "ForceFontMetrics", "1", F_MASK),
BOOLEAN (forcewidth, "forceWidth", "ForceWidth", "0", L_MASK),
BOOLEAN (imagecache, "imageCache", "ImageCache", "1", S_MASK),
BOOLEAN (imagepixmapify, "imagePixmapify", "ImagePixmapify", "0", 0),
STRING  (imagecmd, "imageCmd", "ImageCmd", ""),
STRINGT (mode, "mode", "Mode", "standards", azModes),
STRINGT (parsemode, "parsemode", "Parsemode", "html", azParseModes),
BOOLEAN (shrink, "shrink", "Shrink", "0", S_MASK),
DOUBLE  (zoom, "zoom", "Zoom", "1.0", F_MASK),

/* Debugging options */
BOOLEAN (enablelayout, "enableLayout", "EnableLayout", "1", S_MASK),
BOOLEAN (layoutcache, "layoutCache", "LayoutCache", "1", L_MASK),
STRING  (logcmd, "logCmd", "LogCmd", ""),
STRING  (timercmd, "timerCmd", "TimerCmd", ""),

        {TK_OPTION_END, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    #undef PIXELS
    #undef STRING
    #undef BOOLEAN

    HtmlTree *pTree = (HtmlTree *)clientData;
    char *pOptions = (char *)&pTree->options;
    Tk_Window win = pTree->tkwin;
    Tk_OptionTable otab = pTree->optionTable;
    Tk_SavedOptions saved;

    int mask = 0;
    int init = 0;                /* True if Tk_InitOptions() is called */
    int rc;

    if (!otab) {
        pTree->optionTable = Tk_CreateOptionTable(interp, htmlOptionSpec);
        Tk_InitOptions(interp, pOptions, pTree->optionTable, win);
        init = 1;
        otab = pTree->optionTable;
    }

    rc = Tk_SetOptions(
        interp, pOptions, otab, objc-2, &objv[2], win, (init?0:&saved), &mask
    );
    if (TCL_OK == rc) {
        /* Hard-coded minimum values for width and height */
        pTree->options.height = MAX(pTree->options.height, 0);
        pTree->options.width = MAX(pTree->options.width, 0);

        if (init || (mask & GEOMETRY_MASK)) {
            int w = pTree->options.width;
            int h = pTree->options.height;
            Tk_GeometryRequest(pTree->tkwin, w, h);
        }
    
        if (init || mask & FT_MASK) {
            int nSize;
            Tcl_Obj **apSize;
            int aFontSize[7];
            Tcl_Obj *pFT = pTree->options.fonttable;
            if (
                Tcl_ListObjGetElements(interp, pFT, &nSize, &apSize) ||
                nSize != 7 ||
                Tcl_GetIntFromObj(interp, apSize[0], &aFontSize[0]) ||
                Tcl_GetIntFromObj(interp, apSize[1], &aFontSize[1]) ||
                Tcl_GetIntFromObj(interp, apSize[2], &aFontSize[2]) ||
                Tcl_GetIntFromObj(interp, apSize[3], &aFontSize[3]) ||
                Tcl_GetIntFromObj(interp, apSize[4], &aFontSize[4]) ||
                Tcl_GetIntFromObj(interp, apSize[5], &aFontSize[5]) ||
                Tcl_GetIntFromObj(interp, apSize[6], &aFontSize[6])
            ) {
                Tcl_ResetResult(interp);
                Tcl_AppendResult(interp, 
                    "expected list of 7 integers but got ", 
                    "\"", Tcl_GetString(pFT), "\"", 0
                );
                rc = TCL_ERROR;
            } else {
                memcpy(pTree->aFontSizeTable, aFontSize, sizeof(aFontSize));
                mask |= S_MASK;
                HtmlComputedValuesFreePrototype(pTree);
            }
        }

        if (mask & (S_MASK|F_MASK)) {
            HtmlImageServerSuspendGC(pTree);
            HtmlDrawCleanup(pTree, &pTree->canvas);
            HtmlDrawSnapshotFree(pTree, pTree->cb.pSnapshot);
            pTree->cb.pSnapshot = 0;
            HtmlCallbackRestyle(pTree, pTree->pRoot);
            HtmlWalkTree(pTree, pTree->pRoot, worldChangedCb, 0);
            HtmlCallbackDamage(pTree, 0, 0, Tk_Width(win), Tk_Height(win));

#ifndef NDEBUG
            if (1) {
                Tcl_HashSearch search;
                assert(0 == Tcl_FirstHashEntry(&pTree->aValues, &search));
            }
#endif
        }
        if (mask & F_MASK) {
            /* This happens when one of the font related options changes.
             * The key here is that the font cache (HtmlTree.aFont) must
             * be completely empty before we rerun the styler. Otherwise
             * fonts returned by the cache will match the old -fontscale
             * and -fonttable options.
             */
            HtmlFontCacheClear(pTree, 1);
        }
        if (mask & L_MASK) {
            /* This happens when the -forcewidth option is set. In this
             * case we need to rebuild the layout.
             */
            HtmlCallbackLayout(pTree, pTree->pRoot);
        }

        if (rc != TCL_OK) {
            assert(!init);
            Tk_RestoreSavedOptions(&saved);
        } else if (!init) {
            Tk_FreeSavedOptions(&saved);
        }
    }

    return rc;
    #undef GEOMETRY_MASK
    #undef FT_MASK
}

/*
 *---------------------------------------------------------------------------
 *
 * cgetCmd --
 *
 *     Standard Tk "cget" command for querying options.
 *
 *     <widget> cget -OPTION
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int cgetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tcl_Obj *pRet;
    Tk_OptionTable otab = pTree->optionTable;
    Tk_Window win = pTree->tkwin;
    char *pOptions = (char *)&pTree->options;

    assert(otab);

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "-OPTION");
        return TCL_ERROR;
    }

    pRet = Tk_GetOptionValue(interp, pOptions, otab, objv[2], win);
    if( pRet ) {
        Tcl_SetObjResult(interp, pRet);
    } else {
        char * zOpt = Tcl_GetString(objv[2]);
        Tcl_AppendResult( interp, "unknown option \"", zOpt, "\"", 0);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * resetCmd --
 * 
 *         widget reset
 *
 *     Reset the widget so that no document or stylesheet is loaded.
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
resetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tk_Window win = pTree->tkwin;

    HtmlTreeClear(pTree);

    HtmlImageServerDoGC(pTree);
    if (pTree->options.imagecache) {
        HtmlImageServerSuspendGC(pTree);
    }
    assert(HtmlImageServerCount(pTree) == 0);

    HtmlCallbackScrollY(pTree, 0);
    HtmlCallbackScrollX(pTree, 0);
    HtmlCallbackDamage(pTree, 0, 0, Tk_Width(win), Tk_Height(win));
    doLoadDefaultStyle(pTree);
    pTree->isParseFinished = 0;
    pTree->isSequenceOk = 1;
    if (pTree->eWriteState == HTML_WRITE_WAIT || 
        pTree->eWriteState == HTML_WRITE_NONE
    ) {
        pTree->eWriteState = HTML_WRITE_NONE;
    } else {
        pTree->eWriteState = HTML_WRITE_INHANDLERRESET;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * relayoutCmd --
 * 
 *         $html relayout ?-layout|-style? ?NODE?
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
static int 
relayoutCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    if (objc == 2) {
        HtmlCallbackRestyle(pTree, pTree->pRoot);
        HtmlWalkTree(pTree, pTree->pRoot, relayoutCb, 0);
    } else {
        char *zArg3 = ((objc >= 3) ? Tcl_GetString(objv[2]) : 0);
        char *zArg4 = ((objc >= 4) ? Tcl_GetString(objv[3]) : 0);
        HtmlNode *pNode;

        pNode = HtmlNodeGetPointer(pTree, zArg4 ? zArg4 : zArg3);
        if (!zArg4) {
            HtmlCallbackRestyle(pTree, pNode);
            HtmlCallbackLayout(pTree, pNode);
        } else if (0 == strcmp(zArg3, "-layout")) {
            HtmlCallbackLayout(pTree, pNode);
        } else if (0 == strcmp(zArg3, "-style")) {
            HtmlCallbackRestyle(pTree, pNode);
        } else {
            Tcl_AppendResult(interp, 
                "Bad option \"", zArg3, "\": must be -layout or -style", 0
            );
            return TCL_ERROR;
        }
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * parseCmd --
 *
 *         $widget parse ?-final? HTML-TEXT
 * 
 *     Appends the given HTML text to the end of any HTML text that may have
 *     been inserted by prior calls to this command. See Tkhtml man page for
 *     further details.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
parseCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    int isFinal;
    char *zHtml;
    int nHtml;
    int eWriteState;

    Tcl_Obj *aObj[2];
    SwprocConf aConf[3] = {
        {SWPROC_SWITCH, "final", "0", "1"},   /* -final */
        {SWPROC_ARG, 0, 0, 0},                /* HTML-TEXT */
        {SWPROC_END, 0, 0, 0}
    };

    if (
        SwprocRt(interp, (objc - 2), &objv[2], aConf, aObj) ||
        Tcl_GetBooleanFromObj(interp, aObj[0], &isFinal)
    ) {
        return TCL_ERROR;
    }

    /* zHtml = Tcl_GetByteArrayFromObj(aObj[1], &nHtml); */
    zHtml = Tcl_GetStringFromObj(aObj[1], &nHtml);

    assert(Tcl_IsShared(aObj[1]));
    Tcl_DecrRefCount(aObj[0]);
    Tcl_DecrRefCount(aObj[1]);

    if (pTree->isParseFinished) {
        const char *zWidget = Tcl_GetString(objv[0]);
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, 
            "Cannot call [", zWidget, " parse]" 
            "until after [", zWidget, "] reset", 0
        );
        return TCL_ERROR;
    }

    /* Add the new text to the internal cache of the document. */
    eWriteState = pTree->eWriteState;
    HtmlTokenizerAppend(pTree, zHtml, nHtml, isFinal);
    assert(eWriteState == HTML_WRITE_NONE || pTree->eWriteState == eWriteState);

    if (
        eWriteState != HTML_WRITE_INHANDLERRESET && 
        pTree->eWriteState == HTML_WRITE_INHANDLERRESET
    ) {
        /* This case occurs when a node or script handler callback invokes
         * the [reset] method on this widget. The script-handler may then
         * go on to call [parse], which is the tricky bit...
         */
        int nCount = 0;
        while (pTree->eWriteState == HTML_WRITE_INHANDLERRESET && nCount<100) {
            assert(pTree->nParsed == 0);
            pTree->eWriteState = HTML_WRITE_NONE;
            if (pTree->pDocument) {
                HtmlTokenizerAppend(pTree, "", 0, pTree->isParseFinished);
            }
            nCount++;
        }
        if (nCount==100){
            Tcl_ResetResult(interp);
            Tcl_AppendResult(interp, "infinite loop: "
                "caused by node-handler calling [reset], [parse].", 0
            );
            return TCL_ERROR;
        }
        isFinal = pTree->isParseFinished;
    }

    if (isFinal) {
        HtmlInitTree(pTree);
        pTree->isParseFinished = 1;
        if (pTree->eWriteState == HTML_WRITE_NONE) {
            HtmlFinishNodeHandlers(pTree);
        }
    }
 
    HtmlCheckRestylePoint(pTree);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * preloadCmd --
 *
 *         $widget preload URI
 * 
 *     Preload the image located at the specified URI.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
preloadCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    HtmlImage2 *pImg2 = 0;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "URI");
        return TCL_ERROR;
    }

    pImg2 = HtmlImageServerGet(pTree->pImageServer, Tcl_GetString(objv[2]));
    HtmlImageFree(pImg2);

    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * fragmentCmd --
 *
 *         $widget fragment HTML-TEXT
 *
 *     Parse the supplied markup test and return a list of node handles.
 * 
 * Results:
 *     List of node-handles.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
fragmentCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "HTML-TEXT");
        return TCL_ERROR;
    }
    HtmlParseFragment(pTree, Tcl_GetString(objv[2]));
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * viewCommon --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
viewCommon(pTree, isXview, objc, objv)
    HtmlTree *pTree;
    int isXview;               /* True for [xview], zero for [yview] */
    int objc;
    Tcl_Obj * CONST objv[];
{
    Tcl_Interp *interp = pTree->interp;
    Tk_Window win = pTree->tkwin;

    int iUnitPixels;           /* Value of -[xy]scrollincrement in pixels */
    int iPagePixels;           /* Width or height of the viewport */
    int iMovePixels;           /* Width or height of canvas */
    int iOffScreen;            /* Current scroll position */
    double aRet[2];
    Tcl_Obj *pRet;
    Tcl_Obj *pScrollCommand;

    if (isXview) { 
        iPagePixels = Tk_Width(win);
        iUnitPixels = pTree->options.xscrollincrement;
        iMovePixels = pTree->canvas.right;
        iOffScreen = pTree->iScrollX;
        pScrollCommand = pTree->options.xscrollcommand;
    } else {
        iPagePixels = Tk_Height(win);
        iUnitPixels = pTree->options.yscrollincrement;
        iMovePixels = pTree->canvas.bottom;
        iOffScreen = pTree->iScrollY;
        pScrollCommand = pTree->options.yscrollcommand;
    }

    if (objc > 2) {
        double fraction;
        int count;
        int iNewVal = 0;     /* New value of iScrollY or iScrollX */

        HtmlCallbackForce(pTree);

        /* The [widget yview] command also supports "scroll-to-node" */
        if (!isXview && objc == 3) {
            const char *zCmd = Tcl_GetString(objv[2]);
            HtmlNode *pNode = HtmlNodeGetPointer(pTree, zCmd);
            if (!pNode) {
                return TCL_ERROR;
            }
            iNewVal = HtmlWidgetNodeTop(pTree, pNode);
            iMovePixels = pTree->canvas.bottom;
        } else {
            int eType;       /* One of the TK_SCROLL_ symbols */
            eType = Tk_GetScrollInfoObj(interp, objc, objv, &fraction, &count);
            switch (eType) {
                case TK_SCROLL_MOVETO:
                    iNewVal = (int)((double)iMovePixels * fraction);
                    break;
                case TK_SCROLL_PAGES:
                    iNewVal = iOffScreen + ((count * iPagePixels) * 0.9);
                    break;
                case TK_SCROLL_UNITS:
                    iNewVal = iOffScreen + (count * iUnitPixels);
                    break;
                case TK_SCROLL_ERROR:
                    return TCL_ERROR;
        
                default: assert(!"Not possible");
            }
        }

        /* Clip the new scrolling value for the window size */
        iNewVal = MIN(iNewVal, iMovePixels - iPagePixels);
        iNewVal = MAX(iNewVal, 0);

        if (isXview) { 
            HtmlCallbackScrollX(pTree, iNewVal);
        } else {
            HtmlCallbackScrollY(pTree, iNewVal);
        }
    }

    /* Construct the Tcl result for this command. */
    if (iMovePixels <= iPagePixels) {
        aRet[0] = 0.0;
        aRet[1] = 1.0;
    } else {
        assert(iMovePixels > 0);
        assert(iOffScreen  >= 0);
        assert(iPagePixels >= 0);
        aRet[0] = (double)iOffScreen / (double)iMovePixels;
        aRet[1] = (double)(iOffScreen + iPagePixels) / (double)iMovePixels;
        aRet[1] = MIN(aRet[1], 1.0);
    }
    pRet = Tcl_NewObj();
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewDoubleObj(aRet[0]));
    Tcl_ListObjAppendElement(interp, pRet, Tcl_NewDoubleObj(aRet[1]));
    Tcl_SetObjResult(interp, pRet);

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * xviewCmd --
 * yviewCmd --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
xviewCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    return viewCommon((HtmlTree *)clientData, 1, objc, objv);
}
static int 
yviewCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *const *objv;              /* List of all arguments */
{
    return viewCommon((HtmlTree *)clientData, 0, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * writeCmd --
 *
 *     $widget write wait
 *     $widget write text TEXT
 *     $widget write continue
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
writeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    int eChoice;
    int rc = TCL_ERROR;

    enum SubOptType {
        OPT_WAIT, OPT_TEXT, OPT_CONTINUE
    };
    struct SubOpt {
        char *zSubOption;             /* Name of sub-command */
        enum SubOptType eType;        /* Corresponding OPT_XXX value */
        int iExtraArgs;               /* Number of args following sub-command */
        char *zWrongNumArgsTail;      /* 4th arg to Tcl_WrongNumArgs() */
    } aSub[] = {
        {"wait", OPT_WAIT, 0, ""}, 
        {"text", OPT_TEXT, 1, "TEXT"}, 
        {"continue", OPT_CONTINUE, 0, ""}, 
        {0, 0, 0}
    };

    /* All commands must consist of at least three words - the widget name,
     * "write", and the sub-command name. Otherwise, it's a Tcl error.
     */
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "OPTION");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObjStruct(
        interp, objv[2], aSub, sizeof(struct SubOpt), "option", 0, &eChoice) 
    ){
        return TCL_ERROR;
    }
    if ((objc - 3) != aSub[eChoice].iExtraArgs) {
        Tcl_WrongNumArgs(interp, 3, objv, aSub[eChoice].zWrongNumArgsTail);
        return TCL_ERROR;
    }

    assert(pTree->interp == interp);
    switch (aSub[eChoice].eType) {
        case OPT_WAIT:
            rc = HtmlWriteWait(pTree);
            break;
        case OPT_TEXT:
            rc = HtmlWriteText(pTree, objv[3]);
            break;
        case OPT_CONTINUE:
            rc = HtmlWriteContinue(pTree);
            break;
        default:
            assert(!"Cannot happen");
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * handlerCmd --
 *
 *     $widget handler [node|attribute|script|parse] TAG SCRIPT
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
handlerCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int tag;
    Tcl_Obj *pScript;
    Tcl_HashEntry *pEntry;
    Tcl_HashTable *pHash = 0;
    int newentry;
    HtmlTree *pTree = (HtmlTree *)clientData;
    char *zTag;

    enum HandlerType {
      HANDLER_ATTRIBUTE,
      HANDLER_NODE,
      HANDLER_SCRIPT,
      HANDLER_PARSE
    };

    static const struct HandlerSubCommand {
        const char *zCommand;
        enum HandlerType eSymbol;
    } aSubCommand[] = {
        {"attribute",   HANDLER_ATTRIBUTE},
        {"node",        HANDLER_NODE},
        {"script",      HANDLER_SCRIPT},
        {"parse",       HANDLER_PARSE},
        {0, 0}
    };
    int iChoice;

    if (objc!=5) {
        Tcl_WrongNumArgs(interp, 3, objv, "TAG SCRIPT");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObjStruct(interp, objv[2], aSubCommand, 
        sizeof(struct HandlerSubCommand), "option", 0, &iChoice) 
    ){
        return TCL_ERROR;
    }

    zTag = Tcl_GetString(objv[3]);
    tag = HtmlNameToType(0, zTag);
    if (tag==Html_Unknown) {
        Tcl_AppendResult(interp, "Unknown tag type: ", zTag, 0);
        return TCL_ERROR;
    }

    switch ((enum HandlerType)iChoice) {
        case HANDLER_ATTRIBUTE:
            pHash = &pTree->aAttributeHandler;
            break;
        case HANDLER_NODE:
            pHash = &pTree->aNodeHandler;
            break;
        case HANDLER_PARSE:
            pHash = &pTree->aParseHandler;
            break;
        case HANDLER_SCRIPT:
            pHash = &pTree->aScriptHandler;
            if (0 == zTag[0]) {
                tag = Html_Text;
            } else if ('/' == zTag[0]) {
                tag = HtmlNameToType(0, &zTag[1]);
                if (tag != Html_Unknown) tag = tag * -1;
            }
            break;
    }

    assert(pHash);
    pScript = objv[4];

    if (Tcl_GetCharLength(pScript) == 0) {
        pEntry = Tcl_FindHashEntry(pHash, (char *)((size_t) tag));
        if (pEntry) {
            Tcl_DeleteHashEntry(pEntry);
        }
    } else {
        pEntry = Tcl_CreateHashEntry(pHash,(char*)((size_t) tag),&newentry);
        if (!newentry) {
            Tcl_Obj *pOld = (Tcl_Obj *)Tcl_GetHashValue(pEntry);
            Tcl_DecrRefCount(pOld);
        }
        Tcl_IncrRefCount(pScript);
        Tcl_SetHashValue(pEntry, (ClientData)pScript);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * styleCmd --
 *
 *         $widget style ?options? HTML-TEXT
 *
 *             -importcmd IMPORT-CMD
 *             -id ID
 *             -urlcmd URL-CMD
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
styleCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    SwprocConf aConf[5 + 1] = {
        {SWPROC_OPT, "id", "author", 0},      /* -id <style-sheet id> */
        {SWPROC_OPT, "importcmd", 0, 0},      /* -importcmd <cmd> */
        {SWPROC_OPT, "urlcmd", 0, 0},         /* -urlcmd <cmd> */
        {SWPROC_OPT, "errorvar", 0, 0},       /* -errorvar <varname> */
        {SWPROC_ARG, 0, 0, 0},                /* STYLE-SHEET-TEXT */
        {SWPROC_END, 0, 0, 0}
    };
    Tcl_Obj *apObj[5];
    int rc = TCL_OK;
    int n;
    HtmlTree *pTree = (HtmlTree *)clientData;

    /* First assert() that the sizes of the aConf and apObj array match. Then
     * call SwprocRt() to parse the arguments. If the parse is successful then
     * apObj[] contains the following: 
     *
     *     apObj[0] -> Value passed to -id option (or default "author")
     *     apObj[1] -> Value passed to -importcmd option (or default "")
     *     apObj[2] -> Value passed to -urlcmd option (or default "")
     *     apObj[3] -> Variable to store error log in
     *     apObj[4] -> Text of stylesheet to parse
     *
     * Pass these on to the HtmlStyleParse() command to actually parse the
     * stylesheet.
     */
    assert(sizeof(apObj)/sizeof(apObj[0])+1 == sizeof(aConf)/sizeof(aConf[0]));
    if (TCL_OK != SwprocRt(interp, objc - 2, &objv[2], aConf, apObj)) {
        return TCL_ERROR;
    }

    Tcl_GetStringFromObj(apObj[4], &n);
    if (n > 0) {
        rc = HtmlStyleParse(pTree,apObj[4],apObj[0],apObj[1],apObj[2],apObj[3]);
    } else {
        /* For a zero length stylesheet, we don't need to run the parser.
         * But we do need to set the error-log variable to an empty string
         * if one was specified. 
         */
        if (apObj[3]) {
            Tcl_ObjSetVar2(interp, apObj[3], 0, Tcl_NewObj(), 0);
        }
    }

    /* Clean up object references created by SwprocRt() */
    SwprocCleanup(apObj, sizeof(apObj)/sizeof(Tcl_Obj *));

    if (rc == TCL_OK) {
        HtmlCallbackRestyle(pTree, pTree->pRoot);
    }
    return rc;
}

static int
tagAddCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTagAddRemoveCmd(clientData, interp, objc, objv, HTML_TAG_ADD);
}
static int
tagRemoveCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTagAddRemoveCmd(clientData, interp, objc, objv, HTML_TAG_REMOVE);
}
static int
tagCfgCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTagConfigureCmd(clientData, interp, objc, objv);
}

static int
tagDeleteCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTagDeleteCmd(clientData, interp, objc, objv);
}

static int
callSubCmd(aSub, iIdx, clientData, interp, objc, objv)
    SubCmd *aSub;
    int iIdx;
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int iChoice;

    assert(objc >= iIdx);
    if (iIdx == objc) {
        Tcl_WrongNumArgs(interp, objc, objv, "SUB-COMMAND");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObjStruct(interp, 
            objv[iIdx], aSub, sizeof(SubCmd), "sub-command", 0, &iChoice)
    ){
        return TCL_ERROR;
    }
    return aSub[iChoice].xFunc(clientData, interp, objc, objv);
}

static int
tagCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    SubCmd aSub[] = {
        { "add"      , tagAddCmd }, 
        { "remove"   , tagRemoveCmd }, 
        { "configure", tagCfgCmd }, 
        { "delete"   , tagDeleteCmd }, 
        { 0, 0 }
    };
    return callSubCmd(aSub, 2, clientData, interp, objc, objv);
}

static int
textTextCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTextTextCmd(clientData, interp, objc, objv);
}
static int
textIndexCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTextIndexCmd(clientData, interp, objc, objv);
}
static int
textBboxCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTextBboxCmd(clientData, interp, objc, objv);
}
static int
textOffsetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlTextOffsetCmd(clientData, interp, objc, objv);
}
static int
textCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    SubCmd aSub[] = {
        { "text",   textTextCmd },
        { "index",  textIndexCmd },
        { "bbox",   textBboxCmd },
        { "offset", textOffsetCmd },
        { 0 , 0 }
    };
    return callSubCmd(aSub, 2, clientData, interp, objc, objv);
}


static int 
forceCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlCallbackForce((HtmlTree *)clientData);
    return TCL_OK;
}

static int 
delayCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    int iMilli;
    Tcl_TimerToken t;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "MILLI-SECONDS");
        return TCL_ERROR;
    }
    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[2], &iMilli)) {
        return TCL_ERROR;
    }

    if (pTree->delayToken) {
        Tcl_DeleteTimerHandler(pTree->delayToken);
    }
    pTree->delayToken = 0;

    if (iMilli > 0) {
        t = Tcl_CreateTimerHandler(iMilli, delayCallbackHandler, clientData);
        pTree->delayToken = t;
    } else if (pTree->cb.flags) {
        Tcl_DoWhenIdle(callbackHandler, clientData);
    }
  
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * imageCmd --
 * nodeCmd --
 * primitivesCmd --
 *
 *     New versions of gcc don't allow pointers to non-local functions to
 *     be used as constant initializers (which we need to do in the
 *     aSubcommand[] array inside widgetCmd(). So the following
 *     functions are wrappers around Tcl_ObjCmdProc functions implemented
 *     in other files.
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *     Whatever the called function does.
 *
 *---------------------------------------------------------------------------
 */
static int 
imageCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutImage(clientData, interp, objc, objv);
}
static int 
nodeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlInitTree((HtmlTree *)clientData);
    return HtmlLayoutNode(clientData, interp, objc, objv);
}
static int 
primitivesCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlLayoutPrimitives(clientData, interp, objc, objv);
}
static int 
imagesCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlImageServerReport(clientData, interp, objc, objv);
}
static int 
searchCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlCssSearch(clientData, interp, objc, objv);
}
static int 
styleconfigCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlCssStyleConfigDump(clientData, interp, objc, objv);
}
static int 
stylereportCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlCssStyleReport(clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * bboxCmd --
 *
 *     html bbox ?node-handle?
 *
 * Results:
 *     Tcl result (i.e. TCL_OK, TCL_ERROR).
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
static int 
bboxCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlWidgetBboxCmd(clientData, interp, objc, objv);
#if 0
    HtmlNode *pNode;
    int x, y, w, h;
    HtmlTree *pTree = (HtmlTree *)clientData;
    Tcl_Obj *pRet = Tcl_NewObj();

    if (objc != 2 && objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "?NODE-HANDLE?");
        return TCL_ERROR;
    }

    if (objc == 3) {
        pNode = HtmlNodeGetPointer(pTree, Tcl_GetString(objv[2]));
        if (!pNode) return TCL_ERROR;
    } else {
        pNode = pTree->pRoot;
    }

    HtmlWidgetNodeBox(pTree, pNode, &x, &y, &w, &h);
    if (w > 0 && h > 0) {
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(x));
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(y));
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(x + w));
        Tcl_ListObjAppendElement(0, pRet, Tcl_NewIntObj(y + h));
    }

    Tcl_SetObjResult(interp, pRet);
    return TCL_OK;
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * widgetCmd --
 *
 *     This is the C function invoked for a widget command.
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     Whatever the command does.
 *
 *---------------------------------------------------------------------------
 */
int widgetCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    /* The following array defines all the built-in widget commands.  This
     * function just parses the first one or two arguments and vectors control
     * to one of the command service routines defined in the following array.
     */
    SubCmd aSub[] = {
        {"bbox",         bboxCmd},
        {"cget",         cgetCmd},
        {"configure",    configureCmd},
        {"fragment",     fragmentCmd},
        {"handler",      handlerCmd},
        {"image",        imageCmd},
        {"node",         nodeCmd},
        {"parse",        parseCmd},
        {"preload",      preloadCmd},
        {"reset",        resetCmd},
        {"search",       searchCmd},
        {"style",        styleCmd},
        {"tag",          tagCmd},
        {"text",         textCmd},
        {"write",        writeCmd},
        {"xview",        xviewCmd},
        {"yview",        yviewCmd},

        /* The following are for debugging only. May change at any time.
	 * They are not included in the documentation. Just don't touch Ok? :)
         */
        {"_delay",       delayCmd},
        {"_force",       forceCmd},
        {"_images",      imagesCmd},
        {"_primitives",  primitivesCmd},
        {"_relayout",    relayoutCmd},
        {"_styleconfig", styleconfigCmd},
        {"_stylereport", stylereportCmd},
#ifndef NDEBUG
        {"_hashstats",  hashstatsCmd},
#endif
        { 0, 0}
    };
    return callSubCmd(aSub, 1, clientData, interp, objc, objv);
}



/*
 *---------------------------------------------------------------------------
 *
 * newWidget --
 *
 *     Create a new Html widget command:
 *
 *         html PATH ?<options>?
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
newWidget(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree;
    CONST char *zCmd;
    int rc;
    Tk_Window mainwin;           /* Main window of application */
    Tcl_HashKeyType *pType;

    if (objc<2) {
        Tcl_WrongNumArgs(interp, 1, objv, "WINDOW-PATH ?OPTIONS?");
        return TCL_ERROR;
    }
    
    zCmd = Tcl_GetString(objv[1]);
    pTree = HtmlNew(HtmlTree);

    /* Create the Tk window.
     */
    mainwin = Tk_MainWindow(interp);
    pTree->tkwin = Tk_CreateWindowFromPath(interp, mainwin, zCmd, NULL); 
    if (!pTree->tkwin) {
        HtmlFree(pTree);
        return TCL_ERROR;
    }
    Tk_SetClass(pTree->tkwin, "Html");

    pTree->docwin = Tk_CreateWindow(interp, pTree->tkwin, "document", NULL); 
    if (!pTree->docwin) {
        Tk_DestroyWindow(pTree->tkwin);
        HtmlFree(pTree);
        return TCL_ERROR;
    }
    Tk_MakeWindowExist(pTree->docwin);
    Tk_ResizeWindow(pTree->docwin, 30000, 30000);
    Tk_MapWindow(pTree->docwin);

    pTree->interp = interp;
    Tcl_InitHashTable(&pTree->aParseHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aScriptHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aNodeHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aAttributeHandler, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aOrphan, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&pTree->aTag, TCL_STRING_KEYS);
    pTree->cmd = Tcl_CreateObjCommand(interp,zCmd,widgetCmd,pTree,widgetCmdDel);

    pType = HtmlCaseInsenstiveHashType();
    Tcl_InitCustomHashTable(&pTree->aAtom, TCL_CUSTOM_TYPE_KEYS, pType);

    HtmlCssSearchInit(pTree);

    /* Initialise the hash tables used by styler code */
    HtmlComputedValuesSetupTables(pTree);

    /* Set up an event handler for the widget window */
    Tk_CreateEventHandler(pTree->tkwin, 
            ExposureMask|StructureNotifyMask|VisibilityChangeMask, 
            eventHandler, (ClientData)pTree
    );

    Tk_CreateEventHandler(pTree->docwin, 
        ExposureMask|ButtonMotionMask|ButtonPressMask|
        ButtonReleaseMask|PointerMotionMask|PointerMotionHintMask|
        Button1MotionMask|Button2MotionMask|Button3MotionMask|
        Button4MotionMask|Button5MotionMask|ButtonMotionMask, 
        docwinEventHandler, (ClientData)pTree
    );

    /* Create the image-server */
    HtmlImageServerInit(pTree);

    /* TODO: Handle the case where configureCmd() returns an error. */
    rc = configureCmd(pTree, interp, objc, objv);
    if (rc != TCL_OK) {
        Tk_DestroyWindow(pTree->tkwin);
        return TCL_ERROR;
    }
    assert(!pTree->options.logcmd);
    assert(!pTree->options.timercmd);

    /* Load the default style-sheet, ready for the first document. */
    doLoadDefaultStyle(pTree);
    pTree->isSequenceOk = 1;

#ifdef TKHTML_ENABLE_PROFILE
    if (1) {
        Tcl_CmdInfo cmdinfo;
        Tcl_GetCommandInfo(interp, "::tkhtml::instrument", &cmdinfo);
        pTree->pInstrumentData = cmdinfo.objClientData;
    }
#endif

    /* Return the name of the widget just created. */
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * htmlstyleCmd --
 *
 *     ::tkhtml::htmlstyle ?-quirks?
 *
 * Results:
 *     Built-in html style-sheet, including quirks if the -quirks option
 *     was specified.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
htmlstyleCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    if (objc > 1 && objc != 2 && strcmp(Tcl_GetString(objv[1]), "-quirks")) {
        Tcl_WrongNumArgs(interp, 1, objv, "?-quirks?");
        return TCL_ERROR;
    }

    Tcl_SetResult(interp, HTML_DEFAULT_CSS, TCL_STATIC);
    if (objc == 2) {
        Tcl_AppendResult(interp, HTML_DEFAULT_QUIRKS, 0);
    }

    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * htmlVersionCmd --
 *
 *     ::tkhtml::version
 *
 * Results:
 *     Returns a string containing the versions of the *.c files used
 *     to build the library
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
htmlVersionCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    if (objc > 1) {
        Tcl_WrongNumArgs(interp, 1, objv, "");
        return TCL_ERROR;
    }
    Tcl_SetResult(interp, HTML_SOURCE_FILES, TCL_STATIC);
    return TCL_OK;
}


static int 
htmlDecodeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlDecode(clientData, interp, objc, objv);
}
static int 
htmlEncodeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlEncode(clientData, interp, objc, objv);
}

static int 
htmlEscapeCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlEscapeUriComponent(clientData, interp, objc, objv);
}
static int 
htmlUriCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    return HtmlCreateUri(clientData, interp, objc, objv);
}

/*
 *---------------------------------------------------------------------------
 *
 * htmlByteOffsetCmd --
 * htmlCharOffsetCmd --
 *
 *     ::tkhtml::charoffset STRING BYTE-OFFSET
 *     ::tkhtml::byteoffset STRING CHAR-OFFSET
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static int 
htmlByteOffsetCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int iCharOffset;
    int iRet;
    char *zArg;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "STRING CHAR-OFFSET");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[2], &iCharOffset)) return TCL_ERROR;
    zArg = Tcl_GetString(objv[1]);

    iRet = (Tcl_UtfAtIndex(zArg, iCharOffset) - zArg);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(iRet));
    return TCL_OK;
}
static int 
htmlCharOffsetCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int iByteOffset;
    int iRet;
    char *zArg;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "STRING BYTE-OFFSET");
        return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[2], &iByteOffset)) return TCL_ERROR;
    zArg = Tcl_GetString(objv[1]);

    iRet = Tcl_NumUtfChars(zArg, iByteOffset);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(iRet));
    return TCL_OK;
}


/*
 * Define the DLL_EXPORT macro, which must be set to something or other in
 * order to export the Tkhtml_Init and Tkhtml_SafeInit symbols from a win32
 * DLL file. I don't entirely understand the ins and outs of this, the
 * block below was copied verbatim from another program.
 */
#if INTERFACE
#define DLL_EXPORT
#endif
#if defined(USE_TCL_STUBS) && defined(__WIN32__)
# undef DLL_EXPORT
# define DLL_EXPORT __declspec(dllexport)
#endif
#ifndef DLL_EXPORT
#define DLL_EXPORT
#endif

/*
 *---------------------------------------------------------------------------
 *
 * Tkhtml_Init --
 *
 *     Load the package into an interpreter.
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     Loads the tkhtml package into interpreter interp.
 *
 *---------------------------------------------------------------------------
 */
DLL_EXPORT int Tkhtml_Init(interp)
    Tcl_Interp *interp;
{
    int rc;

    /* Require stubs libraries version 8.4 or greater. */
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.4", 0) == 0) {
        return TCL_ERROR;
    }
    if (Tk_InitStubs(interp, "8.4", 0) == 0) {
        return TCL_ERROR;
    }
#endif

    if (0 == Tcl_PkgRequire(interp, "Tk", "8.4", 0)) {
        return TCL_ERROR;
    }
    Tcl_PkgProvide(interp, "Tkhtml", "3.0");

    Tcl_CreateObjCommand(interp, "html", newWidget, 0, 0);

    Tcl_CreateObjCommand(interp, "::tkhtml::htmlstyle",  htmlstyleCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::tkhtml::version",    htmlVersionCmd, 0, 0);

    Tcl_CreateObjCommand(interp, "::tkhtml::decode",     htmlDecodeCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::tkhtml::encode",     htmlEncodeCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::tkhtml::escape_uri", htmlEscapeCmd, 0, 0);

    Tcl_CreateObjCommand(interp, "::tkhtml::uri", htmlUriCmd, 0, 0);

    Tcl_CreateObjCommand(interp, "::tkhtml::byteoffset", htmlByteOffsetCmd,0,0);
    Tcl_CreateObjCommand(interp, "::tkhtml::charoffset", htmlCharOffsetCmd,0,0);

#ifndef NDEBUG
    Tcl_CreateObjCommand(interp, "::tkhtml::htmlalloc", allocCmd, 0, 0);
    Tcl_CreateObjCommand(interp, "::tkhtml::heapdebug", heapdebugCmd, 0, 0);
#endif

    SwprocInit(interp);
    HtmlInstrumentInit(interp);

    rc = Tcl_EvalEx(interp, HTML_DEFAULT_TCL, -1, TCL_EVAL_GLOBAL);
    assert(rc == TCL_OK);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * Tkhtml_SafeInit --
 *
 *     Load the package into a safe interpreter.
 *
 *     Note that this function has to be below the Tkhtml_Init()
 *     implementation. Otherwise the Tkhtml_Init() invocation in this
 *     function counts as an implicit declaration which causes problems for
 *     MSVC somewhere on down the line.
 *
 * Results:
 *     Tcl result.
 *
 * Side effects:
 *     Loads the tkhtml package into interpreter interp.
 *
 *---------------------------------------------------------------------------
 */
DLL_EXPORT int Tkhtml_SafeInit(interp)
    Tcl_Interp *interp;
{
    return Tkhtml_Init(interp);
}
