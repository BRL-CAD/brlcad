/*
 * tkMacOSXDialog.c --
 *
 *	Contains the Mac implementation of the common dialog boxes.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 * Copyright 2001-2009, Apple Inc.
 * Copyright (c) 2006-2009 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkMacOSXPrivate.h"
#include "tkFileFilter.h"

static const char *colorOptionStrings[] = {
    "-initialcolor", "-parent", "-title", NULL
};
enum colorOptions {
    COLOR_INITIAL, COLOR_PARENT, COLOR_TITLE
};

static const char *openOptionStrings[] = {
    "-defaultextension", "-filetypes", "-initialdir", "-initialfile",
    "-message", "-multiple", "-parent", "-title", "-typevariable",
    "-command", NULL
};
enum openOptions {
    OPEN_DEFAULT, OPEN_FILETYPES, OPEN_INITDIR, OPEN_INITFILE,
    OPEN_MESSAGE, OPEN_MULTIPLE, OPEN_PARENT, OPEN_TITLE,
    OPEN_TYPEVARIABLE, OPEN_COMMAND,
};
static const char *saveOptionStrings[] = {
    "-defaultextension", "-filetypes", "-initialdir", "-initialfile",
    "-message", "-parent", "-title", "-typevariable", "-command",
    "-confirmoverwrite", NULL
};
enum saveOptions {
    SAVE_DEFAULT, SAVE_FILETYPES, SAVE_INITDIR, SAVE_INITFILE,
    SAVE_MESSAGE, SAVE_PARENT, SAVE_TITLE, SAVE_TYPEVARIABLE, SAVE_COMMAND,
    SAVE_CONFIRMOW
};
static const char *chooseOptionStrings[] = {
    "-initialdir", "-message", "-mustexist", "-parent", "-title", "-command",
    NULL
};
enum chooseOptions {
    CHOOSE_INITDIR, CHOOSE_MESSAGE, CHOOSE_MUSTEXIST, CHOOSE_PARENT,
    CHOOSE_TITLE, CHOOSE_COMMAND,
};
typedef struct {
    Tcl_Interp *interp;
    Tcl_Obj *cmdObj;
    int multiple;
} FilePanelCallbackInfo;

static const char *alertOptionStrings[] = {
    "-default", "-detail", "-icon", "-message", "-parent", "-title",
    "-type", "-command", NULL
};
enum alertOptions {
    ALERT_DEFAULT, ALERT_DETAIL, ALERT_ICON, ALERT_MESSAGE, ALERT_PARENT,
    ALERT_TITLE, ALERT_TYPE, ALERT_COMMAND,
};
typedef struct {
    Tcl_Interp *interp;
    Tcl_Obj *cmdObj;
    int typeIndex;
} AlertCallbackInfo;
static const char *alertTypeStrings[] = {
    "abortretryignore", "ok", "okcancel", "retrycancel", "yesno",
    "yesnocancel", NULL
};
enum alertTypeOptions {
    TYPE_ABORTRETRYIGNORE, TYPE_OK, TYPE_OKCANCEL, TYPE_RETRYCANCEL,
    TYPE_YESNO, TYPE_YESNOCANCEL
};
static const char *alertIconStrings[] = {
    "error", "info", "question", "warning", NULL
};
enum alertIconOptions {
    ICON_ERROR, ICON_INFO, ICON_QUESTION, ICON_WARNING
};
static const char *alertButtonStrings[] = {
    "abort", "retry", "ignore", "ok", "cancel", "yes", "no", NULL
};

static const NSString *const alertButtonNames[][3] = {
    [TYPE_ABORTRETRYIGNORE] =   {@"Abort", @"Retry", @"Ignore"},
    [TYPE_OK] =			{@"OK"},
    [TYPE_OKCANCEL] =		{@"OK", @"Cancel"},
    [TYPE_RETRYCANCEL] =	{@"Retry", @"Cancel"},
    [TYPE_YESNO] =		{@"Yes", @"No"},
    [TYPE_YESNOCANCEL] =	{@"Yes", @"No", @"Cancel"},
};
static const NSAlertStyle alertStyles[] = {
    [ICON_ERROR] =		NSWarningAlertStyle,
    [ICON_INFO] =		NSInformationalAlertStyle,
    [ICON_QUESTION] =		NSWarningAlertStyle,
    [ICON_WARNING] =		NSCriticalAlertStyle,
};

/*
 * Need to map from 'alertButtonStrings' and its corresponding integer,
 * index to the native button index, which is 1, 2, 3, from right to left.
 * This is necessary to do for each separate '-type' of button sets.
 */

