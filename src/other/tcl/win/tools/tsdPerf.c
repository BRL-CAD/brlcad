#include <tcl.h>

static Tcl_ThreadDataKey key;

typedef struct {
    int value;
} TsdPerf;


int
tsdPerfSetObjCmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
    TsdPerf *perf = Tcl_GetThreadData(&key, sizeof(TsdPerf));
    int i;

    if (2 != objc) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
    }

    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[1], &i)) {
	return TCL_ERROR;
    }

    perf->value = i;

    return TCL_OK;
}

int tsdPerfGetObjCmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
    TsdPerf *perf = Tcl_GetThreadData(&key, sizeof(TsdPerf));


    Tcl_SetObjResult(interp, Tcl_NewIntObj(perf->value));

    return TCL_OK;
}


int
Tsdperf_Init (Tcl_Interp *interp) {
    if (NULL == Tcl_InitStubs(interp, TCL_VERSION, 0)) {
	return TCL_ERROR;
    }


    Tcl_CreateObjCommand(interp, "tsdPerfSet", tsdPerfSetObjCmd, (ClientData)NULL,
			 (Tcl_CmdDeleteProc *)NULL);
    Tcl_CreateObjCommand(interp, "tsdPerfGet", tsdPerfGetObjCmd, (ClientData)NULL,
			 (Tcl_CmdDeleteProc *)NULL);


    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
