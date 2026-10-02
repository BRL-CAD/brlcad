/*             C H E C K _ N A M E S P A C E . C P P
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
/** @file check_namespace.cpp
 *
 * Verify native namespace creation and table-backed command dispatch.
 */

#include "common.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>

#include "bu/app.h"
#include "bu/str.h"
#include "tclcad.h"


static int
report_arguments(ClientData client_data, Tcl_Interp *interp, int argc,
	const char *argv[])
{
    const char *tag = static_cast<const char *>(client_data);
    Tcl_Obj *result = Tcl_NewListObj(0, NULL);
    int i;

    Tcl_ListObjAppendElement(interp, result, Tcl_NewStringObj(tag, -1));
    for (i = 0; i < argc; i++) {
	Tcl_ListObjAppendElement(interp, result,
		Tcl_NewStringObj(argv[i], -1));
    }
    Tcl_SetObjResult(interp, result);
    return TCL_OK;
}


static int
report_object_arguments(ClientData client_data, Tcl_Interp *interp, int objc,
	Tcl_Obj *const objv[])
{
    const char *tag = static_cast<const char *>(client_data);
    Tcl_Obj *result = Tcl_NewListObj(0, NULL);
    int i;

    Tcl_ListObjAppendElement(interp, result, Tcl_NewStringObj(tag, -1));
    for (i = 0; i < objc; i++)
	Tcl_ListObjAppendElement(interp, result, objv[i]);
    Tcl_SetObjResult(interp, result);
    return TCL_OK;
}


static bool
eval_result_is(Tcl_Interp *interp, const char *script, const char *expected)
{
    if (Tcl_Eval(interp, script) == TCL_OK &&
	BU_STR_EQUAL(Tcl_GetStringResult(interp), expected))
	return true;

    std::fprintf(stderr, "%s failed: expected \"%s\", got \"%s\"\n",
	script, expected, Tcl_GetStringResult(interp));
    return false;
}


static bool
root_commands(Tcl_Interp *interp, std::set<std::string> &commands)
{
    Tcl_Obj **command_objects = NULL;
    int command_count = 0;

    commands.clear();
    if (Tcl_Eval(interp, "info commands ::*") != TCL_OK ||
	Tcl_ListObjGetElements(interp, Tcl_GetObjResult(interp),
	    &command_count, &command_objects) != TCL_OK) {
	std::fprintf(stderr, "Unable to inspect Tcl commands: %s\n",
		Tcl_GetStringResult(interp));
	return false;
    }

    for (int i = 0; i < command_count; i++)
	commands.insert(Tcl_GetString(command_objects[i]));
    Tcl_ResetResult(interp);
    return true;
}


static bool
collect_native_inventory(Tcl_Interp *interp,
	std::map<std::string, std::set<std::string>> &inventory)
{
    struct initializer {
	const char *component;
	int (*function)(Tcl_Interp *);
    };
    static const initializer initializers[] = {
	{"bu", Bu_Init},
	{"bn", Bn_Init},
	{"rt", Rt_Init},
	{"dm", Dm_Init},
	{"ged", Ged_Init},
	{"history", Cho_Init}
    };
    std::set<std::string> before;

    if (Tcl_Init(interp) != TCL_OK) {
	std::fprintf(stderr, "Tcl initialization failed: %s\n",
		Tcl_GetStringResult(interp));
	return false;
    }
    if (!root_commands(interp, before))
	return false;

    for (const initializer &entry : initializers) {
	std::set<std::string> after;
	if (entry.function(interp) != TCL_OK) {
	    std::fprintf(stderr, "%s initialization failed: %s\n",
		    entry.component, Tcl_GetStringResult(interp));
	    return false;
	}
	if (!root_commands(interp, after))
	    return false;
	std::set_difference(after.begin(), after.end(), before.begin(),
		before.end(), std::inserter(inventory[entry.component],
		    inventory[entry.component].end()));
	before.swap(after);
    }

    return true;
}


static void
print_native_inventory(
    const std::map<std::string, std::set<std::string>> &inventory)
{
    for (const auto &component : inventory) {
	for (const std::string &command : component.second) {
	    std::printf("native %s %s legacy {}\n",
		    component.first.c_str(), command.c_str());
	}
    }
}


static bool
validate_native_inventory(Tcl_Interp *interp, const char *policy_script,
	const char *manifest,
	const std::map<std::string, std::set<std::string>> &inventory)
{
    Tcl_Obj *arguments[3] = {NULL, NULL, NULL};
    Tcl_Obj *inventory_object = Tcl_NewDictObj();
    int result;

    if (Tcl_EvalFile(interp, policy_script) != TCL_OK) {
	std::fprintf(stderr, "Unable to load namespace policy: %s\n",
		Tcl_GetStringResult(interp));
	return false;
    }

    Tcl_IncrRefCount(inventory_object);
    for (const auto &component : inventory) {
	Tcl_Obj *commands = Tcl_NewListObj(0, NULL);
	for (const std::string &command : component.second) {
	    Tcl_ListObjAppendElement(interp, commands,
		    Tcl_NewStringObj(command.c_str(), -1));
	}
	Tcl_DictObjPut(interp, inventory_object,
		Tcl_NewStringObj(component.first.c_str(), -1), commands);
    }

    arguments[0] = Tcl_NewStringObj(
	    "::brlcad::namespace_policy::check_native", -1);
    arguments[1] = Tcl_NewStringObj(manifest, -1);
    arguments[2] = inventory_object;
    Tcl_IncrRefCount(arguments[0]);
    Tcl_IncrRefCount(arguments[1]);
    result = Tcl_EvalObjv(interp, 3, arguments, TCL_EVAL_GLOBAL);
    Tcl_DecrRefCount(arguments[1]);
    Tcl_DecrRefCount(arguments[0]);
    Tcl_DecrRefCount(inventory_object);

    if (result != TCL_OK) {
	std::fprintf(stderr, "Native Tcl namespace policy failed:\n%s\n",
		Tcl_GetStringResult(interp));
	return false;
    }

    return true;
}


