
#include "html.h"
#include "cssInt.h"

static const char rcsid[] = "$Id: csssearch.c,v 1.7 2007/10/27 08:37:50 hkoba Exp $";

/*-----------------------------------------------------------------------
 * 
 * Public interface to this subsystem:
 *
 *     Startup and shutdown:
 *
 *         HtmlCssSearchInit()
 *         HtmlCssSearchShutdown()
 *
 *     Query:
 *
 *         HtmlCssSearch()
 *
 *     Discard the contents of the cache:
 *
 *         HtmlCssSearchInvalidateCache()
 *
 */

#define SEARCH_MODE_ALL     1
#define SEARCH_MODE_INDEX   2
#define SEARCH_MODE_LENGTH  3

struct CssCachedSearch {
    int nAlloc;
    int nNode;
    HtmlNode **apNode;
};
typedef struct CssCachedSearch CssCachedSearch;

struct HtmlSearchCache {
    /* Map between CSS selector and search results. The contents
     * are discarded each time HtmlCssSearchInvalidateCache() is
     * called. 
     */
    Tcl_HashTable aCache;
};

struct CssSearch {
  CssRule *pRuleList;        /* The list of CSS selectors */
  HtmlTree *pTree;
  HtmlNode *pSearchRoot;     /* Root of sub-tree to search */
  CssCachedSearch *pCache;   /* Output */
};
typedef struct CssSearch CssSearch;

static int 
cssSearchCb(pTree, pNode, clientData)
    HtmlTree *pTree; 
    HtmlNode *pNode;
    ClientData clientData;
{
    CssSearch *pSearch = (CssSearch *)clientData;
    assert(pSearch->pRuleList);

    if (pNode != pSearch->pSearchRoot && 0 == HtmlNodeIsText(pNode)) {
        CssRule *p;
        for (
            p = pSearch->pRuleList; 
            p && 0 == HtmlCssSelectorTest(p->pSelector, pNode, 0);
            p = p->pNext
        );
        if (p) {
            CssCachedSearch *pCache = pSearch->pCache;
            if (pCache->nNode == pCache->nAlloc){
                pCache->nAlloc = (16 + (pCache->nAlloc * 2));
                pCache->apNode = (HtmlNode **)HtmlRealloc("SearchCache", 
                    pCache->apNode, (pCache->nAlloc * sizeof(HtmlNode *))
                );
            }
            pCache->apNode[pCache->nNode] = pNode;
            pCache->nNode++;
        }
    }
    return HTML_WALK_DESCEND;
}

int 
HtmlCssSearchInit(pTree)
    HtmlTree *pTree;
{
    pTree->pSearchCache = HtmlNew(HtmlSearchCache);
    Tcl_InitHashTable(&pTree->pSearchCache->aCache, TCL_STRING_KEYS);
    return TCL_OK;
}

int 
HtmlCssSearchInvalidateCache(pTree)
    HtmlTree *pTree;
{
    Tcl_HashSearch sSearch;
    Tcl_HashEntry *pEntry;
    Tcl_HashTable *p = &pTree->pSearchCache->aCache;

    while ((pEntry = Tcl_FirstHashEntry(p, &sSearch))) {
        CssCachedSearch *pCache = (CssCachedSearch *)Tcl_GetHashValue(pEntry);
	if (pCache) {
	  HtmlFree(pCache->apNode);
	  HtmlFree(pCache);
	}
        Tcl_DeleteHashEntry(pEntry);
    }
 
    return TCL_OK;
}

