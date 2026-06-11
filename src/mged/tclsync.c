/*                       T C L S Y N C . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file tclsync.c
 *
 * Sync data between the main interp and the search exec interp
 */

#include "common.h"

#include <tcl.h>
#include <string.h>
#include <stdio.h>
#include "bu/str.h"

/* =========================
 * Utility: Check if array
 * ========================= */
static int IsArray(Tcl_Interp *interp, const char *fqName) {
    Tcl_Obj *cmd = Tcl_ObjPrintf("array exists %s", fqName);
    Tcl_IncrRefCount(cmd); /* caller holds a ref so DecrRefCount below is correct */
    int rc = Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(cmd);

    if (rc != TCL_OK) return 0;
    return atoi(Tcl_GetStringResult(interp));
}

/* =========================
 * Append variable (scalar/array)
 * ========================= */
static void AppendVar(Tcl_Interp *interp, Tcl_Obj *script,
                      const char *varName, const char *nsPrefix)
{
    char fqName[1024];

    if (nsPrefix && !BU_STR_EQUAL(nsPrefix, "::")) {
        snprintf(fqName, sizeof(fqName), "%s::%s", nsPrefix, varName);
    } else {
        snprintf(fqName, sizeof(fqName), "%s", varName);
    }

    /* ARRAY */
    if (IsArray(interp, fqName)) {
        Tcl_Obj *getCmd = Tcl_ObjPrintf("array get %s", fqName);
        Tcl_IncrRefCount(getCmd); /* caller holds a ref so DecrRefCount below is correct */

        if (Tcl_EvalObjEx(interp, getCmd, TCL_EVAL_GLOBAL) == TCL_OK) {
            Tcl_Obj *list = Tcl_GetObjResult(interp);
            Tcl_IncrRefCount(list); /* protect against reset by subsequent evals */

            /* Build the command as a proper list so every element is correctly
             * quoted when the script is later parsed:
             *   global:    array set varname {k1 v1 k2 v2 ...}
             *   namespace: namespace eval ::ns { array set varname {k1 v1 ...} } */
            Tcl_Obj *setCmd = Tcl_NewListObj(0, NULL);
            Tcl_IncrRefCount(setCmd);

            if (nsPrefix && !BU_STR_EQUAL(nsPrefix, "::")) {
                Tcl_ListObjAppendElement(NULL, setCmd,
                    Tcl_NewStringObj("namespace", -1));
                Tcl_ListObjAppendElement(NULL, setCmd,
                    Tcl_NewStringObj("eval", -1));
                Tcl_ListObjAppendElement(NULL, setCmd,
                    Tcl_NewStringObj(nsPrefix, -1));
                /* inner command as a nested list: {array set varname LIST} */
                Tcl_Obj *inner = Tcl_NewListObj(0, NULL);
                Tcl_ListObjAppendElement(NULL, inner,
                    Tcl_NewStringObj("array", -1));
                Tcl_ListObjAppendElement(NULL, inner,
                    Tcl_NewStringObj("set", -1));
                Tcl_ListObjAppendElement(NULL, inner,
                    Tcl_NewStringObj(varName, -1));
                Tcl_ListObjAppendElement(NULL, inner, list);
                Tcl_ListObjAppendElement(NULL, setCmd, inner);
            } else {
                Tcl_ListObjAppendElement(NULL, setCmd,
                    Tcl_NewStringObj("array", -1));
                Tcl_ListObjAppendElement(NULL, setCmd,
                    Tcl_NewStringObj("set", -1));
                Tcl_ListObjAppendElement(NULL, setCmd,
                    Tcl_NewStringObj(varName, -1));
                Tcl_ListObjAppendElement(NULL, setCmd, list);
            }

            Tcl_AppendObjToObj(script, setCmd);
            Tcl_AppendToObj(script, "\n", -1);
            Tcl_DecrRefCount(setCmd);
            Tcl_DecrRefCount(list);
        }

        Tcl_DecrRefCount(getCmd);
        return;
    }

    /* SCALAR */
    Tcl_Obj *val = Tcl_GetVar2Ex(interp, fqName, NULL, TCL_GLOBAL_ONLY);
    if (!val) return;

    /* Build set/variable command as a proper list so the value is always
     * quoted correctly regardless of its string content. */
    if (nsPrefix && !BU_STR_EQUAL(nsPrefix, "::")) {
        Tcl_Obj *setCmd = Tcl_NewListObj(0, NULL);
        Tcl_IncrRefCount(setCmd);
        Tcl_ListObjAppendElement(NULL, setCmd,
            Tcl_NewStringObj("namespace", -1));
        Tcl_ListObjAppendElement(NULL, setCmd,
            Tcl_NewStringObj("eval", -1));
        Tcl_ListObjAppendElement(NULL, setCmd,
            Tcl_NewStringObj(nsPrefix, -1));
        Tcl_Obj *inner = Tcl_NewListObj(0, NULL);
        Tcl_ListObjAppendElement(NULL, inner,
            Tcl_NewStringObj("set", -1));
        Tcl_ListObjAppendElement(NULL, inner,
            Tcl_NewStringObj(varName, -1));
        Tcl_ListObjAppendElement(NULL, inner, val);
        Tcl_ListObjAppendElement(NULL, setCmd, inner);
        Tcl_AppendObjToObj(script, setCmd);
        Tcl_AppendToObj(script, "\n", -1);
        Tcl_DecrRefCount(setCmd);
    } else {
        Tcl_Obj *cmd = Tcl_NewListObj(0, NULL);
        Tcl_IncrRefCount(cmd);
        Tcl_ListObjAppendElement(NULL, cmd, Tcl_NewStringObj("set", -1));
        Tcl_ListObjAppendElement(NULL, cmd, Tcl_NewStringObj(varName, -1));
        Tcl_ListObjAppendElement(NULL, cmd, val);

        Tcl_AppendObjToObj(script, cmd);
        Tcl_AppendToObj(script, "\n", -1);
        Tcl_DecrRefCount(cmd);
    }
}

