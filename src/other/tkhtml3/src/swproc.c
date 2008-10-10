
#include <tcl.h>
#include <assert.h>
#include <string.h>
#include "swproc.h"

static const char rcsid[] = "$Id: swproc.c,v 1.6 2006/06/10 12:38:38 danielk1977 Exp $";

/*
 *---------------------------------------------------------------------------
 *
 * SwprocRt --
 *
 *     This function is used to interpret the arguments passed to a Tcl
 *     command. The assumption is that Tcl commands take three types of
 *     arguments:
 *
 *         * Regular arguments, the type interpeted automatically by
 *           [proc]. When using this function, regular arguments may not
 *           have default values.
 *
 *         * Switches that take arguments.
 *
 *         * Switches that do not require an argument (called "options").
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
SwprocRt(interp, objc, objv, aConf, apObj)
    Tcl_Interp *interp;               /* Tcl interpreter */
    int objc;
    Tcl_Obj *CONST objv[];
    SwprocConf *aConf;
    Tcl_Obj **apObj;
{
    SwprocConf *pConf;
    int ii;
    int jj;
    int argsatend = 0;
    int argcnt = 0;       /* Number of compulsory arguments */
    int firstarg;         /* Index of first compulsory arg in aConf */
    int lastswitch;       /* Index of element after last switch or option */
    char const *zSwitch;

    /* Set all the entries in apObj[] to 0. This makes cleaning up in the
     * case of an error easier. Also, check whether the compulsory
     * arguments (if any) are at the start or end of the array. Set argcnt
     * to the number of compulsory args.
     */
    argsatend = (aConf[0].eType == SWPROC_ARG) ? 0 : 1;
    for (jj = 0; aConf[jj].eType != SWPROC_END; jj++) {
        apObj[jj] = 0;
        if (aConf[jj].eType == SWPROC_ARG) {
            argcnt++;
        }
    }

    /* Set values of compulsory arguments. Also set all switches and
     * options to their default values. 
     */
    firstarg = argsatend ? (objc - argcnt) : 0;
    ii = firstarg;
    for (jj = 0; aConf[jj].eType != SWPROC_END; jj++) {
        pConf = &aConf[jj];
        if (pConf->eType == SWPROC_ARG) {
            if (ii < objc && ii >= 0) {
                apObj[jj] = objv[ii];
                Tcl_IncrRefCount(apObj[jj]);
                ii++;
            } else {
                goto error_insufficient_args;
            }
        } else if (pConf->zDefault) {
            apObj[jj] = Tcl_NewStringObj(pConf->zDefault, -1);
            Tcl_IncrRefCount(apObj[jj]);
        }
    }

    /* Now set values for any options or switches passed */
    lastswitch = (argsatend ? firstarg : objc);
    for (ii = (argsatend ? 0 : argcnt); ii < lastswitch ;ii++) {
        zSwitch = Tcl_GetString(objv[ii]);
        if (zSwitch[0] != '-') {
            goto error_no_such_option;
        }

        for (jj = 0; aConf[jj].eType != SWPROC_END; jj++) {
            pConf = &aConf[jj];
            if (pConf->eType == SWPROC_OPT || pConf->eType == SWPROC_SWITCH) {
                if (0 == strcmp(pConf->zSwitch, &zSwitch[1])) {
                   if (apObj[jj]) {
                       Tcl_DecrRefCount(apObj[jj]);
                       apObj[jj] = 0;
                   }
                   if (pConf->eType == SWPROC_SWITCH) {
                       apObj[jj] = Tcl_NewStringObj(pConf->zTrue, -1);
                       Tcl_IncrRefCount(apObj[jj]);
                   } else if (ii+1 < lastswitch) {
                       ii++;
                       apObj[jj] = objv[ii];
                       Tcl_IncrRefCount(apObj[jj]);
                   } else {
                       goto error_option_requires_arg;
                   }
                   break;
                }
            }
        }
        if (aConf[jj].eType == SWPROC_END) {
            goto error_no_such_option;
        }
    }

    return TCL_OK;

error_insufficient_args:
    Tcl_AppendResult(interp, "Insufficient args", 0);
    goto error_out;

error_no_such_option:
    Tcl_AppendResult(interp, "No such option: ", zSwitch, 0);
    goto error_out;

error_option_requires_arg:
    Tcl_AppendResult(interp, "Option \"", zSwitch, "\"requires an argument", 0);
    goto error_out;

error_out:
    /* Any error condition eventually jumps here. Discard any accumulated
     * object references and return TCL_ERROR.
     */
    for (jj = 0; aConf[jj].eType != SWPROC_END; jj++) {
        if (apObj[jj]) {
            Tcl_DecrRefCount(apObj[jj]);
            apObj[jj] = 0;
        }
    }
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * SwprocCleanup --
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
SwprocCleanup(apObj, nObj)
    Tcl_Obj **apObj;
    int nObj;
{
    int ii;
    for (ii = 0; ii < nObj; ii++) {
        if (apObj[ii]) {
            Tcl_DecrRefCount(apObj[ii]);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * swproc_rtCmd --
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
swproc_rtCmd(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    SwprocConf aConf[2 + 1] = {
        {SWPROC_ARG, "conf", 0, 0},         /* CONFIGURATION */
        {SWPROC_ARG, "args", 0, 0},         /* ARGUMENTS */
        {SWPROC_END, 0, 0, 0}
    };
    Tcl_Obj *apObj[2];
    int rc;
    int ii;

    assert(sizeof(apObj)/sizeof(apObj[0])+1 == sizeof(aConf)/sizeof(aConf[0]));
    rc = SwprocRt(interp, objc - 1, &objv[1], aConf, apObj);
    if (rc == TCL_OK) {
        Tcl_Obj **apConf;
        int nConf;

        rc = Tcl_ListObjGetElements(interp, apObj[0], &nConf, &apConf);
        if (rc == TCL_OK) {
            SwprocConf *aScriptConf;
            Tcl_Obj **apVars;

            aScriptConf = (SwprocConf *)ckalloc(
                    nConf * sizeof(Tcl_Obj*) + 
                    (nConf + 1) * sizeof(SwprocConf)
            );
            apVars = (Tcl_Obj **)&aScriptConf[nConf + 1];
            for (ii = 0; ii < nConf && rc == TCL_OK; ii++) {
                SwprocConf *pConf = &aScriptConf[ii];
                Tcl_Obj **apParams;
                int nP;

                rc = Tcl_ListObjGetElements(interp, apConf[ii], &nP, &apParams);
                if (rc == TCL_OK) {
                    switch (nP) {
                        case 3:
                            pConf->eType = SWPROC_SWITCH;
                            pConf->zSwitch=Tcl_GetString(apParams[0]);
                            pConf->zDefault=Tcl_GetString(apParams[1]);
                            pConf->zTrue = Tcl_GetString(apParams[2]);
                            break;
                        case 2:
                            pConf->eType = SWPROC_OPT;
                            pConf->zSwitch=Tcl_GetString(apParams[0]);
                            pConf->zDefault=Tcl_GetString(apParams[1]);
                            break;
                        case 1:
                            pConf->eType = SWPROC_ARG;
                            pConf->zSwitch=Tcl_GetString(apParams[0]);
                            break;
                        default:
                            rc = TCL_ERROR;
                            break;
                    }
                }
            }
            aScriptConf[nConf].eType = SWPROC_END;

            if (rc == TCL_OK) {
                Tcl_Obj **apArgs;
                int nArgs;
                rc = Tcl_ListObjGetElements(interp, apObj[1], &nArgs, &apArgs);
                if (rc == TCL_OK) {
                    rc = SwprocRt(interp, nArgs, apArgs, aScriptConf, apVars);
                    if (rc == TCL_OK) {
                        for (ii = 0; ii < nConf; ii++) {
                            const char *zVar = aScriptConf[ii].zSwitch;
                            const char *zVal = Tcl_GetString(apVars[ii]);
                            Tcl_SetVar(interp, zVar, zVal, 0);
                            Tcl_DecrRefCount(apVars[ii]);
                        }
                    }
                }
            }

            ckfree((char *)aScriptConf);
        }

        for (ii = 0; ii < sizeof(apObj)/sizeof(apObj[0]); ii++) {
            assert(apObj[ii]);
            Tcl_DecrRefCount(apObj[ii]);
        }
    }

    return rc;
}

/*
 *---------------------------------------------------------------------------
 *
 * SwprocInit --
 *
 *     Add the swproc command to the specified interpreter:
 *     
 *             swproc NAME ARGS BODY
 *    
 *         [swproc] is very similar to the proc command, except any procedure
 *         arguments with default values must be specified with switches
 *         instead of on the command line. For example, the following are
 *         equivalent:
 *    
 *             proc   abc {a {b hello} {c goodbye}} {...}
 *             abc one two
 *    
 *             swproc swabc {a {b hello} {c goodbye}} {...}
 *             swabc one -b two
 *    
 *         This means, in the above example, that it is possible to call
 *         [swabc] and supply a value for parameter "c" but not "b". This is
 *         not possible with commands created by regular Tcl [proc].
 *    
 *         Commands created with [swproc] may also accept switches that do not
 *         take arguments. These should be specified as a list of three
 *         elements.  The first is the name of the switch (and variable). The
 *         second is the default value of the variable (if the switch is not
 *         present), and the third is the value if the switch is present. For
 *         example, the following two blocks are equivalent:
 *    
 *             proc abc {a} {...}
 *             abc b
 *             abc c
 *    
 *             swproc abc {{a b c}} {...}
 *             abc
 *             abc -a
 *    
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int SwprocInit(interp)
    Tcl_Interp *interp;
{
    Tcl_CreateObjCommand(interp, "::tkhtml::swproc_rt", swproc_rtCmd, 0, 0);
    Tcl_Eval(interp,
        "proc swproc {procname arguments script} {\n"
        "  uplevel [subst {\n"
        "    proc $procname {args} {\n"
        "      ::tkhtml::swproc_rt [list $arguments] \\$args\n"
        "      $script\n"
        "    }\n"
        "  }]\n"
        "}\n"
    );
    return TCL_OK;
}

