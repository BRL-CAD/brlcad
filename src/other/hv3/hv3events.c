
/*----------------------------------------------------------------------- 
 * DOM Level 2 Events for Hv3.
 *
 *     // Introduced in DOM Level 2:
 *     interface EventTarget {
 *         void addEventListener(
 *             in DOMString type, 
 *             in EventListener listener, 
 *             in boolean useCapture
 *         );
 *         void removeEventListener(
 *             in DOMString type, 
 *             in EventListener listener, 
 *             in boolean useCapture
 *         );
 *         boolean dispatchEvent(in Event evt) raises(EventException);
 *     };
 */

/*
 * By defining STOP_PROPAGATION as "cancelBubble", we can also
 * support the mozilla extension Event.cancelBubble. Setting it to true
 * "cancels bubbling", just like calling stopPropagation().
 *
 * At time of writing google toolkits use this.
 */
#define STOP_PROPAGATION        "cancelBubble"
#define PREVENT_DEFAULT         "hv3__see__preventDefault"
#define CALLED_LISTENER         "hv3__see__calledListener"


typedef struct EventTarget EventTarget;
typedef struct ListenerContainer ListenerContainer;

struct EventTarget {
  SeeInterp *pTclSeeInterp;
  EventType *pTypeList;
};

struct EventType {
  struct SEE_string *zType;
  ListenerContainer *pListenerList;
  EventType *pNext;
};

struct ListenerContainer {
  int isCapture;                  /* True if a capturing event */
  struct SEE_object *pListener;   /* Listener function */
  ListenerContainer *pNext;       /* Next listener on this event type */
};

