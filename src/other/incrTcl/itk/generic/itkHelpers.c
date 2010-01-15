/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tk]
 *  DESCRIPTION:  Building mega-widgets with [incr Tcl]
 *
 *  [incr Tk] provides a framework for building composite "mega-widgets"
 *  using [incr Tcl] classes.  It defines a set of base classes that are
 *  specialized to create all other widgets.
 *
 *  This part adds C implementations for some helxeper methods in the
 *  base class itk::Archetype.
 *
 * ========================================================================
 *  AUTHOR:  Michael J. McLennan
 *           Bell Labs Innovations for Lucent Technologies
 *           mmclennan@lucent.com
 *           http://www.tcltk.com/itcl
 *
 *     RCS:  $Id$
 * ========================================================================
 *           Copyright (c) 1993-1998  Lucent Technologies, Inc.
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include <assert.h>
#include "itkInt.h"


/*
 * ------------------------------------------------------------------------
 *  Itk_ArchOptConfigError()
 *
 *  Simply utility which adds error information after a option
 *  configuration fails.  Adds traceback information to the given
 *  interpreter.
 * ------------------------------------------------------------------------
 */
void
Itk_ArchOptConfigError(
    Tcl_Interp *interp,            /* interpreter handling this object */
    ArchInfo *info,                /* info associated with mega-widget */
    ArchOption *archOpt)           /* configuration option that failed */
{
    Tcl_Obj *objPtr;

    objPtr = Tcl_NewStringObj((char*)NULL, 0);
    Tcl_IncrRefCount(objPtr);

    Tcl_AppendToObj(objPtr, "\n    (while configuring option \"", -1);
    Tcl_AppendToObj(objPtr, archOpt->switchName, -1);
    Tcl_AppendToObj(objPtr, "\"", -1);

    if (info->itclObj && info->itclObj->accessCmd) {
        Tcl_AppendToObj(objPtr, " for widget \"", -1);
        Tcl_GetCommandFullName(interp, info->itclObj->accessCmd, objPtr);
        Tcl_AppendToObj(objPtr, "\")", -1);
    }
    Tcl_AddErrorInfo(interp, Tcl_GetStringFromObj(objPtr, (int*)NULL));
    Tcl_DecrRefCount(objPtr);
}


/*
 * ------------------------------------------------------------------------
 *  Itk_ArchOptAccessError()
 *
 *  Simply utility which adds error information after an option
 *  value access fails.  Adds traceback information to the given
 *  interpreter.
 * ------------------------------------------------------------------------
 */
void
Itk_ArchOptAccessError(
    Tcl_Interp *interp,            /* interpreter handling this object */
    ArchInfo *info,                /* info associated with mega-widget */
    ArchOption *archOpt)           /* option that couldn't be accessed */
{
    Tcl_ResetResult(interp);

    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
        "internal error: cannot access itk_option(", archOpt->switchName, ")",
        (char*)NULL);

    if (info->itclObj->accessCmd) {
        Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);
        Tcl_AppendToObj(resultPtr, " in widget \"", -1);
        Tcl_GetCommandFullName(interp, info->itclObj->accessCmd, resultPtr);
        Tcl_AppendToObj(resultPtr, "\"", -1);
    }
}


/*
 * ------------------------------------------------------------------------
 *  Itk_GetArchInfo()
 *
 *  Finds the extra Archetype info associated with the given object.
 *  Returns TCL_OK and a pointer to the info if found.  Returns
 *  TCL_ERROR along with an error message in interp->result if not.
 * ------------------------------------------------------------------------
 */
int
Itk_GetArchInfo(
    Tcl_Interp *interp,            /* interpreter handling this object */
    ItclObject *contextObj,        /* object with desired data */
    ArchInfo **infoPtr)            /* returns:  pointer to extra info */
{
    Tcl_HashTable *objsWithArchInfo;
    Tcl_HashEntry *entry;


    /*
     *  If there is any problem finding the info, return an error.
     */
    objsWithArchInfo = ItkGetObjsWithArchInfo(interp);
    entry = Tcl_FindHashEntry(objsWithArchInfo, (char*)contextObj);

    if (!entry) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "internal error: no Archetype information for widget",
            (char*)NULL);

        if (contextObj->accessCmd) {
            Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);
            Tcl_AppendToObj(resultPtr, " \"", -1);
            Tcl_GetCommandFullName(interp, contextObj->accessCmd, resultPtr);
            Tcl_AppendToObj(resultPtr, "\"", -1);
        }
        return TCL_ERROR;
    }

    /*
     *  Otherwise, return the requested info.
     */
    *infoPtr = (ArchInfo*)Tcl_GetHashValue(entry);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  ItkGetObjsWithArchInfo()
 *
 *  Returns a pointer to a hash table containing the list of registered
 *  objects in the specified interpreter.  If the hash table does not
 *  already exist, it is created.
 * ------------------------------------------------------------------------
 */
Tcl_HashTable*
ItkGetObjsWithArchInfo(
    Tcl_Interp *interp)  /* interpreter handling this registration */
{
    Tcl_HashTable* objTable;

    /*
     *  If the registration table does not yet exist, then create it.
     */
    objTable = (Tcl_HashTable*)Tcl_GetAssocData(interp,
        "itk_objsWithArchInfo", (Tcl_InterpDeleteProc**)NULL);

    if (!objTable) {
        objTable = (Tcl_HashTable*)ckalloc(sizeof(Tcl_HashTable));
        Tcl_InitHashTable(objTable, TCL_ONE_WORD_KEYS);
        Tcl_SetAssocData(interp, "itk_objsWithArchInfo",
            ItkFreeObjsWithArchInfo, (ClientData)objTable);
    }
    return objTable;
}

/*
 * ------------------------------------------------------------------------
 *  ItkFreeObjsWithArchInfo()
 *
 *  When an interpreter is deleted, this procedure is called to
 *  free up the associated data created by ItkGetObjsWithArchInfo.
 * ------------------------------------------------------------------------
 */
void
ItkFreeObjsWithArchInfo(clientData, interp)
    ClientData clientData;       /* associated data */
    Tcl_Interp *interp;          /* interpreter being freed */
{
    Tcl_HashTable *tablePtr = (Tcl_HashTable*)clientData;
    Tcl_HashSearch place;
    Tcl_HashEntry *entry;

    entry = Tcl_FirstHashEntry(tablePtr, &place);
    while (entry) {
        Itk_DelArchInfo( Tcl_GetHashValue(entry) );
        entry = Tcl_NextHashEntry(&place);
    }

    Tcl_DeleteHashTable(tablePtr);
    ckfree((char*)tablePtr);
}