int
main(int argc, const char **argv)
{
    static char canonical_tag[] = "canonical";
    static char private_tag[] = "private";
    static char object_tag[] = "object";
    static char private_object_tag[] = "private_object";
    static const struct tclcad_cmdtab commands[] = {
	{"echo", "legacy_echo", report_arguments, canonical_tag},
	{"private", NULL, report_arguments, private_tag},
	{NULL, NULL, NULL, NULL}
    };
    static const struct tclcad_objcmdtab object_commands[] = {
	{"echo", "legacy_object_echo", report_object_arguments, object_tag},
	{"private", NULL, report_object_arguments, private_object_tag},
	{NULL, NULL, NULL, NULL}
    };

    bool dump_native = argc == 2 && BU_STR_EQUAL(argv[1], "--dump-native");
    if (!dump_native && argc != 3) {
	std::fprintf(stderr,
		"Usage: %s policy-script manifest\n"
		"       %s --dump-native\n", argv[0], argv[0]);
	return 2;
    }

    bu_setprogname(argv[0]);
    Tcl_FindExecutable(argv[0]);

    Tcl_Interp *interp = Tcl_CreateInterp();
    if (!interp) {
	std::fprintf(stderr, "Unable to create a Tcl interpreter\n");
	return 1;
    }

    bool passed =
	tclcad_create_namespace(interp, "::brlcad::test::nested") == TCL_OK &&
	tclcad_create_namespace(interp, "::brlcad::test::nested") == TCL_OK &&
	Tcl_FindNamespace(interp, "::brlcad", NULL, TCL_GLOBAL_ONLY) != NULL &&
	Tcl_FindNamespace(interp, "::brlcad::test::nested", NULL,
	    TCL_GLOBAL_ONLY) != NULL &&
	tclcad_register_cmd_namespace(interp, "::brlcad::test::commands",
	    commands) == TCL_OK &&
	eval_result_is(interp, "::brlcad::test::commands echo one two",
	    "canonical legacy_echo one two") &&
	eval_result_is(interp, "legacy_echo three",
	    "canonical legacy_echo three") &&
	eval_result_is(interp, "::brlcad::test::commands private four",
	    "private private four") &&
	eval_result_is(interp,
	    "expr {[llength [info commands ::private]] == 0}", "1") &&
	eval_result_is(interp,
	    "catch {::brlcad::test::commands missing} message; set message",
	    "unknown subcommand \"missing\": must be one of echo private") &&
	eval_result_is(interp,
	    "catch {::brlcad::test::commands} message; set message",
	    "wrong # args: should be \"::brlcad::test::commands subcommand ?arg ...?\"") &&
	tclcad_register_objcmd_namespace(interp,
	    "::brlcad::test::object_commands", object_commands) == TCL_OK &&
	eval_result_is(interp,
	    "::brlcad::test::object_commands echo one two",
	    "object legacy_object_echo one two") &&
	eval_result_is(interp, "legacy_object_echo three",
	    "object legacy_object_echo three") &&
	eval_result_is(interp,
	    "::brlcad::test::object_commands private four",
	    "private_object private four") &&
	eval_result_is(interp,
	    "expr {[llength [info commands ::private_object]] == 0}", "1") &&
	eval_result_is(interp,
	    "catch {::brlcad::test::object_commands missing} message; set message",
	    "unknown subcommand \"missing\": must be one of echo private");

    if (!passed)
	std::fprintf(stderr, "Native namespace check failed: %s\n",
	    Tcl_GetStringResult(interp));

    Tcl_ResetResult(interp);
    if (tclcad_create_namespace(interp, "relative") != TCL_ERROR ||
	!BU_STR_EQUAL(Tcl_GetStringResult(interp),
	    "Tcl namespace name must be absolute and non-empty")) {
	std::fprintf(stderr, "Relative namespace name was not rejected: %s\n",
	    Tcl_GetStringResult(interp));
	passed = false;
    }

    Tcl_DeleteInterp(interp);
    if (!passed)
	return 1;

    interp = Tcl_CreateInterp();
    if (!interp) {
	std::fprintf(stderr, "Unable to create a Tcl interpreter\n");
	return 1;
    }

    std::map<std::string, std::set<std::string>> native_inventory;
    passed = collect_native_inventory(interp, native_inventory);
    if (passed && dump_native)
	print_native_inventory(native_inventory);
    if (passed && !dump_native)
	passed = validate_native_inventory(interp, argv[1], argv[2],
		native_inventory);

    Tcl_DeleteInterp(interp);
    if (!passed)
	return 1;

    std::printf("Native Tcl namespace dispatch passed\n");
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