/*
 *---------------------------------------------------------------------------
 *
 * valueToBoolean --
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
valueToBoolean(interp, pValue, eDef)
    struct SEE_interpreter *interp;
    struct SEE_value *pValue;
    int eDef;
{
    switch (SEE_VALUE_GET_TYPE(pValue)) {
        case SEE_BOOLEAN:
            return pValue->u.boolean;

        case SEE_NUMBER:
            return (pValue->u.number != 0.0);

        default:
            break;
    }

    return eDef;
}

/*
 *---------------------------------------------------------------------------
 *
 * setBooleanFlag --
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
setBooleanFlag(interp, pObj, zName, v)
    struct SEE_interpreter *interp;
    struct SEE_object *pObj;
    const char *zName;
    int v;
{
    struct SEE_value yes;
    SEE_SET_BOOLEAN(&yes, v);
    SEE_OBJECT_PUTA(interp, pObj, zName, &yes, 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * getBooleanFlag --
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
getBooleanFlag(interp, pObj, zName)
    struct SEE_interpreter *interp;
    struct SEE_object *pObj;
    const char *zName;
{
    struct SEE_value val;
    SEE_OBJECT_GETA(interp, pObj, zName, &val);
    return valueToBoolean(interp, &val, 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * getEventList --
 *
 * Results: 
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static EventType **
getEventList(interp, pObj)
    struct SEE_interpreter *interp;
    struct SEE_object *pObj;
{
    if (pObj->objectclass == getVtbl()) {
        return &((SeeTclObject *)pObj)->pTypeList;
    }
    return 0;
}

static void
objectCall(pInterp, pObj, pThis, argc, argv, pRes)
    struct SEE_interpreter *pInterp;
    struct SEE_object *pObj;
    struct SEE_object *pThis;
    int argc;
    struct SEE_value **argv;
    struct SEE_value *pRes;
{
#if 1
    SEE_OBJECT_CALL(pInterp, pObj, pThis, argc, argv, pRes);
#else
    SEE_try_context_t try_ctxt;
    SEE_TRY(pInterp, try_ctxt) {
        SEE_OBJECT_CALL(pInterp, pObj, pThis, argc, argv, pRes);
    }
    if (SEE_CAUGHT(try_ctxt)) {
        struct SEE_value str;
        SEE_ToString(pInterp, SEE_CAUGHT(try_ctxt), &str);
        printf("Error in event-handler: ");
        SEE_PrintValue(pInterp, &str, stdout);
        printf("\n");
    } 
#endif
}

/*
 *---------------------------------------------------------------------------
 *
 * runEvent --
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
runEvent(interp, pTarget, pEvent, zType, isCapture)
        struct SEE_interpreter *interp;
        struct SEE_object *pTarget;
        struct SEE_value *pEvent;
        struct SEE_string *zType;
        SEE_boolean_t isCapture;
{
    int rc = 1;
    EventType *pET = 0;
    EventType **ppET;
    ListenerContainer *pL = 0;
    struct SEE_value target;

    /* If *pTarget is a tcl-based object, then set this pointer to pTarget. 
     * Otherwise leave it as null.
     */
    SeeTclObject *pTclObject = 0;

    assert(zType->flags & SEE_STRING_FLAG_INTERNED);
    assert(isCapture == 1 || isCapture == 0);
    assert(SEE_VALUE_GET_TYPE(pEvent) == SEE_OBJECT);

    SEE_SET_OBJECT(&target, pTarget);
    SEE_OBJECT_PUTA(interp, pEvent->u.object, "currentTarget", &target, 0);

    /* Check if stopPropagation() has been called. */
    if (getBooleanFlag(interp, pEvent->u.object, STOP_PROPAGATION)) {
        return 0;
    }

    if (pTarget->objectclass == getVtbl()) {
        pTclObject = (SeeTclObject *)pTarget;
    }

    /* If this is a Tcl based object, run any registered DOM event handlers */
    if (pTclObject) {
        ppET = &pTclObject->pTypeList;
        for (pET = *ppET; pET && pET->zType != zType; pET = pET->pNext);
        for (pL = (pET ? pET->pListenerList: 0); rc && pL; pL = pL->pNext) {
            if (pL->isCapture == isCapture) {
                struct SEE_value r;
                objectCall(interp, pL->pListener, pTarget, 1, &pEvent, &r);
                setBooleanFlag(interp, pEvent->u.object, CALLED_LISTENER, 1);
            }
        }
    }

    /* If this is not the "capturing" phase, run the legacy event-handler. */
    if (!isCapture) {
        struct SEE_value val;
        struct SEE_string *e = SEE_string_new(interp, 128);
        struct SEE_object *pLookup = (pTclObject ?
            ((struct SEE_object *)pTclObject->pNative) : pTarget
        );

        SEE_string_append_ascii(e, "on");
        SEE_string_append(e, zType);
        e = SEE_intern(interp, e);

        SEE_OBJECT_GET(interp, pLookup, e, &val);
        if (SEE_VALUE_GET_TYPE(&val) == SEE_OBJECT) {
            struct SEE_object *pE = pEvent->u.object;
            struct SEE_value res;
            objectCall(interp, val.u.object, pTarget, 1, &pEvent, &res);
            setBooleanFlag(interp, pE, CALLED_LISTENER, 1);
            rc = valueToBoolean(interp, &res, 1);
            if (!rc) {
                setBooleanFlag(interp, pE, PREVENT_DEFAULT, 1);
                setBooleanFlag(interp, pE, STOP_PROPAGATION, 1);
            }
        }
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * preventDefaultFunc --
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
preventDefaultFunc(interp, self, thisobj, argc, argv, res)
    struct SEE_interpreter *interp;
    struct SEE_object *self, *thisobj;
    int argc;
    struct SEE_value **argv, *res;
{
    setBooleanFlag(interp, thisobj, PREVENT_DEFAULT, 1);
}

/*
 *---------------------------------------------------------------------------
 *
 * stopPropagationFunc --
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
stopPropagationFunc(interp, self, thisobj, argc, argv, res)
    struct SEE_interpreter *interp;
    struct SEE_object *self, *thisobj;
    int argc;
    struct SEE_value **argv, *res;
{
    setBooleanFlag(interp, thisobj, STOP_PROPAGATION, 1);
}

static struct SEE_object *
getParentNode(interp, p)
    struct SEE_interpreter *interp;
    struct SEE_object *p;
{
    struct SEE_value val;

    if (p->objectclass == getVtbl()){
        NodeHack *pNode = ((SeeTclObject *)p)->nodehandle;
        if (pNode && pNode->pParent && pNode->pParent->pObj) {
            return (struct SEE_object *)pNode->pParent->pObj;
        }
        if (pNode && pNode->pParent == 0 && pNode->iNode < 0){
            return 0;
        }
        if (pNode && pNode->pParent == 0) {
            /* Return document... */
        }
    }

    SEE_OBJECT_GETA(interp, p, "parentNode", &val);
    if (SEE_VALUE_GET_TYPE(&val) != SEE_OBJECT) return 0;
    return val.u.object;
}

