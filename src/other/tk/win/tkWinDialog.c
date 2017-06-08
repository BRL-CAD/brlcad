/*
 * tkWinDialog.c --
 *
 *	Contains the Windows implementation of the common dialog boxes.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#define WINVER        0x0500   /* Requires Windows 2K definitions */
#define _WIN32_WINNT  0x0500
#include "tkWinInt.h"
#include "tkFileFilter.h"

#include <commdlg.h>		/* includes common dialog functionality */
#ifdef _MSC_VER
#   pragma comment (lib, "comdlg32.lib")
#endif
#include <dlgs.h>		/* includes common dialog template defines */
#include <cderr.h>		/* includes the common dialog error codes */

#include <shlobj.h>		/* includes SHBrowseForFolder */
#ifdef _MSC_VER
#   pragma comment (lib, "shell32.lib")
#endif

/* These needed for compilation with VC++ 5.2 */
#ifndef BIF_EDITBOX
#define BIF_EDITBOX 0x10
#endif

#ifndef BIF_VALIDATE
#define BIF_VALIDATE 0x0020
#endif

#ifndef BIF_NEWDIALOGSTYLE
#define BIF_NEWDIALOGSTYLE 0x0040
#endif

#ifndef BFFM_VALIDATEFAILED
#ifdef UNICODE
#define BFFM_VALIDATEFAILED 4
#else
#define BFFM_VALIDATEFAILED 3
#endif
#endif /* BFFM_VALIDATEFAILED */

#ifndef OPENFILENAME_SIZE_VERSION_400
#define OPENFILENAME_SIZE_VERSION_400 76
#endif

typedef struct ThreadSpecificData {
    int debugFlag;		/* Flags whether we should output debugging
				 * information while displaying a builtin
				 * dialog. */
    Tcl_Interp *debugInterp;	/* Interpreter to used for debugging. */
    UINT WM_LBSELCHANGED;	/* Holds a registered windows event used for
				 * communicating between the Directory Chooser
				 * dialog and its hook proc. */
    HHOOK hMsgBoxHook;		/* Hook proc for tk_messageBox and the */
    HICON hSmallIcon;		/* icons used by a parent to be used in */
    HICON hBigIcon;		/* the message box */
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;

/*
 * The following structures are used by Tk_MessageBoxCmd() to parse arguments
 * and return results.
 */

static const TkStateMap iconMap[] = {
    {MB_ICONERROR,		"error"},
    {MB_ICONINFORMATION,	"info"},
    {MB_ICONQUESTION,		"question"},
    {MB_ICONWARNING,		"warning"},
    {-1,			NULL}
};

static const TkStateMap typeMap[] = {
    {MB_ABORTRETRYIGNORE,	"abortretryignore"},
    {MB_OK,			"ok"},
    {MB_OKCANCEL,		"okcancel"},
    {MB_RETRYCANCEL,		"retrycancel"},
    {MB_YESNO,			"yesno"},
    {MB_YESNOCANCEL,		"yesnocancel"},
    {-1,			NULL}
};

static const TkStateMap buttonMap[] = {
    {IDABORT,			"abort"},
    {IDRETRY,			"retry"},
    {IDIGNORE,			"ignore"},
    {IDOK,			"ok"},
    {IDCANCEL,			"cancel"},
    {IDNO,			"no"},
    {IDYES,			"yes"},
    {-1,			NULL}
};

static const int buttonFlagMap[] = {
    MB_DEFBUTTON1, MB_DEFBUTTON2, MB_DEFBUTTON3, MB_DEFBUTTON4
};

static const struct {int type; int btnIds[3];} allowedTypes[] = {
    {MB_ABORTRETRYIGNORE,	{IDABORT, IDRETRY,  IDIGNORE}},
    {MB_OK,			{IDOK,	  -1,	    -1	    }},
    {MB_OKCANCEL,		{IDOK,	  IDCANCEL, -1	    }},
    {MB_RETRYCANCEL,		{IDRETRY, IDCANCEL, -1	    }},
    {MB_YESNO,			{IDYES,	  IDNO,	    -1	    }},
    {MB_YESNOCANCEL,		{IDYES,	  IDNO,	    IDCANCEL}}
};

#define NUM_TYPES (sizeof(allowedTypes) / sizeof(allowedTypes[0]))

/*
 * Abstract trivial differences between Win32 and Win64.
 */

#define TkWinGetHInstance(from) \
	((HINSTANCE) GetWindowLongPtrW((from), GWLP_HINSTANCE))
#define TkWinGetUserData(from) \
	GetWindowLongPtrW((from), GWLP_USERDATA)
#define TkWinSetUserData(to,what) \
	SetWindowLongPtrW((to), GWLP_USERDATA, (LPARAM)(what))

/*
 * The value of TK_MULTI_MAX_PATH dictates how many files can be retrieved
 * with tk_get*File -multiple 1. It must be allocated on the stack, so make it
 * large enough but not too large. - hobbs
 *
 * The data is stored as <dir>\0<file1>\0<file2>\0...<fileN>\0\0. Since
 * MAX_PATH == 260 on Win2K/NT, *40 is ~10Kbytes.
 */

#define TK_MULTI_MAX_PATH	(MAX_PATH*40)

/*
 * The following structure is used to pass information between the directory
 * chooser function, Tk_ChooseDirectoryObjCmd(), and its dialog hook proc.
 */

typedef struct {
   WCHAR initDir[MAX_PATH];	/* Initial folder to use */
   WCHAR retDir[MAX_PATH];	/* Returned folder to use */
   Tcl_Interp *interp;
   int mustExist;		/* True if file must exist to return from
				 * callback */
} ChooseDir;

/*
 * The following structure is used to pass information between GetFileName
 * function and OFN dialog hook procedures. [Bug 2896501, Patch 2898255]
 */

typedef struct OFNData {
    Tcl_Interp *interp;		/* Interp, used only if debug is turned on,
				 * for setting the "tk_dialog" variable. */
    int dynFileBufferSize;	/* Dynamic filename buffer size, stored to
				 * avoid shrinking and expanding the buffer
				 * when selection changes */
    WCHAR *dynFileBuffer;	/* Dynamic filename buffer */
} OFNData;

/*
 * Definitions of functions used only in this file.
 */

static UINT APIENTRY	ChooseDirectoryValidateProc(HWND hdlg, UINT uMsg,
			    LPARAM wParam, LPARAM lParam);
static UINT CALLBACK	ColorDlgHookProc(HWND hDlg, UINT uMsg, WPARAM wParam,
			    LPARAM lParam);
static int 		GetFileName(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[], int isOpen);
static int 		MakeFilter(Tcl_Interp *interp, Tcl_Obj *valuePtr,
			    Tcl_DString *dsPtr, Tcl_Obj *initialPtr,
			    int *indexPtr);
static UINT APIENTRY	OFNHookProc(HWND hdlg, UINT uMsg, WPARAM wParam,
			    LPARAM lParam);
static LRESULT CALLBACK MsgBoxCBTProc(int nCode, WPARAM wParam, LPARAM lParam);
static void		SetTkDialog(ClientData clientData);
static const char *ConvertExternalFilename(WCHAR *filename,
			    Tcl_DString *dsPtr);

/*
 *-------------------------------------------------------------------------
 *
 * EatSpuriousMessageBugFix --
 *
 *	In the file open/save dialog, double clicking on a list item causes
 *	the dialog box to close, but an unwanted WM_LBUTTONUP message is sent
 *	to the window underneath. If the window underneath happens to be a
 *	windows control (eg a button) then it will be activated by accident.
 *
 * 	This problem does not occur in dialog boxes, because windows must do
 * 	some special processing to solve the problem. (separate message
 * 	processing functions are used to cope with keyboard navigation of
 * 	controls.)
 *
 * 	Here is one solution. After returning, we poll the message queue for
 * 	1/4s looking for WM_LBUTTON up messages. If we see one it's consumed.
 * 	If we get a WM_LBUTTONDOWN message, then we exit early, since the user
 * 	must be doing something new. This fix only works for the current
 * 	application, so the problem will still occur if the open dialog
 * 	happens to be over another applications button. However this is a
 * 	fairly rare occurrance.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Consumes an unwanted BUTTON messages.
 *
 *-------------------------------------------------------------------------
 */

static void
EatSpuriousMessageBugFix(void)
{
    MSG msg;
    DWORD nTime = GetTickCount() + 250;

    while (GetTickCount() < nTime) {
	if (PeekMessageA(&msg, 0, WM_LBUTTONDOWN, WM_LBUTTONDOWN, PM_NOREMOVE)){
	    break;
	}
	PeekMessageA(&msg, 0, WM_LBUTTONUP, WM_LBUTTONUP, PM_REMOVE);
    }
}

/*
 *-------------------------------------------------------------------------
 *
 * TkWinDialogDebug --
 *
 *	Function to turn on/off debugging support for common dialogs under
 *	windows. The variable "tk_debug" is set to the identifier of the
 *	dialog window when the modal dialog window pops up and it is safe to
 *	send messages to the dialog.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This variable only makes sense if just one dialog is up at a time.
 *
 *-------------------------------------------------------------------------
 */

void
TkWinDialogDebug(
    int debug)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    tsdPtr->debugFlag = debug;
}

