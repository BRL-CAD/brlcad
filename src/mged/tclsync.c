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
 * Serialize the script-level state needed by the search -exec interpreter.
 * Tcl interpreters cannot be cloned or used from a thread other than their
 * owning thread, so the snapshot is a Tcl list that may safely be converted
 * to a string, transferred to the search thread, and reconstructed there.
 *
 * Native commands, variable traces, channels, object internal reps, and
 * child interpreters are intentionally not copied.  Native MGED/GED commands
 * are supplied to the search interpreter by cmd.cpp's unknown-command bridge.
 */

#include "common.h"

#include <string.h>

#include <tcl.h>

#include "bu/str.h"


/* Evaluate words as one command without allowing substitutions in the words. */
static int
EvalWords(Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    Tcl_Obj *cmd = Tcl_NewListObj(objc, objv);
    Tcl_IncrRefCount(cmd);
    int ret = Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(cmd);
    return ret;
}


static Tcl_Obj *
ResultCopy(Tcl_Interp *interp)
{
    Tcl_Obj *result = Tcl_DuplicateObj(Tcl_GetObjResult(interp));
    Tcl_IncrRefCount(result);
    return result;
}


static int
SystemNamespace(const char *name)
{
    if (!name)
	return 0;

    if (BU_STR_EQUAL(name, "::tcl") || bu_strncmp(name, "::tcl::", 7) == 0)
	return 1;
    if (BU_STR_EQUAL(name, "::oo") || bu_strncmp(name, "::oo::", 6) == 0)
	return 1;

    return 0;
}


/* The info patterns used below may also match members of child namespaces. */
static int
DirectNamespaceMember(const char *name, const char *ns)
{
    if (!name || !ns)
	return 0;

    if (BU_STR_EQUAL(ns, "::")) {
	if (name[0] == ':' && name[1] == ':')
	    name += 2;
	return name[0] != '\0' && strstr(name, "::") == NULL;
    }

    size_t ns_len = strlen(ns);
    if (bu_strncmp(name, ns, ns_len) != 0 || name[ns_len] != ':' ||
	name[ns_len + 1] != ':')
	return 0;

    name += ns_len + 2;
    return name[0] != '\0' && strstr(name, "::") == NULL;
}


static Tcl_Obj *
QualifiedPattern(const char *ns)
{
    if (BU_STR_EQUAL(ns, "::"))
	return Tcl_NewStringObj("::*", -1);

    Tcl_Obj *pattern = Tcl_NewStringObj(ns, -1);
    Tcl_AppendToObj(pattern, "::*", -1);
    return pattern;
}


static int
AppendRecord(Tcl_Interp *interp, Tcl_Obj *snapshot, Tcl_Obj *record)
{
    return Tcl_ListObjAppendElement(interp, snapshot, record);
}


static int
AppendNamespaceRecord(Tcl_Interp *interp, Tcl_Obj *snapshot, const char *ns)
{
    Tcl_Obj *record = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, record, Tcl_NewStringObj("namespace", -1));
    Tcl_ListObjAppendElement(interp, record, Tcl_NewStringObj(ns, -1));
    return AppendRecord(interp, snapshot, record);
}


