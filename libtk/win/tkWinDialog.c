/*
 * tkWinDialog.c --
 *
 *	Contains the Windows implementation of the common dialog boxes.
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinDialog.c 1.10 97/10/21 11:29:18
 *
 */
 
#include "tkWinInt.h"
#include "tkFileFilter.h"

#include <commdlg.h>    /* includes common dialog functionality */
#include <dlgs.h>       /* includes common dialog template defines */
#include <cderr.h>      /* includes the common dialog error codes */

#if ((TK_MAJOR_VERSION == 4) && (TK_MINOR_VERSION <= 2))
/*
 * The following function is implemented on tk4.3 and after only 
 */
#define Tk_GetHWND TkWinGetHWND
#endif

#define SAVE_FILE 0
#define OPEN_FILE 1

/*----------------------------------------------------------------------
 * MsgTypeInfo --
 *
 *	This structure stores the type of available message box in an
 *	easy-to-process format. Used by th Tk_MessageBox() function
 *----------------------------------------------------------------------
 */
typedef struct MsgTypeInfo {
    char * name;
    int type;
    int numButtons;
    char * btnNames[3];
} MsgTypeInfo;

#define NUM_TYPES 6

static MsgTypeInfo 
msgTypeInfo[NUM_TYPES] = {
    {"abortretryignore", MB_ABORTRETRYIGNORE, 3, {"abort", "retry", "ignore"}},
    {"ok", 		 MB_OK, 	      1, {"ok"                      }},
    {"okcancel",	 MB_OKCANCEL,	      2, {"ok",    "cancel"         }},
    {"retrycancel",	 MB_RETRYCANCEL,      2, {"retry", "cancel"         }},
    {"yesno",		 MB_YESNO,	      2, {"yes",   "no"             }},
    {"yesnocancel",	 MB_YESNOCANCEL,      3, {"yes",   "no",    "cancel"}}
};

/*
 * The following structure is used in the GetOpenFileName() and
 * GetSaveFileName() calls.
 */
typedef struct _OpenFileData {
    Tcl_Interp * interp;
    TCHAR szFile[MAX_PATH+1];
} OpenFileData;

/*
 * The following structure is used in the ChooseColor() call.
 */
typedef struct _ChooseColorData {
    Tcl_Interp * interp;
    char * title;			/* Title of the color dialog */
} ChooseColorData;


static int 		GetFileName _ANSI_ARGS_((ClientData clientData,
    			    Tcl_Interp *interp, int argc, char **argv,
    			    int isOpen));
static UINT CALLBACK	ColorDlgHookProc _ANSI_ARGS_((HWND hDlg, UINT uMsg,
			    WPARAM wParam, LPARAM lParam));
static int 		MakeFilter _ANSI_ARGS_((Tcl_Interp *interp,
    			    OPENFILENAME *ofnPtr, char * string));
static int		ParseFileDlgArgs _ANSI_ARGS_((Tcl_Interp * interp,
    			    OPENFILENAME *ofnPtr, int argc, char ** argv,
			    int isOpen));
static int 		ProcessCDError _ANSI_ARGS_((Tcl_Interp * interp,
			    DWORD dwErrorCode, HWND hWnd));

/*
 *----------------------------------------------------------------------
 *
 * EvalArgv --
 *
 *	Invokes the Tcl procedure with the arguments. argv[0] is set by
 *	the caller of this function. It may be different than cmdName.
 *	The TCL command will see argv[0], not cmdName, as its name if it
 *	invokes [lindex [info level 0] 0]
 *
 * Results:
 *	TCL_ERROR if the command does not exist and cannot be autoloaded.
 *	Otherwise, return the result of the evaluation of the command.
 *
 * Side effects:
 *	The command may be autoloaded.
 *
 *----------------------------------------------------------------------
 */