int 
HtmlCssSearchShutdown(pTree)
    HtmlTree *pTree;
{
    HtmlCssSearchInvalidateCache(pTree);
    Tcl_DeleteHashTable(&pTree->pSearchCache->aCache);
    HtmlFree(pTree->pSearchCache);
    pTree->pSearchCache = 0;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlCssSearch --
 *
 *         widget search CSS-SELECTOR ?OPTIONS?
 *
 *     where OPTIONS are:
 *
 *         -root NODE              (Search the sub-tree at NODE)
 *         -index IDX              (return the idx'th list entry only)
 *         -length                 (return the length of the result only)
 *
 *     With no options, this command is used by the application to 
 *     query the widget for a list of node-handles that match the 
 *     specified css-selector. The nodes are returned in tree-order.
 *
 *     The -index and -length options are mutually exclusive.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlCssSearch(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget */
    Tcl_Interp *interp;                /* The interpreter */
    int objc;                          /* Number of arguments */
    Tcl_Obj *CONST objv[];             /* List of all arguments */
{
    HtmlTree *pTree = (HtmlTree *)clientData;
    char *zOrig;
    int n;
    CssStyleSheet *pStyle = 0;

    /* Search only descendants of this node (NULL means search whole tree) */
    HtmlNode *pSearchRoot = 0;
    int eMode = SEARCH_MODE_ALL;
    int iIndex = 0;

    int iArg;

    Tcl_HashEntry *pEntry = 0;
    CssCachedSearch *pCache = 0;
    int isNew;

    /* Options passed to this command. */
    struct HtmlCssOption {
        const char *zCommand;
        int isBoolean;
        Tcl_Obj *pArg;
    } aOption [] = {
        {"-root",   0, 0}, 
        {"-length", 1, 0}, 
        {"-index",  0, 0}, 
        {0, 0, 0}
    };

    if (objc < 3){
        Tcl_WrongNumArgs(interp, 2, objv, "CSS-SELECTOR ?OPTIONS?");
        return TCL_ERROR;
    }

    for (iArg = 3; iArg < objc; iArg++) {
        int iChoice;
        if (Tcl_GetIndexFromObjStruct(interp, objv[iArg], aOption, 
            sizeof(struct HtmlCssOption), "option", 0, &iChoice)
        ){
            return TCL_ERROR;
        }
        if (!aOption[iChoice].isBoolean) {
            iArg++;
            if (iArg == objc) {
                const char *z = Tcl_GetString(objv[iArg - 1]);
                Tcl_AppendResult(interp, "option requires an argument: ", z, 0);
                return TCL_ERROR;
            }
        } 
        aOption[iChoice].pArg = objv[iArg];
    }

    if (aOption[1].pArg && aOption[2].pArg) {
        const char z[] = "options -length and -index are mutually exclusive";
        Tcl_AppendResult(interp, z, 0);
        return TCL_ERROR;
    }

    if (aOption[0].pArg) {      /* Handle -root option */
        const char *zArg = Tcl_GetString(aOption[0].pArg);
        if (*zArg) {
            pSearchRoot = HtmlNodeGetPointer(pTree, zArg);
        }
    }
    if (aOption[1].pArg) {      /* Handle -length option */
        eMode = SEARCH_MODE_LENGTH;
    }
    if (aOption[2].pArg) {      /* Handle -index option */
        eMode = SEARCH_MODE_INDEX;
        if (Tcl_GetIntFromObj(interp, aOption[2].pArg, &iIndex)) {
            return TCL_ERROR;
        }
    }

    zOrig = Tcl_GetStringFromObj(objv[2], &n);
    if (pSearchRoot) {
        isNew = 1;
    } else {
        pEntry = Tcl_CreateHashEntry(&pTree->pSearchCache->aCache, zOrig, &isNew);
    }
    if (isNew) {
        char *z;
        CssSearch sSearch;

        assert(n == strlen(zOrig));
        n += 11;
        z = (char *)HtmlAlloc("temp", n);
        sprintf(z, "%s {width:0}", zOrig);
        HtmlCssSelectorParse(pTree, n, z, &pStyle);
        if ( !pStyle || !pStyle->pUniversalRules) {
            Tcl_AppendResult(interp, "Bad css selector: \"", zOrig, "\"", 0); 
            return TCL_ERROR;
        }
        sSearch.pRuleList = pStyle->pUniversalRules;
        sSearch.pTree = pTree;
        sSearch.pSearchRoot = pSearchRoot;
        sSearch.pCache = HtmlNew(CssCachedSearch);
        HtmlWalkTree(pTree, pSearchRoot, cssSearchCb, (ClientData)&sSearch);
        pCache = sSearch.pCache;
        HtmlCssStyleSheetFree(pStyle);
        HtmlFree(z);

        if (pEntry) {
            Tcl_SetHashValue(pEntry, sSearch.pCache);
        }
    } else {
        pCache = (CssCachedSearch *)Tcl_GetHashValue(pEntry);
    }

    switch (eMode) {
        case SEARCH_MODE_ALL: {
            Tcl_Obj *pRet = Tcl_NewObj();
            int ii;
            for(ii = 0; ii < pCache->nNode; ii++){
                Tcl_Obj *pCmd = HtmlNodeCommand(pTree, pCache->apNode[ii]);
                Tcl_ListObjAppendElement(interp, pRet, pCmd);
            }
            Tcl_SetObjResult(interp, pRet);
            break;
        }

        case SEARCH_MODE_LENGTH:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(pCache->nNode));
            break;

        case SEARCH_MODE_INDEX:
            if (iIndex >= 0 && iIndex < pCache->nNode) {
                Tcl_SetObjResult(
                    interp, HtmlNodeCommand(pTree, pCache->apNode[iIndex])
                );
            }
            break;
    }

    if (pSearchRoot) {
        HtmlFree(pCache->apNode);
        HtmlFree(pCache);
    }

    return TCL_OK;
}


