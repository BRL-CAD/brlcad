# tcl.decls --
#
#	This file contains the declarations for all supported public
#	functions that are exported by the Tcl library via the stubs table.
#	This file is used to generate the tclDecls.h, tclPlatDecls.h,
#	tclStub.c, and tclPlatStub.c files.
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# Copyright (c) 2001, 2002 by Kevin B. Kenny.  All rights reserved.
# Copyright (c) 2007 Daniel A. Steffen <das@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id$

library tcl

# Define the tcl interface with several sub interfaces:
#     tclPlat	 - platform specific public
#     tclInt	 - generic private
#     tclPlatInt - platform specific private

interface tcl
hooks {tclPlat tclInt tclIntPlat}

# Declare each of the functions in the public Tcl interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

declare 0 generic {
    int Tcl_PkgProvideEx(Tcl_Interp *interp, CONST char *name,
	    CONST char *version, ClientData clientData)
}
declare 1 generic {
    CONST84_RETURN char *Tcl_PkgRequireEx(Tcl_Interp *interp,
	    CONST char *name, CONST char *version, int exact,
	    ClientData *clientDataPtr)
}
declare 2 generic {
    void Tcl_Panic(CONST char *format, ...)
}
declare 3 generic {
    char *Tcl_Alloc(unsigned int size)
}
declare 4 generic {
    void Tcl_Free(char *ptr)
}
declare 5 generic {
    char *Tcl_Realloc(char *ptr, unsigned int size)
}
declare 6 generic {
    char *Tcl_DbCkalloc(unsigned int size, CONST char *file, int line)
}
declare 7 generic {
    int Tcl_DbCkfree(char *ptr, CONST char *file, int line)
}
declare 8 generic {
    char *Tcl_DbCkrealloc(char *ptr, unsigned int size,
	    CONST char *file, int line)
}

# Tcl_CreateFileHandler and Tcl_DeleteFileHandler are only available on unix,
# but they are part of the old generic interface, so we include them here for
# compatibility reasons.

