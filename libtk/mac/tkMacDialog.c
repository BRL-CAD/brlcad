/*
 * tkMacDialog.c --
 *
 *	Contains the Mac implementation of the common dialog boxes.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacDialog.c 1.12 96/12/03 11:15:12
 *
 */

#include <Gestalt.h>
#include <Aliases.h>
#include <Errors.h>
#include <Strings.h>
#include <MoreFiles.h>
#include <MoreFilesExtras.h>
#include <StandardFile.h>
#include <ColorPicker.h>
#include <Lowmem.h>
#include "tkPort.h"
#include "tkInt.h"
#include "tclMacInt.h"
#include "tkFileFilter.h"

/*
 * The following are ID's for resources that are defined in tkMacResource.r
 */
#define OPEN_BOX        130
#define OPEN_POPUP      131
#define OPEN_MENU       132
#define OPEN_POPUP_ITEM 10

#define SAVE_FILE	0
#define OPEN_FILE	1

#define MATCHED		0
#define UNMATCHED	1

/*
 * The following structure is used in the GetFileName() function. It stored
 * information about the file dialog and the file filters.
 */
typedef struct _OpenFileData {
    Tcl_Interp * interp;
    char * initialFile;			/* default file to appear in the
					 * save dialog */
    char * defExt;			/* default extension (not used on the
					 * Mac) */
    FileFilterList fl;			/* List of file filters. */
    SInt16 curType;			/* The filetype currently being
					 * listed */
    int isOpen;				/* True if this is an Open dialog,
					 * false if it is a Save dialog. */
    MenuHandle menu;			/* Handle of the menu in the popup*/
    short dialogId;			/* resource ID of the dialog */
    int popupId;			/* resource ID of the popup */
    short popupItem;			/* item number of the popup in the
					 * dialog */
    int usePopup;			/* True if we show the popup menu (this
    					 * is an open operation and the
					 * -filetypes option is set)
    					 */
} OpenFileData;

static pascal Boolean	FileFilterProc _ANSI_ARGS_((CInfoPBPtr pb,
			    void *myData));
static int 		GetFileName _ANSI_ARGS_ ((
			    ClientData clientData, Tcl_Interp *interp,
    			    int argc, char **argv, int isOpen ));
static Boolean		MatchOneType _ANSI_ARGS_((CInfoPBPtr pb,
			    OpenFileData * myDataPtr, FileFilter * filterPtr));
static pascal short 	OpenHookProc _ANSI_ARGS_((short item,
			    DialogPtr theDialog, OpenFileData * myDataPtr));
static int 		ParseFileDlgArgs _ANSI_ARGS_ ((Tcl_Interp * interp,
			    OpenFileData * myDataPtr, int argc, char ** argv,
			    int isOpen));

/*
 * Filter and hook functions used by the tk_getOpenFile and tk_getSaveFile
 * commands.
 */