/*
 *---------------------------------------------------------------------------
 *
 * dispatchEventFunc --
 *
 *     Implementation of DOM method EventTarget.dispatchEvent().
 *
 *     According to the DOM, the boolean return value of dispatchEvent()
 *     indicates whether any of the listeners which handled the event 
 *     called preventDefault. If preventDefault was called the value is 
 *     false, else the value is true.
 *
 *     Before running any event-handlers, the following are added to
 *     the event object passed as the only argument:
 *
 *         target
 *         stopPropagation()
 *         preventDefault()
 *
 *         currentTarget            (updated throughout)
 *         eventPhase               (updated throughout)
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
dispatchEventFunc(interp, self, thisobj, argc, argv, res)
    struct SEE_interpreter *interp;
    struct SEE_object *self, *thisobj;
    int argc;
    struct SEE_value **argv, *res;
{
    int isBubbler = 0;
    struct SEE_value val;
    struct SEE_value val2;
    int nObj = 0;
    int nObjAlloc = 0;
    struct SEE_object **apObj = 0;
    int ii;

    int isRun = 1;

    struct SEE_value target;
    struct SEE_value eventPhase;

    struct SEE_string *zType = 0;
    struct SEE_object *pEvent = 0;

    /* Check the number of function arguments. */
    if (argc != 1) {
        const char *zErr = "Function requires exactly 1 parameter";
        SEE_error_throw(interp, interp->Error, zErr);
    }

    if (SEE_VALUE_GET_TYPE(argv[0]) != SEE_OBJECT) {
        return;
    }
    pEvent = argv[0]->u.object;
    assert(pEvent);

    SEE_CFUNCTION_PUTA(interp,pEvent,"stopPropagation",stopPropagationFunc,0,0);
    SEE_CFUNCTION_PUTA(interp,pEvent,"preventDefault",preventDefaultFunc,0,0);

    SEE_SET_OBJECT(&target, thisobj);
    SEE_OBJECT_PUTA(interp, pEvent, "target", &target, 0);

    setBooleanFlag(interp, pEvent, STOP_PROPAGATION, 0);
    setBooleanFlag(interp, pEvent, PREVENT_DEFAULT, 0);
    setBooleanFlag(interp, pEvent, CALLED_LISTENER, 0);

    /* Get the event type */
    SEE_OBJECT_GETA(interp, pEvent, "type", &val);
    SEE_ToString(interp, &val, &val2);
    if (SEE_VALUE_GET_TYPE(&val2) != SEE_STRING) {
        /* Event without a type - matches no listeners */
        /* TODO: DOM Level 3 says to throw "UNSPECIFIED_EVENT_TYPE_ERR" */
        return;
    }
    zType = SEE_intern(interp, val2.u.string);

    /* Check if the event "bubbles". */
    SEE_OBJECT_GETA(interp, pEvent, "bubbles", &val);
    isBubbler = valueToBoolean(interp, &val, 0);

    /* If this is a bubbling event, create a list of the nodes ancestry
     * to deliver it to now. This is because any callbacks that modify
     * the document tree are not supposed to affect the delivery of
     * this event.
     */
    if (isBubbler) {
        struct SEE_object *p = thisobj;
        while (1) {
            struct SEE_object *pObj = getParentNode(interp, p);
            if (!pObj) break;
            if (nObj == nObjAlloc) {
                int nNew;
                struct SEE_object **aNew;
                int nCopy = (sizeof(struct SEE_object *) * (nObjAlloc));
                nObjAlloc += 10;
                nNew = (sizeof(struct SEE_object *) * (nObjAlloc));
                aNew = SEE_malloc(interp, nNew);
                if (nObj > 0){
                    memcpy(aNew, apObj, nCopy);
                }
                apObj = aNew;
            }
            apObj[nObj++] = pObj;
            p = pObj;
        }
    }

    /* Deliver the "capturing" phase of the event. */
    SEE_SET_NUMBER(&eventPhase, 1);
    SEE_OBJECT_PUTA(interp, pEvent, "eventPhase", &eventPhase, 0);
    for (ii = nObj - 1; isRun && ii >= 0; ii--) {
        isRun = runEvent(interp, apObj[ii], argv[0], zType, 1);
    }

    /* Deliver the "target" phase of the event. */
    SEE_SET_NUMBER(&eventPhase, 2);
    SEE_OBJECT_PUTA(interp, pEvent, "eventPhase", &eventPhase, 0);
    if (isRun) {
        isRun = runEvent(interp, thisobj, argv[0], zType, 0);
    }

    /* Deliver the "bubbling" phase of the event. */
    SEE_SET_NUMBER(&eventPhase, 3);
    SEE_OBJECT_PUTA(interp, pEvent, "eventPhase", &eventPhase, 0);
    for (ii = 0; isRun && ii < nObj; ii++) {
        isRun = runEvent(interp, apObj[ii], argv[0], zType, 0);
    }

    /* Set the return value. */
    SEE_SET_BOOLEAN(res, getBooleanFlag(interp, pEvent, PREVENT_DEFAULT));
}

/*
 *---------------------------------------------------------------------------
 *
 * eventDispatchCmd --
 *
 *     $see dispatch TARGET-COMMAND EVENT-COMMAND
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
eventDispatchCmd(clientData, pTcl, objc, objv)
    ClientData clientData;
    Tcl_Interp *pTcl;
    int objc;
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    SeeInterp *pTclSeeInterp = (SeeInterp *)clientData;
    struct SEE_interpreter *p = &pTclSeeInterp->interp;
    struct SEE_object *pTarget;
    struct SEE_object *pEvent;
    struct SEE_value event;
    struct SEE_value disp;

    SEE_try_context_t try_ctxt;
    int rc = TCL_OK;

    pTarget = findOrCreateObject(pTclSeeInterp, objv[2], 0);
    // pEvent = createTransient(pTclSeeInterp, objv[3]);
    pEvent = createNative(pTclSeeInterp, objv[3]);
    assert(Tcl_IsShared(objv[3]));

    SEE_TRY (p, try_ctxt) {
        SEE_SET_OBJECT(&event, pEvent);
        SEE_OBJECT_GETA(p, pTarget, "dispatchEvent", &disp);
        if (SEE_VALUE_GET_TYPE(&disp) == SEE_OBJECT) {
            struct SEE_value *ptr = &event;
            struct SEE_value res;
            SEE_OBJECT_CALL(p, disp.u.object, pTarget, 1, &ptr, &res);
        }
    }
    if (SEE_CAUGHT(try_ctxt)) {
        rc = handleJavascriptError(pTclSeeInterp, &try_ctxt);
    } else {
        int isHandled = getBooleanFlag(p, pEvent, CALLED_LISTENER);
        int isPrevent = getBooleanFlag(p, pEvent, PREVENT_DEFAULT);
        Tcl_Obj *pRet = Tcl_NewObj();
        Tcl_ListObjAppendElement(pTcl, pRet, Tcl_NewBooleanObj(isHandled));
        Tcl_ListObjAppendElement(pTcl, pRet, Tcl_NewBooleanObj(isPrevent));
        Tcl_SetObjResult(pTcl, pRet);
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * addEventListenerFunc --
 *
 *     Implementation of DOM method EventTarget.addEventListener():
 *
 *         void addEventListener(
 *             in DOMString type, 
 *             in EventListener listener, 
 *             in boolean useCapture
 *         );
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
addEventListenerFunc(interp, self, thisobj, argc, argv, res)
    struct SEE_interpreter *interp;
    struct SEE_object *self, *thisobj;
    int argc;
    struct SEE_value **argv, *res;
{
    EventType *pET;
    EventType **ppET;
    ListenerContainer *pL;

    /* Parameters passed to javascript function */
    int useCapture = 0;
    struct SEE_string *zType = 0;
    struct SEE_object *pListener = 0;

    /* Check the number of function arguments. */
    if (argc != 3) {
        const char *zErr = "Function requires exactly 3 parameters";
        SEE_error_throw(interp, interp->Error, zErr);
    }

    /* Check that thisobj is a Tcl based object */
    ppET = getEventList(interp, thisobj);
    assert(thisobj != interp->Global || ppET);
    if (!ppET) {
        const char *zErr = "Bad type for thisobj";
        SEE_error_throw(interp, interp->Error, zErr);
        return;
    }

    /* Parse the arguments */
    /* TODO: Something wrong with the first call to SEE_parse_args() here. 
     * Need to figure out what it is and report to see-dev.
     */
#if 0
    SEE_parse_args(interp, argc, argv, "s|o|b|",&zType,&pListener,&useCapture);
#else
    assert(argc == 3);
    SEE_parse_args(interp, 1, argv, "s",&zType);
    SEE_parse_args(interp, 1, &argv[1], "o",&pListener);
    SEE_parse_args(interp, 1, &argv[2], "b",&useCapture);
#endif

    zType = SEE_intern(interp, zType);
    useCapture = (useCapture ? 1 : 0);

    for (pET = *ppET; pET && pET->zType != zType; pET = pET->pNext);
    if (!pET) {
        pET = SEE_NEW(interp, EventType);
        pET->pListenerList = 0;
        pET->zType = zType;
        pET->pNext = *ppET;
        *ppET = pET;
    }

    /* Check that this is not an attempt to insert a duplicate 
     * event-listener. From the DOM Level 2 spec:
     *
     *     "If multiple identical EventListeners are registered on the same
     *     EventTarget with the same parameters the duplicate instances are
     *     discarded."
     */
    for (
        pL = pET->pListenerList; 
        pL && (pL->isCapture != useCapture || pL->pListener != pListener); 
        pL = pL->pNext
    );
    if (pL) {
        return;
    }

    pL = SEE_NEW(interp, ListenerContainer);
    pL->pNext = pET->pListenerList;
    pET->pListenerList = pL;
    pL->isCapture = useCapture;
    pL->pListener = pListener;

    /* DOM says return value is "void" */
    SEE_SET_UNDEFINED(res);
}

/*
 *---------------------------------------------------------------------------
 *
 * removeEventListenerFunc --
 *
 *     Implementation of DOM method EventTarget.removeEventListener():
 *
 *         void removeEventListener(
 *             in DOMString type, 
 *             in EventListener listener, 
 *             in boolean useCapture
 *         );
 * Results: 
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void
removeEventListenerFunc(interp, self, thisobj, argc, argv, res)
        struct SEE_interpreter *interp;
        struct SEE_object *self, *thisobj;
        int argc;
        struct SEE_value **argv, *res;
{
    EventType *pET;
    EventType **ppET;

    /* Parameters passed to javascript function */
    SEE_boolean_t useCapture = 0;
    struct SEE_string *zType = 0;
    struct SEE_object *pListener = 0;

    /* Check the number of function arguments. */
    if (argc != 3) {
        const char *zErr = "Function requires exactly 3 parameters";
        SEE_error_throw(interp, interp->Error, zErr);
    }

    /* Check that thisobj is a Tcl based object. */
    ppET = getEventList(interp, thisobj);
    if (!ppET) {
        const char *zErr = "Bad type for thisobj";
        SEE_error_throw(interp, interp->Error, zErr);
        return;
    }

    /* Parse the arguments */
#if 0
    SEE_parse_args(interp, argc, argv, "s|o|b|",&zType,&pListener,&useCapture);
#endif
    SEE_parse_args(interp, 1, argv, "s",&zType);
    SEE_parse_args(interp, 1, &argv[1], "o",&pListener);
    SEE_parse_args(interp, 1, &argv[2], "b",&useCapture);
    zType = SEE_intern(interp, zType);
    useCapture = (useCapture ? 1 : 0);

    for (pET = *ppET; pET && pET->zType != zType; pET = pET->pNext);
    if (pET) {
        ListenerContainer **ppL = &pET->pListenerList;
        ListenerContainer *pL;
        for (pL = *ppL; pL; pL = pL->pNext) {
            if (pL->isCapture == useCapture && pL->pListener == pListener) {
                *ppL = pL->pNext;
            } else {
                ppL = &pL->pNext;
            }
        }
    }

    /* DOM says return value is "void" */
    SEE_SET_UNDEFINED(res);
}