/* =========================
 * Append global variables
 * ========================= */
static int AppendGlobals(Tcl_Interp *interp, Tcl_Obj *script) {
    if (Tcl_Eval(interp, "info globals") != TCL_OK)
        return TCL_ERROR;

    Tcl_Obj *list = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(list); /* protect against reset by subsequent evals inside the loop */

    int count;
    Tcl_Obj **elems;

    if (Tcl_ListObjGetElements(interp, list, &count, &elems) != TCL_OK) {
        Tcl_DecrRefCount(list);
        return TCL_ERROR;
    }

    for (int i = 0; i < count; i++) {
        const char *name = Tcl_GetString(elems[i]);

        if (name[0] == '_') continue; /* skip internals */

        AppendVar(interp, script, name, NULL);
    }

    Tcl_DecrRefCount(list);
    return TCL_OK;
}

/* =========================
 * Recursive namespace vars
 * ========================= */
static int AppendNamespaceVarsRec(Tcl_Interp *interp,
                                 Tcl_Obj *script,
                                 const char *nsName)
{
    Tcl_Obj *cmd = Tcl_ObjPrintf("info vars %s::*", nsName);
    Tcl_IncrRefCount(cmd); /* caller holds a ref */

    if (Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL) != TCL_OK) {
        Tcl_DecrRefCount(cmd);
        return TCL_ERROR;
    }
    Tcl_DecrRefCount(cmd);

    Tcl_Obj *vars = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(vars); /* protect against reset by AppendVar's internal evals */

    int count;
    Tcl_Obj **elems;

    if (Tcl_ListObjGetElements(interp, vars, &count, &elems) == TCL_OK) {
        for (int i = 0; i < count; i++) {
            const char *fq = Tcl_GetString(elems[i]);

            const char *tail = strrchr(fq, ':');
            if (!tail) continue;
            tail++;

            AppendVar(interp, script, tail, nsName);
        }
    }

    Tcl_DecrRefCount(vars);

    /* recurse children */
    cmd = Tcl_ObjPrintf("namespace children %s", nsName);
    Tcl_IncrRefCount(cmd); /* caller holds a ref */

    if (Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL) != TCL_OK) {
        Tcl_DecrRefCount(cmd);
        return TCL_ERROR;
    }
    Tcl_DecrRefCount(cmd);

    Tcl_Obj *children = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(children); /* protect against reset by recursive calls */

    if (Tcl_ListObjGetElements(interp, children, &count, &elems) == TCL_OK) {
        for (int i = 0; i < count; i++) {
            const char *child = Tcl_GetString(elems[i]);

            if (bu_strncmp(child, "::tcl", 5) == 0 ||
                bu_strncmp(child, "::oo", 4) == 0)
                continue;

            if (AppendNamespaceVarsRec(interp, script, child) != TCL_OK) {
                Tcl_DecrRefCount(children);
                return TCL_ERROR;
            }
        }
    }

    Tcl_DecrRefCount(children);
    return TCL_OK;
}

/* =========================
 * Append namespaces
 * ========================= */
