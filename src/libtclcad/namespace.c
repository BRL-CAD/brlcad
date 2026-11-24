/*                    N A M E S P A C E . C
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
/** @file namespace.c
 *
 * Shared support for native Tcl command namespaces.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "tclcad.h"


struct tclcad_namespace_entry {
    const char *name;
    const char *legacy_name;
    Tcl_CmdProc *string_func;
    Tcl_ObjCmdProc *object_func;
    ClientData client_data;
};


struct tclcad_namespace_cmd {
    size_t command_count;
    struct tclcad_namespace_entry *commands;
};


struct tclcad_legacy_cmd {
    Tcl_CmdProc *func;
    ClientData client_data;
};


static void
tclcad_namespace_cmd_delete(ClientData client_data)
{
    struct tclcad_namespace_cmd *namespace_cmd =
	(struct tclcad_namespace_cmd *)client_data;
    size_t i;

    for (i = 0; i < namespace_cmd->command_count; i++) {
	bu_free((void *)namespace_cmd->commands[i].name,
		"Tcl command namespace subcommand name");
	if (namespace_cmd->commands[i].legacy_name) {
	    bu_free((void *)namespace_cmd->commands[i].legacy_name,
		    "legacy Tcl command name");
	}
    }
    bu_free(namespace_cmd->commands, "Tcl command namespace table");
    BU_PUT(namespace_cmd, struct tclcad_namespace_cmd);
}


static void
tclcad_legacy_cmd_delete(ClientData client_data)
{
    struct tclcad_legacy_cmd *legacy_cmd =
	(struct tclcad_legacy_cmd *)client_data;

    BU_PUT(legacy_cmd, struct tclcad_legacy_cmd);
}


static int
tclcad_call_cmd(Tcl_Interp *interp, Tcl_CmdProc *func,
	ClientData client_data, const char *command_name, int objc,
	Tcl_Obj *const objv[], int first_arg)
{
    const char **argv;
    int argc = objc - first_arg + 1;
    int i;
    int ret;

    argv = (const char **)bu_calloc((size_t)argc + 1, sizeof(char *),
	    "Tcl command arguments");
    argv[0] = command_name;
    for (i = first_arg; i < objc; i++)
	argv[i - first_arg + 1] = Tcl_GetString(objv[i]);

    ret = func(client_data, interp, argc, argv);
    bu_free(argv, "Tcl command arguments");
    return ret;
}


static int
tclcad_call_objcmd(Tcl_Interp *interp, Tcl_ObjCmdProc *func,
	ClientData client_data, const char *command_name, int objc,
	Tcl_Obj *const objv[], int first_arg)
{
    Tcl_Obj **call_objv;
    int call_objc = objc - first_arg + 1;
    int i;
    int ret;

    call_objv = (Tcl_Obj **)bu_calloc((size_t)call_objc, sizeof(Tcl_Obj *),
	    "Tcl object command arguments");
    call_objv[0] = Tcl_NewStringObj(command_name, -1);
    Tcl_IncrRefCount(call_objv[0]);
    for (i = first_arg; i < objc; i++)
	call_objv[i - first_arg + 1] = objv[i];

    ret = func(client_data, interp, call_objc, call_objv);
    Tcl_DecrRefCount(call_objv[0]);
    bu_free(call_objv, "Tcl object command arguments");
    return ret;
}


static int
tclcad_legacy_cmd_dispatch(ClientData client_data, Tcl_Interp *interp,
	int objc, Tcl_Obj *const objv[])
{
    struct tclcad_legacy_cmd *legacy_cmd =
	(struct tclcad_legacy_cmd *)client_data;

    return tclcad_call_cmd(interp, legacy_cmd->func,
	legacy_cmd->client_data, Tcl_GetString(objv[0]), objc, objv, 1);
}


static int
tclcad_namespace_cmd_dispatch(ClientData client_data, Tcl_Interp *interp,
	int objc, Tcl_Obj *const objv[])
{
    struct tclcad_namespace_cmd *namespace_cmd =
	(struct tclcad_namespace_cmd *)client_data;
    const char *subcommand;
    size_t i;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "subcommand ?arg ...?");
	return TCL_ERROR;
    }

    subcommand = Tcl_GetString(objv[1]);
    for (i = 0; i < namespace_cmd->command_count; i++) {
	struct tclcad_namespace_entry *cmd = &namespace_cmd->commands[i];
	if (BU_STR_EQUAL(cmd->name, subcommand)) {
	    const char *command_name = cmd->legacy_name ?
		cmd->legacy_name : cmd->name;
	    if (cmd->object_func) {
		return tclcad_call_objcmd(interp, cmd->object_func,
			cmd->client_data, command_name, objc, objv, 2);
	    }
	    return tclcad_call_cmd(interp, cmd->string_func,
		    cmd->client_data, command_name, objc, objv, 2);
	}
    }

    {
	Tcl_Obj *result = Tcl_ObjPrintf(
	    "unknown subcommand \"%s\": must be one of ", subcommand);
	Tcl_Obj *subcommands = Tcl_NewListObj(0, NULL);
	Tcl_IncrRefCount(subcommands);
	for (i = 0; i < namespace_cmd->command_count; i++) {
	    Tcl_ListObjAppendElement(interp, subcommands,
		    Tcl_NewStringObj(namespace_cmd->commands[i].name, -1));
	}
	Tcl_AppendObjToObj(result, subcommands);
	Tcl_DecrRefCount(subcommands);
	Tcl_SetObjResult(interp, result);
    }
    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "SUBCOMMAND", subcommand,
	    (char *)NULL);
    return TCL_ERROR;
}


static int
tclcad_namespace_name_valid(Tcl_Interp *interp, const char *namespace_name)
{
    const char *component;

    if (!namespace_name || namespace_name[0] != ':' ||
	namespace_name[1] != ':' || namespace_name[2] == '\0') {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"Tcl namespace name must be absolute and non-empty", -1));
	return 0;
    }

    component = namespace_name + 2;
    while (component) {
	const char *separator = strstr(component, "::");
	if (separator == component || (!separator && component[0] == '\0')) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "invalid Tcl namespace name \"%s\"", namespace_name));
	    return 0;
	}
	component = separator ? separator + 2 : NULL;
    }

    return 1;
}


int
tclcad_create_namespace(Tcl_Interp *interp, const char *namespace_name)
{
    struct bu_vls current = BU_VLS_INIT_ZERO;
    const char *component;

    if (!interp)
	return TCL_ERROR;
    if (!tclcad_namespace_name_valid(interp, namespace_name))
	return TCL_ERROR;

    bu_vls_strcpy(&current, "::");
    component = namespace_name + 2;
    while (component) {
	const char *separator = strstr(component, "::");
	size_t component_length = separator ?
	    (size_t)(separator - component) : strlen(component);

	bu_vls_strncat(&current, component, component_length);
	if (!Tcl_FindNamespace(interp, bu_vls_cstr(&current), NULL,
		TCL_GLOBAL_ONLY) &&
	    !Tcl_CreateNamespace(interp, bu_vls_cstr(&current), NULL, NULL)) {
	    bu_vls_free(&current);
	    return TCL_ERROR;
	}
	if (separator)
	    bu_vls_strcat(&current, "::");
	component = separator ? separator + 2 : NULL;
    }

    bu_vls_free(&current);
    return TCL_OK;
}


static int
tclcad_namespace_entries_valid(Tcl_Interp *interp,
	const struct tclcad_namespace_entry *commands)
{
    const struct tclcad_namespace_entry *cmd;
    const struct tclcad_namespace_entry *other;

    if (!commands) {
	Tcl_SetObjResult(interp,
		Tcl_NewStringObj("Tcl command table is NULL", -1));
	return 0;
    }

    for (cmd = commands; cmd->name; cmd++) {
	if (cmd->name[0] == '\0' ||
	    (!cmd->string_func && !cmd->object_func) ||
	    (cmd->string_func && cmd->object_func) ||
	    (cmd->legacy_name && cmd->legacy_name[0] == '\0')) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("invalid Tcl command table entry", -1));
	    return 0;
	}
	for (other = cmd + 1; other->name; other++) {
	    if (BU_STR_EQUAL(cmd->name, other->name)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"duplicate Tcl subcommand \"%s\"", cmd->name));
		return 0;
	    }
	    if (cmd->legacy_name && other->legacy_name &&
		BU_STR_EQUAL(cmd->legacy_name, other->legacy_name)) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			"duplicate legacy Tcl command \"%s\"",
			cmd->legacy_name));
		return 0;
	    }
	}
    }

    return 1;
}


static int
tclcad_create_legacy_cmd(Tcl_Interp *interp,
	const struct tclcad_namespace_entry *cmd)
{
    struct tclcad_legacy_cmd *legacy_cmd;

    if (cmd->object_func) {
	return Tcl_CreateObjCommand(interp, cmd->legacy_name,
		cmd->object_func, cmd->client_data, NULL) ? TCL_OK : TCL_ERROR;
    }

    BU_GET(legacy_cmd, struct tclcad_legacy_cmd);
    legacy_cmd->func = cmd->string_func;
    legacy_cmd->client_data = cmd->client_data;
    if (!Tcl_CreateObjCommand(interp, cmd->legacy_name,
	    tclcad_legacy_cmd_dispatch, (ClientData)legacy_cmd,
	    tclcad_legacy_cmd_delete)) {
	BU_PUT(legacy_cmd, struct tclcad_legacy_cmd);
	return TCL_ERROR;
    }

    return TCL_OK;
}


static int
tclcad_register_namespace(Tcl_Interp *interp, const char *namespace_name,
	const struct tclcad_namespace_entry *commands)
{
    struct tclcad_namespace_cmd *namespace_cmd;
    const struct tclcad_namespace_entry *cmd;
    size_t command_count = 0;
    size_t i;

    if (!interp)
	return TCL_ERROR;
    if (!tclcad_namespace_entries_valid(interp, commands) ||
	tclcad_create_namespace(interp, namespace_name) != TCL_OK)
	return TCL_ERROR;

    for (cmd = commands; cmd->name; cmd++)
	command_count++;
    if (command_count == 0) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"Tcl command table is empty", -1));
	return TCL_ERROR;
    }

    for (cmd = commands; cmd->name; cmd++) {
	if (cmd->legacy_name &&
	    tclcad_create_legacy_cmd(interp, cmd) != TCL_OK)
	    return TCL_ERROR;
    }

    BU_GET(namespace_cmd, struct tclcad_namespace_cmd);
    namespace_cmd->command_count = command_count;
    namespace_cmd->commands = (struct tclcad_namespace_entry *)bu_calloc(
	    command_count, sizeof(struct tclcad_namespace_entry),
	    "Tcl command namespace table");
    for (i = 0; i < command_count; i++) {
	namespace_cmd->commands[i] = commands[i];
	namespace_cmd->commands[i].name = bu_strdup(commands[i].name);
	if (commands[i].legacy_name) {
	    namespace_cmd->commands[i].legacy_name =
		bu_strdup(commands[i].legacy_name);
	}
    }

    if (!Tcl_CreateObjCommand(interp, namespace_name,
	    tclcad_namespace_cmd_dispatch, (ClientData)namespace_cmd,
	    tclcad_namespace_cmd_delete)) {
	tclcad_namespace_cmd_delete((ClientData)namespace_cmd);
	return TCL_ERROR;
    }

    return TCL_OK;
}


int
tclcad_register_cmd_namespace(Tcl_Interp *interp,
	const char *namespace_name, const struct tclcad_cmdtab *cmds)
{
    struct tclcad_namespace_entry *commands;
    const struct tclcad_cmdtab *cmd;
    size_t command_count = 0;
    size_t i;
    int ret;

    if (!cmds)
	return tclcad_register_namespace(interp, namespace_name, NULL);
    for (cmd = cmds; cmd->tcc_name; cmd++)
	command_count++;

    commands = (struct tclcad_namespace_entry *)bu_calloc(command_count + 1,
	    sizeof(struct tclcad_namespace_entry), "Tcl command table adapter");
    for (i = 0; i < command_count; i++) {
	commands[i].name = cmds[i].tcc_name;
	commands[i].legacy_name = cmds[i].tcc_legacy_name;
	commands[i].string_func = cmds[i].tcc_func;
	commands[i].client_data = cmds[i].tcc_client_data;
    }

    ret = tclcad_register_namespace(interp, namespace_name, commands);
    bu_free(commands, "Tcl command table adapter");
    return ret;
}


int
tclcad_register_objcmd_namespace(Tcl_Interp *interp,
	const char *namespace_name, const struct tclcad_objcmdtab *cmds)
{
    struct tclcad_namespace_entry *commands;
    const struct tclcad_objcmdtab *cmd;
    size_t command_count = 0;
    size_t i;
    int ret;

    if (!cmds)
	return tclcad_register_namespace(interp, namespace_name, NULL);
    for (cmd = cmds; cmd->tcc_name; cmd++)
	command_count++;

    commands = (struct tclcad_namespace_entry *)bu_calloc(command_count + 1,
	    sizeof(struct tclcad_namespace_entry),
	    "Tcl object command table adapter");
    for (i = 0; i < command_count; i++) {
	commands[i].name = cmds[i].tcc_name;
	commands[i].legacy_name = cmds[i].tcc_legacy_name;
	commands[i].object_func = cmds[i].tcc_func;
	commands[i].client_data = cmds[i].tcc_client_data;
    }

    ret = tclcad_register_namespace(interp, namespace_name, commands);
    bu_free(commands, "Tcl object command table adapter");
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