static const short alertButtonIndexAndTypeToNativeButtonIndex[][7] = {
			    /*  abort retry ignore ok   cancel yes   no */
    [TYPE_ABORTRETRYIGNORE] =   {1,    2,    3,    0,    0,    0,    0},
    [TYPE_OK] =			{0,    0,    0,    1,    0,    0,    0},
    [TYPE_OKCANCEL] =		{0,    0,    0,    1,    2,    0,    0},
    [TYPE_RETRYCANCEL] =	{0,    1,    0,    0,    2,    0,    0},
    [TYPE_YESNO] =		{0,    0,    0,    0,    0,    1,    2},
    [TYPE_YESNOCANCEL] =	{0,    0,    0,    0,    3,    1,    2},
};

/*
 * Need also the inverse mapping, from NSAlertFirstButtonReturn etc to the
 * descriptive button text string index.
 */

static const short alertNativeButtonIndexAndTypeToButtonIndex[][3] = {
    [TYPE_ABORTRETRYIGNORE] =   {0, 1, 2},
    [TYPE_OK] =			{3, 0, 0},
    [TYPE_OKCANCEL] =		{3, 4, 0},
    [TYPE_RETRYCANCEL] =	{1, 4, 0},
    [TYPE_YESNO] =		{5, 6, 0},
    [TYPE_YESNOCANCEL] =	{5, 6, 4},
};

#pragma mark TKApplication(TKDialog)

@interface NSColorPanel(TKDialog)
- (void)_setUseModalAppearance:(BOOL)flag;
@end

@implementation TKApplication(TKDialog)
- (void)tkFilePanelDidEnd:(NSSavePanel *)panel returnCode:(NSInteger)returnCode
	contextInfo:(void *)contextInfo {
    FilePanelCallbackInfo *callbackInfo = contextInfo;

    if (returnCode == NSFileHandlingPanelOKButton) {
	Tcl_Obj *resultObj;
	if (callbackInfo->multiple) {
	    resultObj = Tcl_NewListObj(0, NULL);
	    for (NSString *name in [(NSOpenPanel*)panel filenames]) {
		Tcl_ListObjAppendElement(callbackInfo->interp, resultObj,
			Tcl_NewStringObj([name UTF8String], -1));
	    }
	} else {
	    resultObj = Tcl_NewStringObj([[panel filename] UTF8String], -1);
	}
	if (callbackInfo->cmdObj) {
	    Tcl_Obj **objv, **tmpv;
	    int objc, result = Tcl_ListObjGetElements(callbackInfo->interp,
		    callbackInfo->cmdObj, &objc, &objv);
	    if (result == TCL_OK && objc) {
		tmpv = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * (objc + 2));
		memcpy(tmpv, objv, sizeof(Tcl_Obj *) * objc);
		tmpv[objc] = resultObj;
		TkBackgroundEvalObjv(callbackInfo->interp, objc + 1, tmpv,
			TCL_EVAL_GLOBAL);
		ckfree((char *)tmpv);
	    }
	} else {
	    Tcl_SetObjResult(callbackInfo->interp, resultObj);
	}
    } else if (returnCode == NSFileHandlingPanelCancelButton) {
	Tcl_ResetResult(callbackInfo->interp);
    }
    if (panel == [NSApp modalWindow]) {
	[NSApp stopModalWithCode:returnCode];
    }
    if (callbackInfo->cmdObj) {
	Tcl_DecrRefCount(callbackInfo->cmdObj);
	ckfree((char*) callbackInfo);
    }
}
- (void)tkAlertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode
	contextInfo:(void *)contextInfo {
    AlertCallbackInfo *callbackInfo = contextInfo;

    if (returnCode != NSAlertErrorReturn) {
	Tcl_Obj *resultObj = Tcl_NewStringObj(alertButtonStrings[
		alertNativeButtonIndexAndTypeToButtonIndex[callbackInfo->
		typeIndex][returnCode - NSAlertFirstButtonReturn]], -1);
	if (callbackInfo->cmdObj) {
	    Tcl_Obj **objv, **tmpv;
	    int objc, result = Tcl_ListObjGetElements(callbackInfo->interp,
		    callbackInfo->cmdObj, &objc, &objv);
	    if (result == TCL_OK && objc) {
		tmpv = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * (objc + 2));
		memcpy(tmpv, objv, sizeof(Tcl_Obj *) * objc);
		tmpv[objc] = resultObj;
		TkBackgroundEvalObjv(callbackInfo->interp, objc + 1, tmpv,
			TCL_EVAL_GLOBAL);
		ckfree((char *)tmpv);
	    }
	} else {
	    Tcl_SetObjResult(callbackInfo->interp, resultObj);
	}
    }
    if ([alert window] == [NSApp modalWindow]) {
	[NSApp stopModalWithCode:returnCode];
    }
    if (callbackInfo->cmdObj) {
	Tcl_DecrRefCount(callbackInfo->cmdObj);
	ckfree((char*) callbackInfo);
    }
}
@end