static int AppendNamespaces(Tcl_Interp *interp, Tcl_Obj *script) {
    if (Tcl_Eval(interp, "namespace children ::") != TCL_OK)
        return TCL_ERROR;

    Tcl_Obj *list = Tcl_GetObjResult(interp);

    int count;
    Tcl_Obj **elems;

    if (Tcl_ListObjGetElements(interp, list, &count, &elems) != TCL_OK)
        return TCL_ERROR;

    for (int i = 0; i < count; i++) {
        const char *ns = Tcl_GetString(elems[i]);

        if (bu_strncmp(ns, "::tcl", 5) == 0 ||
            bu_strncmp(ns, "::oo", 4) == 0)
            continue;

        Tcl_AppendPrintfToObj(script,
            "namespace eval %s {}\n", ns);
    }

    return TCL_OK;
}

/* =========================
 * Append procs
 * ========================= */
static int AppendProcs(Tcl_Interp *interp, Tcl_Obj *script) {
    if (Tcl_Eval(interp, "info procs") != TCL_OK)
        return TCL_ERROR;

    Tcl_Obj *list = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(list); /* protect against reset by info-args/body evals inside the loop */

    int count;
    Tcl_Obj **elems;

    if (Tcl_ListObjGetElements(interp, list, &count, &elems) != TCL_OK) {
        Tcl_DecrRefCount(list);
        return TCL_ERROR;
    }

    for (int i = 0; i < count; i++) {
        const char *name = Tcl_GetString(elems[i]);

        if (bu_strncmp(name, "::tcl", 5) == 0)
            continue;

        Tcl_Obj *argsCmd = Tcl_ObjPrintf("info args %s", name);
        Tcl_IncrRefCount(argsCmd); /* caller holds a ref so DecrRefCount below is correct */
        if (Tcl_EvalObjEx(interp, argsCmd, TCL_EVAL_GLOBAL) != TCL_OK) {
            Tcl_DecrRefCount(argsCmd);
            continue;
        }
        Tcl_DecrRefCount(argsCmd);

        Tcl_Obj *args = Tcl_GetObjResult(interp);
        Tcl_IncrRefCount(args);

        Tcl_Obj *bodyCmd = Tcl_ObjPrintf("info body %s", name);
        Tcl_IncrRefCount(bodyCmd); /* caller holds a ref so DecrRefCount below is correct */
        if (Tcl_EvalObjEx(interp, bodyCmd, TCL_EVAL_GLOBAL) != TCL_OK) {
            Tcl_DecrRefCount(bodyCmd);
            Tcl_DecrRefCount(args);
            continue;
        }
        Tcl_DecrRefCount(bodyCmd);

        Tcl_Obj *body = Tcl_GetObjResult(interp);
        Tcl_IncrRefCount(body);

        /* Brace-quote both args and body so they form single Tcl words.
         * Without quoting, space-separated arg lists and multi-word bodies
         * would be tokenised as multiple proc arguments (wrong # args). */
        Tcl_Obj *line = Tcl_ObjPrintf("proc %s {", name);
        Tcl_IncrRefCount(line);
        Tcl_AppendObjToObj(line, args);
        Tcl_AppendToObj(line, "} {", -1);
        Tcl_AppendObjToObj(line, body);
        Tcl_AppendToObj(line, "}\n", -1);

        Tcl_AppendObjToObj(script, line);
        Tcl_DecrRefCount(line);

        Tcl_DecrRefCount(args);
        Tcl_DecrRefCount(body);
    }

    Tcl_DecrRefCount(list);
    return TCL_OK;
}

/* =========================
 * PUBLIC: Build snapshot
 * ========================= */
Tcl_Obj *BuildInterpSnapshot(Tcl_Interp *interp) {
    Tcl_Obj *script = Tcl_NewObj();
    Tcl_IncrRefCount(script);

    Tcl_AppendToObj(script, "# --- SNAPSHOT START ---\n", -1);

    if (AppendNamespaces(interp, script) != TCL_OK) goto error;
    if (AppendProcs(interp, script) != TCL_OK) goto error;
    if (AppendGlobals(interp, script) != TCL_OK) goto error;
    if (AppendNamespaceVarsRec(interp, script, "::") != TCL_OK) goto error;

    Tcl_AppendToObj(script, "# --- SNAPSHOT END ---\n", -1);

    return script;

error:
    Tcl_DecrRefCount(script);
    return NULL;
}

/* =========================
 * PUBLIC: Replay snapshot
 * ========================= */
int ReplayInterpSnapshot(Tcl_Interp *interp, Tcl_Obj *snapshot) {
    return Tcl_EvalObjEx(interp, snapshot, TCL_EVAL_GLOBAL);
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