/*
 *---------------------------------------------------------------------------
 *
 * eventTargetInit --
 *
 *     This function initialises the events sub-system for the
 *     SeeTclObject passed as an argument. In practice, this means
 *     it evaluates the Tcl script:
 *
 *         eval $obj Events
 *
 *     where $obj is the Tcl command implementing the object. The
 *     return value is expected to be a list of alternating attribute 
 *     names and values. Each value is compiled to a javascript function
 *     and inserted into SeeTclObject.pNative using the supplied attribute
 *     name. For example, if the [Events] script returns:
 *
 *         onclick {alert("click!"} ondblclick {alert("dblclick!")}
 *  
 *     The "onclick" and "ondblclick" properties of SeeTclObject.pNative
 *
 *     are set to the following objects, respectively:
 *
 *         function (event) { alert("click!") }
 *         function (event) { alert("dblclick!") }
 *
 *     Also, add entries to SeeTclObject.pNative for the following 
 *     built-in object methods (DOM Interface EventTarget):
 *
 *         dispatchEvent()
 *         addEventListener()
 *         removeEventListener()
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
eventTargetInit(pTclSeeInterp, p)
    SeeInterp *pTclSeeInterp;
    SeeTclObject *p;
{
    struct SEE_interpreter *pSee = (struct SEE_interpreter *)pTclSeeInterp;
    Tcl_Interp *pTcl = pTclSeeInterp->pTclInterp;
    Tcl_Obj *pList;
    Tcl_Obj **apWord;
    int nWord;
    int ii;

    struct SEE_scope *pScope = 0;

    int rc;
    rc = callSeeTclMethod(pTcl, 0, p, "Events", 0, 0);
    if (rc != TCL_OK) {
        Tcl_BackgroundError(pTcl);
        return;
    }

    pList = Tcl_GetObjResult(pTcl);
    rc = Tcl_ListObjGetElements(pTcl, pList, &nWord, &apWord);
    if (rc != TCL_OK) {
        Tcl_BackgroundError(pTcl);
        return;
    }
    Tcl_IncrRefCount(pList);

    if (nWord > 0) {
        int nS;
        Tcl_Obj **apS;
        Tcl_Obj *pScopeList;

        rc = callSeeTclMethod(pTcl, 0, p, "Scope", 0, 0);
        if (rc != TCL_OK) {
            Tcl_BackgroundError(pTcl);
            return;
        }

        pScopeList = Tcl_GetObjResult(pTcl);
        rc = Tcl_ListObjGetElements(pTcl, pScopeList, &nS, &apS);
        if (rc != TCL_OK) {
            Tcl_DecrRefCount(pList);
            Tcl_BackgroundError(pTcl);
            return;
        }
        Tcl_IncrRefCount(pScopeList);

        pScope = (struct SEE_scope *)SEE_malloc(pSee, sizeof(*pScope) * (nS+1));
        for (ii = 0; ii < nS; ii++){
            pScope[ii].obj = findOrCreateObject(pTclSeeInterp, apS[ii], 0);
            pScope[ii].next = &pScope[ii+1];
        }
        pScope[nS].obj = pSee->Global;
        pScope[nS].next = 0;
    }

    for (ii = 0; ii < (nWord-1); ii += 2){
        Tcl_Obj *pJ;
        struct SEE_input *pInputCode;
        struct SEE_interpreter *pSee = &pTclSeeInterp->interp;
        SEE_try_context_t try_ctxt;

        struct SEE_object *pNative = (struct SEE_object *)p->pNative;
        const char *zAttr   = Tcl_GetString(apWord[ii]);
        const char *zScript = Tcl_GetString(apWord[ii+1]);

        /* Construct a string like this:
         *
         *   this.$zAttr = function (event) { $zScript }
         *
         * We then evaluate the script with the "this" object set to the
         * object we are trying to attach the legacy event handler to.
         */
        pJ = Tcl_NewStringObj("this.", -1);
        Tcl_IncrRefCount(pJ);
        Tcl_AppendToObj(pJ, zAttr, -1);
        Tcl_AppendToObj(pJ, " = function (event) { ", -1);
        Tcl_AppendToObj(pJ, zScript, -1);
        Tcl_AppendToObj(pJ, " } ", -1);
        /* printf("%s\n", Tcl_GetString(pJ)); */

        pInputCode = SEE_input_utf8(&pTclSeeInterp->interp, Tcl_GetString(pJ));

        SEE_TRY(pSee, try_ctxt) {
          SEE_eval(pSee, pInputCode, pNative, pNative, pScope, 0);
        }
        /* Not a lot we can do with an error here... */

        SEE_INPUT_CLOSE(pInputCode);
        Tcl_DecrRefCount(pJ);
    }
    Tcl_DecrRefCount(pList);
    Tcl_ResetResult(pTcl);

    if (!pTclSeeInterp->pEventPrototype) {
        struct SEE_object *pP = SEE_native_new(pSee);
        struct EventFunc {
            const char *zName;
            SEE_call_fn_t xFunc;
            int nLength;
        } functions [] = {
            {"dispatchEvent",       dispatchEventFunc,       1},
            {"removeEventListener", removeEventListenerFunc, 3},
            {"addEventListener",    addEventListenerFunc,    3},
            {0, 0, 0}
        };
        for (ii = 0; functions[ii].zName; ii++) {
            struct EventFunc *f = &functions[ii];
            SEE_CFUNCTION_PUTA(pSee, pP, f->zName, f->xFunc, f->nLength, 0);
        }
        pTclSeeInterp->pEventPrototype = pP;
    }

    p->pNative->object.Prototype = pTclSeeInterp->pEventPrototype;
}

