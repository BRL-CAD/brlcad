/*                   T C L D U M M I E S . C
 *
 * @file TclDummies.c
 *
 * BRL-CAD
 *
 * Copyright (C) 2002-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
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

#define STATIC_BUILD
#include "tcl.h"


void                 Tcl_AppendResult() {}
void                 Tcl_DStringEndSublist() {}
void                 Tcl_DStringStartSublist() {}
char                *Tcl_DStringAppendElement() {return 0;}
int                  Tcl_SplitList() {return 0;}
void                 Tcl_DStringFree() {}
void                 Tcl_DStringResult() {}
void                 Tcl_DStringInit() {}
int                  Tcl_GetInt() {return 0;}
int                  Tcl_GetDouble() {return 0;}
int                  Tcl_GetBoolean() {return 0;}
int                  Tcl_LinkVar() {return 0;}
CONST84_RETURN char *Tcl_SetVar() {return 0;}
int                  Tcl_PkgProvide() {return 0;}
CONST84_RETURN char *Tcl_GetStringResult() {return 0;}
int                  Tcl_Eval() {return 0;}
char                *Tcl_GetStringFromObj() {return 0;}
int                  Tcl_ListObjGetElements() {return 0;}
void                 TclFreeObj() {}
int                  Tcl_ListObjAppendList() {return 0;}
Tcl_Obj             *Tcl_NewListObj() {return 0;}
Tcl_Obj             *Tcl_NewStringObj() {return 0;}
char                *Tcl_DStringAppend() {return 0;}
void                 Tcl_SetResult() {}
void                 Tcl_ResetResult() {}
int                  Tcl_GetBooleanFromObj() {return 0;}
int                  Tcl_GetDoubleFromObj() {return 0;}
int                  Tcl_GetIntFromObj() {return 0;}
char                *Tcl_GetString() {return 0;}
int                  Tcl_ListObjIndex() {return 0;}
int                  Tcl_ListObjLength() {return 0;}
Tcl_Command          Tcl_CreateCommand() {return 0;}
int                  Tcl_DeleteCommand() {return 0;}
void                 Tcl_SetObjResult() {}
void                 Tcl_Free() {}
Tcl_Obj             *Tcl_GetObjResult() {return 0;}
void                 Tcl_AppendElement() {}
int                  Tcl_EvalFile() {return 0;}
int                  Tcl_CreateAlias() {return 0;}
Tcl_Interp          *Tcl_CreateSlave() {return 0;}
Tcl_Interp          *Tcl_CreateInterp() {return 0;}
void                 Tcl_DeleteHashTable() {}
Tcl_HashEntry       *Tcl_NextHashEntry() {return 0;}
Tcl_HashEntry       *Tcl_FirstHashEntry() {return 0;}
void                 Tcl_InitHashTable() {}
int                  Tcl_VarEval() {return 0;}
Tcl_Obj             *Tcl_GetVar2Ex() {return 0;}
int                  Tcl_DoOneEvent() {return 0;}
int                  Tcl_GetCharLength() {return 0;}
CONST84_RETURN char *Tcl_GetVar() {return 0;}
int                  Tcl_Init() {return 0;}
int                  Tcl_Close() {return 0;}
void                 Tcl_DeleteChannelHandler() {}
int                  Tcl_Eof() {return 0;}
void                 Tcl_CreateChannelHandler() {}
Tcl_Channel          Tcl_MakeFileChannel() {return 0;}
void                 Tcl_SetErrno() {}
