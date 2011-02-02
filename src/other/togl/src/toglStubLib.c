#ifndef   USE_TCL_STUBS
#  define USE_TCL_STUBS
#endif
#undef USE_TCL_STUB_PROCS
#ifndef   USE_TK_STUBS
#  define USE_TK_STUBS
#endif
#undef USE_TK_STUB_PROCS

#include "togl.h"

ToglStubs *toglStubsPtr;

/* 
 ** Ensure that Togl_InitStubs is built as an exported symbol.  The other stub
 ** functions should be built as non-exported symbols.
 */
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

/* 
 * Togl_InitStubs --
 *
 *    Checks that the correct version of Togl is loaded and that it
 *    supports stubs.  It then initialises the stub table pointers.
 *
 * Results:
 *     The actual version of Togl that satisfies the request, or
 *     NULL to indicate that an error occurred.
 *
 * Side effects:
 *     sets the stub table pointer.
 *
 */

#ifdef Togl_InitStubs
#  undef Togl_InitStubs
#endif

const char *
Togl_InitStubs(Tcl_Interp *interp, const char *version, int exact)
{
    const char *actualVersion;

    actualVersion = Tcl_PkgRequireEx(interp, "Togl", version, exact,
            (ClientData *) &toglStubsPtr);
    if (!actualVersion) {
        return NULL;
    }

    if (!toglStubsPtr) {
        Tcl_SetResult(interp,
                "This implementation of Togl does not support stubs",
                TCL_STATIC);
        return NULL;
    }

    return actualVersion;
}
