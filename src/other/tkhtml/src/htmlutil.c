
#include <tcl.h>

#ifdef TKHTML_ENABLE_PROFILE

#include <string.h>
#include <sys/time.h>
#include "html.h"

/*
** External interface:
**
**     HtmlInstrumentInit()
**
**     ::tkhtml::instrument
*/

typedef struct InstCommand InstCommand;
typedef struct InstGlobal InstGlobal;
typedef struct InstFrame InstFrame;
typedef struct InstVector InstVector;
typedef struct InstData InstData;

struct InstCommand {
    Tcl_CmdInfo info;       /* Original command info, before instrumentation */
    int isDeleted;          /* True after the Tcl command has been deleted */
    Tcl_Obj *pFullName;     /* The name to use for this object command */
    InstGlobal *pGlobal;    /* Pointer to associated "global" object */
    InstCommand *pNext;     /* Next in linked list starting at pGlobal */
};

struct InstVector {
    InstCommand *p1;
    InstCommand *p2;
};

struct InstData {
    int nCall;
    Tcl_WideInt iClicks;
};

struct InstGlobal {
    void (*xCall)(ClientData, int, void (*)(ClientData), ClientData);

    /* List of all dynamic InstCommand commands */
    InstCommand *pGlobal;

    /* Current caller (or NULL). */
    InstCommand *pCaller;

    Tcl_HashTable aVector;

    InstCommand aCommand[HTML_INSTRUMENT_NUM_SYMS];
};

#define timevalToClicks(tv) ( \
    (Tcl_WideInt)tv.tv_usec + ((Tcl_WideInt)1000000 * (Tcl_WideInt)tv.tv_sec) \
)

static void
updateInstData(pGlobal, p, iClicks)
    InstGlobal *pGlobal;
    InstCommand *p;
    int iClicks;
{
    InstVector vector;
    InstData *pData;
    Tcl_HashEntry *pEntry;
    int isNew;

    /* Update the InstData structure associated with this call vector */
    vector.p1 = pGlobal->pCaller;
    vector.p2 = p;
    pEntry = Tcl_CreateHashEntry(&pGlobal->aVector, (char *)&vector, &isNew);
    if (isNew) {
        pData = (InstData *)ckalloc(sizeof(InstData));
        memset(pData, 0, sizeof(InstData));
        Tcl_SetHashValue(pEntry, pData);
    } else {
        pData = Tcl_GetHashValue(pEntry);
    }
    pData->nCall++;
    pData->iClicks += iClicks;
}

void *
HtmlInstrumentCall2(pClientData, iCall, xFunc, clientData)
    ClientData pClientData;
    int iCall;
    void *(*xFunc)(ClientData);
    ClientData clientData;
{
    InstGlobal *pGlobal = (InstGlobal *)pClientData;
    InstCommand *p = &pGlobal->aCommand[iCall];
    InstCommand *pCaller;       /* Calling frame (if any) */

    Tcl_WideInt iClicks;
    struct timeval tv = {0, 0};
    void *pRet;

    pCaller = pGlobal->pCaller;
    pGlobal->pCaller = p;
    gettimeofday(&tv, 0);
    iClicks = timevalToClicks(tv);

    pRet = xFunc(clientData);

    gettimeofday(&tv, 0);
    iClicks = timevalToClicks(tv) - iClicks;
    pGlobal->pCaller = pCaller;

    updateInstData(pGlobal, p, (int)iClicks);
    return pRet;
}
void 
HtmlInstrumentCall(pClientData, iCall, xFunc, clientData)
    ClientData pClientData;
    int iCall;
    void (*xFunc)(ClientData);
    ClientData clientData;
{
    InstGlobal *pGlobal = (InstGlobal *)pClientData;
    InstCommand *p = &pGlobal->aCommand[iCall];
    InstCommand *pCaller;       /* Calling frame (if any) */

    Tcl_WideInt iClicks;
    struct timeval tv = {0, 0};

    pCaller = pGlobal->pCaller;
    pGlobal->pCaller = p;
    gettimeofday(&tv, 0);
    iClicks = timevalToClicks(tv);

    xFunc(clientData);

    gettimeofday(&tv, 0);
    iClicks = timevalToClicks(tv) - iClicks;
    pGlobal->pCaller = pCaller;

    updateInstData(pGlobal, p, (int)iClicks);
}