static int 
EvalArgv(interp, cmdName, argc, argv)
    Tcl_Interp *interp;		/* Current interpreter. */
    char * cmdName;		/* Name of the TCL command to call */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tcl_CmdInfo cmdInfo;

    if (!Tcl_GetCommandInfo(interp, cmdName, &cmdInfo)) {
	char * cmdArgv[2];

	/*
	 * This comand is not in the interpreter yet -- looks like we
	 * have to auto-load it
	 */
	if (!Tcl_GetCommandInfo(interp, "auto_load", &cmdInfo)) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "cannot execute command \"auto_load\"",
		NULL);
	    return TCL_ERROR;
	}

	cmdArgv[0] = "auto_load";
	cmdArgv[1] = cmdName;

	if ((*cmdInfo.proc)(cmdInfo.clientData, interp, 2, cmdArgv)!= TCL_OK){ 
	    return TCL_ERROR;
	}

	if (!Tcl_GetCommandInfo(interp, cmdName, &cmdInfo)) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "cannot auto-load command \"",
		cmdName, "\"",NULL);
	    return TCL_ERROR;
	}
    }

    return (*cmdInfo.proc)(cmdInfo.clientData, interp, argc, argv);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ChooseColorCmd --
 *
 *	This procedure implements the color dialog box for the Windows
 *	platform. See the user documentation for details on what it
 *	does.
 *
 * Results:
 *	See user documentation.
 *
 * Side effects:
 *	A dialog window is created the first time this procedure is called.
 *	This window is not destroyed and will be reused the next time the
 *	application invokes the "tk_chooseColor" command.
 *
 *----------------------------------------------------------------------
 */

