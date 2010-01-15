# itk.decls --
#
#	This file contains the declarations for all supported public
#	functions that are exported by the Itk library via the stubs table.
#	This file is used to generate the itkDecls.h file.
#	
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: $Id$

# public API
library itk
interface itk
hooks {itkInt}
epoch 0
scspec ITKAPI

# Declare each of the functions in the public Itk interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.


#
#  Exported functions:
#

declare 0 current {
    int Itk_Init (Tcl_Interp *interp)
}
declare 1 current {
    int Itk_SafeInit (Tcl_Interp *interp)
}


#
#  Functions needed for the Archetype base class:
#

declare 13 current {
    int Itk_ArchetypeInit (Tcl_Interp* interp)
}



# private API
interface itkInt

#
# Functions used within the package, but not considered "public"
#

#
#  Functions used internally by this package:
#

declare 2 current {
    int Itk_ConfigBodyCmd (ClientData cdata, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 3 current {
    int Itk_UsualCmd (ClientData cdata, Tcl_Interp *interp, int objc, \
        Tcl_Obj *CONST objv[])
}

#
#  Functions for managing options included in class definitions:
#

declare 4 current {
    int Itk_ClassOptionDefineCmd (ClientData cdata, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 5 current {
    int Itk_ClassOptionIllegalCmd (ClientData cdata, Tcl_Interp *interp, \
        int objc, Tcl_Obj *CONST objv[])
}
declare 6 current {
    int Itk_ConfigClassOption (Tcl_Interp *interp, ItclObject *contextObj, \
        ClientData cdata, CONST char* newVal)
}
declare 7 current {
    ItkClassOptTable* Itk_CreateClassOptTable( Tcl_Interp *interp, \
        ItclClass *cdefn)
}
declare 8 current {
    ItkClassOptTable* Itk_FindClassOptTable (ItclClass *cdefn)
}
#declare 9 current {
#    void Itk_DeleteClassOptTable (Tcl_Interp *interp, ItclClass *cdefn)
#}
declare 10 current {
    int Itk_CreateClassOption (Tcl_Interp *interp, ItclClass *cdefn, \
        char *switchName, char *resName, char *resClass, char *defVal, \
        char *config, ItkClassOption **optPtr)
}
declare 11 current {
    ItkClassOption* Itk_FindClassOption (ItclClass *cdefn, char *switchName)
}
declare 12 current {
    void Itk_DelClassOption (ItkClassOption *opt)
}

#
#  Functions for maintaining the ordered option list:
#

declare 14 current {
    void Itk_OptListInit (ItkOptList* olist, Tcl_HashTable *options)
}
declare 15 current {
    void Itk_OptListFree (ItkOptList* olist)
}
declare 16 current {
    void Itk_OptListAdd (ItkOptList* olist, Tcl_HashEntry *entry)
}
declare 17 current {
    void Itk_OptListRemove (ItkOptList* olist, Tcl_HashEntry *entry)
}