static int
AppendProc(Tcl_Interp *interp, Tcl_Obj *snapshot, const char *name)
{
    Tcl_Obj *words[3];
    words[0] = Tcl_NewStringObj("info", -1);
    words[1] = Tcl_NewStringObj("args", -1);
    words[2] = Tcl_NewStringObj(name, -1);
    if (EvalWords(interp, 3, words) != TCL_OK)
	return TCL_OK;

    Tcl_Obj *args = ResultCopy(interp);
    int argc = 0;
    Tcl_Obj **argv = NULL;
    if (Tcl_ListObjGetElements(interp, args, &argc, &argv) != TCL_OK) {
	Tcl_DecrRefCount(args);
	return TCL_OK;
    }

    Tcl_Obj *arg_spec = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(arg_spec);
    for (int i = 0; i < argc; i++) {
	const char *default_var = "::__mged_search_snapshot_default";
	(void)Tcl_UnsetVar(interp, default_var, TCL_GLOBAL_ONLY);

	Tcl_Obj *default_words[5];
	default_words[0] = Tcl_NewStringObj("info", -1);
	default_words[1] = Tcl_NewStringObj("default", -1);
	default_words[2] = Tcl_NewStringObj(name, -1);
	default_words[3] = argv[i];
	default_words[4] = Tcl_NewStringObj(default_var, -1);

	int has_default = 0;
	if (EvalWords(interp, 5, default_words) == TCL_OK &&
	    Tcl_GetBooleanFromObj(interp, Tcl_GetObjResult(interp), &has_default) == TCL_OK &&
	    has_default) {
	    Tcl_Obj *value = Tcl_GetVar2Ex(interp, default_var, NULL, TCL_GLOBAL_ONLY);
	    if (value) {
		Tcl_Obj *arg = Tcl_NewListObj(0, NULL);
		Tcl_ListObjAppendElement(interp, arg, Tcl_DuplicateObj(argv[i]));
		Tcl_ListObjAppendElement(interp, arg, Tcl_DuplicateObj(value));
		Tcl_ListObjAppendElement(interp, arg_spec, arg);
	    } else {
		Tcl_ListObjAppendElement(interp, arg_spec, Tcl_DuplicateObj(argv[i]));
	    }
	} else {
	    Tcl_ListObjAppendElement(interp, arg_spec, Tcl_DuplicateObj(argv[i]));
	}
	(void)Tcl_UnsetVar(interp, default_var, TCL_GLOBAL_ONLY);
    }
    Tcl_DecrRefCount(args);

    words[0] = Tcl_NewStringObj("info", -1);
    words[1] = Tcl_NewStringObj("body", -1);
    words[2] = Tcl_NewStringObj(name, -1);
    if (EvalWords(interp, 3, words) != TCL_OK) {
	Tcl_DecrRefCount(arg_spec);
	return TCL_OK;
    }
    Tcl_Obj *body = ResultCopy(interp);

    Tcl_Obj *record = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, record, Tcl_NewStringObj("proc", -1));
    Tcl_ListObjAppendElement(interp, record, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp, record, arg_spec);
    Tcl_ListObjAppendElement(interp, record, body);
    int ret = AppendRecord(interp, snapshot, record);

    Tcl_DecrRefCount(arg_spec);
    Tcl_DecrRefCount(body);
    return ret;
}


static int
AppendProcs(Tcl_Interp *interp, Tcl_Obj *snapshot, const char *ns)
{
    Tcl_Obj *words[3];
    words[0] = Tcl_NewStringObj("info", -1);
    words[1] = Tcl_NewStringObj("procs", -1);
    words[2] = QualifiedPattern(ns);
    if (EvalWords(interp, 3, words) != TCL_OK)
	return TCL_ERROR;

    Tcl_Obj *procs = ResultCopy(interp);
    int proc_count = 0;
    Tcl_Obj **procv = NULL;
    if (Tcl_ListObjGetElements(interp, procs, &proc_count, &procv) != TCL_OK) {
	Tcl_DecrRefCount(procs);
	return TCL_ERROR;
    }

    for (int i = 0; i < proc_count; i++) {
	const char *name = Tcl_GetString(procv[i]);
	if (DirectNamespaceMember(name, ns) && AppendProc(interp, snapshot, name) != TCL_OK) {
	    Tcl_DecrRefCount(procs);
	    return TCL_ERROR;
	}
    }

    Tcl_DecrRefCount(procs);
    return TCL_OK;
}


static int
AppendVariable(Tcl_Interp *interp, Tcl_Obj *snapshot, const char *name)
{
    Tcl_Obj *words[3];
    words[0] = Tcl_NewStringObj("array", -1);
    words[1] = Tcl_NewStringObj("exists", -1);
    words[2] = Tcl_NewStringObj(name, -1);
    if (EvalWords(interp, 3, words) != TCL_OK)
	return TCL_OK;

    int is_array = 0;
    if (Tcl_GetBooleanFromObj(interp, Tcl_GetObjResult(interp), &is_array) != TCL_OK)
	return TCL_OK;

    Tcl_Obj *record = Tcl_NewListObj(0, NULL);
    if (is_array) {
	words[0] = Tcl_NewStringObj("array", -1);
	words[1] = Tcl_NewStringObj("get", -1);
	words[2] = Tcl_NewStringObj(name, -1);
	if (EvalWords(interp, 3, words) != TCL_OK)
	    return TCL_OK;
	Tcl_Obj *value = ResultCopy(interp);

	Tcl_ListObjAppendElement(interp, record, Tcl_NewStringObj("array", -1));
	Tcl_ListObjAppendElement(interp, record, Tcl_NewStringObj(name, -1));
	Tcl_ListObjAppendElement(interp, record, value);
	int ret = AppendRecord(interp, snapshot, record);
	Tcl_DecrRefCount(value);
	return ret;
    }

    Tcl_Obj *value = Tcl_GetVar2Ex(interp, name, NULL, TCL_GLOBAL_ONLY);
    if (!value)
	return TCL_OK;

    Tcl_ListObjAppendElement(interp, record, Tcl_NewStringObj("scalar", -1));
    Tcl_ListObjAppendElement(interp, record, Tcl_NewStringObj(name, -1));
    Tcl_ListObjAppendElement(interp, record, Tcl_DuplicateObj(value));
    return AppendRecord(interp, snapshot, record);
}