/*
 *-------------------------------------------------------------------------
 *
 * Tk_ChooseColorObjCmd --
 *
 *	This function implements the color dialog box for the Windows
 *	platform. See the user documentation for details on what it does.
 *
 * Results:
 *	See user documentation.
 *
 * Side effects:
 *	A dialog window is created the first time this function is called.
 *	This window is not destroyed and will be reused the next time the
 *	application invokes the "tk_chooseColor" command.
 *
 *-------------------------------------------------------------------------
 */

int
Tk_ChooseColorObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tk_Window tkwin = (Tk_Window) clientData, parent;
    HWND hWnd;
    int i, oldMode, winCode, result;
    CHOOSECOLORW chooseColor;
    static int inited = 0;
    static COLORREF dwCustColors[16];
    static long oldColor;		/* the color selected last time */
    static const char *optionStrings[] = {
	"-initialcolor", "-parent", "-title", NULL
    };
    enum options {
	COLOR_INITIAL, COLOR_PARENT, COLOR_TITLE
    };

    result = TCL_OK;
    if (inited == 0) {
	/*
	 * dwCustColors stores the custom color which the user can modify. We
	 * store these colors in a static array so that the next time the
	 * color dialog pops up, the same set of custom colors remain in the
	 * dialog.
	 */

	for (i = 0; i < 16; i++) {
	    dwCustColors[i] = RGB(255-i * 10, i, i * 10);
	}
	oldColor = RGB(0xa0, 0xa0, 0xa0);
	inited = 1;
    }

    parent			= tkwin;
    chooseColor.lStructSize	= sizeof(CHOOSECOLORW);
    chooseColor.hwndOwner	= NULL;
    chooseColor.hInstance	= NULL;
    chooseColor.rgbResult	= oldColor;
    chooseColor.lpCustColors	= dwCustColors;
    chooseColor.Flags		= CC_RGBINIT | CC_FULLOPEN | CC_ENABLEHOOK;
    chooseColor.lCustData	= (LPARAM) NULL;
    chooseColor.lpfnHook	= (LPOFNHOOKPROC) ColorDlgHookProc;
    chooseColor.lpTemplateName	= (LPWSTR) interp;

    for (i = 1; i < objc; i += 2) {
	int index;
	const char *string;
	Tcl_Obj *optionPtr, *valuePtr;

	optionPtr = objv[i];
	valuePtr = objv[i + 1];

	if (Tcl_GetIndexFromObj(interp, optionPtr, optionStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (i + 1 == objc) {
	    string = Tcl_GetString(optionPtr);
	    Tcl_AppendResult(interp, "value for \"", string, "\" missing",
		    NULL);
	    return TCL_ERROR;
	}

	string = Tcl_GetString(valuePtr);
	switch ((enum options) index) {
	case COLOR_INITIAL: {
	    XColor *colorPtr;

	    colorPtr = Tk_GetColor(interp, tkwin, string);
	    if (colorPtr == NULL) {
		return TCL_ERROR;
	    }
	    chooseColor.rgbResult = RGB(colorPtr->red / 0x100,
		    colorPtr->green / 0x100, colorPtr->blue / 0x100);
	    break;
	}
	case COLOR_PARENT:
	    parent = Tk_NameToWindow(interp, string, tkwin);
	    if (parent == NULL) {
		return TCL_ERROR;
	    }
	    break;
	case COLOR_TITLE:
	    chooseColor.lCustData = (LPARAM) string;
	    break;
	}
    }

    Tk_MakeWindowExist(parent);
    chooseColor.hwndOwner = NULL;
    hWnd = Tk_GetHWND(Tk_WindowId(parent));
    chooseColor.hwndOwner = hWnd;

    oldMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
    winCode = ChooseColorW(&chooseColor);
    (void) Tcl_SetServiceMode(oldMode);

    /*
     * Ensure that hWnd is enabled, because it can happen that we have updated
     * the wrapper of the parent, which causes us to leave this child disabled
     * (Windows loses sync).
     */

    EnableWindow(hWnd, 1);

    /*
     * Clear the interp result since anything may have happened during the
     * modal loop.
     */

    Tcl_ResetResult(interp);

    /*
     * 3. Process the result of the dialog
     */

    if (winCode) {
	/*
	 * User has selected a color
	 */
	char color[100];

	sprintf(color, "#%02x%02x%02x",
		GetRValue(chooseColor.rgbResult),
		GetGValue(chooseColor.rgbResult),
		GetBValue(chooseColor.rgbResult));
	Tcl_AppendResult(interp, color, NULL);
	oldColor = chooseColor.rgbResult;
	result = TCL_OK;
    }

    return result;
}

/*
 *-------------------------------------------------------------------------
 *
 * ColorDlgHookProc --
 *
 *	Provides special handling of messages for the Color common dialog box.
 *	Used to set the title when the dialog first appears.
 *
 * Results:
 *	The return value is 0 if the default dialog box function should handle
 *	the message, non-zero otherwise.
 *
 * Side effects:
 *	Changes the title of the dialog window.
 *
 *----------------------------------------------------------------------
 */

static UINT CALLBACK
ColorDlgHookProc(
    HWND hDlg,			/* Handle to the color dialog. */
    UINT uMsg,			/* Type of message. */
    WPARAM wParam,		/* First message parameter. */
    LPARAM lParam)		/* Second message parameter. */
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    const char *title;
    CHOOSECOLORW *ccPtr;

    if (WM_INITDIALOG == uMsg) {

	/*
	 * Set the title string of the dialog.
	 */

	ccPtr = (CHOOSECOLORW *) lParam;
	title = (const char *) ccPtr->lCustData;

	if ((title != NULL) && (title[0] != '\0')) {
	    Tcl_DString ds;

	    SetWindowTextW(hDlg, (WCHAR *)Tcl_WinUtfToTChar(title,-1,&ds));
	    Tcl_DStringFree(&ds);
	}
	if (tsdPtr->debugFlag) {
	    tsdPtr->debugInterp = (Tcl_Interp *) ccPtr->lpTemplateName;
	    Tcl_DoWhenIdle(SetTkDialog, (ClientData) hDlg);
	}
	return TRUE;
    }
    return FALSE;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetOpenFileCmd --
 *
 *	This function implements the "open file" dialog box for the Windows
 *	platform. See the user documentation for details on what it does.
 *
 * Results:
 *	See user documentation.
 *
 * Side effects:
 *	A dialog window is created the first this function is called.
 *
 *----------------------------------------------------------------------
 */

int
Tk_GetOpenFileObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    return GetFileName(clientData, interp, objc, objv, 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetSaveFileCmd --
 *
 *	Same as Tk_GetOpenFileCmd but opens a "save file" dialog box
 *	instead
 *
 * Results:
 *	Same as Tk_GetOpenFileCmd.
 *
 * Side effects:
 *	Same as Tk_GetOpenFileCmd.
 *
 *----------------------------------------------------------------------
 */

int
Tk_GetSaveFileObjCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    return GetFileName(clientData, interp, objc, objv, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * GetFileName --
 *
 *	Calls GetOpenFileName() or GetSaveFileName().
 *
 * Results:
 *	See user documentation.
 *
 * Side effects:
 *	See user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
GetFileName(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[],	/* Argument objects. */
    int open)			/* 1 to call GetOpenFileName(), 0 to call
				 * GetSaveFileName(). */
{
    OPENFILENAMEW ofn;
    WCHAR file[TK_MULTI_MAX_PATH];
    OFNData ofnData;
    int cdlgerr;
    int filterIndex = 0, result = TCL_ERROR, winCode, oldMode, i, multi = 0;
    int confirmOverwrite = 1;
    const char *extension = NULL, *title = NULL;
    Tk_Window tkwin = (Tk_Window) clientData;
    HWND hWnd;
    Tcl_Obj *filterObj = NULL, *initialTypeObj = NULL, *typeVariableObj = NULL;
    Tcl_DString utfFilterString, utfDirString, ds;
    Tcl_DString extString, filterString, dirString, titleString;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    enum options {
	FILE_DEFAULT, FILE_TYPES, FILE_INITDIR, FILE_INITFILE, FILE_PARENT,
	FILE_TITLE, FILE_TYPEVARIABLE, FILE_MULTIPLE, FILE_CONFIRMOW
    };
    struct Options {
	const char *name;
	enum options value;
    };
    static const struct Options saveOptions[] = {
	{"-confirmoverwrite",	FILE_CONFIRMOW},
	{"-defaultextension",	FILE_DEFAULT},
	{"-filetypes",		FILE_TYPES},
	{"-initialdir",		FILE_INITDIR},
	{"-initialfile",	FILE_INITFILE},
	{"-parent",		FILE_PARENT},
	{"-title",		FILE_TITLE},
	{"-typevariable",	FILE_TYPEVARIABLE},
	{NULL,			FILE_DEFAULT/*ignored*/ }
    };
    static const struct Options openOptions[] = {
	{"-defaultextension",	FILE_DEFAULT},
	{"-filetypes",		FILE_TYPES},
	{"-initialdir",		FILE_INITDIR},
	{"-initialfile",	FILE_INITFILE},
	{"-multiple",		FILE_MULTIPLE},
	{"-parent",		FILE_PARENT},
	{"-title",		FILE_TITLE},
	{"-typevariable",	FILE_TYPEVARIABLE},
	{NULL,			FILE_DEFAULT/*ignored*/ }
    };
    const struct Options *options = open ? openOptions : saveOptions;

    file[0] = '\0';
    ZeroMemory(&ofnData, sizeof(OFNData));
    Tcl_DStringInit(&utfFilterString);
    Tcl_DStringInit(&utfDirString);

    /*
     * Parse the arguments.
     */

    for (i = 1; i < objc; i += 2) {
	int index;
	const char *string;
	Tcl_Obj *valuePtr = objv[i + 1];

	if (Tcl_GetIndexFromObjStruct(interp, objv[i], options,
		sizeof(struct Options), "option", 0, &index) != TCL_OK) {
	    goto end;
	} else if (i + 1 == objc) {
	    Tcl_AppendResult(interp, "value for \"", options[index].name,
		    "\" missing", NULL);
	    goto end;
	}

	string = Tcl_GetString(valuePtr);
	switch (options[index].value) {
	case FILE_DEFAULT:
	    if (string[0] == '.') {
		string++;
	    }
	    extension = string;
	    break;
	case FILE_TYPES:
	    filterObj = valuePtr;
	    break;
	case FILE_INITDIR:
	    Tcl_DStringFree(&utfDirString);
	    if (Tcl_TranslateFileName(interp, string,
		    &utfDirString) == NULL) {
		goto end;
	    }
	    break;
	case FILE_INITFILE:
	    if (Tcl_TranslateFileName(interp, string, &ds) == NULL) {
		goto end;
	    }
	    Tcl_UtfToExternal(NULL, TkWinGetUnicodeEncoding(),
		    Tcl_DStringValue(&ds), Tcl_DStringLength(&ds), 0, NULL,
		    (char *) file, sizeof(file), NULL, NULL, NULL);
	    Tcl_DStringFree(&ds);
	    break;
	case FILE_PARENT:
	    tkwin = Tk_NameToWindow(interp, string, tkwin);
	    if (tkwin == NULL) {
		goto end;
	    }
	    break;
	case FILE_TITLE:
	    title = string;
	    break;
	case FILE_TYPEVARIABLE:
	    typeVariableObj = valuePtr;
	    initialTypeObj = Tcl_ObjGetVar2(interp, typeVariableObj, NULL,
		    TCL_GLOBAL_ONLY);
	    break;
	case FILE_MULTIPLE:
	    if (Tcl_GetBooleanFromObj(interp, valuePtr, &multi) != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;
	case FILE_CONFIRMOW:
	    if (Tcl_GetBooleanFromObj(interp, valuePtr,
		    &confirmOverwrite) != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;
	}
    }

    if (MakeFilter(interp, filterObj, &utfFilterString, initialTypeObj,
	    &filterIndex) != TCL_OK) {
	goto end;
    }

    Tk_MakeWindowExist(tkwin);
    hWnd = Tk_GetHWND(Tk_WindowId(tkwin));

    ZeroMemory(&ofn, sizeof(OPENFILENAMEW));
    if (LOBYTE(LOWORD(GetVersion())) < 5) {
	ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
    } else {
	ofn.lStructSize = sizeof(OPENFILENAMEW);
    }
    ofn.hwndOwner = hWnd;
    ofn.hInstance = TkWinGetHInstance(ofn.hwndOwner);
    ofn.lpstrFile = file;
    ofn.nMaxFile = TK_MULTI_MAX_PATH;
    ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR
	    | OFN_EXPLORER | OFN_ENABLEHOOK| OFN_ENABLESIZING;
    ofn.lpfnHook = (LPOFNHOOKPROC) OFNHookProc;
    ofn.lCustData = (LPARAM) &ofnData;

    if (open != 0) {
	ofn.Flags |= OFN_FILEMUSTEXIST;
    } else if (confirmOverwrite) {
	ofn.Flags |= OFN_OVERWRITEPROMPT;
    }
    if (tsdPtr->debugFlag != 0) {
	ofnData.interp = interp;
    }
    if (multi != 0) {
	ofn.Flags |= OFN_ALLOWMULTISELECT;

	/*
	 * Starting buffer size. The buffer will be expanded by the OFN dialog
	 * procedure when necessary
	 */

	ofnData.dynFileBufferSize = 512;
	ofnData.dynFileBuffer = (WCHAR *)ckalloc(512 * sizeof(WCHAR));
    }

    if (extension != NULL) {
	Tcl_WinUtfToTChar(extension, -1, &extString);
	ofn.lpstrDefExt = (WCHAR *) Tcl_DStringValue(&extString);
    }

    Tcl_WinUtfToTChar(Tcl_DStringValue(&utfFilterString),
	    Tcl_DStringLength(&utfFilterString), &filterString);
    ofn.lpstrFilter = (WCHAR *) Tcl_DStringValue(&filterString);
    ofn.nFilterIndex = filterIndex;

    if (Tcl_DStringValue(&utfDirString)[0] != '\0') {
	Tcl_WinUtfToTChar(Tcl_DStringValue(&utfDirString),
		Tcl_DStringLength(&utfDirString), &dirString);
    } else {
	/*
	 * NT 5.0 changed the meaning of lpstrInitialDir, so we have to ensure
	 * that we set the [pwd] if the user didn't specify anything else.
	 */

	Tcl_DString cwd;

	Tcl_DStringFree(&utfDirString);
	if ((Tcl_GetCwd(interp, &utfDirString) == NULL) ||
		(Tcl_TranslateFileName(interp,
			Tcl_DStringValue(&utfDirString), &cwd) == NULL)) {
	    Tcl_ResetResult(interp);
	} else {
	    Tcl_WinUtfToTChar(Tcl_DStringValue(&cwd),
		    Tcl_DStringLength(&cwd), &dirString);
	}
	Tcl_DStringFree(&cwd);
    }
    ofn.lpstrInitialDir = (WCHAR *) Tcl_DStringValue(&dirString);

    if (title != NULL) {
	Tcl_WinUtfToTChar(title, -1, &titleString);
	ofn.lpstrTitle = (WCHAR *) Tcl_DStringValue(&titleString);
    }

    /*
     * Popup the dialog.
     */

    oldMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
    if (open != 0) {
	winCode = GetOpenFileNameW(&ofn);
    } else {
	winCode = GetSaveFileNameW(&ofn);
    }
    Tcl_SetServiceMode(oldMode);
    EatSpuriousMessageBugFix();

    /*
     * Ensure that hWnd is enabled, because it can happen that we have updated
     * the wrapper of the parent, which causes us to leave this child disabled
     * (Windows loses sync).
     */

    EnableWindow(hWnd, 1);

    /*
     * Clear the interp result since anything may have happened during the
     * modal loop.
     */

    Tcl_ResetResult(interp);

    /*
     * Process the results.
     *
     * Use the CommDlgExtendedError() function to retrieve the error code.
     * This function can return one of about two dozen codes; most of these
     * indicate some sort of gross system failure (insufficient memory, bad
     * window handles, etc.). Most of the error codes will be ignored; as we
     * find we want more specific error messages for particular errors, we can
     * extend the code as needed.
     */

    cdlgerr = CommDlgExtendedError();

    /*
     * We now allow FNERR_BUFFERTOOSMALL when multiselection is enabled. The
     * filename buffer has been dynamically allocated by the OFN dialog
     * procedure to accomodate all selected files.
     */

    if ((winCode != 0)
	    || ((cdlgerr == FNERR_BUFFERTOOSMALL)
		    && (ofn.Flags & OFN_ALLOWMULTISELECT))) {
	int gotFilename = 0;	/* Flag for tracking whether we have any
				 * filename at all. For details, see
				 * http://stackoverflow.com/q/9227859/301832
				 */

	if (ofn.Flags & OFN_ALLOWMULTISELECT) {
	    /*
	     * The result in dynFileBuffer contains many items, separated by
	     * NUL characters. It is terminated with two nulls in a row. The
	     * first element is the directory path.
	     */

	    WCHAR *files = ofnData.dynFileBuffer;
	    Tcl_Obj *returnList = Tcl_NewObj();
	    int count = 0;

	    /*
	     * Get directory.
	     */

	    ConvertExternalFilename(files, &ds);

	    while (*files != '\0') {
		while (*files != '\0') {
		    files++;
		}
		files++;
		if (*files != '\0') {
		    Tcl_Obj *fullnameObj;
		    Tcl_DString filenameBuf;

		    count++;
		    ConvertExternalFilename(files, &filenameBuf);

		    fullnameObj = Tcl_NewStringObj(Tcl_DStringValue(&ds),
			    Tcl_DStringLength(&ds));
		    Tcl_AppendToObj(fullnameObj, "/", -1);
		    Tcl_AppendToObj(fullnameObj, Tcl_DStringValue(&filenameBuf),
			    Tcl_DStringLength(&filenameBuf));
		    gotFilename = 1;
		    Tcl_DStringFree(&filenameBuf);
		    Tcl_ListObjAppendElement(NULL, returnList, fullnameObj);
		}
	    }

	    if (count == 0) {
		/*
		 * Only one file was returned.
		 */

		Tcl_ListObjAppendElement(NULL, returnList,
			Tcl_NewStringObj(Tcl_DStringValue(&ds),
				Tcl_DStringLength(&ds)));
		gotFilename |= (Tcl_DStringLength(&ds) > 0);
	    }
	    Tcl_SetObjResult(interp, returnList);
	    Tcl_DStringFree(&ds);
	} else {
	    Tcl_AppendResult(interp, ConvertExternalFilename(
		    ofn.lpstrFile, &ds), NULL);
	    gotFilename = (Tcl_DStringLength(&ds) > 0);
	    Tcl_DStringFree(&ds);
	}
	result = TCL_OK;
	if ((ofn.nFilterIndex > 0) && gotFilename && typeVariableObj
		&& filterObj) {
	    int listObjc, count;
	    Tcl_Obj **listObjv = NULL;
	    Tcl_Obj **typeInfo = NULL;

	    if (Tcl_ListObjGetElements(interp, filterObj,
		    &listObjc, &listObjv) != TCL_OK) {
		result = TCL_ERROR;
	    } else if (Tcl_ListObjGetElements(interp,
		    listObjv[ofn.nFilterIndex - 1], &count,
		    &typeInfo) != TCL_OK) {
		result = TCL_ERROR;
	    } else if (Tcl_ObjSetVar2(interp, typeVariableObj, NULL,
		    typeInfo[0], TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	}
    } else if (cdlgerr == FNERR_INVALIDFILENAME) {
	Tcl_SetResult(interp, "invalid filename \"", TCL_STATIC);
	Tcl_AppendResult(interp, ConvertExternalFilename(
		ofn.lpstrFile, &ds), "\"", NULL);
	Tcl_DStringFree(&ds);
    } else {
	result = TCL_OK;
    }

    if (ofn.lpstrTitle != NULL) {
	Tcl_DStringFree(&titleString);
    }
    if (ofn.lpstrInitialDir != NULL) {
	Tcl_DStringFree(&dirString);
    }
    Tcl_DStringFree(&filterString);
    if (ofn.lpstrDefExt != NULL) {
	Tcl_DStringFree(&extString);
    }

  end:
    Tcl_DStringFree(&utfDirString);
    Tcl_DStringFree(&utfFilterString);
    if (ofnData.dynFileBuffer != NULL) {
	ckfree((char *)ofnData.dynFileBuffer);
	ofnData.dynFileBuffer = NULL;
    }

    return result;
}

/*
 *-------------------------------------------------------------------------
 *
 * OFNHookProc --
 *
 *	Dialog box hook function. This is used to sets the "tk_dialog"
 *	variable for test/debugging when the dialog is ready to receive
 *	messages. When multiple file selection is enabled this function
 *	is used to process the list of names.
 *
 * Results:
 *	Returns 0 to allow default processing of messages to occur.
 *
 * Side effects:
 *	None.
 *
 *-------------------------------------------------------------------------
 */

static UINT APIENTRY
OFNHookProc(
    HWND hdlg,			/* Handle to child dialog window. */
    UINT uMsg,			/* Message identifier */
    WPARAM wParam,		/* Message parameter */
    LPARAM lParam)		/* Message parameter */
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    OPENFILENAMEW *ofnPtr;
    OFNData *ofnData;

    if (uMsg == WM_INITDIALOG) {
	TkWinSetUserData(hdlg, lParam);
    } else if (uMsg == WM_NOTIFY) {
	OFNOTIFYW *notifyPtr = (OFNOTIFYW *) lParam;

	/*
	 * This is weird... or not. The CDN_FILEOK is NOT sent when the
	 * selection exceeds declared buffer size (the nMaxFile member of the
	 * OPENFILENAME struct passed to GetOpenFileName function). So, we
	 * have to rely on the most recent CDN_SELCHANGE then. Unfortunately
	 * this means, that gathering the selected filenames happens twice
	 * when they fit into the declared buffer. Luckily, it's not frequent
	 * operation so it should not incur any noticeable delay. See [Bug
	 * 2987995]
	 */

	if (notifyPtr->hdr.code == CDN_FILEOK ||
		notifyPtr->hdr.code == CDN_SELCHANGE) {
	    int dirsize, selsize;
	    WCHAR *buffer;
	    int buffersize;

	    /*
	     * Change of selection. Unscramble the unholy mess that's in the
	     * selection buffer, resizing it if necessary.
	     */

	    ofnPtr = notifyPtr->lpOFN;
	    ofnData = (OFNData *) ofnPtr->lCustData;
	    buffer = ofnData->dynFileBuffer;
	    hdlg = GetParent(hdlg);

	    selsize = SendMessageW(hdlg, CDM_GETSPEC, 0, 0);
	    dirsize = SendMessageW(hdlg, CDM_GETFOLDERPATH, 0, 0);
	    buffersize = (selsize + dirsize + 1);

	    /*
	     * Just empty the buffer if dirsize indicates an error. [Bug
	     * 3071836]
	     */

	    if ((selsize > 1) && (dirsize > 0)) {
		if (ofnData->dynFileBufferSize < buffersize) {
		    buffer = (WCHAR *) ckrealloc((char *) buffer, buffersize * sizeof(WCHAR));
		    ofnData->dynFileBufferSize = buffersize;
		    ofnData->dynFileBuffer = buffer;
		}

		SendMessageW(hdlg, CDM_GETFOLDERPATH, dirsize, (LPARAM) buffer);
		buffer += dirsize;

		SendMessageW(hdlg, CDM_GETSPEC, selsize, (LPARAM) buffer);

		/*
		 * If there are multiple files, delete the quotes and change
		 * every second quote to NULL terminator
		 */

		if (buffer[0] == '"') {
		    BOOL findquote = TRUE;
		    WCHAR *tmp = buffer;

		    while (*buffer != '\0') {
			if (findquote) {
			    if (*buffer == '"') {
				findquote = FALSE;
			    }
			    buffer++;
			} else {
			    if (*buffer == '"') {
				findquote = TRUE;
				*buffer = '\0';
			    }
			    *tmp++ = *buffer++;
			}
		    }
		    *tmp = '\0';		/* Second NULL terminator. */
		} else {

			/*
		     * Replace directory terminating NULL with a with a backslash,
		     * but only if not an absolute path.
		     */

		    Tcl_DString tmpfile;
		    ConvertExternalFilename(buffer, &tmpfile);
		    if (TCL_PATH_ABSOLUTE ==
			    Tcl_GetPathType(Tcl_DStringValue(&tmpfile))) {
			/* re-get the full path to the start of the buffer */
			buffer = (WCHAR *) ofnData->dynFileBuffer;
			SendMessageW(hdlg, CDM_GETSPEC, selsize, (LPARAM) buffer);
		    } else {
			*(buffer-1) = '\\';
		    }
		    buffer[selsize] = '\0'; /* Second NULL terminator. */
		    Tcl_DStringFree(&tmpfile);
		}
	    } else {
		/*
		 * Nothing is selected, so just empty the string.
		 */

		if (buffer != NULL) {
		    *buffer = '\0';
		}
	    }
	}
    } else if (uMsg == WM_WINDOWPOSCHANGED) {
	/*
	 * This message is delivered at the right time to enable Tk to set the
	 * debug information. Unhooks itself so it won't set the debug
	 * information every time it gets a WM_WINDOWPOSCHANGED message.
	 */

	ofnPtr = (OPENFILENAMEW *) TkWinGetUserData(hdlg);
	if (ofnPtr != NULL) {
	    ofnData = (OFNData *) ofnPtr->lCustData;
	    if (ofnData->interp != NULL) {
		hdlg = GetParent(hdlg);
		tsdPtr->debugInterp = ofnData->interp;
		Tcl_DoWhenIdle(SetTkDialog, hdlg);
	    }
	    TkWinSetUserData(hdlg, NULL);
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeFilter --
 *
 *	Allocate a buffer to store the filters in a format understood by
 *	Windows.
 *
 * Results:
 *	A standard TCL return value.
 *
 * Side effects:
 *	ofnPtr->lpstrFilter is modified.
 *
 *----------------------------------------------------------------------
 */

static int
MakeFilter(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Obj *valuePtr,		/* Value of the -filetypes option */
    Tcl_DString *dsPtr,		/* Filled with windows filter string. */
    Tcl_Obj *initialPtr,	/* Initial type name  */
    int *indexPtr)		/* Index of initial type in filter string */
{
    char *filterStr;
    char *p;
    const char *initial = NULL;
    int pass;
    int ix = 0; /* index counter */
    FileFilterList flist;
    FileFilter *filterPtr;

    if (initialPtr) {
	initial = Tcl_GetString(initialPtr);
    }
    TkInitFileFilters(&flist);
    if (TkGetFileFilters(interp, &flist, valuePtr, 1) != TCL_OK) {
	return TCL_ERROR;
    }

    if (flist.filters == NULL) {
	/*
	 * Use "All Files (*.*) as the default filter if none is specified
	 */
	const char *defaultFilter = "All Files (*.*)";

	p = filterStr = ckalloc(30);

	strcpy(p, defaultFilter);
	p+= strlen(defaultFilter);

	*p++ = '\0';
	*p++ = '*';
	*p++ = '.';
	*p++ = '*';
	*p++ = '\0';
	*p++ = '\0';
	*p = '\0';

    } else {
	int len;

	if (valuePtr == NULL) {
	    len = 0;
	} else {
	    (void) Tcl_GetStringFromObj(valuePtr, &len);
	}

	/*
	 * We format the filetype into a string understood by Windows: {"Text
	 * Documents" {.doc .txt} {TEXT}} becomes "Text Documents
	 * (*.doc,*.txt)\0*.doc;*.txt\0"
	 *
	 * See the Windows OPENFILENAME manual page for details on the filter
	 * string format.
	 */

	/*
	 * Since we may only add asterisks (*) to the filter, we need at most
	 * twice the size of the string to format the filter
	 */

	filterStr = ckalloc((unsigned int) len * 3);

	for (filterPtr = flist.filters, p = filterStr; filterPtr;
		filterPtr = filterPtr->next) {
	    const char *sep;
	    FileFilterClause *clausePtr;

	    /*
	     * Check initial index for match, set *indexPtr. Filter index is 1
	     * based so increment first
	     */

	    ix++;
	    if (indexPtr && initial
		    && (strcmp(initial, filterPtr->name) == 0)) {
		*indexPtr = ix;
	    }

	    /*
	     * First, put in the name of the file type.
	     */

	    strcpy(p, filterPtr->name);
	    p+= strlen(filterPtr->name);
	    *p++ = ' ';
	    *p++ = '(';

	    for (pass = 1; pass <= 2; pass++) {
		/*
		 * In the first pass, we format the extensions in the name
		 * field. In the second pass, we format the extensions in the
		 * filter pattern field
		 */

		sep = "";
		for (clausePtr=filterPtr->clauses;clausePtr;
			clausePtr=clausePtr->next) {
		    GlobPattern *globPtr;

		    for (globPtr = clausePtr->patterns; globPtr;
			    globPtr = globPtr->next) {
			strcpy(p, sep);
			p += strlen(sep);
			strcpy(p, globPtr->pattern);
			p += strlen(globPtr->pattern);

			if (pass == 1) {
			    sep = ",";
			} else {
			    sep = ";";
			}
		    }
		}
		if (pass == 1) {
		    *p ++ = ')';
		}
		*p++ = '\0';
	    }
	}

	/*
	 * Windows requires the filter string to be ended by two NULL
	 * characters.
	 */

	*p++ = '\0';
	*p = '\0';
    }

    Tcl_DStringAppend(dsPtr, filterStr, (int) (p - filterStr));
    ckfree((char *) filterStr);

    TkFreeFileFilters(&flist);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ChooseDirectoryObjCmd --
 *
 *	This function implements the "tk_chooseDirectory" dialog box for the
 *	Windows platform. See the user documentation for details on what it
 *	does. Uses the newer SHBrowseForFolder explorer type interface.
 *
 * Results:
 *	See user documentation.
 *
 * Side effects:
 *	A modal dialog window is created. Tcl_SetServiceMode() is called to
 *	allow background events to be processed
 *
 *----------------------------------------------------------------------
 *
 * The function tk_chooseDirectory pops up a dialog box for the user to select
 * a directory. The following option-value pairs are possible as command line
 * arguments:
 *
 * -initialdir dirname
 *
 * Specifies that the directories in directory should be displayed when the
 * dialog pops up. If this parameter is not specified, then the directories in
 * the current working directory are displayed. If the parameter specifies a
 * relative path, the return value will convert the relative path to an
 * absolute path. This option may not always work on the Macintosh. This is
 * not a bug. Rather, the General Controls control panel on the Mac allows the
 * end user to override the application default directory.
 *
 * -parent window
 *
 * Makes window the logical parent of the dialog. The dialog is displayed on
 * top of its parent window.
 *
 * -title titleString
 *
 * Specifies a string to display as the title of the dialog box. If this
 * option is not specified, then a default title will be displayed.
 *
 * -mustexist boolean
 *
 * Specifies whether the user may specify non-existant directories. If this
 * parameter is true, then the user may only select directories that already
 * exist. The default value is false.
 *
 * New Behaviour:
 *
 * - If mustexist = 0 and a user entered folder does not exist, a prompt will
 *   pop-up asking if the user wants another chance to change it. The old
 *   dialog just returned the bogus entry. On mustexist = 1, the entries MUST
 *   exist before exiting the box with OK.
 *
 *   Bugs:
 *
 * - If valid abs directory name is entered into the entry box and Enter
 *   pressed, the box will close returning the name. This is inconsistent when
 *   entering relative names or names with forward slashes, which are
 *   invalidated then corrected in the callback. After correction, the box is
 *   held open to allow further modification by the user.
 *
 * - Not sure how to implement localization of message prompts.
 *
 * - -title is really -message.
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
    WCHAR path[MAX_PATH];
    int oldMode, result = TCL_ERROR, i;
    LPCITEMIDLIST pidl;		/* Returned by browser */
    BROWSEINFOW bInfo;		/* Used by browser */
    ChooseDir cdCBData;	    /* Structure to pass back and forth */
    LPMALLOC pMalloc;		/* Used by shell */
    Tk_Window tkwin = (Tk_Window) clientData;
    HWND hWnd;
    const char *utfTitle = NULL;/* Title for window */
    WCHAR saveDir[MAX_PATH];
    Tcl_DString titleString;	/* Title */
    Tcl_DString initDirString;	/* Initial directory */
    Tcl_DString tempString;	/* temporary */
    Tcl_Obj *objPtr;
    static const char *optionStrings[] = {
	"-initialdir", "-mustexist",  "-parent",  "-title", NULL
    };
    enum options {
	DIR_INITIAL,   DIR_EXIST,  DIR_PARENT, FILE_TITLE
    };

    /*
     * Initialize
     */

    path[0] = '\0';
    ZeroMemory(&cdCBData, sizeof(ChooseDir));
    cdCBData.interp = interp;

    /*
     * Process the command line options
     */

    for (i = 1; i < objc; i += 2) {
	int index;
	const char *string;
	const WCHAR *uniStr;
	Tcl_Obj *optionPtr, *valuePtr;

	optionPtr = objv[i];
	valuePtr = objv[i + 1];

	if (Tcl_GetIndexFromObj(interp, optionPtr, optionStrings, "option", 0,
		&index) != TCL_OK) {
	    goto cleanup;
	}
	if (i + 1 == objc) {
	    string = Tcl_GetString(optionPtr);
	    Tcl_AppendResult(interp, "value for \"", string, "\" missing",
		    NULL);
	    goto cleanup;
	}

	string = Tcl_GetString(valuePtr);
	switch ((enum options) index) {
	case DIR_INITIAL:
	    if (Tcl_TranslateFileName(interp,string,&initDirString) == NULL) {
		goto cleanup;
	    }
	    Tcl_WinUtfToTChar(Tcl_DStringValue(&initDirString), -1,
		    &tempString);
	    uniStr = (WCHAR *) Tcl_DStringValue(&tempString);

	    /*
	     * Convert possible relative path to full path to keep dialog
	     * happy.
	     */

	    GetFullPathNameW(uniStr, MAX_PATH, saveDir, NULL);
	    wcsncpy(cdCBData.initDir, saveDir, MAX_PATH);
	    Tcl_DStringFree(&initDirString);
	    Tcl_DStringFree(&tempString);
	    break;
	case DIR_EXIST:
	    if (Tcl_GetBooleanFromObj(interp, valuePtr,
		    &cdCBData.mustExist) != TCL_OK) {
		goto cleanup;
	    }
	    break;
	case DIR_PARENT:
	    tkwin = Tk_NameToWindow(interp, string, tkwin);
	    if (tkwin == NULL) {
		goto cleanup;
	    }
	    break;
	case FILE_TITLE:
	    utfTitle = string;
	    break;
	}
    }

    /*
     * Get ready to call the browser
     */

    Tk_MakeWindowExist(tkwin);
    hWnd = Tk_GetHWND(Tk_WindowId(tkwin));

    /*
     * Setup the parameters used by SHBrowseForFolder
     */

    bInfo.hwndOwner = hWnd;
    bInfo.pszDisplayName = path;
    bInfo.pidlRoot = NULL;
    if (wcslen(cdCBData.initDir) == 0) {
	GetCurrentDirectoryW(MAX_PATH, cdCBData.initDir);
    }
    bInfo.lParam = (LPARAM) &cdCBData;

    if (utfTitle != NULL) {
	Tcl_WinUtfToTChar(utfTitle, -1, &titleString);
	bInfo.lpszTitle = (LPWSTR) Tcl_DStringValue(&titleString);
    } else {
	bInfo.lpszTitle = L"Please choose a directory, then select OK.";
    }

    /*
     * Set flags to add edit box, status text line and use the new ui. Allow
     * override with magic variable (ignore errors in retrieval). See
     * http://msdn.microsoft.com/en-us/library/bb773205(VS.85).aspx for
     * possible flag values.
     */

    bInfo.ulFlags = BIF_EDITBOX | BIF_STATUSTEXT | BIF_RETURNFSANCESTORS
	| BIF_VALIDATE | BIF_NEWDIALOGSTYLE;
    objPtr = Tcl_GetVar2Ex(interp, "::tk::winChooseDirFlags", NULL,
	    TCL_GLOBAL_ONLY);
    if (objPtr != NULL) {
	int flags;
	Tcl_GetIntFromObj(NULL, objPtr, &flags);
	bInfo.ulFlags = flags;
    }

    /*
     * Callback to handle events
     */

    bInfo.lpfn = (BFFCALLBACK) ChooseDirectoryValidateProc;

    /*
     * Display dialog in background and process result. We look to give the
     * user a chance to change their mind on an invalid folder if mustexist is
     * 0.
     */

    oldMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
    GetCurrentDirectoryW(MAX_PATH, saveDir);
    if (SHGetMalloc(&pMalloc) == NOERROR) {
	pidl = SHBrowseForFolderW(&bInfo);

	/*
	 * This is a fix for Windows 2000, which seems to modify the folder
	 * name buffer even when the dialog is canceled (in this case the
	 * buffer contains garbage). See [Bug #3002230]
	 */

	path[0] = '\0';

	/*
	 * Null for cancel button or invalid dir, otherwise valid.
	 */

	if (pidl != NULL) {
	    if (!SHGetPathFromIDListW(pidl, path)) {
		Tcl_SetResult(interp, "Error: Not a file system folder\n",
			TCL_VOLATILE);
	    }
	    pMalloc->lpVtbl->Free(pMalloc, (void *) pidl);
	} else if (wcslen(cdCBData.retDir) > 0) {
	    wcscpy(path, cdCBData.retDir);
	}
	pMalloc->lpVtbl->Release(pMalloc);
    }
    SetCurrentDirectoryW(saveDir);
    Tcl_SetServiceMode(oldMode);

    /*
     * Ensure that hWnd is enabled, because it can happen that we have updated
     * the wrapper of the parent, which causes us to leave this child disabled
     * (Windows loses sync).
     */

    EnableWindow(hWnd, 1);

    /*
     * Change the pathname to the Tcl "normalized" pathname, where back
     * slashes are used instead of forward slashes
     */

    Tcl_ResetResult(interp);
    if (*path) {
	Tcl_DString ds;

	Tcl_AppendResult(interp, ConvertExternalFilename(path,
		&ds), NULL);
	Tcl_DStringFree(&ds);
    }

    result = TCL_OK;

    if (utfTitle != NULL) {
	Tcl_DStringFree(&titleString);
    }

  cleanup:
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ChooseDirectoryValidateProc --
 *
 *	Hook function called by the explorer ChooseDirectory dialog when
 *	events occur. It is used to validate the text entry the user may have
 *	entered.
 *
 * Results:
 *	Returns 0 to allow default processing of message, or 1 to tell default
 *	dialog function not to close.
 *
 *----------------------------------------------------------------------
 */

static UINT APIENTRY
ChooseDirectoryValidateProc(
    HWND hwnd,
    UINT message,
    LPARAM lParam,
    LPARAM lpData)
{
    WCHAR selDir[MAX_PATH];
    ChooseDir *chooseDirSharedData = (ChooseDir *) lpData;
    Tcl_DString tempString;
    Tcl_DString initDirString;
    WCHAR string[MAX_PATH];
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (tsdPtr->debugFlag) {
	tsdPtr->debugInterp = (Tcl_Interp *) chooseDirSharedData->interp;
	Tcl_DoWhenIdle(SetTkDialog, (ClientData) hwnd);
    }
    chooseDirSharedData->retDir[0] = '\0';
    switch (message) {
    case BFFM_VALIDATEFAILED:
	/*
	 * First save and check to see if it is a valid path name, if so then
	 * make that path the one shown in the window. Otherwise, it failed
	 * the check and should be treated as such. Use
	 * Set/GetCurrentDirectory which allows relative path names and names
	 * with forward slashes. Use Tcl_TranslateFileName to make sure names
	 * like ~ are converted correctly.
	 */

	Tcl_WinTCharToUtf((TCHAR *) lParam, -1, &initDirString);
	if (Tcl_TranslateFileName(chooseDirSharedData->interp,
		Tcl_DStringValue(&initDirString), &tempString) == NULL) {
	    /*
	     * Should we expose the error (in the interp result) to the user
	     * at this point?
	     */

	    chooseDirSharedData->retDir[0] = '\0';
	    return 1;
	}
	Tcl_DStringFree(&initDirString);
	Tcl_WinUtfToTChar(Tcl_DStringValue(&tempString), -1, &initDirString);
	Tcl_DStringFree(&tempString);
	wcsncpy(string, (WCHAR *) Tcl_DStringValue(&initDirString),
		MAX_PATH);
	Tcl_DStringFree(&initDirString);

	if (SetCurrentDirectoryW(string) == 0) {

	    /*
	     * Get the full path name to the user entry, at this point it does
	     * not exist so see if it is supposed to. Otherwise just return
	     * it.
	     */

	    GetFullPathNameW(string, MAX_PATH,
		    chooseDirSharedData->retDir, NULL);
	    if (chooseDirSharedData->mustExist) {
		/*
		 * User HAS to select a valid directory.
		 */

		wsprintfW(selDir, L"Directory '%.200s' does not exist,\nplease select or enter an existing directory.",
			chooseDirSharedData->retDir);
		MessageBoxW(NULL, selDir, NULL, MB_ICONEXCLAMATION|MB_OK);
		chooseDirSharedData->retDir[0] = '\0';
		return 1;
	    }
	} else {
	    /*
	     * Changed to new folder OK, return immediatly with the current
	     * directory in utfRetDir.
	     */

	    GetCurrentDirectoryW(MAX_PATH, chooseDirSharedData->retDir);
	    return 0;
	}
	return 0;

    case BFFM_SELCHANGED:
	/*
	 * Set the status window to the currently selected path and enable the
	 * OK button if a file system folder, otherwise disable the OK button
	 * for things like server names. Perhaps a new switch
	 * -enablenonfolders can be used to allow non folders to be selected.
	 *
	 * Not called when user changes edit box directly.
	 */

	if (SHGetPathFromIDListW((LPITEMIDLIST) lParam, selDir)) {
	    SendMessageW(hwnd, BFFM_SETSTATUSTEXTW, 0, (LPARAM) selDir);
	    // enable the OK button
	    SendMessageW(hwnd, BFFM_ENABLEOK, 0, (LPARAM) 1);
	} else {
	    // disable the OK button
	    SendMessageW(hwnd, BFFM_ENABLEOK, 0, (LPARAM) 0);
	}
	UpdateWindow(hwnd);
	return 1;

    case BFFM_INITIALIZED: {
	/*
	 * Directory browser intializing - tell it where to start from, user
	 * specified parameter.
	 */

	WCHAR *initDir = chooseDirSharedData->initDir;

	SetCurrentDirectoryW(initDir);

	if (*initDir == '\\') {
	    /*
	     * BFFM_SETSELECTION only understands UNC paths as pidls, so
	     * convert path to pidl using IShellFolder interface.
	     */

	    LPMALLOC pMalloc;
	    LPSHELLFOLDER psfFolder;

	    if (SUCCEEDED(SHGetMalloc(&pMalloc))) {
		if (SUCCEEDED(SHGetDesktopFolder(&psfFolder))) {
		    LPITEMIDLIST pidlMain;
		    ULONG ulCount, ulAttr;

		    if (SUCCEEDED(psfFolder->lpVtbl->ParseDisplayName(
			    psfFolder, hwnd, NULL, (WCHAR *)
			    initDir, &ulCount,&pidlMain,&ulAttr))
			    && (pidlMain != NULL)) {
			SendMessageW(hwnd, BFFM_SETSELECTIONW, FALSE,
				(LPARAM) pidlMain);
			pMalloc->lpVtbl->Free(pMalloc, pidlMain);
		    }
		    psfFolder->lpVtbl->Release(psfFolder);
		}
		pMalloc->lpVtbl->Release(pMalloc);
	    }
	} else {
	    SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, (LPARAM) initDir);
	}
	SendMessageW(hwnd, BFFM_ENABLEOK, 0, (LPARAM) 1);
	break;
    }

    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MessageBoxObjCmd --
 *
 *	This function implements the MessageBox window for the Windows
 *	platform. See the user documentation for details on what it does.
 *
 * Results:
 *	See user documentation.
 *
 * Side effects:
 *	None. The MessageBox window will be destroy before this function
 *	returns.
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
    Tk_Window tkwin = (Tk_Window) clientData, parent;
    HWND hWnd;
    Tcl_Obj *messageObj, *titleObj, *detailObj, *tmpObj;
    int defaultBtn, icon, type;
    int i, oldMode, winCode;
    UINT flags;
    static const char *optionStrings[] = {
	"-default",	"-detail",	"-icon",	"-message",
	"-parent",	"-title",	"-type",	NULL
    };
    enum options {
	MSG_DEFAULT,	MSG_DETAIL,	MSG_ICON,	MSG_MESSAGE,
	MSG_PARENT,	MSG_TITLE,	MSG_TYPE
    };
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    defaultBtn = -1;
    detailObj = NULL;
    icon = MB_ICONINFORMATION;
    messageObj = NULL;
    parent = tkwin;
    titleObj = NULL;
    type = MB_OK;

    for (i = 1; i < objc; i += 2) {
	int index;
	Tcl_Obj *optionPtr, *valuePtr;

	optionPtr = objv[i];
	valuePtr = objv[i + 1];

	if (Tcl_GetIndexFromObj(interp, optionPtr, optionStrings, "option",
		TCL_EXACT, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (i + 1 == objc) {
	    const char *string = Tcl_GetString(optionPtr);
	    Tcl_AppendResult(interp, "value for \"", string, "\" missing",
		    NULL);
	    return TCL_ERROR;
	}

	switch ((enum options) index) {
	case MSG_DEFAULT:
	    defaultBtn = TkFindStateNumObj(interp, optionPtr, buttonMap,
		    valuePtr);
	    if (defaultBtn < 0) {
		return TCL_ERROR;
	    }
	    break;

	case MSG_DETAIL:
	    detailObj = valuePtr;
	    break;

	case MSG_ICON:
	    icon = TkFindStateNumObj(interp, optionPtr, iconMap, valuePtr);
	    if (icon < 0) {
		return TCL_ERROR;
	    }
	    break;

	case MSG_MESSAGE:
	    messageObj = valuePtr;
	    break;

	case MSG_PARENT:
	    parent = Tk_NameToWindow(interp, Tcl_GetString(valuePtr), tkwin);
	    if (parent == NULL) {
		return TCL_ERROR;
	    }
	    break;

	case MSG_TITLE:
	    titleObj = valuePtr;
	    break;

	case MSG_TYPE:
	    type = TkFindStateNumObj(interp, optionPtr, typeMap, valuePtr);
	    if (type < 0) {
		return TCL_ERROR;
	    }
	    break;
	}
    }

    while (!Tk_IsTopLevel(parent)) {
	parent = Tk_Parent(parent);
    }
    Tk_MakeWindowExist(parent);
    hWnd = Tk_GetHWND(Tk_WindowId(parent));

    flags = 0;
    if (defaultBtn >= 0) {
	int defaultBtnIdx = -1;

	for (i = 0; i < (int) NUM_TYPES; i++) {
	    if (type == allowedTypes[i].type) {
		int j;

		for (j = 0; j < 3; j++) {
		    if (allowedTypes[i].btnIds[j] == defaultBtn) {
			defaultBtnIdx = j;
			break;
		    }
		}
		if (defaultBtnIdx < 0) {
		    Tcl_AppendResult(interp, "invalid default button \"",
			    TkFindStateString(buttonMap, defaultBtn),
			    "\"", NULL);
		    return TCL_ERROR;
		}
		break;
	    }
	}
	flags = buttonFlagMap[defaultBtnIdx];
    }

    flags |= icon | type | MB_TASKMODAL | MB_SETFOREGROUND;

    tmpObj = messageObj ? Tcl_DuplicateObj(messageObj)
	    : Tcl_NewUnicodeObj(NULL, 0);
    Tcl_IncrRefCount(tmpObj);
    if (detailObj) {
	Tcl_AppendUnicodeToObj(tmpObj, L"\n\n", 2);
	Tcl_AppendObjToObj(tmpObj, detailObj);
    }

    oldMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);

    /*
     * MessageBoxW exists for all platforms. Use it to allow unicode error
     * message to be displayed correctly where possible by the OS.
     *
     * In order to have the parent window icon reflected in a MessageBox, we
     * have to create a hook that will trigger when the MessageBox is being
     * created.
     */

    tsdPtr->hSmallIcon = TkWinGetIcon(parent, ICON_SMALL);
    tsdPtr->hBigIcon   = TkWinGetIcon(parent, ICON_BIG);
    tsdPtr->hMsgBoxHook = SetWindowsHookExW(WH_CBT, MsgBoxCBTProc, NULL,
	    GetCurrentThreadId());
    winCode = MessageBoxW(hWnd, Tcl_GetUnicode(tmpObj),
	    titleObj ? Tcl_GetUnicode(titleObj) : L"", flags);
    UnhookWindowsHookEx(tsdPtr->hMsgBoxHook);
    (void) Tcl_SetServiceMode(oldMode);

    /*
     * Ensure that hWnd is enabled, because it can happen that we have updated
     * the wrapper of the parent, which causes us to leave this child disabled
     * (Windows loses sync).
     */

    EnableWindow(hWnd, 1);

    Tcl_DecrRefCount(tmpObj);

    Tcl_SetResult(interp, TkFindStateString(buttonMap, winCode), TCL_STATIC);
    return TCL_OK;
}

static LRESULT CALLBACK
MsgBoxCBTProc(
    int nCode,
    WPARAM wParam,
    LPARAM lParam)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));

    if (nCode == HCBT_CREATEWND) {
	/*
	 * Window owned by our task is being created. Since the hook is
	 * installed just before the MessageBox call and removed after the
	 * MessageBox call, the window being created is either the message box
	 * or one of its controls. Check that the class is WC_DIALOG to ensure
	 * that it's the one we want.
	 */

	LPCBT_CREATEWND lpcbtcreate = (LPCBT_CREATEWND) lParam;

	if (WC_DIALOG == lpcbtcreate->lpcs->lpszClass) {
	    HWND hwnd = (HWND) wParam;

	    SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
		    (LPARAM) tsdPtr->hSmallIcon);
	    SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM) tsdPtr->hBigIcon);
	}
    }

    /*
     * Call the next hook proc, if there is one
     */

    return CallNextHookEx(tsdPtr->hMsgBoxHook, nCode, wParam, lParam);
}

/*
 * ----------------------------------------------------------------------
 *
 * SetTkDialog --
 *
 *	Records the HWND for a native dialog in the 'tk_dialog' variable so
 *	that the test-suite can operate on the correct dialog window. Use of
 *	this is enabled when a test program calls TkWinDialogDebug by calling
 *	the test command 'tkwinevent debug 1'.
 *
 * ----------------------------------------------------------------------
 */

static void
SetTkDialog(
    ClientData clientData)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)
	    Tcl_GetThreadData(&dataKey, sizeof(ThreadSpecificData));
    char buf[32];

    sprintf(buf, "0x%p", (HWND) clientData);
    Tcl_SetVar(tsdPtr->debugInterp, "tk_dialog", buf, TCL_GLOBAL_ONLY);
}

/*
 * Factored out a common pattern in use in this file.
 */

static const char *
ConvertExternalFilename(
    WCHAR *filename,
    Tcl_DString *dsPtr)
{
    char *p;

    Tcl_WinTCharToUtf((TCHAR *) filename, -1, dsPtr);
    for (p = Tcl_DStringValue(dsPtr); *p != '\0'; p++) {
	/*
	 * Change the pathname to the Tcl "normalized" pathname, where back
	 * slashes are used instead of forward slashes
	 */

	if (*p == '\\') {
	    *p = '/';
	}
    }
    return Tcl_DStringValue(dsPtr);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