#pragma mark -

/*
 *----------------------------------------------------------------------
 *
 * Tk_ChooseColorObjCmd --
 *
 *	This procedure implements the color dialog box for the Mac platform.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_ChooseColorObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int result = TCL_ERROR;
    Tk_Window parent, tkwin = clientData;
    const char *title = NULL;
    int i;
    NSColor *color = nil, *initialColor = nil;
    NSColorPanel *colorPanel;
    NSInteger returnCode, numberOfComponents = 0;

    for (i = 1; i < objc; i += 2) {
	int index;
	const char *option, *value;

	if (Tcl_GetIndexFromObj(interp, objv[i], colorOptionStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    goto end;
	}
	if (i + 1 == objc) {
	    option = Tcl_GetString(objv[i]);
	    Tcl_AppendResult(interp, "value for \"", option, "\" missing",
		    NULL);
	    goto end;
	}
	value = Tcl_GetString(objv[i + 1]);

	switch (index) {
	    case COLOR_INITIAL: {
		XColor *colorPtr;

		colorPtr = Tk_GetColor(interp, tkwin, value);
		if (colorPtr == NULL) {
		    goto end;
		}
		initialColor = TkMacOSXGetNSColor(NULL, colorPtr->pixel);
		Tk_FreeColor(colorPtr);
		break;
	    }
	    case COLOR_PARENT: {
		parent = Tk_NameToWindow(interp, value, tkwin);
		if (parent == NULL) {
		    goto end;
		}
		break;
	    }
	    case COLOR_TITLE: {
		title = value;
		break;
	    }
	}
    }
    colorPanel = [NSColorPanel sharedColorPanel];
    [colorPanel orderOut:NSApp];
    [colorPanel setContinuous:NO];
    [colorPanel setBecomesKeyOnlyIfNeeded:NO];
    [colorPanel setShowsAlpha: NO];
    [colorPanel _setUseModalAppearance:YES];
    if (title) {
	NSString *s = [[NSString alloc] initWithUTF8String:title];
	[colorPanel setTitle:s];
	[s release];
    }
    if (initialColor) {
	[colorPanel setColor:initialColor];
    }
    returnCode = [NSApp runModalForWindow:colorPanel];
    if (returnCode == NSOKButton) {
	color = [[colorPanel color] colorUsingColorSpace:
		[NSColorSpace genericRGBColorSpace]];
	numberOfComponents = [color numberOfComponents];
    }
    if (color && numberOfComponents >= 3 && numberOfComponents <= 4) {
	CGFloat components[4];
	char colorstr[8];

	[color getComponents:components];
	snprintf(colorstr, 8, "#%02x%02x%02x",
		(short)(components[0] * 255),
		(short)(components[1] * 255),
		(short)(components[2] * 255));
	Tcl_SetObjResult(interp, Tcl_NewStringObj(colorstr, 7));
    } else {
	Tcl_ResetResult(interp);
    }
    result = TCL_OK;
end:
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetOpenFileObjCmd --
 *
 *	This procedure implements the "open file" dialog box for the Mac
 *	platform. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See user documentation.
 *----------------------------------------------------------------------
 */