static int
AppendVariables(Tcl_Interp *interp, Tcl_Obj *snapshot, const char *ns)
{
    Tcl_Obj *words[3];
    words[0] = Tcl_NewStringObj("info", -1);
    words[1] = Tcl_NewStringObj(BU_STR_EQUAL(ns, "::") ? "globals" : "vars", -1);
    int word_count = 2;
    if (!BU_STR_EQUAL(ns, "::")) {
	words[2] = QualifiedPattern(ns);
	word_count = 3;
    }

    if (EvalWords(interp, word_count, words) != TCL_OK)
	return TCL_ERROR;

    Tcl_Obj *vars = ResultCopy(interp);
    int var_count = 0;
    Tcl_Obj **varv = NULL;
    if (Tcl_ListObjGetElements(interp, vars, &var_count, &varv) != TCL_OK) {
	Tcl_DecrRefCount(vars);
	return TCL_ERROR;
    }

    for (int i = 0; i < var_count; i++) {
	const char *name = Tcl_GetString(varv[i]);
	if (BU_STR_EQUAL(name, "::__mged_search_snapshot_default"))
	    continue;
	if ((BU_STR_EQUAL(ns, "::") || DirectNamespaceMember(name, ns)) &&
	    AppendVariable(interp, snapshot, name) != TCL_OK) {
	    Tcl_DecrRefCount(vars);
	    return TCL_ERROR;
	}
    }

    Tcl_DecrRefCount(vars);
    return TCL_OK;
}


static int
AppendNamespace(Tcl_Interp *interp, Tcl_Obj *snapshot, const char *ns)
{
    if (!BU_STR_EQUAL(ns, "::") && AppendNamespaceRecord(interp, snapshot, ns) != TCL_OK)
	return TCL_ERROR;

    if (AppendProcs(interp, snapshot, ns) != TCL_OK)
	return TCL_ERROR;
    if (AppendVariables(interp, snapshot, ns) != TCL_OK)
	return TCL_ERROR;

    Tcl_Obj *words[3];
    words[0] = Tcl_NewStringObj("namespace", -1);
    words[1] = Tcl_NewStringObj("children", -1);
    words[2] = Tcl_NewStringObj(ns, -1);
    if (EvalWords(interp, 3, words) != TCL_OK)
	return TCL_ERROR;

    Tcl_Obj *children = ResultCopy(interp);
    int child_count = 0;
    Tcl_Obj **childv = NULL;
    if (Tcl_ListObjGetElements(interp, children, &child_count, &childv) != TCL_OK) {
	Tcl_DecrRefCount(children);
	return TCL_ERROR;
    }

    for (int i = 0; i < child_count; i++) {
	const char *child = Tcl_GetString(childv[i]);
	if (!SystemNamespace(child) && AppendNamespace(interp, snapshot, child) != TCL_OK) {
	    Tcl_DecrRefCount(children);
	    return TCL_ERROR;
	}
    }

    Tcl_DecrRefCount(children);
    return TCL_OK;
}


static int
AppendAliases(Tcl_Interp *interp, Tcl_Obj *snapshot)
{
    Tcl_Obj *words[3];
    words[0] = Tcl_NewStringObj("interp", -1);
    words[1] = Tcl_NewStringObj("aliases", -1);
    words[2] = Tcl_NewStringObj("", 0);
    if (EvalWords(interp, 3, words) != TCL_OK)
	return TCL_ERROR;

    Tcl_Obj *aliases = ResultCopy(interp);
    int alias_count = 0;
    Tcl_Obj **aliasv = NULL;
    if (Tcl_ListObjGetElements(interp, aliases, &alias_count, &aliasv) != TCL_OK) {
	Tcl_DecrRefCount(aliases);
	return TCL_ERROR;
    }

    for (int i = 0; i < alias_count; i++) {
	Tcl_Interp *target_interp = NULL;
	const char *target_cmd = NULL;
	int prefix_count = 0;
	Tcl_Obj **prefixv = NULL;
	if (Tcl_GetAliasObj(interp, Tcl_GetString(aliasv[i]), &target_interp,
		&target_cmd, &prefix_count, &prefixv) != TCL_OK ||
	    target_interp != interp || !target_cmd)
	    continue;

	Tcl_Obj *target = Tcl_NewListObj(0, NULL);
	Tcl_IncrRefCount(target);
	Tcl_ListObjAppendElement(interp, target, Tcl_NewStringObj(target_cmd, -1));
	for (int j = 0; j < prefix_count; j++)
	    Tcl_ListObjAppendElement(interp, target, Tcl_DuplicateObj(prefixv[j]));

	Tcl_Obj *record = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(interp, record, Tcl_NewStringObj("alias", -1));
	Tcl_ListObjAppendElement(interp, record, Tcl_DuplicateObj(aliasv[i]));
	Tcl_ListObjAppendElement(interp, record, target);
	int ret = AppendRecord(interp, snapshot, record);
	Tcl_DecrRefCount(target);
	if (ret != TCL_OK) {
	    Tcl_DecrRefCount(aliases);
	    return TCL_ERROR;
	}
    }

    Tcl_DecrRefCount(aliases);
    return TCL_OK;
}


