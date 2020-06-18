/*              T C L _ T E V E N T . C P P
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file tcl_tevent.cpp
 *
 */

#include "common.h"

#include <iostream>

#include "tcl.h"
#include "tk.h"

#include "bu/app.h"

TCL_DECLARE_MUTEX(calclock)
TCL_DECLARE_MUTEX(threadMutex)

struct cdata {
    int val1;
    int val2;
    int val3;
    int shutdown;
    Tcl_ThreadId parent_id;
    Tcl_ThreadId worker_id;
    Tcl_Interp *parent_interp;
};

struct EventResult {
    Tcl_Condition done;
    int ret;
};

struct UpdateEvent {
    Tcl_Event event;            /* Must be first */
    struct EventResult *result;
    struct cdata *d;
};

struct CalcEvent {
    Tcl_Event event;            /* Must be first */
    struct EventResult *result;
    struct cdata *d;
};


static int
UpdateProc1(Tcl_Event *evPtr, int UNUSED(mask))
{
    struct UpdateEvent *EventPtr = (UpdateEvent *) evPtr;
    struct cdata *d = EventPtr->d;

    Tcl_MutexLock(&calclock);
    d->val1 = d->val1 + 1000;
    Tcl_MutexUnlock(&calclock);

    std::cout << "val1: " << d->val1 << "\n";

    // Return one to signify a successful completion of the process execution
    return 1;
}

static int
UpdateProc3(Tcl_Event *evPtr, int UNUSED(mask))
{
    struct UpdateEvent *EventPtr = (UpdateEvent *) evPtr;
    struct cdata *d = EventPtr->d;

    Tcl_MutexLock(&calclock);
    d->val3 = d->val3 + 2000;
    Tcl_MutexUnlock(&calclock);

    std::cout << "val3: " << d->val3 << "\n";

    // Return one to signify a successful completion of the process execution
    return 1;
}

static Tcl_ThreadCreateType
Calc_Thread(ClientData clientData)
{
    struct cdata *d = (struct cdata *)clientData;
    Tcl_Interp *interp = Tcl_CreateInterp();
    Tcl_Init(interp);

    while (!d->shutdown) {

	Tcl_DoOneEvent(0);

	Tcl_MutexLock(&calclock);
	d->val2 = d->val2 + 500;
	Tcl_MutexUnlock(&calclock);

	std::cout << "val2: " << d->val2 << "\n";
	Tcl_Sleep(100);

	// Generate an event for the parent thread to let it know to update
	// based on the calculation result
	struct UpdateEvent *threadEventPtr = (struct UpdateEvent *)ckalloc(sizeof(UpdateEvent));
	threadEventPtr->d = d;
	threadEventPtr->event.proc = UpdateProc3;
	Tcl_MutexLock(&threadMutex);
	Tcl_ThreadQueueEvent(d->parent_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
	Tcl_ThreadAlert(d->parent_id);
	Tcl_MutexUnlock(&threadMutex);
    }

    // We're well and truly done - the application is closing down
    Tcl_ExitThread(TCL_OK);
    TCL_THREAD_CREATE_RETURN;
}

int
calc_f1(ClientData clientData, Tcl_Interp *UNUSED(interp), int UNUSED(argc), char **UNUSED(argv))
{
    struct cdata *d = (struct cdata *)clientData;
    // Generate an event for the worker thread to let it know to update
    // based on the calculation result
    struct UpdateEvent *threadEventPtr = (struct UpdateEvent *)ckalloc(sizeof(UpdateEvent));
    threadEventPtr->d = d;
    threadEventPtr->event.proc = UpdateProc1;
    Tcl_MutexLock(&threadMutex);
    Tcl_ThreadQueueEvent(d->worker_id, (Tcl_Event *) threadEventPtr, TCL_QUEUE_TAIL);
    Tcl_ThreadAlert(d->worker_id);
    Tcl_MutexUnlock(&threadMutex);

    return TCL_OK;
}

int
main(int UNUSED(argc), const char *argv[])
{

    bu_setprogname(argv[0]);

    struct cdata ddata;
    ddata.val1 = 0;
    ddata.val2 = 0;
    ddata.val3 = 0;
    ddata.shutdown = 0;
    ddata.parent_id = Tcl_GetCurrentThread();

    // Set up Tcl/Tk
    Tcl_FindExecutable(argv[0]);
    Tcl_Interp *interp = Tcl_CreateInterp();
    Tcl_Init(interp);
    Tk_Init(interp);

    // Save a pointer so we can get at the interp later
    ddata.parent_interp = interp;

    // Make a simple toplevel window
    Tk_Window tkwin = Tk_MainWindow(interp);
    Tk_GeometryRequest(tkwin, 512, 512);
    Tk_MakeWindowExist(tkwin);
    Tk_MapWindow(tkwin);

    // Whenever the window changes, call the incrementer
    (void)Tcl_CreateCommand(interp, "calc1", (Tcl_CmdProc *)calc_f1, (ClientData)&ddata, (Tcl_CmdDeleteProc* )NULL);
    Tcl_Eval(interp, "bind . <Configure> {calc1}");

    // Multithreading experiment
    Tcl_ThreadId threadID;
    if (Tcl_CreateThread(&threadID, Calc_Thread, (ClientData)&ddata, TCL_THREAD_STACK_DEFAULT, TCL_THREAD_JOINABLE) != TCL_OK) {
	std::cerr << "can't create thread\n";
    }
    ddata.worker_id = threadID;

    // Enter the main application loop - the initial image will appear, and Button-1 mouse
    // clicks on the window should generate and display new images
    while (1) {
	Tcl_DoOneEvent(0);
	if (ddata.val3 > 100000 && ddata.val2 > 100000) {
	    Tcl_MutexLock(&calclock);
	    ddata.shutdown = 1;
	    Tcl_MutexUnlock(&calclock);
	    int tret;
	    Tcl_JoinThread(threadID, &tret);
	    if (tret != TCL_OK) {
		std::cerr << "Failure to shut down work thread\n";
	    } else {
		std::cout << "Successful work thread shutdown\n";
	    }
	    exit(0);
	}
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
