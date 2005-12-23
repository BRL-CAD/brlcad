#include "bltInt.h"

#ifndef linux
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif /* HAVE_MALLOC_H */
#endif

/*
 * Blt_MallocProcPtr, Blt_FreeProcPtr --
 *
 *	These global variables allow you to override the default
 *	memory allocation/deallocation routines, simply by setting the
 *	pointers to your own C functions.  By default, we try to use
 *	the same memory allocation scheme that Tcl is using: generally
 *	that's Tcl_Alloc and Tcl_Free.
 */
#ifdef WIN32

#ifdef __GNUC__
extern char *Tcl_Alloc _ANSI_ARGS_((unsigned int size));
extern void Tcl_Free _ANSI_ARGS_((char * ptr));
extern char *Tcl_Realloc _ANSI_ARGS_((char *ptr, unsigned int size));
#endif /*__GNUC__*/

Blt_MallocProc *Blt_MallocProcPtr = (Blt_MallocProc *)Tcl_Alloc;
Blt_FreeProc *Blt_FreeProcPtr = (Blt_FreeProc *)Tcl_Free; 
Blt_ReallocProc *Blt_ReallocProcPtr = (Blt_ReallocProc *)Tcl_Realloc; 

#else

/* 
 * Try to use the same memory allocator/deallocator that Tcl is
 * using. Before 8.1 it used malloc/free.
 */

#if (TCL_VERSION_NUMBER >= _VERSION(8,1,0)) 
/* 
 * We're pointing to the private TclpAlloc/TclpFree instead of public
 * Tcl_Alloc/Tcl_Free routines because they don't automatically cause
 * a panic when not enough memory is available. There are cases (such 
 * as allocating a very large vector) where it's recoverable.
 */
EXTERN Blt_MallocProc TclpAlloc;
EXTERN Blt_FreeProc TclpFree;
EXTERN Blt_ReallocProc TclpRealloc;

Blt_MallocProc *Blt_MallocProcPtr = TclpAlloc;
Blt_FreeProc *Blt_FreeProcPtr = TclpFree; 
Blt_ReallocProc *Blt_ReallocProcPtr = TclpRealloc; 
#else

Blt_MallocProc *Blt_MallocProcPtr = malloc;
Blt_FreeProc *Blt_FreeProcPtr = free; 
Blt_ReallocProc *Blt_ReallocProcPtr = realloc;

#endif /* >= 8.1.0 */
#endif /* WIN32 */

void *
Blt_Calloc(nElems, sizeOfElem)
    unsigned int nElems; 
    size_t sizeOfElem;
{
    char *ptr;
    size_t size;

    size = nElems * sizeOfElem;
    ptr = Blt_Malloc(size);
    if (ptr != NULL) {
	memset(ptr, 0, size);
    }
    return ptr;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_Strdup --
 *
 *      Create a copy of the string from heap storage.
 *
 * Results:
 *      Returns a pointer to the need string copy.
 *
 *----------------------------------------------------------------------
 */
char *
Blt_Strdup(string)
    CONST char *string;
{
    size_t size;
    char *ptr;

    size = strlen(string) + 1;
    ptr = Blt_Malloc(size * sizeof(char));
    if (ptr != NULL) {
	strcpy(ptr, string);
    }
    return ptr;
}

