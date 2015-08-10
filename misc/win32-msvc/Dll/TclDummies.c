/*                    T C L D U M M I E S . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2014 United States Government as represented by
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
/** @file TclDummies.c
 *
 *  Comments -
 *      TCL dummy functions to make BRL-CAD work without TCL/TK
 *
 *  Author -
 *      Daniel Rossberg
 *
 *  Source -
 *      IABG mbH
 *      Einsteinstr. 20, 85521 Ottobrunn, Germany
 *
 * $Id$
 */

#include "common.h"

#define STATIC_BUILD
#include "tcl.h"


void                 Tcl_AppendResult(Tcl_Interp *UNUSED(interp), ...) {}
void                 Tcl_DStringEndSublist(Tcl_DString *UNUSED(dsPtr)) {}
void                 Tcl_DStringStartSublist(Tcl_DString *UNUSED(dsPtr)) {}
char                *Tcl_DStringAppendElement(Tcl_DString *UNUSED(dsPtr), CONST char *UNUSED(element)) {return 0;}
int                  Tcl_SplitList(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(listStr), int *UNUSED(argcPtr), CONST84 char ***UNUSED(argvPtr)) {return 0;}
void                 Tcl_DStringFree(Tcl_DString *UNUSED(dsPtr)) {}
void                 Tcl_DStringResult(Tcl_Interp *UNUSED(interp), Tcl_DString *UNUSED(dsPtr)) {}
void                 Tcl_DStringInit(Tcl_DString *UNUSED(dsPtr)) {}
int                  Tcl_GetInt(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(src), int *UNUSED(intPtr)) {return 0;}
int                  Tcl_GetDouble(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(src), double *UNUSED(doublePtr)) {return 0;}
int                  Tcl_GetBoolean(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(src), int *UNUSED(boolPtr)) {return 0;}
int                  Tcl_LinkVar(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(varName), char *UNUSED(addr), int UNUSED(type)) {return 0;}
CONST84_RETURN char *Tcl_SetVar(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(varName), CONST char *UNUSED(newValue), int UNUSED(flags)) {return 0;}
int                  Tcl_PkgProvide(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(name), CONST char *UNUSED(version)) {return 0;}
CONST84_RETURN char *Tcl_GetStringResult(Tcl_Interp *UNUSED(interp)) {return 0;}
int                  Tcl_Eval(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(script)) {return 0;}
char                *Tcl_GetStringFromObj(Tcl_Obj *UNUSED(objPtr), int *UNUSED(lengthPtr)) {return 0;}
int                  Tcl_ListObjGetElements(Tcl_Interp *UNUSED(interp), Tcl_Obj *UNUSED(listPtr), int *UNUSED(objcPtr), Tcl_Obj ***UNUSED(objvPtr)) {return 0;}
void                 TclFreeObj(Tcl_Obj *UNUSED(objPtr)) {}
int                  Tcl_ListObjAppendList(Tcl_Interp *UNUSED(interp), Tcl_Obj *UNUSED(listPtr), Tcl_Obj *UNUSED(elemListPtr)) {return 0;}
Tcl_Obj             *Tcl_NewListObj(int UNUSED(objc), Tcl_Obj *CONST UNUSED(objv[])) {return 0;}
Tcl_Obj             *Tcl_NewStringObj(CONST char *UNUSED(bytes), int UNUSED(length)) {return 0;}
char                *Tcl_DStringAppend(Tcl_DString *UNUSED(dsPtr), CONST char *UNUSED(bytes), int UNUSED(length)) {return 0;}
void                 Tcl_SetResult(Tcl_Interp *UNUSED(interp), char *UNUSED(result), Tcl_FreeProc *UNUSED(freeProc)) {}
void                 Tcl_ResetResult(Tcl_Interp *UNUSED(interp)) {}
int                  Tcl_GetBooleanFromObj(Tcl_Interp *UNUSED(interp), Tcl_Obj *UNUSED(objPtr), int *UNUSED(boolPtr)) {return 0;}
int                  Tcl_GetDoubleFromObj(Tcl_Interp *UNUSED(interp), Tcl_Obj *UNUSED(objPtr), double *UNUSED(doublePtr)) {return 0;}
int                  Tcl_GetIntFromObj(Tcl_Interp *UNUSED(interp), Tcl_Obj *UNUSED(objPtr), int *UNUSED(intPtr)) {return 0;}
char                *Tcl_GetString(Tcl_Obj *UNUSED(objPtr)) {return 0;}
int                  Tcl_ListObjIndex(Tcl_Interp *UNUSED(interp), Tcl_Obj *UNUSED(listPtr), int UNUSED(index), Tcl_Obj **UNUSED(objPtrPtr)) {return 0;}
int                  Tcl_ListObjLength(Tcl_Interp *UNUSED(interp), Tcl_Obj *UNUSED(listPtr), int *UNUSED(lengthPtr)) {return 0;}
Tcl_Command          Tcl_CreateCommand(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(cmdName), Tcl_CmdProc *UNUSED(proc), ClientData UNUSED(clientData), Tcl_CmdDeleteProc *UNUSED(deleteProc)) {return 0;}
int                  Tcl_DeleteCommand(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(cmdName)) {return 0;}
void                 Tcl_SetObjResult(Tcl_Interp *UNUSED(interp), Tcl_Obj *UNUSED(resultObjPtr)) {}
void                 Tcl_Free(char *UNUSED(ptr)) {}
Tcl_Obj             *Tcl_GetObjResult(Tcl_Interp *UNUSED(interp)) {return 0;}
void                 Tcl_AppendElement(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(element)) {}
int                  Tcl_EvalFile(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(fileName)) {return 0;}
int                  Tcl_CreateAlias(Tcl_Interp *UNUSED(slave), CONST char *UNUSED(slaveCmd), Tcl_Interp *UNUSED(target), CONST char *UNUSED(targetCmd), int UNUSED(argc), CONST84 char *CONST *UNUSED(argv)) {return 0;}
Tcl_Interp          *Tcl_CreateSlave(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(slaveName), int UNUSED(isSafe)) {return 0;}
Tcl_Interp          *Tcl_CreateInterp(void) {return 0;}
void                 Tcl_DeleteHashTable(Tcl_HashTable *UNUSED(tablePtr)) {}
Tcl_HashEntry       *Tcl_NextHashEntry(Tcl_HashSearch *UNUSED(searchPtr)) {return 0;}
Tcl_HashEntry       *Tcl_FirstHashEntry(Tcl_HashTable *UNUSED(tablePtr), Tcl_HashSearch *UNUSED(searchPtr)) {return 0;}
void                 Tcl_InitHashTable(Tcl_HashTable *UNUSED(tablePtr), int UNUSED(keyType)) {}
int                  Tcl_VarEval(Tcl_Interp *UNUSED(interp), ...) {return 0;}
Tcl_Obj             *Tcl_GetVar2Ex(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(part1), CONST char *UNUSED(part2), int UNUSED(flags)) {return 0;}
int                  Tcl_DoOneEvent(int UNUSED(flags)) {return 0;}
int                  Tcl_GetCharLength(Tcl_Obj *UNUSED(objPtr)) {return 0;}
CONST84_RETURN char *Tcl_GetVar(Tcl_Interp *UNUSED(interp), CONST char *UNUSED(varName), int UNUSED(flags)) {return 0;}
int                  Tcl_Init(Tcl_Interp *UNUSED(interp)) {return 0;}
int                  Tcl_Close(Tcl_Interp *UNUSED(interp), Tcl_Channel UNUSED(chan)) {return 0;}
void                 Tcl_DeleteChannelHandler(Tcl_Channel UNUSED(chan), Tcl_ChannelProc *UNUSED(proc), ClientData UNUSED(clientData)) {}
int                  Tcl_Eof(Tcl_Channel UNUSED(chan)) {return 0;}
void                 Tcl_CreateChannelHandler(Tcl_Channel UNUSED(chan), int UNUSED(mask), Tcl_ChannelProc *UNUSED(proc), ClientData UNUSED(clientData)) {}
Tcl_Channel          Tcl_MakeFileChannel(ClientData UNUSED(handle), int UNUSED(mode)) {return 0;}
void                 Tcl_SetErrno(int UNUSED(err)) {}
Tcl_Obj             *Tcl_NewDoubleObj(double UNUSED(doubleValue)) {return 0;}
Tcl_Obj             *Tcl_NewIntObj(int UNUSED(intValue)) {return 0;}
