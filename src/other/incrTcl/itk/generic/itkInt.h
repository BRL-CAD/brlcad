/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tk]
 *  DESCRIPTION:  Building mega-widgets with [incr Tcl]
 *
 *  [incr Tk] provides a framework for building composite "mega-widgets"
 *  using [incr Tcl] classes.  It defines a set of base classes that are
 *  specialized to create all other widgets.
 *
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

#include <itclInt.h>
#include "itk.h"

/*
 *  List of options in alphabetical order:
 */
typedef struct ItkOptList {
    Tcl_HashTable *options;     /* list containing the real options */
    Tcl_HashEntry **list;       /* gives ordering of options */
    int len;                    /* number of entries in order list */
    int max;                    /* maximum size of order list */
} ItkOptList;

/*
 *  List of options created in the class definition:
 */
typedef struct ItkClassOptTable {
    Tcl_HashTable options;        /* option storage with fast lookup */
    ItkOptList order;             /* gives ordering of options */
} ItkClassOptTable;

/*
 *  Each option created in the class definition:
 */
typedef struct ItkClassOption {
    Tcl_Obj* namePtr;           /* member name */
    Tcl_Obj* fullNamePtr;       /* member name with "class::" qualifier */
    ItclClass* iclsPtr;         /* class containing this member */
    int protection;             /* protection level */
    int flags;                  /* flags describing member (see above) */
    ItclMemberCode *codePtr;    /* code associated with member */
    char *resName;                /* resource name in X11 database */
    char *resClass;               /* resource class name in X11 database */
    char *init;                   /* initial value for option */
} ItkClassOption;


/*
 *  Info associated with each Archetype mega-widget:
 */
typedef struct ArchInfo {
    ItclObject *itclObj;        /* object containing this info */
    Tk_Window tkwin;            /* window representing this mega-widget */
    Tcl_HashTable components;   /* list of all mega-widget components */
    Tcl_HashTable options;      /* list of all mega-widget options */
    ItkOptList order;           /* gives ordering of options */
} ArchInfo;

/*
 *  Each component widget in an Archetype mega-widget:
 */
typedef struct ArchComponent {
    Tcl_Obj* namePtr;           /* member name */
    Tcl_Obj* fullNamePtr;       /* member name with "class::" qualifier */
    ItclClass* iclsPtr;         /* class containing this member */
    int protection;             /* protection level */
    int flags;                  /* flags describing member (see above) */
    ItclMemberCode *codePtr;    /* code associated with member */
    Tcl_Command accessCmd;      /* access command for component widget */
    Tk_Window tkwin;            /* Tk window for this component widget */
    char *pathName;             /* Tk path name for this component widget.
                                   We can't use the tkwin pointer after
                                   the window has been destroyed so we
                                   need to save a copy for use in
                                   Itk_ArchCompDeleteCmd() */
} ArchComponent;

/*
 *  Each option in an Archetype mega-widget:
 */
typedef struct ArchOption {
    char *switchName;           /* command-line switch for this option */
    char *resName;              /* resource name in X11 database */
    char *resClass;             /* resource class name in X11 database */
    char *init;                 /* initial value for option */
    int flags;                  /* flags representing option state */
    Itcl_List parts;            /* parts relating to this option */
} ArchOption;

/*
 *  Flag bits for ArchOption state:
 */
#define ITK_ARCHOPT_INIT  0x01  /* option has been initialized */

/*
 *  Various parts of a composite option in an Archetype mega-widget:
 */
typedef int (Itk_ConfigOptionPartProc) _ANSI_ARGS_((Tcl_Interp *interp,
    ItclObject *contextObj, ClientData cdata, CONST char* newVal));

typedef struct ArchOptionPart {
    ClientData clientData;                 /* data associated with this part */
    Itk_ConfigOptionPartProc *configProc;  /* update when new vals arrive */
    Tcl_CmdDeleteProc *deleteProc;         /* clean up after clientData */

    ClientData from;                       /* token that indicates who
                                            * contributed this option part */
} ArchOptionPart;