static int 
execInst(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    InstCommand *p = (InstCommand *)clientData;
    InstGlobal *pGlobal = p->pGlobal;
    InstCommand *pCaller;       /* Calling frame (if any) */

    int rc;
    Tcl_WideInt iClicks;
    struct timeval tv = {0, 0};

    pCaller = pGlobal->pCaller;
    pGlobal->pCaller = p;
    gettimeofday(&tv, 0);
    iClicks = timevalToClicks(tv);

    rc = p->info.objProc(p->info.objClientData, interp, objc, objv);

    gettimeofday(&tv, 0);
    iClicks = timevalToClicks(tv) - iClicks;
    pGlobal->pCaller = pCaller;

    updateInstData(pGlobal, p, (int)iClicks);
    return rc;
}

static void
freeInstStruct(p)
    InstCommand *p;
{
    Tcl_DecrRefCount(p->pFullName);
    ckfree((void *)p);
}
static void 
freeInstCommand(clientData)
    ClientData clientData;
{
    InstCommand *p = (InstCommand *)clientData;
    if (p->info.deleteProc) {
        p->info.deleteProc(p->info.deleteData);
    }
    p->isDeleted = 1;
}

static int 
instCommand(clientData, interp, objc, objv)
    ClientData clientData;             /* Pointer to InstGlobal structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    InstGlobal *pGlobal = (InstGlobal *)clientData;
    Tcl_Command token;
    Tcl_CmdInfo new_info;
    InstCommand *pInst;

    token = Tcl_GetCommandFromObj(interp, objv[2]);
    if (!token) {
        Tcl_AppendResult(interp, "no such command: ", Tcl_GetString(objv[2]),0);
        return TCL_ERROR;
    }

    pInst = (InstCommand *)ckalloc(sizeof(InstCommand));
    memset(pInst, 0, sizeof(InstCommand));
    Tcl_GetCommandInfoFromToken(token, &pInst->info);
    pInst->pFullName = Tcl_NewObj();
    pInst->pGlobal = pGlobal;
    Tcl_IncrRefCount(pInst->pFullName);
    Tcl_GetCommandFullName(interp, token, pInst->pFullName);

    pInst->pNext = pGlobal->pGlobal;
    pGlobal->pGlobal = pInst;

    Tcl_GetCommandInfoFromToken(token, &new_info);
    new_info.objClientData = pInst;
    new_info.objProc = execInst;
    new_info.deleteProc = freeInstCommand;
    new_info.deleteData = pInst;
    Tcl_SetCommandInfoFromToken(token, &new_info);

    return TCL_OK;
}

static int 
instVectors(clientData, interp, objc, objv)
    ClientData clientData;             /* Unused */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    InstGlobal *pGlobal = (InstGlobal *)clientData;
    Tcl_Obj *pRet;

    Tcl_HashSearch sSearch;
    Tcl_HashEntry *pEntry;

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    for (
        pEntry = Tcl_FirstHashEntry(&pGlobal->aVector, &sSearch); 
        pEntry;
        pEntry = Tcl_NextHashEntry(&sSearch)
    ) {
        InstData *pData;
        InstVector *pVector;

        pData = (InstData *)Tcl_GetHashValue(pEntry);
        pVector = (InstVector *)Tcl_GetHashKey(&pGlobal->aVector, pEntry);

        if (!pVector->p1) {
            Tcl_Obj *pTop = Tcl_NewStringObj("<toplevel>", -1);
            Tcl_ListObjAppendElement(interp, pRet, pTop);
        } else {
            Tcl_ListObjAppendElement(interp, pRet, pVector->p1->pFullName);
        }
        Tcl_ListObjAppendElement(interp,pRet,pVector->p2->pFullName);
        Tcl_ListObjAppendElement(interp,pRet,Tcl_NewIntObj(pData->nCall));
        Tcl_ListObjAppendElement(interp,pRet,Tcl_NewWideIntObj(pData->iClicks));
    }

    Tcl_SetObjResult(interp, pRet);
    Tcl_DecrRefCount(pRet);

    return TCL_OK;
}