static FileFilterYDUPP openFilter = NULL;
static DlgHookYDUPP openHook = NULL;
static DlgHookYDUPP saveHook = NULL;
  

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
EvalArgv(
    Tcl_Interp *interp,		/* Current interpreter. */
    char * cmdName,		/* Name of the TCL command to call */
    int argc,			/* Number of arguments. */
    char **argv)		/* Argument strings. */
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
 *	This procedure implements the color dialog box for the Mac
 *	platform. See the user documentation for details on what it
 *	does.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_ChooseColorCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    char **argv)		/* Argument strings. */
{
    Tk_Window parent = Tk_MainWindow(interp);
    char * colorStr = NULL;
    XColor * colorPtr = NULL;
    char * title = "Choose a color:";
    int i, version;
    long response = 0;
    OSErr err = noErr;
    char buff[40];
    static RGBColor in;
    static inited = 0;

    /*
     * Use the gestalt manager to determine how to bring
     * up the color picker.  If versin 2.0 isn't available
     * we can assume version 1.0 is available as it comes with
     * Color Quickdraw which Tk requires to run at all.
     */
     
    err = Gestalt(gestaltColorPicker, &response); 
    if ((err == noErr) || (response == 0x0200L)) {
    	version = 2;
    } else {
    	version = 1;
    }
 
    for (i=1; i<argc; i+=2) {
        int v = i+1;
	int len = strlen(argv[i]);

        if (strncmp(argv[i], "-initialcolor", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    colorStr = argv[v];
	} else if (strncmp(argv[i], "-parent", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    parent=Tk_NameToWindow(interp, argv[v], Tk_MainWindow(interp));
	    if (parent == NULL) {
		return TCL_ERROR;
	    }
	} else if (strncmp(argv[i], "-title", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    title = argv[v];
	} else {
    	    Tcl_AppendResult(interp, "unknown option \"", 
		    argv[i], "\", must be -initialcolor, -parent or -title",
		    NULL);
	    return TCL_ERROR;
	}
    }

    if (colorStr) {
        colorPtr = Tk_GetColor(interp, parent, colorStr);
        if (colorPtr == NULL) {
            return TCL_ERROR;
        }
    }

    if (!inited) {
        inited = 1;
        in.red = 0xffff;
        in.green = 0xffff;
        in.blue = 0xffff;
    }
    if (colorPtr) {
        in.red   = colorPtr->red;
        in.green = colorPtr->green;
        in.blue  = colorPtr->blue;
    }
        
    if (version == 1) {
        /*
         * Use version 1.0 of the color picker
         */
    	
    	RGBColor out;
    	Str255 prompt;
    	Point point = {-1, -1};
    	
        prompt[0] = strlen(title);
        strncpy((char*) prompt+1, title, 255);
        
        if (GetColor(point, prompt, &in, &out)) {
            /*
             * user selected a color
             */
            sprintf(buff, "#%02x%02x%02x", out.red >> 8, out.green >> 8,
                out.blue >> 8);
            Tcl_SetResult(interp, buff, TCL_VOLATILE);

            /*
             * Save it for the next time
             */
            in.red   = out.red;
            in.green = out.green;
            in.blue  = out.blue;
        } else {
            Tcl_ResetResult(interp);
    	}
    } else {
        /*
         * Version 2.0 of the color picker is available. Let's use it
         */
	ColorPickerInfo cpinfo;

    	cpinfo.theColor.profile = 0L;
    	cpinfo.theColor.color.rgb.red   = in.red;
    	cpinfo.theColor.color.rgb.green = in.green;
    	cpinfo.theColor.color.rgb.blue  = in.blue;
    	cpinfo.dstProfile = 0L;
    	cpinfo.flags = CanModifyPalette | CanAnimatePalette;
    	cpinfo.placeWhere = kDeepestColorScreen;
    	cpinfo.pickerType = 0L;
    	cpinfo.eventProc = NULL;
    	cpinfo.colorProc = NULL;
    	cpinfo.colorProcData = NULL;

        cpinfo.prompt[0] = strlen(title);
        strncpy((char*)cpinfo.prompt+1, title, 255);
        
        if ((PickColor(&cpinfo) == noErr) && cpinfo.newColorChosen) {
            sprintf(buff, "#%02x%02x%02x",
		cpinfo.theColor.color.rgb.red   >> 8, 
                cpinfo.theColor.color.rgb.green >> 8,
		cpinfo.theColor.color.rgb.blue  >> 8);
            Tcl_SetResult(interp, buff, TCL_VOLATILE);
            
            in.blue  = cpinfo.theColor.color.rgb.red;
    	    in.green = cpinfo.theColor.color.rgb.green;
    	    in.blue  = cpinfo.theColor.color.rgb.blue;
          } else {
            Tcl_ResetResult(interp);
        }
    }

    if (colorPtr) {
	Tk_FreeColor(colorPtr);
    }

    return TCL_OK;

  arg_missing:
    Tcl_AppendResult(interp, "value for \"", argv[argc-1], "\" missing",
	NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetOpenFileCmd --
 *
 *	This procedure implements the "open file" dialog box for the
 *	Mac platform. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	See user documentation.
 *----------------------------------------------------------------------
 */

int
Tk_GetOpenFileCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    char **argv)		/* Argument strings. */
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
 *      A standard Tcl result.
 *
 * Side effects:
 *	See user documentation.
 *----------------------------------------------------------------------
 */

int
Tk_GetSaveFileCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    char **argv)		/* Argument strings. */
{
    return GetFileName(clientData, interp, argc, argv, SAVE_FILE);
}

/*
 *----------------------------------------------------------------------
 *
 * GetFileName --
 *
 *	Calls the Mac file dialog functions for the user to choose a
 *	file to or save.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	If the user selects a file, the native pathname of the file
 *	is returned in interp->result. Otherwise an empty string
 *	is returned in interp->result.
 *
 *----------------------------------------------------------------------
 */

static int
GetFileName(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    char **argv,		/* Argument strings. */
    int isOpen)			/* true if we should call GetOpenFileName(),
				 * false if we should call GetSaveFileName() */
{
    int code = TCL_OK;
    int i;
    OpenFileData myData, *myDataPtr;
    StandardFileReply reply;
    Point mypoint;
    Str255 str;

    myDataPtr = &myData;

    if (openFilter == NULL) {
	openFilter = NewFileFilterYDProc(FileFilterProc);
	openHook = NewDlgHookYDProc(OpenHookProc);
	saveHook = NewDlgHookYDProc(OpenHookProc);
    }

    /*
     * 1. Parse the arguments.
     */
    if (ParseFileDlgArgs(interp, myDataPtr, argc, argv, isOpen) 
	!= TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * 2. Set the items in the file types popup.
     */

    /*
     * Delete all the entries inside the popup menu, in case there's any
     * left overs from previous invocation of this command
     */

    if (myDataPtr->usePopup) {
	FileFilter * filterPtr;

        for (i=CountMItems(myDataPtr->menu); i>0; i--) {
            /*
             * The item indices are one based. Also, if we delete from
             * the beginning, the items may be re-numbered. So we
             * delete from the end
    	     */
    	     DeleteMenuItem(myDataPtr->menu, i);
        }

	if (myDataPtr->fl.filters) {
	    for (filterPtr=myDataPtr->fl.filters; filterPtr;
		    filterPtr=filterPtr->next) {
		strncpy((char*)str+1, filterPtr->name, 254);
		str[0] = strlen(filterPtr->name);
		AppendMenu(myDataPtr->menu, (ConstStr255Param) str);
	    }
	} else {
	    myDataPtr->usePopup = 0;
	}
    }

    /*
     * 3. Call the toolbox file dialog function.
     */
    SetPt(&mypoint, -1, -1);
    TkpSetCursor(NULL);
    
    if (myDataPtr->isOpen) {
        if (myDataPtr->usePopup) {
	    CustomGetFile(openFilter, (short) -1, NULL, &reply, 
	        myDataPtr->dialogId, 
	        mypoint, openHook, NULL, NULL, NULL, (void*)myDataPtr);
	} else {
	    StandardGetFile(NULL, -1, NULL, &reply);
	}
    } else {
	Str255 prompt, def;

	strcpy((char*)prompt+1, "Save as");
	prompt[0] = strlen("Save as");
   	if (myDataPtr->initialFile) {
   	    strncpy((char*)def+1, myDataPtr->initialFile, 254);
	    def[0] = strlen(myDataPtr->initialFile);
        } else {
            def[0] = 0;
        }
   	if (myDataPtr->usePopup) {
   	    /*
   	     * Currently this never gets called because we don't use
   	     * popup for the save dialog.
   	     */
	    CustomPutFile(prompt, def, &reply, myDataPtr->dialogId, mypoint, 
	        saveHook, NULL, NULL, NULL, myDataPtr);
	} else {
	    StandardPutFile(prompt, def, &reply);
	}
    }

    Tcl_ResetResult(interp);    
    if (reply.sfGood) {
        int length;
    	Handle pathHandle = NULL;
    	char * pathName = NULL;
    	
    	FSpPathFromLocation(&reply.sfFile, &length, &pathHandle);

	if (pathHandle != NULL) {
	    HLock(pathHandle);
	    pathName = (char *) ckalloc((unsigned) (length + 1));
	    strcpy(pathName, *pathHandle);
	    HUnlock(pathHandle);
	    DisposeHandle(pathHandle);

	    /*
	     * Return the full pathname of the selected file
	     */

	    Tcl_SetResult(interp, pathName, TCL_DYNAMIC);
	}
    }

  done:
    TkFreeFileFilters(&myDataPtr->fl);
    return code;
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
 *	The OpenFileData structure is initialized and modified according
 *	to the arguments.
 *
 *----------------------------------------------------------------------
 */

static int
ParseFileDlgArgs(
    Tcl_Interp * interp,		/* Current interpreter. */
    OpenFileData * myDataPtr,		/* Information about the file dialog */
    int argc,				/* Number of arguments */
    char ** argv,			/* Argument strings */
    int isOpen)				/* TRUE if this is an "open" dialog */
{
    int i;

    myDataPtr->interp      	= interp;
    myDataPtr->initialFile 	= NULL;
    myDataPtr->curType		= 0;

    TkInitFileFilters(&myDataPtr->fl);
    
    if (isOpen) {
	myDataPtr->isOpen    = 1;
        myDataPtr->usePopup  = 1;
	myDataPtr->menu      = GetMenu(OPEN_MENU);
	myDataPtr->dialogId  = OPEN_BOX;
	myDataPtr->popupId   = OPEN_POPUP;
	myDataPtr->popupItem = OPEN_POPUP_ITEM;
	if (myDataPtr->menu == NULL) {
	    Debugger();
	}
    } else {
        myDataPtr->isOpen    = 0;
	myDataPtr->usePopup  = 0;
    }

    for (i=1; i<argc; i+=2) {
        int v = i+1;
	int len = strlen(argv[i]);

	if (strncmp(argv[i], "-defaultextension", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    myDataPtr->defExt = argv[v];
	}
	else if (strncmp(argv[i], "-filetypes", len)==0) {
	    if (v==argc) {goto arg_missing;}

	    if (TkGetFileFilters(interp, &myDataPtr->fl,argv[v],0) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	else if (strncmp(argv[i], "-initialdir", len)==0) {
	    FSSpec dirSpec;
	    char * dirName;
	    Tcl_DString dstring;
	    long dirID;
	    OSErr err;
	    Boolean isDirectory;

	    if (v==argc) {goto arg_missing;}
	    
	    if (Tcl_TranslateFileName(interp, argv[v], &dstring) == NULL) {
	        return TCL_ERROR;
	    }
	    dirName = dstring.string;
	    if (FSpLocationFromPath(strlen(dirName), dirName, &dirSpec) != 
		    noErr) {
		Tcl_AppendResult(interp, "bad directory \"", argv[v],
	            "\"", NULL);
	        return TCL_ERROR;
	    }
	    err = FSpGetDirectoryID(&dirSpec, &dirID, &isDirectory);
	    if ((err != noErr) || !isDirectory) {
		Tcl_AppendResult(interp, "bad directory \"", argv[v],
	            "\"", NULL);
	        return TCL_ERROR;
	    }
	    /*
	     * Make sure you negate -dirSpec.vRefNum because the standard file
	     * package wants it that way !
	     */
	    LMSetSFSaveDisk(-dirSpec.vRefNum);
	    LMSetCurDirStore(dirID);
	    Tcl_DStringFree(&dstring);
    	}
	else if (strncmp(argv[i], "-initialfile", len)==0) {
	    if (v==argc) {goto arg_missing;}
	    
	    myDataPtr->initialFile = argv[v];
	}
	else if (strncmp(argv[i], "-parent", len)==0) {
	    /*
	     * Ignored on the Mac, but make sure that it's a valid window
	     * pathname
	     */
	    Tk_Window parent;

	    if (v==argc) {goto arg_missing;}
	    	    
	    parent=Tk_NameToWindow(interp, argv[v], Tk_MainWindow(interp));
	    if (parent == NULL) {
		return TCL_ERROR;
	    }	    
	}
	else if (strncmp(argv[i], "-title", len)==0) {
	    if (v==argc) {goto arg_missing;}
	    
	    /*
	     * This option is ignored on the Mac because the Mac file
	     * dialog do not support titles.
	     */
	}
	else {
    	    Tcl_AppendResult(interp, "unknown option \"", 
		argv[i], "\", must be -defaultextension, ",
		"-filetypes, -initialdir, -initialfile, -parent or -title",
		NULL);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;

  arg_missing:
    Tcl_AppendResult(interp, "value for \"", argv[argc-1], "\" missing",
	NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * OpenHookProc --
 *
 *	Gets called for various events that occur in the file dialog box.
 *	Initializes the popup menu or rebuild the file list depending on
 *	the type of the event.
 *
 * Results:
 *	A standard result understood by the Mac file dialog event dispatcher.
 *
 * Side effects:
 *	The contents in the file dialog may be changed depending on
 *	the type of the event.
 *----------------------------------------------------------------------
 */

static pascal short
OpenHookProc(
    short item,			/* Event description. */
    DialogPtr theDialog,	/* The dialog where the event occurs. */
    OpenFileData * myDataPtr)	/* Information about the file dialog. */
{
    short ignore;
    Rect rect;
    Handle handle;
    int newType;

    switch (item) {
	case sfHookFirstCall:
	    if (myDataPtr->usePopup) {
		/*
		 * Set the popup list to display the selected type.
		 */
		GetDialogItem(theDialog, myDataPtr->popupItem,
			&ignore, &handle, &rect);
		SetControlValue((ControlRef) handle, myDataPtr->curType + 1);
	    }
	    return sfHookNullEvent;
      
	case OPEN_POPUP_ITEM:
	    if (myDataPtr->usePopup) {
		GetDialogItem(theDialog, myDataPtr->popupItem,
			&ignore, &handle, &rect);
		newType = GetCtlValue((ControlRef) handle) - 1;
		if (myDataPtr->curType != newType) {
		    if (newType<0 || newType>myDataPtr->fl.numFilters) {
			/*
			 * Sanity check. Looks like the user selected an
			 * non-existent menu item?? Don't do anything.
			 */
		    } else {
			myDataPtr->curType = newType;
		    }
		    return sfHookRebuildList;
		}
	    }  
	    break;
    }

    return item;
}

/*
 *----------------------------------------------------------------------
 *
 * FileFilterProc --
 *
 *	Filters files according to file types. Get called whenever the
 *	file list needs to be updated inside the dialog box.
 *
 * Results:
 *	Returns MATCHED if the file should be shown in the listbox, returns
 *	UNMATCHED otherwise.
 *
 * Side effects:
 *	If MATCHED is returned, the file is shown in the listbox.
 *
 *----------------------------------------------------------------------
 */

static pascal Boolean
FileFilterProc(
    CInfoPBPtr pb,		/* Information about the file */
    void *myData)		/* Client data for this file dialog */
{
    int i;
    OpenFileData * myDataPtr = (OpenFileData*)myData;
    FileFilter * filterPtr;

    if (myDataPtr->fl.numFilters == 0) {
	/*
	 * No types have been specified. List all files by default
	 */
	return MATCHED;
    }

    if (pb->dirInfo.ioFlAttrib & 0x10) {
    	/*
    	 * This is a directory: always show it
    	 */
    	return MATCHED;
    }

    if (myDataPtr->usePopup) {
        i = myDataPtr->curType;
	for (filterPtr=myDataPtr->fl.filters; filterPtr && i>0; i--) {
	    filterPtr = filterPtr->next;
	}
	if (filterPtr) {
	    return MatchOneType(pb, myDataPtr, filterPtr);
	} else {
	    return UNMATCHED;
        }
    } else {
	/*
	 * We are not using the popup menu. In this case, the file is
	 * considered matched if it matches any of the file filters.
	 */

	for (filterPtr=myDataPtr->fl.filters; filterPtr;
		filterPtr=filterPtr->next) {
	    if (MatchOneType(pb, myDataPtr, filterPtr) == MATCHED) {
	        return MATCHED;
	    }
	}
	return UNMATCHED;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MatchOneType --
 *
 *	Match a file with one file type in the list of file types.
 *
 * Results:
 *	Returns MATCHED if the file matches with the file type; returns
 *	UNMATCHED otherwise.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static Boolean
MatchOneType(
    CInfoPBPtr pb,		/* Information about the file */
    OpenFileData * myDataPtr,	/* Information about this file dialog */
    FileFilter * filterPtr)	/* Match the file described by pb against
				 * this filter */
{
    FileFilterClause * clausePtr;

    /*
     * A file matches with a file type if it matches with at least one
     * clause of the type.
     *
     * If the clause has both glob patterns and ostypes, the file must
     * match with at least one pattern AND at least one ostype.
     *
     * If the clause has glob patterns only, the file must match with at least
     * one pattern.
     *
     * If the clause has mac types only, the file must match with at least
     * one mac type.
     *
     * If the clause has neither glob patterns nor mac types, it's
     * considered an error.
     */

    for (clausePtr=filterPtr->clauses; clausePtr; clausePtr=clausePtr->next) {
	int macMatched  = 0;
	int globMatched = 0;
	GlobPattern * globPtr;
	MacFileType * mfPtr;

	if (clausePtr->patterns == NULL) {
	    globMatched = 1;
	}
	if (clausePtr->macTypes == NULL) {
	    macMatched = 1;
	}

	for (globPtr=clausePtr->patterns; globPtr; globPtr=globPtr->next) {
	    char filename[256];
	    int len;
	    char * p, *q, *ext;
        
	    if (pb->hFileInfo.ioNamePtr == NULL) {
		continue;
	    }
	    p = (char*)(pb->hFileInfo.ioNamePtr);
	    len = p[0];
	    strncpy(filename, p+1, len);
	    filename[len] = '\0';
	    ext = globPtr->pattern;

	    if (ext[0] == '\0') {
		/*
		 * We don't want any extensions: OK if the filename doesn't
		 * have "." in it
		 */
		for (q=filename; *q; q++) {
		    if (*q == '.') {
			goto glob_unmatched;
		    }
		}
		goto glob_matched;
	    }
        
	    if (Tcl_StringMatch(filename, ext)) {
		goto glob_matched;
	    } else {
		goto glob_unmatched;
	    }

	  glob_unmatched:
	    continue;

	  glob_matched:
	    globMatched = 1;
	    break;
	}

	for (mfPtr=clausePtr->macTypes; mfPtr; mfPtr=mfPtr->next) {
	    if (pb->hFileInfo.ioFlFndrInfo.fdType == mfPtr->type) {
		macMatched = 1;
		break;
	    }
        }

	if (globMatched && macMatched) {
	    return MATCHED;
	}
    }

    return UNMATCHED;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MessageBoxCmd --
 *
 *	This procedure implements the MessageBox window for the
 *	Mac platform. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	See user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_MessageBoxCmd(
    ClientData clientData,	/* Main window associated with interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    char **argv)		/* Argument strings. */
{
    return EvalArgv(interp, "tkMessageBox", argc, argv);
}