/*
 *  Info kept by the itk::option-parser namespace and shared by
 *  all option processing commands:
 */
typedef struct ArchMergeInfo {
    Tcl_HashTable usualCode;      /* usual option handling code for the
                                   * various widget classes */

    ArchInfo *archInfo;           /* internal option info for mega-widget */
    ArchComponent *archComp;      /* component being merged into mega-widget */
    Tcl_HashTable *optionTable;   /* table of valid configuration options
                                   * for component being merged */
} ArchMergeInfo;

/*
 *  Used to capture component widget configuration options when a
 *  new component is being merged into a mega-widget:
 */
typedef struct GenericConfigOpt {
    char *switchName;             /* command-line switch for this option */
    char *resName;                /* resource name in X11 database */
    char *resClass;               /* resource class name in X11 database */
    char *init;                   /* initial value for this option */
    char *value;                  /* current value for this option */
    char **storage;               /* storage for above strings */

    ArchOption *integrated;       /* integrated into this mega-widget option */
    ArchOptionPart *optPart;      /* integrated as this option part */
} GenericConfigOpt;

/*
 *  Options that are propagated by a "configure" method:
 */
typedef struct ConfigCmdline {
    Tcl_Obj *objv[4];           /* objects representing "configure" command */
} ConfigCmdline;

MODULE_SCOPE int Itk_ArchInitOptsCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchDeleteOptsCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchComponentCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchInitCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchOptionCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchCompAccessCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchConfigureCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchCgetCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));

MODULE_SCOPE int Itk_ArchCompAddCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchCompDeleteCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchConfigOption _ANSI_ARGS_((Tcl_Interp *interp,
    ArchInfo *info, char *name, char *value));
MODULE_SCOPE int Itk_ArchOptionAddCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchOptionRemoveCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_PropagatePublicVar _ANSI_ARGS_((Tcl_Interp *interp,
    ItclObject *contextObj, ClientData cdata, CONST char *newval));
MODULE_SCOPE int Itk_GetArchInfo _ANSI_ARGS_((Tcl_Interp *interp,
    ItclObject* contextObj, ArchInfo **infoPtr));
MODULE_SCOPE void Itk_ArchOptConfigError _ANSI_ARGS_((Tcl_Interp *interp,
    ArchInfo *info, ArchOption *archOpt));
MODULE_SCOPE void Itk_ArchOptAccessError _ANSI_ARGS_((Tcl_Interp *interp,
    ArchInfo *info, ArchOption *archOpt));
MODULE_SCOPE ArchOptionPart* Itk_CreateOptionPart _ANSI_ARGS_((
    Tcl_Interp *interp, ClientData cdata, Itk_ConfigOptionPartProc* cproc,
    Tcl_CmdDeleteProc *dproc, ClientData from));
MODULE_SCOPE int Itk_AddOptionPart _ANSI_ARGS_((Tcl_Interp *interp,
    ArchInfo *info, char *switchName, char *resName, char *resClass,
    CONST char *defVal, char *currVal, ArchOptionPart *optPart,
    ArchOption **raOpt));
MODULE_SCOPE ArchOptionPart* Itk_FindArchOptionPart _ANSI_ARGS_((
    ArchInfo *info, char *switchName, ClientData from));
MODULE_SCOPE void Itk_DelOptionPart _ANSI_ARGS_((ArchOptionPart *optPart));
MODULE_SCOPE void Itk_DelArchInfo _ANSI_ARGS_((ClientData cdata));
MODULE_SCOPE Tcl_HashTable* ItkGetObjsWithArchInfo
    _ANSI_ARGS_((Tcl_Interp *interp));
MODULE_SCOPE void ItkFreeObjsWithArchInfo _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp));
MODULE_SCOPE void Itk_DelMergeInfo _ANSI_ARGS_((char* cdata));
MODULE_SCOPE int Itk_ArchOptKeepCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchOptIgnoreCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchOptRenameCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
MODULE_SCOPE int Itk_ArchOptUsualCmd _ANSI_ARGS_((ClientData cdata,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));


#include "itkIntDecls.h"