int
Tk_GetOpenFileObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tk_Window tkwin = clientData;
    char *str;
    int i, result = TCL_ERROR, haveParentOption = 0;
    int index, len, multiple = 0;
    FileFilterList fl;
    Tcl_Obj *cmdObj = NULL, *typeVariablePtr = NULL;
    FilePanelCallbackInfo callbackInfoStruct;
    FilePanelCallbackInfo *callbackInfo = &callbackInfoStruct;
    NSString *directory = nil, *filename = nil;
    NSString *message, *title, *type;
    NSWindow *parent;
    NSMutableArray *fileTypes = nil;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSInteger returnCode = NSAlertErrorReturn;

    TkInitFileFilters(&fl);
    for (i = 1; i < objc; i += 2) {
	if (Tcl_GetIndexFromObj(interp, objv[i], openOptionStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    goto end;
	}
	if (i + 1 == objc) {
	    str = Tcl_GetString(objv[i]);
	    Tcl_AppendResult(interp, "value for \"", str, "\" missing", NULL);
	    goto end;
	}
	switch (index) {
	case OPEN_DEFAULT:
	    break;
	case OPEN_FILETYPES:
	    if (TkGetFileFilters(interp, &fl, objv[i + 1], 0) != TCL_OK) {
		goto end;
	    }
	    break;
	case OPEN_INITDIR:
	    str = Tcl_GetStringFromObj(objv[i + 1], &len);
	    if (len) {
		directory = [[[NSString alloc] initWithUTF8String:str]
			autorelease];
	    }
	    break;
	case OPEN_INITFILE:
	    str = Tcl_GetStringFromObj(objv[i + 1], &len);
	    if (len) {
		filename = [[[NSString alloc] initWithUTF8String:str]
			autorelease];
	    }
	    break;
	case OPEN_MESSAGE:
	    message = [[NSString alloc] initWithUTF8String:
		    Tcl_GetString(objv[i + 1])];
	    [panel setMessage:message];
	    [message release];
	    break;
	case OPEN_MULTIPLE:
	    if (Tcl_GetBooleanFromObj(interp, objv[i + 1],
		    &multiple) != TCL_OK) {
		goto end;
	    }
	    break;
	case OPEN_PARENT:
	    str = Tcl_GetStringFromObj(objv[i + 1], &len);
	    tkwin = Tk_NameToWindow(interp, str, tkwin);
	    if (!tkwin) {
		goto end;
	    }
	    haveParentOption = 1;
	    break;
	case OPEN_TITLE:
	    title = [[NSString alloc] initWithUTF8String:
		    Tcl_GetString(objv[i + 1])];
	    [panel setTitle:title];
	    [title release];
	    break;
	case OPEN_TYPEVARIABLE:
	    typeVariablePtr = objv[i + 1];
	    break;
	case OPEN_COMMAND:
	    cmdObj = objv[i+1];
	    break;
	}
    }
    if (fl.filters) {
	fileTypes = [NSMutableArray array];
	for (FileFilter *filterPtr = fl.filters; filterPtr;
		filterPtr = filterPtr->next) {
	    for (FileFilterClause *clausePtr = filterPtr->clauses; clausePtr;
		    clausePtr = clausePtr->next) {
		for (GlobPattern *globPtr = clausePtr->patterns; globPtr;
			globPtr = globPtr->next) {
		    str = globPtr->pattern;
		    while (*str && (*str == '*' || *str == '.')) {
			str++;
		    }
		    if (*str) {
			type = [[NSString alloc] initWithUTF8String:str];
			if (![fileTypes containsObject:type]) {
			    [fileTypes addObject:type];
			}
			[type release];
		    }
		}
		for (MacFileType *mfPtr = clausePtr->macTypes; mfPtr;
			mfPtr = mfPtr->next) {
		    if (mfPtr->type) {
			type = NSFileTypeForHFSTypeCode(mfPtr->type);
			if (![fileTypes containsObject:type]) {
			    [fileTypes addObject:type];
			}
		    }
		}
	    }
	}
    }
    [panel setAllowsMultipleSelection:multiple];
    if (cmdObj) {
	callbackInfo = (FilePanelCallbackInfo *)
		ckalloc(sizeof(FilePanelCallbackInfo));
	if (Tcl_IsShared(cmdObj)) {
	    cmdObj = Tcl_DuplicateObj(cmdObj);
	}
	Tcl_IncrRefCount(cmdObj);
    }
    callbackInfo->cmdObj = cmdObj;
    callbackInfo->interp = interp;
    callbackInfo->multiple = multiple;
    parent = TkMacOSXDrawableWindow(((TkWindow *) tkwin)->window);
    if (haveParentOption && parent && ![parent attachedSheet]) {
	[panel beginSheetForDirectory:directory file:filename types:fileTypes
		modalForWindow:parent modalDelegate:NSApp didEndSelector:
		@selector(tkFilePanelDidEnd:returnCode:contextInfo:)
		contextInfo:callbackInfo];
	returnCode = cmdObj ? NSAlertOtherReturn :
		[NSApp runModalForWindow:panel];
    } else {
	returnCode = [panel runModalForDirectory:directory file:filename
		types:fileTypes];
	[NSApp tkFilePanelDidEnd:panel returnCode:returnCode
		contextInfo:callbackInfo];
    }
    result = (returnCode != NSAlertErrorReturn) ? TCL_OK : TCL_ERROR;
    if (typeVariablePtr && result == TCL_OK) {
	/*
	 * The -typevariable option is not really supported.
	 */

	Tcl_SetVar(interp, Tcl_GetString(typeVariablePtr), "",
		TCL_GLOBAL_ONLY);
    }
end:
    TkFreeFileFilters(&fl);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetSaveFileObjCmd --
 *
 *	This procedure implements the "save file" dialog box for the Mac
 *	platform. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See user documentation.
 *----------------------------------------------------------------------
 */

int
Tk_GetSaveFileObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tk_Window tkwin = clientData;
    char *str;
    int i, result = TCL_ERROR, haveParentOption = 0;
    int confirmOverwrite = 1;
    int index, len;
    FileFilterList fl;
    Tcl_Obj *cmdObj = NULL;
    FilePanelCallbackInfo callbackInfoStruct;
    FilePanelCallbackInfo *callbackInfo = &callbackInfoStruct;
    NSString *directory = nil, *filename = nil, *defaultType = nil;
    NSString *message, *title, *type;
    NSWindow *parent;
    NSMutableArray *fileTypes = nil;
    NSSavePanel *panel = [NSSavePanel savePanel];
    NSInteger returnCode = NSAlertErrorReturn;

    TkInitFileFilters(&fl);
    for (i = 1; i < objc; i += 2) {
	if (Tcl_GetIndexFromObj(interp, objv[i], saveOptionStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    goto end;
	}
	if (i + 1 == objc) {
	    str = Tcl_GetString(objv[i]);
	    Tcl_AppendResult(interp, "value for \"", str, "\" missing",
		    NULL);
	    goto end;
	}
	switch (index) {
	case SAVE_DEFAULT:
	    str = Tcl_GetStringFromObj(objv[i + 1], &len);
	    while (*str && (*str == '*' || *str == '.')) {
		str++;
	    }
	    if (*str) {
		defaultType = [[[NSString alloc] initWithUTF8String:str]
			autorelease];
	    }
	    break;
	case SAVE_FILETYPES:
	    if (TkGetFileFilters(interp, &fl, objv[i + 1], 0) != TCL_OK) {
		goto end;
	    }
	    break;
	case SAVE_INITDIR:
	    str = Tcl_GetStringFromObj(objv[i + 1], &len);
	    if (len) {
		directory = [[[NSString alloc] initWithUTF8String:str]
			autorelease];
	    }
	    break;
	case SAVE_INITFILE:
	    str = Tcl_GetStringFromObj(objv[i + 1], &len);
	    if (len) {
		filename = [[[NSString alloc] initWithUTF8String:str]
			autorelease];
	    }
	    break;
	case SAVE_MESSAGE:
	    message = [[NSString alloc] initWithUTF8String:
		    Tcl_GetString(objv[i + 1])];
	    [panel setMessage:message];
	    [message release];
	    break;
	case SAVE_PARENT:
	    str = Tcl_GetStringFromObj(objv[i + 1], &len);
	    tkwin = Tk_NameToWindow(interp, str, tkwin);
	    if (!tkwin) {
		goto end;
	    }
	    haveParentOption = 1;
	    break;
	case SAVE_TITLE:
	    title = [[NSString alloc] initWithUTF8String:
		    Tcl_GetString(objv[i + 1])];
	    [panel setTitle:title];
	    [title release];
	    break;
	case SAVE_TYPEVARIABLE:
	    break;
	case SAVE_COMMAND:
	    cmdObj = objv[i+1];
	    break;
	case SAVE_CONFIRMOW:
	    if (Tcl_GetBooleanFromObj(interp, objv[i + 1],
				      &confirmOverwrite) != TCL_OK) {
		goto end;
	    }
	    break;
	}
    }
    if (fl.filters || defaultType) {
	fileTypes = [NSMutableArray array];
	[fileTypes addObject:defaultType ? defaultType : (id)kUTTypeContent];
	for (FileFilter *filterPtr = fl.filters; filterPtr;
		filterPtr = filterPtr->next) {
	    for (FileFilterClause *clausePtr = filterPtr->clauses; clausePtr;
		    clausePtr = clausePtr->next) {
		for (GlobPattern *globPtr = clausePtr->patterns; globPtr;
			globPtr = globPtr->next) {
		    str = globPtr->pattern;
		    while (*str && (*str == '*' || *str == '.')) {
			str++;
		    }
		    if (*str) {
			type = [[NSString alloc] initWithUTF8String:str];
			if (![fileTypes containsObject:type]) {
			    [fileTypes addObject:type];
			}
			[type release];
		    }
		}
	    }
	}
	[panel setAllowedFileTypes:fileTypes];
	[panel setAllowsOtherFileTypes:YES];
    }
    [panel setCanSelectHiddenExtension:YES];
    [panel setExtensionHidden:NO];
    if (cmdObj) {
	callbackInfo = (FilePanelCallbackInfo *)
		ckalloc(sizeof(FilePanelCallbackInfo));
	if (Tcl_IsShared(cmdObj)) {
	    cmdObj = Tcl_DuplicateObj(cmdObj);
	}
	Tcl_IncrRefCount(cmdObj);
    }
    callbackInfo->cmdObj = cmdObj;
    callbackInfo->interp = interp;
    callbackInfo->multiple = 0;
    parent = TkMacOSXDrawableWindow(((TkWindow *) tkwin)->window);
    if (haveParentOption && parent && ![parent attachedSheet]) {
	[panel beginSheetForDirectory:directory file:filename
		modalForWindow:parent modalDelegate:NSApp didEndSelector:
		@selector(tkFilePanelDidEnd:returnCode:contextInfo:)
		contextInfo:callbackInfo];
	returnCode = cmdObj ? NSAlertOtherReturn :
		[NSApp runModalForWindow:panel];
    } else {
	returnCode = [panel runModalForDirectory:directory file:filename];
	[NSApp tkFilePanelDidEnd:panel returnCode:returnCode
		contextInfo:callbackInfo];
    }
    result = (returnCode != NSAlertErrorReturn) ? TCL_OK : TCL_ERROR;
end:
    TkFreeFileFilters(&fl);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ChooseDirectoryObjCmd --
 *
 *	This procedure implements the "tk_chooseDirectory" dialog box for the
 *	MacOS X platform. See the user documentation for details on what it
 *	does.
 *
 * Results:
 *	See user documentation.
 *
 * Side effects:
 *	A modal dialog window is created.
 *
 *----------------------------------------------------------------------
 */

int
Tk_ChooseDirectoryObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tk_Window tkwin = clientData;
    char *str;
    int i, result = TCL_ERROR, haveParentOption = 0;
    int index, len, mustexist = 0;
    Tcl_Obj *cmdObj = NULL;
    FilePanelCallbackInfo callbackInfoStruct;
    FilePanelCallbackInfo *callbackInfo = &callbackInfoStruct;
    NSString *directory = nil, *filename = nil;
    NSString *message, *title;
    NSWindow *parent;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSInteger returnCode = NSAlertErrorReturn;

    for (i = 1; i < objc; i += 2) {
	if (Tcl_GetIndexFromObj(interp, objv[i], chooseOptionStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    goto end;
	}
	if (i + 1 == objc) {
	    str = Tcl_GetString(objv[i]);
	    Tcl_AppendResult(interp, "value for \"", str, "\" missing", NULL);
	    goto end;
	}
	switch (index) {
	case CHOOSE_INITDIR:
	    str = Tcl_GetStringFromObj(objv[i + 1], &len);
	    if (len) {
		directory = [[[NSString alloc] initWithUTF8String:str]
			autorelease];
	    }
	    break;
	case CHOOSE_MESSAGE:
	    message = [[NSString alloc] initWithUTF8String:
		    Tcl_GetString(objv[i + 1])];
	    [panel setMessage:message];
	    [message release];
	    break;
	case CHOOSE_MUSTEXIST:
	    if (Tcl_GetBooleanFromObj(interp, objv[i + 1],
		    &mustexist) != TCL_OK) {
		goto end;
	    }
	    break;
	case CHOOSE_PARENT:
	    str = Tcl_GetStringFromObj(objv[i + 1], &len);
	    tkwin = Tk_NameToWindow(interp, str, tkwin);
	    if (!tkwin) {
		goto end;
	    }
	    haveParentOption = 1;
	    break;
	case CHOOSE_TITLE:
	    title = [[NSString alloc] initWithUTF8String:
		    Tcl_GetString(objv[i + 1])];
	    [panel setTitle:title];
	    [title release];
	    break;
	case CHOOSE_COMMAND:
	    cmdObj = objv[i+1];
	    break;
	}
    }
    [panel setPrompt:@"Choose"];
    [panel setCanChooseFiles:NO];
    [panel setCanChooseDirectories:YES];
    [panel setCanCreateDirectories:!mustexist];
    if (cmdObj) {
	callbackInfo = (FilePanelCallbackInfo *)
		ckalloc(sizeof(FilePanelCallbackInfo));
	if (Tcl_IsShared(cmdObj)) {
	    cmdObj = Tcl_DuplicateObj(cmdObj);
	}
	Tcl_IncrRefCount(cmdObj);
    }
    callbackInfo->cmdObj = cmdObj;
    callbackInfo->interp = interp;
    callbackInfo->multiple = 0;
    parent = TkMacOSXDrawableWindow(((TkWindow *) tkwin)->window);
    if (haveParentOption && parent && ![parent attachedSheet]) {
	[panel beginSheetForDirectory:directory file:filename
		modalForWindow:parent modalDelegate:NSApp didEndSelector:
		@selector(tkFilePanelDidEnd:returnCode:contextInfo:)
		contextInfo:callbackInfo];
	returnCode = cmdObj ? NSAlertOtherReturn :
		[NSApp runModalForWindow:panel];
    } else {
	returnCode = [panel runModalForDirectory:directory file:filename];
	[NSApp tkFilePanelDidEnd:panel returnCode:returnCode
		contextInfo:callbackInfo];
    }
    result = (returnCode != NSAlertErrorReturn) ? TCL_OK : TCL_ERROR;
end:
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkAboutDlg --
 *
 *	Displays the default Tk About box.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkAboutDlg(void)
{
    NSImage *image;
    NSString *path = [NSApp tkFrameworkImagePath:@"Tk.tiff"];
    if (path) {
	image = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
    } else {
	image = [NSApp applicationIconImage];
    }
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setFormatterBehavior:NSDateFormatterBehavior10_4];
    [dateFormatter setDateFormat:@"Y"];
    NSString *year = [dateFormatter stringFromDate:[NSDate date]];
    [dateFormatter release];
    NSMutableParagraphStyle *style = [[[NSParagraphStyle defaultParagraphStyle]
	    mutableCopy] autorelease];
    [style setAlignment:NSCenterTextAlignment];
    NSDictionary *options = [NSDictionary dictionaryWithObjectsAndKeys:
	    @"Tcl & Tk", @"ApplicationName",
	    @"Tcl " TCL_VERSION " & Tk " TK_VERSION, @"ApplicationVersion",
	    @TK_PATCH_LEVEL, @"Version",
	    image, @"ApplicationIcon",
	    [NSString stringWithFormat:@"Copyright %1$C 1987-%2$@.", 0xA9,
	    year], @"Copyright",
	    [[[NSAttributedString alloc] initWithString:
	    [NSString stringWithFormat:
	    @"%1$C 1987-%2$@ Tcl Core Team." "\n\n"
	     "%1$C 2002-%2$@ Daniel A. Steffen." "\n\n"
	     "%1$C 2001-2009 Apple Inc." "\n\n"
	     "%1$C 2001-2002 Jim Ingham & Ian Reid" "\n\n"
	     "%1$C 1998-2000 Jim Ingham & Ray Johnson" "\n\n"
	     "%1$C 1998-2000 Scriptics Inc." "\n\n"
	     "%1$C 1996-1997 Sun Microsystems Inc.", 0xA9, year] attributes:
	    [NSDictionary dictionaryWithObject:style
	    forKey:NSParagraphStyleAttributeName]] autorelease], @"Credits",
	    nil];
    [NSApp orderFrontStandardAboutPanelWithOptions:options];
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXStandardAboutPanelObjCmd --
 *
 *	Implements the ::tk::mac::standardAboutPanel command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	none
 *
 *----------------------------------------------------------------------
 */

int
TkMacOSXStandardAboutPanelObjCmd(
    ClientData clientData,	/* Unused. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc > 1) {
	Tcl_WrongNumArgs(interp, 1, objv, NULL);
	return TCL_ERROR;
    }
    [NSApp orderFrontStandardAboutPanelWithOptions:nil];
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MessageBoxObjCmd --
 *
 *	Implements the tk_messageBox in native Mac OS X style.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	none
 *
 *----------------------------------------------------------------------
 */

int
Tk_MessageBoxObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tk_Window tkwin = clientData;
    char *str;
    int i, result = TCL_ERROR, haveParentOption = 0;
    int index, typeIndex, iconIndex, indexDefaultOption = 0;
    int defaultNativeButtonIndex = 1; /* 1, 2, 3: right to left */
    Tcl_Obj *cmdObj = NULL;
    AlertCallbackInfo callbackInfoStruct, *callbackInfo = &callbackInfoStruct;
    NSString *message, *title;
    NSWindow *parent;
    NSArray *buttons;
    NSAlert *alert = [NSAlert new];
    NSInteger returnCode = NSAlertErrorReturn;

    iconIndex = ICON_INFO;
    typeIndex = TYPE_OK;
    for (i = 1; i < objc; i += 2) {
	if (Tcl_GetIndexFromObj(interp, objv[i], alertOptionStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    goto end;
	}
	if (i + 1 == objc) {
	    str = Tcl_GetString(objv[i]);
	    Tcl_AppendResult(interp, "value for \"", str, "\" missing", NULL);
	    goto end;
	}
	switch (index) {
	case ALERT_DEFAULT:
	    /*
	     * Need to postpone processing of this option until we are sure to
	     * know the '-type' as well.
	     */

	    indexDefaultOption = i;
	    break;

	case ALERT_DETAIL:
	    message = [[NSString alloc] initWithUTF8String:
		    Tcl_GetString(objv[i + 1])];
	    [alert setInformativeText:message];
	    [message release];
	    break;

	case ALERT_ICON:
	    if (Tcl_GetIndexFromObj(interp, objv[i + 1], alertIconStrings,
		    "value", TCL_EXACT, &iconIndex) != TCL_OK) {
		goto end;
	    }
	    break;

	case ALERT_MESSAGE:
	    message = [[NSString alloc] initWithUTF8String:
		    Tcl_GetString(objv[i + 1])];
	    [alert setMessageText:message];
	    [message release];
	    break;

	case ALERT_PARENT:
	    str = Tcl_GetString(objv[i + 1]);
	    tkwin = Tk_NameToWindow(interp, str, tkwin);
	    if (!tkwin) {
		goto end;
	    }
	    haveParentOption = 1;
	    break;

	case ALERT_TITLE:
	    title = [[NSString alloc] initWithUTF8String:
		    Tcl_GetString(objv[i + 1])];
	    [[alert window] setTitle:title];
	    [title release];
	    break;

	case ALERT_TYPE:
	    if (Tcl_GetIndexFromObj(interp, objv[i + 1], alertTypeStrings,
		    "value", TCL_EXACT, &typeIndex) != TCL_OK) {
		goto end;
	    }
	    break;
	case ALERT_COMMAND:
	    cmdObj = objv[i+1];
	    break;
	}
    }
    if (indexDefaultOption) {
	/*
	 * Any '-default' option needs to know the '-type' option, which is why
	 * we do this here.
	 */

	if (Tcl_GetIndexFromObj(interp, objv[indexDefaultOption + 1],
		alertButtonStrings, "value", TCL_EXACT, &index)
		!= TCL_OK) {
	    goto end;
	}

	/*
	 * Need to map from "ok" etc. to 1, 2, 3, right to left.
	 */

	defaultNativeButtonIndex =
		alertButtonIndexAndTypeToNativeButtonIndex[typeIndex][index];
	if (!defaultNativeButtonIndex) {
	    Tcl_SetObjResult(interp,
		    Tcl_NewStringObj("Illegal default option", -1));
	    goto end;
	}
    }
    [alert setIcon:[NSApp applicationIconImage]];
    [alert setAlertStyle:alertStyles[iconIndex]];
    i = 0;
    while (i < 3 && alertButtonNames[typeIndex][i]) {
	[alert addButtonWithTitle:(NSString*)alertButtonNames[typeIndex][i++]];
    }
    buttons = [alert buttons];
    for (NSButton *b in buttons) {
	NSString *ke = [b keyEquivalent];
	if (([ke isEqualToString:@"\r"] || [ke isEqualToString:@"\033"]) &&
		![b keyEquivalentModifierMask]) {
	    [b setKeyEquivalent:@""];
	}
    }
    [[buttons objectAtIndex:[buttons count]-1] setKeyEquivalent:@"\033"];
    [[buttons objectAtIndex:defaultNativeButtonIndex-1] setKeyEquivalent:@"\r"];
    if (cmdObj) {
	callbackInfo = (AlertCallbackInfo *) ckalloc(sizeof(AlertCallbackInfo));
	if (Tcl_IsShared(cmdObj)) {
	    cmdObj = Tcl_DuplicateObj(cmdObj);
	}
	Tcl_IncrRefCount(cmdObj);
    }
    callbackInfo->cmdObj = cmdObj;
    callbackInfo->interp = interp;
    callbackInfo->typeIndex = typeIndex;
    parent = TkMacOSXDrawableWindow(((TkWindow *) tkwin)->window);
    if (haveParentOption && parent && ![parent attachedSheet]) {
	[alert beginSheetModalForWindow:parent modalDelegate:NSApp
		didEndSelector:@selector(tkAlertDidEnd:returnCode:contextInfo:)
		contextInfo:callbackInfo];
	returnCode = cmdObj ? NSAlertOtherReturn :
		[NSApp runModalForWindow:[alert window]];
    } else {
	returnCode = [alert runModal];
	[NSApp tkAlertDidEnd:alert returnCode:returnCode
		contextInfo:callbackInfo];
    }
    result = (returnCode != NSAlertErrorReturn) ? TCL_OK : TCL_ERROR;
end:
    [alert release];
    return result;
}

/*
 * Local Variables:
 * mode: objc
 * c-basic-offset: 4
 * fill-column: 79
 * coding: utf-8
 * End:
 */