Tcl_Obj *
BuildInterpSnapshot(Tcl_Interp *interp)
{
    Tcl_Obj *saved_result = ResultCopy(interp);
    Tcl_Obj *snapshot = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(snapshot);

    int ret = AppendNamespace(interp, snapshot, "::");
    if (ret == TCL_OK)
	ret = AppendAliases(interp, snapshot);

    Tcl_SetObjResult(interp, saved_result);
    Tcl_DecrRefCount(saved_result);

    if (ret != TCL_OK) {
	Tcl_DecrRefCount(snapshot);
	return NULL;
    }

    return snapshot;
}


static int
ReplayRecord(Tcl_Interp *interp, Tcl_Obj *record)
{
    int field_count = 0;
    Tcl_Obj **fields = NULL;
    if (Tcl_ListObjGetElements(interp, record, &field_count, &fields) != TCL_OK ||
	field_count < 2)
	return TCL_ERROR;

    const char *type = Tcl_GetString(fields[0]);
    if (BU_STR_EQUAL(type, "namespace") && field_count == 2) {
	Tcl_Obj *words[4];
	words[0] = Tcl_NewStringObj("namespace", -1);
	words[1] = Tcl_NewStringObj("eval", -1);
	words[2] = fields[1];
	words[3] = Tcl_NewStringObj("", 0);
	return EvalWords(interp, 4, words);
    }

    if (BU_STR_EQUAL(type, "proc") && field_count == 4) {
	Tcl_Obj *words[4];
	words[0] = Tcl_NewStringObj("proc", -1);
	words[1] = fields[1];
	words[2] = fields[2];
	words[3] = fields[3];
	return EvalWords(interp, 4, words);
    }

    if (BU_STR_EQUAL(type, "scalar") && field_count == 3) {
	Tcl_Obj *words[3];
	words[0] = Tcl_NewStringObj("set", -1);
	words[1] = fields[1];
	words[2] = fields[2];
	return EvalWords(interp, 3, words);
    }

    if (BU_STR_EQUAL(type, "array") && field_count == 3) {
	Tcl_Obj *words[4];
	words[0] = Tcl_NewStringObj("array", -1);
	words[1] = Tcl_NewStringObj("set", -1);
	words[2] = fields[1];
	words[3] = fields[2];
	return EvalWords(interp, 4, words);
    }

    if (BU_STR_EQUAL(type, "alias") && field_count == 3) {
	int target_count = 0;
	Tcl_Obj **targetv = NULL;
	if (Tcl_ListObjGetElements(interp, fields[2], &target_count, &targetv) != TCL_OK ||
	    target_count < 1)
	    return TCL_ERROR;

	Tcl_Obj **words = (Tcl_Obj **)Tcl_Alloc((unsigned)(target_count + 5) * sizeof(Tcl_Obj *));
	words[0] = Tcl_NewStringObj("interp", -1);
	words[1] = Tcl_NewStringObj("alias", -1);
	words[2] = Tcl_NewStringObj("", 0);
	words[3] = fields[1];
	words[4] = Tcl_NewStringObj("", 0);
	for (int i = 0; i < target_count; i++)
	    words[i + 5] = targetv[i];
	int ret = EvalWords(interp, target_count + 5, words);
	Tcl_Free((char *)words);
	return ret;
    }

    Tcl_SetObjResult(interp, Tcl_ObjPrintf("invalid search snapshot record type '%s'", type));
    return TCL_ERROR;
}


int
ReplayInterpSnapshot(Tcl_Interp *interp, Tcl_Obj *snapshot)
{
    int record_count = 0;
    Tcl_Obj **records = NULL;
    if (Tcl_ListObjGetElements(interp, snapshot, &record_count, &records) != TCL_OK)
	return TCL_ERROR;

    int error_count = 0;
    Tcl_Obj *first_error = NULL;
    for (int i = 0; i < record_count; i++) {
	if (ReplayRecord(interp, records[i]) == TCL_OK)
	    continue;

	error_count++;
	if (!first_error)
	    first_error = ResultCopy(interp);
	Tcl_ResetResult(interp);
    }

    if (error_count) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"%d search snapshot record%s could not be replayed; first error: %s",
		error_count, error_count == 1 ? "" : "s",
		first_error ? Tcl_GetString(first_error) : "unknown error"));
	if (first_error)
	    Tcl_DecrRefCount(first_error);
	return TCL_ERROR;
    }

    Tcl_ResetResult(interp);
    return TCL_OK;
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