/*
 *---------------------------------------------------------------------------
 *
 * listenerToString --
 *
 * Results:
 *     Pointer to allocated Tcl object with ref-count 0.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static Tcl_Obj *
listenerToString(pSeeInterp, pListener)
    struct SEE_interpreter *pSeeInterp;
    struct SEE_object *pListener;
{
    struct SEE_value val;
    struct SEE_value res;

    SEE_OBJECT_DEFAULTVALUE(pSeeInterp, pListener, 0, &val);
    SEE_ToString(pSeeInterp, &val, &res);
    return stringToObj(res.u.string);
}

/*
 *---------------------------------------------------------------------------
 *
 * eventTargetDump --
 *
 *         $see events TCL-COMMAND
 *
 *     This function is used to introspect event-listeners from
 *     the Tcl level. The return value is a list. Each element of
 *     the list takes the following form:
 *
 *       {EVENT-TYPE LISTENER-TYPE JAVASCRIPT}
 *
 *     where EVENT-TYPE is the event-type string passed to [addEventListener]
 *     or [setLegacyListener]. LISTENER-TYPE is one of "legacy", "capturing"
 *     or "non-capturing". JAVASCRIPT is the "tostring" version of the
 *     js object to call to process the event.
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
eventDumpCmd(clientData, pTcl, objc, objv)
    ClientData clientData;
    Tcl_Interp *pTcl;
    int objc;
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    SeeTclObject *p;
    SeeInterp *pTclSeeInterp = (SeeInterp *)clientData;
    struct SEE_interpreter *pInterp = &pTclSeeInterp->interp;
    EventType *pType;
    Tcl_Obj *apRow[3];
    Tcl_Obj *pRet;

    struct SEE_enum *pEnum;
    struct SEE_string *pProp;


    assert(objc == 3);
    p = (SeeTclObject *)findOrCreateObject(pTclSeeInterp, objv[2], 0);

    pRet = Tcl_NewObj();
    Tcl_IncrRefCount(pRet);

    for (pType = p->pTypeList; pType; pType = pType->pNext) {
        ListenerContainer *pL;

        Tcl_Obj *pEventType = Tcl_NewUnicodeObj(
                pType->zType->data, pType->zType->length
        );
        Tcl_IncrRefCount(pEventType);
        apRow[0] = pEventType;

        for (pL = pType->pListenerList; pL; pL = pL->pNext) {
            const char *zType = (pL->isCapture?"capturing":"non-capturing");
            apRow[1] = Tcl_NewStringObj(zType, -1);
            apRow[2] = listenerToString(pInterp, pL->pListener);
            Tcl_ListObjAppendElement(pTcl, pRet, Tcl_NewListObj(3, apRow));
        }

        Tcl_DecrRefCount(pEventType);
    }

    pEnum = SEE_OBJECT_ENUMERATOR(pInterp, (struct SEE_object *)p);
    while ((pProp = SEE_ENUM_NEXT(pInterp, pEnum, 0))) {
        int nProp = SEE_string_utf8_size(pInterp, pProp);
        char *zProp = SEE_malloc_string(pInterp, nProp+1);
        SEE_string_toutf8(pInterp, zProp, nProp+1, pProp);
        if (zProp[0] == 'o' && zProp[1] == 'n') {
            struct SEE_value val;
            SEE_OBJECT_GETA(pInterp, (struct SEE_object *)p, zProp, &val);
            if (SEE_VALUE_GET_TYPE(&val) == SEE_OBJECT) {
                apRow[0] = Tcl_NewStringObj(&zProp[2], -1);
                apRow[1] = Tcl_NewStringObj("legacy", -1);
                apRow[2] = listenerToString(pInterp, val.u.object);
                Tcl_ListObjAppendElement(pTcl, pRet, Tcl_NewListObj(3, apRow));
            }
        }
    }

    Tcl_SetObjResult(pTcl, pRet);
    Tcl_DecrRefCount(pRet);

    return TCL_OK;
}