static int 
instZero(clientData, interp, objc, objv)
    ClientData clientData;             /* InstGlobal structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    InstGlobal *pGlobal = (InstGlobal *)clientData;
    InstCommand *p;
    InstCommand **pp;
    Tcl_Obj *pRet;

    Tcl_HashSearch sSearch;
    Tcl_HashEntry *pEntry;

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    pp = &pGlobal->pGlobal;
    for (p = *pp; p; p = *pp) {
        if (p->isDeleted) {
            *pp = p->pNext;
            freeInstStruct(p);
        }else{
            pp = &p->pNext;
        }
    }

    for (
        pEntry = Tcl_FirstHashEntry(&pGlobal->aVector, &sSearch); 
        pEntry;
        pEntry = Tcl_NextHashEntry(&sSearch)
    ) {
        InstData *pData = Tcl_GetHashValue(pEntry);
        ckfree((void *)pData);
    }
    Tcl_DeleteHashTable(&pGlobal->aVector);
    Tcl_InitHashTable(&pGlobal->aVector, sizeof(InstVector)/sizeof(int));

    Tcl_SetObjResult(interp, pRet);
    Tcl_DecrRefCount(pRet);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * instrument_objcmd --
 *
 *     Implementation of [instrument_objcmd]. Syntax is:
 *
 *         instrument command COMMAND
 *         instrument report
 *
 * Results:
 *     TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *     See above.
 *
 *---------------------------------------------------------------------------
 */
static int 
instrument_objcmd(clientData, interp, objc, objv)
    ClientData clientData;             /* Unused */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    int iChoice;
    struct SubCmd {
        const char *zName;
        Tcl_ObjCmdProc *xFunc;
    } aSub[] = {
        { "command", instCommand }, 
        { "vectors", instVectors }, 
        { "zero",    instZero }, 
        { 0, 0 }
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "SUB-COMMAND");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObjStruct(interp, 
            objv[1], aSub, sizeof(aSub[0]), "sub-command", 0, &iChoice)
    ){
        return TCL_ERROR;
    }

    return aSub[iChoice].xFunc(clientData, interp, objc, objv);
}

static void 
instDelCommand(clientData)
    ClientData clientData;
{
    /* InstGlobal *p = (InstGlobal *)clientData; */
    /* TODO */
}

void
HtmlInstrumentInit(interp)
    Tcl_Interp *interp;
{
    InstGlobal *p = (InstGlobal *)ckalloc(sizeof(InstGlobal));
    memset(p, 0, sizeof(InstGlobal));

    p->xCall = HtmlInstrumentCall;
    p->aCommand[0].pFullName = Tcl_NewStringObj("C: External Script Event", -1);
    p->aCommand[1].pFullName = Tcl_NewStringObj("C: callbackHandler()", -1);
    p->aCommand[2].pFullName = Tcl_NewStringObj(
        "C: runDynamicStyleEngine()", -1
    );
    p->aCommand[3].pFullName = Tcl_NewStringObj("C: runStyleEngine()", -1);
    p->aCommand[4].pFullName = Tcl_NewStringObj("C: runLayoutEngine()", -1);
    p->aCommand[5].pFullName = Tcl_NewStringObj("C: allocateNewFont()", -1);

    Tcl_IncrRefCount(p->aCommand[0].pFullName);
    Tcl_IncrRefCount(p->aCommand[1].pFullName);
    Tcl_IncrRefCount(p->aCommand[2].pFullName);
    Tcl_IncrRefCount(p->aCommand[3].pFullName);
    Tcl_IncrRefCount(p->aCommand[4].pFullName);

    Tcl_InitHashTable(&p->aVector, sizeof(InstVector)/sizeof(int));
    Tcl_CreateObjCommand(interp, 
        "::tkhtml::instrument", instrument_objcmd, (ClientData)p, instDelCommand
    );
}

#else  /* TKHTML_ENABLE_PROFILE */
void
HtmlInstrumentInit(interp)
    Tcl_Interp *interp;
{
    /* No-op */
}

#endif


