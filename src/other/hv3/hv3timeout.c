/*
 * hv3timeout.c --
 *
 *     This file contains C-code that implements the following 
 *     methods of the global object in the Hv3 web browser:
 *
 *         setTimeout() 
 *         clearTimeout() 
 *         setInterval() 
 *         clearInterval() 
 *
 *     Events are scheduled using the Tcl event loop (specifically the
 *     Tcl_CreateTimerHandler() API).
 *
 *
 * TODO: Copyright.
 */

struct SeeTimeout {
  Tcl_TimerToken token;

  SeeInterp *pTclSeeInterp;

  struct SEE_object *pWindow;

  struct SEE_value func;
  int nArg;
  struct SEE_value **apArg;

  /* Number of milliseconds for setInterval(). Or -1 for setTimeout(). */
  int iInterval;

  /* Linked list pointers and id number. */
  int iId;
  SeeTimeout *pNext;
  SeeTimeout **ppThis;
};

static void delTimeout(p)
    SeeTimeout *p;
{
    *p->ppThis = p->pNext;
    if( p->pNext ){
        p->pNext->ppThis = p->ppThis;
    }
    p->pNext = 0;
    p->ppThis = 0;
}

#ifndef NDEBUG
  static void realTimeoutCb(ClientData);
  static void
  timeoutCb(clientData)
      ClientData clientData;
  {
      SeeTimeout *p = (SeeTimeout *)clientData;
      ClientData pInstrumentData = p->pTclSeeInterp->pInstrumentData;
      if (pInstrumentData) {
          void (*xCall)(ClientData, int, void (*)(ClientData), ClientData);
          xCall = *((void **)pInstrumentData);
          xCall(pInstrumentData, 0, realTimeoutCb, clientData);
      } else {
          realTimeoutCb(clientData);
      }
  }
  static void realTimeoutCb(clientData)
#else
  static void timeoutCb(clientData)
#endif
    ClientData clientData;
{
    SeeTimeout *p = (SeeTimeout *)clientData;
    struct SEE_interpreter *pSee = &p->pTclSeeInterp->interp;

    SEE_try_context_t try_ctxt;
    struct SEE_value *pError;

    assert(p->ppThis);

    if (SEE_VALUE_GET_TYPE(&p->func) == SEE_OBJECT) {
        struct SEE_value r;
        SEE_TRY(pSee, try_ctxt) {
            SEE_OBJECT_CALL(
                pSee, p->func.u.object, pSee->Global, p->nArg, p->apArg, &r
            );
        }
    } else {
        struct SEE_input *pInputCode;
        struct SEE_value str;
        struct SEE_value res;
        struct SEE_object *pW = p->pWindow;

        struct SEE_scope *pScope;
        pScope = (struct SEE_scope *)SEE_malloc(pSee, sizeof(*pScope) * 2);
        pScope->obj = pW;
        pScope->next = &pScope[1];
        pScope->next->obj = pSee->Global;
        pScope->next->next = 0;

        SEE_ToString(pSee, &p->func, &str);
        assert(SEE_VALUE_GET_TYPE(&str) == SEE_STRING);

        pInputCode = SEE_input_string(pSee, str.u.string);
        SEE_TRY(pSee, try_ctxt) {
            SEE_eval(pSee, pInputCode, pW, pW, pScope, &res);
        }
        SEE_INPUT_CLOSE(pInputCode);
    }

    pError = SEE_CAUGHT(try_ctxt);
    if (pError) {
        struct SEE_value str;
        struct SEE_value err;
        printf("TIMER CALLBACK ERROR:");
        SEE_ToString(pSee, pError, &err);
        SEE_PrintValue(pSee, pError, stdout);
        SEE_PrintString(pSee, err.u.string, stdout);
        printf("\n");
        SEE_ToString(pSee, &p->func, &str);
        SEE_PrintString(pSee, str.u.string, stdout);
        fflush(stdout);
    }

    if (p->token) {
        /* If p->token was NULL, then the callback has invoked javascript
         * function cancelInterval() or cancelTimeout(). Otherwise, either
         * reschedule the callback (for an interval) or remove the structure
         * from the linked-list so the garbage-collector can find it (for a 
         * timeout).
         */
        if (p->iInterval < 0) {
            delTimeout(p);
            p->token = 0;
        } else if (p->token) {
            ClientData c = (ClientData)p;
            assert(p->ppThis);
            p->token = Tcl_CreateTimerHandler(p->iInterval, timeoutCb, c);
        }
    }
}

static void newTimeout(pSeeInterp, pThis, isInterval, argc, argv, res)
    SeeInterp *pSeeInterp;
    struct SEE_object *pThis;
    int isInterval;
    int argc;
    struct SEE_value **argv;
    struct SEE_value *res;           /* OUT: Return the new timer id */
{
    struct SEE_interpreter *pI = &pSeeInterp->interp;
    SeeTimeout *p;
    int ii;
    int iMilli;
    struct SEE_value milli;
    struct SEE_value *pCopy;
    SeeTclObject *pWindow;

    /* Check pThis is a tcl based object */
    if (pThis->objectclass != getVtbl()) {
        SEE_error_throw(pI, pI->Error, "thisobj is not a SeeTclObject");
    }
    pWindow = (SeeTclObject *)pThis;