declare 9 unix {
    void Tcl_CreateFileHandler(int fd, int mask, Tcl_FileProc *proc,
	    ClientData clientData)
}
declare 10 unix {
    void Tcl_DeleteFileHandler(int fd)
}
declare 11 generic {
    void Tcl_SetTimer(Tcl_Time *timePtr)
}
declare 12 generic {
    void Tcl_Sleep(int ms)
}
declare 13 generic {
    int Tcl_WaitForEvent(Tcl_Time *timePtr)
}
declare 14 generic {
    int Tcl_AppendAllObjTypes(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 15 generic {
    void Tcl_AppendStringsToObj(Tcl_Obj *objPtr, ...)
}
declare 16 generic {
    void Tcl_AppendToObj(Tcl_Obj *objPtr, CONST char *bytes, int length)
}
declare 17 generic {
    Tcl_Obj *Tcl_ConcatObj(int objc, Tcl_Obj *CONST objv[])
}
declare 18 generic {
    int Tcl_ConvertToType(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    Tcl_ObjType *typePtr)
}
declare 19 generic {
    void Tcl_DbDecrRefCount(Tcl_Obj *objPtr, CONST char *file, int line)
}
declare 20 generic {
    void Tcl_DbIncrRefCount(Tcl_Obj *objPtr, CONST char *file, int line)
}
declare 21 generic {
    int Tcl_DbIsShared(Tcl_Obj *objPtr, CONST char *file, int line)
}
declare 22 generic {
    Tcl_Obj *Tcl_DbNewBooleanObj(int boolValue, CONST char *file, int line)
}
declare 23 generic {
    Tcl_Obj *Tcl_DbNewByteArrayObj(CONST unsigned char *bytes, int length,
	    CONST char *file, int line)
}
declare 24 generic {
    Tcl_Obj *Tcl_DbNewDoubleObj(double doubleValue, CONST char *file,
	    int line)
}
declare 25 generic {
    Tcl_Obj *Tcl_DbNewListObj(int objc, Tcl_Obj *CONST *objv,
	    CONST char *file, int line)
}
declare 26 generic {
    Tcl_Obj *Tcl_DbNewLongObj(long longValue, CONST char *file, int line)
}
declare 27 generic {
    Tcl_Obj *Tcl_DbNewObj(CONST char *file, int line)
}
declare 28 generic {
    Tcl_Obj *Tcl_DbNewStringObj(CONST char *bytes, int length,
	    CONST char *file, int line)
}
declare 29 generic {
    Tcl_Obj *Tcl_DuplicateObj(Tcl_Obj *objPtr)
}
declare 30 generic {
    void TclFreeObj(Tcl_Obj *objPtr)
}
declare 31 generic {
    int Tcl_GetBoolean(Tcl_Interp *interp, CONST char *src, int *boolPtr)
}
declare 32 generic {
    int Tcl_GetBooleanFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    int *boolPtr)
}
declare 33 generic {
    unsigned char *Tcl_GetByteArrayFromObj(Tcl_Obj *objPtr, int *lengthPtr)
}
declare 34 generic {
    int Tcl_GetDouble(Tcl_Interp *interp, CONST char *src, double *doublePtr)
}
declare 35 generic {
    int Tcl_GetDoubleFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    double *doublePtr)
}
declare 36 generic {
    int Tcl_GetIndexFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    CONST84 char **tablePtr, CONST char *msg, int flags, int *indexPtr)
}
declare 37 generic {
    int Tcl_GetInt(Tcl_Interp *interp, CONST char *src, int *intPtr)
}
declare 38 generic {
    int Tcl_GetIntFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int *intPtr)
}
declare 39 generic {
    int Tcl_GetLongFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, long *longPtr)
}
declare 40 generic {
    Tcl_ObjType *Tcl_GetObjType(CONST char *typeName)
}
declare 41 generic {
    char *Tcl_GetStringFromObj(Tcl_Obj *objPtr, int *lengthPtr)
}
declare 42 generic {
    void Tcl_InvalidateStringRep(Tcl_Obj *objPtr)
}
declare 43 generic {
    int Tcl_ListObjAppendList(Tcl_Interp *interp, Tcl_Obj *listPtr,
	    Tcl_Obj *elemListPtr)
}
declare 44 generic {
    int Tcl_ListObjAppendElement(Tcl_Interp *interp, Tcl_Obj *listPtr,
	    Tcl_Obj *objPtr)
}
declare 45 generic {
    int Tcl_ListObjGetElements(Tcl_Interp *interp, Tcl_Obj *listPtr,
	    int *objcPtr, Tcl_Obj ***objvPtr)
}
declare 46 generic {
    int Tcl_ListObjIndex(Tcl_Interp *interp, Tcl_Obj *listPtr, int index,
	    Tcl_Obj **objPtrPtr)
}
declare 47 generic {
    int Tcl_ListObjLength(Tcl_Interp *interp, Tcl_Obj *listPtr,
	    int *lengthPtr)
}
declare 48 generic {
    int Tcl_ListObjReplace(Tcl_Interp *interp, Tcl_Obj *listPtr, int first,
	    int count, int objc, Tcl_Obj *CONST objv[])
}
declare 49 generic {
    Tcl_Obj *Tcl_NewBooleanObj(int boolValue)
}
declare 50 generic {
    Tcl_Obj *Tcl_NewByteArrayObj(CONST unsigned char *bytes, int length)
}
declare 51 generic {
    Tcl_Obj *Tcl_NewDoubleObj(double doubleValue)
}
declare 52 generic {
    Tcl_Obj *Tcl_NewIntObj(int intValue)
}
declare 53 generic {
    Tcl_Obj *Tcl_NewListObj(int objc, Tcl_Obj *CONST objv[])
}
declare 54 generic {
    Tcl_Obj *Tcl_NewLongObj(long longValue)
}
declare 55 generic {
    Tcl_Obj *Tcl_NewObj(void)
}
declare 56 generic {
    Tcl_Obj *Tcl_NewStringObj(CONST char *bytes, int length)
}
declare 57 generic {
    void Tcl_SetBooleanObj(Tcl_Obj *objPtr, int boolValue)
}
declare 58 generic {
    unsigned char *Tcl_SetByteArrayLength(Tcl_Obj *objPtr, int length)
}
declare 59 generic {
    void Tcl_SetByteArrayObj(Tcl_Obj *objPtr, CONST unsigned char *bytes,
	    int length)
}
declare 60 generic {
    void Tcl_SetDoubleObj(Tcl_Obj *objPtr, double doubleValue)
}
declare 61 generic {
    void Tcl_SetIntObj(Tcl_Obj *objPtr, int intValue)
}
declare 62 generic {
    void Tcl_SetListObj(Tcl_Obj *objPtr, int objc, Tcl_Obj *CONST objv[])
}
declare 63 generic {
    void Tcl_SetLongObj(Tcl_Obj *objPtr, long longValue)
}
declare 64 generic {
    void Tcl_SetObjLength(Tcl_Obj *objPtr, int length)
}
declare 65 generic {
    void Tcl_SetStringObj(Tcl_Obj *objPtr, CONST char *bytes, int length)
}
declare 66 generic {
    void Tcl_AddErrorInfo(Tcl_Interp *interp, CONST char *message)
}
declare 67 generic {
    void Tcl_AddObjErrorInfo(Tcl_Interp *interp, CONST char *message,
	    int length)
}
declare 68 generic {
    void Tcl_AllowExceptions(Tcl_Interp *interp)
}
declare 69 generic {
    void Tcl_AppendElement(Tcl_Interp *interp, CONST char *element)
}
declare 70 generic {
    void Tcl_AppendResult(Tcl_Interp *interp, ...)
}
declare 71 generic {
    Tcl_AsyncHandler Tcl_AsyncCreate(Tcl_AsyncProc *proc,
	    ClientData clientData)
}
declare 72 generic {
    void Tcl_AsyncDelete(Tcl_AsyncHandler async)
}
declare 73 generic {
    int Tcl_AsyncInvoke(Tcl_Interp *interp, int code)
}
declare 74 generic {
    void Tcl_AsyncMark(Tcl_AsyncHandler async)
}
declare 75 generic {
    int Tcl_AsyncReady(void)
}
declare 76 generic {
    void Tcl_BackgroundError(Tcl_Interp *interp)
}
declare 77 generic {
    char Tcl_Backslash(CONST char *src, int *readPtr)
}
declare 78 generic {
    int Tcl_BadChannelOption(Tcl_Interp *interp, CONST char *optionName,
	    CONST char *optionList)
}
declare 79 generic {
    void Tcl_CallWhenDeleted(Tcl_Interp *interp, Tcl_InterpDeleteProc *proc,
	    ClientData clientData)
}
declare 80 generic {
    void Tcl_CancelIdleCall(Tcl_IdleProc *idleProc, ClientData clientData)
}
declare 81 generic {
    int Tcl_Close(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 82 generic {
    int Tcl_CommandComplete(CONST char *cmd)
}
declare 83 generic {
    char *Tcl_Concat(int argc, CONST84 char *CONST *argv)
}
declare 84 generic {
    int Tcl_ConvertElement(CONST char *src, char *dst, int flags)
}
declare 85 generic {
    int Tcl_ConvertCountedElement(CONST char *src, int length, char *dst,
	    int flags)
}
declare 86 generic {
    int Tcl_CreateAlias(Tcl_Interp *slave, CONST char *slaveCmd,
	    Tcl_Interp *target, CONST char *targetCmd, int argc,
	    CONST84 char *CONST *argv)
}
declare 87 generic {
    int Tcl_CreateAliasObj(Tcl_Interp *slave, CONST char *slaveCmd,
	    Tcl_Interp *target, CONST char *targetCmd, int objc,
	    Tcl_Obj *CONST objv[])
}
declare 88 generic {
    Tcl_Channel Tcl_CreateChannel(Tcl_ChannelType *typePtr,
	    CONST char *chanName, ClientData instanceData, int mask)
}
declare 89 generic {
    void Tcl_CreateChannelHandler(Tcl_Channel chan, int mask,
	    Tcl_ChannelProc *proc, ClientData clientData)
}
declare 90 generic {
    void Tcl_CreateCloseHandler(Tcl_Channel chan, Tcl_CloseProc *proc,
	    ClientData clientData)
}
declare 91 generic {
    Tcl_Command Tcl_CreateCommand(Tcl_Interp *interp, CONST char *cmdName,
	    Tcl_CmdProc *proc, ClientData clientData,
	    Tcl_CmdDeleteProc *deleteProc)
}
declare 92 generic {
    void Tcl_CreateEventSource(Tcl_EventSetupProc *setupProc,
	    Tcl_EventCheckProc *checkProc, ClientData clientData)
}
declare 93 generic {
    void Tcl_CreateExitHandler(Tcl_ExitProc *proc, ClientData clientData)
}
declare 94 generic {
    Tcl_Interp *Tcl_CreateInterp(void)
}
declare 95 generic {
    void Tcl_CreateMathFunc(Tcl_Interp *interp, CONST char *name,
	    int numArgs, Tcl_ValueType *argTypes,
	    Tcl_MathProc *proc, ClientData clientData)
}
declare 96 generic {
    Tcl_Command Tcl_CreateObjCommand(Tcl_Interp *interp,
	    CONST char *cmdName,
	    Tcl_ObjCmdProc *proc, ClientData clientData,
	    Tcl_CmdDeleteProc *deleteProc)
}
declare 97 generic {
    Tcl_Interp *Tcl_CreateSlave(Tcl_Interp *interp, CONST char *slaveName,
	    int isSafe)
}
declare 98 generic {
    Tcl_TimerToken Tcl_CreateTimerHandler(int milliseconds,
	    Tcl_TimerProc *proc, ClientData clientData)
}
declare 99 generic {
    Tcl_Trace Tcl_CreateTrace(Tcl_Interp *interp, int level,
	    Tcl_CmdTraceProc *proc, ClientData clientData)
}
declare 100 generic {
    void Tcl_DeleteAssocData(Tcl_Interp *interp, CONST char *name)
}
declare 101 generic {
    void Tcl_DeleteChannelHandler(Tcl_Channel chan, Tcl_ChannelProc *proc,
	    ClientData clientData)
}
declare 102 generic {
    void Tcl_DeleteCloseHandler(Tcl_Channel chan, Tcl_CloseProc *proc,
	    ClientData clientData)
}
declare 103 generic {
    int Tcl_DeleteCommand(Tcl_Interp *interp, CONST char *cmdName)
}
declare 104 generic {
    int Tcl_DeleteCommandFromToken(Tcl_Interp *interp, Tcl_Command command)
}
declare 105 generic {
    void Tcl_DeleteEvents(Tcl_EventDeleteProc *proc, ClientData clientData)
}
declare 106 generic {
    void Tcl_DeleteEventSource(Tcl_EventSetupProc *setupProc,
	    Tcl_EventCheckProc *checkProc, ClientData clientData)
}
declare 107 generic {
    void Tcl_DeleteExitHandler(Tcl_ExitProc *proc, ClientData clientData)
}
declare 108 generic {
    void Tcl_DeleteHashEntry(Tcl_HashEntry *entryPtr)
}
declare 109 generic {
    void Tcl_DeleteHashTable(Tcl_HashTable *tablePtr)
}
declare 110 generic {
    void Tcl_DeleteInterp(Tcl_Interp *interp)
}
declare 111 generic {
    void Tcl_DetachPids(int numPids, Tcl_Pid *pidPtr)
}
declare 112 generic {
    void Tcl_DeleteTimerHandler(Tcl_TimerToken token)
}
declare 113 generic {
    void Tcl_DeleteTrace(Tcl_Interp *interp, Tcl_Trace trace)
}
declare 114 generic {
    void Tcl_DontCallWhenDeleted(Tcl_Interp *interp,
	    Tcl_InterpDeleteProc *proc, ClientData clientData)
}
declare 115 generic {
    int Tcl_DoOneEvent(int flags)
}
declare 116 generic {
    void Tcl_DoWhenIdle(Tcl_IdleProc *proc, ClientData clientData)
}
declare 117 generic {
    char *Tcl_DStringAppend(Tcl_DString *dsPtr, CONST char *bytes, int length)
}
declare 118 generic {
    char *Tcl_DStringAppendElement(Tcl_DString *dsPtr, CONST char *element)
}
declare 119 generic {
    void Tcl_DStringEndSublist(Tcl_DString *dsPtr)
}
declare 120 generic {
    void Tcl_DStringFree(Tcl_DString *dsPtr)
}
declare 121 generic {
    void Tcl_DStringGetResult(Tcl_Interp *interp, Tcl_DString *dsPtr)
}
declare 122 generic {
    void Tcl_DStringInit(Tcl_DString *dsPtr)
}
declare 123 generic {
    void Tcl_DStringResult(Tcl_Interp *interp, Tcl_DString *dsPtr)
}
declare 124 generic {
    void Tcl_DStringSetLength(Tcl_DString *dsPtr, int length)
}
declare 125 generic {
    void Tcl_DStringStartSublist(Tcl_DString *dsPtr)
}
declare 126 generic {
    int Tcl_Eof(Tcl_Channel chan)
}
declare 127 generic {
    CONST84_RETURN char *Tcl_ErrnoId(void)
}
declare 128 generic {
    CONST84_RETURN char *Tcl_ErrnoMsg(int err)
}
declare 129 generic {
    int Tcl_Eval(Tcl_Interp *interp, CONST char *script)
}
# This is obsolete, use Tcl_FSEvalFile
declare 130 generic {
    int Tcl_EvalFile(Tcl_Interp *interp, CONST char *fileName)
}
declare 131 generic {
    int Tcl_EvalObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 132 generic {
    void Tcl_EventuallyFree(ClientData clientData, Tcl_FreeProc *freeProc)
}
declare 133 generic {
    void Tcl_Exit(int status)
}
declare 134 generic {
    int Tcl_ExposeCommand(Tcl_Interp *interp, CONST char *hiddenCmdToken,
	    CONST char *cmdName)
}
declare 135 generic {
    int Tcl_ExprBoolean(Tcl_Interp *interp, CONST char *expr, int *ptr)
}
declare 136 generic {
    int Tcl_ExprBooleanObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int *ptr)
}
declare 137 generic {
    int Tcl_ExprDouble(Tcl_Interp *interp, CONST char *expr, double *ptr)
}
declare 138 generic {
    int Tcl_ExprDoubleObj(Tcl_Interp *interp, Tcl_Obj *objPtr, double *ptr)
}
declare 139 generic {
    int Tcl_ExprLong(Tcl_Interp *interp, CONST char *expr, long *ptr)
}
declare 140 generic {
    int Tcl_ExprLongObj(Tcl_Interp *interp, Tcl_Obj *objPtr, long *ptr)
}
declare 141 generic {
    int Tcl_ExprObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    Tcl_Obj **resultPtrPtr)
}
declare 142 generic {
    int Tcl_ExprString(Tcl_Interp *interp, CONST char *expr)
}
declare 143 generic {
    void Tcl_Finalize(void)
}
declare 144 generic {
    void Tcl_FindExecutable(CONST char *argv0)
}
declare 145 generic {
    Tcl_HashEntry *Tcl_FirstHashEntry(Tcl_HashTable *tablePtr,
	    Tcl_HashSearch *searchPtr)
}
declare 146 generic {
    int Tcl_Flush(Tcl_Channel chan)
}
declare 147 generic {
    void Tcl_FreeResult(Tcl_Interp *interp)
}
declare 148 generic {
    int Tcl_GetAlias(Tcl_Interp *interp, CONST char *slaveCmd,
	    Tcl_Interp **targetInterpPtr, CONST84 char **targetCmdPtr,
	    int *argcPtr, CONST84 char ***argvPtr)
}
declare 149 generic {
    int Tcl_GetAliasObj(Tcl_Interp *interp, CONST char *slaveCmd,
	    Tcl_Interp **targetInterpPtr, CONST84 char **targetCmdPtr,
	    int *objcPtr, Tcl_Obj ***objv)
}
declare 150 generic {
    ClientData Tcl_GetAssocData(Tcl_Interp *interp, CONST char *name,
	    Tcl_InterpDeleteProc **procPtr)
}
declare 151 generic {
    Tcl_Channel Tcl_GetChannel(Tcl_Interp *interp, CONST char *chanName,
	    int *modePtr)
}
declare 152 generic {
    int Tcl_GetChannelBufferSize(Tcl_Channel chan)
}
declare 153 generic {
    int Tcl_GetChannelHandle(Tcl_Channel chan, int direction,
	    ClientData *handlePtr)
}
declare 154 generic {
    ClientData Tcl_GetChannelInstanceData(Tcl_Channel chan)
}
declare 155 generic {
    int Tcl_GetChannelMode(Tcl_Channel chan)
}
declare 156 generic {
    CONST84_RETURN char *Tcl_GetChannelName(Tcl_Channel chan)
}
declare 157 generic {
    int Tcl_GetChannelOption(Tcl_Interp *interp, Tcl_Channel chan,
	    CONST char *optionName, Tcl_DString *dsPtr)
}
declare 158 generic {
    Tcl_ChannelType *Tcl_GetChannelType(Tcl_Channel chan)
}
declare 159 generic {
    int Tcl_GetCommandInfo(Tcl_Interp *interp, CONST char *cmdName,
	    Tcl_CmdInfo *infoPtr)
}
declare 160 generic {
    CONST84_RETURN char *Tcl_GetCommandName(Tcl_Interp *interp,
	    Tcl_Command command)
}
declare 161 generic {
    int Tcl_GetErrno(void)
}
declare 162 generic {
    CONST84_RETURN char *Tcl_GetHostName(void)
}
declare 163 generic {
    int Tcl_GetInterpPath(Tcl_Interp *askInterp, Tcl_Interp *slaveInterp)
}
declare 164 generic {
    Tcl_Interp *Tcl_GetMaster(Tcl_Interp *interp)
}
declare 165 generic {
    CONST char *Tcl_GetNameOfExecutable(void)
}
declare 166 generic {
    Tcl_Obj *Tcl_GetObjResult(Tcl_Interp *interp)
}

# Tcl_GetOpenFile is only available on unix, but it is a part of the old
# generic interface, so we inlcude it here for compatibility reasons.

declare 167 unix {
    int Tcl_GetOpenFile(Tcl_Interp *interp, CONST char *chanID, int forWriting,
	    int checkUsage, ClientData *filePtr)
}
# Obsolete.  Should now use Tcl_FSGetPathType which is objectified
# and therefore usually faster.
declare 168 generic {
    Tcl_PathType Tcl_GetPathType(CONST char *path)
}
declare 169 generic {
    int Tcl_Gets(Tcl_Channel chan, Tcl_DString *dsPtr)
}
declare 170 generic {
    int Tcl_GetsObj(Tcl_Channel chan, Tcl_Obj *objPtr)
}
declare 171 generic {
    int Tcl_GetServiceMode(void)
}
declare 172 generic {
    Tcl_Interp *Tcl_GetSlave(Tcl_Interp *interp, CONST char *slaveName)
}
declare 173 generic {
    Tcl_Channel Tcl_GetStdChannel(int type)
}
declare 174 generic {
    CONST84_RETURN char *Tcl_GetStringResult(Tcl_Interp *interp)
}
declare 175 generic {
    CONST84_RETURN char *Tcl_GetVar(Tcl_Interp *interp, CONST char *varName,
	    int flags)
}
declare 176 generic {
    CONST84_RETURN char *Tcl_GetVar2(Tcl_Interp *interp, CONST char *part1,
	    CONST char *part2, int flags)
}
declare 177 generic {
    int Tcl_GlobalEval(Tcl_Interp *interp, CONST char *command)
}
declare 178 generic {
    int Tcl_GlobalEvalObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 179 generic {
    int Tcl_HideCommand(Tcl_Interp *interp, CONST char *cmdName,
	    CONST char *hiddenCmdToken)
}
declare 180 generic {
    int Tcl_Init(Tcl_Interp *interp)
}
declare 181 generic {
    void Tcl_InitHashTable(Tcl_HashTable *tablePtr, int keyType)
}
declare 182 generic {
    int Tcl_InputBlocked(Tcl_Channel chan)
}
declare 183 generic {
    int Tcl_InputBuffered(Tcl_Channel chan)
}
declare 184 generic {
    int Tcl_InterpDeleted(Tcl_Interp *interp)
}
declare 185 generic {
    int Tcl_IsSafe(Tcl_Interp *interp)
}
# Obsolete, use Tcl_FSJoinPath
declare 186 generic {
    char *Tcl_JoinPath(int argc, CONST84 char *CONST *argv,
	    Tcl_DString *resultPtr)
}
declare 187 generic {
    int Tcl_LinkVar(Tcl_Interp *interp, CONST char *varName, char *addr,
	    int type)
}

# This slot is reserved for use by the plus patch:
#  declare 188 generic {
#	Tcl_MainLoop
#  }

declare 189 generic {
    Tcl_Channel Tcl_MakeFileChannel(ClientData handle, int mode)
}
declare 190 generic {
    int Tcl_MakeSafe(Tcl_Interp *interp)
}
declare 191 generic {
    Tcl_Channel Tcl_MakeTcpClientChannel(ClientData tcpSocket)
}
declare 192 generic {
    char *Tcl_Merge(int argc, CONST84 char *CONST *argv)
}
declare 193 generic {
    Tcl_HashEntry *Tcl_NextHashEntry(Tcl_HashSearch *searchPtr)
}
declare 194 generic {
    void Tcl_NotifyChannel(Tcl_Channel channel, int mask)
}
declare 195 generic {
    Tcl_Obj *Tcl_ObjGetVar2(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
	    Tcl_Obj *part2Ptr, int flags)
}
declare 196 generic {
    Tcl_Obj *Tcl_ObjSetVar2(Tcl_Interp *interp, Tcl_Obj *part1Ptr,
	    Tcl_Obj *part2Ptr, Tcl_Obj *newValuePtr, int flags)
}
declare 197 generic {
    Tcl_Channel Tcl_OpenCommandChannel(Tcl_Interp *interp, int argc,
	    CONST84 char **argv, int flags)
}
# This is obsolete, use Tcl_FSOpenFileChannel
declare 198 generic {
    Tcl_Channel Tcl_OpenFileChannel(Tcl_Interp *interp, CONST char *fileName,
	    CONST char *modeString, int permissions)
}
declare 199 generic {
    Tcl_Channel Tcl_OpenTcpClient(Tcl_Interp *interp, int port,
	    CONST char *address, CONST char *myaddr, int myport, int async)
}
declare 200 generic {
    Tcl_Channel Tcl_OpenTcpServer(Tcl_Interp *interp, int port,
	    CONST char *host, Tcl_TcpAcceptProc *acceptProc,
	    ClientData callbackData)
}
declare 201 generic {
    void Tcl_Preserve(ClientData data)
}
declare 202 generic {
    void Tcl_PrintDouble(Tcl_Interp *interp, double value, char *dst)
}
declare 203 generic {
    int Tcl_PutEnv(CONST char *assignment)
}
declare 204 generic {
    CONST84_RETURN char *Tcl_PosixError(Tcl_Interp *interp)
}
declare 205 generic {
    void Tcl_QueueEvent(Tcl_Event *evPtr, Tcl_QueuePosition position)
}
declare 206 generic {
    int Tcl_Read(Tcl_Channel chan, char *bufPtr, int toRead)
}
declare 207 generic {
    void Tcl_ReapDetachedProcs(void)
}
declare 208 generic {
    int Tcl_RecordAndEval(Tcl_Interp *interp, CONST char *cmd, int flags)
}
declare 209 generic {
    int Tcl_RecordAndEvalObj(Tcl_Interp *interp, Tcl_Obj *cmdPtr, int flags)
}
declare 210 generic {
    void Tcl_RegisterChannel(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 211 generic {
    void Tcl_RegisterObjType(Tcl_ObjType *typePtr)
}
declare 212 generic {
    Tcl_RegExp Tcl_RegExpCompile(Tcl_Interp *interp, CONST char *pattern)
}
declare 213 generic {
    int Tcl_RegExpExec(Tcl_Interp *interp, Tcl_RegExp regexp,
	    CONST char *text, CONST char *start)
}
declare 214 generic {
    int Tcl_RegExpMatch(Tcl_Interp *interp, CONST char *text,
	    CONST char *pattern)
}
declare 215 generic {
    void Tcl_RegExpRange(Tcl_RegExp regexp, int index,
	    CONST84 char **startPtr, CONST84 char **endPtr)
}
declare 216 generic {
    void Tcl_Release(ClientData clientData)
}
declare 217 generic {
    void Tcl_ResetResult(Tcl_Interp *interp)
}
declare 218 generic {
    int Tcl_ScanElement(CONST char *str, int *flagPtr)
}
declare 219 generic {
    int Tcl_ScanCountedElement(CONST char *str, int length, int *flagPtr)
}
# Obsolete
declare 220 generic {
    int Tcl_SeekOld(Tcl_Channel chan, int offset, int mode)
}
declare 221 generic {
    int Tcl_ServiceAll(void)
}
declare 222 generic {
    int Tcl_ServiceEvent(int flags)
}
declare 223 generic {
    void Tcl_SetAssocData(Tcl_Interp *interp, CONST char *name,
	    Tcl_InterpDeleteProc *proc, ClientData clientData)
}
declare 224 generic {
    void Tcl_SetChannelBufferSize(Tcl_Channel chan, int sz)
}
declare 225 generic {
    int Tcl_SetChannelOption(Tcl_Interp *interp, Tcl_Channel chan,
	    CONST char *optionName, CONST char *newValue)
}
declare 226 generic {
    int Tcl_SetCommandInfo(Tcl_Interp *interp, CONST char *cmdName,
	    CONST Tcl_CmdInfo *infoPtr)
}
declare 227 generic {
    void Tcl_SetErrno(int err)
}
declare 228 generic {
    void Tcl_SetErrorCode(Tcl_Interp *interp, ...)
}
declare 229 generic {
    void Tcl_SetMaxBlockTime(Tcl_Time *timePtr)
}
declare 230 generic {
    void Tcl_SetPanicProc(Tcl_PanicProc *panicProc)
}
declare 231 generic {
    int Tcl_SetRecursionLimit(Tcl_Interp *interp, int depth)
}
declare 232 generic {
    void Tcl_SetResult(Tcl_Interp *interp, char *result,
	    Tcl_FreeProc *freeProc)
}
declare 233 generic {
    int Tcl_SetServiceMode(int mode)
}
declare 234 generic {
    void Tcl_SetObjErrorCode(Tcl_Interp *interp, Tcl_Obj *errorObjPtr)
}
declare 235 generic {
    void Tcl_SetObjResult(Tcl_Interp *interp, Tcl_Obj *resultObjPtr)
}
declare 236 generic {
    void Tcl_SetStdChannel(Tcl_Channel channel, int type)
}
declare 237 generic {
    CONST84_RETURN char *Tcl_SetVar(Tcl_Interp *interp, CONST char *varName,
	    CONST char *newValue, int flags)
}
declare 238 generic {
    CONST84_RETURN char *Tcl_SetVar2(Tcl_Interp *interp, CONST char *part1,
	    CONST char *part2, CONST char *newValue, int flags)
}
declare 239 generic {
    CONST84_RETURN char *Tcl_SignalId(int sig)
}
declare 240 generic {
    CONST84_RETURN char *Tcl_SignalMsg(int sig)
}
declare 241 generic {
    void Tcl_SourceRCFile(Tcl_Interp *interp)
}
declare 242 generic {
    int Tcl_SplitList(Tcl_Interp *interp, CONST char *listStr, int *argcPtr,
	    CONST84 char ***argvPtr)
}
# Obsolete, use Tcl_FSSplitPath
declare 243 generic {
    void Tcl_SplitPath(CONST char *path, int *argcPtr, CONST84 char ***argvPtr)
}
declare 244 generic {
    void Tcl_StaticPackage(Tcl_Interp *interp, CONST char *pkgName,
	    Tcl_PackageInitProc *initProc, Tcl_PackageInitProc *safeInitProc)
}
declare 245 generic {
    int Tcl_StringMatch(CONST char *str, CONST char *pattern)
}
# Obsolete
declare 246 generic {
    int Tcl_TellOld(Tcl_Channel chan)
}
declare 247 generic {
    int Tcl_TraceVar(Tcl_Interp *interp, CONST char *varName, int flags,
	    Tcl_VarTraceProc *proc, ClientData clientData)
}
declare 248 generic {
    int Tcl_TraceVar2(Tcl_Interp *interp, CONST char *part1, CONST char *part2,
	    int flags, Tcl_VarTraceProc *proc, ClientData clientData)
}
declare 249 generic {
    char *Tcl_TranslateFileName(Tcl_Interp *interp, CONST char *name,
	    Tcl_DString *bufferPtr)
}
declare 250 generic {
    int Tcl_Ungets(Tcl_Channel chan, CONST char *str, int len, int atHead)
}
declare 251 generic {
    void Tcl_UnlinkVar(Tcl_Interp *interp, CONST char *varName)
}
declare 252 generic {
    int Tcl_UnregisterChannel(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 253 generic {
    int Tcl_UnsetVar(Tcl_Interp *interp, CONST char *varName, int flags)
}
declare 254 generic {
    int Tcl_UnsetVar2(Tcl_Interp *interp, CONST char *part1, CONST char *part2,
	    int flags)
}
declare 255 generic {
    void Tcl_UntraceVar(Tcl_Interp *interp, CONST char *varName, int flags,
	    Tcl_VarTraceProc *proc, ClientData clientData)
}
declare 256 generic {
    void Tcl_UntraceVar2(Tcl_Interp *interp, CONST char *part1,
	    CONST char *part2, int flags, Tcl_VarTraceProc *proc,
	    ClientData clientData)
}
declare 257 generic {
    void Tcl_UpdateLinkedVar(Tcl_Interp *interp, CONST char *varName)
}
declare 258 generic {
    int Tcl_UpVar(Tcl_Interp *interp, CONST char *frameName,
	    CONST char *varName, CONST char *localName, int flags)
}
declare 259 generic {
    int Tcl_UpVar2(Tcl_Interp *interp, CONST char *frameName, CONST char *part1,
	    CONST char *part2, CONST char *localName, int flags)
}
declare 260 generic {
    int Tcl_VarEval(Tcl_Interp *interp, ...)
}
declare 261 generic {
    ClientData Tcl_VarTraceInfo(Tcl_Interp *interp, CONST char *varName,
	    int flags, Tcl_VarTraceProc *procPtr, ClientData prevClientData)
}
declare 262 generic {
    ClientData Tcl_VarTraceInfo2(Tcl_Interp *interp, CONST char *part1,
	    CONST char *part2, int flags, Tcl_VarTraceProc *procPtr,
	    ClientData prevClientData)
}
declare 263 generic {
    int Tcl_Write(Tcl_Channel chan, CONST char *s, int slen)
}
declare 264 generic {
    void Tcl_WrongNumArgs(Tcl_Interp *interp, int objc,
	    Tcl_Obj *CONST objv[], CONST char *message)
}
declare 265 generic {
    int Tcl_DumpActiveMemory(CONST char *fileName)
}
declare 266 generic {
    void Tcl_ValidateAllMemory(CONST char *file, int line)
}
declare 267 generic {
    void Tcl_AppendResultVA(Tcl_Interp *interp, va_list argList)
}
declare 268 generic {
    void Tcl_AppendStringsToObjVA(Tcl_Obj *objPtr, va_list argList)
}
declare 269 generic {
    char *Tcl_HashStats(Tcl_HashTable *tablePtr)
}
declare 270 generic {
    CONST84_RETURN char *Tcl_ParseVar(Tcl_Interp *interp, CONST char *start,
	    CONST84 char **termPtr)
}
declare 271 generic {
    CONST84_RETURN char *Tcl_PkgPresent(Tcl_Interp *interp, CONST char *name,
	    CONST char *version, int exact)
}
declare 272 generic {
    CONST84_RETURN char *Tcl_PkgPresentEx(Tcl_Interp *interp,
	    CONST char *name, CONST char *version, int exact,
	    ClientData *clientDataPtr)
}
declare 273 generic {
    int Tcl_PkgProvide(Tcl_Interp *interp, CONST char *name,
	    CONST char *version)
}
# TIP #268: The internally used new Require function is in slot 573.
declare 274 generic {
    CONST84_RETURN char *Tcl_PkgRequire(Tcl_Interp *interp, CONST char *name,
	    CONST char *version, int exact)
}
declare 275 generic {
    void Tcl_SetErrorCodeVA(Tcl_Interp *interp, va_list argList)
}
declare 276 generic {
    int  Tcl_VarEvalVA(Tcl_Interp *interp, va_list argList)
}
declare 277 generic {
    Tcl_Pid Tcl_WaitPid(Tcl_Pid pid, int *statPtr, int options)
}
declare 278 generic {
    void Tcl_PanicVA(CONST char *format, va_list argList)
}
declare 279 generic {
    void Tcl_GetVersion(int *major, int *minor, int *patchLevel, int *type)
}
declare 280 generic {
    void Tcl_InitMemory(Tcl_Interp *interp)
}

# Andreas Kupries <a.kupries@westend.com>, 03/21/1999
# "Trf-Patch for filtering channels"
#
# C-Level API for (un)stacking of channels. This allows the introduction
# of filtering channels with relatively little changes to the core.
# This patch was created in cooperation with Jan Nijtmans j.nijtmans@chello.nl
# and is therefore part of his plus-patches too.
#
# It would have been possible to place the following definitions according
# to the alphabetical order used elsewhere in this file, but I decided
# against that to ease the maintenance of the patch across new tcl versions
# (patch usually has no problems to integrate the patch file for the last
# version into the new one).

declare 281 generic {
    Tcl_Channel Tcl_StackChannel(Tcl_Interp *interp,
	    Tcl_ChannelType *typePtr, ClientData instanceData,
	    int mask, Tcl_Channel prevChan)
}
declare 282 generic {
    int Tcl_UnstackChannel(Tcl_Interp *interp, Tcl_Channel chan)
}
declare 283 generic {
    Tcl_Channel Tcl_GetStackedChannel(Tcl_Channel chan)
}

# 284 was reserved, but added in 8.4a2
declare 284 generic {
    void Tcl_SetMainLoop(Tcl_MainLoopProc *proc)
}

# Reserved for future use (8.0.x vs. 8.1)
#  declare 285 generic {
#  }

# Added in 8.1:

declare 286 generic {
    void Tcl_AppendObjToObj(Tcl_Obj *objPtr, Tcl_Obj *appendObjPtr)
}
declare 287 generic {
    Tcl_Encoding Tcl_CreateEncoding(CONST Tcl_EncodingType *typePtr)
}
declare 288 generic {
    void Tcl_CreateThreadExitHandler(Tcl_ExitProc *proc, ClientData clientData)
}
declare 289 generic {
    void Tcl_DeleteThreadExitHandler(Tcl_ExitProc *proc, ClientData clientData)
}
declare 290 generic {
    void Tcl_DiscardResult(Tcl_SavedResult *statePtr)
}
declare 291 generic {
    int Tcl_EvalEx(Tcl_Interp *interp, CONST char *script, int numBytes,
	    int flags)
}
declare 292 generic {
    int Tcl_EvalObjv(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
	    int flags)
}
declare 293 generic {
    int Tcl_EvalObjEx(Tcl_Interp *interp, Tcl_Obj *objPtr, int flags)
}
declare 294 generic {
    void Tcl_ExitThread(int status)
}
declare 295 generic {
    int Tcl_ExternalToUtf(Tcl_Interp *interp, Tcl_Encoding encoding,
	    CONST char *src, int srcLen, int flags,
	    Tcl_EncodingState *statePtr, char *dst, int dstLen,
	    int *srcReadPtr, int *dstWrotePtr, int *dstCharsPtr)
}
declare 296 generic {
    char *Tcl_ExternalToUtfDString(Tcl_Encoding encoding,
	    CONST char *src, int srcLen, Tcl_DString *dsPtr)
}
declare 297 generic {
    void Tcl_FinalizeThread(void)
}
declare 298 generic {
    void Tcl_FinalizeNotifier(ClientData clientData)
}
declare 299 generic {
    void Tcl_FreeEncoding(Tcl_Encoding encoding)
}
declare 300 generic {
    Tcl_ThreadId Tcl_GetCurrentThread(void)
}
declare 301 generic {
    Tcl_Encoding Tcl_GetEncoding(Tcl_Interp *interp, CONST char *name)
}
declare 302 generic {
    CONST84_RETURN char *Tcl_GetEncodingName(Tcl_Encoding encoding)
}
declare 303 generic {
    void Tcl_GetEncodingNames(Tcl_Interp *interp)
}
declare 304 generic {
    int Tcl_GetIndexFromObjStruct(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    CONST VOID *tablePtr, int offset, CONST char *msg, int flags,
	    int *indexPtr)
}
declare 305 generic {
    VOID *Tcl_GetThreadData(Tcl_ThreadDataKey *keyPtr, int size)
}
declare 306 generic {
    Tcl_Obj *Tcl_GetVar2Ex(Tcl_Interp *interp, CONST char *part1,
	    CONST char *part2, int flags)
}
declare 307 generic {
    ClientData Tcl_InitNotifier(void)
}
declare 308 generic {
    void Tcl_MutexLock(Tcl_Mutex *mutexPtr)
}
declare 309 generic {
    void Tcl_MutexUnlock(Tcl_Mutex *mutexPtr)
}
declare 310 generic {
    void Tcl_ConditionNotify(Tcl_Condition *condPtr)
}
declare 311 generic {
    void Tcl_ConditionWait(Tcl_Condition *condPtr, Tcl_Mutex *mutexPtr,
	    Tcl_Time *timePtr)
}
declare 312 generic {
    int Tcl_NumUtfChars(CONST char *src, int length)
}
declare 313 generic {
    int Tcl_ReadChars(Tcl_Channel channel, Tcl_Obj *objPtr, int charsToRead,
	    int appendFlag)
}
declare 314 generic {
    void Tcl_RestoreResult(Tcl_Interp *interp, Tcl_SavedResult *statePtr)
}
declare 315 generic {
    void Tcl_SaveResult(Tcl_Interp *interp, Tcl_SavedResult *statePtr)
}
declare 316 generic {
    int Tcl_SetSystemEncoding(Tcl_Interp *interp, CONST char *name)
}
declare 317 generic {
    Tcl_Obj *Tcl_SetVar2Ex(Tcl_Interp *interp, CONST char *part1,
	    CONST char *part2, Tcl_Obj *newValuePtr, int flags)
}
declare 318 generic {
    void Tcl_ThreadAlert(Tcl_ThreadId threadId)
}
declare 319 generic {
    void Tcl_ThreadQueueEvent(Tcl_ThreadId threadId, Tcl_Event *evPtr,
	    Tcl_QueuePosition position)
}
declare 320 generic {
    Tcl_UniChar Tcl_UniCharAtIndex(CONST char *src, int index)
}
declare 321 generic {
    Tcl_UniChar Tcl_UniCharToLower(int ch)
}
declare 322 generic {
    Tcl_UniChar Tcl_UniCharToTitle(int ch)
}
declare 323 generic {
    Tcl_UniChar Tcl_UniCharToUpper(int ch)
}
declare 324 generic {
    int Tcl_UniCharToUtf(int ch, char *buf)
}
declare 325 generic {
    CONST84_RETURN char *Tcl_UtfAtIndex(CONST char *src, int index)
}
declare 326 generic {
    int Tcl_UtfCharComplete(CONST char *src, int length)
}
declare 327 generic {
    int Tcl_UtfBackslash(CONST char *src, int *readPtr, char *dst)
}
declare 328 generic {
    CONST84_RETURN char *Tcl_UtfFindFirst(CONST char *src, int ch)
}
declare 329 generic {
    CONST84_RETURN char *Tcl_UtfFindLast(CONST char *src, int ch)
}
declare 330 generic {
    CONST84_RETURN char *Tcl_UtfNext(CONST char *src)
}
declare 331 generic {
    CONST84_RETURN char *Tcl_UtfPrev(CONST char *src, CONST char *start)
}
declare 332 generic {
    int Tcl_UtfToExternal(Tcl_Interp *interp, Tcl_Encoding encoding,
	    CONST char *src, int srcLen, int flags,
	    Tcl_EncodingState *statePtr, char *dst, int dstLen,
	    int *srcReadPtr, int *dstWrotePtr, int *dstCharsPtr)
}
declare 333 generic {
    char *Tcl_UtfToExternalDString(Tcl_Encoding encoding,
	    CONST char *src, int srcLen, Tcl_DString *dsPtr)
}
declare 334 generic {
    int Tcl_UtfToLower(char *src)
}
declare 335 generic {
    int Tcl_UtfToTitle(char *src)
}
declare 336 generic {
    int Tcl_UtfToUniChar(CONST char *src, Tcl_UniChar *chPtr)
}
declare 337 generic {
    int Tcl_UtfToUpper(char *src)
}
declare 338 generic {
    int Tcl_WriteChars(Tcl_Channel chan, CONST char *src, int srcLen)
}
declare 339 generic {
    int Tcl_WriteObj(Tcl_Channel chan, Tcl_Obj *objPtr)
}
declare 340 generic {
    char *Tcl_GetString(Tcl_Obj *objPtr)
}
declare 341 generic {
    CONST84_RETURN char *Tcl_GetDefaultEncodingDir(void)
}
declare 342 generic {
    void Tcl_SetDefaultEncodingDir(CONST char *path)
}
declare 343 generic {
    void Tcl_AlertNotifier(ClientData clientData)
}
declare 344 generic {
    void Tcl_ServiceModeHook(int mode)
}
declare 345 generic {
    int Tcl_UniCharIsAlnum(int ch)
}
declare 346 generic {
    int Tcl_UniCharIsAlpha(int ch)
}
declare 347 generic {
    int Tcl_UniCharIsDigit(int ch)
}
declare 348 generic {
    int Tcl_UniCharIsLower(int ch)
}
declare 349 generic {
    int Tcl_UniCharIsSpace(int ch)
}
declare 350 generic {
    int Tcl_UniCharIsUpper(int ch)
}
declare 351 generic {
    int Tcl_UniCharIsWordChar(int ch)
}
declare 352 generic {
    int Tcl_UniCharLen(CONST Tcl_UniChar *uniStr)
}
declare 353 generic {
    int Tcl_UniCharNcmp(CONST Tcl_UniChar *ucs, CONST Tcl_UniChar *uct,
	    unsigned long numChars)
}
declare 354 generic {
    char *Tcl_UniCharToUtfDString(CONST Tcl_UniChar *uniStr,
	    int uniLength, Tcl_DString *dsPtr)
}
declare 355 generic {
    Tcl_UniChar *Tcl_UtfToUniCharDString(CONST char *src,
	    int length, Tcl_DString *dsPtr)
}
declare 356 generic {
    Tcl_RegExp Tcl_GetRegExpFromObj(Tcl_Interp *interp, Tcl_Obj *patObj,
	    int flags)
}
declare 357 generic {
    Tcl_Obj *Tcl_EvalTokens(Tcl_Interp *interp, Tcl_Token *tokenPtr,
	    int count)
}
declare 358 generic {
    void Tcl_FreeParse(Tcl_Parse *parsePtr)
}
declare 359 generic {
    void Tcl_LogCommandInfo(Tcl_Interp *interp, CONST char *script,
	    CONST char *command, int length)
}
declare 360 generic {
    int Tcl_ParseBraces(Tcl_Interp *interp, CONST char *start, int numBytes,
	    Tcl_Parse *parsePtr, int append, CONST84 char **termPtr)
}
declare 361 generic {
    int Tcl_ParseCommand(Tcl_Interp *interp, CONST char *start, int numBytes,
	    int nested, Tcl_Parse *parsePtr)
}
declare 362 generic {
    int Tcl_ParseExpr(Tcl_Interp *interp, CONST char *start, int numBytes,
	    Tcl_Parse *parsePtr)
}
declare 363 generic {
    int Tcl_ParseQuotedString(Tcl_Interp *interp, CONST char *start,
	    int numBytes, Tcl_Parse *parsePtr, int append,
	    CONST84 char **termPtr)
}
declare 364 generic {
    int Tcl_ParseVarName(Tcl_Interp *interp, CONST char *start, int numBytes,
	    Tcl_Parse *parsePtr, int append)
}
# These 4 functions are obsolete, use Tcl_FSGetCwd, Tcl_FSChdir,
# Tcl_FSAccess and Tcl_FSStat
declare 365 generic {
    char *Tcl_GetCwd(Tcl_Interp *interp, Tcl_DString *cwdPtr)
}
declare 366 generic {
   int Tcl_Chdir(CONST char *dirName)
}
declare 367 generic {
   int Tcl_Access(CONST char *path, int mode)
}
declare 368 generic {
    int Tcl_Stat(CONST char *path, struct stat *bufPtr)
}
declare 369 generic {
    int Tcl_UtfNcmp(CONST char *s1, CONST char *s2, unsigned long n)
}
declare 370 generic {
    int Tcl_UtfNcasecmp(CONST char *s1, CONST char *s2, unsigned long n)
}
declare 371 generic {
    int Tcl_StringCaseMatch(CONST char *str, CONST char *pattern, int nocase)
}
declare 372 generic {
    int Tcl_UniCharIsControl(int ch)
}
declare 373 generic {
    int Tcl_UniCharIsGraph(int ch)
}
declare 374 generic {
    int Tcl_UniCharIsPrint(int ch)
}
declare 375 generic {
    int Tcl_UniCharIsPunct(int ch)
}
declare 376 generic {
    int Tcl_RegExpExecObj(Tcl_Interp *interp, Tcl_RegExp regexp,
	    Tcl_Obj *textObj, int offset, int nmatches, int flags)
}
declare 377 generic {
    void Tcl_RegExpGetInfo(Tcl_RegExp regexp, Tcl_RegExpInfo *infoPtr)
}
declare 378 generic {
    Tcl_Obj *Tcl_NewUnicodeObj(CONST Tcl_UniChar *unicode, int numChars)
}
declare 379 generic {
    void Tcl_SetUnicodeObj(Tcl_Obj *objPtr, CONST Tcl_UniChar *unicode,
	    int numChars)
}
declare 380 generic {
    int Tcl_GetCharLength(Tcl_Obj *objPtr)
}
declare 381 generic {
    Tcl_UniChar Tcl_GetUniChar(Tcl_Obj *objPtr, int index)
}
declare 382 generic {
    Tcl_UniChar *Tcl_GetUnicode(Tcl_Obj *objPtr)
}
declare 383 generic {
    Tcl_Obj *Tcl_GetRange(Tcl_Obj *objPtr, int first, int last)
}
declare 384 generic {
    void Tcl_AppendUnicodeToObj(Tcl_Obj *objPtr, CONST Tcl_UniChar *unicode,
	    int length)
}
declare 385 generic {
    int Tcl_RegExpMatchObj(Tcl_Interp *interp, Tcl_Obj *textObj,
	    Tcl_Obj *patternObj)
}
declare 386 generic {
    void Tcl_SetNotifier(Tcl_NotifierProcs *notifierProcPtr)
}
declare 387 generic {
    Tcl_Mutex *Tcl_GetAllocMutex(void)
}
declare 388 generic {
    int Tcl_GetChannelNames(Tcl_Interp *interp)
}
declare 389 generic {
    int Tcl_GetChannelNamesEx(Tcl_Interp *interp, CONST char *pattern)
}
declare 390 generic {
    int Tcl_ProcObjCmd(ClientData clientData, Tcl_Interp *interp,
	    int objc, Tcl_Obj *CONST objv[])
}
declare 391 generic {
    void Tcl_ConditionFinalize(Tcl_Condition *condPtr)
}
declare 392 generic {
    void Tcl_MutexFinalize(Tcl_Mutex *mutex)
}
declare 393 generic {
    int Tcl_CreateThread(Tcl_ThreadId *idPtr, Tcl_ThreadCreateProc proc,
	    ClientData clientData, int stackSize, int flags)
}

# Introduced in 8.3.2
declare 394 generic {
    int Tcl_ReadRaw(Tcl_Channel chan, char *dst, int bytesToRead)
}
declare 395 generic {
    int Tcl_WriteRaw(Tcl_Channel chan, CONST char *src, int srcLen)
}
declare 396 generic {
    Tcl_Channel Tcl_GetTopChannel(Tcl_Channel chan)
}
declare 397 generic {
    int Tcl_ChannelBuffered(Tcl_Channel chan)
}
declare 398 generic {
    CONST84_RETURN char *Tcl_ChannelName(CONST Tcl_ChannelType *chanTypePtr)
}
declare 399 generic {
    Tcl_ChannelTypeVersion Tcl_ChannelVersion(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 400 generic {
    Tcl_DriverBlockModeProc *Tcl_ChannelBlockModeProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 401 generic {
    Tcl_DriverCloseProc *Tcl_ChannelCloseProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 402 generic {
    Tcl_DriverClose2Proc *Tcl_ChannelClose2Proc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 403 generic {
    Tcl_DriverInputProc *Tcl_ChannelInputProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 404 generic {
    Tcl_DriverOutputProc *Tcl_ChannelOutputProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 405 generic {
    Tcl_DriverSeekProc *Tcl_ChannelSeekProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 406 generic {
    Tcl_DriverSetOptionProc *Tcl_ChannelSetOptionProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 407 generic {
    Tcl_DriverGetOptionProc *Tcl_ChannelGetOptionProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 408 generic {
    Tcl_DriverWatchProc *Tcl_ChannelWatchProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 409 generic {
    Tcl_DriverGetHandleProc *Tcl_ChannelGetHandleProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 410 generic {
    Tcl_DriverFlushProc *Tcl_ChannelFlushProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}
declare 411 generic {
    Tcl_DriverHandlerProc *Tcl_ChannelHandlerProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}

# Introduced in 8.4a2
declare 412 generic {
    int Tcl_JoinThread(Tcl_ThreadId threadId, int *result)
}
declare 413 generic {
    int Tcl_IsChannelShared(Tcl_Channel channel)
}
declare 414 generic {
    int Tcl_IsChannelRegistered(Tcl_Interp *interp, Tcl_Channel channel)
}
declare 415 generic {
    void Tcl_CutChannel(Tcl_Channel channel)
}
declare 416 generic {
    void Tcl_SpliceChannel(Tcl_Channel channel)
}
declare 417 generic {
    void Tcl_ClearChannelHandlers(Tcl_Channel channel)
}
declare 418 generic {
    int Tcl_IsChannelExisting(CONST char *channelName)
}
declare 419 generic {
    int Tcl_UniCharNcasecmp(CONST Tcl_UniChar *ucs, CONST Tcl_UniChar *uct,
	    unsigned long numChars)
}
declare 420 generic {
    int Tcl_UniCharCaseMatch(CONST Tcl_UniChar *uniStr,
	    CONST Tcl_UniChar *uniPattern, int nocase)
}
declare 421 generic {
    Tcl_HashEntry *Tcl_FindHashEntry(Tcl_HashTable *tablePtr, CONST char *key)
}
declare 422 generic {
    Tcl_HashEntry *Tcl_CreateHashEntry(Tcl_HashTable *tablePtr,
	    CONST char *key, int *newPtr)
}
declare 423 generic {
    void Tcl_InitCustomHashTable(Tcl_HashTable *tablePtr, int keyType,
	    Tcl_HashKeyType *typePtr)
}
declare 424 generic {
    void Tcl_InitObjHashTable(Tcl_HashTable *tablePtr)
}
declare 425 generic {
    ClientData Tcl_CommandTraceInfo(Tcl_Interp *interp, CONST char *varName,
	    int flags, Tcl_CommandTraceProc *procPtr,
	    ClientData prevClientData)
}
declare 426 generic {
    int Tcl_TraceCommand(Tcl_Interp *interp, CONST char *varName, int flags,
	    Tcl_CommandTraceProc *proc, ClientData clientData)
}
declare 427 generic {
    void Tcl_UntraceCommand(Tcl_Interp *interp, CONST char *varName,
	    int flags, Tcl_CommandTraceProc *proc, ClientData clientData)
}
declare 428 generic {
    char *Tcl_AttemptAlloc(unsigned int size)
}
declare 429 generic {
    char *Tcl_AttemptDbCkalloc(unsigned int size, CONST char *file, int line)
}
declare 430 generic {
    char *Tcl_AttemptRealloc(char *ptr, unsigned int size)
}
declare 431 generic {
    char *Tcl_AttemptDbCkrealloc(char *ptr, unsigned int size,
	    CONST char *file, int line)
}
declare 432 generic {
    int Tcl_AttemptSetObjLength(Tcl_Obj *objPtr, int length)
}

# TIP#10 (thread-aware channels) akupries
declare 433 generic {
    Tcl_ThreadId Tcl_GetChannelThread(Tcl_Channel channel)
}

# introduced in 8.4a3
declare 434 generic {
    Tcl_UniChar *Tcl_GetUnicodeFromObj(Tcl_Obj *objPtr, int *lengthPtr)
}

# TIP#15 (math function introspection) dkf
declare 435 generic {
    int Tcl_GetMathFuncInfo(Tcl_Interp *interp, CONST char *name,
	    int *numArgsPtr, Tcl_ValueType **argTypesPtr,
	    Tcl_MathProc **procPtr, ClientData *clientDataPtr)
}
declare 436 generic {
    Tcl_Obj *Tcl_ListMathFuncs(Tcl_Interp *interp, CONST char *pattern)
}

# TIP#36 (better access to 'subst') dkf
declare 437 generic {
    Tcl_Obj *Tcl_SubstObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int flags)
}

# TIP#17 (virtual filesystem layer) vdarley
declare 438 generic {
    int Tcl_DetachChannel(Tcl_Interp *interp, Tcl_Channel channel)
}
declare 439 generic {
    int Tcl_IsStandardChannel(Tcl_Channel channel)
}
declare 440 generic {
    int	Tcl_FSCopyFile(Tcl_Obj *srcPathPtr, Tcl_Obj *destPathPtr)
}
declare 441 generic {
    int	Tcl_FSCopyDirectory(Tcl_Obj *srcPathPtr,
	    Tcl_Obj *destPathPtr, Tcl_Obj **errorPtr)
}
declare 442 generic {
    int	Tcl_FSCreateDirectory(Tcl_Obj *pathPtr)
}
declare 443 generic {
    int	Tcl_FSDeleteFile(Tcl_Obj *pathPtr)
}
declare 444 generic {
    int	Tcl_FSLoadFile(Tcl_Interp *interp, Tcl_Obj *pathPtr, CONST char *sym1,
	    CONST char *sym2, Tcl_PackageInitProc **proc1Ptr,
	    Tcl_PackageInitProc **proc2Ptr, Tcl_LoadHandle *handlePtr,
	    Tcl_FSUnloadFileProc **unloadProcPtr)
}
declare 445 generic {
    int	Tcl_FSMatchInDirectory(Tcl_Interp *interp, Tcl_Obj *result,
	    Tcl_Obj *pathPtr, CONST char *pattern, Tcl_GlobTypeData *types)
}
declare 446 generic {
    Tcl_Obj *Tcl_FSLink(Tcl_Obj *pathPtr, Tcl_Obj *toPtr, int linkAction)
}
declare 447 generic {
    int Tcl_FSRemoveDirectory(Tcl_Obj *pathPtr,
	    int recursive, Tcl_Obj **errorPtr)
}
declare 448 generic {
    int	Tcl_FSRenameFile(Tcl_Obj *srcPathPtr, Tcl_Obj *destPathPtr)
}
declare 449 generic {
    int	Tcl_FSLstat(Tcl_Obj *pathPtr, Tcl_StatBuf *buf)
}
declare 450 generic {
    int Tcl_FSUtime(Tcl_Obj *pathPtr, struct utimbuf *tval)
}
declare 451 generic {
    int Tcl_FSFileAttrsGet(Tcl_Interp *interp,
	    int index, Tcl_Obj *pathPtr, Tcl_Obj **objPtrRef)
}
declare 452 generic {
    int Tcl_FSFileAttrsSet(Tcl_Interp *interp,
	    int index, Tcl_Obj *pathPtr, Tcl_Obj *objPtr)
}
declare 453 generic {
    CONST char **Tcl_FSFileAttrStrings(Tcl_Obj *pathPtr,
	    Tcl_Obj **objPtrRef)
}
declare 454 generic {
    int Tcl_FSStat(Tcl_Obj *pathPtr, Tcl_StatBuf *buf)
}
declare 455 generic {
    int Tcl_FSAccess(Tcl_Obj *pathPtr, int mode)
}
declare 456 generic {
    Tcl_Channel Tcl_FSOpenFileChannel(Tcl_Interp *interp, Tcl_Obj *pathPtr,
	    CONST char *modeString, int permissions)
}
declare 457 generic {
    Tcl_Obj *Tcl_FSGetCwd(Tcl_Interp *interp)
}
declare 458 generic {
    int Tcl_FSChdir(Tcl_Obj *pathPtr)
}
declare 459 generic {
    int Tcl_FSConvertToPathType(Tcl_Interp *interp, Tcl_Obj *pathPtr)
}
declare 460 generic {
    Tcl_Obj *Tcl_FSJoinPath(Tcl_Obj *listObj, int elements)
}
declare 461 generic {
    Tcl_Obj *Tcl_FSSplitPath(Tcl_Obj *pathPtr, int *lenPtr)
}
declare 462 generic {
    int Tcl_FSEqualPaths(Tcl_Obj *firstPtr, Tcl_Obj *secondPtr)
}
declare 463 generic {
    Tcl_Obj *Tcl_FSGetNormalizedPath(Tcl_Interp *interp, Tcl_Obj *pathPtr)
}
declare 464 generic {
    Tcl_Obj *Tcl_FSJoinToPath(Tcl_Obj *pathPtr, int objc,
	    Tcl_Obj *CONST objv[])
}
declare 465 generic {
    ClientData Tcl_FSGetInternalRep(Tcl_Obj *pathPtr,
	    Tcl_Filesystem *fsPtr)
}
declare 466 generic {
    Tcl_Obj *Tcl_FSGetTranslatedPath(Tcl_Interp *interp, Tcl_Obj *pathPtr)
}
declare 467 generic {
    int Tcl_FSEvalFile(Tcl_Interp *interp, Tcl_Obj *fileName)
}
declare 468 generic {
    Tcl_Obj *Tcl_FSNewNativePath(Tcl_Filesystem *fromFilesystem,
	    ClientData clientData)
}
declare 469 generic {
    CONST char *Tcl_FSGetNativePath(Tcl_Obj *pathPtr)
}
declare 470 generic {
    Tcl_Obj *Tcl_FSFileSystemInfo(Tcl_Obj *pathPtr)
}
declare 471 generic {
    Tcl_Obj *Tcl_FSPathSeparator(Tcl_Obj *pathPtr)
}
declare 472 generic {
    Tcl_Obj *Tcl_FSListVolumes(void)
}
declare 473 generic {
    int Tcl_FSRegister(ClientData clientData, Tcl_Filesystem *fsPtr)
}
declare 474 generic {
    int Tcl_FSUnregister(Tcl_Filesystem *fsPtr)
}
declare 475 generic {
    ClientData Tcl_FSData(Tcl_Filesystem *fsPtr)
}
declare 476 generic {
    CONST char *Tcl_FSGetTranslatedStringPath(Tcl_Interp *interp,
	    Tcl_Obj *pathPtr)
}
declare 477 generic {
    Tcl_Filesystem *Tcl_FSGetFileSystemForPath(Tcl_Obj *pathPtr)
}
declare 478 generic {
    Tcl_PathType Tcl_FSGetPathType(Tcl_Obj *pathPtr)
}

# TIP#49 (detection of output buffering) akupries
declare 479 generic {
    int Tcl_OutputBuffered(Tcl_Channel chan)
}
declare 480 generic {
    void Tcl_FSMountsChanged(Tcl_Filesystem *fsPtr)
}

# TIP#56 (evaluate a parsed script) msofer
declare 481 generic {
    int Tcl_EvalTokensStandard(Tcl_Interp *interp, Tcl_Token *tokenPtr,
	    int count)
}

# TIP#73 (access to current time) kbk
declare 482 generic {
    void Tcl_GetTime(Tcl_Time *timeBuf)
}

# TIP#32 (object-enabled traces) kbk
declare 483 generic {
    Tcl_Trace Tcl_CreateObjTrace(Tcl_Interp *interp, int level, int flags,
	    Tcl_CmdObjTraceProc *objProc, ClientData clientData,
	    Tcl_CmdObjTraceDeleteProc *delProc)
}
declare 484 generic {
    int Tcl_GetCommandInfoFromToken(Tcl_Command token, Tcl_CmdInfo *infoPtr)
}
declare 485 generic {
    int Tcl_SetCommandInfoFromToken(Tcl_Command token,
	    CONST Tcl_CmdInfo *infoPtr)
}

### New functions on 64-bit dev branch ###
# TIP#72 (64-bit values) dkf
declare 486 generic {
    Tcl_Obj *Tcl_DbNewWideIntObj(Tcl_WideInt wideValue,
	    CONST char *file, int line)
}
declare 487 generic {
    int Tcl_GetWideIntFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    Tcl_WideInt *widePtr)
}
declare 488 generic {
    Tcl_Obj *Tcl_NewWideIntObj(Tcl_WideInt wideValue)
}
declare 489 generic {
    void Tcl_SetWideIntObj(Tcl_Obj *objPtr, Tcl_WideInt wideValue)
}
declare 490 generic {
    Tcl_StatBuf *Tcl_AllocStatBuf(void)
}
declare 491 generic {
    Tcl_WideInt Tcl_Seek(Tcl_Channel chan, Tcl_WideInt offset, int mode)
}
declare 492 generic {
    Tcl_WideInt Tcl_Tell(Tcl_Channel chan)
}

# TIP#91 (back-compat enhancements for channels) dkf
declare 493 generic {
    Tcl_DriverWideSeekProc *Tcl_ChannelWideSeekProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}

# ----- BASELINE -- FOR -- 8.4.0 ----- #

# TIP#111 (dictionaries) dkf
declare 494 generic {
    int Tcl_DictObjPut(Tcl_Interp *interp, Tcl_Obj *dictPtr,
	    Tcl_Obj *keyPtr, Tcl_Obj *valuePtr)
}
declare 495 generic {
    int Tcl_DictObjGet(Tcl_Interp *interp, Tcl_Obj *dictPtr, Tcl_Obj *keyPtr,
	    Tcl_Obj **valuePtrPtr)
}
declare 496 generic {
    int Tcl_DictObjRemove(Tcl_Interp *interp, Tcl_Obj *dictPtr,
	    Tcl_Obj *keyPtr)
}
declare 497 generic {
    int Tcl_DictObjSize(Tcl_Interp *interp, Tcl_Obj *dictPtr, int *sizePtr)
}
declare 498 generic {
    int Tcl_DictObjFirst(Tcl_Interp *interp, Tcl_Obj *dictPtr,
	    Tcl_DictSearch *searchPtr,
	    Tcl_Obj **keyPtrPtr, Tcl_Obj **valuePtrPtr, int *donePtr)
}
declare 499 generic {
    void Tcl_DictObjNext(Tcl_DictSearch *searchPtr,
	    Tcl_Obj **keyPtrPtr, Tcl_Obj **valuePtrPtr, int *donePtr)
}
declare 500 generic {
    void Tcl_DictObjDone(Tcl_DictSearch *searchPtr)
}
declare 501 generic {
    int Tcl_DictObjPutKeyList(Tcl_Interp *interp, Tcl_Obj *dictPtr,
	    int keyc, Tcl_Obj *CONST *keyv, Tcl_Obj *valuePtr)
}
declare 502 generic {
    int Tcl_DictObjRemoveKeyList(Tcl_Interp *interp, Tcl_Obj *dictPtr,
	    int keyc, Tcl_Obj *CONST *keyv)
}
declare 503 generic {
    Tcl_Obj *Tcl_NewDictObj(void)
}
declare 504 generic {
    Tcl_Obj *Tcl_DbNewDictObj(CONST char *file, int line)
}

# TIP#59 (configuration reporting) akupries
declare 505 generic {
    void Tcl_RegisterConfig(Tcl_Interp *interp, CONST char *pkgName,
	    Tcl_Config *configuration, CONST char *valEncoding)
}

# TIP #139 (partial exposure of namespace API - transferred from tclInt.decls)
# dkf, API by Brent Welch?
declare 506 generic {
    Tcl_Namespace *Tcl_CreateNamespace(Tcl_Interp *interp, CONST char *name,
	    ClientData clientData, Tcl_NamespaceDeleteProc *deleteProc)
}
declare 507 generic {
    void Tcl_DeleteNamespace(Tcl_Namespace *nsPtr)
}
declare 508 generic {
    int Tcl_AppendExportList(Tcl_Interp *interp, Tcl_Namespace *nsPtr,
	    Tcl_Obj *objPtr)
}
declare 509 generic {
    int Tcl_Export(Tcl_Interp *interp, Tcl_Namespace *nsPtr,
	    CONST char *pattern, int resetListFirst)
}
declare 510 generic {
    int Tcl_Import(Tcl_Interp *interp, Tcl_Namespace *nsPtr,
	    CONST char *pattern, int allowOverwrite)
}
declare 511 generic {
    int Tcl_ForgetImport(Tcl_Interp *interp, Tcl_Namespace *nsPtr,
	    CONST char *pattern)
}
declare 512 generic {
    Tcl_Namespace *Tcl_GetCurrentNamespace(Tcl_Interp *interp)
}
declare 513 generic {
    Tcl_Namespace *Tcl_GetGlobalNamespace(Tcl_Interp *interp)
}
declare 514 generic {
    Tcl_Namespace *Tcl_FindNamespace(Tcl_Interp *interp, CONST char *name,
	    Tcl_Namespace *contextNsPtr, int flags)
}
declare 515 generic {
    Tcl_Command Tcl_FindCommand(Tcl_Interp *interp, CONST char *name,
	    Tcl_Namespace *contextNsPtr, int flags)
}
declare 516 generic {
    Tcl_Command Tcl_GetCommandFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 517 generic {
    void Tcl_GetCommandFullName(Tcl_Interp *interp, Tcl_Command command,
	    Tcl_Obj *objPtr)
}

# TIP#137 (encoding-aware source command) dgp for Anton Kovalenko
declare 518 generic {
    int Tcl_FSEvalFileEx(Tcl_Interp *interp, Tcl_Obj *fileName,
	    CONST char *encodingName)
}

# TIP#121 (exit handler) dkf for Joe Mistachkin
declare 519 generic {
    Tcl_ExitProc *Tcl_SetExitProc(Tcl_ExitProc *proc)
}

# TIP#143 (resource limits) dkf
declare 520 generic {
    void Tcl_LimitAddHandler(Tcl_Interp *interp, int type,
	    Tcl_LimitHandlerProc *handlerProc, ClientData clientData,
	    Tcl_LimitHandlerDeleteProc *deleteProc)
}
declare 521 generic {
    void Tcl_LimitRemoveHandler(Tcl_Interp *interp, int type,
	    Tcl_LimitHandlerProc *handlerProc, ClientData clientData)
}
declare 522 generic {
    int Tcl_LimitReady(Tcl_Interp *interp)
}
declare 523 generic {
    int Tcl_LimitCheck(Tcl_Interp *interp)
}
declare 524 generic {
    int Tcl_LimitExceeded(Tcl_Interp *interp)
}
declare 525 generic {
    void Tcl_LimitSetCommands(Tcl_Interp *interp, int commandLimit)
}
declare 526 generic {
    void Tcl_LimitSetTime(Tcl_Interp *interp, Tcl_Time *timeLimitPtr)
}
declare 527 generic {
    void Tcl_LimitSetGranularity(Tcl_Interp *interp, int type, int granularity)
}
declare 528 generic {
    int Tcl_LimitTypeEnabled(Tcl_Interp *interp, int type)
}
declare 529 generic {
    int Tcl_LimitTypeExceeded(Tcl_Interp *interp, int type)
}
declare 530 generic {
    void Tcl_LimitTypeSet(Tcl_Interp *interp, int type)
}
declare 531 generic {
    void Tcl_LimitTypeReset(Tcl_Interp *interp, int type)
}
declare 532 generic {
    int Tcl_LimitGetCommands(Tcl_Interp *interp)
}
declare 533 generic {
    void Tcl_LimitGetTime(Tcl_Interp *interp, Tcl_Time *timeLimitPtr)
}
declare 534 generic {
    int Tcl_LimitGetGranularity(Tcl_Interp *interp, int type)
}

# TIP#226 (interpreter result state management) dgp
declare 535 generic {
    Tcl_InterpState Tcl_SaveInterpState(Tcl_Interp *interp, int status)
}
declare 536 generic {
    int Tcl_RestoreInterpState(Tcl_Interp *interp, Tcl_InterpState state)
}
declare 537 generic {
    void Tcl_DiscardInterpState(Tcl_InterpState state)
}

# TIP#227 (return options interface) dgp
declare 538 generic {
    int Tcl_SetReturnOptions(Tcl_Interp *interp, Tcl_Obj *options)
}
declare 539 generic {
    Tcl_Obj *Tcl_GetReturnOptions(Tcl_Interp *interp, int result)
}

# TIP#235 (ensembles) dkf
declare 540 generic {
    int Tcl_IsEnsemble(Tcl_Command token)
}
declare 541 generic {
    Tcl_Command Tcl_CreateEnsemble(Tcl_Interp *interp, CONST char *name,
	    Tcl_Namespace *namespacePtr, int flags)
}
declare 542 generic {
    Tcl_Command Tcl_FindEnsemble(Tcl_Interp *interp, Tcl_Obj *cmdNameObj,
	    int flags)
}
declare 543 generic {
    int Tcl_SetEnsembleSubcommandList(Tcl_Interp *interp, Tcl_Command token,
	    Tcl_Obj *subcmdList)
}
declare 544 generic {
    int Tcl_SetEnsembleMappingDict(Tcl_Interp *interp, Tcl_Command token,
	    Tcl_Obj *mapDict)
}
declare 545 generic {
    int Tcl_SetEnsembleUnknownHandler(Tcl_Interp *interp, Tcl_Command token,
	    Tcl_Obj *unknownList)
}
declare 546 generic {
    int Tcl_SetEnsembleFlags(Tcl_Interp *interp, Tcl_Command token, int flags)
}
declare 547 generic {
    int Tcl_GetEnsembleSubcommandList(Tcl_Interp *interp, Tcl_Command token,
	    Tcl_Obj **subcmdListPtr)
}
declare 548 generic {
    int Tcl_GetEnsembleMappingDict(Tcl_Interp *interp, Tcl_Command token,
	    Tcl_Obj **mapDictPtr)
}
declare 549 generic {
    int Tcl_GetEnsembleUnknownHandler(Tcl_Interp *interp, Tcl_Command token,
	    Tcl_Obj **unknownListPtr)
}
declare 550 generic {
    int Tcl_GetEnsembleFlags(Tcl_Interp *interp, Tcl_Command token,
	    int *flagsPtr)
}
declare 551 generic {
    int Tcl_GetEnsembleNamespace(Tcl_Interp *interp, Tcl_Command token,
	    Tcl_Namespace **namespacePtrPtr)
}

# TIP#233 (virtualized time) akupries
declare 552 generic {
    void Tcl_SetTimeProc(Tcl_GetTimeProc *getProc,
	    Tcl_ScaleTimeProc *scaleProc,
	    ClientData clientData)
}
declare 553 generic {
    void Tcl_QueryTimeProc(Tcl_GetTimeProc **getProc,
	    Tcl_ScaleTimeProc **scaleProc,
	    ClientData *clientData)
}

# TIP#218 (driver thread actions) davygrvy/akupries ChannelType ver 4
declare 554 generic {
    Tcl_DriverThreadActionProc *Tcl_ChannelThreadActionProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}

# TIP#237 (arbitrary-precision integers) kbk
declare 555 generic {
    Tcl_Obj *Tcl_NewBignumObj(mp_int *value)
}
declare 556 generic {
    Tcl_Obj *Tcl_DbNewBignumObj(mp_int *value, CONST char *file, int line)
}
declare 557 generic {
    void Tcl_SetBignumObj(Tcl_Obj *obj, mp_int *value)
}
declare 558 generic {
    int Tcl_GetBignumFromObj(Tcl_Interp *interp, Tcl_Obj *obj, mp_int *value)
}
declare 559 generic {
    int Tcl_TakeBignumFromObj(Tcl_Interp *interp, Tcl_Obj *obj, mp_int *value)
}

# TIP #208 ('chan' command) jeffh
declare 560 generic {
    int Tcl_TruncateChannel(Tcl_Channel chan, Tcl_WideInt length)
}
declare 561 generic {
    Tcl_DriverTruncateProc *Tcl_ChannelTruncateProc(
	    CONST Tcl_ChannelType *chanTypePtr)
}

# TIP#219 (channel reflection api) akupries
declare 562 generic {
    void Tcl_SetChannelErrorInterp(Tcl_Interp *interp, Tcl_Obj *msg)
}
declare 563 generic {
    void Tcl_GetChannelErrorInterp(Tcl_Interp *interp, Tcl_Obj **msg)
}
declare 564 generic {
    void Tcl_SetChannelError(Tcl_Channel chan, Tcl_Obj *msg)
}
declare 565 generic {
    void Tcl_GetChannelError(Tcl_Channel chan, Tcl_Obj **msg)
}

# TIP #237 (additional conversion functions for bignum support) kbk/dgp
declare 566 generic {
    int Tcl_InitBignumFromDouble(Tcl_Interp *interp, double initval,
	    mp_int *toInit)
}

# TIP#181 (namespace unknown command) dgp for Neil Madden
declare 567 generic {
    Tcl_Obj *Tcl_GetNamespaceUnknownHandler(Tcl_Interp *interp,
	    Tcl_Namespace *nsPtr)
}
declare 568 generic {
    int Tcl_SetNamespaceUnknownHandler(Tcl_Interp *interp,
	    Tcl_Namespace *nsPtr, Tcl_Obj *handlerPtr)
}

# TIP#258 (enhanced interface for encodings) dgp
declare 569 generic {
    int Tcl_GetEncodingFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    Tcl_Encoding *encodingPtr)
}
declare 570 generic {
    Tcl_Obj *Tcl_GetEncodingSearchPath(void)
}
declare 571 generic {
    int Tcl_SetEncodingSearchPath(Tcl_Obj *searchPath)
}
declare 572 generic {
    CONST char *Tcl_GetEncodingNameFromEnvironment(Tcl_DString *bufPtr)
}

# TIP#268 (extended version numbers and requirements) akupries
declare 573 generic {
    int Tcl_PkgRequireProc(Tcl_Interp *interp, CONST char *name,
	    int objc, Tcl_Obj *CONST objv[], ClientData *clientDataPtr)
}

# TIP#270 (utility C routines for string formatting) dgp
declare 574 generic {
    void Tcl_AppendObjToErrorInfo(Tcl_Interp *interp, Tcl_Obj *objPtr)
}
declare 575 generic {
    void Tcl_AppendLimitedToObj(Tcl_Obj *objPtr, CONST char *bytes, int length,
	    int limit, CONST char *ellipsis)
}
declare 576 generic {
    Tcl_Obj *Tcl_Format(Tcl_Interp *interp, CONST char *format, int objc,
	    Tcl_Obj *CONST objv[])
}
declare 577 generic {
    int Tcl_AppendFormatToObj(Tcl_Interp *interp, Tcl_Obj *objPtr,
	    CONST char *format, int objc, Tcl_Obj *CONST objv[])
}
declare 578 generic {
    Tcl_Obj *Tcl_ObjPrintf(CONST char *format, ...)
}
declare 579 generic {
    void Tcl_AppendPrintfToObj(Tcl_Obj *objPtr, CONST char *format, ...)
}

##############################################################################

# Define the platform specific public Tcl interface. These functions are only
# available on the designated platform.

interface tclPlat

################################
# Unix specific functions
#   (none)

################################
# Windows specific functions

# Added in Tcl 8.1

declare 0 win {
    TCHAR *Tcl_WinUtfToTChar(CONST char *str, int len, Tcl_DString *dsPtr)
}
declare 1 win {
    char *Tcl_WinTCharToUtf(CONST TCHAR *str, int len, Tcl_DString *dsPtr)
}

################################
# Mac OS X specific functions

declare 0 macosx {
    int Tcl_MacOSXOpenBundleResources(Tcl_Interp *interp,
	    CONST char *bundleName, int hasResourceFile,
	    int maxPathLen, char *libraryPath)
}
declare 1 macosx {
    int Tcl_MacOSXOpenVersionedBundleResources(Tcl_Interp *interp,
	    CONST char *bundleName, CONST char *bundleVersion,
	    int hasResourceFile, int maxPathLen, char *libraryPath)
}

##############################################################################

# Public functions that are not accessible via the stubs table.

export {
    void Tcl_Main(int argc, char **argv, Tcl_AppInitProc *appInitProc)
}
export {
    CONST char *Tcl_InitStubs(Tcl_Interp *interp, CONST char *version,
	int exact)
}
export {
    CONST char *TclTomMathInitializeStubs(Tcl_Interp* interp,
	CONST char* version, int epoch, int revision)
}
export {
    CONST char *Tcl_PkgInitStubsCheck(Tcl_Interp *interp, CONST char *version,
	int exact)
}
export {
    void Tcl_GetMemoryInfo(Tcl_DString *dsPtr)
}

# Global variables that need to be exported from the tcl shared library.

export {
    TclStubs *tclStubsPtr                       (fool checkstubs)
}
export {
    TclPlatStubs *tclPlatStubsPtr               (fool checkstubs)
}
export {
    TclIntStubs *tclIntStubsPtr                 (fool checkstubs)
}
export {
    TclIntPlatStubs *tclIntPlatStubsPtr         (fool checkstubs)
}
export {
    TclTomMathStubs* tclTomMathStubsPtr         (fool checkstubs)
}
# Local Variables:
# mode: tcl
# End:
