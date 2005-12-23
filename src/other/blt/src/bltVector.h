
/*
 * bltVector.h --
 *
 * Copyright 1993-2000 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 */

#ifndef _BLT_VECTOR_H
#define _BLT_VECTOR_H

typedef enum {
    BLT_VECTOR_NOTIFY_UPDATE = 1, /* The vector's values has been updated */
    BLT_VECTOR_NOTIFY_DESTROY	/* The vector has been destroyed and the client
				 * should no longer use its data (calling
				 * Blt_FreeVectorId) */
} Blt_VectorNotify;

typedef struct Blt_VectorIdStruct *Blt_VectorId;

typedef void (Blt_VectorChangedProc) _ANSI_ARGS_((Tcl_Interp *interp,
	ClientData clientData, Blt_VectorNotify notify));

typedef struct {
    double *valueArr;		/* Array of values (possibly malloc-ed) */
    int numValues;		/* Number of values in the array */
    int arraySize;		/* Size of the allocated space */
    double min, max;		/* Minimum and maximum values in the vector */
    int dirty;			/* Indicates if the vector has been updated */
    int reserved;		/* Reserved for future use */

} Blt_Vector;

typedef double (Blt_VectorIndexProc) _ANSI_ARGS_((Blt_Vector * vecPtr));

typedef enum {
    BLT_MATH_FUNC_SCALAR = 1,	/* The function returns a single double
				 * precision value. */
    BLT_MATH_FUNC_VECTOR	/* The function processes the entire vector. */
} Blt_MathFuncType;

/*
 * To be safe, use the macros below, rather than the fields of the
 * structure directly.
 *
 * The Blt_Vector is basically an opaque type.  But it's also the
 * actual memory address of the vector itself.  I wanted to make the
 * API as unobtrusive as possible.  So instead of giving you a copy of
 * the vector, providing various functions to access and update the
 * vector, you get your hands on the actual memory (array of doubles)
 * shared by all the vector's clients.
 *
 * The trade-off for speed and convenience is safety.  You can easily
 * break things by writing into the vector when other clients are
 * using it.  Use Blt_ResetVector to get around this.  At least the
 * macros are a reminder it isn't really safe to reset the data
 * fields, except by the API routines.  
 */
#define Blt_VecData(v)		((v)->valueArr)
#define Blt_VecLength(v)	((v)->numValues)
#define Blt_VecSize(v)		((v)->arraySize)
#define Blt_VecDirty(v)		((v)->dirty)

EXTERN double Blt_VecMin _ANSI_ARGS_((Blt_Vector *vPtr));
EXTERN double Blt_VecMax _ANSI_ARGS_((Blt_Vector *vPtr));

EXTERN Blt_VectorId Blt_AllocVectorId _ANSI_ARGS_((Tcl_Interp *interp,
	char *vecName));

EXTERN void Blt_SetVectorChangedProc _ANSI_ARGS_((Blt_VectorId clientId,
	Blt_VectorChangedProc * proc, ClientData clientData));

EXTERN void Blt_FreeVectorId _ANSI_ARGS_((Blt_VectorId clientId));

EXTERN int Blt_GetVectorById _ANSI_ARGS_((Tcl_Interp *interp,
	Blt_VectorId clientId, Blt_Vector **vecPtrPtr));

EXTERN char *Blt_NameOfVectorId _ANSI_ARGS_((Blt_VectorId clientId));

EXTERN char *Blt_NameOfVector _ANSI_ARGS_((Blt_Vector *vecPtr));

EXTERN int Blt_VectorNotifyPending _ANSI_ARGS_((Blt_VectorId clientId));

EXTERN int Blt_CreateVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName,
	int size, Blt_Vector ** vecPtrPtr));

EXTERN int Blt_GetVector _ANSI_ARGS_((Tcl_Interp *interp, char *vecName,
	Blt_Vector **vecPtrPtr));

EXTERN int Blt_VectorExists _ANSI_ARGS_((Tcl_Interp *interp, char *vecName));

EXTERN int Blt_ResetVector _ANSI_ARGS_((Blt_Vector *vecPtr, double *dataArr,
	int nValues, int arraySize, Tcl_FreeProc *freeProc));

EXTERN int Blt_ResizeVector _ANSI_ARGS_((Blt_Vector *vecPtr, int nValues));

EXTERN int Blt_DeleteVectorByName _ANSI_ARGS_((Tcl_Interp *interp,
	char *vecName));

EXTERN int Blt_DeleteVector _ANSI_ARGS_((Blt_Vector *vecPtr));

EXTERN int Blt_ExprVector _ANSI_ARGS_((Tcl_Interp *interp, char *expression,
	Blt_Vector *vecPtr));

EXTERN void Blt_InstallIndexProc _ANSI_ARGS_((Tcl_Interp *interp, 
	char *indexName, Blt_VectorIndexProc * procPtr));

#endif /* _BLT_VECTOR_H */