    /* Check the number of function arguments. */
    if (argc < 2) {
        SEE_error_throw(
            pI, pI->Error, "Function requires at least 2 parameters"
        );
    }

    /* Check that if there are more than 2 arguments, the first argument is
     * an object, not a string.
     */
    if (argc >2 && SEE_VALUE_GET_TYPE(argv[0]) != SEE_OBJECT) {
        SEE_error_throw(
            pI, pI->TypeError, "First argument must be of type object"
        );
    }

    /* Convert the second argument to an integer (number of milliseconds) */
    SEE_ToInteger(pI, argv[1], &milli);
    assert(SEE_VALUE_GET_TYPE(&milli) == SEE_NUMBER);
    iMilli = (int)milli.u.number;

    /* Allocate the new SeeTimeout structure and populate the 
     * SeeTimout.func and SeeTimeout.apArg variables.
     */
    p = (SeeTimeout *)SEE_malloc(pI, 
        sizeof(SeeTimeout) + 
        (argc-2)*sizeof(struct SEE_value *) +
        (argc-2)*sizeof(struct SEE_value)
    );
    p->apArg = (struct SEE_value **)&p[1];
    // p->nArg = argc-2;
    p->nArg = 0;
    pCopy = (struct SEE_value *)&p->apArg[p->nArg];
    for (ii = 0; ii < p->nArg; ii++) {
        SEE_VALUE_COPY(pCopy, argv[ii+2]);
        p->apArg[ii] = pCopy;
        pCopy++;
    }
    SEE_VALUE_COPY(&p->func, argv[0]);

    p->iInterval = (isInterval?iMilli:-1);
    p->iId = pSeeInterp->iNextTimeout++;
    p->pTclSeeInterp = pSeeInterp;
    p->pNext = pWindow->pTimeout;
    pWindow->pTimeout = p;
    p->ppThis = &pWindow->pTimeout;
    if (p->pNext) {
        p->pNext->ppThis = &p->pNext;
    }
    p->pWindow = (struct SEE_object *)pWindow;

    assert(p->ppThis);
    p->token = Tcl_CreateTimerHandler(iMilli, timeoutCb, (ClientData)p);
    SEE_SET_NUMBER(res, p->iId);
}

static void cancelTimeout(pSeeInterp, pThis, isInterval, argc, argv, res)
    SeeInterp *pSeeInterp;
    struct SEE_object *pThis;
    int isInterval;
    int argc;
    struct SEE_value **argv;
    struct SEE_value *res;           /* OUT: Return the new timer id */
{
    int iId;
    struct SEE_value id;
    struct SEE_interpreter *pI = &pSeeInterp->interp;
    SeeTimeout *p;
    SeeTclObject *pWindow;

    /* Check pThis is a tcl based object */
    if (pThis->objectclass != getVtbl()) {
        SEE_error_throw(pI, pI->Error, "thisobj is not a SeeTclObject");
    }
    pWindow = (SeeTclObject *)pThis;

    /* Check the number of function arguments. */
    if (argc != 1) {
        SEE_error_throw(
            pI, pI->Error, "Function requires exactly 1 parameter"
        );
    }

    SEE_ToNumber(pI, argv[0], &id);
    assert(SEE_VALUE_GET_TYPE(&id) == SEE_NUMBER);
    iId = (int)id.u.number;

    for (p = pWindow->pTimeout; p; p = p->pNext) {
        if (p->iId == iId) {
            Tcl_DeleteTimerHandler(p->token);
            p->token = 0;
            delTimeout(p);
            break;
        }
    }

    SEE_SET_UNDEFINED(res);
}

static void
setTimeoutFunc(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
    newTimeout((SeeInterp *)interp, thisobj, 0, argc, argv, res);
}

static void
setIntervalFunc(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
    newTimeout((SeeInterp *)interp, thisobj, 1, argc, argv, res);
}

static void
clearTimeoutFunc(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
    cancelTimeout((SeeInterp *)interp, thisobj, 0, argc, argv, res);
}

static void
clearIntervalFunc(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
    cancelTimeout((SeeInterp *)interp, thisobj, 1, argc, argv, res);
}

static void 
interpTimeoutInit(pSeeInterp, pWindow)
    SeeInterp *pSeeInterp;
    SeeTclObject *pWindow;
{
    struct SEE_interpreter *interp = (struct SEE_interpreter *)pSeeInterp;
    struct SEE_object *g = (struct SEE_object *)pWindow;
 
    SEE_CFUNCTION_PUTA(interp, g, "setTimeout", setTimeoutFunc, 2, 0);
    SEE_CFUNCTION_PUTA(interp, g, "setInterval", setIntervalFunc, 2, 0);
    SEE_CFUNCTION_PUTA(interp, g, "clearTimeout", clearTimeoutFunc, 1, 0);
    SEE_CFUNCTION_PUTA(interp, g, "clearInterval", clearIntervalFunc, 1, 0);
}

static void 
interpTimeoutCleanup(pSeeInterp, pWindow)
    SeeInterp *pSeeInterp;
    SeeTclObject *pWindow;
{
    SeeTimeout *p;
    for (p = pWindow->pTimeout; p; p = p->pNext) {
        Tcl_DeleteTimerHandler(p->token);
        assert(*p->ppThis == p);
        *p->ppThis = 0;
        p->ppThis = 0;
    }
    assert(!pWindow->pTimeout);
}