int
Tk_ChooseColorCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window parent = Tk_MainWindow(interp);
    ChooseColorData custData;
    int oldMode;
    CHOOSECOLOR chooseColor;
    char * colorStr = NULL;
    int i;
    int winCode, tclCode;
    XColor * colorPtr = NULL;
    static inited = 0;
    static long dwCustColors[16];
    static long oldColor;		/* the color selected last time */

    custData.title     = NULL;

    if (!inited) {
	/*
	 * dwCustColors stores the custom color which the user can
	 * modify. We store these colors in a fixed array so that the next
	 * time the color dialog pops up, the same set of custom colors
	 * remain in the dialog.
	 */
	for (i=0; i<16; i++) {
	    dwCustColors[i] = (RGB(255-i*10, i, i*10)) ;
	}
	oldColor = RGB(0xa0,0xa0,0xa0);
	inited = 1;
    }

    /*
     * 1. Parse the arguments
     */

    chooseColor.lStructSize  = sizeof(CHOOSECOLOR) ;
    chooseColor.hwndOwner    = 0;			/* filled in below */
    chooseColor.hInstance    = 0;
    chooseColor.rgbResult    = oldColor;
    chooseColor.lpCustColors = (LPDWORD) dwCustColors ;
    chooseColor.Flags        = CC_RGBINIT | CC_FULLOPEN | CC_ENABLEHOOK;
    chooseColor.lCustData    = (LPARAM)&custData;
    chooseColor.lpfnHook     = ColorDlgHookProc;
    chooseColor.lpTemplateName = NULL;

    for (i=1; i<argc; i+=2) {
        int v = i+1;
	int len = strlen(argv[i]);

	if (strncmp(argv[i], "-initialcolor", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    colorStr = argv[v];
	}
	else if (strncmp(argv[i], "-parent", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    parent=Tk_NameToWindow(interp, argv[v], Tk_MainWindow(interp));
	    if (parent == NULL) {
		return TCL_ERROR;
	    }
	}
	else if (strncmp(argv[i], "-title", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    custData.title = argv[v];
	}
	else {
    	    Tcl_AppendResult(interp, "unknown option \"", 
		argv[i], "\", must be -initialcolor, -parent or -title",
		NULL);
		return TCL_ERROR;
	}
    }

    if (Tk_WindowId(parent) == None) {
	Tk_MakeWindowExist(parent);
    }
    chooseColor.hwndOwner = Tk_GetHWND(Tk_WindowId(parent));

    if (colorStr != NULL) {
	colorPtr = Tk_GetColor(interp, Tk_MainWindow(interp), colorStr);
	if (!colorPtr) {
	    return TCL_ERROR;
	}
	chooseColor.rgbResult = RGB((colorPtr->red/0x100), 
	    (colorPtr->green/0x100), (colorPtr->blue/0x100));
    }	

    /*
     * 2. Popup the dialog
     */

    oldMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
    winCode = ChooseColor(&chooseColor);
    (void) Tcl_SetServiceMode(oldMode);

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
	char result[100];

	sprintf(result, "#%02x%02x%02x",
	    GetRValue(chooseColor.rgbResult), 
	    GetGValue(chooseColor.rgbResult), 
	    GetBValue(chooseColor.rgbResult));
        Tcl_AppendResult(interp, result, NULL);
	tclCode = TCL_OK;

	oldColor = chooseColor.rgbResult;
    } else {
	/*
	 * User probably pressed Cancel, or an error occurred
	 */
	tclCode = ProcessCDError(interp, CommDlgExtendedError(), 
	     chooseColor.hwndOwner);
    }

    if (colorPtr) {
	Tk_FreeColor(colorPtr);
    }

    return tclCode;

  arg_missing:
    Tcl_AppendResult(interp, "value for \"", argv[argc-1], "\" missing",
	NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ColorDlgHookProc --
 *
 *	Gets called during the execution of the color dialog. It processes
 *	the "interesting" messages that Windows send to the dialog.
 *
 * Results:
 *	TRUE if the message has been processed, FALSE otherwise.
 *
 * Side effects:
 *	Changes the title of the dialog window when it is popped up.
 *
 *----------------------------------------------------------------------
 */

static UINT
CALLBACK ColorDlgHookProc(hDlg, uMsg, wParam, lParam)
    HWND hDlg;			/* Handle to the color dialog */
    UINT uMsg;			/* Type of message */
    WPARAM wParam;		/* word param, interpretation depends on uMsg*/
    LPARAM lParam;		/* long param, interpretation depends on uMsg*/
{
    CHOOSECOLOR * ccPtr;
    ChooseColorData * pCustData;

    switch (uMsg) {
      case WM_INITDIALOG:
	/* Save the pointer to CHOOSECOLOR so that we can use it later */
	SetWindowLong(hDlg, DWL_USER, lParam);

	/* Set the title string of the dialog */
	ccPtr = (CHOOSECOLOR*)lParam;
	pCustData = (ChooseColorData*)(ccPtr->lCustData);
	if (pCustData->title && *(pCustData->title)) {
 	    SetWindowText(hDlg, (LPCSTR)pCustData->title);
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
 *	This procedure implements the "open file" dialog box for the
 *	Windows platform. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	See user documentation.
 *
 * Side effects:
 *	A dialog window is created the first this procedure is called.
 *	This window is not destroyed and will be reused the next time
 *	the application invokes the "tk_getOpenFile" or
 *	"tk_getSaveFile" command.
 *
 *----------------------------------------------------------------------
 */

int
Tk_GetOpenFileCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return GetFileName(clientData, interp, argc, argv, OPEN_FILE);
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
Tk_GetSaveFileCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return GetFileName(clientData, interp, argc, argv, SAVE_FILE);
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
GetFileName(clientData, interp, argc, argv, isOpen)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
    int isOpen;			/* true if we should call GetOpenFileName(),
				 * false if we should call GetSaveFileName() */
{
    OPENFILENAME openFileName, *ofnPtr;
    int tclCode, winCode, oldMode;
    OpenFileData *custData;
    char buffer[MAX_PATH+1];
    
    ofnPtr = &openFileName;

    /*
     * 1. Parse the arguments.
     */
    if (ParseFileDlgArgs(interp, ofnPtr, argc, argv, isOpen) != TCL_OK) {
	return TCL_ERROR;
    }
    custData = (OpenFileData*) ofnPtr->lCustData;

    /*
     * 2. Call the common dialog function.
     */
    oldMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
    GetCurrentDirectory(MAX_PATH+1, buffer);
    if (isOpen) {
	winCode = GetOpenFileName(ofnPtr);
    } else {
	winCode = GetSaveFileName(ofnPtr);
    }
    SetCurrentDirectory(buffer);
    (void) Tcl_SetServiceMode(oldMode);

    /*
     * Clear the interp result since anything may have happened during the
     * modal loop.
     */

    Tcl_ResetResult(interp);

    if (ofnPtr->lpstrInitialDir != NULL) {
	ckfree((char*) ofnPtr->lpstrInitialDir);
    }

    /*
     * 3. Process the results.
     */
    if (winCode) {
	char *p;
	Tcl_ResetResult(interp);

	for (p = custData->szFile; p && *p; p++) {
	    /*
	     * Change the pathname to the Tcl "normalized" pathname, where
	     * back slashes are used instead of forward slashes
	     */
	    if (*p == '\\') {
		*p = '/';
	    }
	}
	Tcl_AppendResult(interp, custData->szFile, NULL);
	tclCode = TCL_OK;
    } else {
	tclCode = ProcessCDError(interp, CommDlgExtendedError(),
		ofnPtr->hwndOwner);
    }

    if (custData) {
	ckfree((char*)custData);
    }
    if (ofnPtr->lpstrFilter) {
	ckfree((char*)ofnPtr->lpstrFilter);
    }

    return tclCode;
}

/*
 *----------------------------------------------------------------------
 *
 * ParseFileDlgArgs --
 *
 *	Parses the arguments passed to tk_getOpenFile and tk_getSaveFile.
 *
 * Results:
 *	A standard TCL return value.
 *
 * Side effects:
 *	The OPENFILENAME structure is initialized and modified according
 *	to the arguments.
 *
 *----------------------------------------------------------------------
 */

static int 
ParseFileDlgArgs(interp, ofnPtr, argc, argv, isOpen)
    Tcl_Interp * interp;	/* Current interpreter. */
    OPENFILENAME *ofnPtr;	/* Info about the file dialog */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
    int isOpen;			/* true if we should call GetOpenFileName(),
				 * false if we should call GetSaveFileName() */
{
    OpenFileData * custData;
    int i;
    Tk_Window parent = Tk_MainWindow(interp);
    int doneFilter = 0;
    int windowsMajorVersion;
    Tcl_DString buffer;

    custData = (OpenFileData*)ckalloc(sizeof(OpenFileData));
    custData->interp = interp;
    strcpy(custData->szFile, "");

    /* Fill in the OPENFILENAME structure to */
    ofnPtr->lStructSize       = sizeof(OPENFILENAME);
    ofnPtr->hwndOwner         = 0;			/* filled in below */
    ofnPtr->lpstrFilter       = NULL;
    ofnPtr->lpstrCustomFilter = NULL;
    ofnPtr->nMaxCustFilter    = 0;
    ofnPtr->nFilterIndex      = 0;
    ofnPtr->lpstrFile         = custData->szFile;
    ofnPtr->nMaxFile          = sizeof(custData->szFile);
    ofnPtr->lpstrFileTitle    = NULL;
    ofnPtr->nMaxFileTitle     = 0;
    ofnPtr->lpstrInitialDir   = NULL;
    ofnPtr->lpstrTitle        = NULL;
    ofnPtr->nFileOffset       = 0;
    ofnPtr->nFileExtension    = 0;
    ofnPtr->lpstrDefExt       = NULL;
    ofnPtr->lpfnHook 	      = NULL; 
    ofnPtr->lCustData         = (DWORD)custData;
    ofnPtr->lpTemplateName    = NULL;
    ofnPtr->Flags             = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

    windowsMajorVersion = LOBYTE(LOWORD(GetVersion()));
    if (windowsMajorVersion >= 4) {
	/*
	 * Use the "explorer" style file selection box on platforms that
	 * support it (Win95 and NT4.0, both have a major version number
	 * of 4)
	 */
	ofnPtr->Flags |= OFN_EXPLORER;
    }


    if (isOpen) {
	ofnPtr->Flags |= OFN_FILEMUSTEXIST;
    } else {
	ofnPtr->Flags |= OFN_OVERWRITEPROMPT;
    }

    for (i=1; i<argc; i+=2) {
        int v = i+1;
	int len = strlen(argv[i]);

	if (strncmp(argv[i], "-defaultextension", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    ofnPtr->lpstrDefExt = argv[v];
	    if (ofnPtr->lpstrDefExt[0] == '.') {
		/* Windows will insert the dot for us */
		ofnPtr->lpstrDefExt ++;
	    }
	}
	else if (strncmp(argv[i], "-filetypes", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    if (MakeFilter(interp, ofnPtr, argv[v]) != TCL_OK) {
		return TCL_ERROR;
	    }
	    doneFilter = 1;
	}
	else if (strncmp(argv[i], "-initialdir", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    if (Tcl_TranslateFileName(interp, argv[v], &buffer) == NULL) {
		return TCL_ERROR;
	    }
	    ofnPtr->lpstrInitialDir = ckalloc(Tcl_DStringLength(&buffer)+1);
	    strcpy((char*)ofnPtr->lpstrInitialDir, Tcl_DStringValue(&buffer));
	    Tcl_DStringFree(&buffer);
	}
	else if (strncmp(argv[i], "-initialfile", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    if (Tcl_TranslateFileName(interp, argv[v], &buffer) == NULL) {
		return TCL_ERROR;
	    }
	    strcpy(ofnPtr->lpstrFile, Tcl_DStringValue(&buffer));
	    Tcl_DStringFree(&buffer);
	}
	else if (strncmp(argv[i], "-parent", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    parent=Tk_NameToWindow(interp, argv[v], Tk_MainWindow(interp));
	    if (parent == NULL) {
		return TCL_ERROR;
	    }
	}
	else if (strncmp(argv[i], "-title", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    ofnPtr->lpstrTitle = argv[v];
	}
	else {
    	    Tcl_AppendResult(interp, "unknown option \"", 
		argv[i], "\", must be -defaultextension, ",
		"-filetypes, -initialdir, -initialfile, -parent or -title",
		NULL);
	    return TCL_ERROR;
	}
    }

    if (!doneFilter) {
	if (MakeFilter(interp, ofnPtr, "") != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    if (Tk_WindowId(parent) == None) {
	Tk_MakeWindowExist(parent);
    }
    ofnPtr->hwndOwner = Tk_GetHWND(Tk_WindowId(parent));

    return TCL_OK;

  arg_missing:
    Tcl_AppendResult(interp, "value for \"", argv[argc-1], "\" missing",
	NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeFilter --
 *
 *	Allocate a buffer to store the filters in a format understood by
 *	Windows
 *
 * Results:
 *	A standard TCL return value.
 *
 * Side effects:
 *	ofnPtr->lpstrFilter is modified.
 *
 *----------------------------------------------------------------------
 */
static int MakeFilter(interp, ofnPtr, string) 
    Tcl_Interp *interp;		/* Current interpreter. */
    OPENFILENAME *ofnPtr;	/* Info about the file dialog */
    char *string;		/* String value of the -filetypes option */
{
    char *filterStr;
    char *p;
    int pass;
    FileFilterList flist;
    FileFilter *filterPtr;

    TkInitFileFilters(&flist);
    if (TkGetFileFilters(interp, &flist, string, 1) != TCL_OK) {
	return TCL_ERROR;
    }

    if (flist.filters == NULL) {
	/*
	 * Use "All Files (*.*) as the default filter is none is specified
	 */
	char *defaultFilter = "All Files (*.*)";

	p = filterStr = (char*)ckalloc(30 * sizeof(char));

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
	/* We format the filetype into a string understood by Windows:
	 * {"Text Documents" {.doc .txt} {TEXT}} becomes
	 * "Text Documents (*.doc,*.txt)\0*.doc;*.txt\0"
	 *
	 * See the Windows OPENFILENAME manual page for details on the filter
	 * string format.
	 */

	/*
	 * Since we may only add asterisks (*) to the filter, we need at most
	 * twice the size of the string to format the filter
	 */
	filterStr = ckalloc(strlen(string) * 3);

	for (filterPtr = flist.filters, p = filterStr; filterPtr;
	        filterPtr = filterPtr->next) {
	    char *sep;
	    FileFilterClause *clausePtr;

	    /*
	     *  First, put in the name of the file type
	     */
	    strcpy(p, filterPtr->name);
	    p+= strlen(filterPtr->name);
	    *p++ = ' ';
	    *p++ = '(';

	    for (pass = 1; pass <= 2; pass++) {
		/*
		 * In the first pass, we format the extensions in the 
		 * name field. In the second pass, we format the extensions in
		 * the filter pattern field
		 */
		sep = "";
		for (clausePtr=filterPtr->clauses;clausePtr;
		         clausePtr=clausePtr->next) {
		    GlobPattern *globPtr;
		

		    for (globPtr=clausePtr->patterns; globPtr;
			    globPtr=globPtr->next) {
			strcpy(p, sep);
			p+= strlen(sep);
			strcpy(p, globPtr->pattern);
			p+= strlen(globPtr->pattern);

			if (pass==1) {
			    sep = ",";
			} else {
			    sep = ";";
			}
		    }
		}
		if (pass == 1) {
		    if (pass == 1) {
			*p ++ = ')';
		    }
		}
		*p ++ = '\0';
	    }
	}

	/*
	 * Windows requires the filter string to be ended by two NULL
	 * characters.
	 */
	*p++ = '\0';
	*p = '\0';
    }

    if (ofnPtr->lpstrFilter != NULL) {
	ckfree((char*)ofnPtr->lpstrFilter);
    }
    ofnPtr->lpstrFilter = filterStr;

    TkFreeFileFilters(&flist);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MessageBoxCmd --
 *
 *	This procedure implements the MessageBox window for the
 *	Windows platform. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	See user documentation.
 *
 * Side effects:
 *	None. The MessageBox window will be destroy before this procedure
 *	returns.
 *
 *----------------------------------------------------------------------
 */

int
Tk_MessageBoxCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    int flags;
    Tk_Window parent = Tk_MainWindow(interp);
    HWND hWnd;
    char *message = "";
    char *title = "";
    int icon = MB_ICONINFORMATION;
    int type = MB_OK;
    int i, j;
    char *result;
    int code, oldMode;
    char *defaultBtn = NULL;
    int defaultBtnIdx = -1;

    for (i=1; i<argc; i+=2) {
	int v = i+1;
	int len = strlen(argv[i]);

	if (strncmp(argv[i], "-default", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    defaultBtn = argv[v];
	}
	else if (strncmp(argv[i], "-icon", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    if (strcmp(argv[v], "error") == 0) {
		icon = MB_ICONERROR;
	    }
	    else if (strcmp(argv[v], "info") == 0) {
		icon = MB_ICONINFORMATION;
	    }
	    else if (strcmp(argv[v], "question") == 0) {
		icon = MB_ICONQUESTION;
	    }
	    else if (strcmp(argv[v], "warning") == 0) {
		icon = MB_ICONWARNING;
	    }
	    else {
	        Tcl_AppendResult(interp, "invalid icon \"", argv[v],
		    "\", must be error, info, question or warning", NULL);
		return TCL_ERROR;
	    }
	}
	else if (strncmp(argv[i], "-message", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    message = argv[v];
	}
	else if (strncmp(argv[i], "-parent", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    parent=Tk_NameToWindow(interp, argv[v], Tk_MainWindow(interp));
	    if (parent == NULL) {
		return TCL_ERROR;
	    }
	}
	else if (strncmp(argv[i], "-title", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    title = argv[v];
	}
	else if (strncmp(argv[i], "-type", len)==0) {
	    int found = 0;

	    if (v==argc) {goto arg_missing;}

	    for (j=0; j<NUM_TYPES; j++) {
		if (strcmp(argv[v], msgTypeInfo[j].name) == 0) {
		    type = msgTypeInfo[j].type;
		    found = 1;
		    break;
		}
	    }
	    if (!found) {
		Tcl_AppendResult(interp, "invalid message box type \"", 
		    argv[v], "\", must be abortretryignore, ok, ",
		    "okcancel, retrycancel, yesno or yesnocancel", NULL);
		return TCL_ERROR;
	    }
	}
	else {
    	    Tcl_AppendResult(interp, "unknown option \"", 
		argv[i], "\", must be -default, -icon, ",
		"-message, -parent, -title or -type", NULL);
		return TCL_ERROR;
	}
    }

    /* Make sure we have a valid hWnd to act as the parent of this message box
     */
    if (Tk_WindowId(parent) == None) {
	Tk_MakeWindowExist(parent);
    }
    hWnd = Tk_GetHWND(Tk_WindowId(parent));

    if (defaultBtn != NULL) {
	for (i=0; i<NUM_TYPES; i++) {
	    if (type == msgTypeInfo[i].type) {
		for (j=0; j<msgTypeInfo[i].numButtons; j++) {
		    if (strcmp(defaultBtn, msgTypeInfo[i].btnNames[j])==0) {
		        defaultBtnIdx = j;
			break;
		    }
		}
		if (defaultBtnIdx < 0) {
		    Tcl_AppendResult(interp, "invalid default button \"",
			defaultBtn, "\"", NULL);
		    return TCL_ERROR;
		}
		break;
	    }
	}

	switch (defaultBtnIdx) {
	  case 0: flags = MB_DEFBUTTON1; break;
	  case 1: flags = MB_DEFBUTTON2; break;
	  case 2: flags = MB_DEFBUTTON3; break;
	  case 3: flags = MB_DEFBUTTON4; break;
	}
    } else {
	flags = 0;
    }
    
    flags |= icon | type;
    oldMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);
    code = MessageBox(hWnd, message, title, flags|MB_SYSTEMMODAL);
    (void) Tcl_SetServiceMode(oldMode);

    switch (code) {
      case IDABORT:	result = "abort";  break;
      case IDCANCEL:	result = "cancel"; break;
      case IDIGNORE:	result = "ignore"; break;
      case IDNO:	result = "no";     break;
      case IDOK:	result = "ok";     break;
      case IDRETRY:	result = "retry";  break;
      case IDYES:	result = "yes";    break;
      default:		result = "";
    }

    /*
     * When we come to here interp->result may have been changed by some
     * background scripts. Call Tcl_SetResult() to make sure that any stuff
     * lingering in interp->result will not appear in the result of
     * this command.
     */

    Tcl_SetResult(interp, result, TCL_STATIC);
    return TCL_OK;

  arg_missing:
    Tcl_AppendResult(interp, "value for \"", argv[argc-1], "\" missing",
	NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ProcessCDError --
 *
 *	This procedure gets called if a Windows-specific error message
 *	has occurred during the execution of a common dialog or the
 *	user has pressed the CANCEL button.
 *
 * Results:
 *	If an error has indeed happened, returns a standard TCL result
 *	that reports the error code in string format. If the user has
 *	pressed the CANCEL button (dwErrorCode == 0), resets
 *	interp->result to the empty string.
 *
 * Side effects:
 *	interp->result is changed.
 *
 *----------------------------------------------------------------------
 */
static int ProcessCDError(interp, dwErrorCode, hWnd)
    Tcl_Interp * interp;		/* Current interpreter. */
    DWORD dwErrorCode;			/* The Windows-specific error code */
    HWND hWnd;				/* window in which the error happened*/
{
    char *string;

    Tcl_ResetResult(interp);

    switch(dwErrorCode) {
      case 0:	  /* User has hit CANCEL */
	return TCL_OK;

      case CDERR_DIALOGFAILURE:   string="CDERR_DIALOGFAILURE";  	break;
      case CDERR_STRUCTSIZE:      string="CDERR_STRUCTSIZE";   		break;
      case CDERR_INITIALIZATION:  string="CDERR_INITIALIZATION";   	break;
      case CDERR_NOTEMPLATE:      string="CDERR_NOTEMPLATE";   		break;
      case CDERR_NOHINSTANCE:     string="CDERR_NOHINSTANCE";   	break;
      case CDERR_LOADSTRFAILURE:  string="CDERR_LOADSTRFAILURE";   	break;
      case CDERR_FINDRESFAILURE:  string="CDERR_FINDRESFAILURE";   	break;
      case CDERR_LOADRESFAILURE:  string="CDERR_LOADRESFAILURE";   	break;
      case CDERR_LOCKRESFAILURE:  string="CDERR_LOCKRESFAILURE";   	break;
      case CDERR_MEMALLOCFAILURE: string="CDERR_MEMALLOCFAILURE";   	break;
      case CDERR_MEMLOCKFAILURE:  string="CDERR_MEMLOCKFAILURE";   	break;
      case CDERR_NOHOOK:          string="CDERR_NOHOOK";   	 	break;
      case PDERR_SETUPFAILURE:    string="PDERR_SETUPFAILURE";   	break;
      case PDERR_PARSEFAILURE:    string="PDERR_PARSEFAILURE";   	break;
      case PDERR_RETDEFFAILURE:   string="PDERR_RETDEFFAILURE";   	break;
      case PDERR_LOADDRVFAILURE:  string="PDERR_LOADDRVFAILURE";   	break;
      case PDERR_GETDEVMODEFAIL:  string="PDERR_GETDEVMODEFAIL";   	break;
      case PDERR_INITFAILURE:     string="PDERR_INITFAILURE";   	break;
      case PDERR_NODEVICES:       string="PDERR_NODEVICES";   		break;
      case PDERR_NODEFAULTPRN:    string="PDERR_NODEFAULTPRN";   	break;
      case PDERR_DNDMMISMATCH:    string="PDERR_DNDMMISMATCH";   	break;
      case PDERR_CREATEICFAILURE: string="PDERR_CREATEICFAILURE";   	break;
      case PDERR_PRINTERNOTFOUND: string="PDERR_PRINTERNOTFOUND";   	break;
      case CFERR_NOFONTS:         string="CFERR_NOFONTS";   	 	break;
      case FNERR_SUBCLASSFAILURE: string="FNERR_SUBCLASSFAILURE";   	break;
      case FNERR_INVALIDFILENAME: string="FNERR_INVALIDFILENAME";   	break;
      case FNERR_BUFFERTOOSMALL:  string="FNERR_BUFFERTOOSMALL";   	break;
	
      default:
	string="unknown error";
    }

    Tcl_AppendResult(interp, "Win32 internal error: ", string, NULL); 
    return TCL_ERROR;
}
